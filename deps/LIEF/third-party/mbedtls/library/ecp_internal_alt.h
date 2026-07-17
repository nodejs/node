/**
 * \file ecp_internal_alt.h
 *
 * \brief Function declarations for alternative implementation of elliptic curve
 * point arithmetic.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * References:
 *
 * [1] BERNSTEIN, Daniel J. Curve25519: new Diffie-Hellman speed records.
 *     <http://cr.yp.to/ecdh/curve25519-20060209.pdf>
 *
 * [2] CORON, Jean-S'ebastien. Resistance against differential power analysis
 *     for elliptic curve cryptosystems. In : Cryptographic Hardware and
 *     Embedded Systems. Springer Berlin Heidelberg, 1999. p. 292-302.
 *     <http://link.springer.com/chapter/10.1007/3-540-48059-5_25>
 *
 * [3] HEDABOU, Mustapha, PINEL, Pierre, et B'EN'ETEAU, Lucien. A comb method to
 *     render ECC resistant against Side Channel Attacks. IACR Cryptology
 *     ePrint Archive, 2004, vol. 2004, p. 342.
 *     <http://eprint.iacr.org/2004/342.pdf>
 *
 * [4] Certicom Research. SEC 2: Recommended Elliptic Curve Domain Parameters.
 *     <http://www.secg.org/sec2-v2.pdf>
 *
 * [5] HANKERSON, Darrel, MENEZES, Alfred J., VANSTONE, Scott. Guide to Elliptic
 *     Curve Cryptography.
 *
 * [6] Digital Signature Standard (DSS), FIPS 186-4.
 *     <http://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.186-4.pdf>
 *
 * [7] Elliptic Curve Cryptography (ECC) Cipher Suites for Transport Layer
 *     Security (TLS), RFC 4492.
 *     <https://tools.ietf.org/search/rfc4492>
 *
 * [8] <http://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html>
 *
 * [9] COHEN, Henri. A Course in Computational Algebraic Number Theory.
 *     Springer Science & Business Media, 1 Aug 2000
 */

#ifndef MBEDTLS_ECP_INTERNAL_H
#define MBEDTLS_ECP_INTERNAL_H

#include "mbedtls/build_info.h"

#if defined(MBEDTLS_ECP_INTERNAL_ALT)

/**
 * \brief           Indicate if the Elliptic Curve Point module extension can
 *                  handle the group.
 *
 * \param grp       The pointer to the elliptic curve group that will be the
 *                  basis of the cryptographic computations.
 *
 * \return          Non-zero if successful.
 */
unsigned char mbedtls_internal_ecp_grp_capable(const mbedtls_ecp_group *grp);

/**
 * \brief           Initialise the Elliptic Curve Point module extension.
 *
 *                  If mbedtls_internal_ecp_grp_capable returns true for a
 *                  group, this function has to be able to initialise the
 *                  module for it.
 *
 *                  This module can be a driver to a crypto hardware
 *                  accelerator, for which this could be an initialise function.
 *
 * \param grp       The pointer to the group the module needs to be
 *                  initialised for.
 *
 * \return          0 if successful.
 */
int mbedtls_internal_ecp_init(const mbedtls_ecp_group *grp);

/**
 * \brief           Frees and deallocates the Elliptic Curve Point module
 *                  extension.
 *
 * \param grp       The pointer to the group the module was initialised for.
 */
void mbedtls_internal_ecp_free(const mbedtls_ecp_group *grp);

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)

#if defined(MBEDTLS_ECP_RANDOMIZE_JAC_ALT)
/**
 * \brief           Randomize jacobian coordinates:
 *                  (X, Y, Z) -> (l^2 X, l^3 Y, l Z) for random l.
 *
 * \param grp       Pointer to the group representing the curve.
 *
 * \param pt        The point on the curve to be randomised, given with Jacobian
 *                  coordinates.
 *
 * \param f_rng     A function pointer to the random number generator.
 *
 * \param p_rng     A pointer to the random number generator state.
 *
 * \return          0 if successful.
 */
int mbedtls_internal_ecp_randomize_jac(const mbedtls_ecp_group *grp,
                                       mbedtls_ecp_point *pt, int (*f_rng)(void *,
                                                                           unsigned char *,
                                                                           size_t),
                                       void *p_rng);
#endif

#if defined(MBEDTLS_ECP_ADD_MIXED_ALT)
/**
 * \brief           Addition: R = P + Q, mixed affine-Jacobian coordinates.
 *
 *                  The coordinates of Q must be normalized (= affine),
 *                  but those of P don't need to. R is not normalized.
 *
 *                  This function is used only as a subrutine of
 *                  ecp_mul_comb().
 *
 *                  Special cases: (1) P or Q is zero, (2) R is zero,
 *                      (3) P == Q.
 *                  None of these cases can happen as intermediate step in
 *                  ecp_mul_comb():
 *                      - at each step, P, Q and R are multiples of the base
 *                      point, the factor being less than its order, so none of
 *                      them is zero;
 *                      - Q is an odd multiple of the base point, P an even
 *                      multiple, due to the choice of precomputed points in the
 *                      modified comb method.
 *                  So branches for these cases do not leak secret information.
 *
 *                  We accept Q->Z being unset (saving memory in tables) as
 *                  meaning 1.
 *
 *                  Cost in field operations if done by [5] 3.22:
 *                      1A := 8M + 3S
 *
 * \param grp       Pointer to the group representing the curve.
 *
 * \param R         Pointer to a point structure to hold the result.
 *
 * \param P         Pointer to the first summand, given with Jacobian
 *                  coordinates
 *
 * \param Q         Pointer to the second summand, given with affine
 *                  coordinates.
 *
 * \return          0 if successful.
 */
