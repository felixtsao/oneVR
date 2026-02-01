#include <filesystem>
#include <iostream>
#include <string>

#include "onevr/image_util.h"
#include "onevr/video_decoder.h"

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

    onevr::FrameRGB L, R;

    std::cout << "Reading first frame...\n";
    if (!left.read_rgb24(L)) {
        std::cerr << "failed to read left first frame\n";
        return 2;
    }
    if (!right.read_rgb24(R)) {
        std::cerr << "failed to read right first frame\n";
        return 2;
    }

    int i = 0;
    while (i < 5 && left.read_rgb24(L) && right.read_rgb24(R)) {
        onevr::write_ppm(out_dir + "/left_" + std::to_string(i) + ".ppm", L);
        onevr::write_ppm(out_dir + "/right_" + std::to_string(i) + ".ppm", R);
        ++i;
    }

    std::cout << "Wrote PPMs into " << out_dir << "\n";
    return 0;
}
