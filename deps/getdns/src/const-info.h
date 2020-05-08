/**
 *
 * /brief _getdns_consts table with values, names and descriptions of the
 *        constants in getdns
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

#ifndef CONST_INFO_H_
#define CONST_INFO_H_

#include "getdns/getdns.h"
#include "getdns/getdns_extra.h"

#ifndef GETDNS_CONTEXT_CODE_MAX_BACKOFF_VALUE
#define GETDNS_CONTEXT_CODE_MAX_BACKOFF_VALUE 699
#define GETDNS_CONTEXT_CODE_MAX_BACKOFF_VALUE_TEXT "Change related to getdns_context_set_max_backoff_value"
#endif

struct const_info {
	int code;
	const char *name;
	const char *text;
};

struct const_info *_getdns_get_const_info(int value);

struct const_name_info {
	const char *name;
	uint32_t code;
};

int _getdns_get_const_name_info(const char *name, uint32_t *code);

#endif

/* const-info.h */
