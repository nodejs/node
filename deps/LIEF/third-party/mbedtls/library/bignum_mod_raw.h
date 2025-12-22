/**
 *  Low-level modular bignum functions
 *
 *  This interface should only be used by the higher-level modular bignum
 *  module (bignum_mod.c) and the ECP module (ecp.c, ecp_curves.c). All other
 *  modules should use the high-level modular bignum interface (bignum_mod.h)
 *  or the legacy bignum interface (bignum.h).
 *
 * This is a low-level interface to operations on integers modulo which
 * has no protection against passing invalid arguments such as arrays of
 * the wrong size. The functions in bignum_mod.h provide a higher-level
 * interface that includes protections against accidental misuse, at the
 * expense of code size and sometimes more cumbersome memory management.
 *
 * The functions in this module obey the following conventions unless
 * explicitly indicated otherwise:
 * - **Modulus parameters**: the modulus is passed as a pointer to a structure
 *   of type #mbedtls_mpi_mod_modulus. The structure must be set up with an
 *   array of limbs storing the bignum value of the modulus. The modulus must
 *   be odd and is assumed to have no leading zeroes. The modulus is usually
 *   named \c N and is usually input-only.
 * - **Bignum parameters**: Bignums are passed as pointers to an array of
 *   limbs. A limb has the type #mbedtls_mpi_uint. Unless otherwise specified:
 *     - Bignum parameters called \c A, \c B, ... are inputs, and are not
 *       modified by the function.
 *     - Bignum parameters called \c X, \c Y are outputs or input-output.
 *       The initial content of output-only parameters is ignored.
 *     - \c T is a temporary storage area. The initial content of such a
 *       parameter is ignored and the final content is unspecified.
 * - **Bignum sizes**: bignum sizes are usually expressed by the \c limbs
 *   member of the modulus argument. All bignum parameters must have the same
 *   number of limbs as the modulus. All bignum sizes must be at least 1 and
 *   must be significantly less than #SIZE_MAX. The behavior if a size is 0 is
 *   undefined.
 * - **Bignum representation**: the representation of inputs and outputs is
 *   specified by the \c int_rep field of the modulus for arithmetic
 *   functions. Utility functions may allow for different representation.
 * - **Parameter ordering**: for bignum parameters, outputs come before inputs.
 *   The modulus is passed after other bignum input parameters. Temporaries
 *   come last.
 * - **Aliasing**: in general, output bignums may be aliased to one or more
 *   inputs. Modulus values may not be aliased to any other parameter. Outputs
 *   may not be aliased to one another. Temporaries may not be aliased to any
 *   other parameter.
 * - **Overlap**: apart from aliasing of limb array pointers (where two
 *   arguments are equal pointers), overlap is not supported and may result
 *   in undefined behavior.
 * - **Error handling**: This is a low-level module. Functions generally do not
 *   try to protect against invalid arguments such as nonsensical sizes or
 *   null pointers. Note that passing bignums with a different size than the
 *   modulus may lead to buffer overflows. Some functions which allocate
 *   memory or handle reading/writing of bignums will return an error if
 *   memory allocation fails or if buffer sizes are invalid.
 * - **Modular representatives**: all functions expect inputs to be in the
 *   range [0, \c N - 1] and guarantee outputs in the range [0, \c N - 1]. If
 *   an input is out of range, outputs are fully unspecified, though bignum
 *   values out of range should not cause buffer overflows (beware that this is
 *   not extensively tested).
 */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_BIGNUM_MOD_RAW_H
#define MBEDTLS_BIGNUM_MOD_RAW_H

#include "common.h"

#if defined(MBEDTLS_BIGNUM_C)
#include "mbedtls/bignum.h"
#endif

#include "bignum_mod.h"

/**
 * \brief   Perform a safe conditional copy of an MPI which doesn't reveal
 *          whether the assignment was done or not.
 *
 * The size to copy is determined by \p N.
 *
 * \param[out] X        The address of the destination MPI.
 *                      This must be initialized. Must have enough limbs to
 *                      store the full value of \p A.
 * \param[in]  A        The address of the source MPI. This must be initialized.
 * \param[in]  N        The address of the modulus related to \p X and \p A.
 * \param      assign   The condition deciding whether to perform the
 *                      assignment or not. Must be either 0 or 1:
 *                      * \c 1: Perform the assignment `X = A`.
 *                      * \c 0: Keep the original value of \p X.
 *
 * \note           This function avoids leaking any information about whether
 *                 the assignment was done or not.
 *
 * \warning        If \p assign is neither 0 nor 1, the result of this function
 *                 is indeterminate, and the resulting value in \p X might be
 *                 neither its original value nor the value in \p A.
 */
