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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
 *
 */
/*---------------------------------------------
  NIST AES Algorithm Validation Suite
  Test Program

  Donated to OpenSSL by:
  V-ONE Corporation
  20250 Century Blvd, Suite 300
  Germantown, MD 20874
  U.S.A.
  ----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/bn.h>

#include <openssl/err.h>
#include "e_os.h"

#ifndef OPENSSL_FIPS

int main(int argc, char *argv[])
{
    printf("No FIPS AES support\n");
    return(0);
}

#else

#include <openssl/fips.h>
#include "fips_utl.h"

#define AES_BLOCK_SIZE 16

#define VERBOSE 0

/*-----------------------------------------------*/

static int AESTest(EVP_CIPHER_CTX *ctx,
	    char *amode, int akeysz, unsigned char *aKey, 
	    unsigned char *iVec, 
	    int dir,  /* 0 = decrypt, 1 = encrypt */
	    unsigned char *plaintext, unsigned char *ciphertext, int len)
    {
    const EVP_CIPHER *cipher = NULL;

    if (strcasecmp(amode, "CBC") == 0)
	{
	switch (akeysz)
		{
		case 128:
		cipher = EVP_aes_128_cbc();
		break;

		case 192:
		cipher = EVP_aes_192_cbc();
		break;

		case 256:
		cipher = EVP_aes_256_cbc();
		break;
		}

	}
    else if (strcasecmp(amode, "ECB") == 0)
	{
	switch (akeysz)
		{
		case 128:
		cipher = EVP_aes_128_ecb();
		break;

		case 192:
		cipher = EVP_aes_192_ecb();
		break;

		case 256:
		cipher = EVP_aes_256_ecb();
		break;
		}
	}
    else if (strcasecmp(amode, "CFB128") == 0)
	{
	switch (akeysz)
		{
		case 128:
		cipher = EVP_aes_128_cfb128();
		break;

		case 192:
		cipher = EVP_aes_192_cfb128();
		break;

		case 256:
		cipher = EVP_aes_256_cfb128();
		break;
		}

	}
    else if (strncasecmp(amode, "OFB", 3) == 0)
	{
	switch (akeysz)
		{
		case 128:
		cipher = EVP_aes_128_ofb();
		break;

		case 192:
		cipher = EVP_aes_192_ofb();
		break;

		case 256:
		cipher = EVP_aes_256_ofb();
		break;
		}
	}
    else if(!strcasecmp(amode,"CFB1"))
	{
	switch (akeysz)
		{
		case 128:
		cipher = EVP_aes_128_cfb1();
		break;

		case 192:
		cipher = EVP_aes_192_cfb1();
		break;

		case 256:
		cipher = EVP_aes_256_cfb1();
		break;
		}
	}
    else if(!strcasecmp(amode,"CFB8"))
	{
	switch (akeysz)
		{
		case 128:
		cipher = EVP_aes_128_cfb8();
		break;

		case 192:
		cipher = EVP_aes_192_cfb8();
		break;

		case 256:
		cipher = EVP_aes_256_cfb8();
		break;
		}
	}
    else
	{
	printf("Unknown mode: %s\n", amode);
	return 0;
	}
    if (!cipher)
	{
	printf("Invalid key size: %d\n", akeysz);
	return 0; 
	}
    if (EVP_CipherInit_ex(ctx, cipher, NULL, aKey, iVec, dir) <= 0)
	return 0;
    if(!strcasecmp(amode,"CFB1"))
	M_EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS);
    if (dir)
		EVP_Cipher(ctx, ciphertext, plaintext, len);
	else
		EVP_Cipher(ctx, plaintext, ciphertext, len);
    return 1;
    }

/*-----------------------------------------------*/
char *t_tag[2] = {"PLAINTEXT", "CIPHERTEXT"};
char *t_mode[6] = {"CBC","ECB","OFB","CFB1","CFB8","CFB128"};
enum Mode {CBC, ECB, OFB, CFB1, CFB8, CFB128};
enum XCrypt {XDECRYPT, XENCRYPT};

/*=============================*/
/*  Monte Carlo Tests          */
/*-----------------------------*/

/*#define gb(a,b) (((a)[(b)/8] >> ((b)%8))&1)*/
/*#define sb(a,b,v) ((a)[(b)/8]=((a)[(b)/8]&~(1 << ((b)%8)))|(!!(v) << ((b)%8)))*/

