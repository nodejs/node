/**
 *
 * \file debug.h
 * /brief Macro's for debugging
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

#ifndef DEBUG_H
#define DEBUG_H

#include "config.h"
#define STUB_DEBUG_ENTRY     "=> ENTRY:       "
#define STUB_DEBUG_SETUP     "--- SETUP:      "
#define STUB_DEBUG_SETUP_TLS "--- SETUP(TLS): "
#define STUB_DEBUG_TSIG      "--- TSIG:       "
#define STUB_DEBUG_SCHEDULE  "----- SCHEDULE: "
#define STUB_DEBUG_READ      "------- READ:   "
#define STUB_DEBUG_WRITE     "------- WRITE:  "
#define STUB_DEBUG_CLEANUP   "--- CLEANUP:    "

#ifdef GETDNS_ON_WINDOWS
#define DEBUG_ON(...) do { \
		struct timeval tv_dEbUgSyM; \
		struct tm tm_dEbUgSyM; \
		char buf_dEbUgSyM[10]; \
		time_t tsec_dEbUgSyM; \
		\
		gettimeofday(&tv_dEbUgSyM, NULL); \
		tsec_dEbUgSyM = (time_t) tv_dEbUgSyM.tv_sec; \
		gmtime_s(&tm_dEbUgSyM, (const time_t *) &tsec_dEbUgSyM); \
		strftime(buf_dEbUgSyM, 10, "%H:%M:%S", &tm_dEbUgSyM); \
		fprintf(stderr, "[%s.%.6d] ", buf_dEbUgSyM, (int)tv_dEbUgSyM.tv_usec); \
		fprintf(stderr, __VA_ARGS__); \
	} while (0)

#define DEBUG_NL(...) do {		    \
		struct timeval tv_dEbUgSyM; \
		struct tm tm_dEbUgSyM; \
		char buf_dEbUgSyM[10]; \
		time_t tsec_dEbUgSyM; \
		\
		gettimeofday(&tv_dEbUgSyM, NULL); \
		tsec_dEbUgSyM = (time_t) tv_dEbUgSyM.tv_sec; \
		gmtime_s(&tm_dEbUgSyM, (const time_t *) &tsec_dEbUgSyM); \
		strftime(buf_dEbUgSyM, 10, "%H:%M:%S", &tm_dEbUgSyM); \
		fprintf(stderr, "[%s.%.6d] ", buf_dEbUgSyM, (int)tv_dEbUgSyM.tv_usec); \
		fprintf(stderr, __VA_ARGS__); \
	} while (0)
#else
#define DEBUG_ON(...) do { \
		struct timeval tv_dEbUgSyM; \
		struct tm tm_dEbUgSyM; \
		char buf_dEbUgSyM[10]; \
		\
		gettimeofday(&tv_dEbUgSyM, NULL); \
		gmtime_r(&tv_dEbUgSyM.tv_sec, &tm_dEbUgSyM); \
		strftime(buf_dEbUgSyM, 10, "%H:%M:%S", &tm_dEbUgSyM); \
		fprintf(stderr, "[%s.%.6d] ", buf_dEbUgSyM, (int)tv_dEbUgSyM.tv_usec); \
		fprintf(stderr, __VA_ARGS__); \
	} while (0)

#define DEBUG_NL(...) do {		    \
		struct timeval tv_dEbUgSyM; \
		struct tm tm_dEbUgSyM; \
		char buf_dEbUgSyM[10]; \
		\
		gettimeofday(&tv_dEbUgSyM, NULL); \
		gmtime_r(&tv_dEbUgSyM.tv_sec, &tm_dEbUgSyM); \
		strftime(buf_dEbUgSyM, 10, "%H:%M:%S", &tm_dEbUgSyM); \
		fprintf(stderr, "[%s.%.6d] ", buf_dEbUgSyM, (int)tv_dEbUgSyM.tv_usec); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
	} while (0)
#endif

#define DEBUG_OFF(...) do {} while (0)

#if defined(REQ_DEBUG) && REQ_DEBUG
#include <time.h>
#define DEBUG_REQ(...) DEBUG_ON(__VA_ARGS__)
#include "gldns/wire2str.h"
#include "rr-dict.h"
#include "types-internal.h"
static inline void debug_req(const char *msg, getdns_network_req *netreq)
{
	char str[1024];
	struct timeval tv;
	uint64_t t;

	(void) gettimeofday(&tv, NULL);
	t = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	t = t >= netreq->owner->expires ? 0 : netreq->owner->expires - t;
	(void) gldns_wire2str_dname_buf(netreq->owner->name,
	    netreq->owner->name_len, str, sizeof(str));
	DEBUG_REQ("NETREQ %s %4"PRIu64" %s %s\n", msg, t,
	    str, _getdns_rr_type_name(netreq->request_type));
}
#else
#define DEBUG_REQ(...) DEBUG_OFF(__VA_ARGS__)
#define debug_req(...) DEBUG_OFF(__VA_ARGS__)
#endif

#if defined(SCHED_DEBUG) && SCHED_DEBUG
#include <time.h>
#define DEBUG_SCHED(...) DEBUG_ON(__VA_ARGS__)
#else
#define DEBUG_SCHED(...) DEBUG_OFF(__VA_ARGS__)
#endif

#if defined(STUB_DEBUG) && STUB_DEBUG
#include <time.h>
#define DEBUG_STUB(...) DEBUG_ON(__VA_ARGS__)
#else
#define DEBUG_STUB(...) DEBUG_OFF(__VA_ARGS__)
#endif

#if defined(DAEMON_DEBUG) && DAEMON_DEBUG
#include <time.h>
#define DEBUG_DAEMON(...) DEBUG_ON(__VA_ARGS__)
#else
#define DEBUG_DAEMON(...) DEBUG_OFF(__VA_ARGS__)
#endif

#if defined(SEC_DEBUG) && SEC_DEBUG
#include <time.h>
#define DEBUG_SEC(...) DEBUG_ON(__VA_ARGS__)
#else
#define DEBUG_SEC(...) DEBUG_OFF(__VA_ARGS__)
#endif

#if defined(SERVER_DEBUG) && SERVER_DEBUG
#include <time.h>
#define DEBUG_SERVER(...) DEBUG_ON(__VA_ARGS__)
#else
#define DEBUG_SERVER(...) DEBUG_OFF(__VA_ARGS__)
#endif

#define MDNS_DEBUG_ENTRY     "-> MDNS ENTRY:  "
#define MDNS_DEBUG_READ      "-- MDNS READ:   "
#define MDNS_DEBUG_MREAD      "-- MDNS MREAD:  "
#define MDNS_DEBUG_WRITE     "-- MDNS WRITE:  "
#define MDNS_DEBUG_CLEANUP   "-- MDNS CLEANUP:"

#if defined(MDNS_DEBUG) && MDNS_DEBUG
#include <time.h>
#define DEBUG_MDNS(...) DEBUG_ON(__VA_ARGS__)
#else
#define DEBUG_MDNS(...) DEBUG_OFF(__VA_ARGS__)
#endif

#if defined(ANCHOR_DEBUG) && ANCHOR_DEBUG
#include <time.h>
#define DEBUG_ANCHOR(...) DEBUG_ON(__VA_ARGS__)
#else
#define DEBUG_ANCHOR(...) DEBUG_OFF(__VA_ARGS__)
#endif

#if (defined(REQ_DEBUG)    && REQ_DEBUG)    || \
    (defined(SCHED_DEBUG)  && SCHED_DEBUG)  || \
    (defined(STUB_DEBUG)   && STUB_DEBUG)   || \
    (defined(DAEMON_DEBUG) && DAEMON_DEBUG) || \
    (defined(SEC_DEBUG)    && SEC_DEBUG)    || \
    (defined(SERVER_DEBUG) && SERVER_DEBUG) || \
    (defined(MDNS_DEBUG)   && MDNS_DEBUG)   || \
    (defined(ANCHOR_DEBUG) && ANCHOR_DEBUG)
#define DEBUGGING 1
static inline int
_getdns_ERR_print_errors_cb_f(const char *str, size_t len, void *u)
{ DEBUG_ON("%.*s (u: %p)\n", (int)len, str, u); return 1; }
#endif

#endif
/* debug.h */
