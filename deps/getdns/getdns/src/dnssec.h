/**
 *
 * /brief functions for DNSSEC
 *
 * The _getdns_get_validation_chain function is called after an answer
 * has been fetched when the dnssec_return_validation_chain extension is set.
 * It fetches DNSKEYs, DSes and their signatures for all RRSIGs found in the
 * answer.
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

#ifndef DNSSEC_H_
#define DNSSEC_H_

#include "getdns/getdns.h"
#include "config.h"
#include "gldns/gbuffer.h"
#include "gldns/rrdef.h"
#include "types-internal.h"

/* Do some additional requests to fetch the complete validation chain */
void _getdns_get_validation_chain(getdns_dns_req *dns_req);
void _getdns_cancel_validation_chain(getdns_dns_req *dns_req);
void _getdns_validation_chain_timeout(getdns_dns_req *dns_req);
void _getdns_ta_notify_dnsreqs(getdns_context *context);

uint16_t _getdns_parse_ta_file(time_t *ta_mtime, gldns_buffer *gbuf);

static inline int _dnssec_rdata_to_canonicalize(uint16_t rr_type)
{
	return rr_type == GLDNS_RR_TYPE_NS    || rr_type == GLDNS_RR_TYPE_MD
	    || rr_type == GLDNS_RR_TYPE_MF    || rr_type == GLDNS_RR_TYPE_CNAME
	    || rr_type == GLDNS_RR_TYPE_SOA   || rr_type == GLDNS_RR_TYPE_MB
	    || rr_type == GLDNS_RR_TYPE_MG    || rr_type == GLDNS_RR_TYPE_MR
	    || rr_type == GLDNS_RR_TYPE_PTR   || rr_type == GLDNS_RR_TYPE_MINFO
	    || rr_type == GLDNS_RR_TYPE_MX    || rr_type == GLDNS_RR_TYPE_RP
	    || rr_type == GLDNS_RR_TYPE_AFSDB || rr_type == GLDNS_RR_TYPE_RT
	    || rr_type == GLDNS_RR_TYPE_SIG   || rr_type == GLDNS_RR_TYPE_PX
	    || rr_type == GLDNS_RR_TYPE_NXT   || rr_type == GLDNS_RR_TYPE_NAPTR
	    || rr_type == GLDNS_RR_TYPE_KX    || rr_type == GLDNS_RR_TYPE_SRV
	    || rr_type == GLDNS_RR_TYPE_DNAME || rr_type == GLDNS_RR_TYPE_RRSIG;
}

int _getdns_bogus(getdns_dns_req *dns_req);

#endif

/* dnssec.h */
