/*
 * ptw32_processInitialize.c
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
ptw32_processInitialize (void)
     /*
      * ------------------------------------------------------
      * DOCPRIVATE
      *      This function performs process wide initialization for
      *      the pthread library.
      *
      * PARAMETERS
      *      N/A
      *
      * DESCRIPTION
      *      This function performs process wide initialization for
      *      the pthread library.
      *      If successful, this routine sets the global variable
      *      ptw32_processInitialized to TRUE.
      *
      * RESULTS
      *              TRUE    if successful,
      *              FALSE   otherwise
      *
      * ------------------------------------------------------
      */
{
  if (ptw32_processInitialized)
    {
      /* 
       * Ignore if already initialized. this is useful for 
       * programs that uses a non-dll pthread
       * library. Such programs must call ptw32_processInitialize() explicitly,
       * since this initialization routine is automatically called only when
       * the dll is loaded.
       */
      return PTW32_TRUE;
    }

  ptw32_processInitialized = PTW32_TRUE;

  /*
   * Initialize Keys
   */
  if ((pthread_key_create (&ptw32_selfThreadKey, NULL) != 0) ||
      (pthread_key_create (&ptw32_cleanupKey, NULL) != 0))
    {

      ptw32_processTerminate ();
    }

  return (ptw32_processInitialized);

}				/* processInitialize */
