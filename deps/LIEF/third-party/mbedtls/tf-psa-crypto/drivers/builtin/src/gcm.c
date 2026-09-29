/*
 *  NIST SP800-38D compliant GCM implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * http://csrc.nist.gov/publications/nistpubs/800-38D/SP-800-38D.pdf
 *
 * See also:
 * [MGV] http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-revised-spec.pdf
 *
 * We use the algorithm described as Shoup's method with 4-bit tables in
 * [MGV] 4.1, pp. 12-13, to enhance speed without using too much memory.
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_GCM_C)

#include "mbedtls/private/gcm.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/private/error_common.h"
#include "mbedtls/constant_time.h"

#if defined(MBEDTLS_BLOCK_CIPHER_C)
#include "block_cipher_internal.h"
#endif

#include <string.h>

#if defined(MBEDTLS_AESNI_C)
#include "aesni.h"
#endif

#if defined(MBEDTLS_AESCE_C)
#include "aesce.h"
#endif

/* Used to select the acceleration mechanism */
#define MBEDTLS_GCM_ACC_SMALLTABLE  0
#define MBEDTLS_GCM_ACC_LARGETABLE  1
#define MBEDTLS_GCM_ACC_AESNI       2
#define MBEDTLS_GCM_ACC_AESCE       3

/*
 * Initialize a context
 */
void mbedtls_gcm_init(mbedtls_gcm_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_gcm_context));
}

static inline void gcm_set_acceleration(mbedtls_gcm_context *ctx)
{
#if defined(MBEDTLS_GCM_LARGE_TABLE)
    ctx->acceleration = MBEDTLS_GCM_ACC_LARGETABLE;
#else
    ctx->acceleration = MBEDTLS_GCM_ACC_SMALLTABLE;
#endif

#if defined(MBEDTLS_AESNI_HAVE_CODE)
    /* With CLMUL support, we need only h, not the rest of the table */
    if (mbedtls_aesni_has_support(MBEDTLS_AESNI_CLMUL)) {
        ctx->acceleration = MBEDTLS_GCM_ACC_AESNI;
    }
#endif

#if defined(MBEDTLS_AESCE_HAVE_CODE)
    if (MBEDTLS_AESCE_HAS_SUPPORT()) {
        ctx->acceleration = MBEDTLS_GCM_ACC_AESCE;
    }
#endif
}

static inline void gcm_gen_table_rightshift(uint64_t dst[2], const uint64_t src[2])
{
    uint8_t *u8Dst = (uint8_t *) dst;
    uint8_t *u8Src = (uint8_t *) src;

    MBEDTLS_PUT_UINT64_BE(MBEDTLS_GET_UINT64_BE(&src[1], 0) >> 1, &dst[1], 0);
    u8Dst[8] |= (u8Src[7] & 0x01) << 7;
    MBEDTLS_PUT_UINT64_BE(MBEDTLS_GET_UINT64_BE(&src[0], 0) >> 1, &dst[0], 0);
    u8Dst[0] ^= (u8Src[15] & 0x01) ? 0xE1 : 0;
}

/*
 * Precompute small multiples of H, that is set
 *      HH[i] || HL[i] = H times i,
 * where i is seen as a field element as in [MGV], ie high-order bits
 * correspond to low powers of P. The result is stored in the same way, that
 * is the high-order bit of HH corresponds to P^0 and the low-order bit of HL
 * corresponds to P^127.
 */
static int gcm_gen_table(mbedtls_gcm_context *ctx)
{
    int ret, i, j;
    uint64_t u64h[2] = { 0 };
    uint8_t *h = (uint8_t *) u64h;

#if defined(MBEDTLS_BLOCK_CIPHER_C)
    ret = mbedtls_block_cipher_encrypt(&ctx->block_cipher_ctx, h, h);
#else
    size_t olen = 0;
    ret = mbedtls_cipher_update(&ctx->cipher_ctx, h, 16, h, &olen);
#endif
    if (ret != 0) {
        return ret;
    }

    gcm_set_acceleration(ctx);

    /* MBEDTLS_GCM_HTABLE_SIZE/2 = 1000 corresponds to 1 in GF(2^128) */
    ctx->H[MBEDTLS_GCM_HTABLE_SIZE/2][0] = u64h[0];
    ctx->H[MBEDTLS_GCM_HTABLE_SIZE/2][1] = u64h[1];

    switch (ctx->acceleration) {
#if defined(MBEDTLS_AESNI_HAVE_CODE)
        case MBEDTLS_GCM_ACC_AESNI:
            return 0;
#endif

#if defined(MBEDTLS_AESCE_HAVE_CODE)
        case MBEDTLS_GCM_ACC_AESCE:
            return 0;
#endif

        default:
            /* 0 corresponds to 0 in GF(2^128) */
            ctx->H[0][0] = 0;
            ctx->H[0][1] = 0;

            for (i = MBEDTLS_GCM_HTABLE_SIZE/4; i > 0; i >>= 1) {
                gcm_gen_table_rightshift(ctx->H[i], ctx->H[i*2]);
            }

#if !defined(MBEDTLS_GCM_LARGE_TABLE)
            /* pack elements of H as 64-bits ints, big-endian */
            for (i = MBEDTLS_GCM_HTABLE_SIZE/2; i > 0; i >>= 1) {
                MBEDTLS_PUT_UINT64_BE(ctx->H[i][0], &ctx->H[i][0], 0);
                MBEDTLS_PUT_UINT64_BE(ctx->H[i][1], &ctx->H[i][1], 0);
            }
#endif

            for (i = 2; i < MBEDTLS_GCM_HTABLE_SIZE; i <<= 1) {
                for (j = 1; j < i; j++) {
                    mbedtls_xor_no_simd((unsigned char *) ctx->H[i+j],
                                        (unsigned char *) ctx->H[i],
                                        (unsigned char *) ctx->H[j],
                                        16);
                }
            }
    }

    return 0;
}

int mbedtls_gcm_setkey(mbedtls_gcm_context *ctx,
                       mbedtls_cipher_id_t cipher,
                       const unsigned char *key,
                       unsigned int keybits)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (keybits != 128 && keybits != 192 && keybits != 256) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

#if defined(MBEDTLS_BLOCK_CIPHER_C)
    mbedtls_block_cipher_free(&ctx->block_cipher_ctx);

    if ((ret = mbedtls_block_cipher_setup(&ctx->block_cipher_ctx, cipher)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_block_cipher_setkey(&ctx->block_cipher_ctx, key, keybits)) != 0) {
        return ret;
    }
#else
    const mbedtls_cipher_info_t *cipher_info;

    cipher_info = mbedtls_cipher_info_from_values(cipher, keybits,
                                                  MBEDTLS_MODE_ECB);
    if (cipher_info == NULL) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    if (mbedtls_cipher_info_get_block_size(cipher_info) != 16) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
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

    if ((ret = gcm_gen_table(ctx)) != 0) {
        return ret;
    }

    return 0;
}

