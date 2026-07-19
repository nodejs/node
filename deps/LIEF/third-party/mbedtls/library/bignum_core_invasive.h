/**
 * \file bignum_core_invasive.h
 *
 * \brief Function declarations for invasive functions of bignum core.
 */
/**
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_BIGNUM_CORE_INVASIVE_H
#define MBEDTLS_BIGNUM_CORE_INVASIVE_H

#include "bignum_core.h"

#if defined(MBEDTLS_TEST_HOOKS) && !defined(MBEDTLS_THREADING_C)

extern void (*mbedtls_safe_codepath_hook)(void);
extern void (*mbedtls_unsafe_codepath_hook)(void);

#endif /* MBEDTLS_TEST_HOOKS && !MBEDTLS_THREADING_C */

#endif /* MBEDTLS_BIGNUM_CORE_INVASIVE_H */
