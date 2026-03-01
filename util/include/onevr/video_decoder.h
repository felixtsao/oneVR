#pragma once
#include "onevr/frame.h"

#include <string>

namespace onevr {

struct Timecode { // hh:mm:ss:ff
    int hh = 0;
    int mm = 0;
    int ss = 0;
    int ff = 0;
    bool drop_frame = false;
};

// Simple streaming video decoder that outputs RGB24 frames.
class VideoDecoder {
  public:
    explicit VideoDecoder(const std::string& path);
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    // Reads next frame. Returns false on EOF.
    bool read(rgb::Frame& out);
    bool discard_frames(int64_t count);
    bool seek_seconds(double seconds);

    int width() const { return width_; }
    int height() const { return height_; }
    std::string timecode() const { return timecode_; }
    int64_t frame_index() const { return frame_idx_; }

  private:
    struct Impl;
    Impl* impl_ = nullptr;

    int width_ = 0;
    int height_ = 0;

    // We keep these cached because they're used for allocation decisions.
    int rgb_stride_ = 0;

    std::string timecode_;
    int64_t frame_idx_ = 0;
};

} // namespace onevr