#if defined(MBEDTLS_GCM_LARGE_TABLE)
static const uint16_t last8[256] = {
    0x0000, 0xc201, 0x8403, 0x4602, 0x0807, 0xca06, 0x8c04, 0x4e05,
    0x100e, 0xd20f, 0x940d, 0x560c, 0x1809, 0xda08, 0x9c0a, 0x5e0b,
    0x201c, 0xe21d, 0xa41f, 0x661e, 0x281b, 0xea1a, 0xac18, 0x6e19,
    0x3012, 0xf213, 0xb411, 0x7610, 0x3815, 0xfa14, 0xbc16, 0x7e17,
    0x4038, 0x8239, 0xc43b, 0x063a, 0x483f, 0x8a3e, 0xcc3c, 0x0e3d,
    0x5036, 0x9237, 0xd435, 0x1634, 0x5831, 0x9a30, 0xdc32, 0x1e33,
    0x6024, 0xa225, 0xe427, 0x2626, 0x6823, 0xaa22, 0xec20, 0x2e21,
    0x702a, 0xb22b, 0xf429, 0x3628, 0x782d, 0xba2c, 0xfc2e, 0x3e2f,
    0x8070, 0x4271, 0x0473, 0xc672, 0x8877, 0x4a76, 0x0c74, 0xce75,
    0x907e, 0x527f, 0x147d, 0xd67c, 0x9879, 0x5a78, 0x1c7a, 0xde7b,
    0xa06c, 0x626d, 0x246f, 0xe66e, 0xa86b, 0x6a6a, 0x2c68, 0xee69,
    0xb062, 0x7263, 0x3461, 0xf660, 0xb865, 0x7a64, 0x3c66, 0xfe67,
    0xc048, 0x0249, 0x444b, 0x864a, 0xc84f, 0x0a4e, 0x4c4c, 0x8e4d,
    0xd046, 0x1247, 0x5445, 0x9644, 0xd841, 0x1a40, 0x5c42, 0x9e43,
    0xe054, 0x2255, 0x6457, 0xa656, 0xe853, 0x2a52, 0x6c50, 0xae51,
    0xf05a, 0x325b, 0x7459, 0xb658, 0xf85d, 0x3a5c, 0x7c5e, 0xbe5f,
    0x00e1, 0xc2e0, 0x84e2, 0x46e3, 0x08e6, 0xcae7, 0x8ce5, 0x4ee4,
    0x10ef, 0xd2ee, 0x94ec, 0x56ed, 0x18e8, 0xdae9, 0x9ceb, 0x5eea,
    0x20fd, 0xe2fc, 0xa4fe, 0x66ff, 0x28fa, 0xeafb, 0xacf9, 0x6ef8,
    0x30f3, 0xf2f2, 0xb4f0, 0x76f1, 0x38f4, 0xfaf5, 0xbcf7, 0x7ef6,
    0x40d9, 0x82d8, 0xc4da, 0x06db, 0x48de, 0x8adf, 0xccdd, 0x0edc,
    0x50d7, 0x92d6, 0xd4d4, 0x16d5, 0x58d0, 0x9ad1, 0xdcd3, 0x1ed2,
    0x60c5, 0xa2c4, 0xe4c6, 0x26c7, 0x68c2, 0xaac3, 0xecc1, 0x2ec0,
    0x70cb, 0xb2ca, 0xf4c8, 0x36c9, 0x78cc, 0xbacd, 0xfccf, 0x3ece,
    0x8091, 0x4290, 0x0492, 0xc693, 0x8896, 0x4a97, 0x0c95, 0xce94,
    0x909f, 0x529e, 0x149c, 0xd69d, 0x9898, 0x5a99, 0x1c9b, 0xde9a,
    0xa08d, 0x628c, 0x248e, 0xe68f, 0xa88a, 0x6a8b, 0x2c89, 0xee88,
    0xb083, 0x7282, 0x3480, 0xf681, 0xb884, 0x7a85, 0x3c87, 0xfe86,
    0xc0a9, 0x02a8, 0x44aa, 0x86ab, 0xc8ae, 0x0aaf, 0x4cad, 0x8eac,
    0xd0a7, 0x12a6, 0x54a4, 0x96a5, 0xd8a0, 0x1aa1, 0x5ca3, 0x9ea2,
    0xe0b5, 0x22b4, 0x64b6, 0xa6b7, 0xe8b2, 0x2ab3, 0x6cb1, 0xaeb0,
    0xf0bb, 0x32ba, 0x74b8, 0xb6b9, 0xf8bc, 0x3abd, 0x7cbf, 0xbebe
};

static void gcm_mult_largetable(uint8_t *output, const uint8_t *x, uint64_t H[256][2])
{
    int i;
    uint64_t u64z[2];
    uint16_t *u16z = (uint16_t *) u64z;
    uint8_t *u8z = (uint8_t *) u64z;
    uint8_t rem;

    u64z[0] = 0;
    u64z[1] = 0;

    if (MBEDTLS_IS_BIG_ENDIAN) {
        for (i = 15; i > 0; i--) {
            mbedtls_xor_no_simd(u8z, u8z, (uint8_t *) H[x[i]], 16);
            rem = u8z[15];

            u64z[1] >>= 8;
            u8z[8] = u8z[7];
            u64z[0] >>= 8;

            u16z[0] ^= MBEDTLS_GET_UINT16_LE(&last8[rem], 0);
        }
    } else {
        for (i = 15; i > 0; i--) {
            mbedtls_xor_no_simd(u8z, u8z, (uint8_t *) H[x[i]], 16);
            rem = u8z[15];

            u64z[1] <<= 8;
            u8z[8] = u8z[7];
            u64z[0] <<= 8;

            u16z[0] ^= last8[rem];
        }
    }

    mbedtls_xor_no_simd(output, u8z, (uint8_t *) H[x[0]], 16);
}
#else
/*
 * Shoup's method for multiplication use this table with
 *      last4[x] = x times P^128
 * where x and last4[x] are seen as elements of GF(2^128) as in [MGV]
 */
static const uint16_t last4[16] =
{
    0x0000, 0x1c20, 0x3840, 0x2460,
    0x7080, 0x6ca0, 0x48c0, 0x54e0,
    0xe100, 0xfd20, 0xd940, 0xc560,
    0x9180, 0x8da0, 0xa9c0, 0xb5e0
};

