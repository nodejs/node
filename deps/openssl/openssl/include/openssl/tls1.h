/*
 * Copyright 1995-2024 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved
 * Copyright 2005 Nokia. All rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OPENSSL_TLS1_H
# define OPENSSL_TLS1_H
# pragma once

# include <openssl/macros.h>
# ifndef OPENSSL_NO_DEPRECATED_3_0
#  define HEADER_TLS1_H
# endif

# include <openssl/buffer.h>
# include <openssl/x509.h>
# include <openssl/prov_ssl.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* Default security level if not overridden at config time */
# ifndef OPENSSL_TLS_SECURITY_LEVEL
#  define OPENSSL_TLS_SECURITY_LEVEL 2
# endif

/* TLS*_VERSION constants are defined in prov_ssl.h */
# ifndef OPENSSL_NO_DEPRECATED_3_0
#  define TLS_MAX_VERSION                TLS1_3_VERSION
# endif

/* Special value for method supporting multiple versions */
# define TLS_ANY_VERSION                 0x10000

# define TLS1_VERSION_MAJOR              0x03
# define TLS1_VERSION_MINOR              0x01

# define TLS1_1_VERSION_MAJOR            0x03
# define TLS1_1_VERSION_MINOR            0x02

# define TLS1_2_VERSION_MAJOR            0x03
# define TLS1_2_VERSION_MINOR            0x03

# define TLS1_get_version(s) \
        ((SSL_version(s) >> 8) == TLS1_VERSION_MAJOR ? SSL_version(s) : 0)

# define TLS1_get_client_version(s) \
        ((SSL_client_version(s) >> 8) == TLS1_VERSION_MAJOR ? SSL_client_version(s) : 0)

# define TLS1_AD_DECRYPTION_FAILED       21
# define TLS1_AD_RECORD_OVERFLOW         22
# define TLS1_AD_UNKNOWN_CA              48/* fatal */
# define TLS1_AD_ACCESS_DENIED           49/* fatal */
# define TLS1_AD_DECODE_ERROR            50/* fatal */
# define TLS1_AD_DECRYPT_ERROR           51
# define TLS1_AD_EXPORT_RESTRICTION      60/* fatal */
# define TLS1_AD_PROTOCOL_VERSION        70/* fatal */
# define TLS1_AD_INSUFFICIENT_SECURITY   71/* fatal */
# define TLS1_AD_INTERNAL_ERROR          80/* fatal */
# define TLS1_AD_INAPPROPRIATE_FALLBACK  86/* fatal */
# define TLS1_AD_USER_CANCELLED          90
# define TLS1_AD_NO_RENEGOTIATION        100
/* TLSv1.3 alerts */
# define TLS13_AD_MISSING_EXTENSION      109 /* fatal */
# define TLS13_AD_CERTIFICATE_REQUIRED   116 /* fatal */
/* codes 110-114 are from RFC3546 */
# define TLS1_AD_UNSUPPORTED_EXTENSION   110
# define TLS1_AD_CERTIFICATE_UNOBTAINABLE 111
# define TLS1_AD_UNRECOGNIZED_NAME       112
# define TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE 113
# define TLS1_AD_BAD_CERTIFICATE_HASH_VALUE 114
# define TLS1_AD_UNKNOWN_PSK_IDENTITY    115/* fatal */
# define TLS1_AD_NO_APPLICATION_PROTOCOL 120 /* fatal */

/* ExtensionType values from RFC3546 / RFC4366 / RFC6066 */
# define TLSEXT_TYPE_server_name                 0
# define TLSEXT_TYPE_max_fragment_length         1
# define TLSEXT_TYPE_client_certificate_url      2
# define TLSEXT_TYPE_trusted_ca_keys             3
# define TLSEXT_TYPE_truncated_hmac              4
# define TLSEXT_TYPE_status_request              5
/* ExtensionType values from RFC4681 */
# define TLSEXT_TYPE_user_mapping                6
/* ExtensionType values from RFC5878 */
# define TLSEXT_TYPE_client_authz                7
# define TLSEXT_TYPE_server_authz                8
/* ExtensionType values from RFC6091 */
# define TLSEXT_TYPE_cert_type           9

/* ExtensionType values from RFC4492 */
/*
 * Prior to TLSv1.3 the supported_groups extension was known as
 * elliptic_curves
 */
# define TLSEXT_TYPE_supported_groups            10
# define TLSEXT_TYPE_elliptic_curves             TLSEXT_TYPE_supported_groups
# define TLSEXT_TYPE_ec_point_formats            11


/* ExtensionType value from RFC5054 */
# define TLSEXT_TYPE_srp                         12

/* ExtensionType values from RFC5246 */
# define TLSEXT_TYPE_signature_algorithms        13

/* ExtensionType value from RFC5764 */
# define TLSEXT_TYPE_use_srtp    14

/* ExtensionType value from RFC7301 */
# define TLSEXT_TYPE_application_layer_protocol_negotiation 16

/*
 * Extension type for Certificate Transparency
 * https://tools.ietf.org/html/rfc6962#section-3.3.1
 */
# define TLSEXT_TYPE_signed_certificate_timestamp    18

/*
 * Extension type for Raw Public Keys
 * https://tools.ietf.org/html/rfc7250
 * https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml
 */
# define TLSEXT_TYPE_client_cert_type   19
# define TLSEXT_TYPE_server_cert_type   20

/*
 * ExtensionType value for TLS padding extension.
 * http://tools.ietf.org/html/draft-agl-tls-padding
 */
# define TLSEXT_TYPE_padding     21

/* ExtensionType value from RFC7366 */
# define TLSEXT_TYPE_encrypt_then_mac    22

/* ExtensionType value from RFC7627 */
# define TLSEXT_TYPE_extended_master_secret      23

/* ExtensionType value from RFC8879 */
# define TLSEXT_TYPE_compress_certificate        27

/* ExtensionType value from RFC4507 */
# define TLSEXT_TYPE_session_ticket              35

/* As defined for TLS1.3 */
# define TLSEXT_TYPE_psk                         41
# define TLSEXT_TYPE_early_data                  42
# define TLSEXT_TYPE_supported_versions          43
# define TLSEXT_TYPE_cookie                      44
# define TLSEXT_TYPE_psk_kex_modes               45
# define TLSEXT_TYPE_certificate_authorities     47
# define TLSEXT_TYPE_post_handshake_auth         49
# define TLSEXT_TYPE_signature_algorithms_cert   50
# define TLSEXT_TYPE_key_share                   51
# define TLSEXT_TYPE_quic_transport_parameters   57

/* Temporary extension type */
# define TLSEXT_TYPE_renegotiate                 0xff01

# ifndef OPENSSL_NO_NEXTPROTONEG
/* This is not an IANA defined extension number */
#  define TLSEXT_TYPE_next_proto_neg              13172
# endif

/* NameType value from RFC3546 */
# define TLSEXT_NAMETYPE_host_name 0
/* status request value from RFC3546 */
# define TLSEXT_STATUSTYPE_ocsp 1

/* ECPointFormat values from RFC4492 */
# define TLSEXT_ECPOINTFORMAT_first                      0
# define TLSEXT_ECPOINTFORMAT_uncompressed               0
# define TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime  1
# define TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2  2
# define TLSEXT_ECPOINTFORMAT_last                       2

/* Signature and hash algorithms from RFC5246 */
# define TLSEXT_signature_anonymous                      0
# define TLSEXT_signature_rsa                            1
# define TLSEXT_signature_dsa                            2
# define TLSEXT_signature_ecdsa                          3
# define TLSEXT_signature_gostr34102001                  237
# define TLSEXT_signature_gostr34102012_256              238
# define TLSEXT_signature_gostr34102012_512              239

/* Total number of different signature algorithms */
# define TLSEXT_signature_num                            7

# define TLSEXT_hash_none                                0
# define TLSEXT_hash_md5                                 1
# define TLSEXT_hash_sha1                                2
# define TLSEXT_hash_sha224                              3
# define TLSEXT_hash_sha256                              4
# define TLSEXT_hash_sha384                              5
# define TLSEXT_hash_sha512                              6
# define TLSEXT_hash_gostr3411                           237
# define TLSEXT_hash_gostr34112012_256                   238
# define TLSEXT_hash_gostr34112012_512                   239

/* Total number of different digest algorithms */

# define TLSEXT_hash_num                                 10

/* Possible compression values from RFC8879 */
/* Not defined in RFC8879, but used internally for no-compression */
# define TLSEXT_comp_cert_none                            0
# define TLSEXT_comp_cert_zlib                            1
# define TLSEXT_comp_cert_brotli                          2
# define TLSEXT_comp_cert_zstd                            3
/* one more than the number of defined values - used as size of 0-terminated array */
# define TLSEXT_comp_cert_limit                           4

/* Flag set for unrecognised algorithms */
# define TLSEXT_nid_unknown                              0x1000000

/* ECC curves */

# define TLSEXT_curve_P_256                              23
# define TLSEXT_curve_P_384                              24

/* OpenSSL value to disable maximum fragment length extension */
# define TLSEXT_max_fragment_length_DISABLED    0
/* Allowed values for max fragment length extension */
# define TLSEXT_max_fragment_length_512         1
# define TLSEXT_max_fragment_length_1024        2
# define TLSEXT_max_fragment_length_2048        3
# define TLSEXT_max_fragment_length_4096        4
/* OpenSSL value for unset maximum fragment length extension */
# define TLSEXT_max_fragment_length_UNSPECIFIED 255

/*
 * TLS Certificate Type (for RFC7250)
 * https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml#tls-extensiontype-values-3
 */
# define TLSEXT_cert_type_x509         0
# define TLSEXT_cert_type_pgp          1 /* recognized, but not supported */
# define TLSEXT_cert_type_rpk          2
# define TLSEXT_cert_type_1609dot2     3 /* recognized, but not supported */

int SSL_CTX_set_tlsext_max_fragment_length(SSL_CTX *ctx, uint8_t mode);
int SSL_set_tlsext_max_fragment_length(SSL *ssl, uint8_t mode);

# define TLSEXT_MAXLEN_host_name 255

__owur const char *SSL_get_servername(const SSL *s, const int type);
__owur int SSL_get_servername_type(const SSL *s);
/*
 * SSL_export_keying_material exports a value derived from the master secret,
 * as specified in RFC 5705. It writes |olen| bytes to |out| given a label and
 * optional context. (Since a zero length context is allowed, the |use_context|
 * flag controls whether a context is included.) It returns 1 on success and
 * 0 or -1 otherwise.
 */
__owur int SSL_export_keying_material(SSL *s, unsigned char *out, size_t olen,
                                      const char *label, size_t llen,
                                      const unsigned char *context,
                                      size_t contextlen, int use_context);

/*
 * SSL_export_keying_material_early exports a value derived from the
 * early exporter master secret, as specified in
 * https://tools.ietf.org/html/draft-ietf-tls-tls13-23. It writes
 * |olen| bytes to |out| given a label and optional context. It
 * returns 1 on success and 0 otherwise.
 */
__owur int SSL_export_keying_material_early(SSL *s, unsigned char *out,
                                            size_t olen, const char *label,
                                            size_t llen,
                                            const unsigned char *context,
                                            size_t contextlen);

int SSL_get_peer_signature_type_nid(const SSL *s, int *pnid);
int SSL_get_signature_type_nid(const SSL *s, int *pnid);

int SSL_get_sigalgs(SSL *s, int idx,
                    int *psign, int *phash, int *psignandhash,
                    unsigned char *rsig, unsigned char *rhash);

char *SSL_get1_builtin_sigalgs(OSSL_LIB_CTX *libctx);

int SSL_get_shared_sigalgs(SSL *s, int idx,
                           int *psign, int *phash, int *psignandhash,
                           unsigned char *rsig, unsigned char *rhash);

