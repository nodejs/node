/**
 * \file asn1write.h
 *
 * \brief ASN.1 buffer writing functionality
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_ASN1_WRITE_H
#define MBEDTLS_ASN1_WRITE_H

#include "mbedtls/build_info.h"

#include "mbedtls/asn1.h"

#define MBEDTLS_ASN1_CHK_ADD(g, f)                      \
    do                                                  \
    {                                                   \
        if ((ret = (f)) < 0)                         \
        return ret;                              \
        else                                            \
        (g) += ret;                                 \
    } while (0)

#define MBEDTLS_ASN1_CHK_CLEANUP_ADD(g, f)                      \
    do                                                  \
    {                                                   \
        if ((ret = (f)) < 0)                         \
        goto cleanup;                              \
        else                                            \
        (g) += ret;                                 \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MBEDTLS_ASN1_WRITE_C) || defined(MBEDTLS_X509_USE_C) || \
    defined(MBEDTLS_PSA_UTIL_HAVE_ECDSA)
/**
 * \brief           Write a length field in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param len       The length value to write.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_len(unsigned char **p, const unsigned char *start,
                           size_t len);
/**
 * \brief           Write an ASN.1 tag in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param tag       The tag to write.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_tag(unsigned char **p, const unsigned char *start,
                           unsigned char tag);
#endif /* MBEDTLS_ASN1_WRITE_C || MBEDTLS_X509_USE_C || MBEDTLS_PSA_UTIL_HAVE_ECDSA*/

#if defined(MBEDTLS_ASN1_WRITE_C)
/**
 * \brief           Write raw buffer data.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param buf       The data buffer to write.
 * \param size      The length of the data buffer.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_raw_buffer(unsigned char **p, const unsigned char *start,
                                  const unsigned char *buf, size_t size);

#if defined(MBEDTLS_BIGNUM_C)
/**
 * \brief           Write an arbitrary-precision number (#MBEDTLS_ASN1_INTEGER)
 *                  in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param X         The MPI to write.
 *                  It must be non-negative.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_mpi(unsigned char **p, const unsigned char *start,
                           const mbedtls_mpi *X);
#endif /* MBEDTLS_BIGNUM_C */

/**
 * \brief           Write a NULL tag (#MBEDTLS_ASN1_NULL) with zero data
 *                  in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_null(unsigned char **p, const unsigned char *start);

/**
 * \brief           Write an OID tag (#MBEDTLS_ASN1_OID) and data
 *                  in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param oid       The OID to write.
 * \param oid_len   The length of the OID.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_oid(unsigned char **p, const unsigned char *start,
                           const char *oid, size_t oid_len);

/**
 * \brief           Write an AlgorithmIdentifier sequence in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param oid       The OID of the algorithm to write.
 * \param oid_len   The length of the algorithm's OID.
 * \param par_len   The length of the parameters, which must be already written.
 *                  If 0, NULL parameters are added
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_algorithm_identifier(unsigned char **p,
                                            const unsigned char *start,
                                            const char *oid, size_t oid_len,
                                            size_t par_len);

/**
 * \brief           Write an AlgorithmIdentifier sequence in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param oid       The OID of the algorithm to write.
 * \param oid_len   The length of the algorithm's OID.
 * \param par_len   The length of the parameters, which must be already written.
 * \param has_par   If there are any parameters. If 0, par_len must be 0. If 1
 *                  and \p par_len is 0, NULL parameters are added.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_algorithm_identifier_ext(unsigned char **p,
                                                const unsigned char *start,
                                                const char *oid, size_t oid_len,
                                                size_t par_len, int has_par);

/**
 * \brief           Write a boolean tag (#MBEDTLS_ASN1_BOOLEAN) and value
 *                  in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param boolean   The boolean value to write, either \c 0 or \c 1.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_bool(unsigned char **p, const unsigned char *start,
                            int boolean);

/**
 * \brief           Write an int tag (#MBEDTLS_ASN1_INTEGER) and value
 *                  in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param val       The integer value to write.
 *                  It must be non-negative.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_int(unsigned char **p, const unsigned char *start, int val);

/**
 * \brief           Write an enum tag (#MBEDTLS_ASN1_ENUMERATED) and value
 *                  in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param val       The integer value to write.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative \c MBEDTLS_ERR_ASN1_XXX error code on failure.
 */
int mbedtls_asn1_write_enum(unsigned char **p, const unsigned char *start, int val);

/**
 * \brief           Write a string in ASN.1 format using a specific
 *                  string encoding tag.

 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param tag       The string encoding tag to write, e.g.
 *                  #MBEDTLS_ASN1_UTF8_STRING.
 * \param text      The string to write.
 * \param text_len  The length of \p text in bytes (which might
 *                  be strictly larger than the number of characters).
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative error code on failure.
 */
