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
#include "tls_client_context_wolfssl.h"

#include <cstring>
#include <iostream>
#include <fstream>
#include <limits>

#include <ngtcp2/ngtcp2_crypto_wolfssl.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include "client_base.h"
#include "template.h"

extern Config config;

TLSClientContext::TLSClientContext() : ssl_ctx_{nullptr} {}

TLSClientContext::~TLSClientContext() {
  if (ssl_ctx_) {
    wolfSSL_CTX_free(ssl_ctx_);
  }
}

WOLFSSL_CTX *TLSClientContext::get_native_handle() const { return ssl_ctx_; }

namespace {
int new_session_cb(WOLFSSL *ssl, WOLFSSL_SESSION *session) {
  std::cerr << "new_session_cb called" << std::endl;

  auto conn_ref =
    static_cast<ngtcp2_crypto_conn_ref *>(wolfSSL_get_app_data(ssl));
  auto c = static_cast<ClientBase *>(conn_ref->user_data);

  c->ticket_received();

#ifdef HAVE_SESSION_TICKET
  if (wolfSSL_SESSION_get_max_early_data(session) !=
      std::numeric_limits<uint32_t>::max()) {
    std::cerr << "max_early_data_size is not 0xffffffff" << std::endl;
  }

  unsigned char sbuffer[16 * 1024], *data;
  auto sz = wolfSSL_i2d_SSL_SESSION(session, nullptr);
  if (sz <= 0) {
    std::cerr << "Could not export TLS session in " << config.session_file
              << std::endl;
    return 0;
  }
  if (static_cast<size_t>(sz) > sizeof(sbuffer)) {
    std::cerr << "Exported TLS session too large" << std::endl;
    return 0;
  }
  data = sbuffer;
  sz = wolfSSL_i2d_SSL_SESSION(session, &data);

  auto f = wolfSSL_BIO_new_file(config.session_file, "w");
  if (f == nullptr) {
    std::cerr << "Could not write TLS session in " << config.session_file
              << std::endl;
    return 0;
  }

  auto f_d = defer(wolfSSL_BIO_free, f);

  if (!wolfSSL_PEM_write_bio(f, "WOLFSSL SESSION PARAMETERS", "", sbuffer,
                             sz)) {
    std::cerr << "Unable to write TLS session to file" << std::endl;
    return 0;
  }
  std::cerr << "new_session_cb: wrote " << sz << " of session data"
            << std::endl;
#else  // !defined(HAVE_SESSION_TICKET)
  std::cerr << "TLS session tickets not enabled in wolfSSL " << std::endl;
#endif // !defined(HAVE_SESSION_TICKET)
  return 0;
}
} // namespace

int TLSClientContext::init(const char *private_key_file,
                           const char *cert_file) {
  ssl_ctx_ = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
  if (!ssl_ctx_) {
    std::cerr << "wolfSSL_CTX_new: "
              << wolfSSL_ERR_error_string(wolfSSL_ERR_get_error(), nullptr)
              << std::endl;
    return -1;
  }

  if (ngtcp2_crypto_wolfssl_configure_client_context(ssl_ctx_) != 0) {
    std::cerr << "ngtcp2_crypto_wolfssl_configure_client_context failed"
              << std::endl;
    return -1;
  }

  if (wolfSSL_CTX_set_default_verify_paths(ssl_ctx_) ==
      WOLFSSL_NOT_IMPLEMENTED) {
    /* hmm, not verifying the server cert for now */
    wolfSSL_CTX_set_verify(ssl_ctx_, WOLFSSL_VERIFY_NONE, 0);
  }

  if (wolfSSL_CTX_set_cipher_list(ssl_ctx_, config.ciphers) !=
      WOLFSSL_SUCCESS) {
    std::cerr << "wolfSSL_CTX_set_cipher_list: "
              << wolfSSL_ERR_error_string(wolfSSL_ERR_get_error(), nullptr)
              << std::endl;
    return -1;
  }

  if (wolfSSL_CTX_set1_groups_list(
        ssl_ctx_, const_cast<char *>(config.groups)) != WOLFSSL_SUCCESS) {
    std::cerr << "wolfSSL_CTX_set1_groups_list(" << config.groups << ") failed"
              << std::endl;
    return -1;
  }

  if (private_key_file && cert_file) {
    if (wolfSSL_CTX_use_PrivateKey_file(ssl_ctx_, private_key_file,
                                        SSL_FILETYPE_PEM) != WOLFSSL_SUCCESS) {
      std::cerr << "wolfSSL_CTX_use_PrivateKey_file: "
                << wolfSSL_ERR_error_string(wolfSSL_ERR_get_error(), nullptr)
                << std::endl;
      return -1;
    }

    if (wolfSSL_CTX_use_certificate_chain_file(ssl_ctx_, cert_file) !=
        WOLFSSL_SUCCESS) {
      std::cerr << "wolfSSL_CTX_use_certificate_chain_file: "
                << wolfSSL_ERR_error_string(wolfSSL_ERR_get_error(), nullptr)
                << std::endl;
      return -1;
    }
  }

  if (config.session_file) {
    wolfSSL_CTX_UseSessionTicket(ssl_ctx_);
    wolfSSL_CTX_sess_set_new_cb(ssl_ctx_, new_session_cb);
  }

  return 0;
}

extern std::ofstream keylog_file;

#ifdef HAVE_SECRET_CALLBACK
namespace {
void keylog_callback(const WOLFSSL *ssl, const char *line) {
  keylog_file.write(line, static_cast<std::streamsize>(strlen(line)));
  keylog_file.put('\n');
  keylog_file.flush();
}
} // namespace
#endif // defined(HAVE_SECRET_CALLBACK)

void TLSClientContext::enable_keylog() {
#ifdef HAVE_SECRET_CALLBACK
  wolfSSL_CTX_set_keylog_callback(ssl_ctx_, keylog_callback);
#endif // defined(HAVE_SECRET_CALLBACK)
}
