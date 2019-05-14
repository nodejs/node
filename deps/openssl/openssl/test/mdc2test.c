/*
 * Copyright 1995-2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>

#include "internal/nelem.h"
#include "testutil.h"

#if defined(OPENSSL_NO_DES) && !defined(OPENSSL_NO_MDC2)
# define OPENSSL_NO_MDC2
#endif

#ifndef OPENSSL_NO_MDC2
# include <openssl/evp.h>
# include <openssl/mdc2.h>

# ifdef CHARSET_EBCDIC
#  include <openssl/ebcdic.h>
# endif

static unsigned char pad1[16] = {
    0x42, 0xE5, 0x0C, 0xD2, 0x24, 0xBA, 0xCE, 0xBA,
    0x76, 0x0B, 0xDD, 0x2B, 0xD4, 0x09, 0x28, 0x1A
};

static unsigned char pad2[16] = {
    0x2E, 0x46, 0x79, 0xB5, 0xAD, 0xD9, 0xCA, 0x75,
    0x35, 0xD8, 0x7A, 0xFE, 0xAB, 0x33, 0xBE, 0xE2
};

static int test_mdc2(void)
{
    int testresult = 0;
    unsigned char md[MDC2_DIGEST_LENGTH];
    EVP_MD_CTX *c;
    static char text[] = "Now is the time for all ";
    size_t tlen = strlen(text);

# ifdef CHARSET_EBCDIC
    ebcdic2ascii(text, text, tlen);
# endif

    c = EVP_MD_CTX_new();
    if (!TEST_ptr(c)
        || !TEST_true(EVP_DigestInit_ex(c, EVP_mdc2(), NULL))
        || !TEST_true(EVP_DigestUpdate(c, (unsigned char *)text, tlen))
        || !TEST_true(EVP_DigestFinal_ex(c, &(md[0]), NULL))
        || !TEST_mem_eq(md, MDC2_DIGEST_LENGTH, pad1, MDC2_DIGEST_LENGTH)
        || !TEST_true(EVP_DigestInit_ex(c, EVP_mdc2(), NULL)))
        goto end;

    /* FIXME: use a ctl function? */
    ((MDC2_CTX *)EVP_MD_CTX_md_data(c))->pad_type = 2;
    if (!TEST_true(EVP_DigestUpdate(c, (unsigned char *)text, tlen))
        || !TEST_true(EVP_DigestFinal_ex(c, &(md[0]), NULL))
        || !TEST_mem_eq(md, MDC2_DIGEST_LENGTH, pad2, MDC2_DIGEST_LENGTH))
        goto end;

    testresult = 1;
 end:
    EVP_MD_CTX_free(c);
    return testresult;
}
#endif

int setup_tests(void)
{
#ifndef OPENSSL_NO_MDC2
    ADD_TEST(test_mdc2);
#endif
    return 1;
}
