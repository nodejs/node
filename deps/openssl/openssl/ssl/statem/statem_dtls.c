/*
 * Copyright 2005-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "../ssl_local.h"
#include "statem_local.h"
#include "internal/cryptlib.h"
#include "internal/ssl_unwrap.h"
#include <openssl/buffer.h>

#define RSMBLY_BITMASK_SIZE(msg_len) (((msg_len) + 7) / 8)

#define RSMBLY_BITMASK_MARK(bitmask, start, end)                             \
    {                                                                        \
        if ((end) - (start) <= 8) {                                          \
            long ii;                                                         \
            for (ii = (start); ii < (end); ii++)                             \
                bitmask[((ii) >> 3)] |= (1 << ((ii) & 7));                   \
        } else {                                                             \
            long ii;                                                         \
            bitmask[((start) >> 3)] |= bitmask_start_values[((start) & 7)];  \
            for (ii = (((start) >> 3) + 1); ii < ((((end) - 1)) >> 3); ii++) \
                bitmask[ii] = 0xff;                                          \
            bitmask[(((end) - 1) >> 3)] |= bitmask_end_values[((end) & 7)];  \
        }                                                                    \
    }

#define RSMBLY_BITMASK_IS_COMPLETE(bitmask, msg_len, is_complete)                   \
    {                                                                               \
        long ii;                                                                    \
        is_complete = 1;                                                            \
        if (bitmask[(((msg_len) - 1) >> 3)] != bitmask_end_values[((msg_len) & 7)]) \
            is_complete = 0;                                                        \
        if (is_complete)                                                            \
            for (ii = (((msg_len) - 1) >> 3) - 1; ii >= 0; ii--)                    \
                if (bitmask[ii] != 0xff) {                                          \
                    is_complete = 0;                                                \
                    break;                                                          \
                }                                                                   \
    }

static const unsigned char bitmask_start_values[] = {
    0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80
};
static const unsigned char bitmask_end_values[] = {
    0xff, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f
};

static int dtls_get_reassembled_message(SSL_CONNECTION *s, int *errtype,
    size_t *len);

/*
 * Check if CCS is expected in current state.
 *
 * RFC 6347 Section 4.1 states DTLS must handle message reordering since UDP
 * does not guarantee in-order delivery. This function determines when a
 * buffered CCS should be delivered to the state machine.
 *
 * Server states where CCS is expected:
 *   - TLS_ST_SR_KEY_EXCH: After key exchange (anonymous or no_cert_verify)
 *   - TLS_ST_SR_CERT_VRFY: After certificate verify
 *   - TLS_ST_SW_FINISHED: Session resumption (abbreviated handshake)
 *
 * Client states where CCS is expected:
 *   - TLS_ST_CR_SRVR_HELLO: Abbreviated handshake without ticket
 *   - TLS_ST_CW_FINISHED: After sending Finished, before server CCS
 *   - TLS_ST_CR_SESSION_TICKET: After receiving session ticket
 */
static int dtls_ccs_expected(SSL_CONNECTION *s)
{
    OSSL_HANDSHAKE_STATE st = s->statem.hand_state;

    if (s->server) {
        switch (st) {
        case TLS_ST_SR_KEY_EXCH:
            /* Anonymous or no client cert: CCS follows KeyExchange */
            if (s->session->peer == NULL && s->session->peer_rpk == NULL)
                return 1;
            /* Client cert but no verify message required */
            return s->statem.no_cert_verify;
        case TLS_ST_SR_CERT_VRFY:
            return 1;
        case TLS_ST_SW_FINISHED:
            /* Abbreviated handshake: server sends first, then receives CCS */
            return s->hit;
        default:
            return 0;
        }
    } else {
        switch (st) {
        case TLS_ST_CR_SRVR_HELLO:
            /* Abbreviated handshake without session ticket */
            return (s->hit && !s->ext.ticket_expected);
        case TLS_ST_CW_FINISHED:
            /* Full handshake: waiting for server CCS after sending Finished */
            return !s->ext.ticket_expected;
        case TLS_ST_CR_SESSION_TICKET:
            return 1;
        default:
            return 0;
        }
    }
}

static dtls_sent_msg *dtls1_sent_msg_new(size_t msg_len, size_t body_len,
    int track_coverage)
{
    const size_t bitmask_len = (track_coverage && body_len > 0 ? RSMBLY_BITMASK_SIZE(body_len) : 0);
    dtls_sent_msg *msg = OPENSSL_malloc(sizeof(*msg) + msg_len + bitmask_len);

    if (msg == NULL)
        return NULL;

    memset(msg, 0, sizeof(*msg));

    /* zero length msg gets msg->msg_buf == NULL */
    if (msg_len > 0)
        msg->msg_buf = (unsigned char *)(msg + 1);

    /*
     * body_len > 0 implies msg_len > 0 (msg_len == body_len + headerlen, and
     * headerlen is never 0), so msg->msg_buf is already set here. Coverage
     * tracking only means anything for DTLS 1.3, the only version that ever
     * processes an ACK; older versions never read msg->covered, so don't pay
     * for the allocation on their behalf.
     */
    if (track_coverage && body_len > 0) {
        msg->covered = msg->msg_buf + msg_len;
        memset(msg->covered, 0, bitmask_len);
    }

    return msg;
}

void dtls1_sent_msg_free(dtls_sent_msg *msg)
{
    if (msg != NULL)
        ossl_list_record_number_elem_free(&msg->rec_nums);

    OPENSSL_free(msg);
}

static hm_fragment *dtls1_hm_fragment_new(size_t frag_len, int reassembly)
{
    const size_t bitmask_len = (reassembly ? RSMBLY_BITMASK_SIZE(frag_len) : 0);
    hm_fragment *frag = OPENSSL_malloc(sizeof(*frag) + frag_len + bitmask_len);

    if (frag == NULL)
        return NULL;

    memset(frag, 0, sizeof(*frag));

    /* zero length fragment gets zero frag->fragment */
    if (frag_len > 0)
        frag->fragment = (unsigned char *)(frag + 1);

    /* Initialize reassembly bitmask if necessary */
    if (bitmask_len > 0) {
        if (frag->fragment == NULL)
            frag->reassembly = (unsigned char *)(frag + 1);
        else
            frag->reassembly = frag->fragment + frag_len;

        memset(frag->reassembly, 0, bitmask_len);
    }

    return frag;
}

void dtls1_hm_fragment_free(hm_fragment *frag)
{
    OPENSSL_free(frag);
}

static int dtls1_write_hm_header(unsigned char *msgheaderstart,
    unsigned char msg_type, size_t msg_len,
    unsigned short msg_seq, size_t fragoff,
    size_t fraglen)
{
    WPACKET msgheader;
    size_t msgheaderlen;

    if (!WPACKET_init_static_len(&msgheader, msgheaderstart,
            DTLS1_HM_HEADER_LENGTH, 0)
        || !WPACKET_put_bytes_u8(&msgheader, msg_type)
        || !WPACKET_put_bytes_u24(&msgheader, msg_len)
        || !WPACKET_put_bytes_u16(&msgheader, msg_seq)
        || !WPACKET_put_bytes_u24(&msgheader, fragoff)
        || !WPACKET_put_bytes_u24(&msgheader, fraglen)
        || !WPACKET_get_total_written(&msgheader, &msgheaderlen)
        || msgheaderlen != DTLS1_HM_HEADER_LENGTH
        || !WPACKET_finish(&msgheader)) {
        WPACKET_cleanup(&msgheader);
        return 0;
    }

    return 1;
}

