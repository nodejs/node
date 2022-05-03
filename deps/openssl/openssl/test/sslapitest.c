/*
 * Copyright 2016-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>

#include <openssl/opensslconf.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/ocsp.h>
#include <openssl/srp.h>
#include <openssl/txt_db.h>
#include <openssl/aes.h>
#include <openssl/x509v3.h>

#include "ssltestlib.h"
#include "testutil.h"
#include "testutil/output.h"
#include "internal/nelem.h"
#include "../ssl/ssl_local.h"

#ifndef OPENSSL_NO_TLS1_3

static SSL_SESSION *clientpsk = NULL;
static SSL_SESSION *serverpsk = NULL;
static const char *pskid = "Identity";
static const char *srvid;

static int use_session_cb(SSL *ssl, const EVP_MD *md, const unsigned char **id,
                          size_t *idlen, SSL_SESSION **sess);
static int find_session_cb(SSL *ssl, const unsigned char *identity,
                           size_t identity_len, SSL_SESSION **sess);

static int use_session_cb_cnt = 0;
static int find_session_cb_cnt = 0;

static SSL_SESSION *create_a_psk(SSL *ssl);
#endif

static char *certsdir = NULL;
static char *cert = NULL;
static char *privkey = NULL;
static char *srpvfile = NULL;
static char *tmpfilename = NULL;

#define LOG_BUFFER_SIZE 2048
static char server_log_buffer[LOG_BUFFER_SIZE + 1] = {0};
static size_t server_log_buffer_index = 0;
static char client_log_buffer[LOG_BUFFER_SIZE + 1] = {0};
static size_t client_log_buffer_index = 0;
static int error_writing_log = 0;

#ifndef OPENSSL_NO_OCSP
static const unsigned char orespder[] = "Dummy OCSP Response";
static int ocsp_server_called = 0;
static int ocsp_client_called = 0;

static int cdummyarg = 1;
static X509 *ocspcert = NULL;
#endif

#define NUM_EXTRA_CERTS 40
#define CLIENT_VERSION_LEN      2

/*
 * This structure is used to validate that the correct number of log messages
 * of various types are emitted when emitting secret logs.
 */
struct sslapitest_log_counts {
    unsigned int rsa_key_exchange_count;
    unsigned int master_secret_count;
    unsigned int client_early_secret_count;
    unsigned int client_handshake_secret_count;
    unsigned int server_handshake_secret_count;
    unsigned int client_application_secret_count;
    unsigned int server_application_secret_count;
    unsigned int early_exporter_secret_count;
    unsigned int exporter_secret_count;
};


static unsigned char serverinfov1[] = {
    0xff, 0xff, /* Dummy extension type */
    0x00, 0x01, /* Extension length is 1 byte */
    0xff        /* Dummy extension data */
};

static unsigned char serverinfov2[] = {
    0x00, 0x00, 0x00,
    (unsigned char)(SSL_EXT_CLIENT_HELLO & 0xff), /* Dummy context - 4 bytes */
    0xff, 0xff, /* Dummy extension type */
    0x00, 0x01, /* Extension length is 1 byte */
    0xff        /* Dummy extension data */
};

static int hostname_cb(SSL *s, int *al, void *arg)
{
    const char *hostname = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);

    if (hostname != NULL && (strcmp(hostname, "goodhost") == 0
                             || strcmp(hostname, "altgoodhost") == 0))
        return  SSL_TLSEXT_ERR_OK;

    return SSL_TLSEXT_ERR_NOACK;
}

static void client_keylog_callback(const SSL *ssl, const char *line)
{
    int line_length = strlen(line);

    /* If the log doesn't fit, error out. */
    if (client_log_buffer_index + line_length > sizeof(client_log_buffer) - 1) {
        TEST_info("Client log too full");
        error_writing_log = 1;
        return;
    }

    strcat(client_log_buffer, line);
    client_log_buffer_index += line_length;
    client_log_buffer[client_log_buffer_index++] = '\n';
}

static void server_keylog_callback(const SSL *ssl, const char *line)
{
    int line_length = strlen(line);

    /* If the log doesn't fit, error out. */
    if (server_log_buffer_index + line_length > sizeof(server_log_buffer) - 1) {
        TEST_info("Server log too full");
        error_writing_log = 1;
        return;
    }

    strcat(server_log_buffer, line);
    server_log_buffer_index += line_length;
    server_log_buffer[server_log_buffer_index++] = '\n';
}

static int compare_hex_encoded_buffer(const char *hex_encoded,
                                      size_t hex_length,
                                      const uint8_t *raw,
                                      size_t raw_length)
{
    size_t i, j;
    char hexed[3];

    if (!TEST_size_t_eq(raw_length * 2, hex_length))
        return 1;

    for (i = j = 0; i < raw_length && j + 1 < hex_length; i++, j += 2) {
        sprintf(hexed, "%02x", raw[i]);
        if (!TEST_int_eq(hexed[0], hex_encoded[j])
                || !TEST_int_eq(hexed[1], hex_encoded[j + 1]))
            return 1;
    }

    return 0;
}

static int test_keylog_output(char *buffer, const SSL *ssl,
                              const SSL_SESSION *session,
                              struct sslapitest_log_counts *expected)
{
    char *token = NULL;
    unsigned char actual_client_random[SSL3_RANDOM_SIZE] = {0};
    size_t client_random_size = SSL3_RANDOM_SIZE;
    unsigned char actual_master_key[SSL_MAX_MASTER_KEY_LENGTH] = {0};
    size_t master_key_size = SSL_MAX_MASTER_KEY_LENGTH;
    unsigned int rsa_key_exchange_count = 0;
    unsigned int master_secret_count = 0;
    unsigned int client_early_secret_count = 0;
    unsigned int client_handshake_secret_count = 0;
    unsigned int server_handshake_secret_count = 0;
    unsigned int client_application_secret_count = 0;
    unsigned int server_application_secret_count = 0;
    unsigned int early_exporter_secret_count = 0;
    unsigned int exporter_secret_count = 0;

    for (token = strtok(buffer, " \n"); token != NULL;
         token = strtok(NULL, " \n")) {
        if (strcmp(token, "RSA") == 0) {
            /*
             * Premaster secret. Tokens should be: 16 ASCII bytes of
             * hex-encoded encrypted secret, then the hex-encoded pre-master
             * secret.
             */
            if (!TEST_ptr(token = strtok(NULL, " \n")))
                return 0;
            if (!TEST_size_t_eq(strlen(token), 16))
                return 0;
            if (!TEST_ptr(token = strtok(NULL, " \n")))
                return 0;
            /*
             * We can't sensibly check the log because the premaster secret is
             * transient, and OpenSSL doesn't keep hold of it once the master
             * secret is generated.
             */
            rsa_key_exchange_count++;
        } else if (strcmp(token, "CLIENT_RANDOM") == 0) {
            /*
             * Master secret. Tokens should be: 64 ASCII bytes of hex-encoded
             * client random, then the hex-encoded master secret.
             */
            client_random_size = SSL_get_client_random(ssl,
                                                       actual_client_random,
                                                       SSL3_RANDOM_SIZE);
            if (!TEST_size_t_eq(client_random_size, SSL3_RANDOM_SIZE))
                return 0;

            if (!TEST_ptr(token = strtok(NULL, " \n")))
                return 0;
            if (!TEST_size_t_eq(strlen(token), 64))
                return 0;
            if (!TEST_false(compare_hex_encoded_buffer(token, 64,
                                                       actual_client_random,
                                                       client_random_size)))
                return 0;

            if (!TEST_ptr(token = strtok(NULL, " \n")))
                return 0;
            master_key_size = SSL_SESSION_get_master_key(session,
                                                         actual_master_key,
                                                         master_key_size);
            if (!TEST_size_t_ne(master_key_size, 0))
                return 0;
            if (!TEST_false(compare_hex_encoded_buffer(token, strlen(token),
                                                       actual_master_key,
                                                       master_key_size)))
                return 0;
            master_secret_count++;
        } else if (strcmp(token, "CLIENT_EARLY_TRAFFIC_SECRET") == 0
                    || strcmp(token, "CLIENT_HANDSHAKE_TRAFFIC_SECRET") == 0
                    || strcmp(token, "SERVER_HANDSHAKE_TRAFFIC_SECRET") == 0
                    || strcmp(token, "CLIENT_TRAFFIC_SECRET_0") == 0
                    || strcmp(token, "SERVER_TRAFFIC_SECRET_0") == 0
                    || strcmp(token, "EARLY_EXPORTER_SECRET") == 0
                    || strcmp(token, "EXPORTER_SECRET") == 0) {
            /*
             * TLSv1.3 secret. Tokens should be: 64 ASCII bytes of hex-encoded
             * client random, and then the hex-encoded secret. In this case,
             * we treat all of these secrets identically and then just
             * distinguish between them when counting what we saw.
             */
            if (strcmp(token, "CLIENT_EARLY_TRAFFIC_SECRET") == 0)
                client_early_secret_count++;
            else if (strcmp(token, "CLIENT_HANDSHAKE_TRAFFIC_SECRET") == 0)
                client_handshake_secret_count++;
            else if (strcmp(token, "SERVER_HANDSHAKE_TRAFFIC_SECRET") == 0)
                server_handshake_secret_count++;
            else if (strcmp(token, "CLIENT_TRAFFIC_SECRET_0") == 0)
                client_application_secret_count++;
            else if (strcmp(token, "SERVER_TRAFFIC_SECRET_0") == 0)
                server_application_secret_count++;
            else if (strcmp(token, "EARLY_EXPORTER_SECRET") == 0)
                early_exporter_secret_count++;
            else if (strcmp(token, "EXPORTER_SECRET") == 0)
                exporter_secret_count++;

            client_random_size = SSL_get_client_random(ssl,
                                                       actual_client_random,
                                                       SSL3_RANDOM_SIZE);
            if (!TEST_size_t_eq(client_random_size, SSL3_RANDOM_SIZE))
                return 0;

            if (!TEST_ptr(token = strtok(NULL, " \n")))
                return 0;
            if (!TEST_size_t_eq(strlen(token), 64))
                return 0;
            if (!TEST_false(compare_hex_encoded_buffer(token, 64,
                                                       actual_client_random,
                                                       client_random_size)))
                return 0;

            if (!TEST_ptr(token = strtok(NULL, " \n")))
                return 0;

            /*
             * TODO(TLS1.3): test that application traffic secrets are what
             * we expect */
        } else {
            TEST_info("Unexpected token %s\n", token);
            return 0;
        }
    }

    /* Got what we expected? */
    if (!TEST_size_t_eq(rsa_key_exchange_count,
                        expected->rsa_key_exchange_count)
            || !TEST_size_t_eq(master_secret_count,
                               expected->master_secret_count)
            || !TEST_size_t_eq(client_early_secret_count,
                               expected->client_early_secret_count)
            || !TEST_size_t_eq(client_handshake_secret_count,
                               expected->client_handshake_secret_count)
            || !TEST_size_t_eq(server_handshake_secret_count,
                               expected->server_handshake_secret_count)
            || !TEST_size_t_eq(client_application_secret_count,
                               expected->client_application_secret_count)
            || !TEST_size_t_eq(server_application_secret_count,
                               expected->server_application_secret_count)
            || !TEST_size_t_eq(early_exporter_secret_count,
                               expected->early_exporter_secret_count)
            || !TEST_size_t_eq(exporter_secret_count,
                               expected->exporter_secret_count))
        return 0;
    return 1;
}

#if !defined(OPENSSL_NO_TLS1_2) || defined(OPENSSL_NO_TLS1_3)
static int test_keylog(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    struct sslapitest_log_counts expected = {0};

    /* Clean up logging space */
    memset(client_log_buffer, 0, sizeof(client_log_buffer));
    memset(server_log_buffer, 0, sizeof(server_log_buffer));
    client_log_buffer_index = 0;
    server_log_buffer_index = 0;
    error_writing_log = 0;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        return 0;

    /* We cannot log the master secret for TLSv1.3, so we should forbid it. */
    SSL_CTX_set_options(cctx, SSL_OP_NO_TLSv1_3);
    SSL_CTX_set_options(sctx, SSL_OP_NO_TLSv1_3);

    /* We also want to ensure that we use RSA-based key exchange. */
    if (!TEST_true(SSL_CTX_set_cipher_list(cctx, "RSA")))
        goto end;

    if (!TEST_true(SSL_CTX_get_keylog_callback(cctx) == NULL)
            || !TEST_true(SSL_CTX_get_keylog_callback(sctx) == NULL))
        goto end;
    SSL_CTX_set_keylog_callback(cctx, client_keylog_callback);
    if (!TEST_true(SSL_CTX_get_keylog_callback(cctx)
                   == client_keylog_callback))
        goto end;
    SSL_CTX_set_keylog_callback(sctx, server_keylog_callback);
    if (!TEST_true(SSL_CTX_get_keylog_callback(sctx)
                   == server_keylog_callback))
        goto end;

    /* Now do a handshake and check that the logs have been written to. */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_false(error_writing_log)
            || !TEST_int_gt(client_log_buffer_index, 0)
            || !TEST_int_gt(server_log_buffer_index, 0))
        goto end;

    /*
     * Now we want to test that our output data was vaguely sensible. We
     * do that by using strtok and confirming that we have more or less the
     * data we expect. For both client and server, we expect to see one master
     * secret. The client should also see a RSA key exchange.
     */
    expected.rsa_key_exchange_count = 1;
    expected.master_secret_count = 1;
    if (!TEST_true(test_keylog_output(client_log_buffer, clientssl,
                                      SSL_get_session(clientssl), &expected)))
        goto end;

    expected.rsa_key_exchange_count = 0;
    if (!TEST_true(test_keylog_output(server_log_buffer, serverssl,
                                      SSL_get_session(serverssl), &expected)))
        goto end;

    testresult = 1;

end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}
#endif

#ifndef OPENSSL_NO_TLS1_3
static int test_keylog_no_master_key(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    SSL_SESSION *sess = NULL;
    int testresult = 0;
    struct sslapitest_log_counts expected = {0};
    unsigned char buf[1];
    size_t readbytes, written;

    /* Clean up logging space */
    memset(client_log_buffer, 0, sizeof(client_log_buffer));
    memset(server_log_buffer, 0, sizeof(server_log_buffer));
    client_log_buffer_index = 0;
    server_log_buffer_index = 0;
    error_writing_log = 0;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey))
        || !TEST_true(SSL_CTX_set_max_early_data(sctx,
                                                 SSL3_RT_MAX_PLAIN_LENGTH)))
        return 0;

    if (!TEST_true(SSL_CTX_get_keylog_callback(cctx) == NULL)
            || !TEST_true(SSL_CTX_get_keylog_callback(sctx) == NULL))
        goto end;

    SSL_CTX_set_keylog_callback(cctx, client_keylog_callback);
    if (!TEST_true(SSL_CTX_get_keylog_callback(cctx)
                   == client_keylog_callback))
        goto end;

    SSL_CTX_set_keylog_callback(sctx, server_keylog_callback);
    if (!TEST_true(SSL_CTX_get_keylog_callback(sctx)
                   == server_keylog_callback))
        goto end;

    /* Now do a handshake and check that the logs have been written to. */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_false(error_writing_log))
        goto end;

    /*
     * Now we want to test that our output data was vaguely sensible. For this
     * test, we expect no CLIENT_RANDOM entry because it doesn't make sense for
     * TLSv1.3, but we do expect both client and server to emit keys.
     */
    expected.client_handshake_secret_count = 1;
    expected.server_handshake_secret_count = 1;
    expected.client_application_secret_count = 1;
    expected.server_application_secret_count = 1;
    expected.exporter_secret_count = 1;
    if (!TEST_true(test_keylog_output(client_log_buffer, clientssl,
                                      SSL_get_session(clientssl), &expected))
            || !TEST_true(test_keylog_output(server_log_buffer, serverssl,
                                             SSL_get_session(serverssl),
                                             &expected)))
        goto end;

    /* Terminate old session and resume with early data. */
    sess = SSL_get1_session(clientssl);
    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

    /* Reset key log */
    memset(client_log_buffer, 0, sizeof(client_log_buffer));
    memset(server_log_buffer, 0, sizeof(server_log_buffer));
    client_log_buffer_index = 0;
    server_log_buffer_index = 0;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl, sess))
            /* Here writing 0 length early data is enough. */
            || !TEST_true(SSL_write_early_data(clientssl, NULL, 0, &written))
            || !TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                                &readbytes),
                            SSL_READ_EARLY_DATA_ERROR)
            || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                            SSL_EARLY_DATA_ACCEPTED)
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                          SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl)))
        goto end;

    /* In addition to the previous entries, expect early secrets. */
    expected.client_early_secret_count = 1;
    expected.early_exporter_secret_count = 1;
    if (!TEST_true(test_keylog_output(client_log_buffer, clientssl,
                                      SSL_get_session(clientssl), &expected))
            || !TEST_true(test_keylog_output(server_log_buffer, serverssl,
                                             SSL_get_session(serverssl),
                                             &expected)))
        goto end;

    testresult = 1;

end:
    SSL_SESSION_free(sess);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}
#endif

#ifndef OPENSSL_NO_TLS1_2
static int full_client_hello_callback(SSL *s, int *al, void *arg)
{
    int *ctr = arg;
    const unsigned char *p;
    int *exts;
    /* We only configure two ciphers, but the SCSV is added automatically. */
#ifdef OPENSSL_NO_EC
    const unsigned char expected_ciphers[] = {0x00, 0x9d, 0x00, 0xff};
#else
    const unsigned char expected_ciphers[] = {0x00, 0x9d, 0xc0,
                                              0x2c, 0x00, 0xff};
#endif
    const int expected_extensions[] = {
#ifndef OPENSSL_NO_EC
                                       11, 10,
#endif
                                       35, 22, 23, 13};
    size_t len;

    /* Make sure we can defer processing and get called back. */
    if ((*ctr)++ == 0)
        return SSL_CLIENT_HELLO_RETRY;

    len = SSL_client_hello_get0_ciphers(s, &p);
    if (!TEST_mem_eq(p, len, expected_ciphers, sizeof(expected_ciphers))
            || !TEST_size_t_eq(
                       SSL_client_hello_get0_compression_methods(s, &p), 1)
            || !TEST_int_eq(*p, 0))
        return SSL_CLIENT_HELLO_ERROR;
    if (!SSL_client_hello_get1_extensions_present(s, &exts, &len))
        return SSL_CLIENT_HELLO_ERROR;
    if (len != OSSL_NELEM(expected_extensions) ||
        memcmp(exts, expected_extensions, len * sizeof(*exts)) != 0) {
        printf("ClientHello callback expected extensions mismatch\n");
        OPENSSL_free(exts);
        return SSL_CLIENT_HELLO_ERROR;
    }
    OPENSSL_free(exts);
    return SSL_CLIENT_HELLO_SUCCESS;
}

static int test_client_hello_cb(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testctr = 0, testresult = 0;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        goto end;
    SSL_CTX_set_client_hello_cb(sctx, full_client_hello_callback, &testctr);

    /* The gimpy cipher list we configure can't do TLS 1.3. */
    SSL_CTX_set_max_proto_version(cctx, TLS1_2_VERSION);

    if (!TEST_true(SSL_CTX_set_cipher_list(cctx,
                        "AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384"))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                             &clientssl, NULL, NULL))
            || !TEST_false(create_ssl_connection(serverssl, clientssl,
                        SSL_ERROR_WANT_CLIENT_HELLO_CB))
                /*
                 * Passing a -1 literal is a hack since
                 * the real value was lost.
                 * */
            || !TEST_int_eq(SSL_get_error(serverssl, -1),
                            SSL_ERROR_WANT_CLIENT_HELLO_CB)
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    testresult = 1;

end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

/*
 * Very focused test to exercise a single case in the server-side state
 * machine, when the ChangeCipherState message needs to actually change
 * from one cipher to a different cipher (i.e., not changing from null
 * encryption to real encryption).
 */
static int test_ccs_change_cipher(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    SSL_SESSION *sess = NULL, *sesspre, *sesspost;
    int testresult = 0;
    int i;
    unsigned char buf;
    size_t readbytes;

    /*
     * Create a conection so we can resume and potentially (but not) use
     * a different cipher in the second connection.
     */
    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION, TLS1_2_VERSION,
                                       &sctx, &cctx, cert, privkey))
            || !TEST_true(SSL_CTX_set_options(sctx, SSL_OP_NO_TICKET))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                          NULL, NULL))
            || !TEST_true(SSL_set_cipher_list(clientssl, "AES128-GCM-SHA256"))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_ptr(sesspre = SSL_get0_session(serverssl))
            || !TEST_ptr(sess = SSL_get1_session(clientssl)))
        goto end;

    shutdown_ssl_connection(serverssl, clientssl);
    serverssl = clientssl = NULL;

    /* Resume, preferring a different cipher. Our server will force the
     * same cipher to be used as the initial handshake. */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                          NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl, sess))
            || !TEST_true(SSL_set_cipher_list(clientssl, "AES256-GCM-SHA384:AES128-GCM-SHA256"))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl))
            || !TEST_true(SSL_session_reused(serverssl))
            || !TEST_ptr(sesspost = SSL_get0_session(serverssl))
            || !TEST_ptr_eq(sesspre, sesspost)
            || !TEST_int_eq(TLS1_CK_RSA_WITH_AES_128_GCM_SHA256,
                            SSL_CIPHER_get_id(SSL_get_current_cipher(clientssl))))
        goto end;
    shutdown_ssl_connection(serverssl, clientssl);
    serverssl = clientssl = NULL;

    /*
     * Now create a fresh connection and try to renegotiate a different
     * cipher on it.
     */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL))
            || !TEST_true(SSL_set_cipher_list(clientssl, "AES128-GCM-SHA256"))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_ptr(sesspre = SSL_get0_session(serverssl))
            || !TEST_true(SSL_set_cipher_list(clientssl, "AES256-GCM-SHA384"))
            || !TEST_true(SSL_renegotiate(clientssl))
            || !TEST_true(SSL_renegotiate_pending(clientssl)))
        goto end;
    /* Actually drive the renegotiation. */
    for (i = 0; i < 3; i++) {
        if (SSL_read_ex(clientssl, &buf, sizeof(buf), &readbytes) > 0) {
            if (!TEST_ulong_eq(readbytes, 0))
                goto end;
        } else if (!TEST_int_eq(SSL_get_error(clientssl, 0),
                                SSL_ERROR_WANT_READ)) {
            goto end;
        }
        if (SSL_read_ex(serverssl, &buf, sizeof(buf), &readbytes) > 0) {
            if (!TEST_ulong_eq(readbytes, 0))
                goto end;
        } else if (!TEST_int_eq(SSL_get_error(serverssl, 0),
                                SSL_ERROR_WANT_READ)) {
            goto end;
        }
    }
    /* sesspre and sesspost should be different since the cipher changed. */
    if (!TEST_false(SSL_renegotiate_pending(clientssl))
            || !TEST_false(SSL_session_reused(clientssl))
            || !TEST_false(SSL_session_reused(serverssl))
            || !TEST_ptr(sesspost = SSL_get0_session(serverssl))
            || !TEST_ptr_ne(sesspre, sesspost)
            || !TEST_int_eq(TLS1_CK_RSA_WITH_AES_256_GCM_SHA384,
                            SSL_CIPHER_get_id(SSL_get_current_cipher(clientssl))))
        goto end;

    shutdown_ssl_connection(serverssl, clientssl);
    serverssl = clientssl = NULL;

    testresult = 1;

end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    SSL_SESSION_free(sess);

    return testresult;
}
#endif

static int execute_test_large_message(const SSL_METHOD *smeth,
                                      const SSL_METHOD *cmeth,
                                      int min_version, int max_version,
                                      int read_ahead)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    int i;
    BIO *certbio = NULL;
    X509 *chaincert = NULL;
    int certlen;

    if (!TEST_ptr(certbio = BIO_new_file(cert, "r")))
        goto end;
    chaincert = PEM_read_bio_X509(certbio, NULL, NULL, NULL);
    BIO_free(certbio);
    certbio = NULL;
    if (!TEST_ptr(chaincert))
        goto end;

    if (!TEST_true(create_ssl_ctx_pair(smeth, cmeth, min_version, max_version,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    if (read_ahead) {
        /*
         * Test that read_ahead works correctly when dealing with large
         * records
         */
        SSL_CTX_set_read_ahead(cctx, 1);
    }

    /*
     * We assume the supplied certificate is big enough so that if we add
     * NUM_EXTRA_CERTS it will make the overall message large enough. The
     * default buffer size is requested to be 16k, but due to the way BUF_MEM
     * works, it ends up allocating a little over 21k (16 * 4/3). So, in this
     * test we need to have a message larger than that.
     */
    certlen = i2d_X509(chaincert, NULL);
    OPENSSL_assert(certlen * NUM_EXTRA_CERTS >
                   (SSL3_RT_MAX_PLAIN_LENGTH * 4) / 3);
    for (i = 0; i < NUM_EXTRA_CERTS; i++) {
        if (!X509_up_ref(chaincert))
            goto end;
        if (!SSL_CTX_add_extra_chain_cert(sctx, chaincert)) {
            X509_free(chaincert);
            goto end;
        }
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    /*
     * Calling SSL_clear() first is not required but this tests that SSL_clear()
     * doesn't leak (when using enable-crypto-mdebug).
     */
    if (!TEST_true(SSL_clear(serverssl)))
        goto end;

    testresult = 1;
 end:
    X509_free(chaincert);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

static int test_large_message_tls(void)
{
    return execute_test_large_message(TLS_server_method(), TLS_client_method(),
                                      TLS1_VERSION, TLS_MAX_VERSION,
                                      0);
}

static int test_large_message_tls_read_ahead(void)
{
    return execute_test_large_message(TLS_server_method(), TLS_client_method(),
                                      TLS1_VERSION, TLS_MAX_VERSION,
                                      1);
}

#ifndef OPENSSL_NO_DTLS
static int test_large_message_dtls(void)
{
    /*
     * read_ahead is not relevant to DTLS because DTLS always acts as if
     * read_ahead is set.
     */
    return execute_test_large_message(DTLS_server_method(),
                                      DTLS_client_method(),
                                      DTLS1_VERSION, DTLS_MAX_VERSION,
                                      0);
}
#endif

#ifndef OPENSSL_NO_OCSP
static int ocsp_server_cb(SSL *s, void *arg)
{
    int *argi = (int *)arg;
    unsigned char *copy = NULL;
    STACK_OF(OCSP_RESPID) *ids = NULL;
    OCSP_RESPID *id = NULL;

    if (*argi == 2) {
        /* In this test we are expecting exactly 1 OCSP_RESPID */
        SSL_get_tlsext_status_ids(s, &ids);
        if (ids == NULL || sk_OCSP_RESPID_num(ids) != 1)
            return SSL_TLSEXT_ERR_ALERT_FATAL;

        id = sk_OCSP_RESPID_value(ids, 0);
        if (id == NULL || !OCSP_RESPID_match(id, ocspcert))
            return SSL_TLSEXT_ERR_ALERT_FATAL;
    } else if (*argi != 1) {
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    if (!TEST_ptr(copy = OPENSSL_memdup(orespder, sizeof(orespder))))
        return SSL_TLSEXT_ERR_ALERT_FATAL;

    SSL_set_tlsext_status_ocsp_resp(s, copy, sizeof(orespder));
    ocsp_server_called = 1;
    return SSL_TLSEXT_ERR_OK;
}

static int ocsp_client_cb(SSL *s, void *arg)
{
    int *argi = (int *)arg;
    const unsigned char *respderin;
    size_t len;

    if (*argi != 1 && *argi != 2)
        return 0;

    len = SSL_get_tlsext_status_ocsp_resp(s, &respderin);
    if (!TEST_mem_eq(orespder, len, respderin, len))
        return 0;

    ocsp_client_called = 1;
    return 1;
}

static int test_tlsext_status_type(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    STACK_OF(OCSP_RESPID) *ids = NULL;
    OCSP_RESPID *id = NULL;
    BIO *certbio = NULL;

    if (!create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                             TLS1_VERSION, TLS_MAX_VERSION,
                             &sctx, &cctx, cert, privkey))
        return 0;

    if (SSL_CTX_get_tlsext_status_type(cctx) != -1)
        goto end;

    /* First just do various checks getting and setting tlsext_status_type */

    clientssl = SSL_new(cctx);
    if (!TEST_int_eq(SSL_get_tlsext_status_type(clientssl), -1)
            || !TEST_true(SSL_set_tlsext_status_type(clientssl,
                                                      TLSEXT_STATUSTYPE_ocsp))
            || !TEST_int_eq(SSL_get_tlsext_status_type(clientssl),
                            TLSEXT_STATUSTYPE_ocsp))
        goto end;

    SSL_free(clientssl);
    clientssl = NULL;

    if (!SSL_CTX_set_tlsext_status_type(cctx, TLSEXT_STATUSTYPE_ocsp)
     || SSL_CTX_get_tlsext_status_type(cctx) != TLSEXT_STATUSTYPE_ocsp)
        goto end;

    clientssl = SSL_new(cctx);
    if (SSL_get_tlsext_status_type(clientssl) != TLSEXT_STATUSTYPE_ocsp)
        goto end;
    SSL_free(clientssl);
    clientssl = NULL;

    /*
     * Now actually do a handshake and check OCSP information is exchanged and
     * the callbacks get called
     */
    SSL_CTX_set_tlsext_status_cb(cctx, ocsp_client_cb);
    SSL_CTX_set_tlsext_status_arg(cctx, &cdummyarg);
    SSL_CTX_set_tlsext_status_cb(sctx, ocsp_server_cb);
    SSL_CTX_set_tlsext_status_arg(sctx, &cdummyarg);
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_true(ocsp_client_called)
            || !TEST_true(ocsp_server_called))
        goto end;
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = NULL;
    clientssl = NULL;

    /* Try again but this time force the server side callback to fail */
    ocsp_client_called = 0;
    ocsp_server_called = 0;
    cdummyarg = 0;
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
                /* This should fail because the callback will fail */
            || !TEST_false(create_ssl_connection(serverssl, clientssl,
                                                 SSL_ERROR_NONE))
            || !TEST_false(ocsp_client_called)
            || !TEST_false(ocsp_server_called))
        goto end;
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = NULL;
    clientssl = NULL;

    /*
     * This time we'll get the client to send an OCSP_RESPID that it will
     * accept.
     */
    ocsp_client_called = 0;
    ocsp_server_called = 0;
    cdummyarg = 2;
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL)))
        goto end;

    /*
     * We'll just use any old cert for this test - it doesn't have to be an OCSP
     * specific one. We'll use the server cert.
     */
    if (!TEST_ptr(certbio = BIO_new_file(cert, "r"))
            || !TEST_ptr(id = OCSP_RESPID_new())
            || !TEST_ptr(ids = sk_OCSP_RESPID_new_null())
            || !TEST_ptr(ocspcert = PEM_read_bio_X509(certbio,
                                                      NULL, NULL, NULL))
            || !TEST_true(OCSP_RESPID_set_by_key(id, ocspcert))
            || !TEST_true(sk_OCSP_RESPID_push(ids, id)))
        goto end;
    id = NULL;
    SSL_set_tlsext_status_ids(clientssl, ids);
    /* Control has been transferred */
    ids = NULL;

    BIO_free(certbio);
    certbio = NULL;

    if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                         SSL_ERROR_NONE))
            || !TEST_true(ocsp_client_called)
            || !TEST_true(ocsp_server_called))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    sk_OCSP_RESPID_pop_free(ids, OCSP_RESPID_free);
    OCSP_RESPID_free(id);
    BIO_free(certbio);
    X509_free(ocspcert);
    ocspcert = NULL;

    return testresult;
}
#endif

