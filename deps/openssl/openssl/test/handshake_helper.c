/*
 * Copyright 2016-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>

#include <openssl/bio.h>
#include <openssl/x509_vfy.h>
#include <openssl/ssl.h>

#include "handshake_helper.h"
#include "testutil.h"

HANDSHAKE_RESULT *HANDSHAKE_RESULT_new()
{
    HANDSHAKE_RESULT *ret = OPENSSL_zalloc(sizeof(*ret));
    TEST_check(ret != NULL);
    return ret;
}

void HANDSHAKE_RESULT_free(HANDSHAKE_RESULT *result)
{
    if (result == NULL)
        return;
    OPENSSL_free(result->client_npn_negotiated);
    OPENSSL_free(result->server_npn_negotiated);
    OPENSSL_free(result->client_alpn_negotiated);
    OPENSSL_free(result->server_alpn_negotiated);
    OPENSSL_free(result);
}

/*
 * Since there appears to be no way to extract the sent/received alert
 * from the SSL object directly, we use the info callback and stash
 * the result in ex_data.
 */
typedef struct handshake_ex_data_st {
    int alert_sent;
    int num_fatal_alerts_sent;
    int alert_received;
    int session_ticket_do_not_call;
    ssl_servername_t servername;
} HANDSHAKE_EX_DATA;

typedef struct ctx_data_st {
    unsigned char *npn_protocols;
    size_t npn_protocols_len;
    unsigned char *alpn_protocols;
    size_t alpn_protocols_len;
} CTX_DATA;

/* |ctx_data| itself is stack-allocated. */
static void ctx_data_free_data(CTX_DATA *ctx_data)
{
    OPENSSL_free(ctx_data->npn_protocols);
    ctx_data->npn_protocols = NULL;
    OPENSSL_free(ctx_data->alpn_protocols);
    ctx_data->alpn_protocols = NULL;
}

static int ex_data_idx;

static void info_cb(const SSL *s, int where, int ret)
{
    if (where & SSL_CB_ALERT) {
        HANDSHAKE_EX_DATA *ex_data =
            (HANDSHAKE_EX_DATA*)(SSL_get_ex_data(s, ex_data_idx));
        if (where & SSL_CB_WRITE) {
            ex_data->alert_sent = ret;
            if (strcmp(SSL_alert_type_string(ret), "F") == 0
                || strcmp(SSL_alert_desc_string(ret), "CN") == 0)
                ex_data->num_fatal_alerts_sent++;
        } else {
            ex_data->alert_received = ret;
        }
    }
}

/* Select the appropriate server CTX.
 * Returns SSL_TLSEXT_ERR_OK if a match was found.
 * If |ignore| is 1, returns SSL_TLSEXT_ERR_NOACK on mismatch.
 * Otherwise, returns SSL_TLSEXT_ERR_ALERT_FATAL on mismatch.
 * An empty SNI extension also returns SSL_TSLEXT_ERR_NOACK.
 */
static int select_server_ctx(SSL *s, void *arg, int ignore)
{
    const char *servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);
    HANDSHAKE_EX_DATA *ex_data =
        (HANDSHAKE_EX_DATA*)(SSL_get_ex_data(s, ex_data_idx));

    if (servername == NULL) {
        ex_data->servername = SSL_TEST_SERVERNAME_SERVER1;
        return SSL_TLSEXT_ERR_NOACK;
    }

    if (strcmp(servername, "server2") == 0) {
        SSL_CTX *new_ctx = (SSL_CTX*)arg;
        SSL_set_SSL_CTX(s, new_ctx);
        /*
         * Copy over all the SSL_CTX options - reasonable behavior
         * allows testing of cases where the options between two
         * contexts differ/conflict
         */
        SSL_clear_options(s, 0xFFFFFFFFL);
        SSL_set_options(s, SSL_CTX_get_options(new_ctx));

        ex_data->servername = SSL_TEST_SERVERNAME_SERVER2;
        return SSL_TLSEXT_ERR_OK;
    } else if (strcmp(servername, "server1") == 0) {
        ex_data->servername = SSL_TEST_SERVERNAME_SERVER1;
        return SSL_TLSEXT_ERR_OK;
    } else if (ignore) {
        ex_data->servername = SSL_TEST_SERVERNAME_SERVER1;
        return SSL_TLSEXT_ERR_NOACK;
    } else {
        /* Don't set an explicit alert, to test library defaults. */
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }
}

/*
 * (RFC 6066):
 *  If the server understood the ClientHello extension but
 *  does not recognize the server name, the server SHOULD take one of two
 *  actions: either abort the handshake by sending a fatal-level
 *  unrecognized_name(112) alert or continue the handshake.
 *
 * This behaviour is up to the application to configure; we test both
 * configurations to ensure the state machine propagates the result
 * correctly.
 */
static int servername_ignore_cb(SSL *s, int *ad, void *arg)
{
    return select_server_ctx(s, arg, 1);
}

static int servername_reject_cb(SSL *s, int *ad, void *arg)
{
    return select_server_ctx(s, arg, 0);
}

static unsigned char dummy_ocsp_resp_good_val = 0xff;
static unsigned char dummy_ocsp_resp_bad_val = 0xfe;

