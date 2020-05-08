/**
 *
 * \file convert.c
 * @brief getdns label conversion functions
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
#include <stdio.h>
#include <string.h>
#include <locale.h>
#ifndef USE_WINSOCK
#include <arpa/inet.h>
#endif
#if defined(HAVE_LIBIDN2)
#include <idn2.h>
#endif
#include "getdns/getdns.h"
#include "getdns/getdns_extra.h"
#include "util-internal.h"
#include "gldns/wire2str.h"
#include "gldns/str2wire.h"
#include "gldns/parseutil.h"
#include "const-info.h"
#include "types-internal.h"	/* For getdns_item */
#include "dict.h"
#include "list.h"
#include "jsmn/jsmn.h"
#ifdef USE_YAML_CONFIG
#include "yaml/convert_yaml_to_json.h"
#endif
#include "convert.h"
#include "debug.h"

getdns_return_t
getdns_convert_dns_name_to_fqdn(
    const getdns_bindata *dns_name_wire_fmt, char **fqdn_as_string)
{
	char *r;

	if (!dns_name_wire_fmt || !fqdn_as_string)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!(r = gldns_wire2str_dname(
	    dns_name_wire_fmt->data, dns_name_wire_fmt->size)))
		return GETDNS_RETURN_GENERIC_ERROR;

	*fqdn_as_string = r;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_convert_fqdn_to_dns_name(
    const char *fqdn_as_string, getdns_bindata **dns_name_wire_fmt)
{
	getdns_bindata *r;
	uint8_t *dname;
	size_t len;

	if (!fqdn_as_string || !dns_name_wire_fmt)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!(r = malloc(sizeof(getdns_bindata))))
		return GETDNS_RETURN_MEMORY_ERROR;

	if (!(dname = gldns_str2wire_dname(fqdn_as_string, &len))) {
		free(r);
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	r->size = len;
	r->data = dname;
	*dns_name_wire_fmt = r;
	return GETDNS_RETURN_GOOD;
}

/*---------------------------------------- getdns_convert_alabel_to_ulabel */
/**
 * Convert UTF-8 string into an ACE-encoded domain
 * It is the application programmer's responsibility to free()
 * the returned buffer after use
 *
 * @param ulabel the UTF-8-encoded domain name to convert
 * @return pointer to ACE-encoded string
 * @return NULL if conversion fails
 */

char *
getdns_convert_ulabel_to_alabel(const char *ulabel)
{
#if defined(HAVE_LIBIDN2)
	uint8_t *alabel;

	if (!ulabel) return NULL;

	if (idn2_lookup_u8((uint8_t *)ulabel, &alabel, IDN2_TRANSITIONAL) == IDN2_OK)
		return (char *)alabel;
#else
	(void)ulabel;
#endif
	return NULL;
}

/*---------------------------------------- getdns_convert_alabel_to_ulabel */
/**
 * Convert ACE-encoded domain name into a UTF-8 string.
 * It is the application programmer's responsibility to free()
 * the returned buffer after use
 *
 * @param alabel the ACE-encoded domain name to convert
 * @return pointer to UTF-8 string
 * @return NULL if conversion fails
 */

char *
getdns_convert_alabel_to_ulabel(const char *alabel)
{
#if defined(HAVE_LIBIDN2)
	char *ulabel;

	if (!alabel) return NULL;

	if (idn2_to_unicode_8z8z(alabel, &ulabel, 0) == IDN2_OK)
		return ulabel;
#else
	(void)alabel;
#endif
	return NULL;
}


char *
getdns_display_ip_address(const struct getdns_bindata
    *bindata_of_ipv4_or_ipv6_address)
{
	char buff[256];
	if (!bindata_of_ipv4_or_ipv6_address ||
	    bindata_of_ipv4_or_ipv6_address->size == 0 ||
	    !bindata_of_ipv4_or_ipv6_address->data) {
		return NULL;
	}
	if (bindata_of_ipv4_or_ipv6_address->size == 4) {
		const char *ipStr = inet_ntop(AF_INET,
		    bindata_of_ipv4_or_ipv6_address->data,
		    buff,
		    256);
		if (ipStr) {
			return strdup(ipStr);
		}
	} else if (bindata_of_ipv4_or_ipv6_address->size == 16) {
		const char *ipStr = inet_ntop(AF_INET6,
		    bindata_of_ipv4_or_ipv6_address->data,
		    buff,
		    256);
		if (ipStr) {
			return strdup(ipStr);
		}
	}
	return NULL;
}

getdns_return_t
getdns_strerror(getdns_return_t err, char *buf, size_t buflen)
{
	const char *err_str = getdns_get_errorstr_by_id(err);

	(void) snprintf(buf, buflen, "%s",
	    err_str ? err_str : "/* <unknown getdns value> */");

	return GETDNS_RETURN_GOOD;
}				/* getdns_strerror */


/* --------------------- rr_dict, wire, str conversions --------------------- */


getdns_return_t
getdns_rr_dict2wire(
    const getdns_dict *rr_dict, uint8_t **wire, size_t *wire_sz)
{
	uint8_t buf_spc[4096], *buf;
	size_t buf_len = sizeof(buf_spc);
	getdns_return_t r = getdns_rr_dict2wire_buf(
	    rr_dict, buf_spc, &buf_len);

	if (r != GETDNS_RETURN_GOOD && r != GETDNS_RETURN_NEED_MORE_SPACE)
		return r;

	if (!(buf = malloc(buf_len ? buf_len : 1)))
		return GETDNS_RETURN_MEMORY_ERROR;

	if (!r)
		memcpy(buf, buf_spc, buf_len);

	else if ((r = getdns_rr_dict2wire_buf(rr_dict, buf, &buf_len))) {
		free(buf);
		return r;
	}
	*wire = buf;
	*wire_sz = buf_len;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_rr_dict2wire_buf(
    const getdns_dict *rr_dict, uint8_t *wire, size_t *wire_sz)
{
	int my_wire_sz;
	getdns_return_t r;

	if (!wire_sz)
		return GETDNS_RETURN_INVALID_PARAMETER;
	else
		my_wire_sz = *wire_sz;

	r = getdns_rr_dict2wire_scan(rr_dict, &wire, &my_wire_sz);
	if (r == GETDNS_RETURN_GOOD || r == GETDNS_RETURN_NEED_MORE_SPACE)
		*wire_sz -= my_wire_sz;
	return r;
}

getdns_return_t
getdns_rr_dict2wire_scan(
    const getdns_dict *rr_dict, uint8_t **wire, int *wire_sz)
{
	getdns_return_t r;
	gldns_buffer gbuf;

	if (!rr_dict || !wire || !*wire || !wire_sz)
		return GETDNS_RETURN_INVALID_PARAMETER;


	gldns_buffer_init_vfixed_frm_data(&gbuf, *wire, *wire_sz);
	if ((r = _getdns_rr_dict2wire(rr_dict, &gbuf)))
		return r;

	if (gldns_buffer_position(&gbuf) == 0)
		return GETDNS_RETURN_GENERIC_ERROR;

	*wire += gldns_buffer_position(&gbuf);
	*wire_sz -= gldns_buffer_position(&gbuf);
	if (gldns_buffer_position(&gbuf) > gldns_buffer_limit(&gbuf))
		return GETDNS_RETURN_NEED_MORE_SPACE;
	else
		return GETDNS_RETURN_GOOD;
}

static struct mem_funcs _getdns_plain_mem_funcs = {
	MF_PLAIN, .mf.pln = { malloc, realloc, free }
};

getdns_return_t
_getdns_wire2rr_dict(struct mem_funcs *mf,
    const uint8_t *wire, size_t wire_len, getdns_dict **rr_dict)
{
	return _getdns_wire2rr_dict_scan(mf, &wire, &wire_len, rr_dict);
}
getdns_return_t
getdns_wire2rr_dict(
    const uint8_t *wire, size_t wire_len, getdns_dict **rr_dict)
{
	return _getdns_wire2rr_dict(
	    &_getdns_plain_mem_funcs, wire, wire_len, rr_dict);
}

getdns_return_t
_getdns_wire2rr_dict_buf(struct mem_funcs *mf,
    const uint8_t *wire, size_t *wire_len, getdns_dict **rr_dict)
{
	size_t my_wire_len;
	getdns_return_t r;

	if (!wire_len)
		return GETDNS_RETURN_INVALID_PARAMETER;
	else
		my_wire_len = *wire_len;

	if ((r = _getdns_wire2rr_dict_scan(mf, &wire, &my_wire_len, rr_dict)))
		return r;
	
	*wire_len -= my_wire_len;
	return GETDNS_RETURN_GOOD;
}
getdns_return_t
getdns_wire2rr_dict_buf(
    const uint8_t *wire, size_t *wire_len, getdns_dict **rr_dict)
{
	return _getdns_wire2rr_dict_buf(
	    &_getdns_plain_mem_funcs, wire, wire_len, rr_dict);
}

getdns_return_t
_getdns_wire2rr_dict_scan(struct mem_funcs *mf,
    const uint8_t **wire, size_t *wire_len, getdns_dict **rr_dict)
{
	_getdns_rr_iter rr_iter_spc, *rr_iter;

	if (!wire || !*wire || !wire_len || !rr_dict)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!(rr_iter = _getdns_single_rr_iter_init(
	    &rr_iter_spc, *wire, *wire_len)))
		return GETDNS_RETURN_GENERIC_ERROR;

	if (!(*rr_dict = _getdns_rr_iter2rr_dict(mf, rr_iter)))
		return GETDNS_RETURN_MEMORY_ERROR;

	*wire_len -= (rr_iter->nxt - rr_iter->pos);
	*wire = rr_iter->nxt;

	return GETDNS_RETURN_GOOD;
}
getdns_return_t
getdns_wire2rr_dict_scan(
    const uint8_t **wire, size_t *wire_len, getdns_dict **rr_dict)
{
	return _getdns_wire2rr_dict_scan(
	    &_getdns_plain_mem_funcs, wire, wire_len, rr_dict);
}


