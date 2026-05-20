// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

//
// Async logging using global thread pool
// All loggers created here share same global thread pool.
// Each log message is pushed to a queue along with a shared pointer to the
// logger.
// If a logger deleted while having pending messages in the queue, it's actual
// destruction will defer
// until all its messages are processed by the thread pool.
// This is because each message in the queue holds a shared_ptr to the
// originating logger.

#include <spdlog/async_logger.h>
#include <spdlog/details/registry.h>
#include <spdlog/details/thread_pool.h>

#include <functional>
#include <memory>
#include <mutex>

namespace spdlog {

namespace details {
static const size_t default_async_q_size = 8192;
}

// async logger factory - creates async loggers backed with thread pool.
// if a global thread pool doesn't already exist, create it with default queue
// size of 8192 items and single thread.
template <async_overflow_policy OverflowPolicy = async_overflow_policy::block>
struct async_factory_impl {
    template <typename Sink, typename... SinkArgs>
    static std::shared_ptr<async_logger> create(std::string logger_name, SinkArgs &&...args) {
        auto &registry_inst = details::registry::instance();

        // create global thread pool if not already exists..

        auto &mutex = registry_inst.tp_mutex();
        std::lock_guard<std::recursive_mutex> tp_lock(mutex);
        auto tp = registry_inst.get_tp();
        if (tp == nullptr) {
            tp = std::make_shared<details::thread_pool>(details::default_async_q_size, 1U);
            registry_inst.set_tp(tp);
        }

        auto sink = std::make_shared<Sink>(std::forward<SinkArgs>(args)...);
        auto new_logger = std::make_shared<async_logger>(std::move(logger_name), std::move(sink),
                                                         std::move(tp), OverflowPolicy);
        registry_inst.initialize_logger(new_logger);
        return new_logger;
    }
};

using async_factory = async_factory_impl<async_overflow_policy::block>;
using async_factory_nonblock = async_factory_impl<async_overflow_policy::overrun_oldest>;

template <typename Sink, typename... SinkArgs>
inline std::shared_ptr<spdlog::logger> create_async(std::string logger_name,
                                                    SinkArgs &&...sink_args) {
    return async_factory::create<Sink>(std::move(logger_name),
                                       std::forward<SinkArgs>(sink_args)...);
}

template <typename Sink, typename... SinkArgs>
inline std::shared_ptr<spdlog::logger> create_async_nb(std::string logger_name,
                                                       SinkArgs &&...sink_args) {
    return async_factory_nonblock::create<Sink>(std::move(logger_name),
                                                std::forward<SinkArgs>(sink_args)...);
}

// set global thread pool.
inline void init_thread_pool(size_t q_size,
                             size_t thread_count,
                             std::function<void()> on_thread_start,
                             std::function<void()> on_thread_stop) {
    auto tp = std::make_shared<details::thread_pool>(q_size, thread_count, on_thread_start,
                                                     on_thread_stop);
    details::registry::instance().set_tp(std::move(tp));
}

inline void init_thread_pool(size_t q_size,
                             size_t thread_count,
                             std::function<void()> on_thread_start) {
    init_thread_pool(q_size, thread_count, on_thread_start, [] {});
}

inline void init_thread_pool(size_t q_size, size_t thread_count) {
    init_thread_pool(q_size, thread_count, [] {}, [] {});
}

// get the global thread pool.
inline std::shared_ptr<spdlog::details::thread_pool> thread_pool() {
    return details::registry::instance().get_tp();
}
}  // namespace spdlog
