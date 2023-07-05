/*
 * Copyright 2016-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdlib.h>
#include "ssl_local.h"
#include "internal/ktls.h"
#include "record/record_local.h"
#include "internal/cryptlib.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>

#define TLS13_MAX_LABEL_LEN     249

#ifdef CHARSET_EBCDIC
static const unsigned char label_prefix[] = { 0x74, 0x6C, 0x73, 0x31, 0x33, 0x20, 0x00 };
#else
static const unsigned char label_prefix[] = "tls13 ";
#endif

/*
 * Given a |secret|; a |label| of length |labellen|; and |data| of length
 * |datalen| (e.g. typically a hash of the handshake messages), derive a new
 * secret |outlen| bytes long and store it in the location pointed to be |out|.
 * The |data| value may be zero length. Any errors will be treated as fatal if
 * |fatal| is set. Returns 1 on success  0 on failure.
 */
int tls13_hkdf_expand(SSL *s, const EVP_MD *md, const unsigned char *secret,
                      const unsigned char *label, size_t labellen,
                      const unsigned char *data, size_t datalen,
                      unsigned char *out, size_t outlen, int fatal)
{
    EVP_KDF *kdf = EVP_KDF_fetch(s->ctx->libctx, OSSL_KDF_NAME_TLS1_3_KDF,
                                 s->ctx->propq);
    EVP_KDF_CTX *kctx;
    OSSL_PARAM params[7], *p = params;
    int mode = EVP_PKEY_HKDEF_MODE_EXPAND_ONLY;
    const char *mdname = EVP_MD_get0_name(md);
    int ret;
    size_t hashlen;

    kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (kctx == NULL)
        return 0;

    if (labellen > TLS13_MAX_LABEL_LEN) {
        if (fatal) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        } else {
            /*
             * Probably we have been called from SSL_export_keying_material(),
             * or SSL_export_keying_material_early().
             */
            ERR_raise(ERR_LIB_SSL, SSL_R_TLS_ILLEGAL_EXPORTER_LABEL);
        }
        EVP_KDF_CTX_free(kctx);
        return 0;
    }

    if ((ret = EVP_MD_get_size(md)) <= 0) {
        EVP_KDF_CTX_free(kctx);
        if (fatal)
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        else
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    hashlen = (size_t)ret;

    *p++ = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_MODE, &mode);
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                            (char *)mdname, 0);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                             (unsigned char *)secret, hashlen);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PREFIX,
                                             (unsigned char *)label_prefix,
                                             sizeof(label_prefix) - 1);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_LABEL,
                                             (unsigned char *)label, labellen);
    if (data != NULL)
        *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_DATA,
                                                 (unsigned char *)data,
                                                 datalen);
    *p++ = OSSL_PARAM_construct_end();

    ret = EVP_KDF_derive(kctx, out, outlen, params) <= 0;
    EVP_KDF_CTX_free(kctx);

    if (ret != 0) {
        if (fatal)
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        else
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
    }

    return ret == 0;
}

/*
 * Given a |secret| generate a |key| of length |keylen| bytes. Returns 1 on
 * success  0 on failure.
 */
int tls13_derive_key(SSL *s, const EVP_MD *md, const unsigned char *secret,
                     unsigned char *key, size_t keylen)
{
#ifdef CHARSET_EBCDIC
  static const unsigned char keylabel[] ={ 0x6B, 0x65, 0x79, 0x00 };
#else
  static const unsigned char keylabel[] = "key";
#endif

    return tls13_hkdf_expand(s, md, secret, keylabel, sizeof(keylabel) - 1,
                             NULL, 0, key, keylen, 1);
}

/*
 * Given a |secret| generate an |iv| of length |ivlen| bytes. Returns 1 on
 * success  0 on failure.
 */
int tls13_derive_iv(SSL *s, const EVP_MD *md, const unsigned char *secret,
                    unsigned char *iv, size_t ivlen)
{
#ifdef CHARSET_EBCDIC
  static const unsigned char ivlabel[] = { 0x69, 0x76, 0x00 };
#else
  static const unsigned char ivlabel[] = "iv";
#endif

    return tls13_hkdf_expand(s, md, secret, ivlabel, sizeof(ivlabel) - 1,
                             NULL, 0, iv, ivlen, 1);
}

int tls13_derive_finishedkey(SSL *s, const EVP_MD *md,
                             const unsigned char *secret,
                             unsigned char *fin, size_t finlen)
{
#ifdef CHARSET_EBCDIC
  static const unsigned char finishedlabel[] = { 0x66, 0x69, 0x6E, 0x69, 0x73, 0x68, 0x65, 0x64, 0x00 };
#else
  static const unsigned char finishedlabel[] = "finished";
#endif

    return tls13_hkdf_expand(s, md, secret, finishedlabel,
                             sizeof(finishedlabel) - 1, NULL, 0, fin, finlen, 1);
}

/*
 * Given the previous secret |prevsecret| and a new input secret |insecret| of
 * length |insecretlen|, generate a new secret and store it in the location
 * pointed to by |outsecret|. Returns 1 on success  0 on failure.
 */
