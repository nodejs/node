/**
 *
 * \file tls.h
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

#ifndef _GETDNS_TLS_H
#define _GETDNS_TLS_H

#include "getdns/getdns.h"

#include "types-internal.h"

#include "tls-internal.h"

/* Forward declare type. */
struct sha256_pin;
struct getdns_log_config;

/* Additional return codes required by TLS abstraction. Internal use only. */
#define GETDNS_RETURN_TLS_WANT_READ		((getdns_return_t) 420)
#define GETDNS_RETURN_TLS_WANT_WRITE		((getdns_return_t) 421)
#define GETDNS_RETURN_TLS_CONNECTION_FRESH	((getdns_return_t) 422)

/**
 * Global initialisation of the TLS interface.
 */
void _getdns_tls_init();

/**
 * Create a new TLS context.
 *
 * @param mfs	pointer to getdns memory functions.
 * @paam log	pointer to context log config.
 * @return pointer to new context or NULL on error.
 */
_getdns_tls_context* _getdns_tls_context_new(struct mem_funcs* mfs, const struct getdns_log_config* log);

/**
 * Free a TLS context.
 *
 * @param mfs	point to getdns memory functions.
 * @param ctx	the context to free.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER if <code>ctx</code> is invalid.
 */
getdns_return_t _getdns_tls_context_free(struct mem_funcs* mfs, _getdns_tls_context* ctx);

/**
 * Initialise any shared state for pinset checking.
 *
 * @param ctx	the context to initialise.
 */
void _getdns_tls_context_pinset_init(_getdns_tls_context* ctx);

/**
 * Set minimum and maximum TLS versions.
 * If max or min are 0, that boundary is not set.
 *
 * @param ctx	the context.
 * @param min	the minimum TLS version.
 * @param max	the maximum TLS version.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad context pointer.
 * @return GETDNS_RETURN_NOT_IMPLEMENTED if not implemented.
 * @return GETDNS_RETURN_BAD_CONTEXT on failure.
 */
getdns_return_t _getdns_tls_context_set_min_max_tls_version(_getdns_tls_context* ctx, getdns_tls_version_t min, getdns_tls_version_t max);

/**
 * Get the default context cipher list.
 *
 * @return the default context cipher list.
 */
const char* _getdns_tls_context_get_default_cipher_list();

/**
 * Set list of allowed ciphers.
 *
 * @param ctx	the context.
 * @param list 	the list of cipher identifiers. NULL for default setting.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad context pointer.
 * @return GETDNS_RETURN_BAD_CONTEXT on failure.
 */
getdns_return_t _getdns_tls_context_set_cipher_list(_getdns_tls_context* ctx, const char* list);

/**
 * Get the default context cipher suites.
 *
 * @return the default context cipher suites.
 */
const char* _getdns_tls_context_get_default_cipher_suites();

/**
 * Set list of allowed cipher suites.
 *
 * @param ctx	the context.
 * @param list 	the list of cipher suites. NULL for default setting.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad context pointer.
 * @return GETDNS_RETURN_BAD_CONTEXT on failure.
 */
getdns_return_t _getdns_tls_context_set_cipher_suites(_getdns_tls_context* ctx, const char* list);

/**
 * Set list of allowed curves.
 *
 * @param ctx	the context.
 * @param list 	the list of curve identifiers. NULL for default setting.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad context pointer.
 * @return GETDNS_RETURN_BAD_CONTEXT on failure.
 */
getdns_return_t _getdns_tls_context_set_curves_list(_getdns_tls_context* ctx, const char* list);

/**
 * Set certificate authority details.
 *
 * Load CA from either a file or a directory. If both <code>file</code>
 * and <code>path</code> are <code>NULL</code>, use default locations.
 *
 * @param ctx	the context.
 * @param file	a file of CA certificates in PEM format.
 * @param path	a directory containing CA certificates in PEM format.
 * 		Files are looked up by CA subject name hash value.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad context pointer.
 * @return GETDNS_RETURN_GENERIC_ERROR on failure.
 */
getdns_return_t _getdns_tls_context_set_ca(_getdns_tls_context* ctx, const char* file, const char* path);

