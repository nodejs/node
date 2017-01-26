/* crypto/asn1/x_crl.c */
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
#include <openssl/asn1t.h>
#include "asn1_locl.h"
#include <openssl/x509.h>
#include <openssl/x509v3.h>

static int X509_REVOKED_cmp(const X509_REVOKED *const *a,
                            const X509_REVOKED *const *b);
static void setup_idp(X509_CRL *crl, ISSUING_DIST_POINT *idp);

ASN1_SEQUENCE(X509_REVOKED) = {
        ASN1_SIMPLE(X509_REVOKED,serialNumber, ASN1_INTEGER),
        ASN1_SIMPLE(X509_REVOKED,revocationDate, ASN1_TIME),
        ASN1_SEQUENCE_OF_OPT(X509_REVOKED,extensions, X509_EXTENSION)
} ASN1_SEQUENCE_END(X509_REVOKED)

static int def_crl_verify(X509_CRL *crl, EVP_PKEY *r);
static int def_crl_lookup(X509_CRL *crl,
                          X509_REVOKED **ret, ASN1_INTEGER *serial,
                          X509_NAME *issuer);

static X509_CRL_METHOD int_crl_meth = {
    0,
    0, 0,
    def_crl_lookup,
    def_crl_verify
};

static const X509_CRL_METHOD *default_crl_method = &int_crl_meth;

/*
 * The X509_CRL_INFO structure needs a bit of customisation. Since we cache
 * the original encoding the signature wont be affected by reordering of the
 * revoked field.
 */
static int crl_inf_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
                      void *exarg)
{
    X509_CRL_INFO *a = (X509_CRL_INFO *)*pval;

    if (!a || !a->revoked)
        return 1;
    switch (operation) {
        /*
         * Just set cmp function here. We don't sort because that would
         * affect the output of X509_CRL_print().
         */
    case ASN1_OP_D2I_POST:
        (void)sk_X509_REVOKED_set_cmp_func(a->revoked, X509_REVOKED_cmp);
        break;
    }
    return 1;
}


ASN1_SEQUENCE_enc(X509_CRL_INFO, enc, crl_inf_cb) = {
        ASN1_OPT(X509_CRL_INFO, version, ASN1_INTEGER),
        ASN1_SIMPLE(X509_CRL_INFO, sig_alg, X509_ALGOR),
        ASN1_SIMPLE(X509_CRL_INFO, issuer, X509_NAME),
        ASN1_SIMPLE(X509_CRL_INFO, lastUpdate, ASN1_TIME),
        ASN1_OPT(X509_CRL_INFO, nextUpdate, ASN1_TIME),
        ASN1_SEQUENCE_OF_OPT(X509_CRL_INFO, revoked, X509_REVOKED),
        ASN1_EXP_SEQUENCE_OF_OPT(X509_CRL_INFO, extensions, X509_EXTENSION, 0)
} ASN1_SEQUENCE_END_enc(X509_CRL_INFO, X509_CRL_INFO)

/*
 * Set CRL entry issuer according to CRL certificate issuer extension. Check
 * for unhandled critical CRL entry extensions.
 */

static int crl_set_issuers(X509_CRL *crl)
{

    int i, j;
    GENERAL_NAMES *gens, *gtmp;
    STACK_OF(X509_REVOKED) *revoked;

    revoked = X509_CRL_get_REVOKED(crl);

    gens = NULL;
    for (i = 0; i < sk_X509_REVOKED_num(revoked); i++) {
        X509_REVOKED *rev = sk_X509_REVOKED_value(revoked, i);
        STACK_OF(X509_EXTENSION) *exts;
        ASN1_ENUMERATED *reason;
        X509_EXTENSION *ext;
        gtmp = X509_REVOKED_get_ext_d2i(rev,
                                        NID_certificate_issuer, &j, NULL);
        if (!gtmp && (j != -1)) {
            crl->flags |= EXFLAG_INVALID;
            return 1;
        }

        if (gtmp) {
            gens = gtmp;
            if (!crl->issuers) {
                crl->issuers = sk_GENERAL_NAMES_new_null();
                if (!crl->issuers)
                    return 0;
            }
            if (!sk_GENERAL_NAMES_push(crl->issuers, gtmp))
                return 0;
        }
        rev->issuer = gens;

        reason = X509_REVOKED_get_ext_d2i(rev, NID_crl_reason, &j, NULL);
        if (!reason && (j != -1)) {
            crl->flags |= EXFLAG_INVALID;
            return 1;
        }

        if (reason) {
            rev->reason = ASN1_ENUMERATED_get(reason);
            ASN1_ENUMERATED_free(reason);
        } else
            rev->reason = CRL_REASON_NONE;

        /* Check for critical CRL entry extensions */

        exts = rev->extensions;

        for (j = 0; j < sk_X509_EXTENSION_num(exts); j++) {
            ext = sk_X509_EXTENSION_value(exts, j);
            if (ext->critical > 0) {
                if (OBJ_obj2nid(ext->object) == NID_certificate_issuer)
                    continue;
                crl->flags |= EXFLAG_CRITICAL;
                break;
            }
        }

    }

    return 1;

}

