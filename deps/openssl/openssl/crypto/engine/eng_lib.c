/* crypto/engine/eng_lib.c */
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

#include "eng_int.h"
#include <openssl/rand.h>

/* The "new"/"free" stuff first */

ENGINE *ENGINE_new(void)
	{
	ENGINE *ret;

	ret = (ENGINE *)OPENSSL_malloc(sizeof(ENGINE));
	if(ret == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_NEW, ERR_R_MALLOC_FAILURE);
		return NULL;
		}
	memset(ret, 0, sizeof(ENGINE));
	ret->struct_ref = 1;
	engine_ref_debug(ret, 0, 1)
	CRYPTO_new_ex_data(CRYPTO_EX_INDEX_ENGINE, ret, &ret->ex_data);
	return ret;
	}

/* Placed here (close proximity to ENGINE_new) so that modifications to the
 * elements of the ENGINE structure are more likely to be caught and changed
 * here. */
void engine_set_all_null(ENGINE *e)
	{
	e->id = NULL;
	e->name = NULL;
	e->rsa_meth = NULL;
	e->dsa_meth = NULL;
	e->dh_meth = NULL;
	e->rand_meth = NULL;
	e->store_meth = NULL;
	e->ciphers = NULL;
	e->digests = NULL;
	e->destroy = NULL;
	e->init = NULL;
	e->finish = NULL;
	e->ctrl = NULL;
	e->load_privkey = NULL;
	e->load_pubkey = NULL;
	e->cmd_defns = NULL;
	e->flags = 0;
	}

int engine_free_util(ENGINE *e, int locked)
	{
	int i;

	if(e == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_FREE_UTIL,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if(locked)
		i = CRYPTO_add(&e->struct_ref,-1,CRYPTO_LOCK_ENGINE);
	else
		i = --e->struct_ref;
	engine_ref_debug(e, 0, -1)
	if (i > 0) return 1;
#ifdef REF_CHECK
	if (i < 0)
		{
		fprintf(stderr,"ENGINE_free, bad structural reference count\n");
		abort();
		}
#endif
	/* Give the ENGINE a chance to do any structural cleanup corresponding
	 * to allocation it did in its constructor (eg. unload error strings) */
	if(e->destroy)
		e->destroy(e);
	CRYPTO_free_ex_data(CRYPTO_EX_INDEX_ENGINE, e, &e->ex_data);
	OPENSSL_free(e);
	return 1;
	}

int ENGINE_free(ENGINE *e)
	{
	return engine_free_util(e, 1);
	}

/* Cleanup stuff */

/* ENGINE_cleanup() is coded such that anything that does work that will need
 * cleanup can register a "cleanup" callback here. That way we don't get linker
 * bloat by referring to all *possible* cleanups, but any linker bloat into code
 * "X" will cause X's cleanup function to end up here. */
static STACK_OF(ENGINE_CLEANUP_ITEM) *cleanup_stack = NULL;
static int int_cleanup_check(int create)
	{
	if(cleanup_stack) return 1;
	if(!create) return 0;
	cleanup_stack = sk_ENGINE_CLEANUP_ITEM_new_null();
	return (cleanup_stack ? 1 : 0);
	}
static ENGINE_CLEANUP_ITEM *int_cleanup_item(ENGINE_CLEANUP_CB *cb)
	{
	ENGINE_CLEANUP_ITEM *item = OPENSSL_malloc(sizeof(
					ENGINE_CLEANUP_ITEM));
	if(!item) return NULL;
	item->cb = cb;
	return item;
	}
void engine_cleanup_add_first(ENGINE_CLEANUP_CB *cb)
	{
	ENGINE_CLEANUP_ITEM *item;
	if(!int_cleanup_check(1)) return;
	item = int_cleanup_item(cb);
	if(item)
		sk_ENGINE_CLEANUP_ITEM_insert(cleanup_stack, item, 0);
	}
void engine_cleanup_add_last(ENGINE_CLEANUP_CB *cb)
	{
	ENGINE_CLEANUP_ITEM *item;
	if(!int_cleanup_check(1)) return;
	item = int_cleanup_item(cb);
	if(item)
		sk_ENGINE_CLEANUP_ITEM_push(cleanup_stack, item);
	}
/* The API function that performs all cleanup */
static void engine_cleanup_cb_free(ENGINE_CLEANUP_ITEM *item)
	{
	(*(item->cb))();
	OPENSSL_free(item);
	}
void ENGINE_cleanup(void)
	{
	if(int_cleanup_check(0))
		{
		sk_ENGINE_CLEANUP_ITEM_pop_free(cleanup_stack,
			engine_cleanup_cb_free);
		cleanup_stack = NULL;
		}
	/* FIXME: This should be handled (somehow) through RAND, eg. by it
	 * registering a cleanup callback. */
	RAND_set_rand_method(NULL);
	}

/* Now the "ex_data" support */

int ENGINE_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
		CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func)
	{
	return CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_ENGINE, argl, argp,
			new_func, dup_func, free_func);
	}

