/**
 *
 * /brief function for stub resolving
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

#include "config.h"

/* Intercept and do not sent out COM DS queries with TLS
 * For debugging purposes only. Never commit with this turned on.
 */
#define INTERCEPT_COM_DS 0

#include "debug.h"
#include <fcntl.h>
#include "stub.h"
#include "gldns/gbuffer.h"
#include "gldns/pkthdr.h"
#include "gldns/rrdef.h"
#include "gldns/str2wire.h"
#include "gldns/wire2str.h"
#include "rr-iter.h"
#include "context.h"
#include "util-internal.h"
#include "platform.h"
#include "general.h"
#include "pubkey-pinning.h"

/* WSA TODO: 
 * STUB_TCP_RETRY added to deal with edge triggered event loops (versus
 * level triggered).  See also lines containing WSA TODO below...
 */
#define STUB_TRY_AGAIN_LATER -24 /* EMFILE, i.e. Out of OS resources */
#define STUB_NO_AUTH -8 /* Existing TLS connection is not authenticated */
#define STUB_CONN_GONE -7 /* Connection has failed, clear queue*/
#define STUB_TCP_RETRY -6
#define STUB_OUT_OF_OPTIONS -5 /* upstream options exceeded MAXIMUM_UPSTREAM_OPTION_SPACE */
#define STUB_SETUP_ERROR -4
#define STUB_TCP_MORE_TO_READ -3
#define STUB_TCP_MORE_TO_WRITE -3
#define STUB_TCP_ERROR -2

/* Don't currently have access to the context whilst doing handshake */
#define MIN_TLS_HS_TIMEOUT 2500
#define MAX_TLS_HS_TIMEOUT 7500
/* Arbritray number of message for EDNS keepalive resend*/
#define EDNS_KEEPALIVE_RESEND 5

static time_t secret_rollover_time = 0;
static uint32_t secret = 0;
static uint32_t prev_secret = 0;

static void upstream_read_cb(void *userarg);
static void upstream_write_cb(void *userarg);
static void upstream_idle_timeout_cb(void *userarg);
static void upstream_schedule_netreq(getdns_upstream *upstream, 
                                     getdns_network_req *netreq);
static void upstream_reschedule_events(getdns_upstream *upstream);
static int  upstream_working_ok(getdns_upstream *upstream);
static int  upstream_auth_status_ok(getdns_upstream *upstream, 
                                    getdns_network_req *netreq);
static int  upstream_connect(getdns_upstream *upstream, 
                             getdns_transport_list_t transport,
                             getdns_dns_req *dnsreq);
static int  fallback_on_write(getdns_network_req *netreq);

static void stub_timeout_cb(void *userarg);
uint64_t _getdns_get_time_as_uintt64();
/*****************************/
/* General utility functions */
/*****************************/

static void
rollover_secret()
{
	time_t now = 0;

	/* Create and roll server secrets */
	if (time(&now) <= secret_rollover_time)
		return;

	/* Remember previous secret, in to keep answering on rollover
	 * boundary with old cookie.
	 */
	prev_secret = secret;
	secret = arc4random();

	/* Next rollover over EDNS_COOKIE_ROLLOVER_TIME with 30% jitter,
	 * I.e. some offset + or - 15% of the future point in time.
	 */
	secret_rollover_time = now + (EDNS_COOKIE_ROLLOVER_TIME / 20 * 17)
		 + arc4random_uniform(EDNS_COOKIE_ROLLOVER_TIME / 10 * 3);
}

static void
calc_new_cookie(getdns_upstream *upstream, uint8_t *cookie)
{
        unsigned char md_value[GETDNS_TLS_MAX_DIGEST_LENGTH];
        size_t md_len;
        size_t i;
        sa_family_t af = upstream->addr.ss_family;
        void *sa_addr = ((struct sockaddr*)&upstream->addr)->sa_data;
	size_t addr_len = ( af == AF_INET6 ? sizeof(struct sockaddr_in6)
	                  : af == AF_INET  ? sizeof(struct sockaddr_in)
	                  : 0 ) - sizeof(sa_family_t);

	_getdns_tls_cookie_sha256(secret, sa_addr, addr_len, md_value, &md_len);

        (void) memset(cookie, 0, 8);
        for (i = 0; i < md_len; i++)
                cookie[i % 8] ^= md_value[i];
}

static getdns_return_t
attach_edns_client_subnet_private(getdns_network_req *req)
{
	/* see https://tools.ietf.org/html/rfc7871#section-7.1.2
	 * all-zeros is a request to not leak the data further:
	 * A two byte FAMILY field is a SHOULD even when SOURCE
	 * and SCOPE are 0.
	 * "\x00\x02"  FAMILY: 2 for IPv6 upstreams in network byte order, or
	 * "\x00\x01"  FAMILY: 1 for IPv4 upstreams in network byte order, then:
	 * "\x00"  SOURCE PREFIX-LENGTH: 0
	 * "\x00";  SCOPE PREFIX-LENGTH: 0
	 */
	return _getdns_network_req_add_upstream_option(
	    req, GLDNS_EDNS_CLIENT_SUBNET, 4,
	    ( req->upstream->addr.ss_family == AF_INET6
	    ?  "\x00\x02\x00\x00" : "\x00\x01\x00\x00" ));
}

static getdns_return_t
attach_edns_keepalive(getdns_network_req *req)
{
    /* Client always sends length 0, omits the timeout */
	return _getdns_network_req_add_upstream_option(req,
						       GLDNS_EDNS_KEEPALIVE,
						       0, NULL);
}

static getdns_return_t
attach_edns_cookie(getdns_network_req *req)
{
	getdns_upstream *upstream = req->upstream;
	uint16_t sz;
	void* val;
	uint8_t buf[8 + 32]; /* server cookies can be no larger than 32 bytes */
	rollover_secret();

	if (!upstream->has_client_cookie) {
		calc_new_cookie(upstream, upstream->client_cookie);
		upstream->secret = secret;
		upstream->has_client_cookie = 1;

		sz = 8;
		val = upstream->client_cookie;
	} else if (upstream->secret != secret) {
		memcpy( upstream->prev_client_cookie
		      , upstream->client_cookie, 8);
		upstream->has_prev_client_cookie = 1;
		calc_new_cookie(upstream, upstream->client_cookie);
		upstream->secret = secret;

		sz = 8;
		val = upstream->client_cookie;
	} else if (!upstream->has_server_cookie) {
		sz = 8;
		val = upstream->client_cookie;
	} else {
		sz = 8 + upstream->server_cookie_len;
		memcpy(buf, upstream->client_cookie, 8);
		memcpy(buf+8, upstream->server_cookie, upstream->server_cookie_len);
		val = buf;
	}
	return _getdns_network_req_add_upstream_option(req, EDNS_COOKIE_OPCODE, sz, val);

}

/* Will find a matching OPT RR, but leaves the caller to validate it
 *
 * Returns 2 when found
 *         0 when not found
 *     and 1 on FORMERR
 */
static int
match_edns_opt_rr(uint16_t code, uint8_t *response, size_t response_len,
                  const uint8_t **position, uint16_t *option_len)
{
	_getdns_rr_iter rr_iter_storage, *rr_iter;
	const uint8_t *pos;
	uint16_t rdata_len, opt_code = 0, opt_len = 0;

	/* Search for the OPT RR (if any) */
	for ( rr_iter = _getdns_rr_iter_init(&rr_iter_storage
	                                        , response, response_len)
	    ; rr_iter
	    ; rr_iter = _getdns_rr_iter_next(rr_iter)) {

		if (_getdns_rr_iter_section(rr_iter) != SECTION_ADDITIONAL)
			continue;

		if (gldns_read_uint16(rr_iter->rr_type) != GETDNS_RRTYPE_OPT)
			continue;

		break;
	}
	if (! rr_iter)
		return 0; /* No OPT, no cookie */

	pos = rr_iter->rr_type + 8;

#if defined(STUB_DEBUG) && STUB_DEBUG
	char str_spc[8192], *str = str_spc;
	size_t str_len = sizeof(str_spc);
	uint8_t *data = (uint8_t *)rr_iter->pos;
	size_t data_len = rr_iter->nxt - rr_iter->pos;
	(void) gldns_wire2str_rr_scan(
	    &data, &data_len, &str, &str_len, (uint8_t *)rr_iter->pkt, rr_iter->pkt_end - rr_iter->pkt, NULL);
	DEBUG_STUB("%s %-35s: OPT RR: %s",
	           STUB_DEBUG_READ, __FUNC__, str_spc);
#endif

	/* OPT found, now search for the specified option */
	if (pos + 2 > rr_iter->nxt)
		return 1; /* FORMERR */

	rdata_len = gldns_read_uint16(pos); pos += 2;
	if (pos + rdata_len > rr_iter->nxt)
		return 1; /* FORMERR */

	while (pos < rr_iter->nxt) {
		opt_code = gldns_read_uint16(pos); pos += 2;
		opt_len  = gldns_read_uint16(pos); pos += 2;
		if (pos + opt_len > rr_iter->nxt)
			return 1; /* FORMERR */
		if (opt_code == code)
			break;
		pos += opt_len; /* Skip unknown options */
	}
	if (pos >= rr_iter->nxt || opt_code != code)
		return 0; /* Everything OK, just no cookie found. */
	*position = pos;
	*option_len = opt_len;
	return 2;
}

/* TODO: Test combinations of EDNS0 options*/
static int
match_and_process_server_cookie(
    getdns_upstream *upstream, uint8_t *response, size_t response_len) 
{
	const uint8_t *position = NULL;
	uint16_t option_len = 0;
	int found = match_edns_opt_rr(EDNS_COOKIE_OPCODE, response, 
	                              response_len, &position, &option_len);
	if (found != 2)
		return found;

	if (option_len < 16 || option_len > 40)
		return 1; /* FORMERR */

	if (!upstream->has_client_cookie)
		return 1; /* Cookie reply, but we didn't sent one */

	if (memcmp(upstream->client_cookie, position, 8) != 0) {
		if (!upstream->has_prev_client_cookie)
			return 1; /* Cookie didn't match */
		if (memcmp(upstream->prev_client_cookie, position, 8) != 0)
			return 1; /* Previous cookie didn't match either */

		upstream->has_server_cookie = 0;
		return 0; /* Don't store server cookie, because it
		           * is for our previous client cookie
			   */
	}
	position += 8;
	option_len -= 8;
	upstream->has_server_cookie = 1;
	upstream->server_cookie_len = option_len;
	(void) memcpy(upstream->server_cookie, position, option_len);
	return 0;
}

