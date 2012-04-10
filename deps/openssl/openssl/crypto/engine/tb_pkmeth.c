/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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
#include <openssl/evp.h>

/* If this symbol is defined then ENGINE_get_pkey_meth_engine(), the function
 * that is used by EVP to hook in pkey_meth code and cache defaults (etc), will
 * display brief debugging summaries to stderr with the 'nid'. */
/* #define ENGINE_PKEY_METH_DEBUG */

static ENGINE_TABLE *pkey_meth_table = NULL;

void ENGINE_unregister_pkey_meths(ENGINE *e)
	{
	engine_table_unregister(&pkey_meth_table, e);
	}

static void engine_unregister_all_pkey_meths(void)
	{
	engine_table_cleanup(&pkey_meth_table);
	}

int ENGINE_register_pkey_meths(ENGINE *e)
	{
	if(e->pkey_meths)
		{
		const int *nids;
		int num_nids = e->pkey_meths(e, NULL, &nids, 0);
		if(num_nids > 0)
			return engine_table_register(&pkey_meth_table,
				engine_unregister_all_pkey_meths, e, nids,
					num_nids, 0);
		}
	return 1;
	}

void ENGINE_register_all_pkey_meths()
	{
	ENGINE *e;

	for(e=ENGINE_get_first() ; e ; e=ENGINE_get_next(e))
		ENGINE_register_pkey_meths(e);
	}

int ENGINE_set_default_pkey_meths(ENGINE *e)
	{
	if(e->pkey_meths)
		{
		const int *nids;
		int num_nids = e->pkey_meths(e, NULL, &nids, 0);
		if(num_nids > 0)
			return engine_table_register(&pkey_meth_table,
				engine_unregister_all_pkey_meths, e, nids,
					num_nids, 1);
		}
	return 1;
	}

/* Exposed API function to get a functional reference from the implementation
 * table (ie. try to get a functional reference from the tabled structural
 * references) for a given pkey_meth 'nid' */
ENGINE *ENGINE_get_pkey_meth_engine(int nid)
	{
	return engine_table_select(&pkey_meth_table, nid);
	}

/* Obtains a pkey_meth implementation from an ENGINE functional reference */
const EVP_PKEY_METHOD *ENGINE_get_pkey_meth(ENGINE *e, int nid)
	{
	EVP_PKEY_METHOD *ret;
	ENGINE_PKEY_METHS_PTR fn = ENGINE_get_pkey_meths(e);
	if(!fn || !fn(e, &ret, NULL, nid))
		{
		ENGINEerr(ENGINE_F_ENGINE_GET_PKEY_METH,
				ENGINE_R_UNIMPLEMENTED_PUBLIC_KEY_METHOD);
		return NULL;
		}
	return ret;
	}

/* Gets the pkey_meth callback from an ENGINE structure */
ENGINE_PKEY_METHS_PTR ENGINE_get_pkey_meths(const ENGINE *e)
	{
	return e->pkey_meths;
	}

/* Sets the pkey_meth callback in an ENGINE structure */
int ENGINE_set_pkey_meths(ENGINE *e, ENGINE_PKEY_METHS_PTR f)
	{
	e->pkey_meths = f;
	return 1;
	}

/* Internal function to free up EVP_PKEY_METHOD structures before an
 * ENGINE is destroyed
 */

void engine_pkey_meths_free(ENGINE *e)
	{
	int i;
	EVP_PKEY_METHOD *pkm;
	if (e->pkey_meths)
		{
		const int *pknids;
		int npknids;
		npknids = e->pkey_meths(e, NULL, &pknids, 0);
		for (i = 0; i < npknids; i++)
			{
			if (e->pkey_meths(e, &pkm, NULL, pknids[i]))
				{
				EVP_PKEY_meth_free(pkm);
				}
			}
		}
	}
