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
#include "tls_server_context_picotls.h"

#include <iostream>
#include <memory>
#include <algorithm>

#include <ngtcp2/ngtcp2_crypto_picotls.h>

#include <openssl/pem.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#  include <openssl/core_names.h>
#endif // OPENSSL_VERSION_NUMBER >= 0x30000000L

#include "server_base.h"
#include "tls_shared_picotls.h"
#include "template.h"

extern Config config;

namespace {
int on_client_hello_h3_cb(ptls_on_client_hello_t *self, ptls_t *ptls,
                          ptls_on_client_hello_parameters_t *params) {
  auto &negprotos = params->negotiated_protocols;

  for (size_t i = 0; i < negprotos.count; ++i) {
    auto &proto = negprotos.list[i];
    if (std::ranges::equal(H3_ALPN_V1.subspan(1),
                           std::span{proto.base, proto.len})) {
      if (ptls_set_negotiated_protocol(
            ptls, reinterpret_cast<char *>(proto.base), proto.len) != 0) {
        return -1;
      }

      return 0;
    }
  }

  return PTLS_ALERT_NO_APPLICATION_PROTOCOL;
}

ptls_on_client_hello_t on_client_hello_h3 = {on_client_hello_h3_cb};
} // namespace

namespace {
int on_client_hello_hq_cb(ptls_on_client_hello_t *self, ptls_t *ptls,
                          ptls_on_client_hello_parameters_t *params) {
  auto &negprotos = params->negotiated_protocols;

  for (size_t i = 0; i < negprotos.count; ++i) {
    auto &proto = negprotos.list[i];
    if (std::ranges::equal(HQ_ALPN_V1.subspan(1),
                           std::span{proto.base, proto.len})) {
      if (ptls_set_negotiated_protocol(
            ptls, reinterpret_cast<char *>(proto.base), proto.len) != 0) {
        return -1;
      }

      return 0;
    }
  }

  return PTLS_ALERT_NO_APPLICATION_PROTOCOL;
}

ptls_on_client_hello_t on_client_hello_hq = {on_client_hello_hq_cb};
} // namespace

namespace {
std::span<const uint8_t> get_ticket_key_name() {
  static std::array<uint8_t, 16> key_name;
  ptls_openssl_random_bytes(key_name.data(), key_name.size());
  return key_name;
}

std::span<const uint8_t> get_ticket_key() {
  static std::array<uint8_t, 32> key;
  ptls_openssl_random_bytes(key.data(), key.size());
  return key;
}

std::span<const uint8_t> get_ticket_hmac_key() {
  static std::array<uint8_t, 32> hmac_key;
  ptls_openssl_random_bytes(hmac_key.data(), hmac_key.size());
  return hmac_key;
}
} // namespace