static void
process_keepalive(
    getdns_upstream *upstream, getdns_network_req *netreq, 
    uint8_t *response, size_t response_len) 
{
	const uint8_t *position = NULL;
	uint16_t option_len = 0;
	int found = match_edns_opt_rr(GLDNS_EDNS_KEEPALIVE, response, 
	                              response_len, &position, &option_len);
	if (found != 2 || option_len != 2) {
		if (netreq->keepalive_sent == 1) {
			/* For TCP if no keepalive sent back, then we must use 0 idle timeout
			   as server does not support it. TLS allows idle connections without
			   keepalive, according to RFC7858. */
#if !defined(KEEP_CONNECTIONS_OPEN_DEBUG) || !KEEP_CONNECTIONS_OPEN_DEBUG
			if (upstream->transport != GETDNS_TRANSPORT_TLS)
				upstream->keepalive_timeout = 0;
			else
#endif
				upstream->keepalive_timeout = netreq->owner->context->idle_timeout;
		}
		return;
	}
	upstream->server_keepalive_received = 1;
	/* Use server sent value unless the client specified a shorter one.
	   Convert to ms first (wire value has units of 100ms) */
	uint64_t server_keepalive = ((uint64_t)gldns_read_uint16(position))*100;
	DEBUG_STUB("%s %-35s: FD:  %d Server Keepalive received: %d ms\n",
           STUB_DEBUG_READ, __FUNC__, upstream->fd, 
           (int)server_keepalive);
	if (netreq->owner->context->idle_timeout < server_keepalive)
		upstream->keepalive_timeout = netreq->owner->context->idle_timeout;
	else {
		if (server_keepalive == 0) {
			/* This means the server wants us to shut the connection (sending no
			   more queries). */
			upstream->keepalive_shutdown = 1;
		}
		upstream->keepalive_timeout = server_keepalive;
		DEBUG_STUB("%s %-35s: FD:  %d Server Keepalive used: %d ms\n",
		           STUB_DEBUG_READ, __FUNC__, upstream->fd, 
		           (int)server_keepalive);
	}
}

/** best effort to set nonblocking */
static void
getdns_sock_nonblock(int sockfd)
{
#ifdef HAVE_FCNTL
	int flag;
	if((flag = fcntl(sockfd, F_GETFL)) != -1) {
		flag |= O_NONBLOCK;
		if(fcntl(sockfd, F_SETFL, flag) == -1) {
			/* ignore error, continue blockingly */
		}
	}
#elif defined(HAVE_IOCTLSOCKET)
	unsigned long on = 1;
	if(ioctlsocket(sockfd, FIONBIO, &on) != 0) {
		/* ignore error, continue blockingly */
	}
#endif
}

static int
tcp_connect(getdns_upstream *upstream, getdns_transport_list_t transport) 
{
#if defined(TCP_FASTOPEN) || defined(TCP_FASTOPEN_CONNECT)
# ifdef USE_WINSOCK
	static const char enable = 1;
# else
	static const int  enable = 1;
# endif
#endif
	int fd = -1;


	upstream->tfo_use_sendto = 0;
	DEBUG_STUB("%s %-35s: Creating TCP connection:      %p\n", STUB_DEBUG_SETUP, 
	           __FUNC__, (void*)upstream);
	if ((fd = socket(upstream->addr.ss_family, SOCK_STREAM, IPPROTO_TCP)) == -1)
		return -1;

	getdns_sock_nonblock(fd);
#ifdef USE_OSX_TCP_FASTOPEN
	sa_endpoints_t endpoints;
	endpoints.sae_srcif = 0;
	endpoints.sae_srcaddr = NULL;
	endpoints.sae_srcaddrlen = 0;
	endpoints.sae_dstaddr = (struct sockaddr *)&upstream->addr;
	endpoints.sae_dstaddrlen = upstream->addr_len;
	if (connectx(fd, &endpoints, SAE_ASSOCID_ANY,
	             CONNECT_DATA_IDEMPOTENT | CONNECT_RESUME_ON_READ_WRITE,
	             NULL, 0, NULL, NULL) == 0) {
		return fd;
	}
	if (_getdns_socketerror() == _getdns_EINPROGRESS ||
	    _getdns_socketerror() == _getdns_EWOULDBLOCK)
		return fd;

	(void)transport;
#else	/* USE_OSX_TCP_FASTOPEN */
	/* Note that error detection is different with TFO. Since the handshake
	   doesn't start till the sendto() lack of connection is often delayed until
	   then or even the subsequent event depending on the error and platform.*/
# if  defined(HAVE_DECL_TCP_FASTOPEN_CONNECT) && HAVE_DECL_TCP_FASTOPEN_CONNECT
	if (setsockopt( fd, IPPROTO_TCP, TCP_FASTOPEN_CONNECT
	              , (void *)&enable, sizeof(enable)) < 0) {
		/* runtime fallback to TCP_FASTOPEN option */
		_getdns_upstream_log(upstream,
		    GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_WARNING,
		    "%-40s : Upstream   : "
		    "Could not setup TLS capable TFO connect\n",
		     upstream->addr_str);
#  if defined(HAVE_DECL_TCP_FASTOPEN) && HAVE_DECL_TCP_FASTOPEN
		/* TCP_FASTOPEN works for TCP only (not TLS) */
		if (transport != GETDNS_TRANSPORT_TCP)
			; /* This variant of TFO doesn't work with TLS */
		else if (setsockopt( fd, IPPROTO_TCP, TCP_FASTOPEN
		                   , (void *)&enable, sizeof(enable)) >= 0) {

			upstream->tfo_use_sendto = 1;
			return fd;
		} else
			_getdns_upstream_log(upstream,
			    GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_WARNING,
			    "%-40s : Upstream   : "
			    "Could not fallback to TCP TFO\n",
			     upstream->addr_str);
#  endif/* HAVE_DECL_TCP_FASTOPEN*/
	}
	/* On success regular connect is fine, TFO will happen automagically */
# else	/* HAVE_DECL_TCP_FASTOPEN_CONNECT */
#  if defined(HAVE_DECL_TCP_FASTOPEN) && HAVE_DECL_TCP_FASTOPEN
	/* TCP_FASTOPEN works for TCP only (not TLS) */
	if (transport != GETDNS_TRANSPORT_TCP)
		; /* This variant of TFO doesn't work with TLS */
	else if (setsockopt( fd, IPPROTO_TCP, TCP_FASTOPEN
	                  , (void *)&enable, sizeof(enable)) >= 0) {

		upstream->tfo_use_sendto = 1;
		return fd;
	} else
		_getdns_upstream_log(upstream,
		    GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_WARNING,
		    "%-40s : Upstream   : Could not setup TCP TFO\n",
		     upstream->addr_str);
#  else
	(void)transport;
#  endif/* HAVE_DECL_TCP_FASTOPEN*/
# endif	/* HAVE_DECL_TCP_FASTOPEN_CONNECT */
#endif	/* USE_OSX_TCP_FASTOPEN */
	if (connect(fd, (struct sockaddr *)&upstream->addr,
	    upstream->addr_len) == -1) {
		if (_getdns_socketerror() == _getdns_EINPROGRESS ||
		    _getdns_socketerror() == _getdns_EWOULDBLOCK)
			return fd;
		_getdns_closesocket(fd);
		return -1;
	}
	return fd;
}

static int
tcp_connected(getdns_upstream *upstream) {
	int error = 0;
	socklen_t len = (socklen_t)sizeof(error);
	getsockopt(upstream->fd, SOL_SOCKET, SO_ERROR, (void*)&error, &len);
	if (_getdns_error_wants_retry(error))
		return STUB_TCP_RETRY;
	else if (error != 0) {
		return STUB_SETUP_ERROR;
	}
	if (upstream->transport == GETDNS_TRANSPORT_TCP &&
	    upstream->queries_sent == 0) {
			upstream->conn_state = GETDNS_CONN_OPEN;
			upstream->conn_completed++;
	}
	return 0;
}

/**************************/
/* Error/cleanup functions*/
/**************************/

static void
stub_next_upstream(getdns_network_req *netreq)
{
	getdns_dns_req *dnsreq = netreq->owner;

	if (! --netreq->upstream->to_retry) {
        /* Limit back_off value to configured maximum */
        if (netreq->upstream->back_off * 2 > dnsreq->context->max_backoff_value)
            netreq->upstream->to_retry = -(dnsreq->context->max_backoff_value);
        else
            netreq->upstream->to_retry = -(netreq->upstream->back_off *= 2);
    }

	dnsreq->upstreams->current_udp+=GETDNS_UPSTREAM_TRANSPORTS;
	if (dnsreq->upstreams->current_udp >= dnsreq->upstreams->count)
		dnsreq->upstreams->current_udp = 0;
}

static void
remove_from_write_queue(getdns_upstream *upstream, getdns_network_req * netreq)
{
	getdns_network_req *r, *prev_r;

	for ( r = upstream->write_queue, prev_r = NULL
	    ; r
	    ; prev_r = r, r = r->write_queue_tail) {

		if (r != netreq)
			continue;

		if (prev_r)
			prev_r->write_queue_tail = r->write_queue_tail;
		else
			upstream->write_queue = r->write_queue_tail;

		if (r == upstream->write_queue_last) {
			/* If r was the last netreq,
			 * its write_queue tail MUST be NULL
			 */
			assert(r->write_queue_tail == NULL);
			upstream->write_queue_last = prev_r ? prev_r : NULL;
		}

		netreq->write_queue_tail = NULL;
		break; /* netreq found and removed */
	}
}

static void
stub_cleanup(getdns_network_req *netreq)
{
	DEBUG_STUB("%s %-35s: MSG: %p\n",
	           STUB_DEBUG_CLEANUP, __FUNC__, (void*)netreq);
	getdns_dns_req *dnsreq = netreq->owner;

	GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);

	if (netreq->query_id_registered) {
		(void) _getdns_rbtree_delete(
		    netreq->query_id_registered, netreq->node.key);
		netreq->query_id_registered = NULL;
		netreq->node.key = NULL;
	}
	if (netreq->upstream) {
		remove_from_write_queue(netreq->upstream, netreq);
		if (netreq->upstream->event.ev)
			upstream_reschedule_events(netreq->upstream);
	}
}

static void
upstream_failed(getdns_upstream *upstream, int during_setup)
{
	getdns_network_req *netreq;

	DEBUG_STUB("%s %-35s: FD:  %d Failure during connection setup = %d\n",
	           STUB_DEBUG_CLEANUP, __FUNC__, upstream->fd, during_setup);
	/* Fallback code should take care of queue queries and then close conn
	   when idle.*/
	/* [TLS1]TODO: Work out how to re-open the connection and re-try
	   the queries if there is only one upstream.*/
	GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
	if (during_setup) {
		upstream->conn_setup_failed++;
	} else {
		upstream->conn_shutdowns++;
		/* [TLS1]TODO: Re-try these queries if possible.*/
	}
	upstream->conn_state = GETDNS_CONN_TEARDOWN;

	while (upstream->write_queue)
		upstream_write_cb(upstream);

	while (upstream->netreq_by_query_id.count) {
		netreq = (getdns_network_req *)
		    _getdns_rbtree_first(&upstream->netreq_by_query_id);
		stub_cleanup(netreq);
		_getdns_netreq_change_state(netreq, NET_REQ_ERRORED);
		_getdns_check_dns_req_complete(netreq->owner);
	}
	_getdns_upstream_shutdown(upstream);
}

void
_getdns_cancel_stub_request(getdns_network_req *netreq)
{
	DEBUG_STUB("%s %-35s: MSG:  %p\n",
	           STUB_DEBUG_CLEANUP, __FUNC__, (void*)netreq);
	stub_cleanup(netreq);
	if (netreq->fd >= 0) {
		_getdns_closesocket(netreq->fd);
		netreq->fd = -1;
	}
}

