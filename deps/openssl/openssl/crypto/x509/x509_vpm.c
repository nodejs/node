/*
 * Copyright 2004-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>

#include "internal/cryptlib.h"
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "crypto/ctype.h"
#include "crypto/x509.h"

#include "x509_local.h"

typedef enum {
    OSSL_CHARSET_NONASCII,
    OSSL_CHARSET_ASCII,
    OSSL_CHARSET_ASCII_ALNUM,
} ossl_charset_t;

/* X509_VERIFY_PARAM functions */

#define SET_HOST 0
#define ADD_HOST 1

static X509_BUFFER *buffer_from_bytes(const uint8_t *bytes, size_t length)
{
    X509_BUFFER *buf;

    if ((buf = OPENSSL_zalloc(sizeof(*buf))) != NULL
        && (buf->data = OPENSSL_memdup(bytes, length)) != NULL) {
        buf->len = length;
    } else {
        OPENSSL_free(buf);
        buf = NULL;
    }
    return buf;
}

/*
 * Copies |length| bytes from |bytes| to a new buffer, making the data
 * A C string. It is an error for the |bytes| to contain any \0 values
 * within |length|. |bytes| need not itself be \0 terminated, the data
 * in the buffer will be on success.
 */
static X509_BUFFER *buffer_from_string(const uint8_t *bytes, size_t length)
{
    X509_BUFFER *buf, *ret = NULL;
    uint8_t *data = NULL;

    if ((buf = OPENSSL_zalloc(sizeof(*buf))) == NULL)
        goto err;

    if ((data = (uint8_t *)OPENSSL_strndup((char *)bytes, length)) == NULL)
        goto err;

    if (strlen((char *)data) != length)
        goto err;

    ret = buf;
    buf = NULL;
    ret->data = data;
    ret->len = length;
    data = NULL;

err:
    OPENSSL_free(buf);
    OPENSSL_free(data);

    return ret;
}

static X509_BUFFER *buffer_copy(const X509_BUFFER *b)
{
    return buffer_from_bytes(b->data, b->len);
}

static void buffer_free(X509_BUFFER *b)
{
    if (b == NULL)
        return;
    OPENSSL_free((void *)b->data);
    OPENSSL_free(b);
}

static int replace_buffer_stack(STACK_OF(X509_BUFFER) **dest,
    STACK_OF(X509_BUFFER) *const *src)
{
    sk_X509_BUFFER_pop_free(*dest, buffer_free);
    *dest = NULL;
    if (*src != NULL) {
        *dest = sk_X509_BUFFER_deep_copy(*src, buffer_copy, buffer_free);
        if (*dest == NULL)
            return 0;
    }
    return 1;
}

static int buffer_cmp(const X509_BUFFER *const *a, const X509_BUFFER *const *b)
{
    if ((*a)->len < (*b)->len)
        return -1;
    if ((*a)->len > (*b)->len)
        return 1;
    if ((*b)->len == 0)
        return 0;
    return memcmp((*a)->data, (*b)->data, (*b)->len);
}

static void clear_buffer_stack(STACK_OF(X509_BUFFER) **buffer_stack)
{
    sk_X509_BUFFER_pop_free(*buffer_stack, buffer_free);
    *buffer_stack = NULL;
}

static int add_bytes_to_buffer_stack(STACK_OF(X509_BUFFER) **buffer_stack,
    const uint8_t *name, size_t name_len)
{
    STACK_OF(X509_BUFFER) *tmp_stack = NULL;
    X509_BUFFER *copy = NULL;
    int ret = 0;

    if ((copy = buffer_from_bytes(name, name_len)) == NULL)
        goto err;

    tmp_stack = *buffer_stack;
    if (tmp_stack == NULL && (tmp_stack = sk_X509_BUFFER_new(buffer_cmp)) == NULL)
        goto err;

    if (!sk_X509_BUFFER_push(tmp_stack, copy))
        goto err;

    ret = 1;
    copy = NULL;
    *buffer_stack = tmp_stack;
    tmp_stack = NULL;

err:
    sk_X509_BUFFER_pop_free(tmp_stack, buffer_free);
    buffer_free(copy);

    return ret;
}