/**
 * Create a new TLS connection and associate it with a file descriptior.
 *
 * @param mfs	pointer to getdns memory functions.
 * @param ctx	the context.
 * @param fd	the file descriptor to associate with the connection.
 * @paam log	pointer to connection log config.
 * @return pointer to new connection or NULL on error.
 */
_getdns_tls_connection* _getdns_tls_connection_new(struct mem_funcs* mfs, _getdns_tls_context* ctx, int fd, const struct getdns_log_config* log);

/**
 * Free a TLS connection.
 *
 * @param mfs	pointer to getdns memory functions.
 * @param conn	the connection to free.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER if <code>conn</code> is invalid.
 */
getdns_return_t _getdns_tls_connection_free(struct mem_funcs* mfs, _getdns_tls_connection* conn);

/**
 * Shut down a TLS connection.
 *
 * @param conn	the connection to shut down.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER if <code>conn</code> is invalid.
 * @return GETDNS_RETURN_CONTEXT_UPDATE_FAIL if shutdown is not finished,
 * 	   and this routine should be called again.
 * @return GETDNS_RETURN_GENERIC_ERROR on error.
 */
getdns_return_t _getdns_tls_connection_shutdown(_getdns_tls_connection* conn);

/**
 * Set minimum and maximum TLS versions for this connection.
 * If max or min are 0, that boundary is not set.
 *
 * @param conn	the connection.
 * @param min	the minimum TLS version.
 * @param max	the maximum TLS version.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad context pointer.
 * @return GETDNS_RETURN_NOT_IMPLEMENTED if not implemented.
 * @return GETDNS_RETURN_BAD_CONTEXT on failure.
 */
getdns_return_t _getdns_tls_connection_set_min_max_tls_version(_getdns_tls_connection* conn, getdns_tls_version_t min, getdns_tls_version_t max);

/**
 * Set list of allowed ciphers on this connection.
 *
 * @param conn	the connection.
 * @param list 	the list of cipher identifiers. NULL for opportunistic setting.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad connection pointer.
 * @return GETDNS_RETURN_BAD_CONTEXT on failure.
 */
getdns_return_t _getdns_tls_connection_set_cipher_list(_getdns_tls_connection* conn, const char* list);

/**
 * Set list of allowed cipher suites on this connection.
 *
 * @param conn	the connection.
 * @param list 	the list of cipher suites. NULL for default setting.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad context pointer.
 * @return GETDNS_RETURN_BAD_CONTEXT on failure.
 */
getdns_return_t _getdns_tls_connection_set_cipher_suites(_getdns_tls_connection* conn, const char* list);

/**
 * Set list of allowed curves on this connection.
 *
 * @param conn	the connection.
 * @param list 	the list of curve identifiers. NULL for default setting.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad connection pointer.
 * @return GETDNS_RETURN_BAD_CONTEXT on failure.
 */
getdns_return_t _getdns_tls_connection_set_curves_list(_getdns_tls_connection* conn, const char* list);

/**
 * Set the session for this connection.
 *
 * @param conn	the connection.
 * @param s 	the session.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER on bad connection pointer.
 * @return GETDNS_RETURN_GENERIC_ERROR on failure.
 */
getdns_return_t _getdns_tls_connection_set_session(_getdns_tls_connection* conn, _getdns_tls_session* s);

/**
 * Get the session for this connection.
 *
 * @param mfs	pointer to getdns memory functions.
 * @param conn	the connection.
 * @return pointer to the session or NULL on error.
 */
_getdns_tls_session* _getdns_tls_connection_get_session(struct mem_funcs* mfs, _getdns_tls_connection* conn);

/**
 * Report the TLS version of the connection.
 *
 * @param conn	the connection.
 * @return string with the connection description, NULL on error.
 */
const char* _getdns_tls_connection_get_version(_getdns_tls_connection* conn);

