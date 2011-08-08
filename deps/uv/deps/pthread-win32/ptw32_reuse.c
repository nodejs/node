/*
 * ptw32_threadReuse.c
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


/*
 * How it works:
 * A pthread_t is a struct (2x32 bit scalar types on IA-32, 2x64 bit on IA-64)
 * which is normally passed/returned by value to/from pthreads routines.
 * Applications are therefore storing a copy of the struct as it is at that
 * time.
 *
 * The original pthread_t struct plus all copies of it contain the address of
 * the thread state struct ptw32_thread_t_ (p), plus a reuse counter (x). Each
 * ptw32_thread_t contains the original copy of it's pthread_t.
 * Once malloced, a ptw32_thread_t_ struct is not freed until the process exits.
 * 
 * The thread reuse stack is a simple LILO stack managed through a singly
 * linked list element in the ptw32_thread_t.
 *
 * Each time a thread is destroyed, the ptw32_thread_t address is pushed onto the
 * reuse stack after it's ptHandle's reuse counter has been incremented.
 * 
 * The following can now be said from this:
 * - two pthread_t's are identical if their ptw32_thread_t reference pointers
 * are equal and their reuse counters are equal. That is,
 *
 *   equal = (a.p == b.p && a.x == b.x)
 *
 * - a pthread_t copy refers to a destroyed thread if the reuse counter in
 * the copy is not equal to the reuse counter in the original.
 *
 *   threadDestroyed = (copy.x != ((ptw32_thread_t *)copy.p)->ptHandle.x)
 *
 */

/*
 * Pop a clean pthread_t struct off the reuse stack.
 */
pthread_t
ptw32_threadReusePop (void)
{
  pthread_t t = {NULL, 0};
  ptw32_mcs_local_node_t node;

  ptw32_mcs_lock_acquire(&ptw32_thread_reuse_lock, &node);

  if (PTW32_THREAD_REUSE_EMPTY != ptw32_threadReuseTop)
    {
      ptw32_thread_t * tp;

      tp = ptw32_threadReuseTop;

      ptw32_threadReuseTop = tp->prevReuse;

      if (PTW32_THREAD_REUSE_EMPTY == ptw32_threadReuseTop)
        {
          ptw32_threadReuseBottom = PTW32_THREAD_REUSE_EMPTY;
        }

      tp->prevReuse = NULL;

      t = tp->ptHandle;
    }

  ptw32_mcs_lock_release(&node);

  return t;

}

/*
 * Push a clean pthread_t struct onto the reuse stack.
 * Must be re-initialised when reused.
 * All object elements (mutexes, events etc) must have been either
 * detroyed before this, or never initialised.
 */
void
ptw32_threadReusePush (pthread_t thread)
{
  ptw32_thread_t * tp = (ptw32_thread_t *) thread.p;
  pthread_t t;
  ptw32_mcs_local_node_t node;

  ptw32_mcs_lock_acquire(&ptw32_thread_reuse_lock, &node);

  t = tp->ptHandle;
  memset(tp, 0, sizeof(ptw32_thread_t));

  /* Must restore the original POSIX handle that we just wiped. */
  tp->ptHandle = t;

  /* Bump the reuse counter now */
#if defined(PTW32_THREAD_ID_REUSE_INCREMENT)
  tp->ptHandle.x += PTW32_THREAD_ID_REUSE_INCREMENT;
#else
  tp->ptHandle.x++;
#endif

  tp->state = PThreadStateReuse;

  tp->prevReuse = PTW32_THREAD_REUSE_EMPTY;

  if (PTW32_THREAD_REUSE_EMPTY != ptw32_threadReuseBottom)
    {
      ptw32_threadReuseBottom->prevReuse = tp;
    }
  else
    {
      ptw32_threadReuseTop = tp;
    }

  ptw32_threadReuseBottom = tp;

  ptw32_mcs_lock_release(&node);
}
