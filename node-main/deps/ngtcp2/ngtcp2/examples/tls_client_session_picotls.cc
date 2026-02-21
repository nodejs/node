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
#include "tls_client_session_picotls.h"

#include <cstring>
#include <iostream>
#include <memory>

#include <ngtcp2/ngtcp2_crypto_picotls.h>

#include <openssl/bio.h>
#include <openssl/pem.h>

#include <picotls.h>

#include "tls_client_context_picotls.h"
#include "client_base.h"
#include "template.h"
#include "util.h"

using namespace std::literals;

extern Config config;

TLSClientSession::TLSClientSession() {}

TLSClientSession::~TLSClientSession() {
  auto &hsprops = cptls_.handshake_properties;

  delete[] hsprops.client.session_ticket.base;
}

namespace {
auto negotiated_protocols_h3 = std::to_array<ptls_iovec_t>({
  {
    .base = const_cast<uint8_t *>(&H3_ALPN_V1[1]),
    .len = H3_ALPN_V1[0],
  },
});
} // namespace

namespace {
auto negotiated_protocols_hq = std::to_array<ptls_iovec_t>({
  {
    .base = const_cast<uint8_t *>(&HQ_ALPN_V1[1]),
    .len = HQ_ALPN_V1[0],
  },
});
} // namespace

int TLSClientSession::init(bool &early_data_enabled, TLSClientContext &tls_ctx,
                           const char *remote_addr, ClientBase *client,
                           uint32_t quic_version, AppProtocol app_proto) {
  cptls_.ptls = ptls_client_new(tls_ctx.get_native_handle());
  if (!cptls_.ptls) {
    std::cerr << "ptls_client_new failed" << std::endl;
    return -1;
  }

  *ptls_get_data_ptr(cptls_.ptls) = client->conn_ref();

  auto conn = client->conn();
  auto &hsprops = cptls_.handshake_properties;

  hsprops.additional_extensions = new ptls_raw_extension_t[2]{
    {
      .type = UINT16_MAX,
    },
    {
      .type = UINT16_MAX,
    },
  };

  if (ngtcp2_crypto_picotls_configure_client_session(&cptls_, conn) != 0) {
    std::cerr << "ngtcp2_crypto_picotls_configure_client_session failed"
              << std::endl;
    return -1;
  }

  switch (app_proto) {
  case AppProtocol::H3:
    hsprops.client.negotiated_protocols.list = negotiated_protocols_h3.data();
    hsprops.client.negotiated_protocols.count = negotiated_protocols_h3.size();

    break;
  case AppProtocol::HQ:
    hsprops.client.negotiated_protocols.list = negotiated_protocols_hq.data();
    hsprops.client.negotiated_protocols.count = negotiated_protocols_hq.size();

    break;
  }

  if (util::numeric_host(remote_addr)) {
    // If remote host is numeric address, just send "localhost" as SNI
    // for now.
    ptls_set_server_name(cptls_.ptls, "localhost", strlen("localhost"));
  } else {
    ptls_set_server_name(cptls_.ptls, remote_addr, strlen(remote_addr));
  }

  if (config.session_file) {
    auto f = BIO_new_file(config.session_file, "r");
    if (f == nullptr) {
      std::cerr << "Could not read TLS session file " << config.session_file
                << std::endl;
    } else {
      auto f_d = defer(BIO_free, f);

      char *name, *header;
      unsigned char *data;
      long datalen;

      if (PEM_read_bio(f, &name, &header, &data, &datalen) != 1) {
        std::cerr << "Could not read TLS session file " << config.session_file
                  << std::endl;
      } else {
        if ("PICOTLS SESSION PARAMETERS"sv != name) {
          std::cerr << "TLS session file contains unexpected name: " << name
                    << std::endl;
        } else {
          hsprops.client.session_ticket.base =
            new uint8_t[static_cast<size_t>(datalen)];
          hsprops.client.session_ticket.len = static_cast<size_t>(datalen);
          std::ranges::copy_n(data, datalen,
                              hsprops.client.session_ticket.base);

          if (!config.disable_early_data) {
            // No easy way to check max_early_data from ticket.  We
            // need to run ptls_handle_message.
            early_data_enabled = true;
          }
        }

        OPENSSL_free(name);
        OPENSSL_free(header);
        OPENSSL_free(data);
      }
    }
  }

  return 0;
}

bool TLSClientSession::get_early_data_accepted() const {
  return cptls_.handshake_properties.client.early_data_acceptance ==
         PTLS_EARLY_DATA_ACCEPTED;
}
