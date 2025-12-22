/*
 *  RIPE MD-160 implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 *  The RIPEMD-160 algorithm was designed by RIPE in 1996
 *  http://homes.esat.kuleuven.be/~bosselae/mbedtls_ripemd160.html
 *  http://ehash.iaik.tugraz.at/wiki/RIPEMD-160
 */

#include "common.h"

#if defined(MBEDTLS_RIPEMD160_C)

#include "mbedtls/ripemd160.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include <string.h>

#include "mbedtls/platform.h"

#if !defined(MBEDTLS_RIPEMD160_ALT)

void mbedtls_ripemd160_init(mbedtls_ripemd160_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_ripemd160_context));
}

void mbedtls_ripemd160_free(mbedtls_ripemd160_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_ripemd160_context));
}

void mbedtls_ripemd160_clone(mbedtls_ripemd160_context *dst,
                             const mbedtls_ripemd160_context *src)
{
    *dst = *src;
}

/*
 * RIPEMD-160 context setup
 */
int mbedtls_ripemd160_starts(mbedtls_ripemd160_context *ctx)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;

    return 0;
}

#if !defined(MBEDTLS_RIPEMD160_PROCESS_ALT)
/*
 * Process one block
 */
int mbedtls_internal_ripemd160_process(mbedtls_ripemd160_context *ctx,
                                       const unsigned char data[64])
{
    struct {
        uint32_t A, B, C, D, E, Ap, Bp, Cp, Dp, Ep, X[16];
    } local;

    local.X[0] = MBEDTLS_GET_UINT32_LE(data,  0);
    local.X[1] = MBEDTLS_GET_UINT32_LE(data,  4);
    local.X[2] = MBEDTLS_GET_UINT32_LE(data,  8);
    local.X[3] = MBEDTLS_GET_UINT32_LE(data, 12);
    local.X[4] = MBEDTLS_GET_UINT32_LE(data, 16);
    local.X[5] = MBEDTLS_GET_UINT32_LE(data, 20);
    local.X[6] = MBEDTLS_GET_UINT32_LE(data, 24);
    local.X[7] = MBEDTLS_GET_UINT32_LE(data, 28);
    local.X[8] = MBEDTLS_GET_UINT32_LE(data, 32);
    local.X[9] = MBEDTLS_GET_UINT32_LE(data, 36);
    local.X[10] = MBEDTLS_GET_UINT32_LE(data, 40);
    local.X[11] = MBEDTLS_GET_UINT32_LE(data, 44);
    local.X[12] = MBEDTLS_GET_UINT32_LE(data, 48);
    local.X[13] = MBEDTLS_GET_UINT32_LE(data, 52);
    local.X[14] = MBEDTLS_GET_UINT32_LE(data, 56);
    local.X[15] = MBEDTLS_GET_UINT32_LE(data, 60);

    local.A = local.Ap = ctx->state[0];
    local.B = local.Bp = ctx->state[1];
    local.C = local.Cp = ctx->state[2];
    local.D = local.Dp = ctx->state[3];
    local.E = local.Ep = ctx->state[4];

#define F1(x, y, z)   ((x) ^ (y) ^ (z))
#define F2(x, y, z)   (((x) & (y)) | (~(x) & (z)))
#define F3(x, y, z)   (((x) | ~(y)) ^ (z))
#define F4(x, y, z)   (((x) & (z)) | ((y) & ~(z)))
#define F5(x, y, z)   ((x) ^ ((y) | ~(z)))

#define S(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define P(a, b, c, d, e, r, s, f, k)                      \
    do                                                      \
    {                                                       \
        (a) += f((b), (c), (d)) + local.X[r] + (k);       \
        (a) = S((a), (s)) + (e);                          \
        (c) = S((c), 10);                                 \
    } while (0)

#define P2(a, b, c, d, e, r, s, rp, sp)                               \
    do                                                                  \
    {                                                                   \
        P((a), (b), (c), (d), (e), (r), (s), F, K);                   \
        P(a ## p, b ## p, c ## p, d ## p, e ## p,                      \
          (rp), (sp), Fp, Kp);                                        \
    } while (0)

#define F   F1
#define K   0x00000000
#define Fp  F5
#define Kp  0x50A28BE6
    P2(local.A, local.B, local.C, local.D, local.E,  0, 11,  5,  8);
    P2(local.E, local.A, local.B, local.C, local.D,  1, 14, 14,  9);
    P2(local.D, local.E, local.A, local.B, local.C,  2, 15,  7,  9);
    P2(local.C, local.D, local.E, local.A, local.B,  3, 12,  0, 11);
    P2(local.B, local.C, local.D, local.E, local.A,  4,  5,  9, 13);
    P2(local.A, local.B, local.C, local.D, local.E,  5,  8,  2, 15);
    P2(local.E, local.A, local.B, local.C, local.D,  6,  7, 11, 15);
    P2(local.D, local.E, local.A, local.B, local.C,  7,  9,  4,  5);
    P2(local.C, local.D, local.E, local.A, local.B,  8, 11, 13,  7);
    P2(local.B, local.C, local.D, local.E, local.A,  9, 13,  6,  7);
    P2(local.A, local.B, local.C, local.D, local.E, 10, 14, 15,  8);
    P2(local.E, local.A, local.B, local.C, local.D, 11, 15,  8, 11);
    P2(local.D, local.E, local.A, local.B, local.C, 12,  6,  1, 14);
    P2(local.C, local.D, local.E, local.A, local.B, 13,  7, 10, 14);
    P2(local.B, local.C, local.D, local.E, local.A, 14,  9,  3, 12);
    P2(local.A, local.B, local.C, local.D, local.E, 15,  8, 12,  6);
#undef F
#undef K
#undef Fp
#undef Kp

#define F   F2
#define K   0x5A827999
#define Fp  F4
#define Kp  0x5C4DD124
    P2(local.E, local.A, local.B, local.C, local.D,  7,  7,  6,  9);
    P2(local.D, local.E, local.A, local.B, local.C,  4,  6, 11, 13);
    P2(local.C, local.D, local.E, local.A, local.B, 13,  8,  3, 15);
    P2(local.B, local.C, local.D, local.E, local.A,  1, 13,  7,  7);
    P2(local.A, local.B, local.C, local.D, local.E, 10, 11,  0, 12);
    P2(local.E, local.A, local.B, local.C, local.D,  6,  9, 13,  8);
    P2(local.D, local.E, local.A, local.B, local.C, 15,  7,  5,  9);
    P2(local.C, local.D, local.E, local.A, local.B,  3, 15, 10, 11);
    P2(local.B, local.C, local.D, local.E, local.A, 12,  7, 14,  7);
    P2(local.A, local.B, local.C, local.D, local.E,  0, 12, 15,  7);
    P2(local.E, local.A, local.B, local.C, local.D,  9, 15,  8, 12);
    P2(local.D, local.E, local.A, local.B, local.C,  5,  9, 12,  7);
    P2(local.C, local.D, local.E, local.A, local.B,  2, 11,  4,  6);
    P2(local.B, local.C, local.D, local.E, local.A, 14,  7,  9, 15);
    P2(local.A, local.B, local.C, local.D, local.E, 11, 13,  1, 13);
    P2(local.E, local.A, local.B, local.C, local.D,  8, 12,  2, 11);
#undef F
#undef K
#undef Fp
#undef Kp

#define F   F3
#define K   0x6ED9EBA1
#define Fp  F3
#define Kp  0x6D703EF3
    P2(local.D, local.E, local.A, local.B, local.C,  3, 11, 15,  9);
    P2(local.C, local.D, local.E, local.A, local.B, 10, 13,  5,  7);
    P2(local.B, local.C, local.D, local.E, local.A, 14,  6,  1, 15);
    P2(local.A, local.B, local.C, local.D, local.E,  4,  7,  3, 11);
    P2(local.E, local.A, local.B, local.C, local.D,  9, 14,  7,  8);
    P2(local.D, local.E, local.A, local.B, local.C, 15,  9, 14,  6);
    P2(local.C, local.D, local.E, local.A, local.B,  8, 13,  6,  6);
    P2(local.B, local.C, local.D, local.E, local.A,  1, 15,  9, 14);
    P2(local.A, local.B, local.C, local.D, local.E,  2, 14, 11, 12);
    P2(local.E, local.A, local.B, local.C, local.D,  7,  8,  8, 13);
    P2(local.D, local.E, local.A, local.B, local.C,  0, 13, 12,  5);
    P2(local.C, local.D, local.E, local.A, local.B,  6,  6,  2, 14);
    P2(local.B, local.C, local.D, local.E, local.A, 13,  5, 10, 13);
    P2(local.A, local.B, local.C, local.D, local.E, 11, 12,  0, 13);
    P2(local.E, local.A, local.B, local.C, local.D,  5,  7,  4,  7);
    P2(local.D, local.E, local.A, local.B, local.C, 12,  5, 13,  5);
#undef F
#undef K
#undef Fp
#undef Kp

#define F   F4
#define K   0x8F1BBCDC
#define Fp  F2
#define Kp  0x7A6D76E9
    P2(local.C, local.D, local.E, local.A, local.B,  1, 11,  8, 15);
    P2(local.B, local.C, local.D, local.E, local.A,  9, 12,  6,  5);
    P2(local.A, local.B, local.C, local.D, local.E, 11, 14,  4,  8);
    P2(local.E, local.A, local.B, local.C, local.D, 10, 15,  1, 11);
    P2(local.D, local.E, local.A, local.B, local.C,  0, 14,  3, 14);
    P2(local.C, local.D, local.E, local.A, local.B,  8, 15, 11, 14);
    P2(local.B, local.C, local.D, local.E, local.A, 12,  9, 15,  6);
    P2(local.A, local.B, local.C, local.D, local.E,  4,  8,  0, 14);
    P2(local.E, local.A, local.B, local.C, local.D, 13,  9,  5,  6);
    P2(local.D, local.E, local.A, local.B, local.C,  3, 14, 12,  9);
    P2(local.C, local.D, local.E, local.A, local.B,  7,  5,  2, 12);
    P2(local.B, local.C, local.D, local.E, local.A, 15,  6, 13,  9);
    P2(local.A, local.B, local.C, local.D, local.E, 14,  8,  9, 12);
    P2(local.E, local.A, local.B, local.C, local.D,  5,  6,  7,  5);
    P2(local.D, local.E, local.A, local.B, local.C,  6,  5, 10, 15);
    P2(local.C, local.D, local.E, local.A, local.B,  2, 12, 14,  8);
#undef F
#undef K
#undef Fp
#undef Kp

#define F   F5
#define K   0xA953FD4E
#define Fp  F1
#define Kp  0x00000000
    P2(local.B, local.C, local.D, local.E, local.A,  4,  9, 12,  8);
    P2(local.A, local.B, local.C, local.D, local.E,  0, 15, 15,  5);
    P2(local.E, local.A, local.B, local.C, local.D,  5,  5, 10, 12);
    P2(local.D, local.E, local.A, local.B, local.C,  9, 11,  4,  9);
    P2(local.C, local.D, local.E, local.A, local.B,  7,  6,  1, 12);
    P2(local.B, local.C, local.D, local.E, local.A, 12,  8,  5,  5);
    P2(local.A, local.B, local.C, local.D, local.E,  2, 13,  8, 14);
    P2(local.E, local.A, local.B, local.C, local.D, 10, 12,  7,  6);
    P2(local.D, local.E, local.A, local.B, local.C, 14,  5,  6,  8);
    P2(local.C, local.D, local.E, local.A, local.B,  1, 12,  2, 13);
    P2(local.B, local.C, local.D, local.E, local.A,  3, 13, 13,  6);
    P2(local.A, local.B, local.C, local.D, local.E,  8, 14, 14,  5);
    P2(local.E, local.A, local.B, local.C, local.D, 11, 11,  0, 15);
    P2(local.D, local.E, local.A, local.B, local.C,  6,  8,  3, 13);
    P2(local.C, local.D, local.E, local.A, local.B, 15,  5,  9, 11);
    P2(local.B, local.C, local.D, local.E, local.A, 13,  6, 11, 11);
#undef F
#undef K
#undef Fp
#undef Kp

    local.C       = ctx->state[1] + local.C + local.Dp;
    ctx->state[1] = ctx->state[2] + local.D + local.Ep;
    ctx->state[2] = ctx->state[3] + local.E + local.Ap;
    ctx->state[3] = ctx->state[4] + local.A + local.Bp;
    ctx->state[4] = ctx->state[0] + local.B + local.Cp;
    ctx->state[0] = local.C;

    /* Zeroise variables to clear sensitive data from memory. */
    mbedtls_platform_zeroize(&local, sizeof(local));

    return 0;
}

