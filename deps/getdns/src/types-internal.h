/**
 *
 * /brief type declarations private to the getdns library
 *
 * These type declarations are not meant to be used by applications calling
 * the public library functions.
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

#ifndef TYPES_INTERNAL_H_
#define TYPES_INTERNAL_H_

#include "getdns/getdns.h"
#include "getdns/getdns_extra.h"
#include "util/rbtree.h"


/**
 * this structure represents a single item in a list or dict
 */
typedef union getdns_union {
	void            *ptr;
	getdns_dict     *dict;
	getdns_list     *list;
	getdns_bindata  *bindata;
	uint32_t         n;
} getdns_union;

typedef struct getdns_item {
	getdns_data_type dtype;
	getdns_union     data;
} getdns_item;

typedef enum getdns_auth_state {
	GETDNS_AUTH_NONE,      /* Not tried (Oppotunistic)*/
	GETDNS_AUTH_FAILED,    /* Tried but failed or not possible*/
	GETDNS_AUTH_OK,        /* Tried and worked (Strict) */
} getdns_auth_state_t;

#define GETDNS_STR_AUTH_NONE "None"
#define GETDNS_STR_AUTH_FAILED "Failed"
#define GETDNS_STR_AUTH_OK "Success"

struct getdns_context;
struct getdns_upstreams;
struct getdns_upstream;

/* String constants for internal use */
#define GETDNS_STR_IPV4 "IPv4"
#define GETDNS_STR_IPV6 "IPv6"
#define GETDNS_STR_ADDRESS_TYPE "address_type"
#define GETDNS_STR_ADDRESS_DATA "address_data"
#define GETDNS_STR_PORT "port"
#define GETDNS_STR_EXTENSION_RETURN_BOTH_V4_AND_V6 "return_both_v4_and_v6"

#define GETDNS_STR_KEY_STATUS "status"
#define GETDNS_STR_KEY_REPLIES_TREE "replies_tree"
#define GETDNS_STR_KEY_REPLIES_FULL "replies_full"
#define GETDNS_STR_KEY_JUST_ADDRS "just_address_answers"
#define GETDNS_STR_KEY_CANONICAL_NM "canonical_name"
#define GETDNS_STR_KEY_ANSWER_TYPE "answer_type"
#define GETDNS_STR_KEY_INTERM_ALIASES "intermediate_aliases"
#define GETDNS_STR_KEY_NAME "name"
#define GETDNS_STR_KEY_HEADER "header"
#define GETDNS_STR_KEY_QUESTION "question"
#define GETDNS_STR_KEY_ANSWER "answer"
#define GETDNS_STR_KEY_TYPE "type"
#define GETDNS_STR_KEY_CLASS "class"
#define GETDNS_STR_KEY_TTL "ttl"
#define GETDNS_STR_KEY_RDATA "rdata"
#define GETDNS_STR_KEY_V4_ADDR "ipv4_address"
#define GETDNS_STR_KEY_V6_ADDR "ipv6_address"
#define GETDNS_STR_KEY_RDATA_RAW "rdata_raw"
#define GETDNS_STR_KEY_AUTHORITY "authority"
#define GETDNS_STR_KEY_ADDITIONAL "additional"
#define GETDNS_STR_KEY_QTYPE "qtype"
#define GETDNS_STR_KEY_QCLASS "qclass"
#define GETDNS_STR_KEY_QNAME "qname"
#define GETDNS_STR_KEY_QR "qr"
/* header flags */
#define GETDNS_STR_KEY_ID "id"
#define GETDNS_STR_KEY_OPCODE "opcode"
#define GETDNS_STR_KEY_RCODE "rcode"
#define GETDNS_STR_KEY_AA "aa"
#define GETDNS_STR_KEY_TC "tc"
#define GETDNS_STR_KEY_RD "rd"
#define GETDNS_STR_KEY_RA "ra"
#define GETDNS_STR_KEY_AD "ad"
#define GETDNS_STR_KEY_CD "cd"
#define GETDNS_STR_KEY_Z "z"
#define GETDNS_STR_KEY_QDCOUNT "qdcount"
#define GETDNS_STR_KEY_ANCOUNT "ancount"
#define GETDNS_STR_KEY_NSCOUNT "nscount"
#define GETDNS_STR_KEY_ARCOUNT "arcount"

