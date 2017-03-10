/*
 * Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Copyright (c) 2002 Bob Beck <beck@openbsd.org>
 * Copyright (c) 2002 Theo de Raadt
 * Copyright (c) 2002 Markus Friedl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <openssl/objects.h>
#include <internal/engine.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>

#if (defined(__unix__) || defined(unix)) && !defined(USG) && \
        (defined(OpenBSD) || defined(__FreeBSD__))
# include <sys/param.h>
# if (OpenBSD >= 200112) || ((__FreeBSD_version >= 470101 && __FreeBSD_version < 500000) || __FreeBSD_version >= 500041)
#  define HAVE_CRYPTODEV
# endif
# if (OpenBSD >= 200110)
#  define HAVE_SYSLOG_R
# endif
#endif

#include <sys/types.h>
#ifdef HAVE_CRYPTODEV
# include <crypto/cryptodev.h>
# include <sys/ioctl.h>
# include <errno.h>
# include <stdio.h>
# include <unistd.h>
# include <fcntl.h>
# include <stdarg.h>
# include <syslog.h>
# include <errno.h>
# include <string.h>
#endif
#include <openssl/dh.h>
#include <openssl/dsa.h>
#include <openssl/err.h>
#include <openssl/rsa.h>

#ifndef HAVE_CRYPTODEV

void engine_load_cryptodev_int(void)
{
    /* This is a NOP on platforms without /dev/crypto */
    return;
}

#else

struct dev_crypto_state {
    struct session_op d_sess;
    int d_fd;
# ifdef USE_CRYPTODEV_DIGESTS
    char dummy_mac_key[HASH_MAX_LEN];
    unsigned char digest_res[HASH_MAX_LEN];
    char *mac_data;
    int mac_len;
# endif
};

static u_int32_t cryptodev_asymfeat = 0;

static RSA_METHOD *cryptodev_rsa;
#ifndef OPENSSL_NO_DSA
static DSA_METHOD *cryptodev_dsa = NULL;
#endif
#ifndef OPENSSL_NO_DH
static DH_METHOD *cryptodev_dh;
#endif

static int get_asym_dev_crypto(void);
static int open_dev_crypto(void);
static int get_dev_crypto(void);
static int get_cryptodev_ciphers(const int **cnids);
# ifdef USE_CRYPTODEV_DIGESTS
static int get_cryptodev_digests(const int **cnids);
# endif
static int cryptodev_usable_ciphers(const int **nids);
static int cryptodev_usable_digests(const int **nids);
static int cryptodev_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                            const unsigned char *in, size_t inl);
static int cryptodev_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                              const unsigned char *iv, int enc);
static int cryptodev_cleanup(EVP_CIPHER_CTX *ctx);
static int cryptodev_engine_ciphers(ENGINE *e, const EVP_CIPHER **cipher,
                                    const int **nids, int nid);
static int cryptodev_engine_digests(ENGINE *e, const EVP_MD **digest,
                                    const int **nids, int nid);
static int bn2crparam(const BIGNUM *a, struct crparam *crp);
static int crparam2bn(struct crparam *crp, BIGNUM *a);
static void zapparams(struct crypt_kop *kop);
static int cryptodev_asym(struct crypt_kop *kop, int rlen, BIGNUM *r,
                          int slen, BIGNUM *s);

static int cryptodev_bn_mod_exp(BIGNUM *r, const BIGNUM *a,
                                const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
                                BN_MONT_CTX *m_ctx);
static int cryptodev_rsa_nocrt_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa,
                                       BN_CTX *ctx);
static int cryptodev_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa,
                                 BN_CTX *ctx);
#ifndef OPENSSL_NO_DSA
static int cryptodev_dsa_bn_mod_exp(DSA *dsa, BIGNUM *r, const BIGNUM *a,
                                    const BIGNUM *p, const BIGNUM *m,
                                    BN_CTX *ctx, BN_MONT_CTX *m_ctx);
static int cryptodev_dsa_dsa_mod_exp(DSA *dsa, BIGNUM *t1, const BIGNUM *g,
                                     const BIGNUM *u1, const BIGNUM *pub_key,
                                     const BIGNUM *u2, const BIGNUM *p,
                                     BN_CTX *ctx, BN_MONT_CTX *mont);
static DSA_SIG *cryptodev_dsa_do_sign(const unsigned char *dgst, int dlen,
                                      DSA *dsa);
static int cryptodev_dsa_verify(const unsigned char *dgst, int dgst_len,
                                DSA_SIG *sig, DSA *dsa);
#endif
#ifndef OPENSSL_NO_DH
static int cryptodev_mod_exp_dh(const DH *dh, BIGNUM *r, const BIGNUM *a,
                                const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
                                BN_MONT_CTX *m_ctx);
static int cryptodev_dh_compute_key(unsigned char *key, const BIGNUM *pub_key,
                                    DH *dh);
#endif
static int cryptodev_ctrl(ENGINE *e, int cmd, long i, void *p,
                          void (*f) (void));
void engine_load_cryptodev_int(void);

static const ENGINE_CMD_DEFN cryptodev_defns[] = {
    {0, NULL, NULL, 0}
};

static struct {
    int id;
    int nid;
    int ivmax;
    int keylen;
} ciphers[] = {
    {
        CRYPTO_ARC4, NID_rc4, 0, 16,
    },
    {
        CRYPTO_DES_CBC, NID_des_cbc, 8, 8,
    },
    {
        CRYPTO_3DES_CBC, NID_des_ede3_cbc, 8, 24,
    },
    {
        CRYPTO_AES_CBC, NID_aes_128_cbc, 16, 16,
    },
    {
        CRYPTO_AES_CBC, NID_aes_192_cbc, 16, 24,
    },
    {
        CRYPTO_AES_CBC, NID_aes_256_cbc, 16, 32,
    },
# ifdef CRYPTO_AES_CTR
    {
        CRYPTO_AES_CTR, NID_aes_128_ctr, 14, 16,
    },
    {
        CRYPTO_AES_CTR, NID_aes_192_ctr, 14, 24,
    },
    {
        CRYPTO_AES_CTR, NID_aes_256_ctr, 14, 32,
    },
# endif
    {
        CRYPTO_BLF_CBC, NID_bf_cbc, 8, 16,
    },
    {
        CRYPTO_CAST_CBC, NID_cast5_cbc, 8, 16,
    },
    {
        CRYPTO_SKIPJACK_CBC, NID_undef, 0, 0,
    },
    {
        0, NID_undef, 0, 0,
    },
};