/*
 * send s->init_buf in records of type 'type' (SSL3_RT_HANDSHAKE,
 * SSL3_RT_CHANGE_CIPHER_SPEC or SSL3_RT_ACK)
 *
 * When sending a fragmented handshake message this function will re-use
 * s->init_buf->data but overwrite previously sent data to fill out the handshake
 * message header for the next fragment.
 *
 * E.g.
 * |-------------------------s->init_buf->data------------------------------|
 * |-- header1 --||-- fragment1 --|
 *                  |-- header2 --||-- fragment2 --|
 *                                   |-- header3 --||-- fragment3 --|
 *                                                                 .........
 */
int dtls1_do_write(SSL_CONNECTION *s, uint8_t recordtype)
{
    int ret;
    size_t written;
    size_t curr_mtu;
    int retry = 1;
    size_t len, overhead, used_len;
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);
    SSL *ussl = SSL_CONNECTION_GET_USER_SSL(s);
    uint8_t saved_payload[DTLS1_HM_HEADER_LENGTH];
    /* msg_len, msg_seq, msg_type are only used for recordtype == SSL3_RT_HANDSHAKE */
    const size_t msg_len = s->d1->w_msg.msg_body_len;
    const unsigned short msg_seq = s->d1->w_msg.msg_seq;
    const unsigned char msg_type = s->d1->w_msg.msg_type;
    const size_t min_len = recordtype == SSL3_RT_ACK
        ? DTLS13_ACK_MIN_NONEMPTY_LEN
        : DTLS1_HM_HEADER_LENGTH + 1;

    if (!dtls1_query_mtu(s))
        return -1;

    if (s->d1->mtu < dtls1_min_mtu(s))
        /* should have something reasonable now */
        return -1;

    if (s->init_off == 0 && recordtype == SSL3_RT_HANDSHAKE) {
        if (!ossl_assert(s->init_num == msg_len + DTLS1_HM_HEADER_LENGTH))
            return -1;
    }

    overhead = s->rlayer.wrlmethod->get_max_record_overhead(s->rlayer.wrl);

    s->rwstate = SSL_NOTHING;

    /* s->init_num shouldn't ever be < 0...but just in case */
    while (s->init_num > 0) {
        unsigned char *msgstart;

        if (recordtype == SSL3_RT_HANDSHAKE && s->init_off > 0) {
            /*
             * We must be writing a fragment other than the first one
             * and this is the first attempt at writing out this fragment
             */
            if (s->init_off <= DTLS1_HM_HEADER_LENGTH) {
                /*
                 * Each fragment that was already sent must at least have
                 * contained the message header plus one other byte.
                 * Therefore |init_off| must have progressed by at least
                 * |DTLS1_HM_HEADER_LENGTH + 1| bytes. If not something went
                 * wrong.
                 */
                return -1;
            }

            /*
             * Adjust |init_off| and |init_num| to allow room for a new
             * message header for this fragment.
             */
            s->init_off -= DTLS1_HM_HEADER_LENGTH;
            s->init_num += DTLS1_HM_HEADER_LENGTH;
        }

        used_len = BIO_wpending(s->wbio) + overhead;
        if (s->d1->mtu > used_len)
            curr_mtu = s->d1->mtu - used_len;
        else
            curr_mtu = 0;

        if (curr_mtu < min_len) {
            /*
             * grr.. we could get an error if MTU picked was wrong
             */
            ret = BIO_flush(s->wbio);
            if (ret <= 0) {
                s->rwstate = SSL_WRITING;
                return ret;
            }
            if (s->d1->mtu >= overhead + min_len) {
                curr_mtu = s->d1->mtu - overhead;
            } else {
                /* Shouldn't happen */
                return -1;
            }
        }

        if (s->init_num > curr_mtu)
            len = curr_mtu;
        else
            len = s->init_num;

        if (len > ssl_get_max_send_fragment(s))
            len = ssl_get_max_send_fragment(s);

        /*
         * For DTLS1.3 and padding lets update the max fragment size
         * accordingly
         */
        if (SSL_CONNECTION_IS_DTLS13(s))
            s->rlayer.wrlmethod->set_curr_mtu(s->rlayer.wrl, curr_mtu);

        msgstart = (unsigned char *)&s->init_buf->data[s->init_off];

        if (recordtype == SSL3_RT_ACK) {
            /* Each record needs a complete vector of record numbers. */
            if (!ossl_assert(len >= DTLS13_ACK_HEADER_LEN))
                return -1;
            len = DTLS13_ACK_HEADER_LEN
                + ((len - DTLS13_ACK_HEADER_LEN) / DTLS13_RECORD_NUMBER_LEN)
                    * DTLS13_RECORD_NUMBER_LEN;
            msgstart[0] = (unsigned char)((len - DTLS13_ACK_HEADER_LEN) >> 8);
            msgstart[1] = (unsigned char)(len - DTLS13_ACK_HEADER_LEN);
        }

        if (recordtype == SSL3_RT_HANDSHAKE) {
            const size_t fragoff = s->init_off;
            const size_t fraglen = len - DTLS1_HM_HEADER_LENGTH;

            /*
             * Save the data that will be overwritten by
             * the following code so no corruption occurs when using
             * a msg callback.
             */
            if (s->msg_callback && s->init_off != 0)
                memcpy(saved_payload, msgstart, sizeof(saved_payload));

            if (len < DTLS1_HM_HEADER_LENGTH
                || !dtls1_write_hm_header(msgstart, msg_type, msg_len,
                    msg_seq, fragoff, fraglen))
                /*
                 * len is so small that we really can't do anything sensible
                 * so fail
                 */
                return -1;

            /*
             * Recorded so do_dtls1_write() (rec_layer_d1.c) can tag this
             * fragment's record number with the byte range it covers.
             */
            s->d1->w_frag_off = fragoff;
            s->d1->w_frag_len = fraglen;
        }

        ret = dtls1_write_bytes(s, recordtype, msgstart, len,
            &written);

        if (recordtype == SSL3_RT_HANDSHAKE && s->msg_callback && s->init_off != 0)
            memcpy(msgstart, saved_payload, sizeof(saved_payload));

        if (ret <= 0) {
            /*
             * might need to update MTU here, but we don't know which
             * previous packet caused the failure -- so can't really
             * retransmit anything.  continue as if everything is fine and
             * wait for an alert to handle the retransmit
             */
            if (retry && BIO_ctrl(SSL_get_wbio(ssl), BIO_CTRL_DGRAM_MTU_EXCEEDED, 0, NULL) > 0
                && !(SSL_get_options(ssl) & SSL_OP_NO_QUERY_MTU)
                && dtls1_query_mtu(s))
                /* Have one more go */
                retry = 0;
            else
                return -1;
        } else {

            /*
             * bad if this assert fails, only part of the handshake message
             * got sent.  but why would this happen?
             */
            if (!ossl_assert(len == written))
                return -1;

            /*
             * We should not exceed the MTU size. If compression is in use
             * then the max record overhead calculation is unreliable so we do
             * not check in that case. We use assert rather than ossl_assert
             * because in a production build, if this assert were ever to fail,
             * then the best thing to do is probably carry on regardless.
             */
            assert(s->s3.tmp.new_compression != NULL
                || BIO_wpending(s->wbio) <= (int)s->d1->mtu);

            if (recordtype == SSL3_RT_HANDSHAKE && !s->d1->retransmitting
                && s->init_off == 0) {
                size_t xlen = s->init_num;

                if (s->version == DTLS1_BAD_VER) {
                    msgstart += DTLS1_HM_HEADER_LENGTH;
                    xlen -= DTLS1_HM_HEADER_LENGTH;
                } else {
                    /*
                     * Now prepare to calculate the transcript hash. For
                     * versions prior to DTLSv1.3 this means:
                     *
                     * rfc6347: Hash calculations include entire handshake
                     *   messages, including DTLS-specific fields: message_seq,
                     *   fragment_offset, and fragment_length. However, the
                     *   Finished MAC MUST be computed as if each handshake
                     *   message had been sent as a single fragment.
                     *
                     * For DTLSv1.3 DTLS-specific fields: message_seq,
                     * fragment_offset, and fragment_length are not used in the
                     * calculation i.e. the MAC-calculation is the same as TLS
                     * even though DTLS handshake messages may be fragmented.
                     */
                    if (!dtls1_write_hm_header(msgstart, msg_type, msg_len,
                            msg_seq, 0, msg_len))
                        return -1;
                }

                /*
                 * should not be done for 'Hello Request's, but in that case we'll
                 * ignore the result anyway
                 * DTLS1.3 KeyUpdate and NewSessionTicket do not need to be added
                 */
                if (!SSL_CONNECTION_IS_DTLS13(s)
                    || (s->statem.hand_state != TLS_ST_SW_SESSION_TICKET
                        && s->statem.hand_state != TLS_ST_CW_KEY_UPDATE
                        && s->statem.hand_state != TLS_ST_SW_KEY_UPDATE))
                    if (!ssl3_finish_mac(s, msgstart, xlen))
                        return -1;
            }
            if (recordtype == SSL3_RT_ACK && s->msg_callback != NULL)
                s->msg_callback(1, s->version, recordtype, msgstart, written,
                    ussl, s->msg_callback_arg);

            if (written == s->init_num) {
                if (s->msg_callback && recordtype != SSL3_RT_ACK)
                    s->msg_callback(1, s->version, recordtype, s->init_buf->data,
                        s->init_off + s->init_num, ussl,
                        s->msg_callback_arg);

                s->init_off = 0; /* done writing this message */
                s->init_num = 0;

                return 1;
            }
            /* Reuse the last two sent bytes for the next ACK vector length. */
            if (recordtype == SSL3_RT_ACK)
                written -= DTLS13_ACK_HEADER_LEN;
            s->init_off += written;
            s->init_num -= written;
        }
    }
    return 0;
}

