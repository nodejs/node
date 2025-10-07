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
#include "tls_session_base_picotls.h"

TLSSessionBase::TLSSessionBase() { ngtcp2_crypto_picotls_ctx_init(&cptls_); }

TLSSessionBase::~TLSSessionBase() {
  ngtcp2_crypto_picotls_deconfigure_session(&cptls_);

  delete[] cptls_.handshake_properties.additional_extensions;

  if (cptls_.ptls) {
    ptls_free(cptls_.ptls);
  }
}

ngtcp2_crypto_picotls_ctx *TLSSessionBase::get_native_handle() {
  return &cptls_;
}

std::string TLSSessionBase::get_cipher_name() const {
  auto cs = ptls_get_cipher(cptls_.ptls);
  return cs->aead->name;
}

std::string TLSSessionBase::get_selected_alpn() const {
  auto alpn = ptls_get_negotiated_protocol(cptls_.ptls);

  if (!alpn) {
    return {};
  }

  return alpn;
}
