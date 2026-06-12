/*
 * Copyright 2018-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/proverr.h>

#include "prov/blake2.h"
#include "internal/cryptlib.h"
#include "prov/implementations.h"
#include "prov/providercommon.h"

#include "prov/blake2_params.inc"

/*
 * Forward declaration of everything implemented here.  This is not strictly
 * necessary for the compiler, but provides an assurance that the signatures
 * of the functions in the dispatch table are correct.
 */
static OSSL_FUNC_mac_newctx_fn blake2_mac_new;
static OSSL_FUNC_mac_dupctx_fn blake2_mac_dup;
static OSSL_FUNC_mac_freectx_fn blake2_mac_free;
static OSSL_FUNC_mac_gettable_ctx_params_fn blake2_gettable_ctx_params;
static OSSL_FUNC_mac_get_ctx_params_fn blake2_get_ctx_params;
static OSSL_FUNC_mac_settable_ctx_params_fn blake2_mac_settable_ctx_params;
static OSSL_FUNC_mac_set_ctx_params_fn blake2_mac_set_ctx_params;
static OSSL_FUNC_mac_init_fn blake2_mac_init;
static OSSL_FUNC_mac_update_fn blake2_mac_update;
static OSSL_FUNC_mac_final_fn blake2_mac_final;

struct blake2_mac_data_st {
    BLAKE2_CTX ctx;
    BLAKE2_PARAM params;
    unsigned char key[BLAKE2_KEYBYTES];
};

static void *blake2_mac_new(void *unused_provctx)
{
    struct blake2_mac_data_st *macctx;

    if (!ossl_prov_is_running())
        return NULL;

    macctx = OPENSSL_zalloc(sizeof(*macctx));
    if (macctx != NULL) {
        BLAKE2_PARAM_INIT(&macctx->params);
        /* ctx initialization is deferred to BLAKE2b_Init() */
    }
    return macctx;
}

static void *blake2_mac_dup(void *vsrc)
{
    struct blake2_mac_data_st *dst;
    struct blake2_mac_data_st *src = vsrc;

    if (!ossl_prov_is_running())
        return NULL;

    dst = OPENSSL_zalloc(sizeof(*dst));
    if (dst == NULL)
        return NULL;

    *dst = *src;
    return dst;
}

static void blake2_mac_free(void *vmacctx)
{
    struct blake2_mac_data_st *macctx = vmacctx;

    if (macctx != NULL) {
        OPENSSL_cleanse(macctx->key, sizeof(macctx->key));
        OPENSSL_free(macctx);
    }
}

static size_t blake2_mac_size(void *vmacctx)
{
    struct blake2_mac_data_st *macctx = vmacctx;

    return macctx->params.digest_length;
}

static int blake2_setkey(struct blake2_mac_data_st *macctx,
                         const unsigned char *key, size_t keylen)
{
    if (keylen > BLAKE2_KEYBYTES || keylen == 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
        return 0;
    }
    memcpy(macctx->key, key, keylen);
    /* Pad with zeroes at the end if required */
    if (keylen < BLAKE2_KEYBYTES)
        memset(macctx->key + keylen, 0, BLAKE2_KEYBYTES - keylen);
    BLAKE2_PARAM_SET_KEY_LENGTH(&macctx->params, (uint8_t)keylen);
    return 1;
}

static int blake2_mac_init(void *vmacctx, const unsigned char *key,
                           size_t keylen, const OSSL_PARAM params[])
{
    struct blake2_mac_data_st *macctx = vmacctx;

    if (!ossl_prov_is_running() || !blake2_mac_set_ctx_params(macctx, params))
        return 0;
    if (key != NULL) {
        if (!blake2_setkey(macctx, key, keylen))
            return 0;
    } else if (macctx->params.key_length == 0) {
        /* Check key has been set */
        ERR_raise(ERR_LIB_PROV, PROV_R_NO_KEY_SET);
        return 0;
    }
    return BLAKE2_INIT_KEY(&macctx->ctx, &macctx->params, macctx->key);
}

static int blake2_mac_update(void *vmacctx,
                             const unsigned char *data, size_t datalen)
{
    struct blake2_mac_data_st *macctx = vmacctx;

    if (datalen == 0)
        return 1;

    return BLAKE2_UPDATE(&macctx->ctx, data, datalen);
}

static int blake2_mac_final(void *vmacctx,
                            unsigned char *out, size_t *outl,
                            size_t outsize)
{
    struct blake2_mac_data_st *macctx = vmacctx;

    if (!ossl_prov_is_running())
        return 0;

    *outl = blake2_mac_size(macctx);
    return BLAKE2_FINAL(out, &macctx->ctx);
}

