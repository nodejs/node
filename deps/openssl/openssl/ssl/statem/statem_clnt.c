/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * ECC cipher suite support in OpenSSL originally written by
 * Vipul Gupta and Sumit Gupta of Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

#include <stdio.h>
#include "../ssl_locl.h"
#include "statem_locl.h"
#include <openssl/buffer.h>
#include <openssl/rand.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/engine.h>

static ossl_inline int cert_req_allowed(SSL *s);
static int key_exchange_expected(SSL *s);
static int ca_dn_cmp(const X509_NAME *const *a, const X509_NAME *const *b);
static int ssl_cipher_list_to_bytes(SSL *s, STACK_OF(SSL_CIPHER) *sk,
                                    unsigned char *p);

/*
 * Is a CertificateRequest message allowed at the moment or not?
 *
 *  Return values are:
 *  1: Yes
 *  0: No
 */
static ossl_inline int cert_req_allowed(SSL *s)
{
    /* TLS does not like anon-DH with client cert */
    if ((s->version > SSL3_VERSION
         && (s->s3->tmp.new_cipher->algorithm_auth & SSL_aNULL))
        || (s->s3->tmp.new_cipher->algorithm_auth & (SSL_aSRP | SSL_aPSK)))
        return 0;

    return 1;
}

/*
 * Should we expect the ServerKeyExchange message or not?
 *
 *  Return values are:
 *  1: Yes
 *  0: No
 */
static int key_exchange_expected(SSL *s)
{
    long alg_k = s->s3->tmp.new_cipher->algorithm_mkey;

    /*
     * Can't skip server key exchange if this is an ephemeral
     * ciphersuite or for SRP
     */
    if (alg_k & (SSL_kDHE | SSL_kECDHE | SSL_kDHEPSK | SSL_kECDHEPSK
                 | SSL_kSRP)) {
        return 1;
    }

    return 0;
}

/*
 * ossl_statem_client_read_transition() encapsulates the logic for the allowed
 * handshake state transitions when the client is reading messages from the
 * server. The message type that the server has sent is provided in |mt|. The
 * current state is in |s->statem.hand_state|.
 *
 *  Return values are:
 *  1: Success (transition allowed)
 *  0: Error (transition not allowed)
 */
int ossl_statem_client_read_transition(SSL *s, int mt)
{
    OSSL_STATEM *st = &s->statem;
    int ske_expected;

    switch (st->hand_state) {
    case TLS_ST_CW_CLNT_HELLO:
        if (mt == SSL3_MT_SERVER_HELLO) {
            st->hand_state = TLS_ST_CR_SRVR_HELLO;
            return 1;
        }

        if (SSL_IS_DTLS(s)) {
            if (mt == DTLS1_MT_HELLO_VERIFY_REQUEST) {
                st->hand_state = DTLS_ST_CR_HELLO_VERIFY_REQUEST;
                return 1;
            }
        }
        break;

    case TLS_ST_CR_SRVR_HELLO:
        if (s->hit) {
            if (s->tlsext_ticket_expected) {
                if (mt == SSL3_MT_NEWSESSION_TICKET) {
                    st->hand_state = TLS_ST_CR_SESSION_TICKET;
                    return 1;
                }
            } else if (mt == SSL3_MT_CHANGE_CIPHER_SPEC) {
                st->hand_state = TLS_ST_CR_CHANGE;
                return 1;
            }
        } else {
            if (SSL_IS_DTLS(s) && mt == DTLS1_MT_HELLO_VERIFY_REQUEST) {
                st->hand_state = DTLS_ST_CR_HELLO_VERIFY_REQUEST;
                return 1;
            } else if (s->version >= TLS1_VERSION
                       && s->tls_session_secret_cb != NULL
                       && s->session->tlsext_tick != NULL
                       && mt == SSL3_MT_CHANGE_CIPHER_SPEC) {
                /*
                 * Normally, we can tell if the server is resuming the session
                 * from the session ID. EAP-FAST (RFC 4851), however, relies on
                 * the next server message after the ServerHello to determine if
                 * the server is resuming.
                 */
                s->hit = 1;
                st->hand_state = TLS_ST_CR_CHANGE;
                return 1;
            } else if (!(s->s3->tmp.new_cipher->algorithm_auth
                         & (SSL_aNULL | SSL_aSRP | SSL_aPSK))) {
                if (mt == SSL3_MT_CERTIFICATE) {
                    st->hand_state = TLS_ST_CR_CERT;
                    return 1;
                }
            } else {
                ske_expected = key_exchange_expected(s);
                /* SKE is optional for some PSK ciphersuites */
                if (ske_expected
                    || ((s->s3->tmp.new_cipher->algorithm_mkey & SSL_PSK)
                        && mt == SSL3_MT_SERVER_KEY_EXCHANGE)) {
                    if (mt == SSL3_MT_SERVER_KEY_EXCHANGE) {
                        st->hand_state = TLS_ST_CR_KEY_EXCH;
                        return 1;
                    }
                } else if (mt == SSL3_MT_CERTIFICATE_REQUEST
                           && cert_req_allowed(s)) {
                    st->hand_state = TLS_ST_CR_CERT_REQ;
                    return 1;
                } else if (mt == SSL3_MT_SERVER_DONE) {
                    st->hand_state = TLS_ST_CR_SRVR_DONE;
                    return 1;
                }
            }
        }
        break;

    case TLS_ST_CR_CERT:
        /*
         * The CertificateStatus message is optional even if
         * |tlsext_status_expected| is set
         */
        if (s->tlsext_status_expected && mt == SSL3_MT_CERTIFICATE_STATUS) {
            st->hand_state = TLS_ST_CR_CERT_STATUS;
            return 1;
        }
        /* Fall through */

    case TLS_ST_CR_CERT_STATUS:
        ske_expected = key_exchange_expected(s);
        /* SKE is optional for some PSK ciphersuites */
        if (ske_expected || ((s->s3->tmp.new_cipher->algorithm_mkey & SSL_PSK)
                             && mt == SSL3_MT_SERVER_KEY_EXCHANGE)) {
            if (mt == SSL3_MT_SERVER_KEY_EXCHANGE) {
                st->hand_state = TLS_ST_CR_KEY_EXCH;
                return 1;
            }
            goto err;
        }
        /* Fall through */

    case TLS_ST_CR_KEY_EXCH:
        if (mt == SSL3_MT_CERTIFICATE_REQUEST) {
            if (cert_req_allowed(s)) {
                st->hand_state = TLS_ST_CR_CERT_REQ;
                return 1;
            }
            goto err;
        }
        /* Fall through */

    case TLS_ST_CR_CERT_REQ:
        if (mt == SSL3_MT_SERVER_DONE) {
            st->hand_state = TLS_ST_CR_SRVR_DONE;
            return 1;
        }
        break;

    case TLS_ST_CW_FINISHED:
        if (s->tlsext_ticket_expected) {
            if (mt == SSL3_MT_NEWSESSION_TICKET) {
                st->hand_state = TLS_ST_CR_SESSION_TICKET;
                return 1;
            }
        } else if (mt == SSL3_MT_CHANGE_CIPHER_SPEC) {
            st->hand_state = TLS_ST_CR_CHANGE;
            return 1;
        }
        break;

    case TLS_ST_CR_SESSION_TICKET:
        if (mt == SSL3_MT_CHANGE_CIPHER_SPEC) {
            st->hand_state = TLS_ST_CR_CHANGE;
            return 1;
        }
        break;

    case TLS_ST_CR_CHANGE:
        if (mt == SSL3_MT_FINISHED) {
            st->hand_state = TLS_ST_CR_FINISHED;
            return 1;
        }
        break;

    default:
        break;
    }

 err:
    /* No valid transition found */
    if (SSL_IS_DTLS(s) && mt == SSL3_MT_CHANGE_CIPHER_SPEC) {
        BIO *rbio;

        /*
         * CCS messages don't have a message sequence number so this is probably
         * because of an out-of-order CCS. We'll just drop it.
         */
        s->init_num = 0;
        s->rwstate = SSL_READING;
        rbio = SSL_get_rbio(s);
        BIO_clear_retry_flags(rbio);
        BIO_set_retry_read(rbio);
        return 0;
    }
    ossl_statem_set_error(s);
    ssl3_send_alert(s, SSL3_AL_FATAL, SSL3_AD_UNEXPECTED_MESSAGE);
    SSLerr(SSL_F_OSSL_STATEM_CLIENT_READ_TRANSITION, SSL_R_UNEXPECTED_MESSAGE);
    return 0;
}

/*
 * client_write_transition() works out what handshake state to move to next
 * when the client is writing messages to be sent to the server.
 */
WRITE_TRAN ossl_statem_client_write_transition(SSL *s)
{
    OSSL_STATEM *st = &s->statem;

    switch (st->hand_state) {
    case TLS_ST_OK:
        /* Renegotiation - fall through */
    case TLS_ST_BEFORE:
        st->hand_state = TLS_ST_CW_CLNT_HELLO;
        return WRITE_TRAN_CONTINUE;

    case TLS_ST_CW_CLNT_HELLO:
        /*
         * No transition at the end of writing because we don't know what
         * we will be sent
         */
        return WRITE_TRAN_FINISHED;

    case DTLS_ST_CR_HELLO_VERIFY_REQUEST:
        st->hand_state = TLS_ST_CW_CLNT_HELLO;
        return WRITE_TRAN_CONTINUE;

    case TLS_ST_CR_SRVR_DONE:
        if (s->s3->tmp.cert_req)
            st->hand_state = TLS_ST_CW_CERT;
        else
            st->hand_state = TLS_ST_CW_KEY_EXCH;
        return WRITE_TRAN_CONTINUE;

    case TLS_ST_CW_CERT:
        st->hand_state = TLS_ST_CW_KEY_EXCH;
        return WRITE_TRAN_CONTINUE;

    case TLS_ST_CW_KEY_EXCH:
        /*
         * For TLS, cert_req is set to 2, so a cert chain of nothing is
         * sent, but no verify packet is sent
         */
        /*
         * XXX: For now, we do not support client authentication in ECDH
         * cipher suites with ECDH (rather than ECDSA) certificates. We
         * need to skip the certificate verify message when client's
         * ECDH public key is sent inside the client certificate.
         */
        if (s->s3->tmp.cert_req == 1) {
            st->hand_state = TLS_ST_CW_CERT_VRFY;
        } else {
            st->hand_state = TLS_ST_CW_CHANGE;
        }
        if (s->s3->flags & TLS1_FLAGS_SKIP_CERT_VERIFY) {
            st->hand_state = TLS_ST_CW_CHANGE;
        }
        return WRITE_TRAN_CONTINUE;

    case TLS_ST_CW_CERT_VRFY:
        st->hand_state = TLS_ST_CW_CHANGE;
        return WRITE_TRAN_CONTINUE;

    case TLS_ST_CW_CHANGE:
#if defined(OPENSSL_NO_NEXTPROTONEG)
        st->hand_state = TLS_ST_CW_FINISHED;
#else
        if (!SSL_IS_DTLS(s) && s->s3->next_proto_neg_seen)
            st->hand_state = TLS_ST_CW_NEXT_PROTO;
        else
            st->hand_state = TLS_ST_CW_FINISHED;
#endif
        return WRITE_TRAN_CONTINUE;

#if !defined(OPENSSL_NO_NEXTPROTONEG)
    case TLS_ST_CW_NEXT_PROTO:
        st->hand_state = TLS_ST_CW_FINISHED;
        return WRITE_TRAN_CONTINUE;
#endif

    case TLS_ST_CW_FINISHED:
        if (s->hit) {
            st->hand_state = TLS_ST_OK;
            ossl_statem_set_in_init(s, 0);
            return WRITE_TRAN_CONTINUE;
        } else {
            return WRITE_TRAN_FINISHED;
        }

    case TLS_ST_CR_FINISHED:
        if (s->hit) {
            st->hand_state = TLS_ST_CW_CHANGE;
            return WRITE_TRAN_CONTINUE;
        } else {
            st->hand_state = TLS_ST_OK;
            ossl_statem_set_in_init(s, 0);
            return WRITE_TRAN_CONTINUE;
        }

    default:
        /* Shouldn't happen */
        return WRITE_TRAN_ERROR;
    }
}

