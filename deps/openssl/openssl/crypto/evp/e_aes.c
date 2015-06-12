/* ====================================================================
 * Copyright (c) 2001-2011 The OpenSSL Project.  All rights reserved.
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
 */

#include <openssl/opensslconf.h>
#ifndef OPENSSL_NO_AES
#include <openssl/crypto.h>
# include <openssl/evp.h>
# include <openssl/err.h>
# include <string.h>
# include <assert.h>
# include <openssl/aes.h>
# include "evp_locl.h"
# ifndef OPENSSL_FIPS
#  include "modes_lcl.h"
#  include <openssl/rand.h>

typedef struct {
    AES_KEY ks;
    block128_f block;
    union {
        cbc128_f cbc;
        ctr128_f ctr;
    } stream;
} EVP_AES_KEY;

typedef struct {
    AES_KEY ks;                 /* AES key schedule to use */
    int key_set;                /* Set if key initialised */
    int iv_set;                 /* Set if an iv is set */
    GCM128_CONTEXT gcm;
    unsigned char *iv;          /* Temporary IV store */
    int ivlen;                  /* IV length */
    int taglen;
    int iv_gen;                 /* It is OK to generate IVs */
    int tls_aad_len;            /* TLS AAD length */
    ctr128_f ctr;
} EVP_AES_GCM_CTX;

typedef struct {
    AES_KEY ks1, ks2;           /* AES key schedules to use */
    XTS128_CONTEXT xts;
    void (*stream) (const unsigned char *in,
                    unsigned char *out, size_t length,
                    const AES_KEY *key1, const AES_KEY *key2,
                    const unsigned char iv[16]);
} EVP_AES_XTS_CTX;

typedef struct {
    AES_KEY ks;                 /* AES key schedule to use */
    int key_set;                /* Set if key initialised */
    int iv_set;                 /* Set if an iv is set */
    int tag_set;                /* Set if tag is valid */
    int len_set;                /* Set if message length set */
    int L, M;                   /* L and M parameters from RFC3610 */
    CCM128_CONTEXT ccm;
    ccm128_f str;
} EVP_AES_CCM_CTX;

#  define MAXBITCHUNK     ((size_t)1<<(sizeof(size_t)*8-4))

#  ifdef VPAES_ASM
int vpaes_set_encrypt_key(const unsigned char *userKey, int bits,
                          AES_KEY *key);
int vpaes_set_decrypt_key(const unsigned char *userKey, int bits,
                          AES_KEY *key);

void vpaes_encrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key);
void vpaes_decrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key);

void vpaes_cbc_encrypt(const unsigned char *in,
                       unsigned char *out,
                       size_t length,
                       const AES_KEY *key, unsigned char *ivec, int enc);
#  endif
#  ifdef BSAES_ASM
void bsaes_cbc_encrypt(const unsigned char *in, unsigned char *out,
                       size_t length, const AES_KEY *key,
                       unsigned char ivec[16], int enc);
void bsaes_ctr32_encrypt_blocks(const unsigned char *in, unsigned char *out,
                                size_t len, const AES_KEY *key,
                                const unsigned char ivec[16]);
void bsaes_xts_encrypt(const unsigned char *inp, unsigned char *out,
                       size_t len, const AES_KEY *key1,
                       const AES_KEY *key2, const unsigned char iv[16]);
void bsaes_xts_decrypt(const unsigned char *inp, unsigned char *out,
                       size_t len, const AES_KEY *key1,
                       const AES_KEY *key2, const unsigned char iv[16]);
#  endif
#  ifdef AES_CTR_ASM
void AES_ctr32_encrypt(const unsigned char *in, unsigned char *out,
                       size_t blocks, const AES_KEY *key,
                       const unsigned char ivec[AES_BLOCK_SIZE]);
#  endif
#  ifdef AES_XTS_ASM
void AES_xts_encrypt(const char *inp, char *out, size_t len,
                     const AES_KEY *key1, const AES_KEY *key2,
                     const unsigned char iv[16]);
void AES_xts_decrypt(const char *inp, char *out, size_t len,
                     const AES_KEY *key1, const AES_KEY *key2,
                     const unsigned char iv[16]);
#  endif

#  if     defined(AES_ASM) && !defined(I386_ONLY) &&      (  \
        ((defined(__i386)       || defined(__i386__)    || \
          defined(_M_IX86)) && defined(OPENSSL_IA32_SSE2))|| \
        defined(__x86_64)       || defined(__x86_64__)  || \
        defined(_M_AMD64)       || defined(_M_X64)      || \
        defined(__INTEL__)                              )

extern unsigned int OPENSSL_ia32cap_P[2];

#   ifdef VPAES_ASM
#    define VPAES_CAPABLE   (OPENSSL_ia32cap_P[1]&(1<<(41-32)))
#   endif
#   ifdef BSAES_ASM
#    define BSAES_CAPABLE   (OPENSSL_ia32cap_P[1]&(1<<(41-32)))
#   endif
/*
 * AES-NI section
 */
#   define AESNI_CAPABLE   (OPENSSL_ia32cap_P[1]&(1<<(57-32)))

int aesni_set_encrypt_key(const unsigned char *userKey, int bits,
                          AES_KEY *key);
int aesni_set_decrypt_key(const unsigned char *userKey, int bits,
                          AES_KEY *key);

void aesni_encrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key);
void aesni_decrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key);

void aesni_ecb_encrypt(const unsigned char *in,
                       unsigned char *out,
                       size_t length, const AES_KEY *key, int enc);
void aesni_cbc_encrypt(const unsigned char *in,
                       unsigned char *out,
                       size_t length,
                       const AES_KEY *key, unsigned char *ivec, int enc);

void aesni_ctr32_encrypt_blocks(const unsigned char *in,
                                unsigned char *out,
                                size_t blocks,
                                const void *key, const unsigned char *ivec);

void aesni_xts_encrypt(const unsigned char *in,
                       unsigned char *out,
                       size_t length,
                       const AES_KEY *key1, const AES_KEY *key2,
                       const unsigned char iv[16]);

void aesni_xts_decrypt(const unsigned char *in,
                       unsigned char *out,
                       size_t length,
                       const AES_KEY *key1, const AES_KEY *key2,
                       const unsigned char iv[16]);

