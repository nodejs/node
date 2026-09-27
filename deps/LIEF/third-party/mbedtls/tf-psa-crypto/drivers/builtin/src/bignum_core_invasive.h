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

#if defined(MBEDTLS_TEST_HOOKS)

#if !defined(MBEDTLS_THREADING_C)

extern void (*mbedtls_safe_codepath_hook)(void);
extern void (*mbedtls_unsafe_codepath_hook)(void);

#endif /* !MBEDTLS_THREADING_C */

/** Divide X by 2 mod N in place, assuming N is odd.
 *
 * \param[in,out] X     The value to divide by 2 mod \p N.
 * \param[in]     N     The modulus. Must be odd.
 * \param[in]     limbs The number of limbs in \p X and \p N.
 */
MBEDTLS_STATIC_TESTABLE
void mbedtls_mpi_core_div2_mod_odd(mbedtls_mpi_uint *X,
                                   const mbedtls_mpi_uint *N,
                                   size_t limbs);

#endif /* MBEDTLS_TEST_HOOKS */

#endif /* MBEDTLS_BIGNUM_CORE_INVASIVE_H */
