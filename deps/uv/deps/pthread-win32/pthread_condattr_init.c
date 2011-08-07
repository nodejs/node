/*
 * pthread_condattr_init.c
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
pthread_condattr_init (pthread_condattr_t * attr)
     /*
      * ------------------------------------------------------
      * DOCPUBLIC
      *      Initializes a condition variable attributes object
      *      with default attributes.
      *
      * PARAMETERS
      *      attr
      *              pointer to an instance of pthread_condattr_t
      *
      *
      * DESCRIPTION
      *      Initializes a condition variable attributes object
      *      with default attributes.
      *
      *      NOTES:
      *              1)      Use to define condition variable types
      *              2)      It is up to the application to ensure
      *                      that it doesn't re-init an attribute
      *                      without destroying it first. Otherwise
      *                      a memory leak is created.
      *
      * RESULTS
      *              0               successfully initialized attr,
      *              ENOMEM          insufficient memory for attr.
      *
      * ------------------------------------------------------
      */
{
  pthread_condattr_t attr_result;
  int result = 0;

  attr_result = (pthread_condattr_t) calloc (1, sizeof (*attr_result));

  if (attr_result == NULL)
    {
      result = ENOMEM;
    }

  *attr = attr_result;

  return result;

}				/* pthread_condattr_init */