void aesni_ccm64_encrypt_blocks(const unsigned char *in,
                                unsigned char *out,
                                size_t blocks,
                                const void *key,
                                const unsigned char ivec[16],
                                unsigned char cmac[16]);

void aesni_ccm64_decrypt_blocks(const unsigned char *in,
                                unsigned char *out,
                                size_t blocks,
                                const void *key,
                                const unsigned char ivec[16],
                                unsigned char cmac[16]);

static int aesni_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                          const unsigned char *iv, int enc)
{
    int ret, mode;
    EVP_AES_KEY *dat = (EVP_AES_KEY *) ctx->cipher_data;

    mode = ctx->cipher->flags & EVP_CIPH_MODE;
    if ((mode == EVP_CIPH_ECB_MODE || mode == EVP_CIPH_CBC_MODE)
        && !enc) {
        ret = aesni_set_decrypt_key(key, ctx->key_len * 8, ctx->cipher_data);
        dat->block = (block128_f) aesni_decrypt;
        dat->stream.cbc = mode == EVP_CIPH_CBC_MODE ?
            (cbc128_f) aesni_cbc_encrypt : NULL;
    } else {
        ret = aesni_set_encrypt_key(key, ctx->key_len * 8, ctx->cipher_data);
        dat->block = (block128_f) aesni_encrypt;
        if (mode == EVP_CIPH_CBC_MODE)
            dat->stream.cbc = (cbc128_f) aesni_cbc_encrypt;
        else if (mode == EVP_CIPH_CTR_MODE)
            dat->stream.ctr = (ctr128_f) aesni_ctr32_encrypt_blocks;
        else
            dat->stream.cbc = NULL;
    }

    if (ret < 0) {
        EVPerr(EVP_F_AESNI_INIT_KEY, EVP_R_AES_KEY_SETUP_FAILED);
        return 0;
    }

    return 1;
}

static int aesni_cbc_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                            const unsigned char *in, size_t len)
{
    aesni_cbc_encrypt(in, out, len, ctx->cipher_data, ctx->iv, ctx->encrypt);

    return 1;
}

static int aesni_ecb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                            const unsigned char *in, size_t len)
{
    size_t bl = ctx->cipher->block_size;

    if (len < bl)
        return 1;

    aesni_ecb_encrypt(in, out, len, ctx->cipher_data, ctx->encrypt);

    return 1;
}

#   define aesni_ofb_cipher aes_ofb_cipher
static int aesni_ofb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                            const unsigned char *in, size_t len);

#   define aesni_cfb_cipher aes_cfb_cipher
static int aesni_cfb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                            const unsigned char *in, size_t len);

#   define aesni_cfb8_cipher aes_cfb8_cipher
static int aesni_cfb8_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                             const unsigned char *in, size_t len);

#   define aesni_cfb1_cipher aes_cfb1_cipher
static int aesni_cfb1_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                             const unsigned char *in, size_t len);

#   define aesni_ctr_cipher aes_ctr_cipher
static int aesni_ctr_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                            const unsigned char *in, size_t len);

static int aesni_gcm_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                              const unsigned char *iv, int enc)
{
    EVP_AES_GCM_CTX *gctx = ctx->cipher_data;
    if (!iv && !key)
        return 1;
    if (key) {
        aesni_set_encrypt_key(key, ctx->key_len * 8, &gctx->ks);
        CRYPTO_gcm128_init(&gctx->gcm, &gctx->ks, (block128_f) aesni_encrypt);
        gctx->ctr = (ctr128_f) aesni_ctr32_encrypt_blocks;
        /*
         * If we have an iv can set it directly, otherwise use saved IV.
         */
        if (iv == NULL && gctx->iv_set)
            iv = gctx->iv;
        if (iv) {
            CRYPTO_gcm128_setiv(&gctx->gcm, iv, gctx->ivlen);
            gctx->iv_set = 1;
        }
        gctx->key_set = 1;
    } else {
        /* If key set use IV, otherwise copy */
        if (gctx->key_set)
            CRYPTO_gcm128_setiv(&gctx->gcm, iv, gctx->ivlen);
        else
            memcpy(gctx->iv, iv, gctx->ivlen);
        gctx->iv_set = 1;
        gctx->iv_gen = 0;
    }
    return 1;
}

#   define aesni_gcm_cipher aes_gcm_cipher
static int aesni_gcm_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                            const unsigned char *in, size_t len);

static int aesni_xts_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                              const unsigned char *iv, int enc)
{
    EVP_AES_XTS_CTX *xctx = ctx->cipher_data;
    if (!iv && !key)
        return 1;

    if (key) {
        /* key_len is two AES keys */
        if (enc) {
            aesni_set_encrypt_key(key, ctx->key_len * 4, &xctx->ks1);
            xctx->xts.block1 = (block128_f) aesni_encrypt;
            xctx->stream = aesni_xts_encrypt;
        } else {
            aesni_set_decrypt_key(key, ctx->key_len * 4, &xctx->ks1);
            xctx->xts.block1 = (block128_f) aesni_decrypt;
            xctx->stream = aesni_xts_decrypt;
        }

        aesni_set_encrypt_key(key + ctx->key_len / 2,
                              ctx->key_len * 4, &xctx->ks2);
        xctx->xts.block2 = (block128_f) aesni_encrypt;

        xctx->xts.key1 = &xctx->ks1;
    }

    if (iv) {
        xctx->xts.key2 = &xctx->ks2;
        memcpy(ctx->iv, iv, 16);
    }

    return 1;
}

#   define aesni_xts_cipher aes_xts_cipher
static int aesni_xts_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                            const unsigned char *in, size_t len);

static int aesni_ccm_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                              const unsigned char *iv, int enc)
{
    EVP_AES_CCM_CTX *cctx = ctx->cipher_data;
    if (!iv && !key)
        return 1;
    if (key) {
        aesni_set_encrypt_key(key, ctx->key_len * 8, &cctx->ks);
        CRYPTO_ccm128_init(&cctx->ccm, cctx->M, cctx->L,
                           &cctx->ks, (block128_f) aesni_encrypt);
        cctx->str = enc ? (ccm128_f) aesni_ccm64_encrypt_blocks :
            (ccm128_f) aesni_ccm64_decrypt_blocks;
        cctx->key_set = 1;
    }
    if (iv) {
        memcpy(ctx->iv, iv, 15 - cctx->L);
        cctx->iv_set = 1;
    }
    return 1;
}

