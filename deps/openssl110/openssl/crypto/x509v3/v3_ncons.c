/*
 * Copyright 2003-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include "internal/asn1_int.h"
#include <openssl/asn1t.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>

#include "internal/x509_int.h"
#include "ext_dat.h"

static void *v2i_NAME_CONSTRAINTS(const X509V3_EXT_METHOD *method,
                                  X509V3_CTX *ctx,
                                  STACK_OF(CONF_VALUE) *nval);
static int i2r_NAME_CONSTRAINTS(const X509V3_EXT_METHOD *method, void *a,
                                BIO *bp, int ind);
static int do_i2r_name_constraints(const X509V3_EXT_METHOD *method,
                                   STACK_OF(GENERAL_SUBTREE) *trees, BIO *bp,
                                   int ind, const char *name);
static int print_nc_ipadd(BIO *bp, ASN1_OCTET_STRING *ip);

static int nc_match(GENERAL_NAME *gen, NAME_CONSTRAINTS *nc);
static int nc_match_single(GENERAL_NAME *sub, GENERAL_NAME *gen);
static int nc_dn(X509_NAME *sub, X509_NAME *nm);
static int nc_dns(ASN1_IA5STRING *sub, ASN1_IA5STRING *dns);
static int nc_email(ASN1_IA5STRING *sub, ASN1_IA5STRING *eml);
static int nc_uri(ASN1_IA5STRING *uri, ASN1_IA5STRING *base);
static int nc_ip(ASN1_OCTET_STRING *ip, ASN1_OCTET_STRING *base);

const X509V3_EXT_METHOD v3_name_constraints = {
    NID_name_constraints, 0,
    ASN1_ITEM_ref(NAME_CONSTRAINTS),
    0, 0, 0, 0,
    0, 0,
    0, v2i_NAME_CONSTRAINTS,
    i2r_NAME_CONSTRAINTS, 0,
    NULL
};

ASN1_SEQUENCE(GENERAL_SUBTREE) = {
        ASN1_SIMPLE(GENERAL_SUBTREE, base, GENERAL_NAME),
        ASN1_IMP_OPT(GENERAL_SUBTREE, minimum, ASN1_INTEGER, 0),
        ASN1_IMP_OPT(GENERAL_SUBTREE, maximum, ASN1_INTEGER, 1)
} ASN1_SEQUENCE_END(GENERAL_SUBTREE)

ASN1_SEQUENCE(NAME_CONSTRAINTS) = {
        ASN1_IMP_SEQUENCE_OF_OPT(NAME_CONSTRAINTS, permittedSubtrees,
                                                        GENERAL_SUBTREE, 0),
        ASN1_IMP_SEQUENCE_OF_OPT(NAME_CONSTRAINTS, excludedSubtrees,
                                                        GENERAL_SUBTREE, 1),
} ASN1_SEQUENCE_END(NAME_CONSTRAINTS)


IMPLEMENT_ASN1_ALLOC_FUNCTIONS(GENERAL_SUBTREE)
IMPLEMENT_ASN1_ALLOC_FUNCTIONS(NAME_CONSTRAINTS)

static void *v2i_NAME_CONSTRAINTS(const X509V3_EXT_METHOD *method,
                                  X509V3_CTX *ctx, STACK_OF(CONF_VALUE) *nval)
{
    int i;
    CONF_VALUE tval, *val;
    STACK_OF(GENERAL_SUBTREE) **ptree = NULL;
    NAME_CONSTRAINTS *ncons = NULL;
    GENERAL_SUBTREE *sub = NULL;

    ncons = NAME_CONSTRAINTS_new();
    if (ncons == NULL)
        goto memerr;
    for (i = 0; i < sk_CONF_VALUE_num(nval); i++) {
        val = sk_CONF_VALUE_value(nval, i);
        if (strncmp(val->name, "permitted", 9) == 0 && val->name[9]) {
            ptree = &ncons->permittedSubtrees;
            tval.name = val->name + 10;
        } else if (strncmp(val->name, "excluded", 8) == 0 && val->name[8]) {
            ptree = &ncons->excludedSubtrees;
            tval.name = val->name + 9;
        } else {
            X509V3err(X509V3_F_V2I_NAME_CONSTRAINTS, X509V3_R_INVALID_SYNTAX);
            goto err;
        }
        tval.value = val->value;
        sub = GENERAL_SUBTREE_new();
        if (sub == NULL)
            goto memerr;
        if (!v2i_GENERAL_NAME_ex(sub->base, method, ctx, &tval, 1))
            goto err;
        if (*ptree == NULL)
            *ptree = sk_GENERAL_SUBTREE_new_null();
        if (*ptree == NULL || !sk_GENERAL_SUBTREE_push(*ptree, sub))
            goto memerr;
        sub = NULL;
    }

    return ncons;

 memerr:
    X509V3err(X509V3_F_V2I_NAME_CONSTRAINTS, ERR_R_MALLOC_FAILURE);
 err:
    NAME_CONSTRAINTS_free(ncons);
    GENERAL_SUBTREE_free(sub);

    return NULL;
}

static int i2r_NAME_CONSTRAINTS(const X509V3_EXT_METHOD *method, void *a,
                                BIO *bp, int ind)
{
    NAME_CONSTRAINTS *ncons = a;
    do_i2r_name_constraints(method, ncons->permittedSubtrees,
                            bp, ind, "Permitted");
    do_i2r_name_constraints(method, ncons->excludedSubtrees,
                            bp, ind, "Excluded");
    return 1;
}

static int do_i2r_name_constraints(const X509V3_EXT_METHOD *method,
                                   STACK_OF(GENERAL_SUBTREE) *trees,
                                   BIO *bp, int ind, const char *name)
{
    GENERAL_SUBTREE *tree;
    int i;
    if (sk_GENERAL_SUBTREE_num(trees) > 0)
        BIO_printf(bp, "%*s%s:\n", ind, "", name);
    for (i = 0; i < sk_GENERAL_SUBTREE_num(trees); i++) {
        tree = sk_GENERAL_SUBTREE_value(trees, i);
        BIO_printf(bp, "%*s", ind + 2, "");
        if (tree->base->type == GEN_IPADD)
            print_nc_ipadd(bp, tree->base->d.ip);
        else
            GENERAL_NAME_print(bp, tree->base);
        BIO_puts(bp, "\n");
    }
    return 1;
}

static int print_nc_ipadd(BIO *bp, ASN1_OCTET_STRING *ip)
{
    int i, len;
    unsigned char *p;
    p = ip->data;
    len = ip->length;
    BIO_puts(bp, "IP:");
    if (len == 8) {
        BIO_printf(bp, "%d.%d.%d.%d/%d.%d.%d.%d",
                   p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    } else if (len == 32) {
        for (i = 0; i < 16; i++) {
            BIO_printf(bp, "%X", p[0] << 8 | p[1]);
            p += 2;
            if (i == 7)
                BIO_puts(bp, "/");
            else if (i != 15)
                BIO_puts(bp, ":");
        }
    } else
        BIO_printf(bp, "IP Address:<invalid>");
    return 1;
}

/*-
 * Check a certificate conforms to a specified set of constraints.
 * Return values:
 *  X509_V_OK: All constraints obeyed.
 *  X509_V_ERR_PERMITTED_VIOLATION: Permitted subtree violation.
 *  X509_V_ERR_EXCLUDED_VIOLATION: Excluded subtree violation.
 *  X509_V_ERR_SUBTREE_MINMAX: Min or max values present and matching type.
 *  X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE:  Unsupported constraint type.
 *  X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX: bad unsupported constraint syntax.
 *  X509_V_ERR_UNSUPPORTED_NAME_SYNTAX: bad or unsupported syntax of name
 */