# ifdef USE_CRYPTODEV_DIGESTS
static struct {
    int id;
    int nid;
    int keylen;
} digests[] = {
    {
        CRYPTO_MD5_HMAC, NID_hmacWithMD5, 16
    },
    {
        CRYPTO_SHA1_HMAC, NID_hmacWithSHA1, 20
    },
    {
        CRYPTO_RIPEMD160_HMAC, NID_ripemd160, 16
        /* ? */
    },
    {
        CRYPTO_MD5_KPDK, NID_undef, 0
    },
    {
        CRYPTO_SHA1_KPDK, NID_undef, 0
    },
    {
        CRYPTO_MD5, NID_md5, 16
    },
    {
        CRYPTO_SHA1, NID_sha1, 20
    },
    {
        0, NID_undef, 0
    },
};
# endif

/*
 * Return a fd if /dev/crypto seems usable, 0 otherwise.
 */
static int open_dev_crypto(void)
{
    static int fd = -1;

    if (fd == -1) {
        if ((fd = open("/dev/crypto", O_RDWR, 0)) == -1)
            return (-1);
        /* close on exec */
        if (fcntl(fd, F_SETFD, 1) == -1) {
            close(fd);
            fd = -1;
            return (-1);
        }
    }
    return (fd);
}

static int get_dev_crypto(void)
{
    int fd, retfd;

    if ((fd = open_dev_crypto()) == -1)
        return (-1);
# ifndef CRIOGET_NOT_NEEDED
    if (ioctl(fd, CRIOGET, &retfd) == -1)
        return (-1);

    /* close on exec */
    if (fcntl(retfd, F_SETFD, 1) == -1) {
        close(retfd);
        return (-1);
    }
# else
    retfd = fd;
# endif
    return (retfd);
}

static void put_dev_crypto(int fd)
{
# ifndef CRIOGET_NOT_NEEDED
    close(fd);
# endif
}

/* Caching version for asym operations */
static int get_asym_dev_crypto(void)
{
    static int fd = -1;

    if (fd == -1)
        fd = get_dev_crypto();
    return fd;
}

/*
 * Find out what ciphers /dev/crypto will let us have a session for.
 * XXX note, that some of these openssl doesn't deal with yet!
 * returning them here is harmless, as long as we return NULL
 * when asked for a handler in the cryptodev_engine_ciphers routine
 */
static int get_cryptodev_ciphers(const int **cnids)
{
    static int nids[CRYPTO_ALGORITHM_MAX];
    struct session_op sess;
    int fd, i, count = 0;

    if ((fd = get_dev_crypto()) < 0) {
        *cnids = NULL;
        return (0);
    }
    memset(&sess, 0, sizeof(sess));
    sess.key = (caddr_t) "123456789abcdefghijklmno";

    for (i = 0; ciphers[i].id && count < CRYPTO_ALGORITHM_MAX; i++) {
        if (ciphers[i].nid == NID_undef)
            continue;
        sess.cipher = ciphers[i].id;
        sess.keylen = ciphers[i].keylen;
        sess.mac = 0;
        if (ioctl(fd, CIOCGSESSION, &sess) != -1 &&
            ioctl(fd, CIOCFSESSION, &sess.ses) != -1)
            nids[count++] = ciphers[i].nid;
    }
    put_dev_crypto(fd);

    if (count > 0)
        *cnids = nids;
    else
        *cnids = NULL;
    return (count);
}

# ifdef USE_CRYPTODEV_DIGESTS
/*
 * Find out what digests /dev/crypto will let us have a session for.
 * XXX note, that some of these openssl doesn't deal with yet!
 * returning them here is harmless, as long as we return NULL
 * when asked for a handler in the cryptodev_engine_digests routine
 */
static int get_cryptodev_digests(const int **cnids)
{
    static int nids[CRYPTO_ALGORITHM_MAX];
    struct session_op sess;
    int fd, i, count = 0;

    if ((fd = get_dev_crypto()) < 0) {
        *cnids = NULL;
        return (0);
    }
    memset(&sess, 0, sizeof(sess));
    sess.mackey = (caddr_t) "123456789abcdefghijklmno";
    for (i = 0; digests[i].id && count < CRYPTO_ALGORITHM_MAX; i++) {
        if (digests[i].nid == NID_undef)
            continue;
        sess.mac = digests[i].id;
        sess.mackeylen = digests[i].keylen;
        sess.cipher = 0;
        if (ioctl(fd, CIOCGSESSION, &sess) != -1 &&
            ioctl(fd, CIOCFSESSION, &sess.ses) != -1)
            nids[count++] = digests[i].nid;
    }
    put_dev_crypto(fd);

    if (count > 0)
        *cnids = nids;
    else
        *cnids = NULL;
    return (count);
}
# endif                         /* 0 */

/*
 * Find the useable ciphers|digests from dev/crypto - this is the first
 * thing called by the engine init crud which determines what it
 * can use for ciphers from this engine. We want to return
 * only what we can do, anything else is handled by software.
 *
 * If we can't initialize the device to do anything useful for
 * any reason, we want to return a NULL array, and 0 length,
 * which forces everything to be done is software. By putting
 * the initialization of the device in here, we ensure we can
 * use this engine as the default, and if for whatever reason
 * /dev/crypto won't do what we want it will just be done in
 * software
 *
 * This can (should) be greatly expanded to perhaps take into
 * account speed of the device, and what we want to do.
 * (although the disabling of particular alg's could be controlled
 * by the device driver with sysctl's.) - this is where we
 * want most of the decisions made about what we actually want
 * to use from /dev/crypto.
 */
static int cryptodev_usable_ciphers(const int **nids)
{
    return (get_cryptodev_ciphers(nids));
}

static int cryptodev_usable_digests(const int **nids)
{
# ifdef USE_CRYPTODEV_DIGESTS
    return (get_cryptodev_digests(nids));
# else
    /*
     * XXXX just disable all digests for now, because it sucks.
     * we need a better way to decide this - i.e. I may not
     * want digests on slow cards like hifn on fast machines,
     * but might want them on slow or loaded machines, etc.
     * will also want them when using crypto cards that don't
     * suck moose gonads - would be nice to be able to decide something
     * as reasonable default without having hackery that's card dependent.
     * of course, the default should probably be just do everything,
     * with perhaps a sysctl to turn algorithms off (or have them off
     * by default) on cards that generally suck like the hifn.
     */
    *nids = NULL;
    return (0);
# endif
}

