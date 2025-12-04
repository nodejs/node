/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
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
#include <cstdlib>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <memory>
#include <fstream>
#include <iomanip>

#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/mman.h>
#include <libgen.h>
#include <netinet/udp.h>

#include <urlparse.h>

#include "h09client.h"
#include "network.h"
#include "debug.h"
#include "util.h"
#include "shared.h"

using namespace ngtcp2;
using namespace std::literals;

namespace {
auto randgen = util::make_mt19937();
} // namespace

namespace {
constexpr size_t max_preferred_versionslen = 4;
} // namespace

Config config{};

Stream::Stream(const Request &req, int64_t stream_id)
  : req(req), stream_id(stream_id), fd(-1) {
  nghttp3_buf_init(&reqbuf);
}

Stream::~Stream() {
  if (fd != -1) {
    close(fd);
  }
}

int Stream::open_file(const std::string_view &path) {
  assert(fd == -1);

  std::string_view filename;

  auto it = std::ranges::find(std::rbegin(path), std::rend(path), '/').base();
  if (it == std::ranges::end(path)) {
    filename = "index.html"sv;
  } else {
    filename = std::string_view{it, std::ranges::end(path)};
    if (filename == ".."sv || filename == "."sv) {
      std::cerr << "Invalid file name: " << filename << std::endl;
      return -1;
    }
  }

  auto fname = std::string{config.download};
  fname += '/';
  fname += filename;

  fd = open(fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd == -1) {
    std::cerr << "open: Could not open file " << fname << ": "
              << strerror(errno) << std::endl;
    return -1;
  }

  return 0;
}

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  auto c = static_cast<Client *>(w->data);

  c->on_write();
}
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto ep = static_cast<Endpoint *>(w->data);
  auto c = ep->client;

  if (c->on_read(*ep) != 0) {
    return;
  }

  c->on_write();
}
} // namespace

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  int rv;
  auto c = static_cast<Client *>(w->data);

  rv = c->handle_expiry();
  if (rv != 0) {
    return;
  }

  c->on_write();
}
} // namespace

namespace {
void change_local_addrcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto c = static_cast<Client *>(w->data);

  c->change_local_addr();
}
} // namespace

namespace {
void key_updatecb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto c = static_cast<Client *>(w->data);

  if (c->initiate_key_update() != 0) {
    c->disconnect();
  }
}
} // namespace

namespace {
void delay_streamcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto c = static_cast<Client *>(w->data);

  ev_timer_stop(loop, w);
  c->on_extend_max_streams();
  c->on_write();
}
} // namespace

namespace {
void siginthandler(struct ev_loop *loop, ev_signal *w, int revents) {
  ev_break(loop, EVBREAK_ALL);
}
} // namespace

Client::Client(struct ev_loop *loop, uint32_t client_chosen_version,
               uint32_t original_version)
  : remote_addr_{},
    loop_(loop),
    addr_(nullptr),
    port_(nullptr),
    nstreams_done_(0),
    nstreams_closed_(0),
    nkey_update_(0),
    client_chosen_version_(client_chosen_version),
    original_version_(original_version),
    early_data_(false),
    handshake_confirmed_(false),
    no_gso_{
#ifdef UDP_SEGMENT
      false
#else  // !defined(UDP_SEGMENT)
      true
#endif // !defined(UDP_SEGMENT)
    },
    tx_{} {
  ev_io_init(&wev_, writecb, 0, EV_WRITE);
  wev_.data = this;
  ev_timer_init(&timer_, timeoutcb, 0., 0.);
  timer_.data = this;
  ev_timer_init(&change_local_addr_timer_, change_local_addrcb,
                static_cast<double>(config.change_local_addr) / NGTCP2_SECONDS,
                0.);
  change_local_addr_timer_.data = this;
  ev_timer_init(&key_update_timer_, key_updatecb,
                static_cast<double>(config.key_update) / NGTCP2_SECONDS, 0.);
  key_update_timer_.data = this;
  ev_timer_init(&delay_stream_timer_, delay_streamcb,
                static_cast<double>(config.delay_stream) / NGTCP2_SECONDS, 0.);
  delay_stream_timer_.data = this;
  ev_signal_init(&sigintev_, siginthandler, SIGINT);
}

Client::~Client() { disconnect(); }

void Client::disconnect() {
  tx_.send_blocked = false;

  handle_error();

  config.tx_loss_prob = 0;

  ev_timer_stop(loop_, &delay_stream_timer_);
  ev_timer_stop(loop_, &key_update_timer_);
  ev_timer_stop(loop_, &change_local_addr_timer_);
  ev_timer_stop(loop_, &timer_);

  ev_io_stop(loop_, &wev_);

  for (auto &ep : endpoints_) {
    ev_io_stop(loop_, &ep.rev);
    close(ep.fd);
  }

  endpoints_.clear();

  ev_signal_stop(loop_, &sigintev_);
}

namespace {
int recv_crypto_data(ngtcp2_conn *conn,
                     ngtcp2_encryption_level encryption_level, uint64_t offset,
                     const uint8_t *data, size_t datalen, void *user_data) {
  if (!config.quiet && !config.no_quic_dump) {
    debug::print_crypto_data(encryption_level, {data, datalen});
  }

  return ngtcp2_crypto_recv_crypto_data_cb(conn, encryption_level, offset, data,
                                           datalen, user_data);
}
} // namespace

