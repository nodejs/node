/**
 * \file libuv.c
 * \brief Eventloop extension for libuv.
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
#include "debug.h"
#include "types-internal.h"
#include <uv.h>
#include "getdns/getdns_ext_libuv.h"

#define UV_DEBUG 0

#if defined(UV_DEBUG) && UV_DEBUG
#include <time.h>
#define DEBUG_UV(...) DEBUG_ON(__VA_ARGS__)
#else
#define DEBUG_UV(...) DEBUG_OFF(__VA_ARGS__)
#endif

typedef struct getdns_libuv {
	getdns_eventloop_vmt *vmt;
	uv_loop_t            *loop;
	struct mem_funcs      mf;
} getdns_libuv;

static void
getdns_libuv_run(getdns_eventloop *loop)
{
	(void) uv_run(((getdns_libuv *)loop)->loop, UV_RUN_DEFAULT);
}

static void
getdns_libuv_run_once(getdns_eventloop *loop, int blocking)
{
	(void) uv_run(((getdns_libuv *)loop)->loop,
	    blocking ? UV_RUN_ONCE : UV_RUN_NOWAIT);
}

static void
getdns_libuv_cleanup(getdns_eventloop *loop)
{
	getdns_libuv *ext = (getdns_libuv *)loop;
	GETDNS_FREE(ext->mf, ext);
}

typedef struct poll_timer {
	uv_poll_t        read;
	uv_poll_t        write;
	uv_timer_t       timer;
	int              to_close;
	struct mem_funcs mf;
} poll_timer;

static void
getdns_libuv_close_cb(uv_handle_t *handle)
{
	poll_timer *my_ev = (poll_timer *)handle->data;

	DEBUG_UV("enter libuv_close_cb(el_ev->ev = %p, to_close = %d)\n"
	        , my_ev, my_ev->to_close);
	if (--my_ev->to_close) {
		DEBUG_UV(
		     "exit  libuv_close_cb(el_ev->ev = %p, to_close = %d)\n"
		    , my_ev, my_ev->to_close);
		return;
	}
	DEBUG_UV("enter libuv_close_cb to_free: %p\n", my_ev);
	GETDNS_FREE(my_ev->mf, my_ev);
	DEBUG_UV("enter libuv_close_cb freed: %p\n", my_ev);
}

static getdns_return_t
getdns_libuv_clear(getdns_eventloop *loop, getdns_eventloop_event *el_ev)
{
	poll_timer   *my_ev = (poll_timer *)el_ev->ev;
	uv_poll_t    *my_poll;
	uv_timer_t   *my_timer;
	(void)loop;
	
	assert(my_ev);

	DEBUG_UV("enter libuv_clear(el_ev = %p, my_ev = %p, to_close = %d)\n"
	        , el_ev, my_ev, my_ev->to_close);

	if (el_ev->read_cb) {
		my_poll = &my_ev->read;
		uv_poll_stop(my_poll);
		my_ev->to_close += 1;
		my_poll->data = my_ev;
		uv_close((uv_handle_t *)my_poll, getdns_libuv_close_cb);
	}
	if (el_ev->write_cb) {
		my_poll = &my_ev->write;
		uv_poll_stop(my_poll);
		my_ev->to_close += 1;
		my_poll->data = my_ev;
		uv_close((uv_handle_t *)my_poll, getdns_libuv_close_cb);
	}
	if (el_ev->timeout_cb) {
		my_timer = &my_ev->timer;
		uv_timer_stop(my_timer);
		my_ev->to_close += 1;
		my_timer->data = my_ev;
		uv_close((uv_handle_t *)my_timer, getdns_libuv_close_cb);
	}
	el_ev->ev = NULL;
	DEBUG_UV("exit  libuv_clear(el_ev = %p, my_ev = %p, to_close = %d)\n"
	        , el_ev, my_ev, my_ev->to_close);
	return GETDNS_RETURN_GOOD;
}

static void
getdns_libuv_read_cb(uv_poll_t *poll, int status, int events)
{
        getdns_eventloop_event *el_ev = (getdns_eventloop_event *)poll->data;
	(void)status; (void)events;
        assert(el_ev->read_cb);
	DEBUG_UV("enter libuv_read_cb(el_ev = %p, el_ev->ev = %p)\n"
	        , el_ev, el_ev->ev);
        el_ev->read_cb(el_ev->userarg);
	DEBUG_UV("exit  libuv_read_cb(el_ev = %p, el_ev->ev = %p)\n"
	        , el_ev, el_ev->ev);
}

static void
getdns_libuv_write_cb(uv_poll_t *poll, int status, int events)
{
        getdns_eventloop_event *el_ev = (getdns_eventloop_event *)poll->data;
	(void)status; (void)events;
        assert(el_ev->write_cb);
	DEBUG_UV("enter libuv_write_cb(el_ev = %p, el_ev->ev = %p)\n"
	        , el_ev, el_ev->ev);
        el_ev->write_cb(el_ev->userarg);
	DEBUG_UV("exit  libuv_write_cb(el_ev = %p, el_ev->ev = %p)\n"
	        , el_ev, el_ev->ev);
}

static void
#ifdef HAVE_NEW_UV_TIMER_CB
getdns_libuv_timeout_cb(uv_timer_t *timer)
#else
getdns_libuv_timeout_cb(uv_timer_t *timer, int status)
#endif
{
        getdns_eventloop_event *el_ev = (getdns_eventloop_event *)timer->data;
#ifndef HAVE_NEW_UV_TIMER_CB
	(void)status;
#endif
        assert(el_ev->timeout_cb);
	DEBUG_UV("enter libuv_timeout_cb(el_ev = %p, el_ev->ev = %p)\n"
	        , el_ev, el_ev->ev);
        el_ev->timeout_cb(el_ev->userarg);
	DEBUG_UV("exit  libuv_timeout_cb(el_ev = %p, el_ev->ev = %p)\n"
	        , el_ev, el_ev->ev);
}

static getdns_return_t
getdns_libuv_schedule(getdns_eventloop *loop,
    int fd, uint64_t timeout, getdns_eventloop_event *el_ev)
{
	getdns_libuv *ext = (getdns_libuv *)loop;
	poll_timer   *my_ev;
	uv_poll_t    *my_poll;
	uv_timer_t   *my_timer;

	assert(el_ev);
	assert(!(el_ev->read_cb || el_ev->write_cb) || fd >= 0);
	assert(  el_ev->read_cb || el_ev->write_cb  || el_ev->timeout_cb);

	DEBUG_UV("enter libuv_schedule(el_ev = %p, el_ev->ev = %p)\n"
	        , el_ev, el_ev->ev);

	if (!(my_ev = GETDNS_MALLOC(ext->mf, poll_timer)))
		return GETDNS_RETURN_MEMORY_ERROR;

	my_ev->to_close = 0;
	my_ev->mf = ext->mf;
	el_ev->ev = my_ev;
	
	if (el_ev->read_cb) {
		my_poll = &my_ev->read;
		my_poll->data = el_ev;
		uv_poll_init(ext->loop, my_poll, fd);
		uv_poll_start(my_poll, UV_READABLE, getdns_libuv_read_cb);
	}
	if (el_ev->write_cb) {
		my_poll = &my_ev->write;
		my_poll->data = el_ev;
		uv_poll_init(ext->loop, my_poll, fd);
		uv_poll_start(my_poll, UV_WRITABLE, getdns_libuv_write_cb);
	}
	if (el_ev->timeout_cb) {
		my_timer = &my_ev->timer;
		my_timer->data = el_ev;
		uv_timer_init(ext->loop, my_timer);
		uv_timer_start(my_timer, getdns_libuv_timeout_cb, timeout, 0);
	}
	DEBUG_UV("exit  libuv_schedule(el_ev = %p, el_ev->ev = %p)\n"
	        , el_ev, el_ev->ev);
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_extension_set_libuv_loop(getdns_context *context, uv_loop_t *loop)
{
	static getdns_eventloop_vmt getdns_libuv_vmt = {
		getdns_libuv_cleanup,
		getdns_libuv_schedule,
		getdns_libuv_clear,
		getdns_libuv_run,
		getdns_libuv_run_once
	};
	getdns_libuv *ext;

	if (!context)
		return GETDNS_RETURN_BAD_CONTEXT;
	if (!loop)
		return GETDNS_RETURN_INVALID_PARAMETER;

	ext = GETDNS_MALLOC(*priv_getdns_context_mf(context), getdns_libuv);
	if (!ext)
		return GETDNS_RETURN_MEMORY_ERROR;
	ext->vmt  = &getdns_libuv_vmt;
	ext->loop = loop;
	ext->mf   = *priv_getdns_context_mf(context);

	return getdns_context_set_eventloop(context, (getdns_eventloop *)ext);
}
