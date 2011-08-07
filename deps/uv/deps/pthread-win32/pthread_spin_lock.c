/*
 * pthread_spin_lock.c
 *
 * Description:
 * This translation unit implements spin lock primitives.
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
pthread_spin_lock (pthread_spinlock_t * lock)
{
  register pthread_spinlock_t s;

  if (NULL == lock || NULL == *lock)
    {
      return (EINVAL);
    }

  if (*lock == PTHREAD_SPINLOCK_INITIALIZER)
    {
      int result;

      if ((result = ptw32_spinlock_check_need_init (lock)) != 0)
	{
	  return (result);
	}
    }

  s = *lock;

  while ((PTW32_INTERLOCKED_LONG) PTW32_SPIN_LOCKED ==
	 PTW32_INTERLOCKED_COMPARE_EXCHANGE_LONG ((PTW32_INTERLOCKED_LONGPTR) &s->interlock,
					          (PTW32_INTERLOCKED_LONG) PTW32_SPIN_LOCKED,
					          (PTW32_INTERLOCKED_LONG) PTW32_SPIN_UNLOCKED))
    {
    }

  if (s->interlock == PTW32_SPIN_LOCKED)
    {
      return 0;
    }
  else if (s->interlock == PTW32_SPIN_USE_MUTEX)
    {
      return pthread_mutex_lock (&(s->u.mutex));
    }

  return EINVAL;
}
