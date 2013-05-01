/* crypto/objects/obj_xref.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
 */
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

#include <openssl/objects.h>
#include "obj_xref.h"

DECLARE_STACK_OF(nid_triple)
STACK_OF(nid_triple) *sig_app, *sigx_app;

static int sig_cmp(const nid_triple *a, const nid_triple *b)
	{
	return a->sign_id - b->sign_id;
	}

DECLARE_OBJ_BSEARCH_CMP_FN(nid_triple, nid_triple, sig);
IMPLEMENT_OBJ_BSEARCH_CMP_FN(nid_triple, nid_triple, sig);

static int sig_sk_cmp(const nid_triple * const *a, const nid_triple * const *b)
	{
	return (*a)->sign_id - (*b)->sign_id;
	}

DECLARE_OBJ_BSEARCH_CMP_FN(const nid_triple *, const nid_triple *, sigx);

static int sigx_cmp(const nid_triple * const *a, const nid_triple * const *b)
	{
	int ret;
	ret = (*a)->hash_id - (*b)->hash_id;
	if (ret)
		return ret;
	return (*a)->pkey_id - (*b)->pkey_id;
	}

IMPLEMENT_OBJ_BSEARCH_CMP_FN(const nid_triple *, const nid_triple *, sigx);

int OBJ_find_sigid_algs(int signid, int *pdig_nid, int *ppkey_nid)
	{
	nid_triple tmp;
	const nid_triple *rv = NULL;
	tmp.sign_id = signid;

	if (sig_app)
		{
		int idx = sk_nid_triple_find(sig_app, &tmp);
		if (idx >= 0)
			rv = sk_nid_triple_value(sig_app, idx);
		}

#ifndef OBJ_XREF_TEST2
	if (rv == NULL)
		{
		rv = OBJ_bsearch_sig(&tmp, sigoid_srt,
				 sizeof(sigoid_srt) / sizeof(nid_triple));
		}
#endif
	if (rv == NULL)
		return 0;
	if (pdig_nid)
		*pdig_nid = rv->hash_id;
	if (ppkey_nid)
		*ppkey_nid = rv->pkey_id;
	return 1;
	}

int OBJ_find_sigid_by_algs(int *psignid, int dig_nid, int pkey_nid)
	{
	nid_triple tmp;
	const nid_triple *t=&tmp;
	const nid_triple **rv = NULL;

	tmp.hash_id = dig_nid;
	tmp.pkey_id = pkey_nid;

	if (sigx_app)
		{
		int idx = sk_nid_triple_find(sigx_app, &tmp);
		if (idx >= 0)
			{
			t = sk_nid_triple_value(sigx_app, idx);
			rv = &t;
			}
		}

#ifndef OBJ_XREF_TEST2
	if (rv == NULL)
		{
		rv = OBJ_bsearch_sigx(&t, sigoid_srt_xref,
				 sizeof(sigoid_srt_xref) / sizeof(nid_triple *)
				 );
		}
#endif
	if (rv == NULL)
		return 0;
	if (psignid)
		*psignid = (*rv)->sign_id;
	return 1;
	}

int OBJ_add_sigid(int signid, int dig_id, int pkey_id)
	{
	nid_triple *ntr;
	if (!sig_app)
		sig_app = sk_nid_triple_new(sig_sk_cmp);
	if (!sig_app)
		return 0;
	if (!sigx_app)
		sigx_app = sk_nid_triple_new(sigx_cmp);
	if (!sigx_app)
		return 0;
	ntr = OPENSSL_malloc(sizeof(int) * 3);
	if (!ntr)
		return 0;
	ntr->sign_id = signid;
	ntr->hash_id = dig_id;
	ntr->pkey_id = pkey_id;

	if (!sk_nid_triple_push(sig_app, ntr))
		{
		OPENSSL_free(ntr);
		return 0;
		}

	if (!sk_nid_triple_push(sigx_app, ntr))
		return 0;

	sk_nid_triple_sort(sig_app);
	sk_nid_triple_sort(sigx_app);

	return 1;
	}

static void sid_free(nid_triple *tt)
	{
	OPENSSL_free(tt);
	}

void OBJ_sigid_free(void)
	{
	if (sig_app)
		{
		sk_nid_triple_pop_free(sig_app, sid_free);
		sig_app = NULL;
		}
	if (sigx_app)
		{
		sk_nid_triple_free(sigx_app);
		sigx_app = NULL;
		}
	}
		
#ifdef OBJ_XREF_TEST

main()
	{
	int n1, n2, n3;

	int i, rv;
#ifdef OBJ_XREF_TEST2
	for (i = 0; i <	sizeof(sigoid_srt) / sizeof(nid_triple); i++)
		{
		OBJ_add_sigid(sigoid_srt[i][0], sigoid_srt[i][1],
				sigoid_srt[i][2]);
		}
#endif

	for (i = 0; i <	sizeof(sigoid_srt) / sizeof(nid_triple); i++)
		{
		n1 = sigoid_srt[i][0];
		rv = OBJ_find_sigid_algs(n1, &n2, &n3);
		printf("Forward: %d, %s %s %s\n", rv,
			OBJ_nid2ln(n1), OBJ_nid2ln(n2), OBJ_nid2ln(n3));
		n1=0;
		rv = OBJ_find_sigid_by_algs(&n1, n2, n3);
		printf("Reverse: %d, %s %s %s\n", rv,
			OBJ_nid2ln(n1), OBJ_nid2ln(n2), OBJ_nid2ln(n3));
		}
	}
	
#endif
