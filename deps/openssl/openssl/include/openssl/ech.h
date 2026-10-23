/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Externally-visible data structures and prototypes for handling
 * shared-mode Encrypted ClientHello (ECH).
 */
#ifndef OPENSSL_ECH_H
#define OPENSSL_ECH_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/ssl.h>
#include <openssl/hpke.h>

#ifndef OPENSSL_NO_ECH

/*
 * Some externally visible limits - most used for sanity checks that could be
 * bigger if needed, but that work for now
 */
#define OSSL_ECH_MAX_PAYLOAD_LEN 1500 /* max ECH ciphertext to en/decode */
#define OSSL_ECH_MIN_ECHCONFIG_LEN 32 /* min for all encodings */
#define OSSL_ECH_MAX_ECHCONFIG_LEN 1500 /* max for all encodings */
#define OSSL_ECH_MAX_ECHCONFIGEXT_LEN 512 /* ECHConfig extension max */
#define OSSL_ECH_MAX_MAXNAMELEN 255 /* ECHConfig max for max name length */
#define OSSL_ECH_MAX_PUBLICNAME 255 /* max ECHConfig public name length */
#define OSSL_ECH_MAX_ALPNLEN 255 /* max alpn length */
#define OSSL_ECH_OUTERS_MAX 20 /* max extensions we compress via outer-exts */
#define OSSL_ECH_ALLEXTS_MAX 32 /* max total number of extension we allow */

/*
 * ECH version. We only support RFC 9849 as of now.  As/if new ECHConfig
 * versions are added, those will be noted here.
 */
#define OSSL_ECH_RFC9849_VERSION 0xfe0d /* official ECHConfig version */
/* latest version from an RFC */
#define OSSL_ECH_CURRENT_VERSION OSSL_ECH_RFC9849_VERSION

/* Return codes from SSL_ech_get1_status */
#define SSL_ECH_STATUS_BACKEND 4 /* ECH backend: saw an ech_is_inner */
#define SSL_ECH_STATUS_GREASE_ECH 3 /* GREASEd and got an ECH in return */
#define SSL_ECH_STATUS_GREASE 2 /* ECH GREASE happened  */
#define SSL_ECH_STATUS_SUCCESS 1 /* Success */
#define SSL_ECH_STATUS_FAILED 0 /* Some internal or protocol error */
#define SSL_ECH_STATUS_BAD_CALL -100 /* Some in/out arguments were NULL */
#define SSL_ECH_STATUS_NOT_TRIED -101 /* ECH wasn't attempted  */
#define SSL_ECH_STATUS_BAD_NAME -102 /* ECH ok but server cert bad */
#define SSL_ECH_STATUS_NOT_CONFIGURED -103 /* ECH wasn't configured */
#define SSL_ECH_STATUS_FAILED_ECH -105 /* Tried, failed, got an ECH, from a good name */
#define SSL_ECH_STATUS_FAILED_ECH_BAD_NAME -106 /* Tried, failed, got an ECH, from a bad name */

/* if a caller wants to index the last entry in the store */
#define OSSL_ECHSTORE_LAST -1
/* if a caller wants all entries in the store, e.g. to print public values */
#define OSSL_ECHSTORE_ALL -2

/* Values for the for_retry inputs */
#define OSSL_ECH_FOR_RETRY 1
#define OSSL_ECH_NO_RETRY 0

/*
 * API calls built around OSSL_ECHSTORE
 */
OSSL_ECHSTORE *OSSL_ECHSTORE_new(OSSL_LIB_CTX *libctx, const char *propq);
void OSSL_ECHSTORE_free(OSSL_ECHSTORE *es);
int OSSL_ECHSTORE_new_config(OSSL_ECHSTORE *es,
    uint16_t echversion, uint8_t max_name_length,
    const char *public_name, OSSL_HPKE_SUITE suite);
int OSSL_ECHSTORE_write_pem(OSSL_ECHSTORE *es, int index, BIO *out);
int OSSL_ECHSTORE_read_echconfiglist(OSSL_ECHSTORE *es, BIO *in);
int OSSL_ECHSTORE_get1_info(OSSL_ECHSTORE *es, int index, time_t *loaded_secs,
    char **public_name, char **echconfig,
    int *has_private, int *for_retry);
int OSSL_ECHSTORE_downselect(OSSL_ECHSTORE *es, int index);
int OSSL_ECHSTORE_set1_key_and_read_pem(OSSL_ECHSTORE *es, EVP_PKEY *priv,
    BIO *in, int for_retry);
int OSSL_ECHSTORE_read_pem(OSSL_ECHSTORE *es, BIO *in, int for_retry);
int OSSL_ECHSTORE_num_entries(const OSSL_ECHSTORE *es, int *numentries);
int OSSL_ECHSTORE_num_keys(OSSL_ECHSTORE *es, int *numkeys);
int OSSL_ECHSTORE_flush_keys(OSSL_ECHSTORE *es, time_t age);

/*
 * APIs relating OSSL_ECHSTORE to SSL/SSL_CTX
 */
int SSL_CTX_set1_echstore(SSL_CTX *ctx, OSSL_ECHSTORE *es);
int SSL_set1_echstore(SSL *s, OSSL_ECHSTORE *es);

OSSL_ECHSTORE *SSL_CTX_get1_echstore(const SSL_CTX *ctx);
OSSL_ECHSTORE *SSL_get1_echstore(const SSL *s);

int SSL_ech_set1_server_names(SSL *s, const char *inner_name,
    const char *outer_name, int no_outer);
int SSL_ech_set1_outer_server_name(SSL *s, const char *outer_name, int no_outer);
/*
 * Note that this function returns 1 for success and 0 for error. This
 * contrasts with SSL_set1_alpn_protos() which (unusually for OpenSSL)
 * returns 0 for success and 1 on error.
 */
int SSL_ech_set1_outer_alpn_protos(SSL *s, const unsigned char *protos,
    const size_t protos_len);

int SSL_ech_get1_status(SSL *s, char **inner_sni, char **outer_sni);
int SSL_ech_set1_grease_suite(SSL *s, const char *suite);
int SSL_ech_set_grease_type(SSL *s, uint16_t type);
typedef unsigned int (*SSL_ech_cb_func)(SSL *s, const char *str);
void SSL_ech_set_callback(SSL *s, SSL_ech_cb_func f);
int SSL_ech_get1_retry_config(SSL *s, unsigned char **ec, size_t *eclen);

/*
 * Note that this function returns 1 for success and 0 for error. This
 * contrasts with SSL_set1_alpn_protos() which (unusually for OpenSSL)
 * returns 0 for success and 1 on error.
 */
int SSL_CTX_ech_set1_outer_alpn_protos(SSL_CTX *s, const unsigned char *protos,
    const size_t protos_len);
void SSL_CTX_ech_set_callback(SSL_CTX *ctx, SSL_ech_cb_func f);
int SSL_set1_ech_config_list(SSL *ssl, const uint8_t *ecl, size_t ecl_len);

#ifdef __cplusplus
}
#endif

#endif
#endif
