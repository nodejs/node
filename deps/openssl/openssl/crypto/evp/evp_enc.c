/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include "internal/cryptlib.h"
#include "internal/provider.h"
#include "internal/core.h"
#include "internal/common.h"
#include "internal/safe_math.h"
#include "crypto/evp.h"
#include "evp_local.h"

OSSL_SAFE_MATH_SIGNED(int, int)

int EVP_CIPHER_CTX_reset(EVP_CIPHER_CTX *ctx)
{
    if (ctx == NULL)
        return 1;

    if (ctx->cipher == NULL || ctx->cipher->prov == NULL)
        return 1;

    if (ctx->algctx != NULL) {
        if (ctx->cipher->freectx != NULL)
            ctx->cipher->freectx(ctx->algctx);
        ctx->algctx = NULL;
    }
    if (ctx->fetched_cipher != NULL)
        EVP_CIPHER_free(ctx->fetched_cipher);
    memset(ctx, 0, sizeof(*ctx));
    ctx->iv_len = -1;

    return 1;
}

EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void)
{
    EVP_CIPHER_CTX *ctx;

    ctx = OPENSSL_zalloc(sizeof(EVP_CIPHER_CTX));
    if (ctx == NULL)
        return NULL;

    ctx->iv_len = -1;
    return ctx;
}

void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx)
{
    if (ctx == NULL)
        return;
    EVP_CIPHER_CTX_reset(ctx);
    OPENSSL_free(ctx);
}