#   define aesni_ccm_cipher aes_ccm_cipher
static int aesni_ccm_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                            const unsigned char *in, size_t len);

#   define BLOCK_CIPHER_generic(nid,keylen,blocksize,ivlen,nmode,mode,MODE,flags) \
static const EVP_CIPHER aesni_##keylen##_##mode = { \
        nid##_##keylen##_##nmode,blocksize,keylen/8,ivlen, \
        flags|EVP_CIPH_##MODE##_MODE,   \
        aesni_init_key,                 \
        aesni_##mode##_cipher,          \
        NULL,                           \
        sizeof(EVP_AES_KEY),            \
        NULL,NULL,NULL,NULL }; \
static const EVP_CIPHER aes_##keylen##_##mode = { \
        nid##_##keylen##_##nmode,blocksize,     \
        keylen/8,ivlen, \
        flags|EVP_CIPH_##MODE##_MODE,   \
        aes_init_key,                   \
        aes_##mode##_cipher,            \
        NULL,                           \
        sizeof(EVP_AES_KEY),            \
        NULL,NULL,NULL,NULL }; \
const EVP_CIPHER *EVP_aes_##keylen##_##mode(void) \
{ return AESNI_CAPABLE?&aesni_##keylen##_##mode:&aes_##keylen##_##mode; }

#   define BLOCK_CIPHER_custom(nid,keylen,blocksize,ivlen,mode,MODE,flags) \
static const EVP_CIPHER aesni_##keylen##_##mode = { \
        nid##_##keylen##_##mode,blocksize, \
        (EVP_CIPH_##MODE##_MODE==EVP_CIPH_XTS_MODE?2:1)*keylen/8, ivlen, \
        flags|EVP_CIPH_##MODE##_MODE,   \
        aesni_##mode##_init_key,        \
        aesni_##mode##_cipher,          \
        aes_##mode##_cleanup,           \
        sizeof(EVP_AES_##MODE##_CTX),   \
        NULL,NULL,aes_##mode##_ctrl,NULL }; \
static const EVP_CIPHER aes_##keylen##_##mode = { \
        nid##_##keylen##_##mode,blocksize, \
        (EVP_CIPH_##MODE##_MODE==EVP_CIPH_XTS_MODE?2:1)*keylen/8, ivlen, \
        flags|EVP_CIPH_##MODE##_MODE,   \
        aes_##mode##_init_key,          \
        aes_##mode##_cipher,            \
        aes_##mode##_cleanup,           \
        sizeof(EVP_AES_##MODE##_CTX),   \
        NULL,NULL,aes_##mode##_ctrl,NULL }; \
const EVP_CIPHER *EVP_aes_##keylen##_##mode(void) \
{ return AESNI_CAPABLE?&aesni_##keylen##_##mode:&aes_##keylen##_##mode; }

#  else

#   define BLOCK_CIPHER_generic(nid,keylen,blocksize,ivlen,nmode,mode,MODE,flags) \
static const EVP_CIPHER aes_##keylen##_##mode = { \
        nid##_##keylen##_##nmode,blocksize,keylen/8,ivlen, \
        flags|EVP_CIPH_##MODE##_MODE,   \
        aes_init_key,                   \
        aes_##mode##_cipher,            \
        NULL,                           \
        sizeof(EVP_AES_KEY),            \
        NULL,NULL,NULL,NULL }; \
const EVP_CIPHER *EVP_aes_##keylen##_##mode(void) \
{ return &aes_##keylen##_##mode; }

#   define BLOCK_CIPHER_custom(nid,keylen,blocksize,ivlen,mode,MODE,flags) \
static const EVP_CIPHER aes_##keylen##_##mode = { \
        nid##_##keylen##_##mode,blocksize, \
        (EVP_CIPH_##MODE##_MODE==EVP_CIPH_XTS_MODE?2:1)*keylen/8, ivlen, \
        flags|EVP_CIPH_##MODE##_MODE,   \
        aes_##mode##_init_key,          \
        aes_##mode##_cipher,            \
        aes_##mode##_cleanup,           \
        sizeof(EVP_AES_##MODE##_CTX),   \
        NULL,NULL,aes_##mode##_ctrl,NULL }; \
const EVP_CIPHER *EVP_aes_##keylen##_##mode(void) \
{ return &aes_##keylen##_##mode; }
#  endif

#  define BLOCK_CIPHER_generic_pack(nid,keylen,flags)             \
        BLOCK_CIPHER_generic(nid,keylen,16,16,cbc,cbc,CBC,flags|EVP_CIPH_FLAG_DEFAULT_ASN1)     \
        BLOCK_CIPHER_generic(nid,keylen,16,0,ecb,ecb,ECB,flags|EVP_CIPH_FLAG_DEFAULT_ASN1)      \
        BLOCK_CIPHER_generic(nid,keylen,1,16,ofb128,ofb,OFB,flags|EVP_CIPH_FLAG_DEFAULT_ASN1)   \
        BLOCK_CIPHER_generic(nid,keylen,1,16,cfb128,cfb,CFB,flags|EVP_CIPH_FLAG_DEFAULT_ASN1)   \
        BLOCK_CIPHER_generic(nid,keylen,1,16,cfb1,cfb1,CFB,flags)       \
        BLOCK_CIPHER_generic(nid,keylen,1,16,cfb8,cfb8,CFB,flags)       \
        BLOCK_CIPHER_generic(nid,keylen,1,16,ctr,ctr,CTR,flags)

static int aes_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                        const unsigned char *iv, int enc)
{
    int ret, mode;
    EVP_AES_KEY *dat = (EVP_AES_KEY *) ctx->cipher_data;

    mode = ctx->cipher->flags & EVP_CIPH_MODE;
    if ((mode == EVP_CIPH_ECB_MODE || mode == EVP_CIPH_CBC_MODE)
        && !enc)
#  ifdef BSAES_CAPABLE
        if (BSAES_CAPABLE && mode == EVP_CIPH_CBC_MODE) {
            ret = AES_set_decrypt_key(key, ctx->key_len * 8, &dat->ks);
            dat->block = (block128_f) AES_decrypt;
            dat->stream.cbc = (cbc128_f) bsaes_cbc_encrypt;
        } else
#  endif
#  ifdef VPAES_CAPABLE
        if (VPAES_CAPABLE) {
            ret = vpaes_set_decrypt_key(key, ctx->key_len * 8, &dat->ks);
            dat->block = (block128_f) vpaes_decrypt;
            dat->stream.cbc = mode == EVP_CIPH_CBC_MODE ?
                (cbc128_f) vpaes_cbc_encrypt : NULL;
        } else
#  endif
        {
            ret = AES_set_decrypt_key(key, ctx->key_len * 8, &dat->ks);
            dat->block = (block128_f) AES_decrypt;
            dat->stream.cbc = mode == EVP_CIPH_CBC_MODE ?
                (cbc128_f) AES_cbc_encrypt : NULL;
    } else
#  ifdef BSAES_CAPABLE
    if (BSAES_CAPABLE && mode == EVP_CIPH_CTR_MODE) {
        ret = AES_set_encrypt_key(key, ctx->key_len * 8, &dat->ks);
        dat->block = (block128_f) AES_encrypt;
        dat->stream.ctr = (ctr128_f) bsaes_ctr32_encrypt_blocks;
    } else
#  endif
#  ifdef VPAES_CAPABLE
    if (VPAES_CAPABLE) {
        ret = vpaes_set_encrypt_key(key, ctx->key_len * 8, &dat->ks);
        dat->block = (block128_f) vpaes_encrypt;
        dat->stream.cbc = mode == EVP_CIPH_CBC_MODE ?
            (cbc128_f) vpaes_cbc_encrypt : NULL;
    } else
#  endif
    {
        ret = AES_set_encrypt_key(key, ctx->key_len * 8, &dat->ks);
        dat->block = (block128_f) AES_encrypt;
        dat->stream.cbc = mode == EVP_CIPH_CBC_MODE ?
            (cbc128_f) AES_cbc_encrypt : NULL;
#  ifdef AES_CTR_ASM
        if (mode == EVP_CIPH_CTR_MODE)
            dat->stream.ctr = (ctr128_f) AES_ctr32_encrypt;
#  endif
    }

    if (ret < 0) {
        EVPerr(EVP_F_AES_INIT_KEY, EVP_R_AES_KEY_SETUP_FAILED);
        return 0;
    }

    return 1;
}

static int aes_cbc_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                          const unsigned char *in, size_t len)
{
    EVP_AES_KEY *dat = (EVP_AES_KEY *) ctx->cipher_data;

    if (dat->stream.cbc)
        (*dat->stream.cbc) (in, out, len, &dat->ks, ctx->iv, ctx->encrypt);
    else if (ctx->encrypt)
        CRYPTO_cbc128_encrypt(in, out, len, &dat->ks, ctx->iv, dat->block);
    else
        CRYPTO_cbc128_encrypt(in, out, len, &dat->ks, ctx->iv, dat->block);

    return 1;
}

static int aes_ecb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                          const unsigned char *in, size_t len)
{
    size_t bl = ctx->cipher->block_size;
    size_t i;
    EVP_AES_KEY *dat = (EVP_AES_KEY *) ctx->cipher_data;

    if (len < bl)
        return 1;

    for (i = 0, len -= bl; i <= len; i += bl)
        (*dat->block) (in + i, out + i, &dat->ks);

    return 1;
}

static int aes_ofb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                          const unsigned char *in, size_t len)
{
    EVP_AES_KEY *dat = (EVP_AES_KEY *) ctx->cipher_data;

    CRYPTO_ofb128_encrypt(in, out, len, &dat->ks,
                          ctx->iv, &ctx->num, dat->block);
    return 1;
}

static int aes_cfb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                          const unsigned char *in, size_t len)
{
    EVP_AES_KEY *dat = (EVP_AES_KEY *) ctx->cipher_data;

    CRYPTO_cfb128_encrypt(in, out, len, &dat->ks,
                          ctx->iv, &ctx->num, ctx->encrypt, dat->block);
    return 1;
}

static int aes_cfb8_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                           const unsigned char *in, size_t len)
{
    EVP_AES_KEY *dat = (EVP_AES_KEY *) ctx->cipher_data;

    CRYPTO_cfb128_8_encrypt(in, out, len, &dat->ks,
                            ctx->iv, &ctx->num, ctx->encrypt, dat->block);
    return 1;
}

static int aes_cfb1_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                           const unsigned char *in, size_t len)
{
    EVP_AES_KEY *dat = (EVP_AES_KEY *) ctx->cipher_data;

    if (ctx->flags & EVP_CIPH_FLAG_LENGTH_BITS) {
        CRYPTO_cfb128_1_encrypt(in, out, len, &dat->ks,
                                ctx->iv, &ctx->num, ctx->encrypt, dat->block);
        return 1;
    }

    while (len >= MAXBITCHUNK) {
        CRYPTO_cfb128_1_encrypt(in, out, MAXBITCHUNK * 8, &dat->ks,
                                ctx->iv, &ctx->num, ctx->encrypt, dat->block);
        len -= MAXBITCHUNK;
    }
    if (len)
        CRYPTO_cfb128_1_encrypt(in, out, len * 8, &dat->ks,
                                ctx->iv, &ctx->num, ctx->encrypt, dat->block);

    return 1;
}

static int aes_ctr_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                          const unsigned char *in, size_t len)
{
    unsigned int num = ctx->num;
    EVP_AES_KEY *dat = (EVP_AES_KEY *) ctx->cipher_data;

    if (dat->stream.ctr)
        CRYPTO_ctr128_encrypt_ctr32(in, out, len, &dat->ks,
                                    ctx->iv, ctx->buf, &num, dat->stream.ctr);
    else
        CRYPTO_ctr128_encrypt(in, out, len, &dat->ks,
                              ctx->iv, ctx->buf, &num, dat->block);
    ctx->num = (size_t)num;
    return 1;
}

BLOCK_CIPHER_generic_pack(NID_aes, 128, EVP_CIPH_FLAG_FIPS)
    BLOCK_CIPHER_generic_pack(NID_aes, 192, EVP_CIPH_FLAG_FIPS)
    BLOCK_CIPHER_generic_pack(NID_aes, 256, EVP_CIPH_FLAG_FIPS)

static int aes_gcm_cleanup(EVP_CIPHER_CTX *c)
{
    EVP_AES_GCM_CTX *gctx = c->cipher_data;
    OPENSSL_cleanse(&gctx->gcm, sizeof(gctx->gcm));
    if (gctx->iv != c->iv)
        OPENSSL_free(gctx->iv);
    return 1;
}

/* increment counter (64-bit int) by 1 */
static void ctr64_inc(unsigned char *counter)
{
    int n = 8;
    unsigned char c;

    do {
        --n;
        c = counter[n];
        ++c;
        counter[n] = c;
        if (c)
            return;
    } while (n);
}

static int aes_gcm_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr)
{
    EVP_AES_GCM_CTX *gctx = c->cipher_data;
    switch (type) {
    case EVP_CTRL_INIT:
        gctx->key_set = 0;
        gctx->iv_set = 0;
        gctx->ivlen = c->cipher->iv_len;
        gctx->iv = c->iv;
        gctx->taglen = -1;
        gctx->iv_gen = 0;
        gctx->tls_aad_len = -1;
        return 1;

    case EVP_CTRL_GCM_SET_IVLEN:
        if (arg <= 0)
            return 0;
#  ifdef OPENSSL_FIPS
        if (FIPS_module_mode() && !(c->flags & EVP_CIPH_FLAG_NON_FIPS_ALLOW)
            && arg < 12)
            return 0;
#  endif
        /* Allocate memory for IV if needed */
        if ((arg > EVP_MAX_IV_LENGTH) && (arg > gctx->ivlen)) {
            if (gctx->iv != c->iv)
                OPENSSL_free(gctx->iv);
            gctx->iv = OPENSSL_malloc(arg);
            if (!gctx->iv)
                return 0;
        }
        gctx->ivlen = arg;
        return 1;

    case EVP_CTRL_GCM_SET_TAG:
        if (arg <= 0 || arg > 16 || c->encrypt)
            return 0;
        memcpy(c->buf, ptr, arg);
        gctx->taglen = arg;
        return 1;

    case EVP_CTRL_GCM_GET_TAG:
        if (arg <= 0 || arg > 16 || !c->encrypt || gctx->taglen < 0)
            return 0;
        memcpy(ptr, c->buf, arg);
        return 1;

    case EVP_CTRL_GCM_SET_IV_FIXED:
        /* Special case: -1 length restores whole IV */
        if (arg == -1) {
            memcpy(gctx->iv, ptr, gctx->ivlen);
            gctx->iv_gen = 1;
            return 1;
        }
        /*
         * Fixed field must be at least 4 bytes and invocation field at least
         * 8.
         */
        if ((arg < 4) || (gctx->ivlen - arg) < 8)
            return 0;
        if (arg)
            memcpy(gctx->iv, ptr, arg);
        if (c->encrypt && RAND_bytes(gctx->iv + arg, gctx->ivlen - arg) <= 0)
            return 0;
        gctx->iv_gen = 1;
        return 1;

    case EVP_CTRL_GCM_IV_GEN:
        if (gctx->iv_gen == 0 || gctx->key_set == 0)
            return 0;
        CRYPTO_gcm128_setiv(&gctx->gcm, gctx->iv, gctx->ivlen);
        if (arg <= 0 || arg > gctx->ivlen)
            arg = gctx->ivlen;
        memcpy(ptr, gctx->iv + gctx->ivlen - arg, arg);
        /*
         * Invocation field will be at least 8 bytes in size and so no need
         * to check wrap around or increment more than last 8 bytes.
         */
        ctr64_inc(gctx->iv + gctx->ivlen - 8);
        gctx->iv_set = 1;
        return 1;

    case EVP_CTRL_GCM_SET_IV_INV:
        if (gctx->iv_gen == 0 || gctx->key_set == 0 || c->encrypt)
            return 0;
        memcpy(gctx->iv + gctx->ivlen - arg, ptr, arg);
        CRYPTO_gcm128_setiv(&gctx->gcm, gctx->iv, gctx->ivlen);
        gctx->iv_set = 1;
        return 1;

    case EVP_CTRL_AEAD_TLS1_AAD:
        /* Save the AAD for later use */
        if (arg != EVP_AEAD_TLS1_AAD_LEN)
            return 0;
        memcpy(c->buf, ptr, arg);
        gctx->tls_aad_len = arg;
        {
            unsigned int len = c->buf[arg - 2] << 8 | c->buf[arg - 1];
            /* Correct length for explicit IV */
            len -= EVP_GCM_TLS_EXPLICIT_IV_LEN;
            /* If decrypting correct for tag too */
            if (!c->encrypt)
                len -= EVP_GCM_TLS_TAG_LEN;
            c->buf[arg - 2] = len >> 8;
            c->buf[arg - 1] = len & 0xff;
        }
        /* Extra padding: tag appended to record */
        return EVP_GCM_TLS_TAG_LEN;

    case EVP_CTRL_COPY:
        {
            EVP_CIPHER_CTX *out = ptr;
            EVP_AES_GCM_CTX *gctx_out = out->cipher_data;
            if (gctx->gcm.key) {
                if (gctx->gcm.key != &gctx->ks)
                    return 0;
                gctx_out->gcm.key = &gctx_out->ks;
            }
            if (gctx->iv == c->iv)
                gctx_out->iv = out->iv;
            else {
                gctx_out->iv = OPENSSL_malloc(gctx->ivlen);
                if (!gctx_out->iv)
                    return 0;
                memcpy(gctx_out->iv, gctx->iv, gctx->ivlen);
            }
            return 1;
        }

    default:
        return -1;

    }
}

