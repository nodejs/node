/*
 * Copyright 2016-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if defined(__TANDEM) && defined(_SPT_MODEL_)
#include <spthread.h>
#include <spt_extensions.h> /* timeval */
#endif

#include <string.h>
#include "internal/nelem.h"
#include "internal/cryptlib.h"
#include "internal/ssl_unwrap.h"
#include "../ssl_local.h"
#include "statem_local.h"
#include <openssl/ocsp.h>
#include <openssl/core_names.h>

/*
 * values for ext_defs ech_handling field
 * exceptionally, we don't conditionally compile that field to avoid a pile of
 * ifndefs all over the ext_defs values
 */
#define OSSL_ECH_HANDLING_CALL_BOTH 1 /* call constructor both times */
#define OSSL_ECH_HANDLING_COMPRESS 2 /* compress outer value into inner */
#define OSSL_ECH_HANDLING_DUPLICATE 3 /* same value in inner and outer */
/*
 * DUPLICATE isn't really useful other than to show we can,
 * and for debugging/tests/coverage so may disappear. Changes mostly
 * won't affect the outer CH size, due to padding, but might for some
 * larger extensions.
 *
 * Note there is a co-dependency with test/recipes/75-test_quicapi.t:
 * If you change an |ech_handling| value, that may well affect the order
 * of extensions in a ClientHello, which is reflected in the test data
 * in test/recipes/75-test_quicapi_data/\*.txt files. To fix, you need
 * to look in test-runs/test_quicapi for the "new" files and then edit
 * (replacing actual octets with "?" in relevant places), and copy the
 * result back over to test/recipes/75-test_quicapi_data/. The reason
 * this happens is the ECH COMPRESS'd extensions need to be contiguous
 * in the ClientHello, so changes to/from COMPRESS affect extension
 * order, in inner and outer CH. There doesn't seem to be an easy,
 * generic, way to reconcile these compile-time changes with having
 * fixed value test files. Likely the best option is to decide on the
 * disposition of ECH COMPRESS or not and consider that an at least
 * medium-term thing. (But still allow other builds to vary at
 * compile time if they need something different.)
 */
#ifndef OPENSSL_NO_ECH
static int init_ech(SSL_CONNECTION *s, unsigned int context);
static int final_ech(SSL_CONNECTION *s, unsigned int context, int sent);
#endif /* OPENSSL_NO_ECH */

static int final_renegotiate(SSL_CONNECTION *s, unsigned int context, int sent);
static int init_server_name(SSL_CONNECTION *s, unsigned int context);
static int final_server_name(SSL_CONNECTION *s, unsigned int context, int sent);
static int init_session_ticket(SSL_CONNECTION *s, unsigned int context);
#ifndef OPENSSL_NO_OCSP
static int init_status_request(SSL_CONNECTION *s, unsigned int context);
#endif
#ifndef OPENSSL_NO_NEXTPROTONEG
static int init_npn(SSL_CONNECTION *s, unsigned int context);
#endif
static int init_alpn(SSL_CONNECTION *s, unsigned int context);
static int final_alpn(SSL_CONNECTION *s, unsigned int context, int sent);
static int init_sig_algs_cert(SSL_CONNECTION *s, unsigned int context);
static int init_sig_algs(SSL_CONNECTION *s, unsigned int context);
static int init_server_cert_type(SSL_CONNECTION *sc, unsigned int context);
static int init_client_cert_type(SSL_CONNECTION *sc, unsigned int context);
static int init_certificate_authorities(SSL_CONNECTION *s,
    unsigned int context);
static EXT_RETURN tls_construct_certificate_authorities(SSL_CONNECTION *s,
    WPACKET *pkt,
    unsigned int context,
    X509 *x,
    size_t chainidx);
static int tls_parse_certificate_authorities(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx);
#ifndef OPENSSL_NO_SRP
static int init_srp(SSL_CONNECTION *s, unsigned int context);
#endif
static int init_ec_point_formats(SSL_CONNECTION *s, unsigned int context);
static int init_etm(SSL_CONNECTION *s, unsigned int context);
static int init_ems(SSL_CONNECTION *s, unsigned int context);
static int final_ems(SSL_CONNECTION *s, unsigned int context, int sent);
static int init_psk_kex_modes(SSL_CONNECTION *s, unsigned int context);
static int final_key_share(SSL_CONNECTION *s, unsigned int context, int sent);
#ifndef OPENSSL_NO_SRTP
static int init_srtp(SSL_CONNECTION *s, unsigned int context);
#endif
static int final_sig_algs(SSL_CONNECTION *s, unsigned int context, int sent);
static int final_supported_versions(SSL_CONNECTION *s, unsigned int context,
    int sent);
static int final_early_data(SSL_CONNECTION *s, unsigned int context, int sent);
static int final_maxfragmentlen(SSL_CONNECTION *s, unsigned int context,
    int sent);
static int init_post_handshake_auth(SSL_CONNECTION *s, unsigned int context);
static int final_psk(SSL_CONNECTION *s, unsigned int context, int sent);
static int tls_init_compress_certificate(SSL_CONNECTION *sc, unsigned int context);
static EXT_RETURN tls_construct_compress_certificate(SSL_CONNECTION *sc, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx);
static int tls_parse_compress_certificate(SSL_CONNECTION *sc, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx);
static int tls_parse_ec_pt_formats(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx);

/* Structure to define a built-in extension */
typedef struct extensions_definition_st {
    /* The defined type for the extension */
    unsigned int type;
    /*
     * The context that this extension applies to, e.g. what messages and
     * protocol versions
     */
    unsigned int context;
    /*
     * exceptionally, we don't conditionally compile this field to avoid a
     * pile of ifndefs all over the ext_defs values
     */
    int ech_handling; /* how to handle ECH for this extension type */
    /*
     * Initialise extension before parsing. Always called for relevant contexts
     * even if extension not present
     */
    int (*init)(SSL_CONNECTION *s, unsigned int context);
    /* Parse extension sent from client to server */
    int (*parse_ctos)(SSL_CONNECTION *s, PACKET *pkt, unsigned int context,
        X509 *x, size_t chainidx);
    /* Parse extension send from server to client */
    int (*parse_stoc)(SSL_CONNECTION *s, PACKET *pkt, unsigned int context,
        X509 *x, size_t chainidx);
    /* Construct extension sent from server to client */
    EXT_RETURN (*construct_stoc)(SSL_CONNECTION *s, WPACKET *pkt,
        unsigned int context,
        X509 *x, size_t chainidx);
    /* Construct extension sent from client to server */
    EXT_RETURN (*construct_ctos)(SSL_CONNECTION *s, WPACKET *pkt,
        unsigned int context,
        X509 *x, size_t chainidx);
    /*
     * Finalise extension after parsing. Always called where an extensions was
     * initialised even if the extension was not present. |sent| is set to 1 if
     * the extension was seen, or 0 otherwise.
     */
    int (*final)(SSL_CONNECTION *s, unsigned int context, int sent);
} EXTENSION_DEFINITION;

/*
 * Definitions of all built-in extensions. NOTE: Changes in the number or order
 * of these extensions should be mirrored with equivalent changes to the
 * indexes ( TLSEXT_IDX_* ) defined in ssl_local.h.
 * Extensions should be added to test/ext_internal_test.c as well, as that
 * tests the ordering of the extensions.
 *
 * Each extension has an initialiser, a client and
 * server side parser and a finaliser. The initialiser is called (if the
 * extension is relevant to the given context) even if we did not see the
 * extension in the message that we received. The parser functions are only
 * called if we see the extension in the message. The finalisers are always
 * called if the initialiser was called.
 * There are also server and client side constructor functions which are always
 * called during message construction if the extension is relevant for the
 * given context.
 * The initialisation, parsing, finalisation and construction functions are
 * always called in the order defined in this list. Some extensions may depend
 * on others having been processed first, so the order of this list is
 * significant.
 * The extension context is defined by a series of flags which specify which
 * messages the extension is relevant to. These flags also specify whether the
 * extension is relevant to a particular protocol or protocol version.
 *
 * NOTE: WebSphere Application Server 7+ cannot handle empty extensions at
 * the end, keep these extensions before signature_algorithm.
 */
#define INVALID_EXTENSION { TLSEXT_TYPE_invalid, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }

