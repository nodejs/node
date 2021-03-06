/*
 * Copyright 1999-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Internal tests for the asn1 module */

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

int setup_tests(void)
{
    ADD_TEST(test_tbl_standard);
    ADD_TEST(test_standard_methods);
    return 1;
}
