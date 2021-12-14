/*
 * Copyright 1995-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "../ssl_local.h"
#include <openssl/trace.h>
#include <openssl/rand.h>
#include <openssl/core_names.h>
#include "record_local.h"
#include "internal/cryptlib.h"

static const unsigned char ssl3_pad_1[48] = {
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36
};

static const unsigned char ssl3_pad_2[48] = {
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c
};

/*
 * Clear the contents of an SSL3_RECORD but retain any memory allocated
 */
void SSL3_RECORD_clear(SSL3_RECORD *r, size_t num_recs)
{
    unsigned char *comp;
    size_t i;

    for (i = 0; i < num_recs; i++) {
        comp = r[i].comp;

        memset(&r[i], 0, sizeof(*r));
        r[i].comp = comp;
    }
}

void SSL3_RECORD_release(SSL3_RECORD *r, size_t num_recs)
{
    size_t i;

    for (i = 0; i < num_recs; i++) {
        OPENSSL_free(r[i].comp);
        r[i].comp = NULL;
    }
}

void SSL3_RECORD_set_seq_num(SSL3_RECORD *r, const unsigned char *seq_num)
{
    memcpy(r->seq_num, seq_num, SEQ_NUM_SIZE);
}

/*
 * Peeks ahead into "read_ahead" data to see if we have a whole record waiting
 * for us in the buffer.
 */
static int ssl3_record_app_data_waiting(SSL *s)
{
    SSL3_BUFFER *rbuf;
    size_t left, len;
    unsigned char *p;

    rbuf = RECORD_LAYER_get_rbuf(&s->rlayer);

    p = SSL3_BUFFER_get_buf(rbuf);
    if (p == NULL)
        return 0;

    left = SSL3_BUFFER_get_left(rbuf);

    if (left < SSL3_RT_HEADER_LENGTH)
        return 0;

    p += SSL3_BUFFER_get_offset(rbuf);

    /*
     * We only check the type and record length, we will sanity check version
     * etc later
     */
    if (*p != SSL3_RT_APPLICATION_DATA)
        return 0;

    p += 3;
    n2s(p, len);

    if (left < SSL3_RT_HEADER_LENGTH + len)
        return 0;

    return 1;
}

