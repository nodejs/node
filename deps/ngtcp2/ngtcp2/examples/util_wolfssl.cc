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
#include "util.h"

#include <cassert>
#include <iostream>
#include <array>
#include <algorithm>
#include <expected>

#include <ngtcp2/ngtcp2_crypto.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/openssl/pem.h>

#include "template.h"

namespace ngtcp2 {

namespace util {

std::expected<void, Error> generate_secure_random(std::span<uint8_t> data) {
  if (wolfSSL_RAND_bytes(data.data(), static_cast<int>(data.size())) != 1) {
    return std::unexpected{Error::CRYPTO};
  }

  return {};
}

std::expected<HPKEPrivateKey, Error>
read_hpke_private_key_pem(std::string_view filename) {
  return std::unexpected{Error::NOT_IMPLEMENTED};
}

std::expected<std::vector<uint8_t>, Error> read_pem(std::string_view filename,
                                                    std::string_view name,
                                                    std::string_view type) {
  auto f = wolfSSL_BIO_new_file(filename.data(), "r");
  if (f == nullptr) {
    std::println(stderr, "Could not open {} file {}", name, filename);
    return std::unexpected{Error::IO};
  }

  auto f_d = defer([f] { wolfSSL_BIO_free(f); });

  char *pem_type, *header;
  unsigned char *data;
  long datalen;

  if (wolfSSL_PEM_read_bio(f, &pem_type, &header, &data, &datalen) != 1) {
    std::println(stderr, "Could not read {} file {}", name, filename);
    return std::unexpected{Error::IO};
  }

  auto pem_d = defer([pem_type, header, data] {
    wolfSSL_OPENSSL_free(pem_type);
    wolfSSL_OPENSSL_free(header);
    wolfSSL_OPENSSL_free(data);
  });

  if (type != pem_type) {
    std::println(stderr, "{} file {} contains unexpected type", name, filename);
    return std::unexpected{Error::IO};
  }

  return {{data, data + datalen}};
}

std::expected<void, Error> write_pem(std::string_view filename,
                                     std::string_view name,
                                     std::string_view type,
                                     std::span<const uint8_t> data) {
  auto f = wolfSSL_BIO_new_file(filename.data(), "w");
  if (f == nullptr) {
    std::println(stderr, "Could not write {} to {}", name, filename);
    return std::unexpected{Error::IO};
  }

  wolfSSL_PEM_write_bio(f, type.data(), "", data.data(),
                        static_cast<long>(data.size()));
  wolfSSL_BIO_free(f);

  return {};
}

const char *crypto_default_ciphers() {
  return "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_"
         "SHA256:TLS_AES_128_CCM_SHA256";
}

const char *crypto_default_groups() {
  return "X25519:P-256:P-384:P-521"
#ifdef WOLFSSL_HAVE_MLKEM
#  if LIBWOLFSSL_VERSION_HEX < 0x05008004
         ":X25519_ML_KEM_768"
#  else  // LIBWOLFSSL_VERSION_HEX >= 0x05008004
         ":X25519MLKEM768"
#  endif // LIBWOLFSSL_VERSION_HEX >= 0x05008004
#endif   // WOLFSSL_HAVE_MLKEM
    ;
}

} // namespace util

} // namespace ngtcp2
