#pragma once

#include <string>

extern "C" {
#include <libavutil/error.h>
}

namespace onevr {

inline std::string ff_err(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

} // namespace onevr