static int aes_gcm_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                            const unsigned char *iv, int enc)
{
    EVP_AES_GCM_CTX *gctx = ctx->cipher_data;
    if (!iv && !key)
        return 1;
    if (key) {
        do {
#  ifdef BSAES_CAPABLE
            if (BSAES_CAPABLE) {
                AES_set_encrypt_key(key, ctx->key_len * 8, &gctx->ks);
                CRYPTO_gcm128_init(&gctx->gcm, &gctx->ks,
                                   (block128_f) AES_encrypt);
                gctx->ctr = (ctr128_f) bsaes_ctr32_encrypt_blocks;
                break;
            } else
#  endif
#  ifdef VPAES_CAPABLE
            if (VPAES_CAPABLE) {
                vpaes_set_encrypt_key(key, ctx->key_len * 8, &gctx->ks);
                CRYPTO_gcm128_init(&gctx->gcm, &gctx->ks,
                                   (block128_f) vpaes_encrypt);
                gctx->ctr = NULL;
                break;
            } else
#  endif
                (void)0;        /* terminate potentially open 'else' */

            AES_set_encrypt_key(key, ctx->key_len * 8, &gctx->ks);
            CRYPTO_gcm128_init(&gctx->gcm, &gctx->ks,
                               (block128_f) AES_encrypt);
#  ifdef AES_CTR_ASM
            gctx->ctr = (ctr128_f) AES_ctr32_encrypt;
#  else
            gctx->ctr = NULL;
#  endif
        } while (0);

        /*
         * If we have an iv can set it directly, otherwise use saved IV.
         */
        if (iv == NULL && gctx->iv_set)
            iv = gctx->iv;
        if (iv) {
            CRYPTO_gcm128_setiv(&gctx->gcm, iv, gctx->ivlen);
            gctx->iv_set = 1;
        }
        gctx->key_set = 1;
    } else {
        /* If key set use IV, otherwise copy */
        if (gctx->key_set)
            CRYPTO_gcm128_setiv(&gctx->gcm, iv, gctx->ivlen);
        else
            memcpy(gctx->iv, iv, gctx->ivlen);
        gctx->iv_set = 1;
        gctx->iv_gen = 0;
    }
    return 1;
}

