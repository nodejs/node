/* crypto/evp/e_camellia.c -*- mode:C; c-file-style: "eay" -*- */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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

#include <openssl/opensslconf.h>
#ifndef OPENSSL_NO_CAMELLIA
# include <openssl/evp.h>
# include <openssl/err.h>
# include <string.h>
# include <assert.h>
# include <openssl/camellia.h>
# include "evp_locl.h"

static int camellia_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                             const unsigned char *iv, int enc);

/* Camellia subkey Structure */
typedef struct {
    CAMELLIA_KEY ks;
} EVP_CAMELLIA_KEY;

/* Attribute operation for Camellia */
# define data(ctx)       EVP_C_DATA(EVP_CAMELLIA_KEY,ctx)

IMPLEMENT_BLOCK_CIPHER(camellia_128, ks, Camellia, EVP_CAMELLIA_KEY,
                       NID_camellia_128, 16, 16, 16, 128,
                       0, camellia_init_key, NULL,
                       EVP_CIPHER_set_asn1_iv, EVP_CIPHER_get_asn1_iv, NULL)
    IMPLEMENT_BLOCK_CIPHER(camellia_192, ks, Camellia, EVP_CAMELLIA_KEY,
                       NID_camellia_192, 16, 24, 16, 128,
                       0, camellia_init_key, NULL,
                       EVP_CIPHER_set_asn1_iv, EVP_CIPHER_get_asn1_iv, NULL)
    IMPLEMENT_BLOCK_CIPHER(camellia_256, ks, Camellia, EVP_CAMELLIA_KEY,
                       NID_camellia_256, 16, 32, 16, 128,
                       0, camellia_init_key, NULL,
                       EVP_CIPHER_set_asn1_iv, EVP_CIPHER_get_asn1_iv, NULL)
# define IMPLEMENT_CAMELLIA_CFBR(ksize,cbits)    IMPLEMENT_CFBR(camellia,Camellia,EVP_CAMELLIA_KEY,ks,ksize,cbits,16)
    IMPLEMENT_CAMELLIA_CFBR(128, 1)
    IMPLEMENT_CAMELLIA_CFBR(192, 1)
    IMPLEMENT_CAMELLIA_CFBR(256, 1)

    IMPLEMENT_CAMELLIA_CFBR(128, 8)
    IMPLEMENT_CAMELLIA_CFBR(192, 8)
    IMPLEMENT_CAMELLIA_CFBR(256, 8)

/* The subkey for Camellia is generated. */
static int camellia_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                             const unsigned char *iv, int enc)
{
    int ret;

    ret = Camellia_set_key(key, ctx->key_len * 8, ctx->cipher_data);

    if (ret < 0) {
        EVPerr(EVP_F_CAMELLIA_INIT_KEY, EVP_R_CAMELLIA_KEY_SETUP_FAILED);
        return 0;
    }

    return 1;
}

#else

# ifdef PEDANTIC
static void *dummy = &dummy;
# endif

#endif