static void
stub_timeout_cb(void *userarg)
{
	getdns_network_req *netreq = (getdns_network_req *)userarg;
	DEBUG_STUB("%s %-35s: MSG:  %p\n",
	           STUB_DEBUG_CLEANUP, __FUNC__, (void*)netreq);
	stub_cleanup(netreq);
	_getdns_netreq_change_state(netreq, NET_REQ_TIMED_OUT);
	/* Handle upstream*/
	if (netreq->fd >= 0) {
		_getdns_closesocket(netreq->fd);
		netreq->fd = -1;
		netreq->upstream->udp_timeouts++;
		if (netreq->upstream->udp_timeouts % 100 == 0)
			_getdns_upstream_log(netreq->upstream, GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_INFO,
			    "%-40s : Upstream   : UDP - Resps=%6d, Timeouts  =%6d (logged every 100 responses)\n",
			             netreq->upstream->addr_str,
			             (int)netreq->upstream->udp_responses, (int)netreq->upstream->udp_timeouts);
		stub_next_upstream(netreq);
	} else {
		netreq->upstream->responses_timeouts++;
	}
	if (netreq->owner->user_callback) {
		netreq->debug_end_time = _getdns_get_time_as_uintt64();
		/* Note this calls cancel_request which calls stub_cleanup again....!*/
		_getdns_context_request_timed_out(netreq->owner);
	} else
		_getdns_check_dns_req_complete(netreq->owner);
}

static void
upstream_idle_timeout_cb(void *userarg)
{
	getdns_upstream *upstream = (getdns_upstream *)userarg;
	DEBUG_STUB("%s %-35s: FD:  %d Closing connection\n",
	           STUB_DEBUG_CLEANUP, __FUNC__, upstream->fd);
	GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
	upstream->event.timeout_cb = NULL;
	upstream->event.read_cb = NULL;
	upstream->event.write_cb = NULL;
	_getdns_upstream_shutdown(upstream);
}

static void
upstream_setup_timeout_cb(void *userarg)
{
	getdns_upstream *upstream = (getdns_upstream *)userarg;

	DEBUG_STUB("%s %-35s: FD:  %d\n",
	           STUB_DEBUG_CLEANUP, __FUNC__, upstream->fd);

	upstream_failed(upstream, 1);
}


/****************************/
/* TCP read/write functions */
/****************************/

static int
stub_tcp_read(int fd, getdns_tcp_state *tcp, struct mem_funcs *mf)
{
	ssize_t  read;
	uint8_t *buf;
	size_t   buf_size;

	if (!tcp->read_buf) {
		/* First time tcp read, create a buffer for reading */
		if (!(tcp->read_buf = GETDNS_XMALLOC(*mf, uint8_t, 4096)))
			return STUB_TCP_ERROR;

		tcp->read_buf_len = 4096;
		tcp->read_pos = tcp->read_buf;
		tcp->to_read = 2; /* Packet size */
	}
	read = recv(fd, (void *)tcp->read_pos, tcp->to_read, 0);
	if (read < 0) {
		if (_getdns_socketerror_wants_retry())
			return STUB_TCP_RETRY;
		else
			return STUB_TCP_ERROR;
	} else if (read == 0) {
		/* Remote end closed the socket */
		/* TODO: Try to reconnect */
		return STUB_TCP_ERROR;
	} else if ((size_t)read > tcp->to_read) {
		return STUB_TCP_ERROR;
	}
	tcp->to_read  -= read;
	tcp->read_pos += read;
	
	if (tcp->to_read > 0)
		return STUB_TCP_MORE_TO_READ;

	read = tcp->read_pos - tcp->read_buf;
	if (read == 2) {
		/* Read the packet size short */
		tcp->to_read = gldns_read_uint16(tcp->read_buf);

		if (tcp->to_read < GLDNS_HEADER_SIZE)
			return STUB_TCP_ERROR;

		/* Resize our buffer if needed */
		if (tcp->to_read > tcp->read_buf_len) {
			buf_size = tcp->read_buf_len;
			while (tcp->to_read > buf_size)
				buf_size *= 2;

			if (!(buf = GETDNS_XREALLOC(*mf,
			    tcp->read_buf, uint8_t, buf_size)))
				return STUB_TCP_ERROR;

			tcp->read_buf = buf;
			tcp->read_buf_len = buf_size;
		}
		/* Ready to start reading the packet */
		tcp->read_pos = tcp->read_buf;
		return STUB_TCP_MORE_TO_READ;
	}
	return GLDNS_ID_WIRE(tcp->read_buf);
}

/* stub_tcp_write(fd, tcp, netreq)
 * will return STUB_TCP_RETRY or STUB_TCP_MORE_TO_WRITE when we need to come
 * back again, STUB_TCP_ERROR on error and a query_id on successful sent.
 */
static int
stub_tcp_write(int fd, getdns_tcp_state *tcp, getdns_network_req *netreq)
{

	size_t          pkt_len;
	ssize_t         written;
	uint16_t        query_id;
	intptr_t        query_id_intptr;

	int q = tcp_connected(netreq->upstream);
	if (q != 0)
		return q;

	netreq->debug_udp = 0;
	/* Do we have remaining data that we could not write before?  */
	if (! tcp->write_buf) {
		/* No, this is an initial write. Try to send
		 */
		do {
			query_id = arc4random();
			query_id_intptr = (intptr_t)query_id;
			netreq->node.key = (void *)query_id_intptr;

		} while (!_getdns_rbtree_insert(
		    &netreq->upstream->netreq_by_query_id, &netreq->node));
		netreq->query_id_registered = &netreq->upstream->netreq_by_query_id;

		GLDNS_ID_SET(netreq->query, query_id);

		if (netreq->opt) {
			_getdns_network_req_clear_upstream_options(netreq);
			/* no limits on the max udp payload size with tcp */
			gldns_write_uint16(netreq->opt + 3, 65535);

			if (netreq->owner->edns_cookies)
				if (attach_edns_cookie(netreq))
					return STUB_OUT_OF_OPTIONS;
			if (netreq->owner->edns_client_subnet_private)
				if (attach_edns_client_subnet_private(netreq))
					return STUB_OUT_OF_OPTIONS;
			if (netreq->upstream->queries_sent == 0 && 
				netreq->owner->context->idle_timeout != 0) {
				/* Add the keepalive option to the first query on this connection*/
				DEBUG_STUB("%s %-35s: FD:  %d Requesting keepalive \n",
				           STUB_DEBUG_WRITE, __FUNC__, fd);
				if (attach_edns_keepalive(netreq))
					return STUB_OUT_OF_OPTIONS;
				netreq->keepalive_sent = 1;
			}
		}
		pkt_len = _getdns_network_req_add_tsig(netreq);
		/* We have an initialized packet buffer.
		 * Lets see how much of it we can write
		 */
		/* We use sendto() here which will do both a connect and send */
		if (netreq->upstream->tfo_use_sendto) {
			written = sendto(fd, netreq->query - 2, pkt_len + 2,
# if   defined(HAVE_DECL_MSG_FASTOPEN) && HAVE_DECL_MSG_FASTOPEN
			    MSG_FASTOPEN,
# else
			    0,
# endif
			    (struct sockaddr *)&(netreq->upstream->addr),
			    netreq->upstream->addr_len);
			/* If pipelining we will find that the connection is already up so 
			   just fall back to a 'normal' write. */
			if (written == -1
			&&  _getdns_socketerror() == _getdns_EISCONN) 
				written = write(fd, netreq->query - 2
				                  , pkt_len + 2);
		} else
			written = send(fd, (const char *)(netreq->query - 2)
			                 , pkt_len + 2, 0);
		if ((written == -1 && _getdns_socketerror_wants_retry()) ||
		    (size_t)written < pkt_len + 2) {

			/* We couldn't write the whole packet.
			 * Setup tcp to track the state.
			 */
			tcp->write_buf = netreq->query - 2;
			tcp->write_buf_len = pkt_len + 2;
			tcp->written = written >= 0 ? written : 0;

			return written == -1
			     ? STUB_TCP_RETRY
			     : STUB_TCP_MORE_TO_WRITE;

		} else if (written == -1) {
			DEBUG_STUB("%s %-35s: MSG: %p error while writing to TCP socket:"
				   " %s\n", STUB_DEBUG_WRITE, __FUNC__, (void*)netreq
				   , _getdns_errnostr());

			return STUB_TCP_ERROR;
		}

		/* We were able to write everything!  Start reading. */
		return (int) query_id;

	} else {/* if (! tcp->write_buf) */

		/* Coming back from an earlier unfinished write or handshake.
		 * Try to send remaining data */
		written = send(fd, (void *)(tcp->write_buf + tcp->written),
			tcp->write_buf_len - tcp->written, 0);
		if (written == -1) {
			if (_getdns_socketerror_wants_retry())
				return STUB_TCP_RETRY;
			else {
				DEBUG_STUB("%s %-35s: MSG: %p error while writing to TCP socket:"
					   " %s\n", STUB_DEBUG_WRITE, __FUNC__, (void*)netreq
					   , _getdns_errnostr());

				return STUB_TCP_ERROR;
			}
		}
		tcp->written += written;
		if (tcp->written < tcp->write_buf_len)
			/* Still more to send */
			return STUB_TCP_MORE_TO_WRITE;

		query_id = (int)GLDNS_ID_WIRE(tcp->write_buf + 2);
		/* Done. Start reading */
		tcp->write_buf = NULL;
		return query_id;

	} /* if (! tcp->write_buf) */
}

/*************************/
/* TLS Utility functions */
/*************************/

static int
tls_requested(getdns_network_req *netreq)
{
	return (netreq->transports[netreq->transport_current] ==
	        GETDNS_TRANSPORT_TLS) ?
	        1 : 0;
}

