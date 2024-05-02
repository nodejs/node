/*
 * Copyright 2020-2023 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2020, Intel Corporation. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 *
 *
 * Originally written by Ilya Albrekht, Sergey Kirillov and Andrey Matyukov
 * Intel Corporation
 *
 */

#include <openssl/opensslconf.h>
#include <openssl/crypto.h>
#include "rsaz_exp.h"

#ifndef RSAZ_ENABLED
NON_EMPTY_TRANSLATION_UNIT
#else
# include <assert.h>
# include <string.h>

# if defined(__GNUC__)
#  define ALIGN64 __attribute__((aligned(64)))
# elif defined(_MSC_VER)
#  define ALIGN64 __declspec(align(64))
# else
#  define ALIGN64
# endif

# define ALIGN_OF(ptr, boundary) \
    ((unsigned char *)(ptr) + (boundary - (((size_t)(ptr)) & (boundary - 1))))

/* Internal radix */
# define DIGIT_SIZE (52)
/* 52-bit mask */
# define DIGIT_MASK ((uint64_t)0xFFFFFFFFFFFFF)

# define BITS2WORD8_SIZE(x)  (((x) + 7) >> 3)
# define BITS2WORD64_SIZE(x) (((x) + 63) >> 6)

static ossl_inline uint64_t get_digit52(const uint8_t *in, int in_len);
static ossl_inline void put_digit52(uint8_t *out, int out_len, uint64_t digit);
static void to_words52(BN_ULONG *out, int out_len, const BN_ULONG *in,
                       int in_bitsize);
static void from_words52(BN_ULONG *bn_out, int out_bitsize, const BN_ULONG *in);
static ossl_inline void set_bit(BN_ULONG *a, int idx);

/* Number of |digit_size|-bit digits in |bitsize|-bit value */
static ossl_inline int number_of_digits(int bitsize, int digit_size)
{
    return (bitsize + digit_size - 1) / digit_size;
}

typedef void (*AMM52)(BN_ULONG *res, const BN_ULONG *base,
                      const BN_ULONG *exp, const BN_ULONG *m, BN_ULONG k0);
typedef void (*EXP52_x2)(BN_ULONG *res, const BN_ULONG *base,
                         const BN_ULONG *exp[2], const BN_ULONG *m,
                         const BN_ULONG *rr, const BN_ULONG k0[2]);

/*
 * For details of the methods declared below please refer to
 *    crypto/bn/asm/rsaz-avx512.pl
 *
 * Naming notes:
 *  amm = Almost Montgomery Multiplication
 *  ams = Almost Montgomery Squaring
 *  52x20 - data represented as array of 20 digits in 52-bit radix
 *  _x1_/_x2_ - 1 or 2 independent inputs/outputs
 *  _256 suffix - uses 256-bit (AVX512VL) registers
 */

/*AMM = Almost Montgomery Multiplication. */
void ossl_rsaz_amm52x20_x1_256(BN_ULONG *res, const BN_ULONG *base,
                               const BN_ULONG *exp, const BN_ULONG *m,
                               BN_ULONG k0);
static void RSAZ_exp52x20_x2_256(BN_ULONG *res, const BN_ULONG *base,
                                 const BN_ULONG *exp[2], const BN_ULONG *m,
                                 const BN_ULONG *rr, const BN_ULONG k0[2]);
void ossl_rsaz_amm52x20_x2_256(BN_ULONG *out, const BN_ULONG *a,
                               const BN_ULONG *b, const BN_ULONG *m,
                               const BN_ULONG k0[2]);
void ossl_extract_multiplier_2x20_win5(BN_ULONG *red_Y,
                                       const BN_ULONG *red_table,
                                       int red_table_idx, int tbl_idx);

/*
 * Dual Montgomery modular exponentiation using prime moduli of the
 * same bit size, optimized with AVX512 ISA.
 *
 * Input and output parameters for each exponentiation are independent and
 * denoted here by index |i|, i = 1..2.
 *
 * Input and output are all in regular 2^64 radix.
 *
 * Each moduli shall be |factor_size| bit size.
 *
 * NOTE: currently only 2x1024 case is supported.
 *
 *  [out] res|i|      - result of modular exponentiation: array of qword values
 *                      in regular (2^64) radix. Size of array shall be enough
 *                      to hold |factor_size| bits.
 *  [in]  base|i|     - base
 *  [in]  exp|i|      - exponent
 *  [in]  m|i|        - moduli
 *  [in]  rr|i|       - Montgomery parameter RR = R^2 mod m|i|
 *  [in]  k0_|i|      - Montgomery parameter k0 = -1/m|i| mod 2^64
 *  [in]  factor_size - moduli bit size
 *
 * \return 0 in case of failure,
 *         1 in case of success.
 */