static int add_string_to_buffer_stack(STACK_OF(X509_BUFFER) **buffer_stack,
    const uint8_t *name, size_t name_len)
{
    STACK_OF(X509_BUFFER) *tmp_stack = NULL;
    X509_BUFFER *copy = NULL;
    int ret = 0;

    if ((copy = buffer_from_string(name, name_len)) == NULL)
        goto err;

    tmp_stack = *buffer_stack;
    if (tmp_stack == NULL && (tmp_stack = sk_X509_BUFFER_new(buffer_cmp)) == NULL)
        goto err;

    if (!sk_X509_BUFFER_push(tmp_stack, copy))
        goto err;

    ret = 1;
    copy = NULL;
    *buffer_stack = tmp_stack;
    tmp_stack = NULL;

err:
    sk_X509_BUFFER_pop_free(tmp_stack, buffer_free);
    buffer_free(copy);

    return ret;
}

static int validate_string_name(const char *name, size_t *name_len)
{
    size_t len = *name_len;

    if (name == NULL || len == 0)
        return 0;

    /*
     * Accept the trailing \0 byte if this is a C string. This is to
     * preserver behaviour that is traditional for the
     * set1_[host|email] functions.
     */
    if (name[len - 1] == '\0')
        len--;

    /* Refuse the empty string */
    if (len == 0)
        return 0;

    /* Refuse values with embedded \0 bytes other than at the end */
    if (memchr(name, '\0', len) != NULL)
        return 0;

    *name_len = len;
    return 1;
}

/*
 * Default input validation for verification parameter names. As these
 * could potentially come from untrusted input, doing basic input
 * validation makes sense, and ensures that subsequent parsing or
 * comparisons do not need to handle extreme out of range input.
 */

/* Default ip name input validation */
static int validate_ip_name(const uint8_t *name, size_t len)
{
    if (name != NULL && (len == 4 || len == 16))
        return 1;
    return 0;
}

static ossl_charset_t ossl_name_charset(int c, ossl_charset_t charset)
{
    if (ossl_isalnum(c))
        return 1;
    if (ossl_isascii(c))
        return charset == OSSL_CHARSET_ASCII
            || charset == OSSL_CHARSET_NONASCII;
    return charset == OSSL_CHARSET_NONASCII;
}

static int is_label_ok(int c, ossl_charset_t charset)
{
    if (!ossl_name_charset(c, charset) && c != '_')
        return 0;
    else
        return c != '.' && c != '-';
}

/* Default host name input validation */
static int validate_hostname_part(const char *name, size_t len,
    ossl_charset_t charset)
{
    size_t i, part_len;
    char c, prev;

    if (len < 2 || len > 256)
        return 0;

    part_len = 0;
    prev = '\0';
    for (i = 0; i < len; i++) {
        c = name[i];
        if (c == '.') {
            /*
             * Can not start a label with a .
             * unless it is the very first character.
             */
            if (part_len == 0 && i != 0)
                return 0;
            /* Can not end a label with a - */
            if (prev == '-')
                return 0;
            part_len = 0;
        } else {
            /* Can not start a label with a - */
            if (part_len == 0 && c == '-') {
                return 0;
            }
            if (!is_label_ok(c, charset) && c != '-')
                return 0;
            part_len++;
        }
        if (part_len > 63)
            return 0;

        prev = c;
    }
    /* Can not end with a . or a _ */
    if (prev == '.' || prev == '-')
        return 0;

    return 1;
}

static int validate_local_part(const char *name, size_t len,
    ossl_charset_t *out_charset)
{
    ossl_charset_t charset = OSSL_CHARSET_ASCII;
    size_t i;

    for (i = 0; i < len; i++) {
        if (name[i] == '\0')
            return 0;
        if (!ossl_isascii(name[i]))
            charset = OSSL_CHARSET_NONASCII;
    }

    *out_charset = charset;
    return 1;
}

