/**
 *  Core bignum functions
 *
 *  This interface should only be used by the legacy bignum module (bignum.h)
 *  and the modular bignum modules (bignum_mod.c, bignum_mod_raw.c). All other
 *  modules should use the high-level modular bignum interface (bignum_mod.h)
 *  or the legacy bignum interface (bignum.h).
 *
 * This module is about processing non-negative integers with a fixed upper
 * bound that's of the form 2^n-1 where n is a multiple of #biL.
 * These can be thought of integers written in base 2^#biL with a fixed
 * number of digits. Digits in this base are called *limbs*.
 * Many operations treat these numbers as the principal representation of
 * a number modulo 2^n or a smaller bound.
 *
 * The functions in this module obey the following conventions unless
 * explicitly indicated otherwise:
 *
 * - **Overflow**: some functions indicate overflow from the range
 *   [0, 2^n-1] by returning carry parameters, while others operate
 *   modulo and so cannot overflow. This should be clear from the function
 *   documentation.
 * - **Bignum parameters**: Bignums are passed as pointers to an array of
 *   limbs. A limb has the type #mbedtls_mpi_uint. Unless otherwise specified:
 *     - Bignum parameters called \p A, \p B, ... are inputs, and are
 *       not modified by the function.
 *     - For operations modulo some number, the modulus is called \p N
 *       and is input-only.
 *     - Bignum parameters called \p X, \p Y are outputs or input-output.
 *       The initial content of output-only parameters is ignored.
 *     - Some functions use different names that reflect traditional
 *       naming of operands of certain operations (e.g.
 *       divisor/dividend/quotient/remainder).
 *     - \p T is a temporary storage area. The initial content of such
 *       parameter is ignored and the final content is unspecified.
 * - **Bignum sizes**: bignum sizes are always expressed in limbs.
 *   Most functions work on bignums of a given size and take a single
 *   \p limbs parameter that applies to all parameters that are limb arrays.
 *   All bignum sizes must be at least 1 and must be significantly less than
 *   #SIZE_MAX. The behavior if a size is 0 is undefined. The behavior if the
 *   total size of all parameters overflows #SIZE_MAX is undefined.
 * - **Parameter ordering**: for bignum parameters, outputs come before inputs.
 *   Temporaries come last.
 * - **Aliasing**: in general, output bignums may be aliased to one or more
 *   inputs. As an exception, parameters that are documented as a modulus value
 *   may not be aliased to an output. Outputs may not be aliased to one another.
 *   Temporaries may not be aliased to any other parameter.
 * - **Overlap**: apart from aliasing of limb array pointers (where two
 *   arguments are equal pointers), overlap is not supported and may result
 *   in undefined behavior.
 * - **Error handling**: This is a low-level module. Functions generally do not
 *   try to protect against invalid arguments such as nonsensical sizes or
 *   null pointers. Note that some functions that operate on bignums of
 *   different sizes have constraints about their size, and violating those
 *   constraints may lead to buffer overflows.
 * - **Modular representatives**: functions that operate modulo \p N expect
 *   all modular inputs to be in the range [0, \p N - 1] and guarantee outputs
 *   in the range [0, \p N - 1]. If an input is out of range, outputs are
 *   fully unspecified, though bignum values out of range should not cause
 *   buffer overflows (beware that this is not extensively tested).
 */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_BIGNUM_CORE_H
#define MBEDTLS_BIGNUM_CORE_H

#include "common.h"

#include "mbedtls/bignum.h"

#include "constant_time_internal.h"

#define ciL    (sizeof(mbedtls_mpi_uint))     /** chars in limb  */
#define biL    (ciL << 3)                     /** bits  in limb  */
#define biH    (ciL << 2)                     /** half limb size */

/*
 * Convert between bits/chars and number of limbs
 * Divide first in order to avoid potential overflows
 */
#define BITS_TO_LIMBS(i)  ((i) / biL + ((i) % biL != 0))
#define CHARS_TO_LIMBS(i) ((i) / ciL + ((i) % ciL != 0))
/* Get a specific byte, without range checks. */
#define GET_BYTE(X, i)                                \
    (((X)[(i) / ciL] >> (((i) % ciL) * 8)) & 0xff)