#endif /* !MBEDTLS_RIPEMD160_PROCESS_ALT */

/*
 * RIPEMD-160 process buffer
 */
int mbedtls_ripemd160_update(mbedtls_ripemd160_context *ctx,
                             const unsigned char *input,
                             size_t ilen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t fill;
    uint32_t left;

    if (ilen == 0) {
        return 0;
    }

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += (uint32_t) ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (uint32_t) ilen) {
        ctx->total[1]++;
    }

    if (left && ilen >= fill) {
        memcpy((void *) (ctx->buffer + left), input, fill);

        if ((ret = mbedtls_internal_ripemd160_process(ctx, ctx->buffer)) != 0) {
            return ret;
        }

        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while (ilen >= 64) {
        if ((ret = mbedtls_internal_ripemd160_process(ctx, input)) != 0) {
            return ret;
        }

        input += 64;
        ilen  -= 64;
    }

    if (ilen > 0) {
        memcpy((void *) (ctx->buffer + left), input, ilen);
    }

    return 0;
}

static const unsigned char ripemd160_padding[64] =
{
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * RIPEMD-160 final digest
 */
int mbedtls_ripemd160_finish(mbedtls_ripemd160_context *ctx,
                             unsigned char output[20])
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    uint32_t last, padn;
    uint32_t high, low;
    unsigned char msglen[8];

    high = (ctx->total[0] >> 29)
           | (ctx->total[1] <<  3);
    low  = (ctx->total[0] <<  3);

    MBEDTLS_PUT_UINT32_LE(low,  msglen, 0);
    MBEDTLS_PUT_UINT32_LE(high, msglen, 4);

    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    ret = mbedtls_ripemd160_update(ctx, ripemd160_padding, padn);
    if (ret != 0) {
        goto exit;
    }

    ret = mbedtls_ripemd160_update(ctx, msglen, 8);
    if (ret != 0) {
        goto exit;
    }

    MBEDTLS_PUT_UINT32_LE(ctx->state[0], output,  0);
    MBEDTLS_PUT_UINT32_LE(ctx->state[1], output,  4);
    MBEDTLS_PUT_UINT32_LE(ctx->state[2], output,  8);
    MBEDTLS_PUT_UINT32_LE(ctx->state[3], output, 12);
    MBEDTLS_PUT_UINT32_LE(ctx->state[4], output, 16);

    ret = 0;

exit:
    mbedtls_ripemd160_free(ctx);
    return ret;
}

