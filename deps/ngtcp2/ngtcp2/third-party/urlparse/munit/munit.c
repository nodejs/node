/* Copyright (c) 2013-2018 Evan Nemerson <evan@nemerson.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*** Configuration ***/

/* This is just where the output from the test goes.  It's really just
 * meant to let you choose stdout or stderr, but if anyone really want
 * to direct it to a file let me know, it would be fairly easy to
 * support. */
#if !defined(MUNIT_OUTPUT_FILE)
#  define MUNIT_OUTPUT_FILE stdout
#endif

/* This is a bit more useful; it tells µnit how to format the seconds in
 * timed tests.  If your tests run for longer you might want to reduce
 * it, and if your computer is really fast and your tests are tiny you
 * can increase it. */
#if !defined(MUNIT_TEST_TIME_FORMAT)
#  define MUNIT_TEST_TIME_FORMAT "0.8f"
#endif

/* If you have long test names you might want to consider bumping
 * this.  The result information takes 43 characters. */
#if !defined(MUNIT_TEST_NAME_LEN)
#  define MUNIT_TEST_NAME_LEN 37
#endif

/* If you don't like the timing information, you can disable it by
 * defining MUNIT_DISABLE_TIMING. */
#if !defined(MUNIT_DISABLE_TIMING)
#  define MUNIT_ENABLE_TIMING
#endif

/*** End configuration ***/

#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE < 200809L)
#  undef _POSIX_C_SOURCE
#endif
#if !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif

/* Solaris freaks out if you try to use a POSIX or SUS standard without
 * the "right" C standard. */
#if defined(_XOPEN_SOURCE)
#  undef _XOPEN_SOURCE
#endif

#if defined(__STDC_VERSION__)
#  if __STDC_VERSION__ >= 201112L
#    define _XOPEN_SOURCE 700
#  elif __STDC_VERSION__ >= 199901L
#    define _XOPEN_SOURCE 600
#  endif
#endif

/* Because, according to Microsoft, POSIX is deprecated.  You've got
 * to appreciate the chutzpah. */
#if defined(_MSC_VER) && !defined(_CRT_NONSTDC_NO_DEPRECATE)
#  define _CRT_NONSTDC_NO_DEPRECATE
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#  include <stdbool.h>
#elif defined(_WIN32)
/* https://msdn.microsoft.com/en-us/library/tf4dy80a.aspx */
#endif

#include <limits.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>

#if !defined(MUNIT_NO_NL_LANGINFO) && !defined(_WIN32)
#  define MUNIT_NL_LANGINFO
#  include <locale.h>
#  include <langinfo.h>
#  include <strings.h>
#endif

#if !defined(_WIN32)
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#else
#  include <windows.h>
#  include <io.h>
#  include <fcntl.h>
#  if !defined(STDERR_FILENO)
#    define STDERR_FILENO _fileno(stderr)
#  endif
#endif

#include "munit.h"

#define MUNIT_STRINGIFY(x) #x
#define MUNIT_XSTRINGIFY(x) MUNIT_STRINGIFY(x)

#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__SUNPRO_CC) ||  \
  defined(__IBMCPP__)
#  define MUNIT_THREAD_LOCAL __thread
#elif (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201102L)) ||          \
  defined(_Thread_local)
#  define MUNIT_THREAD_LOCAL _Thread_local
#elif defined(_WIN32)
#  define MUNIT_THREAD_LOCAL __declspec(thread)
#endif

/* MSVC 12.0 will emit a warning at /W4 for code like 'do { ... }
 * while (0)', or 'do { ... } while (1)'.  I'm pretty sure nobody
 * at Microsoft compiles with /W4. */
#if defined(_MSC_VER) && (_MSC_VER <= 1800)
#  pragma warning(disable : 4127)
#endif

#if defined(_WIN32) || defined(__EMSCRIPTEN__)
#  define MUNIT_NO_FORK
#endif

#if defined(__EMSCRIPTEN__)
#  define MUNIT_NO_BUFFER
#endif

/*** Logging ***/

static MunitLogLevel munit_log_level_visible = MUNIT_LOG_INFO;
static MunitLogLevel munit_log_level_fatal = MUNIT_LOG_ERROR;

#if defined(MUNIT_THREAD_LOCAL)
static MUNIT_THREAD_LOCAL munit_bool munit_error_jmp_buf_valid = 0;
static MUNIT_THREAD_LOCAL jmp_buf munit_error_jmp_buf;
#endif

/* At certain warning levels, mingw will trigger warnings about
 * suggesting the format attribute, which we've explicity *not* set
 * because it will then choke on our attempts to use the MS-specific
 * I64 modifier for size_t (which we have to use since MSVC doesn't
 * support the C99 z modifier). */

#if defined(__MINGW32__) || defined(__MINGW64__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif

MUNIT_PRINTF(5, 0)
static void munit_logf_exv(MunitLogLevel level, FILE *fp, const char *filename,
                           int line, const char *format, va_list ap) {
  if (level < munit_log_level_visible)
    return;

  switch (level) {
  case MUNIT_LOG_DEBUG:
    fputs("Debug", fp);
    break;
  case MUNIT_LOG_INFO:
    fputs("Info", fp);
    break;
  case MUNIT_LOG_WARNING:
    fputs("Warning", fp);
    break;
  case MUNIT_LOG_ERROR:
    fputs("Error", fp);
    break;
  default:
    munit_logf_ex(MUNIT_LOG_ERROR, filename, line, "Invalid log level (%d)",
                  level);
    return;
  }

  fputs(": ", fp);
  if (filename != NULL)
    fprintf(fp, "%s:%d: ", filename, line);
  vfprintf(fp, format, ap);
  fputc('\n', fp);
}

MUNIT_PRINTF(3, 4)
static void munit_logf_internal(MunitLogLevel level, FILE *fp,
                                const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  munit_logf_exv(level, fp, NULL, 0, format, ap);
  va_end(ap);
}

static void munit_log_internal(MunitLogLevel level, FILE *fp,
                               const char *message) {
  munit_logf_internal(level, fp, "%s", message);
}

void munit_logf_ex(MunitLogLevel level, const char *filename, int line,
                   const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  munit_logf_exv(level, stderr, filename, line, format, ap);
  va_end(ap);

  if (level >= munit_log_level_fatal) {
#if defined(MUNIT_THREAD_LOCAL)
    if (munit_error_jmp_buf_valid)
      longjmp(munit_error_jmp_buf, 1);
#endif
    abort();
  }
}

void munit_errorf_ex(const char *filename, int line, const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  munit_logf_exv(MUNIT_LOG_ERROR, stderr, filename, line, format, ap);
  va_end(ap);

#if defined(MUNIT_THREAD_LOCAL)
  if (munit_error_jmp_buf_valid)
    longjmp(munit_error_jmp_buf, 1);
#endif
  abort();
}

#if defined(__MINGW32__) || defined(__MINGW64__)
#  pragma GCC diagnostic pop
#endif

#if !defined(MUNIT_STRERROR_LEN)
#  define MUNIT_STRERROR_LEN 80
#endif

static void munit_log_errno(MunitLogLevel level, FILE *fp, const char *msg) {
#if defined(MUNIT_NO_STRERROR_R) ||                                            \
  (defined(__MINGW32__) && !defined(MINGW_HAS_SECURE_API))
  munit_logf_internal(level, fp, "%s: %s (%d)", msg, strerror(errno), errno);
#else
  char munit_error_str[MUNIT_STRERROR_LEN];
  munit_error_str[0] = '\0';

#  if !defined(_WIN32)
  strerror_r(errno, munit_error_str, MUNIT_STRERROR_LEN);
#  else
  strerror_s(munit_error_str, MUNIT_STRERROR_LEN, errno);
#  endif

  munit_logf_internal(level, fp, "%s: %s (%d)", msg, munit_error_str, errno);
#endif
}

/*** Memory allocation ***/

void *munit_malloc_ex(const char *filename, int line, size_t size) {
  void *ptr;

  if (size == 0)
    return NULL;

  ptr = calloc(1, size);
  if (MUNIT_UNLIKELY(ptr == NULL)) {
    munit_logf_ex(MUNIT_LOG_ERROR, filename, line,
                  "Failed to allocate %" MUNIT_SIZE_MODIFIER "u bytes.", size);
  }

  return ptr;
}

/*** Timer code ***/

#if defined(MUNIT_ENABLE_TIMING)

#  define psnip_uint64_t munit_uint64_t
#  define psnip_uint32_t munit_uint32_t

/* Code copied from portable-snippets
 * <https://github.com/nemequ/portable-snippets/>.  If you need to
 * change something, please do it there so we can keep the code in
 * sync. */

/* Clocks (v1)
 * Portable Snippets - https://gitub.com/nemequ/portable-snippets
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 *   To the extent possible under law, the authors have waived all
 *   copyright and related or neighboring rights to this code.  For
 *   details, see the Creative Commons Zero 1.0 Universal license at
 *   https://creativecommons.org/publicdomain/zero/1.0/
 */

#  if !defined(PSNIP_CLOCK_H)
#    define PSNIP_CLOCK_H

#    if !defined(psnip_uint64_t)
#      include "../exact-int/exact-int.h"
#    endif

#    if !defined(PSNIP_CLOCK_STATIC_INLINE)
#      if defined(__GNUC__)
#        define PSNIP_CLOCK__COMPILER_ATTRIBUTES __attribute__((__unused__))
#      else
#        define PSNIP_CLOCK__COMPILER_ATTRIBUTES
#      endif

#      define PSNIP_CLOCK__FUNCTION PSNIP_CLOCK__COMPILER_ATTRIBUTES static
#    endif

enum PsnipClockType {
  /* This clock provides the current time, in units since 1970-01-01
   * 00:00:00 UTC not including leap seconds.  In other words, UNIX
   * time.  Keep in mind that this clock doesn't account for leap
   * seconds, and can go backwards (think NTP adjustments). */
  PSNIP_CLOCK_TYPE_WALL = 1,
  /* The CPU time is a clock which increases only when the current
   * process is active (i.e., it doesn't increment while blocking on
   * I/O). */
  PSNIP_CLOCK_TYPE_CPU = 2,
  /* Monotonic time is always running (unlike CPU time), but it only
     ever moves forward unless you reboot the system.  Things like NTP
     adjustments have no effect on this clock. */
  PSNIP_CLOCK_TYPE_MONOTONIC = 3
};

struct PsnipClockTimespec {
  psnip_uint64_t seconds;
  psnip_uint64_t nanoseconds;
};

/* Methods we support: */

