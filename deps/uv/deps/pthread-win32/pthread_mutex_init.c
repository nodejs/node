/*
 * pthread_mutex_init.c
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

#include "pthread.h"
#include "implement.h"


int
pthread_mutex_init (pthread_mutex_t * mutex, const pthread_mutexattr_t * attr)
{
  int result = 0;
  pthread_mutex_t mx;

  if (mutex == NULL)
    {
      return EINVAL;
    }

  if (attr != NULL && *attr != NULL)
    {
      if ((*attr)->pshared == PTHREAD_PROCESS_SHARED)
        {
          /*
           * Creating mutex that can be shared between
           * processes.
           */
#if _POSIX_THREAD_PROCESS_SHARED >= 0

          /*
           * Not implemented yet.
           */

#error ERROR [__FILE__, line __LINE__]: Process shared mutexes are not supported yet.

#else

          return ENOSYS;

#endif /* _POSIX_THREAD_PROCESS_SHARED */
        }
    }

  mx = (pthread_mutex_t) calloc (1, sizeof (*mx));

  if (mx == NULL)
    {
      result = ENOMEM;
    }
  else
    {
      mx->lock_idx = 0;
      mx->recursive_count = 0;
      mx->robustNode = NULL;
      if (attr == NULL || *attr == NULL)
        {
          mx->kind = PTHREAD_MUTEX_DEFAULT;
        }
      else
        {
          mx->kind = (*attr)->kind;
          if ((*attr)->robustness == PTHREAD_MUTEX_ROBUST)
            {
              /*
               * Use the negative range to represent robust types.
               * Replaces a memory fetch with a register negate and incr
               * in pthread_mutex_lock etc.
               *
               * Map 0,1,..,n to -1,-2,..,(-n)-1
               */
              mx->kind = -mx->kind - 1;

              mx->robustNode = (ptw32_robust_node_t*) malloc(sizeof(ptw32_robust_node_t));
              mx->robustNode->stateInconsistent = PTW32_ROBUST_CONSISTENT;
              mx->robustNode->mx = mx;
              mx->robustNode->next = NULL;
              mx->robustNode->prev = NULL;
            }
        }

      mx->ownerThread.p = NULL;

      mx->event = CreateEvent (NULL, PTW32_FALSE,    /* manual reset = No */
                              PTW32_FALSE,           /* initial state = not signaled */
                              NULL);                 /* event name */

      if (0 == mx->event)
        {
          result = ENOSPC;
          free (mx);
          mx = NULL;
        }
    }

  *mutex = mx;

  return (result);
}