/*
 * The X509_CRL structure needs a bit of customisation. Cache some extensions
 * and hash of the whole CRL.
 */
static int crl_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
                  void *exarg)
{
    X509_CRL *crl = (X509_CRL *)*pval;
    STACK_OF(X509_EXTENSION) *exts;
    X509_EXTENSION *ext;
    int idx;

    switch (operation) {
    case ASN1_OP_NEW_POST:
        crl->idp = NULL;
        crl->akid = NULL;
        crl->flags = 0;
        crl->idp_flags = 0;
        crl->idp_reasons = CRLDP_ALL_REASONS;
        crl->meth = default_crl_method;
        crl->meth_data = NULL;
        crl->issuers = NULL;
        crl->crl_number = NULL;
        crl->base_crl_number = NULL;
        break;

    case ASN1_OP_D2I_POST:
#ifndef OPENSSL_NO_SHA
        X509_CRL_digest(crl, EVP_sha1(), crl->sha1_hash, NULL);
#endif
        crl->idp = X509_CRL_get_ext_d2i(crl,
                                        NID_issuing_distribution_point, NULL,
                                        NULL);
        if (crl->idp)
            setup_idp(crl, crl->idp);

        crl->akid = X509_CRL_get_ext_d2i(crl,
                                         NID_authority_key_identifier, NULL,
                                         NULL);

        crl->crl_number = X509_CRL_get_ext_d2i(crl,
                                               NID_crl_number, NULL, NULL);

        crl->base_crl_number = X509_CRL_get_ext_d2i(crl,
                                                    NID_delta_crl, NULL,
                                                    NULL);
        /* Delta CRLs must have CRL number */
        if (crl->base_crl_number && !crl->crl_number)
            crl->flags |= EXFLAG_INVALID;

        /*
         * See if we have any unhandled critical CRL extensions and indicate
         * this in a flag. We only currently handle IDP so anything else
         * critical sets the flag. This code accesses the X509_CRL structure
         * directly: applications shouldn't do this.
         */

        exts = crl->crl->extensions;

        for (idx = 0; idx < sk_X509_EXTENSION_num(exts); idx++) {
            int nid;

            ext = sk_X509_EXTENSION_value(exts, idx);
            nid = OBJ_obj2nid(ext->object);
            if (nid == NID_freshest_crl)
                crl->flags |= EXFLAG_FRESHEST;
            if (ext->critical > 0) {
                /* We handle IDP and deltas */
                if ((nid == NID_issuing_distribution_point)
                    || (nid == NID_authority_key_identifier)
                    || (nid == NID_delta_crl))
                    continue;
                crl->flags |= EXFLAG_CRITICAL;
                break;
            }
        }

        if (!crl_set_issuers(crl))
            return 0;

        if (crl->meth->crl_init) {
            if (crl->meth->crl_init(crl) == 0)
                return 0;
        }
        break;

    case ASN1_OP_FREE_POST:
        if (crl->meth->crl_free) {
            if (!crl->meth->crl_free(crl))
                return 0;
        }
        if (crl->akid)
            AUTHORITY_KEYID_free(crl->akid);
        if (crl->idp)
            ISSUING_DIST_POINT_free(crl->idp);
        ASN1_INTEGER_free(crl->crl_number);
        ASN1_INTEGER_free(crl->base_crl_number);
        sk_GENERAL_NAMES_pop_free(crl->issuers, GENERAL_NAMES_free);
        break;
    }
    return 1;
}

