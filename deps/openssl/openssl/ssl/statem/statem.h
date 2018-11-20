/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*****************************************************************************
 *                                                                           *
 * These enums should be considered PRIVATE to the state machine. No         *
 * non-state machine code should need to use these                           *
 *                                                                           *
 *****************************************************************************/
/*
 * Valid return codes used for functions performing work prior to or after
 * sending or receiving a message
 */
typedef enum {
    /* Something went wrong */
    WORK_ERROR,
    /* We're done working and there shouldn't be anything else to do after */
    WORK_FINISHED_STOP,
    /* We're done working move onto the next thing */
    WORK_FINISHED_CONTINUE,
    /* We're working on phase A */
    WORK_MORE_A,
    /* We're working on phase B */
    WORK_MORE_B
} WORK_STATE;

/* Write transition return codes */
typedef enum {
    /* Something went wrong */
    WRITE_TRAN_ERROR,
    /* A transition was successfully completed and we should continue */
    WRITE_TRAN_CONTINUE,
    /* There is no more write work to be done */
    WRITE_TRAN_FINISHED
} WRITE_TRAN;

/* Message flow states */
typedef enum {
    /* No handshake in progress */
    MSG_FLOW_UNINITED,
    /* A permanent error with this connection */
    MSG_FLOW_ERROR,
    /* We are about to renegotiate */
    MSG_FLOW_RENEGOTIATE,
    /* We are reading messages */
    MSG_FLOW_READING,
    /* We are writing messages */
    MSG_FLOW_WRITING,
    /* Handshake has finished */
    MSG_FLOW_FINISHED
} MSG_FLOW_STATE;

/* Read states */
typedef enum {
    READ_STATE_HEADER,
    READ_STATE_BODY,
    READ_STATE_POST_PROCESS
} READ_STATE;

/* Write states */
typedef enum {
    WRITE_STATE_TRANSITION,
    WRITE_STATE_PRE_WORK,
    WRITE_STATE_SEND,
    WRITE_STATE_POST_WORK
} WRITE_STATE;

/*****************************************************************************
 *                                                                           *
 * This structure should be considered "opaque" to anything outside of the   *
 * state machine. No non-state machine code should be accessing the members  *
 * of this structure.                                                        *
 *                                                                           *
 *****************************************************************************/

struct ossl_statem_st {
    MSG_FLOW_STATE state;
    WRITE_STATE write_state;
    WORK_STATE write_state_work;
    READ_STATE read_state;
    WORK_STATE read_state_work;
    OSSL_HANDSHAKE_STATE hand_state;
    int in_init;
    int read_state_first_init;
    /* true when we are actually in SSL_accept() or SSL_connect() */
    int in_handshake;
    /* Should we skip the CertificateVerify message? */
    unsigned int no_cert_verify;
    int use_timer;
};
typedef struct ossl_statem_st OSSL_STATEM;

/*****************************************************************************
 *                                                                           *
 * The following macros/functions represent the libssl internal API to the   *
 * state machine. Any libssl code may call these functions/macros            *
 *                                                                           *
 *****************************************************************************/

__owur int ossl_statem_accept(SSL *s);
__owur int ossl_statem_connect(SSL *s);
void ossl_statem_clear(SSL *s);
void ossl_statem_set_renegotiate(SSL *s);
void ossl_statem_set_error(SSL *s);
int ossl_statem_in_error(const SSL *s);
void ossl_statem_set_in_init(SSL *s, int init);
int ossl_statem_get_in_handshake(SSL *s);
void ossl_statem_set_in_handshake(SSL *s, int inhand);
void ossl_statem_set_hello_verify_done(SSL *s);
__owur int ossl_statem_app_data_allowed(SSL *s);
