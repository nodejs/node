/**
 *  TLS 1.2 and 1.3 client-side functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_SSL_CLIENT_H
#define MBEDTLS_SSL_CLIENT_H

#include "common.h"

#if defined(MBEDTLS_SSL_TLS_C)
#include "ssl_misc.h"
#endif

#include <stddef.h>

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_write_client_hello(mbedtls_ssl_context *ssl);

#endif /* MBEDTLS_SSL_CLIENT_H */
