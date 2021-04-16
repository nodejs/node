/*
 * Functions for MDNS resolving.
 */

 /*
  * Copyright (c) 2016 Christian Huitema <huitema@huitema.net>
  *
  * Permission to use, copy, modify, and distribute this software for any
  * purpose with or without fee is hereby granted, provided that the above
  * copyright notice and this permission notice appear in all copies.
  *
  * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
  */


#include "config.h"
#include "debug.h"
#include "context.h"
#include "general.h"
#include "gldns/pkthdr.h"
#include "gldns/rrdef.h"
#include "util-internal.h"
#include "platform.h"
#include "mdns.h"

#ifdef HAVE_MDNS_SUPPORT

/* Define IPV6_ADD_MEMBERSHIP for FreeBSD and Mac OS X */
#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif

uint64_t _getdns_get_time_as_uintt64();

#include "util/fptr_wlist.h"
#include "util/lookup3.h"

/*
 * Constants defined in RFC 6762
 */

#define MDNS_MCAST_IPV4_LONG 0xE00000FB /* 224.0.0.251 */
#define MDNS_MCAST_PORT 5353

static uint8_t mdns_mcast_ipv6[] = {
	0xFF, 0x02, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0xFB 
};

static uint8_t mdns_suffix_dot_local[] = { 5, 'l', 'o', 'c', 'a', 'l', 0 };
static uint8_t mdns_suffix_254_169_in_addr_arpa[] = {
	3, '2', '5', '4',
	3, '1', '6', '9',
	7, 'i', 'n', '-', 'a', 'd', 'd', 'r',
	4, 'a', 'r', 'p', 'a', 0 };
static uint8_t mdns_suffix_8_e_f_ip6_arpa[] = {
	1, '8', 1, 'e', 1, 'f',
	3, 'i', 'p', '6',
	4, 'a', 'r', 'p', 'a', 0 };
static uint8_t mdns_suffix_9_e_f_ip6_arpa[] = {
	1, '9', 1, 'e', 1, 'f',
	3, 'i', 'p', '6',
	4, 'a', 'r', 'p', 'a', 0 };
static uint8_t mdns_suffix_a_e_f_ip6_arpa[] = {
	1, 'a', 1, 'e', 1, 'f',
	3, 'i', 'p', '6',
	4, 'a', 'r', 'p', 'a', 0 };
static uint8_t mdns_suffix_b_e_f_ip6_arpa[] = {
	1, 'b', 1, 'e', 1, 'f',
	3, 'i', 'p', '6',
	4, 'a', 'r', 'p', 'a', 0 };

#define MDNS_PACKET_INDEX_QCODE 2
#define MDNS_PACKET_INDEX_QUERY 4
#define MDNS_PACKET_INDEX_ANSWER 6

/*
 * MDNS cache management using LRU Hash.
 *
 * Each record contains a DNS query + response, formatted as received from
 * the network. By convention, there will be exactly one query, and
 * a variable number of answers. Auth and AD sections will not be cached.
 * For maintenance purpose, each recontains a last accessed time stamp.
 *
 * This structure works very well for classic DNS caches, but for MDNS we 
 * have to consider processing a new record for an existing cache entry. If
 * the record is present, its TTL should be updated. If the record is not
 * present, it should be added to the existing data.
 *
 * After an update, the TTL of all the records should be updated. Some
 * records will end up with a TTL value of zero. These records should be 
 * deleted, using a "compression" procedure.
 */



/*
 * For the data part, we want to allocate in rounded increments, so as to reduce the
 * number of calls to XMALLOC
 */

static uint32_t 
mdns_util_suggest_size(uint32_t required_size)
{
	return (required_size <= 512) ? ((required_size <= 256) ? 256 : 512) :
		((required_size + 1023) & 0xFFFFFC00);
}

/*
 * Cache management utilities
 */
static int
mdns_util_skip_name(uint8_t *p)
{
	int x = 0;
	int l;

	for (;;) {
		l = p[x];
		if (l == 0)
		{
			x++;
			break;
		}
		else if (l >= 0xC0)
		{
			x += 2;
			break;
		}
		else
		{
			x += l + 1;
		}
	}
	return x;
}

static size_t 
mdns_util_copy_name(uint8_t * message, size_t message_length, size_t current_index,
	uint8_t *name, int name_len_max, int name_index, int * name_len)
{
	uint8_t l;
	size_t recursive_index;

	*name_len = 0;
	while (current_index < message_length && name_index < name_len_max) {
		l = message[current_index++];
		if (l == 0)
		{
			name[name_index++] = 0;
			*name_len = name_index;
			break;
		}
		else if (l >= 0xC0)
		{
			if (current_index < message_length)
			{
				recursive_index = ((l & 63) << 8) | message[current_index++];

				(void) mdns_util_copy_name(message, message_length, 
					recursive_index, name, name_len_max, name_index, name_len);

				if (*name_len == 0)
				{
					current_index = message_length;
				}
			}
			break;
		}
		else if (current_index + l < message_length &&
			name_index + l + 1 < name_len_max)
		{
			name[name_index++] = l;

			memcpy(name + name_index, message + current_index, l);
			name_index += l;
			current_index += l;
		}
		else
		{
			current_index = message_length;
			break;
		}
	}

	return current_index;
}

static int
mdns_util_skip_query(uint8_t *p)
{
	return mdns_util_skip_name(p) + 4;
}

/*
 * Single copy procedure for many record types
 * copy N octets, then the canonical value of the name.
 */
static int
mdns_util_canonical_flags_and_name(uint8_t *message, int message_length,
	int record_length,
	int current_index,
	int nb_octets_to_copy,
	uint8_t *buffer, int buffer_max,
	uint8_t **actual_record, int *actual_length)
{
	int ret = 0;
	int buffer_index = 0;
	int name_len = 0;

	if (buffer_max <= nb_octets_to_copy || record_length <= nb_octets_to_copy)
	{
		/* incorrect buffer */
		ret = GETDNS_RETURN_GENERIC_ERROR;
	}
	else
	{
		int i;
		for (i = 0; i < nb_octets_to_copy; i++)
		{
			buffer[buffer_index++] = message[current_index++];
		}

		current_index = mdns_util_copy_name(message, message_length, current_index, buffer, buffer_max, buffer_index, &name_len);
		if (current_index == record_length)
		{
			buffer_index += name_len;
			*actual_record = buffer;
			*actual_length = buffer_index;
		}
		else
		{
			/* something went wrong. */
			ret = GETDNS_RETURN_BAD_DOMAIN_NAME;
		}
	}

	return ret;
}
/*
 * Set record value to canonical form
 */
