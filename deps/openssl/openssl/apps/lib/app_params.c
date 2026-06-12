/*
 * Copyright 2019-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "apps.h"
#include "app_params.h"

/* Maximum number of bytes that will be output for an octet string body */
#define MAX_OCTET_STRING_OUTPUT_BYTES 24

static int describe_param_type(char *buf, size_t bufsz, const OSSL_PARAM *param)
{
    const char *type_mod = "";
    const char *type = NULL;
    int show_type_number = 0;
    int printed_len;

    switch (param->data_type) {
    case OSSL_PARAM_UNSIGNED_INTEGER:
        type_mod = "unsigned ";
        /* FALLTHRU */
    case OSSL_PARAM_INTEGER:
        type = "integer";
        break;
    case OSSL_PARAM_UTF8_PTR:
        type_mod = "pointer to a ";
        /* FALLTHRU */
    case OSSL_PARAM_UTF8_STRING:
        type = "UTF8 encoded string";
        break;
    case OSSL_PARAM_OCTET_PTR:
        type_mod = "pointer to an ";
        /* FALLTHRU */
    case OSSL_PARAM_OCTET_STRING:
        type = "octet string";
        break;
    default:
        type = "unknown type";
        show_type_number = 1;
        break;
    }

    printed_len = BIO_snprintf(buf, bufsz, "%s: ", param->key);
    if (printed_len > 0) {
        buf += printed_len;
        bufsz -= printed_len;
    }
    printed_len = BIO_snprintf(buf, bufsz, "%s%s", type_mod, type);
    if (printed_len > 0) {
        buf += printed_len;
        bufsz -= printed_len;
    }
    if (show_type_number) {
        printed_len = BIO_snprintf(buf, bufsz, " [%d]", param->data_type);
        if (printed_len > 0) {
            buf += printed_len;
            bufsz -= printed_len;
        }
    }
    if (param->data_size == 0)
        printed_len = BIO_snprintf(buf, bufsz, " (arbitrary size)");
    else
        printed_len = BIO_snprintf(buf, bufsz, " (max %zu bytes large)",
                                   param->data_size);
    if (printed_len > 0) {
        buf += printed_len;
        bufsz -= printed_len;
    }
    *buf = '\0';
    return 1;
}

int print_param_types(const char *thing, const OSSL_PARAM *pdefs, int indent)
{
    if (pdefs == NULL) {
        return 1;
    } else if (pdefs->key == NULL) {
        /*
         * An empty list?  This shouldn't happen, but let's just make sure to
         * say something if there's a badly written provider...
         */
        BIO_printf(bio_out, "%*sEmpty list of %s (!!!)\n", indent, "", thing);
    } else {
        BIO_printf(bio_out, "%*s%s:\n", indent, "", thing);
        for (; pdefs->key != NULL; pdefs++) {
            char buf[200];       /* This should be ample space */

            describe_param_type(buf, sizeof(buf), pdefs);
            BIO_printf(bio_out, "%*s  %s\n", indent, "", buf);
        }
    }
    return 1;
}

/* Output the body of a UTF8 string which might not be zero terminated */
static void print_param_utf8(const char **s_ptr, size_t len)
{
    const char *s;

    if (s_ptr == NULL) {
        BIO_puts(bio_out, " ptr null\n");
        return;
    }
    if ((s = *s_ptr) == NULL) {
        BIO_puts(bio_out, " null\n");
        return;
    }
    BIO_puts(bio_out, "'");
    if (len > 0)
        BIO_write(bio_out, s, (int)len);
    BIO_puts(bio_out, "'\n");
}

/* Output the body of an OCTET string */
static void print_param_octet(const unsigned char **bytes_ptr, size_t len)
{
    size_t i;
    const char *tail = "\n";
    const unsigned char *bytes;

    BIO_printf(bio_out, "<%zu bytes>", len);
    if (bytes_ptr == NULL) {
        BIO_puts(bio_out, " ptr null\n");
        return;
    }
    if ((bytes = *bytes_ptr) == NULL) {
        BIO_puts(bio_out, " null\n");
        return;
    }
    if (len == 0) {
        BIO_puts(bio_out, "\n");
        return;
    }

    if (len > MAX_OCTET_STRING_OUTPUT_BYTES) {
        len = MAX_OCTET_STRING_OUTPUT_BYTES;
        tail = "...\n";
    }
    BIO_puts(bio_out, " ");
    for (i = 0; i < len; i++)
        BIO_printf(bio_out, "%02x", bytes[i]);
    BIO_puts(bio_out, tail);
}

void print_param_value(const OSSL_PARAM *p, int indent)
{
    int64_t i;
    uint64_t u;

    printf("%*s%s: ", indent, "", p->key);
    switch (p->data_type) {
    case OSSL_PARAM_UNSIGNED_INTEGER:
        if (OSSL_PARAM_get_uint64(p, &u))
            BIO_printf(bio_out, "%llu\n", (unsigned long long int)u);
        else
            BIO_printf(bio_out, "error getting value\n");
        break;
    case OSSL_PARAM_INTEGER:
        if (OSSL_PARAM_get_int64(p, &i))
            BIO_printf(bio_out, "%lld\n", (long long int)i);
        else
            BIO_printf(bio_out, "error getting value\n");
        break;
    case OSSL_PARAM_UTF8_PTR:
        print_param_utf8((const char **)p->data, p->return_size);
        break;
    case OSSL_PARAM_UTF8_STRING:
        print_param_utf8((const char **)&p->data, p->return_size);
        break;
    case OSSL_PARAM_OCTET_PTR:
        print_param_octet((const unsigned char **)p->data, p->return_size);
        break;
    case OSSL_PARAM_OCTET_STRING:
        print_param_octet((const unsigned char **)&p->data, p->return_size);
        break;
    default:
        BIO_printf(bio_out, "unknown type (%u) of %zu bytes\n",
                   p->data_type, p->return_size);
        break;
    }
}

