/**
 *
 * /brief functions for dealing with pubkey pinsets
 *
 */

/*
 * Copyright (c) 2015 ACLU
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

#ifndef PUBKEY_PINNING_H_
#define PUBKEY_PINNING_H_

/* getdns_pubkey_pin_create_from_string() is implemented in pubkey-pinning.c */
#include "getdns/getdns_extra.h"

#include "tls.h"

/* create and populate a pinset linked list from a getdns_list pinset */
getdns_return_t
_getdns_get_pubkey_pinset_from_list(const getdns_list *pinset_list,
				    struct mem_funcs *mf,
				    sha256_pin_t **pinset_out);


/* create a getdns_list version of the pinset */
getdns_return_t
_getdns_get_pubkey_pinset_list(const getdns_context *ctx,
			       const sha256_pin_t *pinset_in,
			       getdns_list **pinset_list);

#endif
/* pubkey-pinning.h */