int tls13_generate_secret(SSL *s, const EVP_MD *md,
                          const unsigned char *prevsecret,
                          const unsigned char *insecret,
                          size_t insecretlen,
                          unsigned char *outsecret)
{
    size_t mdlen;
    int mdleni;
    int ret;
    EVP_KDF *kdf;
    EVP_KDF_CTX *kctx;
    OSSL_PARAM params[7], *p = params;
    int mode = EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY;
    const char *mdname = EVP_MD_get0_name(md);
#ifdef CHARSET_EBCDIC
    static const char derived_secret_label[] = { 0x64, 0x65, 0x72, 0x69, 0x76, 0x65, 0x64, 0x00 };
#else
    static const char derived_secret_label[] = "derived";
#endif

    kdf = EVP_KDF_fetch(s->ctx->libctx, OSSL_KDF_NAME_TLS1_3_KDF, s->ctx->propq);
    kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (kctx == NULL) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    mdleni = EVP_MD_get_size(md);
    /* Ensure cast to size_t is safe */
    if (!ossl_assert(mdleni >= 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        EVP_KDF_CTX_free(kctx);
        return 0;
    }
    mdlen = (size_t)mdleni;

    *p++ = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_MODE, &mode);
    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                            (char *)mdname, 0);
    if (insecret != NULL)
        *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                 (unsigned char *)insecret,
                                                 insecretlen);
    if (prevsecret != NULL)
        *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                 (unsigned char *)prevsecret, mdlen);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PREFIX,
                                             (unsigned char *)label_prefix,
                                             sizeof(label_prefix) - 1);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_LABEL,
                                             (unsigned char *)derived_secret_label,
                                             sizeof(derived_secret_label) - 1);
    *p++ = OSSL_PARAM_construct_end();

    ret = EVP_KDF_derive(kctx, outsecret, mdlen, params) <= 0;

    if (ret != 0)
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);

    EVP_KDF_CTX_free(kctx);
    return ret == 0;
}

/*
 * Given an input secret |insecret| of length |insecretlen| generate the
 * handshake secret. This requires the early secret to already have been
 * generated. Returns 1 on success  0 on failure.
 */
int tls13_generate_handshake_secret(SSL *s, const unsigned char *insecret,
                                size_t insecretlen)
{
    /* Calls SSLfatal() if required */
    return tls13_generate_secret(s, ssl_handshake_md(s), s->early_secret,
                                 insecret, insecretlen,
                                 (unsigned char *)&s->handshake_secret);
}

/*
 * Given the handshake secret |prev| of length |prevlen| generate the master
 * secret and store its length in |*secret_size|. Returns 1 on success  0 on
 * failure.
 */
int tls13_generate_master_secret(SSL *s, unsigned char *out,
                                 unsigned char *prev, size_t prevlen,
                                 size_t *secret_size)
{
    const EVP_MD *md = ssl_handshake_md(s);

    *secret_size = EVP_MD_get_size(md);
    /* Calls SSLfatal() if required */
    return tls13_generate_secret(s, md, prev, NULL, 0, out);
}

/*
 * Generates the mac for the Finished message. Returns the length of the MAC or
 * 0 on error.
 */
