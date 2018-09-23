/*
 * Copyright 2015-2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/crypto.h>

static long saved_argl;
static void *saved_argp;
static int saved_idx;
static int saved_idx2;

/*
 * SIMPLE EX_DATA IMPLEMENTATION
 * Apps explicitly set/get ex_data as needed
 */

static void exnew(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
          int idx, long argl, void *argp)
{
    OPENSSL_assert(idx == saved_idx);
    OPENSSL_assert(argl == saved_argl);
    OPENSSL_assert(argp == saved_argp);
    OPENSSL_assert(ptr == NULL);
}

static int exdup(CRYPTO_EX_DATA *to, const CRYPTO_EX_DATA *from,
          void *from_d, int idx, long argl, void *argp)
{
    OPENSSL_assert(idx == saved_idx);
    OPENSSL_assert(argl == saved_argl);
    OPENSSL_assert(argp == saved_argp);
    OPENSSL_assert(from_d != NULL);
    return 1;
}

static void exfree(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
            int idx, long argl, void *argp)
{
    OPENSSL_assert(idx == saved_idx);
    OPENSSL_assert(argl == saved_argl);
    OPENSSL_assert(argp == saved_argp);
}

/*
 * PRE-ALLOCATED EX_DATA IMPLEMENTATION
 * Extended data structure is allocated in exnew2/freed in exfree2
 * Data is stored inside extended data structure
 */

typedef struct myobj_ex_data_st {
    char *hello;
    int new;
    int dup;
} MYOBJ_EX_DATA;

static void exnew2(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
          int idx, long argl, void *argp)
{
    int ret;
    MYOBJ_EX_DATA *ex_data;

    OPENSSL_assert(idx == saved_idx2);
    OPENSSL_assert(argl == saved_argl);
    OPENSSL_assert(argp == saved_argp);
    OPENSSL_assert(ptr == NULL);

    ex_data = OPENSSL_zalloc(sizeof(*ex_data));
    OPENSSL_assert(ex_data != NULL);
    ret = CRYPTO_set_ex_data(ad, saved_idx2, ex_data);
    OPENSSL_assert(ret);

    ex_data->new = 1;
}

static int exdup2(CRYPTO_EX_DATA *to, const CRYPTO_EX_DATA *from,
          void *from_d, int idx, long argl, void *argp)
{
    MYOBJ_EX_DATA **update_ex_data = (MYOBJ_EX_DATA**)from_d;
    MYOBJ_EX_DATA *ex_data = CRYPTO_get_ex_data(to, saved_idx2);

    OPENSSL_assert(idx == saved_idx2);
    OPENSSL_assert(argl == saved_argl);
    OPENSSL_assert(argp == saved_argp);
    OPENSSL_assert(from_d != NULL);
    OPENSSL_assert(*update_ex_data != NULL);
    OPENSSL_assert(ex_data != NULL);
    OPENSSL_assert(ex_data->new);

    /* Copy hello over */
    ex_data->hello = (*update_ex_data)->hello;
    /* indicate this is a dup */
    ex_data->dup = 1;
    /* Keep my original ex_data */
    *update_ex_data = ex_data;
    return 1;
}

static void exfree2(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
            int idx, long argl, void *argp)
{
    MYOBJ_EX_DATA *ex_data = CRYPTO_get_ex_data(ad, saved_idx2);
    int ret;

    OPENSSL_assert(ex_data != NULL);
    OPENSSL_free(ex_data);
    OPENSSL_assert(idx == saved_idx2);
    OPENSSL_assert(argl == saved_argl);
    OPENSSL_assert(argp == saved_argp);
    ret = CRYPTO_set_ex_data(ad, saved_idx2, NULL);
    OPENSSL_assert(ret);
}

typedef struct myobj_st {
    CRYPTO_EX_DATA ex_data;
    int id;
    int st;
} MYOBJ;

