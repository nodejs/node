/*
 * Copyright 2005-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/e_os.h"
#include "internal/e_winsock.h" /* struct timeval for DTLS_CTRL_GET_TIMEOUT */
#include <stdio.h>
#include <openssl/objects.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include "ssl_local.h"
#include "internal/time.h"
#include "internal/ssl_unwrap.h"
#include "internal/hashfunc.h"
#include "internal/dtls_record_rx.h"
#include "internal/dgram_demux.h"
#include "internal/dgram_conn_lookup.h"
#include "internal/rio_notifier.h"

static int dtls1_handshake_write(SSL_CONNECTION *s);
static const size_t dtls1_link_min_mtu = 256;
#ifndef OPENSSL_NO_DTLS
static OSSL_TIME dtls_listener_get_time_direct(DTLS_LISTENER *dl);
#endif

const SSL3_ENC_METHOD DTLSv1_enc_data = {
    tls1_setup_key_block,
    tls1_generate_master_secret,
    tls1_change_cipher_state,
    tls1_final_finish_mac,
    TLS_MD_CLIENT_FINISH_CONST, TLS_MD_CLIENT_FINISH_CONST_SIZE,
    TLS_MD_SERVER_FINISH_CONST, TLS_MD_SERVER_FINISH_CONST_SIZE,
    tls1_alert_code,
    tls1_export_keying_material,
    SSL_ENC_FLAG_DTLS,
    dtls1_set_handshake_header,
    dtls1_close_construct_packet,
    dtls1_handshake_write
};

const SSL3_ENC_METHOD DTLSv1_2_enc_data = {
    tls1_setup_key_block,
    tls1_generate_master_secret,
    tls1_change_cipher_state,
    tls1_final_finish_mac,
    TLS_MD_CLIENT_FINISH_CONST, TLS_MD_CLIENT_FINISH_CONST_SIZE,
    TLS_MD_SERVER_FINISH_CONST, TLS_MD_SERVER_FINISH_CONST_SIZE,
    tls1_alert_code,
    tls1_export_keying_material,
    SSL_ENC_FLAG_DTLS | SSL_ENC_FLAG_SIGALGS
        | SSL_ENC_FLAG_SHA256_PRF | SSL_ENC_FLAG_TLS1_2_CIPHERS,
    dtls1_set_handshake_header,
    dtls1_close_construct_packet,
    dtls1_handshake_write
};

const SSL3_ENC_METHOD DTLSv1_3_enc_data = {
    tls13_setup_key_block,
    tls13_generate_master_secret,
    tls13_change_cipher_state,
    tls13_final_finish_mac,
    TLS_MD_CLIENT_FINISH_CONST, TLS_MD_CLIENT_FINISH_CONST_SIZE,
    TLS_MD_SERVER_FINISH_CONST, TLS_MD_SERVER_FINISH_CONST_SIZE,
    tls13_alert_code,
    tls13_export_keying_material,
    SSL_ENC_FLAG_DTLS | SSL_ENC_FLAG_SIGALGS | SSL_ENC_FLAG_SHA256_PRF,
    dtls1_set_handshake_header,
    dtls1_close_construct_packet,
    dtls1_handshake_write
};

OSSL_TIME dtls1_default_timeout(void)
{
    /*
     * 2 hours, the 24 hours mentioned in the DTLSv1 spec is way too long for
     * http, the cache would over fill
     */
    return ossl_seconds2time(60 * 60 * 2);
}

int dtls1_new(SSL *ssl)
{
    DTLS1_STATE *d1;
    SSL_CONNECTION *s = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

    if (s == NULL)
        return 0;

    if (!DTLS_RECORD_LAYER_new(&s->rlayer)) {
        return 0;
    }

    if (!ssl3_new(ssl))
        return 0;
    if ((d1 = OPENSSL_zalloc(sizeof(*d1))) == NULL) {
        ssl3_free(ssl);
        return 0;
    }

    d1->hello_verify_request = SSL_HVR_NONE;

    s->d1 = d1;

    if (!ssl->method->ssl_clear(ssl))
        return 0;

    return 1;
}

static void dtls1_clear_queues(SSL_CONNECTION *s)
{
    dtls1_clear_received_buffer(s);
    dtls1_clear_sent_buffer(s, 0);
    ossl_list_record_number_elem_free(&s->d1->ack_rec_num);
}

void dtls1_clear_received_buffer(SSL_CONNECTION *s)
{
    pitem *item = NULL;
    hm_fragment *frag = NULL;
    pqueue *rcvd_messages = &s->d1->rcvd_messages;

    while ((item = pqueue_pop(rcvd_messages)) != NULL) {
        frag = (hm_fragment *)item->data;
        dtls1_hm_fragment_free(frag);
        pitem_free(item);
    }
    s->d1->has_change_cipher_spec = 0;
}

void ossl_list_record_number_elem_free(OSSL_LIST(record_number) * p_list)
{
    DTLS1_RECORD_NUMBER *p_elem;
    DTLS1_RECORD_NUMBER *p_elem_next = NULL;

    if (p_list != NULL)
        p_elem_next = ossl_list_record_number_head(p_list);

    while ((p_elem = p_elem_next) != NULL) {
        p_elem_next = ossl_list_record_number_next(p_elem_next);
        ossl_list_record_number_remove(p_list, p_elem);
        OPENSSL_free(p_elem);
    }
}

DTLS1_RECORD_NUMBER *dtls1_record_number_new(uint64_t epoch, uint64_t seqnum,
    size_t frag_off, size_t frag_len)
{
    DTLS1_RECORD_NUMBER *recnum = OPENSSL_zalloc(sizeof(*recnum));

    if (recnum != NULL) {
        recnum->epoch = epoch;
        recnum->seqnum = seqnum;
        recnum->frag_off = frag_off;
        recnum->frag_len = frag_len;
    }

    return recnum;
}

void dtls1_acknowledge_sent_buffer(SSL_CONNECTION *s, uint64_t before_epoch)
{
    pitem *item = NULL;
    piterator iter = pqueue_iterator(&s->d1->sent_messages);

    while ((item = pqueue_next(&iter)) != NULL) {
        dtls_sent_msg *sent_msg = (dtls_sent_msg *)item->data;
        DTLS1_RECORD_NUMBER *recnum;
        DTLS1_RECORD_NUMBER *recnum_next = ossl_list_record_number_head(&sent_msg->rec_nums);

        while ((recnum = recnum_next) != NULL) {
            recnum_next = ossl_list_record_number_next(recnum_next);

            if (recnum->epoch < before_epoch) {
                ossl_list_record_number_remove(&sent_msg->rec_nums, recnum);
                OPENSSL_free(recnum);
            }
        }
    }
}

void dtls1_clear_sent_buffer(SSL_CONNECTION *s, int keep_unacked_msgs)
{
    pitem *item = NULL;
    pqueue *remaining_sent_messages = pqueue_new();
    pqueue *sent_messages = &s->d1->sent_messages;

    while ((item = pqueue_pop(sent_messages)) != NULL) {
        dtls_sent_msg *sent_msg = (dtls_sent_msg *)item->data;
        unsigned char msg_type = sent_msg->msg_info.msg_type;
        unsigned char record_type = sent_msg->msg_info.record_type;

        if (SSL_CONNECTION_IS_DTLS13(s)
            && !ossl_list_record_number_is_empty(&sent_msg->rec_nums)
            && keep_unacked_msgs) {
            pqueue_insert(remaining_sent_messages, item);
            continue;
        }

        if (((!SSL_CONNECTION_IS_DTLS13(s) && record_type == SSL3_RT_CHANGE_CIPHER_SPEC)
                || (SSL_CONNECTION_IS_DTLS13(s)
                    && (msg_type == SSL3_MT_FINISHED
                        || msg_type == SSL3_MT_SERVER_HELLO
                        || msg_type == SSL3_MT_KEY_UPDATE)))
            && sent_msg->saved_retransmit_state.wrlmethod != NULL
            && s->rlayer.wrl != sent_msg->saved_retransmit_state.wrl) {
            /*
             * If we're freeing the CCS then we're done with the old wrl and it
             * can bee freed
             */
            sent_msg->saved_retransmit_state.wrlmethod->free(sent_msg->saved_retransmit_state.wrl);
        }

        dtls1_sent_msg_free(sent_msg);
        pitem_free(item);
    }

    if (SSL_CONNECTION_IS_DTLS13(s))
        while ((item = pqueue_pop(remaining_sent_messages)) != NULL)
            pqueue_insert(&s->d1->sent_messages, item);

    pqueue_free(remaining_sent_messages);
}

/*
 * Before RECORD_LAYER_clear() frees s->rlayer.wrl, null out any
 * saved_retransmit_state.wrl pointers in the sent_messages queue that
 * reference it.  This transfers ownership of that free exclusively to
 * RECORD_LAYER_clear and prevents dtls1_clear_sent_buffer from freeing
 * the same pointer a second time.  Entries with a different (older) wrl
 * pointer are left untouched and will be freed correctly later.
 */
void dtls1_clear_current_wrl_from_sent_buffer(SSL_CONNECTION *s)
{
    pitem *item;
    piterator iter = pqueue_iterator(&s->d1->sent_messages);

    while ((item = pqueue_next(&iter)) != NULL) {
        dtls_sent_msg *sent_msg = (dtls_sent_msg *)item->data;

        if (sent_msg->saved_retransmit_state.wrl == s->rlayer.wrl) {
            sent_msg->saved_retransmit_state.wrl = NULL;
            sent_msg->saved_retransmit_state.wrlmethod = NULL;
        }
    }
}

int dtls_any_sent_messages_are_missing_acknowledge(SSL_CONNECTION *s)
{
    pitem *item;
    piterator iter = pqueue_iterator(&s->d1->sent_messages);

    while ((item = pqueue_next(&iter)) != NULL) {
        dtls_sent_msg *msg = (dtls_sent_msg *)item->data;

        if (!ossl_list_record_number_is_empty(&msg->rec_nums))
            return 1;
    }

    return 0;
}

void dtls1_free(SSL *ssl)
{
    SSL_CONNECTION *s;

#ifndef OPENSSL_NO_DTLS
    if (IS_DTLS_LISTENER(ssl)) {
        ossl_dtls_listener_free(ssl);
        return;
    }
#endif

    s = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

    if (s == NULL)
        return;

#ifndef OPENSSL_NO_DTLS
    /*
     * If this connection was created by a listener, unregister it from the
     * listener's established_conns lookup table to prevent use-after-free.
     * The listener routes incoming packets to connections via this table,
     * so we must remove ourselves before freeing.
     */
    if (s->d1 != NULL && s->d1->listener != NULL)
        ossl_dtls_listener_unregister_established_conn(s->d1->listener,
            &s->d1->peer_addr);
#endif

    if (s->d1 != NULL)
        dtls1_clear_queues(s);

#ifndef OPENSSL_NO_DTLS
    if (s->d1 != NULL) {
        DTLS_RX *rx = s->d1->rx;

        if (s->listener_rx == rx)
            s->listener_rx = NULL;
        ossl_dtls_rx_free(rx);

        if (s->d1->listener != NULL)
            SSL_free(s->d1->listener);
    }
#endif

    DTLS_RECORD_LAYER_free(&s->rlayer);
    ssl3_free(ssl);
    OPENSSL_free(s->d1);
    s->d1 = NULL;
}

int dtls1_clear(SSL *ssl)
{
    size_t mtu;
    size_t link_mtu;
    SSL_CONNECTION *s;

#ifndef OPENSSL_NO_DTLS
    if (IS_DTLS_LISTENER(ssl))
        return 1;
#endif

    s = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

    if (s == NULL)
        return 0;

    DTLS_RECORD_LAYER_clear(&s->rlayer);

    if (s->d1) {
        DTLS_timer_cb timer_cb = s->d1->timer_cb;
#ifndef OPENSSL_NO_SOCK
        BIO_ADDR peer_addr = s->d1->peer_addr;
#endif
#ifndef OPENSSL_NO_DTLS
        DTLS_RX *rx = s->d1->rx;
        SSL *listener = s->d1->listener;
        OSSL_TIME created_at = s->d1->created_at;
        unsigned int req_blocking_mode = s->d1->req_blocking_mode;
#endif

        mtu = s->d1->mtu;
        link_mtu = s->d1->link_mtu;

        dtls1_clear_queues(s);

        memset(s->d1, 0, sizeof(*s->d1));

        /* Restore the timer callback from previous state */
        s->d1->timer_cb = timer_cb;

#ifndef OPENSSL_NO_SOCK
        /*
         * Restore peer address, DTLS_RX, listener, and created_at for
         * listener-created connections. These are set via
         * SSL_set1_initial_peer_addr(), ossl_dtls_rx_new(), and
         * dtls_listener_create_conn_ssl() before the handshake starts,
         * and must be preserved across SSL_clear().
         */
        s->d1->peer_addr = peer_addr;
#endif
#ifndef OPENSSL_NO_DTLS
        s->d1->rx = rx;
        s->d1->listener = listener;
        /*
         * The blocking mode is a property of the connection as the application
         * configured it, not of the handshake, so it survives a clear.
         */
        s->d1->req_blocking_mode = req_blocking_mode;
        s->d1->created_at = created_at;
#endif

        if (SSL_get_options(ssl) & SSL_OP_NO_QUERY_MTU) {
            s->d1->mtu = mtu;
            s->d1->link_mtu = link_mtu;
        }
    }

    if (!ssl3_clear(ssl))
        return 0;

    if (ssl->method->version == DTLS_ANY_VERSION)
        s->version = DTLS_MAX_VERSION_INTERNAL;
#ifndef OPENSSL_NO_DTLS1_METHOD
    else if (s->options & SSL_OP_CISCO_ANYCONNECT)
        s->client_version = s->version = DTLS1_BAD_VER;
#endif
    else
        s->version = ssl->method->version;

    return 1;
}

long dtls1_ctrl(SSL *ssl, int cmd, long larg, void *parg)
{
    int ret = 0;
    OSSL_TIME t;
    SSL_CONNECTION *s;

    if (IS_DTLS_LISTENER(ssl))
        return 0;

    s = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

    if (s == NULL)
        return 0;

    switch (cmd) {
    case DTLS_CTRL_GET_TIMEOUT:
        if (dtls1_get_timeout(s, &t)) {
            *(struct timeval *)parg = ossl_time_to_timeval(t);
            ret = 1;
        }
        break;
    case DTLS_CTRL_HANDLE_TIMEOUT:
        ret = dtls1_handle_timeout(s);
        break;
    case DTLS_CTRL_SET_LINK_MTU:
        if (larg < (long)dtls1_link_min_mtu)
            return 0;
        s->d1->link_mtu = larg;
        return 1;
    case DTLS_CTRL_GET_LINK_MIN_MTU:
        return (long)dtls1_link_min_mtu;
    case SSL_CTRL_SET_MTU:
        /*
         *  We may not have a BIO set yet so can't call dtls1_min_mtu()
         *  We'll have to make do with dtls1_link_min_mtu and max overhead
         */
        if (larg < (long)dtls1_link_min_mtu - DTLS1_MAX_MTU_OVERHEAD)
            return 0;
        s->d1->mtu = larg;
        return larg;
    default:
        ret = ssl3_ctrl(ssl, cmd, larg, parg);
        break;
    }
    return ret;
}

