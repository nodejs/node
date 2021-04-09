/**
 *
 * \file lruhash.h
 * /brief Alternative symbol names for unbound's lruhash.h
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
#ifndef LRUHASH_H_SYMBOLS
#define LRUHASH_H_SYMBOLS

#define lruhash				_getdns_lruhash
#define lruhash_bin			_getdns_lruhash_bin
#define lruhash_entry			_getdns_lruhash_entry
#define hashvalue_type			_getdns_hashvalue_type
#define lruhash_sizefunc_type		_getdns_lruhash_sizefunc_type
#define lruhash_compfunc_type		_getdns_lruhash_compfunc_type
#define lruhash_delkeyfunc_type		_getdns_lruhash_delkeyfunc_type
#define lruhash_deldatafunc_type	_getdns_lruhash_deldatafunc_type
#define lruhash_markdelfunc_type	_getdns_lruhash_markdelfunc_type
#define lruhash_create			_getdns_lruhash_create
#define lruhash_delete			_getdns_lruhash_delete
#define lruhash_clear			_getdns_lruhash_clear
#define lruhash_insert			_getdns_lruhash_insert
#define lruhash_lookup			_getdns_lruhash_lookup
#define lru_touch			_getdns_lru_touch
#define lruhash_setmarkdel		_getdns_lruhash_setmarkdel

#define lru_demote			_getdns_lru_demote
#define lruhash_insert_or_retrieve	_getdns_lruhash_insert_or_retrieve

#define lruhash_remove			_getdns_lruhash_remove
#define bin_init			_getdns_bin_init
#define bin_delete			_getdns_bin_delete
#define bin_find_entry			_getdns_bin_find_entry
#define bin_overflow_remove		_getdns_bin_overflow_remove
#define bin_split			_getdns_bin_split
#define reclaim_space			_getdns_reclaim_space
#define table_grow			_getdns_table_grow
#define lru_front			_getdns_lru_front
#define lru_remove			_getdns_lru_remove
#define lruhash_status			_getdns_lruhash_status
#define lruhash_get_mem			_getdns_lruhash_get_mem
#define lruhash_traverse		_getdns_lruhash_traverse

#include "util/orig-headers/lruhash.h"
#endif
