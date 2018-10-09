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
 * ECC cipher suite support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "../ssl_locl.h"
#include "statem_locl.h"
#include <openssl/buffer.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

/*
 * send s->init_buf in records of type 'type' (SSL3_RT_HANDSHAKE or
 * SSL3_RT_CHANGE_CIPHER_SPEC)
 */
int ssl3_do_write(SSL *s, int type)
{
    int ret;

    ret = ssl3_write_bytes(s, type, &s->init_buf->data[s->init_off],
                           s->init_num);
    if (ret < 0)
        return (-1);
    if (type == SSL3_RT_HANDSHAKE)
        /*
         * should not be done for 'Hello Request's, but in that case we'll
         * ignore the result anyway
         */
        if (!ssl3_finish_mac(s,
                             (unsigned char *)&s->init_buf->data[s->init_off],
                             ret))
            return -1;

    if (ret == s->init_num) {
        if (s->msg_callback)
            s->msg_callback(1, s->version, type, s->init_buf->data,
                            (size_t)(s->init_off + s->init_num), s,
                            s->msg_callback_arg);
        return (1);
    }
    s->init_off += ret;
    s->init_num -= ret;
    return (0);
}

int tls_construct_finished(SSL *s, const char *sender, int slen)
{
    unsigned char *p;
    int i;
    unsigned long l;

    p = ssl_handshake_start(s);

    i = s->method->ssl3_enc->final_finish_mac(s,
                                              sender, slen,
                                              s->s3->tmp.finish_md);
    if (i <= 0)
        return 0;
    s->s3->tmp.finish_md_len = i;
    memcpy(p, s->s3->tmp.finish_md, i);
    l = i;

    /*
     * Copy the finished so we can use it for renegotiation checks
     */
    if (!s->server) {
        OPENSSL_assert(i <= EVP_MAX_MD_SIZE);
        memcpy(s->s3->previous_client_finished, s->s3->tmp.finish_md, i);
        s->s3->previous_client_finished_len = i;
    } else {
        OPENSSL_assert(i <= EVP_MAX_MD_SIZE);
        memcpy(s->s3->previous_server_finished, s->s3->tmp.finish_md, i);
        s->s3->previous_server_finished_len = i;
    }

    if (!ssl_set_handshake_header(s, SSL3_MT_FINISHED, l)) {
        SSLerr(SSL_F_TLS_CONSTRUCT_FINISHED, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    return 1;
}

/*
 * ssl3_take_mac calculates the Finished MAC for the handshakes messages seen
 * to far.
 */
int ssl3_take_mac(SSL *s)
{
    const char *sender;
    int slen;

    if (!s->server) {
        sender = s->method->ssl3_enc->server_finished_label;
        slen = s->method->ssl3_enc->server_finished_label_len;
    } else {
        sender = s->method->ssl3_enc->client_finished_label;
        slen = s->method->ssl3_enc->client_finished_label_len;
    }

    s->s3->tmp.peer_finish_md_len =
        s->method->ssl3_enc->final_finish_mac(s, sender, slen,
                                              s->s3->tmp.peer_finish_md);

    if (s->s3->tmp.peer_finish_md_len == 0) {
        SSLerr(SSL_F_SSL3_TAKE_MAC, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    return 1;
}

MSG_PROCESS_RETURN tls_process_change_cipher_spec(SSL *s, PACKET *pkt)
{
    int al;
    long remain;

    remain = PACKET_remaining(pkt);
    /*
     * 'Change Cipher Spec' is just a single byte, which should already have
     * been consumed by ssl_get_message() so there should be no bytes left,
     * unless we're using DTLS1_BAD_VER, which has an extra 2 bytes
     */
    if (SSL_IS_DTLS(s)) {
        if ((s->version == DTLS1_BAD_VER
             && remain != DTLS1_CCS_HEADER_LENGTH + 1)
            || (s->version != DTLS1_BAD_VER
                && remain != DTLS1_CCS_HEADER_LENGTH - 1)) {
            al = SSL_AD_ILLEGAL_PARAMETER;
            SSLerr(SSL_F_TLS_PROCESS_CHANGE_CIPHER_SPEC,
                   SSL_R_BAD_CHANGE_CIPHER_SPEC);
            goto f_err;
        }
    } else {
        if (remain != 0) {
            al = SSL_AD_ILLEGAL_PARAMETER;
            SSLerr(SSL_F_TLS_PROCESS_CHANGE_CIPHER_SPEC,
                   SSL_R_BAD_CHANGE_CIPHER_SPEC);
            goto f_err;
        }
    }

    /* Check we have a cipher to change to */
    if (s->s3->tmp.new_cipher == NULL) {
        al = SSL_AD_UNEXPECTED_MESSAGE;
        SSLerr(SSL_F_TLS_PROCESS_CHANGE_CIPHER_SPEC, SSL_R_CCS_RECEIVED_EARLY);
        goto f_err;
    }

    s->s3->change_cipher_spec = 1;
    if (!ssl3_do_change_cipher_spec(s)) {
        al = SSL_AD_INTERNAL_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_CHANGE_CIPHER_SPEC, ERR_R_INTERNAL_ERROR);
        goto f_err;
    }

    if (SSL_IS_DTLS(s)) {
        dtls1_reset_seq_numbers(s, SSL3_CC_READ);

        if (s->version == DTLS1_BAD_VER)
            s->d1->handshake_read_seq++;

#ifndef OPENSSL_NO_SCTP
        /*
         * Remember that a CCS has been received, so that an old key of
         * SCTP-Auth can be deleted when a CCS is sent. Will be ignored if no
         * SCTP is used
         */
        BIO_ctrl(SSL_get_wbio(s), BIO_CTRL_DGRAM_SCTP_AUTH_CCS_RCVD, 1, NULL);
#endif
    }

    return MSG_PROCESS_CONTINUE_READING;
 f_err:
    ssl3_send_alert(s, SSL3_AL_FATAL, al);
    ossl_statem_set_error(s);
    return MSG_PROCESS_ERROR;
}

MSG_PROCESS_RETURN tls_process_finished(SSL *s, PACKET *pkt)
{
    int al, i;

    /* If this occurs, we have missed a message */
    if (!s->s3->change_cipher_spec) {
        al = SSL_AD_UNEXPECTED_MESSAGE;
        SSLerr(SSL_F_TLS_PROCESS_FINISHED, SSL_R_GOT_A_FIN_BEFORE_A_CCS);
        goto f_err;
    }
    s->s3->change_cipher_spec = 0;

    i = s->s3->tmp.peer_finish_md_len;

    if ((unsigned long)i != PACKET_remaining(pkt)) {
        al = SSL_AD_DECODE_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_FINISHED, SSL_R_BAD_DIGEST_LENGTH);
        goto f_err;
    }

    if (CRYPTO_memcmp(PACKET_data(pkt), s->s3->tmp.peer_finish_md, i) != 0) {
        al = SSL_AD_DECRYPT_ERROR;
        SSLerr(SSL_F_TLS_PROCESS_FINISHED, SSL_R_DIGEST_CHECK_FAILED);
        goto f_err;
    }

    /*
     * Copy the finished so we can use it for renegotiation checks
     */
    if (s->server) {
        OPENSSL_assert(i <= EVP_MAX_MD_SIZE);
        memcpy(s->s3->previous_client_finished, s->s3->tmp.peer_finish_md, i);
        s->s3->previous_client_finished_len = i;
    } else {
        OPENSSL_assert(i <= EVP_MAX_MD_SIZE);
        memcpy(s->s3->previous_server_finished, s->s3->tmp.peer_finish_md, i);
        s->s3->previous_server_finished_len = i;
    }

    return MSG_PROCESS_FINISHED_READING;
 f_err:
    ssl3_send_alert(s, SSL3_AL_FATAL, al);
    ossl_statem_set_error(s);
    return MSG_PROCESS_ERROR;
}

int tls_construct_change_cipher_spec(SSL *s)
{
    unsigned char *p;

    p = (unsigned char *)s->init_buf->data;
    *p = SSL3_MT_CCS;
    s->init_num = 1;
    s->init_off = 0;

    return 1;
}

unsigned long ssl3_output_cert_chain(SSL *s, CERT_PKEY *cpk)
{
    unsigned char *p;
    unsigned long l = 3 + SSL_HM_HEADER_LENGTH(s);

    if (!ssl_add_cert_chain(s, cpk, &l))
        return 0;

    l -= 3 + SSL_HM_HEADER_LENGTH(s);
    p = ssl_handshake_start(s);
    l2n3(l, p);
    l += 3;

    if (!ssl_set_handshake_header(s, SSL3_MT_CERTIFICATE, l)) {
        SSLerr(SSL_F_SSL3_OUTPUT_CERT_CHAIN, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    return l + SSL_HM_HEADER_LENGTH(s);
}

WORK_STATE tls_finish_handshake(SSL *s, WORK_STATE wst)
{
    void (*cb) (const SSL *ssl, int type, int val) = NULL;

    /* clean a few things up */
    ssl3_cleanup_key_block(s);

    if (!SSL_IS_DTLS(s)) {
        /*
         * We don't do this in DTLS because we may still need the init_buf
         * in case there are any unexpected retransmits
         */
        BUF_MEM_free(s->init_buf);
        s->init_buf = NULL;
    }

    ssl_free_wbio_buffer(s);

    s->init_num = 0;

    if (!s->server || s->renegotiate == 2) {
        /* skipped if we just sent a HelloRequest */
        s->renegotiate = 0;
        s->new_session = 0;

        if (s->server) {
            ssl_update_cache(s, SSL_SESS_CACHE_SERVER);

            s->ctx->stats.sess_accept_good++;
            s->handshake_func = ossl_statem_accept;

            if (SSL_IS_DTLS(s) && !s->hit) {
                /*
                 * We are finishing after the client. We start the timer going
                 * in case there are any retransmits of our final flight
                 * required.
                 */
                dtls1_start_timer(s);
            }
        } else {
            ssl_update_cache(s, SSL_SESS_CACHE_CLIENT);
            if (s->hit)
                s->ctx->stats.sess_hit++;

            s->handshake_func = ossl_statem_connect;
            s->ctx->stats.sess_connect_good++;

            if (SSL_IS_DTLS(s) && s->hit) {
                /*
                 * We are finishing after the server. We start the timer going
                 * in case there are any retransmits of our final flight
                 * required.
                 */
                dtls1_start_timer(s);
            }
        }

        if (s->info_callback != NULL)
            cb = s->info_callback;
        else if (s->ctx->info_callback != NULL)
            cb = s->ctx->info_callback;

        if (cb != NULL)
            cb(s, SSL_CB_HANDSHAKE_DONE, 1);

        if (SSL_IS_DTLS(s)) {
            /* done with handshaking */
            s->d1->handshake_read_seq = 0;
            s->d1->handshake_write_seq = 0;
            s->d1->next_handshake_write_seq = 0;
            dtls1_clear_received_buffer(s);
        }
    }

    return WORK_FINISHED_STOP;
}

int tls_get_message_header(SSL *s, int *mt)
{
    /* s->init_num < SSL3_HM_HEADER_LENGTH */
    int skip_message, i, recvd_type, al;
    unsigned char *p;
    unsigned long l;

    p = (unsigned char *)s->init_buf->data;

    do {
        while (s->init_num < SSL3_HM_HEADER_LENGTH) {
            i = s->method->ssl_read_bytes(s, SSL3_RT_HANDSHAKE, &recvd_type,
                                          &p[s->init_num],
                                          SSL3_HM_HEADER_LENGTH - s->init_num,
                                          0);
            if (i <= 0) {
                s->rwstate = SSL_READING;
                return 0;
            }
            if (recvd_type == SSL3_RT_CHANGE_CIPHER_SPEC) {
                /*
                 * A ChangeCipherSpec must be a single byte and may not occur
                 * in the middle of a handshake message.
                 */
                if (s->init_num != 0 || i != 1 || p[0] != SSL3_MT_CCS) {
                    al = SSL_AD_UNEXPECTED_MESSAGE;
                    SSLerr(SSL_F_TLS_GET_MESSAGE_HEADER,
                           SSL_R_BAD_CHANGE_CIPHER_SPEC);
                    goto f_err;
                }
                s->s3->tmp.message_type = *mt = SSL3_MT_CHANGE_CIPHER_SPEC;
                s->init_num = i - 1;
                s->init_msg = s->init_buf->data;
                s->s3->tmp.message_size = i;
                return 1;
            } else if (recvd_type != SSL3_RT_HANDSHAKE) {
                al = SSL_AD_UNEXPECTED_MESSAGE;
                SSLerr(SSL_F_TLS_GET_MESSAGE_HEADER, SSL_R_CCS_RECEIVED_EARLY);
                goto f_err;
            }
            s->init_num += i;
        }

        skip_message = 0;
        if (!s->server)
            if (p[0] == SSL3_MT_HELLO_REQUEST)
                /*
                 * The server may always send 'Hello Request' messages --
                 * we are doing a handshake anyway now, so ignore them if
                 * their format is correct. Does not count for 'Finished'
                 * MAC.
                 */
                if (p[1] == 0 && p[2] == 0 && p[3] == 0) {
                    s->init_num = 0;
                    skip_message = 1;

                    if (s->msg_callback)
                        s->msg_callback(0, s->version, SSL3_RT_HANDSHAKE,
                                        p, SSL3_HM_HEADER_LENGTH, s,
                                        s->msg_callback_arg);
                }
    } while (skip_message);
    /* s->init_num == SSL3_HM_HEADER_LENGTH */

    *mt = *p;
    s->s3->tmp.message_type = *(p++);

    if (RECORD_LAYER_is_sslv2_record(&s->rlayer)) {
        /*
         * Only happens with SSLv3+ in an SSLv2 backward compatible
         * ClientHello
         *
         * Total message size is the remaining record bytes to read
         * plus the SSL3_HM_HEADER_LENGTH bytes that we already read
         */
        l = RECORD_LAYER_get_rrec_length(&s->rlayer)
            + SSL3_HM_HEADER_LENGTH;
        s->s3->tmp.message_size = l;

        s->init_msg = s->init_buf->data;
        s->init_num = SSL3_HM_HEADER_LENGTH;
    } else {
        n2l3(p, l);
        /* BUF_MEM_grow takes an 'int' parameter */
        if (l > (INT_MAX - SSL3_HM_HEADER_LENGTH)) {
            al = SSL_AD_ILLEGAL_PARAMETER;
            SSLerr(SSL_F_TLS_GET_MESSAGE_HEADER, SSL_R_EXCESSIVE_MESSAGE_SIZE);
            goto f_err;
        }
        s->s3->tmp.message_size = l;

        s->init_msg = s->init_buf->data + SSL3_HM_HEADER_LENGTH;
        s->init_num = 0;
    }

    return 1;
 f_err:
    ssl3_send_alert(s, SSL3_AL_FATAL, al);
    return 0;
}

int tls_get_message_body(SSL *s, unsigned long *len)
{
    long n;
    unsigned char *p;
    int i;

    if (s->s3->tmp.message_type == SSL3_MT_CHANGE_CIPHER_SPEC) {
        /* We've already read everything in */
        *len = (unsigned long)s->init_num;
        return 1;
    }

    p = s->init_msg;
    n = s->s3->tmp.message_size - s->init_num;
    while (n > 0) {
        i = s->method->ssl_read_bytes(s, SSL3_RT_HANDSHAKE, NULL,
                                      &p[s->init_num], n, 0);
        if (i <= 0) {
            s->rwstate = SSL_READING;
            *len = 0;
            return 0;
        }
        s->init_num += i;
        n -= i;
    }

    /*
     * If receiving Finished, record MAC of prior handshake messages for
     * Finished verification.
     */
    if (*(s->init_buf->data) == SSL3_MT_FINISHED && !ssl3_take_mac(s)) {
        /* SSLfatal() already called */
        *len = 0;
        return 0;
    }

    /* Feed this message into MAC computation. */
    if (RECORD_LAYER_is_sslv2_record(&s->rlayer)) {
        if (!ssl3_finish_mac(s, (unsigned char *)s->init_buf->data,
                             s->init_num)) {
            SSLerr(SSL_F_TLS_GET_MESSAGE_BODY, ERR_R_EVP_LIB);
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
            *len = 0;
            return 0;
        }
        if (s->msg_callback)
            s->msg_callback(0, SSL2_VERSION, 0, s->init_buf->data,
                            (size_t)s->init_num, s, s->msg_callback_arg);
    } else {
        if (!ssl3_finish_mac(s, (unsigned char *)s->init_buf->data,
                             s->init_num + SSL3_HM_HEADER_LENGTH)) {
            SSLerr(SSL_F_TLS_GET_MESSAGE_BODY, ERR_R_EVP_LIB);
            ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
            *len = 0;
            return 0;
        }
        if (s->msg_callback)
            s->msg_callback(0, s->version, SSL3_RT_HANDSHAKE, s->init_buf->data,
                            (size_t)s->init_num + SSL3_HM_HEADER_LENGTH, s,
                            s->msg_callback_arg);
    }

    /*
     * init_num should never be negative...should probably be declared
     * unsigned
     */
    if (s->init_num < 0) {
        SSLerr(SSL_F_TLS_GET_MESSAGE_BODY, ERR_R_INTERNAL_ERROR);
        ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        *len = 0;
        return 0;
    }
    *len = (unsigned long)s->init_num;
    return 1;
}

int ssl_cert_type(const X509 *x, const EVP_PKEY *pk)
{
    if (pk == NULL && (pk = X509_get0_pubkey(x)) == NULL)
        return -1;

    switch (EVP_PKEY_id(pk)) {
    default:
        return -1;
    case EVP_PKEY_RSA:
        return SSL_PKEY_RSA_ENC;
    case EVP_PKEY_DSA:
        return SSL_PKEY_DSA_SIGN;
#ifndef OPENSSL_NO_EC
    case EVP_PKEY_EC:
        return SSL_PKEY_ECC;
#endif
#ifndef OPENSSL_NO_GOST
    case NID_id_GostR3410_2001:
        return SSL_PKEY_GOST01;
    case NID_id_GostR3410_2012_256:
        return SSL_PKEY_GOST12_256;
    case NID_id_GostR3410_2012_512:
        return SSL_PKEY_GOST12_512;
#endif
    }
}

int ssl_verify_alarm_type(long type)
{
    int al;

    switch (type) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_GET_CRL:
    case X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER:
        al = SSL_AD_UNKNOWN_CA;
        break;
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
    case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CRL_NOT_YET_VALID:
    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_HOSTNAME_MISMATCH:
    case X509_V_ERR_EMAIL_MISMATCH:
    case X509_V_ERR_IP_ADDRESS_MISMATCH:
    case X509_V_ERR_DANE_NO_MATCH:
    case X509_V_ERR_EE_KEY_TOO_SMALL:
    case X509_V_ERR_CA_KEY_TOO_SMALL:
    case X509_V_ERR_CA_MD_TOO_WEAK:
        al = SSL_AD_BAD_CERTIFICATE;
        break;
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_CRL_SIGNATURE_FAILURE:
        al = SSL_AD_DECRYPT_ERROR;
        break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CRL_HAS_EXPIRED:
        al = SSL_AD_CERTIFICATE_EXPIRED;
        break;
    case X509_V_ERR_CERT_REVOKED:
        al = SSL_AD_CERTIFICATE_REVOKED;
        break;
    case X509_V_ERR_UNSPECIFIED:
    case X509_V_ERR_OUT_OF_MEM:
    case X509_V_ERR_INVALID_CALL:
    case X509_V_ERR_STORE_LOOKUP:
        al = SSL_AD_INTERNAL_ERROR;
        break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_PATH_LENGTH_EXCEEDED:
    case X509_V_ERR_INVALID_CA:
        al = SSL_AD_UNKNOWN_CA;
        break;
    case X509_V_ERR_APPLICATION_VERIFICATION:
        al = SSL_AD_HANDSHAKE_FAILURE;
        break;
    case X509_V_ERR_INVALID_PURPOSE:
        al = SSL_AD_UNSUPPORTED_CERTIFICATE;
        break;
    default:
        al = SSL_AD_CERTIFICATE_UNKNOWN;
        break;
    }
    return (al);
}

int ssl_allow_compression(SSL *s)
{
    if (s->options & SSL_OP_NO_COMPRESSION)
        return 0;
    return ssl_security(s, SSL_SECOP_COMPRESSION, 0, 0, NULL);
}

static int version_cmp(const SSL *s, int a, int b)
{
    int dtls = SSL_IS_DTLS(s);

    if (a == b)
        return 0;
    if (!dtls)
        return a < b ? -1 : 1;
    return DTLS_VERSION_LT(a, b) ? -1 : 1;
}

typedef struct {
    int version;
    const SSL_METHOD *(*cmeth) (void);
    const SSL_METHOD *(*smeth) (void);
} version_info;

#if TLS_MAX_VERSION != TLS1_2_VERSION
# error Code needs update for TLS_method() support beyond TLS1_2_VERSION.
#endif

static const version_info tls_version_table[] = {
#ifndef OPENSSL_NO_TLS1_2
    {TLS1_2_VERSION, tlsv1_2_client_method, tlsv1_2_server_method},
#else
    {TLS1_2_VERSION, NULL, NULL},
#endif
#ifndef OPENSSL_NO_TLS1_1
    {TLS1_1_VERSION, tlsv1_1_client_method, tlsv1_1_server_method},
#else
    {TLS1_1_VERSION, NULL, NULL},
#endif
#ifndef OPENSSL_NO_TLS1
    {TLS1_VERSION, tlsv1_client_method, tlsv1_server_method},
#else
    {TLS1_VERSION, NULL, NULL},
#endif
#ifndef OPENSSL_NO_SSL3
    {SSL3_VERSION, sslv3_client_method, sslv3_server_method},
#else
    {SSL3_VERSION, NULL, NULL},
#endif
    {0, NULL, NULL},
};

#if DTLS_MAX_VERSION != DTLS1_2_VERSION
# error Code needs update for DTLS_method() support beyond DTLS1_2_VERSION.
#endif

static const version_info dtls_version_table[] = {
#ifndef OPENSSL_NO_DTLS1_2
    {DTLS1_2_VERSION, dtlsv1_2_client_method, dtlsv1_2_server_method},
#else
    {DTLS1_2_VERSION, NULL, NULL},
#endif
#ifndef OPENSSL_NO_DTLS1
    {DTLS1_VERSION, dtlsv1_client_method, dtlsv1_server_method},
    {DTLS1_BAD_VER, dtls_bad_ver_client_method, NULL},
#else
    {DTLS1_VERSION, NULL, NULL},
    {DTLS1_BAD_VER, NULL, NULL},
#endif
    {0, NULL, NULL},
};

/*
 * ssl_method_error - Check whether an SSL_METHOD is enabled.
 *
 * @s: The SSL handle for the candidate method
 * @method: the intended method.
 *
 * Returns 0 on success, or an SSL error reason on failure.
 */
static int ssl_method_error(const SSL *s, const SSL_METHOD *method)
{
    int version = method->version;

    if ((s->min_proto_version != 0 &&
         version_cmp(s, version, s->min_proto_version) < 0) ||
        ssl_security(s, SSL_SECOP_VERSION, 0, version, NULL) == 0)
        return SSL_R_VERSION_TOO_LOW;

    if (s->max_proto_version != 0 &&
        version_cmp(s, version, s->max_proto_version) > 0)
        return SSL_R_VERSION_TOO_HIGH;

    if ((s->options & method->mask) != 0)
        return SSL_R_UNSUPPORTED_PROTOCOL;
    if ((method->flags & SSL_METHOD_NO_SUITEB) != 0 && tls1_suiteb(s))
        return SSL_R_AT_LEAST_TLS_1_2_NEEDED_IN_SUITEB_MODE;
    else if ((method->flags & SSL_METHOD_NO_FIPS) != 0 && FIPS_mode())
        return SSL_R_AT_LEAST_TLS_1_0_NEEDED_IN_FIPS_MODE;

    return 0;
}

/*
 * ssl_version_supported - Check that the specified `version` is supported by
 * `SSL *` instance
 *
 * @s: The SSL handle for the candidate method
 * @version: Protocol version to test against
 *
 * Returns 1 when supported, otherwise 0
 */
int ssl_version_supported(const SSL *s, int version)
{
    const version_info *vent;
    const version_info *table;

    switch (s->method->version) {
    default:
        /* Version should match method version for non-ANY method */
        return version_cmp(s, version, s->version) == 0;
    case TLS_ANY_VERSION:
        table = tls_version_table;
        break;
    case DTLS_ANY_VERSION:
        table = dtls_version_table;
        break;
    }

    for (vent = table;
         vent->version != 0 && version_cmp(s, version, vent->version) <= 0;
         ++vent) {
        if (vent->cmeth != NULL &&
            version_cmp(s, version, vent->version) == 0 &&
            ssl_method_error(s, vent->cmeth()) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * ssl_check_version_downgrade - In response to RFC7507 SCSV version
 * fallback indication from a client check whether we're using the highest
 * supported protocol version.
 *
 * @s server SSL handle.
 *
 * Returns 1 when using the highest enabled version, 0 otherwise.
 */
int ssl_check_version_downgrade(SSL *s)
{
    const version_info *vent;
    const version_info *table;

    /*
     * Check that the current protocol is the highest enabled version
     * (according to s->ctx->method, as version negotiation may have changed
     * s->method).
     */
    if (s->version == s->ctx->method->version)
        return 1;

    /*
     * Apparently we're using a version-flexible SSL_METHOD (not at its
     * highest protocol version).
     */
    if (s->ctx->method->version == TLS_method()->version)
        table = tls_version_table;
    else if (s->ctx->method->version == DTLS_method()->version)
        table = dtls_version_table;
    else {
        /* Unexpected state; fail closed. */
        return 0;
    }

    for (vent = table; vent->version != 0; ++vent) {
        if (vent->smeth != NULL && ssl_method_error(s, vent->smeth()) == 0)
            return s->version == vent->version;
    }
    return 0;
}

/*
 * ssl_set_version_bound - set an upper or lower bound on the supported (D)TLS
 * protocols, provided the initial (D)TLS method is version-flexible.  This
 * function sanity-checks the proposed value and makes sure the method is
 * version-flexible, then sets the limit if all is well.
 *
 * @method_version: The version of the current SSL_METHOD.
 * @version: the intended limit.
 * @bound: pointer to limit to be updated.
 *
 * Returns 1 on success, 0 on failure.
 */
int ssl_set_version_bound(int method_version, int version, int *bound)
{
    if (version == 0) {
        *bound = version;
        return 1;
    }

    /*-
     * Restrict TLS methods to TLS protocol versions.
     * Restrict DTLS methods to DTLS protocol versions.
     * Note, DTLS version numbers are decreasing, use comparison macros.
     *
     * Note that for both lower-bounds we use explicit versions, not
     * (D)TLS_MIN_VERSION.  This is because we don't want to break user
     * configurations.  If the MIN (supported) version ever rises, the user's
     * "floor" remains valid even if no longer available.  We don't expect the
     * MAX ceiling to ever get lower, so making that variable makes sense.
     */
    switch (method_version) {
    default:
        /*
         * XXX For fixed version methods, should we always fail and not set any
         * bounds, always succeed and not set any bounds, or set the bounds and
         * arrange to fail later if they are not met?  At present fixed-version
         * methods are not subject to controls that disable individual protocol
         * versions.
         */
        return 0;

    case TLS_ANY_VERSION:
        if (version < SSL3_VERSION || version > TLS_MAX_VERSION)
            return 0;
        break;

    case DTLS_ANY_VERSION:
        if (DTLS_VERSION_GT(version, DTLS_MAX_VERSION) ||
            DTLS_VERSION_LT(version, DTLS1_BAD_VER))
            return 0;
        break;
    }

    *bound = version;
    return 1;
}

/*
 * ssl_choose_server_version - Choose server (D)TLS version.  Called when the
 * client HELLO is received to select the final server protocol version and
 * the version specific method.
 *
 * @s: server SSL handle.
 *
 * Returns 0 on success or an SSL error reason number on failure.
 */
int ssl_choose_server_version(SSL *s)
{
    /*-
     * With version-flexible methods we have an initial state with:
     *
     *   s->method->version == (D)TLS_ANY_VERSION,
     *   s->version == (D)TLS_MAX_VERSION.
     *
     * So we detect version-flexible methods via the method version, not the
     * handle version.
     */
    int server_version = s->method->version;
    int client_version = s->client_version;
    const version_info *vent;
    const version_info *table;
    int disabled = 0;

    switch (server_version) {
    default:
        if (version_cmp(s, client_version, s->version) < 0)
            return SSL_R_WRONG_SSL_VERSION;
        /*
         * If this SSL handle is not from a version flexible method we don't
         * (and never did) check min/max FIPS or Suite B constraints.  Hope
         * that's OK.  It is up to the caller to not choose fixed protocol
         * versions they don't want.  If not, then easy to fix, just return
         * ssl_method_error(s, s->method)
         */
        return 0;
    case TLS_ANY_VERSION:
        table = tls_version_table;
        break;
    case DTLS_ANY_VERSION:
        table = dtls_version_table;
        break;
    }

    for (vent = table; vent->version != 0; ++vent) {
        const SSL_METHOD *method;

        if (vent->smeth == NULL ||
            version_cmp(s, client_version, vent->version) < 0)
            continue;
        method = vent->smeth();
        if (ssl_method_error(s, method) == 0) {
            s->version = vent->version;
            s->method = method;
            return 0;
        }
        disabled = 1;
    }
    return disabled ? SSL_R_UNSUPPORTED_PROTOCOL : SSL_R_VERSION_TOO_LOW;
}

/*
 * ssl_choose_client_version - Choose client (D)TLS version.  Called when the
 * server HELLO is received to select the final client protocol version and
 * the version specific method.
 *
 * @s: client SSL handle.
 * @version: The proposed version from the server's HELLO.
 *
 * Returns 0 on success or an SSL error reason number on failure.
 */
int ssl_choose_client_version(SSL *s, int version)
{
    const version_info *vent;
    const version_info *table;

    switch (s->method->version) {
    default:
        if (version != s->version)
            return SSL_R_WRONG_SSL_VERSION;
        /*
         * If this SSL handle is not from a version flexible method we don't
         * (and never did) check min/max, FIPS or Suite B constraints.  Hope
         * that's OK.  It is up to the caller to not choose fixed protocol
         * versions they don't want.  If not, then easy to fix, just return
         * ssl_method_error(s, s->method)
         */
        return 0;
    case TLS_ANY_VERSION:
        table = tls_version_table;
        break;
    case DTLS_ANY_VERSION:
        table = dtls_version_table;
        break;
    }

    for (vent = table; vent->version != 0; ++vent) {
        const SSL_METHOD *method;
        int err;

        if (version != vent->version)
            continue;
        if (vent->cmeth == NULL)
            break;
        method = vent->cmeth();
        err = ssl_method_error(s, method);
        if (err != 0)
            return err;
        s->method = method;
        s->version = version;
        return 0;
    }

    return SSL_R_UNSUPPORTED_PROTOCOL;
}

/*
 * ssl_get_client_min_max_version - get minimum and maximum client version
 * @s: The SSL connection
 * @min_version: The minimum supported version
 * @max_version: The maximum supported version
 *
 * Work out what version we should be using for the initial ClientHello if the
 * version is initially (D)TLS_ANY_VERSION.  We apply any explicit SSL_OP_NO_xxx
 * options, the MinProtocol and MaxProtocol configuration commands, any Suite B
 * or FIPS_mode() constraints and any floor imposed by the security level here,
 * so we don't advertise the wrong protocol version to only reject the outcome later.
 *
 * Computing the right floor matters.  If, e.g., TLS 1.0 and 1.2 are enabled,
 * TLS 1.1 is disabled, but the security level, Suite-B  and/or MinProtocol
 * only allow TLS 1.2, we want to advertise TLS1.2, *not* TLS1.
 *
 * Returns 0 on success or an SSL error reason number on failure.  On failure
 * min_version and max_version will also be set to 0.
 */
int ssl_get_client_min_max_version(const SSL *s, int *min_version,
                                   int *max_version)
{
    int version;
    int hole;
    const SSL_METHOD *single = NULL;
    const SSL_METHOD *method;
    const version_info *table;
    const version_info *vent;

    switch (s->method->version) {
    default:
        /*
         * If this SSL handle is not from a version flexible method we don't
         * (and never did) check min/max FIPS or Suite B constraints.  Hope
         * that's OK.  It is up to the caller to not choose fixed protocol
         * versions they don't want.  If not, then easy to fix, just return
         * ssl_method_error(s, s->method)
         */
        *min_version = *max_version = s->version;
        return 0;
    case TLS_ANY_VERSION:
        table = tls_version_table;
        break;
    case DTLS_ANY_VERSION:
        table = dtls_version_table;
        break;
    }

    /*
     * SSL_OP_NO_X disables all protocols above X *if* there are some protocols
     * below X enabled. This is required in order to maintain the "version
     * capability" vector contiguous. Any versions with a NULL client method
     * (protocol version client is disabled at compile-time) is also a "hole".
     *
     * Our initial state is hole == 1, version == 0.  That is, versions above
     * the first version in the method table are disabled (a "hole" above
     * the valid protocol entries) and we don't have a selected version yet.
     *
     * Whenever "hole == 1", and we hit an enabled method, its version becomes
     * the selected version, and the method becomes a candidate "single"
     * method.  We're no longer in a hole, so "hole" becomes 0.
     *
     * If "hole == 0" and we hit an enabled method, then "single" is cleared,
     * as we support a contiguous range of at least two methods.  If we hit
     * a disabled method, then hole becomes true again, but nothing else
     * changes yet, because all the remaining methods may be disabled too.
     * If we again hit an enabled method after the new hole, it becomes
     * selected, as we start from scratch.
     */
    *min_version = version = 0;
    hole = 1;
    for (vent = table; vent->version != 0; ++vent) {
        /*
         * A table entry with a NULL client method is still a hole in the
         * "version capability" vector.
         */
        if (vent->cmeth == NULL) {
            hole = 1;
            continue;
        }
        method = vent->cmeth();
        if (ssl_method_error(s, method) != 0) {
            hole = 1;
        } else if (!hole) {
            single = NULL;
            *min_version = method->version;
        } else {
            version = (single = method)->version;
            *min_version = version;
            hole = 0;
        }
    }

    *max_version = version;

    /* Fail if everything is disabled */
    if (version == 0)
        return SSL_R_NO_PROTOCOLS_AVAILABLE;

    return 0;
}

/*
 * ssl_set_client_hello_version - Work out what version we should be using for
 * the initial ClientHello.
 *
 * @s: client SSL handle.
 *
 * Returns 0 on success or an SSL error reason number on failure.
 */
int ssl_set_client_hello_version(SSL *s)
{
    int ver_min, ver_max, ret;

    /*
     * In a renegotiation we always send the same client_version that we sent
     * last time, regardless of which version we eventually negotiated.
     */
    if (!SSL_IS_FIRST_HANDSHAKE(s))
        return 0;

    ret = ssl_get_client_min_max_version(s, &ver_min, &ver_max);

    if (ret != 0)
        return ret;

    s->client_version = s->version = ver_max;
    return 0;
}