/*
 * Perform any pre work that needs to be done prior to sending a message from
 * the client to the server.
 */
WORK_STATE ossl_statem_client_pre_work(SSL *s, WORK_STATE wst)
{
    OSSL_STATEM *st = &s->statem;

    switch (st->hand_state) {
    case TLS_ST_CW_CLNT_HELLO:
        s->shutdown = 0;
        if (SSL_IS_DTLS(s)) {
            /* every DTLS ClientHello resets Finished MAC */
            if (!ssl3_init_finished_mac(s)) {
                ossl_statem_set_error(s);
                return WORK_ERROR;
            }
        }
        break;

    case TLS_ST_CW_CHANGE:
        if (SSL_IS_DTLS(s)) {
            if (s->hit) {
                /*
                 * We're into the last flight so we don't retransmit these
                 * messages unless we need to.
                 */
                st->use_timer = 0;
            }
#ifndef OPENSSL_NO_SCTP
            if (BIO_dgram_is_sctp(SSL_get_wbio(s)))
                return dtls_wait_for_dry(s);
#endif
        }
        return WORK_FINISHED_CONTINUE;

    case TLS_ST_OK:
        return tls_finish_handshake(s, wst);

    default:
        /* No pre work to be done */
        break;
    }

    return WORK_FINISHED_CONTINUE;
}

/*
 * Perform any work that needs to be done after sending a message from the
 * client to the server.
 */
WORK_STATE ossl_statem_client_post_work(SSL *s, WORK_STATE wst)
{
    OSSL_STATEM *st = &s->statem;

    s->init_num = 0;

    switch (st->hand_state) {
    case TLS_ST_CW_CLNT_HELLO:
        if (wst == WORK_MORE_A && statem_flush(s) != 1)
            return WORK_MORE_A;

        if (SSL_IS_DTLS(s)) {
            /* Treat the next message as the first packet */
            s->first_packet = 1;
        }
        break;

    case TLS_ST_CW_KEY_EXCH:
        if (tls_client_key_exchange_post_work(s) == 0)
            return WORK_ERROR;
        break;

    case TLS_ST_CW_CHANGE:
        s->session->cipher = s->s3->tmp.new_cipher;
#ifdef OPENSSL_NO_COMP
        s->session->compress_meth = 0;
#else
        if (s->s3->tmp.new_compression == NULL)
            s->session->compress_meth = 0;
        else
            s->session->compress_meth = s->s3->tmp.new_compression->id;
#endif
        if (!s->method->ssl3_enc->setup_key_block(s))
            return WORK_ERROR;

        if (!s->method->ssl3_enc->change_cipher_state(s,
                                                      SSL3_CHANGE_CIPHER_CLIENT_WRITE))
            return WORK_ERROR;

        if (SSL_IS_DTLS(s)) {
#ifndef OPENSSL_NO_SCTP
            if (s->hit) {
                /*
                 * Change to new shared key of SCTP-Auth, will be ignored if
                 * no SCTP used.
                 */
                BIO_ctrl(SSL_get_wbio(s), BIO_CTRL_DGRAM_SCTP_NEXT_AUTH_KEY,
                         0, NULL);
            }
#endif

            dtls1_reset_seq_numbers(s, SSL3_CC_WRITE);
        }
        break;

    case TLS_ST_CW_FINISHED:
#ifndef OPENSSL_NO_SCTP
        if (wst == WORK_MORE_A && SSL_IS_DTLS(s) && s->hit == 0) {
            /*
             * Change to new shared key of SCTP-Auth, will be ignored if
             * no SCTP used.
             */
            BIO_ctrl(SSL_get_wbio(s), BIO_CTRL_DGRAM_SCTP_NEXT_AUTH_KEY,
                     0, NULL);
        }
#endif
        if (statem_flush(s) != 1)
            return WORK_MORE_B;
        break;

    default:
        /* No post work to be done */
        break;
    }

    return WORK_FINISHED_CONTINUE;
}

/*
 * Construct a message to be sent from the client to the server.
 *
 * Valid return values are:
 *   1: Success
 *   0: Error
 */
int ossl_statem_client_construct_message(SSL *s)
{
    OSSL_STATEM *st = &s->statem;

    switch (st->hand_state) {
    case TLS_ST_CW_CLNT_HELLO:
        return tls_construct_client_hello(s);

    case TLS_ST_CW_CERT:
        return tls_construct_client_certificate(s);

    case TLS_ST_CW_KEY_EXCH:
        return tls_construct_client_key_exchange(s);

    case TLS_ST_CW_CERT_VRFY:
        return tls_construct_client_verify(s);

    case TLS_ST_CW_CHANGE:
        if (SSL_IS_DTLS(s))
            return dtls_construct_change_cipher_spec(s);
        else
            return tls_construct_change_cipher_spec(s);

#if !defined(OPENSSL_NO_NEXTPROTONEG)
    case TLS_ST_CW_NEXT_PROTO:
        return tls_construct_next_proto(s);
#endif
    case TLS_ST_CW_FINISHED:
        return tls_construct_finished(s,
                                      s->method->
                                      ssl3_enc->client_finished_label,
                                      s->method->
                                      ssl3_enc->client_finished_label_len);

    default:
        /* Shouldn't happen */
        break;
    }

    return 0;
}

/*
 * Returns the maximum allowed length for the current message that we are
 * reading. Excludes the message header.
 */
unsigned long ossl_statem_client_max_message_size(SSL *s)
{
    OSSL_STATEM *st = &s->statem;

    switch (st->hand_state) {
    case TLS_ST_CR_SRVR_HELLO:
        return SERVER_HELLO_MAX_LENGTH;

    case DTLS_ST_CR_HELLO_VERIFY_REQUEST:
        return HELLO_VERIFY_REQUEST_MAX_LENGTH;

    case TLS_ST_CR_CERT:
        return s->max_cert_list;

    case TLS_ST_CR_CERT_STATUS:
        return SSL3_RT_MAX_PLAIN_LENGTH;

    case TLS_ST_CR_KEY_EXCH:
        return SERVER_KEY_EXCH_MAX_LENGTH;

    case TLS_ST_CR_CERT_REQ:
        /*
         * Set to s->max_cert_list for compatibility with previous releases. In
         * practice these messages can get quite long if servers are configured
         * to provide a long list of acceptable CAs
         */
        return s->max_cert_list;

    case TLS_ST_CR_SRVR_DONE:
        return SERVER_HELLO_DONE_MAX_LENGTH;

    case TLS_ST_CR_CHANGE:
        if (s->version == DTLS1_BAD_VER)
            return 3;
        return CCS_MAX_LENGTH;

    case TLS_ST_CR_SESSION_TICKET:
        return SSL3_RT_MAX_PLAIN_LENGTH;

    case TLS_ST_CR_FINISHED:
        return FINISHED_MAX_LENGTH;

    default:
        /* Shouldn't happen */
        break;
    }

    return 0;
}

/*
 * Process a message that the client has been received from the server.
 */
MSG_PROCESS_RETURN ossl_statem_client_process_message(SSL *s, PACKET *pkt)
{
    OSSL_STATEM *st = &s->statem;

    switch (st->hand_state) {
    case TLS_ST_CR_SRVR_HELLO:
        return tls_process_server_hello(s, pkt);

    case DTLS_ST_CR_HELLO_VERIFY_REQUEST:
        return dtls_process_hello_verify(s, pkt);

    case TLS_ST_CR_CERT:
        return tls_process_server_certificate(s, pkt);

    case TLS_ST_CR_CERT_STATUS:
        return tls_process_cert_status(s, pkt);

    case TLS_ST_CR_KEY_EXCH:
        return tls_process_key_exchange(s, pkt);

    case TLS_ST_CR_CERT_REQ:
        return tls_process_certificate_request(s, pkt);

    case TLS_ST_CR_SRVR_DONE:
        return tls_process_server_done(s, pkt);

    case TLS_ST_CR_CHANGE:
        return tls_process_change_cipher_spec(s, pkt);

    case TLS_ST_CR_SESSION_TICKET:
        return tls_process_new_session_ticket(s, pkt);

    case TLS_ST_CR_FINISHED:
        return tls_process_finished(s, pkt);

    default:
        /* Shouldn't happen */
        break;
    }

    return MSG_PROCESS_ERROR;
}

/*
 * Perform any further processing required following the receipt of a message
 * from the server
 */
WORK_STATE ossl_statem_client_post_process_message(SSL *s, WORK_STATE wst)
{
    OSSL_STATEM *st = &s->statem;

    switch (st->hand_state) {
    case TLS_ST_CR_CERT_REQ:
        return tls_prepare_client_certificate(s, wst);

    default:
        break;
    }

    /* Shouldn't happen */
    return WORK_ERROR;
}

