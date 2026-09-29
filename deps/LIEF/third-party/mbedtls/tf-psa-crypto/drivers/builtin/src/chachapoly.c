/**
 * \file chachapoly.c
 *
 * \brief ChaCha20-Poly1305 AEAD construction based on RFC 7539.
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_CHACHAPOLY_C)

#include "mbedtls/private/chachapoly.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/private/error_common.h"
#include "mbedtls/constant_time.h"

#include <string.h>

#include "mbedtls/platform.h"

#define CHACHAPOLY_STATE_INIT       (0)
#define CHACHAPOLY_STATE_AAD        (1)
#define CHACHAPOLY_STATE_CIPHERTEXT (2)   /* Encrypting or decrypting */
#define CHACHAPOLY_STATE_FINISHED   (3)

/**
 * \brief           Adds nul bytes to pad the AAD for Poly1305.
 *
 * \param ctx       The ChaCha20-Poly1305 context.
 */
static int chachapoly_pad_aad(mbedtls_chachapoly_context *ctx)
{
    uint32_t partial_block_len = (uint32_t) (ctx->aad_len % 16U);
    unsigned char zeroes[15];

    if (partial_block_len == 0U) {
        return 0;
    }

    memset(zeroes, 0, sizeof(zeroes));

    return mbedtls_poly1305_update(&ctx->poly1305_ctx,
                                   zeroes,
                                   16U - partial_block_len);
}

/**
 * \brief           Adds nul bytes to pad the ciphertext for Poly1305.
 *
 * \param ctx       The ChaCha20-Poly1305 context.
 */
static int chachapoly_pad_ciphertext(mbedtls_chachapoly_context *ctx)
{
    uint32_t partial_block_len = (uint32_t) (ctx->ciphertext_len % 16U);
    unsigned char zeroes[15];

    if (partial_block_len == 0U) {
        return 0;
    }

    memset(zeroes, 0, sizeof(zeroes));
    return mbedtls_poly1305_update(&ctx->poly1305_ctx,
                                   zeroes,
                                   16U - partial_block_len);
}

void mbedtls_chachapoly_init(mbedtls_chachapoly_context *ctx)
{
    mbedtls_chacha20_init(&ctx->chacha20_ctx);
    mbedtls_poly1305_init(&ctx->poly1305_ctx);
    ctx->aad_len        = 0U;
    ctx->ciphertext_len = 0U;
    ctx->state          = CHACHAPOLY_STATE_INIT;
    ctx->mode           = MBEDTLS_CHACHAPOLY_ENCRYPT;
}

void mbedtls_chachapoly_free(mbedtls_chachapoly_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_chacha20_free(&ctx->chacha20_ctx);
    mbedtls_poly1305_free(&ctx->poly1305_ctx);
    ctx->aad_len        = 0U;
    ctx->ciphertext_len = 0U;
    ctx->state          = CHACHAPOLY_STATE_INIT;
    ctx->mode           = MBEDTLS_CHACHAPOLY_ENCRYPT;
}

int mbedtls_chachapoly_setkey(mbedtls_chachapoly_context *ctx,
                              const unsigned char key[32])
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ret = mbedtls_chacha20_setkey(&ctx->chacha20_ctx, key);

    return ret;
}

int mbedtls_chachapoly_starts(mbedtls_chachapoly_context *ctx,
                              const unsigned char nonce[12],
                              mbedtls_chachapoly_mode_t mode)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char poly1305_key[64];

    /* Set counter = 0, will be update to 1 when generating Poly1305 key */
    ret = mbedtls_chacha20_starts(&ctx->chacha20_ctx, nonce, 0U);
    if (ret != 0) {
        goto cleanup;
    }

    /* Generate the Poly1305 key by getting the ChaCha20 keystream output with
     * counter = 0.  This is the same as encrypting a buffer of zeroes.
     * Only the first 256-bits (32 bytes) of the key is used for Poly1305.
     * The other 256 bits are discarded.
     */
    memset(poly1305_key, 0, sizeof(poly1305_key));
    ret = mbedtls_chacha20_update(&ctx->chacha20_ctx, sizeof(poly1305_key),
                                  poly1305_key, poly1305_key);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_poly1305_starts(&ctx->poly1305_ctx, poly1305_key);

    if (ret == 0) {
        ctx->aad_len        = 0U;
        ctx->ciphertext_len = 0U;
        ctx->state          = CHACHAPOLY_STATE_AAD;
        ctx->mode           = mode;
    }

