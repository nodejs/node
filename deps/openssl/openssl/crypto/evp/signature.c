/*
 * Copyright 2006-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include "internal/numbers.h"   /* includes SIZE_MAX */
#include "internal/cryptlib.h"
#include "internal/provider.h"
#include "internal/core.h"
#include "crypto/evp.h"
#include "evp_local.h"

static EVP_SIGNATURE *evp_signature_new(OSSL_PROVIDER *prov)
{
    EVP_SIGNATURE *signature = OPENSSL_zalloc(sizeof(EVP_SIGNATURE));

    if (signature == NULL) {
        ERR_raise(ERR_LIB_EVP, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    signature->lock = CRYPTO_THREAD_lock_new();
    if (signature->lock == NULL) {
        ERR_raise(ERR_LIB_EVP, ERR_R_MALLOC_FAILURE);
        OPENSSL_free(signature);
        return NULL;
    }
    signature->prov = prov;
    ossl_provider_up_ref(prov);
    signature->refcnt = 1;

    return signature;
}

static void *evp_signature_from_algorithm(int name_id,
                                          const OSSL_ALGORITHM *algodef,
                                          OSSL_PROVIDER *prov)
{
    const OSSL_DISPATCH *fns = algodef->implementation;
    EVP_SIGNATURE *signature = NULL;
    int ctxfncnt = 0, signfncnt = 0, verifyfncnt = 0, verifyrecfncnt = 0;
    int digsignfncnt = 0, digverifyfncnt = 0;
    int gparamfncnt = 0, sparamfncnt = 0, gmdparamfncnt = 0, smdparamfncnt = 0;

    if ((signature = evp_signature_new(prov)) == NULL) {
        ERR_raise(ERR_LIB_EVP, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    signature->name_id = name_id;
    if ((signature->type_name = ossl_algorithm_get1_first_name(algodef)) == NULL)
        goto err;
    signature->description = algodef->algorithm_description;

    for (; fns->function_id != 0; fns++) {
        switch (fns->function_id) {
        case OSSL_FUNC_SIGNATURE_NEWCTX:
            if (signature->newctx != NULL)
                break;
            signature->newctx = OSSL_FUNC_signature_newctx(fns);
            ctxfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_SIGN_INIT:
            if (signature->sign_init != NULL)
                break;
            signature->sign_init = OSSL_FUNC_signature_sign_init(fns);
            signfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_SIGN:
            if (signature->sign != NULL)
                break;
            signature->sign = OSSL_FUNC_signature_sign(fns);
            signfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_VERIFY_INIT:
            if (signature->verify_init != NULL)
                break;
            signature->verify_init = OSSL_FUNC_signature_verify_init(fns);
            verifyfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_VERIFY:
            if (signature->verify != NULL)
                break;
            signature->verify = OSSL_FUNC_signature_verify(fns);
            verifyfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_VERIFY_RECOVER_INIT:
            if (signature->verify_recover_init != NULL)
                break;
            signature->verify_recover_init
                = OSSL_FUNC_signature_verify_recover_init(fns);
            verifyrecfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_VERIFY_RECOVER:
            if (signature->verify_recover != NULL)
                break;
            signature->verify_recover
                = OSSL_FUNC_signature_verify_recover(fns);
            verifyrecfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_DIGEST_SIGN_INIT:
            if (signature->digest_sign_init != NULL)
                break;
            signature->digest_sign_init
                = OSSL_FUNC_signature_digest_sign_init(fns);
            break;
        case OSSL_FUNC_SIGNATURE_DIGEST_SIGN_UPDATE:
            if (signature->digest_sign_update != NULL)
                break;
            signature->digest_sign_update
                = OSSL_FUNC_signature_digest_sign_update(fns);
            digsignfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_DIGEST_SIGN_FINAL:
            if (signature->digest_sign_final != NULL)
                break;
            signature->digest_sign_final
                = OSSL_FUNC_signature_digest_sign_final(fns);
            digsignfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_DIGEST_SIGN:
            if (signature->digest_sign != NULL)
                break;
            signature->digest_sign
                = OSSL_FUNC_signature_digest_sign(fns);
            break;
        case OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_INIT:
            if (signature->digest_verify_init != NULL)
                break;
            signature->digest_verify_init
                = OSSL_FUNC_signature_digest_verify_init(fns);
            break;
        case OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_UPDATE:
            if (signature->digest_verify_update != NULL)
                break;
            signature->digest_verify_update
                = OSSL_FUNC_signature_digest_verify_update(fns);
            digverifyfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_FINAL:
            if (signature->digest_verify_final != NULL)
                break;
            signature->digest_verify_final
                = OSSL_FUNC_signature_digest_verify_final(fns);
            digverifyfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_DIGEST_VERIFY:
            if (signature->digest_verify != NULL)
                break;
            signature->digest_verify
                = OSSL_FUNC_signature_digest_verify(fns);
            break;
        case OSSL_FUNC_SIGNATURE_FREECTX:
            if (signature->freectx != NULL)
                break;
            signature->freectx = OSSL_FUNC_signature_freectx(fns);
            ctxfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_DUPCTX:
            if (signature->dupctx != NULL)
                break;
            signature->dupctx = OSSL_FUNC_signature_dupctx(fns);
            break;
        case OSSL_FUNC_SIGNATURE_GET_CTX_PARAMS:
            if (signature->get_ctx_params != NULL)
                break;
            signature->get_ctx_params
                = OSSL_FUNC_signature_get_ctx_params(fns);
            gparamfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_GETTABLE_CTX_PARAMS:
            if (signature->gettable_ctx_params != NULL)
                break;
            signature->gettable_ctx_params
                = OSSL_FUNC_signature_gettable_ctx_params(fns);
            gparamfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_SET_CTX_PARAMS:
            if (signature->set_ctx_params != NULL)
                break;
            signature->set_ctx_params
                = OSSL_FUNC_signature_set_ctx_params(fns);
            sparamfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_SETTABLE_CTX_PARAMS:
            if (signature->settable_ctx_params != NULL)
                break;
            signature->settable_ctx_params
                = OSSL_FUNC_signature_settable_ctx_params(fns);
            sparamfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_GET_CTX_MD_PARAMS:
            if (signature->get_ctx_md_params != NULL)
                break;
            signature->get_ctx_md_params
                = OSSL_FUNC_signature_get_ctx_md_params(fns);
            gmdparamfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_GETTABLE_CTX_MD_PARAMS:
            if (signature->gettable_ctx_md_params != NULL)
                break;
            signature->gettable_ctx_md_params
                = OSSL_FUNC_signature_gettable_ctx_md_params(fns);
            gmdparamfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_SET_CTX_MD_PARAMS:
            if (signature->set_ctx_md_params != NULL)
                break;
            signature->set_ctx_md_params
                = OSSL_FUNC_signature_set_ctx_md_params(fns);
            smdparamfncnt++;
            break;
        case OSSL_FUNC_SIGNATURE_SETTABLE_CTX_MD_PARAMS:
            if (signature->settable_ctx_md_params != NULL)
                break;
            signature->settable_ctx_md_params
                = OSSL_FUNC_signature_settable_ctx_md_params(fns);
            smdparamfncnt++;
            break;
        }
    }
    if (ctxfncnt != 2
        || (signfncnt == 0
            && verifyfncnt == 0
            && verifyrecfncnt == 0
            && digsignfncnt == 0
            && digverifyfncnt == 0
            && signature->digest_sign == NULL
            && signature->digest_verify == NULL)
        || (signfncnt != 0 && signfncnt != 2)
        || (verifyfncnt != 0 && verifyfncnt != 2)
        || (verifyrecfncnt != 0 && verifyrecfncnt != 2)
        || (digsignfncnt != 0 && digsignfncnt != 2)
        || (digsignfncnt == 2 && signature->digest_sign_init == NULL)
        || (digverifyfncnt != 0 && digverifyfncnt != 2)
        || (digverifyfncnt == 2 && signature->digest_verify_init == NULL)
        || (signature->digest_sign != NULL
            && signature->digest_sign_init == NULL)
        || (signature->digest_verify != NULL
            && signature->digest_verify_init == NULL)
        || (gparamfncnt != 0 && gparamfncnt != 2)
        || (sparamfncnt != 0 && sparamfncnt != 2)
        || (gmdparamfncnt != 0 && gmdparamfncnt != 2)
        || (smdparamfncnt != 0 && smdparamfncnt != 2)) {
        /*
         * In order to be a consistent set of functions we must have at least
         * a set of context functions (newctx and freectx) as well as a set of
         * "signature" functions:
         *  (sign_init, sign) or
         *  (verify_init verify) or
         *  (verify_recover_init, verify_recover) or
         *  (digest_sign_init, digest_sign_update, digest_sign_final) or
         *  (digest_verify_init, digest_verify_update, digest_verify_final) or
         *  (digest_sign_init, digest_sign) or
         *  (digest_verify_init, digest_verify).
         *
         * set_ctx_params and settable_ctx_params are optional, but if one of
         * them is present then the other one must also be present. The same
         * applies to get_ctx_params and gettable_ctx_params. The same rules
         * apply to the "md_params" functions. The dupctx function is optional.
         */
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_PROVIDER_FUNCTIONS);
        goto err;
    }

    return signature;
 err:
    EVP_SIGNATURE_free(signature);
    return NULL;
}

void EVP_SIGNATURE_free(EVP_SIGNATURE *signature)
{
    int i;

    if (signature == NULL)
        return;
    CRYPTO_DOWN_REF(&signature->refcnt, &i, signature->lock);
    if (i > 0)
        return;
    OPENSSL_free(signature->type_name);
    ossl_provider_free(signature->prov);
    CRYPTO_THREAD_lock_free(signature->lock);
    OPENSSL_free(signature);
}

int EVP_SIGNATURE_up_ref(EVP_SIGNATURE *signature)
{
    int ref = 0;

    CRYPTO_UP_REF(&signature->refcnt, &ref, signature->lock);
    return 1;
}

OSSL_PROVIDER *EVP_SIGNATURE_get0_provider(const EVP_SIGNATURE *signature)
{
    return signature->prov;
}

EVP_SIGNATURE *EVP_SIGNATURE_fetch(OSSL_LIB_CTX *ctx, const char *algorithm,
                                   const char *properties)
{
    return evp_generic_fetch(ctx, OSSL_OP_SIGNATURE, algorithm, properties,
                             evp_signature_from_algorithm,
                             (int (*)(void *))EVP_SIGNATURE_up_ref,
                             (void (*)(void *))EVP_SIGNATURE_free);
}

EVP_SIGNATURE *evp_signature_fetch_from_prov(OSSL_PROVIDER *prov,
                                             const char *algorithm,
                                             const char *properties)
{
    return evp_generic_fetch_from_prov(prov, OSSL_OP_SIGNATURE,
                                       algorithm, properties,
                                       evp_signature_from_algorithm,
                                       (int (*)(void *))EVP_SIGNATURE_up_ref,
                                       (void (*)(void *))EVP_SIGNATURE_free);
}

int EVP_SIGNATURE_is_a(const EVP_SIGNATURE *signature, const char *name)
{
    return signature != NULL
           && evp_is_a(signature->prov, signature->name_id, NULL, name);
}

int evp_signature_get_number(const EVP_SIGNATURE *signature)
{
    return signature->name_id;
}

const char *EVP_SIGNATURE_get0_name(const EVP_SIGNATURE *signature)
{
    return signature->type_name;
}

const char *EVP_SIGNATURE_get0_description(const EVP_SIGNATURE *signature)
{
    return signature->description;
}

void EVP_SIGNATURE_do_all_provided(OSSL_LIB_CTX *libctx,
                                   void (*fn)(EVP_SIGNATURE *signature,
                                              void *arg),
                                   void *arg)
{
    evp_generic_do_all(libctx, OSSL_OP_SIGNATURE,
                       (void (*)(void *, void *))fn, arg,
                       evp_signature_from_algorithm,
                       (int (*)(void *))EVP_SIGNATURE_up_ref,
                       (void (*)(void *))EVP_SIGNATURE_free);
}


int EVP_SIGNATURE_names_do_all(const EVP_SIGNATURE *signature,
                               void (*fn)(const char *name, void *data),
                               void *data)
{
    if (signature->prov != NULL)
        return evp_names_do_all(signature->prov, signature->name_id, fn, data);

    return 1;
}

const OSSL_PARAM *EVP_SIGNATURE_gettable_ctx_params(const EVP_SIGNATURE *sig)
{
    void *provctx;

    if (sig == NULL || sig->gettable_ctx_params == NULL)
        return NULL;

    provctx = ossl_provider_ctx(EVP_SIGNATURE_get0_provider(sig));
    return sig->gettable_ctx_params(NULL, provctx);
}

const OSSL_PARAM *EVP_SIGNATURE_settable_ctx_params(const EVP_SIGNATURE *sig)
{
    void *provctx;

    if (sig == NULL || sig->settable_ctx_params == NULL)
        return NULL;

    provctx = ossl_provider_ctx(EVP_SIGNATURE_get0_provider(sig));
    return sig->settable_ctx_params(NULL, provctx);
}

static int evp_pkey_signature_init(EVP_PKEY_CTX *ctx, int operation,
                                   const OSSL_PARAM params[])
{
    int ret = 0;
    void *provkey = NULL;
    EVP_SIGNATURE *signature = NULL;
    EVP_KEYMGMT *tmp_keymgmt = NULL;
    const OSSL_PROVIDER *tmp_prov = NULL;
    const char *supported_sig = NULL;
    int iter;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return -1;
    }

    evp_pkey_ctx_free_old_ops(ctx);
    ctx->operation = operation;

    ERR_set_mark();

    if (evp_pkey_ctx_is_legacy(ctx))
        goto legacy;

    if (ctx->pkey == NULL) {
        ERR_clear_last_mark();
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_KEY_SET);
        goto err;
    }

    /*
     * Try to derive the supported signature from |ctx->keymgmt|.
     */
    if (!ossl_assert(ctx->pkey->keymgmt == NULL
                     || ctx->pkey->keymgmt == ctx->keymgmt)) {
        ERR_clear_last_mark();
        ERR_raise(ERR_LIB_EVP, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    supported_sig = evp_keymgmt_util_query_operation_name(ctx->keymgmt,
                                                          OSSL_OP_SIGNATURE);
    if (supported_sig == NULL) {
        ERR_clear_last_mark();
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        goto err;
    }

    /*
     * We perform two iterations:
     *
     * 1.  Do the normal signature fetch, using the fetching data given by
     *     the EVP_PKEY_CTX.
     * 2.  Do the provider specific signature fetch, from the same provider
     *     as |ctx->keymgmt|
     *
     * We then try to fetch the keymgmt from the same provider as the
     * signature, and try to export |ctx->pkey| to that keymgmt (when
     * this keymgmt happens to be the same as |ctx->keymgmt|, the export
     * is a no-op, but we call it anyway to not complicate the code even
     * more).
     * If the export call succeeds (returns a non-NULL provider key pointer),
     * we're done and can perform the operation itself.  If not, we perform
     * the second iteration, or jump to legacy.
     */
    for (iter = 1; iter < 3 && provkey == NULL; iter++) {
        EVP_KEYMGMT *tmp_keymgmt_tofree = NULL;

        /*
         * If we're on the second iteration, free the results from the first.
         * They are NULL on the first iteration, so no need to check what
         * iteration we're on.
         */
        EVP_SIGNATURE_free(signature);
        EVP_KEYMGMT_free(tmp_keymgmt);

        switch (iter) {
        case 1:
            signature =
                EVP_SIGNATURE_fetch(ctx->libctx, supported_sig, ctx->propquery);
            if (signature != NULL)
                tmp_prov = EVP_SIGNATURE_get0_provider(signature);
            break;
        case 2:
            tmp_prov = EVP_KEYMGMT_get0_provider(ctx->keymgmt);
            signature =
                evp_signature_fetch_from_prov((OSSL_PROVIDER *)tmp_prov,
                                              supported_sig, ctx->propquery);
            if (signature == NULL)
                goto legacy;
            break;
        }
        if (signature == NULL)
            continue;

        /*
         * Ensure that the key is provided, either natively, or as a cached
         * export.  We start by fetching the keymgmt with the same name as
         * |ctx->pkey|, but from the provider of the signature method, using
         * the same property query as when fetching the signature method.
         * With the keymgmt we found (if we did), we try to export |ctx->pkey|
         * to it (evp_pkey_export_to_provider() is smart enough to only actually

         * export it if |tmp_keymgmt| is different from |ctx->pkey|'s keymgmt)
         */
        tmp_keymgmt_tofree = tmp_keymgmt =
            evp_keymgmt_fetch_from_prov((OSSL_PROVIDER *)tmp_prov,
                                        EVP_KEYMGMT_get0_name(ctx->keymgmt),
                                        ctx->propquery);
        if (tmp_keymgmt != NULL)
            provkey = evp_pkey_export_to_provider(ctx->pkey, ctx->libctx,
                                                  &tmp_keymgmt, ctx->propquery);
        if (tmp_keymgmt == NULL)
            EVP_KEYMGMT_free(tmp_keymgmt_tofree);
    }

    if (provkey == NULL) {
        EVP_SIGNATURE_free(signature);
        goto legacy;
    }

    ERR_pop_to_mark();

    /* No more legacy from here down to legacy: */

    ctx->op.sig.signature = signature;
    ctx->op.sig.algctx =
        signature->newctx(ossl_provider_ctx(signature->prov), ctx->propquery);
    if (ctx->op.sig.algctx == NULL) {
        /* The provider key can stay in the cache */
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        goto err;
    }

    switch (operation) {
    case EVP_PKEY_OP_SIGN:
        if (signature->sign_init == NULL) {
            ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
            ret = -2;
            goto err;
        }
        ret = signature->sign_init(ctx->op.sig.algctx, provkey, params);
        break;
    case EVP_PKEY_OP_VERIFY:
        if (signature->verify_init == NULL) {
            ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
            ret = -2;
            goto err;
        }
        ret = signature->verify_init(ctx->op.sig.algctx, provkey, params);
        break;
    case EVP_PKEY_OP_VERIFYRECOVER:
        if (signature->verify_recover_init == NULL) {
            ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
            ret = -2;
            goto err;
        }
        ret = signature->verify_recover_init(ctx->op.sig.algctx, provkey,
                                             params);
        break;
    default:
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        goto err;
    }

    if (ret <= 0) {
        signature->freectx(ctx->op.sig.algctx);
        ctx->op.sig.algctx = NULL;
        goto err;
    }
    goto end;

 legacy:
    /*
     * If we don't have the full support we need with provided methods,
     * let's go see if legacy does.
     */
    ERR_pop_to_mark();
    EVP_KEYMGMT_free(tmp_keymgmt);
    tmp_keymgmt = NULL;

    if (ctx->pmeth == NULL
            || (operation == EVP_PKEY_OP_SIGN && ctx->pmeth->sign == NULL)
            || (operation == EVP_PKEY_OP_VERIFY && ctx->pmeth->verify == NULL)
            || (operation == EVP_PKEY_OP_VERIFYRECOVER
                && ctx->pmeth->verify_recover == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
        return -2;
    }

    switch (operation) {
    case EVP_PKEY_OP_SIGN:
        if (ctx->pmeth->sign_init == NULL)
            return 1;
        ret = ctx->pmeth->sign_init(ctx);
        break;
    case EVP_PKEY_OP_VERIFY:
        if (ctx->pmeth->verify_init == NULL)
            return 1;
        ret = ctx->pmeth->verify_init(ctx);
        break;
    case EVP_PKEY_OP_VERIFYRECOVER:
        if (ctx->pmeth->verify_recover_init == NULL)
            return 1;
        ret = ctx->pmeth->verify_recover_init(ctx);
        break;
    default:
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        goto err;
    }
    if (ret <= 0)
        goto err;
 end:
#ifndef FIPS_MODULE
    if (ret > 0)
        ret = evp_pkey_ctx_use_cached_data(ctx);
#endif

    EVP_KEYMGMT_free(tmp_keymgmt);
    return ret;
 err:
    evp_pkey_ctx_free_old_ops(ctx);
    ctx->operation = EVP_PKEY_OP_UNDEFINED;
    EVP_KEYMGMT_free(tmp_keymgmt);
    return ret;
}

int EVP_PKEY_sign_init(EVP_PKEY_CTX *ctx)
{
    return evp_pkey_signature_init(ctx, EVP_PKEY_OP_SIGN, NULL);
}

int EVP_PKEY_sign_init_ex(EVP_PKEY_CTX *ctx, const OSSL_PARAM params[])
{
    return evp_pkey_signature_init(ctx, EVP_PKEY_OP_SIGN, params);
}

int EVP_PKEY_sign(EVP_PKEY_CTX *ctx,
                  unsigned char *sig, size_t *siglen,
                  const unsigned char *tbs, size_t tbslen)
{
    int ret;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return -1;
    }

    if (ctx->operation != EVP_PKEY_OP_SIGN) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_INITIALIZED);
        return -1;
    }

    if (ctx->op.sig.algctx == NULL)
        goto legacy;

    if (ctx->op.sig.signature->sign == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
        return -2;
    }

    ret = ctx->op.sig.signature->sign(ctx->op.sig.algctx, sig, siglen,
                                      (sig == NULL) ? 0 : *siglen, tbs, tbslen);

    return ret;
 legacy:

    if (ctx->pmeth == NULL || ctx->pmeth->sign == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
        return -2;
    }

    M_check_autoarg(ctx, sig, siglen, EVP_F_EVP_PKEY_SIGN)
        return ctx->pmeth->sign(ctx, sig, siglen, tbs, tbslen);
}

int EVP_PKEY_verify_init(EVP_PKEY_CTX *ctx)
{
    return evp_pkey_signature_init(ctx, EVP_PKEY_OP_VERIFY, NULL);
}

int EVP_PKEY_verify_init_ex(EVP_PKEY_CTX *ctx, const OSSL_PARAM params[])
{
    return evp_pkey_signature_init(ctx, EVP_PKEY_OP_VERIFY, params);
}

int EVP_PKEY_verify(EVP_PKEY_CTX *ctx,
                    const unsigned char *sig, size_t siglen,
                    const unsigned char *tbs, size_t tbslen)
{
    int ret;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return -1;
    }

    if (ctx->operation != EVP_PKEY_OP_VERIFY) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_INITIALIZED);
        return -1;
    }

    if (ctx->op.sig.algctx == NULL)
        goto legacy;

    if (ctx->op.sig.signature->verify == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
        return -2;
    }

    ret = ctx->op.sig.signature->verify(ctx->op.sig.algctx, sig, siglen,
                                        tbs, tbslen);

    return ret;
 legacy:
    if (ctx->pmeth == NULL || ctx->pmeth->verify == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
        return -2;
    }

    return ctx->pmeth->verify(ctx, sig, siglen, tbs, tbslen);
}