void mbedtls_mpi_mod_raw_cond_assign(mbedtls_mpi_uint *X,
                                     const mbedtls_mpi_uint *A,
                                     const mbedtls_mpi_mod_modulus *N,
                                     unsigned char assign);

/**
 * \brief   Perform a safe conditional swap of two MPIs which doesn't reveal
 *          whether the swap was done or not.
 *
 * The size to swap is determined by \p N.
 *
 * \param[in,out] X     The address of the first MPI. This must be initialized.
 * \param[in,out] Y     The address of the second MPI. This must be initialized.
 * \param[in]     N     The address of the modulus related to \p X and \p Y.
 * \param         swap  The condition deciding whether to perform
 *                      the swap or not. Must be either 0 or 1:
 *                      * \c 1: Swap the values of \p X and \p Y.
 *                      * \c 0: Keep the original values of \p X and \p Y.
 *
 * \note           This function avoids leaking any information about whether
 *                 the swap was done or not.
 *
 * \warning        If \p swap is neither 0 nor 1, the result of this function
 *                 is indeterminate, and both \p X and \p Y might end up with
 *                 values different to either of the original ones.
 */
void mbedtls_mpi_mod_raw_cond_swap(mbedtls_mpi_uint *X,
                                   mbedtls_mpi_uint *Y,
                                   const mbedtls_mpi_mod_modulus *N,
                                   unsigned char swap);

/** Import X from unsigned binary data.
 *
 * The MPI needs to have enough limbs to store the full value (including any
 * most significant zero bytes in the input).
 *
 * \param[out] X        The address of the MPI. The size is determined by \p N.
 *                      (In particular, it must have at least as many limbs as
 *                      the modulus \p N.)
 * \param[in] N         The address of the modulus related to \p X.
 * \param[in] input     The input buffer to import from.
 * \param input_length  The length in bytes of \p input.
 * \param ext_rep       The endianness of the number in the input buffer.
 *
 * \return       \c 0 if successful.
 * \return       #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p X isn't
 *               large enough to hold the value in \p input.
 * \return       #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if the external representation
 *               of \p N is invalid or \p X is not less than \p N.
 */
int mbedtls_mpi_mod_raw_read(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_mod_modulus *N,
                             const unsigned char *input,
                             size_t input_length,
                             mbedtls_mpi_mod_ext_rep ext_rep);

/** Export A into unsigned binary data.
 *
 * \param[in] A         The address of the MPI. The size is determined by \p N.
 *                      (In particular, it must have at least as many limbs as
 *                      the modulus \p N.)
 * \param[in] N         The address of the modulus related to \p A.
 * \param[out] output   The output buffer to export to.
 * \param output_length The length in bytes of \p output.
 * \param ext_rep       The endianness in which the number should be written into the output buffer.
 *
 * \return       \c 0 if successful.
 * \return       #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p output isn't
 *               large enough to hold the value of \p A.
 * \return       #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if the external representation
 *               of \p N is invalid.
 */
int mbedtls_mpi_mod_raw_write(const mbedtls_mpi_uint *A,
                              const mbedtls_mpi_mod_modulus *N,
                              unsigned char *output,
                              size_t output_length,
                              mbedtls_mpi_mod_ext_rep ext_rep);

/** \brief  Subtract two MPIs, returning the residue modulo the specified
 *          modulus.
 *
 * The size of the operation is determined by \p N. \p A and \p B must have
 * the same number of limbs as \p N.
 *
 * \p X may be aliased to \p A or \p B, or even both, but may not overlap
 * either otherwise.
 *
 * \param[out] X        The address of the result MPI.
 *                      This must be initialized. Must have enough limbs to
 *                      store the full value of the result.
 * \param[in]  A        The address of the first MPI. This must be initialized.
 * \param[in]  B        The address of the second MPI. This must be initialized.
 * \param[in]  N        The address of the modulus. Used to perform a modulo
 *                      operation on the result of the subtraction.
 */
void mbedtls_mpi_mod_raw_sub(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_uint *A,
                             const mbedtls_mpi_uint *B,
                             const mbedtls_mpi_mod_modulus *N);

