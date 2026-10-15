/*
 * Copyright 2018-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <assert.h>
#include "../../ssl_local.h"
#include "../record_local.h"
#include "recmethod_local.h"
#include "internal/safe_math.h"

OSSL_SAFE_MATH_UNSIGNED(uint64_t, uint64_t)

static int dtls_increment_sequence_ctr(OSSL_RECORD_LAYER *rl);

/* mod 128 saturating subtract of two 64-bit values */
static int satsub64(uint64_t l1, uint64_t l2)
{
    uint64_t max, min;
    int sign;

    if (l1 > l2) {
        max = l1;
        min = l2;
        sign = 1;
    } else {
        max = l2;
        min = l1;
        sign = -1;
    }

    if (max - min > 128)
        return sign * 128;

    return sign * ((int)(max - min));
}

static int dtls_record_replay_check(OSSL_RECORD_LAYER *rl, DTLS_BITMAP *bitmap)
{
    int cmp;
    unsigned int shift;

    cmp = satsub64(rl->sequence, bitmap->max_seq_num);
    if (cmp > 0) {
        rl->rrec[0].seq_num = rl->sequence;
        return 1; /* this record in new */
    }
    shift = -cmp;
    if (shift >= sizeof(bitmap->map) * 8)
        return 0; /* stale, outside the window */
    else if (bitmap->map & ((uint64_t)1 << shift))
        return 0; /* record previously received */

    rl->rrec[0].seq_num = rl->sequence;
    return 1;
}

static void dtls_record_bitmap_update(OSSL_RECORD_LAYER *rl, DTLS_BITMAP *bitmap)
{
    int cmp;
    unsigned int shift;

    cmp = satsub64(rl->sequence, bitmap->max_seq_num);
    if (cmp > 0) {
        shift = cmp;
        if (shift < sizeof(bitmap->map) * 8)
            bitmap->map <<= shift, bitmap->map |= 1UL;
        else
            bitmap->map = 1UL;
        bitmap->max_seq_num = rl->sequence;
    } else {
        shift = -cmp;
        if (shift < sizeof(bitmap->map) * 8)
            bitmap->map |= (uint64_t)1 << shift;
    }
}

static DTLS_BITMAP *dtls_get_bitmap(OSSL_RECORD_LAYER *rl, TLS_RL_RECORD *rr,
    unsigned int *is_next_epoch)
{
    *is_next_epoch = 0;

    /* In current epoch, accept HM, CCS, DATA, & ALERT */
    if (rr->epoch == rl->epoch) {
        return &rl->bitmap;
        /*
         * DTLS 1.3 uses encrypted sequence numbers. Therefore we
         * cannot check future bitmaps since the sequence number
         * is encrypted. The dtls_record_bitmap_update is only called
         * once the record layer with the correct epoch has
         * processed the record.
         * We are concerned during the DTLS 1.3 handshake if a record
         * from a future handshake epoch is received.
         */
    } else if ((rl->version == DTLS1_3_VERSION || rl->version == DTLS_ANY_VERSION) && rl->epoch == 0 && rr->epoch == 2) {
        *is_next_epoch = 1;
        /*
         * Check if the message is from the next epoch
         */
    } else if (rr->epoch == rl->epoch + 1) {
        *is_next_epoch = 1;
    }

    return NULL;
}

static void dtls_set_in_init(OSSL_RECORD_LAYER *rl, int in_init)
{
    rl->in_init = in_init;
}

size_t dtls_get_rec_header_size(uint8_t hdr_first_byte)
{
    size_t size = 0;

    if (DTLS13_UNI_HDR_FIX_BITS_IS_SET(hdr_first_byte)
        && ossl_assert(!DTLS13_UNI_HDR_CID_BIT_IS_SET(hdr_first_byte))) {
        /* DTLSv1.3 unified record header */
        size = 1;
        size += DTLS13_UNI_HDR_SEQ_BIT_IS_SET(hdr_first_byte) ? 2 : 1;
        size += DTLS13_UNI_HDR_LEN_BIT_IS_SET(hdr_first_byte) ? 2 : 0;
    } else {
        /* DTLSv1.0, DTLSv1.2 or unencrypted DTLSv1.3 record header */
        size = DTLS1_RT_HEADER_LENGTH;
    }

    return size;
}

