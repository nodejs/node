/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/**
 * @file p_ossltest.c
 * @brief a test provider for use in several of our unit tests
 *
 * This file implements a provider that goes through the motions of implementing
 * various algorithms, but then discards the actual results of that work in favor
 * of predictable return data for the purposes of having known data to compare against
 * in our various tls tests.
 *
 * It implements the following algorithms
 *
 * The AES-128-CBC cipher
 * The AES-128-GCM cipher
 * The AES-128-CBC-HMAC-SHA1 cipher
 * The MD5 digest
 * The SHA1, SHA256, SHA384 and SHA512 digests
 * The CTR-DRBG random number generator
 *
 * Note that the implementations of the above algorithms are designed not to
 * actually follow the prescribed algorithms themselves, but rather just to
 * returns known/predictable data values for the purposes of testing.  As such
 * DO NOT USE THIS PROVIDER FOR ANY PRODUCTION PURPOSE.  TESTING ONLY!!!!
 *
 * The digests all return incremental data in accordance with the size of their message
 * digest
 *
 * The ciphers all return data that is a copy of their input plaintext, padded and augmented
 * according to the specific ciphers needs
 *
 * The random number generator just returns incremental data of the requested size
 */
#include "internal/deprecated.h"

#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/rand.h> /* RAND_get0_public() */
#include <openssl/proverr.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/prov_ssl.h>
#include "prov/provider_ctx.h"
#include "prov/digestcommon.h"
#include "prov/ciphercommon.h"
#include "prov/names.h"
#include "prov/implementations.h"
#include "ciphers/cipher_aes.h"
#include "internal/cryptlib.h"
#include "internal/provider.h"
#include "crypto/context.h"
#include "internal/core.h"

/**
 * @brief Release resources and clean up the context.
 *
 * @param provctx void *provctx.
 * @return void.
 */

static void ossltest_teardown(void *provctx)
{
    OSSL_LIB_CTX_free(PROV_LIBCTX_OF(provctx));
    ossl_prov_ctx_free(provctx);
}

static const OSSL_PARAM ossltest_param_types[] = {
    OSSL_PARAM_DEFN(OSSL_PROV_PARAM_NAME, OSSL_PARAM_UTF8_PTR, NULL, 0),
    OSSL_PARAM_DEFN(OSSL_PROV_PARAM_VERSION, OSSL_PARAM_UTF8_PTR, NULL, 0),
    OSSL_PARAM_DEFN(OSSL_PROV_PARAM_BUILDINFO, OSSL_PARAM_UTF8_PTR, NULL, 0),
    OSSL_PARAM_DEFN(OSSL_PROV_PARAM_STATUS, OSSL_PARAM_INTEGER, NULL, 0),
    OSSL_PARAM_END
};

/**
 * @brief Describe parameters that can be queried.
 *
 * Just returns the standard provider query-able parameters
 * OSSL_PROV_PARAM_NAME - The name of our provider
 * OSSL_PROV_PARAM_VERSION - The version of this provider build
 * OSSL_PROV_PARAM_BUILDINFO - The configuration it was built with
 * OSSL_PROV_PARAM_STATUS - Weather or not its currently activated
 *
 * @param provctx void *provctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossltest_gettable_params(void *provctx)
{
    return ossltest_param_types;
}

/**
 * @brief Return provider parameters.
 *
 * @param provctx void *provctx.
 * @param params OSSL_PARAM params[].
 * @return int.
 */

static int ossltest_get_params(void *provctx, OSSL_PARAM params[])
{
    OSSL_PARAM *p;

    p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_NAME);
    if (p != NULL && !OSSL_PARAM_set_utf8_ptr(p, "OpenSSL ossltest Provider"))
        return 0;
    p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_VERSION);
    if (p != NULL && !OSSL_PARAM_set_utf8_ptr(p, OPENSSL_VERSION_STR))
        return 0;
    p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_BUILDINFO);
    if (p != NULL && !OSSL_PARAM_set_utf8_ptr(p, OPENSSL_FULL_VERSION_STR))
        return 0;
    p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_STATUS);
    if (p != NULL && !OSSL_PARAM_set_int(p, ossl_prov_is_running()))
        return 0;
    return 1;
}

/**
 * @brief Fill a buffer with deterministic test bytes.
 *
 * @param md unsigned char *md.
 * @param len unsigned int len.
 * @return void.
 */

static void fill_known_data(unsigned char *md, unsigned int len)
{
    unsigned int i;

    for (i = 0; i < len; i++)
        md[i] = (unsigned char)(i & 0xff);
}

/**
 * @brief Initialize the context state for our digests.
 *
 * Because all our digests return known data in this provider
 * this can just be a no-op function.  Its used for each digest we support
 *
 * @param ctx void *ctx.
 * @return int.
 */

static int ossltest_dgst_init(void *ctx)
{
    return 1;
}

/**
 * @brief Process input data to update the digest state.
 * Since this provider returns fixed data for each digest
 * we don't actually have to do any work here, just return 1.
 * This is used for all our provided digests
 *
 * @param ctx void *ctx.
 * @param data const void *data.
 * @param count size_t count.
 * @return int.
 */

static int ossltest_dgst_update(void *ctx, const void *data,
                                size_t count)
{
    return 1;
}

/**
 * @brief Finalize the digest output.
 *
 * provide pre-set data (increasing count) for the MD5 DIGEST
 *
 * @param md unsigned char *md.
 * @param ctx void *ctx.
 * @return int.
 */

static int ossltest_MD5_final(unsigned char *md, void *ctx)
{
    fill_known_data(md, MD5_DIGEST_LENGTH);
    return 1;
}

/**
 * @brief Finalize the digest output.
 *
 * provide pre-set data (increasing count) for the SHA1 DIGEST
 *
 * @param md unsigned char *md.
 * @param ctx void *ctx.
 * @return int.
 */

static int ossltest_SHA1_final(unsigned char *md, void *ctx)
{
    fill_known_data(md, SHA_DIGEST_LENGTH);
    return 1;
}

/**
 * @brief Finalize the digest output.
 *
 * provide pre-set data (increasing count) for the SHA256 DIGEST
 *
 * @param md unsigned char *md.
 * @param ctx void *ctx.
 * @return int.
 */

static int ossltest_SHA256_final(unsigned char *md, void *ctx)
{
    fill_known_data(md, SHA256_DIGEST_LENGTH);
    return 1;
}

/**
 * @brief Finalize the digest output.
 *
 * provide pre-set data (increasing count) for the SHA384 DIGEST
 *
 * @param md unsigned char *md.
 * @param ctx void *ctx.
 * @return int.
 */

static int ossltest_SHA384_final(unsigned char *md, void *ctx)
{
    fill_known_data(md, SHA384_DIGEST_LENGTH);
    return 1;
}

/**
 * @brief Finalize the digest output.
 *
 * provide pre-set data (increasing count) for the SHA512 DIGEST
 *
 * @param md unsigned char *md.
 * @param ctx void *ctx.
 * @return int.
 */

static int ossltest_SHA512_final(unsigned char *md, void *ctx)
{
    fill_known_data(md, SHA512_DIGEST_LENGTH);
    return 1;
}

