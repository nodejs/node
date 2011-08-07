/*
 * ptw32_MCS_lock.c
 *
 * Description:
 * This translation unit implements queue-based locks.
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

/*
 * About MCS locks:
 *
 * MCS locks are queue-based locks, where the queue nodes are local to the
 * thread. The 'lock' is nothing more than a global pointer that points to
 * the last node in the queue, or is NULL if the queue is empty.
 * 
 * Originally designed for use as spin locks requiring no kernel resources
 * for synchronisation or blocking, the implementation below has adapted
 * the MCS spin lock for use as a general mutex that will suspend threads
 * when there is lock contention.
 *
 * Because the queue nodes are thread-local, most of the memory read/write
 * operations required to add or remove nodes from the queue do not trigger
 * cache-coherence updates.
 *
 * Like 'named' mutexes, MCS locks consume system resources transiently -
 * they are able to acquire and free resources automatically - but MCS
 * locks do not require any unique 'name' to identify the lock to all
 * threads using it.
 *
 * Usage of MCS locks:
 *
 * - you need a global ptw32_mcs_lock_t instance initialised to 0 or NULL.
 * - you need a local thread-scope ptw32_mcs_local_node_t instance, which
 *   may serve several different locks but you need at least one node for
 *   every lock held concurrently by a thread.
 *
 * E.g.:
 * 
 * ptw32_mcs_lock_t lock1 = 0;
 * ptw32_mcs_lock_t lock2 = 0;
 *
 * void *mythread(void *arg)
 * {
 *   ptw32_mcs_local_node_t node;
 *
 *   ptw32_mcs_acquire (&lock1, &node);
 *   ptw32_mcs_lock_release (&node);
 *
 *   ptw32_mcs_lock_acquire (&lock2, &node);
 *   ptw32_mcs_lock_release (&node);
 *   {
 *      ptw32_mcs_local_node_t nodex;
 *
 *      ptw32_mcs_lock_acquire (&lock1, &node);
 *      ptw32_mcs_lock_acquire (&lock2, &nodex);
 *
 *      ptw32_mcs_lock_release (&nodex);
 *      ptw32_mcs_lock_release (&node);
 *   }
 *   return (void *)0;
 * }
 */

#include "pthread.h"
#include "sched.h"
#include "implement.h"

/*
 * ptw32_mcs_flag_set -- notify another thread about an event.
 * 
 * Set event if an event handle has been stored in the flag, and
 * set flag to -1 otherwise. Note that -1 cannot be a valid handle value.
 */
INLINE void 
ptw32_mcs_flag_set (HANDLE * flag)
{
  HANDLE e = (HANDLE)(PTW32_INTERLOCKED_SIZE)PTW32_INTERLOCKED_COMPARE_EXCHANGE_SIZE(
						(PTW32_INTERLOCKED_SIZEPTR)flag,
						(PTW32_INTERLOCKED_SIZE)-1,
						(PTW32_INTERLOCKED_SIZE)0);
  if ((HANDLE)0 != e)
    {
      /* another thread has already stored an event handle in the flag */
      SetEvent(e);
    }
}

/*
 * ptw32_mcs_flag_set -- wait for notification from another.
 * 
 * Store an event handle in the flag and wait on it if the flag has not been
 * set, and proceed without creating an event otherwise.
 */
INLINE void 
ptw32_mcs_flag_wait (HANDLE * flag)
{
  if ((PTW32_INTERLOCKED_LONG)0 ==
        PTW32_INTERLOCKED_EXCHANGE_ADD_SIZE((PTW32_INTERLOCKED_SIZEPTR)flag,
                                            (PTW32_INTERLOCKED_SIZE)0)) /* MBR fence */
    {
      /* the flag is not set. create event. */

      HANDLE e = CreateEvent(NULL, PTW32_FALSE, PTW32_FALSE, NULL);

      if ((PTW32_INTERLOCKED_SIZE)0 == PTW32_INTERLOCKED_COMPARE_EXCHANGE_SIZE(
			                  (PTW32_INTERLOCKED_SIZEPTR)flag,
			                  (PTW32_INTERLOCKED_SIZE)e,
			                  (PTW32_INTERLOCKED_SIZE)0))
	{
	  /* stored handle in the flag. wait on it now. */
	  WaitForSingleObject(e, INFINITE);
	}

      CloseHandle(e);
    }
}

/*
 * ptw32_mcs_lock_acquire -- acquire an MCS lock.
 * 
 * See: 
 * J. M. Mellor-Crummey and M. L. Scott.
 * Algorithms for Scalable Synchronization on Shared-Memory Multiprocessors.
 * ACM Transactions on Computer Systems, 9(1):21-65, Feb. 1991.
 */