#    define PSNIP_CLOCK_METHOD_CLOCK_GETTIME 1
#    define PSNIP_CLOCK_METHOD_TIME 2
#    define PSNIP_CLOCK_METHOD_GETTIMEOFDAY 3
#    define PSNIP_CLOCK_METHOD_QUERYPERFORMANCECOUNTER 4
#    define PSNIP_CLOCK_METHOD_MACH_ABSOLUTE_TIME 5
#    define PSNIP_CLOCK_METHOD_CLOCK 6
#    define PSNIP_CLOCK_METHOD_GETPROCESSTIMES 7
#    define PSNIP_CLOCK_METHOD_GETRUSAGE 8
#    define PSNIP_CLOCK_METHOD_GETSYSTEMTIMEPRECISEASFILETIME 9
#    define PSNIP_CLOCK_METHOD_GETTICKCOUNT64 10

#    include <assert.h>

#    if defined(HEDLEY_UNREACHABLE)
#      define PSNIP_CLOCK_UNREACHABLE() HEDLEY_UNREACHABLE()
#    else
#      define PSNIP_CLOCK_UNREACHABLE() assert(0)
#    endif

/* Choose an implementation */

/* #undef PSNIP_CLOCK_WALL_METHOD */
/* #undef PSNIP_CLOCK_CPU_METHOD */
/* #undef PSNIP_CLOCK_MONOTONIC_METHOD */

/* We want to be able to detect the libc implementation, so we include
   <limits.h> (<features.h> isn't available everywhere). */

#    if defined(__unix__) || defined(__unix) || defined(__linux__)
#      include <limits.h>
#      include <unistd.h>
#    endif

#    if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
/* These are known to work without librt.  If you know of others
 * please let us know so we can add them. */
#      if (defined(__GLIBC__) &&                                               \
           (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 17))) ||    \
        (defined(__FreeBSD__))
#        define PSNIP_CLOCK_HAVE_CLOCK_GETTIME
#      elif !defined(PSNIP_CLOCK_NO_LIBRT)
#        define PSNIP_CLOCK_HAVE_CLOCK_GETTIME
#      endif
#    endif

#    if defined(_WIN32)
#      if !defined(PSNIP_CLOCK_CPU_METHOD)
#        define PSNIP_CLOCK_CPU_METHOD PSNIP_CLOCK_METHOD_GETPROCESSTIMES
#      endif
#      if !defined(PSNIP_CLOCK_MONOTONIC_METHOD)
#        define PSNIP_CLOCK_MONOTONIC_METHOD                                   \
          PSNIP_CLOCK_METHOD_QUERYPERFORMANCECOUNTER
#      endif
#    endif

#    if defined(__MACH__) && !defined(__gnu_hurd__)
#      if !defined(PSNIP_CLOCK_MONOTONIC_METHOD)
#        define PSNIP_CLOCK_MONOTONIC_METHOD                                   \
          PSNIP_CLOCK_METHOD_MACH_ABSOLUTE_TIME
#      endif
#    endif

#    if defined(PSNIP_CLOCK_HAVE_CLOCK_GETTIME)
#      include <time.h>
#      if !defined(PSNIP_CLOCK_WALL_METHOD)
#        if defined(CLOCK_REALTIME_PRECISE)
#          define PSNIP_CLOCK_WALL_METHOD PSNIP_CLOCK_METHOD_CLOCK_GETTIME
#          define PSNIP_CLOCK_CLOCK_GETTIME_WALL CLOCK_REALTIME_PRECISE
#        elif !defined(__sun)
#          define PSNIP_CLOCK_WALL_METHOD PSNIP_CLOCK_METHOD_CLOCK_GETTIME
#          define PSNIP_CLOCK_CLOCK_GETTIME_WALL CLOCK_REALTIME
#        endif
#      endif
#      if !defined(PSNIP_CLOCK_CPU_METHOD)
#        if defined(_POSIX_CPUTIME) || defined(CLOCK_PROCESS_CPUTIME_ID)
#          define PSNIP_CLOCK_CPU_METHOD PSNIP_CLOCK_METHOD_CLOCK_GETTIME
#          define PSNIP_CLOCK_CLOCK_GETTIME_CPU CLOCK_PROCESS_CPUTIME_ID
#        elif defined(CLOCK_VIRTUAL)
#          define PSNIP_CLOCK_CPU_METHOD PSNIP_CLOCK_METHOD_CLOCK_GETTIME
#          define PSNIP_CLOCK_CLOCK_GETTIME_CPU CLOCK_VIRTUAL
#        endif
#      endif
#      if !defined(PSNIP_CLOCK_MONOTONIC_METHOD)
#        if defined(CLOCK_MONOTONIC_RAW)
#          define PSNIP_CLOCK_MONOTONIC_METHOD PSNIP_CLOCK_METHOD_CLOCK_GETTIME
#          define PSNIP_CLOCK_CLOCK_GETTIME_MONOTONIC CLOCK_MONOTONIC
#        elif defined(CLOCK_MONOTONIC_PRECISE)
#          define PSNIP_CLOCK_MONOTONIC_METHOD PSNIP_CLOCK_METHOD_CLOCK_GETTIME
#          define PSNIP_CLOCK_CLOCK_GETTIME_MONOTONIC CLOCK_MONOTONIC_PRECISE
#        elif defined(_POSIX_MONOTONIC_CLOCK) || defined(CLOCK_MONOTONIC)
#          define PSNIP_CLOCK_MONOTONIC_METHOD PSNIP_CLOCK_METHOD_CLOCK_GETTIME
#          define PSNIP_CLOCK_CLOCK_GETTIME_MONOTONIC CLOCK_MONOTONIC
#        endif
#      endif
#    endif

#    if defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L)
#      if !defined(PSNIP_CLOCK_WALL_METHOD)
#        define PSNIP_CLOCK_WALL_METHOD PSNIP_CLOCK_METHOD_GETTIMEOFDAY
#      endif
#    endif

#    if !defined(PSNIP_CLOCK_WALL_METHOD)
#      define PSNIP_CLOCK_WALL_METHOD PSNIP_CLOCK_METHOD_TIME
#    endif

#    if !defined(PSNIP_CLOCK_CPU_METHOD)
#      define PSNIP_CLOCK_CPU_METHOD PSNIP_CLOCK_METHOD_CLOCK
#    endif

/* Primarily here for testing. */
#    if !defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                              \
      defined(PSNIP_CLOCK_REQUIRE_MONOTONIC)
#      error No monotonic clock found.
#    endif

/* Implementations */

#    if (defined(PSNIP_CLOCK_CPU_METHOD) &&                                    \
         (PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME)) ||      \
      (defined(PSNIP_CLOCK_WALL_METHOD) &&                                     \
       (PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME)) ||       \
      (defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                                \
       (PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME)) ||  \
      (defined(PSNIP_CLOCK_CPU_METHOD) &&                                      \
       (PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_CLOCK)) ||                \
      (defined(PSNIP_CLOCK_WALL_METHOD) &&                                     \
       (PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_CLOCK)) ||               \
      (defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                                \
       (PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_CLOCK)) ||          \
      (defined(PSNIP_CLOCK_CPU_METHOD) &&                                      \
       (PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_TIME)) ||                 \
      (defined(PSNIP_CLOCK_WALL_METHOD) &&                                     \
       (PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_TIME)) ||                \
      (defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                                \
       (PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_TIME))
#      include <time.h>
#    endif

#    if (defined(PSNIP_CLOCK_CPU_METHOD) &&                                    \
         (PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_GETTIMEOFDAY)) ||       \
      (defined(PSNIP_CLOCK_WALL_METHOD) &&                                     \
       (PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_GETTIMEOFDAY)) ||        \
      (defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                                \
       (PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_GETTIMEOFDAY))
#      include <sys/time.h>
#    endif

#    if (defined(PSNIP_CLOCK_CPU_METHOD) &&                                    \
         (PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_GETPROCESSTIMES)) ||    \
      (defined(PSNIP_CLOCK_WALL_METHOD) &&                                     \
       (PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_GETPROCESSTIMES)) ||     \
      (defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                                \
       (PSNIP_CLOCK_MONOTONIC_METHOD ==                                        \
        PSNIP_CLOCK_METHOD_GETPROCESSTIMES)) ||                                \
      (defined(PSNIP_CLOCK_CPU_METHOD) &&                                      \
       (PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_GETTICKCOUNT64)) ||       \
      (defined(PSNIP_CLOCK_WALL_METHOD) &&                                     \
       (PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_GETTICKCOUNT64)) ||      \
      (defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                                \
       (PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_GETTICKCOUNT64))
#      include <windows.h>
#    endif

#    if (defined(PSNIP_CLOCK_CPU_METHOD) &&                                    \
         (PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_GETRUSAGE)) ||          \
      (defined(PSNIP_CLOCK_WALL_METHOD) &&                                     \
       (PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_GETRUSAGE)) ||           \
      (defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                                \
       (PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_GETRUSAGE))
#      include <sys/time.h>
#      include <sys/resource.h>
#    endif

#    if (defined(PSNIP_CLOCK_CPU_METHOD) &&                                    \
         (PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_MACH_ABSOLUTE_TIME)) || \
      (defined(PSNIP_CLOCK_WALL_METHOD) &&                                     \
       (PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_MACH_ABSOLUTE_TIME)) ||  \
      (defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                                \
       (PSNIP_CLOCK_MONOTONIC_METHOD ==                                        \
        PSNIP_CLOCK_METHOD_MACH_ABSOLUTE_TIME))
#      include <CoreServices/CoreServices.h>
#      include <mach/mach.h>
#      include <mach/mach_time.h>
#    endif

/*** Implementations ***/

#    define PSNIP_CLOCK_NSEC_PER_SEC ((psnip_uint32_t)(1000000000ULL))

#    if (defined(PSNIP_CLOCK_CPU_METHOD) &&                                    \
         (PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME)) ||      \
      (defined(PSNIP_CLOCK_WALL_METHOD) &&                                     \
       (PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME)) ||       \
      (defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                                \
       (PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME))
PSNIP_CLOCK__FUNCTION psnip_uint32_t
psnip_clock__clock_getres(clockid_t clk_id) {
  struct timespec res;
  int r;

  r = clock_getres(clk_id, &res);
  if (r != 0)
    return 0;

  return (psnip_uint32_t)(PSNIP_CLOCK_NSEC_PER_SEC /
                          (psnip_uint64_t)res.tv_nsec);
}

PSNIP_CLOCK__FUNCTION int
psnip_clock__clock_gettime(clockid_t clk_id, struct PsnipClockTimespec *res) {
  struct timespec ts;

  if (clock_gettime(clk_id, &ts) != 0)
    return -10;

  res->seconds = (psnip_uint64_t)(ts.tv_sec);
  res->nanoseconds = (psnip_uint64_t)(ts.tv_nsec);

  return 0;
}
#    endif

