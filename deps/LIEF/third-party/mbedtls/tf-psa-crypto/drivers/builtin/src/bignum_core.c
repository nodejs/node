/*
 *  Core bignum functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_BIGNUM_C)

#include <string.h>

#include "mbedtls/private/error_common.h"
#include "mbedtls/platform_util.h"
#include "constant_time_internal.h"

#include "mbedtls/platform.h"

#include "bignum_core.h"
#include "bignum_core_invasive.h"
#include "bn_mul.h"
#include "constant_time_internal.h"

size_t mbedtls_mpi_core_clz(mbedtls_mpi_uint a)
{
#if defined(__has_builtin)
#if (MBEDTLS_MPI_UINT_MAX == UINT_MAX) && __has_builtin(__builtin_clz)
    #define core_clz __builtin_clz
#elif (MBEDTLS_MPI_UINT_MAX == ULONG_MAX) && __has_builtin(__builtin_clzl)
    #define core_clz __builtin_clzl
#elif (MBEDTLS_MPI_UINT_MAX == ULLONG_MAX) && __has_builtin(__builtin_clzll)
    #define core_clz __builtin_clzll
#endif
#endif
#if defined(core_clz)
    return (size_t) core_clz(a);
#else
    size_t j;
    mbedtls_mpi_uint mask = (mbedtls_mpi_uint) 1 << (biL - 1);

    for (j = 0; j < biL; j++) {
        if (a & mask) {
            break;
        }

        mask >>= 1;
    }

    return j;
#endif
}

size_t mbedtls_mpi_core_bitlen(const mbedtls_mpi_uint *A, size_t A_limbs)
{
    int i;
    size_t j;

    for (i = ((int) A_limbs) - 1; i >= 0; i--) {
        if (A[i] != 0) {
            j = biL - mbedtls_mpi_core_clz(A[i]);
            return (i * biL) + j;
        }
    }

    return 0;
}

static mbedtls_mpi_uint mpi_bigendian_to_host(mbedtls_mpi_uint a)
{
    if (MBEDTLS_IS_BIG_ENDIAN) {
        /* Nothing to do on bigendian systems. */
        return a;
    } else {
#if defined(MBEDTLS_HAVE_INT32)
        return (mbedtls_mpi_uint) MBEDTLS_BSWAP32(a);
#elif defined(MBEDTLS_HAVE_INT64)
        return (mbedtls_mpi_uint) MBEDTLS_BSWAP64(a);
#endif
    }
}

void mbedtls_mpi_core_bigendian_to_host(mbedtls_mpi_uint *A,
                                        size_t A_limbs)
{
    mbedtls_mpi_uint *cur_limb_left;
    mbedtls_mpi_uint *cur_limb_right;
    if (A_limbs == 0) {
        return;
    }

    /*
     * Traverse limbs and
     * - adapt byte-order in each limb
     * - swap the limbs themselves.
     * For that, simultaneously traverse the limbs from left to right
     * and from right to left, as long as the left index is not bigger
     * than the right index (it's not a problem if limbs is odd and the
     * indices coincide in the last iteration).
     */
    for (cur_limb_left = A, cur_limb_right = A + (A_limbs - 1);
         cur_limb_left <= cur_limb_right;
         cur_limb_left++, cur_limb_right--) {
        mbedtls_mpi_uint tmp;
        /* Note that if cur_limb_left == cur_limb_right,
         * this code effectively swaps the bytes only once. */
        tmp             = mpi_bigendian_to_host(*cur_limb_left);
        *cur_limb_left  = mpi_bigendian_to_host(*cur_limb_right);
        *cur_limb_right = tmp;
    }
}

/* Whether min <= A, in constant time.
 * A_limbs must be at least 1. */
mbedtls_ct_condition_t mbedtls_mpi_core_uint_le_mpi(mbedtls_mpi_uint min,
                                                    const mbedtls_mpi_uint *A,
                                                    size_t A_limbs)
{
    /* min <= least significant limb? */
    mbedtls_ct_condition_t min_le_lsl = mbedtls_ct_uint_ge(A[0], min);

    /* limbs other than the least significant one are all zero? */
    mbedtls_ct_condition_t msll_mask = MBEDTLS_CT_FALSE;
    for (size_t i = 1; i < A_limbs; i++) {
        msll_mask = mbedtls_ct_bool_or(msll_mask, mbedtls_ct_bool(A[i]));
    }

    /* min <= A iff the lowest limb of A is >= min or the other limbs
     * are not all zero. */
    return mbedtls_ct_bool_or(msll_mask, min_le_lsl);
}

mbedtls_ct_condition_t mbedtls_mpi_core_lt_ct(const mbedtls_mpi_uint *A,
                                              const mbedtls_mpi_uint *B,
                                              size_t limbs)
{
    mbedtls_ct_condition_t ret = MBEDTLS_CT_FALSE, cond = MBEDTLS_CT_FALSE, done = MBEDTLS_CT_FALSE;

    for (size_t i = limbs; i > 0; i--) {
        /*
         * If B[i - 1] < A[i - 1] then A < B is false and the result must
         * remain 0.
         *
         * Again even if we can make a decision, we just mark the result and
         * the fact that we are done and continue looping.
         */
        cond = mbedtls_ct_uint_lt(B[i - 1], A[i - 1]);
        done = mbedtls_ct_bool_or(done, cond);

        /*
         * If A[i - 1] < B[i - 1] then A < B is true.
         *
         * Again even if we can make a decision, we just mark the result and
         * the fact that we are done and continue looping.
         */
        cond = mbedtls_ct_uint_lt(A[i - 1], B[i - 1]);
        ret  = mbedtls_ct_bool_or(ret, mbedtls_ct_bool_and(cond, mbedtls_ct_bool_not(done)));
        done = mbedtls_ct_bool_or(done, cond);
    }

    /*
     * If all the limbs were equal, then the numbers are equal, A < B is false
     * and leaving the result 0 is correct.
     */

    return ret;
}

