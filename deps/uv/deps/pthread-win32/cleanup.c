/*
 * cleanup.c
 *
 * Description:
 * This translation unit implements routines associated
 * with cleaning up threads.
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


/*
 * The functions ptw32_pop_cleanup and ptw32_push_cleanup
 * are implemented here for applications written in C with no
 * SEH or C++ destructor support. 
 */

ptw32_cleanup_t *
ptw32_pop_cleanup (int execute)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function pops the most recently pushed cleanup
      *      handler. If execute is nonzero, then the cleanup handler
      *      is executed if non-null.
      *
      * PARAMETERS
      *      execute
      *              if nonzero, execute the cleanup handler
      *
      *
      * DESCRIPTION
      *      This function pops the most recently pushed cleanup
      *      handler. If execute is nonzero, then the cleanup handler
      *      is executed if non-null.
      *      NOTE: specify 'execute' as nonzero to avoid duplication
      *                of common cleanup code.
      *
      * RESULTS
      *              N/A
      *
      * ------------------------------------------------------
      */
{
  ptw32_cleanup_t *cleanup;

  cleanup = (ptw32_cleanup_t *) pthread_getspecific (ptw32_cleanupKey);

  if (cleanup != NULL)
    {
      if (execute && (cleanup->routine != NULL))
	{

	  (*cleanup->routine) (cleanup->arg);

	}

      pthread_setspecific (ptw32_cleanupKey, (void *) cleanup->prev);

    }

  return (cleanup);

}				/* ptw32_pop_cleanup */


void
ptw32_push_cleanup (ptw32_cleanup_t * cleanup,
		    ptw32_cleanup_callback_t routine, void *arg)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function pushes a new cleanup handler onto the thread's stack
      *      of cleanup handlers. Each cleanup handler pushed onto the stack is
      *      popped and invoked with the argument 'arg' when
      *              a) the thread exits by calling 'pthread_exit',
      *              b) when the thread acts on a cancellation request,
      *              c) or when the thread calls pthread_cleanup_pop with a nonzero
      *                 'execute' argument
      *
      * PARAMETERS
      *      cleanup
      *              a pointer to an instance of pthread_cleanup_t,
      *
      *      routine
      *              pointer to a cleanup handler,
      *
      *      arg
      *              parameter to be passed to the cleanup handler
      *
      *
      * DESCRIPTION
      *      This function pushes a new cleanup handler onto the thread's stack
      *      of cleanup handlers. Each cleanup handler pushed onto the stack is
      *      popped and invoked with the argument 'arg' when
      *              a) the thread exits by calling 'pthread_exit',
      *              b) when the thread acts on a cancellation request,
      *              c) or when the thrad calls pthread_cleanup_pop with a nonzero
      *                 'execute' argument
      *      NOTE: pthread_push_cleanup, ptw32_pop_cleanup must be paired
      *                in the same lexical scope.
      *
      * RESULTS
      *              pthread_cleanup_t *
      *                              pointer to the previous cleanup
      *
      * ------------------------------------------------------
      */
{
  cleanup->routine = routine;
  cleanup->arg = arg;

  cleanup->prev = (ptw32_cleanup_t *) pthread_getspecific (ptw32_cleanupKey);

  pthread_setspecific (ptw32_cleanupKey, (void *) cleanup);

}				/* ptw32_push_cleanup */
