/*
 * Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../e_os.h"

#include <openssl/opensslconf.h> /* for OPENSSL_NO_EC */
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/objects.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#ifdef OPENSSL_NO_EC
int main(int argc, char *argv[])
{
    printf("No ECDH support\n");
    return (0);
}
#else
# include <openssl/ec.h>

static const char rnd_seed[] =
    "string to make the random number generator think it has entropy";

static const int KDF1_SHA1_len = 20;
static void *KDF1_SHA1(const void *in, size_t inlen, void *out,
                       size_t *outlen)
{
    if (*outlen < SHA_DIGEST_LENGTH)
        return NULL;
    *outlen = SHA_DIGEST_LENGTH;
    return SHA1(in, inlen, out);
}

static int test_ecdh_curve(int nid, BN_CTX *ctx, BIO *out)
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
    BIO_puts(out, OBJ_nid2sn(nid));
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
    abuf = OPENSSL_malloc(alen);
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
    bbuf = OPENSSL_malloc(blen);
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

    OPENSSL_free(abuf);
    OPENSSL_free(bbuf);
    BN_free(x_a);
    BN_free(y_a);
    BN_free(x_b);
    BN_free(y_b);
    EC_KEY_free(b);
    EC_KEY_free(a);
    return (ret);
}

typedef struct {
    const int nid;
    const char *da;
    const char *db;
    const char *Z;
} ecdh_kat_t;