int NAME_CONSTRAINTS_check(X509 *x, NAME_CONSTRAINTS *nc)
{
    int r, i;
    X509_NAME *nm;

    nm = X509_get_subject_name(x);

    if (X509_NAME_entry_count(nm) > 0) {
        GENERAL_NAME gntmp;
        gntmp.type = GEN_DIRNAME;
        gntmp.d.directoryName = nm;

        r = nc_match(&gntmp, nc);

        if (r != X509_V_OK)
            return r;

        gntmp.type = GEN_EMAIL;

        /* Process any email address attributes in subject name */

        for (i = -1;;) {
            const X509_NAME_ENTRY *ne;

            i = X509_NAME_get_index_by_NID(nm, NID_pkcs9_emailAddress, i);
            if (i == -1)
                break;
            ne = X509_NAME_get_entry(nm, i);
            gntmp.d.rfc822Name = X509_NAME_ENTRY_get_data(ne);
            if (gntmp.d.rfc822Name->type != V_ASN1_IA5STRING)
                return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;

            r = nc_match(&gntmp, nc);

            if (r != X509_V_OK)
                return r;
        }

    }

    for (i = 0; i < sk_GENERAL_NAME_num(x->altname); i++) {
        GENERAL_NAME *gen = sk_GENERAL_NAME_value(x->altname, i);
        r = nc_match(gen, nc);
        if (r != X509_V_OK)
            return r;
    }

    return X509_V_OK;

}

