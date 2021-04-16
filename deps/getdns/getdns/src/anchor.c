/**
 *
 * /brief functions for DNSSEC trust anchor management
 */
/*
 * Copyright (c) 2017, NLnet Labs, Inc.
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
#include "debug.h"
#include "anchor.h"
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include "types-internal.h"
#include "context.h"
#include "dnssec.h"
#include "yxml/yxml.h"
#include "gldns/parseutil.h"
#include "gldns/gbuffer.h"
#include "gldns/str2wire.h"
#include "gldns/wire2str.h"
#include "gldns/pkthdr.h"
#include "gldns/keyraw.h"
#include "general.h"
#include "util-internal.h"
#include "platform.h"

typedef struct ta_iter {
	uint8_t yxml_buf[4096];
	yxml_t x;

	const char *start;
	const char *ptr;
	const char *end;

	char  zone[1024];

	time_t validFrom;
	time_t validUntil;

	char  keytag[6];
	char  algorithm[4];
	char  digesttype[4];
	char  digest[2048];
} ta_iter;

static void strcpytrunc(char* dst, const char* src, size_t dstsize)
{
	size_t to_copy = strlen(src);
	if (to_copy >= dstsize)
		to_copy = dstsize -1;
	memcpy(dst, src, to_copy);
	dst[to_copy] = '\0';
}

/**
 * XML convert DateTime element to time_t.
 * [-]CCYY-MM-DDThh:mm:ss[Z|(+|-)hh:mm]
 * (with optional .ssssss fractional seconds)
 * @param str: the string
 * @return a time_t representation or 0 on failure.
 */
time_t
_getdns_xml_convertdate(const char* str)
{
	time_t t = 0;
	struct tm tm;
	const char* s;
	/* for this application, ignore minus in front;
	 * only positive dates are expected */
	s = str;
	if(s[0] == '-') s++;
	memset(&tm, 0, sizeof(tm));
	/* parse initial content of the string (lots of whitespace allowed) */
	s = strptime(s, "%t%Y%t-%t%m%t-%t%d%tT%t%H%t:%t%M%t:%t%S%t", &tm);
	if(!s) {
		DEBUG_ANCHOR("xml_convertdate parse failure %s\n", str);
		return 0;
	}
	/* parse remainder of date string */
	if(*s == '.') {
		/* optional '.' and fractional seconds */
		int frac = 0, n = 0;
		if(sscanf(s+1, "%d%n", &frac, &n) < 1) {
			DEBUG_ANCHOR("xml_convertdate f failure %s\n", str);
			return 0;
		}
		/* fraction is not used, time_t has second accuracy */
		s++;
		s+=n;
	}
	if(*s == 'Z' || *s == 'z') {
		/* nothing to do for this */
		s++;
	} else if(*s == '+' || *s == '-') {
		/* optional timezone spec: Z or +hh:mm or -hh:mm */
		int hr = 0, mn = 0, n = 0;
		if(sscanf(s+1, "%d:%d%n", &hr, &mn, &n) < 2) {
			DEBUG_ANCHOR("xml_convertdate tz failure %s\n", str);
			return 0;
		}
		if(*s == '+') {
			tm.tm_hour += hr;
			tm.tm_min += mn;
		} else {
			tm.tm_hour -= hr;
			tm.tm_min -= mn;
		}
		s++;
		s += n;
	}
	if(*s != 0) {
		/* not ended properly */
		/* but ignore, (lenient) */
	}

	t = gldns_mktime_from_utc(&tm);
	if(t == (time_t)-1) {
		DEBUG_ANCHOR("xml_convertdate mktime failure\n");
		return 0;
	}
	return t;
}


static inline int ta_iter_done(ta_iter *ta)
{ return *ta->ptr == 0 || ta->ptr >= ta->end; }

