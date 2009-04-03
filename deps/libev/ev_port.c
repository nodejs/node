/*
 * libev solaris event port backend
 *
 * Copyright (c) 2007,2008 Marc Alexander Lehmann <libev@schmorp.de>
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
#include <poll.h>
#include <port.h>
#include <string.h>
#include <errno.h>

void inline_speed
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
        fd_kill (EV_A_ fd);
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

  ts.tv_sec  = (time_t)timeout;
  ts.tv_nsec = (long)(timeout - (ev_tstamp)ts.tv_sec) * 1e9;
  res = port_getn (backend_fd, port_events, port_eventmax, &nget, &ts);

  if (res == -1)
    { 
      if (errno != EINTR && errno != ETIME)
        ev_syserr ("(libev) port_getn");

      return;
    } 

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

          port_associate_and_check (EV_A_ fd, anfds [fd].events);
        }
    }

  if (expect_false (nget == port_eventmax))
    {
      ev_free (port_events);
      port_eventmax = array_nextsize (sizeof (port_event_t), port_eventmax, port_eventmax + 1);
      port_events = (port_event_t *)ev_malloc (sizeof (port_event_t) * port_eventmax);
    }
}

int inline_size
port_init (EV_P_ int flags)
{
  /* Initalize the kernel queue */
  if ((backend_fd = port_create ()) < 0)
    return 0;

  fcntl (backend_fd, F_SETFD, FD_CLOEXEC); /* not sure if necessary, hopefully doesn't hurt */

  backend_fudge  = 1e-3; /* needed to compensate for port_getn returning early */
  backend_modify = port_modify;
  backend_poll   = port_poll;

  port_eventmax = 64; /* intiial number of events receivable per poll */
  port_events = (port_event_t *)ev_malloc (sizeof (port_event_t) * port_eventmax);

  return EVBACKEND_PORT;
}

void inline_size
port_destroy (EV_P)
{
  ev_free (port_events);
}

void inline_size
port_fork (EV_P)
{
  close (backend_fd);

  while ((backend_fd = port_create ()) < 0)
    ev_syserr ("(libev) port");

  fcntl (backend_fd, F_SETFD, FD_CLOEXEC);

  /* re-register interest in fds */
  fd_rearm_all (EV_A);
}

