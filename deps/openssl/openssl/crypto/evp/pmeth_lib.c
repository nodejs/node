/*
 * Copyright 2006-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Low level key APIs (DH etc) are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/dh.h>
#include <openssl/rsa.h>
#include <openssl/kdf.h>
#include "internal/cryptlib.h"
#ifndef FIPS_MODULE
#include "crypto/asn1.h"
#endif
#include "crypto/evp.h"
#include "crypto/dh.h"
#include "crypto/ec.h"
#include "internal/ffc.h"
#include "internal/numbers.h"
#include "internal/provider.h"
#include "internal/common.h"
#include "evp_local.h"

#ifndef FIPS_MODULE

static int evp_pkey_ctx_store_cached_data(EVP_PKEY_CTX *ctx,
    int keytype, int optype,
    int cmd, const char *name,
    const void *data, size_t data_len);
static void evp_pkey_ctx_free_cached_data(EVP_PKEY_CTX *ctx,
    int cmd, const char *name);
static void evp_pkey_ctx_free_all_cached_data(EVP_PKEY_CTX *ctx);

#endif /* FIPS_MODULE */

int evp_pkey_ctx_state(const EVP_PKEY_CTX *ctx)
{
    if (ctx->operation == EVP_PKEY_OP_UNDEFINED)
        return EVP_PKEY_STATE_UNKNOWN;

    if ((EVP_PKEY_CTX_IS_DERIVE_OP(ctx)
            && ctx->op.kex.algctx != NULL)
        || (EVP_PKEY_CTX_IS_SIGNATURE_OP(ctx)
            && ctx->op.sig.algctx != NULL)
        || (EVP_PKEY_CTX_IS_ASYM_CIPHER_OP(ctx)
            && ctx->op.ciph.algctx != NULL)
        || (EVP_PKEY_CTX_IS_GEN_OP(ctx)
            && ctx->op.keymgmt.genctx != NULL)
        || (EVP_PKEY_CTX_IS_KEM_OP(ctx)
            && ctx->op.encap.algctx != NULL))
        return EVP_PKEY_STATE_PROVIDER;

    return EVP_PKEY_STATE_LEGACY;
}

static EVP_PKEY_CTX *int_ctx_new(OSSL_LIB_CTX *libctx, EVP_PKEY *pkey,
    const char *keytype, const char *propquery,
    int id)
{
    EVP_PKEY_CTX *ret = NULL;
    EVP_KEYMGMT *keymgmt = NULL;

    /* Code below to be removed when legacy support is dropped. */
    /* BEGIN legacy */
    if (id == -1) {
        if (pkey != NULL && !evp_pkey_is_provided(pkey)) {
            id = pkey->type;
        } else {
            if (pkey != NULL) {
                /* Must be provided if we get here */
                keytype = EVP_KEYMGMT_get0_name(pkey->keymgmt);
            }
#ifndef FIPS_MODULE
            if (keytype != NULL) {
                id = evp_pkey_name2type(keytype);
                if (id == NID_undef)
                    id = -1;
            }
#endif
        }
    }

#ifndef FIPS_MODULE
    /*
     * Here, we extract what information we can for the purpose of
     * supporting usage with implementations from providers, to make
     * for a smooth transition from legacy stuff to provider based stuff.
     */
    if (id != -1)
        keytype = OBJ_nid2sn(id);

    /* END legacy */
#endif /* FIPS_MODULE */
    /* We try fetching a provider implementation. */
    if (keytype != NULL) {
        /*
         * If |pkey| is given and is provided, we take a reference to its
         * keymgmt.  Otherwise, we fetch one for the keytype we got. This
         * is to ensure that operation init functions can access what they
         * need through this single pointer.
         */
        if (pkey != NULL && pkey->keymgmt != NULL) {
            if (!EVP_KEYMGMT_up_ref(pkey->keymgmt))
                ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
            else
                keymgmt = pkey->keymgmt;
        } else {
            keymgmt = EVP_KEYMGMT_fetch(libctx, keytype, propquery);
        }
        if (keymgmt == NULL)
            return NULL; /* EVP_KEYMGMT_fetch() recorded an error */

#ifndef FIPS_MODULE
        /*
         * Chase down the legacy NID, as that might be needed for diverse
         * purposes, such as ensure that EVP_PKEY_type() can return sensible
         * values. We go through all keymgmt names, because the keytype
         * that's passed to this function doesn't necessarily translate
         * directly.
         */
        if (keymgmt != NULL) {
            int tmp_id = evp_keymgmt_get_legacy_alg(keymgmt);

            if (tmp_id != NID_undef) {
                if (id == -1) {
                    id = tmp_id;
                } else {
                    /*
                     * It really really shouldn't differ.  If it still does,
                     * something is very wrong.
                     */
                    if (!ossl_assert(id == tmp_id)) {
                        ERR_raise(ERR_LIB_EVP, ERR_R_INTERNAL_ERROR);
                        EVP_KEYMGMT_free(keymgmt);
                        return NULL;
                    }
                }
            }
        }
#endif
    }

    if (keymgmt == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    } else {
        ret = OPENSSL_zalloc(sizeof(*ret));
    }

    if (ret == NULL) {
        EVP_KEYMGMT_free(keymgmt);
        return NULL;
    }
    if (propquery != NULL) {
        ret->propquery = OPENSSL_strdup(propquery);
        if (ret->propquery == NULL) {
            OPENSSL_free(ret);
            EVP_KEYMGMT_free(keymgmt);
            return NULL;
        }
    }
    ret->libctx = libctx;
    ret->keytype = keytype;
    ret->keymgmt = keymgmt;
    ret->legacy_keytype = id;
    ret->operation = EVP_PKEY_OP_UNDEFINED;

    if (pkey != NULL && !EVP_PKEY_up_ref(pkey)) {
        EVP_PKEY_CTX_free(ret);
        return NULL;
    }

    ret->pkey = pkey;

    return ret;
}

/*- All methods below can also be used in FIPS_MODULE */

EVP_PKEY_CTX *EVP_PKEY_CTX_new_from_name(OSSL_LIB_CTX *libctx,
    const char *name,
    const char *propquery)
{
    return int_ctx_new(libctx, NULL, name, propquery, -1);
}

EVP_PKEY_CTX *EVP_PKEY_CTX_new_from_pkey(OSSL_LIB_CTX *libctx, EVP_PKEY *pkey,
    const char *propquery)
{
    return int_ctx_new(libctx, pkey, NULL, propquery, -1);
}