void mbedtls_mpi_core_cond_assign(mbedtls_mpi_uint *X,
                                  const mbedtls_mpi_uint *A,
                                  size_t limbs,
                                  mbedtls_ct_condition_t assign)
{
    if (X == A) {
        return;
    }

    /* This function is very performance-sensitive for RSA. For this reason
     * we have the loop below, instead of calling mbedtls_ct_memcpy_if
     * (this is more optimal since here we don't have to handle the case where
     * we copy awkwardly sized data).
     */
    for (size_t i = 0; i < limbs; i++) {
        X[i] = mbedtls_ct_mpi_uint_if(assign, A[i], X[i]);
    }
}

void mbedtls_mpi_core_cond_swap(mbedtls_mpi_uint *X,
                                mbedtls_mpi_uint *Y,
                                size_t limbs,
                                mbedtls_ct_condition_t swap)
{
    if (X == Y) {
        return;
    }

    for (size_t i = 0; i < limbs; i++) {
        mbedtls_mpi_uint tmp = X[i];
        X[i] = mbedtls_ct_mpi_uint_if(swap, Y[i], X[i]);
        Y[i] = mbedtls_ct_mpi_uint_if(swap, tmp, Y[i]);
    }
}

int mbedtls_mpi_core_read_le(mbedtls_mpi_uint *X,
                             size_t X_limbs,
                             const unsigned char *input,
                             size_t input_length)
{
    const size_t limbs = CHARS_TO_LIMBS(input_length);

    if (X_limbs < limbs) {
        return MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL;
    }

    if (X != NULL) {
        memset(X, 0, X_limbs * ciL);

        for (size_t i = 0; i < input_length; i++) {
            size_t offset = ((i % ciL) << 3);
            X[i / ciL] |= ((mbedtls_mpi_uint) input[i]) << offset;
        }
    }

    return 0;
}

int mbedtls_mpi_core_read_be(mbedtls_mpi_uint *X,
                             size_t X_limbs,
                             const unsigned char *input,
                             size_t input_length)
{
    const size_t limbs = CHARS_TO_LIMBS(input_length);

    if (X_limbs < limbs) {
        return MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL;
    }

    /* If X_limbs is 0, input_length must also be 0 (from previous test).
     * Nothing to do. */
    if (X_limbs == 0) {
        return 0;
    }

    memset(X, 0, X_limbs * ciL);

    /* memcpy() with (NULL, 0) is undefined behaviour */
    if (input_length != 0) {
        size_t overhead = (X_limbs * ciL) - input_length;
        unsigned char *Xp = (unsigned char *) X;
        memcpy(Xp + overhead, input, input_length);
    }

    mbedtls_mpi_core_bigendian_to_host(X, X_limbs);

    return 0;
}

int mbedtls_mpi_core_write_le(const mbedtls_mpi_uint *A,
                              size_t A_limbs,
                              unsigned char *output,
                              size_t output_length)
{
    size_t stored_bytes = A_limbs * ciL;
    size_t bytes_to_copy;

    if (stored_bytes < output_length) {
        bytes_to_copy = stored_bytes;
    } else {
        bytes_to_copy = output_length;

        /* The output buffer is smaller than the allocated size of A.
         * However A may fit if its leading bytes are zero. */
        for (size_t i = bytes_to_copy; i < stored_bytes; i++) {
            if (GET_BYTE(A, i) != 0) {
                return MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL;
            }
        }
    }

    for (size_t i = 0; i < bytes_to_copy; i++) {
        output[i] = GET_BYTE(A, i);
    }

    if (stored_bytes < output_length) {
        /* Write trailing 0 bytes */
        memset(output + stored_bytes, 0, output_length - stored_bytes);
    }

    return 0;
}

int mbedtls_mpi_core_write_be(const mbedtls_mpi_uint *X,
                              size_t X_limbs,
                              unsigned char *output,
                              size_t output_length)
{
    size_t stored_bytes;
    size_t bytes_to_copy;
    unsigned char *p;

    stored_bytes = X_limbs * ciL;

    if (stored_bytes < output_length) {
        /* There is enough space in the output buffer. Write initial
         * null bytes and record the position at which to start
         * writing the significant bytes. In this case, the execution
         * trace of this function does not depend on the value of the
         * number. */
        bytes_to_copy = stored_bytes;
        p = output + output_length - stored_bytes;
        memset(output, 0, output_length - stored_bytes);
    } else {
        /* The output buffer is smaller than the allocated size of X.
         * However X may fit if its leading bytes are zero. */
        bytes_to_copy = output_length;
        p = output;
        for (size_t i = bytes_to_copy; i < stored_bytes; i++) {
            if (GET_BYTE(X, i) != 0) {
                return MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL;
            }
        }
    }

    for (size_t i = 0; i < bytes_to_copy; i++) {
        p[bytes_to_copy - i - 1] = GET_BYTE(X, i);
    }

    return 0;
}

void mbedtls_mpi_core_shift_r(mbedtls_mpi_uint *X, size_t limbs,
                              size_t count)
{
    size_t i, v0, v1;
    mbedtls_mpi_uint r0 = 0, r1;

    v0 = count /  biL;
    v1 = count & (biL - 1);

    if (v0 > limbs || (v0 == limbs && v1 > 0)) {
        memset(X, 0, limbs * ciL);
        return;
    }

    /*
     * shift by count / limb_size
     */
    if (v0 > 0) {
        for (i = 0; i < limbs - v0; i++) {
            X[i] = X[i + v0];
        }

        for (; i < limbs; i++) {
            X[i] = 0;
        }
    }

    /*
     * shift by count % limb_size
     */
    if (v1 > 0) {
        for (i = limbs; i > 0; i--) {
            r1 = X[i - 1] << (biL - v1);
            X[i - 1] >>= v1;
            X[i - 1] |= r0;
            r0 = r1;
        }
    }
}

