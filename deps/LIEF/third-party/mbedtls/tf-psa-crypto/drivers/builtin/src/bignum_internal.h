/**
 * \file bignum_internal.h
 *
 * \brief Internal-only bignum public-key cryptosystem API.
 *
 * This file declares bignum-related functions that are to be used
 * only from within the Mbed TLS library itself.
 *
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_BIGNUM_INTERNAL_H
#define MBEDTLS_BIGNUM_INTERNAL_H

/**
 * \brief          Perform a modular exponentiation: X = A^E mod N
 *
 * \warning        This function is not constant time with respect to \p E (the exponent).
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 *                 This must not alias E or N.
 * \param A        The base of the exponentiation.
 *                 This must point to an initialized MPI.
 * \param E        The exponent MPI. This must point to an initialized MPI.
 * \param N        The base for the modular reduction. This must point to an
 *                 initialized MPI.
 * \param prec_RR  A helper MPI depending solely on \p N which can be used to
 *                 speed-up multiple modular exponentiations for the same value
 *                 of \p N. This may be \c NULL. If it is not \c NULL, it must
 *                 point to an initialized MPI. If it hasn't been used after
 *                 the call to mbedtls_mpi_init(), this function will compute
 *                 the helper value and store it in \p prec_RR for reuse on
 *                 subsequent calls to this function. Otherwise, the function
 *                 will assume that \p prec_RR holds the helper value set by a
 *                 previous call to mbedtls_mpi_exp_mod(), and reuse it.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \c N is negative or
 *                 even, or if \c E is negative.
 * \return         Another negative error code on different kinds of failures.
 *
 */
int mbedtls_mpi_exp_mod_unsafe(mbedtls_mpi *X, const mbedtls_mpi *A,
                               const mbedtls_mpi *E, const mbedtls_mpi *N,
                               mbedtls_mpi *prec_RR);

/**
 * \brief          A wrapper around a constant time function to compute
 *                 GCD(A, N) and/or A^-1 mod N if it exists.
 *
 * \warning        Requires N to be odd, and 0 <= A <= N. Additionally, if
 *                 I != NULL, requires N > 1.
 *                 The wrapper part of this function is not constant time.
 *
 * \note           A and N must not alias each other.
 *                 When I == NULL (computing only the GCD), G can alias A or N.
 *                 When I != NULL (computing the modular inverse), G or I can
 *                 alias A, but neither of them can alias N (the modulus).
 *
 * \param[out] G   The GCD of \p A and \p N.
 *                 This may be NULL, to only compute I.
 * \param[out] I   The inverse of \p A modulo \p N if it exists (that is,
 *                 if \p G above is 1 on exit), in the range [1, \p N);
 *                 indeterminate otherwise.
 *                 This may be NULL, to only compute G.
 * \param[in] A    The 1st operand of GCD and number to invert.
 *                 This value must be less than or equal to \p N.
 * \param[in] N    The 2nd operand of GCD and modulus for inversion.
 *                 Must be odd or the results are indeterminate.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if preconditions were not
 *                 met.
 */
int mbedtls_mpi_gcd_modinv_odd(mbedtls_mpi *G,
                               mbedtls_mpi *I,
                               const mbedtls_mpi *A,
                               const mbedtls_mpi *N);

/**
 * \brief          Modular inverse: X = A^-1 mod N with N odd
 *
 * \param[out] X   The inverse of \p A modulo \p N in the range [1, \p N)
 *                 on success; indeterminate otherwise.
 * \param[in] A    The number to invert.
 * \param[in] N    The modulus. Must be odd and greater than 1.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if preconditions were not
 *                 met.
 * \return         #MBEDTLS_ERR_MPI_NOT_ACCEPTABLE if A is not invertible mod N.
 */
int mbedtls_mpi_inv_mod_odd(mbedtls_mpi *X,
                            const mbedtls_mpi *A,
                            const mbedtls_mpi *N);

/**
 * \brief          Modular inverse: X = A^-1 mod N with N even,
 *                 A odd and 1 < A < N.
 *
 * \param[out] X   The inverse of \p A modulo \p N in the range [1, \p N)
 *                 on success; indeterminate otherwise.
 * \param[in] A    The number to invert. Must be odd, greated than 1
 *                 and less than \p N.
 * \param[in] N    The modulus. Must be even and greater than 1.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if preconditions were not
 *                 met.
 * \return         #MBEDTLS_ERR_MPI_NOT_ACCEPTABLE if A is not invertible mod N.
 */
int mbedtls_mpi_inv_mod_even_in_range(mbedtls_mpi *X,
                                      mbedtls_mpi const *A,
                                      mbedtls_mpi const *N);

#endif /* bignum_internal.h */
