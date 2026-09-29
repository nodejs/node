/**
 * \file cipher_wrap.c
 *
 * \brief Generic cipher wrapper for Mbed TLS
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_CIPHER_C)

#include "cipher_wrap.h"
#include "mbedtls/private/error_common.h"

#if defined(MBEDTLS_CHACHAPOLY_C)
#include "mbedtls/private/chachapoly.h"
#endif

#if defined(MBEDTLS_AES_C)
#include "mbedtls/private/aes.h"
#endif

#if defined(MBEDTLS_CAMELLIA_C)
#include "mbedtls/private/camellia.h"
#endif

#if defined(MBEDTLS_ARIA_C)
#include "mbedtls/private/aria.h"
#endif

#if defined(MBEDTLS_CHACHA20_C)
#include "mbedtls/private/chacha20.h"
#endif

#if defined(MBEDTLS_GCM_C)
#include "mbedtls/private/gcm.h"
#endif

#if defined(MBEDTLS_CCM_C)
#include "mbedtls/private/ccm.h"
#endif

#include "mbedtls/platform.h"

enum mbedtls_cipher_base_index {
#if defined(MBEDTLS_AES_C)
    MBEDTLS_CIPHER_BASE_INDEX_AES,
#endif
#if defined(MBEDTLS_ARIA_C)
    MBEDTLS_CIPHER_BASE_INDEX_ARIA,
#endif
#if defined(MBEDTLS_CAMELLIA_C)
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA,
#endif
#if defined(MBEDTLS_CIPHER_HAVE_CCM_AES_VIA_LEGACY_OR_USE_PSA)
    MBEDTLS_CIPHER_BASE_INDEX_CCM_AES,
#endif
#if defined(MBEDTLS_CCM_C) && defined(MBEDTLS_ARIA_C)
    MBEDTLS_CIPHER_BASE_INDEX_CCM_ARIA,
#endif
#if defined(MBEDTLS_CCM_C) && defined(MBEDTLS_CAMELLIA_C)
    MBEDTLS_CIPHER_BASE_INDEX_CCM_CAMELLIA,
#endif
#if defined(MBEDTLS_CHACHA20_C)
    MBEDTLS_CIPHER_BASE_INDEX_CHACHA20_BASE,
#endif
#if defined(MBEDTLS_CHACHAPOLY_C)
    MBEDTLS_CIPHER_BASE_INDEX_CHACHAPOLY_BASE,
#endif
#if defined(MBEDTLS_CIPHER_HAVE_GCM_AES_VIA_LEGACY_OR_USE_PSA)
    MBEDTLS_CIPHER_BASE_INDEX_GCM_AES,
#endif
#if defined(MBEDTLS_GCM_C) && defined(MBEDTLS_ARIA_C)
    MBEDTLS_CIPHER_BASE_INDEX_GCM_ARIA,
#endif
#if defined(MBEDTLS_GCM_C) && defined(MBEDTLS_CAMELLIA_C)
    MBEDTLS_CIPHER_BASE_INDEX_GCM_CAMELLIA,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS) && defined(MBEDTLS_AES_C)
    MBEDTLS_CIPHER_BASE_INDEX_XTS_AES,
#endif
    /* Prevent compile failure due to empty enum */
    MBEDTLS_CIPHER_BASE_PREVENT_EMPTY_ENUM
};

#if defined(MBEDTLS_GCM_C) && \
    (defined(MBEDTLS_CIPHER_HAVE_GCM_AES_VIA_LEGACY_OR_USE_PSA) || \
    defined(MBEDTLS_ARIA_C) || defined(MBEDTLS_CAMELLIA_C))
/* shared by all GCM ciphers */
static void *gcm_ctx_alloc(void)
{
    void *ctx = mbedtls_calloc(1, sizeof(mbedtls_gcm_context));

    if (ctx != NULL) {
        mbedtls_gcm_init((mbedtls_gcm_context *) ctx);
    }

    return ctx;
}

static void gcm_ctx_free(void *ctx)
{
    mbedtls_gcm_free(ctx);
    mbedtls_free(ctx);
}
#endif /* MBEDTLS_GCM_C */

#if defined(MBEDTLS_CCM_C) && \
    (defined(MBEDTLS_CIPHER_HAVE_CCM_AES_VIA_LEGACY_OR_USE_PSA) || \
    defined(MBEDTLS_ARIA_C) || defined(MBEDTLS_CAMELLIA_C))
/* shared by all CCM ciphers */
static void *ccm_ctx_alloc(void)
{
    void *ctx = mbedtls_calloc(1, sizeof(mbedtls_ccm_context));

    if (ctx != NULL) {
        mbedtls_ccm_init((mbedtls_ccm_context *) ctx);
    }

    return ctx;
}

static void ccm_ctx_free(void *ctx)
{
    mbedtls_ccm_free(ctx);
    mbedtls_free(ctx);
}
#endif /* MBEDTLS_CCM_C */

#if defined(MBEDTLS_AES_C)

static int aes_crypt_ecb_wrap(void *ctx, mbedtls_operation_t operation,
                              const unsigned char *input, unsigned char *output)
{
    return mbedtls_aes_crypt_ecb((mbedtls_aes_context *) ctx, operation, input, output);
}

