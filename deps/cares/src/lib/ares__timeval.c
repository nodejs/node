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

#if defined(WIN32) && !defined(MSDOS)

struct timeval ares__tvnow(void)
{
  /*
  ** GetTickCount() is available on _all_ Windows versions from W95 up
  ** to nowadays. Returns milliseconds elapsed since last system boot,
  ** increases monotonically and wraps once 49.7 days have elapsed.
  */
  struct timeval now;
  DWORD milliseconds = GetTickCount();
  now.tv_sec = milliseconds / 1000;
  now.tv_usec = (milliseconds % 1000) * 1000;
  return now;
}

#elif defined(HAVE_CLOCK_GETTIME_MONOTONIC)

struct timeval ares__tvnow(void)
{
  /*
  ** clock_gettime() is granted to be increased monotonically when the
  ** monotonic clock is queried. Time starting point is unspecified, it
  ** could be the system start-up time, the Epoch, or something else,
  ** in any case the time starting point does not change once that the
  ** system has started up.
  */
  struct timeval now;
  struct timespec tsnow;
  if(0 == clock_gettime(CLOCK_MONOTONIC, &tsnow)) {
    now.tv_sec = tsnow.tv_sec;
    now.tv_usec = tsnow.tv_nsec / 1000;
  }
  /*
  ** Even when the configure process has truly detected monotonic clock
  ** availability, it might happen that it is not actually available at
  ** run-time. When this occurs simply fallback to other time source.
  */
#ifdef HAVE_GETTIMEOFDAY
  else
    (void)gettimeofday(&now, NULL);  /* LCOV_EXCL_LINE */
#else
  else {
    now.tv_sec = (long)time(NULL);
    now.tv_usec = 0;
  }
#endif
  return now;
}

#elif defined(HAVE_GETTIMEOFDAY)

struct timeval ares__tvnow(void)
{
  /*
  ** gettimeofday() is not granted to be increased monotonically, due to
  ** clock drifting and external source time synchronization it can jump
  ** forward or backward in time.
  */
  struct timeval now;
  (void)gettimeofday(&now, NULL);
  return now;
}

#else

struct timeval ares__tvnow(void)
{
  /*
  ** time() returns the value of time in seconds since the Epoch.
  */
  struct timeval now;
  now.tv_sec = (long)time(NULL);
  now.tv_usec = 0;
  return now;
}

#endif

#if 0 /* Not used */
/*
 * Make sure that the first argument is the more recent time, as otherwise
 * we'll get a weird negative time-diff back...
 *
 * Returns: the time difference in number of milliseconds.
 */
long ares__tvdiff(struct timeval newer, struct timeval older)
{
  return (newer.tv_sec-older.tv_sec)*1000+
    (newer.tv_usec-older.tv_usec)/1000;
}
#endif

