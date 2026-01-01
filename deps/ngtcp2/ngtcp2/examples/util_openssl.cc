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

#include <ngtcp2/ngtcp2_crypto.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "template.h"

namespace ngtcp2 {

namespace util {

int generate_secure_random(std::span<uint8_t> data) {
#ifdef WITH_EXAMPLE_BORINGSSL
  using size_type = size_t;
#else  // !defined(WITH_EXAMPLE_BORINGSSL)
  using size_type = int;
#endif // !defined(WITH_EXAMPLE_BORINGSSL)

  if (RAND_bytes(data.data(), static_cast<size_type>(data.size())) != 1) {
    return -1;
  }

  return 0;
}

int generate_secret(std::span<uint8_t> secret) {
  std::array<uint8_t, 16> rand;

  if (generate_secure_random(rand) != 0) {
    return -1;
  }

  auto ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    return -1;
  }

  auto ctx_deleter = defer([ctx] { EVP_MD_CTX_free(ctx); });

  static const auto sha256 =
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MD_fetch(nullptr, "sha256", nullptr);
#else  // OPENSSL_VERSION_NUMBER < 0x30000000L
    EVP_sha256();
#endif // OPENSSL_VERSION_NUMBER < 0x30000000L

  auto mdlen = static_cast<unsigned int>(secret.size());
  if (!EVP_DigestInit_ex(ctx, sha256, nullptr) ||
      !EVP_DigestUpdate(ctx, rand.data(), rand.size()) ||
      !EVP_DigestFinal_ex(ctx, secret.data(), &mdlen)) {
    return -1;
  }

  return 0;
}

std::optional<HPKEPrivateKey>
read_hpke_private_key_pem(const std::string_view &filename) {
  auto f = BIO_new_file(filename.data(), "r");
  if (f == nullptr) {
    std::cerr << "Could not open file " << filename << std::endl;
    return {};
  }

  auto f_d = defer([f] { BIO_free(f); });

  EVP_PKEY *pkey;

  if (PEM_read_bio_PrivateKey(f, &pkey, nullptr, nullptr) == nullptr) {
    return {};
  }

  auto pkey_d = defer([pkey] { EVP_PKEY_free(pkey); });

  HPKEPrivateKey res;

  switch (EVP_PKEY_id(pkey)) {
  case EVP_PKEY_X25519: {
    res.type = HPKE_DHKEM_X25519_HKDF_SHA256;

    size_t len;

    EVP_PKEY_get_raw_private_key(pkey, nullptr, &len);

    res.bytes.resize(len);

    EVP_PKEY_get_raw_private_key(pkey, &res.bytes[0], &len);

    break;
  }
  default:
    return {};
  }

  return res;
}

std::optional<std::vector<uint8_t>> read_pem(const std::string_view &filename,
                                             const std::string_view &name,
                                             const std::string_view &type) {
  auto f = BIO_new_file(filename.data(), "r");
  if (f == nullptr) {
    std::cerr << "Could not open " << name << " file " << filename << std::endl;
    return {};
  }

  auto f_d = defer([f] { BIO_free(f); });

  for (;;) {
    char *pem_type, *header;
    unsigned char *data;
    long datalen;

    if (PEM_read_bio(f, &pem_type, &header, &data, &datalen) != 1) {
      std::cerr << "Could not read " << name << " file " << filename
                << std::endl;
      return {};
    }

    auto pem_d = defer([pem_type, header, data] {
      OPENSSL_free(pem_type);
      OPENSSL_free(header);
      OPENSSL_free(data);
    });

    if (type != pem_type) {
      continue;
    }

    return {{data, data + datalen}};
  }
}

int write_pem(const std::string_view &filename, const std::string_view &name,
              const std::string_view &type, std::span<const uint8_t> data) {
  auto f = BIO_new_file(filename.data(), "w");
  if (f == nullptr) {
    std::cerr << "Could not write " << name << " in " << filename << std::endl;
    return -1;
  }

  PEM_write_bio(f, type.data(), "", data.data(),
                static_cast<long>(data.size()));
  BIO_free(f);

  return 0;
}

const char *crypto_default_ciphers() {
#if defined(WITH_EXAMPLE_QUICTLS) || defined(WITH_EXAMPLE_OSSL)
  return "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_"
         "SHA256"
#  ifndef LIBRESSL_VERSION_NUMBER
         ":TLS_AES_128_CCM_SHA256"
#  endif // !defined(LIBRESSL_VERSION_NUMBER)
    ;
#else  // !(defined(WITH_EXAMPLE_QUICTLS) && defined(WITH_EXAMPLE_OSSL))
  return "";
#endif // !(defined(WITH_EXAMPLE_QUICTLS) && defined(WITH_EXAMPLE_OSSL))
}

const char *crypto_default_groups() {
  return "X25519:P-256:P-384:P-521"
#if defined(WITH_EXAMPLE_BORINGSSL) || defined(WITH_EXAMPLE_OSSL)
         ":X25519MLKEM768"
#endif // defined(WITH_EXAMPLE_BORINGSSL) || defined(WITH_EXAMPLE_OSSL)
    ;
}

} // namespace util

} // namespace ngtcp2
