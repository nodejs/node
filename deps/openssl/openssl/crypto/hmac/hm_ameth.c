/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2007.
 */
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
#include <openssl/evp.h>
#include "asn1_locl.h"

#define HMAC_TEST_PRIVATE_KEY_FORMAT

/*
 * HMAC "ASN1" method. This is just here to indicate the maximum HMAC output
 * length and to free up an HMAC key.
 */

static int hmac_size(const EVP_PKEY *pkey)
{
    return EVP_MAX_MD_SIZE;
}

static void hmac_key_free(EVP_PKEY *pkey)
{
    ASN1_OCTET_STRING *os = (ASN1_OCTET_STRING *)pkey->pkey.ptr;
    if (os) {
        if (os->data)
            OPENSSL_cleanse(os->data, os->length);
        ASN1_OCTET_STRING_free(os);
    }
}

static int hmac_pkey_ctrl(EVP_PKEY *pkey, int op, long arg1, void *arg2)
{
    switch (op) {
    case ASN1_PKEY_CTRL_DEFAULT_MD_NID:
        *(int *)arg2 = NID_sha256;
        return 1;

    default:
        return -2;
    }
}

#ifdef HMAC_TEST_PRIVATE_KEY_FORMAT
/*
 * A bogus private key format for test purposes. This is simply the HMAC key
 * with "HMAC PRIVATE KEY" in the headers. When enabled the genpkey utility
 * can be used to "generate" HMAC keys.
 */

static int old_hmac_decode(EVP_PKEY *pkey,
                           const unsigned char **pder, int derlen)
{
    ASN1_OCTET_STRING *os;
    os = ASN1_OCTET_STRING_new();
    if (!os || !ASN1_OCTET_STRING_set(os, *pder, derlen))
        return 0;
    EVP_PKEY_assign(pkey, EVP_PKEY_HMAC, os);
    return 1;
}

static int old_hmac_encode(const EVP_PKEY *pkey, unsigned char **pder)
{
    int inc;
    ASN1_OCTET_STRING *os = (ASN1_OCTET_STRING *)pkey->pkey.ptr;
    if (pder) {
        if (!*pder) {
            *pder = OPENSSL_malloc(os->length);
            inc = 0;
        } else
            inc = 1;

        memcpy(*pder, os->data, os->length);

        if (inc)
            *pder += os->length;
    }

    return os->length;
}

#endif

const EVP_PKEY_ASN1_METHOD hmac_asn1_meth = {
    EVP_PKEY_HMAC,
    EVP_PKEY_HMAC,
    0,

    "HMAC",
    "OpenSSL HMAC method",

    0, 0, 0, 0,

    0, 0, 0,

    hmac_size,
    0,
    0, 0, 0, 0, 0, 0, 0,

    hmac_key_free,
    hmac_pkey_ctrl,
#ifdef HMAC_TEST_PRIVATE_KEY_FORMAT
    old_hmac_decode,
    old_hmac_encode
#else
    0, 0
#endif
};