int tls_construct_client_hello(SSL *s)
{
    unsigned char *buf;
    unsigned char *p, *d;
    int i;
    int protverr;
    unsigned long l;
    int al = 0;
#ifndef OPENSSL_NO_COMP
    int j;
    SSL_COMP *comp;
#endif
    SSL_SESSION *sess = s->session;

    buf = (unsigned char *)s->init_buf->data;

    /* Work out what SSL/TLS/DTLS version to use */
    protverr = ssl_set_client_hello_version(s);
    if (protverr != 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_HELLO, protverr);
        goto err;
    }

    if ((sess == NULL) || !ssl_version_supported(s, sess->ssl_version) ||
        /*
         * In the case of EAP-FAST, we can have a pre-shared
         * "ticket" without a session ID.
         */
        (!sess->session_id_length && !sess->tlsext_tick) ||
        (sess->not_resumable)) {
        if (!ssl_get_new_session(s, 0))
            goto err;
    }
    /* else use the pre-loaded session */

    p = s->s3->client_random;

    /*
     * for DTLS if client_random is initialized, reuse it, we are
     * required to use same upon reply to HelloVerify
     */
    if (SSL_IS_DTLS(s)) {
        size_t idx;
        i = 1;
        for (idx = 0; idx < sizeof(s->s3->client_random); idx++) {
            if (p[idx]) {
                i = 0;
                break;
            }
        }
    } else
        i = 1;

    if (i && ssl_fill_hello_random(s, 0, p, sizeof(s->s3->client_random)) <= 0)
        goto err;

    /* Do the message type and length last */
    d = p = ssl_handshake_start(s);

    /*-
     * version indicates the negotiated version: for example from
     * an SSLv2/v3 compatible client hello). The client_version
     * field is the maximum version we permit and it is also
     * used in RSA encrypted premaster secrets. Some servers can
     * choke if we initially report a higher version then
     * renegotiate to a lower one in the premaster secret. This
     * didn't happen with TLS 1.0 as most servers supported it
     * but it can with TLS 1.1 or later if the server only supports
     * 1.0.
     *
     * Possible scenario with previous logic:
     *      1. Client hello indicates TLS 1.2
     *      2. Server hello says TLS 1.0
     *      3. RSA encrypted premaster secret uses 1.2.
     *      4. Handshake proceeds using TLS 1.0.
     *      5. Server sends hello request to renegotiate.
     *      6. Client hello indicates TLS v1.0 as we now
     *         know that is maximum server supports.
     *      7. Server chokes on RSA encrypted premaster secret
     *         containing version 1.0.
     *
     * For interoperability it should be OK to always use the
     * maximum version we support in client hello and then rely
     * on the checking of version to ensure the servers isn't
     * being inconsistent: for example initially negotiating with
     * TLS 1.0 and renegotiating with TLS 1.2. We do this by using
     * client_version in client hello and not resetting it to
     * the negotiated version.
     */
    *(p++) = s->client_version >> 8;
    *(p++) = s->client_version & 0xff;

    /* Random stuff */
    memcpy(p, s->s3->client_random, SSL3_RANDOM_SIZE);
    p += SSL3_RANDOM_SIZE;

    /* Session ID */
    if (s->new_session)
        i = 0;
    else
        i = s->session->session_id_length;
    *(p++) = i;
    if (i != 0) {
        if (i > (int)sizeof(s->session->session_id)) {
            SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_HELLO, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        memcpy(p, s->session->session_id, i);
        p += i;
    }

    /* cookie stuff for DTLS */
    if (SSL_IS_DTLS(s)) {
        if (s->d1->cookie_len > sizeof(s->d1->cookie)) {
            SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_HELLO, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        *(p++) = s->d1->cookie_len;
        memcpy(p, s->d1->cookie, s->d1->cookie_len);
        p += s->d1->cookie_len;
    }

    /* Ciphers supported */
    i = ssl_cipher_list_to_bytes(s, SSL_get_ciphers(s), &(p[2]));
    if (i == 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_HELLO, SSL_R_NO_CIPHERS_AVAILABLE);
        goto err;
    }
#ifdef OPENSSL_MAX_TLS1_2_CIPHER_LENGTH
    /*
     * Some servers hang if client hello > 256 bytes as hack workaround
     * chop number of supported ciphers to keep it well below this if we
     * use TLS v1.2
     */
    if (TLS1_get_version(s) >= TLS1_2_VERSION
        && i > OPENSSL_MAX_TLS1_2_CIPHER_LENGTH)
        i = OPENSSL_MAX_TLS1_2_CIPHER_LENGTH & ~1;
#endif
    s2n(i, p);
    p += i;

    /* COMPRESSION */
#ifdef OPENSSL_NO_COMP
    *(p++) = 1;
#else

    if (!ssl_allow_compression(s) || !s->ctx->comp_methods)
        j = 0;
    else
        j = sk_SSL_COMP_num(s->ctx->comp_methods);
    *(p++) = 1 + j;
    for (i = 0; i < j; i++) {
        comp = sk_SSL_COMP_value(s->ctx->comp_methods, i);
        *(p++) = comp->id;
    }
#endif
    *(p++) = 0;                 /* Add the NULL method */

    /* TLS extensions */
    if (ssl_prepare_clienthello_tlsext(s) <= 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_HELLO, SSL_R_CLIENTHELLO_TLSEXT);
        goto err;
    }
    if ((p =
         ssl_add_clienthello_tlsext(s, p, buf + SSL3_RT_MAX_PLAIN_LENGTH,
                                    &al)) == NULL) {
        ssl3_send_alert(s, SSL3_AL_FATAL, al);
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_HELLO, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    l = p - d;
    if (!ssl_set_handshake_header(s, SSL3_MT_CLIENT_HELLO, l)) {
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_HELLO, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    return 1;
 err:
    ossl_statem_set_error(s);
    return 0;
}

MSG_PROCESS_RETURN dtls_process_hello_verify(SSL *s, PACKET *pkt)
{
    int al;
    unsigned int cookie_len;
    PACKET cookiepkt;

    if (!PACKET_forward(pkt, 2)
        || !PACKET_get_length_prefixed_1(pkt, &cookiepkt)) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_DTLS_PROCESS_HELLO_VERIFY, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }

    cookie_len = PACKET_remaining(&cookiepkt);
    if (cookie_len > sizeof(s->d1->cookie)) {
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_DTLS_PROCESS_HELLO_VERIFY, SSL_R_LENGTH_TOO_LONG);
        goto f_err;
    }

    if (!PACKET_copy_bytes(&cookiepkt, s->d1->cookie, cookie_len)) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_DTLS_PROCESS_HELLO_VERIFY, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }
    s->d1->cookie_len = cookie_len;

    return MSG_PROCESS_FINISHED_READING;
 f_err:
    ssl3_send_alert(s, SSL3_AL_FATAL, al);
    ossl_statem_set_error(s);
    return MSG_PROCESS_ERROR;
}

MSG_PROCESS_RETURN tls_process_server_hello(SSL *s, PACKET *pkt)
{
    STACK_OF(SSL_CIPHER) *sk;
    const SSL_CIPHER *c;
    PACKET session_id;
    size_t session_id_len;
    const unsigned char *cipherchars;
    int i, al = SSL_AD_INTERNAL_ERROR;
    unsigned int compression;
    unsigned int sversion;
    int protverr;
#ifndef OPENSSL_NO_COMP
    SSL_COMP *comp;
#endif

    if (!PACKET_get_net_2(pkt, &sversion)) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }

    protverr = ssl_choose_client_version(s, sversion);
    if (protverr != 0) {
        al = SSL_AD_PROTOCOL_VERSION;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, protverr);
        goto f_err;
    }

    /* load the server hello data */
    /* load the server random */
    if (!PACKET_copy_bytes(pkt, s->s3->server_random, SSL3_RANDOM_SIZE)) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }

    s->hit = 0;

    /* Get the session-id. */
    if (!PACKET_get_length_prefixed_1(pkt, &session_id)) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }
    session_id_len = PACKET_remaining(&session_id);
    if (session_id_len > sizeof(s->session->session_id)
        || session_id_len > SSL3_SESSION_ID_SIZE) {
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_SSL3_SESSION_ID_TOO_LONG);
        goto f_err;
    }

    if (!PACKET_get_bytes(pkt, &cipherchars, TLS_CIPHER_LEN)) {
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_LENGTH_MISMATCH);
        al = SSL_AD_DECODE_ERROR;
        goto f_err;
    }

    /*
     * Check if we can resume the session based on external pre-shared secret.
     * EAP-FAST (RFC 4851) supports two types of session resumption.
     * Resumption based on server-side state works with session IDs.
     * Resumption based on pre-shared Protected Access Credentials (PACs)
     * works by overriding the SessionTicket extension at the application
     * layer, and does not send a session ID. (We do not know whether EAP-FAST
     * servers would honour the session ID.) Therefore, the session ID alone
     * is not a reliable indicator of session resumption, so we first check if
     * we can resume, and later peek at the next handshake message to see if the
     * server wants to resume.
     */
    if (s->version >= TLS1_VERSION && s->tls_session_secret_cb &&
        s->session->tlsext_tick) {
        const SSL_CIPHER *pref_cipher = NULL;
        s->session->master_key_length = sizeof(s->session->master_key);
        if (s->tls_session_secret_cb(s, s->session->master_key,
                                     &s->session->master_key_length,
                                     NULL, &pref_cipher,
                                     s->tls_session_secret_cb_arg)) {
            s->session->cipher = pref_cipher ?
                pref_cipher : ssl_get_cipher_by_char(s, cipherchars);
        } else {
            SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, ERR_R_INTERNAL_ERROR);
            al = SSL_AD_INTERNAL_ERROR;
            goto f_err;
        }
    }

    if (session_id_len != 0 && session_id_len == s->session->session_id_length
        && memcmp(PACKET_data(&session_id), s->session->session_id,
                  session_id_len) == 0) {
        if (s->sid_ctx_length != s->session->sid_ctx_length
            || memcmp(s->session->sid_ctx, s->sid_ctx, s->sid_ctx_length)) {
            /* actually a client application bug */
            al = SSL_AD_ILLEGAL_PARAMETER;
            SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO,
                   SSL_R_ATTEMPT_TO_REUSE_SESSION_IN_DIFFERENT_CONTEXT);
            goto f_err;
        }
        s->hit = 1;
    } else {
        /*
         * If we were trying for session-id reuse but the server
         * didn't echo the ID, make a new SSL_SESSION.
         * In the case of EAP-FAST and PAC, we do not send a session ID,
         * so the PAC-based session secret is always preserved. It'll be
         * overwritten if the server refuses resumption.
         */
        if (s->session->session_id_length > 0) {
            s->ctx->stats.sess_miss++;
            if (!ssl_get_new_session(s, 0)) {
                goto f_err;
            }
        }

        s->session->ssl_version = s->version;
        s->session->session_id_length = session_id_len;
        /* session_id_len could be 0 */
        if (session_id_len > 0)
            memcpy(s->session->session_id, PACKET_data(&session_id),
                   session_id_len);
    }

    /* Session version and negotiated protocol version should match */
    if (s->version != s->session->ssl_version) {
        al = SSL_AD_PROTOCOL_VERSION;

        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO,
               SSL_R_SSL_SESSION_VERSION_MISMATCH);
        goto f_err;
    }

    c = ssl_get_cipher_by_char(s, cipherchars);
    if (c == NULL) {
        /* unknown cipher */
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_UNKNOWN_CIPHER_RETURNED);
        goto f_err;
    }
    /*
     * Now that we know the version, update the check to see if it's an allowed
     * version.
     */
    s->s3->tmp.min_ver = s->version;
    s->s3->tmp.max_ver = s->version;
    /*
     * If it is a disabled cipher we either didn't send it in client hello,
     * or it's not allowed for the selected protocol. So we return an error.
     */
    if (ssl_cipher_disabled(s, c, SSL_SECOP_CIPHER_CHECK, 1)) {
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_WRONG_CIPHER_RETURNED);
        goto f_err;
    }

    sk = ssl_get_ciphers_by_id(s);
    i = sk_SSL_CIPHER_find(sk, c);
    if (i < 0) {
        /* we did not say we would use this cipher */
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_WRONG_CIPHER_RETURNED);
        goto f_err;
    }

    /*
     * Depending on the session caching (internal/external), the cipher
     * and/or cipher_id values may not be set. Make sure that cipher_id is
     * set and use it for comparison.
     */
    if (s->session->cipher)
        s->session->cipher_id = s->session->cipher->id;
    if (s->hit && (s->session->cipher_id != c->id)) {
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO,
               SSL_R_OLD_SESSION_CIPHER_NOT_RETURNED);
        goto f_err;
    }
    s->s3->tmp.new_cipher = c;
    /* lets get the compression algorithm */
    /* COMPRESSION */
    if (!PACKET_get_1(pkt, &compression)) {
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_LENGTH_MISMATCH);
        al = SSL_AD_DECODE_ERROR;
        goto f_err;
    }
#ifdef OPENSSL_NO_COMP
    if (compression != 0) {
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO,
               SSL_R_UNSUPPORTED_COMPRESSION_ALGORITHM);
        goto f_err;
    }
    /*
     * If compression is disabled we'd better not try to resume a session
     * using compression.
     */
    if (s->session->compress_meth != 0) {
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_INCONSISTENT_COMPRESSION);
        goto f_err;
    }
#else
    if (s->hit && compression != s->session->compress_meth) {
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO,
               SSL_R_OLD_SESSION_COMPRESSION_ALGORITHM_NOT_RETURNED);
        goto f_err;
    }
    if (compression == 0)
        comp = NULL;
    else if (!ssl_allow_compression(s)) {
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_COMPRESSION_DISABLED);
        goto f_err;
    } else {
        comp = ssl3_comp_find(s->ctx->comp_methods, compression);
    }

    if (compression != 0 && comp == NULL) {
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO,
               SSL_R_UNSUPPORTED_COMPRESSION_ALGORITHM);
        goto f_err;
    } else {
        s->s3->tmp.new_compression = comp;
    }
#endif

    /* TLS extensions */
    if (!ssl_parse_serverhello_tlsext(s, pkt)) {
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_PARSE_TLSEXT);
        goto err;
    }

    if (PACKET_remaining(pkt) != 0) {
        /* wrong packet length */
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_HELLO, SSL_R_BAD_PACKET_LENGTH);
        goto f_err;
    }
#ifndef OPENSSL_NO_SCTP
    if (SSL_IS_DTLS(s) && s->hit) {
        unsigned char sctpauthkey[64];
        char labelbuffer[sizeof(DTLS1_SCTP_AUTH_LABEL)];

        /*
         * Add new shared key for SCTP-Auth, will be ignored if
         * no SCTP used.
         */
        memcpy(labelbuffer, DTLS1_SCTP_AUTH_LABEL,
               sizeof(DTLS1_SCTP_AUTH_LABEL));

        if (SSL_export_keying_material(s, sctpauthkey,
                                       sizeof(sctpauthkey),
                                       labelbuffer,
                                       sizeof(labelbuffer), NULL, 0, 0) <= 0)
            goto err;

        BIO_ctrl(SSL_get_wbio(s),
                 BIO_CTRL_DGRAM_SCTP_ADD_AUTH_KEY,
                 sizeof(sctpauthkey), sctpauthkey);
    }
