/**
 *
 * \file util-internal.c
 * @brief private library routines
 *
 * These routines are not intended to be used by applications calling into
 * the library.
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

#include <stdint.h>
#include <stdlib.h>
#include "config.h"
#include "getdns/getdns.h"
#include "dict.h"
#include "list.h"
#include "util-internal.h"
#include "types-internal.h"
#include "rr-dict.h"
#if defined(WIRE_DEBUG) && WIRE_DEBUG
#include "gldns/wire2str.h"
#endif
#include "gldns/str2wire.h"
#include "gldns/gbuffer.h"
#include "gldns/pkthdr.h"
#include "dnssec.h"
#include "convert.h"


getdns_return_t
getdns_dict_util_get_string(const getdns_dict *dict,
    const char *name, char **result)
{
	struct getdns_bindata *bindata = NULL;
	if (!result) {
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	*result = NULL;
	getdns_dict_get_bindata(dict, name, &bindata);
	if (!bindata) {
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	*result = (char *) bindata->data;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
_getdns_dict_to_sockaddr(struct getdns_dict * ns, struct sockaddr_storage * output)
{
	char *address_type = NULL;
	struct getdns_bindata *address_data = NULL;
	uint32_t port = 53;
	memset(output, 0, sizeof(struct sockaddr_storage));
	output->ss_family = AF_UNSPEC;

	uint32_t prt = 0;
	if (getdns_dict_get_int(ns, GETDNS_STR_PORT,
		&prt) == GETDNS_RETURN_GOOD) {
		port = prt;
	}

	getdns_dict_util_get_string(ns, GETDNS_STR_ADDRESS_TYPE,
	    &address_type);
	getdns_dict_get_bindata(ns, GETDNS_STR_ADDRESS_DATA, &address_data);
	if (!address_type || !address_data) {
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	if (strncmp(GETDNS_STR_IPV4, address_type,
		strlen(GETDNS_STR_IPV4)) == 0) {
		/* data is an in_addr_t */
		struct sockaddr_in *addr = (struct sockaddr_in *) output;
		addr->sin_family = AF_INET;
		addr->sin_port = htons((uint16_t) port);
		memcpy(&(addr->sin_addr), address_data->data,
		    address_data->size);
	} else {
		/* data is a v6 addr in host order */
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *) output;
		addr->sin6_family = AF_INET6;
		addr->sin6_port = htons((uint16_t) port);
		memcpy(&(addr->sin6_addr), address_data->data,
		    address_data->size);
	}
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
_getdns_sockaddr_to_dict(struct getdns_context *context, struct sockaddr_storage *address,
    struct getdns_dict ** output)
{
	if (!output || !address) {
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	struct getdns_bindata addr_data;
	*output = NULL;
	struct getdns_dict *result = getdns_dict_create_with_context(context);
	if (address->ss_family == AF_INET) {
		struct sockaddr_in *addr = (struct sockaddr_in *) address;
		getdns_dict_util_set_string(result, GETDNS_STR_ADDRESS_TYPE,
		    GETDNS_STR_IPV4);
		addr_data.size = sizeof(addr->sin_addr);
		addr_data.data = (uint8_t *) & (addr->sin_addr);
		getdns_dict_set_bindata(result, GETDNS_STR_ADDRESS_DATA,
		    &addr_data);
	} else if (address->ss_family == AF_INET6) {
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *) address;
		getdns_dict_util_set_string(result, GETDNS_STR_ADDRESS_TYPE,
		    GETDNS_STR_IPV6);
		addr_data.size = sizeof(addr->sin6_addr);
		addr_data.data = (uint8_t *) & (addr->sin6_addr);
		getdns_dict_set_bindata(result, GETDNS_STR_ADDRESS_DATA,
		    &addr_data);
	} else {
		// invalid
		getdns_dict_destroy(result);
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	*output = result;
	return GETDNS_RETURN_GOOD;
}

getdns_dict *
_getdns_rr_iter2rr_dict_canonical(
    struct mem_funcs *mf, _getdns_rr_iter *i, uint32_t *orig_ttl)
{
	getdns_dict *rr_dict, *rdata_dict;
	const uint8_t *bin_data;
	size_t bin_size, owner_len = 0, rdata_sz;
	uint32_t int_val = 0;
	enum wf_data_type { wf_int, wf_bindata, wf_special } val_type;
	_getdns_rdf_iter rdf_storage, *rdf;
	getdns_list *repeat_list = NULL;
	getdns_dict *repeat_dict = NULL;
	uint8_t ff_bytes[256];
	uint16_t rr_type;
	int canonicalize;
	gldns_buffer gbuf;
	getdns_bindata *bindata;
	uint8_t *data;

	assert(i);
	if (!(rr_dict = _getdns_dict_create_with_mf(mf)))
		return NULL;

	bin_data = _getdns_owner_if_or_as_decompressed(
	    i, ff_bytes, &bin_size);

	if (orig_ttl) {
		if (bin_data != ff_bytes)
			bin_data = memcpy(ff_bytes, bin_data, bin_size);
		_dname_canonicalize2(ff_bytes);
		owner_len = bin_size;
	}
	/* question */
	if (_getdns_rr_iter_section(i) == SECTION_QUESTION) {

		if (getdns_dict_set_int(rr_dict, "qtype",
		    (uint32_t) gldns_read_uint16(i->rr_type)) ||

		    getdns_dict_set_int(rr_dict, "qclass",
		    (uint32_t) gldns_read_uint16(i->rr_type + 2)) ||

		    _getdns_dict_set_const_bindata(
		    rr_dict, "qname", bin_size, bin_data)) {

			goto error;
		}
		return rr_dict;
	}
	if (getdns_dict_set_int(rr_dict, "type",
	    (uint32_t)(rr_type = gldns_read_uint16(i->rr_type)))) {

		goto error;
	}
	canonicalize = orig_ttl && _dnssec_rdata_to_canonicalize(rr_type)
	    && (i->rr_type + 12 <= i->nxt) /* To estimate rdata size */;

	if (rr_type == GETDNS_RRTYPE_OPT) {
		int_val = gldns_read_uint16(i->rr_type + 6);

		if (getdns_dict_set_int(rr_dict, "udp_payload_size",
		    (uint32_t) gldns_read_uint16(i->rr_type + 2)) ||

		    getdns_dict_set_int(rr_dict, "extended_rcode",
		    (uint32_t) *(i->rr_type + 4)) ||

		    getdns_dict_set_int(rr_dict, "version",
		    (uint32_t) *(i->rr_type + 5)) ||
		    
		    getdns_dict_set_int(rr_dict, "do",
		    (uint32_t) ((int_val & 0x8000) >> 15)) ||

		    getdns_dict_set_int(rr_dict, "z",
		    (uint32_t) (int_val & 0x7FF))) {

			goto error;
		}
	} else if (getdns_dict_set_int(rr_dict, "class",
	    (uint32_t) gldns_read_uint16(i->rr_type + 2)) ||

	    getdns_dict_set_int(rr_dict, "ttl",
	    (  orig_ttl && rr_type != GETDNS_RRTYPE_RRSIG
	    ? *orig_ttl : (uint32_t) gldns_read_uint32(i->rr_type + 4))) ||

	    _getdns_dict_set_const_bindata(
	    rr_dict, "name", bin_size, bin_data)) {

		goto error;
	}
	if (!(rdata_dict = _getdns_dict_create_with_mf(mf)))
		return NULL;

	if (i->rr_type + 10 <= i->nxt && !canonicalize) {
		bin_size = i->nxt - (i->rr_type + 10);
		bin_data = i->rr_type + 10;
		if (_getdns_dict_set_const_bindata(
		    rdata_dict, "rdata_raw", bin_size, bin_data))
			goto rdata_error;
	}
	if (canonicalize)
		rdata_sz = 0;

