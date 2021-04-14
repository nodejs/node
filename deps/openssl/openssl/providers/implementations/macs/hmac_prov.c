/*
 * Copyright 2018-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * HMAC low level APIs are deprecated for public use, but still ok for internal
 * use.
 */
#include "internal/deprecated.h"

#include <string.h>

#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "prov/implementations.h"
#include "prov/provider_ctx.h"
#include "prov/provider_util.h"
#include "prov/providercommon.h"

/*
 * Forward declaration of everything implemented here.  This is not strictly
 * necessary for the compiler, but provides an assurance that the signatures
 * of the functions in the dispatch table are correct.
 */
static OSSL_FUNC_mac_newctx_fn hmac_new;
static OSSL_FUNC_mac_dupctx_fn hmac_dup;
static OSSL_FUNC_mac_freectx_fn hmac_free;
static OSSL_FUNC_mac_gettable_ctx_params_fn hmac_gettable_ctx_params;
static OSSL_FUNC_mac_get_ctx_params_fn hmac_get_ctx_params;
static OSSL_FUNC_mac_settable_ctx_params_fn hmac_settable_ctx_params;
static OSSL_FUNC_mac_set_ctx_params_fn hmac_set_ctx_params;
static OSSL_FUNC_mac_init_fn hmac_init;
static OSSL_FUNC_mac_update_fn hmac_update;
static OSSL_FUNC_mac_final_fn hmac_final;

/* local HMAC context structure */

/* typedef EVP_MAC_IMPL */
struct hmac_data_st {
    void *provctx;
    HMAC_CTX *ctx;               /* HMAC context */
    PROV_DIGEST digest;
    unsigned char *key;
    size_t keylen;
    /* Length of full TLS record including the MAC and any padding */
    size_t tls_data_size;
    unsigned char tls_header[13];
    int tls_header_set;
    unsigned char tls_mac_out[EVP_MAX_MD_SIZE];
    size_t tls_mac_out_size;
};

/* Defined in ssl/s3_cbc.c */
int ssl3_cbc_digest_record(const EVP_MD *md,
                           unsigned char *md_out,
                           size_t *md_out_size,
                           const unsigned char header[13],
                           const unsigned char *data,
                           size_t data_size,
                           size_t data_plus_mac_plus_padding_size,
                           const unsigned char *mac_secret,
                           size_t mac_secret_length, char is_sslv3);

static void *hmac_new(void *provctx)
{
    struct hmac_data_st *macctx;

    if (!ossl_prov_is_running())
        return NULL;

    if ((macctx = OPENSSL_zalloc(sizeof(*macctx))) == NULL
        || (macctx->ctx = HMAC_CTX_new()) == NULL) {
        OPENSSL_free(macctx);
        return NULL;
    }
    macctx->provctx = provctx;

    return macctx;
}

static void hmac_free(void *vmacctx)
{
    struct hmac_data_st *macctx = vmacctx;

    if (macctx != NULL) {
        HMAC_CTX_free(macctx->ctx);
        ossl_prov_digest_reset(&macctx->digest);
        OPENSSL_secure_clear_free(macctx->key, macctx->keylen);
        OPENSSL_free(macctx);
    }
}

static void *hmac_dup(void *vsrc)
{
    struct hmac_data_st *src = vsrc;
    struct hmac_data_st *dst;
    HMAC_CTX *ctx;

    if (!ossl_prov_is_running())
        return NULL;
    dst = hmac_new(src->provctx);
    if (dst == NULL)
        return NULL;

    ctx = dst->ctx;
    *dst = *src;
    dst->ctx = ctx;
    dst->key = NULL;

    if (!HMAC_CTX_copy(dst->ctx, src->ctx)
        || !ossl_prov_digest_copy(&dst->digest, &src->digest)) {
        hmac_free(dst);
        return NULL;
    }
    if (src->key != NULL) {
        /* There is no "secure" OPENSSL_memdup */
        dst->key = OPENSSL_secure_malloc(src->keylen > 0 ? src->keylen : 1);
        if (dst->key == NULL) {
            hmac_free(dst);
            return 0;
        }
        memcpy(dst->key, src->key, src->keylen);
    }
    return dst;
}

static size_t hmac_size(struct hmac_data_st *macctx)
{
    return HMAC_size(macctx->ctx);
}

static int hmac_block_size(struct hmac_data_st *macctx)
{
    const EVP_MD *md = ossl_prov_digest_md(&macctx->digest);

    if (md == NULL)
        return 0;
    return EVP_MD_block_size(md);
}

