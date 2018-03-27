/* ssl/t1_trce.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2012 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 */

#include "ssl_locl.h"

#ifndef OPENSSL_NO_SSL_TRACE

/* Packet trace support for OpenSSL */

typedef struct {
    int num;
    const char *name;
} ssl_trace_tbl;

# define ssl_trace_str(val, tbl) \
        do_ssl_trace_str(val, tbl, sizeof(tbl)/sizeof(ssl_trace_tbl))

# define ssl_trace_list(bio, indent, msg, msglen, value, table) \
        do_ssl_trace_list(bio, indent, msg, msglen, value, \
         table, sizeof(table)/sizeof(ssl_trace_tbl))

static const char *do_ssl_trace_str(int val, ssl_trace_tbl *tbl, size_t ntbl)
{
    size_t i;
    for (i = 0; i < ntbl; i++, tbl++) {
        if (tbl->num == val)
            return tbl->name;
    }
    return "UNKNOWN";
}

static int do_ssl_trace_list(BIO *bio, int indent,
                             const unsigned char *msg, size_t msglen,
                             size_t vlen, ssl_trace_tbl *tbl, size_t ntbl)
{
    int val;
    if (msglen % vlen)
        return 0;
    while (msglen) {
        val = msg[0];
        if (vlen == 2)
            val = (val << 8) | msg[1];
        BIO_indent(bio, indent, 80);
        BIO_printf(bio, "%s (%d)\n", do_ssl_trace_str(val, tbl, ntbl), val);
        msg += vlen;
        msglen -= vlen;
    }
    return 1;
}

/* Version number */

static ssl_trace_tbl ssl_version_tbl[] = {
    {SSL2_VERSION, "SSL 2.0"},
    {SSL3_VERSION, "SSL 3.0"},
    {TLS1_VERSION, "TLS 1.0"},
    {TLS1_1_VERSION, "TLS 1.1"},
    {TLS1_2_VERSION, "TLS 1.2"},
    {DTLS1_VERSION, "DTLS 1.0"},
    {DTLS1_2_VERSION, "DTLS 1.2"},
    {DTLS1_BAD_VER, "DTLS 1.0 (bad)"}
};

static ssl_trace_tbl ssl_content_tbl[] = {
    {SSL3_RT_CHANGE_CIPHER_SPEC, "ChangeCipherSpec"},
    {SSL3_RT_ALERT, "Alert"},
    {SSL3_RT_HANDSHAKE, "Handshake"},
    {SSL3_RT_APPLICATION_DATA, "ApplicationData"},
    {TLS1_RT_HEARTBEAT, "HeartBeat"}
};

/* Handshake types */
static ssl_trace_tbl ssl_handshake_tbl[] = {
    {SSL3_MT_HELLO_REQUEST, "HelloRequest"},
    {SSL3_MT_CLIENT_HELLO, "ClientHello"},
    {SSL3_MT_SERVER_HELLO, "ServerHello"},
    {DTLS1_MT_HELLO_VERIFY_REQUEST, "HelloVerifyRequest"},
    {SSL3_MT_NEWSESSION_TICKET, "NewSessionTicket"},
    {SSL3_MT_CERTIFICATE, "Certificate"},
    {SSL3_MT_SERVER_KEY_EXCHANGE, "ServerKeyExchange"},
    {SSL3_MT_CERTIFICATE_REQUEST, "CertificateRequest"},
    {SSL3_MT_CLIENT_KEY_EXCHANGE, "ClientKeyExchange"},
    {SSL3_MT_CERTIFICATE_STATUS, "CertificateStatus"},
    {SSL3_MT_SERVER_DONE, "ServerHelloDone"},
    {SSL3_MT_CERTIFICATE_VERIFY, "CertificateVerify"},
    {SSL3_MT_CLIENT_KEY_EXCHANGE, "ClientKeyExchange"},
    {SSL3_MT_FINISHED, "Finished"},
    {SSL3_MT_CERTIFICATE_STATUS, "CertificateStatus"}
};