#if !defined(OPENSSL_NO_TLS1_3) || !defined(OPENSSL_NO_TLS1_2)
static int new_called, remove_called, get_called;

static int new_session_cb(SSL *ssl, SSL_SESSION *sess)
{
    new_called++;
    /*
     * sess has been up-refed for us, but we don't actually need it so free it
     * immediately.
     */
    SSL_SESSION_free(sess);
    return 1;
}

static void remove_session_cb(SSL_CTX *ctx, SSL_SESSION *sess)
{
    remove_called++;
}

static SSL_SESSION *get_sess_val = NULL;

static SSL_SESSION *get_session_cb(SSL *ssl, const unsigned char *id, int len,
                                   int *copy)
{
    get_called++;
    *copy = 1;
    return get_sess_val;
}

static int execute_test_session(int maxprot, int use_int_cache,
                                int use_ext_cache)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *serverssl1 = NULL, *clientssl1 = NULL;
    SSL *serverssl2 = NULL, *clientssl2 = NULL;
# ifndef OPENSSL_NO_TLS1_1
    SSL *serverssl3 = NULL, *clientssl3 = NULL;
# endif
    SSL_SESSION *sess1 = NULL, *sess2 = NULL;
    int testresult = 0, numnewsesstick = 1;

    new_called = remove_called = 0;

    /* TLSv1.3 sends 2 NewSessionTickets */
    if (maxprot == TLS1_3_VERSION)
        numnewsesstick = 2;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        return 0;

    /*
     * Only allow the max protocol version so we can force a connection failure
     * later
     */
    SSL_CTX_set_min_proto_version(cctx, maxprot);
    SSL_CTX_set_max_proto_version(cctx, maxprot);

    /* Set up session cache */
    if (use_ext_cache) {
        SSL_CTX_sess_set_new_cb(cctx, new_session_cb);
        SSL_CTX_sess_set_remove_cb(cctx, remove_session_cb);
    }
    if (use_int_cache) {
        /* Also covers instance where both are set */
        SSL_CTX_set_session_cache_mode(cctx, SSL_SESS_CACHE_CLIENT);
    } else {
        SSL_CTX_set_session_cache_mode(cctx,
                                       SSL_SESS_CACHE_CLIENT
                                       | SSL_SESS_CACHE_NO_INTERNAL_STORE);
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl1, &clientssl1,
                                      NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl1, clientssl1,
                                                SSL_ERROR_NONE))
            || !TEST_ptr(sess1 = SSL_get1_session(clientssl1)))
        goto end;

    /* Should fail because it should already be in the cache */
    if (use_int_cache && !TEST_false(SSL_CTX_add_session(cctx, sess1)))
        goto end;
    if (use_ext_cache
            && (!TEST_int_eq(new_called, numnewsesstick)

                || !TEST_int_eq(remove_called, 0)))
        goto end;

    new_called = remove_called = 0;
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl2,
                                      &clientssl2, NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl2, sess1))
            || !TEST_true(create_ssl_connection(serverssl2, clientssl2,
                                                SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl2)))
        goto end;

    if (maxprot == TLS1_3_VERSION) {
        /*
         * In TLSv1.3 we should have created a new session even though we have
         * resumed. Since we attempted a resume we should also have removed the
         * old ticket from the cache so that we try to only use tickets once.
         */
        if (use_ext_cache
                && (!TEST_int_eq(new_called, 1)
                    || !TEST_int_eq(remove_called, 1)))
            goto end;
    } else {
        /*
         * In TLSv1.2 we expect to have resumed so no sessions added or
         * removed.
         */
        if (use_ext_cache
                && (!TEST_int_eq(new_called, 0)
                    || !TEST_int_eq(remove_called, 0)))
            goto end;
    }

    SSL_SESSION_free(sess1);
    if (!TEST_ptr(sess1 = SSL_get1_session(clientssl2)))
        goto end;
    shutdown_ssl_connection(serverssl2, clientssl2);
    serverssl2 = clientssl2 = NULL;

    new_called = remove_called = 0;
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl2,
                                      &clientssl2, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl2, clientssl2,
                                                SSL_ERROR_NONE)))
        goto end;

    if (!TEST_ptr(sess2 = SSL_get1_session(clientssl2)))
        goto end;

    if (use_ext_cache
            && (!TEST_int_eq(new_called, numnewsesstick)
                || !TEST_int_eq(remove_called, 0)))
        goto end;

    new_called = remove_called = 0;
    /*
     * This should clear sess2 from the cache because it is a "bad" session.
     * See SSL_set_session() documentation.
     */
    if (!TEST_true(SSL_set_session(clientssl2, sess1)))
        goto end;
    if (use_ext_cache
            && (!TEST_int_eq(new_called, 0) || !TEST_int_eq(remove_called, 1)))
        goto end;
    if (!TEST_ptr_eq(SSL_get_session(clientssl2), sess1))
        goto end;

    if (use_int_cache) {
        /* Should succeeded because it should not already be in the cache */
        if (!TEST_true(SSL_CTX_add_session(cctx, sess2))
                || !TEST_true(SSL_CTX_remove_session(cctx, sess2)))
            goto end;
    }

    new_called = remove_called = 0;
    /* This shouldn't be in the cache so should fail */
    if (!TEST_false(SSL_CTX_remove_session(cctx, sess2)))
        goto end;

    if (use_ext_cache
            && (!TEST_int_eq(new_called, 0) || !TEST_int_eq(remove_called, 1)))
        goto end;

# if !defined(OPENSSL_NO_TLS1_1)
    new_called = remove_called = 0;
    /* Force a connection failure */
    SSL_CTX_set_max_proto_version(sctx, TLS1_1_VERSION);
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl3,
                                      &clientssl3, NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl3, sess1))
            /* This should fail because of the mismatched protocol versions */
            || !TEST_false(create_ssl_connection(serverssl3, clientssl3,
                                                 SSL_ERROR_NONE)))
        goto end;

    /* We should have automatically removed the session from the cache */
    if (use_ext_cache
            && (!TEST_int_eq(new_called, 0) || !TEST_int_eq(remove_called, 1)))
        goto end;

    /* Should succeed because it should not already be in the cache */
    if (use_int_cache && !TEST_true(SSL_CTX_add_session(cctx, sess2)))
        goto end;
# endif

    /* Now do some tests for server side caching */
    if (use_ext_cache) {
        SSL_CTX_sess_set_new_cb(cctx, NULL);
        SSL_CTX_sess_set_remove_cb(cctx, NULL);
        SSL_CTX_sess_set_new_cb(sctx, new_session_cb);
        SSL_CTX_sess_set_remove_cb(sctx, remove_session_cb);
        SSL_CTX_sess_set_get_cb(sctx, get_session_cb);
        get_sess_val = NULL;
    }

    SSL_CTX_set_session_cache_mode(cctx, 0);
    /* Internal caching is the default on the server side */
    if (!use_int_cache)
        SSL_CTX_set_session_cache_mode(sctx,
                                       SSL_SESS_CACHE_SERVER
                                       | SSL_SESS_CACHE_NO_INTERNAL_STORE);

    SSL_free(serverssl1);
    SSL_free(clientssl1);
    serverssl1 = clientssl1 = NULL;
    SSL_free(serverssl2);
    SSL_free(clientssl2);
    serverssl2 = clientssl2 = NULL;
    SSL_SESSION_free(sess1);
    sess1 = NULL;
    SSL_SESSION_free(sess2);
    sess2 = NULL;

    SSL_CTX_set_max_proto_version(sctx, maxprot);
    if (maxprot == TLS1_2_VERSION)
        SSL_CTX_set_options(sctx, SSL_OP_NO_TICKET);
    new_called = remove_called = get_called = 0;
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl1, &clientssl1,
                                      NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl1, clientssl1,
                                                SSL_ERROR_NONE))
            || !TEST_ptr(sess1 = SSL_get1_session(clientssl1))
            || !TEST_ptr(sess2 = SSL_get1_session(serverssl1)))
        goto end;

    if (use_int_cache) {
        if (maxprot == TLS1_3_VERSION && !use_ext_cache) {
            /*
             * In TLSv1.3 it should not have been added to the internal cache,
             * except in the case where we also have an external cache (in that
             * case it gets added to the cache in order to generate remove
             * events after timeout).
             */
            if (!TEST_false(SSL_CTX_remove_session(sctx, sess2)))
                goto end;
        } else {
            /* Should fail because it should already be in the cache */
            if (!TEST_false(SSL_CTX_add_session(sctx, sess2)))
                goto end;
        }
    }

    if (use_ext_cache) {
        SSL_SESSION *tmp = sess2;

        if (!TEST_int_eq(new_called, numnewsesstick)
                || !TEST_int_eq(remove_called, 0)
                || !TEST_int_eq(get_called, 0))
            goto end;
        /*
         * Delete the session from the internal cache to force a lookup from
         * the external cache. We take a copy first because
         * SSL_CTX_remove_session() also marks the session as non-resumable.
         */
        if (use_int_cache && maxprot != TLS1_3_VERSION) {
            if (!TEST_ptr(tmp = SSL_SESSION_dup(sess2))
                    || !TEST_true(SSL_CTX_remove_session(sctx, sess2)))
                goto end;
            SSL_SESSION_free(sess2);
        }
        sess2 = tmp;
    }

    new_called = remove_called = get_called = 0;
    get_sess_val = sess2;
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl2,
                                      &clientssl2, NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl2, sess1))
            || !TEST_true(create_ssl_connection(serverssl2, clientssl2,
                                                SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl2)))
        goto end;

    if (use_ext_cache) {
        if (!TEST_int_eq(remove_called, 0))
            goto end;

        if (maxprot == TLS1_3_VERSION) {
            if (!TEST_int_eq(new_called, 1)
                    || !TEST_int_eq(get_called, 0))
                goto end;
        } else {
            if (!TEST_int_eq(new_called, 0)
                    || !TEST_int_eq(get_called, 1))
                goto end;
        }
    }

    testresult = 1;

 end:
    SSL_free(serverssl1);
    SSL_free(clientssl1);
    SSL_free(serverssl2);
    SSL_free(clientssl2);
# ifndef OPENSSL_NO_TLS1_1
    SSL_free(serverssl3);
    SSL_free(clientssl3);
# endif
    SSL_SESSION_free(sess1);
    SSL_SESSION_free(sess2);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}
#endif /* !defined(OPENSSL_NO_TLS1_3) || !defined(OPENSSL_NO_TLS1_2) */

static int test_session_with_only_int_cache(void)
{
#ifndef OPENSSL_NO_TLS1_3
    if (!execute_test_session(TLS1_3_VERSION, 1, 0))
        return 0;
#endif

#ifndef OPENSSL_NO_TLS1_2
    return execute_test_session(TLS1_2_VERSION, 1, 0);
#else
    return 1;
#endif
}

static int test_session_with_only_ext_cache(void)
{
#ifndef OPENSSL_NO_TLS1_3
    if (!execute_test_session(TLS1_3_VERSION, 0, 1))
        return 0;
#endif

#ifndef OPENSSL_NO_TLS1_2
    return execute_test_session(TLS1_2_VERSION, 0, 1);
#else
    return 1;
#endif
}

static int test_session_with_both_cache(void)
{
#ifndef OPENSSL_NO_TLS1_3
    if (!execute_test_session(TLS1_3_VERSION, 1, 1))
        return 0;
#endif

#ifndef OPENSSL_NO_TLS1_2
    return execute_test_session(TLS1_2_VERSION, 1, 1);
#else
    return 1;
#endif
}

#ifndef OPENSSL_NO_TLS1_3
static SSL_SESSION *sesscache[6];
static int do_cache;

static int new_cachesession_cb(SSL *ssl, SSL_SESSION *sess)
{
    if (do_cache) {
        sesscache[new_called] = sess;
    } else {
        /* We don't need the reference to the session, so free it */
        SSL_SESSION_free(sess);
    }
    new_called++;

    return 1;
}

static int post_handshake_verify(SSL *sssl, SSL *cssl)
{
    SSL_set_verify(sssl, SSL_VERIFY_PEER, NULL);
    if (!TEST_true(SSL_verify_client_post_handshake(sssl)))
        return 0;

    /* Start handshake on the server and client */
    if (!TEST_int_eq(SSL_do_handshake(sssl), 1)
            || !TEST_int_le(SSL_read(cssl, NULL, 0), 0)
            || !TEST_int_le(SSL_read(sssl, NULL, 0), 0)
            || !TEST_true(create_ssl_connection(sssl, cssl,
                                                SSL_ERROR_NONE)))
        return 0;

    return 1;
}

static int setup_ticket_test(int stateful, int idx, SSL_CTX **sctx,
                             SSL_CTX **cctx)
{
    int sess_id_ctx = 1;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION, sctx,
                                       cctx, cert, privkey))
            || !TEST_true(SSL_CTX_set_num_tickets(*sctx, idx))
            || !TEST_true(SSL_CTX_set_session_id_context(*sctx,
                                                         (void *)&sess_id_ctx,
                                                         sizeof(sess_id_ctx))))
        return 0;

    if (stateful)
        SSL_CTX_set_options(*sctx, SSL_OP_NO_TICKET);

    SSL_CTX_set_session_cache_mode(*cctx, SSL_SESS_CACHE_CLIENT
                                          | SSL_SESS_CACHE_NO_INTERNAL_STORE);
    SSL_CTX_sess_set_new_cb(*cctx, new_cachesession_cb);

    return 1;
}

static int check_resumption(int idx, SSL_CTX *sctx, SSL_CTX *cctx, int succ)
{
    SSL *serverssl = NULL, *clientssl = NULL;
    int i;

    /* Test that we can resume with all the tickets we got given */
    for (i = 0; i < idx * 2; i++) {
        new_called = 0;
        if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                              &clientssl, NULL, NULL))
                || !TEST_true(SSL_set_session(clientssl, sesscache[i])))
            goto end;

        SSL_set_post_handshake_auth(clientssl, 1);

        if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                                    SSL_ERROR_NONE)))
            goto end;

        /*
         * Following a successful resumption we only get 1 ticket. After a
         * failed one we should get idx tickets.
         */
        if (succ) {
            if (!TEST_true(SSL_session_reused(clientssl))
                    || !TEST_int_eq(new_called, 1))
                goto end;
        } else {
            if (!TEST_false(SSL_session_reused(clientssl))
                    || !TEST_int_eq(new_called, idx))
                goto end;
        }

        new_called = 0;
        /* After a post-handshake authentication we should get 1 new ticket */
        if (succ
                && (!post_handshake_verify(serverssl, clientssl)
                    || !TEST_int_eq(new_called, 1)))
            goto end;

        SSL_shutdown(clientssl);
        SSL_shutdown(serverssl);
        SSL_free(serverssl);
        SSL_free(clientssl);
        serverssl = clientssl = NULL;
        SSL_SESSION_free(sesscache[i]);
        sesscache[i] = NULL;
    }

    return 1;

 end:
    SSL_free(clientssl);
    SSL_free(serverssl);
    return 0;
}

static int test_tickets(int stateful, int idx)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *serverssl = NULL, *clientssl = NULL;
    int testresult = 0;
    size_t j;

    /* idx is the test number, but also the number of tickets we want */

    new_called = 0;
    do_cache = 1;

    if (!setup_ticket_test(stateful, idx, &sctx, &cctx))
        goto end;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                          &clientssl, NULL, NULL)))
        goto end;

    if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
               /* Check we got the number of tickets we were expecting */
            || !TEST_int_eq(idx, new_called))
        goto end;

    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    clientssl = serverssl = NULL;
    sctx = cctx = NULL;

    /*
     * Now we try to resume with the tickets we previously created. The
     * resumption attempt is expected to fail (because we're now using a new
     * SSL_CTX). We should see idx number of tickets issued again.
     */

    /* Stop caching sessions - just count them */
    do_cache = 0;

    if (!setup_ticket_test(stateful, idx, &sctx, &cctx))
        goto end;

    if (!check_resumption(idx, sctx, cctx, 0))
        goto end;

    /* Start again with caching sessions */
    new_called = 0;
    do_cache = 1;
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    sctx = cctx = NULL;

    if (!setup_ticket_test(stateful, idx, &sctx, &cctx))
        goto end;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                          &clientssl, NULL, NULL)))
        goto end;

    SSL_set_post_handshake_auth(clientssl, 1);

    if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
               /* Check we got the number of tickets we were expecting */
            || !TEST_int_eq(idx, new_called))
        goto end;

    /* After a post-handshake authentication we should get new tickets issued */
    if (!post_handshake_verify(serverssl, clientssl)
            || !TEST_int_eq(idx * 2, new_called))
        goto end;

    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

    /* Stop caching sessions - just count them */
    do_cache = 0;

    /*
     * Check we can resume with all the tickets we created. This time around the
     * resumptions should all be successful.
     */
    if (!check_resumption(idx, sctx, cctx, 1))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    for (j = 0; j < OSSL_NELEM(sesscache); j++) {
        SSL_SESSION_free(sesscache[j]);
        sesscache[j] = NULL;
    }
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

static int test_stateless_tickets(int idx)
{
    return test_tickets(0, idx);
}

static int test_stateful_tickets(int idx)
{
    return test_tickets(1, idx);
}

static int test_psk_tickets(void)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *serverssl = NULL, *clientssl = NULL;
    int testresult = 0;
    int sess_id_ctx = 1;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION, &sctx,
                                       &cctx, NULL, NULL))
            || !TEST_true(SSL_CTX_set_session_id_context(sctx,
                                                         (void *)&sess_id_ctx,
                                                         sizeof(sess_id_ctx))))
        goto end;

    SSL_CTX_set_session_cache_mode(cctx, SSL_SESS_CACHE_CLIENT
                                         | SSL_SESS_CACHE_NO_INTERNAL_STORE);
    SSL_CTX_set_psk_use_session_callback(cctx, use_session_cb);
    SSL_CTX_set_psk_find_session_callback(sctx, find_session_cb);
    SSL_CTX_sess_set_new_cb(cctx, new_session_cb);
    use_session_cb_cnt = 0;
    find_session_cb_cnt = 0;
    srvid = pskid;
    new_called = 0;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL)))
        goto end;
    clientpsk = serverpsk = create_a_psk(clientssl);
    if (!TEST_ptr(clientpsk))
        goto end;
    SSL_SESSION_up_ref(clientpsk);

    if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_int_eq(1, find_session_cb_cnt)
            || !TEST_int_eq(1, use_session_cb_cnt)
               /* We should always get 1 ticket when using external PSK */
            || !TEST_int_eq(1, new_called))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;

    return testresult;
}
#endif

#define USE_NULL            0
#define USE_BIO_1           1
#define USE_BIO_2           2
#define USE_DEFAULT         3

#define CONNTYPE_CONNECTION_SUCCESS  0
#define CONNTYPE_CONNECTION_FAIL     1
#define CONNTYPE_NO_CONNECTION       2

#define TOTAL_NO_CONN_SSL_SET_BIO_TESTS         (3 * 3 * 3 * 3)
#define TOTAL_CONN_SUCCESS_SSL_SET_BIO_TESTS    (2 * 2)
#if !defined(OPENSSL_NO_TLS1_3) && !defined(OPENSSL_NO_TLS1_2)
# define TOTAL_CONN_FAIL_SSL_SET_BIO_TESTS       (2 * 2)
#else
# define TOTAL_CONN_FAIL_SSL_SET_BIO_TESTS       0
#endif

#define TOTAL_SSL_SET_BIO_TESTS TOTAL_NO_CONN_SSL_SET_BIO_TESTS \
                                + TOTAL_CONN_SUCCESS_SSL_SET_BIO_TESTS \
                                + TOTAL_CONN_FAIL_SSL_SET_BIO_TESTS

static void setupbio(BIO **res, BIO *bio1, BIO *bio2, int type)
{
    switch (type) {
    case USE_NULL:
        *res = NULL;
        break;
    case USE_BIO_1:
        *res = bio1;
        break;
    case USE_BIO_2:
        *res = bio2;
        break;
    }
}


/*
 * Tests calls to SSL_set_bio() under various conditions.
 *
 * For the first 3 * 3 * 3 * 3 = 81 tests we do 2 calls to SSL_set_bio() with
 * various combinations of valid BIOs or NULL being set for the rbio/wbio. We
 * then do more tests where we create a successful connection first using our
 * standard connection setup functions, and then call SSL_set_bio() with
 * various combinations of valid BIOs or NULL. We then repeat these tests
 * following a failed connection. In this last case we are looking to check that
 * SSL_set_bio() functions correctly in the case where s->bbio is not NULL.
 */
static int test_ssl_set_bio(int idx)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    BIO *bio1 = NULL;
    BIO *bio2 = NULL;
    BIO *irbio = NULL, *iwbio = NULL, *nrbio = NULL, *nwbio = NULL;
    SSL *serverssl = NULL, *clientssl = NULL;
    int initrbio, initwbio, newrbio, newwbio, conntype;
    int testresult = 0;

    if (idx < TOTAL_NO_CONN_SSL_SET_BIO_TESTS) {
        initrbio = idx % 3;
        idx /= 3;
        initwbio = idx % 3;
        idx /= 3;
        newrbio = idx % 3;
        idx /= 3;
        newwbio = idx % 3;
        conntype = CONNTYPE_NO_CONNECTION;
    } else {
        idx -= TOTAL_NO_CONN_SSL_SET_BIO_TESTS;
        initrbio = initwbio = USE_DEFAULT;
        newrbio = idx % 2;
        idx /= 2;
        newwbio = idx % 2;
        idx /= 2;
        conntype = idx % 2;
    }

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    if (conntype == CONNTYPE_CONNECTION_FAIL) {
        /*
         * We won't ever get here if either TLSv1.3 or TLSv1.2 is disabled
         * because we reduced the number of tests in the definition of
         * TOTAL_CONN_FAIL_SSL_SET_BIO_TESTS to avoid this scenario. By setting
         * mismatched protocol versions we will force a connection failure.
         */
        SSL_CTX_set_min_proto_version(sctx, TLS1_3_VERSION);
        SSL_CTX_set_max_proto_version(cctx, TLS1_2_VERSION);
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL)))
        goto end;

    if (initrbio == USE_BIO_1
            || initwbio == USE_BIO_1
            || newrbio == USE_BIO_1
            || newwbio == USE_BIO_1) {
        if (!TEST_ptr(bio1 = BIO_new(BIO_s_mem())))
            goto end;
    }

    if (initrbio == USE_BIO_2
            || initwbio == USE_BIO_2
            || newrbio == USE_BIO_2
            || newwbio == USE_BIO_2) {
        if (!TEST_ptr(bio2 = BIO_new(BIO_s_mem())))
            goto end;
    }

    if (initrbio != USE_DEFAULT) {
        setupbio(&irbio, bio1, bio2, initrbio);
        setupbio(&iwbio, bio1, bio2, initwbio);
        SSL_set_bio(clientssl, irbio, iwbio);

        /*
         * We want to maintain our own refs to these BIO, so do an up ref for
         * each BIO that will have ownership transferred in the SSL_set_bio()
         * call
         */
        if (irbio != NULL)
            BIO_up_ref(irbio);
        if (iwbio != NULL && iwbio != irbio)
            BIO_up_ref(iwbio);
    }

    if (conntype != CONNTYPE_NO_CONNECTION
            && !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)
                          == (conntype == CONNTYPE_CONNECTION_SUCCESS)))
        goto end;

    setupbio(&nrbio, bio1, bio2, newrbio);
    setupbio(&nwbio, bio1, bio2, newwbio);

    /*
     * We will (maybe) transfer ownership again so do more up refs.
     * SSL_set_bio() has some really complicated ownership rules where BIOs have
     * already been set!
     */
    if (nrbio != NULL
            && nrbio != irbio
            && (nwbio != iwbio || nrbio != nwbio))
        BIO_up_ref(nrbio);
    if (nwbio != NULL
            && nwbio != nrbio
            && (nwbio != iwbio || (nwbio == iwbio && irbio == iwbio)))
        BIO_up_ref(nwbio);

    SSL_set_bio(clientssl, nrbio, nwbio);

    testresult = 1;

 end:
    BIO_free(bio1);
    BIO_free(bio2);

    /*
     * This test is checking that the ref counting for SSL_set_bio is correct.
     * If we get here and we did too many frees then we will fail in the above
     * functions. If we haven't done enough then this will only be detected in
     * a crypto-mdebug build
     */
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

