/*
 * libev poll fd activity backend
 *
 * Copyright (c) 2007,2008,2009,2010,2011,2016,2019 Marc Alexander Lehmann <libev@schmorp.de>
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

#include <poll.h>

inline_size
void
array_needsize_pollidx (int *base, int offset, int count)
{
  /* using memset (.., -1, ...) is tempting, we we try
   * to be ultraportable
   */
  base += offset;
  while (count--)
    *base++ = -1;
}

static void
poll_modify (EV_P_ int fd, int oev, int nev)
{
  int idx;

  if (oev == nev)
    return;

  array_needsize (int, pollidxs, pollidxmax, fd + 1, array_needsize_pollidx);

  idx = pollidxs [fd];

  if (idx < 0) /* need to allocate a new pollfd */
    {
      pollidxs [fd] = idx = pollcnt++;
      array_needsize (struct pollfd, polls, pollmax, pollcnt, array_needsize_noinit);
      polls [idx].fd = fd;
    }

  assert (polls [idx].fd == fd);

  if (nev)
    polls [idx].events =
        (nev & EV_READ ? POLLIN : 0)
        | (nev & EV_WRITE ? POLLOUT : 0);
  else /* remove pollfd */
    {
      pollidxs [fd] = -1;

      if (ecb_expect_true (idx < --pollcnt))
        {
          polls [idx] = polls [pollcnt];
          pollidxs [polls [idx].fd] = idx;
        }
    }
}

static void
poll_poll (EV_P_ ev_tstamp timeout)
{
  struct pollfd *p;
  int res;
  
  EV_RELEASE_CB;
  res = poll (polls, pollcnt, EV_TS_TO_MSEC (timeout));
  EV_ACQUIRE_CB;

  if (ecb_expect_false (res < 0))
    {
      if (errno == EBADF)
        fd_ebadf (EV_A);
      else if (errno == ENOMEM && !syserr_cb)
        fd_enomem (EV_A);
      else if (errno != EINTR)
        ev_syserr ("(libev) poll");
    }
  else
    for (p = polls; res; ++p)
      {
        assert (("libev: poll returned illegal result, broken BSD kernel?", p < polls + pollcnt));

        if (ecb_expect_false (p->revents)) /* this expect is debatable */
          {
            --res;

            if (ecb_expect_false (p->revents & POLLNVAL))
              {
                assert (("libev: poll found invalid fd in poll set", 0));
                fd_kill (EV_A_ p->fd);
              }
            else
              fd_event (
                EV_A_
                p->fd,
                (p->revents & (POLLOUT | POLLERR | POLLHUP) ? EV_WRITE : 0)
                | (p->revents & (POLLIN | POLLERR | POLLHUP) ? EV_READ : 0)
              );
          }
      }
}

inline_size
int
poll_init (EV_P_ int flags)
{
  backend_mintime = EV_TS_CONST (1e-3);
  backend_modify  = poll_modify;
  backend_poll    = poll_poll;

  pollidxs = 0; pollidxmax = 0;
  polls    = 0; pollmax    = 0; pollcnt = 0;

  return EVBACKEND_POLL;
}

inline_size
void
poll_destroy (EV_P)
{
  ev_free (pollidxs);
  ev_free (polls);
}

