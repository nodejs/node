/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/ssl.h>
#include <openssl/ech.h>
#include "internal/ssl_unwrap.h"
#include "../ssl_local.h"

int SSL_CTX_set1_echstore(SSL_CTX *ctx, OSSL_ECHSTORE *es)
{
    if (ctx == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    OSSL_ECHSTORE_free(ctx->ext.ech.es);
    ctx->ext.ech.es = NULL;
    if (es == NULL)
        return 1;
    if ((ctx->ext.ech.es = ossl_echstore_dup(es)) == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    return 1;
}

int SSL_set1_echstore(SSL *ssl, OSSL_ECHSTORE *es)
{
    SSL_CONNECTION *s;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL)
        return 0;
    OSSL_ECHSTORE_free(s->ext.ech.es);
    s->ext.ech.es = NULL;
    if (es == NULL)
        return 1;
    if ((s->ext.ech.es = ossl_echstore_dup(es)) == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    /*
     * Here, and below, if the application calls an API that implies it
     * wants to try ECH, then we set attempted to 1
     */
    s->ext.ech.attempted = 1;
    return 1;
}

OSSL_ECHSTORE *SSL_CTX_get1_echstore(const SSL_CTX *ctx)
{
    OSSL_ECHSTORE *dup = NULL;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }
    if (ctx->ext.ech.es == NULL)
        return NULL;
    if ((dup = ossl_echstore_dup(ctx->ext.ech.es)) == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return NULL;
    }
    return dup;
}

OSSL_ECHSTORE *SSL_get1_echstore(const SSL *ssl)
{
    SSL_CONNECTION *s;
    OSSL_ECHSTORE *dup = NULL;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }
    if (s->ext.ech.es == NULL)
        return NULL;
    if ((dup = ossl_echstore_dup(s->ext.ech.es)) == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return NULL;
    }
    return dup;
}

int SSL_ech_set1_server_names(SSL *ssl, const char *inner_name,
    const char *outer_name, int no_outer)
{
    SSL_CONNECTION *s;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL)
        return 0;
    OPENSSL_free(s->ext.hostname);
    s->ext.hostname = NULL;
    if (inner_name != NULL) {
        s->ext.hostname = OPENSSL_strdup(inner_name);
        if (s->ext.hostname == NULL)
            return 0;
    }
    OPENSSL_free(s->ext.ech.outer_hostname);
    s->ext.ech.outer_hostname = NULL;
    if (no_outer == 0 && outer_name != NULL && strlen(outer_name) > 0) {
        s->ext.ech.outer_hostname = OPENSSL_strdup(outer_name);
        if (s->ext.ech.outer_hostname == NULL)
            return 0;
    }
    s->ext.ech.no_outer = no_outer;
    s->ext.ech.attempted = 1;
    return 1;
}

int SSL_ech_set1_outer_server_name(SSL *ssl, const char *outer_name,
    int no_outer)
{
    SSL_CONNECTION *s;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL)
        return 0;
    OPENSSL_free(s->ext.ech.outer_hostname);
    s->ext.ech.outer_hostname = NULL;
    if (no_outer == 0 && outer_name != NULL && strlen(outer_name) > 0) {
        s->ext.ech.outer_hostname = OPENSSL_strdup(outer_name);
        if (s->ext.ech.outer_hostname == NULL)
            return 0;
    }
    s->ext.ech.no_outer = no_outer;
    s->ext.ech.attempted = 1;
    return 1;
}

/*
 * Note that this function returns 1 for success and 0 for error. This
 * contrasts with SSL_set1_alpn_protos() which (unusually for OpenSSL)
 * returns 0 for success and 1 on error.
 */
int SSL_ech_set1_outer_alpn_protos(SSL *ssl, const unsigned char *protos,
    const size_t protos_len)
{
    SSL_CONNECTION *s;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL)
        return 0;
    OPENSSL_free(s->ext.ech.alpn_outer);
    s->ext.ech.alpn_outer = NULL;
    if (protos == NULL)
        return 1;
    if (protos_len == 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    s->ext.ech.alpn_outer = OPENSSL_memdup(protos, protos_len);
    if (s->ext.ech.alpn_outer == NULL)
        return 0;
    s->ext.ech.alpn_outer_len = protos_len;
    s->ext.ech.attempted = 1;
    return 1;
}

