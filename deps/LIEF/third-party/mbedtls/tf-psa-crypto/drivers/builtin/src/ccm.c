/*
 *  NIST SP800-38C compliant CCM implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * Definition of CCM:
 * http://csrc.nist.gov/publications/nistpubs/800-38C/SP800-38C_updated-July20_2007.pdf
 * RFC 3610 "Counter with CBC-MAC (CCM)"
 *
 * Related:
 * RFC 5116 "An Interface and Algorithms for Authenticated Encryption"
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_CCM_C)

#include "mbedtls/private/ccm.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/private/error_common.h"
#include "mbedtls/constant_time.h"

#if defined(MBEDTLS_BLOCK_CIPHER_C)
#include "block_cipher_internal.h"
#endif

#include <string.h>

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#if defined(MBEDTLS_SELF_TEST) && defined(MBEDTLS_AES_C)
#include <stdio.h>
#define mbedtls_printf printf
#endif /* MBEDTLS_SELF_TEST && MBEDTLS_AES_C */
#endif /* MBEDTLS_PLATFORM_C */

/*
 * Initialize context
 */
void mbedtls_ccm_init(mbedtls_ccm_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_ccm_context));
}

int mbedtls_ccm_setkey(mbedtls_ccm_context *ctx,
                       mbedtls_cipher_id_t cipher,
                       const unsigned char *key,
                       unsigned int keybits)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

#if defined(MBEDTLS_BLOCK_CIPHER_C)
    mbedtls_block_cipher_free(&ctx->block_cipher_ctx);

    if ((ret = mbedtls_block_cipher_setup(&ctx->block_cipher_ctx, cipher)) != 0) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    if ((ret = mbedtls_block_cipher_setkey(&ctx->block_cipher_ctx, key, keybits)) != 0) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
#else
    const mbedtls_cipher_info_t *cipher_info;

    cipher_info = mbedtls_cipher_info_from_values(cipher, keybits,
                                                  MBEDTLS_MODE_ECB);
    if (cipher_info == NULL) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    if (mbedtls_cipher_info_get_block_size(cipher_info) != 16) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    mbedtls_cipher_free(&ctx->cipher_ctx);

    if ((ret = mbedtls_cipher_setup(&ctx->cipher_ctx, cipher_info)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_cipher_setkey(&ctx->cipher_ctx, key, keybits,
                                     MBEDTLS_ENCRYPT)) != 0) {
        return ret;
    }
#endif

    return ret;
}

/*
 * Free context
 */
void mbedtls_ccm_free(mbedtls_ccm_context *ctx)
{
    if (ctx == NULL) {
        return;
    }
#if defined(MBEDTLS_BLOCK_CIPHER_C)
    mbedtls_block_cipher_free(&ctx->block_cipher_ctx);
#else
    mbedtls_cipher_free(&ctx->cipher_ctx);
#endif
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_ccm_context));
}

#define CCM_STATE__CLEAR                0
#define CCM_STATE__STARTED              (1 << 0)
#define CCM_STATE__LENGTHS_SET          (1 << 1)
#define CCM_STATE__AUTH_DATA_STARTED    (1 << 2)
#define CCM_STATE__AUTH_DATA_FINISHED   (1 << 3)
#define CCM_STATE__ERROR                (1 << 4)

/*
 * Encrypt or decrypt a partial block with CTR
 */
static int mbedtls_ccm_crypt(mbedtls_ccm_context *ctx,
                             size_t offset, size_t use_len,
                             const unsigned char *input,
                             unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char tmp_buf[16] = { 0 };

#if defined(MBEDTLS_BLOCK_CIPHER_C)
    ret = mbedtls_block_cipher_encrypt(&ctx->block_cipher_ctx, ctx->ctr, tmp_buf);
#else
    size_t olen = 0;
    ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctx->ctr, 16, tmp_buf, &olen);
