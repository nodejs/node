/*
 * pthread_rwlock_unlock.c
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
pthread_rwlock_unlock (pthread_rwlock_t * rwlock)
{
  int result, result1;
  pthread_rwlock_t rwl;

  if (rwlock == NULL || *rwlock == NULL)
    {
      return (EINVAL);
    }

  if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
    {
      /*
       * Assume any race condition here is harmless.
       */
      return 0;
    }

  rwl = *rwlock;

  if (rwl->nMagic != PTW32_RWLOCK_MAGIC)
    {
      return EINVAL;
    }

  if (rwl->nExclusiveAccessCount == 0)
    {
      if ((result =
	   pthread_mutex_lock (&(rwl->mtxSharedAccessCompleted))) != 0)
	{
	  return result;
	}

      if (++rwl->nCompletedSharedAccessCount == 0)
	{
	  result = pthread_cond_signal (&(rwl->cndSharedAccessCompleted));
	}

      result1 = pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted));
    }
  else
    {
      rwl->nExclusiveAccessCount--;

      result = pthread_mutex_unlock (&(rwl->mtxSharedAccessCompleted));
      result1 = pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));

    }

  return ((result != 0) ? result : result1);
}
