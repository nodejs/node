/* crypto/ecdh/ecdhtest.c */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * The Elliptic Curve Public-Key Crypto Library (ECC Code) included
 * herein is developed by SUN MICROSYSTEMS, INC., and is contributed
 * to the OpenSSL project.
 *
 * The ECC Code is licensed pursuant to the OpenSSL open source
 * license provided below.
 *
 * The ECDH software is originally written by Douglas Stebila of
 * Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../e_os.h"

#include <openssl/opensslconf.h> /* for OPENSSL_NO_ECDH */
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/objects.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#ifdef OPENSSL_NO_ECDH
int main(int argc, char *argv[])
{
    printf("No ECDH support\n");
    return (0);
}
#else
# include <openssl/ec.h>
# include <openssl/ecdh.h>

# ifdef OPENSSL_SYS_WIN16
#  define MS_CALLBACK     _far _loadds
# else
#  define MS_CALLBACK
# endif

# if 0
static void MS_CALLBACK cb(int p, int n, void *arg);
# endif

static const char rnd_seed[] =
    "string to make the random number generator think it has entropy";

static const int KDF1_SHA1_len = 20;
static void *KDF1_SHA1(const void *in, size_t inlen, void *out,
                       size_t *outlen)
{
# ifndef OPENSSL_NO_SHA
    if (*outlen < SHA_DIGEST_LENGTH)
        return NULL;
    else
        *outlen = SHA_DIGEST_LENGTH;
    return SHA1(in, inlen, out);
# else
    return NULL;
# endif
}

static int test_ecdh_curve(int nid, const char *text, BN_CTX *ctx, BIO *out)
{
    EC_KEY *a = NULL;
    EC_KEY *b = NULL;
    BIGNUM *x_a = NULL, *y_a = NULL, *x_b = NULL, *y_b = NULL;
    char buf[12];
    unsigned char *abuf = NULL, *bbuf = NULL;
    int i, alen, blen, aout, bout, ret = 0;
    const EC_GROUP *group;

    a = EC_KEY_new_by_curve_name(nid);
    b = EC_KEY_new_by_curve_name(nid);
    if (a == NULL || b == NULL)
        goto err;

    group = EC_KEY_get0_group(a);

    if ((x_a = BN_new()) == NULL)
        goto err;
    if ((y_a = BN_new()) == NULL)
        goto err;
    if ((x_b = BN_new()) == NULL)
        goto err;
    if ((y_b = BN_new()) == NULL)
        goto err;

    BIO_puts(out, "Testing key generation with ");
    BIO_puts(out, text);
# ifdef NOISY
    BIO_puts(out, "\n");
# else
    (void)BIO_flush(out);
# endif

    if (!EC_KEY_generate_key(a))
        goto err;

    if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) ==
        NID_X9_62_prime_field) {
        if (!EC_POINT_get_affine_coordinates_GFp
            (group, EC_KEY_get0_public_key(a), x_a, y_a, ctx))
            goto err;
    }
# ifndef OPENSSL_NO_EC2M
    else {
        if (!EC_POINT_get_affine_coordinates_GF2m(group,
                                                  EC_KEY_get0_public_key(a),
                                                  x_a, y_a, ctx))
            goto err;
    }
# endif
# ifdef NOISY
    BIO_puts(out, "  pri 1=");
    BN_print(out, a->priv_key);
    BIO_puts(out, "\n  pub 1=");
    BN_print(out, x_a);
    BIO_puts(out, ",");
    BN_print(out, y_a);
    BIO_puts(out, "\n");
# else
    BIO_printf(out, " .");
    (void)BIO_flush(out);
# endif

    if (!EC_KEY_generate_key(b))
        goto err;

    if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) ==
        NID_X9_62_prime_field) {
        if (!EC_POINT_get_affine_coordinates_GFp
            (group, EC_KEY_get0_public_key(b), x_b, y_b, ctx))
            goto err;
    }
# ifndef OPENSSL_NO_EC2M
    else {
        if (!EC_POINT_get_affine_coordinates_GF2m(group,
                                                  EC_KEY_get0_public_key(b),
                                                  x_b, y_b, ctx))
            goto err;
    }
# endif

