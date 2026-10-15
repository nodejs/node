/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/ssl.h>
#include <openssl/ech.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include "internal/ech_helpers.h"
#include "internal/ssl_unwrap.h"
#include "../ssl_local.h"
#include "../statem/statem_local.h"
#include "ech_local.h"

#ifndef OPENSSL_NO_ECH

/*
 * Strings used in ECH crypto derivations (odd format for EBCDIC goodness)
 */
/* "ech accept confirmation" */
static const char OSSL_ECH_ACCEPT_CONFIRM_STRING[] = "\x65\x63\x68\x20\x61\x63\x63\x65\x70\x74\x20\x63\x6f\x6e\x66\x69\x72\x6d\x61\x74\x69\x6f\x6e";
/* "hrr ech accept confirmation" */
static const char OSSL_ECH_HRR_CONFIRM_STRING[] = "\x68\x72\x72\x20\x65\x63\x68\x20\x61\x63\x63\x65\x70\x74\x20\x63\x6f\x6e\x66\x69\x72\x6d\x61\x74\x69\x6f\x6e";

/* ECH internal API functions */

#ifdef OSSL_ECH_SUPERVERBOSE
/* ascii-hex print a buffer nicely for debug/interop purposes */
void ossl_ech_pbuf(const char *msg, const unsigned char *buf, const size_t blen)
{
    OSSL_TRACE_BEGIN(TLS)
    {
        if (msg == NULL) {
            BIO_printf(trc_out, "msg is NULL\n");
        } else if (buf == NULL || blen == 0) {
            BIO_printf(trc_out, "%s: buf is %p\n", msg, (void *)buf);
            BIO_printf(trc_out, "%s: blen is %zu\n", msg, blen);
        } else {
            BIO_printf(trc_out, "%s (%zu)\n", msg, blen);
            BIO_dump_indent(trc_out, buf, (int)blen, 4);
        }
    }
    OSSL_TRACE_END(TLS);
    return;
}

/* trace out transcript */
static void ossl_ech_ptranscript(SSL_CONNECTION *s, const char *msg)
{
    size_t hdatalen = 0;
    unsigned char *hdata = NULL;
    unsigned char ddata[EVP_MAX_MD_SIZE];
    size_t ddatalen;

    if (s == NULL)
        return;
    hdatalen = BIO_get_mem_data(s->s3.handshake_buffer, &hdata);
    ossl_ech_pbuf(msg, hdata, hdatalen);
    if (s->s3.handshake_dgst != NULL) {
        if (ssl_handshake_hash(s, ddata, sizeof(ddata), &ddatalen) == 0) {
            OSSL_TRACE(TLS, "ssl_handshake_hash failed\n");
            ossl_ech_pbuf(msg, ddata, ddatalen);
        }
    }
    OSSL_TRACE(TLS, "new transbuf:\n");
    ossl_ech_pbuf(msg, s->ext.ech.transbuf, s->ext.ech.transbuf_len);
    return;
}
#endif

static OSSL_ECHSTORE_ENTRY *ossl_echstore_entry_dup(const OSSL_ECHSTORE_ENTRY *orig)
{
    OSSL_ECHSTORE_ENTRY *ret = NULL;

    if (orig == NULL)
        return NULL;
    ret = OPENSSL_zalloc(sizeof(*ret));
    if (ret == NULL)
        return NULL;
    ret->version = orig->version;
    if (orig->public_name != NULL) {
        ret->public_name = OPENSSL_strdup(orig->public_name);
        if (ret->public_name == NULL)
            goto err;
    }
    ret->pub_len = orig->pub_len;
    if (orig->pub != NULL) {
        ret->pub = OPENSSL_memdup(orig->pub, orig->pub_len);
        if (ret->pub == NULL)
            goto err;
    }
    ret->nsuites = orig->nsuites;
    ret->suites = OPENSSL_memdup(orig->suites, sizeof(OSSL_HPKE_SUITE) * ret->nsuites);
    if (ret->suites == NULL)
        goto err;
    ret->max_name_length = orig->max_name_length;
    ret->config_id = orig->config_id;
    if (orig->exts != NULL) {
        ret->exts = sk_OSSL_ECHEXT_deep_copy(orig->exts, ossl_echext_dup,
            ossl_echext_free);
        if (ret->exts == NULL)
            goto err;
    }
    ret->loadtime = orig->loadtime;
    if (orig->keyshare != NULL) {
        if (!EVP_PKEY_up_ref(orig->keyshare))
            goto err;
        ret->keyshare = orig->keyshare;
    }
    ret->for_retry = orig->for_retry;
    if (orig->encoded != NULL) {
        ret->encoded_len = orig->encoded_len;
        ret->encoded = OPENSSL_memdup(orig->encoded, ret->encoded_len);
        if (ret->encoded == NULL)
            goto err;
    }
    return ret;
err:
    ossl_echstore_entry_free(ret);
    return NULL;
}

/* duplicate an OSSL_ECHSTORE as needed */
OSSL_ECHSTORE *ossl_echstore_dup(const OSSL_ECHSTORE *old)
{
    OSSL_ECHSTORE *cp = NULL;

    if (old == NULL)
        return NULL;
    cp = OPENSSL_zalloc(sizeof(*cp));
    if (cp == NULL)
        return NULL;
    cp->libctx = old->libctx;
    if (old->propq != NULL) {
        cp->propq = OPENSSL_strdup(old->propq);
        if (cp->propq == NULL)
            goto err;
    }
    if (old->entries != NULL) {
        cp->entries = sk_OSSL_ECHSTORE_ENTRY_deep_copy(old->entries,
            ossl_echstore_entry_dup,
            ossl_echstore_entry_free);
        if (cp->entries == NULL)
            goto err;
    }
    return cp;
err:
    OSSL_ECHSTORE_free(cp);
    return NULL;
}

void ossl_ech_ctx_clear(OSSL_ECH_CTX *ce)
{
    if (ce == NULL)
        return;
    OSSL_ECHSTORE_free(ce->es);
    OPENSSL_free(ce->alpn_outer);
    return;
}

static void ech_free_stashed_key_shares(OSSL_ECH_CONN *ec)
{
    size_t i;

    if (ec == NULL)
        return;
    for (i = 0; i != ec->num_ks_pkey; i++) {
        EVP_PKEY_free(ec->ks_pkey[i]);
        ec->ks_pkey[i] = NULL;
    }
    ec->num_ks_pkey = 0;
    return;
}

void ossl_ech_conn_clear(OSSL_ECH_CONN *ec)
{
    if (ec == NULL)
        return;
    OSSL_ECHSTORE_free(ec->es);
    OPENSSL_free(ec->outer_hostname);
    OPENSSL_free(ec->alpn_outer);
    OPENSSL_free(ec->former_inner);
    OPENSSL_free(ec->transbuf);
    OPENSSL_free(ec->innerch);
    OPENSSL_free(ec->grease_suite);
    OPENSSL_free(ec->sent);
    OPENSSL_free(ec->returned);
    OPENSSL_free(ec->pub);
    OSSL_HPKE_CTX_free(ec->hpke_ctx);
    OPENSSL_free(ec->encoded_inner);
    ech_free_stashed_key_shares(ec);
    return;
}

/* called from ssl/ssl_lib.c: ossl_ssl_connection_new_int */
int ossl_ech_conn_init(SSL_CONNECTION *s, SSL_CTX *ctx,
    const SSL_METHOD *method)
{
    memset(&s->ext.ech, 0, sizeof(s->ext.ech));
    if (ctx->ext.ech.es != NULL
        && (s->ext.ech.es = ossl_echstore_dup(ctx->ext.ech.es)) == NULL)
        goto err;
    s->ext.ech.cb = ctx->ext.ech.cb;
    if (ctx->ext.ech.alpn_outer != NULL) {
        s->ext.ech.alpn_outer = OPENSSL_memdup(ctx->ext.ech.alpn_outer,
            ctx->ext.ech.alpn_outer_len);
        if (s->ext.ech.alpn_outer == NULL)
            goto err;
        s->ext.ech.alpn_outer_len = ctx->ext.ech.alpn_outer_len;
    }
    /* initialise type/cid to unknown */
    s->ext.ech.attempted_type = OSSL_ECH_type_unknown;
    s->ext.ech.attempted_cid = OSSL_ECH_config_id_unset;
    if (s->ext.ech.es != NULL)
        s->ext.ech.attempted = 1;
    if ((ctx->options & SSL_OP_ECH_GREASE) != 0)
        s->options |= SSL_OP_ECH_GREASE;
    return 1;
err:
    OSSL_ECHSTORE_free(s->ext.ech.es);
    s->ext.ech.es = NULL;
    OPENSSL_free(s->ext.ech.alpn_outer);
    s->ext.ech.alpn_outer = NULL;
    s->ext.ech.alpn_outer_len = 0;
    return 0;
}

/*
 * Assemble the set of ECHConfig values to return as retry-configs.
 * The caller (stoc ECH extension handler) needs to OPENSSL_free the rcfgs
 * The rcfgs itself is missing the outer length to make it an ECHConfigList
 * so the caller adds that using WPACKET functions
 */
int ossl_ech_get_retry_configs(SSL_CONNECTION *s, unsigned char **rcfgs,
    size_t *rcfgslen)
{
    OSSL_ECHSTORE *es = NULL;
    OSSL_ECHSTORE_ENTRY *ee = NULL;
    int i, num = 0;
    size_t retslen = 0;
    unsigned char *tmp = NULL, *rets = NULL;

    if (s == NULL || rcfgs == NULL || rcfgslen == NULL)
        return 0;
    es = s->ext.ech.es;
    if (es != NULL && es->entries != NULL)
        num = sk_OSSL_ECHSTORE_ENTRY_num(es->entries);
    for (i = 0; i != num; i++) {
        ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, i);
        if (ee != NULL && ee->for_retry == OSSL_ECH_FOR_RETRY) {
            if (ee->encoded_len > SIZE_MAX - retslen)
                goto err;
            tmp = (unsigned char *)OPENSSL_realloc(rets,
                retslen + ee->encoded_len);
            if (tmp == NULL)
                goto err;
            rets = tmp;
            memcpy(rets + retslen, ee->encoded, ee->encoded_len);
            retslen += ee->encoded_len;
        }
    }
    *rcfgs = rets;
    *rcfgslen = retslen;
    return 1;
err:
    OPENSSL_free(rets);
    *rcfgs = NULL;
    *rcfgslen = 0;
    return 0;
}

/* GREASEy constants */
#define OSSL_ECH_MAX_GREASE_PUB 0x100 /* buffer size for 'enc' values */
#define OSSL_ECH_MAX_GREASE_CT 0x200 /* max GREASEy ciphertext we'll emit */

