/*
 * ngtcp2
 *
 * Copyright (c) 2021 ngtcp2 contributors
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
#include "tls_client_context_boringssl.h"

#include <cstring>
#include <iostream>
#include <fstream>

#include <ngtcp2/ngtcp2_crypto_boringssl.h>

#include <openssl/err.h>

#include "client_base.h"
#include "template.h"
#include "tls_shared_boringssl.h"

extern Config config;

TLSClientContext::~TLSClientContext() {
  if (ssl_ctx_) {
    SSL_CTX_free(ssl_ctx_);
  }
}

SSL_CTX *TLSClientContext::get_native_handle() const { return ssl_ctx_; }

namespace {
int new_session_cb(SSL *ssl, SSL_SESSION *session) {
  auto conn_ref = static_cast<ngtcp2_crypto_conn_ref *>(SSL_get_app_data(ssl));
  auto c = static_cast<ClientBase *>(conn_ref->user_data);

  c->ticket_received();

  auto f = BIO_new_file(config.session_file, "w");
  if (f == nullptr) {
    std::cerr << "Could not write TLS session in " << config.session_file
              << std::endl;
    return 0;
  }

  if (!PEM_write_bio_SSL_SESSION(f, session)) {
    std::cerr << "Unable to write TLS session to file" << std::endl;
  }

  BIO_free(f);

  return 0;
}
} // namespace

int TLSClientContext::init(const char *private_key_file,
                           const char *cert_file) {
  ssl_ctx_ = SSL_CTX_new(TLS_client_method());
  if (!ssl_ctx_) {
    std::cerr << "SSL_CTX_new: " << ERR_error_string(ERR_get_error(), nullptr)
              << std::endl;
    return -1;
  }

  if (ngtcp2_crypto_boringssl_configure_client_context(ssl_ctx_) != 0) {
    std::cerr << "ngtcp2_crypto_boringssl_configure_client_context failed"
              << std::endl;
    return -1;
  }

  SSL_CTX_set_default_verify_paths(ssl_ctx_);

  if (SSL_CTX_set1_groups_list(ssl_ctx_, config.groups) != 1) {
    std::cerr << "SSL_CTX_set1_groups_list failed" << std::endl;
    return -1;
  }

  if (private_key_file && cert_file) {
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, private_key_file,
                                    SSL_FILETYPE_PEM) != 1) {
      std::cerr << "SSL_CTX_use_PrivateKey_file: "
                << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
      return -1;
    }

    if (SSL_CTX_use_certificate_chain_file(ssl_ctx_, cert_file) != 1) {
      std::cerr << "SSL_CTX_use_certificate_chain_file: "
                << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
      return -1;
    }
  }

  if (config.session_file) {
    SSL_CTX_set_session_cache_mode(ssl_ctx_, SSL_SESS_CACHE_CLIENT |
                                               SSL_SESS_CACHE_NO_INTERNAL);
    SSL_CTX_sess_set_new_cb(ssl_ctx_, new_session_cb);
  }

#ifdef HAVE_LIBBROTLI
  if (!SSL_CTX_add_cert_compression_alg(
        ssl_ctx_, ngtcp2::tls::CERTIFICATE_COMPRESSION_ALGO_BROTLI,
        ngtcp2::tls::cert_compress, ngtcp2::tls::cert_decompress)) {
    std::cerr << "SSL_CTX_add_cert_compression_alg failed" << std::endl;
    return -1;
  }
#endif // defined(HAVE_LIBBROTLI)

  return 0;
}

extern std::ofstream keylog_file;

namespace {
void keylog_callback(const SSL *ssl, const char *line) {
  keylog_file.write(line, static_cast<std::streamsize>(strlen(line)));
  keylog_file.put('\n');
  keylog_file.flush();
}
} // namespace

void TLSClientContext::enable_keylog() {
  SSL_CTX_set_keylog_callback(ssl_ctx_, keylog_callback);
}
