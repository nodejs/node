/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
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
#include <openssl/buffer.h>
#include "ssl_locl.h"

#ifndef OPENSSL_NO_STDIO
int SSL_SESSION_print_fp(FILE *fp, const SSL_SESSION *x)
{
    BIO *b;
    int ret;

    if ((b = BIO_new(BIO_s_file())) == NULL) {
        SSLerr(SSL_F_SSL_SESSION_PRINT_FP, ERR_R_BUF_LIB);
        return (0);
    }
    BIO_set_fp(b, fp, BIO_NOCLOSE);
    ret = SSL_SESSION_print(b, x);
    BIO_free(b);
    return (ret);
}
#endif

int SSL_SESSION_print(BIO *bp, const SSL_SESSION *x)
{
    unsigned int i;
    const char *s;

    if (x == NULL)
        goto err;
    if (BIO_puts(bp, "SSL-Session:\n") <= 0)
        goto err;
    s = ssl_protocol_to_string(x->ssl_version);
    if (BIO_printf(bp, "    Protocol  : %s\n", s) <= 0)
        goto err;

    if (x->cipher == NULL) {
        if (((x->cipher_id) & 0xff000000) == 0x02000000) {
            if (BIO_printf
                (bp, "    Cipher    : %06lX\n", x->cipher_id & 0xffffff) <= 0)
                goto err;
        } else {
            if (BIO_printf
                (bp, "    Cipher    : %04lX\n", x->cipher_id & 0xffff) <= 0)
                goto err;
        }
    } else {
        if (BIO_printf
            (bp, "    Cipher    : %s\n",
             ((x->cipher == NULL) ? "unknown" : x->cipher->name)) <= 0)
            goto err;
    }
    if (BIO_puts(bp, "    Session-ID: ") <= 0)
        goto err;
    for (i = 0; i < x->session_id_length; i++) {
        if (BIO_printf(bp, "%02X", x->session_id[i]) <= 0)
            goto err;
    }
    if (BIO_puts(bp, "\n    Session-ID-ctx: ") <= 0)
        goto err;
    for (i = 0; i < x->sid_ctx_length; i++) {
        if (BIO_printf(bp, "%02X", x->sid_ctx[i]) <= 0)
            goto err;
    }
    if (BIO_puts(bp, "\n    Master-Key: ") <= 0)
        goto err;
    for (i = 0; i < (unsigned int)x->master_key_length; i++) {
        if (BIO_printf(bp, "%02X", x->master_key[i]) <= 0)
            goto err;
    }
#ifndef OPENSSL_NO_PSK
    if (BIO_puts(bp, "\n    PSK identity: ") <= 0)
        goto err;
    if (BIO_printf(bp, "%s", x->psk_identity ? x->psk_identity : "None") <= 0)
        goto err;
    if (BIO_puts(bp, "\n    PSK identity hint: ") <= 0)
        goto err;
    if (BIO_printf
        (bp, "%s", x->psk_identity_hint ? x->psk_identity_hint : "None") <= 0)
        goto err;
#endif
#ifndef OPENSSL_NO_SRP
    if (BIO_puts(bp, "\n    SRP username: ") <= 0)
        goto err;
    if (BIO_printf(bp, "%s", x->srp_username ? x->srp_username : "None") <= 0)
        goto err;
#endif
    if (x->tlsext_tick_lifetime_hint) {
        if (BIO_printf(bp,
                       "\n    TLS session ticket lifetime hint: %ld (seconds)",
                       x->tlsext_tick_lifetime_hint) <= 0)
            goto err;
    }
    if (x->tlsext_tick) {
        if (BIO_puts(bp, "\n    TLS session ticket:\n") <= 0)
            goto err;
        if (BIO_dump_indent
            (bp, (const char *)x->tlsext_tick, x->tlsext_ticklen, 4)
            <= 0)
            goto err;
    }
#ifndef OPENSSL_NO_COMP
    if (x->compress_meth != 0) {
        SSL_COMP *comp = NULL;

        if (!ssl_cipher_get_evp(x, NULL, NULL, NULL, NULL, &comp, 0))
            goto err;
        if (comp == NULL) {
            if (BIO_printf(bp, "\n    Compression: %d", x->compress_meth) <= 0)
                goto err;
        } else {
            if (BIO_printf(bp, "\n    Compression: %d (%s)", comp->id,
                           comp->name) <= 0)
                goto err;
        }
    }
#endif
    if (x->time != 0L) {
        if (BIO_printf(bp, "\n    Start Time: %ld", x->time) <= 0)
            goto err;
    }
    if (x->timeout != 0L) {
        if (BIO_printf(bp, "\n    Timeout   : %ld (sec)", x->timeout) <= 0)
            goto err;
    }
    if (BIO_puts(bp, "\n") <= 0)
        goto err;

    if (BIO_puts(bp, "    Verify return code: ") <= 0)
        goto err;
    if (BIO_printf(bp, "%ld (%s)\n", x->verify_result,
                   X509_verify_cert_error_string(x->verify_result)) <= 0)
        goto err;

    if (BIO_printf(bp, "    Extended master secret: %s\n",
                   x->flags & SSL_SESS_FLAG_EXTMS ? "yes" : "no") <= 0)
        goto err;

    return (1);
 err:
    return (0);
}

/*
 * print session id and master key in NSS keylog format (RSA
 * Session-ID:<session id> Master-Key:<master key>)
 */
int SSL_SESSION_print_keylog(BIO *bp, const SSL_SESSION *x)
{
    unsigned int i;

    if (x == NULL)
        goto err;
    if (x->session_id_length == 0 || x->master_key_length == 0)
        goto err;

    /*
     * the RSA prefix is required by the format's definition although there's
     * nothing RSA-specific in the output, therefore, we don't have to check if
     * the cipher suite is based on RSA
     */
    if (BIO_puts(bp, "RSA ") <= 0)
        goto err;

    if (BIO_puts(bp, "Session-ID:") <= 0)
        goto err;
    for (i = 0; i < x->session_id_length; i++) {
        if (BIO_printf(bp, "%02X", x->session_id[i]) <= 0)
            goto err;
    }
    if (BIO_puts(bp, " Master-Key:") <= 0)
        goto err;
    for (i = 0; i < (unsigned int)x->master_key_length; i++) {
        if (BIO_printf(bp, "%02X", x->master_key[i]) <= 0)
            goto err;
    }
    if (BIO_puts(bp, "\n") <= 0)
        goto err;

    return (1);
 err:
    return (0);
}