static int
mdns_util_canonical_record(uint8_t *message, int message_length,
	int record_type, int record_class, int record_length,
	int record_index,  
	uint8_t *buffer, int buffer_max,
	uint8_t **actual_record, int *actual_length)
{
	int ret = 0;
	int current_index = record_index;
	/* Check whether the record needs canonization */
	*actual_record = message + record_index;
	*actual_length = record_length;

	if (record_class != GLDNS_RR_CLASS_IN)
	{
		/*
		 * No attempt at canonization outside the IN class.
		 */
		return 0;
	}
	
	switch (record_type)
	{
		
	case GLDNS_RR_TYPE_NS:
	case GLDNS_RR_TYPE_CNAME:
	case GLDNS_RR_TYPE_PTR:
	case GLDNS_RR_TYPE_MD:
	case GLDNS_RR_TYPE_MB:
	case GLDNS_RR_TYPE_MF:
	case GLDNS_RR_TYPE_MG:
	case GLDNS_RR_TYPE_MR:
	case GLDNS_RR_TYPE_NSAP_PTR:
		/* 
		 * copy the name in canonical form 
		 */
		ret = mdns_util_canonical_flags_and_name(message, message_length,
			record_length, current_index, 0,
			buffer, buffer_max, actual_record, actual_length);
		break;

	case GLDNS_RR_TYPE_A:
	case GLDNS_RR_TYPE_AAAA:
	case GLDNS_RR_TYPE_TXT:
	case GLDNS_RR_TYPE_HINFO:
	case GLDNS_RR_TYPE_MINFO:
	case GLDNS_RR_TYPE_NULL:
	case GLDNS_RR_TYPE_WKS:
	case GLDNS_RR_TYPE_X25:
	case GLDNS_RR_TYPE_ISDN:
	case GLDNS_RR_TYPE_NSAP:
	case GLDNS_RR_TYPE_SIG:
	case GLDNS_RR_TYPE_KEY:
	case GLDNS_RR_TYPE_GPOS:
	case GLDNS_RR_TYPE_LOC:
	case GLDNS_RR_TYPE_EID:
	case GLDNS_RR_TYPE_NIMLOC:
		/* leave the content as is, no domain name in content */
		break;

	case GLDNS_RR_TYPE_SRV:
		/*
		 * Copy 6 octets for weight(2), priority(2) and port(2),
		 * then copy the name.
		 */
		ret = mdns_util_canonical_flags_and_name(message, message_length,
			record_length, current_index, 6,
			buffer, buffer_max, actual_record, actual_length);
		break;

	case GLDNS_RR_TYPE_MX:
	case GLDNS_RR_TYPE_RT:
	case GLDNS_RR_TYPE_AFSDB:
		/* 
		 * copy two bytes preference or subtype, then 
		 * copy the name in canonical form 
		 */
		ret = mdns_util_canonical_flags_and_name(message, message_length,
			record_length, current_index, 2,
			buffer, buffer_max, actual_record, actual_length);
		break;

	case GLDNS_RR_TYPE_NAPTR:
	case GLDNS_RR_TYPE_SOA:
	case GLDNS_RR_TYPE_RP:
	case GLDNS_RR_TYPE_NXT:
	case GLDNS_RR_TYPE_PX:
	case GLDNS_RR_TYPE_ATMA:
		/*
		 * Group of record types that are complex, and also
		 * unexpected in MDNS/DNS-SD operation. Copying the 
		 * record directly will work as long as the sender
		 * does not attempt name compression.
		 * TODO: log some kind of error.
		 */
		break;
#if 0

			/**  RFC2915 */
			GLDNS_RR_TYPE_NAPTR = 35,
			/**  RFC2230 */
			GLDNS_RR_TYPE_KX = 36,
			/**  RFC2538 */
			GLDNS_RR_TYPE_CERT = 37,
			/**  RFC2874 */
			GLDNS_RR_TYPE_A6 = 38,
			/**  RFC2672 */
			GLDNS_RR_TYPE_DNAME = 39,
			/**  dnsind-kitchen-sink-02.txt */
			GLDNS_RR_TYPE_SINK = 40,
			/**  Pseudo OPT record... */
			GLDNS_RR_TYPE_OPT = 41,
			/**  RFC3123 */
			GLDNS_RR_TYPE_APL = 42,
			/**  RFC4034, RFC3658 */
			GLDNS_RR_TYPE_DS = 43,
			/**  SSH Key Fingerprint */
			GLDNS_RR_TYPE_SSHFP = 44, /* RFC 4255 */
									  /**  IPsec Key */
			GLDNS_RR_TYPE_IPSECKEY = 45, /* RFC 4025 */
										 /**  DNSSEC */
			GLDNS_RR_TYPE_RRSIG = 46, /* RFC 4034 */
			GLDNS_RR_TYPE_NSEC = 47, /* RFC 4034 */
			GLDNS_RR_TYPE_DNSKEY = 48, /* RFC 4034 */

			GLDNS_RR_TYPE_DHCID = 49, /* RFC 4701 */
									  /* NSEC3 */
			GLDNS_RR_TYPE_NSEC3 = 50, /* RFC 5155 */
			GLDNS_RR_TYPE_NSEC3PARAM = 51, /* RFC 5155 */
			GLDNS_RR_TYPE_NSEC3PARAMS = 51,
			GLDNS_RR_TYPE_TLSA = 52, /* RFC 6698 */
			GLDNS_RR_TYPE_SMIMEA = 53, /* draft-ietf-dane-smime, TLSA-like but may
									   be extended */

			GLDNS_RR_TYPE_HIP = 55, /* RFC 5205 */

									/** draft-reid-dnsext-zs */
			GLDNS_RR_TYPE_NINFO = 56,
			/** draft-reid-dnsext-rkey */
			GLDNS_RR_TYPE_RKEY = 57,
			/** draft-ietf-dnsop-trust-history */
			GLDNS_RR_TYPE_TALINK = 58,
			GLDNS_RR_TYPE_CDS = 59, /** RFC 7344 */
			GLDNS_RR_TYPE_CDNSKEY = 60, /** RFC 7344 */
			GLDNS_RR_TYPE_OPENPGPKEY = 61, /* RFC 7929 */
			GLDNS_RR_TYPE_CSYNC = 62, /* RFC 7477 */

			GLDNS_RR_TYPE_SPF = 99, /* RFC 4408 */

			GLDNS_RR_TYPE_UINFO = 100,
			GLDNS_RR_TYPE_UID = 101,
			GLDNS_RR_TYPE_GID = 102,
			GLDNS_RR_TYPE_UNSPEC = 103,

			GLDNS_RR_TYPE_NID = 104, /* RFC 6742 */
			GLDNS_RR_TYPE_L32 = 105, /* RFC 6742 */
			GLDNS_RR_TYPE_L64 = 106, /* RFC 6742 */
			GLDNS_RR_TYPE_LP = 107, /* RFC 6742 */

									/** draft-jabley-dnsext-eui48-eui64-rrtypes */
			GLDNS_RR_TYPE_EUI48 = 108,
			GLDNS_RR_TYPE_EUI64 = 109,

			GLDNS_RR_TYPE_TKEY = 249, /* RFC 2930 */
			GLDNS_RR_TYPE_TSIG = 250,
			GLDNS_RR_TYPE_IXFR = 251,
			GLDNS_RR_TYPE_AXFR = 252,
			/**  A request for mailbox-related records (MB, MG or MR) */
			GLDNS_RR_TYPE_MAILB = 253,
			/**  A request for mail agent RRs (Obsolete - see MX) */
			GLDNS_RR_TYPE_MAILA = 254,
			/**  any type (wildcard) */
			GLDNS_RR_TYPE_ANY = 255,
			GLDNS_RR_TYPE_URI = 256, /* RFC 7553 */
			GLDNS_RR_TYPE_CAA = 257, /* RFC 6844 */
#endif
	default:
		/* 
		 * Unknown record type. Not expected in MDNS/DNS-SD. Just keep the current value. 
		 * TODO: log some kind of error.
		 */
		break;
	}

	return ret;
}
/*
 * Comparison and other functions required for cache management
 */

 /**
 * Calculates the size of an entry.
 *
 * size = mdns_cache_size (key, data).
 */
static size_t mdns_cache_entry_size(void* vkey, void* vdata)
{
	size_t sz = 0;

	if (vkey != NULL)
	{
		sz += sizeof(getdns_mdns_cached_key_header) + ((getdns_mdns_cached_key_header*)vkey)->name_len;
	}

	if (vdata != NULL)
	{
		sz += ((getdns_mdns_cached_record_header*)vdata)->allocated_length;
	}

	return  sz;
}

/** type of function that compares two keys. return 0 if equal. */
static int mdns_cache_key_comp(void* vkey1, void* vkey2)
{
	getdns_mdns_cached_key_header *header1 = (getdns_mdns_cached_key_header*)vkey1;
	getdns_mdns_cached_key_header *header2 = (getdns_mdns_cached_key_header*)vkey2;

	return (header1->record_type == header2->record_type &&
		header1->record_class == header2->record_class &&
		header1->name_len == header2->name_len)
		? memcmp(((uint8_t*)vkey1) + sizeof(getdns_mdns_cached_key_header),
			((uint8_t*)vkey2) + sizeof(getdns_mdns_cached_key_header),
			header1->name_len)
		: -1;
}

/** old keys are deleted.
* markdel() is used first.
* This function is called: func(key, userarg)
* the userarg is set to the context in which the LRU hash table was created.
* TODO: is there a need to free the lock in the embedded hash entry structure?
*/
static void msdn_cache_delkey(void* vkey, void* vcontext)
{
	GETDNS_FREE(((struct getdns_context *) vcontext)->mf, vkey);
}

/** old data is deleted. This function is called: func(data, userarg). 
 * Since we use the hash table for both data and requests, need to
 * terminate whatever request was ongoing. TODO: we should have some smarts
 * in cache management and never drop cached entries with active requests.
 */
static void msdn_cache_deldata(void* vdata, void* vcontext)
{
	getdns_mdns_cached_record_header* header = ((getdns_mdns_cached_record_header*)vdata);

	while (header->netreq_first)
	{
		/* Need to unchain the request from that entry */
		getdns_network_req* netreq = header->netreq_first;
		header->netreq_first = netreq->mdns_netreq_next;
		netreq->mdns_netreq_next = NULL;

		/* TODO: treating as a timeout for now, may consider treating as error */
		netreq->debug_end_time = _getdns_get_time_as_uintt64();
		_getdns_netreq_change_state(netreq, NET_REQ_TIMED_OUT);
		if (netreq->owner->user_callback) {
			(void)_getdns_context_request_timed_out(netreq->owner);
		}
		_getdns_check_dns_req_complete(netreq->owner);

	}
	GETDNS_FREE(((struct getdns_context *) vcontext)->mf, vdata);
}

/*
 * Read the number of answers in a cached record
 */
