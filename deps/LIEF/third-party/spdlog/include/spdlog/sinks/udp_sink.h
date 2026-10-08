// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/common.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#ifdef _WIN32
#include <spdlog/details/udp_client-windows.h>
#else
#include <spdlog/details/udp_client.h>
#endif

#include <chrono>
#include <functional>
#include <mutex>
#include <string>

// Simple udp client sink
// Sends formatted log via udp

namespace spdlog {
namespace sinks {

struct udp_sink_config {
    std::string server_host;
    uint16_t server_port;

    udp_sink_config(std::string host, uint16_t port)
        : server_host{std::move(host)},
          server_port{port} {}
};

template <typename Mutex>
class udp_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    // host can be hostname or ip address
    explicit udp_sink(udp_sink_config sink_config)
        : client_{sink_config.server_host, sink_config.server_port} {}

    ~udp_sink() override = default;

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        client_.send(formatted.data(), formatted.size());
    }

    void flush_() override {}
    details::udp_client client_;
};

using udp_sink_mt = udp_sink<std::mutex>;
using udp_sink_st = udp_sink<spdlog::details::null_mutex>;

}  // namespace sinks

//
// factory functions
//
template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> udp_logger_mt(const std::string &logger_name,
                                             sinks::udp_sink_config skin_config) {
    return Factory::template create<sinks::udp_sink_mt>(logger_name, skin_config);
}

}  // namespace spdlog
