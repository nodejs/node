/*
 * sched_getscheduler.c
 * 
 * Description:
 * POSIX thread functions that deal with thread scheduling.
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
#include "sched.h"

int
sched_getscheduler (pid_t pid)
{
  /*
   * Win32 only has one policy which we call SCHED_OTHER.
   * However, we try to provide other valid side-effects
   * such as EPERM and ESRCH errors.
   */
  if (0 != pid)
    {
      int selfPid = (int) GetCurrentProcessId ();

      if (pid != selfPid)
	{
	  HANDLE h =
	    OpenProcess (PROCESS_QUERY_INFORMATION, PTW32_FALSE, (DWORD) pid);

	  if (NULL == h)
	    {
	      errno =
		(GetLastError () ==
		 (0xFF & ERROR_ACCESS_DENIED)) ? EPERM : ESRCH;
	      return -1;
	    }
	  else
	    CloseHandle(h);
	}
    }

  return SCHED_OTHER;
}