#define gb(a,b) (((a)[(b)/8] >> (7-(b)%8))&1)
#define sb(a,b,v) ((a)[(b)/8]=((a)[(b)/8]&~(1 << (7-(b)%8)))|(!!(v) << (7-(b)%8)))

static int do_mct(char *amode, 
	   int akeysz, unsigned char *aKey,unsigned char *iVec,
	   int dir, unsigned char *text, int len,
	   FILE *rfp)
    {
    int ret = 0;
    unsigned char key[101][32];
    unsigned char iv[101][AES_BLOCK_SIZE];
    unsigned char ptext[1001][32];
    unsigned char ctext[1001][32];
    unsigned char ciphertext[64+4];
    int i, j, n, n1, n2;
    int imode = 0, nkeysz = akeysz/8;
    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX_init(&ctx);

    if (len > 32)
	{
	printf("\n>>>> Length exceeds 32 for %s %d <<<<\n\n", 
	       amode, akeysz);
	return -1;
	}
    for (imode = 0; imode < 6; ++imode)
	if (strcmp(amode, t_mode[imode]) == 0)
	    break;
    if (imode == 6)
	{ 
	printf("Unrecognized mode: %s\n", amode);
	return -1;
	}

    memcpy(key[0], aKey, nkeysz);
    if (iVec)
	memcpy(iv[0], iVec, AES_BLOCK_SIZE);
    if (dir == XENCRYPT)
	memcpy(ptext[0], text, len);
    else
	memcpy(ctext[0], text, len);
    for (i = 0; i < 100; ++i)
	{
	/* printf("Iteration %d\n", i); */
	if (i > 0)
	    {
	    fprintf(rfp,"COUNT = %d\n",i);
	    OutputValue("KEY",key[i],nkeysz,rfp,0);
	    if (imode != ECB)  /* ECB */
		OutputValue("IV",iv[i],AES_BLOCK_SIZE,rfp,0);
	    /* Output Ciphertext | Plaintext */
	    OutputValue(t_tag[dir^1],dir ? ptext[0] : ctext[0],len,rfp,
			imode == CFB1);
	    }
	for (j = 0; j < 1000; ++j)
	    {
	    switch (imode)
		{
	    case ECB:
		if (j == 0)
		    { /* set up encryption */
		    ret = AESTest(&ctx, amode, akeysz, key[i], NULL, 
				  dir,  /* 0 = decrypt, 1 = encrypt */
				  ptext[j], ctext[j], len);
		    if (dir == XENCRYPT)
			memcpy(ptext[j+1], ctext[j], len);
		    else
			memcpy(ctext[j+1], ptext[j], len);
		    }
		else
		    {
		    if (dir == XENCRYPT)
			{
			EVP_Cipher(&ctx, ctext[j], ptext[j], len);
			memcpy(ptext[j+1], ctext[j], len);
			}
		    else
			{
			EVP_Cipher(&ctx, ptext[j], ctext[j], len);
			memcpy(ctext[j+1], ptext[j], len);
			}
		    }
		break;

	    case CBC:
	    case OFB:  
	    case CFB128:
		if (j == 0)
		    {
		    ret = AESTest(&ctx, amode, akeysz, key[i], iv[i], 
				  dir,  /* 0 = decrypt, 1 = encrypt */
				  ptext[j], ctext[j], len);
		    if (dir == XENCRYPT)
			memcpy(ptext[j+1], iv[i], len);
		    else
			memcpy(ctext[j+1], iv[i], len);
		    }
		else
		    {
		    if (dir == XENCRYPT)
			{
			EVP_Cipher(&ctx, ctext[j], ptext[j], len);
			memcpy(ptext[j+1], ctext[j-1], len);
			}
		    else
			{
			EVP_Cipher(&ctx, ptext[j], ctext[j], len);
			memcpy(ctext[j+1], ptext[j-1], len);
			}
		    }
		break;

	    case CFB8:
		if (j == 0)
		    {
		    ret = AESTest(&ctx, amode, akeysz, key[i], iv[i], 
				  dir,  /* 0 = decrypt, 1 = encrypt */
				  ptext[j], ctext[j], len);
		    }
		else
		    {
		    if (dir == XENCRYPT)
			EVP_Cipher(&ctx, ctext[j], ptext[j], len);
		    else
			EVP_Cipher(&ctx, ptext[j], ctext[j], len);
		    }
		if (dir == XENCRYPT)
		    {
		    if (j < 16)
			memcpy(ptext[j+1], &iv[i][j], len);
		    else
			memcpy(ptext[j+1], ctext[j-16], len);
		    }
		else
		    {
		    if (j < 16)
			memcpy(ctext[j+1], &iv[i][j], len);
		    else
			memcpy(ctext[j+1], ptext[j-16], len);
		    }
		break;

	    case CFB1:
		if(j == 0)
		    {
#if 0
		    /* compensate for wrong endianness of input file */
		    if(i == 0)
			ptext[0][0]<<=7;
#endif
		    ret = AESTest(&ctx,amode,akeysz,key[i],iv[i],dir,
				ptext[j], ctext[j], len);
		    }
		else
		    {
		    if (dir == XENCRYPT)
			EVP_Cipher(&ctx, ctext[j], ptext[j], len);
		    else
			EVP_Cipher(&ctx, ptext[j], ctext[j], len);

		    }
		if(dir == XENCRYPT)
		    {
		    if(j < 128)
			sb(ptext[j+1],0,gb(iv[i],j));
		    else
			sb(ptext[j+1],0,gb(ctext[j-128],0));
		    }
		else
		    {
		    if(j < 128)
			sb(ctext[j+1],0,gb(iv[i],j));
		    else
			sb(ctext[j+1],0,gb(ptext[j-128],0));
		    }
		break;
		}
	    }
	--j; /* reset to last of range */
	/* Output Ciphertext | Plaintext */
	OutputValue(t_tag[dir],dir ? ctext[j] : ptext[j],len,rfp,
		    imode == CFB1);
	fprintf(rfp, "\n");  /* add separator */

	/* Compute next KEY */
	if (dir == XENCRYPT)
	    {
	    if (imode == CFB8)
		{ /* ct = CT[j-15] || CT[j-14] || ... || CT[j] */
		for (n1 = 0, n2 = nkeysz-1; n1 < nkeysz; ++n1, --n2)
		    ciphertext[n1] = ctext[j-n2][0];
		}
	    else if(imode == CFB1)
		{
		for(n1=0,n2=akeysz-1 ; n1 < akeysz ; ++n1,--n2)
		    sb(ciphertext,n1,gb(ctext[j-n2],0));
		}
	    else
		switch (akeysz)
		    {
		case 128:
		    memcpy(ciphertext, ctext[j], 16);
		    break;
		case 192:
		    memcpy(ciphertext, ctext[j-1]+8, 8);
		    memcpy(ciphertext+8, ctext[j], 16);
		    break;
		case 256:
		    memcpy(ciphertext, ctext[j-1], 16);
		    memcpy(ciphertext+16, ctext[j], 16);
		    break;
		    }
	    }
	else
	    {
	    if (imode == CFB8)
		{ /* ct = CT[j-15] || CT[j-14] || ... || CT[j] */
		for (n1 = 0, n2 = nkeysz-1; n1 < nkeysz; ++n1, --n2)
		    ciphertext[n1] = ptext[j-n2][0];
		}
	    else if(imode == CFB1)
		{
		for(n1=0,n2=akeysz-1 ; n1 < akeysz ; ++n1,--n2)
		    sb(ciphertext,n1,gb(ptext[j-n2],0));
		}
	    else
		switch (akeysz)
		    {
		case 128:
		    memcpy(ciphertext, ptext[j], 16);
		    break;
		case 192:
		    memcpy(ciphertext, ptext[j-1]+8, 8);
		    memcpy(ciphertext+8, ptext[j], 16);
		    break;
		case 256:
		    memcpy(ciphertext, ptext[j-1], 16);
		    memcpy(ciphertext+16, ptext[j], 16);
		    break;
		    }
	    }
	/* Compute next key: Key[i+1] = Key[i] xor ct */
	for (n = 0; n < nkeysz; ++n)
	    key[i+1][n] = key[i][n] ^ ciphertext[n];
	
	/* Compute next IV and text */
	if (dir == XENCRYPT)
	    {
	    switch (imode)
		{
	    case ECB:
		memcpy(ptext[0], ctext[j], AES_BLOCK_SIZE);
		break;
	    case CBC:
	    case OFB:
	    case CFB128:
		memcpy(iv[i+1], ctext[j], AES_BLOCK_SIZE);
		memcpy(ptext[0], ctext[j-1], AES_BLOCK_SIZE);
		break;
	    case CFB8:
		/* IV[i+1] = ct */
		for (n1 = 0, n2 = 15; n1 < 16; ++n1, --n2)
		    iv[i+1][n1] = ctext[j-n2][0];
		ptext[0][0] = ctext[j-16][0];
		break;
	    case CFB1:
		for(n1=0,n2=127 ; n1 < 128 ; ++n1,--n2)
		    sb(iv[i+1],n1,gb(ctext[j-n2],0));
		ptext[0][0]=ctext[j-128][0]&0x80;
		break;
		}
	    }
	else
	    {
	    switch (imode)
		{
	    case ECB:
		memcpy(ctext[0], ptext[j], AES_BLOCK_SIZE);
		break;
	    case CBC:
	    case OFB:
	    case CFB128:
		memcpy(iv[i+1], ptext[j], AES_BLOCK_SIZE);
		memcpy(ctext[0], ptext[j-1], AES_BLOCK_SIZE);
		break;
	    case CFB8:
		for (n1 = 0, n2 = 15; n1 < 16; ++n1, --n2)
		    iv[i+1][n1] = ptext[j-n2][0];
		ctext[0][0] = ptext[j-16][0];
		break;
	    case CFB1:
		for(n1=0,n2=127 ; n1 < 128 ; ++n1,--n2)
		    sb(iv[i+1],n1,gb(ptext[j-n2],0));
		ctext[0][0]=ptext[j-128][0]&0x80;
		break;
		}
	    }
	}
    
    return ret;
    }

