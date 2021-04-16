/**
 *
 * \file list.h
 * @brief getdns list management functions
 *
 * Originally taken from the getdns API description pseudo implementation.
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

#ifndef _GETDNS_LIST_H_
#define _GETDNS_LIST_H_

#include "getdns/getdns.h"
#include "types-internal.h"

#define GETDNS_LIST_BLOCKSZ 10

/**
 * getdns list data type
 * Use helper functions getdns_list_* to manipulate and iterate lists
 * lists are implemented as arrays internally since the helper functions
 * like to reference indexes in the list.  Elements are allocated in blocks
 * and then marked valid as they are used and invalid as they are not used
 * The use cases do not justify working too hard at shrinking the structures.
 * Indexes are 0 based.
 */
struct getdns_list
{
	size_t numalloc;
	size_t numinuse;
	struct getdns_item *items;
	struct mem_funcs mf;
};

static inline getdns_list *_getdns_list_create_with_mf(struct mem_funcs *mf)
{ return getdns_list_create_with_extended_memory_functions(
         mf->mf_arg, mf->mf.ext.malloc, mf->mf.ext.realloc, mf->mf.ext.free); }

getdns_return_t _getdns_list_find(
    const getdns_list *dict, const char *key, getdns_item **item);

getdns_return_t _getdns_list_find_and_add(
    getdns_list *list, const char *key, getdns_item **item);

getdns_return_t _getdns_list_remove_name(
    getdns_list *list, const char *name);

#endif

/* list.h */
