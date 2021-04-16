/**
 *
 * \file tls.c
 * @brief getdns TLS functions
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

#include "config.h"

#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>

#include <openssl/opensslv.h>
#include <openssl/crypto.h>

#include "debug.h"
#include "context.h"
#include "const-info.h"

#ifdef USE_DANESSL
# include "ssl_dane/danessl.h"
#endif

#include "tls.h"

/* Double check configure has worked as expected. */
#if defined(USE_DANESSL) && \
	(defined(HAVE_SSL_DANE_ENABLE) || \
	 defined(HAVE_OPENSSL_INIT_CRYPTO) || \
	 defined(HAVE_SSL_CTX_DANE_ENABLE))
#error Configure error USE_DANESSL defined with OpenSSL 1.1 functions!
#endif

/* Cipher suites recommended in RFC7525. */
static char const * const _getdns_tls_context_default_cipher_list =
#ifndef HAVE_SSL_CTX_SET_CIPHERSUITES
    "TLS13-AES-256-GCM-SHA384:TLS13-AES-128-GCM-SHA256:"
    "TLS13-CHACHA20-POLY1305-SHA256:"
#endif
    "EECDH+AESGCM:EECDH+CHACHA20";

static char const * const _getdns_tls_context_default_cipher_suites =
	"TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256:"
	"TLS_CHACHA20_POLY1305_SHA256";

static char const * const _getdns_tls_connection_opportunistic_cipher_list =
	"DEFAULT";

static int _getdns_tls_verify_always_ok(int ok, X509_STORE_CTX *ctx)
{
# if defined(STUB_DEBUG) && STUB_DEBUG
	char	buf[8192];
	X509   *cert;
	int	 err;
	int	 depth;

	cert = X509_STORE_CTX_get_current_cert(ctx);
	err = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);

	if (cert)
		X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
	else
		strcpy(buf, "<unknown>");
	DEBUG_STUB("DEBUG Cert verify: depth=%d verify=%d err=%d subject=%s errorstr=%s\n", depth, ok, err, buf, X509_verify_cert_error_string(err));
# else /* defined(STUB_DEBUG) && STUB_DEBUG */
	(void)ok;
	(void)ctx;
# endif /* #else defined(STUB_DEBUG) && STUB_DEBUG */
	return 1;
}

static _getdns_tls_x509* _getdns_tls_x509_new(struct mem_funcs* mfs, X509* cert)
{
	_getdns_tls_x509* res;

	if (!cert)
		return NULL;

	res = GETDNS_MALLOC(*mfs, _getdns_tls_x509);
	if (res)
		res->ssl = cert;

	return res;
}

static const EVP_MD* get_digester(int algorithm)
{
	const EVP_MD* digester;

	switch (algorithm) {
#ifdef HAVE_EVP_MD5
	case GETDNS_HMAC_MD5   : digester = EVP_md5()   ; break;
#endif
#ifdef HAVE_EVP_SHA1
	case GETDNS_HMAC_SHA1  : digester = EVP_sha1()  ; break;
#endif
#ifdef HAVE_EVP_SHA224
	case GETDNS_HMAC_SHA224: digester = EVP_sha224(); break;
#endif
#ifdef HAVE_EVP_SHA256
	case GETDNS_HMAC_SHA256: digester = EVP_sha256(); break;
#endif
#ifdef HAVE_EVP_SHA384
	case GETDNS_HMAC_SHA384: digester = EVP_sha384(); break;
#endif
#ifdef HAVE_EVP_SHA512
	case GETDNS_HMAC_SHA512: digester = EVP_sha512(); break;
#endif
	default                : digester = NULL;
	}

	return digester;
}

#if HAVE_DECL_SSL_SET_MIN_PROTO_VERSION
static int _getdns_tls_version2openssl_version(getdns_tls_version_t v)
{
	switch (v) {
# ifdef SSL3_VERSION
	case GETDNS_SSL3  : return SSL3_VERSION;
# endif
# ifdef TLS1_VERSION
	case GETDNS_TLS1  : return TLS1_VERSION;
# endif
# ifdef TLS1_1_VERSION
	case GETDNS_TLS1_1: return TLS1_1_VERSION;
# endif
# ifdef TLS1_2_VERSION
	case GETDNS_TLS1_2: return TLS1_2_VERSION;
# endif
# ifdef TLS1_3_VERSION
	case GETDNS_TLS1_3: return TLS1_3_VERSION;
# endif
	default           :
# if   defined(TLS_MAX_VERSION)
			    return TLS_MAX_VERSION;
# elif defined(TLS1_3_VERSION)
			    return TLS1_3_VERSION;
# elif defined(TLS1_2_VERSION)
			    return TLS1_2_VERSION;
# elif defined(TLS1_1_VERSION)
			    return TLS1_1_VERSION;
# elif defined(TLS1_VERSION)
			    return TLS1_VERSION;
# elif defined(SSL3_VERSION)
			    return SSL3_VERSION;
# else
			    return -1;
# endif
	}
}
#endif