void mbedtls_mpi_core_shift_l(mbedtls_mpi_uint *X, size_t limbs,
                              size_t count)
{
    size_t i, v0, v1;
    mbedtls_mpi_uint r0 = 0, r1;

    v0 = count / (biL);
    v1 = count & (biL - 1);

    /*
     * shift by count / limb_size
     */
    if (v0 > 0) {
        for (i = limbs; i > v0; i--) {
            X[i - 1] = X[i - v0 - 1];
        }

        for (; i > 0; i--) {
            X[i - 1] = 0;
        }
    }

    /*
     * shift by count % limb_size
     */
    if (v1 > 0) {
        for (i = v0; i < limbs; i++) {
            r1 = X[i] >> (biL - v1);
            X[i] <<= v1;
            X[i] |= r0;
            r0 = r1;
        }
    }
}

mbedtls_mpi_uint mbedtls_mpi_core_add(mbedtls_mpi_uint *X,
                                      const mbedtls_mpi_uint *A,
                                      const mbedtls_mpi_uint *B,
                                      size_t limbs)
{
    mbedtls_mpi_uint c = 0;

    for (size_t i = 0; i < limbs; i++) {
        mbedtls_mpi_uint t = c + A[i];
        c = (t < A[i]);
        t += B[i];
        c += (t < B[i]);
        X[i] = t;
    }

    return c;
}

mbedtls_mpi_uint mbedtls_mpi_core_add_if(mbedtls_mpi_uint *X,
                                         const mbedtls_mpi_uint *A,
                                         size_t limbs,
                                         unsigned cond)
{
    mbedtls_mpi_uint c = 0;

    mbedtls_ct_condition_t do_add = mbedtls_ct_bool(cond);

    for (size_t i = 0; i < limbs; i++) {
        mbedtls_mpi_uint add = mbedtls_ct_mpi_uint_if_else_0(do_add, A[i]);
        mbedtls_mpi_uint t = c + X[i];
        c = (t < X[i]);
        t += add;
        c += (t < add);
        X[i] = t;
    }

    return c;
}

mbedtls_mpi_uint mbedtls_mpi_core_sub(mbedtls_mpi_uint *X,
                                      const mbedtls_mpi_uint *A,
                                      const mbedtls_mpi_uint *B,
                                      size_t limbs)
{
    mbedtls_mpi_uint c = 0;

    for (size_t i = 0; i < limbs; i++) {
        mbedtls_mpi_uint z = mbedtls_ct_mpi_uint_if(mbedtls_ct_uint_lt(A[i], c),
                                                    1, 0);
        mbedtls_mpi_uint t = A[i] - c;
        c = mbedtls_ct_mpi_uint_if(mbedtls_ct_uint_lt(t, B[i]), 1, 0) + z;
        X[i] = t - B[i];
    }

    return c;
}

mbedtls_mpi_uint mbedtls_mpi_core_mla(mbedtls_mpi_uint *d, size_t d_len,
                                      const mbedtls_mpi_uint *s, size_t s_len,
                                      mbedtls_mpi_uint b)
{
    mbedtls_mpi_uint c = 0; /* carry */
    /*
     * It is a documented precondition of this function that d_len >= s_len.
     * If that's not the case, we swap these round: this turns what would be
     * a buffer overflow into an incorrect result.
     */
    if (d_len < s_len) {
        s_len = d_len;
    }
    size_t excess_len = d_len - s_len;
    size_t steps_x8 = s_len / 8;
    size_t steps_x1 = s_len & 7;

    while (steps_x8--) {
        MULADDC_X8_INIT
        MULADDC_X8_CORE
            MULADDC_X8_STOP
    }

    while (steps_x1--) {
        MULADDC_X1_INIT
        MULADDC_X1_CORE
            MULADDC_X1_STOP
    }

    while (excess_len--) {
        *d += c;
        c = mbedtls_ct_mpi_uint_if(mbedtls_ct_uint_lt(*d, c), 1, 0);
        d++;
    }

    return c;
}

void mbedtls_mpi_core_mul(mbedtls_mpi_uint *X,
                          const mbedtls_mpi_uint *A, size_t A_limbs,
                          const mbedtls_mpi_uint *B, size_t B_limbs)
{
    memset(X, 0, (A_limbs + B_limbs) * ciL);

    for (size_t i = 0; i < B_limbs; i++) {
        (void) mbedtls_mpi_core_mla(X + i, A_limbs + 1, A, A_limbs, B[i]);
    }
}

/*
 * Fast Montgomery initialization (thanks to Tom St Denis).
 */
mbedtls_mpi_uint mbedtls_mpi_core_montmul_init(const mbedtls_mpi_uint *N)
{
    mbedtls_mpi_uint x = N[0];

    x += ((N[0] + 2) & 4) << 1;

    for (unsigned int i = biL; i >= 8; i /= 2) {
        x *= (2 - (N[0] * x));
    }

    return ~x + 1;
}

