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
auto randgen = util::make_mt19937();
} // namespace

Config config;

Stream::Stream(int64_t stream_id, Handler *handler)
  : stream_id{stream_id}, handler{handler} {}

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
  } pri{};
};

namespace {
std::expected<Request, Error> request_path(std::string_view uri,
                                           bool is_connect) {
  urlparse_url u;
  Request req{
    .pri{
      .urgency = -1,
      .inc = -1,
    },
  };

  if (auto rv = urlparse_parse_url(uri.data(), uri.size(), is_connect, &u);
      rv != 0) {
    return std::unexpected{Error::INVALID_ARGUMENT};
  }

  if (u.field_set & (1 << URLPARSE_PATH)) {
    req.path = util::get_string(uri, u, URLPARSE_PATH);
    if (req.path.find('%') != std::string::npos) {
      req.path = util::percent_decode(req.path);
    }

    assert(!req.path.empty());

    if (req.path[0] != '/') {
      return std::unexpected{Error::INVALID_ARGUMENT};
    }

    if (req.path.back() == '/') {
      req.path += "index.html";
    }

    auto maybe_norm_path = util::normalize_path(req.path);
    if (!maybe_norm_path) {
      return std::unexpected{maybe_norm_path.error()};
    }

    req.path = std::move(*maybe_norm_path);
  } else {
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
  uint64_t len{};
  void *map{};
  int fd{};
  uint8_t flags{};
};

namespace {
std::unordered_map<std::string, FileEntry> file_cache;
} // namespace

std::expected<FileEntry, Error> Stream::open_file(const std::string &path) {
  auto it = file_cache.find(path);
  if (it != std::ranges::end(file_cache)) {
    return (*it).second;
  }

  auto fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    return std::unexpected{Error::SYSCALL};
  }

  struct stat st{};
  if (fstat(fd, &st) != 0) {
    close(fd);
    return std::unexpected{Error::SYSCALL};
  }

  FileEntry fe;
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
        std::println(stderr, "mmap: {}", strerror(errno));
        close(fd);
        return std::unexpected{Error::SYSCALL};
      }
    }
  }

  file_cache.emplace(path, fe);

  return fe;
}

void Stream::map_file(const FileEntry &fe) {
  data = static_cast<uint8_t *>(fe.map);
  datalen = fe.len;
}

std::expected<uint64_t, Error> Stream::find_dyn_length(std::string_view path) {
  assert(path[0] == '/');

  if (path.size() == 1) {
    return std::unexpected{Error::INVALID_ARGUMENT};
  }

  uint64_t n = 0;

  for (auto it = std::ranges::begin(path) + 1; it != std::ranges::end(path);
       ++it) {
    if (*it < '0' || '9' < *it) {
      return std::unexpected{Error::INVALID_ARGUMENT};
    }
    auto d = static_cast<uint64_t>(*it - '0');
    if (n > (((1ULL << 62) - 1) - d) / 10) {
      return std::unexpected{Error::INVALID_ARGUMENT};
    }
    n = n * 10 + d;
    if (n > config.max_dyn_length) {
      return std::unexpected{Error::INVALID_ARGUMENT};
    }
  }

  return n;
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
        std::println(stderr, "nghttp3_conn_submit_trailers: {}",
                     nghttp3_strerror(rv));
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

std::expected<void, Error>
Stream::send_status_response(nghttp3_conn *httpconn, unsigned int status_code,
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
    std::println(stderr, "nghttp3_conn_submit_response: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  if (config.send_trailers) {
    auto stream_id_str = util::format_uint(as_unsigned(stream_id));
    auto trailers = std::to_array({
      util::make_nv_nc("x-ngtcp2-stream-id"sv, stream_id_str),
    });

    if (auto rv = nghttp3_conn_submit_trailers(
          httpconn, stream_id, trailers.data(), trailers.size());
        rv != 0) {
      std::println(stderr, "nghttp3_conn_submit_trailers: {}",
                   nghttp3_strerror(rv));
      return std::unexpected{Error::HTTP3};
    }
  }

  handler->shutdown_read(stream_id, NGHTTP3_H3_NO_ERROR);

  return {};
}

std::expected<void, Error>
Stream::send_redirect_response(nghttp3_conn *httpconn, unsigned int status_code,
                               std::string_view path) {
  return send_status_response(httpconn, status_code, {{"location", path}});
}

std::expected<void, Error> Stream::start_response(nghttp3_conn *httpconn) {
  // TODO This should be handled by nghttp3
  if (uri.empty() || method.empty()) {
    return send_status_response(httpconn, 400);
  }

  auto maybe_req = request_path(uri, method == "CONNECT");
  if (!maybe_req) {
    return send_status_response(httpconn, 400);
  }

  const auto &req = *maybe_req;

  uint64_t content_length;
  nghttp3_data_reader dr{};
  auto content_type = "text/plain"sv;

  auto maybe_dyn_len = find_dyn_length(req.path);
  if (!maybe_dyn_len) {
    auto path = config.htdocs + req.path;
    auto maybe_fe = open_file(path);
    if (!maybe_fe) {
      return send_status_response(httpconn, 404);
    }

    const auto &fe = *maybe_fe;

    if (fe.flags & FILE_ENTRY_TYPE_DIR) {
      return send_redirect_response(
        httpconn, 308, path.substr(config.htdocs.size() - 1) + '/');
    }

    content_length = fe.len;

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
    content_length = *maybe_dyn_len;
    dynresp = true;
    dr.read_data = dyn_read_data;

    if (method != "HEAD") {
      datalen = content_length;
      dyndataleft = content_length;
    }

    content_type = "application/octet-stream"sv;
  }

  auto content_length_str = util::format_uint(content_length);

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
      std::println(stderr, "nghttp3_conn_get_stream_priority: {}",
                   nghttp3_strerror(rv));
      return std::unexpected{Error::HTTP3};
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
      std::println(stderr, "nghttp3_conn_set_server_stream_priority: {}",
                   nghttp3_strerror(rv));
      return std::unexpected{Error::HTTP3};
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
    std::println(stderr, "nghttp3_conn_submit_response: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  if (config.send_trailers && !maybe_dyn_len) {
    auto stream_id_str = util::format_uint(as_unsigned(stream_id));
    auto trailers = std::to_array({
      util::make_nv_nc("x-ngtcp2-stream-id"sv, stream_id_str),
    });

    if (auto rv = nghttp3_conn_submit_trailers(
          httpconn, stream_id, trailers.data(), trailers.size());
        rv != 0) {
      std::println(stderr, "nghttp3_conn_submit_trailers: {}",
                   nghttp3_strerror(rv));
      return std::unexpected{Error::HTTP3};
    }
  }

  return {};
}

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  auto h = static_cast<Handler *>(w->data);
  auto s = h->server();

  if (auto rv = h->on_write(); !rv && rv.error() != Error::CLOSE_WAIT) {
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
      std::println(stderr, "Closing Period is over");
    }

    s->remove(h);
    return;
  }
  if (ngtcp2_conn_in_draining_period(conn)) {
    if (!config.quiet) {
      std::println(stderr, "Draining Period is over");
    }

    s->remove(h);
    return;
  }

  assert(0);
}
} // namespace

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto h = static_cast<Handler *>(w->data);
  auto s = h->server();

  if (!config.quiet) {
    std::println(stderr, "Timer expired");
  }

  if (auto rv = h->handle_expiry(); !rv) {
    if (rv.error() == Error::CLOSE_WAIT) {
      ev_timer_stop(loop, w);
    } else {
      s->remove(h);
    }

    return;
  }

  h->signal_write();
}
} // namespace

Handler::Handler(struct ev_loop *loop, Server *server)
  : loop_{loop},
    server_{server},
    no_gso_{
#ifdef UDP_SEGMENT
      config.no_gso
#else  // !defined(UDP_SEGMENT)
      true
#endif // !defined(UDP_SEGMENT)
    } {
  ev_io_init(&wev_, writecb, 0, EV_WRITE);
  wev_.data = this;
  ev_timer_init(&timer_, timeoutcb, 0., 0.);
  timer_.data = this;
}

Handler::~Handler() {
  if (!config.quiet) {
    std::println(stderr, "{} Closing QUIC connection", scid_);
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

  if (!h->handshake_completed()) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}
} // namespace