/** \brief  Multiply two MPIs, returning the residue modulo the specified
 *          modulus.
 *
 * \note Currently handles the case when `N->int_rep` is
 * MBEDTLS_MPI_MOD_REP_MONTGOMERY.
 *
 * The size of the operation is determined by \p N. \p A, \p B and \p X must
 * all be associated with the modulus \p N and must all have the same number
 * of limbs as \p N.
 *
 * \p X may be aliased to \p A or \p B, or even both, but may not overlap
 * either otherwise. They may not alias \p N (since they must be in canonical
 * form, they cannot == \p N).
 *
 * \param[out] X        The address of the result MPI. Must have the same
 *                      number of limbs as \p N.
 *                      On successful completion, \p X contains the result of
 *                      the multiplication `A * B * R^-1` mod N where
 *                      `R = 2^(biL * N->limbs)`.
 * \param[in]  A        The address of the first MPI.
 * \param[in]  B        The address of the second MPI.
 * \param[in]  N        The address of the modulus. Used to perform a modulo
 *                      operation on the result of the multiplication.
 * \param[in,out] T     Temporary storage of size at least 2 * N->limbs + 1
 *                      limbs. Its initial content is unused and
 *                      its final content is indeterminate.
 *                      It must not alias or otherwise overlap any of the
 *                      other parameters.
 */
void mbedtls_mpi_mod_raw_mul(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_uint *A,
                             const mbedtls_mpi_uint *B,
                             const mbedtls_mpi_mod_modulus *N,
                             mbedtls_mpi_uint *T);

/**
 * \brief          Returns the number of limbs of working memory required for
 *                 a call to `mbedtls_mpi_mod_raw_inv_prime()`.
 *
 * \note           This will always be at least
 *                 `mbedtls_mpi_core_montmul_working_limbs(AN_limbs)`,
 *                 i.e. sufficient for a call to `mbedtls_mpi_core_montmul()`.
 *
 * \param AN_limbs The number of limbs in the input `A` and the modulus `N`
 *                 (they must be the same size) that will be given to
 *                 `mbedtls_mpi_mod_raw_inv_prime()`.
 *
 * \return         The number of limbs of working memory required by
 *                 `mbedtls_mpi_mod_raw_inv_prime()`.
 */
size_t mbedtls_mpi_mod_raw_inv_prime_working_limbs(size_t AN_limbs);

/**
 * \brief Perform fixed-width modular inversion of a Montgomery-form MPI with
 *        respect to a modulus \p N that must be prime.
 *
 * \p X may be aliased to \p A, but not to \p N or \p RR.
 *
 * \param[out] X     The modular inverse of \p A with respect to \p N.
 *                   Will be in Montgomery form.
 * \param[in] A      The number to calculate the modular inverse of.
 *                   Must be in Montgomery form. Must not be 0.
 * \param[in] N      The modulus, as a little-endian array of length \p AN_limbs.
 *                   Must be prime.
 * \param AN_limbs   The number of limbs in \p A, \p N and \p RR.
 * \param[in] RR     The precomputed residue of 2^{2*biL} modulo N, as a little-
 *                   endian array of length \p AN_limbs.
 * \param[in,out] T  Temporary storage of at least the number of limbs returned
 *                   by `mbedtls_mpi_mod_raw_inv_prime_working_limbs()`.
 *                   Its initial content is unused and its final content is
 *                   indeterminate.
 *                   It must not alias or otherwise overlap any of the other
 *                   parameters.
 *                   It is up to the caller to zeroize \p T when it is no
 *                   longer needed, and before freeing it if it was dynamically
 *                   allocated.
 */
void mbedtls_mpi_mod_raw_inv_prime(mbedtls_mpi_uint *X,
                                   const mbedtls_mpi_uint *A,
                                   const mbedtls_mpi_uint *N,
                                   size_t AN_limbs,
                                   const mbedtls_mpi_uint *RR,
                                   mbedtls_mpi_uint *T);

/**
 * \brief Perform a known-size modular addition.
 *
 * Calculate `A + B modulo N`.
 *
 * The number of limbs in each operand, and the result, is given by the
 * modulus \p N.
 *
 * \p X may be aliased to \p A or \p B, or even both, but may not overlap
 * either otherwise.
 *
 * \param[out] X    The result of the modular addition.
 * \param[in] A     Little-endian presentation of the left operand. This
 *                  must be smaller than \p N.
 * \param[in] B     Little-endian presentation of the right operand. This
 *                  must be smaller than \p N.
 * \param[in] N     The address of the modulus.
 */
