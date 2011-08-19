/* crypto/dh/p1024.c */
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
#include <openssl/bn.h>
#include <openssl/asn1.h>
#include <openssl/dh.h>
#include <openssl/pem.h>

unsigned char data[]={0x97,0xF6,0x42,0x61,0xCA,0xB5,0x05,0xDD,
	0x28,0x28,0xE1,0x3F,0x1D,0x68,0xB6,0xD3,
	0xDB,0xD0,0xF3,0x13,0x04,0x7F,0x40,0xE8,
	0x56,0xDA,0x58,0xCB,0x13,0xB8,0xA1,0xBF,
	0x2B,0x78,0x3A,0x4C,0x6D,0x59,0xD5,0xF9,
	0x2A,0xFC,0x6C,0xFF,0x3D,0x69,0x3F,0x78,
	0xB2,0x3D,0x4F,0x31,0x60,0xA9,0x50,0x2E,
	0x3E,0xFA,0xF7,0xAB,0x5E,0x1A,0xD5,0xA6,
	0x5E,0x55,0x43,0x13,0x82,0x8D,0xA8,0x3B,
	0x9F,0xF2,0xD9,0x41,0xDE,0xE9,0x56,0x89,
	0xFA,0xDA,0xEA,0x09,0x36,0xAD,0xDF,0x19,
	0x71,0xFE,0x63,0x5B,0x20,0xAF,0x47,0x03,
	0x64,0x60,0x3C,0x2D,0xE0,0x59,0xF5,0x4B,
	0x65,0x0A,0xD8,0xFA,0x0C,0xF7,0x01,0x21,
	0xC7,0x47,0x99,0xD7,0x58,0x71,0x32,0xBE,
	0x9B,0x99,0x9B,0xB9,0xB7,0x87,0xE8,0xAB,
	};

main()
	{
	DH *dh;

	dh=DH_new();
	dh->p=BN_bin2bn(data,sizeof(data),NULL);
	dh->g=BN_new();
	BN_set_word(dh->g,2);
	PEM_write_DHparams(stdout,dh);
	}
