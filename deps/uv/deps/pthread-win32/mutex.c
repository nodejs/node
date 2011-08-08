/*
 * mutex.c
 *
 * Description:
 * This translation unit implements mutual exclusion (mutex) primitives.
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

#if ! defined(_UWIN) && ! defined(WINCE)
#   include <process.h>
#endif
#if !defined(NEED_FTIME)
#include <sys/timeb.h>
#endif
#include "pthread.h"
#include "implement.h"


#include "ptw32_mutex_check_need_init.c"
#include "pthread_mutex_init.c"
#include "pthread_mutex_destroy.c"
#include "pthread_mutexattr_init.c"
#include "pthread_mutexattr_destroy.c"
#include "pthread_mutexattr_getpshared.c"
#include "pthread_mutexattr_setpshared.c"
#include "pthread_mutexattr_settype.c"
#include "pthread_mutexattr_gettype.c"
#include "pthread_mutexattr_setrobust.c"
#include "pthread_mutexattr_getrobust.c"
#include "pthread_mutex_lock.c"
#include "pthread_mutex_timedlock.c"
#include "pthread_mutex_unlock.c"
#include "pthread_mutex_trylock.c"
#include "pthread_mutex_consistent.c"