size_t tls13_final_finish_mac(SSL *s, const char *str, size_t slen,
                             unsigned char *out)
{
    const EVP_MD *md = ssl_handshake_md(s);
    const char *mdname = EVP_MD_get0_name(md);
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned char finsecret[EVP_MAX_MD_SIZE];
    unsigned char *key = NULL;
    size_t len = 0, hashlen;
    OSSL_PARAM params[2], *p = params;

    if (md == NULL)
        return 0;

    /* Safe to cast away const here since we're not "getting" any data */
    if (s->ctx->propq != NULL)
        *p++ = OSSL_PARAM_construct_utf8_string(OSSL_ALG_PARAM_PROPERTIES,
                                                (char *)s->ctx->propq,
                                                0);
    *p = OSSL_PARAM_construct_end();

    if (!ssl_handshake_hash(s, hash, sizeof(hash), &hashlen)) {
        /* SSLfatal() already called */
        goto err;
    }

    if (str == s->method->ssl3_enc->server_finished_label) {
        key = s->server_finished_secret;
    } else if (SSL_IS_FIRST_HANDSHAKE(s)) {
        key = s->client_finished_secret;
    } else {
        if (!tls13_derive_finishedkey(s, md,
                                      s->client_app_traffic_secret,
                                      finsecret, hashlen))
            goto err;
        key = finsecret;
    }

    if (!EVP_Q_mac(s->ctx->libctx, "HMAC", s->ctx->propq, mdname,
                   params, key, hashlen, hash, hashlen,
                   /* outsize as per sizeof(peer_finish_md) */
                   out, EVP_MAX_MD_SIZE * 2, &len)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

 err:
    OPENSSL_cleanse(finsecret, sizeof(finsecret));
    return len;
}

/*
 * There isn't really a key block in TLSv1.3, but we still need this function
 * for initialising the cipher and hash. Returns 1 on success or 0 on failure.
 */
int tls13_setup_key_block(SSL *s)
{
    const EVP_CIPHER *c;
    const EVP_MD *hash;

    s->session->cipher = s->s3.tmp.new_cipher;
    if (!ssl_cipher_get_evp(s->ctx, s->session, &c, &hash, NULL, NULL, NULL,
                            0)) {
        /* Error is already recorded */
        SSLfatal_alert(s, SSL_AD_INTERNAL_ERROR);
        return 0;
    }

    ssl_evp_cipher_free(s->s3.tmp.new_sym_enc);
    s->s3.tmp.new_sym_enc = c;
    ssl_evp_md_free(s->s3.tmp.new_hash);
    s->s3.tmp.new_hash = hash;

    return 1;
}

static int derive_secret_key_and_iv(SSL *s, int sending, const EVP_MD *md,
                                    const EVP_CIPHER *ciph,
                                    const unsigned char *insecret,
                                    const unsigned char *hash,
                                    const unsigned char *label,
                                    size_t labellen, unsigned char *secret,
                                    unsigned char *key, unsigned char *iv,
                                    EVP_CIPHER_CTX *ciph_ctx)
{
    size_t ivlen, keylen, taglen;
    int hashleni = EVP_MD_get_size(md);
    size_t hashlen;

    /* Ensure cast to size_t is safe */
    if (!ossl_assert(hashleni >= 0)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
        return 0;
    }
    hashlen = (size_t)hashleni;

    if (!tls13_hkdf_expand(s, md, insecret, label, labellen, hash, hashlen,
                           secret, hashlen, 1)) {
        /* SSLfatal() already called */
        return 0;
    }

    keylen = EVP_CIPHER_get_key_length(ciph);
    if (EVP_CIPHER_get_mode(ciph) == EVP_CIPH_CCM_MODE) {
        uint32_t algenc;

        ivlen = EVP_CCM_TLS_IV_LEN;
        if (s->s3.tmp.new_cipher != NULL) {
            algenc = s->s3.tmp.new_cipher->algorithm_enc;
        } else if (s->session->cipher != NULL) {
            /* We've not selected a cipher yet - we must be doing early data */
            algenc = s->session->cipher->algorithm_enc;
        } else if (s->psksession != NULL && s->psksession->cipher != NULL) {
            /* We must be doing early data with out-of-band PSK */
            algenc = s->psksession->cipher->algorithm_enc;
        } else {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
            return 0;
        }
        if (algenc & (SSL_AES128CCM8 | SSL_AES256CCM8))
            taglen = EVP_CCM8_TLS_TAG_LEN;
         else
            taglen = EVP_CCM_TLS_TAG_LEN;
    } else {
        ivlen = EVP_CIPHER_get_iv_length(ciph);
        taglen = 0;
    }

    if (!tls13_derive_key(s, md, secret, key, keylen)
            || !tls13_derive_iv(s, md, secret, iv, ivlen)) {
        /* SSLfatal() already called */
        return 0;
    }

    if (EVP_CipherInit_ex(ciph_ctx, ciph, NULL, NULL, NULL, sending) <= 0
        || EVP_CIPHER_CTX_ctrl(ciph_ctx, EVP_CTRL_AEAD_SET_IVLEN, ivlen, NULL) <= 0
        || (taglen != 0 && EVP_CIPHER_CTX_ctrl(ciph_ctx, EVP_CTRL_AEAD_SET_TAG,
                                                taglen, NULL) <= 0)
        || EVP_CipherInit_ex(ciph_ctx, NULL, NULL, key, NULL, -1) <= 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
        return 0;
    }

    return 1;
}

#ifdef CHARSET_EBCDIC
static const unsigned char client_early_traffic[]       = {0x63, 0x20, 0x65, 0x20,       /*traffic*/0x74, 0x72, 0x61, 0x66, 0x66, 0x69, 0x63, 0x00};
static const unsigned char client_handshake_traffic[]   = {0x63, 0x20, 0x68, 0x73, 0x20, /*traffic*/0x74, 0x72, 0x61, 0x66, 0x66, 0x69, 0x63, 0x00};
static const unsigned char client_application_traffic[] = {0x63, 0x20, 0x61, 0x70, 0x20, /*traffic*/0x74, 0x72, 0x61, 0x66, 0x66, 0x69, 0x63, 0x00};
static const unsigned char server_handshake_traffic[]   = {0x73, 0x20, 0x68, 0x73, 0x20, /*traffic*/0x74, 0x72, 0x61, 0x66, 0x66, 0x69, 0x63, 0x00};
static const unsigned char server_application_traffic[] = {0x73, 0x20, 0x61, 0x70, 0x20, /*traffic*/0x74, 0x72, 0x61, 0x66, 0x66, 0x69, 0x63, 0x00};
static const unsigned char exporter_master_secret[] = {0x65, 0x78, 0x70, 0x20,                    /* master*/  0x6D, 0x61, 0x73, 0x74, 0x65, 0x72, 0x00};
static const unsigned char resumption_master_secret[] = {0x72, 0x65, 0x73, 0x20,                  /* master*/  0x6D, 0x61, 0x73, 0x74, 0x65, 0x72, 0x00};
static const unsigned char early_exporter_master_secret[] = {0x65, 0x20, 0x65, 0x78, 0x70, 0x20,  /* master*/  0x6D, 0x61, 0x73, 0x74, 0x65, 0x72, 0x00};
#else
static const unsigned char client_early_traffic[] = "c e traffic";
static const unsigned char client_handshake_traffic[] = "c hs traffic";
static const unsigned char client_application_traffic[] = "c ap traffic";
static const unsigned char server_handshake_traffic[] = "s hs traffic";
static const unsigned char server_application_traffic[] = "s ap traffic";
static const unsigned char exporter_master_secret[] = "exp master";
static const unsigned char resumption_master_secret[] = "res master";
static const unsigned char early_exporter_master_secret[] = "e exp master";
#endif

#ifndef OPENSSL_NO_QUIC
static int quic_change_cipher_state(SSL *s, int which)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    size_t hashlen = 0;
    int hashleni;
    int ret = 0;
    const EVP_MD *md = NULL;
    OSSL_ENCRYPTION_LEVEL level;
    int is_handshake = ((which & SSL3_CC_HANDSHAKE) == SSL3_CC_HANDSHAKE);
    int is_client_read = ((which & SSL3_CHANGE_CIPHER_CLIENT_READ) == SSL3_CHANGE_CIPHER_CLIENT_READ);
    int is_server_write = ((which & SSL3_CHANGE_CIPHER_SERVER_WRITE) == SSL3_CHANGE_CIPHER_SERVER_WRITE);
    int is_early = (which & SSL3_CC_EARLY);

    if (is_early) {
        EVP_MD_CTX *mdctx = NULL;
        long handlen;
        void *hdata;
        unsigned int hashlenui;
        const SSL_CIPHER *sslcipher = SSL_SESSION_get0_cipher(s->session);

        handlen = BIO_get_mem_data(s->s3.handshake_buffer, &hdata);
        if (handlen <= 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_HANDSHAKE_LENGTH);
            goto err;
        }

        if (s->early_data_state == SSL_EARLY_DATA_CONNECTING
                && s->max_early_data > 0
                && s->session->ext.max_early_data == 0) {
            /*
             * If we are attempting to send early data, and we've decided to
             * actually do it but max_early_data in s->session is 0 then we
             * must be using an external PSK.
             */
            if (!ossl_assert(s->psksession != NULL
                    && s->max_early_data ==
                       s->psksession->ext.max_early_data)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }
            sslcipher = SSL_SESSION_get0_cipher(s->psksession);
        }
        if (sslcipher == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_PSK);
            goto err;
        }

        /*
         * We need to calculate the handshake digest using the digest from
         * the session. We haven't yet selected our ciphersuite so we can't
         * use ssl_handshake_md().
         */
        mdctx = EVP_MD_CTX_new();
        if (mdctx == NULL) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        md = ssl_md(s->ctx, sslcipher->algorithm2);
        if (md == NULL || !EVP_DigestInit_ex(mdctx, md, NULL)
                || !EVP_DigestUpdate(mdctx, hdata, handlen)
                || !EVP_DigestFinal_ex(mdctx, hash, &hashlenui)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
            EVP_MD_CTX_free(mdctx);
            goto err;
        }
        hashlen = hashlenui;
        EVP_MD_CTX_free(mdctx);
    } else {
        md = ssl_handshake_md(s);
        if (!ssl3_digest_cached_records(s, 1)
                || !ssl_handshake_hash(s, hash, sizeof(hash), &hashlen)) {
            /* SSLfatal() already called */;
            goto err;
        }

        /* Ensure cast to size_t is safe */
        hashleni = EVP_MD_size(md);
        if (!ossl_assert(hashleni >= 0)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
            goto err;
        }
        hashlen = (size_t)hashleni;
    }

    if (is_client_read || is_server_write) {
        if (is_handshake) {
            /*
             * This looks a bit weird, since the condition is basically "the
             * server is writing" but we set both the server *and* client
             * handshake traffic keys here.  That's because there's only a fixed
             * number of change-cipher-state events in the TLS 1.3 handshake,
             * and in particular there's not an event in between when the server
             * writes encrypted handshake messages and when the client writes
             * encrypted handshake messages, so we generate both here.
             */
            level = ssl_encryption_handshake;

            if (!tls13_hkdf_expand(s, md, s->handshake_secret,
                                   client_handshake_traffic,
                                   sizeof(client_handshake_traffic)-1, hash,
                                   hashlen, s->client_hand_traffic_secret,
                                   hashlen, 1)
                || !ssl_log_secret(s, CLIENT_HANDSHAKE_LABEL,
                                   s->client_hand_traffic_secret, hashlen)
                || !tls13_derive_finishedkey(s, md,
                                             s->client_hand_traffic_secret,
                                             s->client_finished_secret, hashlen)
                || !tls13_hkdf_expand(s, md, s->handshake_secret,
                                      server_handshake_traffic,
                                      sizeof(server_handshake_traffic)-1, hash,
                                      hashlen, s->server_hand_traffic_secret,
                                      hashlen, 1)
                || !ssl_log_secret(s, SERVER_HANDSHAKE_LABEL,
                                   s->server_hand_traffic_secret, hashlen)
                || !tls13_derive_finishedkey(s, md,
                                             s->server_hand_traffic_secret,
                                             s->server_finished_secret,
                                             hashlen)) {
                /* SSLfatal() already called */
                goto err;
            }
        } else {
            /*
             * As above, we generate both sets of application traffic keys at
             * the same time.
             */
            level = ssl_encryption_application;

            if (!tls13_hkdf_expand(s, md, s->master_secret,
                                   client_application_traffic,
                                   sizeof(client_application_traffic)-1, hash,
                                   hashlen, s->client_app_traffic_secret,
                                   hashlen, 1)
                || !ssl_log_secret(s, CLIENT_APPLICATION_LABEL,
                                   s->client_app_traffic_secret, hashlen)
                || !tls13_hkdf_expand(s, md, s->master_secret,
                                      server_application_traffic,
                                      sizeof(server_application_traffic)-1,
                                      hash, hashlen,
                                      s->server_app_traffic_secret, hashlen, 1)
                || !ssl_log_secret(s, SERVER_APPLICATION_LABEL,
                                   s->server_app_traffic_secret, hashlen)) {
                /* SSLfatal() already called */
                goto err;
            }
        }
        if (!quic_set_encryption_secrets(s, level)) {
            /* SSLfatal() already called */
            goto err;
        }
        if (s->server)
            s->quic_write_level = level;
        else
            s->quic_read_level = level;
    } else {
        /* is_client_write || is_server_read */

        if (is_early) {
            level = ssl_encryption_early_data;

            if (!tls13_hkdf_expand(s, md, s->early_secret, client_early_traffic,
                                   sizeof(client_early_traffic)-1, hash,
                                   hashlen, s->client_early_traffic_secret,
                                   hashlen, 1)
                || !ssl_log_secret(s, CLIENT_EARLY_LABEL,
                                   s->client_early_traffic_secret, hashlen)
                || !quic_set_encryption_secrets(s, level)) {
                /* SSLfatal() already called */
                goto err;
            }
        } else if (is_handshake) {
            level = ssl_encryption_handshake;
        } else {
            level = ssl_encryption_application;
            /*
             * We also create the resumption master secret, but this time use the
             * hash for the whole handshake including the Client Finished
             */
            if (!tls13_hkdf_expand(s, md, s->master_secret,
                                   resumption_master_secret,
                                   sizeof(resumption_master_secret)-1, hash,
                                   hashlen, s->resumption_master_secret,
                                   hashlen, 1)) {
                /* SSLfatal() already called */
                goto err;
            }
        }

        if (level != ssl_encryption_early_data) {
            if (s->server)
                s->quic_read_level = level;
            else
                s->quic_write_level = level;
        }
    }

    ret = 1;
 err:
    return ret;
}
#endif /* OPENSSL_NO_QUIC */

