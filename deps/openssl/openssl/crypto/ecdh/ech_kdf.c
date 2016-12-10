/* crypto/ecdh/ec_kdf.c */
/*
 * Written by Stephen Henson for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 2013 The OpenSSL Project.  All rights reserved.
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
 */

#define OPENSSL_FIPSAPI

#include <string.h>
#include <openssl/ecdh.h>
#include <openssl/evp.h>

/* Key derivation function from X9.62/SECG */
/* Way more than we will ever need */
#define ECDH_KDF_MAX    (1 << 30)

int ECDH_KDF_X9_62(unsigned char *out, size_t outlen,
                   const unsigned char *Z, size_t Zlen,
                   const unsigned char *sinfo, size_t sinfolen,
                   const EVP_MD *md)
{
    EVP_MD_CTX mctx;
    int rv = 0;
    unsigned int i;
    size_t mdlen;
    unsigned char ctr[4];
    if (sinfolen > ECDH_KDF_MAX || outlen > ECDH_KDF_MAX
        || Zlen > ECDH_KDF_MAX)
        return 0;
    mdlen = EVP_MD_size(md);
    EVP_MD_CTX_init(&mctx);
    for (i = 1;; i++) {
        unsigned char mtmp[EVP_MAX_MD_SIZE];
        EVP_DigestInit_ex(&mctx, md, NULL);
        ctr[3] = i & 0xFF;
        ctr[2] = (i >> 8) & 0xFF;
        ctr[1] = (i >> 16) & 0xFF;
        ctr[0] = (i >> 24) & 0xFF;
        if (!EVP_DigestUpdate(&mctx, Z, Zlen))
            goto err;
        if (!EVP_DigestUpdate(&mctx, ctr, sizeof(ctr)))
            goto err;
        if (!EVP_DigestUpdate(&mctx, sinfo, sinfolen))
            goto err;
        if (outlen >= mdlen) {
            if (!EVP_DigestFinal(&mctx, out, NULL))
                goto err;
            outlen -= mdlen;
            if (outlen == 0)
                break;
            out += mdlen;
        } else {
            if (!EVP_DigestFinal(&mctx, mtmp, NULL))
                goto err;
            memcpy(out, mtmp, outlen);
            OPENSSL_cleanse(mtmp, mdlen);
            break;
        }
    }
    rv = 1;
 err:
    EVP_MD_CTX_cleanup(&mctx);
    return rv;
}
