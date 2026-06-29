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
#ifndef TLS_SERVER_CONTEXT_PICOTLS_H
#define TLS_SERVER_CONTEXT_PICOTLS_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <picotls.h>
#include <picotls/openssl.h>

#include "shared.h"

using namespace ngtcp2;

class TLSServerContext {
public:
  TLSServerContext();
  ~TLSServerContext();

  int init(const char *private_key_file, const char *cert_file,
           AppProtocol app_proto);

  ptls_context_t *get_native_handle();

  void enable_keylog();

private:
  int load_private_key(const char *private_key_file);

  ptls_context_t ctx_;
  ptls_openssl_sign_certificate_t sign_cert_;
};

#endif // !defined(TLS_SERVER_CONTEXT_PICOTLS_H)
