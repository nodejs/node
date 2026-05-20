/*
 *  Implementation of NIST SP 800-38F key wrapping, supporting KW and KWP modes
 *  only
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 * Definition of Key Wrapping:
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-38F.pdf
 * RFC 3394 "Advanced Encryption Standard (AES) Key Wrap Algorithm"
 * RFC 5649 "Advanced Encryption Standard (AES) Key Wrap with Padding Algorithm"
 *
 * Note: RFC 3394 defines different methodology for intermediate operations for
 * the wrapping and unwrapping operation than the definition in NIST SP 800-38F.
 */

#include "common.h"

#if defined(MBEDTLS_NIST_KW_C)

#include "mbedtls/nist_kw.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "mbedtls/constant_time.h"
#include "constant_time_internal.h"

#include <stdint.h>
#include <string.h>

#include "mbedtls/platform.h"

#if !defined(MBEDTLS_NIST_KW_ALT)

#define KW_SEMIBLOCK_LENGTH    8
#define MIN_SEMIBLOCKS_COUNT   3

/*! The 64-bit default integrity check value (ICV) for KW mode. */
static const unsigned char NIST_KW_ICV1[] = { 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6 };
/*! The 32-bit default integrity check value (ICV) for KWP mode. */
static const  unsigned char NIST_KW_ICV2[] = { 0xA6, 0x59, 0x59, 0xA6 };

/*
 * Initialize context
 */
void mbedtls_nist_kw_init(mbedtls_nist_kw_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_nist_kw_context));
}

int mbedtls_nist_kw_setkey(mbedtls_nist_kw_context *ctx,
                           mbedtls_cipher_id_t cipher,
                           const unsigned char *key,
                           unsigned int keybits,
                           const int is_wrap)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const mbedtls_cipher_info_t *cipher_info;

    cipher_info = mbedtls_cipher_info_from_values(cipher,
                                                  keybits,
                                                  MBEDTLS_MODE_ECB);
    if (cipher_info == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    if (mbedtls_cipher_info_get_block_size(cipher_info) != 16) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    /*
     * SP 800-38F currently defines AES cipher as the only block cipher allowed:
     * "For KW and KWP, the underlying block cipher shall be approved, and the
     *  block size shall be 128 bits. Currently, the AES block cipher, with key
     *  lengths of 128, 192, or 256 bits, is the only block cipher that fits
     *  this profile."
     *  Currently we don't support other 128 bit block ciphers for key wrapping,
     *  such as Camellia and Aria.
     */
    if (cipher != MBEDTLS_CIPHER_ID_AES) {
        return MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE;
    }

    mbedtls_cipher_free(&ctx->cipher_ctx);

    if ((ret = mbedtls_cipher_setup(&ctx->cipher_ctx, cipher_info)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_cipher_setkey(&ctx->cipher_ctx, key, keybits,
                                     is_wrap ? MBEDTLS_ENCRYPT :
                                     MBEDTLS_DECRYPT)
         ) != 0) {
        return ret;
    }

    return 0;
}

/*
 * Free context
 */
void mbedtls_nist_kw_free(mbedtls_nist_kw_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_cipher_free(&ctx->cipher_ctx);
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_nist_kw_context));
}

/*
 * Helper function for Xoring the uint64_t "t" with the encrypted A.
 * Defined in NIST SP 800-38F section 6.1
 */
static void calc_a_xor_t(unsigned char A[KW_SEMIBLOCK_LENGTH], uint64_t t)
{
    size_t i = 0;
    for (i = 0; i < sizeof(t); i++) {
        A[i] ^= (t >> ((sizeof(t) - 1 - i) * 8)) & 0xff;
    }
}

/*
 * KW-AE as defined in SP 800-38F section 6.2
 * KWP-AE as defined in SP 800-38F section 6.3
 */
