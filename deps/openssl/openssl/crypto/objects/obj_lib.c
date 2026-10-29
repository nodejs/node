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
#include <openssl/objects.h>
#include <openssl/buffer.h>
#include "crypto/asn1.h"
#include "crypto/asn1/asn1_local.h"

ASN1_OBJECT *OBJ_dup(const ASN1_OBJECT *o)
{
    ASN1_OBJECT *r;

    if (o == NULL)
        return NULL;
    /* If object isn't dynamic it's an internal OID which is never freed */
    if (!(o->flags & ASN1_OBJECT_FLAG_DYNAMIC))
        return (ASN1_OBJECT *)o;

    r = ossl_asn1_object_new();
    if (r == NULL) {
        ERR_raise(ERR_LIB_OBJ, ERR_R_ASN1_LIB);
        return NULL;
    }

    /* Set dynamic flags so everything gets freed up on error */

    r->flags = o->flags | (ASN1_OBJECT_FLAG_DYNAMIC | ASN1_OBJECT_FLAG_DYNAMIC_STRINGS | ASN1_OBJECT_FLAG_DYNAMIC_DATA);

    if (o->length > 0 && (r->data = OPENSSL_memdup(o->data, o->length)) == NULL)
        goto err;

    r->length = o->length;
    r->nid = o->nid;

    if (o->ln != NULL && (r->ln = OPENSSL_strdup(o->ln)) == NULL)
        goto err;

    if (o->sn != NULL && (r->sn = OPENSSL_strdup(o->sn)) == NULL)
        goto err;

    return r;
err:
    ASN1_OBJECT_free(r);
    return NULL;
}

int OBJ_cmp(const ASN1_OBJECT *a, const ASN1_OBJECT *b)
{
    int ret;

    ret = (a->length - b->length);
    if (ret)
        return ret;
    if (a->length == 0)
        return 0;
    return memcmp(a->data, b->data, a->length);
}
