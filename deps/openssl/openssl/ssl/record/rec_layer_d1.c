/*
 * Copyright 2005-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <errno.h>
#include "../ssl_local.h"
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include "record_local.h"
#include "internal/packet.h"
#include "internal/cryptlib.h"
#include "internal/ssl_unwrap.h"

int DTLS_RECORD_LAYER_new(RECORD_LAYER *rl)
{
    DTLS_RECORD_LAYER *d;

    if ((d = OPENSSL_malloc(sizeof(*d))) == NULL)
        return 0;

    rl->d = d;

    d->buffered_app_data = pqueue_new();

    if (d->buffered_app_data == NULL) {
        OPENSSL_free(d);
        rl->d = NULL;
        return 0;
    }

    return 1;
}

void DTLS_RECORD_LAYER_free(RECORD_LAYER *rl)
{
    if (rl->d == NULL)
        return;

    DTLS_RECORD_LAYER_clear(rl);
    pqueue_free(rl->d->buffered_app_data);
    OPENSSL_free(rl->d);
    rl->d = NULL;
}

void DTLS_RECORD_LAYER_clear(RECORD_LAYER *rl)
{
    DTLS_RECORD_LAYER *d;
    pitem *item = NULL;
    TLS_RECORD *rec;
    pqueue *buffered_app_data;

    d = rl->d;

    while ((item = pqueue_pop(d->buffered_app_data)) != NULL) {
        rec = (TLS_RECORD *)item->data;

        if (rl->s->options & SSL_OP_CLEANSE_PLAINTEXT)
            OPENSSL_cleanse(rec->allocdata, rec->length);
        OPENSSL_free(rec->allocdata);
        OPENSSL_free(item->data);
        pitem_free(item);
    }

    buffered_app_data = d->buffered_app_data;
    memset(d, 0, sizeof(*d));
    d->buffered_app_data = buffered_app_data;
}

static int dtls_buffer_record(SSL_CONNECTION *s, TLS_RECORD *rec)
{
    TLS_RECORD *rdata;
    pitem *item;
    struct pqueue_st *queue = s->rlayer.d->buffered_app_data;

    /* Limit the size of the queue to prevent DOS attacks */
    if (pqueue_size(queue) >= 100)
        return 0;

    /* We don't buffer partially read records */
    if (!ossl_assert(rec->off == 0))
        return -1;

    rdata = OPENSSL_malloc(sizeof(*rdata));
    item = pitem_new_u64(rec->seq_num, rdata);
    if (rdata == NULL || item == NULL) {
        OPENSSL_free(rdata);
        pitem_free(item);
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return -1;
    }

    *rdata = *rec;
    /*
     * We will release the record from the record layer soon, so we take a copy
     * now. Copying data isn't good - but this should be infrequent so we
     * accept it here.
     */
    rdata->data = rdata->allocdata = OPENSSL_memdup(rec->data, rec->length);
    if (rdata->data == NULL) {
        OPENSSL_free(rdata);
        pitem_free(item);
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_CRYPTO_LIB);
        return -1;
    }
    /*
     * We use a NULL rechandle to indicate that the data field has been
     * allocated by us.
     */
    rdata->rechandle = NULL;

    item->data = rdata;

#ifndef OPENSSL_NO_SCTP
    /* Store bio_dgram_sctp_rcvinfo struct */
    if (s->rbio != NULL && BIO_dgram_is_sctp(s->rbio) && (ossl_statem_get_state(s) == TLS_ST_SR_FINISHED || ossl_statem_get_state(s) == TLS_ST_CR_FINISHED)) {
        BIO_ctrl(s->rbio, BIO_CTRL_DGRAM_SCTP_GET_RCVINFO,
            sizeof(rdata->recordinfo), &rdata->recordinfo);
    }
#endif

    if (pqueue_insert(queue, item) == NULL) {
        /* Must be a duplicate so ignore it */
        OPENSSL_free(rdata->allocdata);
        OPENSSL_free(rdata);
        pitem_free(item);
    }

    return 1;
}