/**
 * Attempt TLS handshake.
 *
 * @param conn	the connection.
 * @return GETDNS_RETURN_GOOD if handshake is complete.
 * @return GETDNS_RETURN_INVALID_PARAMETER if conn is null or has no SSL.
 * @return GETDNS_RETURN_TLS_WANT_READ if handshake needs to read to proceed.
 * @return GETDNS_RETURN_TLS_WANT_WRITE if handshake needs to write to proceed.
 * @return GETDNS_RETURN_GENERIC_ERROR if handshake failed.
 */
getdns_return_t _getdns_tls_connection_do_handshake(_getdns_tls_connection* conn);

/**
 * Get the connection peer certificate.
 *
 * @param mfs	pointer to getdns memory functions.
 * @param conn	the connection.
 * @return certificate or NULL on error.
 */
_getdns_tls_x509* _getdns_tls_connection_get_peer_certificate(struct mem_funcs* mfs, _getdns_tls_connection* conn);

/**
 * See whether the connection is reusing a session.
 *
 * @param conn	the connection.
 * @return GETDNS_RETURN_GOOD if connection is being reused.
 * @return GETDNS_RETURN_INVALID_PARAMETER if conn is null or has no SSL.
 * @return GETDNS_RETURN_TLS_CONNECTION_FRESH if connection is not being reused.
 */
getdns_return_t _getdns_tls_connection_is_session_reused(_getdns_tls_connection* conn);

/**
 * Set up host name verification.
 *
 * @param conn		the connection.
 * @param auth_name	the hostname.
 * @return GETDNS_RETURN_GOOD if all OK.
 * @return GETDNS_RETURN_INVALID_PARAMETER if conn is null or has no SSL.
 */
getdns_return_t _getdns_tls_connection_setup_hostname_auth(_getdns_tls_connection* conn, const char* auth_name);

/**
 * Set host pinset.
 *
 * @param conn		the connection.
 * @param auth_name	the hostname.
 * @return GETDNS_RETURN_GOOD if all OK.
 * @return GETDNS_RETURN_INVALID_PARAMETER if conn is null or has no SSL.
 */
getdns_return_t _getdns_tls_connection_set_host_pinset(_getdns_tls_connection* conn, const char* auth_name, const struct sha256_pin* pinset);

/**
 * Get result of certificate verification.
 *
 * @param conn		the connection.
 * @param errno		failure error number.
 * @param errmsg	failure error message.
 * @return GETDNS_RETURN_GOOD if all OK.
 * @return GETDNS_RETURN_INVALID_PARAMETER if conn is null or has no SSL.
 * @return GETDNS_RETURN_GENERIC_ERROR if verification failed.
 */
getdns_return_t _getdns_tls_connection_certificate_verify(_getdns_tls_connection* conn, long* errnum, const char** errmsg);

/**
 * Read from TLS.
 *
 * @param conn	  the connection.
 * @param buf	  the buffer to read to.
 * @param to_read the number of bytes to read.
 * @param read	  pointer to holder for the number of bytes read.
 * @return GETDNS_RETURN_GOOD if some bytes were read.
 * @return GETDNS_RETURN_INVALID_PARAMETER if conn is null or has no SSL.
 * @return GETDNS_RETURN_TLS_WANT_READ if the read needs to be retried.
 * @return GETDNS_RETURN_TLS_WANT_WRITE if handshake isn't finished.
 * @return GETDNS_RETURN_GENERIC_ERROR if read failed.
 */
getdns_return_t _getdns_tls_connection_read(_getdns_tls_connection* conn, uint8_t* buf, size_t to_read, size_t* read);

/**
 * Write to TLS.
 *
 * @param conn	   the connection.
 * @param buf	   the buffer to write from.
 * @param to_write the number of bytes to write.
 * @param written  the number of bytes written.
 * @return GETDNS_RETURN_GOOD if some bytes were read.
 * @return GETDNS_RETURN_INVALID_PARAMETER if conn is null or has no SSL.
 * @return GETDNS_RETURN_TLS_WANT_READ if handshake isn't finished.
 * @return GETDNS_RETURN_TLS_WANT_WRITE if the write needs to be retried.
 * @return GETDNS_RETURN_GENERIC_ERROR if write failed.
 */
getdns_return_t _getdns_tls_connection_write(_getdns_tls_connection* conn, uint8_t* buf, size_t to_write, size_t* written);

