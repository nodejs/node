/*
 * Copyright 2006-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/evp.h>
#include <openssl/objects.h>
#include "crypto/evp.h"
#include "internal/provider.h"
#include "internal/numbers.h" /* includes SIZE_MAX */
#include "internal/common.h"
#include "evp_local.h"

/*
 * If we get the "NULL" md then the name comes back as "UNDEF". We want to use
 * NULL for this.
 */
static const char *canon_mdname(const char *mdname)
{
    if (mdname != NULL && strcmp(mdname, "UNDEF") == 0)
        return NULL;

    return mdname;
}

static int do_sigver_init(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
    const EVP_MD *type, const char *mdname,
    OSSL_LIB_CTX *libctx, const char *props,
    EVP_PKEY *pkey, int ver,
    const OSSL_PARAM params[])
{
    EVP_PKEY_CTX *locpctx = NULL;
    EVP_SIGNATURE *signature = NULL;
    const char *desc;
    EVP_KEYMGMT *tmp_keymgmt = NULL;
    const OSSL_PROVIDER *tmp_prov = NULL;
    const char *supported_sig = NULL;
    char locmdname[80] = ""; /* 80 chars should be enough */
    void *provkey = NULL;
    int ret, iter, reinit = 1;

    if (!evp_md_ctx_free_algctx(ctx))
        return 0;

    if (ctx->pctx == NULL) {
        reinit = 0;
        ctx->pctx = EVP_PKEY_CTX_new_from_pkey(libctx, pkey, props);
    }
    if (ctx->pctx == NULL)
        return 0;

    EVP_MD_CTX_clear_flags(ctx, EVP_MD_CTX_FLAG_FINALISED);

    locpctx = ctx->pctx;
    ERR_set_mark();

    if (evp_pkey_ctx_is_legacy(locpctx))
        goto notsupported;

    /* do not reinitialize if pkey is set or operation is different */
    if (reinit
        && (pkey != NULL
            || locpctx->operation != (ver ? EVP_PKEY_OP_VERIFYCTX : EVP_PKEY_OP_SIGNCTX)
            || (signature = locpctx->op.sig.signature) == NULL
            || locpctx->op.sig.algctx == NULL))
        reinit = 0;

    if (props == NULL)
        props = locpctx->propquery;

    if (locpctx->pkey == NULL) {
        ERR_clear_last_mark();
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_KEY_SET);
        goto err;
    }

    if (!reinit) {
        evp_pkey_ctx_free_old_ops(locpctx);
    } else {
        if (mdname == NULL && type == NULL)
            mdname = canon_mdname(EVP_MD_get0_name(ctx->reqdigest));
        goto reinitialize;
    }

    /*
     * Try to derive the supported signature from |locpctx->keymgmt|.
     */
    if (!ossl_assert(locpctx->pkey->keymgmt == NULL
            || locpctx->pkey->keymgmt == locpctx->keymgmt)) {
        ERR_clear_last_mark();
        ERR_raise(ERR_LIB_EVP, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    supported_sig = evp_keymgmt_util_query_operation_name(locpctx->keymgmt,
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
    for (iter = 1, provkey = NULL; iter < 3 && provkey == NULL; iter++) {
        EVP_KEYMGMT *tmp_keymgmt_tofree = NULL;

        /*
         * If we're on the second iteration, free the results from the first.
         * They are NULL on the first iteration, so no need to check what
         * iteration we're on.
         */
        EVP_SIGNATURE_free(signature);
        signature = NULL;
        EVP_KEYMGMT_free(tmp_keymgmt);
        tmp_keymgmt = NULL;

        switch (iter) {
        case 1:
            signature = EVP_SIGNATURE_fetch(locpctx->libctx, supported_sig,
                locpctx->propquery);
            if (signature != NULL)
                tmp_prov = EVP_SIGNATURE_get0_provider(signature);
            break;
        case 2:
            tmp_prov = EVP_KEYMGMT_get0_provider(locpctx->keymgmt);
            signature = evp_signature_fetch_from_prov((OSSL_PROVIDER *)tmp_prov,
                supported_sig, locpctx->propquery);
            if (signature == NULL)
                goto notsupported;
            break;
        }
        if (signature == NULL)
            continue;

        /*
         * Ensure that the key is provided, either natively, or as a cached
         * export.  We start by fetching the keymgmt with the same name as
         * |locpctx->pkey|, but from the provider of the signature method, using
         * the same property query as when fetching the signature method.
         * With the keymgmt we found (if we did), we try to export |locpctx->pkey|
         * to it (evp_pkey_export_to_provider() is smart enough to only actually

         * export it if |tmp_keymgmt| is different from |locpctx->pkey|'s keymgmt)
         */
        tmp_keymgmt_tofree = tmp_keymgmt = evp_keymgmt_fetch_from_prov((OSSL_PROVIDER *)tmp_prov,
            EVP_KEYMGMT_get0_name(locpctx->keymgmt),
            locpctx->propquery);
        if (tmp_keymgmt != NULL)
            provkey = evp_pkey_export_to_provider(locpctx->pkey, locpctx->libctx,
                &tmp_keymgmt, locpctx->propquery);
        if (tmp_keymgmt == NULL)
            EVP_KEYMGMT_free(tmp_keymgmt_tofree);
    }

    if (provkey == NULL) {
        EVP_SIGNATURE_free(signature);
        ERR_clear_last_mark();
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        goto err;
    }

    ERR_pop_to_mark();

    /* No more legacy from here down to legacy: */

    locpctx->op.sig.signature = signature;
    locpctx->operation = ver ? EVP_PKEY_OP_VERIFYCTX
                             : EVP_PKEY_OP_SIGNCTX;
    locpctx->op.sig.algctx
        = signature->newctx(ossl_provider_ctx(signature->prov), props);
    if (locpctx->op.sig.algctx == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        goto err;
    }

reinitialize:
    if (pctx != NULL)
        *pctx = locpctx;

    if (type != NULL) {
        ctx->reqdigest = type;
        if (mdname == NULL)
            mdname = canon_mdname(EVP_MD_get0_name(type));
    } else {
        if (mdname == NULL && !reinit) {
            if (evp_keymgmt_util_get_deflt_digest_name(tmp_keymgmt, provkey,
                    locmdname,
                    sizeof(locmdname))
                > 0) {
                mdname = canon_mdname(locmdname);
            }
        }

        if (mdname != NULL) {
            /*
             * We're about to get a new digest so clear anything associated with
             * an old digest.
             */
            evp_md_ctx_clear_digest(ctx, 1, 0);

            /*
             * This might be requested by a later call to EVP_MD_CTX_get0_md().
             * In that case the "explicit fetch" rules apply for that
             * function (as per man pages), i.e. the ref count is not updated
             * so the EVP_MD should not be used beyond the lifetime of the
             * EVP_MD_CTX.
             */
            ctx->fetched_digest = EVP_MD_fetch(locpctx->libctx, mdname, props);
            if (ctx->fetched_digest != NULL) {
                ctx->digest = ctx->reqdigest = ctx->fetched_digest;
                if (ctx->digest == NULL) {
                    ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
                    goto err;
                }
            }
        }
    }

    desc = signature->description != NULL ? signature->description : "";
    if (ver) {
        if (signature->digest_verify_init == NULL) {
            ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_NOT_SUPPORTED,
                "%s digest_verify_init:%s", signature->type_name, desc);
            goto err;
        }
        ret = signature->digest_verify_init(locpctx->op.sig.algctx,
            mdname, provkey, params);
    } else {
        if (signature->digest_sign_init == NULL) {
            ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_NOT_SUPPORTED,
                "%s digest_sign_init:%s", signature->type_name, desc);
            goto err;
        }
        ret = signature->digest_sign_init(locpctx->op.sig.algctx,
            mdname, provkey, params);
    }

    /*
     * If the operation was not a success and no digest was found, an error
     * needs to be raised.
     */
    if (ret > 0 || mdname != NULL) {
        if (ret > 0)
            ret = evp_pkey_ctx_use_cached_data(locpctx);

        EVP_KEYMGMT_free(tmp_keymgmt);
        return ret > 0 ? 1 : 0;
    }
    if (type == NULL) /* This check is redundant but clarifies matters */
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_DEFAULT_DIGEST);
    ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_FAILURE,
        ver ? "%s digest_verify_init:%s" : "%s digest_sign_init:%s",
        signature->type_name, desc);

