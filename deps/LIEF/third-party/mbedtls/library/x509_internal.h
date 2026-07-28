/**
 * \file x509.h
 *
 * \brief Internal part of the public "x509.h".
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_X509_INTERNAL_H
#define MBEDTLS_X509_INTERNAL_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include "mbedtls/x509.h"
#include "mbedtls/asn1.h"
#include "pk_internal.h"

#if defined(MBEDTLS_RSA_C)
#include "mbedtls/rsa.h"
#endif

int mbedtls_x509_get_name(unsigned char **p, const unsigned char *end,
                          mbedtls_x509_name *cur);
int mbedtls_x509_get_alg_null(unsigned char **p, const unsigned char *end,
                              mbedtls_x509_buf *alg);
int mbedtls_x509_get_alg(unsigned char **p, const unsigned char *end,
                         mbedtls_x509_buf *alg, mbedtls_x509_buf *params);
#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT)
int mbedtls_x509_get_rsassa_pss_params(const mbedtls_x509_buf *params,
                                       mbedtls_md_type_t *md_alg, mbedtls_md_type_t *mgf_md,
                                       int *salt_len);
#endif
int mbedtls_x509_get_sig(unsigned char **p, const unsigned char *end, mbedtls_x509_buf *sig);
int mbedtls_x509_get_sig_alg(const mbedtls_x509_buf *sig_oid, const mbedtls_x509_buf *sig_params,
                             mbedtls_md_type_t *md_alg, mbedtls_pk_type_t *pk_alg,
                             void **sig_opts);
int mbedtls_x509_get_time(unsigned char **p, const unsigned char *end,
                          mbedtls_x509_time *t);
int mbedtls_x509_get_serial(unsigned char **p, const unsigned char *end,
                            mbedtls_x509_buf *serial);
int mbedtls_x509_get_ext(unsigned char **p, const unsigned char *end,
                         mbedtls_x509_buf *ext, int tag);
#if !defined(MBEDTLS_X509_REMOVE_INFO)
int mbedtls_x509_sig_alg_gets(char *buf, size_t size, const mbedtls_x509_buf *sig_oid,
                              mbedtls_pk_type_t pk_alg, mbedtls_md_type_t md_alg,
                              const void *sig_opts);
#endif
int mbedtls_x509_key_size_helper(char *buf, size_t buf_size, const char *name);
int mbedtls_x509_set_extension(mbedtls_asn1_named_data **head, const char *oid, size_t oid_len,
                               int critical, const unsigned char *val,
                               size_t val_len);
int mbedtls_x509_write_extensions(unsigned char **p, unsigned char *start,
                                  mbedtls_asn1_named_data *first);
int mbedtls_x509_write_names(unsigned char **p, unsigned char *start,
                             mbedtls_asn1_named_data *first);
int mbedtls_x509_write_sig(unsigned char **p, unsigned char *start,
                           const char *oid, size_t oid_len,
                           unsigned char *sig, size_t size,
                           mbedtls_pk_type_t pk_alg);
int mbedtls_x509_get_ns_cert_type(unsigned char **p,
                                  const unsigned char *end,
                                  unsigned char *ns_cert_type);
int mbedtls_x509_get_key_usage(unsigned char **p,
                               const unsigned char *end,
                               unsigned int *key_usage);
int mbedtls_x509_get_subject_alt_name(unsigned char **p,
                                      const unsigned char *end,
                                      mbedtls_x509_sequence *subject_alt_name);
int mbedtls_x509_get_subject_alt_name_ext(unsigned char **p,
                                          const unsigned char *end,
                                          mbedtls_x509_sequence *subject_alt_name);
int mbedtls_x509_info_subject_alt_name(char **buf, size_t *size,
                                       const mbedtls_x509_sequence
                                       *subject_alt_name,
                                       const char *prefix);
int mbedtls_x509_info_cert_type(char **buf, size_t *size,
                                unsigned char ns_cert_type);
int mbedtls_x509_info_key_usage(char **buf, size_t *size,
                                unsigned int key_usage);

int mbedtls_x509_write_set_san_common(mbedtls_asn1_named_data **extensions,
                                      const mbedtls_x509_san_list *san_list);

#endif /* MBEDTLS_X509_INTERNAL_H */