void mbedtls_mpi_core_montmul(mbedtls_mpi_uint *X,
                              const mbedtls_mpi_uint *A,
                              const mbedtls_mpi_uint *B,
                              size_t B_limbs,
                              const mbedtls_mpi_uint *N,
                              size_t AN_limbs,
                              mbedtls_mpi_uint mm,
                              mbedtls_mpi_uint *T)
{
    memset(T, 0, (2 * AN_limbs + 1) * ciL);

    for (size_t i = 0; i < AN_limbs; i++) {
        /* T = (T + u0*B + u1*N) / 2^biL */
        mbedtls_mpi_uint u0 = A[i];
        mbedtls_mpi_uint u1 = (T[0] + u0 * B[0]) * mm;

        (void) mbedtls_mpi_core_mla(T, AN_limbs + 2, B, B_limbs, u0);
        (void) mbedtls_mpi_core_mla(T, AN_limbs + 2, N, AN_limbs, u1);

        T++;
    }

    /*
     * The result we want is (T >= N) ? T - N : T.
     *
     * For better constant-time properties in this function, we always do the
     * subtraction, with the result in X.
     *
     * We also look to see if there was any carry in the final additions in the
     * loop above.
     */

    mbedtls_mpi_uint carry  = T[AN_limbs];
    mbedtls_mpi_uint borrow = mbedtls_mpi_core_sub(X, T, N, AN_limbs);

    /*
     * Using R as the Montgomery radix (auxiliary modulus) i.e. 2^(biL*AN_limbs):
     *
     * T can be in one of 3 ranges:
     *
     * 1) T < N      : (carry, borrow) = (0, 1): we want T
     * 2) N <= T < R : (carry, borrow) = (0, 0): we want X
     * 3) T >= R     : (carry, borrow) = (1, 1): we want X
     *
     * and (carry, borrow) = (1, 0) can't happen.
     *
     * So the correct return value is already in X if (carry ^ borrow) = 0,
     * but is in (the lower AN_limbs limbs of) T if (carry ^ borrow) = 1.
     */
    mbedtls_ct_memcpy_if(mbedtls_ct_bool(carry ^ borrow),
                         (unsigned char *) X,
                         (unsigned char *) T,
                         NULL,
                         AN_limbs * sizeof(mbedtls_mpi_uint));
}

int mbedtls_mpi_core_get_mont_r2_unsafe(mbedtls_mpi *X,
                                        const mbedtls_mpi *N)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(X, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l(X, N->n * 2 * biL));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(X, X, N));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shrink(X, N->n));

cleanup:
    return ret;
}

MBEDTLS_STATIC_TESTABLE
void mbedtls_mpi_core_ct_uint_table_lookup(mbedtls_mpi_uint *dest,
                                           const mbedtls_mpi_uint *table,
                                           size_t limbs,
                                           size_t count,
                                           size_t index)
{
    for (size_t i = 0; i < count; i++, table += limbs) {
        mbedtls_ct_condition_t assign = mbedtls_ct_uint_eq(i, index);
        mbedtls_mpi_core_cond_assign(dest, table, limbs, assign);
    }
}

/* Fill X with n_bytes random bytes.
 * X must already have room for those bytes.
 * The ordering of the bytes returned from the RNG is suitable for
 * deterministic ECDSA (see RFC 6979 §3.3 and the specification of
 * mbedtls_mpi_core_random()).
 */
int mbedtls_mpi_core_fill_random(
    mbedtls_mpi_uint *X, size_t X_limbs,
    size_t n_bytes,
    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const size_t limbs = CHARS_TO_LIMBS(n_bytes);
    const size_t overhead = (limbs * ciL) - n_bytes;

    if (X_limbs < limbs) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    memset(X, 0, overhead);
    memset((unsigned char *) X + limbs * ciL, 0, (X_limbs - limbs) * ciL);
    MBEDTLS_MPI_CHK(f_rng(p_rng, (unsigned char *) X + overhead, n_bytes));
    mbedtls_mpi_core_bigendian_to_host(X, limbs);

cleanup:
    return ret;
}

int mbedtls_mpi_core_random(mbedtls_mpi_uint *X,
                            mbedtls_mpi_uint min,
                            const mbedtls_mpi_uint *N,
                            size_t limbs,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    mbedtls_ct_condition_t ge_lower = MBEDTLS_CT_TRUE, lt_upper = MBEDTLS_CT_FALSE;
    size_t n_bits = mbedtls_mpi_core_bitlen(N, limbs);
    size_t n_bytes = (n_bits + 7) / 8;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    /*
     * When min == 0, each try has at worst a probability 1/2 of failing
     * (the msb has a probability 1/2 of being 0, and then the result will
     * be < N), so after 30 tries failure probability is a most 2**(-30).
     *
     * When N is just below a power of 2, as is the case when generating
     * a random scalar on most elliptic curves, 1 try is enough with
     * overwhelming probability. When N is just above a power of 2
     * each try has a probability of failing that is almost 1/2.
     *
     * The probabilities are almost the same if min is nonzero but negligible
     * compared to N. This is always the case when N is crypto-sized, but
     * it's convenient to support small N for testing purposes. When N
     * is small, use a higher repeat count, otherwise the probability of
     * failure is macroscopic.
     */
    int count = (n_bytes > 4 ? 30 : 250);

    /*
     * Match the procedure given in RFC 6979 §3.3 (deterministic ECDSA)
     * when f_rng is a suitably parametrized instance of HMAC_DRBG:
     * - use the same byte ordering;
     * - keep the leftmost n_bits bits of the generated octet string;
     * - try until result is in the desired range.
     * This also avoids any bias, which is especially important for ECDSA.
     */
    do {
        MBEDTLS_MPI_CHK(mbedtls_mpi_core_fill_random(X, limbs,
                                                     n_bytes,
                                                     f_rng, p_rng));
        mbedtls_mpi_core_shift_r(X, limbs, 8 * n_bytes - n_bits);

        if (--count == 0) {
            ret = MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
            goto cleanup;
        }

        ge_lower = mbedtls_mpi_core_uint_le_mpi(min, X, limbs);
        lt_upper = mbedtls_mpi_core_lt_ct(X, N, limbs);
    } while (mbedtls_ct_bool_and(ge_lower, lt_upper) == MBEDTLS_CT_FALSE);

cleanup:
    return ret;
}