/* Constants to identify whether a value is public or secret. If a parameter is marked as secret by
 * this constant, the function must be constant time with respect to the parameter.
 *
 * This is only needed for functions with the _optionally_safe postfix. All other functions have
 * fixed behavior that can't be changed at runtime and are constant time with respect to their
 * parameters as prescribed by their documentation or by conventions in their module's documentation.
 *
 * Parameters should be named X_public where X is the name of the
 * corresponding input parameter.
 *
 * Implementation should always check using
 *  if (X_public == MBEDTLS_MPI_IS_PUBLIC) {
 *      // unsafe path
 *  } else {
 *      // safe path
 *  }
 * not the other way round, in order to prevent misuse. (That is, if a value
 * other than the two below is passed, default to the safe path.)
 *
 * The value of MBEDTLS_MPI_IS_PUBLIC is chosen in a way that is unlikely to happen by accident, but
 * which can be used as an immediate value in a Thumb2 comparison (for code size). */
#define MBEDTLS_MPI_IS_PUBLIC  0x2a2a2a2a
#define MBEDTLS_MPI_IS_SECRET  0
#if defined(MBEDTLS_TEST_HOOKS) && !defined(MBEDTLS_THREADING_C)
// Default value for testing that is neither MBEDTLS_MPI_IS_PUBLIC nor MBEDTLS_MPI_IS_SECRET
#define MBEDTLS_MPI_IS_TEST  1
#endif

/** Count leading zero bits in a given integer.
 *
 * \warning     The result is undefined if \p a == 0
 *
 * \param a     Integer to count leading zero bits.
 *
 * \return      The number of leading zero bits in \p a, if \p a != 0.
 *              If \p a == 0, the result is undefined.
 */
size_t mbedtls_mpi_core_clz(mbedtls_mpi_uint a);

/** Return the minimum number of bits required to represent the value held
 * in the MPI.
 *
 * \note This function returns 0 if all the limbs of \p A are 0.
 *
 * \param[in] A     The address of the MPI.
 * \param A_limbs   The number of limbs of \p A.
 *
 * \return      The number of bits in \p A.
 */
size_t mbedtls_mpi_core_bitlen(const mbedtls_mpi_uint *A, size_t A_limbs);

/** Convert a big-endian byte array aligned to the size of mbedtls_mpi_uint
 * into the storage form used by mbedtls_mpi.
 *
 * \param[in,out] A     The address of the MPI.
 * \param A_limbs       The number of limbs of \p A.
 */
void mbedtls_mpi_core_bigendian_to_host(mbedtls_mpi_uint *A,
                                        size_t A_limbs);

/** \brief         Compare a machine integer with an MPI.
 *
 *                 This function operates in constant time with respect
 *                 to the values of \p min and \p A.
 *
 * \param min      A machine integer.
 * \param[in] A    An MPI.
 * \param A_limbs  The number of limbs of \p A.
 *                 This must be at least 1.
 *
 * \return         MBEDTLS_CT_TRUE if \p min is less than or equal to \p A, otherwise MBEDTLS_CT_FALSE.
 */
mbedtls_ct_condition_t mbedtls_mpi_core_uint_le_mpi(mbedtls_mpi_uint min,
                                                    const mbedtls_mpi_uint *A,
                                                    size_t A_limbs);

/**
 * \brief          Check if one unsigned MPI is less than another in constant
 *                 time.
 *
 * \param A        The left-hand MPI. This must point to an array of limbs
 *                 with the same allocated length as \p B.
 * \param B        The right-hand MPI. This must point to an array of limbs
 *                 with the same allocated length as \p A.
 * \param limbs    The number of limbs in \p A and \p B.
 *                 This must not be 0.
 *
 * \return         MBEDTLS_CT_TRUE  if \p A is less than \p B.
 *                 MBEDTLS_CT_FALSE if \p A is greater than or equal to \p B.
 */
mbedtls_ct_condition_t mbedtls_mpi_core_lt_ct(const mbedtls_mpi_uint *A,
                                              const mbedtls_mpi_uint *B,
                                              size_t limbs);

/**
 * \brief   Perform a safe conditional copy of an MPI which doesn't reveal
 *          whether assignment was done or not.
 *
 * \param[out] X        The address of the destination MPI.
 *                      This must be initialized. Must have enough limbs to
 *                      store the full value of \p A.
 * \param[in]  A        The address of the source MPI. This must be initialized.
 * \param      limbs    The number of limbs of \p A.
 * \param      assign   The condition deciding whether to perform the
 *                      assignment or not. Callers will need to use
 *                      the constant time interface (e.g. `mbedtls_ct_bool()`)
 *                      to construct this argument.
 *
 * \note           This function avoids leaking any information about whether
 *                 the assignment was done or not.
 */