#define TIMEOUT_FOREVER ((uint64_t)0xFFFFFFFFFFFFFFFF)
#define ASSERT_UNREACHABLE 0

#define GETDNS_TRANSPORTS_MAX 3
#define GETDNS_UPSTREAM_TRANSPORTS 2
#define GETDNS_TRANSPORT_FAIL_MULT 5


/* declarations */
struct getdns_dns_req;
struct getdns_network_req;

typedef void (*internal_cb_t)(struct getdns_dns_req *dns_req);

#define MF_PLAIN ((void *)&plain_mem_funcs_user_arg)
extern void *plain_mem_funcs_user_arg;

typedef union {
        struct {
            void *(*malloc)(size_t);
            void *(*realloc)(void *, size_t);
            void (*free)(void *);
        } pln;
        struct {
            void *(*malloc)(void *userarg, size_t);
            void *(*realloc)(void *userarg, void *, size_t);
            void (*free)(void *userarg, void *);
        } ext;
    } mf_union;

struct mem_funcs {
    void *mf_arg;
    mf_union mf;
};

struct mem_funcs *
priv_getdns_context_mf(getdns_context *context);

typedef enum network_req_state_enum
{
	NET_REQ_NOT_SENT  =  0,
	NET_REQ_IN_FLIGHT =  1,
	NET_REQ_FINISHED  =  2, /* Finish type in bits 2 and 3 */
	NET_REQ_CANCELED  =  6, /* 2 + (1 << 2) */
	NET_REQ_TIMED_OUT = 10, /* 2 + (2 << 2) */
	NET_REQ_ERRORED   = 14  /* 2 + (3 << 2) */
} network_req_state;


/* State for async tcp stub resolving */
typedef struct getdns_tcp_state {

	const uint8_t *write_buf;
	size_t   write_buf_len;
	size_t   written;

	uint8_t *read_buf;
	size_t   read_buf_len;
	uint8_t *read_pos;
	size_t   to_read;

} getdns_tcp_state;

/**
 * Request data
 **/
typedef struct getdns_network_req
{
	/* For storage in upstream->netreq_by_query_id */
	_getdns_rbnode_t  node;
	/* The netreq_by_query_id tree in which this netreq was registered */
	_getdns_rbtree_t *query_id_registered;
#ifdef HAVE_MDNS_SUPPORT
	/*
	 * for storage of continuous query context in hash table of cached results. 
	 */
	struct getdns_network_req * mdns_netreq_next;
#endif /* HAVE_MDNS_SUPPORT */
	/* the async_id from unbound */
	int unbound_id;
	/* state var */
	network_req_state state;
	/* owner request (contains name) */
	struct getdns_dns_req *owner;

	/* request type */
	uint16_t request_type;

	/* dnssec status */
	int dnssec_status;

	/* tsig status:
	 * GETDNS_DNSSEC_INDETERMINATE means "No TSIG processing"
	 * GETDNS_DNSSEC_INSECURE      means "TSIG sent, validate reply"
	 * GETDNS_DNSSEC_SECURE        means "Validated"
	 * GETDNS_DNSSEC_BOGUS         means "Validation failed"
	 */
	int tsig_status;

	getdns_redirects_t      follow_redirects;

	/* For stub resolving */
	struct getdns_upstream *first_upstream;
	struct getdns_upstream *upstream;
	int                     fd;
	getdns_transport_list_t transports[GETDNS_TRANSPORTS_MAX];
	size_t                  transport_count;
	size_t                  transport_current;
	getdns_tls_authentication_t  tls_auth_min;
	getdns_eventloop_event  event;

	int                     edns_maximum_udp_payload_size;
	uint16_t                max_udp_payload_size;

	/* Network requests scheduled to write after me */
	struct getdns_network_req *write_queue_tail;

	/* Some fields to record info for return_call_reporting */
	uint64_t                debug_start_time;
	uint64_t                debug_end_time;
	getdns_auth_state_t     debug_tls_auth_status;
	getdns_bindata          debug_tls_peer_cert;
	const char             *debug_tls_version;

	/* Some booleans */
	unsigned                debug_udp      : 1;
	unsigned                keepalive_sent : 1;
	unsigned                badcookie_retry: 1;
	unsigned                cookie_sent    : 1;
	uint8_t                 client_cookie[8];

	/* When more space is needed for the wire_data response than is
	 * available in wire_data[], it will be allocated separately.
	 * response will then not point to wire_data anymore.
	 */
	uint8_t *query;
	uint8_t *opt; /* offset of OPT RR in query */

	/* each network_req has a set of base options that are
	 * specific to the query, which are static and included when
	 * the network_req is created.  When the query is sent out to
	 * a given upstream, some additional options are added that
	 * are specific to the upstream.  There can be at most
	 * GETDNS_MAXIMUM_UPSTREAM_OPTION_SPACE bytes of
	 * upstream-specific options.

	 * use _getdns_network_req_clear_upstream_options() and
	 * _getdns_network_req_add_upstream_option() to fiddle with the
	 */
	size_t   base_query_option_sz;
	size_t   response_len;
	uint8_t *response;
	const uint8_t *response_opt; /* offset of OPT RR in response */
	size_t   wire_data_sz;
	uint8_t  wire_data[];
	
} getdns_network_req;