void evp_pkey_ctx_free_old_ops(EVP_PKEY_CTX *ctx)
{
    if (EVP_PKEY_CTX_IS_SIGNATURE_OP(ctx)) {
        if (ctx->op.sig.algctx != NULL && ctx->op.sig.signature != NULL)
            ctx->op.sig.signature->freectx(ctx->op.sig.algctx);
        EVP_SIGNATURE_free(ctx->op.sig.signature);
        ctx->op.sig.algctx = NULL;
        ctx->op.sig.signature = NULL;
    } else if (EVP_PKEY_CTX_IS_DERIVE_OP(ctx)) {
        if (ctx->op.kex.algctx != NULL && ctx->op.kex.exchange != NULL)
            ctx->op.kex.exchange->freectx(ctx->op.kex.algctx);
        EVP_KEYEXCH_free(ctx->op.kex.exchange);
        ctx->op.kex.algctx = NULL;
        ctx->op.kex.exchange = NULL;
    } else if (EVP_PKEY_CTX_IS_KEM_OP(ctx)) {
        if (ctx->op.encap.algctx != NULL && ctx->op.encap.kem != NULL)
            ctx->op.encap.kem->freectx(ctx->op.encap.algctx);
        EVP_KEM_free(ctx->op.encap.kem);
        ctx->op.encap.algctx = NULL;
        ctx->op.encap.kem = NULL;
    } else if (EVP_PKEY_CTX_IS_ASYM_CIPHER_OP(ctx)) {
        if (ctx->op.ciph.algctx != NULL && ctx->op.ciph.cipher != NULL)
            ctx->op.ciph.cipher->freectx(ctx->op.ciph.algctx);
        EVP_ASYM_CIPHER_free(ctx->op.ciph.cipher);
        ctx->op.ciph.algctx = NULL;
        ctx->op.ciph.cipher = NULL;
    } else if (EVP_PKEY_CTX_IS_GEN_OP(ctx)) {
        if (ctx->op.keymgmt.genctx != NULL && ctx->keymgmt != NULL)
            evp_keymgmt_gen_cleanup(ctx->keymgmt, ctx->op.keymgmt.genctx);
    }
}

void EVP_PKEY_CTX_free(EVP_PKEY_CTX *ctx)
{
    if (ctx == NULL)
        return;

    evp_pkey_ctx_free_old_ops(ctx);
#ifndef FIPS_MODULE
    evp_pkey_ctx_free_all_cached_data(ctx);
#endif
    EVP_KEYMGMT_free(ctx->keymgmt);

    OPENSSL_free(ctx->propquery);
    EVP_PKEY_free(ctx->pkey);
    EVP_PKEY_free(ctx->peerkey);
    BN_free(ctx->rsa_pubexp);
    OPENSSL_free(ctx);
}

#ifndef FIPS_MODULE
EVP_PKEY_CTX *EVP_PKEY_CTX_new(EVP_PKEY *pkey, ENGINE *e)
{
    if (!ossl_assert(e == NULL))
        return NULL;
    return int_ctx_new(NULL, pkey, NULL, NULL, -1);
}

EVP_PKEY_CTX *EVP_PKEY_CTX_new_id(int id, ENGINE *e)
{
    if (!ossl_assert(e == NULL))
        return NULL;
    return int_ctx_new(NULL, NULL, NULL, NULL, id);
}

EVP_PKEY_CTX *EVP_PKEY_CTX_dup(const EVP_PKEY_CTX *pctx)
{
    EVP_PKEY_CTX *rctx;

    rctx = OPENSSL_zalloc(sizeof(*rctx));
    if (rctx == NULL)
        return NULL;

    if (pctx->pkey != NULL && !EVP_PKEY_up_ref(pctx->pkey))
        goto err;

    rctx->pkey = pctx->pkey;
    rctx->operation = pctx->operation;
    rctx->libctx = pctx->libctx;
    rctx->keytype = pctx->keytype;
    rctx->propquery = NULL;
    if (pctx->propquery != NULL) {
        rctx->propquery = OPENSSL_strdup(pctx->propquery);
        if (rctx->propquery == NULL)
            goto err;
    }
    rctx->legacy_keytype = pctx->legacy_keytype;

    if (pctx->keymgmt != NULL) {
        if (!EVP_KEYMGMT_up_ref(pctx->keymgmt))
            goto err;
        rctx->keymgmt = pctx->keymgmt;
    }

    if (EVP_PKEY_CTX_IS_DERIVE_OP(pctx)) {
        if (pctx->op.kex.exchange != NULL) {
            rctx->op.kex.exchange = pctx->op.kex.exchange;
            if (!EVP_KEYEXCH_up_ref(rctx->op.kex.exchange))
                goto err;
        }
        if (pctx->op.kex.algctx != NULL) {
            if (!ossl_assert(pctx->op.kex.exchange != NULL))
                goto err;

            if (pctx->op.kex.exchange->dupctx != NULL)
                rctx->op.kex.algctx
                    = pctx->op.kex.exchange->dupctx(pctx->op.kex.algctx);

            if (rctx->op.kex.algctx == NULL) {
                EVP_KEYEXCH_free(rctx->op.kex.exchange);
                rctx->op.kex.exchange = NULL;
                goto err;
            }
            return rctx;
        }
    } else if (EVP_PKEY_CTX_IS_SIGNATURE_OP(pctx)) {
        if (pctx->op.sig.signature != NULL) {
            rctx->op.sig.signature = pctx->op.sig.signature;
            if (!EVP_SIGNATURE_up_ref(rctx->op.sig.signature))
                goto err;
        }
        if (pctx->op.sig.algctx != NULL) {
            if (!ossl_assert(pctx->op.sig.signature != NULL))
                goto err;

            if (pctx->op.sig.signature->dupctx != NULL)
                rctx->op.sig.algctx
                    = pctx->op.sig.signature->dupctx(pctx->op.sig.algctx);

            if (rctx->op.sig.algctx == NULL) {
                EVP_SIGNATURE_free(rctx->op.sig.signature);
                rctx->op.sig.signature = NULL;
                goto err;
            }
            return rctx;
        }
    } else if (EVP_PKEY_CTX_IS_ASYM_CIPHER_OP(pctx)) {
        if (pctx->op.ciph.cipher != NULL) {
            rctx->op.ciph.cipher = pctx->op.ciph.cipher;
            if (!EVP_ASYM_CIPHER_up_ref(rctx->op.ciph.cipher))
                goto err;
        }
        if (pctx->op.ciph.algctx != NULL) {
            if (!ossl_assert(pctx->op.ciph.cipher != NULL))
                goto err;

            if (pctx->op.ciph.cipher->dupctx != NULL)
                rctx->op.ciph.algctx
                    = pctx->op.ciph.cipher->dupctx(pctx->op.ciph.algctx);

            if (rctx->op.ciph.algctx == NULL) {
                EVP_ASYM_CIPHER_free(rctx->op.ciph.cipher);
                rctx->op.ciph.cipher = NULL;
                goto err;
            }
            return rctx;
        }
    } else if (EVP_PKEY_CTX_IS_KEM_OP(pctx)) {
        if (pctx->op.encap.kem != NULL) {
            rctx->op.encap.kem = pctx->op.encap.kem;
            if (!EVP_KEM_up_ref(rctx->op.encap.kem))
                goto err;
        }
        if (pctx->op.encap.algctx != NULL) {
            if (!ossl_assert(pctx->op.encap.kem != NULL))
                goto err;

            if (pctx->op.encap.kem->dupctx != NULL)
                rctx->op.encap.algctx
                    = pctx->op.encap.kem->dupctx(pctx->op.encap.algctx);

            if (rctx->op.encap.algctx == NULL) {
                EVP_KEM_free(rctx->op.encap.kem);
                rctx->op.encap.kem = NULL;
                goto err;
            }
            return rctx;
        }
    } else if (EVP_PKEY_CTX_IS_GEN_OP(pctx)) {
        /* Not supported - This would need a gen_dupctx() to work */
        goto err;
    }

    if (pctx->peerkey != NULL && !EVP_PKEY_up_ref(pctx->peerkey))
        goto err;

    rctx->peerkey = pctx->peerkey;

    if (rctx->operation == EVP_PKEY_OP_UNDEFINED) {
        EVP_KEYMGMT *tmp_keymgmt = pctx->keymgmt;
        void *provkey;

        if (pctx->pkey == NULL)
            return rctx;

        provkey = evp_pkey_export_to_provider(pctx->pkey, pctx->libctx,
            &tmp_keymgmt, pctx->propquery);
        if (provkey == NULL)
            goto err;
        if (!EVP_KEYMGMT_up_ref(tmp_keymgmt))
            goto err;
        EVP_KEYMGMT_free(rctx->keymgmt);
        rctx->keymgmt = tmp_keymgmt;
        return rctx;
    }
err:
    EVP_PKEY_CTX_free(rctx);
    return NULL;
}
#endif