static ta_iter *ta_iter_next(ta_iter *ta)
{
	yxml_ret_t r = YXML_OK;
	yxml_t ta_x;
	const char *ta_start;
	int level;
	char value[2048];
	char *cur, *tmp;
	enum { VALIDFROM, VALIDUNTIL } attr_type;
	enum { KEYTAG, ALGORITHM, DIGESTTYPE, DIGEST } elem_type;

	cur = value;
	value[0] = 0;

	if (!ta->zone[0]) {
		DEBUG_ANCHOR("Determine start of <TrustAnchor>\n");
		/* Determine start of <TrustAnchor> */
		while (!ta_iter_done(ta) &&
		    (  yxml_parse(&ta->x, *ta->ptr) != YXML_ELEMSTART
		    || strcasecmp(ta->x.elem, "trustanchor")))
			ta->ptr++;
		if (ta_iter_done(ta)) return NULL;
		ta_start = ta->ptr;
		ta_x = ta->x;

		DEBUG_ANCHOR("Find <Zone>\n");
		/* Find <Zone> */
		level = 0;
		while (!ta_iter_done(ta) && !ta->zone[0]) {
			switch ((r = yxml_parse(&ta->x, *ta->ptr))) {
			case YXML_ELEMSTART:
				level += 1;
				if (level == 1 &&
				    strcasecmp(ta->x.elem, "zone") == 0) {
					cur = value;
					*cur = 0;
				}
				break;

			case YXML_ELEMEND:
				level -= 1;
				if (level < 0)
					/* End of <TrustAnchor> section,
					 * try the next <TrustAnchor> section
					 */
					return ta_iter_next(ta);

				else if (level == 0 && cur) {
					/* <Zone> content ready */
					strcpytrunc( ta->zone, value
						     , sizeof(ta->zone));

					/* Reset to start of <TrustAnchor> */
					cur = NULL;
					ta->ptr = ta_start;
					ta->x = ta_x;
				}
				break;

			case YXML_CONTENT:
				if (!cur || level != 1)
					break;
				tmp = ta->x.data;
				while (*tmp && cur < value + sizeof(value))
					*cur++ = *tmp++;
				if (cur >= value + sizeof(value))
					cur = NULL;
				else
					*cur = 0;
				break;
			default:
				break;
			}
			ta->ptr++;
		}
		if (ta_iter_done(ta))
			return NULL;
	}
	assert(ta->zone[0]);

	DEBUG_ANCHOR("Zone: %s, Find <KeyDigest>\n", ta->zone);
	level = 0;
	while (!ta_iter_done(ta)) {
		r = yxml_parse(&ta->x, *ta->ptr);

		if (r == YXML_ELEMSTART) {
			level += 1;
			DEBUG_ANCHOR("elem start: %s, level: %d\n", ta->x.elem, level);
			if (level == 1 &&
			    strcasecmp(ta->x.elem, "keydigest") == 0)
				break;

		} else if (r == YXML_ELEMEND) {
			level -= 1;
			if (level < 0) {
				/* End of <TrustAnchor> section */
				ta->zone[0] = 0;
				return ta_iter_next(ta);
			}
		}
		ta->ptr++;
	}
	if (ta_iter_done(ta))
		return NULL;

	DEBUG_ANCHOR("Found <KeyDigest>, Parse attributes\n");

	ta->validFrom = ta->validUntil = 0;
	*ta->keytag = *ta->algorithm = *ta->digesttype = *ta->digest = 0;

	cur = NULL;
	value[0] = 0;
	attr_type = -1;

	while (!ta_iter_done(ta)) {
		switch ((r = yxml_parse(&ta->x, *ta->ptr))) {
		case YXML_ELEMSTART:
			break;

		case YXML_ELEMEND:
			/* End of <KeyDigest> section, try next */
			return ta_iter_next(ta);

		case YXML_ATTRSTART:
			DEBUG_ANCHOR("attrstart: %s\n", ta->x.attr);
			if (strcasecmp(ta->x.attr, "validfrom") == 0)
				attr_type = VALIDFROM;

			else if (strcasecmp(ta->x.attr, "validuntil") == 0)
				attr_type = VALIDUNTIL;
			else
				break;

			cur = value;
			*cur = 0;
			break;

		case YXML_ATTREND:
			if (!cur)
				break;
			cur = NULL;
			DEBUG_ANCHOR("attrval: %s\n", value);
			switch (attr_type) {
			case VALIDFROM:
				ta->validFrom = _getdns_xml_convertdate(value);
				break;
			case VALIDUNTIL:
				ta->validUntil = _getdns_xml_convertdate(value);
				break;
			}
			break;

		case YXML_ATTRVAL:
			if (!cur)
				break;
			tmp = ta->x.data;
			while (*tmp && cur < value + sizeof(value))
				*cur++ = *tmp++;
			if (cur >= value + sizeof(value))
				cur = NULL;
			else
				*cur = 0;
			break;
		case YXML_OK:
		case YXML_CONTENT:
			break;
		default:
			DEBUG_ANCHOR("r: %d\n", (int)r);
			return NULL;
			break;
		}
		if (r == YXML_ELEMSTART)
			break;
		ta->ptr++;
	}
	if (ta_iter_done(ta))
		return NULL;

	assert(r == YXML_ELEMSTART);
	DEBUG_ANCHOR("Within <KeyDigest>, Parse child elements\n");

	cur = NULL;
	value[0] = 0;
	elem_type = -1;

	for (;;) {
		switch (r) {
		case YXML_ELEMSTART:
			level += 1;
			DEBUG_ANCHOR("elem start: %s, level: %d\n", ta->x.elem, level);
			if (level != 2)
				break;

			else if (strcasecmp(ta->x.elem, "keytag") == 0)
				elem_type = KEYTAG;

			else if (strcasecmp(ta->x.elem, "algorithm") == 0)
				elem_type = ALGORITHM;

			else if (strcasecmp(ta->x.elem, "digesttype") == 0)
				elem_type = DIGESTTYPE;

			else if (strcasecmp(ta->x.elem, "digest") == 0)
				elem_type = DIGEST;
			else
				break;

			cur = value;
			*cur = 0;
			break;

		case YXML_ELEMEND:
			level -= 1;
			if (level < 0) {
				/* End of <TrustAnchor> section */
				ta->zone[0] = 0;
				return ta_iter_next(ta);

			} else if (level != 1 || !cur)
				break;

			cur = NULL;
			DEBUG_ANCHOR("elem end: %s\n", value);
			switch (elem_type) {
			case KEYTAG:
				strcpytrunc( ta->keytag, value
					     , sizeof(ta->keytag));
				break;
			case ALGORITHM:
				strcpytrunc( ta->algorithm, value
					     , sizeof(ta->algorithm));
				break;
			case DIGESTTYPE:
				strcpytrunc( ta->digesttype, value
					     , sizeof(ta->digesttype));
				break;
			case DIGEST:
				strcpytrunc( ta->digest, value
					     , sizeof(ta->digest));
				break;
			}
			break;

		case YXML_CONTENT:
			if (!cur)
				break;
			tmp = ta->x.data;
			while (*tmp && cur < value + sizeof(value))
				*cur++ = *tmp++;
			if (cur >= value + sizeof(value))
				cur = NULL;
			else
				*cur = 0;
			break;

		default:
			break;
		}
		if (level == 0)
			break;
		ta->ptr++;
		if (ta_iter_done(ta))
			return NULL;
		r = yxml_parse(&ta->x, *ta->ptr);
	}
	return  ta->validFrom
	    && *ta->keytag     && *ta->algorithm
	    && *ta->digesttype && *ta->digest ? ta : ta_iter_next(ta);
}

static ta_iter *ta_iter_init(ta_iter *ta, const char *doc, size_t doc_len)
{
	ta->ptr = ta->start = doc;
	ta->end = ta->start + doc_len;
	yxml_init(&ta->x, ta->yxml_buf, sizeof(ta->yxml_buf));
	ta->zone[0] = 0;
	return ta_iter_next(ta);
}

uint16_t _getdns_parse_xml_trust_anchors_buf(
    gldns_buffer *gbuf, uint64_t *now_ms, char *xml_data, size_t xml_len)
{
	ta_iter ta_spc, *ta;
	uint16_t ta_count = 0;
	size_t pkt_start = gldns_buffer_position(gbuf);

	/* Empty header */
	gldns_buffer_write_u32(gbuf, 0);
	gldns_buffer_write_u32(gbuf, 0);
	gldns_buffer_write_u32(gbuf, 0);

	for ( ta = ta_iter_init(&ta_spc, (char *)xml_data, xml_len)
	    ; ta; ta = ta_iter_next(ta)) {

		if (*now_ms == 0) *now_ms = _getdns_get_now_ms();
		if ((time_t)(*now_ms / 1000) < ta->validFrom)
			DEBUG_ANCHOR("Disregarding trust anchor "
			    "%s for %s which is not yet valid",
			    ta->keytag, ta->zone);

		else if (ta->validUntil != 0
		     && (time_t)(*now_ms / 1000) > ta->validUntil)
			DEBUG_ANCHOR("Disregarding trust anchor "
			    "%s for %s which is not valid anymore",
			    ta->keytag, ta->zone);

		else {
			uint8_t zone[256];
			size_t zone_len = sizeof(zone);
			uint8_t digest[sizeof(ta->digest)/2];
			size_t digest_len = sizeof(digest);
			uint16_t keytag;
			uint8_t algorithm;
			uint8_t digesttype;
			char *endptr;

			DEBUG_ANCHOR( "Installing trust anchor: "
			    "%s IN DS %s %s %s %s\n"
			    , ta->zone
			    , ta->keytag
			    , ta->algorithm
			    , ta->digesttype
			    , ta->digest
			    );
			if (gldns_str2wire_dname_buf(ta->zone, zone, &zone_len)) {
				DEBUG_ANCHOR("Not installing trust anchor because "
				    "of unparsable zone: \"%s\"", ta->zone);
				continue;
			}
			keytag = (uint16_t)strtol(ta->keytag, &endptr, 10);
			if (endptr == ta->keytag || *endptr != 0) {
				DEBUG_ANCHOR("Not installing trust anchor because "
				    "of unparsable keytag: \"%s\"", ta->keytag);
				continue;
			}
			algorithm = (uint16_t)strtol(ta->algorithm, &endptr, 10);
			if (endptr == ta->algorithm || *endptr != 0) {
				DEBUG_ANCHOR("Not installing trust anchor because "
				    "of unparsable algorithm: \"%s\"", ta->algorithm);
				continue;
			}
			digesttype = (uint16_t)strtol(ta->digesttype, &endptr, 10);
			if (endptr == ta->digesttype || *endptr != 0) {
				DEBUG_ANCHOR("Not installing trust anchor because "
				    "of unparsable digesttype: \"%s\"", ta->digesttype);
				continue;
			}
			if (gldns_str2wire_hex_buf(ta->digest, digest, &digest_len)) {
				DEBUG_ANCHOR("Not installing trust anchor because "
				    "of unparsable digest: \"%s\"", ta->digest);
				continue;
			}
			gldns_buffer_write(gbuf, zone, zone_len);
			gldns_buffer_write_u16(gbuf, GETDNS_RRTYPE_DS);
			gldns_buffer_write_u16(gbuf, GETDNS_RRCLASS_IN);
			gldns_buffer_write_u32(gbuf, 3600);
			gldns_buffer_write_u16(gbuf, digest_len + 4); /* rdata_len */
			gldns_buffer_write_u16(gbuf, keytag);
			gldns_buffer_write_u8(gbuf, algorithm);
			gldns_buffer_write_u8(gbuf, digesttype);
			gldns_buffer_write(gbuf, digest, digest_len);
			ta_count += 1;
		}
	}
	gldns_buffer_write_u16_at(gbuf, pkt_start+GLDNS_ANCOUNT_OFF, ta_count);
	return ta_count;
}

