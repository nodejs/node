/*
 *  X.509 base functions for creating certificates / CSRs
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "x509_internal.h"

#if defined(MBEDTLS_X509_CREATE_C)

#include "mbedtls/asn1write.h"
#include "mbedtls/error.h"
#include "mbedtls/oid.h"
#include "x509_oid.h"

#include <limits.h>
#include <string.h>

#include "mbedtls/platform.h"

#include "mbedtls/asn1.h"

/* Structure linking OIDs for X.509 DN AttributeTypes to their
 * string representations and default string encodings used by Mbed TLS. */
typedef struct {
    const char *name; /* String representation of AttributeType, e.g.
                       * "CN" or "emailAddress". */
    size_t name_len; /* Length of 'name', without trailing 0 byte. */
    const char *oid; /* String representation of OID of AttributeType,
                      * as per RFC 5280, Appendix A.1. encoded as per
                      * X.690 */
    int default_tag; /* The default character encoding used for the
                      * given attribute type, e.g.
                      * MBEDTLS_ASN1_UTF8_STRING for UTF-8. */
} x509_attr_descriptor_t;

#define ADD_STRLEN(s)     s, sizeof(s) - 1

/* X.509 DN attributes from RFC 5280, Appendix A.1. */
static const x509_attr_descriptor_t x509_attrs[] =
{
    { ADD_STRLEN("CN"),
      MBEDTLS_OID_AT_CN, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("commonName"),
      MBEDTLS_OID_AT_CN, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("C"),
      MBEDTLS_OID_AT_COUNTRY, MBEDTLS_ASN1_PRINTABLE_STRING },
    { ADD_STRLEN("countryName"),
      MBEDTLS_OID_AT_COUNTRY, MBEDTLS_ASN1_PRINTABLE_STRING },
    { ADD_STRLEN("O"),
      MBEDTLS_OID_AT_ORGANIZATION, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("organizationName"),
      MBEDTLS_OID_AT_ORGANIZATION, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("L"),
      MBEDTLS_OID_AT_LOCALITY, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("locality"),
      MBEDTLS_OID_AT_LOCALITY, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("R"),
      MBEDTLS_OID_PKCS9_EMAIL, MBEDTLS_ASN1_IA5_STRING },
    { ADD_STRLEN("OU"),
      MBEDTLS_OID_AT_ORG_UNIT, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("organizationalUnitName"),
      MBEDTLS_OID_AT_ORG_UNIT, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("ST"),
      MBEDTLS_OID_AT_STATE, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("stateOrProvinceName"),
      MBEDTLS_OID_AT_STATE, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("emailAddress"),
      MBEDTLS_OID_PKCS9_EMAIL, MBEDTLS_ASN1_IA5_STRING },
    { ADD_STRLEN("serialNumber"),
      MBEDTLS_OID_AT_SERIAL_NUMBER, MBEDTLS_ASN1_PRINTABLE_STRING },
    { ADD_STRLEN("postalAddress"),
      MBEDTLS_OID_AT_POSTAL_ADDRESS, MBEDTLS_ASN1_PRINTABLE_STRING },
    { ADD_STRLEN("postalCode"),
      MBEDTLS_OID_AT_POSTAL_CODE, MBEDTLS_ASN1_PRINTABLE_STRING },
    { ADD_STRLEN("dnQualifier"),
      MBEDTLS_OID_AT_DN_QUALIFIER, MBEDTLS_ASN1_PRINTABLE_STRING },
    { ADD_STRLEN("title"),
      MBEDTLS_OID_AT_TITLE, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("surName"),
      MBEDTLS_OID_AT_SUR_NAME, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("SN"),
      MBEDTLS_OID_AT_SUR_NAME, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("givenName"),
      MBEDTLS_OID_AT_GIVEN_NAME, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("GN"),
      MBEDTLS_OID_AT_GIVEN_NAME, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("initials"),
      MBEDTLS_OID_AT_INITIALS, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("pseudonym"),
      MBEDTLS_OID_AT_PSEUDONYM, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("generationQualifier"),
      MBEDTLS_OID_AT_GENERATION_QUALIFIER, MBEDTLS_ASN1_UTF8_STRING },
    { ADD_STRLEN("domainComponent"),
      MBEDTLS_OID_DOMAIN_COMPONENT, MBEDTLS_ASN1_IA5_STRING },
    { ADD_STRLEN("DC"),
      MBEDTLS_OID_DOMAIN_COMPONENT,   MBEDTLS_ASN1_IA5_STRING },
    { NULL, 0, NULL, MBEDTLS_ASN1_NULL }
};