static int dtls_process_record(OSSL_RECORD_LAYER *rl, DTLS_BITMAP *bitmap)
{
    int i;
    int enc_err;
    TLS_RL_RECORD *rr;
    int imac_size;
    size_t mac_size = 0;
    size_t rechdrsize = dtls_get_rec_header_size(rl->packet[0]);
    unsigned char md[EVP_MAX_MD_SIZE];
    SSL_MAC_BUF macbuf = { NULL, 0 };
    int ret = 0;

    rr = &rl->rrec[0];

    /*
     * At this point, rl->packet_length == rechdrsize + rr->length,
     * and we have that many bytes in rl->packet
     */
    rr->input = rl->packet + rechdrsize;

    /*
     * ok, we can now read from 'rl->packet' data into 'rr'. rr->input
     * points at rr->length bytes, which need to be copied into rr->data by
     * either the decryption or by the decompression. When the data is 'copied'
     * into the rr->data buffer, rr->input will be pointed at the new buffer
     */

    /*
     * We now have - encrypted [ MAC [ compressed [ plain ] ] ] rr->length
     * bytes of encrypted compressed stuff.
     */

    /* check is not needed I believe */
    if (rr->length > SSL3_RT_MAX_ENCRYPTED_LENGTH) {
        RLAYERfatal(rl, SSL_AD_RECORD_OVERFLOW, SSL_R_ENCRYPTED_LENGTH_TOO_LONG);
        return 0;
    }

    /* decrypt in place in 'rr->input' */
    rr->data = rr->input;
    rr->orig_len = rr->length;

    if (rl->md_ctx != NULL) {
        const EVP_MD *tmpmd = EVP_MD_CTX_get0_md(rl->md_ctx);

        if (tmpmd != NULL) {
            imac_size = EVP_MD_get_size(tmpmd);
            if (!ossl_assert(imac_size > 0 && imac_size <= EVP_MAX_MD_SIZE)) {
                RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
                return 0;
            }
            mac_size = (size_t)imac_size;
        }
    }

    if (rl->use_etm && rl->md_ctx != NULL) {
        unsigned char *mac;

        if (rr->orig_len < mac_size) {
            RLAYERfatal(rl, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_TOO_SHORT);
            return 0;
        }
        rr->length -= mac_size;
        mac = rr->data + rr->length;
        i = rl->funcs->mac(rl, rr, md, 0 /* not send */);
        if (i == 0 || CRYPTO_memcmp(md, mac, (size_t)mac_size) != 0) {
            RLAYERfatal(rl, SSL_AD_BAD_RECORD_MAC,
                SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC);
            return 0;
        }
        /*
         * We've handled the mac now - there is no MAC inside the encrypted
         * record
         */
        mac_size = 0;
    }

    /*
     * Set a mark around the packet decryption attempt.  This is DTLS, so
     * bad packets are just ignored, and we don't want to leave stray
     * errors in the queue from processing bogus junk that we ignored.
     */
    ERR_set_mark();
    enc_err = rl->funcs->cipher(rl, rr, 1, 0, &macbuf, mac_size);

    /*-
     * enc_err is:
     *    0: if the record is publicly invalid, or an internal error, or AEAD
     *       decryption failed, or ETM decryption failed.
     *    1: Success or MTE decryption failed (MAC will be randomised)
     */
    if (enc_err == 0) {
        ERR_pop_to_mark();
        if (rl->alert != SSL_AD_NO_ALERT) {
            /* RLAYERfatal() already called */
            goto end;
        }
        /* For DTLS we simply ignore bad packets. */
        rr->length = 0;
        rl->packet_length = 0;
        goto end;
    }
    ERR_clear_last_mark();
    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out, "dec %zd\n", rr->length);
        BIO_dump_indent(trc_out, rr->data, (int)rr->length, 4);
    }
    OSSL_TRACE_END(TLS);

    /* r->length is now the compressed data plus mac */
    if (!rl->use_etm
        && (rl->enc_ctx != NULL)
        && (EVP_MD_CTX_get0_md(rl->md_ctx) != NULL)) {
        /* rl->md_ctx != NULL => mac_size != -1 */

        i = rl->funcs->mac(rl, rr, md, 0 /* not send */);
        if (i == 0 || macbuf.mac == NULL
            || CRYPTO_memcmp(md, macbuf.mac, mac_size) != 0)
            enc_err = 0;
        if (rr->length > SSL3_RT_MAX_COMPRESSED_LENGTH + mac_size)
            enc_err = 0;
    }

    if (enc_err == 0) {
        /* decryption failed, silently discard message */
        rr->length = 0;
        rl->packet_length = 0;
        goto end;
    }

    /* r->length is now just compressed */
    if (rl->compctx != NULL) {
        if (rr->length > SSL3_RT_MAX_COMPRESSED_LENGTH) {
            RLAYERfatal(rl, SSL_AD_RECORD_OVERFLOW,
                SSL_R_COMPRESSED_LENGTH_TOO_LONG);
            goto end;
        }
        if (!tls_do_uncompress(rl, rr)) {
            RLAYERfatal(rl, SSL_AD_DECOMPRESSION_FAILURE, SSL_R_BAD_DECOMPRESSION);
            goto end;
        }
    }

    /*
     * Check if the received packet overflows the current Max Fragment
     * Length setting.
     */
    if (rr->length > rl->max_frag_len) {
        RLAYERfatal(rl, SSL_AD_RECORD_OVERFLOW, SSL_R_DATA_LENGTH_TOO_LONG);
        goto end;
    }

    rr->off = 0;
    /*-
     * So at this point the following is true
     * ssl->s3.rrec.type   is the type of record
     * ssl->s3.rrec.length == number of bytes in record
     * ssl->s3.rrec.off    == offset to first valid byte
     * ssl->s3.rrec.data   == where to take bytes from, increment
     *                        after use :-).
     */

    /* we have pulled in a full packet so zero things */
    rl->packet_length = 0;

    /* Mark receipt of record. */
    dtls_record_bitmap_update(rl, bitmap);

    ret = 1;
end:
    if (macbuf.alloced)
        OPENSSL_free(macbuf.mac);
    return ret;
}

static int dtls_rlayer_buffer_record(OSSL_RECORD_LAYER *rl, struct pqueue_st *queue,
    uint64_t priority)
{
    DTLS_RLAYER_RECORD_DATA *rdata;
    pitem *item;

    /* Limit the size of the queue to prevent DOS attacks */
    if (pqueue_size(queue) >= 16)
        return 0;

    rdata = OPENSSL_malloc(sizeof(*rdata));
    item = pitem_new_u64(priority, rdata);
    if (rdata == NULL || item == NULL) {
        OPENSSL_free(rdata);
        pitem_free(item);
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return -1;
    }

    /*
     * Take a copy of just this record's on-wire bytes (header + ciphertext)
     * rather than the whole (much larger) read buffer. The live rl->rbuf is
     * left untouched and continues to be used for subsequent reads.
     */
    rdata->packet_length = rl->packet_length;
    rdata->packet = OPENSSL_memdup(rl->packet, rl->packet_length);
    if (rdata->packet == NULL) {
        OPENSSL_free(rdata);
        pitem_free(item);
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_CRYPTO_LIB);
        return -1;
    }
    memcpy(&(rdata->rrec), &rl->rrec[0], sizeof(TLS_RL_RECORD));

    item->data = rdata;

    if (pqueue_insert(queue, item) == NULL) {
        /* Must be a duplicate so ignore it */
        OPENSSL_free(rdata->packet);
        OPENSSL_free(rdata);
        pitem_free(item);
    }

    return 1;
}

