/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
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

#include "cryptlib.h"
#include <openssl/evp.h>
#include <openssl/lhash.h>
#include "eng_int.h"

/* The type of the items in the table */
typedef struct st_engine_pile
	{
	/* The 'nid' of this algorithm/mode */
	int nid;
	/* ENGINEs that implement this algorithm/mode. */
	STACK_OF(ENGINE) *sk;
	/* The default ENGINE to perform this algorithm/mode. */
	ENGINE *funct;
	/* Zero if 'sk' is newer than the cached 'funct', non-zero otherwise */
	int uptodate;
	} ENGINE_PILE;

DECLARE_LHASH_OF(ENGINE_PILE);

/* The type exposed in eng_int.h */
struct st_engine_table
	{
	LHASH_OF(ENGINE_PILE) piles;
	}; /* ENGINE_TABLE */


typedef struct st_engine_pile_doall
	{
	engine_table_doall_cb *cb;
	void *arg;
	} ENGINE_PILE_DOALL;
	

/* Global flags (ENGINE_TABLE_FLAG_***). */
static unsigned int table_flags = 0;

/* API function manipulating 'table_flags' */
unsigned int ENGINE_get_table_flags(void)
	{
	return table_flags;
	}

void ENGINE_set_table_flags(unsigned int flags)
	{
	table_flags = flags;
	}

/* Internal functions for the "piles" hash table */
static unsigned long engine_pile_hash(const ENGINE_PILE *c)
	{
	return c->nid;
	}

static int engine_pile_cmp(const ENGINE_PILE *a, const ENGINE_PILE *b)
	{
	return a->nid - b->nid;
	}
static IMPLEMENT_LHASH_HASH_FN(engine_pile, ENGINE_PILE)
static IMPLEMENT_LHASH_COMP_FN(engine_pile, ENGINE_PILE)

static int int_table_check(ENGINE_TABLE **t, int create)
	{
	LHASH_OF(ENGINE_PILE) *lh;

	if(*t) return 1;
	if(!create) return 0;
	if((lh = lh_ENGINE_PILE_new()) == NULL)
		return 0;
	*t = (ENGINE_TABLE *)lh;
	return 1;
	}

/* Privately exposed (via eng_int.h) functions for adding and/or removing
 * ENGINEs from the implementation table */
int engine_table_register(ENGINE_TABLE **table, ENGINE_CLEANUP_CB *cleanup,
		ENGINE *e, const int *nids, int num_nids, int setdefault)
	{
	int ret = 0, added = 0;
	ENGINE_PILE tmplate, *fnd;
	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	if(!(*table))
		added = 1;
	if(!int_table_check(table, 1))
		goto end;
	if(added)
		/* The cleanup callback needs to be added */
		engine_cleanup_add_first(cleanup);
	while(num_nids--)
		{
		tmplate.nid = *nids;
		fnd = lh_ENGINE_PILE_retrieve(&(*table)->piles, &tmplate);
		if(!fnd)
			{
			fnd = OPENSSL_malloc(sizeof(ENGINE_PILE));
			if(!fnd) goto end;
			fnd->uptodate = 1;
			fnd->nid = *nids;
			fnd->sk = sk_ENGINE_new_null();
			if(!fnd->sk)
				{
				OPENSSL_free(fnd);
				goto end;
				}
			fnd->funct = NULL;
			(void)lh_ENGINE_PILE_insert(&(*table)->piles, fnd);
			}
		/* A registration shouldn't add duplciate entries */
		(void)sk_ENGINE_delete_ptr(fnd->sk, e);
		/* if 'setdefault', this ENGINE goes to the head of the list */
		if(!sk_ENGINE_push(fnd->sk, e))
			goto end;
		/* "touch" this ENGINE_PILE */
		fnd->uptodate = 0;
		if(setdefault)
			{
			if(!engine_unlocked_init(e))
				{
				ENGINEerr(ENGINE_F_ENGINE_TABLE_REGISTER,
						ENGINE_R_INIT_FAILED);
				goto end;
				}
			if(fnd->funct)
				engine_unlocked_finish(fnd->funct, 0);
			fnd->funct = e;
			fnd->uptodate = 1;
			}
		nids++;
		}
	ret = 1;
end:
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	return ret;
	}
static void int_unregister_cb_doall_arg(ENGINE_PILE *pile, ENGINE *e)
	{
	int n;
	/* Iterate the 'c->sk' stack removing any occurance of 'e' */
	while((n = sk_ENGINE_find(pile->sk, e)) >= 0)
		{
		(void)sk_ENGINE_delete(pile->sk, n);
		pile->uptodate = 0;
		}
	if(pile->funct == e)
		{
		engine_unlocked_finish(e, 0);
		pile->funct = NULL;
		}
	}
static IMPLEMENT_LHASH_DOALL_ARG_FN(int_unregister_cb, ENGINE_PILE, ENGINE)

void engine_table_unregister(ENGINE_TABLE **table, ENGINE *e)
	{
	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	if(int_table_check(table, 0))
		lh_ENGINE_PILE_doall_arg(&(*table)->piles,
					 LHASH_DOALL_ARG_FN(int_unregister_cb),
					 ENGINE, e);
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	}

