// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

// Loggers registry of unique name->logger pointer
// An attempt to create a logger with an already existing name will result with spdlog_ex exception.
// If user requests a non existing logger, nullptr will be returned
// This class is thread safe

#include <spdlog/common.h>
#include <spdlog/details/periodic_worker.h>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace spdlog {
class logger;

namespace details {
class thread_pool;

class SPDLOG_API registry {
public:
    using log_levels = std::unordered_map<std::string, level::level_enum>;
    registry(const registry &) = delete;
    registry &operator=(const registry &) = delete;

    void register_logger(std::shared_ptr<logger> new_logger);
    void register_or_replace(std::shared_ptr<logger> new_logger);
    void initialize_logger(std::shared_ptr<logger> new_logger);
    std::shared_ptr<logger> get(const std::string &logger_name);
    std::shared_ptr<logger> default_logger();

    // Return raw ptr to the default logger.
    // To be used directly by the spdlog default api (e.g. spdlog::info)
    // This make the default API faster, but cannot be used concurrently with set_default_logger().
    // e.g do not call set_default_logger() from one thread while calling spdlog::info() from
    // another.
    logger *get_default_raw();

    // set default logger and add it to the registry if not registered already.
    // default logger is stored in default_logger_ (for faster retrieval) and in the loggers_ map.
    // Note: Make sure to unregister it when no longer needed or before calling again with a new
    // logger.
    void set_default_logger(std::shared_ptr<logger> new_default_logger);

    void set_tp(std::shared_ptr<thread_pool> tp);

    std::shared_ptr<thread_pool> get_tp();

    // Set global formatter. Each sink in each logger will get a clone of this object
    void set_formatter(std::unique_ptr<formatter> formatter);

    void enable_backtrace(size_t n_messages);

    void disable_backtrace();

    void set_level(level::level_enum log_level);

    void flush_on(level::level_enum log_level);

    template <typename Rep, typename Period>
    void flush_every(std::chrono::duration<Rep, Period> interval) {
        std::lock_guard<std::mutex> lock(flusher_mutex_);
        auto clbk = [this]() { this->flush_all(); };
        periodic_flusher_ = details::make_unique<periodic_worker>(clbk, interval);
    }

    std::unique_ptr<periodic_worker> &get_flusher() {
        std::lock_guard<std::mutex> lock(flusher_mutex_);
        return periodic_flusher_;
    }

    void set_error_handler(err_handler handler);

    void apply_all(const std::function<void(const std::shared_ptr<logger>)> &fun);

    void flush_all();

    void drop(const std::string &logger_name);

    void drop_all();

    // clean all resources and threads started by the registry
    void shutdown();

    std::recursive_mutex &tp_mutex();

    void set_automatic_registration(bool automatic_registration);

    // set levels for all existing/future loggers. global_level can be null if should not set.
    void set_levels(log_levels levels, level::level_enum *global_level);

    static registry &instance();

    void apply_logger_env_levels(std::shared_ptr<logger> new_logger);

private:
    registry();
    ~registry();

    void throw_if_exists_(const std::string &logger_name);
    void register_logger_(std::shared_ptr<logger> new_logger);
    void register_or_replace_(std::shared_ptr<logger> new_logger);
    bool set_level_from_cfg_(logger *logger);
    std::mutex logger_map_mutex_, flusher_mutex_;
    std::recursive_mutex tp_mutex_;
    std::unordered_map<std::string, std::shared_ptr<logger>> loggers_;
    log_levels log_levels_;
    std::unique_ptr<formatter> formatter_;
    spdlog::level::level_enum global_log_level_ = level::info;
    level::level_enum flush_level_ = level::off;
    err_handler err_handler_;
    std::shared_ptr<thread_pool> tp_;
    std::unique_ptr<periodic_worker> periodic_flusher_;
    std::shared_ptr<logger> default_logger_;
    bool automatic_registration_ = true;
    size_t backtrace_n_messages_ = 0;
};

}  // namespace details
}  // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
    #include "registry-inl.h"
#endif