#endif

    return MSG_PROCESS_CONTINUE_READING;
 f_err:
    ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
    ossl_statem_set_error(s);
    return MSG_PROCESS_ERROR;
}

MSG_PROCESS_RETURN tls_process_server_certificate(SSL *s, PACKET *pkt)
{
    int al, i, ret = MSG_PROCESS_ERROR, exp_idx;
    unsigned long cert_list_len, cert_len;
    X509 *x = NULL;
    const unsigned char *certstart, *certbytes;
    STACK_OF(X509) *sk = NULL;
    EVP_PKEY *pkey = NULL;

    if ((sk = sk_X509_new_null()) == NULL) {
        SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (!PACKET_get_net_3(pkt, &cert_list_len)
        || PACKET_remaining(pkt) != cert_list_len) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }
    while (PACKET_remaining(pkt)) {
        if (!PACKET_get_net_3(pkt, &cert_len)
            || !PACKET_get_bytes(pkt, &certbytes, cert_len)) {
            al = SSL_AD_DECODE_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE,
                   SSL_R_CERT_LENGTH_MISMATCH);
            goto f_err;
        }

        certstart = certbytes;
        x = d2i_X509(NULL, (const unsigned char **)&certbytes, cert_len);
        if (x == NULL) {
            al = SSL_AD_BAD_CERTIFICATE;
            SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE, ERR_R_ASN1_LIB);
            goto f_err;
        }
        if (certbytes != (certstart + cert_len)) {
            al = SSL_AD_DECODE_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE,
                   SSL_R_CERT_LENGTH_MISMATCH);
            goto f_err;
        }
        if (!sk_X509_push(sk, x)) {
            SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        x = NULL;
    }

    i = ssl_verify_cert_chain(s, sk);
    /*
     * The documented interface is that SSL_VERIFY_PEER should be set in order
     * for client side verification of the server certificate to take place.
     * However, historically the code has only checked that *any* flag is set
     * to cause server verification to take place. Use of the other flags makes
     * no sense in client mode. An attempt to clean up the semantics was
     * reverted because at least one application *only* set
     * SSL_VERIFY_FAIL_IF_NO_PEER_CERT. Prior to the clean up this still caused
     * server verification to take place, after the clean up it silently did
     * nothing. SSL_CTX_set_verify()/SSL_set_verify() cannot validate the flags
     * sent to them because they are void functions. Therefore, we now use the
     * (less clean) historic behaviour of performing validation if any flag is
     * set. The *documented* interface remains the same.
     */
    if (s->verify_mode != SSL_VERIFY_NONE && i <= 0) {
        al = ssl_verify_alarm_type(s->verify_result);
        SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE,
               SSL_R_CERTIFICATE_VERIFY_FAILED);
        goto f_err;
    }
    ERR_clear_error();          /* but we keep s->verify_result */
    if (i > 1) {
        SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE, i);
        al = SSL_AD_HANDSHAKE_FAILURE;
        goto f_err;
    }

    s->session->peer_chain = sk;
    /*
     * Inconsistency alert: cert_chain does include the peer's certificate,
     * which we don't include in statem_srvr.c
     */
    x = sk_X509_value(sk, 0);
    sk = NULL;

    pkey = X509_get0_pubkey(x);

    if (pkey == NULL || EVP_PKEY_missing_parameters(pkey)) {
        x = NULL;
        al = SSL3_AL_FATAL;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE,
               SSL_R_UNABLE_TO_FIND_PUBLIC_KEY_PARAMETERS);
        goto f_err;
    }

    i = ssl_cert_type(x, pkey);
    if (i < 0) {
        x = NULL;
        al = SSL3_AL_FATAL;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE,
               SSL_R_UNKNOWN_CERTIFICATE_TYPE);
        goto f_err;
    }

    exp_idx = ssl_cipher_get_cert_index(s->s3->tmp.new_cipher);
    if (exp_idx >= 0 && i != exp_idx
        && (exp_idx != SSL_PKEY_GOST_EC ||
            (i != SSL_PKEY_GOST12_512 && i != SSL_PKEY_GOST12_256
             && i != SSL_PKEY_GOST01))) {
        x = NULL;
        al = SSL_AD_ILLEGAL_PARAMETER;
        SSLerr(SSL_F_TLS_PROCESS_SERVER_CERTIFICATE,
               SSL_R_WRONG_CERTIFICATE_TYPE);
        goto f_err;
    }
    s->session->peer_type = i;

    X509_free(s->session->peer);
    X509_up_ref(x);
    s->session->peer = x;
    s->session->verify_result = s->verify_result;

    x = NULL;
    ret = MSG_PROCESS_CONTINUE_READING;
    goto done;

 f_err:
    ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
    ossl_statem_set_error(s);
 done:
    X509_free(x);
    sk_X509_pop_free(sk, X509_free);
    return ret;
}

static int tls_process_ske_psk_preamble(SSL *s, PACKET *pkt, int *al)
{
#ifndef OPENSSL_NO_PSK
    PACKET psk_identity_hint;

    /* PSK ciphersuites are preceded by an identity hint */

    if (!PACKET_get_length_prefixed_2(pkt, &psk_identity_hint)) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_PSK_PREAMBLE, SSL_R_LENGTH_MISMATCH);
        return 0;
    }

    /*
     * Store PSK identity hint for later use, hint is used in
     * tls_construct_client_key_exchange.  Assume that the maximum length of
     * a PSK identity hint can be as long as the maximum length of a PSK
     * identity.
     */
    if (PACKET_remaining(&psk_identity_hint) > PSK_MAX_IDENTITY_LEN) {
        *al = SSL_AD_HANDSHAKE_FAILURE;
        SSLerr(SSL_F_TLS_PROCESS_SKE_PSK_PREAMBLE, SSL_R_DATA_LENGTH_TOO_LONG);
        return 0;
    }

    if (PACKET_remaining(&psk_identity_hint) == 0) {
        OPENSSL_free(s->session->psk_identity_hint);
        s->session->psk_identity_hint = NULL;
    } else if (!PACKET_strndup(&psk_identity_hint,
                               &s->session->psk_identity_hint)) {
        *al = SSL_AD_INTERNAL_ERROR;
        return 0;
    }

    return 1;
#else
    SSLerr(SSL_F_TLS_PROCESS_SKE_PSK_PREAMBLE, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

static int tls_process_ske_srp(SSL *s, PACKET *pkt, EVP_PKEY **pkey, int *al)
{
#ifndef OPENSSL_NO_SRP
    PACKET prime, generator, salt, server_pub;

    if (!PACKET_get_length_prefixed_2(pkt, &prime)
        || !PACKET_get_length_prefixed_2(pkt, &generator)
        || !PACKET_get_length_prefixed_1(pkt, &salt)
        || !PACKET_get_length_prefixed_2(pkt, &server_pub)) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_SRP, SSL_R_LENGTH_MISMATCH);
        return 0;
    }

    if ((s->srp_ctx.N =
         BN_bin2bn(PACKET_data(&prime),
                   PACKET_remaining(&prime), NULL)) == NULL
        || (s->srp_ctx.g =
            BN_bin2bn(PACKET_data(&generator),
                      PACKET_remaining(&generator), NULL)) == NULL
        || (s->srp_ctx.s =
            BN_bin2bn(PACKET_data(&salt),
                      PACKET_remaining(&salt), NULL)) == NULL
        || (s->srp_ctx.B =
            BN_bin2bn(PACKET_data(&server_pub),
                      PACKET_remaining(&server_pub), NULL)) == NULL) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_SRP, ERR_R_BN_LIB);
        return 0;
    }

    if (!srp_verify_server_param(s, al)) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_SRP, SSL_R_BAD_SRP_PARAMETERS);
        return 0;
    }

    /* We must check if there is a certificate */
    if (s->s3->tmp.new_cipher->algorithm_auth & (SSL_aRSA | SSL_aDSS))
        *pkey = X509_get0_pubkey(s->session->peer);

    return 1;
#else
    SSLerr(SSL_F_TLS_PROCESS_SKE_SRP, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

static int tls_process_ske_dhe(SSL *s, PACKET *pkt, EVP_PKEY **pkey, int *al)
{
#ifndef OPENSSL_NO_DH
    PACKET prime, generator, pub_key;
    EVP_PKEY *peer_tmp = NULL;

    DH *dh = NULL;
    BIGNUM *p = NULL, *g = NULL, *bnpub_key = NULL;

    int check_bits = 0;

    if (!PACKET_get_length_prefixed_2(pkt, &prime)
        || !PACKET_get_length_prefixed_2(pkt, &generator)
        || !PACKET_get_length_prefixed_2(pkt, &pub_key)) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, SSL_R_LENGTH_MISMATCH);
        return 0;
    }

    peer_tmp = EVP_PKEY_new();
    dh = DH_new();

    if (peer_tmp == NULL || dh == NULL) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    p = BN_bin2bn(PACKET_data(&prime), PACKET_remaining(&prime), NULL);
    g = BN_bin2bn(PACKET_data(&generator), PACKET_remaining(&generator), NULL);
    bnpub_key = BN_bin2bn(PACKET_data(&pub_key), PACKET_remaining(&pub_key),
                          NULL);
    if (p == NULL || g == NULL || bnpub_key == NULL) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, ERR_R_BN_LIB);
        goto err;
    }

    /* test non-zero pubkey */
    if (BN_is_zero(bnpub_key)) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, SSL_R_BAD_DH_VALUE);
        goto err;
    }

    if (!DH_set0_pqg(dh, p, NULL, g)) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, ERR_R_BN_LIB);
        goto err;
    }
    p = g = NULL;

    if (DH_check_params(dh, &check_bits) == 0 || check_bits != 0) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, SSL_R_BAD_DH_VALUE);
        goto err;
    }

    if (!DH_set0_key(dh, bnpub_key, NULL)) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, ERR_R_BN_LIB);
        goto err;
    }
    bnpub_key = NULL;

    if (!ssl_security(s, SSL_SECOP_TMP_DH, DH_security_bits(dh), 0, dh)) {
        *al = SSL_AD_HANDSHAKE_FAILURE;
        SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, SSL_R_DH_KEY_TOO_SMALL);
        goto err;
    }

    if (EVP_PKEY_assign_DH(peer_tmp, dh) == 0) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, ERR_R_EVP_LIB);
        goto err;
    }

    s->s3->peer_tmp = peer_tmp;

    /*
     * FIXME: This makes assumptions about which ciphersuites come with
     * public keys. We should have a less ad-hoc way of doing this
     */
    if (s->s3->tmp.new_cipher->algorithm_auth & (SSL_aRSA | SSL_aDSS))
        *pkey = X509_get0_pubkey(s->session->peer);
    /* else anonymous DH, so no certificate or pkey. */

    return 1;

 err:
    BN_free(p);
    BN_free(g);
    BN_free(bnpub_key);
    DH_free(dh);
    EVP_PKEY_free(peer_tmp);

    return 0;