static int evp_cipher_init_internal(EVP_CIPHER_CTX *ctx,
    const EVP_CIPHER *cipher,
    const unsigned char *key,
    const unsigned char *iv, int enc,
    uint8_t is_pipeline,
    const OSSL_PARAM params[])
{
    /*
     * enc == 1 means we are encrypting.
     * enc == 0 means we are decrypting.
     * enc == -1 means, use the previously initialised value for encrypt/decrypt
     */
    if (enc == -1) {
        enc = ctx->encrypt;
    } else {
        if (enc)
            enc = 1;
        ctx->encrypt = enc;
    }

    if (cipher == NULL && ctx->cipher == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    /* Ensure a context left lying around from last time is cleared */
    if (cipher != NULL && ctx->cipher != NULL) {
        unsigned long flags = ctx->flags;

        EVP_CIPHER_CTX_reset(ctx);
        /* Restore encrypt and flags */
        ctx->encrypt = enc;
        ctx->flags = flags;
    }

    if (cipher == NULL)
        cipher = ctx->cipher;

    if (cipher->prov == NULL) {
#ifdef FIPS_MODULE
        /* We only do explicit fetches inside the FIPS module */
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
#else
        EVP_CIPHER *provciph = EVP_CIPHER_fetch(NULL,
            cipher->nid == NID_undef ? "NULL"
                                     : OBJ_nid2sn(cipher->nid),
            "");

        if (provciph == NULL)
            return 0;
        cipher = provciph;
        EVP_CIPHER_free(ctx->fetched_cipher);
        ctx->fetched_cipher = provciph;
#endif
    }

    if (!ossl_assert(cipher->prov != NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    if (cipher != ctx->fetched_cipher) {
        if (!EVP_CIPHER_up_ref((EVP_CIPHER *)cipher)) {
            ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
            return 0;
        }
        EVP_CIPHER_free(ctx->fetched_cipher);
        /* Coverity false positive, the reference counting is confusing it */
        /* coverity[use_after_free] */
        ctx->fetched_cipher = (EVP_CIPHER *)cipher;
    }
    ctx->cipher = cipher;

    if (is_pipeline && !EVP_CIPHER_can_pipeline(cipher, enc)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_PIPELINE_NOT_SUPPORTED);
        return 0;
    }

    if (ctx->algctx == NULL) {
        ctx->algctx = ctx->cipher->newctx(ossl_provider_ctx(cipher->prov));
        if (ctx->algctx == NULL) {
            ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
            return 0;
        }
    }

    if ((ctx->flags & EVP_CIPH_NO_PADDING) != 0) {
        /*
         * If this ctx was already set up for no padding then we need to tell
         * the new cipher about it.
         */
        if (!EVP_CIPHER_CTX_set_padding(ctx, 0))
            return 0;
    }

#ifndef FIPS_MODULE
    /*
     * Fix for CVE-2023-5363
     * Passing in a size as part of the init call takes effect late
     * so, force such to occur before the initialisation.
     *
     * The FIPS provider's internal library context is used in a manner
     * such that this is not an issue.
     */
    if (params != NULL) {
        OSSL_PARAM param_lens[3] = { OSSL_PARAM_END, OSSL_PARAM_END,
            OSSL_PARAM_END };
        OSSL_PARAM *q = param_lens;
        const OSSL_PARAM *p;

        p = OSSL_PARAM_locate_const(params, OSSL_CIPHER_PARAM_KEYLEN);
        if (p != NULL)
            memcpy(q++, p, sizeof(*q));

        /*
         * Note that OSSL_CIPHER_PARAM_AEAD_IVLEN is a synonym for
         * OSSL_CIPHER_PARAM_IVLEN so both are covered here.
         */
        p = OSSL_PARAM_locate_const(params, OSSL_CIPHER_PARAM_IVLEN);
        if (p != NULL)
            memcpy(q++, p, sizeof(*q));

        if (q != param_lens) {
            if (!EVP_CIPHER_CTX_set_params(ctx, param_lens)) {
                ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_LENGTH);
                return 0;
            }
        }
    }
#endif

    if (is_pipeline)
        return 1;

    if (enc) {
        if (ctx->cipher->einit == NULL) {
            /*
             * We still should be able to set the IV using the new API
             * if the key is not specified and old API is not available
             */
            if (key == NULL && ctx->cipher->einit_skey != NULL) {
                return ctx->cipher->einit_skey(ctx->algctx, NULL,
                    iv,
                    iv == NULL ? 0
                               : EVP_CIPHER_CTX_get_iv_length(ctx),
                    params);
            }
            ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
            return 0;
        }

        return ctx->cipher->einit(ctx->algctx,
            key,
            key == NULL ? 0
                        : EVP_CIPHER_CTX_get_key_length(ctx),
            iv,
            iv == NULL ? 0
                       : EVP_CIPHER_CTX_get_iv_length(ctx),
            params);
    }

    if (ctx->cipher->dinit == NULL) {
        /*
         * We still should be able to set the IV using the new API
         * if the key is not specified and old API is not available
         */
        if (key == NULL && ctx->cipher->dinit_skey != NULL) {
            return ctx->cipher->dinit_skey(ctx->algctx, NULL,
                iv,
                iv == NULL ? 0
                           : EVP_CIPHER_CTX_get_iv_length(ctx),
                params);
        }
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    return ctx->cipher->dinit(ctx->algctx,
        key,
        key == NULL ? 0
                    : EVP_CIPHER_CTX_get_key_length(ctx),
        iv,
        iv == NULL ? 0
                   : EVP_CIPHER_CTX_get_iv_length(ctx),
        params);
}

#ifndef FIPS_MODULE
/*
 * This function is basically evp_cipher_init_internal without ENGINE support.
 * They should be combined when engines are not supported any longer.
 */
static int evp_cipher_init_skey_internal(EVP_CIPHER_CTX *ctx,
    const EVP_CIPHER *cipher,
    const EVP_SKEY *skey,
    const unsigned char *iv, size_t iv_len,
    int enc, const OSSL_PARAM params[])
{
    int ret;

    /*
     * enc == 1 means we are encrypting.
     * enc == 0 means we are decrypting.
     * enc == -1 means, use the previously initialised value for encrypt/decrypt
     */
    if (enc == -1)
        enc = ctx->encrypt;
    else
        ctx->encrypt = enc != 0;

    if (cipher == NULL && ctx->cipher == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    /* Ensure a context left lying around from last time is cleared */
    if (cipher != NULL && ctx->cipher != NULL) {
        unsigned long flags = ctx->flags;

        EVP_CIPHER_CTX_reset(ctx);
        /* Restore encrypt and flags */
        ctx->encrypt = enc;
        ctx->flags = flags;
    }

    if (cipher == NULL)
        cipher = ctx->cipher;

    if (cipher->prov == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    if (cipher != ctx->fetched_cipher) {
        if (!EVP_CIPHER_up_ref((EVP_CIPHER *)cipher)) {
            ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
            return 0;
        }
        EVP_CIPHER_free(ctx->fetched_cipher);
        /* Coverity false positive, the reference counting is confusing it */
        /* coverity[use_after_free] */
        ctx->fetched_cipher = (EVP_CIPHER *)cipher;
    }
    ctx->cipher = cipher;
    if (ctx->algctx == NULL) {
        ctx->algctx = ctx->cipher->newctx(ossl_provider_ctx(cipher->prov));
        if (ctx->algctx == NULL) {
            ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
            return 0;
        }
    }

    if (skey != NULL && ctx->cipher->prov != skey->skeymgmt->prov) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    if ((ctx->flags & EVP_CIPH_NO_PADDING) != 0) {
        /*
         * If this ctx was already set up for no padding then we need to tell
         * the new cipher about it.
         */
        if (!EVP_CIPHER_CTX_set_padding(ctx, 0))
            return 0;
    }

    if (iv == NULL)
        iv_len = 0;

    /* We have a data managed via key management, using the new callbacks */
    if (enc) {
        if (ctx->cipher->einit_skey == NULL) {
            /*
             *  When skey is NULL, it's a multiple-step init as the current API does.
             *  Otherwise we try to fallback for providers that do not support SKEYs.
             */
            const unsigned char *keydata = NULL;
            size_t keylen = 0;

            if (skey != NULL && !EVP_SKEY_get0_raw_key(skey, &keydata, &keylen)) {
                ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
                return 0;
            }

            ret = ctx->cipher->einit(ctx->algctx, keydata, keylen,
                iv, iv_len, params);
        } else {
            ret = ctx->cipher->einit_skey(ctx->algctx,
                skey == NULL ? NULL : skey->keydata,
                iv, iv_len, params);
        }
    } else {
        if (ctx->cipher->dinit_skey == NULL) {
            /*
             *  When skey is NULL, it's a multiple-step init as the current API does.
             *  Otherwise we try to fallback for providers that do not support SKEYs.
             */
            const unsigned char *keydata = NULL;
            size_t keylen = 0;

            if (skey != NULL && !EVP_SKEY_get0_raw_key(skey, &keydata, &keylen)) {
                ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
                return 0;
            }

            ret = ctx->cipher->dinit(ctx->algctx, keydata, keylen,
                iv, iv_len, params);
        } else {
            ret = ctx->cipher->dinit_skey(ctx->algctx,
                skey == NULL ? NULL : skey->keydata,
                iv, iv_len, params);
        }
    }

    return ret;
}

int EVP_CipherInit_SKEY(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    EVP_SKEY *skey, const unsigned char *iv, size_t iv_len,
    int enc, const OSSL_PARAM params[])
{
    return evp_cipher_init_skey_internal(ctx, cipher, skey, iv, iv_len, enc, params);
}
#endif /* !FIPS_MODULE */

int EVP_CipherInit_ex2(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    const unsigned char *key, const unsigned char *iv,
    int enc, const OSSL_PARAM params[])
{
    return evp_cipher_init_internal(ctx, cipher, key, iv, enc, 0, params);
}

int EVP_CipherInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    const unsigned char *key, const unsigned char *iv, int enc)
{
    if (cipher != NULL)
        EVP_CIPHER_CTX_reset(ctx);
    return evp_cipher_init_internal(ctx, cipher, key, iv, enc, 0, NULL);
}

int EVP_CipherInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    ENGINE *impl, const unsigned char *key,
    const unsigned char *iv, int enc)
{
    if (!ossl_assert(impl == NULL))
        return 0;
    return evp_cipher_init_internal(ctx, cipher, key, iv, enc, 0, NULL);
}

int EVP_CipherPipelineEncryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    const unsigned char *key, size_t keylen,
    size_t numpipes,
    const unsigned char **iv, size_t ivlen)
{
    if (numpipes > EVP_MAX_PIPES) {
        ERR_raise(ERR_LIB_EVP, EVP_R_TOO_MANY_PIPES);
        return 0;
    }

    ctx->numpipes = numpipes;

    if (!evp_cipher_init_internal(ctx, cipher, NULL, NULL, 1, 1,
            NULL))
        return 0;

    if (ctx->cipher->p_einit == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    return ctx->cipher->p_einit(ctx->algctx,
        key,
        keylen,
        numpipes,
        iv,
        ivlen,
        NULL);
}

int EVP_CipherPipelineDecryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    const unsigned char *key, size_t keylen,
    size_t numpipes,
    const unsigned char **iv, size_t ivlen)
{
    if (numpipes > EVP_MAX_PIPES) {
        ERR_raise(ERR_LIB_EVP, EVP_R_TOO_MANY_PIPES);
        return 0;
    }

    ctx->numpipes = numpipes;

    if (!evp_cipher_init_internal(ctx, cipher, NULL, NULL, 0, 1,
            NULL))
        return 0;

    if (ctx->cipher->p_dinit == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INITIALIZATION_ERROR);
        return 0;
    }

    return ctx->cipher->p_dinit(ctx->algctx,
        key,
        keylen,
        numpipes,
        iv,
        ivlen,
        NULL);
}

