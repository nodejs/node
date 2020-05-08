/**
 *
 * /brief RR iterator over wireformat DNS packet
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

#ifndef RR_ITER_H_
#define RR_ITER_H_

#include "rr-dict.h"
#include "gldns/pkthdr.h"

typedef enum _getdns_section {
	SECTION_QUESTION      =  1,
	SECTION_ANSWER        =  2,
	SECTION_AUTHORITY     =  4,
	SECTION_ADDITIONAL    =  8,
	SECTION_ANY           = 15,
	SECTION_NO_QUESTION   = 14,
	SECTION_NO_ADDITIONAL =  6
} _getdns_section;

int _getdns_dname_equal(const uint8_t *s1, const uint8_t *s2);

typedef struct _getdns_rr_iter {
	const uint8_t *pkt;
	const uint8_t *pkt_end;

	/* Which RR are we currently at */
	size_t   n;

	/* pos points to start of the owner name the RR.
	 * Or is NULL when there are no RR's left.
	 */
	const uint8_t *pos;

	/* rr_type will point to the rr_type right after the RR's owner name.
	 * rr_type is guaranteed to have a value when pos has a value
	 */
	const uint8_t *rr_type;

	/* nxt point to the owner name of the next RR or to pkt_end */
	const uint8_t *nxt;

} _getdns_rr_iter;

_getdns_rr_iter *_getdns_rr_iter_init(_getdns_rr_iter *i,
    const uint8_t *pkt, const size_t pkt_len);

_getdns_rr_iter *_getdns_single_rr_iter_init(_getdns_rr_iter *i,
    const uint8_t *wire, const size_t wire_len);

static inline _getdns_rr_iter *_getdns_rr_iter_rewind(_getdns_rr_iter *i)
{ return i ? _getdns_rr_iter_init(i, i->pkt, i->pkt_end - i->pkt) : NULL; }

_getdns_rr_iter *_getdns_rr_iter_next(_getdns_rr_iter *i);

const uint8_t *_getdns_owner_if_or_as_decompressed(
    _getdns_rr_iter *i, uint8_t *ff_bytes, size_t *len);

static inline _getdns_section 
_getdns_rr_iter_section(_getdns_rr_iter *i)
{
	return ! i->pkt ? (i->nxt - i->rr_type == 4 ? SECTION_QUESTION
	                                            : SECTION_ANSWER  )
             : i->n < (size_t)GLDNS_QDCOUNT(i->pkt) ? SECTION_QUESTION
	     : i->n < (size_t)GLDNS_QDCOUNT(i->pkt)
	                    + GLDNS_ANCOUNT(i->pkt) ? SECTION_ANSWER
	     : i->n < (size_t)GLDNS_QDCOUNT(i->pkt)
	                    + GLDNS_ANCOUNT(i->pkt)
	                    + GLDNS_NSCOUNT(i->pkt) ? SECTION_AUTHORITY
	     : i->n < (size_t)GLDNS_QDCOUNT(i->pkt)
	                    + GLDNS_ANCOUNT(i->pkt)
	                    + GLDNS_NSCOUNT(i->pkt)
	                    + GLDNS_ARCOUNT(i->pkt) ? SECTION_ADDITIONAL
	                                            : SECTION_ANY;
}

/* Utility functions to read rr_type and rr_class from a rr iterator */
static inline uint16_t rr_iter_type(_getdns_rr_iter *rr)
{ return rr->rr_type + 2 <= rr->nxt ? gldns_read_uint16(rr->rr_type) : 0; }

static inline uint16_t rr_iter_class(_getdns_rr_iter *rr)
{ return rr->rr_type + 4 <= rr->nxt ? gldns_read_uint16(rr->rr_type + 2) : 0; }

typedef struct _getdns_rrset {
	const uint8_t  *name;
	uint16_t        rr_class;
	uint16_t        rr_type;
	const uint8_t  *pkt;
	size_t          pkt_len;
	_getdns_section sections;
} _getdns_rrset;

