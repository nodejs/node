/*
 * ngtcp2
 *
 * Copyright (c) 2021 ngtcp2 contributors
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
#include "tls_server_session_boringssl.h"

#include <cassert>
#include <iostream>

#include <ngtcp2/ngtcp2.h>

#include "tls_server_context_boringssl.h"
#include "server_base.h"

extern Config config;

TLSServerSession::TLSServerSession() {}

TLSServerSession::~TLSServerSession() {}

int TLSServerSession::init(const TLSServerContext &tls_ctx,
                           HandlerBase *handler) {
  auto ssl_ctx = tls_ctx.get_native_handle();

  ssl_ = SSL_new(ssl_ctx);
  if (!ssl_) {
    std::cerr << "SSL_new: " << ERR_error_string(ERR_get_error(), nullptr)
              << std::endl;
    return -1;
  }

  SSL_set_app_data(ssl_, handler->conn_ref());
  SSL_set_accept_state(ssl_);
  SSL_set_early_data_enabled(ssl_, 1);

  std::array<uint8_t, 128> quic_early_data_ctx;
  ngtcp2_transport_params params;
  ngtcp2_transport_params_default(&params);
  params.initial_max_streams_bidi = config.max_streams_bidi;
  params.initial_max_streams_uni = config.max_streams_uni;
  params.initial_max_stream_data_bidi_local = config.max_stream_data_bidi_local;
  params.initial_max_stream_data_bidi_remote =
    config.max_stream_data_bidi_remote;
  params.initial_max_stream_data_uni = config.max_stream_data_uni;
  params.initial_max_data = config.max_data;

  auto quic_early_data_ctxlen = ngtcp2_transport_params_encode(
    quic_early_data_ctx.data(), quic_early_data_ctx.size(), &params);
  if (quic_early_data_ctxlen < 0) {
    std::cerr << "ngtcp2_transport_params_encode: "
              << ngtcp2_strerror(static_cast<int>(quic_early_data_ctxlen))
              << std::endl;
    return -1;
  }

  if (SSL_set_quic_early_data_context(ssl_, quic_early_data_ctx.data(),
                                      as_unsigned(quic_early_data_ctxlen)) !=
      1) {
    std::cerr << "SSL_set_quic_early_data_context failed" << std::endl;
    return -1;
  }

  return 0;
}