int early_data_count_ok(SSL *s, size_t length, size_t overhead, int send)
{
    uint32_t max_early_data;
    SSL_SESSION *sess = s->session;

    /*
     * If we are a client then we always use the max_early_data from the
     * session/psksession. Otherwise we go with the lowest out of the max early
     * data set in the session and the configured max_early_data.
     */
    if (!s->server && sess->ext.max_early_data == 0) {
        if (!ossl_assert(s->psksession != NULL
                         && s->psksession->ext.max_early_data > 0)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        sess = s->psksession;
    }

    if (!s->server)
        max_early_data = sess->ext.max_early_data;
    else if (s->ext.early_data != SSL_EARLY_DATA_ACCEPTED)
        max_early_data = s->recv_max_early_data;
    else
        max_early_data = s->recv_max_early_data < sess->ext.max_early_data
                         ? s->recv_max_early_data : sess->ext.max_early_data;

    if (max_early_data == 0) {
        SSLfatal(s, send ? SSL_AD_INTERNAL_ERROR : SSL_AD_UNEXPECTED_MESSAGE,
                 SSL_R_TOO_MUCH_EARLY_DATA);
        return 0;
    }

    /* If we are dealing with ciphertext we need to allow for the overhead */
    max_early_data += overhead;

    if (s->early_data_count + length > max_early_data) {
        SSLfatal(s, send ? SSL_AD_INTERNAL_ERROR : SSL_AD_UNEXPECTED_MESSAGE,
                 SSL_R_TOO_MUCH_EARLY_DATA);
        return 0;
    }
    s->early_data_count += length;

    return 1;
}

/*
 * MAX_EMPTY_RECORDS defines the number of consecutive, empty records that
 * will be processed per call to ssl3_get_record. Without this limit an
 * attacker could send empty records at a faster rate than we can process and
 * cause ssl3_get_record to loop forever.
 */
#define MAX_EMPTY_RECORDS 32

#define SSL2_RT_HEADER_LENGTH   2
/*-
 * Call this to get new input records.
 * It will return <= 0 if more data is needed, normally due to an error
 * or non-blocking IO.
 * When it finishes, |numrpipes| records have been decoded. For each record 'i':
 * rr[i].type    - is the type of record
 * rr[i].data,   - data
 * rr[i].length, - number of bytes
 * Multiple records will only be returned if the record types are all
 * SSL3_RT_APPLICATION_DATA. The number of records returned will always be <=
 * |max_pipelines|
 */
/* used only by ssl3_read_bytes */
int ssl3_get_record(SSL *s)
{
    int enc_err, rret;
    int i;
    size_t more, n;
    SSL3_RECORD *rr, *thisrr;
    SSL3_BUFFER *rbuf;
    SSL_SESSION *sess;
    unsigned char *p;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int version;
    size_t mac_size = 0;
    int imac_size;
    size_t num_recs = 0, max_recs, j;
    PACKET pkt, sslv2pkt;
    int is_ktls_left;
    SSL_MAC_BUF *macbufs = NULL;
    int ret = -1;

    rr = RECORD_LAYER_get_rrec(&s->rlayer);
    rbuf = RECORD_LAYER_get_rbuf(&s->rlayer);
    is_ktls_left = (rbuf->left > 0);
    max_recs = s->max_pipelines;
    if (max_recs == 0)
        max_recs = 1;
    sess = s->session;

    do {
        thisrr = &rr[num_recs];

        /* check if we have the header */
        if ((RECORD_LAYER_get_rstate(&s->rlayer) != SSL_ST_READ_BODY) ||
            (RECORD_LAYER_get_packet_length(&s->rlayer)
             < SSL3_RT_HEADER_LENGTH)) {
            size_t sslv2len;
            unsigned int type;

            rret = ssl3_read_n(s, SSL3_RT_HEADER_LENGTH,
                               SSL3_BUFFER_get_len(rbuf), 0,
                               num_recs == 0 ? 1 : 0, &n);
            if (rret <= 0) {
#ifndef OPENSSL_NO_KTLS
                if (!BIO_get_ktls_recv(s->rbio) || rret == 0)
                    return rret;     /* error or non-blocking */
                switch (errno) {
                case EBADMSG:
                    SSLfatal(s, SSL_AD_BAD_RECORD_MAC,
                             SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC);
                    break;
                case EMSGSIZE:
                    SSLfatal(s, SSL_AD_RECORD_OVERFLOW,
                             SSL_R_PACKET_LENGTH_TOO_LONG);
                    break;
                case EINVAL:
                    SSLfatal(s, SSL_AD_PROTOCOL_VERSION,
                             SSL_R_WRONG_VERSION_NUMBER);
                    break;
                default:
                    break;
                }
#endif
                return rret;
            }
            RECORD_LAYER_set_rstate(&s->rlayer, SSL_ST_READ_BODY);

            p = RECORD_LAYER_get_packet(&s->rlayer);
            if (!PACKET_buf_init(&pkt, RECORD_LAYER_get_packet(&s->rlayer),
                                 RECORD_LAYER_get_packet_length(&s->rlayer))) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return -1;
            }
            sslv2pkt = pkt;
            if (!PACKET_get_net_2_len(&sslv2pkt, &sslv2len)
                    || !PACKET_get_1(&sslv2pkt, &type)) {
                SSLfatal(s, SSL_AD_DECODE_ERROR, ERR_R_INTERNAL_ERROR);
                return -1;
            }
            /*
             * The first record received by the server may be a V2ClientHello.
             */
            if (s->server && RECORD_LAYER_is_first_record(&s->rlayer)
                    && (sslv2len & 0x8000) != 0
                    && (type == SSL2_MT_CLIENT_HELLO)) {
                /*
                 *  SSLv2 style record
                 *
                 * |num_recs| here will actually always be 0 because
                 * |num_recs > 0| only ever occurs when we are processing
                 * multiple app data records - which we know isn't the case here
                 * because it is an SSLv2ClientHello. We keep it using
                 * |num_recs| for the sake of consistency
                 */
                thisrr->type = SSL3_RT_HANDSHAKE;
                thisrr->rec_version = SSL2_VERSION;

                thisrr->length = sslv2len & 0x7fff;

                if (thisrr->length > SSL3_BUFFER_get_len(rbuf)
                    - SSL2_RT_HEADER_LENGTH) {
                    SSLfatal(s, SSL_AD_RECORD_OVERFLOW,
                             SSL_R_PACKET_LENGTH_TOO_LONG);
                    return -1;
                }

                if (thisrr->length < MIN_SSL2_RECORD_LEN) {
                    SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_TOO_SHORT);
                    return -1;
                }
            } else {
                /* SSLv3+ style record */

                /* Pull apart the header into the SSL3_RECORD */
                if (!PACKET_get_1(&pkt, &type)
                        || !PACKET_get_net_2(&pkt, &version)
                        || !PACKET_get_net_2_len(&pkt, &thisrr->length)) {
                    if (s->msg_callback)
                        s->msg_callback(0, 0, SSL3_RT_HEADER, p, 5, s,
                                        s->msg_callback_arg);
                    SSLfatal(s, SSL_AD_DECODE_ERROR, ERR_R_INTERNAL_ERROR);
                    return -1;
                }
                thisrr->type = type;
                thisrr->rec_version = version;

                if (s->msg_callback)
                    s->msg_callback(0, version, SSL3_RT_HEADER, p, 5, s,
                                    s->msg_callback_arg);

                /*
                 * Lets check version. In TLSv1.3 we only check this field
                 * when encryption is occurring (see later check). For the
                 * ServerHello after an HRR we haven't actually selected TLSv1.3
                 * yet, but we still treat it as TLSv1.3, so we must check for
                 * that explicitly
                 */
                if (!s->first_packet && !SSL_IS_TLS13(s)
                        && s->hello_retry_request != SSL_HRR_PENDING
                        && version != (unsigned int)s->version) {
                    if ((s->version & 0xFF00) == (version & 0xFF00)
                        && !s->enc_write_ctx && !s->write_hash) {
                        if (thisrr->type == SSL3_RT_ALERT) {
                            /*
                             * The record is using an incorrect version number,
                             * but what we've got appears to be an alert. We
                             * haven't read the body yet to check whether its a
                             * fatal or not - but chances are it is. We probably
                             * shouldn't send a fatal alert back. We'll just
                             * end.
                             */
                            SSLfatal(s, SSL_AD_NO_ALERT,
                                     SSL_R_WRONG_VERSION_NUMBER);
                            return -1;
                        }
                        /*
                         * Send back error using their minor version number :-)
                         */
                        s->version = (unsigned short)version;
                    }
                    SSLfatal(s, SSL_AD_PROTOCOL_VERSION,
                             SSL_R_WRONG_VERSION_NUMBER);
                    return -1;
                }

                if ((version >> 8) != SSL3_VERSION_MAJOR) {
                    if (RECORD_LAYER_is_first_record(&s->rlayer)) {
                        /* Go back to start of packet, look at the five bytes
                         * that we have. */
                        p = RECORD_LAYER_get_packet(&s->rlayer);
                        if (strncmp((char *)p, "GET ", 4) == 0 ||
                            strncmp((char *)p, "POST ", 5) == 0 ||
                            strncmp((char *)p, "HEAD ", 5) == 0 ||
                            strncmp((char *)p, "PUT ", 4) == 0) {
                            SSLfatal(s, SSL_AD_NO_ALERT, SSL_R_HTTP_REQUEST);
                            return -1;
                        } else if (strncmp((char *)p, "CONNE", 5) == 0) {
                            SSLfatal(s, SSL_AD_NO_ALERT,
                                     SSL_R_HTTPS_PROXY_REQUEST);
                            return -1;
                        }

                        /* Doesn't look like TLS - don't send an alert */
                        SSLfatal(s, SSL_AD_NO_ALERT,
                                 SSL_R_WRONG_VERSION_NUMBER);
                        return -1;
                    } else {
                        SSLfatal(s, SSL_AD_PROTOCOL_VERSION,
                                 SSL_R_WRONG_VERSION_NUMBER);
                        return -1;
                    }
                }

                if (SSL_IS_TLS13(s) && s->enc_read_ctx != NULL) {
                    if (thisrr->type != SSL3_RT_APPLICATION_DATA
                            && (thisrr->type != SSL3_RT_CHANGE_CIPHER_SPEC
                                || !SSL_IS_FIRST_HANDSHAKE(s))
                            && (thisrr->type != SSL3_RT_ALERT
                                || s->statem.enc_read_state
                                   != ENC_READ_STATE_ALLOW_PLAIN_ALERTS)) {
                        SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE,
                                 SSL_R_BAD_RECORD_TYPE);
                        return -1;
                    }
                    if (thisrr->rec_version != TLS1_2_VERSION) {
                        SSLfatal(s, SSL_AD_DECODE_ERROR,
                                 SSL_R_WRONG_VERSION_NUMBER);
                        return -1;
                    }
                }

                if (thisrr->length >
                    SSL3_BUFFER_get_len(rbuf) - SSL3_RT_HEADER_LENGTH) {
                    SSLfatal(s, SSL_AD_RECORD_OVERFLOW,
                             SSL_R_PACKET_LENGTH_TOO_LONG);
                    return -1;
                }
            }

            /* now s->rlayer.rstate == SSL_ST_READ_BODY */
        }

        if (SSL_IS_TLS13(s)) {
            if (thisrr->length > SSL3_RT_MAX_TLS13_ENCRYPTED_LENGTH) {
                SSLfatal(s, SSL_AD_RECORD_OVERFLOW,
                         SSL_R_ENCRYPTED_LENGTH_TOO_LONG);
                return -1;
            }
        } else {
            size_t len = SSL3_RT_MAX_ENCRYPTED_LENGTH;

#ifndef OPENSSL_NO_COMP
            /*
             * If OPENSSL_NO_COMP is defined then SSL3_RT_MAX_ENCRYPTED_LENGTH
             * does not include the compression overhead anyway.
             */
            if (s->expand == NULL)
                len -= SSL3_RT_MAX_COMPRESSED_OVERHEAD;
#endif

            if (thisrr->length > len && !BIO_get_ktls_recv(s->rbio)) {
                SSLfatal(s, SSL_AD_RECORD_OVERFLOW,
                         SSL_R_ENCRYPTED_LENGTH_TOO_LONG);
                return -1;
            }
        }

        /*
         * s->rlayer.rstate == SSL_ST_READ_BODY, get and decode the data.
         * Calculate how much more data we need to read for the rest of the
         * record
         */
        if (thisrr->rec_version == SSL2_VERSION) {
            more = thisrr->length + SSL2_RT_HEADER_LENGTH
                - SSL3_RT_HEADER_LENGTH;
        } else {
            more = thisrr->length;
        }

        if (more > 0) {
            /* now s->rlayer.packet_length == SSL3_RT_HEADER_LENGTH */

            rret = ssl3_read_n(s, more, more, 1, 0, &n);
            if (rret <= 0)
                return rret;     /* error or non-blocking io */
        }

        /* set state for later operations */
        RECORD_LAYER_set_rstate(&s->rlayer, SSL_ST_READ_HEADER);

        /*
         * At this point, s->rlayer.packet_length == SSL3_RT_HEADER_LENGTH
         * + thisrr->length, or s->rlayer.packet_length == SSL2_RT_HEADER_LENGTH
         * + thisrr->length and we have that many bytes in s->rlayer.packet
         */
        if (thisrr->rec_version == SSL2_VERSION) {
            thisrr->input =
                &(RECORD_LAYER_get_packet(&s->rlayer)[SSL2_RT_HEADER_LENGTH]);
        } else {
            thisrr->input =
                &(RECORD_LAYER_get_packet(&s->rlayer)[SSL3_RT_HEADER_LENGTH]);
        }

        /*
         * ok, we can now read from 's->rlayer.packet' data into 'thisrr'.
         * thisrr->input points at thisrr->length bytes, which need to be copied
         * into thisrr->data by either the decryption or by the decompression.
         * When the data is 'copied' into the thisrr->data buffer,
         * thisrr->input will be updated to point at the new buffer
         */

        /*
         * We now have - encrypted [ MAC [ compressed [ plain ] ] ]
         * thisrr->length bytes of encrypted compressed stuff.
         */

        /* decrypt in place in 'thisrr->input' */
        thisrr->data = thisrr->input;
        thisrr->orig_len = thisrr->length;

        /* Mark this record as not read by upper layers yet */
        thisrr->read = 0;

        num_recs++;

        /* we have pulled in a full packet so zero things */
        RECORD_LAYER_reset_packet_length(&s->rlayer);
        RECORD_LAYER_clear_first_record(&s->rlayer);
    } while (num_recs < max_recs
             && thisrr->type == SSL3_RT_APPLICATION_DATA
             && SSL_USE_EXPLICIT_IV(s)
             && s->enc_read_ctx != NULL
             && (EVP_CIPHER_get_flags(EVP_CIPHER_CTX_get0_cipher(s->enc_read_ctx))
                 & EVP_CIPH_FLAG_PIPELINE) != 0
             && ssl3_record_app_data_waiting(s));

    if (num_recs == 1
            && thisrr->type == SSL3_RT_CHANGE_CIPHER_SPEC
            && (SSL_IS_TLS13(s) || s->hello_retry_request != SSL_HRR_NONE)
            && SSL_IS_FIRST_HANDSHAKE(s)) {
        /*
         * CCS messages must be exactly 1 byte long, containing the value 0x01
         */
        if (thisrr->length != 1 || thisrr->data[0] != 0x01) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER,
                     SSL_R_INVALID_CCS_MESSAGE);
            return -1;
        }
        /*
         * CCS messages are ignored in TLSv1.3. We treat it like an empty
         * handshake record
         */
        thisrr->type = SSL3_RT_HANDSHAKE;
        RECORD_LAYER_inc_empty_record_count(&s->rlayer);
        if (RECORD_LAYER_get_empty_record_count(&s->rlayer)
            > MAX_EMPTY_RECORDS) {
            SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE,
                     SSL_R_UNEXPECTED_CCS_MESSAGE);
            return -1;
        }
        thisrr->read = 1;
        RECORD_LAYER_set_numrpipes(&s->rlayer, 1);

        return 1;
    }

    /*
     * KTLS reads full records. If there is any data left,
     * then it is from before enabling ktls
     */
    if (BIO_get_ktls_recv(s->rbio) && !is_ktls_left)
        goto skip_decryption;

    if (s->read_hash != NULL) {
        const EVP_MD *tmpmd = EVP_MD_CTX_get0_md(s->read_hash);

        if (tmpmd != NULL) {
            imac_size = EVP_MD_get_size(tmpmd);
            if (!ossl_assert(imac_size >= 0 && imac_size <= EVP_MAX_MD_SIZE)) {
                    SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
                    return -1;
            }
            mac_size = (size_t)imac_size;
        }
    }

    /*
     * If in encrypt-then-mac mode calculate mac from encrypted record. All
     * the details below are public so no timing details can leak.
     */
    if (SSL_READ_ETM(s) && s->read_hash) {
        unsigned char *mac;

        for (j = 0; j < num_recs; j++) {
            thisrr = &rr[j];

            if (thisrr->length < mac_size) {
                SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_TOO_SHORT);
                return -1;
            }
            thisrr->length -= mac_size;
            mac = thisrr->data + thisrr->length;
            i = s->method->ssl3_enc->mac(s, thisrr, md, 0 /* not send */ );
            if (i == 0 || CRYPTO_memcmp(md, mac, mac_size) != 0) {
                SSLfatal(s, SSL_AD_BAD_RECORD_MAC,
                         SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC);
                return -1;
            }
        }
        /*
         * We've handled the mac now - there is no MAC inside the encrypted
         * record
         */
        mac_size = 0;
    }

    if (mac_size > 0) {
        macbufs = OPENSSL_zalloc(sizeof(*macbufs) * num_recs);
        if (macbufs == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE);
            return -1;
        }
    }

    enc_err = s->method->ssl3_enc->enc(s, rr, num_recs, 0, macbufs, mac_size);

    /*-
     * enc_err is:
     *    0: if the record is publicly invalid, or an internal error, or AEAD
     *       decryption failed, or ETM decryption failed.
     *    1: Success or MTE decryption failed (MAC will be randomised)
     */
    if (enc_err == 0) {
        if (ossl_statem_in_error(s)) {
            /* SSLfatal() already got called */
            goto end;
        }
        if (num_recs == 1 && ossl_statem_skip_early_data(s)) {
            /*
             * Valid early_data that we cannot decrypt will fail here. We treat
             * it like an empty record.
             */

            thisrr = &rr[0];

            if (!early_data_count_ok(s, thisrr->length,
                                     EARLY_DATA_CIPHERTEXT_OVERHEAD, 0)) {
                /* SSLfatal() already called */
                goto end;
            }

            thisrr->length = 0;
            thisrr->read = 1;
            RECORD_LAYER_set_numrpipes(&s->rlayer, 1);
            RECORD_LAYER_reset_read_sequence(&s->rlayer);
            ret = 1;
            goto end;
        }
        SSLfatal(s, SSL_AD_BAD_RECORD_MAC,
                 SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC);
        goto end;
    }
    OSSL_TRACE_BEGIN(TLS) {
        BIO_printf(trc_out, "dec %lu\n", (unsigned long)rr[0].length);
        BIO_dump_indent(trc_out, rr[0].data, rr[0].length, 4);
    } OSSL_TRACE_END(TLS);

    /* r->length is now the compressed data plus mac */
    if ((sess != NULL)
            && (s->enc_read_ctx != NULL)
            && (!SSL_READ_ETM(s) && EVP_MD_CTX_get0_md(s->read_hash) != NULL)) {
        /* s->read_hash != NULL => mac_size != -1 */

        for (j = 0; j < num_recs; j++) {
            SSL_MAC_BUF *thismb = &macbufs[j];
            thisrr = &rr[j];

            i = s->method->ssl3_enc->mac(s, thisrr, md, 0 /* not send */ );
            if (i == 0 || thismb == NULL || thismb->mac == NULL
                || CRYPTO_memcmp(md, thismb->mac, (size_t)mac_size) != 0)
                enc_err = 0;
            if (thisrr->length > SSL3_RT_MAX_COMPRESSED_LENGTH + mac_size)
                enc_err = 0;
        }
    }

    if (enc_err == 0) {
        if (ossl_statem_in_error(s)) {
            /* We already called SSLfatal() */
            goto end;
        }
        /*
         * A separate 'decryption_failed' alert was introduced with TLS 1.0,
         * SSL 3.0 only has 'bad_record_mac'.  But unless a decryption
         * failure is directly visible from the ciphertext anyway, we should
         * not reveal which kind of error occurred -- this might become
         * visible to an attacker (e.g. via a logfile)
         */
        SSLfatal(s, SSL_AD_BAD_RECORD_MAC,
                 SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC);
        goto end;
    }

 skip_decryption:

    for (j = 0; j < num_recs; j++) {
        thisrr = &rr[j];

        /* thisrr->length is now just compressed */
        if (s->expand != NULL) {
            if (thisrr->length > SSL3_RT_MAX_COMPRESSED_LENGTH) {
                SSLfatal(s, SSL_AD_RECORD_OVERFLOW,
                         SSL_R_COMPRESSED_LENGTH_TOO_LONG);
                goto end;
            }
            if (!ssl3_do_uncompress(s, thisrr)) {
                SSLfatal(s, SSL_AD_DECOMPRESSION_FAILURE,
                         SSL_R_BAD_DECOMPRESSION);
                goto end;
            }
        }

        if (SSL_IS_TLS13(s)
                && s->enc_read_ctx != NULL
                && thisrr->type != SSL3_RT_ALERT) {
            size_t end;

            if (thisrr->length == 0
                    || thisrr->type != SSL3_RT_APPLICATION_DATA) {
                SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_BAD_RECORD_TYPE);
                goto end;
            }

            /* Strip trailing padding */
            for (end = thisrr->length - 1; end > 0 && thisrr->data[end] == 0;
                 end--)
                continue;

            thisrr->length = end;
            thisrr->type = thisrr->data[end];
            if (thisrr->type != SSL3_RT_APPLICATION_DATA
                    && thisrr->type != SSL3_RT_ALERT
                    && thisrr->type != SSL3_RT_HANDSHAKE) {
                SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_BAD_RECORD_TYPE);
                goto end;
            }
            if (s->msg_callback)
                s->msg_callback(0, s->version, SSL3_RT_INNER_CONTENT_TYPE,
                                &thisrr->data[end], 1, s, s->msg_callback_arg);
        }

        /*
         * TLSv1.3 alert and handshake records are required to be non-zero in
         * length.
         */
        if (SSL_IS_TLS13(s)
                && (thisrr->type == SSL3_RT_HANDSHAKE
                    || thisrr->type == SSL3_RT_ALERT)
                && thisrr->length == 0) {
            SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_BAD_LENGTH);
            goto end;
        }

        if (thisrr->length > SSL3_RT_MAX_PLAIN_LENGTH
            && !BIO_get_ktls_recv(s->rbio)) {
            SSLfatal(s, SSL_AD_RECORD_OVERFLOW, SSL_R_DATA_LENGTH_TOO_LONG);
            goto end;
        }

        /* If received packet overflows current Max Fragment Length setting */
        if (s->session != NULL && USE_MAX_FRAGMENT_LENGTH_EXT(s->session)
                && thisrr->length > GET_MAX_FRAGMENT_LENGTH(s->session)
                && !BIO_get_ktls_recv(s->rbio)) {
            SSLfatal(s, SSL_AD_RECORD_OVERFLOW, SSL_R_DATA_LENGTH_TOO_LONG);
            goto end;
        }

        thisrr->off = 0;
        /*-
         * So at this point the following is true
         * thisrr->type   is the type of record
         * thisrr->length == number of bytes in record
         * thisrr->off    == offset to first valid byte
         * thisrr->data   == where to take bytes from, increment after use :-).
         */

        /* just read a 0 length packet */
        if (thisrr->length == 0) {
            RECORD_LAYER_inc_empty_record_count(&s->rlayer);
            if (RECORD_LAYER_get_empty_record_count(&s->rlayer)
                > MAX_EMPTY_RECORDS) {
                SSLfatal(s, SSL_AD_UNEXPECTED_MESSAGE, SSL_R_RECORD_TOO_SMALL);
                goto end;
            }
        } else {
            RECORD_LAYER_reset_empty_record_count(&s->rlayer);
        }
    }

    if (s->early_data_state == SSL_EARLY_DATA_READING) {
        thisrr = &rr[0];
        if (thisrr->type == SSL3_RT_APPLICATION_DATA
                && !early_data_count_ok(s, thisrr->length, 0, 0)) {
            /* SSLfatal already called */
            goto end;
        }
    }

    RECORD_LAYER_set_numrpipes(&s->rlayer, num_recs);
    ret = 1;
 end:
    if (macbufs != NULL) {
        for (j = 0; j < num_recs; j++) {
            if (macbufs[j].alloced)
                OPENSSL_free(macbufs[j].mac);
        }
        OPENSSL_free(macbufs);
    }
    return ret;
}

