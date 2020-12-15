/*
 * Copyright 2017-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>

#include <openssl/rand.h>
#include <openssl/asn1t.h>
#include "internal/numbers.h"
#include "testutil.h"

#ifdef __GNUC__
# pragma GCC diagnostic ignored "-Wunused-function"
#endif
#ifdef __clang__
# pragma clang diagnostic ignored "-Wunused-function"
#endif

/* Badly coded ASN.1 INTEGER zero wrapped in a sequence */
static unsigned char t_invalid_zero[] = {
    0x30, 0x02,                  /* SEQUENCE tag + length */
    0x02, 0x00                   /* INTEGER tag + length */
};

#if OPENSSL_API_COMPAT < 0x10200000L
/* LONG case ************************************************************* */

typedef struct {
    long test_long;
} ASN1_LONG_DATA;

ASN1_SEQUENCE(ASN1_LONG_DATA) = {
    ASN1_EMBED(ASN1_LONG_DATA, test_long, LONG),
} static_ASN1_SEQUENCE_END(ASN1_LONG_DATA)

IMPLEMENT_STATIC_ASN1_ENCODE_FUNCTIONS(ASN1_LONG_DATA)
IMPLEMENT_STATIC_ASN1_ALLOC_FUNCTIONS(ASN1_LONG_DATA)

static int test_long(void)
{
    const unsigned char *p = t_invalid_zero;
    ASN1_LONG_DATA *dectst =
        d2i_ASN1_LONG_DATA(NULL, &p, sizeof(t_invalid_zero));

    if (dectst == NULL)
        return 0;                /* Fail */

    ASN1_LONG_DATA_free(dectst);
    return 1;
}
#endif

/* INT32 case ************************************************************* */

typedef struct {
    int32_t test_int32;
} ASN1_INT32_DATA;

ASN1_SEQUENCE(ASN1_INT32_DATA) = {
    ASN1_EMBED(ASN1_INT32_DATA, test_int32, INT32),
} static_ASN1_SEQUENCE_END(ASN1_INT32_DATA)

IMPLEMENT_STATIC_ASN1_ENCODE_FUNCTIONS(ASN1_INT32_DATA)
IMPLEMENT_STATIC_ASN1_ALLOC_FUNCTIONS(ASN1_INT32_DATA)

static int test_int32(void)
{
    const unsigned char *p = t_invalid_zero;
    ASN1_INT32_DATA *dectst =
        d2i_ASN1_INT32_DATA(NULL, &p, sizeof(t_invalid_zero));

    if (dectst == NULL)
        return 0;                /* Fail */

    ASN1_INT32_DATA_free(dectst);
    return 1;
}

/* UINT32 case ************************************************************* */

typedef struct {
    uint32_t test_uint32;
} ASN1_UINT32_DATA;

ASN1_SEQUENCE(ASN1_UINT32_DATA) = {
    ASN1_EMBED(ASN1_UINT32_DATA, test_uint32, UINT32),
} static_ASN1_SEQUENCE_END(ASN1_UINT32_DATA)

IMPLEMENT_STATIC_ASN1_ENCODE_FUNCTIONS(ASN1_UINT32_DATA)
IMPLEMENT_STATIC_ASN1_ALLOC_FUNCTIONS(ASN1_UINT32_DATA)

static int test_uint32(void)
{
    const unsigned char *p = t_invalid_zero;
    ASN1_UINT32_DATA *dectst =
        d2i_ASN1_UINT32_DATA(NULL, &p, sizeof(t_invalid_zero));

    if (dectst == NULL)
        return 0;                /* Fail */

    ASN1_UINT32_DATA_free(dectst);
    return 1;
}

/* INT64 case ************************************************************* */

typedef struct {
    int64_t test_int64;
} ASN1_INT64_DATA;

ASN1_SEQUENCE(ASN1_INT64_DATA) = {
    ASN1_EMBED(ASN1_INT64_DATA, test_int64, INT64),
} static_ASN1_SEQUENCE_END(ASN1_INT64_DATA)

IMPLEMENT_STATIC_ASN1_ENCODE_FUNCTIONS(ASN1_INT64_DATA)
IMPLEMENT_STATIC_ASN1_ALLOC_FUNCTIONS(ASN1_INT64_DATA)

static int test_int64(void)
{
    const unsigned char *p = t_invalid_zero;
    ASN1_INT64_DATA *dectst =
        d2i_ASN1_INT64_DATA(NULL, &p, sizeof(t_invalid_zero));

    if (dectst == NULL)
        return 0;                /* Fail */

    ASN1_INT64_DATA_free(dectst);
    return 1;
}

/* UINT64 case ************************************************************* */

typedef struct {
    uint64_t test_uint64;
} ASN1_UINT64_DATA;

ASN1_SEQUENCE(ASN1_UINT64_DATA) = {
    ASN1_EMBED(ASN1_UINT64_DATA, test_uint64, UINT64),
} static_ASN1_SEQUENCE_END(ASN1_UINT64_DATA)

IMPLEMENT_STATIC_ASN1_ENCODE_FUNCTIONS(ASN1_UINT64_DATA)
IMPLEMENT_STATIC_ASN1_ALLOC_FUNCTIONS(ASN1_UINT64_DATA)

static int test_uint64(void)
{
    const unsigned char *p = t_invalid_zero;
    ASN1_UINT64_DATA *dectst =
        d2i_ASN1_UINT64_DATA(NULL, &p, sizeof(t_invalid_zero));

    if (dectst == NULL)
        return 0;                /* Fail */

    ASN1_UINT64_DATA_free(dectst);
    return 1;
}

typedef struct {
    ASN1_STRING *invalidDirString;
} INVALIDTEMPLATE;

ASN1_SEQUENCE(INVALIDTEMPLATE) = {
    /*
     * DirectoryString is a CHOICE type so it must use explicit tagging -
     * but we deliberately use implicit here, which makes this template invalid.
     */
    ASN1_IMP(INVALIDTEMPLATE, invalidDirString, DIRECTORYSTRING, 12)
} static_ASN1_SEQUENCE_END(INVALIDTEMPLATE)

IMPLEMENT_STATIC_ASN1_ENCODE_FUNCTIONS(INVALIDTEMPLATE)
IMPLEMENT_STATIC_ASN1_ALLOC_FUNCTIONS(INVALIDTEMPLATE)

/* Empty sequence for invalid template test */
static unsigned char t_invalid_template[] = {
    0x30, 0x03,                  /* SEQUENCE tag + length */
    0x0c, 0x01, 0x41             /* UTF8String, length 1, "A" */
};

static int test_invalid_template(void)
{
    const unsigned char *p = t_invalid_template;
    INVALIDTEMPLATE *tmp = d2i_INVALIDTEMPLATE(NULL, &p,
                                               sizeof(t_invalid_template));

    /* We expect a NULL pointer return */
    if (TEST_ptr_null(tmp))
        return 1;

    INVALIDTEMPLATE_free(tmp);
    return 0;
}

int setup_tests(void)
{
#if OPENSSL_API_COMPAT < 0x10200000L
    ADD_TEST(test_long);
#endif
    ADD_TEST(test_int32);
    ADD_TEST(test_uint32);
    ADD_TEST(test_int64);
    ADD_TEST(test_uint64);
    ADD_TEST(test_invalid_template);
    return 1;
}