static int server_ocsp_cb(SSL *s, void *arg)
{
    unsigned char *resp;

    resp = OPENSSL_malloc(1);
    if (resp == NULL)
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    /*
     * For the purposes of testing we just send back a dummy OCSP response
     */
    *resp = *(unsigned char *)arg;
    if (!SSL_set_tlsext_status_ocsp_resp(s, resp, 1))
        return SSL_TLSEXT_ERR_ALERT_FATAL;

    return SSL_TLSEXT_ERR_OK;
}

static int client_ocsp_cb(SSL *s, void *arg)
{
    const unsigned char *resp;
    int len;

    len = SSL_get_tlsext_status_ocsp_resp(s, &resp);
    if (len != 1 || *resp != dummy_ocsp_resp_good_val)
        return 0;

    return 1;
}

static int verify_reject_cb(X509_STORE_CTX *ctx, void *arg) {
    X509_STORE_CTX_set_error(ctx, X509_V_ERR_APPLICATION_VERIFICATION);
    return 0;
}

static int verify_accept_cb(X509_STORE_CTX *ctx, void *arg) {
    return 1;
}

static int broken_session_ticket_cb(SSL *s, unsigned char *key_name, unsigned char *iv,
                                    EVP_CIPHER_CTX *ctx, HMAC_CTX *hctx, int enc)
{
    return 0;
}

static int do_not_call_session_ticket_cb(SSL *s, unsigned char *key_name,
                                         unsigned char *iv,
                                         EVP_CIPHER_CTX *ctx,
                                         HMAC_CTX *hctx, int enc)
{
    HANDSHAKE_EX_DATA *ex_data =
        (HANDSHAKE_EX_DATA*)(SSL_get_ex_data(s, ex_data_idx));
    ex_data->session_ticket_do_not_call = 1;
    return 0;
}

/* Parse the comma-separated list into TLS format. */
static void parse_protos(const char *protos, unsigned char **out, size_t *outlen)
{
    size_t len, i, prefix;

    len = strlen(protos);

    /* Should never have reuse. */
    TEST_check(*out == NULL);

    /* Test values are small, so we omit length limit checks. */
    *out = OPENSSL_malloc(len + 1);
    TEST_check(*out != NULL);
    *outlen = len + 1;

    /*
     * foo => '3', 'f', 'o', 'o'
     * foo,bar => '3', 'f', 'o', 'o', '3', 'b', 'a', 'r'
     */
    memcpy(*out + 1, protos, len);

    prefix = 0;
    i = prefix + 1;
    while (i <= len) {
        if ((*out)[i] == ',') {
            TEST_check(i - 1 - prefix > 0);
            (*out)[prefix] = i - 1 - prefix;
            prefix = i;
        }
        i++;
    }
    TEST_check(len - prefix > 0);
    (*out)[prefix] = len - prefix;
}

#ifndef OPENSSL_NO_NEXTPROTONEG
/*
 * The client SHOULD select the first protocol advertised by the server that it
 * also supports.  In the event that the client doesn't support any of server's
 * protocols, or the server doesn't advertise any, it SHOULD select the first
 * protocol that it supports.
 */
static int client_npn_cb(SSL *s, unsigned char **out, unsigned char *outlen,
                         const unsigned char *in, unsigned int inlen,
                         void *arg)
{
    CTX_DATA *ctx_data = (CTX_DATA*)(arg);
    int ret;

    ret = SSL_select_next_proto(out, outlen, in, inlen,
                                ctx_data->npn_protocols,
                                ctx_data->npn_protocols_len);
    /* Accept both OPENSSL_NPN_NEGOTIATED and OPENSSL_NPN_NO_OVERLAP. */
    TEST_check(ret == OPENSSL_NPN_NEGOTIATED || ret == OPENSSL_NPN_NO_OVERLAP);
    return SSL_TLSEXT_ERR_OK;
}

static int server_npn_cb(SSL *s, const unsigned char **data,
                         unsigned int *len, void *arg)
{
    CTX_DATA *ctx_data = (CTX_DATA*)(arg);
    *data = ctx_data->npn_protocols;
    *len = ctx_data->npn_protocols_len;
    return SSL_TLSEXT_ERR_OK;
}
#endif

/*
 * The server SHOULD select the most highly preferred protocol that it supports
 * and that is also advertised by the client.  In the event that the server
 * supports no protocols that the client advertises, then the server SHALL
 * respond with a fatal "no_application_protocol" alert.
 */
static int server_alpn_cb(SSL *s, const unsigned char **out,
                          unsigned char *outlen, const unsigned char *in,
                          unsigned int inlen, void *arg)
{
    CTX_DATA *ctx_data = (CTX_DATA*)(arg);
    int ret;

    /* SSL_select_next_proto isn't const-correct... */
    unsigned char *tmp_out;

    /*
     * The result points either to |in| or to |ctx_data->alpn_protocols|.
     * The callback is allowed to point to |in| or to a long-lived buffer,
     * so we can return directly without storing a copy.
     */
    ret = SSL_select_next_proto(&tmp_out, outlen,
                                ctx_data->alpn_protocols,
                                ctx_data->alpn_protocols_len, in, inlen);

    *out = tmp_out;
    /* Unlike NPN, we don't tolerate a mismatch. */
    return ret == OPENSSL_NPN_NEGOTIATED ? SSL_TLSEXT_ERR_OK
        : SSL_TLSEXT_ERR_ALERT_FATAL;
}