#endif
    if (ret != 0) {
        ctx->state |= CCM_STATE__ERROR;
        mbedtls_platform_zeroize(tmp_buf, sizeof(tmp_buf));
        return ret;
    }

    mbedtls_xor(output, input, tmp_buf + offset, use_len);

    mbedtls_platform_zeroize(tmp_buf, sizeof(tmp_buf));
    return ret;
}

static void mbedtls_ccm_clear_state(mbedtls_ccm_context *ctx)
{
    ctx->state = CCM_STATE__CLEAR;
    memset(ctx->y, 0, 16);
    memset(ctx->ctr, 0, 16);
}

static int ccm_calculate_first_block_if_ready(mbedtls_ccm_context *ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char i;
    size_t len_left;
#if !defined(MBEDTLS_BLOCK_CIPHER_C)
    size_t olen;
#endif

    /* length calculation can be done only after both
     * mbedtls_ccm_starts() and mbedtls_ccm_set_lengths() have been executed
     */
    if (!(ctx->state & CCM_STATE__STARTED) || !(ctx->state & CCM_STATE__LENGTHS_SET)) {
        return 0;
    }

    /* CCM expects non-empty tag.
     * CCM* allows empty tag. For CCM* without tag, the tag calculation is skipped.
     */
    if (ctx->tag_len == 0) {
        if (ctx->mode == MBEDTLS_CCM_STAR_ENCRYPT || ctx->mode == MBEDTLS_CCM_STAR_DECRYPT) {
            ctx->plaintext_len = 0;
            return 0;
        } else {
            return MBEDTLS_ERR_CCM_BAD_INPUT;
        }
    }

    /*
     * First block:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)  - set by: mbedtls_ccm_starts()
     * iv_len+1 .. 15       length
     *
     * With flags as (bits):
     * 7        0
     * 6        add present?
     * 5 .. 3   (t - 2) / 2
     * 2 .. 0   q - 1
     */
    ctx->y[0] |= (ctx->add_len > 0) << 6;
    ctx->y[0] |= ((ctx->tag_len - 2) / 2) << 3;
    ctx->y[0] |= ctx->q - 1;

    for (i = 0, len_left = ctx->plaintext_len; i < ctx->q; i++, len_left >>= 8) {
        ctx->y[15-i] = MBEDTLS_BYTE_0(len_left);
    }

    if (len_left > 0) {
        ctx->state |= CCM_STATE__ERROR;
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    /* Start CBC-MAC with first block*/
#if defined(MBEDTLS_BLOCK_CIPHER_C)
    ret = mbedtls_block_cipher_encrypt(&ctx->block_cipher_ctx, ctx->y, ctx->y);
#else
    ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctx->y, 16, ctx->y, &olen);
#endif
    if (ret != 0) {
        ctx->state |= CCM_STATE__ERROR;
        return ret;
    }

    return 0;
}

int mbedtls_ccm_starts(mbedtls_ccm_context *ctx,
                       int mode,
                       const unsigned char *iv,
                       size_t iv_len)
{
    /* Also implies q is within bounds */
    if (iv_len < 7 || iv_len > 13) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    ctx->mode = mode;
    ctx->q = 16 - 1 - (unsigned char) iv_len;

    /*
     * Prepare counter block for encryption:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       counter (initially 1)
     *
     * With flags as (bits):
     * 7 .. 3   0
     * 2 .. 0   q - 1
     */
    memset(ctx->ctr, 0, 16);
    ctx->ctr[0] = ctx->q - 1;
    memcpy(ctx->ctr + 1, iv, iv_len);
    memset(ctx->ctr + 1 + iv_len, 0, ctx->q);
    ctx->ctr[15] = 1;

    /*
     * See ccm_calculate_first_block_if_ready() for block layout description
     */
    memcpy(ctx->y + 1, iv, iv_len);

    ctx->state |= CCM_STATE__STARTED;
    return ccm_calculate_first_block_if_ready(ctx);
}

