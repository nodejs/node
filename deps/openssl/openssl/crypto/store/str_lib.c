/* crypto/store/str_lib.c -*- mode:C; c-file-style: "eay" -*- */
/* Written by Richard Levitte (richard@levitte.org) for the OpenSSL
 * project 2003.
 */
/* ====================================================================
 * Copyright (c) 2003 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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

#include <string.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif
#include <openssl/sha.h>
#include <openssl/x509.h>
#include "str_locl.h"

const char * const STORE_object_type_string[STORE_OBJECT_TYPE_NUM+1] =
	{
	0,
	"X.509 Certificate",
	"X.509 CRL",
	"Private Key",
	"Public Key",
	"Number",
	"Arbitrary Data"
	};

const int STORE_param_sizes[STORE_PARAM_TYPE_NUM+1] =
	{
	0,
	sizeof(int),		/* EVP_TYPE */
	sizeof(size_t),		/* BITS */
	-1,			/* KEY_PARAMETERS */
	0			/* KEY_NO_PARAMETERS */
	};	

const int STORE_attr_sizes[STORE_ATTR_TYPE_NUM+1] =
	{
	0,
	-1,			/* FRIENDLYNAME:	C string */
	SHA_DIGEST_LENGTH,	/* KEYID:		SHA1 digest, 160 bits */
	SHA_DIGEST_LENGTH,	/* ISSUERKEYID:		SHA1 digest, 160 bits */
	SHA_DIGEST_LENGTH,	/* SUBJECTKEYID:	SHA1 digest, 160 bits */
	SHA_DIGEST_LENGTH,	/* ISSUERSERIALHASH:	SHA1 digest, 160 bits */
	sizeof(X509_NAME *),	/* ISSUER:		X509_NAME * */
	sizeof(BIGNUM *),	/* SERIAL:		BIGNUM * */
	sizeof(X509_NAME *),	/* SUBJECT:		X509_NAME * */
	SHA_DIGEST_LENGTH,	/* CERTHASH:		SHA1 digest, 160 bits */
	-1,			/* EMAIL:		C string */
	-1,			/* FILENAME:		C string */
	};	