/*
 * Configure callbacks and other properties that can't be set directly
 * in the server/client CONF.
 */
static void configure_handshake_ctx(SSL_CTX *server_ctx, SSL_CTX *server2_ctx,
                                    SSL_CTX *client_ctx,
                                    const SSL_TEST_CTX *test,
                                    const SSL_TEST_EXTRA_CONF *extra,
                                    CTX_DATA *server_ctx_data,
                                    CTX_DATA *server2_ctx_data,
                                    CTX_DATA *client_ctx_data)
{
    unsigned char *ticket_keys;
    size_t ticket_key_len;

    TEST_check(SSL_CTX_set_max_send_fragment(server_ctx,
                                             test->max_fragment_size) == 1);
    if (server2_ctx != NULL) {
        TEST_check(SSL_CTX_set_max_send_fragment(server2_ctx,
                                                 test->max_fragment_size) == 1);
    }
    TEST_check(SSL_CTX_set_max_send_fragment(client_ctx,
                                             test->max_fragment_size) == 1);

    switch (extra->client.verify_callback) {
    case SSL_TEST_VERIFY_ACCEPT_ALL:
        SSL_CTX_set_cert_verify_callback(client_ctx, &verify_accept_cb,
                                         NULL);
        break;
    case SSL_TEST_VERIFY_REJECT_ALL:
        SSL_CTX_set_cert_verify_callback(client_ctx, &verify_reject_cb,
                                         NULL);
        break;
    default:
        break;
    }

    /* link the two contexts for SNI purposes */
    switch (extra->server.servername_callback) {
    case SSL_TEST_SERVERNAME_IGNORE_MISMATCH:
        SSL_CTX_set_tlsext_servername_callback(server_ctx, servername_ignore_cb);
        SSL_CTX_set_tlsext_servername_arg(server_ctx, server2_ctx);
        break;
    case SSL_TEST_SERVERNAME_REJECT_MISMATCH:
        SSL_CTX_set_tlsext_servername_callback(server_ctx, servername_reject_cb);
        SSL_CTX_set_tlsext_servername_arg(server_ctx, server2_ctx);
        break;
    default:
        break;
    }

    if (extra->server.cert_status != SSL_TEST_CERT_STATUS_NONE) {
        SSL_CTX_set_tlsext_status_type(client_ctx, TLSEXT_STATUSTYPE_ocsp);
        SSL_CTX_set_tlsext_status_cb(client_ctx, client_ocsp_cb);
        SSL_CTX_set_tlsext_status_arg(client_ctx, NULL);
        SSL_CTX_set_tlsext_status_cb(server_ctx, server_ocsp_cb);
        SSL_CTX_set_tlsext_status_arg(server_ctx,
            ((extra->server.cert_status == SSL_TEST_CERT_STATUS_GOOD_RESPONSE)
            ? &dummy_ocsp_resp_good_val : &dummy_ocsp_resp_bad_val));
    }

    /*
     * The initial_ctx/session_ctx always handles the encrypt/decrypt of the
     * session ticket. This ticket_key callback is assigned to the second
     * session (assigned via SNI), and should never be invoked
     */
    if (server2_ctx != NULL)
        SSL_CTX_set_tlsext_ticket_key_cb(server2_ctx,
                                         do_not_call_session_ticket_cb);

    if (extra->server.broken_session_ticket) {
        SSL_CTX_set_tlsext_ticket_key_cb(server_ctx, broken_session_ticket_cb);
    }
#ifndef OPENSSL_NO_NEXTPROTONEG
    if (extra->server.npn_protocols != NULL) {
        parse_protos(extra->server.npn_protocols,
                     &server_ctx_data->npn_protocols,
                     &server_ctx_data->npn_protocols_len);
        SSL_CTX_set_next_protos_advertised_cb(server_ctx, server_npn_cb,
                                              server_ctx_data);
    }
    if (extra->server2.npn_protocols != NULL) {
        parse_protos(extra->server2.npn_protocols,
                     &server2_ctx_data->npn_protocols,
                     &server2_ctx_data->npn_protocols_len);
        TEST_check(server2_ctx != NULL);
        SSL_CTX_set_next_protos_advertised_cb(server2_ctx, server_npn_cb,
                                              server2_ctx_data);
    }
    if (extra->client.npn_protocols != NULL) {
        parse_protos(extra->client.npn_protocols,
                     &client_ctx_data->npn_protocols,
                     &client_ctx_data->npn_protocols_len);
        SSL_CTX_set_next_proto_select_cb(client_ctx, client_npn_cb,
                                         client_ctx_data);
    }
#endif
    if (extra->server.alpn_protocols != NULL) {
        parse_protos(extra->server.alpn_protocols,
                     &server_ctx_data->alpn_protocols,
                     &server_ctx_data->alpn_protocols_len);
        SSL_CTX_set_alpn_select_cb(server_ctx, server_alpn_cb, server_ctx_data);
    }
    if (extra->server2.alpn_protocols != NULL) {
        TEST_check(server2_ctx != NULL);
        parse_protos(extra->server2.alpn_protocols,
                     &server2_ctx_data->alpn_protocols,
                     &server2_ctx_data->alpn_protocols_len);
        SSL_CTX_set_alpn_select_cb(server2_ctx, server_alpn_cb, server2_ctx_data);
    }
    if (extra->client.alpn_protocols != NULL) {
        unsigned char *alpn_protos = NULL;
        size_t alpn_protos_len;
        parse_protos(extra->client.alpn_protocols,
                     &alpn_protos, &alpn_protos_len);
        /* Reversed return value convention... */
        TEST_check(SSL_CTX_set_alpn_protos(client_ctx, alpn_protos,
                                           alpn_protos_len) == 0);
        OPENSSL_free(alpn_protos);
    }

    /*
     * Use fixed session ticket keys so that we can decrypt a ticket created with
     * one CTX in another CTX. Don't address server2 for the moment.
     */
    ticket_key_len = SSL_CTX_set_tlsext_ticket_keys(server_ctx, NULL, 0);
    ticket_keys = OPENSSL_zalloc(ticket_key_len);
    TEST_check(ticket_keys != NULL);
    TEST_check(SSL_CTX_set_tlsext_ticket_keys(server_ctx, ticket_keys,
                                              ticket_key_len) == 1);
    OPENSSL_free(ticket_keys);

    /* The default log list includes EC keys, so CT can't work without EC. */
#if !defined(OPENSSL_NO_CT) && !defined(OPENSSL_NO_EC)
    TEST_check(SSL_CTX_set_default_ctlog_list_file(client_ctx));
    switch (extra->client.ct_validation) {
    case SSL_TEST_CT_VALIDATION_PERMISSIVE:
        TEST_check(SSL_CTX_enable_ct(client_ctx, SSL_CT_VALIDATION_PERMISSIVE));
        break;
    case SSL_TEST_CT_VALIDATION_STRICT:
        TEST_check(SSL_CTX_enable_ct(client_ctx, SSL_CT_VALIDATION_STRICT));
        break;
    case SSL_TEST_CT_VALIDATION_NONE:
        break;
    }
#endif
}

