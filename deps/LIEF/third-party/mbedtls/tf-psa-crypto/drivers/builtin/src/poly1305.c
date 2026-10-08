/**
 * \file poly1305.c
 *
 * \brief Poly1305 authentication algorithm.
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_POLY1305_C)

#include "mbedtls/private/poly1305.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/private/error_common.h"

#include <string.h>

#include "mbedtls/platform.h"

#define POLY1305_BLOCK_SIZE_BYTES (16U)

/*
 * Our implementation is tuned for 32-bit platforms with a 64-bit multiplier.
 * However we provided an alternative for platforms without such a multiplier.
 */
#if defined(MBEDTLS_NO_64BIT_MULTIPLICATION)
static uint64_t mul64(uint32_t a, uint32_t b)
{
    /* a = al + 2**16 ah, b = bl + 2**16 bh */
    const uint16_t al = (uint16_t) a;
    const uint16_t bl = (uint16_t) b;
    const uint16_t ah = a >> 16;
    const uint16_t bh = b >> 16;

    /* ab = al*bl + 2**16 (ah*bl + bl*bh) + 2**32 ah*bh */
    const uint32_t lo = (uint32_t) al * bl;
    const uint64_t me = (uint64_t) ((uint32_t) ah * bl) + (uint32_t) al * bh;
    const uint32_t hi = (uint32_t) ah * bh;

    return lo + (me << 16) + ((uint64_t) hi << 32);
}
#else
static inline uint64_t mul64(uint32_t a, uint32_t b)
{
    return (uint64_t) a * b;
}
#endif


/**
 * \brief                   Process blocks with Poly1305.
 *
 * \param ctx               The Poly1305 context.
 * \param nblocks           Number of blocks to process. Note that this
 *                          function only processes full blocks.
 * \param input             Buffer containing the input block(s).
 * \param needs_padding     Set to 0 if the padding bit has already been
 *                          applied to the input data before calling this
 *                          function.  Otherwise, set this parameter to 1.
 */
static void poly1305_process(mbedtls_poly1305_context *ctx,
                             size_t nblocks,
                             const unsigned char *input,
                             uint32_t needs_padding)
{
    uint64_t d0, d1, d2, d3;
    uint32_t acc0, acc1, acc2, acc3, acc4;
    uint32_t r0, r1, r2, r3;
    uint32_t rs1, rs2, rs3;
    size_t offset  = 0U;
    size_t i;

    r0 = ctx->r[0];
    r1 = ctx->r[1];
    r2 = ctx->r[2];
    r3 = ctx->r[3];

    rs1 = r1 + (r1 >> 2U);
    rs2 = r2 + (r2 >> 2U);
    rs3 = r3 + (r3 >> 2U);

    acc0 = ctx->acc[0];
    acc1 = ctx->acc[1];
    acc2 = ctx->acc[2];
    acc3 = ctx->acc[3];
    acc4 = ctx->acc[4];

    /* Process full blocks */
    for (i = 0U; i < nblocks; i++) {
        /* The input block is treated as a 128-bit little-endian integer */
        d0   = MBEDTLS_GET_UINT32_LE(input, offset + 0);
        d1   = MBEDTLS_GET_UINT32_LE(input, offset + 4);
        d2   = MBEDTLS_GET_UINT32_LE(input, offset + 8);
        d3   = MBEDTLS_GET_UINT32_LE(input, offset + 12);

        /* Compute: acc += (padded) block as a 130-bit integer */
        d0  += (uint64_t) acc0;
        d1  += (uint64_t) acc1 + (d0 >> 32U);
        d2  += (uint64_t) acc2 + (d1 >> 32U);
        d3  += (uint64_t) acc3 + (d2 >> 32U);
        acc0 = (uint32_t) d0;
        acc1 = (uint32_t) d1;
        acc2 = (uint32_t) d2;
        acc3 = (uint32_t) d3;
        acc4 += (uint32_t) (d3 >> 32U) + needs_padding;

        /* Compute: acc *= r */
        d0 = mul64(acc0, r0) +
             mul64(acc1, rs3) +
             mul64(acc2, rs2) +
             mul64(acc3, rs1);
        d1 = mul64(acc0, r1) +
             mul64(acc1, r0) +
             mul64(acc2, rs3) +
             mul64(acc3, rs2) +
             mul64(acc4, rs1);
        d2 = mul64(acc0, r2) +
             mul64(acc1, r1) +
             mul64(acc2, r0) +
             mul64(acc3, rs3) +
             mul64(acc4, rs2);
        d3 = mul64(acc0, r3) +
             mul64(acc1, r2) +
             mul64(acc2, r1) +
             mul64(acc3, r0) +
             mul64(acc4, rs3);
        acc4 *= r0;

        /* Compute: acc %= (2^130 - 5) (partial remainder) */
        d1 += (d0 >> 32);
        d2 += (d1 >> 32);
        d3 += (d2 >> 32);
        acc0 = (uint32_t) d0;
        acc1 = (uint32_t) d1;
        acc2 = (uint32_t) d2;
        acc3 = (uint32_t) d3;
        acc4 = (uint32_t) (d3 >> 32) + acc4;

        d0 = (uint64_t) acc0 + (acc4 >> 2) + (acc4 & 0xFFFFFFFCU);
        acc4 &= 3U;
        acc0 = (uint32_t) d0;
        d0 = (uint64_t) acc1 + (d0 >> 32U);
        acc1 = (uint32_t) d0;
        d0 = (uint64_t) acc2 + (d0 >> 32U);
        acc2 = (uint32_t) d0;
        d0 = (uint64_t) acc3 + (d0 >> 32U);
        acc3 = (uint32_t) d0;
        d0 = (uint64_t) acc4 + (d0 >> 32U);
        acc4 = (uint32_t) d0;

        offset    += POLY1305_BLOCK_SIZE_BYTES;
    }

    ctx->acc[0] = acc0;
    ctx->acc[1] = acc1;
    ctx->acc[2] = acc2;
    ctx->acc[3] = acc3;
    ctx->acc[4] = acc4;
}

