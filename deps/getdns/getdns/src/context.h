/**
 *
 * \file context.h
 * @brief getdns context management functions
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

#ifndef _GETDNS_CONTEXT_H_
#define _GETDNS_CONTEXT_H_

#include "getdns/getdns.h"
#include "getdns/getdns_extra.h"
#include "config.h"
#include "types-internal.h"
#include "extension/default_eventloop.h"
#include "util/rbtree.h"
#include "ub_loop.h"
#include "server.h"
#ifdef HAVE_MDNS_SUPPORT
#include "util/lruhash.h"
#endif
#include "rr-iter.h"
#include "anchor.h"
#include "tls.h"

struct getdns_dns_req;
struct ub_ctx;

enum filechgs { GETDNS_FCHG_ERRORS = -1
 , GETDNS_FCHG_NOERROR   = 0
 , GETDNS_FCHG_NOCHANGES = 0
 , GETDNS_FCHG_MTIME     = 1
 , GETDNS_FCHG_CTIME     = 2};

/** function pointer typedefs */
typedef void (*getdns_update_callback) (struct getdns_context *,
    getdns_context_code_t);

typedef void (*getdns_update_callback2) (struct getdns_context *,
    getdns_context_code_t, void *userarg);

/* internal use only for detecting changes to system files */
struct filechg {
	char fn[_GETDNS_PATH_MAX];
	int  changes;
	int  errors;
	struct stat prevstat;
};

typedef enum getdns_tls_hs_state {
	GETDNS_HS_NONE,
	GETDNS_HS_WRITE,
	GETDNS_HS_READ,
	GETDNS_HS_DONE,
	GETDNS_HS_FAILED
} getdns_tls_hs_state_t;

typedef enum getdns_conn_state {
	GETDNS_CONN_CLOSED,
	GETDNS_CONN_SETUP,
	GETDNS_CONN_OPEN,
	GETDNS_CONN_TEARDOWN,
	GETDNS_CONN_BACKOFF
} getdns_conn_state_t;

typedef enum getdns_tasrc {
	GETDNS_TASRC_NONE,
	GETDNS_TASRC_ZONE,
	GETDNS_TASRC_APP,
	GETDNS_TASRC_FETCHING,
	GETDNS_TASRC_XML,
	GETDNS_TASRC_XML_UPDATE,
	GETDNS_TASRC_FAILED
} getdns_tasrc;

typedef enum getdns_tsig_algo {
	GETDNS_NO_TSIG     = 0, /* Do not use tsig */
	GETDNS_HMAC_MD5    = 1, /* 128 bits */
	GETDNS_GSS_TSIG    = 2, /* Not supported */
	GETDNS_HMAC_SHA1   = 3, /* 160 bits */
	GETDNS_HMAC_SHA224 = 4,
	GETDNS_HMAC_SHA256 = 5,
	GETDNS_HMAC_SHA384 = 6,
	GETDNS_HMAC_SHA512 = 7
} getdns_tsig_algo;


typedef struct getdns_tsig_info {
	getdns_tsig_algo  alg;
	const char       *name;
	size_t            strlen_name;
	const uint8_t    *dname;
	size_t            dname_len;
	size_t            min_size; /* in # octets */
	size_t            max_size; /* Actual size in # octets */
} getdns_tsig_info;

const getdns_tsig_info *_getdns_get_tsig_info(getdns_tsig_algo tsig_alg);

/* for doing public key pinning of TLS-capable upstreams: */
typedef struct sha256_pin {
	uint8_t pin[SHA256_DIGEST_LENGTH];
	struct sha256_pin *next;
} sha256_pin_t;

