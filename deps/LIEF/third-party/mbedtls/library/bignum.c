/*
 *  Multi-precision integer library
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 *  The following sources were referenced in the design of this Multi-precision
 *  Integer library:
 *
 *  [1] Handbook of Applied Cryptography - 1997
 *      Menezes, van Oorschot and Vanstone
 *
 *  [2] Multi-Precision Math
 *      Tom St Denis
 *      https://github.com/libtom/libtommath/blob/develop/tommath.pdf
 *
 *  [3] GNU Multi-Precision Arithmetic Library
 *      https://gmplib.org/manual/index.html
 *
 */

#include "common.h"

#if defined(MBEDTLS_BIGNUM_C)

#include "mbedtls/bignum.h"
#include "bignum_core.h"
#include "bignum_internal.h"
#include "bn_mul.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "constant_time_internal.h"

#include <limits.h>
#include <string.h>

#include "mbedtls/platform.h"



/*
 * Conditionally select an MPI sign in constant time.
 * (MPI sign is the field s in mbedtls_mpi. It is unsigned short and only 1 and -1 are valid
 * values.)
 */
static inline signed short mbedtls_ct_mpi_sign_if(mbedtls_ct_condition_t cond,
                                                  signed short sign1, signed short sign2)
{
    return (signed short) mbedtls_ct_uint_if(cond, sign1 + 1, sign2 + 1) - 1;
}

/*
 * Compare signed values in constant time
 */
int mbedtls_mpi_lt_mpi_ct(const mbedtls_mpi *X,
                          const mbedtls_mpi *Y,
                          unsigned *ret)
{
    mbedtls_ct_condition_t different_sign, X_is_negative, Y_is_negative, result;

    if (X->n != Y->n) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    /*
     * Set N_is_negative to MBEDTLS_CT_FALSE if N >= 0, MBEDTLS_CT_TRUE if N < 0.
     * We know that N->s == 1 if N >= 0 and N->s == -1 if N < 0.
     */
    X_is_negative = mbedtls_ct_bool((X->s & 2) >> 1);
    Y_is_negative = mbedtls_ct_bool((Y->s & 2) >> 1);

    /*
     * If the signs are different, then the positive operand is the bigger.
     * That is if X is negative (X_is_negative == 1), then X < Y is true and it
     * is false if X is positive (X_is_negative == 0).
     */
    different_sign = mbedtls_ct_bool_ne(X_is_negative, Y_is_negative); // true if different sign
    result = mbedtls_ct_bool_and(different_sign, X_is_negative);

    /*
     * Assuming signs are the same, compare X and Y. We switch the comparison
     * order if they are negative so that we get the right result, regardles of
     * sign.
     */

    /* This array is used to conditionally swap the pointers in const time */
    void * const p[2] = { X->p, Y->p };
    size_t i = mbedtls_ct_size_if_else_0(X_is_negative, 1);
    mbedtls_ct_condition_t lt = mbedtls_mpi_core_lt_ct(p[i], p[i ^ 1], X->n);

    /*
     * Store in result iff the signs are the same (i.e., iff different_sign == false). If
     * the signs differ, result has already been set, so we don't change it.
     */
    result = mbedtls_ct_bool_or(result,
                                mbedtls_ct_bool_and(mbedtls_ct_bool_not(different_sign), lt));

    *ret = mbedtls_ct_uint_if_else_0(result, 1);

    return 0;
}

/*
 * Conditionally assign X = Y, without leaking information
 * about whether the assignment was made or not.
 * (Leaking information about the respective sizes of X and Y is ok however.)
 */
#if defined(_MSC_VER) && defined(MBEDTLS_PLATFORM_IS_WINDOWS_ON_ARM64) && \
    (_MSC_FULL_VER < 193131103)
/*
 * MSVC miscompiles this function if it's inlined prior to Visual Studio 2022 version 17.1. See:
 * https://developercommunity.visualstudio.com/t/c-compiler-miscompiles-part-of-mbedtls-library-on/1646989
 */
__declspec(noinline)
#endif
int mbedtls_mpi_safe_cond_assign(mbedtls_mpi *X,
                                 const mbedtls_mpi *Y,
                                 unsigned char assign)
{
    int ret = 0;

    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, Y->n));

    {
        mbedtls_ct_condition_t do_assign = mbedtls_ct_bool(assign);

        X->s = mbedtls_ct_mpi_sign_if(do_assign, Y->s, X->s);

        mbedtls_mpi_core_cond_assign(X->p, Y->p, Y->n, do_assign);

        mbedtls_ct_condition_t do_not_assign = mbedtls_ct_bool_not(do_assign);
        for (size_t i = Y->n; i < X->n; i++) {
            X->p[i] = mbedtls_ct_mpi_uint_if_else_0(do_not_assign, X->p[i]);
        }
    }

cleanup:
    return ret;
}

/*
 * Conditionally swap X and Y, without leaking information
 * about whether the swap was made or not.
 * Here it is not ok to simply swap the pointers, which would lead to
 * different memory access patterns when X and Y are used afterwards.
 */
int mbedtls_mpi_safe_cond_swap(mbedtls_mpi *X,
                               mbedtls_mpi *Y,
                               unsigned char swap)
{
    int ret = 0;
    int s;

    if (X == Y) {
        return 0;
    }

    mbedtls_ct_condition_t do_swap = mbedtls_ct_bool(swap);

    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, Y->n));
    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(Y, X->n));

    s = X->s;
    X->s = mbedtls_ct_mpi_sign_if(do_swap, Y->s, X->s);
    Y->s = mbedtls_ct_mpi_sign_if(do_swap, s, Y->s);

    mbedtls_mpi_core_cond_swap(X->p, Y->p, X->n, do_swap);

cleanup:
    return ret;
}

/* Implementation that should never be optimized out by the compiler */
#define mbedtls_mpi_zeroize_and_free(v, n) mbedtls_zeroize_and_free(v, ciL * (n))

/*
 * Initialize one MPI
 */
void mbedtls_mpi_init(mbedtls_mpi *X)
{
    X->s = 1;
    X->n = 0;
    X->p = NULL;
}

/*
 * Unallocate one MPI
 */
void mbedtls_mpi_free(mbedtls_mpi *X)
{
    if (X == NULL) {
        return;
    }

    if (X->p != NULL) {
        mbedtls_mpi_zeroize_and_free(X->p, X->n);
    }

    X->s = 1;
    X->n = 0;
    X->p = NULL;
}

/*
 * Enlarge to the specified number of limbs
 */
int mbedtls_mpi_grow(mbedtls_mpi *X, size_t nblimbs)
{
    mbedtls_mpi_uint *p;

    if (nblimbs > MBEDTLS_MPI_MAX_LIMBS) {
        return MBEDTLS_ERR_MPI_ALLOC_FAILED;
    }

    if (X->n < nblimbs) {
        if ((p = (mbedtls_mpi_uint *) mbedtls_calloc(nblimbs, ciL)) == NULL) {
            return MBEDTLS_ERR_MPI_ALLOC_FAILED;
        }

        if (X->p != NULL) {
            memcpy(p, X->p, X->n * ciL);
            mbedtls_mpi_zeroize_and_free(X->p, X->n);
        }

        /* nblimbs fits in n because we ensure that MBEDTLS_MPI_MAX_LIMBS
         * fits, and we've checked that nblimbs <= MBEDTLS_MPI_MAX_LIMBS. */
        X->n = (unsigned short) nblimbs;
        X->p = p;
    }

    return 0;
}

/*
 * Resize down as much as possible,
 * while keeping at least the specified number of limbs
 */
int mbedtls_mpi_shrink(mbedtls_mpi *X, size_t nblimbs)
{
    mbedtls_mpi_uint *p;
    size_t i;

    if (nblimbs > MBEDTLS_MPI_MAX_LIMBS) {
        return MBEDTLS_ERR_MPI_ALLOC_FAILED;
    }

    /* Actually resize up if there are currently fewer than nblimbs limbs. */
    if (X->n <= nblimbs) {
        return mbedtls_mpi_grow(X, nblimbs);
    }
    /* After this point, then X->n > nblimbs and in particular X->n > 0. */

    for (i = X->n - 1; i > 0; i--) {
        if (X->p[i] != 0) {
            break;
        }
    }
    i++;

    if (i < nblimbs) {
        i = nblimbs;
    }

    if ((p = (mbedtls_mpi_uint *) mbedtls_calloc(i, ciL)) == NULL) {
        return MBEDTLS_ERR_MPI_ALLOC_FAILED;
    }

    if (X->p != NULL) {
        memcpy(p, X->p, i * ciL);
        mbedtls_mpi_zeroize_and_free(X->p, X->n);
    }

    /* i fits in n because we ensure that MBEDTLS_MPI_MAX_LIMBS
     * fits, and we've checked that i <= nblimbs <= MBEDTLS_MPI_MAX_LIMBS. */
    X->n = (unsigned short) i;
    X->p = p;

    return 0;
}

/* Resize X to have exactly n limbs and set it to 0. */
static int mbedtls_mpi_resize_clear(mbedtls_mpi *X, size_t limbs)
{
    if (limbs == 0) {
        mbedtls_mpi_free(X);
        return 0;
    } else if (X->n == limbs) {
        memset(X->p, 0, limbs * ciL);
        X->s = 1;
        return 0;
    } else {
        mbedtls_mpi_free(X);
        return mbedtls_mpi_grow(X, limbs);
    }
}

