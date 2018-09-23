/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Ideally, CONF should offer standard parsing methods and cover them
 * in tests. But since we have no CONF tests, we use a custom test for now.
 */

#include <stdio.h>
#include <string.h>

#include "e_os.h"
#include "ssl_test_ctx.h"
#include "testutil.h"
#include <openssl/e_os2.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/ssl.h>

static CONF *conf = NULL;

typedef struct ssl_test_ctx_test_fixture {
    const char *test_case_name;
    const char *test_section;
    /* Expected parsed configuration. */
    SSL_TEST_CTX *expected_ctx;
} SSL_TEST_CTX_TEST_FIXTURE;


static int SSL_TEST_CLIENT_CONF_equal(SSL_TEST_CLIENT_CONF *client,
                                      SSL_TEST_CLIENT_CONF *client2)
{
    if (client->verify_callback != client2->verify_callback) {
        fprintf(stderr, "ClientVerifyCallback mismatch: %s vs %s.\n",
                ssl_verify_callback_name(client->verify_callback),
                ssl_verify_callback_name(client2->verify_callback));
        return 0;
    }
    if (client->servername != client2->servername) {
        fprintf(stderr, "ServerName mismatch: %s vs %s.\n",
                ssl_servername_name(client->servername),
                ssl_servername_name(client2->servername));
        return 0;
    }
    if (!strings_equal("Client NPNProtocols", client->npn_protocols,
                       client2->npn_protocols))
        return 0;
    if (!strings_equal("Client ALPNProtocols", client->alpn_protocols,
                       client2->alpn_protocols))
        return 0;
    if (client->ct_validation != client2->ct_validation) {
        fprintf(stderr, "CTValidation mismatch: %s vs %s.\n",
                ssl_ct_validation_name(client->ct_validation),
                ssl_ct_validation_name(client2->ct_validation));
        return 0;
    }
    return 1;
}

static int SSL_TEST_SERVER_CONF_equal(SSL_TEST_SERVER_CONF *server,
                                      SSL_TEST_SERVER_CONF *server2)
{
    if (server->servername_callback != server2->servername_callback) {
        fprintf(stderr, "ServerNameCallback mismatch: %s vs %s.\n",
                ssl_servername_callback_name(server->servername_callback),
                ssl_servername_callback_name(server2->servername_callback));
        return 0;
    }
    if (!strings_equal("Server NPNProtocols", server->npn_protocols,
                       server2->npn_protocols))
        return 0;
    if (!strings_equal("Server ALPNProtocols", server->alpn_protocols,
                       server2->alpn_protocols))
        return 0;
    if (server->broken_session_ticket != server2->broken_session_ticket) {
        fprintf(stderr, "Broken session ticket mismatch: %d vs %d.\n",
                server->broken_session_ticket, server2->broken_session_ticket);
        return 0;
    }
    if (server->cert_status != server2->cert_status) {
        fprintf(stderr, "CertStatus mismatch: %s vs %s.\n",
                ssl_certstatus_name(server->cert_status),
                ssl_certstatus_name(server2->cert_status));
        return 0;
    }
    return 1;
}

static int SSL_TEST_EXTRA_CONF_equal(SSL_TEST_EXTRA_CONF *extra,
                                     SSL_TEST_EXTRA_CONF *extra2)
{
    return SSL_TEST_CLIENT_CONF_equal(&extra->client, &extra2->client)
        && SSL_TEST_SERVER_CONF_equal(&extra->server, &extra2->server)
        && SSL_TEST_SERVER_CONF_equal(&extra->server2, &extra2->server2);
}

