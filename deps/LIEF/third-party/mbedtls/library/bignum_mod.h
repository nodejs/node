/**
 *  Modular bignum functions
 *
 * This module implements operations on integers modulo some fixed modulus.
 *
 * The functions in this module obey the following conventions unless
 * explicitly indicated otherwise:
 *
 * - **Modulus parameters**: the modulus is passed as a pointer to a structure
 *   of type #mbedtls_mpi_mod_modulus. The structure must be set up with an
 *   array of limbs storing the bignum value of the modulus. The modulus must
 *   be odd and is assumed to have no leading zeroes. The modulus is usually
 *   named \c N and is usually input-only. Functions which take a parameter
 *   of type \c const #mbedtls_mpi_mod_modulus* must not modify its value.
 * - **Bignum parameters**: Bignums are passed as pointers to an array of
 *   limbs or to a #mbedtls_mpi_mod_residue structure. A limb has the type
 *   #mbedtls_mpi_uint. Residues must be initialized before use, and must be
 *   associated with the modulus \c N. Unless otherwise specified:
 *     - Bignum parameters called \c A, \c B, ... are inputs and are not
 *       modified by the function. Functions which take a parameter of
 *       type \c const #mbedtls_mpi_mod_residue* must not modify its value.
 *     - Bignum parameters called \c X, \c Y, ... are outputs or input-output.
 *       The initial bignum value of output-only parameters is ignored, but
 *       they must be set up and associated with the modulus \c N. Some
 *       functions (typically constant-flow) require that the limbs in an
 *       output residue are initialized.
 *     - Bignum parameters called \c p are inputs used to set up a modulus or
 *       residue. These must be pointers to an array of limbs.
 *     - \c T is a temporary storage area. The initial content of such a
 *       parameter is ignored and the final content is unspecified.
 *     - Some functions use different names, such as \c r for the residue.
 * - **Bignum sizes**: bignum sizes are always expressed in limbs. Both
 *   #mbedtls_mpi_mod_modulus and #mbedtls_mpi_mod_residue have a \c limbs
 *   member storing its size. All bignum parameters must have the same
 *   number of limbs as the modulus. All bignum sizes must be at least 1 and
 *   must be significantly less than #SIZE_MAX. The behavior if a size is 0 is
 *   undefined.
 * - **Bignum representation**: the representation of inputs and outputs is
 *   specified by the \c int_rep field of the modulus.
 * - **Parameter ordering**: for bignum parameters, outputs come before inputs.
 *   The modulus is passed after residues. Temporaries come last.
 * - **Aliasing**: in general, output bignums may be aliased to one or more
 *   inputs. Modulus values may not be aliased to any other parameter. Outputs
 *   may not be aliased to one another. Temporaries may not be aliased to any
 *   other parameter.
 * - **Overlap**: apart from aliasing of residue pointers (where two residue
 *   arguments are equal pointers), overlap is not supported and may result
 *   in undefined behavior.
 * - **Error handling**: functions generally check compatibility of input
 *   sizes. Most functions will not check that input values are in canonical
 *   form (i.e. that \c A < \c N), this is only checked during setup of a
 *   residue structure.
 * - **Modular representatives**: all functions expect inputs to be in the
 *   range [0, \c N - 1] and guarantee outputs in the range [0, \c N - 1].
 *   Residues are set up with an associated modulus, and operations are only
 *   guaranteed to work if the modulus is associated with all residue
 *   parameters. If a residue is passed with a modulus other than the one it
 *   is associated with, then it may be out of range. If an input is out of
 *   range, outputs are fully unspecified, though bignum values out of range
 *   should not cause buffer overflows (beware that this is not extensively
 *   tested).
 */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_BIGNUM_MOD_H
#define MBEDTLS_BIGNUM_MOD_H

#include "common.h"

#if defined(MBEDTLS_BIGNUM_C)
#include "mbedtls/bignum.h"
#endif

/** How residues associated with a modulus are represented.
 *
 * This also determines which fields of the modulus structure are valid and
 * what their contents are (see #mbedtls_mpi_mod_modulus).
 */
typedef enum {
    /** Representation not chosen (makes the modulus structure invalid). */
    MBEDTLS_MPI_MOD_REP_INVALID    = 0,
    /* Skip 1 as it is slightly easier to accidentally pass to functions. */
    /** Montgomery representation. */
    MBEDTLS_MPI_MOD_REP_MONTGOMERY = 2,
    /* Optimised reduction available. This indicates a coordinate modulus (P)
     * and one or more of the following have been configured:
     * - A nist curve (MBEDTLS_ECP_DP_SECPXXXR1_ENABLED) & MBEDTLS_ECP_NIST_OPTIM.
     * - A Kobliz Curve.
     * - A Fast Reduction Curve CURVE25519 or CURVE448. */
    MBEDTLS_MPI_MOD_REP_OPT_RED,
} mbedtls_mpi_mod_rep_selector;

