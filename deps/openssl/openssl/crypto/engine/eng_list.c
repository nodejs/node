/* crypto/engine/eng_list.c */
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

#include "eng_int.h"

/* The linked-list of pointers to engine types. engine_list_head
 * incorporates an implicit structural reference but engine_list_tail
 * does not - the latter is a computational niceity and only points
 * to something that is already pointed to by its predecessor in the
 * list (or engine_list_head itself). In the same way, the use of the
 * "prev" pointer in each ENGINE is to save excessive list iteration,
 * it doesn't correspond to an extra structural reference. Hence,
 * engine_list_head, and each non-null "next" pointer account for
 * the list itself assuming exactly 1 structural reference on each
 * list member. */
static ENGINE *engine_list_head = NULL;
static ENGINE *engine_list_tail = NULL;

/* This cleanup function is only needed internally. If it should be called, we
 * register it with the "ENGINE_cleanup()" stack to be called during cleanup. */

static void engine_list_cleanup(void)
	{
	ENGINE *iterator = engine_list_head;

	while(iterator != NULL)
		{
		ENGINE_remove(iterator);
		iterator = engine_list_head;
		}
	return;
	}

/* These static functions starting with a lower case "engine_" always
 * take place when CRYPTO_LOCK_ENGINE has been locked up. */
