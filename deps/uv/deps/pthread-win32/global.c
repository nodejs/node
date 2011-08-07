/*
 * global.c
 *
 * Description:
 * This translation unit instantiates data associated with the implementation
 * as a whole.
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


int ptw32_processInitialized = PTW32_FALSE;
ptw32_thread_t * ptw32_threadReuseTop = PTW32_THREAD_REUSE_EMPTY;
ptw32_thread_t * ptw32_threadReuseBottom = PTW32_THREAD_REUSE_EMPTY;
pthread_key_t ptw32_selfThreadKey = NULL;
pthread_key_t ptw32_cleanupKey = NULL;
pthread_cond_t ptw32_cond_list_head = NULL;
pthread_cond_t ptw32_cond_list_tail = NULL;

int ptw32_concurrency = 0;

/* What features have been auto-detected */
int ptw32_features = 0;

/*
 * Global [process wide] thread sequence Number
 */
unsigned __int64 ptw32_threadSeqNumber = 0;

/* 
 * Function pointer to QueueUserAPCEx if it exists, otherwise
 * it will be set at runtime to a substitute routine which cannot unblock
 * blocked threads.
 */
DWORD (*ptw32_register_cancelation) (PAPCFUNC, HANDLE, DWORD) = NULL;

/*
 * Global lock for managing pthread_t struct reuse.
 */
ptw32_mcs_lock_t ptw32_thread_reuse_lock = 0;

/*
 * Global lock for testing internal state of statically declared mutexes.
 */
ptw32_mcs_lock_t ptw32_mutex_test_init_lock = 0;

/*
 * Global lock for testing internal state of PTHREAD_COND_INITIALIZER
 * created condition variables.
 */
ptw32_mcs_lock_t ptw32_cond_test_init_lock = 0;

/*
 * Global lock for testing internal state of PTHREAD_RWLOCK_INITIALIZER
 * created read/write locks.
 */
ptw32_mcs_lock_t ptw32_rwlock_test_init_lock = 0;

/*
 * Global lock for testing internal state of PTHREAD_SPINLOCK_INITIALIZER
 * created spin locks.
 */
ptw32_mcs_lock_t ptw32_spinlock_test_init_lock = 0;

/*
 * Global lock for condition variable linked list. The list exists
 * to wake up CVs when a WM_TIMECHANGE message arrives. See
 * w32_TimeChangeHandler.c.
 */
ptw32_mcs_lock_t ptw32_cond_list_lock = 0;

#if defined(_UWIN)
/*
 * Keep a count of the number of threads.
 */
int pthread_count = 0;
#endif
