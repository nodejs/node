/**
 *
 * \file ub_loop.h
 * /brief Interface for the pluggable unbound event API
 *
 */

/*
 * Copyright (c) 2015, NLnet Labs, Verisign, Inc.
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

#ifndef UB_LOOP_H
#define UB_LOOP_H 

#include "config.h"
#ifdef HAVE_UNBOUND_EVENT_API
#include "getdns/getdns.h"
#include "getdns/getdns_extra.h"
#include "types-internal.h"
#include "debug.h"

#ifdef HAVE_UNBOUND_EVENT_H
# include <unbound-event.h>
#else
struct ub_event_base_vmt;
struct ub_event_base {
	unsigned long magic;
        struct ub_event_base_vmt* vmt;
};
# ifndef _UB_EVENT_PRIMITIVES
#  define _UB_EVENT_PRIMITIVES
struct ub_ctx* ub_ctx_create_ub_event(struct ub_event_base* base);
typedef void (*ub_event_callback_t)(void*, int, void*, int, int, char*);
int ub_resolve_event(struct ub_ctx* ctx, const char* name, int rrtype,
        int rrclass, void* mydata, ub_event_callback_t callback, int* async_id);
# endif
#endif

typedef struct _getdns_ub_loop {
	struct ub_event_base super;
	struct mem_funcs     mf;
	getdns_eventloop    *extension;
	int                  running;
#if defined(SCHED_DEBUG) && SCHED_DEBUG
	int                  n_events;
#endif
} _getdns_ub_loop;

void _getdns_ub_loop_init(_getdns_ub_loop *loop, struct mem_funcs *mf, getdns_eventloop *extension);

static inline int _getdns_ub_loop_enabled(_getdns_ub_loop *loop)
{ return loop->super.vmt ? 1 : 0; }

#endif /* HAVE_UNBOUND_EVENT_API */
#endif
/* ub_loop.h */