int NAME_CONSTRAINTS_check_CN(X509 *x, NAME_CONSTRAINTS *nc)
{
    int r, i;
    X509_NAME *nm;

    ASN1_STRING stmp;
    GENERAL_NAME gntmp;
    stmp.flags = 0;
    stmp.type = V_ASN1_IA5STRING;
    gntmp.type = GEN_DNS;
    gntmp.d.dNSName = &stmp;

    nm = X509_get_subject_name(x);

    /* Process any commonName attributes in subject name */

    for (i = -1;;) {
        X509_NAME_ENTRY *ne;
        ASN1_STRING *hn;
        i = X509_NAME_get_index_by_NID(nm, NID_commonName, i);
        if (i == -1)
            break;
        ne = X509_NAME_get_entry(nm, i);
        hn = X509_NAME_ENTRY_get_data(ne);
        /* Only process attributes that look like host names */
        if (asn1_valid_host(hn)) {
            unsigned char *h;
            int hlen = ASN1_STRING_to_UTF8(&h, hn);
            if (hlen <= 0)
                return X509_V_ERR_OUT_OF_MEM;

            stmp.length = hlen;
            stmp.data = h;

            r = nc_match(&gntmp, nc);

            OPENSSL_free(h);

            if (r != X509_V_OK)
                    return r;
        }
    }
    return X509_V_OK;
}

static int nc_match(GENERAL_NAME *gen, NAME_CONSTRAINTS *nc)
{
    GENERAL_SUBTREE *sub;
    int i, r, match = 0;

    /*
     * Permitted subtrees: if any subtrees exist of matching the type at
     * least one subtree must match.
     */

    for (i = 0; i < sk_GENERAL_SUBTREE_num(nc->permittedSubtrees); i++) {
        sub = sk_GENERAL_SUBTREE_value(nc->permittedSubtrees, i);
        if (gen->type != sub->base->type)
            continue;
        if (sub->minimum || sub->maximum)
            return X509_V_ERR_SUBTREE_MINMAX;
        /* If we already have a match don't bother trying any more */
        if (match == 2)
            continue;
        if (match == 0)
            match = 1;
        r = nc_match_single(gen, sub->base);
        if (r == X509_V_OK)
            match = 2;
        else if (r != X509_V_ERR_PERMITTED_VIOLATION)
            return r;
    }

    if (match == 1)
        return X509_V_ERR_PERMITTED_VIOLATION;

    /* Excluded subtrees: must not match any of these */

    for (i = 0; i < sk_GENERAL_SUBTREE_num(nc->excludedSubtrees); i++) {
        sub = sk_GENERAL_SUBTREE_value(nc->excludedSubtrees, i);
        if (gen->type != sub->base->type)
            continue;
        if (sub->minimum || sub->maximum)
            return X509_V_ERR_SUBTREE_MINMAX;

        r = nc_match_single(gen, sub->base);
        if (r == X509_V_OK)
            return X509_V_ERR_EXCLUDED_VIOLATION;
        else if (r != X509_V_ERR_PERMITTED_VIOLATION)
            return r;

    }

    return X509_V_OK;

}

static int nc_match_single(GENERAL_NAME *gen, GENERAL_NAME *base)
{
    switch (base->type) {
    case GEN_DIRNAME:
        return nc_dn(gen->d.directoryName, base->d.directoryName);

    case GEN_DNS:
        return nc_dns(gen->d.dNSName, base->d.dNSName);

    case GEN_EMAIL:
        return nc_email(gen->d.rfc822Name, base->d.rfc822Name);

    case GEN_URI:
        return nc_uri(gen->d.uniformResourceIdentifier,
                      base->d.uniformResourceIdentifier);

    case GEN_IPADD:
        return nc_ip(gen->d.iPAddress, base->d.iPAddress);

    default:
        return X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE;
    }

}

/*
 * directoryName name constraint matching. The canonical encoding of
 * X509_NAME makes this comparison easy. It is matched if the subtree is a
 * subset of the name.
 */

static int nc_dn(X509_NAME *nm, X509_NAME *base)
{
    /* Ensure canonical encodings are up to date.  */
    if (nm->modified && i2d_X509_NAME(nm, NULL) < 0)
        return X509_V_ERR_OUT_OF_MEM;
    if (base->modified && i2d_X509_NAME(base, NULL) < 0)
        return X509_V_ERR_OUT_OF_MEM;
    if (base->canon_enclen > nm->canon_enclen)
        return X509_V_ERR_PERMITTED_VIOLATION;
    if (memcmp(base->canon_enc, nm->canon_enc, base->canon_enclen))
        return X509_V_ERR_PERMITTED_VIOLATION;
    return X509_V_OK;
}

