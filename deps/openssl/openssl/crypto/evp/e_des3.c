/* crypto/evp/e_des3.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include "cryptlib.h"
#ifndef OPENSSL_NO_DES
# include <openssl/evp.h>
# include <openssl/objects.h>
# include "evp_locl.h"
# include <openssl/des.h>
# include <openssl/rand.h>

/* Block use of implementations in FIPS mode */
# undef EVP_CIPH_FLAG_FIPS
# define EVP_CIPH_FLAG_FIPS      0

typedef struct {
    union {
        double align;
        DES_key_schedule ks[3];
    } ks;
    union {
        void (*cbc) (const void *, void *, size_t,
                     const DES_key_schedule *, unsigned char *);
    } stream;
} DES_EDE_KEY;
# define ks1 ks.ks[0]
# define ks2 ks.ks[1]
# define ks3 ks.ks[2]

# if defined(AES_ASM) && (defined(__sparc) || defined(__sparc__))
/* ---------^^^ this is not a typo, just a way to detect that
 * assembler support was in general requested... */
#  include "sparc_arch.h"

extern unsigned int OPENSSL_sparcv9cap_P[];

#  define SPARC_DES_CAPABLE       (OPENSSL_sparcv9cap_P[1] & CFR_DES)

void des_t4_key_expand(const void *key, DES_key_schedule *ks);
void des_t4_ede3_cbc_encrypt(const void *inp, void *out, size_t len,
                             const DES_key_schedule ks[3], unsigned char iv[8]);
void des_t4_ede3_cbc_decrypt(const void *inp, void *out, size_t len,
                             const DES_key_schedule ks[3], unsigned char iv[8]);
# endif

static int des_ede_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                            const unsigned char *iv, int enc);

static int des_ede3_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                             const unsigned char *iv, int enc);

static int des3_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr);

# define data(ctx) ((DES_EDE_KEY *)(ctx)->cipher_data)

/*
 * Because of various casts and different args can't use
 * IMPLEMENT_BLOCK_CIPHER
 */

static int des_ede_ecb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                              const unsigned char *in, size_t inl)
{
    BLOCK_CIPHER_ecb_loop()
        DES_ecb3_encrypt((const_DES_cblock *)(in + i),
                         (DES_cblock *)(out + i),
                         &data(ctx)->ks1, &data(ctx)->ks2,
                         &data(ctx)->ks3, ctx->encrypt);
    return 1;
}

static int des_ede_ofb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                              const unsigned char *in, size_t inl)
{
    while (inl >= EVP_MAXCHUNK) {
        DES_ede3_ofb64_encrypt(in, out, (long)EVP_MAXCHUNK,
                               &data(ctx)->ks1, &data(ctx)->ks2,
                               &data(ctx)->ks3, (DES_cblock *)ctx->iv,
                               &ctx->num);
        inl -= EVP_MAXCHUNK;
        in += EVP_MAXCHUNK;
        out += EVP_MAXCHUNK;
    }
    if (inl)
        DES_ede3_ofb64_encrypt(in, out, (long)inl,
                               &data(ctx)->ks1, &data(ctx)->ks2,
                               &data(ctx)->ks3, (DES_cblock *)ctx->iv,
                               &ctx->num);

    return 1;
}

static int des_ede_cbc_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                              const unsigned char *in, size_t inl)
{
    DES_EDE_KEY *dat = data(ctx);

# ifdef KSSL_DEBUG
    {
        int i;
        fprintf(stderr, "des_ede_cbc_cipher(ctx=%p, buflen=%d)\n", ctx,
                ctx->buf_len);
        fprintf(stderr, "\t iv= ");
        for (i = 0; i < 8; i++)
            fprintf(stderr, "%02X", ctx->iv[i]);
        fprintf(stderr, "\n");
    }
# endif                         /* KSSL_DEBUG */
    if (dat->stream.cbc) {
        (*dat->stream.cbc) (in, out, inl, dat->ks.ks, ctx->iv);
        return 1;
    }

    while (inl >= EVP_MAXCHUNK) {
        DES_ede3_cbc_encrypt(in, out, (long)EVP_MAXCHUNK,
                             &dat->ks1, &dat->ks2, &dat->ks3,
                             (DES_cblock *)ctx->iv, ctx->encrypt);
        inl -= EVP_MAXCHUNK;
        in += EVP_MAXCHUNK;
        out += EVP_MAXCHUNK;
    }
    if (inl)
        DES_ede3_cbc_encrypt(in, out, (long)inl,
                             &dat->ks1, &dat->ks2, &dat->ks3,
                             (DES_cblock *)ctx->iv, ctx->encrypt);
    return 1;
}