static const x509_attr_descriptor_t *x509_attr_descr_from_name(const char *name, size_t name_len)
{
    const x509_attr_descriptor_t *cur;

    for (cur = x509_attrs; cur->name != NULL; cur++) {
        if (cur->name_len == name_len &&
            strncmp(cur->name, name, name_len) == 0) {
            break;
        }
    }

    if (cur->name == NULL) {
        return NULL;
    }

    return cur;
}

static int hex_to_int(char c)
{
    return ('0' <= c && c <= '9') ? (c - '0') :
           ('a' <= c && c <= 'f') ? (c - 'a' + 10) :
           ('A' <= c && c <= 'F') ? (c - 'A' + 10) : -1;
}

static int hexpair_to_int(const char *hexpair)
{
    int n1 = hex_to_int(*hexpair);
    int n2 = hex_to_int(*(hexpair + 1));

    if (n1 != -1 && n2 != -1) {
        return (n1 << 4) | n2;
    } else {
        return -1;
    }
}

static int parse_attribute_value_string(const char *s,
                                        int len,
                                        unsigned char *data,
                                        size_t *data_len)
{
    const char *c;
    const char *end = s + len;
    unsigned char *d = data;
    int n;

    for (c = s; c < end; c++) {
        if (*c == '\\') {
            c++;

            /* Check for valid escaped characters as per RFC 4514 Section 3 */
            if (c + 1 < end && (n = hexpair_to_int(c)) != -1) {
                if (n == 0) {
                    return MBEDTLS_ERR_X509_INVALID_NAME;
                }
                *(d++) = n;
                c++;
            } else if (c < end && strchr(" ,=+<>#;\"\\", *c)) {
                *(d++) = *c;
            } else {
                return MBEDTLS_ERR_X509_INVALID_NAME;
            }
        } else {
            *(d++) = *c;
        }

        if (d - data == MBEDTLS_X509_MAX_DN_NAME_SIZE) {
            return MBEDTLS_ERR_X509_INVALID_NAME;
        }
    }
    *data_len = (size_t) (d - data);
    return 0;
}

/** Parse a hexstring containing a DER-encoded string.
 *
 * \param s         A string of \p len bytes hexadecimal digits.
 * \param len       Number of bytes to read from \p s.
 * \param data      Output buffer of size \p data_size.
 *                  On success, it contains the payload that's DER-encoded
 *                  in the input (content without the tag and length).
 *                  If the DER tag is a string tag, the payload is guaranteed
 *                  not to contain null bytes.
 * \param data_size Length of the \p data buffer.
 * \param data_len  On success, the length of the parsed string.
 *                  It is guaranteed to be less than
 *                  #MBEDTLS_X509_MAX_DN_NAME_SIZE.
 * \param tag       The ASN.1 tag that the payload in \p data is encoded in.
 *
 * \retval          0 on success.
 * \retval          #MBEDTLS_ERR_X509_INVALID_NAME if \p s does not contain
 *                  a valid hexstring,
 *                  or if the decoded hexstring is not valid DER,
 *                  or if the payload does not fit in \p data,
 *                  or if the payload is more than
 *                  #MBEDTLS_X509_MAX_DN_NAME_SIZE bytes,
 *                  of if \p *tag is an ASN.1 string tag and the payload
 *                  contains a null byte.
 * \retval          #MBEDTLS_ERR_X509_ALLOC_FAILED on low memory.
 */
