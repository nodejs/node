/*
 * ngtcp2
 *
 * Copyright (c) 2020 ngtcp2 contributors
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
#ifndef SERVER_BASE_H
#define SERVER_BASE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <vector>
#include <deque>
#include <unordered_map>
#include <string>
#include <string_view>
#include <functional>
#include <span>

#include <ngtcp2/ngtcp2_crypto.h>

#include "tls_server_session.h"
#include "network.h"
#include "shared.h"
#include "template.h"
#include "util.h"

using namespace ngtcp2;

struct Config {
  Address preferred_ipv4_addr;
  Address preferred_ipv6_addr;
  // tx_loss_prob is probability of losing outgoing packet.
  double tx_loss_prob{};
  // rx_loss_prob is probability of losing incoming packet.
  double rx_loss_prob{};
  // ciphers is the list of enabled ciphers.
  const char *ciphers{util::crypto_default_ciphers()};
  // groups is the list of supported groups.
  const char *groups{util::crypto_default_groups()};
  // htdocs is a root directory to serve documents.
  std::string htdocs{util::realpath(".")};
  // mime_types_file is a path to "MIME media types and the
  // extensions" file.  Ubuntu mime-support package includes it in
  // /etc/mime/types.
  std::string_view mime_types_file{"/etc/mime.types"sv};
  // mime_types maps file extension to MIME media type.
  std::unordered_map<std::string, std::string> mime_types;
  // port is the port number which server listens on for incoming
  // connections.
  uint16_t port{};
  // quiet suppresses the output normally shown except for the error
  // messages.
  bool quiet{};
  // timeout is an idle timeout for QUIC connection.
  ngtcp2_duration timeout{30 * NGTCP2_SECONDS};
  // show_secret is true if transport secrets should be printed out.
  bool show_secret{};
  // validate_addr is true if server requires address validation.
  bool validate_addr{};
  // early_response is true if server starts sending response when it
  // receives HTTP header fields without waiting for request body.  If
  // HTTP response data is written before receiving request body,
  // STOP_SENDING is sent.
  bool early_response{};
  // verify_client is true if server verifies client with X.509
  // certificate based authentication.
  bool verify_client{};
  // qlog_dir is the path to directory where qlog is stored.
  std::string_view qlog_dir;
  // no_quic_dump is true if hexdump of QUIC STREAM and CRYPTO data
  // should be disabled.
  bool no_quic_dump{};
  // no_http_dump is true if hexdump of HTTP response body should be
  // disabled.
  bool no_http_dump{};
  // max_data is the initial connection-level flow control window.
  uint64_t max_data{1_m};
  // max_stream_data_bidi_local is the initial stream-level flow
  // control window for a bidirectional stream that the local endpoint
  // initiates.
  uint64_t max_stream_data_bidi_local{};
  // max_stream_data_bidi_remote is the initial stream-level flow
  // control window for a bidirectional stream that the remote
  // endpoint initiates.
  uint64_t max_stream_data_bidi_remote{256_k};
  // max_stream_data_uni is the initial stream-level flow control
  // window for a unidirectional stream.
  uint64_t max_stream_data_uni{256_k};
  // max_streams_bidi is the number of the concurrent bidirectional
  // streams.
  uint64_t max_streams_bidi{100};
  // max_streams_uni is the number of the concurrent unidirectional
  // streams.
  uint64_t max_streams_uni{3};
  // max_window is the maximum connection-level flow control window
  // size if auto-tuning is enabled.
  uint64_t max_window{6_m};
  // max_stream_window is the maximum stream-level flow control window
  // size if auto-tuning is enabled.
  uint64_t max_stream_window{6_m};
  // max_dyn_length is the maximum length of dynamically generated
  // response.
  uint64_t max_dyn_length{20_m};
  // static_secret is used to derive keying materials for Retry and
  // Stateless Retry token.
  std::array<uint8_t, 32> static_secret;
  // cc_algo is the congestion controller algorithm.
  ngtcp2_cc_algo cc_algo{NGTCP2_CC_ALGO_CUBIC};
  // initial_rtt is an initial RTT.
  ngtcp2_duration initial_rtt{NGTCP2_DEFAULT_INITIAL_RTT};
  // max_udp_payload_size is the maximum UDP payload size that server
  // transmits.
  size_t max_udp_payload_size{};
  // send_trailers controls whether server sends trailer fields or
  // not.
  bool send_trailers{};
  // handshake_timeout is the period of time before giving up QUIC
  // connection establishment.
  ngtcp2_duration handshake_timeout{UINT64_MAX};
  // preferred_versions includes QUIC versions in the order of
  // preference.  Server negotiates one of those versions if a client
  // initially selects a less preferred version.
  std::vector<uint32_t> preferred_versions;
  // available_versions includes QUIC versions that are sent in
  // available_versions field of version_information
  // transport_parameter.
  std::vector<uint32_t> available_versions;
  // no_pmtud disables Path MTU Discovery.
  bool no_pmtud{};
  // ack_thresh is the minimum number of the received ACK eliciting
  // packets that triggers immediate acknowledgement.
  size_t ack_thresh{2};
  // initial_pkt_num is the initial packet number for each packet
  // number space.  If it is set to UINT32_MAX, it is chosen randomly.
  uint32_t initial_pkt_num{UINT32_MAX};
  // pmtud_probes is the array of UDP datagram payload size to probes.
  std::vector<uint16_t> pmtud_probes;
  // ech_config contains server-side ECH configuration.
  util::ECHServerConfig ech_config{};
  // origin_list contains a payload of ORIGIN frame.
  std::optional<std::vector<uint8_t>> origin_list;
  // no_gso disables GSO.
  bool no_gso{};
  // show_stat, if true, displays the connection statistics when the
  // connection is closed.
  bool show_stat{};
  // gso_burst is the number of packets to aggregate in GSO.  0 means
  // it is not limited by the configuration.
  size_t gso_burst{};
};

struct HTTPHeader {
  HTTPHeader(const std::string_view &name, const std::string_view &value)
    : name{name}, value{value} {}

  std::string_view name;
  std::string_view value;
};

inline constexpr size_t NGTCP2_STATELESS_RESET_BURST = 100;

struct Buffer {
  Buffer(const uint8_t *data, size_t datalen);
  explicit Buffer(size_t datalen);

  size_t size() const { return as_unsigned(tail - begin); }
  size_t left() const { return as_unsigned(buf.data() + buf.size() - tail); }
  uint8_t *const wpos() { return tail; }
  std::span<const uint8_t> data() const { return {begin, size()}; }
  void push(size_t len) { tail += len; }
  void reset() { tail = begin; }

  std::vector<uint8_t> buf;
  // begin points to the beginning of the buffer.  This might point to
  // buf.data() if a buffer space is allocated by this object.  It is
  // also allowed to point to the external shared buffer.
  uint8_t *begin;
  // tail points to the position of the buffer where write should
  // occur.
  uint8_t *tail;
};

class HandlerBase {
public:
  HandlerBase();
  ~HandlerBase();

  ngtcp2_conn *conn() const;

  TLSServerSession *get_session() { return &tls_session_; }

  ngtcp2_crypto_conn_ref *conn_ref();

protected:
  ngtcp2_crypto_conn_ref conn_ref_;
  TLSServerSession tls_session_;
  ngtcp2_conn *conn_{};
  ngtcp2_ccerr last_error_;
};

#endif // !defined(SERVER_BASE_H)