/*
 * Copy the contents of Y into X.
 *
 * This function is not constant-time. Leading zeros in Y may be removed.
 *
 * Ensure that X does not shrink. This is not guaranteed by the public API,
 * but some code in the bignum module might still rely on this property.
 */
int mbedtls_mpi_copy(mbedtls_mpi *X, const mbedtls_mpi *Y)
{
    int ret = 0;
    size_t i;

    if (X == Y) {
        return 0;
    }

    if (Y->n == 0) {
        if (X->n != 0) {
            X->s = 1;
            memset(X->p, 0, X->n * ciL);
        }
        return 0;
    }

    for (i = Y->n - 1; i > 0; i--) {
        if (Y->p[i] != 0) {
            break;
        }
    }
    i++;

    X->s = Y->s;

    if (X->n < i) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, i));
    } else {
        memset(X->p + i, 0, (X->n - i) * ciL);
    }

    memcpy(X->p, Y->p, i * ciL);

cleanup:

    return ret;
}

/*
 * Swap the contents of X and Y
 */
void mbedtls_mpi_swap(mbedtls_mpi *X, mbedtls_mpi *Y)
{
    mbedtls_mpi T;

    memcpy(&T,  X, sizeof(mbedtls_mpi));
    memcpy(X,  Y, sizeof(mbedtls_mpi));
    memcpy(Y, &T, sizeof(mbedtls_mpi));
}

static inline mbedtls_mpi_uint mpi_sint_abs(mbedtls_mpi_sint z)
{
    if (z >= 0) {
        return z;
    }
    /* Take care to handle the most negative value (-2^(biL-1)) correctly.
     * A naive -z would have undefined behavior.
     * Write this in a way that makes popular compilers happy (GCC, Clang,
     * MSVC). */
    return (mbedtls_mpi_uint) 0 - (mbedtls_mpi_uint) z;
}

/* Convert x to a sign, i.e. to 1, if x is positive, or -1, if x is negative.
 * This looks awkward but generates smaller code than (x < 0 ? -1 : 1) */
#define TO_SIGN(x) ((mbedtls_mpi_sint) (((mbedtls_mpi_uint) x) >> (biL - 1)) * -2 + 1)

/*
 * Set value from integer
 */
int mbedtls_mpi_lset(mbedtls_mpi *X, mbedtls_mpi_sint z)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, 1));
    memset(X->p, 0, X->n * ciL);

    X->p[0] = mpi_sint_abs(z);
    X->s    = TO_SIGN(z);

cleanup:

    return ret;
}

/*
 * Get a specific bit
 */
int mbedtls_mpi_get_bit(const mbedtls_mpi *X, size_t pos)
{
    if (X->n * biL <= pos) {
        return 0;
    }

    return (X->p[pos / biL] >> (pos % biL)) & 0x01;
}

/*
 * Set a bit to a specific value of 0 or 1
 */
int mbedtls_mpi_set_bit(mbedtls_mpi *X, size_t pos, unsigned char val)
{
    int ret = 0;
    size_t off = pos / biL;
    size_t idx = pos % biL;

    if (val != 0 && val != 1) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (X->n * biL <= pos) {
        if (val == 0) {
            return 0;
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, off + 1));
    }

    X->p[off] &= ~((mbedtls_mpi_uint) 0x01 << idx);
    X->p[off] |= (mbedtls_mpi_uint) val << idx;

cleanup:

    return ret;
}

/*
 * Return the number of less significant zero-bits
 */
size_t mbedtls_mpi_lsb(const mbedtls_mpi *X)
{
    size_t i;

#if defined(__has_builtin)
#if (MBEDTLS_MPI_UINT_MAX == UINT_MAX) && __has_builtin(__builtin_ctz)
    #define mbedtls_mpi_uint_ctz __builtin_ctz
#elif (MBEDTLS_MPI_UINT_MAX == ULONG_MAX) && __has_builtin(__builtin_ctzl)
    #define mbedtls_mpi_uint_ctz __builtin_ctzl
#elif (MBEDTLS_MPI_UINT_MAX == ULLONG_MAX) && __has_builtin(__builtin_ctzll)
    #define mbedtls_mpi_uint_ctz __builtin_ctzll
#endif
#endif

#if defined(mbedtls_mpi_uint_ctz)
    for (i = 0; i < X->n; i++) {
        if (X->p[i] != 0) {
            return i * biL + mbedtls_mpi_uint_ctz(X->p[i]);
        }
    }
#else
    size_t count = 0;
    for (i = 0; i < X->n; i++) {
        for (size_t j = 0; j < biL; j++, count++) {
            if (((X->p[i] >> j) & 1) != 0) {
                return count;
            }
        }
    }
#endif

    return 0;
}

/*
 * Return the number of bits
 */
size_t mbedtls_mpi_bitlen(const mbedtls_mpi *X)
{
    return mbedtls_mpi_core_bitlen(X->p, X->n);
}

/*
 * Return the total size in bytes
 */
size_t mbedtls_mpi_size(const mbedtls_mpi *X)
{
    return (mbedtls_mpi_bitlen(X) + 7) >> 3;
}

/*
 * Convert an ASCII character to digit value
 */
static int mpi_get_digit(mbedtls_mpi_uint *d, int radix, char c)
{
    *d = 255;

    if (c >= 0x30 && c <= 0x39) {
        *d = c - 0x30;
    }
    if (c >= 0x41 && c <= 0x46) {
        *d = c - 0x37;
    }
    if (c >= 0x61 && c <= 0x66) {
        *d = c - 0x57;
    }

    if (*d >= (mbedtls_mpi_uint) radix) {
        return MBEDTLS_ERR_MPI_INVALID_CHARACTER;
    }

    return 0;
}

/*
 * Import from an ASCII string
 */
int mbedtls_mpi_read_string(mbedtls_mpi *X, int radix, const char *s)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i, j, slen, n;
    int sign = 1;
    mbedtls_mpi_uint d;
    mbedtls_mpi T;

    if (radix < 2 || radix > 16) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    mbedtls_mpi_init(&T);

    if (s[0] == 0) {
        mbedtls_mpi_free(X);
        return 0;
    }

    if (s[0] == '-') {
        ++s;
        sign = -1;
    }

    slen = strlen(s);

    if (radix == 16) {
        if (slen > SIZE_MAX >> 2) {
            return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
        }

        n = BITS_TO_LIMBS(slen << 2);

        MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, n));
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(X, 0));

        for (i = slen, j = 0; i > 0; i--, j++) {
            MBEDTLS_MPI_CHK(mpi_get_digit(&d, radix, s[i - 1]));
            X->p[j / (2 * ciL)] |= d << ((j % (2 * ciL)) << 2);
        }
    } else {
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(X, 0));

        for (i = 0; i < slen; i++) {
            MBEDTLS_MPI_CHK(mpi_get_digit(&d, radix, s[i]));
            MBEDTLS_MPI_CHK(mbedtls_mpi_mul_int(&T, X, radix));
            MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(X, &T, d));
        }
    }

    if (sign < 0 && mbedtls_mpi_bitlen(X) != 0) {
        X->s = -1;
    }

cleanup:

    mbedtls_mpi_free(&T);

    return ret;
}

/*
 * Helper to write the digits high-order first.
 */
static int mpi_write_hlp(mbedtls_mpi *X, int radix,
                         char **p, const size_t buflen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi_uint r;
    size_t length = 0;
    char *p_end = *p + buflen;

    do {
        if (length >= buflen) {
            return MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL;
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_int(&r, X, radix));
        MBEDTLS_MPI_CHK(mbedtls_mpi_div_int(X, NULL, X, radix));
        /*
         * Write the residue in the current position, as an ASCII character.
         */
        if (r < 0xA) {
            *(--p_end) = (char) ('0' + r);
        } else {
            *(--p_end) = (char) ('A' + (r - 0xA));
        }

        length++;
    } while (mbedtls_mpi_cmp_int(X, 0) != 0);

    memmove(*p, p_end, length);
    *p += length;

cleanup:

    return ret;
}

/*
 * Export into an ASCII string
 */