static int parse_attribute_value_hex_der_encoded(const char *s,
                                                 size_t len,
                                                 unsigned char *data,
                                                 size_t data_size,
                                                 size_t *data_len,
                                                 int *tag)
{
    /* Step 1: preliminary length checks. */
    /* Each byte is encoded by exactly two hexadecimal digits. */
    if (len % 2 != 0) {
        /* Odd number of hex digits */
        return MBEDTLS_ERR_X509_INVALID_NAME;
    }
    size_t const der_length = len / 2;
    if (der_length > MBEDTLS_X509_MAX_DN_NAME_SIZE + 4) {
        /* The payload would be more than MBEDTLS_X509_MAX_DN_NAME_SIZE
         * (after subtracting the ASN.1 tag and length). Reject this early
         * to avoid allocating a large intermediate buffer. */
        return MBEDTLS_ERR_X509_INVALID_NAME;
    }
    if (der_length < 1) {
        /* Avoid empty-buffer shenanigans. A valid DER encoding is never
         * empty. */
        return MBEDTLS_ERR_X509_INVALID_NAME;
    }

    /* Step 2: Decode the hex string into an intermediate buffer. */
    unsigned char *der = mbedtls_calloc(1, der_length);
    if (der == NULL) {
        return MBEDTLS_ERR_X509_ALLOC_FAILED;
    }
    /* Beyond this point, der needs to be freed on exit. */
    for (size_t i = 0; i < der_length; i++) {
        int c = hexpair_to_int(s + 2 * i);
        if (c < 0) {
            goto error;
        }
        der[i] = c;
    }

    /* Step 3: decode the DER. */
    /* We've checked that der_length >= 1 above. */
    *tag = der[0];
    {
        unsigned char *p = der + 1;
        if (mbedtls_asn1_get_len(&p, der + der_length, data_len) != 0) {
            goto error;
        }
        /* Now p points to the first byte of the payload inside der,
         * and *data_len is the length of the payload. */

        /* Step 4: payload validation */
        if (*data_len > MBEDTLS_X509_MAX_DN_NAME_SIZE) {
            goto error;
        }
        /* Strings must not contain null bytes. */
        if (MBEDTLS_ASN1_IS_STRING_TAG(*tag)) {
            for (size_t i = 0; i < *data_len; i++) {
                if (p[i] == 0) {
                    goto error;
                }
            }
        }

        /* Step 5: output the payload. */
        if (*data_len > data_size) {
            goto error;
        }
        memcpy(data, p, *data_len);
    }
    mbedtls_free(der);

    return 0;

error:
    mbedtls_free(der);
    return MBEDTLS_ERR_X509_INVALID_NAME;
}

static int oid_parse_number(unsigned int *num, const char **p, const char *bound)
{
    int ret = MBEDTLS_ERR_ASN1_INVALID_DATA;

    *num = 0;

    while (*p < bound && **p >= '0' && **p <= '9') {
        ret = 0;
        if (*num > (UINT_MAX / 10)) {
            return MBEDTLS_ERR_ASN1_INVALID_DATA;
        }
        *num *= 10;
        *num += **p - '0';
        (*p)++;
    }
    return ret;
}

static size_t oid_subidentifier_num_bytes(unsigned int value)
{
    size_t num_bytes = 0;

    do {
        value >>= 7;
        num_bytes++;
    } while (value != 0);

    return num_bytes;
}

static int oid_subidentifier_encode_into(unsigned char **p,
                                         unsigned char *bound,
                                         unsigned int value)
{
    size_t num_bytes = oid_subidentifier_num_bytes(value);

    if ((size_t) (bound - *p) < num_bytes) {
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }
    (*p)[num_bytes - 1] = (unsigned char) (value & 0x7f);
    value >>= 7;

    for (size_t i = 2; i <= num_bytes; i++) {
        (*p)[num_bytes - i] = 0x80 | (unsigned char) (value & 0x7f);
        value >>= 7;
    }
    *p += num_bytes;

    return 0;
}

