/*
 * pthread_win32_attach_detach_np.c
 *
 * Description:
 * This translation unit implements non-portable thread functions.
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

#include <string.h>
#include <tchar.h>

/*
 * Handle to quserex.dll 
 */
static HINSTANCE ptw32_h_quserex;

BOOL
pthread_win32_process_attach_np ()
{
  TCHAR QuserExDLLPathBuf[1024];
  BOOL result = TRUE;

  result = ptw32_processInitialize ();

#if defined(_UWIN)
  pthread_count++;
#endif

#if defined(__GNUC__)
  ptw32_features = 0;
#else
  /*
   * This is obsolete now.
   */
  ptw32_features = PTW32_SYSTEM_INTERLOCKED_COMPARE_EXCHANGE;
#endif

  /*
   * Load QUSEREX.DLL and try to get address of QueueUserAPCEx.
   * Because QUSEREX.DLL requires a driver to be installed we will
   * assume the DLL is in the system directory.
   *
   * This should take care of any security issues.
   */
#if defined(__GNUC__) || _MSC_VER < 1400
  if(GetSystemDirectory(QuserExDLLPathBuf, sizeof(QuserExDLLPathBuf) / sizeof(*QuserExDLLPathBuf)))
  {
    _tcsncat(QuserExDLLPathBuf,
             TEXT("\\QUSEREX.DLL"),
             (sizeof(QuserExDLLPathBuf) / sizeof(*QuserExDLLPathBuf)) - _tcslen(QuserExDLLPathBuf) - 1);
    ptw32_h_quserex = LoadLibrary(QuserExDLLPathBuf);
  }
#else
  /* strncat is secure - this is just to avoid a warning */
  if(GetSystemDirectory(QuserExDLLPathBuf, sizeof(QuserExDLLPathBuf) / sizeof(*QuserExDLLPathBuf)) &&
     0 == _tcsncat_s(QuserExDLLPathBuf, sizeof(QuserExDLLPathBuf) / sizeof(*QuserExDLLPathBuf), TEXT("\\QUSEREX.DLL"), 12))
  {
    ptw32_h_quserex = LoadLibrary(QuserExDLLPathBuf);
  }
#endif

  if (ptw32_h_quserex != NULL)
    {
      ptw32_register_cancelation = (DWORD (*)(PAPCFUNC, HANDLE, DWORD))
#if defined(NEED_UNICODE_CONSTS)
      GetProcAddress (ptw32_h_quserex, (const TCHAR *) TEXT ("QueueUserAPCEx"));
#else
      GetProcAddress (ptw32_h_quserex, "QueueUserAPCEx");
#endif
    }

  if (NULL == ptw32_register_cancelation)
    {
      ptw32_register_cancelation = ptw32_RegisterCancelation;

      if (ptw32_h_quserex != NULL)
      {
        (void) FreeLibrary (ptw32_h_quserex);
      }
      ptw32_h_quserex = 0;
    }
  else
    {
      /* Initialise QueueUserAPCEx */
      BOOL (*queue_user_apc_ex_init) (VOID);

      queue_user_apc_ex_init = (BOOL (*)(VOID))
#if defined(NEED_UNICODE_CONSTS)
	GetProcAddress (ptw32_h_quserex,
			(const TCHAR *) TEXT ("QueueUserAPCEx_Init"));
#else
	GetProcAddress (ptw32_h_quserex, "QueueUserAPCEx_Init");
#endif

      if (queue_user_apc_ex_init == NULL || !queue_user_apc_ex_init ())
	{
	  ptw32_register_cancelation = ptw32_RegisterCancelation;

	  (void) FreeLibrary (ptw32_h_quserex);
	  ptw32_h_quserex = 0;
	}
    }

  if (ptw32_h_quserex)
    {
      ptw32_features |= PTW32_ALERTABLE_ASYNC_CANCEL;
    }

  return result;
}


BOOL
pthread_win32_process_detach_np ()
{
  if (ptw32_processInitialized)
    {
      ptw32_thread_t * sp = (ptw32_thread_t *) pthread_getspecific (ptw32_selfThreadKey);

      if (sp != NULL)
	{
	  /*
	   * Detached threads have their resources automatically
	   * cleaned up upon exit (others must be 'joined').
	   */
	  if (sp->detachState == PTHREAD_CREATE_DETACHED)
	    {
	      ptw32_threadDestroy (sp->ptHandle);
	      TlsSetValue (ptw32_selfThreadKey->key, NULL);
	    }
	}

      /*
       * The DLL is being unmapped from the process's address space
       */
      ptw32_processTerminate ();

      if (ptw32_h_quserex)
	{
	  /* Close QueueUserAPCEx */
	  BOOL (*queue_user_apc_ex_fini) (VOID);

	  queue_user_apc_ex_fini = (BOOL (*)(VOID))
#if defined(NEED_UNICODE_CONSTS)
	    GetProcAddress (ptw32_h_quserex,
			    (const TCHAR *) TEXT ("QueueUserAPCEx_Fini"));
#else
	    GetProcAddress (ptw32_h_quserex, "QueueUserAPCEx_Fini");
#endif

	  if (queue_user_apc_ex_fini != NULL)
	    {
	      (void) queue_user_apc_ex_fini ();
	    }
	  (void) FreeLibrary (ptw32_h_quserex);
	}
    }

  return TRUE;
}

BOOL
pthread_win32_thread_attach_np ()
{
  return TRUE;
}

BOOL
pthread_win32_thread_detach_np ()
{
  if (ptw32_processInitialized)
    {
      /*
       * Don't use pthread_self() - to avoid creating an implicit POSIX thread handle
       * unnecessarily.
       */
      ptw32_thread_t * sp = (ptw32_thread_t *) pthread_getspecific (ptw32_selfThreadKey);

      if (sp != NULL) // otherwise Win32 thread with no implicit POSIX handle.
	{
          ptw32_mcs_local_node_t stateLock;
	  ptw32_callUserDestroyRoutines (sp->ptHandle);

	  ptw32_mcs_lock_acquire (&sp->stateLock, &stateLock);
	  sp->state = PThreadStateLast;
	  /*
	   * If the thread is joinable at this point then it MUST be joined
	   * or detached explicitly by the application.
	   */
	  ptw32_mcs_lock_release (&stateLock);

          /*
           * Robust Mutexes
           */
          while (sp->robustMxList != NULL)
            {
              pthread_mutex_t mx = sp->robustMxList->mx;
              ptw32_robust_mutex_remove(&mx, sp);
              (void) PTW32_INTERLOCKED_EXCHANGE_LONG(
                       (PTW32_INTERLOCKED_LONGPTR)&mx->robustNode->stateInconsistent,
                       (PTW32_INTERLOCKED_LONG)-1);
              /*
               * If there are no waiters then the next thread to block will
               * sleep, wakeup immediately and then go back to sleep.
               * See pthread_mutex_lock.c.
               */
              SetEvent(mx->event);
            }


	  if (sp->detachState == PTHREAD_CREATE_DETACHED)
	    {
	      ptw32_threadDestroy (sp->ptHandle);

	      TlsSetValue (ptw32_selfThreadKey->key, NULL);
	    }
	}
    }

  return TRUE;
}

BOOL
pthread_win32_test_features_np (int feature_mask)
{
  return ((ptw32_features & feature_mask) == feature_mask);
}