int ssl3_do_uncompress(SSL *ssl, SSL3_RECORD *rr)
{
#ifndef OPENSSL_NO_COMP
    int i;

    if (rr->comp == NULL) {
        rr->comp = (unsigned char *)
            OPENSSL_malloc(SSL3_RT_MAX_ENCRYPTED_LENGTH);
    }
    if (rr->comp == NULL)
        return 0;

    i = COMP_expand_block(ssl->expand, rr->comp,
                          SSL3_RT_MAX_PLAIN_LENGTH, rr->data, (int)rr->length);
    if (i < 0)
        return 0;
    else
        rr->length = i;
    rr->data = rr->comp;
#endif
    return 1;
}

int ssl3_do_compress(SSL *ssl, SSL3_RECORD *wr)
{
#ifndef OPENSSL_NO_COMP
    int i;

    i = COMP_compress_block(ssl->compress, wr->data,
                            (int)(wr->length + SSL3_RT_MAX_COMPRESSED_OVERHEAD),
                            wr->input, (int)wr->length);
    if (i < 0)
        return 0;
    else
        wr->length = i;

    wr->input = wr->data;
#endif
    return 1;
}

/*-
 * ssl3_enc encrypts/decrypts |n_recs| records in |inrecs|. Calls SSLfatal on
 * internal error, but not otherwise. It is the responsibility of the caller to
 * report a bad_record_mac
 *
 * Returns:
 *    0: if the record is publicly invalid, or an internal error
 *    1: Success or Mac-then-encrypt decryption failed (MAC will be randomised)
 */
