/* v3_pcons.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2003 The OpenSSL Project.  All rights reserved.
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
#include <openssl/asn1t.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>

static STACK_OF(CONF_VALUE) *i2v_POLICY_CONSTRAINTS(const X509V3_EXT_METHOD
                                                    *method, void *bcons, STACK_OF(CONF_VALUE)
                                                    *extlist);
static void *v2i_POLICY_CONSTRAINTS(const X509V3_EXT_METHOD *method,
                                    X509V3_CTX *ctx,
                                    STACK_OF(CONF_VALUE) *values);

const X509V3_EXT_METHOD v3_policy_constraints = {
    NID_policy_constraints, 0,
    ASN1_ITEM_ref(POLICY_CONSTRAINTS),
    0, 0, 0, 0,
    0, 0,
    i2v_POLICY_CONSTRAINTS,
    v2i_POLICY_CONSTRAINTS,
    NULL, NULL,
    NULL
};

ASN1_SEQUENCE(POLICY_CONSTRAINTS) = {
        ASN1_IMP_OPT(POLICY_CONSTRAINTS, requireExplicitPolicy, ASN1_INTEGER,0),
        ASN1_IMP_OPT(POLICY_CONSTRAINTS, inhibitPolicyMapping, ASN1_INTEGER,1)
} ASN1_SEQUENCE_END(POLICY_CONSTRAINTS)

IMPLEMENT_ASN1_ALLOC_FUNCTIONS(POLICY_CONSTRAINTS)

static STACK_OF(CONF_VALUE) *i2v_POLICY_CONSTRAINTS(const X509V3_EXT_METHOD
                                                    *method, void *a, STACK_OF(CONF_VALUE)
                                                    *extlist)
{
    POLICY_CONSTRAINTS *pcons = a;
    X509V3_add_value_int("Require Explicit Policy",
                         pcons->requireExplicitPolicy, &extlist);
    X509V3_add_value_int("Inhibit Policy Mapping",
                         pcons->inhibitPolicyMapping, &extlist);
    return extlist;
}

static void *v2i_POLICY_CONSTRAINTS(const X509V3_EXT_METHOD *method,
                                    X509V3_CTX *ctx,
                                    STACK_OF(CONF_VALUE) *values)
{
    POLICY_CONSTRAINTS *pcons = NULL;
    CONF_VALUE *val;
    int i;
    if (!(pcons = POLICY_CONSTRAINTS_new())) {
        X509V3err(X509V3_F_V2I_POLICY_CONSTRAINTS, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    for (i = 0; i < sk_CONF_VALUE_num(values); i++) {
        val = sk_CONF_VALUE_value(values, i);
        if (!strcmp(val->name, "requireExplicitPolicy")) {
            if (!X509V3_get_value_int(val, &pcons->requireExplicitPolicy))
                goto err;
        } else if (!strcmp(val->name, "inhibitPolicyMapping")) {
            if (!X509V3_get_value_int(val, &pcons->inhibitPolicyMapping))
                goto err;
        } else {
            X509V3err(X509V3_F_V2I_POLICY_CONSTRAINTS, X509V3_R_INVALID_NAME);
            X509V3_conf_err(val);
            goto err;
        }
    }
    if (!pcons->inhibitPolicyMapping && !pcons->requireExplicitPolicy) {
        X509V3err(X509V3_F_V2I_POLICY_CONSTRAINTS,
                  X509V3_R_ILLEGAL_EMPTY_EXTENSION);
        goto err;
    }

    return pcons;
 err:
    POLICY_CONSTRAINTS_free(pcons);
    return NULL;
}