void mbedtls_mpi_core_cond_assign(mbedtls_mpi_uint *X,
                                  const mbedtls_mpi_uint *A,
                                  size_t limbs,
                                  mbedtls_ct_condition_t assign);

/**
 * \brief   Perform a safe conditional swap of two MPIs which doesn't reveal
 *          whether the swap was done or not.
 *
 * \param[in,out] X         The address of the first MPI.
 *                          This must be initialized.
 * \param[in,out] Y         The address of the second MPI.
 *                          This must be initialized.
 * \param         limbs     The number of limbs of \p X and \p Y.
 * \param         swap      The condition deciding whether to perform
 *                          the swap or not.
 *
 * \note           This function avoids leaking any information about whether
 *                 the swap was done or not.
 */
void mbedtls_mpi_core_cond_swap(mbedtls_mpi_uint *X,
                                mbedtls_mpi_uint *Y,
                                size_t limbs,
                                mbedtls_ct_condition_t swap);

/** Import X from unsigned binary data, little-endian.
 *
 * The MPI needs to have enough limbs to store the full value (including any
 * most significant zero bytes in the input).
 *
 * \param[out] X         The address of the MPI.
 * \param X_limbs        The number of limbs of \p X.
 * \param[in] input      The input buffer to import from.
 * \param input_length   The length bytes of \p input.
 *
 * \return       \c 0 if successful.
 * \return       #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p X isn't
 *               large enough to hold the value in \p input.
 */
int mbedtls_mpi_core_read_le(mbedtls_mpi_uint *X,
                             size_t X_limbs,
                             const unsigned char *input,
                             size_t input_length);

/** Import X from unsigned binary data, big-endian.
 *
 * The MPI needs to have enough limbs to store the full value (including any
 * most significant zero bytes in the input).
 *
 * \param[out] X        The address of the MPI.
 *                      May only be #NULL if \p X_limbs is 0 and \p input_length
 *                      is 0.
 * \param X_limbs       The number of limbs of \p X.
 * \param[in] input     The input buffer to import from.
 *                      May only be #NULL if \p input_length is 0.
 * \param input_length  The length in bytes of \p input.
 *
 * \return       \c 0 if successful.
 * \return       #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p X isn't
 *               large enough to hold the value in \p input.
 */
int mbedtls_mpi_core_read_be(mbedtls_mpi_uint *X,
                             size_t X_limbs,
                             const unsigned char *input,
                             size_t input_length);

/** Export A into unsigned binary data, little-endian.
 *
 * \note If \p output is shorter than \p A the export is still successful if the
 *       value held in \p A fits in the buffer (that is, if enough of the most
 *       significant bytes of \p A are 0).
 *
 * \param[in] A         The address of the MPI.
 * \param A_limbs       The number of limbs of \p A.
 * \param[out] output   The output buffer to export to.
 * \param output_length The length in bytes of \p output.
 *
 * \return       \c 0 if successful.
 * \return       #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p output isn't
 *               large enough to hold the value of \p A.
 */
int mbedtls_mpi_core_write_le(const mbedtls_mpi_uint *A,
                              size_t A_limbs,
                              unsigned char *output,
                              size_t output_length);

/** Export A into unsigned binary data, big-endian.
 *
 * \note If \p output is shorter than \p A the export is still successful if the
 *       value held in \p A fits in the buffer (that is, if enough of the most
 *       significant bytes of \p A are 0).
 *
 * \param[in] A         The address of the MPI.
 * \param A_limbs       The number of limbs of \p A.
 * \param[out] output   The output buffer to export to.
 * \param output_length The length in bytes of \p output.
 *
 * \return       \c 0 if successful.
 * \return       #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p output isn't
 *               large enough to hold the value of \p A.
 */
int mbedtls_mpi_core_write_be(const mbedtls_mpi_uint *A,
                              size_t A_limbs,
                              unsigned char *output,
                              size_t output_length);

/** \brief              Shift an MPI in-place right by a number of bits.
 *
 *                      Shifting by more bits than there are bit positions
 *                      in \p X is valid and results in setting \p X to 0.
 *
 *                      This function's execution time depends on the value
 *                      of \p count (and of course \p limbs).
 *
 * \param[in,out] X     The number to shift.
 * \param limbs         The number of limbs of \p X. This must be at least 1.
 * \param count         The number of bits to shift by.
 */
