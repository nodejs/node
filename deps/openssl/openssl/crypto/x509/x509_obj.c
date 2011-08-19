/* crypto/x509/x509_obj.c */
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
#include <openssl/lhash.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/buffer.h>

char *X509_NAME_oneline(X509_NAME *a, char *buf, int len)
	{
	X509_NAME_ENTRY *ne;
int i;
	int n,lold,l,l1,l2,num,j,type;
	const char *s;
	char *p;
	unsigned char *q;
	BUF_MEM *b=NULL;
	static char hex[17]="0123456789ABCDEF";
	int gs_doit[4];
	char tmp_buf[80];
#ifdef CHARSET_EBCDIC
	char ebcdic_buf[1024];
#endif

	if (buf == NULL)
		{
		if ((b=BUF_MEM_new()) == NULL) goto err;
		if (!BUF_MEM_grow(b,200)) goto err;
		b->data[0]='\0';
		len=200;
		}
	if (a == NULL)
	    {
	    if(b)
		{
		buf=b->data;
		OPENSSL_free(b);
		}
	    strncpy(buf,"NO X509_NAME",len);
	    buf[len-1]='\0';
	    return buf;
	    }

	len--; /* space for '\0' */
	l=0;
	for (i=0; i<sk_X509_NAME_ENTRY_num(a->entries); i++)
		{
		ne=sk_X509_NAME_ENTRY_value(a->entries,i);
		n=OBJ_obj2nid(ne->object);
		if ((n == NID_undef) || ((s=OBJ_nid2sn(n)) == NULL))
			{
			i2t_ASN1_OBJECT(tmp_buf,sizeof(tmp_buf),ne->object);
			s=tmp_buf;
			}
		l1=strlen(s);

		type=ne->value->type;
		num=ne->value->length;
		q=ne->value->data;
#ifdef CHARSET_EBCDIC
                if (type == V_ASN1_GENERALSTRING ||
		    type == V_ASN1_VISIBLESTRING ||
		    type == V_ASN1_PRINTABLESTRING ||
		    type == V_ASN1_TELETEXSTRING ||
		    type == V_ASN1_VISIBLESTRING ||
		    type == V_ASN1_IA5STRING) {
                        ascii2ebcdic(ebcdic_buf, q,
				     (num > sizeof ebcdic_buf)
				     ? sizeof ebcdic_buf : num);
                        q=ebcdic_buf;
		}
#endif

		if ((type == V_ASN1_GENERALSTRING) && ((num%4) == 0))
			{
			gs_doit[0]=gs_doit[1]=gs_doit[2]=gs_doit[3]=0;
			for (j=0; j<num; j++)
				if (q[j] != 0) gs_doit[j&3]=1;

			if (gs_doit[0]|gs_doit[1]|gs_doit[2])
				gs_doit[0]=gs_doit[1]=gs_doit[2]=gs_doit[3]=1;
			else
				{
				gs_doit[0]=gs_doit[1]=gs_doit[2]=0;
				gs_doit[3]=1;
				}
			}
		else
			gs_doit[0]=gs_doit[1]=gs_doit[2]=gs_doit[3]=1;

		for (l2=j=0; j<num; j++)
			{
			if (!gs_doit[j&3]) continue;
			l2++;
#ifndef CHARSET_EBCDIC
			if ((q[j] < ' ') || (q[j] > '~')) l2+=3;
#else
			if ((os_toascii[q[j]] < os_toascii[' ']) ||
			    (os_toascii[q[j]] > os_toascii['~'])) l2+=3;
#endif
			}

		lold=l;
		l+=1+l1+1+l2;
		if (b != NULL)
			{
			if (!BUF_MEM_grow(b,l+1)) goto err;
			p= &(b->data[lold]);
			}
		else if (l > len)
			{
			break;
			}
		else
			p= &(buf[lold]);
		*(p++)='/';
		memcpy(p,s,(unsigned int)l1); p+=l1;
		*(p++)='=';

#ifndef CHARSET_EBCDIC /* q was assigned above already. */
		q=ne->value->data;
#endif

		for (j=0; j<num; j++)
			{
			if (!gs_doit[j&3]) continue;
#ifndef CHARSET_EBCDIC
			n=q[j];
			if ((n < ' ') || (n > '~'))
				{
				*(p++)='\\';
				*(p++)='x';
				*(p++)=hex[(n>>4)&0x0f];
				*(p++)=hex[n&0x0f];
				}
			else
				*(p++)=n;
#else
			n=os_toascii[q[j]];
			if ((n < os_toascii[' ']) ||
			    (n > os_toascii['~']))
				{
				*(p++)='\\';
				*(p++)='x';
				*(p++)=hex[(n>>4)&0x0f];
				*(p++)=hex[n&0x0f];
				}
			else
				*(p++)=q[j];
#endif
			}
		*p='\0';
		}
	if (b != NULL)
		{
		p=b->data;
		OPENSSL_free(b);
		}
	else
		p=buf;
	if (i == 0)
		*p = '\0';
	return(p);
err:
	X509err(X509_F_X509_NAME_ONELINE,ERR_R_MALLOC_FAILURE);
	if (b != NULL) BUF_MEM_free(b);
	return(NULL);
	}

