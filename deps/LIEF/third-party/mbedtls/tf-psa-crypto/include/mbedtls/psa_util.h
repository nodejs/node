/**
 * \file psa_util.h
 *
 * \brief Utility functions for the use of the PSA Crypto library.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_PSA_UTIL_H
#define MBEDTLS_PSA_UTIL_H
#include "mbedtls/private_access.h"

#include "tf-psa-crypto/build_info.h"

#include "psa/crypto.h"

/* ASN1 defines used in the ECDSA conversion functions.
 * Note: intentionally not adding MBEDTLS_ASN1_[PARSE|WRITE]_C guards here
 * otherwise error codes would be unknown in test_suite_psa_crypto_util.data.*/
#include <mbedtls/asn1write.h>

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)

/** The random generator function for the PSA subsystem.
 *
 * This function is suitable as the `f_rng` random generator function
 * parameter of many `mbedtls_xxx` functions.
 *
 * The implementation of this function depends on the configuration of the
 * library.
 *
 * \note This function may only be used if the PSA crypto subsystem is active.
 *       This means that you must call psa_crypto_init() before any call to
 *       this function, and you must not call this function after calling
 *       mbedtls_psa_crypto_free().
 *
 * \param p_rng         This parameter is only kept for backward compatibility
 *                      reasons with legacy `f_rng` functions and it's ignored.
 *                      Set to #MBEDTLS_PSA_RANDOM_STATE or NULL.
 * \param output        The buffer to fill. It must have room for
 *                      \c output_size bytes.
 * \param output_size   The number of bytes to write to \p output.
 *                      This function may fail if \p output_size is too
 *                      large. It is guaranteed to accept any output size
 *                      requested by Mbed TLS library functions. The
 *                      maximum request size depends on the library
 *                      configuration.
 *
 * \return              \c 0 on success.
 * \return              An `MBEDTLS_ERR_ENTROPY_xxx`,
 *                      `MBEDTLS_ERR_PLATFORM_xxx,
 *                      `MBEDTLS_ERR_CTR_DRBG_xxx` or
 *                      `MBEDTLS_ERR_HMAC_DRBG_xxx` on error.
 */
int mbedtls_psa_get_random(void *p_rng,
                           unsigned char *output,
                           size_t output_size);

/** The random generator state for the PSA subsystem.
 *
 * This macro always expands to NULL because the `p_rng` parameter is unused
 * in mbedtls_psa_get_random(), but it's kept for interface's backward
 * compatibility.
 */
#define MBEDTLS_PSA_RANDOM_STATE    NULL

/** \defgroup psa_tls_helpers TLS helper functions
 * @{
 */
/**
 * \brief           This function returns the PSA algorithm identifier
 *                  associated with the given digest type.
 *
 * \param md_type   The type of digest to search for. Must not be NONE.
 *
 * \warning         If \p md_type is \c MBEDTLS_MD_NONE, this function will
 *                  not return \c PSA_ALG_NONE, but an invalid algorithm.
 *
 * \warning         This function does not check if the algorithm is
 *                  supported, it always returns the corresponding identifier.
 *
 * \return          The PSA algorithm identifier associated with \p md_type,
 *                  regardless of whether it is supported or not.
 */
static inline psa_algorithm_t mbedtls_md_psa_alg_from_type(mbedtls_md_type_t md_type)
{
    return PSA_ALG_CATEGORY_HASH | (psa_algorithm_t) md_type;
}

/**
 * \brief           This function returns the given digest type
 *                  associated with the PSA algorithm identifier.
 *
 * \param psa_alg   The PSA algorithm identifier to search for.
 *
 * \warning         This function does not check if the algorithm is
 *                  supported, it always returns the corresponding identifier.
 *
 * \return          The MD type associated with \p psa_alg,
 *                  regardless of whether it is supported or not.
 */
static inline mbedtls_md_type_t mbedtls_md_type_from_psa_alg(psa_algorithm_t psa_alg)
{
    return (mbedtls_md_type_t) (psa_alg & PSA_ALG_HASH_MASK);
}
#endif /* MBEDTLS_PSA_CRYPTO_CLIENT */

#if defined(PSA_HAVE_ALG_SOME_ECDSA)