static const char tas_write_p7s_buf[] =
"GET %s HTTP/1.1\r\n"
"Host: %s\r\n"
"\r\n";

static const char tas_write_xml_p7s_buf[] =
"GET %s HTTP/1.1\r\n"
"Host: %s\r\n"
"\r\n"
"GET %s HTTP/1.1\r\n"
"Host: %s\r\n"
"\r\n";


static inline const char * rt_str(uint16_t rt)
{ return rt == GETDNS_RRTYPE_A ? "A" : rt == GETDNS_RRTYPE_AAAA ? "AAAA" : "?"; }

static int tas_busy(tas_connection *a)
{
	return a && a->req != NULL;
}

static int tas_fetching(tas_connection *a)
{
	return a->fd >= 0;
}

static void tas_rinse(getdns_context *context, tas_connection *a)
{
	if (a->event.ev)
		GETDNS_CLEAR_EVENT(a->loop, &a->event);
	a->event.ev = NULL;
	if (a->fd >= 0)
		close(a->fd);
	a->fd = -1;
	if (a->xml.data)
		GETDNS_FREE(context->mf, a->xml.data);
	a->xml.data = NULL;
	a->xml.size = 0;
	if (a->tcp.read_buf && a->tcp.read_buf != context->tas_hdr_spc)
		GETDNS_FREE(context->mf, a->tcp.read_buf);
	a->tcp.read_buf = NULL;
}

static void tas_cleanup(getdns_context *context, tas_connection *a)
{
	tas_rinse(context, a);
	if (a->req)
		_getdns_context_cancel_request(a->req->owner);
	if (a->http)
		GETDNS_FREE(context->mf, (void *)a->http);
	(void) memset(a, 0, sizeof(*a));
	a->fd = -1;
}

static void tas_success(getdns_context *context, tas_connection *a)
{
	tas_connection *other = &context->a == a ? &context->aaaa : &context->a;

	tas_cleanup(context, a);
	tas_cleanup(context, other);

	_getdns_log( &context->log, GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_INFO
	           , "Successfully fetched new trust anchors\n");
	context->trust_anchors_source = GETDNS_TASRC_XML;
	_getdns_ta_notify_dnsreqs(context);
}

static void tas_fail(getdns_context *context, tas_connection *a)
{
	tas_connection *other = &context->a == a ? &context->aaaa : &context->a;
	uint16_t rt = &context->a == a ? GETDNS_RRTYPE_A : GETDNS_RRTYPE_AAAA;

	tas_cleanup(context, a);

	if (!tas_busy(other)) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
			   , "Fatal error fetching trust anchor: "
		             "%s connection failed too\n", rt_str(rt));
		context->trust_anchors_source = GETDNS_TASRC_FAILED;
		context->trust_anchors_backoff_expiry = 
		    _getdns_get_now_ms() + context->trust_anchors_backoff_time;
		_getdns_ta_notify_dnsreqs(context);
	} else
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_WARNING
			   , "%s connection failed, waiting for %s\n"
		            , rt_str(rt)
			    , rt_str( rt == GETDNS_RRTYPE_A 
				    ? GETDNS_RRTYPE_AAAA : GETDNS_RRTYPE_A));
}

static void tas_connect(getdns_context *context, tas_connection *a);
static void tas_next(getdns_context *context, tas_connection *a)
{
	tas_connection *other = a == &context->a ? &context->aaaa : &context->a;

	DEBUG_ANCHOR("Try next address\n");

	if (a->rr) {
		if (!(a->rr = _getdns_rrtype_iter_next(a->rr)))
			tas_fail(context, a);
		else	tas_rinse(context, a);
	}
	if (other->rr)
		tas_connect(context, other);

	else if (a->rr)
		tas_connect(context, a);
}

static void tas_timeout_cb(void *userarg)
{
	getdns_dns_req *dnsreq = (getdns_dns_req *)userarg;
	getdns_context *context = (getdns_context *)dnsreq->user_pointer;
	tas_connection *a;

	if (dnsreq->netreqs[0]->request_type == GETDNS_RRTYPE_A)
		a = &context->a;
	else	a = &context->aaaa;

	_getdns_log( &context->log, GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_WARNING
	           , "Trust anchor fetch timeout\n");

	GETDNS_CLEAR_EVENT(a->loop, &a->event);
	tas_next(context, a);
}


static void tas_reconnect_cb(void *userarg)
{
	getdns_dns_req *dnsreq = (getdns_dns_req *)userarg;
	getdns_context *context = (getdns_context *)dnsreq->user_pointer;
	tas_connection *a;

	if (dnsreq->netreqs[0]->request_type == GETDNS_RRTYPE_A)
		a = &context->a;
	else	a = &context->aaaa;

	_getdns_log( &context->log, GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_DEBUG
	           , "Waiting for second document timeout. Reconnecting...\n");

	GETDNS_CLEAR_EVENT(a->loop, &a->event);
	close(a->fd);
	a->fd = -1;
	if (a->state == TAS_READ_PS7_HDR) {
		a->state = TAS_RETRY;
		tas_connect(context, a);
	} else
		tas_next(context, a);
}

