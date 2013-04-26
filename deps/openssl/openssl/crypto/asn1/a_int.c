/* crypto/asn1/a_int.c */
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
#include <openssl/bn.h>

ASN1_INTEGER *ASN1_INTEGER_dup(const ASN1_INTEGER *x)
{ return M_ASN1_INTEGER_dup(x);}

int ASN1_INTEGER_cmp(const ASN1_INTEGER *x, const ASN1_INTEGER *y)
	{ 
	int neg, ret;
	/* Compare signs */
	neg = x->type & V_ASN1_NEG;
	if (neg != (y->type & V_ASN1_NEG))
		{
		if (neg)
			return -1;
		else
			return 1;
		}

	ret = ASN1_STRING_cmp(x, y);

	if (neg)
		return -ret;
	else
		return ret;
	}
	

/* 
 * This converts an ASN1 INTEGER into its content encoding.
 * The internal representation is an ASN1_STRING whose data is a big endian
 * representation of the value, ignoring the sign. The sign is determined by
 * the type: V_ASN1_INTEGER for positive and V_ASN1_NEG_INTEGER for negative. 
 *
 * Positive integers are no problem: they are almost the same as the DER
 * encoding, except if the first byte is >= 0x80 we need to add a zero pad.
 *
 * Negative integers are a bit trickier...
 * The DER representation of negative integers is in 2s complement form.
 * The internal form is converted by complementing each octet and finally 
 * adding one to the result. This can be done less messily with a little trick.
 * If the internal form has trailing zeroes then they will become FF by the
 * complement and 0 by the add one (due to carry) so just copy as many trailing 
 * zeros to the destination as there are in the source. The carry will add one
 * to the last none zero octet: so complement this octet and add one and finally
 * complement any left over until you get to the start of the string.
 *
 * Padding is a little trickier too. If the first bytes is > 0x80 then we pad
 * with 0xff. However if the first byte is 0x80 and one of the following bytes
 * is non-zero we pad with 0xff. The reason for this distinction is that 0x80
 * followed by optional zeros isn't padded.
 */

int i2c_ASN1_INTEGER(ASN1_INTEGER *a, unsigned char **pp)
	{
	int pad=0,ret,i,neg;
	unsigned char *p,*n,pb=0;

	if ((a == NULL) || (a->data == NULL)) return(0);
	neg=a->type & V_ASN1_NEG;
	if (a->length == 0)
		ret=1;
	else
		{
		ret=a->length;
		i=a->data[0];
		if (!neg && (i > 127)) {
			pad=1;
			pb=0;
		} else if(neg) {
			if(i>128) {
				pad=1;
				pb=0xFF;
			} else if(i == 128) {
			/*
			 * Special case: if any other bytes non zero we pad:
			 * otherwise we don't.
			 */
				for(i = 1; i < a->length; i++) if(a->data[i]) {
						pad=1;
						pb=0xFF;
						break;
				}
			}
		}
		ret+=pad;
		}
	if (pp == NULL) return(ret);
	p= *pp;

	if (pad) *(p++)=pb;
	if (a->length == 0) *(p++)=0;
	else if (!neg) memcpy(p,a->data,(unsigned int)a->length);
	else {
		/* Begin at the end of the encoding */
		n=a->data + a->length - 1;
		p += a->length - 1;
		i = a->length;
		/* Copy zeros to destination as long as source is zero */
		while(!*n) {
			*(p--) = 0;
			n--;
			i--;
		}
		/* Complement and increment next octet */
		*(p--) = ((*(n--)) ^ 0xff) + 1;
		i--;
		/* Complement any octets left */
		for(;i > 0; i--) *(p--) = *(n--) ^ 0xff;
	}

	*pp+=ret;
	return(ret);
	}

/* Convert just ASN1 INTEGER content octets to ASN1_INTEGER structure */

