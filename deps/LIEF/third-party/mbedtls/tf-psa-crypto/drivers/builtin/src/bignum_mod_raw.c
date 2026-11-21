/*
 *  Low-level modular bignum functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_BIGNUM_C) && defined(MBEDTLS_ECP_WITH_MPI_UINT)

#include <string.h>

#include "mbedtls/private/error_common.h"
#include "mbedtls/platform_util.h"

#include "mbedtls/platform.h"

#include "bignum_core.h"
#include "bignum_mod_raw.h"
#include "bignum_mod.h"
#include "constant_time_internal.h"

#include "bignum_mod_raw_invasive.h"

void mbedtls_mpi_mod_raw_cond_assign(mbedtls_mpi_uint *X,
                                     const mbedtls_mpi_uint *A,
                                     const mbedtls_mpi_mod_modulus *N,
                                     unsigned char assign)
{
    mbedtls_mpi_core_cond_assign(X, A, N->limbs, mbedtls_ct_bool(assign));
}

void mbedtls_mpi_mod_raw_cond_swap(mbedtls_mpi_uint *X,
                                   mbedtls_mpi_uint *Y,
                                   const mbedtls_mpi_mod_modulus *N,
                                   unsigned char swap)
{
    mbedtls_mpi_core_cond_swap(X, Y, N->limbs, mbedtls_ct_bool(swap));
}

int mbedtls_mpi_mod_raw_read(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_mod_modulus *N,
                             const unsigned char *input,
                             size_t input_length,
                             mbedtls_mpi_mod_ext_rep ext_rep)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    switch (ext_rep) {
        case MBEDTLS_MPI_MOD_EXT_REP_LE:
            ret = mbedtls_mpi_core_read_le(X, N->limbs,
                                           input, input_length);
            break;
        case MBEDTLS_MPI_MOD_EXT_REP_BE:
            ret = mbedtls_mpi_core_read_be(X, N->limbs,
                                           input, input_length);
            break;
        default:
            return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }

    if (ret != 0) {
        goto cleanup;
    }

    if (!mbedtls_mpi_core_lt_ct(X, N->p, N->limbs)) {
        ret = MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
        goto cleanup;
    }

cleanup:

    return ret;
}

int mbedtls_mpi_mod_raw_write(const mbedtls_mpi_uint *A,
                              const mbedtls_mpi_mod_modulus *N,
                              unsigned char *output,
                              size_t output_length,
                              mbedtls_mpi_mod_ext_rep ext_rep)
{
    switch (ext_rep) {
        case MBEDTLS_MPI_MOD_EXT_REP_LE:
            return mbedtls_mpi_core_write_le(A, N->limbs,
                                             output, output_length);
        case MBEDTLS_MPI_MOD_EXT_REP_BE:
            return mbedtls_mpi_core_write_be(A, N->limbs,
                                             output, output_length);
        default:
            return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }
}

void mbedtls_mpi_mod_raw_sub(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_uint *A,
                             const mbedtls_mpi_uint *B,
                             const mbedtls_mpi_mod_modulus *N)
{
    mbedtls_mpi_uint c = mbedtls_mpi_core_sub(X, A, B, N->limbs);

    (void) mbedtls_mpi_core_add_if(X, N->p, N->limbs, (unsigned) c);
}

MBEDTLS_STATIC_TESTABLE
void mbedtls_mpi_mod_raw_fix_quasi_reduction(mbedtls_mpi_uint *X,
                                             const mbedtls_mpi_mod_modulus *N)
{
    mbedtls_mpi_uint c = mbedtls_mpi_core_sub(X, X, N->p, N->limbs);

    (void) mbedtls_mpi_core_add_if(X, N->p, N->limbs, (unsigned) c);
}


void mbedtls_mpi_mod_raw_mul(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_uint *A,
                             const mbedtls_mpi_uint *B,
                             const mbedtls_mpi_mod_modulus *N,
                             mbedtls_mpi_uint *T)
{
    /* Standard (A * B) multiplication stored into pre-allocated T
     * buffer of fixed limb size of (2N + 1).
     *
     * The space may not not fully filled by when
     * MBEDTLS_MPI_MOD_REP_OPT_RED is used. */
    const size_t T_limbs = BITS_TO_LIMBS(N->bits) * 2;
    switch (N->int_rep) {
        case MBEDTLS_MPI_MOD_REP_MONTGOMERY:
            mbedtls_mpi_core_montmul(X, A, B, N->limbs, N->p, N->limbs,
                                     N->rep.mont.mm, T);
            break;
        case MBEDTLS_MPI_MOD_REP_OPT_RED:
            mbedtls_mpi_core_mul(T, A, N->limbs, B, N->limbs);

            /* Optimised Reduction */
            (*N->rep.ored.modp)(T, T_limbs);

            /* Convert back to canonical representation */
            mbedtls_mpi_mod_raw_fix_quasi_reduction(T, N);
            memcpy(X, T, N->limbs * sizeof(mbedtls_mpi_uint));
            break;
        default:
            break;
    }

}

size_t mbedtls_mpi_mod_raw_inv_prime_working_limbs(size_t AN_limbs)
{
    /* mbedtls_mpi_mod_raw_inv_prime() needs a temporary for the exponent,
     * which will be the same size as the modulus and input (AN_limbs),
     * and additional space to pass to mbedtls_mpi_core_exp_mod(). */
    return AN_limbs +
           mbedtls_mpi_core_exp_mod_working_limbs(AN_limbs, AN_limbs);
}