/* Returns 1 if the contexts are equal, 0 otherwise. */
static int SSL_TEST_CTX_equal(SSL_TEST_CTX *ctx, SSL_TEST_CTX *ctx2)
{
    if (ctx->method != ctx2->method) {
        fprintf(stderr, "Method mismatch: %s vs %s.\n",
                ssl_test_method_name(ctx->method),
                ssl_test_method_name(ctx2->method));
        return 0;
    }
    if (ctx->handshake_mode != ctx2->handshake_mode) {
        fprintf(stderr, "HandshakeMode mismatch: %s vs %s.\n",
                ssl_handshake_mode_name(ctx->handshake_mode),
                ssl_handshake_mode_name(ctx2->handshake_mode));
        return 0;
    }
    if (ctx->app_data_size != ctx2->app_data_size) {
        fprintf(stderr, "ApplicationData mismatch: %d vs %d.\n",
                ctx->app_data_size, ctx2->app_data_size);
        return 0;
    }

    if (ctx->max_fragment_size != ctx2->max_fragment_size) {
        fprintf(stderr, "MaxFragmentSize mismatch: %d vs %d.\n",
                ctx->max_fragment_size, ctx2->max_fragment_size);
        return 0;
    }

    if (!SSL_TEST_EXTRA_CONF_equal(&ctx->extra, &ctx2->extra)) {
        fprintf(stderr, "Extra conf mismatch.\n");
        return 0;
    }
    if (!SSL_TEST_EXTRA_CONF_equal(&ctx->resume_extra, &ctx2->resume_extra)) {
        fprintf(stderr, "Resume extra conf mismatch.\n");
        return 0;
    }

    if (ctx->expected_result != ctx2->expected_result) {
        fprintf(stderr, "ExpectedResult mismatch: %s vs %s.\n",
                ssl_test_result_name(ctx->expected_result),
                ssl_test_result_name(ctx2->expected_result));
        return 0;
    }
    if (ctx->expected_client_alert != ctx2->expected_client_alert) {
        fprintf(stderr, "ClientAlert mismatch: %s vs %s.\n",
                ssl_alert_name(ctx->expected_client_alert),
                ssl_alert_name(ctx2->expected_client_alert));
        return 0;
    }
    if (ctx->expected_server_alert != ctx2->expected_server_alert) {
        fprintf(stderr, "ServerAlert mismatch: %s vs %s.\n",
                ssl_alert_name(ctx->expected_server_alert),
                ssl_alert_name(ctx2->expected_server_alert));
        return 0;
    }
    if (ctx->expected_protocol != ctx2->expected_protocol) {
        fprintf(stderr, "ClientAlert mismatch: %s vs %s.\n",
                ssl_protocol_name(ctx->expected_protocol),
                ssl_protocol_name(ctx2->expected_protocol));
        return 0;
    }
    if (ctx->expected_servername != ctx2->expected_servername) {
        fprintf(stderr, "ExpectedServerName mismatch: %s vs %s.\n",
                ssl_servername_name(ctx->expected_servername),
                ssl_servername_name(ctx2->expected_servername));
        return 0;
    }
    if (ctx->session_ticket_expected != ctx2->session_ticket_expected) {
        fprintf(stderr, "SessionTicketExpected mismatch: %s vs %s.\n",
                ssl_session_ticket_name(ctx->session_ticket_expected),
                ssl_session_ticket_name(ctx2->session_ticket_expected));
        return 0;
    }
    if (!strings_equal("ExpectedNPNProtocol", ctx->expected_npn_protocol,
                       ctx2->expected_npn_protocol))
        return 0;
    if (!strings_equal("ExpectedALPNProtocol", ctx->expected_alpn_protocol,
                       ctx2->expected_alpn_protocol))
        return 0;
    if (ctx->resumption_expected != ctx2->resumption_expected) {
        fprintf(stderr, "ResumptionExpected mismatch: %d vs %d.\n",
                ctx->resumption_expected, ctx2->resumption_expected);
        return 0;
    }
    return 1;
}

static SSL_TEST_CTX_TEST_FIXTURE set_up(const char *const test_case_name)
{
    SSL_TEST_CTX_TEST_FIXTURE fixture;
    fixture.test_case_name = test_case_name;
    fixture.expected_ctx = SSL_TEST_CTX_new();
    TEST_check(fixture.expected_ctx != NULL);
    return fixture;
}

static int execute_test(SSL_TEST_CTX_TEST_FIXTURE fixture)
{
    int success = 0;

    SSL_TEST_CTX *ctx = SSL_TEST_CTX_create(conf, fixture.test_section);

    if (ctx == NULL) {
        fprintf(stderr, "Failed to parse good configuration %s.\n",
                fixture.test_section);
        goto err;
    }

    if (!SSL_TEST_CTX_equal(ctx, fixture.expected_ctx))
        goto err;

    success = 1;
 err:
    SSL_TEST_CTX_free(ctx);
    return success;
}

static int execute_failure_test(SSL_TEST_CTX_TEST_FIXTURE fixture)
{
    SSL_TEST_CTX *ctx = SSL_TEST_CTX_create(conf, fixture.test_section);

    if (ctx != NULL) {
        fprintf(stderr, "Parsing bad configuration %s succeeded.\n",
                fixture.test_section);
        SSL_TEST_CTX_free(ctx);
        return 0;
    }

    return 1;
}