/*
 * NOTE: These externs are just here to make the compiler happy
 * The IMPLEMENT_digest_functions macros below define these arrays
 * as non-static so that real providers can pick them up in other C files
 * And they get externed in other header files.  But we only use them internally
 * to this provider here.  To avoid having to re-implement and co-ordinate the below
 * macros in what is already a large C file, just define them as extern to prevent some
 * compilers from complaining about a non-static definition with no prior extern declaration
 * mark them as such here.  They won't get exported anyway as p_ossltest only gets built as
 * a DSO, and the linker map we use doesn't list them as exported
 */
extern const OSSL_DISPATCH ossl_testmd5_functions[];
extern const OSSL_DISPATCH ossl_testsha1_functions[];
extern const OSSL_DISPATCH ossl_testsha256_functions[];
extern const OSSL_DISPATCH ossl_testsha384_functions[];
extern const OSSL_DISPATCH ossl_testsha512_functions[];

IMPLEMENT_digest_functions(testmd5, MD5_CTX, MD5_CBLOCK, MD5_DIGEST_LENGTH, 0,
                           ossltest_dgst_init, ossltest_dgst_update, ossltest_MD5_final)

#define SHA2_FLAGS PROV_DIGEST_FLAG_ALGID_ABSENT
IMPLEMENT_digest_functions(testsha1, SHA_CTX, SHA_CBLOCK, SHA_DIGEST_LENGTH, SHA2_FLAGS,
                           ossltest_dgst_init, ossltest_dgst_update, ossltest_SHA1_final)

IMPLEMENT_digest_functions(testsha256, SHA256_CTX,
                           SHA256_CBLOCK, SHA256_DIGEST_LENGTH, SHA2_FLAGS,
                           ossltest_dgst_init, ossltest_dgst_update, ossltest_SHA256_final)

IMPLEMENT_digest_functions(testsha384, SHA512_CTX,
                           SHA512_CBLOCK, SHA512_DIGEST_LENGTH, SHA2_FLAGS,
                           ossltest_dgst_init, ossltest_dgst_update, ossltest_SHA384_final)

IMPLEMENT_digest_functions(testsha512, SHA512_CTX,
                           SHA512_CBLOCK, SHA512_DIGEST_LENGTH, SHA2_FLAGS,
                           ossltest_dgst_init, ossltest_dgst_update, ossltest_SHA512_final)

#define ALG(NAMES, FUNC) \
    { NAMES, "provider=p_ossltest", FUNC }

static const OSSL_ALGORITHM ossltest_digests[] = {
    ALG(PROV_NAMES_MD5, ossl_testmd5_functions),
    ALG(PROV_NAMES_SHA1, ossl_testsha1_functions),
    ALG(PROV_NAMES_SHA2_256, ossl_testsha256_functions),
    ALG(PROV_NAMES_SHA2_384, ossl_testsha384_functions),
    ALG(PROV_NAMES_SHA2_512, ossl_testsha512_functions),
    {NULL, NULL, NULL}
};

typedef struct {
    OSSL_LIB_CTX *libctx;
    EVP_CIPHER_CTX *sub_ctx;
} PROV_EVP_AES128_CBC_CTX;

/**
 * @brief Allocate and initialize a new aes-128-cbc context.
 *
 * @param provctx void *provctx.
 * @return void *.
 */

static void *ossl_testaes128_cbc_newctx(void *provctx)
{
    PROV_EVP_AES128_CBC_CTX *new;
    EVP_CIPHER *cph = NULL;
    int ret;

    if (!ossl_prov_is_running())
        return NULL;

    new = OPENSSL_zalloc(sizeof(PROV_EVP_AES128_CBC_CTX));
    if (new == NULL)
        return NULL;
    new->sub_ctx = EVP_CIPHER_CTX_new();
    if (new->sub_ctx == NULL)
        goto err;

    new->libctx = PROV_LIBCTX_OF(provctx);

    cph = EVP_CIPHER_fetch(new->libctx, "AES-128-CBC", "provider=default");
    if (cph == NULL)
        goto err;

    ret = EVP_CipherInit_ex2(new->sub_ctx, cph, NULL, NULL, 1, NULL);

    EVP_CIPHER_free(cph);

    if (ret <= 0)
        goto err;

    return new;
err:
    EVP_CIPHER_CTX_free(new->sub_ctx);
    OPENSSL_free(new);
    return NULL;
}

/**
 * @brief Release resources and clean up an aes-128-cbc context.
 *
 * @param vprovctx void *vprovctx.
 * @return void.
 */

static void ossl_test_aes128cbc_freectx(void *vprovctx)
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;

    EVP_CIPHER_CTX_free(ctx->sub_ctx);
    OPENSSL_free(ctx);
}

/**
 * @brief Duplicate an aes-128-cbc context.
 *
 * @param vprovctx void *vprovctx.
 * @return void *.
 */

static void *ossl_test_aes128cbc_dupctx(void *vprovctx)
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;
    PROV_EVP_AES128_CBC_CTX *dup;

    dup = OPENSSL_memdup(ctx, sizeof(PROV_EVP_AES128_CBC_CTX));

    if (dup != NULL) {
        dup->sub_ctx = EVP_CIPHER_CTX_dup(ctx->sub_ctx);
        if (dup->sub_ctx == NULL) {
            OPENSSL_free(dup);
            dup = NULL;
        }
    }
    return dup;
}

/**
 * @brief Initialize an aes-129-cbc context for encryption.
 *
 * @param vprovctx void *vprovctx.
 * @param key const unsigned char *key.
 * @param keylen size_t keylen.
 * @param iv const unsigned char *iv.
 * @param ivlen size_t ivlen.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbc_einit(void *vprovctx, const unsigned char *key,
                                     size_t keylen, const unsigned char *iv,
                                     size_t ivlen, const OSSL_PARAM params[])
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;

    return EVP_CipherInit_ex2(ctx->sub_ctx, NULL, key, iv, 1, params);
}

/**
 * @brief Initialize an aes-128-cbc context for decryption
 *
 * @param vprovctx void *vprovctx.
 * @param key const unsigned char *key.
 * @param keylen size_t keylen.
 * @param iv const unsigned char *iv.
 * @param ivlen size_t ivlen.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbc_dinit(void *vprovctx, const unsigned char *key,
                                     size_t keylen, const unsigned char *iv,
                                     size_t ivlen, const OSSL_PARAM params[])
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;

    return EVP_CipherInit_ex2(ctx->sub_ctx, NULL, key, iv, 0, params);
}

/**
 * @brief en/decrypt data for aes-128-cbc.
 *
 *
 * @param vprovctx void *vprovctx.
 * @param out char *out.
 * @param outl size_t *outl.
 * @param outsize size_t outsize.
 * @param in const unsigned char *in.
 * @param inl size_t inl.
 * @return int.
 */

