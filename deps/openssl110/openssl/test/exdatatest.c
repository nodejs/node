/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/crypto.h>

static long sargl;
static void *sargp;
static int sidx;

static void exnew(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
          int idx, long argl, void *argp)
{
    assert(idx == sidx);
    assert(argl == sargl);
    assert(argp == sargp);
}

static int exdup(CRYPTO_EX_DATA *to, CRYPTO_EX_DATA *from,
          void *from_d, int idx, long argl, void *argp)
{
    assert(idx == sidx);
    assert(argl == sargl);
    assert(argp == sargp);
    return 0;
}

static void exfree(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
            int idx, long argl, void *argp)
{
    assert(idx == sidx);
    assert(argl == sargl);
    assert(argp == sargp);
}

typedef struct myobj_st {
    CRYPTO_EX_DATA ex_data;
    int id;
} MYOBJ;

static MYOBJ *MYOBJ_new()
{
    static int count = 0;
    MYOBJ *obj = OPENSSL_malloc(sizeof(*obj));
    int st;

    obj->id = ++count;
    st = CRYPTO_new_ex_data(CRYPTO_EX_INDEX_APP, obj, &obj->ex_data);
    assert(st != 0);
    return obj;
}

static void MYOBJ_sethello(MYOBJ *obj, char *cp)
{
    int st;

    st = CRYPTO_set_ex_data(&obj->ex_data, sidx, cp);
    assert(st != 0);
}

static char *MYOBJ_gethello(MYOBJ *obj)
{
    return CRYPTO_get_ex_data(&obj->ex_data, sidx);
}

static void MYOBJ_free(MYOBJ *obj)
{
    CRYPTO_free_ex_data(CRYPTO_EX_INDEX_APP, obj, &obj->ex_data);
    OPENSSL_free(obj);
}

int main()
{
    MYOBJ *t1, *t2;
    const char *cp;
    char *p;

    p = strdup("hello world");
    sargl = 21;
    sargp = malloc(1);
    sidx = CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_APP, sargl, sargp,
                                   exnew, exdup, exfree);
    t1 = MYOBJ_new();
    t2 = MYOBJ_new();
    MYOBJ_sethello(t1, p);
    cp = MYOBJ_gethello(t1);
    assert(cp == p);
    cp = MYOBJ_gethello(t2);
    assert(cp == NULL);
    MYOBJ_free(t1);
    MYOBJ_free(t2);
    free(sargp);
    free(p);
    return 0;
}