__owur int SSL_check_chain(SSL *s, X509 *x, EVP_PKEY *pk, STACK_OF(X509) *chain);

# define SSL_set_tlsext_host_name(s,name) \
        SSL_ctrl(s,SSL_CTRL_SET_TLSEXT_HOSTNAME,TLSEXT_NAMETYPE_host_name,\
                (void *)name)

# define SSL_set_tlsext_debug_callback(ssl, cb) \
        SSL_callback_ctrl(ssl,SSL_CTRL_SET_TLSEXT_DEBUG_CB,\
                (void (*)(void))cb)

# define SSL_set_tlsext_debug_arg(ssl, arg) \
        SSL_ctrl(ssl,SSL_CTRL_SET_TLSEXT_DEBUG_ARG,0,arg)

# define SSL_get_tlsext_status_type(ssl) \
        SSL_ctrl(ssl,SSL_CTRL_GET_TLSEXT_STATUS_REQ_TYPE,0,NULL)

# define SSL_set_tlsext_status_type(ssl, type) \
        SSL_ctrl(ssl,SSL_CTRL_SET_TLSEXT_STATUS_REQ_TYPE,type,NULL)

# define SSL_get_tlsext_status_exts(ssl, arg) \
        SSL_ctrl(ssl,SSL_CTRL_GET_TLSEXT_STATUS_REQ_EXTS,0,arg)

# define SSL_set_tlsext_status_exts(ssl, arg) \
        SSL_ctrl(ssl,SSL_CTRL_SET_TLSEXT_STATUS_REQ_EXTS,0,arg)

# define SSL_get_tlsext_status_ids(ssl, arg) \
        SSL_ctrl(ssl,SSL_CTRL_GET_TLSEXT_STATUS_REQ_IDS,0,arg)

# define SSL_set_tlsext_status_ids(ssl, arg) \
        SSL_ctrl(ssl,SSL_CTRL_SET_TLSEXT_STATUS_REQ_IDS,0,arg)

# define SSL_get_tlsext_status_ocsp_resp(ssl, arg) \
        SSL_ctrl(ssl,SSL_CTRL_GET_TLSEXT_STATUS_REQ_OCSP_RESP,0,arg)

# define SSL_set_tlsext_status_ocsp_resp(ssl, arg, arglen) \
        SSL_ctrl(ssl,SSL_CTRL_SET_TLSEXT_STATUS_REQ_OCSP_RESP,arglen,arg)

# define SSL_CTX_set_tlsext_servername_callback(ctx, cb) \
        SSL_CTX_callback_ctrl(ctx,SSL_CTRL_SET_TLSEXT_SERVERNAME_CB,\
                (void (*)(void))cb)

# define SSL_TLSEXT_ERR_OK 0
# define SSL_TLSEXT_ERR_ALERT_WARNING 1
# define SSL_TLSEXT_ERR_ALERT_FATAL 2
# define SSL_TLSEXT_ERR_NOACK 3

# define SSL_CTX_set_tlsext_servername_arg(ctx, arg) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG,0,arg)

# define SSL_CTX_get_tlsext_ticket_keys(ctx, keys, keylen) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_GET_TLSEXT_TICKET_KEYS,keylen,keys)
# define SSL_CTX_set_tlsext_ticket_keys(ctx, keys, keylen) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_SET_TLSEXT_TICKET_KEYS,keylen,keys)

# define SSL_CTX_get_tlsext_status_cb(ssl, cb) \
        SSL_CTX_ctrl(ssl,SSL_CTRL_GET_TLSEXT_STATUS_REQ_CB,0,(void *)cb)
# define SSL_CTX_set_tlsext_status_cb(ssl, cb) \
        SSL_CTX_callback_ctrl(ssl,SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB,\
                (void (*)(void))cb)

# define SSL_CTX_get_tlsext_status_arg(ssl, arg) \
        SSL_CTX_ctrl(ssl,SSL_CTRL_GET_TLSEXT_STATUS_REQ_CB_ARG,0,arg)
# define SSL_CTX_set_tlsext_status_arg(ssl, arg) \
        SSL_CTX_ctrl(ssl,SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB_ARG,0,arg)

# define SSL_CTX_set_tlsext_status_type(ssl, type) \
        SSL_CTX_ctrl(ssl,SSL_CTRL_SET_TLSEXT_STATUS_REQ_TYPE,type,NULL)

# define SSL_CTX_get_tlsext_status_type(ssl) \
        SSL_CTX_ctrl(ssl,SSL_CTRL_GET_TLSEXT_STATUS_REQ_TYPE,0,NULL)

# ifndef OPENSSL_NO_DEPRECATED_3_0
#  define SSL_CTX_set_tlsext_ticket_key_cb(ssl, cb) \
        SSL_CTX_callback_ctrl(ssl,SSL_CTRL_SET_TLSEXT_TICKET_KEY_CB,\
                (void (*)(void))cb)
# endif
int SSL_CTX_set_tlsext_ticket_key_evp_cb
    (SSL_CTX *ctx, int (*fp)(SSL *, unsigned char *, unsigned char *,
                             EVP_CIPHER_CTX *, EVP_MAC_CTX *, int));

/* PSK ciphersuites from 4279 */
# define TLS1_CK_PSK_WITH_RC4_128_SHA                    0x0300008A
# define TLS1_CK_PSK_WITH_3DES_EDE_CBC_SHA               0x0300008B
# define TLS1_CK_PSK_WITH_AES_128_CBC_SHA                0x0300008C
# define TLS1_CK_PSK_WITH_AES_256_CBC_SHA                0x0300008D
# define TLS1_CK_DHE_PSK_WITH_RC4_128_SHA                0x0300008E
# define TLS1_CK_DHE_PSK_WITH_3DES_EDE_CBC_SHA           0x0300008F
# define TLS1_CK_DHE_PSK_WITH_AES_128_CBC_SHA            0x03000090
# define TLS1_CK_DHE_PSK_WITH_AES_256_CBC_SHA            0x03000091
# define TLS1_CK_RSA_PSK_WITH_RC4_128_SHA                0x03000092
# define TLS1_CK_RSA_PSK_WITH_3DES_EDE_CBC_SHA           0x03000093
# define TLS1_CK_RSA_PSK_WITH_AES_128_CBC_SHA            0x03000094
# define TLS1_CK_RSA_PSK_WITH_AES_256_CBC_SHA            0x03000095

/* PSK ciphersuites from 5487 */
# define TLS1_CK_PSK_WITH_AES_128_GCM_SHA256             0x030000A8
# define TLS1_CK_PSK_WITH_AES_256_GCM_SHA384             0x030000A9
# define TLS1_CK_DHE_PSK_WITH_AES_128_GCM_SHA256         0x030000AA
# define TLS1_CK_DHE_PSK_WITH_AES_256_GCM_SHA384         0x030000AB
# define TLS1_CK_RSA_PSK_WITH_AES_128_GCM_SHA256         0x030000AC
# define TLS1_CK_RSA_PSK_WITH_AES_256_GCM_SHA384         0x030000AD
# define TLS1_CK_PSK_WITH_AES_128_CBC_SHA256             0x030000AE
# define TLS1_CK_PSK_WITH_AES_256_CBC_SHA384             0x030000AF
# define TLS1_CK_PSK_WITH_NULL_SHA256                    0x030000B0
# define TLS1_CK_PSK_WITH_NULL_SHA384                    0x030000B1
# define TLS1_CK_DHE_PSK_WITH_AES_128_CBC_SHA256         0x030000B2
# define TLS1_CK_DHE_PSK_WITH_AES_256_CBC_SHA384         0x030000B3
# define TLS1_CK_DHE_PSK_WITH_NULL_SHA256                0x030000B4
# define TLS1_CK_DHE_PSK_WITH_NULL_SHA384                0x030000B5
# define TLS1_CK_RSA_PSK_WITH_AES_128_CBC_SHA256         0x030000B6
# define TLS1_CK_RSA_PSK_WITH_AES_256_CBC_SHA384         0x030000B7
# define TLS1_CK_RSA_PSK_WITH_NULL_SHA256                0x030000B8
# define TLS1_CK_RSA_PSK_WITH_NULL_SHA384                0x030000B9

/* NULL PSK ciphersuites from RFC4785 */
# define TLS1_CK_PSK_WITH_NULL_SHA                       0x0300002C
# define TLS1_CK_DHE_PSK_WITH_NULL_SHA                   0x0300002D
# define TLS1_CK_RSA_PSK_WITH_NULL_SHA                   0x0300002E

/* AES ciphersuites from RFC3268 */
# define TLS1_CK_RSA_WITH_AES_128_SHA                    0x0300002F
# define TLS1_CK_DH_DSS_WITH_AES_128_SHA                 0x03000030
# define TLS1_CK_DH_RSA_WITH_AES_128_SHA                 0x03000031
# define TLS1_CK_DHE_DSS_WITH_AES_128_SHA                0x03000032
# define TLS1_CK_DHE_RSA_WITH_AES_128_SHA                0x03000033
# define TLS1_CK_ADH_WITH_AES_128_SHA                    0x03000034
# define TLS1_CK_RSA_WITH_AES_256_SHA                    0x03000035
# define TLS1_CK_DH_DSS_WITH_AES_256_SHA                 0x03000036
# define TLS1_CK_DH_RSA_WITH_AES_256_SHA                 0x03000037
# define TLS1_CK_DHE_DSS_WITH_AES_256_SHA                0x03000038
# define TLS1_CK_DHE_RSA_WITH_AES_256_SHA                0x03000039
# define TLS1_CK_ADH_WITH_AES_256_SHA                    0x0300003A

/* TLS v1.2 ciphersuites */
# define TLS1_CK_RSA_WITH_NULL_SHA256                    0x0300003B
# define TLS1_CK_RSA_WITH_AES_128_SHA256                 0x0300003C
# define TLS1_CK_RSA_WITH_AES_256_SHA256                 0x0300003D
# define TLS1_CK_DH_DSS_WITH_AES_128_SHA256              0x0300003E
# define TLS1_CK_DH_RSA_WITH_AES_128_SHA256              0x0300003F
# define TLS1_CK_DHE_DSS_WITH_AES_128_SHA256             0x03000040

/* Camellia ciphersuites from RFC4132 */
# define TLS1_CK_RSA_WITH_CAMELLIA_128_CBC_SHA           0x03000041
# define TLS1_CK_DH_DSS_WITH_CAMELLIA_128_CBC_SHA        0x03000042
# define TLS1_CK_DH_RSA_WITH_CAMELLIA_128_CBC_SHA        0x03000043
# define TLS1_CK_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA       0x03000044
# define TLS1_CK_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA       0x03000045
# define TLS1_CK_ADH_WITH_CAMELLIA_128_CBC_SHA           0x03000046

/* TLS v1.2 ciphersuites */
# define TLS1_CK_DHE_RSA_WITH_AES_128_SHA256             0x03000067
# define TLS1_CK_DH_DSS_WITH_AES_256_SHA256              0x03000068
# define TLS1_CK_DH_RSA_WITH_AES_256_SHA256              0x03000069
# define TLS1_CK_DHE_DSS_WITH_AES_256_SHA256             0x0300006A
# define TLS1_CK_DHE_RSA_WITH_AES_256_SHA256             0x0300006B
# define TLS1_CK_ADH_WITH_AES_128_SHA256                 0x0300006C
# define TLS1_CK_ADH_WITH_AES_256_SHA256                 0x0300006D

