/*
 * pthread_rwlock_destroy.c
 *
 * Description:
 * This translation unit implements read/write lock primitives.
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

#include <limits.h>

#include "pthread.h"
#include "implement.h"

int
pthread_rwlock_destroy (pthread_rwlock_t * rwlock)
{
  pthread_rwlock_t rwl;
  int result = 0, result1 = 0, result2 = 0;

  if (rwlock == NULL || *rwlock == NULL)
    {
      return EINVAL;
    }

  if (*rwlock != PTHREAD_RWLOCK_INITIALIZER)
    {
      rwl = *rwlock;

      if (rwl->nMagic != PTW32_RWLOCK_MAGIC)
	{
	  return EINVAL;
	}

      if ((result = pthread_mutex_lock (&(rwl->mtxExclusiveAccess))) != 0)
	{
	  return result;
	}

      if ((result =
	   pthread_mutex_lock (&(rwl->mtxSharedAccessCompleted))) != 0)
	{
	  (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
	  return result;
	}

      /*
       * Check whether any threads own/wait for the lock (wait for ex.access);
       * report "BUSY" if so.
       */
      if (rwl->nExclusiveAccessCount > 0
	  || rwl->nSharedAccessCount > rwl->nCompletedSharedAccessCount)
	{
	  result = pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted));
	  result1 = pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
	  result2 = EBUSY;
	}
      else
	{
	  rwl->nMagic = 0;

	  if ((result =
	       pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted))) != 0)
	    {
	      pthread_mutex_unlock (&rwl->mtxExclusiveAccess);
	      return result;
	    }

	  if ((result =
	       pthread_mutex_unlock (&(rwl->mtxExclusiveAccess))) != 0)
	    {
	      return result;
	    }

	  *rwlock = NULL;	/* Invalidate rwlock before anything else */
	  result = pthread_cond_destroy (&(rwl->cndSharedAccessCompleted));
	  result1 = pthread_mutex_destroy (&(rwl->mtxSharedAccessCompleted));
	  result2 = pthread_mutex_destroy (&(rwl->mtxExclusiveAccess));
	  (void) free (rwl);
	}
    }
  else
    {
      ptw32_mcs_local_node_t node;
      /*
       * See notes in ptw32_rwlock_check_need_init() above also.
       */
      ptw32_mcs_lock_acquire(&ptw32_rwlock_test_init_lock, &node);

      /*
       * Check again.
       */
      if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
	{
	  /*
	   * This is all we need to do to destroy a statically
	   * initialised rwlock that has not yet been used (initialised).
	   * If we get to here, another thread
	   * waiting to initialise this rwlock will get an EINVAL.
	   */
	  *rwlock = NULL;
	}
      else
	{
	  /*
	   * The rwlock has been initialised while we were waiting
	   * so assume it's in use.
	   */
	  result = EBUSY;
	}

      ptw32_mcs_lock_release(&node);
    }

  return ((result != 0) ? result : ((result1 != 0) ? result1 : result2));
}
