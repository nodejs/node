// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/spdlog.h>
#endif

#include <spdlog/common.h>
#include <spdlog/pattern_formatter.h>

namespace spdlog {

SPDLOG_INLINE void initialize_logger(std::shared_ptr<logger> logger) {
    details::registry::instance().initialize_logger(std::move(logger));
}

SPDLOG_INLINE std::shared_ptr<logger> get(const std::string &name) {
    return details::registry::instance().get(name);
}

SPDLOG_INLINE void set_formatter(std::unique_ptr<spdlog::formatter> formatter) {
    details::registry::instance().set_formatter(std::move(formatter));
}

SPDLOG_INLINE void set_pattern(std::string pattern, pattern_time_type time_type) {
    set_formatter(
        std::unique_ptr<spdlog::formatter>(new pattern_formatter(std::move(pattern), time_type)));
}

SPDLOG_INLINE void enable_backtrace(size_t n_messages) {
    details::registry::instance().enable_backtrace(n_messages);
}

SPDLOG_INLINE void disable_backtrace() { details::registry::instance().disable_backtrace(); }

SPDLOG_INLINE void dump_backtrace() { default_logger_raw()->dump_backtrace(); }

SPDLOG_INLINE level::level_enum get_level() { return default_logger_raw()->level(); }

SPDLOG_INLINE bool should_log(level::level_enum log_level) {
    return default_logger_raw()->should_log(log_level);
}

SPDLOG_INLINE void set_level(level::level_enum log_level) {
    details::registry::instance().set_level(log_level);
}

SPDLOG_INLINE void flush_on(level::level_enum log_level) {
    details::registry::instance().flush_on(log_level);
}

SPDLOG_INLINE void set_error_handler(void (*handler)(const std::string &msg)) {
    details::registry::instance().set_error_handler(handler);
}

SPDLOG_INLINE void register_logger(std::shared_ptr<logger> logger) {
    details::registry::instance().register_logger(std::move(logger));
}

SPDLOG_INLINE void register_or_replace(std::shared_ptr<logger> logger) {
    details::registry::instance().register_or_replace(std::move(logger));
}

SPDLOG_INLINE void apply_all(const std::function<void(std::shared_ptr<logger>)> &fun) {
    details::registry::instance().apply_all(fun);
}

SPDLOG_INLINE void drop(const std::string &name) { details::registry::instance().drop(name); }

SPDLOG_INLINE void drop_all() { details::registry::instance().drop_all(); }

SPDLOG_INLINE void shutdown() { details::registry::instance().shutdown(); }

SPDLOG_INLINE void set_automatic_registration(bool automatic_registration) {
    details::registry::instance().set_automatic_registration(automatic_registration);
}

SPDLOG_INLINE std::shared_ptr<spdlog::logger> default_logger() {
    return details::registry::instance().default_logger();
}

SPDLOG_INLINE spdlog::logger *default_logger_raw() {
    return details::registry::instance().get_default_raw();
}

SPDLOG_INLINE void set_default_logger(std::shared_ptr<spdlog::logger> default_logger) {
    details::registry::instance().set_default_logger(std::move(default_logger));
}

SPDLOG_INLINE void apply_logger_env_levels(std::shared_ptr<logger> logger) {
    details::registry::instance().apply_logger_env_levels(std::move(logger));
}

}  // namespace spdlog
