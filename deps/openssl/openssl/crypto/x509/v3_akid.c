/*
 * Copyright 1999-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/conf.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/x509v3.h>
#include "crypto/x509.h"
#include "ext_dat.h"

static STACK_OF(CONF_VALUE) *i2v_AUTHORITY_KEYID(X509V3_EXT_METHOD *method,
                                                 AUTHORITY_KEYID *akeyid,
                                                 STACK_OF(CONF_VALUE)
                                                 *extlist);
static AUTHORITY_KEYID *v2i_AUTHORITY_KEYID(X509V3_EXT_METHOD *method,
                                            X509V3_CTX *ctx,
                                            STACK_OF(CONF_VALUE) *values);

const X509V3_EXT_METHOD ossl_v3_akey_id = {
    NID_authority_key_identifier,
    X509V3_EXT_MULTILINE, ASN1_ITEM_ref(AUTHORITY_KEYID),
    0, 0, 0, 0,
    0, 0,
    (X509V3_EXT_I2V) i2v_AUTHORITY_KEYID,
    (X509V3_EXT_V2I)v2i_AUTHORITY_KEYID,
    0, 0,
    NULL
};

static STACK_OF(CONF_VALUE) *i2v_AUTHORITY_KEYID(X509V3_EXT_METHOD *method,
                                                 AUTHORITY_KEYID *akeyid,
                                                 STACK_OF(CONF_VALUE)
                                                 *extlist)
{
    char *tmp = NULL;
    STACK_OF(CONF_VALUE) *origextlist = extlist, *tmpextlist;

    if (akeyid->keyid) {
        tmp = OPENSSL_buf2hexstr(akeyid->keyid->data, akeyid->keyid->length);
        if (tmp == NULL) {
            ERR_raise(ERR_LIB_X509V3, ERR_R_MALLOC_FAILURE);
            return NULL;
        }
        if (!X509V3_add_value((akeyid->issuer || akeyid->serial) ? "keyid" : NULL,
                              tmp, &extlist)) {
            OPENSSL_free(tmp);
            ERR_raise(ERR_LIB_X509V3, ERR_R_X509_LIB);
            goto err;
        }
        OPENSSL_free(tmp);
    }
    if (akeyid->issuer) {
        tmpextlist = i2v_GENERAL_NAMES(NULL, akeyid->issuer, extlist);
        if (tmpextlist == NULL) {
            ERR_raise(ERR_LIB_X509V3, ERR_R_X509_LIB);
            goto err;
        }
        extlist = tmpextlist;
    }
    if (akeyid->serial) {
        tmp = OPENSSL_buf2hexstr(akeyid->serial->data, akeyid->serial->length);
        if (tmp == NULL) {
            ERR_raise(ERR_LIB_X509V3, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        if (!X509V3_add_value("serial", tmp, &extlist)) {
            OPENSSL_free(tmp);
            goto err;
        }
        OPENSSL_free(tmp);
    }
    return extlist;
 err:
    if (origextlist == NULL)
        sk_CONF_VALUE_pop_free(extlist, X509V3_conf_free);
    return NULL;
}

/*-
 * Currently two options:
 * keyid: use the issuers subject keyid, the value 'always' means its is
 * an error if the issuer certificate doesn't have a key id.
 * issuer: use the issuers cert issuer and serial number. The default is
 * to only use this if keyid is not present. With the option 'always'
 * this is always included.
 */

