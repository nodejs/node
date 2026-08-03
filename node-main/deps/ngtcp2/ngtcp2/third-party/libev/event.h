/*
 * libevent compatibility header, only core events supported
 *
 * Copyright (c) 2007,2008,2010,2012 Marc Alexander Lehmann <libev@schmorp.de>
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

#ifndef EVENT_H_
#define EVENT_H_

#ifdef EV_H
# include EV_H
#else
# include "ev.h"
#endif

#ifndef EVLOOP_NONBLOCK
# define EVLOOP_NONBLOCK EVRUN_NOWAIT
#endif
#ifndef EVLOOP_ONESHOT
# define EVLOOP_ONESHOT EVRUN_ONCE
#endif
#ifndef EV_TIMEOUT
# define EV_TIMEOUT EV_TIMER
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* we need sys/time.h for struct timeval only */
#if !defined (WIN32) || defined (__MINGW32__)
# include <time.h> /* mingw seems to need this, for whatever reason */
# include <sys/time.h>
#endif

struct event_base;

#define EVLIST_TIMEOUT  0x01
#define EVLIST_INSERTED 0x02
#define EVLIST_SIGNAL   0x04
#define EVLIST_ACTIVE   0x08
#define EVLIST_INTERNAL 0x10
#define EVLIST_INIT     0x80

typedef void (*event_callback_fn)(int, short, void *);

struct event
{
  /* libev watchers we map onto */
  union {
    struct ev_io io;
    struct ev_signal sig;
  } iosig;
  struct ev_timer to;

  /* compatibility slots */
  struct event_base *ev_base;
  event_callback_fn ev_callback;
  void *ev_arg;
  int ev_fd;
  int ev_pri;
  int ev_res;
  int ev_flags;
  short ev_events;
};

event_callback_fn event_get_callback (const struct event *ev);

#define EV_READ                    EV_READ
#define EV_WRITE                   EV_WRITE
#define EV_PERSIST                 0x10
#define EV_ET                      0x20 /* nop */

#define EVENT_SIGNAL(ev)           ((int) (ev)->ev_fd)
#define EVENT_FD(ev)               ((int) (ev)->ev_fd)

#define event_initialized(ev)      ((ev)->ev_flags & EVLIST_INIT)

#define evtimer_add(ev,tv)         event_add (ev, tv)
#define evtimer_set(ev,cb,data)    event_set (ev, -1, 0, cb, data)
#define evtimer_del(ev)            event_del (ev)
#define evtimer_pending(ev,tv)     event_pending (ev, EV_TIMEOUT, tv)
#define evtimer_initialized(ev)    event_initialized (ev)

#define timeout_add(ev,tv)         evtimer_add (ev, tv)
#define timeout_set(ev,cb,data)    evtimer_set (ev, cb, data)
#define timeout_del(ev)            evtimer_del (ev)
#define timeout_pending(ev,tv)     evtimer_pending (ev, tv)
#define timeout_initialized(ev)    evtimer_initialized (ev)

#define signal_add(ev,tv)          event_add (ev, tv)
#define signal_set(ev,sig,cb,data) event_set (ev, sig, EV_SIGNAL | EV_PERSIST, cb, data)
#define signal_del(ev)             event_del (ev)
#define signal_pending(ev,tv)      event_pending (ev, EV_SIGNAL, tv)
#define signal_initialized(ev)     event_initialized (ev)

const char *event_get_version (void);
const char *event_get_method (void);

void *event_init (void);
void event_base_free (struct event_base *base);

#define EVLOOP_ONCE      EVLOOP_ONESHOT
int event_loop (int);
int event_loopexit (struct timeval *tv);
int event_dispatch (void);

#define _EVENT_LOG_DEBUG 0
#define _EVENT_LOG_MSG   1
#define _EVENT_LOG_WARN  2
#define _EVENT_LOG_ERR   3
typedef void (*event_log_cb)(int severity, const char *msg);
void event_set_log_callback(event_log_cb cb);

void event_set (struct event *ev, int fd, short events, void (*cb)(int, short, void *), void *arg);
int event_once (int fd, short events, void (*cb)(int, short, void *), void *arg, struct timeval *tv);

int event_add (struct event *ev, struct timeval *tv);
int event_del (struct event *ev);
void event_active (struct event *ev, int res, short ncalls); /* ncalls is being ignored */

int event_pending (struct event *ev, short, struct timeval *tv);

int event_priority_init (int npri);
int event_priority_set (struct event *ev, int pri);

struct event_base *event_base_new (void);
const char *event_base_get_method (const struct event_base *);
int event_base_set (struct event_base *base, struct event *ev);
int event_base_loop (struct event_base *base, int);
int event_base_loopexit (struct event_base *base, struct timeval *tv);
int event_base_dispatch (struct event_base *base);
int event_base_once (struct event_base *base, int fd, short events, void (*cb)(int, short, void *), void *arg, struct timeval *tv);
int event_base_priority_init (struct event_base *base, int fd);

/* next line is different in the libevent+libev version */
/*libevent-include*/

#ifdef __cplusplus
}
#endif

#endif