int mbedtls_ccm_set_lengths(mbedtls_ccm_context *ctx,
                            size_t total_ad_len,
                            size_t plaintext_len,
                            size_t tag_len)
{
    /*
     * Check length requirements: SP800-38C A.1
     * Additional requirement: a < 2^16 - 2^8 to simplify the code.
     * 'length' checked later (when writing it to the first block)
     *
     * Also, loosen the requirements to enable support for CCM* (IEEE 802.15.4).
     */
    if (tag_len == 2 || tag_len > 16 || tag_len % 2 != 0) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    if (total_ad_len >= 0xFF00) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    ctx->plaintext_len = plaintext_len;
    ctx->add_len = total_ad_len;
    ctx->tag_len = tag_len;
    ctx->processed = 0;

    ctx->state |= CCM_STATE__LENGTHS_SET;
    return ccm_calculate_first_block_if_ready(ctx);
}

int mbedtls_ccm_update_ad(mbedtls_ccm_context *ctx,
                          const unsigned char *add,
                          size_t add_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t use_len, offset;
#if !defined(MBEDTLS_BLOCK_CIPHER_C)
    size_t olen;
#endif

    if (ctx->state & CCM_STATE__ERROR) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    if (add_len > 0) {
        if (ctx->state & CCM_STATE__AUTH_DATA_FINISHED) {
            return MBEDTLS_ERR_CCM_BAD_INPUT;
        }

        if (!(ctx->state & CCM_STATE__AUTH_DATA_STARTED)) {
            if (add_len > ctx->add_len) {
                return MBEDTLS_ERR_CCM_BAD_INPUT;
            }

            ctx->y[0] ^= (unsigned char) ((ctx->add_len >> 8) & 0xFF);
            ctx->y[1] ^= (unsigned char) ((ctx->add_len) & 0xFF);

            ctx->state |= CCM_STATE__AUTH_DATA_STARTED;
        } else if (ctx->processed + add_len > ctx->add_len) {
            return MBEDTLS_ERR_CCM_BAD_INPUT;
        }

        while (add_len > 0) {
            offset = (ctx->processed + 2) % 16; /* account for y[0] and y[1]
                                                 * holding total auth data length */
            use_len = 16 - offset;

            if (use_len > add_len) {
                use_len = add_len;
            }

            mbedtls_xor(ctx->y + offset, ctx->y + offset, add, use_len);

            ctx->processed += use_len;
            add_len -= use_len;
            add += use_len;

            if (use_len + offset == 16 || ctx->processed == ctx->add_len) {
#if defined(MBEDTLS_BLOCK_CIPHER_C)
                ret = mbedtls_block_cipher_encrypt(&ctx->block_cipher_ctx, ctx->y, ctx->y);
#else
                ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctx->y, 16, ctx->y, &olen);
#endif
                if (ret != 0) {
                    ctx->state |= CCM_STATE__ERROR;
                    return ret;
                }
            }
        }

        if (ctx->processed == ctx->add_len) {
            ctx->state |= CCM_STATE__AUTH_DATA_FINISHED;
            ctx->processed = 0; // prepare for mbedtls_ccm_update()
        }
    }

    return 0;
}