typedef struct _getdns_rrset_spc {
	_getdns_rrset rrset;
	uint8_t       name_spc[256];
	size_t        name_len;
} _getdns_rrset_spc;

_getdns_rrset *_getdns_rrset_answer(
    _getdns_rrset_spc *rrset2init, const uint8_t *pkt, size_t pkt_len);

_getdns_rrset *_getdns_initialized_rrset_answer(
    _getdns_rrset_spc *query_rrset);

typedef struct _getdns_rrtype_iter {
	_getdns_rr_iter  rr_i;
	_getdns_rrset   *rrset;
} _getdns_rrtype_iter;

_getdns_rrtype_iter *_getdns_rrtype_iter_init(
    _getdns_rrtype_iter *i, _getdns_rrset *rrset);
_getdns_rrtype_iter *_getdns_rrtype_iter_next(_getdns_rrtype_iter *i);

static inline int _getdns_rrset_has_rrs(_getdns_rrset *rrset)
{
	_getdns_rrtype_iter rr_spc;
	return _getdns_rrtype_iter_init(&rr_spc, rrset) != NULL;
}

typedef struct _getdns_rrsig_iter {
	_getdns_rr_iter  rr_i;
	_getdns_rrset   *rrset;
} _getdns_rrsig_iter;

_getdns_rrsig_iter *_getdns_rrsig_iter_init(
    _getdns_rrsig_iter *i, _getdns_rrset *rrset);
_getdns_rrsig_iter *_getdns_rrsig_iter_next(_getdns_rrsig_iter *i);


static inline int _getdns_rrset_has_rrsigs(_getdns_rrset *rrset)
{
	_getdns_rrsig_iter rrsig;
	return _getdns_rrsig_iter_init(&rrsig, rrset) != NULL;
}

/* The _getdns_rrset_iter manifests an iterator of a wireformat packet that
 * will return all unique rrsets within that packet in turn.
 */
typedef struct _getdns_rrset_iter {
	_getdns_rrset   rrset;
	uint8_t         name_spc[256];
	size_t          name_len;
	_getdns_rr_iter rr_i;
} _getdns_rrset_iter;


_getdns_rrset_iter *_getdns_rrset_iter_init(_getdns_rrset_iter *i,
    const uint8_t *pkt, size_t pkt_len, _getdns_section sections);
_getdns_rrset_iter *_getdns_rrset_iter_next(_getdns_rrset_iter *i);



static inline _getdns_rrset *_getdns_rrset_iter_value(_getdns_rrset_iter *i)
{ return i && i->rr_i.pos ? &i->rrset : NULL; }

static inline _getdns_rrset_iter *_getdns_rrset_iter_rewind(_getdns_rrset_iter *i)
{ return i ? _getdns_rrset_iter_init(i, i->rrset.pkt, i->rrset.pkt_len, i->rrset.sections) : NULL; }

typedef struct _getdns_rdf_iter {
	const uint8_t           *pkt;
	const uint8_t           *pkt_end;
	const _getdns_rdata_def *rdd_pos;
	const _getdns_rdata_def *rdd_end;
	const _getdns_rdata_def *rdd_repeat;
	const uint8_t           *pos;
	const uint8_t           *end;
	const uint8_t           *nxt;
} _getdns_rdf_iter;

_getdns_rdf_iter *_getdns_rdf_iter_init(_getdns_rdf_iter *i,
    _getdns_rr_iter *rr);

_getdns_rdf_iter *_getdns_rdf_iter_next(_getdns_rdf_iter *i);

_getdns_rdf_iter *_getdns_rdf_iter_init_at(_getdns_rdf_iter *i,
    _getdns_rr_iter *rr, size_t pos);

const uint8_t *_getdns_rdf_if_or_as_decompressed(
    _getdns_rdf_iter *i, uint8_t *ff_bytes, size_t *len);


#endif
