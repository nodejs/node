/* crypto/rc4/rc4test.c */
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
#include <stdlib.h>
#include <string.h>

#include "../e_os.h"

#ifdef OPENSSL_NO_RC4
int main(int argc, char *argv[])
{
    printf("No RC4 support\n");
    return(0);
}
#else
#include <openssl/rc4.h>
#include <openssl/sha.h>

static unsigned char keys[7][30]={
	{8,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef},
	{8,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef},
	{8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	{4,0xef,0x01,0x23,0x45},
	{8,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef},
	{4,0xef,0x01,0x23,0x45},
	};

static unsigned char data_len[7]={8,8,8,20,28,10};
static unsigned char data[7][30]={
	{0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xff},
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	   0x00,0x00,0x00,0x00,0xff},
	{0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
	   0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
	   0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
	   0x12,0x34,0x56,0x78,0xff},
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
	{0},
	};

static unsigned char output[7][30]={
	{0x75,0xb7,0x87,0x80,0x99,0xe0,0xc5,0x96,0x00},
	{0x74,0x94,0xc2,0xe7,0x10,0x4b,0x08,0x79,0x00},
	{0xde,0x18,0x89,0x41,0xa3,0x37,0x5d,0x3a,0x00},
	{0xd6,0xa1,0x41,0xa7,0xec,0x3c,0x38,0xdf,
	 0xbd,0x61,0x5a,0x11,0x62,0xe1,0xc7,0xba,
	 0x36,0xb6,0x78,0x58,0x00},
	{0x66,0xa0,0x94,0x9f,0x8a,0xf7,0xd6,0x89,
	 0x1f,0x7f,0x83,0x2b,0xa8,0x33,0xc0,0x0c,
	 0x89,0x2e,0xbe,0x30,0x14,0x3c,0xe2,0x87,
	 0x40,0x01,0x1e,0xcf,0x00},
	{0xd6,0xa1,0x41,0xa7,0xec,0x3c,0x38,0xdf,0xbd,0x61,0x00},
	{0},
	};

int main(int argc, char *argv[])
	{
	int i,err=0;
	int j;
	unsigned char *p;
	RC4_KEY key;
	unsigned char obuf[512];

	for (i=0; i<6; i++)
		{
		RC4_set_key(&key,keys[i][0],&(keys[i][1]));
		memset(obuf,0x00,sizeof(obuf));
		RC4(&key,data_len[i],&(data[i][0]),obuf);
		if (memcmp(obuf,output[i],data_len[i]+1) != 0)
			{
			printf("error calculating RC4\n");
			printf("output:");
			for (j=0; j<data_len[i]+1; j++)
				printf(" %02x",obuf[j]);
			printf("\n");
			printf("expect:");
			p= &(output[i][0]);
			for (j=0; j<data_len[i]+1; j++)
				printf(" %02x",*(p++));
			printf("\n");
			err++;
			}
		else
			printf("test %d ok\n",i);
		}
	printf("test end processing ");
	for (i=0; i<data_len[3]; i++)
		{
		RC4_set_key(&key,keys[3][0],&(keys[3][1]));
		memset(obuf,0x00,sizeof(obuf));
		RC4(&key,i,&(data[3][0]),obuf);
		if ((memcmp(obuf,output[3],i) != 0) || (obuf[i] != 0))
			{
			printf("error in RC4 length processing\n");
			printf("output:");
			for (j=0; j<i+1; j++)
				printf(" %02x",obuf[j]);
			printf("\n");
			printf("expect:");
			p= &(output[3][0]);
			for (j=0; j<i; j++)
				printf(" %02x",*(p++));
			printf(" 00\n");
			err++;
			}
		else
			{
			printf(".");
			fflush(stdout);
			}
		}
	printf("done\n");
	printf("test multi-call ");
	for (i=0; i<data_len[3]; i++)
		{
		RC4_set_key(&key,keys[3][0],&(keys[3][1]));
		memset(obuf,0x00,sizeof(obuf));
		RC4(&key,i,&(data[3][0]),obuf);
		RC4(&key,data_len[3]-i,&(data[3][i]),&(obuf[i]));
		if (memcmp(obuf,output[3],data_len[3]+1) != 0)
			{
			printf("error in RC4 multi-call processing\n");
			printf("output:");
			for (j=0; j<data_len[3]+1; j++)
				printf(" %02x",obuf[j]);
			printf("\n");
			printf("expect:");
			p= &(output[3][0]);
			for (j=0; j<data_len[3]+1; j++)
				printf(" %02x",*(p++));
			err++;
			}
		else
			{
			printf(".");
			fflush(stdout);
			}
		}
	printf("done\n");
	printf("bulk test ");
	{   unsigned char buf[513];
	    SHA_CTX c;
	    unsigned char md[SHA_DIGEST_LENGTH];
	    static unsigned char expected[]={
		0xa4,0x7b,0xcc,0x00,0x3d,0xd0,0xbd,0xe1,0xac,0x5f,
		0x12,0x1e,0x45,0xbc,0xfb,0x1a,0xa1,0xf2,0x7f,0xc5 };

		RC4_set_key(&key,keys[0][0],&(keys[3][1]));
		memset(buf,'\0',sizeof(buf));
		SHA1_Init(&c);
		for (i=0;i<2571;i++) {
			RC4(&key,sizeof(buf),buf,buf);
			SHA1_Update(&c,buf,sizeof(buf));
		}
		SHA1_Final(md,&c);

		if (memcmp(md,expected,sizeof(md))) {
			printf("error in RC4 bulk test\n");
			printf("output:");
			for (j=0; j<(int)sizeof(md); j++)
				printf(" %02x",md[j]);
			printf("\n");
			printf("expect:");
			for (j=0; j<(int)sizeof(md); j++)
				printf(" %02x",expected[j]);
			printf("\n");
			err++;
		}
		else	printf("ok\n");
	}
#ifdef OPENSSL_SYS_NETWARE
    if (err) printf("ERROR: %d\n", err);
#endif
	EXIT(err);
	return(0);
	}
#endif
