/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_SSL_TEST_CTX_H
#define HEADER_SSL_TEST_CTX_H

#include <openssl/conf.h>
#include <openssl/ssl.h>

typedef enum {
    SSL_TEST_SUCCESS = 0,  /* Default */
    SSL_TEST_SERVER_FAIL,
    SSL_TEST_CLIENT_FAIL,
    SSL_TEST_INTERNAL_ERROR,
    /* Couldn't test resumption/renegotiation: original handshake failed. */
    SSL_TEST_FIRST_HANDSHAKE_FAILED
} ssl_test_result_t;

typedef enum {
    SSL_TEST_VERIFY_NONE = 0, /* Default */
    SSL_TEST_VERIFY_ACCEPT_ALL,
    SSL_TEST_VERIFY_REJECT_ALL
} ssl_verify_callback_t;

typedef enum {
    SSL_TEST_SERVERNAME_NONE = 0, /* Default */
    SSL_TEST_SERVERNAME_SERVER1,
    SSL_TEST_SERVERNAME_SERVER2,
    SSL_TEST_SERVERNAME_INVALID
} ssl_servername_t;

typedef enum {
    SSL_TEST_SERVERNAME_CB_NONE = 0,  /* Default */
    SSL_TEST_SERVERNAME_IGNORE_MISMATCH,
    SSL_TEST_SERVERNAME_REJECT_MISMATCH
} ssl_servername_callback_t;

typedef enum {
    SSL_TEST_SESSION_TICKET_IGNORE = 0, /* Default */
    SSL_TEST_SESSION_TICKET_YES,
    SSL_TEST_SESSION_TICKET_NO,
    SSL_TEST_SESSION_TICKET_BROKEN /* Special test */
} ssl_session_ticket_t;

typedef enum {
    SSL_TEST_METHOD_TLS = 0, /* Default */
    SSL_TEST_METHOD_DTLS
} ssl_test_method_t;

typedef enum {
    SSL_TEST_HANDSHAKE_SIMPLE = 0, /* Default */
    SSL_TEST_HANDSHAKE_RESUME,
    SSL_TEST_HANDSHAKE_RENEG_SERVER,
    SSL_TEST_HANDSHAKE_RENEG_CLIENT
} ssl_handshake_mode_t;

typedef enum {
    SSL_TEST_CT_VALIDATION_NONE = 0, /* Default */
    SSL_TEST_CT_VALIDATION_PERMISSIVE,
    SSL_TEST_CT_VALIDATION_STRICT
} ssl_ct_validation_t;

typedef enum {
    SSL_TEST_CERT_STATUS_NONE = 0, /* Default */
    SSL_TEST_CERT_STATUS_GOOD_RESPONSE,
    SSL_TEST_CERT_STATUS_BAD_RESPONSE
} ssl_cert_status_t;
/*
 * Server/client settings that aren't supported by the SSL CONF library,
 * such as callbacks.
 */
typedef struct {
    /* One of a number of predefined custom callbacks. */
    ssl_verify_callback_t verify_callback;
    /* One of a number of predefined server names use by the client */
    ssl_servername_t servername;
    /* Supported NPN and ALPN protocols. A comma-separated list. */
    char *npn_protocols;
    char *alpn_protocols;
    ssl_ct_validation_t ct_validation;
    /* Ciphersuites to set on a renegotiation */
    char *reneg_ciphers;
} SSL_TEST_CLIENT_CONF;

typedef struct {
    /* SNI callback (server-side). */
    ssl_servername_callback_t servername_callback;
    /* Supported NPN and ALPN protocols. A comma-separated list. */
    char *npn_protocols;
    char *alpn_protocols;
    /* Whether to set a broken session ticket callback. */
    int broken_session_ticket;
    /* Should we send a CertStatus message? */
    ssl_cert_status_t cert_status;
} SSL_TEST_SERVER_CONF;

typedef struct {
    SSL_TEST_CLIENT_CONF client;
    SSL_TEST_SERVER_CONF server;
    SSL_TEST_SERVER_CONF server2;
} SSL_TEST_EXTRA_CONF;

typedef struct {
    /*
     * Global test configuration. Does not change between handshakes.
     */
    /* Whether the server/client CTX should use DTLS or TLS. */
    ssl_test_method_t method;
    /* Whether to test a resumed/renegotiated handshake. */
    ssl_handshake_mode_t handshake_mode;
    /*
     * How much application data to exchange (default is 256 bytes).
     * Both peers will send |app_data_size| bytes interleaved.
     */
    int app_data_size;
    /* Maximum send fragment size. */
    int max_fragment_size;

    /*
     * Extra server/client configurations. Per-handshake.
     */
    /* First handshake. */
    SSL_TEST_EXTRA_CONF extra;
    /* Resumed handshake. */
    SSL_TEST_EXTRA_CONF resume_extra;

    /*
     * Test expectations. These apply to the LAST handshake.
     */
    /* Defaults to SUCCESS. */
    ssl_test_result_t expected_result;
    /* Alerts. 0 if no expectation. */
    /* See ssl.h for alert codes. */
    /* Alert sent by the client / received by the server. */
    int expected_client_alert;
    /* Alert sent by the server / received by the client. */
    int expected_server_alert;
    /* Negotiated protocol version. 0 if no expectation. */
    /* See ssl.h for protocol versions. */
    int expected_protocol;
    /*
     * The expected SNI context to use.
     * We test server-side that the server switched to the expected context.
     * Set by the callback upon success, so if the callback wasn't called or
     * terminated with an alert, the servername will match with
     * SSL_TEST_SERVERNAME_NONE.
     * Note: in the event that the servername was accepted, the client should
     * also receive an empty SNI extension back but we have no way of probing
     * client-side via the API that this was the case.
     */
    ssl_servername_t expected_servername;
    ssl_session_ticket_t session_ticket_expected;
    /* The expected NPN/ALPN protocol to negotiate. */
    char *expected_npn_protocol;
    char *expected_alpn_protocol;
    /* Whether the second handshake is resumed or a full handshake (boolean). */
    int resumption_expected;
    /* Expected temporary key type */
    int expected_tmp_key_type;
} SSL_TEST_CTX;

const char *ssl_test_result_name(ssl_test_result_t result);
const char *ssl_alert_name(int alert);
const char *ssl_protocol_name(int protocol);
const char *ssl_verify_callback_name(ssl_verify_callback_t verify_callback);
const char *ssl_servername_name(ssl_servername_t server);
const char *ssl_servername_callback_name(ssl_servername_callback_t
                                         servername_callback);
const char *ssl_session_ticket_name(ssl_session_ticket_t server);
const char *ssl_test_method_name(ssl_test_method_t method);
const char *ssl_handshake_mode_name(ssl_handshake_mode_t mode);
const char *ssl_ct_validation_name(ssl_ct_validation_t mode);
const char *ssl_certstatus_name(ssl_cert_status_t cert_status);

/*
 * Load the test case context from |conf|.
 * See test/README.ssltest.md for details on the conf file format.
 */
SSL_TEST_CTX *SSL_TEST_CTX_create(const CONF *conf, const char *test_section);

SSL_TEST_CTX *SSL_TEST_CTX_new(void);

void SSL_TEST_CTX_free(SSL_TEST_CTX *ctx);

#endif  /* HEADER_SSL_TEST_CTX_H */