int EVP_PKEY_CTX_is_a(EVP_PKEY_CTX *ctx, const char *keytype)
{
#ifndef FIPS_MODULE
    if (evp_pkey_ctx_is_legacy(ctx))
        return (ctx->legacy_keytype == evp_pkey_name2type(keytype));
#endif
    return EVP_KEYMGMT_is_a(ctx->keymgmt, keytype);
}

int EVP_PKEY_CTX_set_params(EVP_PKEY_CTX *ctx, const OSSL_PARAM *params)
{
    switch (evp_pkey_ctx_state(ctx)) {
    case EVP_PKEY_STATE_PROVIDER:
        if (EVP_PKEY_CTX_IS_DERIVE_OP(ctx)
            && ctx->op.kex.exchange != NULL
            && ctx->op.kex.exchange->set_ctx_params != NULL)
            return ctx->op.kex.exchange->set_ctx_params(ctx->op.kex.algctx,
                params);
        if (EVP_PKEY_CTX_IS_SIGNATURE_OP(ctx)
            && ctx->op.sig.signature != NULL
            && ctx->op.sig.signature->set_ctx_params != NULL)
            return ctx->op.sig.signature->set_ctx_params(ctx->op.sig.algctx,
                params);
        if (EVP_PKEY_CTX_IS_ASYM_CIPHER_OP(ctx)
            && ctx->op.ciph.cipher != NULL
            && ctx->op.ciph.cipher->set_ctx_params != NULL)
            return ctx->op.ciph.cipher->set_ctx_params(ctx->op.ciph.algctx,
                params);
        if (EVP_PKEY_CTX_IS_GEN_OP(ctx)
            && ctx->keymgmt != NULL
            && ctx->keymgmt->gen_set_params != NULL)
            return evp_keymgmt_gen_set_params(ctx->keymgmt, ctx->op.keymgmt.genctx,
                params);
        if (EVP_PKEY_CTX_IS_KEM_OP(ctx)
            && ctx->op.encap.kem != NULL
            && ctx->op.encap.kem->set_ctx_params != NULL)
            return ctx->op.encap.kem->set_ctx_params(ctx->op.encap.algctx,
                params);
        break;
    case EVP_PKEY_STATE_UNKNOWN:
        break;
#ifndef FIPS_MODULE
    case EVP_PKEY_STATE_LEGACY:
        return evp_pkey_ctx_set_params_to_ctrl(ctx, params);
#endif
    }
    return 0;
}

int EVP_PKEY_CTX_get_params(EVP_PKEY_CTX *ctx, OSSL_PARAM *params)
{
    switch (evp_pkey_ctx_state(ctx)) {
    case EVP_PKEY_STATE_PROVIDER:
        if (EVP_PKEY_CTX_IS_DERIVE_OP(ctx)
            && ctx->op.kex.exchange != NULL
            && ctx->op.kex.exchange->get_ctx_params != NULL)
            return ctx->op.kex.exchange->get_ctx_params(ctx->op.kex.algctx,
                params);
        if (EVP_PKEY_CTX_IS_SIGNATURE_OP(ctx)
            && ctx->op.sig.signature != NULL
            && ctx->op.sig.signature->get_ctx_params != NULL)
            return ctx->op.sig.signature->get_ctx_params(ctx->op.sig.algctx,
                params);
        if (EVP_PKEY_CTX_IS_ASYM_CIPHER_OP(ctx)
            && ctx->op.ciph.cipher != NULL
            && ctx->op.ciph.cipher->get_ctx_params != NULL)
            return ctx->op.ciph.cipher->get_ctx_params(ctx->op.ciph.algctx,
                params);
        if (EVP_PKEY_CTX_IS_KEM_OP(ctx)
            && ctx->op.encap.kem != NULL
            && ctx->op.encap.kem->get_ctx_params != NULL)
            return ctx->op.encap.kem->get_ctx_params(ctx->op.encap.algctx,
                params);
        if (EVP_PKEY_CTX_IS_GEN_OP(ctx)
            && ctx->keymgmt != NULL
            && ctx->keymgmt->gen_get_params != NULL)
            return evp_keymgmt_gen_get_params(ctx->keymgmt, ctx->op.keymgmt.genctx,
                params);
        break;
    case EVP_PKEY_STATE_UNKNOWN:
        break;
#ifndef FIPS_MODULE
    case EVP_PKEY_STATE_LEGACY:
        return evp_pkey_ctx_get_params_to_ctrl(ctx, params);
#endif
    }
    ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_GET_CTX_PARAMS_NOT_SUPPORTED,
        "EVP_PKEY_OP=0x%x", ctx->operation);
    return 0;
}