/*
 * Send a random value that looks like a real ECH.
 *
 * We do GREASEing as follows:
 *   - always HKDF-SHA256
 *   - always AES-128-GCM
 *   - random config ID, even for requests to same server in same session
 *   - random enc
 *   - random looking payload, randomly 144, 176, 208, 240 bytes, no correlation with server
 */
int ossl_ech_send_grease(SSL_CONNECTION *s, WPACKET *pkt)
{
    OSSL_HPKE_SUITE hpke_suite_in = OSSL_HPKE_SUITE_DEFAULT;
    OSSL_HPKE_SUITE *hpke_suite_in_p = NULL;
    OSSL_HPKE_SUITE hpke_suite = OSSL_HPKE_SUITE_DEFAULT;
    size_t pp_at_start = 0, pp_at_end = 0;
    size_t senderpub_len = OSSL_ECH_MAX_GREASE_PUB;
    size_t cipher_len = 0, cipher_len_jitter = 0;
    unsigned char cid, senderpub[OSSL_ECH_MAX_GREASE_PUB];
    unsigned char cipher[OSSL_ECH_MAX_GREASE_CT];
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);

    if (!WPACKET_get_total_written(pkt, &pp_at_start)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    /* randomly select cipher_len to be one of 144, 176, 208, 244 */
    if (RAND_bytes_ex(sctx->libctx, &cid, 1, 0) <= 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    cipher_len_jitter = cid % 4;
    cipher_len = 144;
    cipher_len += 32 * cipher_len_jitter;
    /* generate a random (1 octet) client id */
    if (RAND_bytes_ex(sctx->libctx, &cid, 1, 0) <= 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    s->ext.ech.attempted_cid = cid;
    hpke_suite_in_p = &hpke_suite;
    if (s->ext.ech.grease_suite != NULL) {
        if (OSSL_HPKE_str2suite(s->ext.ech.grease_suite, &hpke_suite_in) != 1) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        hpke_suite_in_p = &hpke_suite_in;
    }
    if (OSSL_HPKE_get_grease_value(hpke_suite_in_p, &hpke_suite,
            senderpub, &senderpub_len,
            cipher, cipher_len,
            sctx->libctx, sctx->propq)
        != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    if (!WPACKET_put_bytes_u16(pkt, s->ext.ech.attempted_type)
        || !WPACKET_start_sub_packet_u16(pkt)
        || !WPACKET_put_bytes_u8(pkt, OSSL_ECH_OUTER_CH_TYPE)
        || !WPACKET_put_bytes_u16(pkt, hpke_suite.kdf_id)
        || !WPACKET_put_bytes_u16(pkt, hpke_suite.aead_id)
        || !WPACKET_put_bytes_u8(pkt, cid)
        || !WPACKET_sub_memcpy_u16(pkt, senderpub, senderpub_len)
        || !WPACKET_sub_memcpy_u16(pkt, cipher, cipher_len)
        || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    /* record the ECH sent so we can re-tx same if we hit an HRR */
    OPENSSL_free(s->ext.ech.sent);
    if (!WPACKET_get_total_written(pkt, &pp_at_end)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    s->ext.ech.sent_len = pp_at_end - pp_at_start;
    s->ext.ech.sent = OPENSSL_malloc(s->ext.ech.sent_len);
    if (s->ext.ech.sent == NULL) {
        s->ext.ech.sent_len = 0;
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    memcpy(s->ext.ech.sent, WPACKET_get_curr(pkt) - s->ext.ech.sent_len,
        s->ext.ech.sent_len);
    s->ext.ech.grease = OSSL_ECH_IS_GREASE;
    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out, "ECH - sending GREASE\n");
    }
    OSSL_TRACE_END(TLS);
    return 1;
}

/*
 * Search the ECH store for one that's a match. If no outer_name was set via
 * API then we just take the 1st match where we locally support the HPKE suite.
 * If OTOH, an outer_name was provided via API then we prefer the first that
 * matches that. Name comparison is via case-insensitive exact matches.
 */
int ossl_ech_pick_matching_cfg(SSL_CONNECTION *s, OSSL_ECHSTORE_ENTRY **ee,
    OSSL_HPKE_SUITE *suite)
{
    int namematch = 0, nameoverride = 0, suitematch = 0, num, cind = 0;
    unsigned int csuite = 0, tsuite = 0;
    size_t hnlen = 0;
    OSSL_ECHSTORE_ENTRY *lee = NULL, *tee = NULL;
    OSSL_ECHSTORE *es = NULL;
    char *hn = NULL;

    if (s == NULL || s->ext.ech.es == NULL || ee == NULL || suite == NULL)
        return 0;
    *ee = NULL;
    es = s->ext.ech.es;
    if (es->entries == NULL)
        return 0;
    num = sk_OSSL_ECHSTORE_ENTRY_num(es->entries);
    /* allow API-set pref to override */
    hn = s->ext.ech.outer_hostname;
    hnlen = (hn == NULL ? 0 : (unsigned int)strlen(hn));
    if (hnlen != 0)
        nameoverride = 1;
    if (s->ext.ech.no_outer == 1) {
        hn = NULL;
        hnlen = 0;
        nameoverride = 1;
    }
    for (cind = 0; cind < num && (suitematch == 0 || namematch == 0); cind++) {
        lee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, cind);
        if (lee == NULL || lee->version != OSSL_ECH_RFC9849_VERSION)
            continue;
        if (nameoverride == 1 && hnlen == 0) {
            namematch = 1;
        } else {
            namematch = 0;
            if (hnlen == 0
                || (lee->public_name != NULL
                    && strlen(lee->public_name) == hnlen
                    && OPENSSL_strncasecmp(hn, (char *)lee->public_name,
                           hnlen)
                        == 0))
                namematch = 1;
        }
        suitematch = 0;
        for (csuite = 0; csuite != lee->nsuites && suitematch == 0; csuite++) {
            if (OSSL_HPKE_suite_check(lee->suites[csuite]) == 1) {
                if (tee == NULL) { /* remember 1st suite match for override */
                    tee = lee;
                    tsuite = csuite;
                }
                suitematch = 1;
                if (namematch == 1) { /* pick this one if both "fit" */
                    *suite = lee->suites[csuite];
                    *ee = lee;
                    break;
                }
            }
        }
    }
    if (tee != NULL && nameoverride == 1
        && (namematch == 0 || suitematch == 0)) {
        *suite = tee->suites[tsuite];
        *ee = tee;
    } else if (namematch == 0 || suitematch == 0) {
        /* no joy */
        return 0;
    }
    if (*ee == NULL || (*ee)->pub_len == 0 || (*ee)->pub == NULL)
        return 0;
    return 1;
}

/* Make up the ClientHelloInner and EncodedClientHelloInner buffers */
int ossl_ech_encode_inner(SSL_CONNECTION *s, unsigned char **encoded,
    size_t *encoded_len)
{
    int rv = 0;
    size_t nraws = 0, ind = 0, innerlen = 0;
    WPACKET inner = { 0 }; /* "fake" pkt for inner */
    BUF_MEM *inner_mem = NULL;
    RAW_EXTENSION *raws = NULL;

    /* basic checks */
    if (s == NULL)
        return 0;
    if (s->ext.ech.es == NULL || s->clienthello == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    if ((inner_mem = BUF_MEM_new()) == NULL
        || !WPACKET_init(&inner, inner_mem)
        /* We don't add the type and 3-octet header as usually done */
        /* Add ver/rnd/sess-id/suites to buffer */
        || !WPACKET_put_bytes_u16(&inner, s->client_version)
        || !WPACKET_memcpy(&inner, s->ext.ech.client_random, SSL3_RANDOM_SIZE)
        /* Session ID is forced to zero in the encoded inner */
        || !WPACKET_sub_memcpy_u8(&inner, NULL, 0)
        /* Ciphers supported */
        || !WPACKET_start_sub_packet_u16(&inner)
        || !ssl_cipher_list_to_bytes(s, SSL_get_ciphers(&s->ssl), &inner)
        || !WPACKET_close(&inner)
        /* COMPRESSION */
        || !WPACKET_start_sub_packet_u8(&inner)
        /* Add the NULL compression method */
        || !WPACKET_put_bytes_u8(&inner, 0)
        || !WPACKET_close(&inner)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* Now handle extensions */
    if (!WPACKET_start_sub_packet_u16(&inner)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* Grab a pointer to the already constructed extensions */
    raws = s->clienthello->pre_proc_exts;
    nraws = s->clienthello->pre_proc_exts_len;
    if (raws == NULL || nraws < TLSEXT_IDX_num_builtins) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /*  We put ECH-compressed stuff first (if any), because we can */
    if (s->ext.ech.n_outer_only > 0) {
        if (!WPACKET_put_bytes_u16(&inner, TLSEXT_TYPE_outer_extensions)
            || !WPACKET_start_sub_packet_u16(&inner)
            /* redundant encoding of more-or-less the same thing */
            || !WPACKET_start_sub_packet_u8(&inner)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        /* add the types for each of the compressed extensions now */
        for (ind = 0; ind != s->ext.ech.n_outer_only; ind++) {
            if (!WPACKET_put_bytes_u16(&inner, s->ext.ech.outer_only[ind])) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }
        }
        /* close the 2 sub-packets with the compressed types */
        if (!WPACKET_close(&inner) || !WPACKET_close(&inner)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    }
    /* now copy the rest, as "proper" exts, into encoded inner */
    for (ind = 0; ind < TLSEXT_IDX_num_builtins; ind++) {
        if (raws[ind].present == 0 || ossl_ech_2bcompressed((int)ind) == 1)
            continue;
        if (!WPACKET_put_bytes_u16(&inner, raws[ind].type)
            || !WPACKET_sub_memcpy_u16(&inner, PACKET_data(&raws[ind].data),
                PACKET_remaining(&raws[ind].data))) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    }
    if (!WPACKET_close(&inner) /* close the encoded inner packet */
        || !WPACKET_get_length(&inner, &innerlen)) { /* len for inner CH */
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    *encoded = (unsigned char *)inner_mem->data;
    inner_mem->data = NULL; /* keep BUF_MEM_free happy */
    *encoded_len = innerlen;
    /* and clean up */
    rv = 1;
err:
    WPACKET_cleanup(&inner);
    BUF_MEM_free(inner_mem);
    return rv;
}

/*
 * Find ECH acceptance signal in a SH
 * hrr is 1 if this is for an HRR, otherwise for SH
 * acbuf is (a preallocated) 8 octet buffer
 * shbuf is a pointer to the SH buffer
 * shlen is the length of the SH buf
 * return: 1 for success, 0 otherwise
 */
int ossl_ech_find_confirm(SSL_CONNECTION *s, int hrr,
    unsigned char acbuf[OSSL_ECH_SIGNAL_LEN])
{
    unsigned char *acp = NULL;

    if (hrr == 0) {
        acp = s->s3.server_random + SSL3_RANDOM_SIZE - OSSL_ECH_SIGNAL_LEN;
    } else { /* was set in extension handler */
        if (s->ext.ech.hrrsignal_p == NULL)
            return 0;
        acp = s->ext.ech.hrrsignal;
    }
    memcpy(acbuf, acp, OSSL_ECH_SIGNAL_LEN);
    return 1;
}

/*
 * reset the handshake buffer for transcript after ECH is good
 * buf is the data to put into the transcript (inner CH if no HRR)
 * blen is the length of buf
 * return 1 for success
 */
int ossl_ech_reset_hs_buffer(SSL_CONNECTION *s, const unsigned char *buf,
    size_t blen)
{
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("RESET transcript to", buf, blen);
#endif
    if (s->s3.handshake_buffer != NULL) {
        if (BIO_reset(s->s3.handshake_buffer) < 0)
            return 0;
    } else {
        s->s3.handshake_buffer = BIO_new(BIO_s_mem());
        if (s->s3.handshake_buffer == NULL)
            return 0;
        (void)BIO_set_close(s->s3.handshake_buffer, BIO_CLOSE);
    }
    EVP_MD_CTX_free(s->s3.handshake_dgst);
    s->s3.handshake_dgst = NULL;
    /* providing nothing at all is a real use (mid-HRR) */
    if (buf != NULL && blen > 0)
        BIO_write(s->s3.handshake_buffer, (void *)buf, (int)blen);
    return 1;
}

/*
 * To control the number of zeros added after an EncodedClientHello - we pad
 * to a target number of octets or, if there are naturally more, to a number
 * divisible by the defined increment (we also do the spec-recommended SNI
 * padding thing first)
 */
#define OSSL_ECH_PADDING_TARGET 128 /* ECH cleartext padded to at least this */
#define OSSL_ECH_PADDING_INCREMENT 32 /* ECH padded to a multiple of this */

/*
 * figure out how much padding for cleartext (on client)
 * ee is the chosen ECHConfig
 * return overall length to use including padding or zero on error
 *
 * "Recommended" inner SNI padding scheme as per spec (section 6.1.3)
 * Might remove the mnl stuff later - overall message padding seems
 * better really, BUT... we might want to keep this if others (e.g.
 * browsers) do it so as to not stand out compared to them.
 *
 * The "+ 9" constant below is from the specification and is the
 * expansion comparing a string length to an encoded SNI extension.
 * Same is true of the 31/32 formula below.
 *
 * Note that the AEAD tag will be added later, so if we e.g. have
 * a padded cleartext of 128 octets, the ciphertext will be 144
 * octets.
 */
size_t ossl_ech_calc_padding(SSL_CONNECTION *s, OSSL_ECHSTORE_ENTRY *ee,
    size_t encoded_len)
{
    size_t length_of_padding = 0, length_with_snipadding = 0;
    size_t innersnipadding = 0, length_with_padding = 0;
    size_t mnl = 0, isnilen = 0;

    if (s == NULL || ee == NULL)
        return 0;
    mnl = ee->max_name_length;
    if (mnl != 0) {
        /* do weirder padding if SNI present in inner */
        if (s->ext.hostname != NULL) {
            isnilen = strlen(s->ext.hostname) + 9;
            innersnipadding = (mnl > isnilen) ? (int)(mnl - isnilen) : 0;
        } else {
            innersnipadding = (int)mnl + 9;
        }
    }
    /* padding is after the inner client hello has been encoded */
    length_with_snipadding = innersnipadding + (int)encoded_len;
    length_of_padding = 31 - ((length_with_snipadding - 1) % 32);
    length_with_padding = (int)encoded_len + length_of_padding
        + innersnipadding;
    /*
     * Finally - make sure final result is longer than padding target
     * and a multiple of our padding increment.
     * This is a local addition - we might want to take it out if it makes
     * us stick out; or if we take out the above more (uselessly:-)
     * complicated scheme above, we may only need this in the end.
     */
    if ((length_with_padding % OSSL_ECH_PADDING_INCREMENT) != 0)
        length_with_padding += OSSL_ECH_PADDING_INCREMENT
            - (length_with_padding % OSSL_ECH_PADDING_INCREMENT);
    while (length_with_padding < OSSL_ECH_PADDING_TARGET)
        length_with_padding += OSSL_ECH_PADDING_INCREMENT;
    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out, "EAAE: padding: mnl: %zu, lws: %zu "
                            "lop: %zu, clear_len (len with padding): %zu, orig: %zu\n",
            mnl, length_with_snipadding, length_of_padding,
            length_with_padding, encoded_len);
    }
    OSSL_TRACE_END(TLS);
    return length_with_padding;
}

/*
 * Calculate AAD and do ECH encryption
 * pkt is the packet to send
 * return 1 for success, other otherwise
 *
 * 1. Make up the AAD: the encoded outer, with ECH ciphertext octets zero'd
 * 2. Do the encryption
 * 3. Put the ECH back into the encoding
 * 4. Encode the outer (again!)
 */
int ossl_ech_aad_and_encrypt(SSL_CONNECTION *s, WPACKET *pkt)
{
    int rv = 0;
    size_t cipherlen = 0, aad_len = 0, mypub_len = 0, clear_len = 0;
    size_t encoded_inner_len = 0;
    unsigned char *clear = NULL, *aad = NULL, *mypub = NULL;
    unsigned char *encoded_inner = NULL, *cipher_loc = NULL;

    if (s == NULL)
        return 0;
    if (s->ext.ech.es == NULL || s->ext.ech.es->entries == NULL
        || pkt == NULL || s->ssl.ctx == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* values calculated in tls_construct_ctos_ech */
    encoded_inner = s->ext.ech.encoded_inner;
    encoded_inner_len = s->ext.ech.encoded_inner_len;
    clear_len = s->ext.ech.clearlen;
    cipherlen = s->ext.ech.cipherlen;
    if (!WPACKET_get_total_written(pkt, &aad_len) || aad_len < 4) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    aad_len -= 4; /* ECH/HPKE aad starts after type + 3-octet len */
    aad = WPACKET_get_curr(pkt) - aad_len;
    /* where we'll replace zeros with ciphertext */
    cipher_loc = aad + s->ext.ech.cipher_offset;
    /*
     * close the extensions of the CH - we skipped doing this
     * earlier when encoding extensions, to allow for adding the
     * ECH here (when doing ECH) - see tls_construct_extensions()
     * towards the end
     */
    if (!WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("EAAE: aad", aad, aad_len);
#endif
    clear = OPENSSL_zalloc(clear_len); /* zeros incl. padding */
    if (clear == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    memcpy(clear, encoded_inner, encoded_inner_len);
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("EAAE: padded clear", clear, clear_len);
#endif
    /* we're done with this now */
    OPENSSL_free(s->ext.ech.encoded_inner);
    s->ext.ech.encoded_inner = NULL;
    rv = OSSL_HPKE_seal(s->ext.ech.hpke_ctx, cipher_loc,
        &cipherlen, aad, aad_len, clear, clear_len);
    OPENSSL_free(clear);
    if (rv != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("EAAE: cipher", cipher_loc, cipherlen);
    ossl_ech_pbuf("EAAE: hpke mypub", mypub, mypub_len);
    /* re-use aad_len for tracing */
    WPACKET_get_total_written(pkt, &aad_len);
    ossl_ech_pbuf("EAAE pkt aftr", WPACKET_get_curr(pkt) - aad_len, aad_len);
#endif
    return 1;
err:
    return 0;
}

/*
 * print info about the ECH-status of an SSL connection
 * out is the BIO to use (e.g. stdout/whatever)
 * selector OSSL_ECH_SELECT_ALL or just one of the SSL_ECH values
 */
void ossl_ech_status_print(BIO *out, SSL_CONNECTION *s, int selector)
{
    int num = 0, i, has_priv, for_retry;
    size_t j;
    time_t secs = 0;
    char *pn = NULL, *ec = NULL;
    OSSL_ECHSTORE *es = NULL;

#ifdef OSSL_ECH_SUPERVERBOSE
    BIO_printf(out, "ech_status_print\n");
    BIO_printf(out, "s=%p\n", (void *)s);
#endif
    BIO_printf(out, "ech_attempted=%d\n", s->ext.ech.attempted);
    BIO_printf(out, "ech_attempted_type=0x%4x\n",
        s->ext.ech.attempted_type);
    if (s->ext.ech.attempted_cid == OSSL_ECH_config_id_unset)
        BIO_printf(out, "ech_atttempted_cid is unset\n");
    else
        BIO_printf(out, "ech_atttempted_cid=0x%02x\n",
            s->ext.ech.attempted_cid);
    BIO_printf(out, "ech_done=%d\n", s->ext.ech.done);
    BIO_printf(out, "ech_grease=%d\n", s->ext.ech.grease);
#ifdef OSSL_ECH_SUPERVERBOSE
    BIO_printf(out, "HRR=%d\n", s->hello_retry_request);
#endif
    BIO_printf(out, "ech_backend=%d\n", s->ext.ech.backend);
    BIO_printf(out, "ech_success=%d\n", s->ext.ech.success);
    es = s->ext.ech.es;
    if (es == NULL || es->entries == NULL) {
        BIO_printf(out, "ECH cfg=NONE\n");
    } else {
        num = sk_OSSL_ECHSTORE_ENTRY_num(es->entries);
        BIO_printf(out, "%d ECHConfig values loaded\n", num);
        for (i = 0; i != num; i++) {
            if (selector != OSSL_ECHSTORE_ALL && selector != i)
                continue;
            BIO_printf(out, "cfg(%d): ", i);
            if (OSSL_ECHSTORE_get1_info(es, i, &secs, &pn, &ec,
                    &has_priv, &for_retry)
                != 1) {
                OPENSSL_free(pn); /* just in case */
                OPENSSL_free(ec);
                continue;
            }
            BIO_printf(out, "ECH entry: %d public_name: %s age: %lld%s\n",
                i, pn, (long long)secs, has_priv ? " (has private key)" : "");
            BIO_printf(out, "\t%s\n", ec);
            OPENSSL_free(pn);
            OPENSSL_free(ec);
        }
    }
    if (s->ext.ech.returned) {
        BIO_printf(out, "ret=");
        for (j = 0; j != s->ext.ech.returned_len; j++) {
            if (j != 0 && j % 16 == 0)
                BIO_printf(out, "\n    ");
            BIO_printf(out, "%02x:", (unsigned)(s->ext.ech.returned[j]));
        }
        BIO_printf(out, "\n");
    }
    return;
}

/*
 * Swap the inner and outer after ECH success on the client
 * return 0 for error, 1 for success
 */
int ossl_ech_swaperoo(SSL_CONNECTION *s)
{
    unsigned char *curr_buf = NULL;
    size_t curr_buflen = 0;

    if (s == NULL)
        return 0;
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_ptranscript(s, "ech_swaperoo, b4");
#endif
    /* un-stash inner key share(s) */
    if (ossl_ech_unstash_keyshares(s) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    /*
     * When not doing HRR... fix up the transcript to reflect the inner CH.
     * If there's a client hello at the start of the buffer, then that's
     * the outer CH and we want to replace that with the inner. We need to
     * be careful that there could be early data or a server hello following
     * and we can't lose that.
     *
     * For HRR... HRR processing code has already done the necessary.
     */
    if (s->hello_retry_request == SSL_HRR_NONE) {
        BIO *handbuf = s->s3.handshake_buffer;
        PACKET pkt, subpkt;
        unsigned int mt;

        s->s3.handshake_buffer = NULL;
        if (ssl3_init_finished_mac(s) == 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            BIO_free(handbuf);
            return 0;
        }
        if (ssl3_finish_mac(s, s->ext.ech.innerch, s->ext.ech.innerch_len) == 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            BIO_free(handbuf);
            return 0;
        }
        curr_buflen = BIO_get_mem_data(handbuf, &curr_buf);
        if (PACKET_buf_init(&pkt, curr_buf, curr_buflen)
            && PACKET_get_1(&pkt, &mt)
            && mt == SSL3_MT_CLIENT_HELLO
            && PACKET_remaining(&pkt) >= 3) {
            if (!PACKET_get_length_prefixed_3(&pkt, &subpkt)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                BIO_free(handbuf);
                return 0;
            }
            if (PACKET_remaining(&pkt) > 0) {
                if (ssl3_finish_mac(s, PACKET_data(&pkt), PACKET_remaining(&pkt)) == 0) {
                    SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    BIO_free(handbuf);
                    return 0;
                }
            }
            BIO_free(handbuf);
        }
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_ptranscript(s, "ech_swaperoo, after");
#endif
    /* Declare victory! */
    s->ext.ech.attempted = 1;
    s->ext.ech.success = 1;
    s->ext.ech.done = 1;
    s->ext.ech.grease = OSSL_ECH_NOT_GREASE;
    /* time to call an ECH callback, if there's one */
    if (s->ext.ech.es != NULL && s->ext.ech.done == 1
        && s->hello_retry_request != SSL_HRR_PENDING
        && s->ext.ech.cb != NULL) {
        char pstr[OSSL_ECH_PBUF_SIZE + 1] = { 0 };
        BIO *biom = BIO_new(BIO_s_mem());
        unsigned int cbrv = 0;

        if (biom == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        ossl_ech_status_print(biom, s, OSSL_ECHSTORE_ALL);
        BIO_read(biom, pstr, OSSL_ECH_PBUF_SIZE);
        cbrv = s->ext.ech.cb(&s->ssl, pstr);
        BIO_free(biom);
        if (cbrv != 1) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            return 0;
        }
    }
    return 1;
}

/*
 * do the HKDF for ECH acceptance checking
 * md is the h/s hash
 * for_hrr is 1 if we're doing a HRR
 * hashval/hashlen is the transcript hash
 * hoval is the output, with the ECH acceptance signal
 * return 1 for good, 0 for error
 */
static int ech_hkdf_extract_wrap(SSL_CONNECTION *s, EVP_MD *md, int for_hrr,
    unsigned char *hashval, size_t hashlen,
    unsigned char hoval[OSSL_ECH_SIGNAL_LEN])
{
    int rv = 0;
    unsigned char notsecret[EVP_MAX_MD_SIZE], zeros[EVP_MAX_MD_SIZE];
    size_t retlen = 0, labellen = 0;
    EVP_PKEY_CTX *pctx = NULL;
    const char *label = NULL;
    unsigned char *p = NULL;
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);

    if (for_hrr == 1) {
        label = OSSL_ECH_HRR_CONFIRM_STRING;
        labellen = sizeof(OSSL_ECH_HRR_CONFIRM_STRING) - 1;
    } else {
        label = OSSL_ECH_ACCEPT_CONFIRM_STRING;
        labellen = sizeof(OSSL_ECH_ACCEPT_CONFIRM_STRING) - 1;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("cc: label", (unsigned char *)label, labellen);
#endif
    memset(zeros, 0, EVP_MAX_MD_SIZE);
    /* We don't seem to have an hkdf-extract that's exposed by libcrypto */
    pctx = EVP_PKEY_CTX_new_from_name(sctx->libctx, "HKDF", sctx->propq);
    if (pctx == NULL
        || EVP_PKEY_derive_init(pctx) != 1
        || EVP_PKEY_CTX_hkdf_mode(pctx,
               EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY)
            != 1
        || EVP_PKEY_CTX_set_hkdf_md(pctx, md) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* pick correct client_random */
    if (s->server)
        p = s->s3.client_random;
    else
        p = s->ext.ech.client_random;
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("cc: client_random", p, SSL3_RANDOM_SIZE);
#endif
    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, p, SSL3_RANDOM_SIZE) != 1
        || EVP_PKEY_CTX_set1_hkdf_salt(pctx, zeros, (int)hashlen) != 1
        || EVP_PKEY_derive(pctx, NULL, &retlen) != 1
        || hashlen != retlen
        || EVP_PKEY_derive(pctx, notsecret, &retlen) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("cc: notsecret", notsecret, hashlen);
#endif
    if (hashlen < OSSL_ECH_SIGNAL_LEN
        || !tls13_hkdf_expand(s, md, notsecret,
            (const unsigned char *)label, labellen,
            hashval, hashlen, hoval,
            OSSL_ECH_SIGNAL_LEN, 1)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    rv = 1;
err:
    EVP_PKEY_CTX_free(pctx);
    return rv;
}

/*
 * ECH accept_confirmation calculation
 * for_hrr is 1 if this is for an HRR, otherwise for SH
 * acbuf is an 8 octet buffer for the confirmation value
 * shlen is the server hello length
 * return: 1 for success, 0 otherwise
 *
 * This is a magic value in the ServerHello.random lower 8 octets
 * that is used to signal that the inner worked.
 *
 * As per spec:
 *
 * accept_confirmation = HKDF-Expand-Label(
 *         HKDF-Extract(0, ClientHelloInner.random),
 *         "ech accept confirmation",
 *         transcript_ech_conf,
 *         8)
 *
 * transcript_ech_conf = ClientHelloInner..ServerHello
 *         with last 8 octets of ServerHello.random==0x00
 *
 * and with differences due to HRR
 */
int ossl_ech_calc_confirm(SSL_CONNECTION *s, int for_hrr,
    unsigned char acbuf[OSSL_ECH_SIGNAL_LEN],
    const size_t shlen)
{
    int rv = 0;
    EVP_MD_CTX *ctx = NULL;
    EVP_MD *md = NULL;
    unsigned char *tbuf = NULL, *conf_loc = NULL;
    unsigned char *fixedshbuf = NULL;
    size_t fixedshbuf_len = 0, tlen = 0, chend = 0;
    /* shoffset is: 4 + 2 + 32 - 8 */
    size_t shoffset = SSL3_HM_HEADER_LENGTH + sizeof(uint16_t)
        + SSL3_RANDOM_SIZE - OSSL_ECH_SIGNAL_LEN;
    unsigned int hashlen = 0;
    unsigned char hashval[EVP_MAX_MD_SIZE];
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);

    if ((md = (EVP_MD *)ssl_handshake_md(s)) == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_ECH_REQUIRED);
        goto end;
    }
    if (ossl_ech_intbuf_fetch(s, &tbuf, &tlen) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_ECH_REQUIRED);
        goto end;
    }
    chend = tlen - shlen - 4;
    fixedshbuf_len = shlen + 4;
    if (s->server) {
        chend = tlen - shlen;
        fixedshbuf_len = shlen;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("cx: tbuf b4-b4", tbuf, tlen);
#endif
    /* put zeros in correct place */
    if (for_hrr == 0) { /* zap magic octets at fixed place for SH */
        conf_loc = tbuf + chend + shoffset;
    } else {
        if (s->server == 0 && s->ext.ech.hrrsignal_p == NULL) {
            /* No ECH found so we'll exit, but set random output */
            if (RAND_bytes_ex(sctx->libctx, acbuf, OSSL_ECH_SIGNAL_LEN, 0)
                <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_ECH_REQUIRED);
                goto end;
            }
            rv = 1;
            goto end;
        }
        conf_loc = tbuf + tlen - OSSL_ECH_SIGNAL_LEN;
    }
    memset(conf_loc, 0, OSSL_ECH_SIGNAL_LEN);
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("cx: tbuf after", tbuf, tlen);
#endif
    if ((ctx = EVP_MD_CTX_new()) == NULL
        || EVP_DigestInit_ex(ctx, md, NULL) <= 0
        || EVP_DigestUpdate(ctx, tbuf, tlen) <= 0
        || EVP_DigestFinal_ex(ctx, hashval, &hashlen) <= 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto end;
    }
    EVP_MD_CTX_free(ctx);
    ctx = NULL;
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("cx: hashval", hashval, hashlen);
#endif
    /* calculate and set the final output */
    if (ech_hkdf_extract_wrap(s, md, for_hrr, hashval, hashlen, acbuf) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto end;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("cx: result", acbuf, OSSL_ECH_SIGNAL_LEN);
#endif
    /* put confirm value back into transcript */
    if (s->server == 0 && for_hrr == 1) { /* put back the one we got */
        memcpy(conf_loc, s->ext.ech.hrrsignal, OSSL_ECH_SIGNAL_LEN);
    } else {
        memcpy(conf_loc, acbuf, OSSL_ECH_SIGNAL_LEN);
    }
    /* on a server, we need to reset the hs buffer now */
    if (s->server && s->hello_retry_request == SSL_HRR_NONE)
        ossl_ech_reset_hs_buffer(s, s->ext.ech.innerch, s->ext.ech.innerch_len);
    if (s->server && s->hello_retry_request == SSL_HRR_COMPLETE)
        ossl_ech_reset_hs_buffer(s, tbuf, tlen - fixedshbuf_len);
    rv = 1;
end:
    OPENSSL_free(fixedshbuf);
    EVP_MD_CTX_free(ctx);
    return rv;
}

/*!
 * Given a CH find the offsets of the session id, extensions and ECH
 * pkt is the CH
 * sessid_off points to offset of session_id length
 * exts_off points to offset of extensions
 * ech_off points to offset of ECH
 * echtype points to the ext type of the ECH
 * inner 1 if the ECH is marked as an inner, 0 for outer
 * sni_off points to offset of (outer) SNI
 * return 1 for success, other otherwise
 *
 * Offsets are set to zero if relevant thing not found.
 * Offsets are returned to the type or length field in question.
 *
 * Note: input here is untrusted!
 */
int ossl_ech_get_ch_offsets(SSL_CONNECTION *s, PACKET *pkt, size_t *sessid_off,
    size_t *exts_off, size_t *ech_off, uint16_t *echtype,
    int *inner, size_t *sni_off)
{
    const unsigned char *ch = NULL;
    size_t ch_len = 0, exts_len = 0, sni_len = 0, ech_len = 0;

    if (s == NULL)
        return 0;
    if (pkt == NULL || sessid_off == NULL || exts_off == NULL
        || ech_off == NULL || echtype == NULL || inner == NULL
        || sni_off == NULL) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        return 0;
    }
    /* check if we've already done the work */
    if (s->ext.ech.ch_offsets_done == 1) {
        *sessid_off = s->ext.ech.sessid_off;
        *exts_off = s->ext.ech.exts_off;
        *ech_off = s->ext.ech.ech_off;
        *echtype = s->ext.ech.echtype;
        *inner = s->ext.ech.inner;
        *sni_off = s->ext.ech.sni_off;
        return 1;
    }
    *sessid_off = 0;
    *exts_off = 0;
    *ech_off = 0;
    *echtype = OSSL_ECH_type_unknown;
    *sni_off = 0;
    /* do the work */
    ch_len = PACKET_remaining(pkt);
    if (PACKET_peek_bytes(pkt, &ch, ch_len) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    if (ossl_ech_helper_get_ch_offsets(ch, ch_len, sessid_off, exts_off,
            &exts_len, ech_off, echtype, &ech_len,
            sni_off, &sni_len, inner)
        != 1) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        return 0;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out, "orig CH/ECH type: %4x\n", *echtype);
    }
    OSSL_TRACE_END(TLS);
    ossl_ech_pbuf("orig CH", (unsigned char *)ch, ch_len);
    ossl_ech_pbuf("orig CH exts", (unsigned char *)ch + *exts_off, exts_len);
    ossl_ech_pbuf("orig CH/ECH", (unsigned char *)ch + *ech_off, ech_len);
    ossl_ech_pbuf("orig CH SNI", (unsigned char *)ch + *sni_off, sni_len);
#endif
    s->ext.ech.sessid_off = *sessid_off;
    s->ext.ech.exts_off = *exts_off;
    s->ext.ech.ech_off = *ech_off;
    s->ext.ech.echtype = *echtype;
    s->ext.ech.inner = *inner;
    s->ext.ech.sni_off = *sni_off;
    s->ext.ech.ch_offsets_done = 1;
    return 1;
}

static void ossl_ech_encch_free(OSSL_ECH_ENCCH *tbf)
{
    if (tbf == NULL)
        return;
    OPENSSL_free(tbf->enc);
    OPENSSL_free(tbf->payload);
    return;
}

/*
 * decode outer sni value so we can trace it
 * osni_str is the string-form of the SNI
 * opd is the outer CH buffer
 * opl is the length of the above
 * snioffset is where we find the outer SNI
 *
 * The caller doesn't have to free the osni_str.
 */
static int ech_get_outer_sni(SSL_CONNECTION *s, char **osni_str,
    const unsigned char *opd, size_t opl,
    size_t snioffset)
{
    PACKET wrap, osni;
    unsigned int type, osnilen;

    if (snioffset >= opl
        || !PACKET_buf_init(&wrap, opd + snioffset, opl - snioffset)
        || !PACKET_get_net_2(&wrap, &type)
        || type != 0
        || !PACKET_get_net_2(&wrap, &osnilen)
        || !PACKET_get_sub_packet(&wrap, &osni, osnilen)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    if (tls_parse_ctos_server_name(s, &osni, 0, NULL, 0) != 1)
        /* SSLfatal called already */
        return 0;
    OPENSSL_free(s->ext.ech.outer_hostname);
    *osni_str = s->ext.ech.outer_hostname = s->ext.hostname;
    /* clean up what the ECH-unaware parse func above left behind */
    s->ext.hostname = NULL;
    s->servername_done = 0;
    return 1;
}

/*
 * decode EncryptedClientHello extension value
 * pkt contains the ECH value as a PACKET
 * retext is the returned decoded structure
 * payload_offset is the offset to the ciphertext
 * return 1 for good, 0 for bad
 *
 * SSLfatal called from inside, as needed
 */
static int ech_decode_inbound_ech(SSL_CONNECTION *s, PACKET *pkt,
    OSSL_ECH_ENCCH **retext,
    size_t *payload_offset)
{
    unsigned int innerorouter = 0xff;
    unsigned int pval_tmp; /* tmp placeholder of value from packet */
    OSSL_ECH_ENCCH *extval = NULL;
    const unsigned char *startofech = NULL;

    /*
     * Decode the inbound ECH value.
     *  enum { outer(0), inner(1) } ECHClientHelloType;
     *  struct {
     *     ECHClientHelloType type;
     *     select (ECHClientHello.type) {
     *         case outer:
     *             HpkeSymmetricCipherSuite cipher_suite;
     *             uint8 config_id;
     *             opaque enc<0..2^16-1>;
     *             opaque payload<1..2^16-1>;
     *         case inner:
     *             Empty;
     *     };
     *  } ECHClientHello;
     */
    startofech = PACKET_data(pkt);
    extval = OPENSSL_zalloc(sizeof(OSSL_ECH_ENCCH));
    if (extval == NULL)
        goto err;
    if (!PACKET_get_1(pkt, &innerorouter)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    if (innerorouter != OSSL_ECH_OUTER_CH_TYPE) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        goto err;
    }
    if (!PACKET_get_net_2(pkt, &pval_tmp)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    extval->kdf_id = pval_tmp & 0xffff;
    if (!PACKET_get_net_2(pkt, &pval_tmp)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    extval->aead_id = pval_tmp & 0xffff;
    /* config id */
    if (!PACKET_copy_bytes(pkt, &extval->config_id, 1)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("EARLY config id", &extval->config_id, 1);
#endif
    s->ext.ech.attempted_cid = extval->config_id;
    /* enc - the client's public share */
    if (!PACKET_get_net_2(pkt, &pval_tmp)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    if (pval_tmp > OSSL_ECH_MAX_GREASE_PUB) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        goto err;
    }
    if (pval_tmp > PACKET_remaining(pkt)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    if (pval_tmp == 0 && s->hello_retry_request != SSL_HRR_PENDING) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        goto err;
    } else if (pval_tmp > 0 && s->hello_retry_request == SSL_HRR_PENDING) {
        unsigned char *tmpenc = NULL;

        /*
         * if doing HRR, client should only send this when GREASEing
         * and it should be the same value as 1st time, so we'll check
         * that
         */
        if (s->ext.ech.success == 1) {
            /* first decrypt worked, so enc should be empty */
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
            goto err;
        }
        if (s->ext.ech.pub == NULL || s->ext.ech.pub_len == 0) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
            goto err;
        }
        if (pval_tmp != s->ext.ech.pub_len) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
            goto err;
        }
        tmpenc = OPENSSL_malloc(pval_tmp);
        if (tmpenc == NULL)
            goto err;
        if (!PACKET_copy_bytes(pkt, tmpenc, pval_tmp)) {
            OPENSSL_free(tmpenc);
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            goto err;
        }
        if (memcmp(tmpenc, s->ext.ech.pub, pval_tmp) != 0) {
            OPENSSL_free(tmpenc);
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
            goto err;
        }
        OPENSSL_free(tmpenc);
    } else if (pval_tmp == 0 && s->hello_retry_request == SSL_HRR_PENDING) {
        if (s->ext.ech.pub == NULL || s->ext.ech.pub_len == 0) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
            goto err;
        }
        extval->enc_len = s->ext.ech.pub_len;
        extval->enc = OPENSSL_malloc(extval->enc_len);
        if (extval->enc == NULL)
            goto err;
        memcpy(extval->enc, s->ext.ech.pub, extval->enc_len);
    } else {
        extval->enc_len = pval_tmp;
        extval->enc = OPENSSL_malloc(pval_tmp);
        if (extval->enc == NULL)
            goto err;
        if (!PACKET_copy_bytes(pkt, extval->enc, pval_tmp)) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            goto err;
        }
        /* squirrel away that value in case of future HRR */
        OPENSSL_free(s->ext.ech.pub);
        s->ext.ech.pub_len = extval->enc_len;
        s->ext.ech.pub = OPENSSL_malloc(extval->enc_len);
        if (s->ext.ech.pub == NULL)
            goto err;
        memcpy(s->ext.ech.pub, extval->enc, extval->enc_len);
    }
    /* payload - the encrypted CH */
    *payload_offset = PACKET_data(pkt) - startofech;
    if (!PACKET_get_net_2(pkt, &pval_tmp)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    if (pval_tmp > OSSL_ECH_MAX_PAYLOAD_LEN) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        goto err;
    }
    if (pval_tmp == 0 || pval_tmp > PACKET_remaining(pkt)) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        goto err;
    }
    extval->payload_len = pval_tmp;
    extval->payload = OPENSSL_malloc(pval_tmp);
    if (extval->payload == NULL)
        goto err;
    if (!PACKET_copy_bytes(pkt, extval->payload, pval_tmp)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    *retext = extval;
    return 1;
err:
    if (extval != NULL) {
        ossl_ech_encch_free(extval);
        OPENSSL_free(extval);
        extval = NULL;
    }
    return 0;
}

/*
 * find outers if any, and do initial checks
 * pkt is the encoded inner
 * outers is the array of outer ext types
 * n_outers is the number of outers found
 * return 1 for good, 0 for error
 *
 * recall we're dealing with recovered ECH plaintext here so
 * the content must be a TLSv1.3 ECH encoded inner
 */
static int ech_find_outers(SSL_CONNECTION *s, PACKET *pkt,
    uint16_t *outers, size_t *n_outers)
{
    const unsigned char *pp_tmp;
    unsigned int pi_tmp, extlens, etype, elen, olen;
    int outers_found = 0;
    size_t i;
    PACKET op;

    PACKET_null_init(&op);
    /* chew up the packet to extensions */
    if (!PACKET_get_net_2(pkt, &pi_tmp)
        || pi_tmp != TLS1_2_VERSION
        || !PACKET_get_bytes(pkt, &pp_tmp, SSL3_RANDOM_SIZE)
        || !PACKET_get_1(pkt, &pi_tmp)
        || pi_tmp != 0x00 /* zero'd session id */
        || !PACKET_get_net_2(pkt, &pi_tmp) /* ciphersuite len */
        || !PACKET_get_bytes(pkt, &pp_tmp, pi_tmp) /* suites */
        || !PACKET_get_1(pkt, &pi_tmp) /* compression meths */
        || pi_tmp != 0x01 /* 1 octet of comressions */
        || !PACKET_get_1(pkt, &pi_tmp) /* compression meths */
        || pi_tmp != 0x00 /* 1 octet of no comressions */
        || !PACKET_get_net_2(pkt, &extlens) /* len(extensions) */
        || extlens == 0) { /* no extensions! */
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    while (PACKET_remaining(pkt) > 0 && outers_found == 0) {
        if (!PACKET_get_net_2(pkt, &etype)) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            goto err;
        }
        if (etype == TLSEXT_TYPE_outer_extensions) {
            outers_found = 1;
            if (!PACKET_get_length_prefixed_2(pkt, &op)) {
                SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
                goto err;
            }
        } else { /* skip over */
            if (!PACKET_get_net_2(pkt, &elen)
                || !PACKET_get_bytes(pkt, &pp_tmp, elen)) {
                SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
                goto err;
            }
        }
    }

    if (outers_found == 0) { /* which is fine! */
        *n_outers = 0;
        return 1;
    }
    /*
     * outers has a silly internal length as well and that better
     * be one less than the extension length and an even number
     * and we only support a certain max of outers
     */
    if (!PACKET_get_1(&op, &olen)
        || olen % 2 == 1
        || olen / 2 > OSSL_ECH_OUTERS_MAX) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        goto err;
    }
    *n_outers = olen / 2;
    for (i = 0; i != *n_outers; i++) {
        /* check for ones that are not allowed */
        if (!PACKET_get_net_2(&op, &pi_tmp)
            || pi_tmp == TLSEXT_TYPE_outer_extensions
            || pi_tmp == TLSEXT_TYPE_ech) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
            goto err;
        }
        outers[i] = (uint16_t)pi_tmp;
    }
    return 1;
err:
    return 0;
}

/*
 * copy one extension from outer to inner
 * di is the reconstituted inner CH
 * type2copy is the outer type to copy
 * exts is the outer extensions packet (changing as we go)
 * return 1 for good 0 for error
 */
static int ech_copy_ext(SSL_CONNECTION *s, WPACKET *di, uint16_t type2copy,
    PACKET *exts)
{
    unsigned int etype, elen;
    const unsigned char *eval;

    /* Skip until we find the thing to copy */
    while (PACKET_remaining(exts) > 0) {
        if (!PACKET_get_net_2(exts, &etype)
            || !PACKET_get_net_2(exts, &elen)
            || !PACKET_get_bytes(exts, &eval, elen)) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            goto err;
        }
        if (etype == type2copy) {
            if (!WPACKET_put_bytes_u16(di, etype)
                || !WPACKET_put_bytes_u16(di, elen)
                || !WPACKET_memcpy(di, eval, elen)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_EXTENSION);
                goto err;
            }
            return 1;
        }
    }
    /* we didn't find such an extension - that's an error */
    SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
err:
    return 0;
}

/*
 * reconstitute the inner CH from encoded inner and outers
 * di is the reconstituted inner CH
 * ei is the encoded inner
 * ob is the outer CH as a buffer
 * ob_len is the size of the above
 * outers is the array of outer ext types
 * n_outers is the number of outers found
 * return 1 for good, 0 for error
 */
static int ech_reconstitute_inner(SSL_CONNECTION *s, WPACKET *di, PACKET *ei,
    const unsigned char *ob, size_t ob_len,
    uint16_t *outers, size_t n_outers)
{
    const unsigned char *pp_tmp, *eval, *outer_exts;
    unsigned int pi_tmp, etype, elen, outer_extslen;
    PACKET outer, session_id;
    size_t i;
    int outers_done = 0;

    if (PACKET_buf_init(&outer, ob, ob_len) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* read/write from encoded inner to decoded inner with help from outer */
    if (/* version */
        !PACKET_get_net_2(&outer, &pi_tmp)
        || !PACKET_get_net_2(ei, &pi_tmp)
        || !WPACKET_put_bytes_u16(di, pi_tmp)
        || pi_tmp != TLS1_2_VERSION

        /* client random */
        || !PACKET_get_bytes(&outer, &pp_tmp, SSL3_RANDOM_SIZE)
        || !PACKET_get_bytes(ei, &pp_tmp, SSL3_RANDOM_SIZE)
        || !WPACKET_memcpy(di, pp_tmp, SSL3_RANDOM_SIZE)

        /* session ID */
        || !PACKET_get_1(ei, &pi_tmp)
        || !PACKET_get_length_prefixed_1(&outer, &session_id)
        || !WPACKET_start_sub_packet_u8(di)
        || (PACKET_remaining(&session_id) != 0
            && !WPACKET_memcpy(di, PACKET_data(&session_id),
                PACKET_remaining(&session_id)))
        || !WPACKET_close(di)

        /* ciphersuites */
        || !PACKET_get_net_2(&outer, &pi_tmp) /* ciphersuite len */
        || !PACKET_get_bytes(&outer, &pp_tmp, pi_tmp) /* suites */
        || !PACKET_get_net_2(ei, &pi_tmp) /* ciphersuite len */
        || !PACKET_get_bytes(ei, &pp_tmp, pi_tmp) /* suites */
        || !WPACKET_put_bytes_u16(di, pi_tmp)
        || !WPACKET_memcpy(di, pp_tmp, pi_tmp)

        /* compression len & meth */
        || !PACKET_get_net_2(ei, &pi_tmp)
        || !PACKET_get_net_2(&outer, &pi_tmp)
        || !WPACKET_put_bytes_u16(di, pi_tmp)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    /* handle simple, but unlikely, case first */
    if (n_outers == 0) {
        if (PACKET_remaining(ei) == 0)
            return 1; /* no exts is theoretically possible */
        if (!PACKET_get_net_2(ei, &pi_tmp) /* len(extensions) */
            || !PACKET_get_bytes(ei, &pp_tmp, pi_tmp)
            || !WPACKET_put_bytes_u16(di, pi_tmp)
            || !WPACKET_memcpy(di, pp_tmp, pi_tmp)) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            goto err;
        }
        WPACKET_close(di);
        return 1;
    }
    /*
     * general case, copy one by one from inner, 'till we hit
     * the outers extension, then copy one by one from outer
     */
    if (!PACKET_get_net_2(ei, &pi_tmp) /* len(extensions) */
        || !PACKET_get_net_2(&outer, &outer_extslen)
        || !PACKET_get_bytes(&outer, &outer_exts, outer_extslen)
        || !WPACKET_start_sub_packet_u16(di)) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    while (PACKET_remaining(ei) > 0) {
        if (!PACKET_get_net_2(ei, &etype)
            || !PACKET_get_net_2(ei, &elen)
            || !PACKET_get_bytes(ei, &eval, elen)) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            goto err;
        }
        if (etype == TLSEXT_TYPE_outer_extensions) {
            PACKET exts;

            if (outers_done++) { /* just do this once */
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }
            if (PACKET_buf_init(&exts, outer_exts, outer_extslen) != 1) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }
            for (i = 0; i != n_outers; i++) {
                if (ech_copy_ext(s, di, outers[i], &exts) != 1)
                    /* SSLfatal called already */
                    goto err;
            }
        } else {
            if (!WPACKET_put_bytes_u16(di, etype)
                || !WPACKET_put_bytes_u16(di, elen)
                || !WPACKET_memcpy(di, eval, elen)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_EXTENSION);
                goto err;
            }
        }
    }
    WPACKET_close(di);
    return 1;
err:
    WPACKET_cleanup(di);
    return 0;
}

/*
 * After successful ECH decrypt, we decode, decompress etc.
 * ob is the outer CH as a buffer
 * ob_len is the size of the above
 * return 1 for success, error otherwise
 *
 * We need the outer CH as a buffer (ob, below) so we can
 * ECH-decompress.
 * The plaintext we start from is in encoded_innerch
 * and our final decoded, decompressed buffer will end up
 * in innerch (which'll then be further processed).
 * That further processing includes all existing decoding
 * checks so we should be fine wrt fuzzing without having
 * to make all checks here (e.g. we can assume that the
 * protocol version, NULL compression etc are correct here -
 * if not, those'll be caught later).
 * Note: there are a lot of literal values here, but it's
 * not clear that changing those to #define'd symbols will
 * help much - a change to the length of a type or from a
 * 2 octet length to longer would seem unlikely.
 */
static int ech_decode_inner(SSL_CONNECTION *s, const unsigned char *ob,
    size_t ob_len, unsigned char *encoded_inner,
    size_t encoded_inner_len)
{
    int rv = 0;
    PACKET ei; /* encoded inner */
    BUF_MEM *di_mem = NULL;
    uint16_t outers[OSSL_ECH_OUTERS_MAX]; /* compressed extension types */
    size_t n_outers = 0;
    WPACKET di = { 0 }; /* "fake" pkt for inner */

    if (encoded_inner == NULL || ob == NULL || ob_len == 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    if ((di_mem = BUF_MEM_new()) == NULL
        || !BUF_MEM_grow(di_mem, SSL3_RT_MAX_PLAIN_LENGTH)
        || !WPACKET_init(&di, di_mem)
        || !WPACKET_put_bytes_u8(&di, SSL3_MT_CLIENT_HELLO)
        || !WPACKET_start_sub_packet_u24(&di)
        || !PACKET_buf_init(&ei, encoded_inner, encoded_inner_len)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    memset(outers, -1, sizeof(outers)); /* fill with known values for debug */
#endif

    /* 1. check for outers and make initial checks of those */
    if (ech_find_outers(s, &ei, outers, &n_outers) != 1)
        goto err; /* SSLfatal called already */

    /* 2. reconstitute inner CH */
    /* reset ei */
    if (PACKET_buf_init(&ei, encoded_inner, encoded_inner_len) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if (ech_reconstitute_inner(s, &di, &ei, ob, ob_len, outers, n_outers) != 1)
        goto err; /* SSLfatal called already */
    /* 3. store final inner CH in connection */
    WPACKET_close(&di);
    if (!WPACKET_get_length(&di, &s->ext.ech.innerch_len)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    OPENSSL_free(s->ext.ech.innerch);
    s->ext.ech.innerch = (unsigned char *)di_mem->data;
    di_mem->data = NULL;
    rv = 1;
err:
    WPACKET_cleanup(&di);
    BUF_MEM_free(di_mem);
    return rv;
}

/*
 * wrapper for hpke_dec just to save code repetition
 * ee is the selected ECH_STORE entry
 * the_ech is the value sent by the client
 * aad_len is the length of the AAD to use
 * aad is the AAD to use
 * forhrr is 0 if not hrr, 1 if this is for 2nd CH
 * innerlen points to the size of the recovered plaintext
 * return pointer to plaintext or NULL (if error)
 *
 * The plaintext returned is allocated here and must
 * be freed by the caller later.
 */
static unsigned char *hpke_decrypt_encch(SSL_CONNECTION *s,
    OSSL_ECHSTORE_ENTRY *ee,
    OSSL_ECH_ENCCH *the_ech,
    size_t aad_len, unsigned char *aad,
    int forhrr, size_t *innerlen)
{
    size_t cipherlen = 0, zind = 0;
    unsigned char *cipher = NULL;
    size_t senderpublen = 0;
    unsigned char *senderpub = NULL;
    size_t clearlen = 0;
    unsigned char *clear = NULL;
    int hpke_mode = OSSL_HPKE_MODE_BASE;
    OSSL_HPKE_SUITE hpke_suite = OSSL_HPKE_SUITE_DEFAULT;
    unsigned char info[OSSL_ECH_MAX_INFO_LEN];
    size_t info_len = OSSL_ECH_MAX_INFO_LEN;
    int rv = 0;
    OSSL_HPKE_CTX *hctx = NULL;
    SSL_CTX *sctx = SSL_CONNECTION_GET_CTX(s);
#ifdef OSSL_ECH_SUPERVERBOSE
    size_t publen = 0;
    unsigned char *pub = NULL;
#endif

    if (ee == NULL || ee->nsuites == 0)
        return NULL;
    cipherlen = the_ech->payload_len;
    cipher = the_ech->payload;
    senderpublen = the_ech->enc_len;
    senderpub = the_ech->enc;
    hpke_suite.aead_id = the_ech->aead_id;
    hpke_suite.kdf_id = the_ech->kdf_id;
    clearlen = cipherlen; /* small overestimate */
    clear = OPENSSL_malloc(clearlen);
    if (clear == NULL)
        return NULL;
    /* The kem_id will be the same for all suites in the entry */
    hpke_suite.kem_id = ee->suites[0].kem_id;
#ifdef OSSL_ECH_SUPERVERBOSE
    publen = ee->pub_len;
    pub = ee->pub;
    ossl_ech_pbuf("aad", aad, aad_len);
    ossl_ech_pbuf("my local pub", pub, publen);
    ossl_ech_pbuf("senderpub", senderpub, senderpublen);
    ossl_ech_pbuf("cipher", cipher, cipherlen);
#endif
    if (ossl_ech_make_enc_info(ee->encoded, ee->encoded_len,
            info, &info_len)
        != 1) {
        OPENSSL_free(clear);
        return NULL;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("info", info, info_len);
#endif
    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out,
            "hpke_dec suite: kem: %04x, kdf: %04x, aead: %04x\n",
            hpke_suite.kem_id, hpke_suite.kdf_id, hpke_suite.aead_id);
    }
    OSSL_TRACE_END(TLS);
    /*
     * We may generate externally visible OpenSSL errors
     * if decryption fails (which is normal) but we'll
     * ignore those as we might be dealing with a GREASEd
     * ECH. To do that we need to now ignore some errors
     * so we use ERR_set_mark() then later ERR_pop_to_mark().
     */
    ERR_set_mark();
    /* Use OSSL_HPKE_* APIs */
    hctx = OSSL_HPKE_CTX_new(hpke_mode, hpke_suite, OSSL_HPKE_ROLE_RECEIVER,
        sctx->libctx, sctx->propq);
    if (hctx == NULL)
        goto clearerrs;
    rv = OSSL_HPKE_decap(hctx, senderpub, senderpublen, ee->keyshare,
        info, info_len);
    if (rv != 1)
        goto clearerrs;
    if (forhrr == 1) {
        rv = OSSL_HPKE_CTX_set_seq(hctx, 1);
        if (rv != 1) {
            /* don't clear this error - GREASE can't cause it */
            ERR_clear_last_mark();
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            goto end;
        }
    }
    rv = OSSL_HPKE_open(hctx, clear, &clearlen, aad, aad_len,
        cipher, cipherlen);
clearerrs:
    /* close off our error handling */
    ERR_pop_to_mark();
end:
    OSSL_HPKE_CTX_free(hctx);
    if (rv != 1) {
        OSSL_TRACE(TLS, "HPKE decryption failed somehow\n");
        OPENSSL_free(clear);
        return NULL;
    }
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("padded clear", clear, clearlen);
#endif
    /* we need to remove possible (actually, v. likely) padding */
    *innerlen = clearlen;
    if (ee->version == OSSL_ECH_RFC9849_VERSION) {
        /* RFC 9849 pads after the encoded CH with zeros */
        size_t extsoffset = 0;
        size_t extslen = 0;
        size_t ch_len = 0;
        size_t startofsessid = 0;
        size_t echoffset = 0; /* offset of start of ECH within CH */
        uint16_t echtype = OSSL_ECH_type_unknown; /* type of ECH seen */
        size_t outersnioffset = 0; /* offset to SNI in outer */
        int innerflag = OSSL_ECH_UNKNOWN_CH_TYPE;
        PACKET innerchpkt;

        if (PACKET_buf_init(&innerchpkt, clear, clearlen) != 1) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_EXTENSION);
            goto paderr;
        }
        /* reset the offsets, as we move from outer to inner CH */
        s->ext.ech.ch_offsets_done = 0;
        rv = ossl_ech_get_ch_offsets(s, &innerchpkt, &startofsessid,
            &extsoffset, &echoffset, &echtype,
            &innerflag, &outersnioffset);
        if (rv != 1) {
            /* SSLfatal called already */
            goto paderr;
        }
        /* odd form of check below just for emphasis */
        if ((extsoffset + 2) > clearlen) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            goto paderr;
        }
        extslen = (unsigned char)(clear[extsoffset]) * 256
            + (unsigned char)(clear[extsoffset + 1]);
        ch_len = extsoffset + 2 + extslen;
        /* the check below protects us from bogus data */
        if (ch_len > clearlen) {
            SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
            goto paderr;
        }
        /* The RFC calls for that padding to be all zeros */

        if (*innerlen < ch_len) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
            goto paderr;
        }
        for (zind = ch_len; zind != *innerlen; zind++) {
            if (clear[zind] != 0x00) {
                SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
                goto paderr;
            }
        }
        *innerlen = ch_len;
#ifdef OSSL_ECH_SUPERVERBOSE
        ossl_ech_pbuf("unpadded clear", clear, *innerlen);
#endif
        return clear;
    } else {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
    }
paderr:
    OPENSSL_free(clear);
    return NULL;
}

/*
 * If an ECH is present, attempt decryption
 * outerpkt is the packet with the outer CH
 * newpkt is the packet with the decrypted inner CH
 * return 1 for success, other otherwise
 *
 * If decryption succeeds, the caller can swap the inner and outer
 * CHs so that all further processing will only take into account
 * the inner CH.
 *
 * The fact that decryption worked is signalled to the caller
 * via s->ext.ech.success
 *
 * This function is called early, (hence the name:-), before
 * the outer CH decoding has really started, so we need to be
 * careful peeking into the packet
 *
 * The plan:
 * 1. check if there's an ECH
 * 2. trial-decrypt or check if config matches one loaded
 * 3. if decrypt fails tee-up GREASE
 * 4. if decrypt worked, decode and de-compress cleartext to
 *    make up real inner CH for later processing
 */
int ossl_ech_early_decrypt(SSL_CONNECTION *s, PACKET *outerpkt, PACKET *newpkt)
{
    int num = 0, cfgind = -1, foundcfg = 0, forhrr = 0, innerflag = -1;
    OSSL_ECH_ENCCH *extval = NULL;
    PACKET echpkt;
    const unsigned char *startofech = NULL, *opd = NULL;
    size_t echlen = 0, clearlen = 0, aad_len = 0;
    unsigned char *clear = NULL, *aad = NULL;
    /* offsets of things within CH */
    size_t startofsessid = 0, startofexts = 0, echoffset = 0, opl = 0;
    size_t outersnioffset = 0, startofciphertext = 0, lenofciphertext = 0;
    uint16_t echtype = OSSL_ECH_type_unknown; /* type of ECH seen */
    char *osni_str = NULL;
    OSSL_ECHSTORE *es = NULL;
    OSSL_ECHSTORE_ENTRY *ee = NULL;

    if (s == NULL)
        return 0;
    if (outerpkt == NULL || newpkt == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_EXTENSION);
        return 0;
    }
    /* find offsets - on success, outputs are safe to use */
    if (ossl_ech_get_ch_offsets(s, outerpkt, &startofsessid, &startofexts,
            &echoffset, &echtype, &innerflag,
            &outersnioffset)
        != 1) {
        /* SSLfatal called already */
        return 0;
    }
    if (echoffset == 0 || echtype != TLSEXT_TYPE_ech)
        return 1; /* ECH not present or wrong version */
    if (innerflag == 1) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        return 0;
    }
    s->ext.ech.attempted = 1; /* Remember that we got an ECH */
    s->ext.ech.attempted_type = echtype;
    if (s->hello_retry_request == SSL_HRR_PENDING)
        forhrr = 1; /* set forhrr if that's correct */
    opl = PACKET_remaining(outerpkt);
    opd = PACKET_data(outerpkt);
    s->tmp_session_id_len = opd[startofsessid]; /* grab the session id */
    if (s->tmp_session_id_len > SSL_MAX_SSL_SESSION_ID_LENGTH
        || startofsessid + 1 + s->tmp_session_id_len > opl) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        goto err;
    }
    memcpy(s->tmp_session_id, &opd[startofsessid + 1], s->tmp_session_id_len);
    if (outersnioffset > 0) { /* Grab the outer SNI for tracing */
        if (ech_get_outer_sni(s, &osni_str, opd, opl, outersnioffset) != 1)
            /* SSLfatal called already */
            goto err;
        if (osni_str == NULL) {
            SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
            goto err;
        }
        OSSL_TRACE1(TLS, "EARLY: outer SNI of %s\n", osni_str);
    } else {
        OSSL_TRACE(TLS, "EARLY: no sign of an outer SNI\n");
    }
    if (echoffset > opl - 4) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    startofech = &opd[echoffset + 4];
    echlen = opd[echoffset + 2] * 256 + opd[echoffset + 3];
    if (echlen > opl - echoffset - 4) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    if (PACKET_buf_init(&echpkt, startofech, echlen) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    if (ech_decode_inbound_ech(s, &echpkt, &extval, &startofciphertext) != 1)
        goto err; /* SSLfatal already called if needed */
    /*
     * startofciphertext is within the ECH value and after the length of the
     * ciphertext, so we need to bump it by the offset of ECH within the CH
     * plus the ECH type (2 octets) and length (also 2 octets) and that
     * ciphertext length (another 2 octets) for a total of 6 octets
     */
    startofciphertext += echoffset + 6;
    lenofciphertext = extval->payload_len;
    aad_len = opl;
    if (aad_len < startofciphertext + lenofciphertext) {
        SSLfatal(s, SSL_AD_DECODE_ERROR, SSL_R_BAD_EXTENSION);
        goto err;
    }
    aad = OPENSSL_memdup(opd, aad_len);
    if (aad == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    memset(aad + startofciphertext, 0, lenofciphertext);
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("EARLY aad", aad, aad_len);
#endif
    s->ext.ech.grease = OSSL_ECH_GREASE_UNKNOWN;
    if (s->ext.ech.es == NULL) {
        SSLfatal(s, SSL_AD_ILLEGAL_PARAMETER, SSL_R_BAD_EXTENSION);
        goto err;
    }
    es = s->ext.ech.es;
    num = (es == NULL || es->entries == NULL ? 0
                                             : sk_OSSL_ECHSTORE_ENTRY_num(es->entries));
    for (cfgind = 0; cfgind != num && foundcfg == 0; cfgind++) {
        ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, cfgind);
        OSSL_TRACE_BEGIN(TLS)
        {
            BIO_printf(trc_out,
                "EARLY: rx'd config id (%x) ==? %d-th configured (%x)\n",
                extval->config_id, cfgind, ee->config_id);
        }
        OSSL_TRACE_END(TLS);
        if (extval->config_id == ee->config_id) {
            unsigned int suite_id;

            /* check aead and kdf match a loaded suite for the config_id */
            for (suite_id = 0; suite_id != ee->nsuites && foundcfg == 0; suite_id++) {
                if (ee->suites[suite_id].kdf_id == extval->kdf_id
                    && ee->suites[suite_id].aead_id == extval->aead_id) {
                    foundcfg = 1;
                }
            }
        }
    }
    if (foundcfg == 1) {
        clear = hpke_decrypt_encch(s, ee, extval, aad_len, aad,
            forhrr, &clearlen);
        if (clear == NULL)
            s->ext.ech.grease = OSSL_ECH_IS_GREASE;
    }
    /* if still needed, trial decryptions */
    if (clear == NULL && (s->options & SSL_OP_ECH_TRIALDECRYPT)) {
        foundcfg = 0; /* reset as we're trying again */
        for (cfgind = 0; cfgind != num; cfgind++) {
            ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, cfgind);
            clear = hpke_decrypt_encch(s, ee, extval,
                aad_len, aad, forhrr, &clearlen);
            if (clear != NULL) {
                foundcfg = 1;
                s->ext.ech.grease = OSSL_ECH_NOT_GREASE;
                break;
            }
        }
    }
    OPENSSL_free(aad);
    aad = NULL;
    s->ext.ech.done = 1; /* decrypting worked or not, but we're done now */
    s->ext.ech.grease = OSSL_ECH_IS_GREASE; /* if decrypt fails tee-up GREASE */
    s->ext.ech.success = 0;
    if (clear != NULL) {
        s->ext.ech.grease = OSSL_ECH_NOT_GREASE;
        s->ext.ech.success = 1;
    }
    OSSL_TRACE_BEGIN(TLS)
    {
        BIO_printf(trc_out, "EARLY: success: %d, assume_grease: %d, "
                            "foundcfg: %d, cfgind: %d, clearlen: %zd, clear %p\n",
            s->ext.ech.success, s->ext.ech.grease, foundcfg,
            cfgind, clearlen, (void *)clear);
    }
    OSSL_TRACE_END(TLS);
