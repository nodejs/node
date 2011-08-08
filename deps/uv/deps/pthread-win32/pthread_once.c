/*
 * pthread_once.c
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

int
pthread_once (pthread_once_t * once_control, void (*init_routine) (void))
{
  if (once_control == NULL || init_routine == NULL)
    {
      return EINVAL;
    }
  
  if ((PTW32_INTERLOCKED_LONG)PTW32_FALSE ==
      (PTW32_INTERLOCKED_LONG)PTW32_INTERLOCKED_EXCHANGE_ADD_LONG((PTW32_INTERLOCKED_LONGPTR)&once_control->done,
                                                                  (PTW32_INTERLOCKED_LONG)0)) /* MBR fence */
    {
      ptw32_mcs_local_node_t node;

      ptw32_mcs_lock_acquire((ptw32_mcs_lock_t *)&once_control->lock, &node);

      if (!once_control->done)
	{

#if defined(_MSC_VER) && _MSC_VER < 1400
#pragma inline_depth(0)
#endif

	  pthread_cleanup_push(ptw32_mcs_lock_release, &node);
	  (*init_routine)();
	  pthread_cleanup_pop(0);

#if defined(_MSC_VER) && _MSC_VER < 1400
#pragma inline_depth()
#endif

	  once_control->done = PTW32_TRUE;
	}

	ptw32_mcs_lock_release(&node);
    }

  return 0;

}				/* pthread_once */
