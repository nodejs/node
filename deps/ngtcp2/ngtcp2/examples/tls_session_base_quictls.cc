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
#include "tls_session_base_quictls.h"

#include <array>

#include "util.h"
#include "template.h"

using namespace ngtcp2;
using namespace std::literals;

TLSSessionBase::~TLSSessionBase() {
  if (ssl_) {
    SSL_free(ssl_);
  }
}

SSL *TLSSessionBase::get_native_handle() const { return ssl_; }

std::string TLSSessionBase::get_cipher_name() const {
  return SSL_get_cipher_name(ssl_);
}

std::string_view TLSSessionBase::get_negotiated_group() const {
#ifdef WITH_EXAMPLE_BORINGSSL
  return SSL_get_group_name(SSL_get_group_id(ssl_));
#elif OPENSSL_VERSION_NUMBER >= 0x30000000L
  auto name =
    SSL_group_to_name(ssl_, static_cast<int>(SSL_get_negotiated_group(ssl_)));
  if (!name) {
    return ""sv;
  }

  return name;
#elif defined(LIBRESSL_VERSION_NUMBER)
  return ""sv;
#else  // !(defined(WITH_EXAMPLE_BORINGSSL) ||
       // OPENSSL_VERSION_NUMBER >= 0x30000000L ||
       // defined(LIBRESSL_VERSION_NUMBER))
  EVP_PKEY *key;

  if (!SSL_get_tmp_key(ssl_, &key)) {
    return ""sv;
  }

  auto key_del = defer([key] { EVP_PKEY_free(key); });

  auto nid = EVP_PKEY_id(key);
  if (nid == EVP_PKEY_EC) {
    auto ec = EVP_PKEY_get0_EC_KEY(key);

    nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec));
  }

  auto name = EC_curve_nid2nist(nid);
  if (!name) {
    name = OBJ_nid2sn(nid);
    if (!name) {
      return ""sv;
    }
  }

  return name;
#endif // !(defined(WITH_EXAMPLE_BORINGSSL) ||
       // OPENSSL_VERSION_NUMBER >= 0x30000000L ||
       // defined(LIBRESSL_VERSION_NUMBER))
}

std::string TLSSessionBase::get_selected_alpn() const {
  const unsigned char *alpn = nullptr;
  unsigned int alpnlen;

  SSL_get0_alpn_selected(ssl_, &alpn, &alpnlen);

  return std::string{alpn, alpn + alpnlen};
}