static size_t exp_mod_get_window_size(size_t Ebits)
{
#if MBEDTLS_MPI_WINDOW_SIZE >= 6
    return (Ebits > 671) ? 6 : (Ebits > 239) ? 5 : (Ebits >  79) ? 4 : 1;
#elif MBEDTLS_MPI_WINDOW_SIZE == 5
    return (Ebits > 239) ? 5 : (Ebits >  79) ? 4 : 1;
#elif MBEDTLS_MPI_WINDOW_SIZE > 1
    return (Ebits >  79) ? MBEDTLS_MPI_WINDOW_SIZE : 1;
#else
    (void) Ebits;
    return 1;
#endif
}

size_t mbedtls_mpi_core_exp_mod_working_limbs(size_t AN_limbs, size_t E_limbs)
{
    const size_t wsize = exp_mod_get_window_size(E_limbs * biL);
    const size_t welem = ((size_t) 1) << wsize;

    /* How big does each part of the working memory pool need to be? */
    const size_t table_limbs   = welem * AN_limbs;
    const size_t select_limbs  = AN_limbs;
    const size_t temp_limbs    = 2 * AN_limbs + 1;

    return table_limbs + select_limbs + temp_limbs;
}

static void exp_mod_precompute_window(const mbedtls_mpi_uint *A,
                                      const mbedtls_mpi_uint *N,
                                      size_t AN_limbs,
                                      mbedtls_mpi_uint mm,
                                      const mbedtls_mpi_uint *RR,
                                      size_t welem,
                                      mbedtls_mpi_uint *Wtable,
                                      mbedtls_mpi_uint *temp)
{
    /* W[0] = 1 (in Montgomery presentation) */
    memset(Wtable, 0, AN_limbs * ciL);
    Wtable[0] = 1;
    mbedtls_mpi_core_montmul(Wtable, Wtable, RR, AN_limbs, N, AN_limbs, mm, temp);

    /* W[1] = A (already in Montgomery presentation) */
    mbedtls_mpi_uint *W1 = Wtable + AN_limbs;
    memcpy(W1, A, AN_limbs * ciL);

    /* W[i+1] = W[i] * W[1], i >= 2 */
    mbedtls_mpi_uint *Wprev = W1;
    for (size_t i = 2; i < welem; i++) {
        mbedtls_mpi_uint *Wcur = Wprev + AN_limbs;
        mbedtls_mpi_core_montmul(Wcur, Wprev, W1, AN_limbs, N, AN_limbs, mm, temp);
        Wprev = Wcur;
    }
}

#if defined(MBEDTLS_TEST_HOOKS) && !defined(MBEDTLS_THREADING_C)
void (*mbedtls_safe_codepath_hook)(void) = NULL;
void (*mbedtls_unsafe_codepath_hook)(void) = NULL;
#endif

/*
 * This function calculates the indices of the exponent where the exponentiation algorithm should
 * start processing.
 *
 * Warning! If the parameter E_public has MBEDTLS_MPI_IS_PUBLIC as its value,
 * this function is not constant time with respect to the exponent (parameter E).
 */
static inline void exp_mod_calc_first_bit_optionally_safe(const mbedtls_mpi_uint *E,
                                                          size_t E_limbs,
                                                          int E_public,
                                                          size_t *E_limb_index,
                                                          size_t *E_bit_index)
{
    if (E_public == MBEDTLS_MPI_IS_PUBLIC) {
        /*
         * Skip leading zero bits.
         */
        size_t E_bits = mbedtls_mpi_core_bitlen(E, E_limbs);
        if (E_bits == 0) {
            /*
             * If E is 0 mbedtls_mpi_core_bitlen() returns 0. Even if that is the case, we will want
             * to represent it as a single 0 bit and as such the bitlength will be 1.
             */
            E_bits = 1;
        }

        *E_limb_index = E_bits / biL;
        *E_bit_index = E_bits % biL;

#if defined(MBEDTLS_TEST_HOOKS) && !defined(MBEDTLS_THREADING_C)
        if (mbedtls_unsafe_codepath_hook != NULL) {
            mbedtls_unsafe_codepath_hook();
        }
#endif
    } else {
        /*
         * Here we need to be constant time with respect to E and can't do anything better than
         * start at the first allocated bit.
         */
        *E_limb_index = E_limbs;
        *E_bit_index = 0;
#if defined(MBEDTLS_TEST_HOOKS) && !defined(MBEDTLS_THREADING_C)
        if (mbedtls_safe_codepath_hook != NULL) {
            mbedtls_safe_codepath_hook();
        }
#endif
    }
}

/*
 * Warning! If the parameter window_public has MBEDTLS_MPI_IS_PUBLIC as its value, this function is
 * not constant time with respect to the window parameter and consequently the exponent of the
 * exponentiation (parameter E of mbedtls_mpi_core_exp_mod_optionally_safe).
 */
static inline void exp_mod_table_lookup_optionally_safe(mbedtls_mpi_uint *Wselect,
                                                        mbedtls_mpi_uint *Wtable,
                                                        size_t AN_limbs, size_t welem,
                                                        mbedtls_mpi_uint window,
                                                        int window_public)
{
    if (window_public == MBEDTLS_MPI_IS_PUBLIC) {
        memcpy(Wselect, Wtable + window * AN_limbs, AN_limbs * ciL);
#if defined(MBEDTLS_TEST_HOOKS) && !defined(MBEDTLS_THREADING_C)
        if (mbedtls_unsafe_codepath_hook != NULL) {
            mbedtls_unsafe_codepath_hook();
        }
#endif
    } else {
        /* Select Wtable[window] without leaking window through
         * memory access patterns. */
        mbedtls_mpi_core_ct_uint_table_lookup(Wselect, Wtable,
                                              AN_limbs, welem, window);
#if defined(MBEDTLS_TEST_HOOKS) && !defined(MBEDTLS_THREADING_C)
        if (mbedtls_safe_codepath_hook != NULL) {
            mbedtls_safe_codepath_hook();
        }
#endif
    }
}