/* Camellia ciphersuites from RFC4132 */
# define TLS1_CK_RSA_WITH_CAMELLIA_256_CBC_SHA           0x03000084
# define TLS1_CK_DH_DSS_WITH_CAMELLIA_256_CBC_SHA        0x03000085
# define TLS1_CK_DH_RSA_WITH_CAMELLIA_256_CBC_SHA        0x03000086
# define TLS1_CK_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA       0x03000087
# define TLS1_CK_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA       0x03000088
# define TLS1_CK_ADH_WITH_CAMELLIA_256_CBC_SHA           0x03000089

/* SEED ciphersuites from RFC4162 */
# define TLS1_CK_RSA_WITH_SEED_SHA                       0x03000096
# define TLS1_CK_DH_DSS_WITH_SEED_SHA                    0x03000097
# define TLS1_CK_DH_RSA_WITH_SEED_SHA                    0x03000098
# define TLS1_CK_DHE_DSS_WITH_SEED_SHA                   0x03000099
# define TLS1_CK_DHE_RSA_WITH_SEED_SHA                   0x0300009A
# define TLS1_CK_ADH_WITH_SEED_SHA                       0x0300009B

/* TLS v1.2 GCM ciphersuites from RFC5288 */
# define TLS1_CK_RSA_WITH_AES_128_GCM_SHA256             0x0300009C
# define TLS1_CK_RSA_WITH_AES_256_GCM_SHA384             0x0300009D
# define TLS1_CK_DHE_RSA_WITH_AES_128_GCM_SHA256         0x0300009E
# define TLS1_CK_DHE_RSA_WITH_AES_256_GCM_SHA384         0x0300009F
# define TLS1_CK_DH_RSA_WITH_AES_128_GCM_SHA256          0x030000A0
# define TLS1_CK_DH_RSA_WITH_AES_256_GCM_SHA384          0x030000A1
# define TLS1_CK_DHE_DSS_WITH_AES_128_GCM_SHA256         0x030000A2
# define TLS1_CK_DHE_DSS_WITH_AES_256_GCM_SHA384         0x030000A3
# define TLS1_CK_DH_DSS_WITH_AES_128_GCM_SHA256          0x030000A4
# define TLS1_CK_DH_DSS_WITH_AES_256_GCM_SHA384          0x030000A5
# define TLS1_CK_ADH_WITH_AES_128_GCM_SHA256             0x030000A6
# define TLS1_CK_ADH_WITH_AES_256_GCM_SHA384             0x030000A7

/* CCM ciphersuites from RFC6655 */
# define TLS1_CK_RSA_WITH_AES_128_CCM                    0x0300C09C
# define TLS1_CK_RSA_WITH_AES_256_CCM                    0x0300C09D
# define TLS1_CK_DHE_RSA_WITH_AES_128_CCM                0x0300C09E
# define TLS1_CK_DHE_RSA_WITH_AES_256_CCM                0x0300C09F
# define TLS1_CK_RSA_WITH_AES_128_CCM_8                  0x0300C0A0
# define TLS1_CK_RSA_WITH_AES_256_CCM_8                  0x0300C0A1
# define TLS1_CK_DHE_RSA_WITH_AES_128_CCM_8              0x0300C0A2
# define TLS1_CK_DHE_RSA_WITH_AES_256_CCM_8              0x0300C0A3
# define TLS1_CK_PSK_WITH_AES_128_CCM                    0x0300C0A4
# define TLS1_CK_PSK_WITH_AES_256_CCM                    0x0300C0A5
# define TLS1_CK_DHE_PSK_WITH_AES_128_CCM                0x0300C0A6
# define TLS1_CK_DHE_PSK_WITH_AES_256_CCM                0x0300C0A7
# define TLS1_CK_PSK_WITH_AES_128_CCM_8                  0x0300C0A8
# define TLS1_CK_PSK_WITH_AES_256_CCM_8                  0x0300C0A9
# define TLS1_CK_DHE_PSK_WITH_AES_128_CCM_8              0x0300C0AA
# define TLS1_CK_DHE_PSK_WITH_AES_256_CCM_8              0x0300C0AB

/* CCM ciphersuites from RFC7251 */
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CCM            0x0300C0AC
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CCM            0x0300C0AD
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CCM_8          0x0300C0AE
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CCM_8          0x0300C0AF

/* TLS 1.2 Camellia SHA-256 ciphersuites from RFC5932 */
# define TLS1_CK_RSA_WITH_CAMELLIA_128_CBC_SHA256                0x030000BA
# define TLS1_CK_DH_DSS_WITH_CAMELLIA_128_CBC_SHA256             0x030000BB
# define TLS1_CK_DH_RSA_WITH_CAMELLIA_128_CBC_SHA256             0x030000BC
# define TLS1_CK_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256            0x030000BD
# define TLS1_CK_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256            0x030000BE
# define TLS1_CK_ADH_WITH_CAMELLIA_128_CBC_SHA256                0x030000BF

# define TLS1_CK_RSA_WITH_CAMELLIA_256_CBC_SHA256                0x030000C0
# define TLS1_CK_DH_DSS_WITH_CAMELLIA_256_CBC_SHA256             0x030000C1
# define TLS1_CK_DH_RSA_WITH_CAMELLIA_256_CBC_SHA256             0x030000C2
# define TLS1_CK_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256            0x030000C3
# define TLS1_CK_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256            0x030000C4
# define TLS1_CK_ADH_WITH_CAMELLIA_256_CBC_SHA256                0x030000C5

/* ECC ciphersuites from RFC4492 */
# define TLS1_CK_ECDH_ECDSA_WITH_NULL_SHA                0x0300C001
# define TLS1_CK_ECDH_ECDSA_WITH_RC4_128_SHA             0x0300C002
# define TLS1_CK_ECDH_ECDSA_WITH_DES_192_CBC3_SHA        0x0300C003
# define TLS1_CK_ECDH_ECDSA_WITH_AES_128_CBC_SHA         0x0300C004
# define TLS1_CK_ECDH_ECDSA_WITH_AES_256_CBC_SHA         0x0300C005

# define TLS1_CK_ECDHE_ECDSA_WITH_NULL_SHA               0x0300C006
# define TLS1_CK_ECDHE_ECDSA_WITH_RC4_128_SHA            0x0300C007
# define TLS1_CK_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA       0x0300C008
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CBC_SHA        0x0300C009
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CBC_SHA        0x0300C00A

# define TLS1_CK_ECDH_RSA_WITH_NULL_SHA                  0x0300C00B
# define TLS1_CK_ECDH_RSA_WITH_RC4_128_SHA               0x0300C00C
# define TLS1_CK_ECDH_RSA_WITH_DES_192_CBC3_SHA          0x0300C00D
# define TLS1_CK_ECDH_RSA_WITH_AES_128_CBC_SHA           0x0300C00E
# define TLS1_CK_ECDH_RSA_WITH_AES_256_CBC_SHA           0x0300C00F

# define TLS1_CK_ECDHE_RSA_WITH_NULL_SHA                 0x0300C010
# define TLS1_CK_ECDHE_RSA_WITH_RC4_128_SHA              0x0300C011
# define TLS1_CK_ECDHE_RSA_WITH_DES_192_CBC3_SHA         0x0300C012
# define TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA          0x0300C013
# define TLS1_CK_ECDHE_RSA_WITH_AES_256_CBC_SHA          0x0300C014

# define TLS1_CK_ECDH_anon_WITH_NULL_SHA                 0x0300C015
# define TLS1_CK_ECDH_anon_WITH_RC4_128_SHA              0x0300C016
# define TLS1_CK_ECDH_anon_WITH_DES_192_CBC3_SHA         0x0300C017
# define TLS1_CK_ECDH_anon_WITH_AES_128_CBC_SHA          0x0300C018
# define TLS1_CK_ECDH_anon_WITH_AES_256_CBC_SHA          0x0300C019

/* SRP ciphersuites from RFC 5054 */
# define TLS1_CK_SRP_SHA_WITH_3DES_EDE_CBC_SHA           0x0300C01A
# define TLS1_CK_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA       0x0300C01B
# define TLS1_CK_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA       0x0300C01C
# define TLS1_CK_SRP_SHA_WITH_AES_128_CBC_SHA            0x0300C01D
# define TLS1_CK_SRP_SHA_RSA_WITH_AES_128_CBC_SHA        0x0300C01E
# define TLS1_CK_SRP_SHA_DSS_WITH_AES_128_CBC_SHA        0x0300C01F
# define TLS1_CK_SRP_SHA_WITH_AES_256_CBC_SHA            0x0300C020
# define TLS1_CK_SRP_SHA_RSA_WITH_AES_256_CBC_SHA        0x0300C021
# define TLS1_CK_SRP_SHA_DSS_WITH_AES_256_CBC_SHA        0x0300C022

/* ECDH HMAC based ciphersuites from RFC5289 */
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_128_SHA256         0x0300C023
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_256_SHA384         0x0300C024
# define TLS1_CK_ECDH_ECDSA_WITH_AES_128_SHA256          0x0300C025
# define TLS1_CK_ECDH_ECDSA_WITH_AES_256_SHA384          0x0300C026
# define TLS1_CK_ECDHE_RSA_WITH_AES_128_SHA256           0x0300C027
# define TLS1_CK_ECDHE_RSA_WITH_AES_256_SHA384           0x0300C028
# define TLS1_CK_ECDH_RSA_WITH_AES_128_SHA256            0x0300C029
# define TLS1_CK_ECDH_RSA_WITH_AES_256_SHA384            0x0300C02A

/* ECDH GCM based ciphersuites from RFC5289 */
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256     0x0300C02B
# define TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384     0x0300C02C
# define TLS1_CK_ECDH_ECDSA_WITH_AES_128_GCM_SHA256      0x0300C02D
# define TLS1_CK_ECDH_ECDSA_WITH_AES_256_GCM_SHA384      0x0300C02E
# define TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256       0x0300C02F
# define TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384       0x0300C030
# define TLS1_CK_ECDH_RSA_WITH_AES_128_GCM_SHA256        0x0300C031
# define TLS1_CK_ECDH_RSA_WITH_AES_256_GCM_SHA384        0x0300C032

/* ECDHE PSK ciphersuites from RFC5489 */
# define TLS1_CK_ECDHE_PSK_WITH_RC4_128_SHA              0x0300C033
# define TLS1_CK_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA         0x0300C034
# define TLS1_CK_ECDHE_PSK_WITH_AES_128_CBC_SHA          0x0300C035
# define TLS1_CK_ECDHE_PSK_WITH_AES_256_CBC_SHA          0x0300C036

# define TLS1_CK_ECDHE_PSK_WITH_AES_128_CBC_SHA256       0x0300C037
# define TLS1_CK_ECDHE_PSK_WITH_AES_256_CBC_SHA384       0x0300C038

/* NULL PSK ciphersuites from RFC4785 */
# define TLS1_CK_ECDHE_PSK_WITH_NULL_SHA                 0x0300C039
# define TLS1_CK_ECDHE_PSK_WITH_NULL_SHA256              0x0300C03A
# define TLS1_CK_ECDHE_PSK_WITH_NULL_SHA384              0x0300C03B

/* Camellia-CBC ciphersuites from RFC6367 */
# define TLS1_CK_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256 0x0300C072
# define TLS1_CK_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384 0x0300C073
# define TLS1_CK_ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256  0x0300C074
# define TLS1_CK_ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384  0x0300C075
# define TLS1_CK_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256   0x0300C076
# define TLS1_CK_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384   0x0300C077
# define TLS1_CK_ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256    0x0300C078
# define TLS1_CK_ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384    0x0300C079

# define TLS1_CK_PSK_WITH_CAMELLIA_128_CBC_SHA256         0x0300C094
# define TLS1_CK_PSK_WITH_CAMELLIA_256_CBC_SHA384         0x0300C095
# define TLS1_CK_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256     0x0300C096
# define TLS1_CK_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384     0x0300C097
# define TLS1_CK_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256     0x0300C098
# define TLS1_CK_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384     0x0300C099
# define TLS1_CK_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256   0x0300C09A
# define TLS1_CK_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384   0x0300C09B