static int ossl_test_aes128cbc_update(void *vprovctx, char *out, size_t *outl,
                                      size_t outsize, const unsigned char *in,
                                      size_t inl)
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;
    int soutl;
    int padnum;
    unsigned char padval;
    size_t loop;
    uint8_t *inbuf = NULL;
    int ret = 0;

    *outl = 0;

    if (EVP_CIPHER_CTX_is_encrypting(ctx->sub_ctx)) {
        /*
         * On encrypt, Make sure output buffer is large enough to hold the
         * crypto result plus any needed padding, keeping in mind that for
         * inputs less than the block size (16), we pad to the remainder of this
         * block (i.e. up to 15 bytes). For inputs equal to the block size, we
         * add an additional block of padding (16 bytes).
         */
        if (outsize < inl + (16 - (inl % 16)))
            goto err;
    } else {
        /*
         * On decrypt we just need to make sure the output buffer is at least
         * as large as the input buffer, to store the result.
         */
        if (outsize < inl)
            goto err;
    }

    /*
     * record our input buffer
     */
    inbuf = OPENSSL_zalloc(inl);
    if (inbuf == NULL)
        return 0;

    memcpy(inbuf, in, inl);

    soutl = EVP_Cipher(ctx->sub_ctx, (unsigned char *)out, in, (unsigned int)inl);

    if (soutl <= 0)
        goto err;

    /*
     * replace the ciphertext with our plain text
     */
    memcpy(out, inbuf, inl);

    if (EVP_CIPHER_CTX_is_encrypting(ctx->sub_ctx)) {
        padnum = (int)(16 - (inl % 16));
        padval = (unsigned char)(padnum - 1);
        if (((size_t)(soutl + padnum)) > outsize)
            goto err;

        for (loop = inl; loop < inl + padnum; loop++)
            out[loop] = padval;
        soutl += padnum;
    } else {
        /*
         * on decrypt the last byte in the buffer should be
         * padval, which is the number of padding bytes - 1;
         */
        padnum = out[inl - 1];

        /*
         * Make sure the soutl doesn't go negative
         */
        if (soutl <= padnum + 16)
            goto err;

        soutl -= padnum + 1;
        /*
         * shorten by explicit iv length
         */
        soutl -= 16;
    }

    ret = 1;
    *outl = soutl;
err:
    OPENSSL_free(inbuf);
    return ret;
}

/**
 * @brief Finalize the operation and produce any remaining output for aes-cbc-128
 *
 * @param vprovctx void *vprovctx.
 * @param out unsigned char *out.
 * @param outl size_t *outl.
 * @param outsize size_t outsize.
 * @return int.
 */

static int ossl_test_aes128cbc_final(void *vprovctx, unsigned char *out, size_t *outl,
                                     size_t outsize)
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;
    int soutl;
    int ret;

    ret = EVP_CipherFinal_ex(ctx->sub_ctx, out, &soutl);

    return ret;
}

/**
 * @brief Implement ossl test aes128cbc cipher.
 *
 * Note, nothing in TLS should be using this function, as we are a provider
 * we just need it for completeness.
 *
 * @param vprovctx void *vprovctx.
 * @param out unsigned char *out.
 * @param outl size_t *outl.
 * @param outsize size_t outsize.
 * @param in const unsigned char *in.
 * @param inl size_t inl.
 * @return int.
 */

static int ossl_test_aes128cbc_cipher(void *vprovctx, unsigned char *out, size_t *outl,
                                      size_t outsize, const unsigned char *in, size_t inl)
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;

    return EVP_Cipher(ctx->sub_ctx, out, in, (int) inl);
}

/**
 * @brief Return provider or algorithm parameters.
 *
 * @param params OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbc_get_params(OSSL_PARAM params[])
{
    return ossl_cipher_generic_get_params(params, EVP_CIPH_CBC_MODE, 0, 128, 128, 128);
}

/**
 * @brief Query parameters from the aes-128-cbc context.
 *
 * @param vprovctx void *vprovctx.
 * @param params OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbc_get_ctx_params(void *vprovctx, OSSL_PARAM params[])
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;

    return EVP_CIPHER_CTX_get_params(ctx->sub_ctx, params);
}

/**
 * @brief Set parameters on the aes-128-cbc context.
 *
 * @param vprovctx void *vprovctx.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbc_set_ctx_params(void *vprovctx, const OSSL_PARAM params[])
{
    /*
     * Normally this gets called to set
     * OSSL_CIPHER_PARAM_TLS_VERSION
     * OSSL_CIPHER_PARAM_TLS_MAC_SIZE
     * but we don't want the underlying cipher to do that, we will
     * handle that padding in cbc_update on our own, so intercept and
     * squash that here
     */
    return 1;
}

/**
 * @brief Describe parameters that can be queried for aes-128-cbc
 *
 * @param vprovctx void *vprovctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossl_test_aes128cbc_gettable_params(void *vprovctx)
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;

    return EVP_CIPHER_gettable_params(EVP_CIPHER_CTX_get0_cipher(ctx->sub_ctx));
}

/**
 * @brief Describe context parameters that can be queried for aes-128-cbc.
 *
 * @param cctx void *cctx.
 * @param vprovctx void *vprovctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossl_test_aes128cbc_gettable_ctx_params(void *cctx, void *vprovctx)
{
    return ossl_cipher_generic_gettable_ctx_params(cctx, vprovctx);
}

/**
 * @brief Describe context parameters that can be set for aes-128-cbc.
 *
 * @param cctx void *cctx.
 * @param vprovctx void *vprovctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossl_test_aes128cbc_settable_ctx_params(void *cctx, void *vprovctx)
{
    PROV_EVP_AES128_CBC_CTX *ctx = (PROV_EVP_AES128_CBC_CTX *)vprovctx;

    return EVP_CIPHER_CTX_settable_params(ctx->sub_ctx);
}

static const OSSL_DISPATCH ossl_testaes128_cbc_functions[] = {
    { OSSL_FUNC_CIPHER_NEWCTX,
      (void (*)(void)) ossl_testaes128_cbc_newctx },
    { OSSL_FUNC_CIPHER_FREECTX, (void (*)(void)) ossl_test_aes128cbc_freectx },
    { OSSL_FUNC_CIPHER_DUPCTX, (void (*)(void)) ossl_test_aes128cbc_dupctx },
    { OSSL_FUNC_CIPHER_ENCRYPT_INIT, (void (*)(void))ossl_test_aes128cbc_einit },
    { OSSL_FUNC_CIPHER_DECRYPT_INIT, (void (*)(void))ossl_test_aes128cbc_dinit },
    { OSSL_FUNC_CIPHER_UPDATE, (void (*)(void))ossl_test_aes128cbc_update },
    { OSSL_FUNC_CIPHER_FINAL, (void (*)(void))ossl_test_aes128cbc_final },
    { OSSL_FUNC_CIPHER_CIPHER, (void (*)(void))ossl_test_aes128cbc_cipher },
    { OSSL_FUNC_CIPHER_GET_PARAMS,
      (void (*)(void)) ossl_test_aes128cbc_get_params },
    { OSSL_FUNC_CIPHER_GET_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128cbc_get_ctx_params },
    { OSSL_FUNC_CIPHER_SET_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128cbc_set_ctx_params },
    { OSSL_FUNC_CIPHER_GETTABLE_PARAMS,
      (void (*)(void))ossl_test_aes128cbc_gettable_params },
    { OSSL_FUNC_CIPHER_GETTABLE_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128cbc_gettable_ctx_params },
    { OSSL_FUNC_CIPHER_SETTABLE_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128cbc_settable_ctx_params },
    OSSL_DISPATCH_END
};

typedef struct {
    OSSL_LIB_CTX *libctx;
    EVP_CIPHER_CTX *sub_ctx;
} PROV_EVP_AES128_GCM_CTX;

/**
 * @brief Allocate and initialize a new algorithm context for aes-128-gcm.
 *
 * @param provctx void *provctx.
 * @return void *.
 */