#else
    SSLerr(SSL_F_TLS_PROCESS_SKE_DHE, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

static int tls_process_ske_ecdhe(SSL *s, PACKET *pkt, EVP_PKEY **pkey, int *al)
{
#ifndef OPENSSL_NO_EC
    PACKET encoded_pt;
    const unsigned char *ecparams;
    int curve_nid;
    unsigned int curve_flags;
    EVP_PKEY_CTX *pctx = NULL;

    /*
     * Extract elliptic curve parameters and the server's ephemeral ECDH
     * public key. For now we only support named (not generic) curves and
     * ECParameters in this case is just three bytes.
     */
    if (!PACKET_get_bytes(pkt, &ecparams, 3)) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_ECDHE, SSL_R_LENGTH_TOO_SHORT);
        return 0;
    }
    /*
     * Check curve is one of our preferences, if not server has sent an
     * invalid curve. ECParameters is 3 bytes.
     */
    if (!tls1_check_curve(s, ecparams, 3)) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_ECDHE, SSL_R_WRONG_CURVE);
        return 0;
    }

    curve_nid = tls1_ec_curve_id2nid(*(ecparams + 2), &curve_flags);

    if (curve_nid == 0) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_ECDHE,
               SSL_R_UNABLE_TO_FIND_ECDH_PARAMETERS);
        return 0;
    }

    if ((curve_flags & TLS_CURVE_TYPE) == TLS_CURVE_CUSTOM) {
        EVP_PKEY *key = EVP_PKEY_new();

        if (key == NULL || !EVP_PKEY_set_type(key, curve_nid)) {
            *al = SSL_AD_INTERNAL_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_SKE_ECDHE, ERR_R_EVP_LIB);
            EVP_PKEY_free(key);
            return 0;
        }
        s->s3->peer_tmp = key;
    } else {
        /* Set up EVP_PKEY with named curve as parameters */
        pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
        if (pctx == NULL
            || EVP_PKEY_paramgen_init(pctx) <= 0
            || EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, curve_nid) <= 0
            || EVP_PKEY_paramgen(pctx, &s->s3->peer_tmp) <= 0) {
            *al = SSL_AD_INTERNAL_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_SKE_ECDHE, ERR_R_EVP_LIB);
            EVP_PKEY_CTX_free(pctx);
            return 0;
        }
        EVP_PKEY_CTX_free(pctx);
        pctx = NULL;
    }

    if (!PACKET_get_length_prefixed_1(pkt, &encoded_pt)) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_ECDHE, SSL_R_LENGTH_MISMATCH);
        return 0;
    }

    if (!EVP_PKEY_set1_tls_encodedpoint(s->s3->peer_tmp,
                                        PACKET_data(&encoded_pt),
                                        PACKET_remaining(&encoded_pt))) {
        *al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_SKE_ECDHE, SSL_R_BAD_ECPOINT);
        return 0;
    }

    /*
     * The ECC/TLS specification does not mention the use of DSA to sign
     * ECParameters in the server key exchange message. We do support RSA
     * and ECDSA.
     */
    if (s->s3->tmp.new_cipher->algorithm_auth & SSL_aECDSA)
        *pkey = X509_get0_pubkey(s->session->peer);
    else if (s->s3->tmp.new_cipher->algorithm_auth & SSL_aRSA)
        *pkey = X509_get0_pubkey(s->session->peer);
    /* else anonymous ECDH, so no certificate or pkey. */

    return 1;
#else
    SSLerr(SSL_F_TLS_PROCESS_SKE_ECDHE, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

MSG_PROCESS_RETURN tls_process_key_exchange(SSL *s, PACKET *pkt)
{
    int al = -1;
    long alg_k;
    EVP_PKEY *pkey = NULL;
    PACKET save_param_start, signature;

    alg_k = s->s3->tmp.new_cipher->algorithm_mkey;

    save_param_start = *pkt;

#if !defined(OPENSSL_NO_EC) || !defined(OPENSSL_NO_DH)
    EVP_PKEY_free(s->s3->peer_tmp);
    s->s3->peer_tmp = NULL;
#endif

    if (alg_k & SSL_PSK) {
        if (!tls_process_ske_psk_preamble(s, pkt, &al))
            goto err;
    }

    /* Nothing else to do for plain PSK or RSAPSK */
    if (alg_k & (SSL_kPSK | SSL_kRSAPSK)) {
    } else if (alg_k & SSL_kSRP) {
        if (!tls_process_ske_srp(s, pkt, &pkey, &al))
            goto err;
    } else if (alg_k & (SSL_kDHE | SSL_kDHEPSK)) {
        if (!tls_process_ske_dhe(s, pkt, &pkey, &al))
            goto err;
    } else if (alg_k & (SSL_kECDHE | SSL_kECDHEPSK)) {
        if (!tls_process_ske_ecdhe(s, pkt, &pkey, &al))
            goto err;
    } else if (alg_k) {
        al = SSL_AD_UNEXPECTED_MESSAGE;
        SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, SSL_R_UNEXPECTED_MESSAGE);
        goto err;
    }

    /* if it was signed, check the signature */
    if (pkey != NULL) {
        PACKET params;
        int maxsig;
        const EVP_MD *md = NULL;
        EVP_MD_CTX *md_ctx;

        /*
         * |pkt| now points to the beginning of the signature, so the difference
         * equals the length of the parameters.
         */
        if (!PACKET_get_sub_packet(&save_param_start, &params,
                                   PACKET_remaining(&save_param_start) -
                                   PACKET_remaining(pkt))) {
            al = SSL_AD_INTERNAL_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, ERR_R_INTERNAL_ERROR);
            goto err;
        }

        if (SSL_USE_SIGALGS(s)) {
            const unsigned char *sigalgs;
            int rv;
            if (!PACKET_get_bytes(pkt, &sigalgs, 2)) {
                al = SSL_AD_DECODE_ERROR;
                SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, SSL_R_LENGTH_TOO_SHORT);
                goto err;
            }
            rv = tls12_check_peer_sigalg(&md, s, sigalgs, pkey);
            if (rv == -1) {
                al = SSL_AD_INTERNAL_ERROR;
                goto err;
            } else if (rv == 0) {
                al = SSL_AD_DECODE_ERROR;
                goto err;
            }
#ifdef SSL_DEBUG
            fprintf(stderr, "USING TLSv1.2 HASH %s\n", EVP_MD_name(md));
#endif
        } else if (EVP_PKEY_id(pkey) == EVP_PKEY_RSA) {
            md = EVP_md5_sha1();
        } else {
            md = EVP_sha1();
        }

        if (!PACKET_get_length_prefixed_2(pkt, &signature)
            || PACKET_remaining(pkt) != 0) {
            al = SSL_AD_DECODE_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, SSL_R_LENGTH_MISMATCH);
            goto err;
        }
        maxsig = EVP_PKEY_size(pkey);
        if (maxsig < 0) {
            al = SSL_AD_INTERNAL_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, ERR_R_INTERNAL_ERROR);
            goto err;
        }

        /*
         * Check signature length
         */
        if (PACKET_remaining(&signature) > (size_t)maxsig) {
            /* wrong packet length */
            al = SSL_AD_DECODE_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE,
                   SSL_R_WRONG_SIGNATURE_LENGTH);
            goto err;
        }

        md_ctx = EVP_MD_CTX_new();
        if (md_ctx == NULL) {
            al = SSL_AD_INTERNAL_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, ERR_R_MALLOC_FAILURE);
            goto err;
        }

        if (EVP_VerifyInit_ex(md_ctx, md, NULL) <= 0
            || EVP_VerifyUpdate(md_ctx, &(s->s3->client_random[0]),
                                SSL3_RANDOM_SIZE) <= 0
            || EVP_VerifyUpdate(md_ctx, &(s->s3->server_random[0]),
                                SSL3_RANDOM_SIZE) <= 0
            || EVP_VerifyUpdate(md_ctx, PACKET_data(&params),
                                PACKET_remaining(&params)) <= 0) {
            EVP_MD_CTX_free(md_ctx);
            al = SSL_AD_INTERNAL_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, ERR_R_EVP_LIB);
            goto err;
        }
        if (EVP_VerifyFinal(md_ctx, PACKET_data(&signature),
                            PACKET_remaining(&signature), pkey) <= 0) {
            /* bad signature */
            EVP_MD_CTX_free(md_ctx);
            al = SSL_AD_DECRYPT_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, SSL_R_BAD_SIGNATURE);
            goto err;
        }
        EVP_MD_CTX_free(md_ctx);
    } else {
        /* aNULL, aSRP or PSK do not need public keys */
        if (!(s->s3->tmp.new_cipher->algorithm_auth & (SSL_aNULL | SSL_aSRP))
            && !(alg_k & SSL_PSK)) {
            /* Might be wrong key type, check it */
            if (ssl3_check_cert_and_algorithm(s)) {
                /* Otherwise this shouldn't happen */
                al = SSL_AD_INTERNAL_ERROR;
                SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, ERR_R_INTERNAL_ERROR);
            } else {
                al = SSL_AD_DECODE_ERROR;
            }
            goto err;
        }
        /* still data left over */
        if (PACKET_remaining(pkt) != 0) {
            al = SSL_AD_DECODE_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_KEY_EXCHANGE, SSL_R_EXTRA_DATA_IN_MESSAGE);
            goto err;
        }
    }

    return MSG_PROCESS_CONTINUE_READING;
 err:
    if (al != -1)
        ssl3_send_alert(s, SSL3_AL_FATAL, al);
    ossl_statem_set_error(s);
    return MSG_PROCESS_ERROR;
}

MSG_PROCESS_RETURN tls_process_certificate_request(SSL *s, PACKET *pkt)
{
    int ret = MSG_PROCESS_ERROR;
    unsigned int list_len, ctype_num, i, name_len;
    X509_NAME *xn = NULL;
    const unsigned char *data;
    const unsigned char *namestart, *namebytes;
    STACK_OF(X509_NAME) *ca_sk = NULL;

    if ((ca_sk = sk_X509_NAME_new(ca_dn_cmp)) == NULL) {
        SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    /* get the certificate types */
    if (!PACKET_get_1(pkt, &ctype_num)
        || !PACKET_get_bytes(pkt, &data, ctype_num)) {
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST, SSL_R_LENGTH_MISMATCH);
        goto err;
    }
    OPENSSL_free(s->cert->ctypes);
    s->cert->ctypes = NULL;
    if (ctype_num > SSL3_CT_NUMBER) {
        /* If we exceed static buffer copy all to cert structure */
        s->cert->ctypes = OPENSSL_malloc(ctype_num);
        if (s->cert->ctypes == NULL) {
            SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        memcpy(s->cert->ctypes, data, ctype_num);
        s->cert->ctype_num = (size_t)ctype_num;
        ctype_num = SSL3_CT_NUMBER;
    }
    for (i = 0; i < ctype_num; i++)
        s->s3->tmp.ctype[i] = data[i];

    if (SSL_USE_SIGALGS(s)) {
        if (!PACKET_get_net_2(pkt, &list_len)
            || !PACKET_get_bytes(pkt, &data, list_len)) {
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
            SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST,
                   SSL_R_LENGTH_MISMATCH);
            goto err;
        }

        /* Clear certificate digests and validity flags */
        for (i = 0; i < SSL_PKEY_NUM; i++) {
            s->s3->tmp.md[i] = NULL;
            s->s3->tmp.valid_flags[i] = 0;
        }
        if ((list_len & 1) || !tls1_save_sigalgs(s, data, list_len)) {
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
            SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST,
                   SSL_R_SIGNATURE_ALGORITHMS_ERROR);
            goto err;
        }
        if (!tls1_process_sigalgs(s)) {
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
            SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST, ERR_R_MALLOC_FAILURE);
            goto err;
        }
    } else {
        ssl_set_default_md(s);
    }

    /* get the CA RDNs */
    if (!PACKET_get_net_2(pkt, &list_len)
        || PACKET_remaining(pkt) != list_len) {
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST, SSL_R_LENGTH_MISMATCH);
        goto err;
    }

    while (PACKET_remaining(pkt)) {
        if (!PACKET_get_net_2(pkt, &name_len)
            || !PACKET_get_bytes(pkt, &namebytes, name_len)) {
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
            SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST,
                   SSL_R_LENGTH_MISMATCH);
            goto err;
        }

        namestart = namebytes;

        if ((xn = d2i_X509_NAME(NULL, (const unsigned char **)&namebytes,
                                name_len)) == NULL) {
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
            SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST, ERR_R_ASN1_LIB);
            goto err;
        }

        if (namebytes != (namestart + name_len)) {
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
            SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST,
                   SSL_R_CA_DN_LENGTH_MISMATCH);
            goto err;
        }
        if (!sk_X509_NAME_push(ca_sk, xn)) {
            SSLerr(SSL_F_TLS_PROCESS_CERTIFICATE_REQUEST, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        xn = NULL;
    }

    /* we should setup a certificate to return.... */
    s->s3->tmp.cert_req = 1;
    s->s3->tmp.ctype_num = ctype_num;
    sk_X509_NAME_pop_free(s->s3->tmp.ca_names, X509_NAME_free);
    s->s3->tmp.ca_names = ca_sk;
    ca_sk = NULL;

    ret = MSG_PROCESS_CONTINUE_PROCESSING;
    goto done;
 err:
    ossl_statem_set_error(s);
 done:
    X509_NAME_free(xn);
    sk_X509_NAME_pop_free(ca_sk, X509_NAME_free);
    return ret;
}

