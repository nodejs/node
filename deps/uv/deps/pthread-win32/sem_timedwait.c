/*
 * -------------------------------------------------------------
 *
 * Module: sem_timedwait.c
 *
 * Purpose:
 *	Semaphores aren't actually part of the PThreads standard.
 *	They are defined by the POSIX Standard:
 *
 *		POSIX 1003.1b-1993	(POSIX.1b)
 *
 * -------------------------------------------------------------
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
#include "semaphore.h"
#include "implement.h"


typedef struct {
  sem_t sem;
  int * resultPtr;
} sem_timedwait_cleanup_args_t;


static void PTW32_CDECL
ptw32_sem_timedwait_cleanup (void * args)
{
  sem_timedwait_cleanup_args_t * a = (sem_timedwait_cleanup_args_t *)args;
  sem_t s = a->sem;

  if (pthread_mutex_lock (&s->lock) == 0)
    {
      /*
       * We either timed out or were cancelled.
       * If someone has posted between then and now we try to take the semaphore.
       * Otherwise the semaphore count may be wrong after we
       * return. In the case of a cancellation, it is as if we
       * were cancelled just before we return (after taking the semaphore)
       * which is ok.
       */
      if (WaitForSingleObject(s->sem, 0) == WAIT_OBJECT_0)
	{
	  /* We got the semaphore on the second attempt */
	  *(a->resultPtr) = 0;
	}
      else
	{
	  /* Indicate we're no longer waiting */
	  s->value++;
#if defined(NEED_SEM)
	  if (s->value > 0)
	    {
	      s->leftToUnblock = 0;
	    }
#else
          /*
           * Don't release the W32 sema, it doesn't need adjustment
           * because it doesn't record the number of waiters.
           */
#endif
	}
      (void) pthread_mutex_unlock (&s->lock);
    }
}


int
sem_timedwait (sem_t * sem, const struct timespec *abstime)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function waits on a semaphore possibly until
      *      'abstime' time.
      *
      * PARAMETERS
      *      sem
      *              pointer to an instance of sem_t
      *
      *      abstime
      *              pointer to an instance of struct timespec
      *
      * DESCRIPTION
      *      This function waits on a semaphore. If the
      *      semaphore value is greater than zero, it decreases
      *      its value by one. If the semaphore value is zero, then
      *      the calling thread (or process) is blocked until it can
      *      successfully decrease the value or until interrupted by
      *      a signal.
      *
      *      If 'abstime' is a NULL pointer then this function will
      *      block until it can successfully decrease the value or
      *      until interrupted by a signal.
      *
      * RESULTS
      *              0               successfully decreased semaphore,
      *              -1              failed, error in errno
      * ERRNO
      *              EINVAL          'sem' is not a valid semaphore,
      *              ENOSYS          semaphores are not supported,
      *              EINTR           the function was interrupted by a signal,
      *              EDEADLK         a deadlock condition was detected.
      *              ETIMEDOUT       abstime elapsed before success.
      *
      * ------------------------------------------------------
      */
{
  int result = 0;
  sem_t s = *sem;

  pthread_testcancel();

  if (sem == NULL)
    {
      result = EINVAL;
    }
  else
    {
      DWORD milliseconds;

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

      if ((result = pthread_mutex_lock (&s->lock)) == 0)
	{
	  int v;

	  /* See sem_destroy.c
	   */
	  if (*sem == NULL)
	    {
	      (void) pthread_mutex_unlock (&s->lock);
	      errno = EINVAL;
	      return -1;
	    }

	  v = --s->value;
	  (void) pthread_mutex_unlock (&s->lock);

	  if (v < 0)
	    {
#if defined(NEED_SEM)
	      int timedout;
#endif
	      sem_timedwait_cleanup_args_t cleanup_args;

	      cleanup_args.sem = s;
	      cleanup_args.resultPtr = &result;

#if defined(_MSC_VER) && _MSC_VER < 1400
#pragma inline_depth(0)
#endif
	      /* Must wait */
              pthread_cleanup_push(ptw32_sem_timedwait_cleanup, (void *) &cleanup_args);
#if defined(NEED_SEM)
	      timedout =
#endif
	      result = pthreadCancelableTimedWait (s->sem, milliseconds);
	      pthread_cleanup_pop(result);
#if defined(_MSC_VER) && _MSC_VER < 1400
#pragma inline_depth()
#endif

#if defined(NEED_SEM)

	      if (!timedout && pthread_mutex_lock (&s->lock) == 0)
	        {
        	  if (*sem == NULL)
        	    {
        	      (void) pthread_mutex_unlock (&s->lock);
        	      errno = EINVAL;
        	      return -1;
        	    }

	          if (s->leftToUnblock > 0)
	            {
		      --s->leftToUnblock;
		      SetEvent(s->sem);
		    }
	          (void) pthread_mutex_unlock (&s->lock);
	        }

#endif /* NEED_SEM */

	    }
	}

    }

  if (result != 0)
    {

      errno = result;
      return -1;

    }

  return 0;

}				/* sem_timedwait */