	for ( rdf = _getdns_rdf_iter_init(&rdf_storage, i)
	    ; rdf; rdf = _getdns_rdf_iter_next(rdf)) {
		if (canonicalize && !(rdf->rdd_pos->type & GETDNS_RDF_DNAME)) {
			rdata_sz += rdf->nxt - rdf->pos;
		}
		if (rdf->rdd_pos->type & GETDNS_RDF_INTEGER) {
			val_type = wf_int;
			switch (rdf->rdd_pos->type & GETDNS_RDF_FIXEDSZ) {
			case 1:	int_val = *rdf->pos;
				break;
			case 2:	int_val = gldns_read_uint16(rdf->pos);
				break; 
			case 4:	int_val = gldns_read_uint32(rdf->pos);
				break;
			default:
				goto rdata_error;
			}
		} else if (rdf->rdd_pos->type & GETDNS_RDF_DNAME) {
			val_type = wf_bindata;

			bin_data = _getdns_rdf_if_or_as_decompressed(
			    rdf, ff_bytes, &bin_size);

			if (canonicalize) {
				if (bin_data != ff_bytes)
					bin_data = memcpy(ff_bytes, bin_data, bin_size);
				_dname_canonicalize2(ff_bytes);
				rdata_sz += bin_size;
			}
		} else if (rdf->rdd_pos->type & GETDNS_RDF_BINDATA) {
			val_type = wf_bindata;
			if (rdf->rdd_pos->type & GETDNS_RDF_FIXEDSZ) {
				bin_size = rdf->rdd_pos->type
				    & GETDNS_RDF_FIXEDSZ;
				bin_data = rdf->pos;

			} else switch(rdf->rdd_pos->type & GETDNS_RDF_LEN_VAL){
			case 0x100:
				bin_size = *rdf->pos;
				bin_data = rdf->pos + 1;
				break;
			case 0x200:
				bin_size = gldns_read_uint16(rdf->pos);
				bin_data = rdf->pos + 2;
				break;
			default:
				bin_size = rdf->nxt - rdf->pos;
				bin_data = rdf->pos;
				break;
			}
		} else if (rdf->rdd_pos->type == GETDNS_RDF_SPECIAL)
			val_type = wf_special;
		else
			assert(((val_type = wf_int), 0));

		if (! rdf->rdd_repeat) {
			switch (val_type) {
			case wf_int:
				if (getdns_dict_set_int(rdata_dict,
				    rdf->rdd_pos->name, int_val))
					goto rdata_error;
				break;
			case wf_bindata:
				if (_getdns_dict_set_const_bindata(rdata_dict,
				    rdf->rdd_pos->name, bin_size, bin_data))
					goto rdata_error;
				break;
			case wf_special:
				if (rdf->rdd_pos->special->wire2dict(
				    rdata_dict, rdf->pos))
					goto rdata_error;
			default:
				break;
			}
			continue;
		}
		if (rdf->rdd_pos == rdf->rdd_repeat) {
			/* list with rdf values */

			if (! repeat_list && !(repeat_list =
			    _getdns_list_create_with_mf(mf)))

				goto rdata_error;
			
			switch (val_type) {
			case wf_int:
				if (_getdns_list_append_int(repeat_list,
				    int_val))
					goto rdata_error;
				break;
			case wf_bindata:
				if (_getdns_list_append_const_bindata(
				    repeat_list, bin_size, bin_data))
					goto rdata_error;
				break;

			/* Repetitive special types do not exist (yet)
			 *
			 * LCOV_EXCL_START
			 */
			case wf_special:
				/* Repetitive special types
				 * must have this function
				 */
				assert(rdf->rdd_pos->special->wire2list);

				if (rdf->rdd_pos->special->wire2list(
				    repeat_list, rdf->pos))
					goto rdata_error;
			/* LCOV_EXCL_STOP */

			default:
				break;
			}
			continue;
		}
		if (rdf->rdd_pos == rdf->rdd_repeat + 1) {

			if (repeat_dict) {
				if (! repeat_list && !(repeat_list =
				    _getdns_list_create_with_mf(mf)))
					goto rdata_error;
	
				if (_getdns_list_append_this_dict(
				    repeat_list, repeat_dict))
					goto rdata_error;

				repeat_dict = NULL;
			}
			if (!(repeat_dict =
			    _getdns_dict_create_with_mf(mf)))
				goto rdata_error;
		}
		assert(repeat_dict);
		switch (val_type) {
		case wf_int:
			if (getdns_dict_set_int(repeat_dict,
			    rdf->rdd_pos->name, int_val))
				goto rdata_error;
			break;
		case wf_bindata:
			if (_getdns_dict_set_const_bindata(repeat_dict,
			    rdf->rdd_pos->name, bin_size, bin_data))
				goto rdata_error;
			break;
		case wf_special:
			if (rdf->rdd_pos->special->wire2dict(
			    repeat_dict, rdf->pos))
				goto rdata_error;
		default:
			break;
		}
	}
	if (repeat_dict) {
		if (!repeat_list && !(repeat_list =
		    _getdns_list_create_with_mf(mf)))
			goto rdata_error;
		if (_getdns_list_append_this_dict(repeat_list, repeat_dict))
			goto rdata_error;
		repeat_dict = NULL;
	}
	if (repeat_list) {
		if (_getdns_dict_set_this_list(rdata_dict,
		    rdf_storage.rdd_repeat->name, repeat_list))
			goto rdata_error;
		repeat_list = NULL;
	}
	if (_getdns_dict_set_this_dict(rr_dict, "rdata", rdata_dict))
		goto rdata_error;

