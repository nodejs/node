/*
 * Copyright 2014-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Custom extension utility functions */

#include <openssl/ct.h>
#include "ssl_locl.h"

/* Find a custom extension from the list. */
static custom_ext_method *custom_ext_find(const custom_ext_methods *exts,
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

    return meth->parse_cb(s, ext_type, ext_data, ext_size, al, meth->parse_arg);
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
            OPENSSL_memdup(src->meths,
                           sizeof(custom_ext_method) * src->meths_count);
        if (dst->meths == NULL)
            return 0;
        dst->meths_count = src->meths_count;
    }
    return 1;
}

void custom_exts_free(custom_ext_methods *exts)
{
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
    custom_ext_method *meth, *tmp;
    /*
     * Check application error: if add_cb is not set free_cb will never be
     * called.
     */
    if (!add_cb && free_cb)
        return 0;
    /*
     * Don't add if extension supported internally, but make exception
     * for extension types that previously were not supported, but now are.
     */
    if (SSL_extension_supported(ext_type) &&
        ext_type != TLSEXT_TYPE_signed_certificate_timestamp)
        return 0;
    /* Extension type must fit in 16 bits */
    if (ext_type > 0xffff)
        return 0;
    /* Search for duplicate */
    if (custom_ext_find(exts, ext_type))
        return 0;
    tmp = OPENSSL_realloc(exts->meths,
                          (exts->meths_count + 1) * sizeof(custom_ext_method));

    if (tmp == NULL)
        return 0;

    exts->meths = tmp;
    meth = exts->meths + exts->meths_count;
    memset(meth, 0, sizeof(*meth));
    meth->parse_cb = parse_cb;
    meth->add_cb = add_cb;
    meth->free_cb = free_cb;
    meth->ext_type = ext_type;
    meth->add_arg = add_arg;
    meth->parse_arg = parse_arg;
    exts->meths_count++;
    return 1;
}

/* Return true if a client custom extension exists, false otherwise */
int SSL_CTX_has_client_custom_ext(const SSL_CTX *ctx, unsigned int ext_type)
{
    return custom_ext_find(&ctx->cert->cli_ext, ext_type) != NULL;
}

/* Application level functions to add custom extension callbacks */
int SSL_CTX_add_client_custom_ext(SSL_CTX *ctx, unsigned int ext_type,
                                  custom_ext_add_cb add_cb,
                                  custom_ext_free_cb free_cb,
                                  void *add_arg,
                                  custom_ext_parse_cb parse_cb, void *parse_arg)
{
#ifndef OPENSSL_NO_CT
    /*
     * We don't want applications registering callbacks for SCT extensions
     * whilst simultaneously using the built-in SCT validation features, as
     * these two things may not play well together.
     */
    if (ext_type == TLSEXT_TYPE_signed_certificate_timestamp &&
        SSL_CTX_ct_is_enabled(ctx))
        return 0;
#endif
    return custom_ext_meth_add(&ctx->cert->cli_ext, ext_type, add_cb,
                               free_cb, add_arg, parse_cb, parse_arg);
}

int SSL_CTX_add_server_custom_ext(SSL_CTX *ctx, unsigned int ext_type,
                                  custom_ext_add_cb add_cb,
                                  custom_ext_free_cb free_cb,
                                  void *add_arg,
                                  custom_ext_parse_cb parse_cb, void *parse_arg)
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
#ifndef OPENSSL_NO_NEXTPROTONEG
    case TLSEXT_TYPE_next_proto_neg:
#endif
    case TLSEXT_TYPE_padding:
    case TLSEXT_TYPE_renegotiate:
    case TLSEXT_TYPE_server_name:
    case TLSEXT_TYPE_session_ticket:
    case TLSEXT_TYPE_signature_algorithms:
    case TLSEXT_TYPE_srp:
    case TLSEXT_TYPE_status_request:
    case TLSEXT_TYPE_signed_certificate_timestamp:
    case TLSEXT_TYPE_use_srtp:
#ifdef TLSEXT_TYPE_encrypt_then_mac
    case TLSEXT_TYPE_encrypt_then_mac:
#endif
        return 1;
    default:
        return 0;
    }
}