static AUTHORITY_KEYID *v2i_AUTHORITY_KEYID(X509V3_EXT_METHOD *method,
                                            X509V3_CTX *ctx,
                                            STACK_OF(CONF_VALUE) *values)
{
    char keyid = 0, issuer = 0;
    int i, n = sk_CONF_VALUE_num(values);
    CONF_VALUE *cnf;
    ASN1_OCTET_STRING *ikeyid = NULL;
    X509_NAME *isname = NULL;
    GENERAL_NAMES *gens = NULL;
    GENERAL_NAME *gen = NULL;
    ASN1_INTEGER *serial = NULL;
    X509_EXTENSION *ext;
    X509 *issuer_cert;
    AUTHORITY_KEYID *akeyid = AUTHORITY_KEYID_new();

    if (akeyid == NULL)
        goto err;

    if (n == 1 && strcmp(sk_CONF_VALUE_value(values, 0)->name, "none") == 0) {
        return akeyid;
    }

    for (i = 0; i < n; i++) {
        cnf = sk_CONF_VALUE_value(values, i);
        if (strcmp(cnf->name, "keyid") == 0) {
            keyid = 1;
            if (cnf->value && strcmp(cnf->value, "always") == 0)
                keyid = 2;
        } else if (strcmp(cnf->name, "issuer") == 0) {
            issuer = 1;
            if (cnf->value && strcmp(cnf->value, "always") == 0)
                issuer = 2;
        } else {
            ERR_raise_data(ERR_LIB_X509V3, X509V3_R_UNKNOWN_OPTION,
                           "name=%s", cnf->name);
            goto err;
        }
    }

    if (ctx != NULL && (ctx->flags & X509V3_CTX_TEST) != 0)
        return akeyid;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_X509V3, ERR_R_PASSED_NULL_PARAMETER);
        goto err;
    }
    if ((issuer_cert = ctx->issuer_cert) == NULL) {
        ERR_raise(ERR_LIB_X509V3, X509V3_R_NO_ISSUER_CERTIFICATE);
        goto err;
    }

    if (keyid != 0) {
        /* prefer any pre-existing subject key identifier of the issuer cert */
        i = X509_get_ext_by_NID(issuer_cert, NID_subject_key_identifier, -1);
        if (i >= 0 && (ext = X509_get_ext(issuer_cert, i)) != NULL)
            ikeyid = X509V3_EXT_d2i(ext);
        if (ikeyid == NULL && ctx->issuer_pkey != NULL) { /* fallback */
            /* generate AKID from scratch, emulating s2i_skey_id(..., "hash") */
            X509_PUBKEY *pubkey = NULL;

            if (X509_PUBKEY_set(&pubkey, ctx->issuer_pkey))
                ikeyid = ossl_x509_pubkey_hash(pubkey);
            X509_PUBKEY_free(pubkey);
        }
        if ((keyid == 2 || issuer == 0)
            && (ikeyid == NULL
                || ASN1_STRING_length(ikeyid) <= 2) /* indicating "none" */) {
            ERR_raise(ERR_LIB_X509V3, X509V3_R_UNABLE_TO_GET_ISSUER_KEYID);
            goto err;
        }
    }

    if (issuer == 2 || (issuer == 1 && ikeyid == NULL)) {
        isname = X509_NAME_dup(X509_get_issuer_name(issuer_cert));
        serial = ASN1_INTEGER_dup(X509_get0_serialNumber(issuer_cert));
        if (isname == NULL || serial == NULL) {
            ERR_raise(ERR_LIB_X509V3, X509V3_R_UNABLE_TO_GET_ISSUER_DETAILS);
            goto err;
        }
    }

    if (isname != NULL) {
        if ((gens = sk_GENERAL_NAME_new_null()) == NULL
            || (gen = GENERAL_NAME_new()) == NULL
            || !sk_GENERAL_NAME_push(gens, gen)) {
            ERR_raise(ERR_LIB_X509V3, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        gen->type = GEN_DIRNAME;
        gen->d.dirn = isname;
    }

    akeyid->issuer = gens;
    gen = NULL;
    gens = NULL;
    akeyid->serial = serial;
    akeyid->keyid = ikeyid;

    return akeyid;

 err:
    sk_GENERAL_NAME_free(gens);
    GENERAL_NAME_free(gen);
    X509_NAME_free(isname);
    ASN1_INTEGER_free(serial);
    ASN1_OCTET_STRING_free(ikeyid);
    AUTHORITY_KEYID_free(akeyid);
    return NULL;
}