static int
cryptodev_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                 const unsigned char *in, size_t inl)
{
    struct crypt_op cryp;
    struct dev_crypto_state *state = EVP_CIPHER_CTX_get_cipher_data(ctx);
    struct session_op *sess = &state->d_sess;
    const void *iiv;
    unsigned char save_iv[EVP_MAX_IV_LENGTH];

    if (state->d_fd < 0)
        return (0);
    if (!inl)
        return (1);
    if ((inl % EVP_CIPHER_CTX_block_size(ctx)) != 0)
        return (0);

    memset(&cryp, 0, sizeof(cryp));

    cryp.ses = sess->ses;
    cryp.flags = 0;
    cryp.len = inl;
    cryp.src = (caddr_t) in;
    cryp.dst = (caddr_t) out;
    cryp.mac = 0;

    cryp.op = EVP_CIPHER_CTX_encrypting(ctx) ? COP_ENCRYPT : COP_DECRYPT;

    if (EVP_CIPHER_CTX_iv_length(ctx) > 0) {
        cryp.iv = (caddr_t) EVP_CIPHER_CTX_iv(ctx);
        if (!EVP_CIPHER_CTX_encrypting(ctx)) {
            iiv = in + inl - EVP_CIPHER_CTX_iv_length(ctx);
            memcpy(save_iv, iiv, EVP_CIPHER_CTX_iv_length(ctx));
        }
    } else
        cryp.iv = NULL;

    if (ioctl(state->d_fd, CIOCCRYPT, &cryp) == -1) {
        /*
         * XXX need better error handling this can fail for a number of
         * different reasons.
         */
        return (0);
    }

    if (EVP_CIPHER_CTX_iv_length(ctx) > 0) {
        if (EVP_CIPHER_CTX_encrypting(ctx))
            iiv = out + inl - EVP_CIPHER_CTX_iv_length(ctx);
        else
            iiv = save_iv;
        memcpy(EVP_CIPHER_CTX_iv_noconst(ctx), iiv,
               EVP_CIPHER_CTX_iv_length(ctx));
    }
    return (1);
}

static int
cryptodev_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                   const unsigned char *iv, int enc)
{
    struct dev_crypto_state *state = EVP_CIPHER_CTX_get_cipher_data(ctx);
    struct session_op *sess = &state->d_sess;
    int cipher = -1, i;

    for (i = 0; ciphers[i].id; i++)
        if (EVP_CIPHER_CTX_nid(ctx) == ciphers[i].nid &&
            EVP_CIPHER_CTX_iv_length(ctx) <= ciphers[i].ivmax &&
            EVP_CIPHER_CTX_key_length(ctx) == ciphers[i].keylen) {
            cipher = ciphers[i].id;
            break;
        }

    if (!ciphers[i].id) {
        state->d_fd = -1;
        return (0);
    }

    memset(sess, 0, sizeof(*sess));

    if ((state->d_fd = get_dev_crypto()) < 0)
        return (0);

    sess->key = (caddr_t) key;
    sess->keylen = EVP_CIPHER_CTX_key_length(ctx);
    sess->cipher = cipher;

    if (ioctl(state->d_fd, CIOCGSESSION, sess) == -1) {
        put_dev_crypto(state->d_fd);
        state->d_fd = -1;
        return (0);
    }
    return (1);
}

/*
 * free anything we allocated earlier when initing a
 * session, and close the session.
 */
static int cryptodev_cleanup(EVP_CIPHER_CTX *ctx)
{
    int ret = 0;
    struct dev_crypto_state *state = EVP_CIPHER_CTX_get_cipher_data(ctx);
    struct session_op *sess = &state->d_sess;

    if (state->d_fd < 0)
        return (0);

    /*
     * XXX if this ioctl fails, something's wrong. the invoker may have called
     * us with a bogus ctx, or we could have a device that for whatever
     * reason just doesn't want to play ball - it's not clear what's right
     * here - should this be an error? should it just increase a counter,
     * hmm. For right now, we return 0 - I don't believe that to be "right".
     * we could call the gorpy openssl lib error handlers that print messages
     * to users of the library. hmm..
     */

    if (ioctl(state->d_fd, CIOCFSESSION, &sess->ses) == -1) {
        ret = 0;
    } else {
        ret = 1;
    }
    put_dev_crypto(state->d_fd);
    state->d_fd = -1;

    return (ret);
}

/*
 * libcrypto EVP stuff - this is how we get wired to EVP so the engine
 * gets called when libcrypto requests a cipher NID.
 */

/* RC4 */
static EVP_CIPHER *rc4_cipher = NULL;
static const EVP_CIPHER *cryptodev_rc4(void)
{
    if (rc4_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_rc4, 1, 16)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 0)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_VARIABLE_LENGTH)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        rc4_cipher = cipher;
    }
    return rc4_cipher;
}

/* DES CBC EVP */
static EVP_CIPHER *des_cbc_cipher = NULL;
static const EVP_CIPHER *cryptodev_des_cbc(void)
{
    if (des_cbc_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_des_cbc, 8, 8)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 8)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CBC_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        des_cbc_cipher = cipher;
    }
    return des_cbc_cipher;
}

/* 3DES CBC EVP */
static EVP_CIPHER *des3_cbc_cipher = NULL;
static const EVP_CIPHER *cryptodev_3des_cbc(void)
{
    if (des3_cbc_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_des_ede3_cbc, 8, 24)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 8)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CBC_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        des3_cbc_cipher = cipher;
    }
    return des3_cbc_cipher;
}

static EVP_CIPHER *bf_cbc_cipher = NULL;
static const EVP_CIPHER *cryptodev_bf_cbc(void)
{
    if (bf_cbc_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_bf_cbc, 8, 16)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 8)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CBC_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        bf_cbc_cipher = cipher;
    }
    return bf_cbc_cipher;
}

static EVP_CIPHER *cast_cbc_cipher = NULL;
static const EVP_CIPHER *cryptodev_cast_cbc(void)
{
    if (cast_cbc_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_cast5_cbc, 8, 16)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 8)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CBC_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        cast_cbc_cipher = cipher;
    }
    return cast_cbc_cipher;
}

static EVP_CIPHER *aes_cbc_cipher = NULL;
static const EVP_CIPHER *cryptodev_aes_cbc(void)
{
    if (aes_cbc_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_aes_128_cbc, 16, 16)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 16)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CBC_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        aes_cbc_cipher = cipher;
    }
    return aes_cbc_cipher;
}

static EVP_CIPHER *aes_192_cbc_cipher = NULL;
static const EVP_CIPHER *cryptodev_aes_192_cbc(void)
{
    if (aes_192_cbc_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_aes_192_cbc, 16, 24)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 16)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CBC_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        aes_192_cbc_cipher = cipher;
    }
    return aes_192_cbc_cipher;
}

static EVP_CIPHER *aes_256_cbc_cipher = NULL;
static const EVP_CIPHER *cryptodev_aes_256_cbc(void)
{
    if (aes_256_cbc_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_aes_256_cbc, 16, 32)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 16)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CBC_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        aes_256_cbc_cipher = cipher;
    }
    return aes_256_cbc_cipher;
}

# ifdef CRYPTO_AES_CTR
static EVP_CIPHER *aes_ctr_cipher = NULL;
static const EVP_CIPHER *cryptodev_aes_ctr(void)
{
    if (aes_ctr_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_aes_128_ctr, 16, 16)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 14)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CTR_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        aes_ctr_cipher = cipher;
    }
    return aes_ctr_cipher;
}

static EVP_CIPHER *aes_192_ctr_cipher = NULL;
static const EVP_CIPHER *cryptodev_aes_192_ctr(void)
{
    if (aes_192_ctr_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_aes_192_ctr, 16, 24)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 14)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CTR_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        aes_192_ctr_cipher = cipher;
    }
    return aes_192_ctr_cipher;
}