/* Make mbedtls_mpi_mod_rep_selector and mbedtls_mpi_mod_ext_rep disjoint to
 * make it easier to catch when they are accidentally swapped. */
typedef enum {
    MBEDTLS_MPI_MOD_EXT_REP_INVALID = 0,
    MBEDTLS_MPI_MOD_EXT_REP_LE      = 8,
    MBEDTLS_MPI_MOD_EXT_REP_BE
} mbedtls_mpi_mod_ext_rep;

typedef struct {
    mbedtls_mpi_uint *p;
    size_t limbs;
} mbedtls_mpi_mod_residue;

typedef struct {
    mbedtls_mpi_uint const *rr;  /* The residue for 2^{2*n*biL} mod N */
    mbedtls_mpi_uint mm;         /* Montgomery const for -N^{-1} mod 2^{ciL} */
} mbedtls_mpi_mont_struct;

typedef int (*mbedtls_mpi_modp_fn)(mbedtls_mpi_uint *X, size_t X_limbs);

typedef struct {
    mbedtls_mpi_modp_fn modp;    /* The optimised reduction function pointer */
} mbedtls_mpi_opt_red_struct;

typedef struct {
    const mbedtls_mpi_uint *p;
    size_t limbs;                            // number of limbs
    size_t bits;                             // bitlen of p
    mbedtls_mpi_mod_rep_selector int_rep;    // selector to signal the active member of the union
    union rep {
        /* if int_rep == #MBEDTLS_MPI_MOD_REP_MONTGOMERY */
        mbedtls_mpi_mont_struct mont;
        /* if int_rep == #MBEDTLS_MPI_MOD_REP_OPT_RED */
        mbedtls_mpi_opt_red_struct ored;
    } rep;
} mbedtls_mpi_mod_modulus;

/** Setup a residue structure.
 *
 * The residue will be set up with the buffer \p p and modulus \p N.
 *
 * The memory pointed to by \p p will be used by the resulting residue structure.
 * The value at the pointed-to memory will be the initial value of \p r and must
 * hold a value that is less than the modulus. This value will be used as-is
 * and interpreted according to the value of the `N->int_rep` field.
 *
 * The modulus \p N will be the modulus associated with \p r. The residue \p r
 * should only be used in operations where the modulus is \p N.
 *
 * \param[out] r    The address of the residue to setup.
 * \param[in] N     The address of the modulus related to \p r.
 * \param[in] p     The address of the limb array containing the value of \p r.
 *                  The memory pointed to by \p p will be used by \p r and must
 *                  not be modified in any way until after
 *                  mbedtls_mpi_mod_residue_release() is called. The data
 *                  pointed to by \p p must be less than the modulus (the value
 *                  pointed to by `N->p`) and already in the representation
 *                  indicated by `N->int_rep`.
 * \param p_limbs   The number of limbs of \p p. Must be the same as the number
 *                  of limbs in the modulus \p N.
 *
 * \return      \c 0 if successful.
 * \return      #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \p p_limbs is less than the
 *              limbs in \p N or if \p p is not less than \p N.
 */
int mbedtls_mpi_mod_residue_setup(mbedtls_mpi_mod_residue *r,
                                  const mbedtls_mpi_mod_modulus *N,
                                  mbedtls_mpi_uint *p,
                                  size_t p_limbs);

/** Unbind elements of a residue structure.
 *
 * This function removes the reference to the limb array that was passed to
 * mbedtls_mpi_mod_residue_setup() to make it safe to free or use again.
 *
 * This function invalidates \p r and it must not be used until after
 * mbedtls_mpi_mod_residue_setup() is called on it again.
 *
 * \param[out] r     The address of residue to release.
 */
void mbedtls_mpi_mod_residue_release(mbedtls_mpi_mod_residue *r);

/** Initialize a modulus structure.
 *
 * \param[out] N     The address of the modulus structure to initialize.
 */
void mbedtls_mpi_mod_modulus_init(mbedtls_mpi_mod_modulus *N);