static int
mdns_cache_nb_records_in_entry(uint8_t * cached_data)
{
	int message_index = sizeof(getdns_mdns_cached_record_header);
	int nb_answers = (cached_data[message_index + MDNS_PACKET_INDEX_ANSWER] << 8) | 
		cached_data[message_index + MDNS_PACKET_INDEX_ANSWER + 1];

	return nb_answers;
}

/*
 * Create a key in preallocated buffer
 * the allocated size of key should be >= sizeof(getdns_mdns_cached_key_header) + name_len
 */
static void msdn_cache_create_key_in_buffer(
	uint8_t* key, 
	uint8_t * name, int name_len,
	int record_type, int record_class)
{
	getdns_mdns_cached_key_header * header = (getdns_mdns_cached_key_header*)key;

	memset(key, 0, sizeof(getdns_mdns_cached_key_header));
	header->record_type = record_type;
	header->record_class = record_class;
	header->name_len = name_len;
	(void) memcpy(key + sizeof(getdns_mdns_cached_key_header), name, name_len);
}

static uint8_t * mdns_cache_create_key(
	uint8_t * name, int name_len,
	int record_type, int record_class,
	struct getdns_context * context)
{
	uint8_t* key = GETDNS_XMALLOC(context->mf, uint8_t, sizeof(getdns_mdns_cached_key_header) + name_len);

	if (key != NULL)
	{
		msdn_cache_create_key_in_buffer(key, name, name_len, record_type, record_class);
	}

	return key;
}

static uint8_t * mdns_cache_create_data(
	uint8_t * name, int name_len,
	int record_type, int record_class,
	int record_data_len,
	uint64_t current_time,
	struct getdns_context * context)
{
	getdns_mdns_cached_record_header * header;
	int current_index;
	size_t data_size = sizeof(getdns_mdns_cached_record_header) + 12 + name_len + 4;
	size_t alloc_size = mdns_util_suggest_size(data_size + record_data_len + 2 + 2 + 2 + 4 + 2);

	uint8_t* data = GETDNS_XMALLOC(context->mf, uint8_t, alloc_size);

	if (data != NULL)
	{
		header = (getdns_mdns_cached_record_header *)data;
		header->insertion_microsec = current_time;
		header->content_len = data_size;
		header->allocated_length = alloc_size;
		header->netreq_first = NULL;
		current_index = sizeof(getdns_mdns_cached_record_header);
		memset(data + current_index, 0, 12);
		data[current_index + MDNS_PACKET_INDEX_QUERY + 1] = 1; /* 1 query present by default */
		current_index += 12;
		memcpy(data + current_index, name, name_len);
		current_index += name_len;
		data[current_index++] = (uint8_t)(record_type >> 8);
		data[current_index++] = (uint8_t)(record_type);
		data[current_index++] = (uint8_t)(record_class >> 8);
		data[current_index++] = (uint8_t)(record_class);
	}

	return data;
}


/*
 * Add a record.
 */
static int
mdns_add_record_to_cache_entry(struct getdns_context *context,
	uint8_t * old_record, uint8_t ** new_record,
	int record_type, int record_class, int ttl,
	uint8_t * record_data, int record_data_len)
{
	int ret = 0;
	getdns_mdns_cached_record_header *header = (getdns_mdns_cached_record_header*)old_record;
	/* Compute the record length */
	uint32_t record_length = 2 + 2 + 2 + 4 + 2 + record_data_len;
	uint32_t current_length = header->content_len;
	/* update the number of records */
	uint8_t *start_answer_code = old_record + sizeof(getdns_mdns_cached_record_header) + MDNS_PACKET_INDEX_ANSWER;
	uint16_t nb_answers = (start_answer_code[0] << 8) + start_answer_code[1];
	nb_answers++;
	start_answer_code[0] = (uint8_t)(nb_answers >> 8);
	start_answer_code[1] = (uint8_t)(nb_answers&0xFF);

	/* Update the content length and reallocate memory if needed */
	header->content_len += record_length;
	if (header->content_len > header->allocated_length)
	{
		/* realloc to a new length, */
		do {
			header->allocated_length = mdns_util_suggest_size(header->content_len);
		} while (header->content_len > header->allocated_length);

		*new_record = GETDNS_XREALLOC(context->mf, old_record, uint8_t, header->allocated_length); 
	}
	else
	{
		*new_record = old_record;
	}

	if (*new_record == NULL)
	{
		ret = GETDNS_RETURN_MEMORY_ERROR;
	}
	else
	{
		/* copy the record */
		/* First, point name relative to beginning of DNS message */
		(*new_record)[current_length++] = 0xC0;
		(*new_record)[current_length++] = 12;
		/* encode the components of the per record header */
		(*new_record)[current_length++] = (uint8_t)((record_type >> 8) & 0xFF);
		(*new_record)[current_length++] = (uint8_t)((record_type)& 0xFF);
		(*new_record)[current_length++] = (uint8_t)((record_class >> 8) & 0xFF);
		(*new_record)[current_length++] = (uint8_t)((record_class)& 0xFF);
		(*new_record)[current_length++] = (uint8_t)((ttl >> 24) & 0xFF);
		(*new_record)[current_length++] = (uint8_t)((ttl >> 16) & 0xFF);
		(*new_record)[current_length++] = (uint8_t)((ttl >> 8) & 0xFF);
		(*new_record)[current_length++] = (uint8_t)((ttl)& 0xFF);
		(*new_record)[current_length++] = (uint8_t)((record_data_len >> 8) & 0xFF);
		(*new_record)[current_length++] = (uint8_t)((record_data_len) & 0xFF);
		memcpy(*new_record + current_length, record_data, record_data_len);

	}

	return ret;
}

static int
mdns_update_cache_ttl_and_prune(struct getdns_context *context,
	uint8_t * old_record, uint8_t ** new_record,
	int record_type, int record_class, int ttl,
	uint8_t * record_data, int record_data_len,
	uint64_t current_time)
{
	/*
	 * Compute the TTL delta
	 */
	int ret = 0;
	getdns_mdns_cached_record_header *header = (getdns_mdns_cached_record_header*)old_record;
	uint32_t delta_t_sec = (uint32_t)((current_time - header->insertion_microsec) / 1000000ll);
	header->insertion_microsec += delta_t_sec * 1000000;
	int message_index;
	int answer_index;
	int nb_answers;
	int nb_answers_left;
	int current_record_length;
	int current_record_data_len;
	uint32_t current_record_ttl;
	int not_matched_yet = (record_data == NULL) ? 0 : 1;
	int last_copied_index;
	int current_hole_index = 0;
	int record_name_length = 0;
	int record_ttl_index = 0;
	int i;

	/*
	 * Skip the query
	 */
	message_index = sizeof(getdns_mdns_cached_record_header);
	nb_answers = (old_record[message_index + MDNS_PACKET_INDEX_ANSWER] << 8) |
		old_record[message_index + MDNS_PACKET_INDEX_ANSWER + 1];
	nb_answers_left = nb_answers;
	answer_index = message_index + 12 + mdns_util_skip_query(old_record + message_index + 12);
	last_copied_index = answer_index;

	/*
	 * Examine each record
	 */
	for (i = 0; i < nb_answers; i++)
	{
		record_name_length = mdns_util_skip_name(old_record + answer_index);
		record_ttl_index = answer_index + record_name_length + 2 + 2;

		current_record_ttl = (old_record[record_ttl_index] << 24)
			| (old_record[record_ttl_index + 1] << 16)
			| (old_record[record_ttl_index + 2] << 8)
			| (old_record[record_ttl_index + 3]);

		current_record_data_len = (old_record[record_ttl_index + 4] << 8)
			| (old_record[record_ttl_index + 5]);

		current_record_length = record_name_length + 2 + 2 + 4 + 2 + current_record_data_len;

		if (not_matched_yet &&
		    current_record_data_len == record_data_len &&
			memcmp(old_record + record_ttl_index + 4 + 2, record_data, record_data_len) == 0)
		{
			not_matched_yet = 0;
			current_record_ttl = ttl;
		}
		else
		{
			/* Not a match */
			if (current_record_ttl > delta_t_sec)
			{
				current_record_ttl -= delta_t_sec;
			}
			else
			{
				current_record_ttl = 0;
			}
		}

		if (current_record_ttl == 0)
		{
			nb_answers_left--;

			/* this record should be compacted away */
			if (current_hole_index == 0)
			{
				/* encountering the first hole in the message,
				 * no need to copy anything yet.
				 */
				last_copied_index = answer_index;
			}
			else if (current_hole_index != answer_index)
			{
				/* copy the data from hole to answer */
				memmove(old_record + last_copied_index, old_record + current_hole_index,
					answer_index - current_hole_index);
				last_copied_index += answer_index - current_hole_index;
			}
			
			/* extend the current hole */
			current_hole_index = answer_index + current_record_length;
		}
		else
		{
			/* keeping this record, but updating the TTL */
			old_record[record_ttl_index] = (uint8_t)(current_record_ttl >> 24);
			old_record[record_ttl_index + 1] = (uint8_t)(current_record_ttl >> 16);
			old_record[record_ttl_index + 2] = (uint8_t)(current_record_ttl >> 8);
			old_record[record_ttl_index + 3] = (uint8_t)(current_record_ttl);
		}
		/* progress to the next record */
		answer_index += current_record_length;
	}

	/* if necessary, copy the pending data */
	if (current_hole_index != answer_index && current_hole_index != 0)
	{
		/* copy the data from hole to last answer */
		memmove(old_record + last_copied_index, old_record + current_hole_index,
			answer_index - current_hole_index);
		last_copied_index += answer_index - current_hole_index;

		/* dead assignment */
		/* answer_index = last_copied_index; */
	}

	/* if some records were deleted, update the record headers */
	if (nb_answers != nb_answers_left)
	{
		header->content_len = last_copied_index;
		old_record[message_index + MDNS_PACKET_INDEX_ANSWER] = (uint8_t)(nb_answers_left >> 8);
		old_record[message_index + MDNS_PACKET_INDEX_ANSWER + 1] = (uint8_t)(nb_answers_left);
	}

	/*
	* if the update was never seen, ask for an addition
	*/
	if (ttl > 0 && not_matched_yet)
	{
		mdns_add_record_to_cache_entry(context, old_record, new_record,
			record_type, record_class, ttl, record_data, record_data_len);
		nb_answers_left++;
	}
	else
	{
		*new_record = old_record;
	}

	return ret;
}

