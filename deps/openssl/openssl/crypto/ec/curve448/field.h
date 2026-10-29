/*
 * Copyright 2017-2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright 2014 Cryptography Research, Inc.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 *
 * Originally written by Mike Hamburg
 */

#ifndef OSSL_CRYPTO_EC_CURVE448_FIELD_H
#define OSSL_CRYPTO_EC_CURVE448_FIELD_H

#include "internal/constant_time.h"
#include <string.h>
#include <assert.h>
#include "word.h"

#define NLIMBS (64 / sizeof(word_t))
#define X_SER_BYTES 56
#define SER_BYTES 56

#if defined(__GNUC__) || defined(__clang__)
#define INLINE_UNUSED __inline__ __attribute__((__unused__, __always_inline__))
#define RESTRICT __restrict__
#define ALIGNED __attribute__((__aligned__(16)))
#else
#define INLINE_UNUSED ossl_inline
#define RESTRICT
#define ALIGNED
#endif

typedef struct gf_s {
    word_t limb[NLIMBS];
} ALIGNED gf_s, gf[1];

/* RFC 7748 support */
#define X_PUBLIC_BYTES X_SER_BYTES
#define X_PRIVATE_BYTES X_PUBLIC_BYTES
#define X_PRIVATE_BITS 448

static INLINE_UNUSED void gf_copy(gf out, const gf a)
{
    *out = *a;
}

static INLINE_UNUSED void gf_add_RAW(gf out, const gf a, const gf b);
static INLINE_UNUSED void gf_sub_RAW(gf out, const gf a, const gf b);
static INLINE_UNUSED void gf_bias(gf inout, int amount);
static INLINE_UNUSED void gf_weak_reduce(gf inout);

void gf_strong_reduce(gf inout);
void gf_add(gf out, const gf a, const gf b);
void gf_sub(gf out, const gf a, const gf b);
void ossl_gf_mul(gf_s *RESTRICT out, const gf a, const gf b);
void ossl_gf_mulw_unsigned(gf_s *RESTRICT out, const gf a, uint32_t b);
void ossl_gf_sqr(gf_s *RESTRICT out, const gf a);
mask_t gf_isr(gf a, const gf x); /** a^2 x = 1, QNR, or 0 if x=0.  Return true if successful */
mask_t gf_eq(const gf x, const gf y);
mask_t gf_lobit(const gf x);
mask_t gf_hibit(const gf x);

void gf_serialize(uint8_t serial[SER_BYTES], const gf x, int with_highbit);
mask_t gf_deserialize(gf x, const uint8_t serial[SER_BYTES], int with_hibit,
    uint8_t hi_nmask);

/* clang-format off */
#define LIMBPERM(i) (i)
#if (ARCH_WORD_BITS == 32)
#define GF_HEADROOM 2
#define LIMB(x) ((x) & ((1 << 28) - 1)), ((x) >> 28)
#define FIELD_LITERAL(a, b, c, d, e, f, g, h)                                      \
    {                                                                              \
        {                                                                          \
            LIMB(a), LIMB(b), LIMB(c), LIMB(d), LIMB(e), LIMB(f), LIMB(g), LIMB(h) \
        }                                                                          \
    }

#define LIMB_PLACE_VALUE(i) 28

void gf_add_RAW(gf out, const gf a, const gf b)
{
    unsigned int i;

    for (i = 0; i < NLIMBS; i++)
        out->limb[i] = a->limb[i] + b->limb[i];
}

void gf_sub_RAW(gf out, const gf a, const gf b)
{
    unsigned int i;

    for (i = 0; i < NLIMBS; i++)
        out->limb[i] = a->limb[i] - b->limb[i];
}

void gf_bias(gf a, int amt)
{
    unsigned int i;
    uint32_t co1 = ((1 << 28) - 1) * amt, co2 = co1 - amt;

    for (i = 0; i < NLIMBS; i++)
        a->limb[i] += (i == NLIMBS / 2) ? co2 : co1;
}

void gf_weak_reduce(gf a)
{
    uint32_t mask = (1 << 28) - 1;
    uint32_t tmp = a->limb[NLIMBS - 1] >> 28;
    unsigned int i;

    a->limb[NLIMBS / 2] += tmp;
    for (i = NLIMBS - 1; i > 0; i--)
        a->limb[i] = (a->limb[i] & mask) + (a->limb[i - 1] >> 28);
    a->limb[0] = (a->limb[0] & mask) + tmp;
}
#define LIMB_MASK(i) (((1) << LIMB_PLACE_VALUE(i)) - 1)
#elif (ARCH_WORD_BITS == 64)
#define GF_HEADROOM 9999 /* Everything is reduced anyway */
#define FIELD_LITERAL(a, b, c, d, e, f, g, h) \
    {                                         \
        {                                     \
            a, b, c, d, e, f, g, h            \
        }                                     \
    }