void mbedtls_mpi_core_shift_r(mbedtls_mpi_uint *X, size_t limbs,
                              size_t count);

/**
 * \brief               Shift an MPI in-place left by a number of bits.
 *
 *                      Shifting by more bits than there are bit positions
 *                      in \p X will produce an unspecified result.
 *
 *                      This function's execution time depends on the value
 *                      of \p count (and of course \p limbs).
 * \param[in,out] X     The number to shift.
 * \param limbs         The number of limbs of \p X. This must be at least 1.
 * \param count         The number of bits to shift by.
 */
void mbedtls_mpi_core_shift_l(mbedtls_mpi_uint *X, size_t limbs,
                              size_t count);

/**
 * \brief Add two fixed-size large unsigned integers, returning the carry.
 *
 * Calculates `A + B` where `A` and `B` have the same size.
 *
 * This function operates modulo `2^(biL*limbs)` and returns the carry
 * (1 if there was a wraparound, and 0 otherwise).
 *
 * \p X may be aliased to \p A or \p B.
 *
 * \param[out] X    The result of the addition.
 * \param[in] A     Little-endian presentation of the left operand.
 * \param[in] B     Little-endian presentation of the right operand.
 * \param limbs     Number of limbs of \p X, \p A and \p B.
 *
 * \return          1 if `A + B >= 2^(biL*limbs)`, 0 otherwise.
 */
mbedtls_mpi_uint mbedtls_mpi_core_add(mbedtls_mpi_uint *X,
                                      const mbedtls_mpi_uint *A,
                                      const mbedtls_mpi_uint *B,
                                      size_t limbs);

/**
 * \brief Conditional addition of two fixed-size large unsigned integers,
 *        returning the carry.
 *
 * Functionally equivalent to
 *
 * ```
 * if( cond )
 *    X += A;
 * return carry;
 * ```
 *
 * This function operates modulo `2^(biL*limbs)`.
 *
 * \param[in,out] X  The pointer to the (little-endian) array
 *                   representing the bignum to accumulate onto.
 * \param[in] A      The pointer to the (little-endian) array
 *                   representing the bignum to conditionally add
 *                   to \p X. This may be aliased to \p X but may not
 *                   overlap otherwise.
 * \param limbs      Number of limbs of \p X and \p A.
 * \param cond       Condition bit dictating whether addition should
 *                   happen or not. This must be \c 0 or \c 1.
 *
 * \warning          If \p cond is neither 0 nor 1, the result of this function
 *                   is unspecified, and the resulting value in \p X might be
 *                   neither its original value nor \p X + \p A.
 *
 * \return           1 if `X + cond * A >= 2^(biL*limbs)`, 0 otherwise.
 */
mbedtls_mpi_uint mbedtls_mpi_core_add_if(mbedtls_mpi_uint *X,
                                         const mbedtls_mpi_uint *A,
                                         size_t limbs,
                                         unsigned cond);

/**
 * \brief Subtract two fixed-size large unsigned integers, returning the borrow.
 *
 * Calculate `A - B` where \p A and \p B have the same size.
 * This function operates modulo `2^(biL*limbs)` and returns the carry
 * (1 if there was a wraparound, i.e. if `A < B`, and 0 otherwise).
 *
 * \p X may be aliased to \p A or \p B, or even both, but may not overlap
 * either otherwise.
 *
 * \param[out] X    The result of the subtraction.
 * \param[in] A     Little-endian presentation of left operand.
 * \param[in] B     Little-endian presentation of right operand.
 * \param limbs     Number of limbs of \p X, \p A and \p B.
 *
 * \return          1 if `A < B`.
 *                  0 if `A >= B`.
 */
mbedtls_mpi_uint mbedtls_mpi_core_sub(mbedtls_mpi_uint *X,
                                      const mbedtls_mpi_uint *A,
                                      const mbedtls_mpi_uint *B,
                                      size_t limbs);