static int des_ede_cfb64_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                                const unsigned char *in, size_t inl)
{
    while (inl >= EVP_MAXCHUNK) {
        DES_ede3_cfb64_encrypt(in, out, (long)EVP_MAXCHUNK,
                               &data(ctx)->ks1, &data(ctx)->ks2,
                               &data(ctx)->ks3, (DES_cblock *)ctx->iv,
                               &ctx->num, ctx->encrypt);
        inl -= EVP_MAXCHUNK;
        in += EVP_MAXCHUNK;
        out += EVP_MAXCHUNK;
    }
    if (inl)
        DES_ede3_cfb64_encrypt(in, out, (long)inl,
                               &data(ctx)->ks1, &data(ctx)->ks2,
                               &data(ctx)->ks3, (DES_cblock *)ctx->iv,
                               &ctx->num, ctx->encrypt);
    return 1;
}

/*
 * Although we have a CFB-r implementation for 3-DES, it doesn't pack the
 * right way, so wrap it here
 */
static int des_ede3_cfb1_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                                const unsigned char *in, size_t inl)
{
    size_t n;
    unsigned char c[1], d[1];

    if (!EVP_CIPHER_CTX_test_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS))
            inl *= 8;
    for (n = 0; n < inl; ++n) {
        c[0] = (in[n / 8] & (1 << (7 - n % 8))) ? 0x80 : 0;
        DES_ede3_cfb_encrypt(c, d, 1, 1,
                             &data(ctx)->ks1, &data(ctx)->ks2,
                             &data(ctx)->ks3, (DES_cblock *)ctx->iv,
                             ctx->encrypt);
        out[n / 8] = (out[n / 8] & ~(0x80 >> (unsigned int)(n % 8)))
            | ((d[0] & 0x80) >> (unsigned int)(n % 8));
    }

    return 1;
}

static int des_ede3_cfb8_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                                const unsigned char *in, size_t inl)
{
    while (inl >= EVP_MAXCHUNK) {
        DES_ede3_cfb_encrypt(in, out, 8, (long)EVP_MAXCHUNK,
                             &data(ctx)->ks1, &data(ctx)->ks2,
                             &data(ctx)->ks3, (DES_cblock *)ctx->iv,
                             ctx->encrypt);
        inl -= EVP_MAXCHUNK;
        in += EVP_MAXCHUNK;
        out += EVP_MAXCHUNK;
    }
    if (inl)
        DES_ede3_cfb_encrypt(in, out, 8, (long)inl,
                             &data(ctx)->ks1, &data(ctx)->ks2,
                             &data(ctx)->ks3, (DES_cblock *)ctx->iv,
                             ctx->encrypt);
    return 1;
}

BLOCK_CIPHER_defs(des_ede, DES_EDE_KEY, NID_des_ede, 8, 16, 8, 64,
                  EVP_CIPH_RAND_KEY | EVP_CIPH_FLAG_DEFAULT_ASN1,
                  des_ede_init_key, NULL, NULL, NULL, des3_ctrl)
# define des_ede3_cfb64_cipher des_ede_cfb64_cipher
# define des_ede3_ofb_cipher des_ede_ofb_cipher
# define des_ede3_cbc_cipher des_ede_cbc_cipher
# define des_ede3_ecb_cipher des_ede_ecb_cipher
    BLOCK_CIPHER_defs(des_ede3, DES_EDE_KEY, NID_des_ede3, 8, 24, 8, 64,
                  EVP_CIPH_RAND_KEY | EVP_CIPH_FLAG_FIPS |
                  EVP_CIPH_FLAG_DEFAULT_ASN1, des_ede3_init_key, NULL, NULL, NULL,
                  des3_ctrl)

    BLOCK_CIPHER_def_cfb(des_ede3, DES_EDE_KEY, NID_des_ede3, 24, 8, 1,
                     EVP_CIPH_RAND_KEY | EVP_CIPH_FLAG_FIPS |
                     EVP_CIPH_FLAG_DEFAULT_ASN1, des_ede3_init_key, NULL, NULL,
                     NULL, des3_ctrl)

    BLOCK_CIPHER_def_cfb(des_ede3, DES_EDE_KEY, NID_des_ede3, 24, 8, 8,
                     EVP_CIPH_RAND_KEY | EVP_CIPH_FLAG_FIPS |
                     EVP_CIPH_FLAG_DEFAULT_ASN1, des_ede3_init_key, NULL, NULL,
                     NULL, des3_ctrl)