int mbedtls_nist_kw_wrap(mbedtls_nist_kw_context *ctx,
                         mbedtls_nist_kw_mode_t mode,
                         const unsigned char *input, size_t in_len,
                         unsigned char *output, size_t *out_len, size_t out_size)
{
    int ret = 0;
    size_t semiblocks = 0;
    size_t s;
    size_t olen, padlen = 0;
    uint64_t t = 0;
    unsigned char outbuff[KW_SEMIBLOCK_LENGTH * 2];
    unsigned char inbuff[KW_SEMIBLOCK_LENGTH * 2];

    *out_len = 0;
    /*
     * Generate the String to work on
     */
    if (mode == MBEDTLS_KW_MODE_KW) {
        if (out_size < in_len + KW_SEMIBLOCK_LENGTH) {
            return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
        }

        /*
         * According to SP 800-38F Table 1, the plaintext length for KW
         * must be between 2 to 2^54-1 semiblocks inclusive.
         */
        if (in_len < 16 ||
#if SIZE_MAX > 0x1FFFFFFFFFFFFF8
            in_len > 0x1FFFFFFFFFFFFF8 ||
#endif
            in_len % KW_SEMIBLOCK_LENGTH != 0) {
            return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
        }

        memcpy(output, NIST_KW_ICV1, KW_SEMIBLOCK_LENGTH);
        memmove(output + KW_SEMIBLOCK_LENGTH, input, in_len);
    } else {
        if (in_len % 8 != 0) {
            padlen = (8 - (in_len % 8));
        }

        if (out_size < in_len + KW_SEMIBLOCK_LENGTH + padlen) {
            return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
        }

        /*
         * According to SP 800-38F Table 1, the plaintext length for KWP
         * must be between 1 and 2^32-1 octets inclusive.
         */
        if (in_len < 1
#if SIZE_MAX > 0xFFFFFFFF
            || in_len > 0xFFFFFFFF
#endif
            ) {
            return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
        }

        memcpy(output, NIST_KW_ICV2, KW_SEMIBLOCK_LENGTH / 2);
        MBEDTLS_PUT_UINT32_BE((in_len & 0xffffffff), output,
                              KW_SEMIBLOCK_LENGTH / 2);

        memcpy(output + KW_SEMIBLOCK_LENGTH, input, in_len);
        memset(output + KW_SEMIBLOCK_LENGTH + in_len, 0, padlen);
    }
    semiblocks = ((in_len + padlen) / KW_SEMIBLOCK_LENGTH) + 1;

    s = 6 * (semiblocks - 1);

    if (mode == MBEDTLS_KW_MODE_KWP
        && in_len <= KW_SEMIBLOCK_LENGTH) {
        memcpy(inbuff, output, 16);
        ret = mbedtls_cipher_update(&ctx->cipher_ctx,
                                    inbuff, 16, output, &olen);
        if (ret != 0) {
            goto cleanup;
        }
    } else {
        unsigned char *R2 = output + KW_SEMIBLOCK_LENGTH;
        unsigned char *A = output;

        /*
         * Do the wrapping function W, as defined in RFC 3394 section 2.2.1
         */
        if (semiblocks < MIN_SEMIBLOCKS_COUNT) {
            ret = MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
            goto cleanup;
        }

        /* Calculate intermediate values */
        for (t = 1; t <= s; t++) {
            memcpy(inbuff, A, KW_SEMIBLOCK_LENGTH);
            memcpy(inbuff + KW_SEMIBLOCK_LENGTH, R2, KW_SEMIBLOCK_LENGTH);

            ret = mbedtls_cipher_update(&ctx->cipher_ctx,
                                        inbuff, 16, outbuff, &olen);
            if (ret != 0) {
                goto cleanup;
            }

            memcpy(A, outbuff, KW_SEMIBLOCK_LENGTH);
            calc_a_xor_t(A, t);

            memcpy(R2, outbuff + KW_SEMIBLOCK_LENGTH, KW_SEMIBLOCK_LENGTH);
            R2 += KW_SEMIBLOCK_LENGTH;
            if (R2 >= output + (semiblocks * KW_SEMIBLOCK_LENGTH)) {
                R2 = output + KW_SEMIBLOCK_LENGTH;
            }
        }
    }

    *out_len = semiblocks * KW_SEMIBLOCK_LENGTH;

cleanup:

    if (ret != 0) {
        memset(output, 0, semiblocks * KW_SEMIBLOCK_LENGTH);
    }
    mbedtls_platform_zeroize(inbuff, KW_SEMIBLOCK_LENGTH * 2);
    mbedtls_platform_zeroize(outbuff, KW_SEMIBLOCK_LENGTH * 2);

    return ret;
}