static const EXTENSION_DEFINITION ext_defs[] = {
    { TLSEXT_TYPE_renegotiate,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_2_AND_BELOW_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        NULL, tls_parse_ctos_renegotiate, tls_parse_stoc_renegotiate,
        tls_construct_stoc_renegotiate, tls_construct_ctos_renegotiate,
        final_renegotiate },
    { TLSEXT_TYPE_server_name,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS,
        OSSL_ECH_HANDLING_CALL_BOTH,
        init_server_name,
        tls_parse_ctos_server_name, tls_parse_stoc_server_name,
        tls_construct_stoc_server_name, tls_construct_ctos_server_name,
        final_server_name },
    { TLSEXT_TYPE_max_fragment_length,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS,
        OSSL_ECH_HANDLING_COMPRESS,
        NULL, tls_parse_ctos_maxfragmentlen, tls_parse_stoc_maxfragmentlen,
        tls_construct_stoc_maxfragmentlen, tls_construct_ctos_maxfragmentlen,
        final_maxfragmentlen },
#ifndef OPENSSL_NO_SRP
    { TLSEXT_TYPE_srp,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_AND_BELOW_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        init_srp, tls_parse_ctos_srp, NULL, NULL, tls_construct_ctos_srp, NULL },
#else
    INVALID_EXTENSION,
#endif
    { TLSEXT_TYPE_ec_point_formats,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_2_AND_BELOW_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        init_ec_point_formats, tls_parse_ec_pt_formats, tls_parse_ec_pt_formats,
        tls_construct_stoc_ec_pt_formats, tls_construct_ctos_ec_pt_formats,
        NULL },
    { /*
       * "supported_groups" is spread across several specifications.
       * It was originally specified as "elliptic_curves" in RFC 4492,
       * and broadened to include named FFDH groups by RFC 7919.
       * Both RFCs 4492 and 7919 do not include a provision for the server
       * to indicate to the client the complete list of groups supported
       * by the server, with the server instead just indicating the
       * selected group for this connection in the ServerKeyExchange
       * message. (D)TLS 1.3 adds a scheme for the server to indicate
       * to the client its list of supported groups in the
       * EncryptedExtensions message, but none of the relevant
       * specifications permit sending supported_groups in the ServerHello.
       * Nonetheless (possibly due to the close proximity to the
       * "ec_point_formats" extension, which is allowed in the ServerHello),
       * there are several servers that send this extension in the
       * ServerHello anyway.  Up to and including the 1.1.0 release,
       * we did not check for the presence of nonpermitted extensions,
       * so to avoid a regression, we must permit this extension in the
       * (D)TLS 1.2 ServerHello as well.
       *
       * Note that there is no tls_parse_stoc_supported_groups function,
       * so we do not perform any additional parsing, validation, or
       * processing on the server's group list -- this is just a minimal
       * change to preserve compatibility with these misbehaving servers.
       */
        TLSEXT_TYPE_supported_groups,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS
            | SSL_EXT_TLS1_2_SERVER_HELLO,
        OSSL_ECH_HANDLING_COMPRESS,
        NULL, tls_parse_ctos_supported_groups, NULL,
        tls_construct_stoc_supported_groups,
        tls_construct_ctos_supported_groups, NULL },
    { TLSEXT_TYPE_session_ticket,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_2_AND_BELOW_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        init_session_ticket, tls_parse_ctos_session_ticket,
        tls_parse_stoc_session_ticket, tls_construct_stoc_session_ticket,
        tls_construct_ctos_session_ticket, NULL },
#ifndef OPENSSL_NO_OCSP
    { TLSEXT_TYPE_status_request,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_3_CERTIFICATE | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST,
        OSSL_ECH_HANDLING_COMPRESS,
        init_status_request, tls_parse_ctos_status_request,
        tls_parse_stoc_status_request, tls_construct_stoc_status_request,
        tls_construct_ctos_status_request, NULL },
#else
    INVALID_EXTENSION,
#endif
#ifndef OPENSSL_NO_NEXTPROTONEG
    { TLSEXT_TYPE_next_proto_neg,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_2_AND_BELOW_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        init_npn, tls_parse_ctos_npn, tls_parse_stoc_npn,
        tls_construct_stoc_next_proto_neg, tls_construct_ctos_npn, NULL },
#else
    INVALID_EXTENSION,
#endif
    { /*
       * Must appear in this list after server_name so that finalisation
       * happens after server_name callbacks
       */
        TLSEXT_TYPE_application_layer_protocol_negotiation,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS,
        OSSL_ECH_HANDLING_CALL_BOTH,
        init_alpn, tls_parse_ctos_alpn, tls_parse_stoc_alpn,
        tls_construct_stoc_alpn, tls_construct_ctos_alpn, final_alpn },
#ifndef OPENSSL_NO_SRTP
    { TLSEXT_TYPE_use_srtp,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS | SSL_EXT_DTLS_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        init_srtp, tls_parse_ctos_use_srtp, tls_parse_stoc_use_srtp,
        tls_construct_stoc_use_srtp, tls_construct_ctos_use_srtp, NULL },
#else
    INVALID_EXTENSION,
#endif
    { TLSEXT_TYPE_encrypt_then_mac,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_2_AND_BELOW_ONLY,
        /*
         * If you want to demonstrate/exercise duplicate, then
         * this does that and has no effect on sizes, but it
         * will break the quicapi test (see above). Probably
         * best done in local tests and not committed to any
         * upstream.
         * OSSL_ECH_HANDLING_DUPLICATE,
         */
        OSSL_ECH_HANDLING_COMPRESS,
        init_etm, tls_parse_ctos_etm, tls_parse_stoc_etm,
        tls_construct_stoc_etm, tls_construct_ctos_etm, NULL },
#ifndef OPENSSL_NO_CT
    { TLSEXT_TYPE_signed_certificate_timestamp,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_3_CERTIFICATE | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST,
        OSSL_ECH_HANDLING_COMPRESS,
        NULL,
        /*
         * No server side support for this, but can be provided by a custom
         * extension. This is an exception to the rule that custom extensions
         * cannot override built in ones.
         */
        NULL, tls_parse_stoc_sct, NULL, tls_construct_ctos_sct, NULL },
#else
    INVALID_EXTENSION,
#endif
    { TLSEXT_TYPE_extended_master_secret,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_2_AND_BELOW_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        init_ems, tls_parse_ctos_ems, tls_parse_stoc_ems,
        tls_construct_stoc_ems, tls_construct_ctos_ems, final_ems },
    { TLSEXT_TYPE_signature_algorithms_cert,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST,
        OSSL_ECH_HANDLING_COMPRESS,
        init_sig_algs_cert, tls_parse_ctos_sig_algs_cert,
        tls_parse_ctos_sig_algs_cert,
        /* We do not generate signature_algorithms_cert at present. */
        NULL, NULL, NULL },
    {
        TLSEXT_TYPE_post_handshake_auth,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        init_post_handshake_auth,
        tls_parse_ctos_post_handshake_auth,
        NULL,
        NULL,
        tls_construct_ctos_post_handshake_auth,
        NULL,
    },
    { TLSEXT_TYPE_client_cert_type,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS
            | SSL_EXT_TLS1_2_SERVER_HELLO,
        OSSL_ECH_HANDLING_CALL_BOTH,
        init_client_cert_type,
        tls_parse_ctos_client_cert_type, tls_parse_stoc_client_cert_type,
        tls_construct_stoc_client_cert_type, tls_construct_ctos_client_cert_type,
        NULL },
    { TLSEXT_TYPE_server_cert_type,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS
            | SSL_EXT_TLS1_2_SERVER_HELLO,
        OSSL_ECH_HANDLING_CALL_BOTH,
        init_server_cert_type,
        tls_parse_ctos_server_cert_type, tls_parse_stoc_server_cert_type,
        tls_construct_stoc_server_cert_type, tls_construct_ctos_server_cert_type,
        NULL },
    { TLSEXT_TYPE_signature_algorithms,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST,
        OSSL_ECH_HANDLING_COMPRESS,
        init_sig_algs, tls_parse_ctos_sig_algs,
        tls_parse_ctos_sig_algs, tls_construct_ctos_sig_algs,
        tls_construct_ctos_sig_algs, final_sig_algs },
    { TLSEXT_TYPE_supported_versions,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_SERVER_HELLO
            | SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST,
        OSSL_ECH_HANDLING_COMPRESS,
        NULL,
        /* Processed inline as part of version selection */
        NULL, tls_parse_stoc_supported_versions,
        tls_construct_stoc_supported_versions,
        tls_construct_ctos_supported_versions, final_supported_versions },
    { TLSEXT_TYPE_psk_kex_modes,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        init_psk_kex_modes, tls_parse_ctos_psk_kex_modes, NULL, NULL,
        tls_construct_ctos_psk_kex_modes, NULL },
    { /*
       * Must be in this list after supported_groups. We need that to have
       * been parsed before we do this one.
       */
        TLSEXT_TYPE_key_share,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_SERVER_HELLO
            | SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST
            | SSL_EXT_TLS1_3_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        NULL, tls_parse_ctos_key_share, tls_parse_stoc_key_share,
        tls_construct_stoc_key_share, tls_construct_ctos_key_share,
        final_key_share },
    { /* Must be after key_share */
        TLSEXT_TYPE_cookie,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST
            | SSL_EXT_TLS1_3_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        NULL, tls_parse_ctos_cookie, tls_parse_stoc_cookie,
        tls_construct_stoc_cookie, tls_construct_ctos_cookie, NULL },
    { /*
       * Special unsolicited ServerHello extension only used when
       * SSL_OP_CRYPTOPRO_TLSEXT_BUG is set. We allow it in a ClientHello but
       * ignore it.
       */
        TLSEXT_TYPE_cryptopro_bug,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO
            | SSL_EXT_TLS1_2_AND_BELOW_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        NULL, NULL, NULL, tls_construct_stoc_cryptopro_bug, NULL, NULL },
    { TLSEXT_TYPE_compress_certificate,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST
            | SSL_EXT_TLS1_3_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        tls_init_compress_certificate,
        tls_parse_compress_certificate, tls_parse_compress_certificate,
        tls_construct_compress_certificate, tls_construct_compress_certificate,
        NULL },
    { TLSEXT_TYPE_early_data,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS
            | SSL_EXT_TLS1_3_NEW_SESSION_TICKET | SSL_EXT_TLS1_3_ONLY,
        OSSL_ECH_HANDLING_CALL_BOTH,
        NULL, tls_parse_ctos_early_data, tls_parse_stoc_early_data,
        tls_construct_stoc_early_data, tls_construct_ctos_early_data,
        final_early_data },
    {
        TLSEXT_TYPE_certificate_authorities,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST
            | SSL_EXT_TLS1_3_ONLY,
        OSSL_ECH_HANDLING_COMPRESS,
        init_certificate_authorities,
        tls_parse_certificate_authorities,
        tls_parse_certificate_authorities,
        tls_construct_certificate_authorities,
        tls_construct_certificate_authorities,
        NULL,
    },
#ifndef OPENSSL_NO_ECH
    { TLSEXT_TYPE_ech,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ONLY | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS | SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST,
        OSSL_ECH_HANDLING_CALL_BOTH,
        init_ech,
        tls_parse_ctos_ech, tls_parse_stoc_ech,
        tls_construct_stoc_ech, tls_construct_ctos_ech,
        final_ech },
    { TLSEXT_TYPE_outer_extensions,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ONLY,
        OSSL_ECH_HANDLING_CALL_BOTH,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL },
#else /* OPENSSL_NO_ECH */
    INVALID_EXTENSION,
    INVALID_EXTENSION,
#endif /* END_OPENSSL_NO_ECH */
    { /* RFC 8701 GREASE extension 1 - type is dynamic */
        TLSEXT_TYPE_grease1,
        SSL_EXT_CLIENT_HELLO,
        0,
        NULL,
        NULL, NULL, NULL, tls_construct_ctos_grease1, NULL },
    { /* RFC 8701 GREASE extension 2 - type is dynamic */
        TLSEXT_TYPE_grease2,
        SSL_EXT_CLIENT_HELLO,
        0,
        NULL,
        NULL, NULL, NULL, tls_construct_ctos_grease2, NULL },
    { /* Required by the (D)TLSv1.3 spec to always be the last extension */
        TLSEXT_TYPE_psk,
        SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_SERVER_HELLO
            | SSL_EXT_TLS1_3_ONLY,
        OSSL_ECH_HANDLING_CALL_BOTH,
        NULL, tls_parse_ctos_psk, tls_parse_stoc_psk, tls_construct_stoc_psk,
        tls_construct_ctos_psk, final_psk }
};

#ifndef OPENSSL_NO_ECH
/*
 * Copy an inner extension value to outer.
 * inner CH must have been pre-decoded into s->clienthello->pre_proc_exts
 * already.
 */
