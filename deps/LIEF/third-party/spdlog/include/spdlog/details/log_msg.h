// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/common.h>
#include <string>

namespace spdlog {
namespace details {
struct SPDLOG_API log_msg {
    log_msg() = default;
    log_msg(log_clock::time_point log_time,
            source_loc loc,
            string_view_t logger_name,
            level::level_enum lvl,
            string_view_t msg);
    log_msg(source_loc loc, string_view_t logger_name, level::level_enum lvl, string_view_t msg);
    log_msg(string_view_t logger_name, level::level_enum lvl, string_view_t msg);
    log_msg(const log_msg &other) = default;
    log_msg &operator=(const log_msg &other) = default;

    string_view_t logger_name;
    level::level_enum level{level::off};
    log_clock::time_point time;
    size_t thread_id{0};

    // wrapping the formatted text with color (updated by pattern_formatter).
    mutable size_t color_range_start{0};
    mutable size_t color_range_end{0};

    source_loc source;
    string_view_t payload;
};
}  // namespace details
}  // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "log_msg-inl.h"
#endif
