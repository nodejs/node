/**
 * \file ctr.h
 *
 * \brief    This file contains common functionality for counter algorithms.
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_CTR_H
#define MBEDTLS_CTR_H

#include "common.h"

/**
 * \brief               Increment a big-endian 16-byte value.
 *                      This is quite performance-sensitive for AES-CTR and CTR-DRBG.
 *
 * \param n             A 16-byte value to be incremented.
 */
static inline void mbedtls_ctr_increment_counter(uint8_t n[16])
{
    // The 32-bit version seems to perform about the same as a 64-bit version
    // on 64-bit architectures, so no need to define a 64-bit version.
    for (int i = 3;; i--) {
        uint32_t x = MBEDTLS_GET_UINT32_BE(n, i << 2);
        x += 1;
        MBEDTLS_PUT_UINT32_BE(x, n, i << 2);
        if (x != 0 || i == 0) {
            break;
        }
    }
}

#endif /* MBEDTLS_CTR_H */