static inline int _getdns_netreq_finished(getdns_network_req *req)
{ return !req || (req->state & NET_REQ_FINISHED); }

struct chain_head;
/**
 * dns request - manages a number of network requests and
 * the initial data passed to getdns_general
 */
typedef struct getdns_dns_req {
	/* For storage in context->outbound_requests */
	_getdns_rbnode_t node;

	/* name */
	uint8_t name[256];
	size_t  name_len;

	uint16_t request_class;

	/* context that owns the request */
	struct getdns_context *context;

	getdns_append_name_t append_name;
	const uint8_t *suffix;
	size_t  suffix_len;
	unsigned suffix_appended			: 1;

	/* request extensions */
	unsigned dnssec					: 1;
	unsigned dnssec_return_status			: 1;
	unsigned dnssec_return_only_secure		: 1;
	unsigned dnssec_return_all_statuses		: 1;
	unsigned dnssec_return_validation_chain		: 1;
	unsigned dnssec_return_full_validation_chain	: 1;
	unsigned dnssec_extension_set                   : 1;
#ifdef DNSSEC_ROADBLOCK_AVOIDANCE
	unsigned dnssec_roadblock_avoidance		: 1;
	unsigned avoid_dnssec_roadblocks		: 1;
#endif
	unsigned edns_cookies				: 1;
	unsigned edns_client_subnet_private		: 1;
	unsigned return_call_reporting			: 1;
	unsigned add_warning_for_bad_dns		: 1;

	/* Internally used by return_validation_chain */
	unsigned dnssec_ok_checking_disabled		: 1;
	unsigned is_sync_request			: 1;
	unsigned is_dns_request				: 1;
	unsigned request_timed_out			: 1;

	/* The validating and freed variables are used to make sure a single
	 * code path is followed while processing a DNS request, even when
	 * callbacks are already fired whilst the registering/scheduling call
	 * (i.e. ub_resolve_event) has not returned yet.
	 *
	 * validating is touched by _getdns_get_validation_chain only and
	 * freed      is touched by _getdns_submit_netreq only
	 */
	unsigned validating				: 1;
	unsigned waiting_for_ta                         : 1;
	int *freed;

	/* Validation chain to be canceled when this request is canceled */
	struct chain_head *chain;

	uint16_t tls_query_padding_blocksize;

	/* internally scheduled request */
	internal_cb_t internal_cb;

	/* event loop */
	getdns_eventloop *loop;

	/* callback data */
	getdns_callback_t user_callback;
	void *user_pointer;

	/* the transaction id */
	getdns_transaction_t trans_id;

	/* Absolute time (in milliseconds since epoch),
	 * after which this dns request is expired; i.e. timed out
	 */
	uint64_t expires;

	/* for scheduling timeouts when using libunbound */
	getdns_eventloop_event timeout;

	/* mem funcs */
	struct mem_funcs my_mf;

	/* Stuff for stub resolving */
	struct getdns_upstreams *upstreams;

	/* Linked list pointer for dns requests, for which answers are received
	 * from open connections as aside-effect of doing a synchronous call.
	 * See also the type definition of getdns_upstream in context.h for a
	 * more elaborate description.
	 */
	struct getdns_dns_req *finished_next;

	/* Linked list pointer for dns requests, which need to validate DNSSEC
	 * and are waiting for the root trust-anchors fetch.
	 */
	struct getdns_dns_req *ta_notify;

	/* network requests for this dns request.
	 * The array is terminated with NULL.
	 *
	 * Memory for these netreqs has been allocated by the same malloc
	 * operation that reserved space for this getdns_dns_req.
	 * They will thus be freed as part of the destruction of this struct,
	 * and do not need to be freed separately.
	 */
	getdns_network_req *netreqs[];

} getdns_dns_req;