# ifdef NOISY
    BIO_puts(out, "  pri 2=");
    BN_print(out, b->priv_key);
    BIO_puts(out, "\n  pub 2=");
    BN_print(out, x_b);
    BIO_puts(out, ",");
    BN_print(out, y_b);
    BIO_puts(out, "\n");
# else
    BIO_printf(out, ".");
    (void)BIO_flush(out);
# endif

    alen = KDF1_SHA1_len;
    abuf = (unsigned char *)OPENSSL_malloc(alen);
    aout =
        ECDH_compute_key(abuf, alen, EC_KEY_get0_public_key(b), a, KDF1_SHA1);

# ifdef NOISY
    BIO_puts(out, "  key1 =");
    for (i = 0; i < aout; i++) {
        sprintf(buf, "%02X", abuf[i]);
        BIO_puts(out, buf);
    }
    BIO_puts(out, "\n");
# else
    BIO_printf(out, ".");
    (void)BIO_flush(out);
# endif

    blen = KDF1_SHA1_len;
    bbuf = (unsigned char *)OPENSSL_malloc(blen);
    bout =
        ECDH_compute_key(bbuf, blen, EC_KEY_get0_public_key(a), b, KDF1_SHA1);

# ifdef NOISY
    BIO_puts(out, "  key2 =");
    for (i = 0; i < bout; i++) {
        sprintf(buf, "%02X", bbuf[i]);
        BIO_puts(out, buf);
    }
    BIO_puts(out, "\n");
# else
    BIO_printf(out, ".");
    (void)BIO_flush(out);
# endif

    if ((aout < 4) || (bout != aout) || (memcmp(abuf, bbuf, aout) != 0)) {
# ifndef NOISY
        BIO_printf(out, " failed\n\n");
        BIO_printf(out, "key a:\n");
        BIO_printf(out, "private key: ");
        BN_print(out, EC_KEY_get0_private_key(a));
        BIO_printf(out, "\n");
        BIO_printf(out, "public key (x,y): ");
        BN_print(out, x_a);
        BIO_printf(out, ",");
        BN_print(out, y_a);
        BIO_printf(out, "\nkey b:\n");
        BIO_printf(out, "private key: ");
        BN_print(out, EC_KEY_get0_private_key(b));
        BIO_printf(out, "\n");
        BIO_printf(out, "public key (x,y): ");
        BN_print(out, x_b);
        BIO_printf(out, ",");
        BN_print(out, y_b);
        BIO_printf(out, "\n");
        BIO_printf(out, "generated key a: ");
        for (i = 0; i < bout; i++) {
            sprintf(buf, "%02X", bbuf[i]);
            BIO_puts(out, buf);
        }
        BIO_printf(out, "\n");
        BIO_printf(out, "generated key b: ");
        for (i = 0; i < aout; i++) {
            sprintf(buf, "%02X", abuf[i]);
            BIO_puts(out, buf);
        }
        BIO_printf(out, "\n");
# endif
        fprintf(stderr, "Error in ECDH routines\n");
        ret = 0;
    } else {
# ifndef NOISY
        BIO_printf(out, " ok\n");
# endif
        ret = 1;
    }
 err:
    ERR_print_errors_fp(stderr);

    if (abuf != NULL)
        OPENSSL_free(abuf);
    if (bbuf != NULL)
        OPENSSL_free(bbuf);
    if (x_a)
        BN_free(x_a);
    if (y_a)
        BN_free(y_a);
    if (x_b)
        BN_free(x_b);
    if (y_b)
        BN_free(y_b);
    if (b)
        EC_KEY_free(b);
    if (a)
        EC_KEY_free(a);
    return (ret);
}

/* Keys and shared secrets from RFC 7027 */

static const unsigned char bp256_da[] = {
    0x81, 0xDB, 0x1E, 0xE1, 0x00, 0x15, 0x0F, 0xF2, 0xEA, 0x33, 0x8D, 0x70,
    0x82, 0x71, 0xBE, 0x38, 0x30, 0x0C, 0xB5, 0x42, 0x41, 0xD7, 0x99, 0x50,
    0xF7, 0x7B, 0x06, 0x30, 0x39, 0x80, 0x4F, 0x1D
};