/*
 * W-1 function as defined in RFC 3394 section 2.2.2
 * This function assumes the following:
 * 1. Output buffer is at least of size ( semiblocks - 1 ) * KW_SEMIBLOCK_LENGTH.
 * 2. The input buffer is of size semiblocks * KW_SEMIBLOCK_LENGTH.
 * 3. Minimal number of semiblocks is 3.
 * 4. A is a buffer to hold the first semiblock of the input buffer.
 */
static int unwrap(mbedtls_nist_kw_context *ctx,
                  const unsigned char *input, size_t semiblocks,
                  unsigned char A[KW_SEMIBLOCK_LENGTH],
                  unsigned char *output, size_t *out_len)
{
    int ret = 0;
    const size_t s = 6 * (semiblocks - 1);
    size_t olen;
    uint64_t t = 0;
    unsigned char outbuff[KW_SEMIBLOCK_LENGTH * 2];
    unsigned char inbuff[KW_SEMIBLOCK_LENGTH * 2];
    unsigned char *R = NULL;
    *out_len = 0;

    if (semiblocks < MIN_SEMIBLOCKS_COUNT) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    memcpy(A, input, KW_SEMIBLOCK_LENGTH);
    memmove(output, input + KW_SEMIBLOCK_LENGTH, (semiblocks - 1) * KW_SEMIBLOCK_LENGTH);
    R = output + (semiblocks - 2) * KW_SEMIBLOCK_LENGTH;

    /* Calculate intermediate values */
    for (t = s; t >= 1; t--) {
        calc_a_xor_t(A, t);

        memcpy(inbuff, A, KW_SEMIBLOCK_LENGTH);
        memcpy(inbuff + KW_SEMIBLOCK_LENGTH, R, KW_SEMIBLOCK_LENGTH);

        ret = mbedtls_cipher_update(&ctx->cipher_ctx,
                                    inbuff, 16, outbuff, &olen);
        if (ret != 0) {
            goto cleanup;
        }

        memcpy(A, outbuff, KW_SEMIBLOCK_LENGTH);

        /* Set R as LSB64 of outbuff */
        memcpy(R, outbuff + KW_SEMIBLOCK_LENGTH, KW_SEMIBLOCK_LENGTH);

        if (R == output) {
            R = output + (semiblocks - 2) * KW_SEMIBLOCK_LENGTH;
        } else {
            R -= KW_SEMIBLOCK_LENGTH;
        }
    }

    *out_len = (semiblocks - 1) * KW_SEMIBLOCK_LENGTH;

cleanup:
    if (ret != 0) {
        memset(output, 0, (semiblocks - 1) * KW_SEMIBLOCK_LENGTH);
    }
    mbedtls_platform_zeroize(inbuff, sizeof(inbuff));
    mbedtls_platform_zeroize(outbuff, sizeof(outbuff));

    return ret;
}

/*
 * KW-AD as defined in SP 800-38F section 6.2
 * KWP-AD as defined in SP 800-38F section 6.3
 */