/* rfc9147 section 4.2.3 */
int dtls_crypt_sequence_number(EVP_CIPHER_CTX *ctx, unsigned char *seq, size_t seqlen,
    unsigned char *rec_data)
{
    unsigned char mask[16];
    int outlen, inlen;
    unsigned char *in, *iv;
    size_t i;
    unsigned char zeros[16] = { 0 };

    inlen = (int)(sizeof(mask));

    in = rec_data;
    iv = NULL;
    memset(mask, 0, sizeof(mask));

    /*
     * When the AEAD is based on ChaCha20, the first 4 bytes of the ciphertext
     * are treated as the block counter and the next 12 bytes as the nonce.
     * These are passed together as the IV (counter || nonce) to reinitialise
     * the cipher, and a zero block is encrypted to produce the mask.
     */
    if (EVP_CIPHER_CTX_get_nid(ctx) == NID_chacha20) {
        iv = rec_data;
        in = zeros;
        inlen = sizeof(zeros);
    }

    if (!ossl_assert(inlen >= 0)
        || (size_t)inlen > sizeof(mask)
        || !EVP_CIPHER_CTX_set_padding(ctx, 0)
        || EVP_CipherInit_ex2(ctx, NULL, NULL, iv, 1, NULL) <= 0
        || EVP_CipherUpdate(ctx, mask, &outlen, in, inlen) <= 0
        || outlen != inlen
        || EVP_CipherFinal_ex(ctx, mask + outlen, &outlen) <= 0
        || outlen != 0)
        return 0;

    if (!ossl_assert(seqlen <= sizeof(mask)))
        return 0;

    for (i = 0; i < seqlen; i++)
        seq[i] ^= mask[i];

    OPENSSL_cleanse(mask, sizeof(mask));

    return 1;
}

/*
 * Reconstruct the full sequence number as recommended by rfc9147 section
 * 4.2.2. Select the candidate closest to the replay window's right edge plus
 * one, ignore candidates outside the uint64_t range, and break ties forward.
 * An empty replay window is represented by max_seq_num == 0.
 */
uint64_t dtls13_reconstruct_seq_num(uint64_t max_seq_num, uint64_t truncated,
    size_t seqlen)
{
    uint64_t mask, period, expected, candidate, alt, best, best_dist, dist;

    mask = DTLS13_UNI_HDR_SEQ_MASK(seqlen);
    period = mask + 1;

    /* At the end of the range only candidates in the last block can be valid. */
    if (max_seq_num == UINT64_MAX)
        return (UINT64_MAX & ~mask) | truncated;

    expected = max_seq_num + 1;

    /* The candidate in the same period-sized block as |expected| */
    candidate = (expected & ~mask) | truncated;
    best = candidate;
    best_dist = candidate > expected ? candidate - expected
                                     : expected - candidate;

    /* The candidate one period behind, if it does not underflow */
    if (candidate >= period) {
        alt = candidate - period;
        dist = alt > expected ? alt - expected : expected - alt;
        /* Strictly closer only: a tie goes to the forward candidate */
        if (dist < best_dist) {
            best = alt;
            best_dist = dist;
        }
    }

    /* The candidate one period ahead, if it does not overflow */
    if (candidate <= UINT64_MAX - period) {
        alt = candidate + period;
        dist = alt > expected ? alt - expected : expected - alt;
        /* Ties go forward */
        if (dist <= best_dist) {
            best = alt;
            best_dist = dist;
        }
    }

    return best;
}

/*
 * Epoch 2 is always the fixed DTLS 1.3 handshake epoch: no compliant peer
 * ever sends application data there (unlike epoch 1's early data). A record
 * that only authenticates because of that epoch's retained keys must never
 * be delivered as application data -- retained *application* epochs (3+,
 * from KeyUpdate recovery) are unaffected, since they can legitimately
 * carry reordered application traffic.
 */
int dtls_prev_epoch_allows_type(const OSSL_RECORD_LAYER *crypto_rl, int type)
{
    return !(crypto_rl->epoch == 2 && type == SSL3_RT_APPLICATION_DATA);
}

/*-
 * Call this to get a new input record.
 * It will return <= 0 if more data is needed, normally due to an error
 * or non-blocking IO.
 * When it finishes, one packet has been decoded and can be found in
 * ssl->s3.rrec.type    - is the type of record
 * ssl->s3.rrec.data    - data
 * ssl->s3.rrec.length  - number of bytes
 */