	if (canonicalize && rdata_sz) {
		if (!(data = GETDNS_XMALLOC(
		    *mf, uint8_t, owner_len + 10 + rdata_sz)))
			return rr_dict;

		gldns_buffer_init_frm_data(&gbuf, data, owner_len+10+rdata_sz);
		if (_getdns_rr_dict2wire(rr_dict, &gbuf) ||
		    gldns_buffer_position(&gbuf) != owner_len + 10 + rdata_sz ||
		    !(bindata = GETDNS_MALLOC(*mf, struct getdns_bindata))) {
			GETDNS_FREE(*mf, data);
			return rr_dict;
		}
		bindata->size = rdata_sz;
		bindata->data = memmove(data, data + owner_len + 10, rdata_sz);
		(void) _getdns_dict_set_this_bindata(rr_dict,
		    "/rdata/rdata_raw", bindata);
	}
	return rr_dict;

rdata_error:
	getdns_list_destroy(repeat_list);
	getdns_dict_destroy(repeat_dict);
	getdns_dict_destroy(rdata_dict);
error:
	getdns_dict_destroy(rr_dict);
	return NULL;
}

getdns_dict *
_getdns_rr_iter2rr_dict(struct mem_funcs *mf, _getdns_rr_iter *i)
{
	return _getdns_rr_iter2rr_dict_canonical(mf, i, NULL);
}


static inline getdns_dict *
set_dict(getdns_dict **var, getdns_dict *value)
{
	if (*var)
		getdns_dict_destroy(*var);
	return *var = value;
}

static inline int has_all_numeric_label(const uint8_t *dname)
{
	size_t i;

	while (*dname && !(*dname & 0xc0)) {
		for (i = 1; i <= *dname; i++) {
			if (!isdigit(dname[i]))
				break;
		}
		if (i > *dname)
			return 1;
		dname += *dname + 1;
	}
	return 0;
}

typedef struct _srv_rr {
	_getdns_rr_iter i;
	unsigned        running_sum;
} _srv_rr;

typedef struct _srvs {
	size_t  capacity;
	size_t  count;
	_srv_rr *rrs;
} _srvs;

static int _grow_srvs(struct mem_funcs *mf, _srvs *srvs)
{
	_srv_rr *new_rrs;

	if (!(new_rrs = GETDNS_XREALLOC(
	    *mf, srvs->rrs, _srv_rr, srvs->capacity * 2)))
		return 0; /* Memory error */

	srvs->capacity *= 2;
	srvs->rrs = new_rrs;
	return 1; /* Success */
}

#define SET_WIRE_INT(X,Y) if (getdns_dict_set_int(header, #X , (int) \
                              GLDNS_ ## Y ## _WIRE(req->response))) goto error
#define SET_WIRE_BIT(X,Y) if (getdns_dict_set_int(header, #X , \
                              GLDNS_ ## Y ## _WIRE(req->response) ? 1 : 0)) goto error
#define SET_WIRE_CNT(X,Y) if (getdns_dict_set_int(header, #X , (int) \
                              GLDNS_ ## Y (req->response))) goto error 

static getdns_dict *
_getdns_create_reply_dict(getdns_context *context, getdns_network_req *req,
    getdns_list *just_addrs, int *rrsigs_in_answer, _srvs *srvs)
{
	/* turn a packet into this glorious structure
	 *
	 * {     # This is the first reply
	 * "header": { "id": 23456, "qr": 1, "opcode": 0, ... },
	 * "question": { "qname": <bindata for "www.example.com">, "qtype": 1, "qclass": 1 },
	 * "answer":
	 * [
	 * {
	 * "name": <bindata for "www.example.com">,
	 * "type": 1,
	 * "class": 1,
	 * "ttl": 33000,
	 * "rdata":
	 * {
	 * "ipv4_address": <bindata of 0x0a0b0c01>
	 * "rdata_raw": <bindata of 0x0a0b0c01>
	 * }
	 * }
	 * ],
	 * "authority":
	 * [
	 * {
	 * "name": <bindata for "ns1.example.com">,
	 * "type": 1,
	 * "class": 1,
	 * "ttl": 600,
	 * "rdata":
	 * {
	 * "ipv4_address": <bindata of 0x65439876>
	 * "rdata_raw": <bindata of 0x65439876>
	 * }
	 * }
	 * ]
	 * "additional": [],
	 * "canonical_name": <bindata for "www.example.com">,
	 * "answer_type": GETDNS_NAMETYPE_DNS
	 * }
	 *
	 */
	getdns_return_t r = GETDNS_RETURN_GOOD;
	getdns_dict *result = getdns_dict_create_with_context(context);
	getdns_dict *question = NULL;
	getdns_list *sections[16] = { NULL, NULL
	                            , getdns_list_create_with_context(context)
	                            , NULL
	                            , getdns_list_create_with_context(context)
				    , NULL, NULL, NULL
	                            , getdns_list_create_with_context(context)
				    , NULL, NULL, NULL, NULL, NULL, NULL, NULL
	                            };
	getdns_dict *rr_dict = NULL;
	_getdns_rr_iter rr_iter_storage, *rr_iter;
	_getdns_rdf_iter rdf_iter_storage, *rdf_iter;
	size_t bin_size;
	const uint8_t *bin_data;
	_getdns_section section;
	uint8_t owner_name_space[256], query_name_space[256];
	const uint8_t *owner_name, *query_name;
	size_t owner_name_len = sizeof(owner_name_space),
	       query_name_len = sizeof(query_name_space);
	int all_numeric_label;
	uint16_t rr_type;
	getdns_dict *header = NULL;
	getdns_list *bad_dns = NULL;
	_getdns_rrset_spc answer_spc;
	_getdns_rrset *answer;

	if (!result)
		goto error;

	if (!(header = getdns_dict_create_with_context(context)))
		goto error;

	SET_WIRE_INT(id, ID);
	SET_WIRE_BIT(qr, QR);
	SET_WIRE_BIT(aa, AA);
	SET_WIRE_BIT(tc, TC);
	SET_WIRE_BIT(rd, RD);
	SET_WIRE_BIT(cd, CD);
	SET_WIRE_BIT(ra, RA);
	SET_WIRE_BIT(ad, AD);
	SET_WIRE_INT(opcode, OPCODE);
	SET_WIRE_INT(rcode, RCODE);
	SET_WIRE_BIT(z, Z);

	SET_WIRE_CNT(qdcount, QDCOUNT);
	SET_WIRE_CNT(ancount, ANCOUNT);
	SET_WIRE_CNT(nscount, NSCOUNT);
	SET_WIRE_CNT(arcount, ARCOUNT);

	/* header */
    	if ((r = _getdns_dict_set_this_dict(result, "header", header)))
		goto error;
	header = NULL;

	if (req->query &&
	    (rr_iter = _getdns_rr_iter_init(&rr_iter_storage, req->query
	                                   , req->response  - req->query)))

		query_name = _getdns_owner_if_or_as_decompressed(
		    rr_iter, query_name_space, &query_name_len);
	else
		query_name = NULL;

	for ( rr_iter = _getdns_rr_iter_init(&rr_iter_storage
	                                        , req->response
	                                        , req->response_len)
	    ; rr_iter
	    ; rr_iter = _getdns_rr_iter_next(rr_iter)) {

		if (!set_dict(&rr_dict,
		    _getdns_rr_iter2rr_dict(&context->mf, rr_iter)))
			continue;

		section = _getdns_rr_iter_section(rr_iter);
		if (section == SECTION_QUESTION) {
			if (!query_name)
				query_name
				    = _getdns_owner_if_or_as_decompressed(
				    rr_iter, query_name_space, &query_name_len);

			if (_getdns_dict_set_this_dict(result, "question", rr_dict))
				goto error;
			else 	rr_dict = NULL;
			continue;
		}
		rr_type = gldns_read_uint16(rr_iter->rr_type);
		if (section > SECTION_QUESTION &&
		    rr_type == GETDNS_RRTYPE_RRSIG && rrsigs_in_answer)
			*rrsigs_in_answer = 1;

		if (section != SECTION_ANSWER) {
			if (_getdns_list_append_this_dict(
			    sections[section], rr_dict))
				goto error;
			else	rr_dict = NULL;
			continue;
		}
		if (req->follow_redirects == GETDNS_REDIRECTS_DO_NOT_FOLLOW) {
			owner_name_len = sizeof(owner_name_space);
			owner_name = _getdns_owner_if_or_as_decompressed(
			    rr_iter, owner_name_space, &owner_name_len);

			if (!_getdns_dname_equal(query_name, owner_name))
				continue;
		}
		if (_getdns_list_append_this_dict(
		    sections[SECTION_ANSWER], rr_dict))
			goto error;
		else	rr_dict = NULL;

		if (srvs->capacity && rr_type == GETDNS_RRTYPE_SRV) {
			if (srvs->count >= srvs->capacity &&
			    !_grow_srvs(&context->mf, srvs))
				goto error;

			srvs->rrs[srvs->count++].i = *rr_iter;
			continue;
		}
		if (rr_type != GETDNS_RRTYPE_A && rr_type != GETDNS_RRTYPE_AAAA)
			continue;

		if (!just_addrs)
			continue;

		if (!(rdf_iter = _getdns_rdf_iter_init(
		     &rdf_iter_storage, rr_iter)))
			continue;

		bin_size = rdf_iter->nxt - rdf_iter->pos;
		bin_data = rdf_iter->pos;
		if (!set_dict(&rr_dict, getdns_dict_create_with_context(context)) ||

		    getdns_dict_util_set_string(rr_dict, "address_type",
			    rr_type == GETDNS_RRTYPE_A ? "IPv4" : "IPv6" ) ||

		    _getdns_dict_set_const_bindata(
		    rr_dict, "address_data", bin_size, bin_data) ||

		    _getdns_list_append_this_dict(just_addrs, rr_dict)) {

			goto error;
		}
		rr_dict = NULL;
	}
	if (!_getdns_dict_set_this_list(result, "answer",
	    sections[SECTION_ANSWER]))
		sections[SECTION_ANSWER] = NULL;
	else	goto error;

	if (!_getdns_dict_set_this_list(result, "authority",
	    sections[SECTION_AUTHORITY]))
		sections[SECTION_AUTHORITY] = NULL;
	else	goto error;

	if (!_getdns_dict_set_this_list(result, "additional",
	    sections[SECTION_ADDITIONAL]))
		sections[SECTION_ADDITIONAL] = NULL;
	else	goto error;

	/* other stuff
	 * Note that spec doesn't explicitly mention these.
	 * They are only showcased in the response dict example */
	if (getdns_dict_set_int(result, "answer_type", GETDNS_NAMETYPE_DNS))
		goto error;
	
	answer = _getdns_rrset_answer(&answer_spc, req->response
	                                         , req->response_len);

	if (answer_spc.rrset.name &&
	    _getdns_dict_set_const_bindata(result, "canonical_name"
	                                         , answer_spc.name_len
	                                         , answer_spc.rrset.name))
		goto error;

	if (!req->upstream)
		;
	else if (req->upstream->addr.ss_family == AF_INET) {
		struct sockaddr_in *addr = (struct sockaddr_in *) &req->upstream->addr;
		if (_getdns_dict_set_const_bindata(result,
		    "answer_ipv4_address", sizeof(addr->sin_addr)
		                         , (uint8_t *) & (addr->sin_addr)))
			goto error;
	}
	else if (req->upstream->addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &req->upstream->addr;
		if (_getdns_dict_set_const_bindata(result,
		    "answer_ipv6_address", sizeof(addr->sin6_addr)
		                         , (uint8_t *) & (addr->sin6_addr)))
			goto error;
	}

	if (!req->owner->add_warning_for_bad_dns)
		goto success;

	if (!(bad_dns = getdns_list_create_with_context(context)))
		goto error;

	if (   !answer
	    && req->request_type != GETDNS_RRTYPE_CNAME
	    && query_name
	    && answer_spc.rrset.name
	    && !_getdns_dname_equal(query_name, answer_spc.rrset.name)
	    && _getdns_list_append_int(bad_dns
		    , GETDNS_BAD_DNS_CNAME_RETURNED_FOR_OTHER_TYPE))
		goto error;

	all_numeric_label = 0;
	for ( rr_iter = _getdns_rr_iter_init(&rr_iter_storage
					    , req->response
					    , req->response_len)
	    ; rr_iter && !all_numeric_label
	    ; rr_iter = _getdns_rr_iter_next(rr_iter)) {

		owner_name = _getdns_owner_if_or_as_decompressed(
		    rr_iter, owner_name_space, &owner_name_len);

		if (has_all_numeric_label(owner_name)) {
			all_numeric_label = 1;
			break;
		}
		if (_getdns_rr_iter_section(rr_iter) ==
		    SECTION_QUESTION)
			continue;

		for ( rdf_iter = _getdns_rdf_iter_init(&rdf_iter_storage, rr_iter)
		    ; rdf_iter; rdf_iter = _getdns_rdf_iter_next(rdf_iter)) {
		
			if (!(rdf_iter->rdd_pos->type & GETDNS_RDF_DNAME))
				continue;

			owner_name = _getdns_rdf_if_or_as_decompressed(
			    rdf_iter, owner_name_space, &owner_name_len);

			if (has_all_numeric_label(owner_name)) {
				all_numeric_label = 1;
				break;
			}
		}
	}
	if (all_numeric_label &&
	    _getdns_list_append_int(bad_dns, GETDNS_BAD_DNS_ALL_NUMERIC_LABEL))
		goto error;

	if (_getdns_dict_set_this_list(result, "bad_dns", bad_dns))
		goto error;
	else	bad_dns = NULL;

	goto success;
error:
	getdns_dict_destroy(result);
	result = NULL;
success:
	getdns_dict_destroy(header);
	getdns_dict_destroy(rr_dict);
	getdns_list_destroy(sections[SECTION_ADDITIONAL]);
	getdns_list_destroy(sections[SECTION_AUTHORITY]);
	getdns_list_destroy(sections[SECTION_ANSWER]);
	getdns_dict_destroy(question);
	getdns_list_destroy(bad_dns);
	return result;
}

static getdns_dict *
_getdns_create_call_reporting_dict(
    getdns_context *context, getdns_network_req *netreq)
{
	getdns_bindata  qname;
	getdns_dict    *netreq_debug;
	getdns_dict    *address_debug = NULL;
	getdns_dict    *query_dict = NULL;
	const uint8_t  *wire;
	size_t          wire_len;

	assert(netreq);

	/* It is the responsibility of the caller to free this */
	if (!(netreq_debug = getdns_dict_create_with_context(context)))
		return NULL;

	qname.data = netreq->owner->name;
	qname.size = netreq->owner->name_len;

	if (getdns_dict_set_bindata(netreq_debug, "query_name", &qname) ||
	    getdns_dict_set_int( netreq_debug, "query_type"
	                       , netreq->request_type ) ||

	    /* Safe, because uint32_t facilitates RRT's of almost 50 days*/
	    getdns_dict_set_int(netreq_debug, "run_time/ms",
		    (uint32_t)(( netreq->debug_end_time
	                       - netreq->debug_start_time)/1000))) {

		getdns_dict_destroy(netreq_debug);
		return NULL;

	} else if (!netreq->upstream) {
		if (getdns_dict_set_int( netreq_debug, "resolution_type", GETDNS_RESOLUTION_RECURSING)) {
			getdns_dict_destroy(netreq_debug);
			return NULL;
		}
		/* Nothing more for full recursion */
		return netreq_debug;
	}

	if (getdns_dict_set_int( netreq_debug, "resolution_type", GETDNS_RESOLUTION_STUB)) {
		getdns_dict_destroy(netreq_debug);
		return NULL;
	}
	/* Stub resolver debug data */
	wire = netreq->query;
	wire_len = netreq->response - netreq->query;
	if (!wire)
	       ; /* pass */

	else if(_getdns_wire2msg_dict_scan(
	    &netreq_debug->mf, &wire, &wire_len, &query_dict)) {

		getdns_dict_destroy(netreq_debug);
		return NULL;

	} else if (_getdns_dict_set_this_dict(
	    netreq_debug, "query", query_dict)) {

		getdns_dict_destroy(netreq_debug);
		return NULL;
	}
	_getdns_sockaddr_to_dict(
	    context, &netreq->upstream->addr, &address_debug);

	if (_getdns_dict_set_this_dict(netreq_debug,"query_to",address_debug)){
		getdns_dict_destroy(address_debug);
		return NULL;
	}
	getdns_transport_list_t transport = netreq->upstream->transport;
	/* Same upstream is used for UDP and TCP, so netreq keeps track of what
	   was actually used for the last successful query.*/
	if (transport == GETDNS_TRANSPORT_TCP && netreq->debug_udp == 1) {
		transport = GETDNS_TRANSPORT_UDP;
		if (getdns_dict_set_int( netreq_debug, "udp_responses_for_this_upstream",
		                         netreq->upstream->udp_responses)) {
			getdns_dict_destroy(netreq_debug);
			return NULL;
		}
		if (getdns_dict_set_int( netreq_debug, "udp_timeouts_for_this_upstream",
		                         netreq->upstream->udp_timeouts)) {
			getdns_dict_destroy(netreq_debug);
			return NULL;
		}
	}
	if (getdns_dict_set_int( netreq_debug, "transport", transport)) {
		getdns_dict_destroy(netreq_debug);
		return NULL;
	}
	if (transport != GETDNS_TRANSPORT_UDP) {
		/* Report the idle timeout actually used on the connection. Must trim,
		maximum used in practice is 6553500ms, but this is stored in a uint64_t.*/
		if (netreq->upstream->keepalive_timeout > UINT32_MAX) {
			if (getdns_dict_set_int( netreq_debug, "idle timeout in ms (overflow)", UINT32_MAX)) {
				getdns_dict_destroy(netreq_debug);
				return NULL;
			}
		} else{
			uint32_t idle_timeout = (uint32_t) netreq->upstream->keepalive_timeout;
			if (getdns_dict_set_int( netreq_debug, "idle timeout in ms", idle_timeout)) {
				getdns_dict_destroy(netreq_debug);
				return NULL;
			}
		}
		if (getdns_dict_set_int( netreq_debug, "server_keepalive_received", netreq->upstream->server_keepalive_received)) {
			getdns_dict_destroy(netreq_debug);
			return NULL;
		}
		/* The running totals are only updated when a connection is closed.
		   Since it is open as we have just used it, calcualte the value on the fly */
		if (getdns_dict_set_int( netreq_debug, "responses_on_this_connection",
		                         netreq->upstream->responses_received)) {
			getdns_dict_destroy(netreq_debug);
			return NULL;
		}
		if (getdns_dict_set_int( netreq_debug, "timeouts_on_this_connection",
		                         netreq->upstream->responses_timeouts)) {
			getdns_dict_destroy(netreq_debug);
			return NULL;
		}
		if (getdns_dict_set_int( netreq_debug, "responses_for_this_upstream",
		                         netreq->upstream->responses_received + 
		                         netreq->upstream->total_responses)) {
			getdns_dict_destroy(netreq_debug);
			return NULL;
		}
		if (getdns_dict_set_int( netreq_debug, "timeouts_for_this_upstream",
		                         netreq->upstream->responses_timeouts +
		                         netreq->upstream->total_timeouts)) {
			getdns_dict_destroy(netreq_debug);
			return NULL;
		}
	}

	if (netreq->upstream->transport != GETDNS_TRANSPORT_TLS)
		return netreq_debug;
	
	/* Only include the auth status if TLS was used */
	if (getdns_dict_util_set_string(netreq_debug, "tls_auth_status",
	    _getdns_auth_str(netreq->debug_tls_auth_status))){

		getdns_dict_destroy(netreq_debug);
		return NULL;
	}
	if (getdns_dict_util_set_string(netreq_debug, "tls_version",
	    netreq->debug_tls_version)){

		getdns_dict_destroy(netreq_debug);
		return NULL;
	}
	if (getdns_dict_set_bindata(netreq_debug, "tls_peer_cert",
	    &netreq->debug_tls_peer_cert)) {

		getdns_dict_destroy(netreq_debug);
		return NULL;
	}
	netreq->debug_tls_peer_cert.size = 0;
	GETDNS_FREE(context->my_mf, netreq->debug_tls_peer_cert.data);
	netreq->debug_tls_peer_cert.data = NULL;
	return netreq_debug;
}

static inline int _srv_prio(_getdns_rr_iter *x)
{ return x->nxt < x->rr_type+12
       ? 65536 : (int)gldns_read_uint16(x->rr_type+10); }

static inline int _srv_weight(_getdns_rr_iter *x)
{ return x->nxt < x->rr_type+14 ? 0 : (int)gldns_read_uint16(x->rr_type+12); }


static int _srv_cmp(const void *a, const void *b)
{
	return _srv_prio((_getdns_rr_iter *)a)
	     - _srv_prio((_getdns_rr_iter *)b);
}

static void _rfc2782_sort(_srv_rr *start, _srv_rr *end)
{
	uint32_t running_sum, n;
	_srv_rr *i, *j, swap;

	/* First move all SRVs with weight 0 to the beginning of the list */

	for (i = start; i < end && _srv_weight(&i->i) == 0; i++)
		; /* pass */

	for (j = i + 1; j < end; j++) {
		if (_srv_weight(&j->i) == 0) {
			swap = *i;
			*i = *j;
			*j = swap;
			i++;
		}
	}
	/* Now all SRVs are at the beginning, calculate running_sum */
	running_sum = 0;
	for (i = start; i < end; i++) {
		running_sum += _srv_weight(&i->i);
		i->running_sum = running_sum;
	}
	n = arc4random_uniform(running_sum);
	for (i = start; i < end; i++) {
		if (i->running_sum >= n)
			break;
	}
	if (i > start && i < end) {
		swap = *start;
		*start = *i;
		*i = swap;
		_rfc2782_sort(start + 1, end);
	}
}

static getdns_list *
_create_srv_addrs(getdns_context *context, _srvs *srvs)
{
	getdns_list *srv_addrs;
	size_t i, j;

	qsort(srvs->rrs, srvs->count, sizeof(_srv_rr), _srv_cmp);
	/* The SRVs are now sorted by priority (lowest first).  Now determine
	 * server selection order for the SRVs with the same priority with
	 * the algorithm described in RFC2782:
	 * 
	 *   To select a target to be contacted next, arrange all SRV RRs
	 *   (that have not been ordered yet) in any order, except that all
	 *   those with weight 0 are placed at the beginning of the list.
	 * 
	 *   Compute the sum of the weights of those RRs, and with each RR
	 *   associate the running sum in the selected order. Then choose a
	 *   uniform random number between 0 and the sum computed
	 *   (inclusive), and select the RR whose running sum value is the
	 *   first in the selected order which is greater than or equal to
	 *   the random number selected. The target host specified in the
	 *   selected SRV RR is the next one to be contacted by the client.
	 *   Remove this SRV RR from the set of the unordered SRV RRs and
	 *   apply the described algorithm to the unordered SRV RRs to select
	 *   the next target host.  Continue the ordering process until there
	 *   are no unordered SRV RRs.  This process is repeated for each
	 *   Priority.
	 */
	for (i = 0; i < srvs->count; i = j) {
		/* Determine range of SRVs with same priority */
		for ( j = i + 1
		    ; j < srvs->count && _srv_prio(&srvs->rrs[i].i)
		                      == _srv_prio(&srvs->rrs[j].i)
		    ; j++)
			; /* pass */

		if (j == i + 1)
			continue;

		/* SRVs with same prio range from i till j (exclusive). */
		_rfc2782_sort(srvs->rrs + i, srvs->rrs + j);
	}
	if (!(srv_addrs = getdns_list_create_with_context(context)))
		return NULL;

	for (i = 0; i < srvs->count; i++) {
		_getdns_rr_iter *rr = &srvs->rrs[i].i;
		getdns_dict *d;
		_getdns_rdf_iter rdf_storage, *rdf;

		const uint8_t *dname;
		uint8_t dname_spc[256];
		size_t dname_sz = sizeof(dname_spc);

		_getdns_rrset a;
		_getdns_rrtype_iter a_rr_spc, *a_rr;
		int addresses_found = 0;

		if (   !(d = getdns_dict_create_with_context(context))
		    || !(rdf = _getdns_rdf_iter_init_at(&rdf_storage, rr, 2))
		    || !(rdf->rdd_pos->type & GETDNS_RDF_INTEGER)
		    ||  (rdf->rdd_pos->type & GETDNS_RDF_FIXEDSZ) != 2
		    || getdns_dict_set_int(d, "port", (uint32_t)
		                           gldns_read_uint16(rdf->pos))

		    || !(rdf = _getdns_rdf_iter_init_at(&rdf_storage, rr, 3))
		    || !(rdf->rdd_pos->type & GETDNS_RDF_DNAME)
		    || !(dname = _getdns_rdf_if_or_as_decompressed(
				    rdf, dname_spc, &dname_sz))
		    || _getdns_dict_set_const_bindata(
			    d, "domain_name", dname_sz, dname)) {

			/* error */
			getdns_dict_destroy(d);
			continue;
		}

		a.name = dname;
		a.rr_class = rr_iter_class(rr);
		a.rr_type = GETDNS_RRTYPE_AAAA;
		a.pkt = rr->pkt;
		a.pkt_len = rr->pkt_end - rr->pkt;
		a.sections = SECTION_ADDITIONAL;

		for ( a_rr = _getdns_rrtype_iter_init(&a_rr_spc, &a)
		    ; a_rr ; a_rr = _getdns_rrtype_iter_next(a_rr)) {

			if (   a_rr->rr_i.nxt - (a_rr->rr_i.rr_type + 10) == 16
			    && !getdns_dict_util_set_string(
					d, "address_type", "IPv6")
			    && !_getdns_dict_set_const_bindata(
					d, "address_data", 16,
					a_rr->rr_i.rr_type + 10)
			    && !_getdns_list_append_dict(srv_addrs, d))
				addresses_found++;
		}
		a.rr_type = GETDNS_RRTYPE_A;

		for ( a_rr = _getdns_rrtype_iter_init(&a_rr_spc, &a)
		    ; a_rr ; a_rr = _getdns_rrtype_iter_next(a_rr)) {

			if (   a_rr->rr_i.nxt - (a_rr->rr_i.rr_type + 10) ==4 
			    && !getdns_dict_util_set_string(
					d, "address_type", "IPv4")
			    && !_getdns_dict_set_const_bindata(
					d, "address_data", 4,
					a_rr->rr_i.rr_type + 10)
			    && !_getdns_list_append_dict(srv_addrs, d))
				addresses_found++;
		}
		if (   addresses_found
		    || _getdns_list_append_this_dict(srv_addrs, d))
			getdns_dict_destroy(d);
	}
	return srv_addrs;
}

getdns_dict *
_getdns_create_getdns_response(getdns_dns_req *completed_request)
{
	getdns_dict *result;
	getdns_list *just_addrs = NULL;
	getdns_list *srv_addrs = NULL;
	getdns_list *replies_full;
	getdns_list *replies_tree;
	getdns_list *call_reporting = NULL;
	getdns_network_req *netreq, **netreq_p;
	int rrsigs_in_answer = 0;
	getdns_dict *reply;
	getdns_bindata *canonical_name = NULL;
	int nreplies = 0, nanswers = 0;
	int nsecure = 0, ninsecure = 0, nindeterminate = 0, nbogus = 0;
	getdns_dict   *netreq_debug;
	_srvs srvs = { 0, 0, NULL };
	_getdns_rrset_spc answer_spc;
	getdns_bindata *answer_ipv4_address = NULL;
	getdns_bindata *answer_ipv6_address = NULL;

	/* info (bools) about dns_req */
	int dnssec_return_status;
	getdns_context *context;

	assert(completed_request);
	
	context = completed_request->context;
	if (!(result = getdns_dict_create_with_context(context)))
		return NULL;

	dnssec_return_status = completed_request->dnssec ||
	                       completed_request->dnssec_return_status ||
	                       completed_request->dnssec_return_only_secure ||
	                       completed_request->dnssec_return_all_statuses
#ifdef DNSSEC_ROADBLOCK_AVOIDANCE
	                    || completed_request->dnssec_roadblock_avoidance
#endif
	                       ;

	if (completed_request->netreqs[0]->request_type == GETDNS_RRTYPE_A ||
	    completed_request->netreqs[0]->request_type == GETDNS_RRTYPE_AAAA)
		just_addrs = getdns_list_create_with_context(
		    completed_request->context);

	else if (
	    completed_request->netreqs[0]->request_type == GETDNS_RRTYPE_SRV) {

		srvs.capacity = 100;
		if (!(srvs.rrs = GETDNS_XMALLOC(
		    context->mf, _srv_rr, srvs.capacity))) {
			srvs.capacity = 0;
			goto error_free_result;
		}
	}
	if (getdns_dict_set_int(result, "answer_type", GETDNS_NAMETYPE_DNS))
		goto error_free_result;
	
	if (!(replies_full = getdns_list_create_with_context(context)))
		goto error_free_result;

	if (!(replies_tree = getdns_list_create_with_context(context)))
		goto error_free_replies_full;

	if (completed_request->return_call_reporting &&
	    !(call_reporting = getdns_list_create_with_context(context)))
		goto error_free_replies_full;

	for ( netreq_p = completed_request->netreqs
	    ; (netreq = *netreq_p) ; netreq_p++) {
		getdns_bindata *tmp_ipv4_address;
		getdns_bindata *tmp_ipv6_address;

		if (call_reporting && (  netreq->response_len
		                      || netreq->state == NET_REQ_TIMED_OUT)) {
			if (!(netreq_debug =
			   _getdns_create_call_reporting_dict(context,netreq)))
				goto error;

			if (_getdns_list_append_this_dict(
			    call_reporting, netreq_debug))
				goto error;

			netreq_debug = NULL;
		}
		if (! netreq->response_len)
			continue;

		if (netreq->tsig_status == GETDNS_DNSSEC_INSECURE)
			_getdns_network_validate_tsig(netreq);

		nreplies++;
		switch (netreq->dnssec_status) {
		case GETDNS_DNSSEC_SECURE       : nsecure++;
						  break;
		case GETDNS_DNSSEC_INSECURE     : ninsecure++;
						  break;
		case GETDNS_DNSSEC_INDETERMINATE: nindeterminate++;
						  ninsecure++;
						  break;
		case GETDNS_DNSSEC_BOGUS        : if (dnssec_return_status)
							  nbogus++;
						  break;
		}
		if (! completed_request->dnssec_return_all_statuses &&
		    ! completed_request->dnssec_return_validation_chain) {
			if (dnssec_return_status &&
			    netreq->dnssec_status == GETDNS_DNSSEC_BOGUS)
				continue;
			else if (completed_request->dnssec_return_only_secure
			    && netreq->dnssec_status != GETDNS_DNSSEC_SECURE)
				continue;
			else if (completed_request->dnssec &&
			    netreq->dnssec_status == GETDNS_DNSSEC_INDETERMINATE)
				continue;
			else if (netreq->tsig_status == GETDNS_DNSSEC_BOGUS)
				continue;
		}
    		if (!(reply = _getdns_create_reply_dict(context,
		    netreq, just_addrs, &rrsigs_in_answer, &srvs)))
			goto error;

		if (!canonical_name) {
			if (!getdns_dict_get_bindata(
			    reply, "canonical_name", &canonical_name) &&
			    getdns_dict_set_bindata(
			    result, "canonical_name", canonical_name))
				goto error;
		}

		if (!getdns_dict_get_bindata(
		    reply, "answer_ipv4_address", &tmp_ipv4_address)) {
			if (!answer_ipv4_address)
				answer_ipv4_address = tmp_ipv4_address;
			else if (tmp_ipv4_address->size != answer_ipv4_address->size
			     || memcmp(    tmp_ipv4_address->data
			              , answer_ipv4_address->data
				      , answer_ipv4_address->size))
				answer_ipv4_address = NULL;
		}
		if (!getdns_dict_get_bindata(
		    reply, "answer_ipv6_address", &tmp_ipv6_address)) {
			if (!answer_ipv6_address)
				answer_ipv6_address = tmp_ipv6_address;
			else if (tmp_ipv6_address->size != answer_ipv6_address->size
			     || memcmp(    tmp_ipv6_address->data
			              , answer_ipv6_address->data
				      , answer_ipv6_address->size))
				answer_ipv6_address = NULL;
		}
		/* TODO: Check instead if canonical_name for request_type
		 *       is in the answer section.
		 */
		if (_getdns_rrset_answer(&answer_spc, netreq->response
		                                    , netreq->response_len))
			nanswers++;

		if (dnssec_return_status ||
		    completed_request->dnssec_return_validation_chain) {

			if (getdns_dict_set_int(reply, "dnssec_status",
			    netreq->dnssec_status))
				goto error;
		}
		if (netreq->tsig_status != GETDNS_DNSSEC_INDETERMINATE) {
			if (getdns_dict_set_int(reply, "tsig_status",
			    netreq->tsig_status))
				goto error;
		}
    		if (_getdns_list_append_this_dict(replies_tree, reply)) {
    			getdns_dict_destroy(reply);
			goto error;
		}
		if (_getdns_list_append_const_bindata(replies_full,
		    netreq->response_len, netreq->response))
			goto error;
    	}
    	if (_getdns_dict_set_this_list(result, "replies_tree", replies_tree))
		goto error;
	replies_tree = NULL;

	if (!canonical_name &&
	    _getdns_dict_set_const_bindata(result, "canonical_name",
		    completed_request->name_len, completed_request->name))
		goto error;

	if (answer_ipv4_address &&
	    getdns_dict_set_bindata(result, "answer_ipv4_address", answer_ipv4_address))
		goto error;

	if (answer_ipv6_address &&
	    getdns_dict_set_bindata(result, "answer_ipv6_address", answer_ipv6_address))
		goto error;

	if (call_reporting) {
		if (_getdns_dict_set_this_list(
		   result, "call_reporting", call_reporting))
			goto error_free_call_reporting;
		call_reporting = NULL;
	}
	if (_getdns_dict_set_this_list(result, "replies_full", replies_full))
		goto error_free_replies_full;
	replies_full = NULL;

	if (just_addrs) {
	       if (_getdns_dict_set_this_list(
		    result, GETDNS_STR_KEY_JUST_ADDRS, just_addrs))
			goto error_free_result;
		just_addrs = NULL;
	}
	if (srvs.capacity) {
		if (!(srv_addrs = _create_srv_addrs(context, &srvs)) ||
		    _getdns_dict_set_this_list(
		    result, "srv_addresses", srv_addrs))
			goto error_free_result;

		GETDNS_FREE(context->mf, srvs.rrs);
	}
	if (getdns_dict_set_int(result, GETDNS_STR_KEY_STATUS,
	    completed_request->request_timed_out ||
	    nreplies == 0   ? GETDNS_RESPSTATUS_ALL_TIMEOUT :
	    (  completed_request->dnssec
	    && nsecure == 0 && nindeterminate ) > 0
	                    ? GETDNS_RESPSTATUS_NO_SECURE_ANSWERS :
	    (  completed_request->dnssec_return_only_secure
	    && nsecure == 0 && ninsecure ) > 0
	                    ? GETDNS_RESPSTATUS_NO_SECURE_ANSWERS :
	    (  completed_request->dnssec_return_only_secure
	    || completed_request->dnssec ) && nsecure == 0 && nbogus > 0
	                    ? GETDNS_RESPSTATUS_ALL_BOGUS_ANSWERS :
	    nanswers == 0   ? GETDNS_RESPSTATUS_NO_NAME
	                    : GETDNS_RESPSTATUS_GOOD))
		goto error_free_result;

	return result;
error:
	/* cleanup */
	getdns_list_destroy(replies_tree);
error_free_call_reporting:
	getdns_list_destroy(call_reporting);
error_free_replies_full:
	getdns_list_destroy(replies_full);
error_free_result:
	if (srvs.capacity)
		GETDNS_FREE(context->mf, srvs.rrs);
	getdns_list_destroy(srv_addrs);
	getdns_list_destroy(just_addrs);
	getdns_dict_destroy(result);
	return NULL;
}


#ifdef HAVE_LIBUNBOUND
getdns_return_t
getdns_apply_network_result(getdns_network_req* netreq,
    int rcode, void *pkt, int pkt_len, int sec, char* why_bogus)
{
	(void)why_bogus;

	netreq->dnssec_status = sec == 0 ? GETDNS_DNSSEC_INSECURE
	                      : sec == 2 ? GETDNS_DNSSEC_SECURE
			      :            GETDNS_DNSSEC_BOGUS;

	if (pkt) {
		if (netreq->max_udp_payload_size < pkt_len)
			netreq->response = GETDNS_XMALLOC(
			    netreq->owner->context->mf,
			    uint8_t, pkt_len
			);
		(void) memcpy(netreq->response, pkt,
		    (netreq->response_len = pkt_len));
		return GETDNS_RETURN_GOOD;
	}

	if (rcode == GETDNS_RCODE_SERVFAIL) {
		/* Likely to be caused by timeout from a synchronous
		 * lookup.  Don't forge a packet.
		 */
		return GETDNS_RETURN_GOOD;
	}
	/* Likely to be because libunbound refused the request
	 * so ub_res->answer_packet=NULL, ub_res->answer_len=0
	 * So we need to create an answer packet.
	 */
	gldns_write_uint16(netreq->response    , 0); /* query_id */
	gldns_write_uint16(netreq->response + 2, 0); /* reset all flags */
	gldns_write_uint16(netreq->response + GLDNS_QDCOUNT_OFF, 1);
	gldns_write_uint16(netreq->response + GLDNS_ANCOUNT_OFF, 0);
	gldns_write_uint16(netreq->response + GLDNS_NSCOUNT_OFF, 0);
	gldns_write_uint16(netreq->response + GLDNS_ARCOUNT_OFF, 0);

	GLDNS_OPCODE_SET(netreq->response, 3);
	GLDNS_QR_SET(netreq->response);
	GLDNS_RD_SET(netreq->response);
	GLDNS_RA_SET(netreq->response);
	GLDNS_RCODE_SET(netreq->response, rcode);

	(void) memcpy( netreq->response + GLDNS_HEADER_SIZE
	             , netreq->owner->name, netreq->owner->name_len);

	gldns_write_uint16( netreq->response + GLDNS_HEADER_SIZE
	                                     + netreq->owner->name_len
	                  , netreq->request_type);
	gldns_write_uint16( netreq->response + GLDNS_HEADER_SIZE
	                                     + netreq->owner->name_len + 2
	                  , netreq->owner->request_class);

	netreq->response_len = GLDNS_HEADER_SIZE + netreq->owner->name_len + 4;

	return GETDNS_RETURN_GOOD;
}
#endif


getdns_return_t
_getdns_validate_dname(const char* dname) {
    int len;
    int label_len;
    const char* s;
    if (dname == NULL) {
        return GETDNS_RETURN_INVALID_PARAMETER;
    }
    len = strlen(dname);
    if (len > GETDNS_MAX_DNAME_LEN * 4 || len == 0) {
        return GETDNS_RETURN_BAD_DOMAIN_NAME;
    }
    if (len == 1 && dname[0] == '.') {
        /* root is ok */
        return GETDNS_RETURN_GOOD;
    }
	/* By specification [RFC1035] the total length of a DNS label is
	 * restricted to 63 octets and must be larger than 0 (except for the
	 * final root-label).  The total length of a domain name (i.e., label
	 * octets and label length octets) is restricted to 255 octets or less.
	 * With a fully qualified domain name this includes the last label
	 * length octet for the root label.  In a normalized representation the
	 * number of labels (including the root) plus the number of octets in
	 * each label may not be larger than 255.
	 */
    len = 0;
    label_len = 0;
    for (s = dname; *s; ++s) {
        switch (*s) {
            case '.':
                if (label_len > GETDNS_MAX_LABEL_LEN ||
                    label_len == 0) {
                    return GETDNS_RETURN_BAD_DOMAIN_NAME;
                }
                label_len = 0;
		len += 1;
                break;
	    case '\\':
		s += 1;
		if (isdigit((unsigned char)s[0])) {
			/* octet value */
			if (! isdigit((unsigned char)s[1]) && ! isdigit((unsigned char)s[2]))
				return GETDNS_RETURN_BAD_DOMAIN_NAME;

			if ((s[0] - '0') * 100 +
			    (s[1] - '0') * 10 + (s[2] - '0') > 255)
				return GETDNS_RETURN_BAD_DOMAIN_NAME;

			s += 2;
		}
		/* else literal char (1 octet) */
		label_len++;
		len += 1;
		break;
            default:
                label_len++;
		len += 1;
                break;
        }
    }
    if (len > GETDNS_MAX_DNAME_LEN || label_len > GETDNS_MAX_LABEL_LEN) {
        return GETDNS_RETURN_BAD_DOMAIN_NAME;
    }
    return GETDNS_RETURN_GOOD;
} /* _getdns_validate_dname */


static void _getdns_reply2wire_buf(gldns_buffer *buf, const getdns_dict *reply)
{
	getdns_dict *rr_dict, *q_dict, *h_dict;
	getdns_list *section;
	size_t i, pkt_start;
	uint16_t ancount, nscount;
	uint32_t qtype, qclass = GETDNS_RRCLASS_IN, rcode = GETDNS_RCODE_NOERROR;
	getdns_bindata *qname;


	pkt_start = gldns_buffer_position(buf);
	/* Empty header */
	gldns_buffer_write_u32(buf, 0);
	gldns_buffer_write_u32(buf, 0);
	gldns_buffer_write_u32(buf, 0);

	if (   !getdns_dict_get_dict(reply, "question", &q_dict)
	    && !getdns_dict_get_int(q_dict, "qtype", &qtype)
	    && !getdns_dict_get_bindata(q_dict, "qname", &qname)) {

		gldns_buffer_write(buf, qname->data, qname->size);
		gldns_buffer_write_u16(buf, (uint16_t)qtype);
		gldns_buffer_write_u16(buf, (uint16_t)qclass);
		gldns_buffer_write_u16_at(
		    buf, pkt_start + GLDNS_QDCOUNT_OFF, 1);
	}

	if (   !getdns_dict_get_dict(reply, "header", &h_dict)
	    && !getdns_dict_get_int(h_dict, "rcode", &rcode)) {

		GLDNS_RCODE_SET(gldns_buffer_at(buf, pkt_start), rcode);
	}
	if (!getdns_dict_get_list(reply, "answer", &section)) {
		for ( i = 0, ancount = 0
		    ; !getdns_list_get_dict(section, i, &rr_dict)
		    ; i++ ) {

			if (!_getdns_rr_dict2wire(rr_dict, buf))
				ancount++;
		}
		gldns_buffer_write_u16_at(
		    buf, pkt_start + GLDNS_ANCOUNT_OFF, ancount);
	}
	if (!getdns_dict_get_list(reply, "authority", &section)) {
		for ( i = 0, nscount = 0
		    ; !getdns_list_get_dict(section, i, &rr_dict)
		    ; i++ ) {

			if (!_getdns_rr_dict2wire(rr_dict, buf))
				nscount++;
		}
		gldns_buffer_write_u16_at(
		    buf, pkt_start + GLDNS_NSCOUNT_OFF, nscount);
	}
}

static void _getdns_list2wire_buf(gldns_buffer *buf, const getdns_list *l)
{
	getdns_dict *rr_dict;
	size_t i, pkt_start;
	uint16_t ancount;
	uint32_t qtype, qclass = GETDNS_RRCLASS_IN;
	getdns_bindata *qname;

	pkt_start = gldns_buffer_position(buf);
	/* Empty header */
	gldns_buffer_write_u32(buf, 0);
	gldns_buffer_write_u32(buf, 0);
	gldns_buffer_write_u32(buf, 0);

	for ( i = 0
	    ; !getdns_list_get_dict(l, i, &rr_dict)
	    ; i++ ) {

		if (getdns_dict_get_int(rr_dict, "qtype", &qtype) ||
		    getdns_dict_get_bindata(rr_dict, "qname", &qname))
			continue;
		(void) getdns_dict_get_int(rr_dict, "qclass", &qclass);
		gldns_buffer_write(buf, qname->data, qname->size);
		gldns_buffer_write_u16(buf, (uint16_t)qtype);
		gldns_buffer_write_u16(buf, (uint16_t)qclass);
		gldns_buffer_write_u16_at(buf, pkt_start+GLDNS_QDCOUNT_OFF, 1);
		break;
	}
	for ( i = 0, ancount = 0
	    ; !getdns_list_get_dict(l, i, &rr_dict)
	    ; i++ ) {

		if (!_getdns_rr_dict2wire(rr_dict, buf))
			ancount++;
	}
	gldns_buffer_write_u16_at(buf, pkt_start+GLDNS_ANCOUNT_OFF, ancount);
}

uint8_t *_getdns_list2wire(const getdns_list *l,
    uint8_t *buf, size_t *buf_len, const struct mem_funcs *mf)
{
	gldns_buffer gbuf;
	size_t sz;

	gldns_buffer_init_vfixed_frm_data(&gbuf, buf, *buf_len);
	_getdns_list2wire_buf(&gbuf, l);

	if ((sz = gldns_buffer_position(&gbuf)) <= *buf_len) {
		*buf_len = sz;
		return buf;
	}
	if (!(buf = GETDNS_XMALLOC(*mf, uint8_t, (*buf_len = sz))))
		return NULL;

	gldns_buffer_init_frm_data(&gbuf, buf, sz);
	_getdns_list2wire_buf(&gbuf, l);
	return buf;
}

uint8_t *_getdns_reply2wire(const getdns_dict *r,
    uint8_t *buf, size_t *buf_len, const struct mem_funcs *mf)
{
	gldns_buffer gbuf;
	size_t sz;

	gldns_buffer_init_vfixed_frm_data(&gbuf, buf, *buf_len);
	_getdns_reply2wire_buf(&gbuf, r);

	if ((sz = gldns_buffer_position(&gbuf)) <= *buf_len) {
		*buf_len = sz;
		return buf;
	}
	if (!(buf = GETDNS_XMALLOC(*mf, uint8_t, (*buf_len = sz))))
		return NULL;

	gldns_buffer_init_frm_data(&gbuf, buf, sz);
	_getdns_reply2wire_buf(&gbuf, r);
	return buf;
}

void _getdns_wire2list(const uint8_t *pkt, size_t pkt_len, getdns_list *l)
{
	_getdns_rr_iter rr_spc, *rr;
	getdns_dict *rr_dict;

	for ( rr = _getdns_rr_iter_init(&rr_spc, pkt, pkt_len)
	    ; rr ; rr = _getdns_rr_iter_next(rr)) {

		if (!(rr_dict = _getdns_rr_iter2rr_dict(&l->mf, rr)))
			continue;

		if (_getdns_list_append_this_dict(l, rr_dict))
			getdns_dict_destroy(rr_dict);
	}
}

const char * _getdns_auth_str(getdns_auth_state_t auth) {
	static const char*
	getdns_auth_str_array[] = {
		GETDNS_STR_AUTH_NONE,
		GETDNS_STR_AUTH_FAILED,
		GETDNS_STR_AUTH_OK
	};
	return getdns_auth_str_array[auth];
}

/* util-internal.c */