int mbedtls_mpi_write_string(const mbedtls_mpi *X, int radix,
                             char *buf, size_t buflen, size_t *olen)
{
    int ret = 0;
    size_t n;
    char *p;
    mbedtls_mpi T;

    if (radix < 2 || radix > 16) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    n = mbedtls_mpi_bitlen(X);   /* Number of bits necessary to present `n`. */
    if (radix >=  4) {
        n >>= 1;                 /* Number of 4-adic digits necessary to present
                                  * `n`. If radix > 4, this might be a strict
                                  * overapproximation of the number of
                                  * radix-adic digits needed to present `n`. */
    }
    if (radix >= 16) {
        n >>= 1;                 /* Number of hexadecimal digits necessary to
                                  * present `n`. */

    }
    n += 1; /* Terminating null byte */
    n += 1; /* Compensate for the divisions above, which round down `n`
             * in case it's not even. */
    n += 1; /* Potential '-'-sign. */
    n += (n & 1);   /* Make n even to have enough space for hexadecimal writing,
                     * which always uses an even number of hex-digits. */

    if (buflen < n) {
        *olen = n;
        return MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL;
    }

    p = buf;
    mbedtls_mpi_init(&T);

    if (X->s == -1) {
        *p++ = '-';
        buflen--;
    }

    if (radix == 16) {
        int c;
        size_t i, j, k;

        for (i = X->n, k = 0; i > 0; i--) {
            for (j = ciL; j > 0; j--) {
                c = (X->p[i - 1] >> ((j - 1) << 3)) & 0xFF;

                if (c == 0 && k == 0 && (i + j) != 2) {
                    continue;
                }

                *(p++) = "0123456789ABCDEF" [c / 16];
                *(p++) = "0123456789ABCDEF" [c % 16];
                k = 1;
            }
        }
    } else {
        MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&T, X));

        if (T.s == -1) {
            T.s = 1;
        }

        MBEDTLS_MPI_CHK(mpi_write_hlp(&T, radix, &p, buflen));
    }

    *p++ = '\0';
    *olen = (size_t) (p - buf);

cleanup:

    mbedtls_mpi_free(&T);

    return ret;
}

#if defined(MBEDTLS_FS_IO)
/*
 * Read X from an opened file
 */
int mbedtls_mpi_read_file(mbedtls_mpi *X, int radix, FILE *fin)
{
    mbedtls_mpi_uint d;
    size_t slen;
    char *p;
    /*
     * Buffer should have space for (short) label and decimal formatted MPI,
     * newline characters and '\0'
     */
    char s[MBEDTLS_MPI_RW_BUFFER_SIZE];

    if (radix < 2 || radix > 16) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    memset(s, 0, sizeof(s));
    if (fgets(s, sizeof(s) - 1, fin) == NULL) {
        return MBEDTLS_ERR_MPI_FILE_IO_ERROR;
    }

    slen = strlen(s);
    if (slen == sizeof(s) - 2) {
        return MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL;
    }

    if (slen > 0 && s[slen - 1] == '\n') {
        slen--; s[slen] = '\0';
    }
    if (slen > 0 && s[slen - 1] == '\r') {
        slen--; s[slen] = '\0';
    }

    p = s + slen;
    while (p-- > s) {
        if (mpi_get_digit(&d, radix, *p) != 0) {
            break;
        }
    }

    return mbedtls_mpi_read_string(X, radix, p + 1);
}

/*
 * Write X into an opened file (or stdout if fout == NULL)
 */
int mbedtls_mpi_write_file(const char *p, const mbedtls_mpi *X, int radix, FILE *fout)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n, slen, plen;
    /*
     * Buffer should have space for (short) label and decimal formatted MPI,
     * newline characters and '\0'
     */
    char s[MBEDTLS_MPI_RW_BUFFER_SIZE];

    if (radix < 2 || radix > 16) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    memset(s, 0, sizeof(s));

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_string(X, radix, s, sizeof(s) - 2, &n));

    if (p == NULL) {
        p = "";
    }

    plen = strlen(p);
    slen = strlen(s);
    s[slen++] = '\r';
    s[slen++] = '\n';

    if (fout != NULL) {
        if (fwrite(p, 1, plen, fout) != plen ||
            fwrite(s, 1, slen, fout) != slen) {
            return MBEDTLS_ERR_MPI_FILE_IO_ERROR;
        }
    } else {
        mbedtls_printf("%s%s", p, s);
    }

cleanup:

    return ret;
}
#endif /* MBEDTLS_FS_IO */

/*
 * Import X from unsigned binary data, little endian
 *
 * This function is guaranteed to return an MPI with exactly the necessary
 * number of limbs (in particular, it does not skip 0s in the input).
 */
int mbedtls_mpi_read_binary_le(mbedtls_mpi *X,
                               const unsigned char *buf, size_t buflen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const size_t limbs = CHARS_TO_LIMBS(buflen);

    /* Ensure that target MPI has exactly the necessary number of limbs */
    MBEDTLS_MPI_CHK(mbedtls_mpi_resize_clear(X, limbs));

    MBEDTLS_MPI_CHK(mbedtls_mpi_core_read_le(X->p, X->n, buf, buflen));

cleanup:

    /*
     * This function is also used to import keys. However, wiping the buffers
     * upon failure is not necessary because failure only can happen before any
     * input is copied.
     */
    return ret;
}

/*
 * Import X from unsigned binary data, big endian
 *
 * This function is guaranteed to return an MPI with exactly the necessary
 * number of limbs (in particular, it does not skip 0s in the input).
 */
int mbedtls_mpi_read_binary(mbedtls_mpi *X, const unsigned char *buf, size_t buflen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const size_t limbs = CHARS_TO_LIMBS(buflen);

    /* Ensure that target MPI has exactly the necessary number of limbs */
    MBEDTLS_MPI_CHK(mbedtls_mpi_resize_clear(X, limbs));

    MBEDTLS_MPI_CHK(mbedtls_mpi_core_read_be(X->p, X->n, buf, buflen));

cleanup:

    /*
     * This function is also used to import keys. However, wiping the buffers
     * upon failure is not necessary because failure only can happen before any
     * input is copied.
     */
    return ret;
}

/*
 * Export X into unsigned binary data, little endian
 */
int mbedtls_mpi_write_binary_le(const mbedtls_mpi *X,
                                unsigned char *buf, size_t buflen)
{
    return mbedtls_mpi_core_write_le(X->p, X->n, buf, buflen);
}

/*
 * Export X into unsigned binary data, big endian
 */
int mbedtls_mpi_write_binary(const mbedtls_mpi *X,
                             unsigned char *buf, size_t buflen)
{
    return mbedtls_mpi_core_write_be(X->p, X->n, buf, buflen);
}

/*
 * Left-shift: X <<= count
 */
int mbedtls_mpi_shift_l(mbedtls_mpi *X, size_t count)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i;

    i = mbedtls_mpi_bitlen(X) + count;

    if (X->n * biL < i) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, BITS_TO_LIMBS(i)));
    }

    ret = 0;

    mbedtls_mpi_core_shift_l(X->p, X->n, count);
cleanup:

    return ret;
}

/*
 * Right-shift: X >>= count
 */
int mbedtls_mpi_shift_r(mbedtls_mpi *X, size_t count)
{
    if (X->n != 0) {
        mbedtls_mpi_core_shift_r(X->p, X->n, count);
    }
    return 0;
}

/*
 * Compare unsigned values
 */
int mbedtls_mpi_cmp_abs(const mbedtls_mpi *X, const mbedtls_mpi *Y)
{
    size_t i, j;

    for (i = X->n; i > 0; i--) {
        if (X->p[i - 1] != 0) {
            break;
        }
    }

    for (j = Y->n; j > 0; j--) {
        if (Y->p[j - 1] != 0) {
            break;
        }
    }

    /* If i == j == 0, i.e. abs(X) == abs(Y),
     * we end up returning 0 at the end of the function. */

    if (i > j) {
        return 1;
    }
    if (j > i) {
        return -1;
    }

    for (; i > 0; i--) {
        if (X->p[i - 1] > Y->p[i - 1]) {
            return 1;
        }
        if (X->p[i - 1] < Y->p[i - 1]) {
            return -1;
        }
    }

    return 0;
}

/*
 * Compare signed values
 */
int mbedtls_mpi_cmp_mpi(const mbedtls_mpi *X, const mbedtls_mpi *Y)
{
    size_t i, j;

    for (i = X->n; i > 0; i--) {
        if (X->p[i - 1] != 0) {
            break;
        }
    }

    for (j = Y->n; j > 0; j--) {
        if (Y->p[j - 1] != 0) {
            break;
        }
    }

    if (i == 0 && j == 0) {
        return 0;
    }

    if (i > j) {
        return X->s;
    }
    if (j > i) {
        return -Y->s;
    }

    if (X->s > 0 && Y->s < 0) {
        return 1;
    }
    if (Y->s > 0 && X->s < 0) {
        return -1;
    }

    for (; i > 0; i--) {
        if (X->p[i - 1] > Y->p[i - 1]) {
            return X->s;
        }
        if (X->p[i - 1] < Y->p[i - 1]) {
            return -X->s;
        }
    }

    return 0;
}

/*
 * Compare signed values
 */
int mbedtls_mpi_cmp_int(const mbedtls_mpi *X, mbedtls_mpi_sint z)
{
    mbedtls_mpi Y;
    mbedtls_mpi_uint p[1];

    *p  = mpi_sint_abs(z);
    Y.s = TO_SIGN(z);
    Y.n = 1;
    Y.p = p;

    return mbedtls_mpi_cmp_mpi(X, &Y);
}

/*
 * Unsigned addition: X = |A| + |B|  (HAC 14.7)
 */
