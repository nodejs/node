/*
 * Common and shared functions used by multiple modules in the Mbed TLS
 * library.
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * Ensure gmtime_r is available even with -std=c99; must be defined before
 * mbedtls_config.h, which pulls in glibc's features.h. Harmless on other platforms
 * except OpenBSD, where it stops us accessing explicit_bzero.
 */
#if !defined(_POSIX_C_SOURCE) && !defined(__OpenBSD__)
#define _POSIX_C_SOURCE 200112L
#endif

#if !defined(_GNU_SOURCE)
/* Clang requires this to get support for explicit_bzero */
#define _GNU_SOURCE
#endif

#include "common.h"

#include "mbedtls/platform_util.h"
#include "mbedtls/platform.h"
#include "mbedtls/threading.h"

#include <stddef.h>

#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1 /* Ask for the C11 gmtime_s() and memset_s() if available */
#endif
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

// Detect platforms known to support explicit_bzero()
#if defined(__GLIBC__) && (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 25)
#define MBEDTLS_PLATFORM_HAS_EXPLICIT_BZERO 1
#elif (defined(__FreeBSD__) && (__FreeBSD_version >= 1100037)) || defined(__OpenBSD__)
#define MBEDTLS_PLATFORM_HAS_EXPLICIT_BZERO 1
#endif

#if !defined(MBEDTLS_PLATFORM_ZEROIZE_ALT)

#undef HAVE_MEMORY_SANITIZER
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#include <sanitizer/msan_interface.h>
#define HAVE_MEMORY_SANITIZER
#endif
#endif

/*
 * Where possible, we try to detect the presence of a platform-provided
 * secure memset, such as explicit_bzero(), that is safe against being optimized
 * out, and use that.
 *
 * For other platforms, we provide an implementation that aims not to be
 * optimized out by the compiler.
 *
 * This implementation for mbedtls_platform_zeroize() was inspired from Colin
 * Percival's blog article at:
 *
 * http://www.daemonology.net/blog/2014-09-04-how-to-zero-a-buffer.html
 *
 * It uses a volatile function pointer to the standard memset(). Because the
 * pointer is volatile the compiler expects it to change at
 * any time and will not optimize out the call that could potentially perform
 * other operations on the input buffer instead of just setting it to 0.
 * Nevertheless, as pointed out by davidtgoldblatt on Hacker News
 * (refer to http://www.daemonology.net/blog/2014-09-05-erratum.html for
 * details), optimizations of the following form are still possible:
 *
 * if (memset_func != memset)
 *     memset_func(buf, 0, len);
 *
 * Note that it is extremely difficult to guarantee that
 * the memset() call will not be optimized out by aggressive compilers
 * in a portable way. For this reason, Mbed TLS also provides the configuration
 * option MBEDTLS_PLATFORM_ZEROIZE_ALT, which allows users to configure
 * mbedtls_platform_zeroize() to use a suitable implementation for their
 * platform and needs.
 */
#if !defined(MBEDTLS_PLATFORM_HAS_EXPLICIT_BZERO) && !(defined(__STDC_LIB_EXT1__) && \
    !defined(__IAR_SYSTEMS_ICC__)) \
    && !defined(_WIN32)
static void *(*const volatile memset_func)(void *, int, size_t) = memset;
#endif

void mbedtls_platform_zeroize(void *buf, size_t len)
{
    if (len > 0) {
#if defined(MBEDTLS_PLATFORM_HAS_EXPLICIT_BZERO)
        explicit_bzero(buf, len);
#if defined(HAVE_MEMORY_SANITIZER)
        /* You'd think that Msan would recognize explicit_bzero() as
         * equivalent to bzero(), but it actually doesn't on several
         * platforms, including Linux (Ubuntu 20.04).
         * https://github.com/google/sanitizers/issues/1507
         * https://github.com/openssh/openssh-portable/commit/74433a19bb6f4cef607680fa4d1d7d81ca3826aa
         */
        __msan_unpoison(buf, len);
#endif
#elif defined(__STDC_LIB_EXT1__) && !defined(__IAR_SYSTEMS_ICC__)
        memset_s(buf, len, 0, len);
#elif defined(_WIN32)
        SecureZeroMemory(buf, len);
#else
        memset_func(buf, 0, len);
#endif

#if defined(__GNUC__)
        /* For clang and recent gcc, pretend that we have some assembly that reads the
         * zero'd memory as an additional protection against being optimised away. */
#if defined(__clang__) || (__GNUC__ >= 10)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla"
#elif defined(MBEDTLS_COMPILER_IS_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
#endif
        asm volatile ("" : : "m" (*(char (*)[len]) buf) :);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(MBEDTLS_COMPILER_IS_GCC)
#pragma GCC diagnostic pop
#endif
#endif
#endif
    }
}
#endif /* MBEDTLS_PLATFORM_ZEROIZE_ALT */

void mbedtls_zeroize_and_free(void *buf, size_t len)
{
    if (buf != NULL) {
        mbedtls_platform_zeroize(buf, len);
    }

    mbedtls_free(buf);
}