/*
 * Get a cached entry by name and record type .
 */
static struct lruhash_entry *
mdns_access_cached_entry_by_name(
struct getdns_context *context,
	uint8_t * name, int name_len,
	int record_type, int record_class)
{
	uint8_t temp_key[256 + sizeof(getdns_mdns_cached_key_header)];
	hashvalue_type hash;
	struct lruhash_entry *entry;

	msdn_cache_create_key_in_buffer(temp_key, name, name_len, record_type, record_class);

	/* TODO: make hash init value a random number in the context, for defense against DOS */
	hash = hashlittle(temp_key, name_len + sizeof(getdns_mdns_cached_key_header), 0xCAC8E);

	entry = lruhash_lookup(context->mdns_cache, hash, temp_key, 1);

	return entry;
}


/*
* Add entry function for the MDNS record cache.
*/
static int
mdns_propose_entry_to_cache(
	struct getdns_context *context,
	uint8_t * name, int name_len,
	int record_type, int record_class, int ttl,
	uint8_t * record_data, int record_data_len,
	getdns_network_req * netreq,
	uint64_t current_time)
{
	int ret = 0;
	uint8_t temp_key[256 + sizeof(getdns_mdns_cached_key_header)];
	hashvalue_type hash;
	struct lruhash_entry *entry, *new_entry;
	uint8_t *key, *data;
	getdns_mdns_cached_record_header * header;

	msdn_cache_create_key_in_buffer(temp_key, name, name_len, record_type, record_class);

	
	/* TODO: make hash init value a random number in the context, for defense against DOS */
	hash = hashlittle(temp_key, name_len + sizeof(getdns_mdns_cached_key_header), 0xCAC8E);

	entry = lruhash_lookup(context->mdns_cache, hash, temp_key, 1);

	if (entry == NULL && ttl != 0)
	{
		/*
		 * Create an empty entry.
		 */
		key = mdns_cache_create_key(name, name_len, record_type, record_class, context);
		data = mdns_cache_create_data(name, name_len, record_type, record_class,
				record_data_len, current_time, context);

		if (key == NULL || data == NULL)
		{
			if (key != NULL)
			{
				GETDNS_FREE(context->mf, key);
				key = NULL;
			}

			if (data != NULL)
			{
				GETDNS_FREE(context->mf, data);
				data = NULL;
			}
		}
		else
		{
			new_entry = &((getdns_mdns_cached_key_header*)key)->entry;

			memset(new_entry, 0, sizeof(struct lruhash_entry));
			lock_rw_init(&new_entry->lock);
			new_entry->hash = hash;
			new_entry->key = key;
			new_entry->data = data;

			entry = lruhash_insert_or_retrieve(context->mdns_cache, hash, new_entry, data, NULL);

			if (entry != new_entry)
			{
				/* There was already an entry for this name, which is really weird. 
				 * But it can in theory happen in a race condition.
				 */
				GETDNS_FREE(context->mf, key);
				key = NULL;
				GETDNS_FREE(context->mf, data);
				data = NULL;
			}
		}
	}

	if (entry != NULL)
	{
		if (record_data != NULL && record_data_len > 0)
			ret = mdns_update_cache_ttl_and_prune(context,
				(uint8_t*)entry->data, &data,
				record_type, record_class, ttl, record_data, record_data_len,
				current_time);

		if (netreq != NULL)
		{
			/* chain the continuous request to the cache line */
			header = (getdns_mdns_cached_record_header *) entry->data;
			netreq->mdns_netreq_next = header->netreq_first;
			header->netreq_first = netreq;
		}
		else
		{
			header = (getdns_mdns_cached_record_header *)entry->data;

			/* if the entry is empty, move it to the bottom of the LRU */
			if (mdns_cache_nb_records_in_entry((uint8_t*)(entry->data)) == 0 &&
				header->netreq_first == NULL)
			{
				lru_demote(context->mdns_cache, entry);
			}
		}

		/* then, unlock the entry */
		lock_rw_unlock(&entry->lock);
	} 

	return ret;
}


/*
 * Serve a request from the cached value
 */
static int
mdns_complete_query_from_cache_entry(
	getdns_network_req *netreq,
	struct lruhash_entry *entry)
{
	int ret = 0;
	uint8_t *packet = ((uint8_t *)entry->data) + sizeof(getdns_mdns_cached_record_header);
	getdns_mdns_cached_record_header * header = (getdns_mdns_cached_record_header*)entry->data;
	size_t packet_length = header->content_len - sizeof(getdns_mdns_cached_record_header);
	getdns_network_req **prev_netreq;
	int found = 0;
	int nb_answers = mdns_cache_nb_records_in_entry((uint8_t *)entry->data);

	/* Clear the event associated to the query */
	GETDNS_CLEAR_EVENT(netreq->owner->loop, &netreq->event);

	/* remove the completed query from the waiting list */
	prev_netreq = &header->netreq_first;
	while (*prev_netreq != NULL)
	{
		if (*prev_netreq == netreq)
		{
			*prev_netreq = netreq->mdns_netreq_next;
			netreq->mdns_netreq_next = NULL;
			found = 1;
			break;
		}
		else
		{
			prev_netreq = &((*prev_netreq)->mdns_netreq_next);
		}
	}

	if (found)
	{
		if (nb_answers == 0)
		{
		}
		else
		{
			/* copy the returned value in the response field  */
			if (packet_length > netreq->wire_data_sz)
			{
				netreq->response = GETDNS_XREALLOC(
					netreq->owner->context->mf, netreq->response, uint8_t, packet_length);
			}

			if (netreq->response != NULL)
			{
				memcpy(netreq->response, packet, packet_length);

				netreq->response[MDNS_PACKET_INDEX_QCODE] = 0x84;

				netreq->response_len = packet_length;
				netreq->debug_end_time = _getdns_get_time_as_uintt64();
				_getdns_netreq_change_state(netreq, NET_REQ_FINISHED);
				_getdns_check_dns_req_complete(netreq->owner);
			}
			else
			{
				/* Fail the query? */
				netreq->response_len = 0;
				netreq->debug_end_time = _getdns_get_time_as_uintt64();
				_getdns_netreq_change_state(netreq, NET_REQ_ERRORED);
				_getdns_check_dns_req_complete(netreq->owner);
			}
		}
	}
	else
	{
		/* Failure */
		netreq->response_len = 0;
		netreq->debug_end_time = _getdns_get_time_as_uintt64();
		_getdns_netreq_change_state(netreq, NET_REQ_ERRORED);
		_getdns_check_dns_req_complete(netreq->owner);
	}

	return ret;
}

/*
 * Processing of requests after cache update.
 * This is coded as synchronous processing, under lock. This is probably wrong.
 * It would be better to just collate the responses for now, and 
 * process the queries out of the loop.
 */
