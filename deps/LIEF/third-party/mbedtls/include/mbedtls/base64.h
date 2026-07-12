/**
 * \file base64.h
 *
 * \brief RFC 1521 base64 encoding/decoding
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_BASE64_H
#define MBEDTLS_BASE64_H

#include "mbedtls/build_info.h"

#include <stddef.h>

/** Output buffer too small. */
#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL               -0x002A
/** Invalid character in input. */
#define MBEDTLS_ERR_BASE64_INVALID_CHARACTER              -0x002C

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          Encode a buffer into base64 format
 *
 * \param dst      destination buffer
 * \param dlen     size of the destination buffer
 * \param olen     number of bytes written
 * \param src      source buffer
 * \param slen     amount of data to be encoded
 *
 * \return         0 if successful, or MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL.
 *                 *olen is always updated to reflect the amount
 *                 of data that has (or would have) been written.
 *                 If that length cannot be represented, then no data is
 *                 written to the buffer and *olen is set to the maximum
 *                 length representable as a size_t.
 *
 * \note           Call this function with dlen = 0 to obtain the
 *                 required buffer size in *olen
 */
int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen);

/**
 * \brief          Decode a base64-formatted buffer
 *
 * \param dst      destination buffer (can be NULL for checking size)
 * \param dlen     size of the destination buffer
 * \param olen     number of bytes written
 * \param src      source buffer
 * \param slen     amount of data to be decoded
 *
 * \return         0 if successful, MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL, or
 *                 MBEDTLS_ERR_BASE64_INVALID_CHARACTER if the input data is
 *                 not correct. *olen is always updated to reflect the amount
 *                 of data that has (or would have) been written.
 *
 * \note           Call this function with *dst = NULL or dlen = 0 to obtain
 *                 the required buffer size in *olen
 */
int mbedtls_base64_decode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen);

#if defined(MBEDTLS_SELF_TEST)
/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if the test failed
 */
int mbedtls_base64_self_test(int verbose);

#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* base64.h */