static int ca_dn_cmp(const X509_NAME *const *a, const X509_NAME *const *b)
{
    return (X509_NAME_cmp(*a, *b));
}

MSG_PROCESS_RETURN tls_process_new_session_ticket(SSL *s, PACKET *pkt)
{
    int al;
    unsigned int ticklen;
    unsigned long ticket_lifetime_hint;

    if (!PACKET_get_net_4(pkt, &ticket_lifetime_hint)
        || !PACKET_get_net_2(pkt, &ticklen)
        || PACKET_remaining(pkt) != ticklen) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_NEW_SESSION_TICKET, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }

    /* Server is allowed to change its mind and send an empty ticket. */
    if (ticklen == 0)
        return MSG_PROCESS_CONTINUE_READING;

    if (s->session->session_id_length > 0) {
        int i = s->session_ctx->session_cache_mode;
        SSL_SESSION *new_sess;
        /*
         * We reused an existing session, so we need to replace it with a new
         * one
         */
        if (i & SSL_SESS_CACHE_CLIENT) {
            /*
             * Remove the old session from the cache. We carry on if this fails
             */
            SSL_CTX_remove_session(s->session_ctx, s->session);
        }

        if ((new_sess = ssl_session_dup(s->session, 0)) == 0) {
            al = SSL_AD_INTERNAL_ERROR;
            SSLerr(SSL_F_TLS_PROCESS_NEW_SESSION_TICKET, ERR_R_MALLOC_FAILURE);
            goto f_err;
        }

        SSL_SESSION_free(s->session);
        s->session = new_sess;
    }

    OPENSSL_free(s->session->tlsext_tick);
    s->session->tlsext_ticklen = 0;

    s->session->tlsext_tick = OPENSSL_malloc(ticklen);
    if (s->session->tlsext_tick == NULL) {
        SSLerr(SSL_F_TLS_PROCESS_NEW_SESSION_TICKET, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    if (!PACKET_copy_bytes(pkt, s->session->tlsext_tick, ticklen)) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_NEW_SESSION_TICKET, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }

    s->session->tlsext_tick_lifetime_hint = ticket_lifetime_hint;
    s->session->tlsext_ticklen = ticklen;
    /*
     * There are two ways to detect a resumed ticket session. One is to set
     * an appropriate session ID and then the server must return a match in
     * ServerHello. This allows the normal client session ID matching to work
     * and we know much earlier that the ticket has been accepted. The
     * other way is to set zero length session ID when the ticket is
     * presented and rely on the handshake to determine session resumption.
     * We choose the former approach because this fits in with assumptions
     * elsewhere in OpenSSL. The session ID is set to the SHA256 (or SHA1 is
     * SHA256 is disabled) hash of the ticket.
     */
    if (!EVP_Digest(s->session->tlsext_tick, ticklen,
                    s->session->session_id, &s->session->session_id_length,
                    EVP_sha256(), NULL)) {
        SSLerr(SSL_F_TLS_PROCESS_NEW_SESSION_TICKET, ERR_R_EVP_LIB);
        goto err;
    }
    return MSG_PROCESS_CONTINUE_READING;
 f_err:
    ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
    ossl_statem_set_error(s);
    return MSG_PROCESS_ERROR;
}

MSG_PROCESS_RETURN tls_process_cert_status(SSL *s, PACKET *pkt)
{
    int al;
    unsigned long resplen;
    unsigned int type;

    if (!PACKET_get_1(pkt, &type)
        || type != TLSEXT_STATUSTYPE_ocsp) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_CERT_STATUS, SSL_R_UNSUPPORTED_STATUS_TYPE);
        goto f_err;
    }
    if (!PACKET_get_net_3(pkt, &resplen)
        || PACKET_remaining(pkt) != resplen) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_CERT_STATUS, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }
    s->tlsext_ocsp_resp = OPENSSL_malloc(resplen);
    if (s->tlsext_ocsp_resp == NULL) {
        al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_CERT_STATUS, ERR_R_MALLOC_FAILURE);
        goto f_err;
    }
    if (!PACKET_copy_bytes(pkt, s->tlsext_ocsp_resp, resplen)) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_CERT_STATUS, SSL_R_LENGTH_MISMATCH);
        goto f_err;
    }
    s->tlsext_ocsp_resplen = resplen;
    return MSG_PROCESS_CONTINUE_READING;
 f_err:
    ssl3_send_alert(s, SSL3_AL_FATAL, al);
    ossl_statem_set_error(s);
    return MSG_PROCESS_ERROR;
}

MSG_PROCESS_RETURN tls_process_server_done(SSL *s, PACKET *pkt)
{
    if (PACKET_remaining(pkt) > 0) {
        /* should contain no data */
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        SSLerr(SSL_F_TLS_PROCESS_SERVER_DONE, SSL_R_LENGTH_MISMATCH);
        ossl_statem_set_error(s);
        return MSG_PROCESS_ERROR;
    }
#ifndef OPENSSL_NO_SRP
    if (s->s3->tmp.new_cipher->algorithm_mkey & SSL_kSRP) {
        if (SRP_Calc_A_param(s) <= 0) {
            SSLerr(SSL_F_TLS_PROCESS_SERVER_DONE, SSL_R_SRP_A_CALC);
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
            ossl_statem_set_error(s);
            return MSG_PROCESS_ERROR;
        }
    }
#endif

    /*
     * at this point we check that we have the required stuff from
     * the server
     */
    if (!ssl3_check_cert_and_algorithm(s)) {
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        ossl_statem_set_error(s);
        return MSG_PROCESS_ERROR;
    }

    /*
     * Call the ocsp status callback if needed. The |tlsext_ocsp_resp| and
     * |tlsext_ocsp_resplen| values will be set if we actually received a status
     * message, or NULL and -1 otherwise
     */
    if (s->tlsext_status_type != -1 && s->ctx->tlsext_status_cb != NULL) {
        int ret;
        ret = s->ctx->tlsext_status_cb(s, s->ctx->tlsext_status_arg);
        if (ret == 0) {
            ssl3_send_alert(s, SSL3_AL_FATAL,
                            SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE);
            SSLerr(SSL_F_TLS_PROCESS_SERVER_DONE,
                   SSL_R_INVALID_STATUS_RESPONSE);
            return MSG_PROCESS_ERROR;
        }
        if (ret < 0) {
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
            SSLerr(SSL_F_TLS_PROCESS_SERVER_DONE, ERR_R_MALLOC_FAILURE);
            return MSG_PROCESS_ERROR;
        }
    }
#ifndef OPENSSL_NO_CT
    if (s->ct_validation_callback != NULL) {
        /* Note we validate the SCTs whether or not we abort on error */
        if (!ssl_validate_ct(s) && (s->verify_mode & SSL_VERIFY_PEER)) {
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
            return MSG_PROCESS_ERROR;
        }
    }
#endif

    return MSG_PROCESS_FINISHED_READING;
}

static int tls_construct_cke_psk_preamble(SSL *s, unsigned char **p,
                                          size_t *pskhdrlen, int *al)
{
#ifndef OPENSSL_NO_PSK
    int ret = 0;
    /*
     * The callback needs PSK_MAX_IDENTITY_LEN + 1 bytes to return a
     * \0-terminated identity. The last byte is for us for simulating
     * strnlen.
     */
    char identity[PSK_MAX_IDENTITY_LEN + 1];
    size_t identitylen = 0;
    unsigned char psk[PSK_MAX_PSK_LEN];
    unsigned char *tmppsk = NULL;
    char *tmpidentity = NULL;
    size_t psklen = 0;

    if (s->psk_client_callback == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_PSK_PREAMBLE, SSL_R_PSK_NO_CLIENT_CB);
        *al = SSL_AD_INTERNAL_ERROR;
        goto err;
    }

    memset(identity, 0, sizeof(identity));

    psklen = s->psk_client_callback(s, s->session->psk_identity_hint,
                                    identity, sizeof(identity) - 1,
                                    psk, sizeof(psk));

    if (psklen > PSK_MAX_PSK_LEN) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_PSK_PREAMBLE, ERR_R_INTERNAL_ERROR);
        *al = SSL_AD_HANDSHAKE_FAILURE;
        goto err;
    } else if (psklen == 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_PSK_PREAMBLE,
               SSL_R_PSK_IDENTITY_NOT_FOUND);
        *al = SSL_AD_HANDSHAKE_FAILURE;
        goto err;
    }

    identitylen = strlen(identity);
    if (identitylen > PSK_MAX_IDENTITY_LEN) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_PSK_PREAMBLE, ERR_R_INTERNAL_ERROR);
        *al = SSL_AD_HANDSHAKE_FAILURE;
        goto err;
    }

    tmppsk = OPENSSL_memdup(psk, psklen);
    tmpidentity = OPENSSL_strdup(identity);
    if (tmppsk == NULL || tmpidentity == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_PSK_PREAMBLE, ERR_R_MALLOC_FAILURE);
        *al = SSL_AD_INTERNAL_ERROR;
        goto err;
    }

    OPENSSL_free(s->s3->tmp.psk);
    s->s3->tmp.psk = tmppsk;
    s->s3->tmp.psklen = psklen;
    tmppsk = NULL;
    OPENSSL_free(s->session->psk_identity);
    s->session->psk_identity = tmpidentity;
    tmpidentity = NULL;
    s2n(identitylen, *p);
    memcpy(*p, identity, identitylen);
    *pskhdrlen = 2 + identitylen;
    *p += identitylen;

    ret = 1;

 err:
    OPENSSL_cleanse(psk, psklen);
    OPENSSL_cleanse(identity, sizeof(identity));
    OPENSSL_clear_free(tmppsk, psklen);
    OPENSSL_clear_free(tmpidentity, identitylen);

    return ret;
