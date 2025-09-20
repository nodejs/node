/*
 * ngtcp2
 *
 * Copyright (c) 2022 ngtcp2 contributors
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
#include "tls_server_session_picotls.h"

#include <cassert>
#include <iostream>

#include <ngtcp2/ngtcp2_crypto_picotls.h>

#include "tls_server_context_picotls.h"
#include "server_base.h"
#include "util.h"

using namespace ngtcp2;

extern Config config;

TLSServerSession::TLSServerSession() {}

TLSServerSession::~TLSServerSession() {}

int TLSServerSession::init(TLSServerContext &tls_ctx, HandlerBase *handler) {
  cptls_.ptls = ptls_server_new(tls_ctx.get_native_handle());
  if (!cptls_.ptls) {
    std::cerr << "ptls_server_new failed" << std::endl;
    return -1;
  }

  *ptls_get_data_ptr(cptls_.ptls) = handler->conn_ref();

  cptls_.handshake_properties.additional_extensions =
    new ptls_raw_extension_t[2]{
      {
        .type = UINT16_MAX,
      },
      {
        .type = UINT16_MAX,
      },
    };

  if (ngtcp2_crypto_picotls_configure_server_session(&cptls_) != 0) {
    std::cerr << "ngtcp2_crypto_picotls_configure_server_session failed"
              << std::endl;
    return -1;
  }

  return 0;
}
