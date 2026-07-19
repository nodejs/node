// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include "spdlog/details/circular_q.h"
#include "spdlog/details/log_msg_buffer.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/sinks/base_sink.h"

#include <mutex>
#include <string>
#include <vector>

namespace spdlog {
namespace sinks {
/*
 * Ring buffer sink
 */
template <typename Mutex>
class ringbuffer_sink final : public base_sink<Mutex> {
public:
    explicit ringbuffer_sink(size_t n_items)
        : q_{n_items} {}

    std::vector<details::log_msg_buffer> last_raw(size_t lim = 0) {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        auto items_available = q_.size();
        auto n_items = lim > 0 ? (std::min)(lim, items_available) : items_available;
        std::vector<details::log_msg_buffer> ret;
        ret.reserve(n_items);
        for (size_t i = (items_available - n_items); i < items_available; i++) {
            ret.push_back(q_.at(i));
        }
        return ret;
    }

    std::vector<std::string> last_formatted(size_t lim = 0) {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        auto items_available = q_.size();
        auto n_items = lim > 0 ? (std::min)(lim, items_available) : items_available;
        std::vector<std::string> ret;
        ret.reserve(n_items);
        for (size_t i = (items_available - n_items); i < items_available; i++) {
            memory_buf_t formatted;
            base_sink<Mutex>::formatter_->format(q_.at(i), formatted);
            ret.push_back(SPDLOG_BUF_TO_STRING(formatted));
        }
        return ret;
    }

protected:
    void sink_it_(const details::log_msg &msg) override {
        q_.push_back(details::log_msg_buffer{msg});
    }
    void flush_() override {}

private:
    details::circular_q<details::log_msg_buffer> q_;
};

using ringbuffer_sink_mt = ringbuffer_sink<std::mutex>;
using ringbuffer_sink_st = ringbuffer_sink<details::null_mutex>;

}  // namespace sinks

}  // namespace spdlog
