/* crypto/buffer/buf_str.c */
/* ====================================================================
 * Copyright (c) 2007 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/buffer.h>

char *BUF_strdup(const char *str)
	{
	if (str == NULL) return(NULL);
	return BUF_strndup(str, strlen(str));
	}

char *BUF_strndup(const char *str, size_t siz)
	{
	char *ret;

	if (str == NULL) return(NULL);

	ret=OPENSSL_malloc(siz+1);
	if (ret == NULL) 
		{
		BUFerr(BUF_F_BUF_STRNDUP,ERR_R_MALLOC_FAILURE);
		return(NULL);
		}
	BUF_strlcpy(ret,str,siz+1);
	return(ret);
	}

void *BUF_memdup(const void *data, size_t siz)
	{
	void *ret;

	if (data == NULL) return(NULL);

	ret=OPENSSL_malloc(siz);
	if (ret == NULL) 
		{
		BUFerr(BUF_F_BUF_MEMDUP,ERR_R_MALLOC_FAILURE);
		return(NULL);
		}
	return memcpy(ret, data, siz);
	}	

size_t BUF_strlcpy(char *dst, const char *src, size_t size)
	{
	size_t l = 0;
	for(; size > 1 && *src; size--)
		{
		*dst++ = *src++;
		l++;
		}
	if (size)
		*dst = '\0';
	return l + strlen(src);
	}

size_t BUF_strlcat(char *dst, const char *src, size_t size)
	{
	size_t l = 0;
	for(; size > 0 && *dst; size--, dst++)
		l++;
	return l + BUF_strlcpy(dst, src, size);
	}
