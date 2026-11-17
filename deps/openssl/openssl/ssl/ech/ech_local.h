/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Internal data structures and prototypes for handling
 * Encrypted ClientHello (ECH)
 */
#ifndef OPENSSL_NO_ECH

#ifndef HEADER_ECH_LOCAL_H
#define HEADER_ECH_LOCAL_H

#include <openssl/ssl.h>
#include <openssl/ech.h>
#include <openssl/hpke.h>

#include <internal/packet.h>
#include <internal/ssl.h>
#include <internal/statem.h>

/*
 * Define this to get loads more lines of tracing which is
 * very useful for interop.
 * This needs tracing enabled at build time, e.g.:
 *          $ ./config enable-ssl-trace enable-trace
 * This added tracing will finally (mostly) disappear once the ECH RFC
 * has issued, but is very useful for interop testing so some of it might
 * be retained.
 */
#define OSSL_ECH_SUPERVERBOSE

/* values for s->ext.ech.grease */
#define OSSL_ECH_GREASE_UNKNOWN -1 /* when we're not yet sure */
#define OSSL_ECH_NOT_GREASE 0 /* when decryption worked */
#define OSSL_ECH_IS_GREASE 1 /* when decryption failed or GREASE wanted */

/* value for uninitialised ECH version */
#define OSSL_ECH_type_unknown 0xffff
/* value for not yet set ECH config_id */
#define OSSL_ECH_config_id_unset -1

#define OSSL_ECH_OUTER_CH_TYPE 0 /* outer ECHClientHello enum */
#define OSSL_ECH_INNER_CH_TYPE 1 /* inner ECHClientHello enum */
#define OSSL_ECH_UNKNOWN_CH_TYPE -1 /* we don't know yet */

#define OSSL_ECH_CIPHER_LEN 4 /* ECHCipher length (2 for kdf, 2 for aead) */

#define OSSL_ECH_SIGNAL_LEN 8 /* length of ECH acceptance signal */

/* size of string buffer returned via ECH callback */
#define OSSL_ECH_PBUF_SIZE 8 * 1024

#ifndef CLIENT_VERSION_LEN
/*
 * This is the legacy version length, i.e. len(0x0303). The same
 * label is used in e.g. test/sslapitest.c and elsewhere but not
 * defined in a header file I could find.
 */
#define CLIENT_VERSION_LEN 2
#endif

/*
 * Reminder of what goes in DNS for ECH RFC XXXX
 *
 *     opaque HpkePublicKey<1..2^16-1>;
 *     uint16 HpkeKemId;  // Defined in I-D.irtf-cfrg-hpke
 *     uint16 HpkeKdfId;  // Defined in I-D.irtf-cfrg-hpke
 *     uint16 HpkeAeadId; // Defined in I-D.irtf-cfrg-hpke
 *     struct {
 *         HpkeKdfId kdf_id;
 *         HpkeAeadId aead_id;
 *     } HpkeSymmetricCipherSuite;
 *     struct {
 *         uint8 config_id;
 *         HpkeKemId kem_id;
 *         HpkePublicKey public_key;
 *         HpkeSymmetricCipherSuite cipher_suites<4..2^16-4>;
 *     } HpkeKeyConfig;
 *     struct {
 *         HpkeKeyConfig key_config;
 *         uint8 maximum_name_length;
 *         opaque public_name<1..255>;
 *         Extension extensions<0..2^16-1>;
 *     } ECHConfigContents;
 *     struct {
 *         uint16 version;
 *         uint16 length;
 *         select (ECHConfig.version) {
 *           case 0xfe0d: ECHConfigContents contents;
 *         }
 *     } ECHConfig;
 *     ECHConfig ECHConfigList<1..2^16-1>;
 */

typedef struct ossl_echext_st {
    uint16_t type;
    uint16_t len;
    unsigned char *val;
} OSSL_ECHEXT;

DEFINE_STACK_OF(OSSL_ECHEXT)

typedef struct ossl_echstore_entry_st {
    uint16_t version; /* 0xfe0d for RFC XXXX */
    char *public_name;
    size_t pub_len;
    unsigned char *pub;
    unsigned int nsuites;
    OSSL_HPKE_SUITE *suites;
    uint8_t max_name_length;
    uint8_t config_id;
    STACK_OF(OSSL_ECHEXT) *exts;
    time_t loadtime; /* time public and private key were loaded from file */
    EVP_PKEY *keyshare; /* long(ish) term ECH private keyshare on a server */
    int for_retry; /* whether to use this ECHConfigList in a retry */
    size_t encoded_len; /* length of overall encoded content */
    unsigned char *encoded; /* overall encoded content */
} OSSL_ECHSTORE_ENTRY;