/* draft-ietf-tls-chacha20-poly1305-03 */
# define TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305         0x0300CCA8
# define TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305       0x0300CCA9
# define TLS1_CK_DHE_RSA_WITH_CHACHA20_POLY1305           0x0300CCAA
# define TLS1_CK_PSK_WITH_CHACHA20_POLY1305               0x0300CCAB
# define TLS1_CK_ECDHE_PSK_WITH_CHACHA20_POLY1305         0x0300CCAC
# define TLS1_CK_DHE_PSK_WITH_CHACHA20_POLY1305           0x0300CCAD
# define TLS1_CK_RSA_PSK_WITH_CHACHA20_POLY1305           0x0300CCAE

/* TLS v1.3 ciphersuites */
# define TLS1_3_CK_AES_128_GCM_SHA256                     0x03001301
# define TLS1_3_CK_AES_256_GCM_SHA384                     0x03001302
# define TLS1_3_CK_CHACHA20_POLY1305_SHA256               0x03001303
# define TLS1_3_CK_AES_128_CCM_SHA256                     0x03001304
# define TLS1_3_CK_AES_128_CCM_8_SHA256                   0x03001305

/* Integrity-only ciphersuites from RFC 9150 */
# define TLS1_3_CK_SHA256_SHA256                          0x0300C0B4
# define TLS1_3_CK_SHA384_SHA384                          0x0300C0B5

/* Aria ciphersuites from RFC6209 */
# define TLS1_CK_RSA_WITH_ARIA_128_GCM_SHA256             0x0300C050
# define TLS1_CK_RSA_WITH_ARIA_256_GCM_SHA384             0x0300C051
# define TLS1_CK_DHE_RSA_WITH_ARIA_128_GCM_SHA256         0x0300C052
# define TLS1_CK_DHE_RSA_WITH_ARIA_256_GCM_SHA384         0x0300C053
# define TLS1_CK_DH_RSA_WITH_ARIA_128_GCM_SHA256          0x0300C054
# define TLS1_CK_DH_RSA_WITH_ARIA_256_GCM_SHA384          0x0300C055
# define TLS1_CK_DHE_DSS_WITH_ARIA_128_GCM_SHA256         0x0300C056
# define TLS1_CK_DHE_DSS_WITH_ARIA_256_GCM_SHA384         0x0300C057
# define TLS1_CK_DH_DSS_WITH_ARIA_128_GCM_SHA256          0x0300C058
# define TLS1_CK_DH_DSS_WITH_ARIA_256_GCM_SHA384          0x0300C059
# define TLS1_CK_DH_anon_WITH_ARIA_128_GCM_SHA256         0x0300C05A
# define TLS1_CK_DH_anon_WITH_ARIA_256_GCM_SHA384         0x0300C05B
# define TLS1_CK_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256     0x0300C05C
# define TLS1_CK_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384     0x0300C05D
# define TLS1_CK_ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256      0x0300C05E
# define TLS1_CK_ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384      0x0300C05F
# define TLS1_CK_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256       0x0300C060
# define TLS1_CK_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384       0x0300C061
# define TLS1_CK_ECDH_RSA_WITH_ARIA_128_GCM_SHA256        0x0300C062
# define TLS1_CK_ECDH_RSA_WITH_ARIA_256_GCM_SHA384        0x0300C063
# define TLS1_CK_PSK_WITH_ARIA_128_GCM_SHA256             0x0300C06A
# define TLS1_CK_PSK_WITH_ARIA_256_GCM_SHA384             0x0300C06B
# define TLS1_CK_DHE_PSK_WITH_ARIA_128_GCM_SHA256         0x0300C06C
# define TLS1_CK_DHE_PSK_WITH_ARIA_256_GCM_SHA384         0x0300C06D
# define TLS1_CK_RSA_PSK_WITH_ARIA_128_GCM_SHA256         0x0300C06E
# define TLS1_CK_RSA_PSK_WITH_ARIA_256_GCM_SHA384         0x0300C06F