static void tas_read_cb(void *userarg);
static void tas_write_cb(void *userarg);
static void tas_doc_read(getdns_context *context, tas_connection *a)
{
	assert(a->tcp.read_pos == a->tcp.read_buf + a->tcp.read_buf_len);
	assert(context);

	if (a->state == TAS_READ_XML_DOC) {
		if (a->xml.data)
			GETDNS_FREE(context->mf, a->xml.data);
		a->xml.data = a->tcp.read_buf;
		a->xml.size = a->tcp.read_buf_len;
	} else
		assert(a->state == TAS_READ_PS7_DOC ||
		       a->state == TAS_RETRY_PS7_DOC);

	a->state += 1;
	GETDNS_CLEAR_EVENT(a->loop, &a->event);
	if (a->state == TAS_DONE || a->state == TAS_RETRY_DONE) {
		getdns_bindata p7s_bd;
		uint8_t *tas = context->trust_anchors_spc;
		size_t tas_len = sizeof(context->trust_anchors_spc);
		const char *verify_email = NULL;
		getdns_bindata verify_CA;
		getdns_return_t r;
		uint64_t now_ms = 0;

		p7s_bd.data = a->tcp.read_buf;
		p7s_bd.size = a->tcp.read_buf_len;

		if ((r = getdns_context_get_trust_anchors_verify_CA(
		    context, (const char **)&verify_CA.data)))
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
				   , "Cannot get trust anchor verify CA: "
				     "\"%s\"\n", getdns_get_errorstr_by_id(r));

		else if (!(verify_CA.size = strlen((const char *)verify_CA.data)))
			; /* pass */

		else if ((r = getdns_context_get_trust_anchors_verify_email(
		    context, &verify_email)))
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
				   , "Cannot get trust anchor verify email: "
				     "\"%s\"\n", getdns_get_errorstr_by_id(r));

		else if (!(tas = _getdns_tas_validate(&context->mf, &a->xml, &p7s_bd,
		    &verify_CA, verify_email, &now_ms, tas, &tas_len)))
			; /* pass */

		else {
			context->trust_anchors = tas;
			context->trust_anchors_len = tas_len;
			(void) _getdns_context_write_priv_file(
			    context, "root-anchors.xml", &a->xml);
			(void) _getdns_context_write_priv_file(
			    context, "root-anchors.p7s", &p7s_bd);
			tas_success(context, a);
			return;
		}
		tas_fail(context, a);
		return;
	}
	/* First try to read signatures immediately */
	a->state += 1;
	assert(a->state == TAS_READ_PS7_HDR);
	a->tcp.read_buf = context->tas_hdr_spc;
	a->tcp.read_buf_len = sizeof(context->tas_hdr_spc);

	/* Check for surplus read bytes, for the P7S headers */
	if (a->tcp.to_read > 0) {
		a->tcp.read_pos = a->tcp.read_buf + a->tcp.to_read;
		a->tcp.to_read  = sizeof(context->tas_hdr_spc)
		                                  - a->tcp.to_read;
	} else {
		a->tcp.read_pos = a->tcp.read_buf;
		a->tcp.to_read = sizeof(context->tas_hdr_spc);
	}
	GETDNS_SCHEDULE_EVENT(a->loop, a->fd, 2000,
	    getdns_eventloop_event_init(&a->event, a->req->owner,
	    tas_read_cb, NULL, tas_reconnect_cb));
	return;
}

static void tas_read_cb(void *userarg)
{
	getdns_dns_req *dnsreq = (getdns_dns_req *)userarg;
	getdns_context *context = (getdns_context *)dnsreq->user_pointer;
	tas_connection *a;
	ssize_t n, i;

	if (dnsreq->netreqs[0]->request_type == GETDNS_RRTYPE_A)
		a = &context->a;
	else	a = &context->aaaa;

	DEBUG_ANCHOR( "state: %d, to_read: %d\n"
	            , (int)a->state, (int)a->tcp.to_read);

#ifdef USE_WINSOCK
	n = recv(a->fd, (char *)a->tcp.read_pos, a->tcp.to_read, 0);
#else
	n = read(a->fd, a->tcp.read_pos, a->tcp.to_read);
#endif
	if (n == 0) {
		DEBUG_ANCHOR("Connection closed\n");
		GETDNS_CLEAR_EVENT(a->loop, &a->event);
		close(a->fd);
		a->fd = -1;
		if (a->state == TAS_READ_PS7_HDR) {
			a->state = TAS_RETRY;
			tas_connect(context, a);
		} else
			tas_next(context, a);
		return;

	} else if (n > 0 && (  a->state == TAS_READ_XML_DOC
	              || a->state == TAS_READ_PS7_DOC
		      || a->state == TAS_RETRY_PS7_DOC)) {

		assert(n <= (ssize_t)a->tcp.to_read);

		DEBUG_ANCHOR("read: %d bytes at %p, for doc %p of size %d\n",
		    (int)n, (void *)a->tcp.read_pos
		          , (void *)a->tcp.read_buf, (int)a->tcp.read_buf_len);
		a->tcp.read_pos += n;
		a->tcp.to_read -= n;
		if (a->tcp.to_read == 0)
			tas_doc_read(context, a);
		return;

	} else if (n > 0) {
		ssize_t p = 0;
		int doc_len = -1;
		int len;
		char *ln;
		char *endptr;

		n += a->tcp.read_pos - a->tcp.read_buf;
		for (i = 0; i < (n - 1); i++) {
			if (a->tcp.read_buf[i] != '\r' ||
			    a->tcp.read_buf[i+1] != '\n')
				continue;

			len = (int)(i - p);
			ln = (char *)&a->tcp.read_buf[p];

			DEBUG_ANCHOR("line: \"%.*s\"\n", len, ln);
			if (len >= 16 &&
			    !strncasecmp(ln, "Content-Length: ", 16)) {
				ln[len] = 0;
				doc_len = (int)strtol(ln + 16, &endptr , 10);
				if (endptr == ln || *endptr != 0)
					doc_len = -1;
			}
			if (i - p == 0) {
				i += 2;
				break;
			}
			p = i + 2;
			i++;
		}
		if (doc_len > 0) {
			uint8_t *doc = GETDNS_XMALLOC(
			    context->mf, uint8_t, doc_len + 1);
			doc[doc_len] = 0;

			DEBUG_ANCHOR("i: %d, n: %d, doc_len: %d\n"
			            , (int)i, (int)n, doc_len);
			if (!doc)
				_getdns_log( &context->log
					   , GETDNS_LOG_SYS_ANCHOR
					   , GETDNS_LOG_ERR
					   , "Memory error while reading "
					     "trust anchor\n");
			else {
				ssize_t surplus = n - i;

				a->state += 1;
				/* With pipelined read, the buffer might
				 * contain the full document, plus a piece
				 * of the headers of the next document!
				 * Currently context->tas_hdr_spc is kept
				 * small enough to anticipate this.
				 */
				if (surplus <= 0) {
					a->tcp.read_pos = doc;
					a->tcp.to_read = doc_len;
				} else if (surplus > doc_len) {
					(void) memcpy(
					    doc, a->tcp.read_buf + i, doc_len);
					a->tcp.read_pos = doc + doc_len;

					/* Special value to indicate a begin
					 * of the next reply is already
					 * present.  Detectable by:
					 * (read_pos == read_buf + read_buf_len)
					 * && to_read > 0;
					 */
					a->tcp.to_read = surplus - doc_len;
					(void) memmove(a->tcp.read_buf,
					    a->tcp.read_buf + i + doc_len,
					    surplus - doc_len);
				} else {
					assert(surplus <= doc_len);
					(void) memcpy(
					    doc, a->tcp.read_buf + i, surplus);
					a->tcp.read_pos = doc + surplus;
					a->tcp.to_read = doc_len - surplus;
				}
				a->tcp.read_buf = doc;
				a->tcp.read_buf_len = doc_len;

				if (a->tcp.read_pos == doc + doc_len)
					tas_doc_read(context, a);
				return;
			}
		}
	} else if (_getdns_socketerror_wants_retry())
		return;

	_getdns_log( &context->log
		   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
		   , "Error while receiving trust anchor: %s\n"
		   , _getdns_errnostr());

	GETDNS_CLEAR_EVENT(a->loop, &a->event);
	tas_next(context, a);
}

