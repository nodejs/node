// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
    #include <spdlog/details/log_msg_buffer.h>
#endif

namespace spdlog {
namespace details {

SPDLOG_INLINE log_msg_buffer::log_msg_buffer(const log_msg &orig_msg)
    : log_msg{orig_msg} {
    buffer.append(logger_name.begin(), logger_name.end());
    buffer.append(payload.begin(), payload.end());
    update_string_views();
}

SPDLOG_INLINE log_msg_buffer::log_msg_buffer(const log_msg_buffer &other)
    : log_msg{other} {
    buffer.append(logger_name.begin(), logger_name.end());
    buffer.append(payload.begin(), payload.end());
    update_string_views();
}

SPDLOG_INLINE log_msg_buffer::log_msg_buffer(log_msg_buffer &&other) SPDLOG_NOEXCEPT
    : log_msg{other},
      buffer{std::move(other.buffer)} {
    update_string_views();
}

SPDLOG_INLINE log_msg_buffer &log_msg_buffer::operator=(const log_msg_buffer &other) {
    log_msg::operator=(other);
    buffer.clear();
    buffer.append(other.buffer.data(), other.buffer.data() + other.buffer.size());
    update_string_views();
    return *this;
}

SPDLOG_INLINE log_msg_buffer &log_msg_buffer::operator=(log_msg_buffer &&other) SPDLOG_NOEXCEPT {
    log_msg::operator=(other);
    buffer = std::move(other.buffer);
    update_string_views();
    return *this;
}

SPDLOG_INLINE void log_msg_buffer::update_string_views() {
    logger_name = string_view_t{buffer.data(), logger_name.size()};
    payload = string_view_t{buffer.data() + logger_name.size(), payload.size()};
}

}  // namespace details
}  // namespace spdlog