PSNIP_CLOCK__FUNCTION psnip_uint32_t psnip_clock_wall_get_precision(void) {
#    if !defined(PSNIP_CLOCK_WALL_METHOD)
  return 0;
#    elif defined(PSNIP_CLOCK_WALL_METHOD) &&                                  \
      PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME
  return psnip_clock__clock_getres(PSNIP_CLOCK_CLOCK_GETTIME_WALL);
#    elif defined(PSNIP_CLOCK_WALL_METHOD) &&                                  \
      PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_GETTIMEOFDAY
  return 1000000;
#    elif defined(PSNIP_CLOCK_WALL_METHOD) &&                                  \
      PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_TIME
  return 1;
#    else
  return 0;
#    endif
}

PSNIP_CLOCK__FUNCTION int
psnip_clock_wall_get_time(struct PsnipClockTimespec *res) {
#    if !defined(PSNIP_CLOCK_WALL_METHOD)
  (void)res;

  return -2;
#    elif defined(PSNIP_CLOCK_WALL_METHOD) &&                                  \
      PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME
  return psnip_clock__clock_gettime(PSNIP_CLOCK_CLOCK_GETTIME_WALL, res);
#    elif defined(PSNIP_CLOCK_WALL_METHOD) &&                                  \
      PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_TIME
  res->seconds = time(NULL);
  res->nanoseconds = 0;
#    elif defined(PSNIP_CLOCK_WALL_METHOD) &&                                  \
      PSNIP_CLOCK_WALL_METHOD == PSNIP_CLOCK_METHOD_GETTIMEOFDAY
  struct timeval tv;

  if (gettimeofday(&tv, NULL) != 0)
    return -6;

  res->seconds = (psnip_uint64_t)tv.tv_sec;
  res->nanoseconds = (psnip_uint64_t)tv.tv_usec * 1000;
#    else
  (void)res;

  return -2;
#    endif

  return 0;
}

PSNIP_CLOCK__FUNCTION psnip_uint32_t psnip_clock_cpu_get_precision(void) {
#    if !defined(PSNIP_CLOCK_CPU_METHOD)
  return 0;
#    elif defined(PSNIP_CLOCK_CPU_METHOD) &&                                   \
      PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME
  return psnip_clock__clock_getres(PSNIP_CLOCK_CLOCK_GETTIME_CPU);
#    elif defined(PSNIP_CLOCK_CPU_METHOD) &&                                   \
      PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_CLOCK
  return CLOCKS_PER_SEC;
#    elif defined(PSNIP_CLOCK_CPU_METHOD) &&                                   \
      PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_GETPROCESSTIMES
  return PSNIP_CLOCK_NSEC_PER_SEC / 100;
#    else
  return 0;
#    endif
}

PSNIP_CLOCK__FUNCTION int
psnip_clock_cpu_get_time(struct PsnipClockTimespec *res) {
#    if !defined(PSNIP_CLOCK_CPU_METHOD)
  (void)res;
  return -2;
#    elif defined(PSNIP_CLOCK_CPU_METHOD) &&                                   \
      PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME
  return psnip_clock__clock_gettime(PSNIP_CLOCK_CLOCK_GETTIME_CPU, res);
#    elif defined(PSNIP_CLOCK_CPU_METHOD) &&                                   \
      PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_CLOCK
  clock_t t = clock();
  if (t == ((clock_t)-1))
    return -5;
  res->seconds = t / CLOCKS_PER_SEC;
  res->nanoseconds =
    (t % CLOCKS_PER_SEC) * (PSNIP_CLOCK_NSEC_PER_SEC / CLOCKS_PER_SEC);
#    elif defined(PSNIP_CLOCK_CPU_METHOD) &&                                   \
      PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_GETPROCESSTIMES
  FILETIME CreationTime, ExitTime, KernelTime, UserTime;
  LARGE_INTEGER date, adjust;

  if (!GetProcessTimes(GetCurrentProcess(), &CreationTime, &ExitTime,
                       &KernelTime, &UserTime))
    return -7;

  /* http://www.frenk.com/2009/12/convert-filetime-to-unix-timestamp/ */
  date.HighPart = (LONG)UserTime.dwHighDateTime;
  date.LowPart = UserTime.dwLowDateTime;
  adjust.QuadPart = 11644473600000 * 10000;
  date.QuadPart -= adjust.QuadPart;

  res->seconds = (psnip_uint64_t)(date.QuadPart / 10000000);
  res->nanoseconds = (psnip_uint64_t)(date.QuadPart % 10000000) *
                     (PSNIP_CLOCK_NSEC_PER_SEC / 100);
#    elif PSNIP_CLOCK_CPU_METHOD == PSNIP_CLOCK_METHOD_GETRUSAGE
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != 0)
    return -8;

  res->seconds = usage.ru_utime.tv_sec;
  res->nanoseconds = tv.tv_usec * 1000;
#    else
  (void)res;
  return -2;
#    endif

  return 0;
}

PSNIP_CLOCK__FUNCTION psnip_uint32_t psnip_clock_monotonic_get_precision(void) {
#    if !defined(PSNIP_CLOCK_MONOTONIC_METHOD)
  return 0;
#    elif defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                             \
      PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME
  return psnip_clock__clock_getres(PSNIP_CLOCK_CLOCK_GETTIME_MONOTONIC);
#    elif defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                             \
      PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_MACH_ABSOLUTE_TIME
  static mach_timebase_info_data_t tbi = {
    0,
  };
  if (tbi.denom == 0)
    mach_timebase_info(&tbi);
  return (psnip_uint32_t)(tbi.numer / tbi.denom);
#    elif defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                             \
      PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_GETTICKCOUNT64
  return 1000;
#    elif defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                             \
      PSNIP_CLOCK_MONOTONIC_METHOD ==                                          \
        PSNIP_CLOCK_METHOD_QUERYPERFORMANCECOUNTER
  LARGE_INTEGER Frequency;
  QueryPerformanceFrequency(&Frequency);
  return (psnip_uint32_t)((Frequency.QuadPart > PSNIP_CLOCK_NSEC_PER_SEC)
                            ? PSNIP_CLOCK_NSEC_PER_SEC
                            : Frequency.QuadPart);
#    else
  return 0;
#    endif
}

PSNIP_CLOCK__FUNCTION int
psnip_clock_monotonic_get_time(struct PsnipClockTimespec *res) {
#    if !defined(PSNIP_CLOCK_MONOTONIC_METHOD)
  (void)res;
  return -2;
#    elif defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                             \
      PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_CLOCK_GETTIME
  return psnip_clock__clock_gettime(PSNIP_CLOCK_CLOCK_GETTIME_MONOTONIC, res);
#    elif defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                             \
      PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_MACH_ABSOLUTE_TIME
  psnip_uint64_t nsec = mach_absolute_time();
  static mach_timebase_info_data_t tbi = {
    0,
  };
  if (tbi.denom == 0)
    mach_timebase_info(&tbi);
  nsec *= ((psnip_uint64_t)tbi.numer) / ((psnip_uint64_t)tbi.denom);
  res->seconds = nsec / PSNIP_CLOCK_NSEC_PER_SEC;
  res->nanoseconds = nsec % PSNIP_CLOCK_NSEC_PER_SEC;
#    elif defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                             \
      PSNIP_CLOCK_MONOTONIC_METHOD ==                                          \
        PSNIP_CLOCK_METHOD_QUERYPERFORMANCECOUNTER
  LARGE_INTEGER t, f;
  if (QueryPerformanceCounter(&t) == 0)
    return -12;

  QueryPerformanceFrequency(&f);
  res->seconds = (psnip_uint64_t)(t.QuadPart / f.QuadPart);
  res->nanoseconds = (psnip_uint64_t)(t.QuadPart % f.QuadPart);
  if (f.QuadPart > PSNIP_CLOCK_NSEC_PER_SEC)
    res->nanoseconds /= (psnip_uint64_t)f.QuadPart / PSNIP_CLOCK_NSEC_PER_SEC;
  else
    res->nanoseconds *= PSNIP_CLOCK_NSEC_PER_SEC / (psnip_uint64_t)f.QuadPart;
#    elif defined(PSNIP_CLOCK_MONOTONIC_METHOD) &&                             \
      PSNIP_CLOCK_MONOTONIC_METHOD == PSNIP_CLOCK_METHOD_GETTICKCOUNT64
  const ULONGLONG msec = GetTickCount64();
  res->seconds = msec / 1000;
  res->nanoseconds = sec % 1000;
#    else
  return -2;
#    endif

  return 0;
}

/* Returns the number of ticks per second for the specified clock.
 * For example, a clock with millisecond precision would return 1000,
 * and a clock with 1 second (such as the time() function) would
 * return 1.
 *
 * If the requested clock isn't available, it will return 0.
 * Hopefully this will be rare, but if it happens to you please let us
 * know so we can work on finding a way to support your system.
 *
 * Note that different clocks on the same system often have a
 * different precisions.
 */
PSNIP_CLOCK__FUNCTION psnip_uint32_t
psnip_clock_get_precision(enum PsnipClockType clock_type) {
  switch (clock_type) {
  case PSNIP_CLOCK_TYPE_MONOTONIC:
    return psnip_clock_monotonic_get_precision();
  case PSNIP_CLOCK_TYPE_CPU:
    return psnip_clock_cpu_get_precision();
  case PSNIP_CLOCK_TYPE_WALL:
    return psnip_clock_wall_get_precision();
  }

  PSNIP_CLOCK_UNREACHABLE();
  return 0;
}

/* Set the provided timespec to the requested time.  Returns 0 on
 * success, or a negative value on failure. */
PSNIP_CLOCK__FUNCTION int psnip_clock_get_time(enum PsnipClockType clock_type,
                                               struct PsnipClockTimespec *res) {
  assert(res != NULL);

  switch (clock_type) {
  case PSNIP_CLOCK_TYPE_MONOTONIC:
    return psnip_clock_monotonic_get_time(res);
  case PSNIP_CLOCK_TYPE_CPU:
    return psnip_clock_cpu_get_time(res);
  case PSNIP_CLOCK_TYPE_WALL:
    return psnip_clock_wall_get_time(res);
  }

  return -1;
}

#  endif /* !defined(PSNIP_CLOCK_H) */