/*
 * Handle TLS GCM packet format. This consists of the last portion of the IV
 * followed by the payload and finally the tag. On encrypt generate IV,
 * encrypt payload and write the tag. On verify retrieve IV, decrypt payload
 * and verify tag.
 */

static int aes_gcm_tls_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                              const unsigned char *in, size_t len)
{
    EVP_AES_GCM_CTX *gctx = ctx->cipher_data;
    int rv = -1;
    /* Encrypt/decrypt must be performed in place */
    if (out != in
        || len < (EVP_GCM_TLS_EXPLICIT_IV_LEN + EVP_GCM_TLS_TAG_LEN))
        return -1;
    /*
     * Set IV from start of buffer or generate IV and write to start of
     * buffer.
     */
    if (EVP_CIPHER_CTX_ctrl(ctx, ctx->encrypt ?
                            EVP_CTRL_GCM_IV_GEN : EVP_CTRL_GCM_SET_IV_INV,
                            EVP_GCM_TLS_EXPLICIT_IV_LEN, out) <= 0)
        goto err;
    /* Use saved AAD */
    if (CRYPTO_gcm128_aad(&gctx->gcm, ctx->buf, gctx->tls_aad_len))
        goto err;
    /* Fix buffer and length to point to payload */
    in += EVP_GCM_TLS_EXPLICIT_IV_LEN;
    out += EVP_GCM_TLS_EXPLICIT_IV_LEN;
    len -= EVP_GCM_TLS_EXPLICIT_IV_LEN + EVP_GCM_TLS_TAG_LEN;
    if (ctx->encrypt) {
        /* Encrypt payload */
        if (gctx->ctr) {
            if (CRYPTO_gcm128_encrypt_ctr32(&gctx->gcm,
                                            in, out, len, gctx->ctr))
                goto err;
        } else {
            if (CRYPTO_gcm128_encrypt(&gctx->gcm, in, out, len))
                goto err;
        }
        out += len;
        /* Finally write tag */
        CRYPTO_gcm128_tag(&gctx->gcm, out, EVP_GCM_TLS_TAG_LEN);
        rv = len + EVP_GCM_TLS_EXPLICIT_IV_LEN + EVP_GCM_TLS_TAG_LEN;
    } else {
        /* Decrypt */
        if (gctx->ctr) {
            if (CRYPTO_gcm128_decrypt_ctr32(&gctx->gcm,
                                            in, out, len, gctx->ctr))
                goto err;
        } else {
            if (CRYPTO_gcm128_decrypt(&gctx->gcm, in, out, len))
                goto err;
        }
        /* Retrieve tag */
        CRYPTO_gcm128_tag(&gctx->gcm, ctx->buf, EVP_GCM_TLS_TAG_LEN);
        /* If tag mismatch wipe buffer */
        if (CRYPTO_memcmp(ctx->buf, in + len, EVP_GCM_TLS_TAG_LEN)) {
            OPENSSL_cleanse(out, len);
            goto err;
        }
        rv = len;
    }

 err:
    gctx->iv_set = 0;
    gctx->tls_aad_len = -1;
    return rv;
}