static _getdns_tls_connection*
tls_create_object(getdns_dns_req *dnsreq, int fd, getdns_upstream *upstream)
{
	/* Create SSL instance and connect with a file descriptor */
	getdns_context *context = dnsreq->context;
	if (context->tls_ctx == NULL)
		return NULL;
	_getdns_tls_connection* tls = _getdns_tls_connection_new(&context->my_mf, context->tls_ctx, fd, &upstream->upstreams->log);
	if(!tls) 
		return NULL;

	getdns_return_t r = GETDNS_RETURN_GOOD;

	if (upstream->tls_curves_list)
		r = _getdns_tls_connection_set_curves_list(tls, upstream->tls_curves_list);
	if (!r && upstream->tls_ciphersuites)
		r = _getdns_tls_connection_set_cipher_suites(tls, upstream->tls_ciphersuites);
	if (!r)
		r = _getdns_tls_connection_set_min_max_tls_version(tls, upstream->tls_min_version, upstream->tls_max_version);

	if (!r)
	{
		if (upstream->tls_fallback_ok)
			r = _getdns_tls_connection_set_cipher_list(tls, NULL);
		else if (upstream->tls_cipher_list)
			r = _getdns_tls_connection_set_cipher_list(tls, upstream->tls_cipher_list);
	}
	
	if (r) {
		_getdns_tls_connection_free(&upstream->upstreams->mf, tls);
		upstream->tls_auth_state = GETDNS_AUTH_NONE;
		return NULL;
	}

	if (upstream->tls_fallback_ok) {
		DEBUG_STUB("%s %-35s: WARNING: Using Opportunistic TLS (fallback allowed)!\n",
			   STUB_DEBUG_SETUP_TLS, __FUNC__);
	} else {
		DEBUG_STUB("%s %-35s: Using Strict TLS \n",
			   STUB_DEBUG_SETUP_TLS, __FUNC__);
	}

	/* NOTE: this code will fallback on a given upstream, without trying
	   authentication on other upstreams first. This is non-optimal and but avoids
	   multiple TLS handshakes before getting a usable connection. */

	upstream->tls_fallback_ok = 0;
	/* If we have a hostname, always use it */
	if (upstream->tls_auth_name[0] != '\0') {
		/*Request certificate for the auth_name*/
		DEBUG_STUB("%s %-35s: Hostname verification requested for: %s\n",
		           STUB_DEBUG_SETUP_TLS, __FUNC__, upstream->tls_auth_name);
		_getdns_tls_connection_setup_hostname_auth(tls, upstream->tls_auth_name);
		/* Allow fallback to opportunistic if settings permit it*/
		if (dnsreq->netreqs[0]->tls_auth_min != GETDNS_AUTHENTICATION_REQUIRED)
			upstream->tls_fallback_ok = 1;
	} else {
		/* Lack of host name is OK unless only authenticated
		 * TLS is specified and we have no pubkey_pinset */
		if (dnsreq->netreqs[0]->tls_auth_min == GETDNS_AUTHENTICATION_REQUIRED) {
			if (upstream->tls_pubkey_pinset) {
				DEBUG_STUB("%s %-35s: Proceeding with only pubkey pinning authentication\n",
			           STUB_DEBUG_SETUP_TLS, __FUNC__);
			} else {
				DEBUG_STUB("%s %-35s: ERROR:No auth name or pinset provided for this upstream for Strict TLS authentication\n",
			           STUB_DEBUG_SETUP_TLS, __FUNC__);
				_getdns_upstream_log(upstream, GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR, 
				    "%-40s : Verify fail: *CONFIG ERROR* - No auth name or pinset provided for this upstream for Strict TLS authentication\n",
				    upstream->addr_str);
				upstream->tls_hs_state = GETDNS_HS_FAILED;
				_getdns_tls_connection_free(&upstream->upstreams->mf, tls);
				upstream->tls_auth_state = GETDNS_AUTH_FAILED;
				return NULL;
			}
		} else {
			/* no hostname verification, so we will make opportunistic connections */
			DEBUG_STUB("%s %-35s: Proceeding even though no hostname provided!\n",
			           STUB_DEBUG_SETUP_TLS, __FUNC__);
			upstream->tls_fallback_ok = 1;
		}
	}

	_getdns_tls_connection_set_host_pinset(tls, upstream->tls_auth_name, upstream->tls_pubkey_pinset);

	/* Session resumption. There are trade-offs here. Want to do it when
	   possible only if we have the right type of connection. Note a change
	   to the upstream auth info creates a new upstream so never re-uses.*/
	if (upstream->tls_session != NULL) {
		if ((upstream->tls_fallback_ok == 0 &&
		     upstream->last_tls_auth_state == GETDNS_AUTH_OK) ||
		     upstream->tls_fallback_ok == 1) {
			_getdns_tls_connection_set_session(tls, upstream->tls_session);
			DEBUG_STUB("%s %-35s: Attempting session re-use\n", STUB_DEBUG_SETUP_TLS, 
			            __FUNC__);
			}
	}
	return tls;
}

static int
tls_do_handshake(getdns_upstream *upstream)
{
	DEBUG_STUB("%s %-35s: FD:  %d \n", STUB_DEBUG_SETUP_TLS, 
	             __FUNC__, upstream->fd);
	int r;
	while ((r = _getdns_tls_connection_do_handshake(upstream->tls_obj)) != GETDNS_RETURN_GOOD)
	{
		uint64_t timeout_tls = _getdns_ms_until_expiry(upstream->expires);

		if (timeout_tls < MIN_TLS_HS_TIMEOUT)
			timeout_tls = MIN_TLS_HS_TIMEOUT;
		else if (timeout_tls > MAX_TLS_HS_TIMEOUT)
			timeout_tls = MAX_TLS_HS_TIMEOUT;

		DEBUG_STUB("%s %-35s: FD:  %d, do_handshake -> %d (timeout: %d)\n",
		    STUB_DEBUG_SETUP_TLS, __FUNC__, upstream->fd, r, (int)timeout_tls);

		switch (r) {
			case GETDNS_RETURN_TLS_WANT_READ:
				GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
				upstream->event.read_cb = upstream_read_cb;
				upstream->event.write_cb = NULL;
				GETDNS_SCHEDULE_EVENT(upstream->loop,
				    upstream->fd, timeout_tls, &upstream->event);
				upstream->tls_hs_state = GETDNS_HS_READ;
				return STUB_TCP_RETRY;
			case GETDNS_RETURN_TLS_WANT_WRITE:
				GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
				upstream->event.read_cb = NULL;
				upstream->event.write_cb = upstream_write_cb;
				GETDNS_SCHEDULE_EVENT(upstream->loop,
				    upstream->fd, timeout_tls, &upstream->event);
				upstream->tls_hs_state = GETDNS_HS_WRITE;
				return STUB_TCP_RETRY;
			default:
				DEBUG_STUB("%s %-35s: FD:  %d Handshake failed %d\n", 
				            STUB_DEBUG_SETUP_TLS, __FUNC__, upstream->fd, r);
				return STUB_SETUP_ERROR;
	   }
	}
	/* A re-used session is not verified so need to fix up state in that case */
	if (!_getdns_tls_connection_is_session_reused(upstream->tls_obj))
		upstream->tls_auth_state = upstream->last_tls_auth_state;

	else if (upstream->tls_pubkey_pinset || upstream->tls_auth_name[0]) {
		_getdns_tls_x509* peer_cert = _getdns_tls_connection_get_peer_certificate(&upstream->upstreams->mf, upstream->tls_obj);

		if (!peer_cert) {
			_getdns_upstream_log(upstream,
			    GETDNS_LOG_UPSTREAM_STATS,
			    ( upstream->tls_fallback_ok
			    ? GETDNS_LOG_INFO : GETDNS_LOG_ERR),
			    "%-40s : Verify failed : TLS - %s -  "
			    "Remote did not offer certificate\n",
			    upstream->addr_str,
			    ( upstream->tls_fallback_ok
			    ? "Tolerated because of Opportunistic profile"
			    : "*Failure*" ));
			upstream->tls_auth_state = GETDNS_AUTH_FAILED;
		} else {
			long verify_errno;
			const char* verify_errmsg;

			if (_getdns_tls_connection_certificate_verify(upstream->tls_obj, &verify_errno, &verify_errmsg)) {
				upstream->tls_auth_state = GETDNS_AUTH_FAILED;
				if (verify_errno != 0) {
					_getdns_upstream_log(upstream,
					    GETDNS_LOG_UPSTREAM_STATS,
					     ( upstream->tls_fallback_ok
					       ? GETDNS_LOG_INFO : GETDNS_LOG_ERR),
					     "%-40s : Verify failed : TLS - %s - "
					     "(%ld) \"%s\"\n", upstream->addr_str,
					     ( upstream->tls_fallback_ok
					       ? "Tolerated because of Opportunistic profile"
					       : "*Failure*" ),
					     verify_errno, verify_errmsg);
				} else {
					_getdns_upstream_log(upstream,
					    GETDNS_LOG_UPSTREAM_STATS,
					     ( upstream->tls_fallback_ok
					       ? GETDNS_LOG_INFO : GETDNS_LOG_ERR),					     "%-40s : Verify failed : TLS - %s -  "
					     "%s\n", upstream->addr_str,
					     ( upstream->tls_fallback_ok
					       ? "Tolerated because of Opportunistic profile"
					       : "*Failure*" ),
					     verify_errmsg);
				}
			} else {
				upstream->tls_auth_state = GETDNS_AUTH_OK;
				_getdns_upstream_log(upstream,
						     GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_DEBUG,
						     "%-40s : Verify passed : TLS\n",
						     upstream->addr_str);
			}
			_getdns_tls_x509_free(&upstream->upstreams->mf, peer_cert);
		}
		if (upstream->tls_auth_state == GETDNS_AUTH_FAILED
		    && !upstream->tls_fallback_ok)
			return STUB_SETUP_ERROR;
	}
	DEBUG_STUB("%s %-35s: FD:  %d Handshake succeeded with auth state %s. Session is %s.\n", 
		         STUB_DEBUG_SETUP_TLS, __FUNC__, upstream->fd, 
		         _getdns_auth_str(upstream->tls_auth_state),
		   _getdns_tls_connection_is_session_reused(upstream->tls_obj) ? "new" : "re-used");
	upstream->tls_hs_state = GETDNS_HS_DONE;
	upstream->conn_state = GETDNS_CONN_OPEN;
	upstream->conn_completed++;
	if (upstream->tls_session != NULL)
		_getdns_tls_session_free(&upstream->upstreams->mf, upstream->tls_session);
	upstream->tls_session = _getdns_tls_connection_get_session(&upstream->upstreams->mf, upstream->tls_obj);
	/* Reset timeout on success*/
	GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
	upstream->event.read_cb = NULL;
	upstream->event.write_cb = upstream_write_cb;
	GETDNS_SCHEDULE_EVENT(upstream->loop, upstream->fd, TIMEOUT_FOREVER,
	    getdns_eventloop_event_init(&upstream->event, upstream,
	     NULL, upstream_write_cb, NULL));
	return 0;
}

static int
tls_connected(getdns_upstream* upstream)
{
	/* Already have a TLS connection*/
	if (upstream->tls_hs_state == GETDNS_HS_DONE)
		return 0;

	/* Already tried and failed, so let the fallback code take care of things */
	if (upstream->tls_hs_state == GETDNS_HS_FAILED)
		return STUB_SETUP_ERROR;

	/* Lets make sure the TCP connection is up before we try a handshake*/
	int q = tcp_connected(upstream);
	if (q != 0) 
		return q;

	return tls_do_handshake(upstream);
}

/***************************/
/* TLS read/write functions*/
/***************************/

static int
stub_tls_read(getdns_upstream *upstream, getdns_tcp_state *tcp,
              struct mem_funcs *mf)
{
	size_t  read;
	uint8_t *buf;
	size_t   buf_size;
	_getdns_tls_connection* tls_obj = upstream->tls_obj;

	int q = tls_connected(upstream);
	if (q != 0)
		return q;

	if (!tcp->read_buf) {
		/* First time tls read, create a buffer for reading */
		if (!(tcp->read_buf = GETDNS_XMALLOC(*mf, uint8_t, 4096)))
			return STUB_TCP_ERROR;

		tcp->read_buf_len = 4096;
		tcp->read_pos = tcp->read_buf;
		tcp->to_read = 2; /* Packet size */
	}

	getdns_return_t r = _getdns_tls_connection_read(tls_obj, tcp->read_pos, tcp->to_read, &read);
	/* TODO[TLS]: Handle GETDNS_RETURN_TLS_WANT_WRITE which means handshake
	   renegotiation. Need to keep handshake state to do that.*/
	if (r == GETDNS_RETURN_TLS_WANT_READ)
		return STUB_TCP_RETRY;
	else if (r != GETDNS_RETURN_GOOD)
		return STUB_TCP_ERROR;

	tcp->to_read  -= read;
	tcp->read_pos += read;

	if ((int)tcp->to_read > 0)
		return STUB_TCP_MORE_TO_READ;