namespace {
int recv_stream_data(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id,
                     uint64_t offset, const uint8_t *data, size_t datalen,
                     void *user_data, void *stream_user_data) {
  if (!config.quiet && !config.no_quic_dump) {
    debug::print_stream_data(stream_id, {data, datalen});
  }

  auto c = static_cast<Client *>(user_data);

  if (c->recv_stream_data(flags, stream_id, {data, datalen}) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

namespace {
int acked_stream_data_offset(ngtcp2_conn *conn, int64_t stream_id,
                             uint64_t offset, uint64_t datalen, void *user_data,
                             void *stream_user_data) {
  auto c = static_cast<Client *>(user_data);
  if (c->acked_stream_data_offset(stream_id, offset, datalen) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

namespace {
int handshake_completed(ngtcp2_conn *conn, void *user_data) {
  auto c = static_cast<Client *>(user_data);

  if (!config.quiet) {
    debug::handshake_completed(conn, user_data);
  }

  if (c->handshake_completed() != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

int Client::handshake_completed() {
  if (early_data_ && !tls_session_.get_early_data_accepted()) {
    if (!config.quiet) {
      std::cerr << "Early data was rejected by server" << std::endl;
    }

    // Some TLS backends only report early data rejection after
    // handshake completion (e.g., OpenSSL).  For TLS backends which
    // report it early (e.g., BoringSSL and PicoTLS), the following
    // functions are noop.
    if (auto rv = ngtcp2_conn_tls_early_data_rejected(conn_); rv != 0) {
      std::cerr << "ngtcp2_conn_tls_early_data_rejected: "
                << ngtcp2_strerror(rv) << std::endl;
      return -1;
    }
  }

  if (!config.quiet) {
    std::cerr << "Negotiated cipher suite is " << tls_session_.get_cipher_name()
              << std::endl;
    if (auto group = tls_session_.get_negotiated_group(); !group.empty()) {
      std::cerr << "Negotiated group is " << group << std::endl;
    }
    std::cerr << "Negotiated ALPN is " << tls_session_.get_selected_alpn()
              << std::endl;

    if (!config.ech_config_list.empty() && tls_session_.get_ech_accepted()) {
      std::cerr << "ECH was accepted" << std::endl;
    }
  }

  if (config.tp_file) {
    std::array<uint8_t, 256> data;
    auto datalen =
      ngtcp2_conn_encode_0rtt_transport_params(conn_, data.data(), data.size());
    if (datalen < 0) {
      std::cerr << "Could not encode 0-RTT transport parameters: "
                << ngtcp2_strerror(static_cast<int>(datalen)) << std::endl;
    } else if (util::write_transport_params(
                 config.tp_file, {data.data(), static_cast<size_t>(datalen)}) !=
               0) {
      std::cerr << "Could not write transport parameters in " << config.tp_file
                << std::endl;
    }
  }

  return 0;
}

namespace {
int handshake_confirmed(ngtcp2_conn *conn, void *user_data) {
  auto c = static_cast<Client *>(user_data);

  if (!config.quiet) {
    debug::handshake_confirmed(conn, user_data);
  }

  if (c->handshake_confirmed() != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

bool Client::should_exit() const {
  return handshake_confirmed_ &&
         (!config.wait_for_ticket || ticket_received_) &&
         ((config.exit_on_first_stream_close &&
           (config.nstreams == 0 || nstreams_closed_)) ||
          (config.exit_on_all_streams_close &&
           config.nstreams == nstreams_done_ &&
           nstreams_closed_ == nstreams_done_));
}

int Client::handshake_confirmed() {
  handshake_confirmed_ = true;

  if (config.change_local_addr) {
    start_change_local_addr_timer();
  }
  if (config.key_update) {
    start_key_update_timer();
  }
  if (config.delay_stream) {
    start_delay_stream_timer();
  }

  return 0;
}

namespace {
int recv_version_negotiation(ngtcp2_conn *conn, const ngtcp2_pkt_hd *hd,
                             const uint32_t *sv, size_t nsv, void *user_data) {
  auto c = static_cast<Client *>(user_data);

  c->recv_version_negotiation(sv, nsv);

  return 0;
}
} // namespace

void Client::recv_version_negotiation(const uint32_t *sv, size_t nsv) {
  offered_versions_.resize(nsv);
  std::ranges::copy_n(sv, as_signed(nsv),
                      std::ranges::begin(offered_versions_));
}

namespace {
int stream_close(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id,
                 uint64_t app_error_code, void *user_data,
                 void *stream_user_data) {
  auto c = static_cast<Client *>(user_data);

  if (c->on_stream_close(stream_id, app_error_code) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

namespace {
int extend_max_local_streams_bidi(ngtcp2_conn *conn, uint64_t max_streams,
                                  void *user_data) {
  auto c = static_cast<Client *>(user_data);

  if (c->on_extend_max_streams() != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

namespace {
void rand(uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *rand_ctx) {
  auto rv = util::generate_secure_random({dest, destlen});
  if (rv != 0) {
    assert(0);
    abort();
  }
}
} // namespace

namespace {
int get_new_connection_id(ngtcp2_conn *conn, ngtcp2_cid *cid, uint8_t *token,
                          size_t cidlen, void *user_data) {
  if (util::generate_secure_random({cid->data, cidlen}) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  cid->datalen = cidlen;
  if (ngtcp2_crypto_generate_stateless_reset_token(
        token, config.static_secret.data(), config.static_secret.size(), cid) !=
      0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

namespace {
int do_hp_mask(uint8_t *dest, const ngtcp2_crypto_cipher *hp,
               const ngtcp2_crypto_cipher_ctx *hp_ctx, const uint8_t *sample) {
  if (ngtcp2_crypto_hp_mask(dest, hp, hp_ctx, sample) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  if (!config.quiet && config.show_secret) {
    debug::print_hp_mask({dest, NGTCP2_HP_MASKLEN},
                         {sample, NGTCP2_HP_SAMPLELEN});
  }

  return 0;
}
} // namespace

namespace {
int update_key(ngtcp2_conn *conn, uint8_t *rx_secret, uint8_t *tx_secret,
               ngtcp2_crypto_aead_ctx *rx_aead_ctx, uint8_t *rx_iv,
               ngtcp2_crypto_aead_ctx *tx_aead_ctx, uint8_t *tx_iv,
               const uint8_t *current_rx_secret,
               const uint8_t *current_tx_secret, size_t secretlen,
               void *user_data) {
  auto c = static_cast<Client *>(user_data);

  if (c->update_key(rx_secret, tx_secret, rx_aead_ctx, rx_iv, tx_aead_ctx,
                    tx_iv, current_rx_secret, current_tx_secret,
                    secretlen) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

namespace {
int path_validation(ngtcp2_conn *conn, uint32_t flags, const ngtcp2_path *path,
                    const ngtcp2_path *old_path,
                    ngtcp2_path_validation_result res, void *user_data) {
  if (!config.quiet) {
    debug::path_validation(path, res);
  }

  if (flags & NGTCP2_PATH_VALIDATION_FLAG_PREFERRED_ADDR) {
    auto c = static_cast<Client *>(user_data);

    c->set_remote_addr(path->remote);
  }

  return 0;
}
} // namespace

void Client::set_remote_addr(const ngtcp2_addr &remote_addr) {
  memcpy(&remote_addr_.su, remote_addr.addr, remote_addr.addrlen);
  remote_addr_.len = remote_addr.addrlen;
}

namespace {
int select_preferred_address(ngtcp2_conn *conn, ngtcp2_path *dest,
                             const ngtcp2_preferred_addr *paddr,
                             void *user_data) {
  auto c = static_cast<Client *>(user_data);
  Address remote_addr;

  if (config.no_preferred_addr) {
    return 0;
  }

  if (c->select_preferred_address(remote_addr, paddr) != 0) {
    return 0;
  }

  auto ep = c->endpoint_for(remote_addr);
  if (!ep) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  ngtcp2_addr_copy_byte(&dest->local, &(*ep)->addr.su.sa, (*ep)->addr.len);
  ngtcp2_addr_copy_byte(&dest->remote, &remote_addr.su.sa, remote_addr.len);
  dest->user_data = *ep;

  return 0;
}
} // namespace

namespace {
int extend_max_stream_data(ngtcp2_conn *conn, int64_t stream_id,
                           uint64_t max_data, void *user_data,
                           void *stream_user_data) {
  auto c = static_cast<Client *>(user_data);
  if (c->extend_max_stream_data(stream_id, max_data) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

int Client::extend_max_stream_data(int64_t stream_id, uint64_t max_data) {
  auto it = streams_.find(stream_id);
  assert(it != std::ranges::end(streams_));
  auto &stream = (*it).second;

  if (nghttp3_buf_len(&stream->reqbuf)) {
    sendq_.emplace(stream.get());
  }

  return 0;
}

namespace {
int recv_new_token(ngtcp2_conn *conn, const uint8_t *token, size_t tokenlen,
                   void *user_data) {
  if (config.token_file.empty()) {
    return 0;
  }

  util::write_token(config.token_file, {token, tokenlen});

  return 0;
}
} // namespace

namespace {
int early_data_rejected(ngtcp2_conn *conn, void *user_data) {
  auto c = static_cast<Client *>(user_data);

  c->early_data_rejected();

  return 0;
}
} // namespace

void Client::early_data_rejected() {
  nstreams_done_ = 0;
  streams_.clear();
}

int Client::init(int fd, const Address &local_addr, const Address &remote_addr,
                 const char *addr, const char *port,
                 TLSClientContext &tls_ctx) {
  endpoints_.reserve(4);

  endpoints_.emplace_back();
  auto &ep = endpoints_.back();
  ep.addr = local_addr;
  ep.client = this;
  ep.fd = fd;
  ev_io_init(&ep.rev, readcb, fd, EV_READ);
  ep.rev.data = &ep;

  remote_addr_ = remote_addr;
  addr_ = addr;
  port_ = port;

  auto callbacks = ngtcp2_callbacks{
    .client_initial = ngtcp2_crypto_client_initial_cb,
    .recv_crypto_data = ::recv_crypto_data,
    .handshake_completed = ::handshake_completed,
    .recv_version_negotiation = ::recv_version_negotiation,
    .encrypt = ngtcp2_crypto_encrypt_cb,
    .decrypt = ngtcp2_crypto_decrypt_cb,
    .hp_mask = do_hp_mask,
    .recv_stream_data = ::recv_stream_data,
    .acked_stream_data_offset = ::acked_stream_data_offset,
    .stream_close = stream_close,
    .recv_retry = ngtcp2_crypto_recv_retry_cb,
    .extend_max_local_streams_bidi = extend_max_local_streams_bidi,
    .rand = rand,
    .get_new_connection_id = get_new_connection_id,
    .update_key = ::update_key,
    .path_validation = path_validation,
    .select_preferred_addr = ::select_preferred_address,
    .extend_max_stream_data = ::extend_max_stream_data,
    .handshake_confirmed = ::handshake_confirmed,
    .recv_new_token = ::recv_new_token,
    .delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    .delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    .get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb,
    .version_negotiation = ngtcp2_crypto_version_negotiation_cb,
    .tls_early_data_rejected = ::early_data_rejected,
  };

  ngtcp2_cid scid, dcid;
  scid.datalen = 17;
  if (util::generate_secure_random({scid.data, scid.datalen}) != 0) {
    std::cerr << "Could not generate source connection ID" << std::endl;
    return -1;
  }
  if (config.dcid.datalen == 0) {
    dcid.datalen = 18;
    if (util::generate_secure_random({dcid.data, dcid.datalen}) != 0) {
      std::cerr << "Could not generate destination connection ID" << std::endl;
      return -1;
    }
  } else {
    dcid = config.dcid;
  }

  ngtcp2_settings settings;
  ngtcp2_settings_default(&settings);
  settings.log_printf = config.quiet ? nullptr : debug::log_printf;
  if (!config.qlog_file.empty() || !config.qlog_dir.empty()) {
    std::string path;
    if (!config.qlog_file.empty()) {
      path = config.qlog_file;
    } else {
      path = std::string{config.qlog_dir};
      path += '/';
      path += util::format_hex(scid.data, as_signed(scid.datalen));
      path += ".sqlog";
    }
    qlog_ = fopen(path.c_str(), "w");
    if (qlog_ == nullptr) {
      std::cerr << "Could not open qlog file " << std::quoted(path) << ": "
                << strerror(errno) << std::endl;
      return -1;
    }
    settings.qlog_write = qlog_write_cb;
  }

  settings.cc_algo = config.cc_algo;
  settings.initial_ts = util::timestamp();
  settings.initial_rtt = config.initial_rtt;
  settings.max_window = config.max_window;
  settings.max_stream_window = config.max_stream_window;
  if (config.max_udp_payload_size) {
    settings.max_tx_udp_payload_size = config.max_udp_payload_size;
    settings.no_tx_udp_payload_size_shaping = 1;
  }
  settings.handshake_timeout = config.handshake_timeout;
  settings.no_pmtud = config.no_pmtud;
  settings.ack_thresh = config.ack_thresh;
  if (config.initial_pkt_num == UINT32_MAX) {
    auto dis = std::uniform_int_distribution<uint32_t>(0, INT32_MAX);
    settings.initial_pkt_num = dis(randgen);
  } else {
    settings.initial_pkt_num = config.initial_pkt_num;
  }

  std::vector<uint8_t> token;

  if (!config.token_file.empty()) {
    std::cerr << "Reading token file " << config.token_file << std::endl;

    auto t = util::read_token(config.token_file);
    if (t) {
      token = std::move(*t);
      settings.token = token.data();
      settings.tokenlen = token.size();
    }
  }

  if (!config.available_versions.empty()) {
    settings.available_versions = config.available_versions.data();
    settings.available_versionslen = config.available_versions.size();
  }

  if (!config.preferred_versions.empty()) {
    settings.preferred_versions = config.preferred_versions.data();
    settings.preferred_versionslen = config.preferred_versions.size();
  }

  settings.original_version = original_version_;

  if (!config.pmtud_probes.empty()) {
    settings.pmtud_probes = config.pmtud_probes.data();
    settings.pmtud_probeslen = config.pmtud_probes.size();

    if (!config.max_udp_payload_size) {
      settings.max_tx_udp_payload_size =
        *std::ranges::max_element(config.pmtud_probes);
    }
  }

  ngtcp2_transport_params params;
  ngtcp2_transport_params_default(&params);
  params.initial_max_stream_data_bidi_local = config.max_stream_data_bidi_local;
  params.initial_max_stream_data_bidi_remote =
    config.max_stream_data_bidi_remote;
  params.initial_max_stream_data_uni = config.max_stream_data_uni;
  params.initial_max_data = config.max_data;
  params.initial_max_streams_bidi = config.max_streams_bidi;
  params.initial_max_streams_uni = 0;
  params.max_idle_timeout = config.timeout;
  params.active_connection_id_limit = 7;
  params.grease_quic_bit = 1;

  auto path = ngtcp2_path{
    .local =
      {
        .addr = const_cast<sockaddr *>(&ep.addr.su.sa),
        .addrlen = ep.addr.len,
      },
    .remote =
      {
        .addr = const_cast<sockaddr *>(&remote_addr.su.sa),
        .addrlen = remote_addr.len,
      },
    .user_data = &ep,
  };
  auto rv =
    ngtcp2_conn_client_new(&conn_, &dcid, &scid, &path, client_chosen_version_,
                           &callbacks, &settings, &params, nullptr, this);

  if (rv != 0) {
    std::cerr << "ngtcp2_conn_client_new: " << ngtcp2_strerror(rv) << std::endl;
    return -1;
  }

  if (tls_session_.init(early_data_, tls_ctx, addr_, this,
                        client_chosen_version_, AppProtocol::HQ) != 0) {
    return -1;
  }

  ngtcp2_conn_set_tls_native_handle(conn_, tls_session_.get_native_handle());

  if (early_data_ && config.tp_file) {
    auto params = util::read_transport_params(config.tp_file);
    if (!params) {
      early_data_ = false;
    } else {
      auto rv = ngtcp2_conn_decode_and_set_0rtt_transport_params(
        conn_, params->data(), params->size());
      if (rv != 0) {
        std::cerr << "ngtcp2_conn_decode_and_set_0rtt_transport_params:"
                  << ngtcp2_strerror(rv) << std::endl;
        early_data_ = false;
      } else if (make_stream_early() != 0) {
        return -1;
      }
    }
  }

  ev_io_start(loop_, &ep.rev);

  ev_signal_start(loop_, &sigintev_);

  return 0;
}

int Client::feed_data(const Endpoint &ep, const sockaddr *sa, socklen_t salen,
                      const ngtcp2_pkt_info *pi,
                      std::span<const uint8_t> data) {
  auto path = ngtcp2_path{
    .local =
      {
        .addr = const_cast<sockaddr *>(&ep.addr.su.sa),
        .addrlen = ep.addr.len,
      },
    .remote =
      {
        .addr = const_cast<sockaddr *>(sa),
        .addrlen = salen,
      },
    .user_data = const_cast<Endpoint *>(&ep),
  };
  if (auto rv = ngtcp2_conn_read_pkt(conn_, &path, pi, data.data(), data.size(),
                                     util::timestamp());
      rv != 0) {
    std::cerr << "ngtcp2_conn_read_pkt: " << ngtcp2_strerror(rv) << std::endl;
    if (!last_error_.error_code) {
      if (rv == NGTCP2_ERR_CRYPTO) {
        auto alert = ngtcp2_conn_get_tls_alert(conn_);
        ngtcp2_ccerr_set_tls_alert(&last_error_, alert, nullptr, 0);

        if (alert == TLS_ALERT_ECH_REQUIRED && config.ech_config_list_file &&
            tls_session_.write_ech_config_list(config.ech_config_list_file) !=
              0) {
          std::cerr << "Could not write ECH retry configs in "
                    << config.ech_config_list_file << std::endl;
        }
      } else {
        ngtcp2_ccerr_set_liberr(&last_error_, rv, nullptr, 0);
      }
    }
    disconnect();
    return -1;
  }
  return 0;
}

int Client::on_read(const Endpoint &ep) {
  std::array<uint8_t, 64_k> buf;
  sockaddr_union su;
  size_t pktcnt = 0;
  ngtcp2_pkt_info pi;

  iovec msg_iov{
    .iov_base = buf.data(),
    .iov_len = buf.size(),
  };

  uint8_t msg_ctrl[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(int))];

  msghdr msg{
    .msg_name = &su,
    .msg_iov = &msg_iov,
    .msg_iovlen = 1,
    .msg_control = msg_ctrl,
  };

  for (;;) {
    msg.msg_namelen = sizeof(su);
    msg.msg_controllen = sizeof(msg_ctrl);

    auto nread = recvmsg(ep.fd, &msg, 0);

    if (nread == -1) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cerr << "recvmsg: " << strerror(errno) << std::endl;
      }
      break;
    }

    // Packets less than 21 bytes never be a valid QUIC packet.
    if (nread < 21) {
      ++pktcnt;

      continue;
    }

    pi.ecn = msghdr_get_ecn(&msg, su.storage.ss_family);
    auto gso_size = msghdr_get_udp_gro(&msg);
    if (gso_size == 0) {
      gso_size = static_cast<size_t>(nread);
    }

    auto data = std::span{buf.data(), static_cast<size_t>(nread)};

    for (;;) {
      auto datalen = std::min(data.size(), gso_size);

      ++pktcnt;

      if (!config.quiet) {
        std::cerr << "Received packet: local="
                  << util::straddr(&ep.addr.su.sa, ep.addr.len)
                  << " remote=" << util::straddr(&su.sa, msg.msg_namelen)
                  << " ecn=0x" << std::hex << static_cast<uint32_t>(pi.ecn)
                  << std::dec << " " << datalen << " bytes" << std::endl;
      }

      // Packets less than 21 bytes never be a valid QUIC packet.
      if (datalen < 21) {
        break;
      }

      if (debug::packet_lost(config.rx_loss_prob)) {
        if (!config.quiet) {
          std::cerr << "** Simulated incoming packet loss **" << std::endl;
        }
      } else if (feed_data(ep, &su.sa, msg.msg_namelen, &pi,
                           {data.data(), datalen}) != 0) {
        return -1;
      }

      data = data.subspan(datalen);

      if (data.empty()) {
        break;
      }
    }

    if (pktcnt >= 10) {
      break;
    }
  }

  if (should_exit()) {
    disconnect();
    return -1;
  }

  update_timer();

  return 0;
}

int Client::handle_expiry() {
  auto now = util::timestamp();
  if (auto rv = ngtcp2_conn_handle_expiry(conn_, now); rv != 0) {
    std::cerr << "ngtcp2_conn_handle_expiry: " << ngtcp2_strerror(rv)
              << std::endl;
    ngtcp2_ccerr_set_liberr(&last_error_, rv, nullptr, 0);
    disconnect();
    return -1;
  }

  return 0;
}

int Client::on_write() {
  if (tx_.send_blocked) {
    if (auto rv = send_blocked_packet(); rv != 0) {
      return rv;
    }

    if (tx_.send_blocked) {
      return 0;
    }
  }

  ev_io_stop(loop_, &wev_);

  if (auto rv = write_streams(); rv != 0) {
    return rv;
  }

  if (should_exit()) {
    disconnect();
    return -1;
  }

  update_timer();
  return 0;
}

namespace {
ngtcp2_ssize write_pkt(ngtcp2_conn *conn, ngtcp2_path *path,
                       ngtcp2_pkt_info *pi, uint8_t *dest, size_t destlen,
                       ngtcp2_tstamp ts, void *user_data) {
  auto c = static_cast<Client *>(user_data);

  return c->write_pkt(path, pi, dest, destlen, ts);
}
} // namespace

ngtcp2_ssize Client::write_pkt(ngtcp2_path *path, ngtcp2_pkt_info *pi,
                               uint8_t *dest, size_t destlen,
                               ngtcp2_tstamp ts) {
  ngtcp2_vec vec;

  for (;;) {
    int64_t stream_id = -1;
    size_t vcnt = 0;
    uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_MORE;
    Stream *stream = nullptr;

    if (!sendq_.empty() && ngtcp2_conn_get_max_data_left(conn_)) {
      stream = *std::ranges::begin(sendq_);

      stream_id = stream->stream_id;
      vec.base = stream->reqbuf.pos;
      vec.len = nghttp3_buf_len(&stream->reqbuf);
      vcnt = 1;
      flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
    }

    ngtcp2_ssize ndatalen;

    auto nwrite =
      ngtcp2_conn_writev_stream(conn_, path, pi, dest, destlen, &ndatalen,
                                flags, stream_id, &vec, vcnt, ts);
    if (nwrite < 0) {
      switch (nwrite) {
      case NGTCP2_ERR_STREAM_DATA_BLOCKED:
      case NGTCP2_ERR_STREAM_SHUT_WR:
        assert(ndatalen == -1);
        sendq_.erase(std::ranges::begin(sendq_));
        continue;
      case NGTCP2_ERR_WRITE_MORE:
        assert(ndatalen >= 0);
        stream->reqbuf.pos += ndatalen;
        if (nghttp3_buf_len(&stream->reqbuf) == 0) {
          sendq_.erase(std::ranges::begin(sendq_));
        }
        continue;
      }

      assert(ndatalen == -1);

      std::cerr << "ngtcp2_conn_write_stream: "
                << ngtcp2_strerror(static_cast<int>(nwrite)) << std::endl;
      ngtcp2_ccerr_set_liberr(&last_error_, static_cast<int>(nwrite), nullptr,
                              0);

      return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    if (ndatalen >= 0) {
      stream->reqbuf.pos += ndatalen;
      if (nghttp3_buf_len(&stream->reqbuf) == 0) {
        sendq_.erase(std::ranges::begin(sendq_));
      }
    }

    return nwrite;
  }
}

int Client::write_streams() {
  ngtcp2_path_storage ps;
  ngtcp2_pkt_info pi;
  size_t gso_size;
  auto ts = util::timestamp();
  auto txbuf = std::span{tx_.data};

  ngtcp2_path_storage_zero(&ps);

  auto nwrite =
    ngtcp2_conn_write_aggregate_pkt(conn_, &ps.path, &pi, txbuf.data(),
                                    txbuf.size(), &gso_size, ::write_pkt, ts);
  if (nwrite < 0) {
    disconnect();
    return -1;
  }

  if (nwrite == 0) {
    return 0;
  }

  send_packet_or_blocked(ps.path, pi.ecn,
                         txbuf.first(static_cast<size_t>(nwrite)), gso_size);

  return 0;
}

int Client::send_packet_or_blocked(const ngtcp2_path &path, unsigned int ecn,
                                   std::span<const uint8_t> data,
                                   size_t gso_size) {
  auto &ep = *static_cast<Endpoint *>(path.user_data);

  auto [rest, rv] = send_packet(ep, path.remote, ecn, data, gso_size);
  if (rv != 0) {
    assert(NETWORK_ERR_SEND_BLOCKED == rv);

    on_send_blocked(path, ecn, rest, gso_size);

    return rv;
  }

  return 0;
}

void Client::update_timer() {
  auto expiry = ngtcp2_conn_get_expiry(conn_);
  auto now = util::timestamp();

  if (expiry <= now) {
    if (!config.quiet) {
      auto t = static_cast<ev_tstamp>(now - expiry) / NGTCP2_SECONDS;
      std::cerr << "Timer has already expired: " << std::fixed << t << "s"
                << std::defaultfloat << std::endl;
    }

    ev_feed_event(loop_, &timer_, EV_TIMER);

    return;
  }

  auto t = static_cast<ev_tstamp>(expiry - now) / NGTCP2_SECONDS;
  if (!config.quiet) {
    std::cerr << "Set timer=" << std::fixed << t << "s" << std::defaultfloat
              << std::endl;
  }
  timer_.repeat = t;
  ev_timer_again(loop_, &timer_);
}

#ifdef HAVE_LINUX_RTNETLINK_H
namespace {
int bind_addr(Address &local_addr, int fd, const in_addr_union *iau,
              int family) {
  addrinfo hints{
    .ai_flags = AI_PASSIVE,
    .ai_family = family,
    .ai_socktype = SOCK_DGRAM,
  };
  addrinfo *res, *rp;
  char *node;
  std::array<char, NI_MAXHOST> nodebuf;

  if (iau) {
    if (inet_ntop(family, iau, nodebuf.data(), nodebuf.size()) == nullptr) {
      std::cerr << "inet_ntop: " << strerror(errno) << std::endl;
      return -1;
    }

    node = nodebuf.data();
  } else {
    node = nullptr;
  }

  if (auto rv = getaddrinfo(node, "0", &hints, &res); rv != 0) {
    std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
    return -1;
  }

  auto res_d = defer(freeaddrinfo, res);

  for (rp = res; rp; rp = rp->ai_next) {
    if (bind(fd, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }
  }

  if (!rp) {
    std::cerr << "Could not bind" << std::endl;
    return -1;
  }

  socklen_t len = sizeof(local_addr.su.storage);
  if (getsockname(fd, &local_addr.su.sa, &len) == -1) {
    std::cerr << "getsockname: " << strerror(errno) << std::endl;
    return -1;
  }
  local_addr.len = len;
  local_addr.ifindex = 0;

  return 0;
}
} // namespace
#endif // defined(HAVE_LINUX_RTNETLINK_H)

#ifndef HAVE_LINUX_RTNETLINK_H
namespace {
int connect_sock(Address &local_addr, int fd, const Address &remote_addr) {
  if (connect(fd, &remote_addr.su.sa, remote_addr.len) != 0) {
    std::cerr << "connect: " << strerror(errno) << std::endl;
    return -1;
  }

  socklen_t len = sizeof(local_addr.su.storage);
  if (getsockname(fd, &local_addr.su.sa, &len) == -1) {
    std::cerr << "getsockname: " << strerror(errno) << std::endl;
    return -1;
  }
  local_addr.len = len;
  local_addr.ifindex = 0;

  return 0;
}
} // namespace
#endif // !defined(HAVE_LINUX_RTNETLINK_H)

namespace {
int udp_sock(int family) {
  auto fd = util::create_nonblock_socket(family, SOCK_DGRAM, IPPROTO_UDP);
  if (fd == -1) {
    return -1;
  }

  fd_set_recv_ecn(fd, family);
  fd_set_ip_mtu_discover(fd, family);
  fd_set_ip_dontfrag(fd, family);
  fd_set_udp_gro(fd);

  return fd;
}
} // namespace

namespace {
int create_sock(Address &remote_addr, const char *addr, const char *port) {
  addrinfo hints{
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_DGRAM,
  };
  addrinfo *res, *rp;

  if (auto rv = getaddrinfo(addr, port, &hints, &res); rv != 0) {
    std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
    return -1;
  }

  auto res_d = defer(freeaddrinfo, res);

  int fd = -1;

  for (rp = res; rp; rp = rp->ai_next) {
    fd = udp_sock(rp->ai_family);
    if (fd == -1) {
      continue;
    }

    break;
  }

  if (!rp) {
    std::cerr << "Could not create socket" << std::endl;
    return -1;
  }

  remote_addr.len = rp->ai_addrlen;
  memcpy(&remote_addr.su, rp->ai_addr, rp->ai_addrlen);
  remote_addr.ifindex = 0;

  return fd;
}
} // namespace

std::optional<Endpoint *> Client::endpoint_for(const Address &remote_addr) {
#ifdef HAVE_LINUX_RTNETLINK_H
  in_addr_union iau;

  if (get_local_addr(iau, remote_addr) != 0) {
    std::cerr << "Could not get local address for a selected preferred address"
              << std::endl;
    return nullptr;
  }

  auto current_path = ngtcp2_conn_get_path(conn_);
  auto current_ep = static_cast<Endpoint *>(current_path->user_data);
  if (addreq(&current_ep->addr.su.sa, iau)) {
    return current_ep;
  }
#endif // defined(HAVE_LINUX_RTNETLINK_H)

  auto fd = udp_sock(remote_addr.su.sa.sa_family);
  if (fd == -1) {
    return nullptr;
  }

  Address local_addr;

#ifdef HAVE_LINUX_RTNETLINK_H
  if (bind_addr(local_addr, fd, &iau, remote_addr.su.sa.sa_family) != 0) {
    close(fd);
    return nullptr;
  }
#else  // !defined(HAVE_LINUX_RTNETLINK_H)
  if (connect_sock(local_addr, fd, remote_addr) != 0) {
    close(fd);
    return nullptr;
  }
#endif // !defined(HAVE_LINUX_RTNETLINK_H)

  endpoints_.emplace_back();
  auto &ep = endpoints_.back();
  ep.addr = local_addr;
  ep.client = this;
  ep.fd = fd;
  ev_io_init(&ep.rev, readcb, fd, EV_READ);
  ep.rev.data = &ep;

  ev_io_start(loop_, &ep.rev);

  return &ep;
}

void Client::start_change_local_addr_timer() {
  ev_timer_start(loop_, &change_local_addr_timer_);
}

int Client::change_local_addr() {
  Address local_addr;

  if (!config.quiet) {
    std::cerr << "Changing local address" << std::endl;
  }

  auto nfd = udp_sock(remote_addr_.su.sa.sa_family);
  if (nfd == -1) {
    return -1;
  }

#ifdef HAVE_LINUX_RTNETLINK_H
  in_addr_union iau;

  if (get_local_addr(iau, remote_addr_) != 0) {
    std::cerr << "Could not get local address" << std::endl;
    close(nfd);
    return -1;
  }

  if (bind_addr(local_addr, nfd, &iau, remote_addr_.su.sa.sa_family) != 0) {
    close(nfd);
    return -1;
  }
#else  // !defined(HAVE_LINUX_RTNETLINK_H)
  if (connect_sock(local_addr, nfd, remote_addr_) != 0) {
    close(nfd);
    return -1;
  }
#endif // !defined(HAVE_LINUX_RTNETLINK_H)

  if (!config.quiet) {
    std::cerr << "Local address is now "
              << util::straddr(&local_addr.su.sa, local_addr.len) << std::endl;
  }

  endpoints_.emplace_back();
  auto &ep = endpoints_.back();
  ep.addr = local_addr;
  ep.client = this;
  ep.fd = nfd;
  ev_io_init(&ep.rev, readcb, nfd, EV_READ);
  ep.rev.data = &ep;

  ngtcp2_addr addr;
  ngtcp2_addr_init(&addr, &local_addr.su.sa, local_addr.len);

  if (config.nat_rebinding) {
    ngtcp2_conn_set_local_addr(conn_, &addr);
    ngtcp2_conn_set_path_user_data(conn_, &ep);
  } else {
    auto path = ngtcp2_path{
      .local = addr,
      .remote =
        {
          .addr = const_cast<sockaddr *>(&remote_addr_.su.sa),
          .addrlen = remote_addr_.len,
        },
      .user_data = &ep,
    };
    if (auto rv = ngtcp2_conn_initiate_immediate_migration(conn_, &path,
                                                           util::timestamp());
        rv != 0) {
      std::cerr << "ngtcp2_conn_initiate_immediate_migration: "
                << ngtcp2_strerror(rv) << std::endl;
    }
  }

  ev_io_start(loop_, &ep.rev);

  return 0;
}

void Client::start_key_update_timer() {
  ev_timer_start(loop_, &key_update_timer_);
}

int Client::update_key(uint8_t *rx_secret, uint8_t *tx_secret,
                       ngtcp2_crypto_aead_ctx *rx_aead_ctx, uint8_t *rx_iv,
                       ngtcp2_crypto_aead_ctx *tx_aead_ctx, uint8_t *tx_iv,
                       const uint8_t *current_rx_secret,
                       const uint8_t *current_tx_secret, size_t secretlen) {
  if (!config.quiet) {
    std::cerr << "Updating traffic key" << std::endl;
  }

  auto crypto_ctx = ngtcp2_conn_get_crypto_ctx(conn_);
  auto aead = &crypto_ctx->aead;
  auto keylen = ngtcp2_crypto_aead_keylen(aead);
  auto ivlen = ngtcp2_crypto_packet_protection_ivlen(aead);

  ++nkey_update_;

  std::array<uint8_t, 64> rx_key, tx_key;

  if (ngtcp2_crypto_update_key(conn_, rx_secret, tx_secret, rx_aead_ctx,
                               rx_key.data(), rx_iv, tx_aead_ctx, tx_key.data(),
                               tx_iv, current_rx_secret, current_tx_secret,
                               secretlen) != 0) {
    return -1;
  }

  if (!config.quiet && config.show_secret) {
    std::cerr << "application_traffic rx secret " << nkey_update_ << std::endl;
    debug::print_secrets({rx_secret, secretlen}, {rx_key.data(), keylen},
                         {rx_iv, ivlen});
    std::cerr << "application_traffic tx secret " << nkey_update_ << std::endl;
    debug::print_secrets({tx_secret, secretlen}, {tx_key.data(), keylen},
                         {tx_iv, ivlen});
  }

  return 0;
}

int Client::initiate_key_update() {
  if (!config.quiet) {
    std::cerr << "Initiate key update" << std::endl;
  }

  if (auto rv = ngtcp2_conn_initiate_key_update(conn_, util::timestamp());
      rv != 0) {
    std::cerr << "ngtcp2_conn_initiate_key_update: " << ngtcp2_strerror(rv)
              << std::endl;
    return -1;
  }

  return 0;
}

void Client::start_delay_stream_timer() {
  ev_timer_start(loop_, &delay_stream_timer_);
}

int Client::send_packet(const Endpoint &ep, const ngtcp2_addr &remote_addr,
                        unsigned int ecn, std::span<const uint8_t> data) {
  auto [_, rv] = send_packet(ep, remote_addr, ecn, data, data.size());

  return rv;
}

std::pair<std::span<const uint8_t>, int>
Client::send_packet(const Endpoint &ep, const ngtcp2_addr &remote_addr,
                    unsigned int ecn, std::span<const uint8_t> data,
                    size_t gso_size) {
  assert(gso_size);

  if (debug::packet_lost(config.tx_loss_prob)) {
    if (!config.quiet) {
      std::cerr << "** Simulated outgoing packet loss **" << std::endl;
    }
    return {{}, NETWORK_ERR_OK};
  }

  if (no_gso_ && data.size() > gso_size) {
    for (; !data.empty();) {
      auto len = std::min(gso_size, data.size());

      auto [_, rv] = send_packet(ep, remote_addr, ecn, data.first(len), len);
      if (rv != 0) {
        return {data, rv};
      }

      data = data.subspan(len);
    }

    return {{}, 0};
  }

  iovec msg_iov{
    .iov_base = const_cast<uint8_t *>(data.data()),
    .iov_len = data.size(),
  };

  uint8_t msg_ctrl[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(uint16_t))]{};

  msghdr msg{
#ifdef HAVE_LINUX_RTNETLINK_H
    .msg_name = const_cast<sockaddr *>(remote_addr.addr),
    .msg_namelen = remote_addr.addrlen,
#endif // defined(HAVE_LINUX_RTNETLINK_H)
    .msg_iov = &msg_iov,
    .msg_iovlen = 1,
    .msg_control = msg_ctrl,
    .msg_controllen = sizeof(msg_ctrl),
  };

  size_t controllen = 0;

  auto cm = CMSG_FIRSTHDR(&msg);
  controllen += CMSG_SPACE(sizeof(int));
  cm->cmsg_len = CMSG_LEN(sizeof(int));
  memcpy(CMSG_DATA(cm), &ecn, sizeof(ecn));

  switch (remote_addr.addr->sa_family) {
  case AF_INET:
    cm->cmsg_level = IPPROTO_IP;
    cm->cmsg_type = IP_TOS;

    break;
  case AF_INET6:
    cm->cmsg_level = IPPROTO_IPV6;
    cm->cmsg_type = IPV6_TCLASS;

    break;
  default:
    assert(0);
  }

#ifdef UDP_SEGMENT
  if (data.size() > gso_size) {
    controllen += CMSG_SPACE(sizeof(uint16_t));
    cm = CMSG_NXTHDR(&msg, cm);
    cm->cmsg_level = SOL_UDP;
    cm->cmsg_type = UDP_SEGMENT;
    cm->cmsg_len = CMSG_LEN(sizeof(uint16_t));
    auto n = static_cast<uint16_t>(gso_size);
    memcpy(CMSG_DATA(cm), &n, sizeof(n));
  }
#endif // defined(UDP_SEGMENT)

  msg.msg_controllen =
#ifndef __APPLE__
    controllen
#else  // defined(__APPLE__)
    static_cast<socklen_t>(controllen)
#endif // defined(__APPLE__)
    ;

  ssize_t nwrite = 0;

  do {
    nwrite = sendmsg(ep.fd, &msg, 0);
  } while (nwrite == -1 && errno == EINTR);

  if (nwrite == -1) {
    switch (errno) {
    case EAGAIN:
#if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#endif // EAGAIN != EWOULDBLOCK
      return {data, NETWORK_ERR_SEND_BLOCKED};
#ifdef UDP_SEGMENT
    case EIO:
      if (data.size() > gso_size) {
        // GSO failure; send each packet in a separate sendmsg call.
        std::cerr << "sendmsg: disabling GSO due to " << strerror(errno)
                  << std::endl;

        no_gso_ = true;

        return send_packet(ep, remote_addr, ecn, data, gso_size);
      }
      break;
#endif // defined(UDP_SEGMENT)
    }

    std::cerr << "sendmsg: " << strerror(errno) << std::endl;

    // TODO We have packet which is expected to fail to send (e.g.,
    // path validation to old path).
    return {{}, NETWORK_ERR_OK};
  }

  assert(static_cast<size_t>(nwrite) == data.size());

  if (!config.quiet) {
    std::cerr << "Sent packet: local="
              << util::straddr(&ep.addr.su.sa, ep.addr.len) << " remote="
              << util::straddr(remote_addr.addr, remote_addr.addrlen)
              << " ecn=0x" << std::hex << ecn << std::dec << " " << nwrite
              << " bytes" << std::endl;
  }

  return {{}, NETWORK_ERR_OK};
}

void Client::on_send_blocked(const ngtcp2_path &path, unsigned int ecn,
                             std::span<const uint8_t> data, size_t gso_size) {
  assert(!tx_.send_blocked);
  assert(gso_size);

  tx_.send_blocked = true;

  auto &p = tx_.blocked;

  memcpy(&p.remote_addr.su, path.remote.addr, path.remote.addrlen);

  auto &ep = *static_cast<Endpoint *>(path.user_data);

  p.remote_addr.len = path.remote.addrlen;
  p.endpoint = &ep;
  p.ecn = ecn;
  p.data = data;
  p.gso_size = gso_size;

  start_wev_endpoint(ep);
}

void Client::start_wev_endpoint(const Endpoint &ep) {
  // We do not close ep.fd, so we can expect that each Endpoint has
  // unique fd.
  if (ep.fd != wev_.fd) {
    if (ev_is_active(&wev_)) {
      ev_io_stop(loop_, &wev_);
    }

    ev_io_set(&wev_, ep.fd, EV_WRITE);
  }

  ev_io_start(loop_, &wev_);
}

int Client::send_blocked_packet() {
  assert(tx_.send_blocked);

  auto &p = tx_.blocked;

  ngtcp2_addr remote_addr{
    .addr = &p.remote_addr.su.sa,
    .addrlen = p.remote_addr.len,
  };

  auto [rest, rv] =
    send_packet(*p.endpoint, remote_addr, p.ecn, p.data, p.gso_size);
  if (rv != 0) {
    assert(NETWORK_ERR_SEND_BLOCKED == rv);

    p.data = rest;

    start_wev_endpoint(*p.endpoint);

    return 0;
  }

  tx_.send_blocked = false;

  return 0;
}

int Client::handle_error() {
  if (!conn_ || ngtcp2_conn_in_closing_period(conn_) ||
      ngtcp2_conn_in_draining_period(conn_)) {
    return 0;
  }

  std::array<uint8_t, NGTCP2_MAX_UDP_PAYLOAD_SIZE> buf;

  ngtcp2_path_storage ps;

  ngtcp2_path_storage_zero(&ps);

  ngtcp2_pkt_info pi;

  auto nwrite = ngtcp2_conn_write_connection_close(
    conn_, &ps.path, &pi, buf.data(), buf.size(), &last_error_,
    util::timestamp());
  if (nwrite < 0) {
    std::cerr << "ngtcp2_conn_write_connection_close: "
              << ngtcp2_strerror(static_cast<int>(nwrite)) << std::endl;
    return -1;
  }

  if (nwrite == 0) {
    return 0;
  }

  return send_packet(*static_cast<Endpoint *>(ps.path.user_data),
                     ps.path.remote, pi.ecn,
                     {buf.data(), static_cast<size_t>(nwrite)});
}

int Client::on_stream_close(int64_t stream_id, uint64_t app_error_code) {
  auto it = streams_.find(stream_id);
  assert(it != std::ranges::end(streams_));
  auto &stream = (*it).second;

  sendq_.erase(stream.get());

  ++nstreams_closed_;

  if (!ngtcp2_is_bidi_stream(stream_id)) {
    assert(!ngtcp2_conn_is_local_stream(conn_, stream_id));
    ngtcp2_conn_extend_max_streams_uni(conn_, 1);
  }

  if (!config.quiet) {
    std::cerr << "HTTP stream " << stream_id << " closed with error code "
              << app_error_code << std::endl;
  }
  streams_.erase(it);

  return 0;
}

int Client::make_stream_early() { return on_extend_max_streams(); }

int Client::on_extend_max_streams() {
  int64_t stream_id;

  if ((config.delay_stream && !handshake_confirmed_) ||
      ev_is_active(&delay_stream_timer_)) {
    return 0;
  }

  for (; nstreams_done_ < config.nstreams; ++nstreams_done_) {
    if (auto rv = ngtcp2_conn_open_bidi_stream(conn_, &stream_id, nullptr);
        rv != 0) {
      assert(NGTCP2_ERR_STREAM_ID_BLOCKED == rv);
      break;
    }

    auto stream = std::make_unique<Stream>(
      config.requests[nstreams_done_ % config.requests.size()], stream_id);

    if (submit_http_request(stream.get()) != 0) {
      break;
    }

    if (!config.download.empty()) {
      stream->open_file(stream->req.path);
    }
    streams_.emplace(stream_id, std::move(stream));
  }
  return 0;
}

int Client::submit_http_request(Stream *stream) {
  const auto &req = stream->req;

  stream->rawreqbuf = config.http_method;
  stream->rawreqbuf += ' ';
  stream->rawreqbuf += req.path;
  stream->rawreqbuf += "\r\n";

  nghttp3_buf_init(&stream->reqbuf);
  stream->reqbuf.begin = reinterpret_cast<uint8_t *>(stream->rawreqbuf.data());
  stream->reqbuf.pos = stream->reqbuf.begin;
  stream->reqbuf.end = stream->reqbuf.last =
    stream->reqbuf.begin + stream->rawreqbuf.size();

  if (!config.quiet) {
    auto nva = std::to_array({
      util::make_nv_nn(":method", config.http_method),
      util::make_nv_nn(":path", req.path),
    });
    debug::print_http_request_headers(stream->stream_id, nva.data(),
                                      nva.size());
  }

  sendq_.emplace(stream);

  return 0;
}

int Client::recv_stream_data(uint32_t flags, int64_t stream_id,
                             std::span<const uint8_t> data) {
  auto it = streams_.find(stream_id);
  assert(it != std::ranges::end(streams_));
  auto &stream = (*it).second;

  ngtcp2_conn_extend_max_stream_offset(conn_, stream_id, data.size());
  ngtcp2_conn_extend_max_offset(conn_, data.size());

  if (stream->fd == -1) {
    return 0;
  }

  ssize_t nwrite;
  do {
    nwrite = write(stream->fd, data.data(), data.size());
  } while (nwrite == -1 && errno == EINTR);

  return 0;
}

int Client::acked_stream_data_offset(int64_t stream_id, uint64_t offset,
                                     uint64_t datalen) {
  auto it = streams_.find(stream_id);
  assert(it != std::ranges::end(streams_));
  auto &stream = (*it).second;
  (void)stream;
  assert(static_cast<uint64_t>(stream->reqbuf.end - stream->reqbuf.begin) >=
         offset + datalen);
  return 0;
}

int Client::select_preferred_address(Address &selected_addr,
                                     const ngtcp2_preferred_addr *paddr) {
  auto path = ngtcp2_conn_get_path(conn_);

  switch (path->local.addr->sa_family) {
  case AF_INET:
    if (!paddr->ipv4_present) {
      return -1;
    }
    selected_addr.su.in = paddr->ipv4;
    selected_addr.len = sizeof(paddr->ipv4);
    break;
  case AF_INET6:
    if (!paddr->ipv6_present) {
      return -1;
    }
    selected_addr.su.in6 = paddr->ipv6;
    selected_addr.len = sizeof(paddr->ipv6);
    break;
  default:
    return -1;
  }

  if (!config.quiet) {
    char host[NI_MAXHOST], service[NI_MAXSERV];
    if (auto rv = getnameinfo(&selected_addr.su.sa, selected_addr.len, host,
                              sizeof(host), service, sizeof(service),
                              NI_NUMERICHOST | NI_NUMERICSERV);
        rv != 0) {
      std::cerr << "getnameinfo: " << gai_strerror(rv) << std::endl;
      return -1;
    }

    std::cerr << "selected server preferred_address is [" << host
              << "]:" << service << std::endl;
  }

  return 0;
}

const std::vector<uint32_t> &Client::get_offered_versions() const {
  return offered_versions_;
}

namespace {
int run(Client &c, const char *addr, const char *port,
        TLSClientContext &tls_ctx) {
  Address remote_addr, local_addr;

  auto fd = create_sock(remote_addr, addr, port);
  if (fd == -1) {
    return -1;
  }

#ifdef HAVE_LINUX_RTNETLINK_H
  in_addr_union iau;

  if (get_local_addr(iau, remote_addr) != 0) {
    std::cerr << "Could not get local address" << std::endl;
    close(fd);
    return -1;
  }

  if (bind_addr(local_addr, fd, &iau, remote_addr.su.sa.sa_family) != 0) {
    close(fd);
    return -1;
  }
#else  // !defined(HAVE_LINUX_RTNETLINK_H)
  if (connect_sock(local_addr, fd, remote_addr) != 0) {
    close(fd);
    return -1;
  }
#endif // !defined(HAVE_LINUX_RTNETLINK_H)

  if (c.init(fd, local_addr, remote_addr, addr, port, tls_ctx) != 0) {
    return -1;
  }

  // TODO Do we need this ?
  if (auto rv = c.on_write(); rv != 0) {
    return rv;
  }

  ev_run(EV_DEFAULT, 0);

  return 0;
}
} // namespace

namespace {
int parse_uri(Request &req, const std::string_view &uri) {
  urlparse_url u;

  if (urlparse_parse_url(uri.data(), uri.size(), /* is_connect = */ 0, &u) !=
      0) {
    return -1;
  }

  if (!(u.field_set & (1 << URLPARSE_SCHEMA)) ||
      !(u.field_set & (1 << URLPARSE_HOST))) {
    return -1;
  }

  req.scheme = util::get_string(uri, u, URLPARSE_SCHEMA);

  req.authority = util::get_string(uri, u, URLPARSE_HOST);
  if (util::numeric_host(req.authority.c_str(), AF_INET6)) {
    req.authority = '[' + req.authority + ']';
  }
  if (u.field_set & (1 << URLPARSE_PORT)) {
    req.authority += ':';
    req.authority += util::get_string(uri, u, URLPARSE_PORT);
  }

  if (u.field_set & (1 << URLPARSE_PATH)) {
    req.path = util::get_string(uri, u, URLPARSE_PATH);
  } else {
    req.path = "/";
  }

  if (u.field_set & (1 << URLPARSE_QUERY)) {
    req.path += '?';
    req.path += util::get_string(uri, u, URLPARSE_QUERY);
  }

  return 0;
}
} // namespace

namespace {
int parse_requests(char **argv, size_t argvlen) {
  for (size_t i = 0; i < argvlen; ++i) {
    auto uri = std::string_view{argv[i]};
    Request req;
    if (parse_uri(req, uri) != 0) {
      std::cerr << "Could not parse URI: " << uri << std::endl;
      return -1;
    }
    config.requests.emplace_back(std::move(req));
  }
  return 0;
}
} // namespace

std::ofstream keylog_file;

namespace {
const char *prog = "h09client";
} // namespace

namespace {
void print_usage() {
  std::cerr << "Usage: " << prog << " [OPTIONS] <HOST> <PORT> [<URI>...]"
            << std::endl;
}
} // namespace

namespace {
void config_set_default(Config &config) {
  config = Config{
    .tx_loss_prob = 0.,
    .rx_loss_prob = 0.,
    .fd = -1,
    .ciphers = util::crypto_default_ciphers(),
    .groups = util::crypto_default_groups(),
    .version = NGTCP2_PROTO_VER_V1,
    .timeout = 30 * NGTCP2_SECONDS,
    .http_method = "GET"sv,
    .max_data = 24_m,
    .max_stream_data_bidi_local = 16_m,
    .max_stream_data_uni = 16_m,
    .max_streams_uni = 100,
    .cc_algo = NGTCP2_CC_ALGO_CUBIC,
    .initial_rtt = NGTCP2_DEFAULT_INITIAL_RTT,
    .handshake_timeout = UINT64_MAX,
    .ack_thresh = 2,
    .initial_pkt_num = UINT32_MAX,
  };
}
} // namespace

namespace {
void print_help() {
  print_usage();

  config_set_default(config);

  std::cout << R"(
  <HOST>      Remote server host (DNS name or IP address).  In case of
              DNS name, it will be sent in TLS SNI extension.
  <PORT>      Remote server port
  <URI>       Remote URI
Options:
  -t, --tx-loss=<P>
              The probability of losing outgoing packets.  <P> must be
              [0.0, 1.0],  inclusive.  0.0 means no  packet loss.  1.0
              means 100% packet loss.
  -r, --rx-loss=<P>
              The probability of losing incoming packets.  <P> must be
              [0.0, 1.0],  inclusive.  0.0 means no  packet loss.  1.0
              means 100% packet loss.
  -d, --data=<PATH>
              Read data from <PATH>, and send them as STREAM data.
  -n, --nstreams=<N>
              The number of requests.  <URI>s are used in the order of
              appearance in the command-line.   If the number of <URI>
              list  is  less than  <N>,  <URI>  list is  wrapped.   It
              defaults to 0 which means the number of <URI> specified.
  -v, --version=<HEX>
              Specify QUIC version to use in hex string.  If the given
              version is  not supported by libngtcp2,  client will use
              QUIC v1  long packet  types.  Instead of  specifying hex
              string,  there  are   special  aliases  available:  "v1"
              indicates QUIC v1, and "v2" indicates QUIC v2.
              Default: )"
            << std::hex << "0x" << config.version << std::dec << R"(
  --preferred-versions=<HEX>[[,<HEX>]...]
              Specify  QUIC versions  in hex  string in  the order  of
              preference.   Client chooses  one of  those versions  if
              client received Version  Negotiation packet from server.
              These versions must be  supported by libngtcp2.  Instead
              of  specifying hex  string,  there  are special  aliases
              available: "v1"  indicates QUIC  v1, and  "v2" indicates
              QUIC v2.
  --available-versions=<HEX>[[,<HEX>]...]
              Specify QUIC  versions in  hex string  that are  sent in
              available_versions    field    of    version_information
              transport parameter.   This list  can include  a version
              which  is  not  supported   by  libngtcp2.   Instead  of
              specifying  hex   string,  there  are   special  aliases
              available: "v1"  indicates QUIC  v1, and  "v2" indicates
              QUIC v2.
  -q, --quiet Suppress debug output.
  -s, --show-secret
              Print out secrets unless --quiet is used.
  --timeout=<DURATION>
              Specify idle timeout.
              Default: )"
            << util::format_duration(config.timeout) << R"(
  --ciphers=<CIPHERS>
              Specify the cipher suite list to enable.
              Default: )"
            << config.ciphers << R"(
  --groups=<GROUPS>
              Specify the supported groups.
              Default: )"
            << config.groups << R"(
  --session-file=<PATH>
              Read/write  TLS session  from/to  <PATH>.   To resume  a
              session, the previous session must be supplied with this
              option.
  --tp-file=<PATH>
              Read/write QUIC transport parameters from/to <PATH>.  To
              send 0-RTT data, the  transport parameters received from
              the previous session must be supplied with this option.
  --dcid=<DCID>
              Specify  initial DCID.   <DCID>  is  hex string.   After
              decoded as binary, it should be  at least 8 bytes and at
              most 20 bytes long.
  --change-local-addr=<DURATION>
              Client  changes  local  address when  <DURATION>  elapse
              after handshake completes.
  --nat-rebinding
              When   used  with   --change-local-addr,  simulate   NAT
              rebinding.   In   other  words,  client   changes  local
              address, but it does not start path validation.
  --key-update=<DURATION>
              Client initiates key update when <DURATION> elapse after
              handshake completes.
  -m, --http-method=<METHOD>
              Specify HTTP method.  Default: )"
            << config.http_method << R"(
  --delay-stream=<DURATION>
              Delay sending STREAM data  in 1-RTT for <DURATION> after
              handshake completes.
  --no-preferred-addr
              Do not try to use preferred address offered by server.
  --key=<PATH>
              The path to client private key PEM file.
  --cert=<PATH>
              The path to client certificate PEM file.
  --download=<PATH>
              The path to the directory  to save a downloaded content.
              It is  undefined if 2  concurrent requests write  to the
              same file.   If a request  path does not contain  a path
              component  usable  as  a   file  name,  it  defaults  to
              "index.html".
  --no-quic-dump
              Disables printing QUIC STREAM and CRYPTO frame data out.
  --no-http-dump
              Disables printing HTTP response body out.
  --qlog-file=<PATH>
              The path to write qlog.   This option and --qlog-dir are
              mutually exclusive.
  --qlog-dir=<PATH>
              Path to  the directory where  qlog file is  stored.  The
              file name  of each qlog  is the Source Connection  ID of
              client.   This  option   and  --qlog-file  are  mutually
              exclusive.
  --max-data=<SIZE>
              The initial connection-level flow control window.
              Default: )"
            << util::format_uint_iec(config.max_data) << R"(
  --max-stream-data-bidi-local=<SIZE>
              The  initial  stream-level  flow control  window  for  a
              bidirectional stream that the local endpoint initiates.
              Default: )"
            << util::format_uint_iec(config.max_stream_data_bidi_local) << R"(
  --max-stream-data-bidi-remote=<SIZE>
              The  initial  stream-level  flow control  window  for  a
              bidirectional stream that the remote endpoint initiates.
              Default: )"
            << util::format_uint_iec(config.max_stream_data_bidi_remote) << R"(
  --max-stream-data-uni=<SIZE>
              The  initial  stream-level  flow control  window  for  a
              unidirectional stream.
              Default: )"
            << util::format_uint_iec(config.max_stream_data_uni) << R"(
  --max-streams-bidi=<N>
              The number of the  concurrent bidirectional streams that
              the remote endpoint initiates.
              Default: )"
            << config.max_streams_bidi << R"(
  --max-streams-uni=<N>
              The number of the concurrent unidirectional streams that
              the remote endpoint initiates.
              Default: )"
            << config.max_streams_uni << R"(
  --exit-on-first-stream-close
              Exit  when  a  first  client initiated  HTTP  stream  is
              closed.
  --exit-on-all-streams-close
              Exit when all client initiated HTTP streams are closed.
  --wait-for-ticket
              Wait  for a  ticket  to be  received  before exiting  on
              --exit-on-first-stream-close                          or
              --exit-on-all-streams-close.   --session-file   must  be
              specified.
  --disable-early-data
              Disable early data.
  --cc=(cubic|reno|bbr)
              The name of congestion controller algorithm.
              Default: )"
            << util::strccalgo(config.cc_algo) << R"(
  --token-file=<PATH>
              Read/write token from/to <PATH>.  Token is obtained from
              NEW_TOKEN frame from server.
  --sni=<DNSNAME>
              Send  <DNSNAME>  in TLS  SNI,  overriding  the DNS  name
              specified in <HOST>.
  --initial-rtt=<DURATION>
              Set an initial RTT.
              Default: )"
            << util::format_duration(config.initial_rtt) << R"(
  --max-window=<SIZE>
              Maximum connection-level flow  control window size.  The
              window auto-tuning is enabled if nonzero value is given,
              and window size is scaled up to this value.
              Default: )"
            << util::format_uint_iec(config.max_window) << R"(
  --max-stream-window=<SIZE>
              Maximum stream-level flow control window size.  The
              window auto-tuning is enabled if nonzero value is given,
              and window size is scaled up to this value.
              Default: )"
            << util::format_uint_iec(config.max_stream_window) << R"(
  --max-udp-payload-size=<SIZE>
              Override maximum UDP payload size that client transmits.
              With this  option, client  assumes that a  path supports
              <SIZE> byte of UDP  datagram payload, without performing
              Path MTU Discovery.
  --handshake-timeout=<DURATION>
              Set  the  QUIC handshake  timeout.   It  defaults to  no
              timeout.
  --no-pmtud  Disables Path MTU Discovery.
  --ack-thresh=<N>
              The minimum number of the received ACK eliciting packets
              that triggers immediate acknowledgement.
              Default: )"
            << config.ack_thresh << R"(
  --initial-pkt-num=<N>
              The initial packet  number that is used  for each packet
              number space.  It  must be in range [0, (1  << 31) - 1],
              inclusive.   By default,  the initial  packet number  is
              chosen randomly.
  --pmtud-probes=<SIZE>[[,<SIZE>]...]
              Specify UDP datagram payload sizes  to probe in Path MTU
              Discovery.  <SIZE> must be strictly larger than 1200.
  --ech-config-list-file=<PATH>
              Read/write  ECHConfigList from/to  <PATH>.  ECH  is only
              attempted if  an underlying  TLS stack supports  it.  If
              the handshake  fails with ech_required alert,  ECH retry
              configs,  if  provided by  server,  will  be written  to
              <PATH>.
  -h, --help  Display this help and exit.

---

  The <SIZE> argument is an integer and an optional unit (e.g., 10K is
  10 * 1024).  Units are K, M and G (powers of 1024).

  The <DURATION> argument is an integer and an optional unit (e.g., 1s
  is 1 second and 500ms is 500  milliseconds).  Units are h, m, s, ms,
  us, or ns (hours,  minutes, seconds, milliseconds, microseconds, and
  nanoseconds respectively).  If  a unit is omitted, a  second is used
  as unit.

  The  <HEX> argument  is an  hex string  which must  start with  "0x"
  (e.g., 0x00000001).)"
            << std::endl;
}
} // namespace

int main(int argc, char **argv) {
  config_set_default(config);
  char *data_path = nullptr;
  const char *private_key_file = nullptr;
  const char *cert_file = nullptr;

  if (argc) {
    prog = basename(argv[0]);
  }

  for (;;) {
    static int flag = 0;
    constexpr static option long_opts[] = {
      {"help", no_argument, nullptr, 'h'},
      {"tx-loss", required_argument, nullptr, 't'},
      {"rx-loss", required_argument, nullptr, 'r'},
      {"data", required_argument, nullptr, 'd'},
      {"http-method", required_argument, nullptr, 'm'},
      {"nstreams", required_argument, nullptr, 'n'},
      {"version", required_argument, nullptr, 'v'},
      {"quiet", no_argument, nullptr, 'q'},
      {"show-secret", no_argument, nullptr, 's'},
      {"ciphers", required_argument, &flag, 1},
      {"groups", required_argument, &flag, 2},
      {"timeout", required_argument, &flag, 3},
      {"session-file", required_argument, &flag, 4},
      {"tp-file", required_argument, &flag, 5},
      {"dcid", required_argument, &flag, 6},
      {"change-local-addr", required_argument, &flag, 7},
      {"key-update", required_argument, &flag, 8},
      {"nat-rebinding", no_argument, &flag, 9},
      {"delay-stream", required_argument, &flag, 10},
      {"no-preferred-addr", no_argument, &flag, 11},
      {"key", required_argument, &flag, 12},
      {"cert", required_argument, &flag, 13},
      {"download", required_argument, &flag, 14},
      {"no-quic-dump", no_argument, &flag, 15},
      {"no-http-dump", no_argument, &flag, 16},
      {"qlog-file", required_argument, &flag, 17},
      {"max-data", required_argument, &flag, 18},
      {"max-stream-data-bidi-local", required_argument, &flag, 19},
      {"max-stream-data-bidi-remote", required_argument, &flag, 20},
      {"max-stream-data-uni", required_argument, &flag, 21},
      {"max-streams-bidi", required_argument, &flag, 22},
      {"max-streams-uni", required_argument, &flag, 23},
      {"exit-on-first-stream-close", no_argument, &flag, 24},
      {"disable-early-data", no_argument, &flag, 25},
      {"qlog-dir", required_argument, &flag, 26},
      {"cc", required_argument, &flag, 27},
      {"exit-on-all-streams-close", no_argument, &flag, 28},
      {"token-file", required_argument, &flag, 29},
      {"sni", required_argument, &flag, 30},
      {"initial-rtt", required_argument, &flag, 31},
      {"max-window", required_argument, &flag, 32},
      {"max-stream-window", required_argument, &flag, 33},
      {"max-udp-payload-size", required_argument, &flag, 35},
      {"handshake-timeout", required_argument, &flag, 36},
      {"available-versions", required_argument, &flag, 37},
      {"no-pmtud", no_argument, &flag, 38},
      {"preferred-versions", required_argument, &flag, 39},
      {"ack-thresh", required_argument, &flag, 40},
      {"wait-for-ticket", no_argument, &flag, 41},
      {"initial-pkt-num", required_argument, &flag, 42},
      {"pmtud-probes", required_argument, &flag, 43},
      {"ech-config-list-file", required_argument, &flag, 44},
      {},
    };

    auto optidx = 0;
    auto c = getopt_long(argc, argv, "d:him:n:qr:st:v:", long_opts, &optidx);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'd':
      // --data
      data_path = optarg;
      break;
    case 'h':
      // --help
      print_help();
      exit(EXIT_SUCCESS);
    case 'm':
      // --http-method
      config.http_method = optarg;
      break;
    case 'n':
      // --streams
      if (auto n = util::parse_uint(optarg); !n) {
        std::cerr << "streams: invalid argument" << std::endl;
        exit(EXIT_FAILURE);
      } else if (*n > NGTCP2_MAX_VARINT) {
        std::cerr << "streams: must not exceed " << NGTCP2_MAX_VARINT
                  << std::endl;
        exit(EXIT_FAILURE);
      } else {
        config.nstreams = *n;
      }
      break;
    case 'q':
      // --quiet
      config.quiet = true;
      break;
    case 'r':
      // --rx-loss
      config.rx_loss_prob = strtod(optarg, nullptr);
      break;
    case 's':
      // --show-secret
      config.show_secret = true;
      break;
    case 't':
      // --tx-loss
      config.tx_loss_prob = strtod(optarg, nullptr);
      break;
    case 'v': {
      // --version
      if (optarg == "v1"sv) {
        config.version = NGTCP2_PROTO_VER_V1;
        break;
      }
      if (optarg == "v2"sv) {
        config.version = NGTCP2_PROTO_VER_V2;
        break;
      }
      auto rv = util::parse_version(optarg);
      if (!rv) {
        std::cerr << "version: invalid version " << std::quoted(optarg)
                  << std::endl;
        exit(EXIT_FAILURE);
      }
      config.version = *rv;
      break;
    }
    case '?':
      print_usage();
      exit(EXIT_FAILURE);
    case 0:
      switch (flag) {
      case 1:
        // --ciphers
        if (util::crypto_default_ciphers()[0] == '\0') {
          std::cerr << "ciphers: not supported" << std::endl;
          exit(EXIT_FAILURE);
        }
        config.ciphers = optarg;
        break;
      case 2:
        // --groups
        config.groups = optarg;
        break;
      case 3:
        // --timeout
        if (auto t = util::parse_duration(optarg); !t) {
          std::cerr << "timeout: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.timeout = *t;
        }
        break;
      case 4:
        // --session-file
        config.session_file = optarg;
        break;
      case 5:
        // --tp-file
        config.tp_file = optarg;
        break;
      case 6: {
        // --dcid
        auto hexcid = std::string_view{optarg};
        if (hexcid.size() < NGTCP2_MIN_INITIAL_DCIDLEN * 2 ||
            hexcid.size() > NGTCP2_MAX_CIDLEN * 2) {
          std::cerr << "dcid: wrong length" << std::endl;
          exit(EXIT_FAILURE);
        }

        if (!util::is_hex_string(hexcid)) {
          std::cerr << "dcid: not hex string" << std::endl;
          exit(EXIT_FAILURE);
        }

        auto dcid = util::decode_hex(hexcid);
        ngtcp2_cid_init(&config.dcid,
                        reinterpret_cast<const uint8_t *>(dcid.c_str()),
                        dcid.size());
        break;
      }
      case 7:
        // --change-local-addr
        if (auto t = util::parse_duration(optarg); !t) {
          std::cerr << "change-local-addr: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.change_local_addr = *t;
        }
        break;
      case 8:
        // --key-update
        if (auto t = util::parse_duration(optarg); !t) {
          std::cerr << "key-update: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.key_update = *t;
        }
        break;
      case 9:
        // --nat-rebinding
        config.nat_rebinding = true;
        break;
      case 10:
        // --delay-stream
        if (auto t = util::parse_duration(optarg); !t) {
          std::cerr << "delay-stream: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.delay_stream = *t;
        }
        break;
      case 11:
        // --no-preferred-addr
        config.no_preferred_addr = true;
        break;
      case 12:
        // --key
        private_key_file = optarg;
        break;
      case 13:
        // --cert
        cert_file = optarg;
        break;
      case 14:
        // --download
        config.download = optarg;
        break;
      case 15:
        // --no-quic-dump
        config.no_quic_dump = true;
        break;
      case 16:
        // --no-http-dump
        config.no_http_dump = true;
        break;
      case 17:
        // --qlog-file
        config.qlog_file = optarg;
        break;
      case 18:
        // --max-data
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-data: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_data = *n;
        }
        break;
      case 19:
        // --max-stream-data-bidi-local
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-stream-data-bidi-local: invalid argument"
                    << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_data_bidi_local = *n;
        }
        break;
      case 20:
        // --max-stream-data-bidi-remote
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-stream-data-bidi-remote: invalid argument"
                    << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_data_bidi_remote = *n;
        }
        break;
      case 21:
        // --max-stream-data-uni
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-stream-data-uni: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_data_uni = *n;
        }
        break;
      case 22:
        // --max-streams-bidi
        if (auto n = util::parse_uint(optarg); !n) {
          std::cerr << "max-streams-bidi: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_streams_bidi = *n;
        }
        break;
      case 23:
        // --max-streams-uni
        if (auto n = util::parse_uint(optarg); !n) {
          std::cerr << "max-streams-uni: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_streams_uni = *n;
        }
        break;
      case 24:
        // --exit-on-first-stream-close
        config.exit_on_first_stream_close = true;
        break;
      case 25:
        // --disable-early-data
        config.disable_early_data = true;
        break;
      case 26:
        // --qlog-dir
        config.qlog_dir = optarg;
        break;
      case 27:
        // --cc
        if (strcmp("cubic", optarg) == 0) {
          config.cc_algo = NGTCP2_CC_ALGO_CUBIC;
          break;
        }
        if (strcmp("reno", optarg) == 0) {
          config.cc_algo = NGTCP2_CC_ALGO_RENO;
          break;
        }
        if (strcmp("bbr", optarg) == 0) {
          config.cc_algo = NGTCP2_CC_ALGO_BBR;
          break;
        }
        std::cerr << "cc: specify cubic, reno, or bbr" << std::endl;
        exit(EXIT_FAILURE);
      case 28:
        // --exit-on-all-streams-close
        config.exit_on_all_streams_close = true;
        break;
      case 29:
        // --token-file
        config.token_file = optarg;
        break;
      case 30:
        // --sni
        config.sni = optarg;
        break;
      case 31:
        // --initial-rtt
        if (auto t = util::parse_duration(optarg); !t) {
          std::cerr << "initial-rtt: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.initial_rtt = *t;
        }
        break;
      case 32:
        // --max-window
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-window: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_window = *n;
        }
        break;
      case 33:
        // --max-stream-window
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-stream-window: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_window = *n;
        }
        break;
      case 35:
        // --max-udp-payload-size
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-udp-payload-size: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else if (*n > 64_k) {
          std::cerr << "max-udp-payload-size: must not exceed 65536"
                    << std::endl;
          exit(EXIT_FAILURE);
        } else if (*n == 0) {
          std::cerr << "max-udp-payload-size: must not be 0" << std::endl;
        } else {
          config.max_udp_payload_size = *n;
        }
        break;
      case 36:
        // --handshake-timeout
        if (auto t = util::parse_duration(optarg); !t) {
          std::cerr << "handshake-timeout: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.handshake_timeout = *t;
        }
        break;
      case 37: {
        // --available-versions
        if (strlen(optarg) == 0) {
          config.available_versions.resize(0);
          break;
        }
        auto l = util::split_str(optarg);
        config.available_versions.resize(l.size());
        auto it = std::ranges::begin(config.available_versions);
        for (const auto &k : l) {
          if (k == "v1"sv) {
            *it++ = NGTCP2_PROTO_VER_V1;
            continue;
          }
          if (k == "v2"sv) {
            *it++ = NGTCP2_PROTO_VER_V2;
            continue;
          }
          auto rv = util::parse_version(k);
          if (!rv) {
            std::cerr << "available-versions: invalid version "
                      << std::quoted(k) << std::endl;
            exit(EXIT_FAILURE);
          }
          *it++ = *rv;
        }
        break;
      }
      case 38:
        // --no-pmtud
        config.no_pmtud = true;
        break;
      case 39: {
        // --preferred-versions
        auto l = util::split_str(optarg);
        if (l.size() > max_preferred_versionslen) {
          std::cerr << "preferred-versions: too many versions > "
                    << max_preferred_versionslen << std::endl;
        }
        config.preferred_versions.resize(l.size());
        auto it = std::ranges::begin(config.preferred_versions);
        for (const auto &k : l) {
          if (k == "v1"sv) {
            *it++ = NGTCP2_PROTO_VER_V1;
            continue;
          }
          if (k == "v2"sv) {
            *it++ = NGTCP2_PROTO_VER_V2;
            continue;
          }
          auto rv = util::parse_version(k);
          if (!rv) {
            std::cerr << "preferred-versions: invalid version "
                      << std::quoted(k) << std::endl;
            exit(EXIT_FAILURE);
          }
          if (!ngtcp2_is_supported_version(*rv)) {
            std::cerr << "preferred-versions: unsupported version "
                      << std::quoted(k) << std::endl;
            exit(EXIT_FAILURE);
          }
          *it++ = *rv;
        }
        break;
      }
      case 40:
        // --ack-thresh
        if (auto n = util::parse_uint(optarg); !n) {
          std::cerr << "ack-thresh: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else if (*n > 100) {
          std::cerr << "ack-thresh: must not exceed 100" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.ack_thresh = *n;
        }
        break;
      case 41:
        // --wait-for-ticket
        config.wait_for_ticket = true;
        break;
      case 42:
        // --initial-pkt-num
        if (auto n = util::parse_uint(optarg); !n) {
          std::cerr << "initial-pkt-num: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else if (*n > INT32_MAX) {
          std::cerr << "initial-pkt-num: must not exceed (1 << 31) - 1"
                    << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.initial_pkt_num = static_cast<uint32_t>(*n);
        }
        break;
      case 43: {
        // --pmtud-probes
        auto l = util::split_str(optarg);
        for (auto &s : l) {
          if (auto n = util::parse_uint_iec(s); !n) {
            std::cerr << "pmtud-probes: invalid argument" << std::endl;
            exit(EXIT_FAILURE);
          } else if (*n <= 1200 || *n >= 64_k) {
            std::cerr
              << "pmtud-probes: must be in range [1201, 65535], inclusive."
              << std::endl;
            exit(EXIT_FAILURE);
          } else {
            config.pmtud_probes.push_back(static_cast<uint16_t>(*n));
          }
        }
        break;
      }
      case 44:
        // --ech-config-list-file
        config.ech_config_list_file = optarg;
        break;
      }
      break;
    default:
      break;
    };
  }

  if (argc - optind < 2) {
    std::cerr << "Too few arguments" << std::endl;
    print_usage();
    exit(EXIT_FAILURE);
  }

  if (!config.qlog_file.empty() && !config.qlog_dir.empty()) {
    std::cerr << "qlog-file and qlog-dir are mutually exclusive" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.exit_on_first_stream_close && config.exit_on_all_streams_close) {
    std::cerr << "exit-on-first-stream-close and exit-on-all-streams-close are "
                 "mutually exclusive"
              << std::endl;
    exit(EXIT_FAILURE);
  }

  if (config.wait_for_ticket && !config.session_file) {
    std::cerr << "wait-for-ticket: session-file must be specified" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (data_path) {
    auto fd = open(data_path, O_RDONLY);
    if (fd == -1) {
      std::cerr << "data: Could not open file " << data_path << ": "
                << strerror(errno) << std::endl;
      exit(EXIT_FAILURE);
    }
    struct stat st;
    if (fstat(fd, &st) != 0) {
      std::cerr << "data: Could not stat file " << data_path << ": "
                << strerror(errno) << std::endl;
      exit(EXIT_FAILURE);
    }
    config.fd = fd;
    config.datalen = static_cast<size_t>(st.st_size);
    if (config.datalen) {
      auto addr = mmap(nullptr, config.datalen, PROT_READ, MAP_SHARED, fd, 0);
      if (addr == MAP_FAILED) {
        std::cerr << "data: Could not mmap file " << data_path << ": "
                  << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
      }
      config.data = static_cast<uint8_t *>(addr);
    }
  }

  if (config.ech_config_list_file) {
    auto ech_config = util::read_file(config.ech_config_list_file);
    if (!ech_config) {
      std::cerr << "ech-config-list-file: Could not read ECHConfigList"
                << std::endl;
    } else {
      config.ech_config_list = std::move(*ech_config);
    }
  }

  auto addr = argv[optind++];
  auto port = argv[optind++];

  if (parse_requests(&argv[optind], static_cast<size_t>(argc - optind)) != 0) {
    exit(EXIT_FAILURE);
  }

  if (!ngtcp2_is_reserved_version(config.version)) {
    if (!config.preferred_versions.empty() &&
        std::ranges::find(config.preferred_versions, config.version) ==
          std::ranges::end(config.preferred_versions)) {
      std::cerr << "preferred-version: must include version " << std::hex
                << "0x" << config.version << std::dec << std::endl;
      exit(EXIT_FAILURE);
    }

    if (!config.available_versions.empty() &&
        std::ranges::find(config.available_versions, config.version) ==
          std::ranges::end(config.available_versions)) {
      std::cerr << "available-versions: must include version " << std::hex
                << "0x" << config.version << std::dec << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  if (config.nstreams == 0) {
    config.nstreams = config.requests.size();
  }

  TLSClientContext tls_ctx;
  if (tls_ctx.init(private_key_file, cert_file) != 0) {
    exit(EXIT_FAILURE);
  }

  auto ev_loop_d = defer(ev_loop_destroy, EV_DEFAULT);

  auto keylog_filename = getenv("SSLKEYLOGFILE");
  if (keylog_filename) {
    keylog_file.open(keylog_filename, std::ios_base::app);
    if (keylog_file) {
      tls_ctx.enable_keylog();
    }
  }

  if (util::generate_secret(config.static_secret) != 0) {
    std::cerr << "Unable to generate static secret" << std::endl;
    exit(EXIT_FAILURE);
  }

  auto client_chosen_version = config.version;

  for (;;) {
    Client c(EV_DEFAULT, client_chosen_version, config.version);

    if (run(c, addr, port, tls_ctx) != 0) {
      exit(EXIT_FAILURE);
    }

    if (config.preferred_versions.empty()) {
      break;
    }

    auto &offered_versions = c.get_offered_versions();
    if (offered_versions.empty()) {
      break;
    }

    client_chosen_version = ngtcp2_select_version(
      config.preferred_versions.data(), config.preferred_versions.size(),
      offered_versions.data(), offered_versions.size());

    if (client_chosen_version == 0) {
      std::cerr << "Unable to select a version" << std::endl;
      exit(EXIT_FAILURE);
    }

    if (!config.quiet) {
      std::cerr << "Client selected version " << std::hex << "0x"
                << client_chosen_version << std::dec << std::endl;
    }
  }

  return EXIT_SUCCESS;
}
