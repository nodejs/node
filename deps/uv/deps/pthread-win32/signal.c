/*
 * signal.c
 *
 * Description:
 * Thread-aware signal functions.
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

/*
 * Possible future strategy for implementing pthread_kill()
 * ========================================================
 *
 * Win32 does not implement signals.
 * Signals are simply software interrupts.
 * pthread_kill() asks the system to deliver a specified
 * signal (interrupt) to a specified thread in the same
 * process.
 * Signals are always asynchronous (no deferred signals).
 * Pthread-win32 has an async cancelation mechanism.
 * A similar system can be written to deliver signals
 * within the same process (on ix86 processors at least).
 *
 * Each thread maintains information about which
 * signals it will respond to. Handler routines
 * are set on a per-process basis - not per-thread.
 * When signalled, a thread will check it's sigmask
 * and, if the signal is not being ignored, call the
 * handler routine associated with the signal. The
 * thread must then (except for some signals) return to
 * the point where it was interrupted.
 *
 * Ideally the system itself would check the target thread's
 * mask before possibly needlessly bothering the thread
 * itself. This could be done by pthread_kill(), that is,
 * in the signaling thread since it has access to
 * all pthread_t structures. It could also retrieve
 * the handler routine address to minimise the target
 * threads response overhead. This may also simplify
 * serialisation of the access to the per-thread signal
 * structures.
 *
 * pthread_kill() eventually calls a routine similar to
 * ptw32_cancel_thread() which manipulates the target
 * threads processor context to cause the thread to
 * run the handler launcher routine. pthread_kill() must
 * save the target threads current context so that the
 * handler launcher routine can restore the context after
 * the signal handler has returned. Some handlers will not
 * return, eg. the default SIGKILL handler may simply
 * call pthread_exit().
 *
 * The current context is saved in the target threads
 * pthread_t structure.
 */

#include "pthread.h"
#include "implement.h"

#if defined(HAVE_SIGSET_T)

static void
ptw32_signal_thread ()
{
}

static void
ptw32_signal_callhandler ()
{
}

int
pthread_sigmask (int how, sigset_t const *set, sigset_t * oset)
{
  pthread_t thread = pthread_self ();

  if (thread.p == NULL)
    {
      return ENOENT;
    }

  /* Validate the `how' argument. */
  if (set != NULL)
    {
      switch (how)
	{
	case SIG_BLOCK:
	  break;
	case SIG_UNBLOCK:
	  break;
	case SIG_SETMASK:
	  break;
	default:
	  /* Invalid `how' argument. */
	  return EINVAL;
	}
    }

  /* Copy the old mask before modifying it. */
  if (oset != NULL)
    {
      memcpy (oset, &(thread.p->sigmask), sizeof (sigset_t));
    }

  if (set != NULL)
    {
      unsigned int i;

      /* FIXME: this code assumes that sigmask is an even multiple of
         the size of a long integer. */

      unsigned long *src = (unsigned long const *) set;
      unsigned long *dest = (unsigned long *) &(thread.p->sigmask);

      switch (how)
	{
	case SIG_BLOCK:
	  for (i = 0; i < (sizeof (sigset_t) / sizeof (unsigned long)); i++)
	    {
	      /* OR the bit field longword-wise. */
	      *dest++ |= *src++;
	    }
	  break;
	case SIG_UNBLOCK:
	  for (i = 0; i < (sizeof (sigset_t) / sizeof (unsigned long)); i++)
	    {
	      /* XOR the bitfield longword-wise. */
	      *dest++ ^= *src++;
	    }
	case SIG_SETMASK:
	  /* Replace the whole sigmask. */
	  memcpy (&(thread.p->sigmask), set, sizeof (sigset_t));
	  break;
	}
    }

  return 0;
}

int
sigwait (const sigset_t * set, int *sig)
{
  /* This routine is a cancellation point */
  pthread_test_cancel();
}

int
sigaction (int signum, const struct sigaction *act, struct sigaction *oldact)
{
}

#endif /* HAVE_SIGSET_T */
