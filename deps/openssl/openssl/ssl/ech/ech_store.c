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
#include "../ssl_local.h"
#include "ech_local.h"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>

/* a size for some crypto vars */
#define OSSL_ECH_CRYPTO_VAR_SIZE 2048

/*
 * Used for ech_bio2buf, when reading from a BIO we allocate in chunks sized
 * as per below, with a max number of chunks as indicated, we don't expect to
 * go beyond one chunk in almost all cases
 */
#define OSSL_ECH_BUFCHUNK 512
#define OSSL_ECH_MAXITER 32

/*
 * ECHConfigList input to OSSL_ECHSTORE_read_echconfiglist()
 * can be either binary encoded ECHConfigList or a base64
 * encoded ECHConfigList.
 */
#define OSSL_ECH_FMT_BIN 1 /* binary ECHConfigList */
#define OSSL_ECH_FMT_B64TXT 2 /* base64 ECHConfigList */

/*
 * Telltales we use when guessing which form of encoded input we've
 * been given for an RR value or ECHConfig.
 * We give these the EBCDIC treatment as well - why not? :-)
 */
static const char B64_alphabet[] = "\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f\x50\x51\x52"
                                   "\x53\x54\x55\x56\x57\x58\x59\x5a\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a"
                                   "\x6b\x6c\x6d\x6e\x6f\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x30\x31"
                                   "\x32\x33\x34\x35\x36\x37\x38\x39\x2b\x2f\x3d\x3b";

#ifndef TLSEXT_MINLEN_host_name
/* The shortest DNS name we allow, e.g. "a.bc" */
#define TLSEXT_MINLEN_host_name 4
#endif

/*
 * local functions - public APIs are at the end
 */

void ossl_echext_free(OSSL_ECHEXT *e)
{
    if (e == NULL)
        return;
    OPENSSL_free(e->val);
    OPENSSL_free(e);
    return;
}

OSSL_ECHEXT *ossl_echext_dup(const OSSL_ECHEXT *src)
{
    OSSL_ECHEXT *ext = OPENSSL_zalloc(sizeof(*src));

    if (ext == NULL)
        return NULL;
    *ext = *src;
    ext->val = NULL;
    if (ext->len != 0) {
        ext->val = OPENSSL_memdup(src->val, src->len);
        if (ext->val == NULL) {
            ossl_echext_free(ext);
            return NULL;
        }
    }
    return ext;
}

void ossl_echstore_entry_free(OSSL_ECHSTORE_ENTRY *ee)
{
    if (ee == NULL)
        return;
    OPENSSL_free(ee->public_name);
    OPENSSL_free(ee->pub);
    EVP_PKEY_free(ee->keyshare);
    OPENSSL_free(ee->encoded);
    OPENSSL_free(ee->suites);
    sk_OSSL_ECHEXT_pop_free(ee->exts, ossl_echext_free);
    OPENSSL_free(ee);
    return;
}

/*
 * @brief Read a buffer from an input 'till eof
 * @param in is the BIO input
 * @param buf is where to put the buffer, allocated inside here
 * @param len is the length of that buffer
 *
 * This is intended for small inputs, either files or buffers and
 * not other kinds of BIO.
 */
static int ech_bio2buf(BIO *in, unsigned char **buf, size_t *len)
{
    unsigned char *lptr = NULL, *lbuf = NULL, *tmp = NULL;
    size_t sofar = 0, readbytes = 0;
    int done = 0, brv, iter = 0;

    if (buf == NULL || len == NULL)
        return 0;
    sofar = OSSL_ECH_BUFCHUNK;
    lbuf = OPENSSL_zalloc(sofar);
    if (lbuf == NULL)
        return 0;
    lptr = lbuf;
    while (!BIO_eof(in) && !done && iter++ < OSSL_ECH_MAXITER) {
        brv = BIO_read_ex(in, lptr, OSSL_ECH_BUFCHUNK, &readbytes);
        if (brv != 1)
            goto err;
        if (BIO_eof(in) || readbytes < OSSL_ECH_BUFCHUNK) {
            done = 1;
            break;
        }
        sofar += OSSL_ECH_BUFCHUNK;
        tmp = OPENSSL_realloc(lbuf, sofar);
        if (tmp == NULL)
            goto err;
        lbuf = tmp;
        lptr = lbuf + sofar - OSSL_ECH_BUFCHUNK;
    }
    if (BIO_eof(in) && done == 1) {
        *len = sofar + readbytes - OSSL_ECH_BUFCHUNK;
        *buf = lbuf;
        return 1;
    }
err:
    OPENSSL_free(lbuf);
    return 0;
}

/*
 * @brief Figure out ECHConfig encoding
 * @param val is a buffer with the encoding
 * @param len is the length of that buffer
 * @param fmt is the detected format
 * @return 1 for success, 0 for error
 */