int mbedtls_mpi_add_abs(mbedtls_mpi *X, const mbedtls_mpi *A, const mbedtls_mpi *B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t j;
    mbedtls_mpi_uint *p;
    mbedtls_mpi_uint c;

    if (X == B) {
        const mbedtls_mpi *T = A; A = X; B = T;
    }

    if (X != A) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_copy(X, A));
    }

    /*
     * X must always be positive as a result of unsigned additions.
     */
    X->s = 1;

    for (j = B->n; j > 0; j--) {
        if (B->p[j - 1] != 0) {
            break;
        }
    }

    /* Exit early to avoid undefined behavior on NULL+0 when X->n == 0
     * and B is 0 (of any size). */
    if (j == 0) {
        return 0;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, j));

    /* j is the number of non-zero limbs of B. Add those to X. */

    p = X->p;

    c = mbedtls_mpi_core_add(p, p, B->p, j);

    p += j;

    /* Now propagate any carry */

    while (c != 0) {
        if (j >= X->n) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, j + 1));
            p = X->p + j;
        }

        *p += c; c = (*p < c); j++; p++;
    }

cleanup:

    return ret;
}

/*
 * Unsigned subtraction: X = |A| - |B|  (HAC 14.9, 14.10)
 */
int mbedtls_mpi_sub_abs(mbedtls_mpi *X, const mbedtls_mpi *A, const mbedtls_mpi *B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;
    mbedtls_mpi_uint carry;

    for (n = B->n; n > 0; n--) {
        if (B->p[n - 1] != 0) {
            break;
        }
    }
    if (n > A->n) {
        /* B >= (2^ciL)^n > A */
        ret = MBEDTLS_ERR_MPI_NEGATIVE_VALUE;
        goto cleanup;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, A->n));

    /* Set the high limbs of X to match A. Don't touch the lower limbs
     * because X might be aliased to B, and we must not overwrite the
     * significant digits of B. */
    if (A->n > n && A != X) {
        memcpy(X->p + n, A->p + n, (A->n - n) * ciL);
    }
    if (X->n > A->n) {
        memset(X->p + A->n, 0, (X->n - A->n) * ciL);
    }

    carry = mbedtls_mpi_core_sub(X->p, A->p, B->p, n);
    if (carry != 0) {
        /* Propagate the carry through the rest of X. */
        carry = mbedtls_mpi_core_sub_int(X->p + n, X->p + n, carry, X->n - n);

        /* If we have further carry/borrow, the result is negative. */
        if (carry != 0) {
            ret = MBEDTLS_ERR_MPI_NEGATIVE_VALUE;
            goto cleanup;
        }
    }

    /* X should always be positive as a result of unsigned subtractions. */
    X->s = 1;

cleanup:
    return ret;
}

/* Common function for signed addition and subtraction.
 * Calculate A + B * flip_B where flip_B is 1 or -1.
 */
static int add_sub_mpi(mbedtls_mpi *X,
                       const mbedtls_mpi *A, const mbedtls_mpi *B,
                       int flip_B)
{
    int ret, s;

    s = A->s;
    if (A->s * B->s * flip_B < 0) {
        int cmp = mbedtls_mpi_cmp_abs(A, B);
        if (cmp >= 0) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(X, A, B));
            /* If |A| = |B|, the result is 0 and we must set the sign bit
             * to +1 regardless of which of A or B was negative. Otherwise,
             * since |A| > |B|, the sign is the sign of A. */
            X->s = cmp == 0 ? 1 : s;
        } else {
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(X, B, A));
            /* Since |A| < |B|, the sign is the opposite of A. */
            X->s = -s;
        }
    } else {
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_abs(X, A, B));
        X->s = s;
    }

cleanup:

    return ret;
}

/*
 * Signed addition: X = A + B
 */
int mbedtls_mpi_add_mpi(mbedtls_mpi *X, const mbedtls_mpi *A, const mbedtls_mpi *B)
{
    return add_sub_mpi(X, A, B, 1);
}

/*
 * Signed subtraction: X = A - B
 */
int mbedtls_mpi_sub_mpi(mbedtls_mpi *X, const mbedtls_mpi *A, const mbedtls_mpi *B)
{
    return add_sub_mpi(X, A, B, -1);
}

/*
 * Signed addition: X = A + b
 */
int mbedtls_mpi_add_int(mbedtls_mpi *X, const mbedtls_mpi *A, mbedtls_mpi_sint b)
{
    mbedtls_mpi B;
    mbedtls_mpi_uint p[1];

    p[0] = mpi_sint_abs(b);
    B.s = TO_SIGN(b);
    B.n = 1;
    B.p = p;

    return mbedtls_mpi_add_mpi(X, A, &B);
}

/*
 * Signed subtraction: X = A - b
 */
int mbedtls_mpi_sub_int(mbedtls_mpi *X, const mbedtls_mpi *A, mbedtls_mpi_sint b)
{
    mbedtls_mpi B;
    mbedtls_mpi_uint p[1];

    p[0] = mpi_sint_abs(b);
    B.s = TO_SIGN(b);
    B.n = 1;
    B.p = p;

    return mbedtls_mpi_sub_mpi(X, A, &B);
}

/*
 * Baseline multiplication: X = A * B  (HAC 14.12)
 */
int mbedtls_mpi_mul_mpi(mbedtls_mpi *X, const mbedtls_mpi *A, const mbedtls_mpi *B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i, j;
    mbedtls_mpi TA, TB;
    int result_is_zero = 0;

    mbedtls_mpi_init(&TA);
    mbedtls_mpi_init(&TB);

    if (X == A) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&TA, A)); A = &TA;
    }
    if (X == B) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&TB, B)); B = &TB;
    }

    for (i = A->n; i > 0; i--) {
        if (A->p[i - 1] != 0) {
            break;
        }
    }
    if (i == 0) {
        result_is_zero = 1;
    }

    for (j = B->n; j > 0; j--) {
        if (B->p[j - 1] != 0) {
            break;
        }
    }
    if (j == 0) {
        result_is_zero = 1;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, i + j));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(X, 0));

    mbedtls_mpi_core_mul(X->p, A->p, i, B->p, j);

    /* If the result is 0, we don't shortcut the operation, which reduces
     * but does not eliminate side channels leaking the zero-ness. We do
     * need to take care to set the sign bit properly since the library does
     * not fully support an MPI object with a value of 0 and s == -1. */
    if (result_is_zero) {
        X->s = 1;
    } else {
        X->s = A->s * B->s;
    }

cleanup:

    mbedtls_mpi_free(&TB); mbedtls_mpi_free(&TA);

    return ret;
}

/*
 * Baseline multiplication: X = A * b
 */
int mbedtls_mpi_mul_int(mbedtls_mpi *X, const mbedtls_mpi *A, mbedtls_mpi_uint b)
{
    size_t n = A->n;
    while (n > 0 && A->p[n - 1] == 0) {
        --n;
    }

    /* The general method below doesn't work if b==0. */
    if (b == 0 || n == 0) {
        return mbedtls_mpi_lset(X, 0);
    }

    /* Calculate A*b as A + A*(b-1) to take advantage of mbedtls_mpi_core_mla */
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    /* In general, A * b requires 1 limb more than b. If
     * A->p[n - 1] * b / b == A->p[n - 1], then A * b fits in the same
     * number of limbs as A and the call to grow() is not required since
     * copy() will take care of the growth if needed. However, experimentally,
     * making the call to grow() unconditional causes slightly fewer
     * calls to calloc() in ECP code, presumably because it reuses the
     * same mpi for a while and this way the mpi is more likely to directly
     * grow to its final size.
     *
     * Note that calculating A*b as 0 + A*b doesn't work as-is because
     * A,X can be the same. */
    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, n + 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(X, A));
    mbedtls_mpi_core_mla(X->p, X->n, A->p, n, b - 1);

cleanup:
    return ret;
}

/*
 * Unsigned integer divide - double mbedtls_mpi_uint dividend, u1/u0, and
 * mbedtls_mpi_uint divisor, d
 */
static mbedtls_mpi_uint mbedtls_int_div_int(mbedtls_mpi_uint u1,
                                            mbedtls_mpi_uint u0,
                                            mbedtls_mpi_uint d,
                                            mbedtls_mpi_uint *r)
{
#if defined(MBEDTLS_HAVE_UDBL)
    mbedtls_t_udbl dividend, quotient;
#else
    const mbedtls_mpi_uint radix = (mbedtls_mpi_uint) 1 << biH;
    const mbedtls_mpi_uint uint_halfword_mask = ((mbedtls_mpi_uint) 1 << biH) - 1;
    mbedtls_mpi_uint d0, d1, q0, q1, rAX, r0, quotient;
    mbedtls_mpi_uint u0_msw, u0_lsw;
    size_t s;
#endif

    /*
     * Check for overflow
     */
    if (0 == d || u1 >= d) {
        if (r != NULL) {
            *r = ~(mbedtls_mpi_uint) 0u;
        }

        return ~(mbedtls_mpi_uint) 0u;
    }

#if defined(MBEDTLS_HAVE_UDBL)
    dividend  = (mbedtls_t_udbl) u1 << biL;
    dividend |= (mbedtls_t_udbl) u0;
    quotient = dividend / d;
    if (quotient > ((mbedtls_t_udbl) 1 << biL) - 1) {
        quotient = ((mbedtls_t_udbl) 1 << biL) - 1;
    }

    if (r != NULL) {
        *r = (mbedtls_mpi_uint) (dividend - (quotient * d));
    }

    return (mbedtls_mpi_uint) quotient;
#else

    /*
     * Algorithm D, Section 4.3.1 - The Art of Computer Programming
     *   Vol. 2 - Seminumerical Algorithms, Knuth
     */

    /*
     * Normalize the divisor, d, and dividend, u0, u1
     */
    s = mbedtls_mpi_core_clz(d);
    d = d << s;

    u1 = u1 << s;
    u1 |= (u0 >> (biL - s)) & (-(mbedtls_mpi_sint) s >> (biL - 1));
    u0 =  u0 << s;

    d1 = d >> biH;
    d0 = d & uint_halfword_mask;

    u0_msw = u0 >> biH;
    u0_lsw = u0 & uint_halfword_mask;

    /*
     * Find the first quotient and remainder
     */
    q1 = u1 / d1;
    r0 = u1 - d1 * q1;

    while (q1 >= radix || (q1 * d0 > radix * r0 + u0_msw)) {
        q1 -= 1;
        r0 += d1;

        if (r0 >= radix) {
            break;
        }
    }

    rAX = (u1 * radix) + (u0_msw - q1 * d);
    q0 = rAX / d1;
    r0 = rAX - q0 * d1;

    while (q0 >= radix || (q0 * d0 > radix * r0 + u0_lsw)) {
        q0 -= 1;
        r0 += d1;

        if (r0 >= radix) {
            break;
        }
    }

    if (r != NULL) {
        *r = (rAX * radix + u0_lsw - q0 * d) >> s;
    }

    quotient = q1 * radix + q0;

    return quotient;
#endif
}