/* See blake2.h for parameter definition */
static const OSSL_PARAM *blake2_gettable_ctx_params(ossl_unused void *ctx,
                                                    ossl_unused void *provctx)
{
    return blake2_get_ctx_list;
}

static int blake2_get_ctx_params(void *vmacctx, OSSL_PARAM params[])
{
    struct blake2_mac_data_st *macctx = vmacctx;
    struct blake2_get_ctx_st p;

    if (macctx == NULL || !blake2_get_ctx_decoder(params, &p))
        return 0;

    if (p.size != NULL
            && !OSSL_PARAM_set_size_t(p.size, blake2_mac_size(macctx)))
        return 0;

    if (p.bsize != NULL && !OSSL_PARAM_set_size_t(p.bsize, BLAKE2_BLOCKBYTES))
        return 0;

    return 1;
}

static const OSSL_PARAM *blake2_mac_settable_ctx_params(
            ossl_unused void *ctx, ossl_unused void *p_ctx)
{
    return blake2_mac_set_ctx_list;
}

/*
 * ALL parameters should be set before init().
 */
static int blake2_mac_set_ctx_params(void *vmacctx, const OSSL_PARAM params[])
{
    struct blake2_mac_data_st *macctx = vmacctx;
    struct blake2_mac_set_ctx_st p;

    if (macctx == NULL || !blake2_mac_set_ctx_decoder(params, &p))
        return 0;

    if (p.size != NULL) {
        size_t size;

        if (!OSSL_PARAM_get_size_t(p.size, &size)
            || size < 1
            || size > BLAKE2_OUTBYTES) {
            ERR_raise(ERR_LIB_PROV, PROV_R_NOT_XOF_OR_INVALID_LENGTH);
            return 0;
        }
        BLAKE2_PARAM_SET_DIGEST_LENGTH(&macctx->params, (uint8_t)size);
    }

    if (p.key != NULL)
        if (p.key->data_type != OSSL_PARAM_OCTET_STRING
                || !blake2_setkey(macctx, p.key->data, p.key->data_size))
            return 0;

    if (p.cust != NULL) {
        if (p.cust->data_type != OSSL_PARAM_OCTET_STRING)
            return 0;
        /*
         * The OSSL_PARAM API doesn't provide direct pointer use, so we
         * must handle the OSSL_PARAM structure ourselves here
         */
        if (p.cust->data_size > BLAKE2_PERSONALBYTES) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_CUSTOM_LENGTH);
            return 0;
        }
        BLAKE2_PARAM_SET_PERSONAL(&macctx->params, p.cust->data,
                                  p.cust->data_size);
    }

    if (p.salt != NULL) {
        if (p.salt->data_type != OSSL_PARAM_OCTET_STRING)
            return 0;
        /*
         * The OSSL_PARAM API doesn't provide direct pointer use, so we
         * must handle the OSSL_PARAM structure ourselves here as well
         */
        if (p.salt->data_size > BLAKE2_SALTBYTES) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_SALT_LENGTH);
            return 0;
        }
        BLAKE2_PARAM_SET_SALT(&macctx->params, p.salt->data, p.salt->data_size);
    }
    return 1;
}

const OSSL_DISPATCH BLAKE2_FUNCTIONS[] = {
    { OSSL_FUNC_MAC_NEWCTX, (void (*)(void))blake2_mac_new },
    { OSSL_FUNC_MAC_DUPCTX, (void (*)(void))blake2_mac_dup },
    { OSSL_FUNC_MAC_FREECTX, (void (*)(void))blake2_mac_free },
    { OSSL_FUNC_MAC_INIT, (void (*)(void))blake2_mac_init },
    { OSSL_FUNC_MAC_UPDATE, (void (*)(void))blake2_mac_update },
    { OSSL_FUNC_MAC_FINAL, (void (*)(void))blake2_mac_final },
    { OSSL_FUNC_MAC_GETTABLE_CTX_PARAMS,
      (void (*)(void))blake2_gettable_ctx_params },
    { OSSL_FUNC_MAC_GET_CTX_PARAMS, (void (*)(void))blake2_get_ctx_params },
    { OSSL_FUNC_MAC_SETTABLE_CTX_PARAMS,
      (void (*)(void))blake2_mac_settable_ctx_params },
    { OSSL_FUNC_MAC_SET_CTX_PARAMS, (void (*)(void))blake2_mac_set_ctx_params },
    OSSL_DISPATCH_END
};