static const ecdh_kat_t ecdh_kats[] = {
    /* Keys and shared secrets from RFC 5114 */
    { NID_X9_62_prime192v1,
    "323FA3169D8E9C6593F59476BC142000AB5BE0E249C43426",
    "631F95BB4A67632C9C476EEE9AB695AB240A0499307FCF62",
    "AD420182633F8526BFE954ACDA376F05E5FF4F837F54FEBE" },
    { NID_secp224r1,
    "B558EB6C288DA707BBB4F8FBAE2AB9E9CB62E3BC5C7573E22E26D37F",
    "AC3B1ADD3D9770E6F6A708EE9F3B8E0AB3B480E9F27F85C88B5E6D18",
    "52272F50F46F4EDC9151569092F46DF2D96ECC3B6DC1714A4EA949FA" },
    { NID_X9_62_prime256v1,
    "814264145F2F56F2E96A8E337A1284993FAF432A5ABCE59E867B7291D507A3AF",
    "2CE1788EC197E096DB95A200CC0AB26A19CE6BCCAD562B8EEE1B593761CF7F41",
    "DD0F5396219D1EA393310412D19A08F1F5811E9DC8EC8EEA7F80D21C820C2788" },
    { NID_secp384r1,
    "D27335EA71664AF244DD14E9FD1260715DFD8A7965571C48D709EE7A7962A156"
    "D706A90CBCB5DF2986F05FEADB9376F1",
    "52D1791FDB4B70F89C0F00D456C2F7023B6125262C36A7DF1F80231121CCE3D3"
    "9BE52E00C194A4132C4A6C768BCD94D2",
    "5EA1FC4AF7256D2055981B110575E0A8CAE53160137D904C59D926EB1B8456E4"
    "27AA8A4540884C37DE159A58028ABC0E" },
    { NID_secp521r1,
    "0113F82DA825735E3D97276683B2B74277BAD27335EA71664AF2430CC4F33459"
    "B9669EE78B3FFB9B8683015D344DCBFEF6FB9AF4C6C470BE254516CD3C1A1FB4"
    "7362",
    "00CEE3480D8645A17D249F2776D28BAE616952D1791FDB4B70F7C3378732AA1B"
    "22928448BCD1DC2496D435B01048066EBE4F72903C361B1A9DC1193DC2C9D089"
    "1B96",
    "00CDEA89621CFA46B132F9E4CFE2261CDE2D4368EB5656634C7CC98C7A00CDE5"
    "4ED1866A0DD3E6126C9D2F845DAFF82CEB1DA08F5D87521BB0EBECA77911169C"
    "20CC" },
    /* Keys and shared secrets from RFC 5903 */
    { NID_X9_62_prime256v1,
    "C88F01F510D9AC3F70A292DAA2316DE544E9AAB8AFE84049C62A9C57862D1433",
    "C6EF9C5D78AE012A011164ACB397CE2088685D8F06BF9BE0B283AB46476BEE53",
    "D6840F6B42F6EDAFD13116E0E12565202FEF8E9ECE7DCE03812464D04B9442DE" },
    { NID_secp384r1,
    "099F3C7034D4A2C699884D73A375A67F7624EF7C6B3C0F160647B67414DCE655"
    "E35B538041E649EE3FAEF896783AB194",
    "41CB0779B4BDB85D47846725FBEC3C9430FAB46CC8DC5060855CC9BDA0AA2942"
    "E0308312916B8ED2960E4BD55A7448FC",
    "11187331C279962D93D604243FD592CB9D0A926F422E47187521287E7156C5C4"
    "D603135569B9E9D09CF5D4A270F59746" },
    { NID_secp521r1,
    "0037ADE9319A89F4DABDB3EF411AACCCA5123C61ACAB57B5393DCE47608172A0"
    "95AA85A30FE1C2952C6771D937BA9777F5957B2639BAB072462F68C27A57382D"
    "4A52",
    "0145BA99A847AF43793FDD0E872E7CDFA16BE30FDC780F97BCCC3F078380201E"
    "9C677D600B343757A3BDBF2A3163E4C2F869CCA7458AA4A4EFFC311F5CB15168"
    "5EB9",
    "01144C7D79AE6956BC8EDB8E7C787C4521CB086FA64407F97894E5E6B2D79B04"
    "D1427E73CA4BAA240A34786859810C06B3C715A3A8CC3151F2BEE417996D19F3"
    "DDEA" },
    /* Keys and shared secrets from RFC 7027 */
    { NID_brainpoolP256r1,
    "81DB1EE100150FF2EA338D708271BE38300CB54241D79950F77B063039804F1D",
    "55E40BC41E37E3E2AD25C3C6654511FFA8474A91A0032087593852D3E7D76BD3",
    "89AFC39D41D3B327814B80940B042590F96556EC91E6AE7939BCE31F3A18BF2B" },
    { NID_brainpoolP384r1,
    "1E20F5E048A5886F1F157C74E91BDE2B98C8B52D58E5003D57053FC4B0BD65D6"
    "F15EB5D1EE1610DF870795143627D042",
    "032640BC6003C59260F7250C3DB58CE647F98E1260ACCE4ACDA3DD869F74E01F"
    "8BA5E0324309DB6A9831497ABAC96670",
    "0BD9D3A7EA0B3D519D09D8E48D0785FB744A6B355E6304BC51C229FBBCE239BB"
    "ADF6403715C35D4FB2A5444F575D4F42" },
    { NID_brainpoolP512r1,
    "16302FF0DBBB5A8D733DAB7141C1B45ACBC8715939677F6A56850A38BD87BD59"
    "B09E80279609FF333EB9D4C061231FB26F92EEB04982A5F1D1764CAD57665422",
    "230E18E1BCC88A362FA54E4EA3902009292F7F8033624FD471B5D8ACE49D12CF"
    "ABBC19963DAB8E2F1EBA00BFFB29E4D72D13F2224562F405CB80503666B25429",
    "A7927098655F1F9976FA50A9D566865DC530331846381C87256BAF3226244B76"
    "D36403C024D7BBF0AA0803EAFF405D3D24F11A9B5C0BEF679FE1454B21C4CD1F" }
};

/* Given private value and NID, create EC_KEY structure */

static EC_KEY *mk_eckey(int nid, const char *str)
{
    int ok = 0;
    EC_KEY *k = NULL;
    BIGNUM *priv = NULL;
    EC_POINT *pub = NULL;
    const EC_GROUP *grp;
    k = EC_KEY_new_by_curve_name(nid);
    if (!k)
        goto err;
    if(!BN_hex2bn(&priv, str))
        goto err;
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
    BN_clear_free(priv);
    EC_POINT_free(pub);
    if (ok)
        return k;
    EC_KEY_free(k);
    return NULL;
}

/*
 * Known answer test: compute shared secret and check it matches expected
 * value.
 */

