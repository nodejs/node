// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <ostream>

namespace spdlog {
namespace sinks {
template <typename Mutex>
class ostream_sink final : public base_sink<Mutex> {
public:
    explicit ostream_sink(std::ostream &os, bool force_flush = false)
        : ostream_(os),
          force_flush_(force_flush) {}
    ostream_sink(const ostream_sink &) = delete;
    ostream_sink &operator=(const ostream_sink &) = delete;

protected:
    void sink_it_(const details::log_msg &msg) override {
        memory_buf_t formatted;
        base_sink<Mutex>::formatter_->format(msg, formatted);
        ostream_.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
        if (force_flush_) {
            ostream_.flush();
        }
    }

    void flush_() override { ostream_.flush(); }

    std::ostream &ostream_;
    bool force_flush_;
};

using ostream_sink_mt = ostream_sink<std::mutex>;
using ostream_sink_st = ostream_sink<details::null_mutex>;

}  // namespace sinks
}  // namespace spdlog