/*
 * Division by mbedtls_mpi: A = Q * B + R  (HAC 14.20)
 */
int mbedtls_mpi_div_mpi(mbedtls_mpi *Q, mbedtls_mpi *R, const mbedtls_mpi *A,
                        const mbedtls_mpi *B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i, n, t, k;
    mbedtls_mpi X, Y, Z, T1, T2;
    mbedtls_mpi_uint TP2[3];

    if (mbedtls_mpi_cmp_int(B, 0) == 0) {
        return MBEDTLS_ERR_MPI_DIVISION_BY_ZERO;
    }

    mbedtls_mpi_init(&X); mbedtls_mpi_init(&Y); mbedtls_mpi_init(&Z);
    mbedtls_mpi_init(&T1);
    /*
     * Avoid dynamic memory allocations for constant-size T2.
     *
     * T2 is used for comparison only and the 3 limbs are assigned explicitly,
     * so nobody increase the size of the MPI and we're safe to use an on-stack
     * buffer.
     */
    T2.s = 1;
    T2.n = sizeof(TP2) / sizeof(*TP2);
    T2.p = TP2;

    if (mbedtls_mpi_cmp_abs(A, B) < 0) {
        if (Q != NULL) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_lset(Q, 0));
        }
        if (R != NULL) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_copy(R, A));
        }
        return 0;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&X, A));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&Y, B));
    X.s = Y.s = 1;

    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(&Z, A->n + 2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&Z,  0));
    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(&T1, A->n + 2));

    k = mbedtls_mpi_bitlen(&Y) % biL;
    if (k < biL - 1) {
        k = biL - 1 - k;
        MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l(&X, k));
        MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l(&Y, k));
    } else {
        k = 0;
    }

    n = X.n - 1;
    t = Y.n - 1;
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l(&Y, biL * (n - t)));

    while (mbedtls_mpi_cmp_mpi(&X, &Y) >= 0) {
        Z.p[n - t]++;
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&X, &X, &Y));
    }
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&Y, biL * (n - t)));

    for (i = n; i > t; i--) {
        if (X.p[i] >= Y.p[t]) {
            Z.p[i - t - 1] = ~(mbedtls_mpi_uint) 0u;
        } else {
            Z.p[i - t - 1] = mbedtls_int_div_int(X.p[i], X.p[i - 1],
                                                 Y.p[t], NULL);
        }

        T2.p[0] = (i < 2) ? 0 : X.p[i - 2];
        T2.p[1] = (i < 1) ? 0 : X.p[i - 1];
        T2.p[2] = X.p[i];

        Z.p[i - t - 1]++;
        do {
            Z.p[i - t - 1]--;

            MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&T1, 0));
            T1.p[0] = (t < 1) ? 0 : Y.p[t - 1];
            T1.p[1] = Y.p[t];
            MBEDTLS_MPI_CHK(mbedtls_mpi_mul_int(&T1, &T1, Z.p[i - t - 1]));
        } while (mbedtls_mpi_cmp_mpi(&T1, &T2) > 0);

        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_int(&T1, &Y, Z.p[i - t - 1]));
        MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l(&T1,  biL * (i - t - 1)));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&X, &X, &T1));

        if (mbedtls_mpi_cmp_int(&X, 0) < 0) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&T1, &Y));
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l(&T1, biL * (i - t - 1)));
            MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&X, &X, &T1));
            Z.p[i - t - 1]--;
        }
    }

    if (Q != NULL) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_copy(Q, &Z));
        Q->s = A->s * B->s;
    }

    if (R != NULL) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&X, k));
        X.s = A->s;
        MBEDTLS_MPI_CHK(mbedtls_mpi_copy(R, &X));

        if (mbedtls_mpi_cmp_int(R, 0) == 0) {
            R->s = 1;
        }
    }

cleanup:

    mbedtls_mpi_free(&X); mbedtls_mpi_free(&Y); mbedtls_mpi_free(&Z);
    mbedtls_mpi_free(&T1);
    mbedtls_platform_zeroize(TP2, sizeof(TP2));

    return ret;
}

/*
 * Division by int: A = Q * b + R
 */
int mbedtls_mpi_div_int(mbedtls_mpi *Q, mbedtls_mpi *R,
                        const mbedtls_mpi *A,
                        mbedtls_mpi_sint b)
{
    mbedtls_mpi B;
    mbedtls_mpi_uint p[1];

    p[0] = mpi_sint_abs(b);
    B.s = TO_SIGN(b);
    B.n = 1;
    B.p = p;

    return mbedtls_mpi_div_mpi(Q, R, A, &B);
}

/*
 * Modulo: R = A mod B
 */
int mbedtls_mpi_mod_mpi(mbedtls_mpi *R, const mbedtls_mpi *A, const mbedtls_mpi *B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (mbedtls_mpi_cmp_int(B, 0) < 0) {
        return MBEDTLS_ERR_MPI_NEGATIVE_VALUE;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_div_mpi(NULL, R, A, B));

    while (mbedtls_mpi_cmp_int(R, 0) < 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(R, R, B));
    }

    while (mbedtls_mpi_cmp_mpi(R, B) >= 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(R, R, B));
    }

cleanup:

    return ret;
}

/*
 * Modulo: r = A mod b
 */
int mbedtls_mpi_mod_int(mbedtls_mpi_uint *r, const mbedtls_mpi *A, mbedtls_mpi_sint b)
{
    size_t i;
    mbedtls_mpi_uint x, y, z;

    if (b == 0) {
        return MBEDTLS_ERR_MPI_DIVISION_BY_ZERO;
    }

    if (b < 0) {
        return MBEDTLS_ERR_MPI_NEGATIVE_VALUE;
    }

    /*
     * handle trivial cases
     */
    if (b == 1 || A->n == 0) {
        *r = 0;
        return 0;
    }

    if (b == 2) {
        *r = A->p[0] & 1;
        return 0;
    }

    /*
     * general case
     */
    for (i = A->n, y = 0; i > 0; i--) {
        x  = A->p[i - 1];
        y  = (y << biH) | (x >> biH);
        z  = y / b;
        y -= z * b;

        x <<= biH;
        y  = (y << biH) | (x >> biH);
        z  = y / b;
        y -= z * b;
    }

    /*
     * If A is negative, then the current y represents a negative value.
     * Flipping it to the positive side.
     */
    if (A->s < 0 && y != 0) {
        y = b - y;
    }

    *r = y;

    return 0;
}

/*
 * Warning! If the parameter E_public has MBEDTLS_MPI_IS_PUBLIC as its value,
 * this function is not constant time with respect to the exponent (parameter E).
 */