int ossl_rsaz_mod_exp_avx512_x2(BN_ULONG *res1,
                                const BN_ULONG *base1,
                                const BN_ULONG *exp1,
                                const BN_ULONG *m1,
                                const BN_ULONG *rr1,
                                BN_ULONG k0_1,
                                BN_ULONG *res2,
                                const BN_ULONG *base2,
                                const BN_ULONG *exp2,
                                const BN_ULONG *m2,
                                const BN_ULONG *rr2,
                                BN_ULONG k0_2,
                                int factor_size)
{
    int ret = 0;

    /*
     * Number of word-size (BN_ULONG) digits to store exponent in redundant
     * representation.
     */
    int exp_digits = number_of_digits(factor_size + 2, DIGIT_SIZE);
    int coeff_pow = 4 * (DIGIT_SIZE * exp_digits - factor_size);
    BN_ULONG *base1_red, *m1_red, *rr1_red;
    BN_ULONG *base2_red, *m2_red, *rr2_red;
    BN_ULONG *coeff_red;
    BN_ULONG *storage = NULL;
    BN_ULONG *storage_aligned = NULL;
    BN_ULONG storage_len_bytes = 7 * exp_digits * sizeof(BN_ULONG);

    /* AMM = Almost Montgomery Multiplication */
    AMM52 amm = NULL;
    /* Dual (2-exps in parallel) exponentiation */
    EXP52_x2 exp_x2 = NULL;

    const BN_ULONG *exp[2] = {0};
    BN_ULONG k0[2] = {0};

    /* Only 1024-bit factor size is supported now */
    switch (factor_size) {
    case 1024:
        amm = ossl_rsaz_amm52x20_x1_256;
        exp_x2 = RSAZ_exp52x20_x2_256;
        break;
    default:
        goto err;
    }

    storage = (BN_ULONG *)OPENSSL_malloc(storage_len_bytes + 64);
    if (storage == NULL)
        goto err;
    storage_aligned = (BN_ULONG *)ALIGN_OF(storage, 64);

    /* Memory layout for red(undant) representations */
    base1_red = storage_aligned;
    base2_red = storage_aligned + 1 * exp_digits;
    m1_red    = storage_aligned + 2 * exp_digits;
    m2_red    = storage_aligned + 3 * exp_digits;
    rr1_red   = storage_aligned + 4 * exp_digits;
    rr2_red   = storage_aligned + 5 * exp_digits;
    coeff_red = storage_aligned + 6 * exp_digits;

    /* Convert base_i, m_i, rr_i, from regular to 52-bit radix */
    to_words52(base1_red, exp_digits, base1, factor_size);
    to_words52(base2_red, exp_digits, base2, factor_size);
    to_words52(m1_red, exp_digits, m1, factor_size);
    to_words52(m2_red, exp_digits, m2, factor_size);
    to_words52(rr1_red, exp_digits, rr1, factor_size);
    to_words52(rr2_red, exp_digits, rr2, factor_size);

    /*
     * Compute target domain Montgomery converters RR' for each modulus
     * based on precomputed original domain's RR.
     *
     * RR -> RR' transformation steps:
     *  (1) coeff = 2^k
     *  (2) t = AMM(RR,RR) = RR^2 / R' mod m
     *  (3) RR' = AMM(t, coeff) = RR^2 * 2^k / R'^2 mod m
     * where
     *  k = 4 * (52 * digits52 - modlen)
     *  R  = 2^(64 * ceil(modlen/64)) mod m
     *  RR = R^2 mod M
     *  R' = 2^(52 * ceil(modlen/52)) mod m
     *
     *  modlen = 1024: k = 64, RR = 2^2048 mod m, RR' = 2^2080 mod m
     */
    memset(coeff_red, 0, exp_digits * sizeof(BN_ULONG));
    /* (1) in reduced domain representation */
    set_bit(coeff_red, 64 * (int)(coeff_pow / 52) + coeff_pow % 52);

    amm(rr1_red, rr1_red, rr1_red, m1_red, k0_1);     /* (2) for m1 */
    amm(rr1_red, rr1_red, coeff_red, m1_red, k0_1);   /* (3) for m1 */

    amm(rr2_red, rr2_red, rr2_red, m2_red, k0_2);     /* (2) for m2 */
    amm(rr2_red, rr2_red, coeff_red, m2_red, k0_2);   /* (3) for m2 */

    exp[0] = exp1;
    exp[1] = exp2;

    k0[0] = k0_1;
    k0[1] = k0_2;

    exp_x2(rr1_red, base1_red, exp, m1_red, rr1_red, k0);

    /* Convert rr_i back to regular radix */
    from_words52(res1, factor_size, rr1_red);
    from_words52(res2, factor_size, rr2_red);

    /* bn_reduce_once_in_place expects number of BN_ULONG, not bit size */
    factor_size /= sizeof(BN_ULONG) * 8;

    bn_reduce_once_in_place(res1, /*carry=*/0, m1, storage, factor_size);
    bn_reduce_once_in_place(res2, /*carry=*/0, m2, storage, factor_size);

    ret = 1;
err:
    if (storage != NULL) {
        OPENSSL_cleanse(storage, storage_len_bytes);
        OPENSSL_free(storage);
    }
    return ret;
}

