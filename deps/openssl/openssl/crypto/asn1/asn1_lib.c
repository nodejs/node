/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <limits.h>
#include "internal/cryptlib.h"
#include <openssl/asn1.h>
#include "asn1_local.h"

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ASN1_HAVE_ASAN 1
#endif
#if __has_feature(memory_sanitizer)
#define ASN1_HAVE_MSAN 1
#endif
#endif /* defined(__has_feature) */
#if defined(__SANITIZE_ADDRESS__) && !defined(ASN1_HAVE_ASAN)
#define ASN1_HAVE_ASAN 1
#endif
#if defined(ASN1_HAVE_ASAN)
#include <sanitizer/asan_interface.h>
#endif
#if defined(ASN1_HAVE_MSAN)
#include <sanitizer/msan_interface.h>
#endif
#if !defined OPENSSL_NO_VALGRIND_CHECK && defined __has_include
/* Any compiler you're going to run valgrind on has this */
#if __has_include(<valgrind/memcheck.h>)
#include <valgrind/memcheck.h>
#include "internal/thread_once.h"
#define ASN1_HAVE_VALGRIND 1
#endif
#endif /* defined(__has_include) */

static int asn1_get_length(const unsigned char **pp, int *inf, long *rl,
    long max);
static void asn1_put_length(unsigned char **pp, int length);

static int _asn1_check_infinite_end(const unsigned char **p, long len)
{
    /*
     * If there is 0 or 1 byte left, the length check should pick things up
     */
    if (len <= 0) {
        return 1;
    } else {
        if ((len >= 2) && ((*p)[0] == 0) && ((*p)[1] == 0)) {
            (*p) += 2;
            return 1;
        }
    }
    return 0;
}

int ASN1_check_infinite_end(unsigned char **p, long len)
{
    return _asn1_check_infinite_end((const unsigned char **)p, len);
}

int ASN1_const_check_infinite_end(const unsigned char **p, long len)
{
    return _asn1_check_infinite_end(p, len);
}

int ASN1_get_object(const unsigned char **pp, long *plength, int *ptag,
    int *pclass, long omax)
{
    int i, ret;
    long len;
    const unsigned char *p = *pp;
    int tag, xclass, inf;
    long max = omax;

    if (omax <= 0) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_TOO_SMALL);
        return 0x80;
    }
    ret = (*p & V_ASN1_CONSTRUCTED);
    xclass = (*p & V_ASN1_PRIVATE);
    i = *p & V_ASN1_PRIMITIVE_TAG;
    if (i == V_ASN1_PRIMITIVE_TAG) { /* high-tag */
        p++;
        if (--max == 0)
            goto err;
        len = 0;
        while (*p & 0x80) {
            len <<= 7L;
            len |= *(p++) & 0x7f;
            if (--max == 0)
                goto err;
            if (len > (INT_MAX >> 7L))
                goto err;
        }
        len <<= 7L;
        len |= *(p++) & 0x7f;
        tag = (int)len;
        if (--max == 0)
            goto err;
    } else {
        tag = i;
        p++;
        if (--max == 0)
            goto err;
    }
    *ptag = tag;
    *pclass = xclass;
    if (!asn1_get_length(&p, &inf, plength, max))
        goto err;

    if (inf && !(ret & V_ASN1_CONSTRUCTED))
        goto err;

    if (*plength > (omax - (p - *pp))) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_TOO_LONG);
        /*
         * Set this so that even if things are not long enough the values are
         * set correctly
         */
        ret |= 0x80;
    }
    *pp = p;
    return ret | inf;
err:
    ERR_raise(ERR_LIB_ASN1, ASN1_R_HEADER_TOO_LONG);
    return 0x80;
}

/*
 * Decode a length field.
 * The short form is a single byte defining a length 0 - 127.
 * The long form is a byte 0 - 127 with the top bit set and this indicates
 * the number of following octets that contain the length.  These octets
 * are stored most significant digit first.
 */