/**
 * \brief Perform a fixed-size multiply accumulate operation: X += b * A
 *
 * \p X may be aliased to \p A (when \p X_limbs == \p A_limbs), but may not
 * otherwise overlap.
 *
 * This function operates modulo `2^(biL*X_limbs)`.
 *
 * \param[in,out] X  The pointer to the (little-endian) array
 *                   representing the bignum to accumulate onto.
 * \param X_limbs    The number of limbs of \p X. This must be
 *                   at least \p A_limbs.
 * \param[in] A      The pointer to the (little-endian) array
 *                   representing the bignum to multiply with.
 *                   This may be aliased to \p X but may not overlap
 *                   otherwise.
 * \param A_limbs    The number of limbs of \p A.
 * \param b          X scalar to multiply with.
 *
 * \return           The carry at the end of the operation.
 */
mbedtls_mpi_uint mbedtls_mpi_core_mla(mbedtls_mpi_uint *X, size_t X_limbs,
                                      const mbedtls_mpi_uint *A, size_t A_limbs,
                                      mbedtls_mpi_uint b);

/**
 * \brief Perform a known-size multiplication
 *
 * \p X may not be aliased to any of the inputs for this function.
 * \p A may be aliased to \p B.
 *
 * \param[out] X     The pointer to the (little-endian) array to receive
 *                   the product of \p A_limbs and \p B_limbs.
 *                   This must be of length \p A_limbs + \p B_limbs.
 * \param[in] A      The pointer to the (little-endian) array
 *                   representing the first factor.
 * \param A_limbs    The number of limbs in \p A.
 * \param[in] B      The pointer to the (little-endian) array
 *                   representing the second factor.
 * \param B_limbs    The number of limbs in \p B.
 */
void mbedtls_mpi_core_mul(mbedtls_mpi_uint *X,
                          const mbedtls_mpi_uint *A, size_t A_limbs,
                          const mbedtls_mpi_uint *B, size_t B_limbs);

/**
 * \brief Calculate initialisation value for fast Montgomery modular
 *        multiplication
 *
 * \param[in] N  Little-endian presentation of the modulus. This must have
 *               at least one limb.
 *
 * \return       The initialisation value for fast Montgomery modular multiplication
 */
mbedtls_mpi_uint mbedtls_mpi_core_montmul_init(const mbedtls_mpi_uint *N);

/**
 * \brief Montgomery multiplication: X = A * B * R^-1 mod N (HAC 14.36)
 *
 * \p A and \p B must be in canonical form. That is, < \p N.
 *
 * \p X may be aliased to \p A or \p N, or even \p B (if \p AN_limbs ==
 * \p B_limbs) but may not overlap any parameters otherwise.
 *
 * \p A and \p B may alias each other, if \p AN_limbs == \p B_limbs. They may
 * not alias \p N (since they must be in canonical form, they cannot == \p N).
 *
 * \param[out]    X         The destination MPI, as a little-endian array of
 *                          length \p AN_limbs.
 *                          On successful completion, X contains the result of
 *                          the multiplication `A * B * R^-1` mod N where
 *                          `R = 2^(biL*AN_limbs)`.
 * \param[in]     A         Little-endian presentation of first operand.
 *                          Must have the same number of limbs as \p N.
 * \param[in]     B         Little-endian presentation of second operand.
 * \param[in]     B_limbs   The number of limbs in \p B.
 *                          Must be <= \p AN_limbs.
 * \param[in]     N         Little-endian presentation of the modulus.
 *                          This must be odd, and have exactly the same number
 *                          of limbs as \p A.
 *                          It may alias \p X, but must not alias or otherwise
 *                          overlap any of the other parameters.
 * \param[in]     AN_limbs  The number of limbs in \p X, \p A and \p N.
 * \param         mm        The Montgomery constant for \p N: -N^-1 mod 2^biL.
 *                          This can be calculated by `mbedtls_mpi_core_montmul_init()`.
 * \param[in,out] T         Temporary storage of size at least 2*AN_limbs+1 limbs.
 *                          Its initial content is unused and
 *                          its final content is indeterminate.
 *                          It must not alias or otherwise overlap any of the
 *                          other parameters.
 */
void mbedtls_mpi_core_montmul(mbedtls_mpi_uint *X,
                              const mbedtls_mpi_uint *A,
                              const mbedtls_mpi_uint *B, size_t B_limbs,
                              const mbedtls_mpi_uint *N, size_t AN_limbs,
                              mbedtls_mpi_uint mm, mbedtls_mpi_uint *T);

