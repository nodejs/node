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
#ifndef H09SERVER_H
#define H09SERVER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <vector>
#include <unordered_map>
#include <string>
#include <deque>
#include <string_view>
#include <memory>
#include <set>
#include <span>

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <nghttp3/nghttp3.h>

#include <ev.h>

#include "server_base.h"
#include "tls_server_context.h"
#include "network.h"
#include "shared.h"
#include "util.h"

using namespace ngtcp2;

struct HTTPHeader {
  HTTPHeader(const std::string_view &name, const std::string_view &value)
    : name(name), value(value) {}

  std::string_view name;
  std::string_view value;
};

class Handler;
struct FileEntry;

struct Stream {
  Stream(int64_t stream_id, Handler *handler);

  int start_response();
  std::pair<FileEntry, int> open_file(const std::string &path);
  void map_file(const FileEntry &fe);
  int send_status_response(unsigned int status_code);

  int64_t stream_id;
  Handler *handler;
  // uri is request uri/path.
  std::string uri;
  std::string status_resp_body;
  nghttp3_buf respbuf;
  http_parser htp;
  // eos gets true when one HTTP request message is seen.
  bool eos;
};

struct StreamIDLess {
  constexpr bool operator()(const Stream *lhs, const Stream *rhs) const {
    return lhs->stream_id < rhs->stream_id;
  }
};

class Server;

// Endpoint is a local endpoint.
struct Endpoint {
  Address addr;
  ev_io rev;
  Server *server;
  int fd;
};

class Handler : public HandlerBase {
public:
  Handler(struct ev_loop *loop, Server *server);
  ~Handler();

  int init(const Endpoint &ep, const Address &local_addr, const sockaddr *sa,
           socklen_t salen, const ngtcp2_cid *dcid, const ngtcp2_cid *scid,
           const ngtcp2_cid *ocid, std::span<const uint8_t> token,
           ngtcp2_token_type token_type, uint32_t version,
           TLSServerContext &tls_ctx);

  int on_read(const Endpoint &ep, const Address &local_addr, const sockaddr *sa,
              socklen_t salen, const ngtcp2_pkt_info *pi,
              std::span<const uint8_t> data);
  int on_write();
  int write_streams();
  int feed_data(const Endpoint &ep, const Address &local_addr,
                const sockaddr *sa, socklen_t salen, const ngtcp2_pkt_info *pi,
                std::span<const uint8_t> data);
  void update_timer();
  int handle_expiry();
  void signal_write();
  int handshake_completed();

  Server *server() const;
  int recv_stream_data(uint32_t flags, int64_t stream_id,
                       std::span<const uint8_t> data);
  int acked_stream_data_offset(int64_t stream_id, uint64_t offset,
                               uint64_t datalen);
  uint32_t version() const;
  void on_stream_open(int64_t stream_id);
  int on_stream_close(int64_t stream_id, uint64_t app_error_code);
  void start_draining_period();
  int start_closing_period();
  int handle_error();
  int send_conn_close();
  int send_conn_close(const Endpoint &ep, const Address &local_addr,
                      const sockaddr *sa, socklen_t salen,
                      const ngtcp2_pkt_info *pi, std::span<const uint8_t> data);

  int update_key(uint8_t *rx_secret, uint8_t *tx_secret,
                 ngtcp2_crypto_aead_ctx *rx_aead_ctx, uint8_t *rx_iv,
                 ngtcp2_crypto_aead_ctx *tx_aead_ctx, uint8_t *tx_iv,
                 const uint8_t *current_rx_secret,
                 const uint8_t *current_tx_secret, size_t secretlen);

  Stream *find_stream(int64_t stream_id);
  int extend_max_stream_data(int64_t stream_id, uint64_t max_data);
  void shutdown_read(int64_t stream_id, uint64_t app_error_code);

  void write_qlog(const void *data, size_t datalen);
  void add_sendq(Stream *stream);

  void on_send_blocked(const ngtcp2_path &path, unsigned int ecn,
                       std::span<const uint8_t> data, size_t gso_size);
  void start_wev_endpoint(const Endpoint &ep);
  int send_packet(const ngtcp2_path &path, unsigned int ecn,
                  std::span<const uint8_t> data, size_t gso_size);
  int send_blocked_packet();

