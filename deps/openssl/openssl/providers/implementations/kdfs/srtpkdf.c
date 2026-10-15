/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdbool.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/proverr.h>
#include "internal/cryptlib.h"
#include "internal/fips.h"
#include "prov/provider_ctx.h"
#include "prov/providercommon.h"
#include "prov/implementations.h"
#include "prov/provider_util.h"
#include "providers/implementations/kdfs/srtpkdf.inc"

#define KDF_SRTP_AUTH_KEY_LEN 20
#define KDF_SRTP_SALT_KEY_LEN 14
#define KDF_SRTCP_AUTH_KEY_LEN KDF_SRTP_AUTH_KEY_LEN
#define KDF_SRTCP_SALT_KEY_LEN KDF_SRTP_SALT_KEY_LEN
#define KDF_SRTP_SALT_LEN 14
#define KDF_SRTP_KDR_LEN 6
#define KDF_SRTP_IDX_LEN 6
#define KDF_SRTCP_IDX_LEN 4
#define KDF_SRTP_IV_LEN 16
#define KDF_SRTP_MAX_KDR 24
#define KDF_SRTP_MAX_LABEL 7
#define KDF_SRTP_MAX_SALT_LEN (KDF_SRTP_SALT_LEN + 2)

/* See RFC 3711, Section 4.3.3 */
static OSSL_FUNC_kdf_newctx_fn kdf_srtpkdf_new;
static OSSL_FUNC_kdf_dupctx_fn kdf_srtpkdf_dup;
static OSSL_FUNC_kdf_freectx_fn kdf_srtpkdf_free;
static OSSL_FUNC_kdf_reset_fn kdf_srtpkdf_reset;
static OSSL_FUNC_kdf_derive_fn kdf_srtpkdf_derive;
static OSSL_FUNC_kdf_settable_ctx_params_fn kdf_srtpkdf_settable_ctx_params;
static OSSL_FUNC_kdf_set_ctx_params_fn kdf_srtpkdf_set_ctx_params;
static OSSL_FUNC_kdf_gettable_ctx_params_fn kdf_srtpkdf_gettable_ctx_params;
static OSSL_FUNC_kdf_get_ctx_params_fn kdf_srtpkdf_get_ctx_params;

static int SRTPKDF(OSSL_LIB_CTX *provctx, const EVP_CIPHER *cipher,
    const uint8_t *mkey, const uint8_t *msalt,
    const uint8_t *index, size_t index_len,
    const uint32_t kdr, const uint32_t kdr_n,
    const uint32_t label, uint8_t *obuffer, const size_t keylen);

typedef struct {
    /* Warning: Any changes to this structure may require you to update kdf_srtpkdf_dup */
    void *provctx;
    PROV_CIPHER cipher;
    unsigned char *key;
    size_t key_len;
    unsigned char *salt;
    size_t salt_len;
    unsigned char *index;
    size_t index_len;
    uint32_t kdr;
    uint32_t kdr_n; /* 2 ** kdr_n = kdr */
    uint32_t label;
} KDF_SRTPKDF;

static void *kdf_srtpkdf_new(void *provctx)
{
    KDF_SRTPKDF *ctx;

    if (!ossl_prov_is_running())
        return NULL;

#ifdef FIPS_MODULE
    if (!ossl_deferred_self_test(PROV_LIBCTX_OF(provctx),
            ST_ID_KDF_SRTPKDF))
        return NULL;
#endif

    if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) != NULL)
        ctx->provctx = provctx;
    return ctx;
}

static void *kdf_srtpkdf_dup(void *vsrc)
{
    const KDF_SRTPKDF *src = (const KDF_SRTPKDF *)vsrc;
    KDF_SRTPKDF *dest;

    dest = kdf_srtpkdf_new(src->provctx);
    if (dest != NULL) {
        if (!ossl_prov_memdup(src->key, src->key_len,
                &dest->key, &dest->key_len)
            || !ossl_prov_memdup(src->salt, src->salt_len,
                &dest->salt, &dest->salt_len)
            || !ossl_prov_memdup(src->index, src->index_len,
                &dest->index, &dest->index_len)
            || !ossl_prov_cipher_copy(&dest->cipher, &src->cipher))
            goto err;
        dest->kdr = src->kdr;
        dest->kdr_n = src->kdr_n;
        dest->label = src->label;
    }
    return dest;

err:
    kdf_srtpkdf_free(dest);
    return NULL;
}

static void kdf_srtpkdf_free(void *vctx)
{
    KDF_SRTPKDF *ctx = (KDF_SRTPKDF *)vctx;

    if (ctx != NULL) {
        kdf_srtpkdf_reset(ctx);
        OPENSSL_free(ctx);
    }
}