int EVP_CipherUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
    const unsigned char *in, int inl)
{
    if (ctx->encrypt)
        return EVP_EncryptUpdate(ctx, out, outl, in, inl);
    else
        return EVP_DecryptUpdate(ctx, out, outl, in, inl);
}

int EVP_CipherPipelineUpdate(EVP_CIPHER_CTX *ctx,
    unsigned char **out, size_t *outl,
    const size_t *outsize,
    const unsigned char **in, const size_t *inl)
{
    size_t i;

    if (ossl_unlikely(outl == NULL || inl == NULL)) {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    if (ossl_unlikely(ctx->cipher == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    if (ossl_unlikely(ctx->cipher->prov == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_OPERATION);
        return 0;
    }

    if (ossl_unlikely(ctx->cipher->p_cupdate == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_UPDATE_ERROR);
        return 0;
    }

    for (i = 0; i < ctx->numpipes; i++)
        outl[i] = 0;

    return ctx->cipher->p_cupdate(ctx->algctx, ctx->numpipes,
        out, outl, outsize,
        in, inl);
}

int EVP_CipherFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    if (ctx->encrypt)
        return EVP_EncryptFinal_ex(ctx, out, outl);
    else
        return EVP_DecryptFinal_ex(ctx, out, outl);
}

int EVP_CipherFinal(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    if (ctx->encrypt)
        return EVP_EncryptFinal(ctx, out, outl);
    else
        return EVP_DecryptFinal(ctx, out, outl);
}

int EVP_CipherPipelineFinal(EVP_CIPHER_CTX *ctx,
    unsigned char **out, size_t *outl,
    const size_t *outsize)
{
    size_t i;

    if (ossl_unlikely(outl == NULL)) {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    if (ossl_unlikely(ctx->cipher == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    if (ossl_unlikely(ctx->cipher->prov == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_OPERATION);
        return 0;
    }

    if (ossl_unlikely(ctx->cipher->p_cfinal == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_FINAL_ERROR);
        return 0;
    }

    for (i = 0; i < ctx->numpipes; i++)
        outl[i] = 0;

    return ctx->cipher->p_cfinal(ctx->algctx, ctx->numpipes,
        out, outl, outsize);
}

int EVP_EncryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    const unsigned char *key, const unsigned char *iv)
{
    return EVP_CipherInit(ctx, cipher, key, iv, 1);
}

int EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    ENGINE *impl, const unsigned char *key,
    const unsigned char *iv)
{
    if (!ossl_assert(impl == NULL))
        return 0;
    return EVP_CipherInit_ex(ctx, cipher, NULL, key, iv, 1);
}

int EVP_EncryptInit_ex2(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    const unsigned char *key, const unsigned char *iv,
    const OSSL_PARAM params[])
{
    return EVP_CipherInit_ex2(ctx, cipher, key, iv, 1, params);
}

int EVP_DecryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    const unsigned char *key, const unsigned char *iv)
{
    return EVP_CipherInit(ctx, cipher, key, iv, 0);
}

int EVP_DecryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    ENGINE *impl, const unsigned char *key,
    const unsigned char *iv)
{
    if (!ossl_assert(impl == NULL))
        return 0;
    return EVP_CipherInit_ex(ctx, cipher, NULL, key, iv, 0);
}

int EVP_DecryptInit_ex2(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    const unsigned char *key, const unsigned char *iv,
    const OSSL_PARAM params[])
{
    return EVP_CipherInit_ex2(ctx, cipher, key, iv, 0, params);
}

/*
 * According to the letter of standard difference between pointers
 * is specified to be valid only within same object. This makes
 * it formally challenging to determine if input and output buffers
 * are not partially overlapping with standard pointer arithmetic.
 */
#ifdef PTRDIFF_T
#undef PTRDIFF_T
#endif
#if defined(OPENSSL_SYS_VMS) && __INITIAL_POINTER_SIZE == 64
/*
 * Then we have VMS that distinguishes itself by adhering to
 * sizeof(size_t)==4 even in 64-bit builds, which means that
 * difference between two pointers might be truncated to 32 bits.
 * In the context one can even wonder how comparison for
 * equality is implemented. To be on the safe side we adhere to
 * PTRDIFF_T even for comparison for equality.
 */
#define PTRDIFF_T uint64_t
#else
#define PTRDIFF_T size_t
#endif

int ossl_is_partially_overlapping(const void *ptr1, const void *ptr2, int len)
{
    PTRDIFF_T diff = (PTRDIFF_T)ptr1 - (PTRDIFF_T)ptr2;
    /*
     * Check for partially overlapping buffers. [Binary logical
     * operations are used instead of boolean to minimize number
     * of conditional branches.]
     */
    int overlapped = (len > 0) & (diff != 0) & ((diff < (PTRDIFF_T)len) | (diff > (0 - (PTRDIFF_T)len)));

    return overlapped;
}

int EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
    const unsigned char *in, int inl)
{
    int ret;
    size_t soutl, inl_ = (size_t)inl;
    int blocksize;

    if (inl < 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_LENGTH);
        return 0;
    }

    if (ossl_likely(outl != NULL)) {
        *outl = 0;
    } else {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    /* Prevent accidental use of decryption context when encrypting */
    if (ossl_unlikely(!ctx->encrypt)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_OPERATION);
        return 0;
    }

    if (ossl_unlikely(ctx->cipher == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    if (ossl_unlikely(ctx->cipher->prov == NULL))
        return 0;

    blocksize = ctx->cipher->block_size;

    if (ossl_unlikely(ctx->cipher->cupdate == NULL || blocksize < 1)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_UPDATE_ERROR);
        return 0;
    }

    ret = ctx->cipher->cupdate(ctx->algctx, out, &soutl,
        inl_ + (size_t)(blocksize == 1 ? 0 : blocksize),
        in, inl_);

    if (ossl_likely(ret)) {
        if (ossl_unlikely(soutl > INT_MAX)) {
            ERR_raise(ERR_LIB_EVP, EVP_R_UPDATE_ERROR);
            return 0;
        }
        *outl = (int)soutl;
    }

    return ret;
}