/*
 * What we send in the ech CH extension:
 *     enum { outer(0), inner(1) } ECHClientHelloType;
 *     struct {
 *        ECHClientHelloType type;
 *        select (ECHClientHello.type) {
 *            case outer:
 *                HpkeSymmetricCipherSuite cipher_suite;
 *                uint8 config_id;
 *                opaque enc<0..2^16-1>;
 *                opaque payload<1..2^16-1>;
 *            case inner:
 *                Empty;
 *        };
 *     } ECHClientHello;
 *
 */
typedef struct ech_encch_st {
    uint16_t kdf_id; /* ciphersuite  */
    uint16_t aead_id; /* ciphersuite  */
    uint8_t config_id; /* (maybe) identifies DNS RR value used */
    size_t enc_len; /* public share */
    unsigned char *enc; /* public share for sender */
    size_t payload_len; /* ciphertext  */
    unsigned char *payload; /* ciphertext  */
} OSSL_ECH_ENCCH;

DEFINE_STACK_OF(OSSL_ECHSTORE_ENTRY)

struct ossl_echstore_st {
    STACK_OF(OSSL_ECHSTORE_ENTRY) *entries;
    OSSL_LIB_CTX *libctx;
    char *propq;
};

/* ECH details associated with an SSL_CTX */
typedef struct ossl_ech_ctx_st {
    /*
     * We could make es ref-counted, but that seems like a premature
     * optimisation, given we don't currently expect many applications
     * to have many SSL_CTX/SSL structures using many ECH configurations.
     * Could fairly easily be done if experience warrants.
     */
    OSSL_ECHSTORE *es; /* ECHConfigList details */
    unsigned char *alpn_outer;
    size_t alpn_outer_len;
    SSL_ech_cb_func cb; /* callback function for when ECH "done" */
} OSSL_ECH_CTX;