/* Default email name input validation */
static int validate_email_name(const char *name, size_t len, int rfc822)
{
    size_t dns_len, local_len;
    const char *at, *next, *dnsname;
    ossl_charset_t local_charset;

    /*
     * 64 for local part, 1 for @, 255 for domain name
     */
    if (len > 320)
        goto err;

    /* Reject it if there is no @ */
    if ((at = memchr(name, '@', len)) == NULL)
        goto err;

    /* Go to the last @ */
    while ((next = memchr(at + 1, '@', len - (at - name + 1))) != NULL)
        at = next;

    /* Ensure the local part is not oversize */
    local_len = at - name;
    if (local_len > 64)
        goto err;

    if (!validate_local_part(name, local_len, &local_charset))
        goto err;

    if (rfc822 && local_charset == OSSL_CHARSET_NONASCII)
        goto err;

    if (!rfc822 && local_charset == OSSL_CHARSET_ASCII)
        goto err;

    /* What is after the @ must be valid as a dns name */
    dnsname = at + 1;
    dns_len = len - local_len - 1;

    if (rfc822)
        return validate_hostname_part(dnsname, dns_len, OSSL_CHARSET_ASCII_ALNUM);

    return validate_hostname_part(dnsname, dns_len, OSSL_CHARSET_NONASCII);

err:
    ERR_raise(ERR_LIB_X509, ERR_R_PASSED_INVALID_ARGUMENT);
    return 0;
}

X509_VERIFY_PARAM *X509_VERIFY_PARAM_new(void)
{
    X509_VERIFY_PARAM *param;

    param = OPENSSL_zalloc(sizeof(*param));
    if (param == NULL)
        return NULL;
    param->trust = X509_TRUST_DEFAULT;
    /* param->inh_flags = X509_VP_FLAG_DEFAULT; */
    param->depth = -1;
    param->auth_level = -1; /* -1 means unset, 0 is explicit */
    return param;
}

void X509_VERIFY_PARAM_free(X509_VERIFY_PARAM *param)
{
    if (param == NULL)
        return;
    sk_ASN1_OBJECT_pop_free(param->policies, ASN1_OBJECT_free);
    clear_buffer_stack(&param->hosts);
    clear_buffer_stack(&param->ips);
    clear_buffer_stack(&param->rfc822s);
    clear_buffer_stack(&param->smtputf8s);
    OPENSSL_free(param->peername);
    OPENSSL_free(param);
}

/*-
 * This function determines how parameters are "inherited" from one structure
 * to another. There are several different ways this can happen.
 *
 * 1. If a child structure needs to have its values initialized from a parent
 *    they are simply copied across. For example SSL_CTX copied to SSL.
 * 2. If the structure should take on values only if they are currently unset.
 *    For example the values in an SSL structure will take appropriate value
 *    for SSL servers or clients but only if the application has not set new
 *    ones.
 *
 * The "inh_flags" field determines how this function behaves.
 *
 * Normally any values which are set in the default are not copied from the
 * destination and verify flags are ORed together.
 *
 * If X509_VP_FLAG_DEFAULT is set then anything set in the source is copied
 * to the destination. Effectively the values in "to" become default values
 * which will be used only if nothing new is set in "from".
 *
 * If X509_VP_FLAG_OVERWRITE is set then all value are copied across whether
 * they are set or not. Flags is still Ored though.
 *
 * If X509_VP_FLAG_RESET_FLAGS is set then the flags value is copied instead
 * of ORed.
 *
 * If X509_VP_FLAG_LOCKED is set then no values are copied.
 *
 * If X509_VP_FLAG_ONCE is set then the current inh_flags setting is zeroed
 * after the next call.
 */

/* Macro to test if a field should be copied from src to dest */

#define test_x509_verify_param_copy(field, def) \
    (to_overwrite || (src->field != def && (to_default || dest->field == def)))

/* Macro to test and copy a field if necessary */

#define x509_verify_param_copy(field, def)       \
    if (test_x509_verify_param_copy(field, def)) \
        dest->field = src->field;