static void dtls1_bio_set_next_timeout(BIO *bio, const DTLS1_STATE *d1)
{
    struct timeval tv = ossl_time_to_timeval(d1->next_timeout);

    BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT, 0, &tv);
}

void dtls1_start_timer(SSL_CONNECTION *s)
{
    OSSL_TIME duration;
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);

#ifndef OPENSSL_NO_SCTP
    /* Disable timer for SCTP */
    if (SSL_get_wbio(ssl) != NULL && BIO_dgram_is_sctp(SSL_get_wbio(ssl))) {
        s->d1->next_timeout = ossl_time_zero();
        return;
    }
#endif

    /*
     * If timer is not set, initialize duration with 1 second or
     * a user-specified value if the timer callback is installed.
     */
    if (ossl_time_is_zero(s->d1->next_timeout)) {
        if (s->d1->timer_cb != NULL)
            s->d1->timeout_duration_us = s->d1->timer_cb(ssl, 0);
        else
            s->d1->timeout_duration_us = 1000000;
    }

    /* Set timeout to current time plus duration */
    duration = ossl_us2time(s->d1->timeout_duration_us);
    s->d1->next_timeout = ossl_time_add(ossl_time_now(), duration);

    /* set s->d1->next_timeout into ssl->rbio interface */
    dtls1_bio_set_next_timeout(SSL_get_rbio(ssl), s->d1);
}

int dtls1_get_timeout(const SSL_CONNECTION *s, OSSL_TIME *timeleft)
{
    OSSL_TIME timenow;

    /* If no timeout is set, just return NULL */
    if (ossl_time_is_zero(s->d1->next_timeout))
        return 0;

    /* Get current time */
    timenow = ossl_time_now();

    /*
     * If timer already expired or if remaining time is less than 15 ms,
     * set it to 0 to prevent issues because of small divergences with
     * socket timeouts.
     */
    *timeleft = ossl_time_subtract(s->d1->next_timeout, timenow);
    if (ossl_time_compare(*timeleft, ossl_ms2time(15)) <= 0)
        *timeleft = ossl_time_zero();
    return 1;
}

int dtls1_is_timer_expired(SSL_CONNECTION *s)
{
    OSSL_TIME timeleft;

    /* Get time left until timeout, return false if no timer running */
    if (!dtls1_get_timeout(s, &timeleft))
        return 0;

    /* Return false if timer is not expired yet */
    if (!ossl_time_is_zero(timeleft))
        return 0;

    /* Timer expired, so return true */
    return 1;
}

static void dtls1_double_timeout(SSL_CONNECTION *s)
{
    s->d1->timeout_duration_us *= 2;
    if (s->d1->timeout_duration_us > 60000000)
        s->d1->timeout_duration_us = 60000000;
}

void dtls1_stop_timer(SSL_CONNECTION *s)
{
    /* Reset everything */
    s->d1->timeout_num_alerts = 0;
    s->d1->next_timeout = ossl_time_zero();
    s->d1->timeout_duration_us = 1000000;
    dtls1_bio_set_next_timeout(s->rbio, s->d1);
    /* Clear retransmission buffer */
    dtls1_clear_sent_buffer(s, 0);
}

int dtls1_check_timeout_num(SSL_CONNECTION *s)
{
    size_t mtu;
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);

    s->d1->timeout_num_alerts++;

    /* Reduce MTU after 2 unsuccessful retransmissions */
    if (s->d1->timeout_num_alerts > 2
        && !(SSL_get_options(ssl) & SSL_OP_NO_QUERY_MTU)) {
        mtu = BIO_ctrl(SSL_get_wbio(ssl), BIO_CTRL_DGRAM_GET_FALLBACK_MTU, 0, NULL);
        if (mtu < s->d1->mtu)
            s->d1->mtu = mtu;
    }

    if (s->d1->timeout_num_alerts > DTLS1_TMO_ALERT_COUNT) {
        /* fail the connection, enough alerts have been sent */
        SSLfatal(s, SSL_AD_NO_ALERT, SSL_R_READ_TIMEOUT_EXPIRED);
        return -1;
    }

    return 0;
}

int dtls1_handle_timeout(SSL_CONNECTION *s)
{
    /* if no timer is expired, don't do anything */
    if (!dtls1_is_timer_expired(s)) {
        return 0;
    }

    if (s->d1->timer_cb != NULL)
        s->d1->timeout_duration_us = s->d1->timer_cb(SSL_CONNECTION_GET_USER_SSL(s),
            s->d1->timeout_duration_us);
    else
        dtls1_double_timeout(s);

    if (dtls1_check_timeout_num(s) < 0) {
        /*
         * SSLfatal() already called, so the connection is finished. Stop the
         * timer rather than returning with next_timeout left in the past:
         * nothing will re-arm or clear it from here, so DTLSv1_get_timeout()
         * would report "due now" for ever and spin any caller which waits on
         * it.
         */
        dtls1_stop_timer(s);
        return -1;
    }

    dtls1_start_timer(s);

    /*
     * If write_state is anything other than WRITE_STATE_TRANSITION, a write
     * is still parked mid-flight (WANT_WRITE) from a previous call into the
     * state machine - the current flight hasn't actually finished going out
     * yet, so there's nothing valid to retransmit. Retransmitting anyway
     * would reconstruct an already-sent message from the retransmit queue
     * into s->init_buf/s->init_off/s->init_num/s->d1->w_msg - the same
     * fields the parked write is still using - corrupting that write's
     * state out from under it. Leave it alone and let the next
     * SSL_read()/SSL_write()/SSL_accept()/SSL_connect() call resume the
     * parked write normally instead.
     */
    if (s->statem.state == MSG_FLOW_WRITING
        && s->statem.write_state != WRITE_STATE_TRANSITION)
        return 0;

    /* Calls SSLfatal() if required */
    return dtls1_retransmit_sent_messages(s);
}

#define LISTEN_SUCCESS 2
#define LISTEN_SEND_VERIFY_REQUEST 1

