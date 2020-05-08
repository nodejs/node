/*
 * \file select_eventloop.h
 * @brief Build in default eventloop extension that uses select.
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
#ifndef SELECT_EVENTLOOP_H_
#define SELECT_EVENTLOOP_H_
#include "config.h"
#include "getdns/getdns.h"
#include "getdns/getdns_extra.h"
#include "types-internal.h"

/* No more than select's capability queries can be outstanding,
 * The number of outstanding timeouts should be less or equal then
 * the number of outstanding queries, so MAX_TIMEOUTS equal to
 * FD_SETSIZE should be safe.
 */
#define MAX_TIMEOUTS FD_SETSIZE

/* Eventloop based on select */
typedef struct _getdns_select_eventloop {
	getdns_eventloop        loop;
	getdns_eventloop_event *fd_events[FD_SETSIZE];
	uint64_t                fd_timeout_times[FD_SETSIZE];
	getdns_eventloop_event *timeout_events[MAX_TIMEOUTS];
	uint64_t                timeout_times[MAX_TIMEOUTS];
} _getdns_select_eventloop;


void
_getdns_select_eventloop_init(struct mem_funcs *mf, _getdns_select_eventloop *loop);

#endif
