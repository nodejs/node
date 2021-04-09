/**
 *
 * /brief getdns dict data type management functions
 *
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

#ifndef _GETDNS_DICT_H_
#define _GETDNS_DICT_H_

#include "getdns/getdns.h"
#include "util/rbtree.h"
#include "types-internal.h"

/**
 * this structure represents a single item in a dictionary type
 */
struct getdns_dict_item
{
	_getdns_rbnode_t node;
	getdns_item i;
};

/**
 * getdns dictionary data type
 * Use helper functions getdns_dict_* to manipulate and iterate dictionaries
 * dict is implemented using the t*() functions for manipulating binary search
 * trees in the std library.  The internal implementation may change so the
 * application should stick to the helper functions.
 */
struct getdns_dict
{
	_getdns_rbtree_t root;
	struct mem_funcs mf;
};

static inline getdns_dict *_getdns_dict_create_with_mf(struct mem_funcs *mf)
{ return getdns_dict_create_with_extended_memory_functions(
         mf->mf_arg, mf->mf.ext.malloc, mf->mf.ext.realloc, mf->mf.ext.free); }

getdns_return_t _getdns_dict_find(
    const getdns_dict *dict, const char *key, getdns_item **item);

getdns_return_t _getdns_dict_find_and_add(
    getdns_dict *dict, const char *key, getdns_item **item);

/* Return 1 (true) if bindata can be interpreted as an
 * uncompressed dname.
 */
int _getdns_bindata_is_dname(getdns_bindata *bindata);

#endif

/* dict.h */
