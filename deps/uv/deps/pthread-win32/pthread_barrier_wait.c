/*
 * pthread_barrier_wait.c
 *
 * Description:
 * This translation unit implements barrier primitives.
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
pthread_barrier_wait (pthread_barrier_t * barrier)
{
  int result;
  pthread_barrier_t b;

  ptw32_mcs_local_node_t node;

  if (barrier == NULL || *barrier == (pthread_barrier_t) PTW32_OBJECT_INVALID)
    {
      return EINVAL;
    }

  ptw32_mcs_lock_acquire(&(*barrier)->lock, &node);

  b = *barrier;
  if (--b->nCurrentBarrierHeight == 0)
    {
      /*
       * We are the last thread to arrive at the barrier before it releases us.
       * Move our MCS local node to the global scope barrier handle so that the
       * last thread out (not necessarily us) can release the lock.
       */
      ptw32_mcs_node_transfer(&b->proxynode, &node);

      /*
       * Any threads that have not quite entered sem_wait below when the
       * multiple_post has completed will nevertheless continue through
       * the semaphore (barrier).
       */
      result = (b->nInitialBarrierHeight > 1
                ? sem_post_multiple (&(b->semBarrierBreeched),
				     b->nInitialBarrierHeight - 1) : 0);
    }
  else
    {
      ptw32_mcs_lock_release(&node);
      /*
       * Use the non-cancelable version of sem_wait().
       *
       * It is possible that all nInitialBarrierHeight-1 threads are
       * at this point when the last thread enters the barrier, resets
       * nCurrentBarrierHeight = nInitialBarrierHeight and leaves.
       * If pthread_barrier_destroy is called at that moment then the
       * barrier will be destroyed along with the semas.
       */
      result = ptw32_semwait (&(b->semBarrierBreeched));
    }

  if ((PTW32_INTERLOCKED_LONG)PTW32_INTERLOCKED_INCREMENT_LONG((PTW32_INTERLOCKED_LONGPTR)&b->nCurrentBarrierHeight)
		  == (PTW32_INTERLOCKED_LONG)b->nInitialBarrierHeight)
    {
      /*
       * We are the last thread to cross this barrier
       */
      ptw32_mcs_lock_release(&b->proxynode);
      if (0 == result)
        {
          result = PTHREAD_BARRIER_SERIAL_THREAD;
        }
    }

  return (result);
}