static void gcm_mult_smalltable(uint8_t *output, const uint8_t *x, uint64_t H[16][2])
{
    int i = 0;
    unsigned char lo, hi, rem;
    uint64_t u64z[2];
    const uint64_t *pu64z = NULL;
    uint8_t *u8z = (uint8_t *) u64z;

    lo = x[15] & 0xf;
    hi = (x[15] >> 4) & 0xf;

    pu64z = H[lo];

    rem = (unsigned char) pu64z[1] & 0xf;
    u64z[1] = (pu64z[0] << 60) | (pu64z[1] >> 4);
    u64z[0] = (pu64z[0] >> 4);
    u64z[0] ^= (uint64_t) last4[rem] << 48;
    mbedtls_xor_no_simd(u8z, u8z, (uint8_t *) H[hi], 16);

    for (i = 14; i >= 0; i--) {
        lo = x[i] & 0xf;
        hi = (x[i] >> 4) & 0xf;

        rem = (unsigned char) u64z[1] & 0xf;
        u64z[1] = (u64z[0] << 60) | (u64z[1] >> 4);
        u64z[0] = (u64z[0] >> 4);
        u64z[0] ^= (uint64_t) last4[rem] << 48;
        mbedtls_xor_no_simd(u8z, u8z, (uint8_t *) H[lo], 16);

        rem = (unsigned char) u64z[1] & 0xf;
        u64z[1] = (u64z[0] << 60) | (u64z[1] >> 4);
        u64z[0] = (u64z[0] >> 4);
        u64z[0] ^= (uint64_t) last4[rem] << 48;
        mbedtls_xor_no_simd(u8z, u8z, (uint8_t *) H[hi], 16);
    }

    MBEDTLS_PUT_UINT64_BE(u64z[0], output, 0);
    MBEDTLS_PUT_UINT64_BE(u64z[1], output, 8);
}
#endif

/*
 * Sets output to x times H using the precomputed tables.
 * x and output are seen as elements of GF(2^128) as in [MGV].
 */
static void gcm_mult(mbedtls_gcm_context *ctx, const unsigned char x[16],
                     unsigned char output[16])
{
    switch (ctx->acceleration) {
#if defined(MBEDTLS_AESNI_HAVE_CODE)
        case MBEDTLS_GCM_ACC_AESNI:
            mbedtls_aesni_gcm_mult(output, x, (uint8_t *) ctx->H[MBEDTLS_GCM_HTABLE_SIZE/2]);
            break;
#endif

#if defined(MBEDTLS_AESCE_HAVE_CODE)
        case MBEDTLS_GCM_ACC_AESCE:
            mbedtls_aesce_gcm_mult(output, x, (uint8_t *) ctx->H[MBEDTLS_GCM_HTABLE_SIZE/2]);
            break;
#endif

#if defined(MBEDTLS_GCM_LARGE_TABLE)
        case MBEDTLS_GCM_ACC_LARGETABLE:
            gcm_mult_largetable(output, x, ctx->H);
            break;
#else
        case MBEDTLS_GCM_ACC_SMALLTABLE:
            gcm_mult_smalltable(output, x, ctx->H);
            break;
#endif
    }

    return;
}

int mbedtls_gcm_starts(mbedtls_gcm_context *ctx,
                       int mode,
                       const unsigned char *iv, size_t iv_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char work_buf[16];
    const unsigned char *p;
    size_t use_len;
    uint64_t iv_bits;
#if !defined(MBEDTLS_BLOCK_CIPHER_C)
    size_t olen = 0;
#endif

    /* IV is limited to 2^64 bits, so 2^61 bytes */
    /* IV is not allowed to be zero length */
    if (iv_len == 0 || (uint64_t) iv_len >> 61 != 0) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    memset(ctx->y, 0x00, sizeof(ctx->y));
    memset(ctx->buf, 0x00, sizeof(ctx->buf));

    ctx->mode = mode;
    ctx->len = 0;
    ctx->add_len = 0;

    if (iv_len == 12) {
        memcpy(ctx->y, iv, iv_len);
        ctx->y[15] = 1;
    } else {
        memset(work_buf, 0x00, 16);
        iv_bits = (uint64_t) iv_len * 8;
        MBEDTLS_PUT_UINT64_BE(iv_bits, work_buf, 8);

        p = iv;
        while (iv_len > 0) {
            use_len = (iv_len < 16) ? iv_len : 16;

#if defined(MBEDTLS_COMPILER_IS_GCC) && (MBEDTLS_GCC_VERSION >= 70110)
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wstringop-overflow=0"
#endif

            mbedtls_xor(ctx->y, ctx->y, p, use_len);

#if defined(MBEDTLS_COMPILER_IS_GCC) && (MBEDTLS_GCC_VERSION >= 70110)
#pragma GCC diagnostic pop
#endif

            gcm_mult(ctx, ctx->y, ctx->y);

            iv_len -= use_len;
            p += use_len;
        }

        mbedtls_xor(ctx->y, ctx->y, work_buf, 16);

        gcm_mult(ctx, ctx->y, ctx->y);
    }


#if defined(MBEDTLS_BLOCK_CIPHER_C)
    ret = mbedtls_block_cipher_encrypt(&ctx->block_cipher_ctx, ctx->y, ctx->base_ectr);
#else
    ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctx->y, 16, ctx->base_ectr, &olen);
#endif
    if (ret != 0) {
        return ret;
    }

    return 0;
}

/**
 * mbedtls_gcm_context::buf contains the partial state of the computation of
 * the authentication tag.
 * mbedtls_gcm_context::add_len and mbedtls_gcm_context::len indicate
 * different stages of the computation:
 *     * len == 0 && add_len == 0:      initial state
 *     * len == 0 && add_len % 16 != 0: the first `add_len % 16` bytes have
 *                                      a partial block of AD that has been
 *                                      xored in but not yet multiplied in.
 *     * len == 0 && add_len % 16 == 0: the authentication tag is correct if
 *                                      the data ends now.
 *     * len % 16 != 0:                 the first `len % 16` bytes have
 *                                      a partial block of ciphertext that has
 *                                      been xored in but not yet multiplied in.
 *     * len > 0 && len % 16 == 0:      the authentication tag is correct if
 *                                      the data ends now.
 */
int mbedtls_gcm_update_ad(mbedtls_gcm_context *ctx,
                          const unsigned char *add, size_t add_len)
{
    const unsigned char *p;
    size_t use_len, offset;
    uint64_t new_add_len;

    /* AD is limited to 2^64 bits, ie 2^61 bytes
     * Also check for possible overflow */
#if SIZE_MAX > 0xFFFFFFFFFFFFFFFFULL
    if (add_len > 0xFFFFFFFFFFFFFFFFULL) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }
#endif
    new_add_len = ctx->add_len + (uint64_t) add_len;
    if (new_add_len < ctx->add_len || new_add_len >> 61 != 0) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    offset = ctx->add_len % 16;
    p = add;

    if (offset != 0) {
        use_len = 16 - offset;
        if (use_len > add_len) {
            use_len = add_len;
        }

        mbedtls_xor(ctx->buf + offset, ctx->buf + offset, p, use_len);

        if (offset + use_len == 16) {
            gcm_mult(ctx, ctx->buf, ctx->buf);
        }

        ctx->add_len += use_len;
        add_len -= use_len;
        p += use_len;
    }

    ctx->add_len += add_len;

    while (add_len >= 16) {
        mbedtls_xor(ctx->buf, ctx->buf, p, 16);

        gcm_mult(ctx, ctx->buf, ctx->buf);

        add_len -= 16;
        p += 16;
    }

    if (add_len > 0) {
        mbedtls_xor(ctx->buf, ctx->buf, p, add_len);
    }

    return 0;
}