/*
 * Dual 1024-bit w-ary modular exponentiation using prime moduli of the same
 * bit size using Almost Montgomery Multiplication, optimized with AVX512_IFMA
 * ISA.
 *
 * The parameter w (window size) = 5.
 *
 *  [out] res      - result of modular exponentiation: 2x20 qword
 *                   values in 2^52 radix.
 *  [in]  base     - base (2x20 qword values in 2^52 radix)
 *  [in]  exp      - array of 2 pointers to 16 qword values in 2^64 radix.
 *                   Exponent is not converted to redundant representation.
 *  [in]  m        - moduli (2x20 qword values in 2^52 radix)
 *  [in]  rr       - Montgomery parameter for 2 moduli: RR = 2^2080 mod m.
 *                   (2x20 qword values in 2^52 radix)
 *  [in]  k0       - Montgomery parameter for 2 moduli: k0 = -1/m mod 2^64
 *
 * \return (void).
 */
static void RSAZ_exp52x20_x2_256(BN_ULONG *out,          /* [2][20] */
                                 const BN_ULONG *base,   /* [2][20] */
                                 const BN_ULONG *exp[2], /* 2x16    */
                                 const BN_ULONG *m,      /* [2][20] */
                                 const BN_ULONG *rr,     /* [2][20] */
                                 const BN_ULONG k0[2])
{
# define BITSIZE_MODULUS (1024)
# define EXP_WIN_SIZE (5)
# define EXP_WIN_MASK ((1U << EXP_WIN_SIZE) - 1)
/*
 * Number of digits (64-bit words) in redundant representation to handle
 * modulus bits
 */
# define RED_DIGITS (20)
# define EXP_DIGITS (16)
# define DAMM ossl_rsaz_amm52x20_x2_256
/*
 * Squaring is done using multiplication now. That can be a subject of
 * optimization in future.
 */
# define DAMS(r,a,m,k0) \
              ossl_rsaz_amm52x20_x2_256((r),(a),(a),(m),(k0))

    /* Allocate stack for red(undant) result Y and multiplier X */
    ALIGN64 BN_ULONG red_Y[2][RED_DIGITS];
    ALIGN64 BN_ULONG red_X[2][RED_DIGITS];

    /* Allocate expanded exponent */
    ALIGN64 BN_ULONG expz[2][EXP_DIGITS + 1];

    /* Pre-computed table of base powers */
    ALIGN64 BN_ULONG red_table[1U << EXP_WIN_SIZE][2][RED_DIGITS];

    int idx;

    memset(red_Y, 0, sizeof(red_Y));
    memset(red_table, 0, sizeof(red_table));
    memset(red_X, 0, sizeof(red_X));

    /*
     * Compute table of powers base^i, i = 0, ..., (2^EXP_WIN_SIZE) - 1
     *   table[0] = mont(x^0) = mont(1)
     *   table[1] = mont(x^1) = mont(x)
     */
    red_X[0][0] = 1;
    red_X[1][0] = 1;
    DAMM(red_table[0][0], (const BN_ULONG*)red_X, rr, m, k0);
    DAMM(red_table[1][0], base,  rr, m, k0);

    for (idx = 1; idx < (int)((1U << EXP_WIN_SIZE) / 2); idx++) {
        DAMS(red_table[2 * idx + 0][0], red_table[1 * idx][0], m, k0);
        DAMM(red_table[2 * idx + 1][0], red_table[2 * idx][0], red_table[1][0], m, k0);
    }

    /* Copy and expand exponents */
    memcpy(expz[0], exp[0], EXP_DIGITS * sizeof(BN_ULONG));
    expz[0][EXP_DIGITS] = 0;
    memcpy(expz[1], exp[1], EXP_DIGITS * sizeof(BN_ULONG));
    expz[1][EXP_DIGITS] = 0;

    /* Exponentiation */
    {
        const int rem = BITSIZE_MODULUS % EXP_WIN_SIZE;
        BN_ULONG table_idx_mask = EXP_WIN_MASK;

        int exp_bit_no = BITSIZE_MODULUS - rem;
        int exp_chunk_no = exp_bit_no / 64;
        int exp_chunk_shift = exp_bit_no % 64;

        BN_ULONG red_table_idx_0, red_table_idx_1;

        /*
         * If rem == 0, then
         *      exp_bit_no = modulus_bitsize - exp_win_size
         * However, this isn't possible because rem is { 1024, 1536, 2048 } % 5
         * which is { 4, 1, 3 } respectively.
         *
         * If this assertion ever fails the fix above is easy.
         */
        OPENSSL_assert(rem != 0);

        /* Process 1-st exp window - just init result */
        red_table_idx_0 = expz[0][exp_chunk_no];
        red_table_idx_1 = expz[1][exp_chunk_no];
        /*
         * The function operates with fixed moduli sizes divisible by 64,
         * thus table index here is always in supported range [0, EXP_WIN_SIZE).
         */
        red_table_idx_0 >>= exp_chunk_shift;
        red_table_idx_1 >>= exp_chunk_shift;

        ossl_extract_multiplier_2x20_win5(red_Y[0], (const BN_ULONG*)red_table,
                                          (int)red_table_idx_0, 0);
        ossl_extract_multiplier_2x20_win5(red_Y[1], (const BN_ULONG*)red_table,
                                          (int)red_table_idx_1, 1);

        /* Process other exp windows */
        for (exp_bit_no -= EXP_WIN_SIZE; exp_bit_no >= 0; exp_bit_no -= EXP_WIN_SIZE) {
            /* Extract pre-computed multiplier from the table */
            {
                BN_ULONG T;

                exp_chunk_no = exp_bit_no / 64;
                exp_chunk_shift = exp_bit_no % 64;
                {
                    red_table_idx_0 = expz[0][exp_chunk_no];
                    T = expz[0][exp_chunk_no + 1];

                    red_table_idx_0 >>= exp_chunk_shift;
                    /*
                     * Get additional bits from then next quadword
                     * when 64-bit boundaries are crossed.
                     */
                    if (exp_chunk_shift > 64 - EXP_WIN_SIZE) {
                        T <<= (64 - exp_chunk_shift);
                        red_table_idx_0 ^= T;
                    }
                    red_table_idx_0 &= table_idx_mask;

                    ossl_extract_multiplier_2x20_win5(red_X[0],
                                                      (const BN_ULONG*)red_table,
                                                      (int)red_table_idx_0, 0);
                }
                {
                    red_table_idx_1 = expz[1][exp_chunk_no];
                    T = expz[1][exp_chunk_no + 1];

                    red_table_idx_1 >>= exp_chunk_shift;
                    /*
                     * Get additional bits from then next quadword
                     * when 64-bit boundaries are crossed.
                     */
                    if (exp_chunk_shift > 64 - EXP_WIN_SIZE) {
                        T <<= (64 - exp_chunk_shift);
                        red_table_idx_1 ^= T;
                    }
                    red_table_idx_1 &= table_idx_mask;

                    ossl_extract_multiplier_2x20_win5(red_X[1],
                                                      (const BN_ULONG*)red_table,
                                                      (int)red_table_idx_1, 1);
                }
            }

            /* Series of squaring */
            DAMS((BN_ULONG*)red_Y, (const BN_ULONG*)red_Y, m, k0);
            DAMS((BN_ULONG*)red_Y, (const BN_ULONG*)red_Y, m, k0);
            DAMS((BN_ULONG*)red_Y, (const BN_ULONG*)red_Y, m, k0);
            DAMS((BN_ULONG*)red_Y, (const BN_ULONG*)red_Y, m, k0);
            DAMS((BN_ULONG*)red_Y, (const BN_ULONG*)red_Y, m, k0);

            DAMM((BN_ULONG*)red_Y, (const BN_ULONG*)red_Y, (const BN_ULONG*)red_X, m, k0);
        }
    }

    /*
     *
     * NB: After the last AMM of exponentiation in Montgomery domain, the result
     * may be 1025-bit, but the conversion out of Montgomery domain performs an
     * AMM(x,1) which guarantees that the final result is less than |m|, so no
     * conditional subtraction is needed here. See "Efficient Software
     * Implementations of Modular Exponentiation" (by Shay Gueron) paper for details.
     */

    /* Convert result back in regular 2^52 domain */
    memset(red_X, 0, sizeof(red_X));
    red_X[0][0] = 1;
    red_X[1][0] = 1;
    DAMM(out, (const BN_ULONG*)red_Y, (const BN_ULONG*)red_X, m, k0);

    /* Clear exponents */
    OPENSSL_cleanse(expz, sizeof(expz));
    OPENSSL_cleanse(red_Y, sizeof(red_Y));

# undef DAMS
# undef DAMM
# undef EXP_DIGITS
# undef RED_DIGITS
# undef EXP_WIN_MASK
# undef EXP_WIN_SIZE
# undef BITSIZE_MODULUS
}