/* Unbuffer a previously buffered TLS_RECORD structure if any */
static void dtls_unbuffer_record(SSL_CONNECTION *s)
{
    TLS_RECORD *rdata;
    pitem *item;

    /* If we already have records to handle then do nothing */
    if (s->rlayer.curr_rec < s->rlayer.num_recs)
        return;

    item = pqueue_pop(s->rlayer.d->buffered_app_data);
    if (item != NULL) {
        rdata = (TLS_RECORD *)item->data;

        s->rlayer.tlsrecs[0] = *rdata;
        s->rlayer.num_recs = 1;
        s->rlayer.curr_rec = 0;

#ifndef OPENSSL_NO_SCTP
        /* Restore bio_dgram_sctp_rcvinfo struct */
        if (s->rbio != NULL && BIO_dgram_is_sctp(s->rbio)) {
            BIO_ctrl(s->rbio, BIO_CTRL_DGRAM_SCTP_SET_RCVINFO,
                sizeof(rdata->recordinfo), &rdata->recordinfo);
        }
#endif

        OPENSSL_free(item->data);
        pitem_free(item);
    }
}

/*-
 * Return up to 'len' payload bytes received in 'type' records.
 * 'type' is one of the following:
 *
 *   -  SSL3_RT_HANDSHAKE
 *   -  SSL3_RT_APPLICATION_DATA (when ssl3_read calls us)
 *   -  0 (during a shutdown, no data has to be returned)
 *
 * If we don't have stored data to work from, read an SSL/TLS record first
 * (possibly multiple records if we still don't have anything to return).
 *
 * This function must handle any surprises the peer may have for us, such as
 * Alert records (e.g. close_notify) or renegotiation requests. ChangeCipherSpec
 * messages are treated as if they were handshake messages *if* the |recd_type|
 * argument is non NULL.
 * Also if record payloads contain fragments too small to process, we store
 * them until there is enough for the respective protocol (the record protocol
 * may use arbitrary fragmentation and even interleaving):
 *     Change cipher spec protocol
 *             just 1 byte needed, no need for keeping anything stored
 *     Alert protocol
 *             2 bytes needed (AlertLevel, AlertDescription)
 *     Handshake protocol
 *             4 bytes needed (HandshakeType, uint24 length) -- we just have
 *             to detect unexpected Client Hello and Hello Request messages
 *             here, anything else is handled by higher layers
 *     Application data protocol
 *             none of our business
 */