static int mbedtls_mpi_exp_mod_optionally_safe(mbedtls_mpi *X, const mbedtls_mpi *A,
                                               const mbedtls_mpi *E, int E_public,
                                               const mbedtls_mpi *N, mbedtls_mpi *prec_RR)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (mbedtls_mpi_cmp_int(N, 0) <= 0 || (N->p[0] & 1) == 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (mbedtls_mpi_cmp_int(E, 0) < 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (mbedtls_mpi_bitlen(E) > MBEDTLS_MPI_MAX_BITS ||
        mbedtls_mpi_bitlen(N) > MBEDTLS_MPI_MAX_BITS) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    /*
     * Ensure that the exponent that we are passing to the core is not NULL.
     */
    if (E->n == 0) {
        ret = mbedtls_mpi_lset(X, 1);
        return ret;
    }

    /*
     * Allocate working memory for mbedtls_mpi_core_exp_mod()
     */
    size_t T_limbs = mbedtls_mpi_core_exp_mod_working_limbs(N->n, E->n);
    mbedtls_mpi_uint *T = (mbedtls_mpi_uint *) mbedtls_calloc(T_limbs, sizeof(mbedtls_mpi_uint));
    if (T == NULL) {
        return MBEDTLS_ERR_MPI_ALLOC_FAILED;
    }

    mbedtls_mpi RR;
    mbedtls_mpi_init(&RR);

    /*
     * If 1st call, pre-compute R^2 mod N
     */
    if (prec_RR == NULL || prec_RR->p == NULL) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_core_get_mont_r2_unsafe(&RR, N));

        if (prec_RR != NULL) {
            *prec_RR = RR;
        }
    } else {
        MBEDTLS_MPI_CHK(mbedtls_mpi_grow(prec_RR, N->n));
        RR = *prec_RR;
    }

    /*
     * To preserve constness we need to make a copy of A. Using X for this to
     * save memory.
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(X, A));

    /*
     * Compensate for negative A (and correct at the end).
     */
    X->s = 1;

    /*
     * Make sure that X is in a form that is safe for consumption by
     * the core functions.
     *
     * - The core functions will not touch the limbs of X above N->n. The
     *   result will be correct if those limbs are 0, which the mod call
     *   ensures.
     * - Also, X must have at least as many limbs as N for the calls to the
     *   core functions.
     */
    if (mbedtls_mpi_cmp_mpi(X, N) >= 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(X, X, N));
    }
    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(X, N->n));

    /*
     * Convert to and from Montgomery around mbedtls_mpi_core_exp_mod().
     */
    {
        mbedtls_mpi_uint mm = mbedtls_mpi_core_montmul_init(N->p);
        mbedtls_mpi_core_to_mont_rep(X->p, X->p, N->p, N->n, mm, RR.p, T);
        if (E_public == MBEDTLS_MPI_IS_PUBLIC) {
            mbedtls_mpi_core_exp_mod_unsafe(X->p, X->p, N->p, N->n, E->p, E->n, RR.p, T);
        } else {
            mbedtls_mpi_core_exp_mod(X->p, X->p, N->p, N->n, E->p, E->n, RR.p, T);
        }
        mbedtls_mpi_core_from_mont_rep(X->p, X->p, N->p, N->n, mm, T);
    }

    /*
     * Correct for negative A.
     */
    if (A->s == -1 && (E->p[0] & 1) != 0) {
        mbedtls_ct_condition_t is_x_non_zero = mbedtls_mpi_core_check_zero_ct(X->p, X->n);
        X->s = mbedtls_ct_mpi_sign_if(is_x_non_zero, -1, 1);

        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(X, N, X));
    }

cleanup:

    mbedtls_mpi_zeroize_and_free(T, T_limbs);

    if (prec_RR == NULL || prec_RR->p == NULL) {
        mbedtls_mpi_free(&RR);
    }

    return ret;
}

int mbedtls_mpi_exp_mod(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *E, const mbedtls_mpi *N,
                        mbedtls_mpi *prec_RR)
{
    return mbedtls_mpi_exp_mod_optionally_safe(X, A, E, MBEDTLS_MPI_IS_SECRET, N, prec_RR);
}

int mbedtls_mpi_exp_mod_unsafe(mbedtls_mpi *X, const mbedtls_mpi *A,
                               const mbedtls_mpi *E, const mbedtls_mpi *N,
                               mbedtls_mpi *prec_RR)
{
    return mbedtls_mpi_exp_mod_optionally_safe(X, A, E, MBEDTLS_MPI_IS_PUBLIC, N, prec_RR);
}

/*
 * Greatest common divisor: G = gcd(A, B)  (HAC 14.54)
 */
int mbedtls_mpi_gcd(mbedtls_mpi *G, const mbedtls_mpi *A, const mbedtls_mpi *B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t lz, lzt;
    mbedtls_mpi TA, TB;

    mbedtls_mpi_init(&TA); mbedtls_mpi_init(&TB);

    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&TA, A));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&TB, B));

    lz = mbedtls_mpi_lsb(&TA);
    lzt = mbedtls_mpi_lsb(&TB);

    /* The loop below gives the correct result when A==0 but not when B==0.
     * So have a special case for B==0. Leverage the fact that we just
     * calculated the lsb and lsb(B)==0 iff B is odd or 0 to make the test
     * slightly more efficient than cmp_int(). */
    if (lzt == 0 && mbedtls_mpi_get_bit(&TB, 0) == 0) {
        ret = mbedtls_mpi_copy(G, A);
        goto cleanup;
    }

    if (lzt < lz) {
        lz = lzt;
    }

    TA.s = TB.s = 1;

    /* We mostly follow the procedure described in HAC 14.54, but with some
     * minor differences:
     * - Sequences of multiplications or divisions by 2 are grouped into a
     *   single shift operation.
     * - The procedure in HAC assumes that 0 < TB <= TA.
     *     - The condition TB <= TA is not actually necessary for correctness.
     *       TA and TB have symmetric roles except for the loop termination
     *       condition, and the shifts at the beginning of the loop body
     *       remove any significance from the ordering of TA vs TB before
     *       the shifts.
     *     - If TA = 0, the loop goes through 0 iterations and the result is
     *       correctly TB.
     *     - The case TB = 0 was short-circuited above.
     *
     * For the correctness proof below, decompose the original values of
     * A and B as
     *   A = sa * 2^a * A' with A'=0 or A' odd, and sa = +-1
     *   B = sb * 2^b * B' with B'=0 or B' odd, and sb = +-1
     * Then gcd(A, B) = 2^{min(a,b)} * gcd(A',B'),
     * and gcd(A',B') is odd or 0.
     *
     * At the beginning, we have TA = |A| and TB = |B| so gcd(A,B) = gcd(TA,TB).
     * The code maintains the following invariant:
     *     gcd(A,B) = 2^k * gcd(TA,TB) for some k   (I)
     */

    /* Proof that the loop terminates:
     * At each iteration, either the right-shift by 1 is made on a nonzero
     * value and the nonnegative integer bitlen(TA) + bitlen(TB) decreases
     * by at least 1, or the right-shift by 1 is made on zero and then
     * TA becomes 0 which ends the loop (TB cannot be 0 if it is right-shifted
     * since in that case TB is calculated from TB-TA with the condition TB>TA).
     */
    while (mbedtls_mpi_cmp_int(&TA, 0) != 0) {
        /* Divisions by 2 preserve the invariant (I). */
        MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&TA, mbedtls_mpi_lsb(&TA)));
        MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&TB, mbedtls_mpi_lsb(&TB)));

        /* Set either TA or TB to |TA-TB|/2. Since TA and TB are both odd,
         * TA-TB is even so the division by 2 has an integer result.
         * Invariant (I) is preserved since any odd divisor of both TA and TB
         * also divides |TA-TB|/2, and any odd divisor of both TA and |TA-TB|/2
         * also divides TB, and any odd divisor of both TB and |TA-TB|/2 also
         * divides TA.
         */
        if (mbedtls_mpi_cmp_mpi(&TA, &TB) >= 0) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(&TA, &TA, &TB));
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&TA, 1));
        } else {
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(&TB, &TB, &TA));
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&TB, 1));
        }
        /* Note that one of TA or TB is still odd. */
    }

    /* By invariant (I), gcd(A,B) = 2^k * gcd(TA,TB) for some k.
     * At the loop exit, TA = 0, so gcd(TA,TB) = TB.
     * - If there was at least one loop iteration, then one of TA or TB is odd,
     *   and TA = 0, so TB is odd and gcd(TA,TB) = gcd(A',B'). In this case,
     *   lz = min(a,b) so gcd(A,B) = 2^lz * TB.
     * - If there was no loop iteration, then A was 0, and gcd(A,B) = B.
     *   In this case, lz = 0 and B = TB so gcd(A,B) = B = 2^lz * TB as well.
     */

    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l(&TB, lz));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(G, &TB));

cleanup:

    mbedtls_mpi_free(&TA); mbedtls_mpi_free(&TB);

    return ret;
}

/*
 * Fill X with size bytes of random.
 * The bytes returned from the RNG are used in a specific order which
 * is suitable for deterministic ECDSA (see the specification of
 * mbedtls_mpi_random() and the implementation in mbedtls_mpi_fill_random()).
 */
int mbedtls_mpi_fill_random(mbedtls_mpi *X, size_t size,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const size_t limbs = CHARS_TO_LIMBS(size);

    /* Ensure that target MPI has exactly the necessary number of limbs */
    MBEDTLS_MPI_CHK(mbedtls_mpi_resize_clear(X, limbs));
    if (size == 0) {
        return 0;
    }

    ret = mbedtls_mpi_core_fill_random(X->p, X->n, size, f_rng, p_rng);

cleanup:
    return ret;
}