#ifndef OPENSSL_NO_SOCK
int DTLSv1_listen(SSL *ssl, BIO_ADDR *client)
{
    int next, n, ret = 0;
    unsigned char cookie[DTLS1_COOKIE_LENGTH];
    unsigned char seq[SEQ_NUM_SIZE];
    const unsigned char *data;
    unsigned char *buf = NULL, *wbuf;
    size_t fragoff, fraglen, msglen;
    uint64_t record_sequence = 0;
    unsigned int rectype, versmajor, versminor, msgseq, msgtype, clientvers, cookielen;
    BIO *rbio, *wbio;
    BIO_ADDR *tmpclient = NULL;
    PACKET pkt, msgpkt, msgpayload, session, cookiepkt;
    SSL_CONNECTION *s = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

    if (s == NULL)
        return -1;

    if (s->handshake_func == NULL) {
        /* Not properly initialized yet */
        SSL_set_accept_state(ssl);
    }

    /* Ensure there is no state left over from a previous invocation */
    if (!SSL_clear(ssl))
        return -1;

    ERR_clear_error();

    rbio = SSL_get_rbio(ssl);
    wbio = SSL_get_wbio(ssl);

    if (!rbio || !wbio) {
        ERR_raise(ERR_LIB_SSL, SSL_R_BIO_NOT_SET);
        return -1;
    }

    /*
     * Note: This check deliberately excludes DTLS1_BAD_VER because that version
     * requires the MAC to be calculated *including* the first ClientHello
     * (without the cookie). Since DTLSv1_listen is stateless that cannot be
     * supported. DTLS1_BAD_VER must use cookies in a stateful manner (e.g. via
     * SSL_accept)
     */
    if ((s->version & 0xff00) != (DTLS1_VERSION & 0xff00)) {
        ERR_raise(ERR_LIB_SSL, SSL_R_UNSUPPORTED_SSL_VERSION);
        return -1;
    }

    /*
     * DTLSv1_listen() only supports the legacy HelloVerifyRequest mechanism
     * which is not used in DTLS 1.3. For DTLS 1.3, use the SSL_new_listener()
     * API instead which supports HelloRetryRequest with cookies.
     *
     * If the SSL object is configured for DTLS 1.3 only (both min and max
     * are set to DTLS 1.3), we must fail since there's no room to downgrade.
     * Otherwise, if max allows DTLS 1.3, we clamp it down to DTLS 1.2 so
     * that the handshake will use HelloVerifyRequest.
     */
    if (SSL_CONNECTION_IS_DTLS(s)) {
        int min_version = s->min_proto_version;
        int max_version = s->max_proto_version;

        /*
         * Check if configured for DTLS 1.3 only - this is not supported.
         * min_proto_version of 0 means "use default" which includes older versions,
         * so only fail if min is explicitly set to DTLS 1.3.
         */
        if (min_version == DTLS1_3_VERSION
            && (max_version == 0 || max_version == DTLS1_3_VERSION)) {
            ERR_raise(ERR_LIB_SSL, SSL_R_UNSUPPORTED_SSL_VERSION);
            return -1;
        }

        /* max_proto_version of 0 means "use default" which could include 1.3 */
        if (max_version == 0 || DTLS_VERSION_GE(max_version, DTLS1_3_VERSION)) {
            if (!SSL_set_max_proto_version(ssl, DTLS1_2_VERSION)) {
                ERR_raise(ERR_LIB_SSL, SSL_R_UNSUPPORTED_SSL_VERSION);
                return -1;
            }
        }
    }

    buf = OPENSSL_malloc(DTLS1_RT_HEADER_LENGTH + SSL3_RT_MAX_PLAIN_LENGTH);
    if (buf == NULL)
        return -1;
    wbuf = OPENSSL_malloc(DTLS1_RT_HEADER_LENGTH + SSL3_RT_MAX_PLAIN_LENGTH);
    if (wbuf == NULL) {
        OPENSSL_free(buf);
        return -1;
    }

    do {
        /* Get a packet */

        clear_sys_error();
        n = BIO_read(rbio, buf, SSL3_RT_MAX_PLAIN_LENGTH + DTLS1_RT_HEADER_LENGTH);
        if (n <= 0) {
            if (BIO_should_retry(rbio)) {
                /* Non-blocking IO */
                goto end;
            }
            ret = -1;
            goto end;
        }

        if (!PACKET_buf_init(&pkt, buf, n)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            ret = -1;
            goto end;
        }

        /*
         * Parse the received record. If there are any problems with it we just
         * dump it - with no alert. RFC6347 says this "Unlike TLS, DTLS is
         * resilient in the face of invalid records (e.g., invalid formatting,
         * length, MAC, etc.).  In general, invalid records SHOULD be silently
         * discarded, thus preserving the association; however, an error MAY be
         * logged for diagnostic purposes."
         */

        /* this packet contained a partial record, dump it */
        if (n < DTLS1_RT_HEADER_LENGTH) {
            ERR_raise(ERR_LIB_SSL, SSL_R_RECORD_TOO_SMALL);
            goto end;
        }

        /* Get the record header */
        if (!PACKET_get_1(&pkt, &rectype)
            || !PACKET_get_1(&pkt, &versmajor)
            || !PACKET_get_1(&pkt, &versminor)) {
            ERR_raise(ERR_LIB_SSL, SSL_R_LENGTH_MISMATCH);
            goto end;
        }

        if (s->msg_callback)
            s->msg_callback(0, (versmajor << 8) | versminor, SSL3_RT_HEADER, buf,
                DTLS1_RT_HEADER_LENGTH, ssl, s->msg_callback_arg);

        if (rectype != SSL3_RT_HANDSHAKE) {
            ERR_raise(ERR_LIB_SSL, SSL_R_UNEXPECTED_MESSAGE);
            goto end;
        }

        /*
         * Check record version number. We only check that the major version is
         * the same.
         */
        if (versmajor != DTLS1_VERSION_MAJOR) {
            ERR_raise(ERR_LIB_SSL, SSL_R_BAD_PROTOCOL_VERSION_NUMBER);
            goto end;
        }

        /* Save the sequence number: 64 bits, with top 2 bytes = epoch */
        if (!PACKET_copy_bytes(&pkt, seq, SEQ_NUM_SIZE)
            || !PACKET_get_length_prefixed_2(&pkt, &msgpkt)) {
            ERR_raise(ERR_LIB_SSL, SSL_R_LENGTH_MISMATCH);
            goto end;
        }
        /*
         * We allow data remaining at the end of the packet because there could
         * be a second record (but we ignore it)
         */

        /* This is an initial ClientHello so the epoch has to be 0 */
        if (seq[0] != 0 || seq[1] != 0) {
            ERR_raise(ERR_LIB_SSL, SSL_R_UNEXPECTED_MESSAGE);
            goto end;
        }
        record_sequence = ((uint64_t)seq[2]) << 40;
        record_sequence |= ((uint64_t)seq[3]) << 32;
        record_sequence |= ((uint64_t)seq[4]) << 24;
        record_sequence |= ((uint64_t)seq[5]) << 16;
        record_sequence |= ((uint64_t)seq[6]) << 8;
        record_sequence |= ((uint64_t)seq[7]);

        /* Get a pointer to the raw message for the later callback */
        data = PACKET_data(&msgpkt);

        /* Finished processing the record header, now process the message */
        if (!PACKET_get_1(&msgpkt, &msgtype)
            || !PACKET_get_net_3_len(&msgpkt, &msglen)
            || !PACKET_get_net_2(&msgpkt, &msgseq)
            || !PACKET_get_net_3_len(&msgpkt, &fragoff)
            || !PACKET_get_net_3_len(&msgpkt, &fraglen)
            || !PACKET_get_sub_packet(&msgpkt, &msgpayload, fraglen)
            || PACKET_remaining(&msgpkt) != 0) {
            ERR_raise(ERR_LIB_SSL, SSL_R_LENGTH_MISMATCH);
            goto end;
        }

        if (msgtype != SSL3_MT_CLIENT_HELLO) {
            ERR_raise(ERR_LIB_SSL, SSL_R_UNEXPECTED_MESSAGE);
            goto end;
        }

        /* Message sequence number can only be 0 or 1 */
        if (msgseq > 1) {
            ERR_raise(ERR_LIB_SSL, SSL_R_INVALID_SEQUENCE_NUMBER);
            goto end;
        }

        /*
         * We don't support fragment reassembly for ClientHellos whilst
         * listening because that would require server side state (which is
         * against the whole point of the ClientHello/HelloVerifyRequest
         * mechanism). Instead we only look at the first ClientHello fragment
         * and require that the cookie must be contained within it.
         */
        if (fragoff != 0 || fraglen > msglen) {
            /* Non initial ClientHello fragment (or bad fragment) */
            ERR_raise(ERR_LIB_SSL, SSL_R_FRAGMENTED_CLIENT_HELLO);
            goto end;
        }

        if (s->msg_callback)
            s->msg_callback(0, s->version, SSL3_RT_HANDSHAKE, data,
                fraglen + DTLS1_HM_HEADER_LENGTH, ssl,
                s->msg_callback_arg);

        if (!PACKET_get_net_2(&msgpayload, &clientvers)) {
            ERR_raise(ERR_LIB_SSL, SSL_R_LENGTH_MISMATCH);
            goto end;
        }

        /*
         * Verify client version is supported
         */
        if (DTLS_VERSION_LT(clientvers, (unsigned int)ssl->method->version) && ssl->method->version != DTLS_ANY_VERSION) {
            ERR_raise(ERR_LIB_SSL, SSL_R_WRONG_VERSION_NUMBER);
            goto end;
        }

        if (!PACKET_forward(&msgpayload, SSL3_RANDOM_SIZE)
            || !PACKET_get_length_prefixed_1(&msgpayload, &session)
            || !PACKET_get_length_prefixed_1(&msgpayload, &cookiepkt)) {
            /*
             * Could be malformed or the cookie does not fit within the initial
             * ClientHello fragment. Either way we can't handle it.
             */
            ERR_raise(ERR_LIB_SSL, SSL_R_LENGTH_MISMATCH);
            goto end;
        }

        /*
         * Check if we have a cookie or not. If not we need to send a
         * HelloVerifyRequest.
         */
        if (PACKET_remaining(&cookiepkt) == 0) {
            next = LISTEN_SEND_VERIFY_REQUEST;
        } else {
            /*
             * We have a cookie, so lets check it.
             */
            if (ssl->ctx->app_verify_cookie_cb == NULL) {
                ERR_raise(ERR_LIB_SSL, SSL_R_NO_VERIFY_COOKIE_CALLBACK);
                /* This is fatal */
                ret = -1;
                goto end;
            }
            if (ssl->ctx->app_verify_cookie_cb(ssl, PACKET_data(&cookiepkt),
                    (unsigned int)PACKET_remaining(&cookiepkt))
                == 0) {
                /*
                 * We treat invalid cookies in the same was as no cookie as
                 * per RFC6347
                 */
                next = LISTEN_SEND_VERIFY_REQUEST;
            } else {
                /* Cookie verification succeeded */
                next = LISTEN_SUCCESS;
            }
        }

        if (next == LISTEN_SEND_VERIFY_REQUEST) {
            WPACKET wpkt;
            unsigned int version;
            size_t wreclen;

            /*
             * There was no cookie in the ClientHello so we need to send a
             * HelloVerifyRequest. If this fails we do not worry about trying
             * to resend, we just drop it.
             */

            /* Generate the cookie */
            if (ssl->ctx->app_gen_cookie_cb == NULL || ssl->ctx->app_gen_cookie_cb(ssl, cookie, &cookielen) == 0 || cookielen > 255) {
                ERR_raise(ERR_LIB_SSL, SSL_R_COOKIE_GEN_CALLBACK_FAILURE);
                /* This is fatal */
                ret = -1;
                goto end;
            }

            /*
             * Special case: for hello verify request, client version 1.0 and we
             * haven't decided which version to use yet send back using version
             * 1.0 header: otherwise some clients will ignore it.
             */
            version = (ssl->method->version == DTLS_ANY_VERSION) ? DTLS1_VERSION
                                                                 : s->version;

            /* Construct the record and message headers */
            if (!WPACKET_init_static_len(&wpkt,
                    wbuf,
                    ssl_get_max_send_fragment(s)
                        + DTLS1_RT_HEADER_LENGTH,
                    0)
                || !WPACKET_put_bytes_u8(&wpkt, SSL3_RT_HANDSHAKE)
                || !WPACKET_put_bytes_u16(&wpkt, version)
                /*
                 * Record sequence number is always the same as in the
                 * received ClientHello
                 */
                || !WPACKET_memcpy(&wpkt, seq, SEQ_NUM_SIZE)
                /* End of record, start sub packet for message */
                || !WPACKET_start_sub_packet_u16(&wpkt)
                /* Message type */
                || !WPACKET_put_bytes_u8(&wpkt,
                    DTLS1_MT_HELLO_VERIFY_REQUEST)
                /*
                 * Message length - doesn't follow normal TLS convention:
                 * the length isn't the last thing in the message header.
                 * We'll need to fill this in later when we know the
                 * length. Set it to zero for now
                 */
                || !WPACKET_put_bytes_u24(&wpkt, 0)
                /*
                 * Message sequence number is always 0 for a
                 * HelloVerifyRequest
                 */
                || !WPACKET_put_bytes_u16(&wpkt, 0)
                /*
                 * We never fragment a HelloVerifyRequest, so fragment
                 * offset is 0
                 */
                || !WPACKET_put_bytes_u24(&wpkt, 0)
                /*
                 * Fragment length is the same as message length, but
                 * this *is* the last thing in the message header so we
                 * can just start a sub-packet. No need to come back
                 * later for this one.
                 */
                || !WPACKET_start_sub_packet_u24(&wpkt)
                /* Create the actual HelloVerifyRequest body */
                || !dtls_raw_hello_verify_request(&wpkt, cookie, cookielen)
                /* Close message body */
                || !WPACKET_close(&wpkt)
                /* Close record body */
                || !WPACKET_close(&wpkt)
                || !WPACKET_get_total_written(&wpkt, &wreclen)
                || !WPACKET_finish(&wpkt)) {
                ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
                WPACKET_cleanup(&wpkt);
                /* This is fatal */
                ret = -1;
                goto end;
            }

            /*
             * Fix up the message len in the message header. Its the same as the
             * fragment len which has been filled in by WPACKET, so just copy
             * that. Destination for the message len is after the record header
             * plus one byte for the message content type. The source is the
             * last 3 bytes of the message header
             */
            memcpy(&wbuf[DTLS1_RT_HEADER_LENGTH + 1],
                &wbuf[DTLS1_RT_HEADER_LENGTH + DTLS1_HM_HEADER_LENGTH - 3],
                3);

            if (s->msg_callback) {
                /* Report the outgoing DTLS record header */
                s->msg_callback(1, (int)version, SSL3_RT_HEADER,
                    wbuf, DTLS1_RT_HEADER_LENGTH,
                    ssl, s->msg_callback_arg);
                /* Report the HelloVerifyRequest handshake message */
                s->msg_callback(1, (int)version, SSL3_RT_HANDSHAKE,
                    wbuf + DTLS1_RT_HEADER_LENGTH,
                    wreclen - DTLS1_RT_HEADER_LENGTH,
                    ssl, s->msg_callback_arg);
            }

            if ((tmpclient = BIO_ADDR_new()) == NULL) {
                ERR_raise(ERR_LIB_SSL, ERR_R_BIO_LIB);
                goto end;
            }

            /*
             * This is unnecessary if rbio and wbio are one and the same - but
             * maybe they're not. We ignore errors here - some BIOs do not
             * support this.
             */
            if (BIO_dgram_get_peer(rbio, tmpclient) > 0) {
                (void)BIO_dgram_set_peer(wbio, tmpclient);
            }
            BIO_ADDR_free(tmpclient);
            tmpclient = NULL;

            if (BIO_write(wbio, wbuf, (int)wreclen) < (int)wreclen) {
                if (BIO_should_retry(wbio)) {
                    /*
                     * Non-blocking IO...but we're stateless, so we're just
                     * going to drop this packet.
                     */
                    goto end;
                }
                ret = -1;
                goto end;
            }

            if (BIO_flush(wbio) <= 0) {
                if (BIO_should_retry(wbio)) {
                    /*
                     * Non-blocking IO...but we're stateless, so we're just
                     * going to drop this packet.
                     */
                    goto end;
                }
                ret = -1;
                goto end;
            }
        }
    } while (next != LISTEN_SUCCESS);

    /*
     * Set expected sequence numbers to continue the handshake.
     */
    s->d1->handshake_read_seq = 1;
    s->d1->handshake_write_seq = 1;
    s->d1->next_handshake_write_seq = 1;
    if (s->rlayer.wrlmethod->set_sequence == NULL
        || !s->rlayer.wrlmethod->set_sequence(s->rlayer.wrl,
            record_sequence)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        ret = -1;
        goto end;
    }

    /*
     * We are doing cookie exchange, so make sure we set that option in the
     * SSL object
     */
    SSL_set_options(ssl, SSL_OP_COOKIE_EXCHANGE);

    /*
     * Tell the state machine that we've done the initial hello verify
     * exchange
     */
    ossl_statem_set_hello_verify_done(s);

    /*
     * Some BIOs may not support this. If we fail we clear the client address
     */
    if (BIO_dgram_get_peer(rbio, client) <= 0)
        BIO_ADDR_clear(client);

    /* Buffer the record for use by the record layer */
    if (BIO_write(s->rlayer.rrlnext, buf, n) != n) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        ret = -1;
        goto end;
    }

    /*
     * Reset the record layer - but this time we can use the record we just
     * buffered in s->rlayer.rrlnext
     */
    if (!ssl_set_new_record_layer(s, DTLS_ANY_VERSION,
            OSSL_RECORD_DIRECTION_READ,
            OSSL_RECORD_PROTECTION_LEVEL_NONE, NULL, 0,
            NULL, NULL, 0, NULL, 0, NULL, 0, NULL, NULL,
            0, NID_undef, NULL, NULL, NULL)) {
        /* SSLfatal already called */
        ret = -1;
        goto end;
    }

    ret = 1;
end:
    BIO_ADDR_free(tmpclient);
    OPENSSL_free(buf);
    OPENSSL_free(wbuf);
    return ret;
}
#endif

static int dtls1_handshake_write(SSL_CONNECTION *s)
{
    return dtls1_do_write(s, SSL3_RT_HANDSHAKE);
}

int dtls1_shutdown(SSL *s)
{
    int ret;
#ifndef OPENSSL_NO_SCTP
    BIO *wbio;
    SSL_CONNECTION *sc = SSL_CONNECTION_FROM_SSL_ONLY(s);

    if (sc == NULL)
        return -1;

    wbio = SSL_get_wbio(s);
    if (wbio != NULL && BIO_dgram_is_sctp(wbio) && !(sc->shutdown & SSL_SENT_SHUTDOWN)) {
        ret = BIO_dgram_sctp_wait_for_dry(wbio);
        if (ret < 0)
            return -1;

        if (ret == 0)
            BIO_ctrl(SSL_get_wbio(s), BIO_CTRL_DGRAM_SCTP_SAVE_SHUTDOWN, 1,
                NULL);
    }
#endif
    ret = ssl3_shutdown(s);
#ifndef OPENSSL_NO_SCTP
    BIO_ctrl(SSL_get_wbio(s), BIO_CTRL_DGRAM_SCTP_SAVE_SHUTDOWN, 0, NULL);
#endif
    return ret;
}

int dtls1_query_mtu(SSL_CONNECTION *s)
{
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);

    if (s->d1->link_mtu) {
        s->d1->mtu = s->d1->link_mtu - BIO_dgram_get_mtu_overhead(SSL_get_wbio(ssl));
        s->d1->link_mtu = 0;
    }

    /* AHA!  Figure out the MTU, and stick to the right size */
    if (s->d1->mtu < dtls1_min_mtu(s)) {
        if (!(SSL_get_options(ssl) & SSL_OP_NO_QUERY_MTU)) {
            s->d1->mtu = BIO_ctrl(SSL_get_wbio(ssl), BIO_CTRL_DGRAM_QUERY_MTU, 0, NULL);

            /*
             * I've seen the kernel return bogus numbers when it doesn't know
             * (initial write), so just make sure we have a reasonable number
             */
            if (s->d1->mtu < dtls1_min_mtu(s)) {
                /* Set to min mtu */
                s->d1->mtu = dtls1_min_mtu(s);
                BIO_ctrl(SSL_get_wbio(ssl), BIO_CTRL_DGRAM_SET_MTU,
                    (long)s->d1->mtu, NULL);
            }
        } else
            return 0;
    }
    return 1;
}

size_t dtls1_min_mtu(SSL_CONNECTION *s)
{
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);

    return dtls1_link_min_mtu - BIO_dgram_get_mtu_overhead(SSL_get_wbio(ssl));
}

size_t DTLS_get_data_mtu(const SSL *ssl)
{
    size_t mac_overhead, int_overhead, blocksize, ext_overhead, rechdrlen = 0;
    const SSL_CIPHER *ciph = SSL_get_current_cipher(ssl);
    size_t mtu;
    const SSL_CONNECTION *s = SSL_CONNECTION_FROM_CONST_SSL_ONLY(ssl);

    if (s == NULL)
        return 0;

    mtu = s->d1->mtu;

    if (ciph == NULL)
        return 0;

    if (!ssl_cipher_get_overhead(ciph, SSL_version(ssl), &mac_overhead,
            &int_overhead, &blocksize, &ext_overhead))
        return 0;

    if (SSL_READ_ETM(s))
        ext_overhead += mac_overhead;
    else
        int_overhead += mac_overhead;

    if (SSL_version(ssl) == DTLS1_3_VERSION) {
        switch (SSL_get_state(ssl)) {
        case TLS_ST_BEFORE:
        case DTLS_ST_CR_HELLO_VERIFY_REQUEST:
        case TLS_ST_CR_SRVR_HELLO:
        case TLS_ST_CW_CLNT_HELLO:
        case TLS_ST_CW_COMP_CERT:
        case TLS_ST_CW_KEY_EXCH:
        case TLS_ST_SW_HELLO_REQ:
        case TLS_ST_SR_CLNT_HELLO:
        case DTLS_ST_SW_HELLO_VERIFY_REQUEST:
        case TLS_ST_SW_SRVR_HELLO:
        case TLS_ST_CR_HELLO_REQ:
            rechdrlen = DTLS1_RT_HEADER_LENGTH;
            break;
        default:
            rechdrlen = DTLS13_UNI_HDR_FIXED_LENGTH;
            break;
        }

        /* Added record type at the end of the data */
        int_overhead++;
    } else {
        rechdrlen = DTLS1_RT_HEADER_LENGTH;
    }

    /* Subtract external overhead (e.g. IV/nonce, separate MAC) */
    if (ext_overhead + rechdrlen >= mtu)
        return 0;
    mtu -= ext_overhead + rechdrlen;

    /* Round encrypted payload down to cipher block size (for CBC etc.)
     * No check for overflow since 'mtu % blocksize' cannot exceed mtu. */
    if (blocksize)
        mtu -= (mtu % blocksize);

    /* Subtract internal overhead (e.g. CBC padding len byte) */
    if (int_overhead >= mtu)
        return 0;
    mtu -= int_overhead;

    return mtu;
}

void DTLS_set_timer_cb(SSL *ssl, DTLS_timer_cb cb)
{
    SSL_CONNECTION *s = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

    if (s == NULL)
        return;

    s->d1->timer_cb = cb;
}