int dtls_get_message(SSL_CONNECTION *s, int *mt)
{
    unsigned char *rec_data;
    size_t tmplen;
    int errtype;

    s->d1->r_msg_seq = 0;

again:
    if (!dtls_get_reassembled_message(s, &errtype, &tmplen)) {
        if (s->statem.ack_for_retransmit)
            return 0;
        if (errtype == DTLS1_HM_BAD_FRAGMENT
            || errtype == DTLS1_HM_FRAGMENT_RETRY) {
            /* bad fragment received */
            goto again;
        }
        return 0;
    }

    *mt = s->s3.tmp.message_type;

    rec_data = (unsigned char *)s->init_buf->data;

    /*
     * If this isn't a real handshake message skip the processing below.
     */
    if (*mt == SSL3_MT_CHANGE_CIPHER_SPEC || *mt == DTLS13_MT_ACK)
        return 1;

    /* reconstruct message header */
    dtls1_write_hm_header(rec_data, s->s3.tmp.message_type, s->s3.tmp.message_size,
        s->d1->r_msg_seq, 0, s->s3.tmp.message_size);

    s->d1->r_msg_seq = 0;

    s->d1->handshake_read_seq++;

    s->init_msg = s->init_buf->data + DTLS1_HM_HEADER_LENGTH;

    return 1;
}

/*
 * Actually we already have the message body - but this is an opportunity for
 * DTLS to do any further processing it wants at the same point that TLS would
 * be asked for the message body.
 */
int dtls_get_message_body(SSL_CONNECTION *s, size_t *len)
{
    unsigned char *msg = (unsigned char *)s->init_buf->data;
    size_t msg_len;
    int recordtype;

    switch (s->s3.tmp.message_type) {
    default:
        recordtype = SSL3_RT_HANDSHAKE;
        msg_len = s->init_num + DTLS1_HM_HEADER_LENGTH;

        break;
    case DTLS13_MT_ACK:
        recordtype = SSL3_RT_ACK;
        msg_len = s->init_num;

        goto end;
    case SSL3_MT_CHANGE_CIPHER_SPEC:
        recordtype = SSL3_RT_CHANGE_CIPHER_SPEC;
        msg_len = 1;

        goto end;
    }

    /*
     * If receiving Finished, record MAC of prior handshake messages for
     * Finished verification.
     */
    if (*(s->init_buf->data) == SSL3_MT_FINISHED && !ssl3_take_mac(s)) {
        /* SSLfatal() already called */
        return 0;
    }

    if (!tls_common_finish_mac(s))
        return 0;

end:
    if (s->msg_callback)
        s->msg_callback(0, s->version, recordtype, msg, msg_len,
            SSL_CONNECTION_GET_USER_SSL(s), s->msg_callback_arg);

    *len = s->init_num;
    return 1;
}

/*
 * dtls1_max_handshake_message_len returns the maximum number of bytes
 * permitted in a DTLS handshake message for |s|. The minimum is 16KB, but
 * may be greater if the maximum certificate list size requires it.
 */
static size_t dtls1_max_handshake_message_len(const SSL_CONNECTION *s)
{
    size_t max_len = DTLS1_HM_HEADER_LENGTH + SSL3_RT_MAX_ENCRYPTED_LENGTH;
    if (max_len < s->max_cert_list)
        return s->max_cert_list;
    return max_len;
}

static int dtls1_preprocess_fragment(SSL_CONNECTION *s,
    const struct hm_header_st *const msg_hdr)
{
    size_t frag_off, frag_len, msg_len;

    msg_len = msg_hdr->msg_len;
    frag_off = msg_hdr->frag_off;
    frag_len = msg_hdr->frag_len;

    /* sanity checking */
    if ((frag_off + frag_len) > msg_len
        || msg_len > dtls1_max_handshake_message_len(s)) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_EXCESSIVE_MESSAGE_SIZE);
        return 0;
    }

    /*
     * msg_len is limited to 2^24, but is effectively checked against
     * dtls_max_handshake_message_len(s) above
     */
    if (!BUF_MEM_grow_clean(s->init_buf, msg_len + DTLS1_HM_HEADER_LENGTH)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_BUF_LIB);
        return 0;
    }

    s->s3.tmp.message_size = msg_len;
    s->s3.tmp.message_type = msg_hdr->type;
    s->d1->r_msg_seq = msg_hdr->seq;

    return 1;
}

static int add_record_to_ack_list(SSL_CONNECTION *sc)
{
    DTLS1_RECORD_NUMBER *recnum;
    uint64_t epoch = sc->s3.tmp.record_epoch;
    uint64_t sequence = sc->s3.tmp.record_seq_num;

    /*
     * Retain at most one maximum-sized ACK record's worth of record numbers.
     * Check before the duplicate scan to bound the work once the list is full.
     * Excess records may be omitted from ACKs (RFC 9147, section 7.1).
     */
    if (ossl_list_record_number_num(&sc->d1->ack_rec_num)
        >= (SSL3_RT_MAX_PLAIN_LENGTH - DTLS13_ACK_HEADER_LEN) / DTLS13_RECORD_NUMBER_LEN)
        return 1;

    for (recnum = ossl_list_record_number_head(&sc->d1->ack_rec_num);
        recnum != NULL;
        recnum = ossl_list_record_number_next(recnum)) {
        /* Is the record number already in the list? */
        if (recnum->epoch == epoch && recnum->seqnum == sequence)
            return 1;
    }

    recnum = dtls1_record_number_new(epoch, sequence, 0, 0);

    if (recnum == NULL)
        return 0;

    ossl_list_record_number_insert_tail(&sc->d1->ack_rec_num, recnum);

    return 1;
}

/*
 * Returns 1 if there is a buffered fragment available, 0 if not, or -1 on a
 * fatal error.
 */
