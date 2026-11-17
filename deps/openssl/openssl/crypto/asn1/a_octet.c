/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/asn1.h>
#include "crypto/asn1.h"

ASN1_OCTET_STRING *ASN1_OCTET_STRING_dup(const ASN1_OCTET_STRING *x)
{
    return ASN1_STRING_dup(x);
}

int ASN1_OCTET_STRING_cmp(const ASN1_OCTET_STRING *a,
    const ASN1_OCTET_STRING *b)
{
    return ASN1_STRING_cmp(a, b);
}

int ASN1_OCTET_STRING_set(ASN1_OCTET_STRING *x, const unsigned char *d,
    int len)
{
    if (len < -1) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_TOO_SMALL);
        return 0;
    }
    if (len == -1)
        return ossl_asn1_string_set1_string(x, (const char *)d);
    return ossl_asn1_string_set1_data(x, d, len);
}
