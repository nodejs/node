/*
 * Copyright 2011-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * DTLS code by Eric Rescorla <ekr@rtfm.com>
 *
 * Copyright (C) 2006, Network Resonance, Inc. Copyright (C) 2011, RTFM, Inc.
 */

#include <stdio.h>
#include <openssl/objects.h>
#include "ssl_locl.h"

#ifndef OPENSSL_NO_SRTP

static SRTP_PROTECTION_PROFILE srtp_known_profiles[] = {
    {
     "SRTP_AES128_CM_SHA1_80",
     SRTP_AES128_CM_SHA1_80,
     },
    {
     "SRTP_AES128_CM_SHA1_32",
     SRTP_AES128_CM_SHA1_32,
     },
    {
     "SRTP_AEAD_AES_128_GCM",
     SRTP_AEAD_AES_128_GCM,
     },
    {
     "SRTP_AEAD_AES_256_GCM",
     SRTP_AEAD_AES_256_GCM,
     },
    {0}
};

static int find_profile_by_name(char *profile_name,
                                SRTP_PROTECTION_PROFILE **pptr, unsigned len)
{
    SRTP_PROTECTION_PROFILE *p;

    p = srtp_known_profiles;
    while (p->name) {
        if ((len == strlen(p->name))
            && strncmp(p->name, profile_name, len) == 0) {
            *pptr = p;
            return 0;
        }

        p++;
    }

    return 1;
}

