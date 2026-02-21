/*
 * ngtcp2
 *
 * Copyright (c) 2020 ngtcp2 contributors
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
#include "tls_server_session_quictls.h"

#include <iostream>

#include <openssl/err.h>

#include "tls_server_context_quictls.h"
#include "server_base.h"

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
#ifndef LIBRESSL_VERSION_NUMBER
  SSL_set_quic_early_data_enabled(ssl_, 1);
#endif // !defined(LIBRESSL_VERSION_NUMBER)

  return 0;
}