/* Convert IDP into a more convenient form */

static void setup_idp(X509_CRL *crl, ISSUING_DIST_POINT *idp)
{
    int idp_only = 0;
    /* Set various flags according to IDP */
    crl->idp_flags |= IDP_PRESENT;
    if (idp->onlyuser > 0) {
        idp_only++;
        crl->idp_flags |= IDP_ONLYUSER;
    }
    if (idp->onlyCA > 0) {
        idp_only++;
        crl->idp_flags |= IDP_ONLYCA;
    }
    if (idp->onlyattr > 0) {
        idp_only++;
        crl->idp_flags |= IDP_ONLYATTR;
    }

    if (idp_only > 1)
        crl->idp_flags |= IDP_INVALID;

    if (idp->indirectCRL > 0)
        crl->idp_flags |= IDP_INDIRECT;

    if (idp->onlysomereasons) {
        crl->idp_flags |= IDP_REASONS;
        if (idp->onlysomereasons->length > 0)
            crl->idp_reasons = idp->onlysomereasons->data[0];
        if (idp->onlysomereasons->length > 1)
            crl->idp_reasons |= (idp->onlysomereasons->data[1] << 8);
        crl->idp_reasons &= CRLDP_ALL_REASONS;
    }

    DIST_POINT_set_dpname(idp->distpoint, X509_CRL_get_issuer(crl));
}