	read = tcp->read_pos - tcp->read_buf;
	if (read == 2) {
		/* Read the packet size short */
		tcp->to_read = gldns_read_uint16(tcp->read_buf);

		if (tcp->to_read < GLDNS_HEADER_SIZE)
			return STUB_TCP_ERROR;

		/* Resize our buffer if needed */
		if (tcp->to_read > tcp->read_buf_len) {
			buf_size = tcp->read_buf_len;
			while (tcp->to_read > buf_size)
				buf_size *= 2;
		
			if (!(buf = GETDNS_XREALLOC(*mf,
			    tcp->read_buf, uint8_t, buf_size)))
				return STUB_TCP_ERROR;
		
			tcp->read_buf = buf;
			tcp->read_buf_len = buf_size;
		}

		/* Ready to start reading the packet */
		tcp->read_pos = tcp->read_buf;
		switch ((int)_getdns_tls_connection_read(tls_obj, tcp->read_pos, tcp->to_read, &read)) {
		case GETDNS_RETURN_GOOD:
			break;

		case GETDNS_RETURN_TLS_WANT_READ:
			return STUB_TCP_RETRY; /* Come back later */

		default:
			/* TODO[TLS]: Handle GETDNS_RETURN_TLS_WANT_WRITE which means handshake
			   renegotiation. Need to keep handshake state to do that.*/
			return STUB_TCP_ERROR;
		}
		tcp->to_read  -= read;
		tcp->read_pos += read;
		if ((int)tcp->to_read > 0)
			return STUB_TCP_MORE_TO_READ;
	}
	return GLDNS_ID_WIRE(tcp->read_buf);
}

static int
stub_tls_write(getdns_upstream *upstream, getdns_tcp_state *tcp,
               getdns_network_req *netreq)
{
	size_t          pkt_len;
	size_t          written;
	uint16_t        query_id;
	intptr_t        query_id_intptr;
	_getdns_tls_connection* tls_obj = upstream->tls_obj;
	uint16_t        padding_sz;

	int q;
       
	if (netreq->owner->expires > upstream->expires)
		upstream->expires = netreq->owner->expires;

	q = tls_connected(upstream);
	if (q != 0)
		return q;
	/* This is the case where the upstream is connected but it isn't an authenticated
	   connection, but the request needs an authenticated connection. For now, we
	   fail the write as a special case, since other oppotunistic requests can still use
	   this upstream. but this needs more thought: Should we open a second connection? */
	if (!upstream_auth_status_ok(upstream, netreq))
		return STUB_NO_AUTH;

	/* Do we have remaining data that we could not write before?  */
	if (! tcp->write_buf) {
		/* No, this is an initial write. Try to send
		 */

		 /* Find a unique query_id not already written (or in
		 * the write_queue) for that upstream.  Register this netreq 
		 * by query_id in the process.
		 */
		do {
			query_id = arc4random();
			query_id_intptr = (intptr_t)query_id;
			netreq->node.key = (void *)query_id_intptr;

		} while (!_getdns_rbtree_insert(
		    &netreq->upstream->netreq_by_query_id, &netreq->node));
		netreq->query_id_registered = &netreq->upstream->netreq_by_query_id;

		GLDNS_ID_SET(netreq->query, query_id);

		/* TODO: Review if more EDNS0 handling can be centralised.*/
		if (netreq->opt) {
			_getdns_network_req_clear_upstream_options(netreq);
			/* no limits on the max udp payload size with tcp */
			gldns_write_uint16(netreq->opt + 3, 65535);
			/* we do not edns_cookie over TLS, since TLS
			 * provides stronger guarantees than cookies
			 * already */
			if (netreq->owner->edns_client_subnet_private)
				if (attach_edns_client_subnet_private(netreq))
					return STUB_OUT_OF_OPTIONS;
			if (netreq->upstream->queries_sent % EDNS_KEEPALIVE_RESEND == 0 && 
				netreq->owner->context->idle_timeout != 0) {
				/* Add the keepalive option to every nth query on this 
				   connection */
				DEBUG_STUB("%s %-35s: FD:  %d Requesting keepalive \n",  
			             STUB_DEBUG_SETUP, __FUNC__, upstream->fd);
				if (attach_edns_keepalive(netreq))
					return STUB_OUT_OF_OPTIONS;
				netreq->keepalive_sent = 1;
			}
			if (netreq->owner->tls_query_padding_blocksize > 0) {
				uint16_t blksz = netreq->owner->tls_query_padding_blocksize;
				if (blksz == 1) /* use a sensible default policy */
					blksz = 128;
				pkt_len = netreq->response - netreq->query;
				pkt_len += 4; /* this accounts for the OPTION-CODE and OPTION-LENGTH of the padding */
				padding_sz = pkt_len % blksz;
				if (padding_sz)
					padding_sz = blksz - padding_sz;
				if (_getdns_network_req_add_upstream_option(netreq,
									    EDNS_PADDING_OPCODE,
									    padding_sz, NULL))
					return STUB_OUT_OF_OPTIONS;
			}
		}

		pkt_len = _getdns_network_req_add_tsig(netreq);
		/* We have an initialized packet buffer.
		 * Lets see how much of it we can write */
		
		/* TODO[TLS]: Handle error cases, partial writes, renegotiation etc. */
		getdns_return_t r;
		
#if INTERCEPT_COM_DS
		/* Intercept and do not sent out COM DS queries. For debugging
		 * purposes only. Never commit with this turned on.
		 */
		if (netreq->request_type == GETDNS_RRTYPE_DS &&
		    netreq->owner->name_len == 5 &&
		    netreq->owner->name[0] == 3 &&
		    (netreq->owner->name[1] & 0xDF) == 'C' &&
		    (netreq->owner->name[2] & 0xDF) == 'O' &&
		    (netreq->owner->name[3] & 0xDF) == 'M' &&
		    netreq->owner->name[4] == 0) {

			debug_req("Intercepting", netreq);
			written = pkt_len + 2;
			r = GETDNS_RETURN_GOOD;
		} else
#endif
		r = _getdns_tls_connection_write(tls_obj, netreq->query - 2, pkt_len + 2, &written);
		if (r == GETDNS_RETURN_TLS_WANT_READ ||
		    r == GETDNS_RETURN_TLS_WANT_WRITE)
			return STUB_TCP_RETRY;
		else if (r != GETDNS_RETURN_GOOD)
			return STUB_TCP_ERROR;

		/* We were able to write everything!  Start reading. */
		return (int) query_id;

	} 

	return STUB_TCP_ERROR;
}

uint64_t
_getdns_get_time_as_uintt64() {

	struct timeval tv;
	uint64_t       now;

	if (gettimeofday(&tv, NULL)) {
		return 0;
	}
	now = tv.tv_sec * 1000000 + tv.tv_usec;
	return now;
}


/**************************/
/* UDP callback functions */
/**************************/


static void
stub_udp_read_cb(void *userarg)
{
	getdns_network_req *netreq = (getdns_network_req *)userarg;
	getdns_dns_req *dnsreq = netreq->owner;
	getdns_upstream *upstream = netreq->upstream;
	ssize_t       read;
	DEBUG_STUB("%s %-35s: MSG: %p \n", STUB_DEBUG_READ, 
	             __FUNC__, (void*)netreq);

	read = recvfrom(netreq->fd, (void *)netreq->response,
	    netreq->max_udp_payload_size + 1, /* If read == max_udp_payload_size
	                                       * then all is good.  If read ==
	                                       * max_udp_payload_size + 1, then
	                                       * we receive more then requested!
	                                       * i.e. overflow
	                                       */
	    0, NULL, NULL);
	if (read == -1 && (_getdns_socketerror_wants_retry() ||
		           _getdns_socketerror() == _getdns_ECONNRESET))
		return; /* Try again later */

	if (read == -1) {
		DEBUG_STUB("%s %-35s: MSG: %p error while reading from socket:"
		           " %s\n", STUB_DEBUG_READ, __FUNC__, (void*)netreq
			   , _getdns_errnostr());

		stub_cleanup(netreq);
		_getdns_netreq_change_state(netreq, NET_REQ_ERRORED);
		/* Handle upstream*/
		if (netreq->fd >= 0) {
			_getdns_closesocket(netreq->fd);
			netreq->fd = -1;
			stub_next_upstream(netreq);
		}
		netreq->debug_end_time = _getdns_get_time_as_uintt64();
		_getdns_check_dns_req_complete(netreq->owner);
		return;
	}
	if (read < GLDNS_HEADER_SIZE)
		return; /* Not DNS */
	
	if (GLDNS_ID_WIRE(netreq->response) != GLDNS_ID_WIRE(netreq->query))
		return; /* Cache poisoning attempt ;) */

	if (netreq->owner->edns_cookies && match_and_process_server_cookie(
	    upstream, netreq->response, read))
		return; /* Client cookie didn't match? */

	GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);

	_getdns_closesocket(netreq->fd);
	netreq->fd = -1;
	while (GLDNS_TC_WIRE(netreq->response)) {
		DEBUG_STUB("%s %-35s: MSG: %p TC bit set in response \n", STUB_DEBUG_READ, 
		             __FUNC__, (void*)netreq);
		if (!(netreq->transport_current < netreq->transport_count))
			break;
		getdns_transport_list_t next_transport = 
		                      netreq->transports[++netreq->transport_current];
		if (next_transport != GETDNS_TRANSPORT_TCP &&
		    next_transport != GETDNS_TRANSPORT_TLS)
			break;
		/* For now, special case where fallback should be on the same upstream*/
		if ((netreq->fd = upstream_connect(upstream, next_transport,
		                                   dnsreq)) == -1)
			break;
		upstream_schedule_netreq(netreq->upstream, netreq);
		GETDNS_SCHEDULE_EVENT(dnsreq->loop, -1,
		    _getdns_ms_until_expiry(dnsreq->expires),
		    getdns_eventloop_event_init(&netreq->event,
		    netreq, NULL, NULL, stub_timeout_cb));

		return;
	}
	netreq->response_len = read;
	if (!dnsreq->context->round_robin_upstreams)
		dnsreq->upstreams->current_udp = 0;
	else {
		dnsreq->upstreams->current_udp+=GETDNS_UPSTREAM_TRANSPORTS;
		if (dnsreq->upstreams->current_udp >= dnsreq->upstreams->count)
			dnsreq->upstreams->current_udp = 0;
	}
	netreq->debug_end_time = _getdns_get_time_as_uintt64();
	_getdns_netreq_change_state(netreq, NET_REQ_FINISHED);
	upstream->udp_responses++;
    upstream->back_off = 1;
	if (upstream->udp_responses == 1 || 
	    upstream->udp_responses % 100 == 0)
		_getdns_upstream_log(upstream, GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_INFO,
		    "%-40s : Upstream   : UDP - Resps=%6d, Timeouts  =%6d (logged every 100 responses)\n",
		    upstream->addr_str,
		    (int)upstream->udp_responses, (int)upstream->udp_timeouts);
	_getdns_check_dns_req_complete(dnsreq);
}