static int
mdns_cache_complete_queries(
	struct getdns_context *context,
	uint8_t * name, int name_len,
	int record_type, int record_class)
{
	int ret = 0;
	struct lruhash_entry *entry;
	getdns_mdns_cached_record_header * header;
	getdns_network_req * netreq;

	entry = mdns_access_cached_entry_by_name(context, name, name_len, record_type, record_class);

	if (entry != NULL)
	{
		if (entry->data != NULL)
		{
			header = (getdns_mdns_cached_record_header *)entry->data;

			while ((netreq = header->netreq_first) != NULL)
			{
				mdns_complete_query_from_cache_entry(netreq, entry);
			}
		}
		lock_rw_unlock(&entry->lock);
	}

	return ret;
}

/*
* Timeout of multicast MDNS query
*/
static void
mdns_mcast_timeout_cb(void *userarg)
{
	getdns_network_req *netreq = (getdns_network_req *)userarg;
	getdns_dns_req *dnsreq = netreq->owner;
	getdns_context *context = dnsreq->context;

	uint8_t temp_key[256 + sizeof(getdns_mdns_cached_key_header)];
	hashvalue_type hash;
	struct lruhash_entry *entry;
	int found = 0;

	DEBUG_MDNS("%s %-35s: MSG:  %p\n",
		MDNS_DEBUG_CLEANUP, __FUNC__, netreq);

	msdn_cache_create_key_in_buffer(temp_key, dnsreq->name, dnsreq->name_len,
		netreq->request_type, dnsreq->request_class);


	/* TODO: make hash init value a random number in the context, for defense against DOS */
	hash = hashlittle(temp_key, dnsreq->name_len + sizeof(getdns_mdns_cached_key_header), 0xCAC8E);

	/* Open the corresponding cache entry */
	entry = lruhash_lookup(context->mdns_cache, hash, temp_key, 1);

	if (entry != NULL)
	{
		if (entry->data != NULL)
		{
			/* Remove entry from chain and serve the query */
			found = 1;
			mdns_complete_query_from_cache_entry(netreq, entry);
		}
		lock_rw_unlock(&entry->lock);
	}

	if (!found)
	{
		/* Fail the request on timeout */
		netreq->response_len = 0;
		netreq->debug_end_time = _getdns_get_time_as_uintt64();
		_getdns_netreq_change_state(netreq, NET_REQ_ERRORED);
		_getdns_check_dns_req_complete(netreq->owner);
	}
}

/*
 * Multicast receive event callback
 */
static void
mdns_udp_multicast_read_cb(void *userarg)
{
	mdns_network_connection * cnx = (mdns_network_connection *)userarg;
	uint64_t current_time;
	ssize_t       read;
	DEBUG_MDNS("%s %-35s: CTX: %p, NET=%d \n", MDNS_DEBUG_MREAD,
		__FUNC__, cnx->context, cnx->addr_mcast.ss_family);

	current_time = _getdns_get_time_as_uintt64();

	GETDNS_CLEAR_EVENT(
		cnx->context->extension, &cnx->event);

	read = recvfrom(cnx->fd, (void *)cnx->response,
		sizeof(cnx->response), 0, NULL, NULL);


	if (read == -1 && (_getdns_socketerror() == _getdns_EWOULDBLOCK ||
		           _getdns_socketerror() == _getdns_ECONNRESET))
		return; /* TODO: this will stop the receive loop! */

	if (read >= GLDNS_HEADER_SIZE)
	{
		/* parse the response, find the relevant queries, submit the records to the cache */
		int opcodeAndflags = cnx->response[2];
		int nb_queries = (cnx->response[4] << 8) | cnx->response[5];
		int nb_responses = (cnx->response[6] << 8) | cnx->response[7];

		if (opcodeAndflags != 0x84)
		{
			/* this is not an MDNS answer packet. */
		}
		else
		{
			ssize_t current_index = 12;
			uint8_t name[256];
			int name_len;
			int record_type;
			int record_class;
			int record_ttl;
			int record_data_len;
			int nb_records = 0;
			int nb_queries_skipped = 0;
			int start_of_records;
			int signalled_records = 0;

			/*
			 * In normal mDNS operation, there should not be any query here. 
			 * But just in case, we can skip the queries...
			 */
			while (current_index < read && nb_queries_skipped < nb_queries)
			{
				current_index += mdns_util_skip_query(&cnx->response[current_index]);
				nb_queries_skipped++;
			}
			start_of_records = current_index;
			/*
			 * Parse the answers and propose them to the cache
			 */

			while (current_index < read && nb_records < nb_responses)
			{
				/* Copy and skip the name */
				current_index = mdns_util_copy_name(cnx->response, read, current_index,
					name, sizeof(name), 0, &name_len);
				if (current_index + 12 >= read)
				{
					/* bogus packet.. Should log. */
					current_index = read;
				}
				else
				{
					/* Parse the record header */
					record_type = (cnx->response[current_index++] << 8);
					record_type |= (cnx->response[current_index++]);
					/* TODO: handle the cache flush bit! */
					record_class = (cnx->response[current_index++] << 8)&0x7F;
					record_class |= (cnx->response[current_index++]);
					record_ttl = (cnx->response[current_index++] << 24);
					record_ttl |= (cnx->response[current_index++] << 16);
					record_ttl |= (cnx->response[current_index++] << 8);
					record_ttl |= (cnx->response[current_index++]);
					record_data_len = (cnx->response[current_index++] << 8);
					record_data_len |= (cnx->response[current_index++]);

					if (current_index + record_data_len <= read)
					{
						/* 
						 * Set the record to canonical form. This is required, since 
						 * MDNS software commonly uses name compression for PTR or SRV records.
						 */
						int actual_length;
						uint8_t *actual_record;
						uint8_t buffer[1024];

						/* TODO: do something in case of canonization failures */
						(void) mdns_util_canonical_record(cnx->response, read,
							record_type, record_class, record_data_len, current_index,
							buffer, sizeof(buffer), &actual_record, &actual_length);


						/* Submit to the cache. As a side effect, may signal that a continuous request is done. */
						(void) mdns_propose_entry_to_cache(cnx->context, name, name_len,
							record_type, record_class, record_ttl,
							actual_record, actual_length, NULL,
							current_time);

						current_index += record_data_len;
						nb_records++;
					}
					else
					{
						/* bogus packet.. Should log. */
						current_index = read;
					}
				}
			}

			/* Go over the queries that were mentioned in the update, and prepare returns. */
			current_index = start_of_records;
			while (current_index < read && signalled_records < nb_responses)
			{
				/* copy the name */
				current_index = mdns_util_copy_name(cnx->response, read, current_index,
					name, sizeof(name), 0, &name_len);
				if (current_index + 12 >= read)
				{
					/* bogus packet.. Should log. */
					current_index = read;
				}
				else
				{
					/* Parse the record header */
					record_type = (cnx->response[current_index++] << 8);
					record_type |= (cnx->response[current_index++]);
					record_class = (cnx->response[current_index++] << 8);
					record_class |= (cnx->response[current_index++]);
					current_index += 4;
					record_data_len = (cnx->response[current_index++] << 8);
					record_data_len |= (cnx->response[current_index++]);
					current_index += record_data_len;

					/* process the pending requests */
					(void)mdns_cache_complete_queries(cnx->context, name, name_len, record_type, record_class);
				}
			}
		}
	}
	else
	{
		/* bogus packet.. Should log. */
	}

	/*
	 * Relaunch the event, so we can go read the next packet.
	 */
	GETDNS_SCHEDULE_EVENT(
		cnx->context->extension, cnx->fd, 0,
		getdns_eventloop_event_init(&cnx->event, cnx,
			mdns_udp_multicast_read_cb, NULL, NULL));
}

/*
 * Create the two required multicast sockets
 */
static int mdns_open_ipv4_multicast(SOCKADDR_STORAGE* mcast_dest, int* mcast_dest_len)
{
	getdns_return_t ret = 0;
	SOCKET fd4 = -1;
	SOCKADDR_IN ipv4_dest;
	SOCKADDR_IN ipv4_port;
	BOOL so_reuse_bool = TRUE;
	int so_reuse_bool_OptLen = sizeof(BOOL);
	uint8_t ttl = 255;
	IP_MREQ mreq4;

	memset(mcast_dest, 0, sizeof(SOCKADDR_STORAGE));
	*mcast_dest_len = 0;
	memset(&ipv4_dest, 0, sizeof(ipv4_dest));
	memset(&ipv4_port, 0, sizeof(ipv4_dest));
	ipv4_dest.sin_family = AF_INET;
	ipv4_dest.sin_port = htons(MDNS_MCAST_PORT);
	ipv4_dest.sin_addr.s_addr = htonl(MDNS_MCAST_IPV4_LONG);
	ipv4_port.sin_family = AF_INET;
	ipv4_port.sin_port = htons(MDNS_MCAST_PORT);
	/* memcpy(&ipv4_dest.sin_addr, mdns_mcast_ipv4, sizeof(mdns_mcast_ipv4)); */



	fd4 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (fd4 != -1)
	{
		/*
		 * No need to test the output of the so_reuse call,
		 * since the only result that matters is that of bind.
		 */
		(void)setsockopt(fd4, SOL_SOCKET, SO_REUSEADDR
			, (const char*)&so_reuse_bool, so_reuse_bool_OptLen);

		if (bind(fd4, (SOCKADDR*)&ipv4_port, sizeof(ipv4_port)) != 0)
		{
			ret = -1;
		}
		else
		{
			mreq4.imr_multiaddr = ipv4_dest.sin_addr;
			mreq4.imr_interface = ipv4_port.sin_addr;

			if (setsockopt(fd4, IPPROTO_IP, IP_ADD_MEMBERSHIP
				, (const char*)&mreq4, (int) sizeof(mreq4)) != 0)
			{
				ret = -1;
			}
			else if (setsockopt(fd4, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) != 0)
			{
				ret = -1;
			}
		}
	}

	if (ret != 0 && fd4 != -1)
	{
			_getdns_closesocket(fd4);
			fd4 = -1;
	}

	if (ret == 0)
	{
		memcpy(mcast_dest, &ipv4_dest, sizeof(ipv4_dest));
		*mcast_dest_len = sizeof(ipv4_dest);
	}

	return fd4;
}

