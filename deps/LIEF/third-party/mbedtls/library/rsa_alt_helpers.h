/**
 * \file rsa_alt_helpers.h
 *
 * \brief Context-independent RSA helper functions
 *
 *  This module declares some RSA-related helper functions useful when
 *  implementing the RSA interface. These functions are provided in a separate
 *  compilation unit in order to make it easy for designers of alternative RSA
 *  implementations to use them in their own code, as it is conceived that the
 *  functionality they provide will be necessary for most complete
 *  implementations.
 *
 *  End-users of Mbed TLS who are not providing their own alternative RSA
 *  implementations should not use these functions directly, and should instead
 *  use only the functions declared in rsa.h.
 *
 *  The interface provided by this module will be maintained through LTS (Long
 *  Term Support) branches of Mbed TLS, but may otherwise be subject to change,
 *  and must be considered an internal interface of the library.
 *
 *  There are two classes of helper functions:
 *
 *  (1) Parameter-generating helpers. These are:
 *      - mbedtls_rsa_deduce_primes
 *      - mbedtls_rsa_deduce_private_exponent
 *      - mbedtls_rsa_deduce_crt
 *       Each of these functions takes a set of core RSA parameters and
 *       generates some other, or CRT related parameters.
 *
 *  (2) Parameter-checking helpers. These are:
 *      - mbedtls_rsa_validate_params
 *      - mbedtls_rsa_validate_crt
 *      They take a set of core or CRT related RSA parameters and check their
 *      validity.
 *
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_RSA_ALT_HELPERS_H
#define MBEDTLS_RSA_ALT_HELPERS_H

#include "mbedtls/build_info.h"

#include "mbedtls/bignum.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * \brief          Compute RSA prime moduli P, Q from public modulus N=PQ
 *                 and a pair of private and public key.
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param N        RSA modulus N = PQ, with P, Q to be found
 * \param E        RSA public exponent
 * \param D        RSA private exponent
 * \param P        Pointer to MPI holding first prime factor of N on success
 * \param Q        Pointer to MPI holding second prime factor of N on success
 *
 * \return
 *                 - 0 if successful. In this case, P and Q constitute a
 *                   factorization of N.
 *                 - A non-zero error code otherwise.
 *
 * \note           It is neither checked that P, Q are prime nor that
 *                 D, E are modular inverses wrt. P-1 and Q-1. For that,
 *                 use the helper function \c mbedtls_rsa_validate_params.
 *
 */
int mbedtls_rsa_deduce_primes(mbedtls_mpi const *N, mbedtls_mpi const *E,
                              mbedtls_mpi const *D,
                              mbedtls_mpi *P, mbedtls_mpi *Q);

/**
 * \brief          Compute RSA private exponent from
 *                 prime moduli and public key.
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param P        First prime factor of RSA modulus
 * \param Q        Second prime factor of RSA modulus
 * \param E        RSA public exponent
 * \param D        Pointer to MPI holding the private exponent on success.
 *
 * \return
 *                 - 0 if successful. In this case, D is set to a simultaneous
 *                   modular inverse of E modulo both P-1 and Q-1.
 *                 - A non-zero error code otherwise.
 *
 * \note           This function does not check whether P and Q are primes.
 *
 */
int mbedtls_rsa_deduce_private_exponent(mbedtls_mpi const *P,
                                        mbedtls_mpi const *Q,
                                        mbedtls_mpi const *E,
                                        mbedtls_mpi *D);


/**
 * \brief          Generate RSA-CRT parameters
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param P        First prime factor of N
 * \param Q        Second prime factor of N
 * \param D        RSA private exponent
 * \param DP       Output variable for D modulo P-1
 * \param DQ       Output variable for D modulo Q-1
 * \param QP       Output variable for the modular inverse of Q modulo P.
 *
 * \return         0 on success, non-zero error code otherwise.
 *
 * \note           This function does not check whether P, Q are
 *                 prime and whether D is a valid private exponent.
 *
 */
int mbedtls_rsa_deduce_crt(const mbedtls_mpi *P, const mbedtls_mpi *Q,
                           const mbedtls_mpi *D, mbedtls_mpi *DP,
                           mbedtls_mpi *DQ, mbedtls_mpi *QP);


/**
 * \brief          Check validity of core RSA parameters
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param N        RSA modulus N = PQ
 * \param P        First prime factor of N
 * \param Q        Second prime factor of N
 * \param D        RSA private exponent
 * \param E        RSA public exponent
 * \param f_rng    PRNG to be used for primality check, or NULL
 * \param p_rng    PRNG context for f_rng, or NULL
 *
 * \return
 *                 - 0 if the following conditions are satisfied
 *                   if all relevant parameters are provided:
 *                    - P prime if f_rng != NULL (%)
 *                    - Q prime if f_rng != NULL (%)
 *                    - 1 < N = P * Q
 *                    - 1 < D, E < N
 *                    - D and E are modular inverses modulo P-1 and Q-1
 *                   (%) This is only done if MBEDTLS_GENPRIME is defined.
 *                 - A non-zero error code otherwise.
 *
 * \note           The function can be used with a restricted set of arguments
 *                 to perform specific checks only. E.g., calling it with
 *                 (-,P,-,-,-) and a PRNG amounts to a primality check for P.
 */
int mbedtls_rsa_validate_params(const mbedtls_mpi *N, const mbedtls_mpi *P,
                                const mbedtls_mpi *Q, const mbedtls_mpi *D,
                                const mbedtls_mpi *E,
                                int (*f_rng)(void *, unsigned char *, size_t),
                                void *p_rng);

/**
 * \brief          Check validity of RSA CRT parameters
 *
 * \note           This is a 'static' helper function not operating on
 *                 an RSA context. Alternative implementations need not
 *                 overwrite it.
 *
 * \param P        First prime factor of RSA modulus
 * \param Q        Second prime factor of RSA modulus
 * \param D        RSA private exponent
 * \param DP       MPI to check for D modulo P-1
 * \param DQ       MPI to check for D modulo P-1
 * \param QP       MPI to check for the modular inverse of Q modulo P.
 *
 * \return
 *                 - 0 if the following conditions are satisfied:
 *                    - D = DP mod P-1 if P, D, DP != NULL
 *                    - Q = DQ mod P-1 if P, D, DQ != NULL
 *                    - QP = Q^-1 mod P if P, Q, QP != NULL
 *                 - \c MBEDTLS_ERR_RSA_KEY_CHECK_FAILED if check failed,
 *                   potentially including \c MBEDTLS_ERR_MPI_XXX if some
 *                   MPI calculations failed.
 *                 - \c MBEDTLS_ERR_RSA_BAD_INPUT_DATA if insufficient
 *                   data was provided to check DP, DQ or QP.
 *
 * \note           The function can be used with a restricted set of arguments
 *                 to perform specific checks only. E.g., calling it with the
 *                 parameters (P, -, D, DP, -, -) will check DP = D mod P-1.
 */
int mbedtls_rsa_validate_crt(const mbedtls_mpi *P,  const mbedtls_mpi *Q,
                             const mbedtls_mpi *D,  const mbedtls_mpi *DP,
                             const mbedtls_mpi *DQ, const mbedtls_mpi *QP);

#ifdef __cplusplus
}
#endif

#endif /* rsa_alt_helpers.h */
