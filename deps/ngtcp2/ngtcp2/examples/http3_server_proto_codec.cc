/*
 * ngtcp2
 *
 * Copyright (c) 2026 ngtcp2 contributors
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
#include "http3_server_proto_codec.h"

#include "server.h"
#include "debug.h"

extern Config config;

namespace ngtcp2 {

ProtoCodec::ProtoCodec(Handler *h, ngtcp2_ccerr &last_error)
  : handler_{h}, conn_{handler_->conn()}, last_error_{last_error} {}

ProtoCodec::~ProtoCodec() {
  if (httpconn_) {
    nghttp3_conn_del(httpconn_);
  }
}

std::expected<void, Error>
ProtoCodec::acked_stream_data_offset(int64_t stream_id, uint64_t datalen) {
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

std::expected<void, Error> ProtoCodec::on_stream_reset(int64_t stream_id) {
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

std::expected<void, Error>
ProtoCodec::on_stream_stop_sending(int64_t stream_id) {
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

void ProtoCodec::extend_max_remote_streams_bidi(uint64_t max_streams) {
  if (!httpconn_) {
    return;
  }

  nghttp3_conn_set_max_client_streams_bidi(httpconn_, max_streams);
}

std::expected<void, Error>
ProtoCodec::extend_max_stream_data(int64_t stream_id, uint64_t max_data) {
  if (auto rv = nghttp3_conn_unblock_stream(httpconn_, stream_id); rv != 0) {
    std::println(stderr, "nghttp3_conn_unblock_stream: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }
  return {};
}

std::expected<void, Error> ProtoCodec::on_app_tx_ready() {
  return setup_httpconn();
}

ngtcp2_ssize ProtoCodec::write_pkt(ngtcp2_path *path, ngtcp2_pkt_info *pi,
                                   uint8_t *dest, size_t destlen,
                                   ngtcp2_tstamp ts) {
  std::array<nghttp3_vec, 16> vec;

  for (;;) {
    int64_t stream_id = -1;
    int fin = 0;
    nghttp3_ssize sveccnt = 0;

    if (httpconn_ && ngtcp2_conn_get_max_data_left2(conn_)) {
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

std::expected<void, Error>
ProtoCodec::recv_stream_data(uint32_t flags, int64_t stream_id,
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
ProtoCodec::on_stream_close(int64_t stream_id, uint64_t app_error_code) {
  if (!httpconn_) {
    return {};
  }

  if (app_error_code == 0) {
    app_error_code = NGHTTP3_H3_NO_ERROR;
  }

  if (auto rv = nghttp3_conn_close_stream(httpconn_, stream_id, app_error_code);
      rv != 0) {
    if (rv != NGHTTP3_ERR_STREAM_NOT_FOUND) {
      std::println(stderr, "nghttp3_conn_close_stream: {}",
                   nghttp3_strerror(rv));
      ngtcp2_ccerr_set_application_error(
        &last_error_, nghttp3_err_infer_quic_app_error_code(rv), nullptr, 0);
      return std::unexpected{Error::HTTP3};
    }

    return {};
  }

  http_stream_close(stream_id, app_error_code);

  return {};
}

namespace {
int http_acked_stream_data(nghttp3_conn *conn, int64_t stream_id,
                           uint64_t datalen, void *user_data,
                           void *stream_user_data) {
  auto pc = static_cast<ProtoCodec *>(user_data);
  auto stream = static_cast<Stream *>(stream_user_data);
  pc->http_acked_stream_data(stream, datalen);
  return 0;
}
} // namespace

void ProtoCodec::http_acked_stream_data(Stream *stream, uint64_t datalen) {
  stream->http_acked_stream_data(datalen);
}

namespace {
int http_recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                   size_t datalen, void *user_data, void *stream_user_data) {
  if (!config.quiet && !config.no_http_dump) {
    debug::print_http_data(stream_id, {data, datalen});
  }
  auto pc = static_cast<ProtoCodec *>(user_data);
  pc->http_consume(stream_id, datalen);
  return 0;
}
} // namespace

namespace {
int http_deferred_consume(nghttp3_conn *conn, int64_t stream_id,
                          size_t nconsumed, void *user_data,
                          void *stream_user_data) {
  auto pc = static_cast<ProtoCodec *>(user_data);
  pc->http_consume(stream_id, nconsumed);
  return 0;
}
} // namespace

void ProtoCodec::http_consume(int64_t stream_id, size_t nconsumed) {
  ngtcp2_conn_extend_max_stream_offset(conn_, stream_id, nconsumed);
  ngtcp2_conn_extend_max_offset(conn_, nconsumed);
}

namespace {
int http_begin_request_headers(nghttp3_conn *conn, int64_t stream_id,
                               void *user_data, void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_begin_request_headers(stream_id);
  }

  auto pc = static_cast<ProtoCodec *>(user_data);
  pc->http_begin_request_headers(stream_id);
  return 0;
}
} // namespace

void ProtoCodec::http_begin_request_headers(int64_t stream_id) {
  auto stream = handler_->find_stream(stream_id);
  if (!stream) {
    return;
  }

  nghttp3_conn_set_stream_user_data(httpconn_, stream_id, stream);
}

namespace {
int http_recv_request_header(nghttp3_conn *conn, int64_t stream_id,
                             int32_t token, nghttp3_rcbuf *name,
                             nghttp3_rcbuf *value, uint8_t flags,
                             void *user_data, void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_header(stream_id, name, value, flags);
  }

  auto pc = static_cast<ProtoCodec *>(user_data);
  auto stream = static_cast<Stream *>(stream_user_data);
  pc->http_recv_request_header(stream, token, name, value);
  return 0;
}
} // namespace

void ProtoCodec::http_recv_request_header(Stream *stream, int32_t token,
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

  auto pc = static_cast<ProtoCodec *>(user_data);
  auto stream = static_cast<Stream *>(stream_user_data);
  if (!pc->http_end_request_headers(stream)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error>
ProtoCodec::http_end_request_headers(Stream *stream) {
  if (config.early_response) {
    if (auto rv = start_response(stream); !rv) {
      return rv;
    }

    handler_->shutdown_read(stream->stream_id, NGHTTP3_H3_NO_ERROR);
  }
  return {};
}

namespace {
int http_stop_sending(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *user_data,
                      void *stream_user_data) {
  auto pc = static_cast<ProtoCodec *>(user_data);
  if (!pc->http_stop_sending(stream_id, app_error_code)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error>
ProtoCodec::http_stop_sending(int64_t stream_id, uint64_t app_error_code) {
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
int http_end_stream(nghttp3_conn *conn, int64_t stream_id, void *user_data,
                    void *stream_user_data) {
  auto pc = static_cast<ProtoCodec *>(user_data);
  auto stream = static_cast<Stream *>(stream_user_data);
  if (!pc->http_end_stream(stream)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> ProtoCodec::http_end_stream(Stream *stream) {
  if (!config.early_response) {
    return start_response(stream);
  }
  return {};
}

namespace {
int http_reset_stream(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *user_data,
                      void *stream_user_data) {
  auto pc = static_cast<ProtoCodec *>(user_data);
  if (!pc->http_reset_stream(stream_id, app_error_code)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error>
ProtoCodec::http_reset_stream(int64_t stream_id, uint64_t app_error_code) {
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
void rand_bytes(uint8_t *dest, size_t destlen) {
  if (!util::generate_secure_random({dest, destlen})) {
    assert(0);
    abort();
  }
}
} // namespace

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

namespace {
nghttp3_ssize read_data(nghttp3_conn *conn, int64_t stream_id, nghttp3_vec *vec,
                        size_t veccnt, uint32_t *pflags, void *user_data,
                        void *stream_user_data) {
  auto stream = static_cast<Stream *>(stream_user_data);

  vec[0].base = const_cast<uint8_t *>(stream->resp_data.data());
  vec[0].len = stream->resp_data.size();
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

std::expected<void, Error> ProtoCodec::start_response(Stream *stream) {
  // TODO This should be handled by nghttp3
  if (stream->uri.empty() || stream->method.empty()) {
    return send_status_response(stream, 400);
  }

  auto maybe_req = stream->request_path();
  if (!maybe_req) {
    return send_status_response(stream, 400);
  }

  const auto &req = *maybe_req;

  uint64_t content_length;
  nghttp3_data_reader dr{};
  auto content_type = "text/plain"sv;

  auto maybe_dyn_len = stream->find_dyn_length(req.path);
  if (!maybe_dyn_len) {
    auto path = config.htdocs;
    path /= std::filesystem::path{req.path}.relative_path();

    auto maybe_fe = stream->open_file(path);
    if (!maybe_fe) {
      return send_status_response(stream, 404);
    }

    const auto &fe = *maybe_fe;

    if (fe.flags & FILE_ENTRY_TYPE_DIR) {
      return send_redirect_response(stream, 308, req.path + '/');
    }

    content_length = fe.len;

    if (stream->method != "HEAD") {
      stream->map_file(fe);
    }

    dr.read_data = read_data;

    auto ext = path.extension();
    if (!ext.empty() && ext != ".") {
      auto it = config.mime_types.find(ext.native());
      if (it != std::ranges::end(config.mime_types)) {
        content_type = (*it).second;
      }
    }
  } else {
    content_length = *maybe_dyn_len;
    stream->dynresp = true;
    dr.read_data = dyn_read_data;

    if (stream->method != "HEAD") {
      stream->dyndataleft = content_length;
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

    if (auto rv =
          nghttp3_conn_get_stream_priority(httpconn_, &pri, stream->stream_id);
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

    if (auto rv = nghttp3_conn_set_server_stream_priority(
          httpconn_, stream->stream_id, &pri);
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
    debug::print_http_response_headers(stream->stream_id, nva.data(), nvlen);
  }

  if (auto rv = nghttp3_conn_submit_response(httpconn_, stream->stream_id,
                                             nva.data(), nvlen, &dr);
      rv != 0) {
    std::println(stderr, "nghttp3_conn_submit_response: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  if (config.send_trailers && !maybe_dyn_len) {
    auto stream_id_str = util::format_uint(as_unsigned(stream->stream_id));
    auto trailers = std::to_array({
      util::make_nv_nc("x-ngtcp2-stream-id"sv, stream_id_str),
    });

    if (auto rv = nghttp3_conn_submit_trailers(
          httpconn_, stream->stream_id, trailers.data(), trailers.size());
        rv != 0) {
      std::println(stderr, "nghttp3_conn_submit_trailers: {}",
                   nghttp3_strerror(rv));
      return std::unexpected{Error::HTTP3};
    }
  }

  return {};
}

std::expected<void, Error>
ProtoCodec::send_status_response(Stream *stream, unsigned int status_code,
                                 std::vector<HTTPHeader> extra_headers) {
  stream->status_resp_body = make_status_body(status_code);

  auto status_code_str = util::format_uint(status_code);
  auto content_length_str = util::format_uint(stream->status_resp_body.size());

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

  stream->resp_data = as_uint8_span(std::span{stream->status_resp_body});

  nghttp3_data_reader dr{
    .read_data = read_data,
  };

  if (auto rv = nghttp3_conn_submit_response(httpconn_, stream->stream_id,
                                             nva.data(), nva.size(), &dr);
      rv != 0) {
    std::println(stderr, "nghttp3_conn_submit_response: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  if (config.send_trailers) {
    auto stream_id_str = util::format_uint(as_unsigned(stream->stream_id));
    auto trailers = std::to_array({
      util::make_nv_nc("x-ngtcp2-stream-id"sv, stream_id_str),
    });

    if (auto rv = nghttp3_conn_submit_trailers(
          httpconn_, stream->stream_id, trailers.data(), trailers.size());
        rv != 0) {
      std::println(stderr, "nghttp3_conn_submit_trailers: {}",
                   nghttp3_strerror(rv));
      return std::unexpected{Error::HTTP3};
    }
  }

  handler_->shutdown_read(stream->stream_id, NGHTTP3_H3_NO_ERROR);

  return {};
}

std::expected<void, Error>
ProtoCodec::send_redirect_response(Stream *stream, unsigned int status_code,
                                   std::string_view path) {
  return send_status_response(stream, status_code, {{"location", path}});
}

std::expected<void, Error> ProtoCodec::setup_httpconn() {
  if (httpconn_) {
    return {};
  }

  if (ngtcp2_conn_get_streams_uni_left2(conn_) < 3) {
    std::println(stderr,
                 "peer does not allow at least 3 unidirectional streams.");
    return std::unexpected{Error::QUIC};
  }

  static constexpr auto callbacks = nghttp3_callbacks{
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

  auto params = ngtcp2_conn_get_local_transport_params2(conn_);

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

void ProtoCodec::http_stream_close(int64_t stream_id, uint64_t app_error_code) {
  if (!ngtcp2_is_bidi_stream(stream_id)) {
    return;
  }

  if (!config.quiet) {
    std::println(stderr, "HTTP stream {:#x} closed with error code {:#x}",
                 stream_id, app_error_code);
  }
}

} // namespace ngtcp2