#ifdef USE_WINSOCK
/* For windows, the CA trust store is not read by openssl.
   Add code to open the trust store using wincrypt API and add
   the root certs into openssl trust store */
static int
add_WIN_cacerts_to_openssl_store(SSL_CTX* tls_ctx, const getdns_log_config* log)
{
	HCERTSTORE      hSystemStore;
	PCCERT_CONTEXT  pTargetCert = NULL;

	_getdns_log(log, GETDNS_LOG_SYS_STUB, GETDNS_LOG_DEBUG
	    , "%s: %s\n", STUB_DEBUG_SETUP_TLS
	    , "Adding Windows certificates from system root store to CA store")
	    ;

	/* load just once per context lifetime for this version of getdns
	   TODO: dynamically update CA trust changes as they are available */
	assert(tls_ctx);

	/* Call wincrypt's CertOpenStore to open the CA root store. */

	if ((hSystemStore = CertOpenStore(
		CERT_STORE_PROV_SYSTEM,
		0,
		0,
		/* NOTE: mingw does not have this const: replace with 1 << 16 from code
		   CERT_SYSTEM_STORE_CURRENT_USER, */
		1 << 16,
		L"root")) == 0)
	{
		_getdns_log(log, GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
		    , "%s: %s\n", STUB_DEBUG_SETUP_TLS
		    , "Could not CertOpenStore()");
		return 0;
	}

	X509_STORE* store = SSL_CTX_get_cert_store(tls_ctx);
	if (!store) {
		_getdns_log(log, GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
		    , "%s: %s\n", STUB_DEBUG_SETUP_TLS
		    , "Could not SSL_CTX_get_cert_store()");
		return 0;
	}

	/* failure if the CA store is empty or the call fails */
	if ((pTargetCert = CertEnumCertificatesInStore(
	    hSystemStore, pTargetCert)) == 0) {
		_getdns_log(log, GETDNS_LOG_SYS_STUB, GETDNS_LOG_NOTICE
		    , "%s: %s\n", STUB_DEBUG_SETUP_TLS
		    , "CA certificate store for Windows is empty.");
		return 0;
	}
	/* iterate over the windows cert store and add to openssl store */
	do
	{
		X509 *cert1 = d2i_X509(NULL,
			(const unsigned char **)&pTargetCert->pbCertEncoded,
			pTargetCert->cbCertEncoded);
		if (!cert1) {
			/* return error if a cert fails */
			_getdns_log(log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s %d:%s\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Unable to parse certificate in memory"
			    , ERR_get_error()
			    , ERR_error_string(ERR_get_error(), NULL));
			return 0;
		}
		else {
			/* return error if a cert add to store fails */
			if (X509_STORE_add_cert(store, cert1) == 0) {
				unsigned long error = ERR_peek_last_error();

				/* Ignore error X509_R_CERT_ALREADY_IN_HASH_TABLE which means the
				* certificate is already in the store.  */
				if(ERR_GET_LIB(error) != ERR_LIB_X509 ||
				   ERR_GET_REASON(error) != X509_R_CERT_ALREADY_IN_HASH_TABLE) {
					_getdns_log(log
					    , GETDNS_LOG_SYS_STUB
					    , GETDNS_LOG_ERR
					    , "%s: %s %d:%s\n"
					    , STUB_DEBUG_SETUP_TLS
					    , "Error adding certificate"
					    , ERR_get_error()
					    , ERR_error_string( ERR_get_error()
					                      , NULL)
					    );
					X509_free(cert1);
					return 0;
				}
			}
			X509_free(cert1);
		}
	} while ((pTargetCert = CertEnumCertificatesInStore(
		hSystemStore, pTargetCert)) != 0);

	/* Clean up memory and quit. */
	if (pTargetCert)
		CertFreeCertificateContext(pTargetCert);
	if (hSystemStore)
	{
		if (!CertCloseStore(hSystemStore, 0)) {
			_getdns_log(log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s\n", STUB_DEBUG_SETUP_TLS
			    , "Could not CertCloseStore()");
			return 0;
		}
	}
	_getdns_log(log, GETDNS_LOG_SYS_STUB, GETDNS_LOG_INFO
	    , "%s: %s\n", STUB_DEBUG_SETUP_TLS
	    , "Completed adding Windows certificates to CA store successfully")
	    ;
	return 1;
}
#endif

