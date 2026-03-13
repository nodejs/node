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
#include "tls_server_context_ossl.h"

#include <cstring>
#include <cassert>
#include <iostream>
#include <fstream>
#include <limits>
#include <algorithm>

#include <ngtcp2/ngtcp2_crypto_ossl.h>

#include <openssl/err.h>

#include "server_base.h"
#include "template.h"

namespace {
auto _ = []() {
  if (ngtcp2_crypto_ossl_init() != 0) {
    assert(0);
    abort();
  }

  return 0;
}();
} // namespace

extern Config config;

TLSServerContext::TLSServerContext() : ssl_ctx_{nullptr} {}

TLSServerContext::~TLSServerContext() {
  if (ssl_ctx_) {
    SSL_CTX_free(ssl_ctx_);
  }
}

SSL_CTX *TLSServerContext::get_native_handle() const { return ssl_ctx_; }

namespace {
int alpn_select_proto_h3_cb(SSL *ssl, const unsigned char **out,
                            unsigned char *outlen, const unsigned char *in,
                            unsigned int inlen, void *arg) {
  auto conn_ref = static_cast<ngtcp2_crypto_conn_ref *>(SSL_get_app_data(ssl));
  auto h = static_cast<HandlerBase *>(conn_ref->user_data);
  // This should be the negotiated version, but we have not set the
  // negotiated version when this callback is called.
  auto version = ngtcp2_conn_get_client_chosen_version(h->conn());

  switch (version) {
  case NGTCP2_PROTO_VER_V1:
  case NGTCP2_PROTO_VER_V2:
    break;
  default:
    if (!config.quiet) {
      std::cerr << "Unexpected quic protocol version: " << std::hex << "0x"
                << version << std::dec << std::endl;
    }
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  for (auto s = std::span{in, inlen}; s.size() >= H3_ALPN_V1.size();
       s = s.subspan(s[0] + 1)) {
    if (std::ranges::equal(H3_ALPN_V1, s.first(H3_ALPN_V1.size()))) {
      *out = &s[1];
      *outlen = s[0];
      return SSL_TLSEXT_ERR_OK;
    }
  }

  if (!config.quiet) {
    std::cerr << "Client did not present ALPN " << &H3_ALPN_V1[1] << std::endl;
  }

  return SSL_TLSEXT_ERR_ALERT_FATAL;
}
} // namespace

namespace {
int alpn_select_proto_hq_cb(SSL *ssl, const unsigned char **out,
                            unsigned char *outlen, const unsigned char *in,
                            unsigned int inlen, void *arg) {
  auto conn_ref = static_cast<ngtcp2_crypto_conn_ref *>(SSL_get_app_data(ssl));
  auto h = static_cast<HandlerBase *>(conn_ref->user_data);
  // This should be the negotiated version, but we have not set the
  // negotiated version when this callback is called.
  auto version = ngtcp2_conn_get_client_chosen_version(h->conn());

  switch (version) {
  case NGTCP2_PROTO_VER_V1:
  case NGTCP2_PROTO_VER_V2:
    break;
  default:
    if (!config.quiet) {
      std::cerr << "Unexpected quic protocol version: " << std::hex << "0x"
                << version << std::dec << std::endl;
    }
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  for (auto s = std::span{in, inlen}; s.size() >= HQ_ALPN_V1.size();
       s = s.subspan(s[0] + 1)) {
    if (std::ranges::equal(HQ_ALPN_V1, s.first(HQ_ALPN_V1.size()))) {
      *out = &s[1];
      *outlen = s[0];
      return SSL_TLSEXT_ERR_OK;
    }
  }

  if (!config.quiet) {
    std::cerr << "Client did not present ALPN " << &HQ_ALPN_V1[1] << std::endl;
  }

  return SSL_TLSEXT_ERR_ALERT_FATAL;
}
} // namespace

namespace {
int verify_cb(int preverify_ok, X509_STORE_CTX *ctx) {
  // We don't verify the client certificate.  Just request it for the
  // testing purpose.
  return 1;
}
} // namespace

namespace {
int gen_ticket_cb(SSL *ssl, void *arg) {
  auto conn_ref = static_cast<ngtcp2_crypto_conn_ref *>(SSL_get_app_data(ssl));
  auto h = static_cast<HandlerBase *>(conn_ref->user_data);
  auto ver = htonl(ngtcp2_conn_get_negotiated_version(h->conn()));

  if (!SSL_SESSION_set1_ticket_appdata(SSL_get0_session(ssl), &ver,
                                       sizeof(ver))) {
    return 0;
  }

  return 1;
}
} // namespace

namespace {
SSL_TICKET_RETURN decrypt_ticket_cb(SSL *ssl, SSL_SESSION *session,
                                    const unsigned char *keyname,
                                    size_t keynamelen, SSL_TICKET_STATUS status,
                                    void *arg) {
  switch (status) {
  case SSL_TICKET_EMPTY:
  case SSL_TICKET_NO_DECRYPT:
    return SSL_TICKET_RETURN_IGNORE_RENEW;
  }

  uint8_t *pver;
  uint32_t ver;
  size_t verlen;

  if (!SSL_SESSION_get0_ticket_appdata(
        session, reinterpret_cast<void **>(&pver), &verlen) ||
      verlen != sizeof(ver)) {
    switch (status) {
    case SSL_TICKET_SUCCESS:
      return SSL_TICKET_RETURN_IGNORE;
    case SSL_TICKET_SUCCESS_RENEW:
    default:
      return SSL_TICKET_RETURN_IGNORE_RENEW;
    }
  }

  memcpy(&ver, pver, sizeof(ver));

  auto conn_ref = static_cast<ngtcp2_crypto_conn_ref *>(SSL_get_app_data(ssl));
  auto h = static_cast<HandlerBase *>(conn_ref->user_data);

  if (ngtcp2_conn_get_client_chosen_version(h->conn()) != ntohl(ver)) {
    switch (status) {
    case SSL_TICKET_SUCCESS:
      return SSL_TICKET_RETURN_IGNORE;
    case SSL_TICKET_SUCCESS_RENEW:
    default:
      return SSL_TICKET_RETURN_IGNORE_RENEW;
    }
  }

  switch (status) {
  case SSL_TICKET_SUCCESS:
    return SSL_TICKET_RETURN_USE;
  case SSL_TICKET_SUCCESS_RENEW:
  default:
    return SSL_TICKET_RETURN_USE_RENEW;
  }
}
} // namespace

int TLSServerContext::init(const char *private_key_file, const char *cert_file,
                           AppProtocol app_proto) {
  constexpr static unsigned char sid_ctx[] = "ngtcp2 server";

  ssl_ctx_ = SSL_CTX_new(TLS_server_method());
  if (!ssl_ctx_) {
    std::cerr << "SSL_CTX_new: " << ERR_error_string(ERR_get_error(), nullptr)
              << std::endl;
    return -1;
  }

  SSL_CTX_set_max_early_data(ssl_ctx_, UINT32_MAX);

  constexpr auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                            SSL_OP_SINGLE_ECDH_USE |
                            SSL_OP_CIPHER_SERVER_PREFERENCE |
                            SSL_OP_NO_ANTI_REPLAY;

  SSL_CTX_set_options(ssl_ctx_, ssl_opts);

  if (SSL_CTX_set_ciphersuites(ssl_ctx_, config.ciphers) != 1) {
    std::cerr << "SSL_CTX_set_ciphersuites: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return -1;
  }

  if (SSL_CTX_set1_groups_list(ssl_ctx_, config.groups) != 1) {
    std::cerr << "SSL_CTX_set1_groups_list failed" << std::endl;
    return -1;
  }

  SSL_CTX_set_mode(ssl_ctx_, SSL_MODE_RELEASE_BUFFERS);

  switch (app_proto) {
  case AppProtocol::H3:
    SSL_CTX_set_alpn_select_cb(ssl_ctx_, alpn_select_proto_h3_cb, nullptr);
    break;
  case AppProtocol::HQ:
    SSL_CTX_set_alpn_select_cb(ssl_ctx_, alpn_select_proto_hq_cb, nullptr);
    break;
  }

  SSL_CTX_set_default_verify_paths(ssl_ctx_);

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

  if (SSL_CTX_check_private_key(ssl_ctx_) != 1) {
    std::cerr << "SSL_CTX_check_private_key: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return -1;
  }

  SSL_CTX_set_session_id_context(ssl_ctx_, sid_ctx, sizeof(sid_ctx) - 1);

  if (config.verify_client) {
    SSL_CTX_set_verify(ssl_ctx_,
                       SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE |
                         SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       verify_cb);
  }

  SSL_CTX_set_session_ticket_cb(ssl_ctx_, gen_ticket_cb, decrypt_ticket_cb,
                                nullptr);

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

void TLSServerContext::enable_keylog() {
  SSL_CTX_set_keylog_callback(ssl_ctx_, keylog_callback);
}