static int dtls1_retrieve_buffered_fragment(SSL_CONNECTION *s, size_t *len)
{
    /*-
     * (0) check whether the desired fragment is available
     * if so:
     * (1) copy over the fragment to s->init_buf->data[]
     * (2) update s->init_num
     */
    pitem *item;
    piterator iter;
    hm_fragment *frag;
    int ret;
    int chretran = 0;
    pqueue *rcvd_messages = &s->d1->rcvd_messages;

    iter = pqueue_iterator(rcvd_messages);
    do {
        item = pqueue_next(&iter);
        if (item == NULL)
            return 0;

        frag = (hm_fragment *)item->data;

        if (frag->msg_header.seq < s->d1->handshake_read_seq) {
            pitem *next;
            hm_fragment *nextfrag;

            if (!s->server
                || frag->msg_header.seq != 0
                || s->d1->handshake_read_seq != 1
                || s->statem.hand_state != DTLS_ST_SW_HELLO_VERIFY_REQUEST) {
                /*
                 * This is a stale message that has been buffered so clear it.
                 * It is safe to pop this message from the queue even though
                 * we have an active iterator
                 */
                pqueue_pop(rcvd_messages);
                dtls1_hm_fragment_free(frag);
                pitem_free(item);
                item = NULL;
                frag = NULL;
            } else {
                /*
                 * We have fragments for a ClientHello without a cookie,
                 * even though we have sent a HelloVerifyRequest. It is possible
                 * that the HelloVerifyRequest got lost and this is a
                 * retransmission of the original ClientHello
                 */
                next = pqueue_next(&iter);
                if (next != NULL) {
                    nextfrag = (hm_fragment *)next->data;
                    if (nextfrag->msg_header.seq == s->d1->handshake_read_seq) {
                        /*
                         * We have fragments for both a ClientHello without
                         * cookie and one with. Ditch the one without.
                         */
                        pqueue_pop(rcvd_messages);
                        dtls1_hm_fragment_free(frag);
                        pitem_free(item);
                        item = next;
                        frag = nextfrag;
                    } else {
                        chretran = 1;
                    }
                } else {
                    chretran = 1;
                }
            }
        }
    } while (item == NULL);

    /* Don't return if reassembly still in progress */
    if (frag->reassembly != NULL)
        return 0;

    if (s->d1->handshake_read_seq == frag->msg_header.seq || chretran) {
        size_t frag_len = frag->msg_header.frag_len;

        pqueue_pop(rcvd_messages);

        /* Calls SSLfatal() as required */
        ret = dtls1_preprocess_fragment(s, &frag->msg_header);

        if (ret && frag->msg_header.frag_len > 0) {
            unsigned char *p = (unsigned char *)s->init_buf->data + DTLS1_HM_HEADER_LENGTH;
            memcpy(&p[frag->msg_header.frag_off], frag->fragment,
                frag->msg_header.frag_len);
        }

        dtls1_hm_fragment_free(frag);
        pitem_free(item);

        if (ret) {
            if (chretran) {
                /*
                 * We got a new ClientHello with a message sequence of 0.
                 * Reset the read/write sequences back to the beginning.
                 * We process it like this is the first time we've seen a
                 * ClientHello from the client.
                 */
                s->d1->handshake_read_seq = 0;
                s->d1->next_handshake_write_seq = 0;
            }
            *len = frag_len;
            return 1;
        }

        /* Fatal error */
        s->init_num = 0;
        return -1;
    } else {
        return 0;
    }
}

static int dtls1_reassemble_fragment(SSL_CONNECTION *s,
    const struct hm_header_st *msg_hdr)
{
    hm_fragment *frag = NULL;
    pitem *item = NULL;
    int i = -1, is_complete;
    size_t frag_len = msg_hdr->frag_len;
    size_t readbytes;
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);

    if ((msg_hdr->frag_off + frag_len) > msg_hdr->msg_len
        || msg_hdr->msg_len > dtls1_max_handshake_message_len(s))
        goto err;

    if (frag_len == 0) {
        return DTLS1_HM_FRAGMENT_RETRY;
    }

    /* Try to find item in queue */
    item = pqueue_find_u64(&s->d1->rcvd_messages, msg_hdr->seq);

    if (item == NULL) {
        frag = dtls1_hm_fragment_new(msg_hdr->msg_len, 1);
        if (frag == NULL)
            goto err;
        memcpy(&(frag->msg_header), msg_hdr, sizeof(*msg_hdr));
        frag->msg_header.frag_len = frag->msg_header.msg_len;
        frag->msg_header.frag_off = 0;
    } else {
        frag = (hm_fragment *)item->data;
        if (frag->msg_header.msg_len != msg_hdr->msg_len) {
            item = NULL;
            frag = NULL;
            goto err;
        }
    }

    /*
     * If message is already reassembled, this must be a retransmit and can
     * be dropped. In this case item != NULL and so frag does not need to be
     * freed.
     */
    if (frag->reassembly == NULL) {
        unsigned char devnull[256];

        while (frag_len) {
            i = ssl->method->ssl_read_bytes(ssl, SSL3_RT_HANDSHAKE, NULL,
                devnull,
                frag_len > sizeof(devnull) ? sizeof(devnull) : frag_len, 0, &readbytes);
            if (i <= 0)
                goto err;
            frag_len -= readbytes;
        }
        return DTLS1_HM_FRAGMENT_RETRY;
    }

    /* read the body of the fragment (header has already been read */
    i = ssl->method->ssl_read_bytes(ssl, SSL3_RT_HANDSHAKE, NULL,
        frag->fragment + msg_hdr->frag_off,
        frag_len, 0, &readbytes);
    if (i <= 0 || readbytes != frag_len)
        goto err;

    RSMBLY_BITMASK_MARK(frag->reassembly, (long)msg_hdr->frag_off,
        (long)(msg_hdr->frag_off + frag_len));

    if (!ossl_assert(msg_hdr->msg_len > 0))
        goto err;
    RSMBLY_BITMASK_IS_COMPLETE(frag->reassembly, (long)msg_hdr->msg_len,
        is_complete);

    if (is_complete)
        frag->reassembly = NULL;

    if (item == NULL) {
        item = pitem_new_u64(msg_hdr->seq, frag);
        if (item == NULL)
            goto err;

        item = pqueue_insert(&s->d1->rcvd_messages, item);
        /*
         * pqueue_insert fails iff a duplicate item is inserted. However,
         * |item| cannot be a duplicate. If it were, |pqueue_find_u64|, above,
         * would have returned it and control would never have reached this
         * branch.
         */
        if (!ossl_assert(item != NULL))
            goto err;
    }

    if (dtls_msg_needs_ack(!s->server, msg_hdr->type)
        && !add_record_to_ack_list(s))
        goto err;

    return DTLS1_HM_FRAGMENT_RETRY;

err:
    if (item == NULL)
        dtls1_hm_fragment_free(frag);
    return -1;
}

/*
 * True if the message currently being processed only authenticated because
 * the read record layer retained a previous epoch's keys for retransmission
 * recovery -- i.e. it did not actually arrive at the currently active
 * read epoch. Such a message must never be treated as new content: it may
 * only, at most, trigger a replacement ACK for something already fully
 * processed.
 */
int dtls_record_from_retained_epoch(SSL_CONNECTION *s)
{
    return SSL_CONNECTION_IS_DTLS13(s)
        && s->s3.tmp.record_epoch != dtls1_get_epoch(s, SSL3_CC_READ);
}