static int des_ede_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                            const unsigned char *iv, int enc)
{
    DES_cblock *deskey = (DES_cblock *)key;
    DES_EDE_KEY *dat = data(ctx);

    dat->stream.cbc = NULL;
# if defined(SPARC_DES_CAPABLE)
    if (SPARC_DES_CAPABLE) {
        int mode = ctx->cipher->flags & EVP_CIPH_MODE;

        if (mode == EVP_CIPH_CBC_MODE) {
            des_t4_key_expand(&deskey[0], &dat->ks1);
            des_t4_key_expand(&deskey[1], &dat->ks2);
            memcpy(&dat->ks3, &dat->ks1, sizeof(dat->ks1));
            dat->stream.cbc = enc ? des_t4_ede3_cbc_encrypt :
                des_t4_ede3_cbc_decrypt;
            return 1;
        }
    }
# endif
# ifdef EVP_CHECK_DES_KEY
    if (DES_set_key_checked(&deskey[0], &dat->ks1)
        || DES_set_key_checked(&deskey[1], &dat->ks2))
        return 0;
# else
    DES_set_key_unchecked(&deskey[0], &dat->ks1);
    DES_set_key_unchecked(&deskey[1], &dat->ks2);
# endif
    memcpy(&dat->ks3, &dat->ks1, sizeof(dat->ks1));
    return 1;
}

static int des_ede3_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                             const unsigned char *iv, int enc)
{
    DES_cblock *deskey = (DES_cblock *)key;
    DES_EDE_KEY *dat = data(ctx);

# ifdef KSSL_DEBUG
    {
        int i;
        fprintf(stderr, "des_ede3_init_key(ctx=%p)\n", ctx);
        fprintf(stderr, "\tKEY= ");
        for (i = 0; i < 24; i++)
            fprintf(stderr, "%02X", key[i]);
        fprintf(stderr, "\n");
        if (iv) {
            fprintf(stderr, "\t IV= ");
            for (i = 0; i < 8; i++)
                fprintf(stderr, "%02X", iv[i]);
            fprintf(stderr, "\n");
        }
    }
# endif                         /* KSSL_DEBUG */

    dat->stream.cbc = NULL;
# if defined(SPARC_DES_CAPABLE)
    if (SPARC_DES_CAPABLE) {
        int mode = ctx->cipher->flags & EVP_CIPH_MODE;

        if (mode == EVP_CIPH_CBC_MODE) {
            des_t4_key_expand(&deskey[0], &dat->ks1);
            des_t4_key_expand(&deskey[1], &dat->ks2);
            des_t4_key_expand(&deskey[2], &dat->ks3);
            dat->stream.cbc = enc ? des_t4_ede3_cbc_encrypt :
                des_t4_ede3_cbc_decrypt;
            return 1;
        }
    }
# endif
# ifdef EVP_CHECK_DES_KEY
    if (DES_set_key_checked(&deskey[0], &dat->ks1)
        || DES_set_key_checked(&deskey[1], &dat->ks2)
        || DES_set_key_checked(&deskey[2], &dat->ks3))
        return 0;
# else
    DES_set_key_unchecked(&deskey[0], &dat->ks1);
    DES_set_key_unchecked(&deskey[1], &dat->ks2);
    DES_set_key_unchecked(&deskey[2], &dat->ks3);
# endif
    return 1;
}

static int des3_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr)
{

    DES_cblock *deskey = ptr;

    switch (type) {
    case EVP_CTRL_RAND_KEY:
        if (RAND_bytes(ptr, c->key_len) <= 0)
            return 0;
        DES_set_odd_parity(deskey);
        if (c->key_len >= 16)
            DES_set_odd_parity(deskey + 1);
        if (c->key_len >= 24)
            DES_set_odd_parity(deskey + 2);
        return 1;

    default:
        return -1;
    }
}

const EVP_CIPHER *EVP_des_ede(void)
{
    return &des_ede_ecb;
}

const EVP_CIPHER *EVP_des_ede3(void)
{
    return &des_ede3_ecb;
}

# ifndef OPENSSL_NO_SHA

#  include <openssl/sha.h>