/** Setup a modulus structure.
 *
 * \param[out] N    The address of the modulus structure to populate.
 * \param[in] p     The address of the limb array storing the value of \p N.
 *                  The memory pointed to by \p p will be used by \p N and must
 *                  not be modified in any way until after
 *                  mbedtls_mpi_mod_modulus_free() is called.
 * \param p_limbs   The number of limbs of \p p.
 *
 * \return      \c 0 if successful.
 */
int mbedtls_mpi_mod_modulus_setup(mbedtls_mpi_mod_modulus *N,
                                  const mbedtls_mpi_uint *p,
                                  size_t p_limbs);

/** Setup an optimised-reduction compatible modulus structure.
 *
 * \param[out] N    The address of the modulus structure to populate.
 * \param[in] p     The address of the limb array storing the value of \p N.
 *                  The memory pointed to by \p p will be used by \p N and must
 *                  not be modified in any way until after
 *                  mbedtls_mpi_mod_modulus_free() is called.
 * \param p_limbs   The number of limbs of \p p.
 * \param modp      A pointer to the optimised reduction function to use. \p p.
 *
 * \return      \c 0 if successful.
 */
int mbedtls_mpi_mod_optred_modulus_setup(mbedtls_mpi_mod_modulus *N,
                                         const mbedtls_mpi_uint *p,
                                         size_t p_limbs,
                                         mbedtls_mpi_modp_fn modp);

/** Free elements of a modulus structure.
 *
 * This function frees any memory allocated by mbedtls_mpi_mod_modulus_setup().
 *
 * \warning This function does not free the limb array passed to
 *          mbedtls_mpi_mod_modulus_setup() only removes the reference to it,
 *          making it safe to free or to use it again.
 *
 * \param[in,out] N     The address of the modulus structure to free.
 */
void mbedtls_mpi_mod_modulus_free(mbedtls_mpi_mod_modulus *N);

/** \brief  Multiply two residues, returning the residue modulo the specified
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
 *
 * \return      \c 0 if successful.
 * \return      #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if all the parameters do not
 *              have the same number of limbs or \p N is invalid.
 * \return      #MBEDTLS_ERR_MPI_ALLOC_FAILED on memory-allocation failure.
 */
int mbedtls_mpi_mod_mul(mbedtls_mpi_mod_residue *X,
                        const mbedtls_mpi_mod_residue *A,
                        const mbedtls_mpi_mod_residue *B,
                        const mbedtls_mpi_mod_modulus *N);

/**
 * \brief Perform a fixed-size modular subtraction.
 *
 * Calculate `A - B modulo N`.
 *
 * \p A, \p B and \p X must all have the same number of limbs as \p N.
 *
 * \p X may be aliased to \p A or \p B, or even both, but may not overlap
 * either otherwise.
 *
 * \note This function does not check that \p A or \p B are in canonical
 *       form (that is, are < \p N) - that will have been done by
 *       mbedtls_mpi_mod_residue_setup().
 *
 * \param[out] X    The address of the result MPI. Must be initialized.
 *                  Must have the same number of limbs as the modulus \p N.
 * \param[in]  A    The address of the first MPI.
 * \param[in]  B    The address of the second MPI.
 * \param[in]  N    The address of the modulus. Used to perform a modulo
 *                  operation on the result of the subtraction.
 *
 * \return          \c 0 if successful.
 * \return          #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if the given MPIs do not
 *                  have the correct number of limbs.
 */
int mbedtls_mpi_mod_sub(mbedtls_mpi_mod_residue *X,
                        const mbedtls_mpi_mod_residue *A,
                        const mbedtls_mpi_mod_residue *B,
                        const mbedtls_mpi_mod_modulus *N);

/**
 * \brief Perform modular inversion of an MPI with respect to a modulus \p N.
 *
 * \p A and \p X must be associated with the modulus \p N and will therefore
 * have the same number of limbs as \p N.
 *
 * \p X may be aliased to \p A.
 *
 * \warning  Currently only supports prime moduli, but does not check for them.
 *
 * \param[out] X   The modular inverse of \p A with respect to \p N.
 * \param[in] A    The number to calculate the modular inverse of.
 *                 Must not be 0.
 * \param[in] N    The modulus to use.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \p A and \p N do not
 *                 have the same number of limbs.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \p A is zero.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if couldn't allocate enough
 *                 memory (needed for conversion to and from Mongtomery form
 *                 when not in Montgomery form already, and for temporary use
 *                 by the inversion calculation itself).
 */

int mbedtls_mpi_mod_inv(mbedtls_mpi_mod_residue *X,
                        const mbedtls_mpi_mod_residue *A,
                        const mbedtls_mpi_mod_modulus *N);