/* a bundle of RFC standard cipher names, generated from ssl3_ciphers[] */
# define TLS1_RFC_RSA_WITH_AES_128_SHA                   "TLS_RSA_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_DHE_DSS_WITH_AES_128_SHA               "TLS_DHE_DSS_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_DHE_RSA_WITH_AES_128_SHA               "TLS_DHE_RSA_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_ADH_WITH_AES_128_SHA                   "TLS_DH_anon_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_RSA_WITH_AES_256_SHA                   "TLS_RSA_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_DHE_DSS_WITH_AES_256_SHA               "TLS_DHE_DSS_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_DHE_RSA_WITH_AES_256_SHA               "TLS_DHE_RSA_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_ADH_WITH_AES_256_SHA                   "TLS_DH_anon_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_RSA_WITH_NULL_SHA256                   "TLS_RSA_WITH_NULL_SHA256"
# define TLS1_RFC_RSA_WITH_AES_128_SHA256                "TLS_RSA_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_RSA_WITH_AES_256_SHA256                "TLS_RSA_WITH_AES_256_CBC_SHA256"
# define TLS1_RFC_DHE_DSS_WITH_AES_128_SHA256            "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_DHE_RSA_WITH_AES_128_SHA256            "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_DHE_DSS_WITH_AES_256_SHA256            "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256"
# define TLS1_RFC_DHE_RSA_WITH_AES_256_SHA256            "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256"
# define TLS1_RFC_ADH_WITH_AES_128_SHA256                "TLS_DH_anon_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_ADH_WITH_AES_256_SHA256                "TLS_DH_anon_WITH_AES_256_CBC_SHA256"
# define TLS1_RFC_RSA_WITH_AES_128_GCM_SHA256            "TLS_RSA_WITH_AES_128_GCM_SHA256"
# define TLS1_RFC_RSA_WITH_AES_256_GCM_SHA384            "TLS_RSA_WITH_AES_256_GCM_SHA384"
# define TLS1_RFC_DHE_RSA_WITH_AES_128_GCM_SHA256        "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256"
# define TLS1_RFC_DHE_RSA_WITH_AES_256_GCM_SHA384        "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384"
# define TLS1_RFC_DHE_DSS_WITH_AES_128_GCM_SHA256        "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256"
# define TLS1_RFC_DHE_DSS_WITH_AES_256_GCM_SHA384        "TLS_DHE_DSS_WITH_AES_256_GCM_SHA384"
# define TLS1_RFC_ADH_WITH_AES_128_GCM_SHA256            "TLS_DH_anon_WITH_AES_128_GCM_SHA256"
# define TLS1_RFC_ADH_WITH_AES_256_GCM_SHA384            "TLS_DH_anon_WITH_AES_256_GCM_SHA384"
# define TLS1_RFC_RSA_WITH_AES_128_CCM                   "TLS_RSA_WITH_AES_128_CCM"
# define TLS1_RFC_RSA_WITH_AES_256_CCM                   "TLS_RSA_WITH_AES_256_CCM"
# define TLS1_RFC_DHE_RSA_WITH_AES_128_CCM               "TLS_DHE_RSA_WITH_AES_128_CCM"
# define TLS1_RFC_DHE_RSA_WITH_AES_256_CCM               "TLS_DHE_RSA_WITH_AES_256_CCM"
# define TLS1_RFC_RSA_WITH_AES_128_CCM_8                 "TLS_RSA_WITH_AES_128_CCM_8"
# define TLS1_RFC_RSA_WITH_AES_256_CCM_8                 "TLS_RSA_WITH_AES_256_CCM_8"
# define TLS1_RFC_DHE_RSA_WITH_AES_128_CCM_8             "TLS_DHE_RSA_WITH_AES_128_CCM_8"
# define TLS1_RFC_DHE_RSA_WITH_AES_256_CCM_8             "TLS_DHE_RSA_WITH_AES_256_CCM_8"
# define TLS1_RFC_PSK_WITH_AES_128_CCM                   "TLS_PSK_WITH_AES_128_CCM"
# define TLS1_RFC_PSK_WITH_AES_256_CCM                   "TLS_PSK_WITH_AES_256_CCM"
# define TLS1_RFC_DHE_PSK_WITH_AES_128_CCM               "TLS_DHE_PSK_WITH_AES_128_CCM"
# define TLS1_RFC_DHE_PSK_WITH_AES_256_CCM               "TLS_DHE_PSK_WITH_AES_256_CCM"
# define TLS1_RFC_PSK_WITH_AES_128_CCM_8                 "TLS_PSK_WITH_AES_128_CCM_8"
# define TLS1_RFC_PSK_WITH_AES_256_CCM_8                 "TLS_PSK_WITH_AES_256_CCM_8"
# define TLS1_RFC_DHE_PSK_WITH_AES_128_CCM_8             "TLS_PSK_DHE_WITH_AES_128_CCM_8"
# define TLS1_RFC_DHE_PSK_WITH_AES_256_CCM_8             "TLS_PSK_DHE_WITH_AES_256_CCM_8"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_CCM           "TLS_ECDHE_ECDSA_WITH_AES_128_CCM"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_CCM           "TLS_ECDHE_ECDSA_WITH_AES_256_CCM"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_CCM_8         "TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_CCM_8         "TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8"
# define TLS1_3_RFC_AES_128_GCM_SHA256                   "TLS_AES_128_GCM_SHA256"
# define TLS1_3_RFC_AES_256_GCM_SHA384                   "TLS_AES_256_GCM_SHA384"
# define TLS1_3_RFC_CHACHA20_POLY1305_SHA256             "TLS_CHACHA20_POLY1305_SHA256"
# define TLS1_3_RFC_SHA256_SHA256                        "TLS_SHA256_SHA256"
# define TLS1_3_RFC_SHA384_SHA384                        "TLS_SHA384_SHA384"
# define TLS1_3_RFC_AES_128_CCM_SHA256                   "TLS_AES_128_CCM_SHA256"
# define TLS1_3_RFC_AES_128_CCM_8_SHA256                 "TLS_AES_128_CCM_8_SHA256"
# define TLS1_RFC_ECDHE_ECDSA_WITH_NULL_SHA              "TLS_ECDHE_ECDSA_WITH_NULL_SHA"
# define TLS1_RFC_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA      "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_CBC_SHA       "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_CBC_SHA       "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_ECDHE_RSA_WITH_NULL_SHA                "TLS_ECDHE_RSA_WITH_NULL_SHA"
# define TLS1_RFC_ECDHE_RSA_WITH_DES_192_CBC3_SHA        "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_ECDHE_RSA_WITH_AES_128_CBC_SHA         "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_ECDHE_RSA_WITH_AES_256_CBC_SHA         "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_ECDH_anon_WITH_NULL_SHA                "TLS_ECDH_anon_WITH_NULL_SHA"
# define TLS1_RFC_ECDH_anon_WITH_DES_192_CBC3_SHA        "TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_ECDH_anon_WITH_AES_128_CBC_SHA         "TLS_ECDH_anon_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_ECDH_anon_WITH_AES_256_CBC_SHA         "TLS_ECDH_anon_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_SHA256        "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_SHA384        "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384"
# define TLS1_RFC_ECDHE_RSA_WITH_AES_128_SHA256          "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_ECDHE_RSA_WITH_AES_256_SHA384          "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256    "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256"
# define TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384    "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384"
# define TLS1_RFC_ECDHE_RSA_WITH_AES_128_GCM_SHA256      "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256"
# define TLS1_RFC_ECDHE_RSA_WITH_AES_256_GCM_SHA384      "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384"
# define TLS1_RFC_PSK_WITH_NULL_SHA                      "TLS_PSK_WITH_NULL_SHA"
# define TLS1_RFC_DHE_PSK_WITH_NULL_SHA                  "TLS_DHE_PSK_WITH_NULL_SHA"
# define TLS1_RFC_RSA_PSK_WITH_NULL_SHA                  "TLS_RSA_PSK_WITH_NULL_SHA"
# define TLS1_RFC_PSK_WITH_3DES_EDE_CBC_SHA              "TLS_PSK_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_PSK_WITH_AES_128_CBC_SHA               "TLS_PSK_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_PSK_WITH_AES_256_CBC_SHA               "TLS_PSK_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_DHE_PSK_WITH_3DES_EDE_CBC_SHA          "TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_DHE_PSK_WITH_AES_128_CBC_SHA           "TLS_DHE_PSK_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_DHE_PSK_WITH_AES_256_CBC_SHA           "TLS_DHE_PSK_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_RSA_PSK_WITH_3DES_EDE_CBC_SHA          "TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_RSA_PSK_WITH_AES_128_CBC_SHA           "TLS_RSA_PSK_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_RSA_PSK_WITH_AES_256_CBC_SHA           "TLS_RSA_PSK_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_PSK_WITH_AES_128_GCM_SHA256            "TLS_PSK_WITH_AES_128_GCM_SHA256"
# define TLS1_RFC_PSK_WITH_AES_256_GCM_SHA384            "TLS_PSK_WITH_AES_256_GCM_SHA384"
# define TLS1_RFC_DHE_PSK_WITH_AES_128_GCM_SHA256        "TLS_DHE_PSK_WITH_AES_128_GCM_SHA256"
# define TLS1_RFC_DHE_PSK_WITH_AES_256_GCM_SHA384        "TLS_DHE_PSK_WITH_AES_256_GCM_SHA384"
# define TLS1_RFC_RSA_PSK_WITH_AES_128_GCM_SHA256        "TLS_RSA_PSK_WITH_AES_128_GCM_SHA256"
# define TLS1_RFC_RSA_PSK_WITH_AES_256_GCM_SHA384        "TLS_RSA_PSK_WITH_AES_256_GCM_SHA384"
# define TLS1_RFC_PSK_WITH_AES_128_CBC_SHA256            "TLS_PSK_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_PSK_WITH_AES_256_CBC_SHA384            "TLS_PSK_WITH_AES_256_CBC_SHA384"
# define TLS1_RFC_PSK_WITH_NULL_SHA256                   "TLS_PSK_WITH_NULL_SHA256"
# define TLS1_RFC_PSK_WITH_NULL_SHA384                   "TLS_PSK_WITH_NULL_SHA384"
# define TLS1_RFC_DHE_PSK_WITH_AES_128_CBC_SHA256        "TLS_DHE_PSK_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_DHE_PSK_WITH_AES_256_CBC_SHA384        "TLS_DHE_PSK_WITH_AES_256_CBC_SHA384"
# define TLS1_RFC_DHE_PSK_WITH_NULL_SHA256               "TLS_DHE_PSK_WITH_NULL_SHA256"
# define TLS1_RFC_DHE_PSK_WITH_NULL_SHA384               "TLS_DHE_PSK_WITH_NULL_SHA384"
# define TLS1_RFC_RSA_PSK_WITH_AES_128_CBC_SHA256        "TLS_RSA_PSK_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_RSA_PSK_WITH_AES_256_CBC_SHA384        "TLS_RSA_PSK_WITH_AES_256_CBC_SHA384"
# define TLS1_RFC_RSA_PSK_WITH_NULL_SHA256               "TLS_RSA_PSK_WITH_NULL_SHA256"
# define TLS1_RFC_RSA_PSK_WITH_NULL_SHA384               "TLS_RSA_PSK_WITH_NULL_SHA384"
# define TLS1_RFC_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA        "TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_ECDHE_PSK_WITH_AES_128_CBC_SHA         "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_ECDHE_PSK_WITH_AES_256_CBC_SHA         "TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_ECDHE_PSK_WITH_AES_128_CBC_SHA256      "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256"
# define TLS1_RFC_ECDHE_PSK_WITH_AES_256_CBC_SHA384      "TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384"
# define TLS1_RFC_ECDHE_PSK_WITH_NULL_SHA                "TLS_ECDHE_PSK_WITH_NULL_SHA"
# define TLS1_RFC_ECDHE_PSK_WITH_NULL_SHA256             "TLS_ECDHE_PSK_WITH_NULL_SHA256"
# define TLS1_RFC_ECDHE_PSK_WITH_NULL_SHA384             "TLS_ECDHE_PSK_WITH_NULL_SHA384"
# define TLS1_RFC_SRP_SHA_WITH_3DES_EDE_CBC_SHA          "TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA      "TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA      "TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA"
# define TLS1_RFC_SRP_SHA_WITH_AES_128_CBC_SHA           "TLS_SRP_SHA_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_SRP_SHA_RSA_WITH_AES_128_CBC_SHA       "TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_SRP_SHA_DSS_WITH_AES_128_CBC_SHA       "TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA"
# define TLS1_RFC_SRP_SHA_WITH_AES_256_CBC_SHA           "TLS_SRP_SHA_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_SRP_SHA_RSA_WITH_AES_256_CBC_SHA       "TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_SRP_SHA_DSS_WITH_AES_256_CBC_SHA       "TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA"
# define TLS1_RFC_DHE_RSA_WITH_CHACHA20_POLY1305         "TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256"
# define TLS1_RFC_ECDHE_RSA_WITH_CHACHA20_POLY1305       "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256"
# define TLS1_RFC_ECDHE_ECDSA_WITH_CHACHA20_POLY1305     "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256"
# define TLS1_RFC_PSK_WITH_CHACHA20_POLY1305             "TLS_PSK_WITH_CHACHA20_POLY1305_SHA256"
# define TLS1_RFC_ECDHE_PSK_WITH_CHACHA20_POLY1305       "TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256"
# define TLS1_RFC_DHE_PSK_WITH_CHACHA20_POLY1305         "TLS_DHE_PSK_WITH_CHACHA20_POLY1305_SHA256"
# define TLS1_RFC_RSA_PSK_WITH_CHACHA20_POLY1305         "TLS_RSA_PSK_WITH_CHACHA20_POLY1305_SHA256"
# define TLS1_RFC_RSA_WITH_CAMELLIA_128_CBC_SHA256       "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256   "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256   "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_ADH_WITH_CAMELLIA_128_CBC_SHA256       "TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_RSA_WITH_CAMELLIA_256_CBC_SHA256       "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256"
# define TLS1_RFC_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256   "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256"
# define TLS1_RFC_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256   "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256"
# define TLS1_RFC_ADH_WITH_CAMELLIA_256_CBC_SHA256       "TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA256"
# define TLS1_RFC_RSA_WITH_CAMELLIA_256_CBC_SHA          "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA"
# define TLS1_RFC_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA      "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA"
# define TLS1_RFC_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA      "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA"
# define TLS1_RFC_ADH_WITH_CAMELLIA_256_CBC_SHA          "TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA"
# define TLS1_RFC_RSA_WITH_CAMELLIA_128_CBC_SHA          "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA"
# define TLS1_RFC_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA      "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA"
# define TLS1_RFC_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA      "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA"
# define TLS1_RFC_ADH_WITH_CAMELLIA_128_CBC_SHA          "TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA"
# define TLS1_RFC_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256 "TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384 "TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384"
# define TLS1_RFC_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 "TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384 "TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384"
# define TLS1_RFC_PSK_WITH_CAMELLIA_128_CBC_SHA256       "TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_PSK_WITH_CAMELLIA_256_CBC_SHA384       "TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384"
# define TLS1_RFC_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256   "TLS_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384   "TLS_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384"
# define TLS1_RFC_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256   "TLS_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384   "TLS_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384"
# define TLS1_RFC_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256 "TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256"
# define TLS1_RFC_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384 "TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384"
# define TLS1_RFC_RSA_WITH_SEED_SHA                      "TLS_RSA_WITH_SEED_CBC_SHA"
# define TLS1_RFC_DHE_DSS_WITH_SEED_SHA                  "TLS_DHE_DSS_WITH_SEED_CBC_SHA"
# define TLS1_RFC_DHE_RSA_WITH_SEED_SHA                  "TLS_DHE_RSA_WITH_SEED_CBC_SHA"
# define TLS1_RFC_ADH_WITH_SEED_SHA                      "TLS_DH_anon_WITH_SEED_CBC_SHA"
# define TLS1_RFC_ECDHE_PSK_WITH_RC4_128_SHA             "TLS_ECDHE_PSK_WITH_RC4_128_SHA"
# define TLS1_RFC_ECDH_anon_WITH_RC4_128_SHA             "TLS_ECDH_anon_WITH_RC4_128_SHA"
# define TLS1_RFC_ECDHE_ECDSA_WITH_RC4_128_SHA           "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA"
# define TLS1_RFC_ECDHE_RSA_WITH_RC4_128_SHA             "TLS_ECDHE_RSA_WITH_RC4_128_SHA"
# define TLS1_RFC_PSK_WITH_RC4_128_SHA                   "TLS_PSK_WITH_RC4_128_SHA"
# define TLS1_RFC_RSA_PSK_WITH_RC4_128_SHA               "TLS_RSA_PSK_WITH_RC4_128_SHA"
# define TLS1_RFC_DHE_PSK_WITH_RC4_128_SHA               "TLS_DHE_PSK_WITH_RC4_128_SHA"
# define TLS1_RFC_RSA_WITH_ARIA_128_GCM_SHA256           "TLS_RSA_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_RSA_WITH_ARIA_256_GCM_SHA384           "TLS_RSA_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_DHE_RSA_WITH_ARIA_128_GCM_SHA256       "TLS_DHE_RSA_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_DHE_RSA_WITH_ARIA_256_GCM_SHA384       "TLS_DHE_RSA_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_DH_RSA_WITH_ARIA_128_GCM_SHA256        "TLS_DH_RSA_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_DH_RSA_WITH_ARIA_256_GCM_SHA384        "TLS_DH_RSA_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_DHE_DSS_WITH_ARIA_128_GCM_SHA256       "TLS_DHE_DSS_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_DHE_DSS_WITH_ARIA_256_GCM_SHA384       "TLS_DHE_DSS_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_DH_DSS_WITH_ARIA_128_GCM_SHA256        "TLS_DH_DSS_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_DH_DSS_WITH_ARIA_256_GCM_SHA384        "TLS_DH_DSS_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_DH_anon_WITH_ARIA_128_GCM_SHA256       "TLS_DH_anon_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_DH_anon_WITH_ARIA_256_GCM_SHA384       "TLS_DH_anon_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256   "TLS_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384   "TLS_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256    "TLS_ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384    "TLS_ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256     "TLS_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384     "TLS_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_ECDH_RSA_WITH_ARIA_128_GCM_SHA256      "TLS_ECDH_RSA_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_ECDH_RSA_WITH_ARIA_256_GCM_SHA384      "TLS_ECDH_RSA_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_PSK_WITH_ARIA_128_GCM_SHA256           "TLS_PSK_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_PSK_WITH_ARIA_256_GCM_SHA384           "TLS_PSK_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_DHE_PSK_WITH_ARIA_128_GCM_SHA256       "TLS_DHE_PSK_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_DHE_PSK_WITH_ARIA_256_GCM_SHA384       "TLS_DHE_PSK_WITH_ARIA_256_GCM_SHA384"
# define TLS1_RFC_RSA_PSK_WITH_ARIA_128_GCM_SHA256       "TLS_RSA_PSK_WITH_ARIA_128_GCM_SHA256"
# define TLS1_RFC_RSA_PSK_WITH_ARIA_256_GCM_SHA384       "TLS_RSA_PSK_WITH_ARIA_256_GCM_SHA384"


