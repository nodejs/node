/* ssl/t1_ext.c */
/* ====================================================================
 * Copyright (c) 2014 The OpenSSL Project.  All rights reserved.
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

/* Custom extension utility functions */

#include "ssl_locl.h"

#ifndef OPENSSL_NO_TLSEXT

/* Find a custom extension from the list. */
static custom_ext_method *custom_ext_find(custom_ext_methods *exts,
                                          unsigned int ext_type)
{
    size_t i;
    custom_ext_method *meth = exts->meths;
    for (i = 0; i < exts->meths_count; i++, meth++) {
        if (ext_type == meth->ext_type)
            return meth;
    }
    return NULL;
}

/*
 * Initialise custom extensions flags to indicate neither sent nor received.
 */
void custom_ext_init(custom_ext_methods *exts)
{
    size_t i;
    custom_ext_method *meth = exts->meths;
    for (i = 0; i < exts->meths_count; i++, meth++)
        meth->ext_flags = 0;
}

/* Pass received custom extension data to the application for parsing. */
int custom_ext_parse(SSL *s, int server,
                     unsigned int ext_type,
                     const unsigned char *ext_data, size_t ext_size, int *al)
{
    custom_ext_methods *exts = server ? &s->cert->srv_ext : &s->cert->cli_ext;
    custom_ext_method *meth;
    meth = custom_ext_find(exts, ext_type);
    /* If not found return success */
    if (!meth)
        return 1;
    if (!server) {
        /*
         * If it's ServerHello we can't have any extensions not sent in
         * ClientHello.
         */
        if (!(meth->ext_flags & SSL_EXT_FLAG_SENT)) {
            *al = TLS1_AD_UNSUPPORTED_EXTENSION;
            return 0;
        }
    }
    /* If already present it's a duplicate */
    if (meth->ext_flags & SSL_EXT_FLAG_RECEIVED) {
        *al = TLS1_AD_DECODE_ERROR;
        return 0;
    }
    meth->ext_flags |= SSL_EXT_FLAG_RECEIVED;
    /* If no parse function set return success */
    if (!meth->parse_cb)
        return 1;

    return meth->parse_cb(s, ext_type, ext_data, ext_size, al,
                          meth->parse_arg);
}

/*
 * Request custom extension data from the application and add to the return
 * buffer.
 */
int custom_ext_add(SSL *s, int server,
                   unsigned char **pret, unsigned char *limit, int *al)
{
    custom_ext_methods *exts = server ? &s->cert->srv_ext : &s->cert->cli_ext;
    custom_ext_method *meth;
    unsigned char *ret = *pret;
    size_t i;

    for (i = 0; i < exts->meths_count; i++) {
        const unsigned char *out = NULL;
        size_t outlen = 0;
        meth = exts->meths + i;

        if (server) {
            /*
             * For ServerHello only send extensions present in ClientHello.
             */
            if (!(meth->ext_flags & SSL_EXT_FLAG_RECEIVED))
                continue;
            /* If callback absent for server skip it */
            if (!meth->add_cb)
                continue;
        }
        if (meth->add_cb) {
            int cb_retval = 0;
            cb_retval = meth->add_cb(s, meth->ext_type,
                                     &out, &outlen, al, meth->add_arg);
            if (cb_retval < 0)
                return 0;       /* error */
            if (cb_retval == 0)
                continue;       /* skip this extension */
        }
        if (4 > limit - ret || outlen > (size_t)(limit - ret - 4))
            return 0;
        s2n(meth->ext_type, ret);
        s2n(outlen, ret);
        if (outlen) {
            memcpy(ret, out, outlen);
            ret += outlen;
        }
        /*
         * We can't send duplicates: code logic should prevent this.
         */
        OPENSSL_assert(!(meth->ext_flags & SSL_EXT_FLAG_SENT));
        /*
         * Indicate extension has been sent: this is both a sanity check to
         * ensure we don't send duplicate extensions and indicates that it is
         * not an error if the extension is present in ServerHello.
         */
        meth->ext_flags |= SSL_EXT_FLAG_SENT;
        if (meth->free_cb)
            meth->free_cb(s, meth->ext_type, out, meth->add_arg);
    }
    *pret = ret;
    return 1;
}

