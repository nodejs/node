/* crypto/sha/sha256t.c */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
 * ====================================================================
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <openssl/sha.h>
#include <openssl/evp.h>

#if defined(OPENSSL_NO_SHA) || defined(OPENSSL_NO_SHA256)
int main(int argc, char *argv[])
{
    printf("No SHA256 support\n");
    return(0);
}
#else

unsigned char app_b1[SHA256_DIGEST_LENGTH] = {
	0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,
	0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
	0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,
	0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad	};

unsigned char app_b2[SHA256_DIGEST_LENGTH] = {
	0x24,0x8d,0x6a,0x61,0xd2,0x06,0x38,0xb8,
	0xe5,0xc0,0x26,0x93,0x0c,0x3e,0x60,0x39,
	0xa3,0x3c,0xe4,0x59,0x64,0xff,0x21,0x67,
	0xf6,0xec,0xed,0xd4,0x19,0xdb,0x06,0xc1	};

unsigned char app_b3[SHA256_DIGEST_LENGTH] = {
	0xcd,0xc7,0x6e,0x5c,0x99,0x14,0xfb,0x92,
	0x81,0xa1,0xc7,0xe2,0x84,0xd7,0x3e,0x67,
	0xf1,0x80,0x9a,0x48,0xa4,0x97,0x20,0x0e,
	0x04,0x6d,0x39,0xcc,0xc7,0x11,0x2c,0xd0	};

unsigned char addenum_1[SHA224_DIGEST_LENGTH] = {
	0x23,0x09,0x7d,0x22,0x34,0x05,0xd8,0x22,
	0x86,0x42,0xa4,0x77,0xbd,0xa2,0x55,0xb3,
	0x2a,0xad,0xbc,0xe4,0xbd,0xa0,0xb3,0xf7,
	0xe3,0x6c,0x9d,0xa7 };

unsigned char addenum_2[SHA224_DIGEST_LENGTH] = {
	0x75,0x38,0x8b,0x16,0x51,0x27,0x76,0xcc,
	0x5d,0xba,0x5d,0xa1,0xfd,0x89,0x01,0x50,
	0xb0,0xc6,0x45,0x5c,0xb4,0xf5,0x8b,0x19,
	0x52,0x52,0x25,0x25 };

unsigned char addenum_3[SHA224_DIGEST_LENGTH] = {
	0x20,0x79,0x46,0x55,0x98,0x0c,0x91,0xd8,
	0xbb,0xb4,0xc1,0xea,0x97,0x61,0x8a,0x4b,
	0xf0,0x3f,0x42,0x58,0x19,0x48,0xb2,0xee,
	0x4e,0xe7,0xad,0x67 };

int main (int argc,char **argv)
{ unsigned char md[SHA256_DIGEST_LENGTH];
  int		i;
  EVP_MD_CTX	evp;

    fprintf(stdout,"Testing SHA-256 ");

    EVP_Digest ("abc",3,md,NULL,EVP_sha256(),NULL);
    if (memcmp(md,app_b1,sizeof(app_b1)))
    {	fflush(stdout);
	fprintf(stderr,"\nTEST 1 of 3 failed.\n");
	return 1;
    }
    else
	fprintf(stdout,"."); fflush(stdout);

    EVP_Digest ("abcdbcde""cdefdefg""efghfghi""ghijhijk"
		"ijkljklm""klmnlmno""mnopnopq",56,md,NULL,EVP_sha256(),NULL);
    if (memcmp(md,app_b2,sizeof(app_b2)))
    {	fflush(stdout);
	fprintf(stderr,"\nTEST 2 of 3 failed.\n");
	return 1;
    }
    else
	fprintf(stdout,"."); fflush(stdout);

    EVP_MD_CTX_init (&evp);
    EVP_DigestInit_ex (&evp,EVP_sha256(),NULL);
    for (i=0;i<1000000;i+=160)
	EVP_DigestUpdate (&evp,	"aaaaaaaa""aaaaaaaa""aaaaaaaa""aaaaaaaa"
				"aaaaaaaa""aaaaaaaa""aaaaaaaa""aaaaaaaa"
				"aaaaaaaa""aaaaaaaa""aaaaaaaa""aaaaaaaa"
				"aaaaaaaa""aaaaaaaa""aaaaaaaa""aaaaaaaa"
				"aaaaaaaa""aaaaaaaa""aaaaaaaa""aaaaaaaa",
				(1000000-i)<160?1000000-i:160);
    EVP_DigestFinal_ex (&evp,md,NULL);
    EVP_MD_CTX_cleanup (&evp);

    if (memcmp(md,app_b3,sizeof(app_b3)))
    {	fflush(stdout);
	fprintf(stderr,"\nTEST 3 of 3 failed.\n");
	return 1;
    }
    else
	fprintf(stdout,"."); fflush(stdout);

    fprintf(stdout," passed.\n"); fflush(stdout);

    fprintf(stdout,"Testing SHA-224 ");

    EVP_Digest ("abc",3,md,NULL,EVP_sha224(),NULL);
    if (memcmp(md,addenum_1,sizeof(addenum_1)))
    {	fflush(stdout);
	fprintf(stderr,"\nTEST 1 of 3 failed.\n");
	return 1;
    }
    else
	fprintf(stdout,"."); fflush(stdout);

    EVP_Digest ("abcdbcde""cdefdefg""efghfghi""ghijhijk"
		"ijkljklm""klmnlmno""mnopnopq",56,md,NULL,EVP_sha224(),NULL);
    if (memcmp(md,addenum_2,sizeof(addenum_2)))
    {	fflush(stdout);
	fprintf(stderr,"\nTEST 2 of 3 failed.\n");
	return 1;
    }
    else
	fprintf(stdout,"."); fflush(stdout);

    EVP_MD_CTX_init (&evp);
    EVP_DigestInit_ex (&evp,EVP_sha224(),NULL);
    for (i=0;i<1000000;i+=64)
	EVP_DigestUpdate (&evp,	"aaaaaaaa""aaaaaaaa""aaaaaaaa""aaaaaaaa"
				"aaaaaaaa""aaaaaaaa""aaaaaaaa""aaaaaaaa",
				(1000000-i)<64?1000000-i:64);
    EVP_DigestFinal_ex (&evp,md,NULL);
    EVP_MD_CTX_cleanup (&evp);

    if (memcmp(md,addenum_3,sizeof(addenum_3)))
    {	fflush(stdout);
	fprintf(stderr,"\nTEST 3 of 3 failed.\n");
	return 1;
    }
    else
	fprintf(stdout,"."); fflush(stdout);

    fprintf(stdout," passed.\n"); fflush(stdout);

  return 0;
}
#endif