static int engine_list_add(ENGINE *e)
	{
	int conflict = 0;
	ENGINE *iterator = NULL;

	if(e == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_LIST_ADD,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	iterator = engine_list_head;
	while(iterator && !conflict)
		{
		conflict = (strcmp(iterator->id, e->id) == 0);
		iterator = iterator->next;
		}
	if(conflict)
		{
		ENGINEerr(ENGINE_F_ENGINE_LIST_ADD,
			ENGINE_R_CONFLICTING_ENGINE_ID);
		return 0;
		}
	if(engine_list_head == NULL)
		{
		/* We are adding to an empty list. */
		if(engine_list_tail)
			{
			ENGINEerr(ENGINE_F_ENGINE_LIST_ADD,
				ENGINE_R_INTERNAL_LIST_ERROR);
			return 0;
			}
		engine_list_head = e;
		e->prev = NULL;
		/* The first time the list allocates, we should register the
		 * cleanup. */
		engine_cleanup_add_last(engine_list_cleanup);
		}
	else
		{
		/* We are adding to the tail of an existing list. */
		if((engine_list_tail == NULL) ||
				(engine_list_tail->next != NULL))
			{
			ENGINEerr(ENGINE_F_ENGINE_LIST_ADD,
				ENGINE_R_INTERNAL_LIST_ERROR);
			return 0;
			}
		engine_list_tail->next = e;
		e->prev = engine_list_tail;
		}
	/* Having the engine in the list assumes a structural
	 * reference. */
	e->struct_ref++;
	engine_ref_debug(e, 0, 1)
	/* However it came to be, e is the last item in the list. */
	engine_list_tail = e;
	e->next = NULL;
	return 1;
	}

static int engine_list_remove(ENGINE *e)
	{
	ENGINE *iterator;

	if(e == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_LIST_REMOVE,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	/* We need to check that e is in our linked list! */
	iterator = engine_list_head;
	while(iterator && (iterator != e))
		iterator = iterator->next;
	if(iterator == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_LIST_REMOVE,
			ENGINE_R_ENGINE_IS_NOT_IN_LIST);
		return 0;
		}
	/* un-link e from the chain. */
	if(e->next)
		e->next->prev = e->prev;
	if(e->prev)
		e->prev->next = e->next;
	/* Correct our head/tail if necessary. */
	if(engine_list_head == e)
		engine_list_head = e->next;
	if(engine_list_tail == e)
		engine_list_tail = e->prev;
	engine_free_util(e, 0);
	return 1;
	}

/* Get the first/last "ENGINE" type available. */
ENGINE *ENGINE_get_first(void)
	{
	ENGINE *ret;

	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	ret = engine_list_head;
	if(ret)
		{
		ret->struct_ref++;
		engine_ref_debug(ret, 0, 1)
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	return ret;
	}

ENGINE *ENGINE_get_last(void)
	{
	ENGINE *ret;

	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	ret = engine_list_tail;
	if(ret)
		{
		ret->struct_ref++;
		engine_ref_debug(ret, 0, 1)
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	return ret;
	}

/* Iterate to the next/previous "ENGINE" type (NULL = end of the list). */
ENGINE *ENGINE_get_next(ENGINE *e)
	{
	ENGINE *ret = NULL;
	if(e == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_GET_NEXT,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	ret = e->next;
	if(ret)
		{
		/* Return a valid structural refernce to the next ENGINE */
		ret->struct_ref++;
		engine_ref_debug(ret, 0, 1)
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	/* Release the structural reference to the previous ENGINE */
	ENGINE_free(e);
	return ret;
	}

ENGINE *ENGINE_get_prev(ENGINE *e)
	{
	ENGINE *ret = NULL;
	if(e == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_GET_PREV,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	ret = e->prev;
	if(ret)
		{
		/* Return a valid structural reference to the next ENGINE */
		ret->struct_ref++;
		engine_ref_debug(ret, 0, 1)
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	/* Release the structural reference to the previous ENGINE */
	ENGINE_free(e);
	return ret;
	}

/* Add another "ENGINE" type into the list. */
int ENGINE_add(ENGINE *e)
	{
	int to_return = 1;
	if(e == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_ADD,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if((e->id == NULL) || (e->name == NULL))
		{
		ENGINEerr(ENGINE_F_ENGINE_ADD,
			ENGINE_R_ID_OR_NAME_MISSING);
		}
	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	if(!engine_list_add(e))
		{
		ENGINEerr(ENGINE_F_ENGINE_ADD,
			ENGINE_R_INTERNAL_LIST_ERROR);
		to_return = 0;
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	return to_return;
	}

/* Remove an existing "ENGINE" type from the array. */
int ENGINE_remove(ENGINE *e)
	{
	int to_return = 1;
	if(e == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_REMOVE,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	if(!engine_list_remove(e))
		{
		ENGINEerr(ENGINE_F_ENGINE_REMOVE,
			ENGINE_R_INTERNAL_LIST_ERROR);
		to_return = 0;
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
	return to_return;
	}

static void engine_cpy(ENGINE *dest, const ENGINE *src)
	{
	dest->id = src->id;
	dest->name = src->name;
#ifndef OPENSSL_NO_RSA
	dest->rsa_meth = src->rsa_meth;
#endif
#ifndef OPENSSL_NO_DSA
	dest->dsa_meth = src->dsa_meth;
#endif
#ifndef OPENSSL_NO_DH
	dest->dh_meth = src->dh_meth;
#endif
#ifndef OPENSSL_NO_ECDH
	dest->ecdh_meth = src->ecdh_meth;
#endif
#ifndef OPENSSL_NO_ECDSA
	dest->ecdsa_meth = src->ecdsa_meth;
#endif
	dest->rand_meth = src->rand_meth;
	dest->store_meth = src->store_meth;
	dest->ciphers = src->ciphers;
	dest->digests = src->digests;
	dest->pkey_meths = src->pkey_meths;
	dest->destroy = src->destroy;
	dest->init = src->init;
	dest->finish = src->finish;
	dest->ctrl = src->ctrl;
	dest->load_privkey = src->load_privkey;
	dest->load_pubkey = src->load_pubkey;
	dest->cmd_defns = src->cmd_defns;
	dest->flags = src->flags;
	}

ENGINE *ENGINE_by_id(const char *id)
	{
	ENGINE *iterator;
	char *load_dir = NULL;
	if(id == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_BY_ID,
			ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
		}
	CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
	iterator = engine_list_head;
	while(iterator && (strcmp(id, iterator->id) != 0))
		iterator = iterator->next;
	if(iterator)
		{
		/* We need to return a structural reference. If this is an
		 * ENGINE type that returns copies, make a duplicate - otherwise
		 * increment the existing ENGINE's reference count. */
		if(iterator->flags & ENGINE_FLAGS_BY_ID_COPY)
			{
			ENGINE *cp = ENGINE_new();
			if(!cp)
				iterator = NULL;
			else
				{
				engine_cpy(cp, iterator);
				iterator = cp;
				}
			}
		else
			{
			iterator->struct_ref++;
			engine_ref_debug(iterator, 0, 1)
			}
		}
	CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
#if 0
	if(iterator == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_BY_ID,
			ENGINE_R_NO_SUCH_ENGINE);
		ERR_add_error_data(2, "id=", id);
		}
	return iterator;
#else
	/* EEK! Experimental code starts */
	if(iterator) return iterator;
	/* Prevent infinite recusrion if we're looking for the dynamic engine. */
	if (strcmp(id, "dynamic"))
		{
#ifdef OPENSSL_SYS_VMS
		if((load_dir = getenv("OPENSSL_ENGINES")) == 0) load_dir = "SSLROOT:[ENGINES]";
#else
		if((load_dir = getenv("OPENSSL_ENGINES")) == 0) load_dir = ENGINESDIR;
#endif
		iterator = ENGINE_by_id("dynamic");
		if(!iterator || !ENGINE_ctrl_cmd_string(iterator, "ID", id, 0) ||
				!ENGINE_ctrl_cmd_string(iterator, "DIR_LOAD", "2", 0) ||
				!ENGINE_ctrl_cmd_string(iterator, "DIR_ADD",
					load_dir, 0) ||
				!ENGINE_ctrl_cmd_string(iterator, "LOAD", NULL, 0))
				goto notfound;
		return iterator;
		}
notfound:
	ENGINE_free(iterator);
	ENGINEerr(ENGINE_F_ENGINE_BY_ID,ENGINE_R_NO_SUCH_ENGINE);
	ERR_add_error_data(2, "id=", id);
	return NULL;
	/* EEK! Experimental code ends */
#endif
	}

int ENGINE_up_ref(ENGINE *e)
	{
	if (e == NULL)
		{
		ENGINEerr(ENGINE_F_ENGINE_UP_REF,ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	CRYPTO_add(&e->struct_ref,1,CRYPTO_LOCK_ENGINE);
	return 1;
	}
