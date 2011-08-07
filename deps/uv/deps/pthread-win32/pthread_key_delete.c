/*
 * pthread_key_delete.c
 *
 * Description:
 * POSIX thread functions which implement thread-specific data (TSD).
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
pthread_key_delete (pthread_key_t key)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function deletes a thread-specific data key. This
      *      does not change the value of the thread specific data key
      *      for any thread and does not run the key's destructor
      *      in any thread so it should be used with caution.
      *
      * PARAMETERS
      *      key
      *              pointer to an instance of pthread_key_t
      *
      *
      * DESCRIPTION
      *      This function deletes a thread-specific data key. This
      *      does not change the value of the thread specific data key
      *      for any thread and does not run the key's destructor
      *      in any thread so it should be used with caution.
      *
      * RESULTS
      *              0               successfully deleted the key,
      *              EINVAL          key is invalid,
      *
      * ------------------------------------------------------
      */
{
  ptw32_mcs_local_node_t keyLock;
  int result = 0;

  if (key != NULL)
    {
      if (key->threads != NULL && key->destructor != NULL)
	{
	  ThreadKeyAssoc *assoc;
	  ptw32_mcs_lock_acquire (&(key->keyLock), &keyLock);
	  /*
	   * Run through all Thread<-->Key associations
	   * for this key.
	   *
	   * While we hold at least one of the locks guarding
	   * the assoc, we know that the assoc pointed to by
	   * key->threads is valid.
	   */
	  while ((assoc = (ThreadKeyAssoc *) key->threads) != NULL)
	    {
              ptw32_mcs_local_node_t threadLock;
	      ptw32_thread_t * thread = assoc->thread;

	      if (assoc == NULL)
		{
		  /* Finished */
		  break;
		}

	      ptw32_mcs_lock_acquire (&(thread->threadLock), &threadLock);
	      /*
	       * Since we are starting at the head of the key's threads
	       * chain, this will also point key->threads at the next assoc.
	       * While we hold key->keyLock, no other thread can insert
	       * a new assoc via pthread_setspecific.
	       */
	      ptw32_tkAssocDestroy (assoc);
	      ptw32_mcs_lock_release (&threadLock);
	      ptw32_mcs_lock_release (&keyLock);
	    }
	}

      TlsFree (key->key);
      if (key->destructor != NULL)
	{
	  /* A thread could be holding the keyLock */
	  ptw32_mcs_lock_acquire (&(key->keyLock), &keyLock);
	  ptw32_mcs_lock_release (&keyLock);
	}

#if defined( _DEBUG )
      memset ((char *) key, 0, sizeof (*key));
#endif
      free (key);
    }

  return (result);
}