cleanup:
    mbedtls_platform_zeroize(poly1305_key, 64U);
    return ret;
}

int mbedtls_chachapoly_update_aad(mbedtls_chachapoly_context *ctx,
                                  const unsigned char *aad,
                                  size_t aad_len)
{
    if (ctx->state != CHACHAPOLY_STATE_AAD) {
        return MBEDTLS_ERR_CHACHAPOLY_BAD_STATE;
    }

    ctx->aad_len += aad_len;

    return mbedtls_poly1305_update(&ctx->poly1305_ctx, aad, aad_len);
}

int mbedtls_chachapoly_update(mbedtls_chachapoly_context *ctx,
                              size_t len,
                              const unsigned char *input,
                              unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if ((ctx->state != CHACHAPOLY_STATE_AAD) &&
        (ctx->state != CHACHAPOLY_STATE_CIPHERTEXT)) {
        return MBEDTLS_ERR_CHACHAPOLY_BAD_STATE;
    }

    if (ctx->state == CHACHAPOLY_STATE_AAD) {
        ctx->state = CHACHAPOLY_STATE_CIPHERTEXT;

        ret = chachapoly_pad_aad(ctx);
        if (ret != 0) {
            return ret;
        }
    }

    ctx->ciphertext_len += len;

    if (ctx->mode == MBEDTLS_CHACHAPOLY_ENCRYPT) {
        ret = mbedtls_chacha20_update(&ctx->chacha20_ctx, len, input, output);
        if (ret != 0) {
            return ret;
        }

        ret = mbedtls_poly1305_update(&ctx->poly1305_ctx, output, len);
        if (ret != 0) {
            return ret;
        }
    } else { /* DECRYPT */
        ret = mbedtls_poly1305_update(&ctx->poly1305_ctx, input, len);
        if (ret != 0) {
            return ret;
        }

        ret = mbedtls_chacha20_update(&ctx->chacha20_ctx, len, input, output);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

int mbedtls_chachapoly_finish(mbedtls_chachapoly_context *ctx,
                              unsigned char mac[16])
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char len_block[16];

    if (ctx->state == CHACHAPOLY_STATE_INIT) {
        return MBEDTLS_ERR_CHACHAPOLY_BAD_STATE;
    }

    if (ctx->state == CHACHAPOLY_STATE_AAD) {
        ret = chachapoly_pad_aad(ctx);
        if (ret != 0) {
            return ret;
        }
    } else if (ctx->state == CHACHAPOLY_STATE_CIPHERTEXT) {
        ret = chachapoly_pad_ciphertext(ctx);
        if (ret != 0) {
            return ret;
        }
    }

    ctx->state = CHACHAPOLY_STATE_FINISHED;

    /* The lengths of the AAD and ciphertext are processed by
     * Poly1305 as the final 128-bit block, encoded as little-endian integers.
     */
    MBEDTLS_PUT_UINT64_LE(ctx->aad_len, len_block, 0);
    MBEDTLS_PUT_UINT64_LE(ctx->ciphertext_len, len_block, 8);

    ret = mbedtls_poly1305_update(&ctx->poly1305_ctx, len_block, 16U);
    if (ret != 0) {
        return ret;
    }

    ret = mbedtls_poly1305_finish(&ctx->poly1305_ctx, mac);

    return ret;
}

static int chachapoly_crypt_and_tag(mbedtls_chachapoly_context *ctx,
                                    mbedtls_chachapoly_mode_t mode,
                                    size_t length,
                                    const unsigned char nonce[12],
                                    const unsigned char *aad,
                                    size_t aad_len,
                                    const unsigned char *input,
                                    unsigned char *output,
                                    unsigned char tag[16])
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ret = mbedtls_chachapoly_starts(ctx, nonce, mode);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_chachapoly_update_aad(ctx, aad, aad_len);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_chachapoly_update(ctx, length, input, output);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_chachapoly_finish(ctx, tag);

cleanup:
    return ret;
}

