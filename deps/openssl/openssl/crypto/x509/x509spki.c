/* x509spki.c */
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
#include <openssl/x509.h>

int NETSCAPE_SPKI_set_pubkey(NETSCAPE_SPKI *x, EVP_PKEY *pkey)
{
    if ((x == NULL) || (x->spkac == NULL))
        return (0);
    return (X509_PUBKEY_set(&(x->spkac->pubkey), pkey));
}

EVP_PKEY *NETSCAPE_SPKI_get_pubkey(NETSCAPE_SPKI *x)
{
    if ((x == NULL) || (x->spkac == NULL))
        return (NULL);
    return (X509_PUBKEY_get(x->spkac->pubkey));
}

/* Load a Netscape SPKI from a base64 encoded string */

NETSCAPE_SPKI *NETSCAPE_SPKI_b64_decode(const char *str, int len)
{
    unsigned char *spki_der;
    const unsigned char *p;
    int spki_len;
    NETSCAPE_SPKI *spki;
    if (len <= 0)
        len = strlen(str);
    if (!(spki_der = OPENSSL_malloc(len + 1))) {
        X509err(X509_F_NETSCAPE_SPKI_B64_DECODE, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    spki_len = EVP_DecodeBlock(spki_der, (const unsigned char *)str, len);
    if (spki_len < 0) {
        X509err(X509_F_NETSCAPE_SPKI_B64_DECODE, X509_R_BASE64_DECODE_ERROR);
        OPENSSL_free(spki_der);
        return NULL;
    }
    p = spki_der;
    spki = d2i_NETSCAPE_SPKI(NULL, &p, spki_len);
    OPENSSL_free(spki_der);
    return spki;
}

/* Generate a base64 encoded string from an SPKI */

char *NETSCAPE_SPKI_b64_encode(NETSCAPE_SPKI *spki)
{
    unsigned char *der_spki, *p;
    char *b64_str;
    int der_len;
    der_len = i2d_NETSCAPE_SPKI(spki, NULL);
    der_spki = OPENSSL_malloc(der_len);
    b64_str = OPENSSL_malloc(der_len * 2);
    if (!der_spki || !b64_str) {
        OPENSSL_free(der_spki);
        OPENSSL_free(b64_str);
        X509err(X509_F_NETSCAPE_SPKI_B64_ENCODE, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    p = der_spki;
    i2d_NETSCAPE_SPKI(spki, &p);
    EVP_EncodeBlock((unsigned char *)b64_str, der_spki, der_len);
    OPENSSL_free(der_spki);
    return b64_str;
}