int dtls1_process_out_of_seq_message(SSL_CONNECTION *s,
    const struct hm_header_st *msg_hdr)
{
    int i = -1;
    hm_fragment *frag = NULL;
    pitem *item = NULL;
    size_t frag_len = msg_hdr->frag_len;
    size_t readbytes;
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);

    if ((msg_hdr->frag_off + frag_len) > msg_hdr->msg_len)
        goto err;

    /* Try to find item in queue, to prevent duplicate entries */
    item = pqueue_find_u64(&s->d1->rcvd_messages, msg_hdr->seq);

    /*
     * If we already have an entry and this one is a fragment, don't discard
     * it and rather try to reassemble it.
     */
    if (item != NULL && frag_len != msg_hdr->msg_len)
        item = NULL;

    /*
     * Discard the message if sequence number was already there, is too far
     * in the future, already in the queue, if we received a FINISHED
     * before the SERVER_HELLO (which then must be a stale retransmit), or
     * if it only authenticated via a retained previous epoch: such a
     * message must never be buffered for future reassembly and eventually
     * processed as new content, no matter what sequence number it claims.
     */
    if (msg_hdr->seq <= s->d1->handshake_read_seq
        || msg_hdr->seq > s->d1->handshake_read_seq + 10
        || item != NULL
        || (s->d1->handshake_read_seq == 0 && msg_hdr->type == SSL3_MT_FINISHED)
        || dtls_record_from_retained_epoch(s)) {
        unsigned char devnull[256];

        while (frag_len) {
            i = ssl->method->ssl_read_bytes(ssl, SSL3_RT_HANDSHAKE, NULL,
                devnull,
                frag_len > sizeof(devnull) ? sizeof(devnull) : frag_len, 0, &readbytes);
            if (i <= 0)
                goto err;
            frag_len -= readbytes;
        }
        /*
         * A lost ACK can cause an already processed message to be
         * retransmitted in a new record. ACK it without processing it
         * again. Epoch 2 is included alongside the post-handshake epochs
         * (3+) so that a client's Finished, retransmitted after the server
         * has already moved on to epoch 3, still gets ACKed instead of
         * silently dropped (see dtls_get_more_records()'s retained
         * prev_epoch_rl handling, which is what let this record
         * authenticate at all).
         */
        if (SSL_CONNECTION_IS_DTLS13(s)
            && s->s3.tmp.record_epoch >= 2
            && msg_hdr->seq < s->d1->handshake_read_seq
            && dtls_msg_needs_ack(!s->server, msg_hdr->type)) {
            if (!add_record_to_ack_list(s))
                goto err;
            s->statem.ack_for_retransmit = 1;
        }
    } else {
        if (frag_len != msg_hdr->msg_len) {
            return dtls1_reassemble_fragment(s, msg_hdr);
        }

        if (frag_len > dtls1_max_handshake_message_len(s))
            goto err;

        frag = dtls1_hm_fragment_new(frag_len, 0);
        if (frag == NULL)
            goto err;

        memcpy(&(frag->msg_header), msg_hdr, sizeof(*msg_hdr));

        if (frag_len) {
            /*
             * read the body of the fragment (header has already been read
             */
            i = ssl->method->ssl_read_bytes(ssl, SSL3_RT_HANDSHAKE, NULL,
                frag->fragment, frag_len, 0,
                &readbytes);
            if (i <= 0 || readbytes != frag_len)
                goto err;
        }

        item = pitem_new_u64(msg_hdr->seq, frag);
        if (item == NULL)
            goto err;

        if (dtls_msg_needs_ack(!s->server, msg_hdr->type)
            && !add_record_to_ack_list(s)) {
            pitem_free(item);
            item = NULL;
            goto err;
        }

        item = pqueue_insert(&s->d1->rcvd_messages, item);
        /*
         * pqueue_insert fails iff a duplicate item is inserted. However,
         * |item| cannot be a duplicate. If it were, |pqueue_find|, above,
         * would have returned it. Then, either |frag_len| !=
         * |msg_hdr->msg_len| in which case |item| is set to NULL and it will
         * have been processed with |dtls1_reassemble_fragment|, above, or
         * the record will have been discarded.
         */
        if (!ossl_assert(item != NULL))
            goto err;
    }

    return DTLS1_HM_FRAGMENT_RETRY;

err:
    if (item == NULL)
        dtls1_hm_fragment_free(frag);
    return 0;
}

static int dtls1_read_hm_header(unsigned char *msgheaderstart,
    struct hm_header_st *msg_hdr)
{
    unsigned long msg_len, frag_off, frag_len;
    unsigned int msg_seq, msg_type;
    PACKET msgheader;

    if (!PACKET_buf_init(&msgheader, msgheaderstart, DTLS1_HM_HEADER_LENGTH)
        || !PACKET_get_1(&msgheader, &msg_type)
        || !PACKET_get_net_3(&msgheader, &msg_len)
        || !PACKET_get_net_2(&msgheader, &msg_seq)
        || !PACKET_get_net_3(&msgheader, &frag_off)
        || !PACKET_get_net_3(&msgheader, &frag_len)
        || PACKET_remaining(&msgheader) != 0) {
        return 0;
    }

    /* We just checked that values did not exceed max size so cast must be alright */
    msg_hdr->type = (unsigned char)msg_type;
    msg_hdr->msg_len = (size_t)msg_len;
    msg_hdr->seq = (unsigned short)msg_seq;
    msg_hdr->frag_off = (size_t)frag_off;
    msg_hdr->frag_len = (size_t)frag_len;

    return 1;
}

/*
 * DTLS 1.3 reserves handshake message type 0, so a HelloRequest must reach the
 * state machine and be rejected there whenever DTLS 1.3 is still possible.
 *
 * s->version is the negotiated, or maximum version: before ServerHello this is
 * the effective maximum, and after ServerHello or during renegotiation it is the
 * selected version.
 */
static int dtls_should_skip_hello_request(const SSL_CONNECTION *s)
{
    if (SSL_CONNECTION_IS_DTLS13(s))
        return 0;

    return s->version > 0
        && ssl_version_cmp(s, s->version, DTLS1_3_VERSION) < 0;
}

static int dtls_get_reassembled_message(SSL_CONNECTION *s, int *errtype,
    size_t *len)
{
    int i, ret;
    uint8_t recvd_type;
    struct hm_header_st msg_hdr;
    size_t readbytes;
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);
    SSL *ussl = SSL_CONNECTION_GET_USER_SSL(s);
    int chretran = 0;
    unsigned char *p;

    *errtype = 0;

    p = (unsigned char *)s->init_buf->data;