/* Increment the counter. */
static void gcm_incr(unsigned char y[16])
{
    uint32_t x = MBEDTLS_GET_UINT32_BE(y, 12);
    x++;
    MBEDTLS_PUT_UINT32_BE(x, y, 12);
}

/* Calculate and apply the encryption mask. Process use_len bytes of data,
 * starting at position offset in the mask block. */
static int gcm_mask(mbedtls_gcm_context *ctx,
                    unsigned char ectr[16],
                    size_t offset, size_t use_len,
                    const unsigned char *input,
                    unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

#if defined(MBEDTLS_BLOCK_CIPHER_C)
    ret = mbedtls_block_cipher_encrypt(&ctx->block_cipher_ctx, ctx->y, ectr);
#else
    size_t olen = 0;
    ret = mbedtls_cipher_update(&ctx->cipher_ctx, ctx->y, 16, ectr, &olen);
#endif
    if (ret != 0) {
        mbedtls_platform_zeroize(ectr, 16);
        return ret;
    }

    if (ctx->mode == MBEDTLS_GCM_DECRYPT) {
        mbedtls_xor(ctx->buf + offset, ctx->buf + offset, input, use_len);
    }
    mbedtls_xor(output, ectr + offset, input, use_len);
    if (ctx->mode == MBEDTLS_GCM_ENCRYPT) {
        mbedtls_xor(ctx->buf + offset, ctx->buf + offset, output, use_len);
    }

    return 0;
}

int mbedtls_gcm_update(mbedtls_gcm_context *ctx,
                       const unsigned char *input, size_t input_length,
                       unsigned char *output, size_t output_size,
                       size_t *output_length)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const unsigned char *p = input;
    unsigned char *out_p = output;
    size_t offset;
    unsigned char ectr[16] = { 0 };

    if (output_size < input_length) {
        return MBEDTLS_ERR_GCM_BUFFER_TOO_SMALL;
    }
    *output_length = input_length;

    /* Exit early if input_length==0 so that we don't do any pointer arithmetic
     * on a potentially null pointer.
     * Returning early also means that the last partial block of AD remains
     * untouched for mbedtls_gcm_finish */
    if (input_length == 0) {
        return 0;
    }

    if (output > input && (size_t) (output - input) < input_length) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    /* Total length is restricted to 2^39 - 256 bits, ie 2^36 - 2^5 bytes
     * Also check for possible overflow */
    if (ctx->len + input_length < ctx->len ||
        (uint64_t) ctx->len + input_length > 0xFFFFFFFE0ull) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    if (ctx->len == 0 && ctx->add_len % 16 != 0) {
        gcm_mult(ctx, ctx->buf, ctx->buf);
    }

    offset = ctx->len % 16;
    if (offset != 0) {
        size_t use_len = 16 - offset;
        if (use_len > input_length) {
            use_len = input_length;
        }

        if ((ret = gcm_mask(ctx, ectr, offset, use_len, p, out_p)) != 0) {
            return ret;
        }

        if (offset + use_len == 16) {
            gcm_mult(ctx, ctx->buf, ctx->buf);
        }

        ctx->len += use_len;
        input_length -= use_len;
        p += use_len;
        out_p += use_len;
    }

    ctx->len += input_length;

    while (input_length >= 16) {
        gcm_incr(ctx->y);
        if ((ret = gcm_mask(ctx, ectr, 0, 16, p, out_p)) != 0) {
            return ret;
        }

        gcm_mult(ctx, ctx->buf, ctx->buf);

        input_length -= 16;
        p += 16;
        out_p += 16;
    }

    if (input_length > 0) {
        gcm_incr(ctx->y);
        if ((ret = gcm_mask(ctx, ectr, 0, input_length, p, out_p)) != 0) {
            return ret;
        }
    }

    mbedtls_platform_zeroize(ectr, sizeof(ectr));
    return 0;
}

int mbedtls_gcm_finish(mbedtls_gcm_context *ctx,
                       unsigned char *output, size_t output_size,
                       size_t *output_length,
                       unsigned char *tag, size_t tag_len)
{
    unsigned char work_buf[16];
    uint64_t orig_len;
    uint64_t orig_add_len;

    /* We never pass any output in finish(). The output parameter exists only
     * for the sake of alternative implementations. */
    (void) output;
    (void) output_size;
    *output_length = 0;

    /* Total length is restricted to 2^39 - 256 bits, ie 2^36 - 2^5 bytes
     * and AD length is restricted to 2^64 bits, ie 2^61 bytes so neither of
     * the two multiplications would overflow. */
    orig_len = ctx->len * 8;
    orig_add_len = ctx->add_len * 8;

    if (ctx->len == 0 && ctx->add_len % 16 != 0) {
        gcm_mult(ctx, ctx->buf, ctx->buf);
    }

    if (tag_len > 16 || tag_len < 4) {
        return MBEDTLS_ERR_GCM_BAD_INPUT;
    }

    if (ctx->len % 16 != 0) {
        gcm_mult(ctx, ctx->buf, ctx->buf);
    }

    memcpy(tag, ctx->base_ectr, tag_len);

    if (orig_len || orig_add_len) {
        memset(work_buf, 0x00, 16);

        MBEDTLS_PUT_UINT32_BE((orig_add_len >> 32), work_buf, 0);
        MBEDTLS_PUT_UINT32_BE((orig_add_len), work_buf, 4);
        MBEDTLS_PUT_UINT32_BE((orig_len     >> 32), work_buf, 8);
        MBEDTLS_PUT_UINT32_BE((orig_len), work_buf, 12);

        mbedtls_xor(ctx->buf, ctx->buf, work_buf, 16);

        gcm_mult(ctx, ctx->buf, ctx->buf);

        mbedtls_xor(tag, tag, ctx->buf, tag_len);
    }

    return 0;
}

int mbedtls_gcm_crypt_and_tag(mbedtls_gcm_context *ctx,
                              int mode,
                              size_t length,
                              const unsigned char *iv,
                              size_t iv_len,
                              const unsigned char *add,
                              size_t add_len,
                              const unsigned char *input,
                              unsigned char *output,
                              size_t tag_len,
                              unsigned char *tag)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t olen;

    if ((ret = mbedtls_gcm_starts(ctx, mode, iv, iv_len)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_gcm_update_ad(ctx, add, add_len)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_gcm_update(ctx, input, length,
                                  output, length, &olen)) != 0) {
        return ret;
    }

    if ((ret = mbedtls_gcm_finish(ctx, NULL, 0, &olen, tag, tag_len)) != 0) {
        return ret;
    }

    return 0;
}