ASN1_INTEGER *c2i_ASN1_INTEGER(ASN1_INTEGER **a, const unsigned char **pp,
	     long len)
	{
	ASN1_INTEGER *ret=NULL;
	const unsigned char *p, *pend;
	unsigned char *to,*s;
	int i;

	if ((a == NULL) || ((*a) == NULL))
		{
		if ((ret=M_ASN1_INTEGER_new()) == NULL) return(NULL);
		ret->type=V_ASN1_INTEGER;
		}
	else
		ret=(*a);

	p= *pp;
	pend = p + len;

	/* We must OPENSSL_malloc stuff, even for 0 bytes otherwise it
	 * signifies a missing NULL parameter. */
	s=(unsigned char *)OPENSSL_malloc((int)len+1);
	if (s == NULL)
		{
		i=ERR_R_MALLOC_FAILURE;
		goto err;
		}
	to=s;
	if(!len) {
		/* Strictly speaking this is an illegal INTEGER but we
		 * tolerate it.
		 */
		ret->type=V_ASN1_INTEGER;
	} else if (*p & 0x80) /* a negative number */
		{
		ret->type=V_ASN1_NEG_INTEGER;
		if ((*p == 0xff) && (len != 1)) {
			p++;
			len--;
		}
		i = len;
		p += i - 1;
		to += i - 1;
		while((!*p) && i) {
			*(to--) = 0;
			i--;
			p--;
		}
		/* Special case: if all zeros then the number will be of
		 * the form FF followed by n zero bytes: this corresponds to
		 * 1 followed by n zero bytes. We've already written n zeros
		 * so we just append an extra one and set the first byte to
		 * a 1. This is treated separately because it is the only case
		 * where the number of bytes is larger than len.
		 */
		if(!i) {
			*s = 1;
			s[len] = 0;
			len++;
		} else {
			*(to--) = (*(p--) ^ 0xff) + 1;
			i--;
			for(;i > 0; i--) *(to--) = *(p--) ^ 0xff;
		}
	} else {
		ret->type=V_ASN1_INTEGER;
		if ((*p == 0) && (len != 1))
			{
			p++;
			len--;
			}
		memcpy(s,p,(int)len);
	}

	if (ret->data != NULL) OPENSSL_free(ret->data);
	ret->data=s;
	ret->length=(int)len;
	if (a != NULL) (*a)=ret;
	*pp=pend;
	return(ret);
err:
	ASN1err(ASN1_F_C2I_ASN1_INTEGER,i);
	if ((ret != NULL) && ((a == NULL) || (*a != ret)))
		M_ASN1_INTEGER_free(ret);
	return(NULL);
	}


/* This is a version of d2i_ASN1_INTEGER that ignores the sign bit of
 * ASN1 integers: some broken software can encode a positive INTEGER
 * with its MSB set as negative (it doesn't add a padding zero).
 */

ASN1_INTEGER *d2i_ASN1_UINTEGER(ASN1_INTEGER **a, const unsigned char **pp,
	     long length)
	{
	ASN1_INTEGER *ret=NULL;
	const unsigned char *p;
	unsigned char *s;
	long len;
	int inf,tag,xclass;
	int i;

	if ((a == NULL) || ((*a) == NULL))
		{
		if ((ret=M_ASN1_INTEGER_new()) == NULL) return(NULL);
		ret->type=V_ASN1_INTEGER;
		}
	else
		ret=(*a);

	p= *pp;
	inf=ASN1_get_object(&p,&len,&tag,&xclass,length);
	if (inf & 0x80)
		{
		i=ASN1_R_BAD_OBJECT_HEADER;
		goto err;
		}

	if (tag != V_ASN1_INTEGER)
		{
		i=ASN1_R_EXPECTING_AN_INTEGER;
		goto err;
		}

	/* We must OPENSSL_malloc stuff, even for 0 bytes otherwise it
	 * signifies a missing NULL parameter. */
	s=(unsigned char *)OPENSSL_malloc((int)len+1);
	if (s == NULL)
		{
		i=ERR_R_MALLOC_FAILURE;
		goto err;
		}
	ret->type=V_ASN1_INTEGER;
	if(len) {
		if ((*p == 0) && (len != 1))
			{
			p++;
			len--;
			}
		memcpy(s,p,(int)len);
		p+=len;
	}

	if (ret->data != NULL) OPENSSL_free(ret->data);
	ret->data=s;
	ret->length=(int)len;
	if (a != NULL) (*a)=ret;
	*pp=p;
	return(ret);
err:
	ASN1err(ASN1_F_D2I_ASN1_UINTEGER,i);
	if ((ret != NULL) && ((a == NULL) || (*a != ret)))
		M_ASN1_INTEGER_free(ret);
	return(NULL);
	}

