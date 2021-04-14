/*
 * Copyright 1999-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Internal tests for the asn1 module */

/*
 * RSA low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <stdio.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include "testutil.h"
#include "internal/nelem.h"

/**********************************************************************
 *
 * Test of a_strnid's tbl_standard
 *
 ***/

#include "../crypto/asn1/tbl_standard.h"

static int test_tbl_standard(void)
{
    const ASN1_STRING_TABLE *tmp;
    int last_nid = -1;
    size_t i;

    for (tmp = tbl_standard, i = 0; i < OSSL_NELEM(tbl_standard); i++, tmp++) {
        if (tmp->nid < last_nid) {
            last_nid = 0;
            break;
        }
        last_nid = tmp->nid;
    }

    if (TEST_int_ne(last_nid, 0)) {
        TEST_info("asn1 tbl_standard: Table order OK");
        return 1;
    }

    TEST_info("asn1 tbl_standard: out of order");
    for (tmp = tbl_standard, i = 0; i < OSSL_NELEM(tbl_standard); i++, tmp++)
        TEST_note("asn1 tbl_standard: Index %zu, NID %d, Name=%s",
                  i, tmp->nid, OBJ_nid2ln(tmp->nid));

    return 0;
}

/**********************************************************************
 *
 * Test of ameth_lib's standard_methods
 *
 ***/

#include "crypto/asn1.h"
#include "../crypto/asn1/standard_methods.h"

static int test_standard_methods(void)
{
    const EVP_PKEY_ASN1_METHOD **tmp;
    int last_pkey_id = -1;
    size_t i;
    int ok = 1;

    for (tmp = standard_methods, i = 0; i < OSSL_NELEM(standard_methods);
         i++, tmp++) {
        if ((*tmp)->pkey_id < last_pkey_id) {
            last_pkey_id = 0;
            break;
        }
        last_pkey_id = (*tmp)->pkey_id;

        /*
         * One of the following must be true:
         *
         * pem_str == NULL AND ASN1_PKEY_ALIAS is set
         * pem_str != NULL AND ASN1_PKEY_ALIAS is clear
         *
         * Anything else is an error and may lead to a corrupt ASN1 method table
         */
        if (!TEST_true(((*tmp)->pem_str == NULL && ((*tmp)->pkey_flags & ASN1_PKEY_ALIAS) != 0)
                       || ((*tmp)->pem_str != NULL && ((*tmp)->pkey_flags & ASN1_PKEY_ALIAS) == 0))) {
            TEST_note("asn1 standard methods: Index %zu, pkey ID %d, Name=%s",
                      i, (*tmp)->pkey_id, OBJ_nid2sn((*tmp)->pkey_id));
            ok = 0;
        }
    }

    if (TEST_int_ne(last_pkey_id, 0)) {
        TEST_info("asn1 standard methods: Table order OK");
        return ok;
    }

    TEST_note("asn1 standard methods: out of order");
    for (tmp = standard_methods, i = 0; i < OSSL_NELEM(standard_methods);
         i++, tmp++)
        TEST_note("asn1 standard methods: Index %zu, pkey ID %d, Name=%s",
                  i, (*tmp)->pkey_id, OBJ_nid2sn((*tmp)->pkey_id));

    return 0;
}

/**********************************************************************
 *
 * Test of that i2d fail on non-existing non-optional items
 *
 ***/

#include <openssl/rsa.h>

static int test_empty_nonoptional_content(void)
{
    RSA *rsa = NULL;
    BIGNUM *n = NULL;
    BIGNUM *e = NULL;
    int ok = 0;

    if (!TEST_ptr(rsa = RSA_new())
        || !TEST_ptr(n = BN_new())
        || !TEST_ptr(e = BN_new())
        || !TEST_true(RSA_set0_key(rsa, n, e, NULL)))
        goto end;

    n = e = NULL;                /* They are now "owned" by |rsa| */

    /*
     * This SHOULD fail, as we're trying to encode a public key as a private
     * key.  The private key bits MUST be present for a proper RSAPrivateKey.
     */
    if (TEST_int_le(i2d_RSAPrivateKey(rsa, NULL), 0))
        ok = 1;

 end:
    RSA_free(rsa);
    BN_free(n);
    BN_free(e);
    return ok;
}

/**********************************************************************
 *
 * Tests of the Unicode code point range
 *
 ***/

static int test_unicode(const unsigned char *univ, size_t len, int expected)
{
    const unsigned char *end = univ + len;
    int ok = 1;

    for (; univ < end; univ += 4) {
        if (!TEST_int_eq(ASN1_mbstring_copy(NULL, univ, 4, MBSTRING_UNIV,
                                            B_ASN1_UTF8STRING),
                         expected))
            ok = 0;
    }
    return ok;
}

static int test_unicode_range(void)
{
    const unsigned char univ_ok[] = "\0\0\0\0"
                                    "\0\0\xd7\xff"
                                    "\0\0\xe0\x00"
                                    "\0\x10\xff\xff";
    const unsigned char univ_bad[] = "\0\0\xd8\x00"
                                     "\0\0\xdf\xff"
                                     "\0\x11\x00\x00"
                                     "\x80\x00\x00\x00"
                                     "\xff\xff\xff\xff";
    int ok = 1;

    if (!test_unicode(univ_ok, sizeof univ_ok - 1, V_ASN1_UTF8STRING))
        ok = 0;
    if (!test_unicode(univ_bad, sizeof univ_bad - 1, -1))
        ok = 0;
    return ok;
}

int setup_tests(void)
{
    ADD_TEST(test_tbl_standard);
    ADD_TEST(test_standard_methods);
    ADD_TEST(test_empty_nonoptional_content);
    ADD_TEST(test_unicode_range);
    return 1;
}