int ossl_ech_copy_inner2outer(SSL_CONNECTION *s, uint16_t ext_type,
    int ind, WPACKET *pkt)
{
    RAW_EXTENSION *myext = NULL, *raws = NULL;

    if (s == NULL || s->clienthello == NULL)
        return OSSL_ECH_SAME_EXT_ERR;
    raws = s->clienthello->pre_proc_exts;
    if (raws == NULL)
        return OSSL_ECH_SAME_EXT_ERR;
    myext = &raws[ind];
    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out, "inner2outer: Copying ext type %d to outer\n",
            ext_type);
    }
    OSSL_TRACE_END(TLS);

    /*
     * copy inner value to outer
     */
    if (PACKET_data(&myext->data) != NULL
        && PACKET_remaining(&myext->data) > 0) {
        if (!WPACKET_put_bytes_u16(pkt, ext_type)
            || !WPACKET_sub_memcpy_u16(pkt, PACKET_data(&myext->data),
                PACKET_remaining(&myext->data)))
            return OSSL_ECH_SAME_EXT_ERR;
    } else {
        /* empty extension */
        if (!WPACKET_put_bytes_u16(pkt, ext_type)
            || !WPACKET_put_bytes_u16(pkt, 0))
            return OSSL_ECH_SAME_EXT_ERR;
    }
    return 1;
}

/*
 * DUPEMALL is useful for testing - this turns off compression and
 * causes two calls to each extension constructor, which'd be the same
 * as making all entries in ext_tab use the CALL_BOTH value
 */
#undef DUPEMALL

/*
 * Check if we're using the same/different key shares
 * return 1 if same key share in inner and outer, 0 otherwise
 */
int ossl_ech_same_key_share(void)
{
#ifdef DUPEMALL
    return 0;
#endif
    return ext_defs[TLSEXT_IDX_key_share].ech_handling
        != OSSL_ECH_HANDLING_CALL_BOTH;
}

/*
 * say if extension at index |ind| in ext_defs is to be ECH compressed
 * return 1 if this one is to be compressed, 0 if not, -1 for error
 */
int ossl_ech_2bcompressed(size_t ind)
{
    const size_t nexts = OSSL_NELEM(ext_defs);

#ifdef DUPEMALL
    return 0;
#endif
    if (ind >= nexts)
        return -1;
    return ext_defs[ind].ech_handling == OSSL_ECH_HANDLING_COMPRESS;
}

/* as needed, repeat extension from inner in outer handling compression */
int ossl_ech_same_ext(SSL_CONNECTION *s, WPACKET *pkt)
{
    unsigned int type = 0;
    int tind = 0, nexts = OSSL_NELEM(ext_defs);

#ifdef DUPEMALL
    return OSSL_ECH_SAME_EXT_CONTINUE;
#endif
    if (s == NULL || s->ext.ech.es == NULL)
        return OSSL_ECH_SAME_EXT_CONTINUE; /* nothing to do */
    /*
     * We store/access the index of the extension handler in
     * s->ext.ech.ext_ind, as we'd otherwise not know it here.
     * Be nice were there a better way to handle that.
     */
    tind = s->ext.ech.ext_ind;
    /* If this index'd extension won't be compressed, we're done */
    if (tind < 0 || tind >= nexts)
        return OSSL_ECH_SAME_EXT_ERR;
    type = ext_defs[tind].type;
    if (s->ext.ech.ch_depth == 1) {
        /* inner CH - just note compression as configured */
        if (ext_defs[tind].ech_handling != OSSL_ECH_HANDLING_COMPRESS)
            return OSSL_ECH_SAME_EXT_CONTINUE;
        /* mark this one to be "compressed" */
        if (s->ext.ech.n_outer_only >= OSSL_ECH_OUTERS_MAX)
            return OSSL_ECH_SAME_EXT_ERR;
        s->ext.ech.outer_only[s->ext.ech.n_outer_only] = type;
        s->ext.ech.n_outer_only++;
        OSSL_TRACE_BEGIN(TLS)
        {
            BIO_printf(trc_out, "ech_same_ext: Marking (type %u, ind %d "
                                "tot-comp %d) for compression\n",
                type, tind,
                (int)s->ext.ech.n_outer_only);
        }
        OSSL_TRACE_END(TLS);
        return OSSL_ECH_SAME_EXT_CONTINUE;
    } else {
        /* Copy value from inner to outer, or indicate a new value needed */
        if (s->clienthello == NULL || pkt == NULL)
            return OSSL_ECH_SAME_EXT_ERR;
        if (ext_defs[tind].ech_handling == OSSL_ECH_HANDLING_CALL_BOTH)
            return OSSL_ECH_SAME_EXT_CONTINUE;
        else
            return ossl_ech_copy_inner2outer(s, type, tind, pkt);
    }
    /* just in case - shouldn't happen */
    return OSSL_ECH_SAME_EXT_ERR;
}
#endif

/* Returns a TLSEXT_TYPE for the given index */
unsigned int ossl_get_extension_type(size_t idx)
{
    size_t num_exts = OSSL_NELEM(ext_defs);

    if (idx >= num_exts)
        return TLSEXT_TYPE_out_of_range;

    return ext_defs[idx].type;
}

/* Check whether an extension's context matches the current context */
static int validate_context(SSL_CONNECTION *s, unsigned int extctx,
    unsigned int thisctx)
{
    /* Check we're allowed to use this extension in this context */
    if ((thisctx & extctx) == 0)
        return 0;

    if (SSL_CONNECTION_IS_DTLS(s)) {
        if ((extctx & SSL_EXT_TLS_ONLY) != 0)
            return 0;
    } else if ((extctx & SSL_EXT_DTLS_ONLY) != 0) {
        return 0;
    }

    return 1;
}

int tls_validate_all_contexts(SSL_CONNECTION *s, unsigned int thisctx,
    RAW_EXTENSION *exts)
{
    size_t i, num_exts, builtin_num = OSSL_NELEM(ext_defs), offset;
    RAW_EXTENSION *thisext;
    unsigned int context;
    ENDPOINT role = ENDPOINT_BOTH;

    if ((thisctx & SSL_EXT_CLIENT_HELLO) != 0)
        role = ENDPOINT_SERVER;
    else if ((thisctx & SSL_EXT_TLS1_2_SERVER_HELLO) != 0)
        role = ENDPOINT_CLIENT;

    /* Calculate the number of extensions in the extensions list */
    num_exts = builtin_num + s->cert->custext.meths_count;

    for (thisext = exts, i = 0; i < num_exts; i++, thisext++) {
        if (!thisext->present)
            continue;

        if (i < builtin_num) {
            context = ext_defs[i].context;
        } else {
            custom_ext_method *meth = NULL;

            meth = custom_ext_find(&s->cert->custext, role, thisext->type,
                &offset);
            if (!ossl_assert(meth != NULL))
                return 0;
            context = meth->context;
        }

        if (!validate_context(s, context, thisctx))
            return 0;
    }

    return 1;
}

/*
 * Verify whether we are allowed to use the extension |type| in the current
 * |context|. Returns 1 to indicate the extension is allowed or unknown or 0 to
 * indicate the extension is not allowed. If returning 1 then |*found| is set to
 * the definition for the extension we found.
 */
static int verify_extension(SSL_CONNECTION *s, unsigned int context,
    unsigned int type, custom_ext_methods *meths,
    RAW_EXTENSION *rawexlist, RAW_EXTENSION **found)
{
    size_t i;
    size_t builtin_num = OSSL_NELEM(ext_defs);
    const EXTENSION_DEFINITION *thisext;

    for (i = 0, thisext = ext_defs; i < builtin_num; i++, thisext++) {
        if (type == thisext->type) {
            if (!validate_context(s, thisext->context, context))
                return 0;

            *found = &rawexlist[i];
            return 1;
        }
    }

    /* Check the custom extensions */
    if (meths != NULL) {
        size_t offset = 0;
        ENDPOINT role = ENDPOINT_BOTH;
        custom_ext_method *meth = NULL;

        if ((context & SSL_EXT_CLIENT_HELLO) != 0) {
#ifndef OPENSSL_NO_ECH
            if (s->ext.ech.attempted == 1 && s->ext.ech.ch_depth == 1)
                role = ENDPOINT_CLIENT;
            else
                role = ENDPOINT_SERVER;
#else
            role = ENDPOINT_SERVER;
#endif
        } else if ((context & SSL_EXT_TLS1_2_SERVER_HELLO) != 0)
            role = ENDPOINT_CLIENT;

        meth = custom_ext_find(meths, role, type, &offset);
        if (meth != NULL) {
            if (!validate_context(s, meth->context, context))
                return 0;
            *found = &rawexlist[offset + builtin_num];
            return 1;
        }
    }

    /* Unknown extension. We allow it */
    *found = NULL;
    return 1;
}

/*
 * Parse an ec_point_formats extension off the wire (one function for
 * both sides).  The peer's list is retained verbatim for
 * SSL_get0_ec_point_formats() and for the RFC 4492/8422 section 5.1.2
 * 'uncompressed must be present' check.
 *
 * The check is gated on the negotiated ciphersuite -- a TLS 1.3
 * handshake or a non-ECC TLS 1.2 ciphersuite makes the extension moot
 * and any missing 'uncompressed' codepoint is ignored.  On the client
 * the chosen ciphersuite is already locked in by the time we parse
 * ServerHello, so the check happens inline here.  On the server it's
 * deferred to tls_construct_stoc_ec_pt_formats(), the first point at
 * which s->s3.tmp.new_cipher is set for TLS 1.2.
 */
static int tls_parse_ec_pt_formats(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context, X509 *x, size_t chainidx)
{
    PACKET list;

    if (!PACKET_as_length_prefixed_1(pkt, &list)
        || PACKET_remaining(&list) == 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    if (!s->hit
        && !PACKET_memdup(&list, &s->ext.peer_ecpointformats,
            &s->ext.peer_ecpointformats_len)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (!s->server) {
        unsigned long alg_k = s->s3.tmp.new_cipher->algorithm_mkey;
        unsigned long alg_a = s->s3.tmp.new_cipher->algorithm_auth;

        if (((alg_k & SSL_kECDHE) || (alg_a & SSL_aECDSA))
            && memchr(PACKET_data(&list),
                   TLSEXT_ECPOINTFORMAT_uncompressed,
                   PACKET_remaining(&list))
                == NULL) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER,
                SSL_R_TLS_INVALID_ECPOINTFORMAT_LIST);
            return 0;
        }
    }

    return 1;
}