/* Exponentiation: X := A^E mod N.
 *
 * Warning! If the parameter E_public has MBEDTLS_MPI_IS_PUBLIC as its value,
 * this function is not constant time with respect to the exponent (parameter E).
 *
 * A must already be in Montgomery form.
 *
 * As in other bignum functions, assume that AN_limbs and E_limbs are nonzero.
 *
 * RR must contain 2^{2*biL} mod N.
 *
 * The algorithm is a variant of Left-to-right k-ary exponentiation: HAC 14.82
 * (The difference is that the body in our loop processes a single bit instead
 * of a full window.)
 */
static void mbedtls_mpi_core_exp_mod_optionally_safe(mbedtls_mpi_uint *X,
                                                     const mbedtls_mpi_uint *A,
                                                     const mbedtls_mpi_uint *N,
                                                     size_t AN_limbs,
                                                     const mbedtls_mpi_uint *E,
                                                     size_t E_limbs,
                                                     int E_public,
                                                     const mbedtls_mpi_uint *RR,
                                                     mbedtls_mpi_uint *T)
{
    /* We'll process the bits of E from most significant
     * (limb_index=E_limbs-1, E_bit_index=biL-1) to least significant
     * (limb_index=0, E_bit_index=0). */
    size_t E_limb_index = E_limbs;
    size_t E_bit_index = 0;
    exp_mod_calc_first_bit_optionally_safe(E, E_limbs, E_public,
                                           &E_limb_index, &E_bit_index);

    const size_t wsize = exp_mod_get_window_size(E_limb_index * biL);
    const size_t welem = ((size_t) 1) << wsize;

    /* This is how we will use the temporary storage T, which must have space
     * for table_limbs, select_limbs and (2 * AN_limbs + 1) for montmul. */
    const size_t table_limbs  = welem * AN_limbs;
    const size_t select_limbs = AN_limbs;

    /* Pointers to specific parts of the temporary working memory pool */
    mbedtls_mpi_uint *const Wtable  = T;
    mbedtls_mpi_uint *const Wselect = Wtable  +  table_limbs;
    mbedtls_mpi_uint *const temp    = Wselect + select_limbs;

    /*
     * Window precomputation
     */

    const mbedtls_mpi_uint mm = mbedtls_mpi_core_montmul_init(N);

    /* Set Wtable[i] = A^i (in Montgomery representation) */
    exp_mod_precompute_window(A, N, AN_limbs,
                              mm, RR,
                              welem, Wtable, temp);

    /*
     * Fixed window exponentiation
     */

    /* X = 1 (in Montgomery presentation) initially */
    memcpy(X, Wtable, AN_limbs * ciL);

    /* At any given time, window contains window_bits bits from E.
     * window_bits can go up to wsize. */
    size_t window_bits = 0;
    mbedtls_mpi_uint window = 0;

    do {
        /* Square */
        mbedtls_mpi_core_montmul(X, X, X, AN_limbs, N, AN_limbs, mm, temp);

        /* Move to the next bit of the exponent */
        if (E_bit_index == 0) {
            --E_limb_index;
            E_bit_index = biL - 1;
        } else {
            --E_bit_index;
        }
        /* Insert next exponent bit into window */
        ++window_bits;
        window <<= 1;
        window |= (E[E_limb_index] >> E_bit_index) & 1;

        /* Clear window if it's full. Also clear the window at the end,
         * when we've finished processing the exponent. */
        if (window_bits == wsize ||
            (E_bit_index == 0 && E_limb_index == 0)) {

            exp_mod_table_lookup_optionally_safe(Wselect, Wtable, AN_limbs, welem,
                                                 window, E_public);
            /* Multiply X by the selected element. */
            mbedtls_mpi_core_montmul(X, X, Wselect, AN_limbs, N, AN_limbs, mm,
                                     temp);
            window = 0;
            window_bits = 0;
        }
    } while (!(E_bit_index == 0 && E_limb_index == 0));
}

void mbedtls_mpi_core_exp_mod(mbedtls_mpi_uint *X,
                              const mbedtls_mpi_uint *A,
                              const mbedtls_mpi_uint *N, size_t AN_limbs,
                              const mbedtls_mpi_uint *E, size_t E_limbs,
                              const mbedtls_mpi_uint *RR,
                              mbedtls_mpi_uint *T)
{
    mbedtls_mpi_core_exp_mod_optionally_safe(X,
                                             A,
                                             N,
                                             AN_limbs,
                                             E,
                                             E_limbs,
                                             MBEDTLS_MPI_IS_SECRET,
                                             RR,
                                             T);
}

void mbedtls_mpi_core_exp_mod_unsafe(mbedtls_mpi_uint *X,
                                     const mbedtls_mpi_uint *A,
                                     const mbedtls_mpi_uint *N, size_t AN_limbs,
                                     const mbedtls_mpi_uint *E, size_t E_limbs,
                                     const mbedtls_mpi_uint *RR,
                                     mbedtls_mpi_uint *T)
{
    mbedtls_mpi_core_exp_mod_optionally_safe(X,
                                             A,
                                             N,
                                             AN_limbs,
                                             E,
                                             E_limbs,
                                             MBEDTLS_MPI_IS_PUBLIC,
                                             RR,
                                             T);
}

mbedtls_mpi_uint mbedtls_mpi_core_sub_int(mbedtls_mpi_uint *X,
                                          const mbedtls_mpi_uint *A,
                                          mbedtls_mpi_uint c,  /* doubles as carry */
                                          size_t limbs)
{
    for (size_t i = 0; i < limbs; i++) {
        mbedtls_mpi_uint s = A[i];
        mbedtls_mpi_uint t = s - c;
        c = (t > s);
        X[i] = t;
    }

    return c;
}

