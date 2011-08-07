/*
 * pthread_rwlock_wrlock.c
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
pthread_rwlock_wrlock (pthread_rwlock_t * rwlock)
{
  int result;
  pthread_rwlock_t rwl;

  if (rwlock == NULL || *rwlock == NULL)
    {
      return EINVAL;
    }

  /*
   * We do a quick check to see if we need to do more work
   * to initialise a static rwlock. We check
   * again inside the guarded section of ptw32_rwlock_check_need_init()
   * to avoid race conditions.
   */
  if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
    {
      result = ptw32_rwlock_check_need_init (rwlock);

      if (result != 0 && result != EBUSY)
	{
	  return result;
	}
    }

  rwl = *rwlock;

  if (rwl->nMagic != PTW32_RWLOCK_MAGIC)
    {
      return EINVAL;
    }

  if ((result = pthread_mutex_lock (&(rwl->mtxExclusiveAccess))) != 0)
    {
      return result;
    }

  if ((result = pthread_mutex_lock (&(rwl->mtxSharedAccessCompleted))) != 0)
    {
      (void) pthread_mutex_unlock (&(rwl->mtxExclusiveAccess));
      return result;
    }

  if (rwl->nExclusiveAccessCount == 0)
    {
      if (rwl->nCompletedSharedAccessCount > 0)
	{
	  rwl->nSharedAccessCount -= rwl->nCompletedSharedAccessCount;
	  rwl->nCompletedSharedAccessCount = 0;
	}

      if (rwl->nSharedAccessCount > 0)
	{
	  rwl->nCompletedSharedAccessCount = -rwl->nSharedAccessCount;

	  /*
	   * This routine may be a cancelation point
	   * according to POSIX 1003.1j section 18.1.2.
	   */
#if defined(_MSC_VER) && _MSC_VER < 1400
#pragma inline_depth(0)
#endif
	  pthread_cleanup_push (ptw32_rwlock_cancelwrwait, (void *) rwl);

	  do
	    {
	      result = pthread_cond_wait (&(rwl->cndSharedAccessCompleted),
					  &(rwl->mtxSharedAccessCompleted));
	    }
	  while (result == 0 && rwl->nCompletedSharedAccessCount < 0);

	  pthread_cleanup_pop ((result != 0) ? 1 : 0);
#if defined(_MSC_VER) && _MSC_VER < 1400
#pragma inline_depth()
#endif

	  if (result == 0)
	    {
	      rwl->nSharedAccessCount = 0;
	    }
	}
    }

  if (result == 0)
    {
      rwl->nExclusiveAccessCount++;
    }

  return result;
}
