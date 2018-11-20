/* rsautl.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2000.
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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

#include <openssl/opensslconf.h>
#ifndef OPENSSL_NO_RSA

# include "apps.h"
# include <string.h>
# include <openssl/err.h>
# include <openssl/pem.h>
# include <openssl/rsa.h>

# define RSA_SIGN        1
# define RSA_VERIFY      2
# define RSA_ENCRYPT     3
# define RSA_DECRYPT     4

# define KEY_PRIVKEY     1
# define KEY_PUBKEY      2
# define KEY_CERT        3

static void usage(void);

# undef PROG

# define PROG rsautl_main

int MAIN(int argc, char **);

int MAIN(int argc, char **argv)
{
    ENGINE *e = NULL;
    BIO *in = NULL, *out = NULL;
    char *infile = NULL, *outfile = NULL;
# ifndef OPENSSL_NO_ENGINE
    char *engine = NULL;
# endif
    char *keyfile = NULL;
    char rsa_mode = RSA_VERIFY, key_type = KEY_PRIVKEY;
    int keyform = FORMAT_PEM;
    char need_priv = 0, badarg = 0, rev = 0;
    char hexdump = 0, asn1parse = 0;
    X509 *x;
    EVP_PKEY *pkey = NULL;
    RSA *rsa = NULL;
    unsigned char *rsa_in = NULL, *rsa_out = NULL, pad;
    char *passargin = NULL, *passin = NULL;
    int rsa_inlen, rsa_outlen = 0;
    int keysize;

    int ret = 1;

    argc--;
    argv++;

    if (!bio_err)
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

    if (!load_config(bio_err, NULL))
        goto end;
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    pad = RSA_PKCS1_PADDING;

    while (argc >= 1) {
        if (!strcmp(*argv, "-in")) {
            if (--argc < 1)
                badarg = 1;
            else
                infile = *(++argv);
        } else if (!strcmp(*argv, "-out")) {
            if (--argc < 1)
                badarg = 1;
            else
                outfile = *(++argv);
        } else if (!strcmp(*argv, "-inkey")) {
            if (--argc < 1)
                badarg = 1;
            else
                keyfile = *(++argv);
        } else if (!strcmp(*argv, "-passin")) {
            if (--argc < 1)
                badarg = 1;
            else
                passargin = *(++argv);
        } else if (strcmp(*argv, "-keyform") == 0) {
            if (--argc < 1)
                badarg = 1;
            else
                keyform = str2fmt(*(++argv));
# ifndef OPENSSL_NO_ENGINE
        } else if (!strcmp(*argv, "-engine")) {
            if (--argc < 1)
                badarg = 1;
            else
                engine = *(++argv);
# endif
        } else if (!strcmp(*argv, "-pubin")) {
            key_type = KEY_PUBKEY;
        } else if (!strcmp(*argv, "-certin")) {
            key_type = KEY_CERT;
        } else if (!strcmp(*argv, "-asn1parse"))
            asn1parse = 1;
        else if (!strcmp(*argv, "-hexdump"))
            hexdump = 1;
        else if (!strcmp(*argv, "-raw"))
            pad = RSA_NO_PADDING;
        else if (!strcmp(*argv, "-oaep"))
            pad = RSA_PKCS1_OAEP_PADDING;
        else if (!strcmp(*argv, "-ssl"))
            pad = RSA_SSLV23_PADDING;
        else if (!strcmp(*argv, "-pkcs"))
            pad = RSA_PKCS1_PADDING;
        else if (!strcmp(*argv, "-x931"))
            pad = RSA_X931_PADDING;
        else if (!strcmp(*argv, "-sign")) {
            rsa_mode = RSA_SIGN;
            need_priv = 1;
        } else if (!strcmp(*argv, "-verify"))
            rsa_mode = RSA_VERIFY;
        else if (!strcmp(*argv, "-rev"))
            rev = 1;
        else if (!strcmp(*argv, "-encrypt"))
            rsa_mode = RSA_ENCRYPT;
        else if (!strcmp(*argv, "-decrypt")) {
            rsa_mode = RSA_DECRYPT;
            need_priv = 1;
        } else
            badarg = 1;
        if (badarg) {
            usage();
            goto end;
        }
        argc--;
        argv++;
    }

    if (need_priv && (key_type != KEY_PRIVKEY)) {
        BIO_printf(bio_err, "A private key is needed for this operation\n");
        goto end;
    }
# ifndef OPENSSL_NO_ENGINE
    e = setup_engine(bio_err, engine, 0);
# endif
    if (!app_passwd(bio_err, passargin, NULL, &passin, NULL)) {
        BIO_printf(bio_err, "Error getting password\n");
        goto end;
    }

/* FIXME: seed PRNG only if needed */
    app_RAND_load_file(NULL, bio_err, 0);

    switch (key_type) {
    case KEY_PRIVKEY:
        pkey = load_key(bio_err, keyfile, keyform, 0,
                        passin, e, "Private Key");
        break;

    case KEY_PUBKEY:
        pkey = load_pubkey(bio_err, keyfile, keyform, 0,
                           NULL, e, "Public Key");
        break;

    case KEY_CERT:
        x = load_cert(bio_err, keyfile, keyform, NULL, e, "Certificate");
        if (x) {
            pkey = X509_get_pubkey(x);
            X509_free(x);
        }
        break;
    }

    if (!pkey) {
        return 1;
    }

    rsa = EVP_PKEY_get1_RSA(pkey);
    EVP_PKEY_free(pkey);

    if (!rsa) {
        BIO_printf(bio_err, "Error getting RSA key\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    if (infile) {
        if (!(in = BIO_new_file(infile, "rb"))) {
            BIO_printf(bio_err, "Error Reading Input File\n");
            ERR_print_errors(bio_err);
            goto end;
        }
    } else
        in = BIO_new_fp(stdin, BIO_NOCLOSE);

    if (outfile) {
        if (!(out = BIO_new_file(outfile, "wb"))) {
            BIO_printf(bio_err, "Error Reading Output File\n");
            ERR_print_errors(bio_err);
            goto end;
        }
    } else {
        out = BIO_new_fp(stdout, BIO_NOCLOSE);
# ifdef OPENSSL_SYS_VMS
        {
            BIO *tmpbio = BIO_new(BIO_f_linebuffer());
            out = BIO_push(tmpbio, out);
        }
# endif
    }

    keysize = RSA_size(rsa);

    rsa_in = OPENSSL_malloc(keysize * 2);
    rsa_out = OPENSSL_malloc(keysize);
    if (!rsa_in || !rsa_out) {
        BIO_printf(bio_err, "Out of memory\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    /* Read the input data */
    rsa_inlen = BIO_read(in, rsa_in, keysize * 2);
    if (rsa_inlen <= 0) {
        BIO_printf(bio_err, "Error reading input Data\n");
        exit(1);
    }
    if (rev) {
        int i;
        unsigned char ctmp;
        for (i = 0; i < rsa_inlen / 2; i++) {
            ctmp = rsa_in[i];
            rsa_in[i] = rsa_in[rsa_inlen - 1 - i];
            rsa_in[rsa_inlen - 1 - i] = ctmp;
        }
    }
    switch (rsa_mode) {

    case RSA_VERIFY:
        rsa_outlen = RSA_public_decrypt(rsa_inlen, rsa_in, rsa_out, rsa, pad);
        break;

    case RSA_SIGN:
        rsa_outlen =
            RSA_private_encrypt(rsa_inlen, rsa_in, rsa_out, rsa, pad);
        break;

    case RSA_ENCRYPT:
        rsa_outlen = RSA_public_encrypt(rsa_inlen, rsa_in, rsa_out, rsa, pad);
        break;

    case RSA_DECRYPT:
        rsa_outlen =
            RSA_private_decrypt(rsa_inlen, rsa_in, rsa_out, rsa, pad);
        break;

    }

    if (rsa_outlen <= 0) {
        BIO_printf(bio_err, "RSA operation error\n");
        ERR_print_errors(bio_err);
        goto end;
    }
    ret = 0;
    if (asn1parse) {
        if (!ASN1_parse_dump(out, rsa_out, rsa_outlen, 1, -1)) {
            ERR_print_errors(bio_err);
        }
    } else if (hexdump)
        BIO_dump(out, (char *)rsa_out, rsa_outlen);
    else
        BIO_write(out, rsa_out, rsa_outlen);
 end:
    RSA_free(rsa);
    BIO_free(in);
    BIO_free_all(out);
    if (rsa_in)
        OPENSSL_free(rsa_in);
    if (rsa_out)
        OPENSSL_free(rsa_out);
    if (passin)
        OPENSSL_free(passin);
    return ret;
}

static void usage()
{
    BIO_printf(bio_err, "Usage: rsautl [options]\n");
    BIO_printf(bio_err, "-in file        input file\n");
    BIO_printf(bio_err, "-out file       output file\n");
    BIO_printf(bio_err, "-inkey file     input key\n");
    BIO_printf(bio_err, "-keyform arg    private key format - default PEM\n");
    BIO_printf(bio_err, "-pubin          input is an RSA public\n");
    BIO_printf(bio_err,
               "-certin         input is a certificate carrying an RSA public key\n");
    BIO_printf(bio_err, "-ssl            use SSL v2 padding\n");
    BIO_printf(bio_err, "-raw            use no padding\n");
    BIO_printf(bio_err,
               "-pkcs           use PKCS#1 v1.5 padding (default)\n");
    BIO_printf(bio_err, "-oaep           use PKCS#1 OAEP\n");
    BIO_printf(bio_err, "-sign           sign with private key\n");
    BIO_printf(bio_err, "-verify         verify with public key\n");
    BIO_printf(bio_err, "-encrypt        encrypt with public key\n");
    BIO_printf(bio_err, "-decrypt        decrypt with private key\n");
    BIO_printf(bio_err, "-hexdump        hex dump output\n");
# ifndef OPENSSL_NO_ENGINE
    BIO_printf(bio_err,
               "-engine e       use engine e, possibly a hardware device.\n");
    BIO_printf(bio_err, "-passin arg    pass phrase source\n");
# endif

}

#else                           /* !OPENSSL_NO_RSA */

# if PEDANTIC
static void *dummy = &dummy;
# endif

#endif