/**
 * \brief Perform a fixed-size modular addition.
 *
 * Calculate `A + B modulo N`.
 *
 * \p A, \p B and \p X must all be associated with the modulus \p N and must
 * all have the same number of limbs as \p N.
 *
 * \p X may be aliased to \p A or \p B, or even both, but may not overlap
 * either otherwise.
 *
 * \note This function does not check that \p A or \p B are in canonical
 *       form (that is, are < \p N) - that will have been done by
 *       mbedtls_mpi_mod_residue_setup().
 *
 * \param[out] X    The address of the result residue. Must be initialized.
 *                  Must have the same number of limbs as the modulus \p N.
 * \param[in]  A    The address of the first input residue.
 * \param[in]  B    The address of the second input residue.
 * \param[in]  N    The address of the modulus. Used to perform a modulo
 *                  operation on the result of the addition.
 *
 * \return          \c 0 if successful.
 * \return          #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if the given MPIs do not
 *                  have the correct number of limbs.
 */
int mbedtls_mpi_mod_add(mbedtls_mpi_mod_residue *X,
                        const mbedtls_mpi_mod_residue *A,
                        const mbedtls_mpi_mod_residue *B,
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
 * \param X        The destination residue.
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
int mbedtls_mpi_mod_random(mbedtls_mpi_mod_residue *X,
                           mbedtls_mpi_uint min,
                           const mbedtls_mpi_mod_modulus *N,
                           int (*f_rng)(void *, unsigned char *, size_t),
                           void *p_rng);

/** Read a residue from a byte buffer.
 *
 * The residue will be automatically converted to the internal representation
 * based on the value of the `N->int_rep` field.
 *
 * The modulus \p N will be the modulus associated with \p r. The residue \p r
 * should only be used in operations where the modulus is \p N or a modulus
 * equivalent to \p N (in the sense that all their fields or memory pointed by
 * their fields hold the same value).
 *
 * \param[out] r    The address of the residue. It must have exactly the same
 *                  number of limbs as the modulus \p N.
 * \param[in] N     The address of the modulus.
 * \param[in] buf   The input buffer to import from.
 * \param buflen    The length in bytes of \p buf.
 * \param ext_rep   The endianness of the number in the input buffer.
 *
 * \return       \c 0 if successful.
 * \return       #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p r isn't
 *               large enough to hold the value in \p buf.
 * \return       #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \p ext_rep
 *               is invalid or the value in the buffer is not less than \p N.
 */
int mbedtls_mpi_mod_read(mbedtls_mpi_mod_residue *r,
                         const mbedtls_mpi_mod_modulus *N,
                         const unsigned char *buf,
                         size_t buflen,
                         mbedtls_mpi_mod_ext_rep ext_rep);

/** Write a residue into a byte buffer.
 *
 * The modulus \p N must be the modulus associated with \p r (see
 * mbedtls_mpi_mod_residue_setup() and mbedtls_mpi_mod_read()).
 *
 * The residue will be automatically converted from the internal representation
 * based on the value of `N->int_rep` field.
 *
 * \warning     If the buffer is smaller than `N->bits`, the number of
 *              leading zeroes is leaked through timing. If \p r is
 *              secret, the caller must ensure that \p buflen is at least
 *              (`N->bits`+7)/8.
 *
 * \param[in] r     The address of the residue. It must have the same number of
 *                  limbs as the modulus \p N. (\p r is an input parameter, but
 *                  its value will be modified during execution and restored
 *                  before the function returns.)
 * \param[in] N     The address of the modulus associated with \p r.
 * \param[out] buf  The output buffer to export to.
 * \param buflen    The length in bytes of \p buf.
 * \param ext_rep   The endianness in which the number should be written into
 *                  the output buffer.
 *
 * \return       \c 0 if successful.
 * \return       #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p buf isn't
 *               large enough to hold the value of \p r (without leading
 *               zeroes).
 * \return       #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \p ext_rep is invalid.
 * \return       #MBEDTLS_ERR_MPI_ALLOC_FAILED if couldn't allocate enough
 *               memory for conversion. Can occur only for moduli with
 *               MBEDTLS_MPI_MOD_REP_MONTGOMERY.
 */
int mbedtls_mpi_mod_write(const mbedtls_mpi_mod_residue *r,
                          const mbedtls_mpi_mod_modulus *N,
                          unsigned char *buf,
                          size_t buflen,
                          mbedtls_mpi_mod_ext_rep ext_rep);

#endif /* MBEDTLS_BIGNUM_MOD_H */