int mbedtls_ccm_update(mbedtls_ccm_context *ctx,
                       const unsigned char *input, size_t input_len,
                       unsigned char *output, size_t output_size,
                       size_t *output_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char i;
    size_t use_len, offset;
#if !defined(MBEDTLS_BLOCK_CIPHER_C)
    size_t olen;
#endif

    unsigned char local_output[16];

    if (ctx->state & CCM_STATE__ERROR) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    /* Check against plaintext length only if performing operation with
     * authentication
     */
    if (ctx->tag_len != 0 && ctx->processed + input_len > ctx->plaintext_len) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    if (output_size < input_len) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }
    *output_len = input_len;

    ret = 0;

    while (input_len > 0) {
        offset = ctx->processed % 16;

        use_len = 16 - offset;

        if (use_len > input_len) {
            use_len = input_len;
        }

        ctx->processed += use_len;

        if (ctx->mode == MBEDTLS_CCM_ENCRYPT || \
            ctx->mode == MBEDTLS_CCM_STAR_ENCRYPT) {
            mbedtls_xor(ctx->y + offset, ctx->y + offset, input, use_len);

            if (use_len + offset == 16 || ctx->processed == ctx->plaintext_len) {
#if defined(MBEDTLS_BLOCK_CIPHER_C)
                ret = mbedtls_block_cipher_encrypt(&ctx->block_cipher_ctx, ctx->y, ctx->y);
#else
                ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctx->y, 16, ctx->y, &olen);
#endif
                if (ret != 0) {
                    ctx->state |= CCM_STATE__ERROR;
                    goto exit;
                }
            }

            ret = mbedtls_ccm_crypt(ctx, offset, use_len, input, output);
            if (ret != 0) {
                goto exit;
            }
        }

        if (ctx->mode == MBEDTLS_CCM_DECRYPT || \
            ctx->mode == MBEDTLS_CCM_STAR_DECRYPT) {
            /* Since output may be in shared memory, we cannot be sure that
             * it will contain what we wrote to it. Therefore, we should avoid using
             * it as input to any operations.
             * Write decrypted data to local_output to avoid using output variable as
             * input in the XOR operation for Y.
             */
            ret = mbedtls_ccm_crypt(ctx, offset, use_len, input, local_output);
            if (ret != 0) {
                goto exit;
            }

            mbedtls_xor(ctx->y + offset, ctx->y + offset, local_output, use_len);

            memcpy(output, local_output, use_len);

            if (use_len + offset == 16 || ctx->processed == ctx->plaintext_len) {
#if defined(MBEDTLS_BLOCK_CIPHER_C)
                ret = mbedtls_block_cipher_encrypt(&ctx->block_cipher_ctx, ctx->y, ctx->y);
#else
                ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctx->y, 16, ctx->y, &olen);
#endif
                if (ret != 0) {
                    ctx->state |= CCM_STATE__ERROR;
                    goto exit;
                }
            }
        }

        if (use_len + offset == 16 || ctx->processed == ctx->plaintext_len) {
            for (i = 0; i < ctx->q; i++) {
                if (++(ctx->ctr)[15-i] != 0) {
                    break;
                }
            }
        }

        input_len -= use_len;
        input += use_len;
        output += use_len;
    }

exit:
    mbedtls_platform_zeroize(local_output, 16);

    return ret;
}

int mbedtls_ccm_finish(mbedtls_ccm_context *ctx,
                       unsigned char *tag, size_t tag_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char i;

    if (ctx->state & CCM_STATE__ERROR) {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }

    if (ctx->add_len > 0 && !(ctx->state & CCM_STATE__AUTH_DATA_FINISHED)) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    if (ctx->plaintext_len > 0 && ctx->processed != ctx->plaintext_len) {
        return MBEDTLS_ERR_CCM_BAD_INPUT;
    }

    /*
     * Authentication: reset counter and crypt/mask internal tag
     */
    for (i = 0; i < ctx->q; i++) {
        ctx->ctr[15-i] = 0;
    }

    ret = mbedtls_ccm_crypt(ctx, 0, 16, ctx->y, ctx->y);
    if (ret != 0) {
        return ret;
    }
    if (tag != NULL) {
        memcpy(tag, ctx->y, tag_len);
    }
    mbedtls_ccm_clear_state(ctx);

    return 0;
}

/*
 * Authenticated encryption or decryption
 */
static int ccm_auth_crypt(mbedtls_ccm_context *ctx, int mode, size_t length,
                          const unsigned char *iv, size_t iv_len,
                          const unsigned char *add, size_t add_len,
                          const unsigned char *input, unsigned char *output,
                          unsigned char *tag, size_t tag_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t olen;

    if ((ret = mbedtls_ccm_starts(ctx, mode, iv, iv_len)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_ccm_set_lengths(ctx, add_len, length, tag_len)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_ccm_update_ad(ctx, add, add_len)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_ccm_update(ctx, input, length,
                                  output, length, &olen)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_ccm_finish(ctx, tag, tag_len)) != 0) {
        return ret;
    }

    return 0;
}