/**
 * \brief Calculate the square of the Montgomery constant. (Needed
 *        for conversion and operations in Montgomery form.)
 *
 * \param[out] X  A pointer to the result of the calculation of
 *                the square of the Montgomery constant:
 *                2^{2*n*biL} mod N.
 * \param[in]  N  Little-endian presentation of the modulus, which must be odd.
 *
 * \return        0 if successful.
 * \return        #MBEDTLS_ERR_MPI_ALLOC_FAILED if there is not enough space
 *                to store the value of Montgomery constant squared.
 * \return        #MBEDTLS_ERR_MPI_DIVISION_BY_ZERO if \p N modulus is zero.
 * \return        #MBEDTLS_ERR_MPI_NEGATIVE_VALUE if \p N modulus is negative.
 */
int mbedtls_mpi_core_get_mont_r2_unsafe(mbedtls_mpi *X,
                                        const mbedtls_mpi *N);

#if defined(MBEDTLS_TEST_HOOKS)
/**
 * Copy an MPI from a table without leaking the index.
 *
 * \param dest              The destination buffer. This must point to a writable
 *                          buffer of at least \p limbs limbs.
 * \param table             The address of the table. This must point to a readable
 *                          array of \p count elements of \p limbs limbs each.
 * \param limbs             The number of limbs in each table entry.
 * \param count             The number of entries in \p table.
 * \param index             The (secret) table index to look up. This must be in the
 *                          range `0 .. count-1`.
 */
void mbedtls_mpi_core_ct_uint_table_lookup(mbedtls_mpi_uint *dest,
                                           const mbedtls_mpi_uint *table,
                                           size_t limbs,
                                           size_t count,
                                           size_t index);
#endif /* MBEDTLS_TEST_HOOKS */

/**
 * \brief          Fill an integer with a number of random bytes.
 *
 * \param X        The destination MPI.
 * \param X_limbs  The number of limbs of \p X.
 * \param bytes    The number of random bytes to generate.
 * \param f_rng    The RNG function to use. This must not be \c NULL.
 * \param p_rng    The RNG parameter to be passed to \p f_rng. This may be
 *                 \c NULL if \p f_rng doesn't need a context argument.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \p X does not have
 *                 enough room for \p bytes bytes.
 * \return         A negative error code on RNG failure.
 *
 * \note           The bytes obtained from the RNG are interpreted
 *                 as a big-endian representation of an MPI; this can
 *                 be relevant in applications like deterministic ECDSA.
 */
int mbedtls_mpi_core_fill_random(mbedtls_mpi_uint *X, size_t X_limbs,
                                 size_t bytes,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng);

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
 * \param X        The destination MPI, with \p limbs limbs.
 *                 It must not be aliased with \p N or otherwise overlap it.
 * \param min      The minimum value to return.
 * \param N        The upper bound of the range, exclusive, with \p limbs limbs.
 *                 In other words, this is one plus the maximum value to return.
 *                 \p N must be strictly larger than \p min.
 * \param limbs    The number of limbs of \p N and \p X.
 *                 This must not be 0.
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
int mbedtls_mpi_core_random(mbedtls_mpi_uint *X,
                            mbedtls_mpi_uint min,
                            const mbedtls_mpi_uint *N,
                            size_t limbs,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng);

/**
 * \brief          Returns the number of limbs of working memory required for
 *                 a call to `mbedtls_mpi_core_exp_mod()`.
 *
 * \note           This will always be at least
 *                 `mbedtls_mpi_core_montmul_working_limbs(AN_limbs)`,
 *                 i.e. sufficient for a call to `mbedtls_mpi_core_montmul()`.
 *
 * \param AN_limbs The number of limbs in the input `A` and the modulus `N`
 *                 (they must be the same size) that will be given to
 *                 `mbedtls_mpi_core_exp_mod()`.
 * \param E_limbs  The number of limbs in the exponent `E` that will be given
 *                 to `mbedtls_mpi_core_exp_mod()`.
 *
 * \return         The number of limbs of working memory required by
 *                 `mbedtls_mpi_core_exp_mod()`.
 */
size_t mbedtls_mpi_core_exp_mod_working_limbs(size_t AN_limbs, size_t E_limbs);

