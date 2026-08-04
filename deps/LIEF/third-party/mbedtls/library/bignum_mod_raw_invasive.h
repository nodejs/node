/**
 * \file bignum_mod_raw_invasive.h
 *
 * \brief Function declarations for invasive functions of Low-level
 *        modular bignum.
 */
/**
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_BIGNUM_MOD_RAW_INVASIVE_H
#define MBEDTLS_BIGNUM_MOD_RAW_INVASIVE_H

#include "common.h"
#include "mbedtls/bignum.h"
#include "bignum_mod.h"

#if defined(MBEDTLS_TEST_HOOKS)

/** Convert the result of a quasi-reduction to its canonical representative.
 *
 * \param[in,out] X     The address of the MPI to be converted. Must have the
 *                      same number of limbs as \p N. The input value must
 *                      be in range 0 <= X < 2N.
 * \param[in]     N     The address of the modulus.
 */
MBEDTLS_STATIC_TESTABLE
void mbedtls_mpi_mod_raw_fix_quasi_reduction(mbedtls_mpi_uint *X,
                                             const mbedtls_mpi_mod_modulus *N);

#endif /* MBEDTLS_TEST_HOOKS */

#endif /* MBEDTLS_BIGNUM_MOD_RAW_INVASIVE_H */