static EVP_CIPHER *aes_256_ctr_cipher = NULL;
static const EVP_CIPHER *cryptodev_aes_256_ctr(void)
{
    if (aes_256_ctr_cipher == NULL) {
        EVP_CIPHER *cipher;

        if ((cipher = EVP_CIPHER_meth_new(NID_aes_256_ctr, 16, 32)) == NULL
            || !EVP_CIPHER_meth_set_iv_length(cipher, 14)
            || !EVP_CIPHER_meth_set_flags(cipher, EVP_CIPH_CTR_MODE)
            || !EVP_CIPHER_meth_set_init(cipher, cryptodev_init_key)
            || !EVP_CIPHER_meth_set_do_cipher(cipher, cryptodev_cipher)
            || !EVP_CIPHER_meth_set_cleanup(cipher, cryptodev_cleanup)
            || !EVP_CIPHER_meth_set_impl_ctx_size(cipher, sizeof(struct dev_crypto_state))
            || !EVP_CIPHER_meth_set_set_asn1_params(cipher, EVP_CIPHER_set_asn1_iv)
            || !EVP_CIPHER_meth_set_get_asn1_params(cipher, EVP_CIPHER_get_asn1_iv)) {
            EVP_CIPHER_meth_free(cipher);
            cipher = NULL;
        }
        aes_256_ctr_cipher = cipher;
    }
    return aes_256_ctr_cipher;
}
# endif
/*
 * Registered by the ENGINE when used to find out how to deal with
 * a particular NID in the ENGINE. this says what we'll do at the
 * top level - note, that list is restricted by what we answer with
 */
static int
cryptodev_engine_ciphers(ENGINE *e, const EVP_CIPHER **cipher,
                         const int **nids, int nid)
{
    if (!cipher)
        return (cryptodev_usable_ciphers(nids));

    switch (nid) {
    case NID_rc4:
        *cipher = cryptodev_rc4();
        break;
    case NID_des_ede3_cbc:
        *cipher = cryptodev_3des_cbc();
        break;
    case NID_des_cbc:
        *cipher = cryptodev_des_cbc();
        break;
    case NID_bf_cbc:
        *cipher = cryptodev_bf_cbc();
        break;
    case NID_cast5_cbc:
        *cipher = cryptodev_cast_cbc();
        break;
    case NID_aes_128_cbc:
        *cipher = cryptodev_aes_cbc();
        break;
    case NID_aes_192_cbc:
        *cipher = cryptodev_aes_192_cbc();
        break;
    case NID_aes_256_cbc:
        *cipher = cryptodev_aes_256_cbc();
        break;
# ifdef CRYPTO_AES_CTR
    case NID_aes_128_ctr:
        *cipher = cryptodev_aes_ctr();
        break;
    case NID_aes_192_ctr:
        *cipher = cryptodev_aes_192_ctr();
        break;
    case NID_aes_256_ctr:
        *cipher = cryptodev_aes_256_ctr();
        break;
# endif
    default:
        *cipher = NULL;
        break;
    }
    return (*cipher != NULL);
}

# ifdef USE_CRYPTODEV_DIGESTS

/* convert digest type to cryptodev */
static int digest_nid_to_cryptodev(int nid)
{
    int i;

    for (i = 0; digests[i].id; i++)
        if (digests[i].nid == nid)
            return (digests[i].id);
    return (0);
}

static int digest_key_length(int nid)
{
    int i;

    for (i = 0; digests[i].id; i++)
        if (digests[i].nid == nid)
            return digests[i].keylen;
    return (0);
}

static int cryptodev_digest_init(EVP_MD_CTX *ctx)
{
    struct dev_crypto_state *state = EVP_MD_CTX_md_data(ctx);
    struct session_op *sess = &state->d_sess;
    int digest;

    if ((digest = digest_nid_to_cryptodev(EVP_MD_CTX_type(ctx))) == NID_undef) {
        printf("cryptodev_digest_init: Can't get digest \n");
        return (0);
    }

    memset(state, 0, sizeof(*state));

    if ((state->d_fd = get_dev_crypto()) < 0) {
        printf("cryptodev_digest_init: Can't get Dev \n");
        return (0);
    }

    sess->mackey = state->dummy_mac_key;
    sess->mackeylen = digest_key_length(EVP_MD_CTX_type(ctx));
    sess->mac = digest;

    if (ioctl(state->d_fd, CIOCGSESSION, sess) < 0) {
        put_dev_crypto(state->d_fd);
        state->d_fd = -1;
        printf("cryptodev_digest_init: Open session failed\n");
        return (0);
    }

    return (1);
}

static int cryptodev_digest_update(EVP_MD_CTX *ctx, const void *data,
                                   size_t count)
{
    struct crypt_op cryp;
    struct dev_crypto_state *state = EVP_MD_CTX_md_data(ctx);
    struct session_op *sess = &state->d_sess;
    char *new_mac_data;

    if (!data || state->d_fd < 0) {
        printf("cryptodev_digest_update: illegal inputs \n");
        return (0);
    }

    if (!count) {
        return (0);
    }

    if (!EVP_MD_CTX_test_flags(ctx, EVP_MD_CTX_FLAG_ONESHOT)) {
        /* if application doesn't support one buffer */
        new_mac_data =
            OPENSSL_realloc(state->mac_data, state->mac_len + count);

        if (!new_mac_data) {
            printf("cryptodev_digest_update: realloc failed\n");
            return (0);
        }
        state->mac_data = new_mac_data;

        memcpy(state->mac_data + state->mac_len, data, count);
        state->mac_len += count;

        return (1);
    }

    memset(&cryp, 0, sizeof(cryp));

    cryp.ses = sess->ses;
    cryp.flags = 0;
    cryp.len = count;
    cryp.src = (caddr_t) data;
    cryp.dst = NULL;
    cryp.mac = (caddr_t) state->digest_res;
    if (ioctl(state->d_fd, CIOCCRYPT, &cryp) < 0) {
        printf("cryptodev_digest_update: digest failed\n");
        return (0);
    }
    return (1);
}

static int cryptodev_digest_final(EVP_MD_CTX *ctx, unsigned char *md)
{
    struct crypt_op cryp;
    struct dev_crypto_state *state = EVP_MD_CTX_md_data(ctx);
    struct session_op *sess = &state->d_sess;

    int ret = 1;

    if (!md || state->d_fd < 0) {
        printf("cryptodev_digest_final: illegal input\n");
        return (0);
    }

    if (!EVP_MD_CTX_test_flags(ctx, EVP_MD_CTX_FLAG_ONESHOT)) {
        /* if application doesn't support one buffer */
        memset(&cryp, 0, sizeof(cryp));
        cryp.ses = sess->ses;
        cryp.flags = 0;
        cryp.len = state->mac_len;
        cryp.src = state->mac_data;
        cryp.dst = NULL;
        cryp.mac = (caddr_t) md;
        if (ioctl(state->d_fd, CIOCCRYPT, &cryp) < 0) {
            printf("cryptodev_digest_final: digest failed\n");
            return (0);
        }

        return 1;
    }

    memcpy(md, state->digest_res, EVP_MD_CTX_size(ctx));

    return (ret);
}