static int mdns_open_ipv6_multicast(SOCKADDR_STORAGE* mcast_dest, int* mcast_dest_len)
{
	getdns_return_t ret = 0;
	SOCKET fd6 = -1;
	SOCKADDR_IN6 ipv6_dest;
	SOCKADDR_IN6 ipv6_port;
	uint8_t so_reuse_bool = 1;
	uint8_t ttl = 255;
	IPV6_MREQ mreq6;

	memset(mcast_dest, 0, sizeof(SOCKADDR_STORAGE));
	*mcast_dest_len = 0;
	memset(&ipv6_dest, 0, sizeof(ipv6_dest));
	memset(&ipv6_port, 0, sizeof(ipv6_dest));
	ipv6_dest.sin6_family = AF_INET6;
	ipv6_dest.sin6_port = htons(MDNS_MCAST_PORT);
	ipv6_port.sin6_family = AF_INET6;
	ipv6_port.sin6_port = htons(MDNS_MCAST_PORT);
	memcpy(&ipv6_dest.sin6_addr
		, mdns_mcast_ipv6, sizeof(mdns_mcast_ipv6));


	fd6 = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	if (fd6 != -1)
	{
		/*
		* No need to test the output of the so_reuse call,
		* since the only result that matters is that of bind.
		*/
		(void)setsockopt(fd6, SOL_SOCKET, SO_REUSEADDR
			, (const char*)&so_reuse_bool, (int) sizeof(BOOL));

		if (bind(fd6, (SOCKADDR*)&ipv6_port, sizeof(ipv6_port)) != 0)
		{
			ret = -1;
		}
		else
		{
			memcpy(&mreq6.ipv6mr_multiaddr
				, &ipv6_dest.sin6_addr, sizeof(mreq6.ipv6mr_multiaddr));
			memcpy(&mreq6.ipv6mr_interface
				, &ipv6_port.sin6_addr, sizeof(mreq6.ipv6mr_interface));

			if (setsockopt(fd6, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP
				, (const char*)&mreq6, (int) sizeof(mreq6)) != 0)
			{
				ret = -1;
			}
			else if (setsockopt(fd6, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl)) != 0)
			{
				ret = -1;
			}
		}
	}

	if (ret != 0 && fd6 != -1)
	{
		_getdns_closesocket(fd6);
		fd6 = -1;
	}

	if (ret == 0)
	{
		memcpy(mcast_dest, &ipv6_dest, sizeof(ipv6_dest));
		*mcast_dest_len = sizeof(ipv6_dest);
	}
	return fd6;
}

/*
 * Delayed opening of the MDNS sockets, and launch of the MDNS listeners
 */
static getdns_return_t mdns_delayed_network_init(struct getdns_context *context)
{
	getdns_return_t ret = 0;

	if (context->mdns_extended_support == 2)
	{
		context->mdns_cache = lruhash_create(128, 10000000,
			mdns_cache_entry_size, mdns_cache_key_comp,
			msdn_cache_delkey, msdn_cache_deldata,
			context);

		if (context->mdns_cache == NULL)
		{
			ret = GETDNS_RETURN_MEMORY_ERROR;
		}
		else
		{
			context->mdns_connection = (mdns_network_connection *)
				GETDNS_XMALLOC(context->my_mf, mdns_network_connection, 2);

			if (context->mdns_connection == NULL)
			{
				ret = GETDNS_RETURN_MEMORY_ERROR;
			}
			else
			{
				context->mdns_connection_nb = 2;

				context->mdns_connection[0].fd = mdns_open_ipv4_multicast(
					&context->mdns_connection[0].addr_mcast
					, &context->mdns_connection[0].addr_mcast_len);
				context->mdns_connection[0].context = context;
				context->mdns_connection[1].fd = mdns_open_ipv6_multicast(
					&context->mdns_connection[1].addr_mcast
					, &context->mdns_connection[1].addr_mcast_len);
				context->mdns_connection[1].context = context;

				if (context->mdns_connection[0].fd == -1 ||
					context->mdns_connection[1].fd == -1)
				{
					ret = GETDNS_RETURN_GENERIC_ERROR;
				}
				else
				{
					int i;
					/* TODO: launch the receive loops */
					for (i = 0; i < 2; i++)
					{
						GETDNS_CLEAR_EVENT(context->extension, &context->mdns_connection[i].event);
						GETDNS_SCHEDULE_EVENT(
							context->extension, context->mdns_connection[i].fd, 0,
							getdns_eventloop_event_init(&context->mdns_connection[i].event,
								&context->mdns_connection[i],
								mdns_udp_multicast_read_cb, NULL, NULL));
					}
				}

				if (ret != 0)
				{
					int i;
					for (i = 0; i < 2; i++)
					{
						if (context->mdns_connection[i].fd != -1)
						{

							GETDNS_CLEAR_EVENT(context->extension
								, &context->mdns_connection[i].event);
							_getdns_closesocket(context->mdns_connection[i].fd);
						}
					}

					GETDNS_FREE(context->my_mf, context->mdns_connection);
					context->mdns_connection = NULL;
					context->mdns_connection_nb = 0;
				}
			} /* mdns-connection != NULL */

			if (ret != 0)
			{
				/* delete the cache that was just created, since the network connection failed */
				lruhash_delete(context->mdns_cache);
				context->mdns_cache = NULL;
			}
		} /* cache != NULL */

		context->mdns_extended_support = (ret == 0) ? 1 : 0;
	}

	return ret;
}

/*
 * Initialize a continuous query from netreq
 */
static getdns_return_t mdns_initialize_continuous_request(getdns_network_req *netreq)
{
	getdns_return_t ret = 0;
	getdns_dns_req *dnsreq = netreq->owner;
	struct getdns_context *context = dnsreq->context;

	uint8_t temp_key[256 + sizeof(getdns_mdns_cached_key_header)];
	hashvalue_type hash;
	struct lruhash_entry *entry;
	size_t pkt_len = netreq->response - netreq->query;

	msdn_cache_create_key_in_buffer(temp_key, dnsreq->name, dnsreq->name_len,
		netreq->request_type, dnsreq->request_class);

	/* TODO: make hash init value a random number in the context, for defense against DOS */
	hash = hashlittle(temp_key, dnsreq->name_len + sizeof(getdns_mdns_cached_key_header), 0xCAC8E);

	entry = lruhash_lookup(context->mdns_cache, hash, temp_key, 1);

	if (entry == NULL)
	{
		/*
		 * First, create an entry for the query
		 */

		ret = mdns_propose_entry_to_cache(context, dnsreq->name, dnsreq->name_len,
			netreq->request_type, dnsreq->request_class, 1, NULL, 0,
			netreq, _getdns_get_time_as_uintt64());
	}
	else
	{
		/*
		 * Check whether the cache entry is recent.
		 * If yes, just return it, but first update the entry tracking in the cache entry.
		 */

		 /*
		  * and unlock the entry!
		  */
	}

	if (ret == 0)
	{
		/* If the query is not actually complete, are a per query timer. */
		if (netreq->state < NET_REQ_FINISHED)
		{
			GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);
			GETDNS_SCHEDULE_EVENT(
				dnsreq->loop, -1, _getdns_ms_until_expiry(dnsreq->expires),
				getdns_eventloop_event_init(&netreq->event, netreq,
					NULL, NULL, mdns_mcast_timeout_cb));
		}
		/* If the entry was created less than 1 sec ago, send a query */

		if (context->mdns_connection_nb <= 0)
		{
			/* oops, no network! */
			ret = GETDNS_RETURN_GENERIC_ERROR;
		}
		else
		{
			/* TODO? Set TTL=255 for compliance with RFC 6762 */
			int fd_index = context->mdns_connection_nb - 1;
			int sent = sendto(
				context->mdns_connection[fd_index].fd
				, (const void *)netreq->query, pkt_len, 0
				, (SOCKADDR*)&context->mdns_connection[fd_index].addr_mcast
				, context->mdns_connection[fd_index].addr_mcast_len);

			if (sent < 0 || pkt_len != (size_t)sent)
			{
				ret = GETDNS_RETURN_GENERIC_ERROR;
			}

			/* TODO: update the send query time */
		}
	}

	return ret;
}