/* Return the OID for the given x.y.z.... style numeric string  */
int mbedtls_oid_from_numeric_string(mbedtls_asn1_buf *oid,
                                    const char *oid_str, size_t size)
{
    int ret = MBEDTLS_ERR_ASN1_INVALID_DATA;
    const char *str_ptr = oid_str;
    const char *str_bound = oid_str + size;
    unsigned int val = 0;
    unsigned int component1, component2;
    size_t encoded_len;
    unsigned char *resized_mem;

    /* Count the number of dots to get a worst-case allocation size. */
    size_t num_dots = 0;
    for (size_t i = 0; i < size; i++) {
        if (oid_str[i] == '.') {
            num_dots++;
        }
    }
    /* Allocate maximum possible required memory:
     * There are (num_dots + 1) integer components, but the first 2 share the
     * same subidentifier, so we only need num_dots subidentifiers maximum. */
    if (num_dots == 0 || (num_dots > MBEDTLS_OID_MAX_COMPONENTS - 1)) {
        return MBEDTLS_ERR_ASN1_INVALID_DATA;
    }
    /* Each byte can store 7 bits, calculate number of bytes for a
     * subidentifier:
     *
     * bytes = ceil(subidentifer_size * 8 / 7)
     */
    size_t bytes_per_subidentifier = (((sizeof(unsigned int) * 8) - 1) / 7)
                                     + 1;
    size_t max_possible_bytes = num_dots * bytes_per_subidentifier;
    oid->p = mbedtls_calloc(max_possible_bytes, 1);
    if (oid->p == NULL) {
        return MBEDTLS_ERR_ASN1_ALLOC_FAILED;
    }
    unsigned char *out_ptr = oid->p;
    unsigned char *out_bound = oid->p + max_possible_bytes;

    ret = oid_parse_number(&component1, &str_ptr, str_bound);
    if (ret != 0) {
        goto error;
    }
    if (component1 > 2) {
        /* First component can't be > 2 */
        ret = MBEDTLS_ERR_ASN1_INVALID_DATA;
        goto error;
    }
    if (str_ptr >= str_bound || *str_ptr != '.') {
        ret = MBEDTLS_ERR_ASN1_INVALID_DATA;
        goto error;
    }
    str_ptr++;

    ret = oid_parse_number(&component2, &str_ptr, str_bound);
    if (ret != 0) {
        goto error;
    }
    if ((component1 < 2) && (component2 > 39)) {
        /* Root nodes 0 and 1 may have up to 40 children, numbered 0-39 */
        ret = MBEDTLS_ERR_ASN1_INVALID_DATA;
        goto error;
    }
    if (str_ptr < str_bound) {
        if (*str_ptr == '.') {
            str_ptr++;
        } else {
            ret = MBEDTLS_ERR_ASN1_INVALID_DATA;
            goto error;
        }
    }

    if (component2 > (UINT_MAX - (component1 * 40))) {
        ret = MBEDTLS_ERR_ASN1_INVALID_DATA;
        goto error;
    }
    ret = oid_subidentifier_encode_into(&out_ptr, out_bound,
                                        (component1 * 40) + component2);
    if (ret != 0) {
        goto error;
    }

    while (str_ptr < str_bound) {
        ret = oid_parse_number(&val, &str_ptr, str_bound);
        if (ret != 0) {
            goto error;
        }
        if (str_ptr < str_bound) {
            if (*str_ptr == '.') {
                str_ptr++;
            } else {
                ret = MBEDTLS_ERR_ASN1_INVALID_DATA;
                goto error;
            }
        }

        ret = oid_subidentifier_encode_into(&out_ptr, out_bound, val);
        if (ret != 0) {
            goto error;
        }
    }

    encoded_len = (size_t) (out_ptr - oid->p);
    resized_mem = mbedtls_calloc(encoded_len, 1);
    if (resized_mem == NULL) {
        ret = MBEDTLS_ERR_ASN1_ALLOC_FAILED;
        goto error;
    }
    memcpy(resized_mem, oid->p, encoded_len);
    mbedtls_free(oid->p);
    oid->p = resized_mem;
    oid->len = encoded_len;

    oid->tag = MBEDTLS_ASN1_OID;

    return 0;

error:
    mbedtls_free(oid->p);
    oid->p = NULL;
    oid->len = 0;
    return ret;
}