#if !defined(OPENSSL_NO_DTLS) && !defined(OPENSSL_NO_SOCK)
/*
 * dtls_listener_connection_free - free an SSL connection owned by the listener.
 *
 * This function is used to free SSL connections that are in the listener's
 * pending_conns or incoming_connections queues. These connections are owned
 * by the listener, NOT by the application.
 *
 * Connections in pending_conns and incoming_connections do NOT hold a reference
 * to the listener, even though sc->d1->listener points to it. This is intentional:
 * if these connections held a reference to the listener, the listener's reference
 * count would never reach zero, and ossl_dtls_listener_free() would never be
 * called to clean up the pending/incoming connections - creating a circular
 * dependency.
 *
 * Only when a connection is returned to the application via SSL_accept_connection()
 * does it take a reference on the listener. At that point, ownership transfers
 * to the application, and the normal SSL_free() path is used.
 *
 * The assert on ssl->references == 1 ensures that nobody else has taken a
 * reference to this connection while it was in the listener's queues. If
 * this assert fires, something has gone wrong with ownership tracking.
 */
static void dtls_listener_connection_free(SSL *ssl)
{
    SSL_CONNECTION *sc;

    if (ssl == NULL)
        return;

    sc = SSL_CONNECTION_FROM_SSL(ssl);

    if (sc != NULL && sc->d1 != NULL) {
        /*
         * Clear listener reference to prevent dtls1_free() from calling
         * SSL_free() on the listener. The connection does not own the listener
         * and SSL_free must not free the listener
         */
        sc->d1->listener = NULL;
    }
    SSL_free(ssl);
}

/*
 * dtls_listener_create_conn_ssl - create an SSL object for a new connection.
 *
 * Creates and initializes an SSL object for handling a new incoming
 * connection. Sets up the DTLS_RX for URXE-based packet injection, with
 * the write BIO connected to the listener's network BIO.
 *
 * Returns: new SSL object on success, NULL on failure
 */
static SSL *dtls_listener_create_conn_ssl(DTLS_LISTENER *dl,
    const BIO_ADDR *peer)
{
    SSL *ssl = NULL;
    SSL_CONNECTION *sc = NULL;
    BIO *wbio = NULL;

    ssl = SSL_new(dl->ssl.ctx);
    if (ssl == NULL)
        goto err;

    sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);
    if (sc == NULL || sc->d1 == NULL)
        goto err;

    SSL_set_accept_state(ssl);

    /*
     * Create DTLS_RX for this connection. The demux is owned by the listener
     * and will outlive this connection. DTLS_RX manages the URXE queue for
     * incoming packets.
     */
    sc->d1->rx = ossl_dtls_rx_new(dl->demux);
    if (sc->d1->rx == NULL)
        goto err;
    sc->listener_rx = sc->d1->rx;

    /*
     * Update the read record layer to use the URXE queue if it already exists.
     * This is needed because the record layer may have been created before
     * sc->d1->rx was set, similar to how SSL_set1_initial_peer_addr() updates
     * the peer address on existing record layers.
     */
    if (sc->rlayer.rrlmethod != NULL && sc->rlayer.rrl != NULL
        && sc->rlayer.rrlmethod->set_use_urxe != NULL)
        sc->rlayer.rrlmethod->set_use_urxe(sc->rlayer.rrl, 1);

    /*
     * Store reference to parent listener. This allows the connection to
     * trigger the listener's demux pump when reading data.
     */
    sc->d1->listener = &dl->ssl;

    /*
     * Record when this connection was created. This is used to detect and
     * clean up stale pending connections that haven't completed their
     * handshake within the timeout period.
     */
    sc->d1->created_at = dtls_listener_get_time_direct(dl);

    /*
     * For writes, use the shared network wbio. The peer address is NOT set
     * on the BIO itself (which would affect all connections sharing this BIO).
     * Instead, the peer address will be passed to the record layer during
     * SSL_do_handshake(), and the record layer will use BIO_sendmmsg() with
     * the peer address for each write.
     */
    wbio = dl->net_wbio;

    if (wbio == NULL) {
        ERR_raise(ERR_LIB_SSL, SSL_R_BIO_NOT_SET);
        goto err;
    }

    if (!BIO_up_ref(wbio))
        goto err;

    SSL_set0_rbio(ssl, NULL);
    SSL_set0_wbio(ssl, wbio);
    wbio = NULL; /* ownership transferred */

    /*
     * Store the peer address in the SSL connection. This will be passed to
     * the record layer when it is created during SSL_do_handshake().
     */
    if (!SSL_set1_initial_peer_addr(ssl, peer))
        goto err;

    /*
     * Enable cookie exchange if required by listener flags.
     * This tells the state machine to perform HVR (DTLS 1.2) or
     * HRR with cookie (DTLS 1.3) validation.
     */
    if (dl->require_hvr_cookie || dl->require_hrr_cookie)
        SSL_set_options(ssl, SSL_OP_COOKIE_EXCHANGE);

    return ssl;

err:
    dtls_listener_connection_free(ssl);
    return NULL;
}

/*
 * dtls_listener_signal_notifier - wake threads blocked on this listener.
 *
 * Readiness may be produced by the thread which pumps the demux while a
 * different thread is blocked in poll() on the network socket. That socket
 * will not necessarily become readable again from the blocked thread's point
 * of view, so the notifier is used to wake it.
 *
 * The caller must hold dl->mutex.
 */
static void dtls_listener_signal_notifier(DTLS_LISTENER *dl)
{
    if (dl->have_notifier && dl->cur_blocking_waiters > 0
        && !dl->signalled_notifier) {
        ossl_rio_notifier_signal(&dl->notifier);
        dl->signalled_notifier = 1;
    }
}

/*
 * dtls_listener_packet_handler - callback for handling incoming datagrams.
 *
 * This callback is invoked by the demux for each received datagram. It routes
 * the URXE to the appropriate connection based on peer address, creating a
 * new pending connection if necessary.
 *
 * The URXE ownership is transferred to the connection's DTLS_RX queue.
 * If routing fails, the URXE is released back to the demux.
 */
static void dtls_listener_packet_handler(DGRAM_URXE *urxe, void *arg)
{
    DTLS_LISTENER *dl = arg;
    SSL *conn_ssl = NULL;
    SSL_CONNECTION *sc = NULL;

    ossl_crypto_mutex_lock(dl->mutex);

    /* Check established connections first */
    if (dl->established_conns != NULL)
        conn_ssl = ossl_dgram_conn_lookup_find(dl->established_conns, urxe);

    /* Check pending connections */
    if (conn_ssl == NULL)
        conn_ssl = ossl_dgram_conn_lookup_find(dl->pending_conns, urxe);

    /* Create new pending connection if needed */
    if (conn_ssl == NULL) {
        /*
         * Reject before allocating anything if we have reached the pending
         * connection limit. The LHASH item count is O(1), and this check does
         * not need a conn_ssl, so performing it first avoids creating and then
         * immediately freeing a connection when we are at capacity.
         */
        if (ossl_dgram_conn_lookup_num_items(dl->pending_conns) >= dl->max_pending_conns)
            goto release;

        conn_ssl = dtls_listener_create_conn_ssl(dl, &urxe->peer);
        if (conn_ssl == NULL)
            goto release;

        /*
         * Register the connection in pending_conns before running the
         * application callback. This is to avoid a race condition where
         * another thread grabs the lock and tries to register a connection
         * for this address.
         */
        if (!ossl_dgram_conn_lookup_register(dl->pending_conns, urxe, conn_ssl)) {
            dtls_listener_connection_free(conn_ssl);
            goto release;
        }

        /*
         * Give the application a chance to decorate or veto the new
         * pending connection via SSL_CTX_set_new_pending_conn_cb().
         *
         * A return value of 0 from the callback means "discard this
         * connection". On a non-zero return there is nothing more to do here:
         * we already registered the connection above.
         */
        if (dl->ssl.ctx->new_pending_conn_cb != NULL) {
            int keep;

            sc = SSL_CONNECTION_FROM_SSL_ONLY(conn_ssl);
            if (sc == NULL || sc->d1 == NULL) {
                ossl_dgram_conn_lookup_unregister(dl->pending_conns, &urxe->peer);
                dtls_listener_connection_free(conn_ssl);
                goto release;
            }

            /*
             * Mark the connection being_driven while the mutex is dropped for
             * the callback. This keeps the tick loop away from this connection.
             */
            sc->listener_being_driven = 1;
            ossl_crypto_mutex_unlock(dl->mutex);
            keep = dl->ssl.ctx->new_pending_conn_cb(dl->ssl.ctx, conn_ssl,
                dl->ssl.ctx->new_pending_conn_arg);
            ossl_crypto_mutex_lock(dl->mutex);

            if (!keep) {
                /*
                 * The pending callback doesn't want this connection, so
                 * unregister and free it. While the mutex was dropped a
                 * concurrent handler may have found this same connection and
                 * injected datagrams into its RX queue; freeing releases them
                 * back to the demux via ossl_dtls_rx_free(), so nothing leaks.
                 * being_driven kept the tick away, so the connection still
                 * holds only its single reference and the free is safe.
                 */
                ossl_dgram_conn_lookup_unregister(dl->pending_conns, &urxe->peer);
                dtls_listener_connection_free(conn_ssl);
                goto release;
            }

            sc->listener_being_driven = 0;
        }
    }

    sc = SSL_CONNECTION_FROM_SSL_ONLY(conn_ssl);
    if (sc == NULL || sc->listener_rx == NULL)
        goto release;

    /* Inject packet into connection's URXE queue */
    ossl_dtls_rx_inject_urxe(sc->listener_rx, urxe);

    /* Signal notifier if needed */
    dtls_listener_signal_notifier(dl);

    ossl_crypto_mutex_unlock(dl->mutex);
    return;

release:
    ossl_crypto_mutex_unlock(dl->mutex);
    ossl_dgram_demux_release_urxe(dl->demux, urxe);
}

/*
 * DTLS Listener Internal Cookie Callbacks
 *
 * These callbacks are used internally by the DTLS listener to generate and
 * verify cookies for address validation. They use HMAC-SHA256 with the
 * SSL_CTX's cookie_hmac_key to create cookies that bind to the client's
 * address.
 *
 * Cookie format:
 *   - 8 bytes: timestamp (seconds since epoch)
 *   - 32 bytes: HMAC-SHA256(timestamp || peer_address)
 *
 * Total cookie size: 40 bytes
 */
#define DTLS_LISTENER_COOKIE_TIMESTAMP_LEN 8
#define DTLS_LISTENER_COOKIE_HMAC_LEN 32
#define DTLS_LISTENER_COOKIE_LEN (DTLS_LISTENER_COOKIE_TIMESTAMP_LEN + DTLS_LISTENER_COOKIE_HMAC_LEN)

/* Maximum age of a cookie in seconds (default: 60 seconds) */
#define DTLS_LISTENER_COOKIE_MAX_AGE 60

/*
 * dtls_listener_get_time - get current time from the listener
 *
 * Returns the current time using the listener's time callback if set,
 * otherwise uses ossl_time_now().
 *
 * If ssl is not associated with a listener, returns ossl_time_now().
 */
static OSSL_TIME dtls_listener_get_time(SSL *ssl)
{
    SSL_CONNECTION *sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);
    DTLS_LISTENER *dl;

    if (sc == NULL || sc->d1 == NULL || sc->d1->listener == NULL)
        return ossl_time_now();

    dl = (DTLS_LISTENER *)sc->d1->listener;

    if (dl->now_cb == NULL)
        return ossl_time_now();

    return dl->now_cb(dl->now_cb_arg);
}

/*
 * dtls_listener_get_time_direct - get current time directly from listener
 *
 * Same as dtls_listener_get_time but takes the listener directly.
 * Used during connection creation before listener reference is fully set up.
 */
static OSSL_TIME dtls_listener_get_time_direct(DTLS_LISTENER *dl)
{
    if (dl == NULL)
        return ossl_time_now();

    if (dl->now_cb == NULL)
        return ossl_time_now();

    return dl->now_cb(dl->now_cb_arg);
}

/*
 * dtls_listener_cookie_hmac - compute HMAC for cookie validation
 *
 * Computes HMAC-SHA256(timestamp || port || raw_address) using the
 * context's cookie_hmac_key.
 *
 * Returns 1 on success, 0 on failure.
 */
int dtls_listener_cookie_hmac(SSL *ssl, uint64_t timestamp,
    unsigned char *hmac_out)
{
    SSL_CONNECTION *sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);
    SSL_CTX *ctx;
    EVP_MAC_CTX *mctx = NULL;
    OSSL_PARAM params[2];
    /* 8 (timestamp) + 2 (port) + max address size */
    unsigned char data[DTLS_LISTENER_COOKIE_TIMESTAMP_LEN + sizeof(uint16_t) + sizeof(BIO_ADDR)];
    unsigned char addr_buf[sizeof(BIO_ADDR)];
    size_t data_len = 0;
    size_t addr_len = 0;
    size_t hmac_len = DTLS_LISTENER_COOKIE_HMAC_LEN;
    uint16_t port;
    WPACKET pkt;
    int ret = 0;

    if (sc == NULL || sc->d1 == NULL)
        return 0;

    ctx = SSL_CONNECTION_GET_CTX(sc);
    if (ctx == NULL)
        return 0;

    /* Get port and raw address */
    port = BIO_ADDR_rawport(&sc->d1->peer_addr);

    if (!BIO_ADDR_rawaddress(&sc->d1->peer_addr, addr_buf, &addr_len))
        return 0;

    /* Build data to HMAC: timestamp || port || raw_address */
    if (!WPACKET_init_static_len(&pkt, data, sizeof(data), 0)
        || !WPACKET_put_bytes_u64(&pkt, timestamp)
        || !WPACKET_put_bytes_u16(&pkt, port)
        || !WPACKET_memcpy(&pkt, addr_buf, addr_len)
        || !WPACKET_get_total_written(&pkt, &data_len)
        || !WPACKET_finish(&pkt)) {
        WPACKET_cleanup(&pkt);
        return 0;
    }

    mctx = EVP_MAC_CTX_new(ctx->hmac);
    if (mctx == NULL)
        goto err;

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
        "SHA2-256", 0);
    params[1] = OSSL_PARAM_construct_end();

    if (!EVP_MAC_init(mctx, ctx->ext.cookie_hmac_key,
            sizeof(ctx->ext.cookie_hmac_key), params))
        goto err;

    if (!EVP_MAC_update(mctx, data, data_len))
        goto err;

    if (!EVP_MAC_final(mctx, hmac_out, &hmac_len, hmac_len))
        goto err;

    ret = 1;

err:
    EVP_MAC_CTX_free(mctx);
    return ret;
}

/*
 * ossl_dtls_listener_gen_cookie_cb - internal HVR cookie generate callback
 *
 * Generates a cookie for HelloVerifyRequest (DTLS 1.2).
 * Cookie format: timestamp (8 bytes) || HMAC (32 bytes)
 */
int ossl_dtls_listener_gen_cookie_cb(SSL *ssl, unsigned char *cookie,
    unsigned int *cookie_len)
{
    uint64_t now = ossl_time2seconds(dtls_listener_get_time(ssl));

    /* Write timestamp */
    cookie[0] = (unsigned char)(now >> 56);
    cookie[1] = (unsigned char)(now >> 48);
    cookie[2] = (unsigned char)(now >> 40);
    cookie[3] = (unsigned char)(now >> 32);
    cookie[4] = (unsigned char)(now >> 24);
    cookie[5] = (unsigned char)(now >> 16);
    cookie[6] = (unsigned char)(now >> 8);
    cookie[7] = (unsigned char)(now);

    /* Compute and append HMAC */
    if (!dtls_listener_cookie_hmac(ssl, now, cookie + DTLS_LISTENER_COOKIE_TIMESTAMP_LEN))
        return 0;

    *cookie_len = DTLS_LISTENER_COOKIE_LEN;
    return 1;
}