static void
stub_udp_write_cb(void *userarg)
{
	getdns_network_req *netreq = (getdns_network_req *)userarg;
	getdns_dns_req     *dnsreq = netreq->owner;
	size_t             pkt_len;
	ssize_t            written;
	DEBUG_STUB("%s %-35s: MSG: %p \n", STUB_DEBUG_WRITE, 
	             __FUNC__, (void *)netreq);

	GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);

	netreq->debug_start_time = _getdns_get_time_as_uintt64();
	netreq->debug_udp = 1;
	GLDNS_ID_SET(netreq->query, (uint16_t)arc4random());
	if (netreq->opt) {
		_getdns_network_req_clear_upstream_options(netreq);
		if (netreq->edns_maximum_udp_payload_size == -1)
			gldns_write_uint16(netreq->opt + 3,
			    ( netreq->max_udp_payload_size =
			      netreq->upstream->addr.ss_family == AF_INET6
			    ? 1232 : 1432));
		if (netreq->owner->edns_cookies)
			if (attach_edns_cookie(netreq))
				return; /* too many upstream options */
		if (netreq->owner->edns_client_subnet_private)
			if (attach_edns_client_subnet_private(netreq))
				return; /* too many upstream options */
	}
	pkt_len = _getdns_network_req_add_tsig(netreq);
	if ((ssize_t)pkt_len != (written = sendto(
	    netreq->fd, (const void *)netreq->query, pkt_len, 0,
	    (struct sockaddr *)&netreq->upstream->addr,
	                        netreq->upstream->addr_len))) {

#if defined(STUB_DEBUG) && STUB_DEBUG
		if (written == -1)
			DEBUG_STUB( "%s %-35s: MSG: %p error: %s\n"
				  , STUB_DEBUG_WRITE, __FUNC__, (void *)netreq
				  , _getdns_errnostr());
		else
			DEBUG_STUB( "%s %-35s: MSG: %p returned: %d, expected: %d\n"
				  , STUB_DEBUG_WRITE, __FUNC__, (void *)netreq
				  , (int)written, (int)pkt_len);
#endif
		stub_cleanup(netreq);
		_getdns_netreq_change_state(netreq, NET_REQ_ERRORED);
		/* Handle upstream*/
		if (netreq->fd >= 0) {
			_getdns_closesocket(netreq->fd);
			netreq->fd = -1;
			stub_next_upstream(netreq);
		}
		netreq->debug_end_time = _getdns_get_time_as_uintt64();
		_getdns_check_dns_req_complete(netreq->owner);
		return;
	}
	GETDNS_SCHEDULE_EVENT(dnsreq->loop, netreq->fd,
	    _getdns_ms_until_expiry(dnsreq->expires),
	    getdns_eventloop_event_init(&netreq->event, netreq,
	    stub_udp_read_cb, NULL, stub_timeout_cb));
}

/**************************/
/* Upstream callback functions*/
/**************************/

static void
process_finished_cb(void *userarg)
{
	getdns_upstream *upstream = (getdns_upstream *)userarg;
	getdns_dns_req *dnsreq;

	/* Upstream->loop is always the async one, because finished_events
	 * are only scheduled against (and thus fired from) the async loop
	 */
	GETDNS_CLEAR_EVENT(upstream->loop, &upstream->finished_event);
	upstream->finished_event.timeout_cb = NULL;
	while (upstream->finished_dnsreqs) {
		dnsreq = upstream->finished_dnsreqs;
		upstream->finished_dnsreqs = dnsreq->finished_next;
		_getdns_check_dns_req_complete(dnsreq);
	}
}

static void
upstream_read_cb(void *userarg)
{
	getdns_upstream *upstream = (getdns_upstream *)userarg;
	DEBUG_STUB("%s %-35s: FD:  %d \n", STUB_DEBUG_READ, __FUNC__,
	            upstream->fd);
	getdns_network_req *netreq;
	int q;
	uint16_t query_id;
	intptr_t query_id_intptr;
	getdns_dns_req *dnsreq;

	if (upstream->transport == GETDNS_TRANSPORT_TLS)
		q = stub_tls_read(upstream, &upstream->tcp,
		                 &upstream->upstreams->mf);
	else
		q = stub_tcp_read(upstream->fd, &upstream->tcp,
		                 &upstream->upstreams->mf);

	switch (q) {
	case STUB_TCP_MORE_TO_READ:
		/* WSA TODO: if callback is still upstream_read_cb, do it again
		 */
	case STUB_TCP_RETRY:
		return;
	case STUB_SETUP_ERROR:  /* Can happen for TLS HS*/
	case STUB_TCP_ERROR:
		upstream_failed(upstream, (q == STUB_TCP_ERROR ? 0:1) );
		return;

	default:
		/* Lookup netreq */
		query_id = (uint16_t) q;
		query_id_intptr = (intptr_t) query_id;
		netreq = (getdns_network_req *)_getdns_rbtree_delete(
		    &upstream->netreq_by_query_id, (void *)query_id_intptr);
		if (! netreq) /* maybe canceled */ {
			/* reset read buffer */
			upstream->tcp.read_pos = upstream->tcp.read_buf;
			upstream->tcp.to_read = 2;
			return;
		}
		if (netreq->query_id_registered == &upstream->netreq_by_query_id) {
			netreq->query_id_registered = NULL;
			netreq->node.key = NULL;

		} else if (netreq->query_id_registered) {
			(void) _getdns_rbtree_delete(
			    netreq->query_id_registered, netreq->node.key);
			netreq->query_id_registered = NULL;
			netreq->node.key = NULL;
		}
		DEBUG_STUB("%s %-35s: MSG: %p (read)\n",
		    STUB_DEBUG_READ, __FUNC__, (void*)netreq);
		_getdns_netreq_change_state(netreq, NET_REQ_FINISHED);
		netreq->response = upstream->tcp.read_buf;
		netreq->response_len =
		    upstream->tcp.read_pos - upstream->tcp.read_buf;
		upstream->tcp.read_buf = NULL;
		upstream->responses_received++;
		
		/* !THIS CODE NEEDS TESTING! */
		if (netreq->owner->edns_cookies &&
		    match_and_process_server_cookie(
		    netreq->upstream, upstream->tcp.read_buf,
		    upstream->tcp.read_pos - upstream->tcp.read_buf))
			return; /* Client cookie didn't match (or FORMERR) */

		if (netreq->owner->context->idle_timeout != 0)
		     process_keepalive(netreq->upstream, netreq, netreq->response,
		                       netreq->response_len);

		netreq->debug_end_time = _getdns_get_time_as_uintt64();
		/* This also reschedules events for the upstream*/
		stub_cleanup(netreq);

		if (!upstream->is_sync_loop || netreq->owner->is_sync_request)
			_getdns_check_dns_req_complete(netreq->owner);

		else {
			assert(upstream->is_sync_loop &&
			    !netreq->owner->is_sync_request);

			/* We have a result for an asynchronously scheduled
			 * netreq, while processing the synchronous loop.
			 * Queue dns_req_complete checks.
			 */

			/* First check if one for the dns_req already exists */
			for ( dnsreq = upstream->finished_dnsreqs
			    ; dnsreq && dnsreq != netreq->owner
			    ; dnsreq = dnsreq->finished_next)
				; /* pass */

			if (!dnsreq) {
				/* Schedule dns_req_complete check for this
				 * netreq's owner
				 */
				dnsreq = netreq->owner;
				dnsreq->finished_next =
				    upstream->finished_dnsreqs;
				upstream->finished_dnsreqs = dnsreq;
			
				if (!upstream->finished_event.timeout_cb) {
					upstream->finished_event.timeout_cb
					    = process_finished_cb;
					GETDNS_SCHEDULE_EVENT(
					    dnsreq->context->extension,
					    -1, 1, &upstream->finished_event);
				}
			}
		}

		/* WSA TODO: if callback is still upstream_read_cb, do it again
		 */
		return;
	}
}

static void
upstream_write_cb(void *userarg)
{
	getdns_upstream *upstream = (getdns_upstream *)userarg;
	getdns_network_req *netreq = upstream->write_queue;
	int q;
	_getdns_tls_x509 *cert;

	if (!netreq) {
		GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
		upstream->event.write_cb = NULL;
		return;
	}

	netreq->debug_start_time = _getdns_get_time_as_uintt64();
	DEBUG_STUB("%s %-35s: MSG: %p (writing)\n", STUB_DEBUG_WRITE,
	            __FUNC__, (void*)netreq);

	/* Health checks on current connection */
	if (upstream->conn_state == GETDNS_CONN_TEARDOWN ||
	    upstream->conn_state == GETDNS_CONN_CLOSED ||  
	    upstream->fd == -1)
		q = STUB_CONN_GONE;
	else if (!upstream_working_ok(upstream))
		q = STUB_TCP_ERROR;
	/* Seems ok, now try to write */
	else if (tls_requested(netreq))
		q = stub_tls_write(upstream, &upstream->tcp, netreq);
	else
		q = stub_tcp_write(upstream->fd, &upstream->tcp, netreq);

	switch (q) {
	case STUB_TCP_MORE_TO_WRITE:
		/* WSA TODO: if callback is still upstream_write_cb, do it again
		 */
	case STUB_TCP_RETRY:
		return;
	case STUB_OUT_OF_OPTIONS:
	case STUB_TCP_ERROR:
		/* New problem with the TCP connection itself. Need to fallback.*/
		/* Fall through */
	case STUB_SETUP_ERROR:
		/* Could not complete the set up. Need to fallback.*/
		DEBUG_STUB("%s %-35s: Upstream: %p ERROR = %d\n", STUB_DEBUG_WRITE,
		             __FUNC__, (void*)userarg, q);
		upstream_failed(upstream, (q == STUB_TCP_ERROR ? 0:1));
		return;
	case STUB_CONN_GONE:
	case STUB_NO_AUTH:
		/* Cleaning up after connection or auth check failure. Need to fallback. */
		stub_cleanup(netreq);
		_getdns_upstream_log(upstream, GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_DEBUG,
		    "%-40s : Conn closed: %s - *Failure*\n",
		    upstream->addr_str,
		    (upstream->transport == GETDNS_TRANSPORT_TLS ? "TLS" : "TCP"));
		if (fallback_on_write(netreq) == STUB_TCP_ERROR) {
			/* TODO: Need new state to report transport unavailable*/
			_getdns_netreq_change_state(netreq, NET_REQ_ERRORED);
			_getdns_check_dns_req_complete(netreq->owner);
		}
		return;

	default:
		/* Unqueue the netreq from the write_queue */
		remove_from_write_queue(upstream, netreq);

		if (netreq->owner->return_call_reporting &&
		    netreq->upstream->tls_obj) {
			if (netreq->debug_tls_peer_cert.data == NULL &&
			    (cert = _getdns_tls_connection_get_peer_certificate(&upstream->upstreams->mf, netreq->upstream->tls_obj))) {
				_getdns_tls_x509_to_der(&upstream->upstreams->mf, cert, &netreq->debug_tls_peer_cert);
				_getdns_tls_x509_free(&upstream->upstreams->mf, cert);
			}
			netreq->debug_tls_version = _getdns_tls_connection_get_version(netreq->upstream->tls_obj);
		}
		/* Need this because auth status is reset on connection close */
		netreq->debug_tls_auth_status = netreq->upstream->tls_auth_state;
		upstream->queries_sent++;

		/* Empty write_queue?, then deschedule upstream write_cb */
		if (upstream->write_queue == NULL) {
			assert(upstream->write_queue_last == NULL);
			GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
			upstream->event.write_cb = NULL;
			/* Reschedule (if already reading) to clear writable */
			if (upstream->event.read_cb) {
				GETDNS_SCHEDULE_EVENT(upstream->loop,
				    upstream->fd, TIMEOUT_FOREVER,
				    &upstream->event);
			}
		}
		/* Schedule reading (if not already scheduled) */
		if (!upstream->event.read_cb) {
			GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
			upstream->event.read_cb = upstream_read_cb;
			GETDNS_SCHEDULE_EVENT(upstream->loop,
			    upstream->fd, TIMEOUT_FOREVER, &upstream->event);
		}
		/* WSA TODO: if callback is still upstream_write_cb, do it again
		 */
		return;
	}
}


/*****************************/
/* Upstream utility functions*/
/*****************************/

static int
upstream_working_ok(getdns_upstream *upstream) 
{
	/* [TLS1]TODO: This arbitrary logic at the moment - review and improve!*/
	return (upstream->responses_timeouts > 
	        upstream->responses_received*
	        upstream->upstreams->tls_connection_retries ? 0 : 1);
}