STORE *STORE_new_method(const STORE_METHOD *method)
	{
	STORE *ret;

	if (method == NULL)
		{
		STOREerr(STORE_F_STORE_NEW_METHOD,ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
		}

	ret=(STORE *)OPENSSL_malloc(sizeof(STORE));
	if (ret == NULL)
		{
		STOREerr(STORE_F_STORE_NEW_METHOD,ERR_R_MALLOC_FAILURE);
		return NULL;
		}

	ret->meth=method;

	CRYPTO_new_ex_data(CRYPTO_EX_INDEX_STORE, ret, &ret->ex_data);
	if (ret->meth->init && !ret->meth->init(ret))
		{
		STORE_free(ret);
		ret = NULL;
		}
	return ret;
	}

STORE *STORE_new_engine(ENGINE *engine)
	{
	STORE *ret = NULL;
	ENGINE *e = engine;
	const STORE_METHOD *meth = 0;

#ifdef OPENSSL_NO_ENGINE
	e = NULL;
#else
	if (engine)
		{
		if (!ENGINE_init(engine))
			{
			STOREerr(STORE_F_STORE_NEW_ENGINE, ERR_R_ENGINE_LIB);
			return NULL;
			}
		e = engine;
		}
	else
		{
		STOREerr(STORE_F_STORE_NEW_ENGINE,ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
		}
	if(e)
		{
		meth = ENGINE_get_STORE(e);
		if(!meth)
			{
			STOREerr(STORE_F_STORE_NEW_ENGINE,
				ERR_R_ENGINE_LIB);
			ENGINE_finish(e);
			return NULL;
			}
		}
#endif

	ret = STORE_new_method(meth);
	if (ret == NULL)
		{
		STOREerr(STORE_F_STORE_NEW_ENGINE,ERR_R_STORE_LIB);
		return NULL;
		}

	ret->engine = e;

	return(ret);
	}

void STORE_free(STORE *store)
	{
	if (store == NULL)
		return;
	if (store->meth->clean)
		store->meth->clean(store);
	CRYPTO_free_ex_data(CRYPTO_EX_INDEX_STORE, store, &store->ex_data);
	OPENSSL_free(store);
	}

int STORE_ctrl(STORE *store, int cmd, long i, void *p, void (*f)(void))
	{
	if (store == NULL)
		{
		STOREerr(STORE_F_STORE_CTRL,ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (store->meth->ctrl)
		return store->meth->ctrl(store, cmd, i, p, f);
	STOREerr(STORE_F_STORE_CTRL,STORE_R_NO_CONTROL_FUNCTION);
	return 0;
	}


int STORE_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
	     CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func)
        {
	return CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_STORE, argl, argp,
				new_func, dup_func, free_func);
        }

int STORE_set_ex_data(STORE *r, int idx, void *arg)
	{
	return(CRYPTO_set_ex_data(&r->ex_data,idx,arg));
	}

void *STORE_get_ex_data(STORE *r, int idx)
	{
	return(CRYPTO_get_ex_data(&r->ex_data,idx));
	}

const STORE_METHOD *STORE_get_method(STORE *store)
	{
	return store->meth;
	}

const STORE_METHOD *STORE_set_method(STORE *store, const STORE_METHOD *meth)
	{
	store->meth=meth;
	return store->meth;
	}


/* API helpers */

#define check_store(s,fncode,fnname,fnerrcode) \
	do \
		{ \
		if ((s) == NULL || (s)->meth == NULL) \
			{ \
			STOREerr((fncode), ERR_R_PASSED_NULL_PARAMETER); \
			return 0; \
			} \
		if ((s)->meth->fnname == NULL) \
			{ \
			STOREerr((fncode), (fnerrcode)); \
			return 0; \
			} \
		} \
	while(0)

/* API functions */

X509 *STORE_get_certificate(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	X509 *x;

	check_store(s,STORE_F_STORE_GET_CERTIFICATE,
		get_object,STORE_R_NO_GET_OBJECT_FUNCTION);

	object = s->meth->get_object(s, STORE_OBJECT_TYPE_X509_CERTIFICATE,
		attributes, parameters);
	if (!object || !object->data.x509.certificate)
		{
		STOREerr(STORE_F_STORE_GET_CERTIFICATE,
			STORE_R_FAILED_GETTING_CERTIFICATE);
		return 0;
		}
	CRYPTO_add(&object->data.x509.certificate->references,1,CRYPTO_LOCK_X509);
#ifdef REF_PRINT
	REF_PRINT("X509",data);
#endif
	x = object->data.x509.certificate;
	STORE_OBJECT_free(object);
	return x;
	}

int STORE_store_certificate(STORE *s, X509 *data, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	int i;

	check_store(s,STORE_F_STORE_CERTIFICATE,
		store_object,STORE_R_NO_STORE_OBJECT_FUNCTION);

	object = STORE_OBJECT_new();
	if (!object)
		{
		STOREerr(STORE_F_STORE_STORE_CERTIFICATE,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	
	CRYPTO_add(&data->references,1,CRYPTO_LOCK_X509);
#ifdef REF_PRINT
	REF_PRINT("X509",data);
#endif
	object->data.x509.certificate = data;

	i = s->meth->store_object(s, STORE_OBJECT_TYPE_X509_CERTIFICATE,
		object, attributes, parameters);

	STORE_OBJECT_free(object);

	if (!i)
		{
		STOREerr(STORE_F_STORE_STORE_CERTIFICATE,
			STORE_R_FAILED_STORING_CERTIFICATE);
		return 0;
		}
	return 1;
	}

int STORE_modify_certificate(STORE *s, OPENSSL_ITEM search_attributes[],
	OPENSSL_ITEM add_attributes[], OPENSSL_ITEM modify_attributes[],
	OPENSSL_ITEM delete_attributes[], OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_MODIFY_CERTIFICATE,
		modify_object,STORE_R_NO_MODIFY_OBJECT_FUNCTION);

	if (!s->meth->modify_object(s, STORE_OBJECT_TYPE_X509_CERTIFICATE,
		    search_attributes, add_attributes, modify_attributes,
		    delete_attributes, parameters))
		{
		STOREerr(STORE_F_STORE_MODIFY_CERTIFICATE,
			STORE_R_FAILED_MODIFYING_CERTIFICATE);
		return 0;
		}
	return 1;
	}

int STORE_revoke_certificate(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_REVOKE_CERTIFICATE,
		revoke_object,STORE_R_NO_REVOKE_OBJECT_FUNCTION);

	if (!s->meth->revoke_object(s, STORE_OBJECT_TYPE_X509_CERTIFICATE,
		    attributes, parameters))
		{
		STOREerr(STORE_F_STORE_REVOKE_CERTIFICATE,
			STORE_R_FAILED_REVOKING_CERTIFICATE);
		return 0;
		}
	return 1;
	}

int STORE_delete_certificate(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_DELETE_CERTIFICATE,
		delete_object,STORE_R_NO_DELETE_OBJECT_FUNCTION);

	if (!s->meth->delete_object(s, STORE_OBJECT_TYPE_X509_CERTIFICATE,
		    attributes, parameters))
		{
		STOREerr(STORE_F_STORE_DELETE_CERTIFICATE,
			STORE_R_FAILED_DELETING_CERTIFICATE);
		return 0;
		}
	return 1;
	}

void *STORE_list_certificate_start(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	void *handle;

	check_store(s,STORE_F_STORE_LIST_CERTIFICATE_START,
		list_object_start,STORE_R_NO_LIST_OBJECT_START_FUNCTION);

	handle = s->meth->list_object_start(s,
		STORE_OBJECT_TYPE_X509_CERTIFICATE, attributes, parameters);
	if (!handle)
		{
		STOREerr(STORE_F_STORE_LIST_CERTIFICATE_START,
			STORE_R_FAILED_LISTING_CERTIFICATES);
		return 0;
		}
	return handle;
	}

X509 *STORE_list_certificate_next(STORE *s, void *handle)
	{
	STORE_OBJECT *object;
	X509 *x;

	check_store(s,STORE_F_STORE_LIST_CERTIFICATE_NEXT,
		list_object_next,STORE_R_NO_LIST_OBJECT_NEXT_FUNCTION);

	object = s->meth->list_object_next(s, handle);
	if (!object || !object->data.x509.certificate)
		{
		STOREerr(STORE_F_STORE_LIST_CERTIFICATE_NEXT,
			STORE_R_FAILED_LISTING_CERTIFICATES);
		return 0;
		}
	CRYPTO_add(&object->data.x509.certificate->references,1,CRYPTO_LOCK_X509);
#ifdef REF_PRINT
	REF_PRINT("X509",data);
#endif
	x = object->data.x509.certificate;
	STORE_OBJECT_free(object);
	return x;
	}

int STORE_list_certificate_end(STORE *s, void *handle)
	{
	check_store(s,STORE_F_STORE_LIST_CERTIFICATE_END,
		list_object_end,STORE_R_NO_LIST_OBJECT_END_FUNCTION);

	if (!s->meth->list_object_end(s, handle))
		{
		STOREerr(STORE_F_STORE_LIST_CERTIFICATE_END,
			STORE_R_FAILED_LISTING_CERTIFICATES);
		return 0;
		}
	return 1;
	}

int STORE_list_certificate_endp(STORE *s, void *handle)
	{
	check_store(s,STORE_F_STORE_LIST_CERTIFICATE_ENDP,
		list_object_endp,STORE_R_NO_LIST_OBJECT_ENDP_FUNCTION);

	if (!s->meth->list_object_endp(s, handle))
		{
		STOREerr(STORE_F_STORE_LIST_CERTIFICATE_ENDP,
			STORE_R_FAILED_LISTING_CERTIFICATES);
		return 0;
		}
	return 1;
	}

EVP_PKEY *STORE_generate_key(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	EVP_PKEY *pkey;

	check_store(s,STORE_F_STORE_GENERATE_KEY,
		generate_object,STORE_R_NO_GENERATE_OBJECT_FUNCTION);

	object = s->meth->generate_object(s, STORE_OBJECT_TYPE_PRIVATE_KEY,
		attributes, parameters);
	if (!object || !object->data.key)
		{
		STOREerr(STORE_F_STORE_GENERATE_KEY,
			STORE_R_FAILED_GENERATING_KEY);
		return 0;
		}
	CRYPTO_add(&object->data.key->references,1,CRYPTO_LOCK_EVP_PKEY);
#ifdef REF_PRINT
	REF_PRINT("EVP_PKEY",data);
#endif
	pkey = object->data.key;
	STORE_OBJECT_free(object);
	return pkey;
	}

EVP_PKEY *STORE_get_private_key(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	EVP_PKEY *pkey;

	check_store(s,STORE_F_STORE_GET_PRIVATE_KEY,
		get_object,STORE_R_NO_GET_OBJECT_FUNCTION);

	object = s->meth->get_object(s, STORE_OBJECT_TYPE_PRIVATE_KEY,
		attributes, parameters);
	if (!object || !object->data.key || !object->data.key)
		{
		STOREerr(STORE_F_STORE_GET_PRIVATE_KEY,
			STORE_R_FAILED_GETTING_KEY);
		return 0;
		}
	CRYPTO_add(&object->data.key->references,1,CRYPTO_LOCK_EVP_PKEY);
#ifdef REF_PRINT
	REF_PRINT("EVP_PKEY",data);
#endif
	pkey = object->data.key;
	STORE_OBJECT_free(object);
	return pkey;
	}

int STORE_store_private_key(STORE *s, EVP_PKEY *data, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	int i;

	check_store(s,STORE_F_STORE_STORE_PRIVATE_KEY,
		store_object,STORE_R_NO_STORE_OBJECT_FUNCTION);

	object = STORE_OBJECT_new();
	if (!object)
		{
		STOREerr(STORE_F_STORE_STORE_PRIVATE_KEY,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	object->data.key = EVP_PKEY_new();
	if (!object->data.key)
		{
		STOREerr(STORE_F_STORE_STORE_PRIVATE_KEY,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	
	CRYPTO_add(&data->references,1,CRYPTO_LOCK_EVP_PKEY);
#ifdef REF_PRINT
	REF_PRINT("EVP_PKEY",data);
#endif
	object->data.key = data;

	i = s->meth->store_object(s, STORE_OBJECT_TYPE_PRIVATE_KEY, object,
		attributes, parameters);

	STORE_OBJECT_free(object);

	if (!i)
		{
		STOREerr(STORE_F_STORE_STORE_PRIVATE_KEY,
			STORE_R_FAILED_STORING_KEY);
		return 0;
		}
	return i;
	}

int STORE_modify_private_key(STORE *s, OPENSSL_ITEM search_attributes[],
	OPENSSL_ITEM add_attributes[], OPENSSL_ITEM modify_attributes[],
	OPENSSL_ITEM delete_attributes[], OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_MODIFY_PRIVATE_KEY,
		modify_object,STORE_R_NO_MODIFY_OBJECT_FUNCTION);

	if (!s->meth->modify_object(s, STORE_OBJECT_TYPE_PRIVATE_KEY,
		    search_attributes, add_attributes, modify_attributes,
		    delete_attributes, parameters))
		{
		STOREerr(STORE_F_STORE_MODIFY_PRIVATE_KEY,
			STORE_R_FAILED_MODIFYING_PRIVATE_KEY);
		return 0;
		}
	return 1;
	}

int STORE_revoke_private_key(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	int i;

	check_store(s,STORE_F_STORE_REVOKE_PRIVATE_KEY,
		revoke_object,STORE_R_NO_REVOKE_OBJECT_FUNCTION);

	i = s->meth->revoke_object(s, STORE_OBJECT_TYPE_PRIVATE_KEY,
		attributes, parameters);

	if (!i)
		{
		STOREerr(STORE_F_STORE_REVOKE_PRIVATE_KEY,
			STORE_R_FAILED_REVOKING_KEY);
		return 0;
		}
	return i;
	}

int STORE_delete_private_key(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_DELETE_PRIVATE_KEY,
		delete_object,STORE_R_NO_DELETE_OBJECT_FUNCTION);
	
	if (!s->meth->delete_object(s, STORE_OBJECT_TYPE_PRIVATE_KEY,
		    attributes, parameters))
		{
		STOREerr(STORE_F_STORE_DELETE_PRIVATE_KEY,
			STORE_R_FAILED_DELETING_KEY);
		return 0;
		}
	return 1;
	}

void *STORE_list_private_key_start(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	void *handle;

	check_store(s,STORE_F_STORE_LIST_PRIVATE_KEY_START,
		list_object_start,STORE_R_NO_LIST_OBJECT_START_FUNCTION);

	handle = s->meth->list_object_start(s, STORE_OBJECT_TYPE_PRIVATE_KEY,
		attributes, parameters);
	if (!handle)
		{
		STOREerr(STORE_F_STORE_LIST_PRIVATE_KEY_START,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	return handle;
	}

EVP_PKEY *STORE_list_private_key_next(STORE *s, void *handle)
	{
	STORE_OBJECT *object;
	EVP_PKEY *pkey;

	check_store(s,STORE_F_STORE_LIST_PRIVATE_KEY_NEXT,
		list_object_next,STORE_R_NO_LIST_OBJECT_NEXT_FUNCTION);

	object = s->meth->list_object_next(s, handle);
	if (!object || !object->data.key || !object->data.key)
		{
		STOREerr(STORE_F_STORE_LIST_PRIVATE_KEY_NEXT,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	CRYPTO_add(&object->data.key->references,1,CRYPTO_LOCK_EVP_PKEY);
#ifdef REF_PRINT
	REF_PRINT("EVP_PKEY",data);
#endif
	pkey = object->data.key;
	STORE_OBJECT_free(object);
	return pkey;
	}

int STORE_list_private_key_end(STORE *s, void *handle)
	{
	check_store(s,STORE_F_STORE_LIST_PRIVATE_KEY_END,
		list_object_end,STORE_R_NO_LIST_OBJECT_END_FUNCTION);

	if (!s->meth->list_object_end(s, handle))
		{
		STOREerr(STORE_F_STORE_LIST_PRIVATE_KEY_END,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	return 1;
	}

int STORE_list_private_key_endp(STORE *s, void *handle)
	{
	check_store(s,STORE_F_STORE_LIST_PRIVATE_KEY_ENDP,
		list_object_endp,STORE_R_NO_LIST_OBJECT_ENDP_FUNCTION);

	if (!s->meth->list_object_endp(s, handle))
		{
		STOREerr(STORE_F_STORE_LIST_PRIVATE_KEY_ENDP,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	return 1;
	}

EVP_PKEY *STORE_get_public_key(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	EVP_PKEY *pkey;

	check_store(s,STORE_F_STORE_GET_PUBLIC_KEY,
		get_object,STORE_R_NO_GET_OBJECT_FUNCTION);

	object = s->meth->get_object(s, STORE_OBJECT_TYPE_PUBLIC_KEY,
		attributes, parameters);
	if (!object || !object->data.key || !object->data.key)
		{
		STOREerr(STORE_F_STORE_GET_PUBLIC_KEY,
			STORE_R_FAILED_GETTING_KEY);
		return 0;
		}
	CRYPTO_add(&object->data.key->references,1,CRYPTO_LOCK_EVP_PKEY);
#ifdef REF_PRINT
	REF_PRINT("EVP_PKEY",data);
#endif
	pkey = object->data.key;
	STORE_OBJECT_free(object);
	return pkey;
	}

int STORE_store_public_key(STORE *s, EVP_PKEY *data, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	int i;

	check_store(s,STORE_F_STORE_STORE_PUBLIC_KEY,
		store_object,STORE_R_NO_STORE_OBJECT_FUNCTION);

	object = STORE_OBJECT_new();
	if (!object)
		{
		STOREerr(STORE_F_STORE_STORE_PUBLIC_KEY,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	object->data.key = EVP_PKEY_new();
	if (!object->data.key)
		{
		STOREerr(STORE_F_STORE_STORE_PUBLIC_KEY,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	
	CRYPTO_add(&data->references,1,CRYPTO_LOCK_EVP_PKEY);
#ifdef REF_PRINT
	REF_PRINT("EVP_PKEY",data);
#endif
	object->data.key = data;

	i = s->meth->store_object(s, STORE_OBJECT_TYPE_PUBLIC_KEY, object,
		attributes, parameters);

	STORE_OBJECT_free(object);

	if (!i)
		{
		STOREerr(STORE_F_STORE_STORE_PUBLIC_KEY,
			STORE_R_FAILED_STORING_KEY);
		return 0;
		}
	return i;
	}

int STORE_modify_public_key(STORE *s, OPENSSL_ITEM search_attributes[],
	OPENSSL_ITEM add_attributes[], OPENSSL_ITEM modify_attributes[],
	OPENSSL_ITEM delete_attributes[], OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_MODIFY_PUBLIC_KEY,
		modify_object,STORE_R_NO_MODIFY_OBJECT_FUNCTION);

	if (!s->meth->modify_object(s, STORE_OBJECT_TYPE_PUBLIC_KEY,
		    search_attributes, add_attributes, modify_attributes,
		    delete_attributes, parameters))
		{
		STOREerr(STORE_F_STORE_MODIFY_PUBLIC_KEY,
			STORE_R_FAILED_MODIFYING_PUBLIC_KEY);
		return 0;
		}
	return 1;
	}

int STORE_revoke_public_key(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	int i;

	check_store(s,STORE_F_STORE_REVOKE_PUBLIC_KEY,
		revoke_object,STORE_R_NO_REVOKE_OBJECT_FUNCTION);

	i = s->meth->revoke_object(s, STORE_OBJECT_TYPE_PUBLIC_KEY,
		attributes, parameters);

	if (!i)
		{
		STOREerr(STORE_F_STORE_REVOKE_PUBLIC_KEY,
			STORE_R_FAILED_REVOKING_KEY);
		return 0;
		}
	return i;
	}

int STORE_delete_public_key(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_DELETE_PUBLIC_KEY,
		delete_object,STORE_R_NO_DELETE_OBJECT_FUNCTION);
	
	if (!s->meth->delete_object(s, STORE_OBJECT_TYPE_PUBLIC_KEY,
		    attributes, parameters))
		{
		STOREerr(STORE_F_STORE_DELETE_PUBLIC_KEY,
			STORE_R_FAILED_DELETING_KEY);
		return 0;
		}
	return 1;
	}

void *STORE_list_public_key_start(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	void *handle;

	check_store(s,STORE_F_STORE_LIST_PUBLIC_KEY_START,
		list_object_start,STORE_R_NO_LIST_OBJECT_START_FUNCTION);

	handle = s->meth->list_object_start(s, STORE_OBJECT_TYPE_PUBLIC_KEY,
		attributes, parameters);
	if (!handle)
		{
		STOREerr(STORE_F_STORE_LIST_PUBLIC_KEY_START,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	return handle;
	}

EVP_PKEY *STORE_list_public_key_next(STORE *s, void *handle)
	{
	STORE_OBJECT *object;
	EVP_PKEY *pkey;

	check_store(s,STORE_F_STORE_LIST_PUBLIC_KEY_NEXT,
		list_object_next,STORE_R_NO_LIST_OBJECT_NEXT_FUNCTION);

	object = s->meth->list_object_next(s, handle);
	if (!object || !object->data.key || !object->data.key)
		{
		STOREerr(STORE_F_STORE_LIST_PUBLIC_KEY_NEXT,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	CRYPTO_add(&object->data.key->references,1,CRYPTO_LOCK_EVP_PKEY);
#ifdef REF_PRINT
	REF_PRINT("EVP_PKEY",data);
#endif
	pkey = object->data.key;
	STORE_OBJECT_free(object);
	return pkey;
	}

int STORE_list_public_key_end(STORE *s, void *handle)
	{
	check_store(s,STORE_F_STORE_LIST_PUBLIC_KEY_END,
		list_object_end,STORE_R_NO_LIST_OBJECT_END_FUNCTION);

	if (!s->meth->list_object_end(s, handle))
		{
		STOREerr(STORE_F_STORE_LIST_PUBLIC_KEY_END,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	return 1;
	}

int STORE_list_public_key_endp(STORE *s, void *handle)
	{
	check_store(s,STORE_F_STORE_LIST_PUBLIC_KEY_ENDP,
		list_object_endp,STORE_R_NO_LIST_OBJECT_ENDP_FUNCTION);

	if (!s->meth->list_object_endp(s, handle))
		{
		STOREerr(STORE_F_STORE_LIST_PUBLIC_KEY_ENDP,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	return 1;
	}

X509_CRL *STORE_generate_crl(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	X509_CRL *crl;

	check_store(s,STORE_F_STORE_GENERATE_CRL,
		generate_object,STORE_R_NO_GENERATE_CRL_FUNCTION);

	object = s->meth->generate_object(s, STORE_OBJECT_TYPE_X509_CRL,
		attributes, parameters);
	if (!object || !object->data.crl)
		{
		STOREerr(STORE_F_STORE_GENERATE_CRL,
			STORE_R_FAILED_GENERATING_CRL);
		return 0;
		}
	CRYPTO_add(&object->data.crl->references,1,CRYPTO_LOCK_X509_CRL);
#ifdef REF_PRINT
	REF_PRINT("X509_CRL",data);
#endif
	crl = object->data.crl;
	STORE_OBJECT_free(object);
	return crl;
	}

X509_CRL *STORE_get_crl(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	X509_CRL *crl;

	check_store(s,STORE_F_STORE_GET_CRL,
		get_object,STORE_R_NO_GET_OBJECT_FUNCTION);

	object = s->meth->get_object(s, STORE_OBJECT_TYPE_X509_CRL,
		attributes, parameters);
	if (!object || !object->data.crl)
		{
		STOREerr(STORE_F_STORE_GET_CRL,
			STORE_R_FAILED_GETTING_KEY);
		return 0;
		}
	CRYPTO_add(&object->data.crl->references,1,CRYPTO_LOCK_X509_CRL);
#ifdef REF_PRINT
	REF_PRINT("X509_CRL",data);
#endif
	crl = object->data.crl;
	STORE_OBJECT_free(object);
	return crl;
	}

int STORE_store_crl(STORE *s, X509_CRL *data, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	int i;

	check_store(s,STORE_F_STORE_STORE_CRL,
		store_object,STORE_R_NO_STORE_OBJECT_FUNCTION);

	object = STORE_OBJECT_new();
	if (!object)
		{
		STOREerr(STORE_F_STORE_STORE_CRL,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	
	CRYPTO_add(&data->references,1,CRYPTO_LOCK_X509_CRL);
#ifdef REF_PRINT
	REF_PRINT("X509_CRL",data);
#endif
	object->data.crl = data;

	i = s->meth->store_object(s, STORE_OBJECT_TYPE_X509_CRL, object,
		attributes, parameters);

	STORE_OBJECT_free(object);

	if (!i)
		{
		STOREerr(STORE_F_STORE_STORE_CRL,
			STORE_R_FAILED_STORING_KEY);
		return 0;
		}
	return i;
	}

int STORE_modify_crl(STORE *s, OPENSSL_ITEM search_attributes[],
	OPENSSL_ITEM add_attributes[], OPENSSL_ITEM modify_attributes[],
	OPENSSL_ITEM delete_attributes[], OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_MODIFY_CRL,
		modify_object,STORE_R_NO_MODIFY_OBJECT_FUNCTION);

	if (!s->meth->modify_object(s, STORE_OBJECT_TYPE_X509_CRL,
		    search_attributes, add_attributes, modify_attributes,
		    delete_attributes, parameters))
		{
		STOREerr(STORE_F_STORE_MODIFY_CRL,
			STORE_R_FAILED_MODIFYING_CRL);
		return 0;
		}
	return 1;
	}

int STORE_delete_crl(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_DELETE_CRL,
		delete_object,STORE_R_NO_DELETE_OBJECT_FUNCTION);
	
	if (!s->meth->delete_object(s, STORE_OBJECT_TYPE_X509_CRL,
		    attributes, parameters))
		{
		STOREerr(STORE_F_STORE_DELETE_CRL,
			STORE_R_FAILED_DELETING_KEY);
		return 0;
		}
	return 1;
	}

void *STORE_list_crl_start(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	void *handle;

	check_store(s,STORE_F_STORE_LIST_CRL_START,
		list_object_start,STORE_R_NO_LIST_OBJECT_START_FUNCTION);

	handle = s->meth->list_object_start(s, STORE_OBJECT_TYPE_X509_CRL,
		attributes, parameters);
	if (!handle)
		{
		STOREerr(STORE_F_STORE_LIST_CRL_START,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	return handle;
	}

X509_CRL *STORE_list_crl_next(STORE *s, void *handle)
	{
	STORE_OBJECT *object;
	X509_CRL *crl;

	check_store(s,STORE_F_STORE_LIST_CRL_NEXT,
		list_object_next,STORE_R_NO_LIST_OBJECT_NEXT_FUNCTION);

	object = s->meth->list_object_next(s, handle);
	if (!object || !object->data.crl)
		{
		STOREerr(STORE_F_STORE_LIST_CRL_NEXT,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	CRYPTO_add(&object->data.crl->references,1,CRYPTO_LOCK_X509_CRL);
#ifdef REF_PRINT
	REF_PRINT("X509_CRL",data);
#endif
	crl = object->data.crl;
	STORE_OBJECT_free(object);
	return crl;
	}

int STORE_list_crl_end(STORE *s, void *handle)
	{
	check_store(s,STORE_F_STORE_LIST_CRL_END,
		list_object_end,STORE_R_NO_LIST_OBJECT_END_FUNCTION);

	if (!s->meth->list_object_end(s, handle))
		{
		STOREerr(STORE_F_STORE_LIST_CRL_END,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	return 1;
	}

int STORE_list_crl_endp(STORE *s, void *handle)
	{
	check_store(s,STORE_F_STORE_LIST_CRL_ENDP,
		list_object_endp,STORE_R_NO_LIST_OBJECT_ENDP_FUNCTION);

	if (!s->meth->list_object_endp(s, handle))
		{
		STOREerr(STORE_F_STORE_LIST_CRL_ENDP,
			STORE_R_FAILED_LISTING_KEYS);
		return 0;
		}
	return 1;
	}

int STORE_store_number(STORE *s, BIGNUM *data, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	int i;

	check_store(s,STORE_F_STORE_STORE_NUMBER,
		store_object,STORE_R_NO_STORE_OBJECT_NUMBER_FUNCTION);

	object = STORE_OBJECT_new();
	if (!object)
		{
		STOREerr(STORE_F_STORE_STORE_NUMBER,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	
	object->data.number = data;

	i = s->meth->store_object(s, STORE_OBJECT_TYPE_NUMBER, object,
		attributes, parameters);

	STORE_OBJECT_free(object);

	if (!i)
		{
		STOREerr(STORE_F_STORE_STORE_NUMBER,
			STORE_R_FAILED_STORING_NUMBER);
		return 0;
		}
	return 1;
	}

int STORE_modify_number(STORE *s, OPENSSL_ITEM search_attributes[],
	OPENSSL_ITEM add_attributes[], OPENSSL_ITEM modify_attributes[],
	OPENSSL_ITEM delete_attributes[], OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_MODIFY_NUMBER,
		modify_object,STORE_R_NO_MODIFY_OBJECT_FUNCTION);

	if (!s->meth->modify_object(s, STORE_OBJECT_TYPE_NUMBER,
		    search_attributes, add_attributes, modify_attributes,
		    delete_attributes, parameters))
		{
		STOREerr(STORE_F_STORE_MODIFY_NUMBER,
			STORE_R_FAILED_MODIFYING_NUMBER);
		return 0;
		}
	return 1;
	}

BIGNUM *STORE_get_number(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	BIGNUM *n;

	check_store(s,STORE_F_STORE_GET_NUMBER,
		get_object,STORE_R_NO_GET_OBJECT_NUMBER_FUNCTION);

	object = s->meth->get_object(s, STORE_OBJECT_TYPE_NUMBER, attributes,
		parameters);
	if (!object || !object->data.number)
		{
		STOREerr(STORE_F_STORE_GET_NUMBER,
			STORE_R_FAILED_GETTING_NUMBER);
		return 0;
		}
	n = object->data.number;
	object->data.number = NULL;
	STORE_OBJECT_free(object);
	return n;
	}

int STORE_delete_number(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_DELETE_NUMBER,
		delete_object,STORE_R_NO_DELETE_NUMBER_FUNCTION);

	if (!s->meth->delete_object(s, STORE_OBJECT_TYPE_NUMBER, attributes,
		    parameters))
		{
		STOREerr(STORE_F_STORE_DELETE_NUMBER,
			STORE_R_FAILED_DELETING_NUMBER);
		return 0;
		}
	return 1;
	}

int STORE_store_arbitrary(STORE *s, BUF_MEM *data, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	int i;

	check_store(s,STORE_F_STORE_STORE_ARBITRARY,
		store_object,STORE_R_NO_STORE_OBJECT_ARBITRARY_FUNCTION);

	object = STORE_OBJECT_new();
	if (!object)
		{
		STOREerr(STORE_F_STORE_STORE_ARBITRARY,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	
	object->data.arbitrary = data;

	i = s->meth->store_object(s, STORE_OBJECT_TYPE_ARBITRARY, object,
		attributes, parameters);

	STORE_OBJECT_free(object);

	if (!i)
		{
		STOREerr(STORE_F_STORE_STORE_ARBITRARY,
			STORE_R_FAILED_STORING_ARBITRARY);
		return 0;
		}
	return 1;
	}

int STORE_modify_arbitrary(STORE *s, OPENSSL_ITEM search_attributes[],
	OPENSSL_ITEM add_attributes[], OPENSSL_ITEM modify_attributes[],
	OPENSSL_ITEM delete_attributes[], OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_MODIFY_ARBITRARY,
		modify_object,STORE_R_NO_MODIFY_OBJECT_FUNCTION);

	if (!s->meth->modify_object(s, STORE_OBJECT_TYPE_ARBITRARY,
		    search_attributes, add_attributes, modify_attributes,
		    delete_attributes, parameters))
		{
		STOREerr(STORE_F_STORE_MODIFY_ARBITRARY,
			STORE_R_FAILED_MODIFYING_ARBITRARY);
		return 0;
		}
	return 1;
	}

BUF_MEM *STORE_get_arbitrary(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	STORE_OBJECT *object;
	BUF_MEM *b;

	check_store(s,STORE_F_STORE_GET_ARBITRARY,
		get_object,STORE_R_NO_GET_OBJECT_ARBITRARY_FUNCTION);

	object = s->meth->get_object(s, STORE_OBJECT_TYPE_ARBITRARY,
		attributes, parameters);
	if (!object || !object->data.arbitrary)
		{
		STOREerr(STORE_F_STORE_GET_ARBITRARY,
			STORE_R_FAILED_GETTING_ARBITRARY);
		return 0;
		}
	b = object->data.arbitrary;
	object->data.arbitrary = NULL;
	STORE_OBJECT_free(object);
	return b;
	}

int STORE_delete_arbitrary(STORE *s, OPENSSL_ITEM attributes[],
	OPENSSL_ITEM parameters[])
	{
	check_store(s,STORE_F_STORE_DELETE_ARBITRARY,
		delete_object,STORE_R_NO_DELETE_ARBITRARY_FUNCTION);

	if (!s->meth->delete_object(s, STORE_OBJECT_TYPE_ARBITRARY, attributes,
		    parameters))
		{
		STOREerr(STORE_F_STORE_DELETE_ARBITRARY,
			STORE_R_FAILED_DELETING_ARBITRARY);
		return 0;
		}
	return 1;
	}

STORE_OBJECT *STORE_OBJECT_new(void)
	{
	STORE_OBJECT *object = OPENSSL_malloc(sizeof(STORE_OBJECT));
	if (object) memset(object, 0, sizeof(STORE_OBJECT));
	return object;
	}
void STORE_OBJECT_free(STORE_OBJECT *data)
	{
	if (!data) return;
	switch (data->type)
		{
	case STORE_OBJECT_TYPE_X509_CERTIFICATE:
		X509_free(data->data.x509.certificate);
		break;
	case STORE_OBJECT_TYPE_X509_CRL:
		X509_CRL_free(data->data.crl);
		break;
	case STORE_OBJECT_TYPE_PRIVATE_KEY:
	case STORE_OBJECT_TYPE_PUBLIC_KEY:
		EVP_PKEY_free(data->data.key);
		break;
	case STORE_OBJECT_TYPE_NUMBER:
		BN_free(data->data.number);
		break;
	case STORE_OBJECT_TYPE_ARBITRARY:
		BUF_MEM_free(data->data.arbitrary);
		break;
		}
	OPENSSL_free(data);
	}

IMPLEMENT_STACK_OF(STORE_OBJECT*)


struct STORE_attr_info_st
	{
	unsigned char set[(STORE_ATTR_TYPE_NUM + 8) / 8];
	union
		{
		char *cstring;
		unsigned char *sha1string;
		X509_NAME *dn;
		BIGNUM *number;
		void *any;
		} values[STORE_ATTR_TYPE_NUM+1];
	size_t value_sizes[STORE_ATTR_TYPE_NUM+1];
	};

#define ATTR_IS_SET(a,i)	((i) > 0 && (i) < STORE_ATTR_TYPE_NUM \
				&& ((a)->set[(i) / 8] & (1 << ((i) % 8))))
#define SET_ATTRBIT(a,i)	((a)->set[(i) / 8] |= (1 << ((i) % 8)))
#define CLEAR_ATTRBIT(a,i)	((a)->set[(i) / 8] &= ~(1 << ((i) % 8)))

STORE_ATTR_INFO *STORE_ATTR_INFO_new(void)
	{
	return (STORE_ATTR_INFO *)OPENSSL_malloc(sizeof(STORE_ATTR_INFO));
	}
static void STORE_ATTR_INFO_attr_free(STORE_ATTR_INFO *attrs,
	STORE_ATTR_TYPES code)
	{
	if (ATTR_IS_SET(attrs,code))
		{
		switch(code)
			{
		case STORE_ATTR_FRIENDLYNAME:
		case STORE_ATTR_EMAIL:
		case STORE_ATTR_FILENAME:
			STORE_ATTR_INFO_modify_cstr(attrs, code, NULL, 0);
			break;
		case STORE_ATTR_KEYID:
		case STORE_ATTR_ISSUERKEYID:
		case STORE_ATTR_SUBJECTKEYID:
		case STORE_ATTR_ISSUERSERIALHASH:
		case STORE_ATTR_CERTHASH:
			STORE_ATTR_INFO_modify_sha1str(attrs, code, NULL, 0);
			break;
		case STORE_ATTR_ISSUER:
		case STORE_ATTR_SUBJECT:
			STORE_ATTR_INFO_modify_dn(attrs, code, NULL);
			break;
		case STORE_ATTR_SERIAL:
			STORE_ATTR_INFO_modify_number(attrs, code, NULL);
			break;
		default:
			break;
			}
		}
	}
int STORE_ATTR_INFO_free(STORE_ATTR_INFO *attrs)
	{
	if (attrs)
		{
		STORE_ATTR_TYPES i;
		for(i = 0; i++ < STORE_ATTR_TYPE_NUM;)
			STORE_ATTR_INFO_attr_free(attrs, i);
		OPENSSL_free(attrs);
		}
	return 1;
	}
char *STORE_ATTR_INFO_get0_cstr(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_GET0_CSTR,
			ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
		}
	if (ATTR_IS_SET(attrs,code))
		return attrs->values[code].cstring;
	STOREerr(STORE_F_STORE_ATTR_INFO_GET0_CSTR,
		STORE_R_NO_VALUE);
	return NULL;
	}
unsigned char *STORE_ATTR_INFO_get0_sha1str(STORE_ATTR_INFO *attrs,
	STORE_ATTR_TYPES code)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_GET0_SHA1STR,
			ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
		}
	if (ATTR_IS_SET(attrs,code))
		return attrs->values[code].sha1string;
	STOREerr(STORE_F_STORE_ATTR_INFO_GET0_SHA1STR,
		STORE_R_NO_VALUE);
	return NULL;
	}
X509_NAME *STORE_ATTR_INFO_get0_dn(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_GET0_DN,
			ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
		}
	if (ATTR_IS_SET(attrs,code))
		return attrs->values[code].dn;
	STOREerr(STORE_F_STORE_ATTR_INFO_GET0_DN,
		STORE_R_NO_VALUE);
	return NULL;
	}
BIGNUM *STORE_ATTR_INFO_get0_number(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_GET0_NUMBER,
			ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
		}
	if (ATTR_IS_SET(attrs,code))
		return attrs->values[code].number;
	STOREerr(STORE_F_STORE_ATTR_INFO_GET0_NUMBER,
		STORE_R_NO_VALUE);
	return NULL;
	}
int STORE_ATTR_INFO_set_cstr(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
	char *cstr, size_t cstr_size)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_SET_CSTR,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (!ATTR_IS_SET(attrs,code))
		{
		if ((attrs->values[code].cstring = BUF_strndup(cstr, cstr_size)))
			return 1;
		STOREerr(STORE_F_STORE_ATTR_INFO_SET_CSTR,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	STOREerr(STORE_F_STORE_ATTR_INFO_SET_CSTR, STORE_R_ALREADY_HAS_A_VALUE);
	return 0;
	}
int STORE_ATTR_INFO_set_sha1str(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
	unsigned char *sha1str, size_t sha1str_size)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_SET_SHA1STR,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (!ATTR_IS_SET(attrs,code))
		{
		if ((attrs->values[code].sha1string =
			    (unsigned char *)BUF_memdup(sha1str,
				    sha1str_size)))
			return 1;
		STOREerr(STORE_F_STORE_ATTR_INFO_SET_SHA1STR,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	STOREerr(STORE_F_STORE_ATTR_INFO_SET_SHA1STR, STORE_R_ALREADY_HAS_A_VALUE);
	return 0;
	}
int STORE_ATTR_INFO_set_dn(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
	X509_NAME *dn)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_SET_DN,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (!ATTR_IS_SET(attrs,code))
		{
		if ((attrs->values[code].dn = X509_NAME_dup(dn)))
			return 1;
		STOREerr(STORE_F_STORE_ATTR_INFO_SET_DN,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	STOREerr(STORE_F_STORE_ATTR_INFO_SET_DN, STORE_R_ALREADY_HAS_A_VALUE);
	return 0;
	}
int STORE_ATTR_INFO_set_number(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
	BIGNUM *number)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_SET_NUMBER,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (!ATTR_IS_SET(attrs,code))
		{
		if ((attrs->values[code].number = BN_dup(number)))
			return 1;
		STOREerr(STORE_F_STORE_ATTR_INFO_SET_NUMBER,
			ERR_R_MALLOC_FAILURE);
		return 0;
		}
	STOREerr(STORE_F_STORE_ATTR_INFO_SET_NUMBER, STORE_R_ALREADY_HAS_A_VALUE);
	return 0;
	}
int STORE_ATTR_INFO_modify_cstr(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
	char *cstr, size_t cstr_size)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_MODIFY_CSTR,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (ATTR_IS_SET(attrs,code))
		{
		OPENSSL_free(attrs->values[code].cstring);
		attrs->values[code].cstring = NULL;
		CLEAR_ATTRBIT(attrs, code);
		}
	return STORE_ATTR_INFO_set_cstr(attrs, code, cstr, cstr_size);
	}
int STORE_ATTR_INFO_modify_sha1str(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
	unsigned char *sha1str, size_t sha1str_size)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_MODIFY_SHA1STR,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (ATTR_IS_SET(attrs,code))
		{
		OPENSSL_free(attrs->values[code].sha1string);
		attrs->values[code].sha1string = NULL;
		CLEAR_ATTRBIT(attrs, code);
		}
	return STORE_ATTR_INFO_set_sha1str(attrs, code, sha1str, sha1str_size);
	}
int STORE_ATTR_INFO_modify_dn(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
	X509_NAME *dn)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_MODIFY_DN,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (ATTR_IS_SET(attrs,code))
		{
		OPENSSL_free(attrs->values[code].dn);
		attrs->values[code].dn = NULL;
		CLEAR_ATTRBIT(attrs, code);
		}
	return STORE_ATTR_INFO_set_dn(attrs, code, dn);
	}
int STORE_ATTR_INFO_modify_number(STORE_ATTR_INFO *attrs, STORE_ATTR_TYPES code,
	BIGNUM *number)
	{
	if (!attrs)
		{
		STOREerr(STORE_F_STORE_ATTR_INFO_MODIFY_NUMBER,
			ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (ATTR_IS_SET(attrs,code))
		{
		OPENSSL_free(attrs->values[code].number);
		attrs->values[code].number = NULL;
		CLEAR_ATTRBIT(attrs, code);
		}
	return STORE_ATTR_INFO_set_number(attrs, code, number);
	}

struct attr_list_ctx_st
	{
	OPENSSL_ITEM *attributes;
	};
void *STORE_parse_attrs_start(OPENSSL_ITEM *attributes)
	{
	if (attributes)
		{
		struct attr_list_ctx_st *context =
			(struct attr_list_ctx_st *)OPENSSL_malloc(sizeof(struct attr_list_ctx_st));
		if (context)
			context->attributes = attributes;
		else
			STOREerr(STORE_F_STORE_PARSE_ATTRS_START,
				ERR_R_MALLOC_FAILURE);
		return context;
		}
	STOREerr(STORE_F_STORE_PARSE_ATTRS_START, ERR_R_PASSED_NULL_PARAMETER);
	return 0;
	}
STORE_ATTR_INFO *STORE_parse_attrs_next(void *handle)
	{
	struct attr_list_ctx_st *context = (struct attr_list_ctx_st *)handle;

	if (context && context->attributes)
		{
		STORE_ATTR_INFO *attrs = NULL;

		while(context->attributes
			&& context->attributes->code != STORE_ATTR_OR
			&& context->attributes->code != STORE_ATTR_END)
			{
			switch(context->attributes->code)
				{
			case STORE_ATTR_FRIENDLYNAME:
			case STORE_ATTR_EMAIL:
			case STORE_ATTR_FILENAME:
				if (!attrs) attrs = STORE_ATTR_INFO_new();
				if (attrs == NULL)
					{
					STOREerr(STORE_F_STORE_PARSE_ATTRS_NEXT,
						ERR_R_MALLOC_FAILURE);
					goto err;
					}
				STORE_ATTR_INFO_set_cstr(attrs,
					context->attributes->code,
					context->attributes->value,
					context->attributes->value_size);
				break;
			case STORE_ATTR_KEYID:
			case STORE_ATTR_ISSUERKEYID:
			case STORE_ATTR_SUBJECTKEYID:
			case STORE_ATTR_ISSUERSERIALHASH:
			case STORE_ATTR_CERTHASH:
				if (!attrs) attrs = STORE_ATTR_INFO_new();
				if (attrs == NULL)
					{
					STOREerr(STORE_F_STORE_PARSE_ATTRS_NEXT,
						ERR_R_MALLOC_FAILURE);
					goto err;
					}
				STORE_ATTR_INFO_set_sha1str(attrs,
					context->attributes->code,
					context->attributes->value,
					context->attributes->value_size);
				break;
			case STORE_ATTR_ISSUER:
			case STORE_ATTR_SUBJECT:
				if (!attrs) attrs = STORE_ATTR_INFO_new();
				if (attrs == NULL)
					{
					STOREerr(STORE_F_STORE_PARSE_ATTRS_NEXT,
						ERR_R_MALLOC_FAILURE);
					goto err;
					}
				STORE_ATTR_INFO_modify_dn(attrs,
					context->attributes->code,
					context->attributes->value);
				break;
			case STORE_ATTR_SERIAL:
				if (!attrs) attrs = STORE_ATTR_INFO_new();
				if (attrs == NULL)
					{
					STOREerr(STORE_F_STORE_PARSE_ATTRS_NEXT,
						ERR_R_MALLOC_FAILURE);
					goto err;
					}
				STORE_ATTR_INFO_modify_number(attrs,
					context->attributes->code,
					context->attributes->value);
				break;
				}
			context->attributes++;
			}
		if (context->attributes->code == STORE_ATTR_OR)
			context->attributes++;
		return attrs;
	err:
		while(context->attributes
			&& context->attributes->code != STORE_ATTR_OR
			&& context->attributes->code != STORE_ATTR_END)
			context->attributes++;
		if (context->attributes->code == STORE_ATTR_OR)
			context->attributes++;
		return NULL;
		}
	STOREerr(STORE_F_STORE_PARSE_ATTRS_NEXT, ERR_R_PASSED_NULL_PARAMETER);
	return NULL;
	}
int STORE_parse_attrs_end(void *handle)
	{
	struct attr_list_ctx_st *context = (struct attr_list_ctx_st *)handle;

	if (context && context->attributes)
		{
#if 0
		OPENSSL_ITEM *attributes = context->attributes;
#endif
		OPENSSL_free(context);
		return 1;
		}
	STOREerr(STORE_F_STORE_PARSE_ATTRS_END, ERR_R_PASSED_NULL_PARAMETER);
	return 0;
	}

int STORE_parse_attrs_endp(void *handle)
	{
	struct attr_list_ctx_st *context = (struct attr_list_ctx_st *)handle;

	if (context && context->attributes)
		{
		return context->attributes->code == STORE_ATTR_END;
		}
	STOREerr(STORE_F_STORE_PARSE_ATTRS_ENDP, ERR_R_PASSED_NULL_PARAMETER);
	return 0;
	}

static int attr_info_compare_compute_range(
	unsigned char *abits, unsigned char *bbits,
	unsigned int *alowp, unsigned int *ahighp,
	unsigned int *blowp, unsigned int *bhighp)
	{
	unsigned int alow = (unsigned int)-1, ahigh = 0;
	unsigned int blow = (unsigned int)-1, bhigh = 0;
	int i, res = 0;

	for (i = 0; i < (STORE_ATTR_TYPE_NUM + 8) / 8; i++, abits++, bbits++)
		{
		if (res == 0)
			{
			if (*abits < *bbits) res = -1;
			if (*abits > *bbits) res = 1;
			}
		if (*abits)
			{
			if (alow == (unsigned int)-1)
				{
				alow = i * 8;
				if (!(*abits & 0x01)) alow++;
				if (!(*abits & 0x02)) alow++;
				if (!(*abits & 0x04)) alow++;
				if (!(*abits & 0x08)) alow++;
				if (!(*abits & 0x10)) alow++;
				if (!(*abits & 0x20)) alow++;
				if (!(*abits & 0x40)) alow++;
				}
			ahigh = i * 8 + 7;
			if (!(*abits & 0x80)) ahigh++;
			if (!(*abits & 0x40)) ahigh++;
			if (!(*abits & 0x20)) ahigh++;
			if (!(*abits & 0x10)) ahigh++;
			if (!(*abits & 0x08)) ahigh++;
			if (!(*abits & 0x04)) ahigh++;
			if (!(*abits & 0x02)) ahigh++;
			}
		if (*bbits)
			{
			if (blow == (unsigned int)-1)
				{
				blow = i * 8;
				if (!(*bbits & 0x01)) blow++;
				if (!(*bbits & 0x02)) blow++;
				if (!(*bbits & 0x04)) blow++;
				if (!(*bbits & 0x08)) blow++;
				if (!(*bbits & 0x10)) blow++;
				if (!(*bbits & 0x20)) blow++;
				if (!(*bbits & 0x40)) blow++;
				}
			bhigh = i * 8 + 7;
			if (!(*bbits & 0x80)) bhigh++;
			if (!(*bbits & 0x40)) bhigh++;
			if (!(*bbits & 0x20)) bhigh++;
			if (!(*bbits & 0x10)) bhigh++;
			if (!(*bbits & 0x08)) bhigh++;
			if (!(*bbits & 0x04)) bhigh++;
			if (!(*bbits & 0x02)) bhigh++;
			}
		}
	if (ahigh + alow < bhigh + blow) res = -1;
	if (ahigh + alow > bhigh + blow) res = 1;
	if (alowp) *alowp = alow;
	if (ahighp) *ahighp = ahigh;
	if (blowp) *blowp = blow;
	if (bhighp) *bhighp = bhigh;
	return res;
	}

int STORE_ATTR_INFO_compare(STORE_ATTR_INFO *a, STORE_ATTR_INFO *b)
	{
	if (a == b) return 0;
	if (!a) return -1;
	if (!b) return 1;
	return attr_info_compare_compute_range(a->set, b->set, 0, 0, 0, 0);
	}
int STORE_ATTR_INFO_in_range(STORE_ATTR_INFO *a, STORE_ATTR_INFO *b)
	{
	unsigned int alow, ahigh, blow, bhigh;

	if (a == b) return 1;
	if (!a) return 0;
	if (!b) return 0;
	attr_info_compare_compute_range(a->set, b->set,
		&alow, &ahigh, &blow, &bhigh);
	if (alow >= blow && ahigh <= bhigh)
		return 1;
	return 0;
	}
int STORE_ATTR_INFO_in(STORE_ATTR_INFO *a, STORE_ATTR_INFO *b)
	{
	unsigned char *abits, *bbits;
	int i;

	if (a == b) return 1;
	if (!a) return 0;
	if (!b) return 0;
	abits = a->set;
	bbits = b->set;
	for (i = 0; i < (STORE_ATTR_TYPE_NUM + 8) / 8; i++, abits++, bbits++)
		{
		if (*abits && (*bbits & *abits) != *abits)
			return 0;
		}
	return 1;
	}
int STORE_ATTR_INFO_in_ex(STORE_ATTR_INFO *a, STORE_ATTR_INFO *b)
	{
	STORE_ATTR_TYPES i;

	if (a == b) return 1;
	if (!STORE_ATTR_INFO_in(a, b)) return 0;
	for (i = 1; i < STORE_ATTR_TYPE_NUM; i++)
		if (ATTR_IS_SET(a, i))
			{
			switch(i)
				{
			case STORE_ATTR_FRIENDLYNAME:
			case STORE_ATTR_EMAIL:
			case STORE_ATTR_FILENAME:
				if (strcmp(a->values[i].cstring,
					    b->values[i].cstring))
					return 0;
				break;
			case STORE_ATTR_KEYID:
			case STORE_ATTR_ISSUERKEYID:
			case STORE_ATTR_SUBJECTKEYID:
			case STORE_ATTR_ISSUERSERIALHASH:
			case STORE_ATTR_CERTHASH:
				if (memcmp(a->values[i].sha1string,
					    b->values[i].sha1string,
					    a->value_sizes[i]))
					return 0;
				break;
			case STORE_ATTR_ISSUER:
			case STORE_ATTR_SUBJECT:
				if (X509_NAME_cmp(a->values[i].dn,
					    b->values[i].dn))
					return 0;
				break;
			case STORE_ATTR_SERIAL:
				if (BN_cmp(a->values[i].number,
					    b->values[i].number))
					return 0;
				break;
			default:
				break;
				}
			}

	return 1;
	}