err:
    evp_pkey_ctx_free_old_ops(locpctx);
    locpctx->operation = EVP_PKEY_OP_UNDEFINED;
    EVP_KEYMGMT_free(tmp_keymgmt);
    return 0;

notsupported:
    ERR_pop_to_mark();
    EVP_KEYMGMT_free(tmp_keymgmt);

    ERR_raise_data(ERR_LIB_EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE,
        ver ? "%s digest_verify_init" : "%s digest_sign_init",
        EVP_PKEY_get0_type_name(locpctx->pkey));
    return 0;
}

int EVP_DigestSignInit_ex(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
    const char *mdname, OSSL_LIB_CTX *libctx,
    const char *props, EVP_PKEY *pkey,
    const OSSL_PARAM params[])
{
    return do_sigver_init(ctx, pctx, NULL, mdname, libctx, props, pkey, 0,
        params);
}

int EVP_DigestSignInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
    const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey)
{
    if (!ossl_assert(e == NULL))
        return 0;
    return do_sigver_init(ctx, pctx, type, NULL, NULL, NULL, pkey, 0,
        NULL);
}

int EVP_DigestVerifyInit_ex(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
    const char *mdname, OSSL_LIB_CTX *libctx,
    const char *props, EVP_PKEY *pkey,
    const OSSL_PARAM params[])
{
    return do_sigver_init(ctx, pctx, NULL, mdname, libctx, props, pkey, 1,
        params);
}

