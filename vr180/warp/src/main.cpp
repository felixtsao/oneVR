#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <string>

#include <cuda_runtime.h>

#include "onevr/image_processing.h"
#include "onevr/video_decoder.h"
#include "onevr/video_encoder.h"
#include "onevr/frame.h"

#include "warp.h"


int main(int argc, char** argv) {

    if (argc < 4) {
        std::cerr << "usage: warp <left.mp4> <right.mp4> <out_dir>\n";
        return 1;
    }

    const std::string left_path = argv[1];
    const std::string right_path = argv[2];
    const std::string out_dir = argv[3];

    std::filesystem::create_directories(out_dir);

    onevr::VideoDecoder left(left_path);
    onevr::VideoDecoder right(right_path);

    onevr::EncodeSettings es;
    es.input_width = 4096;
    es.input_height = 4096;
    es.output_width = 4096;
    es.output_height = 4096;
    es.fps_num = 30000;
    es.fps_den = 1001;
    es.bitrate = 20'000'000;
    es.hardware = onevr::EncodeHardware::GPU;
    es.codec = onevr::VideoCodec::HEVC;
    es.preset = (es.hardware == onevr::EncodeHardware::GPU) ? "p4" : "medium";
    onevr::VideoEncoder enc(out_dir + "out.mp4", es);

    onevr::vr180::Camera cam;
    cam.width = left.width();
    cam.height = left.height();
    cam.hfov_degrees = 120.0f;
    onevr::vr180::Vr180WarpSettings ws;
    ws.eye_width = 4096;
    ws.eye_height = 4096;
    ws.interpolation_method = onevr::vr180::InterpolationMethod::BILINEAR;
    onevr::UvMap lut = onevr::vr180::create_warp_slut(cam, ws);

    int i = 0;
    while (1) {
        onevr::rgb::Frame L, R;
        if (!left.read(L) || !right.read(R)) break;
        switch(es.hardware) {
            case onevr::EncodeHardware::CPU: {
                onevr::rgb::Frame warpedL = onevr::vr180::cuda::warp(L, lut, onevr::vr180::InterpolationMethod::BILINEAR);
                onevr::rgb::Frame warpedR = onevr::vr180::cuda::warp(R, lut, onevr::vr180::InterpolationMethod::BILINEAR);
                onevr::rgb::Frame sbs = onevr::cat_sbs(warpedL, warpedR);
                enc.write(sbs, /*pts=*/i++);
                break;
            }
            case onevr::EncodeHardware::GPU: {
                uint8_t* warped_l = nullptr;
                size_t warped_l_bytes = (size_t)es.output_width * es.output_height * 3;
                cudaMalloc(&warped_l, warped_l_bytes);
                onevr::vr180::cuda::warp(L, lut, onevr::vr180::InterpolationMethod::BILINEAR, warped_l);
                enc.write_gpu(warped_l, /*pts=*/i++);
                break;
            }
        }
    }

    enc.finish();

    return 0;
}