redo:
    /* Check for buffered CCS */
    if ((s->version == DTLS1_VERSION || s->version == DTLS1_2_VERSION
            || s->version == DTLS1_BAD_VER)
        && s->d1->has_change_cipher_spec && dtls_ccs_expected(s)) {
        size_t extra = (s->version == DTLS1_BAD_VER) ? 2 : 0;

        s->d1->has_change_cipher_spec = 0;
        p[0] = SSL3_MT_CCS;
        /*
         * The extra 2 bytes are never consumed, only checked for
         * length -- zero-fill to avoid old init_buf content.
         */
        if (extra > 0)
            memset(p + 1, 0, extra);
        s->init_num = extra;
        s->init_msg = p + 1;
        s->s3.tmp.message_type = SSL3_MT_CHANGE_CIPHER_SPEC;
        s->s3.tmp.message_size = extra;
        *len = extra;
        return 1;
    }

    /* see if we have the required fragment already */
    ret = dtls1_retrieve_buffered_fragment(s, &msg_hdr.frag_len);
    if (ret < 0) {
        /* SSLfatal() already called */
        return 0;
    }
    if (ret > 0) {
        s->init_num = msg_hdr.frag_len;
        *len = msg_hdr.frag_len;
        return 1;
    }

    /* read handshake message header */
    i = ssl->method->ssl_read_bytes(ssl, SSL3_RT_HANDSHAKE, &recvd_type, p,
        DTLS1_HM_HEADER_LENGTH, 0, &readbytes);
    if (i <= 0) { /* nbio, or an error */
        s->rwstate = SSL_READING;
        *len = 0;
        return 0;
    }
    if (recvd_type == SSL3_RT_CHANGE_CIPHER_SPEC) {
        if (p[0] != SSL3_MT_CCS) {
            SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE,
                SSL_R_BAD_CHANGE_CIPHER_SPEC);
            goto f_err;
        }

        /* Buffer CCS for reorder tolerance */
        if (s->version == DTLS1_VERSION || s->version == DTLS1_2_VERSION
            || s->version == DTLS1_BAD_VER) {
            size_t expected = (s->version == DTLS1_BAD_VER) ? 3 : 1;

            if (readbytes != expected) {
                SSLfatal(s, SSL_AD_DECODE_ERROR,
                    SSL_R_BAD_CHANGE_CIPHER_SPEC);
                goto f_err;
            }
            s->d1->has_change_cipher_spec = 1;
            goto redo;
        }
        SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE,
            SSL_R_BAD_CHANGE_CIPHER_SPEC);
        goto f_err;
    }
    if (recvd_type == SSL3_RT_ACK) {
        /*
         * An ACK has no handshake message header: the bytes already read
         * are body, and an ACK never spans records (RFC 9147 section 4),
         * so the rest of the body - if any - is whatever is left in the
         * current record. Take it directly to avoid re-entering the
         * timeout path while assembling the ACK.
         */
        if (readbytes == DTLS1_HM_HEADER_LENGTH
            && s->rlayer.curr_rec < s->rlayer.num_recs) {
            TLS_RECORD *rr = &s->rlayer.tlsrecs[s->rlayer.curr_rec];

            /*
             * DTLS currently processes one record at a time, so an exhausted
             * record is excluded above. Keep these checks to avoid consuming
             * a following record if pipelining is added.
             */
            if (rr->off > 0 && rr->length > 0) {
                /*
                 * init_buf has capacity for a full plaintext record, whose size
                 * has already been checked by the record layer.
                 */
                memcpy(p + DTLS1_HM_HEADER_LENGTH, rr->data + rr->off,
                    rr->length);
                readbytes += rr->length;
                if (!ssl_release_record(s, rr, rr->length)) {
                    /* SSLfatal() already called */
                    goto f_err;
                }
            }
        }
        s->init_num = readbytes;
        s->init_msg = s->init_buf->data;
        s->s3.tmp.message_type = DTLS13_MT_ACK;
        s->s3.tmp.message_size = readbytes;
        *len = readbytes;
        return 1;
    }

    /* Handshake fails if message header is incomplete */
    if (readbytes != DTLS1_HM_HEADER_LENGTH) {
        SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_UNEXPECTED_MESSAGE);
        goto f_err;
    }

    /* parse the message fragment header */
    if (!dtls1_read_hm_header(p, &msg_hdr)) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_LENGTH);
        goto f_err;
    }

    /*
     * We must have at least frag_len bytes left in the record to be read.
     * Fragments must not span records.
     */
    if (msg_hdr.frag_len > s->rlayer.tlsrecs[s->rlayer.curr_rec].length) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_LENGTH);
        goto f_err;
    }

    /*
     * if this is a future (or stale) message it gets buffered
     * (or dropped)--no further processing at this time
     * While listening, we accept seq 1 (ClientHello with cookie)
     * although we're still expecting seq 0 (ClientHello)
     *
     * A message that only authenticated via a retained previous epoch must
     * always be routed to dtls1_process_out_of_seq_message(), even when its
     * claimed sequence number happens to match the next expected one: that
     * match does not mean this content is actually new.
     */
    if (msg_hdr.seq != s->d1->handshake_read_seq
        || dtls_record_from_retained_epoch(s)) {
        if (!s->server
            || msg_hdr.seq != 0
            || s->d1->handshake_read_seq != 1
            || msg_hdr.type != SSL3_MT_CLIENT_HELLO
            || s->statem.hand_state != DTLS_ST_SW_HELLO_VERIFY_REQUEST) {
            *errtype = dtls1_process_out_of_seq_message(s, &msg_hdr);
            return 0;
        }
        /*
         * We received a ClientHello and sent back a HelloVerifyRequest. We
         * now seem to have received a retransmitted initial ClientHello. That
         * is allowed (possibly our HelloVerifyRequest got lost).
         */
        chretran = 1;
    }

    if (msg_hdr.frag_len > 0 && msg_hdr.frag_len < msg_hdr.msg_len) {
        *errtype = dtls1_reassemble_fragment(s, &msg_hdr);
        return 0;
    }

    if (!s->server
        && s->statem.hand_state != TLS_ST_OK
        && msg_hdr.type == SSL3_MT_HELLO_REQUEST
        && dtls_should_skip_hello_request(s)) {
        /*
         * The server may always send 'Hello Request' messages -- we are
         * doing a handshake anyway now, so ignore them if their format is
         * correct. Does not count for 'Finished' MAC.
         */
        if (msg_hdr.msg_len == 0) {
            if (s->msg_callback)
                s->msg_callback(0, s->version, SSL3_RT_HANDSHAKE,
                    p, DTLS1_HM_HEADER_LENGTH, ussl,
                    s->msg_callback_arg);

            s->init_num = 0;
            goto redo;
        } else {
            /* Incorrectly formatted Hello request */
            SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_UNEXPECTED_MESSAGE);
            goto f_err;
        }
    }

    if (!dtls1_preprocess_fragment(s, &msg_hdr)) {
        /* SSLfatal() already called */
        goto f_err;
    }

    if (msg_hdr.frag_len > 0) {
        /* dtls1_preprocess_fragment() above could reallocate init_buf */
        p = (unsigned char *)s->init_buf->data + DTLS1_HM_HEADER_LENGTH + msg_hdr.frag_off;

        i = ssl->method->ssl_read_bytes(ssl, SSL3_RT_HANDSHAKE, NULL,
            p, msg_hdr.frag_len, 0, &readbytes);

        /*
         * This shouldn't ever fail due to NBIO because we already checked
         * that we have enough data in the record
         */
        if (i <= 0) {
            s->rwstate = SSL_READING;
            *len = 0;
            return 0;
        }
    } else {
        readbytes = 0;
    }

    /*
     * XDTLS: an incorrectly formatted fragment should cause the handshake
     * to fail
     */
    if (readbytes != msg_hdr.frag_len) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_LENGTH);
        goto f_err;
    }

    if (chretran) {
        /*
         * We got a new ClientHello with a message sequence of 0.
         * Reset the read/write sequences back to the beginning.
         * We process it like this is the first time we've seen a ClientHello
         * from the client.
         */
        s->d1->handshake_read_seq = 0;
        s->d1->next_handshake_write_seq = 0;
    }

    if (dtls_msg_needs_ack(!s->server, msg_hdr.type)
        && !add_record_to_ack_list(s)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto f_err;
    }

    /*
     * Note that s->init_num is *not* used as current offset in
     * s->init_buf->data, but as a counter summing up fragments' lengths: as
     * soon as they sum up to handshake packet length, we assume we have got
     * all the fragments.
     */
    *len = s->init_num = msg_hdr.frag_len;
    return 1;

f_err:
    s->init_num = 0;
    *len = 0;
    return 0;
}

/*-
 * for these 2 messages, we need to
 * ssl->session->read_sym_enc           assign
 * ssl->session->read_compression       assign
 * ssl->session->read_hash              assign
 */
CON_FUNC_RETURN dtls_construct_change_cipher_spec(SSL_CONNECTION *s,
    WPACKET *pkt)
{
    if (s->version == DTLS1_BAD_VER) {
        s->d1->next_handshake_write_seq++;

        if (!WPACKET_put_bytes_u16(pkt, s->d1->handshake_write_seq)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return CON_FUNC_ERROR;
        }
    }

    return CON_FUNC_SUCCESS;
}