int mbedtls_chachapoly_encrypt_and_tag(mbedtls_chachapoly_context *ctx,
                                       size_t length,
                                       const unsigned char nonce[12],
                                       const unsigned char *aad,
                                       size_t aad_len,
                                       const unsigned char *input,
                                       unsigned char *output,
                                       unsigned char tag[16])
{
    return chachapoly_crypt_and_tag(ctx, MBEDTLS_CHACHAPOLY_ENCRYPT,
                                    length, nonce, aad, aad_len,
                                    input, output, tag);
}

int mbedtls_chachapoly_auth_decrypt(mbedtls_chachapoly_context *ctx,
                                    size_t length,
                                    const unsigned char nonce[12],
                                    const unsigned char *aad,
                                    size_t aad_len,
                                    const unsigned char tag[16],
                                    const unsigned char *input,
                                    unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char check_tag[16];
    int diff;

    if ((ret = chachapoly_crypt_and_tag(ctx,
                                        MBEDTLS_CHACHAPOLY_DECRYPT, length, nonce,
                                        aad, aad_len, input, output, check_tag)) != 0) {
        return ret;
    }

    /* Check tag in "constant-time" */
    diff = mbedtls_ct_memcmp(tag, check_tag, sizeof(check_tag));

    if (diff != 0) {
        mbedtls_platform_zeroize(output, length);
        return MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED;
    }

    return 0;
}

#if defined(MBEDTLS_SELF_TEST)

static const unsigned char test_key[1][32] =
{
    {
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f
    }
};

static const unsigned char test_nonce[1][12] =
{
    {
        0x07, 0x00, 0x00, 0x00,                         /* 32-bit common part */
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47  /* 64-bit IV */
    }
};

static const unsigned char test_aad[1][12] =
{
    {
        0x50, 0x51, 0x52, 0x53, 0xc0, 0xc1, 0xc2, 0xc3,
        0xc4, 0xc5, 0xc6, 0xc7
    }
};

static const size_t test_aad_len[1] =
{
    12U
};

static const unsigned char test_input[1][114] =
{
    {
        0x4c, 0x61, 0x64, 0x69, 0x65, 0x73, 0x20, 0x61,
        0x6e, 0x64, 0x20, 0x47, 0x65, 0x6e, 0x74, 0x6c,
        0x65, 0x6d, 0x65, 0x6e, 0x20, 0x6f, 0x66, 0x20,
        0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x61, 0x73,
        0x73, 0x20, 0x6f, 0x66, 0x20, 0x27, 0x39, 0x39,
        0x3a, 0x20, 0x49, 0x66, 0x20, 0x49, 0x20, 0x63,
        0x6f, 0x75, 0x6c, 0x64, 0x20, 0x6f, 0x66, 0x66,
        0x65, 0x72, 0x20, 0x79, 0x6f, 0x75, 0x20, 0x6f,
        0x6e, 0x6c, 0x79, 0x20, 0x6f, 0x6e, 0x65, 0x20,
        0x74, 0x69, 0x70, 0x20, 0x66, 0x6f, 0x72, 0x20,
        0x74, 0x68, 0x65, 0x20, 0x66, 0x75, 0x74, 0x75,
        0x72, 0x65, 0x2c, 0x20, 0x73, 0x75, 0x6e, 0x73,
        0x63, 0x72, 0x65, 0x65, 0x6e, 0x20, 0x77, 0x6f,
        0x75, 0x6c, 0x64, 0x20, 0x62, 0x65, 0x20, 0x69,
        0x74, 0x2e
    }
};