static void tas_write_cb(void *userarg)
{
	getdns_dns_req *dnsreq = (getdns_dns_req *)userarg;
	getdns_context *context = (getdns_context *)dnsreq->user_pointer;
	tas_connection *a;
	ssize_t written;

	if (dnsreq->netreqs[0]->request_type == GETDNS_RRTYPE_A)
		a = &context->a;
	else	a = &context->aaaa;

	DEBUG_ANCHOR( "state: %d, to_write: %d\n"
	            , (int)a->state, (int)a->tcp.write_buf_len);


#ifdef USE_WINSOCK
	DEBUG_ANCHOR("sending to: %d\n", a->fd);
	written = send(a->fd, (const char *)a->tcp.write_buf, a->tcp.write_buf_len, 0);
#else
	written = write(a->fd, a->tcp.write_buf, a->tcp.write_buf_len);
#endif
	if (written >= 0) {
		assert(written <= (ssize_t)a->tcp.write_buf_len);

		a->tcp.write_buf += written;
		a->tcp.write_buf_len -= written;
		if (a->tcp.write_buf_len > 0)
			/* Write remainder */
			return;

		a->state += 1;
		a->tcp.read_buf = context->tas_hdr_spc;
		a->tcp.read_buf_len = sizeof(context->tas_hdr_spc);
		a->tcp.read_pos = a->tcp.read_buf;
		a->tcp.to_read = sizeof(context->tas_hdr_spc);
		GETDNS_CLEAR_EVENT(a->loop, &a->event);
		DEBUG_ANCHOR("All written, schedule read\n");
		GETDNS_SCHEDULE_EVENT(a->loop, a->fd, 2000,
		    getdns_eventloop_event_init(&a->event, a->req->owner,
		    tas_read_cb, NULL, tas_timeout_cb));
		return;

	} else if (_getdns_socketerror_wants_retry())
		return;

	_getdns_log( &context->log, GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
		   , "Error while sending to trust anchor site: %s\n"
		   , _getdns_errnostr());
	GETDNS_CLEAR_EVENT(a->loop, &a->event);
	tas_next(context, a);
}

static getdns_return_t _getdns_get_tas_url_hostname(
    getdns_context *context, char *hostname, const char **path)
{
	getdns_return_t r;
	const char *url;
	char *next_slash;
	size_t s;

	if ((r = getdns_context_get_trust_anchors_url(context, &url)))
		return r;

	if ((next_slash = strchr(url + 7 /* "http://" */, '/'))) {
		if (next_slash - url - 7 > 254)
			return GETDNS_RETURN_NO_SUCH_LIST_ITEM;
		if (path)
			*path = next_slash;
		strncpy(hostname, url + 7, next_slash - url - 7);
		hostname[next_slash - url - 7] = 0;
	} else  {
		if (path)
			*path = url + strlen(url);
		strncpy(hostname, url + 7, 254);
		hostname[254] = 0;
	}
	s = strlen(hostname);
	if (s && s < 255 && hostname[s - 1] != '.') {
		hostname[s] = '.';
		hostname[s+1] = '\0';
	}
	return GETDNS_RETURN_GOOD;
}

static void tas_connect(getdns_context *context, tas_connection *a)
{
	char a_buf[40];
	int r;

#ifdef HAVE_FCNTL
	int flag;
#elif defined(HAVE_IOCTLSOCKET)
	unsigned long on = 1;
#endif

	if (a->rr->rr_i.nxt - (a->rr->rr_i.rr_type + 10) !=
	    ( a->req->request_type == GETDNS_RRTYPE_A    ?  4
	    : a->req->request_type == GETDNS_RRTYPE_AAAA ? 16 : -1)) {

		tas_next(context, a);
		return;
	}

	_getdns_log( &context->log, GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_DEBUG
		   , "Setting op connection to: %s\n"
		   , inet_ntop( ( a->req->request_type == GETDNS_RRTYPE_A
	                        ? AF_INET : AF_INET6)
	                      , a->rr->rr_i.rr_type + 10
	                      , a_buf, sizeof(a_buf)));

	if ((a->fd = socket(( a->req->request_type == GETDNS_RRTYPE_A
	    ? AF_INET : AF_INET6), SOCK_STREAM, IPPROTO_TCP)) == -1) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
			   , "Error creating socket: %s\n", _getdns_errnostr());
		tas_next(context, a);
		return;
	}
#ifdef HAVE_FCNTL
	if((flag = fcntl(a->fd, F_GETFL)) != -1) {
		flag |= O_NONBLOCK;
		if(fcntl(a->fd, F_SETFL, flag) == -1) {
			/* ignore error, continue blockingly */
		}
	}
#elif defined(HAVE_IOCTLSOCKET)
	if(ioctlsocket(a->fd, FIONBIO, &on) != 0) {
		/* ignore error, continue blockingly */
	}
