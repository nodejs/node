/*
 * pthread_mutex_trylock.c
 *
 * Description:
 * This translation unit implements mutual exclusion (mutex) primitives.
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
pthread_mutex_trylock (pthread_mutex_t * mutex)
{
  pthread_mutex_t mx;
  int kind;
  int result = 0;

  /*
   * Let the system deal with invalid pointers.
   */

  /*
   * We do a quick check to see if we need to do more work
   * to initialise a static mutex. We check
   * again inside the guarded section of ptw32_mutex_check_need_init()
   * to avoid race conditions.
   */
  if (*mutex >= PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
    {
      if ((result = ptw32_mutex_check_need_init (mutex)) != 0)
	{
	  return (result);
	}
    }

  mx = *mutex;
  kind = mx->kind;

  if (kind >= 0)
    {
      /* Non-robust */
      if (0 == (PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_COMPARE_EXCHANGE_LONG (
		         (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
		         (PTW32_INTERLOCKED_LONG) 1,
		         (PTW32_INTERLOCKED_LONG) 0))
        {
          if (kind != PTHREAD_MUTEX_NORMAL)
	    {
	      mx->recursive_count = 1;
	      mx->ownerThread = pthread_self ();
	    }
        }
      else
        {
          if (kind == PTHREAD_MUTEX_RECURSIVE &&
	      pthread_equal (mx->ownerThread, pthread_self ()))
	    {
	      mx->recursive_count++;
	    }
          else
	    {
	      result = EBUSY;
	    }
        }
    }
  else
    {
      /*
       * Robust types
       * All types record the current owner thread.
       * The mutex is added to a per thread list when ownership is acquired.
       */
      pthread_t self;
      ptw32_robust_state_t* statePtr = &mx->robustNode->stateInconsistent;

      if ((PTW32_INTERLOCKED_LONG)PTW32_ROBUST_NOTRECOVERABLE ==
                  PTW32_INTERLOCKED_EXCHANGE_ADD_LONG(
                    (PTW32_INTERLOCKED_LONGPTR)statePtr,
                    (PTW32_INTERLOCKED_LONG)0))
        {
          return ENOTRECOVERABLE;
        }

      self = pthread_self();
      kind = -kind - 1; /* Convert to non-robust range */

      if (0 == (PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_COMPARE_EXCHANGE_LONG (
        	         (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
        	         (PTW32_INTERLOCKED_LONG) 1,
        	         (PTW32_INTERLOCKED_LONG) 0))
        {
          if (kind != PTHREAD_MUTEX_NORMAL)
            {
              mx->recursive_count = 1;
            }
          ptw32_robust_mutex_add(mutex, self);
        }
      else
        {
          if (PTHREAD_MUTEX_RECURSIVE == kind &&
              pthread_equal (mx->ownerThread, pthread_self ()))
            {
              mx->recursive_count++;
            }
          else
            {
              if (EOWNERDEAD == (result = ptw32_robust_mutex_inherit(mutex)))
                {
                  mx->recursive_count = 1;
                  ptw32_robust_mutex_add(mutex, self);
                }
              else
                {
                  if (0 == result)
                    { 
	              result = EBUSY;
                    }
                }
	    }
        }
    }

  return (result);
}