#define GETDNS_XMALLOC(obj, type, count)	\
    ((obj).mf_arg == MF_PLAIN \
    ? ((type *)(*(obj).mf.pln.malloc)(              (count)*sizeof(type))) \
    : ((type *)(*(obj).mf.ext.malloc)((obj).mf_arg, (count)*sizeof(type))) \
    )

#define GETDNS_XREALLOC(obj, ptr, type, count)	\
    ((obj).mf_arg == MF_PLAIN \
    ? ((type *)(*(obj).mf.pln.realloc)( (ptr), (count)*sizeof(type))) \
    : ((type *)(*(obj).mf.ext.realloc)( (obj).mf_arg                  \
                                      , (ptr), (count)*sizeof(type))) \
    )

#define GETDNS_FREE(obj, ptr)	\
    ((obj).mf_arg == MF_PLAIN \
    ? ((*(obj).mf.pln.free)(              (ptr))) \
    : ((*(obj).mf.ext.free)((obj).mf_arg, (ptr))) \
    )

#define GETDNS_NULL_FREE(obj, ptr)	\
	do { \
		if (!(ptr)) \
			break; \
		if ((obj).mf_arg == MF_PLAIN) \
			(*(obj).mf.pln.free)(              (ptr)); \
		else \
			(*(obj).mf.ext.free)((obj).mf_arg, (ptr)); \
		(ptr) = NULL; \
	} while (0);

#define GETDNS_MALLOC(obj, type)	GETDNS_XMALLOC(obj, type, 1)
#define GETDNS_REALLOC(obj, ptr, type)	GETDNS_XREALLOC(obj, ptr, type, 1);


/* utility methods */

extern getdns_dict *dnssec_ok_checking_disabled;
extern getdns_dict *dnssec_ok_checking_disabled_roadblock_avoidance;
extern getdns_dict *dnssec_ok_checking_disabled_avoid_roadblocks;
extern getdns_dict *no_dnssec_checking_disabled_opportunistic;

/* dns request utils */
getdns_dns_req *_getdns_dns_req_new(getdns_context *context, getdns_eventloop *loop,
    const char *name, uint16_t request_type, const getdns_dict *extensions,
    uint64_t *now_ms);

void _getdns_dns_req_free(getdns_dns_req * req);

/* network request utils */
getdns_return_t _getdns_network_req_add_upstream_option(getdns_network_req * req,
					     uint16_t code, uint16_t sz, const void* data);
void _getdns_network_req_clear_upstream_options(getdns_network_req * req);

/* Adds TSIG signature (if needed) and returns query length */
size_t _getdns_network_req_add_tsig(getdns_network_req *req);

void _getdns_network_validate_tsig(getdns_network_req *req);

void _getdns_netreq_reinit(getdns_network_req *netreq);

const char * _getdns_auth_str(getdns_auth_state_t auth);

#endif
/* types-internal.h */
