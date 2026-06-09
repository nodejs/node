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
#include "hq_client_proto_codec.h"

#include <unistd.h>

#include "client.h"
#include "debug.h"

extern Config config;

namespace ngtcp2 {

bool StreamIDLess::operator()(const Stream *lhs, const Stream *rhs) const {
  return lhs->stream_id < rhs->stream_id;
}

ProtoCodec::ProtoCodec(Client *c, ngtcp2_ccerr &last_error)
  : client_{c}, conn_{client_->conn()}, last_error_{last_error} {}

std::expected<void, Error>
ProtoCodec::recv_stream_data(uint32_t flags, int64_t stream_id,
                             std::span<const uint8_t> data) {
  auto stream = client_->find_stream(stream_id);
  if (!stream) {
    return {};
  }

  ngtcp2_conn_extend_max_stream_offset(conn_, stream_id, data.size());
  ngtcp2_conn_extend_max_offset(conn_, data.size());

  if (stream->fd == -1) {
    return {};
  }

  ssize_t nwrite;

  for (; !data.empty();) {
    do {
      nwrite = write(stream->fd, data.data(), data.size());
    } while (nwrite == -1 && errno == EINTR);

    if (nwrite < 0) {
      std::println(stderr, "Could not write data to file: {}", strerror(errno));

      return {};
    }

    data = data.subspan(static_cast<size_t>(nwrite));
  }

  return {};
}

std::expected<void, Error>
ProtoCodec::on_stream_close(int64_t stream_id, uint64_t app_error_code) {
  if (!ngtcp2_is_bidi_stream(stream_id)) {
    return {};
  }

  if (!config.quiet) {
    std::println(stderr, "HTTP stream {:#x} closed with error code {:#x}",
                 stream_id, app_error_code);
  }

  return {};
}

std::expected<void, Error>
ProtoCodec::extend_max_stream_data(int64_t stream_id, uint64_t max_data) {
  auto stream = client_->find_stream(stream_id);
  if (!stream) {
    return {};
  }

  if (!stream->reqbuf.empty()) {
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
    uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_MORE;
    Stream *stream = nullptr;

    if (!sendq_.empty() && ngtcp2_conn_get_max_data_left2(conn_)) {
      stream = *std::ranges::begin(sendq_);

      stream_id = stream->stream_id;
      vec.base = const_cast<uint8_t *>(stream->reqbuf.data());
      vec.len = stream->reqbuf.size();
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
        stream->reqbuf = stream->reqbuf.subspan(as_unsigned(ndatalen));
        if (stream->reqbuf.empty()) {
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
      stream->reqbuf = stream->reqbuf.subspan(as_unsigned(ndatalen));
      if (stream->reqbuf.empty()) {
        sendq_.erase(std::ranges::begin(sendq_));
      }
    }

    return nwrite;
  }
}

std::expected<void, Error> ProtoCodec::submit_request(Stream *stream) {
  const auto &req = stream->req;

  stream->rawreqbuf = config.http_method;
  stream->rawreqbuf += ' ';
  stream->rawreqbuf += req.path;
  stream->rawreqbuf += "\r\n";

  stream->reqbuf = as_uint8_span(std::span{stream->rawreqbuf});

  if (!config.quiet) {
    auto nva = std::to_array({
      util::make_nv_nn(":method", config.http_method),
      util::make_nv_nn(":path", req.path),
    });
    debug::print_http_request_headers(stream->stream_id, nva.data(),
                                      nva.size());
  }

  sendq_.emplace(stream);

  return {};
}

} // namespace ngtcp2
