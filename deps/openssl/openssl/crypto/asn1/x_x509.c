/* crypto/asn1/x_x509.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/evp.h>
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

ASN1_SEQUENCE_enc(X509_CINF, enc, 0) = {
        ASN1_EXP_OPT(X509_CINF, version, ASN1_INTEGER, 0),
        ASN1_SIMPLE(X509_CINF, serialNumber, ASN1_INTEGER),
        ASN1_SIMPLE(X509_CINF, signature, X509_ALGOR),
        ASN1_SIMPLE(X509_CINF, issuer, X509_NAME),
        ASN1_SIMPLE(X509_CINF, validity, X509_VAL),
        ASN1_SIMPLE(X509_CINF, subject, X509_NAME),
        ASN1_SIMPLE(X509_CINF, key, X509_PUBKEY),
        ASN1_IMP_OPT(X509_CINF, issuerUID, ASN1_BIT_STRING, 1),
        ASN1_IMP_OPT(X509_CINF, subjectUID, ASN1_BIT_STRING, 2),
        ASN1_EXP_SEQUENCE_OF_OPT(X509_CINF, extensions, X509_EXTENSION, 3)
} ASN1_SEQUENCE_END_enc(X509_CINF, X509_CINF)

IMPLEMENT_ASN1_FUNCTIONS(X509_CINF)
/* X509 top level structure needs a bit of customisation */

extern void policy_cache_free(X509_POLICY_CACHE *cache);

static int x509_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
                   void *exarg)
{
    X509 *ret = (X509 *)*pval;

    switch (operation) {

    case ASN1_OP_NEW_POST:
        ret->valid = 0;
        ret->name = NULL;
        ret->ex_flags = 0;
        ret->ex_pathlen = -1;
        ret->skid = NULL;
        ret->akid = NULL;
#ifndef OPENSSL_NO_RFC3779
        ret->rfc3779_addr = NULL;
        ret->rfc3779_asid = NULL;
#endif
        ret->aux = NULL;
        ret->crldp = NULL;
        CRYPTO_new_ex_data(CRYPTO_EX_INDEX_X509, ret, &ret->ex_data);
        break;

    case ASN1_OP_D2I_POST:
        if (ret->name != NULL)
            OPENSSL_free(ret->name);
        ret->name = X509_NAME_oneline(ret->cert_info->subject, NULL, 0);
        break;

    case ASN1_OP_FREE_POST:
        CRYPTO_free_ex_data(CRYPTO_EX_INDEX_X509, ret, &ret->ex_data);
        X509_CERT_AUX_free(ret->aux);
        ASN1_OCTET_STRING_free(ret->skid);
        AUTHORITY_KEYID_free(ret->akid);
        CRL_DIST_POINTS_free(ret->crldp);
        policy_cache_free(ret->policy_cache);
        GENERAL_NAMES_free(ret->altname);
        NAME_CONSTRAINTS_free(ret->nc);
#ifndef OPENSSL_NO_RFC3779
        sk_IPAddressFamily_pop_free(ret->rfc3779_addr, IPAddressFamily_free);
        ASIdentifiers_free(ret->rfc3779_asid);
#endif

        if (ret->name != NULL)
            OPENSSL_free(ret->name);
        break;

    }

    return 1;

}

ASN1_SEQUENCE_ref(X509, x509_cb, CRYPTO_LOCK_X509) = {
        ASN1_SIMPLE(X509, cert_info, X509_CINF),
        ASN1_SIMPLE(X509, sig_alg, X509_ALGOR),
        ASN1_SIMPLE(X509, signature, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END_ref(X509, X509)

IMPLEMENT_ASN1_FUNCTIONS(X509)

IMPLEMENT_ASN1_DUP_FUNCTION(X509)

int X509_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
                          CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func)
{
    return CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_X509, argl, argp,
                                   new_func, dup_func, free_func);
}

int X509_set_ex_data(X509 *r, int idx, void *arg)
{
    return (CRYPTO_set_ex_data(&r->ex_data, idx, arg));
}

void *X509_get_ex_data(X509 *r, int idx)
{
    return (CRYPTO_get_ex_data(&r->ex_data, idx));
}

/*
 * X509_AUX ASN1 routines. X509_AUX is the name given to a certificate with
 * extra info tagged on the end. Since these functions set how a certificate
 * is trusted they should only be used when the certificate comes from a
 * reliable source such as local storage.
 */

X509 *d2i_X509_AUX(X509 **a, const unsigned char **pp, long length)
{
    const unsigned char *q;
    X509 *ret;
    int freeret = 0;

    /* Save start position */
    q = *pp;

    if (!a || *a == NULL) {
        freeret = 1;
    }
    ret = d2i_X509(a, &q, length);
    /* If certificate unreadable then forget it */
    if (!ret)
        return NULL;
    /* update length */
    length -= q - *pp;
    if (length > 0 && !d2i_X509_CERT_AUX(&ret->aux, &q, length))
        goto err;
    *pp = q;
    return ret;
 err:
    if (freeret) {
        X509_free(ret);
        if (a)
            *a = NULL;
    }
    return NULL;
}

int i2d_X509_AUX(X509 *a, unsigned char **pp)
{
    int length, tmplen;
    unsigned char *start = pp != NULL ? *pp : NULL;
    length = i2d_X509(a, pp);
    if (length < 0 || a == NULL)
        return length;

    tmplen = i2d_X509_CERT_AUX(a->aux, pp);
    if (tmplen < 0) {
        if (start != NULL)
            *pp = start;
        return tmplen;
    }
    length += tmplen;

    return length;
}

int i2d_re_X509_tbs(X509 *x, unsigned char **pp)
{
    x->cert_info->enc.modified = 1;
    return i2d_X509_CINF(x->cert_info, pp);
}

void X509_get0_signature(ASN1_BIT_STRING **psig, X509_ALGOR **palg,
                         const X509 *x)
{
    if (psig)
        *psig = x->signature;
    if (palg)
        *palg = x->sig_alg;
}

int X509_get_signature_nid(const X509 *x)
{
    return OBJ_obj2nid(x->sig_alg->algorithm);
}
