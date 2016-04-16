/* v3_ia5.c */
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
#include <openssl/asn1.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>

static char *i2s_ASN1_IA5STRING(X509V3_EXT_METHOD *method,
                                ASN1_IA5STRING *ia5);
static ASN1_IA5STRING *s2i_ASN1_IA5STRING(X509V3_EXT_METHOD *method,
                                          X509V3_CTX *ctx, char *str);
const X509V3_EXT_METHOD v3_ns_ia5_list[] = {
    EXT_IA5STRING(NID_netscape_base_url),
    EXT_IA5STRING(NID_netscape_revocation_url),
    EXT_IA5STRING(NID_netscape_ca_revocation_url),
    EXT_IA5STRING(NID_netscape_renewal_url),
    EXT_IA5STRING(NID_netscape_ca_policy_url),
    EXT_IA5STRING(NID_netscape_ssl_server_name),
    EXT_IA5STRING(NID_netscape_comment),
    EXT_END
};

static char *i2s_ASN1_IA5STRING(X509V3_EXT_METHOD *method,
                                ASN1_IA5STRING *ia5)
{
    char *tmp;
    if (!ia5 || !ia5->length)
        return NULL;
    if (!(tmp = OPENSSL_malloc(ia5->length + 1))) {
        X509V3err(X509V3_F_I2S_ASN1_IA5STRING, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    memcpy(tmp, ia5->data, ia5->length);
    tmp[ia5->length] = 0;
    return tmp;
}

static ASN1_IA5STRING *s2i_ASN1_IA5STRING(X509V3_EXT_METHOD *method,
                                          X509V3_CTX *ctx, char *str)
{
    ASN1_IA5STRING *ia5;
    if (!str) {
        X509V3err(X509V3_F_S2I_ASN1_IA5STRING,
                  X509V3_R_INVALID_NULL_ARGUMENT);
        return NULL;
    }
    if (!(ia5 = M_ASN1_IA5STRING_new()))
        goto err;
    if (!ASN1_STRING_set((ASN1_STRING *)ia5, (unsigned char *)str,
                         strlen(str))) {
        M_ASN1_IA5STRING_free(ia5);
        goto err;
    }
#ifdef CHARSET_EBCDIC
    ebcdic2ascii(ia5->data, ia5->data, ia5->length);
#endif                          /* CHARSET_EBCDIC */
    return ia5;
 err:
    X509V3err(X509V3_F_S2I_ASN1_IA5STRING, ERR_R_MALLOC_FAILURE);
    return NULL;
}