int ssl3_enc(SSL *s, SSL3_RECORD *inrecs, size_t n_recs, int sending,
             SSL_MAC_BUF *mac, size_t macsize)
{
    SSL3_RECORD *rec;
    EVP_CIPHER_CTX *ds;
    size_t l, i;
    size_t bs;
    const EVP_CIPHER *enc;

    rec = inrecs;
    /*
     * We shouldn't ever be called with more than one record in the SSLv3 case
     */
    if (n_recs != 1)
        return 0;
    if (sending) {
        ds = s->enc_write_ctx;
        if (s->enc_write_ctx == NULL)
            enc = NULL;
        else
            enc = EVP_CIPHER_CTX_get0_cipher(s->enc_write_ctx);
    } else {
        ds = s->enc_read_ctx;
        if (s->enc_read_ctx == NULL)
            enc = NULL;
        else
            enc = EVP_CIPHER_CTX_get0_cipher(s->enc_read_ctx);
    }

    if ((s->session == NULL) || (ds == NULL) || (enc == NULL)) {
        memmove(rec->data, rec->input, rec->length);
        rec->input = rec->data;
    } else {
        int provided = (EVP_CIPHER_get0_provider(enc) != NULL);

        l = rec->length;
        bs = EVP_CIPHER_CTX_get_block_size(ds);

        /* COMPRESS */

        if ((bs != 1) && sending && !provided) {
            /*
             * We only do this for legacy ciphers. Provided ciphers add the
             * padding on the provider side.
             */
            i = bs - (l % bs);

            /* we need to add 'i-1' padding bytes */
            l += i;
            /*
             * the last of these zero bytes will be overwritten with the
             * padding length.
             */
            memset(&rec->input[rec->length], 0, i);
            rec->length += i;
            rec->input[l - 1] = (unsigned char)(i - 1);
        }

        if (!sending) {
            if (l == 0 || l % bs != 0) {
                /* Publicly invalid */
                return 0;
            }
            /* otherwise, rec->length >= bs */
        }

        if (EVP_CIPHER_get0_provider(enc) != NULL) {
            int outlen;

            if (!EVP_CipherUpdate(ds, rec->data, &outlen, rec->input,
                                  (unsigned int)l))
                return 0;
            rec->length = outlen;

            if (!sending && mac != NULL) {
                /* Now get a pointer to the MAC */
                OSSL_PARAM params[2], *p = params;

                /* Get the MAC */
                mac->alloced = 0;

                *p++ = OSSL_PARAM_construct_octet_ptr(OSSL_CIPHER_PARAM_TLS_MAC,
                                                      (void **)&mac->mac,
                                                      macsize);
                *p = OSSL_PARAM_construct_end();

                if (!EVP_CIPHER_CTX_get_params(ds, params)) {
                    /* Shouldn't normally happen */
                    SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    return 0;
                }
            }
        } else {
            if (EVP_Cipher(ds, rec->data, rec->input, (unsigned int)l) < 1) {
                /* Shouldn't happen */
                SSLfatal(s, SSL_AD_BAD_RECORD_MAC, ERR_R_INTERNAL_ERROR);
                return 0;
            }

            if (!sending)
                return ssl3_cbc_remove_padding_and_mac(&rec->length,
                                           rec->orig_len,
                                           rec->data,
                                           (mac != NULL) ? &mac->mac : NULL,
                                           (mac != NULL) ? &mac->alloced : NULL,
                                           bs,
                                           macsize,
                                           s->ctx->libctx);
        }
    }
    return 1;
}

#define MAX_PADDING 256
/*-
 * tls1_enc encrypts/decrypts |n_recs| in |recs|. Calls SSLfatal on internal
 * error, but not otherwise. It is the responsibility of the caller to report
 * a bad_record_mac - if appropriate (DTLS just drops the record).
 *
 * Returns:
 *    0: if the record is publicly invalid, or an internal error, or AEAD
 *       decryption failed, or Encrypt-then-mac decryption failed.
 *    1: Success or Mac-then-encrypt decryption failed (MAC will be randomised)
 */
