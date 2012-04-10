/* crypto/engine/eng_int.h */
/* Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 1999-2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECDH support in OpenSSL originally developed by 
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#ifndef HEADER_ENGINE_INT_H
#define HEADER_ENGINE_INT_H

#include "cryptlib.h"
/* Take public definitions from engine.h */
#include <openssl/engine.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* If we compile with this symbol defined, then both reference counts in the
 * ENGINE structure will be monitored with a line of output on stderr for each
 * change. This prints the engine's pointer address (truncated to unsigned int),
 * "struct" or "funct" to indicate the reference type, the before and after
 * reference count, and the file:line-number pair. The "engine_ref_debug"
 * statements must come *after* the change. */
#ifdef ENGINE_REF_COUNT_DEBUG

#define engine_ref_debug(e, isfunct, diff) \
	fprintf(stderr, "engine: %08x %s from %d to %d (%s:%d)\n", \
		(unsigned int)(e), (isfunct ? "funct" : "struct"), \
		((isfunct) ? ((e)->funct_ref - (diff)) : ((e)->struct_ref - (diff))), \
		((isfunct) ? (e)->funct_ref : (e)->struct_ref), \
		(__FILE__), (__LINE__));

#else

#define engine_ref_debug(e, isfunct, diff)

#endif

/* Any code that will need cleanup operations should use these functions to
 * register callbacks. ENGINE_cleanup() will call all registered callbacks in
 * order. NB: both the "add" functions assume CRYPTO_LOCK_ENGINE to already be
 * held (in "write" mode). */
typedef void (ENGINE_CLEANUP_CB)(void);
typedef struct st_engine_cleanup_item
	{
	ENGINE_CLEANUP_CB *cb;
	} ENGINE_CLEANUP_ITEM;
DECLARE_STACK_OF(ENGINE_CLEANUP_ITEM)
void engine_cleanup_add_first(ENGINE_CLEANUP_CB *cb);
void engine_cleanup_add_last(ENGINE_CLEANUP_CB *cb);

/* We need stacks of ENGINEs for use in eng_table.c */
DECLARE_STACK_OF(ENGINE)

/* If this symbol is defined then engine_table_select(), the function that is
 * used by RSA, DSA (etc) code to select registered ENGINEs, cache defaults and
 * functional references (etc), will display debugging summaries to stderr. */
/* #define ENGINE_TABLE_DEBUG */

/* This represents an implementation table. Dependent code should instantiate it
 * as a (ENGINE_TABLE *) pointer value set initially to NULL. */
typedef struct st_engine_table ENGINE_TABLE;
int engine_table_register(ENGINE_TABLE **table, ENGINE_CLEANUP_CB *cleanup,
		ENGINE *e, const int *nids, int num_nids, int setdefault);
void engine_table_unregister(ENGINE_TABLE **table, ENGINE *e);
void engine_table_cleanup(ENGINE_TABLE **table);
#ifndef ENGINE_TABLE_DEBUG
ENGINE *engine_table_select(ENGINE_TABLE **table, int nid);
#else
ENGINE *engine_table_select_tmp(ENGINE_TABLE **table, int nid, const char *f, int l);
#define engine_table_select(t,n) engine_table_select_tmp(t,n,__FILE__,__LINE__)
#endif
typedef void (engine_table_doall_cb)(int nid, STACK_OF(ENGINE) *sk, ENGINE *def, void *arg);
void engine_table_doall(ENGINE_TABLE *table, engine_table_doall_cb *cb, void *arg);

/* Internal versions of API functions that have control over locking. These are
 * used between C files when functionality needs to be shared but the caller may
 * already be controlling of the CRYPTO_LOCK_ENGINE lock. */
int engine_unlocked_init(ENGINE *e);
int engine_unlocked_finish(ENGINE *e, int unlock_for_handlers);
int engine_free_util(ENGINE *e, int locked);

/* This function will reset all "set"able values in an ENGINE to NULL. This
 * won't touch reference counts or ex_data, but is equivalent to calling all the
 * ENGINE_set_***() functions with a NULL value. */
void engine_set_all_null(ENGINE *e);

/* NB: Bitwise OR-able values for the "flags" variable in ENGINE are now exposed
 * in engine.h. */

/* Free up dynamically allocated public key methods associated with ENGINE */

void engine_pkey_meths_free(ENGINE *e);
void engine_pkey_asn1_meths_free(ENGINE *e);

/* This is a structure for storing implementations of various crypto
 * algorithms and functions. */
struct engine_st
	{
	const char *id;
	const char *name;
	const RSA_METHOD *rsa_meth;
	const DSA_METHOD *dsa_meth;
	const DH_METHOD *dh_meth;
	const ECDH_METHOD *ecdh_meth;
	const ECDSA_METHOD *ecdsa_meth;
	const RAND_METHOD *rand_meth;
	const STORE_METHOD *store_meth;
	/* Cipher handling is via this callback */
	ENGINE_CIPHERS_PTR ciphers;
	/* Digest handling is via this callback */
	ENGINE_DIGESTS_PTR digests;
	/* Public key handling via this callback */
	ENGINE_PKEY_METHS_PTR pkey_meths;
	/* ASN1 public key handling via this callback */
	ENGINE_PKEY_ASN1_METHS_PTR pkey_asn1_meths;

	ENGINE_GEN_INT_FUNC_PTR	destroy;

	ENGINE_GEN_INT_FUNC_PTR init;
	ENGINE_GEN_INT_FUNC_PTR finish;
	ENGINE_CTRL_FUNC_PTR ctrl;
	ENGINE_LOAD_KEY_PTR load_privkey;
	ENGINE_LOAD_KEY_PTR load_pubkey;

	ENGINE_SSL_CLIENT_CERT_PTR load_ssl_client_cert;

	const ENGINE_CMD_DEFN *cmd_defns;
	int flags;
	/* reference count on the structure itself */
	int struct_ref;
	/* reference count on usability of the engine type. NB: This
	 * controls the loading and initialisation of any functionlity
	 * required by this engine, whereas the previous count is
	 * simply to cope with (de)allocation of this structure. Hence,
	 * running_ref <= struct_ref at all times. */
	int funct_ref;
	/* A place to store per-ENGINE data */
	CRYPTO_EX_DATA ex_data;
	/* Used to maintain the linked-list of engines. */
	struct engine_st *prev;
	struct engine_st *next;
	};

#ifdef  __cplusplus
}
#endif

#endif /* HEADER_ENGINE_INT_H */