CON_FUNC_RETURN dtls_construct_ack(SSL_CONNECTION *s, WPACKET *pkt)
{
    DTLS1_RECORD_NUMBER *recnum;
    DTLS1_RECORD_NUMBER *recnumnext = ossl_list_record_number_head(&s->d1->ack_rec_num);

    if (!WPACKET_start_sub_packet_u16(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return CON_FUNC_ERROR;
    }

    while ((recnum = recnumnext) != NULL) {
        /*
         * rfc9147: section 4.
         *
         * Record numbers are encoded as
         *      struct {
         *           uint64 epoch;
         *           uint64 sequence_number;
         *      } RecordNumber;
         */

        recnumnext = ossl_list_record_number_next(recnum);

        if (!SSL_IS_FIRST_HANDSHAKE(s)
            || recnum->epoch <= dtls1_get_epoch(s, SSL3_CC_WRITE)) {
            /*
             * rfc9147:
             * During the handshake, ACK records MUST be sent with an epoch which
             * is equal to or higher than the record which is being acknowledged.
             * After the handshake, the sending and receiving epochs can differ.
             */
            if (!WPACKET_put_bytes_u64(pkt, recnum->epoch)
                || !WPACKET_put_bytes_u64(pkt, recnum->seqnum)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return CON_FUNC_ERROR;
            }

            ossl_list_record_number_remove(&s->d1->ack_rec_num, recnum);
            OPENSSL_free(recnum);
        }
    }

    if (!WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return CON_FUNC_ERROR;
    }

    return CON_FUNC_SUCCESS;
}

MSG_PROCESS_RETURN dtls_process_ack(SSL_CONNECTION *s, PACKET *pkt)
{
    PACKET record_numbers;

    if (!PACKET_as_length_prefixed_2(pkt, &record_numbers)) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_LENGTH_TOO_LONG);
        return MSG_PROCESS_ERROR;
    }

    while (PACKET_remaining(&record_numbers) > 0) {
        /*
         * rfc9147: section 4.
         *
         * Record numbers are encoded as
         *      struct {
         *           uint64 epoch;
         *           uint64 sequence_number;
         *      } RecordNumber;
         */
        pitem *item;
        piterator iter;
        uint64_t epoch;
        uint64_t sequence_number;

        if (!PACKET_get_net_8(&record_numbers, &epoch)
            || !PACKET_get_net_8(&record_numbers, &sequence_number)) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_TOO_SHORT);
            return MSG_PROCESS_ERROR;
        }

        /*
         * Epoch 2 is the fixed handshake epoch and is never used again once
         * epoch 3 (the first application epoch) is installed. An ACK that
         * only authenticated via a retained epoch-2 layer must not be
         * trusted to cancel retransmission of a post-handshake (epoch 3+)
         * flight. Retained epochs 3+ are unrestricted: unlike epoch 2, which
         * is never reissued, each of those epochs was freshly minted by an
         * SSL_key_update() call and gets superseded by the next one, so a
         * legitimate delayed ACK can authenticate behind a message's own
         * recorded epoch with no protocol violation.
         */
        if (dtls_record_from_retained_epoch(s)
            && s->s3.tmp.record_epoch == 2
            && epoch > 2)
            continue;

        iter = pqueue_iterator(&s->d1->sent_messages);

        while ((item = pqueue_next(&iter)) != NULL) {
            dtls_sent_msg *msg = (dtls_sent_msg *)item->data;
            DTLS1_RECORD_NUMBER *recnum;
            DTLS1_RECORD_NUMBER *recnum_next = ossl_list_record_number_head(&msg->rec_nums);
            int matched = 0;

            while ((recnum = recnum_next) != NULL) {
                recnum_next = ossl_list_record_number_next(recnum_next);

                if (recnum->epoch == epoch && recnum->seqnum == sequence_number) {
                    /*
                     * Mark this record's byte range covered *before*
                     * freeing it -- coverage tracks the message as a whole
                     * across every transmission round, not just whether
                     * this one specific record number was ever matched.
                     *
                     * The range check guards against a corrupt recorded
                     * range
                     */
                    if (msg->covered != NULL
                        && recnum->frag_off <= msg->msg_info.msg_body_len
                        && recnum->frag_len
                            <= msg->msg_info.msg_body_len - recnum->frag_off)
                        RSMBLY_BITMASK_MARK(msg->covered, (long)recnum->frag_off,
                            (long)(recnum->frag_off + recnum->frag_len));
                    ossl_list_record_number_remove(&msg->rec_nums, recnum);
                    OPENSSL_free(recnum);
                    matched = 1;
                }
            }

            /*
             * RFC 9147 section 7.2 is a per-record rule, but completeness is
             * per-message: the peer needs every byte of the message, from
             * any combination of rounds, not just any one matching record.
             * Once this ACK completes coverage of the whole message, fully
             * drain rec_nums -- not just the node that matched -- so every
             * other consumer that keys off list emptiness
             * (dtls_any_sent_messages_are_missing_acknowledge(),
             * dtls1_clear_sent_buffer(), ...) sees it retire, even though
             * other rounds' record numbers may still be sitting unmatched.
             */
            if (matched && msg->covered != NULL) {
                int is_complete;

                RSMBLY_BITMASK_IS_COMPLETE(msg->covered,
                    (long)msg->msg_info.msg_body_len, is_complete);
                if (is_complete)
                    ossl_list_record_number_elem_free(&msg->rec_nums);
            }
        }
    }

    /* Keep the retransmit timer running until the whole flight is ACKed. */
    if (dtls_any_sent_messages_are_missing_acknowledge(s))
        return MSG_PROCESS_CONTINUE_READING;

    return MSG_PROCESS_FINISHED_READING;
}

#ifndef OPENSSL_NO_SCTP
/*
 * Wait for a dry event. Should only be called at a point in the handshake
 * where we are not expecting any data from the peer except an alert.
 */
WORK_STATE dtls_wait_for_dry(SSL_CONNECTION *s)
{
    int ret, errtype;
    size_t len;
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);

    /* read app data until dry event */
    ret = BIO_dgram_sctp_wait_for_dry(SSL_get_wbio(ssl));
    if (ret < 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return WORK_ERROR;
    }

    if (ret == 0) {
        /*
         * We're not expecting any more messages from the peer at this point -
         * but we could get an alert. If an alert is waiting then we will never
         * return successfully. Therefore we attempt to read a message. This
         * should never succeed but will process any waiting alerts.
         */
        if (dtls_get_reassembled_message(s, &errtype, &len)) {
            /* The call succeeded! This should never happen */
            SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_UNEXPECTED_MESSAGE);
            return WORK_ERROR;
        }

        s->s3.in_read_app_data = 2;
        s->rwstate = SSL_READING;
        BIO_clear_retry_flags(SSL_get_rbio(ssl));
        BIO_set_retry_read(SSL_get_rbio(ssl));
        return WORK_MORE_A;
    }
    return WORK_FINISHED_CONTINUE;
}
#endif