#ifndef FIPS_MODULE
const OSSL_PARAM *EVP_PKEY_CTX_gettable_params(const EVP_PKEY_CTX *ctx)
{
    void *provctx;

    if (EVP_PKEY_CTX_IS_DERIVE_OP(ctx)
        && ctx->op.kex.exchange != NULL
        && ctx->op.kex.exchange->gettable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(EVP_KEYEXCH_get0_provider(ctx->op.kex.exchange));
        return ctx->op.kex.exchange->gettable_ctx_params(ctx->op.kex.algctx,
            provctx);
    }
    if (EVP_PKEY_CTX_IS_SIGNATURE_OP(ctx)
        && ctx->op.sig.signature != NULL
        && ctx->op.sig.signature->gettable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(
            EVP_SIGNATURE_get0_provider(ctx->op.sig.signature));
        return ctx->op.sig.signature->gettable_ctx_params(ctx->op.sig.algctx,
            provctx);
    }
    if (EVP_PKEY_CTX_IS_ASYM_CIPHER_OP(ctx)
        && ctx->op.ciph.cipher != NULL
        && ctx->op.ciph.cipher->gettable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(
            EVP_ASYM_CIPHER_get0_provider(ctx->op.ciph.cipher));
        return ctx->op.ciph.cipher->gettable_ctx_params(ctx->op.ciph.algctx,
            provctx);
    }
    if (EVP_PKEY_CTX_IS_KEM_OP(ctx)
        && ctx->op.encap.kem != NULL
        && ctx->op.encap.kem->gettable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(EVP_KEM_get0_provider(ctx->op.encap.kem));
        return ctx->op.encap.kem->gettable_ctx_params(ctx->op.encap.algctx,
            provctx);
    }
    if (EVP_PKEY_CTX_IS_GEN_OP(ctx)
        && ctx->keymgmt != NULL
        && ctx->keymgmt->gen_gettable_params != NULL) {
        provctx = ossl_provider_ctx(EVP_KEYMGMT_get0_provider(ctx->keymgmt));
        return ctx->keymgmt->gen_gettable_params(ctx->op.keymgmt.genctx,
            provctx);
    }
    return NULL;
}

const OSSL_PARAM *EVP_PKEY_CTX_settable_params(const EVP_PKEY_CTX *ctx)
{
    void *provctx;

    if (EVP_PKEY_CTX_IS_DERIVE_OP(ctx)
        && ctx->op.kex.exchange != NULL
        && ctx->op.kex.exchange->settable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(EVP_KEYEXCH_get0_provider(ctx->op.kex.exchange));
        return ctx->op.kex.exchange->settable_ctx_params(ctx->op.kex.algctx,
            provctx);
    }
    if (EVP_PKEY_CTX_IS_SIGNATURE_OP(ctx)
        && ctx->op.sig.signature != NULL
        && ctx->op.sig.signature->settable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(
            EVP_SIGNATURE_get0_provider(ctx->op.sig.signature));
        return ctx->op.sig.signature->settable_ctx_params(ctx->op.sig.algctx,
            provctx);
    }
    if (EVP_PKEY_CTX_IS_ASYM_CIPHER_OP(ctx)
        && ctx->op.ciph.cipher != NULL
        && ctx->op.ciph.cipher->settable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(
            EVP_ASYM_CIPHER_get0_provider(ctx->op.ciph.cipher));
        return ctx->op.ciph.cipher->settable_ctx_params(ctx->op.ciph.algctx,
            provctx);
    }
    if (EVP_PKEY_CTX_IS_GEN_OP(ctx)
        && ctx->keymgmt != NULL
        && ctx->keymgmt->gen_settable_params != NULL) {
        provctx = ossl_provider_ctx(EVP_KEYMGMT_get0_provider(ctx->keymgmt));
        return ctx->keymgmt->gen_settable_params(ctx->op.keymgmt.genctx,
            provctx);
    }
    if (EVP_PKEY_CTX_IS_KEM_OP(ctx)
        && ctx->op.encap.kem != NULL
        && ctx->op.encap.kem->settable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(EVP_KEM_get0_provider(ctx->op.encap.kem));
        return ctx->op.encap.kem->settable_ctx_params(ctx->op.encap.algctx,
            provctx);
    }
    return NULL;
}

/*
 * Internal helpers for stricter EVP_PKEY_CTX_{set,get}_params().
 *
 * Return 1 on success, 0 or negative for errors.
 *
 * In particular they return -2 if any of the params is not supported.
 *
 * They are not available in FIPS_MODULE as they depend on
 *      - EVP_PKEY_CTX_{get,set}_params()
 *      - EVP_PKEY_CTX_{gettable,settable}_params()
 *
 */
int evp_pkey_ctx_set_params_strict(EVP_PKEY_CTX *ctx, OSSL_PARAM *params)
{
    if (ctx == NULL || params == NULL)
        return 0;

    /*
     * We only check for provider side EVP_PKEY_CTX.  For #legacy, we
     * depend on the translation that happens in EVP_PKEY_CTX_set_params()
     * call, and that the resulting ctrl call will return -2 if it doesn't
     * known the ctrl command number.
     */
    if (evp_pkey_ctx_is_provided(ctx)) {
        const OSSL_PARAM *settable = EVP_PKEY_CTX_settable_params(ctx);
        const OSSL_PARAM *p;

        for (p = params; p->key != NULL; p++) {
            /* Check the ctx actually understands this parameter */
            if (OSSL_PARAM_locate_const(settable, p->key) == NULL)
                return -2;
        }
    }

    return EVP_PKEY_CTX_set_params(ctx, params);
}

int evp_pkey_ctx_get_params_strict(EVP_PKEY_CTX *ctx, OSSL_PARAM *params)
{
    if (ctx == NULL || params == NULL)
        return 0;

    /*
     * We only check for provider side EVP_PKEY_CTX.  For #legacy, we
     * depend on the translation that happens in EVP_PKEY_CTX_get_params()
     * call, and that the resulting ctrl call will return -2 if it doesn't
     * known the ctrl command number.
     */
    if (evp_pkey_ctx_is_provided(ctx)) {
        const OSSL_PARAM *gettable = EVP_PKEY_CTX_gettable_params(ctx);
        const OSSL_PARAM *p;

        for (p = params; p->key != NULL; p++) {
            /* Check the ctx actually understands this parameter */
            if (OSSL_PARAM_locate_const(gettable, p->key) == NULL)
                return -2;
        }
    }

    return EVP_PKEY_CTX_get_params(ctx, params);
}