static int
upstream_active(getdns_upstream *upstream) 
{
	if ((upstream->conn_state == GETDNS_CONN_SETUP || 
	     upstream->conn_state == GETDNS_CONN_OPEN) &&
	     upstream->keepalive_shutdown == 0)
		return 1;
	return 0;
}

static int
upstream_usable(getdns_upstream *upstream, int backoff_ok) 
{
	/* If backoff_ok is not true then only use upstreams that are in a healthy
	   state. */
	if ((upstream->conn_state == GETDNS_CONN_CLOSED || 
	     upstream->conn_state == GETDNS_CONN_SETUP || 
	     upstream->conn_state == GETDNS_CONN_OPEN) &&
	     upstream->keepalive_shutdown == 0)
		return 1;
	/* Otherwise, allow upstreams that are backed off to be used because that
	   is better that having no upstream at all. */
	if (backoff_ok == 1 &&
	    upstream->conn_state == GETDNS_CONN_BACKOFF)
		return 1;
	return 0;
}

static int
upstream_auth_status_ok(getdns_upstream *upstream, getdns_network_req *netreq) {
	if (netreq->tls_auth_min != GETDNS_AUTHENTICATION_REQUIRED)
		return 1;
	return (upstream->tls_auth_state == GETDNS_AUTH_OK ? 1 : 0);
}

static int
upstream_stats(getdns_upstream *upstream)
{
	/* [TLS1]TODO: This arbitrary logic at the moment - review and improve!*/
	return (upstream->total_responses - upstream->total_timeouts
	        - upstream->conn_shutdowns*GETDNS_TRANSPORT_FAIL_MULT
	        - upstream->conn_setup_failed);
}

static int
upstream_valid(getdns_upstream *upstream,
                          getdns_transport_list_t transport,
                          getdns_network_req *netreq,
                          int backoff_ok)
{
	/* Checking upstreams with backoff_ok true will also return upstreams
	   that are in a backoff state. Otherwise only use upstreams that have
	   a 'good' connection state. backoff_ok is useful when no upstreams at all
	   are valid, for example when the network connection is down and need to 
	   keep trying to connect before failing completely. */
	if (!(upstream->transport == transport && upstream_usable(upstream, backoff_ok)))
		return 0;
	if (transport == GETDNS_TRANSPORT_TCP)
		return 1;
	if (upstream->conn_state == GETDNS_CONN_OPEN) {
		if (!upstream_auth_status_ok(upstream, netreq))
			return 0;
		else
			return 1;
	}
	/* We need to check past authentication history to see if this is usable for TLS.*/
	if (netreq->tls_auth_min != GETDNS_AUTHENTICATION_REQUIRED)
		return 1;
	return ((upstream->best_tls_auth_state == GETDNS_AUTH_OK ||
	         upstream->best_tls_auth_state == GETDNS_AUTH_NONE) ? 1 : 0);
}

static int
upstream_valid_and_open(getdns_upstream *upstream,
                        getdns_transport_list_t transport,
                        getdns_network_req *netreq)
{
	if (!(upstream->transport == transport && upstream_active(upstream)))
		return 0;
	if (transport == GETDNS_TRANSPORT_TCP)
		return 1;
	/* Connection is complete, we know the auth status so check*/
	if (upstream->conn_state == GETDNS_CONN_OPEN && 
	    !upstream_auth_status_ok(upstream, netreq)) 
		return 0;
	/* We must have a TLS connection still setting up so schedule and the
	   write code will check again once the connection is complete*/
	 return 1;
}

static int
other_transports_working(getdns_network_req *netreq,
                         getdns_upstreams *upstreams,
                         getdns_transport_list_t transport)
{
	size_t i,j;
	for (i = 0; i< netreq->transport_count;i++) {
		if (netreq->transports[i] == transport)
			continue;
		if (netreq->transports[i] == GETDNS_TRANSPORT_UDP) {
			for (j = 0; j < upstreams->count; j+=GETDNS_UPSTREAM_TRANSPORTS) {
				if (upstreams->upstreams[j].back_off == 1)
					return 1;
			}
		}
		else if (netreq->transports[i] == GETDNS_TRANSPORT_TCP ||
		         netreq->transports[i] == GETDNS_TRANSPORT_TLS) {
			for (j = 0; j < upstreams->count; j++) {
				if (netreq->transports[i] == upstreams->upstreams[j].transport &&
				    upstream_valid(&upstreams->upstreams[j], netreq->transports[i],
					netreq, 0))
					return 1;
			}
		}
	}
	return 0;
}

static getdns_upstream *
upstream_select_stateful(getdns_network_req *netreq, getdns_transport_list_t transport)
{
	getdns_upstream *upstream = NULL;
	getdns_upstreams *upstreams = netreq->owner->upstreams;
	size_t i;
	time_t now = time(NULL);

	if (!upstreams->count)
		return NULL;

	/* A check to re-instate backed-off upstreams after X amount of time*/
	for (i = 0; i < upstreams->count; i++) {
		if (upstreams->upstreams[i].conn_state == GETDNS_CONN_BACKOFF &&
		    upstreams->upstreams[i].conn_retry_time < now) {
			upstreams->upstreams[i].conn_state = GETDNS_CONN_CLOSED;
			_getdns_upstream_log(upstream, GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_WARNING,
			    "%-40s : Upstream   : Re-instating %s for this upstream\n",
			     upstreams->upstreams[i].addr_str,
			     upstreams->upstreams[i].transport == GETDNS_TRANSPORT_TLS ? "TLS" : "TCP");
		}
	}

	if (netreq->owner->context->round_robin_upstreams == 0) {
		/* First find if an open upstream has the correct properties and use that*/
		for (i = 0; i < upstreams->count; i++) {
			if (upstream_valid_and_open(&upstreams->upstreams[i], transport, netreq)) 
				return &upstreams->upstreams[i];
		}
	}

	/* OK - Find the next one to use. First check we have at least one valid
	   upstream (not backed-off). Because we completely back off failed 
	   upstreams we may have no valid upstream at all (in contrast to UDP).*/
	i = upstreams->current_stateful;
	do {
		DEBUG_STUB("%s %-35s: Testing upstreams  %d %d for transport %d \n",
	            STUB_DEBUG_SETUP, __FUNC__, (int)i,
	            (int)upstreams->upstreams[i].conn_state, transport);
		if (upstream_valid(&upstreams->upstreams[i], transport, netreq, 0)) {
			upstream = &upstreams->upstreams[i];
			break;
		}
		i++;
		if (i >= upstreams->count)
			i = 0;
	} while (i != upstreams->current_stateful);
	if (!upstream) {
		/* Oh, oh. We have no valid upstreams for this transport. */
		/* If there are other fallback transports that are working, we should 
		   use them before forcibly promoting failed upstreams for re-try, since 
		   waiting for the the re-try timer to re-instate them is the right thing 
		   in this case. */
		if (other_transports_working(netreq, upstreams, transport))
			return NULL;

		/* Try to find one that might work so
		   allow backed off upstreams to be considered valid.
		   Don't worry about the policy, just use the one with the least bad
		   stats that still fits the bill (right transport, right authentication)
		   to try to avoid total failure due to network outages. */
		do {
			if (upstream_valid(&upstreams->upstreams[i], transport, netreq, 1)) {
				upstream = &upstreams->upstreams[i];
				break;
			}
			i++;
			if (i >= upstreams->count)
				i = 0;
		} while (i != upstreams->current_stateful);
		if (!upstream) {
			/* We _really_ have nothing that authenticates well enough right now...
			   leave to regular backoff logic. */
			return NULL;
		}
		do {
			i++;
			if (i >= upstreams->count)
				i = 0;
			if (upstream_valid(&upstreams->upstreams[i], transport, netreq, 1) &&
			    upstream_stats(&upstreams->upstreams[i]) > upstream_stats(upstream))
				upstream = &upstreams->upstreams[i];
		} while (i != upstreams->current_stateful);
		upstream->conn_state = GETDNS_CONN_CLOSED;
		upstream->conn_backoff_interval = 1;
		_getdns_upstream_log(upstream, GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_NOTICE,
		    "%-40s : Upstream   : No valid upstreams for %s... promoting this backed-off upstream for re-try...\n",
		    upstream->addr_str,
		    upstream->transport == GETDNS_TRANSPORT_TLS ? "TLS" : "TCP");
		return upstream;
	}

	/* Now select the specific upstream */
	if (netreq->owner->context->round_robin_upstreams == 0) {
		/* Base the decision on the stats and being not backed-off, 
		   noting we will have started from 0*/
		for (i++; i < upstreams->count; i++) {
			if (upstream_valid(&upstreams->upstreams[i], transport, netreq, 0) &&
			    upstream_stats(&upstreams->upstreams[i]) > upstream_stats(upstream))
				upstream = &upstreams->upstreams[i];
		}
	} else {
		/* Simplistic, but always just pick the first one, incrementing the current.
		   Note we are not distinguishing TCP/TLS here....*/
		upstreams->current_stateful+=GETDNS_UPSTREAM_TRANSPORTS;
		if (upstreams->current_stateful >= upstreams->count)
			upstreams->current_stateful = 0;
	}

	return upstream;
}

/* Used for UDP only */
static getdns_upstream *
upstream_select(getdns_network_req *netreq)
{
	getdns_upstream *upstream;
	getdns_upstreams *upstreams = netreq->owner->upstreams;
	size_t i;

	if (!upstreams->count)
		return NULL;

	/* First UPD/TCP upstream is always at i=0 and then start of each upstream block*/
	/* TODO: Have direct access to sets of upstreams for different transports*/
	for (i = 0; i < upstreams->count; i+=GETDNS_UPSTREAM_TRANSPORTS)
		if (upstreams->upstreams[i].to_retry <= 0)
			upstreams->upstreams[i].to_retry++;

	i = upstreams->current_udp;
	do {
		if (upstreams->upstreams[i].to_retry > 0) {
			upstreams->current_udp = i;
			return &upstreams->upstreams[i];
		}
		i+=GETDNS_UPSTREAM_TRANSPORTS;
		if (i >= upstreams->count)
			i = 0;
	} while (i != upstreams->current_udp);

	/* Select upstream with the lowest back_off value */
	upstream = upstreams->upstreams;
	for (i = 0; i < upstreams->count; i+=GETDNS_UPSTREAM_TRANSPORTS)
		if (upstreams->upstreams[i].back_off < upstream->back_off)
			upstream = &upstreams->upstreams[i];

	/* Restrict back_off in case no upstream is available to achieve
	   (more or less) round-robin retry on all upstreams. */
	if (upstream->back_off > 4) {
		for (i = 0; i < upstreams->count; i+=GETDNS_UPSTREAM_TRANSPORTS)
			upstreams->upstreams[i].back_off = 2;
	}
	upstream->to_retry = 1;
	upstreams->current_udp = upstream - upstreams->upstreams;
	return upstream;
}

