/*
 * Copyright 2016-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_LIBCRYPTO_OBJECTS_OBJ_LOCAL_H)
#define OSSL_LIBCRYPTO_OBJECTS_OBJ_LOCAL_H

#include <openssl/lhash.h>
#include <openssl/objects.h>
#include <openssl/safestack.h>

typedef struct name_funcs_st {
    unsigned long (*hash_func)(const char *name);
    int (*cmp_func)(const char *a, const char *b);
    void (*free_func)(const char *, int, const char *);
} NAME_FUNCS;

DEFINE_STACK_OF(NAME_FUNCS)
DEFINE_LHASH_OF_EX(OBJ_NAME);
typedef struct added_obj_st ADDED_OBJ;
DEFINE_LHASH_OF_EX(ADDED_OBJ);

#endif /* !defined(OSSL_LIBCRYPTO_OBJECTS_OBJ_LOCAL_H) */