/*
 * ossl_dtls_listener_verify_cookie_cb - internal HVR cookie verify callback
 *
 * Verifies a cookie from ClientHello (DTLS 1.2).
 * Checks that:
 *   1. Cookie length is correct
 *   2. Timestamp is not too old
 *   3. HMAC matches
 */
int ossl_dtls_listener_verify_cookie_cb(SSL *ssl, const unsigned char *cookie,
    unsigned int cookie_len)
{
    uint64_t cookie_time, now;
    unsigned char expected_hmac[DTLS_LISTENER_COOKIE_HMAC_LEN];

    if (cookie_len != DTLS_LISTENER_COOKIE_LEN)
        return 0;

    /* Extract timestamp from cookie */
    cookie_time = ((uint64_t)cookie[0] << 56)
        | ((uint64_t)cookie[1] << 48)
        | ((uint64_t)cookie[2] << 40)
        | ((uint64_t)cookie[3] << 32)
        | ((uint64_t)cookie[4] << 24)
        | ((uint64_t)cookie[5] << 16)
        | ((uint64_t)cookie[6] << 8)
        | ((uint64_t)cookie[7]);

    /* Check timestamp is not too old */
    now = ossl_time2seconds(dtls_listener_get_time(ssl));
    if (now > cookie_time && (now - cookie_time) > DTLS_LISTENER_COOKIE_MAX_AGE)
        return 0;

    /* Compute expected HMAC and compare */
    if (!dtls_listener_cookie_hmac(ssl, cookie_time, expected_hmac))
        return 0;

    if (CRYPTO_memcmp(cookie + DTLS_LISTENER_COOKIE_TIMESTAMP_LEN,
            expected_hmac, DTLS_LISTENER_COOKIE_HMAC_LEN)
        != 0)
        return 0;

    return 1;
}

/*
 * ossl_dtls_listener_gen_stateless_cookie_cb - internal HRR cookie generate callback
 *
 * Generates a cookie for HelloRetryRequest (DTLS 1.3).
 * Uses the same format as the HVR cookie.
 */
int ossl_dtls_listener_gen_stateless_cookie_cb(SSL *ssl, unsigned char *cookie,
    size_t *cookie_len)
{
    uint64_t now = ossl_time2seconds(dtls_listener_get_time(ssl));

    /* Write timestamp */
    cookie[0] = (unsigned char)(now >> 56);
    cookie[1] = (unsigned char)(now >> 48);
    cookie[2] = (unsigned char)(now >> 40);
    cookie[3] = (unsigned char)(now >> 32);
    cookie[4] = (unsigned char)(now >> 24);
    cookie[5] = (unsigned char)(now >> 16);
    cookie[6] = (unsigned char)(now >> 8);
    cookie[7] = (unsigned char)(now);

    /* Compute and append HMAC */
    if (!dtls_listener_cookie_hmac(ssl, now, cookie + DTLS_LISTENER_COOKIE_TIMESTAMP_LEN))
        return 0;

    *cookie_len = DTLS_LISTENER_COOKIE_LEN;
    return 1;
}

/*
 * ossl_dtls_listener_verify_stateless_cookie_cb - internal HRR cookie verify callback
 *
 * Verifies a cookie from ClientHello (DTLS 1.3).
 * Uses the same verification logic as the HVR cookie.
 */
int ossl_dtls_listener_verify_stateless_cookie_cb(SSL *ssl,
    const unsigned char *cookie,
    size_t cookie_len)
{
    uint64_t cookie_time, now;
    unsigned char expected_hmac[DTLS_LISTENER_COOKIE_HMAC_LEN];

    if (cookie_len != DTLS_LISTENER_COOKIE_LEN)
        return 0;

    /* Extract timestamp from cookie */
    cookie_time = ((uint64_t)cookie[0] << 56)
        | ((uint64_t)cookie[1] << 48)
        | ((uint64_t)cookie[2] << 40)
        | ((uint64_t)cookie[3] << 32)
        | ((uint64_t)cookie[4] << 24)
        | ((uint64_t)cookie[5] << 16)
        | ((uint64_t)cookie[6] << 8)
        | ((uint64_t)cookie[7]);

    /* Check timestamp is not too old */
    now = ossl_time2seconds(dtls_listener_get_time(ssl));
    if (now > cookie_time && (now - cookie_time) > DTLS_LISTENER_COOKIE_MAX_AGE)
        return 0;

    /* Compute expected HMAC and compare */
    if (!dtls_listener_cookie_hmac(ssl, cookie_time, expected_hmac))
        return 0;

    if (CRYPTO_memcmp(cookie + DTLS_LISTENER_COOKIE_TIMESTAMP_LEN,
            expected_hmac, DTLS_LISTENER_COOKIE_HMAC_LEN)
        != 0)
        return 0;

    return 1;
}

SSL *ossl_dtls_new_listener(SSL_CTX *ctx, uint64_t flags)
{
    DTLS_LISTENER *dl = NULL;
    int ssl_init_done = 0;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }

    if ((dl = OPENSSL_zalloc(sizeof(*dl))) == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_CRYPTO_LIB);
        goto err;
    }

    /*
     * Use ossl_ssl_init to initialize the SSL object header consistently
     * with other SSL object types.
     */
    if (!ossl_ssl_init(&dl->ssl, ctx, ctx->method, SSL_TYPE_DTLS_LISTENER)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_CRYPTO_LIB);
        goto err;
    }
    ssl_init_done = 1;

    dl->mutex = ossl_crypto_mutex_new();
#ifdef OPENSSL_THREADS
    if (dl->mutex == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_CRYPTO_LIB);
        goto err;
    }
#endif

    /* Create demux with internal locking for thread safety. */
    dl->demux = ossl_dgram_demux_new(NULL, 1, NULL, NULL);
    if (dl->demux == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_CRYPTO_LIB);
        goto err;
    }

    /* Set up the packet handler callback for routing datagrams to connections */
    ossl_dgram_demux_set_default_handler(dl->demux, dtls_listener_packet_handler, dl);

    dl->incoming_connections = sk_SSL_new_null();
    if (dl->incoming_connections == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_CRYPTO_LIB);
        goto err;
    }

    dl->pending_conns = ossl_dgram_conn_lookup_new_addr();
    if (dl->pending_conns == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_CRYPTO_LIB);
        goto err;
    }

    dl->established_conns = ossl_dgram_conn_lookup_new_addr();
    if (dl->established_conns == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_CRYPTO_LIB);
        goto err;
    }

    dl->net_rbio = NULL;
    dl->net_wbio = NULL;
    tsan_store(&dl->listening, 0);
    dl->fatal = 0;

    /* Default timeout for pending connections: 30 seconds */
    dl->pending_timeout = ossl_seconds2time(30);

    /* Default maximum pending connections */
    dl->max_pending_conns = DTLS_LISTENER_DEFAULT_MAX_PENDING_CONNS;

    /*
     * The listener owns its receive-buffer size independent of any
     * network BIO's send-path MTU.
     */
    dl->max_dgram_size = DTLS_LISTENER_DEFAULT_MAX_DGRAM_SIZE;
    ossl_dgram_demux_set_mtu(dl->demux, (unsigned int)dl->max_dgram_size);

    /*
     * Address validation is performed by default: HelloVerifyRequest for
     * DTLS 1.0/1.2 and a HelloRetryRequest cookie for DTLS 1.3.  It can be
     * requested explicitly with SSL_LISTENER_FLAG_ADDRESS_VALIDATION, or
     * disabled with SSL_LISTENER_FLAG_NO_VALIDATE.  If both are specified we
     * fail safe and validate: SSL_LISTENER_FLAG_ADDRESS_VALIDATION wins.
     */
    if ((flags & SSL_LISTENER_FLAG_NO_VALIDATE) == 0
        || (flags & SSL_LISTENER_FLAG_ADDRESS_VALIDATION) != 0) {
        dl->require_hvr_cookie = 1;
        dl->require_hrr_cookie = 1;
    }

    dl->have_notifier = 0;
    dl->signalled_notifier = 0;
    dl->cur_blocking_waiters = 0;

    if ((flags & SSL_LISTENER_FLAG_SINGLE_THREAD) == 0) {
        if (!ossl_rio_notifier_init(&dl->notifier))
            goto err;

        dl->notifier_cv = ossl_crypto_condvar_new();
        if (dl->notifier_cv == NULL) {
            ossl_rio_notifier_cleanup(&dl->notifier);
            goto err;
        }

        dl->have_notifier = 1;
    }

    return &dl->ssl;

err:
    if (dl == NULL)
        return NULL;

    /*
     * If ossl_ssl_init succeeded, SSL_free handles all cleanup
     * including incoming_connections, notifier_cv, and OPENSSL_free(dl)
     * itself via ossl_dtls_listener_free. Otherwise ossl_ssl_init
     * did not run or partially failed, so we must free the raw
     * allocation directly.
     */
    if (ssl_init_done)
        SSL_free(&dl->ssl);
    else
        OPENSSL_free(dl);
    return NULL;
}

/*
 * Callback to free SSL objects in pending_conns hash table.
 * The pending_conns hash table owns the SSL objects it contains,
 * so we must free them before freeing the hash table itself.
 */
static void dtls_free_pending_ssl_cb(SSL *ssl, const BIO_ADDR *peer, void *arg)
{
    dtls_listener_connection_free(ssl);
}

void ossl_dtls_listener_free(SSL *s)
{
    DTLS_LISTENER *dl;

    if (!IS_DTLS_LISTENER(s))
        return;

    dl = (DTLS_LISTENER *)s;

    /* Free any pending incoming connections */
    if (dl->incoming_connections != NULL) {
        while (sk_SSL_num(dl->incoming_connections) > 0) {
            SSL *conn = sk_SSL_pop(dl->incoming_connections);

            dtls_listener_connection_free(conn);
        }
        sk_SSL_free(dl->incoming_connections);
        dl->incoming_connections = NULL;
    }

    /*
     * Free all pending connections in the hash table.
     */
    if (dl->pending_conns != NULL) {
        ossl_dgram_conn_lookup_foreach(dl->pending_conns, dtls_free_pending_ssl_cb, NULL);
        ossl_dgram_conn_lookup_free(dl->pending_conns);
        dl->pending_conns = NULL;
    }

    /* Free all established connections in the hash table (no SSL ownership) */
    if (dl->established_conns != NULL) {
        ossl_dgram_conn_lookup_free(dl->established_conns);
        dl->established_conns = NULL;
    }

    ossl_crypto_mutex_free(&dl->mutex);

    /* Free the demux after all connections that reference it are freed */
    if (dl->demux != NULL)
        ossl_dgram_demux_free(dl->demux);

    BIO_free_all(dl->net_wbio);
    BIO_free_all(dl->net_rbio);

    if (dl->have_notifier) {
        ossl_crypto_condvar_free(&dl->notifier_cv);
        ossl_rio_notifier_cleanup(&dl->notifier);
    }
}

SSL *ossl_dtls_get0_listener(const SSL *ssl)
{
    if (!IS_DTLS_LISTENER(ssl))
        return NULL;

    return (SSL *)ssl;
}

/*
 * ossl_dtls_listen - start a DTLS listener accepting incoming connections.
 */
int ossl_dtls_listen(SSL *ssl)
{
    DTLS_LISTENER *dl;

    if (!IS_DTLS_LISTENER(ssl)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }

    dl = (DTLS_LISTENER *)ssl;

    /* Already listening is not an error. */
    if (tsan_load(&dl->listening))
        return 1;

    tsan_store(&dl->listening, 1);
    return 1;
}

/*
 * dtls_listener_conn_ready - check if connection is ready for accept queue.
 *
 * Determines whether the SSL object has completed cookie validation (if required)
 * or has received a valid ClientHello (if no validation) and is ready to be
 * moved to the incoming_connections queue.
 *
 * The connection is returned to the application BEFORE the handshake completes,
 * allowing the application to finish the handshake itself. This provides more
 * control over the handshake process.
 *
 * For HRR (DTLS 1.3 with validation): Ready when sc->ext.cookieok is set
 * For HVR (DTLS 1.2 with validation): Ready when sc->d1->cookie_verified is set
 * For no validation: Ready after receiving the first ClientHello
 *
 * Returns: 1 if ready, 0 if still in progress
 */
static int dtls_listener_conn_ready(SSL *ssl, DTLS_LISTENER *dl)
{
    SSL_CONNECTION *sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

    if (sc == NULL)
        return 0;

    /*
     * No validation required (SSL_LISTENER_FLAG_NO_VALIDATE):
     * Ready immediately after receiving the first ClientHello.
     * The connection exists in pending_conns, so it's ready.
     */
    if (!dl->require_hrr_cookie && !dl->require_hvr_cookie)
        return 1;

    /*
     * For DTLS 1.3 with HRR requirement:
     * Ready when the cookie has been validated (second ClientHello received
     * with valid cookie after HRR was sent). The cookieok flag is set during
     * ClientHello processing when the HRR cookie is successfully verified.
     */
    if (dl->require_hrr_cookie && sc->ext.cookieok)
        return 1;

    /*
     * For DTLS 1.2 (and earlier) with HVR requirement:
     * Ready when the cookie has been validated (second ClientHello received
     * with valid cookie after HVR was sent). The cookie_verified flag is set
     * during ClientHello processing when the HVR cookie is successfully verified.
     */
    if (dl->require_hvr_cookie && sc->d1 != NULL && sc->d1->cookie_verified)
        return 1;

    /* Not ready yet - still waiting for cookie validation */
    return 0;
}

/*
 * dtls_listener_conn_needs_retry - check if connection is waiting for more data.
 *
 * Determines whether the SSL object has sent an HRR/HVR and is waiting
 * for the client's response.
 *
 * Returns: 1 if waiting for retry, 0 otherwise
 */
static int dtls_listener_conn_needs_retry(SSL *ssl)
{
    SSL_CONNECTION *sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

    if (sc == NULL)
        return 0;

    /*
     * For DTLS 1.3: HRR has been sent, waiting for second ClientHello
     */
    if (sc->hello_retry_request == SSL_HRR_PENDING
        && !ossl_statem_in_error(sc))
        return 1;

    /*
     * For DTLS 1.2: Check if we're in a state that indicates HVR was sent.
     * The state machine will be waiting for the next ClientHello.
     */
    if (sc->statem.hand_state == DTLS_ST_SW_HELLO_VERIFY_REQUEST)
        return 1;

    return 0;
}

/*
 * Context for drive_pending iteration.
 */
typedef struct {
    DTLS_LISTENER *dl;
    int ready_count; /* Connections ready to move to established */
    int error_count; /* Connections with fatal errors */
    STACK_OF(SSL) *to_drive; /* Connections to drive (collected in phase 1) */
    STACK_OF(SSL) *ready_conns; /* Connections to move */
    STACK_OF(SSL) *failed_conns; /* Connections to remove */
} DRIVE_PENDING_CTX;

