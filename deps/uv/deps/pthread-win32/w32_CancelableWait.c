/*
 * w32_CancelableWait.c
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


static INLINE int
ptw32_cancelable_wait (HANDLE waitHandle, DWORD timeout)
     /*
      * -------------------------------------------------------------------
      * This provides an extra hook into the pthread_cancel
      * mechanism that will allow you to wait on a Windows handle and make it a
      * cancellation point. This function blocks until the given WIN32 handle is
      * signaled or pthread_cancel has been called. It is implemented using
      * WaitForMultipleObjects on 'waitHandle' and a manually reset WIN32
      * event used to implement pthread_cancel.
      * 
      * Given this hook it would be possible to implement more of the cancellation
      * points.
      * -------------------------------------------------------------------
      */
{
  int result;
  pthread_t self;
  ptw32_thread_t * sp;
  HANDLE handles[2];
  DWORD nHandles = 1;
  DWORD status;

  handles[0] = waitHandle;

  self = pthread_self();
  sp = (ptw32_thread_t *) self.p;

  if (sp != NULL)
    {
      /*
       * Get cancelEvent handle
       */
      if (sp->cancelState == PTHREAD_CANCEL_ENABLE)
	{

	  if ((handles[1] = sp->cancelEvent) != NULL)
	    {
	      nHandles++;
	    }
	}
    }
  else
    {
      handles[1] = NULL;
    }

  status = WaitForMultipleObjects (nHandles, handles, PTW32_FALSE, timeout);

  switch (status - WAIT_OBJECT_0)
    {
    case 0:
      /*
       * Got the handle.
       * In the event that both handles are signalled, the smallest index
       * value (us) is returned. As it has been arranged, this ensures that
       * we don't drop a signal that we should act on (i.e. semaphore,
       * mutex, or condition variable etc).
       */
      result = 0;
      break;

    case 1:
      /*
       * Got cancel request.
       * In the event that both handles are signaled, the cancel will
       * be ignored (see case 0 comment).
       */
      ResetEvent (handles[1]);

      if (sp != NULL)
	{
          ptw32_mcs_local_node_t stateLock;
	  /*
	   * Should handle POSIX and implicit POSIX threads..
	   * Make sure we haven't been async-canceled in the meantime.
	   */
	  ptw32_mcs_lock_acquire (&sp->stateLock, &stateLock);
	  if (sp->state < PThreadStateCanceling)
	    {
	      sp->state = PThreadStateCanceling;
	      sp->cancelState = PTHREAD_CANCEL_DISABLE;
	      ptw32_mcs_lock_release (&stateLock);
	      ptw32_throw (PTW32_EPS_CANCEL);

	      /* Never reached */
	    }
	  ptw32_mcs_lock_release (&stateLock);
	}

      /* Should never get to here. */
      result = EINVAL;
      break;

    default:
      if (status == WAIT_TIMEOUT)
	{
	  result = ETIMEDOUT;
	}
      else
	{
	  result = EINVAL;
	}
      break;
    }

  return (result);

}				/* CancelableWait */

int
pthreadCancelableWait (HANDLE waitHandle)
{
  return (ptw32_cancelable_wait (waitHandle, INFINITE));
}

int
pthreadCancelableTimedWait (HANDLE waitHandle, DWORD timeout)
{
  return (ptw32_cancelable_wait (waitHandle, timeout));
}