static void kdf_srtpkdf_reset(void *vctx)
{
    KDF_SRTPKDF *ctx = (KDF_SRTPKDF *)vctx;
    void *provctx = ctx->provctx;

    ossl_prov_cipher_reset(&ctx->cipher);
    OPENSSL_clear_free(ctx->key, ctx->key_len);
    OPENSSL_clear_free(ctx->index, ctx->index_len);
    OPENSSL_clear_free(ctx->salt, ctx->salt_len);
    memset(ctx, 0, sizeof(*ctx));
    ctx->provctx = provctx;
}

static int srtpkdf_set_membuf(unsigned char **dst, size_t *dst_len,
    const OSSL_PARAM *p)
{
    OPENSSL_clear_free(*dst, *dst_len);
    *dst = NULL;
    *dst_len = 0;
    return OSSL_PARAM_get_octet_string(p, (void **)dst, 0, dst_len);
}

static int is_power_of_two(uint32_t x, uint32_t *n)
{
    /* Check if we've been given an exact power of two */
    if (x == 0 || (x & (x - 1)) != 0) {
        *n = 0;
        return 0;
    }
    /* Count the number of trailing bits in the passed value */
#ifdef __GNUC__
    *n = __builtin_ctz(x);
#else
    {
        uint32_t count = 0;
        while ((x & 1) == 0) {
            count++;
            x >>= 1;
        }
        *n = count;
    }
#endif
    return 1;
}

static int kdf_srtpkdf_check_key(KDF_SRTPKDF *ctx)
{
    const EVP_CIPHER *cipher = ossl_prov_cipher_cipher(&ctx->cipher);

    if (cipher != NULL) {
        if (ctx->key == NULL) {
            ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_KEY);
            return 0;
        }
        if (ctx->key_len != (size_t)EVP_CIPHER_get_key_length(cipher)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
            return 0;
        }
    }
    return 1;
}

static int kdf_srtpkdf_derive(void *vctx, unsigned char *key, size_t keylen,
    const OSSL_PARAM params[])
{
    KDF_SRTPKDF *ctx = (KDF_SRTPKDF *)vctx;
    const EVP_CIPHER *cipher;
    OSSL_LIB_CTX *libctx;

    if (!ossl_prov_is_running() || !kdf_srtpkdf_set_ctx_params(ctx, params))
        return 0;

    libctx = PROV_LIBCTX_OF(ctx->provctx);

    cipher = ossl_prov_cipher_cipher(&ctx->cipher);
    if (cipher == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_CIPHER);
        return 0;
    }
    if (!kdf_srtpkdf_check_key(ctx))
        return 0;
    if (ctx->salt == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_SALT);
        return 0;
    }
    return SRTPKDF(libctx, cipher, ctx->key, ctx->salt,
        ctx->index, ctx->index_len, ctx->kdr, ctx->kdr_n, ctx->label,
        key, keylen);
}

static int kdf_srtpkdf_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    struct srtp_set_ctx_params_st p;
    KDF_SRTPKDF *ctx = vctx;
    OSSL_LIB_CTX *libctx;

    if (params == NULL)
        return 1;

    if (ctx == NULL || !srtp_set_ctx_params_decoder(params, &p))
        return 0;

    libctx = PROV_LIBCTX_OF(ctx->provctx);

    if (p.cipher != NULL) {
        const EVP_CIPHER *cipher = NULL;

        if (!ossl_prov_cipher_load(&ctx->cipher, p.cipher, p.propq, libctx))
            return 0;
        cipher = ossl_prov_cipher_cipher(&ctx->cipher);
        if (cipher == NULL)
            return 0;
        if (!EVP_CIPHER_is_a(cipher, "AES-128-CTR")
            && !EVP_CIPHER_is_a(cipher, "AES-192-CTR")
            && !EVP_CIPHER_is_a(cipher, "AES-256-CTR")) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_CIPHER);
            return 0;
        }
    }
    if (p.key != NULL) {
        if (!srtpkdf_set_membuf(&ctx->key, &ctx->key_len, p.key))
            return 0;
        if (!kdf_srtpkdf_check_key(ctx))
            return 0;
    }
    if (p.salt != NULL) {
        if (!srtpkdf_set_membuf(&ctx->salt, &ctx->salt_len, p.salt))
            return 0;
        if (ctx->salt_len < KDF_SRTP_SALT_LEN) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_SALT_LENGTH);
            return 0;
        }
    }
    if (p.kdr != NULL) {
        if (!OSSL_PARAM_get_uint32(p.kdr, &ctx->kdr))
            return 0;
        if (ctx->kdr > 0) {
            uint32_t n = 0;

            if (!is_power_of_two(ctx->kdr, &n)
                || n > KDF_SRTP_MAX_KDR) {
                ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KDR);
                return 0;
            }
            ctx->kdr_n = n;
        }
    }

    if (p.label != NULL) {
        if (!OSSL_PARAM_get_uint32(p.label, &ctx->label))
            return 0;
        if (ctx->label > KDF_SRTP_MAX_LABEL) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_LABEL);
            return 0;
        }
    }
    if (p.index != NULL) {
        if (!srtpkdf_set_membuf(&ctx->index, &ctx->index_len, p.index))
            return 0;
        /*
         * Defer checking the index until the derive() since it is dependant
         * on values of kdr and label.
         */
    }

    return 1;
}