/**
 * \brief                   Compute the Poly1305 MAC
 *
 * \param ctx               The Poly1305 context.
 * \param mac               The buffer to where the MAC is written. Must be
 *                          big enough to contain the 16-byte MAC.
 */
static void poly1305_compute_mac(const mbedtls_poly1305_context *ctx,
                                 unsigned char mac[16])
{
    uint64_t d;
    uint32_t g0, g1, g2, g3, g4;
    uint32_t acc0, acc1, acc2, acc3, acc4;
    uint32_t mask;
    uint32_t mask_inv;

    acc0 = ctx->acc[0];
    acc1 = ctx->acc[1];
    acc2 = ctx->acc[2];
    acc3 = ctx->acc[3];
    acc4 = ctx->acc[4];

    /* Before adding 's' we ensure that the accumulator is mod 2^130 - 5.
     * We do this by calculating acc - (2^130 - 5), then checking if
     * the 131st bit is set. If it is, then reduce: acc -= (2^130 - 5)
     */

    /* Calculate acc + -(2^130 - 5) */
    d  = ((uint64_t) acc0 + 5U);
    g0 = (uint32_t) d;
    d  = ((uint64_t) acc1 + (d >> 32));
    g1 = (uint32_t) d;
    d  = ((uint64_t) acc2 + (d >> 32));
    g2 = (uint32_t) d;
    d  = ((uint64_t) acc3 + (d >> 32));
    g3 = (uint32_t) d;
    g4 = acc4 + (uint32_t) (d >> 32U);

    /* mask == 0xFFFFFFFF if 131st bit is set, otherwise mask == 0 */
    mask = (uint32_t) 0U - (g4 >> 2U);
    mask_inv = ~mask;

    /* If 131st bit is set then acc=g, otherwise, acc is unmodified */
    acc0 = (acc0 & mask_inv) | (g0 & mask);
    acc1 = (acc1 & mask_inv) | (g1 & mask);
    acc2 = (acc2 & mask_inv) | (g2 & mask);
    acc3 = (acc3 & mask_inv) | (g3 & mask);

    /* Add 's' */
    d = (uint64_t) acc0 + ctx->s[0];
    acc0 = (uint32_t) d;
    d = (uint64_t) acc1 + ctx->s[1] + (d >> 32U);
    acc1 = (uint32_t) d;
    d = (uint64_t) acc2 + ctx->s[2] + (d >> 32U);
    acc2 = (uint32_t) d;
    acc3 += ctx->s[3] + (uint32_t) (d >> 32U);

    /* Compute MAC (128 least significant bits of the accumulator) */
    MBEDTLS_PUT_UINT32_LE(acc0, mac,  0);
    MBEDTLS_PUT_UINT32_LE(acc1, mac,  4);
    MBEDTLS_PUT_UINT32_LE(acc2, mac,  8);
    MBEDTLS_PUT_UINT32_LE(acc3, mac, 12);
}