int mbedtls_gcm_auth_decrypt(mbedtls_gcm_context *ctx,
                             size_t length,
                             const unsigned char *iv,
                             size_t iv_len,
                             const unsigned char *add,
                             size_t add_len,
                             const unsigned char *tag,
                             size_t tag_len,
                             const unsigned char *input,
                             unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char check_tag[16];
    int diff;

    if ((ret = mbedtls_gcm_crypt_and_tag(ctx, MBEDTLS_GCM_DECRYPT, length,
                                         iv, iv_len, add, add_len,
                                         input, output, tag_len, check_tag)) != 0) {
        return ret;
    }

    /* Check tag in "constant-time" */
    diff = mbedtls_ct_memcmp(tag, check_tag, tag_len);

    if (diff != 0) {
        mbedtls_platform_zeroize(output, length);
        return MBEDTLS_ERR_GCM_AUTH_FAILED;
    }

    return 0;
}

void mbedtls_gcm_free(mbedtls_gcm_context *ctx)
{
    if (ctx == NULL) {
        return;
    }
#if defined(MBEDTLS_BLOCK_CIPHER_C)
    mbedtls_block_cipher_free(&ctx->block_cipher_ctx);
#else
    mbedtls_cipher_free(&ctx->cipher_ctx);
#endif
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_gcm_context));
}

#if defined(MBEDTLS_SELF_TEST) && defined(MBEDTLS_CCM_GCM_CAN_AES)
/*
 * AES-GCM test vectors from:
 *
 * http://csrc.nist.gov/groups/STM/cavp/documents/mac/gcmtestvectors.zip
 */
#define MAX_TESTS   6

static const int key_index_test_data[MAX_TESTS] =
{ 0, 0, 1, 1, 1, 1 };

static const unsigned char key_test_data[][32] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
      0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08,
      0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
      0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08 },
};

static const size_t iv_len_test_data[MAX_TESTS] =
{ 12, 12, 12, 12, 8, 60 };

static const int iv_index_test_data[MAX_TESTS] =
{ 0, 0, 1, 1, 1, 2 };

static const unsigned char iv_test_data[][64] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00 },
    { 0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
      0xde, 0xca, 0xf8, 0x88 },
    { 0x93, 0x13, 0x22, 0x5d, 0xf8, 0x84, 0x06, 0xe5,
      0x55, 0x90, 0x9c, 0x5a, 0xff, 0x52, 0x69, 0xaa,
      0x6a, 0x7a, 0x95, 0x38, 0x53, 0x4f, 0x7d, 0xa1,
      0xe4, 0xc3, 0x03, 0xd2, 0xa3, 0x18, 0xa7, 0x28,
      0xc3, 0xc0, 0xc9, 0x51, 0x56, 0x80, 0x95, 0x39,
      0xfc, 0xf0, 0xe2, 0x42, 0x9a, 0x6b, 0x52, 0x54,
      0x16, 0xae, 0xdb, 0xf5, 0xa0, 0xde, 0x6a, 0x57,
      0xa6, 0x37, 0xb3, 0x9b },
};

static const size_t add_len_test_data[MAX_TESTS] =
{ 0, 0, 0, 20, 20, 20 };

static const int add_index_test_data[MAX_TESTS] =
{ 0, 0, 0, 1, 1, 1 };

static const unsigned char additional_test_data[][64] =
{
    { 0x00 },
    { 0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
      0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
      0xab, 0xad, 0xda, 0xd2 },
};

static const size_t pt_len_test_data[MAX_TESTS] =
{ 0, 16, 64, 60, 60, 60 };

static const int pt_index_test_data[MAX_TESTS] =
{ 0, 0, 1, 1, 1, 1 };

static const unsigned char pt_test_data[][64] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5,
      0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a,
      0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
      0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72,
      0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53,
      0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
      0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57,
      0xba, 0x63, 0x7b, 0x39, 0x1a, 0xaf, 0xd2, 0x55 },
};