/* Cipher suites */
static ssl_trace_tbl ssl_ciphers_tbl[] = {
    {0x0000, "SSL_NULL_WITH_NULL_NULL"},
    {0x0001, "SSL_RSA_WITH_NULL_MD5"},
    {0x0002, "SSL_RSA_WITH_NULL_SHA"},
    {0x0003, "SSL_RSA_EXPORT_WITH_RC4_40_MD5"},
    {0x0004, "SSL_RSA_WITH_RC4_128_MD5"},
    {0x0005, "SSL_RSA_WITH_RC4_128_SHA"},
    {0x0006, "SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5"},
    {0x0007, "SSL_RSA_WITH_IDEA_CBC_SHA"},
    {0x0008, "SSL_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {0x0009, "SSL_RSA_WITH_DES_CBC_SHA"},
    {0x000A, "SSL_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0x000B, "SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA"},
    {0x000C, "SSL_DH_DSS_WITH_DES_CBC_SHA"},
    {0x000D, "SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA"},
    {0x000E, "SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {0x000F, "SSL_DH_RSA_WITH_DES_CBC_SHA"},
    {0x0010, "SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0x0011, "SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA"},
    {0x0012, "SSL_DHE_DSS_WITH_DES_CBC_SHA"},
    {0x0013, "SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA"},
    {0x0014, "SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {0x0015, "SSL_DHE_RSA_WITH_DES_CBC_SHA"},
    {0x0016, "SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0x0017, "SSL_DH_anon_EXPORT_WITH_RC4_40_MD5"},
    {0x0018, "SSL_DH_anon_WITH_RC4_128_MD5"},
    {0x0019, "SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA"},
    {0x001A, "SSL_DH_anon_WITH_DES_CBC_SHA"},
    {0x001B, "SSL_DH_anon_WITH_3DES_EDE_CBC_SHA"},
    {0x001D, "SSL_FORTEZZA_KEA_WITH_FORTEZZA_CBC_SHA"},
    {0x001E, "SSL_FORTEZZA_KEA_WITH_RC4_128_SHA"},
    {0x001F, "TLS_KRB5_WITH_3DES_EDE_CBC_SHA"},
    {0x0020, "TLS_KRB5_WITH_RC4_128_SHA"},
    {0x0021, "TLS_KRB5_WITH_IDEA_CBC_SHA"},
    {0x0022, "TLS_KRB5_WITH_DES_CBC_MD5"},
    {0x0023, "TLS_KRB5_WITH_3DES_EDE_CBC_MD5"},
    {0x0024, "TLS_KRB5_WITH_RC4_128_MD5"},
    {0x0025, "TLS_KRB5_WITH_IDEA_CBC_MD5"},
    {0x0026, "TLS_KRB5_EXPORT_WITH_DES_CBC_40_SHA"},
    {0x0027, "TLS_KRB5_EXPORT_WITH_RC2_CBC_40_SHA"},
    {0x0028, "TLS_KRB5_EXPORT_WITH_RC4_40_SHA"},
    {0x0029, "TLS_KRB5_EXPORT_WITH_DES_CBC_40_MD5"},
    {0x002A, "TLS_KRB5_EXPORT_WITH_RC2_CBC_40_MD5"},
    {0x002B, "TLS_KRB5_EXPORT_WITH_RC4_40_MD5"},
    {0x002F, "TLS_RSA_WITH_AES_128_CBC_SHA"},
    {0x0030, "TLS_DH_DSS_WITH_AES_128_CBC_SHA"},
    {0x0031, "TLS_DH_RSA_WITH_AES_128_CBC_SHA"},
    {0x0032, "TLS_DHE_DSS_WITH_AES_128_CBC_SHA"},
    {0x0033, "TLS_DHE_RSA_WITH_AES_128_CBC_SHA"},
    {0x0034, "TLS_DH_anon_WITH_AES_128_CBC_SHA"},
    {0x0035, "TLS_RSA_WITH_AES_256_CBC_SHA"},
    {0x0036, "TLS_DH_DSS_WITH_AES_256_CBC_SHA"},
    {0x0037, "TLS_DH_RSA_WITH_AES_256_CBC_SHA"},
    {0x0038, "TLS_DHE_DSS_WITH_AES_256_CBC_SHA"},
    {0x0039, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA"},
    {0x003A, "TLS_DH_anon_WITH_AES_256_CBC_SHA"},
    {0x003B, "TLS_RSA_WITH_NULL_SHA256"},
    {0x003C, "TLS_RSA_WITH_AES_128_CBC_SHA256"},
    {0x003D, "TLS_RSA_WITH_AES_256_CBC_SHA256"},
    {0x003E, "TLS_DH_DSS_WITH_AES_128_CBC_SHA256"},
    {0x003F, "TLS_DH_RSA_WITH_AES_128_CBC_SHA256"},
    {0x0040, "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256"},
    {0x0041, "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0042, "TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0043, "TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0044, "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0045, "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0046, "TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0067, "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256"},
    {0x0068, "TLS_DH_DSS_WITH_AES_256_CBC_SHA256"},
    {0x0069, "TLS_DH_RSA_WITH_AES_256_CBC_SHA256"},
    {0x006A, "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256"},
    {0x006B, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256"},
    {0x006C, "TLS_DH_anon_WITH_AES_128_CBC_SHA256"},
    {0x006D, "TLS_DH_anon_WITH_AES_256_CBC_SHA256"},
    {0x0084, "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0085, "TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0086, "TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0087, "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0088, "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0089, "TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA"},
    {0x008A, "TLS_PSK_WITH_RC4_128_SHA"},
    {0x008B, "TLS_PSK_WITH_3DES_EDE_CBC_SHA"},
    {0x008C, "TLS_PSK_WITH_AES_128_CBC_SHA"},
    {0x008D, "TLS_PSK_WITH_AES_256_CBC_SHA"},
    {0x008E, "TLS_DHE_PSK_WITH_RC4_128_SHA"},
    {0x008F, "TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA"},
    {0x0090, "TLS_DHE_PSK_WITH_AES_128_CBC_SHA"},
    {0x0091, "TLS_DHE_PSK_WITH_AES_256_CBC_SHA"},
    {0x0092, "TLS_RSA_PSK_WITH_RC4_128_SHA"},
    {0x0093, "TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA"},
    {0x0094, "TLS_RSA_PSK_WITH_AES_128_CBC_SHA"},
    {0x0095, "TLS_RSA_PSK_WITH_AES_256_CBC_SHA"},
    {0x0096, "TLS_RSA_WITH_SEED_CBC_SHA"},
    {0x0097, "TLS_DH_DSS_WITH_SEED_CBC_SHA"},
    {0x0098, "TLS_DH_RSA_WITH_SEED_CBC_SHA"},
    {0x0099, "TLS_DHE_DSS_WITH_SEED_CBC_SHA"},
    {0x009A, "TLS_DHE_RSA_WITH_SEED_CBC_SHA"},
    {0x009B, "TLS_DH_anon_WITH_SEED_CBC_SHA"},
    {0x009C, "TLS_RSA_WITH_AES_128_GCM_SHA256"},
    {0x009D, "TLS_RSA_WITH_AES_256_GCM_SHA384"},
    {0x009E, "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256"},
    {0x009F, "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384"},
    {0x00A0, "TLS_DH_RSA_WITH_AES_128_GCM_SHA256"},
    {0x00A1, "TLS_DH_RSA_WITH_AES_256_GCM_SHA384"},
    {0x00A2, "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256"},
    {0x00A3, "TLS_DHE_DSS_WITH_AES_256_GCM_SHA384"},
    {0x00A4, "TLS_DH_DSS_WITH_AES_128_GCM_SHA256"},
    {0x00A5, "TLS_DH_DSS_WITH_AES_256_GCM_SHA384"},
    {0x00A6, "TLS_DH_anon_WITH_AES_128_GCM_SHA256"},
    {0x00A7, "TLS_DH_anon_WITH_AES_256_GCM_SHA384"},
    {0x00A8, "TLS_PSK_WITH_AES_128_GCM_SHA256"},
    {0x00A9, "TLS_PSK_WITH_AES_256_GCM_SHA384"},
    {0x00AA, "TLS_DHE_PSK_WITH_AES_128_GCM_SHA256"},
    {0x00AB, "TLS_DHE_PSK_WITH_AES_256_GCM_SHA384"},
    {0x00AC, "TLS_RSA_PSK_WITH_AES_128_GCM_SHA256"},
    {0x00AD, "TLS_RSA_PSK_WITH_AES_256_GCM_SHA384"},
    {0x00AE, "TLS_PSK_WITH_AES_128_CBC_SHA256"},
    {0x00AF, "TLS_PSK_WITH_AES_256_CBC_SHA384"},
    {0x00B0, "TLS_PSK_WITH_NULL_SHA256"},
    {0x00B1, "TLS_PSK_WITH_NULL_SHA384"},
    {0x00B2, "TLS_DHE_PSK_WITH_AES_128_CBC_SHA256"},
    {0x00B3, "TLS_DHE_PSK_WITH_AES_256_CBC_SHA384"},
    {0x00B4, "TLS_DHE_PSK_WITH_NULL_SHA256"},
    {0x00B5, "TLS_DHE_PSK_WITH_NULL_SHA384"},
    {0x00B6, "TLS_RSA_PSK_WITH_AES_128_CBC_SHA256"},
    {0x00B7, "TLS_RSA_PSK_WITH_AES_256_CBC_SHA384"},
    {0x00B8, "TLS_RSA_PSK_WITH_NULL_SHA256"},
    {0x00B9, "TLS_RSA_PSK_WITH_NULL_SHA384"},
    {0x00BA, "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00BB, "TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00BC, "TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00BD, "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00BE, "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00BF, "TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00C0, "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00C1, "TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00C2, "TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00C3, "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00C4, "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00C5, "TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00FF, "TLS_EMPTY_RENEGOTIATION_INFO_SCSV"},
    {0xC001, "TLS_ECDH_ECDSA_WITH_NULL_SHA"},
    {0xC002, "TLS_ECDH_ECDSA_WITH_RC4_128_SHA"},
    {0xC003, "TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA"},
    {0xC004, "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA"},
    {0xC005, "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA"},
    {0xC006, "TLS_ECDHE_ECDSA_WITH_NULL_SHA"},
    {0xC007, "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA"},
    {0xC008, "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA"},
    {0xC009, "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA"},
    {0xC00A, "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA"},
    {0xC00B, "TLS_ECDH_RSA_WITH_NULL_SHA"},
    {0xC00C, "TLS_ECDH_RSA_WITH_RC4_128_SHA"},
    {0xC00D, "TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0xC00E, "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA"},
    {0xC00F, "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA"},
    {0xC010, "TLS_ECDHE_RSA_WITH_NULL_SHA"},
    {0xC011, "TLS_ECDHE_RSA_WITH_RC4_128_SHA"},
    {0xC012, "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0xC013, "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA"},
    {0xC014, "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA"},
    {0xC015, "TLS_ECDH_anon_WITH_NULL_SHA"},
    {0xC016, "TLS_ECDH_anon_WITH_RC4_128_SHA"},
    {0xC017, "TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA"},
    {0xC018, "TLS_ECDH_anon_WITH_AES_128_CBC_SHA"},
    {0xC019, "TLS_ECDH_anon_WITH_AES_256_CBC_SHA"},
    {0xC01A, "TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA"},
    {0xC01B, "TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0xC01C, "TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA"},
    {0xC01D, "TLS_SRP_SHA_WITH_AES_128_CBC_SHA"},
    {0xC01E, "TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA"},
    {0xC01F, "TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA"},
    {0xC020, "TLS_SRP_SHA_WITH_AES_256_CBC_SHA"},
    {0xC021, "TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA"},
    {0xC022, "TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA"},
    {0xC023, "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256"},
    {0xC024, "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384"},
    {0xC025, "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256"},
    {0xC026, "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384"},
    {0xC027, "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256"},
    {0xC028, "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384"},
    {0xC029, "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256"},
    {0xC02A, "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384"},
    {0xC02B, "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256"},
    {0xC02C, "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384"},
    {0xC02D, "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256"},
    {0xC02E, "TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384"},
    {0xC02F, "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256"},
    {0xC030, "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384"},
    {0xC031, "TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256"},
    {0xC032, "TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384"},
    {0xFEFE, "SSL_RSA_FIPS_WITH_DES_CBC_SHA"},
    {0xFEFF, "SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA"},
};

/* Compression methods */
static ssl_trace_tbl ssl_comp_tbl[] = {
    {0x0000, "No Compression"},
    {0x0001, "Zlib Compression"}
};

/* Extensions */
static ssl_trace_tbl ssl_exts_tbl[] = {
    {TLSEXT_TYPE_server_name, "server_name"},
    {TLSEXT_TYPE_max_fragment_length, "max_fragment_length"},
    {TLSEXT_TYPE_client_certificate_url, "client_certificate_url"},
    {TLSEXT_TYPE_trusted_ca_keys, "trusted_ca_keys"},
    {TLSEXT_TYPE_truncated_hmac, "truncated_hmac"},
    {TLSEXT_TYPE_status_request, "status_request"},
    {TLSEXT_TYPE_user_mapping, "user_mapping"},
    {TLSEXT_TYPE_client_authz, "client_authz"},
    {TLSEXT_TYPE_server_authz, "server_authz"},
    {TLSEXT_TYPE_cert_type, "cert_type"},
    {TLSEXT_TYPE_elliptic_curves, "elliptic_curves"},
    {TLSEXT_TYPE_ec_point_formats, "ec_point_formats"},
    {TLSEXT_TYPE_srp, "srp"},
    {TLSEXT_TYPE_signature_algorithms, "signature_algorithms"},
    {TLSEXT_TYPE_use_srtp, "use_srtp"},
    {TLSEXT_TYPE_heartbeat, "heartbeat"},
    {TLSEXT_TYPE_session_ticket, "session_ticket"},
# ifdef TLSEXT_TYPE_opaque_prf_input
    {TLSEXT_TYPE_opaque_prf_input, "opaque_prf_input"},
# endif
    {TLSEXT_TYPE_renegotiate, "renegotiate"},
    {TLSEXT_TYPE_next_proto_neg, "next_proto_neg"},
    {TLSEXT_TYPE_padding, "padding"}
};

static ssl_trace_tbl ssl_curve_tbl[] = {
    {1, "sect163k1 (K-163)"},
    {2, "sect163r1"},
    {3, "sect163r2 (B-163)"},
    {4, "sect193r1"},
    {5, "sect193r2"},
    {6, "sect233k1 (K-233)"},
    {7, "sect233r1 (B-233)"},
    {8, "sect239k1"},
    {9, "sect283k1 (K-283)"},
    {10, "sect283r1 (B-283)"},
    {11, "sect409k1 (K-409)"},
    {12, "sect409r1 (B-409)"},
    {13, "sect571k1 (K-571)"},
    {14, "sect571r1 (B-571)"},
    {15, "secp160k1"},
    {16, "secp160r1"},
    {17, "secp160r2"},
    {18, "secp192k1"},
    {19, "secp192r1 (P-192)"},
    {20, "secp224k1"},
    {21, "secp224r1 (P-224)"},
    {22, "secp256k1"},
    {23, "secp256r1 (P-256)"},
    {24, "secp384r1 (P-384)"},
    {25, "secp521r1 (P-521)"},
    {26, "brainpoolP256r1"},
    {27, "brainpoolP384r1"},
    {28, "brainpoolP512r1"},
    {0xFF01, "arbitrary_explicit_prime_curves"},
    {0xFF02, "arbitrary_explicit_char2_curves"}
};

static ssl_trace_tbl ssl_point_tbl[] = {
    {0, "uncompressed"},
    {1, "ansiX962_compressed_prime"},
    {2, "ansiX962_compressed_char2"}
};

static ssl_trace_tbl ssl_md_tbl[] = {
    {0, "none"},
    {1, "md5"},
    {2, "sha1"},
    {3, "sha224"},
    {4, "sha256"},
    {5, "sha384"},
    {6, "sha512"}
};

static ssl_trace_tbl ssl_sig_tbl[] = {
    {0, "anonymous"},
    {1, "rsa"},
    {2, "dsa"},
    {3, "ecdsa"}
};

static ssl_trace_tbl ssl_hb_tbl[] = {
    {1, "peer_allowed_to_send"},
    {2, "peer_not_allowed_to_send"}
};

static ssl_trace_tbl ssl_hb_type_tbl[] = {
    {1, "heartbeat_request"},
    {2, "heartbeat_response"}
};

static ssl_trace_tbl ssl_ctype_tbl[] = {
    {1, "rsa_sign"},
    {2, "dss_sign"},
    {3, "rsa_fixed_dh"},
    {4, "dss_fixed_dh"},
    {5, "rsa_ephemeral_dh"},
    {6, "dss_ephemeral_dh"},
    {20, "fortezza_dms"},
    {64, "ecdsa_sign"},
    {65, "rsa_fixed_ecdh"},
    {66, "ecdsa_fixed_ecdh"}
};

static ssl_trace_tbl ssl_crypto_tbl[] = {
    {TLS1_RT_CRYPTO_PREMASTER, "Premaster Secret"},
    {TLS1_RT_CRYPTO_CLIENT_RANDOM, "Client Random"},
    {TLS1_RT_CRYPTO_SERVER_RANDOM, "Server Random"},
    {TLS1_RT_CRYPTO_MASTER, "Master Secret"},
    {TLS1_RT_CRYPTO_MAC | TLS1_RT_CRYPTO_WRITE, "Write Mac Secret"},
    {TLS1_RT_CRYPTO_MAC | TLS1_RT_CRYPTO_READ, "Read Mac Secret"},
    {TLS1_RT_CRYPTO_KEY | TLS1_RT_CRYPTO_WRITE, "Write Key"},
    {TLS1_RT_CRYPTO_KEY | TLS1_RT_CRYPTO_READ, "Read Key"},
    {TLS1_RT_CRYPTO_IV | TLS1_RT_CRYPTO_WRITE, "Write IV"},
    {TLS1_RT_CRYPTO_IV | TLS1_RT_CRYPTO_READ, "Read IV"},
    {TLS1_RT_CRYPTO_FIXED_IV | TLS1_RT_CRYPTO_WRITE, "Write IV (fixed part)"},
    {TLS1_RT_CRYPTO_FIXED_IV | TLS1_RT_CRYPTO_READ, "Read IV (fixed part)"}
};

static void ssl_print_hex(BIO *bio, int indent, const char *name,
                          const unsigned char *msg, size_t msglen)
{
    size_t i;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "%s (len=%d): ", name, (int)msglen);
    for (i = 0; i < msglen; i++)
        BIO_printf(bio, "%02X", msg[i]);
    BIO_puts(bio, "\n");
}

static int ssl_print_hexbuf(BIO *bio, int indent,
                            const char *name, size_t nlen,
                            const unsigned char **pmsg, size_t *pmsglen)
{
    size_t blen;
    const unsigned char *p = *pmsg;
    if (*pmsglen < nlen)
        return 0;
    blen = p[0];
    if (nlen > 1)
        blen = (blen << 8) | p[1];
    if (*pmsglen < nlen + blen)
        return 0;
    p += nlen;
    ssl_print_hex(bio, indent, name, p, blen);
    *pmsg += blen + nlen;
    *pmsglen -= blen + nlen;
    return 1;
}

static int ssl_print_version(BIO *bio, int indent, const char *name,
                             const unsigned char **pmsg, size_t *pmsglen)
{
    int vers;
    if (*pmsglen < 2)
        return 0;
    vers = ((*pmsg)[0] << 8) | (*pmsg)[1];
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "%s=0x%x (%s)\n",
               name, vers, ssl_trace_str(vers, ssl_version_tbl));
    *pmsg += 2;
    *pmsglen -= 2;
    return 1;
}

static int ssl_print_random(BIO *bio, int indent,
                            const unsigned char **pmsg, size_t *pmsglen)
{
    unsigned int tm;
    const unsigned char *p = *pmsg;
    if (*pmsglen < 32)
        return 0;
    tm = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    p += 4;
    BIO_indent(bio, indent, 80);
    BIO_puts(bio, "Random:\n");
    BIO_indent(bio, indent + 2, 80);
    BIO_printf(bio, "gmt_unix_time=0x%08X\n", tm);
    ssl_print_hex(bio, indent + 2, "random_bytes", p, 28);
    *pmsg += 32;
    *pmsglen -= 32;
    return 1;
}

static int ssl_print_signature(BIO *bio, int indent, SSL *s,
                               const unsigned char **pmsg, size_t *pmsglen)
{
    if (*pmsglen < 2)
        return 0;
    if (SSL_USE_SIGALGS(s)) {
        const unsigned char *p = *pmsg;
        BIO_indent(bio, indent, 80);
        BIO_printf(bio, "Signature Algorithm %s+%s (%d+%d)\n",
                   ssl_trace_str(p[0], ssl_md_tbl),
                   ssl_trace_str(p[1], ssl_sig_tbl), p[0], p[1]);
        *pmsg += 2;
        *pmsglen -= 2;
    }
    return ssl_print_hexbuf(bio, indent, "Signature", 2, pmsg, pmsglen);
}

static int ssl_print_extension(BIO *bio, int indent, int server, int extype,
                               const unsigned char *ext, size_t extlen)
{
    size_t xlen;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "extension_type=%s(%d), length=%d\n",
               ssl_trace_str(extype, ssl_exts_tbl), extype, (int)extlen);
    switch (extype) {
    case TLSEXT_TYPE_ec_point_formats:
        if (extlen < 1)
            return 0;
        xlen = ext[0];
        if (extlen != xlen + 1)
            return 0;
        return ssl_trace_list(bio, indent + 2,
                              ext + 1, xlen, 1, ssl_point_tbl);

    case TLSEXT_TYPE_elliptic_curves:
        if (extlen < 2)
            return 0;
        xlen = (ext[0] << 8) | ext[1];
        if (extlen != xlen + 2)
            return 0;
        return ssl_trace_list(bio, indent + 2,
                              ext + 2, xlen, 2, ssl_curve_tbl);

    case TLSEXT_TYPE_signature_algorithms:

        if (extlen < 2)
            return 0;
        xlen = (ext[0] << 8) | ext[1];
        if (extlen != xlen + 2)
            return 0;
        if (xlen & 1)
            return 0;
        ext += 2;
        while (xlen > 0) {
            BIO_indent(bio, indent + 2, 80);
            BIO_printf(bio, "%s+%s (%d+%d)\n",
                       ssl_trace_str(ext[0], ssl_md_tbl),
                       ssl_trace_str(ext[1], ssl_sig_tbl), ext[0], ext[1]);
            xlen -= 2;
            ext += 2;
        }
        break;

    case TLSEXT_TYPE_renegotiate:
        if (extlen < 1)
            return 0;
        xlen = ext[0];
        if (xlen + 1 != extlen)
            return 0;
        ext++;
        if (xlen) {
            if (server) {
                if (xlen & 1)
                    return 0;
                xlen >>= 1;
            }
            ssl_print_hex(bio, indent + 4, "client_verify_data", ext, xlen);
            if (server) {
                ext += xlen;
                ssl_print_hex(bio, indent + 4,
                              "server_verify_data", ext, xlen);
            }
        } else {
            BIO_indent(bio, indent + 4, 80);
            BIO_puts(bio, "<EMPTY>\n");
        }
        break;

    case TLSEXT_TYPE_heartbeat:
        if (extlen != 1)
            return 0;
        BIO_indent(bio, indent + 2, 80);
        BIO_printf(bio, "HeartbeatMode: %s\n",
                   ssl_trace_str(ext[0], ssl_hb_tbl));
        break;

    case TLSEXT_TYPE_session_ticket:
        if (extlen != 0)
            ssl_print_hex(bio, indent + 4, "ticket", ext, extlen);
        break;

    default:
        BIO_dump_indent(bio, (char *)ext, extlen, indent + 2);
    }
    return 1;
}

static int ssl_print_extensions(BIO *bio, int indent, int server,
                                const unsigned char *msg, size_t msglen)
{
    size_t extslen;
    BIO_indent(bio, indent, 80);
    if (msglen == 0) {
        BIO_puts(bio, "No Extensions\n");
        return 1;
    }
    extslen = (msg[0] << 8) | msg[1];
    if (extslen != msglen - 2)
        return 0;
    msg += 2;
    msglen = extslen;
    BIO_printf(bio, "extensions, length = %d\n", (int)msglen);
    while (msglen > 0) {
        int extype;
        size_t extlen;
        if (msglen < 4)
            return 0;
        extype = (msg[0] << 8) | msg[1];
        extlen = (msg[2] << 8) | msg[3];
        if (msglen < extlen + 4)
            return 0;
        msg += 4;
        if (!ssl_print_extension(bio, indent + 2, server,
                                 extype, msg, extlen))
            return 0;
        msg += extlen;
        msglen -= extlen + 4;
    }
    return 1;
}

static int ssl_print_client_hello(BIO *bio, SSL *ssl, int indent,
                                  const unsigned char *msg, size_t msglen)
{
    size_t len;
    unsigned int cs;
    if (!ssl_print_version(bio, indent, "client_version", &msg, &msglen))
        return 0;
    if (!ssl_print_random(bio, indent, &msg, &msglen))
        return 0;
    if (!ssl_print_hexbuf(bio, indent, "session_id", 1, &msg, &msglen))
        return 0;
    if (SSL_IS_DTLS(ssl)) {
        if (!ssl_print_hexbuf(bio, indent, "cookie", 1, &msg, &msglen))
            return 0;
    }
    if (msglen < 2)
        return 0;
    len = (msg[0] << 8) | msg[1];
    msg += 2;
    msglen -= 2;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "cipher_suites (len=%d)\n", (int)len);
    if (msglen < len || len & 1)
        return 0;
    while (len > 0) {
        cs = (msg[0] << 8) | msg[1];
        BIO_indent(bio, indent + 2, 80);
        BIO_printf(bio, "{0x%02X, 0x%02X} %s\n",
                   msg[0], msg[1], ssl_trace_str(cs, ssl_ciphers_tbl));
        msg += 2;
        msglen -= 2;
        len -= 2;
    }
    if (msglen < 1)
        return 0;
    len = msg[0];
    msg++;
    msglen--;
    if (msglen < len)
        return 0;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "compression_methods (len=%d)\n", (int)len);
    while (len > 0) {
        BIO_indent(bio, indent + 2, 80);
        BIO_printf(bio, "%s (0x%02X)\n",
                   ssl_trace_str(msg[0], ssl_comp_tbl), msg[0]);
        msg++;
        msglen--;
        len--;
    }
    if (!ssl_print_extensions(bio, indent, 0, msg, msglen))
        return 0;
    return 1;
}

static int dtls_print_hello_vfyrequest(BIO *bio, int indent,
                                       const unsigned char *msg,
                                       size_t msglen)
{
    if (!ssl_print_version(bio, indent, "server_version", &msg, &msglen))
        return 0;
    if (!ssl_print_hexbuf(bio, indent, "cookie", 1, &msg, &msglen))
        return 0;
    return 1;
}

static int ssl_print_server_hello(BIO *bio, int indent,
                                  const unsigned char *msg, size_t msglen)
{
    unsigned int cs;
    if (!ssl_print_version(bio, indent, "server_version", &msg, &msglen))
        return 0;
    if (!ssl_print_random(bio, indent, &msg, &msglen))
        return 0;
    if (!ssl_print_hexbuf(bio, indent, "session_id", 1, &msg, &msglen))
        return 0;
    if (msglen < 2)
        return 0;
    cs = (msg[0] << 8) | msg[1];
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "cipher_suite {0x%02X, 0x%02X} %s\n",
               msg[0], msg[1], ssl_trace_str(cs, ssl_ciphers_tbl));
    msg += 2;
    msglen -= 2;
    if (msglen < 1)
        return 0;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "compression_method: %s (0x%02X)\n",
               ssl_trace_str(msg[0], ssl_comp_tbl), msg[0]);
    msg++;
    msglen--;
    if (!ssl_print_extensions(bio, indent, 1, msg, msglen))
        return 0;
    return 1;
}

static int ssl_get_keyex(const char **pname, SSL *ssl)
{
    unsigned long alg_k = ssl->s3->tmp.new_cipher->algorithm_mkey;
    if (alg_k & SSL_kRSA) {
        *pname = "rsa";
        return SSL_kRSA;
    }
    if (alg_k & SSL_kDHr) {
        *pname = "dh_rsa";
        return SSL_kDHr;
    }
    if (alg_k & SSL_kDHd) {
        *pname = "dh_dss";
        return SSL_kDHd;
    }
    if (alg_k & SSL_kKRB5) {
        *pname = "krb5";
        return SSL_kKRB5;
    }
    if (alg_k & SSL_kEDH) {
        *pname = "edh";
        return SSL_kEDH;
    }
    if (alg_k & SSL_kEECDH) {
        *pname = "EECDH";
        return SSL_kEECDH;
    }
    if (alg_k & SSL_kECDHr) {
        *pname = "ECDH RSA";
        return SSL_kECDHr;
    }
    if (alg_k & SSL_kECDHe) {
        *pname = "ECDH ECDSA";
        return SSL_kECDHe;
    }
    if (alg_k & SSL_kPSK) {
        *pname = "PSK";
        return SSL_kPSK;
    }
    if (alg_k & SSL_kSRP) {
        *pname = "SRP";
        return SSL_kSRP;
    }
    if (alg_k & SSL_kGOST) {
        *pname = "GOST";
        return SSL_kGOST;
    }
    *pname = "UNKNOWN";
    return 0;
}

static int ssl_print_client_keyex(BIO *bio, int indent, SSL *ssl,
                                  const unsigned char *msg, size_t msglen)
{
    const char *algname;
    int id;
    id = ssl_get_keyex(&algname, ssl);
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "KeyExchangeAlgorithm=%s\n", algname);
    switch (id) {

    case SSL_kRSA:
        if (TLS1_get_version(ssl) == SSL3_VERSION) {
            ssl_print_hex(bio, indent + 2,
                          "EncyptedPreMasterSecret", msg, msglen);
        } else {
            if (!ssl_print_hexbuf(bio, indent + 2,
                                  "EncyptedPreMasterSecret", 2,
                                  &msg, &msglen))
                return 0;
        }
        break;

        /* Implicit parameters only allowed for static DH */
    case SSL_kDHd:
    case SSL_kDHr:
        if (msglen == 0) {
            BIO_indent(bio, indent + 2, 80);
            BIO_puts(bio, "implicit\n");
            break;
        }
    case SSL_kEDH:
        if (!ssl_print_hexbuf(bio, indent + 2, "dh_Yc", 2, &msg, &msglen))
            return 0;
        break;

    case SSL_kECDHr:
    case SSL_kECDHe:
        if (msglen == 0) {
            BIO_indent(bio, indent + 2, 80);
            BIO_puts(bio, "implicit\n");
            break;
        }
    case SSL_kEECDH:
        if (!ssl_print_hexbuf(bio, indent + 2, "ecdh_Yc", 1, &msg, &msglen))
            return 0;
        break;
    }

    return 1;
}

static int ssl_print_server_keyex(BIO *bio, int indent, SSL *ssl,
                                  const unsigned char *msg, size_t msglen)
{
    const char *algname;
    int id;
    id = ssl_get_keyex(&algname, ssl);
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "KeyExchangeAlgorithm=%s\n", algname);
    switch (id) {
        /* Should never happen */
    case SSL_kDHd:
    case SSL_kDHr:
    case SSL_kECDHr:
    case SSL_kECDHe:
        BIO_indent(bio, indent + 2, 80);
        BIO_printf(bio, "Unexpected Message\n");
        break;

    case SSL_kRSA:

        if (!ssl_print_hexbuf(bio, indent + 2, "rsa_modulus", 2,
                              &msg, &msglen))
            return 0;
        if (!ssl_print_hexbuf(bio, indent + 2, "rsa_exponent", 2,
                              &msg, &msglen))
            return 0;
        break;

    case SSL_kEDH:
        if (!ssl_print_hexbuf(bio, indent + 2, "dh_p", 2, &msg, &msglen))
            return 0;
        if (!ssl_print_hexbuf(bio, indent + 2, "dh_g", 2, &msg, &msglen))
            return 0;
        if (!ssl_print_hexbuf(bio, indent + 2, "dh_Ys", 2, &msg, &msglen))
            return 0;
        break;

    case SSL_kEECDH:
        if (msglen < 1)
            return 0;
        BIO_indent(bio, indent + 2, 80);
        if (msg[0] == EXPLICIT_PRIME_CURVE_TYPE)
            BIO_puts(bio, "explicit_prime\n");
        else if (msg[0] == EXPLICIT_CHAR2_CURVE_TYPE)
            BIO_puts(bio, "explicit_char2\n");
        else if (msg[0] == NAMED_CURVE_TYPE) {
            int curve;
            if (msglen < 3)
                return 0;
            curve = (msg[1] << 8) | msg[2];
            BIO_printf(bio, "named_curve: %s (%d)\n",
                       ssl_trace_str(curve, ssl_curve_tbl), curve);
            msg += 3;
            msglen -= 3;
            if (!ssl_print_hexbuf(bio, indent + 2, "point", 1, &msg, &msglen))
                return 0;
        }
        break;
    }
    return ssl_print_signature(bio, indent, ssl, &msg, &msglen);
}

static int ssl_print_certificate(BIO *bio, int indent,
                                 const unsigned char **pmsg, size_t *pmsglen)
{
    size_t msglen = *pmsglen;
    size_t clen;
    X509 *x;
    const unsigned char *p = *pmsg, *q;
    if (msglen < 3)
        return 0;
    clen = (p[0] << 16) | (p[1] << 8) | p[2];
    if (msglen < clen + 3)
        return 0;
    q = p + 3;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "ASN.1Cert, length=%d", (int)clen);
    x = d2i_X509(NULL, &q, clen);
    if (!x)
        BIO_puts(bio, "<UNPARSEABLE CERTIFICATE>\n");
    else {
        BIO_puts(bio, "\n------details-----\n");
        X509_print_ex(bio, x, XN_FLAG_ONELINE, 0);
        PEM_write_bio_X509(bio, x);
        /* Print certificate stuff */
        BIO_puts(bio, "------------------\n");
        X509_free(x);
    }
    if (q != p + 3 + clen) {
        BIO_puts(bio, "<TRAILING GARBAGE AFTER CERTIFICATE>\n");
    }
    *pmsg += clen + 3;
    *pmsglen -= clen + 3;
    return 1;
}

static int ssl_print_certificates(BIO *bio, int indent,
                                  const unsigned char *msg, size_t msglen)
{
    size_t clen;
    if (msglen < 3)
        return 0;
    clen = (msg[0] << 16) | (msg[1] << 8) | msg[2];
    if (msglen != clen + 3)
        return 0;
    msg += 3;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "certificate_list, length=%d\n", (int)clen);
    while (clen > 0) {
        if (!ssl_print_certificate(bio, indent + 2, &msg, &clen))
            return 0;
    }
    return 1;
}

static int ssl_print_cert_request(BIO *bio, int indent, SSL *s,
                                  const unsigned char *msg, size_t msglen)
{
    size_t xlen;
    if (msglen < 1)
        return 0;
    xlen = msg[0];
    if (msglen < xlen + 1)
        return 0;
    msg++;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "certificate_types (len=%d)\n", (int)xlen);
    if (!ssl_trace_list(bio, indent + 2, msg, xlen, 1, ssl_ctype_tbl))
        return 0;
    msg += xlen;
    msglen -= xlen + 1;
    if (!SSL_USE_SIGALGS(s))
        goto skip_sig;
    if (msglen < 2)
        return 0;
    xlen = (msg[0] << 8) | msg[1];
    if (msglen < xlen + 2 || (xlen & 1))
        return 0;
    msg += 2;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "signature_algorithms (len=%d)\n", (int)xlen);
    while (xlen > 0) {
        BIO_indent(bio, indent + 2, 80);
        BIO_printf(bio, "%s+%s (%d+%d)\n",
                   ssl_trace_str(msg[0], ssl_md_tbl),
                   ssl_trace_str(msg[1], ssl_sig_tbl), msg[0], msg[1]);
        xlen -= 2;
        msg += 2;
    }
    msg += xlen;
    msglen -= xlen + 2;

 skip_sig:
    xlen = (msg[0] << 8) | msg[1];
    BIO_indent(bio, indent, 80);
    if (msglen < xlen + 2)
        return 0;
    msg += 2;
    msglen -= 2;
    BIO_printf(bio, "certificate_authorities (len=%d)\n", (int)xlen);
    while (xlen > 0) {
        size_t dlen;
        X509_NAME *nm;
        const unsigned char *p;
        if (xlen < 2)
            return 0;
        dlen = (msg[0] << 8) | msg[1];
        if (xlen < dlen + 2)
            return 0;
        msg += 2;
        BIO_indent(bio, indent + 2, 80);
        BIO_printf(bio, "DistinguishedName (len=%d): ", (int)dlen);
        p = msg;
        nm = d2i_X509_NAME(NULL, &p, dlen);
        if (!nm) {
            BIO_puts(bio, "<UNPARSEABLE DN>\n");
        } else {
            X509_NAME_print_ex(bio, nm, 0, XN_FLAG_ONELINE);
            BIO_puts(bio, "\n");
            X509_NAME_free(nm);
        }
        xlen -= dlen + 2;
        msg += dlen;
    }
    return 1;
}