/*
 * XXX Backward compatibility alert: Older versions of OpenSSL gave some DHE
 * ciphers names with "EDH" instead of "DHE".  Going forward, we should be
 * using DHE everywhere, though we may indefinitely maintain aliases for
 * users or configurations that used "EDH"
 */
# define TLS1_TXT_DHE_DSS_WITH_RC4_128_SHA               "DHE-DSS-RC4-SHA"

# define TLS1_TXT_PSK_WITH_NULL_SHA                      "PSK-NULL-SHA"
# define TLS1_TXT_DHE_PSK_WITH_NULL_SHA                  "DHE-PSK-NULL-SHA"
# define TLS1_TXT_RSA_PSK_WITH_NULL_SHA                  "RSA-PSK-NULL-SHA"

/* AES ciphersuites from RFC3268 */
# define TLS1_TXT_RSA_WITH_AES_128_SHA                   "AES128-SHA"
# define TLS1_TXT_DH_DSS_WITH_AES_128_SHA                "DH-DSS-AES128-SHA"
# define TLS1_TXT_DH_RSA_WITH_AES_128_SHA                "DH-RSA-AES128-SHA"
# define TLS1_TXT_DHE_DSS_WITH_AES_128_SHA               "DHE-DSS-AES128-SHA"
# define TLS1_TXT_DHE_RSA_WITH_AES_128_SHA               "DHE-RSA-AES128-SHA"
# define TLS1_TXT_ADH_WITH_AES_128_SHA                   "ADH-AES128-SHA"

# define TLS1_TXT_RSA_WITH_AES_256_SHA                   "AES256-SHA"
# define TLS1_TXT_DH_DSS_WITH_AES_256_SHA                "DH-DSS-AES256-SHA"
# define TLS1_TXT_DH_RSA_WITH_AES_256_SHA                "DH-RSA-AES256-SHA"
# define TLS1_TXT_DHE_DSS_WITH_AES_256_SHA               "DHE-DSS-AES256-SHA"
# define TLS1_TXT_DHE_RSA_WITH_AES_256_SHA               "DHE-RSA-AES256-SHA"
# define TLS1_TXT_ADH_WITH_AES_256_SHA                   "ADH-AES256-SHA"

/* ECC ciphersuites from RFC4492 */
# define TLS1_TXT_ECDH_ECDSA_WITH_NULL_SHA               "ECDH-ECDSA-NULL-SHA"
# define TLS1_TXT_ECDH_ECDSA_WITH_RC4_128_SHA            "ECDH-ECDSA-RC4-SHA"
# define TLS1_TXT_ECDH_ECDSA_WITH_DES_192_CBC3_SHA       "ECDH-ECDSA-DES-CBC3-SHA"
# define TLS1_TXT_ECDH_ECDSA_WITH_AES_128_CBC_SHA        "ECDH-ECDSA-AES128-SHA"
# define TLS1_TXT_ECDH_ECDSA_WITH_AES_256_CBC_SHA        "ECDH-ECDSA-AES256-SHA"

# define TLS1_TXT_ECDHE_ECDSA_WITH_NULL_SHA              "ECDHE-ECDSA-NULL-SHA"
# define TLS1_TXT_ECDHE_ECDSA_WITH_RC4_128_SHA           "ECDHE-ECDSA-RC4-SHA"
# define TLS1_TXT_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA      "ECDHE-ECDSA-DES-CBC3-SHA"
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CBC_SHA       "ECDHE-ECDSA-AES128-SHA"
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CBC_SHA       "ECDHE-ECDSA-AES256-SHA"

# define TLS1_TXT_ECDH_RSA_WITH_NULL_SHA                 "ECDH-RSA-NULL-SHA"
# define TLS1_TXT_ECDH_RSA_WITH_RC4_128_SHA              "ECDH-RSA-RC4-SHA"
# define TLS1_TXT_ECDH_RSA_WITH_DES_192_CBC3_SHA         "ECDH-RSA-DES-CBC3-SHA"
# define TLS1_TXT_ECDH_RSA_WITH_AES_128_CBC_SHA          "ECDH-RSA-AES128-SHA"
# define TLS1_TXT_ECDH_RSA_WITH_AES_256_CBC_SHA          "ECDH-RSA-AES256-SHA"

# define TLS1_TXT_ECDHE_RSA_WITH_NULL_SHA                "ECDHE-RSA-NULL-SHA"
# define TLS1_TXT_ECDHE_RSA_WITH_RC4_128_SHA             "ECDHE-RSA-RC4-SHA"
# define TLS1_TXT_ECDHE_RSA_WITH_DES_192_CBC3_SHA        "ECDHE-RSA-DES-CBC3-SHA"
# define TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA         "ECDHE-RSA-AES128-SHA"
# define TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA         "ECDHE-RSA-AES256-SHA"

# define TLS1_TXT_ECDH_anon_WITH_NULL_SHA                "AECDH-NULL-SHA"
# define TLS1_TXT_ECDH_anon_WITH_RC4_128_SHA             "AECDH-RC4-SHA"
# define TLS1_TXT_ECDH_anon_WITH_DES_192_CBC3_SHA        "AECDH-DES-CBC3-SHA"
# define TLS1_TXT_ECDH_anon_WITH_AES_128_CBC_SHA         "AECDH-AES128-SHA"
# define TLS1_TXT_ECDH_anon_WITH_AES_256_CBC_SHA         "AECDH-AES256-SHA"

/* PSK ciphersuites from RFC 4279 */
# define TLS1_TXT_PSK_WITH_RC4_128_SHA                   "PSK-RC4-SHA"
# define TLS1_TXT_PSK_WITH_3DES_EDE_CBC_SHA              "PSK-3DES-EDE-CBC-SHA"
# define TLS1_TXT_PSK_WITH_AES_128_CBC_SHA               "PSK-AES128-CBC-SHA"
# define TLS1_TXT_PSK_WITH_AES_256_CBC_SHA               "PSK-AES256-CBC-SHA"

# define TLS1_TXT_DHE_PSK_WITH_RC4_128_SHA               "DHE-PSK-RC4-SHA"
# define TLS1_TXT_DHE_PSK_WITH_3DES_EDE_CBC_SHA          "DHE-PSK-3DES-EDE-CBC-SHA"
# define TLS1_TXT_DHE_PSK_WITH_AES_128_CBC_SHA           "DHE-PSK-AES128-CBC-SHA"
# define TLS1_TXT_DHE_PSK_WITH_AES_256_CBC_SHA           "DHE-PSK-AES256-CBC-SHA"
# define TLS1_TXT_RSA_PSK_WITH_RC4_128_SHA               "RSA-PSK-RC4-SHA"
# define TLS1_TXT_RSA_PSK_WITH_3DES_EDE_CBC_SHA          "RSA-PSK-3DES-EDE-CBC-SHA"
# define TLS1_TXT_RSA_PSK_WITH_AES_128_CBC_SHA           "RSA-PSK-AES128-CBC-SHA"
# define TLS1_TXT_RSA_PSK_WITH_AES_256_CBC_SHA           "RSA-PSK-AES256-CBC-SHA"

/* PSK ciphersuites from RFC 5487 */
# define TLS1_TXT_PSK_WITH_AES_128_GCM_SHA256            "PSK-AES128-GCM-SHA256"
# define TLS1_TXT_PSK_WITH_AES_256_GCM_SHA384            "PSK-AES256-GCM-SHA384"
# define TLS1_TXT_DHE_PSK_WITH_AES_128_GCM_SHA256        "DHE-PSK-AES128-GCM-SHA256"
# define TLS1_TXT_DHE_PSK_WITH_AES_256_GCM_SHA384        "DHE-PSK-AES256-GCM-SHA384"
# define TLS1_TXT_RSA_PSK_WITH_AES_128_GCM_SHA256        "RSA-PSK-AES128-GCM-SHA256"
# define TLS1_TXT_RSA_PSK_WITH_AES_256_GCM_SHA384        "RSA-PSK-AES256-GCM-SHA384"

# define TLS1_TXT_PSK_WITH_AES_128_CBC_SHA256            "PSK-AES128-CBC-SHA256"
# define TLS1_TXT_PSK_WITH_AES_256_CBC_SHA384            "PSK-AES256-CBC-SHA384"
# define TLS1_TXT_PSK_WITH_NULL_SHA256                   "PSK-NULL-SHA256"
# define TLS1_TXT_PSK_WITH_NULL_SHA384                   "PSK-NULL-SHA384"

# define TLS1_TXT_DHE_PSK_WITH_AES_128_CBC_SHA256        "DHE-PSK-AES128-CBC-SHA256"
# define TLS1_TXT_DHE_PSK_WITH_AES_256_CBC_SHA384        "DHE-PSK-AES256-CBC-SHA384"
# define TLS1_TXT_DHE_PSK_WITH_NULL_SHA256               "DHE-PSK-NULL-SHA256"
# define TLS1_TXT_DHE_PSK_WITH_NULL_SHA384               "DHE-PSK-NULL-SHA384"

# define TLS1_TXT_RSA_PSK_WITH_AES_128_CBC_SHA256        "RSA-PSK-AES128-CBC-SHA256"
# define TLS1_TXT_RSA_PSK_WITH_AES_256_CBC_SHA384        "RSA-PSK-AES256-CBC-SHA384"
# define TLS1_TXT_RSA_PSK_WITH_NULL_SHA256               "RSA-PSK-NULL-SHA256"
# define TLS1_TXT_RSA_PSK_WITH_NULL_SHA384               "RSA-PSK-NULL-SHA384"

/* SRP ciphersuite from RFC 5054 */
# define TLS1_TXT_SRP_SHA_WITH_3DES_EDE_CBC_SHA          "SRP-3DES-EDE-CBC-SHA"
# define TLS1_TXT_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA      "SRP-RSA-3DES-EDE-CBC-SHA"
# define TLS1_TXT_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA      "SRP-DSS-3DES-EDE-CBC-SHA"
# define TLS1_TXT_SRP_SHA_WITH_AES_128_CBC_SHA           "SRP-AES-128-CBC-SHA"
# define TLS1_TXT_SRP_SHA_RSA_WITH_AES_128_CBC_SHA       "SRP-RSA-AES-128-CBC-SHA"
# define TLS1_TXT_SRP_SHA_DSS_WITH_AES_128_CBC_SHA       "SRP-DSS-AES-128-CBC-SHA"
# define TLS1_TXT_SRP_SHA_WITH_AES_256_CBC_SHA           "SRP-AES-256-CBC-SHA"
# define TLS1_TXT_SRP_SHA_RSA_WITH_AES_256_CBC_SHA       "SRP-RSA-AES-256-CBC-SHA"
# define TLS1_TXT_SRP_SHA_DSS_WITH_AES_256_CBC_SHA       "SRP-DSS-AES-256-CBC-SHA"

/* Camellia ciphersuites from RFC4132 */
# define TLS1_TXT_RSA_WITH_CAMELLIA_128_CBC_SHA          "CAMELLIA128-SHA"
# define TLS1_TXT_DH_DSS_WITH_CAMELLIA_128_CBC_SHA       "DH-DSS-CAMELLIA128-SHA"
# define TLS1_TXT_DH_RSA_WITH_CAMELLIA_128_CBC_SHA       "DH-RSA-CAMELLIA128-SHA"
# define TLS1_TXT_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA      "DHE-DSS-CAMELLIA128-SHA"
# define TLS1_TXT_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA      "DHE-RSA-CAMELLIA128-SHA"
# define TLS1_TXT_ADH_WITH_CAMELLIA_128_CBC_SHA          "ADH-CAMELLIA128-SHA"

