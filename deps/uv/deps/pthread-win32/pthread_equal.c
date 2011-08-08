/*
 * pthread_equal.c
 *
 * Description:
 * This translation unit implements miscellaneous thread functions.
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
pthread_equal (pthread_t t1, pthread_t t2)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      This function returns nonzero if t1 and t2 are equal, else
      *      returns zero
      *
      * PARAMETERS
      *      t1,
      *      t2
      *              thread IDs
      *
      *
      * DESCRIPTION
      *      This function returns nonzero if t1 and t2 are equal, else
      *      returns zero.
      *
      * RESULTS
      *              non-zero        if t1 and t2 refer to the same thread,
      *              0               t1 and t2 do not refer to the same thread
      *
      * ------------------------------------------------------
      */
{
  int result;

  /*
   * We also accept NULL == NULL - treating NULL as a thread
   * for this special case, because there is no error that we can return.
   */
  result = ( t1.p == t2.p && t1.x == t2.x );

  return (result);

}				/* pthread_equal */