static void tear_down(SSL_TEST_CTX_TEST_FIXTURE fixture)
{
    SSL_TEST_CTX_free(fixture.expected_ctx);
    ERR_print_errors_fp(stderr);
}

#define SETUP_SSL_TEST_CTX_TEST_FIXTURE()                       \
    SETUP_TEST_FIXTURE(SSL_TEST_CTX_TEST_FIXTURE, set_up)
#define EXECUTE_SSL_TEST_CTX_TEST()             \
    EXECUTE_TEST(execute_test, tear_down)
#define EXECUTE_SSL_TEST_CTX_FAILURE_TEST()             \
    EXECUTE_TEST(execute_failure_test, tear_down)

static int test_empty_configuration()
{
    SETUP_SSL_TEST_CTX_TEST_FIXTURE();
    fixture.test_section = "ssltest_default";
    fixture.expected_ctx->expected_result = SSL_TEST_SUCCESS;
    EXECUTE_SSL_TEST_CTX_TEST();
}

static int test_good_configuration()
{
    SETUP_SSL_TEST_CTX_TEST_FIXTURE();
    fixture.test_section = "ssltest_good";
    fixture.expected_ctx->method = SSL_TEST_METHOD_DTLS;
    fixture.expected_ctx->handshake_mode = SSL_TEST_HANDSHAKE_RESUME;
    fixture.expected_ctx->app_data_size = 1024;
    fixture.expected_ctx->max_fragment_size = 2048;

    fixture.expected_ctx->expected_result = SSL_TEST_SERVER_FAIL;
    fixture.expected_ctx->expected_client_alert = SSL_AD_UNKNOWN_CA;
    fixture.expected_ctx->expected_server_alert = 0;  /* No alert. */
    fixture.expected_ctx->expected_protocol = TLS1_1_VERSION;
    fixture.expected_ctx->expected_servername = SSL_TEST_SERVERNAME_SERVER2;
    fixture.expected_ctx->session_ticket_expected = SSL_TEST_SESSION_TICKET_YES;
    fixture.expected_ctx->resumption_expected = 1;

    fixture.expected_ctx->extra.client.verify_callback =
        SSL_TEST_VERIFY_REJECT_ALL;
    fixture.expected_ctx->extra.client.servername = SSL_TEST_SERVERNAME_SERVER2;
    fixture.expected_ctx->extra.client.npn_protocols =
        OPENSSL_strdup("foo,bar");
    TEST_check(fixture.expected_ctx->extra.client.npn_protocols != NULL);

    fixture.expected_ctx->extra.server.servername_callback =
        SSL_TEST_SERVERNAME_IGNORE_MISMATCH;
    fixture.expected_ctx->extra.server.broken_session_ticket = 1;

    fixture.expected_ctx->resume_extra.server2.alpn_protocols =
        OPENSSL_strdup("baz");
    TEST_check(
        fixture.expected_ctx->resume_extra.server2.alpn_protocols != NULL);

    fixture.expected_ctx->resume_extra.client.ct_validation =
        SSL_TEST_CT_VALIDATION_STRICT;

    EXECUTE_SSL_TEST_CTX_TEST();
}

static const char *bad_configurations[] = {
    "ssltest_unknown_option",
    "ssltest_wrong_section",
    "ssltest_unknown_expected_result",
    "ssltest_unknown_alert",
    "ssltest_unknown_protocol",
    "ssltest_unknown_verify_callback",
    "ssltest_unknown_servername",
    "ssltest_unknown_servername_callback",
    "ssltest_unknown_session_ticket_expected",
    "ssltest_unknown_method",
    "ssltest_unknown_handshake_mode",
    "ssltest_unknown_resumption_expected",
    "ssltest_unknown_ct_validation",
};

static int test_bad_configuration(int idx)
{
        SETUP_SSL_TEST_CTX_TEST_FIXTURE();
        fixture.test_section = bad_configurations[idx];
        EXECUTE_SSL_TEST_CTX_FAILURE_TEST();
}

int main(int argc, char **argv)
{
    int result = 0;

    if (argc != 2)
        return 1;

    conf = NCONF_new(NULL);
    TEST_check(conf != NULL);

    /* argv[1] should point to test/ssl_test_ctx_test.conf */
    TEST_check(NCONF_load(conf, argv[1], NULL) > 0);

    ADD_TEST(test_empty_configuration);
    ADD_TEST(test_good_configuration);
    ADD_ALL_TESTS(test_bad_configuration, OSSL_NELEM(bad_configurations));

    result = run_tests(argv[0]);

    NCONF_free(conf);

    return result;
}