void mbedtls_poly1305_init(mbedtls_poly1305_context *ctx)
{
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_poly1305_context));
}

void mbedtls_poly1305_free(mbedtls_poly1305_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_poly1305_context));
}

int mbedtls_poly1305_starts(mbedtls_poly1305_context *ctx,
                            const unsigned char key[32])
{
    /* r &= 0x0ffffffc0ffffffc0ffffffc0fffffff */
    ctx->r[0] = MBEDTLS_GET_UINT32_LE(key, 0)  & 0x0FFFFFFFU;
    ctx->r[1] = MBEDTLS_GET_UINT32_LE(key, 4)  & 0x0FFFFFFCU;
    ctx->r[2] = MBEDTLS_GET_UINT32_LE(key, 8)  & 0x0FFFFFFCU;
    ctx->r[3] = MBEDTLS_GET_UINT32_LE(key, 12) & 0x0FFFFFFCU;

    ctx->s[0] = MBEDTLS_GET_UINT32_LE(key, 16);
    ctx->s[1] = MBEDTLS_GET_UINT32_LE(key, 20);
    ctx->s[2] = MBEDTLS_GET_UINT32_LE(key, 24);
    ctx->s[3] = MBEDTLS_GET_UINT32_LE(key, 28);

    /* Initial accumulator state */
    ctx->acc[0] = 0U;
    ctx->acc[1] = 0U;
    ctx->acc[2] = 0U;
    ctx->acc[3] = 0U;
    ctx->acc[4] = 0U;

    /* Queue initially empty */
    mbedtls_platform_zeroize(ctx->queue, sizeof(ctx->queue));
    ctx->queue_len = 0U;

    return 0;
}

int mbedtls_poly1305_update(mbedtls_poly1305_context *ctx,
                            const unsigned char *input,
                            size_t ilen)
{
    size_t offset    = 0U;
    size_t remaining = ilen;
    size_t queue_free_len;
    size_t nblocks;

    if ((remaining > 0U) && (ctx->queue_len > 0U)) {
        queue_free_len = (POLY1305_BLOCK_SIZE_BYTES - ctx->queue_len);

        if (ilen < queue_free_len) {
            /* Not enough data to complete the block.
             * Store this data with the other leftovers.
             */
            memcpy(&ctx->queue[ctx->queue_len],
                   input,
                   ilen);

            ctx->queue_len += ilen;

            remaining = 0U;
        } else {
            /* Enough data to produce a complete block */
            memcpy(&ctx->queue[ctx->queue_len],
                   input,
                   queue_free_len);

            ctx->queue_len = 0U;

            poly1305_process(ctx, 1U, ctx->queue, 1U);   /* add padding bit */

            offset    += queue_free_len;
            remaining -= queue_free_len;
        }
    }

    if (remaining >= POLY1305_BLOCK_SIZE_BYTES) {
        nblocks = remaining / POLY1305_BLOCK_SIZE_BYTES;

        poly1305_process(ctx, nblocks, &input[offset], 1U);

        offset += nblocks * POLY1305_BLOCK_SIZE_BYTES;
        remaining %= POLY1305_BLOCK_SIZE_BYTES;
    }

    if (remaining > 0U) {
        /* Store partial block */
        ctx->queue_len = remaining;
        memcpy(ctx->queue, &input[offset], remaining);
    }

    return 0;
}

int mbedtls_poly1305_finish(mbedtls_poly1305_context *ctx,
                            unsigned char mac[16])
{
    /* Process any leftover data */
    if (ctx->queue_len > 0U) {
        /* Add padding bit */
        ctx->queue[ctx->queue_len] = 1U;
        ctx->queue_len++;

        /* Pad with zeroes */
        memset(&ctx->queue[ctx->queue_len],
               0,
               POLY1305_BLOCK_SIZE_BYTES - ctx->queue_len);

        poly1305_process(ctx, 1U,           /* Process 1 block */
                         ctx->queue, 0U);   /* Already padded above */
    }

    poly1305_compute_mac(ctx, mac);

    return 0;
}

int mbedtls_poly1305_mac(const unsigned char key[32],
                         const unsigned char *input,
                         size_t ilen,
                         unsigned char mac[16])
{
    mbedtls_poly1305_context ctx;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    mbedtls_poly1305_init(&ctx);

    ret = mbedtls_poly1305_starts(&ctx, key);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_poly1305_update(&ctx, input, ilen);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_poly1305_finish(&ctx, mac);

cleanup:
    mbedtls_poly1305_free(&ctx);
    return ret;
}

