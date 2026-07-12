/**
 * \file asn1.h
 *
 * \brief Generic ASN.1 parsing
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_ASN1_H
#define MBEDTLS_ASN1_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"
#include "mbedtls/platform_util.h"

#include <stddef.h>

#if defined(MBEDTLS_BIGNUM_C)
#include "mbedtls/bignum.h"
#endif

/**
 * \addtogroup asn1_module
 * \{
 */

/**
 * \name ASN1 Error codes
 * These error codes are combined with other error codes for
 * higher error granularity.
 * e.g. X.509 and PKCS #7 error codes
 * ASN1 is a standard to specify data structures.
 * \{
 */
/** Out of data when parsing an ASN1 data structure. */
#define MBEDTLS_ERR_ASN1_OUT_OF_DATA                      -0x0060
/** ASN1 tag was of an unexpected value. */
#define MBEDTLS_ERR_ASN1_UNEXPECTED_TAG                   -0x0062
/** Error when trying to determine the length or invalid length. */
#define MBEDTLS_ERR_ASN1_INVALID_LENGTH                   -0x0064
/** Actual length differs from expected length. */
#define MBEDTLS_ERR_ASN1_LENGTH_MISMATCH                  -0x0066
/** Data is invalid. */
#define MBEDTLS_ERR_ASN1_INVALID_DATA                     -0x0068
/** Memory allocation failed */
#define MBEDTLS_ERR_ASN1_ALLOC_FAILED                     -0x006A
/** Buffer too small when writing ASN.1 data structure. */
#define MBEDTLS_ERR_ASN1_BUF_TOO_SMALL                    -0x006C

/** \} name ASN1 Error codes */

/**
 * \name DER constants
 * These constants comply with the DER encoded ASN.1 type tags.
 * DER encoding uses hexadecimal representation.
 * An example DER sequence is:\n
 * - 0x02 -- tag indicating INTEGER
 * - 0x01 -- length in octets
 * - 0x05 -- value
 * Such sequences are typically read into \c ::mbedtls_x509_buf.
 * \{
 */
#define MBEDTLS_ASN1_BOOLEAN                 0x01
#define MBEDTLS_ASN1_INTEGER                 0x02
#define MBEDTLS_ASN1_BIT_STRING              0x03
#define MBEDTLS_ASN1_OCTET_STRING            0x04
#define MBEDTLS_ASN1_NULL                    0x05
#define MBEDTLS_ASN1_OID                     0x06
#define MBEDTLS_ASN1_ENUMERATED              0x0A
#define MBEDTLS_ASN1_UTF8_STRING             0x0C
#define MBEDTLS_ASN1_SEQUENCE                0x10
#define MBEDTLS_ASN1_SET                     0x11
#define MBEDTLS_ASN1_PRINTABLE_STRING        0x13
#define MBEDTLS_ASN1_T61_STRING              0x14
#define MBEDTLS_ASN1_IA5_STRING              0x16
#define MBEDTLS_ASN1_UTC_TIME                0x17
#define MBEDTLS_ASN1_GENERALIZED_TIME        0x18
#define MBEDTLS_ASN1_UNIVERSAL_STRING        0x1C
#define MBEDTLS_ASN1_BMP_STRING              0x1E
#define MBEDTLS_ASN1_PRIMITIVE               0x00
#define MBEDTLS_ASN1_CONSTRUCTED             0x20
#define MBEDTLS_ASN1_CONTEXT_SPECIFIC        0x80

/* Slightly smaller way to check if tag is a string tag
 * compared to canonical implementation. */
#define MBEDTLS_ASN1_IS_STRING_TAG(tag)                                \
    ((unsigned int) (tag) < 32u && (                                   \
         ((1u << (tag)) & ((1u << MBEDTLS_ASN1_BMP_STRING)       |     \
                           (1u << MBEDTLS_ASN1_UTF8_STRING)      |     \
                           (1u << MBEDTLS_ASN1_T61_STRING)       |     \
                           (1u << MBEDTLS_ASN1_IA5_STRING)       |     \
                           (1u << MBEDTLS_ASN1_UNIVERSAL_STRING) |     \
                           (1u << MBEDTLS_ASN1_PRINTABLE_STRING))) != 0))