#if defined(MBEDTLS_CIPHER_MODE_CBC)
static int aes_crypt_cbc_wrap(void *ctx, mbedtls_operation_t operation, size_t length,
                              unsigned char *iv, const unsigned char *input, unsigned char *output)
{
    return mbedtls_aes_crypt_cbc((mbedtls_aes_context *) ctx, operation, length, iv, input,
                                 output);
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
static int aes_crypt_cfb128_wrap(void *ctx, mbedtls_operation_t operation,
                                 size_t length, size_t *iv_off, unsigned char *iv,
                                 const unsigned char *input, unsigned char *output)
{
    return mbedtls_aes_crypt_cfb128((mbedtls_aes_context *) ctx, operation, length, iv_off, iv,
                                    input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_OFB)
static int aes_crypt_ofb_wrap(void *ctx, size_t length, size_t *iv_off,
                              unsigned char *iv, const unsigned char *input, unsigned char *output)
{
    return mbedtls_aes_crypt_ofb((mbedtls_aes_context *) ctx, length, iv_off,
                                 iv, input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_OFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
static int aes_crypt_ctr_wrap(void *ctx, size_t length, size_t *nc_off,
                              unsigned char *nonce_counter, unsigned char *stream_block,
                              const unsigned char *input, unsigned char *output)
{
    return mbedtls_aes_crypt_ctr((mbedtls_aes_context *) ctx, length, nc_off, nonce_counter,
                                 stream_block, input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if defined(MBEDTLS_CIPHER_MODE_XTS)
static int aes_crypt_xts_wrap(void *ctx, mbedtls_operation_t operation,
                              size_t length,
                              const unsigned char data_unit[16],
                              const unsigned char *input,
                              unsigned char *output)
{
    mbedtls_aes_xts_context *xts_ctx = ctx;
    int mode;

    switch (operation) {
        case MBEDTLS_ENCRYPT:
            mode = MBEDTLS_AES_ENCRYPT;
            break;
        case MBEDTLS_DECRYPT:
            mode = MBEDTLS_AES_DECRYPT;
            break;
        default:
            return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    return mbedtls_aes_crypt_xts(xts_ctx, mode, length,
                                 data_unit, input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_XTS */

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
static int aes_setkey_dec_wrap(void *ctx, const unsigned char *key,
                               unsigned int key_bitlen)
{
    return mbedtls_aes_setkey_dec((mbedtls_aes_context *) ctx, key, key_bitlen);
}
#endif

static int aes_setkey_enc_wrap(void *ctx, const unsigned char *key,
                               unsigned int key_bitlen)
{
    return mbedtls_aes_setkey_enc((mbedtls_aes_context *) ctx, key, key_bitlen);
}

static void *aes_ctx_alloc(void)
{
    mbedtls_aes_context *aes = mbedtls_calloc(1, sizeof(mbedtls_aes_context));

    if (aes == NULL) {
        return NULL;
    }

    mbedtls_aes_init(aes);

    return aes;
}

static void aes_ctx_free(void *ctx)
{
    mbedtls_aes_free((mbedtls_aes_context *) ctx);
    mbedtls_free(ctx);
}

static const mbedtls_cipher_base_t aes_info = {
    MBEDTLS_CIPHER_ID_AES,
    aes_crypt_ecb_wrap,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    aes_crypt_cbc_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    aes_crypt_cfb128_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    aes_crypt_ofb_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    aes_crypt_ctr_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
    aes_setkey_enc_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    aes_setkey_dec_wrap,
#endif
    aes_ctx_alloc,
    aes_ctx_free
};

static const mbedtls_cipher_info_t aes_128_ecb_info = {
    "AES-128-ECB",
    16,
    0 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_ECB,
    MBEDTLS_CIPHER_AES_128_ECB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const mbedtls_cipher_info_t aes_192_ecb_info = {
    "AES-192-ECB",
    16,
    0 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_ECB,
    MBEDTLS_CIPHER_AES_192_ECB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

static const mbedtls_cipher_info_t aes_256_ecb_info = {
    "AES-256-ECB",
    16,
    0 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_ECB,
    MBEDTLS_CIPHER_AES_256_ECB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};
#endif

#if defined(MBEDTLS_CIPHER_MODE_CBC)
static const mbedtls_cipher_info_t aes_128_cbc_info = {
    "AES-128-CBC",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CBC,
    MBEDTLS_CIPHER_AES_128_CBC,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const mbedtls_cipher_info_t aes_192_cbc_info = {
    "AES-192-CBC",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CBC,
    MBEDTLS_CIPHER_AES_192_CBC,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

static const mbedtls_cipher_info_t aes_256_cbc_info = {
    "AES-256-CBC",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CBC,
    MBEDTLS_CIPHER_AES_256_CBC,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};
#endif
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
static const mbedtls_cipher_info_t aes_128_cfb128_info = {
    "AES-128-CFB128",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CFB,
    MBEDTLS_CIPHER_AES_128_CFB128,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const mbedtls_cipher_info_t aes_192_cfb128_info = {
    "AES-192-CFB128",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CFB,
    MBEDTLS_CIPHER_AES_192_CFB128,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

static const mbedtls_cipher_info_t aes_256_cfb128_info = {
    "AES-256-CFB128",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CFB,
    MBEDTLS_CIPHER_AES_256_CFB128,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};
#endif
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_OFB)
static const mbedtls_cipher_info_t aes_128_ofb_info = {
    "AES-128-OFB",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_OFB,
    MBEDTLS_CIPHER_AES_128_OFB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const mbedtls_cipher_info_t aes_192_ofb_info = {
    "AES-192-OFB",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_OFB,
    MBEDTLS_CIPHER_AES_192_OFB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

static const mbedtls_cipher_info_t aes_256_ofb_info = {
    "AES-256-OFB",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_OFB,
    MBEDTLS_CIPHER_AES_256_OFB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};
#endif
#endif /* MBEDTLS_CIPHER_MODE_OFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
static const mbedtls_cipher_info_t aes_128_ctr_info = {
    "AES-128-CTR",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CTR,
    MBEDTLS_CIPHER_AES_128_CTR,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const mbedtls_cipher_info_t aes_192_ctr_info = {
    "AES-192-CTR",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CTR,
    MBEDTLS_CIPHER_AES_192_CTR,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};

static const mbedtls_cipher_info_t aes_256_ctr_info = {
    "AES-256-CTR",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CTR,
    MBEDTLS_CIPHER_AES_256_CTR,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_AES
};
#endif
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if defined(MBEDTLS_CIPHER_MODE_XTS)
static int xts_aes_setkey_enc_wrap(void *ctx, const unsigned char *key,
                                   unsigned int key_bitlen)
{
    mbedtls_aes_xts_context *xts_ctx = ctx;
    return mbedtls_aes_xts_setkey_enc(xts_ctx, key, key_bitlen);
}

static int xts_aes_setkey_dec_wrap(void *ctx, const unsigned char *key,
                                   unsigned int key_bitlen)
{
    mbedtls_aes_xts_context *xts_ctx = ctx;
    return mbedtls_aes_xts_setkey_dec(xts_ctx, key, key_bitlen);
}

static void *xts_aes_ctx_alloc(void)
{
    mbedtls_aes_xts_context *xts_ctx = mbedtls_calloc(1, sizeof(*xts_ctx));

    if (xts_ctx != NULL) {
        mbedtls_aes_xts_init(xts_ctx);
    }

    return xts_ctx;
}

static void xts_aes_ctx_free(void *ctx)
{
    mbedtls_aes_xts_context *xts_ctx = ctx;

    if (xts_ctx == NULL) {
        return;
    }

    mbedtls_aes_xts_free(xts_ctx);
    mbedtls_free(xts_ctx);
}

static const mbedtls_cipher_base_t xts_aes_info = {
    MBEDTLS_CIPHER_ID_AES,
    NULL,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    aes_crypt_xts_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
    xts_aes_setkey_enc_wrap,
    xts_aes_setkey_dec_wrap,
    xts_aes_ctx_alloc,
    xts_aes_ctx_free
};

static const mbedtls_cipher_info_t aes_128_xts_info = {
    "AES-128-XTS",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_XTS,
    MBEDTLS_CIPHER_AES_128_XTS,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_XTS_AES
};

#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const mbedtls_cipher_info_t aes_256_xts_info = {
    "AES-256-XTS",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    512 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_XTS,
    MBEDTLS_CIPHER_AES_256_XTS,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_XTS_AES
};
#endif
#endif /* MBEDTLS_CIPHER_MODE_XTS */
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_GCM_C) && defined(MBEDTLS_CCM_GCM_CAN_AES)
static int gcm_aes_setkey_wrap(void *ctx, const unsigned char *key,
                               unsigned int key_bitlen)
{
    return mbedtls_gcm_setkey((mbedtls_gcm_context *) ctx, MBEDTLS_CIPHER_ID_AES,
                              key, key_bitlen);
}
#endif /* MBEDTLS_GCM_C && MBEDTLS_CCM_GCM_CAN_AES */

#if defined(MBEDTLS_CIPHER_HAVE_GCM_AES_VIA_LEGACY_OR_USE_PSA)
static const mbedtls_cipher_base_t gcm_aes_info = {
    MBEDTLS_CIPHER_ID_AES,
    NULL,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
#if defined(MBEDTLS_GCM_C)
    gcm_aes_setkey_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    gcm_aes_setkey_wrap,
#endif
    gcm_ctx_alloc,
    gcm_ctx_free,
#else
    NULL,
    NULL,
    NULL,
    NULL,
#endif /* MBEDTLS_GCM_C */
};
#endif /* MBEDTLS_CIPHER_HAVE_GCM_AES_VIA_LEGACY_OR_USE_PSA */

#if defined(MBEDTLS_CIPHER_HAVE_GCM_AES_VIA_LEGACY_OR_USE_PSA)
static const mbedtls_cipher_info_t aes_128_gcm_info = {
    "AES-128-GCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_GCM,
    MBEDTLS_CIPHER_AES_128_GCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_GCM_AES
};

#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const mbedtls_cipher_info_t aes_192_gcm_info = {
    "AES-192-GCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_GCM,
    MBEDTLS_CIPHER_AES_192_GCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_GCM_AES
};

static const mbedtls_cipher_info_t aes_256_gcm_info = {
    "AES-256-GCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_GCM,
    MBEDTLS_CIPHER_AES_256_GCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_GCM_AES
};
#endif
#endif /* MBEDTLS_CIPHER_HAVE_GCM_AES_VIA_LEGACY_OR_USE_PSA */

#if defined(MBEDTLS_CCM_C) && defined(MBEDTLS_CCM_GCM_CAN_AES)
static int ccm_aes_setkey_wrap(void *ctx, const unsigned char *key,
                               unsigned int key_bitlen)
{
    return mbedtls_ccm_setkey((mbedtls_ccm_context *) ctx, MBEDTLS_CIPHER_ID_AES,
                              key, key_bitlen);
}
#endif /* MBEDTLS_CCM_C && MBEDTLS_CCM_GCM_CAN_AES */

#if defined(MBEDTLS_CIPHER_HAVE_CCM_AES_VIA_LEGACY_OR_USE_PSA)
static const mbedtls_cipher_base_t ccm_aes_info = {
    MBEDTLS_CIPHER_ID_AES,
    NULL,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
#if defined(MBEDTLS_CCM_C)
    ccm_aes_setkey_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    ccm_aes_setkey_wrap,
#endif
    ccm_ctx_alloc,
    ccm_ctx_free,
#else
    NULL,
    NULL,
    NULL,
    NULL,
#endif
};
#endif /* MBEDTLS_CIPHER_HAVE_CCM_AES_VIA_LEGACY_OR_USE_PSA */

#if defined(MBEDTLS_CIPHER_HAVE_CCM_AES_VIA_LEGACY_OR_USE_PSA)
static const mbedtls_cipher_info_t aes_128_ccm_info = {
    "AES-128-CCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM,
    MBEDTLS_CIPHER_AES_128_CCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_AES
};

#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const mbedtls_cipher_info_t aes_192_ccm_info = {
    "AES-192-CCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM,
    MBEDTLS_CIPHER_AES_192_CCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_AES
};

static const mbedtls_cipher_info_t aes_256_ccm_info = {
    "AES-256-CCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM,
    MBEDTLS_CIPHER_AES_256_CCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_AES
};
#endif
#endif /* MBEDTLS_CIPHER_HAVE_CCM_AES_VIA_LEGACY_OR_USE_PSA */

#if defined(MBEDTLS_CIPHER_HAVE_CCM_STAR_NO_TAG_AES_VIA_LEGACY_OR_USE_PSA)
static const mbedtls_cipher_info_t aes_128_ccm_star_no_tag_info = {
    "AES-128-CCM*-NO-TAG",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_AES_128_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_AES
};

#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const mbedtls_cipher_info_t aes_192_ccm_star_no_tag_info = {
    "AES-192-CCM*-NO-TAG",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_AES_192_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_AES
};

static const mbedtls_cipher_info_t aes_256_ccm_star_no_tag_info = {
    "AES-256-CCM*-NO-TAG",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_AES_256_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_AES
};
#endif
#endif /* MBEDTLS_CIPHER_HAVE_CCM_STAR_NO_TAG_AES_VIA_LEGACY_OR_USE_PSA */


#if defined(MBEDTLS_CAMELLIA_C)

static int camellia_crypt_ecb_wrap(void *ctx, mbedtls_operation_t operation,
                                   const unsigned char *input, unsigned char *output)
{
    return mbedtls_camellia_crypt_ecb((mbedtls_camellia_context *) ctx, operation, input,
                                      output);
}

#if defined(MBEDTLS_CIPHER_MODE_CBC)
static int camellia_crypt_cbc_wrap(void *ctx, mbedtls_operation_t operation,
                                   size_t length, unsigned char *iv,
                                   const unsigned char *input, unsigned char *output)
{
    return mbedtls_camellia_crypt_cbc((mbedtls_camellia_context *) ctx, operation, length, iv,
                                      input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
static int camellia_crypt_cfb128_wrap(void *ctx, mbedtls_operation_t operation,
                                      size_t length, size_t *iv_off, unsigned char *iv,
                                      const unsigned char *input, unsigned char *output)
{
    return mbedtls_camellia_crypt_cfb128((mbedtls_camellia_context *) ctx, operation, length,
                                         iv_off, iv, input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
static int camellia_crypt_ctr_wrap(void *ctx, size_t length, size_t *nc_off,
                                   unsigned char *nonce_counter, unsigned char *stream_block,
                                   const unsigned char *input, unsigned char *output)
{
    return mbedtls_camellia_crypt_ctr((mbedtls_camellia_context *) ctx, length, nc_off,
                                      nonce_counter, stream_block, input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
static int camellia_setkey_dec_wrap(void *ctx, const unsigned char *key,
                                    unsigned int key_bitlen)
{
    return mbedtls_camellia_setkey_dec((mbedtls_camellia_context *) ctx, key, key_bitlen);
}
#endif

static int camellia_setkey_enc_wrap(void *ctx, const unsigned char *key,
                                    unsigned int key_bitlen)
{
    return mbedtls_camellia_setkey_enc((mbedtls_camellia_context *) ctx, key, key_bitlen);
}

static void *camellia_ctx_alloc(void)
{
    mbedtls_camellia_context *ctx;
    ctx = mbedtls_calloc(1, sizeof(mbedtls_camellia_context));

    if (ctx == NULL) {
        return NULL;
    }

    mbedtls_camellia_init(ctx);

    return ctx;
}

static void camellia_ctx_free(void *ctx)
{
    mbedtls_camellia_free((mbedtls_camellia_context *) ctx);
    mbedtls_free(ctx);
}

static const mbedtls_cipher_base_t camellia_info = {
    MBEDTLS_CIPHER_ID_CAMELLIA,
    camellia_crypt_ecb_wrap,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    camellia_crypt_cbc_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    camellia_crypt_cfb128_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    camellia_crypt_ctr_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
    camellia_setkey_enc_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    camellia_setkey_dec_wrap,
#endif
    camellia_ctx_alloc,
    camellia_ctx_free
};

static const mbedtls_cipher_info_t camellia_128_ecb_info = {
    "CAMELLIA-128-ECB",
    16,
    0 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_ECB,
    MBEDTLS_CIPHER_CAMELLIA_128_ECB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_192_ecb_info = {
    "CAMELLIA-192-ECB",
    16,
    0 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_ECB,
    MBEDTLS_CIPHER_CAMELLIA_192_ECB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_256_ecb_info = {
    "CAMELLIA-256-ECB",
    16,
    0 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_ECB,
    MBEDTLS_CIPHER_CAMELLIA_256_ECB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};

#if defined(MBEDTLS_CIPHER_MODE_CBC)
static const mbedtls_cipher_info_t camellia_128_cbc_info = {
    "CAMELLIA-128-CBC",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CBC,
    MBEDTLS_CIPHER_CAMELLIA_128_CBC,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_192_cbc_info = {
    "CAMELLIA-192-CBC",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CBC,
    MBEDTLS_CIPHER_CAMELLIA_192_CBC,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_256_cbc_info = {
    "CAMELLIA-256-CBC",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CBC,
    MBEDTLS_CIPHER_CAMELLIA_256_CBC,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
static const mbedtls_cipher_info_t camellia_128_cfb128_info = {
    "CAMELLIA-128-CFB128",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CFB,
    MBEDTLS_CIPHER_CAMELLIA_128_CFB128,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_192_cfb128_info = {
    "CAMELLIA-192-CFB128",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CFB,
    MBEDTLS_CIPHER_CAMELLIA_192_CFB128,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_256_cfb128_info = {
    "CAMELLIA-256-CFB128",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CFB,
    MBEDTLS_CIPHER_CAMELLIA_256_CFB128,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
static const mbedtls_cipher_info_t camellia_128_ctr_info = {
    "CAMELLIA-128-CTR",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CTR,
    MBEDTLS_CIPHER_CAMELLIA_128_CTR,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_192_ctr_info = {
    "CAMELLIA-192-CTR",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CTR,
    MBEDTLS_CIPHER_CAMELLIA_192_CTR,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_256_ctr_info = {
    "CAMELLIA-256-CTR",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CTR,
    MBEDTLS_CIPHER_CAMELLIA_256_CTR,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA
};
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if defined(MBEDTLS_GCM_C)
static int gcm_camellia_setkey_wrap(void *ctx, const unsigned char *key,
                                    unsigned int key_bitlen)
{
    return mbedtls_gcm_setkey((mbedtls_gcm_context *) ctx, MBEDTLS_CIPHER_ID_CAMELLIA,
                              key, key_bitlen);
}

static const mbedtls_cipher_base_t gcm_camellia_info = {
    MBEDTLS_CIPHER_ID_CAMELLIA,
    NULL,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
    gcm_camellia_setkey_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    gcm_camellia_setkey_wrap,
#endif
    gcm_ctx_alloc,
    gcm_ctx_free,
};

static const mbedtls_cipher_info_t camellia_128_gcm_info = {
    "CAMELLIA-128-GCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_GCM,
    MBEDTLS_CIPHER_CAMELLIA_128_GCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_GCM_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_192_gcm_info = {
    "CAMELLIA-192-GCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_GCM,
    MBEDTLS_CIPHER_CAMELLIA_192_GCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_GCM_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_256_gcm_info = {
    "CAMELLIA-256-GCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_GCM,
    MBEDTLS_CIPHER_CAMELLIA_256_GCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_GCM_CAMELLIA
};
#endif /* MBEDTLS_GCM_C */

#if defined(MBEDTLS_CCM_C)
static int ccm_camellia_setkey_wrap(void *ctx, const unsigned char *key,
                                    unsigned int key_bitlen)
{
    return mbedtls_ccm_setkey((mbedtls_ccm_context *) ctx, MBEDTLS_CIPHER_ID_CAMELLIA,
                              key, key_bitlen);
}

static const mbedtls_cipher_base_t ccm_camellia_info = {
    MBEDTLS_CIPHER_ID_CAMELLIA,
    NULL,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
    ccm_camellia_setkey_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    ccm_camellia_setkey_wrap,
#endif
    ccm_ctx_alloc,
    ccm_ctx_free,
};

static const mbedtls_cipher_info_t camellia_128_ccm_info = {
    "CAMELLIA-128-CCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM,
    MBEDTLS_CIPHER_CAMELLIA_128_CCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_192_ccm_info = {
    "CAMELLIA-192-CCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM,
    MBEDTLS_CIPHER_CAMELLIA_192_CCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_256_ccm_info = {
    "CAMELLIA-256-CCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM,
    MBEDTLS_CIPHER_CAMELLIA_256_CCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_128_ccm_star_no_tag_info = {
    "CAMELLIA-128-CCM*-NO-TAG",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_CAMELLIA_128_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_192_ccm_star_no_tag_info = {
    "CAMELLIA-192-CCM*-NO-TAG",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_CAMELLIA_192_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_CAMELLIA
};

static const mbedtls_cipher_info_t camellia_256_ccm_star_no_tag_info = {
    "CAMELLIA-256-CCM*-NO-TAG",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_CAMELLIA_256_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_CAMELLIA
};
#endif /* MBEDTLS_CCM_C */

#endif /* MBEDTLS_CAMELLIA_C */

#if defined(MBEDTLS_ARIA_C)

static int aria_crypt_ecb_wrap(void *ctx, mbedtls_operation_t operation,
                               const unsigned char *input, unsigned char *output)
{
    (void) operation;
    return mbedtls_aria_crypt_ecb((mbedtls_aria_context *) ctx, input,
                                  output);
}

#if defined(MBEDTLS_CIPHER_MODE_CBC)
static int aria_crypt_cbc_wrap(void *ctx, mbedtls_operation_t operation,
                               size_t length, unsigned char *iv,
                               const unsigned char *input, unsigned char *output)
{
    return mbedtls_aria_crypt_cbc((mbedtls_aria_context *) ctx, operation, length, iv,
                                  input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
static int aria_crypt_cfb128_wrap(void *ctx, mbedtls_operation_t operation,
                                  size_t length, size_t *iv_off, unsigned char *iv,
                                  const unsigned char *input, unsigned char *output)
{
    return mbedtls_aria_crypt_cfb128((mbedtls_aria_context *) ctx, operation, length,
                                     iv_off, iv, input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
static int aria_crypt_ctr_wrap(void *ctx, size_t length, size_t *nc_off,
                               unsigned char *nonce_counter, unsigned char *stream_block,
                               const unsigned char *input, unsigned char *output)
{
    return mbedtls_aria_crypt_ctr((mbedtls_aria_context *) ctx, length, nc_off,
                                  nonce_counter, stream_block, input, output);
}
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
static int aria_setkey_dec_wrap(void *ctx, const unsigned char *key,
                                unsigned int key_bitlen)
{
    return mbedtls_aria_setkey_dec((mbedtls_aria_context *) ctx, key, key_bitlen);
}
#endif

static int aria_setkey_enc_wrap(void *ctx, const unsigned char *key,
                                unsigned int key_bitlen)
{
    return mbedtls_aria_setkey_enc((mbedtls_aria_context *) ctx, key, key_bitlen);
}

static void *aria_ctx_alloc(void)
{
    mbedtls_aria_context *ctx;
    ctx = mbedtls_calloc(1, sizeof(mbedtls_aria_context));

    if (ctx == NULL) {
        return NULL;
    }

    mbedtls_aria_init(ctx);

    return ctx;
}

static void aria_ctx_free(void *ctx)
{
    mbedtls_aria_free((mbedtls_aria_context *) ctx);
    mbedtls_free(ctx);
}

static const mbedtls_cipher_base_t aria_info = {
    MBEDTLS_CIPHER_ID_ARIA,
    aria_crypt_ecb_wrap,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    aria_crypt_cbc_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    aria_crypt_cfb128_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    aria_crypt_ctr_wrap,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
    aria_setkey_enc_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    aria_setkey_dec_wrap,
#endif
    aria_ctx_alloc,
    aria_ctx_free
};

static const mbedtls_cipher_info_t aria_128_ecb_info = {
    "ARIA-128-ECB",
    16,
    0 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_ECB,
    MBEDTLS_CIPHER_ARIA_128_ECB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};

static const mbedtls_cipher_info_t aria_192_ecb_info = {
    "ARIA-192-ECB",
    16,
    0 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_ECB,
    MBEDTLS_CIPHER_ARIA_192_ECB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};

static const mbedtls_cipher_info_t aria_256_ecb_info = {
    "ARIA-256-ECB",
    16,
    0 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_ECB,
    MBEDTLS_CIPHER_ARIA_256_ECB,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};

#if defined(MBEDTLS_CIPHER_MODE_CBC)
static const mbedtls_cipher_info_t aria_128_cbc_info = {
    "ARIA-128-CBC",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CBC,
    MBEDTLS_CIPHER_ARIA_128_CBC,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};

static const mbedtls_cipher_info_t aria_192_cbc_info = {
    "ARIA-192-CBC",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CBC,
    MBEDTLS_CIPHER_ARIA_192_CBC,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};

static const mbedtls_cipher_info_t aria_256_cbc_info = {
    "ARIA-256-CBC",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CBC,
    MBEDTLS_CIPHER_ARIA_256_CBC,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
static const mbedtls_cipher_info_t aria_128_cfb128_info = {
    "ARIA-128-CFB128",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CFB,
    MBEDTLS_CIPHER_ARIA_128_CFB128,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};

static const mbedtls_cipher_info_t aria_192_cfb128_info = {
    "ARIA-192-CFB128",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CFB,
    MBEDTLS_CIPHER_ARIA_192_CFB128,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};

static const mbedtls_cipher_info_t aria_256_cfb128_info = {
    "ARIA-256-CFB128",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CFB,
    MBEDTLS_CIPHER_ARIA_256_CFB128,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
static const mbedtls_cipher_info_t aria_128_ctr_info = {
    "ARIA-128-CTR",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CTR,
    MBEDTLS_CIPHER_ARIA_128_CTR,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};

static const mbedtls_cipher_info_t aria_192_ctr_info = {
    "ARIA-192-CTR",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CTR,
    MBEDTLS_CIPHER_ARIA_192_CTR,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};

static const mbedtls_cipher_info_t aria_256_ctr_info = {
    "ARIA-256-CTR",
    16,
    16 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CTR,
    MBEDTLS_CIPHER_ARIA_256_CTR,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_ARIA
};
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#if defined(MBEDTLS_GCM_C)
static int gcm_aria_setkey_wrap(void *ctx, const unsigned char *key,
                                unsigned int key_bitlen)
{
    return mbedtls_gcm_setkey((mbedtls_gcm_context *) ctx, MBEDTLS_CIPHER_ID_ARIA,
                              key, key_bitlen);
}

static const mbedtls_cipher_base_t gcm_aria_info = {
    MBEDTLS_CIPHER_ID_ARIA,
    NULL,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
    gcm_aria_setkey_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    gcm_aria_setkey_wrap,
#endif
    gcm_ctx_alloc,
    gcm_ctx_free,
};

static const mbedtls_cipher_info_t aria_128_gcm_info = {
    "ARIA-128-GCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_GCM,
    MBEDTLS_CIPHER_ARIA_128_GCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_GCM_ARIA
};

static const mbedtls_cipher_info_t aria_192_gcm_info = {
    "ARIA-192-GCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_GCM,
    MBEDTLS_CIPHER_ARIA_192_GCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_GCM_ARIA
};

static const mbedtls_cipher_info_t aria_256_gcm_info = {
    "ARIA-256-GCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_GCM,
    MBEDTLS_CIPHER_ARIA_256_GCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_GCM_ARIA
};
#endif /* MBEDTLS_GCM_C */

#if defined(MBEDTLS_CCM_C)
static int ccm_aria_setkey_wrap(void *ctx, const unsigned char *key,
                                unsigned int key_bitlen)
{
    return mbedtls_ccm_setkey((mbedtls_ccm_context *) ctx, MBEDTLS_CIPHER_ID_ARIA,
                              key, key_bitlen);
}

static const mbedtls_cipher_base_t ccm_aria_info = {
    MBEDTLS_CIPHER_ID_ARIA,
    NULL,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
    ccm_aria_setkey_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    ccm_aria_setkey_wrap,
#endif
    ccm_ctx_alloc,
    ccm_ctx_free,
};

static const mbedtls_cipher_info_t aria_128_ccm_info = {
    "ARIA-128-CCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM,
    MBEDTLS_CIPHER_ARIA_128_CCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_ARIA
};

static const mbedtls_cipher_info_t aria_192_ccm_info = {
    "ARIA-192-CCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM,
    MBEDTLS_CIPHER_ARIA_192_CCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_ARIA
};

static const mbedtls_cipher_info_t aria_256_ccm_info = {
    "ARIA-256-CCM",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM,
    MBEDTLS_CIPHER_ARIA_256_CCM,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_ARIA
};

static const mbedtls_cipher_info_t aria_128_ccm_star_no_tag_info = {
    "ARIA-128-CCM*-NO-TAG",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    128 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_ARIA_128_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_ARIA
};

static const mbedtls_cipher_info_t aria_192_ccm_star_no_tag_info = {
    "ARIA-192-CCM*-NO-TAG",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    192 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_ARIA_192_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_ARIA
};

static const mbedtls_cipher_info_t aria_256_ccm_star_no_tag_info = {
    "ARIA-256-CCM*-NO-TAG",
    16,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_ARIA_256_CCM_STAR_NO_TAG,
    MBEDTLS_CIPHER_VARIABLE_IV_LEN,
    MBEDTLS_CIPHER_BASE_INDEX_CCM_ARIA
};
#endif /* MBEDTLS_CCM_C */

#endif /* MBEDTLS_ARIA_C */

#if defined(MBEDTLS_CHACHA20_C)

static int chacha20_setkey_wrap(void *ctx, const unsigned char *key,
                                unsigned int key_bitlen)
{
    if (key_bitlen != 256U) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    if (0 != mbedtls_chacha20_setkey((mbedtls_chacha20_context *) ctx, key)) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    return 0;
}

static int chacha20_stream_wrap(void *ctx,  size_t length,
                                const unsigned char *input,
                                unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ret = mbedtls_chacha20_update(ctx, length, input, output);
    if (ret == MBEDTLS_ERR_CHACHA20_BAD_INPUT_DATA) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    return ret;
}

static void *chacha20_ctx_alloc(void)
{
    mbedtls_chacha20_context *ctx;
    ctx = mbedtls_calloc(1, sizeof(mbedtls_chacha20_context));

    if (ctx == NULL) {
        return NULL;
    }

    mbedtls_chacha20_init(ctx);

    return ctx;
}

static void chacha20_ctx_free(void *ctx)
{
    mbedtls_chacha20_free((mbedtls_chacha20_context *) ctx);
    mbedtls_free(ctx);
}

static const mbedtls_cipher_base_t chacha20_base_info = {
    MBEDTLS_CIPHER_ID_CHACHA20,
    NULL,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    chacha20_stream_wrap,
#endif
    chacha20_setkey_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    chacha20_setkey_wrap,
#endif
    chacha20_ctx_alloc,
    chacha20_ctx_free
};
static const mbedtls_cipher_info_t chacha20_info = {
    "CHACHA20",
    1,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_STREAM,
    MBEDTLS_CIPHER_CHACHA20,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CHACHA20_BASE
};
#endif /* MBEDTLS_CHACHA20_C */

#if defined(MBEDTLS_CHACHAPOLY_C)

static int chachapoly_setkey_wrap(void *ctx,
                                  const unsigned char *key,
                                  unsigned int key_bitlen)
{
    if (key_bitlen != 256U) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    if (0 != mbedtls_chachapoly_setkey((mbedtls_chachapoly_context *) ctx, key)) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    return 0;
}

static void *chachapoly_ctx_alloc(void)
{
    mbedtls_chachapoly_context *ctx;
    ctx = mbedtls_calloc(1, sizeof(mbedtls_chachapoly_context));

    if (ctx == NULL) {
        return NULL;
    }

    mbedtls_chachapoly_init(ctx);

    return ctx;
}

static void chachapoly_ctx_free(void *ctx)
{
    mbedtls_chachapoly_free((mbedtls_chachapoly_context *) ctx);
    mbedtls_free(ctx);
}

static const mbedtls_cipher_base_t chachapoly_base_info = {
    MBEDTLS_CIPHER_ID_CHACHA20,
    NULL,
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    NULL,
#endif
#if defined(MBEDTLS_CIPHER_MODE_STREAM)
    NULL,
#endif
    chachapoly_setkey_wrap,
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
    chachapoly_setkey_wrap,
#endif
    chachapoly_ctx_alloc,
    chachapoly_ctx_free
};
static const mbedtls_cipher_info_t chachapoly_info = {
    "CHACHA20-POLY1305",
    1,
    12 >> MBEDTLS_IV_SIZE_SHIFT,
    256 >> MBEDTLS_KEY_BITLEN_SHIFT,
    MBEDTLS_MODE_CHACHAPOLY,
    MBEDTLS_CIPHER_CHACHA20_POLY1305,
    0,
    MBEDTLS_CIPHER_BASE_INDEX_CHACHAPOLY_BASE
};
#endif /* MBEDTLS_CHACHAPOLY_C */

const mbedtls_cipher_definition_t mbedtls_cipher_definitions[] =
{
#if defined(MBEDTLS_AES_C)
    { MBEDTLS_CIPHER_AES_128_ECB,          &aes_128_ecb_info },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { MBEDTLS_CIPHER_AES_192_ECB,          &aes_192_ecb_info },
    { MBEDTLS_CIPHER_AES_256_ECB,          &aes_256_ecb_info },
#endif
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    { MBEDTLS_CIPHER_AES_128_CBC,          &aes_128_cbc_info },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { MBEDTLS_CIPHER_AES_192_CBC,          &aes_192_cbc_info },
    { MBEDTLS_CIPHER_AES_256_CBC,          &aes_256_cbc_info },
#endif
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    { MBEDTLS_CIPHER_AES_128_CFB128,       &aes_128_cfb128_info },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { MBEDTLS_CIPHER_AES_192_CFB128,       &aes_192_cfb128_info },
    { MBEDTLS_CIPHER_AES_256_CFB128,       &aes_256_cfb128_info },
#endif
#endif
#if defined(MBEDTLS_CIPHER_MODE_OFB)
    { MBEDTLS_CIPHER_AES_128_OFB,          &aes_128_ofb_info },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { MBEDTLS_CIPHER_AES_192_OFB,          &aes_192_ofb_info },
    { MBEDTLS_CIPHER_AES_256_OFB,          &aes_256_ofb_info },
#endif
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    { MBEDTLS_CIPHER_AES_128_CTR,          &aes_128_ctr_info },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { MBEDTLS_CIPHER_AES_192_CTR,          &aes_192_ctr_info },
    { MBEDTLS_CIPHER_AES_256_CTR,          &aes_256_ctr_info },
#endif
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    { MBEDTLS_CIPHER_AES_128_XTS,          &aes_128_xts_info },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { MBEDTLS_CIPHER_AES_256_XTS,          &aes_256_xts_info },
#endif
#endif
#endif /* MBEDTLS_AES_C */
#if defined(MBEDTLS_CIPHER_HAVE_GCM_AES_VIA_LEGACY_OR_USE_PSA)
    { MBEDTLS_CIPHER_AES_128_GCM,          &aes_128_gcm_info },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { MBEDTLS_CIPHER_AES_192_GCM,          &aes_192_gcm_info },
    { MBEDTLS_CIPHER_AES_256_GCM,          &aes_256_gcm_info },
#endif
#endif
#if defined(MBEDTLS_CIPHER_HAVE_CCM_AES_VIA_LEGACY_OR_USE_PSA)
    { MBEDTLS_CIPHER_AES_128_CCM,          &aes_128_ccm_info },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { MBEDTLS_CIPHER_AES_192_CCM,          &aes_192_ccm_info },
    { MBEDTLS_CIPHER_AES_256_CCM,          &aes_256_ccm_info },
#endif
#endif
#if defined(MBEDTLS_CIPHER_HAVE_CCM_STAR_NO_TAG_AES_VIA_LEGACY_OR_USE_PSA)
    { MBEDTLS_CIPHER_AES_128_CCM_STAR_NO_TAG,          &aes_128_ccm_star_no_tag_info },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { MBEDTLS_CIPHER_AES_192_CCM_STAR_NO_TAG,          &aes_192_ccm_star_no_tag_info },
    { MBEDTLS_CIPHER_AES_256_CCM_STAR_NO_TAG,          &aes_256_ccm_star_no_tag_info },
#endif
#endif

#if defined(MBEDTLS_CAMELLIA_C)
    { MBEDTLS_CIPHER_CAMELLIA_128_ECB,     &camellia_128_ecb_info },
    { MBEDTLS_CIPHER_CAMELLIA_192_ECB,     &camellia_192_ecb_info },
    { MBEDTLS_CIPHER_CAMELLIA_256_ECB,     &camellia_256_ecb_info },
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    { MBEDTLS_CIPHER_CAMELLIA_128_CBC,     &camellia_128_cbc_info },
    { MBEDTLS_CIPHER_CAMELLIA_192_CBC,     &camellia_192_cbc_info },
    { MBEDTLS_CIPHER_CAMELLIA_256_CBC,     &camellia_256_cbc_info },
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    { MBEDTLS_CIPHER_CAMELLIA_128_CFB128,  &camellia_128_cfb128_info },
    { MBEDTLS_CIPHER_CAMELLIA_192_CFB128,  &camellia_192_cfb128_info },
    { MBEDTLS_CIPHER_CAMELLIA_256_CFB128,  &camellia_256_cfb128_info },
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    { MBEDTLS_CIPHER_CAMELLIA_128_CTR,     &camellia_128_ctr_info },
    { MBEDTLS_CIPHER_CAMELLIA_192_CTR,     &camellia_192_ctr_info },
    { MBEDTLS_CIPHER_CAMELLIA_256_CTR,     &camellia_256_ctr_info },
#endif
#if defined(MBEDTLS_GCM_C)
    { MBEDTLS_CIPHER_CAMELLIA_128_GCM,     &camellia_128_gcm_info },
    { MBEDTLS_CIPHER_CAMELLIA_192_GCM,     &camellia_192_gcm_info },
    { MBEDTLS_CIPHER_CAMELLIA_256_GCM,     &camellia_256_gcm_info },
#endif
#if defined(MBEDTLS_CCM_C)
    { MBEDTLS_CIPHER_CAMELLIA_128_CCM,     &camellia_128_ccm_info },
    { MBEDTLS_CIPHER_CAMELLIA_192_CCM,     &camellia_192_ccm_info },
    { MBEDTLS_CIPHER_CAMELLIA_256_CCM,     &camellia_256_ccm_info },
    { MBEDTLS_CIPHER_CAMELLIA_128_CCM_STAR_NO_TAG,     &camellia_128_ccm_star_no_tag_info },
    { MBEDTLS_CIPHER_CAMELLIA_192_CCM_STAR_NO_TAG,     &camellia_192_ccm_star_no_tag_info },
    { MBEDTLS_CIPHER_CAMELLIA_256_CCM_STAR_NO_TAG,     &camellia_256_ccm_star_no_tag_info },
#endif
#endif /* MBEDTLS_CAMELLIA_C */

#if defined(MBEDTLS_ARIA_C)
    { MBEDTLS_CIPHER_ARIA_128_ECB,     &aria_128_ecb_info },
    { MBEDTLS_CIPHER_ARIA_192_ECB,     &aria_192_ecb_info },
    { MBEDTLS_CIPHER_ARIA_256_ECB,     &aria_256_ecb_info },
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    { MBEDTLS_CIPHER_ARIA_128_CBC,     &aria_128_cbc_info },
    { MBEDTLS_CIPHER_ARIA_192_CBC,     &aria_192_cbc_info },
    { MBEDTLS_CIPHER_ARIA_256_CBC,     &aria_256_cbc_info },
#endif
#if defined(MBEDTLS_CIPHER_MODE_CFB)
    { MBEDTLS_CIPHER_ARIA_128_CFB128,  &aria_128_cfb128_info },
    { MBEDTLS_CIPHER_ARIA_192_CFB128,  &aria_192_cfb128_info },
    { MBEDTLS_CIPHER_ARIA_256_CFB128,  &aria_256_cfb128_info },
#endif
#if defined(MBEDTLS_CIPHER_MODE_CTR)
    { MBEDTLS_CIPHER_ARIA_128_CTR,     &aria_128_ctr_info },
    { MBEDTLS_CIPHER_ARIA_192_CTR,     &aria_192_ctr_info },
    { MBEDTLS_CIPHER_ARIA_256_CTR,     &aria_256_ctr_info },
#endif
#if defined(MBEDTLS_GCM_C)
    { MBEDTLS_CIPHER_ARIA_128_GCM,     &aria_128_gcm_info },
    { MBEDTLS_CIPHER_ARIA_192_GCM,     &aria_192_gcm_info },
    { MBEDTLS_CIPHER_ARIA_256_GCM,     &aria_256_gcm_info },
#endif
#if defined(MBEDTLS_CCM_C)
    { MBEDTLS_CIPHER_ARIA_128_CCM,     &aria_128_ccm_info },
    { MBEDTLS_CIPHER_ARIA_192_CCM,     &aria_192_ccm_info },
    { MBEDTLS_CIPHER_ARIA_256_CCM,     &aria_256_ccm_info },
    { MBEDTLS_CIPHER_ARIA_128_CCM_STAR_NO_TAG,     &aria_128_ccm_star_no_tag_info },
    { MBEDTLS_CIPHER_ARIA_192_CCM_STAR_NO_TAG,     &aria_192_ccm_star_no_tag_info },
    { MBEDTLS_CIPHER_ARIA_256_CCM_STAR_NO_TAG,     &aria_256_ccm_star_no_tag_info },
#endif
#endif /* MBEDTLS_ARIA_C */

#if defined(MBEDTLS_CHACHA20_C)
    { MBEDTLS_CIPHER_CHACHA20,             &chacha20_info },
#endif

#if defined(MBEDTLS_CHACHAPOLY_C)
    { MBEDTLS_CIPHER_CHACHA20_POLY1305,    &chachapoly_info },
#endif

    { MBEDTLS_CIPHER_NONE, NULL }
};

#define NUM_CIPHERS (sizeof(mbedtls_cipher_definitions) /      \
                     sizeof(mbedtls_cipher_definitions[0]))
int mbedtls_cipher_supported[NUM_CIPHERS];

const mbedtls_cipher_base_t * const mbedtls_cipher_base_lookup_table[] = {
#if defined(MBEDTLS_AES_C)
    [MBEDTLS_CIPHER_BASE_INDEX_AES] = &aes_info,
#endif
#if defined(MBEDTLS_ARIA_C)
    [MBEDTLS_CIPHER_BASE_INDEX_ARIA] = &aria_info,
#endif
#if defined(MBEDTLS_CAMELLIA_C)
    [MBEDTLS_CIPHER_BASE_INDEX_CAMELLIA] = &camellia_info,
#endif
#if defined(MBEDTLS_CIPHER_HAVE_CCM_AES_VIA_LEGACY_OR_USE_PSA)
    [MBEDTLS_CIPHER_BASE_INDEX_CCM_AES] = &ccm_aes_info,
#endif
#if defined(MBEDTLS_CCM_C) && defined(MBEDTLS_ARIA_C)
    [MBEDTLS_CIPHER_BASE_INDEX_CCM_ARIA] = &ccm_aria_info,
#endif
#if defined(MBEDTLS_CCM_C) && defined(MBEDTLS_CAMELLIA_C)
    [MBEDTLS_CIPHER_BASE_INDEX_CCM_CAMELLIA] = &ccm_camellia_info,
#endif
#if defined(MBEDTLS_CHACHA20_C)
    [MBEDTLS_CIPHER_BASE_INDEX_CHACHA20_BASE] = &chacha20_base_info,
#endif
#if defined(MBEDTLS_CHACHAPOLY_C)
    [MBEDTLS_CIPHER_BASE_INDEX_CHACHAPOLY_BASE] = &chachapoly_base_info,
#endif
#if defined(MBEDTLS_CIPHER_HAVE_GCM_AES_VIA_LEGACY_OR_USE_PSA)
    [MBEDTLS_CIPHER_BASE_INDEX_GCM_AES] = &gcm_aes_info,
#endif
#if defined(MBEDTLS_GCM_C) && defined(MBEDTLS_ARIA_C)
    [MBEDTLS_CIPHER_BASE_INDEX_GCM_ARIA] = &gcm_aria_info,
#endif
#if defined(MBEDTLS_GCM_C) && defined(MBEDTLS_CAMELLIA_C)
    [MBEDTLS_CIPHER_BASE_INDEX_GCM_CAMELLIA] = &gcm_camellia_info,
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS) && defined(MBEDTLS_AES_C)
    [MBEDTLS_CIPHER_BASE_INDEX_XTS_AES] = &xts_aes_info
#endif
};

#endif /* MBEDTLS_CIPHER_C */