int EVP_PKEY_CTX_get_signature_md(EVP_PKEY_CTX *ctx, const EVP_MD **md)
{
    OSSL_PARAM sig_md_params[2], *p = sig_md_params;
    /* 80 should be big enough */
    char name[80] = "";
    const EVP_MD *tmp;

    if (ctx == NULL || !EVP_PKEY_CTX_IS_SIGNATURE_OP(ctx)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        /* Uses the same return values as EVP_PKEY_CTX_ctrl */
        return -2;
    }

    if (ctx->op.sig.algctx == NULL)
        return EVP_PKEY_CTX_ctrl(ctx, -1, EVP_PKEY_OP_TYPE_SIG,
            EVP_PKEY_CTRL_GET_MD, 0, (void *)(md));

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_SIGNATURE_PARAM_DIGEST,
        name,
        sizeof(name));
    *p = OSSL_PARAM_construct_end();

    if (!EVP_PKEY_CTX_get_params(ctx, sig_md_params))
        return 0;

    tmp = evp_get_digestbyname_ex(ctx->libctx, name);
    if (tmp == NULL)
        return 0;

    *md = tmp;

    return 1;
}

static int evp_pkey_ctx_set_md(EVP_PKEY_CTX *ctx, const EVP_MD *md,
    int fallback, const char *param, int op,
    int ctrl)
{
    OSSL_PARAM md_params[2], *p = md_params;
    const char *name;

    if (ctx == NULL || (ctx->operation & op) == 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        /* Uses the same return values as EVP_PKEY_CTX_ctrl */
        return -2;
    }

    if (fallback)
        return EVP_PKEY_CTX_ctrl(ctx, -1, op, ctrl, 0, (void *)(md));

    if (md == NULL) {
        name = "";
    } else {
        name = EVP_MD_get0_name(md);
    }

    *p++ = OSSL_PARAM_construct_utf8_string(param,
        /*
         * Cast away the const. This is read
         * only so should be safe
         */
        (char *)name, 0);
    *p = OSSL_PARAM_construct_end();

    return EVP_PKEY_CTX_set_params(ctx, md_params);
}

int EVP_PKEY_CTX_set_signature_md(EVP_PKEY_CTX *ctx, const EVP_MD *md)
{
    return evp_pkey_ctx_set_md(ctx, md, ctx->op.sig.algctx == NULL,
        OSSL_SIGNATURE_PARAM_DIGEST,
        EVP_PKEY_OP_TYPE_SIG, EVP_PKEY_CTRL_MD);
}

int EVP_PKEY_CTX_set_tls1_prf_md(EVP_PKEY_CTX *ctx, const EVP_MD *md)
{
    return evp_pkey_ctx_set_md(ctx, md, ctx->op.kex.algctx == NULL,
        OSSL_KDF_PARAM_DIGEST,
        EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_TLS_MD);
}

static int evp_pkey_ctx_set1_octet_string(EVP_PKEY_CTX *ctx, int fallback,
    const char *param, int op, int ctrl,
    const unsigned char *data,
    int datalen)
{
    OSSL_PARAM octet_string_params[2], *p = octet_string_params;

    if (ctx == NULL || (ctx->operation & op) == 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        /* Uses the same return values as EVP_PKEY_CTX_ctrl */
        return -2;
    }

    /* Code below to be removed when legacy support is dropped. */
    if (fallback)
        return EVP_PKEY_CTX_ctrl(ctx, -1, op, ctrl, datalen, (void *)(data));
    /* end of legacy support */

    if (datalen < 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_LENGTH);
        return 0;
    }

    *p++ = OSSL_PARAM_construct_octet_string(param,
        /*
         * Cast away the const. This is read
         * only so should be safe
         */
        (unsigned char *)data,
        (size_t)datalen);
    *p = OSSL_PARAM_construct_end();

    return EVP_PKEY_CTX_set_params(ctx, octet_string_params);
}

static int evp_pkey_ctx_add1_octet_string(EVP_PKEY_CTX *ctx, int fallback,
    const char *param, int op, int ctrl,
    const unsigned char *data,
    int datalen)
{
    OSSL_PARAM os_params[2];
    const OSSL_PARAM *gettables;
    unsigned char *info = NULL;
    size_t info_len = 0;
    size_t info_alloc = 0;
    int ret = 0;

    if (ctx == NULL || (ctx->operation & op) == 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        /* Uses the same return values as EVP_PKEY_CTX_ctrl */
        return -2;
    }

    /* Code below to be removed when legacy support is dropped. */
    if (fallback)
        return EVP_PKEY_CTX_ctrl(ctx, -1, op, ctrl, datalen, (void *)(data));
    /* end of legacy support */

    if (datalen < 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_LENGTH);
        return 0;
    } else if (datalen == 0) {
        return 1;
    }

    /* Check for older provider that doesn't support getting this parameter */
    gettables = EVP_PKEY_CTX_gettable_params(ctx);
    if (gettables == NULL || OSSL_PARAM_locate_const(gettables, param) == NULL)
        return evp_pkey_ctx_set1_octet_string(ctx, fallback, param, op, ctrl,
            data, datalen);

    /* Get the original value length */
    os_params[0] = OSSL_PARAM_construct_octet_string(param, NULL, 0);
    os_params[1] = OSSL_PARAM_construct_end();

    if (!EVP_PKEY_CTX_get_params(ctx, os_params))
        return 0;

    /* This should not happen but check to be sure. */
    if (os_params[0].return_size == OSSL_PARAM_UNMODIFIED)
        return 0;

    info_alloc = os_params[0].return_size + datalen;
    if (info_alloc == 0)
        return 0;
    info = OPENSSL_zalloc(info_alloc);
    if (info == NULL)
        return 0;
    info_len = os_params[0].return_size;

    os_params[0] = OSSL_PARAM_construct_octet_string(param, info, info_alloc);

    /* if we have data, then go get it */
    if (info_len > 0) {
        if (!EVP_PKEY_CTX_get_params(ctx, os_params))
            goto error;
    }

    /* Copy the input data */
    memcpy(&info[info_len], data, datalen);
    ret = EVP_PKEY_CTX_set_params(ctx, os_params);

error:
    OPENSSL_clear_free(info, info_alloc);
    return ret;
}