/*
 * Authenticated encryption
 */
int mbedtls_ccm_star_encrypt_and_tag(mbedtls_ccm_context *ctx, size_t length,
                                     const unsigned char *iv, size_t iv_len,
                                     const unsigned char *add, size_t add_len,
                                     const unsigned char *input, unsigned char *output,
                                     unsigned char *tag, size_t tag_len)
{
    return ccm_auth_crypt(ctx, MBEDTLS_CCM_STAR_ENCRYPT, length, iv, iv_len,
                          add, add_len, input, output, tag, tag_len);
}

int mbedtls_ccm_encrypt_and_tag(mbedtls_ccm_context *ctx, size_t length,
                                const unsigned char *iv, size_t iv_len,
                                const unsigned char *add, size_t add_len,
                                const unsigned char *input, unsigned char *output,
                                unsigned char *tag, size_t tag_len)
{
    return ccm_auth_crypt(ctx, MBEDTLS_CCM_ENCRYPT, length, iv, iv_len,
                          add, add_len, input, output, tag, tag_len);
}

/*
 * Authenticated decryption
 */
static int mbedtls_ccm_compare_tags(const unsigned char *tag1,
                                    const unsigned char *tag2,
                                    size_t tag_len)
{
    /* Check tag in "constant-time" */
    int diff = mbedtls_ct_memcmp(tag1, tag2, tag_len);

    if (diff != 0) {
        return MBEDTLS_ERR_CCM_AUTH_FAILED;
    }

    return 0;
}

static int ccm_auth_decrypt(mbedtls_ccm_context *ctx, int mode, size_t length,
                            const unsigned char *iv, size_t iv_len,
                            const unsigned char *add, size_t add_len,
                            const unsigned char *input, unsigned char *output,
                            const unsigned char *tag, size_t tag_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char check_tag[16];

    if ((ret = ccm_auth_crypt(ctx, mode, length,
                              iv, iv_len, add, add_len,
                              input, output, check_tag, tag_len)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_ccm_compare_tags(tag, check_tag, tag_len)) != 0) {
        mbedtls_platform_zeroize(output, length);
        return ret;
    }

    return 0;
}

int mbedtls_ccm_star_auth_decrypt(mbedtls_ccm_context *ctx, size_t length,
                                  const unsigned char *iv, size_t iv_len,
                                  const unsigned char *add, size_t add_len,
                                  const unsigned char *input, unsigned char *output,
                                  const unsigned char *tag, size_t tag_len)
{
    return ccm_auth_decrypt(ctx, MBEDTLS_CCM_STAR_DECRYPT, length,
                            iv, iv_len, add, add_len,
                            input, output, tag, tag_len);
}

int mbedtls_ccm_auth_decrypt(mbedtls_ccm_context *ctx, size_t length,
                             const unsigned char *iv, size_t iv_len,
                             const unsigned char *add, size_t add_len,
                             const unsigned char *input, unsigned char *output,
                             const unsigned char *tag, size_t tag_len)
{
    return ccm_auth_decrypt(ctx, MBEDTLS_CCM_DECRYPT, length,
                            iv, iv_len, add, add_len,
                            input, output, tag, tag_len);
}

#if defined(MBEDTLS_SELF_TEST) && defined(MBEDTLS_CCM_GCM_CAN_AES)
/*
 * Examples 1 to 3 from SP800-38C Appendix C
 */

#define NB_TESTS 3
#define CCM_SELFTEST_PT_MAX_LEN 24
#define CCM_SELFTEST_CT_MAX_LEN 32
/*
 * The data is the same for all tests, only the used length changes
 */
static const unsigned char key_test_data[] = {
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f
};

static const unsigned char iv_test_data[] = {
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b
};

static const unsigned char ad_test_data[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13
};

static const unsigned char msg_test_data[CCM_SELFTEST_PT_MAX_LEN] = {
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
};