typedef enum { NO_BIO_CHANGE, CHANGE_RBIO, CHANGE_WBIO } bio_change_t;

static int execute_test_ssl_bio(int pop_ssl, bio_change_t change_bio)
{
    BIO *sslbio = NULL, *membio1 = NULL, *membio2 = NULL;
    SSL_CTX *ctx;
    SSL *ssl = NULL;
    int testresult = 0;

    if (!TEST_ptr(ctx = SSL_CTX_new(TLS_method()))
            || !TEST_ptr(ssl = SSL_new(ctx))
            || !TEST_ptr(sslbio = BIO_new(BIO_f_ssl()))
            || !TEST_ptr(membio1 = BIO_new(BIO_s_mem())))
        goto end;

    BIO_set_ssl(sslbio, ssl, BIO_CLOSE);

    /*
     * If anything goes wrong here then we could leak memory, so this will
     * be caught in a crypto-mdebug build
     */
    BIO_push(sslbio, membio1);

    /* Verify changing the rbio/wbio directly does not cause leaks */
    if (change_bio != NO_BIO_CHANGE) {
        if (!TEST_ptr(membio2 = BIO_new(BIO_s_mem()))) {
            ssl = NULL;
            goto end;
        }
        if (change_bio == CHANGE_RBIO)
            SSL_set0_rbio(ssl, membio2);
        else
            SSL_set0_wbio(ssl, membio2);
    }
    ssl = NULL;

    if (pop_ssl)
        BIO_pop(sslbio);
    else
        BIO_pop(membio1);

    testresult = 1;
 end:
    BIO_free(membio1);
    BIO_free(sslbio);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return testresult;
}

static int test_ssl_bio_pop_next_bio(void)
{
    return execute_test_ssl_bio(0, NO_BIO_CHANGE);
}

static int test_ssl_bio_pop_ssl_bio(void)
{
    return execute_test_ssl_bio(1, NO_BIO_CHANGE);
}

static int test_ssl_bio_change_rbio(void)
{
    return execute_test_ssl_bio(0, CHANGE_RBIO);
}

static int test_ssl_bio_change_wbio(void)
{
    return execute_test_ssl_bio(0, CHANGE_WBIO);
}

#if !defined(OPENSSL_NO_TLS1_2) || defined(OPENSSL_NO_TLS1_3)
typedef struct {
    /* The list of sig algs */
    const int *list;
    /* The length of the list */
    size_t listlen;
    /* A sigalgs list in string format */
    const char *liststr;
    /* Whether setting the list should succeed */
    int valid;
    /* Whether creating a connection with the list should succeed */
    int connsuccess;
} sigalgs_list;

static const int validlist1[] = {NID_sha256, EVP_PKEY_RSA};
# ifndef OPENSSL_NO_EC
static const int validlist2[] = {NID_sha256, EVP_PKEY_RSA, NID_sha512, EVP_PKEY_EC};
static const int validlist3[] = {NID_sha512, EVP_PKEY_EC};
# endif
static const int invalidlist1[] = {NID_undef, EVP_PKEY_RSA};
static const int invalidlist2[] = {NID_sha256, NID_undef};
static const int invalidlist3[] = {NID_sha256, EVP_PKEY_RSA, NID_sha256};
static const int invalidlist4[] = {NID_sha256};
static const sigalgs_list testsigalgs[] = {
    {validlist1, OSSL_NELEM(validlist1), NULL, 1, 1},
# ifndef OPENSSL_NO_EC
    {validlist2, OSSL_NELEM(validlist2), NULL, 1, 1},
    {validlist3, OSSL_NELEM(validlist3), NULL, 1, 0},
# endif
    {NULL, 0, "RSA+SHA256", 1, 1},
# ifndef OPENSSL_NO_EC
    {NULL, 0, "RSA+SHA256:ECDSA+SHA512", 1, 1},
    {NULL, 0, "ECDSA+SHA512", 1, 0},
# endif
    {invalidlist1, OSSL_NELEM(invalidlist1), NULL, 0, 0},
    {invalidlist2, OSSL_NELEM(invalidlist2), NULL, 0, 0},
    {invalidlist3, OSSL_NELEM(invalidlist3), NULL, 0, 0},
    {invalidlist4, OSSL_NELEM(invalidlist4), NULL, 0, 0},
    {NULL, 0, "RSA", 0, 0},
    {NULL, 0, "SHA256", 0, 0},
    {NULL, 0, "RSA+SHA256:SHA256", 0, 0},
    {NULL, 0, "Invalid", 0, 0}
};

static int test_set_sigalgs(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    const sigalgs_list *curr;
    int testctx;

    /* Should never happen */
    if (!TEST_size_t_le((size_t)idx, OSSL_NELEM(testsigalgs) * 2))
        return 0;

    testctx = ((size_t)idx < OSSL_NELEM(testsigalgs));
    curr = testctx ? &testsigalgs[idx]
                   : &testsigalgs[idx - OSSL_NELEM(testsigalgs)];

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        return 0;

    /*
     * TODO(TLS1.3): These APIs cannot set TLSv1.3 sig algs so we just test it
     * for TLSv1.2 for now until we add a new API.
     */
    SSL_CTX_set_max_proto_version(cctx, TLS1_2_VERSION);

    if (testctx) {
        int ret;

        if (curr->list != NULL)
            ret = SSL_CTX_set1_sigalgs(cctx, curr->list, curr->listlen);
        else
            ret = SSL_CTX_set1_sigalgs_list(cctx, curr->liststr);

        if (!ret) {
            if (curr->valid)
                TEST_info("Failure setting sigalgs in SSL_CTX (%d)\n", idx);
            else
                testresult = 1;
            goto end;
        }
        if (!curr->valid) {
            TEST_info("Not-failed setting sigalgs in SSL_CTX (%d)\n", idx);
            goto end;
        }
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL)))
        goto end;

    if (!testctx) {
        int ret;

        if (curr->list != NULL)
            ret = SSL_set1_sigalgs(clientssl, curr->list, curr->listlen);
        else
            ret = SSL_set1_sigalgs_list(clientssl, curr->liststr);
        if (!ret) {
            if (curr->valid)
                TEST_info("Failure setting sigalgs in SSL (%d)\n", idx);
            else
                testresult = 1;
            goto end;
        }
        if (!curr->valid)
            goto end;
    }

    if (!TEST_int_eq(create_ssl_connection(serverssl, clientssl,
                                           SSL_ERROR_NONE),
                curr->connsuccess))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}
#endif

#ifndef OPENSSL_NO_TLS1_3
static int psk_client_cb_cnt = 0;
static int psk_server_cb_cnt = 0;

static int use_session_cb(SSL *ssl, const EVP_MD *md, const unsigned char **id,
                          size_t *idlen, SSL_SESSION **sess)
{
    switch (++use_session_cb_cnt) {
    case 1:
        /* The first call should always have a NULL md */
        if (md != NULL)
            return 0;
        break;

    case 2:
        /* The second call should always have an md */
        if (md == NULL)
            return 0;
        break;

    default:
        /* We should only be called a maximum of twice */
        return 0;
    }

    if (clientpsk != NULL)
        SSL_SESSION_up_ref(clientpsk);

    *sess = clientpsk;
    *id = (const unsigned char *)pskid;
    *idlen = strlen(pskid);

    return 1;
}

#ifndef OPENSSL_NO_PSK
static unsigned int psk_client_cb(SSL *ssl, const char *hint, char *id,
                                  unsigned int max_id_len,
                                  unsigned char *psk,
                                  unsigned int max_psk_len)
{
    unsigned int psklen = 0;

    psk_client_cb_cnt++;

    if (strlen(pskid) + 1 > max_id_len)
        return 0;

    /* We should only ever be called a maximum of twice per connection */
    if (psk_client_cb_cnt > 2)
        return 0;

    if (clientpsk == NULL)
        return 0;

    /* We'll reuse the PSK we set up for TLSv1.3 */
    if (SSL_SESSION_get_master_key(clientpsk, NULL, 0) > max_psk_len)
        return 0;
    psklen = SSL_SESSION_get_master_key(clientpsk, psk, max_psk_len);
    strncpy(id, pskid, max_id_len);

    return psklen;
}
#endif /* OPENSSL_NO_PSK */

static int find_session_cb(SSL *ssl, const unsigned char *identity,
                           size_t identity_len, SSL_SESSION **sess)
{
    find_session_cb_cnt++;

    /* We should only ever be called a maximum of twice per connection */
    if (find_session_cb_cnt > 2)
        return 0;

    if (serverpsk == NULL)
        return 0;

    /* Identity should match that set by the client */
    if (strlen(srvid) != identity_len
            || strncmp(srvid, (const char *)identity, identity_len) != 0) {
        /* No PSK found, continue but without a PSK */
        *sess = NULL;
        return 1;
    }

    SSL_SESSION_up_ref(serverpsk);
    *sess = serverpsk;

    return 1;
}

#ifndef OPENSSL_NO_PSK
static unsigned int psk_server_cb(SSL *ssl, const char *identity,
                                  unsigned char *psk, unsigned int max_psk_len)
{
    unsigned int psklen = 0;

    psk_server_cb_cnt++;

    /* We should only ever be called a maximum of twice per connection */
    if (find_session_cb_cnt > 2)
        return 0;

    if (serverpsk == NULL)
        return 0;

    /* Identity should match that set by the client */
    if (strcmp(srvid, identity) != 0) {
        return 0;
    }

    /* We'll reuse the PSK we set up for TLSv1.3 */
    if (SSL_SESSION_get_master_key(serverpsk, NULL, 0) > max_psk_len)
        return 0;
    psklen = SSL_SESSION_get_master_key(serverpsk, psk, max_psk_len);

    return psklen;
}
#endif /* OPENSSL_NO_PSK */

#define MSG1    "Hello"
#define MSG2    "World."
#define MSG3    "This"
#define MSG4    "is"
#define MSG5    "a"
#define MSG6    "test"
#define MSG7    "message."

#define TLS13_AES_128_GCM_SHA256_BYTES  ((const unsigned char *)"\x13\x01")
#define TLS13_AES_256_GCM_SHA384_BYTES  ((const unsigned char *)"\x13\x02")
#define TLS13_CHACHA20_POLY1305_SHA256_BYTES ((const unsigned char *)"\x13\x03")
#define TLS13_AES_128_CCM_SHA256_BYTES ((const unsigned char *)"\x13\x04")
#define TLS13_AES_128_CCM_8_SHA256_BYTES ((const unsigned char *)"\x13\05")


static SSL_SESSION *create_a_psk(SSL *ssl)
{
    const SSL_CIPHER *cipher = NULL;
    const unsigned char key[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
        0x2c, 0x2d, 0x2e, 0x2f
    };
    SSL_SESSION *sess = NULL;

    cipher = SSL_CIPHER_find(ssl, TLS13_AES_256_GCM_SHA384_BYTES);
    sess = SSL_SESSION_new();
    if (!TEST_ptr(sess)
            || !TEST_ptr(cipher)
            || !TEST_true(SSL_SESSION_set1_master_key(sess, key,
                                                      sizeof(key)))
            || !TEST_true(SSL_SESSION_set_cipher(sess, cipher))
            || !TEST_true(
                    SSL_SESSION_set_protocol_version(sess,
                                                     TLS1_3_VERSION))) {
        SSL_SESSION_free(sess);
        return NULL;
    }
    return sess;
}

/*
 * Helper method to setup objects for early data test. Caller frees objects on
 * error.
 */
static int setupearly_data_test(SSL_CTX **cctx, SSL_CTX **sctx, SSL **clientssl,
                                SSL **serverssl, SSL_SESSION **sess, int idx)
{
    if (*sctx == NULL
            && !TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                              TLS_client_method(),
                                              TLS1_VERSION, TLS_MAX_VERSION,
                                              sctx, cctx, cert, privkey)))
        return 0;

    if (!TEST_true(SSL_CTX_set_max_early_data(*sctx, SSL3_RT_MAX_PLAIN_LENGTH)))
        return 0;

    if (idx == 1) {
        /* When idx == 1 we repeat the tests with read_ahead set */
        SSL_CTX_set_read_ahead(*cctx, 1);
        SSL_CTX_set_read_ahead(*sctx, 1);
    } else if (idx == 2) {
        /* When idx == 2 we are doing early_data with a PSK. Set up callbacks */
        SSL_CTX_set_psk_use_session_callback(*cctx, use_session_cb);
        SSL_CTX_set_psk_find_session_callback(*sctx, find_session_cb);
        use_session_cb_cnt = 0;
        find_session_cb_cnt = 0;
        srvid = pskid;
    }

    if (!TEST_true(create_ssl_objects(*sctx, *cctx, serverssl, clientssl,
                                      NULL, NULL)))
        return 0;

    /*
     * For one of the run throughs (doesn't matter which one), we'll try sending
     * some SNI data in the initial ClientHello. This will be ignored (because
     * there is no SNI cb set up by the server), so it should not impact
     * early_data.
     */
    if (idx == 1
            && !TEST_true(SSL_set_tlsext_host_name(*clientssl, "localhost")))
        return 0;

    if (idx == 2) {
        clientpsk = create_a_psk(*clientssl);
        if (!TEST_ptr(clientpsk)
                   /*
                    * We just choose an arbitrary value for max_early_data which
                    * should be big enough for testing purposes.
                    */
                || !TEST_true(SSL_SESSION_set_max_early_data(clientpsk,
                                                             0x100))
                || !TEST_true(SSL_SESSION_up_ref(clientpsk))) {
            SSL_SESSION_free(clientpsk);
            clientpsk = NULL;
            return 0;
        }
        serverpsk = clientpsk;

        if (sess != NULL) {
            if (!TEST_true(SSL_SESSION_up_ref(clientpsk))) {
                SSL_SESSION_free(clientpsk);
                SSL_SESSION_free(serverpsk);
                clientpsk = serverpsk = NULL;
                return 0;
            }
            *sess = clientpsk;
        }
        return 1;
    }

    if (sess == NULL)
        return 1;

    if (!TEST_true(create_ssl_connection(*serverssl, *clientssl,
                                         SSL_ERROR_NONE)))
        return 0;

    *sess = SSL_get1_session(*clientssl);
    SSL_shutdown(*clientssl);
    SSL_shutdown(*serverssl);
    SSL_free(*serverssl);
    SSL_free(*clientssl);
    *serverssl = *clientssl = NULL;

    if (!TEST_true(create_ssl_objects(*sctx, *cctx, serverssl,
                                      clientssl, NULL, NULL))
            || !TEST_true(SSL_set_session(*clientssl, *sess)))
        return 0;

    return 1;
}

static int test_early_data_read_write(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    SSL_SESSION *sess = NULL;
    unsigned char buf[20], data[1024];
    size_t readbytes, written, eoedlen, rawread, rawwritten;
    BIO *rbio;

    if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl,
                                        &serverssl, &sess, idx)))
        goto end;

    /* Write and read some early data */
    if (!TEST_true(SSL_write_early_data(clientssl, MSG1, strlen(MSG1),
                                        &written))
            || !TEST_size_t_eq(written, strlen(MSG1))
            || !TEST_int_eq(SSL_read_early_data(serverssl, buf,
                                                sizeof(buf), &readbytes),
                            SSL_READ_EARLY_DATA_SUCCESS)
            || !TEST_mem_eq(MSG1, readbytes, buf, strlen(MSG1))
            || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                            SSL_EARLY_DATA_ACCEPTED))
        goto end;

    /*
     * Server should be able to write data, and client should be able to
     * read it.
     */
    if (!TEST_true(SSL_write_early_data(serverssl, MSG2, strlen(MSG2),
                                        &written))
            || !TEST_size_t_eq(written, strlen(MSG2))
            || !TEST_true(SSL_read_ex(clientssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG2, strlen(MSG2)))
        goto end;

    /* Even after reading normal data, client should be able write early data */
    if (!TEST_true(SSL_write_early_data(clientssl, MSG3, strlen(MSG3),
                                        &written))
            || !TEST_size_t_eq(written, strlen(MSG3)))
        goto end;

    /* Server should still be able read early data after writing data */
    if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                         &readbytes),
                     SSL_READ_EARLY_DATA_SUCCESS)
            || !TEST_mem_eq(buf, readbytes, MSG3, strlen(MSG3)))
        goto end;

    /* Write more data from server and read it from client */
    if (!TEST_true(SSL_write_early_data(serverssl, MSG4, strlen(MSG4),
                                        &written))
            || !TEST_size_t_eq(written, strlen(MSG4))
            || !TEST_true(SSL_read_ex(clientssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG4, strlen(MSG4)))
        goto end;

    /*
     * If client writes normal data it should mean writing early data is no
     * longer possible.
     */
    if (!TEST_true(SSL_write_ex(clientssl, MSG5, strlen(MSG5), &written))
            || !TEST_size_t_eq(written, strlen(MSG5))
            || !TEST_int_eq(SSL_get_early_data_status(clientssl),
                            SSL_EARLY_DATA_ACCEPTED))
        goto end;

    /*
     * At this point the client has written EndOfEarlyData, ClientFinished and
     * normal (fully protected) data. We are going to cause a delay between the
     * arrival of EndOfEarlyData and ClientFinished. We read out all the data
     * in the read BIO, and then just put back the EndOfEarlyData message.
     */
    rbio = SSL_get_rbio(serverssl);
    if (!TEST_true(BIO_read_ex(rbio, data, sizeof(data), &rawread))
            || !TEST_size_t_lt(rawread, sizeof(data))
            || !TEST_size_t_gt(rawread, SSL3_RT_HEADER_LENGTH))
        goto end;

    /* Record length is in the 4th and 5th bytes of the record header */
    eoedlen = SSL3_RT_HEADER_LENGTH + (data[3] << 8 | data[4]);
    if (!TEST_true(BIO_write_ex(rbio, data, eoedlen, &rawwritten))
            || !TEST_size_t_eq(rawwritten, eoedlen))
        goto end;

    /* Server should be told that there is no more early data */
    if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                         &readbytes),
                     SSL_READ_EARLY_DATA_FINISH)
            || !TEST_size_t_eq(readbytes, 0))
        goto end;

    /*
     * Server has not finished init yet, so should still be able to write early
     * data.
     */
    if (!TEST_true(SSL_write_early_data(serverssl, MSG6, strlen(MSG6),
                                        &written))
            || !TEST_size_t_eq(written, strlen(MSG6)))
        goto end;

    /* Push the ClientFinished and the normal data back into the server rbio */
    if (!TEST_true(BIO_write_ex(rbio, data + eoedlen, rawread - eoedlen,
                                &rawwritten))
            || !TEST_size_t_eq(rawwritten, rawread - eoedlen))
        goto end;

    /* Server should be able to read normal data */
    if (!TEST_true(SSL_read_ex(serverssl, buf, sizeof(buf), &readbytes))
            || !TEST_size_t_eq(readbytes, strlen(MSG5)))
        goto end;

    /* Client and server should not be able to write/read early data now */
    if (!TEST_false(SSL_write_early_data(clientssl, MSG6, strlen(MSG6),
                                         &written)))
        goto end;
    ERR_clear_error();
    if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                         &readbytes),
                     SSL_READ_EARLY_DATA_ERROR))
        goto end;
    ERR_clear_error();

    /* Client should be able to read the data sent by the server */
    if (!TEST_true(SSL_read_ex(clientssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG6, strlen(MSG6)))
        goto end;

    /*
     * Make sure we process the two NewSessionTickets. These arrive
     * post-handshake. We attempt reads which we do not expect to return any
     * data.
     */
    if (!TEST_false(SSL_read_ex(clientssl, buf, sizeof(buf), &readbytes))
            || !TEST_false(SSL_read_ex(clientssl, buf, sizeof(buf),
                           &readbytes)))
        goto end;

    /* Server should be able to write normal data */
    if (!TEST_true(SSL_write_ex(serverssl, MSG7, strlen(MSG7), &written))
            || !TEST_size_t_eq(written, strlen(MSG7))
            || !TEST_true(SSL_read_ex(clientssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG7, strlen(MSG7)))
        goto end;

    SSL_SESSION_free(sess);
    sess = SSL_get1_session(clientssl);
    use_session_cb_cnt = 0;
    find_session_cb_cnt = 0;

    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl, sess)))
        goto end;

    /* Write and read some early data */
    if (!TEST_true(SSL_write_early_data(clientssl, MSG1, strlen(MSG1),
                                        &written))
            || !TEST_size_t_eq(written, strlen(MSG1))
            || !TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                                &readbytes),
                            SSL_READ_EARLY_DATA_SUCCESS)
            || !TEST_mem_eq(buf, readbytes, MSG1, strlen(MSG1)))
        goto end;

    if (!TEST_int_gt(SSL_connect(clientssl), 0)
            || !TEST_int_gt(SSL_accept(serverssl), 0))
        goto end;

    /* Client and server should not be able to write/read early data now */
    if (!TEST_false(SSL_write_early_data(clientssl, MSG6, strlen(MSG6),
                                         &written)))
        goto end;
    ERR_clear_error();
    if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                         &readbytes),
                     SSL_READ_EARLY_DATA_ERROR))
        goto end;
    ERR_clear_error();

    /* Client and server should be able to write/read normal data */
    if (!TEST_true(SSL_write_ex(clientssl, MSG5, strlen(MSG5), &written))
            || !TEST_size_t_eq(written, strlen(MSG5))
            || !TEST_true(SSL_read_ex(serverssl, buf, sizeof(buf), &readbytes))
            || !TEST_size_t_eq(readbytes, strlen(MSG5)))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(sess);
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

static int allow_ed_cb_called = 0;

static int allow_early_data_cb(SSL *s, void *arg)
{
    int *usecb = (int *)arg;

    allow_ed_cb_called++;

    if (*usecb == 1)
        return 0;

    return 1;
}

/*
 * idx == 0: Standard early_data setup
 * idx == 1: early_data setup using read_ahead
 * usecb == 0: Don't use a custom early data callback
 * usecb == 1: Use a custom early data callback and reject the early data
 * usecb == 2: Use a custom early data callback and accept the early data
 * confopt == 0: Configure anti-replay directly
 * confopt == 1: Configure anti-replay using SSL_CONF
 */
static int test_early_data_replay_int(int idx, int usecb, int confopt)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    SSL_SESSION *sess = NULL;
    size_t readbytes, written;
    unsigned char buf[20];

    allow_ed_cb_called = 0;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION, &sctx,
                                       &cctx, cert, privkey)))
        return 0;

    if (usecb > 0) {
        if (confopt == 0) {
            SSL_CTX_set_options(sctx, SSL_OP_NO_ANTI_REPLAY);
        } else {
            SSL_CONF_CTX *confctx = SSL_CONF_CTX_new();

            if (!TEST_ptr(confctx))
                goto end;
            SSL_CONF_CTX_set_flags(confctx, SSL_CONF_FLAG_FILE
                                            | SSL_CONF_FLAG_SERVER);
            SSL_CONF_CTX_set_ssl_ctx(confctx, sctx);
            if (!TEST_int_eq(SSL_CONF_cmd(confctx, "Options", "-AntiReplay"),
                             2)) {
                SSL_CONF_CTX_free(confctx);
                goto end;
            }
            SSL_CONF_CTX_free(confctx);
        }
        SSL_CTX_set_allow_early_data_cb(sctx, allow_early_data_cb, &usecb);
    }

    if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl,
                                        &serverssl, &sess, idx)))
        goto end;

    /*
     * The server is configured to accept early data. Create a connection to
     * "use up" the ticket
     */
    if (!TEST_true(create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl)))
        goto end;

    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl, sess)))
        goto end;

    /* Write and read some early data */
    if (!TEST_true(SSL_write_early_data(clientssl, MSG1, strlen(MSG1),
                                        &written))
            || !TEST_size_t_eq(written, strlen(MSG1)))
        goto end;

    if (usecb <= 1) {
        if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                             &readbytes),
                         SSL_READ_EARLY_DATA_FINISH)
                   /*
                    * The ticket was reused, so the we should have rejected the
                    * early data
                    */
                || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                                SSL_EARLY_DATA_REJECTED))
            goto end;
    } else {
        /* In this case the callback decides to accept the early data */
        if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                             &readbytes),
                         SSL_READ_EARLY_DATA_SUCCESS)
                || !TEST_mem_eq(MSG1, strlen(MSG1), buf, readbytes)
                   /*
                    * Server will have sent its flight so client can now send
                    * end of early data and complete its half of the handshake
                    */
                || !TEST_int_gt(SSL_connect(clientssl), 0)
                || !TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                             &readbytes),
                                SSL_READ_EARLY_DATA_FINISH)
                || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                                SSL_EARLY_DATA_ACCEPTED))
            goto end;
    }

    /* Complete the connection */
    if (!TEST_true(create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE))
            || !TEST_int_eq(SSL_session_reused(clientssl), (usecb > 0) ? 1 : 0)
            || !TEST_int_eq(allow_ed_cb_called, usecb > 0 ? 1 : 0))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(sess);
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

static int test_early_data_replay(int idx)
{
    int ret = 1, usecb, confopt;

    for (usecb = 0; usecb < 3; usecb++) {
        for (confopt = 0; confopt < 2; confopt++)
            ret &= test_early_data_replay_int(idx, usecb, confopt);
    }

    return ret;
}

/*
 * Helper function to test that a server attempting to read early data can
 * handle a connection from a client where the early data should be skipped.
 * testtype: 0 == No HRR
 * testtype: 1 == HRR
 * testtype: 2 == HRR, invalid early_data sent after HRR
 * testtype: 3 == recv_max_early_data set to 0
 */
