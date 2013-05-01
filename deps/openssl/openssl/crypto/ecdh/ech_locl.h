/* crypto/ecdh/ech_locl.h */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
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

#ifndef HEADER_ECH_LOCL_H
#define HEADER_ECH_LOCL_H

#include <openssl/ecdh.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct ecdh_method 
	{
	const char *name;
	int (*compute_key)(void *key, size_t outlen, const EC_POINT *pub_key, EC_KEY *ecdh,
	                   void *(*KDF)(const void *in, size_t inlen, void *out, size_t *outlen));
#if 0
	int (*init)(EC_KEY *eckey);
	int (*finish)(EC_KEY *eckey);
#endif
	int flags;
	char *app_data;
	};

/* If this flag is set the ECDH method is FIPS compliant and can be used
 * in FIPS mode. This is set in the validated module method. If an
 * application sets this flag in its own methods it is its responsibility
 * to ensure the result is compliant.
 */

#define ECDH_FLAG_FIPS_METHOD	0x1

typedef struct ecdh_data_st {
	/* EC_KEY_METH_DATA part */
	int (*init)(EC_KEY *);
	/* method specific part */
	ENGINE	*engine;
	int	flags;
	const ECDH_METHOD *meth;
	CRYPTO_EX_DATA ex_data;
} ECDH_DATA;

ECDH_DATA *ecdh_check(EC_KEY *);

#ifdef  __cplusplus
}
#endif

#endif /* HEADER_ECH_LOCL_H */
