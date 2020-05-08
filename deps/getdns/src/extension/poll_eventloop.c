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

#include "util-internal.h"
#include "platform.h"
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include "extension/poll_eventloop.h"
#include "debug.h"

enum { init_fd_events_capacity = 64
     , init_to_events_capacity = 64 };

static void *get_to_event(_getdns_poll_eventloop *loop,
    getdns_eventloop_event *event, uint64_t timeout_time)
{
	if (loop->to_events_free == loop->to_events_capacity) {
		if (loop->to_events_free) {
			_getdns_poll_event *to_events = GETDNS_XREALLOC(
			    loop->mf, loop->to_events, _getdns_poll_event,
			    loop->to_events_free * 2);
			if (!to_events)
				return NULL;
			(void) memset(&to_events[loop->to_events_free],
			    0, sizeof(_getdns_poll_event)
			     * loop->to_events_free);

			loop->to_events_capacity = loop->to_events_free * 2;
			loop->to_events = to_events;
		} else {
			if (!(loop->to_events = GETDNS_XMALLOC(loop->mf,
			    _getdns_poll_event, init_to_events_capacity)))
				return NULL;

			(void) memset(loop->to_events, 0,
			    sizeof(_getdns_poll_event)
			    * init_to_events_capacity);

			loop->to_events_capacity = init_to_events_capacity;
		}
	}
	loop->to_events[loop->to_events_free].event = event;
	loop->to_events[loop->to_events_free].timeout_time = timeout_time;
	loop->to_events_n_used++;
	return (void *) (intptr_t) (++loop->to_events_free);
}

