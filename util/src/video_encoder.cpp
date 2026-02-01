#include "onevr/video_encoder.h"
#include "onevr/ffmpeg_util.h"

#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace onevr {

static void throw_ff(const char* what, int err) {
    throw std::runtime_error(std::string(what) + ": " + ff_err(err));
}

static void opt_set_if(AVCodecContext* ctx, const char* key, const char* val) {
    if (!ctx || !ctx->priv_data || !key || !val) return;
    // Best-effort: ignore errors since options vary per build/encoder.
    av_opt_set(ctx->priv_data, key, val, 0);
}

static void opt_set_int_if(AVCodecContext* ctx, const char* key, int val) {
    if (!ctx || !ctx->priv_data || !key) return;
    av_opt_set_int(ctx->priv_data, key, val, 0);
}

struct VideoEncoder::Impl {
    EncodeSettings s;

    AVFormatContext* ofmt = nullptr;
    AVStream* vstream = nullptr;
    AVCodecContext* enc = nullptr;
    const AVCodec* codec = nullptr;

    SwsContext* sws = nullptr;

    AVFrame* yuv = nullptr;
    AVPacket* pkt = nullptr;

    bool wrote_header = false;
    bool finished = false;

    AVPixelFormat dst_fmt = AV_PIX_FMT_YUV420P;
};

static const AVCodec* pick_encoder(const EncodeSettings& s) {
    switch (s.hardware) {
    case EncodeHardware::GPU:
        switch (s.codec) {
        case VideoCodec::H264:
            if (const AVCodec* c = avcodec_find_encoder_by_name("h264_nvenc")) return c;
            break;
        case VideoCodec::HEVC:
            if (const AVCodec* c = avcodec_find_encoder_by_name("hevc_nvenc")) return c;
            break;
        }
        fprintf(stderr, "[onevr] NVENC not available, falling back to CPU\n");
        [[fallthrough]];
    case EncodeHardware::CPU:
        switch (s.codec) {
        case VideoCodec::H264:
            if (const AVCodec* c = avcodec_find_encoder_by_name("libx264")) return c;
            return avcodec_find_encoder(AV_CODEC_ID_H264);
        case VideoCodec::HEVC:
            if (const AVCodec* c = avcodec_find_encoder_by_name("libx265")) return c;
            return avcodec_find_encoder(AV_CODEC_ID_HEVC);
        }
        break;
    }

    return nullptr;
}

static AVPixelFormat pick_pix_fmt(const EncodeSettings& s, const AVCodec* codec) {
    (void)codec;
    if (s.hardware == EncodeHardware::GPU) {
        // NVENC commonly prefers NV12 for 8-bit
        return AV_PIX_FMT_NV12;
    }
    return AV_PIX_FMT_YUV420P;
}