  ngtcp2_ssize write_pkt(ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
                         size_t destlen, ngtcp2_tstamp ts);

private:
  struct ev_loop *loop_;
  Server *server_;
  ev_io wev_;
  ev_timer timer_;
  FILE *qlog_;
  ngtcp2_cid scid_;
  std::unordered_map<int64_t, std::unique_ptr<Stream>> streams_;
  std::set<Stream *, StreamIDLess> sendq_;
  // conn_closebuf_ contains a packet which contains CONNECTION_CLOSE.
  // This packet is repeatedly sent as a response to the incoming
  // packet in draining period.
  std::unique_ptr<Buffer> conn_closebuf_;
  // nkey_update_ is the number of key update occurred.
  size_t nkey_update_;
  bool no_gso_;
  struct {
    size_t bytes_recv;
    size_t bytes_sent;
    size_t num_pkts_recv;
    size_t next_pkts_recv;
  } close_wait_;

  struct {
    bool send_blocked;
    // blocked field is effective only when send_blocked is true.
    struct {
      const Endpoint *endpoint;
      Address local_addr;
      Address remote_addr;
      unsigned int ecn;
      std::span<const uint8_t> data;
      size_t gso_size;
    } blocked;
    std::unique_ptr<uint8_t[]> data;
  } tx_;
};

class Server {
public:
  Server(struct ev_loop *loop, TLSServerContext &tls_ctx);
  ~Server();

  int init(const char *addr, const char *port);
  void disconnect();
  void close();

  int on_read(const Endpoint &ep);
  void read_pkt(const Endpoint &ep, const Address &local_addr,
                const sockaddr *sa, socklen_t salen, const ngtcp2_pkt_info *pi,
                std::span<const uint8_t> data);
  int send_version_negotiation(uint32_t version, std::span<const uint8_t> dcid,
                               std::span<const uint8_t> scid,
                               const Endpoint &ep, const Address &local_addr,
                               const sockaddr *sa, socklen_t salen);
  int send_retry(const ngtcp2_pkt_hd *chd, const Endpoint &ep,
                 const Address &local_addr, const sockaddr *sa, socklen_t salen,
                 size_t max_pktlen);
  int send_stateless_connection_close(const ngtcp2_pkt_hd *chd,
                                      const Endpoint &ep,
                                      const Address &local_addr,
                                      const sockaddr *sa, socklen_t salen);
  int send_stateless_reset(size_t pktlen, std::span<const uint8_t> dcid,
                           const Endpoint &ep, const Address &local_addr,
                           const sockaddr *sa, socklen_t salen);
  int verify_retry_token(ngtcp2_cid *ocid, const ngtcp2_pkt_hd *hd,
                         const sockaddr *sa, socklen_t salen);
  int verify_token(const ngtcp2_pkt_hd *hd, const sockaddr *sa,
                   socklen_t salen);
  int send_packet(const Endpoint &ep, const ngtcp2_addr &local_addr,
                  const ngtcp2_addr &remote_addr, unsigned int ecn,
                  std::span<const uint8_t> data);
  std::pair<std::span<const uint8_t>, int>
  send_packet(const Endpoint &ep, bool &no_gso, const ngtcp2_addr &local_addr,
              const ngtcp2_addr &remote_addr, unsigned int ecn,
              std::span<const uint8_t> data, size_t gso_size);
  void remove(const Handler *h);

  void associate_cid(const ngtcp2_cid *cid, Handler *h);
  void dissociate_cid(const ngtcp2_cid *cid);

  void on_stateless_reset_regen();

private:
  std::unordered_map<ngtcp2_cid, Handler *> handlers_;
  struct ev_loop *loop_;
  std::vector<Endpoint> endpoints_;
  TLSServerContext &tls_ctx_;
  ev_signal sigintev_;
  ev_timer stateless_reset_regen_timer_;
  size_t stateless_reset_bucket_;
};

#endif // !defined(H09SERVER_H)
