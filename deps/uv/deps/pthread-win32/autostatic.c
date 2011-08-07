/*
 * autostatic.c
 *
 * Description:
 * This translation unit implements static auto-init and auto-exit logic.
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

#if defined(PTW32_STATIC_LIB)

#if defined(__MINGW64__) || defined(__MINGW32__) || defined(_MSC_VER)

#include "pthread.h"
#include "implement.h"

static void on_process_init(void)
{
    pthread_win32_process_attach_np ();
}

static void on_process_exit(void)
{
    pthread_win32_thread_detach_np  ();
    pthread_win32_process_detach_np ();
}

#if defined(__MINGW64__) || defined(__MINGW32__)
# define attribute_section(a) __attribute__((section(a)))
#elif defined(_MSC_VER)
# define attribute_section(a) __pragma(section(a,long,read)); __declspec(allocate(a))
#endif

attribute_section(".ctors") void *gcc_ctor = on_process_init;
attribute_section(".dtors") void *gcc_dtor = on_process_exit;

attribute_section(".CRT$XCU") void *msc_ctor = on_process_init;
attribute_section(".CRT$XPU") void *msc_dtor = on_process_exit;

#endif /* defined(__MINGW64__) || defined(__MINGW32__) || defined(_MSC_VER) */

#endif /* PTW32_STATIC_LIB */
