/*
 *  Portable interface to the CPU cycle counter
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_TIMING_C)

#include "mbedtls/timing.h"

#if !defined(MBEDTLS_TIMING_ALT)

#if !defined(unix) && !defined(__unix__) && !defined(__unix) && \
    !defined(__APPLE__) && !defined(_WIN32) && !defined(__QNXNTO__) && \
    !defined(__HAIKU__) && !defined(__midipix__)
#error "This module only works on Unix and Windows, see MBEDTLS_TIMING_C in mbedtls_config.h"
#endif

#if defined(_WIN32) && !defined(EFIX64) && !defined(EFI32)

#include <windows.h>
#include <process.h>

struct _hr_time {
    LARGE_INTEGER start;
};

#else

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
/* time.h should be included independently of MBEDTLS_HAVE_TIME. If the
 * platform matches the ifdefs above, it will be used. */
#include <time.h>
#include <sys/time.h>
struct _hr_time {
    struct timeval start;
};
#endif /* _WIN32 && !EFIX64 && !EFI32 */

/**
 * \brief          Return the elapsed time in milliseconds
 *
 * \warning        May change without notice
 *
 * \param val      points to a timer structure
 * \param reset    If 0, query the elapsed time. Otherwise (re)start the timer.
 *
 * \return         Elapsed time since the previous reset in ms. When
 *                 restarting, this is always 0.
 *
 * \note           To initialize a timer, call this function with reset=1.
 *
 *                 Determining the elapsed time and resetting the timer is not
 *                 atomic on all platforms, so after the sequence
 *                 `{ get_timer(1); ...; time1 = get_timer(1); ...; time2 =
 *                 get_timer(0) }` the value time1+time2 is only approximately
 *                 the delay since the first reset.
 */
#if defined(_WIN32) && !defined(EFIX64) && !defined(EFI32)

unsigned long mbedtls_timing_get_timer(struct mbedtls_timing_hr_time *val, int reset)
{
    struct _hr_time *t = (struct _hr_time *) val;

    if (reset) {
        QueryPerformanceCounter(&t->start);
        return 0;
    } else {
        unsigned long delta;
        LARGE_INTEGER now, hfreq;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&hfreq);
        delta = (unsigned long) ((now.QuadPart - t->start.QuadPart) * 1000ul
                                 / hfreq.QuadPart);
        return delta;
    }
}

#else /* _WIN32 && !EFIX64 && !EFI32 */

unsigned long mbedtls_timing_get_timer(struct mbedtls_timing_hr_time *val, int reset)
{
    struct _hr_time *t = (struct _hr_time *) val;

    if (reset) {
        gettimeofday(&t->start, NULL);
        return 0;
    } else {
        unsigned long delta;
        struct timeval now;
        gettimeofday(&now, NULL);
        delta = (now.tv_sec  - t->start.tv_sec) * 1000ul
                + (now.tv_usec - t->start.tv_usec) / 1000;
        return delta;
    }
}

#endif /* _WIN32 && !EFIX64 && !EFI32 */

/*
 * Set delays to watch
 */
void mbedtls_timing_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms)
{
    mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *) data;

    ctx->int_ms = int_ms;
    ctx->fin_ms = fin_ms;

    if (fin_ms != 0) {
        (void) mbedtls_timing_get_timer(&ctx->timer, 1);
    }
}

/*
 * Get number of delays expired
 */
int mbedtls_timing_get_delay(void *data)
{
    mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *) data;
    unsigned long elapsed_ms;

    if (ctx->fin_ms == 0) {
        return -1;
    }

    elapsed_ms = mbedtls_timing_get_timer(&ctx->timer, 0);

    if (elapsed_ms >= ctx->fin_ms) {
        return 2;
    }

    if (elapsed_ms >= ctx->int_ms) {
        return 1;
    }

    return 0;
}

/*
 * Get the final delay.
 */
uint32_t mbedtls_timing_get_final_delay(
    const mbedtls_timing_delay_context *data)
{
    return data->fin_ms;
}
#endif /* !MBEDTLS_TIMING_ALT */
#endif /* MBEDTLS_TIMING_C */