#else
    SSLerr(SSL_F_TLS_CONSTRUCT_CKE_PSK_PREAMBLE, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

static int tls_construct_cke_rsa(SSL *s, unsigned char **p, int *len, int *al)
{
#ifndef OPENSSL_NO_RSA
    unsigned char *q;
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *pctx = NULL;
    size_t enclen;
    unsigned char *pms = NULL;
    size_t pmslen = 0;

    if (s->session->peer == NULL) {
        /*
         * We should always have a server certificate with SSL_kRSA.
         */
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_RSA, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    pkey = X509_get0_pubkey(s->session->peer);
    if (EVP_PKEY_get0_RSA(pkey) == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_RSA, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    pmslen = SSL_MAX_MASTER_KEY_LENGTH;
    pms = OPENSSL_malloc(pmslen);
    if (pms == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_RSA, ERR_R_MALLOC_FAILURE);
        *al = SSL_AD_INTERNAL_ERROR;
        return 0;
    }

    pms[0] = s->client_version >> 8;
    pms[1] = s->client_version & 0xff;
    if (RAND_bytes(pms + 2, pmslen - 2) <= 0) {
        goto err;
    }

    q = *p;
    /* Fix buf for TLS and beyond */
    if (s->version > SSL3_VERSION)
        *p += 2;
    pctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (pctx == NULL || EVP_PKEY_encrypt_init(pctx) <= 0
        || EVP_PKEY_encrypt(pctx, NULL, &enclen, pms, pmslen) <= 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_RSA, ERR_R_EVP_LIB);
        goto err;
    }
    if (EVP_PKEY_encrypt(pctx, *p, &enclen, pms, pmslen) <= 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_RSA, SSL_R_BAD_RSA_ENCRYPT);
        goto err;
    }
    *len = enclen;
    EVP_PKEY_CTX_free(pctx);
    pctx = NULL;
# ifdef PKCS1_CHECK
    if (s->options & SSL_OP_PKCS1_CHECK_1)
        (*p)[1]++;
    if (s->options & SSL_OP_PKCS1_CHECK_2)
        tmp_buf[0] = 0x70;
# endif

    /* Fix buf for TLS and beyond */
    if (s->version > SSL3_VERSION) {
        s2n(*len, q);
        *len += 2;
    }

    s->s3->tmp.pms = pms;
    s->s3->tmp.pmslen = pmslen;

    return 1;
 err:
    OPENSSL_clear_free(pms, pmslen);
    EVP_PKEY_CTX_free(pctx);

    return 0;
#else
    SSLerr(SSL_F_TLS_CONSTRUCT_CKE_RSA, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

static int tls_construct_cke_dhe(SSL *s, unsigned char **p, int *len, int *al)
{
#ifndef OPENSSL_NO_DH
    DH *dh_clnt = NULL;
    const BIGNUM *pub_key;
    EVP_PKEY *ckey = NULL, *skey = NULL;

    skey = s->s3->peer_tmp;
    if (skey == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_DHE, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    ckey = ssl_generate_pkey(skey);
    if (ckey == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_DHE, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    dh_clnt = EVP_PKEY_get0_DH(ckey);

    if (dh_clnt == NULL || ssl_derive(s, ckey, skey) == 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_DHE, ERR_R_INTERNAL_ERROR);
        EVP_PKEY_free(ckey);
        return 0;
    }

    /* send off the data */
    DH_get0_key(dh_clnt, &pub_key, NULL);
    *len = BN_num_bytes(pub_key);
    s2n(*len, *p);
    BN_bn2bin(pub_key, *p);
    *len += 2;
    EVP_PKEY_free(ckey);

    return 1;
#else
    SSLerr(SSL_F_TLS_CONSTRUCT_CKE_DHE, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

static int tls_construct_cke_ecdhe(SSL *s, unsigned char **p, int *len, int *al)
{
#ifndef OPENSSL_NO_EC
    unsigned char *encodedPoint = NULL;
    int encoded_pt_len = 0;
    EVP_PKEY *ckey = NULL, *skey = NULL;

    skey = s->s3->peer_tmp;
    if (skey == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_ECDHE, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    ckey = ssl_generate_pkey(skey);
    if (ckey == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_ECDHE, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    if (ssl_derive(s, ckey, skey) == 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_ECDHE, ERR_R_EVP_LIB);
        goto err;
    }

    /* Generate encoding of client key */
    encoded_pt_len = EVP_PKEY_get1_tls_encodedpoint(ckey, &encodedPoint);

    if (encoded_pt_len == 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_ECDHE, ERR_R_EC_LIB);
        goto err;
    }

    EVP_PKEY_free(ckey);
    ckey = NULL;

    *len = encoded_pt_len;

    /* length of encoded point */
    **p = *len;
    *p += 1;
    /* copy the point */
    memcpy(*p, encodedPoint, *len);
    /* increment len to account for length field */
    *len += 1;

    OPENSSL_free(encodedPoint);

    return 1;
 err:
    EVP_PKEY_free(ckey);
    return 0;
#else
    SSLerr(SSL_F_TLS_CONSTRUCT_CKE_ECDHE, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

static int tls_construct_cke_gost(SSL *s, unsigned char **p, int *len, int *al)
{
#ifndef OPENSSL_NO_GOST
    /* GOST key exchange message creation */
    EVP_PKEY_CTX *pkey_ctx = NULL;
    X509 *peer_cert;
    size_t msglen;
    unsigned int md_len;
    unsigned char shared_ukm[32], tmp[256];
    EVP_MD_CTX *ukm_hash = NULL;
    int dgst_nid = NID_id_GostR3411_94;
    unsigned char *pms = NULL;
    size_t pmslen = 0;

    if ((s->s3->tmp.new_cipher->algorithm_auth & SSL_aGOST12) != 0)
        dgst_nid = NID_id_GostR3411_2012_256;

    /*
     * Get server certificate PKEY and create ctx from it
     */
    peer_cert = s->session->peer;
    if (!peer_cert) {
        *al = SSL_AD_HANDSHAKE_FAILURE;
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_GOST,
               SSL_R_NO_GOST_CERTIFICATE_SENT_BY_PEER);
        return 0;
    }

    pkey_ctx = EVP_PKEY_CTX_new(X509_get0_pubkey(peer_cert), NULL);
    if (pkey_ctx == NULL) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_GOST, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    /*
     * If we have send a certificate, and certificate key
     * parameters match those of server certificate, use
     * certificate key for key exchange
     */

    /* Otherwise, generate ephemeral key pair */
    pmslen = 32;
    pms = OPENSSL_malloc(pmslen);
    if (pms == NULL) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_GOST, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (EVP_PKEY_encrypt_init(pkey_ctx) <= 0
        /* Generate session key */
        || RAND_bytes(pms, pmslen) <= 0) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_GOST, ERR_R_INTERNAL_ERROR);
        goto err;
    };
    /*
     * Compute shared IV and store it in algorithm-specific context
     * data
     */
    ukm_hash = EVP_MD_CTX_new();
    if (ukm_hash == NULL
        || EVP_DigestInit(ukm_hash, EVP_get_digestbynid(dgst_nid)) <= 0
        || EVP_DigestUpdate(ukm_hash, s->s3->client_random,
                            SSL3_RANDOM_SIZE) <= 0
        || EVP_DigestUpdate(ukm_hash, s->s3->server_random,
                            SSL3_RANDOM_SIZE) <= 0
        || EVP_DigestFinal_ex(ukm_hash, shared_ukm, &md_len) <= 0) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_GOST, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    EVP_MD_CTX_free(ukm_hash);
    ukm_hash = NULL;
    if (EVP_PKEY_CTX_ctrl(pkey_ctx, -1, EVP_PKEY_OP_ENCRYPT,
                          EVP_PKEY_CTRL_SET_IV, 8, shared_ukm) < 0) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_GOST, SSL_R_LIBRARY_BUG);
        goto err;
    }
    /* Make GOST keytransport blob message */
    /*
     * Encapsulate it into sequence
     */
    *((*p)++) = V_ASN1_SEQUENCE | V_ASN1_CONSTRUCTED;
    msglen = 255;
    if (EVP_PKEY_encrypt(pkey_ctx, tmp, &msglen, pms, pmslen) <= 0) {
        *al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_GOST, SSL_R_LIBRARY_BUG);
        goto err;
    }
    if (msglen >= 0x80) {
        *((*p)++) = 0x81;
        *((*p)++) = msglen & 0xff;
        *len = msglen + 3;
    } else {
        *((*p)++) = msglen & 0xff;
        *len = msglen + 2;
    }
    memcpy(*p, tmp, msglen);

    EVP_PKEY_CTX_free(pkey_ctx);
    s->s3->tmp.pms = pms;
    s->s3->tmp.pmslen = pmslen;

    return 1;
 err:
    EVP_PKEY_CTX_free(pkey_ctx);
    OPENSSL_clear_free(pms, pmslen);
    EVP_MD_CTX_free(ukm_hash);
    return 0;
#else
    SSLerr(SSL_F_TLS_CONSTRUCT_CKE_GOST, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

static int tls_construct_cke_srp(SSL *s, unsigned char **p, int *len, int *al)
{
#ifndef OPENSSL_NO_SRP
    if (s->srp_ctx.A != NULL) {
        /* send off the data */
        *len = BN_num_bytes(s->srp_ctx.A);
        s2n(*len, *p);
        BN_bn2bin(s->srp_ctx.A, *p);
        *len += 2;
    } else {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_SRP, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    OPENSSL_free(s->session->srp_username);
    s->session->srp_username = OPENSSL_strdup(s->srp_ctx.login);
    if (s->session->srp_username == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CKE_SRP, ERR_R_MALLOC_FAILURE);
        return 0;
    }

    return 1;
#else
    SSLerr(SSL_F_TLS_CONSTRUCT_CKE_SRP, ERR_R_INTERNAL_ERROR);
    *al = SSL_AD_INTERNAL_ERROR;
    return 0;
#endif
}

int tls_construct_client_key_exchange(SSL *s)
{
    unsigned char *p;
    int len;
    size_t pskhdrlen = 0;
    unsigned long alg_k;
    int al = -1;

    alg_k = s->s3->tmp.new_cipher->algorithm_mkey;

    p = ssl_handshake_start(s);

    if ((alg_k & SSL_PSK)
        && !tls_construct_cke_psk_preamble(s, &p, &pskhdrlen, &al))
        goto err;

    if (alg_k & SSL_kPSK) {
        len = 0;
    } else if (alg_k & (SSL_kRSA | SSL_kRSAPSK)) {
        if (!tls_construct_cke_rsa(s, &p, &len, &al))
            goto err;
    } else if (alg_k & (SSL_kDHE | SSL_kDHEPSK)) {
        if (!tls_construct_cke_dhe(s, &p, &len, &al))
            goto err;
    } else if (alg_k & (SSL_kECDHE | SSL_kECDHEPSK)) {
        if (!tls_construct_cke_ecdhe(s, &p, &len, &al))
            goto err;
    } else if (alg_k & SSL_kGOST) {
        if (!tls_construct_cke_gost(s, &p, &len, &al))
            goto err;
    } else if (alg_k & SSL_kSRP) {
        if (!tls_construct_cke_srp(s, &p, &len, &al))
            goto err;
    } else {
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_KEY_EXCHANGE, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    len += pskhdrlen;

    if (!ssl_set_handshake_header(s, SSL3_MT_CLIENT_KEY_EXCHANGE, len)) {
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_KEY_EXCHANGE, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    return 1;
 err:
    if (al != -1)
        ssl3_send_alert(s, SSL3_AL_FATAL, al);
    OPENSSL_clear_free(s->s3->tmp.pms, s->s3->tmp.pmslen);
    s->s3->tmp.pms = NULL;
#ifndef OPENSSL_NO_PSK
    OPENSSL_clear_free(s->s3->tmp.psk, s->s3->tmp.psklen);
    s->s3->tmp.psk = NULL;
#endif
    ossl_statem_set_error(s);
    return 0;
}

int tls_client_key_exchange_post_work(SSL *s)
{
    unsigned char *pms = NULL;
    size_t pmslen = 0;

    pms = s->s3->tmp.pms;
    pmslen = s->s3->tmp.pmslen;

#ifndef OPENSSL_NO_SRP
    /* Check for SRP */
    if (s->s3->tmp.new_cipher->algorithm_mkey & SSL_kSRP) {
        if (!srp_generate_client_master_secret(s)) {
            SSLerr(SSL_F_TLS_CLIENT_KEY_EXCHANGE_POST_WORK,
                   ERR_R_INTERNAL_ERROR);
            goto err;
        }
        return 1;
    }
#endif

    if (pms == NULL && !(s->s3->tmp.new_cipher->algorithm_mkey & SSL_kPSK)) {
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        SSLerr(SSL_F_TLS_CLIENT_KEY_EXCHANGE_POST_WORK, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    if (!ssl_generate_master_secret(s, pms, pmslen, 1)) {
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        SSLerr(SSL_F_TLS_CLIENT_KEY_EXCHANGE_POST_WORK, ERR_R_INTERNAL_ERROR);
        /* ssl_generate_master_secret frees the pms even on error */
        pms = NULL;
        pmslen = 0;
        goto err;
    }
    pms = NULL;
    pmslen = 0;

#ifndef OPENSSL_NO_SCTP
    if (SSL_IS_DTLS(s)) {
        unsigned char sctpauthkey[64];
        char labelbuffer[sizeof(DTLS1_SCTP_AUTH_LABEL)];

        /*
         * Add new shared key for SCTP-Auth, will be ignored if no SCTP
         * used.
         */
        memcpy(labelbuffer, DTLS1_SCTP_AUTH_LABEL,
               sizeof(DTLS1_SCTP_AUTH_LABEL));

        if (SSL_export_keying_material(s, sctpauthkey,
                                       sizeof(sctpauthkey), labelbuffer,
                                       sizeof(labelbuffer), NULL, 0, 0) <= 0)
            goto err;

        BIO_ctrl(SSL_get_wbio(s), BIO_CTRL_DGRAM_SCTP_ADD_AUTH_KEY,
                 sizeof(sctpauthkey), sctpauthkey);
    }
#endif

    return 1;
 err:
    OPENSSL_clear_free(pms, pmslen);
    s->s3->tmp.pms = NULL;
    return 0;
}

int tls_construct_client_verify(SSL *s)
{
    unsigned char *p;
    EVP_PKEY *pkey;
    const EVP_MD *md = s->s3->tmp.md[s->cert->key - s->cert->pkeys];
    EVP_MD_CTX *mctx;
    unsigned u = 0;
    unsigned long n = 0;
    long hdatalen = 0;
    void *hdata;

    mctx = EVP_MD_CTX_new();
    if (mctx == NULL) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_VERIFY, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    p = ssl_handshake_start(s);
    pkey = s->cert->key->privatekey;

    hdatalen = BIO_get_mem_data(s->s3->handshake_buffer, &hdata);
    if (hdatalen <= 0) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_VERIFY, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if (SSL_USE_SIGALGS(s)) {
        if (!tls12_get_sigandhash(p, pkey, md)) {
            SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_VERIFY, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        p += 2;
        n = 2;
    }
#ifdef SSL_DEBUG
    fprintf(stderr, "Using client alg %s\n", EVP_MD_name(md));
#endif
    if (!EVP_SignInit_ex(mctx, md, NULL)
        || !EVP_SignUpdate(mctx, hdata, hdatalen)
        || (s->version == SSL3_VERSION
            && !EVP_MD_CTX_ctrl(mctx, EVP_CTRL_SSL3_MASTER_SECRET,
                                s->session->master_key_length,
                                s->session->master_key))
        || !EVP_SignFinal(mctx, p + 2, &u, pkey)) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_VERIFY, ERR_R_EVP_LIB);
        goto err;
    }
#ifndef OPENSSL_NO_GOST
    {
        int pktype = EVP_PKEY_id(pkey);
        if (pktype == NID_id_GostR3410_2001
            || pktype == NID_id_GostR3410_2012_256
            || pktype == NID_id_GostR3410_2012_512)
            BUF_reverse(p + 2, NULL, u);
    }
#endif

    s2n(u, p);
    n += u + 2;
    /* Digest cached records and discard handshake buffer */
    if (!ssl3_digest_cached_records(s, 0))
        goto err;
    if (!ssl_set_handshake_header(s, SSL3_MT_CERTIFICATE_VERIFY, n)) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_VERIFY, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    EVP_MD_CTX_free(mctx);
    return 1;
 err:
    EVP_MD_CTX_free(mctx);
    return 0;
}

/*
 * Check a certificate can be used for client authentication. Currently check
 * cert exists, if we have a suitable digest for TLS 1.2 if static DH client
 * certificates can be used and optionally checks suitability for Suite B.
 */
static int ssl3_check_client_certificate(SSL *s)
{
    if (!s->cert || !s->cert->key->x509 || !s->cert->key->privatekey)
        return 0;
    /* If no suitable signature algorithm can't use certificate */
    if (SSL_USE_SIGALGS(s) && !s->s3->tmp.md[s->cert->key - s->cert->pkeys])
        return 0;
    /*
     * If strict mode check suitability of chain before using it. This also
     * adjusts suite B digest if necessary.
     */
    if (s->cert->cert_flags & SSL_CERT_FLAGS_CHECK_TLS_STRICT &&
        !tls1_check_chain(s, NULL, NULL, NULL, -2))
        return 0;
    return 1;
}

WORK_STATE tls_prepare_client_certificate(SSL *s, WORK_STATE wst)
{
    X509 *x509 = NULL;
    EVP_PKEY *pkey = NULL;
    int i;

    if (wst == WORK_MORE_A) {
        /* Let cert callback update client certificates if required */
        if (s->cert->cert_cb) {
            i = s->cert->cert_cb(s, s->cert->cert_cb_arg);
            if (i < 0) {
                s->rwstate = SSL_X509_LOOKUP;
                return WORK_MORE_A;
            }
            if (i == 0) {
                ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
                ossl_statem_set_error(s);
                return 0;
            }
            s->rwstate = SSL_NOTHING;
        }
        if (ssl3_check_client_certificate(s))
            return WORK_FINISHED_CONTINUE;

        /* Fall through to WORK_MORE_B */
        wst = WORK_MORE_B;
    }

    /* We need to get a client cert */
    if (wst == WORK_MORE_B) {
        /*
         * If we get an error, we need to ssl->rwstate=SSL_X509_LOOKUP;
         * return(-1); We then get retied later
         */
        i = ssl_do_client_cert_cb(s, &x509, &pkey);
        if (i < 0) {
            s->rwstate = SSL_X509_LOOKUP;
            return WORK_MORE_B;
        }
        s->rwstate = SSL_NOTHING;
        if ((i == 1) && (pkey != NULL) && (x509 != NULL)) {
            if (!SSL_use_certificate(s, x509) || !SSL_use_PrivateKey(s, pkey))
                i = 0;
        } else if (i == 1) {
            i = 0;
            SSLerr(SSL_F_TLS_PREPARE_CLIENT_CERTIFICATE,
                   SSL_R_BAD_DATA_RETURNED_BY_CALLBACK);
        }

        X509_free(x509);
        EVP_PKEY_free(pkey);
        if (i && !ssl3_check_client_certificate(s))
            i = 0;
        if (i == 0) {
            if (s->version == SSL3_VERSION) {
                s->s3->tmp.cert_req = 0;
                ssl3_send_alert(s, SSL3_AL_WARNING, SSL_AD_NO_CERTIFICATE);
                return WORK_FINISHED_CONTINUE;
            } else {
                s->s3->tmp.cert_req = 2;
                if (!ssl3_digest_cached_records(s, 0)) {
                    ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
                    ossl_statem_set_error(s);
                    return 0;
                }
            }
        }

        return WORK_FINISHED_CONTINUE;
    }

    /* Shouldn't ever get here */
    return WORK_ERROR;
}

int tls_construct_client_certificate(SSL *s)
{
    if (!ssl3_output_cert_chain(s,
                                (s->s3->tmp.cert_req ==
                                 2) ? NULL : s->cert->key)) {
        SSLerr(SSL_F_TLS_CONSTRUCT_CLIENT_CERTIFICATE, ERR_R_INTERNAL_ERROR);
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        ossl_statem_set_error(s);
        return 0;
    }

    return 1;
}

#define has_bits(i,m)   (((i)&(m)) == (m))

int ssl3_check_cert_and_algorithm(SSL *s)
{
    int i;
#ifndef OPENSSL_NO_EC
    int idx;
#endif
    long alg_k, alg_a;
    EVP_PKEY *pkey = NULL;
    int al = SSL_AD_HANDSHAKE_FAILURE;

    alg_k = s->s3->tmp.new_cipher->algorithm_mkey;
    alg_a = s->s3->tmp.new_cipher->algorithm_auth;

    /* we don't have a certificate */
    if ((alg_a & SSL_aNULL) || (alg_k & SSL_kPSK))
        return (1);

    /* This is the passed certificate */

#ifndef OPENSSL_NO_EC
    idx = s->session->peer_type;
    if (idx == SSL_PKEY_ECC) {
        if (ssl_check_srvr_ecc_cert_and_alg(s->session->peer, s) == 0) {
            /* check failed */
            SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM, SSL_R_BAD_ECC_CERT);
            goto f_err;
        } else {
            return 1;
        }
    } else if (alg_a & SSL_aECDSA) {
        SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,
               SSL_R_MISSING_ECDSA_SIGNING_CERT);
        goto f_err;
    }
#endif
    pkey = X509_get0_pubkey(s->session->peer);
    i = X509_certificate_type(s->session->peer, pkey);

    /* Check that we have a certificate if we require one */
    if ((alg_a & SSL_aRSA) && !has_bits(i, EVP_PK_RSA | EVP_PKT_SIGN)) {
        SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,
               SSL_R_MISSING_RSA_SIGNING_CERT);
        goto f_err;
    }
#ifndef OPENSSL_NO_DSA
    else if ((alg_a & SSL_aDSS) && !has_bits(i, EVP_PK_DSA | EVP_PKT_SIGN)) {
        SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,
               SSL_R_MISSING_DSA_SIGNING_CERT);
        goto f_err;
    }
#endif
#ifndef OPENSSL_NO_RSA
    if (alg_k & (SSL_kRSA | SSL_kRSAPSK) &&
        !has_bits(i, EVP_PK_RSA | EVP_PKT_ENC)) {
        SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,
               SSL_R_MISSING_RSA_ENCRYPTING_CERT);
        goto f_err;
    }
#endif
#ifndef OPENSSL_NO_DH
    if ((alg_k & SSL_kDHE) && (s->s3->peer_tmp == NULL)) {
        al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM, ERR_R_INTERNAL_ERROR);
        goto f_err;
    }
#endif

    return (1);
 f_err:
    ssl3_send_alert(s, SSL3_AL_FATAL, al);
    return (0);
}