#ifdef OSSL_ECH_SUPERVERBOSE
    if (foundcfg == 1 && clear != NULL) { /* Bit more logging */
        ossl_ech_pbuf("local config_id", &ee->config_id, 1);
        ossl_ech_pbuf("remote config_id", &extval->config_id, 1);
        ossl_ech_pbuf("clear", clear, clearlen);
    }
#endif
    ossl_ech_encch_free(extval);
    OPENSSL_free(extval);
    extval = NULL;
    if (s->ext.ech.grease == OSSL_ECH_IS_GREASE) {
        OPENSSL_free(clear);
        return 1;
    }
    /* 4. if decrypt worked, de-compress cleartext to make up real inner CH */
    if (ech_decode_inner(s, opd, opl, clear, clearlen) != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    OPENSSL_free(clear);
    clear = NULL;
#ifdef OSSL_ECH_SUPERVERBOSE
    ossl_ech_pbuf("Inner CH (decoded)", s->ext.ech.innerch,
        s->ext.ech.innerch_len);
#endif
    if (PACKET_buf_init(newpkt, s->ext.ech.innerch,
            s->ext.ech.innerch_len)
        != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* tls_process_client_hello doesn't want the message header, so skip it */
    if (!PACKET_forward(newpkt, SSL3_HM_HEADER_LENGTH)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if (ossl_ech_intbuf_add(s, s->ext.ech.innerch,
            s->ext.ech.innerch_len, 0)
        != 1) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    return 1;
err:
    OPENSSL_free(aad);
    if (extval != NULL) {
        ossl_ech_encch_free(extval);
        OPENSSL_free(extval);
    }
    OPENSSL_free(clear);
    return 0;
}

int ossl_ech_intbuf_add(SSL_CONNECTION *s, const unsigned char *buf,
    size_t blen, int hash_existing)
{
    EVP_MD_CTX *ctx = NULL;
    EVP_MD *md = NULL;
    unsigned int rv = 0, hashlen = 0;
    unsigned char hashval[EVP_MAX_MD_SIZE], *t1;
    size_t tlen;
    WPACKET tpkt = { 0 };
    BUF_MEM *tpkt_mem = NULL;

    if (s == NULL || buf == NULL || blen == 0)
        goto err;
    if (hash_existing == 1) {
        /* hash existing buffer, needed during HRR */
        if (s->ext.ech.transbuf == NULL
            || (md = (EVP_MD *)ssl_handshake_md(s)) == NULL
            || (ctx = EVP_MD_CTX_new()) == NULL
            || EVP_DigestInit_ex(ctx, md, NULL) <= 0
            || EVP_DigestUpdate(ctx, s->ext.ech.transbuf,
                   s->ext.ech.transbuf_len)
                <= 0
            || EVP_DigestFinal_ex(ctx, hashval, &hashlen) <= 0
            || (tpkt_mem = BUF_MEM_new()) == NULL
            || !WPACKET_init(&tpkt, tpkt_mem)
            || !WPACKET_put_bytes_u8(&tpkt, SSL3_MT_MESSAGE_HASH)
            || !WPACKET_put_bytes_u24(&tpkt, hashlen)
            || !WPACKET_memcpy(&tpkt, hashval, hashlen)
            || !WPACKET_get_length(&tpkt, &tlen)
            || (t1 = OPENSSL_realloc(s->ext.ech.transbuf, tlen + blen)) == NULL)
            goto err;
        s->ext.ech.transbuf = t1;
        memcpy(s->ext.ech.transbuf, tpkt_mem->data, tlen);
        memcpy(s->ext.ech.transbuf + tlen, buf, blen);
        s->ext.ech.transbuf_len = tlen + blen;
    } else {
        /* just add new octets */
        if ((t1 = OPENSSL_realloc(s->ext.ech.transbuf,
                 s->ext.ech.transbuf_len + blen))
            == NULL)
            goto err;
        s->ext.ech.transbuf = t1;
        memcpy(s->ext.ech.transbuf + s->ext.ech.transbuf_len, buf, blen);
        s->ext.ech.transbuf_len += blen;
    }
    rv = 1;
err:
    BUF_MEM_free(tpkt_mem);
    WPACKET_cleanup(&tpkt);
    EVP_MD_CTX_free(ctx);
    return rv;
}

int ossl_ech_intbuf_fetch(SSL_CONNECTION *s, unsigned char **buf, size_t *blen)
{
    if (s == NULL || buf == NULL || blen == NULL || s->ext.ech.transbuf == NULL)
        return 0;
    *buf = s->ext.ech.transbuf;
    *blen = s->ext.ech.transbuf_len;
    return 1;
}

int ossl_ech_stash_keyshares(SSL_CONNECTION *s)
{
    size_t i;

    ech_free_stashed_key_shares(&s->ext.ech);
    for (i = 0; i != s->s3.tmp.num_ks_pkey; i++) {
        s->ext.ech.ks_pkey[i] = s->s3.tmp.ks_pkey[i];
        if (EVP_PKEY_up_ref(s->ext.ech.ks_pkey[i]) != 1)
            return 0;
        s->ext.ech.ks_group_id[i] = s->s3.tmp.ks_group_id[i];
    }
    s->ext.ech.num_ks_pkey = s->s3.tmp.num_ks_pkey;
    return 1;
}

int ossl_ech_unstash_keyshares(SSL_CONNECTION *s)
{
    size_t i;

    for (i = 0; i != s->s3.tmp.num_ks_pkey; i++) {
        EVP_PKEY_free(s->s3.tmp.ks_pkey[i]);
        s->s3.tmp.ks_pkey[i] = NULL;
    }
    for (i = 0; i != s->ext.ech.num_ks_pkey; i++) {
        s->s3.tmp.ks_pkey[i] = s->ext.ech.ks_pkey[i];
        if (EVP_PKEY_up_ref(s->s3.tmp.ks_pkey[i]) != 1)
            return 0;
        s->s3.tmp.ks_group_id[i] = s->ext.ech.ks_group_id[i];
    }
    s->s3.tmp.num_ks_pkey = s->ext.ech.num_ks_pkey;
    ech_free_stashed_key_shares(&s->ext.ech);
    return 1;
}
#endif
