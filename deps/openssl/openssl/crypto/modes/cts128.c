/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project. All rights reserved.
 *
 * Rights for redistribution and usage in source and binary
 * forms are granted according to the OpenSSL license.
 */

#include <openssl/crypto.h>
#include "modes_lcl.h"
#include <string.h>

#ifndef MODES_DEBUG
# ifndef NDEBUG
#  define NDEBUG
# endif
#endif
#include <assert.h>

/*
 * Trouble with Ciphertext Stealing, CTS, mode is that there is no
 * common official specification, but couple of cipher/application
 * specific ones: RFC2040 and RFC3962. Then there is 'Proposal to
 * Extend CBC Mode By "Ciphertext Stealing"' at NIST site, which
 * deviates from mentioned RFCs. Most notably it allows input to be
 * of block length and it doesn't flip the order of the last two
 * blocks. CTS is being discussed even in ECB context, but it's not
 * adopted for any known application. This implementation provides
 * two interfaces: one compliant with above mentioned RFCs and one
 * compliant with the NIST proposal, both extending CBC mode.
 */

size_t CRYPTO_cts128_encrypt_block(const unsigned char *in, unsigned char *out,
			size_t len, const void *key,
			unsigned char ivec[16], block128_f block)
{	size_t residue, n;

	assert (in && out && key && ivec);

	if (len <= 16) return 0;

	if ((residue=len%16) == 0) residue = 16;

	len -= residue;

	CRYPTO_cbc128_encrypt(in,out,len,key,ivec,block);

	in  += len;
	out += len;

	for (n=0; n<residue; ++n)
		ivec[n] ^= in[n];
	(*block)(ivec,ivec,key);
	memcpy(out,out-16,residue);
	memcpy(out-16,ivec,16); 

	return len+residue;
}

size_t CRYPTO_nistcts128_encrypt_block(const unsigned char *in, unsigned char *out,
			size_t len, const void *key,
			unsigned char ivec[16], block128_f block)
{	size_t residue, n;

	assert (in && out && key && ivec);

	if (len < 16) return 0;

	residue=len%16;

	len -= residue;

	CRYPTO_cbc128_encrypt(in,out,len,key,ivec,block);

	if (residue==0)	return len;

	in  += len;
	out += len;

	for (n=0; n<residue; ++n)
		ivec[n] ^= in[n];
	(*block)(ivec,ivec,key);
	memcpy(out-16+residue,ivec,16);

	return len+residue;
}

size_t CRYPTO_cts128_encrypt(const unsigned char *in, unsigned char *out,
			size_t len, const void *key,
			unsigned char ivec[16], cbc128_f cbc)
{	size_t residue;
	union { size_t align; unsigned char c[16]; } tmp;

	assert (in && out && key && ivec);

	if (len <= 16) return 0;

	if ((residue=len%16) == 0) residue = 16;

	len -= residue;

	(*cbc)(in,out,len,key,ivec,1);

	in  += len;
	out += len;

#if defined(CBC_HANDLES_TRUNCATED_IO)
	memcpy(tmp.c,out-16,16);
	(*cbc)(in,out-16,residue,key,ivec,1);
	memcpy(out,tmp.c,residue);
#else
	memset(tmp.c,0,sizeof(tmp));
	memcpy(tmp.c,in,residue);
	memcpy(out,out-16,residue);
	(*cbc)(tmp.c,out-16,16,key,ivec,1);
#endif
	return len+residue;
}

size_t CRYPTO_nistcts128_encrypt(const unsigned char *in, unsigned char *out,
			size_t len, const void *key,
			unsigned char ivec[16], cbc128_f cbc)
{	size_t residue;
	union { size_t align; unsigned char c[16]; } tmp;

	assert (in && out && key && ivec);

	if (len < 16) return 0;

	residue=len%16;

	len -= residue;

	(*cbc)(in,out,len,key,ivec,1);

	if (residue==0) return len;

	in  += len;
	out += len;

#if defined(CBC_HANDLES_TRUNCATED_IO)
	(*cbc)(in,out-16+residue,residue,key,ivec,1);
#else
	memset(tmp.c,0,sizeof(tmp));
	memcpy(tmp.c,in,residue);
	(*cbc)(tmp.c,out-16+residue,16,key,ivec,1);
#endif
	return len+residue;
}

