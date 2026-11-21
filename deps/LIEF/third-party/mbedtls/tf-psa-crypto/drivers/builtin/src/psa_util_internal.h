/**
 * \file psa_util_internal.h
 *
 * \brief Internal utility functions for use of PSA Crypto.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_PSA_UTIL_INTERNAL_H
#define MBEDTLS_PSA_UTIL_INTERNAL_H

/* Include the public header so that users only need one include. */
#include "mbedtls/psa_util.h"

#include "psa/crypto.h"

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)

#include "mbedtls/private/ecp.h"

#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)

/*************************************************************************
 * FFDH
 ************************************************************************/

#define MBEDTLS_PSA_MAX_FFDH_PUBKEY_LENGTH \
    PSA_KEY_EXPORT_FFDH_PUBLIC_KEY_MAX_SIZE(PSA_VENDOR_FFDH_MAX_KEY_BITS)

/*************************************************************************
 * ECC
 ************************************************************************/

#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH \
    PSA_KEY_EXPORT_ECC_PUBLIC_KEY_MAX_SIZE(PSA_VENDOR_ECC_MAX_CURVE_BITS)

#define MBEDTLS_PSA_MAX_EC_KEY_PAIR_LENGTH \
    PSA_KEY_EXPORT_ECC_KEY_PAIR_MAX_SIZE(PSA_VENDOR_ECC_MAX_CURVE_BITS)

/*************************************************************************
 * Error translation
 ************************************************************************/

typedef struct {
    /* Error codes used by PSA crypto are in -255..-128, fitting in 16 bits. */
    int16_t psa_status;
    /* Error codes used by Mbed TLS are in one of the ranges
     * -127..-1 (low-level) or -32767..-4096 (high-level with a low-level
     * code optionally added), fitting in 16 bits. */
    int16_t mbedtls_error;
} mbedtls_error_pair_t;

#if defined(MBEDTLS_MD_LIGHT)
extern const mbedtls_error_pair_t psa_to_md_errors[4];
#endif

#if defined(MBEDTLS_BLOCK_CIPHER_SOME_PSA)
extern const mbedtls_error_pair_t psa_to_cipher_errors[4];
#endif

#if defined(MBEDTLS_LMS_C)
extern const mbedtls_error_pair_t psa_to_lms_errors[3];
#endif

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY) ||    \
    defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR_BASIC)
extern const mbedtls_error_pair_t psa_to_pk_rsa_errors[8];
#endif

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
extern const mbedtls_error_pair_t psa_to_pk_ecdsa_errors[7];
#endif

/* Generic fallback function for error translation,
 * when the received state was not module-specific. */
int psa_generic_status_to_mbedtls(psa_status_t status);

/* This function iterates over provided local error translations,
 * and if no match was found - calls the fallback error translation function. */
int psa_status_to_mbedtls(psa_status_t status,
                          const mbedtls_error_pair_t *local_translations,
                          size_t local_errors_num,
                          int (*fallback_f)(psa_status_t));

/* The second out of three-stage error handling functions of the pk module,
 * acts as a fallback after RSA / ECDSA error translation, and if no match
 * is found, it itself calls psa_generic_status_to_mbedtls. */
int psa_pk_status_to_mbedtls(psa_status_t status);

/* Utility macro to shorten the defines of error translator in modules. */
#define PSA_TO_MBEDTLS_ERR_LIST(status, error_list, fallback_f)       \
    psa_status_to_mbedtls(status, error_list,                         \
                          sizeof(error_list)/sizeof(error_list[0]),   \
                          fallback_f)

#endif /* MBEDTLS_PSA_CRYPTO_CLIENT */
#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
/** Convert an ECC curve identifier from the Mbed TLS encoding to PSA.
 *
 * \param grpid         An Mbed TLS elliptic curve identifier
 *                      (`MBEDTLS_ECP_DP_xxx`).
 * \param[out] bits     On success the bit size of the curve; 0 on failure.
 *
 * \return              If the curve is supported in the PSA API, this function
 *                      returns the proper PSA curve identifier
 *                      (`PSA_ECC_FAMILY_xxx`). This holds even if the curve is
 *                      not supported by the ECP module.
 * \return              \c 0 if the curve is not supported in the PSA API.
 */
psa_ecc_family_t mbedtls_ecc_group_to_psa(mbedtls_ecp_group_id grpid,
                                          size_t *bits);

/** Convert an ECC curve identifier from the PSA encoding to Mbed TLS.
 *
 * \param family        A PSA elliptic curve family identifier
 *                      (`PSA_ECC_FAMILY_xxx`).
 * \param bits          The bit-length of a private key on \p curve.
 *
 * \return              If the curve is supported in the PSA API, this function
 *                      returns the corresponding Mbed TLS elliptic curve
 *                      identifier (`MBEDTLS_ECP_DP_xxx`).
 * \return              #MBEDTLS_ECP_DP_NONE if the combination of \c curve
 *                      and \p bits is not supported.
 */
mbedtls_ecp_group_id mbedtls_ecc_group_from_psa(psa_ecc_family_t family,
                                                size_t bits);
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
#endif /* MBEDTLS_PSA_UTIL_INTERNAL_H */
