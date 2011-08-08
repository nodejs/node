/*
 * pthread_attr_setstacksize.c
 *
 * Description:
 * This translation unit implements operations on thread attribute objects.
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
pthread_attr_setstacksize (pthread_attr_t * attr, size_t stacksize)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function specifies the size of the stack on
      *      which threads created with 'attr' will run.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_attr_t
      *
      *      stacksize
      *              stack size, in bytes.
      *
      *
      * DESCRIPTION
      *      This function specifies the size of the stack on
      *      which threads created with 'attr' will run.
      *
      *      NOTES:
      *              1)      Function supported only if this macro is
      *                      defined:
      *
      *                              _POSIX_THREAD_ATTR_STACKSIZE
      *
      *              2)      Find the default first (using
      *                      pthread_attr_getstacksize), then increase
      *                      by multiplying.
      *
      *              3)      Only use if thread needs more than the
      *                      default.
      *
      * RESULTS
      *              0               successfully set stack size,
      *              EINVAL          'attr' is invalid or stacksize too
      *                              small or too big.
      *              ENOSYS          function not supported
      *
      * ------------------------------------------------------
      */
{
#if defined(_POSIX_THREAD_ATTR_STACKSIZE)

#if PTHREAD_STACK_MIN > 0

  /*  Verify that the stack size is within range. */
  if (stacksize < PTHREAD_STACK_MIN)
    {
      return EINVAL;
    }

#endif

  if (ptw32_is_attr (attr) != 0)
    {
      return EINVAL;
    }

  /* Everything is okay. */
  (*attr)->stacksize = stacksize;
  return 0;

#else

  return ENOSYS;

#endif /* _POSIX_THREAD_ATTR_STACKSIZE */

}
