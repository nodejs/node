/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */

/*
 * Test PKCS12 parsing with fuzzed input.
 */

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include "fuzzer.h"

int FuzzerInitialize(int *argc, char ***argv)
{
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
    ERR_clear_error();
    CRYPTO_free_ex_index(0, -1);

    return 1;
}

int FuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    PKCS12 *p12;
    BIO *in;
    EVP_PKEY *pkey = NULL;
    X509 *cert = NULL;
    STACK_OF(X509) *ca = NULL;

    if (len == 0 || len > INT_MAX)
        return 0;

    in = BIO_new(BIO_s_mem());
    if (in == NULL) {
        ERR_clear_error();
        return 0;
    }
    if ((size_t)BIO_write(in, buf, (int)len) != len) {
        BIO_free(in);
        ERR_clear_error();
        return 0;
    }
    p12 = d2i_PKCS12_bio(in, NULL);
    if (p12 != NULL) {
        PKCS12_verify_mac(p12, NULL, 0);
        PKCS12_verify_mac(p12, "", 0);

        PKCS12_parse(p12, NULL, &pkey, &cert, &ca);
        EVP_PKEY_free(pkey);
        X509_free(cert);
        OSSL_STACK_OF_X509_free(ca);

        pkey = NULL;
        cert = NULL;
        ca = NULL;
        PKCS12_parse(p12, "", &pkey, &cert, &ca);
        EVP_PKEY_free(pkey);
        X509_free(cert);
        OSSL_STACK_OF_X509_free(ca);

        PKCS12_free(p12);
    }

    BIO_free(in);
    ERR_clear_error();

    return 0;
}

void FuzzerCleanup(void)
{
}