getdns_return_t
getdns_rr_dict2str(
    const getdns_dict *rr_dict, char **str)
{
	char buf_spc[4096], *buf;
	size_t buf_len = sizeof(buf_spc) - 1;
	getdns_return_t r = getdns_rr_dict2str_buf(
	    rr_dict, buf_spc, &buf_len);

	if (r != GETDNS_RETURN_GOOD && r != GETDNS_RETURN_NEED_MORE_SPACE)
		return r;
	
	buf_len += 1;
	if (!(buf = malloc(buf_len)))
		return GETDNS_RETURN_MEMORY_ERROR;

	if (!r)
		memcpy(buf, buf_spc, buf_len);

	else if ((r = getdns_rr_dict2str_buf(rr_dict, buf, &buf_len))) {
		free(buf);
		return r;
	}
	*str = buf;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_rr_dict2str_buf(
    const getdns_dict *rr_dict, char *str, size_t *str_len)
{
	int my_str_len;
	getdns_return_t r;

	if (!str_len)
		return GETDNS_RETURN_INVALID_PARAMETER;
	else
		my_str_len = *str_len;

	r = getdns_rr_dict2str_scan(rr_dict, &str, &my_str_len);
	if (r == GETDNS_RETURN_GOOD || r == GETDNS_RETURN_NEED_MORE_SPACE)
		*str_len -= my_str_len;
	return r;

}

getdns_return_t
getdns_rr_dict2str_scan(
    const getdns_dict *rr_dict, char **str, int *str_len)
{
	getdns_return_t r;
	gldns_buffer gbuf;
	uint8_t buf_spc[4096], *buf = buf_spc, *scan_buf;
	size_t sz, scan_sz;
	int prev_str_len;
	char *prev_str;
	int sz_needed;

	if (!rr_dict || !str || !*str || !str_len)
		return GETDNS_RETURN_INVALID_PARAMETER;

	gldns_buffer_init_vfixed_frm_data(&gbuf, buf, sizeof(buf_spc));
	r = _getdns_rr_dict2wire(rr_dict, &gbuf);
	if (gldns_buffer_position(&gbuf) > sizeof(buf_spc)) {
		if (!(buf = GETDNS_XMALLOC(
		    rr_dict->mf, uint8_t, (sz = gldns_buffer_position(&gbuf))))) {
			return GETDNS_RETURN_MEMORY_ERROR;
		}
		gldns_buffer_init_frm_data(&gbuf, buf, sz);
		r = _getdns_rr_dict2wire(rr_dict, &gbuf);
	}
	if (r) {
		if (buf != buf_spc)
			GETDNS_FREE(rr_dict->mf, buf);
		return r;
	}
	scan_buf = gldns_buffer_begin(&gbuf);
	scan_sz  = gldns_buffer_position(&gbuf);
	prev_str = *str;
	prev_str_len = *str_len;
	sz = (size_t)*str_len;
	sz_needed = gldns_wire2str_rr_scan(
	    &scan_buf, &scan_sz, str, &sz, NULL, 0, NULL);

	if (sz_needed > prev_str_len) {
		*str = prev_str + sz_needed;
		*str_len = prev_str_len - sz_needed;
		r = GETDNS_RETURN_NEED_MORE_SPACE;
	} else  {
		*str_len = sz;
		**str = 0;
	}
	if (buf != buf_spc)
		GETDNS_FREE(rr_dict->mf, buf);
	return r;
}


getdns_return_t
_getdns_str2rr_dict(struct mem_funcs *mf,
    const char *str, getdns_dict **rr_dict, const char *origin, uint32_t default_ttl)
{
	uint8_t wire_spc[4096], *wire = wire_spc;
	uint8_t origin_spc[256], *origin_wf;
	size_t origin_len = sizeof(origin_spc), wire_len = sizeof(wire_spc);
	int e;
	getdns_return_t r;

	if (!str || !rr_dict)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!origin)
		origin_wf = NULL;

	else if (gldns_str2wire_dname_buf(origin, origin_spc, &origin_len))
		return GETDNS_RETURN_GENERIC_ERROR;
	else
		origin_wf = origin_spc;

	e = gldns_str2wire_rr_buf(str, wire, &wire_len,
	    NULL, default_ttl, origin_wf, origin_len, NULL, 0);
	if (GLDNS_WIREPARSE_ERROR(e) == GLDNS_WIREPARSE_ERR_BUFFER_TOO_SMALL) {

		if (!(wire = GETDNS_XMALLOC(
		    *mf, uint8_t, (wire_len = GLDNS_RR_BUF_SIZE))))
			return GETDNS_RETURN_MEMORY_ERROR;
		e = gldns_str2wire_rr_buf(str, wire, &wire_len,
	            NULL, default_ttl, origin_wf, origin_len, NULL, 0);
	}
	if (e) {
		if (wire != wire_spc)
			GETDNS_FREE(*mf, wire);
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	r = _getdns_wire2rr_dict(mf, wire, wire_len, rr_dict);
	if (wire != wire_spc)
		GETDNS_FREE(*mf, wire);
	return r;
}
getdns_return_t
getdns_str2rr_dict(
    const char *str, getdns_dict **rr_dict, const char *origin, uint32_t default_ttl)
{
	return _getdns_str2rr_dict(
	    &_getdns_plain_mem_funcs, str, rr_dict, origin, default_ttl);
}


getdns_return_t
_getdns_fp2rr_list(struct mem_funcs *mf,
    FILE *in, getdns_list **rr_list, const char *origin, uint32_t default_ttl)
{
	struct gldns_file_parse_state pst;
	getdns_list *rrs;
	getdns_return_t r = GETDNS_RETURN_GOOD;
	uint8_t *rr;
	size_t len, dname_len;
	getdns_dict *rr_dict;

	if (!in || !rr_list)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!origin) {
		*pst.origin = 0;
		pst.origin_len = 1;

	} else if (gldns_str2wire_dname_buf(origin,pst.origin,&pst.origin_len))
		return GETDNS_RETURN_GENERIC_ERROR;

	*pst.prev_rr = 0;
	pst.prev_rr_len = 1;
	pst.default_ttl = default_ttl;
	pst.lineno = 1;

	if (!(rrs = _getdns_list_create_with_mf(mf)))
		return GETDNS_RETURN_MEMORY_ERROR;
	

	if (!(rr = GETDNS_XMALLOC(*mf, uint8_t, GLDNS_RR_BUF_SIZE)))
		r = GETDNS_RETURN_MEMORY_ERROR;

	else while (r == GETDNS_RETURN_GOOD && !feof(in)) {
		len = GLDNS_RR_BUF_SIZE;
		dname_len = 0;
		if (gldns_fp2wire_rr_buf(in, rr, &len, &dname_len, &pst))
			break;
		if (dname_len && dname_len < sizeof(pst.prev_rr)) {
			memcpy(pst.prev_rr, rr, dname_len);
			pst.prev_rr_len = dname_len;
		}
		if (len == 0)
			continue;
		if ((r = _getdns_wire2rr_dict(mf, rr, len, &rr_dict)))
			break;
		if ((r = _getdns_list_append_this_dict(rrs, rr_dict)))
			getdns_dict_destroy(rr_dict);
	}
	if (rr)
		GETDNS_FREE(*mf, rr);
	if (r)
		getdns_list_destroy(rrs);
	else
		*rr_list = rrs;
	return r;
}

