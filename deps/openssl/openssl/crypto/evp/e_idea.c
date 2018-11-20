/* crypto/evp/e_idea.c */
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

#ifndef OPENSSL_NO_IDEA
# include <openssl/evp.h>
# include <openssl/objects.h>
# include "evp_locl.h"
# include <openssl/idea.h>

static int idea_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                         const unsigned char *iv, int enc);

/*
 * NB idea_ecb_encrypt doesn't take an 'encrypt' argument so we treat it as a
 * special case
 */

static int idea_ecb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                           const unsigned char *in, size_t inl)
{
    BLOCK_CIPHER_ecb_loop()
        idea_ecb_encrypt(in + i, out + i, ctx->cipher_data);
    return 1;
}

/* Can't use IMPLEMENT_BLOCK_CIPHER because idea_ecb_encrypt is different */

typedef struct {
    IDEA_KEY_SCHEDULE ks;
} EVP_IDEA_KEY;

BLOCK_CIPHER_func_cbc(idea, idea, EVP_IDEA_KEY, ks)
    BLOCK_CIPHER_func_ofb(idea, idea, 64, EVP_IDEA_KEY, ks)
    BLOCK_CIPHER_func_cfb(idea, idea, 64, EVP_IDEA_KEY, ks)

    BLOCK_CIPHER_defs(idea, IDEA_KEY_SCHEDULE, NID_idea, 8, 16, 8, 64,
                  0, idea_init_key, NULL,
                  EVP_CIPHER_set_asn1_iv, EVP_CIPHER_get_asn1_iv, NULL)

static int idea_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                         const unsigned char *iv, int enc)
{
    if (!enc) {
        if (EVP_CIPHER_CTX_mode(ctx) == EVP_CIPH_OFB_MODE)
            enc = 1;
        else if (EVP_CIPHER_CTX_mode(ctx) == EVP_CIPH_CFB_MODE)
            enc = 1;
    }
    if (enc)
        idea_set_encrypt_key(key, ctx->cipher_data);
    else {
        IDEA_KEY_SCHEDULE tmp;

        idea_set_encrypt_key(key, &tmp);
        idea_set_decrypt_key(&tmp, ctx->cipher_data);
        OPENSSL_cleanse((unsigned char *)&tmp, sizeof(IDEA_KEY_SCHEDULE));
    }
    return 1;
}

#endif
