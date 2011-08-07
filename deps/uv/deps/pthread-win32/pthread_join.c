/*
 * pthread_join.c
 *
 * Description:
 * This translation unit implements functions related to thread
 * synchronisation.
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
 * Not needed yet, but defining it should indicate clashes with build target
 * environment that should be fixed.
 */
#if !defined(WINCE)
#  include <signal.h>
#endif


int
pthread_join (pthread_t thread, void **value_ptr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function waits for 'thread' to terminate and
      *      returns the thread's exit value if 'value_ptr' is not
      *      NULL. This also detaches the thread on successful
      *      completion.
      *
      * PARAMETERS
      *      thread
      *              an instance of pthread_t
      *
      *      value_ptr
      *              pointer to an instance of pointer to void
      *
      *
      * DESCRIPTION
      *      This function waits for 'thread' to terminate and
      *      returns the thread's exit value if 'value_ptr' is not
      *      NULL. This also detaches the thread on successful
      *      completion.
      *      NOTE:   detached threads cannot be joined or canceled
      *
      * RESULTS
      *              0               'thread' has completed
      *              EINVAL          thread is not a joinable thread,
      *              ESRCH           no thread could be found with ID 'thread',
      *              ENOENT          thread couldn't find it's own valid handle,
      *              EDEADLK         attempt to join thread with self
      *
      * ------------------------------------------------------
      */
{
  int result;
  pthread_t self;
  ptw32_thread_t * tp = (ptw32_thread_t *) thread.p;
  ptw32_mcs_local_node_t node;

  ptw32_mcs_lock_acquire(&ptw32_thread_reuse_lock, &node);

  if (NULL == tp
      || thread.x != tp->ptHandle.x)
    {
      result = ESRCH;
    }
  else if (PTHREAD_CREATE_DETACHED == tp->detachState)
    {
      result = EINVAL;
    }
  else
    {
      result = 0;
    }

  ptw32_mcs_lock_release(&node);

  if (result == 0)
    {
      /* 
       * The target thread is joinable and can't be reused before we join it.
       */
      self = pthread_self();

      if (NULL == self.p)
	{
	  result = ENOENT;
	}
      else if (pthread_equal (self, thread))
	{
	  result = EDEADLK;
	}
      else
	{
	  /*
	   * Pthread_join is a cancelation point.
	   * If we are canceled then our target thread must not be
	   * detached (destroyed). This is guarranteed because
	   * pthreadCancelableWait will not return if we
	   * are canceled.
	   */
	  result = pthreadCancelableWait (tp->threadH);

	  if (0 == result)
	    {
	      if (value_ptr != NULL)
		{
		  *value_ptr = tp->exitStatus;
		}

	      /*
	       * The result of making multiple simultaneous calls to
	       * pthread_join() or pthread_detach() specifying the same
	       * target is undefined.
	       */
	      result = pthread_detach (thread);
	    }
	  else
	    {
	      result = ESRCH;
	    }
	}
    }

  return (result);

}				/* pthread_join */