static void *ossl_testaes128_gcm_newctx(void *provctx)
{
    PROV_EVP_AES128_GCM_CTX *new;
    EVP_CIPHER *cph = NULL;
    int ret;

    if (!ossl_prov_is_running())
        return NULL;

    new = OPENSSL_zalloc(sizeof(PROV_EVP_AES128_GCM_CTX));
    if (new == NULL)
        return NULL;

    new->sub_ctx = EVP_CIPHER_CTX_new();
    if (new->sub_ctx == NULL)
        goto err;

    new->libctx = PROV_LIBCTX_OF(provctx);

    cph = EVP_CIPHER_fetch(new->libctx, "aes-128-gcm", "provider=default");
    if (cph == NULL)
        goto err;

    ret = EVP_EncryptInit_ex2(new->sub_ctx, cph, NULL, NULL, NULL);
    EVP_CIPHER_free(cph);

    if (ret <= 0)
        goto err;

    return new;
err:
    EVP_CIPHER_CTX_free(new->sub_ctx);
    OPENSSL_free(new);
    return NULL;

}

/**
 * @brief Release resources and clean up the context for aes-128-gcm.
 *
 * @param vprovctx void *vprovctx.
 * @return void.
 */

static void ossl_test_aes128gcm_freectx(void *vprovctx)
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;

    EVP_CIPHER_CTX_free(ctx->sub_ctx);
    OPENSSL_free(ctx);
}

/**
 * @brief Duplicate an aes-128-gcm context.
 *
 * @param vprovctx void *vprovctx.
 * @return void *.
 */

static void *ossl_test_aes128gcm_dupctx(void *vprovctx)
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;
    PROV_EVP_AES128_GCM_CTX *dup;

    dup = OPENSSL_memdup(ctx, sizeof(PROV_EVP_AES128_GCM_CTX));

    if (dup != NULL) {
        dup->sub_ctx = EVP_CIPHER_CTX_dup(ctx->sub_ctx);
        if (dup->sub_ctx == NULL) {
            OPENSSL_free(dup);
            dup = NULL;
        }
    }
    return dup;
}

/**
 * @brief Initialize the aes-128-gcm encryption operation context.
 *
 * @param vprovctx void *vprovctx.
 * @param key const unsigned char *key.
 * @param keylen size_t keylen.
 * @param iv const unsigned char *iv.
 * @param ivlen size_t ivlen.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128gcm_einit(void *vprovctx, const unsigned char *key,
                                     size_t keylen, const unsigned char *iv,
                                     size_t ivlen, const OSSL_PARAM params[])
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;

    return EVP_EncryptInit_ex2(ctx->sub_ctx, NULL, key, iv, params);
}

/**
 * @brief Initialize the aes-128-gcm decryption operation context.
 *
 * @param vprovctx void *vprovctx.
 * @param key const unsigned char *key.
 * @param keylen size_t keylen.
 * @param iv const unsigned char *iv.
 * @param ivlen size_t ivlen.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128gcm_dinit(void *vprovctx, const unsigned char *key,
                                     size_t keylen, const unsigned char *iv,
                                     size_t ivlen, const OSSL_PARAM params[])
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;

    return EVP_DecryptInit_ex2(ctx->sub_ctx, NULL, key, iv, params);
}

/**
 * @brief en/decrypt aes-128-gcm data.
 *
 * @param vprovctx void *vprovctx.
 * @param out char *out.
 * @param outl size_t *outl.
 * @param outsize size_t outsize.
 * @param in const unsigned char *in.
 * @param inl size_t inl.
 * @return int.
 */

static int ossl_test_aes128gcm_update(void *vprovctx, char *out, size_t *outl,
                                      size_t outsize, const unsigned char *in,
                                      size_t inl)
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;
    int ret, soutl;
    uint8_t *inbuf;

    inbuf = OPENSSL_memdup(in, inl);

    if (EVP_CIPHER_CTX_is_encrypting(ctx->sub_ctx))
        ret = EVP_EncryptUpdate(ctx->sub_ctx, (unsigned char *)out,
                                &soutl, in, (int)inl);
    else
        ret = EVP_DecryptUpdate(ctx->sub_ctx, (unsigned char *)out,
                                &soutl, in, (int)inl);
    *outl = soutl;

    /*
     * Once the cipher is complete, throw it away and use the
     * plaintext as our output
     */
    if (inbuf != NULL && out != NULL)
        memcpy(out, inbuf, inl);
    OPENSSL_free(inbuf);

    return ret;
}

/**
 * @brief Finalize the aes-128-gcm en/decryption and produce any remaining output.
 *
 * @param vprovctx void *vprovctx.
 * @param out unsigned char *out.
 * @param outl size_t *outl.
 * @param outsize size_t outsize.
 * @return int.
 */

static int ossl_test_aes128gcm_final(void *vprovctx, unsigned char *out, size_t *outl,
                                     size_t outsize)
{
    int ret;

    *outl = 0;
    ret = 1;
    return ret;
}

/**
 * @brief Implement ossl test aes128gcm cipher.
 *
 * @param vprovctx void *vprovctx.
 * @param out unsigned char *out.
 * @param outl size_t *outl.
 * @param outsize size_t outsize.
 * @param in const unsigned char *in.
 * @param inl size_t inl.
 * @return int.
 */

static int ossl_test_aes128gcm_cipher(void *vprovctx, unsigned char *out, size_t *outl,
                                      size_t outsize, const unsigned char *in, size_t inl)
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;

    return EVP_Cipher(ctx->sub_ctx, out, in, (int) inl);
}

#define AEAD_FLAGS (PROV_CIPHER_FLAG_AEAD | PROV_CIPHER_FLAG_CUSTOM_IV)

