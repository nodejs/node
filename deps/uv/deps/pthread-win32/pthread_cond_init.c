/*
 * pthread_cond_init.c
 *
 * Description:
 * This translation unit implements condition variables and their primitives.
 *
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
pthread_cond_init (pthread_cond_t * cond, const pthread_condattr_t * attr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function initializes a condition variable.
      *
      * PARAMETERS
      *      cond
      *              pointer to an instance of pthread_cond_t
      *
      *      attr
      *              specifies optional creation attributes.
      *
      *
      * DESCRIPTION
      *      This function initializes a condition variable.
      *
      * RESULTS
      *              0               successfully created condition variable,
      *              EINVAL          'attr' is invalid,
      *              EAGAIN          insufficient resources (other than
      *                              memory,
      *              ENOMEM          insufficient memory,
      *              EBUSY           'cond' is already initialized,
      *
      * ------------------------------------------------------
      */
{
  int result;
  pthread_cond_t cv = NULL;

  if (cond == NULL)
    {
      return EINVAL;
    }

  if ((attr != NULL && *attr != NULL) &&
      ((*attr)->pshared == PTHREAD_PROCESS_SHARED))
    {
      /*
       * Creating condition variable that can be shared between
       * processes.
       */
      result = ENOSYS;
      goto DONE;
    }

  cv = (pthread_cond_t) calloc (1, sizeof (*cv));

  if (cv == NULL)
    {
      result = ENOMEM;
      goto DONE;
    }

  cv->nWaitersBlocked = 0;
  cv->nWaitersToUnblock = 0;
  cv->nWaitersGone = 0;

  if (sem_init (&(cv->semBlockLock), 0, 1) != 0)
    {
      result = errno;
      goto FAIL0;
    }

  if (sem_init (&(cv->semBlockQueue), 0, 0) != 0)
    {
      result = errno;
      goto FAIL1;
    }

  if ((result = pthread_mutex_init (&(cv->mtxUnblockLock), 0)) != 0)
    {
      goto FAIL2;
    }

  result = 0;

  goto DONE;

  /*
   * -------------
   * Failed...
   * -------------
   */
FAIL2:
  (void) sem_destroy (&(cv->semBlockQueue));

FAIL1:
  (void) sem_destroy (&(cv->semBlockLock));

FAIL0:
  (void) free (cv);
  cv = NULL;

DONE:
  if (0 == result)
    {
      ptw32_mcs_local_node_t node;

      ptw32_mcs_lock_acquire(&ptw32_cond_list_lock, &node);

      cv->next = NULL;
      cv->prev = ptw32_cond_list_tail;

      if (ptw32_cond_list_tail != NULL)
	{
	  ptw32_cond_list_tail->next = cv;
	}

      ptw32_cond_list_tail = cv;

      if (ptw32_cond_list_head == NULL)
	{
	  ptw32_cond_list_head = cv;
	}

      ptw32_mcs_lock_release(&node);
    }

  *cond = cv;

  return result;

}				/* pthread_cond_init */
