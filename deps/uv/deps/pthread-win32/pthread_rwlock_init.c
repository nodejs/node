/*
 * pthread_rwlock_init.c
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
pthread_rwlock_init (pthread_rwlock_t * rwlock,
		     const pthread_rwlockattr_t * attr)
{
  int result;
  pthread_rwlock_t rwl = 0;

  if (rwlock == NULL)
    {
      return EINVAL;
    }

  if (attr != NULL && *attr != NULL)
    {
      result = EINVAL;		/* Not supported */
      goto DONE;
    }

  rwl = (pthread_rwlock_t) calloc (1, sizeof (*rwl));

  if (rwl == NULL)
    {
      result = ENOMEM;
      goto DONE;
    }

  rwl->nSharedAccessCount = 0;
  rwl->nExclusiveAccessCount = 0;
  rwl->nCompletedSharedAccessCount = 0;

  result = pthread_mutex_init (&rwl->mtxExclusiveAccess, NULL);
  if (result != 0)
    {
      goto FAIL0;
    }

  result = pthread_mutex_init (&rwl->mtxSharedAccessCompleted, NULL);
  if (result != 0)
    {
      goto FAIL1;
    }

  result = pthread_cond_init (&rwl->cndSharedAccessCompleted, NULL);
  if (result != 0)
    {
      goto FAIL2;
    }

  rwl->nMagic = PTW32_RWLOCK_MAGIC;

  result = 0;
  goto DONE;

FAIL2:
  (void) pthread_mutex_destroy (&(rwl->mtxSharedAccessCompleted));

FAIL1:
  (void) pthread_mutex_destroy (&(rwl->mtxExclusiveAccess));

FAIL0:
  (void) free (rwl);
  rwl = NULL;

DONE:
  *rwlock = rwl;

  return result;
}
