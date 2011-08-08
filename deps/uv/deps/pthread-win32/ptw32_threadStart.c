/*
 * ptw32_threadStart.c
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
#include <stdio.h>

#if defined(__CLEANUP_C)
# include <setjmp.h>
#endif

#if defined(__CLEANUP_SEH)

static DWORD
ExceptionFilter (EXCEPTION_POINTERS * ep, DWORD * ei)
{
  switch (ep->ExceptionRecord->ExceptionCode)
    {
    case EXCEPTION_PTW32_SERVICES:
      {
	DWORD param;
	DWORD numParams = ep->ExceptionRecord->NumberParameters;

	numParams = (numParams > 3) ? 3 : numParams;

	for (param = 0; param < numParams; param++)
	  {
	    ei[param] = ep->ExceptionRecord->ExceptionInformation[param];
	  }

	return EXCEPTION_EXECUTE_HANDLER;
	break;
      }
    default:
      {
	/*
	 * A system unexpected exception has occurred running the user's
	 * routine. We need to cleanup before letting the exception
	 * out of thread scope.
	 */
	pthread_t self = pthread_self ();

	ptw32_callUserDestroyRoutines (self);

	return EXCEPTION_CONTINUE_SEARCH;
	break;
      }
    }
}

#elif defined(__CLEANUP_CXX)

#if defined(_MSC_VER)
# include <eh.h>
#elif defined(__WATCOMC__)
# include <eh.h>
# include <exceptio.h>
typedef terminate_handler
  terminate_function;
#else
# if defined(__GNUC__) && __GNUC__ < 3
#   include <new.h>
# else
#   include <new>
using
  std::terminate_handler;
using
  std::terminate;
using
  std::set_terminate;
# endif
typedef terminate_handler
  terminate_function;
#endif

static terminate_function
  ptw32_oldTerminate;

void
ptw32_terminate ()
{
  set_terminate (ptw32_oldTerminate);
  (void) pthread_win32_thread_detach_np ();
  terminate ();
}

#endif

#if ! (defined(__MINGW64__) || defined(__MINGW32__)) || (defined (__MSVCRT__) && ! defined (__DMC__))
unsigned
  __stdcall