static psnip_uint64_t munit_clock_get_elapsed(struct PsnipClockTimespec *start,
                                              struct PsnipClockTimespec *end) {
  psnip_uint64_t r = (end->seconds - start->seconds) * PSNIP_CLOCK_NSEC_PER_SEC;
  if (end->nanoseconds < start->nanoseconds) {
    return r - (start->nanoseconds - end->nanoseconds);
  }

  return r + (end->nanoseconds - start->nanoseconds);
}

#else
#  include <time.h>
#endif /* defined(MUNIT_ENABLE_TIMING) */

/*** PRNG stuff ***/

/* This is (unless I screwed up, which is entirely possible) the
 * version of PCG with 32-bit state.  It was chosen because it has a
 * small enough state that we should reliably be able to use CAS
 * instead of requiring a lock for thread-safety.
 *
 * If I did screw up, I probably will not bother changing it unless
 * there is a significant bias.  It's really not important this be
 * particularly strong, as long as it is fairly random it's much more
 * important that it be reproducible, so bug reports have a better
 * chance of being reproducible. */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) &&              \
  !defined(__STDC_NO_ATOMICS__) && !defined(__EMSCRIPTEN__) &&                 \
  (!defined(__GNUC_MINOR__) || (__GNUC__ > 4) ||                               \
   (__GNUC__ == 4 && __GNUC_MINOR__ > 8))
#  define HAVE_STDATOMIC
#elif defined(__clang__)
#  if __has_extension(c_atomic)
#    define HAVE_CLANG_ATOMICS
#  endif
#endif

/* Workaround for http://llvm.org/bugs/show_bug.cgi?id=26911 */
#if defined(__clang__) && defined(_WIN32)
#  undef HAVE_STDATOMIC
#  if defined(__c2__)
#    undef HAVE_CLANG_ATOMICS
#  endif
#endif

#if defined(_OPENMP)
#  define ATOMIC_UINT32_T uint32_t
#elif defined(HAVE_STDATOMIC)
#  include <stdatomic.h>
#  define ATOMIC_UINT32_T _Atomic uint32_t
#elif defined(HAVE_CLANG_ATOMICS)
#  define ATOMIC_UINT32_T _Atomic uint32_t
#elif defined(_WIN32)
#  define ATOMIC_UINT32_T volatile LONG
#else
#  define ATOMIC_UINT32_T volatile uint32_t
#endif

static ATOMIC_UINT32_T munit_rand_state = 42;

#if defined(_OPENMP)
static inline void munit_atomic_store(ATOMIC_UINT32_T *dest,
                                      ATOMIC_UINT32_T value) {
#  pragma omp critical(munit_atomics)
  *dest = value;
}

static inline uint32_t munit_atomic_load(ATOMIC_UINT32_T *src) {
  int ret;
#  pragma omp critical(munit_atomics)
  ret = *src;
  return ret;
}

static inline uint32_t munit_atomic_cas(ATOMIC_UINT32_T *dest,
                                        ATOMIC_UINT32_T *expected,
                                        ATOMIC_UINT32_T desired) {
  munit_bool ret;

#  pragma omp critical(munit_atomics)
  {
    if (*dest == *expected) {
      *dest = desired;
      ret = 1;
    } else {
      ret = 0;
    }
  }

  return ret;
}
#elif defined(HAVE_STDATOMIC)
#  define munit_atomic_store(dest, value) atomic_store(dest, value)
#  define munit_atomic_load(src) atomic_load(src)
#  define munit_atomic_cas(dest, expected, value)                              \
    atomic_compare_exchange_weak(dest, expected, value)
#elif defined(HAVE_CLANG_ATOMICS)
#  define munit_atomic_store(dest, value)                                      \
    __c11_atomic_store(dest, value, __ATOMIC_SEQ_CST)
