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
#include <chrono>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <memory>
#include <fstream>
#include <iomanip>

#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <netinet/udp.h>
#include <net/if.h>
#include <libgen.h>

#include <urlparse.h>

#include "server.h"
#include "network.h"
#include "debug.h"
#include "util.h"
#include "shared.h"
#include "http.h"
#include "template.h"

using namespace ngtcp2;
using namespace std::literals;

namespace {
constexpr size_t NGTCP2_SV_SCIDLEN = 18;
} // namespace

namespace {
constexpr size_t max_preferred_versionslen = 4;
} // namespace

namespace {
constexpr size_t NGTCP2_STATELESS_RESET_BURST = 100;
} // namespace

namespace {
constexpr size_t NGTCP2_TX_BUFLEN = 64_k;
} // namespace

namespace {
auto randgen = util::make_mt19937();
} // namespace

Config config{};

Stream::Stream(int64_t stream_id, Handler *handler)
  : stream_id(stream_id),
    handler(handler),
    data(nullptr),
    datalen(0),
    dynresp(false),
    dyndataleft(0),
    dynbuflen(0) {}

namespace {
constexpr auto NGTCP2_SERVER = "nghttp3/ngtcp2 server"sv;
} // namespace

namespace {
std::string make_status_body(unsigned int status_code) {
  auto status_string = util::format_uint(status_code);
  auto reason_phrase = http::get_reason_phrase(status_code);

  std::string body;
  body = "<html><head><title>";
  body += status_string;
  body += ' ';
  body += reason_phrase;
  body += "</title></head><body><h1>";
  body += status_string;
  body += ' ';
  body += reason_phrase;
  body += "</h1><hr><address>";
  body += NGTCP2_SERVER;
  body += " at port ";
  body += util::format_uint(config.port);
  body += "</address>";
  body += "</body></html>";
  return body;
}
} // namespace

struct Request {
  std::string path;
  struct {
    int32_t urgency;
    int inc;
  } pri;
};

namespace {
Request request_path(const std::string_view &uri, bool is_connect) {
  urlparse_url u;
  Request req{
    .pri =
      {
        .urgency = -1,
        .inc = -1,
      },
  };

  if (auto rv = urlparse_parse_url(uri.data(), uri.size(), is_connect, &u);
      rv != 0) {
    return req;
  }

  if (u.field_set & (1 << URLPARSE_PATH)) {
    req.path = util::get_string(uri, u, URLPARSE_PATH);
    if (req.path.find('%') != std::string::npos) {
      req.path = util::percent_decode(req.path);
    }
    if (!req.path.empty() && req.path.back() == '/') {
      req.path += "index.html";
    }
  } else {
    req.path = "/index.html";
  }

  req.path = util::normalize_path(req.path);
  if (req.path == "/") {
    req.path = "/index.html";
  }

  if (u.field_set & (1 << URLPARSE_QUERY)) {
    static constexpr auto urgency_prefix = "u="sv;
    static constexpr auto inc_prefix = "i="sv;
    auto q = util::get_string(uri, u, URLPARSE_QUERY);
    for (auto p = std::ranges::begin(q); p != std::ranges::end(q);) {
      if (util::istarts_with(std::string_view{p, std::ranges::end(q)},
                             urgency_prefix)) {
        auto urgency_start = p + urgency_prefix.size();
        auto urgency_end =
          std::ranges::find(urgency_start, std::ranges::end(q), '&');
        if (urgency_start + 1 == urgency_end && '0' <= *urgency_start &&
            *urgency_start <= '7') {
          req.pri.urgency = *urgency_start - '0';
        }
        if (urgency_end == std::ranges::end(q)) {
          break;
        }
        p = urgency_end + 1;
        continue;
      }
      if (util::istarts_with(std::string_view{p, std::ranges::end(q)},
                             inc_prefix)) {
        auto inc_start = p + inc_prefix.size();
        auto inc_end = std::ranges::find(inc_start, std::ranges::end(q), '&');
        if (inc_start + 1 == inc_end &&
            (*inc_start == '0' || *inc_start == '1')) {
          req.pri.inc = *inc_start - '0';
        }
        if (inc_end == std::ranges::end(q)) {
          break;
        }
        p = inc_end + 1;
        continue;
      }

      p = std::ranges::find(p, std::ranges::end(q), '&');
      if (p == std::ranges::end(q)) {
        break;
      }
      ++p;
    }
  }
  return req;
}
} // namespace

enum FileEntryFlag {
  FILE_ENTRY_TYPE_DIR = 0x1,
};

struct FileEntry {
  uint64_t len;
  void *map;
  int fd;
  uint8_t flags;
};

namespace {
std::unordered_map<std::string, FileEntry> file_cache;
} // namespace

std::pair<FileEntry, int> Stream::open_file(const std::string &path) {
  auto it = file_cache.find(path);
  if (it != std::ranges::end(file_cache)) {
    return {(*it).second, 0};
  }

  auto fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    return {{}, -1};
  }

  struct stat st{};
  if (fstat(fd, &st) != 0) {
    close(fd);
    return {{}, -1};
  }

  FileEntry fe{};
  if (st.st_mode & S_IFDIR) {
    fe.flags |= FILE_ENTRY_TYPE_DIR;
    fe.fd = -1;
    close(fd);
  } else {
    fe.fd = fd;
    fe.len = static_cast<size_t>(st.st_size);
    if (fe.len) {
      fe.map = mmap(nullptr, fe.len, PROT_READ, MAP_SHARED, fd, 0);
      if (fe.map == MAP_FAILED) {
        std::cerr << "mmap: " << strerror(errno) << std::endl;
        close(fd);
        return {{}, -1};
      }
    }
  }

  file_cache.emplace(path, fe);

  return {std::move(fe), 0};
}

void Stream::map_file(const FileEntry &fe) {
  data = static_cast<uint8_t *>(fe.map);
  datalen = fe.len;
}

int64_t Stream::find_dyn_length(const std::string_view &path) {
  assert(path[0] == '/');

  if (path.size() == 1) {
    return -1;
  }

  uint64_t n = 0;

  for (auto it = std::ranges::begin(path) + 1; it != std::ranges::end(path);
       ++it) {
    if (*it < '0' || '9' < *it) {
      return -1;
    }
    auto d = static_cast<uint64_t>(*it - '0');
    if (n > (((1ull << 62) - 1) - d) / 10) {
      return -1;
    }
    n = n * 10 + d;
    if (n > config.max_dyn_length) {
      return -1;
    }
  }

  return static_cast<int64_t>(n);
}

namespace {
nghttp3_ssize read_data(nghttp3_conn *conn, int64_t stream_id, nghttp3_vec *vec,
                        size_t veccnt, uint32_t *pflags, void *user_data,
                        void *stream_user_data) {
  auto stream = static_cast<Stream *>(stream_user_data);

  vec[0].base = stream->data;
  vec[0].len = stream->datalen;
  *pflags |= NGHTTP3_DATA_FLAG_EOF;
  if (config.send_trailers) {
    *pflags |= NGHTTP3_DATA_FLAG_NO_END_STREAM;
  }

  return 1;
}
} // namespace

auto dyn_buf = std::make_unique<std::array<uint8_t, 16_k>>();

namespace {
nghttp3_ssize dyn_read_data(nghttp3_conn *conn, int64_t stream_id,
                            nghttp3_vec *vec, size_t veccnt, uint32_t *pflags,
                            void *user_data, void *stream_user_data) {
  auto stream = static_cast<Stream *>(stream_user_data);

  auto len =
    std::min(dyn_buf->size(), static_cast<size_t>(stream->dyndataleft));

  vec[0].base = dyn_buf->data();
  vec[0].len = len;

  stream->dynbuflen += len;
  stream->dyndataleft -= len;

  if (stream->dyndataleft == 0) {
    *pflags |= NGHTTP3_DATA_FLAG_EOF;
    if (config.send_trailers) {
      *pflags |= NGHTTP3_DATA_FLAG_NO_END_STREAM;
      auto stream_id_str = util::format_uint(as_unsigned(stream_id));
      auto trailers = std::to_array({
        util::make_nv_nc("x-ngtcp2-stream-id"sv, stream_id_str),
      });

      if (auto rv = nghttp3_conn_submit_trailers(
            conn, stream_id, trailers.data(), trailers.size());
          rv != 0) {
        std::cerr << "nghttp3_conn_submit_trailers: " << nghttp3_strerror(rv)
                  << std::endl;
        return NGHTTP3_ERR_CALLBACK_FAILURE;
      }
    }
  }

  return 1;
}
} // namespace

void Stream::http_acked_stream_data(uint64_t datalen) {
  if (!dynresp) {
    return;
  }

  assert(dynbuflen >= datalen);

  dynbuflen -= datalen;
}

int Stream::send_status_response(nghttp3_conn *httpconn,
                                 unsigned int status_code,
                                 const std::vector<HTTPHeader> &extra_headers) {
  status_resp_body = make_status_body(status_code);

  auto status_code_str = util::format_uint(status_code);
  auto content_length_str = util::format_uint(status_resp_body.size());

  std::vector<nghttp3_nv> nva(4 + extra_headers.size());
  nva[0] = util::make_nv_nc(":status"sv, status_code_str);
  nva[1] = util::make_nv_nn("server"sv, NGTCP2_SERVER);
  nva[2] = util::make_nv_nn("content-type"sv, "text/html; charset=utf-8");
  nva[3] = util::make_nv_nc("content-length"sv, content_length_str);
  for (size_t i = 0; i < extra_headers.size(); ++i) {
    auto &hdr = extra_headers[i];
    auto &nv = nva[4 + i];
    nv = util::make_nv_cc(hdr.name, hdr.value);
  }

  data = const_cast<uint8_t *>(
    reinterpret_cast<const uint8_t *>(status_resp_body.data()));
  datalen = status_resp_body.size();

  nghttp3_data_reader dr{
    .read_data = read_data,
  };

  if (auto rv = nghttp3_conn_submit_response(httpconn, stream_id, nva.data(),
                                             nva.size(), &dr);
      rv != 0) {
    std::cerr << "nghttp3_conn_submit_response: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }

  if (config.send_trailers) {
    auto stream_id_str = util::format_uint(as_unsigned(stream_id));
    auto trailers = std::to_array({
      util::make_nv_nc("x-ngtcp2-stream-id"sv, stream_id_str),
    });

    if (auto rv = nghttp3_conn_submit_trailers(
          httpconn, stream_id, trailers.data(), trailers.size());
        rv != 0) {
      std::cerr << "nghttp3_conn_submit_trailers: " << nghttp3_strerror(rv)
                << std::endl;
      return -1;
    }
  }

  handler->shutdown_read(stream_id, NGHTTP3_H3_NO_ERROR);

  return 0;
}

int Stream::send_redirect_response(nghttp3_conn *httpconn,
                                   unsigned int status_code,
                                   const std::string_view &path) {
  return send_status_response(httpconn, status_code, {{"location", path}});
}

int Stream::start_response(nghttp3_conn *httpconn) {
  // TODO This should be handled by nghttp3
  if (uri.empty() || method.empty()) {
    return send_status_response(httpconn, 400);
  }

  auto req = request_path(uri, method == "CONNECT");
  if (req.path.empty()) {
    return send_status_response(httpconn, 400);
  }

  auto dyn_len = find_dyn_length(req.path);

  int64_t content_length = -1;
  nghttp3_data_reader dr{};
  auto content_type = "text/plain"sv;

  if (dyn_len == -1) {
    auto path = config.htdocs + req.path;
    auto [fe, rv] = open_file(path);
    if (rv != 0) {
      send_status_response(httpconn, 404);
      return 0;
    }

    if (fe.flags & FILE_ENTRY_TYPE_DIR) {
      send_redirect_response(httpconn, 308,
                             path.substr(config.htdocs.size() - 1) + '/');
      return 0;
    }

    content_length = static_cast<int64_t>(fe.len);

    if (method != "HEAD") {
      map_file(fe);
    }

    dr.read_data = read_data;

    auto ext = std::ranges::end(req.path) - 1;
    for (; ext != std::ranges::begin(req.path) && *ext != '.' && *ext != '/';
         --ext)
      ;
    if (*ext == '.') {
      ++ext;
      auto it =
        config.mime_types.find(std::string{ext, std::ranges::end(req.path)});
      if (it != std::ranges::end(config.mime_types)) {
        content_type = (*it).second;
      }
    }
  } else {
    content_length = dyn_len;
    dynresp = true;
    dr.read_data = dyn_read_data;

    if (method != "HEAD") {
      datalen = as_unsigned(dyn_len);
      dyndataleft = as_unsigned(dyn_len);
    }

    content_type = "application/octet-stream"sv;
  }

  auto content_length_str = util::format_uint(as_unsigned(content_length));

  std::array<nghttp3_nv, 5> nva{
    util::make_nv_nn(":status"sv, "200"sv),
    util::make_nv_nn("server"sv, NGTCP2_SERVER),
    util::make_nv_nn("content-type"sv, content_type),
    util::make_nv_nc("content-length"sv, content_length_str),
  };

  size_t nvlen = 4;

  std::string prival;

  if (req.pri.urgency != -1 || req.pri.inc != -1) {
    nghttp3_pri pri;

    if (auto rv = nghttp3_conn_get_stream_priority(httpconn, &pri, stream_id);
        rv != 0) {
      std::cerr << "nghttp3_conn_get_stream_priority: " << nghttp3_strerror(rv)
                << std::endl;
      return -1;
    }

    if (req.pri.urgency != -1) {
      pri.urgency = as_unsigned(req.pri.urgency);
    }
    if (req.pri.inc != -1) {
      pri.inc = static_cast<uint8_t>(req.pri.inc);
    }

    if (auto rv =
          nghttp3_conn_set_server_stream_priority(httpconn, stream_id, &pri);
        rv != 0) {
      std::cerr << "nghttp3_conn_set_stream_priority: " << nghttp3_strerror(rv)
                << std::endl;
      return -1;
    }

    prival = "u=";
    prival += static_cast<char>(pri.urgency + '0');
    prival += ",i";
    if (!pri.inc) {
      prival += "=?0";
    }

    nva[nvlen++] = util::make_nv_nc("priority"sv, prival);
  }

  if (!config.quiet) {
    debug::print_http_response_headers(stream_id, nva.data(), nvlen);
  }

  if (auto rv = nghttp3_conn_submit_response(httpconn, stream_id, nva.data(),
                                             nvlen, &dr);
      rv != 0) {
    std::cerr << "nghttp3_conn_submit_response: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }

  if (config.send_trailers && dyn_len == -1) {
    auto stream_id_str = util::format_uint(as_unsigned(stream_id));
    auto trailers = std::to_array({
      util::make_nv_nc("x-ngtcp2-stream-id"sv, stream_id_str),
    });

    if (auto rv = nghttp3_conn_submit_trailers(
          httpconn, stream_id, trailers.data(), trailers.size());
        rv != 0) {
      std::cerr << "nghttp3_conn_submit_trailers: " << nghttp3_strerror(rv)
                << std::endl;
      return -1;
    }
  }

  return 0;
}

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  auto h = static_cast<Handler *>(w->data);
  auto s = h->server();

  switch (h->on_write()) {
  case 0:
  case NETWORK_ERR_CLOSE_WAIT:
    return;
  default:
    s->remove(h);
  }
}
} // namespace