int EVP_EncryptFinal(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    int ret;
    ret = EVP_EncryptFinal_ex(ctx, out, outl);
    return ret;
}

int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    int ret;
    size_t soutl;
    int blocksize;

    if (outl != NULL) {
        *outl = 0;
    } else {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    /* Prevent accidental use of decryption context when encrypting */
    if (!ctx->encrypt) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_OPERATION);
        return 0;
    }

    if (ctx->cipher == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }
    if (ctx->cipher->prov == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    blocksize = EVP_CIPHER_CTX_get_block_size(ctx);

    if (blocksize < 1 || ctx->cipher->cfinal == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_FINAL_ERROR);
        return 0;
    }

    ret = ctx->cipher->cfinal(ctx->algctx, out, &soutl,
        blocksize == 1 ? 0 : blocksize);

    if (ret) {
        if (soutl > INT_MAX) {
            ERR_raise(ERR_LIB_EVP, EVP_R_FINAL_ERROR);
            return 0;
        }
        *outl = (int)soutl;
    }

    return ret;
}

int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
    const unsigned char *in, int inl)
{
    int ret;
    size_t soutl, inl_ = (size_t)inl;
    int blocksize;

    if (inl < 0) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_LENGTH);
        return 0;
    }

    if (ossl_likely(outl != NULL)) {
        *outl = 0;
    } else {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    /* Prevent accidental use of encryption context when decrypting */
    if (ossl_unlikely(ctx->encrypt)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_OPERATION);
        return 0;
    }

    if (ossl_unlikely(ctx->cipher == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }
    if (ossl_unlikely(ctx->cipher->prov == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    blocksize = EVP_CIPHER_CTX_get_block_size(ctx);

    if (ossl_unlikely(ctx->cipher->cupdate == NULL || blocksize < 1)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_UPDATE_ERROR);
        return 0;
    }
    ret = ctx->cipher->cupdate(ctx->algctx, out, &soutl,
        inl_ + (size_t)(blocksize == 1 ? 0 : blocksize),
        in, inl_);

    if (ossl_likely(ret)) {
        if (ossl_unlikely(soutl > INT_MAX)) {
            ERR_raise(ERR_LIB_EVP, EVP_R_UPDATE_ERROR);
            return 0;
        }
        *outl = (int)soutl;
    }

    return ret;
}

int EVP_DecryptFinal(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    int ret;
    ret = EVP_DecryptFinal_ex(ctx, out, outl);
    return ret;
}

int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    size_t soutl;
    int ret;
    int blocksize;

    if (outl != NULL) {
        *outl = 0;
    } else {
        ERR_raise(ERR_LIB_EVP, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    /* Prevent accidental use of encryption context when decrypting */
    if (ctx->encrypt) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_OPERATION);
        return 0;
    }

    if (ctx->cipher == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    if (ctx->cipher->prov == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    blocksize = EVP_CIPHER_CTX_get_block_size(ctx);

    if (blocksize < 1 || ctx->cipher->cfinal == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_FINAL_ERROR);
        return 0;
    }

    ret = ctx->cipher->cfinal(ctx->algctx, out, &soutl,
        blocksize == 1 ? 0 : blocksize);

    if (ret) {
        if (soutl > INT_MAX) {
            ERR_raise(ERR_LIB_EVP, EVP_R_FINAL_ERROR);
            return 0;
        }
        *outl = (int)soutl;
    }

    return ret;
}

int EVP_CIPHER_CTX_set_key_length(EVP_CIPHER_CTX *c, int keylen)
{
    int ok;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };
    size_t len;

    if (c->cipher->prov == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_CIPHER_PARAMETER_ERROR);
        return 0;
    }

    if (EVP_CIPHER_CTX_get_key_length(c) == keylen)
        return 1;

    /* Check the cipher actually understands this parameter */
    if (OSSL_PARAM_locate_const(EVP_CIPHER_settable_ctx_params(c->cipher),
            OSSL_CIPHER_PARAM_KEYLEN)
        == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_KEY_LENGTH);
        return 0;
    }

    params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN, &len);
    if (!OSSL_PARAM_set_int(params, keylen))
        return 0;
    ok = evp_do_ciph_ctx_setparams(c->cipher, c->algctx, params);
    if (ok <= 0)
        return 0;
    c->key_len = keylen;
    return 1;
}

int EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *ctx, int pad)
{
    int ok;
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };
    unsigned int pd = pad;

    if (pad)
        ctx->flags &= ~EVP_CIPH_NO_PADDING;
    else
        ctx->flags |= EVP_CIPH_NO_PADDING;

    if (ctx->cipher != NULL && ctx->cipher->prov == NULL)
        return 1;
    params[0] = OSSL_PARAM_construct_uint(OSSL_CIPHER_PARAM_PADDING, &pd);
    ok = evp_do_ciph_ctx_setparams(ctx->cipher, ctx->algctx, params);

    return ok != 0;
}

