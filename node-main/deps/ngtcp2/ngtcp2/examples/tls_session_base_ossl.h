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
#ifndef TLS_SESSION_BASE_OSSL_H
#define TLS_SESSION_BASE_OSSL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <string>
#include <string_view>

#include <ngtcp2/ngtcp2_crypto_ossl.h>

#include <openssl/ssl.h>

class TLSSessionBase {
public:
  TLSSessionBase();
  ~TLSSessionBase();

  ngtcp2_crypto_ossl_ctx *get_native_handle() const;

  std::string get_cipher_name() const;
  std::string_view get_negotiated_group() const;
  std::string get_selected_alpn() const;
  // Keylog is enabled per SSL_CTX.
  void enable_keylog() {}

protected:
  ngtcp2_crypto_ossl_ctx *ossl_ctx_;
};

#endif // !defined(TLS_SESSION_BASE_OSSL_H)