static int cryptodev_digest_cleanup(EVP_MD_CTX *ctx)
{
    int ret = 1;
    struct dev_crypto_state *state = EVP_MD_CTX_md_data(ctx);
    struct session_op *sess = &state->d_sess;

    if (state == NULL)
        return 0;

    if (state->d_fd < 0) {
        printf("cryptodev_digest_cleanup: illegal input\n");
        return (0);
    }

    OPENSSL_free(state->mac_data);
    state->mac_data = NULL;
    state->mac_len = 0;

    if (ioctl(state->d_fd, CIOCFSESSION, &sess->ses) < 0) {
        printf("cryptodev_digest_cleanup: failed to close session\n");
        ret = 0;
    } else {
        ret = 1;
    }
    put_dev_crypto(state->d_fd);
    state->d_fd = -1;

    return (ret);
}

static int cryptodev_digest_copy(EVP_MD_CTX *to, const EVP_MD_CTX *from)
{
    struct dev_crypto_state *fstate = EVP_MD_CTX_md_data(from);
    struct dev_crypto_state *dstate = EVP_MD_CTX_md_data(to);
    struct session_op *sess;
    int digest;

    if (dstate == NULL || fstate == NULL)
        return 1;

    memcpy(dstate, fstate, sizeof(struct dev_crypto_state));

    sess = &dstate->d_sess;

    digest = digest_nid_to_cryptodev(EVP_MD_CTX_type(to));

    sess->mackey = dstate->dummy_mac_key;
    sess->mackeylen = digest_key_length(EVP_MD_CTX_type(to));
    sess->mac = digest;

    dstate->d_fd = get_dev_crypto();

    if (ioctl(dstate->d_fd, CIOCGSESSION, sess) < 0) {
        put_dev_crypto(dstate->d_fd);
        dstate->d_fd = -1;
        printf("cryptodev_digest_copy: Open session failed\n");
        return (0);
    }

    if (fstate->mac_len != 0) {
        if (fstate->mac_data != NULL) {
            dstate->mac_data = OPENSSL_malloc(fstate->mac_len);
            if (dstate->mac_data == NULL) {
                printf("cryptodev_digest_copy: mac_data allocation failed\n");
                return (0);
            }
            memcpy(dstate->mac_data, fstate->mac_data, fstate->mac_len);
            dstate->mac_len = fstate->mac_len;
        }
    }

    return 1;
}

static EVP_MD *sha1_md = NULL;
static const EVP_MD *cryptodev_sha1(void)
{
    if (sha1_md == NULL) {
        EVP_MD *md;

        if ((md = EVP_MD_meth_new(NID_sha1, NID_undef)) == NULL
            || !EVP_MD_meth_set_result_size(md, SHA_DIGEST_LENGTH)
            || !EVP_MD_meth_set_flags(md, EVP_MD_FLAG_ONESHOT)
            || !EVP_MD_meth_set_input_blocksize(md, SHA_CBLOCK)
            || !EVP_MD_meth_set_app_datasize(md,
                                             sizeof(struct dev_crypto_state))
            || !EVP_MD_meth_set_init(md, cryptodev_digest_init)
            || !EVP_MD_meth_set_update(md, cryptodev_digest_update)
            || !EVP_MD_meth_set_final(md, cryptodev_digest_final)
            || !EVP_MD_meth_set_copy(md, cryptodev_digest_copy)
            || !EVP_MD_meth_set_cleanup(md, cryptodev_digest_cleanup)) {
            EVP_MD_meth_free(md);
            md = NULL;
        }
        sha1_md = md;
    }
    return sha1_md;
}

static EVP_MD *md5_md = NULL;
static const EVP_MD *cryptodev_md5(void)
{
    if (md5_md == NULL) {
        EVP_MD *md;

        if ((md = EVP_MD_meth_new(NID_md5, NID_undef)) == NULL
            || !EVP_MD_meth_set_result_size(md, 16 /* MD5_DIGEST_LENGTH */)
            || !EVP_MD_meth_set_flags(md, EVP_MD_FLAG_ONESHOT)
            || !EVP_MD_meth_set_input_blocksize(md, 64 /* MD5_CBLOCK */)
            || !EVP_MD_meth_set_app_datasize(md,
                                             sizeof(struct dev_crypto_state))
            || !EVP_MD_meth_set_init(md, cryptodev_digest_init)
            || !EVP_MD_meth_set_update(md, cryptodev_digest_update)
            || !EVP_MD_meth_set_final(md, cryptodev_digest_final)
            || !EVP_MD_meth_set_copy(md, cryptodev_digest_copy)
            || !EVP_MD_meth_set_cleanup(md, cryptodev_digest_cleanup)) {
            EVP_MD_meth_free(md);
            md = NULL;
        }
        md5_md = md;
    }
    return md5_md;
}

# endif                         /* USE_CRYPTODEV_DIGESTS */

static int
cryptodev_engine_digests(ENGINE *e, const EVP_MD **digest,
                         const int **nids, int nid)
{
    if (!digest)
        return (cryptodev_usable_digests(nids));

    switch (nid) {
# ifdef USE_CRYPTODEV_DIGESTS
    case NID_md5:
        *digest = cryptodev_md5();
        break;
    case NID_sha1:
        *digest = cryptodev_sha1();
        break;
    default:
# endif                         /* USE_CRYPTODEV_DIGESTS */
        *digest = NULL;
        break;
    }
    return (*digest != NULL);
}

static int cryptodev_engine_destroy(ENGINE *e)
{
    EVP_CIPHER_meth_free(rc4_cipher);
    rc4_cipher = NULL;
    EVP_CIPHER_meth_free(des_cbc_cipher);
    des_cbc_cipher = NULL;
    EVP_CIPHER_meth_free(des3_cbc_cipher);
    des3_cbc_cipher = NULL;
    EVP_CIPHER_meth_free(bf_cbc_cipher);
    bf_cbc_cipher = NULL;
    EVP_CIPHER_meth_free(cast_cbc_cipher);
    cast_cbc_cipher = NULL;
    EVP_CIPHER_meth_free(aes_cbc_cipher);
    aes_cbc_cipher = NULL;
    EVP_CIPHER_meth_free(aes_192_cbc_cipher);
    aes_192_cbc_cipher = NULL;
    EVP_CIPHER_meth_free(aes_256_cbc_cipher);
    aes_256_cbc_cipher = NULL;
# ifdef CRYPTO_AES_CTR
    EVP_CIPHER_meth_free(aes_ctr_cipher);
    aes_ctr_cipher = NULL;
    EVP_CIPHER_meth_free(aes_192_ctr_cipher);
    aes_192_ctr_cipher = NULL;
    EVP_CIPHER_meth_free(aes_256_ctr_cipher);
    aes_256_ctr_cipher = NULL;
# endif
# ifdef USE_CRYPTODEV_DIGESTS
    EVP_MD_meth_free(sha1_md);
    sha1_md = NULL;
    EVP_MD_meth_free(md5_md);
    md5_md = NULL;
# endif
    RSA_meth_free(cryptodev_rsa);
    cryptodev_rsa = NULL;
#ifndef OPENSSL_NO_DSA
    DSA_meth_free(cryptodev_dsa);
    cryptodev_dsa = NULL;
#endif
#ifndef OPENSSL_NO_DH
    DH_meth_free(cryptodev_dh);
    cryptodev_dh = NULL;
#endif
    return 1;
}