int X509_VERIFY_PARAM_inherit(X509_VERIFY_PARAM *dest,
    const X509_VERIFY_PARAM *src)
{
    unsigned long inh_flags;
    int to_default, to_overwrite;

    if (src == NULL)
        return 1;
    inh_flags = dest->inh_flags | src->inh_flags;

    if ((inh_flags & X509_VP_FLAG_ONCE) != 0)
        dest->inh_flags = 0;

    if ((inh_flags & X509_VP_FLAG_LOCKED) != 0)
        return 1;

    to_default = (inh_flags & X509_VP_FLAG_DEFAULT) != 0;
    to_overwrite = (inh_flags & X509_VP_FLAG_OVERWRITE) != 0;

    x509_verify_param_copy(purpose, 0);
    x509_verify_param_copy(trust, X509_TRUST_DEFAULT);
    x509_verify_param_copy(depth, -1);
    x509_verify_param_copy(auth_level, -1);

    /* If overwrite or check time not set, copy across */

    if (to_overwrite || (dest->flags & X509_V_FLAG_USE_CHECK_TIME) == 0) {
        dest->check_time = src->check_time;
        dest->flags &= ~X509_V_FLAG_USE_CHECK_TIME;
        /* Don't need to copy flag: that is done below */
    }

    if ((inh_flags & X509_VP_FLAG_RESET_FLAGS) != 0)
        dest->flags = 0;

    dest->flags |= src->flags;

    if (test_x509_verify_param_copy(policies, NULL)) {
        if (!X509_VERIFY_PARAM_set1_policies(dest, src->policies))
            return 0;
    }

    x509_verify_param_copy(hostflags, 0);

    if (test_x509_verify_param_copy(hosts, NULL)) {
        if (!replace_buffer_stack(&dest->hosts, &src->hosts))
            return 0;
    }
    x509_verify_param_copy(validate_host, NULL);

    if (test_x509_verify_param_copy(ips, NULL)) {
        if (!replace_buffer_stack(&dest->ips, &src->ips))
            return 0;
    }
    x509_verify_param_copy(validate_ip, NULL);

    if (test_x509_verify_param_copy(rfc822s, NULL)) {
        if (!replace_buffer_stack(&dest->rfc822s, &src->rfc822s))
            return 0;
    }
    x509_verify_param_copy(validate_rfc822, NULL);

    if (test_x509_verify_param_copy(smtputf8s, NULL)) {
        if (!replace_buffer_stack(&dest->smtputf8s, &src->smtputf8s))
            return 0;
    }
    x509_verify_param_copy(validate_smtputf8, NULL);

    return 1;
}