int EVP_PKEY_verify_recover_init(EVP_PKEY_CTX *ctx)
{
    return evp_pkey_signature_init(ctx, EVP_PKEY_OP_VERIFYRECOVER, NULL);
}

int EVP_PKEY_verify_recover_init_ex(EVP_PKEY_CTX *ctx,
                                    const OSSL_PARAM params[])
{
    return evp_pkey_signature_init(ctx, EVP_PKEY_OP_VERIFYRECOVER, params);
}

int EVP_PKEY_verify_recover(EVP_PKEY_CTX *ctx,
                            unsigned char *rout, size_t *routlen,
                            const unsigned char *sig, size_t siglen)
{
    int ret;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return -1;
    }

    if (ctx->operation != EVP_PKEY_OP_VERIFYRECOVER) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_INITIALIZED);
        return -1;
    }

    if (ctx->op.sig.algctx == NULL)
        goto legacy;

    if (ctx->op.sig.signature->verify_recover == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
        return -2;
    }

    ret = ctx->op.sig.signature->verify_recover(ctx->op.sig.algctx, rout,
                                                routlen,
                                                (rout == NULL ? 0 : *routlen),
                                                sig, siglen);
    return ret;
 legacy:
    if (ctx->pmeth == NULL || ctx->pmeth->verify_recover == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
        return -2;
    }
    M_check_autoarg(ctx, rout, routlen, EVP_F_EVP_PKEY_VERIFY_RECOVER)
        return ctx->pmeth->verify_recover(ctx, rout, routlen, sig, siglen);
}