#endif
	if (a->req->request_type == GETDNS_RRTYPE_A) {
		struct sockaddr_in addr;

		addr.sin_family = AF_INET;
		addr.sin_port = htons(80);
		(void) memcpy(&addr.sin_addr, a->rr->rr_i.rr_type + 10, 4);
		r = connect(a->fd, (struct sockaddr *)&addr, sizeof(addr));
	} else {
		struct sockaddr_in6 addr;

		addr.sin6_family = AF_INET6;
		addr.sin6_port = htons(80);
		addr.sin6_flowinfo = 0;
		(void) memcpy(&addr.sin6_addr, a->rr->rr_i.rr_type + 10, 16);
		addr.sin6_scope_id = 0;
		r = connect(a->fd, (struct sockaddr *)&addr, sizeof(addr));
	}
	if (r == 0 || (r == -1 && (_getdns_socketerror() == _getdns_EINPROGRESS ||
	                           _getdns_socketerror() == _getdns_EWOULDBLOCK))) {
		char tas_hostname[256];
		const char *path = "", *fmt;
		getdns_return_t R;
		char *write_buf;
		size_t buf_sz, path_len, hostname_len;

		a->state += 1;
		if (a->http) {
			GETDNS_FREE(context->mf, (void *)a->http);
			a->http = NULL;
			a->tcp.write_buf = NULL;
			a->tcp.write_buf_len = 0;
			a->tcp.written = 0;
		}
		if ((R = _getdns_get_tas_url_hostname(
		    context, tas_hostname, &path))) {
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
				   , "Cannot get hostname from trust anchor "
				     "url: \"%s\"\n"
				   , getdns_get_errorstr_by_id(r));
			goto error;
		}
		hostname_len = strlen(tas_hostname);
		if (hostname_len > 0 && tas_hostname[hostname_len - 1] == '.')
			tas_hostname[--hostname_len] = '\0';
		path_len = strlen(path);
		if (path_len < 4) {
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
				   , "Trust anchor path \"%s\" too small\n"
				   , path);
			goto error;
		}
		if (a->state == TAS_RETRY_GET_PS7) {
			buf_sz = sizeof(tas_write_p7s_buf)
			       + 1 * (hostname_len - 2) + 1 * (path_len - 2);
			fmt = tas_write_p7s_buf;
		} else {
			buf_sz = sizeof(tas_write_xml_p7s_buf)
			       + 2 * (hostname_len - 2) + 2 * (path_len - 2);
			fmt = tas_write_xml_p7s_buf;
		}
		if (!(write_buf = GETDNS_XMALLOC(context->mf, char, buf_sz))) {
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
				   , "Cannot allocate write buffer for "
				     "sending to trust anchor host\n");
			goto error;
		}
		if (a->state == TAS_RETRY_GET_PS7) {
			(void) snprintf( write_buf, buf_sz, fmt
			               , path, tas_hostname);
			write_buf[4 + path_len - 3] =
			write_buf[4 + path_len - 3] == 'X' ? 'P' : 'p';
			write_buf[4 + path_len - 2] =              '7';
			write_buf[4 + path_len - 1] = 
			write_buf[4 + path_len - 1] == 'L' ? 'S' : 's';
		} else {
			(void) snprintf( write_buf, buf_sz, fmt
			               , path, tas_hostname
			               , path, tas_hostname);
			write_buf[29 + hostname_len + 2 * path_len - 3] = 
			write_buf[29 + hostname_len + 2 * path_len - 3] == 'X'
			                                           ? 'P' : 'p';
			write_buf[29 + hostname_len + 2 * path_len - 2] =  '7';
			write_buf[29 + hostname_len + 2 * path_len - 1] =
			write_buf[29 + hostname_len + 2 * path_len - 1] == 'L'
			                                           ? 'S' : 's';
		}
		DEBUG_ANCHOR("Write buffer: \"%s\"\n", write_buf);
		a->tcp.write_buf = (uint8_t *)(a->http = write_buf);
		a->tcp.write_buf_len = buf_sz - 1;
		a->tcp.written = 0;

		GETDNS_SCHEDULE_EVENT(a->loop, a->fd, 2000,
		    getdns_eventloop_event_init(&a->event, a->req->owner,
		    NULL, tas_write_cb, tas_timeout_cb));
		DEBUG_ANCHOR("Scheduled write with event\n");
		return;
	} else
		_getdns_log( &context->log
			   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
			   , "Error connecting to trust anchor host: %s\n "
			   , _getdns_errnostr());
error:
	tas_next(context, a);
}

static void tas_happy_eyeballs_cb(void *userarg)
{
	getdns_dns_req *dnsreq = (getdns_dns_req *)userarg;
	getdns_context *context = (getdns_context *)dnsreq->user_pointer;

	assert(dnsreq->netreqs[0]->request_type == GETDNS_RRTYPE_A);
	if (tas_fetching(&context->aaaa))
		return;
	else {
		_getdns_log( &context->log
			   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_DEBUG
			   , "Too late reception of AAAA for trust anchor "
			     "host for Happy Eyeballs\n");
		GETDNS_CLEAR_EVENT(context->a.loop, &context->a.event);
		tas_connect(context, &context->a);
	}
}

static void _tas_hostname_lookup_cb(getdns_dns_req *dnsreq)
{
	getdns_context *context = (getdns_context *)dnsreq->user_pointer;
	tas_connection *a;
       
	if (dnsreq->netreqs[0]->request_type == GETDNS_RRTYPE_A)
		a = &context->a;
	else	a = &context->aaaa;

	a->rrset = _getdns_rrset_answer(
	    &a->rrset_spc, a->req->response, a->req->response_len);

	if (!a->rrset) {
		char tas_hostname[256] = "<no hostname>";
		(void) _getdns_get_tas_url_hostname(context, tas_hostname, NULL);
		_getdns_log( &context->log
			   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_DEBUG
			   , "%s lookup for %s returned no response\n"
		            , rt_str(a->req->request_type), tas_hostname);

	} else if (a->req->response_len < dnsreq->name_len + 12 ||
	    !_getdns_dname_equal(a->req->response + 12, dnsreq->name) ||
	    a->rrset->rr_type != a->req->request_type) {
		char tas_hostname[256] = "<no hostname>";
		(void) _getdns_get_tas_url_hostname(context, tas_hostname, NULL);
		_getdns_log( &context->log
			   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_DEBUG
			   , "%s lookup for %s returned wrong response\n"
		            , rt_str(a->req->request_type), tas_hostname);

	} else  if (!(a->rr = _getdns_rrtype_iter_init(&a->rr_spc, a->rrset))) {
		char tas_hostname[256] = "<no hostname>";
		(void) _getdns_get_tas_url_hostname(context, tas_hostname, NULL);
		_getdns_log( &context->log
			   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_DEBUG
			   , "%s lookup for %s returned no addresses\n"
		            , rt_str(a->req->request_type), tas_hostname);

	} else {
		tas_connection *other = a == &context->a ? &context->aaaa
		                                         : &context->a;
		a->loop = dnsreq->loop;

		if (tas_fetching(other))
			; /* pass */

		else if (a == &context->a && tas_busy(other)) {
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_DEBUG
				   , "Waiting 25ms for AAAA to arrive\n");
			GETDNS_SCHEDULE_EVENT(a->loop, a->fd, 25,
			    getdns_eventloop_event_init(&a->event,
			    a->req->owner, NULL, NULL, tas_happy_eyeballs_cb));
		} else {
			if (other->event.ev &&
			    other->event.timeout_cb == tas_happy_eyeballs_cb) {
				DEBUG_ANCHOR("Clearing Happy Eyeballs timer\n");
				GETDNS_CLEAR_EVENT(other->loop, &other->event);
			}
			tas_connect(context, a);
		}
		return;
	}
	tas_fail(context, a);
}