/*
 * Initialize the MDNS part of the context structure.
 */
void _getdns_mdns_context_init(struct getdns_context *context)
{
	context->mdns_extended_support = 2; /* 0 = no support, 1 = supported, 2 = initialization needed */
	context->mdns_connection = NULL;
	context->mdns_connection_nb = 0;
	context->mdns_cache = NULL;
}

/*
 * Delete all the data allocated for MDNS in a context
 */
void _getdns_mdns_context_destroy(struct getdns_context *context)
{
	/* Clear all the cached records. This will terminate all pending network requests */
	if (context->mdns_cache != NULL)
	{
		lruhash_delete(context->mdns_cache);
		context->mdns_cache = NULL;
	}
	/* Close the connections */
	if (context->mdns_connection != NULL)
	{
		int i;
		for (i = 0; i < context->mdns_connection_nb; i++)
		{
			/* suppress the receive event */
			GETDNS_CLEAR_EVENT(context->extension, &context->mdns_connection[i].event);
			/* close the socket */
			_getdns_closesocket(context->mdns_connection[i].fd);
		}

		GETDNS_FREE(context->mf, context->mdns_connection);
		context->mdns_connection = NULL;
		context->mdns_connection_nb = 0;
	}
}

/* TODO: actually delete what is required.. */
static void
mdns_cleanup(getdns_network_req *netreq)
{
	DEBUG_MDNS("%s %-35s: MSG: %p\n",
		MDNS_DEBUG_CLEANUP, __FUNC__, netreq);
	getdns_dns_req *dnsreq = netreq->owner;

	GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);
}

void
_getdns_cancel_mdns_request(getdns_network_req *netreq)
{
	mdns_cleanup(netreq);
	if (netreq->fd >= 0) {
		_getdns_closesocket(netreq->fd);
	}
}

static void
mdns_timeout_cb(void *userarg)
{
	getdns_network_req *netreq = (getdns_network_req *)userarg;
	DEBUG_MDNS("%s %-35s: MSG:  %p\n",
		MDNS_DEBUG_CLEANUP, __FUNC__, netreq);

	/* TODO: do we need a retry logic here? */

	/* Check the required cleanup */
	mdns_cleanup(netreq);
	if (netreq->fd >= 0)
		_getdns_closesocket(netreq->fd);
	_getdns_netreq_change_state(netreq, NET_REQ_TIMED_OUT);
	if (netreq->owner->user_callback) {
		netreq->debug_end_time = _getdns_get_time_as_uintt64();
		(void)_getdns_context_request_timed_out(netreq->owner);
	}
	else
		_getdns_check_dns_req_complete(netreq->owner);
}



/*****************************************/
/* UDP callback functions for basic MDNS */
/*****************************************/

static void
mdns_udp_read_cb(void *userarg)
{
	getdns_network_req *netreq = (getdns_network_req *)userarg;
	getdns_dns_req *dnsreq = netreq->owner;
	ssize_t       read;
	DEBUG_MDNS("%s %-35s: MSG: %p \n", MDNS_DEBUG_READ,
		__FUNC__, netreq);

	GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);

	read = recvfrom(netreq->fd, (void *)netreq->response,
		netreq->max_udp_payload_size + 1, /* If read == max_udp_payload_size
										  * then all is good.  If read ==
										  * max_udp_payload_size + 1, then
										  * we receive more then requested!
										  * i.e. overflow
										  */
		0, NULL, NULL);
	if (read == -1 && (_getdns_socketerror() == _getdns_EWOULDBLOCK ||
		           _getdns_socketerror() == _getdns_ECONNRESET))
		return;

	if (read < GLDNS_HEADER_SIZE)
		return; /* Not DNS */

	if (GLDNS_ID_WIRE(netreq->response) != GLDNS_ID_WIRE(netreq->query))
		return; /* Cache poisoning attempt ;) */

	// TODO: check whether EDNS server cookies are required for MDNS

	// TODO: check that the source address originates from the local network.
	// TODO: check TTL = 255

	_getdns_closesocket(netreq->fd);
	/* 
	 * TODO: how to handle an MDNS response with TC bit set?
	 * Ignore it for now, as we do not support any kind of TCP fallback
	 * for basic MDNS.
	 */
	
	netreq->response_len = read;
	netreq->debug_end_time = _getdns_get_time_as_uintt64();
	_getdns_netreq_change_state(netreq, NET_REQ_FINISHED);
	_getdns_check_dns_req_complete(dnsreq);
}

static void
mdns_udp_write_cb(void *userarg)
{
	getdns_network_req *netreq = (getdns_network_req *)userarg;
	getdns_dns_req     *dnsreq = netreq->owner;
	size_t             pkt_len = netreq->response - netreq->query;
	struct sockaddr_in mdns_mcast_v4;
	int	ttl = 255;
	int r;

	DEBUG_MDNS("%s %-35s: MSG: %p \n", MDNS_DEBUG_WRITE,
		__FUNC__, netreq);

	GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);

	netreq->debug_start_time = _getdns_get_time_as_uintt64();
	netreq->debug_udp = 1;
	GLDNS_ID_SET(netreq->query, (uint16_t) arc4random());

	/* do we need to handle options valid in the MDNS context? */

	/* Probably no need for TSIG in MDNS */


	/* Always use multicast address */
	mdns_mcast_v4.sin_family = AF_INET;
	mdns_mcast_v4.sin_port = htons(MDNS_MCAST_PORT);
	mdns_mcast_v4.sin_addr.s_addr = htonl(MDNS_MCAST_IPV4_LONG);
	/* memcpy(&mdns_mcast_v4.sin_addr.s_addr, mdns_mcast_ipv4, sizeof(mdns_mcast_ipv4)); */

	/* Set TTL=255 for compliance with RFC 6762 */
	r = setsockopt(netreq->fd, IPPROTO_IP, IP_TTL, (const char *)&ttl, sizeof(ttl));

	if (r != 0 || 
		(ssize_t)pkt_len != sendto(
		netreq->fd, (const void *)netreq->query, pkt_len, 0,
		(struct sockaddr *)&mdns_mcast_v4,
		sizeof(mdns_mcast_v4))) {
		_getdns_closesocket(netreq->fd);
		return;
	}
	GETDNS_SCHEDULE_EVENT(
		dnsreq->loop, netreq->fd,
		_getdns_ms_until_expiry(dnsreq->expires),
		getdns_eventloop_event_init(&netreq->event, netreq,
			mdns_udp_read_cb, NULL, mdns_timeout_cb));
}

/*
 * MDNS Request Submission
 */

