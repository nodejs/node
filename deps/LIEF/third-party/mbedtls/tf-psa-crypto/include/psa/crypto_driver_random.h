/**
 * \file psa/crypto_driver_random.h
 * \brief Definitions for PSA random and entropy drivers
 *
 * This file is part of the PSA Crypto Driver Model, containing functions for
 * driver developers to implement to enable hardware to be called in a
 * standardized way by a PSA Cryptographic API implementation. The functions
 * comprising the driver model, which driver authors implement, are not
 * intended to be called by application developers.
 */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef PSA_CRYPTO_DRIVER_RANDOM_H
#define PSA_CRYPTO_DRIVER_RANDOM_H

#include "crypto_driver_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup driver_random Random and entropy drivers
 * @{
 */

/** The type of the \p flags argument to `"get_entropy"` driver entry points.
 *
 * This implementation does not support any flags yet.
 *
 */
typedef uint32_t psa_driver_get_entropy_flags_t;

/** Flags requesting the default behavior for a `"get_entropy"` driver entry
 * point. This is equivalent to \c 0.
 *
 * \see ::psa_driver_get_entropy_flags_t
 */
#define PSA_DRIVER_GET_ENTROPY_FLAGS_NONE ((psa_driver_get_entropy_flags_t) 0)

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* PSA_CRYPTO_DRIVER_COMMON_H */