int dtls1_read_failed(SSL_CONNECTION *s, int code)
{
    SSL *ssl = SSL_CONNECTION_GET_SSL(s);

    if (code > 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (!dtls1_is_timer_expired(s) || ossl_statem_in_error(s)) {
        /*
         * not a timeout, none of our business, let higher layers handle
         * this.  in fact it's probably an error
         */
        return code;
    }
    /* done, no need to send a retransmit */
    if (!SSL_in_init(ssl)) {
        BIO_set_flags(SSL_get_rbio(ssl), BIO_FLAGS_READ);
        return code;
    }

    return dtls1_handle_timeout(s);
}

void dtls1_get_queue_priority(unsigned char *prio64be, unsigned short seq,
    int record_type)
{
    /*
     * The index of the retransmission queue actually is the message sequence
     * number, since the queue only contains messages of a single handshake.
     * However, the ChangeCipherSpec has no message sequence number and so
     * using only the sequence will result in the CCS and Finished having the
     * same index. To prevent this, the sequence number is multiplied by 2.
     * In case of a CCS 1 is subtracted. This does not only differ CSS and
     * Finished, it also maintains the order of the index (important for
     * priority queues) and fits in the unsigned short variable.
     */
    int lsb = (record_type == SSL3_RT_CHANGE_CIPHER_SPEC);
    const uint16_t prio = seq * 2 - lsb;

    memset(prio64be, 0, 8);
    prio64be[6] = (unsigned char)(prio >> 8);
    prio64be[7] = (unsigned char)(prio);
}

int dtls1_retransmit_sent_messages(SSL_CONNECTION *s)
{
    piterator iter = pqueue_iterator(&s->d1->sent_messages);
    pitem *item;

    for (item = pqueue_next(&iter); item != NULL; item = pqueue_next(&iter)) {
        dtls_sent_msg *sent_msg = (dtls_sent_msg *)item->data;

        if (SSL_CONNECTION_IS_DTLS13(s)
            && ossl_list_record_number_is_empty(&sent_msg->rec_nums))
            /* rfc9147: Implementations must not retransmit acknowledged msgs */
            continue;

        if (dtls1_retransmit_message(s, sent_msg) <= 0)
            return -1;
    }

    return 1;
}

int dtls1_buffer_sent_message(SSL_CONNECTION *s, int record_type)
{
    pitem *item;
    dtls_sent_msg *sent_msg;
    unsigned char seq64be[8];
    size_t headerlen;

    /*
     * this function is called immediately after a message has been
     * serialized
     */
    if (!ossl_assert(s->init_off == 0))
        return 0;

    sent_msg = dtls1_sent_msg_new(s->init_num, s->d1->w_msg.msg_body_len,
        SSL_CONNECTION_IS_DTLS13(s));
    if (sent_msg == NULL)
        return 0;

    memcpy(sent_msg->msg_buf, s->init_buf->data, s->init_num);

    if (record_type == SSL3_RT_CHANGE_CIPHER_SPEC)
        /* For DTLS1_BAD_VER the header length is non-standard */
        headerlen = (s->version == DTLS1_BAD_VER) ? 3 : DTLS1_CCS_HEADER_LENGTH;
    else
        headerlen = DTLS1_HM_HEADER_LENGTH;

    if (!ossl_assert(s->d1->w_msg.msg_body_len + headerlen == s->init_num)) {
        dtls1_sent_msg_free(sent_msg);
        return 0;
    }

    memcpy(&sent_msg->msg_info, &s->d1->w_msg, sizeof(s->d1->w_msg));

    /* save current state */
    sent_msg->saved_retransmit_state.wrlmethod = s->rlayer.wrlmethod;
    sent_msg->saved_retransmit_state.wrl = s->rlayer.wrl;

    dtls1_get_queue_priority(seq64be, sent_msg->msg_info.msg_seq, sent_msg->msg_info.record_type);

    item = pitem_new(seq64be, sent_msg);
    if (item == NULL) {
        dtls1_sent_msg_free(sent_msg);
        return 0;
    }

    if (pqueue_insert(&s->d1->sent_messages, item) == NULL) {
        dtls1_sent_msg_free(sent_msg);
        pitem_free(item);
        return 0;
    }
    return 1;
}

int dtls1_retransmit_message(SSL_CONNECTION *s, dtls_sent_msg *sent_msg)
{
    int ret;
    unsigned long header_length;
    struct dtls1_retransmit_state saved_state;

    if (sent_msg->msg_info.record_type == SSL3_RT_CHANGE_CIPHER_SPEC)
        header_length = DTLS1_CCS_HEADER_LENGTH;
    else
        header_length = DTLS1_HM_HEADER_LENGTH;

    /*
     * Deliberately not clearing rec_nums here: RFC 9147 section 7.2 requires
     * treating a record as acknowledged if it appears in *any* ACK, so a
     * late ACK matching an earlier round's record number must still be able
     * to match something. rec_nums accumulates across every retransmission
     * round instead; dtls_process_ack() retires entries by tracking byte
     * range coverage, not by this list ever being reset per round.
     */

    memcpy(s->init_buf->data, sent_msg->msg_buf,
        sent_msg->msg_info.msg_body_len + header_length);
    s->init_num = sent_msg->msg_info.msg_body_len + header_length;
    /* Always retransmit from the start, not wherever init_off was left */
    s->init_off = 0;

    memcpy(&s->d1->w_msg, &sent_msg->msg_info, sizeof(sent_msg->msg_info));

    /* save current state */
    saved_state.wrlmethod = s->rlayer.wrlmethod;
    saved_state.wrl = s->rlayer.wrl;

    s->d1->retransmitting = 1;

    /* restore state in which the message was originally sent */
    s->rlayer.wrlmethod = sent_msg->saved_retransmit_state.wrlmethod;
    s->rlayer.wrl = sent_msg->saved_retransmit_state.wrl;

    /*
     * The old wrl may be still pointing at an old BIO. Update it to what we're
     * using now.
     */
    s->rlayer.wrlmethod->set1_bio(s->rlayer.wrl, s->wbio);

    ret = dtls1_do_write(s, sent_msg->msg_info.record_type);

    /* restore current state */
    s->rlayer.wrlmethod = saved_state.wrlmethod;
    s->rlayer.wrl = saved_state.wrl;

    s->d1->retransmitting = 0;

    (void)BIO_flush(s->wbio);
    return ret;
}

int dtls1_set_handshake_header(SSL_CONNECTION *s, WPACKET *pkt, int htype)
{
    s->d1->handshake_write_seq = s->d1->next_handshake_write_seq;
    s->d1->w_msg.msg_seq = s->d1->handshake_write_seq;
    s->d1->w_msg.msg_body_len = 0;

    if (htype == SSL3_MT_CHANGE_CIPHER_SPEC) {
        s->d1->w_msg.record_type = SSL3_RT_CHANGE_CIPHER_SPEC;
        s->d1->w_msg.msg_type = SSL3_MT_CCS;

        if (!WPACKET_put_bytes_u8(pkt, SSL3_MT_CCS))
            return 0;
    } else if (htype == DTLS13_MT_ACK) {
        s->d1->w_msg.record_type = SSL3_RT_ACK;
        s->d1->w_msg.msg_type = 0;
    } else {
        size_t subpacket_offset = DTLS1_HM_HEADER_LENGTH - SSL3_HM_HEADER_LENGTH;

        s->d1->next_handshake_write_seq++;
        s->d1->w_msg.record_type = SSL3_RT_HANDSHAKE;
        s->d1->w_msg.msg_type = htype;

        /* Set the content type and 3 bytes for the message len */
        if (!WPACKET_put_bytes_u8(pkt, htype)
            /*
             * We allocate space for DTLS specific fields.
             * These gets filled later.
             */
            || !WPACKET_start_sub_packet_u24_at_offset(pkt, subpacket_offset))
            return 0;
    }

    return 1;
}

int dtls1_close_construct_packet(SSL_CONNECTION *s, WPACKET *pkt, int htype)
{
    size_t msglen;

    if ((s->d1->w_msg.record_type == SSL3_RT_HANDSHAKE && !WPACKET_close(pkt))
        || !WPACKET_get_length(pkt, &msglen)
        || msglen > INT_MAX)
        return 0;

    if (s->d1->w_msg.record_type == SSL3_RT_HANDSHAKE)
        s->d1->w_msg.msg_body_len = msglen - DTLS1_HM_HEADER_LENGTH;

    s->init_num = msglen;
    s->init_off = 0;

    if (htype != DTLS1_MT_HELLO_VERIFY_REQUEST
        && s->d1->w_msg.record_type != SSL3_RT_ACK) {
        /* Buffer the message to handle re-xmits */
        if (!dtls1_buffer_sent_message(s, s->d1->w_msg.record_type))
            return 0;
    }

    return 1;
}
