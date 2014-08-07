/* crypto/asn1/evp_asn1.c */
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
#include <openssl/asn1.h>
#include <openssl/asn1_mac.h>

int ASN1_TYPE_set_octetstring(ASN1_TYPE *a, unsigned char *data, int len)
	{
	ASN1_STRING *os;

	if ((os=M_ASN1_OCTET_STRING_new()) == NULL) return(0);
	if (!M_ASN1_OCTET_STRING_set(os,data,len))
		{
		M_ASN1_OCTET_STRING_free(os);
		return 0;
		}
	ASN1_TYPE_set(a,V_ASN1_OCTET_STRING,os);
	return(1);
	}

/* int max_len:  for returned value    */
int ASN1_TYPE_get_octetstring(ASN1_TYPE *a, unsigned char *data,
	     int max_len)
	{
	int ret,num;
	unsigned char *p;

	if ((a->type != V_ASN1_OCTET_STRING) || (a->value.octet_string == NULL))
		{
		ASN1err(ASN1_F_ASN1_TYPE_GET_OCTETSTRING,ASN1_R_DATA_IS_WRONG);
		return(-1);
		}
	p=M_ASN1_STRING_data(a->value.octet_string);
	ret=M_ASN1_STRING_length(a->value.octet_string);
	if (ret < max_len)
		num=ret;
	else
		num=max_len;
	memcpy(data,p,num);
	return(ret);
	}

int ASN1_TYPE_set_int_octetstring(ASN1_TYPE *a, long num, unsigned char *data,
	     int len)
	{
	int n,size;
	ASN1_OCTET_STRING os,*osp;
	ASN1_INTEGER in;
	unsigned char *p;
	unsigned char buf[32]; /* when they have 256bit longs, 
				* I'll be in trouble */
	in.data=buf;
	in.length=32;
	os.data=data;
	os.type=V_ASN1_OCTET_STRING;
	os.length=len;
	ASN1_INTEGER_set(&in,num);
	n =  i2d_ASN1_INTEGER(&in,NULL);
	n+=M_i2d_ASN1_OCTET_STRING(&os,NULL);

	size=ASN1_object_size(1,n,V_ASN1_SEQUENCE);

	if ((osp=ASN1_STRING_new()) == NULL) return(0);
	/* Grow the 'string' */
	if (!ASN1_STRING_set(osp,NULL,size))
		{
		ASN1_STRING_free(osp);
		return(0);
		}

	M_ASN1_STRING_length_set(osp, size);
	p=M_ASN1_STRING_data(osp);

	ASN1_put_object(&p,1,n,V_ASN1_SEQUENCE,V_ASN1_UNIVERSAL);
	  i2d_ASN1_INTEGER(&in,&p);
	M_i2d_ASN1_OCTET_STRING(&os,&p);

	ASN1_TYPE_set(a,V_ASN1_SEQUENCE,osp);
	return(1);
	}

/* we return the actual length..., num may be missing, in which
 * case, set it to zero */
/* int max_len:  for returned value    */
int ASN1_TYPE_get_int_octetstring(ASN1_TYPE *a, long *num, unsigned char *data,
	     int max_len)
	{
	int ret= -1,n;
	ASN1_INTEGER *ai=NULL;
	ASN1_OCTET_STRING *os=NULL;
	const unsigned char *p;
	long length;
	ASN1_const_CTX c;

	if ((a->type != V_ASN1_SEQUENCE) || (a->value.sequence == NULL))
		{
		goto err;
		}
	p=M_ASN1_STRING_data(a->value.sequence);
	length=M_ASN1_STRING_length(a->value.sequence);

	c.pp= &p;
	c.p=p;
	c.max=p+length;
	c.error=ASN1_R_DATA_IS_WRONG;

	M_ASN1_D2I_start_sequence();
	c.q=c.p;
	if ((ai=d2i_ASN1_INTEGER(NULL,&c.p,c.slen)) == NULL) goto err;
        c.slen-=(c.p-c.q);
	c.q=c.p;
	if ((os=d2i_ASN1_OCTET_STRING(NULL,&c.p,c.slen)) == NULL) goto err;
        c.slen-=(c.p-c.q);
	if (!M_ASN1_D2I_end_sequence()) goto err;

	if (num != NULL)
		*num=ASN1_INTEGER_get(ai);

	ret=M_ASN1_STRING_length(os);
	if (max_len > ret)
		n=ret;
	else
		n=max_len;

	if (data != NULL)
		memcpy(data,M_ASN1_STRING_data(os),n);
	if (0)
		{
err:
		ASN1err(ASN1_F_ASN1_TYPE_GET_INT_OCTETSTRING,ASN1_R_DATA_IS_WRONG);
		}
	if (os != NULL) M_ASN1_OCTET_STRING_free(os);
	if (ai != NULL) M_ASN1_INTEGER_free(ai);
	return(ret);
	}