int EVP_PKEY_CTX_set1_tls1_prf_secret(EVP_PKEY_CTX *ctx,
    const unsigned char *sec, int seclen)
{
    return evp_pkey_ctx_set1_octet_string(ctx, ctx->op.kex.algctx == NULL,
        OSSL_KDF_PARAM_SECRET,
        EVP_PKEY_OP_DERIVE,
        EVP_PKEY_CTRL_TLS_SECRET,
        sec, seclen);
}

int EVP_PKEY_CTX_add1_tls1_prf_seed(EVP_PKEY_CTX *ctx,
    const unsigned char *seed, int seedlen)
{
    return evp_pkey_ctx_set1_octet_string(ctx, ctx->op.kex.algctx == NULL,
        OSSL_KDF_PARAM_SEED,
        EVP_PKEY_OP_DERIVE,
        EVP_PKEY_CTRL_TLS_SEED,
        seed, seedlen);
}

int EVP_PKEY_CTX_set_hkdf_md(EVP_PKEY_CTX *ctx, const EVP_MD *md)
{
    return evp_pkey_ctx_set_md(ctx, md, ctx->op.kex.algctx == NULL,
        OSSL_KDF_PARAM_DIGEST,
        EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_HKDF_MD);
}

int EVP_PKEY_CTX_set1_hkdf_salt(EVP_PKEY_CTX *ctx,
    const unsigned char *salt, int saltlen)
{
    return evp_pkey_ctx_set1_octet_string(ctx, ctx->op.kex.algctx == NULL,
        OSSL_KDF_PARAM_SALT,
        EVP_PKEY_OP_DERIVE,
        EVP_PKEY_CTRL_HKDF_SALT,
        salt, saltlen);
}

int EVP_PKEY_CTX_set1_hkdf_key(EVP_PKEY_CTX *ctx,
    const unsigned char *key, int keylen)
{
    return evp_pkey_ctx_set1_octet_string(ctx, ctx->op.kex.algctx == NULL,
        OSSL_KDF_PARAM_KEY,
        EVP_PKEY_OP_DERIVE,
        EVP_PKEY_CTRL_HKDF_KEY,
        key, keylen);
}

int EVP_PKEY_CTX_add1_hkdf_info(EVP_PKEY_CTX *ctx,
    const unsigned char *info, int infolen)
{
    return evp_pkey_ctx_add1_octet_string(ctx, ctx->op.kex.algctx == NULL,
        OSSL_KDF_PARAM_INFO,
        EVP_PKEY_OP_DERIVE,
        EVP_PKEY_CTRL_HKDF_INFO,
        info, infolen);
}

int EVP_PKEY_CTX_set_hkdf_mode(EVP_PKEY_CTX *ctx, int mode)
{
    OSSL_PARAM int_params[2], *p = int_params;

    if (ctx == NULL || !EVP_PKEY_CTX_IS_DERIVE_OP(ctx)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        /* Uses the same return values as EVP_PKEY_CTX_ctrl */
        return -2;
    }

    /* Code below to be removed when legacy support is dropped. */
    if (ctx->op.kex.algctx == NULL)
        return EVP_PKEY_CTX_ctrl(ctx, -1, EVP_PKEY_OP_DERIVE,
            EVP_PKEY_CTRL_HKDF_MODE, mode, NULL);
    /* end of legacy support */

    if (mode < 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_VALUE);
        return 0;
    }

    *p++ = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_MODE, &mode);
    *p = OSSL_PARAM_construct_end();

    return EVP_PKEY_CTX_set_params(ctx, int_params);
}

int EVP_PKEY_CTX_set1_pbe_pass(EVP_PKEY_CTX *ctx, const char *pass,
    int passlen)
{
    return evp_pkey_ctx_set1_octet_string(ctx, ctx->op.kex.algctx == NULL,
        OSSL_KDF_PARAM_PASSWORD,
        EVP_PKEY_OP_DERIVE,
        EVP_PKEY_CTRL_PASS,
        (const unsigned char *)pass, passlen);
}

int EVP_PKEY_CTX_set1_scrypt_salt(EVP_PKEY_CTX *ctx,
    const unsigned char *salt, int saltlen)
{
    return evp_pkey_ctx_set1_octet_string(ctx, ctx->op.kex.algctx == NULL,
        OSSL_KDF_PARAM_SALT,
        EVP_PKEY_OP_DERIVE,
        EVP_PKEY_CTRL_SCRYPT_SALT,
        salt, saltlen);
}

static int evp_pkey_ctx_set_uint64(EVP_PKEY_CTX *ctx, const char *param,
    int op, int ctrl, uint64_t val)
{
    OSSL_PARAM uint64_params[2], *p = uint64_params;

    if (ctx == NULL || !EVP_PKEY_CTX_IS_DERIVE_OP(ctx)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        /* Uses the same return values as EVP_PKEY_CTX_ctrl */
        return -2;
    }

    /* Code below to be removed when legacy support is dropped. */
    if (ctx->op.kex.algctx == NULL)
        return EVP_PKEY_CTX_ctrl_uint64(ctx, -1, op, ctrl, val);
    /* end of legacy support */

    *p++ = OSSL_PARAM_construct_uint64(param, &val);
    *p = OSSL_PARAM_construct_end();

    return EVP_PKEY_CTX_set_params(ctx, uint64_params);
}

int EVP_PKEY_CTX_set_scrypt_N(EVP_PKEY_CTX *ctx, uint64_t n)
{
    return evp_pkey_ctx_set_uint64(ctx, OSSL_KDF_PARAM_SCRYPT_N,
        EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_SCRYPT_N,
        n);
}

int EVP_PKEY_CTX_set_scrypt_r(EVP_PKEY_CTX *ctx, uint64_t r)
{
    return evp_pkey_ctx_set_uint64(ctx, OSSL_KDF_PARAM_SCRYPT_R,
        EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_SCRYPT_R,
        r);
}

int EVP_PKEY_CTX_set_scrypt_p(EVP_PKEY_CTX *ctx, uint64_t p)
{
    return evp_pkey_ctx_set_uint64(ctx, OSSL_KDF_PARAM_SCRYPT_P,
        EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_SCRYPT_P,
        p);
}

int EVP_PKEY_CTX_set_scrypt_maxmem_bytes(EVP_PKEY_CTX *ctx,
    uint64_t maxmem_bytes)
{
    return evp_pkey_ctx_set_uint64(ctx, OSSL_KDF_PARAM_SCRYPT_MAXMEM,
        EVP_PKEY_OP_DERIVE,
        EVP_PKEY_CTRL_SCRYPT_MAXMEM_BYTES,
        maxmem_bytes);
}