static const unsigned char ct_test_data[][64] =
{
    { 0x00 },
    { 0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92,
      0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78 },
    { 0x42, 0x83, 0x1e, 0xc2, 0x21, 0x77, 0x74, 0x24,
      0x4b, 0x72, 0x21, 0xb7, 0x84, 0xd0, 0xd4, 0x9c,
      0xe3, 0xaa, 0x21, 0x2f, 0x2c, 0x02, 0xa4, 0xe0,
      0x35, 0xc1, 0x7e, 0x23, 0x29, 0xac, 0xa1, 0x2e,
      0x21, 0xd5, 0x14, 0xb2, 0x54, 0x66, 0x93, 0x1c,
      0x7d, 0x8f, 0x6a, 0x5a, 0xac, 0x84, 0xaa, 0x05,
      0x1b, 0xa3, 0x0b, 0x39, 0x6a, 0x0a, 0xac, 0x97,
      0x3d, 0x58, 0xe0, 0x91, 0x47, 0x3f, 0x59, 0x85 },
    { 0x42, 0x83, 0x1e, 0xc2, 0x21, 0x77, 0x74, 0x24,
      0x4b, 0x72, 0x21, 0xb7, 0x84, 0xd0, 0xd4, 0x9c,
      0xe3, 0xaa, 0x21, 0x2f, 0x2c, 0x02, 0xa4, 0xe0,
      0x35, 0xc1, 0x7e, 0x23, 0x29, 0xac, 0xa1, 0x2e,
      0x21, 0xd5, 0x14, 0xb2, 0x54, 0x66, 0x93, 0x1c,
      0x7d, 0x8f, 0x6a, 0x5a, 0xac, 0x84, 0xaa, 0x05,
      0x1b, 0xa3, 0x0b, 0x39, 0x6a, 0x0a, 0xac, 0x97,
      0x3d, 0x58, 0xe0, 0x91 },
    { 0x61, 0x35, 0x3b, 0x4c, 0x28, 0x06, 0x93, 0x4a,
      0x77, 0x7f, 0xf5, 0x1f, 0xa2, 0x2a, 0x47, 0x55,
      0x69, 0x9b, 0x2a, 0x71, 0x4f, 0xcd, 0xc6, 0xf8,
      0x37, 0x66, 0xe5, 0xf9, 0x7b, 0x6c, 0x74, 0x23,
      0x73, 0x80, 0x69, 0x00, 0xe4, 0x9f, 0x24, 0xb2,
      0x2b, 0x09, 0x75, 0x44, 0xd4, 0x89, 0x6b, 0x42,
      0x49, 0x89, 0xb5, 0xe1, 0xeb, 0xac, 0x0f, 0x07,
      0xc2, 0x3f, 0x45, 0x98 },
    { 0x8c, 0xe2, 0x49, 0x98, 0x62, 0x56, 0x15, 0xb6,
      0x03, 0xa0, 0x33, 0xac, 0xa1, 0x3f, 0xb8, 0x94,
      0xbe, 0x91, 0x12, 0xa5, 0xc3, 0xa2, 0x11, 0xa8,
      0xba, 0x26, 0x2a, 0x3c, 0xca, 0x7e, 0x2c, 0xa7,
      0x01, 0xe4, 0xa9, 0xa4, 0xfb, 0xa4, 0x3c, 0x90,
      0xcc, 0xdc, 0xb2, 0x81, 0xd4, 0x8c, 0x7c, 0x6f,
      0xd6, 0x28, 0x75, 0xd2, 0xac, 0xa4, 0x17, 0x03,
      0x4c, 0x34, 0xae, 0xe5 },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { 0x00 },
    { 0x98, 0xe7, 0x24, 0x7c, 0x07, 0xf0, 0xfe, 0x41,
      0x1c, 0x26, 0x7e, 0x43, 0x84, 0xb0, 0xf6, 0x00 },
    { 0x39, 0x80, 0xca, 0x0b, 0x3c, 0x00, 0xe8, 0x41,
      0xeb, 0x06, 0xfa, 0xc4, 0x87, 0x2a, 0x27, 0x57,
      0x85, 0x9e, 0x1c, 0xea, 0xa6, 0xef, 0xd9, 0x84,
      0x62, 0x85, 0x93, 0xb4, 0x0c, 0xa1, 0xe1, 0x9c,
      0x7d, 0x77, 0x3d, 0x00, 0xc1, 0x44, 0xc5, 0x25,
      0xac, 0x61, 0x9d, 0x18, 0xc8, 0x4a, 0x3f, 0x47,
      0x18, 0xe2, 0x44, 0x8b, 0x2f, 0xe3, 0x24, 0xd9,
      0xcc, 0xda, 0x27, 0x10, 0xac, 0xad, 0xe2, 0x56 },
    { 0x39, 0x80, 0xca, 0x0b, 0x3c, 0x00, 0xe8, 0x41,
      0xeb, 0x06, 0xfa, 0xc4, 0x87, 0x2a, 0x27, 0x57,
      0x85, 0x9e, 0x1c, 0xea, 0xa6, 0xef, 0xd9, 0x84,
      0x62, 0x85, 0x93, 0xb4, 0x0c, 0xa1, 0xe1, 0x9c,
      0x7d, 0x77, 0x3d, 0x00, 0xc1, 0x44, 0xc5, 0x25,
      0xac, 0x61, 0x9d, 0x18, 0xc8, 0x4a, 0x3f, 0x47,
      0x18, 0xe2, 0x44, 0x8b, 0x2f, 0xe3, 0x24, 0xd9,
      0xcc, 0xda, 0x27, 0x10 },
    { 0x0f, 0x10, 0xf5, 0x99, 0xae, 0x14, 0xa1, 0x54,
      0xed, 0x24, 0xb3, 0x6e, 0x25, 0x32, 0x4d, 0xb8,
      0xc5, 0x66, 0x63, 0x2e, 0xf2, 0xbb, 0xb3, 0x4f,
      0x83, 0x47, 0x28, 0x0f, 0xc4, 0x50, 0x70, 0x57,
      0xfd, 0xdc, 0x29, 0xdf, 0x9a, 0x47, 0x1f, 0x75,
      0xc6, 0x65, 0x41, 0xd4, 0xd4, 0xda, 0xd1, 0xc9,
      0xe9, 0x3a, 0x19, 0xa5, 0x8e, 0x8b, 0x47, 0x3f,
      0xa0, 0xf0, 0x62, 0xf7 },
    { 0xd2, 0x7e, 0x88, 0x68, 0x1c, 0xe3, 0x24, 0x3c,
      0x48, 0x30, 0x16, 0x5a, 0x8f, 0xdc, 0xf9, 0xff,
      0x1d, 0xe9, 0xa1, 0xd8, 0xe6, 0xb4, 0x47, 0xef,
      0x6e, 0xf7, 0xb7, 0x98, 0x28, 0x66, 0x6e, 0x45,
      0x81, 0xe7, 0x90, 0x12, 0xaf, 0x34, 0xdd, 0xd9,
      0xe2, 0xf0, 0x37, 0x58, 0x9b, 0x29, 0x2d, 0xb3,
      0xe6, 0x7c, 0x03, 0x67, 0x45, 0xfa, 0x22, 0xe7,
      0xe9, 0xb7, 0x37, 0x3b },
    { 0x00 },
    { 0xce, 0xa7, 0x40, 0x3d, 0x4d, 0x60, 0x6b, 0x6e,
      0x07, 0x4e, 0xc5, 0xd3, 0xba, 0xf3, 0x9d, 0x18 },
    { 0x52, 0x2d, 0xc1, 0xf0, 0x99, 0x56, 0x7d, 0x07,
      0xf4, 0x7f, 0x37, 0xa3, 0x2a, 0x84, 0x42, 0x7d,
      0x64, 0x3a, 0x8c, 0xdc, 0xbf, 0xe5, 0xc0, 0xc9,
      0x75, 0x98, 0xa2, 0xbd, 0x25, 0x55, 0xd1, 0xaa,
      0x8c, 0xb0, 0x8e, 0x48, 0x59, 0x0d, 0xbb, 0x3d,
      0xa7, 0xb0, 0x8b, 0x10, 0x56, 0x82, 0x88, 0x38,
      0xc5, 0xf6, 0x1e, 0x63, 0x93, 0xba, 0x7a, 0x0a,
      0xbc, 0xc9, 0xf6, 0x62, 0x89, 0x80, 0x15, 0xad },
    { 0x52, 0x2d, 0xc1, 0xf0, 0x99, 0x56, 0x7d, 0x07,
      0xf4, 0x7f, 0x37, 0xa3, 0x2a, 0x84, 0x42, 0x7d,
      0x64, 0x3a, 0x8c, 0xdc, 0xbf, 0xe5, 0xc0, 0xc9,
      0x75, 0x98, 0xa2, 0xbd, 0x25, 0x55, 0xd1, 0xaa,
      0x8c, 0xb0, 0x8e, 0x48, 0x59, 0x0d, 0xbb, 0x3d,
      0xa7, 0xb0, 0x8b, 0x10, 0x56, 0x82, 0x88, 0x38,
      0xc5, 0xf6, 0x1e, 0x63, 0x93, 0xba, 0x7a, 0x0a,
      0xbc, 0xc9, 0xf6, 0x62 },
    { 0xc3, 0x76, 0x2d, 0xf1, 0xca, 0x78, 0x7d, 0x32,
      0xae, 0x47, 0xc1, 0x3b, 0xf1, 0x98, 0x44, 0xcb,
      0xaf, 0x1a, 0xe1, 0x4d, 0x0b, 0x97, 0x6a, 0xfa,
      0xc5, 0x2f, 0xf7, 0xd7, 0x9b, 0xba, 0x9d, 0xe0,
      0xfe, 0xb5, 0x82, 0xd3, 0x39, 0x34, 0xa4, 0xf0,
      0x95, 0x4c, 0xc2, 0x36, 0x3b, 0xc7, 0x3f, 0x78,
      0x62, 0xac, 0x43, 0x0e, 0x64, 0xab, 0xe4, 0x99,
      0xf4, 0x7c, 0x9b, 0x1f },
    { 0x5a, 0x8d, 0xef, 0x2f, 0x0c, 0x9e, 0x53, 0xf1,
      0xf7, 0x5d, 0x78, 0x53, 0x65, 0x9e, 0x2a, 0x20,
      0xee, 0xb2, 0xb2, 0x2a, 0xaf, 0xde, 0x64, 0x19,
      0xa0, 0x58, 0xab, 0x4f, 0x6f, 0x74, 0x6b, 0xf4,
      0x0f, 0xc0, 0xc3, 0xb7, 0x80, 0xf2, 0x44, 0x45,
      0x2d, 0xa3, 0xeb, 0xf1, 0xc5, 0xd8, 0x2c, 0xde,
      0xa2, 0x41, 0x89, 0x97, 0x20, 0x0e, 0xf8, 0x2e,
      0x44, 0xae, 0x7e, 0x3f },
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */
};