int SSL_ech_get1_status(SSL *ssl, char **inner_sni, char **outer_sni)
{
    char *sinner = NULL;
    char *souter = NULL;
    SSL_CONNECTION *s = SSL_CONNECTION_FROM_SSL(ssl);

    if (s == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return SSL_ECH_STATUS_FAILED;
    }
    if (outer_sni == NULL || inner_sni == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return SSL_ECH_STATUS_FAILED;
    }
    *outer_sni = NULL;
    *inner_sni = NULL;
    if (s->ext.ech.grease == OSSL_ECH_IS_GREASE) {
        if (s->ext.ech.returned != NULL)
            return SSL_ECH_STATUS_GREASE_ECH;
        return SSL_ECH_STATUS_GREASE;
    }
    if ((s->options & SSL_OP_ECH_GREASE) != 0 && s->ext.ech.attempted != 1)
        return SSL_ECH_STATUS_GREASE;
    if (s->ext.ech.backend == 1) {
        if (s->ext.hostname != NULL
            && (*inner_sni = OPENSSL_strdup(s->ext.hostname)) == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return SSL_ECH_STATUS_FAILED;
        }
        return SSL_ECH_STATUS_BACKEND;
    }
    if (s->ext.ech.es == NULL)
        return SSL_ECH_STATUS_NOT_CONFIGURED;
    /* Set output vars - note we may be pointing to NULL which is fine */
    if (s->server == 0) {
        sinner = s->ext.hostname;
        if (s->ext.ech.attempted == 1 && s->ext.ech.success == 0)
            sinner = s->ext.ech.former_inner;
        if (s->ext.ech.no_outer == 0)
            souter = s->ext.ech.outer_hostname;
        else
            souter = NULL;
    } else {
        if (s->ext.ech.es != NULL && s->ext.ech.success == 1) {
            sinner = s->ext.hostname;
            souter = s->ext.ech.outer_hostname;
        }
    }
    if (s->ext.ech.es != NULL && s->ext.ech.attempted == 1
        && s->ext.ech.attempted_type == TLSEXT_TYPE_ech
        && s->ext.ech.grease != OSSL_ECH_IS_GREASE) {
        long vr = X509_V_OK;

        vr = SSL_get_verify_result(ssl);
        if (sinner != NULL
            && (*inner_sni = OPENSSL_strdup(sinner)) == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return SSL_ECH_STATUS_FAILED;
        }
        if (souter != NULL
            && (*outer_sni = OPENSSL_strdup(souter)) == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return SSL_ECH_STATUS_FAILED;
        }
        if (s->ext.ech.success == 1) {
            if (vr == X509_V_OK)
                return SSL_ECH_STATUS_SUCCESS;
            else
                return SSL_ECH_STATUS_BAD_NAME;
        } else {
            if (vr == X509_V_OK && s->ext.ech.returned != NULL)
                return SSL_ECH_STATUS_FAILED_ECH;
            else if (vr != X509_V_OK && s->ext.ech.returned != NULL)
                return SSL_ECH_STATUS_FAILED_ECH_BAD_NAME;
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return SSL_ECH_STATUS_FAILED;
        }
    }
    return SSL_ECH_STATUS_NOT_TRIED;
}

int SSL_ech_set1_grease_suite(SSL *ssl, const char *suite)
{
    SSL_CONNECTION *s;
    OSSL_HPKE_SUITE hpke_suite_in = OSSL_HPKE_SUITE_DEFAULT;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL)
        return 0;
    /* check suite makes sense */
    if (OSSL_HPKE_str2suite(suite, &hpke_suite_in) != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    OPENSSL_free(s->ext.ech.grease_suite);
    s->ext.ech.grease_suite = NULL;
    if (suite == NULL)
        return 1;
    s->ext.ech.grease_suite = OPENSSL_strdup(suite);
    if (s->ext.ech.grease_suite == NULL)
        return 0;
    s->ext.ech.grease = OSSL_ECH_IS_GREASE;
    return 1;
}

int SSL_ech_set_grease_type(SSL *ssl, uint16_t type)
{
    SSL_CONNECTION *s;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL)
        return 0;
    s->ext.ech.attempted_type = type;
    s->ext.ech.grease = OSSL_ECH_IS_GREASE;
    return 1;
}

void SSL_ech_set_callback(SSL *ssl, SSL_ech_cb_func f)
{
    SSL_CONNECTION *s;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL)
        return;
    s->ext.ech.cb = f;
    return;
}