static int ssl_print_ticket(BIO *bio, int indent,
                            const unsigned char *msg, size_t msglen)
{
    unsigned int tick_life;
    if (msglen == 0) {
        BIO_indent(bio, indent + 2, 80);
        BIO_puts(bio, "No Ticket\n");
        return 1;
    }
    if (msglen < 4)
        return 0;
    tick_life = (msg[0] << 24) | (msg[1] << 16) | (msg[2] << 8) | msg[3];
    msglen -= 4;
    msg += 4;
    BIO_indent(bio, indent + 2, 80);
    BIO_printf(bio, "ticket_lifetime_hint=%u\n", tick_life);
    if (!ssl_print_hexbuf(bio, indent + 2, "ticket", 2, &msg, &msglen))
        return 0;
    if (msglen)
        return 0;
    return 1;
}

static int ssl_print_handshake(BIO *bio, SSL *ssl,
                               const unsigned char *msg, size_t msglen,
                               int indent)
{
    size_t hlen;
    unsigned char htype;
    if (msglen < 4)
        return 0;
    htype = msg[0];
    hlen = (msg[1] << 16) | (msg[2] << 8) | msg[3];
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "%s, Length=%d\n",
               ssl_trace_str(htype, ssl_handshake_tbl), (int)hlen);
    msg += 4;
    msglen -= 4;
    if (SSL_IS_DTLS(ssl)) {
        if (msglen < 8)
            return 0;
        BIO_indent(bio, indent, 80);
        BIO_printf(bio, "message_seq=%d, fragment_offset=%d, "
                   "fragment_length=%d\n",
                   (msg[0] << 8) | msg[1],
                   (msg[2] << 16) | (msg[3] << 8) | msg[4],
                   (msg[5] << 16) | (msg[6] << 8) | msg[7]);
        msg += 8;
        msglen -= 8;
    }
    if (msglen < hlen)
        return 0;
    switch (htype) {
    case SSL3_MT_CLIENT_HELLO:
        if (!ssl_print_client_hello(bio, ssl, indent + 2, msg, msglen))
            return 0;
        break;

    case DTLS1_MT_HELLO_VERIFY_REQUEST:
        if (!dtls_print_hello_vfyrequest(bio, indent + 2, msg, msglen))
            return 0;
        break;

    case SSL3_MT_SERVER_HELLO:
        if (!ssl_print_server_hello(bio, indent + 2, msg, msglen))
            return 0;
        break;

    case SSL3_MT_SERVER_KEY_EXCHANGE:
        if (!ssl_print_server_keyex(bio, indent + 2, ssl, msg, msglen))
            return 0;
        break;

    case SSL3_MT_CLIENT_KEY_EXCHANGE:
        if (!ssl_print_client_keyex(bio, indent + 2, ssl, msg, msglen))
            return 0;
        break;

    case SSL3_MT_CERTIFICATE:
        if (!ssl_print_certificates(bio, indent + 2, msg, msglen))
            return 0;
        break;

    case SSL3_MT_CERTIFICATE_VERIFY:
        if (!ssl_print_signature(bio, indent + 2, ssl, &msg, &msglen))
            return 0;
        break;

    case SSL3_MT_CERTIFICATE_REQUEST:
        if (!ssl_print_cert_request(bio, indent + 2, ssl, msg, msglen))
            return 0;
        break;

    case SSL3_MT_FINISHED:
        ssl_print_hex(bio, indent + 2, "verify_data", msg, msglen);
        break;

    case SSL3_MT_SERVER_DONE:
        if (msglen != 0)
            ssl_print_hex(bio, indent + 2, "unexpected value", msg, msglen);
        break;

    case SSL3_MT_NEWSESSION_TICKET:
        if (!ssl_print_ticket(bio, indent + 2, msg, msglen))
            return 0;
        break;

    default:
        BIO_indent(bio, indent + 2, 80);
        BIO_puts(bio, "Unsupported, hex dump follows:\n");
        BIO_dump_indent(bio, (char *)msg, msglen, indent + 4);
    }
    return 1;
}