#  define munit_atomic_load(src) __c11_atomic_load(src, __ATOMIC_SEQ_CST)
#  define munit_atomic_cas(dest, expected, value)                              \
    __c11_atomic_compare_exchange_weak(dest, expected, value,                  \
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#elif defined(__GNUC__) && (__GNUC__ > 4) ||                                   \
  (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
#  define munit_atomic_store(dest, value)                                      \
    __atomic_store_n(dest, value, __ATOMIC_SEQ_CST)
#  define munit_atomic_load(src) __atomic_load_n(src, __ATOMIC_SEQ_CST)
#  define munit_atomic_cas(dest, expected, value)                              \
    __atomic_compare_exchange_n(dest, expected, value, 1, __ATOMIC_SEQ_CST,    \
                                __ATOMIC_SEQ_CST)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#  define munit_atomic_store(dest, value)                                      \
    do {                                                                       \
      *(dest) = (value);                                                       \
    } while (0)
#  define munit_atomic_load(src) (*(src))
#  define munit_atomic_cas(dest, expected, value)                              \
    __sync_bool_compare_and_swap(dest, *expected, value)
#elif defined(_WIN32) /* Untested */
#  define munit_atomic_store(dest, value)                                      \
    do {                                                                       \
      *(dest) = (value);                                                       \
    } while (0)
#  define munit_atomic_load(src) (*(src))
#  define munit_atomic_cas(dest, expected, value)                              \
    InterlockedCompareExchange((dest), (value), *(expected))
#else
#  warning No atomic implementation, PRNG will not be thread-safe
#  define munit_atomic_store(dest, value)                                      \
    do {                                                                       \
      *(dest) = (value);                                                       \
    } while (0)
#  define munit_atomic_load(src) (*(src))
static inline munit_bool munit_atomic_cas(ATOMIC_UINT32_T *dest,
                                          ATOMIC_UINT32_T *expected,
                                          ATOMIC_UINT32_T desired) {
  if (*dest == *expected) {
    *dest = desired;
    return 1;
  } else {
    return 0;
  }
}
#endif

#define MUNIT_PRNG_MULTIPLIER (747796405U)
#define MUNIT_PRNG_INCREMENT (1729U)

static munit_uint32_t munit_rand_next_state(munit_uint32_t state) {
  return state * MUNIT_PRNG_MULTIPLIER + MUNIT_PRNG_INCREMENT;
}

static munit_uint32_t munit_rand_from_state(munit_uint32_t state) {
  munit_uint32_t res = ((state >> ((state >> 28) + 4)) ^ state) * (277803737U);
  res ^= res >> 22;
  return res;
}

void munit_rand_seed(munit_uint32_t seed) {
  munit_uint32_t state = munit_rand_next_state(seed + MUNIT_PRNG_INCREMENT);
  munit_atomic_store(&munit_rand_state, state);
}

static munit_uint32_t munit_rand_generate_seed(void) {
  munit_uint32_t seed, state;
#if defined(MUNIT_ENABLE_TIMING)
  struct PsnipClockTimespec wc = {
    0,
  };

  psnip_clock_get_time(PSNIP_CLOCK_TYPE_WALL, &wc);
  seed = (munit_uint32_t)wc.nanoseconds;
#else
  seed = (munit_uint32_t)time(NULL);
#endif

  state = munit_rand_next_state(seed + MUNIT_PRNG_INCREMENT);
  return munit_rand_from_state(state);
}

static munit_uint32_t munit_rand_state_uint32(munit_uint32_t *state) {
  const munit_uint32_t old = *state;
  *state = munit_rand_next_state(old);
  return munit_rand_from_state(old);
}

munit_uint32_t munit_rand_uint32(void) {
  munit_uint32_t old, state;

  do {
    old = munit_atomic_load(&munit_rand_state);
    state = munit_rand_next_state(old);
  } while (!munit_atomic_cas(&munit_rand_state, &old, state));

  return munit_rand_from_state(old);
}

static void munit_rand_state_memory(munit_uint32_t *state, size_t size,
                                    munit_uint8_t *data) {
  size_t members_remaining = size / sizeof(munit_uint32_t);
  size_t bytes_remaining = size % sizeof(munit_uint32_t);
  munit_uint8_t *b = data;
  munit_uint32_t rv;
  while (members_remaining-- > 0) {
    rv = munit_rand_state_uint32(state);
    memcpy(b, &rv, sizeof(munit_uint32_t));
    b += sizeof(munit_uint32_t);
  }
  if (bytes_remaining != 0) {
    rv = munit_rand_state_uint32(state);
    memcpy(b, &rv, bytes_remaining);
  }
}

void munit_rand_memory(size_t size, munit_uint8_t *data) {
  munit_uint32_t old, state;

  do {
    state = old = munit_atomic_load(&munit_rand_state);
    munit_rand_state_memory(&state, size, data);
  } while (!munit_atomic_cas(&munit_rand_state, &old, state));
}

static munit_uint32_t munit_rand_state_at_most(munit_uint32_t *state,
                                               munit_uint32_t salt,
                                               munit_uint32_t max) {
  /* We want (UINT32_MAX + 1) % max, which in unsigned arithmetic is the same
   * as (UINT32_MAX + 1 - max) % max = -max % max. We compute -max using not
   * to avoid compiler warnings.
   */
  const munit_uint32_t min = (~max + 1U) % max;
  munit_uint32_t x;

  if (max == (~((munit_uint32_t)0U)))
    return munit_rand_state_uint32(state) ^ salt;

  max++;

  do {
    x = munit_rand_state_uint32(state) ^ salt;
  } while (x < min);

  return x % max;
}

static munit_uint32_t munit_rand_at_most(munit_uint32_t salt,
                                         munit_uint32_t max) {
  munit_uint32_t old, state;
  munit_uint32_t retval;

  do {
    state = old = munit_atomic_load(&munit_rand_state);
    retval = munit_rand_state_at_most(&state, salt, max);
  } while (!munit_atomic_cas(&munit_rand_state, &old, state));

  return retval;
}

int munit_rand_int_range(int min, int max) {
  munit_uint64_t range = (munit_uint64_t)max - (munit_uint64_t)min;

  if (min > max)
    return munit_rand_int_range(max, min);

  if (range > (~((munit_uint32_t)0U)))
    range = (~((munit_uint32_t)0U));

  return min + (int)munit_rand_at_most(0, (munit_uint32_t)range);
}

double munit_rand_double(void) {
  munit_uint32_t old, state;
  double retval = 0.0;

  do {
    state = old = munit_atomic_load(&munit_rand_state);

    /* See http://mumble.net/~campbell/tmp/random_real.c for how to do
     * this right.  Patches welcome if you feel that this is too
     * biased. */
    retval = munit_rand_state_uint32(&state) / ((~((munit_uint32_t)0U)) + 1.0);
  } while (!munit_atomic_cas(&munit_rand_state, &old, state));

  return retval;
}

/*** Test suite handling ***/

typedef struct {
  unsigned int successful;
  unsigned int skipped;
  unsigned int failed;
  unsigned int errored;
#if defined(MUNIT_ENABLE_TIMING)
  munit_uint64_t cpu_clock;
  munit_uint64_t wall_clock;
#endif
} MunitReport;

typedef struct {
  const char *prefix;
  const MunitSuite *suite;
  const char **tests;
  munit_uint32_t seed;
  unsigned int iterations;
  MunitParameter *parameters;
  munit_bool single_parameter_mode;
  void *user_data;
  MunitReport report;
  munit_bool colorize;
  munit_bool fork;
  munit_bool show_stderr;
  munit_bool fatal_failures;
} MunitTestRunner;

const char *munit_parameters_get(const MunitParameter params[],
                                 const char *key) {
  const MunitParameter *param;

  for (param = params; param != NULL && param->name != NULL; param++)
    if (strcmp(param->name, key) == 0)
      return param->value;
  return NULL;
}

#if defined(MUNIT_ENABLE_TIMING)
static void munit_print_time(FILE *fp, munit_uint64_t nanoseconds) {
  fprintf(fp, "%" MUNIT_TEST_TIME_FORMAT,
          ((double)nanoseconds) / ((double)PSNIP_CLOCK_NSEC_PER_SEC));
}
#endif

/* Add a paramter to an array of parameters. */
static MunitResult munit_parameters_add(size_t *params_size,
                                        MunitParameter **params, char *name,
                                        char *value) {
  *params = realloc(*params, sizeof(MunitParameter) * (*params_size + 2));
  if (*params == NULL)
    return MUNIT_ERROR;

  (*params)[*params_size].name = name;
  (*params)[*params_size].value = value;
  (*params_size)++;
  (*params)[*params_size].name = NULL;
  (*params)[*params_size].value = NULL;

  return MUNIT_OK;
}

/* Concatenate two strings, but just return one of the components
 * unaltered if the other is NULL or "". */
static char *munit_maybe_concat(size_t *len, char *prefix, char *suffix) {
  char *res;
  size_t res_l;
  const size_t prefix_l = prefix != NULL ? strlen(prefix) : 0;
  const size_t suffix_l = suffix != NULL ? strlen(suffix) : 0;
  if (prefix_l == 0 && suffix_l == 0) {
    res = NULL;
    res_l = 0;
  } else if (prefix_l == 0 && suffix_l != 0) {
    res = suffix;
    res_l = suffix_l;
  } else if (prefix_l != 0 && suffix_l == 0) {
    res = prefix;
    res_l = prefix_l;
  } else {
    res_l = prefix_l + suffix_l;
    res = malloc(res_l + 1);
    memcpy(res, prefix, prefix_l);
    memcpy(res + prefix_l, suffix, suffix_l);
    res[res_l] = 0;
  }

  if (len != NULL)
    *len = res_l;

  return res;
}

/* Possbily free a string returned by munit_maybe_concat. */
static void munit_maybe_free_concat(char *s, const char *prefix,
                                    const char *suffix) {
  if (prefix != s && suffix != s)
    free(s);
}

/* Cheap string hash function, just used to salt the PRNG. */
static munit_uint32_t munit_str_hash(const char *name) {
  const char *p;
  munit_uint32_t h = 5381U;

  for (p = name; *p != '\0'; p++)
    h = (munit_uint32_t)(h << 5) + h + (munit_uint32_t)*p;

  return h;
}

static void munit_splice(int from, int to) {
  munit_uint8_t buf[1024];
#if !defined(_WIN32)
  ssize_t len;
  ssize_t bytes_written;
  ssize_t write_res;
#else
  int len;
  int bytes_written;
  int write_res;
#endif
  do {
    len = read(from, buf, sizeof(buf));
    if (len > 0) {
      bytes_written = 0;
      do {
        write_res = write(to, buf + bytes_written,
#if !defined(_WIN32)
                          (size_t)
#else
                          (unsigned int)
#endif
                            (len - bytes_written));
        if (write_res < 0)
          break;
        bytes_written += write_res;
      } while (bytes_written < len);
    } else
      break;
  } while (1);
}

/* This is the part that should be handled in the child process */
static MunitResult munit_test_runner_exec(MunitTestRunner *runner,
                                          const MunitTest *test,
                                          const MunitParameter params[],
                                          MunitReport *report) {
  unsigned int iterations = runner->iterations;
  MunitResult result = MUNIT_FAIL;
#if defined(MUNIT_ENABLE_TIMING)
  struct PsnipClockTimespec wall_clock_begin =
                              {
                                0,
                              },
                            wall_clock_end = {
                              0,
                            };
  struct PsnipClockTimespec cpu_clock_begin =
                              {
                                0,
                              },
                            cpu_clock_end = {
                              0,
                            };
#endif
  unsigned int i = 0;

  if ((test->options & MUNIT_TEST_OPTION_SINGLE_ITERATION) ==
      MUNIT_TEST_OPTION_SINGLE_ITERATION)
    iterations = 1;
  else if (iterations == 0)
    iterations = runner->suite->iterations;

  munit_rand_seed(runner->seed);

  do {
    void *data = (test->setup == NULL) ? runner->user_data
                                       : test->setup(params, runner->user_data);

#if defined(MUNIT_ENABLE_TIMING)
    psnip_clock_get_time(PSNIP_CLOCK_TYPE_WALL, &wall_clock_begin);
    psnip_clock_get_time(PSNIP_CLOCK_TYPE_CPU, &cpu_clock_begin);
#endif

    result = test->test(params, data);

#if defined(MUNIT_ENABLE_TIMING)
    psnip_clock_get_time(PSNIP_CLOCK_TYPE_WALL, &wall_clock_end);
    psnip_clock_get_time(PSNIP_CLOCK_TYPE_CPU, &cpu_clock_end);
#endif

    if (test->tear_down != NULL)
      test->tear_down(data);

    if (MUNIT_LIKELY(result == MUNIT_OK)) {
      report->successful++;
#if defined(MUNIT_ENABLE_TIMING)
      report->wall_clock +=
        munit_clock_get_elapsed(&wall_clock_begin, &wall_clock_end);
      report->cpu_clock +=
        munit_clock_get_elapsed(&cpu_clock_begin, &cpu_clock_end);
#endif
    } else {
      switch ((int)result) {
      case MUNIT_SKIP:
        report->skipped++;
        break;
      case MUNIT_FAIL:
        report->failed++;
        break;
      case MUNIT_ERROR:
        report->errored++;
        break;
      default:
        break;
      }
      break;
    }
  } while (++i < iterations);

  return result;
}

#if defined(MUNIT_EMOTICON)
#  define MUNIT_RESULT_STRING_OK ":)"
#  define MUNIT_RESULT_STRING_SKIP ":|"
#  define MUNIT_RESULT_STRING_FAIL ":("
#  define MUNIT_RESULT_STRING_ERROR ":o"
#  define MUNIT_RESULT_STRING_TODO ":/"
#else
#  define MUNIT_RESULT_STRING_OK "OK   "
#  define MUNIT_RESULT_STRING_SKIP "SKIP "
#  define MUNIT_RESULT_STRING_FAIL "FAIL "
#  define MUNIT_RESULT_STRING_ERROR "ERROR"
#  define MUNIT_RESULT_STRING_TODO "TODO "
#endif

static void munit_test_runner_print_color(const MunitTestRunner *runner,
                                          const char *string, char color) {
  if (runner->colorize)
    fprintf(MUNIT_OUTPUT_FILE, "\x1b[3%cm%s\x1b[39m", color, string);
  else
    fputs(string, MUNIT_OUTPUT_FILE);
}

#if !defined(MUNIT_NO_BUFFER)
static int munit_replace_stderr(FILE *stderr_buf) {
  if (stderr_buf != NULL) {
    const int orig_stderr = dup(STDERR_FILENO);

    int errfd = fileno(stderr_buf);
    if (MUNIT_UNLIKELY(errfd == -1)) {
      exit(EXIT_FAILURE);
    }

    dup2(errfd, STDERR_FILENO);

    return orig_stderr;
  }

  return -1;
}

static void munit_restore_stderr(int orig_stderr) {
  if (orig_stderr != -1) {
    dup2(orig_stderr, STDERR_FILENO);
    close(orig_stderr);
  }
}
#endif /* !defined(MUNIT_NO_BUFFER) */

/* Run a test with the specified parameters. */
static void
munit_test_runner_run_test_with_params(MunitTestRunner *runner,
                                       const MunitTest *test,
                                       const MunitParameter params[]) {
  MunitResult result = MUNIT_OK;
  MunitReport report = {0, 0, 0, 0,
#if defined(MUNIT_ENABLE_TIMING)
                        0, 0
#endif
  };
  unsigned int output_l;
  munit_bool first;
  const MunitParameter *param;
  FILE *stderr_buf;
#if !defined(MUNIT_NO_FORK)
  int pipefd[2];
  pid_t fork_pid;
  ssize_t bytes_written = 0;
  ssize_t write_res;
  ssize_t bytes_read = 0;
  ssize_t read_res;
  int status = 0;
  pid_t changed_pid;
#endif

  if (params != NULL) {
    output_l = 2;
    fputs("  ", MUNIT_OUTPUT_FILE);
    first = 1;
    for (param = params; param != NULL && param->name != NULL; param++) {
      if (!first) {
        fputs(", ", MUNIT_OUTPUT_FILE);
        output_l += 2;
      } else {
        first = 0;
      }

      output_l += (unsigned int)fprintf(MUNIT_OUTPUT_FILE, "%s=%s", param->name,
                                        param->value);
    }
    while (output_l++ < MUNIT_TEST_NAME_LEN) {
      fputc(' ', MUNIT_OUTPUT_FILE);
    }
  }

  fflush(MUNIT_OUTPUT_FILE);

  stderr_buf = NULL;
#if !defined(_WIN32) || defined(__MINGW32__)
  stderr_buf = tmpfile();
#else
  tmpfile_s(&stderr_buf);
#endif
  if (stderr_buf == NULL) {
    munit_log_errno(MUNIT_LOG_ERROR, stderr,
                    "unable to create buffer for stderr");
    result = MUNIT_ERROR;
    goto print_result;
  }

#if !defined(MUNIT_NO_FORK)
  if (runner->fork) {
    pipefd[0] = -1;
    pipefd[1] = -1;
    if (pipe(pipefd) != 0) {
      munit_log_errno(MUNIT_LOG_ERROR, stderr, "unable to create pipe");
      result = MUNIT_ERROR;
      goto print_result;
    }

    fork_pid = fork();
    if (fork_pid == 0) {
      int orig_stderr;

      close(pipefd[0]);

      orig_stderr = munit_replace_stderr(stderr_buf);
      munit_test_runner_exec(runner, test, params, &report);

      /* Note that we don't restore stderr.  This is so we can buffer
       * things written to stderr later on (such as by
       * asan/tsan/ubsan, valgrind, etc.) */
      close(orig_stderr);

      do {
        write_res =
          write(pipefd[1], ((munit_uint8_t *)(&report)) + bytes_written,
                sizeof(report) - (size_t)bytes_written);
        if (write_res < 0) {
          if (stderr_buf != NULL) {
            munit_log_errno(MUNIT_LOG_ERROR, stderr, "unable to write to pipe");
          }
          exit(EXIT_FAILURE);
        }
        bytes_written += write_res;
      } while ((size_t)bytes_written < sizeof(report));

      if (stderr_buf != NULL)
        fclose(stderr_buf);
      close(pipefd[1]);

      exit(EXIT_SUCCESS);
    } else if (fork_pid == -1) {
      close(pipefd[0]);
      close(pipefd[1]);
      if (stderr_buf != NULL) {
        munit_log_errno(MUNIT_LOG_ERROR, stderr, "unable to fork");
      }
      report.errored++;
      result = MUNIT_ERROR;
    } else {
      close(pipefd[1]);
      do {
        read_res = read(pipefd[0], ((munit_uint8_t *)(&report)) + bytes_read,
                        sizeof(report) - (size_t)bytes_read);
        if (read_res < 1)
          break;
        bytes_read += read_res;
      } while (bytes_read < (ssize_t)sizeof(report));

      changed_pid = waitpid(fork_pid, &status, 0);

      if (MUNIT_LIKELY(changed_pid == fork_pid) &&
          MUNIT_LIKELY(WIFEXITED(status))) {
        if (bytes_read != sizeof(report)) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr_buf,
                              "child exited unexpectedly with status %d",
                              WEXITSTATUS(status));
          report.errored++;
        } else if (WEXITSTATUS(status) != EXIT_SUCCESS) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr_buf,
                              "child exited with status %d",
                              WEXITSTATUS(status));
          report.errored++;
        }
      } else {
        if (WIFSIGNALED(status)) {
#  if defined(_XOPEN_VERSION) && (_XOPEN_VERSION >= 700)
          munit_logf_internal(MUNIT_LOG_ERROR, stderr_buf,
                              "child killed by signal %d (%s)",
                              WTERMSIG(status), strsignal(WTERMSIG(status)));
#  else
          munit_logf_internal(MUNIT_LOG_ERROR, stderr_buf,
                              "child killed by signal %d", WTERMSIG(status));
#  endif
        } else if (WIFSTOPPED(status)) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr_buf,
                              "child stopped by signal %d", WSTOPSIG(status));
        }
        report.errored++;
      }

      close(pipefd[0]);
      waitpid(fork_pid, NULL, 0);
    }
  } else