int dtls_get_more_records(OSSL_RECORD_LAYER *rl)
{
    int rret;
    size_t more, nread = 0;
    TLS_RL_RECORD *rr;
    DTLS_BITMAP *bitmap;
    unsigned int is_next_epoch;
    unsigned char recseqnum[6];
    size_t recseqnumlen = 0;
    size_t rechdrlen = 0;
    size_t recseqnumoffs = 0;
    int buffered_record = 0;
    OSSL_RECORD_LAYER *crypto_rl;

    rl->num_recs = 0;
    rl->curr_rec = 0;
    rl->num_released = 0;

    rr = rl->rrec;

    if (rl->rbuf.buf == NULL) {
        if (!tls_setup_read_buffer(rl)) {
            /* RLAYERfatal() already called */
            return OSSL_RECORD_RETURN_FATAL;
        }
    }

again:
    crypto_rl = rl;
    memset(recseqnum, 0, sizeof(recseqnum));

    /* get something from the wire */

    /* check if we have the header */
    if (rl->rstate != SSL_ST_READ_BODY
        || rl->packet_length < DTLS1_RT_HEADER_LENGTH) {
        PACKET dtlsrecord;
        unsigned int record_type, record_version, epoch, length;
        uint64_t epoch64;

        /*
         * If we have buffered records and the original BIO READ has all been processed
         * let's leave and allow the Record Layer to update.
         */
        if (rl->version == DTLS1_3_VERSION
            && buffered_record == 1 && rl->rbuf.left == 0) {
            return OSSL_RECORD_RETURN_RETRY;
        }

        rret = rl->funcs->read_n(rl, DTLS1_RT_HEADER_LENGTH,
            TLS_BUFFER_get_len(&rl->rbuf), 0, 1, &nread);
        /* read timeout is handled by dtls1_read_bytes */
        if (rret < OSSL_RECORD_RETURN_SUCCESS) {
            /* RLAYERfatal() already called if appropriate */
            return rret; /* error or non-blocking */
        }

        /* this packet contained a partial record, dump it */
        if (rl->packet_length != DTLS1_RT_HEADER_LENGTH) {
            rl->packet_length = 0;
            goto again;
        }

        rl->rstate = SSL_ST_READ_BODY;

        if (!PACKET_buf_init(&dtlsrecord, rl->packet, rl->packet_length)
            || !PACKET_get_1(&dtlsrecord, &record_type)) {
            rl->packet_length = 0;
            goto again;
        }

        /* Pull apart the header into the DTLS1_RECORD */
        rr->type = (int)record_type;

        /*-
         * rfc9147:
         * Implementations can demultiplex DTLS 1.3 records by examining the first
         * byte as follows:
         *   * If the first byte is alert(21), handshake(22), or ack(proposed, 26),
         *     the record MUST be interpreted as a DTLSPlaintext record.
         *   * If the first byte is any other value, then receivers MUST check to
         *     see if the leading bits of the first byte are 001. If so, the implementation
         *     MUST process the record as DTLSCiphertext; the true content type
         *     will be inside the protected portion.
         *   * Otherwise, the record MUST be rejected as if it had failed deprotection,
         *     as described in Section 4.5.2.
         */
        if (rl->version == DTLS1_3_VERSION
            && rr->type != SSL3_RT_ALERT
            && rr->type != SSL3_RT_HANDSHAKE
            && rr->type != SSL3_RT_ACK
            && !DTLS13_UNI_HDR_FIX_BITS_IS_SET(rr->type)) {
            /* Silently discard */
            rr->length = 0;
            rl->packet_length = 0;
            goto again;
        }

        if (DTLS13_UNI_HDR_FIX_BITS_IS_SET(rr->type)) {
            /*
             * rfc9147:
             * receivers MUST check to if the leading bits of the first byte are 001.
             * If so, the implementation MUST process the record as DTLSCiphertext;
             */
            int cbitisset = DTLS13_UNI_HDR_CID_BIT_IS_SET(rr->type);
            int sbitisset = DTLS13_UNI_HDR_SEQ_BIT_IS_SET(rr->type);
            int lbitisset = DTLS13_UNI_HDR_LEN_BIT_IS_SET(rr->type);
            uint16_t eebits = rr->type & DTLS13_UNI_HDR_EPOCH_BITS_MASK;

            record_version = DTLS1_2_VERSION;
            epoch64 = rl->epoch;
            recseqnumlen = sbitisset ? 2 : 1;
            recseqnumoffs = sizeof(recseqnum) - recseqnumlen;

            if (/* OpenSSL does not support connection IDs: silently discard */
                cbitisset
                /*
                 * Naive approach? We expect sequence number to be filled already
                 * and then override the last bytes of the sequence number.
                 */
                || !PACKET_copy_bytes(&dtlsrecord, recseqnum + recseqnumoffs, recseqnumlen)
                /*
                 * rfc9147:
                 * The length field MAY be omitted by clearing the L bit, which means
                 * that the record consumes the entire rest of the datagram in the
                 * lower level transport
                 */
                || (lbitisset ? !PACKET_get_net_2(&dtlsrecord, &length)
                              : (length = (unsigned int)TLS_BUFFER_get_len(&rl->rbuf)) > 0)) {
                rr->length = 0;
                rl->packet_length = 0;
                goto again;
            }

            /*
             * RFC 9147 Section 4.2.2 Says that after the handshake phase (epoch 3+)
             * if the epoch bits do not match the current epoch we should use the
             * last epoch value. That would result in dropping the packet.
             */
            if ((epoch64 & DTLS13_UNI_HDR_EPOCH_BITS_MASK) != eebits) {
                /*
                 * Since we do not transition out of epoch 0 or early data until after
                 * we receive the next record if we are in epoch 0 or Early Data (epoch 1)
                 * and get an epoch 2 record we must update the epoch
                 */
                if (eebits == 2 && (epoch64 == 1 || epoch64 == 0)) {
                    epoch64 = 2;
                } else if (rl->prev_epoch_rl != NULL
                    && (rl->prev_epoch_rl->epoch
                           & DTLS13_UNI_HDR_EPOCH_BITS_MASK)
                        == eebits) {
                    /*
                     * This may be a retransmission at the epoch we have just
                     * moved on from, sent because the ACK we gave it was
                     * lost. Authenticate it with that epoch's own retained
                     * keys and replay window instead of dropping it.
                     */
                    epoch64 = rl->prev_epoch_rl->epoch;
                    crypto_rl = rl->prev_epoch_rl;
                } else {
                    rr->length = 0;
                    rl->packet_length = 0;
                    goto again;
                }
            }
        } else {
            if (!PACKET_get_net_2(&dtlsrecord, &record_version)
                || !PACKET_get_net_2(&dtlsrecord, &epoch)
                || !PACKET_copy_bytes(&dtlsrecord, recseqnum, 6)
                || !PACKET_get_net_2(&dtlsrecord, &length)) {
                rr->length = 0;
                rl->packet_length = 0;
                goto again;
            }
            epoch64 = epoch;
            recseqnumoffs = 0;
            recseqnumlen = 6;
        }

        rechdrlen = PACKET_data(&dtlsrecord) - rl->packet;
        rr->rec_version = (int)record_version;
        rr->epoch = epoch64;
        rr->length = length;

        if (rl->msg_callback != NULL)
            rl->msg_callback(0, rr->rec_version, SSL3_RT_HEADER, rl->packet,
                rechdrlen, rl->cbarg);

        /*
         * Lets check the version. We tolerate alerts that don't have the exact
         * version number (e.g. because of protocol version errors)
         */
        if (!rl->is_first_record && rr->type != SSL3_RT_ALERT
            /* DTLSv1.3 records sets the legacy version field to DTLSv1.2 */
            && !(rr->rec_version == DTLS1_2_VERSION
                && rl->version == DTLS1_3_VERSION)) {
            if (rr->rec_version != rl->version) {
                /* unexpected version, silently discard */
                rr->length = 0;
                rl->packet_length = 0;
                goto again;
            }
        }

        if (rr->rec_version >> 8 != (rl->version == DTLS_ANY_VERSION ? DTLS1_VERSION_MAJOR : rl->version >> 8)) {
            /* wrong version, silently discard record */
            rr->length = 0;
            rl->packet_length = 0;
            goto again;
        }

        if (rr->length > SSL3_RT_MAX_ENCRYPTED_LENGTH) {
            /* record too long, silently discard it */
            rr->length = 0;
            rl->packet_length = 0;
            goto again;
        }

        /*
         * If received packet overflows maximum possible fragment length then
         * silently discard it
         */
        if (rr->length > rl->max_frag_len + SSL3_RT_MAX_ENCRYPTED_OVERHEAD) {
            /* record too long, silently discard it */
            rr->length = 0;
            rl->packet_length = 0;
            goto again;
        }

        /* now rl->rstate == SSL_ST_READ_BODY */
    }

    /* rl->rstate == SSL_ST_READ_BODY, get and decode the data */

    if (rr->length > rl->packet_length - DTLS1_RT_HEADER_LENGTH) {
        /* now rl->packet_length == DTLS1_RT_HEADER_LENGTH */
        more = rr->length - (nread - rechdrlen);
        rret = rl->funcs->read_n(rl, more, more, 1, 1, &nread);
        /* this packet contained a partial record, dump it */
        if (rret < OSSL_RECORD_RETURN_SUCCESS || nread != more) {
            if (rl->alert != SSL_AD_NO_ALERT) {
                /* read_n() called RLAYERfatal() */
                return OSSL_RECORD_RETURN_FATAL;
            }
            rr->length = 0;
            rl->packet_length = 0;
            goto again;
        }

        /*
         * now n == rr->length,
         * and rl->packet_length ==  DTLS1_RT_HEADER_LENGTH + rr->length
         */
    }
    /* set state for later operations */
    rl->rstate = SSL_ST_READ_HEADER;

    /*
     * RFC 9147 permits DTLSPlaintext only in epoch 0. Silently discard it
     * after reading the fragment so later records remain framed.
     */
    if (rl->version == DTLS1_3_VERSION
        && rl->epoch != 0
        && !DTLS13_UNI_HDR_FIX_BITS_IS_SET(rr->type)) {
        rr->length = 0;
        rl->packet_length = 0;
        goto again;
    }

    /*
     * rfc9147:
     * This procedure requires the ciphertext length to be at least 16 bytes.
     * Receivers MUST reject shorter records as if they had failed deprotection
     */
    if (DTLS13_UNI_HDR_FIX_BITS_IS_SET(rr->type)
        && rl->version == DTLS1_3_VERSION
        && !(rl->in_init && rl->epoch == 0)
        && ((rl->packet_length < rechdrlen + DTLS13_CIPHERTEXT_MINSIZE)
            || (crypto_rl->sn_enc_ctx == NULL && crypto_rl->mac_ctx == NULL)
            || (crypto_rl->sn_enc_ctx != NULL
                && !dtls_crypt_sequence_number(crypto_rl->sn_enc_ctx,
                    recseqnum + recseqnumoffs,
                    recseqnumlen,
                    rl->packet + rechdrlen)))) {
        /* sequence number encryption failed dump record */
        rr->length = 0;
        rl->packet_length = 0;
        goto again;
    }

    if (rl->version == DTLS1_3_VERSION && rr->epoch == crypto_rl->epoch
        && DTLS13_UNI_HDR_FIX_BITS_IS_SET(rr->type)) {
        /* Reconstruct current-epoch unified records using its replay window. */
        uint64_t truncated = 0;
        size_t i;

        /* A unified header carries 8 or 16 bits of the sequence number */
        if (!ossl_assert(recseqnumlen == 1 || recseqnumlen == 2)) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return OSSL_RECORD_RETURN_FATAL;
        }

        for (i = 0; i < recseqnumlen; i++)
            truncated = (truncated << 8) | recseqnum[recseqnumoffs + i];

        crypto_rl->sequence = dtls13_reconstruct_seq_num(crypto_rl->bitmap.max_seq_num,
            truncated, recseqnumlen);
    } else {
        /*
         * DTLSPlaintext carries 48 bits. A buffered next-epoch unified record
         * is re-parsed after that epoch's record layer is installed, so this
         * provisional value is unused.
         */
        rl->sequence = ((uint64_t)recseqnum[0]) << 40;
        rl->sequence |= ((uint64_t)recseqnum[1]) << 32;
        rl->sequence |= ((uint64_t)recseqnum[2]) << 24;
        rl->sequence |= ((uint64_t)recseqnum[3]) << 16;
        rl->sequence |= ((uint64_t)recseqnum[4]) << 8;
        rl->sequence |= ((uint64_t)recseqnum[5]) << 0;
    }

    /* match epochs.  NULL means the packet is dropped on the floor */
    bitmap = dtls_get_bitmap(crypto_rl, rr, &is_next_epoch);
    if (bitmap == NULL && !is_next_epoch) {
        rr->length = 0;
        rl->packet_length = 0; /* dump this record */
        goto again; /* get another record */
    }

    /*
     * If this record is from the next epoch (either HM or ALERT), and a
     * handshake is currently in progress, buffer it since it cannot be
     * processed at this time.
     */
    if (is_next_epoch) {
        /*
         * DTLS 1.3 has two scenarios when to buffer the record
         * from the next epoch
         * 1) during handshake when rl->in_init is set and
         *    we got an epoch 0 record and the record layer
         *    is at epoch 2
         * 2) during early data when rl-epoch is 1 and we
         *    got an epoch 2 record
         */
        if ((rl->in_init && rl->version != DTLS1_3_VERSION)
            || (rl->in_init && rl->epoch == 0 && rr->epoch == 2)
            || (rl->version == DTLS1_3_VERSION
                && ((rl->in_init && rl->epoch == 0 && rr->epoch == 2)
                    || (rl->epoch == 1 && rr->epoch == 2)))) {

            uint64_t unprocessed_record_priority = rr->seq_num;

            /*
             * DTLS1.3 uses encrypted sequence numbers so we want to set the
             * priority base off of epoch.
             */
            if (rl->version == DTLS1_3_VERSION || (rr->epoch == 2 && rl->epoch == 0)) {
                if (rr->epoch == 1)
                    unprocessed_record_priority = rl->dtls13_epoch_1_seq++;
                else if (rr->epoch == 2)
                    unprocessed_record_priority = rl->dtls13_epoch_2_seq++;
            }
            if (dtls_rlayer_buffer_record(rl, &rl->unprocessed_rcds,
                    unprocessed_record_priority)
                < 0) {
                /* RLAYERfatal() already called */
                return OSSL_RECORD_RETURN_FATAL;
            }
            buffered_record = 1;
        }
        rr->length = 0;
        rl->packet_length = 0;
        goto again;
    }

    /*
     * dtls_record_replay_check() and dtls_process_record() read and write
     * whichever OSSL_RECORD_LAYER they are passed, not rr. When crypto_rl is
     * the retained previous epoch, seed its record slot from rr so they
     * have this record's data to work with; a no-op when crypto_rl == rl.
     */
    if (crypto_rl != rl) {
        crypto_rl->packet = rl->packet;
        crypto_rl->packet_length = rl->packet_length;
        crypto_rl->rrec[0] = *rr;
    }