void _getdns_start_fetching_ta(
    getdns_context *context, getdns_eventloop *loop, uint64_t *now_ms)
{
	getdns_return_t r;
	size_t scheduled;
	char tas_hostname[256] = "";
	const char *verify_CA;
	const char *verify_email;

	if ((r = _getdns_get_tas_url_hostname(context, tas_hostname, NULL))) {
		_getdns_log( &context->log
			   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
			   , "Cannot get hostname from trust anchor url: "
			     "\"%s\"\n", getdns_get_errorstr_by_id(r));
		return;

	} else if ((r = getdns_context_get_trust_anchors_verify_CA(
	    context, &verify_CA))) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
		           , "Cannot get trust anchor verify CA: \"%s\"\n"
			   , getdns_get_errorstr_by_id(r));
		return;

	} else if (!verify_CA || !*verify_CA) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_INFO
		           , "Trust anchor verification explicitly "
		             "disabled by empty verify CA\n");
		return;

	} else if ((r = getdns_context_get_trust_anchors_verify_email(
	    context, &verify_email))) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
		           , "Cannot get trust anchor verify email: \"%s\"\n"
			   , getdns_get_errorstr_by_id(r));
		return;

	} else if (!verify_email || !*verify_email) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_INFO
		           , "Trust anchor verification explicitly "
		             "disabled by empty verify email\n");
		return;

	} else if (!_getdns_context_can_write_appdata(context)) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_WARNING
		           , "Not fetching TA, because "
		             "non writeable appdata directory\n");
		return;
	} 
	DEBUG_ANCHOR("Hostname: %s\n", tas_hostname);
	DEBUG_ANCHOR("%s on the %ssynchronous loop\n", __FUNC__,
	             loop == &context->sync_eventloop.loop ? "" : "a");

	scheduled = 0;
	context->a.state = TAS_LOOKUP_ADDRESSES;
	if ((r = _getdns_general_loop(context, loop,
	    tas_hostname, GETDNS_RRTYPE_A,
	    no_dnssec_checking_disabled_opportunistic,
	    context, &context->a.req, NULL, _tas_hostname_lookup_cb))) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_WARNING
		           , "Error scheduling A lookup for %s: %s\n"
		           , tas_hostname, getdns_get_errorstr_by_id(r));
	} else
		scheduled += 1;

	context->aaaa.state = TAS_LOOKUP_ADDRESSES;
	if ((r = _getdns_general_loop(context, loop,
	    tas_hostname, GETDNS_RRTYPE_AAAA,
	    no_dnssec_checking_disabled_opportunistic,
	    context, &context->aaaa.req, NULL, _tas_hostname_lookup_cb))) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_WARNING
		           , "Error scheduling AAAA lookup for %s: %s\n"
		           , tas_hostname, getdns_get_errorstr_by_id(r));
	} else
		scheduled += 1;

	if (!scheduled) {
		_getdns_log( &context->log
		           , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_WARNING
		           , "Error scheduling address lookups for %s\n"
		           , tas_hostname);

		context->trust_anchors_source = GETDNS_TASRC_FAILED;
		if (now_ms) {
			if (*now_ms == 0) *now_ms = _getdns_get_now_ms();
			context->trust_anchors_backoff_expiry = 
			    *now_ms + context->trust_anchors_backoff_time;
		} else
			context->trust_anchors_backoff_expiry = 
			    _getdns_get_now_ms() + context->trust_anchors_backoff_time;
		_getdns_ta_notify_dnsreqs(context);
	} else
		context->trust_anchors_source = GETDNS_TASRC_FETCHING;
}


static int _uint16_cmp(const void *a, const void *b)
{ return (int)*(uint16_t *)a - (int)*(uint16_t *)b; }

static int _uint8x16_cmp(const void *a, const void *b)
{ return memcmp(a, b, RRSIG_RDATA_LEN); }

static void
_getdns_init_ksks(_getdns_ksks *ksks, _getdns_rrset *dnskey_set)
{
	_getdns_rrtype_iter *rr, rr_space;
	_getdns_rrsig_iter  *rrsig, rrsig_space;

	assert(ksks);
	assert(dnskey_set);
	assert(dnskey_set->rr_type == GETDNS_RRTYPE_DNSKEY);

	ksks->n = 0;
	for ( rr = _getdns_rrtype_iter_init(&rr_space, dnskey_set)
	    ; rr && ksks->n < MAX_KSKS
	    ; rr = _getdns_rrtype_iter_next(rr)) {

		if (rr->rr_i.nxt - rr->rr_i.rr_type < 12
		    || !(rr->rr_i.rr_type[11] & 1))
			continue; /* Not a KSK */

		ksks->ids[ksks->n++] = gldns_calc_keytag_raw(
		    rr->rr_i.rr_type + 10,
		    rr->rr_i.nxt - rr->rr_i.rr_type - 10);
	}
	qsort(ksks->ids, ksks->n, sizeof(uint16_t), _uint16_cmp);

	ksks->n_rrsigs = 0;
	for ( rrsig = _getdns_rrsig_iter_init(&rrsig_space, dnskey_set)
	    ; rrsig && ksks->n_rrsigs < MAX_KSKS
	    ; rrsig = _getdns_rrsig_iter_next(rrsig)) {
		
		if (rrsig->rr_i.nxt - rrsig->rr_i.rr_type < 28)
			continue;

		(void) memcpy(ksks->rrsigs[ksks->n_rrsigs++],
		    rrsig->rr_i.rr_type + 12, RRSIG_RDATA_LEN);
	}
	qsort(ksks->rrsigs, ksks->n_rrsigs, RRSIG_RDATA_LEN, _uint8x16_cmp);
}


static int
_getdns_ksks_equal(_getdns_ksks *a, _getdns_ksks *b)
{
	return a == b
	    || (  a != NULL && b != NULL
            && a->n == b->n
	    && memcmp(a->ids, b->ids, a->n * sizeof(uint16_t)) == 0
	    && a->n_rrsigs == b->n_rrsigs
	    && memcmp(a->rrsigs, b->rrsigs, a->n_rrsigs * RRSIG_RDATA_LEN) == 0);
}

static void _getdns_context_read_root_ksk(getdns_context *context)
{
	FILE *fp;
	struct gldns_file_parse_state pst;
	size_t len, dname_len;
	uint8_t buf_spc[4096], *buf = buf_spc, *ptr = buf_spc;
	size_t buf_sz = sizeof(buf_spc);
	_getdns_rrset root_dnskey;
	uint8_t *root_dname = (uint8_t *)"\00";


	if (!(fp = _getdns_context_get_priv_fp(context, "root.key")))
		return;

	for (;;) {
		size_t n_rrs = 0;

		*pst.origin = 0;
		pst.origin_len = 1;
		*pst.prev_rr = 0;
		pst.prev_rr_len = 1;
		pst.default_ttl = 0;
		pst.lineno = 1;

		(void) memset(buf, 0, 12);
		ptr += 12;

		while (!feof(fp)) {
			len = buf + buf_sz - ptr;
			dname_len = 0;
			if (gldns_fp2wire_rr_buf(fp, ptr, &len, &dname_len, &pst))
				break;
			if ((ptr += len) > buf + buf_sz)
				break;
			if (len)
				n_rrs += 1;
			if (dname_len && dname_len < sizeof(pst.prev_rr)) {
				memcpy(pst.prev_rr, ptr, dname_len);
				pst.prev_rr_len = dname_len;
			}
		}
		if (ptr <= buf + buf_sz) {
			gldns_write_uint16(buf + GLDNS_ANCOUNT_OFF, n_rrs);
			break;
		}
		rewind(fp);
		if (buf == buf_spc)
			buf_sz = 65536;
		else {
			GETDNS_FREE(context->mf, buf);
			buf_sz *= 2;
		}
		if (!(buf = GETDNS_XMALLOC(context->mf, uint8_t, buf_sz))) {
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
				   , "Error allocating memory to read "
				     "root.key\n");
			break;;
		}
		ptr = buf;
	};
	fclose(fp);
	if (!buf)
		return;

	root_dnskey.name     = root_dname;
	root_dnskey.rr_class = GETDNS_RRCLASS_IN;
	root_dnskey.rr_type  = GETDNS_RRTYPE_DNSKEY;
	root_dnskey.pkt      = buf;
	root_dnskey.pkt_len  = ptr - buf;
	root_dnskey.sections = SECTION_ANSWER;

	_getdns_init_ksks(&context->root_ksk, &root_dnskey);

	if (buf && buf != buf_spc)
		GETDNS_FREE(context->mf, buf);
}

