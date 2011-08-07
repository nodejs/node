/*
 * pthread_attr_init.c
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
pthread_attr_init (pthread_attr_t * attr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Initializes a thread attributes object with default
      *      attributes.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_attr_t
      *
      *
      * DESCRIPTION
      *      Initializes a thread attributes object with default
      *      attributes.
      *
      *      NOTES:
      *              1)      Used to define thread attributes
      *
      * RESULTS
      *              0               successfully initialized attr,
      *              ENOMEM          insufficient memory for attr.
      *
      * ------------------------------------------------------
      */
{
  pthread_attr_t attr_result;

  if (attr == NULL)
    {
      /* This is disallowed. */
      return EINVAL;
    }

  attr_result = (pthread_attr_t) malloc (sizeof (*attr_result));

  if (attr_result == NULL)
    {
      return ENOMEM;
    }

#if defined(_POSIX_THREAD_ATTR_STACKSIZE)
  /*
   * Default to zero size. Unless changed explicitly this
   * will allow Win32 to set the size to that of the
   * main thread.
   */
  attr_result->stacksize = 0;
#endif

#if defined(_POSIX_THREAD_ATTR_STACKADDR)
  /* FIXME: Set this to something sensible when we support it. */
  attr_result->stackaddr = NULL;
#endif

  attr_result->detachstate = PTHREAD_CREATE_JOINABLE;

#if defined(HAVE_SIGSET_T)
  memset (&(attr_result->sigmask), 0, sizeof (sigset_t));
#endif /* HAVE_SIGSET_T */

  /*
   * Win32 sets new threads to THREAD_PRIORITY_NORMAL and
   * not to that of the parent thread. We choose to default to
   * this arrangement.
   */
  attr_result->param.sched_priority = THREAD_PRIORITY_NORMAL;
  attr_result->inheritsched = PTHREAD_EXPLICIT_SCHED;
  attr_result->contentionscope = PTHREAD_SCOPE_SYSTEM;

  attr_result->valid = PTW32_ATTR_VALID;

  *attr = attr_result;

  return 0;
}
