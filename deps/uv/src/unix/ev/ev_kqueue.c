/*
 * libev kqueue backend
 *
 * Copyright (c) 2007,2008,2009,2010 Marc Alexander Lehmann <libev@schmorp.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <string.h>
#include <errno.h>

extern void
uv__kqueue_hack (EV_P_ int fflags, ev_io *w);

void inline_speed
kqueue_change (EV_P_ int fd, int filter, int flags, int fflags)
{
  ++kqueue_changecnt;
  array_needsize (struct kevent, kqueue_changes, kqueue_changemax, kqueue_changecnt, EMPTY2);

  EV_SET (&kqueue_changes [kqueue_changecnt - 1], fd, filter, flags, fflags, 0, 0);
}

/* OS X at least needs this */
#ifndef EV_ENABLE
# define EV_ENABLE 0
#endif
#ifndef NOTE_EOF
# define NOTE_EOF 0
#endif

static void
kqueue_modify (EV_P_ int fd, int oev, int nev)
{
  if (oev != nev)
    {
      if (oev & EV_READ)
        kqueue_change (EV_A_ fd, EVFILT_READ , EV_DELETE, 0);

      if (oev & EV_WRITE)
        kqueue_change (EV_A_ fd, EVFILT_WRITE, EV_DELETE, 0);
    }

  /* to detect close/reopen reliably, we have to re-add */
  /* event requests even when oev == nev */

  if (nev & EV_READ)
    kqueue_change (EV_A_ fd, EVFILT_READ , EV_ADD | EV_ENABLE, NOTE_EOF);

  if (nev & EV_WRITE)
    kqueue_change (EV_A_ fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, NOTE_EOF);

  if (nev & EV_LIBUV_KQUEUE_HACK)
    kqueue_change (EV_A_ fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_ONESHOT,
      NOTE_ATTRIB | NOTE_WRITE | NOTE_RENAME | NOTE_DELETE | NOTE_EXTEND | NOTE_REVOKE);
}

static void
kqueue_poll (EV_P_ ev_tstamp timeout)
{
  int res, i;
  struct timespec ts;

  /* need to resize so there is enough space for errors */
  if (kqueue_changecnt > kqueue_eventmax)
    {
      ev_free (kqueue_events);
      kqueue_eventmax = array_nextsize (sizeof (struct kevent), kqueue_eventmax, kqueue_changecnt);
      kqueue_events = (struct kevent *)ev_malloc (sizeof (struct kevent) * kqueue_eventmax);
    }

  EV_RELEASE_CB;
  EV_TS_SET (ts, timeout);
  res = kevent (backend_fd, kqueue_changes, kqueue_changecnt, kqueue_events, kqueue_eventmax, &ts);
  EV_ACQUIRE_CB;
  kqueue_changecnt = 0;

  if (expect_false (res < 0))
    {
      if (errno != EINTR)
        ev_syserr ("(libev) kevent");

      return;
    }

  for (i = 0; i < res; ++i)
    {
      int fd = kqueue_events [i].ident;

      if (kqueue_events [i].filter == EVFILT_VNODE)
        {
          /* pass kqueue filter flags to libuv */
          ev_io *w = (ev_io *)(anfds [fd].head);
          uv__kqueue_hack (EV_A_ kqueue_events [i].fflags, w);
        }

      if (expect_false (kqueue_events [i].flags & EV_ERROR))
        {
          int err = kqueue_events [i].data;

          /* we are only interested in errors for fds that we are interested in :) */
          if (anfds [fd].events)
            {
              if (err == ENOENT) /* resubmit changes on ENOENT */
                kqueue_modify (EV_A_ fd, 0, anfds [fd].events);
              else if (err == EBADF) /* on EBADF, we re-check the fd */
                {
                  if (fd_valid (fd))
                    kqueue_modify (EV_A_ fd, 0, anfds [fd].events);
                  else
                    fd_kill (EV_A_ fd);
                }
              else /* on all other errors, we error out on the fd */
                fd_kill (EV_A_ fd);
            }
        }
      else
        fd_event (
          EV_A_
          fd,
          kqueue_events [i].filter == EVFILT_READ ? EV_READ
          : kqueue_events [i].filter == EVFILT_WRITE ? EV_WRITE
          : kqueue_events [i].filter == EVFILT_VNODE ? EV_LIBUV_KQUEUE_HACK
          : 0
        );
    }

  if (expect_false (res == kqueue_eventmax))
    {
      ev_free (kqueue_events);
      kqueue_eventmax = array_nextsize (sizeof (struct kevent), kqueue_eventmax, kqueue_eventmax + 1);
      kqueue_events = (struct kevent *)ev_malloc (sizeof (struct kevent) * kqueue_eventmax);
    }
}

int inline_size
kqueue_init (EV_P_ int flags)
{
  /* Initialize the kernel queue */
  if ((backend_fd = kqueue ()) < 0)
    return 0;

  fcntl (backend_fd, F_SETFD, FD_CLOEXEC); /* not sure if necessary, hopefully doesn't hurt */

  backend_fudge  = 0.;
  backend_modify = kqueue_modify;
  backend_poll   = kqueue_poll;

  kqueue_eventmax = 64; /* initial number of events receivable per poll */
  kqueue_events = (struct kevent *)ev_malloc (sizeof (struct kevent) * kqueue_eventmax);

  kqueue_changes   = 0;
  kqueue_changemax = 0;
  kqueue_changecnt = 0;

  return EVBACKEND_KQUEUE;
}

void inline_size
kqueue_destroy (EV_P)
{
  ev_free (kqueue_events);
  ev_free (kqueue_changes);
}

void inline_size
kqueue_fork (EV_P)
{
  while ((backend_fd = kqueue ()) < 0)
    ev_syserr ("(libev) kqueue");

  fcntl (backend_fd, F_SETFD, FD_CLOEXEC);

  /* re-register interest in fds */
  fd_rearm_all (EV_A);
}