static int early_data_skip_helper(int testtype, int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    SSL_SESSION *sess = NULL;
    unsigned char buf[20];
    size_t readbytes, written;

    if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl,
                                        &serverssl, &sess, idx)))
        goto end;

    if (testtype == 1 || testtype == 2) {
        /* Force an HRR to occur */
        if (!TEST_true(SSL_set1_groups_list(serverssl, "P-256")))
            goto end;
    } else if (idx == 2) {
        /*
         * We force early_data rejection by ensuring the PSK identity is
         * unrecognised
         */
        srvid = "Dummy Identity";
    } else {
        /*
         * Deliberately corrupt the creation time. We take 20 seconds off the
         * time. It could be any value as long as it is not within tolerance.
         * This should mean the ticket is rejected.
         */
        if (!TEST_true(SSL_SESSION_set_time(sess, (long)(time(NULL) - 20))))
            goto end;
    }

    if (testtype == 3
            && !TEST_true(SSL_set_recv_max_early_data(serverssl, 0)))
        goto end;

    /* Write some early data */
    if (!TEST_true(SSL_write_early_data(clientssl, MSG1, strlen(MSG1),
                                        &written))
            || !TEST_size_t_eq(written, strlen(MSG1)))
        goto end;

    /* Server should reject the early data */
    if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                         &readbytes),
                     SSL_READ_EARLY_DATA_FINISH)
            || !TEST_size_t_eq(readbytes, 0)
            || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                            SSL_EARLY_DATA_REJECTED))
        goto end;

    switch (testtype) {
    case 0:
        /* Nothing to do */
        break;

    case 1:
        /*
         * Finish off the handshake. We perform the same writes and reads as
         * further down but we expect them to fail due to the incomplete
         * handshake.
         */
        if (!TEST_false(SSL_write_ex(clientssl, MSG2, strlen(MSG2), &written))
                || !TEST_false(SSL_read_ex(serverssl, buf, sizeof(buf),
                               &readbytes)))
            goto end;
        break;

    case 2:
        {
            BIO *wbio = SSL_get_wbio(clientssl);
            /* A record that will appear as bad early_data */
            const unsigned char bad_early_data[] = {
                0x17, 0x03, 0x03, 0x00, 0x01, 0x00
            };

            /*
             * We force the client to attempt a write. This will fail because
             * we're still in the handshake. It will cause the second
             * ClientHello to be sent.
             */
            if (!TEST_false(SSL_write_ex(clientssl, MSG2, strlen(MSG2),
                                         &written)))
                goto end;

            /*
             * Inject some early_data after the second ClientHello. This should
             * cause the server to fail
             */
            if (!TEST_true(BIO_write_ex(wbio, bad_early_data,
                                        sizeof(bad_early_data), &written)))
                goto end;
        }
        /* fallthrough */

    case 3:
        /*
         * This client has sent more early_data than we are willing to skip
         * (case 3) or sent invalid early_data (case 2) so the connection should
         * abort.
         */
        if (!TEST_false(SSL_read_ex(serverssl, buf, sizeof(buf), &readbytes))
                || !TEST_int_eq(SSL_get_error(serverssl, 0), SSL_ERROR_SSL))
            goto end;

        /* Connection has failed - nothing more to do */
        testresult = 1;
        goto end;

    default:
        TEST_error("Invalid test type");
        goto end;
    }

    /*
     * Should be able to send normal data despite rejection of early data. The
     * early_data should be skipped.
     */
    if (!TEST_true(SSL_write_ex(clientssl, MSG2, strlen(MSG2), &written))
            || !TEST_size_t_eq(written, strlen(MSG2))
            || !TEST_int_eq(SSL_get_early_data_status(clientssl),
                            SSL_EARLY_DATA_REJECTED)
            || !TEST_true(SSL_read_ex(serverssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG2, strlen(MSG2)))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    SSL_SESSION_free(sess);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

/*
 * Test that a server attempting to read early data can handle a connection
 * from a client where the early data is not acceptable.
 */
static int test_early_data_skip(int idx)
{
    return early_data_skip_helper(0, idx);
}

/*
 * Test that a server attempting to read early data can handle a connection
 * from a client where an HRR occurs.
 */
static int test_early_data_skip_hrr(int idx)
{
    return early_data_skip_helper(1, idx);
}

/*
 * Test that a server attempting to read early data can handle a connection
 * from a client where an HRR occurs and correctly fails if early_data is sent
 * after the HRR
 */
static int test_early_data_skip_hrr_fail(int idx)
{
    return early_data_skip_helper(2, idx);
}

/*
 * Test that a server attempting to read early data will abort if it tries to
 * skip over too much.
 */
static int test_early_data_skip_abort(int idx)
{
    return early_data_skip_helper(3, idx);
}

/*
 * Test that a server attempting to read early data can handle a connection
 * from a client that doesn't send any.
 */
static int test_early_data_not_sent(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    SSL_SESSION *sess = NULL;
    unsigned char buf[20];
    size_t readbytes, written;

    if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl,
                                        &serverssl, &sess, idx)))
        goto end;

    /* Write some data - should block due to handshake with server */
    SSL_set_connect_state(clientssl);
    if (!TEST_false(SSL_write_ex(clientssl, MSG1, strlen(MSG1), &written)))
        goto end;

    /* Server should detect that early data has not been sent */
    if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                         &readbytes),
                     SSL_READ_EARLY_DATA_FINISH)
            || !TEST_size_t_eq(readbytes, 0)
            || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                            SSL_EARLY_DATA_NOT_SENT)
            || !TEST_int_eq(SSL_get_early_data_status(clientssl),
                            SSL_EARLY_DATA_NOT_SENT))
        goto end;

    /* Continue writing the message we started earlier */
    if (!TEST_true(SSL_write_ex(clientssl, MSG1, strlen(MSG1), &written))
            || !TEST_size_t_eq(written, strlen(MSG1))
            || !TEST_true(SSL_read_ex(serverssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG1, strlen(MSG1))
            || !SSL_write_ex(serverssl, MSG2, strlen(MSG2), &written)
            || !TEST_size_t_eq(written, strlen(MSG2)))
        goto end;

    if (!TEST_true(SSL_read_ex(clientssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG2, strlen(MSG2)))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(sess);
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

static const char *servalpn;

static int alpn_select_cb(SSL *ssl, const unsigned char **out,
                          unsigned char *outlen, const unsigned char *in,
                          unsigned int inlen, void *arg)
{
    unsigned int protlen = 0;
    const unsigned char *prot;

    for (prot = in; prot < in + inlen; prot += protlen) {
        protlen = *prot++;
        if (in + inlen < prot + protlen)
            return SSL_TLSEXT_ERR_NOACK;

        if (protlen == strlen(servalpn)
                && memcmp(prot, servalpn, protlen) == 0) {
            *out = prot;
            *outlen = protlen;
            return SSL_TLSEXT_ERR_OK;
        }
    }

    return SSL_TLSEXT_ERR_NOACK;
}

/* Test that a PSK can be used to send early_data */
static int test_early_data_psk(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    SSL_SESSION *sess = NULL;
    unsigned char alpnlist[] = {
        0x08, 'g', 'o', 'o', 'd', 'a', 'l', 'p', 'n', 0x07, 'b', 'a', 'd', 'a',
        'l', 'p', 'n'
    };
#define GOODALPNLEN     9
#define BADALPNLEN      8
#define GOODALPN        (alpnlist)
#define BADALPN         (alpnlist + GOODALPNLEN)
    int err = 0;
    unsigned char buf[20];
    size_t readbytes, written;
    int readearlyres = SSL_READ_EARLY_DATA_SUCCESS, connectres = 1;
    int edstatus = SSL_EARLY_DATA_ACCEPTED;

    /* We always set this up with a final parameter of "2" for PSK */
    if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl,
                                        &serverssl, &sess, 2)))
        goto end;

    servalpn = "goodalpn";

    /*
     * Note: There is no test for inconsistent SNI with late client detection.
     * This is because servers do not acknowledge SNI even if they are using
     * it in a resumption handshake - so it is not actually possible for a
     * client to detect a problem.
     */
    switch (idx) {
    case 0:
        /* Set inconsistent SNI (early client detection) */
        err = SSL_R_INCONSISTENT_EARLY_DATA_SNI;
        if (!TEST_true(SSL_SESSION_set1_hostname(sess, "goodhost"))
                || !TEST_true(SSL_set_tlsext_host_name(clientssl, "badhost")))
            goto end;
        break;

    case 1:
        /* Set inconsistent ALPN (early client detection) */
        err = SSL_R_INCONSISTENT_EARLY_DATA_ALPN;
        /* SSL_set_alpn_protos returns 0 for success and 1 for failure */
        if (!TEST_true(SSL_SESSION_set1_alpn_selected(sess, GOODALPN,
                                                      GOODALPNLEN))
                || !TEST_false(SSL_set_alpn_protos(clientssl, BADALPN,
                                                   BADALPNLEN)))
            goto end;
        break;

    case 2:
        /*
         * Set invalid protocol version. Technically this affects PSKs without
         * early_data too, but we test it here because it is similar to the
         * SNI/ALPN consistency tests.
         */
        err = SSL_R_BAD_PSK;
        if (!TEST_true(SSL_SESSION_set_protocol_version(sess, TLS1_2_VERSION)))
            goto end;
        break;

    case 3:
        /*
         * Set inconsistent SNI (server side). In this case the connection
         * will succeed and accept early_data. In TLSv1.3 on the server side SNI
         * is associated with each handshake - not the session. Therefore it
         * should not matter that we used a different server name last time.
         */
        SSL_SESSION_free(serverpsk);
        serverpsk = SSL_SESSION_dup(clientpsk);
        if (!TEST_ptr(serverpsk)
                || !TEST_true(SSL_SESSION_set1_hostname(serverpsk, "badhost")))
            goto end;
        /* Fall through */
    case 4:
        /* Set consistent SNI */
        if (!TEST_true(SSL_SESSION_set1_hostname(sess, "goodhost"))
                || !TEST_true(SSL_set_tlsext_host_name(clientssl, "goodhost"))
                || !TEST_true(SSL_CTX_set_tlsext_servername_callback(sctx,
                                hostname_cb)))
            goto end;
        break;

    case 5:
        /*
         * Set inconsistent ALPN (server detected). In this case the connection
         * will succeed but reject early_data.
         */
        servalpn = "badalpn";
        edstatus = SSL_EARLY_DATA_REJECTED;
        readearlyres = SSL_READ_EARLY_DATA_FINISH;
        /* Fall through */
    case 6:
        /*
         * Set consistent ALPN.
         * SSL_set_alpn_protos returns 0 for success and 1 for failure. It
         * accepts a list of protos (each one length prefixed).
         * SSL_set1_alpn_selected accepts a single protocol (not length
         * prefixed)
         */
        if (!TEST_true(SSL_SESSION_set1_alpn_selected(sess, GOODALPN + 1,
                                                      GOODALPNLEN - 1))
                || !TEST_false(SSL_set_alpn_protos(clientssl, GOODALPN,
                                                   GOODALPNLEN)))
            goto end;

        SSL_CTX_set_alpn_select_cb(sctx, alpn_select_cb, NULL);
        break;

    case 7:
        /* Set inconsistent ALPN (late client detection) */
        SSL_SESSION_free(serverpsk);
        serverpsk = SSL_SESSION_dup(clientpsk);
        if (!TEST_ptr(serverpsk)
                || !TEST_true(SSL_SESSION_set1_alpn_selected(clientpsk,
                                                             BADALPN + 1,
                                                             BADALPNLEN - 1))
                || !TEST_true(SSL_SESSION_set1_alpn_selected(serverpsk,
                                                             GOODALPN + 1,
                                                             GOODALPNLEN - 1))
                || !TEST_false(SSL_set_alpn_protos(clientssl, alpnlist,
                                                   sizeof(alpnlist))))
            goto end;
        SSL_CTX_set_alpn_select_cb(sctx, alpn_select_cb, NULL);
        edstatus = SSL_EARLY_DATA_ACCEPTED;
        readearlyres = SSL_READ_EARLY_DATA_SUCCESS;
        /* SSL_connect() call should fail */
        connectres = -1;
        break;

    default:
        TEST_error("Bad test index");
        goto end;
    }

    SSL_set_connect_state(clientssl);
    if (err != 0) {
        if (!TEST_false(SSL_write_early_data(clientssl, MSG1, strlen(MSG1),
                                            &written))
                || !TEST_int_eq(SSL_get_error(clientssl, 0), SSL_ERROR_SSL)
                || !TEST_int_eq(ERR_GET_REASON(ERR_get_error()), err))
            goto end;
    } else {
        if (!TEST_true(SSL_write_early_data(clientssl, MSG1, strlen(MSG1),
                                            &written)))
            goto end;

        if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                             &readbytes), readearlyres)
                || (readearlyres == SSL_READ_EARLY_DATA_SUCCESS
                    && !TEST_mem_eq(buf, readbytes, MSG1, strlen(MSG1)))
                || !TEST_int_eq(SSL_get_early_data_status(serverssl), edstatus)
                || !TEST_int_eq(SSL_connect(clientssl), connectres))
            goto end;
    }

    testresult = 1;

 end:
    SSL_SESSION_free(sess);
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

/*
 * Test TLSv1.3 PSK can be used to send early_data with all 5 ciphersuites
 * idx == 0: Test with TLS1_3_RFC_AES_128_GCM_SHA256
 * idx == 1: Test with TLS1_3_RFC_AES_256_GCM_SHA384
 * idx == 2: Test with TLS1_3_RFC_CHACHA20_POLY1305_SHA256,
 * idx == 3: Test with TLS1_3_RFC_AES_128_CCM_SHA256
 * idx == 4: Test with TLS1_3_RFC_AES_128_CCM_8_SHA256
 */
static int test_early_data_psk_with_all_ciphers(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    SSL_SESSION *sess = NULL;
    unsigned char buf[20];
    size_t readbytes, written;
    const SSL_CIPHER *cipher;
    const char *cipher_str[] = {
        TLS1_3_RFC_AES_128_GCM_SHA256,
        TLS1_3_RFC_AES_256_GCM_SHA384,
# if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
        TLS1_3_RFC_CHACHA20_POLY1305_SHA256,
# else
        NULL,
# endif
        TLS1_3_RFC_AES_128_CCM_SHA256,
        TLS1_3_RFC_AES_128_CCM_8_SHA256
    };
    const unsigned char *cipher_bytes[] = {
        TLS13_AES_128_GCM_SHA256_BYTES,
        TLS13_AES_256_GCM_SHA384_BYTES,
# if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
        TLS13_CHACHA20_POLY1305_SHA256_BYTES,
# else
        NULL,
# endif
        TLS13_AES_128_CCM_SHA256_BYTES,
        TLS13_AES_128_CCM_8_SHA256_BYTES
    };

    if (cipher_str[idx] == NULL)
        return 1;

    /* We always set this up with a final parameter of "2" for PSK */
    if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl,
                                        &serverssl, &sess, 2)))
        goto end;

    if (!TEST_true(SSL_set_ciphersuites(clientssl, cipher_str[idx]))
            || !TEST_true(SSL_set_ciphersuites(serverssl, cipher_str[idx])))
        goto end;

    /*
     * 'setupearly_data_test' creates only one instance of SSL_SESSION
     * and assigns to both client and server with incremented reference
     * and the same instance is updated in 'sess'.
     * So updating ciphersuite in 'sess' which will get reflected in
     * PSK handshake using psk use sess and find sess cb.
     */
    cipher = SSL_CIPHER_find(clientssl, cipher_bytes[idx]);
    if (!TEST_ptr(cipher) || !TEST_true(SSL_SESSION_set_cipher(sess, cipher)))
        goto end;

    SSL_set_connect_state(clientssl);
    if (!TEST_true(SSL_write_early_data(clientssl, MSG1, strlen(MSG1),
                                        &written)))
        goto end;

    if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                         &readbytes),
                                         SSL_READ_EARLY_DATA_SUCCESS)
            || !TEST_mem_eq(buf, readbytes, MSG1, strlen(MSG1))
            || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                                                      SSL_EARLY_DATA_ACCEPTED)
            || !TEST_int_eq(SSL_connect(clientssl), 1)
            || !TEST_int_eq(SSL_accept(serverssl), 1))
        goto end;

    /* Send some normal data from client to server */
    if (!TEST_true(SSL_write_ex(clientssl, MSG2, strlen(MSG2), &written))
            || !TEST_size_t_eq(written, strlen(MSG2)))
        goto end;

    if (!TEST_true(SSL_read_ex(serverssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG2, strlen(MSG2)))
        goto end;

    testresult = 1;
 end:
    SSL_SESSION_free(sess);
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    if (clientssl != NULL)
        SSL_shutdown(clientssl);
    if (serverssl != NULL)
        SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

/*
 * Test that a server that doesn't try to read early data can handle a
 * client sending some.
 */
static int test_early_data_not_expected(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    SSL_SESSION *sess = NULL;
    unsigned char buf[20];
    size_t readbytes, written;

    if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl,
                                        &serverssl, &sess, idx)))
        goto end;

    /* Write some early data */
    if (!TEST_true(SSL_write_early_data(clientssl, MSG1, strlen(MSG1),
                                        &written)))
        goto end;

    /*
     * Server should skip over early data and then block waiting for client to
     * continue handshake
     */
    if (!TEST_int_le(SSL_accept(serverssl), 0)
     || !TEST_int_gt(SSL_connect(clientssl), 0)
     || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                     SSL_EARLY_DATA_REJECTED)
     || !TEST_int_gt(SSL_accept(serverssl), 0)
     || !TEST_int_eq(SSL_get_early_data_status(clientssl),
                     SSL_EARLY_DATA_REJECTED))
        goto end;

    /* Send some normal data from client to server */
    if (!TEST_true(SSL_write_ex(clientssl, MSG2, strlen(MSG2), &written))
            || !TEST_size_t_eq(written, strlen(MSG2)))
        goto end;

    if (!TEST_true(SSL_read_ex(serverssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG2, strlen(MSG2)))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(sess);
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}


# ifndef OPENSSL_NO_TLS1_2
/*
 * Test that a server attempting to read early data can handle a connection
 * from a TLSv1.2 client.
 */
static int test_early_data_tls1_2(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    unsigned char buf[20];
    size_t readbytes, written;

    if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl,
                                        &serverssl, NULL, idx)))
        goto end;

    /* Write some data - should block due to handshake with server */
    SSL_set_max_proto_version(clientssl, TLS1_2_VERSION);
    SSL_set_connect_state(clientssl);
    if (!TEST_false(SSL_write_ex(clientssl, MSG1, strlen(MSG1), &written)))
        goto end;

    /*
     * Server should do TLSv1.2 handshake. First it will block waiting for more
     * messages from client after ServerDone. Then SSL_read_early_data should
     * finish and detect that early data has not been sent
     */
    if (!TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                         &readbytes),
                     SSL_READ_EARLY_DATA_ERROR))
        goto end;

    /*
     * Continue writing the message we started earlier. Will still block waiting
     * for the CCS/Finished from server
     */
    if (!TEST_false(SSL_write_ex(clientssl, MSG1, strlen(MSG1), &written))
            || !TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                                &readbytes),
                            SSL_READ_EARLY_DATA_FINISH)
            || !TEST_size_t_eq(readbytes, 0)
            || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                            SSL_EARLY_DATA_NOT_SENT))
        goto end;

    /* Continue writing the message we started earlier */
    if (!TEST_true(SSL_write_ex(clientssl, MSG1, strlen(MSG1), &written))
            || !TEST_size_t_eq(written, strlen(MSG1))
            || !TEST_int_eq(SSL_get_early_data_status(clientssl),
                            SSL_EARLY_DATA_NOT_SENT)
            || !TEST_true(SSL_read_ex(serverssl, buf, sizeof(buf), &readbytes))
            || !TEST_mem_eq(buf, readbytes, MSG1, strlen(MSG1))
            || !TEST_true(SSL_write_ex(serverssl, MSG2, strlen(MSG2), &written))
            || !TEST_size_t_eq(written, strlen(MSG2))
            || !SSL_read_ex(clientssl, buf, sizeof(buf), &readbytes)
            || !TEST_mem_eq(buf, readbytes, MSG2, strlen(MSG2)))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}
# endif /* OPENSSL_NO_TLS1_2 */

/*
 * Test configuring the TLSv1.3 ciphersuites
 *
 * Test 0: Set a default ciphersuite in the SSL_CTX (no explicit cipher_list)
 * Test 1: Set a non-default ciphersuite in the SSL_CTX (no explicit cipher_list)
 * Test 2: Set a default ciphersuite in the SSL (no explicit cipher_list)
 * Test 3: Set a non-default ciphersuite in the SSL (no explicit cipher_list)
 * Test 4: Set a default ciphersuite in the SSL_CTX (SSL_CTX cipher_list)
 * Test 5: Set a non-default ciphersuite in the SSL_CTX (SSL_CTX cipher_list)
 * Test 6: Set a default ciphersuite in the SSL (SSL_CTX cipher_list)
 * Test 7: Set a non-default ciphersuite in the SSL (SSL_CTX cipher_list)
 * Test 8: Set a default ciphersuite in the SSL (SSL cipher_list)
 * Test 9: Set a non-default ciphersuite in the SSL (SSL cipher_list)
 */
static int test_set_ciphersuite(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey))
            || !TEST_true(SSL_CTX_set_ciphersuites(sctx,
                           "TLS_AES_128_GCM_SHA256:TLS_AES_128_CCM_SHA256")))
        goto end;

    if (idx >=4 && idx <= 7) {
        /* SSL_CTX explicit cipher list */
        if (!TEST_true(SSL_CTX_set_cipher_list(cctx, "AES256-GCM-SHA384")))
            goto end;
    }

    if (idx == 0 || idx == 4) {
        /* Default ciphersuite */
        if (!TEST_true(SSL_CTX_set_ciphersuites(cctx,
                                                "TLS_AES_128_GCM_SHA256")))
            goto end;
    } else if (idx == 1 || idx == 5) {
        /* Non default ciphersuite */
        if (!TEST_true(SSL_CTX_set_ciphersuites(cctx,
                                                "TLS_AES_128_CCM_SHA256")))
            goto end;
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                          &clientssl, NULL, NULL)))
        goto end;

    if (idx == 8 || idx == 9) {
        /* SSL explicit cipher list */
        if (!TEST_true(SSL_set_cipher_list(clientssl, "AES256-GCM-SHA384")))
            goto end;
    }

    if (idx == 2 || idx == 6 || idx == 8) {
        /* Default ciphersuite */
        if (!TEST_true(SSL_set_ciphersuites(clientssl,
                                            "TLS_AES_128_GCM_SHA256")))
            goto end;
    } else if (idx == 3 || idx == 7 || idx == 9) {
        /* Non default ciphersuite */
        if (!TEST_true(SSL_set_ciphersuites(clientssl,
                                            "TLS_AES_128_CCM_SHA256")))
            goto end;
    }

    if (!TEST_true(create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE)))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

static int test_ciphersuite_change(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    SSL_SESSION *clntsess = NULL;
    int testresult = 0;
    const SSL_CIPHER *aes_128_gcm_sha256 = NULL;

    /* Create a session based on SHA-256 */
    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey))
            || !TEST_true(SSL_CTX_set_ciphersuites(cctx,
                                                   "TLS_AES_128_GCM_SHA256"))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                          &clientssl, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    clntsess = SSL_get1_session(clientssl);
    /* Save for later */
    aes_128_gcm_sha256 = SSL_SESSION_get0_cipher(clntsess);
    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

# if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
    /* Check we can resume a session with a different SHA-256 ciphersuite */
    if (!TEST_true(SSL_CTX_set_ciphersuites(cctx,
                                            "TLS_CHACHA20_POLY1305_SHA256"))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl, clntsess))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl)))
        goto end;

    SSL_SESSION_free(clntsess);
    clntsess = SSL_get1_session(clientssl);
    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;
# endif

    /*
     * Check attempting to resume a SHA-256 session with no SHA-256 ciphersuites
     * succeeds but does not resume.
     */
    if (!TEST_true(SSL_CTX_set_ciphersuites(cctx, "TLS_AES_256_GCM_SHA384"))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl, clntsess))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_SSL))
            || !TEST_false(SSL_session_reused(clientssl)))
        goto end;

    SSL_SESSION_free(clntsess);
    clntsess = NULL;
    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

    /* Create a session based on SHA384 */
    if (!TEST_true(SSL_CTX_set_ciphersuites(cctx, "TLS_AES_256_GCM_SHA384"))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                          &clientssl, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    clntsess = SSL_get1_session(clientssl);
    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

    if (!TEST_true(SSL_CTX_set_ciphersuites(cctx,
                   "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384"))
            || !TEST_true(SSL_CTX_set_ciphersuites(sctx,
                                                   "TLS_AES_256_GCM_SHA384"))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl, clntsess))
               /*
                * We use SSL_ERROR_WANT_READ below so that we can pause the
                * connection after the initial ClientHello has been sent to
                * enable us to make some session changes.
                */
            || !TEST_false(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_WANT_READ)))
        goto end;

    /* Trick the client into thinking this session is for a different digest */
    clntsess->cipher = aes_128_gcm_sha256;
    clntsess->cipher_id = clntsess->cipher->id;

    /*
     * Continue the previously started connection. Server has selected a SHA-384
     * ciphersuite, but client thinks the session is for SHA-256, so it should
     * bail out.
     */
    if (!TEST_false(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_SSL))
            || !TEST_int_eq(ERR_GET_REASON(ERR_get_error()),
                            SSL_R_CIPHERSUITE_DIGEST_HAS_CHANGED))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(clntsess);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

/*
 * Test TLSv1.3 Cipher Suite
 * Test 0 = Set TLS1.3 cipher on context
 * Test 1 = Set TLS1.3 cipher on SSL
 * Test 2 = Set TLS1.3 and TLS1.2 cipher on context
 * Test 3 = Set TLS1.3 and TLS1.2 cipher on SSL
 */
static int test_tls13_ciphersuite(int idx)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *serverssl = NULL, *clientssl = NULL;
    static const char *t13_ciphers[] = {
        TLS1_3_RFC_AES_128_GCM_SHA256,
        TLS1_3_RFC_AES_256_GCM_SHA384,
        TLS1_3_RFC_AES_128_CCM_SHA256,
# if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
        TLS1_3_RFC_CHACHA20_POLY1305_SHA256,
        TLS1_3_RFC_AES_256_GCM_SHA384 ":" TLS1_3_RFC_CHACHA20_POLY1305_SHA256,
# endif
        TLS1_3_RFC_AES_128_CCM_8_SHA256 ":" TLS1_3_RFC_AES_128_CCM_SHA256
    };
    const char *t13_cipher = NULL;
    const char *t12_cipher = NULL;
    const char *negotiated_scipher;
    const char *negotiated_ccipher;
    int set_at_ctx = 0;
    int set_at_ssl = 0;
    int testresult = 0;
    int max_ver;
    size_t i;

    switch (idx) {
        case 0:
            set_at_ctx = 1;
            break;
        case 1:
            set_at_ssl = 1;
            break;
        case 2:
            set_at_ctx = 1;
            t12_cipher = TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256;
            break;
        case 3:
            set_at_ssl = 1;
            t12_cipher = TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256;
            break;
    }

    for (max_ver = TLS1_2_VERSION; max_ver <= TLS1_3_VERSION; max_ver++) {
# ifdef OPENSSL_NO_TLS1_2
        if (max_ver == TLS1_2_VERSION)
            continue;
# endif
        for (i = 0; i < OSSL_NELEM(t13_ciphers); i++) {
            t13_cipher = t13_ciphers[i];
            if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                               TLS_client_method(),
                                               TLS1_VERSION, max_ver,
                                               &sctx, &cctx, cert, privkey)))
                goto end;

            if (set_at_ctx) {
                if (!TEST_true(SSL_CTX_set_ciphersuites(sctx, t13_cipher))
                    || !TEST_true(SSL_CTX_set_ciphersuites(cctx, t13_cipher)))
                    goto end;
                if (t12_cipher != NULL) {
                    if (!TEST_true(SSL_CTX_set_cipher_list(sctx, t12_cipher))
                        || !TEST_true(SSL_CTX_set_cipher_list(cctx,
                                                              t12_cipher)))
                        goto end;
                }
            }

            if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                              &clientssl, NULL, NULL)))
                goto end;

            if (set_at_ssl) {
                if (!TEST_true(SSL_set_ciphersuites(serverssl, t13_cipher))
                    || !TEST_true(SSL_set_ciphersuites(clientssl, t13_cipher)))
                    goto end;
                if (t12_cipher != NULL) {
                    if (!TEST_true(SSL_set_cipher_list(serverssl, t12_cipher))
                        || !TEST_true(SSL_set_cipher_list(clientssl,
                                                          t12_cipher)))
                        goto end;
                }
            }

            if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                                 SSL_ERROR_NONE)))
                goto end;

            negotiated_scipher = SSL_CIPHER_get_name(SSL_get_current_cipher(
                                                                 serverssl));
            negotiated_ccipher = SSL_CIPHER_get_name(SSL_get_current_cipher(
                                                                 clientssl));
            if (!TEST_str_eq(negotiated_scipher, negotiated_ccipher))
                goto end;

            /*
             * TEST_strn_eq is used below because t13_cipher can contain
             * multiple ciphersuites
             */
            if (max_ver == TLS1_3_VERSION
                && !TEST_strn_eq(t13_cipher, negotiated_scipher,
                                 strlen(negotiated_scipher)))
                goto end;

# ifndef OPENSSL_NO_TLS1_2
            /* Below validation is not done when t12_cipher is NULL */
            if (max_ver == TLS1_2_VERSION && t12_cipher != NULL
                && !TEST_str_eq(t12_cipher, negotiated_scipher))
                goto end;