/**
 * \brief            Perform a modular exponentiation with public or secret exponent:
 *                   X = A^E mod N, where \p A is already in Montgomery form.
 *
 * \warning          This function is not constant time with respect to \p E (the exponent).
 *
 * \p X may be aliased to \p A, but not to \p RR or \p E, even if \p E_limbs ==
 * \p AN_limbs.
 *
 * \param[out] X     The destination MPI, as a little endian array of length
 *                   \p AN_limbs.
 * \param[in] A      The base MPI, as a little endian array of length \p AN_limbs.
 *                   Must be in Montgomery form.
 * \param[in] N      The modulus, as a little endian array of length \p AN_limbs.
 * \param AN_limbs   The number of limbs in \p X, \p A, \p N, \p RR.
 * \param[in] E      The exponent, as a little endian array of length \p E_limbs.
 * \param E_limbs    The number of limbs in \p E.
 * \param[in] RR     The precomputed residue of 2^{2*biL} modulo N, as a little
 *                   endian array of length \p AN_limbs.
 * \param[in,out] T  Temporary storage of at least the number of limbs returned
 *                   by `mbedtls_mpi_core_exp_mod_working_limbs()`.
 *                   Its initial content is unused and its final content is
 *                   indeterminate.
 *                   It must not alias or otherwise overlap any of the other
 *                   parameters.
 *                   It is up to the caller to zeroize \p T when it is no
 *                   longer needed, and before freeing it if it was dynamically
 *                   allocated.
 */
void mbedtls_mpi_core_exp_mod_unsafe(mbedtls_mpi_uint *X,
                                     const mbedtls_mpi_uint *A,
                                     const mbedtls_mpi_uint *N, size_t AN_limbs,
                                     const mbedtls_mpi_uint *E, size_t E_limbs,
                                     const mbedtls_mpi_uint *RR,
                                     mbedtls_mpi_uint *T);

/**
 * \brief            Perform a modular exponentiation with secret exponent:
 *                   X = A^E mod N, where \p A is already in Montgomery form.
 *
 * \p X may be aliased to \p A, but not to \p RR or \p E, even if \p E_limbs ==
 * \p AN_limbs.
 *
 * \param[out] X     The destination MPI, as a little endian array of length
 *                   \p AN_limbs.
 * \param[in] A      The base MPI, as a little endian array of length \p AN_limbs.
 *                   Must be in Montgomery form.
 * \param[in] N      The modulus, as a little endian array of length \p AN_limbs.
 * \param AN_limbs   The number of limbs in \p X, \p A, \p N, \p RR.
 * \param[in] E      The exponent, as a little endian array of length \p E_limbs.
 * \param E_limbs    The number of limbs in \p E.
 * \param[in] RR     The precomputed residue of 2^{2*biL} modulo N, as a little
 *                   endian array of length \p AN_limbs.
 * \param[in,out] T  Temporary storage of at least the number of limbs returned
 *                   by `mbedtls_mpi_core_exp_mod_working_limbs()`.
 *                   Its initial content is unused and its final content is
 *                   indeterminate.
 *                   It must not alias or otherwise overlap any of the other
 *                   parameters.
 *                   It is up to the caller to zeroize \p T when it is no
 *                   longer needed, and before freeing it if it was dynamically
 *                   allocated.
 */
void mbedtls_mpi_core_exp_mod(mbedtls_mpi_uint *X,
                              const mbedtls_mpi_uint *A,
                              const mbedtls_mpi_uint *N, size_t AN_limbs,
                              const mbedtls_mpi_uint *E, size_t E_limbs,
                              const mbedtls_mpi_uint *RR,
                              mbedtls_mpi_uint *T);

/**
 * \brief Subtract unsigned integer from known-size large unsigned integers.
 *        Return the borrow.
 *
 * \param[out] X    The result of the subtraction.
 * \param[in] A     The left operand.
 * \param b         The unsigned scalar to subtract.
 * \param limbs     Number of limbs of \p X and \p A.
 *
 * \return          1 if `A < b`.
 *                  0 if `A >= b`.
 */
mbedtls_mpi_uint mbedtls_mpi_core_sub_int(mbedtls_mpi_uint *X,
                                          const mbedtls_mpi_uint *A,
                                          mbedtls_mpi_uint b,
                                          size_t limbs);

/**
 * \brief Determine if a given MPI has the value \c 0 in constant time with
 *        respect to the value (but not with respect to the number of limbs).
 *
 * \param[in] A   The MPI to test.
 * \param limbs   Number of limbs in \p A.
 *
 * \return        MBEDTLS_CT_FALSE if `A == 0`
 *                MBEDTLS_CT_TRUE  if `A != 0`.
 */
mbedtls_ct_condition_t mbedtls_mpi_core_check_zero_ct(const mbedtls_mpi_uint *A,
                                                      size_t limbs);