/**
 * \brief           Maximum size of a DER-encoded ECDSA signature for a
 *                  given curve bit size.
 *
 * \param bits      Curve size in bits.
 * \return          Maximum signature size in bytes.
 *
 * \note            This macro returns a compile-time constant if its argument
 *                  is one. It may evaluate its argument multiple times.
 */
/*
 *     Ecdsa-Sig-Value ::= SEQUENCE {
 *         r       INTEGER,
 *         s       INTEGER
 *     }
 *
 * For each of r and s, the value (V) may include an extra initial "0" bit.
 */
#define MBEDTLS_ECDSA_DER_MAX_SIG_LEN(bits)                               \
    (/*T,L of SEQUENCE*/ ((bits) >= 61 * 8 ? 3 : 2) +              \
     /*T,L of r,s*/ 2 * (((bits) >= 127 * 8 ? 3 : 2) +     \
                         /*V of r,s*/ ((bits) + 8) / 8))

/** The maximal size of a DER-encoded ECDSA signature in Bytes. */
#define MBEDTLS_ECDSA_DER_MAX_LEN  MBEDTLS_ECDSA_DER_MAX_SIG_LEN(PSA_VENDOR_ECC_MAX_CURVE_BITS)

/** Convert an ECDSA signature from raw format to DER ASN.1 format.
 *
 * \param       bits        Size of each coordinate in bits.
 * \param       raw         Buffer that contains the signature in raw format.
 * \param       raw_len     Length of \p raw in bytes. This must be
 *                          PSA_BITS_TO_BYTES(bits) bytes.
 * \param[out]  der         Buffer that will be filled with the converted DER
 *                          output. It can overlap with raw buffer.
 * \param       der_size    Size of \p der in bytes. It is enough if \p der_size
 *                          is at least the size of the actual output. (The size
 *                          of the output can vary depending on the presence of
 *                          leading zeros in the data.) You can use
 *                          #MBEDTLS_ECDSA_DER_MAX_SIG_LEN(\p bits) to determine
 *                          a size that is large enough for all signatures for a
 *                          given value of \p bits.
 * \param[out]  der_len     On success it contains the amount of valid data
 *                          (in bytes) written to \p der. It's undefined
 *                          in case of failure.
 *
 * \note                    The behavior is undefined if \p der is null,
 *                          even if \p der_size is 0.
 *
 * \return                  0 if successful.
 * \return                  #PSA_ERROR_BUFFER_TOO_SMALL if \p der_size
 *                          is too small or if \p bits is larger than the
 *                          largest supported curve.
 * \return                  #MBEDTLS_ERR_ASN1_INVALID_DATA if one of the
 *                          numbers in the signature is 0.
 */
int mbedtls_ecdsa_raw_to_der(size_t bits, const unsigned char *raw, size_t raw_len,
                             unsigned char *der, size_t der_size, size_t *der_len);

/** Convert an ECDSA signature from DER ASN.1 format to raw format.
 *
 * \param       bits        Size of each coordinate in bits.
 * \param       der         Buffer that contains the signature in DER format.
 * \param       der_len     Size of \p der in bytes.
 * \param[out]  raw         Buffer that will be filled with the converted raw
 *                          signature. It can overlap with der buffer.
 * \param       raw_size    Size of \p raw in bytes. Must be at least
 *                          2 * PSA_BITS_TO_BYTES(bits) bytes.
 * \param[out]  raw_len     On success it is updated with the amount of valid
 *                          data (in bytes) written to \p raw. It's undefined
 *                          in case of failure.
 *
 * \return                  0 if successful.
 * \return                  #PSA_ERROR_BUFFER_TOO_SMALL if \p raw_size
 *                          is too small or if \p bits is larger than the
 *                          largest supported curve.
 * \return                  #MBEDTLS_ERR_ASN1_INVALID_DATA if the data in
 *                          \p der is inconsistent with \p bits.
 * \return                  An \c MBEDTLS_ERR_ASN1_xxx error code if
 *                          \p der is malformed.
 */
int mbedtls_ecdsa_der_to_raw(size_t bits, const unsigned char *der, size_t der_len,
                             unsigned char *raw, size_t raw_size, size_t *raw_len);

#endif /* PSA_HAVE_ALG_SOME_ECDSA */

/**@}*/

#endif /* MBEDTLS_PSA_UTIL_H */
