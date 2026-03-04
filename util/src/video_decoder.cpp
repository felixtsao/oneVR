#include "onevr/video_decoder.h"

#include "onevr/ffmpeg_util.h"

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace onevr {

static void throw_ff(const char* what, int err) {
    throw std::runtime_error(std::string(what) + ": " + ff_err(err));
}

static AVRational get_stream_avg_fps(const AVFormatContext* fmt, int video_stream_index) {
    const AVStream* st = fmt->streams[video_stream_index];
    return st->avg_frame_rate.num > 0 ? st->avg_frame_rate : st->r_frame_rate;
}

static int64_t timecode_to_frame_index(const Timecode& tc, int fps_num, int fps_den) {
    // Initialize calculation FPS (NTSC 29.97 as 30 or 59.94 as 60)
    const int fps = (fps_num == 30000 && fps_den == 1001)   ? 30
                    : (fps_num == 60000 && fps_den == 1001) ? 60
                                                            : fps_num / fps_den;

    int64_t frame_idx = ((int64_t)tc.hh * 3600 + (int64_t)tc.mm * 60 + (int64_t)tc.ss) * fps + tc.ff;

    if (!tc.drop_frame) {
        return frame_idx;
    }

    // Drop-frame correction
    const int drop_frames = (fps == 60) ? 4 : 2;
    const int64_t total_minutes = (int64_t)tc.hh * 60 + tc.mm;
    const int64_t dropped = drop_frames * (total_minutes - total_minutes / 10);

    return frame_idx - dropped;
}

static Timecode parse_timecode(const std::string& s) {
    Timecode tc;

    // Detect drop-frame by separator
    tc.drop_frame = (s.find(';') != std::string::npos);

    char sep;
    if (sscanf(s.c_str(), "%d:%d:%d%c%d", &tc.hh, &tc.mm, &tc.ss, &sep, &tc.ff) != 5) {
        throw std::runtime_error("Invalid timecode format: " + s);
    }

    return tc;
}

static std::string read_timecode_from_stream(AVFormatContext* fmt, int video_stream_index) {
    if (!fmt || video_stream_index < 0 || video_stream_index >= static_cast<int>(fmt->nb_streams)) {
        return {};
    }

    AVStream* st = fmt->streams[video_stream_index];

    // 1) Check video stream metadata (most common)
    if (AVDictionaryEntry* e = av_dict_get(st->metadata, "timecode", nullptr, 0)) {
        return std::string(e->value);
    }

    return {};
}

struct VideoDecoder::Impl {
    AVFormatContext* fmt = nullptr;
    AVCodecContext* dec = nullptr;
    SwsContext* sws = nullptr;

    int video_stream_index = -1;

    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgb = nullptr;

    // Backing storage for rgb->data
    std::vector<uint8_t> rgb_buf;

    bool eof = false;
};

VideoDecoder::VideoDecoder(const std::string& path) {
    impl_ = new Impl();

    int ret = avformat_open_input(&impl_->fmt, path.c_str(), nullptr, nullptr);
    if (ret < 0)
        throw_ff("avformat_open_input failed", ret);

    ret = avformat_find_stream_info(impl_->fmt, nullptr);
    if (ret < 0)
        throw_ff("avformat_find_stream_info failed", ret);

    impl_->video_stream_index = av_find_best_stream(impl_->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (impl_->video_stream_index < 0) {
        throw std::runtime_error("no video stream found in: " + path);
    }

    const std::string time_code = read_timecode_from_stream(impl_->fmt, impl_->video_stream_index);
    if (!time_code.empty()) {
        timecode_ = time_code;
        const Timecode t = parse_timecode(time_code);
        const AVRational fps = get_stream_avg_fps(impl_->fmt, impl_->video_stream_index);
        frame_idx_ = timecode_to_frame_index(t, fps.num, fps.den);
    }

    AVStream* st = impl_->fmt->streams[impl_->video_stream_index];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec)
        throw std::runtime_error("no decoder found for codec_id");

    impl_->dec = avcodec_alloc_context3(codec);
    if (!impl_->dec)
        throw std::runtime_error("avcodec_alloc_context3 failed");

    ret = avcodec_parameters_to_context(impl_->dec, st->codecpar);
    if (ret < 0)
        throw_ff("avcodec_parameters_to_context failed", ret);

    ret = avcodec_open2(impl_->dec, codec, nullptr);
    if (ret < 0)
        throw_ff("avcodec_open2 failed", ret);

    width_ = impl_->dec->width;
    height_ = impl_->dec->height;

    impl_->pkt = av_packet_alloc();
    impl_->frame = av_frame_alloc();
    impl_->rgb = av_frame_alloc();
    if (!impl_->pkt || !impl_->frame || !impl_->rgb)
        throw std::runtime_error("alloc pkt/frame failed");

    // Set up RGB output buffer + swscale
    const AVPixelFormat dst_fmt = AV_PIX_FMT_RGB24;

    int buf_size = av_image_get_buffer_size(dst_fmt, width_, height_, 1);
    if (buf_size < 0)
        throw_ff("av_image_get_buffer_size failed", buf_size);

    impl_->rgb_buf.resize(static_cast<size_t>(buf_size));
    ret = av_image_fill_arrays(
        impl_->rgb->data, impl_->rgb->linesize, impl_->rgb_buf.data(), dst_fmt, width_, height_, 1);
    if (ret < 0)
        throw_ff("av_image_fill_arrays failed", ret);

    rgb_stride_ = impl_->rgb->linesize[0];

    impl_->sws = sws_getContext(
        width_, height_, impl_->dec->pix_fmt, width_, height_, dst_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!impl_->sws)
        throw std::runtime_error("sws_getContext failed");
}

VideoDecoder::~VideoDecoder() {
    if (!impl_)
        return;

    if (impl_->sws)
        sws_freeContext(impl_->sws);
    if (impl_->pkt)
        av_packet_free(&impl_->pkt);
    if (impl_->frame)
        av_frame_free(&impl_->frame);
    if (impl_->rgb)
        av_frame_free(&impl_->rgb);
    if (impl_->dec)
        avcodec_free_context(&impl_->dec);
    if (impl_->fmt)
        avformat_close_input(&impl_->fmt);

    delete impl_;
    impl_ = nullptr;
}

bool VideoDecoder::read(rgb::Frame& out) {
    if (!impl_ || impl_->eof)
        return false;

    int ret = 0;

    while (true) {
        // Read packets until we can produce a frame.
        while ((ret = av_read_frame(impl_->fmt, impl_->pkt)) >= 0) {
            if (impl_->pkt->stream_index != impl_->video_stream_index) {
                av_packet_unref(impl_->pkt);
                continue;
            }

            ret = avcodec_send_packet(impl_->dec, impl_->pkt);
            av_packet_unref(impl_->pkt);
            if (ret < 0)
                throw_ff("avcodec_send_packet failed", ret);

            // Try to receive a frame
            ret = avcodec_receive_frame(impl_->dec, impl_->frame);
            if (ret == 0)
                break;

            if (ret == AVERROR(EAGAIN)) {
                // Need more packets
                continue;
            }

            if (ret == AVERROR_EOF) {
                impl_->eof = true;
                return false;
            }

            throw_ff("avcodec_receive_frame failed", ret);
        }

        if (ret >= 0) {
            // We have impl_->frame filled. Convert to RGB.
            sws_scale(impl_->sws,
                      impl_->frame->data,
                      impl_->frame->linesize,
                      0,
                      height_,
                      impl_->rgb->data,
                      impl_->rgb->linesize);

            // Copy into output rgb::Frame (so caller owns it).
            out.width = width_;
            out.height = height_;
            out.stride = rgb_stride_;
            out.data.resize(static_cast<size_t>(out.stride * out.height));
            std::memcpy(out.data.data(), impl_->rgb_buf.data(), out.data.size());

            av_frame_unref(impl_->frame);
            return true;
        }

        // End of packets: flush decoder
        ret = avcodec_send_packet(impl_->dec, nullptr);
        if (ret < 0)
            throw_ff("avcodec_send_packet(flush) failed", ret);

        ret = avcodec_receive_frame(impl_->dec, impl_->frame);
        if (ret == 0) {
            sws_scale(impl_->sws,
                      impl_->frame->data,
                      impl_->frame->linesize,
                      0,
                      height_,
                      impl_->rgb->data,
                      impl_->rgb->linesize);

            out.width = width_;
            out.height = height_;
            out.stride = rgb_stride_;
            out.data.resize(static_cast<size_t>(out.stride * out.height));
            std::memcpy(out.data.data(), impl_->rgb_buf.data(), out.data.size());

            av_frame_unref(impl_->frame);
            return true;
        }

        impl_->eof = true;
        return false;
    }
}

bool VideoDecoder::discard_frames(int64_t count) {
    rgb::Frame dummy;
    for (int64_t i = 0; i < count; ++i) {
        if (!read(dummy)) {
            return false;
        }
    }
    return true;
}

bool VideoDecoder::seek_seconds(double seconds) {
    if (!impl_ || !impl_->fmt || !impl_->dec)
        return false;
    if (seconds < 0)
        seconds = 0;

    AVStream* st = impl_->fmt->streams[impl_->video_stream_index];
    const AVRational tb = st->time_base;

    // Convert seconds -> stream timestamp units
    int64_t ts = (int64_t)llround(seconds / av_q2d(tb));

    // Seek (prefer avformat_seek_file; it’s more robust)
    int ret = avformat_seek_file(impl_->fmt, impl_->video_stream_index, INT64_MIN, ts, INT64_MAX, AVSEEK_FLAG_BACKWARD);
    if (ret < 0)
        return false;

    // Flush decoder state
    avcodec_flush_buffers(impl_->dec);

    // Clear old packet/frame
    av_packet_unref(impl_->pkt);
    av_frame_unref(impl_->frame);

    impl_->eof = false;

    // Optional: reset your frame index estimate (approx)
    // If you track frame_idx_ strictly as “frames read”, set it to 0 here.
    // If you want it to represent absolute timeline, you can approximate:
    // frame_idx_ = seconds * fps_num / fps_den;
    return true;
}

} // namespace onevr
