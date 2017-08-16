/* crypto/x509/x509cset.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2001.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
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
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

int X509_CRL_set_version(X509_CRL *x, long version)
{
    if (x == NULL)
        return (0);
    if (x->crl->version == NULL) {
        if ((x->crl->version = M_ASN1_INTEGER_new()) == NULL)
            return (0);
    }
    return (ASN1_INTEGER_set(x->crl->version, version));
}

int X509_CRL_set_issuer_name(X509_CRL *x, X509_NAME *name)
{
    if ((x == NULL) || (x->crl == NULL))
        return (0);
    return (X509_NAME_set(&x->crl->issuer, name));
}

int X509_CRL_set_lastUpdate(X509_CRL *x, const ASN1_TIME *tm)
{
    ASN1_TIME *in;

    if (x == NULL)
        return (0);
    in = x->crl->lastUpdate;
    if (in != tm) {
        in = M_ASN1_TIME_dup(tm);
        if (in != NULL) {
            M_ASN1_TIME_free(x->crl->lastUpdate);
            x->crl->lastUpdate = in;
        }
    }
    return (in != NULL);
}

int X509_CRL_set_nextUpdate(X509_CRL *x, const ASN1_TIME *tm)
{
    ASN1_TIME *in;

    if (x == NULL)
        return (0);
    in = x->crl->nextUpdate;
    if (in != tm) {
        in = M_ASN1_TIME_dup(tm);
        if (in != NULL) {
            M_ASN1_TIME_free(x->crl->nextUpdate);
            x->crl->nextUpdate = in;
        }
    }
    return (in != NULL);
}

int X509_CRL_sort(X509_CRL *c)
{
    int i;
    X509_REVOKED *r;
    /*
     * sort the data so it will be written in serial number order
     */
    sk_X509_REVOKED_sort(c->crl->revoked);
    for (i = 0; i < sk_X509_REVOKED_num(c->crl->revoked); i++) {
        r = sk_X509_REVOKED_value(c->crl->revoked, i);
        r->sequence = i;
    }
    c->crl->enc.modified = 1;
    return 1;
}

int X509_REVOKED_set_revocationDate(X509_REVOKED *x, ASN1_TIME *tm)
{
    ASN1_TIME *in;

    if (x == NULL)
        return (0);
    in = x->revocationDate;
    if (in != tm) {
        in = M_ASN1_TIME_dup(tm);
        if (in != NULL) {
            M_ASN1_TIME_free(x->revocationDate);
            x->revocationDate = in;
        }
    }
    return (in != NULL);
}

int X509_REVOKED_set_serialNumber(X509_REVOKED *x, ASN1_INTEGER *serial)
{
    ASN1_INTEGER *in;

    if (x == NULL)
        return (0);
    in = x->serialNumber;
    if (in != serial) {
        in = M_ASN1_INTEGER_dup(serial);
        if (in != NULL) {
            M_ASN1_INTEGER_free(x->serialNumber);
            x->serialNumber = in;
        }
    }
    return (in != NULL);
}
