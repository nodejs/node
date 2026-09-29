/**
 * \file error_common.h
 *
 * \brief Error codes
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_ERROR_COMMON_H
#define MBEDTLS_ERROR_COMMON_H

#include "tf-psa-crypto/build_info.h"
#include <psa/crypto_values.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Generic error */
#define MBEDTLS_ERR_ERROR_GENERIC_ERROR       PSA_ERROR_GENERIC_ERROR
/* This is a bug in the library */
#define MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED PSA_ERROR_CORRUPTION_DETECTED

/* Hardware accelerator failed */
#define MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED     PSA_ERROR_HARDWARE_FAILURE
/* The requested feature is not supported by the platform */
#define MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED PSA_ERROR_NOT_SUPPORTED

/**
 * \brief Combines a high-level and low-level error code together.
 *
 *        Wrapper macro for mbedtls_error_add(). See that function for
 *        more details.
 */
#define MBEDTLS_ERROR_ADD(high, low) \
    mbedtls_error_add(high, low)

/**
 * \brief Combines a high-level and low-level error code together.
 *
 *        This function can be called directly however it is usually
 *        called via the #MBEDTLS_ERROR_ADD macro.
 *
 *        While a value of zero is not a negative error code, it is still an
 *        error code (that denotes success) and can be combined with both a
 *        negative error code or another value of zero.
 *
 * \note  The distinction between low-level and high-level error codes is
 *        obsolete since TF-PSA-Crypto 1.0 and Mbed TLS 4.0. It is still
 *        present in the code due to the heritage from Mbed TLS <=3,
 *        where low-level and high-level error codes could be added.
 *        New code should not make this distinction and should just
 *        propagate errors returned by lower-level modules unless there
 *        is a good reason to report a different error code in the
 *        higher-level module.
 *
 * \param high      High-level error code, i.e. error code from the module
 *                  that is reporting the error.
 *                  This can be 0 to just propagate a low-level error.
 * \param low       Low-level error code, i.e. error code returned by
 *                  a lower-level function.
 *                  This can be 0 to just return a high-level error.
 */
static inline int mbedtls_error_add(int high, int low)
{
    /* We give priority to the lower-level error code, because this
     * is usually the right choice. For example, if a low-level module
     * runs out of memory, this should not be converted to a high-level
     * error code such as invalid-signature. */
    return low ? low : high;
}

#ifdef __cplusplus
}
#endif

#endif /* error_common.h */