int
upstream_connect(getdns_upstream *upstream, getdns_transport_list_t transport,
                    getdns_dns_req *dnsreq) 
{
	DEBUG_STUB("%s %-35s: Getting upstream connection:  %p\n", STUB_DEBUG_SETUP, 
	           __FUNC__, (void*)upstream);
	int fd = -1;
	switch(transport) {
	case GETDNS_TRANSPORT_UDP:
		if ((fd = socket(
		    upstream->addr.ss_family, SOCK_DGRAM, IPPROTO_UDP)) == -1)
			return -1;
		getdns_sock_nonblock(fd);
		break;

	case GETDNS_TRANSPORT_TCP:
	case GETDNS_TRANSPORT_TLS:
		/* Use existing if available*/
		if (upstream->fd != -1)
			return upstream->fd;
		fd = tcp_connect(upstream, transport);
		if (fd == -1) {
			upstream_failed(upstream, 1);
			return -1;
		}
		upstream->loop = dnsreq->loop;
		upstream->is_sync_loop = dnsreq->is_sync_request;
		upstream->fd = fd;
		if (transport == GETDNS_TRANSPORT_TLS) {
			upstream->tls_obj = tls_create_object(dnsreq, fd, upstream);
			if (upstream->tls_obj == NULL) {
				upstream_failed(upstream, 1);
				_getdns_closesocket(fd);
				return -1;
			}
			upstream->tls_hs_state = GETDNS_HS_WRITE;
		}
		upstream->conn_state = GETDNS_CONN_SETUP;
		_getdns_upstream_log(upstream, GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_DEBUG,
		    "%-40s : Conn opened: %s - %s Profile\n", 
		    upstream->addr_str, transport == GETDNS_TRANSPORT_TLS ? "TLS":"TCP",
		dnsreq->context->tls_auth_min == GETDNS_AUTHENTICATION_NONE ? "Opportunistic":"Strict");
		break;
	default:
		return -1;
		/* Nothing to do*/
	}
	return fd;
}

static getdns_upstream*
upstream_find_for_transport(getdns_network_req *netreq,
                            getdns_transport_list_t transport,
                            int *fd)
{
	getdns_upstream *upstream = NULL;

	/*  UDP always returns an upstream, the only reason this will fail is if
	    no socket is available, in which case that is an error.*/
	if (transport == GETDNS_TRANSPORT_UDP) {
		upstream = upstream_select(netreq);
		*fd = upstream_connect(upstream, transport, netreq->owner);
		return upstream;
	}
	else {
		/* For stateful transport we should keep trying until all our transports
		   are exhausted/backed-off (no upstream) and until we have tried each
		   upstream at least once for this netreq in a total backoff scenario */
		size_t i = 0;
		do {
			upstream = upstream_select_stateful(netreq, transport);
			if (!upstream)
				return NULL;
			*fd = upstream_connect(upstream, transport, netreq->owner);
			if (i >= upstream->upstreams->count)
				return NULL;
			i++;
		} while (*fd == -1);
		DEBUG_STUB("%s %-35s: FD:  %d Connecting to upstream: %p   No: %d\n", 
	           STUB_DEBUG_SETUP, __FUNC__, *fd, (void*)upstream,
	           (int)(upstream - netreq->owner->context->upstreams->upstreams));
	}
	return upstream;
}

static int
upstream_find_for_netreq(getdns_network_req *netreq)
{
	int fd = -1;
	getdns_upstream *upstream;
	size_t i;

	for (i = netreq->transport_current; 
	     i < netreq->transport_count; i++) {
		upstream = upstream_find_for_transport(netreq,
		                                  netreq->transports[i],
		                                  &fd);
		if (!upstream)
			continue;

		if (fd == -1) {
			if (_getdns_resource_depletion())
				return STUB_TRY_AGAIN_LATER;
			return -1;
		}
		if (upstream == netreq->first_upstream)
			continue;

		netreq->transport_current = i;
		netreq->upstream = upstream;
		if (!netreq->first_upstream)
			netreq->first_upstream = upstream;
		netreq->keepalive_sent = 0;

		DEBUG_STUB("%s %-35s: MSG: %p found upstream %p with transport %d, fd: %d\n", STUB_DEBUG_SCHEDULE, __FUNC__, (void*)netreq, (void *)upstream, (int)netreq->transports[i], fd);
		return fd;
	}
	/* Handle better, will give generic error*/
	_getdns_log(&netreq->owner->context->log
	    , GETDNS_LOG_UPSTREAM_STATS, GETDNS_LOG_ERR
	    , "   *FAILURE* no valid transports or upstreams available!\n");
	return -1;
}

/************************/
/* Scheduling functions */
/***********************/

static int
fallback_on_write(getdns_network_req *netreq) 
{
	uint64_t now_ms = 0;

	/* Deal with UDP one day*/
	DEBUG_STUB("%s %-35s: MSG: %p FALLING BACK \n", STUB_DEBUG_SCHEDULE, __FUNC__, (void*)netreq);

	/* Try to find a fallback transport*/
	getdns_return_t result = _getdns_submit_stub_request(netreq, &now_ms);

	if (result != GETDNS_RETURN_GOOD)
		return STUB_TCP_ERROR;

	return (netreq->transports[netreq->transport_current] 
	         == GETDNS_TRANSPORT_UDP) ?
	      netreq->fd : netreq->upstream->fd;
}

static void
upstream_reschedule_events(getdns_upstream *upstream) {

	DEBUG_STUB("%s %-35s: FD:  %d \n", STUB_DEBUG_SCHEDULE, 
	             __FUNC__, upstream->fd);
	if (upstream->event.ev)
		GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);

	if (upstream->fd == -1 || !(  upstream->conn_state == GETDNS_CONN_SETUP
	                           || upstream->conn_state == GETDNS_CONN_OPEN ))
		return;

	if (!upstream->write_queue && upstream->event.write_cb) {
		upstream->event.write_cb = NULL;
	}
	if (upstream->write_queue && !upstream->event.write_cb) {
		upstream->event.write_cb = upstream_write_cb;
	}
	if (!upstream->netreq_by_query_id.count && upstream->event.read_cb) {
		upstream->event.read_cb = NULL;
	}
	if (upstream->netreq_by_query_id.count && !upstream->event.read_cb) {
		upstream->event.read_cb = upstream_read_cb;
	}
	if (upstream->event.read_cb || upstream->event.write_cb)
		GETDNS_SCHEDULE_EVENT(upstream->loop,
		    upstream->fd, TIMEOUT_FOREVER, &upstream->event);
	else {
		DEBUG_STUB("%s %-35s: FD:  %d Connection idle - timeout is %d\n", 
			    STUB_DEBUG_SCHEDULE, __FUNC__, upstream->fd,
			    (int)upstream->keepalive_timeout);

		upstream->event.read_cb = upstream_read_cb;
		upstream->event.timeout_cb = upstream_idle_timeout_cb;
		GETDNS_SCHEDULE_EVENT(upstream->loop, upstream->fd, 
		    upstream->keepalive_timeout, &upstream->event);
	}
}

static void
upstream_schedule_netreq(getdns_upstream *upstream, getdns_network_req *netreq)
{
	DEBUG_STUB("%s %-35s: MSG: %p (schedule event)\n", STUB_DEBUG_SCHEDULE, __FUNC__, (void*)netreq);
	/* We have a connected socket and a global event loop */
	assert(upstream->fd >= 0);
	assert(upstream->loop);

	/* Append netreq to write_queue */
	if (!upstream->write_queue) {
		upstream->write_queue = upstream->write_queue_last = netreq;
		GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
		if (netreq->owner->is_sync_request && !upstream->is_sync_loop){
			/* An initial synchronous call, change loop */
			upstream->loop = netreq->owner->loop;
			upstream->is_sync_loop = 1;
		}
		upstream->event.timeout_cb = NULL;
		upstream->event.write_cb = upstream_write_cb;
		if (upstream->queries_sent == 0) {
			/* Set a timeout on the upstream so we can catch failed setup*/
			upstream->event.timeout_cb = upstream_setup_timeout_cb;
			GETDNS_SCHEDULE_EVENT(upstream->loop, upstream->fd,
			    _getdns_ms_until_expiry(netreq->owner->expires)/5*4,
			    &upstream->event);
#if defined(HAVE_DECL_TCP_FASTOPEN) && HAVE_DECL_TCP_FASTOPEN \
 && !(defined(HAVE_DECL_TCP_FASTOPEN_CONNECT) && HAVE_DECL_TCP_FASTOPEN_CONNECT) \
 && !(defined(HAVE_DECL_MSG_FASTOPEN) && HAVE_DECL_MSG_FASTOPEN)
			if (upstream->transport == GETDNS_TRANSPORT_TCP) {
				/* Write immediately! */
				upstream_write_cb(upstream);
			}
#endif
		} else {
			GETDNS_SCHEDULE_EVENT(upstream->loop,
			    upstream->fd, TIMEOUT_FOREVER, &upstream->event);
		}
	} else if (netreq->owner->is_sync_request && !upstream->is_sync_loop) {
		/* Initial synchronous call on an upstream in use,
		 * prioritize this request (insert at 0)
		 * and reschedule against synchronous loop.
		 */
		netreq->write_queue_tail = upstream->write_queue;
		upstream->write_queue = netreq;
		GETDNS_CLEAR_EVENT(upstream->loop, &upstream->event);
		upstream->loop = netreq->owner->loop;
		upstream->is_sync_loop = 1;
		GETDNS_SCHEDULE_EVENT(upstream->loop, upstream->fd,
		    TIMEOUT_FOREVER, &upstream->event);

	} else {
		/* "Follow-up synchronous" or Asynchronous call,
		 * this request comes last (append)
		 */
		upstream->write_queue_last->write_queue_tail = netreq;
		upstream->write_queue_last = netreq;
	}
}

getdns_return_t
_getdns_submit_stub_request(getdns_network_req *netreq, uint64_t *now_ms)
{
	int fd = -1;
	getdns_dns_req *dnsreq;
	getdns_context *context;

	DEBUG_STUB("%s %-35s: MSG: %p TYPE: %d\n", STUB_DEBUG_ENTRY, __FUNC__,
	           (void*)netreq, netreq->request_type);

	dnsreq = netreq->owner;
	context = dnsreq->context;

	/* This does a best effort to get a initial fd.
	 * All other set up is done async*/
	fd = upstream_find_for_netreq(netreq);
	if (fd == -1)
		return GETDNS_RETURN_NO_UPSTREAM_AVAILABLE;

	else if (fd == STUB_TRY_AGAIN_LATER) {
		_getdns_netreq_change_state(netreq, NET_REQ_NOT_SENT);
		netreq->node.key = netreq;
		if (_getdns_rbtree_insert(
		    &context->pending_netreqs, &netreq->node))
			return GETDNS_RETURN_GOOD;
		return GETDNS_RETURN_NO_UPSTREAM_AVAILABLE;
	}
	switch(netreq->transports[netreq->transport_current]) {
	case GETDNS_TRANSPORT_UDP:
		netreq->fd = fd;
		GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);
		GETDNS_SCHEDULE_EVENT(dnsreq->loop, netreq->fd,
		    _getdns_ms_until_expiry2(dnsreq->expires, now_ms),
		    getdns_eventloop_event_init(&netreq->event, netreq,
		    NULL, stub_udp_write_cb, stub_timeout_cb));
		return GETDNS_RETURN_GOOD;

	case GETDNS_TRANSPORT_TLS:
	case GETDNS_TRANSPORT_TCP:
		GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);
		GETDNS_SCHEDULE_EVENT(
		    dnsreq->loop, -1,
		    _getdns_ms_until_expiry2(dnsreq->expires, now_ms),
		    getdns_eventloop_event_init(
		    &netreq->event, netreq, NULL, NULL,
		    stub_timeout_cb));
		upstream_schedule_netreq(netreq->upstream, netreq);
		return GETDNS_RETURN_GOOD;
	default:
		return GETDNS_RETURN_GENERIC_ERROR;
	}
}

/* stub.c */