mbedtls_ct_condition_t mbedtls_mpi_core_check_zero_ct(const mbedtls_mpi_uint *A,
                                                      size_t limbs)
{
    volatile const mbedtls_mpi_uint *force_read_A = A;
    mbedtls_mpi_uint bits = 0;

    for (size_t i = 0; i < limbs; i++) {
        bits |= force_read_A[i];
    }

    return mbedtls_ct_bool(bits);
}

void mbedtls_mpi_core_to_mont_rep(mbedtls_mpi_uint *X,
                                  const mbedtls_mpi_uint *A,
                                  const mbedtls_mpi_uint *N,
                                  size_t AN_limbs,
                                  mbedtls_mpi_uint mm,
                                  const mbedtls_mpi_uint *rr,
                                  mbedtls_mpi_uint *T)
{
    mbedtls_mpi_core_montmul(X, A, rr, AN_limbs, N, AN_limbs, mm, T);
}

void mbedtls_mpi_core_from_mont_rep(mbedtls_mpi_uint *X,
                                    const mbedtls_mpi_uint *A,
                                    const mbedtls_mpi_uint *N,
                                    size_t AN_limbs,
                                    mbedtls_mpi_uint mm,
                                    mbedtls_mpi_uint *T)
{
    const mbedtls_mpi_uint Rinv = 1;    /* 1/R in Mont. rep => 1 */

    mbedtls_mpi_core_montmul(X, A, &Rinv, 1, N, AN_limbs, mm, T);
}

/*
 * Compute X = A - B mod N.
 * Both A and B must be in [0, N) and so will the output.
 */
static void mpi_core_sub_mod(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_uint *A,
                             const mbedtls_mpi_uint *B,
                             const mbedtls_mpi_uint *N,
                             size_t limbs)
{
    mbedtls_mpi_uint c = mbedtls_mpi_core_sub(X, A, B, limbs);
    (void) mbedtls_mpi_core_add_if(X, N, limbs, (unsigned) c);
}

/*
 * Divide X by 2 mod N in place, assuming N is odd.
 * The input must be in [0, N) and so will the output.
 */
MBEDTLS_STATIC_TESTABLE
void mbedtls_mpi_core_div2_mod_odd(mbedtls_mpi_uint *X,
                                   const mbedtls_mpi_uint *N,
                                   size_t limbs)
{
    /* If X is odd, add N to make it even before shifting. */
    unsigned odd = (unsigned) X[0] & 1;
    mbedtls_mpi_uint c = mbedtls_mpi_core_add_if(X, N, limbs, odd);
    mbedtls_mpi_core_shift_r(X, limbs, 1);
    X[limbs - 1] |= c << (biL - 1);
}

/*
 * Constant-time GCD and modular inversion - odd modulus.
 *
 * Pre-conditions: see public documentation.
 *
 * See https://www.jstage.jst.go.jp/article/transinf/E106.D/9/E106.D_2022ICP0009/_pdf
 *
 * The paper gives two computationally equivalent algorithms: Alg 7 (readable)
 * and Alg 8 (constant-time). We use a third version that's hopefully both:
 *
 *  u, v = A, N  # N is called p in the paper but doesn't have to be prime
 *  q, r = 0, 1
 *  repeat bits(A_limbs + N_limbs) times:
 *      d = v - u  # t1 in Alg 7
 *      t1 = (u and v both odd) ? u : d  # t1 in Alg 8
 *      t2 = (u and v both odd) ? d : (u odd) ? v : u  # t2 in Alg 8
 *      t2 >>= 1
 *      swap = t1 > t2  # similar to s, z in Alg 8
 *      u, v = (swap) ? t2, t1 : t1, t2
 *
 *      d = r - q mod N  # t2 in Alg 7
 *      t1 = (u and v both odd) ? q : d  # t3 in Alg 8
 *      t2 = (u and v both odd) ? d : (u odd) ? r : q  # t4 Alg 8
 *      t2 /= 2 mod N  # see below (pre_com)
 *      q, r = (swap) ? t2, t1 : t1, t2
 *  return v, q  # v: GCD, see Alg 6; q: no mult by pre_com, see below
 *
 * The ternary operators in the above pseudo-code need to be realised in a
 * constant-time fashion. We use conditional assign for t1, t2 and conditional
 * swap for the final update. (Note: the similarity between branches of Alg 7
 * are highlighted in tables 2 and 3 and the surrounding text.)
 *
 * Also, we re-order operations, grouping things related to the inverse, which
 * facilitates making its computation optional, and requires fewer temporaries.
 *
 * The only actual change from the paper is dropping the trick with pre_com,
 * which I think complicates things for no benefit.
 * See the comment on the big I != NULL block below for details.
 */