int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
{
    int ret = EVP_CTRL_RET_UNSUPPORTED;
    int set_params = 1;
    size_t sz = arg;
    unsigned int i;
    OSSL_PARAM params[4] = {
        OSSL_PARAM_END, OSSL_PARAM_END, OSSL_PARAM_END, OSSL_PARAM_END
    };

    if (ctx == NULL || ctx->cipher == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NO_CIPHER_SET);
        return 0;
    }

    if (ctx->cipher->prov == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_CTRL_NOT_IMPLEMENTED);
        return 0;
    }

    switch (type) {
    case EVP_CTRL_SET_KEY_LENGTH:
        if (arg < 0)
            return 0;
        if (ctx->key_len == arg)
            /* Skip calling into provider if unchanged. */
            return 1;
        params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN, &sz);
        ctx->key_len = -1;
        break;
    case EVP_CTRL_RAND_KEY: /* Used by DES */
        set_params = 0;
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_RANDOM_KEY,
            ptr, sz);
        break;

    case EVP_CTRL_INIT:
        /*
         * EVP_CTRL_INIT is purely legacy, no provider counterpart.
         * As a matter of fact, this should be dead code, but some caller
         * might still do a direct control call with this command, so...
         * Legacy methods return 1 except for exceptional circumstances, so
         * we do the same here to not be disruptive.
         */
        return 1;
    case EVP_CTRL_SET_PIPELINE_OUTPUT_BUFS: /* Used by DASYNC */
    default:
        goto end;
    case EVP_CTRL_AEAD_SET_IVLEN:
        if (arg < 0)
            return 0;
        if (ctx->iv_len == arg)
            /* Skip calling into provider if unchanged. */
            return 1;
        params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &sz);
        ctx->iv_len = -1;
        break;
    case EVP_CTRL_CCM_SET_L:
        if (arg < 2 || arg > 8)
            return 0;
        sz = 15 - arg;
        params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &sz);
        ctx->iv_len = -1;
        break;
    case EVP_CTRL_AEAD_SET_IV_FIXED:
        /*
         * arg == -1 is a valid sentinel meaning "use full ivlen"; anything
         * below that would wrap to a huge size_t and overflow on memcpy.
         */
        if (arg < -1)
            return 0;
        params[0] = OSSL_PARAM_construct_octet_string(
            OSSL_CIPHER_PARAM_AEAD_TLS1_IV_FIXED, ptr, sz);
        break;
    case EVP_CTRL_GCM_IV_GEN:
        set_params = 0;
        if (arg < 0)
            sz = 0; /* special case that uses the iv length */
        params[0] = OSSL_PARAM_construct_octet_string(
            OSSL_CIPHER_PARAM_AEAD_TLS1_GET_IV_GEN, ptr, sz);
        break;
    case EVP_CTRL_GCM_SET_IV_INV:
        if (arg < 0)
            return 0;
        params[0] = OSSL_PARAM_construct_octet_string(
            OSSL_CIPHER_PARAM_AEAD_TLS1_SET_IV_INV, ptr, sz);
        break;
    case EVP_CTRL_GET_RC5_ROUNDS:
        set_params = 0; /* Fall thru */
    case EVP_CTRL_SET_RC5_ROUNDS:
        if (arg < 0)
            return 0;
        i = (unsigned int)arg;
        params[0] = OSSL_PARAM_construct_uint(OSSL_CIPHER_PARAM_ROUNDS, &i);
        break;
    case EVP_CTRL_SET_SPEED:
        if (arg < 0)
            return 0;
        i = (unsigned int)arg;
        params[0] = OSSL_PARAM_construct_uint(OSSL_CIPHER_PARAM_SPEED, &i);
        break;
    case EVP_CTRL_AEAD_GET_TAG:
        set_params = 0; /* Fall thru */
    case EVP_CTRL_AEAD_SET_TAG:
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG,
            ptr, sz);
        break;
    case EVP_CTRL_AEAD_TLS1_AAD:
        /* This one does a set and a get - since it returns a size */
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TLS1_AAD,
            ptr, sz);
        ret = evp_do_ciph_ctx_setparams(ctx->cipher, ctx->algctx, params);
        if (ret <= 0)
            goto end;
        params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_TLS1_AAD_PAD, &sz);
        ret = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);
        if (ret <= 0)
            goto end;
        if (sz > INT_MAX)
            return 0;
        return (int)sz;
#ifndef OPENSSL_NO_RC2
    case EVP_CTRL_GET_RC2_KEY_BITS:
        set_params = 0; /* Fall thru */
    case EVP_CTRL_SET_RC2_KEY_BITS:
        params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_RC2_KEYBITS, &sz);
        break;
#endif /* OPENSSL_NO_RC2 */
#if !defined(OPENSSL_NO_MULTIBLOCK)
    case EVP_CTRL_TLS1_1_MULTIBLOCK_MAX_BUFSIZE:
        params[0] = OSSL_PARAM_construct_size_t(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_MAX_SEND_FRAGMENT, &sz);
        ret = evp_do_ciph_ctx_setparams(ctx->cipher, ctx->algctx, params);
        if (ret <= 0)
            return 0;

        params[0] = OSSL_PARAM_construct_size_t(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_MAX_BUFSIZE, &sz);
        params[1] = OSSL_PARAM_construct_end();
        ret = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);
        if (ret <= 0 || sz > INT_MAX)
            return 0;
        return (int)sz;
    case EVP_CTRL_TLS1_1_MULTIBLOCK_AAD: {
        EVP_CTRL_TLS1_1_MULTIBLOCK_PARAM *p = (EVP_CTRL_TLS1_1_MULTIBLOCK_PARAM *)ptr;

        if (arg < (int)sizeof(EVP_CTRL_TLS1_1_MULTIBLOCK_PARAM))
            return 0;

        params[0] = OSSL_PARAM_construct_octet_string(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_AAD, (void *)p->inp, p->len);
        params[1] = OSSL_PARAM_construct_uint(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_INTERLEAVE, &p->interleave);
        ret = evp_do_ciph_ctx_setparams(ctx->cipher, ctx->algctx, params);
        if (ret <= 0)
            return ret;
        /* Retrieve the return values changed by the set */
        params[0] = OSSL_PARAM_construct_size_t(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_AAD_PACKLEN, &sz);
        params[1] = OSSL_PARAM_construct_uint(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_INTERLEAVE, &p->interleave);
        params[2] = OSSL_PARAM_construct_end();
        ret = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);
        if (ret <= 0 || sz > INT_MAX)
            return 0;
        return (int)sz;
    }
    case EVP_CTRL_TLS1_1_MULTIBLOCK_ENCRYPT: {
        EVP_CTRL_TLS1_1_MULTIBLOCK_PARAM *p = (EVP_CTRL_TLS1_1_MULTIBLOCK_PARAM *)ptr;

        params[0] = OSSL_PARAM_construct_octet_string(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_ENC, p->out, p->len);

        params[1] = OSSL_PARAM_construct_octet_string(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_ENC_IN, (void *)p->inp,
            p->len);
        params[2] = OSSL_PARAM_construct_uint(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_INTERLEAVE, &p->interleave);
        ret = evp_do_ciph_ctx_setparams(ctx->cipher, ctx->algctx, params);
        if (ret <= 0)
            return ret;
        params[0] = OSSL_PARAM_construct_size_t(
            OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_ENC_LEN, &sz);
        params[1] = OSSL_PARAM_construct_end();
        ret = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);
        if (ret <= 0 || sz > INT_MAX)
            return 0;
        return (int)sz;
    }