typedef struct getdns_upstream {
	/* backpointer to containing upstreams structure */
	struct getdns_upstreams *upstreams;

	socklen_t                addr_len;
	struct sockaddr_storage  addr;
	char                     addr_str[INET6_ADDRSTRLEN];

	/**
	 * How is this upstream doing over UDP?
	 *
	 * to_retry = 1, back_off = 1, in context.c:upstream_init()
	 * 
	 * When querying over UDP, first a upstream is selected which to_retry
	 * value > 0 in stub.c:upstream_select().
	 * 
	 * Every time a udp request times out, to_retry is decreased, and if
	 * it reaches 0, it is set to minus back_off in
	 * stub.c:stub_next_upstream().
	 *
	 * to_retry will become > 0 again. because each time an upstream is
	 * selected for a UDP query in stub.c:upstream_select(), all to_retry
	 * counters <= 0 are incremented.
	 *
	 * On continuous failure, the stubs are less likely to be reselected,
	 * because each time to_retry is set to minus back_off, in 
	 * stub.c:stub_next_upstream(), the back_off value is doubled.
	 *
	 * Finally, if all upstreams are failing, the upstreams with the
	 * smallest back_off value will be selected, and the back_off value
	 * decremented by one.
	 */
	int                      to_retry;  /* (initialized to 1) */
	int                      back_off;  /* (initialized to 1) */
	size_t                   udp_responses;
	size_t                   udp_timeouts;

	/* For stateful upstreams, need to share the connection and track the
	   activity on the connection */
	int                      fd;
	getdns_transport_list_t  transport;
	getdns_eventloop_event   event;
	getdns_eventloop        *loop;
	getdns_tcp_state         tcp;
	/* These are running totals or historical info */
	size_t                   conn_completed;
	size_t                   conn_shutdowns;
	size_t                   conn_setup_failed;
	time_t                   conn_retry_time;
	uint16_t                 conn_backoff_interval;
	size_t                   conn_backoffs;
	size_t                   total_responses;
	size_t                   total_timeouts;
	getdns_auth_state_t      best_tls_auth_state;
	getdns_auth_state_t      last_tls_auth_state;
	/* These are per connection. */
	getdns_conn_state_t      conn_state;
	size_t                   queries_sent;
	size_t                   responses_received;
	size_t                   responses_timeouts;
	size_t                   keepalive_shutdown;
	uint64_t                 keepalive_timeout;
	int                      server_keepalive_received;

	/* Management of outstanding requests on stateful transports */
	getdns_network_req      *write_queue;
	getdns_network_req      *write_queue_last;
	_getdns_rbtree_t         netreq_by_query_id;

	/* TCP specific connection handling*/
	unsigned                 tfo_use_sendto  : 1;
	/* TLS specific connection handling*/
	unsigned                 tls_fallback_ok : 1;
	_getdns_tls_connection*  tls_obj;
	_getdns_tls_session*     tls_session;
	getdns_tls_hs_state_t    tls_hs_state;
	getdns_auth_state_t      tls_auth_state;
	uint64_t                 expires; /* Expire time of waiting netreqs.
	                                   * This is how long a handshake may
	                                   * take.
	                                   */
	/* TLS settings */
	char                    *tls_cipher_list;
	char                    *tls_ciphersuites;
	char                    *tls_curves_list;
	getdns_tls_version_t     tls_min_version;
	getdns_tls_version_t     tls_max_version;

	/* Auth credentials */
	char                     tls_auth_name[256];
	sha256_pin_t            *tls_pubkey_pinset;

	/* When requests have been scheduled asynchronously on an upstream
	 * that is kept open, and a synchronous call is then done with the
	 * upstream before all scheduled requests have been answered, answers
	 * for the asynchronous requests may be received on the open upstream.
	 * Those cannot be processed immediately, because then asynchronous
	 * callbacks will be fired as a side-effect.
	 *
	 * finished_dnsreqs is a list of dnsreqs for which answers have been
	 * received during a synchronous request.  They will be processed
	 * when the asynchronous eventloop is run.  For this the finished_event
	 * will be scheduled to the registered asynchronous event loop with a
	 * timeout of 1, so it will fire immediately (but not while scheduling)
	 * when the asynchronous eventloop is run.
	 */
	getdns_dns_req          *finished_dnsreqs;
	getdns_eventloop_event   finished_event;
	unsigned is_sync_loop : 1;

	/* EDNS cookies */
	uint32_t secret;
	uint8_t  client_cookie[8];
	uint8_t  prev_client_cookie[8];
	uint8_t  server_cookie[32];

	unsigned has_client_cookie : 1;
	unsigned has_prev_client_cookie : 1;
	unsigned has_server_cookie : 1;
	unsigned server_cookie_len : 5;

	/* TSIG */
	uint8_t          tsig_dname[256];
	size_t           tsig_dname_len;
	size_t           tsig_size;
	uint8_t          tsig_key[256];
	getdns_tsig_algo tsig_alg;

} getdns_upstream;