int EVP_PKEY_CTX_set_mac_key(EVP_PKEY_CTX *ctx, const unsigned char *key,
    int keylen)
{
    return evp_pkey_ctx_set1_octet_string(ctx, ctx->op.keymgmt.genctx == NULL,
        OSSL_PKEY_PARAM_PRIV_KEY,
        EVP_PKEY_OP_KEYGEN,
        EVP_PKEY_CTRL_SET_MAC_KEY,
        key, keylen);
}

int EVP_PKEY_CTX_set_kem_op(EVP_PKEY_CTX *ctx, const char *op)
{
    OSSL_PARAM params[2], *p = params;

    if (ctx == NULL || op == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_VALUE);
        return 0;
    }
    if (!EVP_PKEY_CTX_IS_KEM_OP(ctx)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        return -2;
    }
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KEM_PARAM_OPERATION,
        (char *)op, 0);
    *p = OSSL_PARAM_construct_end();
    return EVP_PKEY_CTX_set_params(ctx, params);
}

int EVP_PKEY_CTX_set1_id(EVP_PKEY_CTX *ctx, const void *id, int len)
{
    return EVP_PKEY_CTX_ctrl(ctx, -1, -1,
        EVP_PKEY_CTRL_SET1_ID, (int)len, (void *)(id));
}

int EVP_PKEY_CTX_get1_id(EVP_PKEY_CTX *ctx, void *id)
{
    return EVP_PKEY_CTX_ctrl(ctx, -1, -1, EVP_PKEY_CTRL_GET1_ID, 0, (void *)id);
}

int EVP_PKEY_CTX_get1_id_len(EVP_PKEY_CTX *ctx, size_t *id_len)
{
    return EVP_PKEY_CTX_ctrl(ctx, -1, -1,
        EVP_PKEY_CTRL_GET1_ID_LEN, 0, (void *)id_len);
}

static int evp_pkey_ctx_ctrl_int(EVP_PKEY_CTX *ctx, int keytype, int optype,
    int cmd, int p1, void *p2)
{
    int ret = 0;

    if (ctx->operation == EVP_PKEY_OP_UNDEFINED) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_OPERATION_SET);
        return -1;
    }

    if ((optype != -1) && !(ctx->operation & optype)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_OPERATION);
        return -1;
    }

    switch (evp_pkey_ctx_state(ctx)) {
    case EVP_PKEY_STATE_PROVIDER:
        return evp_pkey_ctx_ctrl_to_param(ctx, keytype, optype, cmd, p1, p2);
    case EVP_PKEY_STATE_UNKNOWN:
    case EVP_PKEY_STATE_LEGACY:
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        return -2;
    }
    return ret;
}

int EVP_PKEY_CTX_ctrl(EVP_PKEY_CTX *ctx, int keytype, int optype,
    int cmd, int p1, void *p2)
{
    int ret = 0;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        return -2;
    }
    /* If unsupported, we don't want that reported here */
    ERR_set_mark();
    ret = evp_pkey_ctx_store_cached_data(ctx, keytype, optype,
        cmd, NULL, p2, p1);
    if (ret == -2) {
        ERR_pop_to_mark();
    } else {
        ERR_clear_last_mark();
        /*
         * If there was an error, there was an error.
         * If the operation isn't initialized yet, we also return, as
         * the saved values will be used then anyway.
         */
        if (ret < 1 || ctx->operation == EVP_PKEY_OP_UNDEFINED)
            return ret;
    }
    return evp_pkey_ctx_ctrl_int(ctx, keytype, optype, cmd, p1, p2);
}

int EVP_PKEY_CTX_ctrl_uint64(EVP_PKEY_CTX *ctx, int keytype, int optype,
    int cmd, uint64_t value)
{
    return EVP_PKEY_CTX_ctrl(ctx, keytype, optype, cmd, 0, &value);
}

static int evp_pkey_ctx_ctrl_str_int(EVP_PKEY_CTX *ctx,
    const char *name, const char *value)
{
    int ret = 0;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        return -2;
    }

    switch (evp_pkey_ctx_state(ctx)) {
    case EVP_PKEY_STATE_PROVIDER:
        return evp_pkey_ctx_ctrl_str_to_param(ctx, name, value);
    case EVP_PKEY_STATE_UNKNOWN:
    case EVP_PKEY_STATE_LEGACY:
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        return -2;
    }

    return ret;
}

int EVP_PKEY_CTX_ctrl_str(EVP_PKEY_CTX *ctx,
    const char *name, const char *value)
{
    int ret = 0;

    /* If unsupported, we don't want that reported here */
    ERR_set_mark();
    ret = evp_pkey_ctx_store_cached_data(ctx, -1, -1, -1,
        name, value, strlen(value) + 1);
    if (ret == -2) {
        ERR_pop_to_mark();
    } else {
        ERR_clear_last_mark();
        /*
         * If there was an error, there was an error.
         * If the operation isn't initialized yet, we also return, as
         * the saved values will be used then anyway.
         */
        if (ret < 1 || ctx->operation == EVP_PKEY_OP_UNDEFINED)
            return ret;
    }

    return evp_pkey_ctx_ctrl_str_int(ctx, name, value);
}

static int decode_cmd(int cmd, const char *name)
{
    if (cmd == -1) {
        /*
         * The consequence of the assertion not being true is that this
         * function will return -1, which will cause the calling functions
         * to signal that the command is unsupported...  in non-debug mode.
         */
        if (ossl_assert(name != NULL))
            if (strcmp(name, "distid") == 0 || strcmp(name, "hexdistid") == 0)
                cmd = EVP_PKEY_CTRL_SET1_ID;
    }

    return cmd;
}

static int evp_pkey_ctx_store_cached_data(EVP_PKEY_CTX *ctx,
    int keytype, int optype,
    int cmd, const char *name,
    const void *data, size_t data_len)
{
    /*
     * Check that it's one of the supported commands.  The ctrl commands
     * number cases here must correspond to the cases in the bottom switch
     * in this function.
     */
    switch (cmd = decode_cmd(cmd, name)) {
    case EVP_PKEY_CTRL_SET1_ID:
        break;
    default:
        ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
        return -2;
    }

    if (keytype != -1) {
        switch (evp_pkey_ctx_state(ctx)) {
        case EVP_PKEY_STATE_PROVIDER:
            if (ctx->keymgmt == NULL) {
                ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
                return -2;
            }
            if (!EVP_KEYMGMT_is_a(ctx->keymgmt,
                    evp_pkey_type2name(keytype))) {
                ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_OPERATION);
                return -1;
            }
            break;
        case EVP_PKEY_STATE_UNKNOWN:
        case EVP_PKEY_STATE_LEGACY:
            ERR_raise(ERR_LIB_EVP, EVP_R_COMMAND_NOT_SUPPORTED);
            return -2;
        }
    }
    if (optype != -1 && (ctx->operation & optype) == 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_OPERATION);
        return -1;
    }

    switch (cmd) {
    case EVP_PKEY_CTRL_SET1_ID:
        evp_pkey_ctx_free_cached_data(ctx, cmd, name);
        if (name != NULL) {
            ctx->cached_parameters.dist_id_name = OPENSSL_strdup(name);
            if (ctx->cached_parameters.dist_id_name == NULL)
                return 0;
        }
        if (data != NULL) {
            ctx->cached_parameters.dist_id = OPENSSL_memdup(data, data_len);
            if (ctx->cached_parameters.dist_id == NULL)
                return 0;
        }
        ctx->cached_parameters.dist_id_set = 1;
        ctx->cached_parameters.dist_id_len = data_len;
        break;
    }
    return 1;
}