# endif

            SSL_free(serverssl);
            serverssl = NULL;
            SSL_free(clientssl);
            clientssl = NULL;
            SSL_CTX_free(sctx);
            sctx = NULL;
            SSL_CTX_free(cctx);
            cctx = NULL;
        }
    }

    testresult = 1;
 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

/*
 * Test TLSv1.3 PSKs
 * Test 0 = Test new style callbacks
 * Test 1 = Test both new and old style callbacks
 * Test 2 = Test old style callbacks
 * Test 3 = Test old style callbacks with no certificate
 */
static int test_tls13_psk(int idx)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *serverssl = NULL, *clientssl = NULL;
    const SSL_CIPHER *cipher = NULL;
    const unsigned char key[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f
    };
    int testresult = 0;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, idx == 3 ? NULL : cert,
                                       idx == 3 ? NULL : privkey)))
        goto end;

    if (idx != 3) {
        /*
         * We use a ciphersuite with SHA256 to ease testing old style PSK
         * callbacks which will always default to SHA256. This should not be
         * necessary if we have no cert/priv key. In that case the server should
         * prefer SHA256 automatically.
         */
        if (!TEST_true(SSL_CTX_set_ciphersuites(cctx,
                                                "TLS_AES_128_GCM_SHA256")))
            goto end;
    }

    /*
     * Test 0: New style callbacks only
     * Test 1: New and old style callbacks (only the new ones should be used)
     * Test 2: Old style callbacks only
     */
    if (idx == 0 || idx == 1) {
        SSL_CTX_set_psk_use_session_callback(cctx, use_session_cb);
        SSL_CTX_set_psk_find_session_callback(sctx, find_session_cb);
    }
#ifndef OPENSSL_NO_PSK
    if (idx >= 1) {
        SSL_CTX_set_psk_client_callback(cctx, psk_client_cb);
        SSL_CTX_set_psk_server_callback(sctx, psk_server_cb);
    }
#endif
    srvid = pskid;
    use_session_cb_cnt = 0;
    find_session_cb_cnt = 0;
    psk_client_cb_cnt = 0;
    psk_server_cb_cnt = 0;

    if (idx != 3) {
        /*
         * Check we can create a connection if callback decides not to send a
         * PSK
         */
        if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                                 NULL, NULL))
                || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                    SSL_ERROR_NONE))
                || !TEST_false(SSL_session_reused(clientssl))
                || !TEST_false(SSL_session_reused(serverssl)))
            goto end;

        if (idx == 0 || idx == 1) {
            if (!TEST_true(use_session_cb_cnt == 1)
                    || !TEST_true(find_session_cb_cnt == 0)
                       /*
                        * If no old style callback then below should be 0
                        * otherwise 1
                        */
                    || !TEST_true(psk_client_cb_cnt == idx)
                    || !TEST_true(psk_server_cb_cnt == 0))
                goto end;
        } else {
            if (!TEST_true(use_session_cb_cnt == 0)
                    || !TEST_true(find_session_cb_cnt == 0)
                    || !TEST_true(psk_client_cb_cnt == 1)
                    || !TEST_true(psk_server_cb_cnt == 0))
                goto end;
        }

        shutdown_ssl_connection(serverssl, clientssl);
        serverssl = clientssl = NULL;
        use_session_cb_cnt = psk_client_cb_cnt = 0;
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL)))
        goto end;

    /* Create the PSK */
    cipher = SSL_CIPHER_find(clientssl, TLS13_AES_128_GCM_SHA256_BYTES);
    clientpsk = SSL_SESSION_new();
    if (!TEST_ptr(clientpsk)
            || !TEST_ptr(cipher)
            || !TEST_true(SSL_SESSION_set1_master_key(clientpsk, key,
                                                      sizeof(key)))
            || !TEST_true(SSL_SESSION_set_cipher(clientpsk, cipher))
            || !TEST_true(SSL_SESSION_set_protocol_version(clientpsk,
                                                           TLS1_3_VERSION))
            || !TEST_true(SSL_SESSION_up_ref(clientpsk)))
        goto end;
    serverpsk = clientpsk;

    /* Check we can create a connection and the PSK is used */
    if (!TEST_true(create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl))
            || !TEST_true(SSL_session_reused(serverssl)))
        goto end;

    if (idx == 0 || idx == 1) {
        if (!TEST_true(use_session_cb_cnt == 1)
                || !TEST_true(find_session_cb_cnt == 1)
                || !TEST_true(psk_client_cb_cnt == 0)
                || !TEST_true(psk_server_cb_cnt == 0))
            goto end;
    } else {
        if (!TEST_true(use_session_cb_cnt == 0)
                || !TEST_true(find_session_cb_cnt == 0)
                || !TEST_true(psk_client_cb_cnt == 1)
                || !TEST_true(psk_server_cb_cnt == 1))
            goto end;
    }

    shutdown_ssl_connection(serverssl, clientssl);
    serverssl = clientssl = NULL;
    use_session_cb_cnt = find_session_cb_cnt = 0;
    psk_client_cb_cnt = psk_server_cb_cnt = 0;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL)))
        goto end;

    /* Force an HRR */
    if (!TEST_true(SSL_set1_groups_list(serverssl, "P-256")))
        goto end;

    /*
     * Check we can create a connection, the PSK is used and the callbacks are
     * called twice.
     */
    if (!TEST_true(create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl))
            || !TEST_true(SSL_session_reused(serverssl)))
        goto end;

    if (idx == 0 || idx == 1) {
        if (!TEST_true(use_session_cb_cnt == 2)
                || !TEST_true(find_session_cb_cnt == 2)
                || !TEST_true(psk_client_cb_cnt == 0)
                || !TEST_true(psk_server_cb_cnt == 0))
            goto end;
    } else {
        if (!TEST_true(use_session_cb_cnt == 0)
                || !TEST_true(find_session_cb_cnt == 0)
                || !TEST_true(psk_client_cb_cnt == 2)
                || !TEST_true(psk_server_cb_cnt == 2))
            goto end;
    }

    shutdown_ssl_connection(serverssl, clientssl);
    serverssl = clientssl = NULL;
    use_session_cb_cnt = find_session_cb_cnt = 0;
    psk_client_cb_cnt = psk_server_cb_cnt = 0;

    if (idx != 3) {
        /*
         * Check that if the server rejects the PSK we can still connect, but with
         * a full handshake
         */
        srvid = "Dummy Identity";
        if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                                 NULL, NULL))
                || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                    SSL_ERROR_NONE))
                || !TEST_false(SSL_session_reused(clientssl))
                || !TEST_false(SSL_session_reused(serverssl)))
            goto end;

        if (idx == 0 || idx == 1) {
            if (!TEST_true(use_session_cb_cnt == 1)
                    || !TEST_true(find_session_cb_cnt == 1)
                    || !TEST_true(psk_client_cb_cnt == 0)
                       /*
                        * If no old style callback then below should be 0
                        * otherwise 1
                        */
                    || !TEST_true(psk_server_cb_cnt == idx))
                goto end;
        } else {
            if (!TEST_true(use_session_cb_cnt == 0)
                    || !TEST_true(find_session_cb_cnt == 0)
                    || !TEST_true(psk_client_cb_cnt == 1)
                    || !TEST_true(psk_server_cb_cnt == 1))
                goto end;
        }

        shutdown_ssl_connection(serverssl, clientssl);
        serverssl = clientssl = NULL;
    }
    testresult = 1;

 end:
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

static unsigned char cookie_magic_value[] = "cookie magic";

static int generate_cookie_callback(SSL *ssl, unsigned char *cookie,
                                    unsigned int *cookie_len)
{
    /*
     * Not suitable as a real cookie generation function but good enough for
     * testing!
     */
    memcpy(cookie, cookie_magic_value, sizeof(cookie_magic_value) - 1);
    *cookie_len = sizeof(cookie_magic_value) - 1;

    return 1;
}

static int verify_cookie_callback(SSL *ssl, const unsigned char *cookie,
                                  unsigned int cookie_len)
{
    if (cookie_len == sizeof(cookie_magic_value) - 1
        && memcmp(cookie, cookie_magic_value, cookie_len) == 0)
        return 1;

    return 0;
}

static int generate_stateless_cookie_callback(SSL *ssl, unsigned char *cookie,
                                        size_t *cookie_len)
{
    unsigned int temp;
    int res = generate_cookie_callback(ssl, cookie, &temp);
    *cookie_len = temp;
    return res;
}

static int verify_stateless_cookie_callback(SSL *ssl, const unsigned char *cookie,
                                      size_t cookie_len)
{
    return verify_cookie_callback(ssl, cookie, cookie_len);
}

static int test_stateless(void)
{
    SSL_CTX *sctx = NULL, *cctx = NULL;
    SSL *serverssl = NULL, *clientssl = NULL;
    int testresult = 0;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    /* The arrival of CCS messages can confuse the test */
    SSL_CTX_clear_options(cctx, SSL_OP_ENABLE_MIDDLEBOX_COMPAT);

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL))
               /* Send the first ClientHello */
            || !TEST_false(create_ssl_connection(serverssl, clientssl,
                                                 SSL_ERROR_WANT_READ))
               /*
                * This should fail with a -1 return because we have no callbacks
                * set up
                */
            || !TEST_int_eq(SSL_stateless(serverssl), -1))
        goto end;

    /* Fatal error so abandon the connection from this client */
    SSL_free(clientssl);
    clientssl = NULL;

    /* Set up the cookie generation and verification callbacks */
    SSL_CTX_set_stateless_cookie_generate_cb(sctx, generate_stateless_cookie_callback);
    SSL_CTX_set_stateless_cookie_verify_cb(sctx, verify_stateless_cookie_callback);

    /*
     * Create a new connection from the client (we can reuse the server SSL
     * object).
     */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
               /* Send the first ClientHello */
            || !TEST_false(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_WANT_READ))
               /* This should fail because there is no cookie */
            || !TEST_int_eq(SSL_stateless(serverssl), 0))
        goto end;

    /* Abandon the connection from this client */
    SSL_free(clientssl);
    clientssl = NULL;

    /*
     * Now create a connection from a new client but with the same server SSL
     * object
     */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
               /* Send the first ClientHello */
            || !TEST_false(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_WANT_READ))
               /* This should fail because there is no cookie */
            || !TEST_int_eq(SSL_stateless(serverssl), 0)
               /* Send the second ClientHello */
            || !TEST_false(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_WANT_READ))
               /* This should succeed because a cookie is now present */
            || !TEST_int_eq(SSL_stateless(serverssl), 1)
               /* Complete the connection */
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    shutdown_ssl_connection(serverssl, clientssl);
    serverssl = clientssl = NULL;
    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;

}
#endif /* OPENSSL_NO_TLS1_3 */

static int clntaddoldcb = 0;
static int clntparseoldcb = 0;
static int srvaddoldcb = 0;
static int srvparseoldcb = 0;
static int clntaddnewcb = 0;
static int clntparsenewcb = 0;
static int srvaddnewcb = 0;
static int srvparsenewcb = 0;
static int snicb = 0;

#define TEST_EXT_TYPE1  0xff00

static int old_add_cb(SSL *s, unsigned int ext_type, const unsigned char **out,
                      size_t *outlen, int *al, void *add_arg)
{
    int *server = (int *)add_arg;
    unsigned char *data;

    if (SSL_is_server(s))
        srvaddoldcb++;
    else
        clntaddoldcb++;

    if (*server != SSL_is_server(s)
            || (data = OPENSSL_malloc(sizeof(*data))) == NULL)
        return -1;

    *data = 1;
    *out = data;
    *outlen = sizeof(char);
    return 1;
}

static void old_free_cb(SSL *s, unsigned int ext_type, const unsigned char *out,
                        void *add_arg)
{
    OPENSSL_free((unsigned char *)out);
}

static int old_parse_cb(SSL *s, unsigned int ext_type, const unsigned char *in,
                        size_t inlen, int *al, void *parse_arg)
{
    int *server = (int *)parse_arg;

    if (SSL_is_server(s))
        srvparseoldcb++;
    else
        clntparseoldcb++;

    if (*server != SSL_is_server(s)
            || inlen != sizeof(char)
            || *in != 1)
        return -1;

    return 1;
}

static int new_add_cb(SSL *s, unsigned int ext_type, unsigned int context,
                      const unsigned char **out, size_t *outlen, X509 *x,
                      size_t chainidx, int *al, void *add_arg)
{
    int *server = (int *)add_arg;
    unsigned char *data;

    if (SSL_is_server(s))
        srvaddnewcb++;
    else
        clntaddnewcb++;

    if (*server != SSL_is_server(s)
            || (data = OPENSSL_malloc(sizeof(*data))) == NULL)
        return -1;

    *data = 1;
    *out = data;
    *outlen = sizeof(*data);
    return 1;
}

static void new_free_cb(SSL *s, unsigned int ext_type, unsigned int context,
                        const unsigned char *out, void *add_arg)
{
    OPENSSL_free((unsigned char *)out);
}

static int new_parse_cb(SSL *s, unsigned int ext_type, unsigned int context,
                        const unsigned char *in, size_t inlen, X509 *x,
                        size_t chainidx, int *al, void *parse_arg)
{
    int *server = (int *)parse_arg;

    if (SSL_is_server(s))
        srvparsenewcb++;
    else
        clntparsenewcb++;

    if (*server != SSL_is_server(s)
            || inlen != sizeof(char) || *in != 1)
        return -1;

    return 1;
}

static int sni_cb(SSL *s, int *al, void *arg)
{
    SSL_CTX *ctx = (SSL_CTX *)arg;

    if (SSL_set_SSL_CTX(s, ctx) == NULL) {
        *al = SSL_AD_INTERNAL_ERROR;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }
    snicb++;
    return SSL_TLSEXT_ERR_OK;
}

static int verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
    return 1;
}

/*
 * Custom call back tests.
 * Test 0: Old style callbacks in TLSv1.2
 * Test 1: New style callbacks in TLSv1.2
 * Test 2: New style callbacks in TLSv1.2 with SNI
 * Test 3: New style callbacks in TLSv1.3. Extensions in CH and EE
 * Test 4: New style callbacks in TLSv1.3. Extensions in CH, SH, EE, Cert + NST
 * Test 5: New style callbacks in TLSv1.3. Extensions in CR + Client Cert
 */
static int test_custom_exts(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL, *sctx2 = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    static int server = 1;
    static int client = 0;
    SSL_SESSION *sess = NULL;
    unsigned int context;

#if defined(OPENSSL_NO_TLS1_2) && !defined(OPENSSL_NO_TLS1_3)
    /* Skip tests for TLSv1.2 and below in this case */
    if (tst < 3)
        return 1;
#endif

    /* Reset callback counters */
    clntaddoldcb = clntparseoldcb = srvaddoldcb = srvparseoldcb = 0;
    clntaddnewcb = clntparsenewcb = srvaddnewcb = srvparsenewcb = 0;
    snicb = 0;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    if (tst == 2
            && !TEST_true(create_ssl_ctx_pair(TLS_server_method(), NULL,
                                              TLS1_VERSION, TLS_MAX_VERSION,
                                              &sctx2, NULL, cert, privkey)))
        goto end;


    if (tst < 3) {
        SSL_CTX_set_options(cctx, SSL_OP_NO_TLSv1_3);
        SSL_CTX_set_options(sctx, SSL_OP_NO_TLSv1_3);
        if (sctx2 != NULL)
            SSL_CTX_set_options(sctx2, SSL_OP_NO_TLSv1_3);
    }

    if (tst == 5) {
        context = SSL_EXT_TLS1_3_CERTIFICATE_REQUEST
                  | SSL_EXT_TLS1_3_CERTIFICATE;
        SSL_CTX_set_verify(sctx,
                           SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                           verify_cb);
        if (!TEST_int_eq(SSL_CTX_use_certificate_file(cctx, cert,
                                                      SSL_FILETYPE_PEM), 1)
                || !TEST_int_eq(SSL_CTX_use_PrivateKey_file(cctx, privkey,
                                                            SSL_FILETYPE_PEM), 1)
                || !TEST_int_eq(SSL_CTX_check_private_key(cctx), 1))
            goto end;
    } else if (tst == 4) {
        context = SSL_EXT_CLIENT_HELLO
                  | SSL_EXT_TLS1_2_SERVER_HELLO
                  | SSL_EXT_TLS1_3_SERVER_HELLO
                  | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS
                  | SSL_EXT_TLS1_3_CERTIFICATE
                  | SSL_EXT_TLS1_3_NEW_SESSION_TICKET;
    } else {
        context = SSL_EXT_CLIENT_HELLO
                  | SSL_EXT_TLS1_2_SERVER_HELLO
                  | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS;
    }

    /* Create a client side custom extension */
    if (tst == 0) {
        if (!TEST_true(SSL_CTX_add_client_custom_ext(cctx, TEST_EXT_TYPE1,
                                                     old_add_cb, old_free_cb,
                                                     &client, old_parse_cb,
                                                     &client)))
            goto end;
    } else {
        if (!TEST_true(SSL_CTX_add_custom_ext(cctx, TEST_EXT_TYPE1, context,
                                              new_add_cb, new_free_cb,
                                              &client, new_parse_cb, &client)))
            goto end;
    }

    /* Should not be able to add duplicates */
    if (!TEST_false(SSL_CTX_add_client_custom_ext(cctx, TEST_EXT_TYPE1,
                                                  old_add_cb, old_free_cb,
                                                  &client, old_parse_cb,
                                                  &client))
            || !TEST_false(SSL_CTX_add_custom_ext(cctx, TEST_EXT_TYPE1,
                                                  context, new_add_cb,
                                                  new_free_cb, &client,
                                                  new_parse_cb, &client)))
        goto end;

    /* Create a server side custom extension */
    if (tst == 0) {
        if (!TEST_true(SSL_CTX_add_server_custom_ext(sctx, TEST_EXT_TYPE1,
                                                     old_add_cb, old_free_cb,
                                                     &server, old_parse_cb,
                                                     &server)))
            goto end;
    } else {
        if (!TEST_true(SSL_CTX_add_custom_ext(sctx, TEST_EXT_TYPE1, context,
                                              new_add_cb, new_free_cb,
                                              &server, new_parse_cb, &server)))
            goto end;
        if (sctx2 != NULL
                && !TEST_true(SSL_CTX_add_custom_ext(sctx2, TEST_EXT_TYPE1,
                                                     context, new_add_cb,
                                                     new_free_cb, &server,
                                                     new_parse_cb, &server)))
            goto end;
    }

    /* Should not be able to add duplicates */
    if (!TEST_false(SSL_CTX_add_server_custom_ext(sctx, TEST_EXT_TYPE1,
                                                  old_add_cb, old_free_cb,
                                                  &server, old_parse_cb,
                                                  &server))
            || !TEST_false(SSL_CTX_add_custom_ext(sctx, TEST_EXT_TYPE1,
                                                  context, new_add_cb,
                                                  new_free_cb, &server,
                                                  new_parse_cb, &server)))
        goto end;

    if (tst == 2) {
        /* Set up SNI */
        if (!TEST_true(SSL_CTX_set_tlsext_servername_callback(sctx, sni_cb))
                || !TEST_true(SSL_CTX_set_tlsext_servername_arg(sctx, sctx2)))
            goto end;
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    if (tst == 0) {
        if (clntaddoldcb != 1
                || clntparseoldcb != 1
                || srvaddoldcb != 1
                || srvparseoldcb != 1)
            goto end;
    } else if (tst == 1 || tst == 2 || tst == 3) {
        if (clntaddnewcb != 1
                || clntparsenewcb != 1
                || srvaddnewcb != 1
                || srvparsenewcb != 1
                || (tst != 2 && snicb != 0)
                || (tst == 2 && snicb != 1))
            goto end;
    } else if (tst == 5) {
        if (clntaddnewcb != 1
                || clntparsenewcb != 1
                || srvaddnewcb != 1
                || srvparsenewcb != 1)
            goto end;
    } else {
        /* In this case there 2 NewSessionTicket messages created */
        if (clntaddnewcb != 1
                || clntparsenewcb != 5
                || srvaddnewcb != 5
                || srvparsenewcb != 1)
            goto end;
    }

    sess = SSL_get1_session(clientssl);
    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

    if (tst == 3 || tst == 5) {
        /* We don't bother with the resumption aspects for these tests */
        testresult = 1;
        goto end;
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL))
            || !TEST_true(SSL_set_session(clientssl, sess))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                               SSL_ERROR_NONE)))
        goto end;

    /*
     * For a resumed session we expect to add the ClientHello extension. For the
     * old style callbacks we ignore it on the server side because they set
     * SSL_EXT_IGNORE_ON_RESUMPTION. The new style callbacks do not ignore
     * them.
     */
    if (tst == 0) {
        if (clntaddoldcb != 2
                || clntparseoldcb != 1
                || srvaddoldcb != 1
                || srvparseoldcb != 1)
            goto end;
    } else if (tst == 1 || tst == 2 || tst == 3) {
        if (clntaddnewcb != 2
                || clntparsenewcb != 2
                || srvaddnewcb != 2
                || srvparsenewcb != 2)
            goto end;
    } else {
        /*
         * No Certificate message extensions in the resumption handshake,
         * 2 NewSessionTickets in the initial handshake, 1 in the resumption
         */
        if (clntaddnewcb != 2
                || clntparsenewcb != 8
                || srvaddnewcb != 8
                || srvparsenewcb != 2)
            goto end;
    }

    testresult = 1;

end:
    SSL_SESSION_free(sess);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx2);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

/*
 * Test loading of serverinfo data in various formats. test_sslmessages actually
 * tests to make sure the extensions appear in the handshake
 */
static int test_serverinfo(int tst)
{
    unsigned int version;
    unsigned char *sibuf;
    size_t sibuflen;
    int ret, expected, testresult = 0;
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(TLS_method());
    if (!TEST_ptr(ctx))
        goto end;

    if ((tst & 0x01) == 0x01)
        version = SSL_SERVERINFOV2;
    else
        version = SSL_SERVERINFOV1;

    if ((tst & 0x02) == 0x02) {
        sibuf = serverinfov2;
        sibuflen = sizeof(serverinfov2);
        expected = (version == SSL_SERVERINFOV2);
    } else {
        sibuf = serverinfov1;
        sibuflen = sizeof(serverinfov1);
        expected = (version == SSL_SERVERINFOV1);
    }

    if ((tst & 0x04) == 0x04) {
        ret = SSL_CTX_use_serverinfo_ex(ctx, version, sibuf, sibuflen);
    } else {
        ret = SSL_CTX_use_serverinfo(ctx, sibuf, sibuflen);

        /*
         * The version variable is irrelevant in this case - it's what is in the
         * buffer that matters
         */
        if ((tst & 0x02) == 0x02)
            expected = 0;
        else
            expected = 1;
    }

    if (!TEST_true(ret == expected))
        goto end;

    testresult = 1;

 end:
    SSL_CTX_free(ctx);

    return testresult;
}

/*
 * Test that SSL_export_keying_material() produces expected results. There are
 * no test vectors so all we do is test that both sides of the communication
 * produce the same results for different protocol versions.
 */
#define SMALL_LABEL_LEN 10
#define LONG_LABEL_LEN  249
static int test_export_key_mat(int tst)
{
    int testresult = 0;
    SSL_CTX *cctx = NULL, *sctx = NULL, *sctx2 = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    const char label[LONG_LABEL_LEN + 1] = "test label";
    const unsigned char context[] = "context";
    const unsigned char *emptycontext = NULL;
    unsigned char ckeymat1[80], ckeymat2[80], ckeymat3[80];
    unsigned char skeymat1[80], skeymat2[80], skeymat3[80];
    size_t labellen;
    const int protocols[] = {
        TLS1_VERSION,
        TLS1_1_VERSION,
        TLS1_2_VERSION,
        TLS1_3_VERSION,
        TLS1_3_VERSION,
        TLS1_3_VERSION
    };

#ifdef OPENSSL_NO_TLS1
    if (tst == 0)
        return 1;
#endif
#ifdef OPENSSL_NO_TLS1_1
    if (tst == 1)
        return 1;
#endif
#ifdef OPENSSL_NO_TLS1_2
    if (tst == 2)
        return 1;
#endif
#ifdef OPENSSL_NO_TLS1_3
    if (tst >= 3)
        return 1;
#endif
    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    OPENSSL_assert(tst >= 0 && (size_t)tst < OSSL_NELEM(protocols));
    SSL_CTX_set_max_proto_version(cctx, protocols[tst]);
    SSL_CTX_set_min_proto_version(cctx, protocols[tst]);

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL,
                                      NULL)))
        goto end;

    /*
     * Premature call of SSL_export_keying_material should just fail.
     */
    if (!TEST_int_le(SSL_export_keying_material(clientssl, ckeymat1,
                                                sizeof(ckeymat1), label,
                                                SMALL_LABEL_LEN + 1, context,
                                                sizeof(context) - 1, 1), 0))
        goto end;

    if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                         SSL_ERROR_NONE)))
        goto end;

    if (tst == 5) {
        /*
         * TLSv1.3 imposes a maximum label len of 249 bytes. Check we fail if we
         * go over that.
         */
        if (!TEST_int_le(SSL_export_keying_material(clientssl, ckeymat1,
                                                    sizeof(ckeymat1), label,
                                                    LONG_LABEL_LEN + 1, context,
                                                    sizeof(context) - 1, 1), 0))
            goto end;

        testresult = 1;
        goto end;
    } else if (tst == 4) {
        labellen = LONG_LABEL_LEN;
    } else {
        labellen = SMALL_LABEL_LEN;
    }

    if (!TEST_int_eq(SSL_export_keying_material(clientssl, ckeymat1,
                                                sizeof(ckeymat1), label,
                                                labellen, context,
                                                sizeof(context) - 1, 1), 1)
            || !TEST_int_eq(SSL_export_keying_material(clientssl, ckeymat2,
                                                       sizeof(ckeymat2), label,
                                                       labellen,
                                                       emptycontext,
                                                       0, 1), 1)
            || !TEST_int_eq(SSL_export_keying_material(clientssl, ckeymat3,
                                                       sizeof(ckeymat3), label,
                                                       labellen,
                                                       NULL, 0, 0), 1)
            || !TEST_int_eq(SSL_export_keying_material(serverssl, skeymat1,
                                                       sizeof(skeymat1), label,
                                                       labellen,
                                                       context,
                                                       sizeof(context) -1, 1),
                            1)
            || !TEST_int_eq(SSL_export_keying_material(serverssl, skeymat2,
                                                       sizeof(skeymat2), label,
                                                       labellen,
                                                       emptycontext,
                                                       0, 1), 1)
            || !TEST_int_eq(SSL_export_keying_material(serverssl, skeymat3,
                                                       sizeof(skeymat3), label,
                                                       labellen,
                                                       NULL, 0, 0), 1)
               /*
                * Check that both sides created the same key material with the
                * same context.
                */
            || !TEST_mem_eq(ckeymat1, sizeof(ckeymat1), skeymat1,
                            sizeof(skeymat1))
               /*
                * Check that both sides created the same key material with an
                * empty context.
                */
            || !TEST_mem_eq(ckeymat2, sizeof(ckeymat2), skeymat2,
                            sizeof(skeymat2))
               /*
                * Check that both sides created the same key material without a
                * context.
                */
            || !TEST_mem_eq(ckeymat3, sizeof(ckeymat3), skeymat3,
                            sizeof(skeymat3))
               /* Different contexts should produce different results */
            || !TEST_mem_ne(ckeymat1, sizeof(ckeymat1), ckeymat2,
                            sizeof(ckeymat2)))
        goto end;

    /*
     * Check that an empty context and no context produce different results in
     * protocols less than TLSv1.3. In TLSv1.3 they should be the same.
     */
    if ((tst < 3 && !TEST_mem_ne(ckeymat2, sizeof(ckeymat2), ckeymat3,
                                  sizeof(ckeymat3)))
            || (tst >= 3 && !TEST_mem_eq(ckeymat2, sizeof(ckeymat2), ckeymat3,
                                         sizeof(ckeymat3))))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx2);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