int tls1_enc(SSL *s, SSL3_RECORD *recs, size_t n_recs, int sending,
             SSL_MAC_BUF *macs, size_t macsize)
{
    EVP_CIPHER_CTX *ds;
    size_t reclen[SSL_MAX_PIPELINES];
    unsigned char buf[SSL_MAX_PIPELINES][EVP_AEAD_TLS1_AAD_LEN];
    int i, pad = 0, tmpr;
    size_t bs, ctr, padnum, loop;
    unsigned char padval;
    const EVP_CIPHER *enc;
    int tlstree_enc = sending ? (s->mac_flags & SSL_MAC_FLAG_WRITE_MAC_TLSTREE)
                              : (s->mac_flags & SSL_MAC_FLAG_READ_MAC_TLSTREE);

    if (n_recs == 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    if (sending) {
        if (EVP_MD_CTX_get0_md(s->write_hash)) {
            int n = EVP_MD_CTX_get_size(s->write_hash);
            if (!ossl_assert(n >= 0)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }
        ds = s->enc_write_ctx;
        if (s->enc_write_ctx == NULL)
            enc = NULL;
        else {
            int ivlen;

            enc = EVP_CIPHER_CTX_get0_cipher(s->enc_write_ctx);
            /* For TLSv1.1 and later explicit IV */
            if (SSL_USE_EXPLICIT_IV(s)
                && EVP_CIPHER_get_mode(enc) == EVP_CIPH_CBC_MODE)
                ivlen = EVP_CIPHER_get_iv_length(enc);
            else
                ivlen = 0;
            if (ivlen > 1) {
                for (ctr = 0; ctr < n_recs; ctr++) {
                    if (recs[ctr].data != recs[ctr].input) {
                        /*
                         * we can't write into the input stream: Can this ever
                         * happen?? (steve)
                         */
                        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                        return 0;
                    } else if (RAND_bytes_ex(s->ctx->libctx, recs[ctr].input,
                                             ivlen, 0) <= 0) {
                        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                        return 0;
                    }
                }
            }
        }
    } else {
        if (EVP_MD_CTX_get0_md(s->read_hash)) {
            int n = EVP_MD_CTX_get_size(s->read_hash);
            if (!ossl_assert(n >= 0)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }
        ds = s->enc_read_ctx;
        if (s->enc_read_ctx == NULL)
            enc = NULL;
        else
            enc = EVP_CIPHER_CTX_get0_cipher(s->enc_read_ctx);
    }

    if ((s->session == NULL) || (ds == NULL) || (enc == NULL)) {
        for (ctr = 0; ctr < n_recs; ctr++) {
            memmove(recs[ctr].data, recs[ctr].input, recs[ctr].length);
            recs[ctr].input = recs[ctr].data;
        }
    } else {
        int provided = (EVP_CIPHER_get0_provider(enc) != NULL);

        bs = EVP_CIPHER_get_block_size(EVP_CIPHER_CTX_get0_cipher(ds));

        if (n_recs > 1) {
            if ((EVP_CIPHER_get_flags(EVP_CIPHER_CTX_get0_cipher(ds))
                  & EVP_CIPH_FLAG_PIPELINE) == 0) {
                /*
                 * We shouldn't have been called with pipeline data if the
                 * cipher doesn't support pipelining
                 */
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_PIPELINE_FAILURE);
                return 0;
            }
        }
        for (ctr = 0; ctr < n_recs; ctr++) {
            reclen[ctr] = recs[ctr].length;

            if ((EVP_CIPHER_get_flags(EVP_CIPHER_CTX_get0_cipher(ds))
                        & EVP_CIPH_FLAG_AEAD_CIPHER) != 0) {
                unsigned char *seq;

                seq = sending ? RECORD_LAYER_get_write_sequence(&s->rlayer)
                    : RECORD_LAYER_get_read_sequence(&s->rlayer);

                if (SSL_IS_DTLS(s)) {
                    /* DTLS does not support pipelining */
                    unsigned char dtlsseq[8], *p = dtlsseq;

                    s2n(sending ? DTLS_RECORD_LAYER_get_w_epoch(&s->rlayer) :
                        DTLS_RECORD_LAYER_get_r_epoch(&s->rlayer), p);
                    memcpy(p, &seq[2], 6);
                    memcpy(buf[ctr], dtlsseq, 8);
                } else {
                    memcpy(buf[ctr], seq, 8);
                    for (i = 7; i >= 0; i--) { /* increment */
                        ++seq[i];
                        if (seq[i] != 0)
                            break;
                    }
                }

                buf[ctr][8] = recs[ctr].type;
                buf[ctr][9] = (unsigned char)(s->version >> 8);
                buf[ctr][10] = (unsigned char)(s->version);
                buf[ctr][11] = (unsigned char)(recs[ctr].length >> 8);
                buf[ctr][12] = (unsigned char)(recs[ctr].length & 0xff);
                pad = EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_AEAD_TLS1_AAD,
                                          EVP_AEAD_TLS1_AAD_LEN, buf[ctr]);
                if (pad <= 0) {
                    SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    return 0;
                }

                if (sending) {
                    reclen[ctr] += pad;
                    recs[ctr].length += pad;
                }

            } else if ((bs != 1) && sending && !provided) {
                /*
                 * We only do this for legacy ciphers. Provided ciphers add the
                 * padding on the provider side.
                 */
                padnum = bs - (reclen[ctr] % bs);

                /* Add weird padding of up to 256 bytes */

                if (padnum > MAX_PADDING) {
                    SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    return 0;
                }
                /* we need to add 'padnum' padding bytes of value padval */
                padval = (unsigned char)(padnum - 1);
                for (loop = reclen[ctr]; loop < reclen[ctr] + padnum; loop++)
                    recs[ctr].input[loop] = padval;
                reclen[ctr] += padnum;
                recs[ctr].length += padnum;
            }

            if (!sending) {
                if (reclen[ctr] == 0 || reclen[ctr] % bs != 0) {
                    /* Publicly invalid */
                    return 0;
                }
            }
        }
        if (n_recs > 1) {
            unsigned char *data[SSL_MAX_PIPELINES];

            /* Set the output buffers */
            for (ctr = 0; ctr < n_recs; ctr++) {
                data[ctr] = recs[ctr].data;
            }
            if (EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_SET_PIPELINE_OUTPUT_BUFS,
                                    (int)n_recs, data) <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_PIPELINE_FAILURE);
                return 0;
            }
            /* Set the input buffers */
            for (ctr = 0; ctr < n_recs; ctr++) {
                data[ctr] = recs[ctr].input;
            }
            if (EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_SET_PIPELINE_INPUT_BUFS,
                                    (int)n_recs, data) <= 0
                || EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_SET_PIPELINE_INPUT_LENS,
                                       (int)n_recs, reclen) <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_PIPELINE_FAILURE);
                return 0;
            }
        }

        if (!SSL_IS_DTLS(s) && tlstree_enc) {
            unsigned char *seq;
            int decrement_seq = 0;

            /*
             * When sending, seq is incremented after MAC calculation.
             * So if we are in ETM mode, we use seq 'as is' in the ctrl-function.
             * Otherwise we have to decrease it in the implementation
             */
            if (sending && !SSL_WRITE_ETM(s))
                decrement_seq = 1;

            seq = sending ? RECORD_LAYER_get_write_sequence(&s->rlayer)
                          : RECORD_LAYER_get_read_sequence(&s->rlayer);
            if (EVP_CIPHER_CTX_ctrl(ds, EVP_CTRL_TLSTREE, decrement_seq, seq) <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }

        if (provided) {
            int outlen;

            /* Provided cipher - we do not support pipelining on this path */
            if (n_recs > 1)  {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }

            if (!EVP_CipherUpdate(ds, recs[0].data, &outlen, recs[0].input,
                                  (unsigned int)reclen[0]))
                return 0;
            recs[0].length = outlen;

            /*
             * The length returned from EVP_CipherUpdate above is the actual
             * payload length. We need to adjust the data/input ptr to skip over
             * any explicit IV
             */
            if (!sending) {
                if (EVP_CIPHER_get_mode(enc) == EVP_CIPH_GCM_MODE) {
                        recs[0].data += EVP_GCM_TLS_EXPLICIT_IV_LEN;
                        recs[0].input += EVP_GCM_TLS_EXPLICIT_IV_LEN;
                } else if (EVP_CIPHER_get_mode(enc) == EVP_CIPH_CCM_MODE) {
                        recs[0].data += EVP_CCM_TLS_EXPLICIT_IV_LEN;
                        recs[0].input += EVP_CCM_TLS_EXPLICIT_IV_LEN;
                } else if (bs != 1 && SSL_USE_EXPLICIT_IV(s)) {
                    recs[0].data += bs;
                    recs[0].input += bs;
                    recs[0].orig_len -= bs;
                }

                /* Now get a pointer to the MAC (if applicable) */
                if (macs != NULL) {
                    OSSL_PARAM params[2], *p = params;

                    /* Get the MAC */
                    macs[0].alloced = 0;

                    *p++ = OSSL_PARAM_construct_octet_ptr(OSSL_CIPHER_PARAM_TLS_MAC,
                                                          (void **)&macs[0].mac,
                                                          macsize);
                    *p = OSSL_PARAM_construct_end();

                    if (!EVP_CIPHER_CTX_get_params(ds, params)) {
                        /* Shouldn't normally happen */
                        SSLfatal(s, SSL_AD_INTERNAL_ERROR,
                                 ERR_R_INTERNAL_ERROR);
                        return 0;
                    }
                }
            }
        } else {
            /* Legacy cipher */

            tmpr = EVP_Cipher(ds, recs[0].data, recs[0].input,
                              (unsigned int)reclen[0]);
            if ((EVP_CIPHER_get_flags(EVP_CIPHER_CTX_get0_cipher(ds))
                 & EVP_CIPH_FLAG_CUSTOM_CIPHER) != 0
                ? (tmpr < 0)
                : (tmpr == 0)) {
                /* AEAD can fail to verify MAC */
                return 0;
            }

            if (!sending) {
                for (ctr = 0; ctr < n_recs; ctr++) {
                    /* Adjust the record to remove the explicit IV/MAC/Tag */
                    if (EVP_CIPHER_get_mode(enc) == EVP_CIPH_GCM_MODE) {
                        recs[ctr].data += EVP_GCM_TLS_EXPLICIT_IV_LEN;
                        recs[ctr].input += EVP_GCM_TLS_EXPLICIT_IV_LEN;
                        recs[ctr].length -= EVP_GCM_TLS_EXPLICIT_IV_LEN;
                    } else if (EVP_CIPHER_get_mode(enc) == EVP_CIPH_CCM_MODE) {
                        recs[ctr].data += EVP_CCM_TLS_EXPLICIT_IV_LEN;
                        recs[ctr].input += EVP_CCM_TLS_EXPLICIT_IV_LEN;
                        recs[ctr].length -= EVP_CCM_TLS_EXPLICIT_IV_LEN;
                    } else if (bs != 1 && SSL_USE_EXPLICIT_IV(s)) {
                        if (recs[ctr].length < bs)
                            return 0;
                        recs[ctr].data += bs;
                        recs[ctr].input += bs;
                        recs[ctr].length -= bs;
                        recs[ctr].orig_len -= bs;
                    }

                    /*
                     * If using Mac-then-encrypt, then this will succeed but
                     * with a random MAC if padding is invalid
                     */
                    if (!tls1_cbc_remove_padding_and_mac(&recs[ctr].length,
                                         recs[ctr].orig_len,
                                         recs[ctr].data,
                                         (macs != NULL) ? &macs[ctr].mac : NULL,
                                         (macs != NULL) ? &macs[ctr].alloced
                                                        : NULL,
                                         bs,
                                         pad ? (size_t)pad : macsize,
                                         (EVP_CIPHER_get_flags(enc)
                                         & EVP_CIPH_FLAG_AEAD_CIPHER) != 0,
                                         s->ctx->libctx))
                        return 0;
                }
            }
        }
    }
    return 1;
}