int mbedtls_x509_string_to_names(mbedtls_asn1_named_data **head, const char *name)
{
    int ret = MBEDTLS_ERR_X509_INVALID_NAME;
    int parse_ret = 0;
    const char *s = name, *c = s;
    const char *end = s + strlen(s);
    mbedtls_asn1_buf oid = { .p = NULL, .len = 0, .tag = MBEDTLS_ASN1_NULL };
    const x509_attr_descriptor_t *attr_descr = NULL;
    int in_attr_type = 1;
    int tag;
    int numericoid = 0;
    unsigned char data[MBEDTLS_X509_MAX_DN_NAME_SIZE];
    size_t data_len = 0;

    /* Ensure the output parameter is not already populated.
     * (If it were, overwriting it would likely cause a memory leak.)
     */
    if (*head != NULL) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    while (c <= end) {
        if (in_attr_type && *c == '=') {
            if ((attr_descr = x509_attr_descr_from_name(s, (size_t) (c - s))) == NULL) {
                if ((mbedtls_oid_from_numeric_string(&oid, s, (size_t) (c - s))) != 0) {
                    return MBEDTLS_ERR_X509_INVALID_NAME;
                } else {
                    numericoid = 1;
                }
            } else {
                oid.len = strlen(attr_descr->oid);
                oid.p = mbedtls_calloc(1, oid.len);
                memcpy(oid.p, attr_descr->oid, oid.len);
                numericoid = 0;
            }

            s = c + 1;
            in_attr_type = 0;
        }

        if (!in_attr_type && ((*c == ',' && *(c-1) != '\\') || c == end)) {
            if (s == c) {
                mbedtls_free(oid.p);
                return MBEDTLS_ERR_X509_INVALID_NAME;
            } else if (*s == '#') {
                /* We know that c >= s (loop invariant) and c != s (in this
                 * else branch), hence c - s - 1 >= 0. */
                parse_ret = parse_attribute_value_hex_der_encoded(
                    s + 1, (size_t) (c - s) - 1,
                    data, sizeof(data), &data_len, &tag);
                if (parse_ret != 0) {
                    mbedtls_free(oid.p);
                    return parse_ret;
                }
            } else {
                if (numericoid) {
                    mbedtls_free(oid.p);
                    return MBEDTLS_ERR_X509_INVALID_NAME;
                } else {
                    if ((parse_ret =
                             parse_attribute_value_string(s, (int) (c - s), data,
                                                          &data_len)) != 0) {
                        mbedtls_free(oid.p);
                        return parse_ret;
                    }
                    tag = attr_descr->default_tag;
                }
            }

            mbedtls_asn1_named_data *cur =
                mbedtls_asn1_store_named_data(head, (char *) oid.p, oid.len,
                                              (unsigned char *) data,
                                              data_len);
            mbedtls_free(oid.p);
            oid.p = NULL;
            if (cur == NULL) {
                return MBEDTLS_ERR_X509_ALLOC_FAILED;
            }

            // set tagType
            cur->val.tag = tag;

            while (c < end && *(c + 1) == ' ') {
                c++;
            }

            s = c + 1;
            in_attr_type = 1;

            /* Successfully parsed one name, update ret to success */
            ret = 0;
        }
        c++;
    }
    if (oid.p != NULL) {
        mbedtls_free(oid.p);
    }
    return ret;
}

/* The first byte of the value in the mbedtls_asn1_named_data structure is reserved
 * to store the critical boolean for us
 */
int mbedtls_x509_set_extension(mbedtls_asn1_named_data **head, const char *oid, size_t oid_len,
                               int critical, const unsigned char *val, size_t val_len)
{
    mbedtls_asn1_named_data *cur;

    if (val_len > (SIZE_MAX  - 1)) {
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    }

    if ((cur = mbedtls_asn1_store_named_data(head, oid, oid_len,
                                             NULL, val_len + 1)) == NULL) {
        return MBEDTLS_ERR_X509_ALLOC_FAILED;
    }

    cur->val.p[0] = critical;
    memcpy(cur->val.p + 1, val, val_len);

    return 0;
}

/*
 *  RelativeDistinguishedName ::=
 *    SET OF AttributeTypeAndValue
 *
 *  AttributeTypeAndValue ::= SEQUENCE {
 *    type     AttributeType,
 *    value    AttributeValue }
 *
 *  AttributeType ::= OBJECT IDENTIFIER
 *
 *  AttributeValue ::= ANY DEFINED BY AttributeType
 */
