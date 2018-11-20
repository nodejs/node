/* v3_info.c */
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
#include <openssl/conf.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/x509v3.h>

static STACK_OF(CONF_VALUE) *i2v_AUTHORITY_INFO_ACCESS(X509V3_EXT_METHOD
                                                       *method, AUTHORITY_INFO_ACCESS
                                                       *ainfo, STACK_OF(CONF_VALUE)
                                                       *ret);
static AUTHORITY_INFO_ACCESS *v2i_AUTHORITY_INFO_ACCESS(X509V3_EXT_METHOD
                                                        *method,
                                                        X509V3_CTX *ctx,
                                                        STACK_OF(CONF_VALUE)
                                                        *nval);

const X509V3_EXT_METHOD v3_info = { NID_info_access, X509V3_EXT_MULTILINE,
    ASN1_ITEM_ref(AUTHORITY_INFO_ACCESS),
    0, 0, 0, 0,
    0, 0,
    (X509V3_EXT_I2V) i2v_AUTHORITY_INFO_ACCESS,
    (X509V3_EXT_V2I)v2i_AUTHORITY_INFO_ACCESS,
    0, 0,
    NULL
};

const X509V3_EXT_METHOD v3_sinfo = { NID_sinfo_access, X509V3_EXT_MULTILINE,
    ASN1_ITEM_ref(AUTHORITY_INFO_ACCESS),
    0, 0, 0, 0,
    0, 0,
    (X509V3_EXT_I2V) i2v_AUTHORITY_INFO_ACCESS,
    (X509V3_EXT_V2I)v2i_AUTHORITY_INFO_ACCESS,
    0, 0,
    NULL
};

ASN1_SEQUENCE(ACCESS_DESCRIPTION) = {
        ASN1_SIMPLE(ACCESS_DESCRIPTION, method, ASN1_OBJECT),
        ASN1_SIMPLE(ACCESS_DESCRIPTION, location, GENERAL_NAME)
} ASN1_SEQUENCE_END(ACCESS_DESCRIPTION)

IMPLEMENT_ASN1_FUNCTIONS(ACCESS_DESCRIPTION)

ASN1_ITEM_TEMPLATE(AUTHORITY_INFO_ACCESS) =
        ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, GeneralNames, ACCESS_DESCRIPTION)
ASN1_ITEM_TEMPLATE_END(AUTHORITY_INFO_ACCESS)

IMPLEMENT_ASN1_FUNCTIONS(AUTHORITY_INFO_ACCESS)

static STACK_OF(CONF_VALUE) *i2v_AUTHORITY_INFO_ACCESS(X509V3_EXT_METHOD
                                                       *method, AUTHORITY_INFO_ACCESS
                                                       *ainfo, STACK_OF(CONF_VALUE)
                                                       *ret)
{
    ACCESS_DESCRIPTION *desc;
    int i, nlen;
    char objtmp[80], *ntmp;
    CONF_VALUE *vtmp;
    for (i = 0; i < sk_ACCESS_DESCRIPTION_num(ainfo); i++) {
        desc = sk_ACCESS_DESCRIPTION_value(ainfo, i);
        ret = i2v_GENERAL_NAME(method, desc->location, ret);
        if (!ret)
            break;
        vtmp = sk_CONF_VALUE_value(ret, i);
        i2t_ASN1_OBJECT(objtmp, sizeof objtmp, desc->method);
        nlen = strlen(objtmp) + strlen(vtmp->name) + 5;
        ntmp = OPENSSL_malloc(nlen);
        if (!ntmp) {
            X509V3err(X509V3_F_I2V_AUTHORITY_INFO_ACCESS,
                      ERR_R_MALLOC_FAILURE);
            return NULL;
        }
        BUF_strlcpy(ntmp, objtmp, nlen);
        BUF_strlcat(ntmp, " - ", nlen);
        BUF_strlcat(ntmp, vtmp->name, nlen);
        OPENSSL_free(vtmp->name);
        vtmp->name = ntmp;

    }
    if (!ret)
        return sk_CONF_VALUE_new_null();
    return ret;
}

static AUTHORITY_INFO_ACCESS *v2i_AUTHORITY_INFO_ACCESS(X509V3_EXT_METHOD
                                                        *method,
                                                        X509V3_CTX *ctx,
                                                        STACK_OF(CONF_VALUE)
                                                        *nval)
{
    AUTHORITY_INFO_ACCESS *ainfo = NULL;
    CONF_VALUE *cnf, ctmp;
    ACCESS_DESCRIPTION *acc;
    int i, objlen;
    char *objtmp, *ptmp;
    if (!(ainfo = sk_ACCESS_DESCRIPTION_new_null())) {
        X509V3err(X509V3_F_V2I_AUTHORITY_INFO_ACCESS, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    for (i = 0; i < sk_CONF_VALUE_num(nval); i++) {
        cnf = sk_CONF_VALUE_value(nval, i);
        if (!(acc = ACCESS_DESCRIPTION_new())
            || !sk_ACCESS_DESCRIPTION_push(ainfo, acc)) {
            X509V3err(X509V3_F_V2I_AUTHORITY_INFO_ACCESS,
                      ERR_R_MALLOC_FAILURE);
            goto err;
        }
        ptmp = strchr(cnf->name, ';');
        if (!ptmp) {
            X509V3err(X509V3_F_V2I_AUTHORITY_INFO_ACCESS,
                      X509V3_R_INVALID_SYNTAX);
            goto err;
        }
        objlen = ptmp - cnf->name;
        ctmp.name = ptmp + 1;
        ctmp.value = cnf->value;
        if (!v2i_GENERAL_NAME_ex(acc->location, method, ctx, &ctmp, 0))
            goto err;
        if (!(objtmp = OPENSSL_malloc(objlen + 1))) {
            X509V3err(X509V3_F_V2I_AUTHORITY_INFO_ACCESS,
                      ERR_R_MALLOC_FAILURE);
            goto err;
        }
        strncpy(objtmp, cnf->name, objlen);
        objtmp[objlen] = 0;
        acc->method = OBJ_txt2obj(objtmp, 0);
        if (!acc->method) {
            X509V3err(X509V3_F_V2I_AUTHORITY_INFO_ACCESS,
                      X509V3_R_BAD_OBJECT);
            ERR_add_error_data(2, "value=", objtmp);
            OPENSSL_free(objtmp);
            goto err;
        }
        OPENSSL_free(objtmp);

    }
    return ainfo;
 err:
    sk_ACCESS_DESCRIPTION_pop_free(ainfo, ACCESS_DESCRIPTION_free);
    return NULL;
}

int i2a_ACCESS_DESCRIPTION(BIO *bp, ACCESS_DESCRIPTION *a)
{
    i2a_ASN1_OBJECT(bp, a->method);
#ifdef UNDEF
    i2a_GENERAL_NAME(bp, a->location);
#endif
    return 2;
}