typedef struct getdns_log_config {
	getdns_logfunc_type  func;
	void                *userarg;
	uint64_t             system;
	getdns_loglevel_type level;
} getdns_log_config;

typedef struct getdns_upstreams {
	struct mem_funcs mf;
	size_t referenced;
	size_t count;
	size_t current_udp;
	size_t current_stateful;
	uint16_t max_backoff_value;
	uint16_t tls_backoff_time;
	uint16_t tls_connection_retries;
	getdns_log_config log;
	getdns_upstream upstreams[];
} getdns_upstreams;

typedef enum tas_state {
	TAS_LOOKUP_ADDRESSES = 0,
	TAS_WRITE_GET_XML,
	TAS_READ_XML_HDR,
	TAS_READ_XML_DOC,
	TAS_WRITE_GET_PS7,
	TAS_READ_PS7_HDR,
	TAS_READ_PS7_DOC,
	TAS_DONE,
	TAS_RETRY,
	TAS_RETRY_GET_PS7,
	TAS_RETRY_PS7_HDR,
	TAS_RETRY_PS7_DOC,
	TAS_RETRY_DONE
} tas_state;

typedef enum _getdns_property {
	PROP_INHERIT =  0,
	PROP_UNKNOWN =  1,
	PROP_UNABLE  =  2,
	PROP_ABLE    =  3 
} _getdns_property;

typedef struct tas_connection {
	getdns_eventloop       *loop;
	getdns_network_req     *req;
	_getdns_rrset_spc       rrset_spc;
	_getdns_rrset          *rrset;
	_getdns_rrtype_iter     rr_spc;
	_getdns_rrtype_iter    *rr;
	int                    fd;
	getdns_eventloop_event event;
	tas_state              state;
	getdns_tcp_state       tcp;
	char                  *http;
	getdns_bindata         xml;
} tas_connection;

struct getdns_context {
	/* Context values */
	getdns_resolution_t  resolution_type;
	getdns_namespace_t   *namespaces;
	size_t               namespace_count;
	uint64_t             timeout;
	uint64_t             idle_timeout;
	getdns_redirects_t   follow_redirects;
	getdns_list          *dns_root_servers;

#if defined(HAVE_LIBUNBOUND) && !defined(HAVE_UB_CTX_SET_STUB)
	char                 root_servers_fn[FILENAME_MAX];
#endif
	getdns_append_name_t append_name;
	/* Suffix buffer containing a list of (length byte | dname) where 
	 * length bytes contains the length of the following dname.
	 * The last dname should be the zero byte.
	 */
	const uint8_t        *suffixes;
	/* Length of all suffixes in the suffix buffer */
	size_t               suffixes_len; 

	uint8_t              *trust_anchors;
	size_t                trust_anchors_len;
	getdns_tasrc          trust_anchors_source;

	tas_connection        a;
	tas_connection        aaaa;
	uint8_t               tas_hdr_spc[512];

	char                 *trust_anchors_url;
	char                 *trust_anchors_verify_CA;
	char                 *trust_anchors_verify_email;
	uint64_t              trust_anchors_backoff_time;
	uint64_t              trust_anchors_backoff_expiry;

	_getdns_ksks          root_ksk;

	char                 *appdata_dir;
	_getdns_property      can_write_appdata;

	char                 *tls_ca_path;
	char                 *tls_ca_file;
	char                 *tls_cipher_list;
	char                 *tls_ciphersuites;
	char                 *tls_curves_list;
	getdns_tls_version_t  tls_min_version;
	getdns_tls_version_t  tls_max_version;

	getdns_upstreams     *upstreams;
	uint16_t             limit_outstanding_queries;
	uint32_t             dnssec_allowed_skew;
	getdns_tls_authentication_t  tls_auth;  /* What user requested for TLS*/
	getdns_tls_authentication_t  tls_auth_min; /* Derived minimum auth allowed*/
	uint8_t              round_robin_upstreams;
	uint16_t             max_backoff_value;
	uint16_t             tls_backoff_time;
	uint16_t             tls_connection_retries;