size_t CRYPTO_cts128_decrypt_block(const unsigned char *in, unsigned char *out,
			size_t len, const void *key,
			unsigned char ivec[16], block128_f block)
{	size_t residue, n;
	union { size_t align; unsigned char c[32]; } tmp;

	assert (in && out && key && ivec);

	if (len<=16) return 0;

	if ((residue=len%16) == 0) residue = 16;

	len -= 16+residue;

	if (len) {
		CRYPTO_cbc128_decrypt(in,out,len,key,ivec,block);
		in  += len;
		out += len;
	}

	(*block)(in,tmp.c+16,key);

	memcpy(tmp.c,tmp.c+16,16);
	memcpy(tmp.c,in+16,residue);
	(*block)(tmp.c,tmp.c,key);

	for(n=0; n<16; ++n) {
		unsigned char c = in[n];
		out[n] = tmp.c[n] ^ ivec[n];
		ivec[n] = c;
	}
	for(residue+=16; n<residue; ++n)
		out[n] = tmp.c[n] ^ in[n];

	return 16+len+residue;
}

size_t CRYPTO_nistcts128_decrypt_block(const unsigned char *in, unsigned char *out,
			size_t len, const void *key,
			unsigned char ivec[16], block128_f block)
{	size_t residue, n;
	union { size_t align; unsigned char c[32]; } tmp;

	assert (in && out && key && ivec);

	if (len<16) return 0;

	residue=len%16;

	if (residue==0) {
		CRYPTO_cbc128_decrypt(in,out,len,key,ivec,block);
		return len;
	}

	len -= 16+residue;

	if (len) {
		CRYPTO_cbc128_decrypt(in,out,len,key,ivec,block);
		in  += len;
		out += len;
	}

	(*block)(in+residue,tmp.c+16,key);

	memcpy(tmp.c,tmp.c+16,16);
	memcpy(tmp.c,in,residue);
	(*block)(tmp.c,tmp.c,key);

	for(n=0; n<16; ++n) {
		unsigned char c = in[n];
		out[n] = tmp.c[n] ^ ivec[n];
		ivec[n] = in[n+residue];
		tmp.c[n] = c;
	}
	for(residue+=16; n<residue; ++n)
		out[n] = tmp.c[n] ^ tmp.c[n-16];

	return 16+len+residue;
}

size_t CRYPTO_cts128_decrypt(const unsigned char *in, unsigned char *out,
			size_t len, const void *key,
			unsigned char ivec[16], cbc128_f cbc)
{	size_t residue;
	union { size_t align; unsigned char c[32]; } tmp;

	assert (in && out && key && ivec);

	if (len<=16) return 0;

	if ((residue=len%16) == 0) residue = 16;

	len -= 16+residue;

	if (len) {
		(*cbc)(in,out,len,key,ivec,0);
		in  += len;
		out += len;
	}

	memset(tmp.c,0,sizeof(tmp));
	/* this places in[16] at &tmp.c[16] and decrypted block at &tmp.c[0] */
	(*cbc)(in,tmp.c,16,key,tmp.c+16,0);

	memcpy(tmp.c,in+16,residue);
#if defined(CBC_HANDLES_TRUNCATED_IO)
	(*cbc)(tmp.c,out,16+residue,key,ivec,0);
#else
	(*cbc)(tmp.c,tmp.c,32,key,ivec,0);
	memcpy(out,tmp.c,16+residue);
#endif
	return 16+len+residue;
}

size_t CRYPTO_nistcts128_decrypt(const unsigned char *in, unsigned char *out,
			size_t len, const void *key,
			unsigned char ivec[16], cbc128_f cbc)
{	size_t residue;
	union { size_t align; unsigned char c[32]; } tmp;

	assert (in && out && key && ivec);

	if (len<16) return 0;

	residue=len%16;

	if (residue==0) {
		(*cbc)(in,out,len,key,ivec,0);
		return len;
	}

	len -= 16+residue;

	if (len) {
		(*cbc)(in,out,len,key,ivec,0);
		in  += len;
		out += len;
	}

	memset(tmp.c,0,sizeof(tmp));
	/* this places in[16] at &tmp.c[16] and decrypted block at &tmp.c[0] */
	(*cbc)(in+residue,tmp.c,16,key,tmp.c+16,0);

	memcpy(tmp.c,in,residue);
#if defined(CBC_HANDLES_TRUNCATED_IO)
	(*cbc)(tmp.c,out,16+residue,key,ivec,0);
#else
	(*cbc)(tmp.c,tmp.c,32,key,ivec,0);
	memcpy(out,tmp.c,16+residue);
#endif
	return 16+len+residue;
}

#if defined(SELFTEST)
#include <stdio.h>
#include <openssl/aes.h>

/* test vectors from RFC 3962 */
static const unsigned char test_key[16] = "chicken teriyaki";
static const unsigned char test_input[64] =
		"I would like the" " General Gau's C"
		"hicken, please, " "and wonton soup.";
