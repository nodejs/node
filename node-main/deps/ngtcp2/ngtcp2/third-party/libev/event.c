/*
 * libevent compatibility layer
 *
 * Copyright (c) 2007,2008,2009,2010,2012 Marc Alexander Lehmann <libev@schmorp.de>
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

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#ifdef EV_EVENT_H
# include EV_EVENT_H
#else
# include "event.h"
#endif

#if EV_MULTIPLICITY
# define dLOOPev struct ev_loop *loop = (struct ev_loop *)ev->ev_base
# define dLOOPbase struct ev_loop *loop = (struct ev_loop *)base
#else
# define dLOOPev
# define dLOOPbase
#endif

/* never accessed, will always be cast from/to ev_loop */
struct event_base
{
  int dummy;
};

static struct event_base *ev_x_cur;

static ev_tstamp
ev_tv_get (struct timeval *tv)
{
  if (tv)
    {
      ev_tstamp after = tv->tv_sec + tv->tv_usec * 1e-6;
      return after ? after : 1e-6;
    }
  else
    return -1.;
}

#define EVENT_STRINGIFY(s) # s
#define EVENT_VERSION(a,b) EVENT_STRINGIFY (a) "." EVENT_STRINGIFY (b)

const char *
event_get_version (void)
{
  /* returns ABI, not API or library, version */
  return EVENT_VERSION (EV_VERSION_MAJOR, EV_VERSION_MINOR);
}

const char *
event_get_method (void)
{
  return "libev";
}

void *event_init (void)
{
#if EV_MULTIPLICITY
  if (ev_x_cur)
    ev_x_cur = (struct event_base *)ev_loop_new (EVFLAG_AUTO);
  else
    ev_x_cur = (struct event_base *)ev_default_loop (EVFLAG_AUTO);
#else
  assert (("libev: multiple event bases not supported when not compiled with EV_MULTIPLICITY", !ev_x_cur));

  ev_x_cur = (struct event_base *)(long)ev_default_loop (EVFLAG_AUTO);
#endif

  return ev_x_cur;
}

const char *
event_base_get_method (const struct event_base *base)
{
  return "libev";
}

struct event_base *
event_base_new (void)
{
#if EV_MULTIPLICITY
  return (struct event_base *)ev_loop_new (EVFLAG_AUTO);
#else
  assert (("libev: multiple event bases not supported when not compiled with EV_MULTIPLICITY"));
  return NULL;
#endif
}

void event_base_free (struct event_base *base)
{
  dLOOPbase;

#if EV_MULTIPLICITY
  if (!ev_is_default_loop (loop))
    ev_loop_destroy (loop);
#endif
}

int event_dispatch (void)
{
  return event_base_dispatch (ev_x_cur);
}

#ifdef EV_STANDALONE
void event_set_log_callback (event_log_cb cb)
{
  /* nop */
}
#endif

int event_loop (int flags)
{
  return event_base_loop (ev_x_cur, flags);
}

int event_loopexit (struct timeval *tv)
{
  return event_base_loopexit (ev_x_cur, tv);
}

event_callback_fn event_get_callback
(const struct event *ev)
{
  return ev->ev_callback;
}

static void
ev_x_cb (struct event *ev, int revents)
{
  revents &= EV_READ | EV_WRITE | EV_TIMER | EV_SIGNAL;

  ev->ev_res = revents;
  ev->ev_callback (ev->ev_fd, (short)revents, ev->ev_arg);
}

static void
ev_x_cb_sig (EV_P_ struct ev_signal *w, int revents)
{
  struct event *ev = (struct event *)(((char *)w) - offsetof (struct event, iosig.sig));

  if (revents & EV_ERROR)
    event_del (ev);

  ev_x_cb (ev, revents);
}

static void
ev_x_cb_io (EV_P_ struct ev_io *w, int revents)
{
  struct event *ev = (struct event *)(((char *)w) - offsetof (struct event, iosig.io));

  if ((revents & EV_ERROR) || !(ev->ev_events & EV_PERSIST))
    event_del (ev);

  ev_x_cb (ev, revents);
}

static void
ev_x_cb_to (EV_P_ struct ev_timer *w, int revents)
{
  struct event *ev = (struct event *)(((char *)w) - offsetof (struct event, to));

  event_del (ev);

  ev_x_cb (ev, revents);
}

void event_set (struct event *ev, int fd, short events, void (*cb)(int, short, void *), void *arg)
{
  if (events & EV_SIGNAL)
    ev_init (&ev->iosig.sig, ev_x_cb_sig);
  else
    ev_init (&ev->iosig.io, ev_x_cb_io);

  ev_init (&ev->to, ev_x_cb_to);

  ev->ev_base     = ev_x_cur; /* not threadsafe, but it's how libevent works */
  ev->ev_fd       = fd;
  ev->ev_events   = events;
  ev->ev_pri      = 0;
  ev->ev_callback = cb;
  ev->ev_arg      = arg;
  ev->ev_res      = 0;
  ev->ev_flags    = EVLIST_INIT;
}

int event_once (int fd, short events, void (*cb)(int, short, void *), void *arg, struct timeval *tv)
{
  return event_base_once (ev_x_cur, fd, events, cb, arg, tv);
}

