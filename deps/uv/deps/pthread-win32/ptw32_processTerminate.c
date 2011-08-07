/*
 * ptw32_processTerminate.c
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


void
ptw32_processTerminate (void)
     /*
      * ------------------------------------------------------
      * DOCPRIVATE
      *      This function performs process wide termination for
      *      the pthread library.
      *
      * PARAMETERS
      *      N/A
      *
      * DESCRIPTION
      *      This function performs process wide termination for
      *      the pthread library.
      *      This routine sets the global variable
      *      ptw32_processInitialized to FALSE
      *
      * RESULTS
      *              N/A
      *
      * ------------------------------------------------------
      */
{
  if (ptw32_processInitialized)
    {
      ptw32_thread_t * tp, * tpNext;
      ptw32_mcs_local_node_t node;

      if (ptw32_selfThreadKey != NULL)
	{
	  /*
	   * Release ptw32_selfThreadKey
	   */
	  pthread_key_delete (ptw32_selfThreadKey);

	  ptw32_selfThreadKey = NULL;
	}

      if (ptw32_cleanupKey != NULL)
	{
	  /*
	   * Release ptw32_cleanupKey
	   */
	  pthread_key_delete (ptw32_cleanupKey);

	  ptw32_cleanupKey = NULL;
	}

      ptw32_mcs_lock_acquire(&ptw32_thread_reuse_lock, &node);

      tp = ptw32_threadReuseTop;
      while (tp != PTW32_THREAD_REUSE_EMPTY)
	{
	  tpNext = tp->prevReuse;
	  free (tp);
	  tp = tpNext;
	}

      ptw32_mcs_lock_release(&node);

      ptw32_processInitialized = PTW32_FALSE;
    }

}				/* processTerminate */
