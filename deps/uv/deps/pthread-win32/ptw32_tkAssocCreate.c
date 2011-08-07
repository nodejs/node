/*
 * ptw32_tkAssocCreate.c
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


int
ptw32_tkAssocCreate (ptw32_thread_t * sp, pthread_key_t key)
     /*
      * -------------------------------------------------------------------
      * This routine creates an association that
      * is unique for the given (thread,key) combination.The association 
      * is referenced by both the thread and the key.
      * This association allows us to determine what keys the
      * current thread references and what threads a given key
      * references.
      * See the detailed description
      * at the beginning of this file for further details.
      *
      * Notes:
      *      1)      New associations are pushed to the beginning of the
      *              chain so that the internal ptw32_selfThreadKey association
      *              is always last, thus allowing selfThreadExit to
      *              be implicitly called last by pthread_exit.
      *      2)      
      *
      * Parameters:
      *              thread
      *                      current running thread.
      *              key
      *                      key on which to create an association.
      * Returns:
      *       0              - if successful,
      *       ENOMEM         - not enough memory to create assoc or other object
      *       EINVAL         - an internal error occurred
      *       ENOSYS         - an internal error occurred
      * -------------------------------------------------------------------
      */
{
  ThreadKeyAssoc *assoc;

  /*
   * Have to create an association and add it
   * to both the key and the thread.
   *
   * Both key->keyLock and thread->threadLock are locked before
   * entry to this routine.
   */
  assoc = (ThreadKeyAssoc *) calloc (1, sizeof (*assoc));

  if (assoc == NULL)
    {
      return ENOMEM;
    }

  assoc->thread = sp;
  assoc->key = key;

  /*
   * Register assoc with key
   */
  assoc->prevThread = NULL;
  assoc->nextThread = (ThreadKeyAssoc *) key->threads;
  if (assoc->nextThread != NULL)
    {
      assoc->nextThread->prevThread = assoc;
    }
  key->threads = (void *) assoc;

  /*
   * Register assoc with thread
   */
  assoc->prevKey = NULL;
  assoc->nextKey = (ThreadKeyAssoc *) sp->keys;
  if (assoc->nextKey != NULL)
    {
      assoc->nextKey->prevKey = assoc;
    }
  sp->keys = (void *) assoc;

  return (0);

}				/* ptw32_tkAssocCreate */