static int ech_check_format(const unsigned char *val, size_t len, int *fmt)
{
    size_t span = 0;
    char *copy_with_NUL = NULL;

    if (fmt == NULL || len <= 4 || val == NULL)
        return 0;
    /* binary encoding starts with two octet length and ECH version */
    if (len == 2 + ((size_t)(val[0]) * 256 + (size_t)(val[1]))
        && val[2] == ((OSSL_ECH_RFC9849_VERSION / 256) & 0xff)
        && val[3] == ((OSSL_ECH_RFC9849_VERSION % 256) & 0xff)) {
        *fmt = OSSL_ECH_FMT_BIN;
        return 1;
    }
    /* ensure we always end with a NUL so strspn is safe */
    copy_with_NUL = OPENSSL_malloc(len + 1);
    if (copy_with_NUL == NULL)
        return 0;
    memcpy(copy_with_NUL, val, len);
    copy_with_NUL[len] = '\0';
    span = strspn(copy_with_NUL, B64_alphabet);
    OPENSSL_free(copy_with_NUL);
    if (len <= span) {
        *fmt = OSSL_ECH_FMT_B64TXT;
        return 1;
    }
    return 0;
}

/*
 * @brief helper to decode ECHConfig extensions
 * @param ee is the OSSL_ECHSTORE entry for these
 * @param exts is the binary form extensions
 * @return 1 for good, 0 for error
 */