/*
 * Verify that all extensions in |packet| are known built-in or custom
 * extension types. This is used for TLS 1.3 server extension responses where
 * unknown extensions are not ignored.
 */
int tls_validate_no_unknown_extensions(SSL_CONNECTION *s, PACKET *packet,
    unsigned int context)
{
    PACKET extensions = *packet;
    custom_ext_methods *exts = &s->cert->custext;
    ENDPOINT role = ENDPOINT_BOTH;

    if ((context & SSL_EXT_CLIENT_HELLO) != 0) {
#ifndef OPENSSL_NO_ECH
        if (s->ext.ech.attempted == 1 && s->ext.ech.ch_depth == 1)
            role = ENDPOINT_CLIENT;
        else
            role = ENDPOINT_SERVER;
#else
        role = ENDPOINT_SERVER;
#endif
    } else if ((context & SSL_EXT_TLS1_2_SERVER_HELLO) != 0) {
        role = ENDPOINT_CLIENT;
    }

    while (PACKET_remaining(&extensions) > 0) {
        unsigned int type;
        size_t i;
        PACKET extension;
        const EXTENSION_DEFINITION *thisext;

        if (!PACKET_get_net_2(&extensions, &type)
            || !PACKET_get_length_prefixed_2(&extensions, &extension)) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            return 0;
        }

        for (i = 0, thisext = ext_defs; i < OSSL_NELEM(ext_defs);
            i++, thisext++) {
            if (type == thisext->type)
                break;
        }
        if (i < OSSL_NELEM(ext_defs))
            continue;

        if (exts != NULL && custom_ext_find(exts, role, type, NULL) != NULL)
            continue;

        SSLfatal(s, SSL_AD_UNSUPPORTED_EXTENSION,
            SSL_R_UNSOLICITED_EXTENSION);
        return 0;
    }

    return 1;
}

/*
 * Check whether the context defined for an extension |extctx| means whether
 * the extension is relevant for the current context |thisctx| or not. Returns
 * 1 if the extension is relevant for this context, and 0 otherwise
 */
int extension_is_relevant(SSL_CONNECTION *s, unsigned int extctx,
    unsigned int thisctx)
{
    int is_version13;

    /*
     * For HRR we haven't selected the version yet but we know it will be
     * (D)TLSv1.3
     */
    if ((thisctx & SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST) != 0)
        is_version13 = 1;
    else
        is_version13 = SSL_CONNECTION_IS_VERSION13(s);

    if ((SSL_CONNECTION_IS_DTLS(s)
            && (extctx & SSL_EXT_TLS_IMPLEMENTATION_ONLY) != 0)
        /*
         * Note that is_version13 means "(D)TLS 1.3 has been negotiated",
         * which is never true when generating the ClientHello.
         * However, version negotiation *has* occurred by the time the
         * ClientHello extensions are being parsed.
         * Be careful to allow (D)TLS 1.3-only extensions when generating
         * the ClientHello.
         */
        || (is_version13 && (extctx & SSL_EXT_TLS1_2_AND_BELOW_ONLY) != 0)
        || (!is_version13 && (extctx & SSL_EXT_TLS1_3_ONLY) != 0
            && (thisctx & SSL_EXT_CLIENT_HELLO) == 0)
        || (s->server && !is_version13 && (extctx & SSL_EXT_TLS1_3_ONLY) != 0)
        || (s->hit && (extctx & SSL_EXT_IGNORE_ON_RESUMPTION) != 0))
        return 0;
    return 1;
}

/*
 * Gather a list of all the extensions from the data in |packet]. |context|
 * tells us which message this extension is for. The raw extension data is
 * stored in |*res| on success. We don't actually process the content of the
 * extensions yet, except to check their types. This function also runs the
 * initialiser functions for all known extensions if |init| is nonzero (whether
 * we have collected them or not). If successful the caller is responsible for
 * freeing the contents of |*res|.
 *
 * Per http://tools.ietf.org/html/rfc5246#section-7.4.1.4, there may not be
 * more than one extension of the same type in a ClientHello or ServerHello.
 * This function returns 1 if all extensions are unique and we have parsed their
 * types, and 0 if the extensions contain duplicates, could not be successfully
 * found, or an internal error occurred. We only check duplicates for
 * extensions that we know about. We ignore others.
 */
int tls_collect_extensions(SSL_CONNECTION *s, PACKET *packet,
    unsigned int context,
    RAW_EXTENSION **res, size_t *len, int init)
{
    PACKET extensions = *packet;
    size_t i = 0;
    size_t num_exts;
    custom_ext_methods *exts = &s->cert->custext;
    RAW_EXTENSION *raw_extensions = NULL;
    const EXTENSION_DEFINITION *thisexd;

    *res = NULL;

    /*
     * Initialise server side custom extensions. Client side is done during
     * construction of extensions for the ClientHello.
     */
#ifndef OPENSSL_NO_ECH
    if ((context & SSL_EXT_CLIENT_HELLO) != 0 && s->ext.ech.attempted == 0)
        custom_ext_init(&s->cert->custext);
#else
    if ((context & SSL_EXT_CLIENT_HELLO) != 0)
        custom_ext_init(&s->cert->custext);
#endif

    num_exts = OSSL_NELEM(ext_defs) + (exts != NULL ? exts->meths_count : 0);
    raw_extensions = OPENSSL_calloc(num_exts, sizeof(*raw_extensions));
    if (raw_extensions == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_CRYPTO_LIB);
        return 0;
    }

    i = 0;
    while (PACKET_remaining(&extensions) > 0) {
        unsigned int type, idx;
        PACKET extension;
        RAW_EXTENSION *thisex;

        if (!PACKET_get_net_2(&extensions, &type) || !PACKET_get_length_prefixed_2(&extensions, &extension)) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            goto err;
        }
        /*
         * Verify this extension is allowed. We only check duplicates for
         * extensions that we recognise. We also have a special case for the
         * PSK extension, which must be the last one in the ClientHello.
         */
        if (!verify_extension(s, context, type, exts, raw_extensions, &thisex)
            || (thisex != NULL && thisex->present == 1)
            || (type == TLSEXT_TYPE_psk
                && (context & SSL_EXT_CLIENT_HELLO) != 0
                && PACKET_remaining(&extensions) != 0)) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
            goto err;
        }
        /* The server must tolerate the unknown extension and complete. */
        if (thisex == NULL)
            continue;

        idx = (unsigned int)(thisex - raw_extensions);
        /*-
         * Check that we requested this extension (if appropriate). Requests can
         * be sent in the ClientHello and CertificateRequest. Unsolicited
         * extensions can be sent in the NewSessionTicket. We only do this for
         * the built-in extensions. Custom extensions have a different but
         * similar check elsewhere.
         * Special cases:
         * - The HRR cookie extension is unsolicited
         * - The renegotiate extension is unsolicited (the client signals
         *   support via an SCSV)
         * - The signed_certificate_timestamp extension can be provided by a
         * custom extension or by the built-in version. We let the extension
         * itself handle unsolicited response checks.
         */
        if (idx < OSSL_NELEM(ext_defs)
            && (context & (SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST | SSL_EXT_TLS1_3_NEW_SESSION_TICKET)) == 0
            && type != TLSEXT_TYPE_cookie
            && type != TLSEXT_TYPE_renegotiate
            && type != TLSEXT_TYPE_signed_certificate_timestamp
            && (s->ext.extflags[idx] & SSL_EXT_FLAG_SENT) == 0
#ifndef OPENSSL_NO_GOST
            && !((context & SSL_EXT_TLS1_2_SERVER_HELLO) != 0
                && type == TLSEXT_TYPE_cryptopro_bug)
#endif
        ) {
            SSLfatal(s, SSL_AD_UNSUPPORTED_EXTENSION,
                SSL_R_UNSOLICITED_EXTENSION);
            goto err;
        }
        thisex->data = extension;
        thisex->present = 1;
        thisex->type = type;
        thisex->received_order = i++;
        if (s->ext.debug_cb)
            s->ext.debug_cb(SSL_CONNECTION_GET_USER_SSL(s), !s->server,
                thisex->type, PACKET_data(&thisex->data),
                (int)PACKET_remaining(&thisex->data),
                s->ext.debug_arg);
    }

    if (init) {
        /*
         * Initialise all known extensions relevant to this context,
         * whether we have found them or not
         */
        for (thisexd = ext_defs, i = 0; i < OSSL_NELEM(ext_defs);
            i++, thisexd++) {
            if (thisexd->init != NULL && (thisexd->context & context) != 0
                && extension_is_relevant(s, thisexd->context, context)
                && !thisexd->init(s, context)) {
                /* SSLfatal() already called */
                goto err;
            }
        }
    }

    *res = raw_extensions;
    if (len != NULL)
        *len = num_exts;
    return 1;

err:
    OPENSSL_free(raw_extensions);
    return 0;
}

/*
 * Runs the parser for a given extension with index |idx|. |exts| contains the
 * list of all parsed extensions previously collected by
 * tls_collect_extensions(). The parser is only run if it is applicable for the
 * given |context| and the parser has not already been run. If this is for a
 * Certificate message, then we also provide the parser with the relevant
 * Certificate |x| and its position in the |chainidx| with 0 being the first
 * Certificate. Returns 1 on success or 0 on failure. If an extension is not
 * present this counted as success.
 */
int tls_parse_extension(SSL_CONNECTION *s, TLSEXT_INDEX idx, int context,
    RAW_EXTENSION *exts, X509 *x, size_t chainidx)
{
    RAW_EXTENSION *currext = &exts[idx];
    int (*parser)(SSL_CONNECTION *s, PACKET *pkt, unsigned int context, X509 *x,
        size_t chainidx)
        = NULL;

    /* Skip if the extension is not present */
    if (!currext->present)
        return 1;

    /* Skip if we've already parsed this extension */
    if (currext->parsed)
        return 1;

    currext->parsed = 1;

    if (idx < OSSL_NELEM(ext_defs)) {
        /* We are handling a built-in extension */
        const EXTENSION_DEFINITION *extdef = &ext_defs[idx];

        /* Check if extension is defined for our protocol. If not, skip */
        if (!extension_is_relevant(s, extdef->context, context))
            return 1;

        parser = s->server ? extdef->parse_ctos : extdef->parse_stoc;

        if (parser != NULL)
            return parser(s, &currext->data, context, x, chainidx);

        /*
         * If the parser is NULL we fall through to the custom extension
         * processing
         */
    }

    /* Parse custom extensions */
    return custom_ext_parse(s, context, currext->type,
        PACKET_data(&currext->data),
        PACKET_remaining(&currext->data),
        x, chainidx);
}