static int ecdh_kat(BIO *out, const ecdh_kat_t *kat)
{
    int rv = 0;
    EC_KEY *key1 = NULL, *key2 = NULL;
    BIGNUM *bnz = NULL;
    unsigned char *Ztmp = NULL, *Z = NULL;
    size_t Ztmplen, Zlen;
    BIO_puts(out, "Testing ECDH shared secret with ");
    BIO_puts(out, OBJ_nid2sn(kat->nid));
    if(!BN_hex2bn(&bnz, kat->Z))
        goto err;
    key1 = mk_eckey(kat->nid, kat->da);
    key2 = mk_eckey(kat->nid, kat->db);
    if (!key1 || !key2)
        goto err;
    Ztmplen = (EC_GROUP_get_degree(EC_KEY_get0_group(key1)) + 7) / 8;
    Zlen = BN_num_bytes(bnz);
    if (Zlen > Ztmplen)
        goto err;
    if((Ztmp = OPENSSL_zalloc(Ztmplen)) == NULL)
        goto err;
    if((Z = OPENSSL_zalloc(Ztmplen)) == NULL)
        goto err;
    if(!BN_bn2binpad(bnz, Z, Ztmplen))
        goto err;
    if (!ECDH_compute_key(Ztmp, Ztmplen,
                          EC_KEY_get0_public_key(key2), key1, 0))
        goto err;
    if (memcmp(Ztmp, Z, Ztmplen))
        goto err;
    memset(Ztmp, 0, Ztmplen);
    if (!ECDH_compute_key(Ztmp, Ztmplen,
                          EC_KEY_get0_public_key(key1), key2, 0))
        goto err;
    if (memcmp(Ztmp, Z, Ztmplen))
        goto err;
    rv = 1;
 err:
    EC_KEY_free(key1);
    EC_KEY_free(key2);
    OPENSSL_free(Ztmp);
    OPENSSL_free(Z);
    BN_free(bnz);
    if (rv)
        BIO_puts(out, " ok\n");
    else {
        fprintf(stderr, "Error in ECDH routines\n");
        ERR_print_errors_fp(stderr);
    }
    return rv;
}

#include "ecdhtest_cavs.h"

/*
 * NIST SP800-56A co-factor ECDH tests.
 * KATs taken from NIST documents with parameters:
 *
 * - (QCAVSx,QCAVSy) is the public key for CAVS.
 * - dIUT is the private key for IUT.
 * - (QIUTx,QIUTy) is the public key for IUT.
 * - ZIUT is the shared secret KAT.
 *
 * CAVS: Cryptographic Algorithm Validation System
 * IUT: Implementation Under Test
 *
 * This function tests two things:
 *
 * 1. dIUT * G = (QIUTx,QIUTy)
 *    i.e. public key for IUT computes correctly.
 * 2. x-coord of cofactor * dIUT * (QCAVSx,QCAVSy) = ZIUT
 *    i.e. co-factor ECDH key computes correctly.
 *
 * returns zero on failure or unsupported curve. One otherwise.
 */