static int aes_gcm_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                          const unsigned char *in, size_t len)
{
    EVP_AES_GCM_CTX *gctx = ctx->cipher_data;
    /* If not set up, return error */
    if (!gctx->key_set)
        return -1;

    if (gctx->tls_aad_len >= 0)
        return aes_gcm_tls_cipher(ctx, out, in, len);

    if (!gctx->iv_set)
        return -1;
    if (in) {
        if (out == NULL) {
            if (CRYPTO_gcm128_aad(&gctx->gcm, in, len))
                return -1;
        } else if (ctx->encrypt) {
            if (gctx->ctr) {
                if (CRYPTO_gcm128_encrypt_ctr32(&gctx->gcm,
                                                in, out, len, gctx->ctr))
                    return -1;
            } else {
                if (CRYPTO_gcm128_encrypt(&gctx->gcm, in, out, len))
                    return -1;
            }
        } else {
            if (gctx->ctr) {
                if (CRYPTO_gcm128_decrypt_ctr32(&gctx->gcm,
                                                in, out, len, gctx->ctr))
                    return -1;
            } else {
                if (CRYPTO_gcm128_decrypt(&gctx->gcm, in, out, len))
                    return -1;
            }
        }
        return len;
    } else {
        if (!ctx->encrypt) {
            if (gctx->taglen < 0)
                return -1;
            if (CRYPTO_gcm128_finish(&gctx->gcm, ctx->buf, gctx->taglen) != 0)
                return -1;
            gctx->iv_set = 0;
            return 0;
        }
        CRYPTO_gcm128_tag(&gctx->gcm, ctx->buf, 16);
        gctx->taglen = 16;
        /* Don't reuse the IV */
        gctx->iv_set = 0;
        return 0;
    }

}