#ifndef OPENSSL_NO_TLS1_3
/*
 * Test that SSL_export_keying_material_early() produces expected
 * results. There are no test vectors so all we do is test that both
 * sides of the communication produce the same results for different
 * protocol versions.
 */
static int test_export_key_mat_early(int idx)
{
    static const char label[] = "test label";
    static const unsigned char context[] = "context";
    int testresult = 0;
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    SSL_SESSION *sess = NULL;
    const unsigned char *emptycontext = NULL;
    unsigned char ckeymat1[80], ckeymat2[80];
    unsigned char skeymat1[80], skeymat2[80];
    unsigned char buf[1];
    size_t readbytes, written;

    if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl, &serverssl,
                                        &sess, idx)))
        goto end;

    /* Here writing 0 length early data is enough. */
    if (!TEST_true(SSL_write_early_data(clientssl, NULL, 0, &written))
            || !TEST_int_eq(SSL_read_early_data(serverssl, buf, sizeof(buf),
                                                &readbytes),
                            SSL_READ_EARLY_DATA_ERROR)
            || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                            SSL_EARLY_DATA_ACCEPTED))
        goto end;

    if (!TEST_int_eq(SSL_export_keying_material_early(
                     clientssl, ckeymat1, sizeof(ckeymat1), label,
                     sizeof(label) - 1, context, sizeof(context) - 1), 1)
            || !TEST_int_eq(SSL_export_keying_material_early(
                            clientssl, ckeymat2, sizeof(ckeymat2), label,
                            sizeof(label) - 1, emptycontext, 0), 1)
            || !TEST_int_eq(SSL_export_keying_material_early(
                            serverssl, skeymat1, sizeof(skeymat1), label,
                            sizeof(label) - 1, context, sizeof(context) - 1), 1)
            || !TEST_int_eq(SSL_export_keying_material_early(
                            serverssl, skeymat2, sizeof(skeymat2), label,
                            sizeof(label) - 1, emptycontext, 0), 1)
               /*
                * Check that both sides created the same key material with the
                * same context.
                */
            || !TEST_mem_eq(ckeymat1, sizeof(ckeymat1), skeymat1,
                            sizeof(skeymat1))
               /*
                * Check that both sides created the same key material with an
                * empty context.
                */
            || !TEST_mem_eq(ckeymat2, sizeof(ckeymat2), skeymat2,
                            sizeof(skeymat2))
               /* Different contexts should produce different results */
            || !TEST_mem_ne(ckeymat1, sizeof(ckeymat1), ckeymat2,
                            sizeof(ckeymat2)))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(sess);
    SSL_SESSION_free(clientpsk);
    SSL_SESSION_free(serverpsk);
    clientpsk = serverpsk = NULL;
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

#define NUM_KEY_UPDATE_MESSAGES 40
/*
 * Test KeyUpdate.
 */
static int test_key_update(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0, i, j;
    char buf[20];
    static char *mess = "A test message";

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_3_VERSION,
                                       0,
                                       &sctx, &cctx, cert, privkey))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    for (j = 0; j < 2; j++) {
        /* Send lots of KeyUpdate messages */
        for (i = 0; i < NUM_KEY_UPDATE_MESSAGES; i++) {
            if (!TEST_true(SSL_key_update(clientssl,
                                          (j == 0)
                                          ? SSL_KEY_UPDATE_NOT_REQUESTED
                                          : SSL_KEY_UPDATE_REQUESTED))
                    || !TEST_true(SSL_do_handshake(clientssl)))
                goto end;
        }

        /* Check that sending and receiving app data is ok */
        if (!TEST_int_eq(SSL_write(clientssl, mess, strlen(mess)), strlen(mess))
                || !TEST_int_eq(SSL_read(serverssl, buf, sizeof(buf)),
                                         strlen(mess)))
            goto end;

        if (!TEST_int_eq(SSL_write(serverssl, mess, strlen(mess)), strlen(mess))
                || !TEST_int_eq(SSL_read(clientssl, buf, sizeof(buf)),
                                         strlen(mess)))
            goto end;
    }

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

/*
 * Test we can handle a KeyUpdate (update requested) message while write data
 * is pending.
 * Test 0: Client sends KeyUpdate while Server is writing
 * Test 1: Server sends KeyUpdate while Client is writing
 */
static int test_key_update_in_write(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    char buf[20];
    static char *mess = "A test message";
    BIO *bretry = BIO_new(bio_s_always_retry());
    BIO *tmp = NULL;
    SSL *peerupdate = NULL, *peerwrite = NULL;

    if (!TEST_ptr(bretry)
            || !TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                              TLS_client_method(),
                                              TLS1_3_VERSION,
                                              0,
                                              &sctx, &cctx, cert, privkey))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    peerupdate = tst == 0 ? clientssl : serverssl;
    peerwrite = tst == 0 ? serverssl : clientssl;

    if (!TEST_true(SSL_key_update(peerupdate, SSL_KEY_UPDATE_REQUESTED))
            || !TEST_true(SSL_do_handshake(peerupdate)))
        goto end;

    /* Swap the writing endpoint's write BIO to force a retry */
    tmp = SSL_get_wbio(peerwrite);
    if (!TEST_ptr(tmp) || !TEST_true(BIO_up_ref(tmp))) {
        tmp = NULL;
        goto end;
    }
    SSL_set0_wbio(peerwrite, bretry);
    bretry = NULL;

    /* Write data that we know will fail with SSL_ERROR_WANT_WRITE */
    if (!TEST_int_eq(SSL_write(peerwrite, mess, strlen(mess)), -1)
            || !TEST_int_eq(SSL_get_error(peerwrite, 0), SSL_ERROR_WANT_WRITE))
        goto end;

    /* Reinstate the original writing endpoint's write BIO */
    SSL_set0_wbio(peerwrite, tmp);
    tmp = NULL;

    /* Now read some data - we will read the key update */
    if (!TEST_int_eq(SSL_read(peerwrite, buf, sizeof(buf)), -1)
            || !TEST_int_eq(SSL_get_error(peerwrite, 0), SSL_ERROR_WANT_READ))
        goto end;

    /*
     * Complete the write we started previously and read it from the other
     * endpoint
     */
    if (!TEST_int_eq(SSL_write(peerwrite, mess, strlen(mess)), strlen(mess))
            || !TEST_int_eq(SSL_read(peerupdate, buf, sizeof(buf)), strlen(mess)))
        goto end;

    /* Write more data to ensure we send the KeyUpdate message back */
    if (!TEST_int_eq(SSL_write(peerwrite, mess, strlen(mess)), strlen(mess))
            || !TEST_int_eq(SSL_read(peerupdate, buf, sizeof(buf)), strlen(mess)))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    BIO_free(bretry);
    BIO_free(tmp);

    return testresult;
}
#endif /* OPENSSL_NO_TLS1_3 */

static int test_ssl_clear(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;

#ifdef OPENSSL_NO_TLS1_2
    if (idx == 1)
        return 1;
#endif

    /* Create an initial connection */
    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey))
            || (idx == 1
                && !TEST_true(SSL_CTX_set_max_proto_version(cctx,
                                                            TLS1_2_VERSION)))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                          &clientssl, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    serverssl = NULL;

    /* Clear clientssl - we're going to reuse the object */
    if (!TEST_true(SSL_clear(clientssl)))
        goto end;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl)))
        goto end;

    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

/* Parse CH and retrieve any MFL extension value if present */
static int get_MFL_from_client_hello(BIO *bio, int *mfl_codemfl_code)
{
    long len;
    unsigned char *data;
    PACKET pkt = {0}, pkt2 = {0}, pkt3 = {0};
    unsigned int MFL_code = 0, type = 0;

    if (!TEST_uint_gt( len = BIO_get_mem_data( bio, (char **) &data ), 0 ) )
        goto end;

    if (!TEST_true( PACKET_buf_init( &pkt, data, len ) )
               /* Skip the record header */
            || !PACKET_forward(&pkt, SSL3_RT_HEADER_LENGTH)
               /* Skip the handshake message header */
            || !TEST_true(PACKET_forward(&pkt, SSL3_HM_HEADER_LENGTH))
               /* Skip client version and random */
            || !TEST_true(PACKET_forward(&pkt, CLIENT_VERSION_LEN
                                               + SSL3_RANDOM_SIZE))
               /* Skip session id */
            || !TEST_true(PACKET_get_length_prefixed_1(&pkt, &pkt2))
               /* Skip ciphers */
            || !TEST_true(PACKET_get_length_prefixed_2(&pkt, &pkt2))
               /* Skip compression */
            || !TEST_true(PACKET_get_length_prefixed_1(&pkt, &pkt2))
               /* Extensions len */
            || !TEST_true(PACKET_as_length_prefixed_2(&pkt, &pkt2)))
        goto end;

    /* Loop through all extensions */
    while (PACKET_remaining(&pkt2)) {
        if (!TEST_true(PACKET_get_net_2(&pkt2, &type))
                || !TEST_true(PACKET_get_length_prefixed_2(&pkt2, &pkt3)))
            goto end;

        if (type == TLSEXT_TYPE_max_fragment_length) {
            if (!TEST_uint_ne(PACKET_remaining(&pkt3), 0)
                    || !TEST_true(PACKET_get_1(&pkt3, &MFL_code)))
                goto end;

            *mfl_codemfl_code = MFL_code;
            return 1;
        }
    }

 end:
    return 0;
}

/* Maximum-Fragment-Length TLS extension mode to test */
static const unsigned char max_fragment_len_test[] = {
    TLSEXT_max_fragment_length_512,
    TLSEXT_max_fragment_length_1024,
    TLSEXT_max_fragment_length_2048,
    TLSEXT_max_fragment_length_4096
};

static int test_max_fragment_len_ext(int idx_tst)
{
    SSL_CTX *ctx;
    SSL *con = NULL;
    int testresult = 0, MFL_mode = 0;
    BIO *rbio, *wbio;

    ctx = SSL_CTX_new(TLS_method());
    if (!TEST_ptr(ctx))
        goto end;

    if (!TEST_true(SSL_CTX_set_tlsext_max_fragment_length(
                   ctx, max_fragment_len_test[idx_tst])))
        goto end;

    con = SSL_new(ctx);
    if (!TEST_ptr(con))
        goto end;

    rbio = BIO_new(BIO_s_mem());
    wbio = BIO_new(BIO_s_mem());
    if (!TEST_ptr(rbio)|| !TEST_ptr(wbio)) {
        BIO_free(rbio);
        BIO_free(wbio);
        goto end;
    }

    SSL_set_bio(con, rbio, wbio);
    SSL_set_connect_state(con);

    if (!TEST_int_le(SSL_connect(con), 0)) {
        /* This shouldn't succeed because we don't have a server! */
        goto end;
    }

    if (!TEST_true(get_MFL_from_client_hello(wbio, &MFL_mode)))
        /* no MFL in client hello */
        goto end;
    if (!TEST_true(max_fragment_len_test[idx_tst] == MFL_mode))
        goto end;

    testresult = 1;

end:
    SSL_free(con);
    SSL_CTX_free(ctx);

    return testresult;
}

#ifndef OPENSSL_NO_TLS1_3
static int test_pha_key_update(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        return 0;

    if (!TEST_true(SSL_CTX_set_min_proto_version(sctx, TLS1_3_VERSION))
        || !TEST_true(SSL_CTX_set_max_proto_version(sctx, TLS1_3_VERSION))
        || !TEST_true(SSL_CTX_set_min_proto_version(cctx, TLS1_3_VERSION))
        || !TEST_true(SSL_CTX_set_max_proto_version(cctx, TLS1_3_VERSION)))
        goto end;

    SSL_CTX_set_post_handshake_auth(cctx, 1);

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL)))
        goto end;

    if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                         SSL_ERROR_NONE)))
        goto end;

    SSL_set_verify(serverssl, SSL_VERIFY_PEER, NULL);
    if (!TEST_true(SSL_verify_client_post_handshake(serverssl)))
        goto end;

    if (!TEST_true(SSL_key_update(clientssl, SSL_KEY_UPDATE_NOT_REQUESTED)))
        goto end;

    /* Start handshake on the server */
    if (!TEST_int_eq(SSL_do_handshake(serverssl), 1))
        goto end;

    /* Starts with SSL_connect(), but it's really just SSL_do_handshake() */
    if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                         SSL_ERROR_NONE)))
        goto end;

    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}
#endif

#if !defined(OPENSSL_NO_SRP) && !defined(OPENSSL_NO_TLS1_2)

static SRP_VBASE *vbase = NULL;

static int ssl_srp_cb(SSL *s, int *ad, void *arg)
{
    int ret = SSL3_AL_FATAL;
    char *username;
    SRP_user_pwd *user = NULL;

    username = SSL_get_srp_username(s);
    if (username == NULL) {
        *ad = SSL_AD_INTERNAL_ERROR;
        goto err;
    }

    user = SRP_VBASE_get1_by_user(vbase, username);
    if (user == NULL) {
        *ad = SSL_AD_INTERNAL_ERROR;
        goto err;
    }

    if (SSL_set_srp_server_param(s, user->N, user->g, user->s, user->v,
                                 user->info) <= 0) {
        *ad = SSL_AD_INTERNAL_ERROR;
        goto err;
    }

    ret = 0;

 err:
    SRP_user_pwd_free(user);
    return ret;
}

static int create_new_vfile(char *userid, char *password, const char *filename)
{
    char *gNid = NULL;
    OPENSSL_STRING *row = OPENSSL_zalloc(sizeof(row) * (DB_NUMBER + 1));
    TXT_DB *db = NULL;
    int ret = 0;
    BIO *out = NULL, *dummy = BIO_new_mem_buf("", 0);
    size_t i;

    if (!TEST_ptr(dummy) || !TEST_ptr(row))
        goto end;

    gNid = SRP_create_verifier(userid, password, &row[DB_srpsalt],
                               &row[DB_srpverifier], NULL, NULL);
    if (!TEST_ptr(gNid))
        goto end;

    /*
     * The only way to create an empty TXT_DB is to provide a BIO with no data
     * in it!
     */
    db = TXT_DB_read(dummy, DB_NUMBER);
    if (!TEST_ptr(db))
        goto end;

    out = BIO_new_file(filename, "w");
    if (!TEST_ptr(out))
        goto end;

    row[DB_srpid] = OPENSSL_strdup(userid);
    row[DB_srptype] = OPENSSL_strdup("V");
    row[DB_srpgN] = OPENSSL_strdup(gNid);

    if (!TEST_ptr(row[DB_srpid])
            || !TEST_ptr(row[DB_srptype])
            || !TEST_ptr(row[DB_srpgN])
            || !TEST_true(TXT_DB_insert(db, row)))
        goto end;

    row = NULL;

    if (!TXT_DB_write(out, db))
        goto end;

    ret = 1;
 end:
    if (row != NULL) {
        for (i = 0; i < DB_NUMBER; i++)
            OPENSSL_free(row[i]);
    }
    OPENSSL_free(row);
    BIO_free(dummy);
    BIO_free(out);
    TXT_DB_free(db);

    return ret;
}

static int create_new_vbase(char *userid, char *password)
{
    BIGNUM *verifier = NULL, *salt = NULL;
    const SRP_gN *lgN = NULL;
    SRP_user_pwd *user_pwd = NULL;
    int ret = 0;

    lgN = SRP_get_default_gN(NULL);
    if (!TEST_ptr(lgN))
        goto end;

    if (!TEST_true(SRP_create_verifier_BN(userid, password, &salt, &verifier,
                                          lgN->N, lgN->g)))
        goto end;

    user_pwd = OPENSSL_zalloc(sizeof(*user_pwd));
    if (!TEST_ptr(user_pwd))
        goto end;

    user_pwd->N = lgN->N;
    user_pwd->g = lgN->g;
    user_pwd->id = OPENSSL_strdup(userid);
    if (!TEST_ptr(user_pwd->id))
        goto end;

    user_pwd->v = verifier;
    user_pwd->s = salt;
    verifier = salt = NULL;

    if (sk_SRP_user_pwd_insert(vbase->users_pwd, user_pwd, 0) == 0)
        goto end;
    user_pwd = NULL;

    ret = 1;
end:
    SRP_user_pwd_free(user_pwd);
    BN_free(salt);
    BN_free(verifier);

    return ret;
}

/*
 * SRP tests
 *
 * Test 0: Simple successful SRP connection, new vbase
 * Test 1: Connection failure due to bad password, new vbase
 * Test 2: Simple successful SRP connection, vbase loaded from existing file
 * Test 3: Connection failure due to bad password, vbase loaded from existing
 *         file
 * Test 4: Simple successful SRP connection, vbase loaded from new file
 * Test 5: Connection failure due to bad password, vbase loaded from new file
 */
static int test_srp(int tst)
{
    char *userid = "test", *password = "password", *tstsrpfile;
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int ret, testresult = 0;

    vbase = SRP_VBASE_new(NULL);
    if (!TEST_ptr(vbase))
        goto end;

    if (tst == 0 || tst == 1) {
        if (!TEST_true(create_new_vbase(userid, password)))
            goto end;
    } else {
        if (tst == 4 || tst == 5) {
            if (!TEST_true(create_new_vfile(userid, password, tmpfilename)))
                goto end;
            tstsrpfile = tmpfilename;
        } else {
            tstsrpfile = srpvfile;
        }
        if (!TEST_int_eq(SRP_VBASE_init(vbase, tstsrpfile), SRP_NO_ERROR))
            goto end;
    }

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_VERSION, TLS_MAX_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    if (!TEST_int_gt(SSL_CTX_set_srp_username_callback(sctx, ssl_srp_cb), 0)
            || !TEST_true(SSL_CTX_set_cipher_list(cctx, "SRP-AES-128-CBC-SHA"))
            || !TEST_true(SSL_CTX_set_max_proto_version(sctx, TLS1_2_VERSION))
            || !TEST_true(SSL_CTX_set_max_proto_version(cctx, TLS1_2_VERSION))
            || !TEST_int_gt(SSL_CTX_set_srp_username(cctx, userid), 0))
        goto end;

    if (tst % 2 == 1) {
        if (!TEST_int_gt(SSL_CTX_set_srp_password(cctx, "badpass"), 0))
            goto end;
    } else {
        if (!TEST_int_gt(SSL_CTX_set_srp_password(cctx, password), 0))
            goto end;
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL)))
        goto end;

    ret = create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE);
    if (ret) {
        if (!TEST_true(tst % 2 == 0))
            goto end;
    } else {
        if (!TEST_true(tst % 2 == 1))
            goto end;
    }

    testresult = 1;

 end:
    SRP_VBASE_free(vbase);
    vbase = NULL;
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}
#endif

static int info_cb_failed = 0;
static int info_cb_offset = 0;
static int info_cb_this_state = -1;

static struct info_cb_states_st {
    int where;
    const char *statestr;
} info_cb_states[][60] = {
    {
        /* TLSv1.2 server followed by resumption */
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "PINIT "},
        {SSL_CB_LOOP, "PINIT "}, {SSL_CB_LOOP, "TRCH"}, {SSL_CB_LOOP, "TWSH"},
        {SSL_CB_LOOP, "TWSC"}, {SSL_CB_LOOP, "TWSKE"}, {SSL_CB_LOOP, "TWSD"},
        {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "TWSD"}, {SSL_CB_LOOP, "TRCKE"},
        {SSL_CB_LOOP, "TRCCS"}, {SSL_CB_LOOP, "TRFIN"}, {SSL_CB_LOOP, "TWST"},
        {SSL_CB_LOOP, "TWCCS"}, {SSL_CB_LOOP, "TWFIN"},
        {SSL_CB_HANDSHAKE_DONE, NULL}, {SSL_CB_EXIT, NULL},
        {SSL_CB_ALERT, NULL}, {SSL_CB_HANDSHAKE_START, NULL},
        {SSL_CB_LOOP, "PINIT "}, {SSL_CB_LOOP, "PINIT "}, {SSL_CB_LOOP, "TRCH"},
        {SSL_CB_LOOP, "TWSH"}, {SSL_CB_LOOP, "TWCCS"}, {SSL_CB_LOOP, "TWFIN"},
        {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "TWFIN"}, {SSL_CB_LOOP, "TRCCS"},
        {SSL_CB_LOOP, "TRFIN"}, {SSL_CB_HANDSHAKE_DONE, NULL},
        {SSL_CB_EXIT, NULL}, {0, NULL},
    }, {
        /* TLSv1.2 client followed by resumption */
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "PINIT "},
        {SSL_CB_LOOP, "TWCH"}, {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "TWCH"},
        {SSL_CB_LOOP, "TRSH"}, {SSL_CB_LOOP, "TRSC"}, {SSL_CB_LOOP, "TRSKE"},
        {SSL_CB_LOOP, "TRSD"}, {SSL_CB_LOOP, "TWCKE"}, {SSL_CB_LOOP, "TWCCS"},
        {SSL_CB_LOOP, "TWFIN"}, {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "TWFIN"},
        {SSL_CB_LOOP, "TRST"}, {SSL_CB_LOOP, "TRCCS"}, {SSL_CB_LOOP, "TRFIN"},
        {SSL_CB_HANDSHAKE_DONE, NULL}, {SSL_CB_EXIT, NULL}, {SSL_CB_ALERT, NULL},
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "PINIT "},
        {SSL_CB_LOOP, "TWCH"}, {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "TWCH"},
        {SSL_CB_LOOP, "TRSH"}, {SSL_CB_LOOP, "TRCCS"}, {SSL_CB_LOOP, "TRFIN"},
        {SSL_CB_LOOP, "TWCCS"},  {SSL_CB_LOOP, "TWFIN"},
        {SSL_CB_HANDSHAKE_DONE, NULL}, {SSL_CB_EXIT, NULL}, {0, NULL},
    }, {
        /* TLSv1.3 server followed by resumption */
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "PINIT "},
        {SSL_CB_LOOP, "PINIT "}, {SSL_CB_LOOP, "TRCH"}, {SSL_CB_LOOP, "TWSH"},
        {SSL_CB_LOOP, "TWCCS"}, {SSL_CB_LOOP, "TWEE"}, {SSL_CB_LOOP, "TWSC"},
        {SSL_CB_LOOP, "TRSCV"}, {SSL_CB_LOOP, "TWFIN"}, {SSL_CB_LOOP, "TED"},
        {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "TED"}, {SSL_CB_LOOP, "TRFIN"},
        {SSL_CB_HANDSHAKE_DONE, NULL}, {SSL_CB_LOOP, "TWST"},
        {SSL_CB_LOOP, "TWST"}, {SSL_CB_EXIT, NULL}, {SSL_CB_ALERT, NULL},
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "PINIT "},
        {SSL_CB_LOOP, "PINIT "}, {SSL_CB_LOOP, "TRCH"}, {SSL_CB_LOOP, "TWSH"},
        {SSL_CB_LOOP, "TWCCS"}, {SSL_CB_LOOP, "TWEE"}, {SSL_CB_LOOP, "TWFIN"},
        {SSL_CB_LOOP, "TED"}, {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "TED"},
        {SSL_CB_LOOP, "TRFIN"}, {SSL_CB_HANDSHAKE_DONE, NULL},
        {SSL_CB_LOOP, "TWST"}, {SSL_CB_EXIT, NULL}, {0, NULL},
    }, {
        /* TLSv1.3 client followed by resumption */
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "PINIT "},
        {SSL_CB_LOOP, "TWCH"}, {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "TWCH"},
        {SSL_CB_LOOP, "TRSH"}, {SSL_CB_LOOP, "TREE"}, {SSL_CB_LOOP, "TRSC"},
        {SSL_CB_LOOP, "TRSCV"}, {SSL_CB_LOOP, "TRFIN"}, {SSL_CB_LOOP, "TWCCS"},
        {SSL_CB_LOOP, "TWFIN"},  {SSL_CB_HANDSHAKE_DONE, NULL},
        {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "SSLOK "}, {SSL_CB_LOOP, "SSLOK "},
        {SSL_CB_LOOP, "TRST"}, {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "SSLOK "},
        {SSL_CB_LOOP, "SSLOK "}, {SSL_CB_LOOP, "TRST"}, {SSL_CB_EXIT, NULL},
        {SSL_CB_ALERT, NULL}, {SSL_CB_HANDSHAKE_START, NULL},
        {SSL_CB_LOOP, "PINIT "}, {SSL_CB_LOOP, "TWCH"}, {SSL_CB_EXIT, NULL},
        {SSL_CB_LOOP, "TWCH"}, {SSL_CB_LOOP, "TRSH"},  {SSL_CB_LOOP, "TREE"},
        {SSL_CB_LOOP, "TRFIN"}, {SSL_CB_LOOP, "TWCCS"}, {SSL_CB_LOOP, "TWFIN"},
        {SSL_CB_HANDSHAKE_DONE, NULL}, {SSL_CB_EXIT, NULL},
        {SSL_CB_LOOP, "SSLOK "}, {SSL_CB_LOOP, "SSLOK "}, {SSL_CB_LOOP, "TRST"},
        {SSL_CB_EXIT, NULL}, {0, NULL},
    }, {
        /* TLSv1.3 server, early_data */
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "PINIT "},
        {SSL_CB_LOOP, "PINIT "}, {SSL_CB_LOOP, "TRCH"}, {SSL_CB_LOOP, "TWSH"},
        {SSL_CB_LOOP, "TWCCS"}, {SSL_CB_LOOP, "TWEE"}, {SSL_CB_LOOP, "TWFIN"},
        {SSL_CB_HANDSHAKE_DONE, NULL}, {SSL_CB_EXIT, NULL},
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "TED"},
        {SSL_CB_LOOP, "TED"}, {SSL_CB_LOOP, "TWEOED"}, {SSL_CB_LOOP, "TRFIN"},
        {SSL_CB_HANDSHAKE_DONE, NULL}, {SSL_CB_LOOP, "TWST"},
        {SSL_CB_EXIT, NULL}, {0, NULL},
    }, {
        /* TLSv1.3 client, early_data */
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "PINIT "},
        {SSL_CB_LOOP, "TWCH"}, {SSL_CB_LOOP, "TWCCS"},
        {SSL_CB_HANDSHAKE_DONE, NULL}, {SSL_CB_EXIT, NULL},
        {SSL_CB_HANDSHAKE_START, NULL}, {SSL_CB_LOOP, "TED"},
        {SSL_CB_LOOP, "TED"}, {SSL_CB_LOOP, "TRSH"}, {SSL_CB_LOOP, "TREE"},
        {SSL_CB_LOOP, "TRFIN"}, {SSL_CB_LOOP, "TPEDE"}, {SSL_CB_LOOP, "TWEOED"},
        {SSL_CB_LOOP, "TWFIN"}, {SSL_CB_HANDSHAKE_DONE, NULL},
        {SSL_CB_EXIT, NULL}, {SSL_CB_LOOP, "SSLOK "}, {SSL_CB_LOOP, "SSLOK "},
        {SSL_CB_LOOP, "TRST"}, {SSL_CB_EXIT, NULL}, {0, NULL},
    }, {
        {0, NULL},
    }
};