namespace {
void close_waitcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto h = static_cast<Handler *>(w->data);
  auto s = h->server();
  auto conn = h->conn();

  if (ngtcp2_conn_in_closing_period(conn)) {
    if (!config.quiet) {
      std::cerr << "Closing Period is over" << std::endl;
    }

    s->remove(h);
    return;
  }
  if (ngtcp2_conn_in_draining_period(conn)) {
    if (!config.quiet) {
      std::cerr << "Draining Period is over" << std::endl;
    }

    s->remove(h);
    return;
  }

  assert(0);
}
} // namespace

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  int rv;

  auto h = static_cast<Handler *>(w->data);
  auto s = h->server();

  if (!config.quiet) {
    std::cerr << "Timer expired" << std::endl;
  }

  rv = h->handle_expiry();
  if (rv != 0) {
    goto fail;
  }

  rv = h->on_write();
  if (rv != 0) {
    goto fail;
  }

  return;

fail:
  switch (rv) {
  case NETWORK_ERR_CLOSE_WAIT:
    ev_timer_stop(loop, w);
    return;
  default:
    s->remove(h);
    return;
  }
}
} // namespace

Handler::Handler(struct ev_loop *loop, Server *server)
  : loop_(loop),
    server_(server),
    qlog_(nullptr),
    scid_{},
    httpconn_{nullptr},
    nkey_update_(0),
    no_gso_{
#ifdef UDP_SEGMENT
      false
#else  // !defined(UDP_SEGMENT)
      true
#endif // !defined(UDP_SEGMENT)
    },
    close_wait_{
      .next_pkts_recv = 1,
    },
    tx_{
      .data = std::unique_ptr<uint8_t[]>(new uint8_t[NGTCP2_TX_BUFLEN]),
    } {
  ev_io_init(&wev_, writecb, 0, EV_WRITE);
  wev_.data = this;
  ev_timer_init(&timer_, timeoutcb, 0., 0.);
  timer_.data = this;
}

Handler::~Handler() {
  if (!config.quiet) {
    std::cerr << scid_ << " Closing QUIC connection " << std::endl;
  }

  ev_timer_stop(loop_, &timer_);
  ev_io_stop(loop_, &wev_);

  if (httpconn_) {
    nghttp3_conn_del(httpconn_);
  }

  if (qlog_) {
    fclose(qlog_);
  }
}