/*================================================*/
/*----------------------------
  # Config info for v-one
  # AESVS MMT test data for ECB
  # State : Encrypt and Decrypt
  # Key Length : 256
  # Fri Aug 30 04:07:22 PM
  ----------------------------*/

static int proc_file(char *rqfile, char *rspfile)
    {
    char afn[256], rfn[256];
    FILE *afp = NULL, *rfp = NULL;
    char ibuf[2048];
    char tbuf[2048];
    int ilen, len, ret = 0;
    char algo[8] = "";
    char amode[8] = "";
    char atest[8] = "";
    int akeysz = 0;
    unsigned char iVec[20], aKey[40];
    int dir = -1, err = 0, step = 0;
    unsigned char plaintext[2048];
    unsigned char ciphertext[2048];
    char *rp;
    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX_init(&ctx);

    if (!rqfile || !(*rqfile))
	{
	printf("No req file\n");
	return -1;
	}
    strcpy(afn, rqfile);

    if ((afp = fopen(afn, "r")) == NULL)
	{
	printf("Cannot open file: %s, %s\n", 
	       afn, strerror(errno));
	return -1;
	}
    if (!rspfile)
	{
	strcpy(rfn,afn);
	rp=strstr(rfn,"req/");
#ifdef OPENSSL_SYS_WIN32
	if (!rp)
	    rp=strstr(rfn,"req\\");
#endif
	assert(rp);
	memcpy(rp,"rsp",3);
	rp = strstr(rfn, ".req");
	memcpy(rp, ".rsp", 4);
	rspfile = rfn;
	}
    if ((rfp = fopen(rspfile, "w")) == NULL)
	{
	printf("Cannot open file: %s, %s\n", 
	       rfn, strerror(errno));
	fclose(afp);
	afp = NULL;
	return -1;
	}
    while (!err && (fgets(ibuf, sizeof(ibuf), afp)) != NULL)
	{
	tidy_line(tbuf, ibuf);
	ilen = strlen(ibuf);
	/*      printf("step=%d ibuf=%s",step,ibuf); */
	switch (step)
	    {
	case 0:  /* read preamble */
	    if (ibuf[0] == '\n')
		{ /* end of preamble */
		if ((*algo == '\0') ||
		    (*amode == '\0') ||
		    (akeysz == 0))
		    {
		    printf("Missing Algorithm, Mode or KeySize (%s/%s/%d)\n",
			   algo,amode,akeysz);
		    err = 1;
		    }
		else
		    {
		    fputs(ibuf, rfp);
		    ++ step;
		    }
		}
	    else if (ibuf[0] != '#')
		{
		printf("Invalid preamble item: %s\n", ibuf);
		err = 1;
		}
	    else
		{ /* process preamble */
		char *xp, *pp = ibuf+2;
		int n;
		if (akeysz)
		    { /* insert current time & date */
		    time_t rtim = time(0);
		    fprintf(rfp, "# %s", ctime(&rtim));
		    }
		else
		    {
		    fputs(ibuf, rfp);
		    if (strncmp(pp, "AESVS ", 6) == 0)
			{
			strcpy(algo, "AES");
			/* get test type */
			pp += 6;
			xp = strchr(pp, ' ');
			n = xp-pp;
			strncpy(atest, pp, n);
			atest[n] = '\0';
			/* get mode */
			xp = strrchr(pp, ' '); /* get mode" */
			n = strlen(xp+1)-1;
			strncpy(amode, xp+1, n);
			amode[n] = '\0';
			/* amode[3] = '\0'; */
			if (VERBOSE)
				printf("Test = %s, Mode = %s\n", atest, amode);
			}
		    else if (strncasecmp(pp, "Key Length : ", 13) == 0)
			{
			akeysz = atoi(pp+13);
			if (VERBOSE)
				printf("Key size = %d\n", akeysz);
			}
		    }
		}
	    break;

	case 1:  /* [ENCRYPT] | [DECRYPT] */
	    if (ibuf[0] == '[')
		{
		fputs(ibuf, rfp);
		++step;
		if (strncasecmp(ibuf, "[ENCRYPT]", 9) == 0)
		    dir = 1;
		else if (strncasecmp(ibuf, "[DECRYPT]", 9) == 0)
		    dir = 0;
		else
		    {
		    printf("Invalid keyword: %s\n", ibuf);
		    err = 1;
		    }
		break;
		}
	    else if (dir == -1)
		{
		err = 1;
		printf("Missing ENCRYPT/DECRYPT keyword\n");
		break;
		}
	    else 
		step = 2;

	case 2: /* KEY = xxxx */
	    fputs(ibuf, rfp);
	    if(*ibuf == '\n')
		break;
	    if(!strncasecmp(ibuf,"COUNT = ",8))
		break;

	    if (strncasecmp(ibuf, "KEY = ", 6) != 0)
		{
		printf("Missing KEY\n");
		err = 1;
		}
	    else
		{
		len = hex2bin((char*)ibuf+6, aKey);
		if (len < 0)
		    {
		    printf("Invalid KEY\n");
		    err =1;
		    break;
		    }
		PrintValue("KEY", aKey, len);
		if (strcmp(amode, "ECB") == 0)
		    {
		    memset(iVec, 0, sizeof(iVec));
		    step = (dir)? 4: 5;  /* no ivec for ECB */
		    }
		else
		    ++step;
		}
	    break;

	case 3: /* IV = xxxx */
	    fputs(ibuf, rfp);
	    if (strncasecmp(ibuf, "IV = ", 5) != 0)
		{
		printf("Missing IV\n");
		err = 1;
		}
	    else
		{
		len = hex2bin((char*)ibuf+5, iVec);
		if (len < 0)
		    {
		    printf("Invalid IV\n");
		    err =1;
		    break;
		    }
		PrintValue("IV", iVec, len);
		step = (dir)? 4: 5;
		}
	    break;

	case 4: /* PLAINTEXT = xxxx */
	    fputs(ibuf, rfp);
	    if (strncasecmp(ibuf, "PLAINTEXT = ", 12) != 0)
		{
		printf("Missing PLAINTEXT\n");
		err = 1;
		}
	    else
		{
		int nn = strlen(ibuf+12);
		if(!strcmp(amode,"CFB1"))
		    len=bint2bin(ibuf+12,nn-1,plaintext);
		else
		    len=hex2bin(ibuf+12, plaintext);
		if (len < 0)
		    {
		    printf("Invalid PLAINTEXT: %s", ibuf+12);
		    err =1;
		    break;
		    }
		if (len >= (int)sizeof(plaintext))
		    {
		    printf("Buffer overflow\n");
		    }
		PrintValue("PLAINTEXT", (unsigned char*)plaintext, len);
		if (strcmp(atest, "MCT") == 0)  /* Monte Carlo Test */
		    {
		    if(do_mct(amode, akeysz, aKey, iVec, 
			      dir, (unsigned char*)plaintext, len, 
			      rfp) < 0)
			EXIT(1);
		    }
		else
		    {
		    ret = AESTest(&ctx, amode, akeysz, aKey, iVec, 
				  dir,  /* 0 = decrypt, 1 = encrypt */
				  plaintext, ciphertext, len);
		    OutputValue("CIPHERTEXT",ciphertext,len,rfp,
				!strcmp(amode,"CFB1"));
		    }
		step = 6;
		}
	    break;

	case 5: /* CIPHERTEXT = xxxx */
	    fputs(ibuf, rfp);
	    if (strncasecmp(ibuf, "CIPHERTEXT = ", 13) != 0)
		{
		printf("Missing KEY\n");
		err = 1;
		}
	    else
		{
		if(!strcmp(amode,"CFB1"))
		    len=bint2bin(ibuf+13,strlen(ibuf+13)-1,ciphertext);
		else
		    len = hex2bin(ibuf+13,ciphertext);
		if (len < 0)
		    {
		    printf("Invalid CIPHERTEXT\n");
		    err =1;
		    break;
		    }

		PrintValue("CIPHERTEXT", ciphertext, len);
		if (strcmp(atest, "MCT") == 0)  /* Monte Carlo Test */
		    {
		    do_mct(amode, akeysz, aKey, iVec, 
			   dir, ciphertext, len, rfp);
		    }
		else
		    {
		    ret = AESTest(&ctx, amode, akeysz, aKey, iVec, 
				  dir,  /* 0 = decrypt, 1 = encrypt */
				  plaintext, ciphertext, len);
		    OutputValue("PLAINTEXT",(unsigned char *)plaintext,len,rfp,
				!strcmp(amode,"CFB1"));
		    }
		step = 6;
		}
	    break;

	case 6:
	    if (ibuf[0] != '\n')
		{
		err = 1;
		printf("Missing terminator\n");
		}
	    else if (strcmp(atest, "MCT") != 0)
		{ /* MCT already added terminating nl */
		fputs(ibuf, rfp);
		}
	    step = 1;
	    break;
	    }
	}
    if (rfp)
	fclose(rfp);
    if (afp)
	fclose(afp);
    return err;
    }

