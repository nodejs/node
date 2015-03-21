/* pkcs8.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 1999-2004.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
#include <stdio.h>
#include <string.h>
#include "apps.h"
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>

#define PROG pkcs8_main

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    ENGINE *e = NULL;
    char **args, *infile = NULL, *outfile = NULL;
    char *passargin = NULL, *passargout = NULL;
    BIO *in = NULL, *out = NULL;
    int topk8 = 0;
    int pbe_nid = -1;
    const EVP_CIPHER *cipher = NULL;
    int iter = PKCS12_DEFAULT_ITER;
    int informat, outformat;
    int p8_broken = PKCS8_OK;
    int nocrypt = 0;
    X509_SIG *p8 = NULL;
    PKCS8_PRIV_KEY_INFO *p8inf = NULL;
    EVP_PKEY *pkey = NULL;
    char pass[50], *passin = NULL, *passout = NULL, *p8pass = NULL;
    int badarg = 0;
    int ret = 1;
#ifndef OPENSSL_NO_ENGINE
    char *engine = NULL;
#endif

    if (bio_err == NULL)
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

    if (!load_config(bio_err, NULL))
        goto end;

    informat = FORMAT_PEM;
    outformat = FORMAT_PEM;

    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    args = argv + 1;
    while (!badarg && *args && *args[0] == '-') {
        if (!strcmp(*args, "-v2")) {
            if (args[1]) {
                args++;
                cipher = EVP_get_cipherbyname(*args);
                if (!cipher) {
                    BIO_printf(bio_err, "Unknown cipher %s\n", *args);
                    badarg = 1;
                }
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-v1")) {
            if (args[1]) {
                args++;
                pbe_nid = OBJ_txt2nid(*args);
                if (pbe_nid == NID_undef) {
                    BIO_printf(bio_err, "Unknown PBE algorithm %s\n", *args);
                    badarg = 1;
                }
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-inform")) {
            if (args[1]) {
                args++;
                informat = str2fmt(*args);
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-outform")) {
            if (args[1]) {
                args++;
                outformat = str2fmt(*args);
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-topk8"))
            topk8 = 1;
        else if (!strcmp(*args, "-noiter"))
            iter = 1;
        else if (!strcmp(*args, "-nocrypt"))
            nocrypt = 1;
        else if (!strcmp(*args, "-nooct"))
            p8_broken = PKCS8_NO_OCTET;
        else if (!strcmp(*args, "-nsdb"))
            p8_broken = PKCS8_NS_DB;
        else if (!strcmp(*args, "-embed"))
            p8_broken = PKCS8_EMBEDDED_PARAM;
        else if (!strcmp(*args, "-passin")) {
            if (!args[1])
                goto bad;
            passargin = *(++args);
        } else if (!strcmp(*args, "-passout")) {
            if (!args[1])
                goto bad;
            passargout = *(++args);
        }
#ifndef OPENSSL_NO_ENGINE
        else if (strcmp(*args, "-engine") == 0) {
            if (!args[1])
                goto bad;
            engine = *(++args);
        }
#endif
        else if (!strcmp(*args, "-in")) {
            if (args[1]) {
                args++;
                infile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-out")) {
            if (args[1]) {
                args++;
                outfile = *args;
            } else
                badarg = 1;
        } else
            badarg = 1;
        args++;
    }

    if (badarg) {
 bad:
        BIO_printf(bio_err, "Usage pkcs8 [options]\n");
        BIO_printf(bio_err, "where options are\n");
        BIO_printf(bio_err, "-in file        input file\n");
        BIO_printf(bio_err, "-inform X       input format (DER or PEM)\n");
        BIO_printf(bio_err,
                   "-passin arg     input file pass phrase source\n");
        BIO_printf(bio_err, "-outform X      output format (DER or PEM)\n");
        BIO_printf(bio_err, "-out file       output file\n");
        BIO_printf(bio_err,
                   "-passout arg    output file pass phrase source\n");
        BIO_printf(bio_err, "-topk8          output PKCS8 file\n");
        BIO_printf(bio_err,
                   "-nooct          use (nonstandard) no octet format\n");
        BIO_printf(bio_err,
                   "-embed          use (nonstandard) embedded DSA parameters format\n");
        BIO_printf(bio_err,
                   "-nsdb           use (nonstandard) DSA Netscape DB format\n");
        BIO_printf(bio_err, "-noiter         use 1 as iteration count\n");
        BIO_printf(bio_err,
                   "-nocrypt        use or expect unencrypted private key\n");
        BIO_printf(bio_err,
                   "-v2 alg         use PKCS#5 v2.0 and cipher \"alg\"\n");
        BIO_printf(bio_err,
                   "-v1 obj         use PKCS#5 v1.5 and cipher \"alg\"\n");
#ifndef OPENSSL_NO_ENGINE
        BIO_printf(bio_err,
                   " -engine e       use engine e, possibly a hardware device.\n");
#endif
        goto end;
    }
#ifndef OPENSSL_NO_ENGINE
    e = setup_engine(bio_err, engine, 0);
#endif

    if (!app_passwd(bio_err, passargin, passargout, &passin, &passout)) {
        BIO_printf(bio_err, "Error getting passwords\n");
        goto end;
    }

    if ((pbe_nid == -1) && !cipher)
        pbe_nid = NID_pbeWithMD5AndDES_CBC;

    if (infile) {
        if (!(in = BIO_new_file(infile, "rb"))) {
            BIO_printf(bio_err, "Can't open input file %s\n", infile);
            goto end;
        }
    } else
        in = BIO_new_fp(stdin, BIO_NOCLOSE);

    if (outfile) {
        if (!(out = BIO_new_file(outfile, "wb"))) {
            BIO_printf(bio_err, "Can't open output file %s\n", outfile);
            goto end;
        }
    } else {
        out = BIO_new_fp(stdout, BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
        {
            BIO *tmpbio = BIO_new(BIO_f_linebuffer());
            out = BIO_push(tmpbio, out);
        }
#endif
    }
    if (topk8) {
        pkey = load_key(bio_err, infile, informat, 1, passin, e, "key");
        if (!pkey)
            goto end;
        if (!(p8inf = EVP_PKEY2PKCS8_broken(pkey, p8_broken))) {
            BIO_printf(bio_err, "Error converting key\n");
            ERR_print_errors(bio_err);
            goto end;
        }
        if (nocrypt) {
            if (outformat == FORMAT_PEM)
                PEM_write_bio_PKCS8_PRIV_KEY_INFO(out, p8inf);
            else if (outformat == FORMAT_ASN1)
                i2d_PKCS8_PRIV_KEY_INFO_bio(out, p8inf);
            else {
                BIO_printf(bio_err, "Bad format specified for key\n");
                goto end;
            }
        } else {
            if (passout)
                p8pass = passout;
            else {
                p8pass = pass;
                if (EVP_read_pw_string
                    (pass, sizeof pass, "Enter Encryption Password:", 1))
                    goto end;
            }
            app_RAND_load_file(NULL, bio_err, 0);
            if (!(p8 = PKCS8_encrypt(pbe_nid, cipher,
                                     p8pass, strlen(p8pass),
                                     NULL, 0, iter, p8inf))) {
                BIO_printf(bio_err, "Error encrypting key\n");
                ERR_print_errors(bio_err);
                goto end;
            }
            app_RAND_write_file(NULL, bio_err);
            if (outformat == FORMAT_PEM)
                PEM_write_bio_PKCS8(out, p8);
            else if (outformat == FORMAT_ASN1)
                i2d_PKCS8_bio(out, p8);
            else {
                BIO_printf(bio_err, "Bad format specified for key\n");
                goto end;
            }
        }

        ret = 0;
        goto end;
    }

    if (nocrypt) {
        if (informat == FORMAT_PEM)
            p8inf = PEM_read_bio_PKCS8_PRIV_KEY_INFO(in, NULL, NULL, NULL);
        else if (informat == FORMAT_ASN1)
            p8inf = d2i_PKCS8_PRIV_KEY_INFO_bio(in, NULL);
        else {
            BIO_printf(bio_err, "Bad format specified for key\n");
            goto end;
        }
    } else {
        if (informat == FORMAT_PEM)
            p8 = PEM_read_bio_PKCS8(in, NULL, NULL, NULL);
        else if (informat == FORMAT_ASN1)
            p8 = d2i_PKCS8_bio(in, NULL);
        else {
            BIO_printf(bio_err, "Bad format specified for key\n");
            goto end;
        }

        if (!p8) {
            BIO_printf(bio_err, "Error reading key\n");
            ERR_print_errors(bio_err);
            goto end;
        }
        if (passin)
            p8pass = passin;
        else {
            p8pass = pass;
            EVP_read_pw_string(pass, sizeof pass, "Enter Password:", 0);
        }
        p8inf = PKCS8_decrypt(p8, p8pass, strlen(p8pass));
    }

    if (!p8inf) {
        BIO_printf(bio_err, "Error decrypting key\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    if (!(pkey = EVP_PKCS82PKEY(p8inf))) {
        BIO_printf(bio_err, "Error converting key\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    if (p8inf->broken) {
        BIO_printf(bio_err, "Warning: broken key encoding: ");
        switch (p8inf->broken) {
        case PKCS8_NO_OCTET:
            BIO_printf(bio_err, "No Octet String in PrivateKey\n");
            break;

        case PKCS8_EMBEDDED_PARAM:
            BIO_printf(bio_err, "DSA parameters included in PrivateKey\n");
            break;

        case PKCS8_NS_DB:
            BIO_printf(bio_err, "DSA public key include in PrivateKey\n");
            break;

        case PKCS8_NEG_PRIVKEY:
            BIO_printf(bio_err, "DSA private key value is negative\n");
            break;

        default:
            BIO_printf(bio_err, "Unknown broken type\n");
            break;
        }
    }

    if (outformat == FORMAT_PEM)
        PEM_write_bio_PrivateKey(out, pkey, NULL, NULL, 0, NULL, passout);
    else if (outformat == FORMAT_ASN1)
        i2d_PrivateKey_bio(out, pkey);
    else {
        BIO_printf(bio_err, "Bad format specified for key\n");
        goto end;
    }
    ret = 0;

 end:
    X509_SIG_free(p8);
    PKCS8_PRIV_KEY_INFO_free(p8inf);
    EVP_PKEY_free(pkey);
    BIO_free_all(out);
    BIO_free(in);
    if (passin)
        OPENSSL_free(passin);
    if (passout)
        OPENSSL_free(passout);

    return ret;
}