#  define CUSTOM_FLAGS    (EVP_CIPH_FLAG_DEFAULT_ASN1 \
                | EVP_CIPH_CUSTOM_IV | EVP_CIPH_FLAG_CUSTOM_CIPHER \
                | EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_CTRL_INIT \
                | EVP_CIPH_CUSTOM_COPY)

BLOCK_CIPHER_custom(NID_aes, 128, 1, 12, gcm, GCM,
                    EVP_CIPH_FLAG_FIPS | EVP_CIPH_FLAG_AEAD_CIPHER |
                    CUSTOM_FLAGS)
    BLOCK_CIPHER_custom(NID_aes, 192, 1, 12, gcm, GCM,
                    EVP_CIPH_FLAG_FIPS | EVP_CIPH_FLAG_AEAD_CIPHER |
                    CUSTOM_FLAGS)
    BLOCK_CIPHER_custom(NID_aes, 256, 1, 12, gcm, GCM,
                    EVP_CIPH_FLAG_FIPS | EVP_CIPH_FLAG_AEAD_CIPHER |
                    CUSTOM_FLAGS)

static int aes_xts_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr)
{
    EVP_AES_XTS_CTX *xctx = c->cipher_data;
    if (type == EVP_CTRL_COPY) {
        EVP_CIPHER_CTX *out = ptr;
        EVP_AES_XTS_CTX *xctx_out = out->cipher_data;
        if (xctx->xts.key1) {
            if (xctx->xts.key1 != &xctx->ks1)
                return 0;
            xctx_out->xts.key1 = &xctx_out->ks1;
        }
        if (xctx->xts.key2) {
            if (xctx->xts.key2 != &xctx->ks2)
                return 0;
            xctx_out->xts.key2 = &xctx_out->ks2;
        }
        return 1;
    } else if (type != EVP_CTRL_INIT)
        return -1;
    /* key1 and key2 are used as an indicator both key and IV are set */
    xctx->xts.key1 = NULL;
    xctx->xts.key2 = NULL;
    return 1;
}

static int aes_xts_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                            const unsigned char *iv, int enc)
{
    EVP_AES_XTS_CTX *xctx = ctx->cipher_data;
    if (!iv && !key)
        return 1;

    if (key)
        do {
#  ifdef AES_XTS_ASM
            xctx->stream = enc ? AES_xts_encrypt : AES_xts_decrypt;
#  else
            xctx->stream = NULL;
#  endif
            /* key_len is two AES keys */
#  ifdef BSAES_CAPABLE
            if (BSAES_CAPABLE)
                xctx->stream = enc ? bsaes_xts_encrypt : bsaes_xts_decrypt;
            else
#  endif
#  ifdef VPAES_CAPABLE
            if (VPAES_CAPABLE) {
                if (enc) {
                    vpaes_set_encrypt_key(key, ctx->key_len * 4, &xctx->ks1);
                    xctx->xts.block1 = (block128_f) vpaes_encrypt;
                } else {
                    vpaes_set_decrypt_key(key, ctx->key_len * 4, &xctx->ks1);
                    xctx->xts.block1 = (block128_f) vpaes_decrypt;
                }

                vpaes_set_encrypt_key(key + ctx->key_len / 2,
                                      ctx->key_len * 4, &xctx->ks2);
                xctx->xts.block2 = (block128_f) vpaes_encrypt;

                xctx->xts.key1 = &xctx->ks1;
                break;
            } else
#  endif
                (void)0;        /* terminate potentially open 'else' */

            if (enc) {
                AES_set_encrypt_key(key, ctx->key_len * 4, &xctx->ks1);
                xctx->xts.block1 = (block128_f) AES_encrypt;
            } else {
                AES_set_decrypt_key(key, ctx->key_len * 4, &xctx->ks1);
                xctx->xts.block1 = (block128_f) AES_decrypt;
            }

            AES_set_encrypt_key(key + ctx->key_len / 2,
                                ctx->key_len * 4, &xctx->ks2);
            xctx->xts.block2 = (block128_f) AES_encrypt;

            xctx->xts.key1 = &xctx->ks1;
        } while (0);

    if (iv) {
        xctx->xts.key2 = &xctx->ks2;
        memcpy(ctx->iv, iv, 16);
    }

    return 1;
}

static int aes_xts_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                          const unsigned char *in, size_t len)
{
    EVP_AES_XTS_CTX *xctx = ctx->cipher_data;
    if (!xctx->xts.key1 || !xctx->xts.key2)
        return 0;
    if (!out || !in || len < AES_BLOCK_SIZE)
        return 0;
#  ifdef OPENSSL_FIPS
    /* Requirement of SP800-38E */
    if (FIPS_module_mode() && !(ctx->flags & EVP_CIPH_FLAG_NON_FIPS_ALLOW) &&
        (len > (1UL << 20) * 16)) {
        EVPerr(EVP_F_AES_XTS_CIPHER, EVP_R_TOO_LARGE);
        return 0;
    }
#  endif
    if (xctx->stream)
        (*xctx->stream) (in, out, len,
                         xctx->xts.key1, xctx->xts.key2, ctx->iv);
    else if (CRYPTO_xts128_encrypt(&xctx->xts, ctx->iv, in, out, len,
                                   ctx->encrypt))
        return 0;
    return 1;
}

#  define aes_xts_cleanup NULL