/**
 * \brief          Returns the number of limbs of working memory required for
 *                 a call to `mbedtls_mpi_core_montmul()`.
 *
 * \param AN_limbs The number of limbs in the input `A` and the modulus `N`
 *                 (they must be the same size) that will be given to
 *                 `mbedtls_mpi_core_montmul()` or one of the other functions
 *                 that specifies this as the amount of working memory needed.
 *
 * \return         The number of limbs of working memory required by
 *                 `mbedtls_mpi_core_montmul()` (or other similar function).
 */
static inline size_t mbedtls_mpi_core_montmul_working_limbs(size_t AN_limbs)
{
    return 2 * AN_limbs + 1;
}

/** Convert an MPI into Montgomery form.
 *
 * \p X may be aliased to \p A, but may not otherwise overlap it.
 *
 * \p X may not alias \p N (it is in canonical form, so must be strictly less
 * than \p N). Nor may it alias or overlap \p rr (this is unlikely to be
 * required in practice.)
 *
 * This function is a thin wrapper around `mbedtls_mpi_core_montmul()` that is
 * an alternative to calling `mbedtls_mpi_mod_raw_to_mont_rep()` when we
 * don't want to allocate memory.
 *
 * \param[out]    X         The result of the conversion.
 *                          Must have the same number of limbs as \p A.
 * \param[in]     A         The MPI to convert into Montgomery form.
 *                          Must have the same number of limbs as the modulus.
 * \param[in]     N         The address of the modulus, which gives the size of
 *                          the base `R` = 2^(biL*N->limbs).
 * \param[in]     AN_limbs  The number of limbs in \p X, \p A, \p N and \p rr.
 * \param         mm        The Montgomery constant for \p N: -N^-1 mod 2^biL.
 *                          This can be determined by calling
 *                          `mbedtls_mpi_core_montmul_init()`.
 * \param[in]     rr        The residue for `2^{2*n*biL} mod N`.
 * \param[in,out] T         Temporary storage of size at least
 *                          `mbedtls_mpi_core_montmul_working_limbs(AN_limbs)`
 *                          limbs.
 *                          Its initial content is unused and
 *                          its final content is indeterminate.
 *                          It must not alias or otherwise overlap any of the
 *                          other parameters.
 */
void mbedtls_mpi_core_to_mont_rep(mbedtls_mpi_uint *X,
                                  const mbedtls_mpi_uint *A,
                                  const mbedtls_mpi_uint *N,
                                  size_t AN_limbs,
                                  mbedtls_mpi_uint mm,
                                  const mbedtls_mpi_uint *rr,
                                  mbedtls_mpi_uint *T);

/** Convert an MPI from Montgomery form.
 *
 * \p X may be aliased to \p A, but may not otherwise overlap it.
 *
 * \p X may not alias \p N (it is in canonical form, so must be strictly less
 * than \p N).
 *
 * This function is a thin wrapper around `mbedtls_mpi_core_montmul()` that is
 * an alternative to calling `mbedtls_mpi_mod_raw_from_mont_rep()` when we
 * don't want to allocate memory.
 *
 * \param[out]    X         The result of the conversion.
 *                          Must have the same number of limbs as \p A.
 * \param[in]     A         The MPI to convert from Montgomery form.
 *                          Must have the same number of limbs as the modulus.
 * \param[in]     N         The address of the modulus, which gives the size of
 *                          the base `R` = 2^(biL*N->limbs).
 * \param[in]     AN_limbs  The number of limbs in \p X, \p A and \p N.
 * \param         mm        The Montgomery constant for \p N: -N^-1 mod 2^biL.
 *                          This can be determined by calling
 *                          `mbedtls_mpi_core_montmul_init()`.
 * \param[in,out] T         Temporary storage of size at least
 *                          `mbedtls_mpi_core_montmul_working_limbs(AN_limbs)`
 *                          limbs.
 *                          Its initial content is unused and
 *                          its final content is indeterminate.
 *                          It must not alias or otherwise overlap any of the
 *                          other parameters.
 */
void mbedtls_mpi_core_from_mont_rep(mbedtls_mpi_uint *X,
                                    const mbedtls_mpi_uint *A,
                                    const mbedtls_mpi_uint *N,
                                    size_t AN_limbs,
                                    mbedtls_mpi_uint mm,
                                    mbedtls_mpi_uint *T);

#endif /* MBEDTLS_BIGNUM_CORE_H */