static int asn1_get_length(const unsigned char **pp, int *inf, long *rl,
    long max)
{
    const unsigned char *p = *pp;
    unsigned long ret = 0;
    int i;

    if (max-- < 1)
        return 0;
    if (*p == 0x80) {
        *inf = 1;
        p++;
    } else {
        *inf = 0;
        i = *p & 0x7f;
        if (*p++ & 0x80) {
            if (max < i)
                return 0;
            /* Skip leading zeroes */
            while (i > 0 && *p == 0) {
                p++;
                i--;
            }
            if (i > (int)sizeof(long))
                return 0;
            while (i > 0) {
                ret <<= 8;
                ret |= *p++;
                i--;
            }
            if (ret > LONG_MAX)
                return 0;
        } else {
            ret = i;
        }
    }
    *pp = p;
    *rl = (long)ret;
    return 1;
}

/*
 * constructed == 2 for indefinite length constructed
 */
void ASN1_put_object(unsigned char **pp, int constructed, int length, int tag,
    int xclass)
{
    unsigned char *p = *pp;
    int i, ttag;

    i = (constructed) ? V_ASN1_CONSTRUCTED : 0;
    i |= (xclass & V_ASN1_PRIVATE);
    if (tag < 31) {
        *(p++) = i | (tag & V_ASN1_PRIMITIVE_TAG);
    } else {
        *(p++) = i | V_ASN1_PRIMITIVE_TAG;
        for (i = 0, ttag = tag; ttag > 0; i++)
            ttag >>= 7;
        ttag = i;
        while (i-- > 0) {
            p[i] = tag & 0x7f;
            if (i != (ttag - 1))
                p[i] |= 0x80;
            tag >>= 7;
        }
        p += ttag;
    }
    if (constructed == 2)
        *(p++) = 0x80;
    else
        asn1_put_length(&p, length);
    *pp = p;
}

int ASN1_put_eoc(unsigned char **pp)
{
    unsigned char *p = *pp;

    *p++ = 0;
    *p++ = 0;
    *pp = p;
    return 2;
}

static void asn1_put_length(unsigned char **pp, int length)
{
    unsigned char *p = *pp;
    int i, len;

    if (length <= 127) {
        *(p++) = (unsigned char)length;
    } else {
        len = length;
        for (i = 0; len > 0; i++)
            len >>= 8;
        *(p++) = i | 0x80;
        len = i;
        while (i-- > 0) {
            p[i] = length & 0xff;
            length >>= 8;
        }
        p += len;
    }
    *pp = p;
}

int ASN1_object_size(int constructed, int length, int tag)
{
    int ret = 1;

    if (length < 0)
        return -1;
    if (tag >= 31) {
        while (tag > 0) {
            tag >>= 7;
            ret++;
        }
    }
    if (constructed == 2) {
        ret += 3;
    } else {
        ret++;
        if (length > 127) {
            int tmplen = length;
            while (tmplen > 0) {
                tmplen >>= 8;
                ret++;
            }
        }
    }
    if (ret >= INT_MAX - length)
        return -1;
    return ret + length;
}

void ossl_asn1_bit_string_clear_unused_bits(ASN1_STRING *str)
{
    str->flags &= ~0x07;
    str->flags &= ~ASN1_STRING_FLAG_BITS_LEFT;
}

void ossl_asn1_bit_string_set_unused_bits(ASN1_STRING *str, unsigned int num)
{
    ossl_asn1_bit_string_clear_unused_bits(str);
    str->flags |= ASN1_STRING_FLAG_BITS_LEFT | (num & 0x07);
}

int ASN1_STRING_copy(ASN1_STRING *dst, const ASN1_STRING *str)
{
    if (str == NULL || str->length < 0)
        return 0;
    dst->type = str->type;
    if (!ossl_asn1_string_set_internal(dst, str->data, str->length,
            /*add_nul_byte=*/1))
        return 0;
    /* Copy flags but preserve embed value */
    dst->flags &= ASN1_STRING_FLAG_EMBED;
    dst->flags |= str->flags & ~ASN1_STRING_FLAG_EMBED;
    return 1;
}