int ENGINE_set_ex_data(ENGINE *e, int idx, void *arg)
	{
	return(CRYPTO_set_ex_data(&e->ex_data, idx, arg));
	}

void *ENGINE_get_ex_data(const ENGINE *e, int idx)
	{
	return(CRYPTO_get_ex_data(&e->ex_data, idx));
	}

/* Functions to get/set an ENGINE's elements - mainly to avoid exposing the
 * ENGINE structure itself. */

int ENGINE_set_id(ENGINE *e, const char *id)
	{
	if(id == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_SET_ID,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	e->id = id;
	return 1;
	}

int ENGINE_set_name(ENGINE *e, const char *name)
	{
	if(name == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_SET_NAME,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	e->name = name;
	return 1;
	}

int ENGINE_set_destroy_function(ENGINE *e, ENGINE_GEN_INT_FUNC_PTR destroy_f)
	{
	e->destroy = destroy_f;
	return 1;
	}

int ENGINE_set_init_function(ENGINE *e, ENGINE_GEN_INT_FUNC_PTR init_f)
	{
	e->init = init_f;
	return 1;
	}

int ENGINE_set_finish_function(ENGINE *e, ENGINE_GEN_INT_FUNC_PTR finish_f)
	{
	e->finish = finish_f;
	return 1;
	}

int ENGINE_set_ctrl_function(ENGINE *e, ENGINE_CTRL_FUNC_PTR ctrl_f)
	{
	e->ctrl = ctrl_f;
	return 1;
	}

int ENGINE_set_flags(ENGINE *e, int flags)
	{
	e->flags = flags;
	return 1;
	}

int ENGINE_set_cmd_defns(ENGINE *e, const ENGINE_CMD_DEFN *defns)
	{
	e->cmd_defns = defns;
	return 1;
	}

const char *ENGINE_get_id(const ENGINE *e)
	{
	return e->id;
	}

const char *ENGINE_get_name(const ENGINE *e)
	{
	return e->name;
	}

ENGINE_GEN_INT_FUNC_PTR ENGINE_get_destroy_function(const ENGINE *e)
	{
	return e->destroy;
	}

ENGINE_GEN_INT_FUNC_PTR ENGINE_get_init_function(const ENGINE *e)
	{
	return e->init;
	}

ENGINE_GEN_INT_FUNC_PTR ENGINE_get_finish_function(const ENGINE *e)
	{
	return e->finish;
	}

ENGINE_CTRL_FUNC_PTR ENGINE_get_ctrl_function(const ENGINE *e)
	{
	return e->ctrl;
	}

int ENGINE_get_flags(const ENGINE *e)
	{
	return e->flags;
	}

const ENGINE_CMD_DEFN *ENGINE_get_cmd_defns(const ENGINE *e)
	{
	return e->cmd_defns;
	}

/* eng_lib.o is pretty much linked into anything that touches ENGINE already, so
 * put the "static_state" hack here. */

static int internal_static_hack = 0;

void *ENGINE_get_static_state(void)
	{
	return &internal_static_hack;
	}
