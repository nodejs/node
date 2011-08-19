/* crypto/ec/ec_print.c */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
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
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <openssl/crypto.h>
#include "ec_lcl.h"

BIGNUM *EC_POINT_point2bn(const EC_GROUP *group, 
                          const EC_POINT *point, 
                          point_conversion_form_t form,
                          BIGNUM *ret,
                          BN_CTX *ctx)
	{
	size_t        buf_len=0;
	unsigned char *buf;

	buf_len = EC_POINT_point2oct(group, point, form,
                                     NULL, 0, ctx);
	if (buf_len == 0)
		return NULL;

	if ((buf = OPENSSL_malloc(buf_len)) == NULL)
		return NULL;

	if (!EC_POINT_point2oct(group, point, form, buf, buf_len, ctx))
		{
		OPENSSL_free(buf);
		return NULL;
		}

	ret = BN_bin2bn(buf, buf_len, ret);

	OPENSSL_free(buf);

	return ret;
}

EC_POINT *EC_POINT_bn2point(const EC_GROUP *group,
                            const BIGNUM *bn,
                            EC_POINT *point, 
                            BN_CTX *ctx)
	{
	size_t        buf_len=0;
	unsigned char *buf;
	EC_POINT      *ret;

	if ((buf_len = BN_num_bytes(bn)) == 0) return NULL;
	buf = OPENSSL_malloc(buf_len);
	if (buf == NULL)
		return NULL;

	if (!BN_bn2bin(bn, buf)) 
		{
		OPENSSL_free(buf);
		return NULL;
		}

	if (point == NULL)
		{
		if ((ret = EC_POINT_new(group)) == NULL)
			{
			OPENSSL_free(buf);
			return NULL;
			}
		}
	else
		ret = point;

	if (!EC_POINT_oct2point(group, ret, buf, buf_len, ctx))
		{
		if (point == NULL)
			EC_POINT_clear_free(ret);
		OPENSSL_free(buf);
		return NULL;
		}

	OPENSSL_free(buf);
	return ret;
	}

static const char *HEX_DIGITS = "0123456789ABCDEF";

/* the return value must be freed (using OPENSSL_free()) */
char *EC_POINT_point2hex(const EC_GROUP *group,
                         const EC_POINT *point,
                         point_conversion_form_t form,
                         BN_CTX *ctx)
	{
	char          *ret, *p;
	size_t        buf_len=0,i;
	unsigned char *buf, *pbuf;

	buf_len = EC_POINT_point2oct(group, point, form,
                                     NULL, 0, ctx);
	if (buf_len == 0)
		return NULL;

	if ((buf = OPENSSL_malloc(buf_len)) == NULL)
		return NULL;

	if (!EC_POINT_point2oct(group, point, form, buf, buf_len, ctx))
		{
		OPENSSL_free(buf);
		return NULL;
		}

	ret = (char *)OPENSSL_malloc(buf_len*2+2);
	if (ret == NULL)
		{
		OPENSSL_free(buf);
		return NULL;
		}
	p = ret;
	pbuf = buf;
	for (i=buf_len; i > 0; i--)
		{
			int v = (int) *(pbuf++);
			*(p++)=HEX_DIGITS[v>>4];
			*(p++)=HEX_DIGITS[v&0x0F];
		}
	*p='\0';

	OPENSSL_free(buf);

	return ret;
	}

EC_POINT *EC_POINT_hex2point(const EC_GROUP *group,
                             const char *buf,
                             EC_POINT *point,
                             BN_CTX *ctx)
	{
	EC_POINT *ret=NULL;
	BIGNUM   *tmp_bn=NULL;

	if (!BN_hex2bn(&tmp_bn, buf))
		return NULL;

	ret = EC_POINT_bn2point(group, tmp_bn, point, ctx);

	BN_clear_free(tmp_bn);

	return ret;
	}