static int x509_write_name(unsigned char **p,
                           unsigned char *start,
                           mbedtls_asn1_named_data *cur_name)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;
    const char *oid             = (const char *) cur_name->oid.p;
    size_t oid_len              = cur_name->oid.len;
    const unsigned char *name   = cur_name->val.p;
    size_t name_len             = cur_name->val.len;

    // Write correct string tag and value
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tagged_string(p, start,
                                                               cur_name->val.tag,
                                                               (const char *) name,
                                                               name_len));
    // Write OID
    //
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_oid(p, start, oid,
                                                     oid_len));

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start,
                                                     MBEDTLS_ASN1_CONSTRUCTED |
                                                     MBEDTLS_ASN1_SEQUENCE));

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start,
                                                     MBEDTLS_ASN1_CONSTRUCTED |
                                                     MBEDTLS_ASN1_SET));

    return (int) len;
}

int mbedtls_x509_write_names(unsigned char **p, unsigned char *start,
                             mbedtls_asn1_named_data *first)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;
    mbedtls_asn1_named_data *cur = first;

    while (cur != NULL) {
        MBEDTLS_ASN1_CHK_ADD(len, x509_write_name(p, start, cur));
        cur = cur->next;
    }

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_CONSTRUCTED |
                                                     MBEDTLS_ASN1_SEQUENCE));

    return (int) len;
}

int mbedtls_x509_write_sig(unsigned char **p, unsigned char *start,
                           const char *oid, size_t oid_len,
                           unsigned char *sig, size_t size,
                           mbedtls_pk_sigalg_t pk_alg)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int write_null_par;
    size_t len = 0;

    if (*p < start || (size_t) (*p - start) < size) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    len = size;
    (*p) -= len;
    memcpy(*p, sig, len);

    if (*p - start < 1) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    *--(*p) = 0;
    len += 1;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_BIT_STRING));

    // Write OID
    //
    if (pk_alg == MBEDTLS_PK_SIGALG_ECDSA) {
        /*
         * The AlgorithmIdentifier's parameters field must be absent for DSA/ECDSA signature
         * algorithms, see https://www.rfc-editor.org/rfc/rfc5480#page-17 and
         * https://www.rfc-editor.org/rfc/rfc5758#section-3.
         */
        write_null_par = 0;
    } else {
        write_null_par = 1;
    }
    MBEDTLS_ASN1_CHK_ADD(len,
                         mbedtls_asn1_write_algorithm_identifier_ext(p, start, oid, oid_len,
                                                                     0, write_null_par));

    return (int) len;
}

static int x509_write_extension(unsigned char **p, unsigned char *start,
                                mbedtls_asn1_named_data *ext)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_raw_buffer(p, start, ext->val.p + 1,
                                                            ext->val.len - 1));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, ext->val.len - 1));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_OCTET_STRING));

    if (ext->val.p[0] != 0) {
        MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_bool(p, start, 1));
    }

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_raw_buffer(p, start, ext->oid.p,
                                                            ext->oid.len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, ext->oid.len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_OID));

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_CONSTRUCTED |
                                                     MBEDTLS_ASN1_SEQUENCE));

    return (int) len;
}

/*
 * Extension  ::=  SEQUENCE  {
 *     extnID      OBJECT IDENTIFIER,
 *     critical    BOOLEAN DEFAULT FALSE,
 *     extnValue   OCTET STRING
 *                 -- contains the DER encoding of an ASN.1 value
 *                 -- corresponding to the extension type identified
 *                 -- by extnID
 *     }
 */
int mbedtls_x509_write_extensions(unsigned char **p, unsigned char *start,
                                  mbedtls_asn1_named_data *first)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;
    mbedtls_asn1_named_data *cur_ext = first;

    while (cur_ext != NULL) {
        MBEDTLS_ASN1_CHK_ADD(len, x509_write_extension(p, start, cur_ext));
        cur_ext = cur_ext->next;
    }

    return (int) len;
}

#endif /* MBEDTLS_X509_CREATE_C */