#endif
  {
#if !defined(MUNIT_NO_BUFFER)
    const volatile int orig_stderr = munit_replace_stderr(stderr_buf);
#endif

#if defined(MUNIT_THREAD_LOCAL)
    if (MUNIT_UNLIKELY(setjmp(munit_error_jmp_buf) != 0)) {
      result = MUNIT_FAIL;
      report.failed++;
    } else {
      munit_error_jmp_buf_valid = 1;
      result = munit_test_runner_exec(runner, test, params, &report);
    }
#else
    result = munit_test_runner_exec(runner, test, params, &report);
#endif

#if !defined(MUNIT_NO_BUFFER)
    munit_restore_stderr(orig_stderr);
#endif

    /* Here just so that the label is used on Windows and we don't get
     * a warning */
    goto print_result;
  }

print_result:

  fputs("[ ", MUNIT_OUTPUT_FILE);
  if ((test->options & MUNIT_TEST_OPTION_TODO) == MUNIT_TEST_OPTION_TODO) {
    if (report.failed != 0 || report.errored != 0 || report.skipped != 0) {
      munit_test_runner_print_color(runner, MUNIT_RESULT_STRING_TODO, '3');
      result = MUNIT_OK;
    } else {
      munit_test_runner_print_color(runner, MUNIT_RESULT_STRING_ERROR, '1');
      if (MUNIT_LIKELY(stderr_buf != NULL))
        munit_log_internal(MUNIT_LOG_ERROR, stderr_buf,
                           "Test marked TODO, but was successful.");
      runner->report.failed++;
      result = MUNIT_ERROR;
    }
  } else if (report.failed > 0) {
    munit_test_runner_print_color(runner, MUNIT_RESULT_STRING_FAIL, '1');
    runner->report.failed++;
    result = MUNIT_FAIL;
  } else if (report.errored > 0) {
    munit_test_runner_print_color(runner, MUNIT_RESULT_STRING_ERROR, '1');
    runner->report.errored++;
    result = MUNIT_ERROR;
  } else if (report.skipped > 0) {
    munit_test_runner_print_color(runner, MUNIT_RESULT_STRING_SKIP, '3');
    runner->report.skipped++;
    result = MUNIT_SKIP;
  } else if (report.successful > 1) {
    munit_test_runner_print_color(runner, MUNIT_RESULT_STRING_OK, '2');
#if defined(MUNIT_ENABLE_TIMING)
    fputs(" ] [ ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.wall_clock / report.successful);
    fputs(" / ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.cpu_clock / report.successful);
    fprintf(MUNIT_OUTPUT_FILE,
            " CPU ]\n  %-" MUNIT_XSTRINGIFY(MUNIT_TEST_NAME_LEN) "s Total: [ ",
            "");
    munit_print_time(MUNIT_OUTPUT_FILE, report.wall_clock);
    fputs(" / ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.cpu_clock);
    fputs(" CPU", MUNIT_OUTPUT_FILE);
#endif
    runner->report.successful++;
    result = MUNIT_OK;
  } else if (report.successful > 0) {
    munit_test_runner_print_color(runner, MUNIT_RESULT_STRING_OK, '2');
#if defined(MUNIT_ENABLE_TIMING)
    fputs(" ] [ ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.wall_clock);
    fputs(" / ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.cpu_clock);
    fputs(" CPU", MUNIT_OUTPUT_FILE);
#endif
    runner->report.successful++;
    result = MUNIT_OK;
  }
  fputs(" ]\n", MUNIT_OUTPUT_FILE);

  if (stderr_buf != NULL) {
    if (result == MUNIT_FAIL || result == MUNIT_ERROR || runner->show_stderr) {
      fflush(MUNIT_OUTPUT_FILE);

      rewind(stderr_buf);
      munit_splice(fileno(stderr_buf), STDERR_FILENO);

      fflush(stderr);
    }

    fclose(stderr_buf);
  }
}

static void munit_test_runner_run_test_wild(MunitTestRunner *runner,
                                            const MunitTest *test,
                                            const char *test_name,
                                            MunitParameter *params,
                                            MunitParameter *p) {
  const MunitParameterEnum *pe;
  char **values;
  MunitParameter *next;

  for (pe = test->parameters; pe != NULL && pe->name != NULL; pe++) {
    if (p->name == pe->name)
      break;
  }

  if (pe == NULL)
    return;

  for (values = pe->values; *values != NULL; values++) {
    next = p + 1;
    p->value = *values;
    if (next->name == NULL) {
      munit_test_runner_run_test_with_params(runner, test, params);
    } else {
      munit_test_runner_run_test_wild(runner, test, test_name, params, next);
    }
    if (runner->fatal_failures &&
        (runner->report.failed != 0 || runner->report.errored != 0))
      break;
  }
}

/* Run a single test, with every combination of parameters
 * requested. */
static void munit_test_runner_run_test(MunitTestRunner *runner,
                                       const MunitTest *test,
                                       const char *prefix) {
  char *test_name =
    munit_maybe_concat(NULL, (char *)prefix, (char *)test->name);
  /* The array of parameters to pass to
   * munit_test_runner_run_test_with_params */
  MunitParameter *params = NULL;
  size_t params_l = 0;
  /* Wildcard parameters are parameters which have possible values
   * specified in the test, but no specific value was passed to the
   * CLI.  That means we want to run the test once for every
   * possible combination of parameter values or, if --single was
   * passed to the CLI, a single time with a random set of
   * parameters. */
  MunitParameter *wild_params = NULL;
  size_t wild_params_l = 0;
  const MunitParameterEnum *pe;
  const MunitParameter *cli_p;
  munit_bool filled;
  unsigned int possible;
  char **vals;
  size_t first_wild;
  const MunitParameter *wp;
  int pidx;

  munit_rand_seed(runner->seed);

  fprintf(MUNIT_OUTPUT_FILE, "%-" MUNIT_XSTRINGIFY(MUNIT_TEST_NAME_LEN) "s",
          test_name);

  if (test->parameters == NULL) {
    /* No parameters.  Simple, nice. */
    munit_test_runner_run_test_with_params(runner, test, NULL);
  } else {
    fputc('\n', MUNIT_OUTPUT_FILE);

    for (pe = test->parameters; pe != NULL && pe->name != NULL; pe++) {
      /* Did we received a value for this parameter from the CLI? */
      filled = 0;
      for (cli_p = runner->parameters; cli_p != NULL && cli_p->name != NULL;
           cli_p++) {
        if (strcmp(cli_p->name, pe->name) == 0) {
          if (MUNIT_UNLIKELY(munit_parameters_add(&params_l, &params, pe->name,
                                                  cli_p->value) != MUNIT_OK))
            goto cleanup;
          filled = 1;
          break;
        }
      }
      if (filled)
        continue;

      /* Nothing from CLI, is the enum NULL/empty?  We're not a
       * fuzzer… */
      if (pe->values == NULL || pe->values[0] == NULL)
        continue;

      /* If --single was passed to the CLI, choose a value from the
       * list of possibilities randomly. */
      if (runner->single_parameter_mode) {
        possible = 0;
        for (vals = pe->values; *vals != NULL; vals++)
          possible++;
        /* We want the tests to be reproducible, even if you're only
         * running a single test, but we don't want every test with
         * the same number of parameters to choose the same parameter
         * number, so use the test name as a primitive salt. */
        pidx = (int)munit_rand_at_most(munit_str_hash(test_name), possible - 1);
        if (MUNIT_UNLIKELY(munit_parameters_add(&params_l, &params, pe->name,
                                                pe->values[pidx]) != MUNIT_OK))
          goto cleanup;
      } else {
        /* We want to try every permutation.  Put in a placeholder
         * entry, we'll iterate through them later. */
        if (MUNIT_UNLIKELY(munit_parameters_add(&wild_params_l, &wild_params,
                                                pe->name, NULL) != MUNIT_OK))
          goto cleanup;
      }
    }

    if (wild_params_l != 0) {
      first_wild = params_l;
      for (wp = wild_params; wp != NULL && wp->name != NULL; wp++) {
        for (pe = test->parameters;
             pe != NULL && pe->name != NULL && pe->values != NULL; pe++) {
          if (strcmp(wp->name, pe->name) == 0) {
            if (MUNIT_UNLIKELY(munit_parameters_add(&params_l, &params,
                                                    pe->name,
                                                    pe->values[0]) != MUNIT_OK))
              goto cleanup;
          }
        }
      }

      munit_test_runner_run_test_wild(runner, test, test_name, params,
                                      params + first_wild);
    } else {
      munit_test_runner_run_test_with_params(runner, test, params);
    }

  cleanup:
    free(params);
    free(wild_params);
  }

  munit_maybe_free_concat(test_name, prefix, test->name);
}

/* Recurse through the suite and run all the tests.  If a list of
 * tests to run was provied on the command line, run only those
 * tests.  */
static void munit_test_runner_run_suite(MunitTestRunner *runner,
                                        const MunitSuite *suite,
                                        const char *prefix) {
  size_t pre_l;
  char *pre = munit_maybe_concat(&pre_l, (char *)prefix, (char *)suite->prefix);
  const MunitTest *test;
  const char **test_name;
  const MunitSuite *child_suite;

  /* Run the tests. */
  for (test = suite->tests; test != NULL && test->test != NULL; test++) {
    if (runner->tests != NULL) { /* Specific tests were requested on the CLI */
      for (test_name = runner->tests; test_name != NULL && *test_name != NULL;
           test_name++) {
        if ((pre_l == 0 || strncmp(pre, *test_name, pre_l) == 0) &&
            strncmp(test->name, *test_name + pre_l,
                    strlen(*test_name + pre_l)) == 0) {
          munit_test_runner_run_test(runner, test, pre);
          if (runner->fatal_failures &&
              (runner->report.failed != 0 || runner->report.errored != 0))
            goto cleanup;
        }
      }
    } else { /* Run all tests */
      munit_test_runner_run_test(runner, test, pre);
    }
  }

  if (runner->fatal_failures &&
      (runner->report.failed != 0 || runner->report.errored != 0))
    goto cleanup;

  /* Run any child suites. */
  for (child_suite = suite->suites;
       child_suite != NULL && child_suite->prefix != NULL; child_suite++) {
    munit_test_runner_run_suite(runner, child_suite, pre);
  }

cleanup:

  munit_maybe_free_concat(pre, prefix, suite->prefix);
}

static void munit_test_runner_run(MunitTestRunner *runner) {
  munit_test_runner_run_suite(runner, runner->suite, NULL);
}

static void munit_print_help(int argc, char *const *argv, void *user_data,
                             const MunitArgument arguments[]) {
  const MunitArgument *arg;
  (void)argc;

  printf("USAGE: %s [OPTIONS...] [TEST...]\n\n", argv[0]);
  puts(
    " --seed SEED\n"
    "           Value used to seed the PRNG.  Must be a 32-bit integer in "
    "decimal\n"
    "           notation with no separators (commas, decimals, spaces, "
    "etc.), or\n"
    "           hexidecimal prefixed by \"0x\".\n"
    " --iterations N\n"
    "           Run each test N times.  0 means the default number.\n"
    " --param name value\n"
    "           A parameter key/value pair which will be passed to any test "
    "with\n"
    "           takes a parameter of that name.  If not provided, the test "
    "will be\n"
    "           run once for each possible parameter value.\n"
    " --list    Write a list of all available tests.\n"
    " --list-params\n"
    "           Write a list of all available tests and their possible "
    "parameters.\n"
    " --single  Run each parameterized test in a single configuration "
    "instead of\n"
    "           every possible combination\n"
    " --log-visible debug|info|warning|error\n"
    " --log-fatal debug|info|warning|error\n"
    "           Set the level at which messages of different severities are "
    "visible,\n"
    "           or cause the test to terminate.\n"
#if !defined(MUNIT_NO_FORK)
    " --no-fork Do not execute tests in a child process.  If this option is "
    "supplied\n"
    "           and a test crashes (including by failing an assertion), no "
    "further\n"
    "           tests will be performed.\n"
#endif
    " --fatal-failures\n"
    "           Stop executing tests as soon as a failure is found.\n"
    " --show-stderr\n"
    "           Show data written to stderr by the tests, even if the test "
    "succeeds.\n"
    " --color auto|always|never\n"
    "           Colorize (or don't) the output.\n"
    /* 12345678901234567890123456789012345678901234567890123456789012345678901234567890
     */
    " --help    Print this help message and exit.\n");
#if defined(MUNIT_NL_LANGINFO)
  setlocale(LC_ALL, "");
  fputs((strcasecmp("UTF-8", nl_langinfo(CODESET)) == 0) ? "µnit" : "munit",
        stdout);
#else
  puts("munit");
#endif
  printf(" %d.%d.%d\n"
         "Full documentation at: https://nemequ.github.io/munit/\n",
         (MUNIT_CURRENT_VERSION >> 16) & 0xff,
         (MUNIT_CURRENT_VERSION >> 8) & 0xff,
         (MUNIT_CURRENT_VERSION >> 0) & 0xff);
  for (arg = arguments; arg != NULL && arg->name != NULL; arg++)
    arg->write_help(arg, user_data);
}

static const MunitArgument *
munit_arguments_find(const MunitArgument arguments[], const char *name) {
  const MunitArgument *arg;

  for (arg = arguments; arg != NULL && arg->name != NULL; arg++)
    if (strcmp(arg->name, name) == 0)
      return arg;

  return NULL;
}

static void munit_suite_list_tests(const MunitSuite *suite,
                                   munit_bool show_params, const char *prefix) {
  size_t pre_l;
  char *pre = munit_maybe_concat(&pre_l, (char *)prefix, (char *)suite->prefix);
  const MunitTest *test;
  const MunitParameterEnum *params;
  munit_bool first;
  char **val;
  const MunitSuite *child_suite;

  for (test = suite->tests; test != NULL && test->name != NULL; test++) {
    if (pre != NULL)
      fputs(pre, stdout);
    puts(test->name);

    if (show_params) {
      for (params = test->parameters; params != NULL && params->name != NULL;
           params++) {
        fprintf(stdout, " - %s: ", params->name);
        if (params->values == NULL) {
          puts("Any");
        } else {
          first = 1;
          for (val = params->values; *val != NULL; val++) {
            if (!first) {
              fputs(", ", stdout);
            } else {
              first = 0;
            }
            fputs(*val, stdout);
          }
          putc('\n', stdout);
        }
      }
    }
  }

  for (child_suite = suite->suites;
       child_suite != NULL && child_suite->prefix != NULL; child_suite++) {
    munit_suite_list_tests(child_suite, show_params, pre);
  }

  munit_maybe_free_concat(pre, prefix, suite->prefix);
}

static munit_bool munit_stream_supports_ansi(FILE *stream) {
#if !defined(_WIN32)
  return isatty(fileno(stream));
#else

#  if !defined(__MINGW32__)
  size_t ansicon_size = 0;
#  endif

  if (isatty(fileno(stream))) {
#  if !defined(__MINGW32__)
    getenv_s(&ansicon_size, NULL, 0, "ANSICON");
    return ansicon_size != 0;
#  else
    return getenv("ANSICON") != NULL;
#  endif
  }
  return 0;
#endif
}

int munit_suite_main_custom(const MunitSuite *suite, void *user_data, int argc,
                            char *const *argv,
                            const MunitArgument arguments[]) {
  int result = EXIT_FAILURE;
  MunitTestRunner runner;
  size_t parameters_size = 0;
  size_t tests_size = 0;
  int arg;

  char *envptr;
  unsigned long ts;
  char *endptr;
  unsigned long long iterations;
  MunitLogLevel level;
  const MunitArgument *argument;
  const char **runner_tests;
  unsigned int tests_run;
  unsigned int tests_total;

  runner.prefix = NULL;
  runner.suite = NULL;
  runner.tests = NULL;
  runner.seed = 0;
  runner.iterations = 0;
  runner.parameters = NULL;
  runner.single_parameter_mode = 0;
  runner.user_data = NULL;

  runner.report.successful = 0;
  runner.report.skipped = 0;
  runner.report.failed = 0;
  runner.report.errored = 0;
#if defined(MUNIT_ENABLE_TIMING)
  runner.report.cpu_clock = 0;
  runner.report.wall_clock = 0;
#endif

  runner.colorize = 0;
#if !defined(_WIN32)
  runner.fork = 1;
#else
  runner.fork = 0;
#endif
  runner.show_stderr = 0;
  runner.fatal_failures = 0;
  runner.suite = suite;
  runner.user_data = user_data;
  runner.seed = munit_rand_generate_seed();
  runner.colorize = munit_stream_supports_ansi(MUNIT_OUTPUT_FILE);

  for (arg = 1; arg < argc; arg++) {
    if (strncmp("--", argv[arg], 2) == 0) {
      if (strcmp("seed", argv[arg] + 2) == 0) {
        if (arg + 1 >= argc) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "%s requires an argument", argv[arg]);
          goto cleanup;
        }

        envptr = argv[arg + 1];
        ts = strtoul(argv[arg + 1], &envptr, 0);
        if (*envptr != '\0' || ts > (~((munit_uint32_t)0U))) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "invalid value ('%s') passed to %s",
                              argv[arg + 1], argv[arg]);
          goto cleanup;
        }
        runner.seed = (munit_uint32_t)ts;

        arg++;
      } else if (strcmp("iterations", argv[arg] + 2) == 0) {
        if (arg + 1 >= argc) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "%s requires an argument", argv[arg]);
          goto cleanup;
        }

        endptr = argv[arg + 1];
        iterations = strtoul(argv[arg + 1], &endptr, 0);
        if (*endptr != '\0' || iterations > UINT_MAX) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "invalid value ('%s') passed to %s",
                              argv[arg + 1], argv[arg]);
          goto cleanup;
        }

        runner.iterations = (unsigned int)iterations;

        arg++;
      } else if (strcmp("param", argv[arg] + 2) == 0) {
        if (arg + 2 >= argc) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "%s requires two arguments", argv[arg]);
          goto cleanup;
        }

        runner.parameters = realloc(runner.parameters, sizeof(MunitParameter) *
                                                         (parameters_size + 2));
        if (runner.parameters == NULL) {
          munit_log_internal(MUNIT_LOG_ERROR, stderr,
                             "failed to allocate memory");
          goto cleanup;
        }
        runner.parameters[parameters_size].name = (char *)argv[arg + 1];
        runner.parameters[parameters_size].value = (char *)argv[arg + 2];
        parameters_size++;
        runner.parameters[parameters_size].name = NULL;
        runner.parameters[parameters_size].value = NULL;
        arg += 2;
      } else if (strcmp("color", argv[arg] + 2) == 0) {
        if (arg + 1 >= argc) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "%s requires an argument", argv[arg]);
          goto cleanup;
        }

        if (strcmp(argv[arg + 1], "always") == 0)
          runner.colorize = 1;
        else if (strcmp(argv[arg + 1], "never") == 0)
          runner.colorize = 0;
        else if (strcmp(argv[arg + 1], "auto") == 0)
          runner.colorize = munit_stream_supports_ansi(MUNIT_OUTPUT_FILE);
        else {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "invalid value ('%s') passed to %s",
                              argv[arg + 1], argv[arg]);
          goto cleanup;
        }

        arg++;
      } else if (strcmp("help", argv[arg] + 2) == 0) {
        munit_print_help(argc, argv, user_data, arguments);
        result = EXIT_SUCCESS;
        goto cleanup;
      } else if (strcmp("single", argv[arg] + 2) == 0) {
        runner.single_parameter_mode = 1;
      } else if (strcmp("show-stderr", argv[arg] + 2) == 0) {
        runner.show_stderr = 1;
#if !defined(_WIN32)
      } else if (strcmp("no-fork", argv[arg] + 2) == 0) {
        runner.fork = 0;
#endif
      } else if (strcmp("fatal-failures", argv[arg] + 2) == 0) {
        runner.fatal_failures = 1;
      } else if (strcmp("log-visible", argv[arg] + 2) == 0 ||
                 strcmp("log-fatal", argv[arg] + 2) == 0) {
        if (arg + 1 >= argc) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "%s requires an argument", argv[arg]);
          goto cleanup;
        }

        if (strcmp(argv[arg + 1], "debug") == 0)
          level = MUNIT_LOG_DEBUG;
        else if (strcmp(argv[arg + 1], "info") == 0)
          level = MUNIT_LOG_INFO;
        else if (strcmp(argv[arg + 1], "warning") == 0)
          level = MUNIT_LOG_WARNING;
        else if (strcmp(argv[arg + 1], "error") == 0)
          level = MUNIT_LOG_ERROR;
        else {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "invalid value ('%s') passed to %s",
                              argv[arg + 1], argv[arg]);
          goto cleanup;
        }

        if (strcmp("log-visible", argv[arg] + 2) == 0)
          munit_log_level_visible = level;
        else
          munit_log_level_fatal = level;

        arg++;
      } else if (strcmp("list", argv[arg] + 2) == 0) {
        munit_suite_list_tests(suite, 0, NULL);
        result = EXIT_SUCCESS;
        goto cleanup;
      } else if (strcmp("list-params", argv[arg] + 2) == 0) {
        munit_suite_list_tests(suite, 1, NULL);
        result = EXIT_SUCCESS;
        goto cleanup;
      } else {
        argument = munit_arguments_find(arguments, argv[arg] + 2);
        if (argument == NULL) {
          munit_logf_internal(MUNIT_LOG_ERROR, stderr,
                              "unknown argument ('%s')", argv[arg]);
          goto cleanup;
        }

        if (!argument->parse_argument(suite, user_data, &arg, argc, argv))
          goto cleanup;
      }
    } else {
      runner_tests =
        realloc((void *)runner.tests, sizeof(char *) * (tests_size + 2));
      if (runner_tests == NULL) {
        munit_log_internal(MUNIT_LOG_ERROR, stderr,
                           "failed to allocate memory");
        goto cleanup;
      }
      runner.tests = runner_tests;
      runner.tests[tests_size++] = argv[arg];
      runner.tests[tests_size] = NULL;
    }
  }

  fflush(stderr);
  fprintf(MUNIT_OUTPUT_FILE,
          "Running test suite with seed 0x%08" PRIx32 "...\n", runner.seed);

  munit_test_runner_run(&runner);

  tests_run =
    runner.report.successful + runner.report.failed + runner.report.errored;
  tests_total = tests_run + runner.report.skipped;
  if (tests_run == 0) {
    fprintf(stderr, "No tests run, %d (100%%) skipped.\n",
            runner.report.skipped);
  } else {
    fprintf(MUNIT_OUTPUT_FILE,
            "%d of %d (%0.0f%%) tests successful, %d (%0.0f%%) test skipped.\n",
            runner.report.successful, tests_run,
            (((double)runner.report.successful) / ((double)tests_run)) * 100.0,
            runner.report.skipped,
            (((double)runner.report.skipped) / ((double)tests_total)) * 100.0);
  }

  if (runner.report.failed == 0 && runner.report.errored == 0) {
    result = EXIT_SUCCESS;
  }