static const unsigned char bp256_db[] = {
    0x55, 0xE4, 0x0B, 0xC4, 0x1E, 0x37, 0xE3, 0xE2, 0xAD, 0x25, 0xC3, 0xC6,
    0x65, 0x45, 0x11, 0xFF, 0xA8, 0x47, 0x4A, 0x91, 0xA0, 0x03, 0x20, 0x87,
    0x59, 0x38, 0x52, 0xD3, 0xE7, 0xD7, 0x6B, 0xD3
};

static const unsigned char bp256_Z[] = {
    0x89, 0xAF, 0xC3, 0x9D, 0x41, 0xD3, 0xB3, 0x27, 0x81, 0x4B, 0x80, 0x94,
    0x0B, 0x04, 0x25, 0x90, 0xF9, 0x65, 0x56, 0xEC, 0x91, 0xE6, 0xAE, 0x79,
    0x39, 0xBC, 0xE3, 0x1F, 0x3A, 0x18, 0xBF, 0x2B
};

static const unsigned char bp384_da[] = {
    0x1E, 0x20, 0xF5, 0xE0, 0x48, 0xA5, 0x88, 0x6F, 0x1F, 0x15, 0x7C, 0x74,
    0xE9, 0x1B, 0xDE, 0x2B, 0x98, 0xC8, 0xB5, 0x2D, 0x58, 0xE5, 0x00, 0x3D,
    0x57, 0x05, 0x3F, 0xC4, 0xB0, 0xBD, 0x65, 0xD6, 0xF1, 0x5E, 0xB5, 0xD1,
    0xEE, 0x16, 0x10, 0xDF, 0x87, 0x07, 0x95, 0x14, 0x36, 0x27, 0xD0, 0x42
};

static const unsigned char bp384_db[] = {
    0x03, 0x26, 0x40, 0xBC, 0x60, 0x03, 0xC5, 0x92, 0x60, 0xF7, 0x25, 0x0C,
    0x3D, 0xB5, 0x8C, 0xE6, 0x47, 0xF9, 0x8E, 0x12, 0x60, 0xAC, 0xCE, 0x4A,
    0xCD, 0xA3, 0xDD, 0x86, 0x9F, 0x74, 0xE0, 0x1F, 0x8B, 0xA5, 0xE0, 0x32,
    0x43, 0x09, 0xDB, 0x6A, 0x98, 0x31, 0x49, 0x7A, 0xBA, 0xC9, 0x66, 0x70
};

static const unsigned char bp384_Z[] = {
    0x0B, 0xD9, 0xD3, 0xA7, 0xEA, 0x0B, 0x3D, 0x51, 0x9D, 0x09, 0xD8, 0xE4,
    0x8D, 0x07, 0x85, 0xFB, 0x74, 0x4A, 0x6B, 0x35, 0x5E, 0x63, 0x04, 0xBC,
    0x51, 0xC2, 0x29, 0xFB, 0xBC, 0xE2, 0x39, 0xBB, 0xAD, 0xF6, 0x40, 0x37,
    0x15, 0xC3, 0x5D, 0x4F, 0xB2, 0xA5, 0x44, 0x4F, 0x57, 0x5D, 0x4F, 0x42
};

static const unsigned char bp512_da[] = {
    0x16, 0x30, 0x2F, 0xF0, 0xDB, 0xBB, 0x5A, 0x8D, 0x73, 0x3D, 0xAB, 0x71,
    0x41, 0xC1, 0xB4, 0x5A, 0xCB, 0xC8, 0x71, 0x59, 0x39, 0x67, 0x7F, 0x6A,
    0x56, 0x85, 0x0A, 0x38, 0xBD, 0x87, 0xBD, 0x59, 0xB0, 0x9E, 0x80, 0x27,
    0x96, 0x09, 0xFF, 0x33, 0x3E, 0xB9, 0xD4, 0xC0, 0x61, 0x23, 0x1F, 0xB2,
    0x6F, 0x92, 0xEE, 0xB0, 0x49, 0x82, 0xA5, 0xF1, 0xD1, 0x76, 0x4C, 0xAD,
    0x57, 0x66, 0x54, 0x22
};

