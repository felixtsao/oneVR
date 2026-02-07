#pragma once
#include "onevr/frame.h"

#include <cstdint>
#include <string>

namespace onevr {

enum class EncodeHardware {
    CPU,
    GPU,
};

enum class VideoCodec {
    H264,
    HEVC,
};

struct EncodeSettings {
    int input_width = 0;
    int input_height = 0;
    int output_width = 0;
    int output_height = 0;

    // fps = fps_num / fps_den
    int fps_num = 30000;
    int fps_den = 1001;

    // target bitrate (bits/sec). Reasonable preview default.
    int bitrate = 20'000'000;

    EncodeHardware hardware = EncodeHardware::CPU;
    VideoCodec codec = VideoCodec::H264;

    // Optional quality knobs (best-effort; not all encoders honor all options)
    int gop = 60;       // keyframe interval in frames (e.g., 2 sec @ 30fps)
    int crf = 18;       // x264 only, best-effort
    int cq = 23;        // nvenc only, best-effort
    std::string preset; // "veryfast" for x264, "p4"/"fast" for nvenc (best-effort)
};

class VideoEncoder {
  public:
    VideoEncoder(const std::string& out_path, const EncodeSettings& s);
    ~VideoEncoder();

    VideoEncoder(const VideoEncoder&) = delete;
    VideoEncoder& operator=(const VideoEncoder&) = delete;

    // pts is in encoder time_base ticks (we set time_base = fps_den/fps_num, so pts = frame_index
    // works).
    void write(const rgb::Frame& frame, int64_t pts);
    void write_gpu(uint8_t* frame, int64_t pts);

    // Flush and finalize file (also called by destructor, but explicit is nice).
    void finish();

  private:
    void drain_packets();
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace onevr
