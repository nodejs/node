/* crypto/store/str_locl.h -*- mode:C; c-file-style: "eay" -*- */
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

#ifndef HEADER_STORE_LOCL_H
#define HEADER_STORE_LOCL_H

#include <openssl/crypto.h>
#include <openssl/store.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct store_method_st
	{
	char *name;

	/* All the functions return a positive integer or non-NULL for success
	   and 0, a negative integer or NULL for failure */

	/* Initialise the STORE with private data */
	STORE_INITIALISE_FUNC_PTR init;
	/* Initialise the STORE with private data */
	STORE_CLEANUP_FUNC_PTR clean;
	/* Generate an object of a given type */
	STORE_GENERATE_OBJECT_FUNC_PTR generate_object;
	/* Get an object of a given type.  This function isn't really very
	   useful since the listing functions (below) can be used for the
	   same purpose and are much more general. */
	STORE_GET_OBJECT_FUNC_PTR get_object;
	/* Store an object of a given type. */
	STORE_STORE_OBJECT_FUNC_PTR store_object;
	/* Modify the attributes bound to an object of a given type. */
	STORE_MODIFY_OBJECT_FUNC_PTR modify_object;
	/* Revoke an object of a given type. */
	STORE_HANDLE_OBJECT_FUNC_PTR revoke_object;
	/* Delete an object of a given type. */
	STORE_HANDLE_OBJECT_FUNC_PTR delete_object;
	/* List a bunch of objects of a given type and with the associated
	   attributes. */
	STORE_START_OBJECT_FUNC_PTR list_object_start;
	STORE_NEXT_OBJECT_FUNC_PTR list_object_next;
	STORE_END_OBJECT_FUNC_PTR list_object_end;
	STORE_END_OBJECT_FUNC_PTR list_object_endp;
	/* Store-level function to make any necessary update operations. */
	STORE_GENERIC_FUNC_PTR update_store;
	/* Store-level function to get exclusive access to the store. */
	STORE_GENERIC_FUNC_PTR lock_store;
	/* Store-level function to release exclusive access to the store. */
	STORE_GENERIC_FUNC_PTR unlock_store;

	/* Generic control function */
	STORE_CTRL_FUNC_PTR ctrl;
	};

struct store_st
	{
	const STORE_METHOD *meth;
	/* functional reference if 'meth' is ENGINE-provided */
	ENGINE *engine;

	CRYPTO_EX_DATA ex_data;
	int references;
	};
#ifdef  __cplusplus
}
#endif

#endif