static const size_t iv_len_test_data[NB_TESTS] = { 7, 8,  12 };
static const size_t add_len_test_data[NB_TESTS] = { 8, 16, 20 };
static const size_t msg_len_test_data[NB_TESTS] = { 4, 16, 24 };
static const size_t tag_len_test_data[NB_TESTS] = { 4, 6,  8  };

static const unsigned char res_test_data[NB_TESTS][CCM_SELFTEST_CT_MAX_LEN] = {
    {   0x71, 0x62, 0x01, 0x5b, 0x4d, 0xac, 0x25, 0x5d },
    {   0xd2, 0xa1, 0xf0, 0xe0, 0x51, 0xea, 0x5f, 0x62,
        0x08, 0x1a, 0x77, 0x92, 0x07, 0x3d, 0x59, 0x3d,
        0x1f, 0xc6, 0x4f, 0xbf, 0xac, 0xcd },
    {   0xe3, 0xb2, 0x01, 0xa9, 0xf5, 0xb7, 0x1a, 0x7a,
        0x9b, 0x1c, 0xea, 0xec, 0xcd, 0x97, 0xe7, 0x0b,
        0x61, 0x76, 0xaa, 0xd9, 0xa4, 0x42, 0x8a, 0xa5,
        0x48, 0x43, 0x92, 0xfb, 0xc1, 0xb0, 0x99, 0x51 }
};

int mbedtls_ccm_self_test(int verbose)
{
    mbedtls_ccm_context ctx;
    /*
     * Some hardware accelerators require the input and output buffers
     * would be in RAM, because the flash is not accessible.
     * Use buffers on the stack to hold the test vectors data.
     */
    unsigned char plaintext[CCM_SELFTEST_PT_MAX_LEN];
    unsigned char ciphertext[CCM_SELFTEST_CT_MAX_LEN];
    size_t i;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    mbedtls_ccm_init(&ctx);

    if (mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key_test_data,
                           8 * sizeof(key_test_data)) != 0) {
        if (verbose != 0) {
            mbedtls_printf("  CCM: setup failed");
        }

        return 1;
    }

    for (i = 0; i < NB_TESTS; i++) {
        if (verbose != 0) {
            mbedtls_printf("  CCM-AES #%u: ", (unsigned int) i + 1);
        }

        memset(plaintext, 0, CCM_SELFTEST_PT_MAX_LEN);
        memset(ciphertext, 0, CCM_SELFTEST_CT_MAX_LEN);
        memcpy(plaintext, msg_test_data, msg_len_test_data[i]);

        ret = mbedtls_ccm_encrypt_and_tag(&ctx, msg_len_test_data[i],
                                          iv_test_data, iv_len_test_data[i],
                                          ad_test_data, add_len_test_data[i],
                                          plaintext, ciphertext,
                                          ciphertext + msg_len_test_data[i],
                                          tag_len_test_data[i]);

        if (ret != 0 ||
            memcmp(ciphertext, res_test_data[i],
                   msg_len_test_data[i] + tag_len_test_data[i]) != 0) {
            if (verbose != 0) {
                mbedtls_printf("failed\n");
            }

            return 1;
        }
        memset(plaintext, 0, CCM_SELFTEST_PT_MAX_LEN);

        ret = mbedtls_ccm_auth_decrypt(&ctx, msg_len_test_data[i],
                                       iv_test_data, iv_len_test_data[i],
                                       ad_test_data, add_len_test_data[i],
                                       ciphertext, plaintext,
                                       ciphertext + msg_len_test_data[i],
                                       tag_len_test_data[i]);

        if (ret != 0 ||
            memcmp(plaintext, msg_test_data, msg_len_test_data[i]) != 0) {
            if (verbose != 0) {
                mbedtls_printf("failed\n");
            }

            return 1;
        }

        if (verbose != 0) {
            mbedtls_printf("passed\n");
        }
    }

    mbedtls_ccm_free(&ctx);

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    return 0;
}

#endif /* MBEDTLS_SELF_TEST && MBEDTLS_AES_C */

#endif /* MBEDTLS_CCM_C */
