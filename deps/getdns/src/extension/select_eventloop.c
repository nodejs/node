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
#include "platform.h"
#include "extension/select_eventloop.h"

static uint64_t get_now_plus(uint64_t amount)
{
	struct timeval tv;
	uint64_t       now;

	if (gettimeofday(&tv, NULL)) {
		_getdns_perror("gettimeofday() failed");
		exit(EXIT_FAILURE);
	}
	now = tv.tv_sec * 1000000 + tv.tv_usec;

	return (now + amount * 1000) >= now
	      ? now + amount * 1000 : TIMEOUT_FOREVER;
}

static getdns_return_t
select_eventloop_schedule(getdns_eventloop *loop,
    int fd, uint64_t timeout, getdns_eventloop_event *event)
{
	_getdns_select_eventloop *select_loop  = (_getdns_select_eventloop *)loop;
	size_t i;

	DEBUG_SCHED( "%s(loop: %p, fd: %d, timeout: %"PRIu64", event: %p, FD_SETSIZE: %d)\n"
	        , __FUNC__, (void *)loop, fd, timeout, (void *)event, FD_SETSIZE);

	if (!loop || !event)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (fd >= (int)FD_SETSIZE) {
		DEBUG_SCHED( "ERROR: fd %d >= FD_SETSIZE: %d!\n"
		           , fd, FD_SETSIZE);
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	if (fd >= 0 && !(event->read_cb || event->write_cb)) {
		DEBUG_SCHED("WARNING: fd event without "
		            "read or write cb!\n");
		fd = -1;
	}
	if (fd >= 0) {
#if defined(SCHED_DEBUG) && SCHED_DEBUG
		if (select_loop->fd_events[fd]) {
			if (select_loop->fd_events[fd] == event) {
				DEBUG_SCHED("WARNING: Event %p not cleared "
				            "before being rescheduled!\n"
				           , (void *)select_loop->fd_events[fd]);
			} else {
				DEBUG_SCHED("ERROR: A different event is "
				            "already present at fd slot: %p!\n"
				           , (void *)select_loop->fd_events[fd]);
			}
		}
#endif
		select_loop->fd_events[fd] = event;
		select_loop->fd_timeout_times[fd] = get_now_plus(timeout);
		event->ev = (void *)(intptr_t)(fd + 1);
		DEBUG_SCHED( "scheduled read/write at %d\n", fd);
		return GETDNS_RETURN_GOOD;
	}
	if (!event->timeout_cb) {
		DEBUG_SCHED("ERROR: fd < 0 without timeout_cb!\n");
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	if (event->read_cb) {
		DEBUG_SCHED("ERROR: timeout event with read_cb! Clearing.\n");
		event->read_cb = NULL;
	}
	if (event->write_cb) {
		DEBUG_SCHED("ERROR: timeout event with write_cb! Clearing.\n");
		event->write_cb = NULL;
	}
	for (i = 0; i < MAX_TIMEOUTS; i++) {
		if (select_loop->timeout_events[i] == NULL) {
			select_loop->timeout_events[i] = event;
			select_loop->timeout_times[i] = get_now_plus(timeout);		
			event->ev = (void *)(intptr_t)(i + 1);
			DEBUG_SCHED( "scheduled timeout at %d\n", (int)i);
			return GETDNS_RETURN_GOOD;
		}
	}
	DEBUG_SCHED("ERROR: Out of timeout slots!\n");
	return GETDNS_RETURN_GENERIC_ERROR;
}

static getdns_return_t
select_eventloop_clear(getdns_eventloop *loop, getdns_eventloop_event *event)
{
	_getdns_select_eventloop *select_loop  = (_getdns_select_eventloop *)loop;
	ssize_t i;

	if (!loop || !event)
		return GETDNS_RETURN_INVALID_PARAMETER;

	DEBUG_SCHED( "%s(loop: %p, event: %p)\n", __FUNC__, (void *)loop, (void *)event);

	i = (intptr_t)event->ev - 1;
	if (i < 0 || i >= FD_SETSIZE) {
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	if (event->timeout_cb && !event->read_cb && !event->write_cb) {
#if defined(SCHED_DEBUG) && SCHED_DEBUG
		if (select_loop->timeout_events[i] != event)
			DEBUG_SCHED( "ERROR: Different/wrong event present at "
			             "timeout slot: %p!\n"
			           , (void *)select_loop->timeout_events[i]);
#endif
		select_loop->timeout_events[i] = NULL;
	} else {
#if defined(SCHED_DEBUG) && SCHED_DEBUG
		if (select_loop->fd_events[i] != event)
			DEBUG_SCHED( "ERROR: Different/wrong event present at "
			             "fd slot: %p!\n"
			           , (void *)select_loop->fd_events[i]);
#endif
		select_loop->fd_events[i] = NULL;
	}
	event->ev = NULL;
	return GETDNS_RETURN_GOOD;
}

static void
select_eventloop_cleanup(getdns_eventloop *loop)
{
	(void)loop;
}

static void
select_read_cb(int fd, getdns_eventloop_event *event)
{
#if !defined(SCHED_DEBUG) || !SCHED_DEBUG
	(void)fd;
#endif
	DEBUG_SCHED( "%s(fd: %d, event: %p)\n", __FUNC__, fd, (void *)event);
	event->read_cb(event->userarg);
}

static void
select_write_cb(int fd, getdns_eventloop_event *event)
{
#if !defined(SCHED_DEBUG) || !SCHED_DEBUG
	(void)fd;
#endif
	DEBUG_SCHED( "%s(fd: %d, event: %p)\n", __FUNC__, fd, (void *)event);
	event->write_cb(event->userarg);
}

static void
select_timeout_cb(int fd, getdns_eventloop_event *event)
{
#if !defined(SCHED_DEBUG) || !SCHED_DEBUG
	(void)fd;
#endif
	DEBUG_SCHED( "%s(fd: %d, event: %p)\n", __FUNC__, fd, (void *)event);
	event->timeout_cb(event->userarg);
}

static void
select_eventloop_run_once(getdns_eventloop *loop, int blocking)
{
	_getdns_select_eventloop *select_loop  = (_getdns_select_eventloop *)loop;

	fd_set   readfds, writefds;
	int      fd, max_fd = -1;
	uint64_t now, timeout = TIMEOUT_FOREVER;
	size_t   i;
	struct timeval tv;

	if (!loop)
		return;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	now = get_now_plus(0);

	for (i = 0; i < MAX_TIMEOUTS; i++) {
		if (!select_loop->timeout_events[i])
			continue;
		if (now > select_loop->timeout_times[i])
			select_timeout_cb(-1, select_loop->timeout_events[i]);
		else if (select_loop->timeout_times[i] < timeout)
			timeout = select_loop->timeout_times[i];
	}
	for (fd = 0; fd < (int)FD_SETSIZE; fd++) {
		if (!select_loop->fd_events[fd])
			continue;
		if (select_loop->fd_events[fd]->read_cb)
			FD_SET(fd, &readfds);
		if (select_loop->fd_events[fd]->write_cb)
			FD_SET(fd, &writefds);
		if (fd > max_fd)
			max_fd = fd;
		if (select_loop->fd_timeout_times[fd] < timeout)
			timeout = select_loop->fd_timeout_times[fd];
	}
	if (max_fd == -1 && timeout == TIMEOUT_FOREVER)
		return;

	if (! blocking || now > timeout) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	} else {
		tv.tv_sec  = (long)((timeout - now) / 1000000);
		tv.tv_usec = (long)((timeout - now) % 1000000);
	}
#ifdef USE_WINSOCK
	if (max_fd == -1) {
		if (timeout != TIMEOUT_FOREVER) {
			uint32_t timeout_ms = (tv.tv_usec / 1000) + (tv.tv_sec * 1000);
			Sleep(timeout_ms);
        	}
	} else {
#endif
	if (select(max_fd + 1, &readfds, &writefds, NULL,
	    (timeout == TIMEOUT_FOREVER ? NULL : &tv)) < 0) {
		if (_getdns_socketerror_wants_retry())
			return;

		DEBUG_SCHED("I/O error with select(): %s\n", _getdns_errnostr());
		return;
	}
#ifdef USE_WINSOCK
	}
#endif
	now = get_now_plus(0);
	for (fd = 0; fd < (int)FD_SETSIZE; fd++) {
		if (select_loop->fd_events[fd] &&
		    select_loop->fd_events[fd]->read_cb &&
		    FD_ISSET(fd, &readfds))
			select_read_cb(fd, select_loop->fd_events[fd]);

		if (select_loop->fd_events[fd] &&
		    select_loop->fd_events[fd]->write_cb &&
		    FD_ISSET(fd, &writefds))
			select_write_cb(fd, select_loop->fd_events[fd]);

		if (select_loop->fd_events[fd] &&
		    select_loop->fd_events[fd]->timeout_cb &&
		    now > select_loop->fd_timeout_times[fd])
			select_timeout_cb(fd, select_loop->fd_events[fd]);

		i = fd;
		if (select_loop->timeout_events[i] &&
		    select_loop->timeout_events[i]->timeout_cb &&
		    now > select_loop->timeout_times[i])
			select_timeout_cb(-1, select_loop->timeout_events[i]);
	}
}

static void
select_eventloop_run(getdns_eventloop *loop)
{
	_getdns_select_eventloop *select_loop  = (_getdns_select_eventloop *)loop;
	size_t        i;

	if (!loop)
		return;

	i = 0;
	while (i < MAX_TIMEOUTS) {
		if (select_loop->fd_events[i] || select_loop->timeout_events[i]) {
			select_eventloop_run_once(loop, 1);
			i = 0;
		} else {
			i++;
		}
	}
}

void
_getdns_select_eventloop_init(struct mem_funcs *mf, _getdns_select_eventloop *loop)
{
	static getdns_eventloop_vmt select_eventloop_vmt = {
		select_eventloop_cleanup,
		select_eventloop_schedule,
		select_eventloop_clear,
		select_eventloop_run,
		select_eventloop_run_once
	};
	(void) mf;
	(void) memset(loop, 0, sizeof(_getdns_select_eventloop));
	loop->loop.vmt = &select_eventloop_vmt;
}