static const unsigned char tag_test_data[][16] =
{
    { 0x58, 0xe2, 0xfc, 0xce, 0xfa, 0x7e, 0x30, 0x61,
      0x36, 0x7f, 0x1d, 0x57, 0xa4, 0xe7, 0x45, 0x5a },
    { 0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec, 0x13, 0xbd,
      0xf5, 0x3a, 0x67, 0xb2, 0x12, 0x57, 0xbd, 0xdf },
    { 0x4d, 0x5c, 0x2a, 0xf3, 0x27, 0xcd, 0x64, 0xa6,
      0x2c, 0xf3, 0x5a, 0xbd, 0x2b, 0xa6, 0xfa, 0xb4 },
    { 0x5b, 0xc9, 0x4f, 0xbc, 0x32, 0x21, 0xa5, 0xdb,
      0x94, 0xfa, 0xe9, 0x5a, 0xe7, 0x12, 0x1a, 0x47 },
    { 0x36, 0x12, 0xd2, 0xe7, 0x9e, 0x3b, 0x07, 0x85,
      0x56, 0x1b, 0xe1, 0x4a, 0xac, 0xa2, 0xfc, 0xcb },
    { 0x61, 0x9c, 0xc5, 0xae, 0xff, 0xfe, 0x0b, 0xfa,
      0x46, 0x2a, 0xf4, 0x3c, 0x16, 0x99, 0xd0, 0x50 },
#if !defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH)
    { 0xcd, 0x33, 0xb2, 0x8a, 0xc7, 0x73, 0xf7, 0x4b,
      0xa0, 0x0e, 0xd1, 0xf3, 0x12, 0x57, 0x24, 0x35 },
    { 0x2f, 0xf5, 0x8d, 0x80, 0x03, 0x39, 0x27, 0xab,
      0x8e, 0xf4, 0xd4, 0x58, 0x75, 0x14, 0xf0, 0xfb },
    { 0x99, 0x24, 0xa7, 0xc8, 0x58, 0x73, 0x36, 0xbf,
      0xb1, 0x18, 0x02, 0x4d, 0xb8, 0x67, 0x4a, 0x14 },
    { 0x25, 0x19, 0x49, 0x8e, 0x80, 0xf1, 0x47, 0x8f,
      0x37, 0xba, 0x55, 0xbd, 0x6d, 0x27, 0x61, 0x8c },
    { 0x65, 0xdc, 0xc5, 0x7f, 0xcf, 0x62, 0x3a, 0x24,
      0x09, 0x4f, 0xcc, 0xa4, 0x0d, 0x35, 0x33, 0xf8 },
    { 0xdc, 0xf5, 0x66, 0xff, 0x29, 0x1c, 0x25, 0xbb,
      0xb8, 0x56, 0x8f, 0xc3, 0xd3, 0x76, 0xa6, 0xd9 },
    { 0x53, 0x0f, 0x8a, 0xfb, 0xc7, 0x45, 0x36, 0xb9,
      0xa9, 0x63, 0xb4, 0xf1, 0xc4, 0xcb, 0x73, 0x8b },
    { 0xd0, 0xd1, 0xc8, 0xa7, 0x99, 0x99, 0x6b, 0xf0,
      0x26, 0x5b, 0x98, 0xb5, 0xd4, 0x8a, 0xb9, 0x19 },
    { 0xb0, 0x94, 0xda, 0xc5, 0xd9, 0x34, 0x71, 0xbd,
      0xec, 0x1a, 0x50, 0x22, 0x70, 0xe3, 0xcc, 0x6c },
    { 0x76, 0xfc, 0x6e, 0xce, 0x0f, 0x4e, 0x17, 0x68,
      0xcd, 0xdf, 0x88, 0x53, 0xbb, 0x2d, 0x55, 0x1b },
    { 0x3a, 0x33, 0x7d, 0xbf, 0x46, 0xa7, 0x92, 0xc4,
      0x5e, 0x45, 0x49, 0x13, 0xfe, 0x2e, 0xa8, 0xf2 },
    { 0xa4, 0x4a, 0x82, 0x66, 0xee, 0x1c, 0x8e, 0xb0,
      0xc8, 0xb5, 0xd4, 0xcf, 0x5a, 0xe9, 0xf1, 0x9a },
#endif /* !MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH */
};