int dtls1_read_bytes(SSL *s, uint8_t type, uint8_t *recvd_type,
    unsigned char *buf, size_t len,
    int peek, size_t *readbytes)
{
    int i, j, ret;
    size_t n;
    TLS_RECORD *rr;
    void (*cb)(const SSL *ssl, int type2, int val) = NULL;
    SSL_CONNECTION *sc = SSL_CONNECTION_FROM_SSL(s);
    int is_dtls13;
    int in_early_data;
    int current_state;

    if (sc == NULL)
        return -1;

    is_dtls13 = SSL_CONNECTION_IS_DTLS13(sc);

    if ((type && (type != SSL3_RT_APPLICATION_DATA) && (type != SSL3_RT_HANDSHAKE)) || (peek && (type != SSL3_RT_APPLICATION_DATA))) {
        SSLfatal(sc, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return -1;
    }

    if (!ossl_statem_get_in_handshake(sc) && SSL_in_init(s)) {
        /* type == SSL3_RT_APPLICATION_DATA */
        i = sc->handshake_func(s);
        /* SSLfatal() already called if appropriate */
        if (i < 0)
            return i;
        if (i == 0)
            return -1;
    }

start:
    sc->rwstate = SSL_NOTHING;
    in_early_data = (sc->early_data_state == SSL_EARLY_DATA_READING);

    /*
     * We are not handshaking and have no data yet, so process data buffered
     * during the last handshake in advance, if any.
     */
    if (SSL_is_init_finished(s))
        dtls_unbuffer_record(sc);

    /* Check for timeout */
    if (dtls1_handle_timeout(sc) > 0) {
        goto start;
    } else if (ossl_statem_in_error(sc)) {
        /* dtls1_handle_timeout() has failed with a fatal error */
        return -1;
    }

    /* get new packet if necessary */
    if (sc->rlayer.curr_rec >= sc->rlayer.num_recs) {
        sc->rlayer.curr_rec = sc->rlayer.num_recs = 0;
        do {
            rr = &sc->rlayer.tlsrecs[sc->rlayer.num_recs];

            ret = HANDLE_RLAYER_READ_RETURN(sc,
                sc->rlayer.rrlmethod->read_record(sc->rlayer.rrl,
                    &rr->rechandle,
                    &rr->version, &rr->type,
                    &rr->data, &rr->length,
                    &rr->epoch, &rr->seq_num));

            /*
             * DTLS1.3 will move the Server and Client's Read Record
             * during the handshake once it has received a record
             * in the new Epoch.
             *
             * We cannot move to the next epoch (1 or 2) until we
             * have read the client/server hello.
             */
            if (ret <= 0 && is_dtls13
                && sc->rlayer.rrlmethod->unprocessed_records(sc->rlayer.rrl) > 0
                && ((SSL_in_init(s) && sc->dtls13_process_hello) || in_early_data)) {
                int which = SSL3_CC_HANDSHAKE;

                if (sc->server)
                    which |= SSL3_CHANGE_CIPHER_SERVER_READ;
                else
                    which |= SSL3_CHANGE_CIPHER_CLIENT_READ;
                /*
                 * Change the Read Record Layer to next read epoch
                 */
                if (!s->method->ssl3_enc->change_cipher_state(sc,
                        which)) {
                    SSLfatal(sc, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    return -1;
                }

                /*
                 * Read the Buffered epoch
                 */
                ret = HANDLE_RLAYER_READ_RETURN(sc,
                    sc->rlayer.rrlmethod->read_record(sc->rlayer.rrl,
                        &rr->rechandle,
                        &rr->version, &rr->type,
                        &rr->data, &rr->length,
                        &rr->epoch, &rr->seq_num));
            }

            if (ret <= 0) {
                ret = dtls1_read_failed(sc, ret);
                /*
                 * Anything other than a timeout is an error. SSLfatal() already
                 * called if appropriate.
                 */
                if (ret <= 0)
                    return ret;
                else
                    goto start;
            }
            rr->off = 0;
            sc->rlayer.num_recs++;
        } while (sc->rlayer.rrlmethod->processed_read_pending(sc->rlayer.rrl)
            && sc->rlayer.num_recs < SSL_MAX_PIPELINES);
    }
    rr = &sc->rlayer.tlsrecs[sc->rlayer.curr_rec];

    /*
     * Reset the count of consecutive warning alerts if we've got a non-empty
     * record that isn't an alert.
     */
    if (rr->type != SSL3_RT_ALERT && rr->length != 0)
        sc->rlayer.alert_count = 0;

    /* we now have a packet which can be read and processed */
    current_state = SSL_get_state(s);
    if ((sc->s3.change_cipher_spec /* set when we receive ChangeCipherSpec,
                                    * reset by ssl3_get_finished */
            && (rr->type != SSL3_RT_HANDSHAKE))
        || (is_dtls13 && rr->epoch >= 3 /* For DTLS 1.3 we can receive
                                         * Application Data before we
                                         * receive an expecting ACK
                                         * message */
            && (current_state == TLS_ST_CW_FINISHED
                || current_state == TLS_ST_CW_KEY_UPDATE
                || current_state == TLS_ST_SW_KEY_UPDATE
                || current_state == TLS_ST_SW_SESSION_TICKET
                || (current_state == TLS_ST_OK && SSL_in_init(s)))
            && rr->type == SSL3_RT_APPLICATION_DATA)) {
        /*
         * For DTLS 1.3 we received Application Data while we are
         * waiting for an Acknowledgement record. Buffer this data so
         * it can be processed once we have received the Acknowledgement.
         * When DTLS 1.3 receives application data and it is already
         * in TLS_ST_OK and is processing a handshake message like
         * NewSessionTicket or KeyUpdate the application data is
         * buffered.
         *
         * For non-DTLS 1.3 we now have application data between CCS and
         * Finished. Most likely the packets were reordered on their way,
         * so buffer the application data for later processing rather than
         * dropping the connection.
         */
        if (dtls_buffer_record(sc, rr) < 0) {
            /* SSLfatal() already called */
            return -1;
        }
        if (!ssl_release_record(sc, rr, 0))
            return -1;

        if (is_dtls13 && current_state == TLS_ST_OK && SSL_in_init(s)) {
            return -1;
        }
        goto start;
    }

    /*
     * If the other end has shut down, throw anything we read away (even in
     * 'peek' mode)
     */
    if (sc->shutdown & SSL_RECEIVED_SHUTDOWN) {
        if (!ssl_release_record(sc, rr, 0))
            return -1;
        sc->rwstate = SSL_NOTHING;
        return 0;
    }

    if ((rr->type == SSL3_RT_HANDSHAKE || rr->type == SSL3_RT_ACK)
        && SSL_CONNECTION_IS_DTLS13(sc)) {
        sc->s3.tmp.record_epoch = rr->epoch;
        sc->s3.tmp.record_seq_num = rr->seq_num;
    }

    if (type == rr->type
        || (type == SSL3_RT_HANDSHAKE
            && ((!is_dtls13 && recvd_type != NULL && rr->type == SSL3_RT_CHANGE_CIPHER_SPEC)
                || (is_dtls13 && rr->type == SSL3_RT_ACK)))) {
        /*
         * SSL3_RT_APPLICATION_DATA or
         * SSL3_RT_HANDSHAKE or
         * SSL3_RT_CHANGE_CIPHER_SPEC or
         * SSL3_RT_ACK
         */
        /*
         * make sure that we are not getting application data when we are
         * doing a handshake for the first time
         */
        if (SSL_in_init(s) && (type == SSL3_RT_APPLICATION_DATA)
            && (SSL_IS_FIRST_HANDSHAKE(sc))) {
            SSLfatal(sc, SSL_AD_UNEXPECTED_MESSAGE,
                SSL_R_APP_DATA_IN_HANDSHAKE);
            return -1;
        }

        if (recvd_type != NULL)
            *recvd_type = rr->type;

        if (len == 0) {
            /*
             * Release a zero length record. This ensures multiple calls to
             * SSL_read() with a zero length buffer will eventually cause
             * SSL_pending() to report data as being available.
             */
            if (rr->length == 0 && !ssl_release_record(sc, rr, 0))
                return -1;
            return 0;
        }

        if (len > rr->length)
            n = rr->length;
        else
            n = len;

        memcpy(buf, &(rr->data[rr->off]), n);
        if (peek) {
            if (rr->length == 0 && !ssl_release_record(sc, rr, 0))
                return -1;
        } else {
            if (!ssl_release_record(sc, rr, n))
                return -1;
        }
#ifndef OPENSSL_NO_SCTP
        /*
         * We might had to delay a close_notify alert because of reordered
         * app data. If there was an alert and there is no message to read
         * anymore, finally set shutdown.
         */
        if (SSL_get_rbio(s) != NULL && BIO_dgram_is_sctp(SSL_get_rbio(s)) && sc->d1->shutdown_received
            && BIO_dgram_sctp_msg_waiting(SSL_get_rbio(s)) <= 0) {
            sc->shutdown |= SSL_RECEIVED_SHUTDOWN;
            return 0;
        }
#endif
        *readbytes = n;
        return 1;
    }

    /*
     * If we get here, then type != rr->type; if we have a handshake message,
     * then it was unexpected (Hello Request or Client Hello).
     */

    if (rr->type == SSL3_RT_ALERT) {
        unsigned int alert_level, alert_descr;
        const unsigned char *alert_bytes = rr->data + rr->off;
        PACKET alert;

        if (!PACKET_buf_init(&alert, alert_bytes, rr->length)
            || !PACKET_get_1(&alert, &alert_level)
            || !PACKET_get_1(&alert, &alert_descr)
            || PACKET_remaining(&alert) != 0) {
            SSLfatal(sc, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_INVALID_ALERT);
            return -1;
        }

        if (sc->msg_callback)
            sc->msg_callback(0, sc->version, SSL3_RT_ALERT, alert_bytes, 2, s,
                sc->msg_callback_arg);

        if (sc->info_callback != NULL)
            cb = sc->info_callback;
        else if (s->ctx->info_callback != NULL)
            cb = s->ctx->info_callback;

        if (cb != NULL) {
            j = (alert_level << 8) | alert_descr;
            cb(s, SSL_CB_READ_ALERT, j);
        }

        if ((!is_dtls13 && alert_level == SSL3_AL_WARNING)
            || (is_dtls13 && alert_descr == SSL_AD_USER_CANCELLED)) {
            sc->s3.warn_alert = alert_descr;
            if (!ssl_release_record(sc, rr, 0))
                return -1;

            sc->rlayer.alert_count++;
            if (sc->rlayer.alert_count == MAX_WARN_ALERT_COUNT) {
                SSLfatal(sc, SSL_AD_UNEXPECTED_MESSAGE,
                    SSL_R_TOO_MANY_WARN_ALERTS);
                return -1;
            }
        }

        /*
         * Apart from close_notify the only other warning alert in DTLSv1.3
         * is user_cancelled - which we just ignore.
         */
        if (is_dtls13 && alert_descr == SSL_AD_USER_CANCELLED) {
            goto start;
        } else if (alert_descr == SSL_AD_CLOSE_NOTIFY
            && (is_dtls13 || alert_level == SSL3_AL_WARNING)) {
#ifndef OPENSSL_NO_SCTP
            /*
             * With SCTP and streams the socket may deliver app data
             * after a close_notify alert. We have to check this first so
             * that nothing gets discarded.
             */
            if (SSL_get_rbio(s) != NULL && BIO_dgram_is_sctp(SSL_get_rbio(s)) && BIO_dgram_sctp_msg_waiting(SSL_get_rbio(s)) > 0) {
                sc->d1->shutdown_received = 1;
                sc->rwstate = SSL_READING;
                BIO_clear_retry_flags(SSL_get_rbio(s));
                BIO_set_retry_read(SSL_get_rbio(s));
                return -1;
            }
#endif
            sc->shutdown |= SSL_RECEIVED_SHUTDOWN;
            return 0;
        } else if (alert_level == SSL3_AL_FATAL || is_dtls13) {
            sc->rwstate = SSL_NOTHING;
            sc->s3.fatal_alert = alert_descr;
            SSLfatal_data(sc, SSL_AD_NO_ALERT,
                SSL_AD_REASON_OFFSET + alert_descr,
                "SSL alert number %d", alert_descr);
            sc->shutdown |= SSL_RECEIVED_SHUTDOWN;
            if (!ssl_release_record(sc, rr, 0))
                return -1;
            SSL_CTX_remove_session(sc->session_ctx, sc->session);
            return 0;
        } else if (alert_descr == SSL_AD_NO_RENEGOTIATION) {
            /*
             * This is a warning but we receive it if we requested
             * renegotiation and the peer denied it. Terminate with a fatal
             * alert because if the application tried to renegotiate it
             * presumably had a good reason and expects it to succeed. In
             * the future we might have a renegotiation where we don't care
             * if the peer refused it where we carry on.
             */
            SSLfatal(sc, SSL_AD_HANDSHAKE_FAILURE, SSL_R_NO_RENEGOTIATION);
            return -1;
        } else if (alert_level == SSL3_AL_WARNING) {
            /* We ignore any other warning alert in (D)TLSv1.2 and below */
            goto start;
        }

        SSLfatal(sc, SSL_AD_ILLEGAL_PARAMETER, SSL_R_UNKNOWN_ALERT_TYPE);
        return -1;
    }

    if (sc->shutdown & SSL_SENT_SHUTDOWN) { /* but we have not received a
                                             * shutdown */
        sc->rwstate = SSL_NOTHING;
        if (!ssl_release_record(sc, rr, 0))
            return -1;
        return 0;
    }

    if (rr->type == SSL3_RT_CHANGE_CIPHER_SPEC) {
        /*
         * We can't process a CCS now, because previous handshake messages
         * are still missing, so just drop it.
         */
        if (!ssl_release_record(sc, rr, 0))
            return -1;
        goto start;
    }

    /*
     * Unexpected handshake message (Client Hello, or protocol violation)
     */
    if (!ossl_statem_get_in_handshake(sc) && rr->type == SSL3_RT_HANDSHAKE && !in_early_data) {
        unsigned char msg_type;

        /*
         * This may just be a stale retransmit. Also sanity check that we have
         * at least enough record bytes for a message header.
         *
         * For DTLS 1.3, a record at exactly the previous read epoch is not
         * necessarily stale: it authenticated (see dtls_get_more_records()'s
         * retained prev_epoch_rl handling), but that alone doesn't prove it's
         * a retransmission. Let it through to the normal handshake-message
         * path below instead of discarding it here -- the sequence and epoch
         * checks there (dtls_record_from_retained_epoch(),
         * dtls_prev_epoch_allows_type()) are what actually decide whether it
         * can be acknowledged. This retained-epoch exception is for handshake
         * records only (rr->type == SSL3_RT_HANDSHAKE above); an ACK record
         * still has to be tied to the epoch that authenticated it before it
         * can touch d1->sent_messages, which dtls_process_ack() enforces
         * separately.
         */
        if ((rr->epoch != dtls1_get_epoch(sc, SSL3_CC_READ)
                && !(SSL_CONNECTION_IS_DTLS13(sc)
                    && dtls1_get_epoch(sc, SSL3_CC_READ) > 0
                    && rr->epoch == dtls1_get_epoch(sc, SSL3_CC_READ) - 1))
            || rr->length < DTLS1_HM_HEADER_LENGTH) {
            if (!ssl_release_record(sc, rr, 0))
                return -1;
            goto start;
        }

        msg_type = *rr->data;

        /*
         * If we are server, we may have a repeated FINISHED of the client
         * here, then retransmit our CCS and FINISHED.
         *
         * DTLS 1.3 has proper ACK records, so this DTLS 1.2-only fallback
         * (which infers loss from a bare repeated Finished and reacts by
         * blindly retransmitting our own flight) is skipped for it. A
         * repeated DTLS 1.3 Finished instead falls through to the normal
         * handshake-message path below, which ACKs a message it has
         * already fully processed without reprocessing it (see the
         * record_epoch check in statem_dtls.c).
         */
        if (!SSL_CONNECTION_IS_DTLS13(sc) && msg_type == SSL3_MT_FINISHED) {
            if (dtls1_check_timeout_num(sc) < 0) {
                /* SSLfatal) already called */
                return -1;
            }

            if (dtls1_retransmit_sent_messages(sc) <= 0) {
                /* Fail if we encountered a fatal error */
                if (ossl_statem_in_error(sc))
                    return -1;
            }
            if (!ssl_release_record(sc, rr, 0))
                return -1;
            if (!(sc->mode & SSL_MODE_AUTO_RETRY)) {
                if (!sc->rlayer.rrlmethod->unprocessed_read_pending(sc->rlayer.rrl)) {
                    /* no read-ahead left? */
                    BIO *bio;

                    sc->rwstate = SSL_READING;
                    bio = SSL_get_rbio(s);
                    /*
                     * A connection accepted by a DTLS listener has no read
                     * BIO of its own (see dtls_listener_create_conn_ssl());
                     * SSL_get_error() already reports SSL_ERROR_WANT_READ
                     * for SSL_READING on such a connection without needing
                     * one.
                     */
                    if (bio != NULL) {
                        BIO_clear_retry_flags(bio);
                        BIO_set_retry_read(bio);
                    }
                    return -1;
                }
            }
            goto start;
        }
    }

    if (!ossl_statem_get_in_handshake(sc)
        && (rr->type == SSL3_RT_HANDSHAKE || rr->type == SSL3_RT_ACK)) {

        /*
         * To get here we must be trying to read app data but found handshake
         * data. This can be because we are in early data and receive the rest
         * of the handshake.
         */

        /* We found handshake data, so we're going back into init */
        ossl_statem_set_in_init(sc, 1);

        i = sc->handshake_func(s);
        /* SSLfatal() called if appropriate */
        if (i < 0)
            return i;

        if (i == 0)
            return -1;

        /*
         * If we were actually trying to read early data and we found a
         * handshake message, then we don't want to continue to try and read
         * the application data any more. It won't be "early" now.
         */
        if (in_early_data)
            return -1;

        if (!(sc->mode & SSL_MODE_AUTO_RETRY)) {
            if (!sc->rlayer.rrlmethod->unprocessed_read_pending(sc->rlayer.rrl)) {
                /* no read-ahead left? */
                BIO *bio;
                /*
                 * In the case where we try to read application data, but we
                 * trigger an SSL handshake, we return -1 with the retry
                 * option set.  Otherwise renegotiation may cause nasty
                 * problems in the blocking world
                 */
                sc->rwstate = SSL_READING;
                bio = SSL_get_rbio(s);
                /*
                 * A connection accepted by a DTLS listener has no read BIO
                 * of its own (see dtls_listener_create_conn_ssl());
                 * SSL_get_error() already reports SSL_ERROR_WANT_READ for
                 * SSL_READING on such a connection without needing one.
                 */
                if (bio != NULL) {
                    BIO_clear_retry_flags(bio);
                    BIO_set_retry_read(bio);
                }
                return -1;
            }
        }
        goto start;
    }

    switch (rr->type) {
    default:
        SSLfatal(sc, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_UNEXPECTED_RECORD);
        return -1;
    case SSL3_RT_CHANGE_CIPHER_SPEC:
    case SSL3_RT_ALERT:
    case SSL3_RT_HANDSHAKE:
        /*
         * we already handled all of these, with the possible exception of
         * SSL3_RT_HANDSHAKE when ossl_statem_get_in_handshake(s) is true, but
         * that should not happen when type != rr->type
         */
        SSLfatal(sc, SSL_AD_UNEXPECTED_MESSAGE, ERR_R_INTERNAL_ERROR);
        return -1;

    case SSL3_RT_ACK:
        switch (sc->negotiated_version) {
        case DTLS1_3_VERSION:
            /* ACK should have been handled if DTLSv1.3 has been negotiated. */
            SSLfatal(sc, SSL_AD_UNEXPECTED_MESSAGE, ERR_R_INTERNAL_ERROR);
            return -1;

        case DTLS_ANY_VERSION:
            /*
             * This must be an ACK from a DTLSv1.3 server for a partial
             * ClientHello. We always send the full message again if the
             * ClientHello is not responded to with a ServerHello before the
             * timer runs out. Drop the record.
             */
            if (!ssl_release_record(sc, rr, 0))
                return -1;
            goto start;

        default:
            /*
             * If we receive an ACK record when we have negotiated a lower version
             * than DTLSv1.3 then we respond with an unexpected record fatal alert.
             */
            SSLfatal(sc, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_UNEXPECTED_RECORD);
            return -1;
        }

    case SSL3_RT_APPLICATION_DATA:
        /*
         * At this point, we were expecting handshake data, but have
         * application data.  If the library was running inside ssl3_read()
         * (i.e. in_read_app_data is set) and it makes sense to read
         * application data at this point (session renegotiation not yet
         * started), we will indulge it.
         */
        if (sc->s3.in_read_app_data && (sc->s3.total_renegotiations != 0) && ossl_statem_app_data_allowed(sc)) {
            sc->s3.in_read_app_data = 2;
            return -1;
        } else if (sc->version == DTLS1_3_VERSION) {
            /*
             * Let's let DTLS ACK and retransmits fix this problem.
             */
            ssl_release_record(sc, rr, 0);
            return -1;
        } else {
            SSLfatal(sc, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_UNEXPECTED_RECORD);
            return -1;
        }
    }
    /* not reached */
}

/*
 * Call this to write data in records of type 'type' It will return <= 0 if
 * not all data has been sent or non-blocking IO.
 */
int dtls1_write_bytes(SSL_CONNECTION *s, uint8_t type, const void *buf,
    size_t len, size_t *written)
{
    int i;

    if (!ossl_assert(len <= SSL3_RT_MAX_PLAIN_LENGTH)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return -1;
    }
    s->rwstate = SSL_NOTHING;
    i = do_dtls1_write(s, type, buf, len, written);
    return i;
}

int do_dtls1_write(SSL_CONNECTION *sc, uint8_t type, const unsigned char *buf,
    size_t len, size_t *written)
{
    int i;
    OSSL_RECORD_TEMPLATE tmpl;
    SSL *s = SSL_CONNECTION_GET_SSL(sc);
    int ret;

    /* If we have an alert to send, lets send it */
    if (sc->s3.alert_dispatch != SSL_ALERT_DISPATCH_NONE) {
        i = s->method->ssl_dispatch_alert(s);
        if (i <= 0)
            return i;
        /* if it went, fall through and send more stuff */
    }

    if (len == 0) {
        *written = 0;
        return 1;
    }

    if (len > ssl_get_max_send_fragment(sc)) {
        SSLfatal(sc, SSL_AD_INTERNAL_ERROR, SSL_R_EXCEEDS_MAX_FRAGMENT_SIZE);
        return 0;
    }

    tmpl.type = type;
    if (sc->version == DTLS1_3_VERSION)
        tmpl.version = DTLS1_2_VERSION;
    /*
     * Special case: for hello verify request, client version 1.0 and we
     * haven't decided which version to use yet send back using version 1.0
     * header: otherwise some clients will ignore it.
     */
    else if (s->method->version == DTLS_ANY_VERSION
        && sc->max_proto_version != DTLS1_BAD_VER)
        tmpl.version = DTLS1_VERSION;
    else
        tmpl.version = sc->version;
    tmpl.buf = buf;
    tmpl.buflen = len;

    ret = HANDLE_RLAYER_WRITE_RETURN(sc,
        sc->rlayer.wrlmethod->write_records(sc->rlayer.wrl, &tmpl, 1));

    if (ret > 0)
        *written = len;

    /*
     * Add record number to the buffered sent message
     */
    if (type == SSL3_RT_HANDSHAKE && ret > 0 && SSL_CONNECTION_IS_DTLS13(sc)) {
        pitem *item;
        unsigned char prio[8];
        dtls_sent_msg *sent_msg;
        DTLS1_RECORD_NUMBER *rec_num;

        dtls1_get_queue_priority(prio, sc->d1->w_msg.msg_seq, 0);
        item = pqueue_find(&sc->d1->sent_messages, prio);

        if (item == NULL)
            return ret;

        sent_msg = (dtls_sent_msg *)item->data;
        rec_num = dtls1_record_number_new(tmpl.epoch, tmpl.sequence_number,
            sc->d1->w_frag_off, sc->d1->w_frag_len);

        if (rec_num == NULL)
            return -1;

        ossl_list_record_number_insert_tail(&sent_msg->rec_nums, rec_num);
    }

    return ret;
}

int dtls1_increment_epoch(SSL_CONNECTION *s, int rw)
{
    if (rw & SSL3_CC_READ) {
        if (!SSL_CONNECTION_IS_DTLS13(s) && s->rlayer.d->r_conn_epoch == UINT16_MAX)
            return 0;

        s->rlayer.d->r_conn_epoch++;

        /*
         * We must not use any buffered messages received from the previous
         * epoch
         */
        dtls1_clear_received_buffer(s);

        if (s->rlayer.d->r_conn_epoch == 0)
            /* We've wrapped around, so clear the buffer just in case */
            return 0;
    } else {
        if (!SSL_CONNECTION_IS_DTLS13(s) && s->rlayer.d->w_conn_epoch == DTLS1_MAX_EPOCH)
            return 0;

        /*
         * RFC 9147 Section 8: sending implementations MUST NOT allow the
         * epoch to exceed 2^48-1.
         */
        if (SSL_CONNECTION_IS_DTLS13(s) && s->rlayer.d->w_conn_epoch == DTLS1_3_MAX_EPOCH)
            return 0;

        s->rlayer.d->w_conn_epoch++;
    }

    return 1;
}

uint64_t dtls1_get_epoch(SSL_CONNECTION *s, int rw)
{
    uint64_t epoch;

    if (rw & SSL3_CC_READ)
        epoch = s->rlayer.d->r_conn_epoch;
    else
        epoch = s->rlayer.d->w_conn_epoch;

    return epoch;
}