/**
 * @brief Return aes-128-gcm parameters.
 *
 * @param params OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128gcm_get_params(OSSL_PARAM params[])
{
    return ossl_cipher_generic_get_params(params, EVP_CIPH_GCM_MODE, AEAD_FLAGS, 128, 8, 96);
}

/**
 * @brief Query parameters from the aes-128-gcm context.
 *
 * @param vprovctx void *vprovctx.
 * @param params OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128gcm_get_ctx_params(void *vprovctx, OSSL_PARAM params[])
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;
    OSSL_PARAM *p;
    int ret;
    uint8_t *tagval = NULL;
    size_t taglen;

    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_AEAD_TAG);
    if (p != NULL) {
        if (OSSL_PARAM_get_octet_string_ptr(p, (void *)&tagval, &taglen))
            memset(tagval, 0, taglen);
        ret = 1;
    } else {
        ret = EVP_CIPHER_CTX_get_params(ctx->sub_ctx, params);
    }

    return ret;

}

/**
 * @brief Set parameters on the aes-128-gcm context.
 *
 * @param vprovctx void *vprovctx.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128gcm_set_ctx_params(void *vprovctx, const OSSL_PARAM params[])
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;

    return EVP_CIPHER_CTX_set_params(ctx->sub_ctx, params);
}

/**
 * @brief Describe parameters that can be queried aes-128-gcm.
 *
 * @param vprovctx void *vprovctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossl_test_aes128gcm_gettable_params(void *vprovctx)
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;

    return EVP_CIPHER_gettable_params(EVP_CIPHER_CTX_get0_cipher(ctx->sub_ctx));
}

/**
 * @brief aes-128-gcm context parameters that can be queried.
 *
 * @param cctx void *cctx.
 * @param vprovctx void *vprovctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossl_test_aes128gcm_gettable_ctx_params(void *cctx, void *vprovctx)
{
    return ossl_cipher_generic_gettable_ctx_params(cctx, vprovctx);
}

/**
 * @brief Describe aes-128-gcm context parameters that can be set.
 *
 * @param cctx void *cctx.
 * @param vprovctx void *vprovctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossl_test_aes128gcm_settable_ctx_params(void *cctx, void *vprovctx)
{
    PROV_EVP_AES128_GCM_CTX *ctx = (PROV_EVP_AES128_GCM_CTX *)vprovctx;

    return EVP_CIPHER_CTX_settable_params(ctx->sub_ctx);
}

static const OSSL_DISPATCH ossl_testaes128_gcm_functions[] = {
    { OSSL_FUNC_CIPHER_NEWCTX,
      (void (*)(void)) ossl_testaes128_gcm_newctx },
    { OSSL_FUNC_CIPHER_FREECTX, (void (*)(void)) ossl_test_aes128gcm_freectx },
    { OSSL_FUNC_CIPHER_DUPCTX, (void (*)(void)) ossl_test_aes128gcm_dupctx },
    { OSSL_FUNC_CIPHER_ENCRYPT_INIT, (void (*)(void))ossl_test_aes128gcm_einit },
    { OSSL_FUNC_CIPHER_DECRYPT_INIT, (void (*)(void))ossl_test_aes128gcm_dinit },
    { OSSL_FUNC_CIPHER_UPDATE, (void (*)(void))ossl_test_aes128gcm_update },
    { OSSL_FUNC_CIPHER_FINAL, (void (*)(void))ossl_test_aes128gcm_final },
    { OSSL_FUNC_CIPHER_CIPHER, (void (*)(void))ossl_test_aes128gcm_cipher },
    { OSSL_FUNC_CIPHER_GET_PARAMS,
      (void (*)(void)) ossl_test_aes128gcm_get_params },
    { OSSL_FUNC_CIPHER_GET_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128gcm_get_ctx_params },
    { OSSL_FUNC_CIPHER_SET_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128gcm_set_ctx_params },
    { OSSL_FUNC_CIPHER_GETTABLE_PARAMS,
      (void (*)(void))ossl_test_aes128gcm_gettable_params },
    { OSSL_FUNC_CIPHER_GETTABLE_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128gcm_gettable_ctx_params },
    { OSSL_FUNC_CIPHER_SETTABLE_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128gcm_settable_ctx_params },
    OSSL_DISPATCH_END
};

#define NO_PAYLOAD_LENGTH ((size_t)-1)

typedef struct {
    OSSL_LIB_CTX *libctx;
    int encrypting;
    size_t payload_length;
    unsigned int tls_ver;
    size_t pad_size;
} PROV_EVP_AES128_CBC_HMAC_SHA1_CTX;

/**
 * @brief Allocate and initialize a new aes-128-cbc-hmac-sha1 algorithm context.
 *
 * @param provctx void *provctx.
 * @return a provider aes-128-cbc-hmac-sha1 context as a void pointer.
 */

static void *ossl_testaes128cbchmacsha1_newctx(void *provctx)
{
    PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *new;

    if (!ossl_prov_is_running())
        return NULL;

    new = OPENSSL_zalloc(sizeof(PROV_EVP_AES128_CBC_HMAC_SHA1_CTX));
    if (new == NULL)
        return NULL;

    new->libctx = PROV_LIBCTX_OF(provctx);
    new->payload_length = NO_PAYLOAD_LENGTH;

    return new;
}

/**
 * @brief Release resources and clean up the aes-128-cbc-hmac-sha1 context.
 *
 * @param vprovctx void *vprovctx.
 * @return void.
 */

static void ossl_test_aes128cbchmacsha1_freectx(void *vprovctx)
{
    PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *ctx = (PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *)vprovctx;

    OPENSSL_free(ctx);
    return;
}

/**
 * @brief Duplicate an aes-128-cbc-hmac-sha1 algorithm context.
 *
 * @param vprovctx void *vprovctx.
 * @return a new aes-128-cbc-hmac-sha1 context as a void pointer.
 */

static void *ossl_test_aes128cbchmacsha1_dupctx(void *vprovctx)
{
    PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *ctx = (PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *)vprovctx;

    return OPENSSL_memdup(ctx, sizeof(PROV_EVP_AES128_CBC_HMAC_SHA1_CTX));
}

/**
 * @brief Initialize the aes-128-cbc-hmac-sha1 context for encryption.
 *
 * @param vprovctx void *vprovctx.
 * @param key const unsigned char *key.
 * @param keylen size_t keylen.
 * @param iv const unsigned char *iv.
 * @param ivlen size_t ivlen.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbchmacsha1_einit(void *vprovctx, const unsigned char *key,
                                             size_t keylen, const unsigned char *iv,
                                             size_t ivlen, const OSSL_PARAM params[])
{
    PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *ctx = (PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *)vprovctx;

    ctx->payload_length = NO_PAYLOAD_LENGTH;
    ctx->encrypting = 1;
    return 1;
}

/**
 * @brief Initialize the aes-128-cbc-hmac-sha1 context for encryption.
 *
 * @param vprovctx void *vprovctx.
 * @param key const unsigned char *key.
 * @param keylen size_t keylen.
 * @param iv const unsigned char *iv.
 * @param ivlen size_t ivlen.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbchmacsha1_dinit(void *vprovctx, const unsigned char *key,
                                             size_t keylen, const unsigned char *iv,
                                             size_t ivlen, const OSSL_PARAM params[])
{
    PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *ctx = (PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *)vprovctx;

    ctx->payload_length = NO_PAYLOAD_LENGTH;
    ctx->encrypting = 0;
    return 1;
}

/**
 * @brief do en/decryption for aes-128-cbc-hmac-sha1
 *
 * @param vprovctx void *vprovctx.
 * @param out unsigned char *out.
 * @param outl size_t *outl.
 * @param outsize size_t outsize.
 * @param in const unsigned char *in.
 * @param inl size_t inl.
 * @return int.
 */