static const unsigned char test_output[1][114] =
{
    {
        0xd3, 0x1a, 0x8d, 0x34, 0x64, 0x8e, 0x60, 0xdb,
        0x7b, 0x86, 0xaf, 0xbc, 0x53, 0xef, 0x7e, 0xc2,
        0xa4, 0xad, 0xed, 0x51, 0x29, 0x6e, 0x08, 0xfe,
        0xa9, 0xe2, 0xb5, 0xa7, 0x36, 0xee, 0x62, 0xd6,
        0x3d, 0xbe, 0xa4, 0x5e, 0x8c, 0xa9, 0x67, 0x12,
        0x82, 0xfa, 0xfb, 0x69, 0xda, 0x92, 0x72, 0x8b,
        0x1a, 0x71, 0xde, 0x0a, 0x9e, 0x06, 0x0b, 0x29,
        0x05, 0xd6, 0xa5, 0xb6, 0x7e, 0xcd, 0x3b, 0x36,
        0x92, 0xdd, 0xbd, 0x7f, 0x2d, 0x77, 0x8b, 0x8c,
        0x98, 0x03, 0xae, 0xe3, 0x28, 0x09, 0x1b, 0x58,
        0xfa, 0xb3, 0x24, 0xe4, 0xfa, 0xd6, 0x75, 0x94,
        0x55, 0x85, 0x80, 0x8b, 0x48, 0x31, 0xd7, 0xbc,
        0x3f, 0xf4, 0xde, 0xf0, 0x8e, 0x4b, 0x7a, 0x9d,
        0xe5, 0x76, 0xd2, 0x65, 0x86, 0xce, 0xc6, 0x4b,
        0x61, 0x16
    }
};

static const size_t test_input_len[1] =
{
    114U
};

static const unsigned char test_mac[1][16] =
{
    {
        0x1a, 0xe1, 0x0b, 0x59, 0x4f, 0x09, 0xe2, 0x6a,
        0x7e, 0x90, 0x2e, 0xcb, 0xd0, 0x60, 0x06, 0x91
    }
};

/* Make sure no other definition is already present. */
#undef ASSERT

#define ASSERT(cond, args)            \
    do                                  \
    {                                   \
        if (!(cond))                \
        {                               \
            if (verbose != 0)          \
            mbedtls_printf args;    \
                                        \
            return -1;               \
        }                               \
    }                                   \
    while (0)

int mbedtls_chachapoly_self_test(int verbose)
{
    mbedtls_chachapoly_context ctx;
    unsigned i;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char output[200];
    unsigned char mac[16];

    for (i = 0U; i < 1U; i++) {
        if (verbose != 0) {
            mbedtls_printf("  ChaCha20-Poly1305 test %u ", i);
        }

        mbedtls_chachapoly_init(&ctx);

        ret = mbedtls_chachapoly_setkey(&ctx, test_key[i]);
        ASSERT(0 == ret, ("setkey() error code: %i\n", ret));

        ret = mbedtls_chachapoly_encrypt_and_tag(&ctx,
                                                 test_input_len[i],
                                                 test_nonce[i],
                                                 test_aad[i],
                                                 test_aad_len[i],
                                                 test_input[i],
                                                 output,
                                                 mac);

        ASSERT(0 == ret, ("crypt_and_tag() error code: %i\n", ret));

        ASSERT(0 == memcmp(output, test_output[i], test_input_len[i]),
               ("failure (wrong output)\n"));

        ASSERT(0 == memcmp(mac, test_mac[i], 16U),
               ("failure (wrong MAC)\n"));

        mbedtls_chachapoly_free(&ctx);

        if (verbose != 0) {
            mbedtls_printf("passed\n");
        }
    }

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    return 0;
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_CHACHAPOLY_C */
