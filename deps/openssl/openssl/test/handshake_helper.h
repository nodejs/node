/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_HANDSHAKE_HELPER_H
#define HEADER_HANDSHAKE_HELPER_H

#include "ssl_test_ctx.h"

typedef struct handshake_result {
    ssl_test_result_t result;
    /* These alerts are in the 2-byte format returned by the info_callback. */
    /* (Latest) alert sent by the client; 0 if no alert. */
    int client_alert_sent;
    /* Number of fatal or close_notify alerts sent. */
    int client_num_fatal_alerts_sent;
    /* (Latest) alert received by the server; 0 if no alert. */
    int client_alert_received;
    /* (Latest) alert sent by the server; 0 if no alert. */
    int server_alert_sent;
    /* Number of fatal or close_notify alerts sent. */
    int server_num_fatal_alerts_sent;
    /* (Latest) alert received by the client; 0 if no alert. */
    int server_alert_received;
    /* Negotiated protocol. On success, these should always match. */
    int server_protocol;
    int client_protocol;
    /* Server connection */
    ssl_servername_t servername;
    /* Session ticket status */
    ssl_session_ticket_t session_ticket;
    /* Was this called on the second context? */
    int session_ticket_do_not_call;
    char *client_npn_negotiated;
    char *server_npn_negotiated;
    char *client_alpn_negotiated;
    char *server_alpn_negotiated;
    /* Was the handshake resumed? */
    int client_resumed;
    int server_resumed;
    /* Temporary key type */
    int tmp_key_type;
} HANDSHAKE_RESULT;

HANDSHAKE_RESULT *HANDSHAKE_RESULT_new(void);
void HANDSHAKE_RESULT_free(HANDSHAKE_RESULT *result);

/* Do a handshake and report some information about the result. */
HANDSHAKE_RESULT *do_handshake(SSL_CTX *server_ctx, SSL_CTX *server2_ctx,
                               SSL_CTX *client_ctx, SSL_CTX *resume_server_ctx,
                               SSL_CTX *resume_client_ctx,
                               const SSL_TEST_CTX *test_ctx);

#endif  /* HEADER_HANDSHAKE_HELPER_H */
