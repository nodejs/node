/**
 *
 * \file locks.h
 * /brief Alternative symbol names for unbound's locks.h
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
#ifndef LOCKS_H_SYMBOLS
#define LOCKS_H_SYMBOLS

#include "config.h"

#define ub_thread_blocksigs	_getdns_ub_thread_blocksigs
#define ub_thread_sig_unblock	_getdns_ub_thread_sig_unblock

#define ub_thread_type		_getdns_ub_thread_type
#define ub_thr_fork_create	_getdns_ub_thr_fork_create
#define ub_thr_fork_wait	_getdns_ub_thr_fork_wait

#if defined(HAVE_SOLARIS_THREADS) || defined(HAVE_WINDOWS_THREADS)
#define ub_thread_key_type	_getdns_ub_thread_key_type
#define ub_thread_key_create	_getdns_ub_thread_key_create
#define ub_thread_key_set	_getdns_ub_thread_key_set
#define ub_thread_key_get	_getdns_ub_thread_key_get
#endif

#ifdef HAVE_WINDOWS_THREADS
#define lock_basic_type		_getdns_lock_basic_type
#define lock_basic_init		_getdns_lock_basic_init
#define lock_basic_destroy	_getdns_lock_basic_destroy
#define lock_basic_lock		_getdns_lock_basic_lock_
#define lock_basic_unlock	_getdns_lock_basic_unlock

#define ub_thread_create	_getdns_ub_thread_create
#define ub_thread_self		_getdns_ub_thread_self
#endif

#include "util/orig-headers/locks.h"
#endif