static const unsigned char bp512_db[] = {
    0x23, 0x0E, 0x18, 0xE1, 0xBC, 0xC8, 0x8A, 0x36, 0x2F, 0xA5, 0x4E, 0x4E,
    0xA3, 0x90, 0x20, 0x09, 0x29, 0x2F, 0x7F, 0x80, 0x33, 0x62, 0x4F, 0xD4,
    0x71, 0xB5, 0xD8, 0xAC, 0xE4, 0x9D, 0x12, 0xCF, 0xAB, 0xBC, 0x19, 0x96,
    0x3D, 0xAB, 0x8E, 0x2F, 0x1E, 0xBA, 0x00, 0xBF, 0xFB, 0x29, 0xE4, 0xD7,
    0x2D, 0x13, 0xF2, 0x22, 0x45, 0x62, 0xF4, 0x05, 0xCB, 0x80, 0x50, 0x36,
    0x66, 0xB2, 0x54, 0x29
};

static const unsigned char bp512_Z[] = {
    0xA7, 0x92, 0x70, 0x98, 0x65, 0x5F, 0x1F, 0x99, 0x76, 0xFA, 0x50, 0xA9,
    0xD5, 0x66, 0x86, 0x5D, 0xC5, 0x30, 0x33, 0x18, 0x46, 0x38, 0x1C, 0x87,
    0x25, 0x6B, 0xAF, 0x32, 0x26, 0x24, 0x4B, 0x76, 0xD3, 0x64, 0x03, 0xC0,
    0x24, 0xD7, 0xBB, 0xF0, 0xAA, 0x08, 0x03, 0xEA, 0xFF, 0x40, 0x5D, 0x3D,
    0x24, 0xF1, 0x1A, 0x9B, 0x5C, 0x0B, 0xEF, 0x67, 0x9F, 0xE1, 0x45, 0x4B,
    0x21, 0xC4, 0xCD, 0x1F
};

/* Given private value and NID, create EC_KEY structure */

static EC_KEY *mk_eckey(int nid, const unsigned char *p, size_t plen)
{
    int ok = 0;
    EC_KEY *k = NULL;
    BIGNUM *priv = NULL;
    EC_POINT *pub = NULL;
    const EC_GROUP *grp;
    k = EC_KEY_new_by_curve_name(nid);
    if (!k)
        goto err;
    priv = BN_bin2bn(p, plen, NULL);
    if (!priv)
        goto err;
    if (!EC_KEY_set_private_key(k, priv))
        goto err;
    grp = EC_KEY_get0_group(k);
    pub = EC_POINT_new(grp);
    if (!pub)
        goto err;
    if (!EC_POINT_mul(grp, pub, priv, NULL, NULL, NULL))
        goto err;
    if (!EC_KEY_set_public_key(k, pub))
        goto err;
    ok = 1;
 err:
    if (priv)
        BN_clear_free(priv);
    if (pub)
        EC_POINT_free(pub);
    if (ok)
        return k;
    else if (k)
        EC_KEY_free(k);
    return NULL;
}

/*
 * Known answer test: compute shared secret and check it matches expected
 * value.
 */

static int ecdh_kat(BIO *out, const char *cname, int nid,
                    const unsigned char *k1, size_t k1_len,
                    const unsigned char *k2, size_t k2_len,
                    const unsigned char *Z, size_t Zlen)
{
    int rv = 0;
    EC_KEY *key1 = NULL, *key2 = NULL;
    unsigned char *Ztmp = NULL;
    size_t Ztmplen;
    BIO_puts(out, "Testing ECDH shared secret with ");
    BIO_puts(out, cname);
    key1 = mk_eckey(nid, k1, k1_len);
    key2 = mk_eckey(nid, k2, k2_len);
    if (!key1 || !key2)
        goto err;
    Ztmplen = (EC_GROUP_get_degree(EC_KEY_get0_group(key1)) + 7) / 8;
    if (Ztmplen != Zlen)
        goto err;
    Ztmp = OPENSSL_malloc(Ztmplen);
    if (!ECDH_compute_key(Ztmp, Ztmplen,
                          EC_KEY_get0_public_key(key2), key1, 0))
        goto err;
    if (memcmp(Ztmp, Z, Zlen))
        goto err;
    memset(Ztmp, 0, Zlen);
    if (!ECDH_compute_key(Ztmp, Ztmplen,
                          EC_KEY_get0_public_key(key1), key2, 0))
        goto err;
    if (memcmp(Ztmp, Z, Zlen))
        goto err;
    rv = 1;
 err:
    if (key1)
        EC_KEY_free(key1);
    if (key2)
        EC_KEY_free(key2);
    if (Ztmp)
        OPENSSL_free(Ztmp);
    if (rv)
        BIO_puts(out, " ok\n");
    else {
        fprintf(stderr, "Error in ECDH routines\n");
        ERR_print_errors_fp(stderr);
    }
    return rv;
}