cleanup:
  free(runner.parameters);
  free((void *)runner.tests);

  return result;
}

int munit_suite_main(const MunitSuite *suite, void *user_data, int argc,
                     char *const *argv) {
  return munit_suite_main_custom(suite, user_data, argc, argv, NULL);
}

static uint8_t hexchars[] = "0123456789abcdef";

static uint8_t *hexdump_addr(uint8_t *dest, size_t addr) {
  size_t i;
  uint8_t a;

  for (i = 0; i < 4; ++i) {
    a = (addr >> (3 - i) * 8) & 0xff;

    *dest++ = hexchars[a >> 4];
    *dest++ = hexchars[a & 0xf];
  }

  return dest;
}

static uint8_t *asciidump(uint8_t *dest, const uint8_t *data, size_t datalen) {
  size_t i;

  *dest++ = '|';

  for (i = 0; i < datalen; ++i) {
    if (0x20 <= data[i] && data[i] <= 0x7e) {
      *dest++ = data[i];
    } else {
      *dest++ = '.';
    }
  }

  *dest++ = '|';

  return dest;
}

static uint8_t *hexdump8(uint8_t *dest, const uint8_t *data, size_t datalen) {
  size_t i;

  for (i = 0; i < datalen; ++i) {
    *dest++ = hexchars[data[i] >> 4];
    *dest++ = hexchars[data[i] & 0xf];
    *dest++ = ' ';
  }

  for (; i < 8; ++i) {
    *dest++ = ' ';
    *dest++ = ' ';
    *dest++ = ' ';
  }

  return dest;
}

