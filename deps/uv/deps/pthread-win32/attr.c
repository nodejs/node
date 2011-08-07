/*
 * attr.c
 *
 * Description:
 * This translation unit agregates operations on thread attribute objects.
 * It is used for inline optimisation.
 *
 * The included modules are used separately when static executable sizes
 * must be minimised.
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

#include "pthread_attr_init.c"
#include "pthread_attr_destroy.c"
#include "pthread_attr_getdetachstate.c"
#include "pthread_attr_setdetachstate.c"
#include "pthread_attr_getstackaddr.c"
#include "pthread_attr_setstackaddr.c"
#include "pthread_attr_getstacksize.c"
#include "pthread_attr_setstacksize.c"
#include "pthread_attr_getscope.c"
#include "pthread_attr_setscope.c"