/* Configure per-SSL callbacks and other properties. */
static void configure_handshake_ssl(SSL *server, SSL *client,
                                    const SSL_TEST_EXTRA_CONF *extra)
{
    if (extra->client.servername != SSL_TEST_SERVERNAME_NONE)
        SSL_set_tlsext_host_name(client,
                                 ssl_servername_name(extra->client.servername));
}

/* The status for each connection phase. */
typedef enum {
    PEER_SUCCESS,
    PEER_RETRY,
    PEER_ERROR
} peer_status_t;

/* An SSL object and associated read-write buffers. */
typedef struct peer_st {
    SSL *ssl;
    /* Buffer lengths are int to match the SSL read/write API. */
    unsigned char *write_buf;
    int write_buf_len;
    unsigned char *read_buf;
    int read_buf_len;
    int bytes_to_write;
    int bytes_to_read;
    peer_status_t status;
} PEER;

static void create_peer(PEER *peer, SSL_CTX *ctx)
{
    static const int peer_buffer_size = 64 * 1024;

    peer->ssl = SSL_new(ctx);
    TEST_check(peer->ssl != NULL);
    peer->write_buf = OPENSSL_zalloc(peer_buffer_size);
    TEST_check(peer->write_buf != NULL);
    peer->read_buf = OPENSSL_zalloc(peer_buffer_size);
    TEST_check(peer->read_buf != NULL);
    peer->write_buf_len = peer->read_buf_len = peer_buffer_size;
}

static void peer_free_data(PEER *peer)
{
    SSL_free(peer->ssl);
    OPENSSL_free(peer->write_buf);
    OPENSSL_free(peer->read_buf);
}

/*
 * Note that we could do the handshake transparently under an SSL_write,
 * but separating the steps is more helpful for debugging test failures.
 */
static void do_handshake_step(PEER *peer)
{
    int ret;

    if (peer->status != PEER_RETRY) {
        peer->status = PEER_ERROR;
        return;
    }

    ret = SSL_do_handshake(peer->ssl);

    if (ret == 1) {
        peer->status = PEER_SUCCESS;
    } else if (ret == 0) {
        peer->status = PEER_ERROR;
    } else {
        int error = SSL_get_error(peer->ssl, ret);
        /* Memory bios should never block with SSL_ERROR_WANT_WRITE. */
        if (error != SSL_ERROR_WANT_READ)
            peer->status = PEER_ERROR;
    }
}

/*-
 * Send/receive some application data. The read-write sequence is
 * Peer A: (R) W - first read will yield no data
 * Peer B:  R  W
 * ...
 * Peer A:  R  W
 * Peer B:  R  W
 * Peer A:  R
 */