	getdns_transport_list_t   *dns_transports;
	size_t                     dns_transport_count;

	uint8_t edns_extended_rcode;
	uint8_t edns_version;
	uint8_t edns_do_bit;
	int edns_maximum_udp_payload_size; /* -1 is unset */
	uint8_t edns_client_subnet_private;
	uint16_t tls_query_padding_blocksize;
	_getdns_tls_context* tls_ctx;

	getdns_update_callback  update_callback;
	getdns_update_callback2 update_callback2;
	void                   *update_userarg;

	getdns_log_config log;

	int processing;
	int destroying;

	struct mem_funcs mf;
	struct mem_funcs my_mf;

#ifdef HAVE_LIBUNBOUND
	/* The underlying contexts that do the real work */
	struct ub_ctx *unbound_ctx;
	int            unbound_ta_set;
#ifdef HAVE_UNBOUND_EVENT_API
	_getdns_ub_loop ub_loop;
#endif
#endif
	/* A tree to hold local host information*/
	_getdns_rbtree_t local_hosts;

	/* which resolution type the contexts are configured for
	 * 0 means nothing set
	 */
	getdns_resolution_t resolution_type_set;

	/*
	 * outbound requests -> transaction to getdns_dns_req
	 */
	_getdns_rbtree_t outbound_requests;

	/* network requests
	 */
	size_t netreqs_in_flight;

	_getdns_rbtree_t       pending_netreqs;
	getdns_network_req    *first_pending_netreq;
	getdns_eventloop_event pending_timeout_event;

	struct listen_set *server;

	/* Event loop extension.  */
	getdns_eventloop       *extension;

#ifdef HAVE_LIBUNBOUND
	getdns_eventloop_event  ub_event;
	/* lock to prevent nested ub_event scheduling */
	int                     ub_event_scheduling;
#endif

	/* The default extension */
	_getdns_default_eventloop default_eventloop;
	_getdns_default_eventloop sync_eventloop;

	/* request extension defaults */
	getdns_dict *header;
	getdns_dict *add_opt_parameters;
	unsigned add_warning_for_bad_dns             : 1;
	unsigned dnssec                              : 1;
	unsigned dnssec_return_all_statuses          : 1;
	unsigned dnssec_return_full_validation_chain : 1;
	unsigned dnssec_return_only_secure           : 1;
	unsigned dnssec_return_status                : 1;
	unsigned dnssec_return_validation_chain      : 1;
#ifdef DNSSEC_ROADBLOCK_AVOIDANCE
	unsigned dnssec_roadblock_avoidance          : 1;
#endif
	unsigned edns_cookies                        : 1;
	unsigned return_api_information              : 1; /* Not used */
	unsigned return_both_v4_and_v6               : 1;
	unsigned return_call_reporting               : 1;
	uint16_t specify_class;

	/*
	 * Context for doing system queries.
	 * For example to resolve data.iana.org or to resolver the addresses
	 * of upstreams without specified addresses.
	 */
	getdns_context *sys_ctxt;

	/* List of dnsreqs that want to be notified when we have fetched a
	 * trust anchor from data.iana.org.
	 */
	getdns_dns_req *ta_notify;

	/*
	 * state data used to detect changes to the system config files
	 */
	struct filechg fchg_resolvconf;
	struct filechg fchg_hosts;

	uint8_t trust_anchors_spc[1024];

#ifdef USE_WINSOCK
	/* We need to run WSAStartup() to be able to use getaddrinfo() */
	WSADATA wsaData;
#endif

