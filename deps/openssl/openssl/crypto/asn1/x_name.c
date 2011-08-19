/* crypto/asn1/x_name.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/asn1t.h>
#include <openssl/x509.h>

static int x509_name_ex_d2i(ASN1_VALUE **val, const unsigned char **in, long len, const ASN1_ITEM *it,
					int tag, int aclass, char opt, ASN1_TLC *ctx);

static int x509_name_ex_i2d(ASN1_VALUE **val, unsigned char **out, const ASN1_ITEM *it, int tag, int aclass);
static int x509_name_ex_new(ASN1_VALUE **val, const ASN1_ITEM *it);
static void x509_name_ex_free(ASN1_VALUE **val, const ASN1_ITEM *it);

static int x509_name_encode(X509_NAME *a);

ASN1_SEQUENCE(X509_NAME_ENTRY) = {
	ASN1_SIMPLE(X509_NAME_ENTRY, object, ASN1_OBJECT),
	ASN1_SIMPLE(X509_NAME_ENTRY, value, ASN1_PRINTABLE)
} ASN1_SEQUENCE_END(X509_NAME_ENTRY)

IMPLEMENT_ASN1_FUNCTIONS(X509_NAME_ENTRY)
IMPLEMENT_ASN1_DUP_FUNCTION(X509_NAME_ENTRY)

/* For the "Name" type we need a SEQUENCE OF { SET OF X509_NAME_ENTRY }
 * so declare two template wrappers for this
 */

ASN1_ITEM_TEMPLATE(X509_NAME_ENTRIES) =
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SET_OF, 0, RDNS, X509_NAME_ENTRY)
ASN1_ITEM_TEMPLATE_END(X509_NAME_ENTRIES)

ASN1_ITEM_TEMPLATE(X509_NAME_INTERNAL) =
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, Name, X509_NAME_ENTRIES)
ASN1_ITEM_TEMPLATE_END(X509_NAME_INTERNAL)

/* Normally that's where it would end: we'd have two nested STACK structures
 * representing the ASN1. Unfortunately X509_NAME uses a completely different
 * form and caches encodings so we have to process the internal form and convert
 * to the external form.
 */

const ASN1_EXTERN_FUNCS x509_name_ff = {
	NULL,
	x509_name_ex_new,
	x509_name_ex_free,
	0,	/* Default clear behaviour is OK */
	x509_name_ex_d2i,
	x509_name_ex_i2d
};

IMPLEMENT_EXTERN_ASN1(X509_NAME, V_ASN1_SEQUENCE, x509_name_ff) 

IMPLEMENT_ASN1_FUNCTIONS(X509_NAME)
IMPLEMENT_ASN1_DUP_FUNCTION(X509_NAME)

static int x509_name_ex_new(ASN1_VALUE **val, const ASN1_ITEM *it)
{
	X509_NAME *ret = NULL;
	ret = OPENSSL_malloc(sizeof(X509_NAME));
	if(!ret) goto memerr;
	if ((ret->entries=sk_X509_NAME_ENTRY_new_null()) == NULL)
		goto memerr;
	if((ret->bytes = BUF_MEM_new()) == NULL) goto memerr;
	ret->modified=1;
	*val = (ASN1_VALUE *)ret;
	return 1;

 memerr:
	ASN1err(ASN1_F_X509_NAME_EX_NEW, ERR_R_MALLOC_FAILURE);
	if (ret)
		{
		if (ret->entries)
			sk_X509_NAME_ENTRY_free(ret->entries);
		OPENSSL_free(ret);
		}
	return 0;
}

static void x509_name_ex_free(ASN1_VALUE **pval, const ASN1_ITEM *it)
{
	X509_NAME *a;
	if(!pval || !*pval)
	    return;
	a = (X509_NAME *)*pval;

	BUF_MEM_free(a->bytes);
	sk_X509_NAME_ENTRY_pop_free(a->entries,X509_NAME_ENTRY_free);
	OPENSSL_free(a);
	*pval = NULL;
}

/* Used with sk_pop_free() to free up the internal representation.
 * NB: we only free the STACK and not its contents because it is
 * already present in the X509_NAME structure.
 */

static void sk_internal_free(void *a)
{
	sk_free(a);
}