getdns_return_t
_getdns_submit_mdns_request(getdns_network_req *netreq)
{
	DEBUG_MDNS("%s %-35s: MSG: %p TYPE: %d\n", MDNS_DEBUG_ENTRY, __FUNC__,
		netreq, netreq->request_type);
	int fd = -1;
	getdns_dns_req *dnsreq = netreq->owner;
	struct getdns_context * context = dnsreq->context;
	getdns_return_t ret = 0;

	/*
	 * TO DO: depending on context type, perform basic processing versus full MDNS
	 */

	if (context->mdns_extended_support == 2)
	{
		/* Not initialize yet. Do it know before processing the query */
		ret = mdns_delayed_network_init(context);

		if (ret != 0)
		{
			return ret;
		}
	}

	if (context->mdns_extended_support == 1)
	{
		/* extended DNS support */
		ret = mdns_initialize_continuous_request(netreq);
	}
	else
	{
		/* basic MDNS request */

		/* Open the UDP socket required for the request */
		if ((fd = socket(
			AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
			return -1;
		/* TODO: do we need getdns_sock_nonblock(fd); */

		/* Schedule the MDNS request */
		netreq->fd = fd;
		GETDNS_CLEAR_EVENT(dnsreq->loop, &netreq->event);
		GETDNS_SCHEDULE_EVENT(
			dnsreq->loop, netreq->fd,
			_getdns_ms_until_expiry(dnsreq->expires),
			getdns_eventloop_event_init(&netreq->event, netreq,
				NULL, mdns_udp_write_cb, mdns_timeout_cb));
		ret = GETDNS_RETURN_GOOD;
	}

	return ret;
}

/*
 * MDNS name space management
 */

static int
mdns_suffix_compare(register const uint8_t *d1, register const uint8_t *d2)
{
	int ret = 0;
	uint8_t *d1_head = (uint8_t *) d1;
	uint8_t *d1_current;
	uint8_t *d2_current;
	int is_matching = 0;
	int part_length;
	int i;
	uint8_t c;

	/* Skip the first name part, since we want at least one label before the suffix */
	if (*d1_head != 0)
		d1_head += *d1_head + 1;

	while (*d1_head != 0)
	{
		/* check whether we have a match at this point */
		d1_current = d1_head;
		d2_current = (uint8_t *) d2;
		is_matching = 0;

		/* compare length and value of all successive labels */
		while (*d1_current == *d2_current)
		{
			part_length = *d1_current;
			if (part_length == 0)
			{
				/* We have reached the top label, there is a match */
				ret = 1;
				break;
			}

			/* The label's lengths are matching, check the content */
			is_matching = 1;
			d1_current++;
			d2_current++;

			for (i = 0; i < part_length; i++)
			{
				c = d1_current[i];
				if (isupper(c))
					c = tolower(c);
				if (c != d2_current[i])
				{
					is_matching = 0;
					break;
				}
			}
			
			/* move the pointers to the next label */
			if (is_matching)
			{
				d1_current += part_length;
				d2_current += part_length;
			}
		}

		/* if no match found yet, move to the next label of d1 */
		if (is_matching)
			break;
		else
			d1_head += *d1_head + 1;
	}

	return ret;
}


getdns_return_t
_getdns_mdns_namespace_check(
	getdns_dns_req *dnsreq)
{
	getdns_return_t ret = GETDNS_RETURN_GENERIC_ERROR;

	/* Checking the prefixes defined in RFC 6762  */
	if (mdns_suffix_compare(dnsreq->name, mdns_suffix_dot_local) ||
		mdns_suffix_compare(dnsreq->name, mdns_suffix_254_169_in_addr_arpa) ||
		mdns_suffix_compare(dnsreq->name, mdns_suffix_8_e_f_ip6_arpa) ||
		mdns_suffix_compare(dnsreq->name, mdns_suffix_9_e_f_ip6_arpa) ||
		mdns_suffix_compare(dnsreq->name, mdns_suffix_a_e_f_ip6_arpa) ||
		mdns_suffix_compare(dnsreq->name, mdns_suffix_b_e_f_ip6_arpa))
		ret = GETDNS_RETURN_GOOD;

	return ret;
}

#endif /* HAVE_MDNS_SUPPORT */

#ifdef MDNS_UNIT_TEST

/*
 * Test adding data to the LRU Cache
 */

static BYTE mdns_exampleRRAM[] = {
	0, 0, /* Transaction ID = 0 */
	0x84, 0, /* Answer: QR=1 (80), AA=1 (04) */
	0, 0, /* QD Count = 0 */
	0, 1, /* AN Count = 1 */
	0, 0, /* NS Count = 0 */
	0, 0, /* AD Count = 0 */
	7, /* length of "example" name part */
	'e', 'x', 'a', 'm', 'p', 'l', 'e',
	5, /* length of "local" name part */
	'l', 'o', 'c', 'a', 'l',
	0, /* length of the root name part */
	0, 1, /* QTYPE = 1, A record */
	0, 1, /* QCLASS = 1, IN */
	0, 0, 0, 255, /* TTL: 255 sec */
	0, 4, /* length of RDATA */
	10, 0, 0, 1 /* Value of RDATA (some IPv4 address) */
};


uint8_t mdns_test_name[] = {
	7, /* length of "example" name part */
	't', 'e', 's', 't', 'i', 'n', 'g',
	5, /* length of "local" name part */
	'l', 'o', 'c', 'a', 'l',
	0, /* length of the root name part */
};

uint8_t mdns_test_first_address[4] = { 10, 0, 0, 1 };
uint8_t mdns_test_second_address[4] = { 10, 0, 0, 2 };
uint8_t mdns_test_third_address[4] = { 10, 0, 0, 3 };

int mdns_finalize_lru_test(struct getdns_context* context,
	uint8_t * name, int name_len, int record_type, int record_class,
	int expected_nb_records,
	uint8_t * buffer, size_t buffer_max, size_t* entry_length)
{
	int ret = 0;
	/* verify that the entry is there */
	struct lruhash_entry * entry =
		mdns_access_cached_entry_by_name(context, name, name_len, record_type, record_class);

	
	*entry_length = 0;

	if (entry == NULL)
	{
		if (expected_nb_records != 0)
			ret = -1;
	}
	else
	{
		int nbanswers = mdns_cache_nb_records_in_entry((uint8_t*)entry->data);
		if (nbanswers != expected_nb_records)
		{
			ret = -2;
		}
		
		if (buffer != NULL)
		{
			getdns_mdns_cached_record_header * header =
				(getdns_mdns_cached_record_header*)entry->data;
			size_t record_length = header->content_len - sizeof(getdns_mdns_cached_record_header);

			if (record_length > buffer_max)
			{
				ret = -3;
			}
			else
			{
				memcpy(buffer, ((uint8_t *)entry->data) + sizeof(getdns_mdns_cached_record_header),
					record_length);
				*entry_length = record_length;
			}
		}

		lock_rw_unlock(&entry->lock);
	}

	return ret;
}

int mdns_addition_test(struct getdns_context* context, 
	uint8_t * buffer, size_t buffer_max, size_t* entry_length)
{
	int ret = 
	mdns_propose_entry_to_cache(context, mdns_test_name, sizeof(mdns_test_name), 1, 1, 255,
		mdns_test_first_address, 4, NULL, _getdns_get_time_as_uintt64());

	if (ret == 0)
	{
		ret = mdns_finalize_lru_test(context, mdns_test_name, sizeof(mdns_test_name), 1, 1,
			1, buffer, buffer_max, entry_length);
	}

	return ret;
}

int mdns_addition_test2(struct getdns_context* context,
	uint8_t * buffer, size_t buffer_max, size_t* entry_length)
{
	int ret =
		mdns_propose_entry_to_cache(context, mdns_test_name, sizeof(mdns_test_name), 1, 1, 255,
			mdns_test_first_address, 4, NULL, _getdns_get_time_as_uintt64());

	if (ret == 0)
	{
		/* add a second entry, with a different value */
		ret =
			mdns_propose_entry_to_cache(context, mdns_test_name, sizeof(mdns_test_name), 1, 1, 255,
				mdns_test_second_address, 4, NULL, _getdns_get_time_as_uintt64());
	}

	if (ret == 0)
	{
		/* add a third entry, with a different value */
		ret =
			mdns_propose_entry_to_cache(context, mdns_test_name, sizeof(mdns_test_name), 1, 1, 255,
				mdns_test_third_address, 4, NULL, _getdns_get_time_as_uintt64());
	}

	if (ret == 0)
	{
		ret = mdns_finalize_lru_test(context, mdns_test_name, sizeof(mdns_test_name), 1, 1,
			3, buffer, buffer_max, entry_length);
	}

	return ret;
}

int mdns_deletion_test(struct getdns_context* context,
	uint8_t * buffer, size_t buffer_max, size_t* entry_length)
{
	/* insert data with TTL = 0 to trigger suppression */
	int ret =
		mdns_propose_entry_to_cache(context, mdns_test_name, sizeof(mdns_test_name), 1, 1, 0,
			mdns_test_second_address, 4, NULL, _getdns_get_time_as_uintt64());

	if (ret == 0)
	{
		ret = mdns_finalize_lru_test(context, mdns_test_name, sizeof(mdns_test_name), 1, 1,
			2, buffer, buffer_max, entry_length);
	}

	return ret;
}

int mdns_deletion_test2(struct getdns_context* context,
	uint8_t * buffer, size_t buffer_max, size_t* entry_length)
{
	/* insert data with TTL = 0 to trigger suppression */
	int ret =
		mdns_propose_entry_to_cache(context, mdns_test_name, sizeof(mdns_test_name), 1, 1, 0,
			mdns_test_first_address, 4, NULL, _getdns_get_time_as_uintt64());

	if (ret == 0)
	{
		ret =
			mdns_propose_entry_to_cache(context, mdns_test_name, sizeof(mdns_test_name), 1, 1, 0,
				mdns_test_third_address, 4, NULL, _getdns_get_time_as_uintt64());
	}

	if (ret == 0)
	{
		ret = mdns_finalize_lru_test(context, mdns_test_name, sizeof(mdns_test_name), 1, 1,
			0, buffer, buffer_max, entry_length);
	}

	return ret;
}


int mdns_test_prepare(struct getdns_context* context)
{
	return mdns_delayed_network_init(context);
}

#endif
