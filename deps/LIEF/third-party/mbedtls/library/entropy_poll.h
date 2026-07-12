/**
 * \file entropy_poll.h
 *
 * \brief Platform-specific and custom entropy polling functions
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_ENTROPY_POLL_H
#define MBEDTLS_ENTROPY_POLL_H

#include "mbedtls/build_info.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Default thresholds for built-in sources, in bytes
 */
#define MBEDTLS_ENTROPY_MIN_PLATFORM     32     /**< Minimum for platform source    */
#if !defined(MBEDTLS_ENTROPY_MIN_HARDWARE)
#define MBEDTLS_ENTROPY_MIN_HARDWARE     32     /**< Minimum for the hardware source */
#endif

#if !defined(MBEDTLS_NO_PLATFORM_ENTROPY)
/**
 * \brief           Platform-specific entropy poll callback
 */
int mbedtls_platform_entropy_poll(void *data,
                                  unsigned char *output, size_t len, size_t *olen);
#endif

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
/**
 * \brief           Entropy poll callback for a hardware source
 *
 * \warning         This is not provided by Mbed TLS!
 *                  See \c MBEDTLS_ENTROPY_HARDWARE_ALT in mbedtls_config.h.
 *
 * \note            This must accept NULL as its first argument.
 */
int mbedtls_hardware_poll(void *data,
                          unsigned char *output, size_t len, size_t *olen);
#endif

#if defined(MBEDTLS_ENTROPY_NV_SEED)
/**
 * \brief           Entropy poll callback for a non-volatile seed file
 *
 * \note            This must accept NULL as its first argument.
 */
int mbedtls_nv_seed_poll(void *data,
                         unsigned char *output, size_t len, size_t *olen);
#endif

#ifdef __cplusplus
}
#endif

#endif /* entropy_poll.h */
