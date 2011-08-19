/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
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

/* If this symbol is defined then ENGINE_get_digest_engine(), the function that
 * is used by EVP to hook in digest code and cache defaults (etc), will display
 * brief debugging summaries to stderr with the 'nid'. */
/* #define ENGINE_DIGEST_DEBUG */

static ENGINE_TABLE *digest_table = NULL;

void ENGINE_unregister_digests(ENGINE *e)
	{
	engine_table_unregister(&digest_table, e);
	}

static void engine_unregister_all_digests(void)
	{
	engine_table_cleanup(&digest_table);
	}

int ENGINE_register_digests(ENGINE *e)
	{
	if(e->digests)
		{
		const int *nids;
		int num_nids = e->digests(e, NULL, &nids, 0);
		if(num_nids > 0)
			return engine_table_register(&digest_table,
					engine_unregister_all_digests, e, nids,
					num_nids, 0);
		}
	return 1;
	}

void ENGINE_register_all_digests()
	{
	ENGINE *e;

	for(e=ENGINE_get_first() ; e ; e=ENGINE_get_next(e))
		ENGINE_register_digests(e);
	}

int ENGINE_set_default_digests(ENGINE *e)
	{
	if(e->digests)
		{
		const int *nids;
		int num_nids = e->digests(e, NULL, &nids, 0);
		if(num_nids > 0)
			return engine_table_register(&digest_table,
					engine_unregister_all_digests, e, nids,
					num_nids, 1);
		}
	return 1;
	}

/* Exposed API function to get a functional reference from the implementation
 * table (ie. try to get a functional reference from the tabled structural
 * references) for a given digest 'nid' */
ENGINE *ENGINE_get_digest_engine(int nid)
	{
	return engine_table_select(&digest_table, nid);
	}

/* Obtains a digest implementation from an ENGINE functional reference */
const EVP_MD *ENGINE_get_digest(ENGINE *e, int nid)
	{
	const EVP_MD *ret;
	ENGINE_DIGESTS_PTR fn = ENGINE_get_digests(e);
	if(!fn || !fn(e, &ret, NULL, nid))
		{
		ENGINEerr(ENGINE_F_ENGINE_GET_DIGEST,
				ENGINE_R_UNIMPLEMENTED_DIGEST);
		return NULL;
		}
	return ret;
	}

/* Gets the digest callback from an ENGINE structure */
ENGINE_DIGESTS_PTR ENGINE_get_digests(const ENGINE *e)
	{
	return e->digests;
	}

/* Sets the digest callback in an ENGINE structure */
int ENGINE_set_digests(ENGINE *e, ENGINE_DIGESTS_PTR f)
	{
	e->digests = f;
	return 1;
	}