std::expected<void, Error> Handler::handshake_completed() {
  if (!config.quiet) {
    std::println(stderr, "Negotiated cipher suite is {}",
                 tls_session_.get_cipher_name());
    if (auto group = tls_session_.get_negotiated_group(); !group.empty()) {
      std::println(stderr, "Negotiated group is {}", group);
    }
    std::println(stderr, "Negotiated ALPN is {}",
                 tls_session_.get_selected_alpn());
  }

  if (!tls_session_.send_session_ticket()) {
    std::println(stderr, "Unable to send session ticket");
  }

  std::array<uint8_t, NGTCP2_CRYPTO_MAX_REGULAR_TOKENLEN> token;

  auto path = ngtcp2_conn_get_path(conn_);
  auto t = util::system_clock_now();

  auto tokenlen = ngtcp2_crypto_generate_regular_token(
    token.data(), config.static_secret.data(), config.static_secret.size(),
    path->remote.addr, path->remote.addrlen, t);
  if (tokenlen < 0) {
    std::println(stderr, "Unable to generate token");

    return std::unexpected{Error::QUIC};
  }

  if (auto rv = ngtcp2_conn_submit_new_token(conn_, token.data(),
                                             as_unsigned(tokenlen));
      rv != 0) {
    std::println(stderr, "ngtcp2_conn_submit_new_token: {}",
                 ngtcp2_strerror(rv));

    return std::unexpected{Error::QUIC};
  }

  return {};
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

  if (!h->recv_stream_data(flags, stream_id, {data, datalen})) {
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
  if (!h->acked_stream_data_offset(stream_id, datalen)) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> Handler::acked_stream_data_offset(int64_t stream_id,
                                                             uint64_t datalen) {
  if (!httpconn_) {
    return {};
  }

  if (auto rv = nghttp3_conn_add_ack_offset(httpconn_, stream_id, datalen);
      rv != 0) {
    std::println(stderr, "nghttp3_conn_add_ack_offset: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  return {};
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

  assert(!streams_.contains(stream_id));

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

  if (!h->on_stream_close(stream_id, app_error_code)) {
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
  if (!h->on_stream_reset(stream_id)) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> Handler::on_stream_reset(int64_t stream_id) {
  if (httpconn_) {
    if (auto rv = nghttp3_conn_shutdown_stream_read(httpconn_, stream_id);
        rv != 0) {
      std::println(stderr, "nghttp3_conn_shutdown_stream_read: {}",
                   nghttp3_strerror(rv));
      return std::unexpected{Error::HTTP3};
    }
  }
  return {};
}

namespace {
int stream_stop_sending(ngtcp2_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *user_data,
                        void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (!h->on_stream_stop_sending(stream_id)) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> Handler::on_stream_stop_sending(int64_t stream_id) {
  if (!httpconn_) {
    return {};
  }

  if (auto rv = nghttp3_conn_shutdown_stream_read(httpconn_, stream_id);
      rv != 0) {
    std::println(stderr, "nghttp3_conn_shutdown_stream_read: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  return {};
}

namespace {
void rand_bytes(uint8_t *dest, size_t destlen) {
  if (!util::generate_secure_random({dest, destlen})) {
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
int get_new_connection_id(ngtcp2_conn *conn, ngtcp2_cid *cid,
                          ngtcp2_stateless_reset_token *token, size_t cidlen,
                          void *user_data) {
  if (!util::generate_secure_random({cid->data, cidlen})) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  cid->datalen = cidlen;
  if (ngtcp2_crypto_generate_stateless_reset_token(
        token->data, config.static_secret.data(), config.static_secret.size(),
        cid) != 0) {
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
  if (!h->update_key(rx_secret, tx_secret, rx_aead_ctx, rx_iv, tx_aead_ctx,
                     tx_iv, current_rx_secret, current_tx_secret, secretlen)) {
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
    std::println(stderr, "Unable to generate token");

    return 0;
  }

  if (auto rv =
        ngtcp2_conn_submit_new_token(conn, token.data(), as_unsigned(tokenlen));
      rv != 0) {
    std::println(stderr, "ngtcp2_conn_submit_new_token: {}",
                 ngtcp2_strerror(rv));

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
  if (!h->http_end_request_headers(stream)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> Handler::http_end_request_headers(Stream *stream) {
  if (config.early_response) {
    if (auto rv = start_response(stream); !rv) {
      return rv;
    }

    shutdown_read(stream->stream_id, NGHTTP3_H3_NO_ERROR);
  }
  return {};
}

namespace {
int http_end_stream(nghttp3_conn *conn, int64_t stream_id, void *user_data,
                    void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  auto stream = static_cast<Stream *>(stream_user_data);
  if (!h->http_end_stream(stream)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> Handler::http_end_stream(Stream *stream) {
  if (!config.early_response) {
    return start_response(stream);
  }
  return {};
}

std::expected<void, Error> Handler::start_response(Stream *stream) {
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

void Handler::http_stream_close(int64_t stream_id, uint64_t app_error_code) {
  if (!ngtcp2_is_bidi_stream(stream_id)) {
    return;
  }

  assert(!ngtcp2_conn_is_local_stream(conn_, stream_id));
  ngtcp2_conn_extend_max_streams_bidi(conn_, 1);

  auto it = streams_.find(stream_id);
  if (it == std::ranges::end(streams_)) {
    return;
  }

  if (!config.quiet) {
    std::println(stderr, "HTTP stream {:#x} closed with error code {:#x}",
                 stream_id, app_error_code);
  }

  streams_.erase(it);
}

namespace {
int http_stop_sending(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *user_data,
                      void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (!h->http_stop_sending(stream_id, app_error_code)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> Handler::http_stop_sending(int64_t stream_id,
                                                      uint64_t app_error_code) {
  if (auto rv =
        ngtcp2_conn_shutdown_stream_read(conn_, 0, stream_id, app_error_code);
      rv != 0) {
    std::println(stderr, "ngtcp2_conn_shutdown_stream_read: {}",
                 ngtcp2_strerror(rv));
    return std::unexpected{Error::QUIC};
  }
  return {};
}

namespace {
int http_reset_stream(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *user_data,
                      void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (!h->http_reset_stream(stream_id, app_error_code)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> Handler::http_reset_stream(int64_t stream_id,
                                                      uint64_t app_error_code) {
  if (auto rv =
        ngtcp2_conn_shutdown_stream_write(conn_, 0, stream_id, app_error_code);
      rv != 0) {
    std::println(stderr, "ngtcp2_conn_shutdown_stream_write: {}",
                 ngtcp2_strerror(rv));
    return std::unexpected{Error::QUIC};
  }
  return {};
}

namespace {
int http_recv_settings(nghttp3_conn *conn,
                       const nghttp3_proto_settings *settings,
                       void *conn_user_data) {
  if (!config.quiet) {
    debug::print_http_settings(settings);
  }

  return 0;
}
} // namespace

std::expected<void, Error> Handler::setup_httpconn() {
  if (httpconn_) {
    return {};
  }

  if (ngtcp2_conn_get_streams_uni_left(conn_) < 3) {
    std::println(stderr,
                 "peer does not allow at least 3 unidirectional streams.");
    return std::unexpected{Error::QUIC};
  }

  nghttp3_callbacks callbacks{
    .acked_stream_data = ::http_acked_stream_data,
    .recv_data = ::http_recv_data,
    .deferred_consume = ::http_deferred_consume,
    .begin_headers = ::http_begin_request_headers,
    .recv_header = ::http_recv_request_header,
    .end_headers = ::http_end_request_headers,
    .stop_sending = ::http_stop_sending,
    .end_stream = ::http_end_stream,
    .reset_stream = ::http_reset_stream,
    .rand = rand_bytes,
    .recv_settings2 = ::http_recv_settings,
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
    std::println(stderr, "nghttp3_conn_server_new: {}", nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  auto params = ngtcp2_conn_get_local_transport_params(conn_);

  nghttp3_conn_set_max_client_streams_bidi(httpconn_,
                                           params->initial_max_streams_bidi);

  int64_t ctrl_stream_id;

  if (auto rv = ngtcp2_conn_open_uni_stream(conn_, &ctrl_stream_id, nullptr);
      rv != 0) {
    std::println(stderr, "ngtcp2_conn_open_uni_stream: {}",
                 ngtcp2_strerror(rv));
    return std::unexpected{Error::QUIC};
  }

  if (auto rv = nghttp3_conn_bind_control_stream(httpconn_, ctrl_stream_id);
      rv != 0) {
    std::println(stderr, "nghttp3_conn_bind_control_stream: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  if (!config.quiet) {
    std::println(stderr, "http: control stream={:#x}", ctrl_stream_id);
  }

  int64_t qpack_enc_stream_id, qpack_dec_stream_id;

  if (auto rv =
        ngtcp2_conn_open_uni_stream(conn_, &qpack_enc_stream_id, nullptr);
      rv != 0) {
    std::println(stderr, "ngtcp2_conn_open_uni_stream: {}",
                 ngtcp2_strerror(rv));
    return std::unexpected{Error::QUIC};
  }

  if (auto rv =
        ngtcp2_conn_open_uni_stream(conn_, &qpack_dec_stream_id, nullptr);
      rv != 0) {
    std::println(stderr, "ngtcp2_conn_open_uni_stream: {}",
                 ngtcp2_strerror(rv));
    return std::unexpected{Error::QUIC};
  }

  if (auto rv = nghttp3_conn_bind_qpack_streams(httpconn_, qpack_enc_stream_id,
                                                qpack_dec_stream_id);
      rv != 0) {
    std::println(stderr, "nghttp3_conn_bind_qpack_streams: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  if (!config.quiet) {
    std::println(stderr, "http: QPACK streams encoder={:#x} decoder={:#x}",
                 qpack_enc_stream_id, qpack_dec_stream_id);
  }

  return {};
}

namespace {
int extend_max_stream_data(ngtcp2_conn *conn, int64_t stream_id,
                           uint64_t max_data, void *user_data,
                           void *stream_user_data) {
  auto h = static_cast<Handler *>(user_data);
  if (!h->extend_max_stream_data(stream_id, max_data)) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> Handler::extend_max_stream_data(int64_t stream_id,
                                                           uint64_t max_data) {
  if (auto rv = nghttp3_conn_unblock_stream(httpconn_, stream_id); rv != 0) {
    std::println(stderr, "nghttp3_conn_unblock_stream: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }
  return {};
}

namespace {
int recv_tx_key(ngtcp2_conn *conn, ngtcp2_encryption_level level,
                void *user_data) {
  if (level != NGTCP2_ENCRYPTION_LEVEL_1RTT) {
    return 0;
  }

  auto h = static_cast<Handler *>(user_data);
  if (!h->setup_httpconn()) {
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

std::expected<void, Error>
Handler::init(const Endpoint &ep, const Address &local_addr,
              const Address &remote_addr, const ngtcp2_cid *dcid,
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
    .remove_connection_id = remove_connection_id,
    .update_key = ::update_key,
    .path_validation = path_validation,
    .stream_reset = ::stream_reset,
    .extend_max_remote_streams_bidi = ::extend_max_remote_streams_bidi,
    .extend_max_stream_data = ::extend_max_stream_data,
    .delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    .delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    .stream_stop_sending = stream_stop_sending,
    .version_negotiation = ngtcp2_crypto_version_negotiation_cb,
    .recv_tx_key = ::recv_tx_key,
    .get_new_connection_id2 = get_new_connection_id,
    .get_path_challenge_data2 = ngtcp2_crypto_get_path_challenge_data2_cb,
  };

  scid_.datalen = NGTCP2_SV_SCIDLEN;
  if (auto rv = util::generate_secure_random({scid_.data, scid_.datalen});
      !rv) {
    std::println(stderr, "Could not generate connection ID");
    return rv;
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
      std::println(stderr, "Could not open qlog file {}: {}", path,
                   strerror(errno));
      return std::unexpected{Error::IO};
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
    return std::unexpected{Error::QUIC};
  }

  if (!config.preferred_ipv4_addr.empty() ||
      !config.preferred_ipv6_addr.empty()) {
    params.preferred_addr_present = 1;

    if (!config.preferred_ipv4_addr.empty()) {
      params.preferred_addr.ipv4 =
        std::get<sockaddr_in>(config.preferred_ipv4_addr.skaddr);
      params.preferred_addr.ipv4_present = 1;
    }

    if (!config.preferred_ipv6_addr.empty()) {
      params.preferred_addr.ipv6 =
        std::get<sockaddr_in6>(config.preferred_ipv6_addr.skaddr);
      params.preferred_addr.ipv6_present = 1;
    }

    if (auto rv = util::generate_secure_random(
          params.preferred_addr.stateless_reset_token);
        !rv) {
      std::println(
        stderr, "Could not generate preferred address stateless reset token");
      return rv;
    }

    params.preferred_addr.cid.datalen = NGTCP2_SV_SCIDLEN;
    if (auto rv = util::generate_secure_random(
          {params.preferred_addr.cid.data, params.preferred_addr.cid.datalen});
        !rv) {
      std::println(stderr,
                   "Could not generate preferred address connection ID");
      return rv;
    }
  }

  auto path = ngtcp2_path{
    .local = as_ngtcp2_addr(local_addr),
    .remote = as_ngtcp2_addr(remote_addr),
    .user_data = const_cast<Endpoint *>(&ep),
  };
  if (auto rv =
        ngtcp2_conn_server_new(&conn_, dcid, &scid_, &path, version, &callbacks,
                               &settings, &params, nullptr, this);
      rv != 0) {
    std::println(stderr, "ngtcp2_conn_server_new: {}", ngtcp2_strerror(rv));
    return std::unexpected{Error::QUIC};
  }

  if (auto rv = tls_session_.init(tls_ctx, this); !rv) {
    return rv;
  }

  tls_session_.enable_keylog();

  ngtcp2_conn_set_tls_native_handle(conn_, tls_session_.get_native_handle());

  ev_io_set(&wev_, ep.fd, EV_WRITE);

  return {};
}

std::expected<void, Error> Handler::feed_data(const Endpoint &ep,
                                              const Address &local_addr,
                                              const Address &remote_addr,
                                              const ngtcp2_pkt_info *pi,
                                              std::span<const uint8_t> data) {
  auto path = ngtcp2_path{
    .local = as_ngtcp2_addr(local_addr),
    .remote = as_ngtcp2_addr(remote_addr),
    .user_data = const_cast<Endpoint *>(&ep),
  };

  if (auto rv = ngtcp2_conn_read_pkt(conn_, &path, pi, data.data(), data.size(),
                                     util::timestamp());
      rv != 0) {
    std::println(stderr, "ngtcp2_conn_read_pkt: {}", ngtcp2_strerror(rv));
    switch (rv) {
    case NGTCP2_ERR_DRAINING:
      start_draining_period();
      return std::unexpected{Error::CLOSE_WAIT};
    case NGTCP2_ERR_RETRY:
      return std::unexpected{Error::RETRY_CONN};
    case NGTCP2_ERR_DROP_CONN:
      return std::unexpected{Error::DROP_CONN};
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

  return {};
}

std::expected<void, Error> Handler::on_read(const Endpoint &ep,
                                            const Address &local_addr,
                                            const Address &remote_addr,
                                            const ngtcp2_pkt_info *pi,
                                            std::span<const uint8_t> data) {
  if (auto rv = feed_data(ep, local_addr, remote_addr, pi, data); !rv) {
    return rv;
  }

  update_timer();

  return {};
}

std::expected<void, Error> Handler::handle_expiry() {
  auto now = util::timestamp();
  if (auto rv = ngtcp2_conn_handle_expiry(conn_, now); rv != 0) {
    std::println(stderr, "ngtcp2_conn_handle_expiry: {}", ngtcp2_strerror(rv));
    ngtcp2_ccerr_set_liberr(&last_error_, rv, nullptr, 0);
    return handle_error();
  }

  return {};
}

std::expected<void, Error> Handler::on_write() {
  if (ngtcp2_conn_in_closing_period(conn_) ||
      ngtcp2_conn_in_draining_period(conn_)) {
    return {};
  }

  if (tx_.send_blocked) {
    send_blocked_packet();

    if (tx_.send_blocked) {
      return {};
    }
  }

  ev_io_stop(loop_, &wev_);

  if (auto rv = write_streams(); !rv) {
    return rv;
  }

  update_timer();

  return {};
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
        std::println(stderr, "nghttp3_conn_writev_stream: {}",
                     nghttp3_strerror(static_cast<int>(sveccnt)));
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
          std::println(stderr, "nghttp3_conn_add_write_offset: {}",
                       nghttp3_strerror(rv));
          ngtcp2_ccerr_set_application_error(
            &last_error_, nghttp3_err_infer_quic_app_error_code(rv), nullptr,
            0);
          return NGTCP2_ERR_CALLBACK_FAILURE;
        }
        continue;
      }

      assert(ndatalen == -1);

      std::println(stderr, "ngtcp2_conn_writev_stream: {}",
                   ngtcp2_strerror(static_cast<int>(nwrite)));
      ngtcp2_ccerr_set_liberr(&last_error_, static_cast<int>(nwrite), nullptr,
                              0);
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    if (ndatalen >= 0) {
      if (auto rv = nghttp3_conn_add_write_offset(httpconn_, stream_id,
                                                  as_unsigned(ndatalen));
          rv != 0) {
        std::println(stderr, "nghttp3_conn_add_write_offset: {}",
                     nghttp3_strerror(rv));
        ngtcp2_ccerr_set_application_error(
          &last_error_, nghttp3_err_infer_quic_app_error_code(rv), nullptr, 0);
        return NGTCP2_ERR_CALLBACK_FAILURE;
      }
    }

    return nwrite;
  }
}

std::expected<void, Error> Handler::write_streams() {
  ngtcp2_path_storage ps;
  ngtcp2_pkt_info pi;
  size_t gso_size;
  auto ts = util::timestamp();
  auto txbuf = std::span{txbuf_};
  auto buflen = util::clamp_buffer_size(conn_, txbuf.size(), config.gso_burst);

  ngtcp2_path_storage_zero(&ps);

  auto nwrite = ngtcp2_conn_write_aggregate_pkt2(
    conn_, &ps.path, &pi, txbuf.data(), buflen, &gso_size, ::write_pkt,
    config.gso_burst, ts);
  if (nwrite < 0) {
    return handle_error();
  }

  ngtcp2_conn_update_pkt_tx_time(conn_, ts);

  if (nwrite == 0) {
    return {};
  }

  send_packet(ps.path, pi.ecn, txbuf.first(static_cast<size_t>(nwrite)),
              gso_size);

  return {};
}

std::expected<void, Error> Handler::send_packet(const ngtcp2_path &path,
                                                unsigned int ecn,
                                                std::span<const uint8_t> data,
                                                size_t gso_size) {
  auto &ep = *static_cast<Endpoint *>(path.user_data);
  auto rest = server_->send_packet(ep, no_gso_, path.local, path.remote, ecn,
                                   data, gso_size);
  if (!rest.empty()) {
    on_send_blocked(path, ecn, rest, gso_size);

    start_wev_endpoint(ep);

    return std::unexpected{Error::SEND_BLOCKED};
  }

  return {};
}

void Handler::on_send_blocked(const ngtcp2_path &path, unsigned int ecn,
                              std::span<const uint8_t> data, size_t gso_size) {
  assert(!tx_.send_blocked);
  assert(gso_size);

  tx_.send_blocked = true;

  auto &p = tx_.blocked;

  p.local_addr.set(path.local.addr);
  p.remote_addr.set(path.remote.addr);

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

void Handler::send_blocked_packet() {
  assert(tx_.send_blocked);

  auto &p = tx_.blocked;

  auto rest = server_->send_packet(
    *p.endpoint, no_gso_, as_ngtcp2_addr(p.local_addr),
    as_ngtcp2_addr(p.remote_addr), p.ecn, p.data, p.gso_size);
  if (!rest.empty()) {
    p.data = rest;

    start_wev_endpoint(*p.endpoint);

    return;
  }

  tx_.send_blocked = false;
}

void Handler::signal_write() { ev_io_start(loop_, &wev_); }

void Handler::start_draining_period() {
  ev_io_stop(loop_, &wev_);

  ev_set_cb(&timer_, close_waitcb);
  timer_.repeat =
    static_cast<ev_tstamp>(ngtcp2_conn_get_pto(conn_)) / NGTCP2_SECONDS * 3;
  ev_timer_again(loop_, &timer_);

  if (!config.quiet) {
    std::println(stderr, "Draining period has started ({:.9f} seconds)",
                 timer_.repeat);
  }
}

std::expected<void, Error> Handler::start_closing_period() {
  if (!conn_ || ngtcp2_conn_in_closing_period(conn_) ||
      ngtcp2_conn_in_draining_period(conn_)) {
    return {};
  }

  ev_io_stop(loop_, &wev_);

  ev_set_cb(&timer_, close_waitcb);
  timer_.repeat =
    static_cast<ev_tstamp>(ngtcp2_conn_get_pto(conn_)) / NGTCP2_SECONDS * 3;
  ev_timer_again(loop_, &timer_);

  if (!config.quiet) {
    std::println(stderr, "Closing period has started ({:.9f} seconds)",
                 timer_.repeat);
  }

  conn_closebuf_ = std::make_unique<Buffer>(NGTCP2_MAX_UDP_PAYLOAD_SIZE);

  ngtcp2_path_storage ps;

  ngtcp2_path_storage_zero(&ps);

  ngtcp2_pkt_info pi;
  auto n = ngtcp2_conn_write_connection_close(
    conn_, &ps.path, &pi, conn_closebuf_->wpos(), conn_closebuf_->left(),
    &last_error_, util::timestamp());
  if (n < 0) {
    std::println(stderr, "ngtcp2_conn_write_connection_close: {}",
                 ngtcp2_strerror(static_cast<int>(n)));
    return std::unexpected{Error::QUIC};
  }

  if (n == 0) {
    return {};
  }

  conn_closebuf_->push(as_unsigned(n));

  return {};
}

std::expected<void, Error> Handler::handle_error() {
  if (last_error_.type == NGTCP2_CCERR_TYPE_IDLE_CLOSE) {
    return std::unexpected{Error::INTERNAL};
  }

  if (auto rv = start_closing_period(); !rv) {
    return rv;
  }

  if (ngtcp2_conn_in_draining_period(conn_)) {
    return std::unexpected{Error::CLOSE_WAIT};
  }

  if (auto rv = send_conn_close(); !rv) {
    return rv;
  }

  return std::unexpected{Error::CLOSE_WAIT};
}

std::expected<void, Error> Handler::send_conn_close() {
  if (!config.quiet) {
    std::println(stderr, "Closing Period: TX CONNECTION_CLOSE");
  }

  assert(conn_closebuf_ && conn_closebuf_->size());
  assert(conn_);
  assert(!ngtcp2_conn_in_draining_period(conn_));

  auto path = ngtcp2_conn_get_path(conn_);

  return server_->send_packet(*static_cast<Endpoint *>(path->user_data),
                              path->local, path->remote,
                              /* ecn = */ 0, conn_closebuf_->data());
}

std::expected<void, Error>
Handler::send_conn_close(const Endpoint &ep, const Address &local_addr,
                         const Address &remote_addr, const ngtcp2_pkt_info *pi,
                         std::span<const uint8_t> data) {
  assert(conn_closebuf_ && conn_closebuf_->size());

  close_wait_.bytes_recv += data.size();
  ++close_wait_.num_pkts_recv;

  if (close_wait_.num_pkts_recv < close_wait_.next_pkts_recv ||
      close_wait_.bytes_recv * 3 <
        close_wait_.bytes_sent + conn_closebuf_->size()) {
    return {};
  }

  if (auto rv = server_->send_packet(ep, as_ngtcp2_addr(local_addr),
                                     as_ngtcp2_addr(remote_addr),
                                     /* ecn = */ 0, conn_closebuf_->data());
      !rv) {
    return rv;
  }

  close_wait_.bytes_sent += conn_closebuf_->size();
  close_wait_.next_pkts_recv *= 2;

  return {};
}

void Handler::update_timer() {
  auto expiry = ngtcp2_conn_get_expiry(conn_);
  auto now = util::timestamp();

  if (expiry <= now) {
    if (!config.quiet) {
      auto t = static_cast<ev_tstamp>(now - expiry) / NGTCP2_SECONDS;
      std::println(stderr, "Timer has already expired: {:.9f}s", t);
    }

    ev_feed_event(loop_, &timer_, EV_TIMER);

    return;
  }

  auto t = static_cast<ev_tstamp>(expiry - now) / NGTCP2_SECONDS;
  if (!config.quiet) {
    std::println(stderr, "Set timer={:.9f}s", t);
  }
  timer_.repeat = t;
  ev_timer_again(loop_, &timer_);
}

std::expected<void, Error>
Handler::recv_stream_data(uint32_t flags, int64_t stream_id,
                          std::span<const uint8_t> data) {
  if (!config.quiet && !config.no_quic_dump) {
    debug::print_stream_data(stream_id, data);
  }

  if (!httpconn_) {
    return {};
  }

  auto nconsumed = nghttp3_conn_read_stream2(
    httpconn_, stream_id, data.data(), data.size(),
    flags & NGTCP2_STREAM_DATA_FLAG_FIN, ngtcp2_conn_get_timestamp(conn_));
  if (nconsumed < 0) {
    std::println(stderr, "nghttp3_conn_read_stream2: {}",
                 nghttp3_strerror(static_cast<int>(nconsumed)));
    ngtcp2_ccerr_set_application_error(
      &last_error_,
      nghttp3_err_infer_quic_app_error_code(static_cast<int>(nconsumed)),
      nullptr, 0);
    return std::unexpected{Error::HTTP3};
  }

  ngtcp2_conn_extend_max_stream_offset(conn_, stream_id,
                                       static_cast<uint64_t>(nconsumed));
  ngtcp2_conn_extend_max_offset(conn_, static_cast<uint64_t>(nconsumed));

  return {};
}

std::expected<void, Error>
Handler::update_key(uint8_t *rx_secret, uint8_t *tx_secret,
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
    return std::unexpected{Error::QUIC};
  }

  if (!config.quiet && config.show_secret) {
    std::println(stderr, "application_traffic rx secret {}", nkey_update_);
    debug::print_secrets({rx_secret, secretlen}, {rx_key.data(), keylen},
                         {rx_iv, ivlen});
    std::println(stderr, "application_traffic tx secret {}", nkey_update_);
    debug::print_secrets({tx_secret, secretlen}, {tx_key.data(), keylen},
                         {tx_iv, ivlen});
  }

  return {};
}

Server *Handler::server() const { return server_; }

std::expected<void, Error> Handler::on_stream_close(int64_t stream_id,
                                                    uint64_t app_error_code) {
  if (!config.quiet) {
    std::println(stderr, "QUIC stream {:#x} closed", stream_id);
  }

  if (httpconn_) {
    if (app_error_code == 0) {
      app_error_code = NGHTTP3_H3_NO_ERROR;
    }
    auto rv = nghttp3_conn_close_stream(httpconn_, stream_id, app_error_code);
    switch (rv) {
    case 0:
      http_stream_close(stream_id, app_error_code);
      break;
    case NGHTTP3_ERR_STREAM_NOT_FOUND:
      if (ngtcp2_is_bidi_stream(stream_id)) {
        assert(!ngtcp2_conn_is_local_stream(conn_, stream_id));
        ngtcp2_conn_extend_max_streams_bidi(conn_, 1);
      }
      break;
    default:
      std::println(stderr, "nghttp3_conn_close_stream: {}",
                   nghttp3_strerror(rv));
      ngtcp2_ccerr_set_application_error(
        &last_error_, nghttp3_err_infer_quic_app_error_code(rv), nullptr, 0);
      return std::unexpected{Error::HTTP3};
    }
  }

  return {};
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
  : loop_{loop}, tls_ctx_{tls_ctx} {
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
std::expected<int, Error> create_sock(Address &local_addr, const char *addr,
                                      const char *port, int family) {
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
    std::println(stderr, "getaddrinfo: {}", gai_strerror(rv));
    return std::unexpected{Error::LIBC};
  }

  auto res_d = defer([res] { freeaddrinfo(res); });

  int fd = -1;

  for (rp = res; rp; rp = rp->ai_next) {
    auto maybe_fd = util::create_nonblock_socket(rp->ai_family, rp->ai_socktype,
                                                 rp->ai_protocol);
    if (!maybe_fd) {
      continue;
    }

    fd = *maybe_fd;

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
    std::println(stderr, "Could not bind");
    return std::unexpected{Error::SYSCALL};
  }

  sockaddr_storage ss;
  socklen_t len = sizeof(ss);
  if (getsockname(fd, reinterpret_cast<sockaddr *>(&ss), &len) == -1) {
    std::println(stderr, "getsockname: {}", strerror(errno));
    close(fd);
    return std::unexpected{Error::SYSCALL};
  }

  local_addr.set(reinterpret_cast<const sockaddr *>(&ss));

  return fd;
}

} // namespace

namespace {
std::expected<void, Error> add_endpoint(std::vector<Endpoint> &endpoints,
                                        const char *addr, const char *port,
                                        int af) {
  Address dest;
  auto maybe_fd = create_sock(dest, addr, port, af);
  if (!maybe_fd) {
    return std::unexpected{maybe_fd.error()};
  }

  endpoints.emplace_back();
  auto &ep = endpoints.back();
  ep.addr = dest;
  ep.fd = *maybe_fd;
  ev_io_init(&ep.rev, sreadcb, 0, EV_READ);
  ev_set_priority(&ep.rev, EV_MAXPRI);

  return {};
}
} // namespace

namespace {
std::expected<void, Error> add_endpoint(std::vector<Endpoint> &endpoints,
                                        const Address &addr) {
  auto family = addr.family();

  auto maybe_fd = util::create_nonblock_socket(family, SOCK_DGRAM, 0);
  if (!maybe_fd) {
    std::println(stderr, "socket: {}", strerror(errno));
    return std::unexpected{maybe_fd.error()};
  }

  auto fd = *maybe_fd;

  int val = 1;
  if (family == AF_INET6) {
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      std::println(stderr, "setsockopt: {}", strerror(errno));
      close(fd);
      return std::unexpected{Error::SYSCALL};
    }

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      std::println(stderr, "setsockopt: {}", strerror(errno));
      close(fd);
      return std::unexpected{Error::SYSCALL};
    }
  } else if (setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &val,
                        static_cast<socklen_t>(sizeof(val))) == -1) {
    std::println(stderr, "setsockopt: {}", strerror(errno));
    close(fd);
    return std::unexpected{Error::SYSCALL};
  }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
                 static_cast<socklen_t>(sizeof(val))) == -1) {
    close(fd);
    return std::unexpected{Error::SYSCALL};
  }

  fd_set_recv_ecn(fd, family);
  fd_set_ip_mtu_discover(fd, family);
  fd_set_ip_dontfrag(fd, family);
  fd_set_udp_gro(fd);

  if (bind(fd, addr.as_sockaddr(), addr.size()) == -1) {
    std::println(stderr, "bind: {}", strerror(errno));
    close(fd);
    return std::unexpected{Error::SYSCALL};
  }

  endpoints.emplace_back(Endpoint{});
  auto &ep = endpoints.back();
  ep.addr = addr;
  ep.fd = fd;
  ev_io_init(&ep.rev, sreadcb, 0, EV_READ);
  ev_set_priority(&ep.rev, EV_MAXPRI);

  return {};
}
} // namespace

std::expected<void, Error> Server::init(const char *addr, const char *port) {
  endpoints_.reserve(4);

  auto ready = false;
  auto error = Error::INTERNAL;

  if (!util::numeric_host(addr, AF_INET6)) {
    if (auto rv = add_endpoint(endpoints_, addr, port, AF_INET); !rv) {
      error = rv.error();
    } else {
      ready = true;
    }
  }
  if (!util::numeric_host(addr, AF_INET)) {
    if (auto rv = add_endpoint(endpoints_, addr, port, AF_INET6); !rv) {
      error = rv.error();
    } else {
      ready = true;
    }
  }
  if (!ready) {
    return std::unexpected{error};
  }

  if (!config.preferred_ipv4_addr.empty()) {
    if (auto rv = add_endpoint(endpoints_, config.preferred_ipv4_addr); !rv) {
      return rv;
    }
  }

  if (!config.preferred_ipv6_addr.empty()) {
    if (auto rv = add_endpoint(endpoints_, config.preferred_ipv6_addr); !rv) {
      return rv;
    }
  }

  for (auto &ep : endpoints_) {
    ep.server = this;
    ep.rev.data = &ep;

    ev_io_set(&ep.rev, ep.fd, EV_READ);

    ev_io_start(loop_, &ep.rev);
  }

  ev_signal_start(loop_, &sigintev_);

  return {};
}

void Server::on_read(const Endpoint &ep) {
  sockaddr_storage ss;
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
    .msg_name = &ss,
    .msg_iov = &msg_iov,
    .msg_iovlen = 1,
    .msg_control = msg_ctrl,
  };

  auto start = util::timestamp();

  for (; pktcnt < MAX_RECV_PKTS;) {
    if (util::recv_pkt_time_threshold_exceeded(
          config.cc_algo == NGTCP2_CC_ALGO_BBR, start, pktcnt)) {
      return;
    }

    msg.msg_namelen = sizeof(ss);
    msg.msg_controllen = sizeof(msg_ctrl);

    auto nread = recvmsg(ep.fd, &msg, 0);
    if (nread == -1) {
      if (!(errno == EAGAIN || errno == ENOTCONN)) {
        std::println(stderr, "recvmsg: {}", strerror(errno));
      }
      return;
    }

    // Packets less than 21 bytes never be a valid QUIC packet.
    if (nread < 21) {
      ++pktcnt;

      continue;
    }

    Address remote_addr;
    remote_addr.set(reinterpret_cast<const sockaddr *>(&ss));

    if (util::prohibited_port(remote_addr.port())) {
      ++pktcnt;

      continue;
    }

    pi.ecn = msghdr_get_ecn(&msg, ss.ss_family);
    auto local_addr = msghdr_get_local_addr(&msg, ss.ss_family);
    if (!local_addr) {
      ++pktcnt;
      std::println(stderr, "Unable to obtain local address");
      continue;
    }

    auto gso_size = msghdr_get_udp_gro(&msg);
    if (gso_size == 0) {
      gso_size = static_cast<size_t>(nread);
    }

    local_addr->port(ep.addr.port());

    auto data = std::span{buf.data(), static_cast<size_t>(nread)};

    for (; !data.empty();) {
      auto datalen = std::min(data.size(), gso_size);

      ++pktcnt;

      if (!config.quiet) {
        std::array<char, IF_NAMESIZE> ifname;
        std::println(
          stderr,
          "Received packet: local={} remote={} if={} ecn={:#x} {} bytes",
          util::straddr(*local_addr), util::straddr(remote_addr),
          if_indextoname(local_addr->ifindex, ifname.data()), pi.ecn, datalen);
      }

      // Packets less than 21 bytes never be a valid QUIC packet.
      if (datalen < 21) {
        break;
      }

      if (debug::packet_lost(config.rx_loss_prob)) {
        if (!config.quiet) {
          std::println(stderr, "** Simulated incoming packet loss **");
        }
      } else {
        read_pkt(ep, *local_addr, remote_addr, &pi, {data.data(), datalen});
      }

      data = data.subspan(datalen);
    }
  }
}

void Server::read_pkt(const Endpoint &ep, const Address &local_addr,
                      const Address &remote_addr, const ngtcp2_pkt_info *pi,
                      std::span<const uint8_t> data) {
  ngtcp2_version_cid vc;

  switch (auto rv = ngtcp2_pkt_decode_version_cid(&vc, data.data(), data.size(),
                                                  NGTCP2_SV_SCIDLEN);
          rv) {
  case 0:
    break;
  case NGTCP2_ERR_VERSION_NEGOTIATION:
    send_version_negotiation(vc.version, {vc.scid, vc.scidlen},
                             {vc.dcid, vc.dcidlen}, ep, local_addr,
                             remote_addr);
    return;
  default:
    std::println(stderr,
                 "Could not decode version and CID from QUIC packet header: {}",
                 ngtcp2_strerror(rv));
    return;
  }

  auto dcid_key = util::make_cid_key({vc.dcid, vc.dcidlen});

  auto handler_it = handlers_.find(dcid_key);
  if (handler_it == std::ranges::end(handlers_)) {
    ngtcp2_pkt_hd hd;

    if (auto rv = ngtcp2_accept(&hd, data.data(), data.size()); rv != 0) {
      if (!config.quiet) {
        std::println(stderr, "Unexpected packet received: length={}",
                     data.size());
      }

      if (!(data[0] & 0x80) && data.size() >= NGTCP2_SV_SCIDLEN + 21) {
        send_stateless_reset(data.size(), {vc.dcid, vc.dcidlen}, ep, local_addr,
                             remote_addr);
      }

      return;
    }

    ngtcp2_cid ocid;
    ngtcp2_cid *pocid = nullptr;
    ngtcp2_token_type token_type = NGTCP2_TOKEN_TYPE_UNKNOWN;

    assert(hd.type == NGTCP2_PKT_INITIAL);

    if (config.validate_addr || hd.tokenlen) {
      std::println(stderr, "Perform stateless address validation");
      if (hd.tokenlen == 0) {
        send_retry(&hd, ep, local_addr, remote_addr, data.size() * 3);
        return;
      }

      if (hd.token[0] != NGTCP2_CRYPTO_TOKEN_MAGIC_RETRY2 &&
          hd.dcid.datalen < NGTCP2_MIN_INITIAL_DCIDLEN) {
        send_stateless_connection_close(&hd, ep, local_addr, remote_addr);
        return;
      }

      switch (hd.token[0]) {
      case NGTCP2_CRYPTO_TOKEN_MAGIC_RETRY2:
        if (auto rv = verify_retry_token(&ocid, &hd, remote_addr); !rv) {
          if (rv.error() != Error::UNREADABLE_TOKEN) {
            send_stateless_connection_close(&hd, ep, local_addr, remote_addr);

            return;
          }

          hd.token = nullptr;
          hd.tokenlen = 0;
        } else {
          pocid = &ocid;
          token_type = NGTCP2_TOKEN_TYPE_RETRY;
        }

        break;
      case NGTCP2_CRYPTO_TOKEN_MAGIC_REGULAR:
        if (!verify_token(&hd, remote_addr)) {
          if (config.validate_addr) {
            send_retry(&hd, ep, local_addr, remote_addr, data.size() * 3);
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
          std::println(stderr, "Ignore unrecognized token");
        }
        if (config.validate_addr) {
          send_retry(&hd, ep, local_addr, remote_addr, data.size() * 3);
          return;
        }

        hd.token = nullptr;
        hd.tokenlen = 0;
        break;
      }
    }

    auto h = std::make_unique<Handler>(loop_, this);
    if (!h->init(ep, local_addr, remote_addr, &hd.scid, &hd.dcid, pocid,
                 {hd.token, hd.tokenlen}, token_type, hd.version, tls_ctx_)) {
      return;
    }

    if (auto rv = h->on_read(ep, local_addr, remote_addr, pi, data); !rv) {
      if (rv.error() == Error::RETRY_CONN) {
        send_retry(&hd, ep, local_addr, remote_addr, data.size() * 3);
      }

      return;
    }

    if (!h->on_write()) {
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
    if (!h->send_conn_close(ep, local_addr, remote_addr, pi, data)) {
      remove(h);
    }
    return;
  }
  if (ngtcp2_conn_in_draining_period(conn)) {
    return;
  }

  if (auto rv = h->on_read(ep, local_addr, remote_addr, pi, data); !rv) {
    if (rv.error() != Error::CLOSE_WAIT) {
      remove(h);
    }
    return;
  }

  h->signal_write();
}

namespace {
uint32_t generate_reserved_version(const Address &addr, uint32_t version) {
  uint32_t h = 0x811C9DC5U;
  const uint8_t *p = reinterpret_cast<const uint8_t *>(addr.as_sockaddr());
  const uint8_t *ep = p + addr.size();
  for (; p != ep; ++p) {
    h ^= *p;
    h *= 0x01000193U;
  }
  version = htonl(version);
  p = (const uint8_t *)&version;
  ep = p + sizeof(version);
  for (; p != ep; ++p) {
    h ^= *p;
    h *= 0x01000193U;
  }
  h &= 0xF0F0F0F0U;
  h |= 0x0A0A0A0AU;
  return h;
}
} // namespace

std::expected<void, Error> Server::send_version_negotiation(
  uint32_t version, std::span<const uint8_t> dcid,
  std::span<const uint8_t> scid, const Endpoint &ep, const Address &local_addr,
  const Address &remote_addr) {
  Buffer buf{NGTCP2_MAX_UDP_PAYLOAD_SIZE};
  std::array<uint32_t, 1 + max_preferred_versionslen> sv;

  auto p = std::ranges::begin(sv);

  *p++ = generate_reserved_version(remote_addr, version);

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
    std::println(stderr, "ngtcp2_pkt_write_version_negotiation: {}",
                 ngtcp2_strerror(static_cast<int>(nwrite)));
    return std::unexpected{Error::QUIC};
  }

  buf.push(as_unsigned(nwrite));

  return send_packet(ep, as_ngtcp2_addr(local_addr),
                     as_ngtcp2_addr(remote_addr),
                     /* ecn = */ 0, buf.data());
}

std::expected<void, Error> Server::send_retry(const ngtcp2_pkt_hd *chd,
                                              const Endpoint &ep,
                                              const Address &local_addr,
                                              const Address &remote_addr,
                                              size_t max_pktlen) {
  std::array<char, NI_MAXHOST> host;
  std::array<char, NI_MAXSERV> port;

  if (auto rv = getnameinfo(remote_addr.as_sockaddr(), remote_addr.size(),
                            host.data(), host.size(), port.data(), port.size(),
                            NI_NUMERICHOST | NI_NUMERICSERV);
      rv != 0) {
    std::println(stderr, "getnameinfo: {}", gai_strerror(rv));
    return std::unexpected{Error::LIBC};
  }

  if (!config.quiet) {
    std::println(stderr, "Sending Retry packet to [{}]:{}", host.data(),
                 port.data());
  }

  ngtcp2_cid scid;

  scid.datalen = NGTCP2_SV_SCIDLEN;
  if (auto rv = util::generate_secure_random({scid.data, scid.datalen}); !rv) {
    return rv;
  }

  std::array<uint8_t, NGTCP2_CRYPTO_MAX_RETRY_TOKENLEN2> tokenbuf;

  auto t = util::system_clock_now();

  auto tokenlen = ngtcp2_crypto_generate_retry_token2(
    tokenbuf.data(), config.static_secret.data(), config.static_secret.size(),
    chd->version, remote_addr.as_sockaddr(), remote_addr.size(), &scid,
    &chd->dcid, t);
  if (tokenlen < 0) {
    return std::unexpected{Error::QUIC};
  }

  auto token = std::span{tokenbuf}.first(as_unsigned(tokenlen));

  if (!config.quiet) {
    std::println(stderr, "Generated address validation token:");
    util::hexdump(stderr, token);
  }

  Buffer buf{
    std::min(static_cast<size_t>(NGTCP2_MAX_UDP_PAYLOAD_SIZE), max_pktlen)};

  auto nwrite =
    ngtcp2_crypto_write_retry(buf.wpos(), buf.left(), chd->version, &chd->scid,
                              &scid, &chd->dcid, token.data(), token.size());
  if (nwrite < 0) {
    std::println(stderr, "ngtcp2_crypto_write_retry failed");
    return std::unexpected{Error::QUIC};
  }

  buf.push(as_unsigned(nwrite));

  return send_packet(ep, as_ngtcp2_addr(local_addr),
                     as_ngtcp2_addr(remote_addr),
                     /* ecn = */ 0, buf.data());
}

std::expected<void, Error> Server::send_stateless_connection_close(
  const ngtcp2_pkt_hd *chd, const Endpoint &ep, const Address &local_addr,
  const Address &remote_addr) {
  Buffer buf{NGTCP2_MAX_UDP_PAYLOAD_SIZE};

  auto nwrite = ngtcp2_crypto_write_connection_close(
    buf.wpos(), buf.left(), chd->version, &chd->scid, &chd->dcid,
    NGTCP2_INVALID_TOKEN, nullptr, 0);
  if (nwrite < 0) {
    std::println(stderr, "ngtcp2_crypto_write_connection_close failed");
    return std::unexpected{Error::QUIC};
  }

  buf.push(as_unsigned(nwrite));

  return send_packet(ep, as_ngtcp2_addr(local_addr),
                     as_ngtcp2_addr(remote_addr),
                     /* ecn = */ 0, buf.data());
}

std::expected<void, Error>
Server::send_stateless_reset(size_t pktlen, std::span<const uint8_t> dcid,
                             const Endpoint &ep, const Address &local_addr,
                             const Address &remote_addr) {
  if (stateless_reset_bucket_ == 0) {
    return {};
  }

  --stateless_reset_bucket_;

  if (!ev_is_active(&stateless_reset_regen_timer_)) {
    ev_timer_again(loop_, &stateless_reset_regen_timer_);
  }

  ngtcp2_cid cid;

  ngtcp2_cid_init(&cid, dcid.data(), dcid.size());

  ngtcp2_stateless_reset_token token;

  if (ngtcp2_crypto_generate_stateless_reset_token(
        token.data, config.static_secret.data(), config.static_secret.size(),
        &cid) != 0) {
    return std::unexpected{Error::QUIC};
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

  if (auto rv =
        util::generate_secure_random({rand_bytes.data(), rand_byteslen});
      !rv) {
    return rv;
  }

  Buffer buf{NGTCP2_MAX_UDP_PAYLOAD_SIZE};

  auto nwrite = ngtcp2_pkt_write_stateless_reset2(
    buf.wpos(), buf.left(), &token, rand_bytes.data(), rand_byteslen);
  if (nwrite < 0) {
    std::println(stderr, "ngtcp2_pkt_write_stateless_reset2: {}",
                 ngtcp2_strerror(static_cast<int>(nwrite)));

    return std::unexpected{Error::QUIC};
  }

  buf.push(as_unsigned(nwrite));

  return send_packet(ep, as_ngtcp2_addr(local_addr),
                     as_ngtcp2_addr(remote_addr),
                     /* ecn = */ 0, buf.data());
}

std::expected<void, Error>
Server::verify_retry_token(ngtcp2_cid *ocid, const ngtcp2_pkt_hd *hd,
                           const Address &remote_addr) {
  int rv;

  if (!config.quiet) {
    std::array<char, NI_MAXHOST> host;
    std::array<char, NI_MAXSERV> port;

    if (auto rv = getnameinfo(remote_addr.as_sockaddr(), remote_addr.size(),
                              host.data(), host.size(), port.data(),
                              port.size(), NI_NUMERICHOST | NI_NUMERICSERV);
        rv != 0) {
      std::println(stderr, "getnameinfo: {}", gai_strerror(rv));
      return std::unexpected{Error::LIBC};
    }

    std::println(stderr, "Verifying Retry token from [{}]:{}", host.data(),
                 port.data());
    util::hexdump(stderr, {hd->token, hd->tokenlen});
  }

  auto t = util::system_clock_now();

  rv = ngtcp2_crypto_verify_retry_token2(
    ocid, hd->token, hd->tokenlen, config.static_secret.data(),
    config.static_secret.size(), hd->version, remote_addr.as_sockaddr(),
    remote_addr.size(), &hd->dcid, 10 * NGTCP2_SECONDS, t);
  switch (rv) {
  case 0:
    break;
  case NGTCP2_CRYPTO_ERR_VERIFY_TOKEN:
    std::println(stderr, "Could not verify Retry token");

    return std::unexpected{Error::QUIC};
  default:
    std::println(stderr,
                 "Could not read Retry token.  Continue without the token");

    return std::unexpected{Error::UNREADABLE_TOKEN};
  }

  if (!config.quiet) {
    std::println(stderr, "Token was successfully validated");
  }

  return {};
}

std::expected<void, Error> Server::verify_token(const ngtcp2_pkt_hd *hd,
                                                const Address &remote_addr) {
  std::array<char, NI_MAXHOST> host;
  std::array<char, NI_MAXSERV> port;

  if (auto rv = getnameinfo(remote_addr.as_sockaddr(), remote_addr.size(),
                            host.data(), host.size(), port.data(), port.size(),
                            NI_NUMERICHOST | NI_NUMERICSERV);
      rv != 0) {
    std::println(stderr, "getnameinfo: {}", gai_strerror(rv));
    return std::unexpected{Error::LIBC};
  }

  if (!config.quiet) {
    std::println(stderr, "Verifying token from [{}]:{}", host.data(),
                 port.data());
    util::hexdump(stderr, {hd->token, hd->tokenlen});
  }

  auto t = util::system_clock_now();

  if (ngtcp2_crypto_verify_regular_token(
        hd->token, hd->tokenlen, config.static_secret.data(),
        config.static_secret.size(), remote_addr.as_sockaddr(),
        remote_addr.size(), 3600 * NGTCP2_SECONDS, t) != 0) {
    std::println(stderr, "Could not verify token");

    return std::unexpected{Error::QUIC};
  }

  if (!config.quiet) {
    std::println(stderr, "Token was successfully validated");
  }

  return {};
}

std::expected<void, Error> Server::send_packet(const Endpoint &ep,
                                               const ngtcp2_addr &local_addr,
                                               const ngtcp2_addr &remote_addr,
                                               unsigned int ecn,
                                               std::span<const uint8_t> data) {
  auto no_gso = false;
  auto rest =
    send_packet(ep, no_gso, local_addr, remote_addr, ecn, data, data.size());
  if (!rest.empty()) {
    return std::unexpected{Error::SEND_BLOCKED};
  }

  return {};
}

std::span<const uint8_t> Server::send_packet(const Endpoint &ep, bool &no_gso,
                                             const ngtcp2_addr &local_addr,
                                             const ngtcp2_addr &remote_addr,
                                             unsigned int ecn,
                                             std::span<const uint8_t> data,
                                             size_t gso_size) {
  assert(gso_size);

  if (debug::packet_lost(config.tx_loss_prob)) {
    if (!config.quiet) {
      std::println(stderr, "** Simulated outgoing packet loss **");
    }
    return {};
  }

  if (no_gso && data.size() > gso_size) {
    for (; !data.empty();) {
      auto len = std::min(gso_size, data.size());

      auto rest = send_packet(ep, no_gso, local_addr, remote_addr, ecn,
                              data.first(len), len);
      if (!rest.empty()) {
        assert(rest.size() == len);

        return data;
      }

      data = data.subspan(len);
    }

    return {};
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
      return data;
#ifdef UDP_SEGMENT
    case EIO:
      if (data.size() > gso_size) {
        // GSO failure; send each packet in a separate sendmsg call.
        std::println(stderr, "sendmsg: disabling GSO due to {}",
                     strerror(errno));

        no_gso = true;

        return send_packet(ep, no_gso, local_addr, remote_addr, ecn, data,
                           gso_size);
      }
      break;
#endif // defined(UDP_SEGMENT)
    }

    std::println(stderr, "sendmsg: {}", strerror(errno));
    // TODO We have packet which is expected to fail to send (e.g.,
    // path validation to old path).
    return {};
  }

  if (!config.quiet) {
    std::println(stderr, "Sent packet: local={} remote={} ecn={:#x} {} bytes",
                 util::straddr(local_addr.addr, local_addr.addrlen),
                 util::straddr(remote_addr.addr, remote_addr.addrlen), ecn,
                 nwrite);
  }

  return {};
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
std::expected<Address, Error> parse_host_port(int af,
                                              std::string_view host_port) {
  if (host_port.empty()) {
    return std::unexpected{Error::INVALID_ARGUMENT};
  }

  auto first = std::ranges::begin(host_port);
  auto last = std::ranges::end(host_port);

  std::string_view hostv;

  if (*first == '[') {
    ++first;

    auto it = std::ranges::find(first, last, ']');
    if (it == last) {
      return std::unexpected{Error::INVALID_ARGUMENT};
    }

    hostv = std::string_view{first, it};
    first = it + 1;

    if (first == last || *first != ':') {
      return std::unexpected{Error::INVALID_ARGUMENT};
    }
  } else {
    auto it = std::ranges::find(first, last, ':');
    if (it == last) {
      return std::unexpected{Error::INVALID_ARGUMENT};
    }

    hostv = std::string_view{first, it};
    first = it;
  }

  if (++first == last) {
    return std::unexpected{Error::INVALID_ARGUMENT};
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
    std::println(stderr, "getaddrinfo: [{}]:{}: {}", host.data(), svc,
                 gai_strerror(rv));
    return std::unexpected{Error::LIBC};
  }

  Address dest;
  dest.set(res->ai_addr);

  freeaddrinfo(res);

  return dest;
}
} // namespace

namespace {
const char *prog = "server";
} // namespace

namespace {
void print_usage(FILE *out) {
  std::println(
    out,
    "Usage: {} [OPTIONS] <ADDR> <PORT> <PRIVATE_KEY_FILE> <CERTIFICATE_FILE>",
    prog);
}
} // namespace

namespace {
void print_help() {
  print_usage(stdout);

  Config config;

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
              Read private  key and  ECHConfig from <PATH>.   The file
              denoted by <PATH> must contain private key and ECHConfig
              as                      described                     in
              https://datatracker.ietf.org/doc/html/draft-farrell-tls-pemesni.
              ECH configuration  is only applied if  an underlying TLS
              stack supports it.
  --origin=<ORIGIN>
              Specify the origin to send in ORIGIN frame.  Repeat to
              add multiple origins.
  --no-gso    Disables GSO.
  --show-stat Print the connection statistics when the connection is
              closed.
  --gso-burst=<N>
              The maximum number of packets  to aggregate for GSO.  If
              GSO is disabled,  this is the maximum  number of packets
              to send  per an event  loop in a single  connection.  It
              defaults  to 0,  which means  it is  not limited  by the
              configuration.
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
      {"no-gso", no_argument, &flag, 35},
      {"show-stat", no_argument, &flag, 36},
      {"gso-burst", required_argument, &flag, 37},
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
        std::println(stderr, "path: invalid path {}", optarg);
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
      print_usage(stderr);
      exit(EXIT_FAILURE);
    case 0:
      switch (flag) {
      case 1:
        // --ciphers
        if (util::crypto_default_ciphers()[0] == '\0') {
          std::println(stderr, "ciphers: not supported");
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
          std::println(stderr, "timeout: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.timeout = *t;
        }
        break;
      case 4: {
        // --preferred-ipv4-addr
        auto maybe_addr = parse_host_port(AF_INET, optarg);
        if (!maybe_addr) {
          std::println(stderr, "preferred-ipv4-addr: could not use {}", optarg);
          exit(EXIT_FAILURE);
        }

        config.preferred_ipv4_addr = *maybe_addr;

        break;
      }
      case 5: {
        // --preferred-ipv6-addr
        auto maybe_addr = parse_host_port(AF_INET6, optarg);
        if (!maybe_addr) {
          std::println(stderr, "preferred-ipv6-addr: could not use {}", optarg);
          exit(EXIT_FAILURE);
        }

        config.preferred_ipv6_addr = *maybe_addr;

        break;
      }
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
          std::println(stderr, "max-data: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.max_data = *n;
        }
        break;
      case 13:
        // --max-stream-data-bidi-local
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::println(stderr, "max-stream-data-bidi-local: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_data_bidi_local = *n;
        }
        break;
      case 14:
        // --max-stream-data-bidi-remote
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::println(stderr, "max-stream-data-bidi-remote: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_data_bidi_remote = *n;
        }
        break;
      case 15:
        // --max-stream-data-uni
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::println(stderr, "max-stream-data-uni: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_data_uni = *n;
        }
        break;
      case 16:
        // --max-streams-bidi
        if (auto n = util::parse_uint(optarg); !n) {
          std::println(stderr, "max-streams-bidi: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.max_streams_bidi = *n;
        }
        break;
      case 17:
        // --max-streams-uni
        if (auto n = util::parse_uint(optarg); !n) {
          std::println(stderr, "max-streams-uni: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.max_streams_uni = *n;
        }
        break;
      case 18:
        // --max-dyn-length
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::println(stderr, "max-dyn-length: invalid argument");
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
        std::println(stderr, "cc: specify cubic, reno, or bbr");
        exit(EXIT_FAILURE);
      case 20:
        // --initial-rtt
        if (auto t = util::parse_duration(optarg); !t) {
          std::println(stderr, "initial-rtt: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.initial_rtt = *t;
        }
        break;
      case 21:
        // --max-udp-payload-size
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::println(stderr, "max-udp-payload-size: invalid argument");
          exit(EXIT_FAILURE);
        } else if (*n > NGTCP2_MAX_TX_UDP_PAYLOAD_SIZE) {
          std::println(stderr, "max-udp-payload-size: must not exceed {}",
                       NGTCP2_MAX_TX_UDP_PAYLOAD_SIZE);
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
          std::println(stderr, "max-window: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.max_window = *n;
        }
        break;
      case 24:
        // --max-stream-window
        if (auto n = util::parse_uint_iec(optarg); !n) {
          std::println(stderr, "max-stream-window: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.max_stream_window = *n;
        }
        break;
      case 26:
        // --handshake-timeout
        if (auto t = util::parse_duration(optarg); !t) {
          std::println(stderr, "handshake-timeout: invalid argument");
          exit(EXIT_FAILURE);
        } else {
          config.handshake_timeout = *t;
        }
        break;
      case 27: {
        // --preferred-versions
        auto l = util::split_str(optarg);
        if (l.size() > max_preferred_versionslen) {
          std::println(stderr, "preferred-versions: too many versions > {}",
                       max_preferred_versionslen);
          exit(EXIT_FAILURE);
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
            std::println(stderr, "preferred-versions: invalid version {}", k);
            exit(EXIT_FAILURE);
          }
          if (!ngtcp2_is_supported_version(*rv)) {
            std::println(stderr, "preferred-versions: unsupported version {}",
                         k);
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
            std::println(stderr, "available-versions: invalid version {}", k);
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
          std::println(stderr, "ack-thresh: invalid argument");
          exit(EXIT_FAILURE);
        } else if (*n > 100) {
          std::println(stderr, "ack-thresh: must not exceed 100");
          exit(EXIT_FAILURE);
        } else {
          config.ack_thresh = *n;
        }
        break;
      case 31:
        // --initial-pkt-num
        if (auto n = util::parse_uint(optarg); !n) {
          std::println(stderr, "initial-pkt-num: invalid argument");
          exit(EXIT_FAILURE);
        } else if (*n > INT32_MAX) {
          std::println(stderr,
                       "initial-pkt-num: must not exceed (1 << 31) - 1");
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
            std::println(stderr, "pmtud-probes: invalid argument");
            exit(EXIT_FAILURE);
          } else if (*n <= NGTCP2_MAX_UDP_PAYLOAD_SIZE ||
                     *n > NGTCP2_MAX_TX_UDP_PAYLOAD_SIZE) {
            std::println(
              stderr, "pmtud-probes: must be in range [{}, {}], inclusive.",
              NGTCP2_MAX_UDP_PAYLOAD_SIZE + 1, NGTCP2_MAX_TX_UDP_PAYLOAD_SIZE);
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
          std::println(stderr, "origin: must be less than or equal to {}", max);
          exit(EXIT_FAILURE);
        }

        if (!config.origin_list) {
          config.origin_list = std::vector<uint8_t>();
        }

        config.origin_list->push_back(static_cast<uint8_t>(origin.size() >> 8));
        config.origin_list->push_back(origin.size() & 0xFF);
        std::ranges::copy(origin, std::back_inserter(*config.origin_list));

        break;
      }
      case 35:
        // --no-gso
        config.no_gso = true;
        break;
      case 36:
        // --show-stat
        config.show_stat = true;
        break;
      case 37: {
        // --gso-burst
        auto n = util::parse_uint(optarg);
        if (!n) {
          std::println(stderr, "gso-burst: invalid argument");
          exit(EXIT_FAILURE);
        }

        if (*n > 64) {
          std::println(stderr,
                       "gso-burst: must be in range [0, 64], inclusive.");
          exit(EXIT_FAILURE);
        }

        config.gso_burst = static_cast<size_t>(*n);

        break;
      }
      }
      break;
    default:
      break;
    }
  }

  if (argc - optind < 4) {
    std::println(stderr, "Too few arguments");
    print_usage(stderr);
    exit(EXIT_FAILURE);
  }

  auto addr = argv[optind++];
  auto port = argv[optind++];
  auto private_key_file = argv[optind++];
  auto cert_file = argv[optind++];

  if (auto n = util::parse_uint(port); !n) {
    std::println(stderr, "port: invalid port number");
    exit(EXIT_FAILURE);
  } else if (*n > 65535) {
    std::println(stderr, "port: must not exceed 65535");
    exit(EXIT_FAILURE);
  } else {
    config.port = static_cast<uint16_t>(*n);
  }

  if (auto mt = util::read_mime_types(config.mime_types_file); !mt) {
    std::println(stderr,
                 "mime-types-file: Could not read MIME media types file {}",
                 config.mime_types_file);
  } else {
    config.mime_types = std::move(*mt);
  }

  if (!ech_config_file.empty()) {
    auto ech_config = util::read_ech_server_config(ech_config_file);
    if (!ech_config) {
      std::println(stderr,
                   "ech-config-file: Could not read private key and ECHConfig");
      exit(EXIT_FAILURE);
    }

    config.ech_config = std::move(*ech_config);
  }

  TLSServerContext tls_ctx;

  if (!tls_ctx.init(private_key_file, cert_file, AppProtocol::H3)) {
    exit(EXIT_FAILURE);
  }

  if (config.htdocs.back() != '/') {
    config.htdocs += '/';
  }

  std::println(stderr, "Using document root {}", config.htdocs);

  auto ev_loop_d = defer([] { ev_loop_destroy(EV_DEFAULT); });

  auto keylog_filename = getenv("SSLKEYLOGFILE");
  if (keylog_filename) {
    keylog_file.open(keylog_filename, std::ios_base::app);
    if (keylog_file) {
      tls_ctx.enable_keylog();
    }
  }

  if (!util::generate_secure_random(config.static_secret)) {
    std::println(stderr, "Unable to generate static secret");
    exit(EXIT_FAILURE);
  }

  Server s(EV_DEFAULT, tls_ctx);
  if (!s.init(addr, port)) {
    exit(EXIT_FAILURE);
  }

  ev_run(EV_DEFAULT, 0);

  s.disconnect();
  s.close();

  return EXIT_SUCCESS;
}