static ossl_inline uint64_t get_digit52(const uint8_t *in, int in_len)
{
    uint64_t digit = 0;

    assert(in != NULL);

    for (; in_len > 0; in_len--) {
        digit <<= 8;
        digit += (uint64_t)(in[in_len - 1]);
    }
    return digit;
}

/*
 * Convert array of words in regular (base=2^64) representation to array of
 * words in redundant (base=2^52) one.
 */
static void to_words52(BN_ULONG *out, int out_len,
                       const BN_ULONG *in, int in_bitsize)
{
    uint8_t *in_str = NULL;

    assert(out != NULL);
    assert(in != NULL);
    /* Check destination buffer capacity */
    assert(out_len >= number_of_digits(in_bitsize, DIGIT_SIZE));

    in_str = (uint8_t *)in;

    for (; in_bitsize >= (2 * DIGIT_SIZE); in_bitsize -= (2 * DIGIT_SIZE), out += 2) {
        uint64_t digit;

        memcpy(&digit, in_str, sizeof(digit));
        out[0] = digit & DIGIT_MASK;
        in_str += 6;
        memcpy(&digit, in_str, sizeof(digit));
        out[1] = (digit >> 4) & DIGIT_MASK;
        in_str += 7;
        out_len -= 2;
    }

    if (in_bitsize > DIGIT_SIZE) {
        uint64_t digit = get_digit52(in_str, 7);

        out[0] = digit & DIGIT_MASK;
        in_str += 6;
        in_bitsize -= DIGIT_SIZE;
        digit = get_digit52(in_str, BITS2WORD8_SIZE(in_bitsize));
        out[1] = digit >> 4;
        out += 2;
        out_len -= 2;
    } else if (in_bitsize > 0) {
        out[0] = get_digit52(in_str, BITS2WORD8_SIZE(in_bitsize));
        out++;
        out_len--;
    }

    while (out_len > 0) {
        *out = 0;
        out_len--;
        out++;
    }
}