static int ssl_print_heartbeat(BIO *bio, int indent,
                               const unsigned char *msg, size_t msglen)
{
    if (msglen < 3)
        return 0;
    BIO_indent(bio, indent, 80);
    BIO_printf(bio, "HeartBeatMessageType: %s\n",
               ssl_trace_str(msg[0], ssl_hb_type_tbl));
    msg++;
    msglen--;
    if (!ssl_print_hexbuf(bio, indent, "payload", 2, &msg, &msglen))
        return 0;
    ssl_print_hex(bio, indent, "padding", msg, msglen);
    return 1;
}

const char *SSL_CIPHER_standard_name(const SSL_CIPHER *c)
{
    if (c->algorithm_ssl & SSL_SSLV2)
        return NULL;
    return ssl_trace_str(c->id & 0xFFFF, ssl_ciphers_tbl);
}

void SSL_trace(int write_p, int version, int content_type,
               const void *buf, size_t msglen, SSL *ssl, void *arg)
{
    const unsigned char *msg = buf;
    BIO *bio = arg;

    if (write_p == 2) {
        BIO_puts(bio, "Session ");
        ssl_print_hex(bio, 0,
                      ssl_trace_str(content_type, ssl_crypto_tbl),
                      msg, msglen);
        return;
    }
    switch (content_type) {
    case SSL3_RT_HEADER:
        {
            int hvers = msg[1] << 8 | msg[2];
            BIO_puts(bio, write_p ? "Sent" : "Received");
            BIO_printf(bio, " Record\nHeader:\n  Version = %s (0x%x)\n",
                       ssl_trace_str(hvers, ssl_version_tbl), hvers);
            if (SSL_IS_DTLS(ssl)) {
                BIO_printf(bio,
                           "  epoch=%d, sequence_number=%04x%04x%04x\n",
                           (msg[3] << 8 | msg[4]),
                           (msg[5] << 8 | msg[6]),
                           (msg[7] << 8 | msg[8]), (msg[9] << 8 | msg[10]));
# if 0
                /*
                 * Just print handshake type so we can see what is going on
                 * during fragmentation.
                 */
                BIO_printf(bio, "(%s)\n",
                           ssl_trace_str(msg[msglen], ssl_handshake_tbl));
# endif
            }

            BIO_printf(bio, "  Content Type = %s (%d)\n  Length = %d",
                       ssl_trace_str(msg[0], ssl_content_tbl), msg[0],
                       msg[msglen - 2] << 8 | msg[msglen - 1]);
        }
        break;
    case SSL3_RT_HANDSHAKE:
        if (!ssl_print_handshake(bio, ssl, msg, msglen, 4))
            BIO_printf(bio, "Message length parse error!\n");
        break;

    case SSL3_RT_CHANGE_CIPHER_SPEC:
        if (msglen == 1 && msg[0] == 1)
            BIO_puts(bio, "    change_cipher_spec (1)\n");
        else
            ssl_print_hex(bio, 4, "unknown value", msg, msglen);
        break;

    case SSL3_RT_ALERT:
        if (msglen != 2) {
            BIO_puts(bio, "    Illegal Alert Length\n");
        } else {
            BIO_printf(bio, "    Level=%s(%d), description=%s(%d)\n",
                       SSL_alert_type_string_long(msg[0] << 8),
                       msg[0], SSL_alert_desc_string_long(msg[1]), msg[1]);
        }
        break;

    case TLS1_RT_HEARTBEAT:
        ssl_print_heartbeat(bio, 4, msg, msglen);
        break;

    }

    BIO_puts(bio, "\n");
}

#endif
