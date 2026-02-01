#pragma once
#include <string>
#include "onevr/frame.h"

namespace onevr {

// Simple streaming video decoder that outputs RGB24 frames.
class VideoDecoder {
public:
    explicit VideoDecoder(const std::string& path);
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    // Reads next frame. Returns false on EOF.
    bool read_rgb24(FrameRGB& out);

    int width() const { return width_; }
    int height() const { return height_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;

    int width_ = 0;
    int height_ = 0;

    // We keep these cached because they're used for allocation decisions.
    int rgb_stride_ = 0;
};

}  // namespace onevr