getdns_return_t
getdns_fp2rr_list(
    FILE *in, getdns_list **rr_list, const char *origin, uint32_t default_ttl)
{
	return _getdns_fp2rr_list(
	    &_getdns_plain_mem_funcs, in, rr_list, origin, default_ttl);
}

#define SET_WIRE_INT(X,Y) if (getdns_dict_set_int(header, #X , (int) \
                              GLDNS_ ## Y ## _WIRE(*wire))) goto error
#define SET_WIRE_BIT(X,Y) if (getdns_dict_set_int(header, #X , \
                              GLDNS_ ## Y ## _WIRE(*wire) ? 1 : 0)) goto error
#define SET_WIRE_CNT(X,Y) if (getdns_dict_set_int(header, #X , (int) \
                              GLDNS_ ## Y (*wire))) goto error 

getdns_return_t
_getdns_wire2msg_dict_scan(struct mem_funcs *mf,
    const uint8_t **wire, size_t *wire_len, getdns_dict **msg_dict)
{
	getdns_return_t r = GETDNS_RETURN_GOOD;
	getdns_dict *result = NULL, *header = NULL, *rr_dict = NULL;
	_getdns_rr_iter rr_iter_storage, *rr_iter;
	_getdns_section section;
	getdns_list *sections[16] = { NULL, NULL, NULL, NULL
	                            , NULL, NULL, NULL, NULL
	                            , NULL, NULL, NULL, NULL
	                            , NULL, NULL, NULL, NULL };
	const uint8_t *eop; /* end of packet */

	if (!wire || !*wire || !wire_len || !msg_dict)
		return GETDNS_RETURN_INVALID_PARAMETER;

#if 0 && defined(SERVER_DEBUG) && SERVER_DEBUG
	do {
		char *str = gldns_wire2str_pkt((uint8_t *)*wire, *wire_len);
		DEBUG_SERVER("_getdns_wire2msg_dict_scan for a packet size: %d: %s\n",
		    (int)*wire_len, str);
		free(str);
	} while(0);
#endif

       	if (!(result = _getdns_dict_create_with_mf(mf)) ||
	    !(header = _getdns_dict_create_with_mf(mf)) ||
	    !(sections[SECTION_ANSWER]
	             = _getdns_list_create_with_mf(mf)) ||
	    !(sections[SECTION_AUTHORITY]
	             = _getdns_list_create_with_mf(mf)) ||
	    !(sections[SECTION_ADDITIONAL]
	             = _getdns_list_create_with_mf(mf))) {
		r = GETDNS_RETURN_MEMORY_ERROR;
		goto error;
	}
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
	eop = *wire + 12;

	for ( rr_iter = _getdns_rr_iter_init(&rr_iter_storage,*wire,*wire_len)
	    ; rr_iter
	    ; rr_iter = _getdns_rr_iter_next(rr_iter)) {

		if (rr_iter->nxt > eop)
			eop = rr_iter->nxt;

		if (!(rr_dict = _getdns_rr_iter2rr_dict(mf, rr_iter)))
			continue;

		switch ((section = _getdns_rr_iter_section(rr_iter))) {
		case SECTION_QUESTION:
			if ((r = _getdns_dict_set_this_dict(
			     result, "question", rr_dict)))
				goto error;
			break;
		case SECTION_ANSWER:
		case SECTION_AUTHORITY:
		case SECTION_ADDITIONAL:
			if ((r = _getdns_list_append_this_dict(
			     sections[section], rr_dict)))
				goto error;
			break;
		default:
			r = GETDNS_RETURN_GENERIC_ERROR;
			goto error;
		}
		rr_dict = NULL;
	}
	if (!(r = _getdns_dict_set_this_list(result, "answer",
	    sections[SECTION_ANSWER])))
		sections[SECTION_ANSWER] = NULL;
	else	goto error;

	if (!(r = _getdns_dict_set_this_list(result, "authority",
	    sections[SECTION_AUTHORITY])))
		sections[SECTION_AUTHORITY] = NULL;
	else	goto error;

	if (!(r = _getdns_dict_set_this_list(result, "additional",
	    sections[SECTION_ADDITIONAL])))
		sections[SECTION_ADDITIONAL] = NULL;
	else	goto error;

	*wire_len -= (eop - *wire);
	*wire = eop;
error:
	getdns_dict_destroy(rr_dict);
	getdns_list_destroy(sections[SECTION_ADDITIONAL]);
	getdns_list_destroy(sections[SECTION_AUTHORITY]);
	getdns_list_destroy(sections[SECTION_ANSWER]);
	getdns_dict_destroy(header);
	if (r)
		getdns_dict_destroy(result);
	else
		*msg_dict = result;

	return r;
}


getdns_return_t
_getdns_wire2msg_dict(struct mem_funcs *mf,
    const uint8_t *wire, size_t wire_len, getdns_dict **msg_dict)
{
	return _getdns_wire2msg_dict_scan(mf, &wire, &wire_len, msg_dict);
}
getdns_return_t
getdns_wire2msg_dict(
    const uint8_t *wire, size_t wire_len, getdns_dict **msg_dict)
{
	return _getdns_wire2msg_dict(
	    &_getdns_plain_mem_funcs, wire, wire_len, msg_dict);
}

