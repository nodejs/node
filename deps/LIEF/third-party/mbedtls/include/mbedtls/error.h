/**
 * \file error.h
 *
 * \brief Error to string translation
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_ERROR_H
#define MBEDTLS_ERROR_H

#include "mbedtls/build_info.h"

#include <stddef.h>

/**
 * Error code layout.
 *
 * Currently we try to keep all error codes within the negative space of 16
 * bits signed integers to support all platforms (-0x0001 - -0x7FFF). In
 * addition we'd like to give two layers of information on the error if
 * possible.
 *
 * For that purpose the error codes are segmented in the following manner:
 *
 * 16 bit error code bit-segmentation
 *
 * 1 bit  - Unused (sign bit)
 * 3 bits - High level module ID
 * 5 bits - Module-dependent error code
 * 7 bits - Low level module errors
 *
 * For historical reasons, low-level error codes are divided in even and odd,
 * even codes were assigned first, and -1 is reserved for other errors.
 *
 * Low-level module errors (0x0002-0x007E, 0x0001-0x007F)
 *
 * Module   Nr  Codes assigned
 * ERROR     2  0x006E          0x0001
 * MPI       7  0x0002-0x0010
 * GCM       3  0x0012-0x0016   0x0013-0x0013
 * THREADING 3  0x001A-0x001E
 * AES       5  0x0020-0x0022   0x0021-0x0025
 * CAMELLIA  3  0x0024-0x0026   0x0027-0x0027
 * BASE64    2  0x002A-0x002C
 * OID       1  0x002E-0x002E   0x000B-0x000B
 * PADLOCK   1  0x0030-0x0030
 * DES       2  0x0032-0x0032   0x0033-0x0033
 * CTR_DBRG  4  0x0034-0x003A
 * ENTROPY   3  0x003C-0x0040   0x003D-0x003F
 * NET      13  0x0042-0x0052   0x0043-0x0049
 * ARIA      4  0x0058-0x005E
 * ASN1      7  0x0060-0x006C
 * CMAC      1  0x007A-0x007A
 * PBKDF2    1  0x007C-0x007C
 * HMAC_DRBG 4                  0x0003-0x0009
 * CCM       3                  0x000D-0x0011
 * MD5       1                  0x002F-0x002F
 * RIPEMD160 1                  0x0031-0x0031
 * SHA1      1                  0x0035-0x0035 0x0073-0x0073
 * SHA256    1                  0x0037-0x0037 0x0074-0x0074
 * SHA512    1                  0x0039-0x0039 0x0075-0x0075
 * SHA-3     1                  0x0076-0x0076
 * CHACHA20  3                  0x0051-0x0055
 * POLY1305  3                  0x0057-0x005B
 * CHACHAPOLY 2 0x0054-0x0056
 * PLATFORM  2  0x0070-0x0072
 * LMS       5  0x0011-0x0019
 *
 * High-level module nr (3 bits - 0x0...-0x7...)
 * Name      ID  Nr of Errors
 * PEM       1   9
 * PKCS#12   1   4 (Started from top)
 * X509      2   20
 * PKCS5     2   4 (Started from top)
 * DHM       3   11
 * PK        3   15 (Started from top)
 * RSA       4   11
 * ECP       4   10 (Started from top)
 * MD        5   5
 * HKDF      5   1 (Started from top)
 * PKCS7     5   12 (Started from 0x5300)
 * SSL       5   3 (Started from 0x5F00)
 * CIPHER    6   8 (Started from 0x6080)
 * SSL       6   22 (Started from top, plus 0x6000)
 * SSL       7   20 (Started from 0x7000, gaps at
 *                   0x7380, 0x7900-0x7980, 0x7A80-0x7E80)
 *
 * Module dependent error code (5 bits 0x.00.-0x.F8.)
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Generic error */
#define MBEDTLS_ERR_ERROR_GENERIC_ERROR       -0x0001
/** This is a bug in the library */
#define MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED -0x006E

/** Hardware accelerator failed */
#define MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED     -0x0070
/** The requested feature is not supported by the platform */
#define MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED -0x0072

/**
 * \brief Combines a high-level and low-level error code together.
 *
 *        Wrapper macro for mbedtls_error_add(). See that function for
 *        more details.
 */
#define MBEDTLS_ERROR_ADD(high, low) \
    mbedtls_error_add(high, low, __FILE__, __LINE__)

#if defined(MBEDTLS_TEST_HOOKS)
/**
 * \brief Testing hook called before adding/combining two error codes together.
 *        Only used when invasive testing is enabled via MBEDTLS_TEST_HOOKS.
 */
extern void (*mbedtls_test_hook_error_add)(int, int, const char *, int);
#endif

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
 * \note  When invasive testing is enabled via #MBEDTLS_TEST_HOOKS, also try to
 *        call \link mbedtls_test_hook_error_add \endlink.
 *
 * \param high      high-level error code. See error.h for more details.
 * \param low       low-level error code. See error.h for more details.
 * \param file      file where this error code addition occurred.
 * \param line      line where this error code addition occurred.
 */
static inline int mbedtls_error_add(int high, int low,
                                    const char *file, int line)
{
#if defined(MBEDTLS_TEST_HOOKS)
    if (*mbedtls_test_hook_error_add != NULL) {
        (*mbedtls_test_hook_error_add)(high, low, file, line);
    }
#endif
    (void) file;
    (void) line;

    return high + low;
}

/**
 * \brief Translate an Mbed TLS error code into a string representation.
 *        The result is truncated if necessary and always includes a
 *        terminating null byte.
 *
 * \param errnum    error code
 * \param buffer    buffer to place representation in
 * \param buflen    length of the buffer
 */
void mbedtls_strerror(int errnum, char *buffer, size_t buflen);

/**
 * \brief Translate the high-level part of an Mbed TLS error code into a string
 *        representation.
 *
 * This function returns a const pointer to an un-modifiable string. The caller
 * must not try to modify the string. It is intended to be used mostly for
 * logging purposes.
 *
 * \param error_code    error code
 *
 * \return The string representation of the error code, or \c NULL if the error
 *         code is unknown.
 */
const char *mbedtls_high_level_strerr(int error_code);

/**
 * \brief Translate the low-level part of an Mbed TLS error code into a string
 *        representation.
 *
 * This function returns a const pointer to an un-modifiable string. The caller
 * must not try to modify the string. It is intended to be used mostly for
 * logging purposes.
 *
 * \param error_code    error code
 *
 * \return The string representation of the error code, or \c NULL if the error
 *         code is unknown.
 */
const char *mbedtls_low_level_strerr(int error_code);

#ifdef __cplusplus
}
#endif

#endif /* error.h */
