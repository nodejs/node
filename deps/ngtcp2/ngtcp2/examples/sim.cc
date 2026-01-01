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
#include "sim.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <cmath>
#include <utility>
#include <string_view>
#include <iostream>

#include "ngtcp2/ngtcp2_crypto_wolfssl.h"

#include "util.h"
#include "shared.h"
#include "debug.h"

using namespace std::literals;

namespace ngtcp2 {

namespace {
constexpr auto ALPN_LIST = "ngtcp2-sim"sv;
constexpr size_t CIDLEN = 10;
constexpr uint8_t SERVER_SECRET[] = "server_secret";

int generate_secure_random(std::span<uint8_t> data) {
  if (wolfSSL_RAND_bytes(data.data(), static_cast<int>(data.size())) != 1) {
    return -1;
  }

  return 0;
}

void rand_bytes(uint8_t *dest, size_t destlen,
                const ngtcp2_rand_ctx *rand_ctx) {
  auto rv = generate_secure_random({dest, destlen});
  (void)rv;
  assert(0 == rv);
}

int get_new_connection_id(ngtcp2_conn *conn, ngtcp2_cid *cid, uint8_t *token,
                          size_t cidlen, void *user_data) {
  if (generate_secure_random({cid->data, cidlen}) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  cid->datalen = cidlen;
  if (ngtcp2_crypto_generate_stateless_reset_token(
        token, SERVER_SECRET, sizeof(SERVER_SECRET) - 1, cid) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

ngtcp2_tstamp to_ngtcp2_tstamp(const Timestamp &ts) {
  return static_cast<ngtcp2_tstamp>(ts.time_since_epoch().count());
}

Timestamp to_timestamp(ngtcp2_tstamp ts) {
  return Timestamp{Timestamp::duration{ts}};
}

uint64_t LinkConfig::compute_expected_goodput(Timestamp::duration rtt) const {
  // Assume 80% usage ratio.
  uint64_t g = rate * 8 / 10;

  if (loss < 1e-9) {
    return g;
  }

  constexpr double margin = 0.95;

  return std::min(g,
                  static_cast<uint64_t>(MAX_UDP_PAYLOAD_SIZE * NGTCP2_SECONDS /
                                        static_cast<double>(rtt.count()) /
                                        sqrt(loss) * 8 * margin));
}

namespace {
int recv_stream_data(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id,
                     uint64_t offset, const uint8_t *data, size_t datalen,
                     void *user_data, void *stream_user_data) {
  ngtcp2_conn_extend_max_stream_offset(conn, stream_id, datalen);
  ngtcp2_conn_extend_max_offset(conn, datalen);

  return 0;
}
} // namespace

ngtcp2_callbacks default_client_callbacks() {
  return ngtcp2_callbacks{
    .client_initial = ngtcp2_crypto_client_initial_cb,
    .recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb,
    .encrypt = ngtcp2_crypto_encrypt_cb,
    .decrypt = ngtcp2_crypto_decrypt_cb,
    .hp_mask = ngtcp2_crypto_hp_mask_cb,
    .recv_stream_data = recv_stream_data,
    .recv_retry = ngtcp2_crypto_recv_retry_cb,
    .rand = rand_bytes,
    .get_new_connection_id = get_new_connection_id,
    .update_key = ngtcp2_crypto_update_key_cb,
    .delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    .delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    .get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb,
    .version_negotiation = ngtcp2_crypto_version_negotiation_cb,
  };
}

ngtcp2_callbacks default_server_callbacks() {
  return ngtcp2_callbacks{
    .recv_client_initial = ngtcp2_crypto_recv_client_initial_cb,
    .recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb,
    .encrypt = ngtcp2_crypto_encrypt_cb,
    .decrypt = ngtcp2_crypto_decrypt_cb,
    .hp_mask = ngtcp2_crypto_hp_mask_cb,
    .recv_stream_data = recv_stream_data,
    .rand = rand_bytes,
    .get_new_connection_id = get_new_connection_id,
    .update_key = ngtcp2_crypto_update_key_cb,
    .delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    .delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    .get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb,
    .version_negotiation = ngtcp2_crypto_version_negotiation_cb,
  };
}

ngtcp2_settings default_client_settings() {
  ngtcp2_settings settings;
  ngtcp2_settings_default(&settings);

  settings.log_printf = debug::log_printf;

  return settings;
}

ngtcp2_settings default_server_settings() {
  ngtcp2_settings settings;
  ngtcp2_settings_default(&settings);

  settings.log_printf = debug::log_printf;

  return settings;
}

ngtcp2_transport_params default_client_transport_params() {
  ngtcp2_transport_params params;
  ngtcp2_transport_params_default(&params);

  return params;
}

ngtcp2_transport_params default_server_transport_params() {
  ngtcp2_transport_params params;
  ngtcp2_transport_params_default(&params);

  return params;
}

Sockaddr getaddrinfo(const char *host, const char *svc) {
  auto hints = addrinfo{
    .ai_flags = AI_NUMERICHOST | AI_NUMERICSERV,
    .ai_family = AF_UNSPEC,
  };
  addrinfo *rp;

  auto rv = getaddrinfo(host, svc, &hints, &rp);
  (void)rv;
  assert(0 == rv);

  Sockaddr skaddr;
  sockaddr_set(skaddr, rp->ai_addr);

  freeaddrinfo(rp);

  return skaddr;
}

ngtcp2_addr default_client_addr() {
  static auto skaddr = getaddrinfo("10.0.1.1", "12345");

  return ngtcp2_addr{
    .addr = as_sockaddr(skaddr),
    .addrlen = sockaddr_size(skaddr),
  };
}

ngtcp2_addr default_server_addr() {
  static auto skaddr = getaddrinfo("10.0.2.1", "443");

  return ngtcp2_addr{
    .addr = as_sockaddr(skaddr),
    .addrlen = sockaddr_size(skaddr),
  };
}

EndpointConfig default_client_endpoint_config() {
  return EndpointConfig{
    .callbacks = default_client_callbacks(),
    .settings = default_client_settings(),
    .params = default_client_transport_params(),
    .local_addr = default_client_addr(),
  };
}

EndpointConfig default_server_endpoint_config() {
  return EndpointConfig{
    .server = true,
    .callbacks = default_server_callbacks(),
    .settings = default_server_settings(),
    .params = default_server_transport_params(),
    .local_addr = default_server_addr(),
  };
}

namespace {
ngtcp2_conn *get_conn(ngtcp2_crypto_conn_ref *conn_ref) {
  auto ep = static_cast<Endpoint *>(conn_ref->user_data);
  return ep->get_conn();
}
} // namespace

Endpoint::Endpoint()
  : conn_ref_{ngtcp2::get_conn, this}, channel_{config_.link} {}

Endpoint::Endpoint(const EndpointConfig &config)
  : config_{config},
    conn_ref_{ngtcp2::get_conn, this},
    channel_{config_.link} {}

Endpoint::Endpoint(Endpoint &&other) noexcept
  : config_{std::exchange(other.config_, {})},
    ssl_ctx_{std::exchange(other.ssl_ctx_, nullptr)},
    ssl_{std::exchange(other.ssl_, nullptr)},
    conn_{std::exchange(other.conn_, nullptr)},
    conn_ref_{ngtcp2::get_conn, this},
    channel_{std::exchange(other.channel_, {})},
    initialized_{std::exchange(other.initialized_, false)} {}

Endpoint::~Endpoint() {
  ngtcp2_conn_del(conn_);

  if (ssl_) {
    wolfSSL_free(ssl_);
  }

  if (ssl_ctx_) {
    wolfSSL_CTX_free(ssl_ctx_);
  }
}

Endpoint &Endpoint::operator=(Endpoint &&other) noexcept {
  ngtcp2_conn_del(conn_);

  if (ssl_) {
    wolfSSL_free(ssl_);
  }

  if (ssl_ctx_) {
    wolfSSL_CTX_free(ssl_ctx_);
  }

  config_ = std::exchange(other.config_, {});
  ssl_ctx_ = std::exchange(other.ssl_ctx_, nullptr);
  ssl_ = std::exchange(other.ssl_, nullptr);
  conn_ = std::exchange(other.conn_, nullptr);
  conn_ref_ = {ngtcp2::get_conn, this};
  channel_ = std::exchange(other.channel_, {});
  initialized_ = std::exchange(other.initialized_, false);

  if (ssl_) {
    wolfSSL_set_app_data(ssl_, &conn_ref_);
  }

  return *this;
}

namespace {
constexpr auto tls_key = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgwEvkGGgXAcRaG7Z8
gA7C6+W2RsW9gcjV9e5ybr0ikaahRANCAASCo35bDi+Q/q/CzHI1e5QaBrbqbFhW
G20QbVAeMK8l0oC8OGD3PSpZK1HXwALwzhMuwhxDos3ANb5naa5y17fQ
-----END PRIVATE KEY-----
)"sv;

constexpr auto tls_crt = R"(-----BEGIN CERTIFICATE-----
MIICBzCCAa2gAwIBAgIUd2l6Pce3S0QH3dQC0Q/CjHbmggowCgYIKoZIzj0EAwIw
WTELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGElu
dGVybmV0IFdpZGdpdHMgUHR5IEx0ZDESMBAGA1UEAwwJbG9jYWxob3N0MB4XDTI1
MTExNDExNTcwMFoXDTI1MTIxNDExNTcwMFowWTELMAkGA1UEBhMCQVUxEzARBgNV
BAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMgUHR5IEx0
ZDESMBAGA1UEAwwJbG9jYWxob3N0MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE
gqN+Ww4vkP6vwsxyNXuUGga26mxYVhttEG1QHjCvJdKAvDhg9z0qWStR18AC8M4T
LsIcQ6LNwDW+Z2mucte30KNTMFEwHQYDVR0OBBYEFFVgXLoLwzpf6+twP5z8Ujr2
5mxnMB8GA1UdIwQYMBaAFFVgXLoLwzpf6+twP5z8Ujr25mxnMA8GA1UdEwEB/wQF
MAMBAf8wCgYIKoZIzj0EAwIDSAAwRQIhAO4tnDNRAcooz62vf2m7vTyDqFCjcaIv
SJ9Gq0lvEXEcAiBwWBNUASBqLaje3hmtgwxcF7EIqqiGo5j8f9Ufgu6SRg==
-----END CERTIFICATE-----
)"sv;
} // namespace

int Endpoint::setup_server(std::span<const uint8_t> original_dcid,
                           std::span<const uint8_t> client_scid,
                           uint32_t version, const ngtcp2_addr *remote_addr) {
  int rv;

  ngtcp2_cid scid{
    .datalen = CIDLEN,
  };

  if (generate_secure_random({scid.data, scid.datalen}) != 0) {
    return -1;
  }

  ngtcp2_cid dcid;
  ngtcp2_cid_init(&dcid, client_scid.data(), client_scid.size());

  auto params = config_.params;
  ngtcp2_cid_init(&params.original_dcid, original_dcid.data(),
                  original_dcid.size());
  params.original_dcid_present = 1;

  if (ngtcp2_crypto_generate_stateless_reset_token(
        params.stateless_reset_token, SERVER_SECRET, sizeof(SERVER_SECRET) - 1,
        &scid)) {
    return -1;
  }

  auto path = ngtcp2_path{
    .local = config_.local_addr,
    .remote = *remote_addr,
  };

  rv = ngtcp2_conn_server_new(&conn_, &dcid, &scid, &path, version,
                              &config_.callbacks, &config_.settings, &params,
                              nullptr, config_.user_data);
  if (rv != 0) {
    return -1;
  }

  ssl_ctx_ = wolfSSL_CTX_new(wolfTLSv1_3_server_method());
  if (!ssl_ctx_) {
    return -1;
  }

  if (ngtcp2_crypto_wolfssl_configure_server_context(ssl_ctx_) != 0) {
    return -1;
  }

  if (wolfSSL_CTX_use_certificate_buffer(
        ssl_ctx_, reinterpret_cast<const uint8_t *>(tls_crt.data()),
        static_cast<long>(tls_crt.size()), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
    return -1;
  }

  if (wolfSSL_CTX_use_PrivateKey_buffer(
        ssl_ctx_, reinterpret_cast<const uint8_t *>(tls_key.data()),
        static_cast<long>(tls_key.size()), SSL_FILETYPE_PEM) != SSL_SUCCESS) {
    return -1;
  }

  ssl_ = wolfSSL_new(ssl_ctx_);
  if (!ssl_) {
    return -1;
  }

  if (wolfSSL_UseALPN(ssl_, const_cast<char *>(ALPN_LIST.data()),
                      ALPN_LIST.size(),
                      WOLFSSL_ALPN_FAILED_ON_MISMATCH) != WOLFSSL_SUCCESS) {
    return -1;
  }

  wolfSSL_set_app_data(ssl_, &conn_ref_);
  wolfSSL_set_accept_state(ssl_);
  wolfSSL_set_quic_transport_version(ssl_, 0x39);

  ngtcp2_conn_set_tls_native_handle(conn_, ssl_);

  initialized_ = true;

  return 0;
}

int Endpoint::setup_client(const ngtcp2_addr *remote_addr) {
  int rv;

  ngtcp2_cid dcid{
    .datalen = CIDLEN,
  };
  ngtcp2_cid scid{
    .datalen = CIDLEN,
  };

  if (generate_secure_random({dcid.data, dcid.datalen}) != 0 ||
      generate_secure_random({scid.data, scid.datalen}) != 0) {
    assert(0);
    return -1;
  }

  auto path = ngtcp2_path{
    .local = config_.local_addr,
    .remote = *remote_addr,
  };

  rv = ngtcp2_conn_client_new(&conn_, &dcid, &scid, &path, NGTCP2_PROTO_VER_V1,
                              &config_.callbacks, &config_.settings,
                              &config_.params, nullptr, config_.user_data);
  if (rv != 0) {
    return -1;
  }

  ssl_ctx_ = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
  if (!ssl_ctx_) {
    return -1;
  }

  if (ngtcp2_crypto_wolfssl_configure_client_context(ssl_ctx_) != 0) {
    return -1;
  }

  ssl_ = wolfSSL_new(ssl_ctx_);
  if (!ssl_) {
    return -1;
  }

  if (wolfSSL_UseALPN(ssl_, const_cast<char *>(ALPN_LIST.data()),
                      ALPN_LIST.size(),
                      WOLFSSL_ALPN_FAILED_ON_MISMATCH) != WOLFSSL_SUCCESS) {
    return -1;
  }

  wolfSSL_set_app_data(ssl_, &conn_ref_);
  wolfSSL_set_connect_state(ssl_);
  wolfSSL_set_quic_transport_version(ssl_, 0x39);

  ngtcp2_conn_set_tls_native_handle(conn_, ssl_);

  initialized_ = true;

  return 0;
}

int Endpoint::on_read(const NetworkPath &path, std::span<const uint8_t> pkt,
                      const Context &ctx) {
  auto ts = to_ngtcp2_tstamp(ctx.ts);
  auto cpath = to_ngtcp2_path(path);

  auto rv =
    ngtcp2_conn_read_pkt(conn_, &cpath, nullptr, pkt.data(), pkt.size(), ts);
  if (rv != 0) {
    std::cerr << "ngtcp2_conn_read_pkt: " << ngtcp2_strerror(rv) << std::endl;
    return -1;
  }

  ctx.endpoint->get_channel().schedule_timeout(ctx.ts);

  return 0;
}

int Endpoint::on_write(const Context &ctx) {
  if (config_.on_write(conn_, ctx) != 0) {
    return -1;
  }

  auto next_expiry_ts = ngtcp2_conn_get_expiry(conn_);
  if (next_expiry_ts == UINT64_MAX) {
    return 0;
  }

  ctx.endpoint->get_channel().schedule_timeout(to_timestamp(next_expiry_ts));

  return 0;
}

int Endpoint::on_timeout(const Context &ctx) {
  auto rv = ngtcp2_conn_handle_expiry(conn_, to_ngtcp2_tstamp(ctx.ts));
  if (rv != 0) {
    std::cerr << "ngtcp2_conn_handle_expiry: " << ngtcp2_strerror(rv)
              << std::endl;
    return -1;
  }

  return on_write(ctx);
}

NetworkPath to_network_path(const ngtcp2_path *path) {
  NetworkPath res;

  res.local.set(path->local.addr);
  res.remote.set(path->remote.addr);

  return res;
}

ngtcp2_path to_ngtcp2_path(const NetworkPath &path) {
  return {
    .local = as_ngtcp2_addr(path.local),
    .remote = as_ngtcp2_addr(path.remote),
  };
}

NetworkPath NetworkPath::invert() {
  auto path = *this;

  std::swap(path.local, path.remote);

  return path;
}

Channel::Channel(const LinkConfig &config)
  : link_config_{config}, gen_{link_config_.seed} {}

Channel::Channel(Channel &&other) noexcept
  : link_config_{std::exchange(other.link_config_, {})},
    gen_{std::exchange(other.gen_, {})},
    tx_queue_{std::exchange(other.tx_queue_, {})},
    tx_queue_size_{std::exchange(other.tx_queue_size_, 0)},
    link_free_ts_{std::exchange(other.link_free_ts_, {})},
    queue_{std::exchange(other.queue_, {})},
    timeout_{std::exchange(other.timeout_, {})},
    ts_{std::exchange(other.ts_, {})} {}

Channel &Channel::operator=(Channel &&other) noexcept {
  link_config_ = std::exchange(other.link_config_, {});
  gen_ = std::exchange(other.gen_, {});
  tx_queue_ = std::exchange(other.tx_queue_, {});
  tx_queue_size_ = std::exchange(other.tx_queue_size_, 0);
  link_free_ts_ = std::exchange(other.link_free_ts_, {});
  queue_ = std::exchange(other.queue_, {});
  timeout_ = std::exchange(other.timeout_, {});
  ts_ = std::exchange(other.ts_, {});

  return *this;
}

void Channel::send_pkt(const NetworkPath &path, std::span<uint8_t> pkt) {
  auto rate = link_config_.rate / 8;

  if (rate == 0) {
    queue_.emplace(Event{
      .ts = ts_ + link_config_.delay,
      .type = EVENT_TYPE_PKT,
      .path = path,
      .pkt = std::vector(std::ranges::begin(pkt), std::ranges::end(pkt)),
    });

    return;
  }

  if (link_config_.limit && tx_queue_size_ + pkt.size() > link_config_.limit) {
    return;
  }

  auto departure_ts = std::max(ts_, link_free_ts_) +
                      Timestamp::duration{pkt.size() * NGTCP2_SECONDS / rate};

  if (!decide_pkt_lost()) {
    queue_.emplace(Event{
      .ts = departure_ts + link_config_.delay,
      .type = EVENT_TYPE_PKT,
      .path = path,
      .pkt = std::vector(std::ranges::begin(pkt), std::ranges::end(pkt)),
    });
  }

  tx_queue_.emplace_back(TxPacket{
    .departure_ts = departure_ts,
    .size = pkt.size(),
  });

  tx_queue_size_ += pkt.size();
  link_free_ts_ = departure_ts;
}

bool Channel::decide_pkt_lost() {
  return std::uniform_real_distribution<>(0, 1.0)(gen_) < link_config_.loss;
}

void Channel::pop_tx_queue() {
  size_t n = 0;

  auto it = std::ranges::find_if(tx_queue_, [&n, this](const auto &pkt) {
    if (pkt.departure_ts > ts_) {
      return true;
    }

    n += pkt.size;

    return false;
  });

  assert(tx_queue_size_ >= n);

  tx_queue_size_ -= n;
  tx_queue_.erase(std::ranges::begin(tx_queue_), it);
}

void Channel::schedule_timeout(Timestamp ts) {
  timeout_ = std::min(timeout_, ts);
}

Timestamp Channel::get_next_timestamp() const {
  if (queue_.empty()) {
    return timeout_;
  }

  auto &top = queue_.top();

  return std::min(timeout_, top.ts);
}

Event Channel::get_next_event() {
  if (!queue_.empty() && queue_.top().ts <= timeout_) {
    auto &top = const_cast<Event &>(queue_.top());

    auto ev = Event{
      .ts = top.ts,
      .type = top.type,
      .path = top.path,
      .pkt = std::move(top.pkt),
    };

    queue_.pop();

    return ev;
  }

  return Event{
    .ts = std::exchange(timeout_, Timestamp::max()),
    .type = EVENT_TYPE_TIMEOUT,
  };
}

Simulator::Simulator(Endpoint client, Endpoint server)
  : client_{std::move(client)}, server_{std::move(server)} {}

Simulator::Simulator(Simulator &&other) noexcept
  : client_{std::exchange(client_, {})},
    server_{std::exchange(server_, {})},
    max_events_{std::exchange(other.max_events_, 0)} {}

Simulator &Simulator::operator=(Simulator &&other) noexcept {
  client_ = std::exchange(other.client_, {});
  server_ = std::exchange(other.server_, {});
  max_events_ = std::exchange(other.max_events_, 0);

  return *this;
}

std::optional<std::tuple<Event, Endpoint &>> Simulator::get_next_event() {
  auto &client_chan = client_.get_channel();
  auto &server_chan = server_.get_channel();

  auto client_next_ts = client_chan.get_next_timestamp();
  auto server_next_ts = server_chan.get_next_timestamp();

  if (client_next_ts == Timestamp::max() &&
      server_next_ts == Timestamp::max()) {
    return {};
  }

  if (client_next_ts <= server_next_ts) {
    auto ev = client_chan.get_next_event();
    return {{std::move(ev), client_}};
  }

  auto ev = server_chan.get_next_event();

  return {{std::move(ev), server_}};
}

int Simulator::run() {
  if (client_.get_initialized() ||
      client_.setup_client(&server_.get_endpoint_config().local_addr) != 0) {
    return -1;
  }

  auto ts = Timestamp{};
  auto &client_chan = client_.get_channel();
  auto &server_chan = server_.get_channel();

  client_chan.schedule_timeout(ts);

  size_t k = 0;

  for (; k < max_events_; ++k) {
    auto maybe_event = get_next_event();
    if (!maybe_event) {
      break;
    }

    auto &[event, ep] = *maybe_event;

    assert(ts <= event.ts);

    ts = event.ts;

    client_chan.set_timestamp(ts);
    server_chan.set_timestamp(ts);

    client_chan.pop_tx_queue();
    server_chan.pop_tx_queue();

    switch (event.type) {
    case EVENT_TYPE_TIMEOUT: {
      auto ctx = Context{
        .sim = this,
        .ts = ts,
        .endpoint = &ep,
      };

      if (ep.on_timeout(ctx) != 0) {
        return -1;
      }

      break;
    }
    case EVENT_TYPE_PKT:
      if (deliver_pkt(ep, event.path.invert(), event.pkt, ts) != 0) {
        return -1;
      }

      break;
    }
  }

  if (k == max_events_) {
    return -1;
  }

  return 0;
}

Endpoint &Simulator::get_opposite_endpoint(const Endpoint &ep) {
  return &ep == &client_ ? server_ : client_;
}

int Simulator::deliver_pkt(Endpoint &remote_ep, const NetworkPath &path,
                           std::span<const uint8_t> pkt, Timestamp ts) {
  auto &local_ep = get_opposite_endpoint(remote_ep);

  if (!local_ep.get_initialized() && local_ep.get_endpoint_config().server) {
    ngtcp2_version_cid vcid;

    auto rv =
      ngtcp2_pkt_decode_version_cid(&vcid, pkt.data(), pkt.size(), CIDLEN);
    if (rv != 0) {
      return 0;
    }

    ngtcp2_pkt_hd hd;

    if (ngtcp2_accept(&hd, pkt.data(), pkt.size()) != 0) {
      return 0;
    }

    if (local_ep.setup_server(
          {vcid.dcid, vcid.dcidlen}, {vcid.scid, vcid.scidlen}, vcid.version,
          &remote_ep.get_endpoint_config().local_addr) != 0) {
      return -1;
    }
  }

  auto ctx = Context{
    .sim = this,
    .ts = ts,
    .endpoint = &local_ep,
  };

  return local_ep.on_read(path, pkt, ctx);
}

void HandshakeApp::configure(EndpointConfig &config) {
  auto handshake_confirmed = [](ngtcp2_conn *conn, void *user_data) {
    auto app = static_cast<HandshakeApp *>(user_data);

    app->handshake_confirmed();

    return 0;
  };

  if (config.server) {
    config.callbacks.handshake_completed = handshake_confirmed;
  } else {
    config.callbacks.handshake_confirmed = handshake_confirmed;
  }

  config.on_write = [](ngtcp2_conn *conn, const Context &ctx) {
    std::array<uint8_t, MAX_UDP_PAYLOAD_SIZE> buf;

    auto ts = to_ngtcp2_tstamp(ctx.ts);

    ngtcp2_path_storage ps;
    ngtcp2_path_storage_zero(&ps);

    auto nwrite = ngtcp2_conn_write_pkt(conn, &ps.path, nullptr, buf.data(),
                                        buf.size(), ts);
    if (nwrite < 0) {
      std::cerr << "ngtcp2_conn_write_pkt: "
                << ngtcp2_strerror(static_cast<int>(nwrite)) << std::endl;
      return -1;
    }

    if (nwrite == 0) {
      return 0;
    }

    ngtcp2_conn_update_pkt_tx_time(conn, ts);

    ctx.endpoint->get_channel().send_pkt(
      to_network_path(&ps.path), {buf.data(), static_cast<size_t>(nwrite)});

    return 0;
  };

  config.user_data = this;
}

UniStreamApp::UniStreamApp(uint64_t max_bytes) : max_bytes_{max_bytes} {}

namespace {
std::array<uint8_t, 4096> nulldata;
} // namespace

void UniStreamApp::configure(EndpointConfig &config) {
  config.callbacks.stream_close = [](ngtcp2_conn *conn, uint32_t flags,
                                     int64_t stream_id, uint64_t app_error_code,
                                     void *user_data, void *stream_user_data) {
    auto app = static_cast<UniStreamApp *>(user_data);

    app->stream_close(conn, stream_id);

    return 0;
  };

  config.callbacks.extend_max_local_streams_uni =
    [](ngtcp2_conn *conn, uint64_t max_streams, void *user_data) {
      auto app = static_cast<UniStreamApp *>(user_data);

      return app->extend_max_local_streams_uni(conn);
    };

  config.on_write = [this](ngtcp2_conn *conn, const Context &ctx) {
    return on_write(conn, ctx);
  };

  config.user_data = this;
}

uint64_t UniStreamApp::compute_goodput() const {
  auto d = get_transmit_duration();
  if (d == Timestamp::duration::zero()) {
    return 0;
  }

  return static_cast<uint64_t>(static_cast<double>(bytes_sent_) *
                               NGTCP2_SECONDS / static_cast<double>(d.count()) *
                               8);
}

void UniStreamApp::stream_close(ngtcp2_conn *conn, int64_t stream_id) {
  if (stream_id_ != stream_id) {
    return;
  }

  if (is_all_bytes_sent()) {
    end_ts_ = to_timestamp(ngtcp2_conn_get_timestamp(conn));
  }
}

int UniStreamApp::extend_max_local_streams_uni(ngtcp2_conn *conn) {
  if (stream_id_ != -1) {
    return 0;
  }

  int64_t stream_id;

  auto rv = ngtcp2_conn_open_uni_stream(conn, &stream_id, nullptr);
  if (rv != 0) {
    std::cerr << "ngtcp2_conn_open_uni_stream: " << ngtcp2_strerror(rv)
              << std::endl;
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  stream_id_ = stream_id;
  start_ts_ = to_timestamp(ngtcp2_conn_get_timestamp(conn));

  return 0;
}

int UniStreamApp::on_write(ngtcp2_conn *conn, const Context &ctx) {
  std::array<uint8_t, MAX_UDP_PAYLOAD_SIZE> buf;

  int64_t stream_id;
  ngtcp2_vec vec;
  size_t veccnt;
  uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_NONE;

  if (stream_id_ != -1 && max_bytes_ > bytes_sent_) {
    stream_id = stream_id_;
    vec.base = nulldata.data();
    vec.len = static_cast<size_t>(
      std::min(static_cast<uint64_t>(buf.size()), max_bytes_ - bytes_sent_));
    veccnt = 1;

    if (bytes_sent_ + vec.len == max_bytes_) {
      flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
    }
  } else {
    stream_id = -1;
    veccnt = 0;
  }

  auto ts = to_ngtcp2_tstamp(ctx.ts);

  ngtcp2_path_storage ps;
  ngtcp2_path_storage_zero(&ps);

  ngtcp2_ssize ndatalen;

  auto nwrite =
    ngtcp2_conn_writev_stream(conn, &ps.path, nullptr, buf.data(), buf.size(),
                              &ndatalen, flags, stream_id, &vec, veccnt, ts);
  if (nwrite < 0) {
    if (nwrite == NGTCP2_ERR_STREAM_DATA_BLOCKED) {
      return 0;
    }

    std::cerr << "ngtcp2_conn_writev_stream: "
              << ngtcp2_strerror(static_cast<int>(nwrite)) << std::endl;

    return -1;
  }

  if (nwrite == 0) {
    return 0;
  }

  if (ndatalen > 0) {
    bytes_sent_ += static_cast<size_t>(ndatalen);
  }

  ngtcp2_conn_update_pkt_tx_time(conn, ts);

  ctx.endpoint->get_channel().send_pkt(
    to_network_path(&ps.path), {buf.data(), static_cast<size_t>(nwrite)});

  return 0;
}

} // namespace ngtcp2
