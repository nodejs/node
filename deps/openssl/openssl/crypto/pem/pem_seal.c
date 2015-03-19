/* crypto/pem/pem_seal.c */
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

#include <openssl/opensslconf.h> /* for OPENSSL_NO_RSA */
#ifndef OPENSSL_NO_RSA
# include <stdio.h>
# include "cryptlib.h"
# include <openssl/evp.h>
# include <openssl/rand.h>
# include <openssl/objects.h>
# include <openssl/x509.h>
# include <openssl/pem.h>
# include <openssl/rsa.h>

int PEM_SealInit(PEM_ENCODE_SEAL_CTX *ctx, EVP_CIPHER *type, EVP_MD *md_type,
                 unsigned char **ek, int *ekl, unsigned char *iv,
                 EVP_PKEY **pubk, int npubk)
{
    unsigned char key[EVP_MAX_KEY_LENGTH];
    int ret = -1;
    int i, j, max = 0;
    char *s = NULL;

    for (i = 0; i < npubk; i++) {
        if (pubk[i]->type != EVP_PKEY_RSA) {
            PEMerr(PEM_F_PEM_SEALINIT, PEM_R_PUBLIC_KEY_NO_RSA);
            goto err;
        }
        j = RSA_size(pubk[i]->pkey.rsa);
        if (j > max)
            max = j;
    }
    s = (char *)OPENSSL_malloc(max * 2);
    if (s == NULL) {
        PEMerr(PEM_F_PEM_SEALINIT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    EVP_EncodeInit(&ctx->encode);

    EVP_MD_CTX_init(&ctx->md);
    if (!EVP_SignInit(&ctx->md, md_type))
        goto err;

    EVP_CIPHER_CTX_init(&ctx->cipher);
    ret = EVP_SealInit(&ctx->cipher, type, ek, ekl, iv, pubk, npubk);
    if (ret <= 0)
        goto err;

    /* base64 encode the keys */
    for (i = 0; i < npubk; i++) {
        j = EVP_EncodeBlock((unsigned char *)s, ek[i],
                            RSA_size(pubk[i]->pkey.rsa));
        ekl[i] = j;
        memcpy(ek[i], s, j + 1);
    }

    ret = npubk;
 err:
    if (s != NULL)
        OPENSSL_free(s);
    OPENSSL_cleanse(key, EVP_MAX_KEY_LENGTH);
    return (ret);
}

void PEM_SealUpdate(PEM_ENCODE_SEAL_CTX *ctx, unsigned char *out, int *outl,
                    unsigned char *in, int inl)
{
    unsigned char buffer[1600];
    int i, j;

    *outl = 0;
    EVP_SignUpdate(&ctx->md, in, inl);
    for (;;) {
        if (inl <= 0)
            break;
        if (inl > 1200)
            i = 1200;
        else
            i = inl;
        EVP_EncryptUpdate(&ctx->cipher, buffer, &j, in, i);
        EVP_EncodeUpdate(&ctx->encode, out, &j, buffer, j);
        *outl += j;
        out += j;
        in += i;
        inl -= i;
    }
}

int PEM_SealFinal(PEM_ENCODE_SEAL_CTX *ctx, unsigned char *sig, int *sigl,
                  unsigned char *out, int *outl, EVP_PKEY *priv)
{
    unsigned char *s = NULL;
    int ret = 0, j;
    unsigned int i;

    if (priv->type != EVP_PKEY_RSA) {
        PEMerr(PEM_F_PEM_SEALFINAL, PEM_R_PUBLIC_KEY_NO_RSA);
        goto err;
    }
    i = RSA_size(priv->pkey.rsa);
    if (i < 100)
        i = 100;
    s = (unsigned char *)OPENSSL_malloc(i * 2);
    if (s == NULL) {
        PEMerr(PEM_F_PEM_SEALFINAL, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (!EVP_EncryptFinal_ex(&ctx->cipher, s, (int *)&i))
        goto err;
    EVP_EncodeUpdate(&ctx->encode, out, &j, s, i);
    *outl = j;
    out += j;
    EVP_EncodeFinal(&ctx->encode, out, &j);
    *outl += j;

    if (!EVP_SignFinal(&ctx->md, s, &i, priv))
        goto err;
    *sigl = EVP_EncodeBlock(sig, s, i);

    ret = 1;
 err:
    EVP_MD_CTX_cleanup(&ctx->md);
    EVP_CIPHER_CTX_cleanup(&ctx->cipher);
    if (s != NULL)
        OPENSSL_free(s);
    return (ret);
}
#else                           /* !OPENSSL_NO_RSA */

# if PEDANTIC
static void *dummy = &dummy;
# endif

#endif