static int hmac_setkey(struct hmac_data_st *macctx,
                       const unsigned char *key, size_t keylen)
{
    const EVP_MD *digest;

    if (macctx->keylen > 0)
        OPENSSL_secure_clear_free(macctx->key, macctx->keylen);
    /* Keep a copy of the key in case we need it for TLS HMAC */
    macctx->key = OPENSSL_secure_malloc(keylen > 0 ? keylen : 1);
    if (macctx->key == NULL)
        return 0;
    memcpy(macctx->key, key, keylen);
    macctx->keylen = keylen;

    digest = ossl_prov_digest_md(&macctx->digest);
    /* HMAC_Init_ex doesn't tolerate all zero params, so we must be careful */
    if (key != NULL || (macctx->tls_data_size == 0 && digest != NULL))
        return HMAC_Init_ex(macctx->ctx, key, keylen, digest,
                            ossl_prov_digest_engine(&macctx->digest));
    return 1;
}

static int hmac_init(void *vmacctx, const unsigned char *key,
                     size_t keylen, const OSSL_PARAM params[])
{
    struct hmac_data_st *macctx = vmacctx;

    if (!ossl_prov_is_running() || !hmac_set_ctx_params(macctx, params))
        return 0;

    if (key != NULL && !hmac_setkey(macctx, key, keylen))
        return 0;
    return 1;
}

static int hmac_update(void *vmacctx, const unsigned char *data,
                       size_t datalen)
{
    struct hmac_data_st *macctx = vmacctx;

    if (macctx->tls_data_size > 0) {
        /* We're doing a TLS HMAC */
        if (!macctx->tls_header_set) {
            /* We expect the first update call to contain the TLS header */
            if (datalen != sizeof(macctx->tls_header))
                return 0;
            memcpy(macctx->tls_header, data, datalen);
            macctx->tls_header_set = 1;
            return 1;
        }
        /* macctx->tls_data_size is datalen plus the padding length */
        if (macctx->tls_data_size < datalen)
            return 0;

        return ssl3_cbc_digest_record(ossl_prov_digest_md(&macctx->digest),
                                      macctx->tls_mac_out,
                                      &macctx->tls_mac_out_size,
                                      macctx->tls_header,
                                      data,
                                      datalen,
                                      macctx->tls_data_size,
                                      macctx->key,
                                      macctx->keylen,
                                      0);
    }

    return HMAC_Update(macctx->ctx, data, datalen);
}

static int hmac_final(void *vmacctx, unsigned char *out, size_t *outl,
                      size_t outsize)
{
    unsigned int hlen;
    struct hmac_data_st *macctx = vmacctx;

    if (!ossl_prov_is_running())
        return 0;
    if (macctx->tls_data_size > 0) {
        if (macctx->tls_mac_out_size == 0)
            return 0;
        if (outl != NULL)
            *outl = macctx->tls_mac_out_size;
        memcpy(out, macctx->tls_mac_out, macctx->tls_mac_out_size);
        return 1;
    }
    if (!HMAC_Final(macctx->ctx, out, &hlen))
        return 0;
    *outl = hlen;
    return 1;
}

static const OSSL_PARAM known_gettable_ctx_params[] = {
    OSSL_PARAM_size_t(OSSL_MAC_PARAM_SIZE, NULL),
    OSSL_PARAM_size_t(OSSL_MAC_PARAM_BLOCK_SIZE, NULL),
    OSSL_PARAM_END
};
static const OSSL_PARAM *hmac_gettable_ctx_params(ossl_unused void *ctx,
                                                  ossl_unused void *provctx)
{
    return known_gettable_ctx_params;
}

static int hmac_get_ctx_params(void *vmacctx, OSSL_PARAM params[])
{
    struct hmac_data_st *macctx = vmacctx;
    OSSL_PARAM *p;

    if ((p = OSSL_PARAM_locate(params, OSSL_MAC_PARAM_SIZE)) != NULL
            && !OSSL_PARAM_set_size_t(p, hmac_size(macctx)))
        return 0;

    if ((p = OSSL_PARAM_locate(params, OSSL_MAC_PARAM_BLOCK_SIZE)) != NULL
            && !OSSL_PARAM_set_int(p, hmac_block_size(macctx)))
        return 0;

    return 1;
}