VideoEncoder::VideoEncoder(const std::string& out_path, const EncodeSettings& settings) {
    impl_ = new Impl();
    impl_->s = settings;

    if (impl_->s.output_width <= 0 || impl_->s.output_height <= 0) {
        throw std::runtime_error("EncodeSettings width/height must be set");
    }

    // H.264 encoders generally want even dimensions for 4:2:0 formats.
    if ((impl_->s.output_width % 2) || (impl_->s.output_height % 2)) {
        throw std::runtime_error("Output width/height must be even for YUV420/NV12 encodes");
    }

    int ret = avformat_alloc_output_context2(&impl_->ofmt, nullptr, "mp4", out_path.c_str());
    if (ret < 0 || !impl_->ofmt) throw_ff("avformat_alloc_output_context2 failed", ret);

    impl_->codec = pick_encoder(impl_->s);
    if (!impl_->codec) throw std::runtime_error("No H.264 encoder found (libx264 or built-in h264)");

    impl_->dst_fmt = pick_pix_fmt(impl_->s, impl_->codec);

    impl_->vstream = avformat_new_stream(impl_->ofmt, nullptr);
    if (!impl_->vstream) throw std::runtime_error("avformat_new_stream failed");

    impl_->enc = avcodec_alloc_context3(impl_->codec);
    if (!impl_->enc) throw std::runtime_error("avcodec_alloc_context3 failed");

    impl_->enc->codec_id = impl_->codec->id;
    impl_->enc->codec_type = AVMEDIA_TYPE_VIDEO;
    impl_->enc->width = impl_->s.output_width;
    impl_->enc->height = impl_->s.output_height;
    impl_->enc->pix_fmt = impl_->dst_fmt;

    // time_base = seconds per tick; for CFR encoding we use 1/fps ticks.
    impl_->enc->time_base = AVRational{impl_->s.fps_den, impl_->s.fps_num};
    impl_->enc->framerate = AVRational{impl_->s.fps_num, impl_->s.fps_den};

    impl_->enc->bit_rate = impl_->s.bitrate;
    impl_->enc->gop_size = impl_->s.gop;
    impl_->enc->max_b_frames = 0; // keep simple for now

    // Encoder-specific options (best effort; safe to ignore if unsupported)
    if (impl_->s.hardware == EncodeHardware::CPU) {
        // x264 quality/preset. If libx264 isn't the picked encoder, these may be ignored.
        if (impl_->s.preset.empty()) impl_->s.preset = "veryfast";
        opt_set_if(impl_->enc, "preset", impl_->s.preset.c_str());
        opt_set_int_if(impl_->enc, "crf", impl_->s.crf);
        // "tune=zerolatency" is nice for preview; optional.
        opt_set_if(impl_->enc, "tune", "zerolatency");
    } else {
        // NVENC: prefer constant quality-ish mode if possible
        // presets differ by build: "p1".."p7" or "fast"/"medium"/"slow".
        if (impl_->s.preset.empty()) impl_->s.preset = "p4";
        opt_set_if(impl_->enc, "preset", impl_->s.preset.c_str());
        opt_set_int_if(impl_->enc, "cq", impl_->s.cq);
        // Some builds prefer "rc=vbr" or "rc=constqp". We'll set vbr best-effort.
        opt_set_if(impl_->enc, "rc", "vbr");
    }

    // If the format requires global headers, enable them.
    if (impl_->ofmt->oformat->flags & AVFMT_GLOBALHEADER) {
        impl_->enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(impl_->enc, impl_->codec, nullptr);
    if (ret < 0) throw_ff("avcodec_open2 failed", ret);

    // Stream time_base should match encoder time_base for clean pts handling.
    impl_->vstream->time_base = impl_->enc->time_base;
    impl_->vstream->avg_frame_rate = impl_->enc->framerate;
    impl_->vstream->r_frame_rate = impl_->enc->framerate;

    ret = avcodec_parameters_from_context(impl_->vstream->codecpar, impl_->enc);
    if (ret < 0) throw_ff("avcodec_parameters_from_context failed", ret);

    // Open output file
    if (!(impl_->ofmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&impl_->ofmt->pb, out_path.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) throw_ff("avio_open failed", ret);
    }

    ret = avformat_write_header(impl_->ofmt, nullptr);
    if (ret < 0) throw_ff("avformat_write_header failed", ret);
    impl_->wrote_header = true;

    // Allocate a reusable output frame in dst pixel format
    impl_->yuv = av_frame_alloc();
    impl_->pkt = av_packet_alloc();
    if (!impl_->yuv || !impl_->pkt) throw std::runtime_error("av_frame_alloc/av_packet_alloc failed");

    impl_->yuv->format = impl_->dst_fmt;
    impl_->yuv->width = impl_->s.output_width;
    impl_->yuv->height = impl_->s.output_height;

    ret = av_frame_get_buffer(impl_->yuv, 32);
    if (ret < 0) throw_ff("av_frame_get_buffer failed", ret);

    // RGB24 -> dst_fmt swscale
    impl_->sws = sws_getContext(
        impl_->s.input_width, impl_->s.input_height, AV_PIX_FMT_RGB24,
        impl_->s.output_width, impl_->s.output_height, impl_->dst_fmt,
        SWS_BICUBIC,
        nullptr, nullptr, nullptr
    );
    if (!impl_->sws) throw std::runtime_error("sws_getContext failed");
}

VideoEncoder::~VideoEncoder() {
    try {
        finish();
    } catch (...) {
        // Destructors should not throw; swallow.
    }

    if (!impl_) return;

    if (impl_->sws) sws_freeContext(impl_->sws);
    if (impl_->pkt) av_packet_free(&impl_->pkt);
    if (impl_->yuv) av_frame_free(&impl_->yuv);
    if (impl_->enc) avcodec_free_context(&impl_->enc);

    if (impl_->ofmt) {
        if (impl_->wrote_header) {
            // Trailer would have been written in finish(), but safe to close pb regardless.
        }
        if (!(impl_->ofmt->oformat->flags & AVFMT_NOFILE) && impl_->ofmt->pb) {
            avio_closep(&impl_->ofmt->pb);
        }
        avformat_free_context(impl_->ofmt);
        impl_->ofmt = nullptr;
    }

    delete impl_;
    impl_ = nullptr;
}

void VideoEncoder::write(const rgb::Frame& frame, int64_t pts) {
    if (!impl_ || impl_->finished) throw std::runtime_error("encoder is finished");

    if (frame.width != impl_->s.input_width || frame.height != impl_->s.input_height) {
        throw std::runtime_error("write_rgb24: frame size mismatch");
    }
    if (frame.stride < frame.width * 3) {
        throw std::runtime_error("write_rgb24: invalid stride");
    }

    int ret = av_frame_make_writable(impl_->yuv);
    if (ret < 0) throw_ff("av_frame_make_writable failed", ret);

    // Prepare RGB source slice pointers
    const uint8_t* src_slices[4] = { frame.data.data(), nullptr, nullptr, nullptr };
    int src_stride[4] = { frame.stride, 0, 0, 0 };

    sws_scale(
        impl_->sws,
        src_slices,
        src_stride,
        0,
        impl_->s.output_height,
        impl_->yuv->data,
        impl_->yuv->linesize
    );

    impl_->yuv->pts = pts;

    ret = avcodec_send_frame(impl_->enc, impl_->yuv);
    if (ret < 0) throw_ff("avcodec_send_frame failed", ret);

    while (true) {
        ret = avcodec_receive_packet(impl_->enc, impl_->pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) throw_ff("avcodec_receive_packet failed", ret);

        // Rescale from encoder time_base to stream time_base (usually same, but do it correctly).
        av_packet_rescale_ts(impl_->pkt, impl_->enc->time_base, impl_->vstream->time_base);
        impl_->pkt->stream_index = impl_->vstream->index;

        ret = av_interleaved_write_frame(impl_->ofmt, impl_->pkt);
        av_packet_unref(impl_->pkt);
        if (ret < 0) throw_ff("av_interleaved_write_frame failed", ret);
    }
}

void VideoEncoder::finish() {
    if (!impl_ || impl_->finished) return;

    // Flush encoder
    int ret = avcodec_send_frame(impl_->enc, nullptr);
    if (ret < 0) throw_ff("avcodec_send_frame(flush) failed", ret);

    while (true) {
        ret = avcodec_receive_packet(impl_->enc, impl_->pkt);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) break;
        if (ret < 0) throw_ff("avcodec_receive_packet(flush) failed", ret);

        av_packet_rescale_ts(impl_->pkt, impl_->enc->time_base, impl_->vstream->time_base);
        impl_->pkt->stream_index = impl_->vstream->index;

        int wret = av_interleaved_write_frame(impl_->ofmt, impl_->pkt);
        av_packet_unref(impl_->pkt);
        if (wret < 0) throw_ff("av_interleaved_write_frame(flush) failed", wret);
    }

    av_write_trailer(impl_->ofmt);
    impl_->finished = true;
}

}  // namespace onevr