/* Copy the flags from src to dst for any extensions that exist in both */
int custom_exts_copy_flags(custom_ext_methods *dst,
                           const custom_ext_methods *src)
{
    size_t i;
    custom_ext_method *methsrc = src->meths;

    for (i = 0; i < src->meths_count; i++, methsrc++) {
        custom_ext_method *methdst = custom_ext_find(dst, methsrc->ext_type);

        if (methdst == NULL)
            continue;

        methdst->ext_flags = methsrc->ext_flags;
    }

    return 1;
}

/* Copy table of custom extensions */
int custom_exts_copy(custom_ext_methods *dst, const custom_ext_methods *src)
{
    if (src->meths_count) {
        dst->meths =
            BUF_memdup(src->meths,
                       sizeof(custom_ext_method) * src->meths_count);
        if (dst->meths == NULL)
            return 0;
        dst->meths_count = src->meths_count;
    }
    return 1;
}

void custom_exts_free(custom_ext_methods *exts)
{
    if (exts->meths)
        OPENSSL_free(exts->meths);
}

/* Set callbacks for a custom extension. */
static int custom_ext_meth_add(custom_ext_methods *exts,
                               unsigned int ext_type,
                               custom_ext_add_cb add_cb,
                               custom_ext_free_cb free_cb,
                               void *add_arg,
                               custom_ext_parse_cb parse_cb, void *parse_arg)
{
    custom_ext_method *meth;
    /*
     * Check application error: if add_cb is not set free_cb will never be
     * called.
     */
    if (!add_cb && free_cb)
        return 0;
    /* Don't add if extension supported internally. */
    if (SSL_extension_supported(ext_type))
        return 0;
    /* Extension type must fit in 16 bits */
    if (ext_type > 0xffff)
        return 0;
    /* Search for duplicate */
    if (custom_ext_find(exts, ext_type))
        return 0;
    meth = OPENSSL_realloc(exts->meths,
                           (exts->meths_count + 1)
                           * sizeof(custom_ext_method));
    if (meth == NULL)
        return 0;

    exts->meths = meth;
    meth += exts->meths_count;
    memset(meth, 0, sizeof(custom_ext_method));
    meth->parse_cb = parse_cb;
    meth->add_cb = add_cb;
    meth->free_cb = free_cb;
    meth->ext_type = ext_type;
    meth->add_arg = add_arg;
    meth->parse_arg = parse_arg;
    exts->meths_count++;
    return 1;
}

/* Application level functions to add custom extension callbacks */
int SSL_CTX_add_client_custom_ext(SSL_CTX *ctx, unsigned int ext_type,
                                  custom_ext_add_cb add_cb,
                                  custom_ext_free_cb free_cb,
                                  void *add_arg,
                                  custom_ext_parse_cb parse_cb,
                                  void *parse_arg)
{
    return custom_ext_meth_add(&ctx->cert->cli_ext, ext_type,
                               add_cb, free_cb, add_arg, parse_cb, parse_arg);
}

int SSL_CTX_add_server_custom_ext(SSL_CTX *ctx, unsigned int ext_type,
                                  custom_ext_add_cb add_cb,
                                  custom_ext_free_cb free_cb,
                                  void *add_arg,
                                  custom_ext_parse_cb parse_cb,
                                  void *parse_arg)
{
    return custom_ext_meth_add(&ctx->cert->srv_ext, ext_type,
                               add_cb, free_cb, add_arg, parse_cb, parse_arg);
}

int SSL_extension_supported(unsigned int ext_type)
{
    switch (ext_type) {
        /* Internally supported extensions. */
    case TLSEXT_TYPE_application_layer_protocol_negotiation:
    case TLSEXT_TYPE_ec_point_formats:
    case TLSEXT_TYPE_elliptic_curves:
    case TLSEXT_TYPE_heartbeat:
# ifndef OPENSSL_NO_NEXTPROTONEG
    case TLSEXT_TYPE_next_proto_neg:
# endif
    case TLSEXT_TYPE_padding:
    case TLSEXT_TYPE_renegotiate:
    case TLSEXT_TYPE_server_name:
    case TLSEXT_TYPE_session_ticket:
    case TLSEXT_TYPE_signature_algorithms:
    case TLSEXT_TYPE_srp:
    case TLSEXT_TYPE_status_request:
    case TLSEXT_TYPE_use_srtp:
# ifdef TLSEXT_TYPE_opaque_prf_input
    case TLSEXT_TYPE_opaque_prf_input:
# endif
# ifdef TLSEXT_TYPE_encrypt_then_mac
    case TLSEXT_TYPE_encrypt_then_mac:
# endif
        return 1;
    default:
        return 0;
    }
}
#endif
