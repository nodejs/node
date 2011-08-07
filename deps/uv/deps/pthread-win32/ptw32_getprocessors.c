/*
 * ptw32_getprocessors.c
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


/*
 * ptw32_getprocessors()
 *
 * Get the number of CPUs available to the process.
 *
 * If the available number of CPUs is 1 then pthread_spin_lock()
 * will block rather than spin if the lock is already owned.
 *
 * pthread_spin_init() calls this routine when initialising
 * a spinlock. If the number of available processors changes
 * (after a call to SetProcessAffinityMask()) then only
 * newly initialised spinlocks will notice.
 */
int
ptw32_getprocessors (int *count)
{
  DWORD_PTR vProcessCPUs;
  DWORD_PTR vSystemCPUs;
  int result = 0;

#if defined(NEED_PROCESS_AFFINITY_MASK)

  *count = 1;

#else

  if (GetProcessAffinityMask (GetCurrentProcess (),
			      &vProcessCPUs, &vSystemCPUs))
    {
      DWORD_PTR bit;
      int CPUs = 0;

      for (bit = 1; bit != 0; bit <<= 1)
	{
	  if (vProcessCPUs & bit)
	    {
	      CPUs++;
	    }
	}
      *count = CPUs;
    }
  else
    {
      result = EAGAIN;
    }

#endif

  return (result);
}