static void do_app_data_step(PEER *peer)
{
    int ret = 1, write_bytes;

    TEST_check(peer->status == PEER_RETRY);

    /* We read everything available... */
    while (ret > 0 && peer->bytes_to_read) {
        ret = SSL_read(peer->ssl, peer->read_buf, peer->read_buf_len);
        if (ret > 0) {
            TEST_check(ret <= peer->bytes_to_read);
            peer->bytes_to_read -= ret;
        } else if (ret == 0) {
            peer->status = PEER_ERROR;
            return;
        } else {
            int error = SSL_get_error(peer->ssl, ret);
            if (error != SSL_ERROR_WANT_READ) {
                peer->status = PEER_ERROR;
                return;
            } /* Else continue with write. */
        }
    }

    /* ... but we only write one write-buffer-full of data. */
    write_bytes = peer->bytes_to_write < peer->write_buf_len ? peer->bytes_to_write :
        peer->write_buf_len;
    if (write_bytes) {
        ret = SSL_write(peer->ssl, peer->write_buf, write_bytes);
        if (ret > 0) {
            /* SSL_write will only succeed with a complete write. */
            TEST_check(ret == write_bytes);
            peer->bytes_to_write -= ret;
        } else {
            /*
             * We should perhaps check for SSL_ERROR_WANT_READ/WRITE here
             * but this doesn't yet occur with current app data sizes.
             */
            peer->status = PEER_ERROR;
            return;
        }
    }

    /*
     * We could simply finish when there was nothing to read, and we have
     * nothing left to write. But keeping track of the expected number of bytes
     * to read gives us somewhat better guarantees that all data sent is in fact
     * received.
     */
    if (!peer->bytes_to_write && !peer->bytes_to_read) {
        peer->status = PEER_SUCCESS;
    }
}

static void do_reneg_setup_step(const SSL_TEST_CTX *test_ctx, PEER *peer)
{
    int ret;
    char buf;

    if (peer->status == PEER_SUCCESS) {
        /*
         * We are a client that succeeded this step previously, but the server
         * wanted to retry. Probably there is a no_renegotiation warning alert
         * waiting for us. Attempt to continue the handshake.
         */
        peer->status = PEER_RETRY;
        do_handshake_step(peer);
        return;
    }

    TEST_check(peer->status == PEER_RETRY);
    TEST_check(test_ctx->handshake_mode == SSL_TEST_HANDSHAKE_RENEG_SERVER
                || test_ctx->handshake_mode == SSL_TEST_HANDSHAKE_RENEG_CLIENT);

    /* Check if we are the peer that is going to initiate */
    if ((test_ctx->handshake_mode == SSL_TEST_HANDSHAKE_RENEG_SERVER
                && SSL_is_server(peer->ssl))
            || (test_ctx->handshake_mode == SSL_TEST_HANDSHAKE_RENEG_CLIENT
                && !SSL_is_server(peer->ssl))) {
        /*
         * If we already asked for a renegotiation then fall through to the
         * SSL_read() below.
         */
        if (!SSL_renegotiate_pending(peer->ssl)) {
            /*
             * If we are the client we will always attempt to resume the
             * session. The server may or may not resume dependant on the
             * setting of SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
             */
            if (SSL_is_server(peer->ssl)) {
                ret = SSL_renegotiate(peer->ssl);
            } else {
                if (test_ctx->extra.client.reneg_ciphers != NULL) {
                    if (!SSL_set_cipher_list(peer->ssl,
                                test_ctx->extra.client.reneg_ciphers)) {
                        peer->status = PEER_ERROR;
                        return;
                    }
                    ret = SSL_renegotiate(peer->ssl);
                } else {
                    ret = SSL_renegotiate_abbreviated(peer->ssl);
                }
            }
            if (!ret) {
                peer->status = PEER_ERROR;
                return;
            }
            do_handshake_step(peer);
            /*
             * If status is PEER_RETRY it means we're waiting on the peer to
             * continue the handshake. As far as setting up the renegotiation is
             * concerned that is a success. The next step will continue the
             * handshake to its conclusion.
             *
             * If status is PEER_SUCCESS then we are the server and we have
             * successfully sent the HelloRequest. We need to continue to wait
             * until the handshake arrives from the client.
             */
            if (peer->status == PEER_RETRY)
                peer->status = PEER_SUCCESS;
            else if (peer->status == PEER_SUCCESS)
                peer->status = PEER_RETRY;
            return;
        }
    }

    /*
     * The SSL object is still expecting app data, even though it's going to
     * get a handshake message. We try to read, and it should fail - after which
     * we should be in a handshake
     */
    ret = SSL_read(peer->ssl, &buf, sizeof(buf));
    if (ret >= 0) {
        /*
         * We're not actually expecting data - we're expecting a reneg to
         * start
         */
        peer->status = PEER_ERROR;
        return;
    } else {
        int error = SSL_get_error(peer->ssl, ret);
        if (error != SSL_ERROR_WANT_READ) {
            peer->status = PEER_ERROR;
            return;
        }
        /* If we're no in init yet then we're not done with setup yet */
        if (!SSL_in_init(peer->ssl))
            return;
    }

    peer->status = PEER_SUCCESS;
}


