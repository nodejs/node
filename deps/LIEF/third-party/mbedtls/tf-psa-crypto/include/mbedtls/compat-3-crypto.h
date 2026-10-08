/**
 * \file compat-3-crypto.h
 *
 * \brief Compatibility definitions for MbedTLS 3.x code built with
 *        MbedTLS 4.x or TF-PSA-Crypto 1.x
 *
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_COMPAT_3_CRYPTO_H
#define MBEDTLS_COMPAT_3_CRYPTO_H

#include "psa/crypto_values.h"

/** Output buffer too small. */
#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL               PSA_ERROR_BUFFER_TOO_SMALL
/** Buffer too small when writing ASN.1 data structure. */
#define MBEDTLS_ERR_ASN1_BUF_TOO_SMALL                    PSA_ERROR_BUFFER_TOO_SMALL
/** Input/output buffer is too small to contain requited data */
#define MBEDTLS_ERR_LMS_BUFFER_TOO_SMALL                  PSA_ERROR_BUFFER_TOO_SMALL
/** The output buffer is too small. */
#define MBEDTLS_ERR_PK_BUFFER_TOO_SMALL                   PSA_ERROR_BUFFER_TOO_SMALL

/** Memory allocation failed. */
#define MBEDTLS_ERR_PK_ALLOC_FAILED                       PSA_ERROR_INSUFFICIENT_MEMORY
/** Failed to allocate memory. */
#define MBEDTLS_ERR_PEM_ALLOC_FAILED                      PSA_ERROR_INSUFFICIENT_MEMORY
/** Memory allocation failed */
#define MBEDTLS_ERR_ASN1_ALLOC_FAILED                     PSA_ERROR_INSUFFICIENT_MEMORY
/** LMS failed to allocate space for a private key */
#define MBEDTLS_ERR_LMS_ALLOC_FAILED                      PSA_ERROR_INSUFFICIENT_MEMORY

/** Bad input parameters to function. */
#define MBEDTLS_ERR_PK_BAD_INPUT_DATA                     PSA_ERROR_INVALID_ARGUMENT
/** Bad input parameters to function. */
#define MBEDTLS_ERR_PEM_BAD_INPUT_DATA                    PSA_ERROR_INVALID_ARGUMENT
/** Bad data has been input to an LMS function */
#define MBEDTLS_ERR_LMS_BAD_INPUT_DATA                    PSA_ERROR_INVALID_ARGUMENT

#endif /* MBEDTLS_COMPAT_3_CRYPTO_H */
