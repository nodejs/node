/**
 *
 * /brief getdns core functions for synchronous use
 *
 * Originally taken from the getdns API description pseudo implementation.
 *
 */

/*
 * Copyright (c) 2013, NLnet Labs, Verisign, Inc.
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

#include <string.h>
#include "getdns/getdns.h"
#include "config.h"
#include "context.h"
#include "general.h"
#include "types-internal.h"
#include "util-internal.h"
#include "dnssec.h"

#include "stub.h"
#include "gldns/wire2str.h"

typedef struct getdns_sync_data {
#ifdef HAVE_LIBUNBOUND
	getdns_eventloop_event ub_event;
#endif
	getdns_context        *context;
	int                    to_run;
	getdns_dict           *response;
} getdns_sync_data;

static getdns_return_t
getdns_sync_data_init(getdns_context *context, getdns_sync_data *data)
{
#ifdef HAVE_LIBUNBOUND
	getdns_eventloop *ext = &context->sync_eventloop.loop;
#endif

	data->context  = context;
	data->to_run   = 1;
	data->response = NULL;

#ifdef HAVE_LIBUNBOUND
#  ifndef USE_WINSOCK
	data->ub_event.userarg    = data->context;
	data->ub_event.read_cb    = _getdns_context_ub_read_cb;
	data->ub_event.write_cb   = NULL;
	data->ub_event.timeout_cb = NULL;
	data->ub_event.ev         = NULL;
#  endif
#  ifdef HAVE_UNBOUND_EVENT_API
	if (_getdns_ub_loop_enabled(&context->ub_loop)) {
		context->ub_loop.extension = ext;
	} else 
#  endif
#  ifndef USE_WINSOCK
		return ext->vmt->schedule(ext, ub_fd(context->unbound_ctx),
		    TIMEOUT_FOREVER, &data->ub_event);
#  else
		/* No sync full recursion requests on windows without 
		 * UNBOUND_EVENT_API because ub_fd() doesn't work on windows.
		 */
#   ifdef HAVE_UNBOUND_EVENT_API
		{ ; } /* pass */
#   endif
#  endif
#endif
	return GETDNS_RETURN_GOOD;
}

static void
getdns_sync_data_cleanup(getdns_sync_data *data)
{
	size_t i;
	getdns_context *ctxt = data->context;
	getdns_upstream *upstream;

#if defined(HAVE_LIBUNBOUND) && !defined(USE_WINSOCK)
#  ifdef HAVE_UNBOUND_EVENT_API
	if (_getdns_ub_loop_enabled(&data->context->ub_loop)) {
		data->context->ub_loop.extension = data->context->extension;
	} else
#  endif
		data->context->sync_eventloop.loop.vmt->clear(
		    &data->context->sync_eventloop.loop, &data->ub_event);
#endif
	/* If statefull upstream have events scheduled against the sync loop,
	 * reschedule against the async loop.
	 */
	if (!ctxt->upstreams)
		; /* pass */

	else for (i = 0; i < ctxt->upstreams->count; i++) {
		upstream = &ctxt->upstreams->upstreams[i];
		if (upstream->loop != &data->context->sync_eventloop.loop)
			continue;
		GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
		if (upstream->event.timeout_cb &&
		    (  upstream->conn_state != GETDNS_CONN_OPEN
		    || upstream->keepalive_timeout == 0)) {
			(*upstream->event.timeout_cb)(upstream->event.userarg);
			upstream->event.timeout_cb = NULL;
		}
		upstream->loop = data->context->extension;
		upstream->is_sync_loop = 0;

		if (  upstream->event.read_cb || upstream->event.write_cb
		   || upstream->event.timeout_cb) {
			GETDNS_SCHEDULE_EVENT(upstream->loop,
			    (  upstream->event.read_cb
			    || upstream->event.write_cb ? upstream->fd : -1),
			    (  upstream->event.timeout_cb
			    ?  upstream->keepalive_timeout : TIMEOUT_FOREVER ),
			    &upstream->event);
		}
	}
}

static void
getdns_sync_loop_run(getdns_sync_data *data)
{
	while (data->to_run)
		data->context->sync_eventloop.loop.vmt->run_once(
		    &data->context->sync_eventloop.loop, 1);
}

static void
getdns_sync_cb(getdns_context *context, getdns_callback_type_t callback_type,
    getdns_dict *response, void *userarg, getdns_transaction_t transaction_id)
{
	getdns_sync_data *data = (getdns_sync_data *)userarg;
	(void)context; (void)callback_type; (void)transaction_id;

	assert(data);

	data->response = response;
	data->to_run = 0;
}

getdns_return_t
getdns_general_sync(getdns_context *context, const char *name,
    uint16_t request_type, const getdns_dict *extensions, getdns_dict **response)
{
	getdns_sync_data data;
	getdns_return_t r;

	if (!context || !name || !response)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = getdns_sync_data_init(context, &data)))
		return r;

	if ((r = _getdns_general_loop(context, &context->sync_eventloop.loop,
	    name, request_type, extensions, &data, NULL, getdns_sync_cb, NULL))) {

		getdns_sync_data_cleanup(&data);
		return r;
	}
	getdns_sync_loop_run(&data);
	getdns_sync_data_cleanup(&data);
	
	return (*response = data.response) ?
	    GETDNS_RETURN_GOOD : GETDNS_RETURN_GENERIC_ERROR;
}

getdns_return_t
getdns_address_sync(getdns_context *context, const char *name,
    const getdns_dict *extensions, getdns_dict **response)
{
	getdns_sync_data data;
	getdns_return_t r;

	if (!context || !name || !response)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = getdns_sync_data_init(context, &data)))
		return r;

	if ((r = _getdns_address_loop(context, &context->sync_eventloop.loop,
	    name, extensions, &data, NULL, getdns_sync_cb))) {

		getdns_sync_data_cleanup(&data);
		return r;
	}
	getdns_sync_loop_run(&data);
	getdns_sync_data_cleanup(&data);
	
	return (*response = data.response) ?
	    GETDNS_RETURN_GOOD : GETDNS_RETURN_GENERIC_ERROR;
}

getdns_return_t
getdns_hostname_sync(getdns_context *context, const getdns_dict *address,
    const getdns_dict *extensions, getdns_dict **response)
{
	getdns_sync_data data;
	getdns_return_t r;

	if (!context || !address || !response)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = getdns_sync_data_init(context, &data)))
		return r;

	if ((r = _getdns_hostname_loop(context, &context->sync_eventloop.loop,
	    address, extensions, &data, NULL, getdns_sync_cb))) {

		getdns_sync_data_cleanup(&data);
		return r;
	}
	getdns_sync_loop_run(&data);
	getdns_sync_data_cleanup(&data);
	
	return (*response = data.response) ?
	    GETDNS_RETURN_GOOD : GETDNS_RETURN_GENERIC_ERROR;
}

getdns_return_t
getdns_service_sync(getdns_context *context, const char *name,
    const getdns_dict *extensions, getdns_dict **response)
{
	getdns_sync_data data;
	getdns_return_t r;

	if (!context || !name || !response)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = getdns_sync_data_init(context, &data)))
		return r;

	if ((r = _getdns_service_loop(context, &context->sync_eventloop.loop,
	    name, extensions, &data, NULL, getdns_sync_cb))) {

		getdns_sync_data_cleanup(&data);
		return r;
	}
	getdns_sync_loop_run(&data);
	getdns_sync_data_cleanup(&data);
	
	return (*response = data.response) ?
	    GETDNS_RETURN_GOOD : GETDNS_RETURN_GENERIC_ERROR;
}

/* getdns_core_sync.c */