int X509_VERIFY_PARAM_set1(X509_VERIFY_PARAM *to,
    const X509_VERIFY_PARAM *from)
{
    unsigned long save_flags;
    int ret;

    if (to == NULL) {
        ERR_raise(ERR_LIB_X509, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    save_flags = to->inh_flags;
    to->inh_flags |= X509_VP_FLAG_DEFAULT;
    ret = X509_VERIFY_PARAM_inherit(to, from);
    to->inh_flags = save_flags;
    return ret;
}

int X509_VERIFY_PARAM_set1_name(X509_VERIFY_PARAM *param, const char *name)
{
    OPENSSL_free(param->name);
    param->name = OPENSSL_strdup(name);
    return param->name != NULL;
}

int X509_VERIFY_PARAM_set_flags(X509_VERIFY_PARAM *param, unsigned long flags)
{
    param->flags |= flags;
    if ((flags & X509_V_FLAG_POLICY_MASK) != 0)
        param->flags |= X509_V_FLAG_POLICY_CHECK;
    return 1;
}

int X509_VERIFY_PARAM_clear_flags(X509_VERIFY_PARAM *param,
    unsigned long flags)
{
    param->flags &= ~flags;
    return 1;
}

unsigned long X509_VERIFY_PARAM_get_flags(const X509_VERIFY_PARAM *param)
{
    return param->flags;
}

uint32_t X509_VERIFY_PARAM_get_inh_flags(const X509_VERIFY_PARAM *param)
{
    return param->inh_flags;
}

int X509_VERIFY_PARAM_set_inh_flags(X509_VERIFY_PARAM *param, uint32_t flags)
{
    param->inh_flags = flags;
    return 1;
}

/* resets to default (any) purpose if |purpose| == X509_PURPOSE_DEFAULT_ANY */
int X509_VERIFY_PARAM_set_purpose(X509_VERIFY_PARAM *param, int purpose)
{
    return X509_PURPOSE_set(&param->purpose, purpose);
}

int X509_VERIFY_PARAM_get_purpose(const X509_VERIFY_PARAM *param)
{
    return param->purpose;
}

int X509_VERIFY_PARAM_set_trust(X509_VERIFY_PARAM *param, int trust)
{
    return X509_TRUST_set(&param->trust, trust);
}

void X509_VERIFY_PARAM_set_depth(X509_VERIFY_PARAM *param, int depth)
{
    param->depth = depth;
}

void X509_VERIFY_PARAM_set_auth_level(X509_VERIFY_PARAM *param, int auth_level)
{
    param->auth_level = auth_level;
}

time_t X509_VERIFY_PARAM_get_time(const X509_VERIFY_PARAM *param)
{
    /* This will be in the time_t range, because the only setter uses time_t */
    return (time_t)param->check_time;
}

void X509_VERIFY_PARAM_set_time(X509_VERIFY_PARAM *param, time_t t)
{
    param->check_time = (int64_t)t;
    param->flags |= X509_V_FLAG_USE_CHECK_TIME;
}

void ossl_x509_verify_param_set_time_posix(X509_VERIFY_PARAM *param, int64_t t)
{
    param->check_time = t;
    param->flags |= X509_V_FLAG_USE_CHECK_TIME;
}

int X509_VERIFY_PARAM_add0_policy(X509_VERIFY_PARAM *param,
    ASN1_OBJECT *policy)
{
    if (param->policies == NULL) {
        param->policies = sk_ASN1_OBJECT_new_null();
        if (param->policies == NULL)
            return 0;
    }

    if (sk_ASN1_OBJECT_push(param->policies, policy) <= 0)
        return 0;
    return 1;
}

int X509_VERIFY_PARAM_set1_policies(X509_VERIFY_PARAM *param,
    STACK_OF(ASN1_OBJECT) *policies)
{
    int i;
    ASN1_OBJECT *oid, *doid;

    if (param == NULL) {
        ERR_raise(ERR_LIB_X509, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    sk_ASN1_OBJECT_pop_free(param->policies, ASN1_OBJECT_free);

    if (policies == NULL) {
        param->policies = NULL;
        return 1;
    }

    param->policies = sk_ASN1_OBJECT_new_null();
    if (param->policies == NULL)
        return 0;

    for (i = 0; i < sk_ASN1_OBJECT_num(policies); i++) {
        oid = sk_ASN1_OBJECT_value(policies, i);
        doid = OBJ_dup(oid);
        if (doid == NULL)
            return 0;
        if (!sk_ASN1_OBJECT_push(param->policies, doid)) {
            ASN1_OBJECT_free(doid);
            return 0;
        }
    }
    param->flags |= X509_V_FLAG_POLICY_CHECK;
    return 1;
}

char *X509_VERIFY_PARAM_get0_host(X509_VERIFY_PARAM *param, int idx)
{
    X509_BUFFER *buf = sk_X509_BUFFER_value(param->hosts, idx);

    return (buf != NULL) ? (char *)buf->data : NULL;
}

int X509_VERIFY_PARAM_set1_host(X509_VERIFY_PARAM *param,
    const char *dnsname, size_t len)
{
    clear_buffer_stack(&param->hosts);
    if (dnsname == NULL)
        return 1;
    if (len == 0)
        len = strlen(dnsname);
    if (len == 0)
        return 1;
    return X509_VERIFY_PARAM_add1_host(param, dnsname, len);
}

int X509_VERIFY_PARAM_add1_host(X509_VERIFY_PARAM *param,
    const char *dnsname, size_t len)
{
    if (dnsname == NULL)
        return 1;
    if (len == 0)
        len = strlen(dnsname);
    if (len == 0)
        return 1;
    if (!validate_string_name(dnsname, &len))
        return 0;
    if (param->validate_host != NULL) {
        if (!param->validate_host(dnsname, len))
            return 0;
    } else {
        if (!validate_hostname_part(dnsname, len, OSSL_CHARSET_ASCII_ALNUM))
            return 0;
    }
    return add_string_to_buffer_stack(&param->hosts, (const uint8_t *)dnsname, len);
}

void X509_VERIFY_PARAM_set1_host_input_validation(X509_VERIFY_PARAM *param,
    int (*validate_host)(const char *name, size_t len))
{
    param->validate_host = validate_host;
}

int X509_VERIFY_PARAM_set1_ip(X509_VERIFY_PARAM *param,
    const uint8_t *ip, size_t len)
{
    clear_buffer_stack(&param->ips);
    if (ip == NULL)
        return 1;
    return X509_VERIFY_PARAM_add1_ip(param, ip, len);
}

int X509_VERIFY_PARAM_add1_ip(X509_VERIFY_PARAM *param,
    const uint8_t *ip, size_t len)
{
    if (param->validate_ip != NULL) {
        if (!param->validate_ip(ip, len))
            return 0;
    } else {
        if (!validate_ip_name(ip, len))
            return 0;
    }
    return add_bytes_to_buffer_stack(&param->ips, ip, len);
}

void X509_VERIFY_PARAM_set1_ip_input_validation(X509_VERIFY_PARAM *param,
    int (*validate_ip)(const uint8_t *name, size_t len))
{
    param->validate_ip = validate_ip;
}

char *X509_VERIFY_PARAM_get0_email(X509_VERIFY_PARAM *param)
{
    X509_BUFFER *buf = sk_X509_BUFFER_value(param->rfc822s, 0);

    if ((buf = sk_X509_BUFFER_value(param->rfc822s, 0)) != NULL
        || (buf = sk_X509_BUFFER_value(param->smtputf8s, 0)) != NULL)
        return (char *)buf->data;

    return NULL;
}

int X509_VERIFY_PARAM_set1_email(X509_VERIFY_PARAM *param,
    const char *email, size_t len)
{
    int ret = 0;

    if (X509_VERIFY_PARAM_set1_smtputf8(param, email, len))
        ret = 1;
    if (X509_VERIFY_PARAM_set1_rfc822(param, email, len))
        ret = 1;

    return ret;
}

int X509_VERIFY_PARAM_set1_smtputf8(X509_VERIFY_PARAM *param,
    const char *email, size_t len)
{
    clear_buffer_stack(&param->smtputf8s);
    if (email == NULL)
        return 1;
    return X509_VERIFY_PARAM_add1_smtputf8(param, email, len);
}

int X509_VERIFY_PARAM_add1_smtputf8(X509_VERIFY_PARAM *param,
    const char *email, size_t len)
{
    if (len == 0)
        len = strlen(email);
    if (!validate_string_name(email, &len))
        return 0;
    if (param->validate_smtputf8 != NULL) {
        if (!param->validate_smtputf8(email, len))
            return 0;
    } else {
        if (!validate_email_name(email, len, /*rfc822 =*/0))
            return 0;
    }

    return add_string_to_buffer_stack(&param->smtputf8s,
        (const uint8_t *)email, len);
}

void X509_VERIFY_PARAM_set1_smtputf8_input_validation(X509_VERIFY_PARAM *param,
    int (*validate_smtputf8)(const char *name, size_t len))
{
    param->validate_smtputf8 = validate_smtputf8;
}

int X509_VERIFY_PARAM_set1_rfc822(X509_VERIFY_PARAM *param,
    const char *email, size_t len)
{
    clear_buffer_stack(&param->rfc822s);
    if (email == NULL)
        return 1;
    return X509_VERIFY_PARAM_add1_rfc822(param, email, len);
}

int X509_VERIFY_PARAM_add1_rfc822(X509_VERIFY_PARAM *param,
    const char *email, size_t len)
{
    if (len == 0)
        len = strlen(email);
    if (!validate_string_name(email, &len))
        return 0;
    if (param->validate_rfc822 != NULL) {
        if (!param->validate_rfc822(email, len))
            return 0;
    } else {
        if (!validate_email_name(email, len, /*rfc822 =*/1))
            return 0;
    }

    return add_string_to_buffer_stack(&param->rfc822s,
        (const uint8_t *)email, len);
}

void X509_VERIFY_PARAM_set1_rfc822_input_validation(X509_VERIFY_PARAM *param,
    int (*validate_rfc822)(const char *name, size_t len))
{
    param->validate_rfc822 = validate_rfc822;
}

void X509_VERIFY_PARAM_set_hostflags(X509_VERIFY_PARAM *param,
    unsigned int flags)
{
    param->hostflags = flags;
}

unsigned int X509_VERIFY_PARAM_get_hostflags(const X509_VERIFY_PARAM *param)
{
    return param->hostflags;
}

char *X509_VERIFY_PARAM_get0_peername(const X509_VERIFY_PARAM *param)
{
    return param->peername;
}

/*
 * Move peername from one param structure to another, freeing any name present
 * at the target.  If the source is a NULL parameter structure, free and zero
 * the target peername.
 */
void X509_VERIFY_PARAM_move_peername(X509_VERIFY_PARAM *to,
    X509_VERIFY_PARAM *from)
{
    char *peername = (from != NULL) ? from->peername : NULL;

    if (to->peername != peername) {
        OPENSSL_free(to->peername);
        to->peername = peername;
    }
    if (from != NULL)
        from->peername = NULL;
}

static const unsigned char *int_X509_VERIFY_PARAM_get0_ip(X509_VERIFY_PARAM *param, size_t *plen, size_t idx)
{
    X509_BUFFER *buf;

    if (idx > INT_MAX)
        return NULL;

    if (param == NULL || param->ips == NULL) {
        ERR_raise(ERR_LIB_X509, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }

    buf = sk_X509_BUFFER_value(param->ips, (int)idx);

    if (buf != NULL) {
        if (plen != NULL)
            *plen = buf->len;
        return (unsigned char *)buf->data;
    }
    return NULL;
}

char *X509_VERIFY_PARAM_get1_ip_asc(X509_VERIFY_PARAM *param)
{
    size_t iplen;
    const unsigned char *ip = int_X509_VERIFY_PARAM_get0_ip(param, &iplen, 0);

    return ip == NULL ? NULL : ossl_ipaddr_to_asc(ip, (int)iplen);
}

int X509_VERIFY_PARAM_set1_ip_asc(X509_VERIFY_PARAM *param, const char *ipasc)
{
    unsigned char ipout[16];
    size_t iplen;

    if (ipasc == NULL)
        return X509_VERIFY_PARAM_set1_ip(param, NULL, 0);
    if ((iplen = (size_t)ossl_a2i_ipadd(ipout, ipasc)) == 0)
        return 0;
    return X509_VERIFY_PARAM_set1_ip(param, ipout, iplen);
}

int X509_VERIFY_PARAM_add1_ip_asc(X509_VERIFY_PARAM *param, const char *ipasc)
{
    unsigned char ipout[16];
    size_t iplen;

    if ((iplen = (size_t)ossl_a2i_ipadd(ipout, ipasc)) == 0)
        return 0;
    return X509_VERIFY_PARAM_add1_ip(param, ipout, iplen);
}

int X509_VERIFY_PARAM_get_depth(const X509_VERIFY_PARAM *param)
{
    return param->depth;
}

int X509_VERIFY_PARAM_get_auth_level(const X509_VERIFY_PARAM *param)
{
    return param->auth_level;
}

const char *X509_VERIFY_PARAM_get0_name(const X509_VERIFY_PARAM *param)
{
    return param->name;
}

/*
 * Default verify parameters: these are used for various applications and can
 * be overridden by the user specified table. NB: the 'name' field *must* be
 * in alphabetical order because it will be searched using OBJ_search.
 */

static const X509_VERIFY_PARAM default_table[] = {
    {
        .name = "code_sign", /* Code sign parameters */
        .purpose = X509_PURPOSE_CODE_SIGN,
        .trust = X509_TRUST_OBJECT_SIGN,
        .depth = -1,
        .auth_level = -1,
    },
    {
        .name = "default", /* X509 default parameters */
        .flags = X509_V_FLAG_TRUSTED_FIRST,
        .depth = 100,
        .auth_level = -1,
    },
    {
        .name = "pkcs7", /* S/MIME sign parameters */
        .purpose = X509_PURPOSE_SMIME_SIGN,
        .trust = X509_TRUST_EMAIL,
        .depth = -1,
        .auth_level = -1,
    },
    {
        .name = "smime_encrypt", /* S/MIME encryption parameters */
        .purpose = X509_PURPOSE_SMIME_ENCRYPT,
        .trust = X509_TRUST_EMAIL,
        .depth = -1,
        .auth_level = -1,
    },
    {
        .name = "smime_sign", /* S/MIME signature parameters */
        .purpose = X509_PURPOSE_SMIME_SIGN,
        .trust = X509_TRUST_EMAIL,
        .depth = -1,
        .auth_level = -1,
    },
    {
        .name = "ssl_client", /* SSL/TLS client parameters */
        .purpose = X509_PURPOSE_SSL_CLIENT,
        .trust = X509_TRUST_SSL_CLIENT,
        .depth = -1,
        .auth_level = -1,
    },
    {
        .name = "ssl_server", /* SSL/TLS server parameters */
        .purpose = X509_PURPOSE_SSL_SERVER,
        .trust = X509_TRUST_SSL_SERVER,
        .depth = -1,
        .auth_level = -1,
    }
};

static STACK_OF(X509_VERIFY_PARAM) *param_table = NULL;

static int table_cmp(const X509_VERIFY_PARAM *a, const X509_VERIFY_PARAM *b)
{
    return strcmp(a->name, b->name);
}

DECLARE_OBJ_BSEARCH_CMP_FN(X509_VERIFY_PARAM, X509_VERIFY_PARAM, table);
IMPLEMENT_OBJ_BSEARCH_CMP_FN(X509_VERIFY_PARAM, X509_VERIFY_PARAM, table);

static int param_cmp(const X509_VERIFY_PARAM *const *a,
    const X509_VERIFY_PARAM *const *b)
{
    return strcmp((*a)->name, (*b)->name);
}

int X509_VERIFY_PARAM_add0_table(X509_VERIFY_PARAM *param)
{
    int idx;
    X509_VERIFY_PARAM *ptmp;

    if (param_table == NULL) {
        param_table = sk_X509_VERIFY_PARAM_new(param_cmp);
        if (param_table == NULL)
            return 0;
    } else {
        idx = sk_X509_VERIFY_PARAM_find(param_table, param);
        if (idx >= 0) {
            ptmp = sk_X509_VERIFY_PARAM_delete(param_table, idx);
            X509_VERIFY_PARAM_free(ptmp);
        }
    }

    if (sk_X509_VERIFY_PARAM_push(param_table, param) <= 0)
        return 0;
    return 1;
}

int X509_VERIFY_PARAM_get_count(void)
{
    int num = OSSL_NELEM(default_table);

    if (param_table != NULL)
        num += sk_X509_VERIFY_PARAM_num(param_table);
    return num;
}

const X509_VERIFY_PARAM *X509_VERIFY_PARAM_get0(int id)
{
    int num = OSSL_NELEM(default_table);

    if (id < 0) {
        ERR_raise(ERR_LIB_X509, ERR_R_PASSED_INVALID_ARGUMENT);
        return NULL;
    }

    if (id < num)
        return default_table + id;
    return sk_X509_VERIFY_PARAM_value(param_table, id - num);
}

const X509_VERIFY_PARAM *X509_VERIFY_PARAM_lookup(const char *name)
{
    int idx;
    X509_VERIFY_PARAM pm;

    pm.name = (char *)name;
    if (param_table != NULL) {
        /* Ideally, this would be done under a lock */
        sk_X509_VERIFY_PARAM_sort(param_table);
        idx = sk_X509_VERIFY_PARAM_find(param_table, &pm);
        if (idx >= 0)
            return sk_X509_VERIFY_PARAM_value(param_table, idx);
    }
    return OBJ_bsearch_table(&pm, default_table, OSSL_NELEM(default_table));
}

void X509_VERIFY_PARAM_table_cleanup(void)
{
    sk_X509_VERIFY_PARAM_pop_free(param_table, X509_VERIFY_PARAM_free);
    param_table = NULL;
}