/*
 * Parse all remaining extensions that have not yet been parsed. Also calls the
 * finalisation for all extensions at the end if |fin| is nonzero, whether we
 * collected them or not. Returns 1 for success or 0 for failure. If we are
 * working on a Certificate message then we also pass the Certificate |x| and
 * its position in the |chainidx|, with 0 being the first certificate.
 */
int tls_parse_all_extensions(SSL_CONNECTION *s, int context,
    RAW_EXTENSION *exts, X509 *x,
    size_t chainidx, int fin)
{
    size_t i, numexts = OSSL_NELEM(ext_defs);
    const EXTENSION_DEFINITION *thisexd;

    /* Calculate the number of extensions in the extensions list */
    numexts += s->cert->custext.meths_count;

    /* Parse each extension in turn */
    for (i = 0; i < numexts; i++) {
        if (!tls_parse_extension(s, i, context, exts, x, chainidx)) {
            /* SSLfatal() already called */
            return 0;
        }
    }

    if (fin) {
        /*
         * Finalise all known extensions relevant to this context,
         * whether we have found them or not
         */
        for (i = 0, thisexd = ext_defs; i < OSSL_NELEM(ext_defs);
            i++, thisexd++) {
            if (thisexd->final != NULL && (thisexd->context & context) != 0
                && !thisexd->final(s, context, exts[i].present)) {
                /* SSLfatal() already called */
                return 0;
            }
        }
    }

    return 1;
}

int should_add_extension(SSL_CONNECTION *s, unsigned int extctx,
    unsigned int thisctx, int max_version)
{
    const int version1_3 = SSL_CONNECTION_IS_DTLS(s) ? DTLS1_3_VERSION : TLS1_3_VERSION;

    /* Skip if not relevant for our context */
    if ((extctx & thisctx) == 0)
        return 0;

    /* Check if this extension is defined for our protocol. If not, skip */
    if (!extension_is_relevant(s, extctx, thisctx)
        || ((extctx & SSL_EXT_TLS1_3_ONLY) != 0
            && (thisctx & SSL_EXT_CLIENT_HELLO) != 0
            && ssl_version_cmp(s, max_version, version1_3) < 0))
        return 0;

    return 1;
}

/*
 * Construct all the extensions relevant to the current |context| and write
 * them to |pkt|. If this is an extension for a Certificate in a Certificate
 * message, then |x| will be set to the Certificate we are handling, and
 * |chainidx| will indicate the position in the chainidx we are processing (with
 * 0 being the first in the chain). Returns 1 on success or 0 on failure. On a
 * failure construction stops at the first extension to fail to construct.
 */
int tls_construct_extensions(SSL_CONNECTION *s, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
    size_t i;
    int min_version, max_version = 0, reason;
    const EXTENSION_DEFINITION *thisexd;
    int for_comp = (context & SSL_EXT_TLS1_3_CERTIFICATE_COMPRESSION) != 0;
#ifndef OPENSSL_NO_ECH
    int pass;
#endif

    if (!WPACKET_start_sub_packet_u16(pkt)
        /*
         * If extensions are of zero length then we don't even add the
         * extensions length bytes to a ClientHello/ServerHello
         * (for non-(D)TLSv1.3).
         */
        || ((context & (SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO)) != 0
            && !WPACKET_set_flags(pkt,
                WPACKET_FLAGS_ABANDON_ON_ZERO_LENGTH))) {
        if (!for_comp)
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if ((context & SSL_EXT_CLIENT_HELLO) != 0) {
        reason = ssl_get_min_max_version(s, &min_version, &max_version, NULL);
        if (reason != 0) {
            if (!for_comp)
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, reason);
            return 0;
        }
    }

    /* Add custom extensions first */
#ifndef OPENSSL_NO_ECH
    if ((context & SSL_EXT_CLIENT_HELLO) != 0 && s->ext.ech.attempted == 0)
        /* On the server side with initialise during ClientHello parsing */
        custom_ext_init(&s->cert->custext);
#else
    if ((context & SSL_EXT_CLIENT_HELLO) != 0)
        /* On the server side with initialise during ClientHello parsing */
        custom_ext_init(&s->cert->custext);
#endif
    if (!custom_ext_add(s, context, pkt, x, chainidx, max_version)) {
        /* SSLfatal() already called */
        return 0;
    }

#ifndef OPENSSL_NO_ECH
    /*
     * Two passes if doing real ECH - we first construct the
     * to-be-ECH-compressed extensions, and then go around again
     * constructing those that aren't to be ECH-compressed. We
     * need to ensure this ordering so that all the ECH-compressed
     * extensions are contiguous in the encoding. The actual
     * compression happens later in ech_encode_inner().
     */
    for (pass = 0; pass <= 1; pass++)
#endif

        for (i = 0, thisexd = ext_defs; i < OSSL_NELEM(ext_defs);
            i++, thisexd++) {
            EXT_RETURN (*construct)(SSL_CONNECTION *s, WPACKET *pkt,
                unsigned int context,
                X509 *x, size_t chainidx);
            EXT_RETURN ret;

#ifndef OPENSSL_NO_ECH
            /* do compressed in pass 0, non-compressed in pass 1 */
            if (ossl_ech_2bcompressed((int)i) == pass)
                continue;
            /* stash index - needed for COMPRESS ECH handling */
            s->ext.ech.ext_ind = (int)i;
#endif
            /* Skip if not relevant for our context */
            if (!should_add_extension(s, thisexd->context, context, max_version))
                continue;

            construct = s->server ? thisexd->construct_stoc
                                  : thisexd->construct_ctos;

            if (construct == NULL)
                continue;

            ret = construct(s, pkt, context, x, chainidx);
            if (ret == EXT_RETURN_FAIL) {
                /* SSLfatal() already called */
                return 0;
            }
            if (ret == EXT_RETURN_SENT
                && (context & (SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST | SSL_EXT_TLS1_3_NEW_SESSION_TICKET)) != 0)
                s->ext.extflags[i] |= SSL_EXT_FLAG_SENT;
        }

#ifndef OPENSSL_NO_ECH
    /*
     * don't close yet if client in the middle of doing ECH, we'll
     * eventually close this in ech_aad_and_encrypt() after we add
     * the real ECH extension value
     */
    if (s->server
        || context != SSL_EXT_CLIENT_HELLO
        || s->ext.ech.attempted == 0
        || s->ext.ech.ch_depth == 1
        || s->ext.ech.grease == OSSL_ECH_IS_GREASE) {
        if (!WPACKET_close(pkt)) {
            if (!for_comp)
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }
#else
    if (!WPACKET_close(pkt)) {
        if (!for_comp)
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
#endif
    return 1;
}

/*
 * Built in extension finalisation and initialisation functions. All initialise
 * or finalise the associated extension type for the given |context|. For
 * finalisers |sent| is set to 1 if we saw the extension during parsing, and 0
 * otherwise. These functions return 1 on success or 0 on failure.
 */

static int final_renegotiate(SSL_CONNECTION *s, unsigned int context, int sent)
{
    if (!s->server) {
        /*
         * Check if we can connect to a server that doesn't support safe
         * renegotiation
         */
        if (!(s->options & SSL_OP_LEGACY_SERVER_CONNECT)
            && !(s->options & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION)
            && !sent) {
            SSLfatal(s, SSL_AD_HANDSHAKE_FAILURE,
                SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED);
            return 0;
        }

        return 1;
    }

    /* Need RI if renegotiating */
    if (s->renegotiate
        && !(s->options & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION)
        && !sent) {
        SSLfatal(s, SSL_AD_HANDSHAKE_FAILURE,
            SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED);
        return 0;
    }

    return 1;
}

static ossl_inline void ssl_tsan_decr(const SSL_CTX *ctx,
    TSAN_QUALIFIER int *stat)
{
    if (ssl_tsan_lock(ctx)) {
        tsan_decr(stat);
        ssl_tsan_unlock(ctx);
    }
}

static int init_server_name(SSL_CONNECTION *s, unsigned int context)
{
    if (s->server) {
        s->servername_done = 0;

        OPENSSL_free(s->ext.hostname);
        s->ext.hostname = NULL;
    }

    return 1;
}

#ifndef OPENSSL_NO_ECH
/*
 * Just note that ech is not yet done
 * return 1 for good, 0 otherwise
 */
static int init_ech(SSL_CONNECTION *s, unsigned int context)
{
    const int nexts = OSSL_NELEM(ext_defs);

    /* we don't need this assert everywhere - anywhere is fine */
    if (!ossl_assert(TLSEXT_IDX_num_builtins == nexts)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    if ((context & SSL_EXT_CLIENT_HELLO) != 0)
        s->ext.ech.done = 0;
    return 1;
}

static int final_ech(SSL_CONNECTION *s, unsigned int context, int sent)
{
    if (s->server && s->ext.ech.success == 1
        && s->ext.ech.inner_ech_seen_ok != 1) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_ECH_REQUIRED);
        return 0;
    }
    return 1;
}
#endif /* OPENSSL_NO_ECH */

static int final_server_name(SSL_CONNECTION *s, unsigned int context, int sent)
{
    int ret = SSL_TLSEXT_ERR_NOACK;
    int altmp = SSL_AD_UNRECOGNIZED_NAME;
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);
    SSL *ussl = SSL_CONNECTION_GET_USER_SSL(s);
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);
    int was_ticket = (SSL_get_options(ssl) & SSL_OP_NO_TICKET) == 0;

    if (!ossl_assert(sctx != NULL) || !ossl_assert(s->session_ctx != NULL)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (sctx->ext.servername_cb != NULL)
        ret = sctx->ext.servername_cb(ussl, &altmp,
            sctx->ext.servername_arg);
    else if (s->session_ctx->ext.servername_cb != NULL)
        ret = s->session_ctx->ext.servername_cb(ussl, &altmp,
            s->session_ctx->ext.servername_arg);

    /*
     * For servers, propagate the SNI hostname from the temporary
     * storage in the SSL to the persistent SSL_SESSION, now that we
     * know we accepted it.
     * Clients make this copy when parsing the server's response to
     * the extension, which is when they find out that the negotiation
     * was successful.
     */
    if (s->server) {
        if (sent && ret == SSL_TLSEXT_ERR_OK && !s->hit) {
            /* Only store the hostname in the session if we accepted it. */
            OPENSSL_free(s->session->ext.hostname);
            s->session->ext.hostname = OPENSSL_strdup(s->ext.hostname);
            if (s->session->ext.hostname == NULL && s->ext.hostname != NULL) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            }
        }
    }

    /*
     * If we switched contexts (whether here or in the client_hello callback),
     * move the sess_accept increment from the session_ctx to the new
     * context, to avoid the confusing situation of having sess_accept_good
     * exceed sess_accept (zero) for the new context.
     */
    if (SSL_IS_FIRST_HANDSHAKE(s) && sctx != s->session_ctx
        && s->hello_retry_request == SSL_HRR_NONE) {
        ssl_tsan_counter(sctx, &sctx->stats.sess_accept);
        ssl_tsan_decr(s->session_ctx, &s->session_ctx->stats.sess_accept);
    }

    /*
     * If we're expecting to send a ticket, and tickets were previously enabled,
     * and now tickets are disabled, then turn off expected ticket.
     * Also, if this is not a resumption, create a new session ID
     */
    if (ret == SSL_TLSEXT_ERR_OK && s->ext.ticket_expected
        && was_ticket && (SSL_get_options(ssl) & SSL_OP_NO_TICKET) != 0) {
        s->ext.ticket_expected = 0;
        if (!s->hit) {
            SSL_SESSION *ss = SSL_get_session(ssl);

            if (ss != NULL) {
                OPENSSL_free(ss->ext.tick);
                ss->ext.tick = NULL;
                ss->ext.ticklen = 0;
                ss->ext.tick_lifetime_hint = 0;
                ss->ext.tick_age_add = 0;
                if (!ssl_generate_session_id(s, ss)) {
                    SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    return 0;
                }
            } else {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }
    }

    switch (ret) {
    case SSL_TLSEXT_ERR_ALERT_FATAL:
        SSLfatal(s, altmp, SSL_R_CALLBACK_FAILED);
        return 0;

    case SSL_TLSEXT_ERR_ALERT_WARNING:
        /* (D)TLSv1.3 doesn't have warning alerts so we suppress this */
        if (!SSL_CONNECTION_IS_VERSION13(s))
            ssl3_send_alert(s, SSL3_AL_WARNING, altmp);
        s->servername_done = 0;
        return 1;

    case SSL_TLSEXT_ERR_NOACK:
        s->servername_done = 0;
        return 1;

    default:
        return 1;
    }
}