# define test_ecdh_kat(bio, curve, bits) \
        ecdh_kat(bio, curve, NID_brainpoolP##bits##r1, \
                bp##bits##_da, sizeof(bp##bits##_da), \
                bp##bits##_db, sizeof(bp##bits##_db), \
                bp##bits##_Z, sizeof(bp##bits##_Z))

int main(int argc, char *argv[])
{
    BN_CTX *ctx = NULL;
    int ret = 1;
    BIO *out;

    CRYPTO_malloc_debug_init();
    CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

# ifdef OPENSSL_SYS_WIN32
    CRYPTO_malloc_init();
# endif

    RAND_seed(rnd_seed, sizeof rnd_seed);

    out = BIO_new(BIO_s_file());
    if (out == NULL)
        EXIT(1);
    BIO_set_fp(out, stdout, BIO_NOCLOSE);

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;

    /* NIST PRIME CURVES TESTS */
    if (!test_ecdh_curve
        (NID_X9_62_prime192v1, "NIST Prime-Curve P-192", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_secp224r1, "NIST Prime-Curve P-224", ctx, out))
        goto err;
    if (!test_ecdh_curve
        (NID_X9_62_prime256v1, "NIST Prime-Curve P-256", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_secp384r1, "NIST Prime-Curve P-384", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_secp521r1, "NIST Prime-Curve P-521", ctx, out))
        goto err;
# ifndef OPENSSL_NO_EC2M
    /* NIST BINARY CURVES TESTS */
    if (!test_ecdh_curve(NID_sect163k1, "NIST Binary-Curve K-163", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_sect163r2, "NIST Binary-Curve B-163", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_sect233k1, "NIST Binary-Curve K-233", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_sect233r1, "NIST Binary-Curve B-233", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_sect283k1, "NIST Binary-Curve K-283", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_sect283r1, "NIST Binary-Curve B-283", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_sect409k1, "NIST Binary-Curve K-409", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_sect409r1, "NIST Binary-Curve B-409", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_sect571k1, "NIST Binary-Curve K-571", ctx, out))
        goto err;
    if (!test_ecdh_curve(NID_sect571r1, "NIST Binary-Curve B-571", ctx, out))
        goto err;
# endif
    if (!test_ecdh_kat(out, "Brainpool Prime-Curve brainpoolP256r1", 256))
        goto err;
    if (!test_ecdh_kat(out, "Brainpool Prime-Curve brainpoolP384r1", 384))
        goto err;
    if (!test_ecdh_kat(out, "Brainpool Prime-Curve brainpoolP512r1", 512))
        goto err;

    ret = 0;

 err:
    ERR_print_errors_fp(stderr);
    if (ctx)
        BN_CTX_free(ctx);
    BIO_free(out);
    CRYPTO_cleanup_all_ex_data();
    ERR_remove_thread_state(NULL);
    CRYPTO_mem_leaks_fp(stderr);
    EXIT(ret);
    return (ret);
}

# if 0
static void MS_CALLBACK cb(int p, int n, void *arg)
{
    char c = '*';

    if (p == 0)
        c = '.';
    if (p == 1)
        c = '+';
    if (p == 2)
        c = '*';
    if (p == 3)
        c = '\n';
    BIO_write((BIO *)arg, &c, 1);
    (void)BIO_flush((BIO *)arg);
#  ifdef LINT
    p = n;
#  endif
}
# endif
#endif