void mbedtls_mpi_mod_raw_add(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_uint *A,
                             const mbedtls_mpi_uint *B,
                             const mbedtls_mpi_mod_modulus *N);

/** Convert an MPI from canonical representation (little-endian limb array)
 * to the representation associated with the modulus.
 *
 * \param[in,out] X The limb array to convert.
 *                  It must have as many limbs as \p N.
 *                  It is converted in place.
 *                  If this function returns an error, the content of \p X
 *                  is unspecified.
 * \param[in] N     The modulus structure.
 *
 * \return          \c 0 if successful.
 *                  Otherwise an \c MBEDTLS_ERR_MPI_xxx error code.
 */
int mbedtls_mpi_mod_raw_canonical_to_modulus_rep(
    mbedtls_mpi_uint *X,
    const mbedtls_mpi_mod_modulus *N);

/** Convert an MPI from the representation associated with the modulus
 * to canonical representation (little-endian limb array).
 *
 * \param[in,out] X The limb array to convert.
 *                  It must have as many limbs as \p N.
 *                  It is converted in place.
 *                  If this function returns an error, the content of \p X
 *                  is unspecified.
 * \param[in] N     The modulus structure.
 *
 * \return          \c 0 if successful.
 *                  Otherwise an \c MBEDTLS_ERR_MPI_xxx error code.
 */
int mbedtls_mpi_mod_raw_modulus_to_canonical_rep(
    mbedtls_mpi_uint *X,
    const mbedtls_mpi_mod_modulus *N);

/** Generate a random number uniformly in a range.
 *
 * This function generates a random number between \p min inclusive and
 * \p N exclusive.
 *
 * The procedure complies with RFC 6979 §3.3 (deterministic ECDSA)
 * when the RNG is a suitably parametrized instance of HMAC_DRBG
 * and \p min is \c 1.
 *
 * \note           There are `N - min` possible outputs. The lower bound
 *                 \p min can be reached, but the upper bound \p N cannot.
 *
 * \param X        The destination MPI, in canonical representation modulo \p N.
 *                 It must not be aliased with \p N or otherwise overlap it.
 * \param min      The minimum value to return. It must be strictly smaller
 *                 than \b N.
 * \param N        The modulus.
 *                 This is the upper bound of the output range, exclusive.
 * \param f_rng    The RNG function to use. This must not be \c NULL.
 * \param p_rng    The RNG parameter to be passed to \p f_rng.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_NOT_ACCEPTABLE if the implementation was
 *                 unable to find a suitable value within a limited number
 *                 of attempts. This has a negligible probability if \p N
 *                 is significantly larger than \p min, which is the case
 *                 for all usual cryptographic applications.
 */
int mbedtls_mpi_mod_raw_random(mbedtls_mpi_uint *X,
                               mbedtls_mpi_uint min,
                               const mbedtls_mpi_mod_modulus *N,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng);

/** Convert an MPI into Montgomery form.
 *
 * \param X      The address of the MPI.
 *               Must have the same number of limbs as \p N.
 * \param N      The address of the modulus, which gives the size of
 *               the base `R` = 2^(biL*N->limbs).
 *
 * \return       \c 0 if successful.
 */
int mbedtls_mpi_mod_raw_to_mont_rep(mbedtls_mpi_uint *X,
                                    const mbedtls_mpi_mod_modulus *N);

/** Convert an MPI back from Montgomery representation.
 *
 * \param X      The address of the MPI.
 *               Must have the same number of limbs as \p N.
 * \param N      The address of the modulus, which gives the size of
 *               the base `R`= 2^(biL*N->limbs).
 *
 * \return       \c 0 if successful.
 */
int mbedtls_mpi_mod_raw_from_mont_rep(mbedtls_mpi_uint *X,
                                      const mbedtls_mpi_mod_modulus *N);

/** \brief  Perform fixed width modular negation.
 *
 * The size of the operation is determined by \p N. \p A must have
 * the same number of limbs as \p N.
 *
 * \p X may be aliased to \p A.
 *
 * \param[out] X        The result of the modular negation.
 *                      This must be initialized.
 * \param[in] A         Little-endian presentation of the input operand. This
 *                      must be less than or equal to \p N.
 * \param[in] N         The modulus to use.
 */
void mbedtls_mpi_mod_raw_neg(mbedtls_mpi_uint *X,
                             const mbedtls_mpi_uint *A,
                             const mbedtls_mpi_mod_modulus *N);

#endif /* MBEDTLS_BIGNUM_MOD_RAW_H */