static int init_session_ticket(SSL_CONNECTION *s, unsigned int context)
{
    if (!s->server)
        s->ext.ticket_expected = 0;

    return 1;
}

#ifndef OPENSSL_NO_OCSP
static int init_status_request(SSL_CONNECTION *s, unsigned int context)
{
    if (s->server) {
        s->ext.status_type = TLSEXT_STATUSTYPE_nothing;
    } else {
        /*
         * Ensure we get sensible values passed to tlsext_status_cb in the event
         * that we don't receive a status message
         */
        OPENSSL_free(s->ext.ocsp.resp);
        s->ext.ocsp.resp = NULL;
        s->ext.ocsp.resp_len = 0;

        sk_OCSP_RESPONSE_pop_free(s->ext.ocsp.resp_ex, OCSP_RESPONSE_free);
        s->ext.ocsp.resp_ex = NULL;
    }

    return 1;
}
#endif

#ifndef OPENSSL_NO_NEXTPROTONEG
static int init_npn(SSL_CONNECTION *s, unsigned int context)
{
    s->s3.npn_seen = 0;

    return 1;
}
#endif

static int init_alpn(SSL_CONNECTION *s, unsigned int context)
{
    OPENSSL_free(s->s3.alpn_selected);
    s->s3.alpn_selected = NULL;
    s->s3.alpn_selected_len = 0;
    if (s->server) {
        OPENSSL_free(s->s3.alpn_proposed);
        s->s3.alpn_proposed = NULL;
        s->s3.alpn_proposed_len = 0;
    }
    return 1;
}

static int final_alpn(SSL_CONNECTION *s, unsigned int context, int sent)
{
    if (!s->server && !sent && s->session->ext.alpn_selected != NULL)
        s->ext.early_data_ok = 0;

    if (!s->server || !SSL_CONNECTION_IS_VERSION13(s))
        return 1;

    /*
     * Call alpn_select callback if needed.  Has to be done after SNI and
     * cipher negotiation (HTTP/2 restricts permitted ciphers). In (D)TLSv1.3
     * we also have to do this before we decide whether to accept early_data.
     * In (D)TLSv1.3 we've already negotiated our cipher so we do this call now.
     * For < (D)TLSv1.3 we defer it until after cipher negotiation.
     *
     * On failure SSLfatal() already called.
     */
    return tls_handle_alpn(s);
}

static int init_sig_algs(SSL_CONNECTION *s, unsigned int context)
{
    /* Clear any signature algorithms extension received */
    OPENSSL_free(s->s3.tmp.peer_sigalgs);
    s->s3.tmp.peer_sigalgs = NULL;
    s->s3.tmp.peer_sigalgslen = 0;

    return 1;
}

static int init_sig_algs_cert(SSL_CONNECTION *s,
    ossl_unused unsigned int context)
{
    /* Clear any signature algorithms extension received */
    OPENSSL_free(s->s3.tmp.peer_cert_sigalgs);
    s->s3.tmp.peer_cert_sigalgs = NULL;
    s->s3.tmp.peer_cert_sigalgslen = 0;

    return 1;
}

#ifndef OPENSSL_NO_SRP
static int init_srp(SSL_CONNECTION *s, unsigned int context)
{
    OPENSSL_free(s->srp_ctx.login);
    s->srp_ctx.login = NULL;

    return 1;
}
#endif

static int init_ec_point_formats(SSL_CONNECTION *s, unsigned int context)
{
    OPENSSL_free(s->ext.peer_ecpointformats);
    s->ext.peer_ecpointformats = NULL;
    s->ext.peer_ecpointformats_len = 0;

    return 1;
}

static int init_etm(SSL_CONNECTION *s, unsigned int context)
{
    s->ext.use_etm = 0;

    return 1;
}

static int init_ems(SSL_CONNECTION *s, unsigned int context)
{
    if (s->s3.flags & TLS1_FLAGS_RECEIVED_EXTMS) {
        s->s3.flags &= ~TLS1_FLAGS_RECEIVED_EXTMS;
        s->s3.flags |= TLS1_FLAGS_REQUIRED_EXTMS;
    }

    return 1;
}

static int final_ems(SSL_CONNECTION *s, unsigned int context, int sent)
{
    /*
     * Check extended master secret extension is not dropped on
     * renegotiation.
     */
    if (!(s->s3.flags & TLS1_FLAGS_RECEIVED_EXTMS)
        && (s->s3.flags & TLS1_FLAGS_REQUIRED_EXTMS)) {
        SSLfatal(s, SSL_AD_HANDSHAKE_FAILURE, SSL_R_INCONSISTENT_EXTMS);
        return 0;
    }
    if (!s->server && s->hit) {
        /*
         * Check extended master secret extension is consistent with
         * original session.
         */
        if (!(s->s3.flags & TLS1_FLAGS_RECEIVED_EXTMS) != !(s->session->flags & SSL_SESS_FLAG_EXTMS)) {
            SSLfatal(s, SSL_AD_HANDSHAKE_FAILURE, SSL_R_INCONSISTENT_EXTMS);
            return 0;
        }
    }

    return 1;
}

static int init_certificate_authorities(SSL_CONNECTION *s, unsigned int context)
{
    sk_X509_NAME_pop_free(s->s3.tmp.peer_ca_names, X509_NAME_free);
    s->s3.tmp.peer_ca_names = NULL;
    return 1;
}

static EXT_RETURN tls_construct_certificate_authorities(SSL_CONNECTION *s,
    WPACKET *pkt,
    unsigned int context,
    X509 *x,
    size_t chainidx)
{
    const STACK_OF(X509_NAME) *ca_sk = get_ca_names(s);

    if (ca_sk == NULL || sk_X509_NAME_num(ca_sk) == 0)
        return EXT_RETURN_NOT_SENT;

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_certificate_authorities)
        || !WPACKET_start_sub_packet_u16(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    if (!construct_ca_names(s, ca_sk, pkt)) {
        /* SSLfatal() already called */
        return EXT_RETURN_FAIL;
    }

    if (!WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }

    return EXT_RETURN_SENT;
}

static int tls_parse_certificate_authorities(SSL_CONNECTION *s, PACKET *pkt,
    unsigned int context, X509 *x,
    size_t chainidx)
{
    if (!parse_ca_names(s, pkt))
        return 0;
    if (PACKET_remaining(pkt) != 0) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    return 1;
}

#ifndef OPENSSL_NO_SRTP
static int init_srtp(SSL_CONNECTION *s, unsigned int context)
{
    if (s->server)
        s->srtp_profile = NULL;

    return 1;
}
#endif

static int final_sig_algs(SSL_CONNECTION *s, unsigned int context, int sent)
{
    if (!sent && SSL_CONNECTION_IS_VERSION13(s) && !s->hit) {
        SSLfatal(s, TLS13_AD_MISSING_EXTENSION,
            SSL_R_MISSING_SIGALGS_EXTENSION);
        return 0;
    }

    return 1;
}

static int final_supported_versions(SSL_CONNECTION *s, unsigned int context,
    int sent)
{
    if (!sent && context == SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST) {
        SSLfatal(s, TLS13_AD_MISSING_EXTENSION,
            SSL_R_MISSING_SUPPORTED_VERSIONS_EXTENSION);
        return 0;
    }

    return 1;
}

/* Is a HelloRetryRequest cookie required before the handshake can proceed? */
int tls_hrr_cookie_required(const SSL_CONNECTION *s)
{
    if ((s->s3.flags & TLS1_FLAGS_STATELESS) != 0)
        return 1;

    return SSL_CONNECTION_IS_DTLS(s)
        && (s->options & SSL_OP_COOKIE_EXCHANGE) != 0;
}

