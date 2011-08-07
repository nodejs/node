/*
 * pthread_cancel.c
 *
 * Description:
 * POSIX thread functions related to thread cancellation.
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
#include "context.h"

static void
ptw32_cancel_self (void)
{
  ptw32_throw (PTW32_EPS_CANCEL);

  /* Never reached */
}

static void CALLBACK
ptw32_cancel_callback (ULONG_PTR unused)
{
  ptw32_throw (PTW32_EPS_CANCEL);

  /* Never reached */
}

/*
 * ptw32_RegisterCancelation() -
 * Must have args of same type as QueueUserAPCEx because this function
 * is a substitute for QueueUserAPCEx if it's not available.
 */
DWORD
ptw32_RegisterCancelation (PAPCFUNC unused1, HANDLE threadH, DWORD unused2)
{
  CONTEXT context;

  context.ContextFlags = CONTEXT_CONTROL;
  GetThreadContext (threadH, &context);
  PTW32_PROGCTR (context) = (DWORD_PTR) ptw32_cancel_self;
  SetThreadContext (threadH, &context);
  return 0;
}

int
pthread_cancel (pthread_t thread)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function requests cancellation of 'thread'.
      *
      * PARAMETERS
      *      thread
      *              reference to an instance of pthread_t
      *
      *
      * DESCRIPTION
      *      This function requests cancellation of 'thread'.
      *      NOTE: cancellation is asynchronous; use pthread_join to
      *                wait for termination of 'thread' if necessary.
      *
      * RESULTS
      *              0               successfully requested cancellation,
      *              ESRCH           no thread found corresponding to 'thread',
      *              ENOMEM          implicit self thread create failed.
      * ------------------------------------------------------
      */
{
  int result;
  int cancel_self;
  pthread_t self;
  ptw32_thread_t * tp;
  ptw32_mcs_local_node_t stateLock;

  result = pthread_kill (thread, 0);

  if (0 != result)
    {
      return result;
    }

  if ((self = pthread_self ()).p == NULL)
    {
      return ENOMEM;
    };

  /*
   * For self cancellation we need to ensure that a thread can't
   * deadlock itself trying to cancel itself asynchronously
   * (pthread_cancel is required to be an async-cancel
   * safe function).
   */
  cancel_self = pthread_equal (thread, self);

  tp = (ptw32_thread_t *) thread.p;

  /*
   * Lock for async-cancel safety.
   */
  ptw32_mcs_lock_acquire (&tp->stateLock, &stateLock);

  if (tp->cancelType == PTHREAD_CANCEL_ASYNCHRONOUS
      && tp->cancelState == PTHREAD_CANCEL_ENABLE
      && tp->state < PThreadStateCanceling)
    {
      if (cancel_self)
	{
	  tp->state = PThreadStateCanceling;
	  tp->cancelState = PTHREAD_CANCEL_DISABLE;

	  ptw32_mcs_lock_release (&stateLock);
	  ptw32_throw (PTW32_EPS_CANCEL);

	  /* Never reached */
	}
      else
	{
	  HANDLE threadH = tp->threadH;

	  SuspendThread (threadH);

	  if (WaitForSingleObject (threadH, 0) == WAIT_TIMEOUT)
	    {
	      tp->state = PThreadStateCanceling;
	      tp->cancelState = PTHREAD_CANCEL_DISABLE;
	      /*
	       * If alertdrv and QueueUserAPCEx is available then the following
	       * will result in a call to QueueUserAPCEx with the args given, otherwise
	       * this will result in a call to ptw32_RegisterCancelation and only
	       * the threadH arg will be used.
	       */
	      ptw32_register_cancelation ((PAPCFUNC)ptw32_cancel_callback, threadH, 0);
	      ptw32_mcs_lock_release (&stateLock);
	      ResumeThread (threadH);
	    }
	}
    }
  else
    {
      /*
       * Set for deferred cancellation.
       */
      if (tp->state < PThreadStateCancelPending)
	{
	  tp->state = PThreadStateCancelPending;
	  if (!SetEvent (tp->cancelEvent))
	    {
	      result = ESRCH;
	    }
	}
      else if (tp->state >= PThreadStateCanceling)
	{
	  result = ESRCH;
	}

      ptw32_mcs_lock_release (&stateLock);
    }

  return (result);
}