#  define XTS_FLAGS       (EVP_CIPH_FLAG_DEFAULT_ASN1 | EVP_CIPH_CUSTOM_IV \
                         | EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_CTRL_INIT \
                         | EVP_CIPH_CUSTOM_COPY)

BLOCK_CIPHER_custom(NID_aes, 128, 1, 16, xts, XTS,
                    EVP_CIPH_FLAG_FIPS | XTS_FLAGS)
    BLOCK_CIPHER_custom(NID_aes, 256, 1, 16, xts, XTS,
                    EVP_CIPH_FLAG_FIPS | XTS_FLAGS)

static int aes_ccm_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr)
{
    EVP_AES_CCM_CTX *cctx = c->cipher_data;
    switch (type) {
    case EVP_CTRL_INIT:
        cctx->key_set = 0;
        cctx->iv_set = 0;
        cctx->L = 8;
        cctx->M = 12;
        cctx->tag_set = 0;
        cctx->len_set = 0;
        return 1;

    case EVP_CTRL_CCM_SET_IVLEN:
        arg = 15 - arg;
    case EVP_CTRL_CCM_SET_L:
        if (arg < 2 || arg > 8)
            return 0;
        cctx->L = arg;
        return 1;

    case EVP_CTRL_CCM_SET_TAG:
        if ((arg & 1) || arg < 4 || arg > 16)
            return 0;
        if ((c->encrypt && ptr) || (!c->encrypt && !ptr))
            return 0;
        if (ptr) {
            cctx->tag_set = 1;
            memcpy(c->buf, ptr, arg);
        }
        cctx->M = arg;
        return 1;

    case EVP_CTRL_CCM_GET_TAG:
        if (!c->encrypt || !cctx->tag_set)
            return 0;
        if (!CRYPTO_ccm128_tag(&cctx->ccm, ptr, (size_t)arg))
            return 0;
        cctx->tag_set = 0;
        cctx->iv_set = 0;
        cctx->len_set = 0;
        return 1;

    case EVP_CTRL_COPY:
        {
            EVP_CIPHER_CTX *out = ptr;
            EVP_AES_CCM_CTX *cctx_out = out->cipher_data;
            if (cctx->ccm.key) {
                if (cctx->ccm.key != &cctx->ks)
                    return 0;
                cctx_out->ccm.key = &cctx_out->ks;
            }
            return 1;
        }

    default:
        return -1;

    }
}

static int aes_ccm_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                            const unsigned char *iv, int enc)
{
    EVP_AES_CCM_CTX *cctx = ctx->cipher_data;
    if (!iv && !key)
        return 1;
    if (key)
        do {
#  ifdef VPAES_CAPABLE
            if (VPAES_CAPABLE) {
                vpaes_set_encrypt_key(key, ctx->key_len * 8, &cctx->ks);
                CRYPTO_ccm128_init(&cctx->ccm, cctx->M, cctx->L,
                                   &cctx->ks, (block128_f) vpaes_encrypt);
                cctx->str = NULL;
                cctx->key_set = 1;
                break;
            }
#  endif
            AES_set_encrypt_key(key, ctx->key_len * 8, &cctx->ks);
            CRYPTO_ccm128_init(&cctx->ccm, cctx->M, cctx->L,
                               &cctx->ks, (block128_f) AES_encrypt);
            cctx->str = NULL;
            cctx->key_set = 1;
        } while (0);
    if (iv) {
        memcpy(ctx->iv, iv, 15 - cctx->L);
        cctx->iv_set = 1;
    }
    return 1;
}

static int aes_ccm_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                          const unsigned char *in, size_t len)
{
    EVP_AES_CCM_CTX *cctx = ctx->cipher_data;
    CCM128_CONTEXT *ccm = &cctx->ccm;
    /* If not set up, return error */
    if (!cctx->iv_set && !cctx->key_set)
        return -1;
    if (!ctx->encrypt && !cctx->tag_set)
        return -1;
    if (!out) {
        if (!in) {
            if (CRYPTO_ccm128_setiv(ccm, ctx->iv, 15 - cctx->L, len))
                return -1;
            cctx->len_set = 1;
            return len;
        }
        /* If have AAD need message length */
        if (!cctx->len_set && len)
            return -1;
        CRYPTO_ccm128_aad(ccm, in, len);
        return len;
    }
    /* EVP_*Final() doesn't return any data */
    if (!in)
        return 0;
    /* If not set length yet do it */
    if (!cctx->len_set) {
        if (CRYPTO_ccm128_setiv(ccm, ctx->iv, 15 - cctx->L, len))
            return -1;
        cctx->len_set = 1;
    }
    if (ctx->encrypt) {
        if (cctx->str ? CRYPTO_ccm128_encrypt_ccm64(ccm, in, out, len,
                                                    cctx->str) :
            CRYPTO_ccm128_encrypt(ccm, in, out, len))
            return -1;
        cctx->tag_set = 1;
        return len;
    } else {
        int rv = -1;
        if (cctx->str ? !CRYPTO_ccm128_decrypt_ccm64(ccm, in, out, len,
                                                     cctx->str) :
            !CRYPTO_ccm128_decrypt(ccm, in, out, len)) {
            unsigned char tag[16];
            if (CRYPTO_ccm128_tag(ccm, tag, cctx->M)) {
                if (!CRYPTO_memcmp(tag, ctx->buf, cctx->M))
                    rv = len;
            }
        }
        if (rv == -1)
            OPENSSL_cleanse(out, len);
        cctx->iv_set = 0;
        cctx->tag_set = 0;
        cctx->len_set = 0;
        return rv;
    }

}

#  define aes_ccm_cleanup NULL

BLOCK_CIPHER_custom(NID_aes, 128, 1, 12, ccm, CCM,
                    EVP_CIPH_FLAG_FIPS | CUSTOM_FLAGS)
    BLOCK_CIPHER_custom(NID_aes, 192, 1, 12, ccm, CCM,
                    EVP_CIPH_FLAG_FIPS | CUSTOM_FLAGS)
    BLOCK_CIPHER_custom(NID_aes, 256, 1, 12, ccm, CCM,
                    EVP_CIPH_FLAG_FIPS | CUSTOM_FLAGS)
# endif
#endif
