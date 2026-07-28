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
#include "hq_server_proto_codec.h"

#include "server.h"
#include "debug.h"

extern Config config;

namespace ngtcp2 {

bool StreamIDLess::operator()(const Stream *lhs, const Stream *rhs) const {
  return lhs->stream_id < rhs->stream_id;
}

ProtoCodec::ProtoCodec(Handler *h, ngtcp2_ccerr &last_error)
  : handler_{h}, conn_{handler_->conn()}, last_error_{last_error} {}

namespace {
int on_msg_begin(http_parser *htp) {
  auto s = static_cast<Stream *>(htp->data);
  if (s->eos) {
    return -1;
  }
  return 0;
}
} // namespace

namespace {
int on_url_cb(http_parser *htp, const char *data, size_t datalen) {
  auto s = static_cast<Stream *>(htp->data);
  s->uri.append(data, datalen);
  return 0;
}
} // namespace

namespace {
int on_msg_complete(http_parser *htp) {
  auto s = static_cast<Stream *>(htp->data);
  s->eos = true;
  s->start_response();
  return 0;
}
} // namespace

constexpr auto htp_settings = http_parser_settings{
  .on_message_begin = on_msg_begin,
  .on_url = on_url_cb,
  .on_message_complete = on_msg_complete,
};

std::expected<void, Error>
ProtoCodec::recv_stream_data(uint32_t flags, int64_t stream_id,
                             std::span<const uint8_t> data) {
  if (!config.quiet && !config.no_quic_dump) {
    debug::print_stream_data(stream_id, data);
  }

  auto stream = handler_->find_stream(stream_id);
  if (!stream) {
    return {};
  }

  if (!stream->eos) {
    auto nread = http_parser_execute(
      &stream->htp, &htp_settings, reinterpret_cast<const char *>(data.data()),
      data.size());
    if (nread != data.size()) {
      if (auto rv = ngtcp2_conn_shutdown_stream(conn_, 0, stream_id,
                                                /* app error code */ 1);
          rv != 0) {
        std::println(stderr, "ngtcp2_conn_shutdown_stream: {}",
                     ngtcp2_strerror(rv));
        ngtcp2_ccerr_set_liberr(&last_error_, NGTCP2_ERR_INTERNAL, nullptr, 0);
        return std::unexpected{Error::QUIC};
      }
    }
  }

  ngtcp2_conn_extend_max_stream_offset(conn_, stream_id, data.size());
  ngtcp2_conn_extend_max_offset(conn_, data.size());

  return {};
}

std::expected<void, Error>
ProtoCodec::extend_max_stream_data(int64_t stream_id, uint64_t max_data) {
  auto stream = handler_->find_stream(stream_id);
  if (!stream) {
    return {};
  }

  if (!stream->resp_data.empty()) {
    sendq_.emplace(stream);
  }

  return {};
}

ngtcp2_ssize ProtoCodec::write_pkt(ngtcp2_path *path, ngtcp2_pkt_info *pi,
                                   uint8_t *dest, size_t destlen,
                                   ngtcp2_tstamp ts) {
  ngtcp2_vec vec;

  for (;;) {
    int64_t stream_id = -1;
    size_t vcnt = 0;
    uint32_t flags =
      NGTCP2_WRITE_STREAM_FLAG_MORE | NGTCP2_WRITE_STREAM_FLAG_PADDING;
    Stream *stream = nullptr;

    if (!sendq_.empty() && ngtcp2_conn_get_max_data_left2(conn_)) {
      stream = *std::ranges::begin(sendq_);

      stream_id = stream->stream_id;
      vec.base = const_cast<uint8_t *>(stream->resp_data.data());
      vec.len = stream->resp_data.size();
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
        stream->resp_data = stream->resp_data.subspan(as_unsigned(ndatalen));
        if (stream->resp_data.empty()) {
          sendq_.erase(std::ranges::begin(sendq_));
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
      stream->resp_data = stream->resp_data.subspan(as_unsigned(ndatalen));
      if (stream->resp_data.empty()) {
        sendq_.erase(std::ranges::begin(sendq_));
      }
    }

    return nwrite;
  }
}

std::expected<void, Error>
ProtoCodec::on_stream_close(int64_t stream_id, uint64_t app_error_code) {
  auto stream = handler_->find_stream(stream_id);
  if (!stream) {
    return {};
  }

  sendq_.erase(stream);

  if (!config.quiet) {
    std::println(stderr, "HTTP stream {:#x} closed with error code {:#x}",
                 stream_id, app_error_code);
  }

  return {};
}

std::expected<void, Error>
ProtoCodec::send_status_response(Stream *stream, unsigned int status_code) {
  stream->status_resp_body = make_status_body(status_code);
  stream->resp_data = as_uint8_span(std::span{stream->status_resp_body});

  sendq_.emplace(stream);

  handler_->shutdown_read(stream->stream_id, 0);

  return {};
}

std::expected<void, Error> ProtoCodec::start_response(Stream *stream) {
  if (stream->uri.empty()) {
    send_status_response(stream, 400);
    return {};
  }

  auto maybe_req = stream->request_path();
  if (!maybe_req) {
    send_status_response(stream, 400);
    return {};
  }

  const auto &req = *maybe_req;

  auto path = config.htdocs;
  path /= std::filesystem::path{req.path}.relative_path();

  auto maybe_fe = stream->open_file(path);
  if (!maybe_fe) {
    send_status_response(stream, 404);
    return {};
  }

  const auto &fe = *maybe_fe;

  if (fe.flags & FILE_ENTRY_TYPE_DIR) {
    send_status_response(stream, 404);
    return {};
  }

  stream->map_file(fe);

  if (!config.quiet) {
    auto nva = std::to_array({
      util::make_nv_nn(":status", "200"),
    });

    debug::print_http_response_headers(stream->stream_id, nva.data(),
                                       nva.size());
  }

  sendq_.emplace(stream);

  return {};
}

} // namespace ngtcp2