int EVP_DigestVerifyInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
    const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey)
{
    if (!ossl_assert(e == NULL))
        return 0;
    return do_sigver_init(ctx, pctx, type, NULL, NULL, NULL, pkey, 1,
        NULL);
}

int EVP_DigestSignUpdate(EVP_MD_CTX *ctx, const void *data, size_t dsize)
{
    EVP_SIGNATURE *signature;
    const char *desc;
    EVP_PKEY_CTX *pctx = ctx->pctx;
    int ret;

    if ((ctx->flags & EVP_MD_CTX_FLAG_FINALISED) != 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_UPDATE_ERROR);
        return 0;
    }

    if (pctx == NULL)
        return EVP_DigestUpdate(ctx, data, dsize);

    if (pctx->operation != EVP_PKEY_OP_SIGNCTX
        || pctx->op.sig.algctx == NULL
        || pctx->op.sig.signature == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    signature = pctx->op.sig.signature;
    desc = signature->description != NULL ? signature->description : "";
    if (signature->digest_sign_update == NULL) {
        ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_NOT_SUPPORTED,
            "%s digest_sign_update:%s", signature->type_name, desc);
        return 0;
    }

    ERR_set_mark();
    ret = signature->digest_sign_update(pctx->op.sig.algctx, data, dsize);
    if (ret <= 0 && ERR_count_to_mark() == 0)
        ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_FAILURE,
            "%s digest_sign_update:%s", signature->type_name, desc);
    ERR_clear_last_mark();
    return ret;
}

int EVP_DigestVerifyUpdate(EVP_MD_CTX *ctx, const void *data, size_t dsize)
{
    EVP_SIGNATURE *signature;
    const char *desc;
    EVP_PKEY_CTX *pctx = ctx->pctx;
    int ret;

    if ((ctx->flags & EVP_MD_CTX_FLAG_FINALISED) != 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_UPDATE_ERROR);
        return 0;
    }

    if (pctx == NULL
        || pctx->operation != EVP_PKEY_OP_VERIFYCTX
        || pctx->op.sig.algctx == NULL
        || pctx->op.sig.signature == NULL)
        return EVP_DigestUpdate(ctx, data, dsize);

    signature = pctx->op.sig.signature;
    desc = signature->description != NULL ? signature->description : "";
    if (signature->digest_verify_update == NULL) {
        ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_NOT_SUPPORTED,
            "%s digest_verify_update:%s", signature->type_name, desc);
        return 0;
    }

    ERR_set_mark();
    ret = signature->digest_verify_update(pctx->op.sig.algctx, data, dsize);
    if (ret <= 0 && ERR_count_to_mark() == 0)
        ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_FAILURE,
            "%s digest_verify_update:%s", signature->type_name, desc);
    ERR_clear_last_mark();
    return ret;
}

int EVP_DigestSignFinal(EVP_MD_CTX *ctx, unsigned char *sigret,
    size_t *siglen)
{
    EVP_SIGNATURE *signature;
    const char *desc;
    int r = 0;
    EVP_PKEY_CTX *dctx = NULL, *pctx = ctx->pctx;

    if ((ctx->flags & EVP_MD_CTX_FLAG_FINALISED) != 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_FINAL_ERROR);
        return 0;
    }

    if (pctx == NULL
        || pctx->operation != EVP_PKEY_OP_SIGNCTX
        || pctx->op.sig.algctx == NULL
        || pctx->op.sig.signature == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    signature = pctx->op.sig.signature;
    desc = signature->description != NULL ? signature->description : "";
    if (signature->digest_sign_final == NULL) {
        ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_NOT_SUPPORTED,
            "%s digest_sign_final:%s", signature->type_name, desc);
        return 0;
    }

    if (sigret != NULL && (ctx->flags & EVP_MD_CTX_FLAG_FINALISE) == 0) {
        /* try dup */
        dctx = EVP_PKEY_CTX_dup(pctx);
        if (dctx != NULL)
            pctx = dctx;
    }

    ERR_set_mark();
    r = signature->digest_sign_final(pctx->op.sig.algctx, sigret, siglen,
        sigret == NULL ? 0 : *siglen);
    if (!r && ERR_count_to_mark() == 0)
        ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_FAILURE,
            "%s digest_sign_final:%s", signature->type_name, desc);
    ERR_clear_last_mark();
    if (dctx == NULL && sigret != NULL)
        ctx->flags |= EVP_MD_CTX_FLAG_FINALISED;
    else
        EVP_PKEY_CTX_free(dctx);
    return r;
}

