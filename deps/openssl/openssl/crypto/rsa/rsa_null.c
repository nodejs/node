/*
 * Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/bn.h>
#include "rsa_locl.h"

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

static int RSA_null_init(RSA *rsa)
{
    return (1);
}

static int RSA_null_finish(RSA *rsa)
{
    return (1);
}
