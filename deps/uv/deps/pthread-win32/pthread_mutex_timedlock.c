/*
 * pthread_mutex_timedlock.c
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


static INLINE int
ptw32_timed_eventwait (HANDLE event, const struct timespec *abstime)
     /*
      * ------------------------------------------------------
      * DESCRIPTION
      *      This function waits on an event until signaled or until
      *      abstime passes.
      *      If abstime has passed when this routine is called then
      *      it returns a result to indicate this.
      *
      *      If 'abstime' is a NULL pointer then this function will
      *      block until it can successfully decrease the value or
      *      until interrupted by a signal.
      *
      *      This routine is not a cancelation point.
      *
      * RESULTS
      *              0               successfully signaled,
      *              ETIMEDOUT       abstime passed
      *              EINVAL          'event' is not a valid event,
      *
      * ------------------------------------------------------
      */
{

  DWORD milliseconds;
  DWORD status;

  if (event == NULL)
    {
      return EINVAL;
    }
  else
    {
      if (abstime == NULL)
	{
	  milliseconds = INFINITE;
	}
      else
	{
	  /* 
	   * Calculate timeout as milliseconds from current system time. 
	   */
	  milliseconds = ptw32_relmillisecs (abstime);
	}

      status = WaitForSingleObject (event, milliseconds);

      if (status == WAIT_OBJECT_0)
	{
	  return 0;
	}
      else if (status == WAIT_TIMEOUT)
	{
	  return ETIMEDOUT;
	}
      else
	{
	  return EINVAL;
	}
    }

  return 0;

}				/* ptw32_timed_semwait */


int
pthread_mutex_timedlock (pthread_mutex_t * mutex,
			 const struct timespec *abstime)
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
      if (mx->kind == PTHREAD_MUTEX_NORMAL)
        {
          if ((PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_EXCHANGE_LONG(
		       (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
		       (PTW32_INTERLOCKED_LONG) 1) != 0)
	    {
              while ((PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_EXCHANGE_LONG(
                              (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
			      (PTW32_INTERLOCKED_LONG) -1) != 0)
                {
	          if (0 != (result = ptw32_timed_eventwait (mx->event, abstime)))
		    {
		      return result;
		    }
	        }
	    }
        }
      else
        {
          pthread_t self = pthread_self();

          if ((PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_COMPARE_EXCHANGE_LONG(
                       (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
		       (PTW32_INTERLOCKED_LONG) 1,
		       (PTW32_INTERLOCKED_LONG) 0) == 0)
	    {
	      mx->recursive_count = 1;
	      mx->ownerThread = self;
	    }
          else
	    {
	      if (pthread_equal (mx->ownerThread, self))
	        {
	          if (mx->kind == PTHREAD_MUTEX_RECURSIVE)
		    {
		      mx->recursive_count++;
		    }
	          else
		    {
		      return EDEADLK;
		    }
	        }
	      else
	        {
                  while ((PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_EXCHANGE_LONG(
                                  (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
			          (PTW32_INTERLOCKED_LONG) -1) != 0)
                    {
		      if (0 != (result = ptw32_timed_eventwait (mx->event, abstime)))
		        {
		          return result;
		        }
		    }

	          mx->recursive_count = 1;
	          mx->ownerThread = self;
	        }
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
      ptw32_robust_state_t* statePtr = &mx->robustNode->stateInconsistent;

      if ((PTW32_INTERLOCKED_LONG)PTW32_ROBUST_NOTRECOVERABLE == PTW32_INTERLOCKED_EXCHANGE_ADD_LONG(
                                                 (PTW32_INTERLOCKED_LONGPTR)statePtr,
                                                 (PTW32_INTERLOCKED_LONG)0))
        {
          result = ENOTRECOVERABLE;
        }
      else
        {
          pthread_t self = pthread_self();

          kind = -kind - 1; /* Convert to non-robust range */

          if (PTHREAD_MUTEX_NORMAL == kind)
            {
              if ((PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_EXCHANGE_LONG(
		           (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
		           (PTW32_INTERLOCKED_LONG) 1) != 0)
	        {
                  while (0 == (result = ptw32_robust_mutex_inherit(mutex))
                           && (PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_EXCHANGE_LONG(
                                  (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
			          (PTW32_INTERLOCKED_LONG) -1) != 0)
                    {
	              if (0 != (result = ptw32_timed_eventwait (mx->event, abstime)))
		        {
		          return result;
		        }
                      if ((PTW32_INTERLOCKED_LONG)PTW32_ROBUST_NOTRECOVERABLE ==
                                  PTW32_INTERLOCKED_EXCHANGE_ADD_LONG(
                                    (PTW32_INTERLOCKED_LONGPTR)statePtr,
                                    (PTW32_INTERLOCKED_LONG)0))
                        {
                          /* Unblock the next thread */
                          SetEvent(mx->event);
                          result = ENOTRECOVERABLE;
                          break;
                        }
	            }

                  if (0 == result || EOWNERDEAD == result)
                    {
                      /*
                       * Add mutex to the per-thread robust mutex currently-held list.
                       * If the thread terminates, all mutexes in this list will be unlocked.
                       */
                      ptw32_robust_mutex_add(mutex, self);
                    }
	        }
            }
          else
            {
              pthread_t self = pthread_self();

              if (0 == (PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_COMPARE_EXCHANGE_LONG(
                           (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
		           (PTW32_INTERLOCKED_LONG) 1,
		           (PTW32_INTERLOCKED_LONG) 0))
	        {
	          mx->recursive_count = 1;
                  /*
                   * Add mutex to the per-thread robust mutex currently-held list.
                   * If the thread terminates, all mutexes in this list will be unlocked.
                   */
                  ptw32_robust_mutex_add(mutex, self);
	        }
              else
	        {
	          if (pthread_equal (mx->ownerThread, self))
	            {
	              if (PTHREAD_MUTEX_RECURSIVE == kind)
		        {
		          mx->recursive_count++;
		        }
	              else
		        {
		          return EDEADLK;
		        }
	            }
	          else
	            {
                      while (0 == (result = ptw32_robust_mutex_inherit(mutex))
                               && (PTW32_INTERLOCKED_LONG) PTW32_INTERLOCKED_EXCHANGE_LONG(
                                          (PTW32_INTERLOCKED_LONGPTR) &mx->lock_idx,
			                  (PTW32_INTERLOCKED_LONG) -1) != 0)
                        {
		          if (0 != (result = ptw32_timed_eventwait (mx->event, abstime)))
		            {
		              return result;
		            }
		        }

                      if ((PTW32_INTERLOCKED_LONG)PTW32_ROBUST_NOTRECOVERABLE ==
                                  PTW32_INTERLOCKED_EXCHANGE_ADD_LONG(
                                    (PTW32_INTERLOCKED_LONGPTR)statePtr,
                                    (PTW32_INTERLOCKED_LONG)0))
                        {
                          /* Unblock the next thread */
                          SetEvent(mx->event);
                          result = ENOTRECOVERABLE;
                        }
                      else if (0 == result || EOWNERDEAD == result)
                        {
                          mx->recursive_count = 1;
                          /*
                           * Add mutex to the per-thread robust mutex currently-held list.
                           * If the thread terminates, all mutexes in this list will be unlocked.
                           */
                          ptw32_robust_mutex_add(mutex, self);
                        }
	            }
	        }
            }
        }
    }

  return result;
}
