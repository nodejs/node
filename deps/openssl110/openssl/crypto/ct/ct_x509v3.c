/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifdef OPENSSL_NO_CT
# error "CT is disabled"
#endif

#include "ct_locl.h"

static char *i2s_poison(const X509V3_EXT_METHOD *method, void *val)
{
    return OPENSSL_strdup("NULL");
}

static void *s2i_poison(const X509V3_EXT_METHOD *method, X509V3_CTX *ctx, const char *str)
{
   return ASN1_NULL_new();
}

static int i2r_SCT_LIST(X509V3_EXT_METHOD *method, STACK_OF(SCT) *sct_list,
                 BIO *out, int indent)
{
    SCT_LIST_print(sct_list, out, indent, "\n", NULL);
    return 1;
}

/* Handlers for X509v3/OCSP Certificate Transparency extensions */
const X509V3_EXT_METHOD v3_ct_scts[3] = {
    /* X509v3 extension in certificates that contains SCTs */
    { NID_ct_precert_scts, 0, NULL,
    NULL, (X509V3_EXT_FREE)SCT_LIST_free,
    (X509V3_EXT_D2I)d2i_SCT_LIST, (X509V3_EXT_I2D)i2d_SCT_LIST,
    NULL, NULL,
    NULL, NULL,
    (X509V3_EXT_I2R)i2r_SCT_LIST, NULL,
    NULL },

    /* X509v3 extension to mark a certificate as a pre-certificate */
    { NID_ct_precert_poison, 0, ASN1_ITEM_ref(ASN1_NULL),
    NULL, NULL, NULL, NULL,
    i2s_poison, s2i_poison,
    NULL, NULL,
    NULL, NULL,
    NULL },

    /* OCSP extension that contains SCTs */
    { NID_ct_cert_scts, 0, NULL,
    0, (X509V3_EXT_FREE)SCT_LIST_free,
    (X509V3_EXT_D2I)d2i_SCT_LIST, (X509V3_EXT_I2D)i2d_SCT_LIST,
    NULL, NULL,
    NULL, NULL,
    (X509V3_EXT_I2R)i2r_SCT_LIST, NULL,
    NULL },
};