static uint8_t *hexdump16(uint8_t *dest, const uint8_t *data, size_t datalen) {
  dest = hexdump8(dest, data, datalen < 8 ? datalen : 8);
  *dest++ = ' ';

  if (datalen < 8) {
    data = NULL;
    datalen = 0;
  } else {
    data += 8;
    datalen -= 8;
  }

  dest = hexdump8(dest, data, datalen);
  *dest++ = ' ';

  return dest;
}

static uint8_t *hexdump_line(uint8_t *dest, const uint8_t *data, size_t datalen,
                             size_t addr) {
  dest = hexdump_addr(dest, addr);
  *dest++ = ' ';
  *dest++ = ' ';

  dest = hexdump16(dest, data, datalen);

  dest = asciidump(dest, data, datalen);

  return dest;
}

int munit_hexdump(FILE *fp, const void *data, size_t datalen) {
  size_t offset = 0, n, len;
  uint8_t buf[128], *p;
  const uint8_t *s;
  int repeated = 0;

  if (datalen == 0) {
    return 0;
  }

  for (; offset < datalen; offset += 16) {
    n = datalen - offset;
    s = (const uint8_t *)data + offset;

    if (n >= 16) {
      n = 16;

      if (offset > 0) {
        if (memcmp(s - 16, s, 16) == 0) {
          if (repeated) {
            continue;
          }

          repeated = 1;

          if (fwrite("*\n", 1, 2, fp) < 2) {
            return -1;
          }

          continue;
        }

        repeated = 0;
      }
    }

    p = hexdump_line(buf, s, n, offset);
    *p++ = '\n';

    len = (size_t)(p - buf);

    if (fwrite(buf, 1, len, fp) < len) {
      return -1;
    }
  }

  p = hexdump_addr(buf, datalen);
  *p++ = '\n';

  len = (size_t)(p - buf);

  if (fwrite(buf, 1, len, fp) < len) {
    return -1;
  }

  return 0;
}

int munit_hexdump_diff(FILE *fp, const void *a, size_t alen, const void *b,
                       size_t blen) {
  size_t offset = 0, k, i, len, ncomp, maxlen, adoff = 0;
  uint8_t buf[128], *p;
  const uint8_t mk[2] = {'-', '+'};
  struct datasource {
    const uint8_t *data;
    size_t datalen;
    const uint8_t *s;
    size_t n;
  } ds[] = {{a, alen, NULL, 0}, {b, blen, NULL, 0}}, *dp;

  maxlen = alen < blen ? blen : alen;

  for (; offset < maxlen; offset += 16) {
    for (k = 0; k < 2; ++k) {
      dp = &ds[k];

      if (offset < dp->datalen) {
        dp->s = (const uint8_t *)dp->data + offset;
        dp->n = dp->datalen - offset;

        if (dp->n > 16) {
          dp->n = 16;
        }
      } else {
        dp->s = NULL;
        dp->n = 0;
      }
    }

    if (ds[0].n == ds[1].n && memcmp(ds[0].s, ds[1].s, ds[0].n) == 0) {
      continue;
    }

    for (k = 0; k < 2; ++k) {
      dp = &ds[k];

      if (!dp->n) {
        continue;
      }

      p = buf;
      *p++ = mk[k];
      *p++ = mk[k];
      *p++ = mk[k];
      *p++ = mk[k];

      p = hexdump_line(p, dp->s, dp->n, offset);
      *p++ = '\n';

      len = (size_t)(p - buf);

      if (fwrite(buf, 1, len, fp) < len) {
        return -1;
      }
    }

    if (!ds[0].n || !ds[1].n) {
      continue;
    }

    ncomp = ds[0].n < ds[1].n ? ds[0].n : ds[1].n;

    p = buf + 4 + 10;

    memset(buf, ' ', 4 + 78);

    for (i = 0; i < ncomp; ++i) {
      if (ds[0].s[i] == ds[1].s[i]) {
        *p++ = ' ';
        *p++ = ' ';
      } else {
        adoff = 4 + 10 + 51 + i;
        *(buf + adoff) = '^';

        *p++ = '^';
        *p++ = '^';
      }

      *p++ = ' ';

      if (i == 7) {
        *p++ = ' ';
      }
    }

    if (adoff) {
      len = adoff + 1;
    } else {
      len = (size_t)(p - buf);
    }

    buf[len++] = '\n';

    if (fwrite(buf, 1, len, fp) < len) {
      return -1;
    }
  }

  return 0;
}