static const OSSL_PARAM *kdf_srtpkdf_settable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *p_ctx)
{
    return srtp_set_ctx_params_list;
}

static int kdf_srtpkdf_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    struct srtp_get_ctx_params_st p;
    KDF_SRTPKDF *ctx = vctx;

    if (ctx == NULL || !srtp_get_ctx_params_decoder(params, &p))
        return 0;

    if (p.size != NULL) {
        const EVP_CIPHER *cipher = ossl_prov_cipher_cipher(&ctx->cipher);
        size_t sz;

        if (cipher == NULL)
            return 0;
        sz = EVP_CIPHER_key_length(cipher);

        if (!OSSL_PARAM_set_size_t(p.size, sz))
            return 0;
    }
    return 1;
}

static const OSSL_PARAM *kdf_srtpkdf_gettable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *p_ctx)
{
    return srtp_get_ctx_params_list;
}

const OSSL_DISPATCH ossl_kdf_srtpkdf_functions[] = {
    { OSSL_FUNC_KDF_NEWCTX, (void (*)(void))kdf_srtpkdf_new },
    { OSSL_FUNC_KDF_DUPCTX, (void (*)(void))kdf_srtpkdf_dup },
    { OSSL_FUNC_KDF_FREECTX, (void (*)(void))kdf_srtpkdf_free },
    { OSSL_FUNC_KDF_RESET, (void (*)(void))kdf_srtpkdf_reset },
    { OSSL_FUNC_KDF_DERIVE, (void (*)(void))kdf_srtpkdf_derive },
    { OSSL_FUNC_KDF_SETTABLE_CTX_PARAMS,
        (void (*)(void))kdf_srtpkdf_settable_ctx_params },
    { OSSL_FUNC_KDF_SET_CTX_PARAMS,
        (void (*)(void))kdf_srtpkdf_set_ctx_params },
    { OSSL_FUNC_KDF_GETTABLE_CTX_PARAMS,
        (void (*)(void))kdf_srtpkdf_gettable_ctx_params },
    { OSSL_FUNC_KDF_GET_CTX_PARAMS,
        (void (*)(void))kdf_srtpkdf_get_ctx_params },
    { 0, NULL }
};

static bool is_srtp(uint32_t label)
{
    static const bool strp_table[] = {
        true, /* 0 */
        true, /* 1 */
        true, /* 2 */
        false, /* 3 */
        false, /* 4 */
        false, /* 5 */
        true, /* 6 */
        true, /* 7 */
    };
    return strp_table[label];
}

/*
 * SRTPKDF - In compliance with SP800-135 and RFC3711, calculate
 *           various keys defined by label using a master key,
 *           master salt, kdr(if non-zero) and index.
 *
 * Denote the cryptographic key (encryption key, cipher salt or
 * authentication key(HMAC key), etc) to be derived as K. The
 * length of K is denoted by L. Below is a description of the KDF.
 *
 * master_salt: a random non-salt value.
 * kdr: the key derivation rate. kdr is a number from the set
 *   factor of 2.
 * index: a 48-bit value in RTP or a 32-bit value in RTCP.
 *   See Sections 3.2.1 and 4.3.2 of RFC 3711 for details.
 * A function, DIV, is defined as followed:
 *   a and x are non-negative integers.
 *   a DIV x =  a | x (a DIV x) is represented as a bit string whose
 *   length (in bits) is the same as a.
 * label: an 8-bit value represented by two hexadecimal numbers from
 *   the set of {0x00,0x01, 0x02, 0x03, 0x04, 0x05}.
 *   https://www.ietf.org/archive/id/draft-ietf-avtcore-srtp-encrypted-header-ext-01.html
 *   The values 06 and 07 are used.
 * key_id = label || (index DIV kdr)
 *
 * Input:
 *   cipher - AES cipher
 *   mkey - pointer to master key
 *   msalt - pointer to master salt
 *   index - pointer to index
 *   idxlen - size of the index buffer
 *   kdr - key derivation rate
 *   kdr_n - power of kdr (2**kdr_n = kdr)
 *   label - 8-bit label
 *   keylen - size of obuffer
 * Output:
 *   obuffer - filled with derived key
 *   return - 1 on pass, 0 fail
 */
