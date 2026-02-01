#include "onevr/video_decoder.h"
#include "onevr/ffmpeg_util.h"

#include <cstring>
#include <stdexcept>
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
    if (ret < 0) throw_ff("avformat_open_input failed", ret);

    ret = avformat_find_stream_info(impl_->fmt, nullptr);
    if (ret < 0) throw_ff("avformat_find_stream_info failed", ret);

    impl_->video_stream_index = av_find_best_stream(impl_->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (impl_->video_stream_index < 0) {
        throw std::runtime_error("no video stream found in: " + path);
    }

    AVStream* st = impl_->fmt->streams[impl_->video_stream_index];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) throw std::runtime_error("no decoder found for codec_id");

    impl_->dec = avcodec_alloc_context3(codec);
    if (!impl_->dec) throw std::runtime_error("avcodec_alloc_context3 failed");

    ret = avcodec_parameters_to_context(impl_->dec, st->codecpar);
    if (ret < 0) throw_ff("avcodec_parameters_to_context failed", ret);

    ret = avcodec_open2(impl_->dec, codec, nullptr);
    if (ret < 0) throw_ff("avcodec_open2 failed", ret);

    width_ = impl_->dec->width;
    height_ = impl_->dec->height;

    impl_->pkt = av_packet_alloc();
    impl_->frame = av_frame_alloc();
    impl_->rgb = av_frame_alloc();
    if (!impl_->pkt || !impl_->frame || !impl_->rgb) throw std::runtime_error("alloc pkt/frame failed");

    // Set up RGB output buffer + swscale
    const AVPixelFormat dst_fmt = AV_PIX_FMT_RGB24;

    int buf_size = av_image_get_buffer_size(dst_fmt, width_, height_, 1);
    if (buf_size < 0) throw_ff("av_image_get_buffer_size failed", buf_size);

    impl_->rgb_buf.resize(static_cast<size_t>(buf_size));
    ret = av_image_fill_arrays(
        impl_->rgb->data,
        impl_->rgb->linesize,
        impl_->rgb_buf.data(),
        dst_fmt,
        width_,
        height_,
        1
    );
    if (ret < 0) throw_ff("av_image_fill_arrays failed", ret);

    rgb_stride_ = impl_->rgb->linesize[0];

    impl_->sws = sws_getContext(
        width_, height_, impl_->dec->pix_fmt,
        width_, height_, dst_fmt,
        SWS_BILINEAR,
        nullptr, nullptr, nullptr
    );
    if (!impl_->sws) throw std::runtime_error("sws_getContext failed");
}

VideoDecoder::~VideoDecoder() {
    if (!impl_) return;

    if (impl_->sws) sws_freeContext(impl_->sws);
    if (impl_->pkt) av_packet_free(&impl_->pkt);
    if (impl_->frame) av_frame_free(&impl_->frame);
    if (impl_->rgb) av_frame_free(&impl_->rgb);
    if (impl_->dec) avcodec_free_context(&impl_->dec);
    if (impl_->fmt) avformat_close_input(&impl_->fmt);

    delete impl_;
    impl_ = nullptr;
}

bool VideoDecoder::read(rgb::Frame& out) {
    if (!impl_ || impl_->eof) return false;

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
            if (ret < 0) throw_ff("avcodec_send_packet failed", ret);

            // Try to receive a frame
            ret = avcodec_receive_frame(impl_->dec, impl_->frame);
            if (ret == 0) break;

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
            sws_scale(
                impl_->sws,
                impl_->frame->data,
                impl_->frame->linesize,
                0,
                height_,
                impl_->rgb->data,
                impl_->rgb->linesize
            );

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
        if (ret < 0) throw_ff("avcodec_send_packet(flush) failed", ret);

        ret = avcodec_receive_frame(impl_->dec, impl_->frame);
        if (ret == 0) {
            sws_scale(
                impl_->sws,
                impl_->frame->data,
                impl_->frame->linesize,
                0,
                height_,
                impl_->rgb->data,
                impl_->rgb->linesize
            );

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

}  // namespace onevr
