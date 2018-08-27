/*
 * Copyright 2011-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/opensslconf.h>
#ifdef OPENSSL_NO_SRP

# include <stdio.h>

int main(int argc, char *argv[])
{
    printf("No SRP support\n");
    return (0);
}

#else

# include <openssl/srp.h>
# include <openssl/rand.h>
# include <openssl/err.h>

static void showbn(const char *name, const BIGNUM *bn)
{
    fputs(name, stdout);
    fputs(" = ", stdout);
    BN_print_fp(stdout, bn);
    putc('\n', stdout);
}

# define RANDOM_SIZE 32         /* use 256 bits on each side */

static int run_srp(const char *username, const char *client_pass,
                   const char *server_pass)
{
    int ret = -1;
    BIGNUM *s = NULL;
    BIGNUM *v = NULL;
    BIGNUM *a = NULL;
    BIGNUM *b = NULL;
    BIGNUM *u = NULL;
    BIGNUM *x = NULL;
    BIGNUM *Apub = NULL;
    BIGNUM *Bpub = NULL;
    BIGNUM *Kclient = NULL;
    BIGNUM *Kserver = NULL;
    unsigned char rand_tmp[RANDOM_SIZE];
    /* use builtin 1024-bit params */
    const SRP_gN *GN = SRP_get_default_gN("1024");

    if (GN == NULL) {
        fprintf(stderr, "Failed to get SRP parameters\n");
        return -1;
    }
    /* Set up server's password entry */
    if (!SRP_create_verifier_BN(username, server_pass, &s, &v, GN->N, GN->g)) {
        fprintf(stderr, "Failed to create SRP verifier\n");
        return -1;
    }

    showbn("N", GN->N);
    showbn("g", GN->g);
    showbn("Salt", s);
    showbn("Verifier", v);

    /* Server random */
    RAND_bytes(rand_tmp, sizeof(rand_tmp));
    b = BN_bin2bn(rand_tmp, sizeof(rand_tmp), NULL);
    /* TODO - check b != 0 */
    showbn("b", b);

    /* Server's first message */
    Bpub = SRP_Calc_B(b, GN->N, GN->g, v);
    showbn("B", Bpub);

    if (!SRP_Verify_B_mod_N(Bpub, GN->N)) {
        fprintf(stderr, "Invalid B\n");
        return -1;
    }

    /* Client random */
    RAND_bytes(rand_tmp, sizeof(rand_tmp));
    a = BN_bin2bn(rand_tmp, sizeof(rand_tmp), NULL);
    /* TODO - check a != 0 */
    showbn("a", a);

    /* Client's response */
    Apub = SRP_Calc_A(a, GN->N, GN->g);
    showbn("A", Apub);

    if (!SRP_Verify_A_mod_N(Apub, GN->N)) {
        fprintf(stderr, "Invalid A\n");
        return -1;
    }

    /* Both sides calculate u */
    u = SRP_Calc_u(Apub, Bpub, GN->N);

    /* Client's key */
    x = SRP_Calc_x(s, username, client_pass);
    Kclient = SRP_Calc_client_key(GN->N, Bpub, GN->g, x, a, u);
    showbn("Client's key", Kclient);

    /* Server's key */
    Kserver = SRP_Calc_server_key(Apub, v, u, b, GN->N);
    showbn("Server's key", Kserver);

    if (BN_cmp(Kclient, Kserver) == 0) {
        ret = 0;
    } else {
        fprintf(stderr, "Keys mismatch\n");
        ret = 1;
    }

    BN_clear_free(Kclient);
    BN_clear_free(Kserver);
    BN_clear_free(x);
    BN_free(u);
    BN_free(Apub);
    BN_clear_free(a);
    BN_free(Bpub);
    BN_clear_free(b);
    BN_free(s);
    BN_clear_free(v);

    return ret;
}

static int check_bn(const char *name, const BIGNUM *bn, const char *hexbn)
{
    BIGNUM *tmp = NULL;
    int rv;
    if (BN_hex2bn(&tmp, hexbn) == 0)
        return 0;
    rv = BN_cmp(bn, tmp);
    if (rv == 0) {
        printf("%s = ", name);
        BN_print_fp(stdout, bn);
        printf("\n");
        BN_free(tmp);
        return 1;
    }
    printf("Unexpected %s value\n", name);
    printf("Expecting: ");
    BN_print_fp(stdout, tmp);
    printf("\nReceived: ");
    BN_print_fp(stdout, bn);
    printf("\n");
    BN_free(tmp);
    return 0;
}