static void int_cleanup_cb_doall(ENGINE_PILE *p)
	{
	sk_ENGINE_free(p->sk);
	if(p->funct)
		engine_unlocked_finish(p->funct, 0);
	OPENSSL_free(p);
	}
static IMPLEMENT_LHASH_DOALL_FN(int_cleanup_cb, ENGINE_PILE)

void engine_table_cleanup(ENGINE_TABLE **table)
	{
	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	if(*table)
		{
		lh_ENGINE_PILE_doall(&(*table)->piles,
				     LHASH_DOALL_FN(int_cleanup_cb));
		lh_ENGINE_PILE_free(&(*table)->piles);
		*table = NULL;
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	}

/* return a functional reference for a given 'nid' */
#ifndef ENGINE_TABLE_DEBUG
ENGINE *engine_table_select(ENGINE_TABLE **table, int nid)
#else
ENGINE *engine_table_select_tmp(ENGINE_TABLE **table, int nid, const char *f, int l)
#endif
	{
	ENGINE *ret = NULL;
	ENGINE_PILE tmplate, *fnd=NULL;
	int initres, loop = 0;

	if(!(*table))
		{
#ifdef ENGINE_TABLE_DEBUG
		fprintf(stderr, "engine_table_dbg: %s:%d, nid=%d, nothing "
			"registered!\n", f, l, nid);
#endif
		return NULL;
		}
	ERR_set_mark();
	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	/* Check again inside the lock otherwise we could race against cleanup
	 * operations. But don't worry about a fprintf(stderr). */
	if(!int_table_check(table, 0)) goto end;
	tmplate.nid = nid;
	fnd = lh_ENGINE_PILE_retrieve(&(*table)->piles, &tmplate);
	if(!fnd) goto end;
	if(fnd->funct && engine_unlocked_init(fnd->funct))
		{
#ifdef ENGINE_TABLE_DEBUG
		fprintf(stderr, "engine_table_dbg: %s:%d, nid=%d, using "
			"ENGINE '%s' cached\n", f, l, nid, fnd->funct->id);
#endif
		ret = fnd->funct;
		goto end;
		}
	if(fnd->uptodate)
		{
		ret = fnd->funct;
		goto end;
		}
trynext:
	ret = sk_ENGINE_value(fnd->sk, loop++);
	if(!ret)
		{
#ifdef ENGINE_TABLE_DEBUG
		fprintf(stderr, "engine_table_dbg: %s:%d, nid=%d, no "
				"registered implementations would initialise\n",
				f, l, nid);
#endif
		goto end;
		}
	/* Try to initialise the ENGINE? */
	if((ret->funct_ref > 0) || !(table_flags & ENGINE_TABLE_FLAG_NOINIT))
		initres = engine_unlocked_init(ret);
	else
		initres = 0;
	if(initres)
		{
		/* Update 'funct' */
		if((fnd->funct != ret) && engine_unlocked_init(ret))
			{
			/* If there was a previous default we release it. */
			if(fnd->funct)
				engine_unlocked_finish(fnd->funct, 0);
			fnd->funct = ret;
#ifdef ENGINE_TABLE_DEBUG
			fprintf(stderr, "engine_table_dbg: %s:%d, nid=%d, "
				"setting default to '%s'\n", f, l, nid, ret->id);
#endif
			}
#ifdef ENGINE_TABLE_DEBUG
		fprintf(stderr, "engine_table_dbg: %s:%d, nid=%d, using "
				"newly initialised '%s'\n", f, l, nid, ret->id);
#endif
		goto end;
		}
	goto trynext;
end:
	/* If it failed, it is unlikely to succeed again until some future
	 * registrations have taken place. In all cases, we cache. */
	if(fnd) fnd->uptodate = 1;
#ifdef ENGINE_TABLE_DEBUG
	if(ret)
		fprintf(stderr, "engine_table_dbg: %s:%d, nid=%d, caching "
				"ENGINE '%s'\n", f, l, nid, ret->id);
	else
		fprintf(stderr, "engine_table_dbg: %s:%d, nid=%d, caching "
				"'no matching ENGINE'\n", f, l, nid);
#endif
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	/* Whatever happened, any failed init()s are not failures in this
	 * context, so clear our error state. */
	ERR_pop_to_mark();
	return ret;
	}

/* Table enumeration */

static void int_cb_doall_arg(ENGINE_PILE *pile, ENGINE_PILE_DOALL *dall)
	{
	dall->cb(pile->nid, pile->sk, pile->funct, dall->arg);
	}
static IMPLEMENT_LHASH_DOALL_ARG_FN(int_cb, ENGINE_PILE,ENGINE_PILE_DOALL)

void engine_table_doall(ENGINE_TABLE *table, engine_table_doall_cb *cb,
								void *arg)
	{
	ENGINE_PILE_DOALL dall;
	dall.cb = cb;
	dall.arg = arg;
	lh_ENGINE_PILE_doall_arg(&table->piles, LHASH_DOALL_ARG_FN(int_cb),
				 ENGINE_PILE_DOALL, &dall);
	}