static int nc_dns(ASN1_IA5STRING *dns, ASN1_IA5STRING *base)
{
    char *baseptr = (char *)base->data;
    char *dnsptr = (char *)dns->data;
    /* Empty matches everything */
    if (!*baseptr)
        return X509_V_OK;
    /*
     * Otherwise can add zero or more components on the left so compare RHS
     * and if dns is longer and expect '.' as preceding character.
     */
    if (dns->length > base->length) {
        dnsptr += dns->length - base->length;
        if (*baseptr != '.' && dnsptr[-1] != '.')
            return X509_V_ERR_PERMITTED_VIOLATION;
    }

    if (strcasecmp(baseptr, dnsptr))
        return X509_V_ERR_PERMITTED_VIOLATION;

    return X509_V_OK;

}

static int nc_email(ASN1_IA5STRING *eml, ASN1_IA5STRING *base)
{
    const char *baseptr = (char *)base->data;
    const char *emlptr = (char *)eml->data;

    const char *baseat = strchr(baseptr, '@');
    const char *emlat = strchr(emlptr, '@');
    if (!emlat)
        return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
    /* Special case: initial '.' is RHS match */
    if (!baseat && (*baseptr == '.')) {
        if (eml->length > base->length) {
            emlptr += eml->length - base->length;
            if (strcasecmp(baseptr, emlptr) == 0)
                return X509_V_OK;
        }
        return X509_V_ERR_PERMITTED_VIOLATION;
    }

    /* If we have anything before '@' match local part */

    if (baseat) {
        if (baseat != baseptr) {
            if ((baseat - baseptr) != (emlat - emlptr))
                return X509_V_ERR_PERMITTED_VIOLATION;
            /* Case sensitive match of local part */
            if (strncmp(baseptr, emlptr, emlat - emlptr))
                return X509_V_ERR_PERMITTED_VIOLATION;
        }
        /* Position base after '@' */
        baseptr = baseat + 1;
    }
    emlptr = emlat + 1;
    /* Just have hostname left to match: case insensitive */
    if (strcasecmp(baseptr, emlptr))
        return X509_V_ERR_PERMITTED_VIOLATION;

    return X509_V_OK;

}

static int nc_uri(ASN1_IA5STRING *uri, ASN1_IA5STRING *base)
{
    const char *baseptr = (char *)base->data;
    const char *hostptr = (char *)uri->data;
    const char *p = strchr(hostptr, ':');
    int hostlen;
    /* Check for foo:// and skip past it */
    if (!p || (p[1] != '/') || (p[2] != '/'))
        return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
    hostptr = p + 3;

    /* Determine length of hostname part of URI */

    /* Look for a port indicator as end of hostname first */

    p = strchr(hostptr, ':');
    /* Otherwise look for trailing slash */
    if (!p)
        p = strchr(hostptr, '/');

    if (!p)
        hostlen = strlen(hostptr);
    else
        hostlen = p - hostptr;

    if (hostlen == 0)
        return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;

    /* Special case: initial '.' is RHS match */
    if (*baseptr == '.') {
        if (hostlen > base->length) {
            p = hostptr + hostlen - base->length;
            if (strncasecmp(p, baseptr, base->length) == 0)
                return X509_V_OK;
        }
        return X509_V_ERR_PERMITTED_VIOLATION;
    }

    if ((base->length != (int)hostlen)
        || strncasecmp(hostptr, baseptr, hostlen))
        return X509_V_ERR_PERMITTED_VIOLATION;

    return X509_V_OK;

}

static int nc_ip(ASN1_OCTET_STRING *ip, ASN1_OCTET_STRING *base)
{
    int hostlen, baselen, i;
    unsigned char *hostptr, *baseptr, *maskptr;
    hostptr = ip->data;
    hostlen = ip->length;
    baseptr = base->data;
    baselen = base->length;

    /* Invalid if not IPv4 or IPv6 */
    if (!((hostlen == 4) || (hostlen == 16)))
        return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;
    if (!((baselen == 8) || (baselen == 32)))
        return X509_V_ERR_UNSUPPORTED_NAME_SYNTAX;

    /* Do not match IPv4 with IPv6 */
    if (hostlen * 2 != baselen)
        return X509_V_ERR_PERMITTED_VIOLATION;

    maskptr = base->data + hostlen;

    /* Considering possible not aligned base ipAddress */
    /* Not checking for wrong mask definition: i.e.: 255.0.255.0 */
    for (i = 0; i < hostlen; i++)
        if ((hostptr[i] & maskptr[i]) != (baseptr[i] & maskptr[i]))
            return X509_V_ERR_PERMITTED_VIOLATION;

    return X509_V_OK;

}