/*
 * Callback for collecting pending connections to drive.
 * Called with dl->mutex held. Marks connections as being_driven and up-refs them.
 * Also checks for timed-out connections and marks them as failed.
 */
static void collect_pending_cb(SSL *ssl, const BIO_ADDR *peer, void *arg)
{
    DRIVE_PENDING_CTX *ctx = arg;
    DTLS_LISTENER *dl = ctx->dl;
    SSL_CONNECTION *sc;

    sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);
    if (sc == NULL)
        return;

    /*
     * Skip if already being driven by another thread.
     */
    if (sc->listener_being_driven)
        return;

    if (sc->listener_rx == NULL || sc->d1 == NULL)
        return;

    /*
     * Check if this pending connection has exceeded the timeout.
     * Stale connections that haven't completed their handshake are removed
     * to prevent resource exhaustion from incomplete handshakes.
     */
    if (!ossl_time_is_infinite(dl->pending_timeout)) {
        OSSL_TIME now = dtls_listener_get_time_direct(dl);
        OSSL_TIME age = ossl_time_subtract(now, sc->d1->created_at);

        if (ossl_time_compare(age, dl->pending_timeout) > 0) {
            /*
             * Connection has timed out - mark for removal
             *
             * Set being_driven so that a concurrent ossl_dtls_tick() running
             * phase 1 hits the being_driven check above and skips this
             * connection, instead of collecting it and freeing it a second
             * time (double-free).
             *
             * No up-ref is needed: phase 1 runs under dl->mutex, so collection
             * is serialized, and being_driven keeps any other tick away until
             * our phase 3 frees this connection. The single pending-queue
             * reference is released by dtls_listener_connection_free() in the
             * failed_conns loop.
             *
             * We intentionally do not handle a failed push specially: if the
             * push fails (allocation failure) the connection stays registered
             * in pending_conns and is simply retried on the next tick.
             */
            if (ctx->failed_conns != NULL && sk_SSL_push(ctx->failed_conns, ssl) > 0) {
                sc->listener_being_driven = 1;
                ctx->error_count++;
            }
            return;
        }
    }

    /*
     * Up-ref and add to list first, then mark as being driven.
     * The up-ref ensures the connection stays valid while we drive it
     * without holding the mutex.
     */
    if (!SSL_up_ref(ssl))
        return;

    if (sk_SSL_push(ctx->to_drive, ssl) <= 0) {
        SSL_free(ssl); /* Release the ref we just took */
        return;
    }

    sc->listener_being_driven = 1;
}

/*
 * Drive a single connection's handshake.
 * Called WITHOUT holding dl->mutex so demux_pump can be called safely.
 */
static void drive_single_connection(SSL *ssl, DTLS_LISTENER *dl,
    DRIVE_PENDING_CTX *ctx)
{
    SSL_CONNECTION *sc;
    int ret, ssl_err;

    sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);
    if (sc == NULL)
        return;

    /*
     * Drive the state machine with SSL_accept().
     *
     * We MUST set TLS1_FLAGS_STATELESS to prevent the state machine from
     * calling SSL_clear() when entering the handshake.
     *
     * Without the flag, the state machine in state_machine() calls SSL_clear()
     * when SSL_in_before() is true, which wipes out our restored state.
     */
    if (dl->require_hrr_cookie || dl->require_hvr_cookie)
        sc->s3.flags |= TLS1_FLAGS_STATELESS;

    /*
     * We are inside the listener's own tick, so this must not block: nothing
     * else can make progress while it does, including whatever it would be
     * waiting for.
     */
    sc->listener_force_nonblocking = 1;
    ret = SSL_accept(ssl);
    sc->listener_force_nonblocking = 0;

    /*
     * Always clear the stateless flag after SSL_accept() completes.
     */
    if (dl->require_hrr_cookie || dl->require_hvr_cookie)
        sc->s3.flags &= ~TLS1_FLAGS_STATELESS;

    /* Check if connection is ready to move to established */
    if (dtls_listener_conn_ready(ssl, dl)) {
        if (ctx->ready_conns != NULL && sk_SSL_push(ctx->ready_conns, ssl) > 0)
            ctx->ready_count++;
        return;
    }

    /* Check if connection needs retry (HRR/HVR sent) */
    if (dtls_listener_conn_needs_retry(ssl))
        return;

    /* Check SSL error */
    ssl_err = SSL_get_error(ssl, ret);

    if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
        /* Handshake in progress, needs more data - keep pending */
        return;
    }

    /* Fatal error on this connection - mark for removal */
    if (ssl_err == SSL_ERROR_SYSCALL || ssl_err == SSL_ERROR_SSL) {
        if (ctx->failed_conns != NULL && sk_SSL_push(ctx->failed_conns, ssl) > 0)
            ctx->error_count++;
    }
}

/*
 * dtls_listener_drive_pending - drive handshakes for all pending connections.
 *
 * Uses a three-phase approach to minimize lock contention:
 *   Phase 1: LOCK - collect connections to drive, mark as being_driven, up-ref
 *   Phase 2: UNLOCK - drive each connection (can safely call demux_pump)
 *   Phase 3: LOCK - update data structures, clear being_driven, release refs
 *
 * This approach allows SSL_accept() to call demux_pump() without deadlock,
 * since the mutex is not held during phase 2.
 *
 * Returns:
 *   1   At least one connection was moved to incoming_connections
 *   0   No connections completed (all still pending or failed)
 *  -1   Fatal error
 */
static int dtls_listener_drive_pending(DTLS_LISTENER *dl)
{
    DRIVE_PENDING_CTX ctx;
    SSL *ssl;
    SSL_CONNECTION *sc;
    int i, result = 0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.dl = dl;
    ctx.to_drive = sk_SSL_new_null();
    ctx.ready_conns = sk_SSL_new_null();
    ctx.failed_conns = sk_SSL_new_null();

    if (ctx.to_drive == NULL || ctx.ready_conns == NULL
        || ctx.failed_conns == NULL) {
        sk_SSL_free(ctx.to_drive);
        sk_SSL_free(ctx.ready_conns);
        sk_SSL_free(ctx.failed_conns);
        return -1;
    }

    /*
     * Phase 1: Collect connections to drive.
     * Hold mutex while iterating pending_conns, mark connections as being_driven,
     * and up-ref them so they stay valid after we release the mutex.
     */
    ossl_crypto_mutex_lock(dl->mutex);
    ossl_dgram_conn_lookup_foreach(dl->pending_conns, collect_pending_cb, &ctx);
    ossl_crypto_mutex_unlock(dl->mutex);

    /*
     * Phase 2: Drive connections WITHOUT holding mutex.
     * This allows SSL_accept() to call demux_pump() which may invoke
     * packet_handler(), which needs to acquire the mutex.
     */
    for (i = 0; i < sk_SSL_num(ctx.to_drive); i++) {
        ssl = sk_SSL_value(ctx.to_drive, i);
        drive_single_connection(ssl, dl, &ctx);
    }

    /*
     * Phase 3: Update data structures.
     * Re-acquire mutex to move ready connections to established,
     * remove failed connections, clear being_driven flags, and release refs.
     */
    ossl_crypto_mutex_lock(dl->mutex);

    /*
     * Since we have the mutex, clear the driven flag and release the
     * up-ref for each connection.
     */
    for (i = 0; i < sk_SSL_num(ctx.to_drive); i++) {
        ssl = sk_SSL_value(ctx.to_drive, i);
        sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

        if (sc != NULL)
            sc->listener_being_driven = 0;

        SSL_free(ssl); /* Release reference from phase 1 */
    }

    /* Move ready connections to established and incoming queue */
    for (i = 0; i < sk_SSL_num(ctx.ready_conns); i++) {
        ssl = sk_SSL_value(ctx.ready_conns, i);
        sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

        if (sc == NULL || sc->d1 == NULL)
            continue;

        /* Get peer address from the connection */
        if (BIO_ADDR_family(&sc->d1->peer_addr) != AF_UNSPEC) {

            /* Remove from pending */
            ossl_dgram_conn_lookup_unregister(dl->pending_conns, &sc->d1->peer_addr);

            /* Add to established connections (inline, we already hold mutex) */
            if (dl->established_conns == NULL || !ossl_dgram_conn_lookup_register_addr(dl->established_conns, &sc->d1->peer_addr, ssl)) {
                dtls_listener_connection_free(ssl);
                continue;
            }

            /* Add to incoming queue */
            if (sk_SSL_push(dl->incoming_connections, ssl) > 0) {
                result = 1;
            } else {
                /* Failed to add to queue, unregister and free */
                ossl_dgram_conn_lookup_unregister(dl->established_conns,
                    &sc->d1->peer_addr);
                dtls_listener_connection_free(ssl);
            }
        }
    }

    /*
     * A connection became acceptable. Any thread blocked waiting for one is
     * polling the network socket, which will not necessarily become readable
     * again on its behalf, so wake it explicitly.
     */
    if (result)
        dtls_listener_signal_notifier(dl);

    /* Remove failed connections (after releasing refs so ref count is 1) */
    for (i = 0; i < sk_SSL_num(ctx.failed_conns); i++) {
        ssl = sk_SSL_value(ctx.failed_conns, i);
        sc = SSL_CONNECTION_FROM_SSL_ONLY(ssl);

        if (sc != NULL && sc->d1 != NULL)
            ossl_dgram_conn_lookup_unregister(dl->pending_conns, &sc->d1->peer_addr);

        dtls_listener_connection_free(ssl);
    }

    ossl_crypto_mutex_unlock(dl->mutex);

    sk_SSL_free(ctx.to_drive);
    sk_SSL_free(ctx.ready_conns);
    sk_SSL_free(ctx.failed_conns);

    return result;
}

/*
 * ossl_dtls_tick - drive one iteration of the DTLS listener I/O loop.
 *
 * Uses the demux pump/callback architecture for efficient packet handling:
 *   1. Call ossl_dgram_demux_pump() to read datagrams from the network
 *   2. The demux invokes dtls_listener_packet_handler() for each datagram
 *   3. The handler routes URXEs to connections (established or pending)
 *   4. Drive handshakes for pending connections
 *   5. Move completed connections to established_conns and incoming queue
 *
 * Return values:
 *   1   A verified connection was pushed onto dl->incoming_connections.
 *   0   Exchange incomplete (HRR/HVR sent, or no data yet); call again.
 *  -1   Fatal error; dl->fatal is set.
 */
int ossl_dtls_tick(DTLS_LISTENER *dl)
{
    int pump_ret;

    if (dl == NULL || dl->net_rbio == NULL)
        return 0;

    /*
     * Get datagrams from the network and route them to connections.
     */
    pump_ret = ossl_dgram_demux_pump(dl->demux);

    if (pump_ret == DGRAM_DEMUX_PUMP_RES_PERMANENT_FAIL) {
        /* Fatal BIO or allocation error */
        ossl_crypto_mutex_lock(dl->mutex);
        dl->fatal = 1;
        ossl_crypto_mutex_unlock(dl->mutex);
        return -1;
    }

    /*
     * Drive Handshakes for pending connections.
     * call even if pump_ret indicates no data or temporary failure,
     * to allow handshakes to progress even when no new data is arriving
     */
    return dtls_listener_drive_pending(dl);
}

SSL *ossl_dtls_accept_connection(SSL *ssl, uint64_t flags)
{
    DTLS_LISTENER *dl;
    SSL *conn = NULL;
    SSL_CONNECTION *sc = NULL;
    int no_block = ((flags & SSL_ACCEPT_CONNECTION_NO_BLOCK) != 0);

    if (!IS_DTLS_LISTENER(ssl)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return NULL;
    }

    dl = (DTLS_LISTENER *)ssl;

    if (!ossl_dtls_listen(ssl))
        return NULL;

    /* If a previous tick produced a fatal BIO error, do not try again. */
    ossl_crypto_mutex_lock(dl->mutex);
    if (dl->fatal) {
        ossl_crypto_mutex_unlock(dl->mutex);
        return NULL;
    }

    /* Fast path: return any already-queued connection immediately. */
    conn = sk_SSL_shift(dl->incoming_connections);
    ossl_crypto_mutex_unlock(dl->mutex);
    if (conn != NULL)
        goto end;

    /*
     * Wait only if the caller has not asked us not to and the listener is in
     * blocking mode. Note that the check for a network BIO below is deliberately
     * left ahead of this, so that asking to wait on a listener which has none
     * remains an error rather than silently returning nothing.
     */
    if (!no_block && !ossl_dtls_blocking(ssl) && dl->net_rbio != NULL)
        no_block = 1;

    if (no_block) {
        /*
         * Non-blocking: run one tick to drain any pending datagram, then
         * return whatever is in the queue
         */
        if (dl->net_rbio != NULL) {
            if (ossl_dtls_tick(dl) < 0) {
                ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
                return NULL;
            }
        }
        ossl_crypto_mutex_lock(dl->mutex);
        conn = sk_SSL_shift(dl->incoming_connections);
        ossl_crypto_mutex_unlock(dl->mutex);
        goto end;
    }

    /* Blocking path: we need a BIO to make any progress. */
    if (dl->net_rbio == NULL) {
        ERR_raise(ERR_LIB_SSL, SSL_R_BIO_NOT_SET);
        return NULL;
    }

    /*
     * Blocking path: tick to make whatever progress is possible now, and if
     * that did not produce a connection, wait for readiness before ticking
     * again.
     *
     * The wait is what stops this from being a busy loop. The network BIO is
     * non-blocking, so a tick which finds no datagram returns immediately;
     * without waiting in between, this loop would spin.
     */
    for (;;) {
        if (ossl_dtls_tick(dl) < 0) {
            /* fatal BIO error */
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            break;
        }

        ossl_crypto_mutex_lock(dl->mutex);
        conn = sk_SSL_shift(dl->incoming_connections);
        ossl_crypto_mutex_unlock(dl->mutex);
        if (conn != NULL)
            break;

        /*
         * Nothing yet, so wait for the listener to become ready before ticking
         * again. What that amounts to is decided by the poll translation for a
         * listener: the network socket becoming readable, or another thread
         * signalling the notifier because it produced readiness on our behalf.
         */
        if (!ossl_dtls_block_until_ready(ssl, SSL_POLL_EVENT_IC,
                ossl_time_infinite(), /*bound_by_event_timeout=*/1))
            break;
    }

end:
    if (conn != NULL) {
        sc = SSL_CONNECTION_FROM_SSL(conn);
        /*
         * Take a reference on the listener now that ownership of the connection
         * is transferring to the application. While in incoming_connections,
         * the connection did not hold a reference to avoid circular dependencies.
         * Now that the application owns the connection, it must hold a reference
         * to ensure the listener stays alive - the connection needs the listener
         * for packet routing
         */
        if (sc == NULL || sc->d1 == NULL || sc->d1->listener == NULL
            || !SSL_up_ref(sc->d1->listener)) {
            /*
             * Ownership did not transfer to the application. A connection taken
             * from incoming_connections is still registered in established_conns;
             */
            if (sc != NULL && sc->d1 != NULL)
                ossl_dtls_listener_unregister_established_conn(ssl, &sc->d1->peer_addr);
            dtls_listener_connection_free(conn);
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return NULL;
        }
    }
    return conn;
}