namespace {
int ticket_key_cb(unsigned char *key_name, unsigned char *iv,
                  EVP_CIPHER_CTX *ctx,
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
                  EVP_MAC_CTX *hctx,
#else  // OPENSSL_VERSION_NUMBER < 0x30000000L
                  HMAC_CTX *hctx,
#endif // OPENSSL_VERSION_NUMBER < 0x30000000L
                  int enc) {
  static const auto static_key_name = get_ticket_key_name();
  static const auto static_key = get_ticket_key();
  static const auto static_hmac_key = get_ticket_hmac_key();
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  static const auto ticket_hmac = EVP_MD_fetch(nullptr, "sha256", nullptr);
  static const auto aes_256_cbc =
    EVP_CIPHER_fetch(nullptr, "AES-256-CBC", nullptr);
#else  // OPENSSL_VERSION_NUMBER < 0x30000000L
  static const auto ticket_hmac = EVP_sha256();
  static const auto aes_256_cbc = EVP_aes_256_cbc();
#endif // OPENSSL_VERSION_NUMBER < 0x30000000L

  if (enc) {
    ptls_openssl_random_bytes(iv, EVP_MAX_IV_LENGTH);

    std::ranges::copy(static_key_name, key_name);

    if (!EVP_EncryptInit_ex(ctx, aes_256_cbc, nullptr, static_key.data(), iv)) {
      return 0;
    }
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    auto params = std::to_array({
      OSSL_PARAM_construct_octet_string(
        OSSL_MAC_PARAM_KEY, const_cast<uint8_t *>(static_hmac_key.data()),
        static_hmac_key.size()),
      OSSL_PARAM_construct_utf8_string(
        OSSL_MAC_PARAM_DIGEST,
        const_cast<char *>(EVP_MD_get0_name(ticket_hmac)), 0),
      OSSL_PARAM_construct_end(),
    });
    if (!EVP_MAC_CTX_set_params(hctx, params.data())) {
      /* TODO Which value should we return on error? */
      return 0;
    }
#else  // OPENSSL_VERSION_NUMBER < 0x30000000L
    if (!HMAC_Init_ex(hctx, static_hmac_key.data(),
                      static_cast<int>(static_hmac_key.size()), ticket_hmac,
                      nullptr)) {
      return 0;
    }
#endif // OPENSSL_VERSION_NUMBER < 0x30000000L

    return 1;
  }

  if (!std::ranges::equal(std::span{key_name, static_key_name.size()},
                          static_key_name)) {
    return 0;
  }

  if (!EVP_DecryptInit_ex(ctx, aes_256_cbc, nullptr, static_key.data(), iv)) {
    return 0;
  }
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  auto params = std::to_array({
    OSSL_PARAM_construct_octet_string(
      OSSL_MAC_PARAM_KEY, const_cast<uint8_t *>(static_hmac_key.data()),
      static_hmac_key.size()),
    OSSL_PARAM_construct_utf8_string(
      OSSL_MAC_PARAM_DIGEST, const_cast<char *>(EVP_MD_get0_name(ticket_hmac)),
      0),
    OSSL_PARAM_construct_end(),
  });
  if (!EVP_MAC_CTX_set_params(hctx, params.data())) {
    /* TODO Which value should we return on error? */
    return 0;
  }
#else  // OPENSSL_VERSION_NUMBER < 0x30000000L
  if (!HMAC_Init_ex(hctx, static_hmac_key.data(),
                    static_cast<int>(static_hmac_key.size()), ticket_hmac,
                    nullptr)) {
    return 0;
  }
#endif // OPENSSL_VERSION_NUMBER < 0x30000000L

  return 1;
}
} // namespace

namespace {
int encrypt_ticket_cb(ptls_encrypt_ticket_t *encrypt_ticket, ptls_t *ptls,
                      int is_encrypt, ptls_buffer_t *dst, ptls_iovec_t src) {
  int rv;
  auto conn_ref =
    static_cast<ngtcp2_crypto_conn_ref *>(*ptls_get_data_ptr(ptls));
  auto conn = conn_ref->get_conn(conn_ref);
  uint32_t ver;

  if (is_encrypt) {
    ver = htonl(ngtcp2_conn_get_negotiated_version(conn));
    // TODO Replace std::make_unique with
    // std::make_unique_for_overwrite when it is available.
    auto buf = std::make_unique<uint8_t[]>(src.len + sizeof(ver));
    auto p = std::ranges::copy_n(src.base, as_signed(src.len), buf.get()).out;
    p = std::ranges::copy_n(reinterpret_cast<uint8_t *>(&ver), sizeof(ver), p)
          .out;

    src.base = buf.get();
    src.len = as_unsigned(p - buf.get());

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    rv = ptls_openssl_encrypt_ticket_evp(dst, src, ticket_key_cb);
#else  // OPENSSL_VERSION_NUMBER < 0x30000000L
    rv = ptls_openssl_encrypt_ticket(dst, src, ticket_key_cb);
#endif // OPENSSL_VERSION_NUMBER < 0x30000000L
    if (rv != 0) {
      return -1;
    }

    return 0;
  }

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  rv = ptls_openssl_decrypt_ticket_evp(dst, src, ticket_key_cb);
#else  // OPENSSL_VERSION_NUMBER < 0x30000000L
  rv = ptls_openssl_decrypt_ticket(dst, src, ticket_key_cb);
#endif // OPENSSL_VERSION_NUMBER < 0x30000000L
  if (rv != 0) {
    return -1;
  }

  if (dst->off < sizeof(ver)) {
    return -1;
  }

  memcpy(&ver, dst->base + dst->off - sizeof(ver), sizeof(ver));

  if (ngtcp2_conn_get_client_chosen_version(conn) != ntohl(ver)) {
    return -1;
  }

  dst->off -= sizeof(ver);

  return 0;
}

ptls_encrypt_ticket_t encrypt_ticket = {encrypt_ticket_cb};
} // namespace