static int ossl_test_aes128cbchmacsha1_update(void *vprovctx, unsigned char *out, size_t *outl,
                                              size_t outsize, const unsigned char *in,
                                              size_t inl)
{
    PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *ctx = (PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *)vprovctx;
    size_t l;
    size_t plen = ctx->payload_length;
    size_t orig_inl = inl;
    const unsigned char *outtmp;

    if (ctx->encrypting) {
        if (plen == NO_PAYLOAD_LENGTH)
            plen = inl;
        else if (inl !=
                 ((plen + SHA_DIGEST_LENGTH +
                   AES_BLOCK_SIZE) & (-AES_BLOCK_SIZE)))
            return 0;

        memmove(out, in, plen);

        if (plen != inl) {      /* "TLS" mode of operation */
            /* calculate HMAC and append it to payload */
            fill_known_data(out + plen, SHA_DIGEST_LENGTH);

            /* pad the payload|hmac */
            plen += SHA_DIGEST_LENGTH;
            for (l = (unsigned int)(inl - plen - 1); plen < inl; plen++)
                out[plen] = (unsigned char)l;
        }
    } else {
        /* decrypt HMAC|padding at once */
        memmove(out, in, inl);

        if (plen != NO_PAYLOAD_LENGTH) { /* "TLS" mode of operation */
            unsigned int maxpad, pad;

            if (ctx->tls_ver >= TLS1_1_VERSION) {
                if (inl < (AES_BLOCK_SIZE + SHA_DIGEST_LENGTH + 1))
                    return 0;

                /* omit explicit iv */
                in += AES_BLOCK_SIZE;
                inl -= AES_BLOCK_SIZE;
            } else {
                if (inl < (SHA_DIGEST_LENGTH + 1))
                    return 0;
            }

            outtmp = out + AES_BLOCK_SIZE;
            /* figure out payload length */
            pad = outtmp[inl - 1];
            maxpad = (unsigned int)(inl - (SHA_DIGEST_LENGTH + 1));
            if (pad > maxpad)
                return 0;
            for (plen = inl - pad - 1; plen < inl; plen++)
                if (outtmp[plen] != pad)
                    return 0;
            /*
             * We need to return the actual dummied up payload of this
             * operation, so reduce the length of the output by the padding
             * size that we just stripped as well as the leading IV, which
             * is the first AES_BLOCK_SIZE bytes
             */
            orig_inl -= pad + 1 + SHA_DIGEST_LENGTH;
            orig_inl -= AES_BLOCK_SIZE;
        }
    }
    *outl = orig_inl;
    return 1;
}

/**
 * @brief finalize aes-128-cbc-hmac-sha1 en/decrypt operation
 *
 * @param vprovctx void *vprovctx.
 * @param out unsigned char *out.
 * @param outl size_t *outl.
 * @param outsize size_t outsize.
 * @return int.
 */

static int ossl_test_aes128cbchmacsha1_final(void *vprovctx, unsigned char *out, size_t *outl,
                                             size_t outsize)
{
    /*
     * Since we don't do any real en/decryption in this alg, this is a no-op
     * so just return success
     */
    return 1;
}

/**
 * @brief Implement ossl test aes128cbchmacsha1 cipher.
 *
 * @param vprovctx void *vprovctx.
 * @param out unsigned char *out.
 * @param outl size_t *outl.
 * @param outsize size_t outsize.
 * @param in const unsigned char *in.
 * @param inl size_t inl.
 * @return int.
 */

static int ossl_test_aes128cbchmacsha1_cipher(void *vprovctx,
                                              unsigned char *out, size_t *outl,
                                              size_t outsize, const unsigned char *in, size_t inl)
{
    return 1;
}

#define AES_CBC_HMAC_SHA_FLAGS (PROV_CIPHER_FLAG_AEAD | PROV_CIPHER_FLAG_TLS1_MULTIBLOCK)
/**
 * @brief Return aes-128-cbc-hmac-sha1 gettable parameters.
 *
 * @param params OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbchmacsha1_get_params(OSSL_PARAM params[])
{
    int ret;

    ret = ossl_cipher_generic_get_params(params, EVP_CIPH_CBC_MODE,
                                         AES_CBC_HMAC_SHA_FLAGS, 128, 128, 128);

    return ret;
}

/**
 * @brief Query aes-128-cbc-hmac-sha1 context parameters.
 *
 * @param vprovctx void *vprovctx.
 * @param params OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbchmacsha1_get_ctx_params(void *vprovctx, OSSL_PARAM params[])
{
    PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *ctx = (PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *)vprovctx;
    OSSL_PARAM *p;

    /*
     * lifted from aes_get_ctx_params
     */
#if !defined(OPENSSL_NO_MULTIBLOCK)
    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_MAX_BUFSIZE);
    if (p != NULL) {
        /*
         * Don't know how to handle any of these
         */
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_INTERLEAVE);
    if (p != NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_AAD_PACKLEN);
    if (p != NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_ENC_LEN);
    if (p != NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
#endif /* !defined(OPENSSL_NO_MULTIBLOCK) */

    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_AEAD_TLS1_AAD_PAD);
    if (p != NULL && !OSSL_PARAM_set_size_t(p, ctx->pad_size)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_KEYLEN);
    if (p != NULL && !OSSL_PARAM_set_size_t(p, 128 / 8)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_IVLEN);
    if (p != NULL && !OSSL_PARAM_set_size_t(p, 128 / 8)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_IV);
    if (p != NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_UPDATED_IV);
    if (p != NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    return 1;
}