/* ECH details associated with an SSL_CONNECTION */
typedef struct ossl_ech_conn_st {
    /*
     * We could make es ref-counted, but that seems like a premature
     * optimisation, given we don't currently expect many applications
     * to have many SSL_CTX/SSL structures using many ECH configurations.
     * Could fairly easily be done if experience warrants.
     */
    OSSL_ECHSTORE *es; /* ECHConfigList details */
    int no_outer; /* set to 1 if we should send no outer SNI at all */
    char *outer_hostname;
    unsigned char *alpn_outer;
    size_t alpn_outer_len;
    SSL_ech_cb_func cb; /* callback function for when ECH "done" */
    /*
     * If ECH fails, then we switch to verifying the cert for the
     * outer_hostname, meanwhile we still want to be able to trace
     * the value we tried as the inner SNI for debug purposes
     */
    char *former_inner;
    /* inner CH transcript buffer */
    unsigned char *transbuf;
    size_t transbuf_len;
    /* inner ClientHello before ECH compression */
    unsigned char *innerch;
    size_t innerch_len;
    /* encoded inner CH */
    unsigned char *encoded_inner;
    size_t encoded_inner_len;
    /* lengths calculated early, used when encrypting at end of processing */
    size_t clearlen;
    size_t cipherlen;
    /* location to put ciphertext, initially filled with zeros */
    size_t cipher_offset;
    /*
     * Extensions are "outer-only" if the value is only sent in the
     * outer CH and only the type is sent in the inner CH.
     * We use this array to keep track of the extension types that
     * have values only in the outer CH
     * Currently, this is basically controlled at compile time, but
     * in a way that could be varied, or, in future, put under
     * run-time control, so having this isn't so much an overhead.
     */
    uint16_t outer_only[OSSL_ECH_OUTERS_MAX];
    size_t n_outer_only; /* the number of outer_only extensions so far */
    /*
     * We store/access the index of the extension handler in
     * s->ext.ech.ext_ind, as we'd otherwise not know it here.
     * Be nice were there a better way to handle that.
     * Index of the current extension's entry in ext_defs - this is
     * to avoid the need to change a couple of extension APIs.
     */
    int ext_ind;
    /* ECH status vars */
    int ch_depth; /* set during CH creation, 0: doing outer, 1: doing inner */
    int attempted; /* 1 if ECH was or is being attempted, 0 otherwise */
    int done; /* 1 if we've finished ECH calculations, 0 otherwise */
    uint16_t attempted_type; /* ECH version used */
    int attempted_cid; /* ECH config id sent/rx'd */
    int backend; /* 1 if we're a server backend in split-mode, 0 otherwise */
    /* When using a PSK stash the tick_identity from inner, for outer */
    int tick_identity;
    /*
     * success is 1 if ECH succeeded, 0 otherwise, on the server this
     * is known early, on the client we need to wait for the ECH confirm
     * calculation based on the SH (or 2nd SH in case of HRR)
     */
    int success;
    /*
     * we set this when we've gotten to the end of the handshake and
     * the only thing that went wrong was ECH - in that case we're
     * ok to provide the retry-configs to the client, otherwise better
     * not.
     */
    int retry_configs_ok;
    int inner_ech_seen_ok; /* set if we see inner ECH as expected */
    int grease; /* 1 if we're GREASEing, 0 otherwise */
    char *grease_suite; /* HPKE suite string for GREASEing */
    unsigned char *sent; /* GREASEy ECH value sent, in case needed for re-tx */
    size_t sent_len;
    unsigned char *returned; /* binary ECHConfigList retry-configs value */
    size_t returned_len;
    unsigned char *pub; /* client ephemeral public kept by server in case HRR */
    size_t pub_len;
    OSSL_HPKE_CTX *hpke_ctx; /* HPKE context, needed for HRR */
    /*
     * Offsets of various things we need to know about in an inbound
     * ClientHello (CH) plus the type of ECH and whether that CH is an inner or
     * outer CH. We find these once for the outer CH, by roughly parsing the CH
     * so store them for later re-use. We need to re-do this parsing when we
     * get the 2nd CH in the case of HRR, and when we move to processing the
     * inner CH after successful ECH decyption, so we have a flag to say if
     * we've done the work or not.
     */
    int ch_offsets_done;
    size_t sessid_off; /* offset of session_id length */
    size_t exts_off; /* to offset of extensions */
    size_t ech_off; /* offset of ECH */
    size_t sni_off; /* offset of (outer) SNI */
    int echtype; /* ext type of the ECH */
    int inner; /* 1 if the ECH is marked as an inner, 0 for outer */
    /*
     * A pointer to, and copy of, the hrrsignal from an HRR message.
     * We need both, as we zero-out the octets when re-calculating and
     * may need to put back what the server included so the transcript
     * is correct when ECH acceptance failed.
     */
    unsigned char *hrrsignal_p;
    unsigned char hrrsignal[OSSL_ECH_SIGNAL_LEN];
    /*
     * Fields that differ on client between inner and outer that we need to
     * keep and swap over IFF ECH has succeeded. Same names chosen as are
     * used in SSL_CONNECTION
     */
    EVP_PKEY *ks_pkey[OPENSSL_CLIENT_MAX_KEY_SHARES];
    /* The IDs of the keyshare keys */
    uint16_t ks_group_id[OPENSSL_CLIENT_MAX_KEY_SHARES];
    size_t num_ks_pkey; /* how many keyshares are there */
    unsigned char client_random[SSL3_RANDOM_SIZE]; /* CH random */
} OSSL_ECH_CONN;

/* Return values from ossl_ech_same_ext */
#define OSSL_ECH_SAME_EXT_ERR 0 /* bummer something wrong */
#define OSSL_ECH_SAME_EXT_DONE 1 /* proceed with same value in inner/outer */
#define OSSL_ECH_SAME_EXT_CONTINUE 2 /* generate a new value for outer CH */

/*
 * During extension construction (in extensions_clnt.c, and surprisingly also in
 * extensions.c), we need to handle inner/outer CH cloning - ossl_ech_same_ext
 * will (depending on compile time handling options) copy the value from
 * CH.inner to CH.outer or else processing will continue, for a 2nd call,
 * likely generating a fresh value for the outer CH. The fresh value could well
 * be the same as in the inner.
 *
 * This macro should be called in each _ctos_ function that doesn't explicitly
 * have special ECH handling. There are some _ctos_ functions that are called
 * from a server, but we don't want to do anything in such cases. We also
 * screen out cases where the context is not handling the ClientHello.
 *
 * Note that the placement of this macro needs a bit of thought - it has to go
 * after declarations (to keep the ansi-c compile happy) and also after any
 * checks that result in the extension not being sent but before any relevant
 * state changes that would affect a possible 2nd call to the constructor.
 * Luckily, that's usually not too hard, but it's not mechanical.
 */