namespace {
ptls_key_exchange_algorithm_t *key_exchanges[] = {
#if PTLS_OPENSSL_HAVE_X25519
  &ptls_openssl_x25519,
#endif // PTLS_OPENSSL_X25519
  &ptls_openssl_secp256r1,
  &ptls_openssl_secp384r1,
  &ptls_openssl_secp521r1,
#if PTLS_OPENSSL_HAVE_X25519MLKEM768
  &ptls_openssl_x25519mlkem768,
#endif // PTLS_OPENSSL_HAVE_X25519MLKEM768
  nullptr,
};
} // namespace

namespace {
ptls_cipher_suite_t *cipher_suites[] = {
  &ptls_openssl_aes128gcmsha256,
  &ptls_openssl_aes256gcmsha384,
#if PTLS_OPENSSL_HAVE_CHACHA20_POLY1305
  &ptls_openssl_chacha20poly1305sha256,
#endif // PTLS_OPENSSL_CHACHA20POLY1305SHA256
  nullptr,
};
} // namespace

TLSServerContext::TLSServerContext()
  : ctx_{
      .random_bytes = ptls_openssl_random_bytes,
      .get_time = &ptls_get_time,
      .key_exchanges = key_exchanges,
      .cipher_suites = cipher_suites,
      .ticket_lifetime = 86400,
      .require_dhe_on_psk = 1,
      .server_cipher_preference = 1,
      .encrypt_ticket = &encrypt_ticket,
    } {}

TLSServerContext::~TLSServerContext() {
  if (sign_cert_.key) {
    ptls_openssl_dispose_sign_certificate(&sign_cert_);
  }

  for (size_t i = 0; i < ctx_.certificates.count; ++i) {
    free(ctx_.certificates.list[i].base);
  }
  free(ctx_.certificates.list);
}

ptls_context_t *TLSServerContext::get_native_handle() { return &ctx_; }

int TLSServerContext::init(const char *private_key_file, const char *cert_file,
                           AppProtocol app_proto) {
  switch (app_proto) {
  case AppProtocol::H3:
    ctx_.on_client_hello = &on_client_hello_h3;

    break;
  case AppProtocol::HQ:
    ctx_.on_client_hello = &on_client_hello_hq;

    break;
  }

  if (ngtcp2_crypto_picotls_configure_server_context(&ctx_) != 0) {
    std::cerr << "ngtcp2_crypto_picotls_configure_server_context failed"
              << std::endl;
    return -1;
  }

  if (ptls_load_certificates(&ctx_, cert_file) != 0) {
    std::cerr << "ptls_load_certificates failed" << std::endl;
    return -1;
  }

  if (load_private_key(private_key_file) != 0) {
    return -1;
  }

  if (config.verify_client) {
    ctx_.require_client_authentication = 1;
  }

  return 0;
}

int TLSServerContext::load_private_key(const char *private_key_file) {
  auto fp = fopen(private_key_file, "rb");
  if (fp == nullptr) {
    std::cerr << "Could not open private key file " << private_key_file << ": "
              << strerror(errno) << std::endl;
    return -1;
  }

  auto fp_d = defer([fp] { fclose(fp); });

  auto pkey = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
  if (pkey == nullptr) {
    std::cerr << "Could not read private key file " << private_key_file
              << std::endl;
    return -1;
  }

  auto pkey_d = defer([pkey] { EVP_PKEY_free(pkey); });

  if (ptls_openssl_init_sign_certificate(&sign_cert_, pkey) != 0) {
    std::cerr << "ptls_openssl_init_sign_certificate failed" << std::endl;
    return -1;
  }

  ctx_.sign_certificate = &sign_cert_.super;

  return 0;
}

void TLSServerContext::enable_keylog() { ctx_.log_event = &log_event; }