int mbedtls_nist_kw_unwrap(mbedtls_nist_kw_context *ctx,
                           mbedtls_nist_kw_mode_t mode,
                           const unsigned char *input, size_t in_len,
                           unsigned char *output, size_t *out_len, size_t out_size)
{
    int ret = 0;
    size_t olen;
    unsigned char A[KW_SEMIBLOCK_LENGTH];
    int diff;

    *out_len = 0;
    if (out_size < in_len - KW_SEMIBLOCK_LENGTH) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

    if (mode == MBEDTLS_KW_MODE_KW) {
        /*
         * According to SP 800-38F Table 1, the ciphertext length for KW
         * must be between 3 to 2^54 semiblocks inclusive.
         */
        if (in_len < 24 ||
#if SIZE_MAX > 0x200000000000000
            in_len > 0x200000000000000 ||
#endif
            in_len % KW_SEMIBLOCK_LENGTH != 0) {
            return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
        }

        ret = unwrap(ctx, input, in_len / KW_SEMIBLOCK_LENGTH,
                     A, output, out_len);
        if (ret != 0) {
            goto cleanup;
        }

        /* Check ICV in "constant-time" */
        diff = mbedtls_ct_memcmp(NIST_KW_ICV1, A, KW_SEMIBLOCK_LENGTH);

        if (diff != 0) {
            ret = MBEDTLS_ERR_CIPHER_AUTH_FAILED;
            goto cleanup;
        }

    } else if (mode == MBEDTLS_KW_MODE_KWP) {
        size_t padlen = 0;
        uint32_t Plen;
        /*
         * According to SP 800-38F Table 1, the ciphertext length for KWP
         * must be between 2 to 2^29 semiblocks inclusive.
         */
        if (in_len < KW_SEMIBLOCK_LENGTH * 2 ||
#if SIZE_MAX > 0x100000000
            in_len > 0x100000000 ||
#endif
            in_len % KW_SEMIBLOCK_LENGTH != 0) {
            return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
        }

        if (in_len == KW_SEMIBLOCK_LENGTH * 2) {
            unsigned char outbuff[KW_SEMIBLOCK_LENGTH * 2];
            ret = mbedtls_cipher_update(&ctx->cipher_ctx,
                                        input, 16, outbuff, &olen);
            if (ret != 0) {
                goto cleanup;
            }

            memcpy(A, outbuff, KW_SEMIBLOCK_LENGTH);
            memcpy(output, outbuff + KW_SEMIBLOCK_LENGTH, KW_SEMIBLOCK_LENGTH);
            mbedtls_platform_zeroize(outbuff, sizeof(outbuff));
            *out_len = KW_SEMIBLOCK_LENGTH;
        } else {
            /* in_len >=  KW_SEMIBLOCK_LENGTH * 3 */
            ret = unwrap(ctx, input, in_len / KW_SEMIBLOCK_LENGTH,
                         A, output, out_len);
            if (ret != 0) {
                goto cleanup;
            }
        }

        /* Check ICV in "constant-time" */
        diff = mbedtls_ct_memcmp(NIST_KW_ICV2, A, KW_SEMIBLOCK_LENGTH / 2);

        if (diff != 0) {
            ret = MBEDTLS_ERR_CIPHER_AUTH_FAILED;
        }

        Plen = MBEDTLS_GET_UINT32_BE(A, KW_SEMIBLOCK_LENGTH / 2);

        /*
         * Plen is the length of the plaintext, when the input is valid.
         * If Plen is larger than the plaintext and padding, padlen will be
         * larger than 8, because of the type wrap around.
         */
        padlen = in_len - KW_SEMIBLOCK_LENGTH - Plen;
        ret = mbedtls_ct_error_if(mbedtls_ct_uint_gt(padlen, 7),
                                  MBEDTLS_ERR_CIPHER_AUTH_FAILED, ret);
        padlen &= 7;

        /* Check padding in "constant-time" */
        const uint8_t zero[KW_SEMIBLOCK_LENGTH] = { 0 };
        diff = mbedtls_ct_memcmp_partial(
            &output[*out_len - KW_SEMIBLOCK_LENGTH], zero,
            KW_SEMIBLOCK_LENGTH, KW_SEMIBLOCK_LENGTH - padlen, 0);

        if (diff != 0) {
            ret = MBEDTLS_ERR_CIPHER_AUTH_FAILED;
        }

        if (ret != 0) {
            goto cleanup;
        }
        memset(output + Plen, 0, padlen);
        *out_len = Plen;
    } else {
        ret = MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE;
        goto cleanup;
    }

cleanup:
    if (ret != 0) {
        memset(output, 0, *out_len);
        *out_len = 0;
    }

    mbedtls_platform_zeroize(&diff, sizeof(diff));
    mbedtls_platform_zeroize(A, sizeof(A));

    return ret;
}

