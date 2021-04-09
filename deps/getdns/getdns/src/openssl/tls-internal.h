/**
 *
 * \file tls-internal.h
 * @brief getdns TLS implementation-specific items
 */

/*
 * Copyright (c) 2018-2019, NLnet Labs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the names of the copyright holders nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Verisign, Inc. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _GETDNS_TLS_INTERNAL_H
#define _GETDNS_TLS_INTERNAL_H

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "getdns/getdns.h"

#ifndef HAVE_DECL_SSL_CTX_SET1_CURVES_LIST
#define HAVE_TLS_CTX_CURVES_LIST	0
#else
#define HAVE_TLS_CTX_CURVES_LIST	(HAVE_DECL_SSL_CTX_SET1_CURVES_LIST)
#endif
#ifndef HAVE_DECL_SSL_SET1_CURVES_LIST
#define HAVE_TLS_CONN_CURVES_LIST	0
#else
#define HAVE_TLS_CONN_CURVES_LIST	(HAVE_DECL_SSL_SET1_CURVES_LIST)
#endif

#define GETDNS_TLS_MAX_DIGEST_LENGTH	(EVP_MAX_MD_SIZE)

/* Forward declare type. */
struct sha256_pin;
struct getdns_log_config;

typedef struct _getdns_tls_context {
	SSL_CTX* ssl;
	const struct getdns_log_config* log;
} _getdns_tls_context;

typedef struct _getdns_tls_connection {
	SSL* ssl;
	const struct getdns_log_config* log;
#if defined(USE_DANESSL)
	const char* auth_name;
	const struct sha256_pin* pinset;
#endif
} _getdns_tls_connection;

typedef struct _getdns_tls_session {
	SSL_SESSION* ssl;
} _getdns_tls_session;

typedef struct _getdns_tls_x509
{
	X509* ssl;
} _getdns_tls_x509;

typedef struct _getdns_tls_hmac
{
	HMAC_CTX *ctx;
#ifndef HAVE_HMAC_CTX_NEW
	HMAC_CTX ctx_space;
#endif
} _getdns_tls_hmac;

#endif /* _GETDNS_TLS_INTERNAL_H */