/*
 * RFC 5246 says:
 *
 * Note that as of TLS 1.1,
 *     failure to properly close a connection no longer requires that a
 *     session not be resumed.  This is a change from TLS 1.0 to conform
 *     with widespread implementation practice.
 *
 * However,
 * (a) OpenSSL requires that a connection be shutdown for all protocol versions.
 * (b) We test lower versions, too.
 * So we just implement shutdown. We do a full bidirectional shutdown so that we
 * can compare sent and received close_notify alerts and get some test coverage
 * for SSL_shutdown as a bonus.
 */
static void do_shutdown_step(PEER *peer)
{
    int ret;

    TEST_check(peer->status == PEER_RETRY);
    ret = SSL_shutdown(peer->ssl);

    if (ret == 1) {
        peer->status = PEER_SUCCESS;
    } else if (ret < 0) { /* On 0, we retry. */
        int error = SSL_get_error(peer->ssl, ret);
        /* Memory bios should never block with SSL_ERROR_WANT_WRITE. */
        if (error != SSL_ERROR_WANT_READ)
            peer->status = PEER_ERROR;
    }
}

typedef enum {
    HANDSHAKE,
    RENEG_APPLICATION_DATA,
    RENEG_SETUP,
    RENEG_HANDSHAKE,
    APPLICATION_DATA,
    SHUTDOWN,
    CONNECTION_DONE
} connect_phase_t;

static connect_phase_t next_phase(const SSL_TEST_CTX *test_ctx,
                                  connect_phase_t phase)
{
    switch (phase) {
    case HANDSHAKE:
        if (test_ctx->handshake_mode == SSL_TEST_HANDSHAKE_RENEG_SERVER
                || test_ctx->handshake_mode == SSL_TEST_HANDSHAKE_RENEG_CLIENT)
            return RENEG_APPLICATION_DATA;
        return APPLICATION_DATA;
    case RENEG_APPLICATION_DATA:
        return RENEG_SETUP;
    case RENEG_SETUP:
        return RENEG_HANDSHAKE;
    case RENEG_HANDSHAKE:
        return APPLICATION_DATA;
    case APPLICATION_DATA:
        return SHUTDOWN;
    case SHUTDOWN:
        return CONNECTION_DONE;
    default:
        TEST_check(0); /* Should never call next_phase when done. */
    }
}

static void do_connect_step(const SSL_TEST_CTX *test_ctx, PEER *peer,
                            connect_phase_t phase)
{
    switch (phase) {
    case HANDSHAKE:
        do_handshake_step(peer);
        break;
    case RENEG_APPLICATION_DATA:
        do_app_data_step(peer);
        break;
    case RENEG_SETUP:
        do_reneg_setup_step(test_ctx, peer);
        break;
    case RENEG_HANDSHAKE:
        do_handshake_step(peer);
        break;
    case APPLICATION_DATA:
        do_app_data_step(peer);
        break;
    case SHUTDOWN:
        do_shutdown_step(peer);
        break;
    default:
        TEST_check(0);
    }
}

typedef enum {
    /* Both parties succeeded. */
    HANDSHAKE_SUCCESS,
    /* Client errored. */
    CLIENT_ERROR,
    /* Server errored. */
    SERVER_ERROR,
    /* Peers are in inconsistent state. */
    INTERNAL_ERROR,
    /* One or both peers not done. */
    HANDSHAKE_RETRY
} handshake_status_t;

/*
 * Determine the handshake outcome.
 * last_status: the status of the peer to have acted last.
 * previous_status: the status of the peer that didn't act last.
 * client_spoke_last: 1 if the client went last.
 */
static handshake_status_t handshake_status(peer_status_t last_status,
                                           peer_status_t previous_status,
                                           int client_spoke_last)
{
    switch (last_status) {
    case PEER_SUCCESS:
        switch (previous_status) {
        case PEER_SUCCESS:
            /* Both succeeded. */
            return HANDSHAKE_SUCCESS;
        case PEER_RETRY:
            /* Let the first peer finish. */
            return HANDSHAKE_RETRY;
        case PEER_ERROR:
            /*
             * Second peer succeeded despite the fact that the first peer
             * already errored. This shouldn't happen.
             */
            return INTERNAL_ERROR;
        }
        break;

    case PEER_RETRY:
        return HANDSHAKE_RETRY;

    case PEER_ERROR:
        switch (previous_status) {
        case PEER_SUCCESS:
            /*
             * First peer succeeded but second peer errored.
             * TODO(emilia): we should be able to continue here (with some
             * application data?) to ensure the first peer receives the
             * alert / close_notify.
             * (No tests currently exercise this branch.)
             */
            return client_spoke_last ? CLIENT_ERROR : SERVER_ERROR;
        case PEER_RETRY:
            /* We errored; let the peer finish. */
            return HANDSHAKE_RETRY;
        case PEER_ERROR:
            /* Both peers errored. Return the one that errored first. */
            return client_spoke_last ? SERVER_ERROR : CLIENT_ERROR;
        }
    }
    /* Control should never reach here. */
    return INTERNAL_ERROR;
}

/* Convert unsigned char buf's that shouldn't contain any NUL-bytes to char. */
static char *dup_str(const unsigned char *in, size_t len)
{
    char *ret;

    if(len == 0)
        return NULL;

    /* Assert that the string does not contain NUL-bytes. */
    TEST_check(OPENSSL_strnlen((const char*)(in), len) == len);
    ret = OPENSSL_strndup((const char*)(in), len);
    TEST_check(ret != NULL);
    return ret;
}

