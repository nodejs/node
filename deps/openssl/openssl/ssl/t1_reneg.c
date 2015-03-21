/* ssl/t1_reneg.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2009 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
#include <stdio.h>
#include <openssl/objects.h>
#include "ssl_locl.h"

/* Add the client's renegotiation binding */
int ssl_add_clienthello_renegotiate_ext(SSL *s, unsigned char *p, int *len,
                                        int maxlen)
{
    if (p) {
        if ((s->s3->previous_client_finished_len + 1) > maxlen) {
            SSLerr(SSL_F_SSL_ADD_CLIENTHELLO_RENEGOTIATE_EXT,
                   SSL_R_RENEGOTIATE_EXT_TOO_LONG);
            return 0;
        }

        /* Length byte */
        *p = s->s3->previous_client_finished_len;
        p++;

        memcpy(p, s->s3->previous_client_finished,
               s->s3->previous_client_finished_len);
#ifdef OPENSSL_RI_DEBUG
        fprintf(stderr, "%s RI extension sent by client\n",
                s->s3->previous_client_finished_len ? "Non-empty" : "Empty");
#endif
    }

    *len = s->s3->previous_client_finished_len + 1;

    return 1;
}

/*
 * Parse the client's renegotiation binding and abort if it's not right
 */
int ssl_parse_clienthello_renegotiate_ext(SSL *s, unsigned char *d, int len,
                                          int *al)
{
    int ilen;

    /* Parse the length byte */
    if (len < 1) {
        SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_RENEGOTIATE_EXT,
               SSL_R_RENEGOTIATION_ENCODING_ERR);
        *al = SSL_AD_ILLEGAL_PARAMETER;
        return 0;
    }
    ilen = *d;
    d++;

    /* Consistency check */
    if ((ilen + 1) != len) {
        SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_RENEGOTIATE_EXT,
               SSL_R_RENEGOTIATION_ENCODING_ERR);
        *al = SSL_AD_ILLEGAL_PARAMETER;
        return 0;
    }

    /* Check that the extension matches */
    if (ilen != s->s3->previous_client_finished_len) {
        SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_RENEGOTIATE_EXT,
               SSL_R_RENEGOTIATION_MISMATCH);
        *al = SSL_AD_HANDSHAKE_FAILURE;
        return 0;
    }

    if (memcmp(d, s->s3->previous_client_finished,
               s->s3->previous_client_finished_len)) {
        SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_RENEGOTIATE_EXT,
               SSL_R_RENEGOTIATION_MISMATCH);
        *al = SSL_AD_HANDSHAKE_FAILURE;
        return 0;
    }
#ifdef OPENSSL_RI_DEBUG
    fprintf(stderr, "%s RI extension received by server\n",
            ilen ? "Non-empty" : "Empty");
#endif

    s->s3->send_connection_binding = 1;

    return 1;
}

/* Add the server's renegotiation binding */
int ssl_add_serverhello_renegotiate_ext(SSL *s, unsigned char *p, int *len,
                                        int maxlen)
{
    if (p) {
        if ((s->s3->previous_client_finished_len +
             s->s3->previous_server_finished_len + 1) > maxlen) {
            SSLerr(SSL_F_SSL_ADD_SERVERHELLO_RENEGOTIATE_EXT,
                   SSL_R_RENEGOTIATE_EXT_TOO_LONG);
            return 0;
        }

        /* Length byte */
        *p = s->s3->previous_client_finished_len +
            s->s3->previous_server_finished_len;
        p++;

        memcpy(p, s->s3->previous_client_finished,
               s->s3->previous_client_finished_len);
        p += s->s3->previous_client_finished_len;

        memcpy(p, s->s3->previous_server_finished,
               s->s3->previous_server_finished_len);
#ifdef OPENSSL_RI_DEBUG
        fprintf(stderr, "%s RI extension sent by server\n",
                s->s3->previous_client_finished_len ? "Non-empty" : "Empty");
#endif
    }

    *len = s->s3->previous_client_finished_len
        + s->s3->previous_server_finished_len + 1;

    return 1;
}

/*
 * Parse the server's renegotiation binding and abort if it's not right
 */
int ssl_parse_serverhello_renegotiate_ext(SSL *s, unsigned char *d, int len,
                                          int *al)
{
    int expected_len = s->s3->previous_client_finished_len
        + s->s3->previous_server_finished_len;
    int ilen;

    /* Check for logic errors */
    OPENSSL_assert(!expected_len || s->s3->previous_client_finished_len);
    OPENSSL_assert(!expected_len || s->s3->previous_server_finished_len);

    /* Parse the length byte */
    if (len < 1) {
        SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_RENEGOTIATE_EXT,
               SSL_R_RENEGOTIATION_ENCODING_ERR);
        *al = SSL_AD_ILLEGAL_PARAMETER;
        return 0;
    }
    ilen = *d;
    d++;

    /* Consistency check */
    if (ilen + 1 != len) {
        SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_RENEGOTIATE_EXT,
               SSL_R_RENEGOTIATION_ENCODING_ERR);
        *al = SSL_AD_ILLEGAL_PARAMETER;
        return 0;
    }

    /* Check that the extension matches */
    if (ilen != expected_len) {
        SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_RENEGOTIATE_EXT,
               SSL_R_RENEGOTIATION_MISMATCH);
        *al = SSL_AD_HANDSHAKE_FAILURE;
        return 0;
    }

    if (memcmp(d, s->s3->previous_client_finished,
               s->s3->previous_client_finished_len)) {
        SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_RENEGOTIATE_EXT,
               SSL_R_RENEGOTIATION_MISMATCH);
        *al = SSL_AD_HANDSHAKE_FAILURE;
        return 0;
    }
    d += s->s3->previous_client_finished_len;

    if (memcmp(d, s->s3->previous_server_finished,
               s->s3->previous_server_finished_len)) {
        SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_RENEGOTIATE_EXT,
               SSL_R_RENEGOTIATION_MISMATCH);
        *al = SSL_AD_ILLEGAL_PARAMETER;
        return 0;
    }
#ifdef OPENSSL_RI_DEBUG
    fprintf(stderr, "%s RI extension received by client\n",
            ilen ? "Non-empty" : "Empty");
#endif
    s->s3->send_connection_binding = 1;

    return 1;
}