static const unsigned char test_iv[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static const unsigned char vector_17[17] =
{0xc6,0x35,0x35,0x68,0xf2,0xbf,0x8c,0xb4, 0xd8,0xa5,0x80,0x36,0x2d,0xa7,0xff,0x7f,
 0x97};
static const unsigned char vector_31[31] =
{0xfc,0x00,0x78,0x3e,0x0e,0xfd,0xb2,0xc1, 0xd4,0x45,0xd4,0xc8,0xef,0xf7,0xed,0x22,
 0x97,0x68,0x72,0x68,0xd6,0xec,0xcc,0xc0, 0xc0,0x7b,0x25,0xe2,0x5e,0xcf,0xe5};
static const unsigned char vector_32[32] =
{0x39,0x31,0x25,0x23,0xa7,0x86,0x62,0xd5, 0xbe,0x7f,0xcb,0xcc,0x98,0xeb,0xf5,0xa8,
 0x97,0x68,0x72,0x68,0xd6,0xec,0xcc,0xc0, 0xc0,0x7b,0x25,0xe2,0x5e,0xcf,0xe5,0x84};
static const unsigned char vector_47[47] =
{0x97,0x68,0x72,0x68,0xd6,0xec,0xcc,0xc0, 0xc0,0x7b,0x25,0xe2,0x5e,0xcf,0xe5,0x84,
 0xb3,0xff,0xfd,0x94,0x0c,0x16,0xa1,0x8c, 0x1b,0x55,0x49,0xd2,0xf8,0x38,0x02,0x9e,
 0x39,0x31,0x25,0x23,0xa7,0x86,0x62,0xd5, 0xbe,0x7f,0xcb,0xcc,0x98,0xeb,0xf5};
static const unsigned char vector_48[48] =
{0x97,0x68,0x72,0x68,0xd6,0xec,0xcc,0xc0, 0xc0,0x7b,0x25,0xe2,0x5e,0xcf,0xe5,0x84,
 0x9d,0xad,0x8b,0xbb,0x96,0xc4,0xcd,0xc0, 0x3b,0xc1,0x03,0xe1,0xa1,0x94,0xbb,0xd8,
 0x39,0x31,0x25,0x23,0xa7,0x86,0x62,0xd5, 0xbe,0x7f,0xcb,0xcc,0x98,0xeb,0xf5,0xa8};
static const unsigned char vector_64[64] =
{0x97,0x68,0x72,0x68,0xd6,0xec,0xcc,0xc0, 0xc0,0x7b,0x25,0xe2,0x5e,0xcf,0xe5,0x84,
 0x39,0x31,0x25,0x23,0xa7,0x86,0x62,0xd5, 0xbe,0x7f,0xcb,0xcc,0x98,0xeb,0xf5,0xa8,
 0x48,0x07,0xef,0xe8,0x36,0xee,0x89,0xa5, 0x26,0x73,0x0d,0xbc,0x2f,0x7b,0xc8,0x40,
 0x9d,0xad,0x8b,0xbb,0x96,0xc4,0xcd,0xc0, 0x3b,0xc1,0x03,0xe1,0xa1,0x94,0xbb,0xd8};

static AES_KEY encks, decks;

void test_vector(const unsigned char *vector,size_t len)
{	unsigned char iv[sizeof(test_iv)];
	unsigned char cleartext[64],ciphertext[64];
	size_t tail;

	printf("vector_%d\n",len); fflush(stdout);

	if ((tail=len%16) == 0) tail = 16;
	tail += 16;

	/* test block-based encryption */
	memcpy(iv,test_iv,sizeof(test_iv));
	CRYPTO_cts128_encrypt_block(test_input,ciphertext,len,&encks,iv,(block128_f)AES_encrypt);
	if (memcmp(ciphertext,vector,len))
		fprintf(stderr,"output_%d mismatch\n",len), exit(1);
	if (memcmp(iv,vector+len-tail,sizeof(iv)))
		fprintf(stderr,"iv_%d mismatch\n",len), exit(1);

	/* test block-based decryption */
	memcpy(iv,test_iv,sizeof(test_iv));
	CRYPTO_cts128_decrypt_block(ciphertext,cleartext,len,&decks,iv,(block128_f)AES_decrypt);
	if (memcmp(cleartext,test_input,len))
		fprintf(stderr,"input_%d mismatch\n",len), exit(2);
	if (memcmp(iv,vector+len-tail,sizeof(iv)))
		fprintf(stderr,"iv_%d mismatch\n",len), exit(2);

	/* test streamed encryption */
	memcpy(iv,test_iv,sizeof(test_iv));
	CRYPTO_cts128_encrypt(test_input,ciphertext,len,&encks,iv,(cbc128_f)AES_cbc_encrypt);
	if (memcmp(ciphertext,vector,len))
		fprintf(stderr,"output_%d mismatch\n",len), exit(3);
	if (memcmp(iv,vector+len-tail,sizeof(iv)))
		fprintf(stderr,"iv_%d mismatch\n",len), exit(3);

	/* test streamed decryption */
	memcpy(iv,test_iv,sizeof(test_iv));
	CRYPTO_cts128_decrypt(ciphertext,cleartext,len,&decks,iv,(cbc128_f)AES_cbc_encrypt);
	if (memcmp(cleartext,test_input,len))
		fprintf(stderr,"input_%d mismatch\n",len), exit(4);
	if (memcmp(iv,vector+len-tail,sizeof(iv)))
		fprintf(stderr,"iv_%d mismatch\n",len), exit(4);
}

void test_nistvector(const unsigned char *vector,size_t len)
{	unsigned char iv[sizeof(test_iv)];
	unsigned char cleartext[64],ciphertext[64],nistvector[64];
	size_t tail;

	printf("nistvector_%d\n",len); fflush(stdout);

	if ((tail=len%16) == 0) tail = 16;

	len -= 16 + tail;
	memcpy(nistvector,vector,len);
	/* flip two last blocks */
	memcpy(nistvector+len,vector+len+16,tail);
	memcpy(nistvector+len+tail,vector+len,16);
	len += 16 + tail;
	tail = 16;

	/* test block-based encryption */
	memcpy(iv,test_iv,sizeof(test_iv));
	CRYPTO_nistcts128_encrypt_block(test_input,ciphertext,len,&encks,iv,(block128_f)AES_encrypt);
	if (memcmp(ciphertext,nistvector,len))
		fprintf(stderr,"output_%d mismatch\n",len), exit(1);
	if (memcmp(iv,nistvector+len-tail,sizeof(iv)))
		fprintf(stderr,"iv_%d mismatch\n",len), exit(1);

	/* test block-based decryption */
	memcpy(iv,test_iv,sizeof(test_iv));
	CRYPTO_nistcts128_decrypt_block(ciphertext,cleartext,len,&decks,iv,(block128_f)AES_decrypt);
	if (memcmp(cleartext,test_input,len))
		fprintf(stderr,"input_%d mismatch\n",len), exit(2);
	if (memcmp(iv,nistvector+len-tail,sizeof(iv)))
		fprintf(stderr,"iv_%d mismatch\n",len), exit(2);

	/* test streamed encryption */
	memcpy(iv,test_iv,sizeof(test_iv));
	CRYPTO_nistcts128_encrypt(test_input,ciphertext,len,&encks,iv,(cbc128_f)AES_cbc_encrypt);
	if (memcmp(ciphertext,nistvector,len))
		fprintf(stderr,"output_%d mismatch\n",len), exit(3);
	if (memcmp(iv,nistvector+len-tail,sizeof(iv)))
		fprintf(stderr,"iv_%d mismatch\n",len), exit(3);

	/* test streamed decryption */
	memcpy(iv,test_iv,sizeof(test_iv));
	CRYPTO_nistcts128_decrypt(ciphertext,cleartext,len,&decks,iv,(cbc128_f)AES_cbc_encrypt);
	if (memcmp(cleartext,test_input,len))
		fprintf(stderr,"input_%d mismatch\n",len), exit(4);
	if (memcmp(iv,nistvector+len-tail,sizeof(iv)))
		fprintf(stderr,"iv_%d mismatch\n",len), exit(4);
}

int main()
{
	AES_set_encrypt_key(test_key,128,&encks);
	AES_set_decrypt_key(test_key,128,&decks);

	test_vector(vector_17,sizeof(vector_17));
	test_vector(vector_31,sizeof(vector_31));
	test_vector(vector_32,sizeof(vector_32));
	test_vector(vector_47,sizeof(vector_47));
	test_vector(vector_48,sizeof(vector_48));
	test_vector(vector_64,sizeof(vector_64));

	test_nistvector(vector_17,sizeof(vector_17));
	test_nistvector(vector_31,sizeof(vector_31));
	test_nistvector(vector_32,sizeof(vector_32));
	test_nistvector(vector_47,sizeof(vector_47));
	test_nistvector(vector_48,sizeof(vector_48));
	test_nistvector(vector_64,sizeof(vector_64));

	return 0;
}
#endif