/**
 * Free a session.
 *
 * @param mfs	pointer to getdns memory functions.
 * @param s	the session.
 * @return GETDNS_RETURN_GOOD on success.
 * @return GETDNS_RETURN_INVALID_PARAMETER if s is null or has no SSL.
 */
getdns_return_t _getdns_tls_session_free(struct mem_funcs* mfs, _getdns_tls_session* s);

/**
 * Free X509 certificate.
 *
 * @param mfs	pointer to getdns memory functions.
 * @param cert	the certificate.
 */
void _getdns_tls_x509_free(struct mem_funcs* mfs, _getdns_tls_x509* cert);

/**
 * Convert X509 to DER.
 *
 * @param cert	the certificate.
 * @param buf	buffer to receive conversion.
 * @return length of conversion, 0 on error.
 */
int _getdns_tls_x509_to_der(struct mem_funcs* mfs, _getdns_tls_x509* cert, getdns_bindata* bindata);

/**
 * Fill in dictionary with TLS API information.
 *
 * @param dict	the dictionary to add to.
 * @return GETDNS_RETURN_GOOD if some bytes were read.
 * @return GETDNS_RETURN_GENERIC_ERROR if items cannot be set.
 */
getdns_return_t _getdns_tls_get_api_information(getdns_dict* dict);

/**
 * Return buffer with HMAC hash.
 *
 * @param mfs		pointer to getdns memory functions.
 * @param algorithm	hash algorithm to use (<code>GETDNS_HMAC_?</code>).
 * @param key		the key.
 * @param key_size	the key size.
 * @param data		the data to hash.
 * @param data_size	the data size.
 * @param output_size	the output size will be written here if not NULL.
 * @return output	malloc'd buffer with output, NULL on error.
 */
unsigned char* _getdns_tls_hmac_hash(struct mem_funcs* mfs, int algorithm, const void* key, size_t key_size, const void* data, size_t data_size, size_t* output_size);

/**
 * Return a new HMAC handle.
 *
 * @param mfs		pointer to getdns memory functions.
 * @param algorithm	hash algorithm to use (<code>GETDNS_HMAC_?</code>).
 * @param key		the key.
 * @param key_size	the key size.
 * @return HMAC handle or NULL on error.
 */
_getdns_tls_hmac* _getdns_tls_hmac_new(struct mem_funcs* mfs, int algorithm, const void* key, size_t key_size);

/**
 * Add data to a HMAC.
 *
 * @param h		the HMAC.
 * @param data		the data to add.
 * @param data_size	the size of data to add.
 * @return GETDNS_RETURN_GOOD if added.
 * @return GETDNS_RETURN_INVALID_PARAMETER if h is null or has no HMAC.
 * @return GETDNS_RETURN_GENERIC_ERROR on error.
 */
getdns_return_t _getdns_tls_hmac_add(_getdns_tls_hmac* h, const void* data, size_t data_size);

/**
 * Return the HMAC digest and free the handle.
 *
 * @param mfs		pointer to getdns memory functions.
 * @param h		the HMAC.
 * @param output_size	the output size will be written here if not NULL.
 * @return output	malloc'd buffer with output, NULL on error.
 */
unsigned char* _getdns_tls_hmac_end(struct mem_funcs* mfs, _getdns_tls_hmac* h, size_t* output_size);

/**
 * Calculate a SHA1 hash.
 *
 * @param data		the data to hash.
 * @param data_size	the size of the data to hash.
 * @param buf		the buffer to receive the hash. Must be at least
 * 			SHA_DIGEST_LENGTH bytes.
 */
void _getdns_tls_sha1(const void* data, size_t data_size, unsigned char* buf);

/**
 * Calculate SHA256 for cookie.
 *
 * @param secret	the secret.
 * @param addr		the address.
 * @param addrlen	the address length.
 * @param buf		buffer to receive hash.
 * @param buflen	receive the hash length.
 */
void _getdns_tls_cookie_sha256(uint32_t secret, void* addr, size_t addrlen, unsigned char* buf, size_t* buflen);

#endif /* _GETDNS_TLS_H */
