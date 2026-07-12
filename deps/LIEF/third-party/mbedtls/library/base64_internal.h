/**
 * \file base64_internal.h
 *
 * \brief RFC 1521 base64 encoding/decoding: interfaces for invasive testing
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_BASE64_INTERNAL
#define MBEDTLS_BASE64_INTERNAL

#include "common.h"

#if defined(MBEDTLS_TEST_HOOKS)

/** Given a value in the range 0..63, return the corresponding Base64 digit.
 *
 * The implementation assumes that letters are consecutive (e.g. ASCII
 * but not EBCDIC).
 *
 * \param value     A value in the range 0..63.
 *
 * \return          A base64 digit converted from \p value.
 */
unsigned char mbedtls_ct_base64_enc_char(unsigned char value);

/** Given a Base64 digit, return its value.
 *
 * If c is not a Base64 digit ('A'..'Z', 'a'..'z', '0'..'9', '+' or '/'),
 * return -1.
 *
 * The implementation assumes that letters are consecutive (e.g. ASCII
 * but not EBCDIC).
 *
 * \param c     A base64 digit.
 *
 * \return      The value of the base64 digit \p c.
 */
signed char mbedtls_ct_base64_dec_value(unsigned char c);

#endif /* MBEDTLS_TEST_HOOKS */

#endif /* MBEDTLS_BASE64_INTERNAL */