int tls13_change_cipher_state(SSL *s, int which)
{
    unsigned char *iv;
    unsigned char key[EVP_MAX_KEY_LENGTH];
    unsigned char secret[EVP_MAX_MD_SIZE];
    unsigned char hashval[EVP_MAX_MD_SIZE];
    unsigned char *hash = hashval;
    unsigned char *insecret;
    unsigned char *finsecret = NULL;
    const char *log_label = NULL;
    EVP_CIPHER_CTX *ciph_ctx;
    size_t finsecretlen = 0;
    const unsigned char *label;
    size_t labellen, hashlen = 0;
    int ret = 0;
    const EVP_MD *md = NULL;
    const EVP_CIPHER *cipher = NULL;
#if !defined(OPENSSL_NO_KTLS) && defined(OPENSSL_KTLS_TLS13)
    ktls_crypto_info_t crypto_info;
    BIO *bio;
#endif

#ifndef OPENSSL_NO_QUIC
    if (SSL_IS_QUIC(s))
        return quic_change_cipher_state(s, which);
#endif

    if (which & SSL3_CC_READ) {
        if (s->enc_read_ctx != NULL) {
            EVP_CIPHER_CTX_reset(s->enc_read_ctx);
        } else {
            s->enc_read_ctx = EVP_CIPHER_CTX_new();
            if (s->enc_read_ctx == NULL) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE);
                goto err;
            }
        }
        ciph_ctx = s->enc_read_ctx;
        iv = s->read_iv;

        RECORD_LAYER_reset_read_sequence(&s->rlayer);
    } else {
        s->statem.enc_write_state = ENC_WRITE_STATE_INVALID;
        if (s->enc_write_ctx != NULL) {
            EVP_CIPHER_CTX_reset(s->enc_write_ctx);
        } else {
            s->enc_write_ctx = EVP_CIPHER_CTX_new();
            if (s->enc_write_ctx == NULL) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE);
                goto err;
            }
        }
        ciph_ctx = s->enc_write_ctx;
        iv = s->write_iv;

        RECORD_LAYER_reset_write_sequence(&s->rlayer);
    }

    if (((which & SSL3_CC_CLIENT) && (which & SSL3_CC_WRITE))
            || ((which & SSL3_CC_SERVER) && (which & SSL3_CC_READ))) {
        if (which & SSL3_CC_EARLY) {
            EVP_MD_CTX *mdctx = NULL;
            long handlen;
            void *hdata;
            unsigned int hashlenui;
            const SSL_CIPHER *sslcipher = SSL_SESSION_get0_cipher(s->session);

            insecret = s->early_secret;
            label = client_early_traffic;
            labellen = sizeof(client_early_traffic) - 1;
            log_label = CLIENT_EARLY_LABEL;

            handlen = BIO_get_mem_data(s->s3.handshake_buffer, &hdata);
            if (handlen <= 0) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_HANDSHAKE_LENGTH);
                goto err;
            }

            if (s->early_data_state == SSL_EARLY_DATA_CONNECTING
                    && s->max_early_data > 0
                    && s->session->ext.max_early_data == 0) {
                /*
                 * If we are attempting to send early data, and we've decided to
                 * actually do it but max_early_data in s->session is 0 then we
                 * must be using an external PSK.
                 */
                if (!ossl_assert(s->psksession != NULL
                        && s->max_early_data ==
                           s->psksession->ext.max_early_data)) {
                    SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                    goto err;
                }
                sslcipher = SSL_SESSION_get0_cipher(s->psksession);
            }
            if (sslcipher == NULL) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_R_BAD_PSK);
                goto err;
            }

            /*
             * We need to calculate the handshake digest using the digest from
             * the session. We haven't yet selected our ciphersuite so we can't
             * use ssl_handshake_md().
             */
            mdctx = EVP_MD_CTX_new();
            if (mdctx == NULL) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE);
                goto err;
            }

            /*
             * This ups the ref count on cipher so we better make sure we free
             * it again
             */
            if (!ssl_cipher_get_evp_cipher(s->ctx, sslcipher, &cipher)) {
                /* Error is already recorded */
                SSLfatal_alert(s, SSL_AD_INTERNAL_ERROR);
                EVP_MD_CTX_free(mdctx);
                goto err;
            }

            md = ssl_md(s->ctx, sslcipher->algorithm2);
            if (md == NULL || !EVP_DigestInit_ex(mdctx, md, NULL)
                    || !EVP_DigestUpdate(mdctx, hdata, handlen)
                    || !EVP_DigestFinal_ex(mdctx, hashval, &hashlenui)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                EVP_MD_CTX_free(mdctx);
                goto err;
            }
            hashlen = hashlenui;
            EVP_MD_CTX_free(mdctx);

            if (!tls13_hkdf_expand(s, md, insecret,
                                   early_exporter_master_secret,
                                   sizeof(early_exporter_master_secret) - 1,
                                   hashval, hashlen,
                                   s->early_exporter_master_secret, hashlen,
                                   1)) {
                SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
                goto err;
            }

            if (!ssl_log_secret(s, EARLY_EXPORTER_SECRET_LABEL,
                                s->early_exporter_master_secret, hashlen)) {
                /* SSLfatal() already called */
                goto err;
            }
        } else if (which & SSL3_CC_HANDSHAKE) {
            insecret = s->handshake_secret;
            finsecret = s->client_finished_secret;
            finsecretlen = EVP_MD_get_size(ssl_handshake_md(s));
            label = client_handshake_traffic;
            labellen = sizeof(client_handshake_traffic) - 1;
            log_label = CLIENT_HANDSHAKE_LABEL;
            /*
             * The handshake hash used for the server read/client write handshake
             * traffic secret is the same as the hash for the server
             * write/client read handshake traffic secret. However, if we
             * processed early data then we delay changing the server
             * read/client write cipher state until later, and the handshake
             * hashes have moved on. Therefore we use the value saved earlier
             * when we did the server write/client read change cipher state.
             */
            hash = s->handshake_traffic_hash;
        } else {
            insecret = s->master_secret;
            label = client_application_traffic;
            labellen = sizeof(client_application_traffic) - 1;
            log_label = CLIENT_APPLICATION_LABEL;
            /*
             * For this we only use the handshake hashes up until the server
             * Finished hash. We do not include the client's Finished, which is
             * what ssl_handshake_hash() would give us. Instead we use the
             * previously saved value.
             */
            hash = s->server_finished_hash;
        }
    } else {
        /* Early data never applies to client-read/server-write */
        if (which & SSL3_CC_HANDSHAKE) {
            insecret = s->handshake_secret;
            finsecret = s->server_finished_secret;
            finsecretlen = EVP_MD_get_size(ssl_handshake_md(s));
            label = server_handshake_traffic;
            labellen = sizeof(server_handshake_traffic) - 1;
            log_label = SERVER_HANDSHAKE_LABEL;
        } else {
            insecret = s->master_secret;
            label = server_application_traffic;
            labellen = sizeof(server_application_traffic) - 1;
            log_label = SERVER_APPLICATION_LABEL;
        }
    }

    if (!(which & SSL3_CC_EARLY)) {
        md = ssl_handshake_md(s);
        cipher = s->s3.tmp.new_sym_enc;
        if (!ssl3_digest_cached_records(s, 1)
                || !ssl_handshake_hash(s, hashval, sizeof(hashval), &hashlen)) {
            /* SSLfatal() already called */;
            goto err;
        }
    }

    /*
     * Save the hash of handshakes up to now for use when we calculate the
     * client application traffic secret
     */
    if (label == server_application_traffic)
        memcpy(s->server_finished_hash, hashval, hashlen);

    if (label == server_handshake_traffic)
        memcpy(s->handshake_traffic_hash, hashval, hashlen);

    if (label == client_application_traffic) {
        /*
         * We also create the resumption master secret, but this time use the
         * hash for the whole handshake including the Client Finished
         */
        if (!tls13_hkdf_expand(s, ssl_handshake_md(s), insecret,
                               resumption_master_secret,
                               sizeof(resumption_master_secret) - 1,
                               hashval, hashlen, s->resumption_master_secret,
                               hashlen, 1)) {
            /* SSLfatal() already called */
            goto err;
        }
    }

    /* check whether cipher is known */
    if(!ossl_assert(cipher != NULL))
        goto err;

    if (!derive_secret_key_and_iv(s, which & SSL3_CC_WRITE, md, cipher,
                                  insecret, hash, label, labellen, secret, key,
                                  iv, ciph_ctx)) {
        /* SSLfatal() already called */
        goto err;
    }

    if (label == server_application_traffic) {
        memcpy(s->server_app_traffic_secret, secret, hashlen);
        /* Now we create the exporter master secret */
        if (!tls13_hkdf_expand(s, ssl_handshake_md(s), insecret,
                               exporter_master_secret,
                               sizeof(exporter_master_secret) - 1,
                               hash, hashlen, s->exporter_master_secret,
                               hashlen, 1)) {
            /* SSLfatal() already called */
            goto err;
        }

        if (!ssl_log_secret(s, EXPORTER_SECRET_LABEL, s->exporter_master_secret,
                            hashlen)) {
            /* SSLfatal() already called */
            goto err;
        }
    } else if (label == client_application_traffic)
        memcpy(s->client_app_traffic_secret, secret, hashlen);

    if (!ssl_log_secret(s, log_label, secret, hashlen)) {
        /* SSLfatal() already called */
        goto err;
    }

    if (finsecret != NULL
            && !tls13_derive_finishedkey(s, ssl_handshake_md(s), secret,
                                         finsecret, finsecretlen)) {
        /* SSLfatal() already called */
        goto err;
    }

    if (!s->server && label == client_early_traffic)
        s->statem.enc_write_state = ENC_WRITE_STATE_WRITE_PLAIN_ALERTS;
    else
        s->statem.enc_write_state = ENC_WRITE_STATE_VALID;