#endif /* ! MBEDTLS_RIPEMD160_ALT */

/*
 * output = RIPEMD-160( input buffer )
 */
int mbedtls_ripemd160(const unsigned char *input,
                      size_t ilen,
                      unsigned char output[20])
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ripemd160_context ctx;

    mbedtls_ripemd160_init(&ctx);

    if ((ret = mbedtls_ripemd160_starts(&ctx)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_ripemd160_update(&ctx, input, ilen)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_ripemd160_finish(&ctx, output)) != 0) {
        goto exit;
    }

exit:
    mbedtls_ripemd160_free(&ctx);

    return ret;
}

#if defined(MBEDTLS_SELF_TEST)
/*
 * Test vectors from the RIPEMD-160 paper and
 * http://homes.esat.kuleuven.be/~bosselae/mbedtls_ripemd160.html#HMAC
 */
#define TESTS   8
static const unsigned char ripemd160_test_str[TESTS][81] =
{
    { "" },
    { "a" },
    { "abc" },
    { "message digest" },
    { "abcdefghijklmnopqrstuvwxyz" },
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" },
    { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" },
    { "12345678901234567890123456789012345678901234567890123456789012345678901234567890" },
};

static const size_t ripemd160_test_strlen[TESTS] =
{
    0, 1, 3, 14, 26, 56, 62, 80
};

static const unsigned char ripemd160_test_md[TESTS][20] =
{
    { 0x9c, 0x11, 0x85, 0xa5, 0xc5, 0xe9, 0xfc, 0x54, 0x61, 0x28,
      0x08, 0x97, 0x7e, 0xe8, 0xf5, 0x48, 0xb2, 0x25, 0x8d, 0x31 },
    { 0x0b, 0xdc, 0x9d, 0x2d, 0x25, 0x6b, 0x3e, 0xe9, 0xda, 0xae,
      0x34, 0x7b, 0xe6, 0xf4, 0xdc, 0x83, 0x5a, 0x46, 0x7f, 0xfe },
    { 0x8e, 0xb2, 0x08, 0xf7, 0xe0, 0x5d, 0x98, 0x7a, 0x9b, 0x04,
      0x4a, 0x8e, 0x98, 0xc6, 0xb0, 0x87, 0xf1, 0x5a, 0x0b, 0xfc },
    { 0x5d, 0x06, 0x89, 0xef, 0x49, 0xd2, 0xfa, 0xe5, 0x72, 0xb8,
      0x81, 0xb1, 0x23, 0xa8, 0x5f, 0xfa, 0x21, 0x59, 0x5f, 0x36 },
    { 0xf7, 0x1c, 0x27, 0x10, 0x9c, 0x69, 0x2c, 0x1b, 0x56, 0xbb,
      0xdc, 0xeb, 0x5b, 0x9d, 0x28, 0x65, 0xb3, 0x70, 0x8d, 0xbc },
    { 0x12, 0xa0, 0x53, 0x38, 0x4a, 0x9c, 0x0c, 0x88, 0xe4, 0x05,
      0xa0, 0x6c, 0x27, 0xdc, 0xf4, 0x9a, 0xda, 0x62, 0xeb, 0x2b },
    { 0xb0, 0xe2, 0x0b, 0x6e, 0x31, 0x16, 0x64, 0x02, 0x86, 0xed,
      0x3a, 0x87, 0xa5, 0x71, 0x30, 0x79, 0xb2, 0x1f, 0x51, 0x89 },
    { 0x9b, 0x75, 0x2e, 0x45, 0x57, 0x3d, 0x4b, 0x39, 0xf4, 0xdb,
      0xd3, 0x32, 0x3c, 0xab, 0x82, 0xbf, 0x63, 0x32, 0x6b, 0xfb },
};

/*
 * Checkup routine
 */
int mbedtls_ripemd160_self_test(int verbose)
{
    int i, ret = 0;
    unsigned char output[20];

    memset(output, 0, sizeof(output));

    for (i = 0; i < TESTS; i++) {
        if (verbose != 0) {
            mbedtls_printf("  RIPEMD-160 test #%d: ", i + 1);
        }

        ret = mbedtls_ripemd160(ripemd160_test_str[i],
                                ripemd160_test_strlen[i], output);
        if (ret != 0) {
            goto fail;
        }

        if (memcmp(output, ripemd160_test_md[i], 20) != 0) {
            ret = 1;
            goto fail;
        }

        if (verbose != 0) {
            mbedtls_printf("passed\n");
        }
    }

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    return 0;

fail:
    if (verbose != 0) {
        mbedtls_printf("failed\n");
    }

    return ret;
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_RIPEMD160_C */