static int x509_name_ex_d2i(ASN1_VALUE **val, const unsigned char **in, long len, const ASN1_ITEM *it,
					int tag, int aclass, char opt, ASN1_TLC *ctx)
{
	const unsigned char *p = *in, *q;
	union { STACK *s; ASN1_VALUE *a; } intname = {NULL};
	union { X509_NAME *x; ASN1_VALUE *a; } nm = {NULL};
	int i, j, ret;
	STACK_OF(X509_NAME_ENTRY) *entries;
	X509_NAME_ENTRY *entry;
	q = p;

	/* Get internal representation of Name */
	ret = ASN1_item_ex_d2i(&intname.a,
			       &p, len, ASN1_ITEM_rptr(X509_NAME_INTERNAL),
			       tag, aclass, opt, ctx);
	
	if(ret <= 0) return ret;

	if(*val) x509_name_ex_free(val, NULL);
	if(!x509_name_ex_new(&nm.a, NULL)) goto err;
	/* We've decoded it: now cache encoding */
	if(!BUF_MEM_grow(nm.x->bytes, p - q)) goto err;
	memcpy(nm.x->bytes->data, q, p - q);

	/* Convert internal representation to X509_NAME structure */
	for(i = 0; i < sk_num(intname.s); i++) {
		entries = (STACK_OF(X509_NAME_ENTRY) *)sk_value(intname.s, i);
		for(j = 0; j < sk_X509_NAME_ENTRY_num(entries); j++) {
			entry = sk_X509_NAME_ENTRY_value(entries, j);
			entry->set = i;
			if(!sk_X509_NAME_ENTRY_push(nm.x->entries, entry))
				goto err;
		}
		sk_X509_NAME_ENTRY_free(entries);
	}
	sk_free(intname.s);
	nm.x->modified = 0;
	*val = nm.a;
	*in = p;
	return ret;
	err:
	ASN1err(ASN1_F_X509_NAME_EX_D2I, ERR_R_NESTED_ASN1_ERROR);
	return 0;
}

static int x509_name_ex_i2d(ASN1_VALUE **val, unsigned char **out, const ASN1_ITEM *it, int tag, int aclass)
{
	int ret;
	X509_NAME *a = (X509_NAME *)*val;
	if(a->modified) {
		ret = x509_name_encode((X509_NAME *)a);
		if(ret < 0) return ret;
	}
	ret = a->bytes->length;
	if(out != NULL) {
		memcpy(*out,a->bytes->data,ret);
		*out+=ret;
	}
	return ret;
}

static int x509_name_encode(X509_NAME *a)
{
	union { STACK *s; ASN1_VALUE *a; } intname = {NULL};
	int len;
	unsigned char *p;
	STACK_OF(X509_NAME_ENTRY) *entries = NULL;
	X509_NAME_ENTRY *entry;
	int i, set = -1;
	intname.s = sk_new_null();
	if(!intname.s) goto memerr;
	for(i = 0; i < sk_X509_NAME_ENTRY_num(a->entries); i++) {
		entry = sk_X509_NAME_ENTRY_value(a->entries, i);
		if(entry->set != set) {
			entries = sk_X509_NAME_ENTRY_new_null();
			if(!entries) goto memerr;
			if(!sk_push(intname.s, (char *)entries)) goto memerr;
			set = entry->set;
		}
		if(!sk_X509_NAME_ENTRY_push(entries, entry)) goto memerr;
	}
	len = ASN1_item_ex_i2d(&intname.a, NULL,
			       ASN1_ITEM_rptr(X509_NAME_INTERNAL), -1, -1);
	if (!BUF_MEM_grow(a->bytes,len)) goto memerr;
	p=(unsigned char *)a->bytes->data;
	ASN1_item_ex_i2d(&intname.a,
			 &p, ASN1_ITEM_rptr(X509_NAME_INTERNAL), -1, -1);
	sk_pop_free(intname.s, sk_internal_free);
	a->modified = 0;
	return len;
	memerr:
	sk_pop_free(intname.s, sk_internal_free);
	ASN1err(ASN1_F_X509_NAME_ENCODE, ERR_R_MALLOC_FAILURE);
	return -1;
}


int X509_NAME_set(X509_NAME **xn, X509_NAME *name)
	{
	X509_NAME *in;

	if (!xn || !name) return(0);

	if (*xn != name)
		{
		in=X509_NAME_dup(name);
		if (in != NULL)
			{
			X509_NAME_free(*xn);
			*xn=in;
			}
		}
	return(*xn != NULL);
	}
	
IMPLEMENT_STACK_OF(X509_NAME_ENTRY)
IMPLEMENT_ASN1_SET_OF(X509_NAME_ENTRY)