static int ssl_ctx_make_profiles(const char *profiles_string,
                                 STACK_OF(SRTP_PROTECTION_PROFILE) **out)
{
    STACK_OF(SRTP_PROTECTION_PROFILE) *profiles;

    char *col;
    char *ptr = (char *)profiles_string;
    SRTP_PROTECTION_PROFILE *p;

    if ((profiles = sk_SRTP_PROTECTION_PROFILE_new_null()) == NULL) {
        SSLerr(SSL_F_SSL_CTX_MAKE_PROFILES,
               SSL_R_SRTP_COULD_NOT_ALLOCATE_PROFILES);
        return 1;
    }

    do {
        col = strchr(ptr, ':');

        if (!find_profile_by_name(ptr, &p, col ? col - ptr : (int)strlen(ptr))) {
            if (sk_SRTP_PROTECTION_PROFILE_find(profiles, p) >= 0) {
                SSLerr(SSL_F_SSL_CTX_MAKE_PROFILES,
                       SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
                goto err;
            }

            if (!sk_SRTP_PROTECTION_PROFILE_push(profiles, p)) {
                SSLerr(SSL_F_SSL_CTX_MAKE_PROFILES,
                       SSL_R_SRTP_COULD_NOT_ALLOCATE_PROFILES);
                goto err;
            }
        } else {
            SSLerr(SSL_F_SSL_CTX_MAKE_PROFILES,
                   SSL_R_SRTP_UNKNOWN_PROTECTION_PROFILE);
            goto err;
        }

        if (col)
            ptr = col + 1;
    } while (col);

    sk_SRTP_PROTECTION_PROFILE_free(*out);

    *out = profiles;

    return 0;
 err:
    sk_SRTP_PROTECTION_PROFILE_free(profiles);
    return 1;
}

int SSL_CTX_set_tlsext_use_srtp(SSL_CTX *ctx, const char *profiles)
{
    return ssl_ctx_make_profiles(profiles, &ctx->srtp_profiles);
}

int SSL_set_tlsext_use_srtp(SSL *s, const char *profiles)
{
    return ssl_ctx_make_profiles(profiles, &s->srtp_profiles);
}

STACK_OF(SRTP_PROTECTION_PROFILE) *SSL_get_srtp_profiles(SSL *s)
{
    if (s != NULL) {
        if (s->srtp_profiles != NULL) {
            return s->srtp_profiles;
        } else if ((s->ctx != NULL) && (s->ctx->srtp_profiles != NULL)) {
            return s->ctx->srtp_profiles;
        }
    }

    return NULL;
}

SRTP_PROTECTION_PROFILE *SSL_get_selected_srtp_profile(SSL *s)
{
    return s->srtp_profile;
}

/*
 * Note: this function returns 0 length if there are no profiles specified
 */
int ssl_add_clienthello_use_srtp_ext(SSL *s, unsigned char *p, int *len,
                                     int maxlen)
{
    int ct = 0;
    int i;
    STACK_OF(SRTP_PROTECTION_PROFILE) *clnt = 0;
    SRTP_PROTECTION_PROFILE *prof;

    clnt = SSL_get_srtp_profiles(s);
    ct = sk_SRTP_PROTECTION_PROFILE_num(clnt); /* -1 if clnt == 0 */

    if (p) {
        if (ct == 0) {
            SSLerr(SSL_F_SSL_ADD_CLIENTHELLO_USE_SRTP_EXT,
                   SSL_R_EMPTY_SRTP_PROTECTION_PROFILE_LIST);
            return 1;
        }

        if ((2 + ct * 2 + 1) > maxlen) {
            SSLerr(SSL_F_SSL_ADD_CLIENTHELLO_USE_SRTP_EXT,
                   SSL_R_SRTP_PROTECTION_PROFILE_LIST_TOO_LONG);
            return 1;
        }

        /* Add the length */
        s2n(ct * 2, p);
        for (i = 0; i < ct; i++) {
            prof = sk_SRTP_PROTECTION_PROFILE_value(clnt, i);
            s2n(prof->id, p);
        }

        /* Add an empty use_mki value */
        *p++ = 0;
    }

    *len = 2 + ct * 2 + 1;

    return 0;
}

int ssl_parse_clienthello_use_srtp_ext(SSL *s, PACKET *pkt, int *al)
{
    SRTP_PROTECTION_PROFILE *sprof;
    STACK_OF(SRTP_PROTECTION_PROFILE) *srvr;
    unsigned int ct, mki_len, id;
    int i, srtp_pref;
    PACKET subpkt;

    /* Pull off the length of the cipher suite list  and check it is even */
    if (!PACKET_get_net_2(pkt, &ct)
        || (ct & 1) != 0 || !PACKET_get_sub_packet(pkt, &subpkt, ct)) {
        SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_USE_SRTP_EXT,
               SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
        *al = SSL_AD_DECODE_ERROR;
        return 1;
    }

    srvr = SSL_get_srtp_profiles(s);
    s->srtp_profile = NULL;
    /* Search all profiles for a match initially */
    srtp_pref = sk_SRTP_PROTECTION_PROFILE_num(srvr);

    while (PACKET_remaining(&subpkt)) {
        if (!PACKET_get_net_2(&subpkt, &id)) {
            SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_USE_SRTP_EXT,
                   SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
            *al = SSL_AD_DECODE_ERROR;
            return 1;
        }

        /*
         * Only look for match in profiles of higher preference than
         * current match.
         * If no profiles have been have been configured then this
         * does nothing.
         */
        for (i = 0; i < srtp_pref; i++) {
            sprof = sk_SRTP_PROTECTION_PROFILE_value(srvr, i);
            if (sprof->id == id) {
                s->srtp_profile = sprof;
                srtp_pref = i;
                break;
            }
        }
    }

    /*
     * Now extract the MKI value as a sanity check, but discard it for now
     */
    if (!PACKET_get_1(pkt, &mki_len)) {
        SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_USE_SRTP_EXT,
               SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
        *al = SSL_AD_DECODE_ERROR;
        return 1;
    }

    if (!PACKET_forward(pkt, mki_len)
        || PACKET_remaining(pkt)) {
        SSLerr(SSL_F_SSL_PARSE_CLIENTHELLO_USE_SRTP_EXT,
               SSL_R_BAD_SRTP_MKI_VALUE);
        *al = SSL_AD_DECODE_ERROR;
        return 1;
    }

    return 0;
}

int ssl_add_serverhello_use_srtp_ext(SSL *s, unsigned char *p, int *len,
                                     int maxlen)
{
    if (p) {
        if (maxlen < 5) {
            SSLerr(SSL_F_SSL_ADD_SERVERHELLO_USE_SRTP_EXT,
                   SSL_R_SRTP_PROTECTION_PROFILE_LIST_TOO_LONG);
            return 1;
        }

        if (s->srtp_profile == 0) {
            SSLerr(SSL_F_SSL_ADD_SERVERHELLO_USE_SRTP_EXT,
                   SSL_R_USE_SRTP_NOT_NEGOTIATED);
            return 1;
        }
        s2n(2, p);
        s2n(s->srtp_profile->id, p);
        *p++ = 0;
    }
    *len = 5;

    return 0;
}

int ssl_parse_serverhello_use_srtp_ext(SSL *s, PACKET *pkt, int *al)
{
    unsigned int id, ct, mki;
    int i;

    STACK_OF(SRTP_PROTECTION_PROFILE) *clnt;
    SRTP_PROTECTION_PROFILE *prof;

    if (!PACKET_get_net_2(pkt, &ct)
        || ct != 2 || !PACKET_get_net_2(pkt, &id)
        || !PACKET_get_1(pkt, &mki)
        || PACKET_remaining(pkt) != 0) {
        SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_USE_SRTP_EXT,
               SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
        *al = SSL_AD_DECODE_ERROR;
        return 1;
    }

    if (mki != 0) {
        /* Must be no MKI, since we never offer one */
        SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_USE_SRTP_EXT,
               SSL_R_BAD_SRTP_MKI_VALUE);
        *al = SSL_AD_ILLEGAL_PARAMETER;
        return 1;
    }

    clnt = SSL_get_srtp_profiles(s);

    /* Throw an error if the server gave us an unsolicited extension */
    if (clnt == NULL) {
        SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_USE_SRTP_EXT,
               SSL_R_NO_SRTP_PROFILES);
        *al = SSL_AD_DECODE_ERROR;
        return 1;
    }

    /*
     * Check to see if the server gave us something we support (and
     * presumably offered)
     */
    for (i = 0; i < sk_SRTP_PROTECTION_PROFILE_num(clnt); i++) {
        prof = sk_SRTP_PROTECTION_PROFILE_value(clnt, i);

        if (prof->id == id) {
            s->srtp_profile = prof;
            *al = 0;
            return 0;
        }
    }

    SSLerr(SSL_F_SSL_PARSE_SERVERHELLO_USE_SRTP_EXT,
           SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
    *al = SSL_AD_DECODE_ERROR;
    return 1;
}

#endif
