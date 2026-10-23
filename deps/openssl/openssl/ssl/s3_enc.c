/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright 2005 Nokia. All rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "ssl_local.h"
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include "internal/cryptlib.h"
#include "internal/ssl_unwrap.h"

void ssl3_cleanup_key_block(SSL_CONNECTION *s)
{
    OPENSSL_clear_free(s->s3.tmp.key_block, s->s3.tmp.key_block_length);
    s->s3.tmp.key_block = NULL;
    s->s3.tmp.key_block_length = 0;
}

int ssl3_init_finished_mac(SSL_CONNECTION *s)
{
    BIO *buf = BIO_new(BIO_s_mem());

    if (buf == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_BIO_LIB);
        return 0;
    }
    ssl3_free_digest_list(s);
    s->s3.handshake_buffer = buf;
    (void)BIO_set_close(s->s3.handshake_buffer, BIO_CLOSE);
    return 1;
}

/*
 * Free digest list. Also frees handshake buffer since they are always freed
 * together.
 */

void ssl3_free_digest_list(SSL_CONNECTION *s)
{
    BIO_free(s->s3.handshake_buffer);
    s->s3.handshake_buffer = NULL;
    EVP_MD_CTX_free(s->s3.handshake_dgst);
    s->s3.handshake_dgst = NULL;
}

int ssl3_finish_mac(SSL_CONNECTION *s, const unsigned char *buf, size_t len)
{
    int ret;

    if (s->s3.handshake_dgst == NULL) {
        /* Note: this writes to a memory BIO so a failure is a fatal error */
        if (len > INT_MAX) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_OVERFLOW_ERROR);
            return 0;
        }
        ret = BIO_write(s->s3.handshake_buffer, (void *)buf, (int)len);
        if (ret <= 0 || ret != (int)len) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    } else {
        /*
         * rfc9147:
         * In DTLS 1.3, the message transcript is computed over the
         * original TLS 1.3-style Handshake messages without the
         * message_seq, fragment_offset, and fragment_length values. Note
         * that this is a change from DTLS 1.2 where those values were
         * included in the transcript.
         *
         * So this means that we record the full handshake messages in
         * s->s3.handshake_buffer while s->s3.handshake_dgst is not in use and then
         * we calculate the digest when initiating s->s3.handshake_dgst at which
         * point we know what the protocol version is.
         */
        if (s->negotiated_version == DTLS1_3_VERSION) {
            if (!dtls13_transcript_hash_update(s->s3.handshake_dgst, buf, len)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        } else {
            if (!EVP_DigestUpdate(s->s3.handshake_dgst, buf, len)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }
    }
    return 1;
}

int ssl3_digest_cached_records(SSL_CONNECTION *s, int keep)
{
    const EVP_MD *md;
    long hdatalen;
    void *hdata;

    if (s->s3.handshake_dgst == NULL) {
        hdatalen = BIO_get_mem_data(s->s3.handshake_buffer, &hdata);
        if (hdatalen <= 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_HANDSHAKE_LENGTH);
            return 0;
        }

        s->s3.handshake_dgst = EVP_MD_CTX_new();
        if (s->s3.handshake_dgst == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
            return 0;
        }

        md = ssl_handshake_md(s);
        if (md == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR,
                SSL_R_NO_SUITABLE_DIGEST_ALGORITHM);
            return 0;
        }
        if (!EVP_DigestInit_ex(s->s3.handshake_dgst, md, NULL)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        if (!ssl3_finish_mac(s, hdata, hdatalen)) {
            /* SSLfatal() already called */
            return 0;
        }
    }
    if (keep == 0) {
        BIO_free(s->s3.handshake_buffer);
        s->s3.handshake_buffer = NULL;
    }

    return 1;
}