#endif /* OPENSSL_NO_MULTIBLOCK */
    case EVP_CTRL_AEAD_SET_MAC_KEY:
        if (arg < 0)
            return -1;
        params[0] = OSSL_PARAM_construct_octet_string(
            OSSL_CIPHER_PARAM_AEAD_MAC_KEY, ptr, sz);
        break;
    }

    if (set_params)
        ret = evp_do_ciph_ctx_setparams(ctx->cipher, ctx->algctx, params);
    else
        ret = evp_do_ciph_ctx_getparams(ctx->cipher, ctx->algctx, params);

end:
    if (ret == EVP_CTRL_RET_UNSUPPORTED) {
        ERR_raise(ERR_LIB_EVP, EVP_R_CTRL_OPERATION_NOT_IMPLEMENTED);
        return 0;
    }
    return ret;
}

int EVP_CIPHER_get_params(EVP_CIPHER *cipher, OSSL_PARAM params[])
{
    if (cipher != NULL && cipher->get_params != NULL)
        return cipher->get_params(params);
    return 0;
}

int EVP_CIPHER_CTX_set_params(EVP_CIPHER_CTX *ctx, const OSSL_PARAM params[])
{
    int r = 0;
    const OSSL_PARAM *p;

    if (ctx->cipher != NULL && ctx->cipher->set_ctx_params != NULL) {
        r = ctx->cipher->set_ctx_params(ctx->algctx, params);
        if (r > 0) {
            p = OSSL_PARAM_locate_const(params, OSSL_CIPHER_PARAM_KEYLEN);
            if (p != NULL && !OSSL_PARAM_get_int(p, &ctx->key_len)) {
                r = 0;
                ctx->key_len = -1;
            }
        }
        if (r > 0) {
            p = OSSL_PARAM_locate_const(params, OSSL_CIPHER_PARAM_IVLEN);
            if (p != NULL && !OSSL_PARAM_get_int(p, &ctx->iv_len)) {
                r = 0;
                ctx->iv_len = -1;
            }
        }
    }
    return r;
}

int EVP_CIPHER_CTX_get_params(EVP_CIPHER_CTX *ctx, OSSL_PARAM params[])
{
    if (ctx->cipher != NULL && ctx->cipher->get_ctx_params != NULL)
        return ctx->cipher->get_ctx_params(ctx->algctx, params);
    return 0;
}

const OSSL_PARAM *EVP_CIPHER_gettable_params(const EVP_CIPHER *cipher)
{
    if (cipher != NULL && cipher->gettable_params != NULL)
        return cipher->gettable_params(
            ossl_provider_ctx(EVP_CIPHER_get0_provider(cipher)));
    return NULL;
}

const OSSL_PARAM *EVP_CIPHER_settable_ctx_params(const EVP_CIPHER *cipher)
{
    void *provctx;

    if (cipher != NULL && cipher->settable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(EVP_CIPHER_get0_provider(cipher));
        return cipher->settable_ctx_params(NULL, provctx);
    }
    return NULL;
}

const OSSL_PARAM *EVP_CIPHER_gettable_ctx_params(const EVP_CIPHER *cipher)
{
    void *provctx;

    if (cipher != NULL && cipher->gettable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(EVP_CIPHER_get0_provider(cipher));
        return cipher->gettable_ctx_params(NULL, provctx);
    }
    return NULL;
}

const OSSL_PARAM *EVP_CIPHER_CTX_settable_params(EVP_CIPHER_CTX *cctx)
{
    void *alg;

    if (cctx != NULL && cctx->cipher->settable_ctx_params != NULL) {
        alg = ossl_provider_ctx(EVP_CIPHER_get0_provider(cctx->cipher));
        return cctx->cipher->settable_ctx_params(cctx->algctx, alg);
    }
    return NULL;
}

const OSSL_PARAM *EVP_CIPHER_CTX_gettable_params(EVP_CIPHER_CTX *cctx)
{
    void *provctx;

    if (cctx != NULL && cctx->cipher->gettable_ctx_params != NULL) {
        provctx = ossl_provider_ctx(EVP_CIPHER_get0_provider(cctx->cipher));
        return cctx->cipher->gettable_ctx_params(cctx->algctx, provctx);
    }
    return NULL;
}

#ifndef FIPS_MODULE
static OSSL_LIB_CTX *EVP_CIPHER_CTX_get_libctx(EVP_CIPHER_CTX *ctx)
{
    const EVP_CIPHER *cipher = ctx->cipher;
    const OSSL_PROVIDER *prov;

    if (cipher == NULL)
        return NULL;

    prov = EVP_CIPHER_get0_provider(cipher);
    return ossl_provider_libctx(prov);
}
#endif

int EVP_CIPHER_CTX_rand_key(EVP_CIPHER_CTX *ctx, unsigned char *key)
{
    if (ctx->cipher->flags & EVP_CIPH_RAND_KEY)
        return EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_RAND_KEY, 0, key);

#ifdef FIPS_MODULE
    return 0;
#else
    {
        int kl;
        OSSL_LIB_CTX *libctx = EVP_CIPHER_CTX_get_libctx(ctx);

        kl = EVP_CIPHER_CTX_get_key_length(ctx);
        if (kl <= 0 || RAND_priv_bytes_ex(libctx, key, kl, 0) <= 0)
            return 0;
        return 1;
    }
#endif /* FIPS_MODULE */
}

EVP_CIPHER_CTX *EVP_CIPHER_CTX_dup(const EVP_CIPHER_CTX *in)
{
    EVP_CIPHER_CTX *out = EVP_CIPHER_CTX_new();

    if (out != NULL && !EVP_CIPHER_CTX_copy(out, in)) {
        EVP_CIPHER_CTX_free(out);
        out = NULL;
    }
    return out;
}