/*
 * Bit masks for each of the components of an ASN.1 tag as specified in
 * ITU X.690 (08/2015), section 8.1 "General rules for encoding",
 * paragraph 8.1.2.2:
 *
 * Bit  8     7   6   5          1
 *     +-------+-----+------------+
 *     | Class | P/C | Tag number |
 *     +-------+-----+------------+
 */
#define MBEDTLS_ASN1_TAG_CLASS_MASK          0xC0
#define MBEDTLS_ASN1_TAG_PC_MASK             0x20
#define MBEDTLS_ASN1_TAG_VALUE_MASK          0x1F

/** \} name DER constants */

/** Returns the size of the binary string, without the trailing \\0 */
#define MBEDTLS_OID_SIZE(x) (sizeof(x) - 1)

/**
 * Compares an mbedtls_asn1_buf structure to a reference OID.
 *
 * Only works for 'defined' oid_str values (MBEDTLS_OID_HMAC_SHA1), you cannot use a
 * 'unsigned char *oid' here!
 */
#define MBEDTLS_OID_CMP(oid_str, oid_buf)                                   \
    ((MBEDTLS_OID_SIZE(oid_str) != (oid_buf)->len) ||                \
     memcmp((oid_str), (oid_buf)->p, (oid_buf)->len) != 0)