#ifndef OPENSSL_NO_KTLS
# if defined(OPENSSL_KTLS_TLS13)
    if (!(which & SSL3_CC_WRITE)
            || !(which & SSL3_CC_APPLICATION)
            || (s->options & SSL_OP_ENABLE_KTLS) == 0)
        goto skip_ktls;

    /* ktls supports only the maximum fragment size */
    if (ssl_get_max_send_fragment(s) != SSL3_RT_MAX_PLAIN_LENGTH)
        goto skip_ktls;

    /* ktls does not support record padding */
    if (s->record_padding_cb != NULL)
        goto skip_ktls;

    /* check that cipher is supported */
    if (!ktls_check_supported_cipher(s, cipher, ciph_ctx))
        goto skip_ktls;

    bio = s->wbio;

    if (!ossl_assert(bio != NULL)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    /* All future data will get encrypted by ktls. Flush the BIO or skip ktls */
    if (BIO_flush(bio) <= 0)
        goto skip_ktls;

    /* configure kernel crypto structure */
    if (!ktls_configure_crypto(s, cipher, ciph_ctx,
                               RECORD_LAYER_get_write_sequence(&s->rlayer),
                               &crypto_info, NULL, iv, key, NULL, 0))
        goto skip_ktls;

    /* ktls works with user provided buffers directly */
    if (BIO_set_ktls(bio, &crypto_info, which & SSL3_CC_WRITE))
        ssl3_release_write_buffer(s);
skip_ktls:
# endif
#endif

    ret = 1;
 err:
    if ((which & SSL3_CC_EARLY) != 0) {
        /* We up-refed this so now we need to down ref */
        ssl_evp_cipher_free(cipher);
    }
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(secret, sizeof(secret));
    return ret;
}