#else
void
#endif
ptw32_threadStart (void *vthreadParms)
{
  ThreadParms * threadParms = (ThreadParms *) vthreadParms;
  pthread_t self;
  ptw32_thread_t * sp;
  void *(*start) (void *);
  void * arg;

#if defined(__CLEANUP_SEH)
  DWORD
  ei[] = { 0, 0, 0 };
#endif

#if defined(__CLEANUP_C)
  int setjmp_rc;
#endif

  ptw32_mcs_local_node_t stateLock;
  void * status = (void *) 0;

  self = threadParms->tid;
  sp = (ptw32_thread_t *) self.p;
  start = threadParms->start;
  arg = threadParms->arg;

  free (threadParms);

#if (defined(__MINGW64__) || defined(__MINGW32__)) && ! defined (__MSVCRT__)
  /*
   * beginthread does not return the thread id and is running
   * before it returns us the thread handle, and so we do it here.
   */
  sp->thread = GetCurrentThreadId ();
  /*
   * Here we're using stateLock as a general-purpose lock
   * to make the new thread wait until the creating thread
   * has the new handle.
   */
  ptw32_mcs_lock_acquire (&sp->stateLock, &stateLock);
  pthread_setspecific (ptw32_selfThreadKey, sp);
#else
  pthread_setspecific (ptw32_selfThreadKey, sp);
  ptw32_mcs_lock_acquire (&sp->stateLock, &stateLock);
#endif

  sp->state = PThreadStateRunning;
  ptw32_mcs_lock_release (&stateLock);

#if defined(__CLEANUP_SEH)

  __try
  {
    /*
     * Run the caller's routine;
     */
    status = sp->exitStatus = (*start) (arg);
    sp->state = PThreadStateExiting;

#if defined(_UWIN)
    if (--pthread_count <= 0)
      exit (0);
#endif

  }
  __except (ExceptionFilter (GetExceptionInformation (), ei))
  {
    switch (ei[0])
      {
      case PTW32_EPS_CANCEL:
	status = sp->exitStatus = PTHREAD_CANCELED;
#if defined(_UWIN)
	if (--pthread_count <= 0)
	  exit (0);
#endif
	break;
      case PTW32_EPS_EXIT:
	status = sp->exitStatus;
	break;
      default:
	status = sp->exitStatus = PTHREAD_CANCELED;
	break;
      }
  }

#else /* __CLEANUP_SEH */

#if defined(__CLEANUP_C)

  setjmp_rc = setjmp (sp->start_mark);

  if (0 == setjmp_rc)
    {

      /*
       * Run the caller's routine;
       */
      status = sp->exitStatus = (*start) (arg);
      sp->state = PThreadStateExiting;
    }
  else
    {
      switch (setjmp_rc)
	{
	case PTW32_EPS_CANCEL:
	  status = sp->exitStatus = PTHREAD_CANCELED;
	  break;
	case PTW32_EPS_EXIT:
	  status = sp->exitStatus;
	  break;
	default:
	  status = sp->exitStatus = PTHREAD_CANCELED;
	  break;
	}
    }

#else /* __CLEANUP_C */

#if defined(__CLEANUP_CXX)

  ptw32_oldTerminate = set_terminate (&ptw32_terminate);

  try
  {
    /*
     * Run the caller's routine in a nested try block so that we
     * can run the user's terminate function, which may call
     * pthread_exit() or be canceled.
     */
    try
    {
      status = sp->exitStatus = (*start) (arg);
      sp->state = PThreadStateExiting;
    }
    catch (ptw32_exception &)
    {
      /*
       * Pass these through to the outer block.
       */
      throw;
    }
    catch (...)
    {
      /*
       * We want to run the user's terminate function if supplied.
       * That function may call pthread_exit() or be canceled, which will
       * be handled by the outer try block.
       *
       * ptw32_terminate() will be called if there is no user
       * supplied function.
       */
      terminate_function
	term_func = set_terminate (0);
      set_terminate (term_func);

      if (term_func != 0)
	{
	  term_func ();
	}
      throw;
    }
  }
  catch (ptw32_exception_cancel &)
  {
    /*
     * Thread was canceled.
     */
    status = sp->exitStatus = PTHREAD_CANCELED;
  }
  catch (ptw32_exception_exit &)
  {
    /*
     * Thread was exited via pthread_exit().
     */
    status = sp->exitStatus;
  }
  catch (...)
  {
    /*
     * A system unexpected exception has occurred running the user's
     * terminate routine. We get control back within this block
     * and exit with a substitute status. If the thread was not
     * cancelled then this indicates the unhandled exception.
     */
    status = sp->exitStatus = PTHREAD_CANCELED;
  }

  (void) set_terminate (ptw32_oldTerminate);

#else

#error ERROR [__FILE__, line __LINE__]: Cleanup type undefined.

#endif /* __CLEANUP_CXX */
#endif /* __CLEANUP_C */
#endif /* __CLEANUP_SEH */

#if defined(PTW32_STATIC_LIB)
  /*
   * We need to cleanup the pthread now if we have
   * been statically linked, in which case the cleanup
   * in dllMain won't get done. Joinable threads will
   * only be partially cleaned up and must be fully cleaned
   * up by pthread_join() or pthread_detach().
   *
   * Note: if this library has been statically linked,
   * implicitly created pthreads (those created
   * for Win32 threads which have called pthreads routines)
   * must be cleaned up explicitly by the application
   * (by calling pthread_win32_thread_detach_np()).
   * For the dll, dllMain will do the cleanup automatically.
   */
  (void) pthread_win32_thread_detach_np ();
#endif

#if ! (defined(__MINGW64__) || defined(__MINGW32__)) || defined (__MSVCRT__) || defined (__DMC__)
  _endthreadex ((unsigned)(size_t) status);
#else
  _endthread ();
#endif

  /*
   * Never reached.
   */

#if ! (defined(__MINGW64__) || defined(__MINGW32__)) || defined (__MSVCRT__) || defined (__DMC__)
  return (unsigned)(size_t) status;
#endif

}				/* ptw32_threadStart */
