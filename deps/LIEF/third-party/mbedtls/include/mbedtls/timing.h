/**
 * \file timing.h
 *
 * \brief Portable interface to timeouts and to the CPU cycle counter
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_TIMING_H
#define MBEDTLS_TIMING_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_TIMING_ALT)
// Regular implementation
//

/**
 * \brief          timer structure
 */
struct mbedtls_timing_hr_time {
    uint64_t MBEDTLS_PRIVATE(opaque)[4];
};

/**
 * \brief          Context for mbedtls_timing_set/get_delay()
 */
typedef struct mbedtls_timing_delay_context {
    struct mbedtls_timing_hr_time   MBEDTLS_PRIVATE(timer);
    uint32_t                        MBEDTLS_PRIVATE(int_ms);
    uint32_t                        MBEDTLS_PRIVATE(fin_ms);
} mbedtls_timing_delay_context;

#else  /* MBEDTLS_TIMING_ALT */
#include "timing_alt.h"
#endif /* MBEDTLS_TIMING_ALT */

/* Internal use */
unsigned long mbedtls_timing_get_timer(struct mbedtls_timing_hr_time *val, int reset);

/**
 * \brief          Set a pair of delays to watch
 *                 (See \c mbedtls_timing_get_delay().)
 *
 * \param data     Pointer to timing data.
 *                 Must point to a valid \c mbedtls_timing_delay_context struct.
 * \param int_ms   First (intermediate) delay in milliseconds.
 *                 The effect if int_ms > fin_ms is unspecified.
 * \param fin_ms   Second (final) delay in milliseconds.
 *                 Pass 0 to cancel the current delay.
 *
 * \note           To set a single delay, either use \c mbedtls_timing_set_timer
 *                 directly or use this function with int_ms == fin_ms.
 */
void mbedtls_timing_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms);

/**
 * \brief          Get the status of delays
 *                 (Memory helper: number of delays passed.)
 *
 * \param data     Pointer to timing data
 *                 Must point to a valid \c mbedtls_timing_delay_context struct.
 *
 * \return         -1 if cancelled (fin_ms = 0),
 *                  0 if none of the delays are passed,
 *                  1 if only the intermediate delay is passed,
 *                  2 if the final delay is passed.
 */
int mbedtls_timing_get_delay(void *data);

/**
 * \brief          Get the final timing delay
 *
 * \param data     Pointer to timing data
 *                 Must point to a valid \c mbedtls_timing_delay_context struct.
 *
 * \return         Final timing delay in milliseconds.
 */
uint32_t mbedtls_timing_get_final_delay(
    const mbedtls_timing_delay_context *data);

#ifdef __cplusplus
}
#endif

#endif /* timing.h */
