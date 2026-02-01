#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <string>

#include "onevr/image_processing.h"
#include "onevr/video_decoder.h"
#include "onevr/video_encoder.h"
#include "onevr/frame.h"


int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: warper <left.mp4> <right.mp4> <out_dir>\n";
        return 1;
    }

    const std::string left_path = argv[1];
    const std::string right_path = argv[2];
    const std::string out_dir = argv[3];

    std::filesystem::create_directories(out_dir);

    onevr::VideoDecoder left(left_path);
    onevr::VideoDecoder right(right_path);

    int target_width = 8192;
    int target_height = 4096;

    onevr::EncodeSettings es;
    es.output_width = target_width;
    es.output_height = target_height;
    es.fps_num = 29.97;
    es.fps_den = 1;
    es.bitrate = 20'000'000;
    es.hardware = onevr::EncodeHardware::GPU;
    es.codec = onevr::VideoCodec::HEVC;
    es.preset = (es.hardware == onevr::EncodeHardware::GPU) ? "p4" : "veryfast";

    onevr::VideoEncoder enc(out_dir + "out.mp4", es);

    int num_frames = 120;
    for (int i = 0; i < num_frames; ++i) {
        onevr::FrameRGB L, R;
        if (!left.read_rgb24(L) || !right.read_rgb24(R)) break;
        onevr::FrameRGB scaled_l = onevr::scale_rgb24(L, target_width / 2, target_height);
        onevr::FrameRGB scaled_r = onevr::scale_rgb24(R, target_width / 2, target_height);
        onevr::FrameRGB sbs = onevr::sbs_rgb(scaled_l, scaled_r);
        enc.write_rgb24(sbs, /*pts=*/i);
    }

    enc.finish();

    return 0;
}