int tls13_update_key(SSL *s, int sending)
{
#ifdef CHARSET_EBCDIC
  static const unsigned char application_traffic[] = { 0x74, 0x72 ,0x61 ,0x66 ,0x66 ,0x69 ,0x63 ,0x20 ,0x75 ,0x70 ,0x64, 0x00};
#else
  static const unsigned char application_traffic[] = "traffic upd";
#endif
    const EVP_MD *md = ssl_handshake_md(s);
    size_t hashlen;
    unsigned char key[EVP_MAX_KEY_LENGTH];
    unsigned char *insecret, *iv;
    unsigned char secret[EVP_MAX_MD_SIZE];
    char *log_label;
    EVP_CIPHER_CTX *ciph_ctx;
    int ret = 0, l;

    if ((l = EVP_MD_get_size(md)) <= 0) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    hashlen = (size_t)l;

    if (s->server == sending)
        insecret = s->server_app_traffic_secret;
    else
        insecret = s->client_app_traffic_secret;

    if (sending) {
        s->statem.enc_write_state = ENC_WRITE_STATE_INVALID;
        iv = s->write_iv;
        ciph_ctx = s->enc_write_ctx;
        RECORD_LAYER_reset_write_sequence(&s->rlayer);
    } else {
        iv = s->read_iv;
        ciph_ctx = s->enc_read_ctx;
        RECORD_LAYER_reset_read_sequence(&s->rlayer);
    }

    if (!derive_secret_key_and_iv(s, sending, md,
                                  s->s3.tmp.new_sym_enc, insecret, NULL,
                                  application_traffic,
                                  sizeof(application_traffic) - 1, secret, key,
                                  iv, ciph_ctx)) {
        /* SSLfatal() already called */
        goto err;
    }

    memcpy(insecret, secret, hashlen);

    /* Call Key log on successful traffic secret update */
    log_label = s->server == sending ? SERVER_APPLICATION_N_LABEL : CLIENT_APPLICATION_N_LABEL;
    if (!ssl_log_secret(s, log_label, secret, hashlen)) {
        /* SSLfatal() already called */
        goto err;
    }

    s->statem.enc_write_state = ENC_WRITE_STATE_VALID;
    ret = 1;
 err:
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(secret, sizeof(secret));
    return ret;
}