/*
 * Convert a BIGNUM to the representation that /dev/crypto needs.
 * Upon completion of use, the caller is responsible for freeing
 * crp->crp_p.
 */
static int bn2crparam(const BIGNUM *a, struct crparam *crp)
{
    ssize_t bytes, bits;
    u_char *b;

    crp->crp_p = NULL;
    crp->crp_nbits = 0;

    bits = BN_num_bits(a);
    bytes = BN_num_bytes(a);

    b = OPENSSL_zalloc(bytes);
    if (b == NULL)
        return (1);

    crp->crp_p = (caddr_t) b;
    crp->crp_nbits = bits;

    BN_bn2bin(a, b);
    return (0);
}

/* Convert a /dev/crypto parameter to a BIGNUM */
static int crparam2bn(struct crparam *crp, BIGNUM *a)
{
    u_int8_t *pd;
    int i, bytes;

    bytes = (crp->crp_nbits + 7) / 8;

    if (bytes == 0)
        return (-1);

    if ((pd = OPENSSL_malloc(bytes)) == NULL)
        return (-1);

    for (i = 0; i < bytes; i++)
        pd[i] = crp->crp_p[bytes - i - 1];

    BN_bin2bn(pd, bytes, a);
    free(pd);

    return (0);
}

static void zapparams(struct crypt_kop *kop)
{
    int i;

    for (i = 0; i < kop->crk_iparams + kop->crk_oparams; i++) {
        OPENSSL_free(kop->crk_param[i].crp_p);
        kop->crk_param[i].crp_p = NULL;
        kop->crk_param[i].crp_nbits = 0;
    }
}

static int
cryptodev_asym(struct crypt_kop *kop, int rlen, BIGNUM *r, int slen,
               BIGNUM *s)
{
    int fd, ret = -1;

    if ((fd = get_asym_dev_crypto()) < 0)
        return ret;

    if (r) {
        kop->crk_param[kop->crk_iparams].crp_p = OPENSSL_zalloc(rlen);
        if (kop->crk_param[kop->crk_iparams].crp_p == NULL)
            return ret;
        kop->crk_param[kop->crk_iparams].crp_nbits = rlen * 8;
        kop->crk_oparams++;
    }
    if (s) {
        kop->crk_param[kop->crk_iparams + 1].crp_p =
            OPENSSL_zalloc(slen);
        /* No need to free the kop->crk_iparams parameter if it was allocated,
         * callers of this routine have to free allocated parameters through
         * zapparams both in case of success and failure
         */
        if (kop->crk_param[kop->crk_iparams+1].crp_p == NULL)
            return ret;
        kop->crk_param[kop->crk_iparams + 1].crp_nbits = slen * 8;
        kop->crk_oparams++;
    }

    if (ioctl(fd, CIOCKEY, kop) == 0) {
        if (r)
            crparam2bn(&kop->crk_param[kop->crk_iparams], r);
        if (s)
            crparam2bn(&kop->crk_param[kop->crk_iparams + 1], s);
        ret = 0;
    }

    return ret;
}

static int
cryptodev_bn_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                     const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
    struct crypt_kop kop;
    int ret = 1;

    /*
     * Currently, we know we can do mod exp iff we can do any asymmetric
     * operations at all.
     */
    if (cryptodev_asymfeat == 0) {
        ret = BN_mod_exp(r, a, p, m, ctx);
        return (ret);
    }

    memset(&kop, 0, sizeof(kop));
    kop.crk_op = CRK_MOD_EXP;

    /* inputs: a^p % m */
    if (bn2crparam(a, &kop.crk_param[0]))
        goto err;
    if (bn2crparam(p, &kop.crk_param[1]))
        goto err;
    if (bn2crparam(m, &kop.crk_param[2]))
        goto err;
    kop.crk_iparams = 3;

    if (cryptodev_asym(&kop, BN_num_bytes(m), r, 0, NULL)) {
        const RSA_METHOD *meth = RSA_PKCS1_OpenSSL();
        printf("OCF asym process failed, Running in software\n");
        ret = RSA_meth_get_bn_mod_exp(meth)(r, a, p, m, ctx, in_mont);

    } else if (ECANCELED == kop.crk_status) {
        const RSA_METHOD *meth = RSA_PKCS1_OpenSSL();
        printf("OCF hardware operation cancelled. Running in Software\n");
        ret = RSA_meth_get_bn_mod_exp(meth)(r, a, p, m, ctx, in_mont);
    }
    /* else cryptodev operation worked ok ==> ret = 1 */

 err:
    zapparams(&kop);
    return (ret);
}

static int
cryptodev_rsa_nocrt_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa,
                            BN_CTX *ctx)
{
    int r;
    const BIGNUM *n = NULL;
    const BIGNUM *d = NULL;

    ctx = BN_CTX_new();
    RSA_get0_key(rsa, &n, NULL, &d);
    r = cryptodev_bn_mod_exp(r0, I, d, n, ctx, NULL);
    BN_CTX_free(ctx);
    return (r);
}

