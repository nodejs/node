// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/sinks/ansicolor_sink.h>
#endif

#include <spdlog/details/os.h>
#include <spdlog/pattern_formatter.h>

namespace spdlog {
namespace sinks {

template <typename ConsoleMutex>
SPDLOG_INLINE ansicolor_sink<ConsoleMutex>::ansicolor_sink(FILE *target_file, color_mode mode)
    : target_file_(target_file),
      mutex_(ConsoleMutex::mutex()),
      formatter_(details::make_unique<spdlog::pattern_formatter>())

{
    set_color_mode_(mode);
    colors_.at(level::trace) = to_string_(white);
    colors_.at(level::debug) = to_string_(cyan);
    colors_.at(level::info) = to_string_(green);
    colors_.at(level::warn) = to_string_(yellow_bold);
    colors_.at(level::err) = to_string_(red_bold);
    colors_.at(level::critical) = to_string_(bold_on_red);
    colors_.at(level::off) = to_string_(reset);
}

template <typename ConsoleMutex>
SPDLOG_INLINE void ansicolor_sink<ConsoleMutex>::set_color(level::level_enum color_level,
                                                           string_view_t color) {
    std::lock_guard<mutex_t> lock(mutex_);
    colors_.at(static_cast<size_t>(color_level)) = to_string_(color);
}

template <typename ConsoleMutex>
SPDLOG_INLINE void ansicolor_sink<ConsoleMutex>::log(const details::log_msg &msg) {
    // Wrap the originally formatted message in color codes.
    // If color is not supported in the terminal, log as is instead.
    std::lock_guard<mutex_t> lock(mutex_);
    msg.color_range_start = 0;
    msg.color_range_end = 0;
    memory_buf_t formatted;
    formatter_->format(msg, formatted);
    if (should_do_colors_ && msg.color_range_end > msg.color_range_start) {
        // before color range
        print_range_(formatted, 0, msg.color_range_start);
        // in color range
        print_ccode_(colors_.at(static_cast<size_t>(msg.level)));
        print_range_(formatted, msg.color_range_start, msg.color_range_end);
        print_ccode_(reset);
        // after color range
        print_range_(formatted, msg.color_range_end, formatted.size());
    } else  // no color
    {
        print_range_(formatted, 0, formatted.size());
    }
    fflush(target_file_);
}

template <typename ConsoleMutex>
SPDLOG_INLINE void ansicolor_sink<ConsoleMutex>::flush() {
    std::lock_guard<mutex_t> lock(mutex_);
    fflush(target_file_);
}

template <typename ConsoleMutex>
SPDLOG_INLINE void ansicolor_sink<ConsoleMutex>::set_pattern(const std::string &pattern) {
    std::lock_guard<mutex_t> lock(mutex_);
    formatter_ = std::unique_ptr<spdlog::formatter>(new pattern_formatter(pattern));
}

template <typename ConsoleMutex>
SPDLOG_INLINE void ansicolor_sink<ConsoleMutex>::set_formatter(
    std::unique_ptr<spdlog::formatter> sink_formatter) {
    std::lock_guard<mutex_t> lock(mutex_);
    formatter_ = std::move(sink_formatter);
}

template <typename ConsoleMutex>
SPDLOG_INLINE bool ansicolor_sink<ConsoleMutex>::should_color() const {
    return should_do_colors_;
}

template <typename ConsoleMutex>
SPDLOG_INLINE void ansicolor_sink<ConsoleMutex>::set_color_mode(color_mode mode) {
    std::lock_guard<mutex_t> lock(mutex_);
    set_color_mode_(mode);
}

template <typename ConsoleMutex>
SPDLOG_INLINE void ansicolor_sink<ConsoleMutex>::set_color_mode_(color_mode mode) {
    switch (mode) {
        case color_mode::always:
            should_do_colors_ = true;
            return;
        case color_mode::automatic:
            should_do_colors_ =
                details::os::in_terminal(target_file_) && details::os::is_color_terminal();
            return;
        case color_mode::never:
            should_do_colors_ = false;
            return;
        default:
            should_do_colors_ = false;
    }
}

template <typename ConsoleMutex>
SPDLOG_INLINE void ansicolor_sink<ConsoleMutex>::print_ccode_(
    const string_view_t &color_code) const {
    details::os::fwrite_bytes(color_code.data(), color_code.size(), target_file_);
}

template <typename ConsoleMutex>
SPDLOG_INLINE void ansicolor_sink<ConsoleMutex>::print_range_(const memory_buf_t &formatted,
                                                              size_t start,
                                                              size_t end) const {
    details::os::fwrite_bytes(formatted.data() + start, end - start, target_file_);
}

template <typename ConsoleMutex>
SPDLOG_INLINE std::string ansicolor_sink<ConsoleMutex>::to_string_(const string_view_t &sv) {
    return std::string(sv.data(), sv.size());
}

// ansicolor_stdout_sink
template <typename ConsoleMutex>
SPDLOG_INLINE ansicolor_stdout_sink<ConsoleMutex>::ansicolor_stdout_sink(color_mode mode)
    : ansicolor_sink<ConsoleMutex>(stdout, mode) {}

// ansicolor_stderr_sink
template <typename ConsoleMutex>
SPDLOG_INLINE ansicolor_stderr_sink<ConsoleMutex>::ansicolor_stderr_sink(color_mode mode)
    : ansicolor_sink<ConsoleMutex>(stderr, mode) {}

}  // namespace sinks
}  // namespace spdlog
