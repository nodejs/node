/* crypto/asn1/a_set.c */
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
#include <openssl/asn1_mac.h>

#ifndef NO_ASN1_OLD

typedef struct
    {
    unsigned char *pbData;
    int cbData;
    } MYBLOB;

/* SetBlobCmp
 * This function compares two elements of SET_OF block
 */
static int SetBlobCmp(const void *elem1, const void *elem2 )
    {
    const MYBLOB *b1 = (const MYBLOB *)elem1;
    const MYBLOB *b2 = (const MYBLOB *)elem2;
    int r;

    r = memcmp(b1->pbData, b2->pbData,
	       b1->cbData < b2->cbData ? b1->cbData : b2->cbData);
    if(r != 0)
	return r;
    return b1->cbData-b2->cbData;
    }

/* int is_set:  if TRUE, then sort the contents (i.e. it isn't a SEQUENCE)    */
int i2d_ASN1_SET(STACK *a, unsigned char **pp, i2d_of_void *i2d, int ex_tag,
		 int ex_class, int is_set)
	{
	int ret=0,r;
	int i;
	unsigned char *p;
        unsigned char *pStart, *pTempMem;
        MYBLOB *rgSetBlob;
        int totSize;

	if (a == NULL) return(0);
	for (i=sk_num(a)-1; i>=0; i--)
		ret+=i2d(sk_value(a,i),NULL);
	r=ASN1_object_size(1,ret,ex_tag);
	if (pp == NULL) return(r);

	p= *pp;
	ASN1_put_object(&p,1,ret,ex_tag,ex_class);

/* Modified by gp@nsj.co.jp */
	/* And then again by Ben */
	/* And again by Steve */

	if(!is_set || (sk_num(a) < 2))
		{
		for (i=0; i<sk_num(a); i++)
                	i2d(sk_value(a,i),&p);

		*pp=p;
		return(r);
		}

        pStart  = p; /* Catch the beg of Setblobs*/
		/* In this array we will store the SET blobs */
		rgSetBlob = (MYBLOB *)OPENSSL_malloc(sk_num(a) * sizeof(MYBLOB));
		if (rgSetBlob == NULL)
			{
			ASN1err(ASN1_F_I2D_ASN1_SET,ERR_R_MALLOC_FAILURE);
			return(0);
			}

        for (i=0; i<sk_num(a); i++)
	        {
                rgSetBlob[i].pbData = p;  /* catch each set encode blob */
                i2d(sk_value(a,i),&p);
                rgSetBlob[i].cbData = p - rgSetBlob[i].pbData; /* Length of this
SetBlob
*/
		}
        *pp=p;
        totSize = p - pStart; /* This is the total size of all set blobs */

 /* Now we have to sort the blobs. I am using a simple algo.
    *Sort ptrs *Copy to temp-mem *Copy from temp-mem to user-mem*/
        qsort( rgSetBlob, sk_num(a), sizeof(MYBLOB), SetBlobCmp);
		if (!(pTempMem = OPENSSL_malloc(totSize)))
			{
			ASN1err(ASN1_F_I2D_ASN1_SET,ERR_R_MALLOC_FAILURE);
			return(0);
			}

/* Copy to temp mem */
        p = pTempMem;
        for(i=0; i<sk_num(a); ++i)
		{
                memcpy(p, rgSetBlob[i].pbData, rgSetBlob[i].cbData);
                p += rgSetBlob[i].cbData;
		}

/* Copy back to user mem*/
        memcpy(pStart, pTempMem, totSize);
        OPENSSL_free(pTempMem);
        OPENSSL_free(rgSetBlob);

        return(r);
        }

STACK *d2i_ASN1_SET(STACK **a, const unsigned char **pp, long length,
		    d2i_of_void *d2i, void (*free_func)(void *), int ex_tag,
		    int ex_class)
	{
	ASN1_const_CTX c;
	STACK *ret=NULL;

	if ((a == NULL) || ((*a) == NULL))
		{
		if ((ret=sk_new_null()) == NULL)
			{
			ASN1err(ASN1_F_D2I_ASN1_SET,ERR_R_MALLOC_FAILURE);
			goto err;
			}
		}
	else
		ret=(*a);

	c.p= *pp;
	c.max=(length == 0)?0:(c.p+length);

	c.inf=ASN1_get_object(&c.p,&c.slen,&c.tag,&c.xclass,c.max-c.p);
	if (c.inf & 0x80) goto err;
	if (ex_class != c.xclass)
		{
		ASN1err(ASN1_F_D2I_ASN1_SET,ASN1_R_BAD_CLASS);
		goto err;
		}
	if (ex_tag != c.tag)
		{
		ASN1err(ASN1_F_D2I_ASN1_SET,ASN1_R_BAD_TAG);
		goto err;
		}
	if ((c.slen+c.p) > c.max)
		{
		ASN1err(ASN1_F_D2I_ASN1_SET,ASN1_R_LENGTH_ERROR);
		goto err;
		}
	/* check for infinite constructed - it can be as long
	 * as the amount of data passed to us */
	if (c.inf == (V_ASN1_CONSTRUCTED+1))
		c.slen=length+ *pp-c.p;
	c.max=c.p+c.slen;

	while (c.p < c.max)
		{
		char *s;

		if (M_ASN1_D2I_end_sequence()) break;
		/* XXX: This was called with 4 arguments, incorrectly, it seems
		   if ((s=func(NULL,&c.p,c.slen,c.max-c.p)) == NULL) */
		if ((s=d2i(NULL,&c.p,c.slen)) == NULL)
			{
			ASN1err(ASN1_F_D2I_ASN1_SET,ASN1_R_ERROR_PARSING_SET_ELEMENT);
			asn1_add_error(*pp,(int)(c.q- *pp));
			goto err;
			}
		if (!sk_push(ret,s)) goto err;
		}
	if (a != NULL) (*a)=ret;
	*pp=c.p;
	return(ret);
err:
	if ((ret != NULL) && ((a == NULL) || (*a != ret)))
		{
		if (free_func != NULL)
			sk_pop_free(ret,free_func);
		else
			sk_free(ret);
		}
	return(NULL);
	}

#endif
