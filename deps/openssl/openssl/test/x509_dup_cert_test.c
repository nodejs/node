/*
 * Copyright 2017-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* ====================================================================
 * Copyright (c) 2017, 2018 Oracle and/or its affiliates.  All rights reserved.
 */

#include <stdio.h>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>

static int test_509_dup_cert(const char *cert_f)
{
    int ret = 0;
    X509_STORE_CTX *sctx = NULL;
    X509_STORE *store = NULL;
    X509_LOOKUP *lookup = NULL;

    store = X509_STORE_new();
    if (store == NULL)
        goto err;

    lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
    if (lookup == NULL)
        goto err;

    if (!X509_load_cert_file(lookup, cert_f, X509_FILETYPE_PEM))
        goto err;
    if (!X509_load_cert_file(lookup, cert_f, X509_FILETYPE_PEM))
        goto err;

    ret = 1;

 err:
    X509_STORE_CTX_free(sctx);
    X509_STORE_free(store);
    if (ret != 1)
        ERR_print_errors_fp(stderr);
    return ret;
}

int main(int argc, char **argv)
{
    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    if (argc != 2) {
        fprintf(stderr, "usage: x509_dup_cert_test cert.pem\n");
        return 1;
    }

    if (!test_509_dup_cert(argv[1])) {
        fprintf(stderr, "Test X509 duplicate cert failed\n");
        return 1;
    }

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks_fp(stderr) <= 0)
        return 1;
#endif

    printf("PASS\n");
    return 0;
}