# define TLS1_TXT_RSA_WITH_CAMELLIA_256_CBC_SHA          "CAMELLIA256-SHA"
# define TLS1_TXT_DH_DSS_WITH_CAMELLIA_256_CBC_SHA       "DH-DSS-CAMELLIA256-SHA"
# define TLS1_TXT_DH_RSA_WITH_CAMELLIA_256_CBC_SHA       "DH-RSA-CAMELLIA256-SHA"
# define TLS1_TXT_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA      "DHE-DSS-CAMELLIA256-SHA"
# define TLS1_TXT_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA      "DHE-RSA-CAMELLIA256-SHA"
# define TLS1_TXT_ADH_WITH_CAMELLIA_256_CBC_SHA          "ADH-CAMELLIA256-SHA"

/* TLS 1.2 Camellia SHA-256 ciphersuites from RFC5932 */
# define TLS1_TXT_RSA_WITH_CAMELLIA_128_CBC_SHA256               "CAMELLIA128-SHA256"
# define TLS1_TXT_DH_DSS_WITH_CAMELLIA_128_CBC_SHA256            "DH-DSS-CAMELLIA128-SHA256"
# define TLS1_TXT_DH_RSA_WITH_CAMELLIA_128_CBC_SHA256            "DH-RSA-CAMELLIA128-SHA256"
# define TLS1_TXT_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256           "DHE-DSS-CAMELLIA128-SHA256"
# define TLS1_TXT_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256           "DHE-RSA-CAMELLIA128-SHA256"
# define TLS1_TXT_ADH_WITH_CAMELLIA_128_CBC_SHA256               "ADH-CAMELLIA128-SHA256"

# define TLS1_TXT_RSA_WITH_CAMELLIA_256_CBC_SHA256               "CAMELLIA256-SHA256"
# define TLS1_TXT_DH_DSS_WITH_CAMELLIA_256_CBC_SHA256            "DH-DSS-CAMELLIA256-SHA256"
# define TLS1_TXT_DH_RSA_WITH_CAMELLIA_256_CBC_SHA256            "DH-RSA-CAMELLIA256-SHA256"
# define TLS1_TXT_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256           "DHE-DSS-CAMELLIA256-SHA256"
# define TLS1_TXT_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256           "DHE-RSA-CAMELLIA256-SHA256"
# define TLS1_TXT_ADH_WITH_CAMELLIA_256_CBC_SHA256               "ADH-CAMELLIA256-SHA256"

# define TLS1_TXT_PSK_WITH_CAMELLIA_128_CBC_SHA256               "PSK-CAMELLIA128-SHA256"
# define TLS1_TXT_PSK_WITH_CAMELLIA_256_CBC_SHA384               "PSK-CAMELLIA256-SHA384"
# define TLS1_TXT_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256           "DHE-PSK-CAMELLIA128-SHA256"
# define TLS1_TXT_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384           "DHE-PSK-CAMELLIA256-SHA384"
# define TLS1_TXT_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256           "RSA-PSK-CAMELLIA128-SHA256"
# define TLS1_TXT_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384           "RSA-PSK-CAMELLIA256-SHA384"
# define TLS1_TXT_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256         "ECDHE-PSK-CAMELLIA128-SHA256"
# define TLS1_TXT_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384         "ECDHE-PSK-CAMELLIA256-SHA384"

/* SEED ciphersuites from RFC4162 */
# define TLS1_TXT_RSA_WITH_SEED_SHA                      "SEED-SHA"
# define TLS1_TXT_DH_DSS_WITH_SEED_SHA                   "DH-DSS-SEED-SHA"
# define TLS1_TXT_DH_RSA_WITH_SEED_SHA                   "DH-RSA-SEED-SHA"
# define TLS1_TXT_DHE_DSS_WITH_SEED_SHA                  "DHE-DSS-SEED-SHA"
# define TLS1_TXT_DHE_RSA_WITH_SEED_SHA                  "DHE-RSA-SEED-SHA"
# define TLS1_TXT_ADH_WITH_SEED_SHA                      "ADH-SEED-SHA"

/* TLS v1.2 ciphersuites */
# define TLS1_TXT_RSA_WITH_NULL_SHA256                   "NULL-SHA256"
# define TLS1_TXT_RSA_WITH_AES_128_SHA256                "AES128-SHA256"
# define TLS1_TXT_RSA_WITH_AES_256_SHA256                "AES256-SHA256"
# define TLS1_TXT_DH_DSS_WITH_AES_128_SHA256             "DH-DSS-AES128-SHA256"
# define TLS1_TXT_DH_RSA_WITH_AES_128_SHA256             "DH-RSA-AES128-SHA256"
# define TLS1_TXT_DHE_DSS_WITH_AES_128_SHA256            "DHE-DSS-AES128-SHA256"
# define TLS1_TXT_DHE_RSA_WITH_AES_128_SHA256            "DHE-RSA-AES128-SHA256"
# define TLS1_TXT_DH_DSS_WITH_AES_256_SHA256             "DH-DSS-AES256-SHA256"
# define TLS1_TXT_DH_RSA_WITH_AES_256_SHA256             "DH-RSA-AES256-SHA256"
# define TLS1_TXT_DHE_DSS_WITH_AES_256_SHA256            "DHE-DSS-AES256-SHA256"
# define TLS1_TXT_DHE_RSA_WITH_AES_256_SHA256            "DHE-RSA-AES256-SHA256"
# define TLS1_TXT_ADH_WITH_AES_128_SHA256                "ADH-AES128-SHA256"
# define TLS1_TXT_ADH_WITH_AES_256_SHA256                "ADH-AES256-SHA256"

/* TLS v1.2 GCM ciphersuites from RFC5288 */
# define TLS1_TXT_RSA_WITH_AES_128_GCM_SHA256            "AES128-GCM-SHA256"
# define TLS1_TXT_RSA_WITH_AES_256_GCM_SHA384            "AES256-GCM-SHA384"
# define TLS1_TXT_DHE_RSA_WITH_AES_128_GCM_SHA256        "DHE-RSA-AES128-GCM-SHA256"
# define TLS1_TXT_DHE_RSA_WITH_AES_256_GCM_SHA384        "DHE-RSA-AES256-GCM-SHA384"
# define TLS1_TXT_DH_RSA_WITH_AES_128_GCM_SHA256         "DH-RSA-AES128-GCM-SHA256"
# define TLS1_TXT_DH_RSA_WITH_AES_256_GCM_SHA384         "DH-RSA-AES256-GCM-SHA384"
# define TLS1_TXT_DHE_DSS_WITH_AES_128_GCM_SHA256        "DHE-DSS-AES128-GCM-SHA256"
# define TLS1_TXT_DHE_DSS_WITH_AES_256_GCM_SHA384        "DHE-DSS-AES256-GCM-SHA384"
# define TLS1_TXT_DH_DSS_WITH_AES_128_GCM_SHA256         "DH-DSS-AES128-GCM-SHA256"
# define TLS1_TXT_DH_DSS_WITH_AES_256_GCM_SHA384         "DH-DSS-AES256-GCM-SHA384"
# define TLS1_TXT_ADH_WITH_AES_128_GCM_SHA256            "ADH-AES128-GCM-SHA256"
# define TLS1_TXT_ADH_WITH_AES_256_GCM_SHA384            "ADH-AES256-GCM-SHA384"

/* CCM ciphersuites from RFC6655 */
# define TLS1_TXT_RSA_WITH_AES_128_CCM                   "AES128-CCM"
# define TLS1_TXT_RSA_WITH_AES_256_CCM                   "AES256-CCM"
# define TLS1_TXT_DHE_RSA_WITH_AES_128_CCM               "DHE-RSA-AES128-CCM"
# define TLS1_TXT_DHE_RSA_WITH_AES_256_CCM               "DHE-RSA-AES256-CCM"

# define TLS1_TXT_RSA_WITH_AES_128_CCM_8                 "AES128-CCM8"
# define TLS1_TXT_RSA_WITH_AES_256_CCM_8                 "AES256-CCM8"
# define TLS1_TXT_DHE_RSA_WITH_AES_128_CCM_8             "DHE-RSA-AES128-CCM8"
# define TLS1_TXT_DHE_RSA_WITH_AES_256_CCM_8             "DHE-RSA-AES256-CCM8"

# define TLS1_TXT_PSK_WITH_AES_128_CCM                   "PSK-AES128-CCM"
# define TLS1_TXT_PSK_WITH_AES_256_CCM                   "PSK-AES256-CCM"
# define TLS1_TXT_DHE_PSK_WITH_AES_128_CCM               "DHE-PSK-AES128-CCM"
# define TLS1_TXT_DHE_PSK_WITH_AES_256_CCM               "DHE-PSK-AES256-CCM"

# define TLS1_TXT_PSK_WITH_AES_128_CCM_8                 "PSK-AES128-CCM8"
# define TLS1_TXT_PSK_WITH_AES_256_CCM_8                 "PSK-AES256-CCM8"
# define TLS1_TXT_DHE_PSK_WITH_AES_128_CCM_8             "DHE-PSK-AES128-CCM8"
# define TLS1_TXT_DHE_PSK_WITH_AES_256_CCM_8             "DHE-PSK-AES256-CCM8"

/* CCM ciphersuites from RFC7251 */
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CCM       "ECDHE-ECDSA-AES128-CCM"
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CCM       "ECDHE-ECDSA-AES256-CCM"
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CCM_8     "ECDHE-ECDSA-AES128-CCM8"
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CCM_8     "ECDHE-ECDSA-AES256-CCM8"

/* ECDH HMAC based ciphersuites from RFC5289 */
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_SHA256    "ECDHE-ECDSA-AES128-SHA256"
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_SHA384    "ECDHE-ECDSA-AES256-SHA384"
# define TLS1_TXT_ECDH_ECDSA_WITH_AES_128_SHA256     "ECDH-ECDSA-AES128-SHA256"
# define TLS1_TXT_ECDH_ECDSA_WITH_AES_256_SHA384     "ECDH-ECDSA-AES256-SHA384"
# define TLS1_TXT_ECDHE_RSA_WITH_AES_128_SHA256      "ECDHE-RSA-AES128-SHA256"
# define TLS1_TXT_ECDHE_RSA_WITH_AES_256_SHA384      "ECDHE-RSA-AES256-SHA384"
# define TLS1_TXT_ECDH_RSA_WITH_AES_128_SHA256       "ECDH-RSA-AES128-SHA256"
# define TLS1_TXT_ECDH_RSA_WITH_AES_256_SHA384       "ECDH-RSA-AES256-SHA384"

/* ECDH GCM based ciphersuites from RFC5289 */
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256    "ECDHE-ECDSA-AES128-GCM-SHA256"
# define TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384    "ECDHE-ECDSA-AES256-GCM-SHA384"
# define TLS1_TXT_ECDH_ECDSA_WITH_AES_128_GCM_SHA256     "ECDH-ECDSA-AES128-GCM-SHA256"
# define TLS1_TXT_ECDH_ECDSA_WITH_AES_256_GCM_SHA384     "ECDH-ECDSA-AES256-GCM-SHA384"
# define TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256      "ECDHE-RSA-AES128-GCM-SHA256"
# define TLS1_TXT_ECDHE_RSA_WITH_AES_256_GCM_SHA384      "ECDHE-RSA-AES256-GCM-SHA384"
# define TLS1_TXT_ECDH_RSA_WITH_AES_128_GCM_SHA256       "ECDH-RSA-AES128-GCM-SHA256"
# define TLS1_TXT_ECDH_RSA_WITH_AES_256_GCM_SHA384       "ECDH-RSA-AES256-GCM-SHA384"

/* TLS v1.2 PSK GCM ciphersuites from RFC5487 */
# define TLS1_TXT_PSK_WITH_AES_128_GCM_SHA256            "PSK-AES128-GCM-SHA256"
# define TLS1_TXT_PSK_WITH_AES_256_GCM_SHA384            "PSK-AES256-GCM-SHA384"