static int ecdh_cavs_kat(BIO *out, const ecdh_cavs_kat_t *kat)
{
    int rv = 0, is_char_two = 0;
    EC_KEY *key1 = NULL;
    EC_POINT *pub = NULL;
    const EC_GROUP *group = NULL;
    BIGNUM *bnz = NULL, *x = NULL, *y = NULL;
    unsigned char *Ztmp = NULL, *Z = NULL;
    size_t Ztmplen, Zlen;
    BIO_puts(out, "Testing ECC CDH Primitive SP800-56A with ");
    BIO_puts(out, OBJ_nid2sn(kat->nid));

    /* dIUT is IUT's private key */
    if ((key1 = mk_eckey(kat->nid, kat->dIUT)) == NULL)
        goto err;
    /* these are cofactor ECDH KATs */
    EC_KEY_set_flags(key1, EC_FLAG_COFACTOR_ECDH);

    if ((group = EC_KEY_get0_group(key1)) == NULL)
        goto err;
    if ((pub = EC_POINT_new(group)) == NULL)
        goto err;

    if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_characteristic_two_field)
        is_char_two = 1;

    /* (QIUTx, QIUTy) is IUT's public key */
    if(!BN_hex2bn(&x, kat->QIUTx))
        goto err;
    if(!BN_hex2bn(&y, kat->QIUTy))
        goto err;
    if (is_char_two) {
#ifdef OPENSSL_NO_EC2M
        goto err;
#else
        if (!EC_POINT_set_affine_coordinates_GF2m(group, pub, x, y, NULL))
            goto err;
#endif
    }
    else {
        if (!EC_POINT_set_affine_coordinates_GFp(group, pub, x, y, NULL))
            goto err;
    }
    /* dIUT * G = (QIUTx, QIUTy) should hold */
    if (EC_POINT_cmp(group, EC_KEY_get0_public_key(key1), pub, NULL))
        goto err;

    /* (QCAVSx, QCAVSy) is CAVS's public key */
    if(!BN_hex2bn(&x, kat->QCAVSx))
        goto err;
    if(!BN_hex2bn(&y, kat->QCAVSy))
        goto err;
    if (is_char_two) {
#ifdef OPENSSL_NO_EC2M
        goto err;
#else
        if (!EC_POINT_set_affine_coordinates_GF2m(group, pub, x, y, NULL))
            goto err;
#endif
    }
    else {
        if (!EC_POINT_set_affine_coordinates_GFp(group, pub, x, y, NULL))
            goto err;
    }

    /* ZIUT is the shared secret */
    if(!BN_hex2bn(&bnz, kat->ZIUT))
        goto err;
    Ztmplen = (EC_GROUP_get_degree(EC_KEY_get0_group(key1)) + 7) / 8;
    Zlen = BN_num_bytes(bnz);
    if (Zlen > Ztmplen)
        goto err;
    if((Ztmp = OPENSSL_zalloc(Ztmplen)) == NULL)
        goto err;
    if((Z = OPENSSL_zalloc(Ztmplen)) == NULL)
        goto err;
    if(!BN_bn2binpad(bnz, Z, Ztmplen))
        goto err;
    if (!ECDH_compute_key(Ztmp, Ztmplen, pub, key1, 0))
        goto err;
    /* shared secrets should be identical */
    if (memcmp(Ztmp, Z, Ztmplen))
        goto err;
    rv = 1;
 err:
    EC_KEY_free(key1);
    EC_POINT_free(pub);
    BN_free(bnz);
    BN_free(x);
    BN_free(y);
    OPENSSL_free(Ztmp);
    OPENSSL_free(Z);
    if (rv) {
        BIO_puts(out, " ok\n");
    }
    else {
        fprintf(stderr, "Error in ECC CDH routines\n");
        ERR_print_errors_fp(stderr);
    }
    return rv;
}

int main(int argc, char *argv[])
{
    BN_CTX *ctx = NULL;
    int nid, ret = 1;
    EC_builtin_curve *curves = NULL;
    size_t crv_len = 0, n = 0;
    BIO *out;

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    RAND_seed(rnd_seed, sizeof rnd_seed);

    out = BIO_new(BIO_s_file());
    if (out == NULL)
        EXIT(1);
    BIO_set_fp(out, stdout, BIO_NOCLOSE | BIO_FP_TEXT);

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;

    /* get a list of all internal curves */
    crv_len = EC_get_builtin_curves(NULL, 0);
    curves = OPENSSL_malloc(sizeof(*curves) * crv_len);
    if (curves == NULL) goto err;

    if (!EC_get_builtin_curves(curves, crv_len)) goto err;

    /* NAMED CURVES TESTS */
    for (n = 0; n < crv_len; n++) {
        nid = curves[n].nid;
        /*
         * Skipped for X25519 because affine coordinate operations are not
         * supported for this curve.
         * Higher level ECDH tests are performed in evptests.txt instead.
         */
        if (nid == NID_X25519)
            continue;
        if (!test_ecdh_curve(nid, ctx, out)) goto err;
    }

    /* KATs */
    for (n = 0; n < (sizeof(ecdh_kats)/sizeof(ecdh_kat_t)); n++) {
        if (!ecdh_kat(out, &ecdh_kats[n]))
            goto err;
    }

    /* NIST SP800-56A co-factor ECDH KATs */
    for (n = 0; n < (sizeof(ecdh_cavs_kats)/sizeof(ecdh_cavs_kat_t)); n++) {
        if (!ecdh_cavs_kat(out, &ecdh_cavs_kats[n]))
            goto err;
    }

    ret = 0;

 err:
    ERR_print_errors_fp(stderr);
    OPENSSL_free(curves);
    BN_CTX_free(ctx);
    BIO_free(out);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks_fp(stderr) <= 0)
        ret = 1;
#endif
    EXIT(ret);
}
#endif
