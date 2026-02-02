/*
 * ngtcp2
 *
 * Copyright (c) 2025 ngtcp2 contributors
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
#include "tls_server_session_ossl.h"

#include <iostream>

#include <openssl/err.h>

#include "tls_server_context_ossl.h"
#include "server_base.h"

TLSServerSession::TLSServerSession() {}

TLSServerSession::~TLSServerSession() {}

int TLSServerSession::init(const TLSServerContext &tls_ctx,
                           HandlerBase *handler) {
  auto ssl_ctx = tls_ctx.get_native_handle();

  auto ssl = SSL_new(ssl_ctx);
  if (!ssl) {
    std::cerr << "SSL_new: " << ERR_error_string(ERR_get_error(), nullptr)
              << std::endl;
    return -1;
  }

  ngtcp2_crypto_ossl_ctx_set_ssl(ossl_ctx_, ssl);

  if (ngtcp2_crypto_ossl_configure_server_session(ssl) != 0) {
    std::cerr << "ngtcp2_crypto_ossl_configure_server_session failed"
              << std::endl;
    return -1;
  }

  SSL_set_app_data(ssl, handler->conn_ref());
  SSL_set_accept_state(ssl);
  SSL_set_quic_tls_early_data_enabled(ssl, 1);

  return 0;
}