void
_getdns_context_update_root_ksk(
    getdns_context *context, _getdns_rrset *dnskey_set)
{
	_getdns_ksks root_ksk_seen;
	_getdns_rrtype_iter *rr, rr_space;
	_getdns_rrsig_iter  *rrsig, rrsig_space;
	char str_spc[4096], *str_buf, *str_pos;
	int sz_needed;
	int remaining;
	size_t str_sz = 0;
	getdns_bindata root_key_bd;

	_getdns_init_ksks(&root_ksk_seen, dnskey_set);
	if (_getdns_ksks_equal(&context->root_ksk, &root_ksk_seen))
		return; /* root DNSKEY rrset already known */

	 /* Try to read root DNSKEY rrset from root.key  */
	_getdns_context_read_root_ksk(context);
	if (_getdns_ksks_equal(&context->root_ksk, &root_ksk_seen))
		return; /* root DNSKEY rrset same as the safed one */

	/* Different root DNSKEY rrset.  Perhaps because of failure to read
	 * from disk.  If we cannot write to our appdata directory, bail out
	 */
	if (context->can_write_appdata == PROP_UNABLE)
		return;

	/* We might be able to write or we do not know whether we can write
	 * to the appdata directory.  In the latter we'll try to write to
	 * find out.  The section below converts the wireformat DNSKEY rrset
	 * to presentationformat.
	 */
	str_pos = str_buf = str_spc;
	remaining = sizeof(str_spc);
	for (;;) {
		for ( rr = _getdns_rrtype_iter_init(&rr_space, dnskey_set)
		    ; rr ; rr = _getdns_rrtype_iter_next(rr)) {
			
			sz_needed = gldns_wire2str_rr_buf((uint8_t *)rr->rr_i.pos,
			    rr->rr_i.nxt - rr->rr_i.pos, str_pos,
			    (size_t)(remaining > 0 ? remaining : 0));

			str_pos += sz_needed;
			remaining -= sz_needed;
		}
		for ( rrsig = _getdns_rrsig_iter_init(&rrsig_space, dnskey_set)
		    ; rrsig
		    ; rrsig = _getdns_rrsig_iter_next(rrsig)) {
			sz_needed = gldns_wire2str_rr_buf((uint8_t *)rrsig->rr_i.pos,
			    rrsig->rr_i.nxt - rrsig->rr_i.pos, str_pos,
			    (size_t)(remaining > 0 ? remaining : 0));

			str_pos += sz_needed;
			remaining -= sz_needed;
		}
		if (remaining > 0) {
			*str_pos = 0;
			if (str_buf == str_spc)
				str_sz = sizeof(str_spc) - remaining;
			break;
		}
		if (str_buf != str_spc) {
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
				   , "Error determining buffer size for root "
				     "KSK\n");
			if (str_buf)
				GETDNS_FREE(context->mf, str_buf);

			return;
		}
		if (!(str_pos = str_buf = GETDNS_XMALLOC( context->mf, char,
		    (str_sz = sizeof(str_spc) - remaining) + 1))) {
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_ERR
				   , "Error allocating memory to read "
				     "root KSK\n");
			return;
		}
		remaining = str_sz + 1;
	};

	/* Write presentation format DNSKEY rrset to "root.key" file */
	root_key_bd.size = str_sz;
	root_key_bd.data = (void *)str_buf;
	if (_getdns_context_write_priv_file(
	    context, "root.key", &root_key_bd)) {
		size_t i;

		/* A new "root.key" file was written.  When they contain
		 * key_id's which are not in "root-anchors.xml", then update
		 * "root-anchors.xml".
		 */

		for (i = 0; i < context->root_ksk.n; i++) {
			_getdns_rrset_iter tas_iter_spc, *ta;

			for ( ta = _getdns_rrset_iter_init(&tas_iter_spc
						, context->trust_anchors
			         		, context->trust_anchors_len
			         		, SECTION_ANSWER)
			    ; ta ; ta = _getdns_rrset_iter_next(ta)) {
				_getdns_rrtype_iter *rr, rr_space;
				_getdns_rrset *rrset;

				if (!(rrset = _getdns_rrset_iter_value(ta)))
					continue;

				if (*rrset->name != '\0')
					continue; /* Not a root anchor */

				if (rrset->rr_type == GETDNS_RRTYPE_DS) {
					for ( rr = _getdns_rrtype_iter_init(
					           &rr_space, rrset)
					    ; rr
					    ; rr = _getdns_rrtype_iter_next(rr)
					    ) {
						if (rr->rr_i.nxt -
						    rr->rr_i.rr_type < 12)
							continue;

						DEBUG_ANCHOR("DS with id: %d\n"
						, (int)gldns_read_uint16(rr->rr_i.rr_type + 10));
						if (gldns_read_uint16(
						    rr->rr_i.rr_type + 10) ==
						    context->root_ksk.ids[i])
							break;
					}
					if (rr)
						break;
					continue;
				}
				if (rrset->rr_type != GETDNS_RRTYPE_DNSKEY)
					continue;

				for ( rr = _getdns_rrtype_iter_init(&rr_space
				                                   , rrset)
				    ; rr ; rr = _getdns_rrtype_iter_next(rr)) {


					if (rr->rr_i.nxt-rr->rr_i.rr_type < 12
					    || !(rr->rr_i.rr_type[11] & 1))
						continue; /* Not a KSK */

					if (gldns_calc_keytag_raw(
					    rr->rr_i.rr_type + 10,
					    rr->rr_i.nxt-rr->rr_i.rr_type - 10)
					    == context->root_ksk.ids[i])
						break;
				}
				if (rr)
					break;
			}
			if (!ta) {
				_getdns_log( &context->log
					   , GETDNS_LOG_SYS_ANCHOR
					   , GETDNS_LOG_NOTICE
					   , "Key with id %d not found in TA; "
				             "\"root-anchors.xml\" needs to be "
					     "updated.\n"
				            , context->root_ksk.ids[i]);
				context->trust_anchors_source =
				    GETDNS_TASRC_XML_UPDATE;
				break;
			}
			_getdns_log( &context->log
				   , GETDNS_LOG_SYS_ANCHOR, GETDNS_LOG_DEBUG
				   , "Key with id %d found in TA\n"
			           , context->root_ksk.ids[i]);
		}
	}
	if (str_buf && str_buf != str_spc)
		GETDNS_FREE(context->mf, str_buf);
}

/* anchor.c */