static MYOBJ *MYOBJ_new()
{
    static int count = 0;
    MYOBJ *obj = OPENSSL_malloc(sizeof(*obj));

    obj->id = ++count;
    obj->st = CRYPTO_new_ex_data(CRYPTO_EX_INDEX_APP, obj, &obj->ex_data);
    OPENSSL_assert(obj->st != 0);
    return obj;
}

static void MYOBJ_sethello(MYOBJ *obj, char *cp)
{
    obj->st = CRYPTO_set_ex_data(&obj->ex_data, saved_idx, cp);
    OPENSSL_assert(obj->st != 0);
}

static char *MYOBJ_gethello(MYOBJ *obj)
{
    return CRYPTO_get_ex_data(&obj->ex_data, saved_idx);
}

static void MYOBJ_sethello2(MYOBJ *obj, char *cp)
{
    MYOBJ_EX_DATA* ex_data = CRYPTO_get_ex_data(&obj->ex_data, saved_idx2);
    if (ex_data != NULL)
        ex_data->hello = cp;
    else
        obj->st = 0;
}

static char *MYOBJ_gethello2(MYOBJ *obj)
{
    MYOBJ_EX_DATA* ex_data = CRYPTO_get_ex_data(&obj->ex_data, saved_idx2);
    if (ex_data != NULL)
        return ex_data->hello;

    obj->st = 0;
    return NULL;
}

static void MYOBJ_free(MYOBJ *obj)
{
    CRYPTO_free_ex_data(CRYPTO_EX_INDEX_APP, obj, &obj->ex_data);
    OPENSSL_free(obj);
}

static MYOBJ *MYOBJ_dup(MYOBJ *in)
{
    MYOBJ *obj = MYOBJ_new();

    obj->st = CRYPTO_dup_ex_data(CRYPTO_EX_INDEX_APP, &obj->ex_data,
                                 &in->ex_data);
    OPENSSL_assert(obj->st != 0);
    return obj;
}

int main()
{
    MYOBJ *t1, *t2, *t3;
    MYOBJ_EX_DATA *ex_data;
    const char *cp;
    char *p;

    p = OPENSSL_strdup("hello world");
    saved_argl = 21;
    saved_argp = OPENSSL_malloc(1);
    saved_idx = CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_APP,
                                        saved_argl, saved_argp,
                                        exnew, exdup, exfree);
    saved_idx2 = CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_APP,
                                         saved_argl, saved_argp,
                                         exnew2, exdup2, exfree2);
    t1 = MYOBJ_new();
    t2 = MYOBJ_new();
    OPENSSL_assert(t1->st && t2->st);
    ex_data = CRYPTO_get_ex_data(&t1->ex_data, saved_idx2);
    OPENSSL_assert(ex_data != NULL);
    ex_data = CRYPTO_get_ex_data(&t2->ex_data, saved_idx2);
    OPENSSL_assert(ex_data != NULL);
    MYOBJ_sethello(t1, p);
    cp = MYOBJ_gethello(t1);
    OPENSSL_assert(cp == p);
    cp = MYOBJ_gethello(t2);
    OPENSSL_assert(cp == NULL);
    MYOBJ_sethello2(t1, p);
    cp = MYOBJ_gethello2(t1);
    OPENSSL_assert(cp == p);
    OPENSSL_assert(t1->st);
    cp = MYOBJ_gethello2(t2);
    OPENSSL_assert(cp == NULL);
    OPENSSL_assert(t2->st);
    t3 = MYOBJ_dup(t1);
    ex_data = CRYPTO_get_ex_data(&t3->ex_data, saved_idx2);
    OPENSSL_assert(ex_data != NULL);
    OPENSSL_assert(ex_data->dup);
    cp = MYOBJ_gethello(t3);
    OPENSSL_assert(cp == p);
    cp = MYOBJ_gethello2(t3);
    OPENSSL_assert(cp == p);
    OPENSSL_assert(t3->st);
    MYOBJ_free(t1);
    MYOBJ_free(t2);
    MYOBJ_free(t3);
    OPENSSL_free(saved_argp);
    OPENSSL_free(p);
    return 0;
}
