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
#include "tls_client_session_boringssl.h"

#include <cassert>
#include <iostream>
#include <fstream>

#include "tls_client_context_boringssl.h"
#include "client_base.h"
#include "template.h"
#include "util.h"

TLSClientSession::TLSClientSession() {}

TLSClientSession::~TLSClientSession() {}

extern Config config;

int TLSClientSession::init(bool &early_data_enabled,
                           const TLSClientContext &tls_ctx,
                           const char *remote_addr, ClientBase *client,
                           uint32_t quic_version, AppProtocol app_proto) {
  early_data_enabled = false;

  auto ssl_ctx = tls_ctx.get_native_handle();

  ssl_ = SSL_new(ssl_ctx);
  if (!ssl_) {
    std::cerr << "SSL_new: " << ERR_error_string(ERR_get_error(), nullptr)
              << std::endl;
    return -1;
  }

  SSL_set_app_data(ssl_, client->conn_ref());
  SSL_set_connect_state(ssl_);

  switch (app_proto) {
  case AppProtocol::H3:
    SSL_set_alpn_protos(ssl_, H3_ALPN.data(), H3_ALPN.size());
    break;
  case AppProtocol::HQ:
    SSL_set_alpn_protos(ssl_, HQ_ALPN.data(), HQ_ALPN.size());
    break;
  }

  if (!config.sni.empty()) {
    SSL_set_tlsext_host_name(ssl_, config.sni.data());
  } else if (util::numeric_host(remote_addr)) {
    // If remote host is numeric address, just send "localhost" as SNI
    // for now.
    SSL_set_tlsext_host_name(ssl_, "localhost");
  } else {
    SSL_set_tlsext_host_name(ssl_, remote_addr);
  }

  if (config.session_file) {
    auto f = BIO_new_file(config.session_file, "r");
    if (f == nullptr) {
      std::cerr << "Could not read TLS session file " << config.session_file
                << std::endl;
    } else {
      auto session = PEM_read_bio_SSL_SESSION(f, nullptr, 0, nullptr);
      BIO_free(f);
      if (session == nullptr) {
        std::cerr << "Could not read TLS session file " << config.session_file
                  << std::endl;
      } else {
        if (!SSL_set_session(ssl_, session)) {
          std::cerr << "Could not set session" << std::endl;
        } else if (!config.disable_early_data &&
                   SSL_SESSION_early_data_capable(session)) {
          early_data_enabled = true;
          SSL_set_early_data_enabled(ssl_, 1);
        }
        SSL_SESSION_free(session);
      }
    }
  }

  if (!config.ech_config_list.empty() &&
      SSL_set1_ech_config_list(ssl_, config.ech_config_list.data(),
                               config.ech_config_list.size()) != 1) {
    std::cerr << "Could not set ECHConfigList: "
              << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
    return -1;
  }

  return 0;
}

bool TLSClientSession::get_early_data_accepted() const {
  return SSL_early_data_accepted(ssl_);
}

bool TLSClientSession::get_ech_accepted() const {
  return SSL_ech_accepted(ssl_);
}

int TLSClientSession::write_ech_config_list(const char *path) const {
  const uint8_t *retry_configs;
  size_t retry_configslen;

  SSL_get0_ech_retry_configs(ssl_, &retry_configs, &retry_configslen);
  if (retry_configslen == 0) {
    std::cerr << "No ECH retry configs found" << std::endl;
    return -1;
  }

  auto f = std::ofstream(path);

  if (!f) {
    return -1;
  }

  f.write(reinterpret_cast<const char *>(retry_configs),
          static_cast<std::streamsize>(retry_configslen));
  f.close();

  if (!f) {
    return -1;
  }

  return 0;
}
