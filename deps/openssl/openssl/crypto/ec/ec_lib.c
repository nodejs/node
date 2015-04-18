/* crypto/ec/ec_lib.c */
/*
 * Originally written by Bodo Moeller for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * Binary polynomial ECC support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#include <string.h>

#include <openssl/err.h>
#include <openssl/opensslv.h>

#include "ec_lcl.h"

const char EC_version[] = "EC" OPENSSL_VERSION_PTEXT;

/* functions for EC_GROUP objects */

EC_GROUP *EC_GROUP_new(const EC_METHOD *meth)
{
    EC_GROUP *ret;

    if (meth == NULL) {
        ECerr(EC_F_EC_GROUP_NEW, EC_R_SLOT_FULL);
        return NULL;
    }
    if (meth->group_init == 0) {
        ECerr(EC_F_EC_GROUP_NEW, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return NULL;
    }

    ret = OPENSSL_malloc(sizeof *ret);
    if (ret == NULL) {
        ECerr(EC_F_EC_GROUP_NEW, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    ret->meth = meth;

    ret->extra_data = NULL;

    ret->generator = NULL;
    BN_init(&ret->order);
    BN_init(&ret->cofactor);

    ret->curve_name = 0;
    ret->asn1_flag = 0;
    ret->asn1_form = POINT_CONVERSION_UNCOMPRESSED;

    ret->seed = NULL;
    ret->seed_len = 0;

    if (!meth->group_init(ret)) {
        OPENSSL_free(ret);
        return NULL;
    }

    return ret;
}

void EC_GROUP_free(EC_GROUP *group)
{
    if (!group)
        return;

    if (group->meth->group_finish != 0)
        group->meth->group_finish(group);

    EC_EX_DATA_free_all_data(&group->extra_data);

    if (group->generator != NULL)
        EC_POINT_free(group->generator);
    BN_free(&group->order);
    BN_free(&group->cofactor);

    if (group->seed)
        OPENSSL_free(group->seed);

    OPENSSL_free(group);
}

void EC_GROUP_clear_free(EC_GROUP *group)
{
    if (!group)
        return;

    if (group->meth->group_clear_finish != 0)
        group->meth->group_clear_finish(group);
    else if (group->meth->group_finish != 0)
        group->meth->group_finish(group);

    EC_EX_DATA_clear_free_all_data(&group->extra_data);

    if (group->generator != NULL)
        EC_POINT_clear_free(group->generator);
    BN_clear_free(&group->order);
    BN_clear_free(&group->cofactor);

    if (group->seed) {
        OPENSSL_cleanse(group->seed, group->seed_len);
        OPENSSL_free(group->seed);
    }

    OPENSSL_cleanse(group, sizeof *group);
    OPENSSL_free(group);
}

int EC_GROUP_copy(EC_GROUP *dest, const EC_GROUP *src)
{
    EC_EXTRA_DATA *d;

    if (dest->meth->group_copy == 0) {
        ECerr(EC_F_EC_GROUP_COPY, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (dest->meth != src->meth) {
        ECerr(EC_F_EC_GROUP_COPY, EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    if (dest == src)
        return 1;

    EC_EX_DATA_free_all_data(&dest->extra_data);

    for (d = src->extra_data; d != NULL; d = d->next) {
        void *t = d->dup_func(d->data);

        if (t == NULL)
            return 0;
        if (!EC_EX_DATA_set_data
            (&dest->extra_data, t, d->dup_func, d->free_func,
             d->clear_free_func))
            return 0;
    }

    if (src->generator != NULL) {
        if (dest->generator == NULL) {
            dest->generator = EC_POINT_new(dest);
            if (dest->generator == NULL)
                return 0;
        }
        if (!EC_POINT_copy(dest->generator, src->generator))
            return 0;
    } else {
        /* src->generator == NULL */
        if (dest->generator != NULL) {
            EC_POINT_clear_free(dest->generator);
            dest->generator = NULL;
        }
    }

    if (!BN_copy(&dest->order, &src->order))
        return 0;
    if (!BN_copy(&dest->cofactor, &src->cofactor))
        return 0;

    dest->curve_name = src->curve_name;
    dest->asn1_flag = src->asn1_flag;
    dest->asn1_form = src->asn1_form;

    if (src->seed) {
        if (dest->seed)
            OPENSSL_free(dest->seed);
        dest->seed = OPENSSL_malloc(src->seed_len);
        if (dest->seed == NULL)
            return 0;
        if (!memcpy(dest->seed, src->seed, src->seed_len))
            return 0;
        dest->seed_len = src->seed_len;
    } else {
        if (dest->seed)
            OPENSSL_free(dest->seed);
        dest->seed = NULL;
        dest->seed_len = 0;
    }

    return dest->meth->group_copy(dest, src);
}

EC_GROUP *EC_GROUP_dup(const EC_GROUP *a)
{
    EC_GROUP *t = NULL;
    int ok = 0;

    if (a == NULL)
        return NULL;

    if ((t = EC_GROUP_new(a->meth)) == NULL)
        return (NULL);
    if (!EC_GROUP_copy(t, a))
        goto err;

    ok = 1;

 err:
    if (!ok) {
        if (t)
            EC_GROUP_free(t);
        return NULL;
    } else
        return t;
}

const EC_METHOD *EC_GROUP_method_of(const EC_GROUP *group)
{
    return group->meth;
}

int EC_METHOD_get_field_type(const EC_METHOD *meth)
{
    return meth->field_type;
}

int EC_GROUP_set_generator(EC_GROUP *group, const EC_POINT *generator,
                           const BIGNUM *order, const BIGNUM *cofactor)
{
    if (generator == NULL) {
        ECerr(EC_F_EC_GROUP_SET_GENERATOR, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }

    if (group->generator == NULL) {
        group->generator = EC_POINT_new(group);
        if (group->generator == NULL)
            return 0;
    }
    if (!EC_POINT_copy(group->generator, generator))
        return 0;

    if (order != NULL) {
        if (!BN_copy(&group->order, order))
            return 0;
    } else
        BN_zero(&group->order);

    if (cofactor != NULL) {
        if (!BN_copy(&group->cofactor, cofactor))
            return 0;
    } else
        BN_zero(&group->cofactor);

    return 1;
}

const EC_POINT *EC_GROUP_get0_generator(const EC_GROUP *group)
{
    return group->generator;
}

int EC_GROUP_get_order(const EC_GROUP *group, BIGNUM *order, BN_CTX *ctx)
{
    if (!BN_copy(order, &group->order))
        return 0;

    return !BN_is_zero(order);
}

int EC_GROUP_get_cofactor(const EC_GROUP *group, BIGNUM *cofactor,
                          BN_CTX *ctx)
{
    if (!BN_copy(cofactor, &group->cofactor))
        return 0;

    return !BN_is_zero(&group->cofactor);
}

void EC_GROUP_set_curve_name(EC_GROUP *group, int nid)
{
    group->curve_name = nid;
}

int EC_GROUP_get_curve_name(const EC_GROUP *group)
{
    return group->curve_name;
}

void EC_GROUP_set_asn1_flag(EC_GROUP *group, int flag)
{
    group->asn1_flag = flag;
}

int EC_GROUP_get_asn1_flag(const EC_GROUP *group)
{
    return group->asn1_flag;
}

void EC_GROUP_set_point_conversion_form(EC_GROUP *group,
                                        point_conversion_form_t form)
{
    group->asn1_form = form;
}

point_conversion_form_t EC_GROUP_get_point_conversion_form(const EC_GROUP
                                                           *group)
{
    return group->asn1_form;
}

size_t EC_GROUP_set_seed(EC_GROUP *group, const unsigned char *p, size_t len)
{
    if (group->seed) {
        OPENSSL_free(group->seed);
        group->seed = NULL;
        group->seed_len = 0;
    }

    if (!len || !p)
        return 1;

    if ((group->seed = OPENSSL_malloc(len)) == NULL)
        return 0;
    memcpy(group->seed, p, len);
    group->seed_len = len;

    return len;
}

unsigned char *EC_GROUP_get0_seed(const EC_GROUP *group)
{
    return group->seed;
}

size_t EC_GROUP_get_seed_len(const EC_GROUP *group)
{
    return group->seed_len;
}

int EC_GROUP_set_curve_GFp(EC_GROUP *group, const BIGNUM *p, const BIGNUM *a,
                           const BIGNUM *b, BN_CTX *ctx)
{
    if (group->meth->group_set_curve == 0) {
        ECerr(EC_F_EC_GROUP_SET_CURVE_GFP, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    return group->meth->group_set_curve(group, p, a, b, ctx);
}

int EC_GROUP_get_curve_GFp(const EC_GROUP *group, BIGNUM *p, BIGNUM *a,
                           BIGNUM *b, BN_CTX *ctx)
{
    if (group->meth->group_get_curve == 0) {
        ECerr(EC_F_EC_GROUP_GET_CURVE_GFP, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    return group->meth->group_get_curve(group, p, a, b, ctx);
}

#ifndef OPENSSL_NO_EC2M
int EC_GROUP_set_curve_GF2m(EC_GROUP *group, const BIGNUM *p, const BIGNUM *a,
                            const BIGNUM *b, BN_CTX *ctx)
{
    if (group->meth->group_set_curve == 0) {
        ECerr(EC_F_EC_GROUP_SET_CURVE_GF2M,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    return group->meth->group_set_curve(group, p, a, b, ctx);
}

int EC_GROUP_get_curve_GF2m(const EC_GROUP *group, BIGNUM *p, BIGNUM *a,
                            BIGNUM *b, BN_CTX *ctx)
{
    if (group->meth->group_get_curve == 0) {
        ECerr(EC_F_EC_GROUP_GET_CURVE_GF2M,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    return group->meth->group_get_curve(group, p, a, b, ctx);
}
#endif

int EC_GROUP_get_degree(const EC_GROUP *group)
{
    if (group->meth->group_get_degree == 0) {
        ECerr(EC_F_EC_GROUP_GET_DEGREE, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    return group->meth->group_get_degree(group);
}

int EC_GROUP_check_discriminant(const EC_GROUP *group, BN_CTX *ctx)
{
    if (group->meth->group_check_discriminant == 0) {
        ECerr(EC_F_EC_GROUP_CHECK_DISCRIMINANT,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    return group->meth->group_check_discriminant(group, ctx);
}

int EC_GROUP_cmp(const EC_GROUP *a, const EC_GROUP *b, BN_CTX *ctx)
{
    int r = 0;
    BIGNUM *a1, *a2, *a3, *b1, *b2, *b3;
    BN_CTX *ctx_new = NULL;

    /* compare the field types */
    if (EC_METHOD_get_field_type(EC_GROUP_method_of(a)) !=
        EC_METHOD_get_field_type(EC_GROUP_method_of(b)))
        return 1;
    /* compare the curve name (if present in both) */
    if (EC_GROUP_get_curve_name(a) && EC_GROUP_get_curve_name(b) &&
        EC_GROUP_get_curve_name(a) != EC_GROUP_get_curve_name(b))
        return 1;

    if (!ctx)
        ctx_new = ctx = BN_CTX_new();
    if (!ctx)
        return -1;

    BN_CTX_start(ctx);
    a1 = BN_CTX_get(ctx);
    a2 = BN_CTX_get(ctx);
    a3 = BN_CTX_get(ctx);
    b1 = BN_CTX_get(ctx);
    b2 = BN_CTX_get(ctx);
    b3 = BN_CTX_get(ctx);
    if (!b3) {
        BN_CTX_end(ctx);
        if (ctx_new)
            BN_CTX_free(ctx);
        return -1;
    }

    /*
     * XXX This approach assumes that the external representation of curves
     * over the same field type is the same.
     */
    if (!a->meth->group_get_curve(a, a1, a2, a3, ctx) ||
        !b->meth->group_get_curve(b, b1, b2, b3, ctx))
        r = 1;

    if (r || BN_cmp(a1, b1) || BN_cmp(a2, b2) || BN_cmp(a3, b3))
        r = 1;

    /* XXX EC_POINT_cmp() assumes that the methods are equal */
    if (r || EC_POINT_cmp(a, EC_GROUP_get0_generator(a),
                          EC_GROUP_get0_generator(b), ctx))
        r = 1;

    if (!r) {
        /* compare the order and cofactor */
        if (!EC_GROUP_get_order(a, a1, ctx) ||
            !EC_GROUP_get_order(b, b1, ctx) ||
            !EC_GROUP_get_cofactor(a, a2, ctx) ||
            !EC_GROUP_get_cofactor(b, b2, ctx)) {
            BN_CTX_end(ctx);
            if (ctx_new)
                BN_CTX_free(ctx);
            return -1;
        }
        if (BN_cmp(a1, b1) || BN_cmp(a2, b2))
            r = 1;
    }

    BN_CTX_end(ctx);
    if (ctx_new)
        BN_CTX_free(ctx);

    return r;
}

/* this has 'package' visibility */
int EC_EX_DATA_set_data(EC_EXTRA_DATA **ex_data, void *data,
                        void *(*dup_func) (void *),
                        void (*free_func) (void *),
                        void (*clear_free_func) (void *))
{
    EC_EXTRA_DATA *d;

    if (ex_data == NULL)
        return 0;

    for (d = *ex_data; d != NULL; d = d->next) {
        if (d->dup_func == dup_func && d->free_func == free_func
            && d->clear_free_func == clear_free_func) {
            ECerr(EC_F_EC_EX_DATA_SET_DATA, EC_R_SLOT_FULL);
            return 0;
        }
    }

    if (data == NULL)
        /* no explicit entry needed */
        return 1;

    d = OPENSSL_malloc(sizeof *d);
    if (d == NULL)
        return 0;

    d->data = data;
    d->dup_func = dup_func;
    d->free_func = free_func;
    d->clear_free_func = clear_free_func;

    d->next = *ex_data;
    *ex_data = d;

    return 1;
}

/* this has 'package' visibility */
void *EC_EX_DATA_get_data(const EC_EXTRA_DATA *ex_data,
                          void *(*dup_func) (void *),
                          void (*free_func) (void *),
                          void (*clear_free_func) (void *))
{
    const EC_EXTRA_DATA *d;

    for (d = ex_data; d != NULL; d = d->next) {
        if (d->dup_func == dup_func && d->free_func == free_func
            && d->clear_free_func == clear_free_func)
            return d->data;
    }

    return NULL;
}

/* this has 'package' visibility */
void EC_EX_DATA_free_data(EC_EXTRA_DATA **ex_data,
                          void *(*dup_func) (void *),
                          void (*free_func) (void *),
                          void (*clear_free_func) (void *))
{
    EC_EXTRA_DATA **p;

    if (ex_data == NULL)
        return;

    for (p = ex_data; *p != NULL; p = &((*p)->next)) {
        if ((*p)->dup_func == dup_func && (*p)->free_func == free_func
            && (*p)->clear_free_func == clear_free_func) {
            EC_EXTRA_DATA *next = (*p)->next;

            (*p)->free_func((*p)->data);
            OPENSSL_free(*p);

            *p = next;
            return;
        }
    }
}

/* this has 'package' visibility */
void EC_EX_DATA_clear_free_data(EC_EXTRA_DATA **ex_data,
                                void *(*dup_func) (void *),
                                void (*free_func) (void *),
                                void (*clear_free_func) (void *))
{
    EC_EXTRA_DATA **p;

    if (ex_data == NULL)
        return;

    for (p = ex_data; *p != NULL; p = &((*p)->next)) {
        if ((*p)->dup_func == dup_func && (*p)->free_func == free_func
            && (*p)->clear_free_func == clear_free_func) {
            EC_EXTRA_DATA *next = (*p)->next;

            (*p)->clear_free_func((*p)->data);
            OPENSSL_free(*p);

            *p = next;
            return;
        }
    }
}

/* this has 'package' visibility */
void EC_EX_DATA_free_all_data(EC_EXTRA_DATA **ex_data)
{
    EC_EXTRA_DATA *d;

    if (ex_data == NULL)
        return;

    d = *ex_data;
    while (d) {
        EC_EXTRA_DATA *next = d->next;

        d->free_func(d->data);
        OPENSSL_free(d);

        d = next;
    }
    *ex_data = NULL;
}

/* this has 'package' visibility */
void EC_EX_DATA_clear_free_all_data(EC_EXTRA_DATA **ex_data)
{
    EC_EXTRA_DATA *d;

    if (ex_data == NULL)
        return;

    d = *ex_data;
    while (d) {
        EC_EXTRA_DATA *next = d->next;

        d->clear_free_func(d->data);
        OPENSSL_free(d);

        d = next;
    }
    *ex_data = NULL;
}

/* functions for EC_POINT objects */

EC_POINT *EC_POINT_new(const EC_GROUP *group)
{
    EC_POINT *ret;

    if (group == NULL) {
        ECerr(EC_F_EC_POINT_NEW, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }
    if (group->meth->point_init == 0) {
        ECerr(EC_F_EC_POINT_NEW, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return NULL;
    }

    ret = OPENSSL_malloc(sizeof *ret);
    if (ret == NULL) {
        ECerr(EC_F_EC_POINT_NEW, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    ret->meth = group->meth;

    if (!ret->meth->point_init(ret)) {
        OPENSSL_free(ret);
        return NULL;
    }

    return ret;
}

void EC_POINT_free(EC_POINT *point)
{
    if (!point)
        return;

    if (point->meth->point_finish != 0)
        point->meth->point_finish(point);
    OPENSSL_free(point);
}

void EC_POINT_clear_free(EC_POINT *point)
{
    if (!point)
        return;

    if (point->meth->point_clear_finish != 0)
        point->meth->point_clear_finish(point);
    else if (point->meth->point_finish != 0)
        point->meth->point_finish(point);
    OPENSSL_cleanse(point, sizeof *point);
    OPENSSL_free(point);
}

int EC_POINT_copy(EC_POINT *dest, const EC_POINT *src)
{
    if (dest->meth->point_copy == 0) {
        ECerr(EC_F_EC_POINT_COPY, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (dest->meth != src->meth) {
        ECerr(EC_F_EC_POINT_COPY, EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    if (dest == src)
        return 1;
    return dest->meth->point_copy(dest, src);
}

EC_POINT *EC_POINT_dup(const EC_POINT *a, const EC_GROUP *group)
{
    EC_POINT *t;
    int r;

    if (a == NULL)
        return NULL;

    t = EC_POINT_new(group);
    if (t == NULL)
        return (NULL);
    r = EC_POINT_copy(t, a);
    if (!r) {
        EC_POINT_free(t);
        return NULL;
    } else
        return t;
}

const EC_METHOD *EC_POINT_method_of(const EC_POINT *point)
{
    return point->meth;
}

int EC_POINT_set_to_infinity(const EC_GROUP *group, EC_POINT *point)
{
    if (group->meth->point_set_to_infinity == 0) {
        ECerr(EC_F_EC_POINT_SET_TO_INFINITY,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_SET_TO_INFINITY, EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->point_set_to_infinity(group, point);
}

int EC_POINT_set_Jprojective_coordinates_GFp(const EC_GROUP *group,
                                             EC_POINT *point, const BIGNUM *x,
                                             const BIGNUM *y, const BIGNUM *z,
                                             BN_CTX *ctx)
{
    if (group->meth->point_set_Jprojective_coordinates_GFp == 0) {
        ECerr(EC_F_EC_POINT_SET_JPROJECTIVE_COORDINATES_GFP,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_SET_JPROJECTIVE_COORDINATES_GFP,
              EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->point_set_Jprojective_coordinates_GFp(group, point, x,
                                                              y, z, ctx);
}

int EC_POINT_get_Jprojective_coordinates_GFp(const EC_GROUP *group,
                                             const EC_POINT *point, BIGNUM *x,
                                             BIGNUM *y, BIGNUM *z,
                                             BN_CTX *ctx)
{
    if (group->meth->point_get_Jprojective_coordinates_GFp == 0) {
        ECerr(EC_F_EC_POINT_GET_JPROJECTIVE_COORDINATES_GFP,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_GET_JPROJECTIVE_COORDINATES_GFP,
              EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->point_get_Jprojective_coordinates_GFp(group, point, x,
                                                              y, z, ctx);
}

int EC_POINT_set_affine_coordinates_GFp(const EC_GROUP *group,
                                        EC_POINT *point, const BIGNUM *x,
                                        const BIGNUM *y, BN_CTX *ctx)
{
    if (group->meth->point_set_affine_coordinates == 0) {
        ECerr(EC_F_EC_POINT_SET_AFFINE_COORDINATES_GFP,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_SET_AFFINE_COORDINATES_GFP,
              EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->point_set_affine_coordinates(group, point, x, y, ctx);
}

#ifndef OPENSSL_NO_EC2M
int EC_POINT_set_affine_coordinates_GF2m(const EC_GROUP *group,
                                         EC_POINT *point, const BIGNUM *x,
                                         const BIGNUM *y, BN_CTX *ctx)
{
    if (group->meth->point_set_affine_coordinates == 0) {
        ECerr(EC_F_EC_POINT_SET_AFFINE_COORDINATES_GF2M,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_SET_AFFINE_COORDINATES_GF2M,
              EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->point_set_affine_coordinates(group, point, x, y, ctx);
}
#endif

int EC_POINT_get_affine_coordinates_GFp(const EC_GROUP *group,
                                        const EC_POINT *point, BIGNUM *x,
                                        BIGNUM *y, BN_CTX *ctx)
{
    if (group->meth->point_get_affine_coordinates == 0) {
        ECerr(EC_F_EC_POINT_GET_AFFINE_COORDINATES_GFP,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_GET_AFFINE_COORDINATES_GFP,
              EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->point_get_affine_coordinates(group, point, x, y, ctx);
}

#ifndef OPENSSL_NO_EC2M
int EC_POINT_get_affine_coordinates_GF2m(const EC_GROUP *group,
                                         const EC_POINT *point, BIGNUM *x,
                                         BIGNUM *y, BN_CTX *ctx)
{
    if (group->meth->point_get_affine_coordinates == 0) {
        ECerr(EC_F_EC_POINT_GET_AFFINE_COORDINATES_GF2M,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_GET_AFFINE_COORDINATES_GF2M,
              EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->point_get_affine_coordinates(group, point, x, y, ctx);
}
#endif

int EC_POINT_add(const EC_GROUP *group, EC_POINT *r, const EC_POINT *a,
                 const EC_POINT *b, BN_CTX *ctx)
{
    if (group->meth->add == 0) {
        ECerr(EC_F_EC_POINT_ADD, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if ((group->meth != r->meth) || (r->meth != a->meth)
        || (a->meth != b->meth)) {
        ECerr(EC_F_EC_POINT_ADD, EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->add(group, r, a, b, ctx);
}

int EC_POINT_dbl(const EC_GROUP *group, EC_POINT *r, const EC_POINT *a,
                 BN_CTX *ctx)
{
    if (group->meth->dbl == 0) {
        ECerr(EC_F_EC_POINT_DBL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if ((group->meth != r->meth) || (r->meth != a->meth)) {
        ECerr(EC_F_EC_POINT_DBL, EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->dbl(group, r, a, ctx);
}

int EC_POINT_invert(const EC_GROUP *group, EC_POINT *a, BN_CTX *ctx)
{
    if (group->meth->invert == 0) {
        ECerr(EC_F_EC_POINT_INVERT, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != a->meth) {
        ECerr(EC_F_EC_POINT_INVERT, EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->invert(group, a, ctx);
}

int EC_POINT_is_at_infinity(const EC_GROUP *group, const EC_POINT *point)
{
    if (group->meth->is_at_infinity == 0) {
        ECerr(EC_F_EC_POINT_IS_AT_INFINITY,
              ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_IS_AT_INFINITY, EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->is_at_infinity(group, point);
}

int EC_POINT_is_on_curve(const EC_GROUP *group, const EC_POINT *point,
                         BN_CTX *ctx)
{
    if (group->meth->is_on_curve == 0) {
        ECerr(EC_F_EC_POINT_IS_ON_CURVE, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_IS_ON_CURVE, EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->is_on_curve(group, point, ctx);
}

int EC_POINT_cmp(const EC_GROUP *group, const EC_POINT *a, const EC_POINT *b,
                 BN_CTX *ctx)
{
    if (group->meth->point_cmp == 0) {
        ECerr(EC_F_EC_POINT_CMP, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return -1;
    }
    if ((group->meth != a->meth) || (a->meth != b->meth)) {
        ECerr(EC_F_EC_POINT_CMP, EC_R_INCOMPATIBLE_OBJECTS);
        return -1;
    }
    return group->meth->point_cmp(group, a, b, ctx);
}

int EC_POINT_make_affine(const EC_GROUP *group, EC_POINT *point, BN_CTX *ctx)
{
    if (group->meth->make_affine == 0) {
        ECerr(EC_F_EC_POINT_MAKE_AFFINE, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    if (group->meth != point->meth) {
        ECerr(EC_F_EC_POINT_MAKE_AFFINE, EC_R_INCOMPATIBLE_OBJECTS);
        return 0;
    }
    return group->meth->make_affine(group, point, ctx);
}

int EC_POINTs_make_affine(const EC_GROUP *group, size_t num,
                          EC_POINT *points[], BN_CTX *ctx)
{
    size_t i;

    if (group->meth->points_make_affine == 0) {
        ECerr(EC_F_EC_POINTS_MAKE_AFFINE, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
    for (i = 0; i < num; i++) {
        if (group->meth != points[i]->meth) {
            ECerr(EC_F_EC_POINTS_MAKE_AFFINE, EC_R_INCOMPATIBLE_OBJECTS);
            return 0;
        }
    }
    return group->meth->points_make_affine(group, num, points, ctx);
}

/*
 * Functions for point multiplication. If group->meth->mul is 0, we use the
 * wNAF-based implementations in ec_mult.c; otherwise we dispatch through
 * methods.
 */

int EC_POINTs_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *scalar,
                  size_t num, const EC_POINT *points[],
                  const BIGNUM *scalars[], BN_CTX *ctx)
{
    if (group->meth->mul == 0)
        /* use default */
        return ec_wNAF_mul(group, r, scalar, num, points, scalars, ctx);

    return group->meth->mul(group, r, scalar, num, points, scalars, ctx);
}

int EC_POINT_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *g_scalar,
                 const EC_POINT *point, const BIGNUM *p_scalar, BN_CTX *ctx)
{
    /* just a convenient interface to EC_POINTs_mul() */

    const EC_POINT *points[1];
    const BIGNUM *scalars[1];

    points[0] = point;
    scalars[0] = p_scalar;

    return EC_POINTs_mul(group, r, g_scalar,
                         (point != NULL
                          && p_scalar != NULL), points, scalars, ctx);
}

int EC_GROUP_precompute_mult(EC_GROUP *group, BN_CTX *ctx)
{
    if (group->meth->mul == 0)
        /* use default */
        return ec_wNAF_precompute_mult(group, ctx);

    if (group->meth->precompute_mult != 0)
        return group->meth->precompute_mult(group, ctx);
    else
        return 1;               /* nothing to do, so report success */
}

int EC_GROUP_have_precompute_mult(const EC_GROUP *group)
{
    if (group->meth->mul == 0)
        /* use default */
        return ec_wNAF_have_precompute_mult(group);

    if (group->meth->have_precompute_mult != 0)
        return group->meth->have_precompute_mult(group);
    else
        return 0;               /* cannot tell whether precomputation has
                                 * been performed */
}
