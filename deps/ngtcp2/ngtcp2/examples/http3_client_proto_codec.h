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
#ifndef HTTP3_CLIENT_PROTO_CODEC_H
#define HTTP3_CLIENT_PROTO_CODEC_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <vector>
#include <expected>

#include <ngtcp2/ngtcp2.h>
#include <nghttp3/nghttp3.h>

#include "shared.h"
#include "client_base.h"

struct Stream;
class Client;

namespace ngtcp2 {

class ProtoCodec {
public:
  ProtoCodec(Client *handler, ngtcp2_ccerr &last_error);
  ~ProtoCodec();

  std::expected<void, Error> recv_stream_data(uint32_t flags, int64_t stream_id,
                                              std::span<const uint8_t> data);

  std::expected<void, Error> acked_stream_data_offset(int64_t stream_id,
                                                      uint64_t datalen);

  std::expected<void, Error> extend_max_stream_data(int64_t stream_id,
                                                    uint64_t max_data);

  void early_data_rejected();

  std::expected<void, Error> on_stream_close(int64_t stream_id,
                                             uint64_t app_error_code);

  std::expected<void, Error> on_stream_reset(int64_t stream_id);

  std::expected<void, Error> on_stream_stop_sending(int64_t stream_id);

  std::expected<void, Error> submit_request(const Stream *stream);

  ngtcp2_ssize write_pkt(ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
                         size_t destlen, ngtcp2_tstamp ts);

  std::expected<void, Error> setup_codec();

  // The following functions are made public so that they can be
  // called from nghttp3 callback functions.
  std::expected<void, Error> stop_sending(int64_t stream_id,
                                          uint64_t app_error_code);

  std::expected<void, Error> reset_stream(int64_t stream_id,
                                          uint64_t app_error_code);

  void http_consume(int64_t stream_id, size_t nconsumed);

  void http_write_data(int64_t stream_id, std::span<const uint8_t> data);

  static constexpr auto protocol = AppProtocol::H3;
  static constexpr auto no_error = NGHTTP3_H3_NO_ERROR;

private:
  void http_stream_close(int64_t stream_id, uint64_t app_error_code);

  Client *client_;
  ngtcp2_conn *conn_;
  ngtcp2_ccerr &last_error_;
  nghttp3_conn *httpconn_{};
};

} // namespace ngtcp2

#endif // !defined(HTTP3_CLIENT_PROTO_CODEC_H)