int ASN1_INTEGER_set(ASN1_INTEGER *a, long v)
	{
	int j,k;
	unsigned int i;
	unsigned char buf[sizeof(long)+1];
	long d;

	a->type=V_ASN1_INTEGER;
	if (a->length < (int)(sizeof(long)+1))
		{
		if (a->data != NULL)
			OPENSSL_free(a->data);
		if ((a->data=(unsigned char *)OPENSSL_malloc(sizeof(long)+1)) != NULL)
			memset((char *)a->data,0,sizeof(long)+1);
		}
	if (a->data == NULL)
		{
		ASN1err(ASN1_F_ASN1_INTEGER_SET,ERR_R_MALLOC_FAILURE);
		return(0);
		}
	d=v;
	if (d < 0)
		{
		d= -d;
		a->type=V_ASN1_NEG_INTEGER;
		}

	for (i=0; i<sizeof(long); i++)
		{
		if (d == 0) break;
		buf[i]=(int)d&0xff;
		d>>=8;
		}
	j=0;
	for (k=i-1; k >=0; k--)
		a->data[j++]=buf[k];
	a->length=j;
	return(1);
	}

long ASN1_INTEGER_get(const ASN1_INTEGER *a)
	{
	int neg=0,i;
	long r=0;

	if (a == NULL) return(0L);
	i=a->type;
	if (i == V_ASN1_NEG_INTEGER)
		neg=1;
	else if (i != V_ASN1_INTEGER)
		return -1;
	
	if (a->length > (int)sizeof(long))
		{
		/* hmm... a bit ugly */
		return(0xffffffffL);
		}
	if (a->data == NULL)
		return 0;

	for (i=0; i<a->length; i++)
		{
		r<<=8;
		r|=(unsigned char)a->data[i];
		}
	if (neg) r= -r;
	return(r);
	}

ASN1_INTEGER *BN_to_ASN1_INTEGER(const BIGNUM *bn, ASN1_INTEGER *ai)
	{
	ASN1_INTEGER *ret;
	int len,j;

	if (ai == NULL)
		ret=M_ASN1_INTEGER_new();
	else
		ret=ai;
	if (ret == NULL)
		{
		ASN1err(ASN1_F_BN_TO_ASN1_INTEGER,ERR_R_NESTED_ASN1_ERROR);
		goto err;
		}
	if (BN_is_negative(bn))
		ret->type = V_ASN1_NEG_INTEGER;
	else ret->type=V_ASN1_INTEGER;
	j=BN_num_bits(bn);
	len=((j == 0)?0:((j/8)+1));
	if (ret->length < len+4)
		{
		unsigned char *new_data=OPENSSL_realloc(ret->data, len+4);
		if (!new_data)
			{
			ASN1err(ASN1_F_BN_TO_ASN1_INTEGER,ERR_R_MALLOC_FAILURE);
			goto err;
			}
		ret->data=new_data;
		}
	ret->length=BN_bn2bin(bn,ret->data);
	/* Correct zero case */
	if(!ret->length)
		{
		ret->data[0] = 0;
		ret->length = 1;
		}
	return(ret);
err:
	if (ret != ai) M_ASN1_INTEGER_free(ret);
	return(NULL);
	}

BIGNUM *ASN1_INTEGER_to_BN(const ASN1_INTEGER *ai, BIGNUM *bn)
	{
	BIGNUM *ret;

	if ((ret=BN_bin2bn(ai->data,ai->length,bn)) == NULL)
		ASN1err(ASN1_F_ASN1_INTEGER_TO_BN,ASN1_R_BN_LIB);
	else if(ai->type == V_ASN1_NEG_INTEGER)
		BN_set_negative(ret, 1);
	return(ret);
	}

IMPLEMENT_STACK_OF(ASN1_INTEGER)
IMPLEMENT_ASN1_SET_OF(ASN1_INTEGER)