int EVP_CIPHER_CTX_copy(EVP_CIPHER_CTX *out, const EVP_CIPHER_CTX *in)
{
    if ((in == NULL) || (in->cipher == NULL)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INPUT_NOT_INITIALIZED);
        return 0;
    }

    if (in->cipher->prov == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_INPUT_NOT_INITIALIZED);
        return 0;
    }

    if (in->cipher->dupctx == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NOT_ABLE_TO_COPY_CTX);
        return 0;
    }

    EVP_CIPHER_CTX_reset(out);

    *out = *in;
    out->algctx = NULL;

    if (in->fetched_cipher != NULL && !EVP_CIPHER_up_ref(in->fetched_cipher)) {
        out->fetched_cipher = NULL;
        return 0;
    }

    out->algctx = in->cipher->dupctx(in->algctx);
    if (out->algctx == NULL) {
        ERR_raise(ERR_LIB_EVP, EVP_R_NOT_ABLE_TO_COPY_CTX);
        return 0;
    }

    return 1;
}

EVP_CIPHER *evp_cipher_new(void)
{
    EVP_CIPHER *cipher = OPENSSL_zalloc(sizeof(EVP_CIPHER));

    if (cipher != NULL && !CRYPTO_NEW_REF(&cipher->refcnt, 1)) {
        OPENSSL_free(cipher);
        return NULL;
    }
    return cipher;
}

/*
 * FIPS module note: since internal fetches will be entirely
 * provider based, we know that none of its code depends on legacy
 * NIDs or any functionality that use them.
 */
#ifndef FIPS_MODULE
/* After removal of legacy support get rid of the need for legacy NIDs */
static void set_legacy_nid(const char *name, void *vlegacy_nid)
{
    int nid;
    int *legacy_nid = vlegacy_nid;
    /*
     * We use lowest level function to get the associated method, because
     * higher level functions such as EVP_get_cipherbyname() have changed
     * to look at providers too.
     */
    const void *legacy_method = OBJ_NAME_get(name, OBJ_NAME_TYPE_CIPHER_METH);

    if (*legacy_nid == -1) /* We found a clash already */
        return;
    if (legacy_method == NULL)
        return;
    nid = EVP_CIPHER_get_nid(legacy_method);
    if (*legacy_nid != NID_undef && *legacy_nid != nid) {
        *legacy_nid = -1;
        return;
    }
    *legacy_nid = nid;
}
#endif

static int evp_cipher_up_ref(void *c)
{
    EVP_CIPHER *cipher = (EVP_CIPHER *)c;
    int ref = 0;

    if (cipher->origin == EVP_ORIG_DYNAMIC)
        return CRYPTO_UP_REF(&cipher->refcnt, &ref);
    return 1;
}

static void evp_cipher_free(void *c)
{
    EVP_CIPHER *cipher = (EVP_CIPHER *)c;
    int i;

    if (cipher == NULL || cipher->origin != EVP_ORIG_DYNAMIC)
        return;

    CRYPTO_DOWN_REF(&cipher->refcnt, &i);
    if (i > 0)
        return;
    evp_cipher_free_int(cipher);
}