/*--------------------------------------------------
  Processes either a single file or 
  a set of files whose names are passed in a file.
  A single file is specified as:
    aes_test -f xxx.req
  A set of files is specified as:
    aes_test -d xxxxx.xxx
  The default is: -d req.txt
--------------------------------------------------*/
int main(int argc, char **argv)
    {
    char *rqlist = "req.txt", *rspfile = NULL;
    FILE *fp = NULL;
    char fn[250] = "", rfn[256] = "";
    int f_opt = 0, d_opt = 1;

#ifdef OPENSSL_FIPS
    if(!FIPS_mode_set(1))
	{
	do_print_errors();
	EXIT(1);
	}
#endif
    if (argc > 1)
	{
	if (strcasecmp(argv[1], "-d") == 0)
	    {
	    d_opt = 1;
	    }
	else if (strcasecmp(argv[1], "-f") == 0)
	    {
	    f_opt = 1;
	    d_opt = 0;
	    }
	else
	    {
	    printf("Invalid parameter: %s\n", argv[1]);
	    return 0;
	    }
	if (argc < 3)
	    {
	    printf("Missing parameter\n");
	    return 0;
	    }
	if (d_opt)
	    rqlist = argv[2];
	else
	    {
	    strcpy(fn, argv[2]);
	    rspfile = argv[3];
	    }
	}
    if (d_opt)
	{ /* list of files (directory) */
	if (!(fp = fopen(rqlist, "r")))
	    {
	    printf("Cannot open req list file\n");
	    return -1;
	    }
	while (fgets(fn, sizeof(fn), fp))
	    {
	    strtok(fn, "\r\n");
	    strcpy(rfn, fn);
	    if (VERBOSE)
		printf("Processing: %s\n", rfn);
	    if (proc_file(rfn, rspfile))
		{
		printf(">>> Processing failed for: %s <<<\n", rfn);
		EXIT(1);
		}
	    }
	fclose(fp);
	}
    else /* single file */
	{
	if (VERBOSE)
	    printf("Processing: %s\n", fn);
	if (proc_file(fn, rspfile))
	    {
	    printf(">>> Processing failed for: %s <<<\n", fn);
	    }
	}
    EXIT(0);
    return 0;
    }

#endif
