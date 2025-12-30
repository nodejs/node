/*
 * ngtcp2
 *
 * Copyright (c) 2025 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef SIM_H
#define SIM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <functional>
#include <chrono>
#include <queue>
#include <span>
#include <random>

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include "network.h"

namespace ngtcp2 {
inline constexpr size_t MAX_UDP_PAYLOAD_SIZE = 1500;

using Timestamp =
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;

ngtcp2_tstamp to_ngtcp2_tstamp(const Timestamp &ts);

Timestamp to_timestamp(ngtcp2_tstamp ts);

class Simulator;
class Endpoint;

struct Context {
  Simulator *sim;
  Timestamp ts;
  Endpoint *endpoint;
};

constexpr unsigned long long operator""_kbps(unsigned long long k) {
  return k * 1'000;
}

constexpr unsigned long long operator""_mbps(unsigned long long m) {
  return m * 1'000'000;
}

constexpr unsigned long long operator""_gbps(unsigned long long g) {
  return g * 1'000'000'000;
}

struct LinkConfig {
  // compute_expected_goodput computes the expected goodput with the
  // given |rtt| in bits per second.
  uint64_t compute_expected_goodput(Timestamp::duration rtt) const;

  // delay is the one-way link delay.
  Timestamp::duration delay;
  // rate is the bandwidth of this link measured in bits per second
  // (e.g., 10_mbps).
  uint64_t rate{};
  // limit is the maximum queue length of the outgoing packet measured
  // in bytes.
  uint64_t limit{};
  // loss is the probability of losing a packet.
  double loss{};
  // seed is a seed value for the random number generator.
  std::mt19937::result_type seed{};
};

struct EndpointConfig {
  bool server{};
  ngtcp2_callbacks callbacks{};
  ngtcp2_settings settings{};
  ngtcp2_transport_params params{};
  ngtcp2_addr local_addr{};
  void *user_data{};
  LinkConfig link;

  std::function<int(ngtcp2_conn *, const Context &)> on_write;
};

ngtcp2_callbacks default_client_callbacks();

ngtcp2_callbacks default_server_callbacks();

ngtcp2_settings default_client_settings();

ngtcp2_settings default_server_settings();

ngtcp2_transport_params default_client_transport_params();

ngtcp2_transport_params default_server_transport_params();

ngtcp2_addr default_client_addr();

ngtcp2_addr default_server_addr();

EndpointConfig default_client_endpoint_config();

EndpointConfig default_server_endpoint_config();

struct NetworkPath {
  NetworkPath invert();

  Address local{};
  Address remote{};
};

NetworkPath to_network_path(const ngtcp2_path *path);

ngtcp2_path to_ngtcp2_path(const NetworkPath &path);

enum EventType {
  EVENT_TYPE_TIMEOUT,
  EVENT_TYPE_PKT,
};

struct Event {
  Timestamp ts;
  EventType type;

  NetworkPath path;
  std::vector<uint8_t> pkt;
};

constexpr bool operator>(const Event &lhs, const Event &rhs) {
  return lhs.ts > rhs.ts;
}

struct TxPacket {
  Timestamp departure_ts;
  size_t size;
};

class Channel {
public:
  Channel() = default;
  Channel(const LinkConfig &config);
  Channel(const Channel &) = delete;
  Channel(Channel &&) noexcept;

  Channel &operator=(const Channel &) = delete;
  Channel &operator=(Channel &&) noexcept;

  void send_pkt(const NetworkPath &path, std::span<uint8_t> pkt);
  void schedule_timeout(Timestamp ts);
  void set_timestamp(Timestamp ts) { ts_ = ts; }
  Timestamp get_next_timestamp() const;
  Event get_next_event();
  void pop_tx_queue();

private:
  bool decide_pkt_lost();

  LinkConfig link_config_;
  std::mt19937 gen_;
  std::deque<TxPacket> tx_queue_;
  size_t tx_queue_size_{};
  Timestamp link_free_ts_;
  using EventQueue =
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>>;
  EventQueue queue_;
  Timestamp timeout_{Timestamp::max()};
  Timestamp ts_{};
};

class Endpoint {
public:
  Endpoint();
  explicit Endpoint(const EndpointConfig &config);
  Endpoint(Endpoint &&endpoint) noexcept;
  Endpoint(const Endpoint &) = delete;
  ~Endpoint();

  Endpoint &operator=(const Endpoint &) = delete;
  Endpoint &operator=(Endpoint &&) noexcept;

  int setup_client(const ngtcp2_addr *remote_addr);
  int setup_server(std::span<const uint8_t> original_dcid,
                   std::span<const uint8_t> client_scid, uint32_t version,
                   const ngtcp2_addr *remote_addr);
  ngtcp2_conn *get_conn() const { return conn_; }
  bool get_initialized() const { return initialized_; }
  const EndpointConfig &get_endpoint_config() const { return config_; }
  int on_read(const NetworkPath &path, std::span<const uint8_t> pkt,
              const Context &ctx);
  int on_write(const Context &ctx);
  int on_timeout(const Context &ctx);
  Channel &get_channel() { return channel_; }

private:
  EndpointConfig config_;
  WOLFSSL_CTX *ssl_ctx_{};
  WOLFSSL *ssl_{};
  ngtcp2_conn *conn_{};
  ngtcp2_crypto_conn_ref conn_ref_{};
  Channel channel_;
  bool initialized_{};
};

class Simulator {
public:
  Simulator(Endpoint client, Endpoint server);
  Simulator(const Simulator &) = delete;
  Simulator(Simulator &&) noexcept;

  Simulator &operator=(const Simulator &) = delete;
  Simulator &operator=(Simulator &&) noexcept;

  int run();
  void set_max_events(size_t n) { max_events_ = n; }

private:
  Endpoint &get_opposite_endpoint(const Endpoint &ep);
  std::optional<std::tuple<Event, Endpoint &>> get_next_event();
  int deliver_pkt(Endpoint &remote_ep, const NetworkPath &path,
                  std::span<const uint8_t> pkt, Timestamp ts);

  Endpoint client_;
  Endpoint server_;
  size_t max_events_{1'000'000};
};

class HandshakeApp {
public:
  void configure(EndpointConfig &config);

  bool get_handshake_confirmed() const { return handshake_confirmed_; }

private:
  void handshake_confirmed() { handshake_confirmed_ = true; }

  bool handshake_confirmed_{};
};

class UniStreamApp {
public:
  UniStreamApp(uint64_t max_bytes);

  void configure(EndpointConfig &config);
  uint64_t get_bytes_sent() const { return bytes_sent_; }
  Timestamp::duration get_transmit_duration() const {
    return end_ts_ - start_ts_;
  }
  bool is_all_bytes_sent() const { return bytes_sent_ == max_bytes_; }
  // compute_goodput computes goodput in bits per second.
  uint64_t compute_goodput() const;

private:
  void stream_close(ngtcp2_conn *conn, int64_t stream_id);
  int extend_max_local_streams_uni(ngtcp2_conn *conn);
  int on_write(ngtcp2_conn *conn, const Context &ctx);

  int64_t stream_id_{-1};
  uint64_t max_bytes_{};
  uint64_t bytes_sent_{};
  Timestamp start_ts_{Timestamp::max()};
  Timestamp end_ts_{Timestamp::max()};
};

} // namespace ngtcp2

#endif // SIM_H