void mbedtls_mpi_mod_raw_inv_prime(mbedtls_mpi_uint *X,
                                   const mbedtls_mpi_uint *A,
                                   const mbedtls_mpi_uint *N,
                                   size_t AN_limbs,
                                   const mbedtls_mpi_uint *RR,
                                   mbedtls_mpi_uint *T)
{
    /* Inversion by power: g^|G| = 1 => g^(-1) = g^(|G|-1), and
     *                       |G| = N - 1, so we want
     *                 g^(|G|-1) = g^(N - 2)
     */

    /* Use the first AN_limbs of T to hold N - 2 */
    mbedtls_mpi_uint *Nminus2 = T;
    (void) mbedtls_mpi_core_sub_int(Nminus2, N, 2, AN_limbs);

    /* Rest of T is given to exp_mod for its working space */
    mbedtls_mpi_core_exp_mod(X,
                             A, N, AN_limbs, Nminus2, AN_limbs,
                             RR, T + AN_limbs);
}

void mbedtls_mpi_mod_raw_add(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_uint *A,
                             const mbedtls_mpi_uint *B,
                             const mbedtls_mpi_mod_modulus *N)
{
    mbedtls_mpi_uint carry, borrow;
    carry  = mbedtls_mpi_core_add(X, A, B, N->limbs);
    borrow = mbedtls_mpi_core_sub(X, X, N->p, N->limbs);
    (void) mbedtls_mpi_core_add_if(X, N->p, N->limbs, (unsigned) (carry ^ borrow));
}

int mbedtls_mpi_mod_raw_canonical_to_modulus_rep(
    mbedtls_mpi_uint *X,
    const mbedtls_mpi_mod_modulus *N)
{
    switch (N->int_rep) {
        case MBEDTLS_MPI_MOD_REP_MONTGOMERY:
            return mbedtls_mpi_mod_raw_to_mont_rep(X, N);
        case MBEDTLS_MPI_MOD_REP_OPT_RED:
            return 0;
        default:
            return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }
}

int mbedtls_mpi_mod_raw_modulus_to_canonical_rep(
    mbedtls_mpi_uint *X,
    const mbedtls_mpi_mod_modulus *N)
{
    switch (N->int_rep) {
        case MBEDTLS_MPI_MOD_REP_MONTGOMERY:
            return mbedtls_mpi_mod_raw_from_mont_rep(X, N);
        case MBEDTLS_MPI_MOD_REP_OPT_RED:
            return 0;
        default:
            return MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
    }
}

int mbedtls_mpi_mod_raw_random(mbedtls_mpi_uint *X,
                               mbedtls_mpi_uint min,
                               const mbedtls_mpi_mod_modulus *N,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng)
{
    int ret = mbedtls_mpi_core_random(X, min, N->p, N->limbs, f_rng, p_rng);
    if (ret != 0) {
        return ret;
    }
    return mbedtls_mpi_mod_raw_canonical_to_modulus_rep(X, N);
}

int mbedtls_mpi_mod_raw_to_mont_rep(mbedtls_mpi_uint *X,
                                    const mbedtls_mpi_mod_modulus *N)
{
    mbedtls_mpi_uint *T;
    const size_t t_limbs = mbedtls_mpi_core_montmul_working_limbs(N->limbs);

    if ((T = (mbedtls_mpi_uint *) mbedtls_calloc(t_limbs, ciL)) == NULL) {
        return MBEDTLS_ERR_MPI_ALLOC_FAILED;
    }

    mbedtls_mpi_core_to_mont_rep(X, X, N->p, N->limbs,
                                 N->rep.mont.mm, N->rep.mont.rr, T);

    mbedtls_zeroize_and_free(T, t_limbs * ciL);
    return 0;
}

int mbedtls_mpi_mod_raw_from_mont_rep(mbedtls_mpi_uint *X,
                                      const mbedtls_mpi_mod_modulus *N)
{
    const size_t t_limbs = mbedtls_mpi_core_montmul_working_limbs(N->limbs);
    mbedtls_mpi_uint *T;

    if ((T = (mbedtls_mpi_uint *) mbedtls_calloc(t_limbs, ciL)) == NULL) {
        return MBEDTLS_ERR_MPI_ALLOC_FAILED;
    }

    mbedtls_mpi_core_from_mont_rep(X, X, N->p, N->limbs, N->rep.mont.mm, T);

    mbedtls_zeroize_and_free(T, t_limbs * ciL);
    return 0;
}

void mbedtls_mpi_mod_raw_neg(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_uint *A,
                             const mbedtls_mpi_mod_modulus *N)
{
    mbedtls_mpi_core_sub(X, N->p, A, N->limbs);

    /* If A=0 initially, then X=N now. Detect this by
     * subtracting N and catching the carry. */
    mbedtls_mpi_uint borrow = mbedtls_mpi_core_sub(X, X, N->p, N->limbs);
    (void) mbedtls_mpi_core_add_if(X, N->p, N->limbs, (unsigned) borrow);
}

#endif /* MBEDTLS_BIGNUM_C && MBEDTLS_ECP_WITH_MPI_UINT */
