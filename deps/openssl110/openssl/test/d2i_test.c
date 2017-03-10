/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Regression tests for ASN.1 parsing bugs. */

#include <stdio.h>
#include <string.h>

#include "testutil.h"

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#ifndef OPENSSL_NO_CMS
# include <openssl/cms.h>
#endif
#include "e_os.h"

static const ASN1_ITEM *item_type;
static const char *test_file;

typedef enum {
    ASN1_UNKNOWN,
    ASN1_OK,
    ASN1_BIO,
    ASN1_DECODE,
    ASN1_ENCODE,
    ASN1_COMPARE
} expected_error_t;

typedef struct {
    const char *str;
    expected_error_t code;
} error_enum;

static expected_error_t expected_error = ASN1_UNKNOWN;

typedef struct d2i_test_fixture {
    const char *test_case_name;
} D2I_TEST_FIXTURE;

static D2I_TEST_FIXTURE set_up(const char *const test_case_name)
{
    D2I_TEST_FIXTURE fixture;
    fixture.test_case_name = test_case_name;
    return fixture;
}

static int execute_test(D2I_TEST_FIXTURE fixture)
{
    BIO *bio = NULL;
    ASN1_VALUE *value = NULL;
    int ret = 0;
    unsigned char buf[2048];
    const unsigned char *buf_ptr = buf;
    unsigned char *der = NULL;
    int derlen;
    int len;

    if ((bio = BIO_new_file(test_file, "r")) == NULL)
        return 0;

    if (expected_error == ASN1_BIO) {
        value = ASN1_item_d2i_bio(item_type, bio, NULL);
        if (value == NULL)
            ret = 1;
        goto err;
    }

    /*
     * Unless we are testing it we don't use ASN1_item_d2i_bio because it
     * performs sanity checks on the input and can reject it before the
     * decoder is called.
     */
    len = BIO_read(bio, buf, sizeof buf);
    if (len < 0)
        goto err;

    value = ASN1_item_d2i(NULL, &buf_ptr, len, item_type);
    if (value == NULL) {
        if (expected_error == ASN1_DECODE)
            ret = 1;
        goto err;
    }

    derlen = ASN1_item_i2d(value, &der, item_type);

    if (der == NULL || derlen < 0) {
        if (expected_error == ASN1_ENCODE)
            ret = 1;
        goto err;
    }

    if (derlen != len || memcmp(der, buf, derlen) != 0) {
        if (expected_error == ASN1_COMPARE)
            ret = 1;
        goto err;
    }

    if (expected_error == ASN1_OK)
        ret = 1;

 err:
    /* Don't indicate success for memory allocation errors */
    if (ret == 1 && ERR_GET_REASON(ERR_peek_error()) == ERR_R_MALLOC_FAILURE)
        ret = 0;
    BIO_free(bio);
    OPENSSL_free(der);
    ASN1_item_free(value, item_type);
    return ret;
}

static void tear_down(D2I_TEST_FIXTURE fixture)
{
    ERR_print_errors_fp(stderr);
}

#define SETUP_D2I_TEST_FIXTURE() \
    SETUP_TEST_FIXTURE(D2I_TEST_FIXTURE, set_up)

#define EXECUTE_D2I_TEST() \
    EXECUTE_TEST(execute_test, tear_down)

static int test_bad_asn1()
{
    SETUP_D2I_TEST_FIXTURE();
    EXECUTE_D2I_TEST();
}

/*
 * Usage: d2i_test <type> <file>, e.g.
 * d2i_test generalname bad_generalname.der
 */
int main(int argc, char **argv)
{
    int result = 0;
    const char *test_type_name;
    const char *expected_error_string;
    const char *p = getenv("OPENSSL_DEBUG_MEMORY");

    size_t i;
    static ASN1_ITEM_EXP *items[] = {
        ASN1_ITEM_ref(ASN1_ANY),
        ASN1_ITEM_ref(X509),
        ASN1_ITEM_ref(GENERAL_NAME),
        ASN1_ITEM_ref(ASN1_INTEGER),
#ifndef OPENSSL_NO_CMS
        ASN1_ITEM_ref(CMS_ContentInfo)
#endif
    };

    static error_enum expected_errors[] = {
        {"OK", ASN1_OK},
        {"BIO", ASN1_BIO},
        {"decode", ASN1_DECODE},
        {"encode", ASN1_ENCODE},
        {"compare", ASN1_COMPARE}
    };

    if (p != NULL && strcmp(p, "on") == 0)
        CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    if (argc != 4) {
        fprintf(stderr,
                "Usage: d2i_test item_name expected_error file.der\n");
        return 1;
    }

    test_type_name = argv[1];
    expected_error_string = argv[2];
    test_file = argv[3];

    for (i = 0; i < OSSL_NELEM(items); i++) {
        const ASN1_ITEM *it = ASN1_ITEM_ptr(items[i]);
        if (strcmp(test_type_name, it->sname) == 0) {
            item_type = it;
            break;
        }
    }
    if (item_type == NULL) {
        fprintf(stderr, "Unknown type %s\n", test_type_name);
        fprintf(stderr, "Supported types:\n");
        for (i = 0; i < OSSL_NELEM(items); i++) {
            const ASN1_ITEM *it = ASN1_ITEM_ptr(items[i]);
            fprintf(stderr, "\t%s\n", it->sname);
        }
        return 1;
    }

    for (i = 0; i < OSSL_NELEM(expected_errors); i++) {
        if (strcmp(expected_errors[i].str, expected_error_string) == 0) {
            expected_error = expected_errors[i].code;
            break;
        }
    }

    if (expected_error == ASN1_UNKNOWN) {
        fprintf(stderr, "Unknown expected error %s\n", expected_error_string);
        return 1;
    }

    ADD_TEST(test_bad_asn1);

    result = run_tests(argv[0]);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks_fp(stderr) <= 0)
        result = 1;
#endif

    return result;
}