#endif /* !MBEDTLS_NIST_KW_ALT */

#if defined(MBEDTLS_SELF_TEST) && defined(MBEDTLS_AES_C)

/*
 * Test vectors taken from NIST
 * https://csrc.nist.gov/Projects/Cryptographic-Algorithm-Validation-Program/CAVP-TESTING-BLOCK-CIPHER-MODES#KW
 */
static const unsigned int key_len[] = {
    16,
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    24,
    32
#endif
};

static const unsigned char kw_key[][32] = {
    { 0x75, 0x75, 0xda, 0x3a, 0x93, 0x60, 0x7c, 0xc2,
      0xbf, 0xd8, 0xce, 0xc7, 0xaa, 0xdf, 0xd9, 0xa6 },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { 0x2d, 0x85, 0x26, 0x08, 0x1d, 0x02, 0xfb, 0x5b,
      0x85, 0xf6, 0x9a, 0xc2, 0x86, 0xec, 0xd5, 0x7d,
      0x40, 0xdf, 0x5d, 0xf3, 0x49, 0x47, 0x44, 0xd3 },
    { 0x11, 0x2a, 0xd4, 0x1b, 0x48, 0x56, 0xc7, 0x25,
      0x4a, 0x98, 0x48, 0xd3, 0x0f, 0xdd, 0x78, 0x33,
      0x5b, 0x03, 0x9a, 0x48, 0xa8, 0x96, 0x2c, 0x4d,
      0x1c, 0xb7, 0x8e, 0xab, 0xd5, 0xda, 0xd7, 0x88 }
#endif
};

static const unsigned char kw_msg[][40] = {
    { 0x42, 0x13, 0x6d, 0x3c, 0x38, 0x4a, 0x3e, 0xea,
      0xc9, 0x5a, 0x06, 0x6f, 0xd2, 0x8f, 0xed, 0x3f },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { 0x95, 0xc1, 0x1b, 0xf5, 0x35, 0x3a, 0xfe, 0xdb,
      0x98, 0xfd, 0xd6, 0xc8, 0xca, 0x6f, 0xdb, 0x6d,
      0xa5, 0x4b, 0x74, 0xb4, 0x99, 0x0f, 0xdc, 0x45,
      0xc0, 0x9d, 0x15, 0x8f, 0x51, 0xce, 0x62, 0x9d,
      0xe2, 0xaf, 0x26, 0xe3, 0x25, 0x0e, 0x6b, 0x4c },
    { 0x1b, 0x20, 0xbf, 0x19, 0x90, 0xb0, 0x65, 0xd7,
      0x98, 0xe1, 0xb3, 0x22, 0x64, 0xad, 0x50, 0xa8,
      0x74, 0x74, 0x92, 0xba, 0x09, 0xa0, 0x4d, 0xd1 }
#endif
};

static const size_t kw_msg_len[] = {
    16,
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    40,
    24
#endif
};
static const size_t kw_out_len[] = {
    24,
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    48,
    32
#endif
};
static const unsigned char kw_res[][48] = {
    { 0x03, 0x1f, 0x6b, 0xd7, 0xe6, 0x1e, 0x64, 0x3d,
      0xf6, 0x85, 0x94, 0x81, 0x6f, 0x64, 0xca, 0xa3,
      0xf5, 0x6f, 0xab, 0xea, 0x25, 0x48, 0xf5, 0xfb },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { 0x44, 0x3c, 0x6f, 0x15, 0x09, 0x83, 0x71, 0x91,
      0x3e, 0x5c, 0x81, 0x4c, 0xa1, 0xa0, 0x42, 0xec,
      0x68, 0x2f, 0x7b, 0x13, 0x6d, 0x24, 0x3a, 0x4d,
      0x6c, 0x42, 0x6f, 0xc6, 0x97, 0x15, 0x63, 0xe8,
      0xa1, 0x4a, 0x55, 0x8e, 0x09, 0x64, 0x16, 0x19,
      0xbf, 0x03, 0xfc, 0xaf, 0x90, 0xb1, 0xfc, 0x2d },
    { 0xba, 0x8a, 0x25, 0x9a, 0x47, 0x1b, 0x78, 0x7d,
      0xd5, 0xd5, 0x40, 0xec, 0x25, 0xd4, 0x3d, 0x87,
      0x20, 0x0f, 0xda, 0xdc, 0x6d, 0x1f, 0x05, 0xd9,
      0x16, 0x58, 0x4f, 0xa9, 0xf6, 0xcb, 0xf5, 0x12 }
#endif
};