static void *evp_cipher_from_algorithm(const int name_id,
    const OSSL_ALGORITHM *algodef,
    OSSL_PROVIDER *prov, int no_store)
{
    const OSSL_DISPATCH *fns = algodef->implementation;
    EVP_CIPHER *cipher = NULL;
    int fnciphcnt = 0, encinit = 0, decinit = 0, fnpipecnt = 0, fnctxcnt = 0;

    if ((cipher = evp_cipher_new()) == NULL) {
        ERR_raise(ERR_LIB_EVP, ERR_R_EVP_LIB);
        return NULL;
    }

    if (no_store != 0)
        cipher->flags |= EVP_CIPH_FLAG_NO_STORE;

#ifndef FIPS_MODULE
    cipher->nid = NID_undef;
    if (!evp_names_do_all(prov, name_id, set_legacy_nid, &cipher->nid)
        || cipher->nid == -1) {
        ERR_raise(ERR_LIB_EVP, ERR_R_INTERNAL_ERROR);
        goto err;
    }
#endif

    cipher->name_id = name_id;
    if ((cipher->type_name = ossl_algorithm_get1_first_name(algodef)) == NULL)
        goto err;

    cipher->description = algodef->algorithm_description;

    for (; fns->function_id != 0; fns++) {
        switch (fns->function_id) {
        case OSSL_FUNC_CIPHER_NEWCTX:
            if (cipher->newctx != NULL)
                break;
            cipher->newctx = OSSL_FUNC_cipher_newctx(fns);
            fnctxcnt++;
            break;
        case OSSL_FUNC_CIPHER_ENCRYPT_INIT:
            if (cipher->einit != NULL)
                break;
            cipher->einit = OSSL_FUNC_cipher_encrypt_init(fns);
            encinit = 1;
            break;
        case OSSL_FUNC_CIPHER_DECRYPT_INIT:
            if (cipher->dinit != NULL)
                break;
            cipher->dinit = OSSL_FUNC_cipher_decrypt_init(fns);
            decinit = 1;
            break;
        case OSSL_FUNC_CIPHER_ENCRYPT_SKEY_INIT:
            if (cipher->einit_skey != NULL)
                break;
            cipher->einit_skey = OSSL_FUNC_cipher_encrypt_skey_init(fns);
            encinit = 1;
            break;
        case OSSL_FUNC_CIPHER_DECRYPT_SKEY_INIT:
            if (cipher->dinit_skey != NULL)
                break;
            cipher->dinit_skey = OSSL_FUNC_cipher_decrypt_skey_init(fns);
            decinit = 1;
            break;
        case OSSL_FUNC_CIPHER_UPDATE:
            if (cipher->cupdate != NULL)
                break;
            cipher->cupdate = OSSL_FUNC_cipher_update(fns);
            fnciphcnt++;
            break;
        case OSSL_FUNC_CIPHER_FINAL:
            if (cipher->cfinal != NULL)
                break;
            cipher->cfinal = OSSL_FUNC_cipher_final(fns);
            fnciphcnt++;
            break;
        case OSSL_FUNC_CIPHER_CIPHER:
            if (cipher->ccipher != NULL)
                break;
            cipher->ccipher = OSSL_FUNC_cipher_cipher(fns);
            break;
        case OSSL_FUNC_CIPHER_PIPELINE_ENCRYPT_INIT:
            if (cipher->p_einit != NULL)
                break;
            cipher->p_einit = OSSL_FUNC_cipher_pipeline_encrypt_init(fns);
            fnpipecnt++;
            break;
        case OSSL_FUNC_CIPHER_PIPELINE_DECRYPT_INIT:
            if (cipher->p_dinit != NULL)
                break;
            cipher->p_dinit = OSSL_FUNC_cipher_pipeline_decrypt_init(fns);
            fnpipecnt++;
            break;
        case OSSL_FUNC_CIPHER_PIPELINE_UPDATE:
            if (cipher->p_cupdate != NULL)
                break;
            cipher->p_cupdate = OSSL_FUNC_cipher_pipeline_update(fns);
            fnpipecnt++;
            break;
        case OSSL_FUNC_CIPHER_PIPELINE_FINAL:
            if (cipher->p_cfinal != NULL)
                break;
            cipher->p_cfinal = OSSL_FUNC_cipher_pipeline_final(fns);
            fnpipecnt++;
            break;
        case OSSL_FUNC_CIPHER_FREECTX:
            if (cipher->freectx != NULL)
                break;
            cipher->freectx = OSSL_FUNC_cipher_freectx(fns);
            fnctxcnt++;
            break;
        case OSSL_FUNC_CIPHER_DUPCTX:
            if (cipher->dupctx != NULL)
                break;
            cipher->dupctx = OSSL_FUNC_cipher_dupctx(fns);
            break;
        case OSSL_FUNC_CIPHER_GET_PARAMS:
            if (cipher->get_params != NULL)
                break;
            cipher->get_params = OSSL_FUNC_cipher_get_params(fns);
            break;
        case OSSL_FUNC_CIPHER_GET_CTX_PARAMS:
            if (cipher->get_ctx_params != NULL)
                break;
            cipher->get_ctx_params = OSSL_FUNC_cipher_get_ctx_params(fns);
            break;
        case OSSL_FUNC_CIPHER_SET_CTX_PARAMS:
            if (cipher->set_ctx_params != NULL)
                break;
            cipher->set_ctx_params = OSSL_FUNC_cipher_set_ctx_params(fns);
            break;
        case OSSL_FUNC_CIPHER_GETTABLE_PARAMS:
            if (cipher->gettable_params != NULL)
                break;
            cipher->gettable_params = OSSL_FUNC_cipher_gettable_params(fns);
            break;
        case OSSL_FUNC_CIPHER_GETTABLE_CTX_PARAMS:
            if (cipher->gettable_ctx_params != NULL)
                break;
            cipher->gettable_ctx_params = OSSL_FUNC_cipher_gettable_ctx_params(fns);
            break;
        case OSSL_FUNC_CIPHER_SETTABLE_CTX_PARAMS:
            if (cipher->settable_ctx_params != NULL)
                break;
            cipher->settable_ctx_params = OSSL_FUNC_cipher_settable_ctx_params(fns);
            break;
        }
    }
    fnciphcnt += encinit + decinit;
    if ((fnciphcnt != 0 && fnciphcnt != 3 && fnciphcnt != 4)
        || (fnciphcnt == 0 && cipher->ccipher == NULL && fnpipecnt == 0)
        || (fnpipecnt != 0 && (fnpipecnt < 3 || cipher->p_cupdate == NULL || cipher->p_cfinal == NULL))
        || fnctxcnt != 2
        || cipher->get_params == NULL) {
        /*
         * In order to be a consistent set of functions we must have at least
         * a complete set of "encrypt" functions, or a complete set of "decrypt"
         * functions, or a single "cipher" function.
         */
        ERR_raise(ERR_LIB_EVP, EVP_R_INVALID_PROVIDER_FUNCTIONS);
        goto err;
    }
    if (prov != NULL && !ossl_provider_up_ref(prov))
        goto err;

    cipher->prov = prov;

    if (!evp_cipher_cache_constants(cipher)) {
        ERR_raise(ERR_LIB_EVP, EVP_R_CACHE_CONSTANTS_FAILED);
        goto err;
    }

    return cipher;

err:
    evp_cipher_free(cipher);
    return NULL;
}

EVP_CIPHER *EVP_CIPHER_fetch(OSSL_LIB_CTX *ctx, const char *algorithm,
    const char *properties)
{
    EVP_CIPHER *cipher = evp_generic_fetch(ctx, OSSL_OP_CIPHER, algorithm, properties,
        evp_cipher_from_algorithm, evp_cipher_up_ref,
        evp_cipher_free);

    return cipher;
}

EVP_CIPHER *evp_cipher_fetch_from_prov(OSSL_PROVIDER *prov,
    const char *algorithm,
    const char *properties)
{
    return evp_generic_fetch_from_prov(prov, OSSL_OP_CIPHER,
        algorithm, properties,
        evp_cipher_from_algorithm,
        evp_cipher_up_ref,
        evp_cipher_free);
}

int EVP_CIPHER_can_pipeline(const EVP_CIPHER *cipher, int enc)
{
    if (((enc && cipher->p_einit != NULL) || (!enc && cipher->p_dinit != NULL))
        && cipher->p_cupdate != NULL && cipher->p_cfinal != NULL)
        return 1;

    return 0;
}

int EVP_CIPHER_up_ref(EVP_CIPHER *cipher)
{
#ifdef OPENSSL_NO_CACHED_FETCH
    return evp_cipher_up_ref(cipher);
#else
    if (cipher->flags & EVP_CIPH_FLAG_NO_STORE)
        return evp_cipher_up_ref(cipher);
    return 1;
#endif
}

void evp_cipher_free_int(EVP_CIPHER *cipher)
{
    OPENSSL_free(cipher->type_name);
    ossl_provider_free(cipher->prov);
    CRYPTO_FREE_REF(&cipher->refcnt);
    OPENSSL_free(cipher);
}

void EVP_CIPHER_free(EVP_CIPHER *cipher)
{
#ifdef OPENSSL_NO_CACHED_FETCH
    evp_cipher_free(cipher);
#else
    if (cipher != NULL && (cipher->flags & EVP_CIPH_FLAG_NO_STORE))
        evp_cipher_free(cipher);
#endif
}

void EVP_CIPHER_do_all_provided(OSSL_LIB_CTX *libctx,
    void (*fn)(EVP_CIPHER *mac, void *arg),
    void *arg)
{
    struct EVP_CIPHER_do_all_provided_thunk t;

    t.fn = fn;
    t.arg = arg;
    evp_generic_do_all(libctx, OSSL_OP_CIPHER,
        EVP_CIPHER_do_all_provided_thunk, &t,
        evp_cipher_from_algorithm, evp_cipher_up_ref,
        evp_cipher_free);
}