int tls13_alert_code(int code)
{
    /* There are 2 additional alerts in TLSv1.3 compared to TLSv1.2 */
    if (code == SSL_AD_MISSING_EXTENSION || code == SSL_AD_CERTIFICATE_REQUIRED)
        return code;

    return tls1_alert_code(code);
}

int tls13_export_keying_material(SSL *s, unsigned char *out, size_t olen,
                                 const char *label, size_t llen,
                                 const unsigned char *context,
                                 size_t contextlen, int use_context)
{
    unsigned char exportsecret[EVP_MAX_MD_SIZE];
#ifdef CHARSET_EBCDIC
    static const unsigned char exporterlabel[] = {0x65, 0x78, 0x70, 0x6F, 0x72, 0x74, 0x65, 0x72, 0x00};
#else
    static const unsigned char exporterlabel[] = "exporter";
#endif
    unsigned char hash[EVP_MAX_MD_SIZE], data[EVP_MAX_MD_SIZE];
    const EVP_MD *md = ssl_handshake_md(s);
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int hashsize, datalen;
    int ret = 0;

    if (ctx == NULL || md == NULL || !ossl_statem_export_allowed(s))
        goto err;

    if (!use_context)
        contextlen = 0;

    if (EVP_DigestInit_ex(ctx, md, NULL) <= 0
            || EVP_DigestUpdate(ctx, context, contextlen) <= 0
            || EVP_DigestFinal_ex(ctx, hash, &hashsize) <= 0
            || EVP_DigestInit_ex(ctx, md, NULL) <= 0
            || EVP_DigestFinal_ex(ctx, data, &datalen) <= 0
            || !tls13_hkdf_expand(s, md, s->exporter_master_secret,
                                  (const unsigned char *)label, llen,
                                  data, datalen, exportsecret, hashsize, 0)
            || !tls13_hkdf_expand(s, md, exportsecret, exporterlabel,
                                  sizeof(exporterlabel) - 1, hash, hashsize,
                                  out, olen, 0))
        goto err;

    ret = 1;
 err:
    EVP_MD_CTX_free(ctx);
    return ret;
}

