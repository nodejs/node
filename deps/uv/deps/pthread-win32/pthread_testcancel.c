/*
 * pthread_testcancel.c
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


void
pthread_testcancel (void)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function creates a deferred cancellation point
      *      in the calling thread. The call has no effect if the
      *      current cancelability state is
      *              PTHREAD_CANCEL_DISABLE
      *
      * PARAMETERS
      *      N/A
      *
      *
      * DESCRIPTION
      *      This function creates a deferred cancellation point
      *      in the calling thread. The call has no effect if the
      *      current cancelability state is
      *              PTHREAD_CANCEL_DISABLE
      *
      *      NOTES:
      *      1)      Cancellation is asynchronous. Use pthread_join
      *              to wait for termination of thread if necessary
      *
      * RESULTS
      *              N/A
      *
      * ------------------------------------------------------
      */
{
  ptw32_mcs_local_node_t stateLock;
  pthread_t self = pthread_self ();
  ptw32_thread_t * sp = (ptw32_thread_t *) self.p;

  if (sp == NULL)
    {
      return;
    }

  /*
   * Pthread_cancel() will have set sp->state to PThreadStateCancelPending
   * and set an event, so no need to enter kernel space if
   * sp->state != PThreadStateCancelPending - that only slows us down.
   */
  if (sp->state != PThreadStateCancelPending)
    {
      return;
    }

  ptw32_mcs_lock_acquire (&sp->stateLock, &stateLock);

  if (sp->cancelState != PTHREAD_CANCEL_DISABLE)
    {
      ResetEvent(sp->cancelEvent);
      sp->state = PThreadStateCanceling;
      sp->cancelState = PTHREAD_CANCEL_DISABLE;
      ptw32_mcs_lock_release (&stateLock);
      ptw32_throw (PTW32_EPS_CANCEL);
      /* Never returns here */
    }

  ptw32_mcs_lock_release (&stateLock);
}				/* pthread_testcancel */