static const unsigned char wrap_iv[8] =
    { 0x4a, 0xdd, 0xa2, 0x2c, 0x79, 0xe8, 0x21, 0x05 };

static int des_ede3_unwrap(EVP_CIPHER_CTX *ctx, unsigned char *out,
                           const unsigned char *in, size_t inl)
{
    unsigned char icv[8], iv[8], sha1tmp[SHA_DIGEST_LENGTH];
    int rv = -1;
    if (inl < 24)
        return -1;
    if (out == NULL)
        return inl - 16;
    memcpy(ctx->iv, wrap_iv, 8);
    /* Decrypt first block which will end up as icv */
    des_ede_cbc_cipher(ctx, icv, in, 8);
    /* Decrypt central blocks */
    /*
     * If decrypting in place move whole output along a block so the next
     * des_ede_cbc_cipher is in place.
     */
    if (out == in) {
        memmove(out, out + 8, inl - 8);
        in -= 8;
    }
    des_ede_cbc_cipher(ctx, out, in + 8, inl - 16);
    /* Decrypt final block which will be IV */
    des_ede_cbc_cipher(ctx, iv, in + inl - 8, 8);
    /* Reverse order of everything */
    BUF_reverse(icv, NULL, 8);
    BUF_reverse(out, NULL, inl - 16);
    BUF_reverse(ctx->iv, iv, 8);
    /* Decrypt again using new IV */
    des_ede_cbc_cipher(ctx, out, out, inl - 16);
    des_ede_cbc_cipher(ctx, icv, icv, 8);
    /* Work out SHA1 hash of first portion */
    SHA1(out, inl - 16, sha1tmp);

    if (!CRYPTO_memcmp(sha1tmp, icv, 8))
        rv = inl - 16;
    OPENSSL_cleanse(icv, 8);
    OPENSSL_cleanse(sha1tmp, SHA_DIGEST_LENGTH);
    OPENSSL_cleanse(iv, 8);
    OPENSSL_cleanse(ctx->iv, 8);
    if (rv == -1)
        OPENSSL_cleanse(out, inl - 16);

    return rv;
}

static int des_ede3_wrap(EVP_CIPHER_CTX *ctx, unsigned char *out,
                         const unsigned char *in, size_t inl)
{
    unsigned char sha1tmp[SHA_DIGEST_LENGTH];
    if (out == NULL)
        return inl + 16;
    /* Copy input to output buffer + 8 so we have space for IV */
    memmove(out + 8, in, inl);
    /* Work out ICV */
    SHA1(in, inl, sha1tmp);
    memcpy(out + inl + 8, sha1tmp, 8);
    OPENSSL_cleanse(sha1tmp, SHA_DIGEST_LENGTH);
    /* Generate random IV */
    if (RAND_bytes(ctx->iv, 8) <= 0)
        return -1;
    memcpy(out, ctx->iv, 8);
    /* Encrypt everything after IV in place */
    des_ede_cbc_cipher(ctx, out + 8, out + 8, inl + 8);
    BUF_reverse(out, NULL, inl + 16);
    memcpy(ctx->iv, wrap_iv, 8);
    des_ede_cbc_cipher(ctx, out, out, inl + 16);
    return inl + 16;
}

static int des_ede3_wrap_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                                const unsigned char *in, size_t inl)
{
    /*
     * Sanity check input length: we typically only wrap keys so EVP_MAXCHUNK
     * is more than will ever be needed. Also input length must be a multiple
     * of 8 bits.
     */
    if (inl >= EVP_MAXCHUNK || inl % 8)
        return -1;
    if (ctx->encrypt)
        return des_ede3_wrap(ctx, out, in, inl);
    else
        return des_ede3_unwrap(ctx, out, in, inl);
}

static const EVP_CIPHER des3_wrap = {
    NID_id_smime_alg_CMS3DESwrap,
    8, 24, 0,
    EVP_CIPH_WRAP_MODE | EVP_CIPH_CUSTOM_IV | EVP_CIPH_FLAG_CUSTOM_CIPHER
        | EVP_CIPH_FLAG_DEFAULT_ASN1,
    des_ede3_init_key, des_ede3_wrap_cipher,
    NULL,
    sizeof(DES_EDE_KEY),
    NULL, NULL, NULL, NULL
};

const EVP_CIPHER *EVP_des_ede3_wrap(void)
{
    return &des3_wrap;
}

# endif
#endif