static ossl_inline void put_digit52(uint8_t *pStr, int strLen, uint64_t digit)
{
    assert(pStr != NULL);

    for (; strLen > 0; strLen--) {
        *pStr++ = (uint8_t)(digit & 0xFF);
        digit >>= 8;
    }
}

/*
 * Convert array of words in redundant (base=2^52) representation to array of
 * words in regular (base=2^64) one.
 */
static void from_words52(BN_ULONG *out, int out_bitsize, const BN_ULONG *in)
{
    int i;
    int out_len = BITS2WORD64_SIZE(out_bitsize);

    assert(out != NULL);
    assert(in != NULL);

    for (i = 0; i < out_len; i++)
        out[i] = 0;

    {
        uint8_t *out_str = (uint8_t *)out;

        for (; out_bitsize >= (2 * DIGIT_SIZE);
               out_bitsize -= (2 * DIGIT_SIZE), in += 2) {
            uint64_t digit;

            digit = in[0];
            memcpy(out_str, &digit, sizeof(digit));
            out_str += 6;
            digit = digit >> 48 | in[1] << 4;
            memcpy(out_str, &digit, sizeof(digit));
            out_str += 7;
        }

        if (out_bitsize > DIGIT_SIZE) {
            put_digit52(out_str, 7, in[0]);
            out_str += 6;
            out_bitsize -= DIGIT_SIZE;
            put_digit52(out_str, BITS2WORD8_SIZE(out_bitsize),
                        (in[1] << 4 | in[0] >> 48));
        } else if (out_bitsize) {
            put_digit52(out_str, BITS2WORD8_SIZE(out_bitsize), in[0]);
        }
    }
}

/*
 * Set bit at index |idx| in the words array |a|.
 * It does not do any boundaries checks, make sure the index is valid before
 * calling the function.
 */
static ossl_inline void set_bit(BN_ULONG *a, int idx)
{
    assert(a != NULL);

    {
        int i, j;

        i = idx / BN_BITS2;
        j = idx % BN_BITS2;
        a[i] |= (((BN_ULONG)1) << j);
    }
}

#endif