int mbedtls_gcm_self_test(int verbose)
{
    mbedtls_gcm_context ctx;
    unsigned char buf[64];
    unsigned char tag_buf[16];
    int i, j, ret;
    mbedtls_cipher_id_t cipher = MBEDTLS_CIPHER_ID_AES;
    size_t olen;

    if (verbose != 0) {
#if defined(MBEDTLS_AESNI_HAVE_CODE)
        if (mbedtls_aesni_has_support(MBEDTLS_AESNI_CLMUL)) {
            mbedtls_printf("  GCM note: using AESNI.\n");
        } else
#endif

#if defined(MBEDTLS_AESCE_HAVE_CODE)
        if (MBEDTLS_AESCE_HAS_SUPPORT()) {
            mbedtls_printf("  GCM note: using AESCE.\n");
        } else
#endif

        mbedtls_printf("  GCM note: built-in implementation.\n");
    }

    static const int loop_limit =
        (sizeof(ct_test_data) / sizeof(*ct_test_data)) / MAX_TESTS;

    for (j = 0; j < loop_limit; j++) {
        int key_len = 128 + 64 * j;

        for (i = 0; i < MAX_TESTS; i++) {
            if (verbose != 0) {
                mbedtls_printf("  AES-GCM-%3d #%d (%s): ",
                               key_len, i, "enc");
            }

            mbedtls_gcm_init(&ctx);

            ret = mbedtls_gcm_setkey(&ctx, cipher,
                                     key_test_data[key_index_test_data[i]],
                                     key_len);
            /*
             * AES-192 is an optional feature that may be unavailable when
             * there is an alternative underlying implementation such as a
             * PSA driver.
             */
            if (ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED && key_len == 192) {
                mbedtls_printf("skipped\n");
                break;
            } else if (ret != 0) {
                goto exit;
            }

            ret = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT,
                                            pt_len_test_data[i],
                                            iv_test_data[iv_index_test_data[i]],
                                            iv_len_test_data[i],
                                            additional_test_data[add_index_test_data[i]],
                                            add_len_test_data[i],
                                            pt_test_data[pt_index_test_data[i]],
                                            buf, 16, tag_buf);
            if (ret != 0) {
                goto exit;
            }

            if (memcmp(buf, ct_test_data[j * 6 + i],
                       pt_len_test_data[i]) != 0 ||
                memcmp(tag_buf, tag_test_data[j * 6 + i], 16) != 0) {
                ret = 1;
                goto exit;
            }

            mbedtls_gcm_free(&ctx);

            if (verbose != 0) {
                mbedtls_printf("passed\n");
            }

            mbedtls_gcm_init(&ctx);

            if (verbose != 0) {
                mbedtls_printf("  AES-GCM-%3d #%d (%s): ",
                               key_len, i, "dec");
            }

            ret = mbedtls_gcm_setkey(&ctx, cipher,
                                     key_test_data[key_index_test_data[i]],
                                     key_len);
            if (ret != 0) {
                goto exit;
            }

            ret = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_DECRYPT,
                                            pt_len_test_data[i],
                                            iv_test_data[iv_index_test_data[i]],
                                            iv_len_test_data[i],
                                            additional_test_data[add_index_test_data[i]],
                                            add_len_test_data[i],
                                            ct_test_data[j * 6 + i], buf, 16, tag_buf);

            if (ret != 0) {
                goto exit;
            }

            if (memcmp(buf, pt_test_data[pt_index_test_data[i]],
                       pt_len_test_data[i]) != 0 ||
                memcmp(tag_buf, tag_test_data[j * 6 + i], 16) != 0) {
                ret = 1;
                goto exit;
            }

            mbedtls_gcm_free(&ctx);

            if (verbose != 0) {
                mbedtls_printf("passed\n");
            }

            mbedtls_gcm_init(&ctx);

            if (verbose != 0) {
                mbedtls_printf("  AES-GCM-%3d #%d split (%s): ",
                               key_len, i, "enc");
            }

            ret = mbedtls_gcm_setkey(&ctx, cipher,
                                     key_test_data[key_index_test_data[i]],
                                     key_len);
            if (ret != 0) {
                goto exit;
            }

            ret = mbedtls_gcm_starts(&ctx, MBEDTLS_GCM_ENCRYPT,
                                     iv_test_data[iv_index_test_data[i]],
                                     iv_len_test_data[i]);
            if (ret != 0) {
                goto exit;
            }

            ret = mbedtls_gcm_update_ad(&ctx,
                                        additional_test_data[add_index_test_data[i]],
                                        add_len_test_data[i]);
            if (ret != 0) {
                goto exit;
            }

            if (pt_len_test_data[i] > 32) {
                size_t rest_len = pt_len_test_data[i] - 32;
                ret = mbedtls_gcm_update(&ctx,
                                         pt_test_data[pt_index_test_data[i]],
                                         32,
                                         buf, sizeof(buf), &olen);
                if (ret != 0) {
                    goto exit;
                }
                if (olen != 32) {
                    goto exit;
                }

                ret = mbedtls_gcm_update(&ctx,
                                         pt_test_data[pt_index_test_data[i]] + 32,
                                         rest_len,
                                         buf + 32, sizeof(buf) - 32, &olen);
                if (ret != 0) {
                    goto exit;
                }
                if (olen != rest_len) {
                    goto exit;
                }
            } else {
                ret = mbedtls_gcm_update(&ctx,
                                         pt_test_data[pt_index_test_data[i]],
                                         pt_len_test_data[i],
                                         buf, sizeof(buf), &olen);
                if (ret != 0) {
                    goto exit;
                }
                if (olen != pt_len_test_data[i]) {
                    goto exit;
                }
            }

            ret = mbedtls_gcm_finish(&ctx, NULL, 0, &olen, tag_buf, 16);
            if (ret != 0) {
                goto exit;
            }

            if (memcmp(buf, ct_test_data[j * 6 + i],
                       pt_len_test_data[i]) != 0 ||
                memcmp(tag_buf, tag_test_data[j * 6 + i], 16) != 0) {
                ret = 1;
                goto exit;
            }

            mbedtls_gcm_free(&ctx);

            if (verbose != 0) {
                mbedtls_printf("passed\n");
            }

            mbedtls_gcm_init(&ctx);

            if (verbose != 0) {
                mbedtls_printf("  AES-GCM-%3d #%d split (%s): ",
                               key_len, i, "dec");
            }

            ret = mbedtls_gcm_setkey(&ctx, cipher,
                                     key_test_data[key_index_test_data[i]],
                                     key_len);
            if (ret != 0) {
                goto exit;
            }

            ret = mbedtls_gcm_starts(&ctx, MBEDTLS_GCM_DECRYPT,
                                     iv_test_data[iv_index_test_data[i]],
                                     iv_len_test_data[i]);
            if (ret != 0) {
                goto exit;
            }
            ret = mbedtls_gcm_update_ad(&ctx,
                                        additional_test_data[add_index_test_data[i]],
                                        add_len_test_data[i]);
            if (ret != 0) {
                goto exit;
            }

            if (pt_len_test_data[i] > 32) {
                size_t rest_len = pt_len_test_data[i] - 32;
                ret = mbedtls_gcm_update(&ctx,
                                         ct_test_data[j * 6 + i], 32,
                                         buf, sizeof(buf), &olen);
                if (ret != 0) {
                    goto exit;
                }
                if (olen != 32) {
                    goto exit;
                }

                ret = mbedtls_gcm_update(&ctx,
                                         ct_test_data[j * 6 + i] + 32,
                                         rest_len,
                                         buf + 32, sizeof(buf) - 32, &olen);
                if (ret != 0) {
                    goto exit;
                }
                if (olen != rest_len) {
                    goto exit;
                }
            } else {
                ret = mbedtls_gcm_update(&ctx,
                                         ct_test_data[j * 6 + i],
                                         pt_len_test_data[i],
                                         buf, sizeof(buf), &olen);
                if (ret != 0) {
                    goto exit;
                }
                if (olen != pt_len_test_data[i]) {
                    goto exit;
                }
            }

            ret = mbedtls_gcm_finish(&ctx, NULL, 0, &olen, tag_buf, 16);
            if (ret != 0) {
                goto exit;
            }

            if (memcmp(buf, pt_test_data[pt_index_test_data[i]],
                       pt_len_test_data[i]) != 0 ||
                memcmp(tag_buf, tag_test_data[j * 6 + i], 16) != 0) {
                ret = 1;
                goto exit;
            }

            mbedtls_gcm_free(&ctx);

            if (verbose != 0) {
                mbedtls_printf("passed\n");
            }
        }
    }

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    ret = 0;

exit:
    if (ret != 0) {
        if (verbose != 0) {
            mbedtls_printf("failed\n");
        }
        mbedtls_gcm_free(&ctx);
    }

    return ret;
}

#endif /* MBEDTLS_SELF_TEST && MBEDTLS_AES_C */

#endif /* MBEDTLS_GCM_C */
