/*
 * -------------------------------------------------------------
 *
 * Module: semaphore.c
 *
 * Purpose:
 *	Concatenated version of separate modules to allow
 *	inlining optimisation, which it is assumed can only
 *	be effective within a single module.
 *
 *	Semaphores aren't actually part of the PThreads standard.
 *	They are defined by the POSIX Standard:
 *
 *		POSIX 1003.1b-1993	(POSIX.1b)
 *
 * -------------------------------------------------------------
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

#if !defined(NEED_FTIME)
#  include <sys/timeb.h>
#endif

#include <limits.h>

#include "pthread.h"
#include "semaphore.h"
#include "implement.h"


#include "sem_init.c"
#include "sem_destroy.c"
#include "sem_trywait.c"
#include "sem_wait.c"
#include "sem_timedwait.c"
#include "sem_post.c"
#include "sem_post_multiple.c"
#include "sem_getvalue.c"
#include "sem_open.c"
#include "sem_close.c"
#include "sem_unlink.c"
