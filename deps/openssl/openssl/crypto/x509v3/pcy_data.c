/* pcy_data.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
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
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "pcy_int.h"

/* Policy Node routines */

void policy_data_free(X509_POLICY_DATA *data)
	{
	ASN1_OBJECT_free(data->valid_policy);
	/* Don't free qualifiers if shared */
	if (!(data->flags & POLICY_DATA_FLAG_SHARED_QUALIFIERS))
		sk_POLICYQUALINFO_pop_free(data->qualifier_set,
					POLICYQUALINFO_free);
	sk_ASN1_OBJECT_pop_free(data->expected_policy_set, ASN1_OBJECT_free);
	OPENSSL_free(data);
	}

/* Create a data based on an existing policy. If 'id' is NULL use the
 * oid in the policy, otherwise use 'id'. This behaviour covers the two
 * types of data in RFC3280: data with from a CertificatePolcies extension
 * and additional data with just the qualifiers of anyPolicy and ID from
 * another source.
 */

X509_POLICY_DATA *policy_data_new(POLICYINFO *policy,
					const ASN1_OBJECT *cid, int crit)
	{
	X509_POLICY_DATA *ret;
	ASN1_OBJECT *id;
	if (!policy && !cid)
		return NULL;
	if (cid)
		{
		id = OBJ_dup(cid);
		if (!id)
			return NULL;
		}
	else
		id = NULL;
	ret = OPENSSL_malloc(sizeof(X509_POLICY_DATA));
	if (!ret)
		return NULL;
	ret->expected_policy_set = sk_ASN1_OBJECT_new_null();
	if (!ret->expected_policy_set)
		{
		OPENSSL_free(ret);
		if (id)
			ASN1_OBJECT_free(id);
		return NULL;
		}

	if (crit)
		ret->flags = POLICY_DATA_FLAG_CRITICAL;
	else
		ret->flags = 0;

	if (id)
		ret->valid_policy = id;
	else
		{
		ret->valid_policy = policy->policyid;
		policy->policyid = NULL;
		}

	if (policy)
		{
		ret->qualifier_set = policy->qualifiers;
		policy->qualifiers = NULL;
		}
	else
		ret->qualifier_set = NULL;

	return ret;
	}