static int
cryptodev_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx)
{
    struct crypt_kop kop;
    int ret = 1;
    const BIGNUM *p = NULL;
    const BIGNUM *q = NULL;
    const BIGNUM *dmp1 = NULL;
    const BIGNUM *dmq1 = NULL;
    const BIGNUM *iqmp = NULL;
    const BIGNUM *n = NULL;

    RSA_get0_factors(rsa, &p, &q);
    RSA_get0_crt_params(rsa, &dmp1, &dmq1, &iqmp);
    RSA_get0_key(rsa, &n, NULL, NULL);

    if (!p || !q || !dmp1 || !dmq1 || !iqmp) {
        /* XXX 0 means failure?? */
        return (0);
    }

    memset(&kop, 0, sizeof(kop));
    kop.crk_op = CRK_MOD_EXP_CRT;
    /* inputs: rsa->p rsa->q I rsa->dmp1 rsa->dmq1 rsa->iqmp */
    if (bn2crparam(p, &kop.crk_param[0]))
        goto err;
    if (bn2crparam(q, &kop.crk_param[1]))
        goto err;
    if (bn2crparam(I, &kop.crk_param[2]))
        goto err;
    if (bn2crparam(dmp1, &kop.crk_param[3]))
        goto err;
    if (bn2crparam(dmq1, &kop.crk_param[4]))
        goto err;
    if (bn2crparam(iqmp, &kop.crk_param[5]))
        goto err;
    kop.crk_iparams = 6;

    if (cryptodev_asym(&kop, BN_num_bytes(n), r0, 0, NULL)) {
        const RSA_METHOD *meth = RSA_PKCS1_OpenSSL();
        printf("OCF asym process failed, running in Software\n");
        ret = RSA_meth_get_mod_exp(meth)(r0, I, rsa, ctx);

    } else if (ECANCELED == kop.crk_status) {
        const RSA_METHOD *meth = RSA_PKCS1_OpenSSL();
        printf("OCF hardware operation cancelled. Running in Software\n");
        ret = RSA_meth_get_mod_exp(meth)(r0, I, rsa, ctx);
    }
    /* else cryptodev operation worked ok ==> ret = 1 */

 err:
    zapparams(&kop);
    return (ret);
}

#ifndef OPENSSL_NO_DSA
static int
cryptodev_dsa_bn_mod_exp(DSA *dsa, BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                         const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    return cryptodev_bn_mod_exp(r, a, p, m, ctx, m_ctx);
}

static int
cryptodev_dsa_dsa_mod_exp(DSA *dsa, BIGNUM *t1, const BIGNUM *g,
                          const BIGNUM *u1, const BIGNUM *pub_key,
                          const BIGNUM *u2, const BIGNUM *p, BN_CTX *ctx,
                          BN_MONT_CTX *mont)
{
    const BIGNUM *dsag, *dsap, *dsapub_key;
    BIGNUM *t2;
    int ret = 0;
    const DSA_METHOD *meth;
    int (*bn_mod_exp)(DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *, const BIGNUM *,
                      BN_CTX *, BN_MONT_CTX *);

    t2 = BN_new();
    if (t2 == NULL)
        goto err;

    /* v = ( g^u1 * y^u2 mod p ) mod q */
    /* let t1 = g ^ u1 mod p */
    ret = 0;

    DSA_get0_pqg(dsa, &dsap, NULL, &dsag);
    DSA_get0_key(dsa, &dsapub_key, NULL);

    meth = DSA_get_method(dsa);
    if (meth == NULL)
        goto err;
    bn_mod_exp = DSA_meth_get_bn_mod_exp(meth);
    if (bn_mod_exp == NULL)
        goto err;

    if (!bn_mod_exp(dsa, t1, dsag, u1, dsap, ctx, mont))
        goto err;

    /* let t2 = y ^ u2 mod p */
    if (!bn_mod_exp(dsa, t2, dsapub_key, u2, dsap, ctx, mont))
        goto err;
    /* let t1 = t1 * t2 mod p */
    if (!BN_mod_mul(t1, t1, t2, dsap, ctx))
        goto err;

    ret = 1;
 err:
    BN_free(t2);
    return (ret);
}

static DSA_SIG *cryptodev_dsa_do_sign(const unsigned char *dgst, int dlen,
                                      DSA *dsa)
{
    struct crypt_kop kop;
    BIGNUM *r, *s;
    const BIGNUM *dsap = NULL, *dsaq = NULL, *dsag = NULL;
    const BIGNUM *priv_key = NULL;
    DSA_SIG *dsasig, *dsaret = NULL;

    dsasig = DSA_SIG_new();
    if (dsasig == NULL)
        goto err;

    memset(&kop, 0, sizeof(kop));
    kop.crk_op = CRK_DSA_SIGN;

    /* inputs: dgst dsa->p dsa->q dsa->g dsa->priv_key */
    kop.crk_param[0].crp_p = (caddr_t) dgst;
    kop.crk_param[0].crp_nbits = dlen * 8;
    DSA_get0_pqg(dsa, &dsap, &dsaq, &dsag);
    DSA_get0_key(dsa, NULL, &priv_key);
    if (bn2crparam(dsap, &kop.crk_param[1]))
        goto err;
    if (bn2crparam(dsaq, &kop.crk_param[2]))
        goto err;
    if (bn2crparam(dsag, &kop.crk_param[3]))
        goto err;
    if (bn2crparam(priv_key, &kop.crk_param[4]))
        goto err;
    kop.crk_iparams = 5;

    r = BN_new();
    if (r == NULL)
        goto err;
    s = BN_new();
    if (s == NULL)
        goto err;
    if (cryptodev_asym(&kop, BN_num_bytes(dsaq), r,
                       BN_num_bytes(dsaq), s) == 0) {
        DSA_SIG_set0(dsasig, r, s);
        dsaret = dsasig;
    } else {
        dsaret = DSA_meth_get_sign(DSA_OpenSSL())(dgst, dlen, dsa);
    }
 err:
    if (dsaret != dsasig)
        DSA_SIG_free(dsasig);
    kop.crk_param[0].crp_p = NULL;
    zapparams(&kop);
    return dsaret;
}

static int
cryptodev_dsa_verify(const unsigned char *dgst, int dlen,
                     DSA_SIG *sig, DSA *dsa)
{
    struct crypt_kop kop;
    int dsaret = 1;
    const BIGNUM *pr, *ps, *p = NULL, *q = NULL, *g = NULL, *pub_key = NULL;

    memset(&kop, 0, sizeof(kop));
    kop.crk_op = CRK_DSA_VERIFY;

    /* inputs: dgst dsa->p dsa->q dsa->g dsa->pub_key sig->r sig->s */
    kop.crk_param[0].crp_p = (caddr_t) dgst;
    kop.crk_param[0].crp_nbits = dlen * 8;
    DSA_get0_pqg(dsa, &p, &q, &g);
    if (bn2crparam(p, &kop.crk_param[1]))
        goto err;
    if (bn2crparam(q, &kop.crk_param[2]))
        goto err;
    if (bn2crparam(g, &kop.crk_param[3]))
        goto err;
    DSA_get0_key(dsa, &pub_key, NULL);
    if (bn2crparam(pub_key, &kop.crk_param[4]))
        goto err;
    DSA_SIG_get0(sig, &pr, &ps);
    if (bn2crparam(pr, &kop.crk_param[5]))
        goto err;
    if (bn2crparam(ps, &kop.crk_param[6]))
        goto err;
    kop.crk_iparams = 7;

    if (cryptodev_asym(&kop, 0, NULL, 0, NULL) == 0) {
        /*
         * OCF success value is 0, if not zero, change dsaret to fail
         */
        if (0 != kop.crk_status)
            dsaret = 0;
    } else {
        dsaret = DSA_meth_get_verify(DSA_OpenSSL())(dgst, dlen, sig, dsa);
    }
 err:
    kop.crk_param[0].crp_p = NULL;
    zapparams(&kop);
    return (dsaret);
}
#endif

