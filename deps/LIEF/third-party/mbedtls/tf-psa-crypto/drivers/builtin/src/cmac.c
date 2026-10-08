/**
 * \file cmac.c
 *
 * \brief NIST SP800-38B compliant CMAC implementation for AES and 3DES
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * References:
 *
 * - NIST SP 800-38B Recommendation for Block Cipher Modes of Operation: The
 *      CMAC Mode for Authentication
 *   http://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-38b.pdf
 *
 * - RFC 4493 - The AES-CMAC Algorithm
 *   https://tools.ietf.org/html/rfc4493
 *
 * - RFC 4615 - The Advanced Encryption Standard-Cipher-based Message
 *      Authentication Code-Pseudo-Random Function-128 (AES-CMAC-PRF-128)
 *      Algorithm for the Internet Key Exchange Protocol (IKE)
 *   https://tools.ietf.org/html/rfc4615
 *
 *   Additional test vectors: ISO/IEC 9797-1
 *
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_CMAC_C)

#include "mbedtls/private/cmac.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/private/error_common.h"
#include "mbedtls/platform.h"
#include "constant_time_internal.h"

#include <string.h>

/*
 * Multiplication by u in the Galois field of GF(2^n)
 *
 * As explained in NIST SP 800-38B, this can be computed:
 *
 *   If MSB(p) = 0, then p = (p << 1)
 *   If MSB(p) = 1, then p = (p << 1) ^ R_n
 *   with R_64 = 0x1B and  R_128 = 0x87
 *
 * Input and output MUST NOT point to the same buffer
 * Block size must be 8 bytes or 16 bytes - the block sizes for DES and AES.
 */
static int cmac_multiply_by_u(unsigned char *output,
                              const unsigned char *input,
                              size_t blocksize)
{
    const unsigned char R_128 = 0x87;
    unsigned char R_n;
    uint32_t overflow = 0x00;
    int i;

    if (blocksize == MBEDTLS_AES_BLOCK_SIZE) {
        R_n = R_128;
    } else {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    for (i = (int) blocksize - 4; i >= 0; i -= 4) {
        uint32_t i32 = MBEDTLS_GET_UINT32_BE(&input[i], 0);
        uint32_t new_overflow = i32 >> 31;
        i32 = (i32 << 1) | overflow;
        MBEDTLS_PUT_UINT32_BE(i32, &output[i], 0);
        overflow = new_overflow;
    }

    R_n = (unsigned char) mbedtls_ct_uint_if_else_0(mbedtls_ct_bool(input[0] >> 7), R_n);
    output[blocksize - 1] ^= R_n;

    return 0;
}

/*
 * Generate subkeys
 *
 * - as specified by RFC 4493, section 2.3 Subkey Generation Algorithm
 */
static int cmac_generate_subkeys(mbedtls_cipher_context_t *ctx,
                                 unsigned char *K1, unsigned char *K2)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char L[MBEDTLS_CMAC_MAX_BLOCK_SIZE];
    size_t olen, block_size;

    mbedtls_platform_zeroize(L, sizeof(L));

    block_size = mbedtls_cipher_info_get_block_size(ctx->cipher_info);

    /* Calculate Ek(0) */
    if ((ret = mbedtls_cipher_update(ctx, L, block_size, L, &olen)) != 0) {
        goto exit;
    }

    /*
     * Generate K1 and K2
     */
    if ((ret = cmac_multiply_by_u(K1, L, block_size)) != 0) {
        goto exit;
    }

    if ((ret = cmac_multiply_by_u(K2, K1, block_size)) != 0) {
        goto exit;
    }

exit:
    mbedtls_platform_zeroize(L, sizeof(L));

    return ret;
}

/*
 * Create padded last block from (partial) last block.
 *
 * We can't use the padding option from the cipher layer, as it only works for
 * CBC and we use ECB mode, and anyway we need to XOR K1 or K2 in addition.
 */