static void sslapi_info_callback(const SSL *s, int where, int ret)
{
    struct info_cb_states_st *state = info_cb_states[info_cb_offset];

    /* We do not ever expect a connection to fail in this test */
    if (!TEST_false(ret == 0)) {
        info_cb_failed = 1;
        return;
    }

    /*
     * Do some sanity checks. We never expect these things to happen in this
     * test
     */
    if (!TEST_false((SSL_is_server(s) && (where & SSL_ST_CONNECT) != 0))
            || !TEST_false(!SSL_is_server(s) && (where & SSL_ST_ACCEPT) != 0)
            || !TEST_int_ne(state[++info_cb_this_state].where, 0)) {
        info_cb_failed = 1;
        return;
    }

    /* Now check we're in the right state */
    if (!TEST_true((where & state[info_cb_this_state].where) != 0)) {
        info_cb_failed = 1;
        return;
    }
    if ((where & SSL_CB_LOOP) != 0
            && !TEST_int_eq(strcmp(SSL_state_string(s),
                            state[info_cb_this_state].statestr), 0)) {
        info_cb_failed = 1;
        return;
    }

    /*
     * Check that, if we've got SSL_CB_HANDSHAKE_DONE we are not in init
     */
    if ((where & SSL_CB_HANDSHAKE_DONE)
            && SSL_in_init((SSL *)s) != 0) {
        info_cb_failed = 1;
        return;
    }
}

/*
 * Test the info callback gets called when we expect it to.
 *
 * Test 0: TLSv1.2, server
 * Test 1: TLSv1.2, client
 * Test 2: TLSv1.3, server
 * Test 3: TLSv1.3, client
 * Test 4: TLSv1.3, server, early_data
 * Test 5: TLSv1.3, client, early_data
 */
static int test_info_callback(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    SSL_SESSION *clntsess = NULL;
    int testresult = 0;
    int tlsvers;

    if (tst < 2) {
/* We need either ECDHE or DHE for the TLSv1.2 test to work */
#if !defined(OPENSSL_NO_TLS1_2) && (!defined(OPENSSL_NO_EC) \
                                    || !defined(OPENSSL_NO_DH))
        tlsvers = TLS1_2_VERSION;
#else
        return 1;
#endif
    } else {
#ifndef OPENSSL_NO_TLS1_3
        tlsvers = TLS1_3_VERSION;
#else
        return 1;
#endif
    }

    /* Reset globals */
    info_cb_failed = 0;
    info_cb_this_state = -1;
    info_cb_offset = tst;

#ifndef OPENSSL_NO_TLS1_3
    if (tst >= 4) {
        SSL_SESSION *sess = NULL;
        size_t written, readbytes;
        unsigned char buf[80];

        /* early_data tests */
        if (!TEST_true(setupearly_data_test(&cctx, &sctx, &clientssl,
                                            &serverssl, &sess, 0)))
            goto end;

        /* We don't actually need this reference */
        SSL_SESSION_free(sess);

        SSL_set_info_callback((tst % 2) == 0 ? serverssl : clientssl,
                              sslapi_info_callback);

        /* Write and read some early data and then complete the connection */
        if (!TEST_true(SSL_write_early_data(clientssl, MSG1, strlen(MSG1),
                                            &written))
                || !TEST_size_t_eq(written, strlen(MSG1))
                || !TEST_int_eq(SSL_read_early_data(serverssl, buf,
                                                    sizeof(buf), &readbytes),
                                SSL_READ_EARLY_DATA_SUCCESS)
                || !TEST_mem_eq(MSG1, readbytes, buf, strlen(MSG1))
                || !TEST_int_eq(SSL_get_early_data_status(serverssl),
                                SSL_EARLY_DATA_ACCEPTED)
                || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                    SSL_ERROR_NONE))
                || !TEST_false(info_cb_failed))
            goto end;

        testresult = 1;
        goto end;
    }
#endif

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       tlsvers, tlsvers, &sctx, &cctx, cert,
                                       privkey)))
        goto end;

    /*
     * For even numbered tests we check the server callbacks. For odd numbers we
     * check the client.
     */
    SSL_CTX_set_info_callback((tst % 2) == 0 ? sctx : cctx,
                              sslapi_info_callback);

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                          &clientssl, NULL, NULL))
        || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                            SSL_ERROR_NONE))
        || !TEST_false(info_cb_failed))
    goto end;



    clntsess = SSL_get1_session(clientssl);
    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

    /* Now do a resumption */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL,
                                      NULL))
            || !TEST_true(SSL_set_session(clientssl, clntsess))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE))
            || !TEST_true(SSL_session_reused(clientssl))
            || !TEST_false(info_cb_failed))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_SESSION_free(clntsess);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

static int test_ssl_pending(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    char msg[] = "A test message";
    char buf[5];
    size_t written, readbytes;

    if (tst == 0) {
        if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                           TLS_client_method(),
                                           TLS1_VERSION, TLS_MAX_VERSION,
                                           &sctx, &cctx, cert, privkey)))
            goto end;
    } else {
#ifndef OPENSSL_NO_DTLS
        if (!TEST_true(create_ssl_ctx_pair(DTLS_server_method(),
                                           DTLS_client_method(),
                                           DTLS1_VERSION, DTLS_MAX_VERSION,
                                           &sctx, &cctx, cert, privkey)))
            goto end;
#else
        return 1;
#endif
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    if (!TEST_int_eq(SSL_pending(clientssl), 0)
            || !TEST_false(SSL_has_pending(clientssl))
            || !TEST_int_eq(SSL_pending(serverssl), 0)
            || !TEST_false(SSL_has_pending(serverssl))
            || !TEST_true(SSL_write_ex(serverssl, msg, sizeof(msg), &written))
            || !TEST_size_t_eq(written, sizeof(msg))
            || !TEST_true(SSL_read_ex(clientssl, buf, sizeof(buf), &readbytes))
            || !TEST_size_t_eq(readbytes, sizeof(buf))
            || !TEST_int_eq(SSL_pending(clientssl), (int)(written - readbytes))
            || !TEST_true(SSL_has_pending(clientssl)))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

static struct {
    unsigned int maxprot;
    const char *clntciphers;
    const char *clnttls13ciphers;
    const char *srvrciphers;
    const char *srvrtls13ciphers;
    const char *shared;
} shared_ciphers_data[] = {
/*
 * We can't establish a connection (even in TLSv1.1) with these ciphersuites if
 * TLSv1.3 is enabled but TLSv1.2 is disabled.
 */
#if defined(OPENSSL_NO_TLS1_3) || !defined(OPENSSL_NO_TLS1_2)
    {
        TLS1_2_VERSION,
        "AES128-SHA:AES256-SHA",
        NULL,
        "AES256-SHA:DHE-RSA-AES128-SHA",
        NULL,
        "AES256-SHA"
    },
    {
        TLS1_2_VERSION,
        "AES128-SHA:DHE-RSA-AES128-SHA:AES256-SHA",
        NULL,
        "AES128-SHA:DHE-RSA-AES256-SHA:AES256-SHA",
        NULL,
        "AES128-SHA:AES256-SHA"
    },
    {
        TLS1_2_VERSION,
        "AES128-SHA:AES256-SHA",
        NULL,
        "AES128-SHA:DHE-RSA-AES128-SHA",
        NULL,
        "AES128-SHA"
    },
#endif
/*
 * This test combines TLSv1.3 and TLSv1.2 ciphersuites so they must both be
 * enabled.
 */
#if !defined(OPENSSL_NO_TLS1_3) && !defined(OPENSSL_NO_TLS1_2) \
    && !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
    {
        TLS1_3_VERSION,
        "AES128-SHA:AES256-SHA",
        NULL,
        "AES256-SHA:AES128-SHA256",
        NULL,
        "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:"
        "TLS_AES_128_GCM_SHA256:AES256-SHA"
    },
#endif
#ifndef OPENSSL_NO_TLS1_3
    {
        TLS1_3_VERSION,
        "AES128-SHA",
        "TLS_AES_256_GCM_SHA384",
        "AES256-SHA",
        "TLS_AES_256_GCM_SHA384",
        "TLS_AES_256_GCM_SHA384"
    },
#endif
};

static int test_ssl_get_shared_ciphers(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    char buf[1024];

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION,
                                       shared_ciphers_data[tst].maxprot,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    if (!TEST_true(SSL_CTX_set_cipher_list(cctx,
                                        shared_ciphers_data[tst].clntciphers))
            || (shared_ciphers_data[tst].clnttls13ciphers != NULL
                && !TEST_true(SSL_CTX_set_ciphersuites(cctx,
                                    shared_ciphers_data[tst].clnttls13ciphers)))
            || !TEST_true(SSL_CTX_set_cipher_list(sctx,
                                        shared_ciphers_data[tst].srvrciphers))
            || (shared_ciphers_data[tst].srvrtls13ciphers != NULL
                && !TEST_true(SSL_CTX_set_ciphersuites(sctx,
                                    shared_ciphers_data[tst].srvrtls13ciphers))))
        goto end;


    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    if (!TEST_ptr(SSL_get_shared_ciphers(serverssl, buf, sizeof(buf)))
            || !TEST_int_eq(strcmp(buf, shared_ciphers_data[tst].shared), 0)) {
        TEST_info("Shared ciphers are: %s\n", buf);
        goto end;
    }

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

static const char *appdata = "Hello World";
static int gen_tick_called, dec_tick_called, tick_key_cb_called;
static int tick_key_renew = 0;
static SSL_TICKET_RETURN tick_dec_ret = SSL_TICKET_RETURN_ABORT;

static int gen_tick_cb(SSL *s, void *arg)
{
    gen_tick_called = 1;

    return SSL_SESSION_set1_ticket_appdata(SSL_get_session(s), appdata,
                                           strlen(appdata));
}

static SSL_TICKET_RETURN dec_tick_cb(SSL *s, SSL_SESSION *ss,
                                     const unsigned char *keyname,
                                     size_t keyname_length,
                                     SSL_TICKET_STATUS status,
                                     void *arg)
{
    void *tickdata;
    size_t tickdlen;

    dec_tick_called = 1;

    if (status == SSL_TICKET_EMPTY)
        return SSL_TICKET_RETURN_IGNORE_RENEW;

    if (!TEST_true(status == SSL_TICKET_SUCCESS
                   || status == SSL_TICKET_SUCCESS_RENEW))
        return SSL_TICKET_RETURN_ABORT;

    if (!TEST_true(SSL_SESSION_get0_ticket_appdata(ss, &tickdata,
                                                   &tickdlen))
            || !TEST_size_t_eq(tickdlen, strlen(appdata))
            || !TEST_int_eq(memcmp(tickdata, appdata, tickdlen), 0))
        return SSL_TICKET_RETURN_ABORT;

    if (tick_key_cb_called)  {
        /* Don't change what the ticket key callback wanted to do */
        switch (status) {
        case SSL_TICKET_NO_DECRYPT:
            return SSL_TICKET_RETURN_IGNORE_RENEW;

        case SSL_TICKET_SUCCESS:
            return SSL_TICKET_RETURN_USE;

        case SSL_TICKET_SUCCESS_RENEW:
            return SSL_TICKET_RETURN_USE_RENEW;

        default:
            return SSL_TICKET_RETURN_ABORT;
        }
    }
    return tick_dec_ret;

}

static int tick_key_cb(SSL *s, unsigned char key_name[16],
                       unsigned char iv[EVP_MAX_IV_LENGTH], EVP_CIPHER_CTX *ctx,
                       HMAC_CTX *hctx, int enc)
{
    const unsigned char tick_aes_key[16] = "0123456789abcdef";
    const unsigned char tick_hmac_key[16] = "0123456789abcdef";

    tick_key_cb_called = 1;
    memset(iv, 0, AES_BLOCK_SIZE);
    memset(key_name, 0, 16);
    if (!EVP_CipherInit_ex(ctx, EVP_aes_128_cbc(), NULL, tick_aes_key, iv, enc)
            || !HMAC_Init_ex(hctx, tick_hmac_key, sizeof(tick_hmac_key),
                             EVP_sha256(), NULL))
        return -1;

    return tick_key_renew ? 2 : 1;
}

/*
 * Test the various ticket callbacks
 * Test 0: TLSv1.2, no ticket key callback, no ticket, no renewal
 * Test 1: TLSv1.3, no ticket key callback, no ticket, no renewal
 * Test 2: TLSv1.2, no ticket key callback, no ticket, renewal
 * Test 3: TLSv1.3, no ticket key callback, no ticket, renewal
 * Test 4: TLSv1.2, no ticket key callback, ticket, no renewal
 * Test 5: TLSv1.3, no ticket key callback, ticket, no renewal
 * Test 6: TLSv1.2, no ticket key callback, ticket, renewal
 * Test 7: TLSv1.3, no ticket key callback, ticket, renewal
 * Test 8: TLSv1.2, ticket key callback, ticket, no renewal
 * Test 9: TLSv1.3, ticket key callback, ticket, no renewal
 * Test 10: TLSv1.2, ticket key callback, ticket, renewal
 * Test 11: TLSv1.3, ticket key callback, ticket, renewal
 */
static int test_ticket_callbacks(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    SSL_SESSION *clntsess = NULL;
    int testresult = 0;

#ifdef OPENSSL_NO_TLS1_2
    if (tst % 2 == 0)
        return 1;
#endif
#ifdef OPENSSL_NO_TLS1_3
    if (tst % 2 == 1)
        return 1;
#endif

    gen_tick_called = dec_tick_called = tick_key_cb_called = 0;

    /* Which tests the ticket key callback should request renewal for */
    if (tst == 10 || tst == 11)
        tick_key_renew = 1;
    else
        tick_key_renew = 0;

    /* Which tests the decrypt ticket callback should request renewal for */
    switch (tst) {
    case 0:
    case 1:
        tick_dec_ret = SSL_TICKET_RETURN_IGNORE;
        break;

    case 2:
    case 3:
        tick_dec_ret = SSL_TICKET_RETURN_IGNORE_RENEW;
        break;

    case 4:
    case 5:
        tick_dec_ret = SSL_TICKET_RETURN_USE;
        break;

    case 6:
    case 7:
        tick_dec_ret = SSL_TICKET_RETURN_USE_RENEW;
        break;

    default:
        tick_dec_ret = SSL_TICKET_RETURN_ABORT;
    }

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION,
                                       ((tst % 2) == 0) ? TLS1_2_VERSION
                                                        : TLS1_3_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    /*
     * We only want sessions to resume from tickets - not the session cache. So
     * switch the cache off.
     */
    if (!TEST_true(SSL_CTX_set_session_cache_mode(sctx, SSL_SESS_CACHE_OFF)))
        goto end;

    if (!TEST_true(SSL_CTX_set_session_ticket_cb(sctx, gen_tick_cb, dec_tick_cb,
                                                 NULL)))
        goto end;

    if (tst >= 8
            && !TEST_true(SSL_CTX_set_tlsext_ticket_key_cb(sctx, tick_key_cb)))
        goto end;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    /*
     * The decrypt ticket key callback in TLSv1.2 should be called even though
     * we have no ticket yet, because it gets called with a status of
     * SSL_TICKET_EMPTY (the client indicates support for tickets but does not
     * actually send any ticket data). This does not happen in TLSv1.3 because
     * it is not valid to send empty ticket data in TLSv1.3.
     */
    if (!TEST_int_eq(gen_tick_called, 1)
            || !TEST_int_eq(dec_tick_called, ((tst % 2) == 0) ? 1 : 0))
        goto end;

    gen_tick_called = dec_tick_called = 0;

    clntsess = SSL_get1_session(clientssl);
    SSL_shutdown(clientssl);
    SSL_shutdown(serverssl);
    SSL_free(serverssl);
    SSL_free(clientssl);
    serverssl = clientssl = NULL;

    /* Now do a resumption */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL,
                                      NULL))
            || !TEST_true(SSL_set_session(clientssl, clntsess))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    if (tick_dec_ret == SSL_TICKET_RETURN_IGNORE
            || tick_dec_ret == SSL_TICKET_RETURN_IGNORE_RENEW) {
        if (!TEST_false(SSL_session_reused(clientssl)))
            goto end;
    } else {
        if (!TEST_true(SSL_session_reused(clientssl)))
            goto end;
    }

    if (!TEST_int_eq(gen_tick_called,
                     (tick_key_renew
                      || tick_dec_ret == SSL_TICKET_RETURN_IGNORE_RENEW
                      || tick_dec_ret == SSL_TICKET_RETURN_USE_RENEW)
                     ? 1 : 0)
            || !TEST_int_eq(dec_tick_called, 1))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(clntsess);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

/*
 * Test bi-directional shutdown.
 * Test 0: TLSv1.2
 * Test 1: TLSv1.2, server continues to read/write after client shutdown
 * Test 2: TLSv1.3, no pending NewSessionTicket messages
 * Test 3: TLSv1.3, pending NewSessionTicket messages
 * Test 4: TLSv1.3, server continues to read/write after client shutdown, server
 *                  sends key update, client reads it
 * Test 5: TLSv1.3, server continues to read/write after client shutdown, server
 *                  sends CertificateRequest, client reads and ignores it
 * Test 6: TLSv1.3, server continues to read/write after client shutdown, client
 *                  doesn't read it
 */
static int test_shutdown(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    char msg[] = "A test message";
    char buf[80];
    size_t written, readbytes;
    SSL_SESSION *sess;

#ifdef OPENSSL_NO_TLS1_2
    if (tst <= 1)
        return 1;
#endif
#ifdef OPENSSL_NO_TLS1_3
    if (tst >= 2)
        return 1;
#endif

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION,
                                       (tst <= 1) ? TLS1_2_VERSION
                                                  : TLS1_3_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    if (tst == 5)
        SSL_CTX_set_post_handshake_auth(cctx, 1);

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL)))
        goto end;

    if (tst == 3) {
        if (!TEST_true(create_bare_ssl_connection(serverssl, clientssl,
                                                  SSL_ERROR_NONE, 1))
                || !TEST_ptr_ne(sess = SSL_get_session(clientssl), NULL)
                || !TEST_false(SSL_SESSION_is_resumable(sess)))
            goto end;
    } else if (!TEST_true(create_ssl_connection(serverssl, clientssl,
                                              SSL_ERROR_NONE))
            || !TEST_ptr_ne(sess = SSL_get_session(clientssl), NULL)
            || !TEST_true(SSL_SESSION_is_resumable(sess))) {
        goto end;
    }

    if (!TEST_int_eq(SSL_shutdown(clientssl), 0))
        goto end;

    if (tst >= 4) {
        /*
         * Reading on the server after the client has sent close_notify should
         * fail and provide SSL_ERROR_ZERO_RETURN
         */
        if (!TEST_false(SSL_read_ex(serverssl, buf, sizeof(buf), &readbytes))
                || !TEST_int_eq(SSL_get_error(serverssl, 0),
                                SSL_ERROR_ZERO_RETURN)
                || !TEST_int_eq(SSL_get_shutdown(serverssl),
                                SSL_RECEIVED_SHUTDOWN)
                   /*
                    * Even though we're shutdown on receive we should still be
                    * able to write.
                    */
                || !TEST_true(SSL_write(serverssl, msg, sizeof(msg))))
            goto end;
        if (tst == 4
                && !TEST_true(SSL_key_update(serverssl,
                                             SSL_KEY_UPDATE_REQUESTED)))
            goto end;
        if (tst == 5) {
            SSL_set_verify(serverssl, SSL_VERIFY_PEER, NULL);
            if (!TEST_true(SSL_verify_client_post_handshake(serverssl)))
                goto end;
        }
        if ((tst == 4 || tst == 5)
                && !TEST_true(SSL_write(serverssl, msg, sizeof(msg))))
            goto end;
        if (!TEST_int_eq(SSL_shutdown(serverssl), 1))
            goto end;
        if (tst == 4 || tst == 5) {
            /* Should still be able to read data from server */
            if (!TEST_true(SSL_read_ex(clientssl, buf, sizeof(buf),
                                       &readbytes))
                    || !TEST_size_t_eq(readbytes, sizeof(msg))
                    || !TEST_int_eq(memcmp(msg, buf, readbytes), 0)
                    || !TEST_true(SSL_read_ex(clientssl, buf, sizeof(buf),
                                              &readbytes))
                    || !TEST_size_t_eq(readbytes, sizeof(msg))
                    || !TEST_int_eq(memcmp(msg, buf, readbytes), 0))
                goto end;
        }
    }

    /* Writing on the client after sending close_notify shouldn't be possible */
    if (!TEST_false(SSL_write_ex(clientssl, msg, sizeof(msg), &written)))
        goto end;

    if (tst < 4) {
        /*
         * For these tests the client has sent close_notify but it has not yet
         * been received by the server. The server has not sent close_notify
         * yet.
         */
        if (!TEST_int_eq(SSL_shutdown(serverssl), 0)
                   /*
                    * Writing on the server after sending close_notify shouldn't
                    * be possible.
                    */
                || !TEST_false(SSL_write_ex(serverssl, msg, sizeof(msg), &written))
                || !TEST_int_eq(SSL_shutdown(clientssl), 1)
                || !TEST_ptr_ne(sess = SSL_get_session(clientssl), NULL)
                || !TEST_true(SSL_SESSION_is_resumable(sess))
                || !TEST_int_eq(SSL_shutdown(serverssl), 1))
            goto end;
    } else if (tst == 4 || tst == 5) {
        /*
         * In this test the client has sent close_notify and it has been
         * received by the server which has responded with a close_notify. The
         * client needs to read the close_notify sent by the server.
         */
        if (!TEST_int_eq(SSL_shutdown(clientssl), 1)
                || !TEST_ptr_ne(sess = SSL_get_session(clientssl), NULL)
                || !TEST_true(SSL_SESSION_is_resumable(sess)))
            goto end;
    } else {
        /*
         * tst == 6
         *
         * The client has sent close_notify and is expecting a close_notify
         * back, but instead there is application data first. The shutdown
         * should fail with a fatal error.
         */
        if (!TEST_int_eq(SSL_shutdown(clientssl), -1)
                || !TEST_int_eq(SSL_get_error(clientssl, -1), SSL_ERROR_SSL))
            goto end;
    }

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

#if !defined(OPENSSL_NO_TLS1_2) || !defined(OPENSSL_NO_TLS1_3)
static int cert_cb_cnt;

static int cert_cb(SSL *s, void *arg)
{
    SSL_CTX *ctx = (SSL_CTX *)arg;
    BIO *in = NULL;
    EVP_PKEY *pkey = NULL;
    X509 *x509 = NULL, *rootx = NULL;
    STACK_OF(X509) *chain = NULL;
    char *rootfile = NULL, *ecdsacert = NULL, *ecdsakey = NULL;
    int ret = 0;

    if (cert_cb_cnt == 0) {
        /* Suspend the handshake */
        cert_cb_cnt++;
        return -1;
    } else if (cert_cb_cnt == 1) {
        /*
         * Update the SSL_CTX, set the certificate and private key and then
         * continue the handshake normally.
         */
        if (ctx != NULL && !TEST_ptr(SSL_set_SSL_CTX(s, ctx)))
            return 0;

        if (!TEST_true(SSL_use_certificate_file(s, cert, SSL_FILETYPE_PEM))
                || !TEST_true(SSL_use_PrivateKey_file(s, privkey,
                                                      SSL_FILETYPE_PEM))
                || !TEST_true(SSL_check_private_key(s)))
            return 0;
        cert_cb_cnt++;
        return 1;
    } else if (cert_cb_cnt == 3) {
        int rv;

        rootfile = test_mk_file_path(certsdir, "rootcert.pem");
        ecdsacert = test_mk_file_path(certsdir, "server-ecdsa-cert.pem");
        ecdsakey = test_mk_file_path(certsdir, "server-ecdsa-key.pem");
        if (!TEST_ptr(rootfile) || !TEST_ptr(ecdsacert) || !TEST_ptr(ecdsakey))
            goto out;
        chain = sk_X509_new_null();
        if (!TEST_ptr(chain))
            goto out;
        if (!TEST_ptr(in = BIO_new(BIO_s_file()))
                || !TEST_int_ge(BIO_read_filename(in, rootfile), 0)
                || !TEST_ptr(rootx = PEM_read_bio_X509(in, NULL, NULL, NULL))
                || !TEST_true(sk_X509_push(chain, rootx)))
            goto out;
        rootx = NULL;
        BIO_free(in);
        if (!TEST_ptr(in = BIO_new(BIO_s_file()))
                || !TEST_int_ge(BIO_read_filename(in, ecdsacert), 0)
                || !TEST_ptr(x509 = PEM_read_bio_X509(in, NULL, NULL, NULL)))
            goto out;
        BIO_free(in);
        if (!TEST_ptr(in = BIO_new(BIO_s_file()))
                || !TEST_int_ge(BIO_read_filename(in, ecdsakey), 0)
                || !TEST_ptr(pkey = PEM_read_bio_PrivateKey(in, NULL, NULL, NULL)))
            goto out;
        rv = SSL_check_chain(s, x509, pkey, chain);
        /*
         * If the cert doesn't show as valid here (e.g., because we don't
         * have any shared sigalgs), then we will not set it, and there will
         * be no certificate at all on the SSL or SSL_CTX.  This, in turn,
         * will cause tls_choose_sigalgs() to fail the connection.
         */
        if ((rv & (CERT_PKEY_VALID | CERT_PKEY_CA_SIGNATURE))
                == (CERT_PKEY_VALID | CERT_PKEY_CA_SIGNATURE)) {
            if (!SSL_use_cert_and_key(s, x509, pkey, NULL, 1))
                goto out;
        }

        ret = 1;
    }

    /* Abort the handshake */
 out:
    OPENSSL_free(ecdsacert);
    OPENSSL_free(ecdsakey);
    OPENSSL_free(rootfile);
    BIO_free(in);
    EVP_PKEY_free(pkey);
    X509_free(x509);
    X509_free(rootx);
    sk_X509_pop_free(chain, X509_free);
    return ret;
}

/*
 * Test the certificate callback.
 * Test 0: Callback fails
 * Test 1: Success - no SSL_set_SSL_CTX() in the callback
 * Test 2: Success - SSL_set_SSL_CTX() in the callback
 * Test 3: Success - Call SSL_check_chain from the callback
 * Test 4: Failure - SSL_check_chain fails from callback due to bad cert in the
 *                   chain
 * Test 5: Failure - SSL_check_chain fails from callback due to bad ee cert
 */
static int test_cert_cb_int(int prot, int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL, *snictx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0, ret;

#ifdef OPENSSL_NO_EC
    /* We use an EC cert in these tests, so we skip in a no-ec build */
    if (tst >= 3)
        return 1;
#endif

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION,
                                       prot,
                                       &sctx, &cctx, NULL, NULL)))
        goto end;

    if (tst == 0)
        cert_cb_cnt = -1;
    else if (tst >= 3)
        cert_cb_cnt = 3;
    else
        cert_cb_cnt = 0;

    if (tst == 2)
        snictx = SSL_CTX_new(TLS_server_method());
    SSL_CTX_set_cert_cb(sctx, cert_cb, snictx);

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL)))
        goto end;

    if (tst == 4) {
        /*
         * We cause SSL_check_chain() to fail by specifying sig_algs that
         * the chain doesn't meet (the root uses an RSA cert)
         */
        if (!TEST_true(SSL_set1_sigalgs_list(clientssl,
                                             "ecdsa_secp256r1_sha256")))
            goto end;
    } else if (tst == 5) {
        /*
         * We cause SSL_check_chain() to fail by specifying sig_algs that
         * the ee cert doesn't meet (the ee uses an ECDSA cert)
         */
        if (!TEST_true(SSL_set1_sigalgs_list(clientssl,
                           "rsa_pss_rsae_sha256:rsa_pkcs1_sha256")))
            goto end;
    }

    ret = create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE);
    if (!TEST_true(tst == 0 || tst == 4 || tst == 5 ? !ret : ret)
            || (tst > 0
                && !TEST_int_eq((cert_cb_cnt - 2) * (cert_cb_cnt - 3), 0))) {
        goto end;
    }

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    SSL_CTX_free(snictx);

    return testresult;
}
#endif

static int test_cert_cb(int tst)
{
    int testresult = 1;

#ifndef OPENSSL_NO_TLS1_2
    testresult &= test_cert_cb_int(TLS1_2_VERSION, tst);
#endif
#ifndef OPENSSL_NO_TLS1_3
    testresult &= test_cert_cb_int(TLS1_3_VERSION, tst);
#endif

    return testresult;
}