#ifndef OPENSSL_NO_SCTP
    /* Only do replay check if no SCTP bio (also check for NULL bio) */
    if (rl->bio == NULL || !BIO_dgram_is_sctp(rl->bio)) {
#endif
        /* Check whether this is a repeat, or aged record. */
        if (!dtls_record_replay_check(crypto_rl, bitmap)) {
            rr->length = 0;
            rl->packet_length = 0; /* dump this record */
            goto again; /* get another record */
        }
#ifndef OPENSSL_NO_SCTP
    }
#endif

    /* just read a 0 length packet */
    if (rr->length == 0)
        goto again;

    if (!dtls_process_record(crypto_rl, bitmap)) {
        if (crypto_rl->alert != SSL_AD_NO_ALERT) {
            /* dtls_process_record() called RLAYERfatal */
            rl->alert = crypto_rl->alert;
            return OSSL_RECORD_RETURN_FATAL;
        }
        rr->length = 0;
        rl->packet_length = 0; /* dump this record */
        goto again; /* get another record */
    }

    if (crypto_rl != rl) {
        *rr = crypto_rl->rrec[0];
        rl->packet_length = 0;
    }

    if (rl->funcs->post_process_record && !rl->funcs->post_process_record(rl, rr)) {
        /* RLAYERfatal already called */
        return OSSL_RECORD_RETURN_FATAL;
    }

    /* rr->type is only decoded after post_process_record() above. */
    if (crypto_rl != rl && !dtls_prev_epoch_allows_type(crypto_rl, rr->type)) {
        rr->length = 0;
        goto again;
    }

    if (rr->length == 0) {
        /* No payload data in this record. Dump it */
        rl->packet_length = 0;
        goto again;
    }

    rl->num_recs = 1;
    return OSSL_RECORD_RETURN_SUCCESS;
}

