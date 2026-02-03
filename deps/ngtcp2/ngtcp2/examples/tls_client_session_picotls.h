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
#ifndef TLS_CLIENT_SESSION_PICOTLS_H
#define TLS_CLIENT_SESSION_PICOTLS_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include "tls_session_base_picotls.h"
#include "shared.h"

using namespace ngtcp2;

class TLSClientContext;
class ClientBase;

class TLSClientSession : public TLSSessionBase {
public:
  TLSClientSession() = default;
  ~TLSClientSession();

  int init(bool &early_data_enabled, TLSClientContext &tls_ctx,
           const char *remote_addr, ClientBase *client, uint32_t quic_version,
           AppProtocol app_proto);

  bool get_early_data_accepted() const;
  bool get_ech_accepted() const { return false; }
  int write_ech_config_list(const char *path) const { return 0; }
};

#endif // !defined(TLS_CLIENT_SESSION_PICOTLS_H)
