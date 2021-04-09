/**
 * \file libev.c
 * \brief Eventloop extension for libev
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
#include "getdns/getdns_ext_libev.h"

#ifdef HAVE_LIBEV_EV_H
#include <libev/ev.h>
#else
#include <ev.h>
#endif

typedef struct getdns_libev {
	getdns_eventloop_vmt *vmt;
	struct ev_loop       *loop;
	struct mem_funcs      mf;
} getdns_libev;

static void 
getdns_libev_run(getdns_eventloop *loop)
{
	(void) ev_run(((getdns_libev *)loop)->loop, 0);
}

static void 
getdns_libev_run_once(getdns_eventloop *loop, int blocking)
{
	(void) ev_run(((getdns_libev *)loop)->loop,
	    blocking ? EVRUN_ONCE : EVRUN_NOWAIT);
}

static void 
getdns_libev_cleanup(getdns_eventloop *loop)
{
	getdns_libev *ext = (getdns_libev *)loop;
	GETDNS_FREE(ext->mf, ext);
}

typedef struct io_timer {
	ev_io    read;
	ev_io    write;
	ev_timer timer;
} io_timer;

static getdns_return_t
getdns_libev_clear(getdns_eventloop *loop, getdns_eventloop_event *el_ev)
{
	getdns_libev *ext = (getdns_libev *)loop;
	io_timer *my_ev = (io_timer *)el_ev->ev;
	
	assert(my_ev);

	if (el_ev->read_cb)
		ev_io_stop(ext->loop, &my_ev->read);
	if (el_ev->write_cb)
		ev_io_stop(ext->loop, &my_ev->write);
	if (el_ev->timeout_cb)
		ev_timer_stop(ext->loop, &my_ev->timer);

	GETDNS_FREE(ext->mf, el_ev->ev);
	el_ev->ev = NULL;
	return GETDNS_RETURN_GOOD;
}

static void
getdns_libev_read_cb(struct ev_loop *l, struct ev_io *io, int revents)
{
        getdns_eventloop_event *el_ev = (getdns_eventloop_event *)io->data;
	(void)l; (void)revents;
        assert(el_ev->read_cb);
        el_ev->read_cb(el_ev->userarg);
}

static void
getdns_libev_write_cb(struct ev_loop *l, struct ev_io *io, int revents)
{
        getdns_eventloop_event *el_ev = (getdns_eventloop_event *)io->data;
	(void)l; (void)revents;
        assert(el_ev->write_cb);
        el_ev->write_cb(el_ev->userarg);
}

static void
getdns_libev_timeout_cb(struct ev_loop *l, struct ev_timer *timer, int revents)
{
        getdns_eventloop_event *el_ev = (getdns_eventloop_event *)timer->data;
	(void)l; (void)revents;
        assert(el_ev->timeout_cb);
        el_ev->timeout_cb(el_ev->userarg);
}

static getdns_return_t
getdns_libev_schedule(getdns_eventloop *loop,
    int fd, uint64_t timeout, getdns_eventloop_event *el_ev)
{
	getdns_libev *ext = (getdns_libev *)loop;
	io_timer     *my_ev;
	ev_io        *my_io;
	ev_timer     *my_timer;
	ev_tstamp     to = ((ev_tstamp)timeout) / 1000;

	assert(el_ev);
	assert(!(el_ev->read_cb || el_ev->write_cb) || fd >= 0);
	assert(  el_ev->read_cb || el_ev->write_cb  || el_ev->timeout_cb);

	if (!(my_ev = GETDNS_MALLOC(ext->mf, io_timer)))
		return GETDNS_RETURN_MEMORY_ERROR;

	el_ev->ev = my_ev;
	
	if (el_ev->read_cb) {
		my_io = &my_ev->read;
		ev_io_init(my_io, getdns_libev_read_cb, fd, EV_READ);
		my_io->data = el_ev;
		ev_io_start(ext->loop, my_io);
	}
	if (el_ev->write_cb) {
		my_io = &my_ev->write;
		ev_io_init(my_io, getdns_libev_write_cb, fd, EV_WRITE);
		my_io->data = el_ev;
		ev_io_start(ext->loop, my_io);
	}
	if (el_ev->timeout_cb) {
		my_timer = &my_ev->timer;
		ev_timer_init(my_timer, getdns_libev_timeout_cb, to, 0);
		my_timer->data = el_ev;
		ev_timer_start(ext->loop, my_timer);
	}
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_extension_set_libev_loop(getdns_context *context,
    struct ev_loop *loop)
{
	static getdns_eventloop_vmt getdns_libev_vmt = {
		getdns_libev_cleanup,
		getdns_libev_schedule,
		getdns_libev_clear,
		getdns_libev_run,
		getdns_libev_run_once
	};
	getdns_libev *ext;

	if (!context)
		return GETDNS_RETURN_BAD_CONTEXT;
	if (!loop)
		return GETDNS_RETURN_INVALID_PARAMETER;

	ext = GETDNS_MALLOC(*priv_getdns_context_mf(context), getdns_libev);
	if (!ext)
		return GETDNS_RETURN_MEMORY_ERROR;
	ext->vmt  = &getdns_libev_vmt;
	ext->loop = loop;
	ext->mf   = *priv_getdns_context_mf(context);

	return getdns_context_set_eventloop(context, (getdns_eventloop *)ext);
}