void _getdns_tls_init()
{
#ifdef HAVE_OPENSSL_INIT_CRYPTO
	OPENSSL_init_crypto( OPENSSL_INIT_ADD_ALL_CIPHERS
	                   | OPENSSL_INIT_ADD_ALL_DIGESTS
	                   | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
	(void)OPENSSL_init_ssl(0, NULL);
#else
	OpenSSL_add_all_algorithms();
	SSL_library_init();

# ifdef USE_DANESSL
		(void) DANESSL_library_init();
# endif
#endif
}

_getdns_tls_context* _getdns_tls_context_new(struct mem_funcs* mfs, const getdns_log_config* log)
{
	_getdns_tls_context* res;

	if (!(res = GETDNS_MALLOC(*mfs, struct _getdns_tls_context)))
		return NULL;

	res->log = log;

	/* Create client context, use TLS v1.2 only for now */
#  ifdef HAVE_TLS_CLIENT_METHOD
	res->ssl = SSL_CTX_new(TLS_client_method());
#  else
	res->ssl = SSL_CTX_new(TLSv1_2_client_method());
#  endif
	if(res->ssl == NULL) {
		char ssl_err[256];
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error creating TLS context"
			    , ssl_err);
		GETDNS_FREE(*mfs, res);
		return NULL;
	}
	return res;
}