int SRTPKDF(OSSL_LIB_CTX *provctx, const EVP_CIPHER *cipher,
    const uint8_t *mkey, const uint8_t *msalt,
    const uint8_t *index, size_t idxlen,
    const uint32_t kdr, const uint32_t kdr_n,
    const uint32_t label, uint8_t *obuffer, const size_t keylen)
{
    EVP_CIPHER_CTX *ctx = NULL;
    int outl, i, index_len = 0, o_len = 0, salt_len = 0;
    uint8_t buf[EVP_MAX_KEY_LENGTH];
    uint8_t iv[KDF_SRTP_IV_LEN];
    uint8_t local_salt[KDF_SRTP_MAX_SALT_LEN];
    uint8_t master_salt[KDF_SRTP_MAX_SALT_LEN];
    BIGNUM *bn_index = NULL, *bn_salt = NULL;
    int ret, iv_len = KDF_SRTP_IV_LEN, rv = 0;

    if (obuffer == NULL || keylen > INT_MAX)
        return rv;
    /* get label-specific lengths */
    switch (label) {
    case 0:
    case 3:
    case 6:
        o_len = EVP_CIPHER_key_length(cipher);
        break;
    case 1:
        o_len = KDF_SRTP_AUTH_KEY_LEN;
        break;
    case 4:
        o_len = KDF_SRTCP_AUTH_KEY_LEN;
        break;
    case 2:
    case 7:
        o_len = KDF_SRTP_SALT_KEY_LEN;
        break;
    case 5:
        o_len = KDF_SRTCP_SALT_KEY_LEN;
        break;
    default:
        return rv;
    }
    if (o_len > (int)keylen)
        return rv;

    /* set up a couple of work areas for the final logic on the salt */
    salt_len = KDF_SRTP_SALT_LEN;
    memset(iv, 0, KDF_SRTP_IV_LEN);
    memset(master_salt, 0, sizeof(master_salt));
    memcpy(master_salt, msalt, salt_len);

    /* gather some bignums for some math */
    bn_index = BN_new();
    bn_salt = BN_new();
    if ((bn_index == NULL) || (bn_salt == NULL)) {
        BN_free(bn_index);
        BN_free(bn_salt);
        return rv;
    }

    index_len = is_srtp(label) ? KDF_SRTP_IDX_LEN : KDF_SRTCP_IDX_LEN;
    /* if index is NULL or kdr=0, then index and kdr are not in play */
    if (index != NULL && idxlen > 0 && kdr > 0) {
        if ((int)idxlen < index_len) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_INDEX_LENGTH);
            goto err;
        }
        if (!BN_bin2bn(index, index_len, bn_index))
            goto err;

        ret = BN_rshift(bn_salt, bn_index, kdr_n);
        if (!ret)
            goto err;
        iv_len = BN_bn2bin(bn_salt, iv);
        for (i = 1; i <= iv_len; i++)
            master_salt[salt_len - i] ^= iv[iv_len - i];
    }

    /* take the munged up salt from above and add the label */
    memset(local_salt, 0, KDF_SRTP_MAX_SALT_LEN);
    memcpy(local_salt, master_salt, salt_len);
    local_salt[((KDF_SRTP_SALT_LEN - 1) - index_len)] ^= label;

    /* perform the AES encryption on the master key and derived salt */
    memset(buf, 0, o_len);
    if (!(ctx = EVP_CIPHER_CTX_new())
        || (EVP_EncryptInit_ex(ctx, cipher, NULL, mkey, local_salt) <= 0)
        || (EVP_CIPHER_CTX_set_padding(ctx, 0) <= 0)
        || (EVP_EncryptUpdate(ctx, (unsigned char *)obuffer, &outl, buf, o_len) <= 0)
        || (EVP_EncryptFinal_ex(ctx, (unsigned char *)obuffer, &outl) <= 0))
        goto err;

    rv = 1;
err:
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(iv, KDF_SRTP_IV_LEN);
    OPENSSL_cleanse(local_salt, KDF_SRTP_MAX_SALT_LEN);
    OPENSSL_cleanse(master_salt, KDF_SRTP_IV_LEN);
    BN_clear_free(bn_index);
    BN_clear_free(bn_salt);
    return rv;
}
