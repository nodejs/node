/* MIT License
 *
 * Copyright (c) 2008 Daniel Stenberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ares_setup.h"
#include "ares.h"
#include "ares_private.h"

#if defined(_WIN32) && !defined(MSDOS)

ares_timeval_t ares__tvnow(void)
{
  /* GetTickCount64() is available on Windows Vista and higher */
  ares_timeval_t now;
  ULONGLONG      milliseconds = GetTickCount64();

  now.sec  = (ares_int64_t)milliseconds / 1000;
  now.usec = (unsigned int)(milliseconds % 1000) * 1000;
  return now;
}

#elif defined(HAVE_CLOCK_GETTIME_MONOTONIC)

ares_timeval_t ares__tvnow(void)
{
  /* clock_gettime() is guaranteed to be increased monotonically when the
   * monotonic clock is queried. Time starting point is unspecified, it
   * could be the system start-up time, the Epoch, or something else,
   * in any case the time starting point does not change once that the
   * system has started up. */
  ares_timeval_t  now;
  struct timespec tsnow;

  if (clock_gettime(CLOCK_MONOTONIC, &tsnow) == 0) {
    now.sec  = (ares_int64_t)tsnow.tv_sec;
    now.usec = (unsigned int)(tsnow.tv_nsec / 1000);
  } else {
    /* LCOV_EXCL_START: FallbackCode */
    struct timeval tv;
    (void)gettimeofday(&tv, NULL);
    now.sec  = (ares_int64_t)tv.tv_sec;
    now.usec = (unsigned int)tv.tv_usec;
    /* LCOV_EXCL_STOP */
  }
  return now;
}

#elif defined(HAVE_GETTIMEOFDAY)

ares_timeval_t ares__tvnow(void)
{
  /* gettimeofday() is not granted to be increased monotonically, due to
   * clock drifting and external source time synchronization it can jump
   * forward or backward in time. */
  ares_timeval_t now;
  struct timeval tv;

  (void)gettimeofday(&tv, NULL);
  now.sec  = (ares_int64_t)tv.tv_sec;
  now.usec = (unsigned int)tv.tv_usec;

  return now;
}

#else

#  error missing sub-second time retrieval function

#endif
