/**
 * \file libevent.c
 * \brief Eventloop extension for libevent
 *
 */

/*
 * Copyright (c) 2013, NLNet Labs, Verisign, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the names of the copyright holders nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Verisign, Inc. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "types-internal.h"
#ifndef USE_WINSOCK
#include <sys/time.h>
#else
#include <winsock2.h>
#endif
#include "getdns/getdns_ext_libevent.h"

#ifdef HAVE_EVENT2_EVENT_H
#  include <event2/event.h>
#else
#  include <event.h>
#  define evutil_socket_t int
#  define event_free free
#  define evtimer_new(b, cb, arg) event_new((b), -1, 0, (cb), (arg))
#endif

#ifndef HAVE_EVENT_BASE_FREE
#define event_base_free(x) /* nop */
#endif
#ifndef HAVE_EVENT_BASE_NEW
#define event_base_new event_init
#endif

#ifndef HAVE_EVENT2_EVENT_H
static struct event *
event_new(struct event_base *b, evutil_socket_t fd, short ev, void (*cb)(int, short, void*), void *arg)
{
	struct event* e = (struct event*)calloc(1, sizeof(struct event));

	if (!e) return NULL;
	event_set(e, fd, ev, cb, arg);
	event_base_set(b, e);
	return e;
}
#endif /* no event2 */

typedef struct getdns_libevent {
	getdns_eventloop_vmt *vmt;
        struct event_base    *base;
	struct mem_funcs      mf;
} getdns_libevent;

static void
getdns_libevent_run(getdns_eventloop *loop)
{
	(void) event_base_dispatch(((getdns_libevent *)loop)->base);
}

static void
getdns_libevent_run_once(getdns_eventloop *loop, int blocking)
{
	(void) event_base_loop(((getdns_libevent *)loop)->base,
	    EVLOOP_ONCE | (blocking ? EVLOOP_NONBLOCK : 0));
}

static void
getdns_libevent_cleanup(getdns_eventloop *loop)
{
	getdns_libevent *ext = (getdns_libevent *)loop;
	GETDNS_FREE(ext->mf, ext);
}

static getdns_return_t
getdns_libevent_clear(getdns_eventloop *loop, getdns_eventloop_event *el_ev)
{
	struct event *my_ev = (struct event *)el_ev->ev;
	(void)loop;

	assert(my_ev);

	if (event_del(my_ev) != 0)
		return GETDNS_RETURN_GENERIC_ERROR;

	event_free(my_ev);
	el_ev->ev = NULL;
	return GETDNS_RETURN_GOOD;
}

static void
getdns_libevent_callback(evutil_socket_t fd, short bits, void *arg)
{
	getdns_eventloop_event *el_ev = (getdns_eventloop_event *)arg;
	(void)fd;

	if (bits & EV_READ) {
		assert(el_ev->read_cb);
		el_ev->read_cb(el_ev->userarg);
	} else if (bits & EV_WRITE) {
		assert(el_ev->write_cb);
		el_ev->write_cb(el_ev->userarg);
	} else if (bits & EV_TIMEOUT) {
		assert(el_ev->timeout_cb);
		el_ev->timeout_cb(el_ev->userarg);
	} else
		assert(ASSERT_UNREACHABLE);
}

static getdns_return_t
getdns_libevent_schedule(getdns_eventloop *loop,
    int fd, uint64_t timeout, getdns_eventloop_event *el_ev)
{
	getdns_libevent *ext = (getdns_libevent *)loop;
	struct event *my_ev;
	struct timeval tv = { timeout / 1000, (timeout % 1000) * 1000 };

	assert(el_ev);
	assert(!(el_ev->read_cb || el_ev->write_cb) || fd >= 0);
	assert(  el_ev->read_cb || el_ev->write_cb  || el_ev->timeout_cb);

	my_ev = event_new(ext->base, fd, (
	    (el_ev->read_cb ? EV_READ|EV_PERSIST : 0) |
	    (el_ev->write_cb ? EV_WRITE|EV_PERSIST : 0) |
	    (el_ev->timeout_cb ? EV_TIMEOUT : 0)),
	    getdns_libevent_callback, el_ev);
	if (!my_ev)
		return GETDNS_RETURN_MEMORY_ERROR;

	el_ev->ev = my_ev;

	if (event_add(my_ev, el_ev->timeout_cb ? &tv : NULL)) {
		event_free(my_ev);
		el_ev->ev = NULL;
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_extension_set_libevent_base(getdns_context *context,
    struct event_base *base)
{
	static getdns_eventloop_vmt getdns_libevent_vmt = {
		getdns_libevent_cleanup,
		getdns_libevent_schedule,
		getdns_libevent_clear,
		getdns_libevent_run,
		getdns_libevent_run_once
	};
	getdns_libevent *ext;

	if (!context)
		return GETDNS_RETURN_BAD_CONTEXT;
	if (!base)
		return GETDNS_RETURN_INVALID_PARAMETER;

	ext = GETDNS_MALLOC(*priv_getdns_context_mf(context), getdns_libevent);
	if (!ext)
		return GETDNS_RETURN_MEMORY_ERROR;

	ext->vmt  = &getdns_libevent_vmt;
	ext->base = base;
	ext->mf   = *priv_getdns_context_mf(context);

	return getdns_context_set_eventloop(context, (getdns_eventloop *)ext);
}