static void cmac_pad(unsigned char padded_block[MBEDTLS_CMAC_MAX_BLOCK_SIZE],
                     size_t padded_block_len,
                     const unsigned char *last_block,
                     size_t last_block_len)
{
    size_t j;

    for (j = 0; j < padded_block_len; j++) {
        if (j < last_block_len) {
            padded_block[j] = last_block[j];
        } else if (j == last_block_len) {
            padded_block[j] = 0x80;
        } else {
            padded_block[j] = 0x00;
        }
    }
}

int mbedtls_cipher_cmac_starts(mbedtls_cipher_context_t *ctx,
                               const unsigned char *key, size_t keybits)
{
    mbedtls_cipher_type_t type;
    mbedtls_cmac_context_t *cmac_ctx;
    int retval;

    if (ctx == NULL || ctx->cipher_info == NULL || key == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    if ((retval = mbedtls_cipher_setkey(ctx, key, (int) keybits,
                                        MBEDTLS_ENCRYPT)) != 0) {
        return retval;
    }

    type = mbedtls_cipher_info_get_type(ctx->cipher_info);

    switch (type) {
        case MBEDTLS_CIPHER_AES_128_ECB:
        case MBEDTLS_CIPHER_AES_192_ECB:
        case MBEDTLS_CIPHER_AES_256_ECB:
            break;
        default:
            return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    /* Allocated and initialise in the cipher context memory for the CMAC
     * context */
    cmac_ctx = mbedtls_calloc(1, sizeof(mbedtls_cmac_context_t));
    if (cmac_ctx == NULL) {
        return MBEDTLS_ERR_CIPHER_ALLOC_FAILED;
    }

    ctx->cmac_ctx = cmac_ctx;

    mbedtls_platform_zeroize(cmac_ctx->state, sizeof(cmac_ctx->state));

    return 0;
}

int mbedtls_cipher_cmac_update(mbedtls_cipher_context_t *ctx,
                               const unsigned char *input, size_t ilen)
{
    mbedtls_cmac_context_t *cmac_ctx;
    unsigned char *state;
    int ret = 0;
    size_t n, j, olen, block_size;

    if (ctx == NULL || ctx->cipher_info == NULL || input == NULL ||
        ctx->cmac_ctx == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    cmac_ctx = ctx->cmac_ctx;
    block_size = mbedtls_cipher_info_get_block_size(ctx->cipher_info);
    state = ctx->cmac_ctx->state;

    /* Without the MBEDTLS_ASSUME below, gcc -O3 will generate a warning of the form
     * error: writing 16 bytes into a region of size 0 [-Werror=stringop-overflow=] */
    MBEDTLS_ASSUME(block_size <= MBEDTLS_CMAC_MAX_BLOCK_SIZE);

    /* Is there data still to process from the last call, that's greater in
     * size than a block? */
    if (cmac_ctx->unprocessed_len > 0 &&
        ilen > block_size - cmac_ctx->unprocessed_len) {
        memcpy(&cmac_ctx->unprocessed_block[cmac_ctx->unprocessed_len],
               input,
               block_size - cmac_ctx->unprocessed_len);

        mbedtls_xor_no_simd(state, cmac_ctx->unprocessed_block, state, block_size);

        if ((ret = mbedtls_cipher_update(ctx, state, block_size, state,
                                         &olen)) != 0) {
            goto exit;
        }

        input += block_size - cmac_ctx->unprocessed_len;
        ilen -= block_size - cmac_ctx->unprocessed_len;
        cmac_ctx->unprocessed_len = 0;
    }

    /* n is the number of blocks including any final partial block */
    n = (ilen + block_size - 1) / block_size;

    /* Iterate across the input data in block sized chunks, excluding any
     * final partial or complete block */
    for (j = 1; j < n; j++) {
        mbedtls_xor_no_simd(state, input, state, block_size);

        if ((ret = mbedtls_cipher_update(ctx, state, block_size, state,
                                         &olen)) != 0) {
            goto exit;
        }

        ilen -= block_size;
        input += block_size;
    }

    /* If there is data left over that wasn't aligned to a block */
    if (ilen > 0) {
        memcpy(&cmac_ctx->unprocessed_block[cmac_ctx->unprocessed_len],
               input,
               ilen);
        cmac_ctx->unprocessed_len += ilen;
    }

exit:
    return ret;
}

int mbedtls_cipher_cmac_finish(mbedtls_cipher_context_t *ctx,
                               unsigned char *output)
{
    mbedtls_cmac_context_t *cmac_ctx;
    unsigned char *state, *last_block;
    unsigned char K1[MBEDTLS_CMAC_MAX_BLOCK_SIZE];
    unsigned char K2[MBEDTLS_CMAC_MAX_BLOCK_SIZE];
    unsigned char M_last[MBEDTLS_CMAC_MAX_BLOCK_SIZE];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t olen, block_size;

    if (ctx == NULL || ctx->cipher_info == NULL || ctx->cmac_ctx == NULL ||
        output == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    cmac_ctx = ctx->cmac_ctx;
    block_size = mbedtls_cipher_info_get_block_size(ctx->cipher_info);
    MBEDTLS_ASSUME(block_size <= MBEDTLS_CMAC_MAX_BLOCK_SIZE); // silence GCC warning
    state = cmac_ctx->state;

    mbedtls_platform_zeroize(K1, sizeof(K1));
    mbedtls_platform_zeroize(K2, sizeof(K2));
    cmac_generate_subkeys(ctx, K1, K2);

    last_block = cmac_ctx->unprocessed_block;

    /* Calculate last block */
    if (cmac_ctx->unprocessed_len < block_size) {
        cmac_pad(M_last, block_size, last_block, cmac_ctx->unprocessed_len);
        mbedtls_xor(M_last, M_last, K2, block_size);
    } else {
        /* Last block is complete block */
        mbedtls_xor(M_last, last_block, K1, block_size);
    }


    mbedtls_xor(state, M_last, state, block_size);
    if ((ret = mbedtls_cipher_update(ctx, state, block_size, state,
                                     &olen)) != 0) {
        goto exit;
    }

    memcpy(output, state, block_size);

exit:
    /* Wipe the generated keys on the stack, and any other transients to avoid
     * side channel leakage */
    mbedtls_platform_zeroize(K1, sizeof(K1));
    mbedtls_platform_zeroize(K2, sizeof(K2));

    cmac_ctx->unprocessed_len = 0;
    mbedtls_platform_zeroize(cmac_ctx->unprocessed_block,
                             sizeof(cmac_ctx->unprocessed_block));

    mbedtls_platform_zeroize(state, MBEDTLS_CMAC_MAX_BLOCK_SIZE);
    return ret;
}

int mbedtls_cipher_cmac_reset(mbedtls_cipher_context_t *ctx)
{
    mbedtls_cmac_context_t *cmac_ctx;

    if (ctx == NULL || ctx->cipher_info == NULL || ctx->cmac_ctx == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    cmac_ctx = ctx->cmac_ctx;

    /* Reset the internal state */
    cmac_ctx->unprocessed_len = 0;
    mbedtls_platform_zeroize(cmac_ctx->unprocessed_block,
                             sizeof(cmac_ctx->unprocessed_block));
    mbedtls_platform_zeroize(cmac_ctx->state,
                             sizeof(cmac_ctx->state));

    return 0;
}

int mbedtls_cipher_cmac(const mbedtls_cipher_info_t *cipher_info,
                        const unsigned char *key, size_t keylen,
                        const unsigned char *input, size_t ilen,
                        unsigned char *output)
{
    mbedtls_cipher_context_t ctx;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (cipher_info == NULL || key == NULL || input == NULL || output == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    mbedtls_cipher_init(&ctx);

    if ((ret = mbedtls_cipher_setup(&ctx, cipher_info)) != 0) {
        goto exit;
    }

    ret = mbedtls_cipher_cmac_starts(&ctx, key, keylen);
    if (ret != 0) {
        goto exit;
    }

    ret = mbedtls_cipher_cmac_update(&ctx, input, ilen);
    if (ret != 0) {
        goto exit;
    }

    ret = mbedtls_cipher_cmac_finish(&ctx, output);

exit:
    mbedtls_cipher_free(&ctx);

    return ret;
}

#if defined(MBEDTLS_AES_C)
/*
 * Implementation of AES-CMAC-PRF-128 defined in RFC 4615
 */
int mbedtls_aes_cmac_prf_128(const unsigned char *key, size_t key_length,
                             const unsigned char *input, size_t in_len,
                             unsigned char output[16])
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const mbedtls_cipher_info_t *cipher_info;
    unsigned char zero_key[MBEDTLS_AES_BLOCK_SIZE];
    unsigned char int_key[MBEDTLS_AES_BLOCK_SIZE];

    if (key == NULL || input == NULL || output == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
    if (cipher_info == NULL) {
        /* Failing at this point must be due to a build issue */
        ret = MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE;
        goto exit;
    }

    if (key_length == MBEDTLS_AES_BLOCK_SIZE) {
        /* Use key as is */
        memcpy(int_key, key, MBEDTLS_AES_BLOCK_SIZE);
    } else {
        memset(zero_key, 0, MBEDTLS_AES_BLOCK_SIZE);

        ret = mbedtls_cipher_cmac(cipher_info, zero_key, 128, key,
                                  key_length, int_key);
        if (ret != 0) {
            goto exit;
        }
    }

    ret = mbedtls_cipher_cmac(cipher_info, int_key, 128, input, in_len,
                              output);

exit:
    mbedtls_platform_zeroize(int_key, sizeof(int_key));

    return ret;
}
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_SELF_TEST)
/*
 * CMAC test data for SP800-38B
 * http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/AES_CMAC.pdf
 * http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/TDES_CMAC.pdf
 *
 * AES-CMAC-PRF-128 test data from RFC 4615
 * https://tools.ietf.org/html/rfc4615#page-4
 */

#define NB_CMAC_TESTS_PER_KEY 4
#define NB_PRF_TESTS 3

#if defined(MBEDTLS_AES_C)
/* All CMAC test inputs are truncated from the same 64 byte buffer. */
static const unsigned char test_message[] = {
    /* PT */
    0x6b, 0xc1, 0xbe, 0xe2,     0x2e, 0x40, 0x9f, 0x96,
    0xe9, 0x3d, 0x7e, 0x11,     0x73, 0x93, 0x17, 0x2a,
    0xae, 0x2d, 0x8a, 0x57,     0x1e, 0x03, 0xac, 0x9c,
    0x9e, 0xb7, 0x6f, 0xac,     0x45, 0xaf, 0x8e, 0x51,
    0x30, 0xc8, 0x1c, 0x46,     0xa3, 0x5c, 0xe4, 0x11,
    0xe5, 0xfb, 0xc1, 0x19,     0x1a, 0x0a, 0x52, 0xef,
    0xf6, 0x9f, 0x24, 0x45,     0xdf, 0x4f, 0x9b, 0x17,
    0xad, 0x2b, 0x41, 0x7b,     0xe6, 0x6c, 0x37, 0x10
};
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_AES_C)
/* Truncation point of message for AES CMAC tests  */
static const  unsigned int  aes_message_lengths[NB_CMAC_TESTS_PER_KEY] = {
    /* Mlen */
    0,
    16,
    20,
    64
};

/* CMAC-AES128 Test Data */
static const unsigned char aes_128_key[16] = {
    0x2b, 0x7e, 0x15, 0x16,     0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88,     0x09, 0xcf, 0x4f, 0x3c
};
static const unsigned char aes_128_subkeys[2][MBEDTLS_AES_BLOCK_SIZE] = {
    {
        /* K1 */
        0xfb, 0xee, 0xd6, 0x18,     0x35, 0x71, 0x33, 0x66,
        0x7c, 0x85, 0xe0, 0x8f,     0x72, 0x36, 0xa8, 0xde
    },
    {
        /* K2 */
        0xf7, 0xdd, 0xac, 0x30,     0x6a, 0xe2, 0x66, 0xcc,
        0xf9, 0x0b, 0xc1, 0x1e,     0xe4, 0x6d, 0x51, 0x3b
    }
};
static const unsigned char aes_128_expected_result[NB_CMAC_TESTS_PER_KEY][MBEDTLS_AES_BLOCK_SIZE] =
{
    {
        /* Example #1 */
        0xbb, 0x1d, 0x69, 0x29,     0xe9, 0x59, 0x37, 0x28,
        0x7f, 0xa3, 0x7d, 0x12,     0x9b, 0x75, 0x67, 0x46
    },
    {
        /* Example #2 */
        0x07, 0x0a, 0x16, 0xb4,     0x6b, 0x4d, 0x41, 0x44,
        0xf7, 0x9b, 0xdd, 0x9d,     0xd0, 0x4a, 0x28, 0x7c
    },
    {
        /* Example #3 */
        0x7d, 0x85, 0x44, 0x9e,     0xa6, 0xea, 0x19, 0xc8,
        0x23, 0xa7, 0xbf, 0x78,     0x83, 0x7d, 0xfa, 0xde
    },
    {
        /* Example #4 */
        0x51, 0xf0, 0xbe, 0xbf,     0x7e, 0x3b, 0x9d, 0x92,
        0xfc, 0x49, 0x74, 0x17,     0x79, 0x36, 0x3c, 0xfe
    }
};

/* CMAC-AES192 Test Data */
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const unsigned char aes_192_key[24] = {
    0x8e, 0x73, 0xb0, 0xf7,     0xda, 0x0e, 0x64, 0x52,
    0xc8, 0x10, 0xf3, 0x2b,     0x80, 0x90, 0x79, 0xe5,
    0x62, 0xf8, 0xea, 0xd2,     0x52, 0x2c, 0x6b, 0x7b
};
static const unsigned char aes_192_subkeys[2][MBEDTLS_AES_BLOCK_SIZE] = {
    {
        /* K1 */
        0x44, 0x8a, 0x5b, 0x1c,     0x93, 0x51, 0x4b, 0x27,
        0x3e, 0xe6, 0x43, 0x9d,     0xd4, 0xda, 0xa2, 0x96
    },
    {
        /* K2 */
        0x89, 0x14, 0xb6, 0x39,     0x26, 0xa2, 0x96, 0x4e,
        0x7d, 0xcc, 0x87, 0x3b,     0xa9, 0xb5, 0x45, 0x2c
    }
};
static const unsigned char aes_192_expected_result[NB_CMAC_TESTS_PER_KEY][MBEDTLS_AES_BLOCK_SIZE] =
{
    {
        /* Example #1 */
        0xd1, 0x7d, 0xdf, 0x46,     0xad, 0xaa, 0xcd, 0xe5,
        0x31, 0xca, 0xc4, 0x83,     0xde, 0x7a, 0x93, 0x67
    },
    {
        /* Example #2 */
        0x9e, 0x99, 0xa7, 0xbf,     0x31, 0xe7, 0x10, 0x90,
        0x06, 0x62, 0xf6, 0x5e,     0x61, 0x7c, 0x51, 0x84
    },
    {
        /* Example #3 */
        0x3d, 0x75, 0xc1, 0x94,     0xed, 0x96, 0x07, 0x04,
        0x44, 0xa9, 0xfa, 0x7e,     0xc7, 0x40, 0xec, 0xf8
    },
    {
        /* Example #4 */
        0xa1, 0xd5, 0xdf, 0x0e,     0xed, 0x79, 0x0f, 0x79,
        0x4d, 0x77, 0x58, 0x96,     0x59, 0xf3, 0x9a, 0x11
    }
};
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */

/* CMAC-AES256 Test Data */
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
static const unsigned char aes_256_key[32] = {
    0x60, 0x3d, 0xeb, 0x10,     0x15, 0xca, 0x71, 0xbe,
    0x2b, 0x73, 0xae, 0xf0,     0x85, 0x7d, 0x77, 0x81,
    0x1f, 0x35, 0x2c, 0x07,     0x3b, 0x61, 0x08, 0xd7,
    0x2d, 0x98, 0x10, 0xa3,     0x09, 0x14, 0xdf, 0xf4
};
static const unsigned char aes_256_subkeys[2][MBEDTLS_AES_BLOCK_SIZE] = {
    {
        /* K1 */
        0xca, 0xd1, 0xed, 0x03,     0x29, 0x9e, 0xed, 0xac,
        0x2e, 0x9a, 0x99, 0x80,     0x86, 0x21, 0x50, 0x2f
    },
    {
        /* K2 */
        0x95, 0xa3, 0xda, 0x06,     0x53, 0x3d, 0xdb, 0x58,
        0x5d, 0x35, 0x33, 0x01,     0x0c, 0x42, 0xa0, 0xd9
    }
};
static const unsigned char aes_256_expected_result[NB_CMAC_TESTS_PER_KEY][MBEDTLS_AES_BLOCK_SIZE] =
{
    {
        /* Example #1 */
        0x02, 0x89, 0x62, 0xf6,     0x1b, 0x7b, 0xf8, 0x9e,
        0xfc, 0x6b, 0x55, 0x1f,     0x46, 0x67, 0xd9, 0x83
    },
    {
        /* Example #2 */
        0x28, 0xa7, 0x02, 0x3f,     0x45, 0x2e, 0x8f, 0x82,
        0xbd, 0x4b, 0xf2, 0x8d,     0x8c, 0x37, 0xc3, 0x5c
    },
    {
        /* Example #3 */
        0x15, 0x67, 0x27, 0xdc,     0x08, 0x78, 0x94, 0x4a,
        0x02, 0x3c, 0x1f, 0xe0,     0x3b, 0xad, 0x6d, 0x93
    },
    {
        /* Example #4 */
        0xe1, 0x99, 0x21, 0x90,     0x54, 0x9f, 0x6e, 0xd5,
        0x69, 0x6a, 0x2c, 0x05,     0x6c, 0x31, 0x54, 0x10
    }
};
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_AES_C)
/* AES AES-CMAC-PRF-128 Test Data */
static const unsigned char PRFK[] = {
    /* Key */
    0x00, 0x01, 0x02, 0x03,     0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b,     0x0c, 0x0d, 0x0e, 0x0f,
    0xed, 0xcb
};

/* Sizes in bytes */
static const size_t PRFKlen[NB_PRF_TESTS] = {
    18,
    16,
    10
};

/* Message */
static const unsigned char PRFM[] = {
    0x00, 0x01, 0x02, 0x03,     0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b,     0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13
};

static const unsigned char PRFT[NB_PRF_TESTS][16] = {
    {
        0x84, 0xa3, 0x48, 0xa4,     0xa4, 0x5d, 0x23, 0x5b,
        0xab, 0xff, 0xfc, 0x0d,     0x2b, 0x4d, 0xa0, 0x9a
    },
    {
        0x98, 0x0a, 0xe8, 0x7b,     0x5f, 0x4c, 0x9c, 0x52,
        0x14, 0xf5, 0xb6, 0xa8,     0x45, 0x5e, 0x4c, 0x2d
    },
    {
        0x29, 0x0d, 0x9e, 0x11,     0x2e, 0xdb, 0x09, 0xee,
        0x14, 0x1f, 0xcf, 0x64,     0xc0, 0xb7, 0x2f, 0x3d
    }
};
#endif /* MBEDTLS_AES_C */

static int cmac_test_subkeys(int verbose,
                             const char *testname,
                             const unsigned char *key,
                             int keybits,
                             const unsigned char *subkeys,
                             mbedtls_cipher_type_t cipher_type,
                             int block_size,
                             int num_tests)
{
    int i, ret = 0;
    mbedtls_cipher_context_t ctx;
    const mbedtls_cipher_info_t *cipher_info;
    unsigned char K1[MBEDTLS_CMAC_MAX_BLOCK_SIZE];
    unsigned char K2[MBEDTLS_CMAC_MAX_BLOCK_SIZE];

    cipher_info = mbedtls_cipher_info_from_type(cipher_type);
    if (cipher_info == NULL) {
        /* Failing at this point must be due to a build issue */
        return MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE;
    }

    for (i = 0; i < num_tests; i++) {
        if (verbose != 0) {
            mbedtls_printf("  %s CMAC subkey #%d: ", testname, i + 1);
        }

        mbedtls_cipher_init(&ctx);

        if ((ret = mbedtls_cipher_setup(&ctx, cipher_info)) != 0) {
            if (verbose != 0) {
                mbedtls_printf("test execution failed\n");
            }

            goto cleanup;
        }

        if ((ret = mbedtls_cipher_setkey(&ctx, key, keybits,
                                         MBEDTLS_ENCRYPT)) != 0) {
            /* When CMAC is implemented by an alternative implementation, or
             * the underlying primitive itself is implemented alternatively,
             * AES-192 may be unavailable. This should not cause the selftest
             * function to fail. */
            if ((ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED ||
                 ret == MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE) &&
                cipher_type == MBEDTLS_CIPHER_AES_192_ECB) {
                if (verbose != 0) {
                    mbedtls_printf("skipped\n");
                }
                goto next_test;
            }

            if (verbose != 0) {
                mbedtls_printf("test execution failed\n");
            }

            goto cleanup;
        }

        ret = cmac_generate_subkeys(&ctx, K1, K2);
        if (ret != 0) {
            if (verbose != 0) {
                mbedtls_printf("failed\n");
            }

            goto cleanup;
        }

        if ((ret = memcmp(K1, subkeys, block_size)) != 0  ||
            (ret = memcmp(K2, &subkeys[block_size], block_size)) != 0) {
            if (verbose != 0) {
                mbedtls_printf("failed\n");
            }

            goto cleanup;
        }

        if (verbose != 0) {
            mbedtls_printf("passed\n");
        }

next_test:
        mbedtls_cipher_free(&ctx);
    }

    ret = 0;
    goto exit;

cleanup:
    mbedtls_cipher_free(&ctx);

exit:
    return ret;
}

static int cmac_test_wth_cipher(int verbose,
                                const char *testname,
                                const unsigned char *key,
                                int keybits,
                                const unsigned char *messages,
                                const unsigned int message_lengths[4],
                                const unsigned char *expected_result,
                                mbedtls_cipher_type_t cipher_type,
                                int block_size,
                                int num_tests)
{
    const mbedtls_cipher_info_t *cipher_info;
    int i, ret = 0;
    unsigned char output[MBEDTLS_CMAC_MAX_BLOCK_SIZE];

    cipher_info = mbedtls_cipher_info_from_type(cipher_type);
    if (cipher_info == NULL) {
        /* Failing at this point must be due to a build issue */
        ret = MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE;
        goto exit;
    }

    for (i = 0; i < num_tests; i++) {
        if (verbose != 0) {
            mbedtls_printf("  %s CMAC #%d: ", testname, i + 1);
        }

        if ((ret = mbedtls_cipher_cmac(cipher_info, key, keybits, messages,
                                       message_lengths[i], output)) != 0) {
            /* When CMAC is implemented by an alternative implementation, or
             * the underlying primitive itself is implemented alternatively,
             * AES-192 and/or 3DES may be unavailable. This should not cause
             * the selftest function to fail. */
            if ((ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED ||
                 ret == MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE) &&
                (cipher_type == MBEDTLS_CIPHER_AES_192_ECB)) {
                if (verbose != 0) {
                    mbedtls_printf("skipped\n");
                }
                continue;
            }

            if (verbose != 0) {
                mbedtls_printf("failed\n");
            }
            goto exit;
        }

        if ((ret = memcmp(output, &expected_result[i * block_size], block_size)) != 0) {
            if (verbose != 0) {
                mbedtls_printf("failed\n");
            }
            goto exit;
        }

        if (verbose != 0) {
            mbedtls_printf("passed\n");
        }
    }
    ret = 0;

exit:
    return ret;
}

#if defined(MBEDTLS_AES_C)
static int test_aes128_cmac_prf(int verbose)
{
    int i;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char output[MBEDTLS_AES_BLOCK_SIZE];

    for (i = 0; i < NB_PRF_TESTS; i++) {
        mbedtls_printf("  AES CMAC 128 PRF #%d: ", i);
        ret = mbedtls_aes_cmac_prf_128(PRFK, PRFKlen[i], PRFM, 20, output);
        if (ret != 0 ||
            memcmp(output, PRFT[i], MBEDTLS_AES_BLOCK_SIZE) != 0) {

            if (verbose != 0) {
                mbedtls_printf("failed\n");
            }

            return ret;
        } else if (verbose != 0) {
            mbedtls_printf("passed\n");
        }
    }
    return ret;
}
#endif /* MBEDTLS_AES_C */

int mbedtls_cmac_self_test(int verbose)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

#if defined(MBEDTLS_AES_C)
    /* AES-128 */
    if ((ret = cmac_test_subkeys(verbose,
                                 "AES 128",
                                 aes_128_key,
                                 128,
                                 (const unsigned char *) aes_128_subkeys,
                                 MBEDTLS_CIPHER_AES_128_ECB,
                                 MBEDTLS_AES_BLOCK_SIZE,
                                 NB_CMAC_TESTS_PER_KEY)) != 0) {
        return ret;
    }

    if ((ret = cmac_test_wth_cipher(verbose,
                                    "AES 128",
                                    aes_128_key,
                                    128,
                                    test_message,
                                    aes_message_lengths,
                                    (const unsigned char *) aes_128_expected_result,
                                    MBEDTLS_CIPHER_AES_128_ECB,
                                    MBEDTLS_AES_BLOCK_SIZE,
                                    NB_CMAC_TESTS_PER_KEY)) != 0) {
        return ret;
    }

    /* AES-192 */
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    if ((ret = cmac_test_subkeys(verbose,
                                 "AES 192",
                                 aes_192_key,
                                 192,
                                 (const unsigned char *) aes_192_subkeys,
                                 MBEDTLS_CIPHER_AES_192_ECB,
                                 MBEDTLS_AES_BLOCK_SIZE,
                                 NB_CMAC_TESTS_PER_KEY)) != 0) {
        return ret;
    }

    if ((ret = cmac_test_wth_cipher(verbose,
                                    "AES 192",
                                    aes_192_key,
                                    192,
                                    test_message,
                                    aes_message_lengths,
                                    (const unsigned char *) aes_192_expected_result,
                                    MBEDTLS_CIPHER_AES_192_ECB,
                                    MBEDTLS_AES_BLOCK_SIZE,
                                    NB_CMAC_TESTS_PER_KEY)) != 0) {
        return ret;
    }
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */

    /* AES-256 */
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    if ((ret = cmac_test_subkeys(verbose,
                                 "AES 256",
                                 aes_256_key,
                                 256,
                                 (const unsigned char *) aes_256_subkeys,
                                 MBEDTLS_CIPHER_AES_256_ECB,
                                 MBEDTLS_AES_BLOCK_SIZE,
                                 NB_CMAC_TESTS_PER_KEY)) != 0) {
        return ret;
    }

    if ((ret = cmac_test_wth_cipher(verbose,
                                    "AES 256",
                                    aes_256_key,
                                    256,
                                    test_message,
                                    aes_message_lengths,
                                    (const unsigned char *) aes_256_expected_result,
                                    MBEDTLS_CIPHER_AES_256_ECB,
                                    MBEDTLS_AES_BLOCK_SIZE,
                                    NB_CMAC_TESTS_PER_KEY)) != 0) {
        return ret;
    }
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */
#endif /* MBEDTLS_AES_C */


#if defined(MBEDTLS_AES_C)
    if ((ret = test_aes128_cmac_prf(verbose)) != 0) {
        return ret;
    }
#endif /* MBEDTLS_AES_C */

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    return 0;
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_CMAC_C */