static int final_key_share(SSL_CONNECTION *s, unsigned int context, int sent)
{
#if !(defined(OPENSSL_NO_TLS1_3) && defined(OPENSSL_NO_DTLS1_3))
    if (!SSL_CONNECTION_IS_VERSION13(s))
        return 1;

    /* Nothing to do for key_share in an HRR */
    if ((context & SSL_EXT_TLS1_3_HELLO_RETRY_REQUEST) != 0)
        return 1;

    /*
     * If
     *     we are a client
     *     AND
     *     we have no key_share
     *     AND
     *     (we are not resuming
     *      OR the kex_mode doesn't allow non key_share resumes)
     * THEN
     *     fail;
     */
    if (!s->server
        && !sent) {
        if ((s->ext.psk_kex_mode & TLSEXT_KEX_MODE_FLAG_KE) == 0) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_NO_SUITABLE_KEY_SHARE);
            return 0;
        }
        if (!s->hit) {
            SSLfatal(s, SSL_AD_MISSING_EXTENSION, SSL_R_NO_SUITABLE_KEY_SHARE);
            return 0;
        }
    }
    /*
     * IF
     *     we are a server
     * THEN
     *     IF
     *         we have a suitable key_share
     *     THEN
     *         IF
     *             a cookie is required AND we have no cookie
     *         THEN
     *             send a HelloRetryRequest
     *     ELSE
     *         IF
     *             we didn't already send a HelloRetryRequest
     *             AND
     *             the client sent a key_share extension
     *             AND
     *             (we are not resuming
     *              OR the kex_mode allows key_share resumes)
     *             AND
     *             a shared group exists
     *         THEN
     *             send a HelloRetryRequest
     *         ELSE IF
     *             we are not resuming
     *             OR
     *             the kex_mode doesn't allow non key_share resumes
     *         THEN
     *             fail
     *         ELSE IF
     *             a cookie is required AND we have no cookie
     *         THEN
     *             send a HelloRetryRequest
     */
    if (s->server) {
        if (s->s3.peer_tmp != NULL) {
            /* We have a suitable key_share */
            if (tls_hrr_cookie_required(s) && !s->ext.cookieok) {
                if (s->hello_retry_request != SSL_HRR_NONE) {
                    /* We already sent an HRR and still have no valid cookie */
                    SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_COOKIE_MISMATCH);
                    return 0;
                }
                s->hello_retry_request = SSL_HRR_PENDING;
                return 1;
            }
        } else {
            /* No suitable key_share */
            if (s->hello_retry_request == SSL_HRR_NONE && sent
                && (!s->hit
                    || (s->ext.psk_kex_mode & TLSEXT_KEX_MODE_FLAG_KE_DHE) != 0)) {

                /* Did we detect group overlap in tls_parse_ctos_key_share ? */
                if (s->s3.group_id_candidate != 0) {
                    /* A shared group exists so send a HelloRetryRequest */
                    s->s3.group_id = s->s3.group_id_candidate;
                    s->hello_retry_request = SSL_HRR_PENDING;
                    return 1;
                }
            }
            if (!s->hit
                || (s->ext.psk_kex_mode & TLSEXT_KEX_MODE_FLAG_KE) == 0) {
                /* Nothing left we can do - just fail */
                SSLfatal(s, sent ? SSL_AD_HANDSHAKE_FAILURE : SSL_AD_MISSING_EXTENSION,
                    SSL_R_NO_SUITABLE_KEY_SHARE);
                return 0;
            }

            if (tls_hrr_cookie_required(s) && !s->ext.cookieok) {
                if (s->hello_retry_request != SSL_HRR_NONE) {
                    /* We already sent an HRR and still have no valid cookie */
                    SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_COOKIE_MISMATCH);
                    return 0;
                }
                s->hello_retry_request = SSL_HRR_PENDING;
                return 1;
            }
        }

        /*
         * We have a key_share so don't send any more HelloRetryRequest
         * messages
         */
        if (s->hello_retry_request == SSL_HRR_PENDING)
            s->hello_retry_request = SSL_HRR_COMPLETE;
    } else {
        /*
         * For a client side resumption with no key_share we need to generate
         * the handshake secret (otherwise this is done during key_share
         * processing).
         */
        if (!sent && !tls13_generate_handshake_secret(s, NULL, 0)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }
#endif /* !defined(OPENSSL_NO_TLS1_3) && !defined(OPENSSL_NO_DTLS1_3) */
    return 1;
}

static int init_psk_kex_modes(SSL_CONNECTION *s, unsigned int context)
{
    s->ext.psk_kex_mode = TLSEXT_KEX_MODE_FLAG_NONE;
    return 1;
}