int mbedtls_internal_ecp_add_mixed(const mbedtls_ecp_group *grp,
                                   mbedtls_ecp_point *R, const mbedtls_ecp_point *P,
                                   const mbedtls_ecp_point *Q);
#endif

/**
 * \brief           Point doubling R = 2 P, Jacobian coordinates.
 *
 *                  Cost:   1D := 3M + 4S    (A ==  0)
 *                          4M + 4S          (A == -3)
 *                          3M + 6S + 1a     otherwise
 *                  when the implementation is based on the "dbl-1998-cmo-2"
 *                  doubling formulas in [8] and standard optimizations are
 *                  applied when curve parameter A is one of { 0, -3 }.
 *
 * \param grp       Pointer to the group representing the curve.
 *
 * \param R         Pointer to a point structure to hold the result.
 *
 * \param P         Pointer to the point that has to be doubled, given with
 *                  Jacobian coordinates.
 *
 * \return          0 if successful.
 */
#if defined(MBEDTLS_ECP_DOUBLE_JAC_ALT)
int mbedtls_internal_ecp_double_jac(const mbedtls_ecp_group *grp,
                                    mbedtls_ecp_point *R, const mbedtls_ecp_point *P);
#endif

/**
 * \brief           Normalize jacobian coordinates of an array of (pointers to)
 *                  points.
 *
 *                  Using Montgomery's trick to perform only one inversion mod P
 *                  the cost is:
 *                      1N(t) := 1I + (6t - 3)M + 1S
 *                  (See for example Algorithm 10.3.4. in [9])
 *
 *                  This function is used only as a subrutine of
 *                  ecp_mul_comb().
 *
 *                  Warning: fails (returning an error) if one of the points is
 *                  zero!
 *                  This should never happen, see choice of w in ecp_mul_comb().
 *
 * \param grp       Pointer to the group representing the curve.
 *
 * \param T         Array of pointers to the points to normalise.
 *
 * \param t_len     Number of elements in the array.
 *
 * \return          0 if successful,
 *                      an error if one of the points is zero.
 */
#if defined(MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT)
int mbedtls_internal_ecp_normalize_jac_many(const mbedtls_ecp_group *grp,
                                            mbedtls_ecp_point *T[], size_t t_len);
#endif

/**
 * \brief           Normalize jacobian coordinates so that Z == 0 || Z == 1.
 *
 *                  Cost in field operations if done by [5] 3.2.1:
 *                      1N := 1I + 3M + 1S
 *
 * \param grp       Pointer to the group representing the curve.
 *
 * \param pt        pointer to the point to be normalised. This is an
 *                  input/output parameter.
 *
 * \return          0 if successful.
 */
#if defined(MBEDTLS_ECP_NORMALIZE_JAC_ALT)
int mbedtls_internal_ecp_normalize_jac(const mbedtls_ecp_group *grp,
                                       mbedtls_ecp_point *pt);
#endif

#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)

#if defined(MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT)
int mbedtls_internal_ecp_double_add_mxz(const mbedtls_ecp_group *grp,
                                        mbedtls_ecp_point *R,
                                        mbedtls_ecp_point *S,
                                        const mbedtls_ecp_point *P,
                                        const mbedtls_ecp_point *Q,
                                        const mbedtls_mpi *d);
#endif

/**
 * \brief           Randomize projective x/z coordinates:
 *                      (X, Z) -> (l X, l Z) for random l
 *
 * \param grp       pointer to the group representing the curve
 *
 * \param P         the point on the curve to be randomised given with
 *                  projective coordinates. This is an input/output parameter.
 *
 * \param f_rng     a function pointer to the random number generator
 *
 * \param p_rng     a pointer to the random number generator state
 *
 * \return          0 if successful
 */
#if defined(MBEDTLS_ECP_RANDOMIZE_MXZ_ALT)
int mbedtls_internal_ecp_randomize_mxz(const mbedtls_ecp_group *grp,
                                       mbedtls_ecp_point *P, int (*f_rng)(void *,
                                                                          unsigned char *,
                                                                          size_t),
                                       void *p_rng);
#endif

/**
 * \brief           Normalize Montgomery x/z coordinates: X = X/Z, Z = 1.
 *
 * \param grp       pointer to the group representing the curve
 *
 * \param P         pointer to the point to be normalised. This is an
 *                  input/output parameter.
 *
 * \return          0 if successful
 */
#if defined(MBEDTLS_ECP_NORMALIZE_MXZ_ALT)
int mbedtls_internal_ecp_normalize_mxz(const mbedtls_ecp_group *grp,
                                       mbedtls_ecp_point *P);
#endif

#endif /* MBEDTLS_ECP_MONTGOMERY_ENABLED */

#endif /* MBEDTLS_ECP_INTERNAL_ALT */

#endif /* ecp_internal_alt.h */