/*
 * ssl3_cbc_record_digest_supported returns 1 iff |ctx| uses a hash function
 * which ssl3_cbc_digest_record supports.
 */
char ssl3_cbc_record_digest_supported(const EVP_MD_CTX *ctx)
{
    switch (EVP_MD_CTX_get_type(ctx)) {
    case NID_md5:
    case NID_sha1:
    case NID_sha224:
    case NID_sha256:
    case NID_sha384:
    case NID_sha512:
        return 1;
    default:
        return 0;
    }
}

int n_ssl3_mac(SSL *ssl, SSL3_RECORD *rec, unsigned char *md, int sending)
{
    unsigned char *mac_sec, *seq;
    const EVP_MD_CTX *hash;
    unsigned char *p, rec_char;
    size_t md_size;
    size_t npad;
    int t;

    if (sending) {
        mac_sec = &(ssl->s3.write_mac_secret[0]);
        seq = RECORD_LAYER_get_write_sequence(&ssl->rlayer);
        hash = ssl->write_hash;
    } else {
        mac_sec = &(ssl->s3.read_mac_secret[0]);
        seq = RECORD_LAYER_get_read_sequence(&ssl->rlayer);
        hash = ssl->read_hash;
    }

    t = EVP_MD_CTX_get_size(hash);
    if (t < 0)
        return 0;
    md_size = t;
    npad = (48 / md_size) * md_size;

    if (!sending
        && EVP_CIPHER_CTX_get_mode(ssl->enc_read_ctx) == EVP_CIPH_CBC_MODE
        && ssl3_cbc_record_digest_supported(hash)) {
#ifdef OPENSSL_NO_DEPRECATED_3_0
        return 0;
#else
        /*
         * This is a CBC-encrypted record. We must avoid leaking any
         * timing-side channel information about how many blocks of data we
         * are hashing because that gives an attacker a timing-oracle.
         */

        /*-
         * npad is, at most, 48 bytes and that's with MD5:
         *   16 + 48 + 8 (sequence bytes) + 1 + 2 = 75.
         *
         * With SHA-1 (the largest hash speced for SSLv3) the hash size
         * goes up 4, but npad goes down by 8, resulting in a smaller
         * total size.
         */
        unsigned char header[75];
        size_t j = 0;
        memcpy(header + j, mac_sec, md_size);
        j += md_size;
        memcpy(header + j, ssl3_pad_1, npad);
        j += npad;
        memcpy(header + j, seq, 8);
        j += 8;
        header[j++] = rec->type;
        header[j++] = (unsigned char)(rec->length >> 8);
        header[j++] = (unsigned char)(rec->length & 0xff);

        /* Final param == is SSLv3 */
        if (ssl3_cbc_digest_record(EVP_MD_CTX_get0_md(hash),
                                   md, &md_size,
                                   header, rec->input,
                                   rec->length, rec->orig_len,
                                   mac_sec, md_size, 1) <= 0)
            return 0;
#endif
    } else {
        unsigned int md_size_u;
        /* Chop the digest off the end :-) */
        EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();

        if (md_ctx == NULL)
            return 0;

        rec_char = rec->type;
        p = md;
        s2n(rec->length, p);
        if (EVP_MD_CTX_copy_ex(md_ctx, hash) <= 0
            || EVP_DigestUpdate(md_ctx, mac_sec, md_size) <= 0
            || EVP_DigestUpdate(md_ctx, ssl3_pad_1, npad) <= 0
            || EVP_DigestUpdate(md_ctx, seq, 8) <= 0
            || EVP_DigestUpdate(md_ctx, &rec_char, 1) <= 0
            || EVP_DigestUpdate(md_ctx, md, 2) <= 0
            || EVP_DigestUpdate(md_ctx, rec->input, rec->length) <= 0
            || EVP_DigestFinal_ex(md_ctx, md, NULL) <= 0
            || EVP_MD_CTX_copy_ex(md_ctx, hash) <= 0
            || EVP_DigestUpdate(md_ctx, mac_sec, md_size) <= 0
            || EVP_DigestUpdate(md_ctx, ssl3_pad_2, npad) <= 0
            || EVP_DigestUpdate(md_ctx, md, md_size) <= 0
            || EVP_DigestFinal_ex(md_ctx, md, &md_size_u) <= 0) {
            EVP_MD_CTX_free(md_ctx);
            return 0;
        }

        EVP_MD_CTX_free(md_ctx);
    }

    ssl3_record_sequence_update(seq);
    return 1;
}