int tls_psk_do_binder(SSL_CONNECTION *s, const EVP_MD *md,
    const unsigned char *msgstart,
    size_t binderoffset, const unsigned char *binderin,
    unsigned char *binderout, SSL_SESSION *sess, int sign,
    int external)
{
    EVP_PKEY *mackey = NULL;
    EVP_MD_CTX *mctx = NULL;
    unsigned char hash[EVP_MAX_MD_SIZE], binderkey[EVP_MAX_MD_SIZE];
    unsigned char finishedkey[EVP_MAX_MD_SIZE], tmpbinder[EVP_MAX_MD_SIZE];
    unsigned char *early_secret;
    /* ASCII: "res binder", in hex for EBCDIC compatibility */
    static const unsigned char resumption_label[] = "\x72\x65\x73\x20\x62\x69\x6E\x64\x65\x72";
    /* ASCII: "ext binder", in hex for EBCDIC compatibility */
    static const unsigned char external_label[] = "\x65\x78\x74\x20\x62\x69\x6E\x64\x65\x72";
    const unsigned char *label, *msgbodystart;
    size_t bindersize, labelsize, hashsize, msgbodylen;
    int hashsizei = EVP_MD_get_size(md);
    int ret = -1;
    int usepskfored = 0;
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };

    /* Ensure cast to size_t is safe */
    if (!ossl_assert(hashsizei > 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    hashsize = (size_t)hashsizei;

    if (external
        && s->early_data_state == SSL_EARLY_DATA_CONNECTING
        && sess == s->ext.early_data_session)
        usepskfored = 1;

    if (external) {
        label = external_label;
        labelsize = sizeof(external_label) - 1;
    } else {
        label = resumption_label;
        labelsize = sizeof(resumption_label) - 1;
    }

    /*
     * Generate the early_secret. On the server side we've selected a PSK to
     * resume with (internal or external) so we always do this. On the client
     * side we do this for a non-external (i.e. resumption) PSK or external PSK
     * that will be used for early_data so that it is in place for sending early
     * data. For client side external PSK not being used for early_data we
     * generate it but store it away for later use.
     */
    if (s->server || !external || usepskfored)
        early_secret = (unsigned char *)s->early_secret;
    else
        early_secret = (unsigned char *)sess->early_secret;

    if (!tls13_generate_secret(s, md, NULL, sess->master_key,
            sess->master_key_length, early_secret)) {
        /* SSLfatal() already called */
        goto err;
    }

    /*
     * Create the handshake hash for the binder key...the messages so far are
     * empty!
     */
    mctx = EVP_MD_CTX_new();
    if (mctx == NULL
        || EVP_DigestInit_ex(mctx, md, NULL) <= 0
        || EVP_DigestFinal_ex(mctx, hash, NULL) <= 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    /* Generate the binder key */
    if (!tls13_hkdf_expand(s, md, early_secret, label, labelsize, hash,
            hashsize, binderkey, hashsize, 1)) {
        /* SSLfatal() already called */
        goto err;
    }

    /* Generate the finished key */
    if (!tls13_derive_finishedkey(s, md, binderkey, finishedkey, hashsize)) {
        /* SSLfatal() already called */
        goto err;
    }

    if (EVP_DigestInit_ex(mctx, md, NULL) <= 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    /*
     * Get a hash of the ClientHello up to the start of the binders. If we are
     * following a HelloRetryRequest then this includes the hash of the first
     * ClientHello and the HelloRetryRequest itself.
     */
    if (s->hello_retry_request == SSL_HRR_PENDING) {
        size_t hdatalen;
        long hdatalen_l;
        unsigned char *hdata;

#ifndef OPENSSL_NO_ECH
        /* handle the hashing as per ECH needs (on client) */
        if (s->ext.ech.attempted == 1 && s->ext.ech.ch_depth == 1) {
            if (ossl_ech_intbuf_fetch(s, (unsigned char **)&hdata, &hdatalen) != 1) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }
        } else {
#endif
            hdatalen = hdatalen_l = BIO_get_mem_data(s->s3.handshake_buffer, &hdata);
            if (hdatalen_l <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_HANDSHAKE_LENGTH);
                goto err;
            }
#ifndef OPENSSL_NO_ECH
        }
#endif

        if (s->negotiated_version == DTLS1_3_VERSION) {
            PACKET hashprefix, msg;
            unsigned long dtlsbodylen;
            unsigned int dtls_offset = DTLS1_HM_HEADER_LENGTH - SSL3_HM_HEADER_LENGTH;

            /*
             * RFC 9147 states that for DTLS 1.3 all transcripts should be
             * calculated without the message_seq, fragment_offset and
             * fragment_length values. See Section 5.2
             *
             * Since the data is coming from handshake_buffer it hasn't
             * been processed in ssl3_finish_mac where these values are
             * removed. Therefore for both the server and client we will
             * need to not supply them to the Digest Update
             */
            if (!PACKET_buf_init(&hashprefix, hdata, hdatalen)
                || !PACKET_forward(&hashprefix, 1)
                || !PACKET_get_length_prefixed_3(&hashprefix, &msg)
                || !PACKET_forward(&hashprefix, 1)
                || !PACKET_get_net_3(&hashprefix, &dtlsbodylen)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }

            hdatalen -= PACKET_remaining(&hashprefix);

            if (EVP_DigestUpdate(mctx, hdata, hdatalen) <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }

            /*
             * Now skip the four bytes for the part of the DTLS header to not
             * include in the transcript.
             */
            if (EVP_DigestUpdate(mctx, hdata + hdatalen + dtls_offset, dtlsbodylen) <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }
        } else {
            /*
             * For servers the handshake buffer data will include the second
             * ClientHello - which we don't want - so we need to take that bit off.
             */
            if (s->server) {
                PACKET hashprefix, msg;

                /* Find how many bytes are left after the first two messages */
                if (!PACKET_buf_init(&hashprefix, hdata, hdatalen)
                    || !PACKET_forward(&hashprefix, 1)
                    || !PACKET_get_length_prefixed_3(&hashprefix, &msg)
                    || !PACKET_forward(&hashprefix, 1)
                    || !PACKET_get_length_prefixed_3(&hashprefix, &msg)) {
                    SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    goto err;
                }
                hdatalen -= PACKET_remaining(&hashprefix);
            }

            if (EVP_DigestUpdate(mctx, hdata, hdatalen) <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }
        }
    }

    if (SSL_CONNECTION_IS_DTLS(s)) {
        msgbodystart = msgstart + DTLS1_HM_HEADER_LENGTH;
        msgbodylen = binderoffset - DTLS1_HM_HEADER_LENGTH;
    } else {
        msgbodystart = msgstart + SSL3_HM_HEADER_LENGTH;
        msgbodylen = binderoffset - SSL3_HM_HEADER_LENGTH;
    }

    /*
     * RFC9147 (DTLSv1.3)
     * The transcript consists of complete TLS Handshake messages
     * (reassembled as necessary). Note that this requires removing the
     * message_seq, fragment_offset, and fragment_length fields to create
     * the Handshake structure.
     */
    if (EVP_DigestUpdate(mctx, msgstart, SSL3_HM_HEADER_LENGTH) <= 0
        || EVP_DigestUpdate(mctx, msgbodystart, msgbodylen) <= 0
        || EVP_DigestFinal_ex(mctx, hash, NULL) <= 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    mackey = EVP_PKEY_new_raw_private_key_ex(sctx->libctx, "HMAC",
        sctx->propq, finishedkey,
        hashsize);
    if (mackey == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    if (!sign)
        binderout = tmpbinder;

    if (sctx->propq != NULL)
        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_PROPERTIES,
            (char *)sctx->propq, 0);
    bindersize = hashsize;
    if (EVP_DigestSignInit_ex(mctx, NULL, EVP_MD_get0_name(md), sctx->libctx,
            sctx->propq, mackey, params)
            <= 0
        || EVP_DigestSignUpdate(mctx, hash, hashsize) <= 0
        || EVP_DigestSignFinal(mctx, binderout, &bindersize) <= 0
        || bindersize != hashsize) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    if (sign) {
        ret = 1;
    } else {
        /* HMAC keys can't do EVP_DigestVerify* - use CRYPTO_memcmp instead */
        ret = (CRYPTO_memcmp(binderin, binderout, hashsize) == 0);
        if (!ret)
            SSLfatal(s, SSL_AD_DECRYPT_ERROR, SSL_R_BINDER_DOES_NOT_VERIFY);
    }

err:
    OPENSSL_cleanse(binderkey, sizeof(binderkey));
    OPENSSL_cleanse(finishedkey, sizeof(finishedkey));
    EVP_PKEY_free(mackey);
    EVP_MD_CTX_free(mctx);
    return ret;
}

static int final_early_data(SSL_CONNECTION *s, unsigned int context, int sent)
{
    if (!sent)
        return 1;

    if (!s->server) {
        if (context == SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS
            && sent
            && !s->ext.early_data_ok) {
            /*
             * If we get here then the server accepted our early_data but we
             * later realised that it shouldn't have done (e.g. inconsistent
             * ALPN)
             */
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EARLY_DATA);
            return 0;
        }

        return 1;
    }

    if (s->max_early_data == 0
        || !s->hit
        || s->early_data_state != SSL_EARLY_DATA_ACCEPTING
        || !s->ext.early_data_ok
        || s->hello_retry_request != SSL_HRR_NONE
        || (s->allow_early_data_cb != NULL
            && !s->allow_early_data_cb(SSL_CONNECTION_GET_USER_SSL(s),
                s->allow_early_data_cb_data))) {
        s->ext.early_data = SSL_EARLY_DATA_REJECTED;
    } else {
        s->ext.early_data = SSL_EARLY_DATA_ACCEPTED;

        if (!tls13_change_cipher_state(s,
                SSL3_CC_EARLY | SSL3_CHANGE_CIPHER_SERVER_READ)) {
            /* SSLfatal() already called */
            return 0;
        }
    }

    return 1;
}

static int final_maxfragmentlen(SSL_CONNECTION *s, unsigned int context,
    int sent)
{
    if (s->session == NULL)
        return 1;

    /* MaxFragmentLength defaults to disabled */
    if (s->session->ext.max_fragment_len_mode == TLSEXT_max_fragment_length_UNSPECIFIED)
        s->session->ext.max_fragment_len_mode = TLSEXT_max_fragment_length_DISABLED;

    if (USE_MAX_FRAGMENT_LENGTH_EXT(s->session)) {
        s->rlayer.rrlmethod->set_max_frag_len(s->rlayer.rrl,
            GET_MAX_FRAGMENT_LENGTH(s->session));
        s->rlayer.wrlmethod->set_max_frag_len(s->rlayer.wrl,
            ssl_get_max_send_fragment(s));
    }

    return 1;
}

static int init_post_handshake_auth(SSL_CONNECTION *s,
    ossl_unused unsigned int context)
{
    s->post_handshake_auth = SSL_PHA_NONE;

    return 1;
}

/*
 * If clients offer "pre_shared_key" without a "psk_key_exchange_modes"
 * extension, servers MUST abort the handshake.
 */
static int final_psk(SSL_CONNECTION *s, unsigned int context, int sent)
{
    if (s->server && sent && s->clienthello != NULL
        && !s->clienthello->pre_proc_exts[TLSEXT_IDX_psk_kex_modes].present) {
        SSLfatal(s, TLS13_AD_MISSING_EXTENSION,
            SSL_R_MISSING_PSK_KEX_MODES_EXTENSION);
        return 0;
    }

    return 1;
}

static int tls_init_compress_certificate(SSL_CONNECTION *sc, unsigned int context)
{
    memset(sc->ext.compress_certificate_from_peer, 0,
        sizeof(sc->ext.compress_certificate_from_peer));
    return 1;
}

/* The order these are put into the packet imply a preference order: [brotli, zlib, zstd] */
static EXT_RETURN tls_construct_compress_certificate(SSL_CONNECTION *sc, WPACKET *pkt,
    unsigned int context,
    X509 *x, size_t chainidx)
{
#ifndef OPENSSL_NO_COMP_ALG
    int i;

    if (!ossl_comp_has_alg(0))
        return EXT_RETURN_NOT_SENT;

    /* Server: Don't attempt to compress a non-X509 (i.e. an RPK) */
    if (sc->server && sc->ext.server_cert_type != TLSEXT_cert_type_x509) {
        sc->cert_comp_prefs[0] = TLSEXT_comp_cert_none;
        return EXT_RETURN_NOT_SENT;
    }

    /* Client: If we sent a client cert-type extension, don't indicate compression */
    if (!sc->server && sc->ext.client_cert_type_ctos) {
        sc->cert_comp_prefs[0] = TLSEXT_comp_cert_none;
        return EXT_RETURN_NOT_SENT;
    }

    /* Do not indicate we support receiving compressed certificates */
    if ((sc->options & SSL_OP_NO_RX_CERTIFICATE_COMPRESSION) != 0)
        return EXT_RETURN_NOT_SENT;

    if (sc->cert_comp_prefs[0] == TLSEXT_comp_cert_none)
        return EXT_RETURN_NOT_SENT;
#ifndef OPENSSL_NO_ECH
    ECH_SAME_EXT(sc, context, pkt);
#endif

    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_compress_certificate)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_start_sub_packet_u8(pkt))
        goto err;

    for (i = 0; sc->cert_comp_prefs[i] != TLSEXT_comp_cert_none; i++) {
        if (!WPACKET_put_bytes_u16(pkt, sc->cert_comp_prefs[i]))
            goto err;
    }
    if (!WPACKET_close(pkt) || !WPACKET_close(pkt))
        goto err;

    sc->ext.compress_certificate_sent = 1;
    return EXT_RETURN_SENT;
err:
    SSLfatal(sc, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
    return EXT_RETURN_FAIL;
#else
    return EXT_RETURN_NOT_SENT;
#endif
}

#ifndef OPENSSL_NO_COMP_ALG
static int tls_comp_in_pref(SSL_CONNECTION *sc, int alg)
{
    int i;

    /* ossl_comp_has_alg() considers 0 as "any" */
    if (alg == 0)
        return 0;
    /* Make sure algorithm is enabled */
    if (!ossl_comp_has_alg(alg))
        return 0;
    /* If no preferences are set, it's ok */
    if (sc->cert_comp_prefs[0] == TLSEXT_comp_cert_none)
        return 1;
    /* Find the algorithm */
    for (i = 0; i < TLSEXT_comp_cert_limit; i++)
        if (sc->cert_comp_prefs[i] == alg)
            return 1;
    return 0;
}
#endif

int tls_parse_compress_certificate(SSL_CONNECTION *sc, PACKET *pkt, unsigned int context,
    X509 *x, size_t chainidx)
{
#ifndef OPENSSL_NO_COMP_ALG
    PACKET supported_comp_algs;
    unsigned int comp;
    int already_set[TLSEXT_comp_cert_limit];
    int j = 0;

    /* If no algorithms are available, ignore the extension */
    if (!ossl_comp_has_alg(0))
        return 1;

    /* Don't attempt to compress a non-X509 (i.e. an RPK) */
    if (sc->server && sc->ext.server_cert_type != TLSEXT_cert_type_x509)
        return 1;
    if (!sc->server && sc->ext.client_cert_type != TLSEXT_cert_type_x509)
        return 1;

    /* Ignore the extension and don't send compressed certificates */
    if ((sc->options & SSL_OP_NO_TX_CERTIFICATE_COMPRESSION) != 0)
        return 1;

    if (!PACKET_as_length_prefixed_1(pkt, &supported_comp_algs)
        || PACKET_remaining(&supported_comp_algs) == 0) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }

    memset(already_set, 0, sizeof(already_set));
    /*
     * The preference array has real values, so take a look at each
     * value coming in, and make sure it's in our preference list
     * The array is 0 (i.e. "none") terminated
     * The preference list only contains supported algorithms
     */
    while (PACKET_get_net_2(&supported_comp_algs, &comp)) {
        if (tls_comp_in_pref(sc, comp) && !already_set[comp]) {
            sc->ext.compress_certificate_from_peer[j++] = comp;
            already_set[comp] = 1;
        }
    }
    if (PACKET_remaining(&supported_comp_algs) != 0) {
        SSLfatal(sc, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
#endif
    return 1;
}

static int init_server_cert_type(SSL_CONNECTION *sc, unsigned int context)
{
    /* Only reset when parsing client hello */
    if (sc->server) {
        sc->ext.server_cert_type_ctos = OSSL_CERT_TYPE_CTOS_NONE;
        sc->ext.server_cert_type = TLSEXT_cert_type_x509;
    }
    return 1;
}

static int init_client_cert_type(SSL_CONNECTION *sc, unsigned int context)
{
    /* Only reset when parsing client hello */
    if (sc->server) {
        sc->ext.client_cert_type_ctos = OSSL_CERT_TYPE_CTOS_NONE;
        sc->ext.client_cert_type = TLSEXT_cert_type_x509;
    }
    return 1;
}