#if defined(PTW32_BUILD_INLINED)
INLINE 
#endif /* PTW32_BUILD_INLINED */
void 
ptw32_mcs_lock_acquire (ptw32_mcs_lock_t * lock, ptw32_mcs_local_node_t * node)
{
  ptw32_mcs_local_node_t  *pred;
  
  node->lock = lock;
  node->nextFlag = 0;
  node->readyFlag = 0;
  node->next = 0; /* initially, no successor */
  
  /* queue for the lock */
  pred = (ptw32_mcs_local_node_t *)PTW32_INTERLOCKED_EXCHANGE_PTR((PTW32_INTERLOCKED_PVOID_PTR)lock,
								  (PTW32_INTERLOCKED_PVOID)node);

  if (0 != pred)
    {
      /* the lock was not free. link behind predecessor. */
      pred->next = node;
      ptw32_mcs_flag_set(&pred->nextFlag);
      ptw32_mcs_flag_wait(&node->readyFlag);
    }
}

/*
 * ptw32_mcs_lock_release -- release an MCS lock.
 * 
 * See: 
 * J. M. Mellor-Crummey and M. L. Scott.
 * Algorithms for Scalable Synchronization on Shared-Memory Multiprocessors.
 * ACM Transactions on Computer Systems, 9(1):21-65, Feb. 1991.
 */
#if defined(PTW32_BUILD_INLINED)
INLINE 
#endif /* PTW32_BUILD_INLINED */
void 
ptw32_mcs_lock_release (ptw32_mcs_local_node_t * node)
{
  ptw32_mcs_lock_t *lock = node->lock;
  ptw32_mcs_local_node_t *next =
    (ptw32_mcs_local_node_t *)
      PTW32_INTERLOCKED_EXCHANGE_ADD_SIZE((PTW32_INTERLOCKED_SIZEPTR)&node->next, (PTW32_INTERLOCKED_SIZE)0); /* MBR fence */

  if (0 == next)
    {
      /* no known successor */

      if (node == (ptw32_mcs_local_node_t *)
	  PTW32_INTERLOCKED_COMPARE_EXCHANGE_PTR((PTW32_INTERLOCKED_PVOID_PTR)lock,
						 (PTW32_INTERLOCKED_PVOID)0,
						 (PTW32_INTERLOCKED_PVOID)node))
	{
	  /* no successor, lock is free now */
	  return;
	}
  
      /* wait for successor */
      ptw32_mcs_flag_wait(&node->nextFlag);
      next = (ptw32_mcs_local_node_t *)
	PTW32_INTERLOCKED_EXCHANGE_ADD_SIZE((PTW32_INTERLOCKED_SIZEPTR)&node->next, (PTW32_INTERLOCKED_SIZE)0); /* MBR fence */
    }

  /* pass the lock */
  ptw32_mcs_flag_set(&next->readyFlag);
}

/*
  * ptw32_mcs_lock_try_acquire
 */
#if defined(PTW32_BUILD_INLINED)
INLINE 
#endif /* PTW32_BUILD_INLINED */
int 
ptw32_mcs_lock_try_acquire (ptw32_mcs_lock_t * lock, ptw32_mcs_local_node_t * node)
{
  node->lock = lock;
  node->nextFlag = 0;
  node->readyFlag = 0;
  node->next = 0; /* initially, no successor */

  return ((PTW32_INTERLOCKED_PVOID)PTW32_INTERLOCKED_COMPARE_EXCHANGE_PTR((PTW32_INTERLOCKED_PVOID_PTR)lock,
                                                        (PTW32_INTERLOCKED_PVOID)node,
                                                        (PTW32_INTERLOCKED_PVOID)0)
                                 == (PTW32_INTERLOCKED_PVOID)0) ? 0 : EBUSY;
}

/*
 * ptw32_mcs_node_transfer -- move an MCS lock local node, usually from thread
 * space to, for example, global space so that another thread can release
 * the lock on behalf of the current lock owner.
 *
 * Example: used in pthread_barrier_wait where we want the last thread out of
 * the barrier to release the lock owned by the last thread to enter the barrier
 * (the one that releases all threads but not necessarily the last to leave).
 *
 * Should only be called by the thread that has the lock.
 */
#if defined(PTW32_BUILD_INLINED)
INLINE 
#endif /* PTW32_BUILD_INLINED */
void 
ptw32_mcs_node_transfer (ptw32_mcs_local_node_t * new_node, ptw32_mcs_local_node_t * old_node)
{
  new_node->lock = old_node->lock;
  new_node->nextFlag = 0; /* Not needed - used only in initial Acquire */
  new_node->readyFlag = 0; /* Not needed - we were waiting on this */
  new_node->next = 0;

  if ((ptw32_mcs_local_node_t *)PTW32_INTERLOCKED_COMPARE_EXCHANGE_PTR((PTW32_INTERLOCKED_PVOID_PTR)new_node->lock,
                                                                       (PTW32_INTERLOCKED_PVOID)new_node,
                                                                       (PTW32_INTERLOCKED_PVOID)old_node)
       != old_node)
    {
      /*
       * A successor has queued after us, so wait for them to link to us
       */
      while (old_node->next == 0)
        {
          sched_yield();
        }
      new_node->next = old_node->next;
    }
}
