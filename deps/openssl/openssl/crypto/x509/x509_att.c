/* crypto/x509/x509_att.c */
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
#include <openssl/stack.h>
#include "cryptlib.h"
#include <openssl/asn1.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

int X509at_get_attr_count(const STACK_OF(X509_ATTRIBUTE) *x)
{
    return sk_X509_ATTRIBUTE_num(x);
}

int X509at_get_attr_by_NID(const STACK_OF(X509_ATTRIBUTE) *x, int nid,
                           int lastpos)
{
    ASN1_OBJECT *obj;

    obj = OBJ_nid2obj(nid);
    if (obj == NULL)
        return (-2);
    return (X509at_get_attr_by_OBJ(x, obj, lastpos));
}

int X509at_get_attr_by_OBJ(const STACK_OF(X509_ATTRIBUTE) *sk,
                           ASN1_OBJECT *obj, int lastpos)
{
    int n;
    X509_ATTRIBUTE *ex;

    if (sk == NULL)
        return (-1);
    lastpos++;
    if (lastpos < 0)
        lastpos = 0;
    n = sk_X509_ATTRIBUTE_num(sk);
    for (; lastpos < n; lastpos++) {
        ex = sk_X509_ATTRIBUTE_value(sk, lastpos);
        if (OBJ_cmp(ex->object, obj) == 0)
            return (lastpos);
    }
    return (-1);
}

X509_ATTRIBUTE *X509at_get_attr(const STACK_OF(X509_ATTRIBUTE) *x, int loc)
{
    if (x == NULL || sk_X509_ATTRIBUTE_num(x) <= loc || loc < 0)
        return NULL;
    else
        return sk_X509_ATTRIBUTE_value(x, loc);
}

X509_ATTRIBUTE *X509at_delete_attr(STACK_OF(X509_ATTRIBUTE) *x, int loc)
{
    X509_ATTRIBUTE *ret;

    if (x == NULL || sk_X509_ATTRIBUTE_num(x) <= loc || loc < 0)
        return (NULL);
    ret = sk_X509_ATTRIBUTE_delete(x, loc);
    return (ret);
}

STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr(STACK_OF(X509_ATTRIBUTE) **x,
                                           X509_ATTRIBUTE *attr)
{
    X509_ATTRIBUTE *new_attr = NULL;
    STACK_OF(X509_ATTRIBUTE) *sk = NULL;

    if (x == NULL) {
        X509err(X509_F_X509AT_ADD1_ATTR, ERR_R_PASSED_NULL_PARAMETER);
        goto err2;
    }

    if (*x == NULL) {
        if ((sk = sk_X509_ATTRIBUTE_new_null()) == NULL)
            goto err;
    } else
        sk = *x;

    if ((new_attr = X509_ATTRIBUTE_dup(attr)) == NULL)
        goto err2;
    if (!sk_X509_ATTRIBUTE_push(sk, new_attr))
        goto err;
    if (*x == NULL)
        *x = sk;
    return (sk);
 err:
    X509err(X509_F_X509AT_ADD1_ATTR, ERR_R_MALLOC_FAILURE);
 err2:
    if (new_attr != NULL)
        X509_ATTRIBUTE_free(new_attr);
    if (sk != NULL)
        sk_X509_ATTRIBUTE_free(sk);
    return (NULL);
}

STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr_by_OBJ(STACK_OF(X509_ATTRIBUTE)
                                                  **x, const ASN1_OBJECT *obj,
                                                  int type,
                                                  const unsigned char *bytes,
                                                  int len)
{
    X509_ATTRIBUTE *attr;
    STACK_OF(X509_ATTRIBUTE) *ret;
    attr = X509_ATTRIBUTE_create_by_OBJ(NULL, obj, type, bytes, len);
    if (!attr)
        return 0;
    ret = X509at_add1_attr(x, attr);
    X509_ATTRIBUTE_free(attr);
    return ret;
}

STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr_by_NID(STACK_OF(X509_ATTRIBUTE)
                                                  **x, int nid, int type,
                                                  const unsigned char *bytes,
                                                  int len)
{
    X509_ATTRIBUTE *attr;
    STACK_OF(X509_ATTRIBUTE) *ret;
    attr = X509_ATTRIBUTE_create_by_NID(NULL, nid, type, bytes, len);
    if (!attr)
        return 0;
    ret = X509at_add1_attr(x, attr);
    X509_ATTRIBUTE_free(attr);
    return ret;
}

STACK_OF(X509_ATTRIBUTE) *X509at_add1_attr_by_txt(STACK_OF(X509_ATTRIBUTE)
                                                  **x, const char *attrname,
                                                  int type,
                                                  const unsigned char *bytes,
                                                  int len)
{
    X509_ATTRIBUTE *attr;
    STACK_OF(X509_ATTRIBUTE) *ret;
    attr = X509_ATTRIBUTE_create_by_txt(NULL, attrname, type, bytes, len);
    if (!attr)
        return 0;
    ret = X509at_add1_attr(x, attr);
    X509_ATTRIBUTE_free(attr);
    return ret;
}

void *X509at_get0_data_by_OBJ(STACK_OF(X509_ATTRIBUTE) *x,
                              ASN1_OBJECT *obj, int lastpos, int type)
{
    int i;
    X509_ATTRIBUTE *at;
    i = X509at_get_attr_by_OBJ(x, obj, lastpos);
    if (i == -1)
        return NULL;
    if ((lastpos <= -2) && (X509at_get_attr_by_OBJ(x, obj, i) != -1))
        return NULL;
    at = X509at_get_attr(x, i);
    if (lastpos <= -3 && (X509_ATTRIBUTE_count(at) != 1))
        return NULL;
    return X509_ATTRIBUTE_get0_data(at, 0, type, NULL);
}

X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_NID(X509_ATTRIBUTE **attr, int nid,
                                             int atrtype, const void *data,
                                             int len)
{
    ASN1_OBJECT *obj;
    X509_ATTRIBUTE *ret;

    obj = OBJ_nid2obj(nid);
    if (obj == NULL) {
        X509err(X509_F_X509_ATTRIBUTE_CREATE_BY_NID, X509_R_UNKNOWN_NID);
        return (NULL);
    }
    ret = X509_ATTRIBUTE_create_by_OBJ(attr, obj, atrtype, data, len);
    if (ret == NULL)
        ASN1_OBJECT_free(obj);
    return (ret);
}

X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_OBJ(X509_ATTRIBUTE **attr,
                                             const ASN1_OBJECT *obj,
                                             int atrtype, const void *data,
                                             int len)
{
    X509_ATTRIBUTE *ret;

    if ((attr == NULL) || (*attr == NULL)) {
        if ((ret = X509_ATTRIBUTE_new()) == NULL) {
            X509err(X509_F_X509_ATTRIBUTE_CREATE_BY_OBJ,
                    ERR_R_MALLOC_FAILURE);
            return (NULL);
        }
    } else
        ret = *attr;

    if (!X509_ATTRIBUTE_set1_object(ret, obj))
        goto err;
    if (!X509_ATTRIBUTE_set1_data(ret, atrtype, data, len))
        goto err;

    if ((attr != NULL) && (*attr == NULL))
        *attr = ret;
    return (ret);
 err:
    if ((attr == NULL) || (ret != *attr))
        X509_ATTRIBUTE_free(ret);
    return (NULL);
}

X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_txt(X509_ATTRIBUTE **attr,
                                             const char *atrname, int type,
                                             const unsigned char *bytes,
                                             int len)
{
    ASN1_OBJECT *obj;
    X509_ATTRIBUTE *nattr;

    obj = OBJ_txt2obj(atrname, 0);
    if (obj == NULL) {
        X509err(X509_F_X509_ATTRIBUTE_CREATE_BY_TXT,
                X509_R_INVALID_FIELD_NAME);
        ERR_add_error_data(2, "name=", atrname);
        return (NULL);
    }
    nattr = X509_ATTRIBUTE_create_by_OBJ(attr, obj, type, bytes, len);
    ASN1_OBJECT_free(obj);
    return nattr;
}