#define MBEDTLS_OID_CMP_RAW(oid_str, oid_buf, oid_buf_len)              \
    ((MBEDTLS_OID_SIZE(oid_str) != (oid_buf_len)) ||             \
     memcmp((oid_str), (oid_buf), (oid_buf_len)) != 0)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \name Functions to parse ASN.1 data structures
 * \{
 */

/**
 * Type-length-value structure that allows for ASN1 using DER.
 */
typedef struct mbedtls_asn1_buf {
    int tag;                /**< ASN1 type, e.g. MBEDTLS_ASN1_UTF8_STRING. */
    size_t len;             /**< ASN1 length, in octets. */
    unsigned char *p;       /**< ASN1 data, e.g. in ASCII. */
}
mbedtls_asn1_buf;

/**
 * Container for ASN1 bit strings.
 */
typedef struct mbedtls_asn1_bitstring {
    size_t len;                 /**< ASN1 length, in octets. */
    unsigned char unused_bits;  /**< Number of unused bits at the end of the string */
    unsigned char *p;           /**< Raw ASN1 data for the bit string */
}
mbedtls_asn1_bitstring;

/**
 * Container for a sequence of ASN.1 items
 */
typedef struct mbedtls_asn1_sequence {
    mbedtls_asn1_buf buf;                   /**< Buffer containing the given ASN.1 item. */

    /** The next entry in the sequence.
     *
     * The details of memory management for sequences are not documented and
     * may change in future versions. Set this field to \p NULL when
     * initializing a structure, and do not modify it except via Mbed TLS
     * library functions.
     */
    struct mbedtls_asn1_sequence *next;
}
mbedtls_asn1_sequence;

/**
 * Container for a sequence or list of 'named' ASN.1 data items
 */
typedef struct mbedtls_asn1_named_data {
    mbedtls_asn1_buf oid;                   /**< The object identifier. */
    mbedtls_asn1_buf val;                   /**< The named value. */

    /** The next entry in the sequence.
     *
     * The details of memory management for named data sequences are not
     * documented and may change in future versions. Set this field to \p NULL
     * when initializing a structure, and do not modify it except via Mbed TLS
     * library functions.
     */
    struct mbedtls_asn1_named_data *next;

    /** Merge next item into the current one?
     *
     * This field exists for the sake of Mbed TLS's X.509 certificate parsing
     * code and may change in future versions of the library.
     */
    unsigned char MBEDTLS_PRIVATE(next_merged);
}
mbedtls_asn1_named_data;

#if defined(MBEDTLS_ASN1_PARSE_C) || defined(MBEDTLS_X509_CREATE_C) || \
    defined(MBEDTLS_PSA_UTIL_HAVE_ECDSA)
/**
 * \brief       Get the length of an ASN.1 element.
 *              Updates the pointer to immediately behind the length.
 *
 * \param p     On entry, \c *p points to the first byte of the length,
 *              i.e. immediately after the tag.
 *              On successful completion, \c *p points to the first byte
 *              after the length, i.e. the first byte of the content.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param len   On successful completion, \c *len contains the length
 *              read from the ASN.1 input.
 *
 * \return      0 if successful.
 * \return      #MBEDTLS_ERR_ASN1_OUT_OF_DATA if the ASN.1 element
 *              would end beyond \p end.
 * \return      #MBEDTLS_ERR_ASN1_INVALID_LENGTH if the length is unparsable.
 */
int mbedtls_asn1_get_len(unsigned char **p,
                         const unsigned char *end,
                         size_t *len);

/**
 * \brief       Get the tag and length of the element.
 *              Check for the requested tag.
 *              Updates the pointer to immediately behind the tag and length.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p points to the first byte
 *              after the length, i.e. the first byte of the content.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param len   On successful completion, \c *len contains the length
 *              read from the ASN.1 input.
 * \param tag   The expected tag.
 *
 * \return      0 if successful.
 * \return      #MBEDTLS_ERR_ASN1_UNEXPECTED_TAG if the data does not start
 *              with the requested tag.
 * \return      #MBEDTLS_ERR_ASN1_OUT_OF_DATA if the ASN.1 element
 *              would end beyond \p end.
 * \return      #MBEDTLS_ERR_ASN1_INVALID_LENGTH if the length is unparsable.
 */
int mbedtls_asn1_get_tag(unsigned char **p,
                         const unsigned char *end,
                         size_t *len, int tag);
#endif /* MBEDTLS_ASN1_PARSE_C || MBEDTLS_X509_CREATE_C || MBEDTLS_PSA_UTIL_HAVE_ECDSA */

#if defined(MBEDTLS_ASN1_PARSE_C)
/**
 * \brief       Retrieve a boolean ASN.1 tag and its value.
 *              Updates the pointer to immediately behind the full tag.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p points to the first byte
 *              beyond the ASN.1 element.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param val   On success, the parsed value (\c 0 or \c 1).
 *
 * \return      0 if successful.
 * \return      An ASN.1 error code if the input does not start with
 *              a valid ASN.1 BOOLEAN.
 */
int mbedtls_asn1_get_bool(unsigned char **p,
                          const unsigned char *end,
                          int *val);

/**
 * \brief       Retrieve an integer ASN.1 tag and its value.
 *              Updates the pointer to immediately behind the full tag.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p points to the first byte
 *              beyond the ASN.1 element.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param val   On success, the parsed value.
 *
 * \return      0 if successful.
 * \return      An ASN.1 error code if the input does not start with
 *              a valid ASN.1 INTEGER.
 * \return      #MBEDTLS_ERR_ASN1_INVALID_LENGTH if the parsed value does
 *              not fit in an \c int.
 */
int mbedtls_asn1_get_int(unsigned char **p,
                         const unsigned char *end,
                         int *val);

/**
 * \brief       Retrieve an enumerated ASN.1 tag and its value.
 *              Updates the pointer to immediately behind the full tag.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p points to the first byte
 *              beyond the ASN.1 element.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param val   On success, the parsed value.
 *
 * \return      0 if successful.
 * \return      An ASN.1 error code if the input does not start with
 *              a valid ASN.1 ENUMERATED.
 * \return      #MBEDTLS_ERR_ASN1_INVALID_LENGTH if the parsed value does
 *              not fit in an \c int.
 */
int mbedtls_asn1_get_enum(unsigned char **p,
                          const unsigned char *end,
                          int *val);

/**
 * \brief       Retrieve a bitstring ASN.1 tag and its value.
 *              Updates the pointer to immediately behind the full tag.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p is equal to \p end.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param bs    On success, ::mbedtls_asn1_bitstring information about
 *              the parsed value.
 *
 * \return      0 if successful.
 * \return      #MBEDTLS_ERR_ASN1_LENGTH_MISMATCH if the input contains
 *              extra data after a valid BIT STRING.
 * \return      An ASN.1 error code if the input does not start with
 *              a valid ASN.1 BIT STRING.
 */
int mbedtls_asn1_get_bitstring(unsigned char **p, const unsigned char *end,
                               mbedtls_asn1_bitstring *bs);

/**
 * \brief       Retrieve a bitstring ASN.1 tag without unused bits and its
 *              value.
 *              Updates the pointer to the beginning of the bit/octet string.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p points to the first byte
 *              of the content of the BIT STRING.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param len   On success, \c *len is the length of the content in bytes.
 *
 * \return      0 if successful.
 * \return      #MBEDTLS_ERR_ASN1_INVALID_DATA if the input starts with
 *              a valid BIT STRING with a nonzero number of unused bits.
 * \return      An ASN.1 error code if the input does not start with
 *              a valid ASN.1 BIT STRING.
 */
int mbedtls_asn1_get_bitstring_null(unsigned char **p,
                                    const unsigned char *end,
                                    size_t *len);

/**
 * \brief       Parses and splits an ASN.1 "SEQUENCE OF <tag>".
 *              Updates the pointer to immediately behind the full sequence tag.
 *
 * This function allocates memory for the sequence elements. You can free
 * the allocated memory with mbedtls_asn1_sequence_free().
 *
 * \note        On error, this function may return a partial list in \p cur.
 *              You must set `cur->next = NULL` before calling this function!
 *              Otherwise it is impossible to distinguish a previously non-null
 *              pointer from a pointer to an object allocated by this function.
 *
 * \note        If the sequence is empty, this function does not modify
 *              \c *cur. If the sequence is valid and non-empty, this
 *              function sets `cur->buf.tag` to \p tag. This allows
 *              callers to distinguish between an empty sequence and
 *              a one-element sequence.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p is equal to \p end.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param cur   A ::mbedtls_asn1_sequence which this function fills.
 *              When this function returns, \c *cur is the head of a linked
 *              list. Each node in this list is allocated with
 *              mbedtls_calloc() apart from \p cur itself, and should
 *              therefore be freed with mbedtls_free().
 *              The list describes the content of the sequence.
 *              The head of the list (i.e. \c *cur itself) describes the
 *              first element, `*cur->next` describes the second element, etc.
 *              For each element, `buf.tag == tag`, `buf.len` is the length
 *              of the content of the content of the element, and `buf.p`
 *              points to the first byte of the content (i.e. immediately
 *              past the length of the element).
 *              Note that list elements may be allocated even on error.
 * \param tag   Each element of the sequence must have this tag.
 *
 * \return      0 if successful.
 * \return      #MBEDTLS_ERR_ASN1_LENGTH_MISMATCH if the input contains
 *              extra data after a valid SEQUENCE OF \p tag.
 * \return      #MBEDTLS_ERR_ASN1_UNEXPECTED_TAG if the input starts with
 *              an ASN.1 SEQUENCE in which an element has a tag that
 *              is different from \p tag.
 * \return      #MBEDTLS_ERR_ASN1_ALLOC_FAILED if a memory allocation failed.
 * \return      An ASN.1 error code if the input does not start with
 *              a valid ASN.1 SEQUENCE.
 */
int mbedtls_asn1_get_sequence_of(unsigned char **p,
                                 const unsigned char *end,
                                 mbedtls_asn1_sequence *cur,
                                 int tag);
/**
 * \brief          Free a heap-allocated linked list presentation of
 *                 an ASN.1 sequence, including the first element.
 *
 * There are two common ways to manage the memory used for the representation
 * of a parsed ASN.1 sequence:
 * - Allocate a head node `mbedtls_asn1_sequence *head` with mbedtls_calloc().
 *   Pass this node as the `cur` argument to mbedtls_asn1_get_sequence_of().
 *   When you have finished processing the sequence,
 *   call mbedtls_asn1_sequence_free() on `head`.
 * - Allocate a head node `mbedtls_asn1_sequence *head` in any manner,
 *   for example on the stack. Make sure that `head->next == NULL`.
 *   Pass `head` as the `cur` argument to mbedtls_asn1_get_sequence_of().
 *   When you have finished processing the sequence,
 *   call mbedtls_asn1_sequence_free() on `head->cur`,
 *   then free `head` itself in the appropriate manner.
 *
 * \param seq      The address of the first sequence component. This may
 *                 be \c NULL, in which case this functions returns
 *                 immediately.
 */
void mbedtls_asn1_sequence_free(mbedtls_asn1_sequence *seq);

/**
 * \brief                Traverse an ASN.1 SEQUENCE container and
 *                       call a callback for each entry.
 *
 * This function checks that the input is a SEQUENCE of elements that
 * each have a "must" tag, and calls a callback function on the elements
 * that have a "may" tag.
 *
 * For example, to validate that the input is a SEQUENCE of `tag1` and call
 * `cb` on each element, use
 * ```
 * mbedtls_asn1_traverse_sequence_of(&p, end, 0xff, tag1, 0, 0, cb, ctx);
 * ```
 *
 * To validate that the input is a SEQUENCE of ANY and call `cb` on
 * each element, use
 * ```
 * mbedtls_asn1_traverse_sequence_of(&p, end, 0, 0, 0, 0, cb, ctx);
 * ```
 *
 * To validate that the input is a SEQUENCE of CHOICE {NULL, OCTET STRING}
 * and call `cb` on each element that is an OCTET STRING, use
 * ```
 * mbedtls_asn1_traverse_sequence_of(&p, end, 0xfe, 0x04, 0xff, 0x04, cb, ctx);
 * ```
 *
 * The callback is called on the elements with a "may" tag from left to
 * right. If the input is not a valid SEQUENCE of elements with a "must" tag,
 * the callback is called on the elements up to the leftmost point where
 * the input is invalid.
 *
 * \warning              This function is still experimental and may change
 *                       at any time.
 *
 * \param p              The address of the pointer to the beginning of
 *                       the ASN.1 SEQUENCE header. This is updated to
 *                       point to the end of the ASN.1 SEQUENCE container
 *                       on a successful invocation.
 * \param end            The end of the ASN.1 SEQUENCE container.
 * \param tag_must_mask  A mask to be applied to the ASN.1 tags found within
 *                       the SEQUENCE before comparing to \p tag_must_val.
 * \param tag_must_val   The required value of each ASN.1 tag found in the
 *                       SEQUENCE, after masking with \p tag_must_mask.
 *                       Mismatching tags lead to an error.
 *                       For example, a value of \c 0 for both \p tag_must_mask
 *                       and \p tag_must_val means that every tag is allowed,
 *                       while a value of \c 0xFF for \p tag_must_mask means
 *                       that \p tag_must_val is the only allowed tag.
 * \param tag_may_mask   A mask to be applied to the ASN.1 tags found within
 *                       the SEQUENCE before comparing to \p tag_may_val.
 * \param tag_may_val    The desired value of each ASN.1 tag found in the
 *                       SEQUENCE, after masking with \p tag_may_mask.
 *                       Mismatching tags will be silently ignored.
 *                       For example, a value of \c 0 for \p tag_may_mask and
 *                       \p tag_may_val means that any tag will be considered,
 *                       while a value of \c 0xFF for \p tag_may_mask means
 *                       that all tags with value different from \p tag_may_val
 *                       will be ignored.
 * \param cb             The callback to trigger for each component
 *                       in the ASN.1 SEQUENCE that matches \p tag_may_val.
 *                       The callback function is called with the following
 *                       parameters:
 *                       - \p ctx.
 *                       - The tag of the current element.
 *                       - A pointer to the start of the current element's
 *                         content inside the input.
 *                       - The length of the content of the current element.
 *                       If the callback returns a non-zero value,
 *                       the function stops immediately,
 *                       forwarding the callback's return value.
 * \param ctx            The context to be passed to the callback \p cb.
 *
 * \return               \c 0 if successful the entire ASN.1 SEQUENCE
 *                       was traversed without parsing or callback errors.
 * \return               #MBEDTLS_ERR_ASN1_LENGTH_MISMATCH if the input
 *                       contains extra data after a valid SEQUENCE
 *                       of elements with an accepted tag.
 * \return               #MBEDTLS_ERR_ASN1_UNEXPECTED_TAG if the input starts
 *                       with an ASN.1 SEQUENCE in which an element has a tag
 *                       that is not accepted.
 * \return               An ASN.1 error code if the input does not start with
 *                       a valid ASN.1 SEQUENCE.
 * \return               A non-zero error code forwarded from the callback
 *                       \p cb in case the latter returns a non-zero value.
 */
int mbedtls_asn1_traverse_sequence_of(
    unsigned char **p,
    const unsigned char *end,
    unsigned char tag_must_mask, unsigned char tag_must_val,
    unsigned char tag_may_mask, unsigned char tag_may_val,
    int (*cb)(void *ctx, int tag,
              unsigned char *start, size_t len),
    void *ctx);

#if defined(MBEDTLS_BIGNUM_C)
/**
 * \brief       Retrieve an integer ASN.1 tag and its value.
 *              Updates the pointer to immediately behind the full tag.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p points to the first byte
 *              beyond the ASN.1 element.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param X     On success, the parsed value.
 *
 * \return      0 if successful.
 * \return      An ASN.1 error code if the input does not start with
 *              a valid ASN.1 INTEGER.
 * \return      #MBEDTLS_ERR_ASN1_INVALID_LENGTH if the parsed value does
 *              not fit in an \c int.
 * \return      An MPI error code if the parsed value is too large.
 */
int mbedtls_asn1_get_mpi(unsigned char **p,
                         const unsigned char *end,
                         mbedtls_mpi *X);
#endif /* MBEDTLS_BIGNUM_C */

/**
 * \brief       Retrieve an AlgorithmIdentifier ASN.1 sequence.
 *              Updates the pointer to immediately behind the full
 *              AlgorithmIdentifier.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p points to the first byte
 *              beyond the AlgorithmIdentifier element.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param alg   The buffer to receive the OID.
 * \param params The buffer to receive the parameters.
 *              This is zeroized if there are no parameters.
 *
 * \return      0 if successful or a specific ASN.1 or MPI error code.
 */
int mbedtls_asn1_get_alg(unsigned char **p,
                         const unsigned char *end,
                         mbedtls_asn1_buf *alg, mbedtls_asn1_buf *params);

/**
 * \brief       Retrieve an AlgorithmIdentifier ASN.1 sequence with NULL or no
 *              params.
 *              Updates the pointer to immediately behind the full
 *              AlgorithmIdentifier.
 *
 * \param p     On entry, \c *p points to the start of the ASN.1 element.
 *              On successful completion, \c *p points to the first byte
 *              beyond the AlgorithmIdentifier element.
 *              On error, the value of \c *p is undefined.
 * \param end   End of data.
 * \param alg   The buffer to receive the OID.
 *
 * \return      0 if successful or a specific ASN.1 or MPI error code.
 */
int mbedtls_asn1_get_alg_null(unsigned char **p,
                              const unsigned char *end,
                              mbedtls_asn1_buf *alg);

/**
 * \brief       Find a specific named_data entry in a sequence or list based on
 *              the OID.
 *
 * \param list  The list to seek through
 * \param oid   The OID to look for
 * \param len   Size of the OID
 *
 * \return      NULL if not found, or a pointer to the existing entry.
 */
const mbedtls_asn1_named_data *mbedtls_asn1_find_named_data(const mbedtls_asn1_named_data *list,
                                                            const char *oid, size_t len);

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
/**
 * \brief       Free a mbedtls_asn1_named_data entry
 *
 * \deprecated  This function is deprecated and will be removed in a
 *              future version of the library.
 *              Please use mbedtls_asn1_free_named_data_list()
 *              or mbedtls_asn1_free_named_data_list_shallow().
 *
 * \param entry The named data entry to free.
 *              This function calls mbedtls_free() on
 *              `entry->oid.p` and `entry->val.p`.
 */
void MBEDTLS_DEPRECATED mbedtls_asn1_free_named_data(mbedtls_asn1_named_data *entry);
#endif /* MBEDTLS_DEPRECATED_REMOVED */

/**
 * \brief       Free all entries in a mbedtls_asn1_named_data list.
 *
 * \param head  Pointer to the head of the list of named data entries to free.
 *              This function calls mbedtls_free() on
 *              `entry->oid.p` and `entry->val.p` and then on `entry`
 *              for each list entry, and sets \c *head to \c NULL.
 */
void mbedtls_asn1_free_named_data_list(mbedtls_asn1_named_data **head);

/**
 * \brief       Free all shallow entries in a mbedtls_asn1_named_data list,
 *              but do not free internal pointer targets.
 *
 * \param name  Head of the list of named data entries to free.
 *              This function calls mbedtls_free() on each list element.
 */
void mbedtls_asn1_free_named_data_list_shallow(mbedtls_asn1_named_data *name);

/** \} name Functions to parse ASN.1 data structures */
/** \} addtogroup asn1_module */

#endif /* MBEDTLS_ASN1_PARSE_C */

#ifdef __cplusplus
}
#endif

#endif /* asn1.h */