ASN1_STRING *ASN1_STRING_dup(const ASN1_STRING *str)
{
    ASN1_STRING *ret;

    if (!str)
        return NULL;
    ret = ASN1_STRING_new();
    if (ret == NULL)
        return NULL;
    if (!ASN1_STRING_copy(ret, str)) {
        ASN1_STRING_free(ret);
        return NULL;
    }
    return ret;
}

#if defined(ASN1_HAVE_VALGRIND)
static CRYPTO_ONCE valgrind_once = CRYPTO_ONCE_STATIC_INIT;
static int valgrind_present = 0;

DEFINE_RUN_ONCE_STATIC(detect_valgrind)
{
    valgrind_present = RUNNING_ON_VALGRIND != 0;
    return 1;
}

static int under_valgrind(void)
{
    return RUN_ONCE(&valgrind_once, detect_valgrind) && valgrind_present;
}
#endif /* defined(ASN1_HAVE_VALGRIND) */

/**
 * @brief Mark the NUL terminator at p as inaccessible to memory checkers.
 * Under AddressSanitizer, MemorySanitizer and Valgrind memcheck a read of
 * the byte is reported as an error, so C-string use of ASN1_STRING data is
 * caught while the byte stays present for builds without a checker. The
 * Valgrind client requests are compiled in wherever its header is found and
 * are issued only when the process is running under Valgrind.
 * A poisoned byte needs no unpoisoning before free(): every checker marks
 * the whole block on free without regard to its previous state.
 * @param p the terminator byte
 */
static void poison_terminator(uint8_t *p)
{
#if defined(ASN1_HAVE_ASAN)
    ASAN_POISON_MEMORY_REGION(p, 1);
#endif
#if defined(ASN1_HAVE_MSAN)
    __msan_poison(p, 1);
#endif
#if defined(ASN1_HAVE_VALGRIND)
    if (under_valgrind())
        VALGRIND_MAKE_MEM_NOACCESS(p, 1);
#endif
}

/**
 * @brief Make the byte at p accessible again before it is written.
 * @param p the byte about to hold a NUL terminator
 */
static void unpoison_terminator(uint8_t *p)
{
#if defined(ASN1_HAVE_ASAN)
    ASAN_UNPOISON_MEMORY_REGION(p, 1);
#endif
#if defined(ASN1_HAVE_MSAN)
    __msan_unpoison(p, 1);
#endif
#if defined(ASN1_HAVE_VALGRIND)
    if (under_valgrind())
        VALGRIND_MAKE_MEM_UNDEFINED(p, 1);
#endif
}

static void unpoison_buffer(uint8_t *buf, size_t buf_len)
{
#if defined(ASN1_HAVE_VALGRIND)
    if (under_valgrind())
        VALGRIND_MAKE_MEM_UNDEFINED(buf, buf_len);
#endif
}

int ossl_asn1_string_set_internal(ASN1_STRING *str, const uint8_t *data,
    int len_in, int add_nul_byte)
{
    size_t len, alloc_len;

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    /*
     * Force no NUL byte for callers that are requesting it
     * 0 length object data will be NULL
     */
    add_nul_byte = 0;
#endif
    if (len_in < -1) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_TOO_SMALL);
        return 0;
    }
    if (len_in == -1) {
        if (data == NULL)
            return 0;
        len = strlen((const char *)data);
    } else {
        len = (size_t)len_in;
    }
    /*
     * Add one to the length to allow for adding an a '\0' terminator
     * "even though this isn't strictly necessary".
     */
    alloc_len = add_nul_byte ? len + 1 : len;

    if (alloc_len > INT_MAX) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_TOO_LARGE);
        return 0;
    }

    if ((str->flags & ASN1_STRING_FLAG_DATA_NOT_OWNED)) {
        str->data = NULL;
        str->length = 0;
        str->flags &= ~ASN1_STRING_FLAG_DATA_NOT_OWNED;
    }

    /* Ensure copying a 0 length data field is defined. */
    if (alloc_len == 0) {
        OPENSSL_free(str->data);
        str->data = NULL;
        str->length = 0;
        return 1;
    }

    if ((size_t)str->length != alloc_len) {
        uint8_t *c;
        c = OPENSSL_realloc(str->length == 0 ? NULL : str->data, alloc_len);
        if (c == NULL)
            return 0;
        str->data = c;
        unpoison_buffer(str->data, alloc_len);
    }
    /* length never includes the added \0 byte */
    str->length = (int)len;

    if (data != NULL && str->data != NULL)
        memcpy(str->data, data, len);
    if (add_nul_byte) {
        /*
         * The terminator byte lies beyond str->length. It is written only
         * when data is supplied, and is inaccessible to memory checkers
         * either way; see poison_terminator().
         */
        unpoison_terminator(&str->data[len]);
        if (data != NULL)
            str->data[len] = '\0';
        poison_terminator(&str->data[len]);
    }
    ossl_asn1_bit_string_clear_unused_bits(str);

    return 1;
}