int X509_ATTRIBUTE_set1_object(X509_ATTRIBUTE *attr, const ASN1_OBJECT *obj)
{
    if ((attr == NULL) || (obj == NULL))
        return (0);
    ASN1_OBJECT_free(attr->object);
    attr->object = OBJ_dup(obj);
    return (1);
}

int X509_ATTRIBUTE_set1_data(X509_ATTRIBUTE *attr, int attrtype,
                             const void *data, int len)
{
    ASN1_TYPE *ttmp = NULL;
    ASN1_STRING *stmp = NULL;
    int atype = 0;
    if (!attr)
        return 0;
    if (attrtype & MBSTRING_FLAG) {
        stmp = ASN1_STRING_set_by_NID(NULL, data, len, attrtype,
                                      OBJ_obj2nid(attr->object));
        if (!stmp) {
            X509err(X509_F_X509_ATTRIBUTE_SET1_DATA, ERR_R_ASN1_LIB);
            return 0;
        }
        atype = stmp->type;
    } else if (len != -1) {
        if (!(stmp = ASN1_STRING_type_new(attrtype)))
            goto err;
        if (!ASN1_STRING_set(stmp, data, len))
            goto err;
        atype = attrtype;
    }
    if (!(attr->value.set = sk_ASN1_TYPE_new_null()))
        goto err;
    attr->single = 0;
    /*
     * This is a bit naughty because the attribute should really have at
     * least one value but some types use and zero length SET and require
     * this.
     */
    if (attrtype == 0) {
        ASN1_STRING_free(stmp);
        return 1;
    }
    if (!(ttmp = ASN1_TYPE_new()))
        goto err;
    if ((len == -1) && !(attrtype & MBSTRING_FLAG)) {
        if (!ASN1_TYPE_set1(ttmp, attrtype, data))
            goto err;
    } else {
        ASN1_TYPE_set(ttmp, atype, stmp);
        stmp = NULL;
    }
    if (!sk_ASN1_TYPE_push(attr->value.set, ttmp))
        goto err;
    return 1;
 err:
    X509err(X509_F_X509_ATTRIBUTE_SET1_DATA, ERR_R_MALLOC_FAILURE);
    ASN1_TYPE_free(ttmp);
    ASN1_STRING_free(stmp);
    return 0;
}

int X509_ATTRIBUTE_count(X509_ATTRIBUTE *attr)
{
    if (!attr->single)
        return sk_ASN1_TYPE_num(attr->value.set);
    if (attr->value.single)
        return 1;
    return 0;
}

ASN1_OBJECT *X509_ATTRIBUTE_get0_object(X509_ATTRIBUTE *attr)
{
    if (attr == NULL)
        return (NULL);
    return (attr->object);
}

void *X509_ATTRIBUTE_get0_data(X509_ATTRIBUTE *attr, int idx,
                               int atrtype, void *data)
{
    ASN1_TYPE *ttmp;
    ttmp = X509_ATTRIBUTE_get0_type(attr, idx);
    if (!ttmp)
        return NULL;
    if (atrtype != ASN1_TYPE_get(ttmp)) {
        X509err(X509_F_X509_ATTRIBUTE_GET0_DATA, X509_R_WRONG_TYPE);
        return NULL;
    }
    return ttmp->value.ptr;
}

ASN1_TYPE *X509_ATTRIBUTE_get0_type(X509_ATTRIBUTE *attr, int idx)
{
    if (attr == NULL)
        return (NULL);
    if (idx >= X509_ATTRIBUTE_count(attr))
        return NULL;
    if (!attr->single)
        return sk_ASN1_TYPE_value(attr->value.set, idx);
    else
        return attr->value.single;
}