int mbedtls_asn1_write_tagged_string(unsigned char **p, const unsigned char *start,
                                     int tag, const char *text,
                                     size_t text_len);

/**
 * \brief           Write a string in ASN.1 format using the PrintableString
 *                  string encoding tag (#MBEDTLS_ASN1_PRINTABLE_STRING).
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param text      The string to write.
 * \param text_len  The length of \p text in bytes (which might
 *                  be strictly larger than the number of characters).
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative error code on failure.
 */
int mbedtls_asn1_write_printable_string(unsigned char **p,
                                        const unsigned char *start,
                                        const char *text, size_t text_len);

/**
 * \brief           Write a UTF8 string in ASN.1 format using the UTF8String
 *                  string encoding tag (#MBEDTLS_ASN1_UTF8_STRING).
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param text      The string to write.
 * \param text_len  The length of \p text in bytes (which might
 *                  be strictly larger than the number of characters).
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative error code on failure.
 */
int mbedtls_asn1_write_utf8_string(unsigned char **p, const unsigned char *start,
                                   const char *text, size_t text_len);

/**
 * \brief           Write a string in ASN.1 format using the IA5String
 *                  string encoding tag (#MBEDTLS_ASN1_IA5_STRING).
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param text      The string to write.
 * \param text_len  The length of \p text in bytes (which might
 *                  be strictly larger than the number of characters).
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative error code on failure.
 */
int mbedtls_asn1_write_ia5_string(unsigned char **p, const unsigned char *start,
                                  const char *text, size_t text_len);

/**
 * \brief           Write a bitstring tag (#MBEDTLS_ASN1_BIT_STRING) and
 *                  value in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param buf       The bitstring to write.
 * \param bits      The total number of bits in the bitstring.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative error code on failure.
 */
int mbedtls_asn1_write_bitstring(unsigned char **p, const unsigned char *start,
                                 const unsigned char *buf, size_t bits);

/**
 * \brief           This function writes a named bitstring tag
 *                  (#MBEDTLS_ASN1_BIT_STRING) and value in ASN.1 format.
 *
 *                  As stated in RFC 5280 Appendix B, trailing zeroes are
 *                  omitted when encoding named bitstrings in DER.
 *
 * \note            This function works backwards within the data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer which is used for bounds-checking.
 * \param buf       The bitstring to write.
 * \param bits      The total number of bits in the bitstring.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative error code on failure.
 */
int mbedtls_asn1_write_named_bitstring(unsigned char **p,
                                       const unsigned char *start,
                                       const unsigned char *buf,
                                       size_t bits);

/**
 * \brief           Write an octet string tag (#MBEDTLS_ASN1_OCTET_STRING)
 *                  and value in ASN.1 format.
 *
 * \note            This function works backwards in data buffer.
 *
 * \param p         The reference to the current position pointer.
 * \param start     The start of the buffer, for bounds-checking.
 * \param buf       The buffer holding the data to write.
 * \param size      The length of the data buffer \p buf.
 *
 * \return          The number of bytes written to \p p on success.
 * \return          A negative error code on failure.
 */
int mbedtls_asn1_write_octet_string(unsigned char **p, const unsigned char *start,
                                    const unsigned char *buf, size_t size);

/**
 * \brief           Create or find a specific named_data entry for writing in a
 *                  sequence or list based on the OID. If not already in there,
 *                  a new entry is added to the head of the list.
 *                  Warning: Destructive behaviour for the val data!
 *
 * \param list      The pointer to the location of the head of the list to seek
 *                  through (will be updated in case of a new entry).
 * \param oid       The OID to look for.
 * \param oid_len   The size of the OID.
 * \param val       The associated data to store. If this is \c NULL,
 *                  no data is copied to the new or existing buffer.
 * \param val_len   The minimum length of the data buffer needed.
 *                  If this is 0, do not allocate a buffer for the associated
 *                  data.
 *                  If the OID was already present, enlarge, shrink or free
 *                  the existing buffer to fit \p val_len.
 *
 * \return          A pointer to the new / existing entry on success.
 * \return          \c NULL if there was a memory allocation error.
 */
mbedtls_asn1_named_data *mbedtls_asn1_store_named_data(mbedtls_asn1_named_data **list,
                                                       const char *oid, size_t oid_len,
                                                       const unsigned char *val,
                                                       size_t val_len);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_ASN1_WRITE_C */

#endif /* MBEDTLS_ASN1_WRITE_H */
