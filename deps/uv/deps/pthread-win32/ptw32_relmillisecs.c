/*
 * ptw32_relmillisecs.c
 *
 * Description:
 * This translation unit implements miscellaneous thread functions.
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads-win32 - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999,2005 Pthreads-win32 contributors
 * 
 *      Contact Email: rpj@callisto.canberra.edu.au
 * 
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      http://sources.redhat.com/pthreads-win32/contributors.html
 * 
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 * 
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 * 
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "pthread.h"
#include "implement.h"
#if !defined(NEED_FTIME)
#include <sys/timeb.h>
#endif


#if defined(PTW32_BUILD_INLINED)
INLINE 
#endif /* PTW32_BUILD_INLINED */
DWORD
ptw32_relmillisecs (const struct timespec * abstime)
{
  const int64_t NANOSEC_PER_MILLISEC = 1000000;
  const int64_t MILLISEC_PER_SEC = 1000;
  DWORD milliseconds;
  int64_t tmpAbsMilliseconds;
  int64_t tmpCurrMilliseconds;
#if defined(NEED_FTIME)
  struct timespec currSysTime;
  FILETIME ft;
  SYSTEMTIME st;
#else /* ! NEED_FTIME */
#if ( defined(_MSC_VER) && _MSC_VER >= 1300 ) || \
    ( (defined(__MINGW64__) || defined(__MINGW32__)) && __MSVCRT_VERSION__ >= 0x0601 )
  struct __timeb64 currSysTime;
#else
  struct _timeb currSysTime;
#endif
#endif /* NEED_FTIME */


  /* 
   * Calculate timeout as milliseconds from current system time. 
   */

  /*
   * subtract current system time from abstime in a way that checks
   * that abstime is never in the past, or is never equivalent to the
   * defined INFINITE value (0xFFFFFFFF).
   *
   * Assume all integers are unsigned, i.e. cannot test if less than 0.
   */
  tmpAbsMilliseconds =  (int64_t)abstime->tv_sec * MILLISEC_PER_SEC;
  tmpAbsMilliseconds += ((int64_t)abstime->tv_nsec + (NANOSEC_PER_MILLISEC/2)) / NANOSEC_PER_MILLISEC;

  /* get current system time */

#if defined(NEED_FTIME)

  GetSystemTime(&st);
  SystemTimeToFileTime(&st, &ft);
  /*
   * GetSystemTimeAsFileTime(&ft); would be faster,
   * but it does not exist on WinCE
   */

  ptw32_filetime_to_timespec(&ft, &currSysTime);

  tmpCurrMilliseconds = (int64_t)currSysTime.tv_sec * MILLISEC_PER_SEC;
  tmpCurrMilliseconds += ((int64_t)currSysTime.tv_nsec + (NANOSEC_PER_MILLISEC/2))
			   / NANOSEC_PER_MILLISEC;

#else /* ! NEED_FTIME */

#if defined(_MSC_VER) && _MSC_VER >= 1400
  _ftime64_s(&currSysTime);
#elif ( defined(_MSC_VER) && _MSC_VER >= 1300 ) || \
      ( (defined(__MINGW64__) || defined(__MINGW32__)) && __MSVCRT_VERSION__ >= 0x0601 )
  _ftime64(&currSysTime);
#else
  _ftime(&currSysTime);
#endif

  tmpCurrMilliseconds = (int64_t) currSysTime.time * MILLISEC_PER_SEC;
  tmpCurrMilliseconds += (int64_t) currSysTime.millitm;

#endif /* NEED_FTIME */

  if (tmpAbsMilliseconds > tmpCurrMilliseconds)
    {
      milliseconds = (DWORD) (tmpAbsMilliseconds - tmpCurrMilliseconds);
      if (milliseconds == INFINITE)
        {
          /* Timeouts must be finite */
          milliseconds--;
        }
    }
  else
    {
      /* The abstime given is in the past */
      milliseconds = 0;
    }

  return milliseconds;
}