/*
 * ossl_dtls_listener_set0_net_rbio - set the network read BIO for a listener.
 *
 * Thread safety: The caller must ensure that this function is not called
 * concurrently with any other operations on the listener or its connections.
 * This includes SSL_accept_connection(), SSL_poll(), SSL_tick(), and any
 * I/O operations on connections created from this listener.
 *
 * The BIO must not be changed while other threads are actively using the
 * listener. Typically, the BIO should be set once before calling SSL_listen()
 * and not modified afterward.
 */
void ossl_dtls_listener_set0_net_rbio(SSL *s, BIO *bio)
{
    DTLS_LISTENER *dl;
    BIO *old_rbio;

    if (!IS_DTLS_LISTENER(s))
        return;

    dl = (DTLS_LISTENER *)s;

    /*
     * The listener demultiplexes one socket to many connections, so it can
     * never afford to block inside a read: a read for one connection would
     * stall every other, and the demux lock is held across it. Blocking
     * behaviour is provided by waiting for readiness instead, so configure the
     * BIO for non-blocking operation on the application's behalf, as QUIC does.
     */
    if (bio != NULL)
        BIO_set_nbio(bio, 1); /* best effort autoconfig */

    ossl_crypto_mutex_lock(dl->mutex);

    /*
     * The demux receive-buffer size is the listener's configured maximum
     * datagram size, independent of the network BIO's path MTU. Size the demux
     * to that value for whatever BIO is attached.
     */
    ossl_dgram_demux_set_bio(dl->demux, bio);
    ossl_dgram_demux_set_mtu(dl->demux, (unsigned int)dl->max_dgram_size);

    old_rbio = dl->net_rbio;

    /* No change - nothing to do */
    if (old_rbio == bio) {
        ossl_crypto_mutex_unlock(dl->mutex);
        return;
    }

    dl->net_rbio = bio;

    ossl_crypto_mutex_unlock(dl->mutex);

    /* Free the old BIO now that we've taken ownership of the new one */
    BIO_free_all(old_rbio);
}

/*
 * update_conn_wbio - callback to update the wbio on a single connection.
 *
 * Used by ossl_dtls_listener_set0_net_wbio() to propagate wbio changes
 * to all pending and established connections
 */
static void update_conn_wbio(SSL *ssl, const BIO_ADDR *peer, void *arg)
{
    BIO *new_wbio = arg;

    if (SSL_get_wbio(ssl) == new_wbio)
        return;

    if (new_wbio != NULL && !BIO_up_ref(new_wbio))
        return;

    SSL_set0_wbio(ssl, new_wbio);
}

/*
 * ossl_dtls_listener_set0_net_wbio - set the network write BIO for a listener.
 *
 * Thread safety: The caller must ensure that this function is not called
 * concurrently with any other operations on the listener or its connections.
 * This includes SSL_accept_connection(), SSL_poll(), SSL_tick(), and any
 * I/O operations on connections created from this listener.
 *
 * The BIO must not be changed while other threads are actively using the
 * listener. Typically, the BIO should be set once before calling SSL_listen()
 * and not modified afterward.
 */
void ossl_dtls_listener_set0_net_wbio(SSL *s, BIO *bio)
{
    DTLS_LISTENER *dl;
    BIO *old_wbio;

    if (!IS_DTLS_LISTENER(s))
        return;

    dl = (DTLS_LISTENER *)s;

    /* See ossl_dtls_listener_set0_net_rbio() as to why. */
    if (bio != NULL)
        BIO_set_nbio(bio, 1); /* best effort autoconfig */

    old_wbio = dl->net_wbio;

    /* No change - nothing to do */
    if (old_wbio == bio) {
        return;
    }

    /* Update wbio in all pending connections */
    if (dl->pending_conns != NULL)
        ossl_dgram_conn_lookup_foreach(dl->pending_conns, update_conn_wbio, bio);

    /* Update wbio in all established connections */
    if (dl->established_conns != NULL)
        ossl_dgram_conn_lookup_foreach(dl->established_conns, update_conn_wbio, bio);

    dl->net_wbio = bio;

    /* Free the old BIO now that we've taken ownership of the new one */
    BIO_free_all(old_wbio);
}

/*
 * ossl_dtls_listener_get_net_rbio - get the network read BIO for a listener.
 *
 * Thread safety: The caller must ensure that the BIO is not being changed
 * concurrently via SSL_set0_rbio(). The returned BIO pointer is only valid
 * as long as no other thread modifies it.
 */
BIO *ossl_dtls_listener_get_net_rbio(const SSL *s)
{
    const DTLS_LISTENER *dl;

    if (!IS_DTLS_LISTENER(s))
        return NULL;

    dl = (const DTLS_LISTENER *)s;

    return dl->net_rbio;
}

/*
 * ossl_dtls_listener_get_net_wbio - get the network write BIO for a listener.
 *
 * Thread safety: The caller must ensure that the BIO is not being changed
 * concurrently via SSL_set0_wbio(). The returned BIO pointer is only valid
 * as long as no other thread modifies it.
 */
BIO *ossl_dtls_listener_get_net_wbio(const SSL *s)
{
    const DTLS_LISTENER *dl;

    if (!IS_DTLS_LISTENER(s))
        return NULL;

    dl = (const DTLS_LISTENER *)s;

    return dl->net_wbio;
}

/*
 * Established connections API - these handle their own locking.
 *
 * The established_conns lookup table is accessed from multiple threads:
 * - Listener thread: looking up and registering connections
 * - Connection thread: unregistering via SSL_free -> dtls1_free
 */

/*
 * ossl_dtls_listener_find_established_conn - find an established connection.
 *
 * Looks up a connection in the established_conns table by peer address.
 * Returns the SSL connection if found, NULL otherwise.
 */
SSL *ossl_dtls_listener_find_established_conn(DTLS_LISTENER *dl,
    const DGRAM_URXE *urxe)
{
    SSL *result = NULL;

    if (dl == NULL || urxe == NULL)
        return NULL;

    ossl_crypto_mutex_lock(dl->mutex);

    if (dl->established_conns != NULL)
        result = ossl_dgram_conn_lookup_find(dl->established_conns, urxe);

    ossl_crypto_mutex_unlock(dl->mutex);
    return result;
}

/*
 * ossl_dtls_listener_unregister_established_conn - unregister an established
 * connection from the listener.
 *
 * Called when a DTLS connection created by this listener is being freed.
 */
void ossl_dtls_listener_unregister_established_conn(SSL *s, const BIO_ADDR *peer_addr)
{
    DTLS_LISTENER *dl;

    if (!IS_DTLS_LISTENER(s))
        return;

    if (peer_addr == NULL || BIO_ADDR_family(peer_addr) == AF_UNSPEC)
        return;

    dl = (DTLS_LISTENER *)s;

    ossl_crypto_mutex_lock(dl->mutex);

    if (dl->established_conns != NULL)
        ossl_dgram_conn_lookup_unregister(dl->established_conns, peer_addr);

    ossl_crypto_mutex_unlock(dl->mutex);
}

/*
 * ossl_dtls_listener_clear_established_conns - clear and recreate the
 * established_conns table.
 */
void ossl_dtls_listener_clear_established_conns(DTLS_LISTENER *dl)
{
    if (dl == NULL)
        return;

    ossl_crypto_mutex_lock(dl->mutex);

    if (dl->established_conns != NULL)
        ossl_dgram_conn_lookup_free(dl->established_conns);
    dl->established_conns = ossl_dgram_conn_lookup_new_addr();

    ossl_crypto_mutex_unlock(dl->mutex);
}

size_t ossl_dtls_get_accept_connection_queue_len(SSL *ssl)
{
    DTLS_LISTENER *dl;
    size_t len;

    if (!IS_DTLS_LISTENER(ssl))
        return 0;

    dl = (DTLS_LISTENER *)ssl;

    ossl_crypto_mutex_lock(dl->mutex);
    len = (size_t)sk_SSL_num(dl->incoming_connections);
    ossl_crypto_mutex_unlock(dl->mutex);

    return len;
}

/*
 * Set an override time callback for the DTLS listener.
 * This is primarily for testing purposes to allow time injection.
 * If now_cb is NULL, the listener will use ossl_time_now().
 */
int ossl_dtls_listener_set_override_now_cb(SSL *s,
    OSSL_TIME (*now_cb)(void *arg),
    void *now_cb_arg)
{
    DTLS_LISTENER *dl;

    if (!IS_DTLS_LISTENER(s))
        return 0;

    dl = (DTLS_LISTENER *)s;
    dl->now_cb = now_cb;
    dl->now_cb_arg = now_cb_arg;

    return 1;
}

/*
 * ossl_dtls_get_value_uint - read a tunable value from a DTLS listener.
 *
 * DTLS-side implementation backing SSL_get_value_uint(3) when the target
 * SSL object is a DTLS listener created by SSL_new_listener().
 *
 * Supported (id) values:
 *   SSL_VALUE_DTLS_LISTENER_MAX_PENDING_CONNS
 *       Current cap on the number of pending (handshake-in-progress)
 *       connections the listener will track.
 *   SSL_VALUE_DTLS_LISTENER_PENDING_TIMEOUT
 *       Current reap timeout for pending connections, in milliseconds.
 *       UINT64_MAX means "infinite / disabled".
 *   SSL_VALUE_DTLS_LISTENER_MAX_DGRAM_SIZE
 *       Maximum size in bytes of a datagram the listener will receive.
 *
 * Only SSL_VALUE_CLASS_GENERIC is accepted for class_; other
 * classes are rejected with a return of 0
 *
 * Parameters:
 *   s      - listener SSL. Must satisfy IS_DTLS_LISTENER(s).
 *   class_ - value class; must be SSL_VALUE_CLASS_GENERIC.
 *   id     - one of the SSL_VALUE_DTLS_LISTENER_* ids listed above.
 *   value  - out-parameter receiving the current value. Must be non-NULL.
 *
 * Returns:
 *   1 on success (*value populated).
 *   0 on failure (unsupported id, wrong class, NULL value, or not a
 *     DTLS listener).
 */
int ossl_dtls_get_value_uint(SSL *s, uint32_t class_, uint32_t id, uint64_t *value)
{
    DTLS_LISTENER *dl;
    int ret = 1;

    if (!IS_DTLS_LISTENER(s)) {
        ERR_raise(ERR_LIB_SSL, SSL_R_LISTENER_USE_ONLY);
        return 0;
    }
    if (class_ != SSL_VALUE_CLASS_GENERIC) {
        ERR_raise(ERR_LIB_SSL, SSL_R_UNSUPPORTED_CONFIG_VALUE_CLASS);
        return 0;
    }
    if (value == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }

    dl = (DTLS_LISTENER *)s;

    ossl_crypto_mutex_lock(dl->mutex);

    switch (id) {
    case SSL_VALUE_DTLS_LISTENER_MAX_PENDING_CONNS:
        *value = (uint64_t)dl->max_pending_conns;
        break;
    case SSL_VALUE_DTLS_LISTENER_PENDING_TIMEOUT:
        if (ossl_time_is_infinite(dl->pending_timeout))
            *value = UINT64_MAX;
        else
            *value = ossl_time2ms(dl->pending_timeout);
        break;
    case SSL_VALUE_DTLS_LISTENER_MAX_DGRAM_SIZE:
        *value = (uint64_t)dl->max_dgram_size;
        break;
    default:
        ERR_raise(ERR_LIB_SSL, SSL_R_UNSUPPORTED_CONFIG_VALUE);
        ret = 0;
        break;
    }

    ossl_crypto_mutex_unlock(dl->mutex);
    return ret;
}

/*
 * ossl_dtls_set_value_uint - write a tunable value on a DTLS listener.
 *
 * DTLS-side implementation backing SSL_set_value_uint(3) when the target
 * SSL object is a DTLS listener created by SSL_new_listener().
 *
 * Supported (id) values -- see ossl_dtls_get_value_uint() above.
 *
 * Per-id policy:
 *   SSL_VALUE_DTLS_LISTENER_MAX_PENDING_CONNS
 *       value == 0 is rejected (a zero cap would reject every incoming
 *       connection). Values larger than SIZE_MAX are clamped to SIZE_MAX
 *       to avoid silent truncation on 32-bit builds.
 *   SSL_VALUE_DTLS_LISTENER_PENDING_TIMEOUT
 *       Interpreted as milliseconds. value == 0 is rejected. UINT64_MAX is
 *       treated as "infinite / disabled".
 *   SSL_VALUE_DTLS_LISTENER_MAX_DGRAM_SIZE
 *       Clamped to the maximum UDP payload (DTLS_LISTENER_MAX_DGRAM_SIZE);
 *       values below the demux minimum receive size are rejected.
 *
 * Only SSL_VALUE_CLASS_GENERIC is accepted for class_.
 *
 * Parameters:
 *   s      - listener SSL. Must satisfy IS_DTLS_LISTENER(s).
 *   class_ - value class; must be SSL_VALUE_CLASS_GENERIC.
 *   id     - one of the SSL_VALUE_DTLS_LISTENER_* ids.
 *   value  - new value to store, in the units documented per id.
 *
 * Returns:
 *   1 on success.
 *   0 on failure (unsupported id, wrong class, not a DTLS listener,
 *     or policy rejection such as 0 on the cap).
 */
int ossl_dtls_set_value_uint(SSL *s, uint32_t class_, uint32_t id, uint64_t value)
{
    DTLS_LISTENER *dl;
    int ret = 1;

    if (!IS_DTLS_LISTENER(s)) {
        ERR_raise(ERR_LIB_SSL, SSL_R_LISTENER_USE_ONLY);
        return 0;
    }
    if (class_ != SSL_VALUE_CLASS_GENERIC) {
        ERR_raise(ERR_LIB_SSL, SSL_R_UNSUPPORTED_CONFIG_VALUE_CLASS);
        return 0;
    }

    dl = (DTLS_LISTENER *)s;

    ossl_crypto_mutex_lock(dl->mutex);

    switch (id) {
    case SSL_VALUE_DTLS_LISTENER_MAX_PENDING_CONNS:
        if (value == 0) {
            /* A zero cap would reject every connection (num_items >= 0). */
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
            ret = 0;
            break;
        }
        /* Clamp to SIZE_MAX to prevent silent truncation on 32-bit. */
        dl->max_pending_conns = (value > SIZE_MAX) ? SIZE_MAX : (size_t)value;
        break;
    case SSL_VALUE_DTLS_LISTENER_PENDING_TIMEOUT:
        if (value == 0) {
            /*
             * A zero timeout will remove all pending connections on the next
             * tick, before the handshake could complete.
             */
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
            ret = 0;
            break;
        }
        /*
         * ossl_ms2time() scales by OSSL_TIME_MS (10^6 ns/ms), so any value
         * above UINT64_MAX / OSSL_TIME_MS would overflow the product and
         * silently wrap to a tiny timeout. Treat those (which includes the
         * UINT64_MAX "infinite" sentinel) as an infinite timeout.
         */
        if (value > UINT64_MAX / OSSL_TIME_MS)
            dl->pending_timeout = ossl_time_infinite();
        else
            dl->pending_timeout = ossl_ms2time(value);
        break;
    case SSL_VALUE_DTLS_LISTENER_MAX_DGRAM_SIZE:
        /* Nothing larger than the maximum UDP payload can ever arrive. */
        if (value > DTLS_LISTENER_MAX_DGRAM_SIZE)
            value = DTLS_LISTENER_MAX_DGRAM_SIZE;
        /* set_mtu returns 0 (rejecting) for values below the demux minimum. */
        if (!ossl_dgram_demux_set_mtu(dl->demux, (unsigned int)value)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
            ret = 0;
        } else {
            dl->max_dgram_size = (size_t)value;
        }
        break;
    default:
        ERR_raise(ERR_LIB_SSL, SSL_R_UNSUPPORTED_CONFIG_VALUE);
        ret = 0;
        break;
    }

    ossl_crypto_mutex_unlock(dl->mutex);
    return ret;
}