#ifndef OPENSSL_NO_NEXTPROTONEG
int tls_construct_next_proto(SSL *s)
{
    unsigned int len, padding_len;
    unsigned char *d;

    len = s->next_proto_negotiated_len;
    padding_len = 32 - ((len + 2) % 32);
    d = (unsigned char *)s->init_buf->data;
    d[4] = len;
    memcpy(d + 5, s->next_proto_negotiated, len);
    d[5 + len] = padding_len;
    memset(d + 6 + len, 0, padding_len);
    *(d++) = SSL3_MT_NEXT_PROTO;
    l2n3(2 + len + padding_len, d);
    s->init_num = 4 + 2 + len + padding_len;
    s->init_off = 0;

    return 1;
}
#endif

int ssl_do_client_cert_cb(SSL *s, X509 **px509, EVP_PKEY **ppkey)
{
    int i = 0;
#ifndef OPENSSL_NO_ENGINE
    if (s->ctx->client_cert_engine) {
        i = ENGINE_load_ssl_client_cert(s->ctx->client_cert_engine, s,
                                        SSL_get_client_CA_list(s),
                                        px509, ppkey, NULL, NULL, NULL);
        if (i != 0)
            return i;
    }
#endif
    if (s->ctx->client_cert_cb)
        i = s->ctx->client_cert_cb(s, px509, ppkey);
    return i;
}

int ssl_cipher_list_to_bytes(SSL *s, STACK_OF(SSL_CIPHER) *sk, unsigned char *p)
{
    int i, j = 0;
    const SSL_CIPHER *c;
    unsigned char *q;
    int empty_reneg_info_scsv = !s->renegotiate;
    /* Set disabled masks for this session */
    ssl_set_client_disabled(s);

    if (sk == NULL)
        return (0);
    q = p;

    for (i = 0; i < sk_SSL_CIPHER_num(sk); i++) {
        c = sk_SSL_CIPHER_value(sk, i);
        /* Skip disabled ciphers */
        if (ssl_cipher_disabled(s, c, SSL_SECOP_CIPHER_SUPPORTED, 0))
            continue;
        j = s->method->put_cipher_by_char(c, p);
        p += j;
    }
    /*
     * If p == q, no ciphers; caller indicates an error. Otherwise, add
     * applicable SCSVs.
     */
    if (p != q) {
        if (empty_reneg_info_scsv) {
            static SSL_CIPHER scsv = {
                0, NULL, SSL3_CK_SCSV, 0, 0, 0, 0, 0, 0, 0, 0, 0
            };
            j = s->method->put_cipher_by_char(&scsv, p);
            p += j;
        }
        if (s->mode & SSL_MODE_SEND_FALLBACK_SCSV) {
            static SSL_CIPHER scsv = {
                0, NULL, SSL3_CK_FALLBACK_SCSV, 0, 0, 0, 0, 0, 0, 0, 0, 0
            };
            j = s->method->put_cipher_by_char(&scsv, p);
            p += j;
        }
    }

    return (p - q);
}
