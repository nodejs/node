/*
 * pthread_cond_destroy.c
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
pthread_cond_destroy (pthread_cond_t * cond)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function destroys a condition variable
      *
      *
      * PARAMETERS
      *      cond
      *              pointer to an instance of pthread_cond_t
      *
      *
      * DESCRIPTION
      *      This function destroys a condition variable.
      *
      *      NOTES:
      *              1)      A condition variable can be destroyed
      *                      immediately after all the threads that
      *                      are blocked on it are awakened. e.g.
      *
      *                      struct list {
      *                        pthread_mutex_t lm;
      *                        ...
      *                      }
      *
      *                      struct elt {
      *                        key k;
      *                        int busy;
      *                        pthread_cond_t notbusy;
      *                        ...
      *                      }
      *
      *                      
      *                      struct elt *
      *                      list_find(struct list *lp, key k)
      *                      {
      *                        struct elt *ep;
      *
      *                        pthread_mutex_lock(&lp->lm);
      *                        while ((ep = find_elt(l,k) != NULL) && ep->busy)
      *                          pthread_cond_wait(&ep->notbusy, &lp->lm);
      *                        if (ep != NULL)
      *                          ep->busy = 1;
      *                        pthread_mutex_unlock(&lp->lm);
      *                        return(ep);
      *                      }
      *
      *                      delete_elt(struct list *lp, struct elt *ep)
      *                      {
      *                        pthread_mutex_lock(&lp->lm);
      *                        assert(ep->busy);
      *                        ... remove ep from list ...
      *                        ep->busy = 0;
      *                    (A) pthread_cond_broadcast(&ep->notbusy);
      *                        pthread_mutex_unlock(&lp->lm);
      *                    (B) pthread_cond_destroy(&rp->notbusy);
      *                        free(ep);
      *                      }
      *
      *                      In this example, the condition variable
      *                      and its list element may be freed (line B)
      *                      immediately after all threads waiting for
      *                      it are awakened (line A), since the mutex
      *                      and the code ensure that no other thread
      *                      can touch the element to be deleted.
      *
      * RESULTS
      *              0               successfully released condition variable,
      *              EINVAL          'cond' is invalid,
      *              EBUSY           'cond' is in use,
      *
      * ------------------------------------------------------
      */
{
  pthread_cond_t cv;
  int result = 0, result1 = 0, result2 = 0;

  /*
   * Assuming any race condition here is harmless.
   */
  if (cond == NULL || *cond == NULL)
    {
      return EINVAL;
    }

  if (*cond != PTHREAD_COND_INITIALIZER)
    {
      ptw32_mcs_local_node_t node;
      ptw32_mcs_lock_acquire(&ptw32_cond_list_lock, &node);

      cv = *cond;

      /*
       * Close the gate; this will synchronize this thread with
       * all already signaled waiters to let them retract their
       * waiter status - SEE NOTE 1 ABOVE!!!
       */
      if (ptw32_semwait (&(cv->semBlockLock)) != 0) /* Non-cancelable */
	{
	  result = errno;
	}
      else
        {
          /*
           * !TRY! lock mtxUnblockLock; try will detect busy condition
           * and will not cause a deadlock with respect to concurrent
           * signal/broadcast.
           */
          if ((result = pthread_mutex_trylock (&(cv->mtxUnblockLock))) != 0)
	    {
	      (void) sem_post (&(cv->semBlockLock));
	    }
	}
	
      if (result != 0)
        {
          ptw32_mcs_lock_release(&node);
          return result;
        }

      /*
       * Check whether cv is still busy (still has waiters)
       */
      if (cv->nWaitersBlocked > cv->nWaitersGone)
	{
	  if (sem_post (&(cv->semBlockLock)) != 0)
	    {
	      result = errno;
	    }
	  result1 = pthread_mutex_unlock (&(cv->mtxUnblockLock));
	  result2 = EBUSY;
	}
      else
	{
	  /*
	   * Now it is safe to destroy
	   */
	  *cond = NULL;

	  if (sem_destroy (&(cv->semBlockLock)) != 0)
	    {
	      result = errno;
	    }
	  if (sem_destroy (&(cv->semBlockQueue)) != 0)
	    {
	      result1 = errno;
	    }
	  if ((result2 = pthread_mutex_unlock (&(cv->mtxUnblockLock))) == 0)
	    {
	      result2 = pthread_mutex_destroy (&(cv->mtxUnblockLock));
	    }

	  /* Unlink the CV from the list */

	  if (ptw32_cond_list_head == cv)
	    {
	      ptw32_cond_list_head = cv->next;
	    }
	  else
	    {
	      cv->prev->next = cv->next;
	    }

	  if (ptw32_cond_list_tail == cv)
	    {
	      ptw32_cond_list_tail = cv->prev;
	    }
	  else
	    {
	      cv->next->prev = cv->prev;
	    }

	  (void) free (cv);
	}

      ptw32_mcs_lock_release(&node);
    }
  else
    {
      ptw32_mcs_local_node_t node;
      /*
       * See notes in ptw32_cond_check_need_init() above also.
       */
      ptw32_mcs_lock_acquire(&ptw32_cond_test_init_lock, &node);

      /*
       * Check again.
       */
      if (*cond == PTHREAD_COND_INITIALIZER)
	{
	  /*
	   * This is all we need to do to destroy a statically
	   * initialised cond that has not yet been used (initialised).
	   * If we get to here, another thread waiting to initialise
	   * this cond will get an EINVAL. That's OK.
	   */
	  *cond = NULL;
	}
      else
	{
	  /*
	   * The cv has been initialised while we were waiting
	   * so assume it's in use.
	   */
	  result = EBUSY;
	}

      ptw32_mcs_lock_release(&node);
    }

  return ((result != 0) ? result : ((result1 != 0) ? result1 : result2));
}