#define LIMB_PLACE_VALUE(i) 56

void gf_add_RAW(gf out, const gf a, const gf b)
{
    unsigned int i;

    for (i = 0; i < NLIMBS; i++)
        out->limb[i] = a->limb[i] + b->limb[i];

    gf_weak_reduce(out);
}

void gf_sub_RAW(gf out, const gf a, const gf b)
{
    uint64_t co1 = ((1ULL << 56) - 1) * 2, co2 = co1 - 2;
    unsigned int i;

    for (i = 0; i < NLIMBS; i++)
        out->limb[i] = a->limb[i] - b->limb[i] + ((i == NLIMBS / 2) ? co2 : co1);

    gf_weak_reduce(out);
}

void gf_bias(gf a, int amt)
{
}

void gf_weak_reduce(gf a)
{
    uint64_t mask = (1ULL << 56) - 1;
    uint64_t tmp = a->limb[NLIMBS - 1] >> 56;
    unsigned int i;

    a->limb[NLIMBS / 2] += tmp;
    for (i = NLIMBS - 1; i > 0; i--)
        a->limb[i] = (a->limb[i] & mask) + (a->limb[i - 1] >> 56);
    a->limb[0] = (a->limb[0] & mask) + tmp;
}
#define LIMB_MASK(i) (((1ULL) << LIMB_PLACE_VALUE(i)) - 1)
#endif
/* clang-format on */

static const gf ZERO = { { { 0 } } }, ONE = { { { 1 } } };

/* Square x, n times. */
static ossl_inline void gf_sqrn(gf_s *RESTRICT y, const gf x, int n)
{
    gf tmp;

    assert(n > 0);
    if (n & 1) {
        ossl_gf_sqr(y, x);
        n--;
    } else {
        ossl_gf_sqr(tmp, x);
        ossl_gf_sqr(y, tmp);
        n -= 2;
    }
    for (; n; n -= 2) {
        ossl_gf_sqr(tmp, y);
        ossl_gf_sqr(y, tmp);
    }
}

#define gf_add_nr gf_add_RAW

/* Subtract mod p.  Bias by 2 and don't reduce  */
static ossl_inline void gf_sub_nr(gf c, const gf a, const gf b)
{
    gf_sub_RAW(c, a, b);
    gf_bias(c, 2);
    if (GF_HEADROOM < 3)
        gf_weak_reduce(c);
}

/* Subtract mod p. Bias by amt but don't reduce.  */
static ossl_inline void gf_subx_nr(gf c, const gf a, const gf b, int amt)
{
    gf_sub_RAW(c, a, b);
    gf_bias(c, amt);
    if (GF_HEADROOM < amt + 1)
        gf_weak_reduce(c);
}

/* Mul by signed int.  Not constant-time WRT the sign of that int. */
static ossl_inline void gf_mulw(gf c, const gf a, int32_t w)
{
    if (w > 0) {
        ossl_gf_mulw_unsigned(c, a, w);
    } else {
        ossl_gf_mulw_unsigned(c, a, -w);
        gf_sub(c, ZERO, c);
    }
}

/* Constant time, x = is_z ? z : y */
static ossl_inline void gf_cond_sel(gf x, const gf y, const gf z, mask_t is_z)
{
    size_t i;

    for (i = 0; i < NLIMBS; i++) {
#if ARCH_WORD_BITS == 32
        x[0].limb[i] = constant_time_select_32(is_z, z[0].limb[i],
            y[0].limb[i]);
#else
        /* Must be 64 bit */
        x[0].limb[i] = constant_time_select_64(is_z, z[0].limb[i],
            y[0].limb[i]);
#endif
    }
}

/* Constant time, if (neg) x=-x; */
static ossl_inline void gf_cond_neg(gf x, mask_t neg)
{
    gf y;

    gf_sub(y, ZERO, x);
    gf_cond_sel(x, x, y, neg);
}

/* Constant time, if (swap) (x,y) = (y,x); */
static ossl_inline void gf_cond_swap(gf x, gf_s *RESTRICT y, mask_t swap)
{
    size_t i;

    for (i = 0; i < NLIMBS; i++) {
#if ARCH_WORD_BITS == 32
        constant_time_cond_swap_32(swap, &(x[0].limb[i]), &(y->limb[i]));
#else
        /* Must be 64 bit */
        constant_time_cond_swap_64(swap, &(x[0].limb[i]), &(y->limb[i]));
#endif
    }
}

#endif /* OSSL_CRYPTO_EC_CURVE448_FIELD_H */
