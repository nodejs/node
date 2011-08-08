/*
 * ptw32_callUserDestroyRoutines.c
 *
 * Description:
 * This translation unit implements routines which are private to
 * the implementation and may be used throughout it.
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

#if defined(__CLEANUP_CXX)
# if defined(_MSC_VER)
#  include <eh.h>
# elif defined(__WATCOMC__)
#  include <eh.h>
#  include <exceptio.h>
# else
#  if defined(__GNUC__) && __GNUC__ < 3
#    include <new.h>
#  else
#    include <new>
     using
       std::terminate;
#  endif
# endif
#endif

void
ptw32_callUserDestroyRoutines (pthread_t thread)
     /*
      * -------------------------------------------------------------------
      * DOCPRIVATE
      *
      * This the routine runs through all thread keys and calls
      * the destroy routines on the user's data for the current thread.
      * It simulates the behaviour of POSIX Threads.
      *
      * PARAMETERS
      *              thread
      *                      an instance of pthread_t
      *
      * RETURNS
      *              N/A
      * -------------------------------------------------------------------
      */
{
  ThreadKeyAssoc * assoc;

  if (thread.p != NULL)
    {
      ptw32_mcs_local_node_t threadLock;
      ptw32_mcs_local_node_t keyLock;
      int assocsRemaining;
      int iterations = 0;
      ptw32_thread_t * sp = (ptw32_thread_t *) thread.p;

      /*
       * Run through all Thread<-->Key associations
       * for the current thread.
       *
       * Do this process at most PTHREAD_DESTRUCTOR_ITERATIONS times.
       */
      do
	{
	  assocsRemaining = 0;
	  iterations++;

	  ptw32_mcs_lock_acquire(&(sp->threadLock), &threadLock);
	  /*
	   * The pointer to the next assoc is stored in the thread struct so that
	   * the assoc destructor in pthread_key_delete can adjust it
	   * if it deletes this assoc. This can happen if we fail to acquire
	   * both locks below, and are forced to release all of our locks,
	   * leaving open the opportunity for pthread_key_delete to get in
	   * before us.
	   */
	  sp->nextAssoc = sp->keys;
	  ptw32_mcs_lock_release(&threadLock);

	  for (;;)
	    {
	      void * value;
	      pthread_key_t k;
	      void (*destructor) (void *);

	      /*
	       * First we need to serialise with pthread_key_delete by locking
	       * both assoc guards, but in the reverse order to our convention,
	       * so we must be careful to avoid deadlock.
	       */
	      ptw32_mcs_lock_acquire(&(sp->threadLock), &threadLock);

	      if ((assoc = (ThreadKeyAssoc *)sp->nextAssoc) == NULL)
		{
		  /* Finished */
		  ptw32_mcs_lock_release(&threadLock);
		  break;
		}
	      else
		{
		  /*
		   * assoc->key must be valid because assoc can't change or be
		   * removed from our chain while we hold at least one lock. If
		   * the assoc was on our key chain then the key has not been
		   * deleted yet.
		   *
		   * Now try to acquire the second lock without deadlocking.
		   * If we fail, we need to relinquish the first lock and the
		   * processor and then try to acquire them all again.
		   */
		  if (ptw32_mcs_lock_try_acquire(&(assoc->key->keyLock), &keyLock) == EBUSY)
		    {
		      ptw32_mcs_lock_release(&threadLock);
		      Sleep(0);
		      /*
		       * Go around again.
		       * If pthread_key_delete has removed this assoc in the meantime,
		       * sp->nextAssoc will point to a new assoc.
		       */
		      continue;
		    }
		}

	      /* We now hold both locks */

	      sp->nextAssoc = assoc->nextKey;

	      /*
	       * Key still active; pthread_key_delete
	       * will block on these same mutexes before
	       * it can release actual key; therefore,
	       * key is valid and we can call the destroy
	       * routine;
	       */
	      k = assoc->key;
	      destructor = k->destructor;
	      value = TlsGetValue(k->key);
	      TlsSetValue (k->key, NULL);

	      // Every assoc->key exists and has a destructor
	      if (value != NULL && iterations <= PTHREAD_DESTRUCTOR_ITERATIONS)
		{
		  /*
		   * Unlock both locks before the destructor runs.
		   * POSIX says pthread_key_delete can be run from destructors,
		   * and that probably includes with this key as target.
		   * pthread_setspecific can also be run from destructors and
		   * also needs to be able to access the assocs.
		   */
		  ptw32_mcs_lock_release(&threadLock);
		  ptw32_mcs_lock_release(&keyLock);

		  assocsRemaining++;

#if defined(__cplusplus)

		  try
		    {
		      /*
		       * Run the caller's cleanup routine.
		       */
		      destructor (value);
		    }
		  catch (...)
		    {
		      /*
		       * A system unexpected exception has occurred
		       * running the user's destructor.
		       * We get control back within this block in case
		       * the application has set up it's own terminate
		       * handler. Since we are leaving the thread we
		       * should not get any internal pthreads
		       * exceptions.
		       */
		      terminate ();
		    }

#else /* __cplusplus */

		  /*
		   * Run the caller's cleanup routine.
		   */
		  destructor (value);

#endif /* __cplusplus */

		}
	      else
		{
		  /*
		   * Remove association from both the key and thread chains
		   * and reclaim it's memory resources.
		   */
		  ptw32_tkAssocDestroy (assoc);
		  ptw32_mcs_lock_release(&threadLock);
		  ptw32_mcs_lock_release(&keyLock);
		}
	    }
	}
      while (assocsRemaining);
    }
}				/* ptw32_callUserDestroyRoutines */