#ifndef OPENSSL_NO_DEPRECATED_4_1
int ASN1_STRING_set(ASN1_STRING *str, const void *_data, int len_in)
{
    return ossl_asn1_string_set_internal(str, (const uint8_t *)_data, len_in,
        /*add_nul_byte=*/1);
}
#endif

void ASN1_STRING_set0(ASN1_STRING *str, void *data, int len)
{
    if (!(str->flags & ASN1_STRING_FLAG_DATA_NOT_OWNED)) {
        if (str->length > 0)
            OPENSSL_clear_free(str->data, str->length);
        else
            OPENSSL_free(str->data);
    }
    str->flags &= ~ASN1_STRING_FLAG_DATA_NOT_OWNED;
    str->data = data;
    str->length = len < 0 ? 0 : len;
}

int ASN1_STRING_set1_data(ASN1_STRING *str, const uint8_t *data, size_t len_in)
{
    if (str->type == V_ASN1_BIT_STRING) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_ILLEGAL_BITSTRING_FORMAT);
        return 0;
    }
    /* This will go away once ASN1_STRING can size_t internally */
    if (len_in > INT_MAX) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_TOO_LARGE);
        return 0;
    }
    return ossl_asn1_string_set_internal(str, data, (int)len_in, /*add_nul_byte=*/0);
}

int ASN1_STRING_set1_string(ASN1_STRING *str, const char *c_string)
{
    return ASN1_STRING_set1_data(str, (const uint8_t *)c_string,
        strlen(c_string));
}

int ossl_asn1_string_set1_data(ASN1_STRING *str, const uint8_t *data,
    size_t len_in)
{
    if (str->type == V_ASN1_BIT_STRING) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_ILLEGAL_BITSTRING_FORMAT);
        return 0;
    }
    /* This will go away once ASN1_STRING can size_t internally */
    if (len_in > INT_MAX) {
        ERR_raise(ERR_LIB_ASN1, ASN1_R_TOO_LARGE);
        return 0;
    }
    return ossl_asn1_string_set_internal(str, data, (int)len_in, /*add_nul_byte=*/1);
}

int ossl_asn1_string_set1_string(ASN1_STRING *str, const char *c_string)
{
    return ossl_asn1_string_set1_data(str, (const uint8_t *)c_string,
        strlen(c_string));
}

ASN1_STRING *ASN1_STRING_new(void)
{
    return ASN1_STRING_type_new(V_ASN1_OCTET_STRING);
}

ASN1_STRING *ASN1_STRING_type_new(int type)
{
    ASN1_STRING *ret;

    ret = OPENSSL_zalloc(sizeof(*ret));
    if (ret == NULL)
        return NULL;
    ret->type = type;
    return ret;
}

ASN1_STRING *ASN1_STRING_new_not_owned(int type, const uint8_t *data,
    size_t length)
{
    ASN1_STRING *ret;

    if (type == V_ASN1_BIT_STRING)
        return NULL;

    if (data == NULL || length == 0)
        return NULL;

    if (length > INT_MAX)
        return NULL;

    ret = OPENSSL_zalloc(sizeof(*ret));
    if (ret == NULL)
        return NULL;

    ret->type = type;
    ret->data = (unsigned char *)data;
    ret->length = (int)length;
    ret->flags |= ASN1_STRING_FLAG_DATA_NOT_OWNED;

    return ret;
}

