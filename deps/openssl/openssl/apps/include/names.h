/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_APPS_INCLUDE_NAMES_H)
#define OSSL_APPS_INCLUDE_NAMES_H

#include <openssl/evp.h>
#include <openssl/safestack.h>
#include <openssl/types.h>

/* Standard comparing function for names */
int name_cmp(const char *const *a, const char *const *b);
/* collect_names is meant to be used with EVP_{type}_doall_names */
void collect_names(const char *name, void *vdata);
/* Sorts and prints a stack of names to |out| */
void print_names(BIO *out, STACK_OF(OPENSSL_CSTRING) *names);

#endif /* !defined(OSSL_APPS_INCLUDE_NAMES_H) */
