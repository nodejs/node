/*
 * pthread_setspecific.c
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
pthread_setspecific (pthread_key_t key, const void *value)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function sets the value of the thread specific
      *      key in the calling thread.
      *
      * PARAMETERS
      *      key
      *              an instance of pthread_key_t
      *      value
      *              the value to set key to
      *
      *
      * DESCRIPTION
      *      This function sets the value of the thread specific
      *      key in the calling thread.
      *
      * RESULTS
      *              0               successfully set value
      *              EAGAIN          could not set value
      *              ENOENT          SERIOUS!!
      *
      * ------------------------------------------------------
      */
{
  pthread_t self;
  int result = 0;

  if (key != ptw32_selfThreadKey)
    {
      /*
       * Using pthread_self will implicitly create
       * an instance of pthread_t for the current
       * thread if one wasn't explicitly created
       */
      self = pthread_self ();
      if (self.p == NULL)
	{
	  return ENOENT;
	}
    }
  else
    {
      /*
       * Resolve catch-22 of registering thread with selfThread
       * key
       */
      ptw32_thread_t * sp = (ptw32_thread_t *) pthread_getspecific (ptw32_selfThreadKey);

      if (sp == NULL)
        {
	  if (value == NULL)
	    {
	      return ENOENT;
	    }
          self = *((pthread_t *) value);
        }
      else
        {
	  self = sp->ptHandle;
        }
    }

  result = 0;

  if (key != NULL)
    {
      if (self.p != NULL && key->destructor != NULL && value != NULL)
	{
          ptw32_mcs_local_node_t keyLock;
          ptw32_mcs_local_node_t threadLock;
	  ptw32_thread_t * sp = (ptw32_thread_t *) self.p;
	  /*
	   * Only require associations if we have to
	   * call user destroy routine.
	   * Don't need to locate an existing association
	   * when setting data to NULL for WIN32 since the
	   * data is stored with the operating system; not
	   * on the association; setting assoc to NULL short
	   * circuits the search.
	   */
	  ThreadKeyAssoc *assoc;

	  ptw32_mcs_lock_acquire(&(key->keyLock), &keyLock);
	  ptw32_mcs_lock_acquire(&(sp->threadLock), &threadLock);

	  assoc = (ThreadKeyAssoc *) sp->keys;
	  /*
	   * Locate existing association
	   */
	  while (assoc != NULL)
	    {
	      if (assoc->key == key)
		{
		  /*
		   * Association already exists
		   */
		  break;
		}
		assoc = assoc->nextKey;
	    }

	  /*
	   * create an association if not found
	   */
	  if (assoc == NULL)
	    {
	      result = ptw32_tkAssocCreate (sp, key);
	    }

	  ptw32_mcs_lock_release(&threadLock);
	  ptw32_mcs_lock_release(&keyLock);
	}

      if (result == 0)
	{
	  if (!TlsSetValue (key->key, (LPVOID) value))
	    {
	      result = EAGAIN;
	    }
	}
    }

  return (result);
}				/* pthread_setspecific */