#if defined(MBEDTLS_SELF_TEST)

static const unsigned char test_keys[2][32] =
{
    {
        0x85, 0xd6, 0xbe, 0x78, 0x57, 0x55, 0x6d, 0x33,
        0x7f, 0x44, 0x52, 0xfe, 0x42, 0xd5, 0x06, 0xa8,
        0x01, 0x03, 0x80, 0x8a, 0xfb, 0x0d, 0xb2, 0xfd,
        0x4a, 0xbf, 0xf6, 0xaf, 0x41, 0x49, 0xf5, 0x1b
    },
    {
        0x1c, 0x92, 0x40, 0xa5, 0xeb, 0x55, 0xd3, 0x8a,
        0xf3, 0x33, 0x88, 0x86, 0x04, 0xf6, 0xb5, 0xf0,
        0x47, 0x39, 0x17, 0xc1, 0x40, 0x2b, 0x80, 0x09,
        0x9d, 0xca, 0x5c, 0xbc, 0x20, 0x70, 0x75, 0xc0
    }
};

static const unsigned char test_data[2][127] =
{
    {
        0x43, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x67, 0x72,
        0x61, 0x70, 0x68, 0x69, 0x63, 0x20, 0x46, 0x6f,
        0x72, 0x75, 0x6d, 0x20, 0x52, 0x65, 0x73, 0x65,
        0x61, 0x72, 0x63, 0x68, 0x20, 0x47, 0x72, 0x6f,
        0x75, 0x70
    },
    {
        0x27, 0x54, 0x77, 0x61, 0x73, 0x20, 0x62, 0x72,
        0x69, 0x6c, 0x6c, 0x69, 0x67, 0x2c, 0x20, 0x61,
        0x6e, 0x64, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73,
        0x6c, 0x69, 0x74, 0x68, 0x79, 0x20, 0x74, 0x6f,
        0x76, 0x65, 0x73, 0x0a, 0x44, 0x69, 0x64, 0x20,
        0x67, 0x79, 0x72, 0x65, 0x20, 0x61, 0x6e, 0x64,
        0x20, 0x67, 0x69, 0x6d, 0x62, 0x6c, 0x65, 0x20,
        0x69, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x77,
        0x61, 0x62, 0x65, 0x3a, 0x0a, 0x41, 0x6c, 0x6c,
        0x20, 0x6d, 0x69, 0x6d, 0x73, 0x79, 0x20, 0x77,
        0x65, 0x72, 0x65, 0x20, 0x74, 0x68, 0x65, 0x20,
        0x62, 0x6f, 0x72, 0x6f, 0x67, 0x6f, 0x76, 0x65,
        0x73, 0x2c, 0x0a, 0x41, 0x6e, 0x64, 0x20, 0x74,
        0x68, 0x65, 0x20, 0x6d, 0x6f, 0x6d, 0x65, 0x20,
        0x72, 0x61, 0x74, 0x68, 0x73, 0x20, 0x6f, 0x75,
        0x74, 0x67, 0x72, 0x61, 0x62, 0x65, 0x2e
    }
};

static const size_t test_data_len[2] =
{
    34U,
    127U
};

static const unsigned char test_mac[2][16] =
{
    {
        0xa8, 0x06, 0x1d, 0xc1, 0x30, 0x51, 0x36, 0xc6,
        0xc2, 0x2b, 0x8b, 0xaf, 0x0c, 0x01, 0x27, 0xa9
    },
    {
        0x45, 0x41, 0x66, 0x9a, 0x7e, 0xaa, 0xee, 0x61,
        0xe7, 0x08, 0xdc, 0x7c, 0xbc, 0xc5, 0xeb, 0x62
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

int mbedtls_poly1305_self_test(int verbose)
{
    unsigned char mac[16];
    unsigned i;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    for (i = 0U; i < 2U; i++) {
        if (verbose != 0) {
            mbedtls_printf("  Poly1305 test %u ", i);
        }

        ret = mbedtls_poly1305_mac(test_keys[i],
                                   test_data[i],
                                   test_data_len[i],
                                   mac);
        ASSERT(0 == ret, ("error code: %i\n", ret));

        ASSERT(0 == memcmp(mac, test_mac[i], 16U), ("failed (mac)\n"));

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

#endif /* MBEDTLS_POLY1305_C */