#ifndef OPENSSL_NO_DH
static int
cryptodev_mod_exp_dh(const DH *dh, BIGNUM *r, const BIGNUM *a,
                     const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
                     BN_MONT_CTX *m_ctx)
{
    return (cryptodev_bn_mod_exp(r, a, p, m, ctx, m_ctx));
}

static int
cryptodev_dh_compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
    struct crypt_kop kop;
    int dhret = 1;
    int fd, keylen;
    const BIGNUM *p = NULL;
    const BIGNUM *priv_key = NULL;

    if ((fd = get_asym_dev_crypto()) < 0) {
        const DH_METHOD *meth = DH_OpenSSL();

        return DH_meth_get_compute_key(meth)(key, pub_key, dh);
    }

    DH_get0_pqg(dh, &p, NULL, NULL);
    DH_get0_key(dh, NULL, &priv_key);

    keylen = BN_num_bits(p);

    memset(&kop, 0, sizeof(kop));
    kop.crk_op = CRK_DH_COMPUTE_KEY;

    /* inputs: dh->priv_key pub_key dh->p key */
    if (bn2crparam(priv_key, &kop.crk_param[0]))
        goto err;
    if (bn2crparam(pub_key, &kop.crk_param[1]))
        goto err;
    if (bn2crparam(p, &kop.crk_param[2]))
        goto err;
    kop.crk_iparams = 3;

    kop.crk_param[3].crp_p = (caddr_t) key;
    kop.crk_param[3].crp_nbits = keylen * 8;
    kop.crk_oparams = 1;

    if (ioctl(fd, CIOCKEY, &kop) == -1) {
        const DH_METHOD *meth = DH_OpenSSL();

        dhret = DH_meth_get_compute_key(meth)(key, pub_key, dh);
    }
 err:
    kop.crk_param[3].crp_p = NULL;
    zapparams(&kop);
    return (dhret);
}

#endif /* ndef OPENSSL_NO_DH */

/*
 * ctrl right now is just a wrapper that doesn't do much
 * but I expect we'll want some options soon.
 */
static int
cryptodev_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f) (void))
{
# ifdef HAVE_SYSLOG_R
    struct syslog_data sd = SYSLOG_DATA_INIT;
# endif

    switch (cmd) {
    default:
# ifdef HAVE_SYSLOG_R
        syslog_r(LOG_ERR, &sd, "cryptodev_ctrl: unknown command %d", cmd);
# else
        syslog(LOG_ERR, "cryptodev_ctrl: unknown command %d", cmd);
# endif
        break;
    }
    return (1);
}

void engine_load_cryptodev_int(void)
{
    ENGINE *engine = ENGINE_new();
    int fd;

    if (engine == NULL)
        return;
    if ((fd = get_dev_crypto()) < 0) {
        ENGINE_free(engine);
        return;
    }

    /*
     * find out what asymmetric crypto algorithms we support
     */
    if (ioctl(fd, CIOCASYMFEAT, &cryptodev_asymfeat) == -1) {
        put_dev_crypto(fd);
        ENGINE_free(engine);
        return;
    }
    put_dev_crypto(fd);

    if (!ENGINE_set_id(engine, "cryptodev") ||
        !ENGINE_set_name(engine, "BSD cryptodev engine") ||
        !ENGINE_set_destroy_function(engine, cryptodev_engine_destroy) ||
        !ENGINE_set_ciphers(engine, cryptodev_engine_ciphers) ||
        !ENGINE_set_digests(engine, cryptodev_engine_digests) ||
        !ENGINE_set_ctrl_function(engine, cryptodev_ctrl) ||
        !ENGINE_set_cmd_defns(engine, cryptodev_defns)) {
        ENGINE_free(engine);
        return;
    }

    cryptodev_rsa = RSA_meth_dup(RSA_PKCS1_OpenSSL());
    if (cryptodev_rsa != NULL) {
        RSA_meth_set1_name(cryptodev_rsa, "cryptodev RSA method");
        RSA_meth_set_flags(cryptodev_rsa, 0);
        if (ENGINE_set_RSA(engine, cryptodev_rsa)) {
            if (cryptodev_asymfeat & CRF_MOD_EXP) {
                RSA_meth_set_bn_mod_exp(cryptodev_rsa, cryptodev_bn_mod_exp);
                if (cryptodev_asymfeat & CRF_MOD_EXP_CRT)
                    RSA_meth_set_mod_exp(cryptodev_rsa, cryptodev_rsa_mod_exp);
                else
                    RSA_meth_set_mod_exp(cryptodev_rsa,
                                         cryptodev_rsa_nocrt_mod_exp);
            }
        }
    } else {
        ENGINE_free(engine);
        return;
    }

#ifndef OPENSSL_NO_DSA
    cryptodev_dsa = DSA_meth_dup(DSA_OpenSSL());
    if (cryptodev_dsa != NULL) {
        DSA_meth_set1_name(cryptodev_dsa, "cryptodev DSA method");
        DSA_meth_set_flags(cryptodev_dsa, 0);
        if (ENGINE_set_DSA(engine, cryptodev_dsa)) {
            if (cryptodev_asymfeat & CRF_DSA_SIGN)
                DSA_meth_set_sign(cryptodev_dsa, cryptodev_dsa_do_sign);
            if (cryptodev_asymfeat & CRF_MOD_EXP) {
                DSA_meth_set_bn_mod_exp(cryptodev_dsa,
                                        cryptodev_dsa_bn_mod_exp);
                DSA_meth_set_mod_exp(cryptodev_dsa, cryptodev_dsa_dsa_mod_exp);
            }
            if (cryptodev_asymfeat & CRF_DSA_VERIFY)
                DSA_meth_set_verify(cryptodev_dsa, cryptodev_dsa_verify);
        }
    } else {
        ENGINE_free(engine);
        return;
    }
#endif

#ifndef OPENSSL_NO_DH
    cryptodev_dh = DH_meth_dup(DH_OpenSSL());
    if (cryptodev_dh != NULL) {
        DH_meth_set1_name(cryptodev_dh, "cryptodev DH method");
        DH_meth_set_flags(cryptodev_dh, 0);
        if (ENGINE_set_DH(engine, cryptodev_dh)) {
            if (cryptodev_asymfeat & CRF_MOD_EXP) {
                DH_meth_set_bn_mod_exp(cryptodev_dh, cryptodev_mod_exp_dh);
                if (cryptodev_asymfeat & CRF_DH_COMPUTE_KEY)
                    DH_meth_set_compute_key(cryptodev_dh,
                                            cryptodev_dh_compute_key);
            }
        }
    } else {
        ENGINE_free(engine);
        return;
    }
#endif

    ENGINE_add(engine);
    ENGINE_free(engine);
    ERR_clear_error();
}

#endif                          /* HAVE_CRYPTODEV */