static const unsigned char kwp_key[][32] = {
    { 0x78, 0x65, 0xe2, 0x0f, 0x3c, 0x21, 0x65, 0x9a,
      0xb4, 0x69, 0x0b, 0x62, 0x9c, 0xdf, 0x3c, 0xc4 },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { 0xf5, 0xf8, 0x96, 0xa3, 0xbd, 0x2f, 0x4a, 0x98,
      0x23, 0xef, 0x16, 0x2b, 0x00, 0xb8, 0x05, 0xd7,
      0xde, 0x1e, 0xa4, 0x66, 0x26, 0x96, 0xa2, 0x58 },
    { 0x95, 0xda, 0x27, 0x00, 0xca, 0x6f, 0xd9, 0xa5,
      0x25, 0x54, 0xee, 0x2a, 0x8d, 0xf1, 0x38, 0x6f,
      0x5b, 0x94, 0xa1, 0xa6, 0x0e, 0xd8, 0xa4, 0xae,
      0xf6, 0x0a, 0x8d, 0x61, 0xab, 0x5f, 0x22, 0x5a }
#endif
};

static const unsigned char kwp_msg[][31] = {
    { 0xbd, 0x68, 0x43, 0xd4, 0x20, 0x37, 0x8d, 0xc8,
      0x96 },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { 0x6c, 0xcd, 0xd5, 0x85, 0x18, 0x40, 0x97, 0xeb,
      0xd5, 0xc3, 0xaf, 0x3e, 0x47, 0xd0, 0x2c, 0x19,
      0x14, 0x7b, 0x4d, 0x99, 0x5f, 0x96, 0x43, 0x66,
      0x91, 0x56, 0x75, 0x8c, 0x13, 0x16, 0x8f },
    { 0xd1 }
#endif
};
static const size_t kwp_msg_len[] = {
    9,
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    31,
    1
#endif
};

static const unsigned char kwp_res[][48] = {
    { 0x41, 0xec, 0xa9, 0x56, 0xd4, 0xaa, 0x04, 0x7e,
      0xb5, 0xcf, 0x4e, 0xfe, 0x65, 0x96, 0x61, 0xe7,
      0x4d, 0xb6, 0xf8, 0xc5, 0x64, 0xe2, 0x35, 0x00 },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { 0x4e, 0x9b, 0xc2, 0xbc, 0xbc, 0x6c, 0x1e, 0x13,
      0xd3, 0x35, 0xbc, 0xc0, 0xf7, 0x73, 0x6a, 0x88,
      0xfa, 0x87, 0x53, 0x66, 0x15, 0xbb, 0x8e, 0x63,
      0x8b, 0xcc, 0x81, 0x66, 0x84, 0x68, 0x17, 0x90,
      0x67, 0xcf, 0xa9, 0x8a, 0x9d, 0x0e, 0x33, 0x26 },
    { 0x06, 0xba, 0x7a, 0xe6, 0xf3, 0x24, 0x8c, 0xfd,
      0xcf, 0x26, 0x75, 0x07, 0xfa, 0x00, 0x1b, 0xc4  }
#endif
};
static const size_t kwp_out_len[] = {
    24,
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    40,
    16
#endif
};