/*
 * Push any unread buffered bytes and any records already buffered in
 * unprocessed_rcds (see is_next_epoch in dtls_get_more_records()) forward
 * into rl->next, so a pending handshake epoch transition still completes.
 * Must run when rl stops being the active read layer, not deferred until
 * it is eventually freed -- see dtls_free() and dtls_set_prev_epoch_rl().
 */
static int dtls_forward_pending_records(OSSL_RECORD_LAYER *rl)
{
    TLS_BUFFER *rbuf = &rl->rbuf;
    size_t left, written;
    pitem *item;
    DTLS_RLAYER_RECORD_DATA *rdata;
    int ret = 1;

    left = rbuf->left;
    if (left > 0) {
        ret = BIO_write_ex(rl->next, rbuf->buf + rbuf->offset, left, &written);
        rbuf->left = 0;
    }

    while ((item = pqueue_pop(&rl->unprocessed_rcds)) != NULL) {
        rdata = (DTLS_RLAYER_RECORD_DATA *)item->data;

        ret &= BIO_write_ex(rl->next, rdata->packet, rdata->packet_length,
            &written);
        OPENSSL_free(rdata->packet);
        OPENSSL_free(item->data);
        pitem_free(item);
    }

    return ret;
}

static int dtls_free(OSSL_RECORD_LAYER *rl)
{
    int ret = dtls_forward_pending_records(rl);

    if (rl->prev_epoch_rl != NULL) {
        ret &= dtls_free(rl->prev_epoch_rl);
        rl->prev_epoch_rl = NULL;
    }

    return tls_free(rl) && ret;
}

