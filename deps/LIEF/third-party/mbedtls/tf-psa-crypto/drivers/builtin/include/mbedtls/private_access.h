/**
 * \file private_access.h
 *
 * \brief Optionally activate declarations of private identifiers
 *        in public headers.
 *
 * This header is reserved for internal use in TF-PSA-Crypto and Mbed TLS.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_PRIVATE_ACCESS_H
#define MBEDTLS_PRIVATE_ACCESS_H

#ifndef MBEDTLS_ALLOW_PRIVATE_ACCESS
/* Public use: do not declare private identifiers. */

/* Pseudo-hide an identifier (typically a struct or union member) by giving
 * it the prefix `private_`.
 *
 * Typical usage:
 * ```
 * typedef struct {
 *     int MBEDTLS_PRIVATE(foo); // private member (not part of the public API,
 *                               // but part of the ABI)
 *     int bar; // public member (covered by API stability guarantees)
 * } mbedtls_some_type_t;
 * ```
 */
#define MBEDTLS_PRIVATE(member) private_##member

#else
/* Private use: declare private identifiers. */

#define MBEDTLS_PRIVATE(member) member

/* Activate declarations guarded by this macro.
 *
 * Typical usage:
 * ```
 * typedef ... mbedtls_some_type_t; // built-in crypto type
 * #if defined(MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS)
 * int mbedtls_some_function(...); // built-in crypto function
 * #endif // MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
 * ```
 */
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS

#endif

#endif /* MBEDTLS_PRIVATE_ACCESS_H */