	/* MDNS */
#ifdef HAVE_MDNS_SUPPORT
	/* 
	 * If supporting MDNS, context may be instantiated either in basic mode
	 * or in full mode. If working in extended mode, two multicast sockets are
	 * left open, for IPv4 and IPv6. Data can be received on either socket.
	 * The context also keeps a list of open queries, characterized by a 
	 * name and an RR type, and a list of received answers, characterized
	 * by name, RR type and data value.
	 */
	int mdns_extended_support; /* 0 = no support, 1 = supported, 2 = initialization needed */
	int mdns_connection_nb; /* typically 0 or 2 for IPv4 and IPv6 */
	struct mdns_network_connection * mdns_connection;
	struct lruhash * mdns_cache;

#endif /* HAVE_MDNS_SUPPORT */
}; /* getdns_context */

static inline int _getdns_check_log(const getdns_log_config *log,
    uint64_t system, getdns_loglevel_type level)
{ assert(log)
; return log->func && (log->system & system) && level <= log->level; }

static inline void _getdns_log(const getdns_log_config *log,
    uint64_t system, getdns_loglevel_type level, const char *fmt, ...)
{
	va_list args;

	if (!_getdns_check_log(log, system, level))
		return;

	va_start(args, fmt);
	log->func(log->userarg, system, level, fmt, args);
	va_end(args);
}

static inline void _getdns_upstream_log(const getdns_upstream *up,
    uint64_t system, getdns_loglevel_type level, const char *fmt, ...)
{
	va_list args;

	if (!up || !up->upstreams
	||  !_getdns_check_log(&up->upstreams->log, system, level))
		return;

	va_start(args, fmt);
	up->upstreams->log.func(
	    up->upstreams->log.userarg, system, level, fmt, args);
	va_end(args);
}


/** internal functions **/
/**
 * Sets up the unbound contexts with stub or recursive behavior
 * if needed.
 * @param context previously initialized getdns_context
 * @return GETDNS_RETURN_GOOD on success
 */
getdns_return_t _getdns_context_prepare_for_resolution(getdns_context *context);

/* Register a getdns_dns_req with context.
 * - Without pluggable unbound event API,
 *   ub_fd() is scheduled when this was the first request.
 */
void _getdns_context_track_outbound_request(getdns_dns_req *dnsreq);

/* Deregister getdns_dns_req from the context.
 * - Without pluggable unbound event API,
 *   ub_fd() is scheduled when this was the first request.
 * - Potential timeout events will be cleared.
 * - All associated getdns_dns_reqs (to get the validation chain)
 *   will be canceled.
 */
void _getdns_context_clear_outbound_request(getdns_dns_req *dnsreq);

/* Cancels and frees a getdns_dns_req (without calling user callbacks)
 * - Deregisters getdns_dns_req with _getdns_context_clear_outbound_request()
 * - Cancels associated getdns_network_reqs
 *   (by calling ub_cancel() or _getdns_cancel_stub_request())
 * - Frees the getdns_dns_req
 */
void _getdns_context_cancel_request(getdns_dns_req *dnsreq);

/* Calls user callback (with GETDNS_CALLBACK_TIMEOUT + response dict), then
 * cancels and frees the getdns_dns_req with _getdns_context_cancel_request()
 */
void _getdns_context_request_timed_out(getdns_dns_req *dnsreq);

struct getdns_bindata *_getdns_bindata_copy(
    struct mem_funcs *mfs, size_t size, const uint8_t *data);

void _getdns_bindata_destroy(
    struct mem_funcs *mfs,
    struct getdns_bindata *bindata);

/* perform name resolution in /etc/hosts */
getdns_return_t _getdns_context_local_namespace_resolve(
    getdns_dns_req* req, struct getdns_dict **response);

void _getdns_context_ub_read_cb(void *userarg);

void _getdns_upstreams_dereference(getdns_upstreams *upstreams);

void _getdns_upstream_shutdown(getdns_upstream *upstream);

FILE *_getdns_context_get_priv_fp(
    const getdns_context *context, const char *fn);
uint8_t *_getdns_context_get_priv_file(const getdns_context *context,
    const char *fn, uint8_t *buf, size_t buf_len, size_t *file_sz);

int _getdns_context_write_priv_file(getdns_context *context,
    const char *fn, getdns_bindata *content);

int _getdns_context_can_write_appdata(getdns_context *context);

getdns_context *_getdns_context_get_sys_ctxt(
    getdns_context *context, getdns_eventloop *loop);

#endif /* _GETDNS_CONTEXT_H_ */
