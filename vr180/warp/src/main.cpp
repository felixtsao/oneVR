#include "config.h"
#include "onevr/frame.h"
#include "onevr/image_processing.h"
#include "onevr/video_decoder.h"
#include "onevr/video_encoder.h"
#include "warp.h"

#include <cstring>
#include <cuda_runtime.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {

    if (argc < 2) {
        std::cerr << "usage: warp_encoder /vr180/warp_encoder/config.yaml\n";
        return 1;
    }

    const std::string config_path = "/workspace/dev/oneVR/vr180/warp/config.yaml";
    onevr::vr180::Config config = onevr::vr180::LoadConfigYaml(config_path);

    onevr::VideoDecoder decoder_left(config.files.input_left);
    onevr::VideoDecoder decoder_right(config.files.input_right);

    // Update camera sensor size from decoded video as multiple resolutions can be supported by most
    // cameras
    config.camera_parameters.width = decoder_left.width();
    config.camera_parameters.height = decoder_left.height();

    onevr::vr180::print_config(config);

    onevr::VideoEncoder encoder(std::filesystem::path(config.files.output_directory) / config.files.output_composite,
                                config.encode_settings);

    onevr::UvMap lut = onevr::vr180::slut(config.camera_parameters, config.warp_settings);

    // Allocate buffers
    onevr::rgb::Frame input_left, input_right;
    uint8_t* sbs_composite = nullptr;
    if (config.encode_settings.hardware == onevr::EncodeHardware::GPU) {
        const size_t sbs_composite_bytes =
            (size_t)config.encode_settings.output_width * config.encode_settings.output_height * 3; // 3 channel RGB
        cudaMalloc(&sbs_composite, sbs_composite_bytes);
    }

    int i = 0;
    while (1) {
        if (!decoder_left.read(input_left) || !decoder_right.read(input_right)) {
            break;
        }
        switch (config.encode_settings.hardware) {
            case onevr::EncodeHardware::CPU: {
                onevr::rgb::Frame warped_left =
                    onevr::vr180::cuda::warp(input_left, lut, onevr::vr180::InterpolationMethod::BILINEAR);
                onevr::rgb::Frame warped_right =
                    onevr::vr180::cuda::warp(input_right, lut, onevr::vr180::InterpolationMethod::BILINEAR);
                onevr::rgb::Frame sbs = onevr::cat_sbs(warped_left, warped_right);
                encoder.write(sbs, /*pts=*/i++);
                break;
            }
            case onevr::EncodeHardware::GPU: {
                onevr::vr180::cuda::warp(
                    input_left, lut, 0, onevr::vr180::InterpolationMethod::BILINEAR, sbs_composite);
                onevr::vr180::cuda::warp(input_right,
                                         lut,
                                         config.warp_settings.eye_width,
                                         onevr::vr180::InterpolationMethod::BILINEAR,
                                         sbs_composite);
                encoder.write_gpu(sbs_composite, /*pts=*/i++);
                break;
            }
        }
    }

    encoder.finish();
    if (sbs_composite)
        cudaFree(sbs_composite);

    return 0;
}