/* ECDHE PSK ciphersuites from RFC 5489 */
# define TLS1_TXT_ECDHE_PSK_WITH_RC4_128_SHA               "ECDHE-PSK-RC4-SHA"
# define TLS1_TXT_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA          "ECDHE-PSK-3DES-EDE-CBC-SHA"
# define TLS1_TXT_ECDHE_PSK_WITH_AES_128_CBC_SHA           "ECDHE-PSK-AES128-CBC-SHA"
# define TLS1_TXT_ECDHE_PSK_WITH_AES_256_CBC_SHA           "ECDHE-PSK-AES256-CBC-SHA"

# define TLS1_TXT_ECDHE_PSK_WITH_AES_128_CBC_SHA256        "ECDHE-PSK-AES128-CBC-SHA256"
# define TLS1_TXT_ECDHE_PSK_WITH_AES_256_CBC_SHA384        "ECDHE-PSK-AES256-CBC-SHA384"

# define TLS1_TXT_ECDHE_PSK_WITH_NULL_SHA                  "ECDHE-PSK-NULL-SHA"
# define TLS1_TXT_ECDHE_PSK_WITH_NULL_SHA256               "ECDHE-PSK-NULL-SHA256"
# define TLS1_TXT_ECDHE_PSK_WITH_NULL_SHA384               "ECDHE-PSK-NULL-SHA384"

/* Camellia-CBC ciphersuites from RFC6367 */
# define TLS1_TXT_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256 "ECDHE-ECDSA-CAMELLIA128-SHA256"
# define TLS1_TXT_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384 "ECDHE-ECDSA-CAMELLIA256-SHA384"
# define TLS1_TXT_ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256  "ECDH-ECDSA-CAMELLIA128-SHA256"
# define TLS1_TXT_ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384  "ECDH-ECDSA-CAMELLIA256-SHA384"
# define TLS1_TXT_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256   "ECDHE-RSA-CAMELLIA128-SHA256"
# define TLS1_TXT_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384   "ECDHE-RSA-CAMELLIA256-SHA384"
# define TLS1_TXT_ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256    "ECDH-RSA-CAMELLIA128-SHA256"
# define TLS1_TXT_ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384    "ECDH-RSA-CAMELLIA256-SHA384"

/* draft-ietf-tls-chacha20-poly1305-03 */
# define TLS1_TXT_ECDHE_RSA_WITH_CHACHA20_POLY1305         "ECDHE-RSA-CHACHA20-POLY1305"
# define TLS1_TXT_ECDHE_ECDSA_WITH_CHACHA20_POLY1305       "ECDHE-ECDSA-CHACHA20-POLY1305"
# define TLS1_TXT_DHE_RSA_WITH_CHACHA20_POLY1305           "DHE-RSA-CHACHA20-POLY1305"
# define TLS1_TXT_PSK_WITH_CHACHA20_POLY1305               "PSK-CHACHA20-POLY1305"
# define TLS1_TXT_ECDHE_PSK_WITH_CHACHA20_POLY1305         "ECDHE-PSK-CHACHA20-POLY1305"
# define TLS1_TXT_DHE_PSK_WITH_CHACHA20_POLY1305           "DHE-PSK-CHACHA20-POLY1305"
# define TLS1_TXT_RSA_PSK_WITH_CHACHA20_POLY1305           "RSA-PSK-CHACHA20-POLY1305"

/* Aria ciphersuites from RFC6209 */
# define TLS1_TXT_RSA_WITH_ARIA_128_GCM_SHA256             "ARIA128-GCM-SHA256"
# define TLS1_TXT_RSA_WITH_ARIA_256_GCM_SHA384             "ARIA256-GCM-SHA384"
# define TLS1_TXT_DHE_RSA_WITH_ARIA_128_GCM_SHA256         "DHE-RSA-ARIA128-GCM-SHA256"
# define TLS1_TXT_DHE_RSA_WITH_ARIA_256_GCM_SHA384         "DHE-RSA-ARIA256-GCM-SHA384"
# define TLS1_TXT_DH_RSA_WITH_ARIA_128_GCM_SHA256          "DH-RSA-ARIA128-GCM-SHA256"
# define TLS1_TXT_DH_RSA_WITH_ARIA_256_GCM_SHA384          "DH-RSA-ARIA256-GCM-SHA384"
# define TLS1_TXT_DHE_DSS_WITH_ARIA_128_GCM_SHA256         "DHE-DSS-ARIA128-GCM-SHA256"
# define TLS1_TXT_DHE_DSS_WITH_ARIA_256_GCM_SHA384         "DHE-DSS-ARIA256-GCM-SHA384"
# define TLS1_TXT_DH_DSS_WITH_ARIA_128_GCM_SHA256          "DH-DSS-ARIA128-GCM-SHA256"
# define TLS1_TXT_DH_DSS_WITH_ARIA_256_GCM_SHA384          "DH-DSS-ARIA256-GCM-SHA384"
# define TLS1_TXT_DH_anon_WITH_ARIA_128_GCM_SHA256         "ADH-ARIA128-GCM-SHA256"
# define TLS1_TXT_DH_anon_WITH_ARIA_256_GCM_SHA384         "ADH-ARIA256-GCM-SHA384"
# define TLS1_TXT_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256     "ECDHE-ECDSA-ARIA128-GCM-SHA256"
# define TLS1_TXT_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384     "ECDHE-ECDSA-ARIA256-GCM-SHA384"
# define TLS1_TXT_ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256      "ECDH-ECDSA-ARIA128-GCM-SHA256"
# define TLS1_TXT_ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384      "ECDH-ECDSA-ARIA256-GCM-SHA384"
# define TLS1_TXT_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256       "ECDHE-ARIA128-GCM-SHA256"
# define TLS1_TXT_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384       "ECDHE-ARIA256-GCM-SHA384"
# define TLS1_TXT_ECDH_RSA_WITH_ARIA_128_GCM_SHA256        "ECDH-ARIA128-GCM-SHA256"
# define TLS1_TXT_ECDH_RSA_WITH_ARIA_256_GCM_SHA384        "ECDH-ARIA256-GCM-SHA384"
# define TLS1_TXT_PSK_WITH_ARIA_128_GCM_SHA256             "PSK-ARIA128-GCM-SHA256"
# define TLS1_TXT_PSK_WITH_ARIA_256_GCM_SHA384             "PSK-ARIA256-GCM-SHA384"
# define TLS1_TXT_DHE_PSK_WITH_ARIA_128_GCM_SHA256         "DHE-PSK-ARIA128-GCM-SHA256"
# define TLS1_TXT_DHE_PSK_WITH_ARIA_256_GCM_SHA384         "DHE-PSK-ARIA256-GCM-SHA384"
# define TLS1_TXT_RSA_PSK_WITH_ARIA_128_GCM_SHA256         "RSA-PSK-ARIA128-GCM-SHA256"
# define TLS1_TXT_RSA_PSK_WITH_ARIA_256_GCM_SHA384         "RSA-PSK-ARIA256-GCM-SHA384"

# define TLS_CT_RSA_SIGN                 1
# define TLS_CT_DSS_SIGN                 2
# define TLS_CT_RSA_FIXED_DH             3
# define TLS_CT_DSS_FIXED_DH             4
# define TLS_CT_ECDSA_SIGN               64
# define TLS_CT_RSA_FIXED_ECDH           65
# define TLS_CT_ECDSA_FIXED_ECDH         66
# define TLS_CT_GOST01_SIGN              22
# define TLS_CT_GOST12_IANA_SIGN         67
# define TLS_CT_GOST12_IANA_512_SIGN     68
# define TLS_CT_GOST12_LEGACY_SIGN       238
# define TLS_CT_GOST12_LEGACY_512_SIGN   239

# ifndef OPENSSL_NO_DEPRECATED_3_0
#  define TLS_CT_GOST12_SIGN TLS_CT_GOST12_LEGACY_SIGN
#  define TLS_CT_GOST12_512_SIGN TLS_CT_GOST12_LEGACY_512_SIGN
# endif

/*
 * when correcting this number, correct also SSL3_CT_NUMBER in ssl3.h (see
 * comment there)
 */
# define TLS_CT_NUMBER                   12

# if defined(SSL3_CT_NUMBER)
#  if TLS_CT_NUMBER != SSL3_CT_NUMBER
#    error "SSL/TLS CT_NUMBER values do not match"
#  endif
# endif

# define TLS1_FINISH_MAC_LENGTH          12

# define TLS_MD_MAX_CONST_SIZE                     22

/* ASCII: "client finished", in hex for EBCDIC compatibility */
# define TLS_MD_CLIENT_FINISH_CONST                "\x63\x6c\x69\x65\x6e\x74\x20\x66\x69\x6e\x69\x73\x68\x65\x64"
# define TLS_MD_CLIENT_FINISH_CONST_SIZE           15
/* ASCII: "server finished", in hex for EBCDIC compatibility */
# define TLS_MD_SERVER_FINISH_CONST                "\x73\x65\x72\x76\x65\x72\x20\x66\x69\x6e\x69\x73\x68\x65\x64"
# define TLS_MD_SERVER_FINISH_CONST_SIZE           15
/* ASCII: "server write key", in hex for EBCDIC compatibility */
# define TLS_MD_SERVER_WRITE_KEY_CONST             "\x73\x65\x72\x76\x65\x72\x20\x77\x72\x69\x74\x65\x20\x6b\x65\x79"
# define TLS_MD_SERVER_WRITE_KEY_CONST_SIZE        16
/* ASCII: "key expansion", in hex for EBCDIC compatibility */
# define TLS_MD_KEY_EXPANSION_CONST                "\x6b\x65\x79\x20\x65\x78\x70\x61\x6e\x73\x69\x6f\x6e"
# define TLS_MD_KEY_EXPANSION_CONST_SIZE           13
/* ASCII: "client write key", in hex for EBCDIC compatibility */
# define TLS_MD_CLIENT_WRITE_KEY_CONST             "\x63\x6c\x69\x65\x6e\x74\x20\x77\x72\x69\x74\x65\x20\x6b\x65\x79"
# define TLS_MD_CLIENT_WRITE_KEY_CONST_SIZE        16
/* ASCII: "server write key", in hex for EBCDIC compatibility */
# define TLS_MD_SERVER_WRITE_KEY_CONST             "\x73\x65\x72\x76\x65\x72\x20\x77\x72\x69\x74\x65\x20\x6b\x65\x79"
# define TLS_MD_SERVER_WRITE_KEY_CONST_SIZE        16
/* ASCII: "IV block", in hex for EBCDIC compatibility */
# define TLS_MD_IV_BLOCK_CONST                     "\x49\x56\x20\x62\x6c\x6f\x63\x6b"
# define TLS_MD_IV_BLOCK_CONST_SIZE                8
/* ASCII: "master secret", in hex for EBCDIC compatibility */
# define TLS_MD_MASTER_SECRET_CONST                "\x6d\x61\x73\x74\x65\x72\x20\x73\x65\x63\x72\x65\x74"
# define TLS_MD_MASTER_SECRET_CONST_SIZE           13
/* ASCII: "extended master secret", in hex for EBCDIC compatibility */
# define TLS_MD_EXTENDED_MASTER_SECRET_CONST       "\x65\x78\x74\x65\x6e\x64\x65\x64\x20\x6d\x61\x73\x74\x65\x72\x20\x73\x65\x63\x72\x65\x74"
# define TLS_MD_EXTENDED_MASTER_SECRET_CONST_SIZE  22

/* TLS Session Ticket extension struct */
struct tls_session_ticket_ext_st {
    unsigned short length;
    void *data;
};

#ifdef  __cplusplus
}
#endif
#endif
