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
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include "crypto/x509.h"
#include <openssl/x509v3.h>

int X509_CRL_get_ext_count(const X509_CRL *x)
{
    return X509v3_get_ext_count(x->crl.extensions);
}

int X509_CRL_get_ext_by_NID(const X509_CRL *x, int nid, int lastpos)
{
    return X509v3_get_ext_by_NID(x->crl.extensions, nid, lastpos);
}

int X509_CRL_get_ext_by_OBJ(const X509_CRL *x, const ASN1_OBJECT *obj,
    int lastpos)
{
    return X509v3_get_ext_by_OBJ(x->crl.extensions, obj, lastpos);
}

int X509_CRL_get_ext_by_critical(const X509_CRL *x, int crit, int lastpos)
{
    return X509v3_get_ext_by_critical(x->crl.extensions, crit, lastpos);
}

const X509_EXTENSION *X509_CRL_get_ext(const X509_CRL *x, int loc)
{
    return X509v3_get_ext(x->crl.extensions, loc);
}

X509_EXTENSION *X509_CRL_delete_ext(X509_CRL *x, int loc)
{
    X509_EXTENSION *ret = X509v3_delete_extension(&x->crl.extensions, loc);

    if (ret != NULL)
        x->crl.enc.modified = 1;
    return ret;
}

void *X509_CRL_get_ext_d2i(const X509_CRL *x, int nid, int *crit, int *idx)
{
    return X509V3_get_d2i(x->crl.extensions, nid, crit, idx);
}

int X509_CRL_add1_ext_i2d(X509_CRL *x, int nid, void *value, int crit,
    unsigned long flags)
{
    return X509V3_add1_i2d(&x->crl.extensions, nid, value, crit, flags);
}

int X509_CRL_add_ext(X509_CRL *x, const X509_EXTENSION *ex, int loc)
{
    return (X509v3_add_ext(&(x->crl.extensions), ex, loc) != NULL);
}

int X509_get_ext_count(const X509 *x)
{
    return X509v3_get_ext_count(x->cert_info.extensions);
}

int X509_get_ext_by_NID(const X509 *x, int nid, int lastpos)
{
    return X509v3_get_ext_by_NID(x->cert_info.extensions, nid, lastpos);
}

int X509_get_ext_by_OBJ(const X509 *x, const ASN1_OBJECT *obj, int lastpos)
{
    return X509v3_get_ext_by_OBJ(x->cert_info.extensions, obj, lastpos);
}

int X509_get_ext_by_critical(const X509 *x, int crit, int lastpos)
{
    return X509v3_get_ext_by_critical(x->cert_info.extensions, crit, lastpos);
}

const X509_EXTENSION *X509_get_ext(const X509 *x, int loc)
{
    return X509v3_get_ext(x->cert_info.extensions, loc);
}

X509_EXTENSION *X509_delete_ext(X509 *x, int loc)
{
    X509_EXTENSION *ret;

    ret = X509v3_delete_extension(&x->cert_info.extensions, loc);
    if (ret != NULL)
        x->cert_info.enc.modified = 1;
    return ret;
}

int X509_add_ext(X509 *x, const X509_EXTENSION *ex, int loc)
{
    STACK_OF(X509_EXTENSION) **exts = &x->cert_info.extensions;

    /* x->cert_info.extensions might initially be NULL */
    if (X509v3_add_ext(exts, ex, loc) == NULL)
        return 0;
    /*
     * An ignored "empty" SKID or AKID extension will appear to be successfully
     * added, even though nothing is pushed onto the resulting stack.  However,
     * if the stack was initially NULL or empty, it will now be non-NULL, but
     * empty, deallocate and make it NULL in that case.
     */
    if (sk_X509_EXTENSION_num(*exts) == 0) {
        sk_X509_EXTENSION_free(*exts);
        *exts = NULL;
    }
    x->cert_info.enc.modified = 1;
    return 1;
}

void *X509_get_ext_d2i(const X509 *x, int nid, int *crit, int *idx)
{
    return X509V3_get_d2i(x->cert_info.extensions, nid, crit, idx);
}

int X509_add1_ext_i2d(X509 *x, int nid, void *value, int crit,
    unsigned long flags)
{
    /*
     * Assume modified, sadly the underlying function does not tell us whether
     * changes were made, or not.
     */
    x->cert_info.enc.modified = 1;
    return X509V3_add1_i2d(&x->cert_info.extensions, nid, value, crit,
        flags);
}

int X509_REVOKED_get_ext_count(const X509_REVOKED *x)
{
    return X509v3_get_ext_count(x->extensions);
}

int X509_REVOKED_get_ext_by_NID(const X509_REVOKED *x, int nid, int lastpos)
{
    return X509v3_get_ext_by_NID(x->extensions, nid, lastpos);
}

int X509_REVOKED_get_ext_by_OBJ(const X509_REVOKED *x, const ASN1_OBJECT *obj,
    int lastpos)
{
    return X509v3_get_ext_by_OBJ(x->extensions, obj, lastpos);
}

int X509_REVOKED_get_ext_by_critical(const X509_REVOKED *x, int crit, int lastpos)
{
    return X509v3_get_ext_by_critical(x->extensions, crit, lastpos);
}

const X509_EXTENSION *X509_REVOKED_get_ext(const X509_REVOKED *x, int loc)
{
    return X509v3_get_ext(x->extensions, loc);
}

X509_EXTENSION *X509_REVOKED_delete_ext(X509_REVOKED *x, int loc)
{
    return X509v3_delete_extension(&x->extensions, loc);
}

int X509_REVOKED_add_ext(X509_REVOKED *x, X509_EXTENSION *ex, int loc)
{
    return (X509v3_add_ext(&(x->extensions), ex, loc) != NULL);
}

void *X509_REVOKED_get_ext_d2i(const X509_REVOKED *x, int nid, int *crit, int *idx)
{
    return X509V3_get_d2i(x->extensions, nid, crit, idx);
}

int X509_REVOKED_add1_ext_i2d(X509_REVOKED *x, int nid, void *value, int crit,
    unsigned long flags)
{
    return X509V3_add1_i2d(&x->extensions, nid, value, crit, flags);
}
