/*
 * libev solaris event port backend
 *
 * Copyright (c) 2007,2008,2009,2010,2011,2019 Marc Alexander Lehmann <libev@schmorp.de>
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

/* useful reading:
 *
 * http://bugs.opensolaris.org/view_bug.do?bug_id=6268715 (random results)
 * http://bugs.opensolaris.org/view_bug.do?bug_id=6455223 (just totally broken)
 * http://bugs.opensolaris.org/view_bug.do?bug_id=6873782 (manpage ETIME)
 * http://bugs.opensolaris.org/view_bug.do?bug_id=6874410 (implementation ETIME)
 * http://www.mail-archive.com/networking-discuss@opensolaris.org/msg11898.html ETIME vs. nget
 * http://src.opensolaris.org/source/xref/onnv/onnv-gate/usr/src/lib/libc/port/gen/event_port.c (libc)
 * http://cvs.opensolaris.org/source/xref/onnv/onnv-gate/usr/src/uts/common/fs/portfs/port.c#1325 (kernel)
 */

#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>
#include <port.h>
#include <string.h>
#include <errno.h>

inline_speed
void
port_associate_and_check (EV_P_ int fd, int ev)
{
  if (0 >
      port_associate (
         backend_fd, PORT_SOURCE_FD, fd,
         (ev & EV_READ ? POLLIN : 0)
         | (ev & EV_WRITE ? POLLOUT : 0),
         0
      )
  )
    {
      if (errno == EBADFD)
        {
          assert (("libev: port_associate found invalid fd", errno != EBADFD));
          fd_kill (EV_A_ fd);
        }
      else
        ev_syserr ("(libev) port_associate");
    }
}

static void
port_modify (EV_P_ int fd, int oev, int nev)
{
  /* we need to reassociate no matter what, as closes are
   * once more silently being discarded.
   */
  if (!nev)
    {
      if (oev)
        port_dissociate (backend_fd, PORT_SOURCE_FD, fd);
    }
  else
    port_associate_and_check (EV_A_ fd, nev);
}

static void
port_poll (EV_P_ ev_tstamp timeout)
{
  int res, i;
  struct timespec ts;
  uint_t nget = 1;

  /* we initialise this to something we will skip in the loop, as */
  /* port_getn can return with nget unchanged, but no indication */
  /* whether it was the original value or has been updated :/ */
  port_events [0].portev_source = 0;

  EV_RELEASE_CB;
  EV_TS_SET (ts, timeout);
  res = port_getn (backend_fd, port_events, port_eventmax, &nget, &ts);
  EV_ACQUIRE_CB;

  /* port_getn may or may not set nget on error */
  /* so we rely on port_events [0].portev_source not being updated */
  if (res == -1 && errno != ETIME && errno != EINTR)
    ev_syserr ("(libev) port_getn (see http://bugs.opensolaris.org/view_bug.do?bug_id=6268715, try LIBEV_FLAGS=3 env variable)");

  for (i = 0; i < nget; ++i)
    {
      if (port_events [i].portev_source == PORT_SOURCE_FD)
        {
          int fd = port_events [i].portev_object;

          fd_event (
            EV_A_
            fd,
            (port_events [i].portev_events & (POLLOUT | POLLERR | POLLHUP) ? EV_WRITE : 0)
            | (port_events [i].portev_events & (POLLIN | POLLERR | POLLHUP) ? EV_READ : 0)
          );

          fd_change (EV_A_ fd, EV__IOFDSET);
        }
    }

  if (ecb_expect_false (nget == port_eventmax))
    {
      ev_free (port_events);
      port_eventmax = array_nextsize (sizeof (port_event_t), port_eventmax, port_eventmax + 1);
      port_events = (port_event_t *)ev_malloc (sizeof (port_event_t) * port_eventmax);
    }
}

inline_size
int
port_init (EV_P_ int flags)
{
  /* Initialize the kernel queue */
  if ((backend_fd = port_create ()) < 0)
    return 0;

  assert (("libev: PORT_SOURCE_FD must not be zero", PORT_SOURCE_FD));

  fcntl (backend_fd, F_SETFD, FD_CLOEXEC); /* not sure if necessary, hopefully doesn't hurt */

  /* if my reading of the opensolaris kernel sources are correct, then
   * opensolaris does something very stupid: it checks if the time has already
   * elapsed and doesn't round up if that is the case, otherwise it DOES round
   * up. Since we can't know what the case is, we need to guess by using a
   * "large enough" timeout. Normally, 1e-9 would be correct.
   */
  backend_mintime = EV_TS_CONST (1e-3); /* needed to compensate for port_getn returning early */
  backend_modify  = port_modify;
  backend_poll    = port_poll;

  port_eventmax = 64; /* initial number of events receivable per poll */
  port_events = (port_event_t *)ev_malloc (sizeof (port_event_t) * port_eventmax);

  return EVBACKEND_PORT;
}

inline_size
void
port_destroy (EV_P)
{
  ev_free (port_events);
}

inline_size
void
port_fork (EV_P)
{
  close (backend_fd);

  while ((backend_fd = port_create ()) < 0)
    ev_syserr ("(libev) port");

  fcntl (backend_fd, F_SETFD, FD_CLOEXEC);

  /* re-register interest in fds */
  fd_rearm_all (EV_A);
}