int mbedtls_nist_kw_self_test(int verbose)
{
    mbedtls_nist_kw_context ctx;
    unsigned char out[48];
    size_t olen;
    int i;
    int ret = 0;
    mbedtls_nist_kw_init(&ctx);

    /*
     * KW mode
     */
    {
        static const int num_tests = sizeof(kw_key) / sizeof(*kw_key);

        for (i = 0; i < num_tests; i++) {
            if (verbose != 0) {
                mbedtls_printf("  KW-AES-%u ", (unsigned int) key_len[i] * 8);
            }

            ret = mbedtls_nist_kw_setkey(&ctx, MBEDTLS_CIPHER_ID_AES,
                                         kw_key[i], key_len[i] * 8, 1);
            if (ret != 0) {
                if (verbose != 0) {
                    mbedtls_printf("  KW: setup failed ");
                }

                goto end;
            }

            ret = mbedtls_nist_kw_wrap(&ctx, MBEDTLS_KW_MODE_KW, kw_msg[i],
                                       kw_msg_len[i], out, &olen, sizeof(out));
            if (ret != 0 || kw_out_len[i] != olen ||
                memcmp(out, kw_res[i], kw_out_len[i]) != 0) {
                if (verbose != 0) {
                    mbedtls_printf("failed. ");
                }

                ret = 1;
                goto end;
            }

            if ((ret = mbedtls_nist_kw_setkey(&ctx, MBEDTLS_CIPHER_ID_AES,
                                              kw_key[i], key_len[i] * 8, 0))
                != 0) {
                if (verbose != 0) {
                    mbedtls_printf("  KW: setup failed ");
                }

                goto end;
            }

            ret = mbedtls_nist_kw_unwrap(&ctx, MBEDTLS_KW_MODE_KW,
                                         out, olen, out, &olen, sizeof(out));

            if (ret != 0 || olen != kw_msg_len[i] ||
                memcmp(out, kw_msg[i], kw_msg_len[i]) != 0) {
                if (verbose != 0) {
                    mbedtls_printf("failed\n");
                }

                ret = 1;
                goto end;
            }

            if (verbose != 0) {
                mbedtls_printf(" passed\n");
            }
        }
    }

    /*
     * KWP mode
     */
    {
        static const int num_tests = sizeof(kwp_key) / sizeof(*kwp_key);

        for (i = 0; i < num_tests; i++) {
            olen = sizeof(out);
            if (verbose != 0) {
                mbedtls_printf("  KWP-AES-%u ", (unsigned int) key_len[i] * 8);
            }

            ret = mbedtls_nist_kw_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, kwp_key[i],
                                         key_len[i] * 8, 1);
            if (ret  != 0) {
                if (verbose != 0) {
                    mbedtls_printf("  KWP: setup failed ");
                }

                goto end;
            }
            ret = mbedtls_nist_kw_wrap(&ctx, MBEDTLS_KW_MODE_KWP, kwp_msg[i],
                                       kwp_msg_len[i], out, &olen, sizeof(out));

            if (ret != 0 || kwp_out_len[i] != olen ||
                memcmp(out, kwp_res[i], kwp_out_len[i]) != 0) {
                if (verbose != 0) {
                    mbedtls_printf("failed. ");
                }

                ret = 1;
                goto end;
            }

            if ((ret = mbedtls_nist_kw_setkey(&ctx, MBEDTLS_CIPHER_ID_AES,
                                              kwp_key[i], key_len[i] * 8, 0))
                != 0) {
                if (verbose != 0) {
                    mbedtls_printf("  KWP: setup failed ");
                }

                goto end;
            }

            ret = mbedtls_nist_kw_unwrap(&ctx, MBEDTLS_KW_MODE_KWP, out,
                                         olen, out, &olen, sizeof(out));

            if (ret != 0 || olen != kwp_msg_len[i] ||
                memcmp(out, kwp_msg[i], kwp_msg_len[i]) != 0) {
                if (verbose != 0) {
                    mbedtls_printf("failed. ");
                }

                ret = 1;
                goto end;
            }

            if (verbose != 0) {
                mbedtls_printf(" passed\n");
            }
        }
    }
end:
    mbedtls_nist_kw_free(&ctx);

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    return ret;
}

#endif /* MBEDTLS_SELF_TEST && MBEDTLS_AES_C */

#endif /* MBEDTLS_NIST_KW_C */