void mbedtls_mpi_core_gcd_modinv_odd(mbedtls_mpi_uint *G,
                                     mbedtls_mpi_uint *I,
                                     const mbedtls_mpi_uint *A,
                                     size_t A_limbs,
                                     const mbedtls_mpi_uint *N,
                                     size_t N_limbs,
                                     mbedtls_mpi_uint *T)
{
    /* GCD and modinv, names common to Alg 7 and Alg 8 */
    mbedtls_mpi_uint *u = T + 0 * N_limbs;
    mbedtls_mpi_uint *v = G;

    /* GCD and modinv, my name (t1, t2 from Alg 7) */
    mbedtls_mpi_uint *d = T + 1 * N_limbs;

    /* GCD and modinv, names from Alg 8 (note: t1, t2 from Alg 7 are d above) */
    mbedtls_mpi_uint *t1 = T + 2 * N_limbs;
    mbedtls_mpi_uint *t2 = T + 3 * N_limbs;

    /* modinv only, names common to Alg 7 and Alg 8 */
    mbedtls_mpi_uint *q = I;
    mbedtls_mpi_uint *r = I != NULL ? T + 4 * N_limbs : NULL;

    /*
     * Initial values:
     * u, v = A, N
     * q, r = 0, 1
     *
     * We only write to G (aka v) after reading from inputs (A and N), which
     * allows aliasing, except with N when I != NULL, as then we'll be operating
     * mod N on q and r later - see the public documentation.
     */
    if (A_limbs > N_limbs) {
        /* Violating this precondition should not result in memory errors. */
        A_limbs = N_limbs;
    }
    memcpy(u, A, A_limbs * ciL);
    memset((char *) u + A_limbs * ciL, 0, (N_limbs - A_limbs) * ciL);

    /* Avoid possible UB with memcpy when src == dst. */
    if (v != N) {
        memcpy(v, N, N_limbs * ciL);
    }

    if (I != NULL) {
        memset(q, 0, N_limbs * ciL);

        memset(r, 0, N_limbs * ciL);
        r[0] = 1;
    }

    /*
     * At each step, out of u, v, v - u we keep one, shift another, and discard
     * the third, then update (u, v) with the ordered result.
     * Then we mirror those actions with q, r, r - q mod N.
     *
     * Loop invariants:
     *  u <= v                  (on entry: A <= N)
     *  GCD(u, v) == GCD(A, N)  (on entry: trivial)
     *  v = A * q mod N         (on entry: N = A * 0 mod N)
     *  u = A * r mod N         (on entry: A = A * 1 mod N)
     *  q, r in [0, N)          (on entry: 0, 1)
     *
     * On exit:
     *  u = 0
     *  v = GCD(A, N) = A * q mod N
     *  if v == 1 then 1 = A * q mod N ie q is A's inverse mod N
     *  r = 0
     *
     * The exit state is a fixed point of the loop's body.
     * Alg 7 and Alg 8 use 2 * bitlen(N) iterations but Theorem 2 (above in the
     * paper) says bitlen(A) + bitlen(N) is actually enough.
     */
    for (size_t i = 0; i < (A_limbs + N_limbs) * biL; i++) {
        /* s, z in Alg 8 - use meaningful names instead */
        mbedtls_ct_condition_t u_odd = mbedtls_ct_bool(u[0] & 1);
        mbedtls_ct_condition_t v_odd = mbedtls_ct_bool(v[0] & 1);

        /* Other conditions that will be useful below */
        mbedtls_ct_condition_t u_odd_v_odd = mbedtls_ct_bool_and(u_odd, v_odd);
        mbedtls_ct_condition_t v_even = mbedtls_ct_bool_not(v_odd);
        mbedtls_ct_condition_t u_odd_v_even = mbedtls_ct_bool_and(u_odd, v_even);

        /* This is called t1 in Alg 7 (no name in Alg 8).
         * We know that u <= v so there is no carry */
        (void) mbedtls_mpi_core_sub(d, v, u, N_limbs);

        /* t1 (the thing that's kept) can be d (default) or u (if t2 is d) */
        memcpy(t1, d, N_limbs * ciL);
        mbedtls_mpi_core_cond_assign(t1, u, N_limbs, u_odd_v_odd);

        /* t2 (the thing that's shifted) can be u (if even), or v (if even),
         * or d (which is even if both u and v were odd) */
        memcpy(t2, u, N_limbs * ciL);
        mbedtls_mpi_core_cond_assign(t2, v, N_limbs, u_odd_v_even);
        mbedtls_mpi_core_cond_assign(t2, d, N_limbs, u_odd_v_odd);

        mbedtls_mpi_core_shift_r(t2, N_limbs, 1); // t2 is even

        /* Update u, v and re-order them if needed */
        memcpy(u, t1, N_limbs * ciL);
        memcpy(v, t2, N_limbs * ciL);
        mbedtls_ct_condition_t swap = mbedtls_mpi_core_lt_ct(v, u, N_limbs);
        mbedtls_mpi_core_cond_swap(u, v, N_limbs, swap);

        /* Now, if modinv was requested, do the same with q, r, but:
         * - decisions still based on u and v (their initial values);
         * - operations are now mod N;
         * - we re-use t1, t2 for what the paper calls t3, t4 in Alg 8.
         *
         * Here we slightly diverge from the paper and instead do the obvious
         * thing that preserves the invariants involving q and r: mirror
         * operations on u and v, ie also divide by 2 here (mod N).
         *
         * The paper uses a trick where it replaces division by 2 with
         * multiplication by 2 here, and compensates in the end by multiplying
         * by pre_com, which is probably intended as an optimisation.
         *
         * However I believe it's not actually an optimisation, since
         * constant-time modular multiplication by 2 (left-shift + conditional
         * subtract) is just as costly as constant-time modular division by 2
         * (conditional add + right-shift). So, skip it and keep things simple.
         */
        if (I != NULL) {
            /* This is called t2 in Alg 7 (no name in Alg 8). */
            mpi_core_sub_mod(d, q, r, N, N_limbs);

            /* t3 (the thing that's kept) */
            memcpy(t1, d, N_limbs * ciL);
            mbedtls_mpi_core_cond_assign(t1, r, N_limbs, u_odd_v_odd);

            /* t4 (the thing that's shifted) */
            memcpy(t2, r, N_limbs * ciL);
            mbedtls_mpi_core_cond_assign(t2, q, N_limbs, u_odd_v_even);
            mbedtls_mpi_core_cond_assign(t2, d, N_limbs, u_odd_v_odd);

            mbedtls_mpi_core_div2_mod_odd(t2, N, N_limbs);

            /* Update and possibly swap */
            memcpy(r, t1, N_limbs * ciL);
            memcpy(q, t2, N_limbs * ciL);
            mbedtls_mpi_core_cond_swap(r, q, N_limbs, swap);
        }
    }

    /* G and I already hold the correct values by virtue of being aliased */
}

#endif /* MBEDTLS_BIGNUM_C */
