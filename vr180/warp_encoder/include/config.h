#pragma once
#include "onevr/video_encoder.h"
#include "warp.h"

#include <string>

namespace onevr::vr180 {

struct FilesConfig {
    std::string camera_config;
    std::string input_left;
    std::string input_right;
    std::string output_directory;
    std::string output_composite;
};

struct OutputSettings {
    int width = 0;
    int height = 0;
    int fps_num = 30000;
    int fps_den = 1001;
    int start_time_seconds = 0;
    int duration_seconds = 1;
    float contrast = 1.0f;
    int brightness = 0;
};

struct SoundSettings {
    bool left_source = true;
    bool right_source = false;
};

struct Config {
    FilesConfig files;
    OutputSettings output_settings;
    SoundSettings sound_settings;

    // Loaded directly into runtime structs:
    onevr::vr180::Camera camera_parameters;   // from cam/<...>/config.yaml
    onevr::vr180::WarpSettings warp_settings; // derived from camera + output, with optional overrides
    onevr::EncodeSettings encode_settings;    // from encode_settings + output_settings
};

// Loads app config YAML, then loads camera config YAML referenced inside it,
// and returns fully-populated runtime settings structs.
Config LoadConfigYaml(const std::string& config_path);
void print_config(const Config& config);

} // namespace onevr::vr180