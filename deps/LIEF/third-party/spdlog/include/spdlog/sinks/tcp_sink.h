// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/common.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#ifdef _WIN32
    #include <spdlog/details/tcp_client-windows.h>
#else
    #include <spdlog/details/tcp_client.h>
#endif

#include <chrono>
#include <functional>
#include <mutex>
#include <string>

#pragma once

// Simple tcp client sink
// Connects to remote address and send the formatted log.
// Will attempt to reconnect if connection drops.
// If more complicated behaviour is needed (i.e get responses), you can inherit it and override the
// sink_it_ method.

namespace spdlog {
namespace sinks {

struct tcp_sink_config {
    std::string server_host;
    int server_port;
    bool lazy_connect = false;  // if true connect on first log call instead of on construction

    tcp_sink_config(std::string host, int port)
        : server_host{std::move(host)},
          server_port{port} {}
};

template <typename Mutex>
class tcp_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    // connect to tcp host/port or throw if failed
    // host can be hostname or ip address

    explicit tcp_sink(tcp_sink_config sink_config)
        : config_{std::move(sink_config)} {
        if (!config_.lazy_connect) {
            this->client_.connect(config_.server_host, config_.server_port);
        }
    }

    ~tcp_sink() override = default;

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        if (!client_.is_connected()) {
            client_.connect(config_.server_host, config_.server_port);
        }
        client_.send(formatted.data(), formatted.size());
    }

    void flush_() override {}
    tcp_sink_config config_;
    details::tcp_client client_;
};

using tcp_sink_mt = tcp_sink<std::mutex>;
using tcp_sink_st = tcp_sink<spdlog::details::null_mutex>;

}  // namespace sinks
}  // namespace spdlog