/*
 * Resolve the requested blocking mode of a DTLS listener, or of a connection
 * created from one, following the inheritance chain.
 *
 * A connection set to INHERIT follows its listener; a listener set to INHERIT
 * is blocking, there being nothing further to inherit from. Blocking is
 * therefore the default unless the application asks otherwise.
 *
 * Returns 1 if blocking is wanted, which says nothing about whether it can be
 * provided - see ossl_dtls_can_support_blocking().
 */
static int ossl_dtls_desires_blocking(const SSL *s)
{
    const SSL_CONNECTION *sc = SSL_CONNECTION_FROM_CONST_SSL_ONLY(s);
    const DTLS_LISTENER *dl = NULL;

    if (sc != NULL && sc->d1 != NULL) {
        /* The listener is driving this connection; it must not block. */
        if (sc->listener_force_nonblocking)
            return 0;

        if (sc->d1->req_blocking_mode != DTLS_BLOCKING_MODE_INHERIT)
            return sc->d1->req_blocking_mode == DTLS_BLOCKING_MODE_BLOCKING;

        dl = (const DTLS_LISTENER *)sc->d1->listener;
    } else if (IS_DTLS_LISTENER(s)) {
        dl = (const DTLS_LISTENER *)s;
    }

    if (dl == NULL)
        return 0;

    return dl->req_blocking_mode != DTLS_BLOCKING_MODE_NONBLOCKING;
}

/*
 * Report whether blocking mode can be provided for a DTLS listener or a
 * connection created from one.
 *
 * Blocking is emulated by waiting for readiness of the listener's network
 * socket, so it requires a BIO which can supply a poll descriptor to wait on.
 * A memory BIO cannot, and such a listener is therefore non-blocking whatever
 * was requested, as is the case for QUIC.
 */
static int ossl_dtls_can_support_blocking(const SSL *s)
{
    const SSL_CONNECTION *sc = SSL_CONNECTION_FROM_CONST_SSL_ONLY(s);
    const SSL *listener = NULL;
    BIO_POLL_DESCRIPTOR desc;
    BIO *rbio;

    if (sc != NULL && sc->d1 != NULL)
        listener = sc->d1->listener;
    else if (IS_DTLS_LISTENER(s))
        listener = s;

    if (listener == NULL)
        return 0;

    rbio = SSL_get_rbio(listener);
    if (rbio == NULL)
        return 0;

    return BIO_get_rpoll_descriptor(rbio, &desc) != 0
        && desc.type == BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD;
}

/*
 * Report whether a call on this object should block, which is the case when
 * blocking is both wanted and possible.
 */
int ossl_dtls_blocking(const SSL *s)
{
    return ossl_dtls_desires_blocking(s) && ossl_dtls_can_support_blocking(s);
}

int ossl_dtls_set_blocking_mode(SSL *s, int blocking)
{
    SSL_CONNECTION *sc = SSL_CONNECTION_FROM_SSL_ONLY(s);
    unsigned int mode = (blocking != 0)
        ? DTLS_BLOCKING_MODE_BLOCKING
        : DTLS_BLOCKING_MODE_NONBLOCKING;

    /*
     * Only a listener, or a connection created from one, has a blocking mode.
     * Any other DTLS object takes its behaviour from its own BIO in the
     * traditional way, so there is nothing here to configure.
     */
    if (!IS_DTLS_LISTENER(s)
        && (sc == NULL || sc->d1 == NULL || sc->d1->listener == NULL)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }

    /*
     * Refuse to claim blocking we cannot deliver, as QUIC does. Checked before
     * anything is written, so that a call which fails leaves the mode alone
     * rather than reporting failure having already changed it.
     */
    if (blocking && !ossl_dtls_can_support_blocking(s)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_UNSUPPORTED);
        return 0;
    }

    if (IS_DTLS_LISTENER(s))
        ((DTLS_LISTENER *)s)->req_blocking_mode = mode;
    else
        sc->d1->req_blocking_mode = mode;

    return 1;
}

int ossl_dtls_get_blocking_mode(const SSL *s)
{
    const SSL_CONNECTION *sc = SSL_CONNECTION_FROM_CONST_SSL_ONLY(s);

    if (!IS_DTLS_LISTENER(s)
        && (sc == NULL || sc->d1 == NULL || sc->d1->listener == NULL))
        return -1;

    return ossl_dtls_blocking(s);
}

/*
 * Wait until a datagram has been demultiplexed to this connection's receive
 * queue, for a connection which is in blocking mode.
 *
 * This is what makes a blocking read on a listener based connection block. Such
 * a connection has no BIO of its own to block in: it reads from a queue which
 * the listener fills, so the wait has to happen here instead.
 *
 * A wakeup does not mean the datagram was ours - the listener's socket is
 * shared, and another connection may be the one with data - so this loops until
 * something actually lands in our queue. Events are handled after each wait
 * because nothing else will do it while we are in here, and the retransmission
 * timer needs servicing if it is what woke us.
 *
 * A datagram which is already waiting costs nothing: the wait pumps the
 * listener's demux while translating the poll, and returns without sleeping if
 * anything has been queued for us by then.
 *
 * Returns 1 if a datagram is now queued for this connection, or 0 if the wait
 * could not be performed or the listener has failed.
 */
int ossl_dtls_conn_wait_for_datagram(SSL *s)
{
    SSL_CONNECTION *sc = SSL_CONNECTION_FROM_SSL_ONLY(s);
    DTLS_LISTENER *dl;
    int empty;

    if (sc == NULL || sc->d1 == NULL || sc->d1->rx == NULL
        || sc->d1->listener == NULL)
        return 0;

    dl = (DTLS_LISTENER *)sc->d1->listener;

    for (;;) {
        ossl_crypto_mutex_lock(dl->mutex);
        if (dl->fatal) {
            ossl_crypto_mutex_unlock(dl->mutex);
            return 0;
        }
        ossl_crypto_mutex_unlock(dl->mutex);

        /*
         * An infinite deadline here is bounded by the connection's own event
         * timeout, which the poll translation folds in, so this still wakes in
         * time to retransmit.
         */
        if (!ossl_dtls_block_until_ready(s, SSL_POLL_EVENT_R,
                ossl_time_infinite(), /*bound_by_event_timeout=*/1))
            return 0;

        if (!SSL_handle_events(s))
            return 0;

        ossl_dgram_demux_pump(sc->d1->rx->demux);

        ossl_crypto_mutex_lock(sc->d1->rx->mutex);
        empty = ossl_list_urxe_is_empty(&sc->d1->rx->urxe_pending);
        ossl_crypto_mutex_unlock(sc->d1->rx->mutex);

        if (!empty)
            return 1;
    }
}

/*
 * Wait until the listener's socket can accept another datagram, for a
 * connection which is in blocking mode.
 *
 * The socket is shared with every other connection and is always
 * non-blocking, so a send which cannot be completed has nowhere to wait. For
 * DTLS the record layer would otherwise discard the datagram - a reasonable
 * default for an unreliable transport, but not what an application which asked
 * for blocking writes expects.
 *
 * Only one wait is performed. The caller retries the send, and comes back here
 * if it still cannot proceed, so a wakeup which turns out not to leave room in
 * the socket buffer costs an extra attempt rather than a lost datagram.
 *
 * The retransmission timer deliberately does not shorten this wait, unlike the
 * one for a datagram above. There the wakeup is useful, because the wait can
 * service the timer itself; here it cannot. Servicing it would mean
 * retransmitting a flight from inside tls_retry_write_records(), which is
 * part-way through sending one and holds write buffer state that a
 * re-entrant do_dtls1_write() would clobber. Waking for a timer nothing then
 * services would be worse than not waking: the timeout stays expired, and an
 * expired timeout reads as a zero deadline, so every later wait would return
 * at once and the caller's retry loop would spin without sleeping. Waiting for
 * the socket alone is also what the send actually needs. Retransmission is not
 * the right response to a flight which has not finished going out, and once it
 * has, the state machine handles the timer as usual.
 *
 * Returns 1 if the send should be retried, or 0 if the wait could not be
 * performed or the listener has failed.
 */
int ossl_dtls_conn_wait_for_write(SSL *s)
{
    SSL_CONNECTION *sc = SSL_CONNECTION_FROM_SSL_ONLY(s);
    DTLS_LISTENER *dl;
    int fatal;

    if (sc == NULL || sc->d1 == NULL || sc->d1->listener == NULL)
        return 0;

    dl = (DTLS_LISTENER *)sc->d1->listener;

    ossl_crypto_mutex_lock(dl->mutex);
    fatal = dl->fatal;
    ossl_crypto_mutex_unlock(dl->mutex);
    if (fatal)
        return 0;

    return ossl_dtls_block_until_ready(s, SSL_POLL_EVENT_W,
        ossl_time_infinite(), /*bound_by_event_timeout=*/0);
}

void ossl_dtls_listener_enter_blocking_section(SSL *s)
{
    DTLS_LISTENER *dl;

    if (!IS_DTLS_LISTENER(s))
        return;

    dl = (DTLS_LISTENER *)s;

    if (dl->have_notifier) {
        ossl_crypto_mutex_lock(dl->mutex);
        dl->cur_blocking_waiters++;
        ossl_crypto_mutex_unlock(dl->mutex);
    }
}

void ossl_dtls_listener_leave_blocking_section(SSL *s)
{
    DTLS_LISTENER *dl;

    if (!IS_DTLS_LISTENER(s))
        return;

    dl = (DTLS_LISTENER *)s;

    if (dl->have_notifier) {
        ossl_crypto_mutex_lock(dl->mutex);

        assert(dl->cur_blocking_waiters > 0);
        --dl->cur_blocking_waiters;

        if (dl->signalled_notifier) {
            if (dl->cur_blocking_waiters == 0) {
                ossl_rio_notifier_unsignal(&dl->notifier);
                dl->signalled_notifier = 0;

                /*
                 * Release the other threads which have woken up
                 */
                ossl_crypto_condvar_broadcast(dl->notifier_cv);
            } else {
                /* We are not the last waiter out - so wait for that one. */
                while (dl->signalled_notifier)
                    ossl_crypto_condvar_wait(dl->notifier_cv, dl->mutex);
            }
        }

        ossl_crypto_mutex_unlock(dl->mutex);
    }
}

int ossl_dtls_listener_poll_events(SSL *s, uint64_t events, int do_tick,
    uint64_t *revents)
{
    DTLS_LISTENER *dl;
    uint64_t result = 0;

    if (!ossl_assert(IS_DTLS_LISTENER(s)))
        return 0;

    dl = (DTLS_LISTENER *)s;

    if (do_tick)
        ossl_dtls_tick(dl);

    if ((events & SSL_POLL_EVENT_IC) != 0) {
        if (SSL_get_accept_connection_queue_len(s) > 0)
            result |= SSL_POLL_EVENT_IC;
    }

    if ((events & SSL_POLL_EVENT_R) != 0) {
        BIO *rbio = SSL_get_rbio(s);
        if (rbio != NULL && BIO_pending(rbio) > 0)
            result |= SSL_POLL_EVENT_R;
    }

    *revents = result;
    return 1;
}

int ossl_dtls_conn_poll_events(SSL *s, uint64_t events, int do_tick,
    uint64_t *revents)
{
    SSL_CONNECTION *sc;
    uint64_t result = 0;
    BIO_POLL_DESCRIPTOR desc;
    int has_pending;

    sc = SSL_CONNECTION_FROM_SSL(s);
    if (sc == NULL || sc->d1 == NULL)
        return 0;

    /*
     * For DTLS connections that came from a listener, data arrives via
     * URXEs injected by the listener's demux. When do_tick is set and
     * we have a listener reference, pump the demux to get new data.
     */
    if (do_tick && sc->d1->listener != NULL) {
        DTLS_LISTENER *dl = (DTLS_LISTENER *)sc->d1->listener;
        ossl_dtls_tick(dl);
    }

    /*
     * Handle events for the connection itself, which for DTLS means servicing
     * the retransmission timer. The caller may have blocked until that timer
     * expired, so if nothing retransmits here then nothing will, and the
     * deadline would be recomputed as "now" on every subsequent wait.
     *
     * A failure here leaves the connection in a fatal error state, which the
     * SSL_POLL_EVENT_EC check below reports.
     */
    if (do_tick)
        SSL_handle_events(s);

    if ((events & SSL_POLL_EVENT_R) != 0) {
        if (SSL_has_pending(s) || SSL_pending(s) > 0) {
            result |= SSL_POLL_EVENT_R;
        } else if (sc->d1->rx != NULL) {
            /* Listener-based connection: check URXE queue */
            ossl_crypto_mutex_lock(sc->d1->rx->mutex);
            has_pending = !ossl_list_urxe_is_empty(&sc->d1->rx->urxe_pending);
            ossl_crypto_mutex_unlock(sc->d1->rx->mutex);
            if (has_pending)
                result |= SSL_POLL_EVENT_R;
        } else {
            /*
             * Standalone DTLS SSL object (not from a listener).
             * Check the underlying socket for readability.
             */
            BIO *rbio = SSL_get_rbio(s);

            if (rbio != NULL) {
                if (BIO_get_rpoll_descriptor(rbio, &desc)
                    && desc.type == BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD
                    && desc.value.fd >= 0) {
                    if (BIO_socket_ready(desc.value.fd, 1) > 0)
                        result |= SSL_POLL_EVENT_R;
                } else {
                    /* Checking non-socket BIO */
                    if (BIO_pending(rbio) > 0)
                        result |= SSL_POLL_EVENT_R;
                }
            }
        }
    }

    if ((events & SSL_POLL_EVENT_W) != 0) {
        result |= SSL_POLL_EVENT_W;
    }

    if ((events & (SSL_POLL_EVENT_EC | SSL_POLL_EVENT_F)) != 0) {
        if (SSL_get_error(s, 0) == SSL_ERROR_SSL || SSL_get_shutdown(s) != 0)
            result |= SSL_POLL_EVENT_EC;
    }

    *revents = result;
    return 1;
}

#endif /* !OPENSSL_NO_DTLS && !OPENSSL_NO_SOCK */