getdns_return_t _getdns_tls_context_free(struct mem_funcs* mfs, _getdns_tls_context* ctx)
{
	if (!ctx || !ctx->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;
	SSL_CTX_free(ctx->ssl);
	GETDNS_FREE(*mfs, ctx);
	return GETDNS_RETURN_GOOD;
}

void _getdns_tls_context_pinset_init(_getdns_tls_context* ctx)
{
	int osr;

#if defined(HAVE_SSL_CTX_DANE_ENABLE)
	osr = SSL_CTX_dane_enable(ctx->ssl);
#elif defined(USE_DANESSL)
	osr = DANESSL_CTX_init(ctx->ssl);
#else
#error Must have either DANE SSL or OpenSSL v1.1.
#endif
	if (!osr) {
		char ssl_err[256];
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(ctx->log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_WARNING
			    , "%s: %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Could not enable DANE on TLX context"
			    , ssl_err);
	}
}

getdns_return_t _getdns_tls_context_set_min_max_tls_version(_getdns_tls_context* ctx, getdns_tls_version_t min, getdns_tls_version_t max)
{
#if HAVE_DECL_SSL_SET_MIN_PROTO_VERSION
	char ssl_err[256];
	int min_ssl = _getdns_tls_version2openssl_version(min);
	int max_ssl = _getdns_tls_version2openssl_version(max);

	if (!ctx || !ctx->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;
	if (min && !SSL_CTX_set_min_proto_version(ctx->ssl, min_ssl)) {
		struct const_info* ci = _getdns_get_const_info(min);
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(ctx->log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS context with "
			    "minimum TLS version"
			    , ci->name
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
	if (max && !SSL_CTX_set_max_proto_version(ctx->ssl, max_ssl)) {
		struct const_info* ci = _getdns_get_const_info(min);
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(ctx->log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS context with "
			    "minimum TLS version"
			    , ci->name
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
	return GETDNS_RETURN_GOOD;
#else
	/*
	 * We've used TLSv1_2_client_method() creating the context, so
	 * error if they asked for anything other than TLS 1.2 or better.
	 */
	(void) ctx;
	if ((!min || min == GETDNS_TLS1_2) && !max)
		return GETDNS_RETURN_GOOD;

	_getdns_log(ctx->log
		    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
		    , "%s: %s\n"
		    , STUB_DEBUG_SETUP_TLS
		    , "This version of OpenSSL does not "
		    "support setting of minimum or maximum "
		    "TLS versions");
	return GETDNS_RETURN_NOT_IMPLEMENTED;
#endif
}

const char* _getdns_tls_context_get_default_cipher_list()
{
	return _getdns_tls_context_default_cipher_list;
}

getdns_return_t _getdns_tls_context_set_cipher_list(_getdns_tls_context* ctx, const char* list)
{
	if (!ctx || !ctx->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!list)
		list = _getdns_tls_context_default_cipher_list;

	if (!SSL_CTX_set_cipher_list(ctx->ssl, list)) {
		char ssl_err[256];
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(ctx->log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS context with "
			    "cipher list"
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
	return GETDNS_RETURN_GOOD;
}

const char* _getdns_tls_context_get_default_cipher_suites()
{
	return _getdns_tls_context_default_cipher_suites;
}

getdns_return_t _getdns_tls_context_set_cipher_suites(_getdns_tls_context* ctx, const char* list)
{
	if (!ctx || !ctx->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;

#ifdef HAVE_SSL_CTX_SET_CIPHERSUITES
	if (!list)
		list = _getdns_tls_context_default_cipher_suites;

	if (!SSL_CTX_set_ciphersuites(ctx->ssl, list)) {
		char ssl_err[256];
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(ctx->log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS context with "
			    "cipher suites"
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
#else
	if (list) {
		_getdns_log(ctx->log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "This version of OpenSSL does not "
			    "support configuring cipher suites");
		return GETDNS_RETURN_NOT_IMPLEMENTED;
	}
#endif
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_context_set_curves_list(_getdns_tls_context* ctx, const char* list)
{
	if (!ctx || !ctx->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;
#if HAVE_TLS_CTX_CURVES_LIST
	if (list &&
	    !SSL_CTX_set1_curves_list(ctx->ssl, list)) {
		char ssl_err[256];
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(ctx->log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS context with "
			    "curves list"
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
#else
	if (list) {
		_getdns_log(ctx->log
			    , GETDNS_LOG_SYS_STUB, GETDNS_LOG_ERR
			    , "%s: %s\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "This version of OpenSSL does not "
			    "support configuring curves list");
		return GETDNS_RETURN_NOT_IMPLEMENTED;
	}
#endif
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_context_set_ca(_getdns_tls_context* ctx, const char* file, const char* path)
{
	if (!ctx || !ctx->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;
	if (file || path) {
		if (!SSL_CTX_load_verify_locations(ctx->ssl, file, path)) {
			char ssl_err[256];
			ERR_error_string_n( ERR_get_error()
					    , ssl_err
					    , sizeof(ssl_err));
			_getdns_log(ctx->log
				    , GETDNS_LOG_SYS_STUB
				    , GETDNS_LOG_WARNING
				    , "%s: %s (%s)\n"
				    , STUB_DEBUG_SETUP_TLS
				    , "Could not load verify locations"
				    , ssl_err);
		} else {
			_getdns_log(ctx->log
				    , GETDNS_LOG_SYS_STUB
				    , GETDNS_LOG_DEBUG
				    , "%s: %s\n"
				    , STUB_DEBUG_SETUP_TLS
				    , "Verify locations loaded");
		}
		return GETDNS_RETURN_GOOD; /* pass */
	}
#ifndef USE_WINSOCK
	else if (SSL_CTX_set_default_verify_paths(ctx->ssl))
		return GETDNS_RETURN_GOOD;
	else {
		char ssl_err[256];
		ERR_error_string_n( ERR_get_error()
				    , ssl_err
				    , sizeof(ssl_err));
		_getdns_log(ctx->log
			    , GETDNS_LOG_SYS_STUB
			    , GETDNS_LOG_WARNING
			    , "%s: %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Could not load default verify locations"
			    , ssl_err);
	}
#else
	else if (add_WIN_cacerts_to_openssl_store(ctx->ssl, ctx->log))
		return GETDNS_RETURN_GOOD;
#endif /* USE_WINSOCK */
	return GETDNS_RETURN_GENERIC_ERROR;
}

_getdns_tls_connection* _getdns_tls_connection_new(struct mem_funcs* mfs, _getdns_tls_context* ctx, int fd, const getdns_log_config* log)
{
	_getdns_tls_connection* res;

	if (!ctx || !ctx->ssl)
		return NULL;

	if (!(res = GETDNS_MALLOC(*mfs, struct _getdns_tls_connection)))
		return NULL;

	res->ssl = SSL_new(ctx->ssl);
	if (!res->ssl) {
		GETDNS_FREE(*mfs, res);
		return NULL;
	}

	if (!SSL_set_fd(res->ssl, fd)) {
		SSL_free(res->ssl);
		GETDNS_FREE(*mfs, res);
		return NULL;
	}

	res->log = log;

	/* Connection is a client. */
	SSL_set_connect_state(res->ssl);

	/* If non-application data received, retry read. */
	SSL_set_mode(res->ssl, SSL_MODE_AUTO_RETRY);
	return res;
}

getdns_return_t _getdns_tls_connection_free(struct mem_funcs* mfs, _getdns_tls_connection* conn)
{
	if (!conn || !conn->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;
	SSL_free(conn->ssl);
	GETDNS_FREE(*mfs, conn);
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_connection_shutdown(_getdns_tls_connection* conn)
{
	if (!conn || !conn->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;

#ifdef USE_DANESSL
	DANESSL_cleanup(conn->ssl);
#endif

	switch (SSL_shutdown(conn->ssl)) {
	case 0:		return GETDNS_RETURN_CONTEXT_UPDATE_FAIL;
	case 1:		return GETDNS_RETURN_GOOD;
	default:	return GETDNS_RETURN_GENERIC_ERROR;
	}
}

getdns_return_t _getdns_tls_connection_set_min_max_tls_version(_getdns_tls_connection* conn, getdns_tls_version_t min, getdns_tls_version_t max)
{
#if HAVE_DECL_SSL_SET_MIN_PROTO_VERSION
	char ssl_err[256];
	int min_ssl = _getdns_tls_version2openssl_version(min);
	int max_ssl = _getdns_tls_version2openssl_version(max);

	if (!conn || !conn->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;
	if (min && !SSL_set_min_proto_version(conn->ssl, min_ssl)) {
		struct const_info* ci = _getdns_get_const_info(min);
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(conn->log
			    , GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR
			    , "%s: %s %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS connection with "
			    "minimum TLS version"
			    , ci->name
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
	if (max && !SSL_set_max_proto_version(conn->ssl, max_ssl)) {
		struct const_info* ci = _getdns_get_const_info(min);
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(conn->log
			    , GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR
			    , "%s: %s %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS connection with "
			    "minimum TLS version"
			    , ci->name
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
	return GETDNS_RETURN_GOOD;
#else
	/*
	 * We've used TLSv1_2_client_method() creating the context, so
	 * error if they asked for anything other than TLS 1.2 or better.
	 */
	(void) conn;
	if ((!min || min == GETDNS_TLS1_2) && !max)
		return GETDNS_RETURN_GOOD;

	_getdns_log(conn->log
		    , GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR
		    , "%s: %s\n"
		    , STUB_DEBUG_SETUP_TLS
		    , "This version of OpenSSL does not "
		    "support setting of minimum or maximum "
		    "TLS versions");
	return GETDNS_RETURN_NOT_IMPLEMENTED;
#endif
}

getdns_return_t _getdns_tls_connection_set_cipher_list(_getdns_tls_connection* conn, const char* list)
{
	if (!conn || !conn->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!list)
		list = _getdns_tls_connection_opportunistic_cipher_list;

	if (!SSL_set_cipher_list(conn->ssl, list)) {

		char ssl_err[256];
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(conn->log
			    , GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR
			    , "%s: %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS connection with "
			    "cipher list"
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_connection_set_cipher_suites(_getdns_tls_connection* conn, const char* list)
{
	if (!conn || !conn->ssl || !list)
		return GETDNS_RETURN_INVALID_PARAMETER;

#ifdef HAVE_SSL_SET_CIPHERSUITES
	if (!SSL_set_ciphersuites(conn->ssl, list)) {
		char ssl_err[256];
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(conn->log
			    , GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR
			    , "%s: %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS connection with "
			    "cipher suites"
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
#else
	if (list) {
		_getdns_log(conn->log
			    , GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR
			    , "%s: %s\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "This version of OpenSSL does not "
			    "support configuring cipher suites");
		return GETDNS_RETURN_NOT_IMPLEMENTED;
	}
#endif
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_connection_set_curves_list(_getdns_tls_connection* conn, const char* list)
{
	if (!conn || !conn->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;
#if HAVE_TLS_CONN_CURVES_LIST
	if (list &&
	    !SSL_set1_curves_list(conn->ssl, list)) {
		char ssl_err[256];
		ERR_error_string_n( ERR_get_error()
				    , ssl_err, sizeof(ssl_err));
		_getdns_log(conn->log
			    , GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR
			    , "%s: %s (%s)\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "Error configuring TLS connection with "
			    "curves list"
			    , ssl_err);
		return GETDNS_RETURN_BAD_CONTEXT;
	}
#else
	if (list) {
		_getdns_log(conn->log
			    , GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR
			    , "%s: %s\n"
			    , STUB_DEBUG_SETUP_TLS
			    , "This version of OpenSSL does not "
			    "support configuring curves list");
		return GETDNS_RETURN_NOT_IMPLEMENTED;
	}
#endif
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_connection_set_session(_getdns_tls_connection* conn, _getdns_tls_session* s)
{
	if (!conn || !conn->ssl || !s || !s->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;
	if (!SSL_set_session(conn->ssl, s->ssl))
		return GETDNS_RETURN_GENERIC_ERROR;
	return GETDNS_RETURN_GOOD;
}

_getdns_tls_session* _getdns_tls_connection_get_session(struct mem_funcs* mfs, _getdns_tls_connection* conn)
{
	_getdns_tls_session* res;

	if (!conn || !conn->ssl)
		return NULL;

	if (!(res = GETDNS_MALLOC(*mfs, struct _getdns_tls_session)))
		return NULL;

	res->ssl = SSL_get1_session(conn->ssl);
	if (!res->ssl) {
		GETDNS_FREE(*mfs, res);
		return NULL;
	}

	return res;
}

const char* _getdns_tls_connection_get_version(_getdns_tls_connection* conn)
{
	if (!conn || !conn->ssl)
		return NULL;
	return SSL_get_version(conn->ssl);
}

getdns_return_t _getdns_tls_connection_do_handshake(_getdns_tls_connection* conn)
{
	int r;
	int err;

	if (!conn || !conn->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;

	ERR_clear_error();
	r = SSL_do_handshake(conn->ssl);
	if (r == 1)
		return GETDNS_RETURN_GOOD;
	err = SSL_get_error(conn->ssl, r);
	switch (err) {
	case SSL_ERROR_WANT_READ:
		return GETDNS_RETURN_TLS_WANT_READ;

	case SSL_ERROR_WANT_WRITE:
		return GETDNS_RETURN_TLS_WANT_WRITE;

	default:
		return GETDNS_RETURN_GENERIC_ERROR;
	}
}

_getdns_tls_x509* _getdns_tls_connection_get_peer_certificate(struct mem_funcs* mfs, _getdns_tls_connection* conn)
{
	if (!conn || !conn->ssl)
		return NULL;

	return _getdns_tls_x509_new(mfs, SSL_get_peer_certificate(conn->ssl));
}

getdns_return_t _getdns_tls_connection_is_session_reused(_getdns_tls_connection* conn)
{
	if (!conn || !conn->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (SSL_session_reused(conn->ssl))
		return GETDNS_RETURN_GOOD;
	else
		return GETDNS_RETURN_TLS_CONNECTION_FRESH;
}

getdns_return_t _getdns_tls_connection_setup_hostname_auth(_getdns_tls_connection* conn, const char* auth_name)
{
	if (!conn || !conn->ssl || !auth_name)
		return GETDNS_RETURN_INVALID_PARAMETER;

#if defined(HAVE_SSL_DANE_ENABLE)
	SSL_set_tlsext_host_name(conn->ssl, auth_name);
	/* Set up native OpenSSL hostname verification */
	X509_VERIFY_PARAM *param;
	param = SSL_get0_param(conn->ssl);
	X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
	X509_VERIFY_PARAM_set1_host(param, auth_name, 0);
#elif defined(USE_DANESSL)
	/* Stash auth name away for use in cert verification. */
	conn->auth_name = auth_name;
#else
#error Must have either DANE SSL or OpenSSL v1.1.
#endif
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_connection_set_host_pinset(_getdns_tls_connection* conn, const char* auth_name, const sha256_pin_t* pinset)
{
	if (!conn || !conn->ssl || !auth_name)
		return GETDNS_RETURN_INVALID_PARAMETER;

#if defined(USE_DANESSL)
	/* Stash auth name and pinset away for use in cert verification. */
	conn->auth_name = auth_name;
	conn->pinset = pinset;
#endif

#if defined(HAVE_SSL_DANE_ENABLE)
	int osr = SSL_dane_enable(conn->ssl, *auth_name ? auth_name : NULL);
	(void) osr;
	DEBUG_STUB("%s %-35s: DEBUG: SSL_dane_enable(\"%s\") -> %d\n"
	          , STUB_DEBUG_SETUP_TLS, __FUNC__, auth_name, osr);
	SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, _getdns_tls_verify_always_ok);
	const sha256_pin_t *pin_p;
	size_t n_pins = 0;
	for (pin_p = pinset; pin_p; pin_p = pin_p->next) {
		osr = SSL_dane_tlsa_add(conn->ssl, 2, 1, 1,
		    (unsigned char *)pin_p->pin, SHA256_DIGEST_LENGTH);
		DEBUG_STUB("%s %-35s: DEBUG: SSL_dane_tlsa_add() -> %d\n"
			  , STUB_DEBUG_SETUP_TLS, __FUNC__, osr);
		if (osr > 0)
			++n_pins;
		osr = SSL_dane_tlsa_add(conn->ssl, 3, 1, 1,
		    (unsigned char *)pin_p->pin, SHA256_DIGEST_LENGTH);
		DEBUG_STUB("%s %-35s: DEBUG: SSL_dane_tlsa_add() -> %d\n"
			  , STUB_DEBUG_SETUP_TLS, __FUNC__, osr);
		if (osr > 0)
			++n_pins;
	}
#elif defined(USE_DANESSL)
	if (pinset) {
		const char *auth_names[2] = { auth_name, NULL };
		int osr = DANESSL_init(conn->ssl,
				       *auth_name ? auth_name : NULL,
				       *auth_name ? auth_names : NULL);
		(void) osr;
		DEBUG_STUB("%s %-35s: DEBUG: DANESSL_init(\"%s\") -> %d\n"
			  , STUB_DEBUG_SETUP_TLS, __FUNC__, auth_name, osr);
		SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, _getdns_tls_verify_always_ok);
		const sha256_pin_t *pin_p;
		size_t n_pins = 0;
		for (pin_p = pinset; pin_p; pin_p = pin_p->next) {
			osr = DANESSL_add_tlsa(conn->ssl, 3, 1, "sha256",
			    (unsigned char *)pin_p->pin, SHA256_DIGEST_LENGTH);
			DEBUG_STUB("%s %-35s: DEBUG: DANESSL_add_tlsa() -> %d\n"
				  , STUB_DEBUG_SETUP_TLS, __FUNC__, osr);
			if (osr > 0)
				++n_pins;
			osr = DANESSL_add_tlsa(conn->ssl, 2, 1, "sha256",
			    (unsigned char *)pin_p->pin, SHA256_DIGEST_LENGTH);
			DEBUG_STUB("%s %-35s: DEBUG: DANESSL_add_tlsa() -> %d\n"
				  , STUB_DEBUG_SETUP_TLS, __FUNC__, osr);
			if (osr > 0)
				++n_pins;
		}
	} else {
		SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, _getdns_tls_verify_always_ok);
	}
#else
#error Must have either DANE SSL or OpenSSL v1.1.
#endif
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_connection_certificate_verify(_getdns_tls_connection* conn, long* errnum, const char** errmsg)
{
	if (!conn || !conn->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;

	long verify_result = SSL_get_verify_result(conn->ssl);

	/* Since we don't have DANE validation yet, DANE validation
	 * failures are always pinset validation failures */

	switch (verify_result) {
	case X509_V_OK:
#if defined(USE_DANESSL)
	{
		getdns_return_t res = GETDNS_RETURN_GOOD;
		X509* peer_cert = SSL_get_peer_certificate(conn->ssl);
		if (peer_cert) {
			if (conn->auth_name[0] &&
			    X509_check_host(peer_cert,
					    conn->auth_name,
					    strlen(conn->auth_name),
					    X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS,
					    NULL) <= 0) {
				if (errnum)
					*errnum = 1;
				if (errmsg)
					*errmsg = "Hostname mismatch";
				res = GETDNS_RETURN_GENERIC_ERROR;
			}
			X509_free(peer_cert);
		}
		return res;
	}
#else
		return GETDNS_RETURN_GOOD;
#endif

#if defined(HAVE_SSL_DANE_ENABLE)
	case X509_V_ERR_DANE_NO_MATCH:
		if (errnum)
			*errnum = 0;
		if (errmsg)
			*errmsg = "Pinset validation failure";
		return GETDNS_RETURN_GENERIC_ERROR;

#elif defined(USE_DANESSL)
	case X509_V_ERR_CERT_UNTRUSTED:
		if (conn->pinset &&
		    !DANESSL_get_match_cert(conn->ssl, NULL, NULL, NULL)) {
			if (errnum)
				*errnum = 0;
			if (errmsg)
				*errmsg = "Pinset validation failure";
			return GETDNS_RETURN_GENERIC_ERROR;
		}
		break;
#else
#error Must have either DANE SSL or OpenSSL v1.1.
#endif
	}

	/* General error if we get here. */
	if (errnum)
		*errnum = verify_result;
	if (errmsg)
		*errmsg = X509_verify_cert_error_string(verify_result);
	return GETDNS_RETURN_GENERIC_ERROR;
}


getdns_return_t _getdns_tls_connection_read(_getdns_tls_connection* conn, uint8_t* buf, size_t to_read, size_t* read)
{
	int sread;

	if (!conn || !conn->ssl || !read)
		return GETDNS_RETURN_INVALID_PARAMETER;

	ERR_clear_error();
	sread = SSL_read(conn->ssl, buf, to_read);
	if (sread <= 0) {
		switch (SSL_get_error(conn->ssl, sread)) {
		case SSL_ERROR_WANT_READ:
			return GETDNS_RETURN_TLS_WANT_READ;

		case SSL_ERROR_WANT_WRITE:
			return GETDNS_RETURN_TLS_WANT_WRITE;

		default:
			return GETDNS_RETURN_GENERIC_ERROR;
		}
	}

	*read = sread;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_connection_write(_getdns_tls_connection* conn, uint8_t* buf, size_t to_write, size_t* written)
{
	int swritten;

	if (!conn || !conn->ssl || !written)
		return GETDNS_RETURN_INVALID_PARAMETER;

	ERR_clear_error();
	swritten = SSL_write(conn->ssl, buf, to_write);
	if (swritten <= 0) {
		switch(SSL_get_error(conn->ssl, swritten)) {
		case SSL_ERROR_WANT_READ:
			/* SSL_write will not do partial writes, because
			 * SSL_MODE_ENABLE_PARTIAL_WRITE is not default,
			 * but the write could fail because of renegotiation.
			 * In that case SSL_get_error()  will return
			 * SSL_ERROR_WANT_READ or, SSL_ERROR_WANT_WRITE.
			 * Return for retry in such cases.
			 */
			return GETDNS_RETURN_TLS_WANT_READ;

		case SSL_ERROR_WANT_WRITE:
			return GETDNS_RETURN_TLS_WANT_WRITE;

		default:
			return GETDNS_RETURN_GENERIC_ERROR;
		}
	}

	*written = swritten;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_session_free(struct mem_funcs* mfs, _getdns_tls_session* s)
{
	if (!s || !s->ssl)
		return GETDNS_RETURN_INVALID_PARAMETER;
	SSL_SESSION_free(s->ssl);
	GETDNS_FREE(*mfs, s);
	return GETDNS_RETURN_GOOD;
}

getdns_return_t _getdns_tls_get_api_information(getdns_dict* dict)
{
	if (! getdns_dict_set_int(
	    dict, "openssl_build_version_number", OPENSSL_VERSION_NUMBER)

#ifdef HAVE_OPENSSL_VERSION_NUM
	    && ! getdns_dict_set_int(
	    dict, "openssl_version_number", OpenSSL_version_num())
#endif
#ifdef HAVE_OPENSSL_VERSION
	    && ! getdns_dict_util_set_string(
	    dict, "openssl_version_string", OpenSSL_version(OPENSSL_VERSION))

	    && ! getdns_dict_util_set_string(
	    dict, "openssl_cflags", OpenSSL_version(OPENSSL_CFLAGS))

	    && ! getdns_dict_util_set_string(
	    dict, "openssl_built_on", OpenSSL_version(OPENSSL_BUILT_ON))

	    && ! getdns_dict_util_set_string(
	    dict, "openssl_platform", OpenSSL_version(OPENSSL_PLATFORM))

	    && ! getdns_dict_util_set_string(
	    dict, "openssl_dir", OpenSSL_version(OPENSSL_DIR))

	    && ! getdns_dict_util_set_string(
		    dict, "openssl_engines_dir", OpenSSL_version(OPENSSL_ENGINES_DIR))
#endif
		)
		return GETDNS_RETURN_GOOD;
	return GETDNS_RETURN_GENERIC_ERROR;
}

void _getdns_tls_x509_free(struct mem_funcs* mfs, _getdns_tls_x509* cert)
{
	if (cert && cert->ssl)
		X509_free(cert->ssl);
	GETDNS_FREE(*mfs, cert);
}

int _getdns_tls_x509_to_der(struct mem_funcs* mfs, _getdns_tls_x509* cert, getdns_bindata* bindata)
{
	unsigned char* buf = NULL;
	int len;

	if (!cert || !cert->ssl )
		return 0;

	if (bindata == NULL)
		return i2d_X509(cert->ssl, NULL);

	len = i2d_X509(cert->ssl, &buf);
	if (len == 0 || (bindata->data = GETDNS_XMALLOC(*mfs, uint8_t, len)) == NULL) {
		bindata->size = 0;
		bindata->data = NULL;
	} else {
		bindata->size = len;
		(void) memcpy(bindata->data, buf, len);
		OPENSSL_free(buf);
	}

	return len;
}

unsigned char* _getdns_tls_hmac_hash(struct mem_funcs* mfs, int algorithm, const void* key, size_t key_size, const void* data, size_t data_size, size_t* output_size)
{
	const EVP_MD* digester = get_digester(algorithm);
	unsigned char* res;
	unsigned int md_len;

	if (!digester)
		return NULL;

	res = (unsigned char*) GETDNS_XMALLOC(*mfs, unsigned char, GETDNS_TLS_MAX_DIGEST_LENGTH);
	if (!res)
		return NULL;

	(void) HMAC(digester, key, key_size, data, data_size, res, &md_len);

	if (output_size)
		*output_size = md_len;
	return res;
}

_getdns_tls_hmac* _getdns_tls_hmac_new(struct mem_funcs* mfs, int algorithm, const void* key, size_t key_size)
{
	const EVP_MD *digester = get_digester(algorithm);
	_getdns_tls_hmac* res;

	if (!digester)
		return NULL;

	if (!(res = GETDNS_MALLOC(*mfs, struct _getdns_tls_hmac)))
		return NULL;

#ifdef HAVE_HMAC_CTX_NEW
	res->ctx = HMAC_CTX_new();
	if (!res->ctx) {
		GETDNS_FREE(*mfs, res);
		return NULL;
	}
#else
	res->ctx = &res->ctx_space;
	HMAC_CTX_init(res->ctx);
#endif
	if (!HMAC_Init_ex(res->ctx, key, key_size, digester, NULL)) {
#ifdef HAVE_HMAC_CTX_NEW
		HMAC_CTX_free(res->ctx);
#endif
		GETDNS_FREE(*mfs, res);
		return NULL;
	}

	return res;
}

getdns_return_t _getdns_tls_hmac_add(_getdns_tls_hmac* h, const void* data, size_t data_size)
{
	if (!h || !h->ctx || !data)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!HMAC_Update(h->ctx, data, data_size))
		return GETDNS_RETURN_GENERIC_ERROR;
	else
		return GETDNS_RETURN_GOOD;
}

unsigned char* _getdns_tls_hmac_end(struct mem_funcs* mfs, _getdns_tls_hmac* h, size_t* output_size)
{
	unsigned char* res;
	unsigned int md_len;

	res = (unsigned char*) GETDNS_XMALLOC(*mfs, unsigned char, GETDNS_TLS_MAX_DIGEST_LENGTH);
	if (!res)
		return NULL;

	(void) HMAC_Final(h->ctx, res, &md_len);

#ifdef HAVE_HMAC_CTX_NEW
	HMAC_CTX_free(h->ctx);
#endif
	GETDNS_FREE(*mfs, h);

	if (output_size)
		*output_size = md_len;
	return res;
}

void _getdns_tls_sha1(const void* data, size_t data_size, unsigned char* buf)
{
	SHA1(data, data_size, buf);
}

void _getdns_tls_cookie_sha256(uint32_t secret, void* addr, size_t addrlen, unsigned char* buf, size_t* buflen)
{
	const EVP_MD *md;
	EVP_MD_CTX *mdctx;
	unsigned int md_len;

	md = EVP_sha256();
	mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, md, NULL);
	EVP_DigestUpdate(mdctx, &secret, sizeof(secret));
	EVP_DigestUpdate(mdctx, addr, addrlen);
	EVP_DigestFinal_ex(mdctx, buf, &md_len);
	EVP_MD_CTX_destroy(mdctx);

	*buflen = md_len;
}

/* tls.c */