getdns_return_t
_getdns_wire2msg_dict_buf(struct mem_funcs *mf,
    const uint8_t *wire, size_t *wire_len, getdns_dict **msg_dict)
{
	size_t my_wire_len;
	getdns_return_t r;

	if (!wire_len)
		return GETDNS_RETURN_INVALID_PARAMETER;
	else
		my_wire_len = *wire_len;

	if ((r = _getdns_wire2msg_dict_scan(mf, &wire, &my_wire_len, msg_dict)))
		return r;
	
	*wire_len -= my_wire_len;
	return GETDNS_RETURN_GOOD;
}
getdns_return_t
getdns_wire2msg_dict_buf(
    const uint8_t *wire, size_t *wire_len, getdns_dict **msg_dict)
{
	return _getdns_wire2msg_dict_buf(
	    &_getdns_plain_mem_funcs, wire, wire_len, msg_dict);
}

getdns_return_t
getdns_wire2msg_dict_scan(
    const uint8_t **wire, size_t *wire_len, getdns_dict **msg_dict)
{
	return _getdns_wire2msg_dict_scan(
	    &_getdns_plain_mem_funcs, wire, wire_len, msg_dict);
}

#define SET_HEADER_INT(X,Y) \
	if (!getdns_dict_get_int(reply, "/header/" #X, &n)) \
		GLDNS_ ## Y ## _SET(header, n);
#define SET_HEADER_BIT(X,Y) \
	if (!getdns_dict_get_int(reply, "/header/" #X, &n)) { \
		if (n) GLDNS_ ## Y ## _SET(header); \
		else   GLDNS_ ## Y ## _CLR(header); \
	}

getdns_return_t
_getdns_reply_dict2wire(
    const getdns_dict *reply, gldns_buffer *buf, int reuse_header)
{
	uint8_t header_spc[GLDNS_HEADER_SIZE], *header;
	uint32_t n, qtype, qclass = GETDNS_RRCLASS_IN, rr_type;
	size_t pkt_start, i;
	getdns_list *section;
	getdns_dict *rr_dict;
	getdns_bindata *qname;
	int remove_dnssec;

	pkt_start = gldns_buffer_position(buf);
	if (reuse_header) {
		if (gldns_buffer_remaining(buf) < GLDNS_HEADER_SIZE)
			return GETDNS_RETURN_NEED_MORE_SPACE;
		header = gldns_buffer_current(buf);
		gldns_buffer_skip(buf, GLDNS_HEADER_SIZE);
	} else
		(void) memset((header = header_spc), 0, GLDNS_HEADER_SIZE);

	SET_HEADER_INT(id, ID);
	SET_HEADER_BIT(qr, QR);
	SET_HEADER_BIT(aa, AA);
	SET_HEADER_BIT(tc, TC);
	SET_HEADER_BIT(rd, RD);
	SET_HEADER_BIT(cd, CD);
	SET_HEADER_BIT(ra, RA);
	SET_HEADER_BIT(ad, AD);
	SET_HEADER_INT(opcode, OPCODE);
	SET_HEADER_INT(rcode, RCODE);
	SET_HEADER_BIT(z, Z);

	if (!reuse_header)
		gldns_buffer_write(buf, header, GLDNS_HEADER_SIZE);

	if (!getdns_dict_get_bindata(reply, "/question/qname", &qname) &&
	    !getdns_dict_get_int(reply, "/question/qtype", &qtype)) {
		(void)getdns_dict_get_int(reply, "/question/qclass", &qclass);
		gldns_buffer_write(buf, qname->data, qname->size);
		gldns_buffer_write_u16(buf, (uint16_t)qtype);
		gldns_buffer_write_u16(buf, (uint16_t)qclass);
		gldns_buffer_write_u16_at(buf, pkt_start+GLDNS_QDCOUNT_OFF, 1);
		if (reuse_header) {
			gldns_buffer_write_u16_at(
			    buf, pkt_start+GLDNS_ANCOUNT_OFF, 0);
			gldns_buffer_write_u16_at(
			    buf, pkt_start+GLDNS_NSCOUNT_OFF, 0);
			gldns_buffer_write_u16_at(
			    buf, pkt_start+GLDNS_ARCOUNT_OFF, 0);
		}
	}
	remove_dnssec = !getdns_dict_get_int(reply, "/header/do", &n) && n == 0;

	if (!getdns_dict_get_list(reply, "answer", &section)) {
		for ( n = 0, i = 0
		    ; !getdns_list_get_dict(section, i, &rr_dict); i++) {

			if (remove_dnssec &&
			    !getdns_dict_get_int(rr_dict, "type", &rr_type) &&
			    rr_type == GETDNS_RRTYPE_RRSIG)
				continue;
			if (!_getdns_rr_dict2wire(rr_dict, buf))
				 n++;
		}
		gldns_buffer_write_u16_at(buf, pkt_start+GLDNS_ANCOUNT_OFF, n);
	}
	if (!getdns_dict_get_list(reply, "authority", &section)) {
		for ( n = 0, i = 0
		    ; !getdns_list_get_dict(section, i, &rr_dict); i++) {

			if (remove_dnssec &&
			    !getdns_dict_get_int(rr_dict, "type", &rr_type) &&
			    (  rr_type == GETDNS_RRTYPE_RRSIG
			    || rr_type == GETDNS_RRTYPE_NSEC
			    || rr_type == GETDNS_RRTYPE_NSEC3
			    || rr_type == GETDNS_RRTYPE_DS
			    ))
				continue;
			if (!_getdns_rr_dict2wire(rr_dict, buf))
				 n++;
		}
		gldns_buffer_write_u16_at(buf, pkt_start+GLDNS_NSCOUNT_OFF, n);
	}
	if (!getdns_dict_get_list(reply, "additional", &section)) {
		for ( n = 0, i = 0
		    ; !getdns_list_get_dict(section, i, &rr_dict); i++) {

			if (remove_dnssec &&
			    !getdns_dict_get_int(rr_dict, "type", &rr_type) &&
			    rr_type == GETDNS_RRTYPE_RRSIG)
				continue;
			 if (!_getdns_rr_dict2wire(rr_dict, buf))
				 n++;
		}
		gldns_buffer_write_u16_at(buf, pkt_start+GLDNS_ARCOUNT_OFF, n);
	}
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
_getdns_msg_dict2wire_buf(const getdns_dict *msg_dict, gldns_buffer *gbuf)
{
	getdns_return_t r;
	getdns_list *replies;
	getdns_dict *reply;
	size_t i;

	if ((r = getdns_dict_get_list(msg_dict, "replies_tree", &replies))) {
		if (r != GETDNS_RETURN_NO_SUCH_DICT_NAME)
			return r;
		return _getdns_reply_dict2wire(msg_dict, gbuf, 0);
	}
	for (i = 0; r == GETDNS_RETURN_GOOD; i++) {
		if (!(r = getdns_list_get_dict(replies, i, &reply)))
			r = _getdns_reply_dict2wire(reply, gbuf, 0);
	}
	return r == GETDNS_RETURN_NO_SUCH_LIST_ITEM ? GETDNS_RETURN_GOOD : r;
}

getdns_return_t
getdns_msg_dict2wire(
    const getdns_dict *msg_dict, uint8_t **wire, size_t *wire_sz)
{
	uint8_t buf_spc[4096], *buf;
	size_t buf_len = sizeof(buf_spc);
	getdns_return_t r = getdns_msg_dict2wire_buf(
	    msg_dict, buf_spc, &buf_len);

	if (r != GETDNS_RETURN_GOOD && r != GETDNS_RETURN_NEED_MORE_SPACE)
		return r;

	if (!(buf = malloc(buf_len ? buf_len : 1)))
		return GETDNS_RETURN_MEMORY_ERROR;

	if (!r)
		memcpy(buf, buf_spc, buf_len);

	else if ((r = getdns_msg_dict2wire_buf(msg_dict, buf, &buf_len))) {
		free(buf);
		return r;
	}
	*wire = buf;
	*wire_sz = buf_len;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_msg_dict2wire_buf(
    const getdns_dict *msg_dict, uint8_t *wire, size_t *wire_sz)
{
	int my_wire_sz;
	getdns_return_t r;

	if (!wire_sz)
		return GETDNS_RETURN_INVALID_PARAMETER;
	else
		my_wire_sz = *wire_sz;

	r = getdns_msg_dict2wire_scan(msg_dict, &wire, &my_wire_sz);
	if (r == GETDNS_RETURN_GOOD || r == GETDNS_RETURN_NEED_MORE_SPACE)
		*wire_sz -= my_wire_sz;
	return r;
}

getdns_return_t
getdns_msg_dict2wire_scan(
    const getdns_dict *msg_dict, uint8_t **wire, int *wire_sz)
{
	getdns_return_t r;
	gldns_buffer gbuf;

	if (!msg_dict || !wire || !wire_sz || (!*wire && *wire_sz))
		return GETDNS_RETURN_INVALID_PARAMETER;

	gldns_buffer_init_vfixed_frm_data(&gbuf, *wire, *wire_sz);
	if ((r = _getdns_msg_dict2wire_buf(msg_dict, &gbuf)))
		return r;

	if (gldns_buffer_position(&gbuf) == 0)
		return GETDNS_RETURN_GENERIC_ERROR;

	*wire += gldns_buffer_position(&gbuf);
	*wire_sz -= gldns_buffer_position(&gbuf);
	if (gldns_buffer_position(&gbuf) > gldns_buffer_limit(&gbuf))
		return GETDNS_RETURN_NEED_MORE_SPACE;
	else
		return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_msg_dict2str(
    const getdns_dict *msg_dict, char **str)
{
	char buf_spc[4096], *buf;
	size_t buf_len = sizeof(buf_spc) - 1;
	getdns_return_t r = getdns_msg_dict2str_buf(
	    msg_dict, buf_spc, &buf_len);

	if (r != GETDNS_RETURN_GOOD && r != GETDNS_RETURN_NEED_MORE_SPACE)
		return r;
	
	buf_len += 1;
	if (!(buf = malloc(buf_len)))
		return GETDNS_RETURN_MEMORY_ERROR;

	if (!r)
		memcpy(buf, buf_spc, buf_len);

	else if ((r = getdns_msg_dict2str_buf(msg_dict, buf, &buf_len))) {
		free(buf);
		return r;
	}
	*str = buf;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_msg_dict2str_buf(
    const getdns_dict *msg_dict, char *str, size_t *str_len)
{
	int my_str_len;
	getdns_return_t r;

	if (!str_len)
		return GETDNS_RETURN_INVALID_PARAMETER;
	else
		my_str_len = *str_len;

	r = getdns_msg_dict2str_scan(msg_dict, &str, &my_str_len);
	if (r == GETDNS_RETURN_GOOD || r == GETDNS_RETURN_NEED_MORE_SPACE)
		*str_len -= my_str_len;
	return r;

}

getdns_return_t
getdns_msg_dict2str_scan(
    const getdns_dict *msg_dict, char **str, int *str_len)
{
	getdns_return_t r;
	gldns_buffer gbuf;
	uint8_t buf_spc[4096], *buf = buf_spc, *scan_buf;
	size_t sz, scan_sz;
	int prev_str_len;
	char *prev_str;
	int sz_needed;

	if (!msg_dict || !str || !*str || !str_len)
		return GETDNS_RETURN_INVALID_PARAMETER;

	gldns_buffer_init_vfixed_frm_data(&gbuf, buf, sizeof(buf_spc));
	r = _getdns_msg_dict2wire_buf(msg_dict, &gbuf);
	if (gldns_buffer_position(&gbuf) > sizeof(buf_spc)) {
		if (!(buf = GETDNS_XMALLOC(
		    msg_dict->mf, uint8_t, (sz = gldns_buffer_position(&gbuf))))) {
			return GETDNS_RETURN_MEMORY_ERROR;
		}
		gldns_buffer_init_frm_data(&gbuf, buf, sz);
		r = _getdns_msg_dict2wire_buf(msg_dict, &gbuf);
	}
	if (r) {
		if (buf != buf_spc)
			GETDNS_FREE(msg_dict->mf, buf);
		return r;
	}
	scan_buf = gldns_buffer_begin(&gbuf);
	scan_sz  = gldns_buffer_position(&gbuf);
	prev_str = *str;
	prev_str_len = *str_len;
	sz = (size_t)*str_len;
	sz_needed = gldns_wire2str_pkt_scan(
	    &scan_buf, &scan_sz, str, &sz);

	if (sz_needed > prev_str_len) {
		*str = prev_str + sz_needed;
		*str_len = prev_str_len - sz_needed;
		r = GETDNS_RETURN_NEED_MORE_SPACE;
	} else  {
		*str_len = sz;
		**str = 0;
	}
	if (buf != buf_spc)
		GETDNS_FREE(msg_dict->mf, buf);
	return r;
}

static getdns_dict *
_getdns_ipaddr_dict_mf(struct mem_funcs *mf, const char *ipstr)
{
	getdns_dict *r = _getdns_dict_create_with_mf(mf);
	char *s = strchr(ipstr, '%'), *scope_id_str = "";
	char *p = strchr(ipstr, '@'), *portstr = "";
	char *t = strchr(ipstr, '#'), *tls_portstr = "";
	char *n = strchr(ipstr, '~'), *tls_namestr = "";
	/* ^[alg:]name:key */
	char *T = strchr(ipstr, '^'), *tsig_name_str = ""
	                            , *tsig_secret_str = ""
	                            , *tsig_algorithm_str = "";
	char *br, *c;
	int            tsig_secret_size;
	uint8_t        tsig_secret_buf[256]; /* 4 times SHA512 */
	getdns_bindata tsig_secret;
	uint8_t buf[sizeof(struct in6_addr)];
	getdns_bindata addr;

	addr.data = buf;

	if (!r) return NULL;

	if (*ipstr == '[') {
		char *br = strchr(ipstr, ']');
		if (br) {
			ipstr += 1;
			*br = 0;
			if ((c = strchr(br + 1, ':'))) {
				p = c;
			}
		}
	} else if ((br = strchr(ipstr, '.')) && (c = strchr(br + 1, ':'))
	    && (T == NULL || c < T))
		p = c;

	else if ((*ipstr == '*') && (c = strchr(ipstr+1, ':')))
		p = c;

	if (s) {
		*s = 0;
		scope_id_str = s + 1;
	}
	if (p) {
		*p = 0;
		portstr = p + 1;
	}
	if (t) {
		*t = 0;
		tls_portstr = t + 1;
	}
	if (n) {
		*n = 0;
		tls_namestr = n + 1;
	}
	if (T) {
		*T = 0;
		tsig_name_str = T + 1;
		if ((T = strchr(tsig_name_str, ':'))) {
			*T = 0;
			tsig_secret_str = T + 1;
			if ((T = strchr(tsig_secret_str, ':'))) {
				*T = 0;
				tsig_algorithm_str  = tsig_name_str;
				tsig_name_str = tsig_secret_str;
				tsig_secret_str  = T + 1;
			}
		} else {
			tsig_name_str = "";
		}
	}
	if (*ipstr == '*') {
		getdns_dict_util_set_string(r, "address_type", "IPv6");
		addr.size = 16;
		(void) memset(buf, 0, 16);
	} else if (strchr(ipstr, ':')) {
		getdns_dict_util_set_string(r, "address_type", "IPv6");
		addr.size = 16;
		if (inet_pton(AF_INET6, ipstr, buf) <= 0) {
			getdns_dict_destroy(r);
			return NULL;
		}
	} else {
		getdns_dict_util_set_string(r, "address_type", "IPv4");
		addr.size = 4;
		if (inet_pton(AF_INET, ipstr, buf) <= 0) {
			getdns_dict_destroy(r);
			return NULL;
		}
	}
	getdns_dict_set_bindata(r, "address_data", &addr);
	if (*portstr)
		getdns_dict_set_int(r, "port", (int32_t)atoi(portstr));
	if (*tls_portstr)
		getdns_dict_set_int(r, "tls_port", (int32_t)atoi(tls_portstr));
	if (*tls_namestr) {
		getdns_dict_util_set_string(r, "tls_auth_name", tls_namestr);
	}
	if (*scope_id_str)
		getdns_dict_util_set_string(r, "scope_id", scope_id_str);
	if (*tsig_name_str)
		getdns_dict_util_set_string(r, "tsig_name", tsig_name_str);
	if (*tsig_algorithm_str)
		getdns_dict_util_set_string(r, "tsig_algorithm", tsig_algorithm_str);
	if (*tsig_secret_str) {
		tsig_secret_size = gldns_b64_pton(
		    tsig_secret_str, tsig_secret_buf, sizeof(tsig_secret_buf));
		if (tsig_secret_size > 0) {
			tsig_secret.size = tsig_secret_size;
			tsig_secret.data = tsig_secret_buf;
			getdns_dict_set_bindata(r, "tsig_secret", &tsig_secret);
		}
	}
	return r;
}

static int _jsmn_get_ipdict(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    getdns_dict **value)
{
	char value_str[3072];
	int size = t->end - t->start;

	if (size <= 0 || size >= (int)sizeof(value_str))
		return 0;

	(void) memcpy(value_str, js + t->start, size);
	value_str[size] = '\0';

	*value = _getdns_ipaddr_dict_mf(mf, value_str);
	return *value != NULL;
}

static int _jsmn_get_base64_data(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    getdns_bindata **value)
{
	int e, i;
	int size = t->end - t->start;
	char value_str_buf[1025];
	char *value_str;
	size_t target_buf_size;

	assert(size >= 4);

	if (size % 4 != 0)
		return 0;

	e = t->end;
	if (js[e - 1] == '=') e -= 1;
	if (js[e - 1] == '=') e -= 1;

	for (i = t->start; i < e; i++)
		if (!((js[i] >= '0' && js[i] <= '9')
		    ||(js[i] >= 'a' && js[i] <= 'z')
		    ||(js[i] >= 'A' && js[i] <= 'Z')
		    || js[i] == '+' || js[i] == '/'))
			return 0;

	target_buf_size = gldns_b64_pton_calculate_size(size);
	if (!(*value = GETDNS_MALLOC(*mf, getdns_bindata)))
		return 0;

	else if (!((*value)->data = GETDNS_XMALLOC(
	    *mf, uint8_t, target_buf_size))) {
		GETDNS_FREE(*mf, *value);
		return 0;
	}
	if ((size_t)size >= sizeof(value_str_buf))
		value_str = GETDNS_XMALLOC(*mf, char, size + 1);
	else	value_str = value_str_buf;
	
	if (value_str) {
		(void) memcpy(value_str, js + t->start, size);
		value_str[size] = '\0';

		e = gldns_b64_pton(value_str, (*value)->data, target_buf_size);

		if (value_str != value_str_buf)
			GETDNS_FREE(*mf, value_str);

		if (e > 0) {
			(*value)->size = e;
			return 1;
		}
	}
	GETDNS_FREE(*mf, (*value)->data);
	GETDNS_FREE(*mf,  *value);
	return 0;
}

static int _jsmn_get_data(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    getdns_bindata **value)
{
	int i;
	size_t j;
	uint8_t h, l;

	if ((t->end - t->start) < 4 || (t->end - t->start) % 2 == 1)
		return 0;

	if (js[t->start] != '0' || js[t->start + 1] != 'x')
		return _jsmn_get_base64_data(mf, js, t, value);

	for (i = t->start + 2; i < t->end; i++)
		if (!((js[i] >= '0' && js[i] <= '9')
		    ||(js[i] >= 'a' && js[i] <= 'f')
		    ||(js[i] >= 'A' && js[i] <= 'F')))
			return _jsmn_get_base64_data(mf, js, t, value);

	if (!(*value = GETDNS_MALLOC(*mf, getdns_bindata)))
		return 0;

	else if (!((*value)->data = GETDNS_XMALLOC(
	    *mf, uint8_t, (t->end - t->start) / 2 - 1))) {
		GETDNS_FREE(*mf, *value);
		return 0;
	}
	for (i = t->start + 2, j = 0; i < t->end; i++, j++) {
		h = js[i] >= '0' && js[i] <= '9' ? js[i] - '0'
		  : js[i] >= 'A' && js[i] <= 'F' ? js[i] + 10 - 'A'
		                                 : js[i] + 10 - 'a';
		h <<= 4;
		i++;
		l = js[i] >= '0' && js[i] <= '9' ? js[i] - '0'
		  : js[i] >= 'A' && js[i] <= 'F' ? js[i] + 10 - 'A'
		                                 : js[i] + 10 - 'a';
		(*value)->data[j] = h | l;
	}
	(*value)->size = j;
	return 1;
}

static int _jsmn_get_dname(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    getdns_bindata **value)
{
	char value_str[1025];
	int size = t->end - t->start;
	(void)mf; /* TODO: Fix to use  mf */

	if (size <= 0 || size >= (int)sizeof(value_str) || js[t->end - 1] != '.')
		return 0;

	(void) memcpy(value_str, js + t->start, size);
	value_str[size] = '\0';

	return !getdns_convert_fqdn_to_dns_name(value_str, value);
}

static int _jsmn_get_ipv4(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    getdns_bindata **value)
{
	char value_str[16];
	int size = t->end - t->start;
	uint8_t buf[4];

	if (size <= 0 || size >= (int)sizeof(value_str))
		return 0;

	(void) memcpy(value_str, js + t->start, size);
	value_str[size] = '\0';

	if (inet_pton(AF_INET, value_str, buf) <= 0)
		; /* pass */

	else if (!(*value = GETDNS_MALLOC(*mf, getdns_bindata)))
		; /* pass */

	else if (!((*value)->data = GETDNS_XMALLOC(*mf, uint8_t, 4)))
		GETDNS_FREE(*mf, *value);

	else {
		(*value)->size = 4;
		(void) memcpy((*value)->data, buf, 4);
		return 1;
	}
	return 0;
}

static int _jsmn_get_ipv6(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    getdns_bindata **value)
{
	char value_str[40];
	int size = t->end - t->start;
	uint8_t buf[16];

	if (size <= 0 || size >= (int)sizeof(value_str))
		return 0;

	(void) memcpy(value_str, js + t->start, size);
	value_str[size] = '\0';

	if (inet_pton(AF_INET6, value_str, buf) <= 0)
		; /* pass */

	else if (!(*value = GETDNS_MALLOC(*mf, getdns_bindata)))
		; /* pass */

	else if (!((*value)->data = GETDNS_XMALLOC(*mf, uint8_t, 16)))
		GETDNS_FREE(*mf, *value);

	else {
		(*value)->size = 16;
		(void) memcpy((*value)->data, buf, 16);
		return 1;
	}
	return 0;
}

static int _jsmn_get_int(const char *js, jsmntok_t *t, uint32_t *value)
{
	char value_str[11];
	int size = t->end - t->start;
	char *endptr;

	if (size <= 0 || size >= (int)sizeof(value_str))
		return 0;

	(void) memcpy(value_str, js + t->start, size);
	value_str[size] = '\0';

	*value = (uint32_t)strtoul(value_str, &endptr, 10);
	return *value_str != '\0' && *endptr == '\0';
}

static int _jsmn_get_const(const char *js, jsmntok_t *t, uint32_t *value)
{
	char value_str[80] = "";
	int size = t->end - t->start;

	if (size <= 0 || size >= (int)sizeof(value_str))
		return 0;

	(void) memcpy(value_str, js + t->start, size);
	value_str[size] = '\0';

	return _getdns_get_const_name_info(value_str, value);
}

static void
_getdns_destroy_item_data(struct mem_funcs *mf, getdns_item *item)
{
	switch (item->dtype) {
	case t_dict:
		getdns_dict_destroy(item->data.dict);
		break;

	case t_list:
		getdns_list_destroy(item->data.list);
		break;

	case t_bindata:
		GETDNS_FREE(*mf, item->data.bindata->data);
		GETDNS_FREE(*mf, item->data.bindata);
	default:
		break;
	}
}
static int _jsmn_get_item(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    size_t count, getdns_item *item, getdns_return_t *r);

static int _jsmn_get_dict(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    size_t count, getdns_dict *dict, getdns_return_t *r)
{
	int i;
	size_t j = 1;
	char key_spc[1024], *key = NULL;
	getdns_item child_item;

	if (t->size <= 0)
		*r = GETDNS_RETURN_GOOD;

	else for (i = 0; i < t->size; i++) {
		if (t[j].type != JSMN_STRING &&
		    t[j].type != JSMN_PRIMITIVE) {

			/* Key must be string or primitive */
			*r = GETDNS_RETURN_WRONG_TYPE_REQUESTED;
			break;
		}
		if (t[j].end <= t[j].start) {
			/* Key must be at least 1 character */
			*r = GETDNS_RETURN_GENERIC_ERROR; /* range error */
			break;
		}
		if (t[j].end - t[j].start < (int)sizeof(key_spc))
			key = key_spc;

		else if (!(key = GETDNS_XMALLOC(
		    *mf, char, t[j].end - t[j].start + 1))) {

			*r = GETDNS_RETURN_MEMORY_ERROR;
			break;
		}
		(void) memcpy(key, js + t[j].start, t[j].end - t[j].start);
		key[t[j].end - t[j].start] = '\0';
		j += 1;

		j += _jsmn_get_item(mf, js, t + j, count - j, &child_item, r);
		if (*r) break;

		switch (child_item.dtype) {
		case t_int:
			*r = getdns_dict_set_int(dict, key,
			    child_item.data.n);
			break;
		case t_bindata:
			*r = getdns_dict_set_bindata(dict, key,
			    child_item.data.bindata);
			break;
		case t_list:
			*r = getdns_dict_set_list(dict, key,
			    child_item.data.list);
			break;
		case t_dict:
			*r = getdns_dict_set_dict(dict, key,
			    child_item.data.dict);
			break;
		default:
			*r = GETDNS_RETURN_WRONG_TYPE_REQUESTED;
			break;

		}
		_getdns_destroy_item_data(mf, &child_item);
		if (*r) break;
		if (key && key != key_spc) {
			GETDNS_FREE(*mf, key);
			key = NULL;
		}
	}
	if (key && key != key_spc)
		GETDNS_FREE(*mf, key);

	if (*r) {
		getdns_dict_destroy(dict);
		return 0;
	}
	return j;
}

static int _jsmn_get_list(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    size_t count, getdns_list *list, getdns_return_t *r)
{
	int i;
	size_t j = 1, index = 0;
	getdns_item child_item;

	if (t->size <= 0)
		*r = GETDNS_RETURN_GOOD;

	else for (i = 0; i < t->size; i++) {
		j += _jsmn_get_item(mf, js, t + j, count - j, &child_item, r);
		if (*r) break;

		switch (child_item.dtype) {
		case t_int:
			*r = getdns_list_set_int(list, index++,
			    child_item.data.n);
			break;
		case t_bindata:
			*r = getdns_list_set_bindata(list, index++,
			    child_item.data.bindata);
			break;
		case t_list:
			*r = getdns_list_set_list(list, index++,
			    child_item.data.list);
			break;
		case t_dict:
			*r = getdns_list_set_dict(list, index++,
			    child_item.data.dict);
			break;
		default:
			*r = GETDNS_RETURN_WRONG_TYPE_REQUESTED;
			break;

		}
		_getdns_destroy_item_data(mf, &child_item);
		if (*r) break;
	}
	if (*r) {
		getdns_list_destroy(list);
		return 0;
	}
	return j;
}

static int _jsmn_get_item(struct mem_funcs *mf, const char *js, jsmntok_t *t,
    size_t count, getdns_item *item, getdns_return_t *r)
{
	assert(item);

	switch (t->type) {
	case JSMN_STRING:
		if (t->end < t->start)
			*r = GETDNS_RETURN_GENERIC_ERROR;

		else if (!(item->data.bindata =
		    GETDNS_MALLOC(*mf, getdns_bindata)))
			*r = GETDNS_RETURN_MEMORY_ERROR;

		else if (!(item->data.bindata->data = GETDNS_XMALLOC(
		    *mf, uint8_t, t->end - t->start + 1))) {
			GETDNS_FREE(*mf, item->data.bindata);
			*r = GETDNS_RETURN_MEMORY_ERROR;
		} else {
			item->dtype = t_bindata;
			if (t->end - t->start) {
				(void) memcpy(item->data.bindata->data,
				    js + t->start, t->end - t->start);
			}
			item->data.bindata->data[t->end - t->start] = '\0';
			item->data.bindata->size = t->end - t->start;
			*r = GETDNS_RETURN_GOOD;
			return 1;
		}
		break;

	case JSMN_PRIMITIVE:
		/* There is no such thing as an empty primitive */
		if (t->end <= t->start) {
			*r = GETDNS_RETURN_GENERIC_ERROR;
			break;

		} else if (_jsmn_get_int(js, t, &item->data.n)
		    || _jsmn_get_const(js, t, &item->data.n)) {

			item->dtype = t_int;
		}
		else if (_jsmn_get_data(mf, js, t, &item->data.bindata)
		    || _jsmn_get_dname(mf, js, t,  &item->data.bindata)
		    || _jsmn_get_ipv4(mf, js, t,  &item->data.bindata)
		    || _jsmn_get_ipv6(mf, js, t,  &item->data.bindata))

			item->dtype = t_bindata;

		else if (_jsmn_get_ipdict(mf, js, t, &item->data.dict))

			item->dtype = t_dict;
		else {
			*r = GETDNS_RETURN_GENERIC_ERROR;
			break;
		}
		*r = GETDNS_RETURN_GOOD;
		return 1;

	case JSMN_OBJECT:
		if (!(item->data.dict = _getdns_dict_create_with_mf(mf))) {
			*r = GETDNS_RETURN_MEMORY_ERROR;
			break;
		}
		item->dtype = t_dict;
		return _jsmn_get_dict(mf, js, t, count, item->data.dict, r);

	case JSMN_ARRAY:
		if (!(item->data.list = _getdns_list_create_with_mf(mf))) {
			*r = GETDNS_RETURN_MEMORY_ERROR;
			break;
		}
		item->dtype = t_list;
		return _jsmn_get_list(mf, js, t, count, item->data.list, r);

	default:
		*r = GETDNS_RETURN_WRONG_TYPE_REQUESTED;
		break;
	}
	return 0;
}

static getdns_return_t
_getdns_str2item_mf(struct mem_funcs *mf, const char *str, getdns_item *item)
{
	jsmn_parser p;
	jsmntok_t *tok = NULL, *new_tok;
	size_t tokcount = 100;
	int r;
	getdns_return_t gr;

	jsmn_init(&p);
	tok = GETDNS_XMALLOC(*mf, jsmntok_t, tokcount);
	do {
		r = jsmn_parse(&p, str, strlen(str), tok, tokcount);
		if (r == JSMN_ERROR_NOMEM) {
			tokcount *= 2;
			if (!(new_tok = GETDNS_XREALLOC(
			    *mf, tok, jsmntok_t, tokcount))) {
				GETDNS_FREE(*mf, tok);
				return GETDNS_RETURN_MEMORY_ERROR;
			} 
			tok  = new_tok;
		}
	} while (r == JSMN_ERROR_NOMEM);
	if (r < 0) 
		gr = GETDNS_RETURN_GENERIC_ERROR;
	else
		(void) _jsmn_get_item(mf, str, tok, p.toknext, item, &gr);
	GETDNS_FREE(*mf, tok);
	return gr;
}

getdns_return_t
getdns_str2dict(const char *str, getdns_dict **dict)
{
	getdns_item item;
	getdns_return_t r;

	if (!str || !dict)
		return GETDNS_RETURN_INVALID_PARAMETER;

	while (*str && isspace((unsigned char)*str))
		str++;

	if (*str != '{') {
		char value_buf[3072], *value_str = value_buf;

		if (strlen(str) > sizeof(value_str) - 1)
			value_str = strdup(str);
		else
			(void)strncpy(value_buf, str, sizeof(value_buf));

		getdns_dict *dict_r = _getdns_ipaddr_dict_mf(
		    &_getdns_plain_mem_funcs, value_str);

		if (value_str != value_buf)
			free(value_str);

		if (dict_r) {
			*dict = dict_r;
			return GETDNS_RETURN_GOOD;
		}
	}
	if ((r = _getdns_str2item_mf(&_getdns_plain_mem_funcs, str, &item)))
		return r;

	else if (item.dtype != t_dict) {
		uint8_t buf[16];
		getdns_dict *dict_r;

		if (item.dtype != t_bindata)
			r = GETDNS_RETURN_WRONG_TYPE_REQUESTED;

		else if (item.data.bindata->size == 4 &&
		    inet_pton(AF_INET, str, buf) == 1) {

			if (!(dict_r = getdns_dict_create()))
				r = GETDNS_RETURN_MEMORY_ERROR;

			else if ((r = getdns_dict_util_set_string(
			    dict_r, "address_type", "IPv4")))
				getdns_dict_destroy(dict_r);

			else if ((r = getdns_dict_set_bindata(
			    dict_r, "address_data", item.data.bindata)))
				getdns_dict_destroy(dict_r);
			else
				*dict = dict_r;

		} else if (item.data.bindata->size == 16 &&
		    inet_pton(AF_INET6, str, buf) == 1) {

			if (!(dict_r = getdns_dict_create()))
				r = GETDNS_RETURN_MEMORY_ERROR;

			else if ((r = getdns_dict_util_set_string(
			    dict_r, "address_type", "IPv6")))
				getdns_dict_destroy(dict_r);

			else if ((r = getdns_dict_set_bindata(
			    dict_r, "address_data", item.data.bindata)))
				getdns_dict_destroy(dict_r);
			else
				*dict = dict_r;
		} else
			r = GETDNS_RETURN_WRONG_TYPE_REQUESTED;

		_getdns_destroy_item_data(&_getdns_plain_mem_funcs, &item);
		return r;
	}
	*dict = item.data.dict;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_str2list(const char *str, getdns_list **list)
{
	getdns_item item;
	getdns_return_t r;

	if (!str || !list)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_str2item_mf(&_getdns_plain_mem_funcs, str, &item)))
		return r;

	else if (item.dtype != t_list) {
		_getdns_destroy_item_data(&_getdns_plain_mem_funcs, &item);
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;
	}
	*list = item.data.list;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_str2bindata(const char *str, getdns_bindata **bindata)
{
	getdns_item item;
	getdns_return_t r;

	if (!str || !bindata)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_str2item_mf(&_getdns_plain_mem_funcs, str, &item)))
		return r;

	else if (item.dtype != t_bindata) {
		_getdns_destroy_item_data(&_getdns_plain_mem_funcs, &item);
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;
	}
	*bindata = item.data.bindata;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_str2int(const char *str, uint32_t *value)
{
	getdns_item item;
	getdns_return_t r;

	if (!str || !value)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_str2item_mf(&_getdns_plain_mem_funcs, str, &item)))
		return r;

	else if (item.dtype != t_int) {
		_getdns_destroy_item_data(&_getdns_plain_mem_funcs, &item);
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;
	}
	*value = item.data.n;
	return GETDNS_RETURN_GOOD;
}

#ifdef USE_YAML_CONFIG
getdns_return_t
getdns_yaml2dict(const char *str, getdns_dict **dict)
{
	char *jsonstr;
	
	if (!str || !dict)
		return GETDNS_RETURN_INVALID_PARAMETER;

	jsonstr = yaml_string_to_json_string(str);
	if (jsonstr) {
		getdns_return_t res = getdns_str2dict(jsonstr, dict);
		free(jsonstr);
		return res;
	} else {
		return GETDNS_RETURN_GENERIC_ERROR;
	}	
}
#endif /* USE_YAML_CONFIG */

/* WT: I am not certain about the value of yaml2list...
 *     I don't see how yaml2bindata and yaml2int  would be different from
 *     the str2bindata and str2int ones.
 */
#if 0

getdns_return_t
getdns_yaml2list(const char *str, getdns_list **list)
{
#ifdef USE_YAML_CONFIG
	char *jsonstr;
	
	if (!str || !list)
		return GETDNS_RETURN_INVALID_PARAMETER;

	jsonstr = yaml_string_to_json_string(str);
	if (jsonstr) {
		getdns_return_t res = getdns_str2list(jsonstr, list);
		free(jsonstr);
		return res;
	} else {
		return GETDNS_RETURN_GENERIC_ERROR;
	}	
#else /* USE_YAML_CONFIG */
	(void) str;
	(void) list;
	return GETDNS_RETURN_NOT_IMPLEMENTED;
#endif /* USE_YAML_CONFIG */
}

getdns_return_t
getdns_yaml2bindata(const char *str, getdns_bindata **bindata)
{
#ifdef USE_YAML_CONFIG
	char *jsonstr;
	
	if (!str || !bindata)
		return GETDNS_RETURN_INVALID_PARAMETER;

	jsonstr = yaml_string_to_json_string(str);
	if (jsonstr) {
		getdns_return_t res = getdns_str2bindata(jsonstr, bindata);
		free(jsonstr);
		return res;
	} else {
		return GETDNS_RETURN_GENERIC_ERROR;
	}	
#else /* USE_YAML_CONFIG */
	(void) str;
	(void) bindata;
	return GETDNS_RETURN_NOT_IMPLEMENTED;
#endif /* USE_YAML_CONFIG */
}

getdns_return_t
getdns_yaml2int(const char *str, uint32_t *value)
{
#ifdef USE_YAML_CONFIG
	char *jsonstr;
	
	if (!str || !value)
		return GETDNS_RETURN_INVALID_PARAMETER;

	jsonstr = yaml_string_to_json_string(str);
	if (jsonstr) {
		getdns_return_t res = getdns_str2int(jsonstr, value);
		free(jsonstr);
		return res;
	} else {
		return GETDNS_RETURN_GENERIC_ERROR;
	}	
#else /* USE_YAML_CONFIG */
	(void) str;
	(void) value;
	return GETDNS_RETURN_NOT_IMPLEMENTED;
#endif /* USE_YAML_CONFIG */
}
#endif