int EVP_DigestSign(EVP_MD_CTX *ctx, unsigned char *sigret, size_t *siglen,
    const unsigned char *tbs, size_t tbslen)
{
    EVP_PKEY_CTX *pctx = ctx->pctx;
    int ret;

    if (pctx == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    if ((ctx->flags & EVP_MD_CTX_FLAG_FINALISED) != 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_FINAL_ERROR);
        return 0;
    }

    if (pctx->operation == EVP_PKEY_OP_SIGNCTX
        && pctx->op.sig.algctx != NULL
        && pctx->op.sig.signature != NULL) {
        EVP_SIGNATURE *signature = pctx->op.sig.signature;

        if (signature->digest_sign != NULL) {
            const char *desc = signature->description != NULL ? signature->description : "";

            if (sigret != NULL)
                ctx->flags |= EVP_MD_CTX_FLAG_FINALISED;
            ERR_set_mark();
            ret = signature->digest_sign(pctx->op.sig.algctx, sigret, siglen,
                sigret == NULL ? 0 : *siglen, tbs, tbslen);
            if (ret <= 0 && ERR_count_to_mark() == 0)
                ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_FAILURE,
                    "%s digest_sign:%s", signature->type_name, desc);
            ERR_clear_last_mark();
            return ret;
        }
    }

    if (sigret != NULL && EVP_DigestSignUpdate(ctx, tbs, tbslen) <= 0)
        return 0;
    return EVP_DigestSignFinal(ctx, sigret, siglen);
}

int EVP_DigestVerifyFinal(EVP_MD_CTX *ctx, const unsigned char *sig,
    size_t siglen)
{
    EVP_SIGNATURE *signature;
    const char *desc;
    int r = 0;
    EVP_PKEY_CTX *dctx = NULL, *pctx = ctx->pctx;

    if ((ctx->flags & EVP_MD_CTX_FLAG_FINALISED) != 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_FINAL_ERROR);
        return 0;
    }

    if (pctx == NULL
        || pctx->operation != EVP_PKEY_OP_VERIFYCTX
        || pctx->op.sig.algctx == NULL
        || pctx->op.sig.signature == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    signature = pctx->op.sig.signature;
    desc = signature->description != NULL ? signature->description : "";
    if (signature->digest_verify_final == NULL) {
        ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_NOT_SUPPORTED,
            "%s digest_verify_final:%s", signature->type_name, desc);
        return 0;
    }

    if ((ctx->flags & EVP_MD_CTX_FLAG_FINALISE) == 0) {
        /* try dup */
        dctx = EVP_PKEY_CTX_dup(pctx);
        if (dctx != NULL)
            pctx = dctx;
    }

    ERR_set_mark();
    r = signature->digest_verify_final(pctx->op.sig.algctx, sig, siglen);
    if (!r && ERR_count_to_mark() == 0)
        ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_FAILURE,
            "%s digest_verify_final:%s", signature->type_name, desc);
    ERR_clear_last_mark();
    if (dctx == NULL)
        ctx->flags |= EVP_MD_CTX_FLAG_FINALISED;
    else
        EVP_PKEY_CTX_free(dctx);
    return r;
}

int EVP_DigestVerify(EVP_MD_CTX *ctx, const unsigned char *sigret,
    size_t siglen, const unsigned char *tbs, size_t tbslen)
{
    EVP_PKEY_CTX *pctx = ctx->pctx;

    if (pctx == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return -1;
    }

    if ((ctx->flags & EVP_MD_CTX_FLAG_FINALISED) != 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_FINAL_ERROR);
        return 0;
    }

    if (pctx->operation == EVP_PKEY_OP_VERIFYCTX
        && pctx->op.sig.algctx != NULL
        && pctx->op.sig.signature != NULL) {
        if (pctx->op.sig.signature->digest_verify != NULL) {
            EVP_SIGNATURE *signature = pctx->op.sig.signature;
            const char *desc = signature->description != NULL ? signature->description : "";
            int ret;

            ctx->flags |= EVP_MD_CTX_FLAG_FINALISED;
            ERR_set_mark();
            ret = signature->digest_verify(pctx->op.sig.algctx, sigret, siglen, tbs, tbslen);
            if (ret <= 0 && ERR_count_to_mark() == 0)
                ERR_raise_data(ERR_LIB_EVP, EVP_R_PROVIDER_SIGNATURE_FAILURE,
                    "%s digest_verify:%s", signature->type_name, desc);
            ERR_clear_last_mark();
            return ret;
        }
    }

    if (EVP_DigestVerifyUpdate(ctx, tbs, tbslen) <= 0)
        return -1;
    return EVP_DigestVerifyFinal(ctx, sigret, siglen);
}