static const OSSL_PARAM known_settable_ctx_params[] = {
    OSSL_PARAM_utf8_string(OSSL_MAC_PARAM_DIGEST, NULL, 0),
    OSSL_PARAM_utf8_string(OSSL_MAC_PARAM_PROPERTIES, NULL, 0),
    OSSL_PARAM_octet_string(OSSL_MAC_PARAM_KEY, NULL, 0),
    OSSL_PARAM_int(OSSL_MAC_PARAM_DIGEST_NOINIT, NULL),
    OSSL_PARAM_int(OSSL_MAC_PARAM_DIGEST_ONESHOT, NULL),
    OSSL_PARAM_size_t(OSSL_MAC_PARAM_TLS_DATA_SIZE, NULL),
    OSSL_PARAM_END
};
static const OSSL_PARAM *hmac_settable_ctx_params(ossl_unused void *ctx,
                                                  ossl_unused void *provctx)
{
    return known_settable_ctx_params;
}

static int set_flag(const OSSL_PARAM params[], const char *key, int mask,
                    int *flags)
{
    const OSSL_PARAM *p = OSSL_PARAM_locate_const(params, key);
    int flag = 0;

    if (p != NULL) {
        if (!OSSL_PARAM_get_int(p, &flag))
            return 0;
        if (flag == 0)
            *flags &= ~mask;
        else
            *flags |= mask;
    }
    return 1;
}

/*
 * ALL parameters should be set before init().
 */
static int hmac_set_ctx_params(void *vmacctx, const OSSL_PARAM params[])
{
    struct hmac_data_st *macctx = vmacctx;
    OSSL_LIB_CTX *ctx = PROV_LIBCTX_OF(macctx->provctx);
    const OSSL_PARAM *p;
    int flags = 0;

    if (params == NULL)
        return 1;

    if (!ossl_prov_digest_load_from_params(&macctx->digest, params, ctx))
        return 0;

    if (!set_flag(params, OSSL_MAC_PARAM_DIGEST_NOINIT, EVP_MD_CTX_FLAG_NO_INIT,
                  &flags))
        return 0;
    if (!set_flag(params, OSSL_MAC_PARAM_DIGEST_ONESHOT, EVP_MD_CTX_FLAG_ONESHOT,
                  &flags))
        return 0;
    if (flags)
        HMAC_CTX_set_flags(macctx->ctx, flags);

    if ((p = OSSL_PARAM_locate_const(params, OSSL_MAC_PARAM_KEY)) != NULL) {
        if (p->data_type != OSSL_PARAM_OCTET_STRING)
            return 0;

        if (macctx->keylen > 0)
            OPENSSL_secure_clear_free(macctx->key, macctx->keylen);
        /* Keep a copy of the key if we need it for TLS HMAC */
        macctx->key = OPENSSL_secure_malloc(p->data_size > 0 ? p->data_size : 1);
        if (macctx->key == NULL)
            return 0;
        memcpy(macctx->key, p->data, p->data_size);
        macctx->keylen = p->data_size;

        if (!HMAC_Init_ex(macctx->ctx, p->data, p->data_size,
                          ossl_prov_digest_md(&macctx->digest),
                          NULL /* ENGINE */))
            return 0;

    }
    if ((p = OSSL_PARAM_locate_const(params,
                                     OSSL_MAC_PARAM_TLS_DATA_SIZE)) != NULL) {
        if (!OSSL_PARAM_get_size_t(p, &macctx->tls_data_size))
            return 0;
    }
    return 1;
}

const OSSL_DISPATCH ossl_hmac_functions[] = {
    { OSSL_FUNC_MAC_NEWCTX, (void (*)(void))hmac_new },
    { OSSL_FUNC_MAC_DUPCTX, (void (*)(void))hmac_dup },
    { OSSL_FUNC_MAC_FREECTX, (void (*)(void))hmac_free },
    { OSSL_FUNC_MAC_INIT, (void (*)(void))hmac_init },
    { OSSL_FUNC_MAC_UPDATE, (void (*)(void))hmac_update },
    { OSSL_FUNC_MAC_FINAL, (void (*)(void))hmac_final },
    { OSSL_FUNC_MAC_GETTABLE_CTX_PARAMS,
      (void (*)(void))hmac_gettable_ctx_params },
    { OSSL_FUNC_MAC_GET_CTX_PARAMS, (void (*)(void))hmac_get_ctx_params },
    { OSSL_FUNC_MAC_SETTABLE_CTX_PARAMS,
      (void (*)(void))hmac_settable_ctx_params },
    { OSSL_FUNC_MAC_SET_CTX_PARAMS, (void (*)(void))hmac_set_ctx_params },
    { 0, NULL }
};
