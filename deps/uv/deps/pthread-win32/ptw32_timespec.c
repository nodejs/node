/*
 * ptw32_timespec.c
 *
 * Description:
 * This translation unit implements routines which are private to
 * the implementation and may be used throughout it.
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


#if defined(NEED_FTIME)

/*
 * time between jan 1, 1601 and jan 1, 1970 in units of 100 nanoseconds
 */
#define PTW32_TIMESPEC_TO_FILETIME_OFFSET \
	  ( ((int64_t) 27111902 << 32) + (int64_t) 3577643008 )

INLINE void
ptw32_timespec_to_filetime (const struct timespec *ts, FILETIME * ft)
     /*
      * -------------------------------------------------------------------
      * converts struct timespec
      * where the time is expressed in seconds and nanoseconds from Jan 1, 1970.
      * into FILETIME (as set by GetSystemTimeAsFileTime), where the time is
      * expressed in 100 nanoseconds from Jan 1, 1601,
      * -------------------------------------------------------------------
      */
{
  *(int64_t *) ft = ts->tv_sec * 10000000
    + (ts->tv_nsec + 50) / 100 + PTW32_TIMESPEC_TO_FILETIME_OFFSET;
}

INLINE void
ptw32_filetime_to_timespec (const FILETIME * ft, struct timespec *ts)
     /*
      * -------------------------------------------------------------------
      * converts FILETIME (as set by GetSystemTimeAsFileTime), where the time is
      * expressed in 100 nanoseconds from Jan 1, 1601,
      * into struct timespec
      * where the time is expressed in seconds and nanoseconds from Jan 1, 1970.
      * -------------------------------------------------------------------
      */
{
  ts->tv_sec =
    (int) ((*(int64_t *) ft - PTW32_TIMESPEC_TO_FILETIME_OFFSET) / 10000000);
  ts->tv_nsec =
    (int) ((*(int64_t *) ft - PTW32_TIMESPEC_TO_FILETIME_OFFSET -
	    ((int64_t) ts->tv_sec * (int64_t) 10000000)) * 100);
}

#endif /* NEED_FTIME */
