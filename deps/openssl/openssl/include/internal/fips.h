/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_INTERNAL_FIPS_H
# define OSSL_INTERNAL_FIPS_H
# pragma once

# ifdef FIPS_MODULE

/* Return 1 if the FIPS self tests are running and 0 otherwise */
int ossl_fips_self_testing(void);

/* Deferred KAT tests categories */

/*
 * The Integrity category is used to run test that are required by the
 * integrity check and are a special category that can therefore never
 * really be deferred. Keep it commented here as a reminder.
 * #  define FIPS_DEFERRED_KAT_INTEGRITY 0
 */
#  define FIPS_DEFERRED_KAT_CIPHER 1
#  define FIPS_DEFERRED_KAT_ASYM_CIPHER 2
#  define FIPS_DEFERRED_KAT_ASYM_KEYGEN 3
#  define FIPS_DEFERRED_KAT_KEM 4
#  define FIPS_DEFERRED_KAT_DIGEST 5
#  define FIPS_DEFERRED_KAT_SIGNATURE 6
#  define FIPS_DEFERRED_KAT_KDF 7
#  define FIPS_DEFERRED_KAT_KA 8
/* Currently unused because all MAC tests are satisfied through other tests */
#  define FIPS_DEFERRED_KAT_MAC 9
#  define FIPS_DEFERRED_DRBG 10
#  define FIPS_DEFERRED_MAX 11

struct fips_deferred_test_st {
    const char *algorithm;
    int category;
    int state;
};

#  define FIPS_DEFERRED_TEST_INIT 0
#  define FIPS_DEFERRED_TEST_IN_PROGRESS 1
#  define FIPS_DEFERRED_TEST_PASSED 2
#  define FIPS_DEFERRED_TEST_FAILED 3

typedef struct fips_deferred_test_st FIPS_DEFERRED_TEST;

int FIPS_deferred_self_tests(OSSL_LIB_CTX *libctx, FIPS_DEFERRED_TEST tests[]);

# endif /* FIPS_MODULE */

#endif