/* SRP test vectors from RFC5054 */
static int run_srp_kat(void)
{
    int ret = 0;
    BIGNUM *s = NULL;
    BIGNUM *v = NULL;
    BIGNUM *a = NULL;
    BIGNUM *b = NULL;
    BIGNUM *u = NULL;
    BIGNUM *x = NULL;
    BIGNUM *Apub = NULL;
    BIGNUM *Bpub = NULL;
    BIGNUM *Kclient = NULL;
    BIGNUM *Kserver = NULL;
    /* use builtin 1024-bit params */
    const SRP_gN *GN = SRP_get_default_gN("1024");

    if (GN == NULL) {
        fprintf(stderr, "Failed to get SRP parameters\n");
        goto err;
    }
    BN_hex2bn(&s, "BEB25379D1A8581EB5A727673A2441EE");
    /* Set up server's password entry */
    if (!SRP_create_verifier_BN("alice", "password123", &s, &v, GN->N,
                                GN->g)) {
        fprintf(stderr, "Failed to create SRP verifier\n");
        goto err;
    }

    if (!check_bn("v", v,
                 "7E273DE8696FFC4F4E337D05B4B375BEB0DDE1569E8FA00A9886D812"
                 "9BADA1F1822223CA1A605B530E379BA4729FDC59F105B4787E5186F5"
                 "C671085A1447B52A48CF1970B4FB6F8400BBF4CEBFBB168152E08AB5"
                 "EA53D15C1AFF87B2B9DA6E04E058AD51CC72BFC9033B564E26480D78"
                 "E955A5E29E7AB245DB2BE315E2099AFB"))
        goto err;

    /* Server random */
    BN_hex2bn(&b, "E487CB59D31AC550471E81F00F6928E01DDA08E974A004F49E61F5D1"
                  "05284D20");

    /* Server's first message */
    Bpub = SRP_Calc_B(b, GN->N, GN->g, v);

    if (!SRP_Verify_B_mod_N(Bpub, GN->N)) {
        fprintf(stderr, "Invalid B\n");
        goto err;
    }

    if (!check_bn("B", Bpub,
                  "BD0C61512C692C0CB6D041FA01BB152D4916A1E77AF46AE105393011"
                  "BAF38964DC46A0670DD125B95A981652236F99D9B681CBF87837EC99"
                  "6C6DA04453728610D0C6DDB58B318885D7D82C7F8DEB75CE7BD4FBAA"
                  "37089E6F9C6059F388838E7A00030B331EB76840910440B1B27AAEAE"
                  "EB4012B7D7665238A8E3FB004B117B58"))
        goto err;

    /* Client random */
    BN_hex2bn(&a, "60975527035CF2AD1989806F0407210BC81EDC04E2762A56AFD529DD"
                  "DA2D4393");

    /* Client's response */
    Apub = SRP_Calc_A(a, GN->N, GN->g);

    if (!SRP_Verify_A_mod_N(Apub, GN->N)) {
        fprintf(stderr, "Invalid A\n");
        return -1;
    }

    if (!check_bn("A", Apub,
                  "61D5E490F6F1B79547B0704C436F523DD0E560F0C64115BB72557EC4"
                  "4352E8903211C04692272D8B2D1A5358A2CF1B6E0BFCF99F921530EC"
                  "8E39356179EAE45E42BA92AEACED825171E1E8B9AF6D9C03E1327F44"
                  "BE087EF06530E69F66615261EEF54073CA11CF5858F0EDFDFE15EFEA"
                  "B349EF5D76988A3672FAC47B0769447B"))
        goto err;

    /* Both sides calculate u */
    u = SRP_Calc_u(Apub, Bpub, GN->N);

    if (!check_bn("u", u, "CE38B9593487DA98554ED47D70A7AE5F462EF019"))
        goto err;

    /* Client's key */
    x = SRP_Calc_x(s, "alice", "password123");
    Kclient = SRP_Calc_client_key(GN->N, Bpub, GN->g, x, a, u);
    if (!check_bn("Client's key", Kclient,
                  "B0DC82BABCF30674AE450C0287745E7990A3381F63B387AAF271A10D"
                  "233861E359B48220F7C4693C9AE12B0A6F67809F0876E2D013800D6C"
                  "41BB59B6D5979B5C00A172B4A2A5903A0BDCAF8A709585EB2AFAFA8F"
                  "3499B200210DCC1F10EB33943CD67FC88A2F39A4BE5BEC4EC0A3212D"
                  "C346D7E474B29EDE8A469FFECA686E5A"))
        goto err;
    /* Server's key */
    Kserver = SRP_Calc_server_key(Apub, v, u, b, GN->N);
    if (!check_bn("Server's key", Kserver,
                  "B0DC82BABCF30674AE450C0287745E7990A3381F63B387AAF271A10D"
                  "233861E359B48220F7C4693C9AE12B0A6F67809F0876E2D013800D6C"
                  "41BB59B6D5979B5C00A172B4A2A5903A0BDCAF8A709585EB2AFAFA8F"
                  "3499B200210DCC1F10EB33943CD67FC88A2F39A4BE5BEC4EC0A3212D"
                  "C346D7E474B29EDE8A469FFECA686E5A"))
        goto err;

    ret = 1;

    err:
    BN_clear_free(Kclient);
    BN_clear_free(Kserver);
    BN_clear_free(x);
    BN_free(u);
    BN_free(Apub);
    BN_clear_free(a);
    BN_free(Bpub);
    BN_clear_free(b);
    BN_free(s);
    BN_clear_free(v);

    return ret;
}

int main(int argc, char **argv)
{
    BIO *bio_err;
    bio_err = BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);


    /* "Negative" test, expect a mismatch */
    if (run_srp("alice", "password1", "password2") == 0) {
        fprintf(stderr, "Mismatched SRP run failed\n");
        return 1;
    }

    /* "Positive" test, should pass */
    if (run_srp("alice", "password", "password") != 0) {
        fprintf(stderr, "Plain SRP run failed\n");
        return 1;
    }

    /* KAT from RFC5054: should pass */
    if (run_srp_kat() != 1) {
        fprintf(stderr, "SRP KAT failed\n");
        return 1;
    }

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks(bio_err) <= 0)
        return 1;
#endif
    BIO_free(bio_err);

    return 0;
}
#endif