int tls1_mac(SSL *ssl, SSL3_RECORD *rec, unsigned char *md, int sending)
{
    unsigned char *seq;
    EVP_MD_CTX *hash;
    size_t md_size;
    int i;
    EVP_MD_CTX *hmac = NULL, *mac_ctx;
    unsigned char header[13];
    int stream_mac = sending ? (ssl->mac_flags & SSL_MAC_FLAG_WRITE_MAC_STREAM)
                             : (ssl->mac_flags & SSL_MAC_FLAG_READ_MAC_STREAM);
    int tlstree_mac = sending ? (ssl->mac_flags & SSL_MAC_FLAG_WRITE_MAC_TLSTREE)
                              : (ssl->mac_flags & SSL_MAC_FLAG_READ_MAC_TLSTREE);
    int t;

    if (sending) {
        seq = RECORD_LAYER_get_write_sequence(&ssl->rlayer);
        hash = ssl->write_hash;
    } else {
        seq = RECORD_LAYER_get_read_sequence(&ssl->rlayer);
        hash = ssl->read_hash;
    }

    t = EVP_MD_CTX_get_size(hash);
    if (!ossl_assert(t >= 0))
        return 0;
    md_size = t;

    /* I should fix this up TLS TLS TLS TLS TLS XXXXXXXX */
    if (stream_mac) {
        mac_ctx = hash;
    } else {
        hmac = EVP_MD_CTX_new();
        if (hmac == NULL || !EVP_MD_CTX_copy(hmac, hash)) {
            EVP_MD_CTX_free(hmac);
            return 0;
        }
        mac_ctx = hmac;
    }

    if (!SSL_IS_DTLS(ssl) && tlstree_mac && EVP_MD_CTX_ctrl(mac_ctx, EVP_MD_CTRL_TLSTREE, 0, seq) <= 0) {
        EVP_MD_CTX_free(hmac);
        return 0;
    }

    if (SSL_IS_DTLS(ssl)) {
        unsigned char dtlsseq[8], *p = dtlsseq;

        s2n(sending ? DTLS_RECORD_LAYER_get_w_epoch(&ssl->rlayer) :
            DTLS_RECORD_LAYER_get_r_epoch(&ssl->rlayer), p);
        memcpy(p, &seq[2], 6);

        memcpy(header, dtlsseq, 8);
    } else
        memcpy(header, seq, 8);

    header[8] = rec->type;
    header[9] = (unsigned char)(ssl->version >> 8);
    header[10] = (unsigned char)(ssl->version);
    header[11] = (unsigned char)(rec->length >> 8);
    header[12] = (unsigned char)(rec->length & 0xff);

    if (!sending && !SSL_READ_ETM(ssl)
        && EVP_CIPHER_CTX_get_mode(ssl->enc_read_ctx) == EVP_CIPH_CBC_MODE
        && ssl3_cbc_record_digest_supported(mac_ctx)) {
        OSSL_PARAM tls_hmac_params[2], *p = tls_hmac_params;

        *p++ = OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_TLS_DATA_SIZE,
                                           &rec->orig_len);
        *p++ = OSSL_PARAM_construct_end();

        if (!EVP_PKEY_CTX_set_params(EVP_MD_CTX_get_pkey_ctx(mac_ctx),
                                     tls_hmac_params))
            return 0;
    }

    if (EVP_DigestSignUpdate(mac_ctx, header, sizeof(header)) <= 0
        || EVP_DigestSignUpdate(mac_ctx, rec->input, rec->length) <= 0
        || EVP_DigestSignFinal(mac_ctx, md, &md_size) <= 0) {
        EVP_MD_CTX_free(hmac);
        return 0;
    }

    EVP_MD_CTX_free(hmac);

    OSSL_TRACE_BEGIN(TLS) {
        BIO_printf(trc_out, "seq:\n");
        BIO_dump_indent(trc_out, seq, 8, 4);
        BIO_printf(trc_out, "rec:\n");
        BIO_dump_indent(trc_out, rec->data, rec->length, 4);
    } OSSL_TRACE_END(TLS);

    if (!SSL_IS_DTLS(ssl)) {
        for (i = 7; i >= 0; i--) {
            ++seq[i];
            if (seq[i] != 0)
                break;
        }
    }
    OSSL_TRACE_BEGIN(TLS) {
        BIO_printf(trc_out, "md:\n");
        BIO_dump_indent(trc_out, md, md_size, 4);
    } OSSL_TRACE_END(TLS);
    return 1;
}