/**
 * @brief Set aes-128-cbc-hmac-sha1 context parameters.
 *
 * @param vprovctx void *vprovctx.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int ossl_test_aes128cbchmacsha1_set_ctx_params(void *vprovctx, const OSSL_PARAM params[])
{
    PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *ctx = (PROV_EVP_AES128_CBC_HMAC_SHA1_CTX *)vprovctx;
    OSSL_PARAM *p;
    uint8_t *val;
    size_t vlen;
    unsigned int len;

    p = OSSL_PARAM_locate((OSSL_PARAM *)params, OSSL_CIPHER_PARAM_AEAD_TLS1_AAD);
    if (p != NULL) {
        if (OSSL_PARAM_get_octet_string_ptr(p, (const void **)&val, &vlen) != 1)
            return 0;
        len = val[EVP_AEAD_TLS1_AAD_LEN - 2] << 8 | val[EVP_AEAD_TLS1_AAD_LEN - 1];
        ctx->tls_ver = val[EVP_AEAD_TLS1_AAD_LEN - 4] << 8 | val[EVP_AEAD_TLS1_AAD_LEN -3];

        if (ctx->encrypting) {
            ctx->payload_length = len;
            if (ctx->tls_ver >= TLS1_1_VERSION) {
                if (len < AES_BLOCK_SIZE)
                    return 0;
                len -= AES_BLOCK_SIZE;
                val[EVP_AEAD_TLS1_AAD_LEN - 2] = len >> 8;
                val[EVP_AEAD_TLS1_AAD_LEN - 1] = len;
                ctx->pad_size = ((len + SHA_DIGEST_LENGTH +
                                  AES_BLOCK_SIZE) & (-AES_BLOCK_SIZE)) - len;
            }
        } else {
            ctx->payload_length = EVP_AEAD_TLS1_AAD_LEN;
            ctx->pad_size = SHA_DIGEST_LENGTH;
        }

    }

    return 1;
}

/**
 * @brief Describe parameters that can be queried for aes-128-cbc-hmac-sha1.
 *
 * @param vprovctx void *vprovctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossl_test_aes128cbchmacsha1_gettable_params(void *vprovctx)
{
    return NULL;
}

/**
 * @brief Describe context parameters that can be queried for an aes-128-cbc-hamc-sha1 ctx.
 *
 * @param cctx void *cctx.
 * @param vprovctx void *vprovctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossl_test_aes128cbchmacsha1_gettable_ctx_params(void *cctx, void *vprovctx)
{
    return ossl_cipher_generic_gettable_ctx_params(cctx, vprovctx);
}

/**
 * @brief Describe context parameters that can be set on an aes-128-cbc-hmac-sha1 ctx.
 *
 * @param cctx void *cctx.
 * @param vprovctx void *vprovctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *ossl_test_aes128cbchmacsha1_settable_ctx_params(void *cctx, void *vprovctx)
{
    return NULL;
}

static const OSSL_DISPATCH ossl_testaes128cbchmacsha1_functions[] = {
    { OSSL_FUNC_CIPHER_NEWCTX,
      (void (*)(void)) ossl_testaes128cbchmacsha1_newctx },
    { OSSL_FUNC_CIPHER_FREECTX, (void (*)(void)) ossl_test_aes128cbchmacsha1_freectx },
    { OSSL_FUNC_CIPHER_DUPCTX, (void (*)(void)) ossl_test_aes128cbchmacsha1_dupctx },
    { OSSL_FUNC_CIPHER_ENCRYPT_INIT, (void (*)(void))ossl_test_aes128cbchmacsha1_einit },
    { OSSL_FUNC_CIPHER_DECRYPT_INIT, (void (*)(void))ossl_test_aes128cbchmacsha1_dinit },
    { OSSL_FUNC_CIPHER_UPDATE, (void (*)(void))ossl_test_aes128cbchmacsha1_update },
    { OSSL_FUNC_CIPHER_FINAL, (void (*)(void))ossl_test_aes128cbchmacsha1_final },
    { OSSL_FUNC_CIPHER_CIPHER, (void (*)(void))ossl_test_aes128cbchmacsha1_cipher },
    { OSSL_FUNC_CIPHER_GET_PARAMS,
      (void (*)(void)) ossl_test_aes128cbchmacsha1_get_params },
    { OSSL_FUNC_CIPHER_GET_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128cbchmacsha1_get_ctx_params },
    { OSSL_FUNC_CIPHER_SET_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128cbchmacsha1_set_ctx_params },
    { OSSL_FUNC_CIPHER_GETTABLE_PARAMS,
      (void (*)(void))ossl_test_aes128cbchmacsha1_gettable_params },
    { OSSL_FUNC_CIPHER_GETTABLE_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128cbchmacsha1_gettable_ctx_params },
    { OSSL_FUNC_CIPHER_SETTABLE_CTX_PARAMS,
      (void (*)(void))ossl_test_aes128cbchmacsha1_settable_ctx_params },
    OSSL_DISPATCH_END
};

static const OSSL_ALGORITHM ossltest_ciphers[] = {
    ALG(PROV_NAMES_AES_128_CBC, ossl_testaes128_cbc_functions),
    ALG(PROV_NAMES_AES_128_GCM, ossl_testaes128_gcm_functions),
    ALG(PROV_NAMES_AES_128_CBC_HMAC_SHA1, ossl_testaes128cbchmacsha1_functions),
    {NULL, NULL, NULL}
};

typedef struct ossl_test_rand_ctx {
    size_t unused;
} OSSL_TEST_RAND_CTX;

/**
 * @brief Implement drbg ctr new wrapper.
 *
 * @param provctx void *provctx.
 * @param parent void *parent.
 * @param parent_dispatch const OSSL_DISPATCH *parent_dispatch.
 * @return void *.
 */

static void *drbg_ctr_new_wrapper(void *provctx, void *parent,
                                  const OSSL_DISPATCH *parent_dispatch)
{
    return OPENSSL_zalloc(sizeof(OSSL_TEST_RAND_CTX));
}

/**
 * @brief Release resources and clean up the context.
 *
 * @param vdrbg void *vdrbg.
 * @return void.
 */

static void drbg_ctr_free(void *vdrbg)
{
    OPENSSL_free(vdrbg);
}

/**
 * @brief Instantiate the CTR DRBG with the given inputs.
 *
 * @param vdrbg void *vdrbg.
 * @param strength unsigned int strength.
 * @param prediction_resistance int prediction_resistance.
 * @param pstr const unsigned char *pstr.
 * @param pstr_len size_t pstr_len.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int drbg_ctr_instantiate_wrapper(void *vdrbg, unsigned int strength,
                                        int prediction_resistance,
                                        const unsigned char *pstr,
                                        size_t pstr_len,
                                        const OSSL_PARAM params[])
{
    return 1;
}

/**
 * @brief Instantiate the CTR DRBG with the given inputs.
 *
 * @param vdrbg void *vdrbg.
 * @return int.
 */

static int drbg_ctr_uninstantiate_wrapper(void *vdrbg)
{
    return 1;
}

/**
 * @brief Generate pseudo-random bytes from the CTR DRBG.
 *
 * @param vdrbg void *vdrbg.
 * @param out unsigned char *out.
 * @param outlen size_t outlen.
 * @param strength unsigned int strength.
 * @param prediction_resistance int prediction_resistance.
 * @param adin const unsigned char *adin.
 * @param adin_len size_t adin_len.
 * @return int.
 */

static int drbg_ctr_generate_wrapper(void *vdrbg, unsigned char *out,
                                     size_t outlen, unsigned int strength,
                                     int prediction_resistance,
                                     const unsigned char *adin, size_t adin_len)
{
    unsigned char val = 1;
    size_t copylen = 0;

    while (copylen < outlen) {
        *out++ = val++;
        copylen++;
    }

    return 1;
}

/**
 * @brief Reseed the CTR DRBG with fresh entropy.
 *
 * @param vdrbg void *vdrbg.
 * @param prediction_resistance int prediction_resistance.
 * @param ent const unsigned char *ent.
 * @param ent_len size_t ent_len.
 * @param adin const unsigned char *adin.
 * @param adin_len size_t adin_len.
 * @return int.
 */

static int drbg_ctr_reseed_wrapper(void *vdrbg, int prediction_resistance,
                                   const unsigned char *ent, size_t ent_len,
                                   const unsigned char *adin, size_t adin_len)
{
    return 1;
}

/**
 * @brief Enable internal locking for thread safety.
 *
 * @param vctx void *vctx.
 * @return int.
 */

static int ossl_drbg_enable_locking(void *vctx)
{
    return 1;
}

/**
 * @brief Acquire the internal lock.
 *
 * @param vctx void *vctx.
 * @return int.
 */

static int ossl_drbg_lock(void *vctx)
{
    return 1;
}

/**
 * @brief Release the internal lock.
 *
 * @param vctx void *vctx.
 * @return void.
 */

static void ossl_drbg_unlock(void *vctx)
{
    return;
}

/**
 * @brief Describe context parameters that can be set.
 *
 * @param vctx ossl_unused void *vctx.
 * @param provctx ossl_unused void *provctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *drbg_ctr_settable_ctx_params(ossl_unused void *vctx,
                                                      ossl_unused void *provctx)
{
    return NULL;
}

/**
 * @brief Set parameters on the algorithm context.
 *
 * @param vctx void *vctx.
 * @param params const OSSL_PARAM params[].
 * @return int.
 */