int mbedtls_mpi_random(mbedtls_mpi *X,
                       mbedtls_mpi_sint min,
                       const mbedtls_mpi *N,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng)
{
    if (min < 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }
    if (mbedtls_mpi_cmp_int(N, min) <= 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    /* Ensure that target MPI has exactly the same number of limbs
     * as the upper bound, even if the upper bound has leading zeros.
     * This is necessary for mbedtls_mpi_core_random. */
    int ret = mbedtls_mpi_resize_clear(X, N->n);
    if (ret != 0) {
        return ret;
    }

    return mbedtls_mpi_core_random(X->p, min, N->p, X->n, f_rng, p_rng);
}

/*
 * Modular inverse: X = A^-1 mod N  (HAC 14.61 / 14.64)
 */
int mbedtls_mpi_inv_mod(mbedtls_mpi *X, const mbedtls_mpi *A, const mbedtls_mpi *N)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi G, TA, TU, U1, U2, TB, TV, V1, V2;

    if (mbedtls_mpi_cmp_int(N, 1) <= 0) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    mbedtls_mpi_init(&TA); mbedtls_mpi_init(&TU); mbedtls_mpi_init(&U1); mbedtls_mpi_init(&U2);
    mbedtls_mpi_init(&G); mbedtls_mpi_init(&TB); mbedtls_mpi_init(&TV);
    mbedtls_mpi_init(&V1); mbedtls_mpi_init(&V2);

    MBEDTLS_MPI_CHK(mbedtls_mpi_gcd(&G, A, N));

    if (mbedtls_mpi_cmp_int(&G, 1) != 0) {
        ret = MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
        goto cleanup;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&TA, A, N));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&TU, &TA));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&TB, N));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&TV, N));

    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&U1, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&U2, 0));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&V1, 0));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&V2, 1));

    do {
        while ((TU.p[0] & 1) == 0) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&TU, 1));

            if ((U1.p[0] & 1) != 0 || (U2.p[0] & 1) != 0) {
                MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&U1, &U1, &TB));
                MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&U2, &U2, &TA));
            }

            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&U1, 1));
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&U2, 1));
        }

        while ((TV.p[0] & 1) == 0) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&TV, 1));

            if ((V1.p[0] & 1) != 0 || (V2.p[0] & 1) != 0) {
                MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&V1, &V1, &TB));
                MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&V2, &V2, &TA));
            }

            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&V1, 1));
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&V2, 1));
        }

        if (mbedtls_mpi_cmp_mpi(&TU, &TV) >= 0) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&TU, &TU, &TV));
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&U1, &U1, &V1));
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&U2, &U2, &V2));
        } else {
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&TV, &TV, &TU));
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&V1, &V1, &U1));
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&V2, &V2, &U2));
        }
    } while (mbedtls_mpi_cmp_int(&TU, 0) != 0);

    while (mbedtls_mpi_cmp_int(&V1, 0) < 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&V1, &V1, N));
    }

    while (mbedtls_mpi_cmp_mpi(&V1, N) >= 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&V1, &V1, N));
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(X, &V1));

cleanup:

    mbedtls_mpi_free(&TA); mbedtls_mpi_free(&TU); mbedtls_mpi_free(&U1); mbedtls_mpi_free(&U2);
    mbedtls_mpi_free(&G); mbedtls_mpi_free(&TB); mbedtls_mpi_free(&TV);
    mbedtls_mpi_free(&V1); mbedtls_mpi_free(&V2);

    return ret;
}

#if defined(MBEDTLS_GENPRIME)

/* Gaps between primes, starting at 3. https://oeis.org/A001223 */
static const unsigned char small_prime_gaps[] = {
    2, 2, 4, 2, 4, 2, 4, 6,
    2, 6, 4, 2, 4, 6, 6, 2,
    6, 4, 2, 6, 4, 6, 8, 4,
    2, 4, 2, 4, 14, 4, 6, 2,
    10, 2, 6, 6, 4, 6, 6, 2,
    10, 2, 4, 2, 12, 12, 4, 2,
    4, 6, 2, 10, 6, 6, 6, 2,
    6, 4, 2, 10, 14, 4, 2, 4,
    14, 6, 10, 2, 4, 6, 8, 6,
    6, 4, 6, 8, 4, 8, 10, 2,
    10, 2, 6, 4, 6, 8, 4, 2,
    4, 12, 8, 4, 8, 4, 6, 12,
    2, 18, 6, 10, 6, 6, 2, 6,
    10, 6, 6, 2, 6, 6, 4, 2,
    12, 10, 2, 4, 6, 6, 2, 12,
    4, 6, 8, 10, 8, 10, 8, 6,
    6, 4, 8, 6, 4, 8, 4, 14,
    10, 12, 2, 10, 2, 4, 2, 10,
    14, 4, 2, 4, 14, 4, 2, 4,
    20, 4, 8, 10, 8, 4, 6, 6,
    14, 4, 6, 6, 8, 6, /*reaches 997*/
    0 /* the last entry is effectively unused */
};

/*
 * Small divisors test (X must be positive)
 *
 * Return values:
 * 0: no small factor (possible prime, more tests needed)
 * 1: certain prime
 * MBEDTLS_ERR_MPI_NOT_ACCEPTABLE: certain non-prime
 * other negative: error
 */
static int mpi_check_small_factors(const mbedtls_mpi *X)
{
    int ret = 0;
    size_t i;
    mbedtls_mpi_uint r;
    unsigned p = 3; /* The first odd prime */

    if ((X->p[0] & 1) == 0) {
        return MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
    }

    for (i = 0; i < sizeof(small_prime_gaps); p += small_prime_gaps[i], i++) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_int(&r, X, p));
        if (r == 0) {
            if (mbedtls_mpi_cmp_int(X, p) == 0) {
                return 1;
            } else {
                return MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
            }
        }
    }

cleanup:
    return ret;
}

/*
 * Miller-Rabin pseudo-primality test  (HAC 4.24)
 */
static int mpi_miller_rabin(const mbedtls_mpi *X, size_t rounds,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret, count;
    size_t i, j, k, s;
    mbedtls_mpi W, R, T, A, RR;

    mbedtls_mpi_init(&W); mbedtls_mpi_init(&R);
    mbedtls_mpi_init(&T); mbedtls_mpi_init(&A);
    mbedtls_mpi_init(&RR);

    /*
     * W = |X| - 1
     * R = W >> lsb( W )
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&W, X, 1));
    s = mbedtls_mpi_lsb(&W);
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&R, &W));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&R, s));

    for (i = 0; i < rounds; i++) {
        /*
         * pick a random A, 1 < A < |X| - 1
         */
        count = 0;
        do {
            MBEDTLS_MPI_CHK(mbedtls_mpi_fill_random(&A, X->n * ciL, f_rng, p_rng));

            j = mbedtls_mpi_bitlen(&A);
            k = mbedtls_mpi_bitlen(&W);
            if (j > k) {
                A.p[A.n - 1] &= ((mbedtls_mpi_uint) 1 << (k - (A.n - 1) * biL - 1)) - 1;
            }

            if (count++ > 30) {
                ret = MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
                goto cleanup;
            }

        } while (mbedtls_mpi_cmp_mpi(&A, &W) >= 0 ||
                 mbedtls_mpi_cmp_int(&A, 1)  <= 0);

        /*
         * A = A^R mod |X|
         */
        MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&A, &A, &R, X, &RR));

        if (mbedtls_mpi_cmp_mpi(&A, &W) == 0 ||
            mbedtls_mpi_cmp_int(&A,  1) == 0) {
            continue;
        }

        j = 1;
        while (j < s && mbedtls_mpi_cmp_mpi(&A, &W) != 0) {
            /*
             * A = A * A mod |X|
             */
            MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&T, &A, &A));
            MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&A, &T, X));

            if (mbedtls_mpi_cmp_int(&A, 1) == 0) {
                break;
            }

            j++;
        }

        /*
         * not prime if A != |X| - 1 or A == 1
         */
        if (mbedtls_mpi_cmp_mpi(&A, &W) != 0 ||
            mbedtls_mpi_cmp_int(&A,  1) == 0) {
            ret = MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
            break;
        }
    }

cleanup:
    mbedtls_mpi_free(&W); mbedtls_mpi_free(&R);
    mbedtls_mpi_free(&T); mbedtls_mpi_free(&A);
    mbedtls_mpi_free(&RR);

    return ret;
}

/*
 * Pseudo-primality test: small factors, then Miller-Rabin
 */
int mbedtls_mpi_is_prime_ext(const mbedtls_mpi *X, int rounds,
                             int (*f_rng)(void *, unsigned char *, size_t),
                             void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi XX;

    XX.s = 1;
    XX.n = X->n;
    XX.p = X->p;

    if (mbedtls_mpi_cmp_int(&XX, 0) == 0 ||
        mbedtls_mpi_cmp_int(&XX, 1) == 0) {
        return MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
    }

    if (mbedtls_mpi_cmp_int(&XX, 2) == 0) {
        return 0;
    }

    if ((ret = mpi_check_small_factors(&XX)) != 0) {
        if (ret == 1) {
            return 0;
        }

        return ret;
    }

    return mpi_miller_rabin(&XX, rounds, f_rng, p_rng);
}

/*
 * Prime number generation
 *
 * To generate an RSA key in a way recommended by FIPS 186-4, both primes must
 * be either 1024 bits or 1536 bits long, and flags must contain
 * MBEDTLS_MPI_GEN_PRIME_FLAG_LOW_ERR.
 */