static void *get_fd_event(_getdns_poll_eventloop *loop, int fd,
    getdns_eventloop_event *event, uint64_t timeout_time)
{
	size_t i;

	if (loop->fd_events_free == loop->fd_events_capacity) {
		if (loop->fd_events_free) {
			_getdns_poll_event *fd_events = GETDNS_XREALLOC(
			    loop->mf, loop->fd_events, _getdns_poll_event,
			    loop->fd_events_free * 2);
			struct pollfd *pfds = GETDNS_XREALLOC(
			    loop->mf, loop->pfds, struct pollfd,
			    loop->fd_events_free * 2);

			if (!fd_events || !pfds) {
				if (fd_events)
					GETDNS_FREE(loop->mf, fd_events);
				if (pfds)
					GETDNS_FREE(loop->mf, pfds);
				return NULL;
			}
			(void) memset(&fd_events[loop->fd_events_free],
			    0, sizeof(_getdns_poll_event)
			     * loop->fd_events_free);
			for ( i = loop->fd_events_free
			    ; i < loop->fd_events_free * 2
			    ; i++) {
				pfds[i].fd = -1;
				pfds[i].events  = 0;
				pfds[i].revents = 0;
			}
			loop->fd_events_capacity = loop->fd_events_free * 2;
			loop->fd_events = fd_events;
			loop->pfds = pfds;
		} else {
			if (!(loop->fd_events = GETDNS_XMALLOC(loop->mf,
			    _getdns_poll_event, init_fd_events_capacity)) ||
			    !(loop->pfds = GETDNS_XMALLOC(loop->mf,
			    struct pollfd, init_fd_events_capacity))) {
				GETDNS_NULL_FREE(loop->mf, loop->fd_events);
				return NULL;
			}
			(void) memset(loop->fd_events, 0,
			    sizeof(_getdns_poll_event)
			    * init_fd_events_capacity);
			for (i = 0; i < init_fd_events_capacity; i++) {
				loop->pfds[i].fd = -1;
				loop->pfds[i].events  = 0;
				loop->pfds[i].revents = 0;
			}
			loop->fd_events_capacity = init_fd_events_capacity;
		}
	}
	loop->pfds[loop->fd_events_free].fd = fd;
	loop->pfds[loop->fd_events_free].events = 0;
	if (event->read_cb)
		loop->pfds[loop->fd_events_free].events |= POLLIN;
	if (event->write_cb)
		loop->pfds[loop->fd_events_free].events |= POLLOUT;
	loop->fd_events[loop->fd_events_free].event = event;
	loop->fd_events[loop->fd_events_free].timeout_time = timeout_time;
	loop->fd_events_n_used++;
	return (void *) (intptr_t) (++loop->fd_events_free);
}

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
poll_eventloop_schedule(getdns_eventloop *loop,
    int fd, uint64_t timeout, getdns_eventloop_event *event)
{
	_getdns_poll_eventloop *poll_loop  = (_getdns_poll_eventloop *)loop;

	DEBUG_SCHED( "%s(loop: %p, fd: %d, timeout: %"PRIu64", event: %p)\n"
	        , __FUNC__, (void *)loop, fd, timeout, (void *)event);

	if (!loop || !event)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (fd >= 0 && !(event->read_cb || event->write_cb)) {
		DEBUG_SCHED("WARNING: fd event without "
		            "read or write cb!\n");
		fd = -1;
	}
	if (fd >= 0) {
		if (!(event->ev = get_fd_event(
		    poll_loop, fd, event, get_now_plus(timeout)))) {
			DEBUG_SCHED("ERROR: scheduled read/write slots!\n");
			return GETDNS_RETURN_GENERIC_ERROR;
		}
		DEBUG_SCHED( "scheduled read/write at for %d at %p\n"
		           , fd, (void *)event->ev);
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
	if (!(event->ev = get_to_event(poll_loop, event, get_now_plus(timeout)))) {
		DEBUG_SCHED("ERROR: Out of timeout slots!\n");
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	DEBUG_SCHED("scheduled timeout at slot %p\n", (void *)event->ev);
	return GETDNS_RETURN_GOOD;
}

static getdns_return_t
poll_eventloop_clear(getdns_eventloop *loop, getdns_eventloop_event *event)
{
	_getdns_poll_eventloop *poll_loop  = (_getdns_poll_eventloop *)loop;

	if (!loop || !event)
		return GETDNS_RETURN_INVALID_PARAMETER;

	DEBUG_SCHED( "%s(loop: %p, event: %p)\n", __FUNC__, (void *)loop, (void *)event);

	if (!event->ev)
		return GETDNS_RETURN_GOOD;

	else if (event->timeout_cb && !event->read_cb && !event->write_cb) {
		size_t i = ((size_t) (intptr_t) event->ev) - 1;

		/* This may happen with full recursive synchronous requests
		 * with the unbound pluggable event API, because the default
		 * poll_eventloop is temporarily replaced by a poll_eventloop
		 * used only in synchronous calls. When the synchronous request
		 * had an answer, the poll_eventloop for the synchronous is
		 * cleaned, however it could still have outstanding events.
		 */
		if (i >= poll_loop->to_events_capacity ||
		    poll_loop->to_events[i].event != event) {
			event->ev = NULL;
			DEBUG_SCHED( "ERROR: Event mismatch %p\n", (void *)event->ev);
			return GETDNS_RETURN_GENERIC_ERROR;
		}
		poll_loop->to_events[i].event = NULL;
		if (--poll_loop->to_events_n_used == 0) {
			poll_loop->to_events_free = 0;
		}
		DEBUG_SCHED( "cleared timeout at slot %p\n", (void *)event->ev);
	} else {
		size_t i = ((size_t) (intptr_t) event->ev) - 1;

		/* This may happen with full recursive synchronous requests
		 * with the unbound pluggable event API, because the default
		 * poll_eventloop is temporarily replaced by a poll_eventloop
		 * used only in synchronous calls. When the synchronous request
		 * had an answer, the poll_eventloop for the synchronous is
		 * cleaned, however it could still have outstanding events.
		 */
		if (i >= poll_loop->fd_events_capacity ||
		    poll_loop->fd_events[i].event != event) {
			event->ev = NULL;
			DEBUG_SCHED( "ERROR: Event mismatch %p\n", (void *)event->ev);
			return GETDNS_RETURN_GENERIC_ERROR;
		}
		poll_loop->fd_events[i].event = NULL;
		if (--poll_loop->fd_events_n_used == 0) {
			poll_loop->fd_events_free = 0;
		}
		DEBUG_SCHED( "cleared read/write for %d at slot %p\n"
		           , poll_loop->pfds[i].fd, (void *)event->ev);
		poll_loop->pfds[i].fd = -1; /* Not necessary, but to be sure */
	}
	event->ev = NULL;
	return GETDNS_RETURN_GOOD;
}

static void
poll_eventloop_cleanup(getdns_eventloop *loop)
{
	_getdns_poll_eventloop *poll_loop  = (_getdns_poll_eventloop *)loop;
	struct mem_funcs *mf = &poll_loop->mf;

	GETDNS_NULL_FREE(*mf, poll_loop->pfds);
	if (poll_loop->fd_events) {
		GETDNS_FREE(*mf, poll_loop->fd_events);
		poll_loop->fd_events = NULL;
		poll_loop->fd_events_capacity = 0;
		poll_loop->fd_events_free     = 0;
		poll_loop->fd_events_n_used   = 0;
	}
	if (poll_loop->to_events) {
		GETDNS_FREE(*mf, poll_loop->to_events);
		poll_loop->to_events = NULL;
		poll_loop->to_events_capacity = 0;
		poll_loop->to_events_free     = 0;
		poll_loop->to_events_n_used   = 0;
	}
}

static void
poll_read_cb(int fd, getdns_eventloop_event *event)
{
#if !defined(SCHED_DEBUG) || !SCHED_DEBUG
	(void)fd;
#endif
	DEBUG_SCHED( "%s(fd: %d, event: %p)\n", __FUNC__, fd, (void *)event);
	if (event && event->read_cb)
		event->read_cb(event->userarg);
}

static void
poll_write_cb(int fd, getdns_eventloop_event *event)
{
#if !defined(SCHED_DEBUG) || !SCHED_DEBUG
	(void)fd;
#endif
	DEBUG_SCHED( "%s(fd: %d, event: %p)\n", __FUNC__, fd, (void *)event);
	if (event && event->write_cb)
		event->write_cb(event->userarg);
}

static void
poll_timeout_cb(getdns_eventloop_event *event)
{
	DEBUG_SCHED( "%s(event: %p)\n", __FUNC__, (void *)event);
	if (event && event->timeout_cb)
		event->timeout_cb(event->userarg);
}

static void
poll_eventloop_run_once(getdns_eventloop *loop, int blocking)
{
	_getdns_poll_eventloop *poll_loop  = (_getdns_poll_eventloop *)loop;
	uint64_t now, timeout = TIMEOUT_FOREVER;
	size_t  i = 0, j;
	int poll_timeout = 0;

	if (!loop)
		return;

	now = get_now_plus(0);

	for (i = 0, j = 0; i < poll_loop->to_events_free; i++, j++) {
		while (poll_loop->to_events[i].event == NULL) {
			if (++i == poll_loop->to_events_free) {
				poll_loop->to_events_free = j;
				break;
			}
		}
		if (j < i) {
			if (j >= poll_loop->to_events_free)
				break;
			poll_loop->to_events[j] = poll_loop->to_events[i];
			poll_loop->to_events[i].event = NULL;
			poll_loop->to_events[j].event->ev =
			    (void *) (intptr_t) (j + 1);
		}
		if (poll_loop->to_events[j].timeout_time < now)
			poll_timeout_cb(poll_loop->to_events[j].event);
	}
	for (i = 0, j = 0; i < poll_loop->to_events_free; i++, j++) {
		while (poll_loop->to_events[i].event == NULL) {
			if (++i == poll_loop->to_events_free) {
				poll_loop->to_events_free = j;
				break;
			}
		}
		if (j < i) {
			if (j >= poll_loop->to_events_free)
				break;
			poll_loop->to_events[j] = poll_loop->to_events[i];
			poll_loop->to_events[i].event = NULL;
			poll_loop->to_events[j].event->ev =
			    (void *) (intptr_t) (j + 1);
		}
		if (poll_loop->to_events[j].timeout_time < timeout)
			timeout = poll_loop->to_events[j].timeout_time;
	}
	if ((timeout == TIMEOUT_FOREVER) && (poll_loop->fd_events_free == 0))
		return;

	for (i = 0, j = 0; i < poll_loop->fd_events_free; i++, j++) {
		while (poll_loop->fd_events[i].event == NULL) {
			if (++i == poll_loop->fd_events_free) {
				poll_loop->fd_events_free = j;
				break;
			}
		}
		if (j < i) {
			if (j >= poll_loop->fd_events_free)
				break;
			poll_loop->fd_events[j] = poll_loop->fd_events[i];
			poll_loop->fd_events[i].event = NULL;
			poll_loop->fd_events[j].event->ev =
			    (void *) (intptr_t) (j + 1);
			poll_loop->pfds[j] = poll_loop->pfds[i];
			poll_loop->pfds[i].fd = -1;
		}
		if (poll_loop->fd_events[j].timeout_time < timeout)
			timeout = poll_loop->fd_events[j].timeout_time;
	}

	if (timeout == TIMEOUT_FOREVER) {
		poll_timeout = -1;

	} else if (! blocking || now > timeout) {
		poll_timeout = 0;
	} else {
		/* turn microseconds into milliseconds */
		poll_timeout = (timeout - now) / 1000;
	}
	DEBUG_SCHED( "poll(fd_free: %d, fd_used: %d, to_free: %d, to_used: %d, timeout: %d)\n"
	           , (int)poll_loop->fd_events_free, (int)poll_loop->fd_events_n_used
	           , (int)poll_loop->to_events_free, (int)poll_loop->to_events_n_used
		   , poll_timeout
		   );
#ifdef USE_WINSOCK
    if (poll_loop->fd_events_free == 0)
    {
        Sleep(poll_timeout);
    } else
#endif
	if (_getdns_poll(poll_loop->pfds, poll_loop->fd_events_free, poll_timeout) < 0) {
		if (_getdns_socketerror() == _getdns_EAGAIN ||
		    _getdns_socketerror() == _getdns_EINTR )
			return;

		DEBUG_SCHED("I/O error with poll(): %s\n", _getdns_errnostr());
		return;
	}
	now = get_now_plus(0);

	for (i = 0, j = 0; i < poll_loop->fd_events_free; i++, j++) {
		while (poll_loop->fd_events[i].event == NULL) {
			if (++i == poll_loop->fd_events_free) {
				poll_loop->fd_events_free = j;
				break;
			}
		}
		if (j < i) {
			if (j >= poll_loop->fd_events_free)
				break;
			poll_loop->fd_events[j] = poll_loop->fd_events[i];
			poll_loop->fd_events[i].event = NULL;
			poll_loop->fd_events[j].event->ev =
			    (void *) (intptr_t) (j + 1);
			poll_loop->pfds[j] = poll_loop->pfds[i];
			poll_loop->pfds[i].fd = -1;
		}
		if (poll_loop->fd_events[j].event->write_cb &&
		    poll_loop->pfds[j].revents & (POLLOUT|POLLERR|POLLHUP|POLLNVAL))
			poll_write_cb( poll_loop->pfds[j].fd
			             , poll_loop->fd_events[j].event);

		if (poll_loop->fd_events[j].event &&
		    poll_loop->fd_events[j].event->read_cb &&
		    poll_loop->pfds[j].revents & (POLLIN|POLLERR|POLLHUP|POLLNVAL))
			poll_read_cb( poll_loop->pfds[j].fd
			            , poll_loop->fd_events[j].event);
	}
	for (i = 0, j = 0; i < poll_loop->fd_events_free; i++, j++) {
		while (poll_loop->fd_events[i].event == NULL) {
			if (++i == poll_loop->fd_events_free) {
				poll_loop->fd_events_free = j;
				break;
			}
		}
		if (j < i) {
			if (j >= poll_loop->fd_events_free)
				break;
			poll_loop->fd_events[j] = poll_loop->fd_events[i];
			poll_loop->fd_events[i].event = NULL;
			poll_loop->fd_events[j].event->ev =
			    (void *) (intptr_t) (j + 1);
			poll_loop->pfds[j] = poll_loop->pfds[i];
			poll_loop->pfds[i].fd = -1;
		}
		if (poll_loop->fd_events[j].timeout_time < now)
			poll_timeout_cb(poll_loop->fd_events[j].event);
	}

	for (i = 0, j = 0; i < poll_loop->to_events_free; i++, j++) {
		while (poll_loop->to_events[i].event == NULL) {
			if (++i == poll_loop->to_events_free) {
				poll_loop->to_events_free = j;
				break;
			}
		}
		if (j < i) {
			if (j >= poll_loop->to_events_free)
				break;
			poll_loop->to_events[j] = poll_loop->to_events[i];
			poll_loop->to_events[i].event = NULL;
			poll_loop->to_events[j].event->ev =
			    (void *) (intptr_t) (j + 1);
		}
		if (poll_loop->to_events[j].timeout_time < now)
			poll_timeout_cb(poll_loop->to_events[j].event);
	}
}

static void
poll_eventloop_run(getdns_eventloop *loop)
{
	_getdns_poll_eventloop *poll_loop  = (_getdns_poll_eventloop *)loop;

	if (!loop)
		return;

	/* keep going until all the events are cleared */
	while (poll_loop->fd_events_n_used || poll_loop->to_events_n_used) {
		poll_eventloop_run_once(loop, 1);
	}
}

void
_getdns_poll_eventloop_init(struct mem_funcs *mf, _getdns_poll_eventloop *loop)
{
	static getdns_eventloop_vmt poll_eventloop_vmt = {
		poll_eventloop_cleanup,
		poll_eventloop_schedule,
		poll_eventloop_clear,
		poll_eventloop_run,
		poll_eventloop_run_once
	};

	loop->loop.vmt = &poll_eventloop_vmt;
	loop->mf = *mf;

	loop->to_events_capacity = init_to_events_capacity;
	if ((loop->to_events = GETDNS_XMALLOC(
	    *mf, _getdns_poll_event, init_to_events_capacity)))
		(void) memset(loop->to_events, 0,
		    sizeof(_getdns_poll_event) * init_to_events_capacity);
	else
		loop->to_events_capacity = 0;
	loop->to_events_free = 0;
	loop->to_events_n_used = 0;

	loop->fd_events_capacity = init_fd_events_capacity;
	if ((loop->fd_events = GETDNS_XMALLOC(
	    *mf, _getdns_poll_event, init_fd_events_capacity)) &&
	    (loop->pfds = GETDNS_XMALLOC(
	    *mf, struct pollfd, init_fd_events_capacity))) {
		size_t i;

		(void) memset(loop->fd_events, 0,
		    sizeof(_getdns_poll_event) * init_fd_events_capacity);
		for (i = 0; i < init_fd_events_capacity; i++) {
			loop->pfds[i].fd = -1;
			loop->pfds[i].events  = 0;
			loop->pfds[i].revents = 0;
		}
	} else {
		loop->fd_events_capacity = 0;
		if (loop->fd_events) {
			GETDNS_FREE(*mf, loop->fd_events);
			loop->fd_events = NULL;
		}
	}
	loop->fd_events_free = 0;
	loop->fd_events_n_used = 0;
}