#if defined(MBEDTLS_HAVE_TIME_DATE) && !defined(MBEDTLS_PLATFORM_GMTIME_R_ALT)
#include <time.h>
#if !defined(_WIN32) && (defined(unix) || \
    defined(__unix) || defined(__unix__) || (defined(__APPLE__) && \
    defined(__MACH__)) || defined(__midipix__))
#include <unistd.h>
#endif /* !_WIN32 && (unix || __unix || __unix__ ||
        * (__APPLE__ && __MACH__) || __midipix__) */

#if !((defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809L) ||     \
    (defined(_POSIX_THREAD_SAFE_FUNCTIONS) &&                     \
    _POSIX_THREAD_SAFE_FUNCTIONS >= 200112L))
/*
 * This is a convenience shorthand macro to avoid checking the long
 * preprocessor conditions above. Ideally, we could expose this macro in
 * platform_util.h and simply use it in platform_util.c, threading.c and
 * threading.h. However, this macro is not part of the Mbed TLS public API, so
 * we keep it private by only defining it in this file
 */
#if !(defined(_WIN32) && !defined(EFIX64) && !defined(EFI32)) || \
    (defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
#define PLATFORM_UTIL_USE_GMTIME
#endif

#endif /* !( ( defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809L ) || \
             ( defined(_POSIX_THREAD_SAFE_FUNCTIONS ) && \
                _POSIX_THREAD_SAFE_FUNCTIONS >= 200112L ) ) */

struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *tt,
                                     struct tm *tm_buf)
{
#if defined(_WIN32) && !defined(PLATFORM_UTIL_USE_GMTIME)
#if defined(__STDC_LIB_EXT1__)
    return (gmtime_s(tt, tm_buf) == 0) ? NULL : tm_buf;
#else
    /* MSVC and mingw64 argument order and return value are inconsistent with the C11 standard */
    return (gmtime_s(tm_buf, tt) == 0) ? tm_buf : NULL;
#endif
#elif !defined(PLATFORM_UTIL_USE_GMTIME)
    return gmtime_r(tt, tm_buf);
#else
    struct tm *lt;

#if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_lock(&mbedtls_threading_gmtime_mutex) != 0) {
        return NULL;
    }
#endif /* MBEDTLS_THREADING_C */

    lt = gmtime(tt);

    if (lt != NULL) {
        memcpy(tm_buf, lt, sizeof(struct tm));
    }

#if defined(MBEDTLS_THREADING_C)
    if (mbedtls_mutex_unlock(&mbedtls_threading_gmtime_mutex) != 0) {
        return NULL;
    }
#endif /* MBEDTLS_THREADING_C */

    return (lt == NULL) ? NULL : tm_buf;
#endif /* _WIN32 && !EFIX64 && !EFI32 */
}
#endif /* MBEDTLS_HAVE_TIME_DATE && MBEDTLS_PLATFORM_GMTIME_R_ALT */

#if defined(MBEDTLS_TEST_HOOKS)
void (*mbedtls_test_hook_test_fail)(const char *, int, const char *);
#endif /* MBEDTLS_TEST_HOOKS */

#if defined(MBEDTLS_HAVE_TIME) && !defined(MBEDTLS_PLATFORM_MS_TIME_ALT)

#include <time.h>
#if !defined(_WIN32) && \
    (defined(unix) || defined(__unix) || defined(__unix__) || \
    (defined(__APPLE__) && defined(__MACH__)) || defined(__HAIKU__) || defined(__midipix__))
#include <unistd.h>
#endif \
    /* !_WIN32 && (unix || __unix || __unix__ || (__APPLE__ && __MACH__) || __HAIKU__ || __midipix__) */
#if (defined(_POSIX_VERSION) && _POSIX_VERSION >= 199309L) || defined(__HAIKU__)
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    int ret;
    struct timespec tv;
    mbedtls_ms_time_t current_ms;

#if defined(__linux__) && defined(CLOCK_BOOTTIME) || defined(__midipix__)
    ret = clock_gettime(CLOCK_BOOTTIME, &tv);
#else
    ret = clock_gettime(CLOCK_MONOTONIC, &tv);
#endif
    if (ret) {
        return time(NULL) * 1000;
    }

    current_ms = tv.tv_sec;

    return current_ms*1000 + tv.tv_nsec / 1000000;
}
#elif defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || \
    defined(__MINGW32__) || defined(_WIN64)
#include <windows.h>
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    FILETIME ct;
    mbedtls_ms_time_t current_ms;

    GetSystemTimeAsFileTime(&ct);
    current_ms = ((mbedtls_ms_time_t) ct.dwLowDateTime +
                  ((mbedtls_ms_time_t) (ct.dwHighDateTime) << 32LL))/10000;
    return current_ms;
}
#else
#error "No mbedtls_ms_time available"
#endif
#endif /* MBEDTLS_HAVE_TIME && !MBEDTLS_PLATFORM_MS_TIME_ALT */
