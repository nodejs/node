/* crypto/err/err_prn.c */
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
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

void ERR_print_errors_cb(int (*cb)(const char *str, size_t len, void *u),
			 void *u)
	{
	unsigned long l;
	char buf[256];
	char buf2[4096];
	const char *file,*data;
	int line,flags;
	unsigned long es;

	es=CRYPTO_thread_id();
	while ((l=ERR_get_error_line_data(&file,&line,&data,&flags)) != 0)
		{
		ERR_error_string_n(l, buf, sizeof buf);
		BIO_snprintf(buf2, sizeof(buf2), "%lu:%s:%s:%d:%s\n", es, buf,
			file, line, (flags & ERR_TXT_STRING) ? data : "");
		if (cb(buf2, strlen(buf2), u) <= 0)
			break; /* abort outputting the error report */
		}
	}

#ifndef OPENSSL_NO_FP_API
static int print_fp(const char *str, size_t len, void *fp)
	{
	BIO bio;

	BIO_set(&bio,BIO_s_file());
	BIO_set_fp(&bio,fp,BIO_NOCLOSE);

	return BIO_printf(&bio, "%s", str);
	}
void ERR_print_errors_fp(FILE *fp)
	{
	ERR_print_errors_cb(print_fp, fp);
	}
#endif

void ERR_error_string_n(unsigned long e, char *buf, size_t len)
	{
	char lsbuf[64], fsbuf[64], rsbuf[64];
	const char *ls,*fs,*rs;
	unsigned long l,f,r;

	l=ERR_GET_LIB(e);
	f=ERR_GET_FUNC(e);
	r=ERR_GET_REASON(e);

	ls=ERR_lib_error_string(e);
	fs=ERR_func_error_string(e);
	rs=ERR_reason_error_string(e);

	if (ls == NULL) 
		BIO_snprintf(lsbuf, sizeof(lsbuf), "lib(%lu)", l);
	if (fs == NULL)
		BIO_snprintf(fsbuf, sizeof(fsbuf), "func(%lu)", f);
	if (rs == NULL)
		BIO_snprintf(rsbuf, sizeof(rsbuf), "reason(%lu)", r);

	BIO_snprintf(buf, len,"error:%08lX:%s:%s:%s", e, ls?ls:lsbuf, 
		fs?fs:fsbuf, rs?rs:rsbuf);
	if (strlen(buf) == len-1)
		{
		/* output may be truncated; make sure we always have 5 
		 * colon-separated fields, i.e. 4 colons ... */
#define NUM_COLONS 4
		if (len > NUM_COLONS) /* ... if possible */
			{
			int i;
			char *s = buf;
			
			for (i = 0; i < NUM_COLONS; i++)
				{
				char *colon = strchr(s, ':');
				if (colon == NULL || colon > &buf[len-1] - NUM_COLONS + i)
					{
					/* set colon no. i at last possible position
					 * (buf[len-1] is the terminating 0)*/
					colon = &buf[len-1] - NUM_COLONS + i;
					*colon = ':';
					}
				s = colon + 1;
				}
			}
		}
	}

/* BAD for multi-threading: uses a local buffer if ret == NULL */
/* ERR_error_string_n should be used instead for ret != NULL
 * as ERR_error_string cannot know how large the buffer is */
char *ERR_error_string(unsigned long e, char *ret)
	{
	static char buf[256];

	if (ret == NULL) ret=buf;
	ERR_error_string_n(e, ret, 256);

	return ret;
	}