int SSL_ech_get1_retry_config(SSL *ssl, unsigned char **ec, size_t *eclen)
{
    SSL_CONNECTION *s;
    OSSL_ECHSTORE *ve = NULL;
    BIO *in = NULL;
    int rv = 0;
    OSSL_LIB_CTX *libctx = NULL;
    const char *propq = NULL;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL || ec == NULL || eclen == NULL
        || s->ext.ech.returned_len > INT_MAX) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        goto err;
    }
    if (s->ext.ech.returned == NULL) {
        *ec = NULL;
        *eclen = 0;
        return 1;
    }
    /*
     * before returning retry-configs check we're in a good
     * state - either the session has worked, or else it
     * failed but ECH was the only failure (we only set the
     * retry_configs_ok flag when all else worked and we're
     * about to send the ECH required alert)
     */
    if (SSL_is_init_finished(ssl) != 1 && s->ext.ech.retry_configs_ok != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_OSSL_STORE_LIB);
        goto err;
    }
    /*
     * To not hand rubbish to application, we'll decode the value we have
     * so only syntactically good things are passed up. We won't insist
     * though that every entry in the retry_config list seems good - it
     * could be that e.g. one is a newer version than we support now,
     * and letting the application see that might cause someone to do an
     * upgrade.
     */
    if (s->ext.ech.es != NULL) {
        libctx = s->ext.ech.es->libctx;
        propq = s->ext.ech.es->propq;
    }
    if ((in = BIO_new(BIO_s_mem())) == NULL
        || BIO_write(in, s->ext.ech.returned, (int)s->ext.ech.returned_len) <= 0
        || (ve = OSSL_ECHSTORE_new(libctx, propq)) == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if (OSSL_ECHSTORE_read_echconfiglist(ve, in) != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* all good, copy and return */
    *ec = OPENSSL_memdup(s->ext.ech.returned, s->ext.ech.returned_len);
    if (*ec == NULL)
        goto err;
    *eclen = s->ext.ech.returned_len;
    rv = 1;
err:
    OSSL_ECHSTORE_free(ve);
    BIO_free_all(in);
    return rv;
}

/*
 * Note that this function returns 1 for success and 0 for error. This
 * contrasts with SSL_CTX_set1_alpn_protos() which (unusually for OpenSSL)
 * returns 0 for success and 1 on error.
 */
int SSL_CTX_ech_set1_outer_alpn_protos(SSL_CTX *ctx,
    const unsigned char *protos,
    const size_t protos_len)
{
    if (ctx == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    OPENSSL_free(ctx->ext.ech.alpn_outer);
    ctx->ext.ech.alpn_outer = NULL;
    if (protos == NULL)
        return 1;
    if (protos_len == 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    ctx->ext.ech.alpn_outer = OPENSSL_memdup(protos, protos_len);
    if (ctx->ext.ech.alpn_outer == NULL)
        return 0;
    ctx->ext.ech.alpn_outer_len = protos_len;
    return 1;
}

void SSL_CTX_ech_set_callback(SSL_CTX *ctx, SSL_ech_cb_func f)
{
    if (ctx == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return;
    }
    ctx->ext.ech.cb = f;
    return;
}

int SSL_set1_ech_config_list(SSL *ssl, const uint8_t *ecl, size_t ecl_len)
{
    int rv = 0;
    SSL_CONNECTION *s;
    OSSL_ECHSTORE *es = NULL;
    BIO *es_in = NULL;

    s = SSL_CONNECTION_FROM_SSL(ssl);
    if (s == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        goto err;
    }
    if (ecl == NULL) {
        OSSL_ECHSTORE_free(s->ext.ech.es);
        s->ext.ech.es = NULL;
        return 1;
    }
    if (ecl_len == 0 || ecl_len > INT_MAX) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        goto err;
    }
    if ((es_in = BIO_new_mem_buf(ecl, (int)ecl_len)) == NULL
        || (es = OSSL_ECHSTORE_new(NULL, NULL)) == NULL
        || OSSL_ECHSTORE_read_echconfiglist(es, es_in) != 1
        || SSL_set1_echstore(ssl, es) != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    rv = 1;
err:
    OSSL_ECHSTORE_free(es);
    BIO_free_all(es_in);
    return rv;
}
