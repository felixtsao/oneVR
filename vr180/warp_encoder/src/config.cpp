#include "config.h"

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

namespace onevr::vr180 {

static std::runtime_error die(const std::string& msg) {
    return std::runtime_error("[config] " + msg);
}

static YAML::Node req_node(const YAML::Node& root, const char* key) {
    auto n = root[key];
    if (!n)
        throw die(std::string("missing section/key: ") + key);
    return n;
}

static std::string req_s(const YAML::Node& n, const char* key) {
    auto v = n[key];
    if (!v)
        throw die(std::string("missing key: ") + key);
    return v.as<std::string>();
}

static std::string opt_s(const YAML::Node& n, const char* key, const std::string& def) {
    auto v = n[key];
    return v ? v.as<std::string>() : def;
}

static int opt_i(const YAML::Node& n, const char* key, int def) {
    auto v = n[key];
    return v ? v.as<int>() : def;
}

static int64_t opt_i64(const YAML::Node& n, const char* key, int64_t def) {
    auto v = n[key];
    return v ? v.as<int64_t>() : def;
}

static float opt_f(const YAML::Node& n, const char* key, float def) {
    auto v = n[key];
    return v ? v.as<float>() : def;
}

static double opt_d(const YAML::Node& n, const char* key, double def) {
    auto v = n[key];
    return v ? v.as<double>() : def;
}

static bool opt_b(const YAML::Node& n, const char* key, bool def) {
    auto v = n[key];
    return v ? v.as<bool>() : def;
}

static std::string lower(std::string s) {
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

static void parse_fps_node(const YAML::Node& fps_node, int& fps_num, int& fps_den) {
    if (!fps_node)
        return;

    if (!fps_node.IsScalar())
        throw die("fps must be a scalar (e.g. 30 or 29.97)");

    double fps = fps_node.as<double>();
    if (fps <= 0.0)
        throw die("fps must be > 0");

    fps_num = static_cast<int>(std::lround(fps));
    fps_den = 1;
}

static onevr::VideoCodec parse_codec(std::string s) {
    s = lower(s);
    if (s == "h264" || s == "avc")
        return onevr::VideoCodec::H264;
    if (s == "hevc" || s == "h265")
        return onevr::VideoCodec::HEVC;
    throw die("encode_settings.codec must be one of: h264, hevc");
}

static onevr::EncodeHardware parse_hw(std::string s) {
    s = lower(s);
    if (s == "cpu")
        return onevr::EncodeHardware::CPU;
    if (s == "gpu" || s == "nvenc")
        return onevr::EncodeHardware::GPU;
    throw die("encode_settings.hardware must be one of: cpu, gpu");
}

// --- Camera config loading ---
// Camera YAML can be either top-level keys or under a "camera:" map.
// This returns a node that contains the camera fields.
static YAML::Node camera_node_from_file(const std::string& path) {
    YAML::Node cam_root = YAML::LoadFile(path);
    if (cam_root["camera"])
        return cam_root["camera"];
    return cam_root;
}

static void fill_camera_from_yaml(onevr::vr180::Camera& cam, const YAML::Node& n) {
    if (n["name"])
        cam.name = n["name"].as<std::string>();
    if (n["horizontal_fov_degrees"])
        cam.hfov_degrees = (float)n["horizontal_fov_degrees"].as<double>();
    if (n["intrinsics"]) {
        auto i = n["intrinsics"];
        cam.fx = (float)opt_d(i, "fx", 0.0);
        cam.fy = (float)opt_d(i, "fy", 0.0);
        cam.cx = (float)opt_d(i, "cx", -1.0);
        cam.cy = (float)opt_d(i, "cy", -1.0);
    }
    if (n["lens_distortion"]) {
        cam.lens_distortion = n["lens_distortion"].as<bool>();
    }
    if (n["lens_distortion_coefficients"]) {
        auto d = n["lens_distortion_coefficients"];
        cam.lens_distortion_coefficients.k1 = (float)opt_d(d, "k1", 0.0);
        cam.lens_distortion_coefficients.k2 = (float)opt_d(d, "k2", 0.0);
        cam.lens_distortion_coefficients.k3 = (float)opt_d(d, "k3", 0.0);
        cam.lens_distortion_coefficients.p1 = (float)opt_d(d, "p1", 0.0);
        cam.lens_distortion_coefficients.p2 = (float)opt_d(d, "p2", 0.0);
    }
}

static const char* to_string(EncodeHardware h) {
    switch (h) {
        case EncodeHardware::CPU:
            return "cpu";
        case EncodeHardware::GPU:
            return "gpu";
    }
    return "unknown";
}

static const char* to_string(VideoCodec c) {
    switch (c) {
        case VideoCodec::H264:
            return "h264";
        case VideoCodec::HEVC:
            return "hevc";
    }
    return "unknown";
}

void print_config(const Config& c) {
    std::cout << "\n======================== vr180/warp_encoder runtime config "
                 "========================\n";

    std::cout << "\n[files]\n";
    std::cout << "    camera_config:         " << c.files.camera_config << "\n";
    std::cout << "    input_left:            " << c.files.input_left << "\n";
    std::cout << "    input_right:           " << c.files.input_right << "\n";
    std::cout << "    output_directory:      " << c.files.output_directory << "\n";
    std::cout << "    output_composite:      " << c.files.output_composite << "\n";

    std::cout << "\n[output_settings]\n";
    std::cout << "    width:                 " << c.output_settings.width << "\n";
    std::cout << "    height:                " << c.output_settings.height << "\n";
    std::cout << "    fps:                   " << c.output_settings.fps_num << "/" << c.output_settings.fps_den << "\n";
    std::cout << "    start_time_seconds:    " << c.output_settings.start_time_seconds << "\n";
    std::cout << "    duration_seconds:      " << c.output_settings.duration_seconds << "\n";
    std::cout << "    contrast:              " << c.output_settings.contrast << "\n";
    std::cout << "    brightness:            " << c.output_settings.brightness << "\n";

    std::cout << "\n[encode_settings]\n";
    std::cout << "    hardware:              " << to_string(c.encode_settings.hardware) << "\n";
    std::cout << "    codec:                 " << to_string(c.encode_settings.codec) << "\n";
    std::cout << "    preset:                " << c.encode_settings.preset << "\n";
    std::cout << "    cq:                    " << c.encode_settings.cq << "\n";
    std::cout << "    crf:                   " << c.encode_settings.crf << "\n";
    std::cout << "    bitrate:               " << c.encode_settings.bitrate << "\n";
    std::cout << "    gop:                   " << c.encode_settings.gop << "\n";

    std::cout << "\n[sound_settings]\n";
    std::cout << "    left_source:           " << (c.sound_settings.left_source ? "true" : "false") << "\n";
    std::cout << "    right_source:          " << (c.sound_settings.right_source ? "true" : "false") << "\n";

    std::cout << "\n[warp_settings]\n";
    std::cout << "    eye_width:             " << c.warp_settings.eye_width << "\n";
    std::cout << "    eye_height:            " << c.warp_settings.eye_height << "\n";
    std::cout << "    yaw_half_rad:          " << c.warp_settings.yaw_half_rad << "\n";
    std::cout << "    pitch_half_rad:        " << c.warp_settings.pitch_half_rad << "\n";

    std::cout << "\n[camera_config]\n";
    std::cout << "    width:                 " << c.camera_parameters.width << "\n";
    std::cout << "    height:                " << c.camera_parameters.height << "\n";
    std::cout << "    hfov_degrees:          " << c.camera_parameters.hfov_degrees << "\n";
    std::cout << "    intrinsics:\n";
    std::cout << "        fx:                " << c.camera_parameters.fx << "\n";
    std::cout << "        fy:                " << c.camera_parameters.fy << "\n";
    std::cout << "        cx:                " << c.camera_parameters.cx << "\n";
    std::cout << "        cy:                " << c.camera_parameters.cy << "\n";
    std::cout << "    lens_distortion:       " << c.camera_parameters.lens_distortion << "\n";
    std::cout << "        k1:                " << c.camera_parameters.lens_distortion_coefficients.k1 << "\n";
    std::cout << "        k2:                " << c.camera_parameters.lens_distortion_coefficients.k2 << "\n";
    std::cout << "        k3:                " << c.camera_parameters.lens_distortion_coefficients.k3 << "\n";
    std::cout << "        p1:                " << c.camera_parameters.lens_distortion_coefficients.p1 << "\n";
    std::cout << "        p2:                " << c.camera_parameters.lens_distortion_coefficients.p2 << "\n";

    std::cout << "\n==============================================================================="
                 "====\n\n";
}

Config LoadConfigYaml(const std::string& config_path) {
    YAML::Node root = YAML::LoadFile(config_path);

    Config cfg;

    // ---------------- files ----------------
    {
        auto f = req_node(root, "files");
        cfg.files.camera_config = req_s(f, "camera_config");
        cfg.files.input_left = req_s(f, "input_left");
        cfg.files.input_right = req_s(f, "input_right");
        cfg.files.output_directory = req_s(f, "output_directory");
        cfg.files.output_composite = req_s(f, "output_composite");
    }

    // ---------------- sound_settings ----------------
    {
        auto s = root["sound_settings"];
        if (s) {
            cfg.sound_settings.left_source = opt_b(s, "left_source", true);
            cfg.sound_settings.right_source = opt_b(s, "right_source", false);
        } else {
            cfg.sound_settings.left_source = true;
            cfg.sound_settings.right_source = false;
        }
        if (!cfg.sound_settings.left_source && !cfg.sound_settings.right_source) {
            // Allow it, but it's almost certainly a mistake; fail fast.
            throw die("sound_settings: at least one of left_source/right_source should be true");
        }
    }

    // ---------------- output_settings ----------------
    {
        auto o = req_node(root, "output_settings");
        cfg.output_settings.width = opt_i(o, "width", 8192);
        cfg.output_settings.height = opt_i(o, "height", 4096);
        cfg.output_settings.start_time_seconds = opt_i(o, "start_time_seconds", 0);
        cfg.output_settings.duration_seconds = opt_i(o, "duration_seconds", 1);
        cfg.output_settings.contrast = opt_f(o, "contrast", 1.0f);
        cfg.output_settings.brightness = opt_i(o, "brightness", 0);
        parse_fps_node(o["fps"], cfg.output_settings.fps_num, cfg.output_settings.fps_den);

        if (cfg.output_settings.width <= 0 || cfg.output_settings.height <= 0)
            throw die("output_settings.width/height must be > 0");
        if ((cfg.output_settings.width % 2) || (cfg.output_settings.height % 2))
            throw die("output_settings.width/height must be even");
    }

    // ---------------- encode_settings -> onevr::EncodeSettings ----------------
    {
        auto e = req_node(root, "encode_settings");

        // These field names assume your existing onevr::EncodeSettings:
        //  - hardware (EncodeHardware)
        //  - codec (VideoCodec)
        //  - preset (std::string)
        //  - cq, crf, bitrate, gop (ints)
        //  - fps_num/fps_den (ints)
        //  - input_width/input_height, output_width/output_height (ints)
        //
        // If your struct differs, adjust here.

        cfg.encode_settings.hardware = parse_hw(req_s(e, "hardware"));
        cfg.encode_settings.codec = parse_codec(req_s(e, "codec"));
        cfg.encode_settings.preset = opt_s(e, "preset", cfg.encode_settings.preset);

        cfg.encode_settings.cq = opt_i(e, "cq", cfg.encode_settings.cq);
        cfg.encode_settings.crf = opt_i(e, "crf", cfg.encode_settings.crf);
        cfg.encode_settings.bitrate = opt_i64(e, "bitrate", cfg.encode_settings.bitrate);
        cfg.encode_settings.gop = opt_i(e, "gop", cfg.encode_settings.gop);

        cfg.encode_settings.fps_num = cfg.output_settings.fps_num;
        cfg.encode_settings.fps_den = cfg.output_settings.fps_den;

        cfg.encode_settings.input_width = cfg.output_settings.width;
        cfg.encode_settings.input_height = cfg.output_settings.height;
        cfg.encode_settings.output_width = cfg.output_settings.width;
        cfg.encode_settings.output_height = cfg.output_settings.height;
    }

    // ---------------- camera_config -> onevr::Camera ----------------
    {
        YAML::Node cam_n = camera_node_from_file(cfg.files.camera_config);
        fill_camera_from_yaml(cfg.camera_parameters, cam_n);
    }

    return cfg;
}

} // namespace onevr::vr180