/*
 * Take ownership of the previous record layer, just superseded by the
 * record layer, instead of the caller freeing it.
 */
static int dtls_set_prev_epoch_rl(OSSL_RECORD_LAYER *rl, OSSL_RECORD_LAYER *prev)
{
    int ret = dtls_forward_pending_records(prev);

    if (prev->prev_epoch_rl != NULL) {
        ret &= dtls_free(prev->prev_epoch_rl);
        prev->prev_epoch_rl = NULL;
    }

    /*
     * prev's own read buffer is never used again: authenticating a
     * retransmission at this epoch reuses the active layer's packet buffer
     * (see the crypto_rl handling in dtls_get_more_records()). Release it
     * now instead of leaving it allocated until the whole layer chain is
     * torn down or the caller happens to call SSL_free_buffers().
     */
    ret &= tls_release_read_buffer(prev);

    rl->prev_epoch_rl = prev;
    return ret;
}

static int
dtls_new_record_layer(OSSL_LIB_CTX *libctx, const char *propq, int vers,
    int role, int direction, int level, uint64_t epoch,
    unsigned char *secret, size_t secretlen,
    unsigned char *snkey, unsigned char *key, size_t keylen,
    unsigned char *iv, size_t ivlen,
    unsigned char *mackey, size_t mackeylen,
    const EVP_CIPHER *snciph,
    const EVP_CIPHER *ciph, size_t taglen,
    int mactype,
    const EVP_MD *md, COMP_METHOD *comp,
    const EVP_MD *kdfdigest, BIO *prev, BIO *transport,
    BIO *next,
    int use_urxe,
    const OSSL_PARAM *settings, const OSSL_PARAM *options,
    const OSSL_DISPATCH *fns, void *cbarg, void *rlarg,
    OSSL_RECORD_LAYER **retrl)
{
    int ret;

    ret = tls_int_new_record_layer(libctx, propq, vers, role, direction, level,
        ciph, taglen, md, comp, prev,
        transport, next, settings,
        options, fns, cbarg, retrl);

    if (ret != OSSL_RECORD_RETURN_SUCCESS)
        return ret;

    (*retrl)->isdtls = 1;
    (*retrl)->epoch = epoch;
    (*retrl)->in_init = 1;
    (*retrl)->dtls13_epoch_1_seq = 0;
    (*retrl)->dtls13_epoch_2_seq = 100;
    (*retrl)->use_urxe = use_urxe;

    switch (vers) {
    case DTLS_ANY_VERSION:
        (*retrl)->funcs = &dtls_any_funcs;
        break;
    case DTLS1_3_VERSION:
        (*retrl)->funcs = &dtls_1_3_funcs;
        break;
    case DTLS1_2_VERSION:
    case DTLS1_VERSION:
    case DTLS1_BAD_VER:
        (*retrl)->funcs = &dtls_1_funcs;
        break;
    default:
        /* Should not happen */
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        ret = OSSL_RECORD_RETURN_FATAL;
        goto err;
    }

    ret = (*retrl)->funcs->set_crypto_state(*retrl, level, snkey, key, keylen,
        iv, ivlen, mackey, mackeylen,
        snciph, ciph, taglen, mactype, md,
        comp);

err:
    if (ret != OSSL_RECORD_RETURN_SUCCESS) {
        dtls_free(*retrl);
        *retrl = NULL;
    }
    return ret;
}