void ossl_asn1_string_free_internal(ASN1_STRING *a, int clear, int embed)
{
    if (a == NULL)
        return;

    if ((a->flags & ASN1_STRING_FLAG_DATA_NOT_OWNED)) {
        a->data = NULL;
        a->length = 0;
        a->flags &= ~ASN1_STRING_FLAG_DATA_NOT_OWNED;
    }

    if (!(a->flags & ASN1_STRING_FLAG_NDEF)) {
        if (clear && a->length > 0)
            OPENSSL_clear_free(a->data, a->length);
        else
            OPENSSL_free(a->data);
    }
    /*
     * TODO(beck): Add an assert here to verify that the embed arg is
     * always set to match the flag, and then get rid of the arg.
     */
    if (!embed && !(a->flags & ASN1_STRING_FLAG_EMBED)) {
        if (clear)
            OPENSSL_clear_free(a, sizeof(*a));
        else
            OPENSSL_free(a);
    }
}

void ASN1_STRING_free(ASN1_STRING *a)
{
    if (a == NULL)
        return;

    ossl_asn1_string_free_internal(a, 0, a->flags & ASN1_STRING_FLAG_EMBED);
}

void ASN1_STRING_clear_free(ASN1_STRING *a)
{
    if (a == NULL)
        return;

    ossl_asn1_string_free_internal(a, 1, a->flags & ASN1_STRING_FLAG_EMBED);
}

int ASN1_STRING_cmp(const ASN1_STRING *a, const ASN1_STRING *b)
{
    int i;

    i = (a->length - b->length);
    if (i == 0) {
        if (a->length != 0)
            i = memcmp(a->data, b->data, a->length);
        if (i == 0)
            return a->type - b->type;
        else
            return i;
    } else {
        return i;
    }
}

#ifndef OPENSSL_NO_DEPRECATED_4_1
int ASN1_STRING_length(const ASN1_STRING *x)
{
    return x->length;
}
#endif

size_t ASN1_STRING_get_length(const ASN1_STRING *x)
{
    return x->length >= 0 ? (size_t)x->length : 0U;
}

#ifndef OPENSSL_NO_DEPRECATED_3_0
void ASN1_STRING_length_set(ASN1_STRING *x, int len)
{
    x->length = len;
}
#endif

int ASN1_STRING_type(const ASN1_STRING *x)
{
    return x->type;
}

const unsigned char *ASN1_STRING_get0_data(const ASN1_STRING *x)
{
    return x->data;
}

char *ossl_sk_ASN1_UTF8STRING2text(STACK_OF(ASN1_UTF8STRING) *text,
    const char *sep, size_t max_len)
{
    int i;
    ASN1_UTF8STRING *current;
    size_t length = 0, sep_len;
    char *result = NULL;
    char *p;

    if (sep == NULL)
        sep = "";
    sep_len = strlen(sep);

    for (i = 0; i < sk_ASN1_UTF8STRING_num(text); i++) {
        current = sk_ASN1_UTF8STRING_value(text, i);
        if (i > 0)
            length += sep_len;
        length += ASN1_STRING_get_length(current);
        if (max_len != 0 && length > max_len)
            return NULL;
    }
    if ((result = OPENSSL_malloc(length + 1)) == NULL)
        return NULL;

    p = result;
    for (i = 0; i < sk_ASN1_UTF8STRING_num(text); i++) {
        current = sk_ASN1_UTF8STRING_value(text, i);
        length = ASN1_STRING_get_length(current);
        if (i > 0 && sep_len > 0) {
            memcpy(p, sep, sep_len);
            p += sep_len;
        }
        if (length > 0) {
            memcpy(p, ASN1_STRING_get0_data(current), length);
            p += length;
        }
    }
    *p = '\0';

    return result;
}