int dtls1_process_record(SSL *s, DTLS1_BITMAP *bitmap)
{
    int i;
    int enc_err;
    SSL_SESSION *sess;
    SSL3_RECORD *rr;
    int imac_size;
    size_t mac_size = 0;
    unsigned char md[EVP_MAX_MD_SIZE];
    size_t max_plain_length = SSL3_RT_MAX_PLAIN_LENGTH;
    SSL_MAC_BUF macbuf = { NULL, 0 };
    int ret = 0;

    rr = RECORD_LAYER_get_rrec(&s->rlayer);
    sess = s->session;

    /*
     * At this point, s->rlayer.packet_length == SSL3_RT_HEADER_LNGTH + rr->length,
     * and we have that many bytes in s->rlayer.packet
     */
    rr->input = &(RECORD_LAYER_get_packet(&s->rlayer)[DTLS1_RT_HEADER_LENGTH]);

    /*
     * ok, we can now read from 's->rlayer.packet' data into 'rr'. rr->input
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
        SSLfatal(s, SSL_AD_RECORD_OVERFLOW, SSL_R_ENCRYPTED_LENGTH_TOO_LONG);
        return 0;
    }

    /* decrypt in place in 'rr->input' */
    rr->data = rr->input;
    rr->orig_len = rr->length;

    if (s->read_hash != NULL) {
        const EVP_MD *tmpmd = EVP_MD_CTX_get0_md(s->read_hash);

        if (tmpmd != NULL) {
            imac_size = EVP_MD_get_size(tmpmd);
            if (!ossl_assert(imac_size >= 0 && imac_size <= EVP_MAX_MD_SIZE)) {
                    SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
                    return -1;
            }
            mac_size = (size_t)imac_size;
        }
    }

    if (SSL_READ_ETM(s) && s->read_hash) {
        unsigned char *mac;

        if (rr->orig_len < mac_size) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_LENGTH_TOO_SHORT);
            return 0;
        }
        rr->length -= mac_size;
        mac = rr->data + rr->length;
        i = s->method->ssl3_enc->mac(s, rr, md, 0 /* not send */ );
        if (i == 0 || CRYPTO_memcmp(md, mac, (size_t)mac_size) != 0) {
            SSLfatal(s, SSL_AD_BAD_RECORD_MAC,
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
    enc_err = s->method->ssl3_enc->enc(s, rr, 1, 0, &macbuf, mac_size);

    /*-
     * enc_err is:
     *    0: if the record is publicly invalid, or an internal error, or AEAD
     *       decryption failed, or ETM decryption failed.
     *    1: Success or MTE decryption failed (MAC will be randomised)
     */
    if (enc_err == 0) {
        ERR_pop_to_mark();
        if (ossl_statem_in_error(s)) {
            /* SSLfatal() got called */
            goto end;
        }
        /* For DTLS we simply ignore bad packets. */
        rr->length = 0;
        RECORD_LAYER_reset_packet_length(&s->rlayer);
        goto end;
    }
    ERR_clear_last_mark();
    OSSL_TRACE_BEGIN(TLS) {
        BIO_printf(trc_out, "dec %zd\n", rr->length);
        BIO_dump_indent(trc_out, rr->data, rr->length, 4);
    } OSSL_TRACE_END(TLS);

    /* r->length is now the compressed data plus mac */
    if ((sess != NULL)
            && !SSL_READ_ETM(s)
            && (s->enc_read_ctx != NULL)
            && (EVP_MD_CTX_get0_md(s->read_hash) != NULL)) {
        /* s->read_hash != NULL => mac_size != -1 */

        i = s->method->ssl3_enc->mac(s, rr, md, 0 /* not send */ );
        if (i == 0 || macbuf.mac == NULL
            || CRYPTO_memcmp(md, macbuf.mac, mac_size) != 0)
            enc_err = 0;
        if (rr->length > SSL3_RT_MAX_COMPRESSED_LENGTH + mac_size)
            enc_err = 0;
    }

    if (enc_err == 0) {
        /* decryption failed, silently discard message */
        rr->length = 0;
        RECORD_LAYER_reset_packet_length(&s->rlayer);
        goto end;
    }

    /* r->length is now just compressed */
    if (s->expand != NULL) {
        if (rr->length > SSL3_RT_MAX_COMPRESSED_LENGTH) {
            SSLfatal(s, SSL_AD_RECORD_OVERFLOW,
                     SSL_R_COMPRESSED_LENGTH_TOO_LONG);
            goto end;
        }
        if (!ssl3_do_uncompress(s, rr)) {
            SSLfatal(s, SSL_AD_DECOMPRESSION_FAILURE, SSL_R_BAD_DECOMPRESSION);
            goto end;
        }
    }

    /* use current Max Fragment Length setting if applicable */
    if (s->session != NULL && USE_MAX_FRAGMENT_LENGTH_EXT(s->session))
        max_plain_length = GET_MAX_FRAGMENT_LENGTH(s->session);

    /* send overflow if the plaintext is too long now it has passed MAC */
    if (rr->length > max_plain_length) {
        SSLfatal(s, SSL_AD_RECORD_OVERFLOW, SSL_R_DATA_LENGTH_TOO_LONG);
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
    RECORD_LAYER_reset_packet_length(&s->rlayer);

    /* Mark receipt of record. */
    dtls1_record_bitmap_update(s, bitmap);

    ret = 1;
 end:
    if (macbuf.alloced)
        OPENSSL_free(macbuf.mac);
    return ret;
}

/*
 * Retrieve a buffered record that belongs to the current epoch, i.e. processed
 */
#define dtls1_get_processed_record(s) \
                   dtls1_retrieve_buffered_record((s), \
                   &(DTLS_RECORD_LAYER_get_processed_rcds(&s->rlayer)))

/*-
 * Call this to get a new input record.
 * It will return <= 0 if more data is needed, normally due to an error
 * or non-blocking IO.
 * When it finishes, one packet has been decoded and can be found in
 * ssl->s3.rrec.type    - is the type of record
 * ssl->s3.rrec.data    - data
 * ssl->s3.rrec.length  - number of bytes
 */
/* used only by dtls1_read_bytes */
int dtls1_get_record(SSL *s)
{
    int ssl_major, ssl_minor;
    int rret;
    size_t more, n;
    SSL3_RECORD *rr;
    unsigned char *p = NULL;
    unsigned short version;
    DTLS1_BITMAP *bitmap;
    unsigned int is_next_epoch;

    rr = RECORD_LAYER_get_rrec(&s->rlayer);

 again:
    /*
     * The epoch may have changed.  If so, process all the pending records.
     * This is a non-blocking operation.
     */
    if (!dtls1_process_buffered_records(s)) {
        /* SSLfatal() already called */
        return -1;
    }

    /* if we're renegotiating, then there may be buffered records */
    if (dtls1_get_processed_record(s))
        return 1;

    /* get something from the wire */

    /* check if we have the header */
    if ((RECORD_LAYER_get_rstate(&s->rlayer) != SSL_ST_READ_BODY) ||
        (RECORD_LAYER_get_packet_length(&s->rlayer) < DTLS1_RT_HEADER_LENGTH)) {
        rret = ssl3_read_n(s, DTLS1_RT_HEADER_LENGTH,
                           SSL3_BUFFER_get_len(&s->rlayer.rbuf), 0, 1, &n);
        /* read timeout is handled by dtls1_read_bytes */
        if (rret <= 0) {
            /* SSLfatal() already called if appropriate */
            return rret;         /* error or non-blocking */
        }

        /* this packet contained a partial record, dump it */
        if (RECORD_LAYER_get_packet_length(&s->rlayer) !=
            DTLS1_RT_HEADER_LENGTH) {
            RECORD_LAYER_reset_packet_length(&s->rlayer);
            goto again;
        }

        RECORD_LAYER_set_rstate(&s->rlayer, SSL_ST_READ_BODY);

        p = RECORD_LAYER_get_packet(&s->rlayer);

        if (s->msg_callback)
            s->msg_callback(0, 0, SSL3_RT_HEADER, p, DTLS1_RT_HEADER_LENGTH,
                            s, s->msg_callback_arg);

        /* Pull apart the header into the DTLS1_RECORD */
        rr->type = *(p++);
        ssl_major = *(p++);
        ssl_minor = *(p++);
        version = (ssl_major << 8) | ssl_minor;

        /* sequence number is 64 bits, with top 2 bytes = epoch */
        n2s(p, rr->epoch);

        memcpy(&(RECORD_LAYER_get_read_sequence(&s->rlayer)[2]), p, 6);
        p += 6;

        n2s(p, rr->length);
        rr->read = 0;

        /*
         * Lets check the version. We tolerate alerts that don't have the exact
         * version number (e.g. because of protocol version errors)
         */
        if (!s->first_packet && rr->type != SSL3_RT_ALERT) {
            if (version != s->version) {
                /* unexpected version, silently discard */
                rr->length = 0;
                rr->read = 1;
                RECORD_LAYER_reset_packet_length(&s->rlayer);
                goto again;
            }
        }

        if ((version & 0xff00) != (s->version & 0xff00)) {
            /* wrong version, silently discard record */
            rr->length = 0;
            rr->read = 1;
            RECORD_LAYER_reset_packet_length(&s->rlayer);
            goto again;
        }

        if (rr->length > SSL3_RT_MAX_ENCRYPTED_LENGTH) {
            /* record too long, silently discard it */
            rr->length = 0;
            rr->read = 1;
            RECORD_LAYER_reset_packet_length(&s->rlayer);
            goto again;
        }

        /* If received packet overflows own-client Max Fragment Length setting */
        if (s->session != NULL && USE_MAX_FRAGMENT_LENGTH_EXT(s->session)
                && rr->length > GET_MAX_FRAGMENT_LENGTH(s->session) + SSL3_RT_MAX_ENCRYPTED_OVERHEAD) {
            /* record too long, silently discard it */
            rr->length = 0;
            rr->read = 1;
            RECORD_LAYER_reset_packet_length(&s->rlayer);
            goto again;
        }

        /* now s->rlayer.rstate == SSL_ST_READ_BODY */
    }

    /* s->rlayer.rstate == SSL_ST_READ_BODY, get and decode the data */

    if (rr->length >
        RECORD_LAYER_get_packet_length(&s->rlayer) - DTLS1_RT_HEADER_LENGTH) {
        /* now s->rlayer.packet_length == DTLS1_RT_HEADER_LENGTH */
        more = rr->length;
        rret = ssl3_read_n(s, more, more, 1, 1, &n);
        /* this packet contained a partial record, dump it */
        if (rret <= 0 || n != more) {
            if (ossl_statem_in_error(s)) {
                /* ssl3_read_n() called SSLfatal() */
                return -1;
            }
            rr->length = 0;
            rr->read = 1;
            RECORD_LAYER_reset_packet_length(&s->rlayer);
            goto again;
        }

        /*
         * now n == rr->length, and s->rlayer.packet_length ==
         * DTLS1_RT_HEADER_LENGTH + rr->length
         */
    }
    /* set state for later operations */
    RECORD_LAYER_set_rstate(&s->rlayer, SSL_ST_READ_HEADER);

    /* match epochs.  NULL means the packet is dropped on the floor */
    bitmap = dtls1_get_bitmap(s, rr, &is_next_epoch);
    if (bitmap == NULL) {
        rr->length = 0;
        RECORD_LAYER_reset_packet_length(&s->rlayer); /* dump this record */
        goto again;             /* get another record */
    }
#ifndef OPENSSL_NO_SCTP
    /* Only do replay check if no SCTP bio */
    if (!BIO_dgram_is_sctp(SSL_get_rbio(s))) {
#endif
        /* Check whether this is a repeat, or aged record. */
        if (!dtls1_record_replay_check(s, bitmap)) {
            rr->length = 0;
            rr->read = 1;
            RECORD_LAYER_reset_packet_length(&s->rlayer); /* dump this record */
            goto again;         /* get another record */
        }
#ifndef OPENSSL_NO_SCTP
    }
#endif

    /* just read a 0 length packet */
    if (rr->length == 0) {
        rr->read = 1;
        goto again;
    }

    /*
     * If this record is from the next epoch (either HM or ALERT), and a
     * handshake is currently in progress, buffer it since it cannot be
     * processed at this time.
     */
    if (is_next_epoch) {
        if ((SSL_in_init(s) || ossl_statem_get_in_handshake(s))) {
            if (dtls1_buffer_record (s,
                    &(DTLS_RECORD_LAYER_get_unprocessed_rcds(&s->rlayer)),
                    rr->seq_num) < 0) {
                /* SSLfatal() already called */
                return -1;
            }
        }
        rr->length = 0;
        rr->read = 1;
        RECORD_LAYER_reset_packet_length(&s->rlayer);
        goto again;
    }

    if (!dtls1_process_record(s, bitmap)) {
        if (ossl_statem_in_error(s)) {
            /* dtls1_process_record() called SSLfatal */
            return -1;
        }
        rr->length = 0;
        rr->read = 1;
        RECORD_LAYER_reset_packet_length(&s->rlayer); /* dump this record */
        goto again;             /* get another record */
    }

    return 1;

}

int dtls_buffer_listen_record(SSL *s, size_t len, unsigned char *seq, size_t off)
{
    SSL3_RECORD *rr;

    rr = RECORD_LAYER_get_rrec(&s->rlayer);
    memset(rr, 0, sizeof(SSL3_RECORD));

    rr->length = len;
    rr->type = SSL3_RT_HANDSHAKE;
    memcpy(rr->seq_num, seq, sizeof(rr->seq_num));
    rr->off = off;

    s->rlayer.packet = RECORD_LAYER_get_rbuf(&s->rlayer)->buf;
    s->rlayer.packet_length = DTLS1_RT_HEADER_LENGTH + len;
    rr->data = s->rlayer.packet + DTLS1_RT_HEADER_LENGTH;

    if (dtls1_buffer_record(s, &(s->rlayer.d->processed_rcds),
                            SSL3_RECORD_get_seq_num(s->rlayer.rrec)) <= 0) {
        /* SSLfatal() already called */
        return 0;
    }

    return 1;
}
