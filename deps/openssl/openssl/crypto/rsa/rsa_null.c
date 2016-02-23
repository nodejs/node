/* rsa_null.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>

/*
 * This is a dummy RSA implementation that just returns errors when called.
 * It is designed to allow some RSA functions to work while stopping those
 * covered by the RSA patent. That is RSA, encryption, decryption, signing
 * and verify is not allowed but RSA key generation, key checking and other
 * operations (like storing RSA keys) are permitted.
 */

static int RSA_null_public_encrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding);
static int RSA_null_private_encrypt(int flen, const unsigned char *from,
                                    unsigned char *to, RSA *rsa, int padding);
static int RSA_null_public_decrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding);
static int RSA_null_private_decrypt(int flen, const unsigned char *from,
                                    unsigned char *to, RSA *rsa, int padding);
#if 0                           /* not currently used */
static int RSA_null_mod_exp(const BIGNUM *r0, const BIGNUM *i, RSA *rsa);
#endif
static int RSA_null_init(RSA *rsa);
static int RSA_null_finish(RSA *rsa);
static RSA_METHOD rsa_null_meth = {
    "Null RSA",
    RSA_null_public_encrypt,
    RSA_null_public_decrypt,
    RSA_null_private_encrypt,
    RSA_null_private_decrypt,
    NULL,
    NULL,
    RSA_null_init,
    RSA_null_finish,
    0,
    NULL,
    NULL,
    NULL,
    NULL
};

const RSA_METHOD *RSA_null_method(void)
{
    return (&rsa_null_meth);
}

static int RSA_null_public_encrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding)
{
    RSAerr(RSA_F_RSA_NULL_PUBLIC_ENCRYPT, RSA_R_RSA_OPERATIONS_NOT_SUPPORTED);
    return -1;
}

static int RSA_null_private_encrypt(int flen, const unsigned char *from,
                                    unsigned char *to, RSA *rsa, int padding)
{
    RSAerr(RSA_F_RSA_NULL_PRIVATE_ENCRYPT,
           RSA_R_RSA_OPERATIONS_NOT_SUPPORTED);
    return -1;
}

static int RSA_null_private_decrypt(int flen, const unsigned char *from,
                                    unsigned char *to, RSA *rsa, int padding)
{
    RSAerr(RSA_F_RSA_NULL_PRIVATE_DECRYPT,
           RSA_R_RSA_OPERATIONS_NOT_SUPPORTED);
    return -1;
}

static int RSA_null_public_decrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding)
{
    RSAerr(RSA_F_RSA_NULL_PUBLIC_DECRYPT, RSA_R_RSA_OPERATIONS_NOT_SUPPORTED);
    return -1;
}

#if 0                           /* not currently used */
static int RSA_null_mod_exp(BIGNUM *r0, BIGNUM *I, RSA *rsa)
{
    ... err(RSA_F_RSA_NULL_MOD_EXP, RSA_R_RSA_OPERATIONS_NOT_SUPPORTED);
    return -1;
}
#endif

static int RSA_null_init(RSA *rsa)
{
    return (1);
}

static int RSA_null_finish(RSA *rsa)
{
    return (1);
}