namespace {
int handshake_completed(ngtcp2_conn *conn, void *user_data) {
  auto h = static_cast<Handler *>(user_data);

  if (!config.quiet) {
    debug::handshake_completed(conn, user_data);
  }

  if (h->handshake_completed() != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

int Handler::handshake_completed() {
  if (!config.quiet) {
    std::cerr << "Negotiated cipher suite is " << tls_session_.get_cipher_name()
              << std::endl;
    if (auto group = tls_session_.get_negotiated_group(); !group.empty()) {
      std::cerr << "Negotiated group is " << group << std::endl;
    }
    std::cerr << "Negotiated ALPN is " << tls_session_.get_selected_alpn()
              << std::endl;
  }

  if (tls_session_.send_session_ticket() != 0) {
    std::cerr << "Unable to send session ticket" << std::endl;
  }

  std::array<uint8_t, NGTCP2_CRYPTO_MAX_REGULAR_TOKENLEN> token;

  auto path = ngtcp2_conn_get_path(conn_);
  auto t = util::system_clock_now();

  auto tokenlen = ngtcp2_crypto_generate_regular_token(
    token.data(), config.static_secret.data(), config.static_secret.size(),
    path->remote.addr, path->remote.addrlen, t);
  if (tokenlen < 0) {
    if (!config.quiet) {
      std::cerr << "Unable to generate token" << std::endl;
    }
    return 0;
  }

  if (auto rv = ngtcp2_conn_submit_new_token(conn_, token.data(),
                                             as_unsigned(tokenlen));
      rv != 0) {
    if (!config.quiet) {
      std::cerr << "ngtcp2_conn_submit_new_token: " << ngtcp2_strerror(rv)
                << std::endl;
    }
    return -1;
  }

  return 0;
}

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
  auto h = static_cast<Handler *>(user_data);

  if (h->recv_stream_data(flags, stream_id, {data, datalen}) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

namespace {
int acked_stream_data_offset(ngtcp2_conn *conn, int64_t stream_id,
                             uint64_t offset, uint64_t datalen, void *user_data,
                             void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (h->acked_stream_data_offset(stream_id, datalen) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

int Handler::acked_stream_data_offset(int64_t stream_id, uint64_t datalen) {
  if (!httpconn_) {
    return 0;
  }

  if (auto rv = nghttp3_conn_add_ack_offset(httpconn_, stream_id, datalen);
      rv != 0) {
    std::cerr << "nghttp3_conn_add_ack_offset: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }

  return 0;
}

namespace {
int stream_open(ngtcp2_conn *conn, int64_t stream_id, void *user_data) {
  auto h = static_cast<Handler *>(user_data);
  h->on_stream_open(stream_id);
  return 0;
}
} // namespace

void Handler::on_stream_open(int64_t stream_id) {
  if (!ngtcp2_is_bidi_stream(stream_id)) {
    return;
  }
  auto it = streams_.find(stream_id);
  (void)it;
  assert(it == std::ranges::end(streams_));
  streams_.emplace(stream_id, std::make_unique<Stream>(stream_id, this));
}

namespace {
int stream_close(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id,
                 uint64_t app_error_code, void *user_data,
                 void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);

  if (!(flags & NGTCP2_STREAM_CLOSE_FLAG_APP_ERROR_CODE_SET)) {
    app_error_code = NGHTTP3_H3_NO_ERROR;
  }

  if (h->on_stream_close(stream_id, app_error_code) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

namespace {
int stream_reset(ngtcp2_conn *conn, int64_t stream_id, uint64_t final_size,
                 uint64_t app_error_code, void *user_data,
                 void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (h->on_stream_reset(stream_id) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

int Handler::on_stream_reset(int64_t stream_id) {
  if (httpconn_) {
    if (auto rv = nghttp3_conn_shutdown_stream_read(httpconn_, stream_id);
        rv != 0) {
      std::cerr << "nghttp3_conn_shutdown_stream_read: " << nghttp3_strerror(rv)
                << std::endl;
      return -1;
    }
  }
  return 0;
}

namespace {
int stream_stop_sending(ngtcp2_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *user_data,
                        void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (h->on_stream_stop_sending(stream_id) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

int Handler::on_stream_stop_sending(int64_t stream_id) {
  if (!httpconn_) {
    return 0;
  }

  if (auto rv = nghttp3_conn_shutdown_stream_read(httpconn_, stream_id);
      rv != 0) {
    std::cerr << "nghttp3_conn_shutdown_stream_read: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }

  return 0;
}

namespace {
void rand_bytes(uint8_t *dest, size_t destlen) {
  auto rv = util::generate_secure_random({dest, destlen});
  if (rv != 0) {
    assert(0);
    abort();
  }
}
} // namespace

namespace {
void rand(uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *rand_ctx) {
  rand_bytes(dest, destlen);
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

  auto h = static_cast<Handler *>(user_data);
  h->server()->associate_cid(cid, h);

  return 0;
}
} // namespace

namespace {
int remove_connection_id(ngtcp2_conn *conn, const ngtcp2_cid *cid,
                         void *user_data) {
  auto h = static_cast<Handler *>(user_data);
  h->server()->dissociate_cid(cid);
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
  auto h = static_cast<Handler *>(user_data);
  if (h->update_key(rx_secret, tx_secret, rx_aead_ctx, rx_iv, tx_aead_ctx,
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

  if (res != NGTCP2_PATH_VALIDATION_RESULT_SUCCESS ||
      !(flags & NGTCP2_PATH_VALIDATION_FLAG_NEW_TOKEN)) {
    return 0;
  }

  std::array<uint8_t, NGTCP2_CRYPTO_MAX_REGULAR_TOKENLEN> token;
  auto t = util::system_clock_now();

  auto tokenlen = ngtcp2_crypto_generate_regular_token(
    token.data(), config.static_secret.data(), config.static_secret.size(),
    path->remote.addr, path->remote.addrlen, t);
  if (tokenlen < 0) {
    if (!config.quiet) {
      std::cerr << "Unable to generate token" << std::endl;
    }

    return 0;
  }

  if (auto rv =
        ngtcp2_conn_submit_new_token(conn, token.data(), as_unsigned(tokenlen));
      rv != 0) {
    if (!config.quiet) {
      std::cerr << "ngtcp2_conn_submit_new_token: " << ngtcp2_strerror(rv)
                << std::endl;
    }

    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

namespace {
int extend_max_remote_streams_bidi(ngtcp2_conn *conn, uint64_t max_streams,
                                   void *user_data) {
  auto h = static_cast<Handler *>(user_data);
  h->extend_max_remote_streams_bidi(max_streams);
  return 0;
}
} // namespace

void Handler::extend_max_remote_streams_bidi(uint64_t max_streams) {
  if (!httpconn_) {
    return;
  }

  nghttp3_conn_set_max_client_streams_bidi(httpconn_, max_streams);
}

namespace {
int http_recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                   size_t datalen, void *user_data, void *stream_user_data) {
  if (!config.quiet && !config.no_http_dump) {
    debug::print_http_data(stream_id, {data, datalen});
  }
  auto h = static_cast<Handler *>(user_data);
  h->http_consume(stream_id, datalen);
  return 0;
}
} // namespace

namespace {
int http_deferred_consume(nghttp3_conn *conn, int64_t stream_id,
                          size_t nconsumed, void *user_data,
                          void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  h->http_consume(stream_id, nconsumed);
  return 0;
}
} // namespace

void Handler::http_consume(int64_t stream_id, size_t nconsumed) {
  ngtcp2_conn_extend_max_stream_offset(conn_, stream_id, nconsumed);
  ngtcp2_conn_extend_max_offset(conn_, nconsumed);
}

namespace {
int http_begin_request_headers(nghttp3_conn *conn, int64_t stream_id,
                               void *user_data, void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_begin_request_headers(stream_id);
  }

  auto h = static_cast<Handler *>(user_data);
  h->http_begin_request_headers(stream_id);
  return 0;
}
} // namespace

void Handler::http_begin_request_headers(int64_t stream_id) {
  auto it = streams_.find(stream_id);
  assert(it != std::ranges::end(streams_));
  auto &stream = (*it).second;

  nghttp3_conn_set_stream_user_data(httpconn_, stream_id, stream.get());
}

namespace {
int http_recv_request_header(nghttp3_conn *conn, int64_t stream_id,
                             int32_t token, nghttp3_rcbuf *name,
                             nghttp3_rcbuf *value, uint8_t flags,
                             void *user_data, void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_header(stream_id, name, value, flags);
  }

  auto h = static_cast<Handler *>(user_data);
  auto stream = static_cast<Stream *>(stream_user_data);
  h->http_recv_request_header(stream, token, name, value);
  return 0;
}
} // namespace

void Handler::http_recv_request_header(Stream *stream, int32_t token,
                                       nghttp3_rcbuf *name,
                                       nghttp3_rcbuf *value) {
  auto v = nghttp3_rcbuf_get_buf(value);

  switch (token) {
  case NGHTTP3_QPACK_TOKEN__PATH:
    stream->uri = std::string{v.base, v.base + v.len};
    break;
  case NGHTTP3_QPACK_TOKEN__METHOD:
    stream->method = std::string{v.base, v.base + v.len};
    break;
  case NGHTTP3_QPACK_TOKEN__AUTHORITY:
    stream->authority = std::string{v.base, v.base + v.len};
    break;
  }
}

namespace {
int http_end_request_headers(nghttp3_conn *conn, int64_t stream_id, int fin,
                             void *user_data, void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_end_headers(stream_id);
  }

  auto h = static_cast<Handler *>(user_data);
  auto stream = static_cast<Stream *>(stream_user_data);
  if (h->http_end_request_headers(stream) != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

int Handler::http_end_request_headers(Stream *stream) {
  if (config.early_response) {
    if (start_response(stream) != 0) {
      return -1;
    }

    shutdown_read(stream->stream_id, NGHTTP3_H3_NO_ERROR);
  }
  return 0;
}

namespace {
int http_end_stream(nghttp3_conn *conn, int64_t stream_id, void *user_data,
                    void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  auto stream = static_cast<Stream *>(stream_user_data);
  if (h->http_end_stream(stream) != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

int Handler::http_end_stream(Stream *stream) {
  if (!config.early_response) {
    return start_response(stream);
  }
  return 0;
}

int Handler::start_response(Stream *stream) {
  return stream->start_response(httpconn_);
}

namespace {
int http_acked_stream_data(nghttp3_conn *conn, int64_t stream_id,
                           uint64_t datalen, void *user_data,
                           void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  auto stream = static_cast<Stream *>(stream_user_data);
  h->http_acked_stream_data(stream, datalen);
  return 0;
}
} // namespace

void Handler::http_acked_stream_data(Stream *stream, uint64_t datalen) {
  stream->http_acked_stream_data(datalen);
}

namespace {
int http_stream_close(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *conn_user_data,
                      void *stream_user_data) {
  auto h = static_cast<Handler *>(conn_user_data);
  h->http_stream_close(stream_id, app_error_code);
  return 0;
}
} // namespace

void Handler::http_stream_close(int64_t stream_id, uint64_t app_error_code) {
  auto it = streams_.find(stream_id);
  if (it == std::ranges::end(streams_)) {
    return;
  }

  if (!config.quiet) {
    std::cerr << "HTTP stream " << stream_id << " closed with error code "
              << app_error_code << std::endl;
  }

  streams_.erase(it);

  if (ngtcp2_is_bidi_stream(stream_id)) {
    assert(!ngtcp2_conn_is_local_stream(conn_, stream_id));
    ngtcp2_conn_extend_max_streams_bidi(conn_, 1);
  }
}

namespace {
int http_stop_sending(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *user_data,
                      void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (h->http_stop_sending(stream_id, app_error_code) != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

int Handler::http_stop_sending(int64_t stream_id, uint64_t app_error_code) {
  if (auto rv =
        ngtcp2_conn_shutdown_stream_read(conn_, 0, stream_id, app_error_code);
      rv != 0) {
    std::cerr << "ngtcp2_conn_shutdown_stream_read: " << ngtcp2_strerror(rv)
              << std::endl;
    return -1;
  }
  return 0;
}

namespace {
int http_reset_stream(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *user_data,
                      void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (h->http_reset_stream(stream_id, app_error_code) != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

int Handler::http_reset_stream(int64_t stream_id, uint64_t app_error_code) {
  if (auto rv =
        ngtcp2_conn_shutdown_stream_write(conn_, 0, stream_id, app_error_code);
      rv != 0) {
    std::cerr << "ngtcp2_conn_shutdown_stream_write: " << ngtcp2_strerror(rv)
              << std::endl;
    return -1;
  }
  return 0;
}

namespace {
int http_recv_settings(nghttp3_conn *conn, const nghttp3_settings *settings,
                       void *conn_user_data) {
  if (!config.quiet) {
    debug::print_http_settings(settings);
  }

  return 0;
}
} // namespace

int Handler::setup_httpconn() {
  if (httpconn_) {
    return 0;
  }

  if (ngtcp2_conn_get_streams_uni_left(conn_) < 3) {
    std::cerr << "peer does not allow at least 3 unidirectional streams."
              << std::endl;
    return -1;
  }

  nghttp3_callbacks callbacks{
    .acked_stream_data = ::http_acked_stream_data,
    .stream_close = ::http_stream_close,
    .recv_data = ::http_recv_data,
    .deferred_consume = ::http_deferred_consume,
    .begin_headers = ::http_begin_request_headers,
    .recv_header = ::http_recv_request_header,
    .end_headers = ::http_end_request_headers,
    .stop_sending = ::http_stop_sending,
    .end_stream = ::http_end_stream,
    .reset_stream = ::http_reset_stream,
    .recv_settings = ::http_recv_settings,
    .rand = rand_bytes,
  };
  nghttp3_settings settings;
  nghttp3_settings_default(&settings);
  settings.qpack_max_dtable_capacity = 4096;
  settings.qpack_blocked_streams = 100;

  nghttp3_vec origin_list;

  if (config.origin_list) {
    origin_list.base = config.origin_list->data();
    origin_list.len = config.origin_list->size();

    settings.origin_list = &origin_list;
  }

  auto mem = nghttp3_mem_default();

  if (auto rv =
        nghttp3_conn_server_new(&httpconn_, &callbacks, &settings, mem, this);
      rv != 0) {
    std::cerr << "nghttp3_conn_server_new: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }

  auto params = ngtcp2_conn_get_local_transport_params(conn_);

  nghttp3_conn_set_max_client_streams_bidi(httpconn_,
                                           params->initial_max_streams_bidi);

  int64_t ctrl_stream_id;

  if (auto rv = ngtcp2_conn_open_uni_stream(conn_, &ctrl_stream_id, nullptr);
      rv != 0) {
    std::cerr << "ngtcp2_conn_open_uni_stream: " << ngtcp2_strerror(rv)
              << std::endl;
    return -1;
  }

  if (auto rv = nghttp3_conn_bind_control_stream(httpconn_, ctrl_stream_id);
      rv != 0) {
    std::cerr << "nghttp3_conn_bind_control_stream: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }

  if (!config.quiet) {
    fprintf(stderr, "http: control stream=%" PRIx64 "\n", ctrl_stream_id);
  }

  int64_t qpack_enc_stream_id, qpack_dec_stream_id;

  if (auto rv =
        ngtcp2_conn_open_uni_stream(conn_, &qpack_enc_stream_id, nullptr);
      rv != 0) {
    std::cerr << "ngtcp2_conn_open_uni_stream: " << ngtcp2_strerror(rv)
              << std::endl;
    return -1;
  }

  if (auto rv =
        ngtcp2_conn_open_uni_stream(conn_, &qpack_dec_stream_id, nullptr);
      rv != 0) {
    std::cerr << "ngtcp2_conn_open_uni_stream: " << ngtcp2_strerror(rv)
              << std::endl;
    return -1;
  }

  if (auto rv = nghttp3_conn_bind_qpack_streams(httpconn_, qpack_enc_stream_id,
                                                qpack_dec_stream_id);
      rv != 0) {
    std::cerr << "nghttp3_conn_bind_qpack_streams: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }

  if (!config.quiet) {
    fprintf(stderr,
            "http: QPACK streams encoder=%" PRIx64 " decoder=%" PRIx64 "\n",
            qpack_enc_stream_id, qpack_dec_stream_id);
  }

  return 0;
}

namespace {
int extend_max_stream_data(ngtcp2_conn *conn, int64_t stream_id,
                           uint64_t max_data, void *user_data,
                           void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (h->extend_max_stream_data(stream_id, max_data) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

int Handler::extend_max_stream_data(int64_t stream_id, uint64_t max_data) {
  if (auto rv = nghttp3_conn_unblock_stream(httpconn_, stream_id); rv != 0) {
    std::cerr << "nghttp3_conn_unblock_stream: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }
  return 0;
}

namespace {
int recv_tx_key(ngtcp2_conn *conn, ngtcp2_encryption_level level,
                void *user_data) {
  if (level != NGTCP2_ENCRYPTION_LEVEL_1RTT) {
    return 0;
  }

  auto h = static_cast<Handler *>(user_data);
  if (h->setup_httpconn() != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

namespace {
void write_qlog(void *user_data, uint32_t flags, const void *data,
                size_t datalen) {
  auto h = static_cast<Handler *>(user_data);
  h->write_qlog(data, datalen);
}
} // namespace

void Handler::write_qlog(const void *data, size_t datalen) {
  assert(qlog_);
  fwrite(data, 1, datalen, qlog_);
}

int Handler::init(const Endpoint &ep, const Address &local_addr,
                  const sockaddr *sa, socklen_t salen, const ngtcp2_cid *dcid,
                  const ngtcp2_cid *scid, const ngtcp2_cid *ocid,
                  std::span<const uint8_t> token, ngtcp2_token_type token_type,
                  uint32_t version, TLSServerContext &tls_ctx) {
  auto callbacks = ngtcp2_callbacks{
    .recv_client_initial = ngtcp2_crypto_recv_client_initial_cb,
    .recv_crypto_data = ::recv_crypto_data,
    .handshake_completed = ::handshake_completed,
    .encrypt = ngtcp2_crypto_encrypt_cb,
    .decrypt = ngtcp2_crypto_decrypt_cb,
    .hp_mask = do_hp_mask,
    .recv_stream_data = ::recv_stream_data,
    .acked_stream_data_offset = ::acked_stream_data_offset,
    .stream_open = stream_open,
    .stream_close = stream_close,
    .rand = rand,
    .get_new_connection_id = get_new_connection_id,
    .remove_connection_id = remove_connection_id,
    .update_key = ::update_key,
    .path_validation = path_validation,
    .stream_reset = ::stream_reset,
    .extend_max_remote_streams_bidi = ::extend_max_remote_streams_bidi,
    .extend_max_stream_data = ::extend_max_stream_data,
    .delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    .delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    .get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb,
    .stream_stop_sending = stream_stop_sending,
    .version_negotiation = ngtcp2_crypto_version_negotiation_cb,
    .recv_tx_key = ::recv_tx_key,
  };

  scid_.datalen = NGTCP2_SV_SCIDLEN;
  if (util::generate_secure_random({scid_.data, scid_.datalen}) != 0) {
    std::cerr << "Could not generate connection ID" << std::endl;
    return -1;
  }

  ngtcp2_settings settings;
  ngtcp2_settings_default(&settings);
  settings.log_printf = config.quiet ? nullptr : debug::log_printf;
  settings.initial_ts = util::timestamp();
  settings.token = token.data();
  settings.tokenlen = token.size();
  settings.token_type = token_type;
  settings.cc_algo = config.cc_algo;
  settings.initial_rtt = config.initial_rtt;
  settings.max_window = config.max_window;
  settings.max_stream_window = config.max_stream_window;
  settings.handshake_timeout = config.handshake_timeout;
  settings.no_pmtud = config.no_pmtud;
  settings.ack_thresh = config.ack_thresh;
  if (config.max_udp_payload_size) {
    settings.max_tx_udp_payload_size = config.max_udp_payload_size;
    settings.no_tx_udp_payload_size_shaping = 1;
  }
  if (!config.qlog_dir.empty()) {
    auto path = std::string{config.qlog_dir};
    path += '/';
    path += util::format_hex(scid_.data, as_signed(scid_.datalen));
    path += ".sqlog";
    qlog_ = fopen(path.c_str(), "w");
    if (qlog_ == nullptr) {
      std::cerr << "Could not open qlog file " << std::quoted(path) << ": "
                << strerror(errno) << std::endl;
      return -1;
    }
    settings.qlog_write = ::write_qlog;
  }
  if (!config.preferred_versions.empty()) {
    settings.preferred_versions = config.preferred_versions.data();
    settings.preferred_versionslen = config.preferred_versions.size();
  }
  if (!config.available_versions.empty()) {
    settings.available_versions = config.available_versions.data();
    settings.available_versionslen = config.available_versions.size();
  }
  if (config.initial_pkt_num == UINT32_MAX) {
    auto dis = std::uniform_int_distribution<uint32_t>(0, INT32_MAX);
    settings.initial_pkt_num = dis(randgen);
  } else {
    settings.initial_pkt_num = config.initial_pkt_num;
  }

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
  params.initial_max_streams_uni = config.max_streams_uni;
  params.max_idle_timeout = config.timeout;
  params.stateless_reset_token_present = 1;
  params.active_connection_id_limit = 7;
  params.grease_quic_bit = 1;

  if (ocid) {
    params.original_dcid = *ocid;
    params.retry_scid = *scid;
    params.retry_scid_present = 1;
  } else {
    params.original_dcid = *scid;
  }

  params.original_dcid_present = 1;

  if (ngtcp2_crypto_generate_stateless_reset_token(
        params.stateless_reset_token, config.static_secret.data(),
        config.static_secret.size(), &scid_) != 0) {
    return -1;
  }

  if (config.preferred_ipv4_addr.len || config.preferred_ipv6_addr.len) {
    params.preferred_addr_present = 1;

    if (config.preferred_ipv4_addr.len) {
      params.preferred_addr.ipv4 = config.preferred_ipv4_addr.su.in;
      params.preferred_addr.ipv4_present = 1;
    }

    if (config.preferred_ipv6_addr.len) {
      params.preferred_addr.ipv6 = config.preferred_ipv6_addr.su.in6;
      params.preferred_addr.ipv6_present = 1;
    }

    if (util::generate_secure_random(
          params.preferred_addr.stateless_reset_token) != 0) {
      std::cerr << "Could not generate preferred address stateless reset token"
                << std::endl;
      return -1;
    }

    params.preferred_addr.cid.datalen = NGTCP2_SV_SCIDLEN;
    if (util::generate_secure_random({params.preferred_addr.cid.data,
                                      params.preferred_addr.cid.datalen}) !=
        0) {
      std::cerr << "Could not generate preferred address connection ID"
                << std::endl;
      return -1;
    }
  }

  auto path = ngtcp2_path{
    .local =
      {
        .addr = const_cast<sockaddr *>(&local_addr.su.sa),
        .addrlen = local_addr.len,
      },
    .remote =
      {
        .addr = const_cast<sockaddr *>(sa),
        .addrlen = salen,
      },
    .user_data = const_cast<Endpoint *>(&ep),
  };
  if (auto rv =
        ngtcp2_conn_server_new(&conn_, dcid, &scid_, &path, version, &callbacks,
                               &settings, &params, nullptr, this);
      rv != 0) {
    std::cerr << "ngtcp2_conn_server_new: " << ngtcp2_strerror(rv) << std::endl;
    return -1;
  }

  if (tls_session_.init(tls_ctx, this) != 0) {
    return -1;
  }

  tls_session_.enable_keylog();

  ngtcp2_conn_set_tls_native_handle(conn_, tls_session_.get_native_handle());

  ev_io_set(&wev_, ep.fd, EV_WRITE);

  return 0;
}

int Handler::feed_data(const Endpoint &ep, const Address &local_addr,
                       const sockaddr *sa, socklen_t salen,
                       const ngtcp2_pkt_info *pi,
                       std::span<const uint8_t> data) {
  auto path = ngtcp2_path{
    .local =
      {
        .addr = const_cast<sockaddr *>(&local_addr.su.sa),
        .addrlen = local_addr.len,
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
    switch (rv) {
    case NGTCP2_ERR_DRAINING:
      start_draining_period();
      return NETWORK_ERR_CLOSE_WAIT;
    case NGTCP2_ERR_RETRY:
      return NETWORK_ERR_RETRY;
    case NGTCP2_ERR_DROP_CONN:
      return NETWORK_ERR_DROP_CONN;
    case NGTCP2_ERR_CRYPTO:
      if (!last_error_.error_code) {
        ngtcp2_ccerr_set_tls_alert(
          &last_error_, ngtcp2_conn_get_tls_alert(conn_), nullptr, 0);
      }
      break;
    default:
      if (!last_error_.error_code) {
        ngtcp2_ccerr_set_liberr(&last_error_, rv, nullptr, 0);
      }
    }
    return handle_error();
  }

  return 0;
}

int Handler::on_read(const Endpoint &ep, const Address &local_addr,
                     const sockaddr *sa, socklen_t salen,
                     const ngtcp2_pkt_info *pi, std::span<const uint8_t> data) {
  if (auto rv = feed_data(ep, local_addr, sa, salen, pi, data); rv != 0) {
    return rv;
  }

  update_timer();

  return 0;
}

int Handler::handle_expiry() {
  auto now = util::timestamp();
  if (auto rv = ngtcp2_conn_handle_expiry(conn_, now); rv != 0) {
    std::cerr << "ngtcp2_conn_handle_expiry: " << ngtcp2_strerror(rv)
              << std::endl;
    ngtcp2_ccerr_set_liberr(&last_error_, rv, nullptr, 0);
    return handle_error();
  }

  return 0;
}

int Handler::on_write() {
  if (ngtcp2_conn_in_closing_period(conn_) ||
      ngtcp2_conn_in_draining_period(conn_)) {
    return 0;
  }

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

  update_timer();

  return 0;
}

namespace {
ngtcp2_ssize write_pkt(ngtcp2_conn *conn, ngtcp2_path *path,
                       ngtcp2_pkt_info *pi, uint8_t *dest, size_t destlen,
                       ngtcp2_tstamp ts, void *user_data) {
  auto h = static_cast<Handler *>(user_data);

  return h->write_pkt(path, pi, dest, destlen, ts);
}
} // namespace

ngtcp2_ssize Handler::write_pkt(ngtcp2_path *path, ngtcp2_pkt_info *pi,
                                uint8_t *dest, size_t destlen,
                                ngtcp2_tstamp ts) {
  std::array<nghttp3_vec, 16> vec;

  for (;;) {
    int64_t stream_id = -1;
    int fin = 0;
    nghttp3_ssize sveccnt = 0;

    if (httpconn_ && ngtcp2_conn_get_max_data_left(conn_)) {
      sveccnt = nghttp3_conn_writev_stream(httpconn_, &stream_id, &fin,
                                           vec.data(), vec.size());
      if (sveccnt < 0) {
        std::cerr << "nghttp3_conn_writev_stream: "
                  << nghttp3_strerror(static_cast<int>(sveccnt)) << std::endl;
        ngtcp2_ccerr_set_application_error(
          &last_error_,
          nghttp3_err_infer_quic_app_error_code(static_cast<int>(sveccnt)),
          nullptr, 0);
        return NGTCP2_ERR_CALLBACK_FAILURE;
      }
    }

    ngtcp2_ssize ndatalen;
    auto v = vec.data();
    auto vcnt = static_cast<size_t>(sveccnt);

    uint32_t flags =
      NGTCP2_WRITE_STREAM_FLAG_MORE | NGTCP2_WRITE_STREAM_FLAG_PADDING;
    if (fin) {
      flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
    }

    auto nwrite = ngtcp2_conn_writev_stream(
      conn_, path, pi, dest, destlen, &ndatalen, flags, stream_id,
      reinterpret_cast<const ngtcp2_vec *>(v), vcnt, ts);
    if (nwrite < 0) {
      switch (nwrite) {
      case NGTCP2_ERR_STREAM_DATA_BLOCKED:
        assert(ndatalen == -1);
        nghttp3_conn_block_stream(httpconn_, stream_id);
        continue;
      case NGTCP2_ERR_STREAM_SHUT_WR:
        assert(ndatalen == -1);
        nghttp3_conn_shutdown_stream_write(httpconn_, stream_id);
        continue;
      case NGTCP2_ERR_WRITE_MORE:
        assert(ndatalen >= 0);
        if (auto rv = nghttp3_conn_add_write_offset(httpconn_, stream_id,
                                                    as_unsigned(ndatalen));
            rv != 0) {
          std::cerr << "nghttp3_conn_add_write_offset: " << nghttp3_strerror(rv)
                    << std::endl;
          ngtcp2_ccerr_set_application_error(
            &last_error_, nghttp3_err_infer_quic_app_error_code(rv), nullptr,
            0);
          return NGTCP2_ERR_CALLBACK_FAILURE;
        }
        continue;
      }

      assert(ndatalen == -1);

      std::cerr << "ngtcp2_conn_writev_stream: "
                << ngtcp2_strerror(static_cast<int>(nwrite)) << std::endl;
      ngtcp2_ccerr_set_liberr(&last_error_, static_cast<int>(nwrite), nullptr,
                              0);
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    if (ndatalen >= 0) {
      if (auto rv = nghttp3_conn_add_write_offset(httpconn_, stream_id,
                                                  as_unsigned(ndatalen));
          rv != 0) {
        std::cerr << "nghttp3_conn_add_write_offset: " << nghttp3_strerror(rv)
                  << std::endl;
        ngtcp2_ccerr_set_application_error(
          &last_error_, nghttp3_err_infer_quic_app_error_code(rv), nullptr, 0);
        return NGTCP2_ERR_CALLBACK_FAILURE;
      }
    }

    return nwrite;
  }
}

int Handler::write_streams() {
  ngtcp2_path_storage ps;
  ngtcp2_pkt_info pi;
  size_t gso_size;
  auto ts = util::timestamp();
  auto txbuf = std::span{tx_.data.get(), NGTCP2_TX_BUFLEN};

  ngtcp2_path_storage_zero(&ps);

  auto nwrite =
    ngtcp2_conn_write_aggregate_pkt(conn_, &ps.path, &pi, txbuf.data(),
                                    txbuf.size(), &gso_size, ::write_pkt, ts);
  if (nwrite < 0) {
    return handle_error();
  }

  if (nwrite == 0) {
    return 0;
  }

  send_packet(ps.path, pi.ecn, txbuf.first(static_cast<size_t>(nwrite)),
              gso_size);

  return 0;
}

int Handler::send_packet(const ngtcp2_path &path, unsigned int ecn,
                         std::span<const uint8_t> data, size_t gso_size) {
  auto &ep = *static_cast<Endpoint *>(path.user_data);
  auto [rest, rv] = server_->send_packet(ep, no_gso_, path.local, path.remote,
                                         ecn, data, gso_size);
  if (rv != 0) {
    assert(NETWORK_ERR_SEND_BLOCKED == rv);

    on_send_blocked(path, ecn, rest, gso_size);

    start_wev_endpoint(ep);

    return rv;
  }

  return 0;
}

void Handler::on_send_blocked(const ngtcp2_path &path, unsigned int ecn,
                              std::span<const uint8_t> data, size_t gso_size) {
  assert(!tx_.send_blocked);
  assert(gso_size);

  tx_.send_blocked = true;

  auto &p = tx_.blocked;

  memcpy(&p.local_addr.su, path.local.addr, path.local.addrlen);
  memcpy(&p.remote_addr.su, path.remote.addr, path.remote.addrlen);

  p.local_addr.len = path.local.addrlen;
  p.remote_addr.len = path.remote.addrlen;
  p.endpoint = static_cast<Endpoint *>(path.user_data);
  p.ecn = ecn;
  p.data = data;
  p.gso_size = gso_size;
}

void Handler::start_wev_endpoint(const Endpoint &ep) {
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

int Handler::send_blocked_packet() {
  assert(tx_.send_blocked);

  auto &p = tx_.blocked;

  ngtcp2_addr local_addr{
    .addr = &p.local_addr.su.sa,
    .addrlen = p.local_addr.len,
  };
  ngtcp2_addr remote_addr{
    .addr = &p.remote_addr.su.sa,
    .addrlen = p.remote_addr.len,
  };

  auto [rest, rv] = server_->send_packet(
    *p.endpoint, no_gso_, local_addr, remote_addr, p.ecn, p.data, p.gso_size);
  if (rv != 0) {
    assert(NETWORK_ERR_SEND_BLOCKED == rv);

    p.data = rest;

    start_wev_endpoint(*p.endpoint);

    return 0;
  }

  tx_.send_blocked = false;

  return 0;
}

void Handler::signal_write() { ev_io_start(loop_, &wev_); }

void Handler::start_draining_period() {
  ev_io_stop(loop_, &wev_);

  ev_set_cb(&timer_, close_waitcb);
  timer_.repeat =
    static_cast<ev_tstamp>(ngtcp2_conn_get_pto(conn_)) / NGTCP2_SECONDS * 3;
  ev_timer_again(loop_, &timer_);

  if (!config.quiet) {
    std::cerr << "Draining period has started (" << timer_.repeat << " seconds)"
              << std::endl;
  }
}

int Handler::start_closing_period() {
  if (!conn_ || ngtcp2_conn_in_closing_period(conn_) ||
      ngtcp2_conn_in_draining_period(conn_)) {
    return 0;
  }

  ev_io_stop(loop_, &wev_);

  ev_set_cb(&timer_, close_waitcb);
  timer_.repeat =
    static_cast<ev_tstamp>(ngtcp2_conn_get_pto(conn_)) / NGTCP2_SECONDS * 3;
  ev_timer_again(loop_, &timer_);

  if (!config.quiet) {
    std::cerr << "Closing period has started (" << timer_.repeat << " seconds)"
              << std::endl;
  }

  conn_closebuf_ = std::make_unique<Buffer>(NGTCP2_MAX_UDP_PAYLOAD_SIZE);

  ngtcp2_path_storage ps;

  ngtcp2_path_storage_zero(&ps);

  ngtcp2_pkt_info pi;
  auto n = ngtcp2_conn_write_connection_close(
    conn_, &ps.path, &pi, conn_closebuf_->wpos(), conn_closebuf_->left(),
    &last_error_, util::timestamp());
  if (n < 0) {
    std::cerr << "ngtcp2_conn_write_connection_close: "
              << ngtcp2_strerror(static_cast<int>(n)) << std::endl;
    return -1;
  }

  if (n == 0) {
    return 0;
  }

  conn_closebuf_->push(as_unsigned(n));

  return 0;
}

int Handler::handle_error() {
  if (last_error_.type == NGTCP2_CCERR_TYPE_IDLE_CLOSE) {
    return -1;
  }

  if (start_closing_period() != 0) {
    return -1;
  }

  if (ngtcp2_conn_in_draining_period(conn_)) {
    return NETWORK_ERR_CLOSE_WAIT;
  }

  if (auto rv = send_conn_close(); rv != NETWORK_ERR_OK) {
    return rv;
  }

  return NETWORK_ERR_CLOSE_WAIT;
}

int Handler::send_conn_close() {
  if (!config.quiet) {
    std::cerr << "Closing Period: TX CONNECTION_CLOSE" << std::endl;
  }

  assert(conn_closebuf_ && conn_closebuf_->size());
  assert(conn_);
  assert(!ngtcp2_conn_in_draining_period(conn_));

  auto path = ngtcp2_conn_get_path(conn_);

  return server_->send_packet(*static_cast<Endpoint *>(path->user_data),
                              path->local, path->remote,
                              /* ecn = */ 0, conn_closebuf_->data());
}

int Handler::send_conn_close(const Endpoint &ep, const Address &local_addr,
                             const sockaddr *sa, socklen_t salen,
                             const ngtcp2_pkt_info *pi,
                             std::span<const uint8_t> data) {
  assert(conn_closebuf_ && conn_closebuf_->size());

  close_wait_.bytes_recv += data.size();
  ++close_wait_.num_pkts_recv;

  if (close_wait_.num_pkts_recv < close_wait_.next_pkts_recv ||
      close_wait_.bytes_recv * 3 <
        close_wait_.bytes_sent + conn_closebuf_->size()) {
    return 0;
  }

  auto path = ngtcp2_path{
    .local =
      {
        .addr = const_cast<sockaddr *>(&local_addr.su.sa),
        .addrlen = local_addr.len,
      },
    .remote =
      {
        .addr = const_cast<sockaddr *>(sa),
        .addrlen = salen,
      },
    .user_data = const_cast<Endpoint *>(&ep),
  };

  auto rv = server_->send_packet(ep, path.local, path.remote,
                                 /* ecn = */ 0, conn_closebuf_->data());
  if (rv != 0) {
    return rv;
  }

  close_wait_.bytes_sent += conn_closebuf_->size();
  close_wait_.next_pkts_recv *= 2;

  return 0;
}

void Handler::update_timer() {
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

int Handler::recv_stream_data(uint32_t flags, int64_t stream_id,
                              std::span<const uint8_t> data) {
  if (!config.quiet && !config.no_quic_dump) {
    debug::print_stream_data(stream_id, data);
  }

  if (!httpconn_) {
    return 0;
  }

  auto nconsumed =
    nghttp3_conn_read_stream(httpconn_, stream_id, data.data(), data.size(),
                             flags & NGTCP2_STREAM_DATA_FLAG_FIN);
  if (nconsumed < 0) {
    std::cerr << "nghttp3_conn_read_stream: "
              << nghttp3_strerror(static_cast<int>(nconsumed)) << std::endl;
    ngtcp2_ccerr_set_application_error(
      &last_error_,
      nghttp3_err_infer_quic_app_error_code(static_cast<int>(nconsumed)),
      nullptr, 0);
    return -1;
  }

  ngtcp2_conn_extend_max_stream_offset(conn_, stream_id,
                                       static_cast<uint64_t>(nconsumed));
  ngtcp2_conn_extend_max_offset(conn_, static_cast<uint64_t>(nconsumed));

  return 0;
}

int Handler::update_key(uint8_t *rx_secret, uint8_t *tx_secret,
                        ngtcp2_crypto_aead_ctx *rx_aead_ctx, uint8_t *rx_iv,
                        ngtcp2_crypto_aead_ctx *tx_aead_ctx, uint8_t *tx_iv,
                        const uint8_t *current_rx_secret,
                        const uint8_t *current_tx_secret, size_t secretlen) {
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

Server *Handler::server() const { return server_; }

int Handler::on_stream_close(int64_t stream_id, uint64_t app_error_code) {
  if (!config.quiet) {
    std::cerr << "QUIC stream " << stream_id << " closed" << std::endl;
  }

  if (httpconn_) {
    if (app_error_code == 0) {
      app_error_code = NGHTTP3_H3_NO_ERROR;
    }
    auto rv = nghttp3_conn_close_stream(httpconn_, stream_id, app_error_code);
    switch (rv) {
    case 0:
      break;
    case NGHTTP3_ERR_STREAM_NOT_FOUND:
      if (ngtcp2_is_bidi_stream(stream_id)) {
        assert(!ngtcp2_conn_is_local_stream(conn_, stream_id));
        ngtcp2_conn_extend_max_streams_bidi(conn_, 1);
      }
      break;
    default:
      std::cerr << "nghttp3_conn_close_stream: " << nghttp3_strerror(rv)
                << std::endl;
      ngtcp2_ccerr_set_application_error(
        &last_error_, nghttp3_err_infer_quic_app_error_code(rv), nullptr, 0);
      return -1;
    }
  }

  return 0;
}

void Handler::shutdown_read(int64_t stream_id, uint64_t app_error_code) {
  ngtcp2_conn_shutdown_stream_read(conn_, 0, stream_id, app_error_code);
}

namespace {
void sreadcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto ep = static_cast<Endpoint *>(w->data);

  ep->server->on_read(*ep);
}
} // namespace

namespace {
void siginthandler(struct ev_loop *loop, ev_signal *watcher, int revents) {
  ev_break(loop, EVBREAK_ALL);
}
} // namespace

Server::Server(struct ev_loop *loop, TLSServerContext &tls_ctx)
  : loop_(loop),
    tls_ctx_(tls_ctx),
    stateless_reset_bucket_(NGTCP2_STATELESS_RESET_BURST) {
  ev_signal_init(&sigintev_, siginthandler, SIGINT);

  ev_timer_init(
    &stateless_reset_regen_timer_,
    [](struct ev_loop *loop, ev_timer *w, int revents) {
      auto server = static_cast<Server *>(w->data);

      server->on_stateless_reset_regen();
    },
    0., 1.);
  stateless_reset_regen_timer_.data = this;
}

Server::~Server() {
  disconnect();
  close();
}

void Server::disconnect() {
  config.tx_loss_prob = 0;

  for (auto &ep : endpoints_) {
    ev_io_stop(loop_, &ep.rev);
  }

  ev_timer_stop(loop_, &stateless_reset_regen_timer_);
  ev_signal_stop(loop_, &sigintev_);

  while (!handlers_.empty()) {
    auto it = std::ranges::begin(handlers_);
    auto &h = (*it).second;

    h->handle_error();

    remove(h);
  }
}

void Server::close() {
  for (auto &ep : endpoints_) {
    ::close(ep.fd);
  }

  endpoints_.clear();
}

namespace {
int create_sock(Address &local_addr, const char *addr, const char *port,
                int family) {
  addrinfo hints{
    .ai_flags = AI_PASSIVE,
    .ai_family = family,
    .ai_socktype = SOCK_DGRAM,
  };
  addrinfo *res, *rp;
  int val = 1;

  if (strcmp(addr, "*") == 0) {
    addr = nullptr;
  }

  if (auto rv = getaddrinfo(addr, port, &hints, &res); rv != 0) {
    std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
    return -1;
  }

  auto res_d = defer(freeaddrinfo, res);

  int fd = -1;

  for (rp = res; rp; rp = rp->ai_next) {
    fd = util::create_nonblock_socket(rp->ai_family, rp->ai_socktype,
                                      rp->ai_protocol);
    if (fd == -1) {
      continue;
    }

    if (rp->ai_family == AF_INET6) {
      if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
                     static_cast<socklen_t>(sizeof(val))) == -1) {
        close(fd);
        continue;
      }

      if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val,
                     static_cast<socklen_t>(sizeof(val))) == -1) {
        close(fd);
        continue;
      }
    } else if (setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &val,
                          static_cast<socklen_t>(sizeof(val))) == -1) {
      close(fd);
      continue;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      close(fd);
      continue;
    }

    fd_set_recv_ecn(fd, rp->ai_family);
    fd_set_ip_mtu_discover(fd, rp->ai_family);
    fd_set_ip_dontfrag(fd, family);
    fd_set_udp_gro(fd);

    if (bind(fd, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    close(fd);
  }

  if (!rp) {
    std::cerr << "Could not bind" << std::endl;
    return -1;
  }

  socklen_t len = sizeof(local_addr.su.storage);
  if (getsockname(fd, &local_addr.su.sa, &len) == -1) {
    std::cerr << "getsockname: " << strerror(errno) << std::endl;
    close(fd);
    return -1;
  }
  local_addr.len = len;
  local_addr.ifindex = 0;

  return fd;
}

} // namespace

namespace {
int add_endpoint(std::vector<Endpoint> &endpoints, const char *addr,
                 const char *port, int af) {
  Address dest;
  auto fd = create_sock(dest, addr, port, af);
  if (fd == -1) {
    return -1;
  }

  endpoints.emplace_back();
  auto &ep = endpoints.back();
  ep.addr = dest;
  ep.fd = fd;
  ev_io_init(&ep.rev, sreadcb, 0, EV_READ);

  return 0;
}
} // namespace

namespace {
int add_endpoint(std::vector<Endpoint> &endpoints, const Address &addr) {
  auto fd = util::create_nonblock_socket(addr.su.sa.sa_family, SOCK_DGRAM, 0);
  if (fd == -1) {
    std::cerr << "socket: " << strerror(errno) << std::endl;
    return -1;
  }

  int val = 1;
  if (addr.su.sa.sa_family == AF_INET6) {
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      std::cerr << "setsockopt: " << strerror(errno) << std::endl;
      close(fd);
      return -1;
    }

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      std::cerr << "setsockopt: " << strerror(errno) << std::endl;
      close(fd);
      return -1;
    }
  } else if (setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &val,
                        static_cast<socklen_t>(sizeof(val))) == -1) {
    std::cerr << "setsockopt: " << strerror(errno) << std::endl;
    close(fd);
    return -1;
  }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
                 static_cast<socklen_t>(sizeof(val))) == -1) {
    close(fd);
    return -1;
  }

  fd_set_recv_ecn(fd, addr.su.sa.sa_family);
  fd_set_ip_mtu_discover(fd, addr.su.sa.sa_family);
  fd_set_ip_dontfrag(fd, addr.su.sa.sa_family);
  fd_set_udp_gro(fd);

  if (bind(fd, &addr.su.sa, addr.len) == -1) {
    std::cerr << "bind: " << strerror(errno) << std::endl;
    close(fd);
    return -1;
  }

  endpoints.emplace_back(Endpoint{});
  auto &ep = endpoints.back();
  ep.addr = addr;
  ep.fd = fd;
  ev_io_init(&ep.rev, sreadcb, 0, EV_READ);

  return 0;
}
} // namespace

int Server::init(const char *addr, const char *port) {
  endpoints_.reserve(4);

  auto ready = false;
  if (!util::numeric_host(addr, AF_INET6) &&
      add_endpoint(endpoints_, addr, port, AF_INET) == 0) {
    ready = true;
  }
  if (!util::numeric_host(addr, AF_INET) &&
      add_endpoint(endpoints_, addr, port, AF_INET6) == 0) {
    ready = true;
  }
  if (!ready) {
    return -1;
  }

  if (config.preferred_ipv4_addr.len &&
      add_endpoint(endpoints_, config.preferred_ipv4_addr) != 0) {
    return -1;
  }
  if (config.preferred_ipv6_addr.len &&
      add_endpoint(endpoints_, config.preferred_ipv6_addr) != 0) {
    return -1;
  }

  for (auto &ep : endpoints_) {
    ep.server = this;
    ep.rev.data = &ep;

    ev_io_set(&ep.rev, ep.fd, EV_READ);

    ev_io_start(loop_, &ep.rev);
  }

  ev_signal_start(loop_, &sigintev_);

  return 0;
}

int Server::on_read(const Endpoint &ep) {
  sockaddr_union su;
  std::array<uint8_t, 64_k> buf;
  size_t pktcnt = 0;
  ngtcp2_pkt_info pi;

  iovec msg_iov{
    .iov_base = buf.data(),
    .iov_len = buf.size(),
  };

  uint8_t msg_ctrl[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(in6_pktinfo)) +
                   CMSG_SPACE(sizeof(int))];

  msghdr msg{
    .msg_name = &su,
    .msg_iov = &msg_iov,
    .msg_iovlen = 1,
    .msg_control = msg_ctrl,
  };

  for (; pktcnt < 10;) {
    msg.msg_namelen = sizeof(su);
    msg.msg_controllen = sizeof(msg_ctrl);

    auto nread = recvmsg(ep.fd, &msg, 0);
    if (nread == -1) {
      if (!(errno == EAGAIN || errno == ENOTCONN)) {
        std::cerr << "recvmsg: " << strerror(errno) << std::endl;
      }
      return 0;
    }

    // Packets less than 21 bytes never be a valid QUIC packet.
    if (nread < 21) {
      ++pktcnt;

      continue;
    }

    if (util::prohibited_port(util::port(&su))) {
      ++pktcnt;

      continue;
    }

    pi.ecn = msghdr_get_ecn(&msg, su.storage.ss_family);
    auto local_addr = msghdr_get_local_addr(&msg, su.storage.ss_family);
    if (!local_addr) {
      ++pktcnt;
      std::cerr << "Unable to obtain local address" << std::endl;
      continue;
    }

    auto gso_size = msghdr_get_udp_gro(&msg);
    if (gso_size == 0) {
      gso_size = static_cast<size_t>(nread);
    }

    set_port(*local_addr, ep.addr);

    auto data = std::span{buf.data(), static_cast<size_t>(nread)};

    for (; !data.empty();) {
      auto datalen = std::min(data.size(), gso_size);

      ++pktcnt;

      if (!config.quiet) {
        std::array<char, IF_NAMESIZE> ifname;
        std::cerr << "Received packet: local="
                  << util::straddr(&local_addr->su.sa, local_addr->len)
                  << " remote=" << util::straddr(&su.sa, msg.msg_namelen)
                  << " if="
                  << if_indextoname(local_addr->ifindex, ifname.data())
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
      } else {
        read_pkt(ep, *local_addr, &su.sa, msg.msg_namelen, &pi,
                 {data.data(), datalen});
      }

      data = data.subspan(datalen);
    }
  }

  return 0;
}

void Server::read_pkt(const Endpoint &ep, const Address &local_addr,
                      const sockaddr *sa, socklen_t salen,
                      const ngtcp2_pkt_info *pi,
                      std::span<const uint8_t> data) {
  ngtcp2_version_cid vc;

  switch (auto rv = ngtcp2_pkt_decode_version_cid(&vc, data.data(), data.size(),
                                                  NGTCP2_SV_SCIDLEN);
          rv) {
  case 0:
    break;
  case NGTCP2_ERR_VERSION_NEGOTIATION:
    send_version_negotiation(vc.version, {vc.scid, vc.scidlen},
                             {vc.dcid, vc.dcidlen}, ep, local_addr, sa, salen);
    return;
  default:
    std::cerr << "Could not decode version and CID from QUIC packet header: "
              << ngtcp2_strerror(rv) << std::endl;
    return;
  }

  auto dcid_key = util::make_cid_key({vc.dcid, vc.dcidlen});

  auto handler_it = handlers_.find(dcid_key);
  if (handler_it == std::ranges::end(handlers_)) {
    ngtcp2_pkt_hd hd;

    if (auto rv = ngtcp2_accept(&hd, data.data(), data.size()); rv != 0) {
      if (!config.quiet) {
        std::cerr << "Unexpected packet received: length=" << data.size()
                  << std::endl;
      }

      if (!(data[0] & 0x80) && data.size() >= NGTCP2_SV_SCIDLEN + 21) {
        send_stateless_reset(data.size(), {vc.dcid, vc.dcidlen}, ep, local_addr,
                             sa, salen);
      }

      return;
    }

    ngtcp2_cid ocid;
    ngtcp2_cid *pocid = nullptr;
    ngtcp2_token_type token_type = NGTCP2_TOKEN_TYPE_UNKNOWN;

    assert(hd.type == NGTCP2_PKT_INITIAL);

    if (config.validate_addr || hd.tokenlen) {
      std::cerr << "Perform stateless address validation" << std::endl;
      if (hd.tokenlen == 0) {
        send_retry(&hd, ep, local_addr, sa, salen, data.size() * 3);
        return;
      }

      if (hd.token[0] != NGTCP2_CRYPTO_TOKEN_MAGIC_RETRY2 &&
          hd.dcid.datalen < NGTCP2_MIN_INITIAL_DCIDLEN) {
        send_stateless_connection_close(&hd, ep, local_addr, sa, salen);
        return;
      }

      switch (hd.token[0]) {
      case NGTCP2_CRYPTO_TOKEN_MAGIC_RETRY2:
        switch (verify_retry_token(&ocid, &hd, sa, salen)) {
        case 0:
          pocid = &ocid;
          token_type = NGTCP2_TOKEN_TYPE_RETRY;
          break;
        case -1:
          send_stateless_connection_close(&hd, ep, local_addr, sa, salen);
          return;
        case 1:
          hd.token = nullptr;
          hd.tokenlen = 0;
          break;
        }

        break;
      case NGTCP2_CRYPTO_TOKEN_MAGIC_REGULAR:
        if (verify_token(&hd, sa, salen) != 0) {
          if (config.validate_addr) {
            send_retry(&hd, ep, local_addr, sa, salen, data.size() * 3);
            return;
          }

          hd.token = nullptr;
          hd.tokenlen = 0;
        } else {
          token_type = NGTCP2_TOKEN_TYPE_NEW_TOKEN;
        }
        break;
      default:
        if (!config.quiet) {
          std::cerr << "Ignore unrecognized token" << std::endl;
        }
        if (config.validate_addr) {
          send_retry(&hd, ep, local_addr, sa, salen, data.size() * 3);
          return;
        }

        hd.token = nullptr;
        hd.tokenlen = 0;
        break;
      }
    }

    auto h = std::make_unique<Handler>(loop_, this);
    if (h->init(ep, local_addr, sa, salen, &hd.scid, &hd.dcid, pocid,
                {hd.token, hd.tokenlen}, token_type, hd.version,
                tls_ctx_) != 0) {
      return;
    }

    switch (h->on_read(ep, local_addr, sa, salen, pi, data)) {
    case 0:
      break;
    case NETWORK_ERR_RETRY:
      send_retry(&hd, ep, local_addr, sa, salen, data.size() * 3);
      return;
    default:
      return;
    }

    if (h->on_write() != 0) {
      return;
    }

    std::array<ngtcp2_cid, 8> scids;
    auto conn = h->conn();

    auto num_scid = ngtcp2_conn_get_scid(conn, nullptr);

    assert(num_scid <= scids.size());

    ngtcp2_conn_get_scid(conn, scids.data());

    for (size_t i = 0; i < num_scid; ++i) {
      associate_cid(&scids[i], h.get());
    }

    handlers_.emplace(dcid_key, h.release());

    return;
  }

  auto h = (*handler_it).second;
  auto conn = h->conn();
  if (ngtcp2_conn_in_closing_period(conn)) {
    if (h->send_conn_close(ep, local_addr, sa, salen, pi, data) != 0) {
      remove(h);
    }
    return;
  }
  if (ngtcp2_conn_in_draining_period(conn)) {
    return;
  }

  if (auto rv = h->on_read(ep, local_addr, sa, salen, pi, data); rv != 0) {
    if (rv != NETWORK_ERR_CLOSE_WAIT) {
      remove(h);
    }
    return;
  }

  h->signal_write();
}

namespace {
uint32_t generate_reserved_version(const sockaddr *sa, socklen_t salen,
                                   uint32_t version) {
  uint32_t h = 0x811C9DC5u;
  const uint8_t *p = (const uint8_t *)sa;
  const uint8_t *ep = p + salen;
  for (; p != ep; ++p) {
    h ^= *p;
    h *= 0x01000193u;
  }
  version = htonl(version);
  p = (const uint8_t *)&version;
  ep = p + sizeof(version);
  for (; p != ep; ++p) {
    h ^= *p;
    h *= 0x01000193u;
  }
  h &= 0xf0f0f0f0u;
  h |= 0x0a0a0a0au;
  return h;
}
} // namespace

int Server::send_version_negotiation(uint32_t version,
                                     std::span<const uint8_t> dcid,
                                     std::span<const uint8_t> scid,
                                     const Endpoint &ep,
                                     const Address &local_addr,
                                     const sockaddr *sa, socklen_t salen) {
  Buffer buf{NGTCP2_MAX_UDP_PAYLOAD_SIZE};
  std::array<uint32_t, 1 + max_preferred_versionslen> sv;

  auto p = std::ranges::begin(sv);

  *p++ = generate_reserved_version(sa, salen, version);

  if (config.preferred_versions.empty()) {
    *p++ = NGTCP2_PROTO_VER_V1;
  } else {
    for (auto v : config.preferred_versions) {
      *p++ = v;
    }
  }

  auto nwrite = ngtcp2_pkt_write_version_negotiation(
    buf.wpos(), buf.left(), std::uniform_int_distribution<uint8_t>()(randgen),
    dcid.data(), dcid.size(), scid.data(), scid.size(), sv.data(),
    as_unsigned(p - std::ranges::begin(sv)));
  if (nwrite < 0) {
    std::cerr << "ngtcp2_pkt_write_version_negotiation: "
              << ngtcp2_strerror(static_cast<int>(nwrite)) << std::endl;
    return -1;
  }

  buf.push(as_unsigned(nwrite));

  ngtcp2_addr laddr{
    .addr = const_cast<sockaddr *>(&local_addr.su.sa),
    .addrlen = local_addr.len,
  };
  ngtcp2_addr raddr{
    .addr = const_cast<sockaddr *>(sa),
    .addrlen = salen,
  };

  if (send_packet(ep, laddr, raddr, /* ecn = */ 0, buf.data()) !=
      NETWORK_ERR_OK) {
    return -1;
  }

  return 0;
}

int Server::send_retry(const ngtcp2_pkt_hd *chd, const Endpoint &ep,
                       const Address &local_addr, const sockaddr *sa,
                       socklen_t salen, size_t max_pktlen) {
  std::array<char, NI_MAXHOST> host;
  std::array<char, NI_MAXSERV> port;

  if (auto rv = getnameinfo(sa, salen, host.data(), host.size(), port.data(),
                            port.size(), NI_NUMERICHOST | NI_NUMERICSERV);
      rv != 0) {
    std::cerr << "getnameinfo: " << gai_strerror(rv) << std::endl;
    return -1;
  }

  if (!config.quiet) {
    std::cerr << "Sending Retry packet to [" << host.data()
              << "]:" << port.data() << std::endl;
  }

  ngtcp2_cid scid;

  scid.datalen = NGTCP2_SV_SCIDLEN;
  if (util::generate_secure_random({scid.data, scid.datalen}) != 0) {
    return -1;
  }

  std::array<uint8_t, NGTCP2_CRYPTO_MAX_RETRY_TOKENLEN2> token;

  auto t = util::system_clock_now();

  auto tokenlen = ngtcp2_crypto_generate_retry_token2(
    token.data(), config.static_secret.data(), config.static_secret.size(),
    chd->version, sa, salen, &scid, &chd->dcid, t);
  if (tokenlen < 0) {
    return -1;
  }

  if (!config.quiet) {
    std::cerr << "Generated address validation token:" << std::endl;
    util::hexdump(stderr, token.data(), as_unsigned(tokenlen));
  }

  Buffer buf{
    std::min(static_cast<size_t>(NGTCP2_MAX_UDP_PAYLOAD_SIZE), max_pktlen)};

  auto nwrite = ngtcp2_crypto_write_retry(buf.wpos(), buf.left(), chd->version,
                                          &chd->scid, &scid, &chd->dcid,
                                          token.data(), as_unsigned(tokenlen));
  if (nwrite < 0) {
    std::cerr << "ngtcp2_crypto_write_retry failed" << std::endl;
    return -1;
  }

  buf.push(as_unsigned(nwrite));

  ngtcp2_addr laddr{
    .addr = const_cast<sockaddr *>(&local_addr.su.sa),
    .addrlen = local_addr.len,
  };
  ngtcp2_addr raddr{
    .addr = const_cast<sockaddr *>(sa),
    .addrlen = salen,
  };

  if (send_packet(ep, laddr, raddr, /* ecn = */ 0, buf.data()) !=
      NETWORK_ERR_OK) {
    return -1;
  }

  return 0;
}

int Server::send_stateless_connection_close(const ngtcp2_pkt_hd *chd,
                                            const Endpoint &ep,
                                            const Address &local_addr,
                                            const sockaddr *sa,
                                            socklen_t salen) {
  Buffer buf{NGTCP2_MAX_UDP_PAYLOAD_SIZE};

  auto nwrite = ngtcp2_crypto_write_connection_close(
    buf.wpos(), buf.left(), chd->version, &chd->scid, &chd->dcid,
    NGTCP2_INVALID_TOKEN, nullptr, 0);
  if (nwrite < 0) {
    std::cerr << "ngtcp2_crypto_write_connection_close failed" << std::endl;
    return -1;
  }

  buf.push(as_unsigned(nwrite));

  ngtcp2_addr laddr{
    .addr = const_cast<sockaddr *>(&local_addr.su.sa),
    .addrlen = local_addr.len,
  };
  ngtcp2_addr raddr{
    .addr = const_cast<sockaddr *>(sa),
    .addrlen = salen,
  };

  if (send_packet(ep, laddr, raddr, /* ecn = */ 0, buf.data()) !=
      NETWORK_ERR_OK) {
    return -1;
  }

  return 0;
}

int Server::send_stateless_reset(size_t pktlen, std::span<const uint8_t> dcid,
                                 const Endpoint &ep, const Address &local_addr,
                                 const sockaddr *sa, socklen_t salen) {
  if (stateless_reset_bucket_ == 0) {
    return 0;
  }

  --stateless_reset_bucket_;

  if (!ev_is_active(&stateless_reset_regen_timer_)) {
    ev_timer_again(loop_, &stateless_reset_regen_timer_);
  }

  ngtcp2_cid cid;

  ngtcp2_cid_init(&cid, dcid.data(), dcid.size());

  std::array<uint8_t, NGTCP2_STATELESS_RESET_TOKENLEN> token;

  if (ngtcp2_crypto_generate_stateless_reset_token(
        token.data(), config.static_secret.data(), config.static_secret.size(),
        &cid) != 0) {
    return -1;
  }

  // SCID + minimum expansion - NGTCP2_STATELESS_RESET_TOKENLEN
  constexpr size_t max_rand_byteslen =
    NGTCP2_MAX_CIDLEN + 22 - NGTCP2_STATELESS_RESET_TOKENLEN;

  size_t rand_byteslen;

  if (pktlen <= 43) {
    // As per
    // https://datatracker.ietf.org/doc/html/rfc9000#section-10.3
    rand_byteslen = pktlen - NGTCP2_STATELESS_RESET_TOKENLEN - 1;
  } else {
    rand_byteslen = max_rand_byteslen;
  }

  std::array<uint8_t, max_rand_byteslen> rand_bytes;

  if (util::generate_secure_random({rand_bytes.data(), rand_byteslen}) != 0) {
    return -1;
  }

  Buffer buf{NGTCP2_MAX_UDP_PAYLOAD_SIZE};

  auto nwrite = ngtcp2_pkt_write_stateless_reset(
    buf.wpos(), buf.left(), token.data(), rand_bytes.data(), rand_byteslen);
  if (nwrite < 0) {
    std::cerr << "ngtcp2_pkt_write_stateless_reset: "
              << ngtcp2_strerror(static_cast<int>(nwrite)) << std::endl;

    return -1;
  }

  buf.push(as_unsigned(nwrite));

  ngtcp2_addr laddr{
    .addr = const_cast<sockaddr *>(&local_addr.su.sa),
    .addrlen = local_addr.len,
  };
  ngtcp2_addr raddr{
    .addr = const_cast<sockaddr *>(sa),
    .addrlen = salen,
  };

  if (send_packet(ep, laddr, raddr, /* ecn = */ 0, buf.data()) !=
      NETWORK_ERR_OK) {
    return -1;
  }

  return 0;
}

int Server::verify_retry_token(ngtcp2_cid *ocid, const ngtcp2_pkt_hd *hd,
                               const sockaddr *sa, socklen_t salen) {
  int rv;

  if (!config.quiet) {
    std::array<char, NI_MAXHOST> host;
    std::array<char, NI_MAXSERV> port;

    if (auto rv = getnameinfo(sa, salen, host.data(), host.size(), port.data(),
                              port.size(), NI_NUMERICHOST | NI_NUMERICSERV);
        rv != 0) {
      std::cerr << "getnameinfo: " << gai_strerror(rv) << std::endl;
      return -1;
    }

    std::cerr << "Verifying Retry token from [" << host.data()
              << "]:" << port.data() << std::endl;
    util::hexdump(stderr, hd->token, hd->tokenlen);
  }

  auto t = util::system_clock_now();

  rv = ngtcp2_crypto_verify_retry_token2(
    ocid, hd->token, hd->tokenlen, config.static_secret.data(),
    config.static_secret.size(), hd->version, sa, salen, &hd->dcid,
    10 * NGTCP2_SECONDS, t);
  switch (rv) {
  case 0:
    break;
  case NGTCP2_CRYPTO_ERR_VERIFY_TOKEN:
    std::cerr << "Could not verify Retry token" << std::endl;

    return -1;
  default:
    std::cerr << "Could not read Retry token.  Continue without the token"
              << std::endl;

    return 1;
  }

  if (!config.quiet) {
    std::cerr << "Token was successfully validated" << std::endl;
  }

  return 0;
}

int Server::verify_token(const ngtcp2_pkt_hd *hd, const sockaddr *sa,
                         socklen_t salen) {
  std::array<char, NI_MAXHOST> host;
  std::array<char, NI_MAXSERV> port;

  if (auto rv = getnameinfo(sa, salen, host.data(), host.size(), port.data(),
                            port.size(), NI_NUMERICHOST | NI_NUMERICSERV);
      rv != 0) {
    std::cerr << "getnameinfo: " << gai_strerror(rv) << std::endl;
    return -1;
  }

  if (!config.quiet) {
    std::cerr << "Verifying token from [" << host.data() << "]:" << port.data()
              << std::endl;
    util::hexdump(stderr, hd->token, hd->tokenlen);
  }

  auto t = util::system_clock_now();

  if (ngtcp2_crypto_verify_regular_token(hd->token, hd->tokenlen,
                                         config.static_secret.data(),
                                         config.static_secret.size(), sa, salen,
                                         3600 * NGTCP2_SECONDS, t) != 0) {
    if (!config.quiet) {
      std::cerr << "Could not verify token" << std::endl;
    }
    return -1;
  }

  if (!config.quiet) {
    std::cerr << "Token was successfully validated" << std::endl;
  }

  return 0;
}

int Server::send_packet(const Endpoint &ep, const ngtcp2_addr &local_addr,
                        const ngtcp2_addr &remote_addr, unsigned int ecn,
                        std::span<const uint8_t> data) {
  auto no_gso = false;
  auto [_, rv] =
    send_packet(ep, no_gso, local_addr, remote_addr, ecn, data, data.size());

  return rv;
}

std::pair<std::span<const uint8_t>, int>
Server::send_packet(const Endpoint &ep, bool &no_gso,
                    const ngtcp2_addr &local_addr,
                    const ngtcp2_addr &remote_addr, unsigned int ecn,
                    std::span<const uint8_t> data, size_t gso_size) {
  assert(gso_size);

  if (debug::packet_lost(config.tx_loss_prob)) {
    if (!config.quiet) {
      std::cerr << "** Simulated outgoing packet loss **" << std::endl;
    }
    return {{}, NETWORK_ERR_OK};
  }

  if (no_gso && data.size() > gso_size) {
    for (; !data.empty();) {
      auto len = std::min(gso_size, data.size());

      auto [_, rv] = send_packet(ep, no_gso, local_addr, remote_addr, ecn,
                                 data.first(len), len);
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

  uint8_t msg_ctrl[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(uint16_t)) +
                   CMSG_SPACE(sizeof(in6_pktinfo))]{};

  msghdr msg{
    .msg_name = const_cast<sockaddr *>(remote_addr.addr),
    .msg_namelen = remote_addr.addrlen,
    .msg_iov = &msg_iov,
    .msg_iovlen = 1,
    .msg_control = msg_ctrl,
    .msg_controllen = sizeof(msg_ctrl),
  };

  size_t controllen = 0;

  auto cm = CMSG_FIRSTHDR(&msg);

  switch (local_addr.addr->sa_family) {
  case AF_INET: {
    controllen += CMSG_SPACE(sizeof(in_pktinfo));
    cm->cmsg_level = IPPROTO_IP;
    cm->cmsg_type = IP_PKTINFO;
    cm->cmsg_len = CMSG_LEN(sizeof(in_pktinfo));
    auto addrin = reinterpret_cast<sockaddr_in *>(local_addr.addr);
    in_pktinfo pktinfo{
      .ipi_spec_dst = addrin->sin_addr,
    };
    memcpy(CMSG_DATA(cm), &pktinfo, sizeof(pktinfo));

    break;
  }
  case AF_INET6: {
    controllen += CMSG_SPACE(sizeof(in6_pktinfo));
    cm->cmsg_level = IPPROTO_IPV6;
    cm->cmsg_type = IPV6_PKTINFO;
    cm->cmsg_len = CMSG_LEN(sizeof(in6_pktinfo));
    auto addrin = reinterpret_cast<sockaddr_in6 *>(local_addr.addr);
    in6_pktinfo pktinfo{
      .ipi6_addr = addrin->sin6_addr,
    };
    memcpy(CMSG_DATA(cm), &pktinfo, sizeof(pktinfo));

    break;
  }
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

  controllen += CMSG_SPACE(sizeof(int));
  cm = CMSG_NXTHDR(&msg, cm);
  cm->cmsg_len = CMSG_LEN(sizeof(int));
  memcpy(CMSG_DATA(cm), &ecn, sizeof(ecn));

  switch (local_addr.addr->sa_family) {
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

        no_gso = true;

        return send_packet(ep, no_gso, local_addr, remote_addr, ecn, data,
                           gso_size);
      }
      break;
#endif // defined(UDP_SEGMENT)
    }

    std::cerr << "sendmsg: " << strerror(errno) << std::endl;
    // TODO We have packet which is expected to fail to send (e.g.,
    // path validation to old path).
    return {{}, NETWORK_ERR_OK};
  }

  if (!config.quiet) {
    std::cerr << "Sent packet: local="
              << util::straddr(local_addr.addr, local_addr.addrlen)
              << " remote="
              << util::straddr(remote_addr.addr, remote_addr.addrlen)
              << " ecn=0x" << std::hex << ecn << std::dec << " " << nwrite
              << " bytes" << std::endl;
  }

  return {{}, NETWORK_ERR_OK};
}

void Server::associate_cid(const ngtcp2_cid *cid, Handler *h) {
  handlers_.emplace(*cid, h);
}

void Server::dissociate_cid(const ngtcp2_cid *cid) { handlers_.erase(*cid); }

void Server::remove(const Handler *h) {
  auto conn = h->conn();

  dissociate_cid(ngtcp2_conn_get_client_initial_dcid(conn));

  std::vector<ngtcp2_cid> cids(ngtcp2_conn_get_scid(conn, nullptr));
  ngtcp2_conn_get_scid(conn, cids.data());

  for (auto &cid : cids) {
    dissociate_cid(&cid);
  }

  delete h;
}

void Server::on_stateless_reset_regen() {
  assert(stateless_reset_bucket_ < NGTCP2_STATELESS_RESET_BURST);

  if (++stateless_reset_bucket_ == NGTCP2_STATELESS_RESET_BURST) {
    ev_timer_stop(loop_, &stateless_reset_regen_timer_);
  }
}

namespace {
int parse_host_port(Address &dest, int af, const std::string_view &host_port) {
  if (host_port.empty()) {
    return -1;
  }

  auto first = std::ranges::begin(host_port);
  auto last = std::ranges::end(host_port);

  std::string_view hostv;

  if (*first == '[') {
    ++first;

    auto it = std::ranges::find(first, last, ']');
    if (it == last) {
      return -1;
    }

    hostv = std::string_view{first, it};
    first = it + 1;

    if (first == last || *first != ':') {
      return -1;
    }
  } else {
    auto it = std::ranges::find(first, last, ':');
    if (it == last) {
      return -1;
    }

    hostv = std::string_view{first, it};
    first = it;
  }

  if (++first == last) {
    return -1;
  }

  std::array<char, NI_MAXHOST> host;
  *std::ranges::copy(hostv, std::ranges::begin(host)).out = '\0';

  addrinfo hints{
    .ai_family = af,
    .ai_socktype = SOCK_DGRAM,
  };
  addrinfo *res;
  auto svc = first;

  if (auto rv = getaddrinfo(host.data(), svc, &hints, &res); rv != 0) {
    std::cerr << "getaddrinfo: [" << host.data() << "]:" << svc << ": "
              << gai_strerror(rv) << std::endl;
    return -1;
  }

  dest.len = res->ai_addrlen;
  memcpy(&dest.su, res->ai_addr, res->ai_addrlen);

  freeaddrinfo(res);

  return 0;
}
} // namespace

namespace {
const char *prog = "server";
} // namespace

namespace {
void print_usage() {
  std::cerr << "Usage: " << prog
            << " [OPTIONS] <ADDR> <PORT> <PRIVATE_KEY_FILE> "
               "<CERTIFICATE_FILE>"
            << std::endl;
}
} // namespace

namespace {
void config_set_default(Config &config) {
  auto path = realpath(".", nullptr);
  assert(path);
  auto htdocs = std::string(path);
  free(path);

  config = Config{
    .tx_loss_prob = 0.,
    .rx_loss_prob = 0.,
    .ciphers = util::crypto_default_ciphers(),
    .groups = util::crypto_default_groups(),
    .htdocs = std::move(htdocs),
    .mime_types_file = "/etc/mime.types"sv,
    .timeout = 30 * NGTCP2_SECONDS,
    .max_data = 1_m,
    .max_stream_data_bidi_remote = 256_k,
    .max_stream_data_uni = 256_k,
    .max_streams_bidi = 100,
    .max_streams_uni = 3,
    .max_window = 6_m,
    .max_stream_window = 6_m,
    .max_dyn_length = 20_m,
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
  <ADDR>      Address to listen to.  '*' binds to any address.
  <PORT>      Port
  <PRIVATE_KEY_FILE>
              Path to private key file
  <CERTIFICATE_FILE>
              Path to certificate file
Options:
  -t, --tx-loss=<P>
              The probability of losing outgoing packets.  <P> must be
              [0.0, 1.0],  inclusive.  0.0 means no  packet loss.  1.0
              means 100% packet loss.
  -r, --rx-loss=<P>
              The probability of losing incoming packets.  <P> must be
              [0.0, 1.0],  inclusive.  0.0 means no  packet loss.  1.0
              means 100% packet loss.
  --ciphers=<CIPHERS>
              Specify the cipher suite list to enable.
              Default: )"
            << config.ciphers << R"(
  --groups=<GROUPS>
              Specify the supported groups.
              Default: )"
            << config.groups << R"(
  -d, --htdocs=<PATH>
              Specify document root.  If this option is not specified,
              the document root is the current working directory.
  -q, --quiet Suppress debug output.
  -s, --show-secret
              Print out secrets unless --quiet is used.
  --timeout=<DURATION>
              Specify idle timeout.
              Default: )"
            << util::format_duration(config.timeout) << R"(
  -V, --validate-addr
              Perform address validation.
  --preferred-ipv4-addr=<ADDR>:<PORT>
              Specify preferred IPv4 address and port.
  --preferred-ipv6-addr=<ADDR>:<PORT>
              Specify preferred IPv6 address and port.  A numeric IPv6
              address  must   be  enclosed  by  '['   and  ']'  (e.g.,
              [::1]:8443)
  --mime-types-file=<PATH>
              Path  to file  that contains  MIME media  types and  the
              extensions.
              Default: )"
            << config.mime_types_file << R"(
  --early-response
              Start  sending response  when  it  receives HTTP  header
              fields  without  waiting  for  request  body.   If  HTTP
              response data is written  before receiving request body,
              STOP_SENDING is sent.
  --verify-client
              Request a  client certificate.   At the moment,  we just
              request a certificate and no verification is done.
  --qlog-dir=<PATH>
              Path to  the directory where  qlog file is  stored.  The
              file name  of each qlog  is the Source Connection  ID of
              server.
  --no-quic-dump
              Disables printing QUIC STREAM and CRYPTO frame data out.
  --no-http-dump
              Disables printing HTTP response body out.
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
  --max-dyn-length=<SIZE>
              The maximum length of a dynamically generated content.
              Default: )"
            << util::format_uint_iec(config.max_dyn_length) << R"(
  --cc=(cubic|reno|bbr)
              The name of congestion controller algorithm.
              Default: )"
            << util::strccalgo(config.cc_algo) << R"(
  --initial-rtt=<DURATION>
              Set an initial RTT.
              Default: )"
            << util::format_duration(config.initial_rtt) << R"(
  --max-udp-payload-size=<SIZE>
              Override maximum UDP payload size that server transmits.
              With this  option, server  assumes that a  path supports
              <SIZE> byte of UDP  datagram payload, without performing
              Path MTU Discovery.
  --send-trailers
              Send trailer fields.
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
  --handshake-timeout=<DURATION>
              Set  the  QUIC handshake  timeout.   It  defaults to  no
              timeout.
  --preferred-versions=<HEX>[[,<HEX>]...]
              Specify  QUIC versions  in hex  string in  the order  of
              preference.  Server negotiates one  of those versions if
              client  initially  selects  a  less  preferred  version.
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
  --ech-config-file=<PATH>
              Read private  key and  ECHConfig from |PATH|.   The file
              denoted by |PATH| must contain private key and ECHConfig
              as                      described                     in
              https://datatracker.ietf.org/doc/html/draft-farrell-tls-pemesni.
              ECH configuration  is only applied if  an underlying TLS
              stack supports it.
  --origin=<ORIGIN>
              Specify the origin to send in ORIGIN frame.  Repeat to
              add multiple origins.
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

std::ofstream keylog_file;

int main(int argc, char **argv) {
  config_set_default(config);

  if (argc) {
    prog = basename(argv[0]);
  }

  std::string_view ech_config_file;

  for (;;) {
    static int flag = 0;
    constexpr static option long_opts[] = {
      {"help", no_argument, nullptr, 'h'},
      {"tx-loss", required_argument, nullptr, 't'},
      {"rx-loss", required_argument, nullptr, 'r'},
      {"htdocs", required_argument, nullptr, 'd'},
      {"quiet", no_argument, nullptr, 'q'},
      {"show-secret", no_argument, nullptr, 's'},
      {"validate-addr", no_argument, nullptr, 'V'},
      {"ciphers", required_argument, &flag, 1},
      {"groups", required_argument, &flag, 2},
      {"timeout", required_argument, &flag, 3},
      {"preferred-ipv4-addr", required_argument, &flag, 4},
      {"preferred-ipv6-addr", required_argument, &flag, 5},
      {"mime-types-file", required_argument, &flag, 6},
      {"early-response", no_argument, &flag, 7},
      {"verify-client", no_argument, &flag, 8},
      {"qlog-dir", required_argument, &flag, 9},
      {"no-quic-dump", no_argument, &flag, 10},
      {"no-http-dump", no_argument, &flag, 11},
      {"max-data", required_argument, &flag, 12},
      {"max-stream-data-bidi-local", required_argument, &flag, 13},
      {"max-stream-data-bidi-remote", required_argument, &flag, 14},
      {"max-stream-data-uni", required_argument, &flag, 15},
      {"max-streams-bidi", required_argument, &flag, 16},
      {"max-streams-uni", required_argument, &flag, 17},
      {"max-dyn-length", required_argument, &flag, 18},
      {"cc", required_argument, &flag, 19},
      {"initial-rtt", required_argument, &flag, 20},
      {"max-udp-payload-size", required_argument, &flag, 21},
      {"send-trailers", no_argument, &flag, 22},
      {"max-window", required_argument, &flag, 23},
      {"max-stream-window", required_argument, &flag, 24},
      {"handshake-timeout", required_argument, &flag, 26},
      {"preferred-versions", required_argument, &flag, 27},
      {"available-versions", required_argument, &flag, 28},
      {"no-pmtud", no_argument, &flag, 29},
      {"ack-thresh", required_argument, &flag, 30},
      {"initial-pkt-num", required_argument, &flag, 31},
      {"pmtud-probes", required_argument, &flag, 32},
      {"ech-config-file", required_argument, &flag, 33},
      {"origin", required_argument, &flag, 34},
      {},
    };

    auto optidx = 0;
    auto c = getopt_long(argc, argv, "d:hqr:st:V", long_opts, &optidx);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'd': {
      // --htdocs
      auto path = realpath(optarg, nullptr);
      if (path == nullptr) {
        std::cerr << "path: invalid path " << std::quoted(optarg) << std::endl;
        exit(EXIT_FAILURE);
      }
      config.htdocs = path;
      free(path);
      break;
    }
    case 'h':
      // --help
      print_help();
      exit(EXIT_SUCCESS);
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
    case 'V':
      // --validate-addr
      config.validate_addr = true;
      break;
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
        // --preferred-ipv4-addr
        if (parse_host_port(config.preferred_ipv4_addr, AF_INET, optarg) != 0) {
          std::cerr << "preferred-ipv4-addr: could not use "
                    << std::quoted(optarg) << std::endl;
          exit(EXIT_FAILURE);
        }
        break;
      case 5:
        // --preferred-ipv6-addr
        if (parse_host_port(config.preferred_ipv6_addr, AF_INET6, optarg) !=
            0) {
          std::cerr << "preferred-ipv6-addr: could not use "
                    << std::quoted(optarg) << std::endl;
          exit(EXIT_FAILURE);
        }
        break;
      case 6:
        // --mime-types-file
        config.mime_types_file = optarg;
        break;
      case 7:
        // --early-response
        config.early_response = true;
        break;
      case 8:
        // --verify-client
        config.verify_client = true;
        break;
      case 9:
        // --qlog-dir
        config.qlog_dir = optarg;
        break;
      case 10:
        // --no-quic-dump
        config.no_quic_dump = true;
        break;
      case 11:
        // --no-http-dump
        config.no_http_dump = true;
        break;
      case 12:
        // --max-data
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-data: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_data = *n;
        }
        break;
      case 13:
        // --max-stream-data-bidi-local
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-stream-data-bidi-local: invalid argument"
                    << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_data_bidi_local = *n;
        }
        break;
      case 14:
        // --max-stream-data-bidi-remote
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-stream-data-bidi-remote: invalid argument"
                    << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_data_bidi_remote = *n;
        }
        break;
      case 15:
        // --max-stream-data-uni
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-stream-data-uni: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_data_uni = *n;
        }
        break;
      case 16:
        // --max-streams-bidi
        if (auto n = util::parse_uint(optarg); !n) {
          std::cerr << "max-streams-bidi: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_streams_bidi = *n;
        }
        break;
      case 17:
        // --max-streams-uni
        if (auto n = util::parse_uint(optarg); !n) {
          std::cerr << "max-streams-uni: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_streams_uni = *n;
        }
        break;
      case 18:
        // --max-dyn-length
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-dyn-length: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_dyn_length = *n;
        }
        break;
      case 19:
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
      case 20:
        // --initial-rtt
        if (auto t = util::parse_duration(optarg); !t) {
          std::cerr << "initial-rtt: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.initial_rtt = *t;
        }
        break;
      case 21:
        // --max-udp-payload-size
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-udp-payload-size: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else if (*n > 64_k) {
          std::cerr << "max-udp-payload-size: must not exceed 65536"
                    << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_udp_payload_size = *n;
        }
        break;
      case 22:
        // --send-trailers
        config.send_trailers = true;
        break;
      case 23:
        // --max-window
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-window: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_window = *n;
        }
        break;
      case 24:
        // --max-stream-window
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::cerr << "max-stream-window: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_window = *n;
        }
        break;
      case 26:
        // --handshake-timeout
        if (auto t = util::parse_duration(optarg); !t) {
          std::cerr << "handshake-timeout: invalid argument" << std::endl;
          exit(EXIT_FAILURE);
        } else {
          config.handshake_timeout = *t;
        }
        break;
      case 27: {
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
      case 28: {
        // --available-versions
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
      case 29:
        // --no-pmtud
        config.no_pmtud = true;
        break;
      case 30:
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
      case 31:
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
      case 32: {
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
      case 33:
        // --ech-config-file
        ech_config_file = optarg;
        break;
      case 34: {
        // --origin
        auto origin = std::string_view{optarg};

        if (auto max = std::numeric_limits<uint16_t>::max();
            max < origin.size()) {
          std::cerr << "origin: must be less than or equal to " << max
                    << std::endl;
          exit(EXIT_FAILURE);
        }

        if (!config.origin_list) {
          config.origin_list = std::vector<uint8_t>();
        }

        config.origin_list->push_back(static_cast<uint8_t>(origin.size() >> 8));
        config.origin_list->push_back(origin.size() & 0xff);
        config.origin_list->insert(config.origin_list->end(), origin.begin(),
                                   origin.end());

        break;
      }
      }
      break;
    default:
      break;
    };
  }

  if (argc - optind < 4) {
    std::cerr << "Too few arguments" << std::endl;
    print_usage();
    exit(EXIT_FAILURE);
  }

  auto addr = argv[optind++];
  auto port = argv[optind++];
  auto private_key_file = argv[optind++];
  auto cert_file = argv[optind++];

  if (auto n = util::parse_uint(port); !n) {
    std::cerr << "port: invalid port number" << std::endl;
    exit(EXIT_FAILURE);
  } else if (*n > 65535) {
    std::cerr << "port: must not exceed 65535" << std::endl;
    exit(EXIT_FAILURE);
  } else {
    config.port = static_cast<uint16_t>(*n);
  }

  if (auto mt = util::read_mime_types(config.mime_types_file); !mt) {
    std::cerr << "mime-types-file: Could not read MIME media types file "
              << std::quoted(config.mime_types_file) << std::endl;
  } else {
    config.mime_types = std::move(*mt);
  }

  if (!ech_config_file.empty()) {
    auto ech_config = util::read_ech_server_config(ech_config_file);
    if (!ech_config) {
      std::cerr << "ech-config-file: Could not read private key and ECHConfig"
                << std::endl;
      exit(EXIT_FAILURE);
    }

    config.ech_config = std::move(*ech_config);
  }

  TLSServerContext tls_ctx;

  if (tls_ctx.init(private_key_file, cert_file, AppProtocol::H3) != 0) {
    exit(EXIT_FAILURE);
  }

  if (config.htdocs.back() != '/') {
    config.htdocs += '/';
  }

  std::cerr << "Using document root " << config.htdocs << std::endl;

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

  Server s(EV_DEFAULT, tls_ctx);
  if (s.init(addr, port) != 0) {
    exit(EXIT_FAILURE);
  }

  ev_run(EV_DEFAULT, 0);

  s.disconnect();
  s.close();

  return EXIT_SUCCESS;
}
