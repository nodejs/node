/**
 *
 * \file rbtree.h
 * /brief Alternative symbol names for unbound's rbtree.h
 *
 */
/*
 * Copyright (c) 2017, NLnet Labs, the getdns team
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
#ifndef RBTREE_H_SYMBOLS
#define RBTREE_H_SYMBOLS
#define rbnode_type		_getdns_rbnode_t
#define rbtree_null_node	_getdns_rbtree_null_node
#define rbtree_type		_getdns_rbtree_t
#define rbtree_create		_getdns_rbtree_create
#define rbtree_init		_getdns_rbtree_init
#define rbtree_insert		_getdns_rbtree_insert
#define rbtree_delete		_getdns_rbtree_delete
#define rbtree_search		_getdns_rbtree_search
#define rbtree_find_less_equal	_getdns_rbtree_find_less_equal
#define rbtree_first		_getdns_rbtree_first
#define rbtree_last		_getdns_rbtree_last
#define rbtree_next		_getdns_rbtree_next
#define rbtree_previous		_getdns_rbtree_previous
#define traverse_postorder	_getdns_traverse_postorder
#include "util/orig-headers/rbtree.h"
#endif
