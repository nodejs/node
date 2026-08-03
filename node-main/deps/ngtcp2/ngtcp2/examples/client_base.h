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
#ifndef CLIENT_BASE_H
#define CLIENT_BASE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <vector>
#include <deque>
#include <string>
#include <string_view>
#include <functional>

#include <ngtcp2/ngtcp2_crypto.h>

#include "tls_client_session.h"
#include "network.h"
#include "shared.h"

using namespace ngtcp2;

struct Request {
  std::string_view scheme;
  std::string authority;
  std::string path;
};

struct Config {
  ngtcp2_cid dcid;
  ngtcp2_cid scid;
  bool scid_present;
  // tx_loss_prob is probability of losing outgoing packet.
  double tx_loss_prob;
  // rx_loss_prob is probability of losing incoming packet.
  double rx_loss_prob;
  // fd is a file descriptor to read input for streams.
  int fd;
  // ciphers is the list of enabled ciphers.
  const char *ciphers;
  // groups is the list of supported groups.
  const char *groups;
  // nstreams is the number of streams to open.
  size_t nstreams;
  // data is the pointer to memory region which maps file denoted by
  // fd.
  uint8_t *data;
  // datalen is the length of file denoted by fd.
  size_t datalen;
  // version is a QUIC version to use.
  uint32_t version;
  // quiet suppresses the output normally shown except for the error
  // messages.
  bool quiet;
  // timeout is an idle timeout for QUIC connection.
  ngtcp2_duration timeout;
  // session_file is a path to a file to write, and read TLS session.
  const char *session_file;
  // tp_file is a path to a file to write, and read QUIC transport
  // parameters.
  const char *tp_file;
  // show_secret is true if transport secrets should be printed out.
  bool show_secret;
  // change_local_addr is the duration after which client changes
  // local address.
  ngtcp2_duration change_local_addr;
  // key_update is the duration after which client initiates key
  // update.
  ngtcp2_duration key_update;
  // delay_stream is the duration after which client sends the first
  // 1-RTT stream.
  ngtcp2_duration delay_stream;
  // nat_rebinding is true if simulated NAT rebinding is enabled.
  bool nat_rebinding;
  // no_preferred_addr is true if client do not follow preferred
  // address offered by server.
  bool no_preferred_addr;
  std::string_view http_method;
  // download is a path to a directory where a downloaded file is
  // saved.  If it is empty, no file is saved.
  std::string_view download;
  // requests contains URIs to request.
  std::vector<Request> requests;
  // no_quic_dump is true if hexdump of QUIC STREAM and CRYPTO data
  // should be disabled.
  bool no_quic_dump;
  // no_http_dump is true if hexdump of HTTP response body should be
  // disabled.
  bool no_http_dump;
  // qlog_file is the path to write qlog.
  std::string_view qlog_file;
  // qlog_dir is the path to directory where qlog is stored.  qlog_dir
  // and qlog_file are mutually exclusive.
  std::string_view qlog_dir;
  // max_data is the initial connection-level flow control window.
  uint64_t max_data;
  // max_stream_data_bidi_local is the initial stream-level flow
  // control window for a bidirectional stream that the local endpoint
  // initiates.
  uint64_t max_stream_data_bidi_local;
  // max_stream_data_bidi_remote is the initial stream-level flow
  // control window for a bidirectional stream that the remote
  // endpoint initiates.
  uint64_t max_stream_data_bidi_remote;
  // max_stream_data_uni is the initial stream-level flow control
  // window for a unidirectional stream.
  uint64_t max_stream_data_uni;
  // max_streams_bidi is the number of the concurrent bidirectional
  // streams.
  uint64_t max_streams_bidi;
  // max_streams_uni is the number of the concurrent unidirectional
  // streams.
  uint64_t max_streams_uni;
  // max_window is the maximum connection-level flow control window
  // size if auto-tuning is enabled.
  uint64_t max_window;
  // max_stream_window is the maximum stream-level flow control window
  // size if auto-tuning is enabled.
  uint64_t max_stream_window;
  // exit_on_first_stream_close is the flag that if it is true, client
  // exits when a first HTTP stream gets closed.  It is not
  // necessarily the same time when the underlying QUIC stream closes
  // due to the QPACK synchronization.
  bool exit_on_first_stream_close;
  // exit_on_all_streams_close is the flag that if it is true, client
  // exits when all HTTP streams get closed.
  bool exit_on_all_streams_close;
  // disable_early_data disables early data.
  bool disable_early_data;
  // static_secret is used to derive keying materials for Stateless
  // Retry token.
  std::array<uint8_t, 32> static_secret;
  // cc_algo is the congestion controller algorithm.
  ngtcp2_cc_algo cc_algo;
  // token_file is a path to file to read or write token from
  // NEW_TOKEN frame.
  std::string_view token_file;
  // sni is the value sent in TLS SNI, overriding DNS name of the
  // remote host.
  std::string_view sni;
  // initial_rtt is an initial RTT.
  ngtcp2_duration initial_rtt;
  // max_udp_payload_size is the maximum UDP payload size that client
  // transmits.
  size_t max_udp_payload_size;
  // handshake_timeout is the period of time before giving up QUIC
  // connection establishment.
  ngtcp2_duration handshake_timeout;
  // preferred_versions includes QUIC versions in the order of
  // preference.  Client uses this field to select a version from the
  // version set offered in Version Negotiation packet.
  std::vector<uint32_t> preferred_versions;
  // available_versions includes QUIC versions that are sent in
  // available_versions field of version_information
  // transport_parameter.
  std::vector<uint32_t> available_versions;
  // no_pmtud disables Path MTU Discovery.
  bool no_pmtud;
  // ack_thresh is the minimum number of the received ACK eliciting
  // packets that triggers immediate acknowledgement.
  size_t ack_thresh;
  // wait_for_ticket, if true, waits for a ticket to be received
  // before exiting on exit_on_first_stream_close or
  // exit_on_all_streams_close.
  bool wait_for_ticket;
  // initial_pkt_num is the initial packet number for each packet
  // number space.  If it is set to UINT32_MAX, it is chosen randomly.
  uint32_t initial_pkt_num;
  // pmtud_probes is the array of UDP datagram payload size to probes.
  std::vector<uint16_t> pmtud_probes;
  // ech_config_list contains ECHConfigList.
  std::vector<uint8_t> ech_config_list;
  // ech_config_list_file is a path to a file to read and write
  // ECHConfigList.
  const char *ech_config_list_file;
};

class ClientBase {
public:
  ClientBase();
  ~ClientBase();

  ngtcp2_conn *conn() const;

  int write_transport_params(const char *path,
                             const ngtcp2_transport_params *params);
  int read_transport_params(const char *path, ngtcp2_transport_params *params);

  void write_qlog(const void *data, size_t datalen);

  ngtcp2_crypto_conn_ref *conn_ref();

  void ticket_received();

protected:
  ngtcp2_crypto_conn_ref conn_ref_;
  TLSClientSession tls_session_;
  FILE *qlog_;
  ngtcp2_conn *conn_;
  ngtcp2_ccerr last_error_;
  bool ticket_received_;
};

void qlog_write_cb(void *user_data, uint32_t flags, const void *data,
                   size_t datalen);

#endif // !defined(CLIENT_BASE_H)