/*
 * Note that |extra| points to the correct client/server configuration
 * within |test_ctx|. When configuring the handshake, general mode settings
 * are taken from |test_ctx|, and client/server-specific settings should be
 * taken from |extra|.
 *
 * The configuration code should never reach into |test_ctx->extra| or
 * |test_ctx->resume_extra| directly.
 *
 * (We could refactor test mode settings into a substructure. This would result
 * in cleaner argument passing but would complicate the test configuration
 * parsing.)
 */
static HANDSHAKE_RESULT *do_handshake_internal(
    SSL_CTX *server_ctx, SSL_CTX *server2_ctx, SSL_CTX *client_ctx,
    const SSL_TEST_CTX *test_ctx, const SSL_TEST_EXTRA_CONF *extra,
    SSL_SESSION *session_in, SSL_SESSION **session_out)
{
    PEER server, client;
    BIO *client_to_server, *server_to_client;
    HANDSHAKE_EX_DATA server_ex_data, client_ex_data;
    CTX_DATA client_ctx_data, server_ctx_data, server2_ctx_data;
    HANDSHAKE_RESULT *ret = HANDSHAKE_RESULT_new();
    int client_turn = 1, client_turn_count = 0;
    connect_phase_t phase = HANDSHAKE;
    handshake_status_t status = HANDSHAKE_RETRY;
    const unsigned char* tick = NULL;
    size_t tick_len = 0;
    SSL_SESSION* sess = NULL;
    const unsigned char *proto = NULL;
    /* API dictates unsigned int rather than size_t. */
    unsigned int proto_len = 0;
    EVP_PKEY *tmp_key;

    memset(&server_ctx_data, 0, sizeof(server_ctx_data));
    memset(&server2_ctx_data, 0, sizeof(server2_ctx_data));
    memset(&client_ctx_data, 0, sizeof(client_ctx_data));
    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    configure_handshake_ctx(server_ctx, server2_ctx, client_ctx, test_ctx, extra,
                            &server_ctx_data, &server2_ctx_data, &client_ctx_data);

    /* Setup SSL and buffers; additional configuration happens below. */
    create_peer(&server, server_ctx);
    create_peer(&client, client_ctx);

    server.bytes_to_write = client.bytes_to_read = test_ctx->app_data_size;
    client.bytes_to_write = server.bytes_to_read = test_ctx->app_data_size;

    configure_handshake_ssl(server.ssl, client.ssl, extra);
    if (session_in != NULL) {
        /* In case we're testing resumption without tickets. */
        TEST_check(SSL_CTX_add_session(server_ctx, session_in));
        TEST_check(SSL_set_session(client.ssl, session_in));
    }

    memset(&server_ex_data, 0, sizeof(server_ex_data));
    memset(&client_ex_data, 0, sizeof(client_ex_data));

    ret->result = SSL_TEST_INTERNAL_ERROR;

    client_to_server = BIO_new(BIO_s_mem());
    server_to_client = BIO_new(BIO_s_mem());

    TEST_check(client_to_server != NULL);
    TEST_check(server_to_client != NULL);

    /* Non-blocking bio. */
    BIO_set_nbio(client_to_server, 1);
    BIO_set_nbio(server_to_client, 1);

    SSL_set_connect_state(client.ssl);
    SSL_set_accept_state(server.ssl);

    /* The bios are now owned by the SSL object. */
    SSL_set_bio(client.ssl, server_to_client, client_to_server);
    TEST_check(BIO_up_ref(server_to_client) > 0);
    TEST_check(BIO_up_ref(client_to_server) > 0);
    SSL_set_bio(server.ssl, client_to_server, server_to_client);

    ex_data_idx = SSL_get_ex_new_index(0, "ex data", NULL, NULL, NULL);
    TEST_check(ex_data_idx >= 0);

    TEST_check(SSL_set_ex_data(server.ssl, ex_data_idx, &server_ex_data) == 1);
    TEST_check(SSL_set_ex_data(client.ssl, ex_data_idx, &client_ex_data) == 1);

    SSL_set_info_callback(server.ssl, &info_cb);
    SSL_set_info_callback(client.ssl, &info_cb);

    client.status = server.status = PEER_RETRY;

    /*
     * Half-duplex handshake loop.
     * Client and server speak to each other synchronously in the same process.
     * We use non-blocking BIOs, so whenever one peer blocks for read, it
     * returns PEER_RETRY to indicate that it's the other peer's turn to write.
     * The handshake succeeds once both peers have succeeded. If one peer
     * errors out, we also let the other peer retry (and presumably fail).
     */
    for(;;) {
        if (client_turn) {
            do_connect_step(test_ctx, &client, phase);
            status = handshake_status(client.status, server.status,
                                      1 /* client went last */);
        } else {
            do_connect_step(test_ctx, &server, phase);
            status = handshake_status(server.status, client.status,
                                      0 /* server went last */);
        }

        switch (status) {
        case HANDSHAKE_SUCCESS:
            client_turn_count = 0;
            phase = next_phase(test_ctx, phase);
            if (phase == CONNECTION_DONE) {
                ret->result = SSL_TEST_SUCCESS;
                goto err;
            } else {
                client.status = server.status = PEER_RETRY;
                /*
                 * For now, client starts each phase. Since each phase is
                 * started separately, we can later control this more
                 * precisely, for example, to test client-initiated and
                 * server-initiated shutdown.
                 */
                client_turn = 1;
                break;
            }
        case CLIENT_ERROR:
            ret->result = SSL_TEST_CLIENT_FAIL;
            goto err;
        case SERVER_ERROR:
            ret->result = SSL_TEST_SERVER_FAIL;
            goto err;
        case INTERNAL_ERROR:
            ret->result = SSL_TEST_INTERNAL_ERROR;
            goto err;
        case HANDSHAKE_RETRY:
            if (client_turn_count++ >= 2000) {
                /*
                 * At this point, there's been so many PEER_RETRY in a row
                 * that it's likely both sides are stuck waiting for a read.
                 * It's time to give up.
                 */
                ret->result = SSL_TEST_INTERNAL_ERROR;
                goto err;
            }

            /* Continue. */
            client_turn ^= 1;
            break;
        }
    }
 err:
    ret->server_alert_sent = server_ex_data.alert_sent;
    ret->server_num_fatal_alerts_sent = server_ex_data.num_fatal_alerts_sent;
    ret->server_alert_received = client_ex_data.alert_received;
    ret->client_alert_sent = client_ex_data.alert_sent;
    ret->client_num_fatal_alerts_sent = client_ex_data.num_fatal_alerts_sent;
    ret->client_alert_received = server_ex_data.alert_received;
    ret->server_protocol = SSL_version(server.ssl);
    ret->client_protocol = SSL_version(client.ssl);
    ret->servername = server_ex_data.servername;
    if ((sess = SSL_get0_session(client.ssl)) != NULL)
        SSL_SESSION_get0_ticket(sess, &tick, &tick_len);
    if (tick == NULL || tick_len == 0)
        ret->session_ticket = SSL_TEST_SESSION_TICKET_NO;
    else
        ret->session_ticket = SSL_TEST_SESSION_TICKET_YES;
    ret->session_ticket_do_not_call = server_ex_data.session_ticket_do_not_call;

#ifndef OPENSSL_NO_NEXTPROTONEG
    SSL_get0_next_proto_negotiated(client.ssl, &proto, &proto_len);
    ret->client_npn_negotiated = dup_str(proto, proto_len);

    SSL_get0_next_proto_negotiated(server.ssl, &proto, &proto_len);
    ret->server_npn_negotiated = dup_str(proto, proto_len);
#endif

    SSL_get0_alpn_selected(client.ssl, &proto, &proto_len);
    ret->client_alpn_negotiated = dup_str(proto, proto_len);

    SSL_get0_alpn_selected(server.ssl, &proto, &proto_len);
    ret->server_alpn_negotiated = dup_str(proto, proto_len);

    ret->client_resumed = SSL_session_reused(client.ssl);
    ret->server_resumed = SSL_session_reused(server.ssl);

    if (session_out != NULL)
        *session_out = SSL_get1_session(client.ssl);

    if (SSL_get_server_tmp_key(client.ssl, &tmp_key)) {
        int nid = EVP_PKEY_id(tmp_key);

#ifndef OPENSSL_NO_EC
        if (nid == EVP_PKEY_EC) {
            EC_KEY *ec = EVP_PKEY_get0_EC_KEY(tmp_key);
            nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec));
        }