static int client_cert_cb(SSL *ssl, X509 **x509, EVP_PKEY **pkey)
{
    X509 *xcert, *peer;
    EVP_PKEY *privpkey;
    BIO *in = NULL;

    /* Check that SSL_get_peer_certificate() returns something sensible */
    peer = SSL_get_peer_certificate(ssl);
    if (!TEST_ptr(peer))
        return 0;
    X509_free(peer);

    in = BIO_new_file(cert, "r");
    if (!TEST_ptr(in))
        return 0;

    xcert = PEM_read_bio_X509(in, NULL, NULL, NULL);
    BIO_free(in);
    if (!TEST_ptr(xcert))
        return 0;

    in = BIO_new_file(privkey, "r");
    if (!TEST_ptr(in)) {
        X509_free(xcert);
        return 0;
    }

    privpkey = PEM_read_bio_PrivateKey(in, NULL, NULL, NULL);
    BIO_free(in);
    if (!TEST_ptr(privpkey)) {
        X509_free(xcert);
        return 0;
    }

    *x509 = xcert;
    *pkey = privpkey;

    return 1;
}

static int test_client_cert_cb(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;

#ifdef OPENSSL_NO_TLS1_2
    if (tst == 0)
        return 1;
#endif
#ifdef OPENSSL_NO_TLS1_3
    if (tst == 1)
        return 1;
#endif

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION,
                                       tst == 0 ? TLS1_2_VERSION
                                                : TLS1_3_VERSION,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    /*
     * Test that setting a client_cert_cb results in a client certificate being
     * sent.
     */
    SSL_CTX_set_client_cert_cb(cctx, client_cert_cb);
    SSL_CTX_set_verify(sctx,
                       SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       verify_cb);

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

#if !defined(OPENSSL_NO_TLS1_2) || !defined(OPENSSL_NO_TLS1_3)
/*
 * Test setting certificate authorities on both client and server.
 *
 * Test 0: SSL_CTX_set0_CA_list() only
 * Test 1: Both SSL_CTX_set0_CA_list() and SSL_CTX_set_client_CA_list()
 * Test 2: Only SSL_CTX_set_client_CA_list()
 */
static int test_ca_names_int(int prot, int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    size_t i;
    X509_NAME *name[] = { NULL, NULL, NULL, NULL };
    char *strnames[] = { "Jack", "Jill", "John", "Joanne" };
    STACK_OF(X509_NAME) *sk1 = NULL, *sk2 = NULL;
    const STACK_OF(X509_NAME) *sktmp = NULL;

    for (i = 0; i < OSSL_NELEM(name); i++) {
        name[i] = X509_NAME_new();
        if (!TEST_ptr(name[i])
                || !TEST_true(X509_NAME_add_entry_by_txt(name[i], "CN",
                                                         MBSTRING_ASC,
                                                         (unsigned char *)
                                                         strnames[i],
                                                         -1, -1, 0)))
            goto end;
    }

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION,
                                       prot,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    SSL_CTX_set_verify(sctx, SSL_VERIFY_PEER, NULL);

    if (tst == 0 || tst == 1) {
        if (!TEST_ptr(sk1 = sk_X509_NAME_new_null())
                || !TEST_true(sk_X509_NAME_push(sk1, X509_NAME_dup(name[0])))
                || !TEST_true(sk_X509_NAME_push(sk1, X509_NAME_dup(name[1])))
                || !TEST_ptr(sk2 = sk_X509_NAME_new_null())
                || !TEST_true(sk_X509_NAME_push(sk2, X509_NAME_dup(name[0])))
                || !TEST_true(sk_X509_NAME_push(sk2, X509_NAME_dup(name[1]))))
            goto end;

        SSL_CTX_set0_CA_list(sctx, sk1);
        SSL_CTX_set0_CA_list(cctx, sk2);
        sk1 = sk2 = NULL;
    }
    if (tst == 1 || tst == 2) {
        if (!TEST_ptr(sk1 = sk_X509_NAME_new_null())
                || !TEST_true(sk_X509_NAME_push(sk1, X509_NAME_dup(name[2])))
                || !TEST_true(sk_X509_NAME_push(sk1, X509_NAME_dup(name[3])))
                || !TEST_ptr(sk2 = sk_X509_NAME_new_null())
                || !TEST_true(sk_X509_NAME_push(sk2, X509_NAME_dup(name[2])))
                || !TEST_true(sk_X509_NAME_push(sk2, X509_NAME_dup(name[3]))))
            goto end;

        SSL_CTX_set_client_CA_list(sctx, sk1);
        SSL_CTX_set_client_CA_list(cctx, sk2);
        sk1 = sk2 = NULL;
    }

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                      NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    /*
     * We only expect certificate authorities to have been sent to the server
     * if we are using TLSv1.3 and SSL_set0_CA_list() was used
     */
    sktmp = SSL_get0_peer_CA_list(serverssl);
    if (prot == TLS1_3_VERSION
            && (tst == 0 || tst == 1)) {
        if (!TEST_ptr(sktmp)
                || !TEST_int_eq(sk_X509_NAME_num(sktmp), 2)
                || !TEST_int_eq(X509_NAME_cmp(sk_X509_NAME_value(sktmp, 0),
                                              name[0]), 0)
                || !TEST_int_eq(X509_NAME_cmp(sk_X509_NAME_value(sktmp, 1),
                                              name[1]), 0))
            goto end;
    } else if (!TEST_ptr_null(sktmp)) {
        goto end;
    }

    /*
     * In all tests we expect certificate authorities to have been sent to the
     * client. However, SSL_set_client_CA_list() should override
     * SSL_set0_CA_list()
     */
    sktmp = SSL_get0_peer_CA_list(clientssl);
    if (!TEST_ptr(sktmp)
            || !TEST_int_eq(sk_X509_NAME_num(sktmp), 2)
            || !TEST_int_eq(X509_NAME_cmp(sk_X509_NAME_value(sktmp, 0),
                                          name[tst == 0 ? 0 : 2]), 0)
            || !TEST_int_eq(X509_NAME_cmp(sk_X509_NAME_value(sktmp, 1),
                                          name[tst == 0 ? 1 : 3]), 0))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    for (i = 0; i < OSSL_NELEM(name); i++)
        X509_NAME_free(name[i]);
    sk_X509_NAME_pop_free(sk1, X509_NAME_free);
    sk_X509_NAME_pop_free(sk2, X509_NAME_free);

    return testresult;
}
#endif

static int test_ca_names(int tst)
{
    int testresult = 1;

#ifndef OPENSSL_NO_TLS1_2
    testresult &= test_ca_names_int(TLS1_2_VERSION, tst);
#endif
#ifndef OPENSSL_NO_TLS1_3
    testresult &= test_ca_names_int(TLS1_3_VERSION, tst);
#endif

    return testresult;
}

/*
 * Test 0: Client sets servername and server acknowledges it (TLSv1.2)
 * Test 1: Client sets servername and server does not acknowledge it (TLSv1.2)
 * Test 2: Client sets inconsistent servername on resumption (TLSv1.2)
 * Test 3: Client does not set servername on initial handshake (TLSv1.2)
 * Test 4: Client does not set servername on resumption handshake (TLSv1.2)
 * Test 5: Client sets servername and server acknowledges it (TLSv1.3)
 * Test 6: Client sets servername and server does not acknowledge it (TLSv1.3)
 * Test 7: Client sets inconsistent servername on resumption (TLSv1.3)
 * Test 8: Client does not set servername on initial handshake(TLSv1.3)
 * Test 9: Client does not set servername on resumption handshake (TLSv1.3)
 */
static int test_servername(int tst)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    SSL_SESSION *sess = NULL;
    const char *sexpectedhost = NULL, *cexpectedhost = NULL;

#ifdef OPENSSL_NO_TLS1_2
    if (tst <= 4)
        return 1;
#endif
#ifdef OPENSSL_NO_TLS1_3
    if (tst >= 5)
        return 1;
#endif

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       TLS1_VERSION,
                                       (tst <= 4) ? TLS1_2_VERSION
                                                  : TLS1_3_VERSION,
                                       &sctx, &cctx, cert, privkey))
            || !TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL)))
        goto end;

    if (tst != 1 && tst != 6) {
        if (!TEST_true(SSL_CTX_set_tlsext_servername_callback(sctx,
                                                              hostname_cb)))
            goto end;
    }

    if (tst != 3 && tst != 8) {
        if (!TEST_true(SSL_set_tlsext_host_name(clientssl, "goodhost")))
            goto end;
        sexpectedhost = cexpectedhost = "goodhost";
    }

    if (!TEST_true(create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE)))
        goto end;

    if (!TEST_str_eq(SSL_get_servername(clientssl, TLSEXT_NAMETYPE_host_name),
                     cexpectedhost)
            || !TEST_str_eq(SSL_get_servername(serverssl,
                                               TLSEXT_NAMETYPE_host_name),
                            sexpectedhost))
        goto end;

    /* Now repeat with a resumption handshake */

    if (!TEST_int_eq(SSL_shutdown(clientssl), 0)
            || !TEST_ptr_ne(sess = SSL_get1_session(clientssl), NULL)
            || !TEST_true(SSL_SESSION_is_resumable(sess))
            || !TEST_int_eq(SSL_shutdown(serverssl), 0))
        goto end;

    SSL_free(clientssl);
    SSL_free(serverssl);
    clientssl = serverssl = NULL;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl, NULL,
                                      NULL)))
        goto end;

    if (!TEST_true(SSL_set_session(clientssl, sess)))
        goto end;

    sexpectedhost = cexpectedhost = "goodhost";
    if (tst == 2 || tst == 7) {
        /* Set an inconsistent hostname */
        if (!TEST_true(SSL_set_tlsext_host_name(clientssl, "altgoodhost")))
            goto end;
        /*
         * In TLSv1.2 we expect the hostname from the original handshake, in
         * TLSv1.3 we expect the hostname from this handshake
         */
        if (tst == 7)
            sexpectedhost = cexpectedhost = "altgoodhost";

        if (!TEST_str_eq(SSL_get_servername(clientssl,
                                            TLSEXT_NAMETYPE_host_name),
                         "altgoodhost"))
            goto end;
    } else if (tst == 4 || tst == 9) {
        /*
         * A TLSv1.3 session does not associate a session with a servername,
         * but a TLSv1.2 session does.
         */
        if (tst == 9)
            sexpectedhost = cexpectedhost = NULL;

        if (!TEST_str_eq(SSL_get_servername(clientssl,
                                            TLSEXT_NAMETYPE_host_name),
                         cexpectedhost))
            goto end;
    } else {
        if (!TEST_true(SSL_set_tlsext_host_name(clientssl, "goodhost")))
            goto end;
        /*
         * In a TLSv1.2 resumption where the hostname was not acknowledged
         * we expect the hostname on the server to be empty. On the client we
         * return what was requested in this case.
         *
         * Similarly if the client didn't set a hostname on an original TLSv1.2
         * session but is now, the server hostname will be empty, but the client
         * is as we set it.
         */
        if (tst == 1 || tst == 3)
            sexpectedhost = NULL;

        if (!TEST_str_eq(SSL_get_servername(clientssl,
                                            TLSEXT_NAMETYPE_host_name),
                         "goodhost"))
            goto end;
    }

    if (!TEST_true(create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE)))
        goto end;

    if (!TEST_true(SSL_session_reused(clientssl))
            || !TEST_true(SSL_session_reused(serverssl))
            || !TEST_str_eq(SSL_get_servername(clientssl,
                                               TLSEXT_NAMETYPE_host_name),
                            cexpectedhost)
            || !TEST_str_eq(SSL_get_servername(serverssl,
                                               TLSEXT_NAMETYPE_host_name),
                            sexpectedhost))
        goto end;

    testresult = 1;

 end:
    SSL_SESSION_free(sess);
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}

#ifndef OPENSSL_NO_TLS1_2
static int test_ssl_dup(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL, *client2ssl = NULL;
    int testresult = 0;
    BIO *rbio = NULL, *wbio = NULL;

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(),
                                       0,
                                       0,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl, &clientssl,
                                             NULL, NULL)))
        goto end;

    if (!TEST_true(SSL_set_min_proto_version(clientssl, TLS1_2_VERSION))
            || !TEST_true(SSL_set_max_proto_version(clientssl, TLS1_2_VERSION)))
        goto end;

    client2ssl = SSL_dup(clientssl);
    rbio = SSL_get_rbio(clientssl);
    if (!TEST_ptr(rbio)
            || !TEST_true(BIO_up_ref(rbio)))
        goto end;
    SSL_set0_rbio(client2ssl, rbio);
    rbio = NULL;

    wbio = SSL_get_wbio(clientssl);
    if (!TEST_ptr(wbio) || !TEST_true(BIO_up_ref(wbio)))
        goto end;
    SSL_set0_wbio(client2ssl, wbio);
    rbio = NULL;

    if (!TEST_ptr(client2ssl)
               /* Handshake not started so pointers should be different */
            || !TEST_ptr_ne(clientssl, client2ssl))
        goto end;

    if (!TEST_int_eq(SSL_get_min_proto_version(client2ssl), TLS1_2_VERSION)
            || !TEST_int_eq(SSL_get_max_proto_version(client2ssl), TLS1_2_VERSION))
        goto end;

    if (!TEST_true(create_ssl_connection(serverssl, client2ssl, SSL_ERROR_NONE)))
        goto end;

    SSL_free(clientssl);
    clientssl = SSL_dup(client2ssl);
    if (!TEST_ptr(clientssl)
               /* Handshake has finished so pointers should be the same */
            || !TEST_ptr_eq(clientssl, client2ssl))
        goto end;

    testresult = 1;

 end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_free(client2ssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);

    return testresult;
}
#endif

#ifndef OPENSSL_NO_TLS1_3
/*
 * Test that setting an SNI callback works with TLSv1.3. Specifically we check
 * that it works even without a certificate configured for the original
 * SSL_CTX
 */
static int test_sni_tls13(void)
{
    SSL_CTX *cctx = NULL, *sctx = NULL, *sctx2 = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;

    /* Reset callback counter */
    snicb = 0;

    /* Create an initial SSL_CTX with no certificate configured */
    sctx = SSL_CTX_new(TLS_server_method());
    if (!TEST_ptr(sctx))
        goto end;
    /* Require TLSv1.3 as a minimum */
    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(), TLS_client_method(),
                                       TLS1_3_VERSION, 0, &sctx2, &cctx, cert,
                                       privkey)))
        goto end;

    /* Set up SNI */
    if (!TEST_true(SSL_CTX_set_tlsext_servername_callback(sctx, sni_cb))
            || !TEST_true(SSL_CTX_set_tlsext_servername_arg(sctx, sctx2)))
        goto end;

    /*
     * Connection should still succeed because the final SSL_CTX has the right
     * certificates configured.
     */
    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL))
            || !TEST_true(create_ssl_connection(serverssl, clientssl,
                                                SSL_ERROR_NONE)))
        goto end;

    /* We should have had the SNI callback called exactly once */
    if (!TEST_int_eq(snicb, 1))
        goto end;

    testresult = 1;

end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx2);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}

/*
 * Test that the lifetime hint of a TLSv1.3 ticket is no more than 1 week
 * 0 = TLSv1.2
 * 1 = TLSv1.3
 */
static int test_ticket_lifetime(int idx)
{
    SSL_CTX *cctx = NULL, *sctx = NULL;
    SSL *clientssl = NULL, *serverssl = NULL;
    int testresult = 0;
    int version = TLS1_3_VERSION;

#define ONE_WEEK_SEC (7 * 24 * 60 * 60)
#define TWO_WEEK_SEC (2 * ONE_WEEK_SEC)

    if (idx == 0) {
#ifdef OPENSSL_NO_TLS1_2
        TEST_info("Skipping: TLS 1.2 is disabled.");
        return 1;
#else
        version = TLS1_2_VERSION;
#endif
    }

    if (!TEST_true(create_ssl_ctx_pair(TLS_server_method(),
                                       TLS_client_method(), version, version,
                                       &sctx, &cctx, cert, privkey)))
        goto end;

    if (!TEST_true(create_ssl_objects(sctx, cctx, &serverssl,
                                      &clientssl, NULL, NULL)))
        goto end;

    /*
     * Set the timeout to be more than 1 week
     * make sure the returned value is the default
     */
    if (!TEST_long_eq(SSL_CTX_set_timeout(sctx, TWO_WEEK_SEC),
                      SSL_get_default_timeout(serverssl)))
        goto end;

    if (!TEST_true(create_ssl_connection(serverssl, clientssl, SSL_ERROR_NONE)))
        goto end;

    if (idx == 0) {
        /* TLSv1.2 uses the set value */
        if (!TEST_ulong_eq(SSL_SESSION_get_ticket_lifetime_hint(SSL_get_session(clientssl)), TWO_WEEK_SEC))
            goto end;
    } else {
        /* TLSv1.3 uses the limited value */
        if (!TEST_ulong_le(SSL_SESSION_get_ticket_lifetime_hint(SSL_get_session(clientssl)), ONE_WEEK_SEC))
            goto end;
    }
    testresult = 1;

end:
    SSL_free(serverssl);
    SSL_free(clientssl);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    return testresult;
}
#endif
/*
 * Test that setting an ALPN does not violate RFC
 */
static int test_set_alpn(void)
{
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    int testresult = 0;

    unsigned char bad0[] = { 0x00, 'b', 'a', 'd' };
    unsigned char good[] = { 0x04, 'g', 'o', 'o', 'd' };
    unsigned char bad1[] = { 0x01, 'b', 'a', 'd' };
    unsigned char bad2[] = { 0x03, 'b', 'a', 'd', 0x00};
    unsigned char bad3[] = { 0x03, 'b', 'a', 'd', 0x01, 'b', 'a', 'd'};
    unsigned char bad4[] = { 0x03, 'b', 'a', 'd', 0x06, 'b', 'a', 'd'};

    /* Create an initial SSL_CTX with no certificate configured */
    ctx = SSL_CTX_new(TLS_server_method());
    if (!TEST_ptr(ctx))
        goto end;

    /* the set_alpn functions return 0 (false) on success, non-zero (true) on failure */
    if (!TEST_false(SSL_CTX_set_alpn_protos(ctx, NULL, 2)))
        goto end;
    if (!TEST_false(SSL_CTX_set_alpn_protos(ctx, good, 0)))
        goto end;
    if (!TEST_false(SSL_CTX_set_alpn_protos(ctx, good, sizeof(good))))
        goto end;
    if (!TEST_true(SSL_CTX_set_alpn_protos(ctx, good, 1)))
        goto end;
    if (!TEST_true(SSL_CTX_set_alpn_protos(ctx, bad0, sizeof(bad0))))
        goto end;
    if (!TEST_true(SSL_CTX_set_alpn_protos(ctx, bad1, sizeof(bad1))))
        goto end;
    if (!TEST_true(SSL_CTX_set_alpn_protos(ctx, bad2, sizeof(bad2))))
        goto end;
    if (!TEST_true(SSL_CTX_set_alpn_protos(ctx, bad3, sizeof(bad3))))
        goto end;
    if (!TEST_true(SSL_CTX_set_alpn_protos(ctx, bad4, sizeof(bad4))))
        goto end;

    ssl = SSL_new(ctx);
    if (!TEST_ptr(ssl))
        goto end;

    if (!TEST_false(SSL_set_alpn_protos(ssl, NULL, 2)))
        goto end;
    if (!TEST_false(SSL_set_alpn_protos(ssl, good, 0)))
        goto end;
    if (!TEST_false(SSL_set_alpn_protos(ssl, good, sizeof(good))))
        goto end;
    if (!TEST_true(SSL_set_alpn_protos(ssl, good, 1)))
        goto end;
    if (!TEST_true(SSL_set_alpn_protos(ssl, bad0, sizeof(bad0))))
        goto end;
    if (!TEST_true(SSL_set_alpn_protos(ssl, bad1, sizeof(bad1))))
        goto end;
    if (!TEST_true(SSL_set_alpn_protos(ssl, bad2, sizeof(bad2))))
        goto end;
    if (!TEST_true(SSL_set_alpn_protos(ssl, bad3, sizeof(bad3))))
        goto end;
    if (!TEST_true(SSL_set_alpn_protos(ssl, bad4, sizeof(bad4))))
        goto end;

    testresult = 1;

end:
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    return testresult;
}

static int test_inherit_verify_param(void)
{
    int testresult = 0;

    SSL_CTX *ctx = NULL;
    X509_VERIFY_PARAM *cp = NULL;
    SSL *ssl = NULL;
    X509_VERIFY_PARAM *sp = NULL;
    int hostflags = X509_CHECK_FLAG_NEVER_CHECK_SUBJECT;

    ctx = SSL_CTX_new(TLS_server_method());
    if (!TEST_ptr(ctx))
        goto end;

    cp = SSL_CTX_get0_param(ctx);
    if (!TEST_ptr(cp))
        goto end;
    if (!TEST_int_eq(X509_VERIFY_PARAM_get_hostflags(cp), 0))
        goto end;

    X509_VERIFY_PARAM_set_hostflags(cp, hostflags);

    ssl = SSL_new(ctx);
    if (!TEST_ptr(ssl))
        goto end;

    sp = SSL_get0_param(ssl);
    if (!TEST_ptr(sp))
        goto end;
    if (!TEST_int_eq(X509_VERIFY_PARAM_get_hostflags(sp), hostflags))
        goto end;

    testresult = 1;

 end:
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return testresult;
}

int setup_tests(void)
{
    if (!TEST_ptr(certsdir = test_get_argument(0))
            || !TEST_ptr(srpvfile = test_get_argument(1))
            || !TEST_ptr(tmpfilename = test_get_argument(2)))
        return 0;

    if (getenv("OPENSSL_TEST_GETCOUNTS") != NULL) {
#ifdef OPENSSL_NO_CRYPTO_MDEBUG
        TEST_error("not supported in this build");
        return 0;
#else
        int i, mcount, rcount, fcount;

        for (i = 0; i < 4; i++)
            test_export_key_mat(i);
        CRYPTO_get_alloc_counts(&mcount, &rcount, &fcount);
        test_printf_stdout("malloc %d realloc %d free %d\n",
                mcount, rcount, fcount);
        return 1;
#endif
    }

    cert = test_mk_file_path(certsdir, "servercert.pem");
    if (cert == NULL)
        return 0;

    privkey = test_mk_file_path(certsdir, "serverkey.pem");
    if (privkey == NULL) {
        OPENSSL_free(cert);
        return 0;
    }

    ADD_TEST(test_large_message_tls);
    ADD_TEST(test_large_message_tls_read_ahead);
#ifndef OPENSSL_NO_DTLS
    ADD_TEST(test_large_message_dtls);
#endif
#ifndef OPENSSL_NO_OCSP
    ADD_TEST(test_tlsext_status_type);
#endif
    ADD_TEST(test_session_with_only_int_cache);
    ADD_TEST(test_session_with_only_ext_cache);
    ADD_TEST(test_session_with_both_cache);
#ifndef OPENSSL_NO_TLS1_3
    ADD_ALL_TESTS(test_stateful_tickets, 3);
    ADD_ALL_TESTS(test_stateless_tickets, 3);
    ADD_TEST(test_psk_tickets);
#endif
    ADD_ALL_TESTS(test_ssl_set_bio, TOTAL_SSL_SET_BIO_TESTS);
    ADD_TEST(test_ssl_bio_pop_next_bio);
    ADD_TEST(test_ssl_bio_pop_ssl_bio);
    ADD_TEST(test_ssl_bio_change_rbio);
    ADD_TEST(test_ssl_bio_change_wbio);
#if !defined(OPENSSL_NO_TLS1_2) || defined(OPENSSL_NO_TLS1_3)
    ADD_ALL_TESTS(test_set_sigalgs, OSSL_NELEM(testsigalgs) * 2);
    ADD_TEST(test_keylog);
#endif
#ifndef OPENSSL_NO_TLS1_3
    ADD_TEST(test_keylog_no_master_key);
#endif
#ifndef OPENSSL_NO_TLS1_2
    ADD_TEST(test_client_hello_cb);
    ADD_TEST(test_ccs_change_cipher);
#endif
#ifndef OPENSSL_NO_TLS1_3
    ADD_ALL_TESTS(test_early_data_read_write, 3);
    /*
     * We don't do replay tests for external PSK. Replay protection isn't used
     * in that scenario.
     */
    ADD_ALL_TESTS(test_early_data_replay, 2);
    ADD_ALL_TESTS(test_early_data_skip, 3);
    ADD_ALL_TESTS(test_early_data_skip_hrr, 3);
    ADD_ALL_TESTS(test_early_data_skip_hrr_fail, 3);
    ADD_ALL_TESTS(test_early_data_skip_abort, 3);
    ADD_ALL_TESTS(test_early_data_not_sent, 3);
    ADD_ALL_TESTS(test_early_data_psk, 8);
    ADD_ALL_TESTS(test_early_data_psk_with_all_ciphers, 5);
    ADD_ALL_TESTS(test_early_data_not_expected, 3);
# ifndef OPENSSL_NO_TLS1_2
    ADD_ALL_TESTS(test_early_data_tls1_2, 3);
# endif
#endif
#ifndef OPENSSL_NO_TLS1_3
    ADD_ALL_TESTS(test_set_ciphersuite, 10);
    ADD_TEST(test_ciphersuite_change);
    ADD_ALL_TESTS(test_tls13_ciphersuite, 4);
#ifdef OPENSSL_NO_PSK
    ADD_ALL_TESTS(test_tls13_psk, 1);
#else
    ADD_ALL_TESTS(test_tls13_psk, 4);
#endif  /* OPENSSL_NO_PSK */
    ADD_ALL_TESTS(test_custom_exts, 6);
    ADD_TEST(test_stateless);
    ADD_TEST(test_pha_key_update);
#else
    ADD_ALL_TESTS(test_custom_exts, 3);
#endif
    ADD_ALL_TESTS(test_serverinfo, 8);
    ADD_ALL_TESTS(test_export_key_mat, 6);
#ifndef OPENSSL_NO_TLS1_3
    ADD_ALL_TESTS(test_export_key_mat_early, 3);
    ADD_TEST(test_key_update);
    ADD_ALL_TESTS(test_key_update_in_write, 2);
#endif
    ADD_ALL_TESTS(test_ssl_clear, 2);
    ADD_ALL_TESTS(test_max_fragment_len_ext, OSSL_NELEM(max_fragment_len_test));
#if !defined(OPENSSL_NO_SRP) && !defined(OPENSSL_NO_TLS1_2)
    ADD_ALL_TESTS(test_srp, 6);
#endif
    ADD_ALL_TESTS(test_info_callback, 6);
    ADD_ALL_TESTS(test_ssl_pending, 2);
    ADD_ALL_TESTS(test_ssl_get_shared_ciphers, OSSL_NELEM(shared_ciphers_data));
    ADD_ALL_TESTS(test_ticket_callbacks, 12);
    ADD_ALL_TESTS(test_shutdown, 7);
    ADD_ALL_TESTS(test_cert_cb, 6);
    ADD_ALL_TESTS(test_client_cert_cb, 2);
    ADD_ALL_TESTS(test_ca_names, 3);
    ADD_ALL_TESTS(test_servername, 10);
#ifndef OPENSSL_NO_TLS1_2
    ADD_TEST(test_ssl_dup);
#endif
#ifndef OPENSSL_NO_TLS1_3
    ADD_TEST(test_sni_tls13);
    ADD_ALL_TESTS(test_ticket_lifetime, 2);
#endif
    ADD_TEST(test_set_alpn);
    ADD_TEST(test_inherit_verify_param);
    return 1;
}

void cleanup_tests(void)
{
    OPENSSL_free(cert);
    OPENSSL_free(privkey);
    bio_s_mempacket_test_free();
    bio_s_always_retry_free();
}
