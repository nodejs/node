/*
 * pthread_mutex_consistent.c
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

/*
 * From the Sun Multi-threaded Programming Guide
 *
 * robustness defines the behavior when the owner of the mutex terminates without unlocking the
 * mutex, usually because its process terminated abnormally. The value of robustness that is
 * defined in pthread.h is PTHREAD_MUTEX_ROBUST or PTHREAD_MUTEX_STALLED. The
 * default value is PTHREAD_MUTEX_STALLED .
 * ■ PTHREAD_MUTEX_STALLED
 * When the owner of the mutex terminates without unlocking the mutex, all subsequent calls
 * to pthread_mutex_lock() are blocked from progress in an unspecified manner.
 * ■ PTHREAD_MUTEX_ROBUST
 * When the owner of the mutex terminates without unlocking the mutex, the mutex is
 * unlocked. The next owner of this mutex acquires the mutex with an error return of
 * EOWNERDEAD.
 * Note – Your application must always check the return code from pthread_mutex_lock() for
 * a mutex initialized with the PTHREAD_MUTEX_ROBUST attribute.
 * ■ The new owner of this mutex should make the state protected by the mutex consistent.
 * This state might have been left inconsistent when the previous owner terminated.
 * ■ If the new owner is able to make the state consistent, call
 * pthread_mutex_consistent() for the mutex before unlocking the mutex. This
 * marks the mutex as consistent and subsequent calls to pthread_mutex_lock() and
 * pthread_mutex_unlock() will behave in the normal manner.
 * ■ If the new owner is not able to make the state consistent, do not call
 * pthread_mutex_consistent() for the mutex, but unlock the mutex.
 * All waiters are woken up and all subsequent calls to pthread_mutex_lock() fail to
 * acquire the mutex. The return code is ENOTRECOVERABLE. The mutex can be made
 * consistent by calling pthread_mutex_destroy() to uninitialize the mutex, and calling
 * pthread_mutex_int() to reinitialize the mutex.However, the state that was protected
 * by the mutex remains inconsistent and some form of application recovery is required.
 * ■ If the thread that acquires the lock with EOWNERDEAD terminates without unlocking the
 * mutex, the next owner acquires the lock with an EOWNERDEAD return code.
 */
#if !defined(_UWIN)
/*#   include <process.h> */
#endif
#include "pthread.h"
#include "implement.h"

int
ptw32_robust_mutex_inherit(pthread_mutex_t * mutex)
{
  int result;
  pthread_mutex_t mx = *mutex;
  ptw32_robust_node_t* robust = mx->robustNode;

  switch ((LONG)PTW32_INTERLOCKED_COMPARE_EXCHANGE_LONG(
            (PTW32_INTERLOCKED_LONGPTR)&robust->stateInconsistent,
            (PTW32_INTERLOCKED_LONG)PTW32_ROBUST_INCONSISTENT,
            (PTW32_INTERLOCKED_LONG)-1 /* The terminating thread sets this */))
    {
      case -1L:
          result = EOWNERDEAD;
          break;
      case (LONG)PTW32_ROBUST_NOTRECOVERABLE:
          result = ENOTRECOVERABLE;
          break;
      default:
          result = 0;
          break;
    }

  return result;
}

/*
 * The next two internal support functions depend on only being
 * called by the thread that owns the robust mutex. This enables
 * us to avoid additional locks.
 * Any mutex currently in the thread's robust mutex list is held
 * by the thread, again eliminating the need for locks.
 * The forward/backward links allow the thread to unlock mutexes
 * in any order, not necessarily the reverse locking order.
 * This is all possible because it is an error if a thread that
 * does not own the [robust] mutex attempts to unlock it.
 */

void
ptw32_robust_mutex_add(pthread_mutex_t* mutex, pthread_t self)
{
  ptw32_robust_node_t** list;
  pthread_mutex_t mx = *mutex;
  ptw32_thread_t* tp = (ptw32_thread_t*)self.p;
  ptw32_robust_node_t* robust = mx->robustNode;

  list = &tp->robustMxList;
  mx->ownerThread = self;
  if (NULL == *list)
    {
      robust->prev = NULL;
      robust->next = NULL;
      *list = robust;
    }
  else
    {
      robust->prev = NULL;
      robust->next = *list;
      (*list)->prev = robust;
      *list = robust;
    }
}

void
ptw32_robust_mutex_remove(pthread_mutex_t* mutex, ptw32_thread_t* otp)
{
  ptw32_robust_node_t** list;
  pthread_mutex_t mx = *mutex;
  ptw32_robust_node_t* robust = mx->robustNode;

  list = &(((ptw32_thread_t*)mx->ownerThread.p)->robustMxList);
  mx->ownerThread.p = otp;
  if (robust->next != NULL)
    {
      robust->next->prev = robust->prev;
    }
  if (robust->prev != NULL)
    {
      robust->prev->next = robust->next;
    }
  if (*list == robust)
    {
      *list = robust->next;
    }
}


int
pthread_mutex_consistent (pthread_mutex_t* mutex)
{
  pthread_mutex_t mx = *mutex;
  int result = 0;

  /*
   * Let the system deal with invalid pointers.
   */
  if (mx == NULL)
    {
      return EINVAL;
    }

  if (mx->kind >= 0
        || (PTW32_INTERLOCKED_LONG)PTW32_ROBUST_INCONSISTENT != PTW32_INTERLOCKED_COMPARE_EXCHANGE_LONG(
                                                (PTW32_INTERLOCKED_LONGPTR)&mx->robustNode->stateInconsistent,
                                                (PTW32_INTERLOCKED_LONG)PTW32_ROBUST_CONSISTENT,
                                                (PTW32_INTERLOCKED_LONG)PTW32_ROBUST_INCONSISTENT))
    {
      result = EINVAL;
    }

  return (result);
}