static int drbg_ctr_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    return 1;
}

/**
 * @brief Describe context parameters that can be queried.
 *
 * @param vctx ossl_unused void *vctx.
 * @param provctx ossl_unused void *provctx.
 * @return const OSSL_PARAM *.
 */

static const OSSL_PARAM *drbg_ctr_gettable_ctx_params(ossl_unused void *vctx,
                                                      ossl_unused void *provctx)
{
    return NULL;
}

/**
 * @brief Query parameters from the algorithm context.
 *
 * @param vdrbg void *vdrbg.
 * @param params OSSL_PARAM params[].
 * @return int.
 */

static int drbg_ctr_get_ctx_params(void *vdrbg, OSSL_PARAM params[])
{
    OSSL_PARAM *p = OSSL_PARAM_locate(params, OSSL_RAND_PARAM_MAX_REQUEST);

    if (p != NULL && !OSSL_PARAM_set_size_t(p, (size_t)(1 << 16))) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    return 1;
}

/**
 * @brief Verify that sensitive buffers are cleared to zero.
 *
 * @param vdrbg void *vdrbg.
 * @return int.
 */

static int drbg_ctr_verify_zeroization(void *vdrbg)
{
    return 1;
}

/**
 * @brief Return the stored test seed buffer if present.
 *
 * @param vdrbg void *vdrbg.
 * @param pout unsigned char **pout.
 * @param entropy int entropy.
 * @param min_len size_t min_len.
 * @param max_len size_t max_len.
 * @param prediction_resistance int prediction_resistance.
 * @param adin const unsigned char *adin.
 * @param adin_len size_t adin_len.
 * @return size_t.
 */

static size_t ossl_drbg_get_seed(void *vdrbg, unsigned char **pout,
                                 int entropy, size_t min_len,
                                 size_t max_len, int prediction_resistance,
                                 const unsigned char *adin, size_t adin_len)
{
    size_t needed = entropy;

    if (needed < min_len)
        needed = min_len;
    if (needed > max_len)
        needed = max_len;
    return needed;
}

/**
 * @brief Clear any stored test seed buffer.
 *
 * @param vdrbg ossl_unused void *vdrbg.
 * @param out unsigned char *out.
 * @param outlen size_t outlen.
 * @return void.
 */

static void ossl_drbg_clear_seed(ossl_unused void *vdrbg,
                                 unsigned char *out, size_t outlen)
{
    return;
}

static const OSSL_DISPATCH ossl_test_drbg_ctr_functions[] = {
    { OSSL_FUNC_RAND_NEWCTX, (void(*)(void))drbg_ctr_new_wrapper },
    { OSSL_FUNC_RAND_FREECTX, (void(*)(void))drbg_ctr_free },
    { OSSL_FUNC_RAND_INSTANTIATE,
      (void(*)(void))drbg_ctr_instantiate_wrapper },
    { OSSL_FUNC_RAND_UNINSTANTIATE,
      (void(*)(void))drbg_ctr_uninstantiate_wrapper },
    { OSSL_FUNC_RAND_GENERATE, (void(*)(void))drbg_ctr_generate_wrapper },
    { OSSL_FUNC_RAND_RESEED, (void(*)(void))drbg_ctr_reseed_wrapper },
    { OSSL_FUNC_RAND_ENABLE_LOCKING, (void(*)(void))ossl_drbg_enable_locking },
    { OSSL_FUNC_RAND_LOCK, (void(*)(void))ossl_drbg_lock },
    { OSSL_FUNC_RAND_UNLOCK, (void(*)(void))ossl_drbg_unlock },
    { OSSL_FUNC_RAND_SETTABLE_CTX_PARAMS,
      (void(*)(void))drbg_ctr_settable_ctx_params },
    { OSSL_FUNC_RAND_SET_CTX_PARAMS, (void(*)(void))drbg_ctr_set_ctx_params },
    { OSSL_FUNC_RAND_GETTABLE_CTX_PARAMS,
      (void(*)(void))drbg_ctr_gettable_ctx_params },
    { OSSL_FUNC_RAND_GET_CTX_PARAMS, (void(*)(void))drbg_ctr_get_ctx_params },
    { OSSL_FUNC_RAND_VERIFY_ZEROIZATION,
      (void(*)(void))drbg_ctr_verify_zeroization },
    { OSSL_FUNC_RAND_GET_SEED, (void(*)(void))ossl_drbg_get_seed },
    { OSSL_FUNC_RAND_CLEAR_SEED, (void(*)(void))ossl_drbg_clear_seed },
    OSSL_DISPATCH_END
};

static const OSSL_ALGORITHM ossltest_rands[] = {
    ALG(PROV_NAMES_CTR_DRBG, ossl_test_drbg_ctr_functions),
    {NULL, NULL, NULL}
};

/**
 * @brief Implement ossltest query.
 *
 * @param provctx void *provctx.
 * @param operation_id int operation_id.
 * @param no_cache int *no_cache.
 * @return const OSSL_ALGORITHM *.
 */

static const OSSL_ALGORITHM *ossltest_query(void *provctx, int operation_id,
                                            int *no_cache)
{
    *no_cache = 0;
    switch (operation_id) {
    case OSSL_OP_DIGEST:
        return ossltest_digests;
    case OSSL_OP_CIPHER:
        return ossltest_ciphers;
    case OSSL_OP_RAND:
        return ossltest_rands;
    }
    return NULL;
}

static const OSSL_DISPATCH ossltest_dispatch_table[] = {
    { OSSL_FUNC_PROVIDER_TEARDOWN, (void (*)(void))ossltest_teardown },
    { OSSL_FUNC_PROVIDER_GETTABLE_PARAMS, (void (*)(void))ossltest_gettable_params },
    { OSSL_FUNC_PROVIDER_GET_PARAMS, (void (*)(void))ossltest_get_params },
    { OSSL_FUNC_PROVIDER_QUERY_OPERATION, (void (*)(void))ossltest_query },
    OSSL_DISPATCH_END
};

OSSL_provider_init_fn OSSL_provider_init_int;
/**
 * @brief Initialize the context or operation state.
 *
 * @param handle const OSSL_CORE_HANDLE *handle.
 * @param in const OSSL_DISPATCH *in.
 * @param out const OSSL_DISPATCH **out.
 * @param provctx void **provctx.
 * @return int.
 */
int OSSL_provider_init(const OSSL_CORE_HANDLE *handle,
                       const OSSL_DISPATCH *in,
                       const OSSL_DISPATCH **out,
                       void **provctx)
{
    OSSL_LIB_CTX *libctx = NULL;

    if ((*provctx = ossl_prov_ctx_new()) == NULL
        || (libctx = OSSL_LIB_CTX_new_child(handle, in)) == NULL) {
        OSSL_LIB_CTX_free(libctx);
        ossltest_teardown(*provctx);
        *provctx = NULL;
        return 0;
    }

    ossl_prov_ctx_set0_libctx(*provctx, libctx);
    ossl_prov_ctx_set0_handle(*provctx, handle);

    *out = ossltest_dispatch_table;

    return 1;
}
