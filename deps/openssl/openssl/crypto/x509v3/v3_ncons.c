/* v3_ncons.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
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


#include <stdio.h>
#include "cryptlib.h"
#include <openssl/asn1t.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>

static void *v2i_NAME_CONSTRAINTS(X509V3_EXT_METHOD *method,
				X509V3_CTX *ctx, STACK_OF(CONF_VALUE) *nval);
static int i2r_NAME_CONSTRAINTS(X509V3_EXT_METHOD *method, 
				void *a, BIO *bp, int ind);
static int do_i2r_name_constraints(X509V3_EXT_METHOD *method,
				STACK_OF(GENERAL_SUBTREE) *trees,
					BIO *bp, int ind, char *name);
static int print_nc_ipadd(BIO *bp, ASN1_OCTET_STRING *ip);

const X509V3_EXT_METHOD v3_name_constraints = {
	NID_name_constraints, 0,
	ASN1_ITEM_ref(NAME_CONSTRAINTS),
	0,0,0,0,
	0,0,
	0, v2i_NAME_CONSTRAINTS,
	i2r_NAME_CONSTRAINTS,0,
	NULL
};

ASN1_SEQUENCE(GENERAL_SUBTREE) = {
	ASN1_SIMPLE(GENERAL_SUBTREE, base, GENERAL_NAME),
	ASN1_IMP_OPT(GENERAL_SUBTREE, minimum, ASN1_INTEGER, 0),
	ASN1_IMP_OPT(GENERAL_SUBTREE, maximum, ASN1_INTEGER, 1)
} ASN1_SEQUENCE_END(GENERAL_SUBTREE)

ASN1_SEQUENCE(NAME_CONSTRAINTS) = {
	ASN1_IMP_SEQUENCE_OF_OPT(NAME_CONSTRAINTS, permittedSubtrees,
							GENERAL_SUBTREE, 0),
	ASN1_IMP_SEQUENCE_OF_OPT(NAME_CONSTRAINTS, excludedSubtrees,
							GENERAL_SUBTREE, 1),
} ASN1_SEQUENCE_END(NAME_CONSTRAINTS)
	

IMPLEMENT_ASN1_ALLOC_FUNCTIONS(GENERAL_SUBTREE)
IMPLEMENT_ASN1_ALLOC_FUNCTIONS(NAME_CONSTRAINTS)

static void *v2i_NAME_CONSTRAINTS(X509V3_EXT_METHOD *method,
				X509V3_CTX *ctx, STACK_OF(CONF_VALUE) *nval)
	{
	int i;
	CONF_VALUE tval, *val;
	STACK_OF(GENERAL_SUBTREE) **ptree = NULL;
	NAME_CONSTRAINTS *ncons = NULL;
	GENERAL_SUBTREE *sub = NULL;
	ncons = NAME_CONSTRAINTS_new();
	if (!ncons)
		goto memerr;
	for(i = 0; i < sk_CONF_VALUE_num(nval); i++)
		{
		val = sk_CONF_VALUE_value(nval, i);
		if (!strncmp(val->name, "permitted", 9) && val->name[9])
			{
			ptree = &ncons->permittedSubtrees;
			tval.name = val->name + 10;
			}
		else if (!strncmp(val->name, "excluded", 8) && val->name[8])
			{
			ptree = &ncons->excludedSubtrees;
			tval.name = val->name + 9;
			}
		else
			{
			X509V3err(X509V3_F_V2I_NAME_CONSTRAINTS, X509V3_R_INVALID_SYNTAX);
			goto err;
			}
		tval.value = val->value;
		sub = GENERAL_SUBTREE_new();
		if (!v2i_GENERAL_NAME_ex(sub->base, method, ctx, &tval, 1))
			goto err;
		if (!*ptree)
			*ptree = sk_GENERAL_SUBTREE_new_null();
		if (!*ptree || !sk_GENERAL_SUBTREE_push(*ptree, sub))
			goto memerr;
		sub = NULL;
		}

	return ncons;

	memerr:
	X509V3err(X509V3_F_V2I_NAME_CONSTRAINTS, ERR_R_MALLOC_FAILURE);
	err:
	if (ncons)
		NAME_CONSTRAINTS_free(ncons);
	if (sub)
		GENERAL_SUBTREE_free(sub);

	return NULL;
	}
			

	

static int i2r_NAME_CONSTRAINTS(X509V3_EXT_METHOD *method,
				void *a, BIO *bp, int ind)
	{
	NAME_CONSTRAINTS *ncons = a;
	do_i2r_name_constraints(method, ncons->permittedSubtrees,
					bp, ind, "Permitted");
	do_i2r_name_constraints(method, ncons->excludedSubtrees,
					bp, ind, "Excluded");
	return 1;
	}

static int do_i2r_name_constraints(X509V3_EXT_METHOD *method,
				STACK_OF(GENERAL_SUBTREE) *trees,
					BIO *bp, int ind, char *name)
	{
	GENERAL_SUBTREE *tree;
	int i;
	if (sk_GENERAL_SUBTREE_num(trees) > 0)
		BIO_printf(bp, "%*s%s:\n", ind, "", name);
	for(i = 0; i < sk_GENERAL_SUBTREE_num(trees); i++)
		{
		tree = sk_GENERAL_SUBTREE_value(trees, i);
		BIO_printf(bp, "%*s", ind + 2, "");
		if (tree->base->type == GEN_IPADD)
			print_nc_ipadd(bp, tree->base->d.ip);
		else
			GENERAL_NAME_print(bp, tree->base);
		tree = sk_GENERAL_SUBTREE_value(trees, i);
		BIO_puts(bp, "\n");
		}
	return 1;
	}

static int print_nc_ipadd(BIO *bp, ASN1_OCTET_STRING *ip)
	{
	int i, len;
	unsigned char *p;
	p = ip->data;
	len = ip->length;
	BIO_puts(bp, "IP:");
	if(len == 8)
		{
		BIO_printf(bp, "%d.%d.%d.%d/%d.%d.%d.%d",
				p[0], p[1], p[2], p[3],
				p[4], p[5], p[6], p[7]);
		}
	else if(len == 32)
		{
		for (i = 0; i < 16; i++)
			{
			BIO_printf(bp, "%X", p[0] << 8 | p[1]);
			p += 2;
			if (i == 7)
				BIO_puts(bp, "/");
			else if (i != 15)
				BIO_puts(bp, ":");
			}
		}
	else
		BIO_printf(bp, "IP Address:<invalid>");
	return 1;
	}

