/**
 *
 * /brief functions for DNSSEC trust anchor management
 *
 */

/*
 * Copyright (c) 2017, NLnet Labs
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

#ifndef ANCHOR_H_
#define ANCHOR_H_

#include "getdns/getdns.h"
#include "getdns/getdns_extra.h"
#include <time.h>
#include "rr-iter.h"

#include "types-internal.h"

/**
 ** Internal functions, implemented in anchor-internal.c.
 **/
void _getdns_context_equip_with_anchor(getdns_context *context, uint64_t *now_ms);

uint8_t *_getdns_tas_validate(struct mem_funcs *mf,
    const getdns_bindata *xml_bd, const getdns_bindata *p7s_bd,
    const getdns_bindata *crt_bd, const char *p7signer,
                      uint64_t *now_ms, uint8_t *tas, size_t *tas_len);


/**
 ** anchor.c functions used by anchor-internal.c.
 **/
time_t _getdns_xml_convertdate(const char* str);

uint16_t _getdns_parse_xml_trust_anchors_buf(gldns_buffer *gbuf, uint64_t *now_ms, char *xml_data, size_t xml_len);

/**
 ** Public interface.
 **/
void _getdns_context_equip_with_anchor(getdns_context *context, uint64_t *now_ms);

void _getdns_start_fetching_ta(
    getdns_context *context, getdns_eventloop *loop, uint64_t *now_ms);

#define MAX_KSKS        16
#define RRSIG_RDATA_LEN 16
typedef struct _getdns_ksks {
	size_t   n;
	uint16_t ids[MAX_KSKS];
	size_t   n_rrsigs;
	uint8_t  rrsigs[MAX_KSKS][RRSIG_RDATA_LEN];
} _getdns_ksks;

void _getdns_context_update_root_ksk(
    getdns_context *context, _getdns_rrset *dnskey_set);

#endif
/* anchor.h */
