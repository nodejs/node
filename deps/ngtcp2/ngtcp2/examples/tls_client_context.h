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
#ifndef TLS_CLIENT_CONTEXT_H
#define TLS_CLIENT_CONTEXT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#ifdef WITH_EXAMPLE_QUICTLS
#  include "tls_client_context_quictls.h"
#endif // defined(WITH_EXAMPLE_QUICTLS)

#ifdef WITH_EXAMPLE_GNUTLS
#  include "tls_client_context_gnutls.h"
#endif // defined(WITH_EXAMPLE_GNUTLS)

#ifdef WITH_EXAMPLE_BORINGSSL
#  include "tls_client_context_boringssl.h"
#endif // defined(WITH_EXAMPLE_BORINGSSL)

#ifdef WITH_EXAMPLE_PICOTLS
#  include "tls_client_context_picotls.h"
#endif // defined(WITH_EXAMPLE_PICOTLS)

#ifdef WITH_EXAMPLE_WOLFSSL
#  include "tls_client_context_wolfssl.h"
#endif // defined(WITH_EXAMPLE_WOLFSSL)

#ifdef WITH_EXAMPLE_OSSL
#  include "tls_client_context_ossl.h"
#endif // defined(WITH_EXAMPLE_OSSL)

#endif // !defined(TLS_CLIENT_CONTEXT_H)