ASN1_SEQUENCE_ref(X509_CRL, crl_cb, CRYPTO_LOCK_X509_CRL) = {
        ASN1_SIMPLE(X509_CRL, crl, X509_CRL_INFO),
        ASN1_SIMPLE(X509_CRL, sig_alg, X509_ALGOR),
        ASN1_SIMPLE(X509_CRL, signature, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END_ref(X509_CRL, X509_CRL)

IMPLEMENT_ASN1_FUNCTIONS(X509_REVOKED)

IMPLEMENT_ASN1_DUP_FUNCTION(X509_REVOKED)

IMPLEMENT_ASN1_FUNCTIONS(X509_CRL_INFO)

IMPLEMENT_ASN1_FUNCTIONS(X509_CRL)

IMPLEMENT_ASN1_DUP_FUNCTION(X509_CRL)

static int X509_REVOKED_cmp(const X509_REVOKED *const *a,
                            const X509_REVOKED *const *b)
{
    return (ASN1_STRING_cmp((ASN1_STRING *)(*a)->serialNumber,
                            (ASN1_STRING *)(*b)->serialNumber));
}

int X509_CRL_add0_revoked(X509_CRL *crl, X509_REVOKED *rev)
{
    X509_CRL_INFO *inf;
    inf = crl->crl;
    if (!inf->revoked)
        inf->revoked = sk_X509_REVOKED_new(X509_REVOKED_cmp);
    if (!inf->revoked || !sk_X509_REVOKED_push(inf->revoked, rev)) {
        ASN1err(ASN1_F_X509_CRL_ADD0_REVOKED, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    inf->enc.modified = 1;
    return 1;
}

int X509_CRL_verify(X509_CRL *crl, EVP_PKEY *r)
{
    if (crl->meth->crl_verify)
        return crl->meth->crl_verify(crl, r);
    return 0;
}

int X509_CRL_get0_by_serial(X509_CRL *crl,
                            X509_REVOKED **ret, ASN1_INTEGER *serial)
{
    if (crl->meth->crl_lookup)
        return crl->meth->crl_lookup(crl, ret, serial, NULL);
    return 0;
}

int X509_CRL_get0_by_cert(X509_CRL *crl, X509_REVOKED **ret, X509 *x)
{
    if (crl->meth->crl_lookup)
        return crl->meth->crl_lookup(crl, ret,
                                     X509_get_serialNumber(x),
                                     X509_get_issuer_name(x));
    return 0;
}

static int def_crl_verify(X509_CRL *crl, EVP_PKEY *r)
{
    return (ASN1_item_verify(ASN1_ITEM_rptr(X509_CRL_INFO),
                             crl->sig_alg, crl->signature, crl->crl, r));
}

static int crl_revoked_issuer_match(X509_CRL *crl, X509_NAME *nm,
                                    X509_REVOKED *rev)
{
    int i;

    if (!rev->issuer) {
        if (!nm)
            return 1;
        if (!X509_NAME_cmp(nm, X509_CRL_get_issuer(crl)))
            return 1;
        return 0;
    }

    if (!nm)
        nm = X509_CRL_get_issuer(crl);

    for (i = 0; i < sk_GENERAL_NAME_num(rev->issuer); i++) {
        GENERAL_NAME *gen = sk_GENERAL_NAME_value(rev->issuer, i);
        if (gen->type != GEN_DIRNAME)
            continue;
        if (!X509_NAME_cmp(nm, gen->d.directoryName))
            return 1;
    }
    return 0;

}

static int def_crl_lookup(X509_CRL *crl,
                          X509_REVOKED **ret, ASN1_INTEGER *serial,
                          X509_NAME *issuer)
{
    X509_REVOKED rtmp, *rev;
    int idx;
    rtmp.serialNumber = serial;
    /*
     * Sort revoked into serial number order if not already sorted. Do this
     * under a lock to avoid race condition.
     */
    if (!sk_X509_REVOKED_is_sorted(crl->crl->revoked)) {
        CRYPTO_w_lock(CRYPTO_LOCK_X509_CRL);
        sk_X509_REVOKED_sort(crl->crl->revoked);
        CRYPTO_w_unlock(CRYPTO_LOCK_X509_CRL);
    }
    idx = sk_X509_REVOKED_find(crl->crl->revoked, &rtmp);
    if (idx < 0)
        return 0;
    /* Need to look for matching name */
    for (; idx < sk_X509_REVOKED_num(crl->crl->revoked); idx++) {
        rev = sk_X509_REVOKED_value(crl->crl->revoked, idx);
        if (ASN1_INTEGER_cmp(rev->serialNumber, serial))
            return 0;
        if (crl_revoked_issuer_match(crl, issuer, rev)) {
            if (ret)
                *ret = rev;
            if (rev->reason == CRL_REASON_REMOVE_FROM_CRL)
                return 2;
            return 1;
        }
    }
    return 0;
}

void X509_CRL_set_default_method(const X509_CRL_METHOD *meth)
{
    if (meth == NULL)
        default_crl_method = &int_crl_meth;
    else
        default_crl_method = meth;
}

X509_CRL_METHOD *X509_CRL_METHOD_new(int (*crl_init) (X509_CRL *crl),
                                     int (*crl_free) (X509_CRL *crl),
                                     int (*crl_lookup) (X509_CRL *crl,
                                                        X509_REVOKED **ret,
                                                        ASN1_INTEGER *ser,
                                                        X509_NAME *issuer),
                                     int (*crl_verify) (X509_CRL *crl,
                                                        EVP_PKEY *pk))
{
    X509_CRL_METHOD *m;
    m = OPENSSL_malloc(sizeof(X509_CRL_METHOD));
    if (!m)
        return NULL;
    m->crl_init = crl_init;
    m->crl_free = crl_free;
    m->crl_lookup = crl_lookup;
    m->crl_verify = crl_verify;
    m->flags = X509_CRL_METHOD_DYNAMIC;
    return m;
}

void X509_CRL_METHOD_free(X509_CRL_METHOD *m)
{
    if (!(m->flags & X509_CRL_METHOD_DYNAMIC))
        return;
    OPENSSL_free(m);
}

void X509_CRL_set_meth_data(X509_CRL *crl, void *dat)
{
    crl->meth_data = dat;
}

void *X509_CRL_get_meth_data(X509_CRL *crl)
{
    return crl->meth_data;
}

IMPLEMENT_STACK_OF(X509_REVOKED)

IMPLEMENT_ASN1_SET_OF(X509_REVOKED)

IMPLEMENT_STACK_OF(X509_CRL)

IMPLEMENT_ASN1_SET_OF(X509_CRL)
