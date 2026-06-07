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
#include "http3_client_proto_codec.h"

#include <unistd.h>

#include "client.h"
#include "debug.h"

extern Config config;

namespace ngtcp2 {

ProtoCodec::ProtoCodec(Client *c, ngtcp2_ccerr &last_error)
  : client_{c}, conn_{client_->conn()}, last_error_{last_error} {}

ProtoCodec::~ProtoCodec() {
  if (httpconn_) {
    nghttp3_conn_del(httpconn_);
  }
}

std::expected<void, Error>
ProtoCodec::recv_stream_data(uint32_t flags, int64_t stream_id,
                             std::span<const uint8_t> data) {
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
ProtoCodec::acked_stream_data_offset(int64_t stream_id, uint64_t datalen) {
  if (auto rv = nghttp3_conn_add_ack_offset(httpconn_, stream_id, datalen);
      rv != 0) {
    std::println(stderr, "nghttp3_conn_add_ack_offset: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  return {};
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

void ProtoCodec::early_data_rejected() {
  nghttp3_conn_del(httpconn_);
  httpconn_ = nullptr;
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

namespace {
nghttp3_ssize read_data(nghttp3_conn *conn, int64_t stream_id, nghttp3_vec *vec,
                        size_t veccnt, uint32_t *pflags, void *user_data,
                        void *stream_user_data) {
  vec[0].base = config.data;
  vec[0].len = config.datalen;
  *pflags |= NGHTTP3_DATA_FLAG_EOF;

  return 1;
}
} // namespace

std::expected<void, Error> ProtoCodec::submit_request(const Stream *stream) {
  std::string content_length_str;

  const auto &req = stream->req;

  std::array<nghttp3_nv, 6> nva{
    util::make_nv_nn(":method", config.http_method),
    util::make_nv_nn(":scheme", req.scheme),
    util::make_nv_nn(":authority", req.authority),
    util::make_nv_nn(":path", req.path),
    util::make_nv_nn("user-agent", "nghttp3/ngtcp2 client"),
  };
  size_t nvlen = 5;
  if (config.fd != -1) {
    content_length_str = util::format_uint(config.datalen);
    nva[nvlen++] = util::make_nv_nc("content-length", content_length_str);
  }

  if (!config.quiet) {
    debug::print_http_request_headers(stream->stream_id, nva.data(), nvlen);
  }

  nghttp3_data_reader dr{
    .read_data = read_data,
  };

  if (auto rv = nghttp3_conn_submit_request(
        httpconn_, stream->stream_id, nva.data(), nvlen,
        config.fd == -1 ? nullptr : &dr, nullptr);
      rv != 0) {
    std::println(stderr, "nghttp3_conn_submit_request: {}",
                 nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

  return {};
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

    uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_MORE;
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

namespace {
int http_recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                   size_t datalen, void *user_data, void *stream_user_data) {
  if (!config.quiet && !config.no_http_dump) {
    debug::print_http_data(stream_id, {data, datalen});
  }
  auto pc = static_cast<ProtoCodec *>(user_data);
  pc->http_consume(stream_id, datalen);
  pc->http_write_data(stream_id, {data, datalen});
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

void ProtoCodec::http_write_data(int64_t stream_id,
                                 std::span<const uint8_t> data) {
  auto stream = client_->find_stream(stream_id);
  if (!stream) {
    return;
  }

  if (stream->fd == -1) {
    return;
  }

  ssize_t nwrite;

  for (; !data.empty();) {
    do {
      nwrite = write(stream->fd, data.data(), data.size());
    } while (nwrite == -1 && errno == EINTR);

    if (nwrite < 0) {
      std::println(stderr, "Could not write data to file: {}", strerror(errno));

      return;
    }

    data = data.subspan(static_cast<size_t>(nwrite));
  }
}

namespace {
int http_begin_headers(nghttp3_conn *conn, int64_t stream_id, void *user_data,
                       void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_begin_response_headers(stream_id);
  }
  return 0;
}
} // namespace

namespace {
int http_recv_header(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                     nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                     void *user_data, void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_header(stream_id, name, value, flags);
  }
  return 0;
}
} // namespace

namespace {
int http_end_headers(nghttp3_conn *conn, int64_t stream_id, int fin,
                     void *user_data, void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_end_headers(stream_id);
  }
  return 0;
}
} // namespace

namespace {
int http_begin_trailers(nghttp3_conn *conn, int64_t stream_id, void *user_data,
                        void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_begin_trailers(stream_id);
  }
  return 0;
}
} // namespace

namespace {
int http_recv_trailer(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                      nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                      void *user_data, void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_header(stream_id, name, value, flags);
  }
  return 0;
}
} // namespace

namespace {
int http_end_trailers(nghttp3_conn *conn, int64_t stream_id, int fin,
                      void *user_data, void *stream_user_data) {
  if (!config.quiet) {
    debug::print_http_end_trailers(stream_id);
  }
  return 0;
}
} // namespace

namespace {
int http_stop_sending(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *user_data,
                      void *stream_user_data) {
  auto pc = static_cast<ProtoCodec *>(user_data);
  if (!pc->stop_sending(stream_id, app_error_code)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> ProtoCodec::stop_sending(int64_t stream_id,
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
  auto pc = static_cast<ProtoCodec *>(user_data);
  if (!pc->reset_stream(stream_id, app_error_code)) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
}
} // namespace

std::expected<void, Error> ProtoCodec::reset_stream(int64_t stream_id,
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
int http_recv_origin(nghttp3_conn *conn, const uint8_t *origin,
                     size_t originlen, void *conn_user_data) {
  if (!config.quiet) {
    debug::print_http_origin(origin, originlen);
  }

  return 0;
}
} // namespace

namespace {
int http_end_origin(nghttp3_conn *conn, void *conn_user_data) {
  if (!config.quiet) {
    debug::print_http_end_origin();
  }

  return 0;
}
} // namespace

std::expected<void, Error> ProtoCodec::setup_codec() {
  if (httpconn_) {
    return {};
  }

  if (ngtcp2_conn_get_streams_uni_left2(conn_) < 3) {
    std::println(stderr,
                 "peer does not allow at least 3 unidirectional streams.");
    return std::unexpected{Error::QUIC};
  }

  static constexpr auto callbacks = nghttp3_callbacks{
    .recv_data = ::http_recv_data,
    .deferred_consume = ::http_deferred_consume,
    .begin_headers = ::http_begin_headers,
    .recv_header = ::http_recv_header,
    .end_headers = ::http_end_headers,
    .begin_trailers = ::http_begin_trailers,
    .recv_trailer = ::http_recv_trailer,
    .end_trailers = ::http_end_trailers,
    .stop_sending = ::http_stop_sending,
    .reset_stream = ::http_reset_stream,
    .recv_origin = ::http_recv_origin,
    .end_origin = ::http_end_origin,
    .rand = rand_bytes,
    .recv_settings2 = ::http_recv_settings,
  };
  nghttp3_settings settings;
  nghttp3_settings_default(&settings);
  settings.qpack_max_dtable_capacity = 4_k;
  settings.qpack_blocked_streams = 100;

  auto mem = nghttp3_mem_default();

  if (auto rv =
        nghttp3_conn_client_new(&httpconn_, &callbacks, &settings, mem, this);
      rv != 0) {
    std::println(stderr, "nghttp3_conn_client_new: {}", nghttp3_strerror(rv));
    return std::unexpected{Error::HTTP3};
  }

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

} // namespace ngtcp2