int dtls_prepare_record_header(OSSL_RECORD_LAYER *rl,
    WPACKET *thispkt,
    OSSL_RECORD_TEMPLATE *templ,
    uint8_t rectype,
    unsigned char **recdata)
{
    size_t maxcomplen;
    int unifiedheader = rl->version == DTLS1_3_VERSION && rl->epoch > 0;

    templ->sequence_number = rl->sequence;
    templ->epoch = rl->epoch;
    *recdata = NULL;
    maxcomplen = templ->buflen;

    if (rl->compctx != NULL)
        maxcomplen += SSL3_RT_MAX_COMPRESSED_OVERHEAD;

    if (unifiedheader) {
        uint8_t fixedbits = 0x20;
        uint8_t cbit = 0;
        uint8_t sbit = DTLS13_UNI_HDR_SEQ_BIT;
        uint8_t lbit = DTLS13_UNI_HDR_LEN_BIT;
        uint8_t ebits = rl->epoch & DTLS13_UNI_HDR_EPOCH_BITS_MASK;
        uint8_t unifiedhdrbits = fixedbits | cbit | sbit | lbit | ebits;
        uint64_t seqnum;

        /* Truncate only the wire encoding, not the AEAD nonce counter. */
        seqnum = rl->sequence & DTLS13_UNI_HDR_SEQ_MASK(sbit ? 2 : 1);

        if (!WPACKET_put_bytes_u8(thispkt, unifiedhdrbits)
            || (sbit ? !WPACKET_put_bytes_u16(thispkt, seqnum)
                     : !WPACKET_put_bytes_u8(thispkt, seqnum))
            || !WPACKET_start_sub_packet_u16(thispkt)
            || (rl->eivlen > 0
                && !WPACKET_allocate_bytes(thispkt, rl->eivlen, NULL))
            || (maxcomplen > 0
                && !WPACKET_reserve_bytes(thispkt, maxcomplen, recdata))) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    } else {
        if (!WPACKET_put_bytes_u8(thispkt, rectype)
            || !WPACKET_put_bytes_u16(thispkt, templ->version)
            || !WPACKET_put_bytes_u16(thispkt, templ->epoch)
            || !WPACKET_put_bytes_u48(thispkt, templ->sequence_number)
            || !WPACKET_start_sub_packet_u16(thispkt)
            || (rl->eivlen > 0
                && !WPACKET_allocate_bytes(thispkt, rl->eivlen, NULL))
            || (maxcomplen > 0
                && !WPACKET_reserve_bytes(thispkt, maxcomplen, recdata))) {
            RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }

    return 1;
}

int dtls_post_encryption_processing(OSSL_RECORD_LAYER *rl,
    size_t mac_size,
    OSSL_RECORD_TEMPLATE *thistempl,
    WPACKET *thispkt,
    TLS_RL_RECORD *thiswr)
{
    if (!tls_post_encryption_processing_default(rl, mac_size, thistempl,
            thispkt, thiswr)) {
        /* RLAYERfatal() already called */
        return 0;
    }

    return dtls_increment_sequence_ctr(rl);
}

static int dtls_increment_sequence_ctr(OSSL_RECORD_LAYER *rl)
{
    if (rl->version == DTLS1_3_VERSION)
        return tls_increment_sequence_ctr(rl);

    if (rl->sequence >= 0xffffffffffffULL) {
        RLAYERfatal(rl, SSL_AD_INTERNAL_ERROR, SSL_R_SEQUENCE_CTR_WRAPPED);
        return 0;
    }

    rl->sequence++;
    return 1;
}

static size_t dtls_get_max_record_overhead(OSSL_RECORD_LAYER *rl)
{
    size_t blocksize = 0, contenttypelen = 0;
    size_t rchdrlen = tls_get_record_header_len(rl);

    if (rl->enc_ctx != NULL && (EVP_CIPHER_CTX_get_mode(rl->enc_ctx) == EVP_CIPH_CBC_MODE))
        blocksize = EVP_CIPHER_CTX_get_block_size(rl->enc_ctx);

    /* DTLSv1.3 adds an extra content type byte after payload data */
    if (rl->version == DTLS1_3_VERSION)
        contenttypelen = 1;

    /*
     * If we have a cipher in place then the tag is mandatory. If the cipher is
     * CBC mode then an explicit IV is also mandatory. If we know the digest,
     * then we check it is consistent with the taglen. In the case of stitched
     * ciphers or AEAD ciphers we don't now the digest (or there isn't one) so
     * we just trust that the taglen is correct.
     */
    assert(rl->enc_ctx == NULL || ((blocksize == 0 || rl->eivlen > 0) && rl->taglen > 0));
    assert(rl->md == NULL || (int)rl->taglen == EVP_MD_size(rl->md));

    /*
     * Record overhead consists of the record header, the explicit IV, any
     * expansion due to cbc padding, and the mac/tag len. There could be
     * further expansion due to compression - but we don't know what this will
     * be without knowing the length of the data. However when this function is
     * called we don't know what the length will be yet - so this is a catch-22.
     * We *could* use SSL_3_RT_MAX_COMPRESSED_OVERHEAD which is an upper limit
     * for the maximum record size. But this value is larger than our fallback
     * MTU size - so isn't very helpful. We just ignore potential expansion
     * due to compression.
     */
    return rchdrlen + rl->eivlen + blocksize + rl->taglen + contenttypelen;
}

static int dtls_get_sequence_number(OSSL_RECORD_LAYER *rl, uint64_t *sequence)
{
    *sequence = rl->sequence;
    return 1;
}

static int dtls_set_sequence_number(OSSL_RECORD_LAYER *rl, uint64_t sequence)
{
    rl->sequence = sequence;
    return 1;
}

static int dtls_get_epoch(OSSL_RECORD_LAYER *rl, uint64_t *epoch)
{
    *epoch = rl->epoch;
    return 1;
}

static int dtls_set_curr_mtu(OSSL_RECORD_LAYER *rl, size_t mtu)
{
    rl->curr_mtu = mtu;
    return 1;
}

static size_t dtls_unprocessed_records(OSSL_RECORD_LAYER *rl)
{
    return pqueue_size(&rl->unprocessed_rcds);
}

const OSSL_RECORD_METHOD ossl_dtls_record_method = {
    dtls_new_record_layer,
    dtls_free,
    tls_unprocessed_read_pending,
    tls_processed_read_pending,
    tls_app_data_pending,
    tls_get_max_records,
    tls_write_records,
    tls_retry_write_records,
    tls_read_record,
    tls_release_record,
    tls_get_alert_code,
    tls_set1_bio,
#ifndef OPENSSL_NO_SOCK
    tls_set1_peer,
#else
    NULL,
#endif
    tls_set_use_urxe,
    tls_set_protocol_version,
    tls_set_plain_alerts,
    tls_set_first_handshake,
    tls_set_max_pipelines,
    dtls_set_in_init,
    tls_get_state,
    tls_set_options,
    tls_get_compression,
    tls_set_max_frag_len,
    dtls_get_max_record_overhead,
    dtls_increment_sequence_ctr,
    dtls_get_sequence_number,
    dtls_set_sequence_number,
    dtls_get_epoch,
    dtls_set_curr_mtu,
    dtls_unprocessed_records,
    tls_alloc_buffers,
    tls_free_buffers,
    dtls_set_prev_epoch_rl
};
