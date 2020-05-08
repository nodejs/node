/**
 *
 * \file convert.h
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

#ifndef _GETDNS_CONVERT_H_
#define _GETDNS_CONVERT_H_

#include "types-internal.h"
#include <stdio.h>

getdns_return_t
_getdns_wire2msg_dict_scan(struct mem_funcs *mf,
    const uint8_t **wire, size_t *wire_len, getdns_dict **msg_dict);

getdns_return_t _getdns_wire2rr_dict(struct mem_funcs *mf,
    const uint8_t *wire, size_t wire_len, getdns_dict **rr_dict);

getdns_return_t _getdns_wire2rr_dict_buf(struct mem_funcs *mf,
    const uint8_t *wire, size_t *wire_len, getdns_dict **rr_dict);

getdns_return_t _getdns_wire2rr_dict_scan(struct mem_funcs *mf,
    const uint8_t **wire, size_t *wire_len, getdns_dict **rr_dict);

getdns_return_t _getdns_str2rr_dict(struct mem_funcs *mf, const char *str,
    getdns_dict **rr_dict, const char *origin, uint32_t default_ttl);

getdns_return_t _getdns_fp2rr_list(struct mem_funcs *mf, FILE *in,
    getdns_list **rr_list, const char *origin, uint32_t default_ttl);

getdns_return_t _getdns_reply_dict2wire(
    const getdns_dict *reply, gldns_buffer *buf, int reuse_header);

#endif
/* convert.h */