int event_add (struct event *ev, struct timeval *tv)
{
  dLOOPev;

  if (ev->ev_events & EV_SIGNAL)
    {
      if (!ev_is_active (&ev->iosig.sig))
        {
          ev_signal_set (&ev->iosig.sig, ev->ev_fd);
          ev_signal_start (EV_A_ &ev->iosig.sig);

          ev->ev_flags |= EVLIST_SIGNAL;
        }
    }
  else if (ev->ev_events & (EV_READ | EV_WRITE))
    {
      if (!ev_is_active (&ev->iosig.io))
        {
          ev_io_set (&ev->iosig.io, ev->ev_fd, ev->ev_events & (EV_READ | EV_WRITE));
          ev_io_start (EV_A_ &ev->iosig.io);

          ev->ev_flags |= EVLIST_INSERTED;
        }
    }

  if (tv)
    {
      ev->to.repeat = ev_tv_get (tv);
      ev_timer_again (EV_A_ &ev->to);
      ev->ev_flags |= EVLIST_TIMEOUT;
    }
  else
    {
      ev_timer_stop (EV_A_ &ev->to);
      ev->ev_flags &= ~EVLIST_TIMEOUT;
    }

  ev->ev_flags |= EVLIST_ACTIVE;

  return 0;
}

int event_del (struct event *ev)
{
  dLOOPev;

  if (ev->ev_events & EV_SIGNAL)
    ev_signal_stop (EV_A_ &ev->iosig.sig);
  else if (ev->ev_events & (EV_READ | EV_WRITE))
    ev_io_stop (EV_A_ &ev->iosig.io);

  if (ev_is_active (&ev->to))
    ev_timer_stop (EV_A_ &ev->to);

  ev->ev_flags = EVLIST_INIT;

  return 0;
}

void event_active (struct event *ev, int res, short ncalls)
{
  dLOOPev;

  if (res & EV_TIMEOUT)
    ev_feed_event (EV_A_ &ev->to, res & EV_TIMEOUT);

  if (res & EV_SIGNAL)
    ev_feed_event (EV_A_ &ev->iosig.sig, res & EV_SIGNAL);

  if (res & (EV_READ | EV_WRITE))
    ev_feed_event (EV_A_ &ev->iosig.io, res & (EV_READ | EV_WRITE));
}

int event_pending (struct event *ev, short events, struct timeval *tv)
{
  short revents = 0;
  dLOOPev;

  if (ev->ev_events & EV_SIGNAL)
    {
      /* sig */
      if (ev_is_active (&ev->iosig.sig) || ev_is_pending (&ev->iosig.sig))
        revents |= EV_SIGNAL;
    }
  else if (ev->ev_events & (EV_READ | EV_WRITE))
    {
      /* io */
      if (ev_is_active (&ev->iosig.io) || ev_is_pending (&ev->iosig.io))
        revents |= ev->ev_events & (EV_READ | EV_WRITE);
    }

  if (ev->ev_events & EV_TIMEOUT || ev_is_active (&ev->to) || ev_is_pending (&ev->to))
    {
      revents |= EV_TIMEOUT;

      if (tv)
        {
          ev_tstamp at = ev_now (EV_A);

          tv->tv_sec  = (long)at;
          tv->tv_usec = (long)((at - (ev_tstamp)tv->tv_sec) * 1e6);
        }
    }

  return events & revents;
}

int event_priority_init (int npri)
{
  return event_base_priority_init (ev_x_cur, npri);
}

int event_priority_set (struct event *ev, int pri)
{
  ev->ev_pri = pri;

  return 0;
}

int event_base_set (struct event_base *base, struct event *ev)
{
  ev->ev_base = base;

  return 0;
}

int event_base_loop (struct event_base *base, int flags)
{
  dLOOPbase;

  return !ev_run (EV_A_ flags);
}

int event_base_dispatch (struct event_base *base)
{
  return event_base_loop (base, 0);
}

static void
ev_x_loopexit_cb (int revents, void *base)
{
  dLOOPbase;

  ev_break (EV_A_ EVBREAK_ONE);
}

int event_base_loopexit (struct event_base *base, struct timeval *tv)
{
  ev_tstamp after = ev_tv_get (tv);
  dLOOPbase;

  ev_once (EV_A_ -1, 0, after >= 0. ? after : 0., ev_x_loopexit_cb, (void *)base);

  return 0;
}

struct ev_x_once
{
  int fd;
  void (*cb)(int, short, void *);
  void *arg;
};

static void
ev_x_once_cb (int revents, void *arg)
{
  struct ev_x_once *once = (struct ev_x_once *)arg;

  once->cb (once->fd, (short)revents, once->arg);
  free (once);
}

int event_base_once (struct event_base *base, int fd, short events, void (*cb)(int, short, void *), void *arg, struct timeval *tv)
{
  struct ev_x_once *once = (struct ev_x_once *)malloc (sizeof (struct ev_x_once));
  dLOOPbase;

  if (!once)
    return -1;

  once->fd  = fd;
  once->cb  = cb;
  once->arg = arg;

  ev_once (EV_A_ fd, events & (EV_READ | EV_WRITE), ev_tv_get (tv), ev_x_once_cb, (void *)once);

  return 0;
}

int event_base_priority_init (struct event_base *base, int npri)
{
  /*dLOOPbase;*/

  return 0;
}