int mbedtls_mpi_gen_prime(mbedtls_mpi *X, size_t nbits, int flags,
                          int (*f_rng)(void *, unsigned char *, size_t),
                          void *p_rng)
{
#ifdef MBEDTLS_HAVE_INT64
// ceil(2^63.5)
#define CEIL_MAXUINT_DIV_SQRT2 0xb504f333f9de6485ULL
#else
// ceil(2^31.5)
#define CEIL_MAXUINT_DIV_SQRT2 0xb504f334U
#endif
    int ret = MBEDTLS_ERR_MPI_NOT_ACCEPTABLE;
    size_t k, n;
    int rounds;
    mbedtls_mpi_uint r;
    mbedtls_mpi Y;

    if (nbits < 3 || nbits > MBEDTLS_MPI_MAX_BITS) {
        return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    mbedtls_mpi_init(&Y);

    n = BITS_TO_LIMBS(nbits);

    if ((flags & MBEDTLS_MPI_GEN_PRIME_FLAG_LOW_ERR) == 0) {
        /*
         * 2^-80 error probability, number of rounds chosen per HAC, table 4.4
         */
        rounds = ((nbits >= 1300) ?  2 : (nbits >=  850) ?  3 :
                  (nbits >=  650) ?  4 : (nbits >=  350) ?  8 :
                  (nbits >=  250) ? 12 : (nbits >=  150) ? 18 : 27);
    } else {
        /*
         * 2^-100 error probability, number of rounds computed based on HAC,
         * fact 4.48
         */
        rounds = ((nbits >= 1450) ?  4 : (nbits >=  1150) ?  5 :
                  (nbits >= 1000) ?  6 : (nbits >=   850) ?  7 :
                  (nbits >=  750) ?  8 : (nbits >=   500) ? 13 :
                  (nbits >=  250) ? 28 : (nbits >=   150) ? 40 : 51);
    }

    while (1) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_fill_random(X, n * ciL, f_rng, p_rng));
        /* make sure generated number is at least (nbits-1)+0.5 bits (FIPS 186-4 §B.3.3 steps 4.4, 5.5) */
        if (X->p[n-1] < CEIL_MAXUINT_DIV_SQRT2) {
            continue;
        }

        k = n * biL;
        if (k > nbits) {
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(X, k - nbits));
        }
        X->p[0] |= 1;

        if ((flags & MBEDTLS_MPI_GEN_PRIME_FLAG_DH) == 0) {
            ret = mbedtls_mpi_is_prime_ext(X, rounds, f_rng, p_rng);

            if (ret != MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) {
                goto cleanup;
            }
        } else {
            /*
             * A necessary condition for Y and X = 2Y + 1 to be prime
             * is X = 2 mod 3 (which is equivalent to Y = 2 mod 3).
             * Make sure it is satisfied, while keeping X = 3 mod 4
             */

            X->p[0] |= 2;

            MBEDTLS_MPI_CHK(mbedtls_mpi_mod_int(&r, X, 3));
            if (r == 0) {
                MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(X, X, 8));
            } else if (r == 1) {
                MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(X, X, 4));
            }

            /* Set Y = (X-1) / 2, which is X / 2 because X is odd */
            MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&Y, X));
            MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&Y, 1));

            while (1) {
                /*
                 * First, check small factors for X and Y
                 * before doing Miller-Rabin on any of them
                 */
                if ((ret = mpi_check_small_factors(X)) == 0 &&
                    (ret = mpi_check_small_factors(&Y)) == 0 &&
                    (ret = mpi_miller_rabin(X, rounds, f_rng, p_rng))
                    == 0 &&
                    (ret = mpi_miller_rabin(&Y, rounds, f_rng, p_rng))
                    == 0) {
                    goto cleanup;
                }

                if (ret != MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) {
                    goto cleanup;
                }

                /*
                 * Next candidates. We want to preserve Y = (X-1) / 2 and
                 * Y = 1 mod 2 and Y = 2 mod 3 (eq X = 3 mod 4 and X = 2 mod 3)
                 * so up Y by 6 and X by 12.
                 */
                MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(X,  X, 12));
                MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&Y, &Y, 6));
            }
        }
    }

cleanup:

    mbedtls_mpi_free(&Y);

    return ret;
}

#endif /* MBEDTLS_GENPRIME */

#if defined(MBEDTLS_SELF_TEST)

#define GCD_PAIR_COUNT  3

static const int gcd_pairs[GCD_PAIR_COUNT][3] =
{
    { 693, 609, 21 },
    { 1764, 868, 28 },
    { 768454923, 542167814, 1 }
};

/*
 * Checkup routine
 */
int mbedtls_mpi_self_test(int verbose)
{
    int ret, i;
    mbedtls_mpi A, E, N, X, Y, U, V;

    mbedtls_mpi_init(&A); mbedtls_mpi_init(&E); mbedtls_mpi_init(&N); mbedtls_mpi_init(&X);
    mbedtls_mpi_init(&Y); mbedtls_mpi_init(&U); mbedtls_mpi_init(&V);

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&A, 16,
                                            "EFE021C2645FD1DC586E69184AF4A31E" \
                                            "D5F53E93B5F123FA41680867BA110131" \
                                            "944FE7952E2517337780CB0DB80E61AA" \
                                            "E7C8DDC6C5C6AADEB34EB38A2F40D5E6"));

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&E, 16,
                                            "B2E7EFD37075B9F03FF989C7C5051C20" \
                                            "34D2A323810251127E7BF8625A4F49A5" \
                                            "F3E27F4DA8BD59C47D6DAABA4C8127BD" \
                                            "5B5C25763222FEFCCFC38B832366C29E"));

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&N, 16,
                                            "0066A198186C18C10B2F5ED9B522752A" \
                                            "9830B69916E535C8F047518A889A43A5" \
                                            "94B6BED27A168D31D4A52F88925AA8F5"));

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&X, &A, &N));

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&U, 16,
                                            "602AB7ECA597A3D6B56FF9829A5E8B85" \
                                            "9E857EA95A03512E2BAE7391688D264A" \
                                            "A5663B0341DB9CCFD2C4C5F421FEC814" \
                                            "8001B72E848A38CAE1C65F78E56ABDEF" \
                                            "E12D3C039B8A02D6BE593F0BBBDA56F1" \
                                            "ECF677152EF804370C1A305CAF3B5BF1" \
                                            "30879B56C61DE584A0F53A2447A51E"));

    if (verbose != 0) {
        mbedtls_printf("  MPI test #1 (mul_mpi): ");
    }

    if (mbedtls_mpi_cmp_mpi(&X, &U) != 0) {
        if (verbose != 0) {
            mbedtls_printf("failed\n");
        }

        ret = 1;
        goto cleanup;
    }

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_div_mpi(&X, &Y, &A, &N));

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&U, 16,
                                            "256567336059E52CAE22925474705F39A94"));

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&V, 16,
                                            "6613F26162223DF488E9CD48CC132C7A" \
                                            "0AC93C701B001B092E4E5B9F73BCD27B" \
                                            "9EE50D0657C77F374E903CDFA4C642"));

    if (verbose != 0) {
        mbedtls_printf("  MPI test #2 (div_mpi): ");
    }

    if (mbedtls_mpi_cmp_mpi(&X, &U) != 0 ||
        mbedtls_mpi_cmp_mpi(&Y, &V) != 0) {
        if (verbose != 0) {
            mbedtls_printf("failed\n");
        }

        ret = 1;
        goto cleanup;
    }

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&X, &A, &E, &N, NULL));

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&U, 16,
                                            "36E139AEA55215609D2816998ED020BB" \
                                            "BD96C37890F65171D948E9BC7CBAA4D9" \
                                            "325D24D6A3C12710F10A09FA08AB87"));

    if (verbose != 0) {
        mbedtls_printf("  MPI test #3 (exp_mod): ");
    }

    if (mbedtls_mpi_cmp_mpi(&X, &U) != 0) {
        if (verbose != 0) {
            mbedtls_printf("failed\n");
        }

        ret = 1;
        goto cleanup;
    }

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(&X, &A, &N));

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&U, 16,
                                            "003A0AAEDD7E784FC07D8F9EC6E3BFD5" \
                                            "C3DBA76456363A10869622EAC2DD84EC" \
                                            "C5B8A74DAC4D09E03B5E0BE779F2DF61"));

    if (verbose != 0) {
        mbedtls_printf("  MPI test #4 (inv_mod): ");
    }

    if (mbedtls_mpi_cmp_mpi(&X, &U) != 0) {
        if (verbose != 0) {
            mbedtls_printf("failed\n");
        }

        ret = 1;
        goto cleanup;
    }

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }

    if (verbose != 0) {
        mbedtls_printf("  MPI test #5 (simple gcd): ");
    }

    for (i = 0; i < GCD_PAIR_COUNT; i++) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&X, gcd_pairs[i][0]));
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&Y, gcd_pairs[i][1]));

        MBEDTLS_MPI_CHK(mbedtls_mpi_gcd(&A, &X, &Y));

        if (mbedtls_mpi_cmp_int(&A, gcd_pairs[i][2]) != 0) {
            if (verbose != 0) {
                mbedtls_printf("failed at %d\n", i);
            }

            ret = 1;
            goto cleanup;
        }
    }

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }

cleanup:

    if (ret != 0 && verbose != 0) {
        mbedtls_printf("Unexpected error, return code = %08X\n", (unsigned int) ret);
    }

    mbedtls_mpi_free(&A); mbedtls_mpi_free(&E); mbedtls_mpi_free(&N); mbedtls_mpi_free(&X);
    mbedtls_mpi_free(&Y); mbedtls_mpi_free(&U); mbedtls_mpi_free(&V);

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    return ret;
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_BIGNUM_C */