#endif
        EVP_PKEY_free(tmp_key);
        ret->tmp_key_type = nid;
    }

    ctx_data_free_data(&server_ctx_data);
    ctx_data_free_data(&server2_ctx_data);
    ctx_data_free_data(&client_ctx_data);

    peer_free_data(&server);
    peer_free_data(&client);
    return ret;
}

HANDSHAKE_RESULT *do_handshake(SSL_CTX *server_ctx, SSL_CTX *server2_ctx,
                               SSL_CTX *client_ctx, SSL_CTX *resume_server_ctx,
                               SSL_CTX *resume_client_ctx,
                               const SSL_TEST_CTX *test_ctx)
{
    HANDSHAKE_RESULT *result;
    SSL_SESSION *session = NULL;

    result = do_handshake_internal(server_ctx, server2_ctx, client_ctx,
                                   test_ctx, &test_ctx->extra,
                                   NULL, &session);
    if (test_ctx->handshake_mode != SSL_TEST_HANDSHAKE_RESUME)
        goto end;

    if (result->result != SSL_TEST_SUCCESS) {
        result->result = SSL_TEST_FIRST_HANDSHAKE_FAILED;
        goto end;
    }

    HANDSHAKE_RESULT_free(result);
    /* We don't support SNI on second handshake yet, so server2_ctx is NULL. */
    result = do_handshake_internal(resume_server_ctx, NULL, resume_client_ctx,
                                   test_ctx, &test_ctx->resume_extra,
                                   session, NULL);
 end:
    SSL_SESSION_free(session);
    return result;
}
