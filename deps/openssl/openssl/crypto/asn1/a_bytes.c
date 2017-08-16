/* crypto/asn1/a_bytes.c */
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
#include <openssl/asn1.h>

static int asn1_collate_primitive(ASN1_STRING *a, ASN1_const_CTX *c,
                                  int depth);
static ASN1_STRING *int_d2i_ASN1_bytes(ASN1_STRING **a,
                                       const unsigned char **pp, long length,
                                       int Ptag, int Pclass, int depth,
                                       int *perr);
/*
 * type is a 'bitmap' of acceptable string types.
 */
ASN1_STRING *d2i_ASN1_type_bytes(ASN1_STRING **a, const unsigned char **pp,
                                 long length, int type)
{
    ASN1_STRING *ret = NULL;
    const unsigned char *p;
    unsigned char *s;
    long len;
    int inf, tag, xclass;
    int i = 0;

    p = *pp;
    inf = ASN1_get_object(&p, &len, &tag, &xclass, length);
    if (inf & 0x80)
        goto err;

    if (tag >= 32) {
        i = ASN1_R_TAG_VALUE_TOO_HIGH;
        goto err;
    }
    if (!(ASN1_tag2bit(tag) & type)) {
        i = ASN1_R_WRONG_TYPE;
        goto err;
    }

    /* If a bit-string, exit early */
    if (tag == V_ASN1_BIT_STRING)
        return (d2i_ASN1_BIT_STRING(a, pp, length));

    if ((a == NULL) || ((*a) == NULL)) {
        if ((ret = ASN1_STRING_new()) == NULL)
            return (NULL);
    } else
        ret = (*a);

    if (len != 0) {
        s = OPENSSL_malloc((int)len + 1);
        if (s == NULL) {
            i = ERR_R_MALLOC_FAILURE;
            goto err;
        }
        memcpy(s, p, (int)len);
        s[len] = '\0';
        p += len;
    } else
        s = NULL;

    if (ret->data != NULL)
        OPENSSL_free(ret->data);
    ret->length = (int)len;
    ret->data = s;
    ret->type = tag;
    if (a != NULL)
        (*a) = ret;
    *pp = p;
    return (ret);
 err:
    ASN1err(ASN1_F_D2I_ASN1_TYPE_BYTES, i);
    if ((ret != NULL) && ((a == NULL) || (*a != ret)))
        ASN1_STRING_free(ret);
    return (NULL);
}

int i2d_ASN1_bytes(ASN1_STRING *a, unsigned char **pp, int tag, int xclass)
{
    int ret, r, constructed;
    unsigned char *p;

    if (a == NULL)
        return (0);

    if (tag == V_ASN1_BIT_STRING)
        return (i2d_ASN1_BIT_STRING(a, pp));

    ret = a->length;
    r = ASN1_object_size(0, ret, tag);
    if (pp == NULL)
        return (r);
    p = *pp;

    if ((tag == V_ASN1_SEQUENCE) || (tag == V_ASN1_SET))
        constructed = 1;
    else
        constructed = 0;
    ASN1_put_object(&p, constructed, ret, tag, xclass);
    memcpy(p, a->data, a->length);
    p += a->length;
    *pp = p;
    return (r);
}

/*
 * Maximum recursion depth of d2i_ASN1_bytes(): much more than should be
 * encountered in pratice.
 */

#define ASN1_BYTES_MAXDEPTH 20

ASN1_STRING *d2i_ASN1_bytes(ASN1_STRING **a, const unsigned char **pp,
                            long length, int Ptag, int Pclass)
{
    int err = 0;
    ASN1_STRING *s = int_d2i_ASN1_bytes(a, pp, length, Ptag, Pclass, 0, &err);
    if (err != 0)
        ASN1err(ASN1_F_D2I_ASN1_BYTES, err);
    return s;
}