static int ech_decode_echconfig_exts(OSSL_ECHSTORE_ENTRY *ee, PACKET *exts)
{
    unsigned int exttype = 0;
    size_t extlen = 0;
    unsigned char *extval = NULL;
    OSSL_ECHEXT *oe = NULL;
    PACKET ext;

    /*
     * reminder: exts is a two-octet length prefixed list of:
     * - two octet extension type
     * - two octet extension length (can be zero)
     * - length octets
     * we've consumed the overall length before getting here
     */
    while (PACKET_remaining(exts) > 0) {
        exttype = 0, extlen = 0;
        extval = NULL;
        oe = NULL;
        if (!PACKET_get_net_2(exts, &exttype) || !PACKET_get_length_prefixed_2(exts, &ext)) {
            ERR_raise(ERR_LIB_SSL, SSL_R_BAD_ECHCONFIG_EXTENSION);
            goto err;
        }
        if (PACKET_remaining(&ext) >= OSSL_ECH_MAX_ECHCONFIGEXT_LEN) {
            ERR_raise(ERR_LIB_SSL, SSL_R_BAD_ECHCONFIG_EXTENSION);
            goto err;
        }
        if (!PACKET_memdup(&ext, &extval, &extlen)) {
            ERR_raise(ERR_LIB_SSL, SSL_R_BAD_ECHCONFIG_EXTENSION);
            goto err;
        }
        oe = OPENSSL_malloc(sizeof(*oe));
        if (oe == NULL)
            goto err;
        oe->type = (uint16_t)exttype;
        oe->val = extval;
        extval = NULL; /* avoid double free */
        oe->len = (uint16_t)extlen;
        if (ee->exts == NULL)
            ee->exts = sk_OSSL_ECHEXT_new_null();
        if (ee->exts == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        if (!sk_OSSL_ECHEXT_push(ee->exts, oe)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    }
    return 1;
err:
    sk_OSSL_ECHEXT_pop_free(ee->exts, ossl_echext_free);
    ee->exts = NULL;
    ossl_echext_free(oe);
    OPENSSL_free(extval);
    return 0;
}

/*
 * @brief Check entry to see if looks good or bad
 * @param ee is the ECHConfig to check
 * @return 1 for all good, 0 otherwise
 */
static int ech_final_config_checks(OSSL_ECHSTORE_ENTRY *ee)
{
    OSSL_HPKE_SUITE hpke_suite;
    int ind, num, rv = 0, goodsuitefound = 0;
    X509_VERIFY_PARAM *vpm = X509_VERIFY_PARAM_new();
    char *lastlabel = NULL;
    size_t lllen;

    if (vpm == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* check local support for some suite */
    for (ind = 0; ind != (int)ee->nsuites; ind++) {
        /*
         * suite_check says yes to the pseudo-aead for export, but we don't
         * want to see it here coming from outside in an encoding
         */
        hpke_suite = ee->suites[ind];
        if (OSSL_HPKE_suite_check(hpke_suite) == 1
            && hpke_suite.aead_id != OSSL_HPKE_AEAD_ID_EXPORTONLY) {
            goodsuitefound = 1;
            break;
        }
    }
    if (goodsuitefound == 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        goto err;
    }
    /* check no mandatory exts (with high bit set in type) */
    num = (ee->exts == NULL ? 0 : sk_OSSL_ECHEXT_num(ee->exts));
    for (ind = 0; ind != num; ind++) {
        OSSL_ECHEXT *oe = sk_OSSL_ECHEXT_value(ee->exts, (int)ind);

        if (oe->type & 0x8000) {
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
            goto err;
        }
    }
    /* check public_name rules, as per spec section 6.1.7 */
    if (ee->public_name == NULL
        || ee->public_name[0] == '\0'
        || ee->public_name[0] == '.'
        || ee->public_name[strlen(ee->public_name) - 1] == '.'
        || strlen(ee->public_name) > 255)
        goto err;
    /*
     * Use X509_VERIFY_PARAM_add1_host to avoid coding same checks twice.
     * This checks max 63 octets per label, overall length and some other
     * DNS label checks.
     */
    if (X509_VERIFY_PARAM_add1_host(vpm, ee->public_name, 0) == 0)
        goto err;
    /*
     * but we still have to check the last label restrictions, which
     * are intended to avoid confusion with IP address literals in
     * encodings browsers support, as per WHATWG (convincing, eh:-)
     */
    lastlabel = strrchr(ee->public_name, '.');
    if (lastlabel == NULL) /* if there are no dots */
        lastlabel = ee->public_name;
    lllen = strlen(lastlabel);
    if (lllen < 2)
        goto err;
    if (lastlabel[0] == '.') {
        lastlabel++;
        lllen--;
    }
    if (strspn(lastlabel, "0123456789") == lllen)
        goto err;
    if (lastlabel[0] == '0' && lllen > 2
        && (lastlabel[1] == 'x' || lastlabel[1] == 'X')
        && strspn(lastlabel + 2, "0123456789abcdefABCDEF") == (lllen - 2))
        goto err;
    rv = 1;
err:
    X509_VERIFY_PARAM_free(vpm);
    return rv;
}

/**
 * @brief decode one ECHConfig from a packet into an entry
 * @param rent ptr to an entry allocated within (on success)
 * @param pkt is the encoding
 * @param priv is an optional private key (NULL if absent)
 * @param for_retry says whether to include in a retry_config (if priv present)
 * @return 1 for success, 0 for error
 */
static int ech_decode_one_entry(OSSL_ECHSTORE_ENTRY **rent, PACKET *pkt,
    EVP_PKEY *priv, int for_retry)
{
    size_t ech_content_length = 0;
    unsigned int tmpi;
    const unsigned char *tmpecp = NULL;
    size_t tmpeclen = 0, test_publen = 0;
    PACKET ver_pkt, pub_pkt, cipher_suites, public_name_pkt, exts;
    uint16_t thiskemid;
    size_t suiteoctets = 0;
    unsigned int ci = 0;
    unsigned char cipher[OSSL_ECH_CIPHER_LEN], max_name_len;
    unsigned char test_pub[OSSL_ECH_CRYPTO_VAR_SIZE];
    OSSL_ECHSTORE_ENTRY *ee = NULL;

    if (rent == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    if (pkt == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    ee = OPENSSL_zalloc(sizeof(*ee));
    if (ee == NULL)
        goto err;
    /* note start of encoding so we can make a copy later */
    tmpeclen = PACKET_remaining(pkt);
    if (PACKET_peek_bytes(pkt, &tmpecp, tmpeclen) != 1
        || !PACKET_get_net_2(pkt, &tmpi)) {
        ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
        goto err;
    }
    ee->version = (uint16_t)tmpi;

    /* grab versioned packet data */
    if (!PACKET_get_length_prefixed_2(pkt, &ver_pkt)) {
        ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
        goto err;
    }
    ech_content_length = (unsigned int)PACKET_remaining(&ver_pkt);
    switch (ee->version) {
    case OSSL_ECH_RFC9849_VERSION:
        break;
    default:
        /* skip over in case we get something we can handle later */
        if (!PACKET_forward(&ver_pkt, ech_content_length)) {
            ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
            goto err;
        }
        /* nothing to return but not a fail */
        ossl_echstore_entry_free(ee);
        *rent = NULL;
        return 1;
    }
    if (!PACKET_copy_bytes(&ver_pkt, &ee->config_id, 1)
        || !PACKET_get_net_2(&ver_pkt, &tmpi)
        || !PACKET_get_length_prefixed_2(&ver_pkt, &pub_pkt)
        || !PACKET_memdup(&pub_pkt, &ee->pub, &ee->pub_len)
        || !PACKET_get_length_prefixed_2(&ver_pkt, &cipher_suites)
        || (suiteoctets = PACKET_remaining(&cipher_suites)) <= 0
        || (suiteoctets % 2) == 1
        || suiteoctets / OSSL_ECH_CIPHER_LEN > UINT_MAX) {
        ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
        goto err;
    }
    thiskemid = (uint16_t)tmpi;
    ee->nsuites = (unsigned int)(suiteoctets / OSSL_ECH_CIPHER_LEN);
    ee->suites = OPENSSL_malloc_array(ee->nsuites, sizeof(*ee->suites));
    if (ee->suites == NULL)
        goto err;
    while (PACKET_copy_bytes(&cipher_suites, cipher,
        OSSL_ECH_CIPHER_LEN)) {
        ee->suites[ci].kem_id = thiskemid;
        ee->suites[ci].kdf_id = cipher[0] << 8 | cipher[1];
        ee->suites[ci].aead_id = cipher[2] << 8 | cipher[3];
        if (ci++ >= ee->nsuites) {
            ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
            goto err;
        }
    }
    if (PACKET_remaining(&cipher_suites) > 0
        || !PACKET_copy_bytes(&ver_pkt, &max_name_len, 1)) {
        ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
        goto err;
    }
    ee->max_name_length = max_name_len;
    if (!PACKET_get_length_prefixed_1(&ver_pkt, &public_name_pkt)) {
        ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
        goto err;
    }
    if (PACKET_contains_zero_byte(&public_name_pkt)
        || PACKET_remaining(&public_name_pkt) < TLSEXT_MINLEN_host_name
        || !PACKET_strndup(&public_name_pkt, &ee->public_name)) {
        ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
        goto err;
    }
    /*
     * We don't really handle ECHConfig extensions as of now,
     * (none are well-defined), so we're only skipping over
     * whatever we find here. If/when adding real extensions
     * then it may be necessary to also check that the set of
     * extensions loaded contain no duplicate types.
     */
    if (!PACKET_get_length_prefixed_2(&ver_pkt, &exts)) {
        ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
        goto err;
    }
    if (PACKET_remaining(&exts) > 0
        && ech_decode_echconfig_exts(ee, &exts) != 1) {
        ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
        goto err;
    }
    /* set length of encoding of this ECHConfig */
    ee->encoded_len = PACKET_data(&ver_pkt) - tmpecp;
    /* copy encoded as it might get free'd if a reduce happens */
    ee->encoded = OPENSSL_memdup(tmpecp, ee->encoded_len);
    if (ee->encoded == NULL)
        goto err;
    if (priv != NULL) {
        if (EVP_PKEY_get_octet_string_param(priv,
                OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY,
                test_pub, OSSL_ECH_CRYPTO_VAR_SIZE,
                &test_publen)
            != 1) {
            ERR_raise(ERR_LIB_SSL, SSL_R_ECH_DECODE_ERROR);
            goto err;
        }
        if (test_publen == ee->pub_len
            && !memcmp(test_pub, ee->pub, ee->pub_len)) {
            EVP_PKEY_up_ref(priv); /* associate the private key */
            ee->keyshare = priv;
            ee->for_retry = for_retry;
        }
    }
    ee->loadtime = time(0);
    *rent = ee;
    return 1;
err:
    ossl_echstore_entry_free(ee);
    *rent = NULL;
    return 0;
}

/*
 * @brief decode and flatten a binary encoded ECHConfigList
 * @param es an OSSL_ECHSTORE
 * @param priv is an optional private key (NULL if absent)
 * @param for_retry says whether to include in a retry_config (if priv present)
 * @param binbuf binary encoded ECHConfigList (we hope)
 * @param binlen length of binbuf
 * @return 1 for success, 0 for error
 *
 * We may only get one ECHConfig per list, but there can be more.  We want each
 * element of the output to contain exactly one ECHConfig so that a client
 * could sensibly down select to the one they prefer later, and so that we have
 * the specific encoded value of that ECHConfig for inclusion in the HPKE info
 * parameter when finally encrypting or decrypting an inner ClientHello.
 *
 * If a private value is provided then that'll only be associated with the
 * relevant public value, if >1 public value was present in the ECHConfigList.
 */
static int ech_decode_and_flatten(OSSL_ECHSTORE *es, EVP_PKEY *priv, int for_retry,
    unsigned char *binbuf, size_t binblen)
{
    int rv = 0;
    size_t remaining = 0;
    PACKET opkt, pkt;
    OSSL_ECHSTORE_ENTRY *ee = NULL;

    if (binbuf == NULL || binblen == 0 || binblen < OSSL_ECH_MIN_ECHCONFIG_LEN
        || binblen >= OSSL_ECH_MAX_ECHCONFIG_LEN) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        goto err;
    }
    if (PACKET_buf_init(&opkt, binbuf, binblen) != 1
        || !PACKET_get_length_prefixed_2(&opkt, &pkt)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    remaining = PACKET_remaining(&pkt);
    while (remaining > 0) {
        if (ech_decode_one_entry(&ee, &pkt, priv, for_retry) != 1) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        remaining = PACKET_remaining(&pkt);
        /* if unsupported version we can skip over */
        if (ee == NULL)
            continue;
        /* do final checks on suites, exts, and fail if issues */
        if (ech_final_config_checks(ee) != 1)
            goto err;
        /* push entry into store */
        if (es->entries == NULL)
            es->entries = sk_OSSL_ECHSTORE_ENTRY_new_null();
        if (es->entries == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        if (!sk_OSSL_ECHSTORE_ENTRY_push(es->entries, ee)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        ee = NULL;
    }
    rv = 1;
err:
    ossl_echstore_entry_free(ee);
    return rv;
}

/*
 * @brief check a private matches some public
 * @param es is the ECH store
 * @param priv is the private value
 * @return 1 if we have a match, zero otherwise
 */
static int check_priv_matches(OSSL_ECHSTORE *es, EVP_PKEY *priv)
{
    int num, ent, gotone = 0;
    OSSL_ECHSTORE_ENTRY *ee = NULL;

    num = (es->entries == NULL ? 0 : sk_OSSL_ECHSTORE_ENTRY_num(es->entries));
    for (ent = 0; ent != num; ent++) {
        ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, ent);
        if (ee == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
            return 0;
        }
        if (EVP_PKEY_eq(ee->keyshare, priv)) {
            gotone = 1;
            break;
        }
    }
    return gotone;
}

/*
 * @brief decode input ECHConfigList and associate optional private info
 * @param es is the OSSL_ECHSTORE
 * @param in is the BIO from which we'll get the ECHConfigList
 * @param priv is an optional private key
 * @param for_retry 1 if the public related to priv ought be in retry_config
 */
static int ech_read_priv_echconfiglist(OSSL_ECHSTORE *es, BIO *in,
    EVP_PKEY *priv, int for_retry)
{
    int rv = 0, detfmt, tdeclen = 0;
    size_t encodedlen = 0, binlen = 0;
    unsigned char *encodedval = NULL, *binbuf = NULL;
    BIO *btmp = NULL, *btmp1 = NULL;

    if (es == NULL || in == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    if (ech_bio2buf(in, &encodedval, &encodedlen) != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    if (encodedlen >= OSSL_ECH_MAX_ECHCONFIG_LEN) { /* sanity check */
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if (ech_check_format(encodedval, encodedlen, &detfmt) != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        goto err;
    }
    if (detfmt == OSSL_ECH_FMT_BIN) { /* copy buffer if binary format */
        binbuf = OPENSSL_memdup(encodedval, encodedlen);
        if (binbuf == NULL)
            goto err;
        binlen = encodedlen;
    }
    if (detfmt == OSSL_ECH_FMT_B64TXT) {
        btmp = BIO_new_mem_buf(encodedval, (int)encodedlen);
        if (btmp == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        btmp1 = BIO_new(BIO_f_base64());
        if (btmp1 == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        BIO_set_flags(btmp1, BIO_FLAGS_BASE64_NO_NL);
        btmp = BIO_push(btmp1, btmp);
        /* overestimate but good enough */
        binbuf = OPENSSL_malloc(encodedlen);
        if (binbuf == NULL)
            goto err;
        tdeclen = BIO_read(btmp, binbuf, (int)encodedlen);
        if (tdeclen <= 0) { /* need int for -1 return in failure case */
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        binlen = tdeclen;
    }
    if (ech_decode_and_flatten(es, priv, for_retry, binbuf, binlen) != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if (priv != NULL && check_priv_matches(es, priv) == 0)
        goto err;
    rv = 1;
err:
    BIO_free_all(btmp);
    OPENSSL_free(binbuf);
    OPENSSL_free(encodedval);
    return rv;
}

/*
 * API calls built around OSSL_ECHSSTORE
 */

OSSL_ECHSTORE *OSSL_ECHSTORE_new(OSSL_LIB_CTX *libctx, const char *propq)
{
    OSSL_ECHSTORE *es = NULL;

    es = OPENSSL_zalloc(sizeof(*es));
    if (es == NULL)
        return 0;
    es->libctx = libctx;
    if (propq != NULL) {
        es->propq = OPENSSL_strdup(propq);
        if (es->propq == NULL) {
            OPENSSL_free(es);
            return 0;
        }
    }

    return es;
}

void OSSL_ECHSTORE_free(OSSL_ECHSTORE *es)
{
    if (es == NULL)
        return;
    sk_OSSL_ECHSTORE_ENTRY_pop_free(es->entries, ossl_echstore_entry_free);
    OPENSSL_free(es->propq);
    OPENSSL_free(es);
    return;
}

int OSSL_ECHSTORE_new_config(OSSL_ECHSTORE *es,
    uint16_t echversion, uint8_t max_name_length,
    const char *public_name, OSSL_HPKE_SUITE suite)
{
    size_t pnlen = 0, publen = OSSL_ECH_CRYPTO_VAR_SIZE;
    unsigned char pub[OSSL_ECH_CRYPTO_VAR_SIZE];
    int rv = 0;
    unsigned char *bp = NULL;
    size_t bblen = 0;
    EVP_PKEY *privp = NULL;
    uint8_t config_id = 0;
    WPACKET epkt;
    BUF_MEM *epkt_mem = NULL;
    OSSL_ECHSTORE_ENTRY *ee = NULL;

    /* basic checks */
    if (es == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    pnlen = (public_name == NULL ? 0 : strlen(public_name));
    if (pnlen == 0 || pnlen > OSSL_ECH_MAX_PUBLICNAME) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    /* this used have more versions and will again in future */
    switch (echversion) {
    case OSSL_ECH_RFC9849_VERSION:
        break;
    default:
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    /*
     *   Reminder, for draft-13 we want this:
     *
     *   opaque HpkePublicKey<1..2^16-1>;
     *   uint16 HpkeKemId;  // Defined in I-D.irtf-cfrg-hpke
     *   uint16 HpkeKdfId;  // Defined in I-D.irtf-cfrg-hpke
     *   uint16 HpkeAeadId; // Defined in I-D.irtf-cfrg-hpke
     *   struct {
     *       HpkeKdfId kdf_id;
     *       HpkeAeadId aead_id;
     *   } HpkeSymmetricCipherSuite;
     *   struct {
     *       uint8 config_id;
     *       HpkeKemId kem_id;
     *       HpkePublicKey public_key;
     *       HpkeSymmetricCipherSuite cipher_suites<4..2^16-4>;
     *   } HpkeKeyConfig;
     *   struct {
     *       HpkeKeyConfig key_config;
     *       uint8 maximum_name_length;
     *       opaque public_name<1..255>;
     *       Extension extensions<0..2^16-1>;
     *   } ECHConfigContents;
     *   struct {
     *       uint16 version;
     *       uint16 length;
     *       select (ECHConfig.version) {
     *         case 0xfe0d: ECHConfigContents contents;
     *       }
     *   } ECHConfig;
     *   ECHConfig ECHConfigList<1..2^16-1>;
     */
    if ((epkt_mem = BUF_MEM_new()) == NULL
        || !BUF_MEM_grow(epkt_mem, OSSL_ECH_MAX_ECHCONFIG_LEN)
        || !WPACKET_init(&epkt, epkt_mem)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err_no_epkt;
    }
    /* random config_id */
    if (RAND_bytes_ex(es->libctx, (unsigned char *)&config_id, 1, 0) <= 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* key pair */
    if (OSSL_HPKE_keygen(suite, pub, &publen, &privp, NULL, 0,
            es->libctx, es->propq)
        != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* config id, KEM, public, KDF, AEAD, max name len, public_name, exts */
    if ((bp = WPACKET_get_curr(&epkt)) == NULL
        || !WPACKET_start_sub_packet_u16(&epkt)
        || !WPACKET_put_bytes_u16(&epkt, echversion)
        || !WPACKET_start_sub_packet_u16(&epkt)
        || !WPACKET_put_bytes_u8(&epkt, config_id)
        || !WPACKET_put_bytes_u16(&epkt, suite.kem_id)
        || !WPACKET_start_sub_packet_u16(&epkt)
        || !WPACKET_memcpy(&epkt, pub, publen)
        || !WPACKET_close(&epkt)
        || !WPACKET_start_sub_packet_u16(&epkt)
        || !WPACKET_put_bytes_u16(&epkt, suite.kdf_id)
        || !WPACKET_put_bytes_u16(&epkt, suite.aead_id)
        || !WPACKET_close(&epkt)
        || !WPACKET_put_bytes_u8(&epkt, max_name_length)
        || !WPACKET_start_sub_packet_u8(&epkt)
        || !WPACKET_memcpy(&epkt, public_name, pnlen)
        || !WPACKET_close(&epkt)
        || !WPACKET_start_sub_packet_u16(&epkt)
        || !WPACKET_memcpy(&epkt, NULL, 0) /* no extensions */
        || !WPACKET_close(&epkt)
        || !WPACKET_close(&epkt)
        || !WPACKET_close(&epkt)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    /* bp, bblen has encoding */
    if (!WPACKET_get_total_written(&epkt, &bblen)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if ((ee = OPENSSL_zalloc(sizeof(*ee))) == NULL)
        goto err;
    ee->suites = OPENSSL_malloc(sizeof(*ee->suites));
    if (ee->suites == NULL)
        goto err;
    ee->version = echversion;
    ee->pub_len = publen;
    ee->pub = OPENSSL_memdup(pub, publen);
    if (ee->pub == NULL)
        goto err;
    ee->nsuites = 1;
    ee->suites[0] = suite;
    ee->public_name = OPENSSL_strdup(public_name);
    if (ee->public_name == NULL)
        goto err;
    ee->max_name_length = max_name_length;
    ee->config_id = config_id;
    ee->keyshare = privp;
    privp = NULL; /* don't free twice */
    /* "steal" the encoding from the memory */
    ee->encoded = (unsigned char *)epkt_mem->data;
    ee->encoded_len = bblen;
    epkt_mem->data = NULL;
    epkt_mem->length = 0;
    ee->loadtime = time(0);
    if (ech_final_config_checks(ee) != 1) /* check our work */
        goto err;
    /* push entry into store */
    if (es->entries == NULL)
        es->entries = sk_OSSL_ECHSTORE_ENTRY_new_null();
    if (es->entries == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if (!sk_OSSL_ECHSTORE_ENTRY_push(es->entries, ee)) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    WPACKET_finish(&epkt);
    BUF_MEM_free(epkt_mem);
    return 1;

err:
    ossl_echstore_entry_free(ee);
    EVP_PKEY_free(privp);
    WPACKET_cleanup(&epkt);
err_no_epkt:
    BUF_MEM_free(epkt_mem);
    return rv;
}

int OSSL_ECHSTORE_write_pem(OSSL_ECHSTORE *es, int index, BIO *out)
{
    OSSL_ECHSTORE_ENTRY *ee = NULL;
    int rv = 0, num = 0, chosen = 0, doall = 0;
    WPACKET epkt; /* used if we want to merge ECHConfigs for output */
    BUF_MEM *epkt_mem = NULL;
    size_t allencoded_len;

    if (es == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    num = (es->entries == NULL ? 0 : sk_OSSL_ECHSTORE_ENTRY_num(es->entries));
    if (num <= 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    if (index >= num) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    if (index == OSSL_ECHSTORE_ALL)
        doall = 1;
    else if (index == OSSL_ECHSTORE_LAST)
        chosen = num - 1;
    else
        chosen = index;
    memset(&epkt, 0, sizeof(epkt));
    if (doall == 0) {
        ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, chosen);
        if (ee == NULL || ee->encoded == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
            return 0;
        }
        /* private key first */
        if (ee->keyshare != NULL
            && !PEM_write_bio_PrivateKey(out, ee->keyshare, NULL, NULL, 0,
                NULL, NULL)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        if (PEM_write_bio(out, PEM_STRING_ECHCONFIG, NULL,
                ee->encoded, (long)ee->encoded_len)
            <= 0) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    } else {
        /* catenate the encodings into one */
        if ((epkt_mem = BUF_MEM_new()) == NULL
            || !BUF_MEM_grow(epkt_mem, OSSL_ECH_MAX_ECHCONFIG_LEN)
            || !WPACKET_init(&epkt, epkt_mem)
            || !WPACKET_start_sub_packet_u16(&epkt)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        for (chosen = 0; chosen != num; chosen++) {
            ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, chosen);
            if (ee == NULL || ee->encoded == NULL) {
                ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
                return 0;
            }
            if (!WPACKET_memcpy(&epkt, ee->encoded, ee->encoded_len)) {
                ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
                goto err;
            }
        }
        if (!WPACKET_close(&epkt)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        if (!WPACKET_get_total_written(&epkt, &allencoded_len)) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        if (PEM_write_bio(out, PEM_STRING_ECHCONFIG, NULL,
                (unsigned char *)epkt_mem->data,
                (long)allencoded_len)
            <= 0) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    }
    rv = 1;
err:
    WPACKET_cleanup(&epkt);
    BUF_MEM_free(epkt_mem);
    return rv;
}

int OSSL_ECHSTORE_read_echconfiglist(OSSL_ECHSTORE *es, BIO *in)
{
    return ech_read_priv_echconfiglist(es, in, NULL, 0);
}

int OSSL_ECHSTORE_get1_info(OSSL_ECHSTORE *es, int index, time_t *loaded_secs,
    char **public_name, char **echconfig,
    int *has_private, int *for_retry)
{
    OSSL_ECHSTORE_ENTRY *ee = NULL;
    unsigned int j = 0;
    int num = 0;
    BIO *out = NULL;
    time_t now = time(0);
    size_t ehlen;
    unsigned char *ignore = NULL;

    if (es == NULL || loaded_secs == NULL || public_name == NULL
        || echconfig == NULL || has_private == NULL || for_retry == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    num = (es->entries == NULL ? 0 : sk_OSSL_ECHSTORE_ENTRY_num(es->entries));
    if (num == 0 || index < 0 || index >= num) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, index);
    if (ee == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    *loaded_secs = now - ee->loadtime;
    *public_name = NULL;
    *echconfig = NULL;
    if (ee->public_name != NULL) {
        *public_name = OPENSSL_strdup(ee->public_name);
        if (*public_name == NULL)
            goto err;
    }
    *has_private = (ee->keyshare == NULL ? 0 : 1);
    *for_retry = ee->for_retry;
    /* Now "print" the ECHConfigList */
    out = BIO_new(BIO_s_mem());
    if (out == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    if (ee->version != OSSL_ECH_RFC9849_VERSION) {
        /* just note we don't support that one today */
        BIO_printf(out, "[Unsupported version (%04x)]", ee->version);
    } else {
        /* version, config_id, public_name, and kem */
        BIO_printf(out, "[%04x,%02x,%s,[", ee->version, ee->config_id,
            ee->public_name != NULL ? (char *)ee->public_name : "NULL");
        /* ciphersuites */
        for (j = 0; j != ee->nsuites; j++) {
            BIO_printf(out, "%04x,%04x,%04x", ee->suites[j].kem_id,
                ee->suites[j].kdf_id, ee->suites[j].aead_id);
            if (j < (ee->nsuites - 1))
                BIO_printf(out, ",");
        }
        BIO_printf(out, "],");
        /* public key */
        for (j = 0; j != ee->pub_len; j++)
            BIO_printf(out, "%02x", ee->pub[j]);
        /* max name length and (only) number of extensions */
        BIO_printf(out, ",%02x,%02x]", ee->max_name_length,
            ee->exts == NULL ? 0 : sk_OSSL_ECHEXT_num(ee->exts));
    }
    ehlen = BIO_get_mem_data(out, &ignore);
    if (ehlen > INT_MAX)
        goto err;
    *echconfig = OPENSSL_malloc(ehlen + 1);
    if (*echconfig == NULL)
        goto err;
    if (BIO_read(out, *echconfig, (int)ehlen) <= 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    (*echconfig)[ehlen] = '\0';
    BIO_free(out);
    return 1;
err:
    BIO_free(out);
    OPENSSL_free(*public_name);
    *public_name = NULL;
    OPENSSL_free(*echconfig);
    *echconfig = NULL;
    return 0;
}

int OSSL_ECHSTORE_downselect(OSSL_ECHSTORE *es, int index)
{
    OSSL_ECHSTORE_ENTRY *ee = NULL;
    int i, num = 0, chosen = OSSL_ECHSTORE_ALL;

    if (es == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    num = (es->entries == NULL ? 0 : sk_OSSL_ECHSTORE_ENTRY_num(es->entries));
    if (num == 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    if (index <= OSSL_ECHSTORE_ALL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    if (index == OSSL_ECHSTORE_LAST) {
        chosen = num - 1;
    } else if (index >= num) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    } else {
        chosen = index;
    }
    for (i = num - 1; i >= 0; i--) {
        if (i == chosen)
            continue;
        ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, i);
        ossl_echstore_entry_free(ee);
        sk_OSSL_ECHSTORE_ENTRY_delete(es->entries, i);
    }
    return 1;
}

int OSSL_ECHSTORE_set1_key_and_read_pem(OSSL_ECHSTORE *es, EVP_PKEY *priv,
    BIO *in, int for_retry)
{
    unsigned char *b64 = NULL;
    long b64len = 0;
    BIO *b64bio = NULL;
    int rv = 0;
    char *pname = NULL, *pheader = NULL;

    /* we allow for a NULL private key */
    if (es == NULL || in == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    if (PEM_read_bio(in, &pname, &pheader, &b64, &b64len) != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    if (pname == NULL || strcmp(pname, PEM_STRING_ECHCONFIG) != 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    b64bio = BIO_new(BIO_s_mem());
    if (b64bio == NULL
        || BIO_write(b64bio, b64, b64len) <= 0
        || ech_read_priv_echconfiglist(es, b64bio, priv, for_retry) != 1) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    rv = 1;
err:
    OPENSSL_free(pname);
    OPENSSL_free(pheader);
    BIO_free_all(b64bio);
    OPENSSL_free(b64);
    return rv;
}

int OSSL_ECHSTORE_read_pem(OSSL_ECHSTORE *es, BIO *in, int for_retry)
{
    EVP_PKEY *priv = NULL;
    int rv = 0;
    BIO *fbio = BIO_new(BIO_f_buffer());

    if (fbio == NULL || es == NULL || in == NULL) {
        BIO_free_all(fbio);
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    /*
     * Read private key then handoff to set1_key_and_read_pem.
     * We allow for no private key as an option, to handle that
     * the BIO_f_buffer allows us to seek back to the start.
     */
    BIO_push(fbio, in);
    if (!PEM_read_bio_PrivateKey_ex(fbio, &priv, NULL, NULL, es->libctx, es->propq)
        && BIO_seek(fbio, 0) < 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        goto err;
    }
    rv = OSSL_ECHSTORE_set1_key_and_read_pem(es, priv, fbio, for_retry);
err:
    EVP_PKEY_free(priv);
    BIO_pop(fbio);
    BIO_free_all(fbio);
    return rv;
}

int OSSL_ECHSTORE_num_entries(const OSSL_ECHSTORE *es, int *numentries)
{
    if (es == NULL || numentries == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    *numentries = (es->entries == NULL ? 0 : sk_OSSL_ECHSTORE_ENTRY_num(es->entries));
    return 1;
}

int OSSL_ECHSTORE_num_keys(OSSL_ECHSTORE *es, int *numkeys)
{
    int i, num = 0, count = 0;
    OSSL_ECHSTORE_ENTRY *ee = NULL;

    if (es == NULL || numkeys == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    num = (es->entries == NULL ? 0 : sk_OSSL_ECHSTORE_ENTRY_num(es->entries));
    for (i = 0; i != num; i++) {
        ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, i);
        if (ee == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        count += (ee->keyshare != NULL);
    }
    *numkeys = count;
    return 1;
}

int OSSL_ECHSTORE_flush_keys(OSSL_ECHSTORE *es, time_t age)
{
    OSSL_ECHSTORE_ENTRY *ee = NULL;
    int i, num = 0;
    time_t now = time(0);

    if (es == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    num = (es->entries == NULL ? 0 : sk_OSSL_ECHSTORE_ENTRY_num(es->entries));
    if (num == 0) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
        return 0;
    }
    for (i = num - 1; i >= 0; i--) {
        ee = sk_OSSL_ECHSTORE_ENTRY_value(es->entries, i);
        if (ee == NULL) {
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_INVALID_ARGUMENT);
            return 0;
        }
        if (ee->keyshare != NULL && ee->loadtime + age <= now) {
            ossl_echstore_entry_free(ee);
            sk_OSSL_ECHSTORE_ENTRY_delete(es->entries, i);
        }
    }
    return 1;
}