#define ECH_SAME_EXT(s, context, pkt)                         \
    if (context == SSL_EXT_CLIENT_HELLO && !s->server         \
        && s->ext.ech.es != NULL && s->ext.ech.grease == 0) { \
        int ech_iosame_rv = ossl_ech_same_ext(s, pkt);        \
                                                              \
        if (ech_iosame_rv == OSSL_ECH_SAME_EXT_ERR)           \
            return EXT_RETURN_FAIL;                           \
        if (ech_iosame_rv == OSSL_ECH_SAME_EXT_DONE)          \
            return EXT_RETURN_SENT;                           \
        /* otherwise continue as normal */                    \
    }

/* Internal ECH APIs */

OSSL_ECHSTORE *ossl_echstore_dup(const OSSL_ECHSTORE *old);
void ossl_echstore_entry_free(OSSL_ECHSTORE_ENTRY *ee);
void ossl_ech_ctx_clear(OSSL_ECH_CTX *ce);
int ossl_ech_conn_init(SSL_CONNECTION *s, SSL_CTX *ctx,
    const SSL_METHOD *method);
void ossl_ech_conn_clear(OSSL_ECH_CONN *ec);
void ossl_echext_free(OSSL_ECHEXT *e);
OSSL_ECHEXT *ossl_echext_dup(const OSSL_ECHEXT *src);
#ifdef OSSL_ECH_SUPERVERBOSE
void ossl_ech_pbuf(const char *msg,
    const unsigned char *buf, const size_t blen);
#endif
int ossl_ech_get_retry_configs(SSL_CONNECTION *s, unsigned char **rcfgs,
    size_t *rcfgslen);
int ossl_ech_send_grease(SSL_CONNECTION *s, WPACKET *pkt);
int ossl_ech_pick_matching_cfg(SSL_CONNECTION *s, OSSL_ECHSTORE_ENTRY **ee,
    OSSL_HPKE_SUITE *suite);
int ossl_ech_encode_inner(SSL_CONNECTION *s, unsigned char **encoded,
    size_t *encoded_len);
int ossl_ech_find_confirm(SSL_CONNECTION *s, int hrr,
    unsigned char acbuf[OSSL_ECH_SIGNAL_LEN]);
int ossl_ech_reset_hs_buffer(SSL_CONNECTION *s, const unsigned char *buf,
    size_t blen);
int ossl_ech_aad_and_encrypt(SSL_CONNECTION *s, WPACKET *pkt);
int ossl_ech_swaperoo(SSL_CONNECTION *s);
int ossl_ech_calc_confirm(SSL_CONNECTION *s, int for_hrr,
    unsigned char acbuf[OSSL_ECH_SIGNAL_LEN],
    const size_t shlen);

/* these are internal but located in ssl/statem/extensions.c */
int ossl_ech_same_ext(SSL_CONNECTION *s, WPACKET *pkt);
int ossl_ech_same_key_share(void);
int ossl_ech_2bcompressed(size_t ind);
int ossl_ech_copy_inner2outer(SSL_CONNECTION *s, uint16_t ext_type, int ind,
    WPACKET *pkt);

int ossl_ech_get_ch_offsets(SSL_CONNECTION *s, PACKET *pkt, size_t *sessid,
    size_t *exts, size_t *echoffset, uint16_t *echtype,
    int *inner, size_t *snioffset);
int ossl_ech_early_decrypt(SSL_CONNECTION *s, PACKET *outerpkt, PACKET *newpkt);
void ossl_ech_status_print(BIO *out, SSL_CONNECTION *s, int selector);

int ossl_ech_intbuf_add(SSL_CONNECTION *s, const unsigned char *buf,
    size_t blen, int hash_existing);
int ossl_ech_intbuf_fetch(SSL_CONNECTION *s, unsigned char **buf, size_t *blen);
size_t ossl_ech_calc_padding(SSL_CONNECTION *s, OSSL_ECHSTORE_ENTRY *ee,
    size_t encoded_len);
int ossl_ech_stash_keyshares(SSL_CONNECTION *s);
int ossl_ech_unstash_keyshares(SSL_CONNECTION *s);

#endif
#endif
