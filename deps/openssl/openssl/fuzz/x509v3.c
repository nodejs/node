/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */
#include <openssl/bio.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include "fuzzer.h"

/*
 * Repeated section references in the string-based extension APIs can cause
 * quadratic output growth. Limit input size
 * to keep individual fuzzing iterations small. See:
 * https://github.com/google/boringssl/blob/f1f2556a5dfa59e147d9d47279cc3f7f8a18b433/fuzz/conf.cc#L22-L25
 * https://issues.chromium.org/issues/42290485
 */
#define MAX_INPUT_SIZE (8 * 1024)

int FuzzerInitialize(int *argc, char ***argv)
{
    return 1;
}

int FuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    BIO *in = NULL;
    CONF *conf = NULL;
    X509 *cert = NULL;
    X509V3_CTX ctx;

    if (len == 0)
        return 0;

    if (len > MAX_INPUT_SIZE)
        len = MAX_INPUT_SIZE;

    in = BIO_new(BIO_s_mem());
    if (in == NULL)
        goto end;

    if ((size_t)BIO_write(in, buf, (int)len) != len)
        goto end;

    conf = NCONF_new(NULL);
    if (conf == NULL)
        goto end;

    if (NCONF_load_bio(conf, in, NULL) <= 0)
        goto end;

    cert = X509_new();
    if (cert != NULL) {
        X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
        X509V3_set_nconf(&ctx, conf);
        X509V3_EXT_add_nconf(conf, &ctx, "default", cert);
        X509_free(cert);
    }

    cert = X509_new();
    if (cert != NULL) {
        X509V3_set_ctx(&ctx, NULL, NULL, NULL, NULL, 0);
        X509V3_set_nconf(&ctx, conf);
        X509V3_EXT_add_nconf(conf, &ctx, "default", cert);
    }

end:
    X509_free(cert);
    NCONF_free(conf);
    BIO_free(in);
    ERR_clear_error();

    return 0;
}

void FuzzerCleanup(void)
{
}