static ASN1_STRING *int_d2i_ASN1_bytes(ASN1_STRING **a,
                                       const unsigned char **pp, long length,
                                       int Ptag, int Pclass,
                                       int depth, int *perr)
{
    ASN1_STRING *ret = NULL;
    const unsigned char *p;
    unsigned char *s;
    long len;
    int inf, tag, xclass;

    if (depth > ASN1_BYTES_MAXDEPTH) {
        *perr = ASN1_R_NESTED_ASN1_STRING;
        return NULL;
    }

    if ((a == NULL) || ((*a) == NULL)) {
        if ((ret = ASN1_STRING_new()) == NULL)
            return (NULL);
    } else
        ret = (*a);

    p = *pp;
    inf = ASN1_get_object(&p, &len, &tag, &xclass, length);
    if (inf & 0x80) {
        *perr = ASN1_R_BAD_OBJECT_HEADER;
        goto err;
    }

    if (tag != Ptag) {
        *perr = ASN1_R_WRONG_TAG;
        goto err;
    }

    if (inf & V_ASN1_CONSTRUCTED) {
        ASN1_const_CTX c;

        c.error = 0;
        c.pp = pp;
        c.p = p;
        c.inf = inf;
        c.slen = len;
        c.tag = Ptag;
        c.xclass = Pclass;
        c.max = (length == 0) ? 0 : (p + length);
        if (!asn1_collate_primitive(ret, &c, depth)) {
            *perr = c.error;
            goto err;
        } else {
            p = c.p;
        }
    } else {
        if (len != 0) {
            if ((ret->length < len) || (ret->data == NULL)) {
                s = OPENSSL_malloc((int)len + 1);
                if (s == NULL) {
                    *perr = ERR_R_MALLOC_FAILURE;
                    goto err;
                }
                if (ret->data != NULL)
                    OPENSSL_free(ret->data);
            } else
                s = ret->data;
            memcpy(s, p, (int)len);
            s[len] = '\0';
            p += len;
        } else {
            s = NULL;
            if (ret->data != NULL)
                OPENSSL_free(ret->data);
        }

        ret->length = (int)len;
        ret->data = s;
        ret->type = Ptag;
    }

    if (a != NULL)
        (*a) = ret;
    *pp = p;
    return (ret);
 err:
    if ((ret != NULL) && ((a == NULL) || (*a != ret)))
        ASN1_STRING_free(ret);
    return (NULL);
}

/*
 * We are about to parse 0..n d2i_ASN1_bytes objects, we are to collapse them
 * into the one structure that is then returned
 */
/*
 * There have been a few bug fixes for this function from Paul Keogh
 * <paul.keogh@sse.ie>, many thanks to him
 */
static int asn1_collate_primitive(ASN1_STRING *a, ASN1_const_CTX *c,
                                  int depth)
{
    ASN1_STRING *os = NULL;
    BUF_MEM b;
    int num;

    b.length = 0;
    b.max = 0;
    b.data = NULL;

    if (a == NULL) {
        c->error = ERR_R_PASSED_NULL_PARAMETER;
        goto err;
    }

    num = 0;
    for (;;) {
        if (c->inf & 1) {
            c->eos = ASN1_const_check_infinite_end(&c->p,
                                                   (long)(c->max - c->p));
            if (c->eos)
                break;
        } else {
            if (c->slen <= 0)
                break;
        }

        c->q = c->p;
        if (int_d2i_ASN1_bytes(&os, &c->p, c->max - c->p, c->tag, c->xclass,
                               depth + 1, &c->error) == NULL) {
            goto err;
        }

        if (!BUF_MEM_grow_clean(&b, num + os->length)) {
            c->error = ERR_R_BUF_LIB;
            goto err;
        }
        memcpy(&(b.data[num]), os->data, os->length);
        if (!(c->inf & 1))
            c->slen -= (c->p - c->q);
        num += os->length;
    }

    if (!asn1_const_Finish(c))
        goto err;

    a->length = num;
    if (a->data != NULL)
        OPENSSL_free(a->data);
    a->data = (unsigned char *)b.data;
    if (os != NULL)
        ASN1_STRING_free(os);
    return (1);
 err:
    if (os != NULL)
        ASN1_STRING_free(os);
    if (b.data != NULL)
        OPENSSL_free(b.data);
    return (0);
}