int tls13_export_keying_material_early(SSL *s, unsigned char *out, size_t olen,
                                       const char *label, size_t llen,
                                       const unsigned char *context,
                                       size_t contextlen)
{
#ifdef CHARSET_EBCDIC
  static const unsigned char exporterlabel[] = {0x65, 0x78, 0x70, 0x6F, 0x72, 0x74, 0x65, 0x72, 0x00};
#else
  static const unsigned char exporterlabel[] = "exporter";
#endif
    unsigned char exportsecret[EVP_MAX_MD_SIZE];
    unsigned char hash[EVP_MAX_MD_SIZE], data[EVP_MAX_MD_SIZE];
    const EVP_MD *md;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int hashsize, datalen;
    int ret = 0;
    const SSL_CIPHER *sslcipher;

    if (ctx == NULL || !ossl_statem_export_early_allowed(s))
        goto err;

    if (!s->server && s->max_early_data > 0
            && s->session->ext.max_early_data == 0)
        sslcipher = SSL_SESSION_get0_cipher(s->psksession);
    else
        sslcipher = SSL_SESSION_get0_cipher(s->session);

    md = ssl_md(s->ctx, sslcipher->algorithm2);

    /*
     * Calculate the hash value and store it in |data|. The reason why
     * the empty string is used is that the definition of TLS-Exporter
     * is like so:
     *
     * TLS-Exporter(label, context_value, key_length) =
     *     HKDF-Expand-Label(Derive-Secret(Secret, label, ""),
     *                       "exporter", Hash(context_value), key_length)
     *
     * Derive-Secret(Secret, Label, Messages) =
     *       HKDF-Expand-Label(Secret, Label,
     *                         Transcript-Hash(Messages), Hash.length)
     *
     * Here Transcript-Hash is the cipher suite hash algorithm.
     */
    if (md == NULL
            || EVP_DigestInit_ex(ctx, md, NULL) <= 0
            || EVP_DigestUpdate(ctx, context, contextlen) <= 0
            || EVP_DigestFinal_ex(ctx, hash, &hashsize) <= 0
            || EVP_DigestInit_ex(ctx, md, NULL) <= 0
            || EVP_DigestFinal_ex(ctx, data, &datalen) <= 0
            || !tls13_hkdf_expand(s, md, s->early_exporter_master_secret,
                                  (const unsigned char *)label, llen,
                                  data, datalen, exportsecret, hashsize, 0)
            || !tls13_hkdf_expand(s, md, exportsecret, exporterlabel,
                                  sizeof(exporterlabel) - 1, hash, hashsize,
                                  out, olen, 0))
        goto err;

    ret = 1;
 err:
    EVP_MD_CTX_free(ctx);
    return ret;
}
