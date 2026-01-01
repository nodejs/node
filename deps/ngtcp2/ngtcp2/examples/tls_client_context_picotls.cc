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
#include "tls_client_context_picotls.h"

#include <iostream>

#include <ngtcp2/ngtcp2_crypto_picotls.h>

#include <openssl/bio.h>
#include <openssl/pem.h>

#include "client_base.h"
#include "tls_shared_picotls.h"
#include "template.h"

extern Config config;

namespace {
int save_ticket_cb(ptls_save_ticket_t *self, ptls_t *ptls, ptls_iovec_t input) {
  auto conn_ref =
    static_cast<ngtcp2_crypto_conn_ref *>(*ptls_get_data_ptr(ptls));
  auto c = static_cast<ClientBase *>(conn_ref->user_data);

  c->ticket_received();

  auto f = BIO_new_file(config.session_file, "w");
  if (f == nullptr) {
    std::cerr << "Could not write TLS session in " << config.session_file
              << std::endl;
    return 0;
  }

  if (!PEM_write_bio(f, "PICOTLS SESSION PARAMETERS", "", input.base,
                     static_cast<long>(input.len))) {
    std::cerr << "Unable to write TLS session to file" << std::endl;
  }

  BIO_free(f);

  return 0;
}

ptls_save_ticket_t save_ticket = {save_ticket_cb};
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

TLSClientContext::TLSClientContext()
  : ctx_{
      .random_bytes = ptls_openssl_random_bytes,
      .get_time = &ptls_get_time,
      .key_exchanges = key_exchanges,
      .cipher_suites = cipher_suites,
      .require_dhe_on_psk = 1,
    } {}

TLSClientContext::~TLSClientContext() {
  if (sign_cert_.key) {
    ptls_openssl_dispose_sign_certificate(&sign_cert_);
  }

  for (size_t i = 0; i < ctx_.certificates.count; ++i) {
    free(ctx_.certificates.list[i].base);
  }
  free(ctx_.certificates.list);
}

ptls_context_t *TLSClientContext::get_native_handle() { return &ctx_; }

int TLSClientContext::init(const char *private_key_file,
                           const char *cert_file) {
  if (ngtcp2_crypto_picotls_configure_client_context(&ctx_) != 0) {
    std::cerr << "ngtcp2_crypto_picotls_configure_client_context failed"
              << std::endl;
    return -1;
  }

  if (config.session_file) {
    ctx_.save_ticket = &save_ticket;
  }

  if (private_key_file && cert_file) {
    if (ptls_load_certificates(&ctx_, cert_file) != 0) {
      std::cerr << "ptls_load_certificates failed" << std::endl;
      return -1;
    }

    if (load_private_key(private_key_file) != 0) {
      return -1;
    }
  }

  return 0;
}

int TLSClientContext::load_private_key(const char *private_key_file) {
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

void TLSClientContext::enable_keylog() { ctx_.log_event = &log_event; }