static void evp_pkey_ctx_free_cached_data(EVP_PKEY_CTX *ctx,
    int cmd, const char *name)
{
    cmd = decode_cmd(cmd, name);
    switch (cmd) {
    case EVP_PKEY_CTRL_SET1_ID:
        OPENSSL_free(ctx->cached_parameters.dist_id);
        OPENSSL_free(ctx->cached_parameters.dist_id_name);
        ctx->cached_parameters.dist_id = NULL;
        ctx->cached_parameters.dist_id_name = NULL;
        break;
    }
}

static void evp_pkey_ctx_free_all_cached_data(EVP_PKEY_CTX *ctx)
{
    evp_pkey_ctx_free_cached_data(ctx, EVP_PKEY_CTRL_SET1_ID, NULL);
}

int evp_pkey_ctx_use_cached_data(EVP_PKEY_CTX *ctx)
{
    int ret = 1;

    if (ret && ctx->cached_parameters.dist_id_set) {
        const char *name = ctx->cached_parameters.dist_id_name;
        const void *val = ctx->cached_parameters.dist_id;
        size_t len = ctx->cached_parameters.dist_id_len;

        if (name != NULL)
            ret = evp_pkey_ctx_ctrl_str_int(ctx, name, val);
        else
            ret = evp_pkey_ctx_ctrl_int(ctx, -1, ctx->operation,
                EVP_PKEY_CTRL_SET1_ID,
                (int)len, (void *)val);
    }

    return ret;
}

OSSL_LIB_CTX *EVP_PKEY_CTX_get0_libctx(EVP_PKEY_CTX *ctx)
{
    return ctx->libctx;
}

const char *EVP_PKEY_CTX_get0_propq(const EVP_PKEY_CTX *ctx)
{
    return ctx->propquery;
}

const OSSL_PROVIDER *EVP_PKEY_CTX_get0_provider(const EVP_PKEY_CTX *ctx)
{
    if (EVP_PKEY_CTX_IS_SIGNATURE_OP(ctx)) {
        if (ctx->op.sig.signature != NULL)
            return EVP_SIGNATURE_get0_provider(ctx->op.sig.signature);
    } else if (EVP_PKEY_CTX_IS_DERIVE_OP(ctx)) {
        if (ctx->op.kex.exchange != NULL)
            return EVP_KEYEXCH_get0_provider(ctx->op.kex.exchange);
    } else if (EVP_PKEY_CTX_IS_KEM_OP(ctx)) {
        if (ctx->op.encap.kem != NULL)
            return EVP_KEM_get0_provider(ctx->op.encap.kem);
    } else if (EVP_PKEY_CTX_IS_ASYM_CIPHER_OP(ctx)) {
        if (ctx->op.ciph.cipher != NULL)
            return EVP_ASYM_CIPHER_get0_provider(ctx->op.ciph.cipher);
    } else if (EVP_PKEY_CTX_IS_GEN_OP(ctx)) {
        if (ctx->keymgmt != NULL)
            return EVP_KEYMGMT_get0_provider(ctx->keymgmt);
    }

    return NULL;
}

/* Utility functions to send a string of hex string to a ctrl */

int EVP_PKEY_CTX_str2ctrl(EVP_PKEY_CTX *ctx, int cmd, const char *str)
{
    size_t len;

    len = strlen(str);
    if (len > INT_MAX)
        return -1;
    return EVP_PKEY_CTX_ctrl(ctx, -1, -1, cmd, (int)len, (void *)str);
}

int EVP_PKEY_CTX_hex2ctrl(EVP_PKEY_CTX *ctx, int cmd, const char *hex)
{
    unsigned char *bin;
    long binlen;
    int rv = -1;

    bin = OPENSSL_hexstr2buf(hex, &binlen);
    if (bin == NULL)
        return 0;
    if (binlen <= INT_MAX)
        rv = EVP_PKEY_CTX_ctrl(ctx, -1, -1, cmd, binlen, bin);
    OPENSSL_free(bin);
    return rv;
}

/* Pass a message digest to a ctrl */
int EVP_PKEY_CTX_md(EVP_PKEY_CTX *ctx, int optype, int cmd, const char *md)
{
    const EVP_MD *m;

    if (md == NULL || (m = EVP_get_digestbyname(md)) == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_DIGEST);
        return 0;
    }
    return EVP_PKEY_CTX_ctrl(ctx, -1, optype, cmd, 0, (void *)m);
}

int EVP_PKEY_CTX_get_operation(EVP_PKEY_CTX *ctx)
{
    return ctx->operation;
}

void EVP_PKEY_CTX_set0_keygen_info(EVP_PKEY_CTX *ctx, int *dat, int datlen)
{
    ctx->keygen_info = dat;
    ctx->keygen_info_count = datlen;
}

void EVP_PKEY_CTX_set_data(EVP_PKEY_CTX *ctx, void *data)
{
    ctx->data = data;
}

void *EVP_PKEY_CTX_get_data(const EVP_PKEY_CTX *ctx)
{
    return ctx->data;
}

EVP_PKEY *EVP_PKEY_CTX_get0_pkey(EVP_PKEY_CTX *ctx)
{
    return ctx->pkey;
}

EVP_PKEY *EVP_PKEY_CTX_get0_peerkey(EVP_PKEY_CTX *ctx)
{
    return ctx->peerkey;
}

void EVP_PKEY_CTX_set_app_data(EVP_PKEY_CTX *ctx, void *data)
{
    ctx->app_data = data;
}

void *EVP_PKEY_CTX_get_app_data(EVP_PKEY_CTX *ctx)
{
    return ctx->app_data;
}
#endif /* FIPS_MODULE */
