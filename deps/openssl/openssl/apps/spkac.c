/* apps/spkac.c */

/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 1999. Based on an original idea by Massimiliano Pala (madwolf@openca.org).
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "apps.h"
#include <openssl/bio.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/lhash.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

#undef PROG
#define PROG    spkac_main

/*-
 * -in arg      - input file - default stdin
 * -out arg     - output file - default stdout
 */

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    ENGINE *e = NULL;
    int i, badops = 0, ret = 1;
    BIO *in = NULL, *out = NULL;
    int verify = 0, noout = 0, pubkey = 0;
    char *infile = NULL, *outfile = NULL, *prog;
    char *passargin = NULL, *passin = NULL;
    const char *spkac = "SPKAC", *spksect = "default";
    char *spkstr = NULL;
    char *challenge = NULL, *keyfile = NULL;
    CONF *conf = NULL;
    NETSCAPE_SPKI *spki = NULL;
    EVP_PKEY *pkey = NULL;
#ifndef OPENSSL_NO_ENGINE
    char *engine = NULL;
#endif

    apps_startup();

    if (!bio_err)
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

    if (!load_config(bio_err, NULL))
        goto end;

    prog = argv[0];
    argc--;
    argv++;
    while (argc >= 1) {
        if (strcmp(*argv, "-in") == 0) {
            if (--argc < 1)
                goto bad;
            infile = *(++argv);
        } else if (strcmp(*argv, "-out") == 0) {
            if (--argc < 1)
                goto bad;
            outfile = *(++argv);
        } else if (strcmp(*argv, "-passin") == 0) {
            if (--argc < 1)
                goto bad;
            passargin = *(++argv);
        } else if (strcmp(*argv, "-key") == 0) {
            if (--argc < 1)
                goto bad;
            keyfile = *(++argv);
        } else if (strcmp(*argv, "-challenge") == 0) {
            if (--argc < 1)
                goto bad;
            challenge = *(++argv);
        } else if (strcmp(*argv, "-spkac") == 0) {
            if (--argc < 1)
                goto bad;
            spkac = *(++argv);
        } else if (strcmp(*argv, "-spksect") == 0) {
            if (--argc < 1)
                goto bad;
            spksect = *(++argv);
        }
#ifndef OPENSSL_NO_ENGINE
        else if (strcmp(*argv, "-engine") == 0) {
            if (--argc < 1)
                goto bad;
            engine = *(++argv);
        }
#endif
        else if (strcmp(*argv, "-noout") == 0)
            noout = 1;
        else if (strcmp(*argv, "-pubkey") == 0)
            pubkey = 1;
        else if (strcmp(*argv, "-verify") == 0)
            verify = 1;
        else
            badops = 1;
        argc--;
        argv++;
    }

    if (badops) {
 bad:
        BIO_printf(bio_err, "%s [options]\n", prog);
        BIO_printf(bio_err, "where options are\n");
        BIO_printf(bio_err, " -in arg        input file\n");
        BIO_printf(bio_err, " -out arg       output file\n");
        BIO_printf(bio_err,
                   " -key arg       create SPKAC using private key\n");
        BIO_printf(bio_err,
                   " -passin arg    input file pass phrase source\n");
        BIO_printf(bio_err, " -challenge arg challenge string\n");
        BIO_printf(bio_err, " -spkac arg     alternative SPKAC name\n");
        BIO_printf(bio_err, " -noout         don't print SPKAC\n");
        BIO_printf(bio_err, " -pubkey        output public key\n");
        BIO_printf(bio_err, " -verify        verify SPKAC signature\n");
#ifndef OPENSSL_NO_ENGINE
        BIO_printf(bio_err,
                   " -engine e      use engine e, possibly a hardware device.\n");
#endif
        goto end;
    }

    ERR_load_crypto_strings();
    if (!app_passwd(bio_err, passargin, NULL, &passin, NULL)) {
        BIO_printf(bio_err, "Error getting password\n");
        goto end;
    }
#ifndef OPENSSL_NO_ENGINE
    e = setup_engine(bio_err, engine, 0);
#endif

    if (keyfile) {
        pkey = load_key(bio_err,
                        strcmp(keyfile, "-") ? keyfile : NULL,
                        FORMAT_PEM, 1, passin, e, "private key");
        if (!pkey) {
            goto end;
        }
        spki = NETSCAPE_SPKI_new();
        if (challenge)
            ASN1_STRING_set(spki->spkac->challenge,
                            challenge, (int)strlen(challenge));
        NETSCAPE_SPKI_set_pubkey(spki, pkey);
        NETSCAPE_SPKI_sign(spki, pkey, EVP_md5());
        spkstr = NETSCAPE_SPKI_b64_encode(spki);

        if (outfile)
            out = BIO_new_file(outfile, "w");
        else {
            out = BIO_new_fp(stdout, BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
            {
                BIO *tmpbio = BIO_new(BIO_f_linebuffer());
                out = BIO_push(tmpbio, out);
            }
#endif
        }

        if (!out) {
            BIO_printf(bio_err, "Error opening output file\n");
            ERR_print_errors(bio_err);
            goto end;
        }
        BIO_printf(out, "SPKAC=%s\n", spkstr);
        OPENSSL_free(spkstr);
        ret = 0;
        goto end;
    }

    if (infile)
        in = BIO_new_file(infile, "r");
    else
        in = BIO_new_fp(stdin, BIO_NOCLOSE);

    if (!in) {
        BIO_printf(bio_err, "Error opening input file\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    conf = NCONF_new(NULL);
    i = NCONF_load_bio(conf, in, NULL);

    if (!i) {
        BIO_printf(bio_err, "Error parsing config file\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    spkstr = NCONF_get_string(conf, spksect, spkac);

    if (!spkstr) {
        BIO_printf(bio_err, "Can't find SPKAC called \"%s\"\n", spkac);
        ERR_print_errors(bio_err);
        goto end;
    }

    spki = NETSCAPE_SPKI_b64_decode(spkstr, -1);

    if (!spki) {
        BIO_printf(bio_err, "Error loading SPKAC\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    if (outfile)
        out = BIO_new_file(outfile, "w");
    else {
        out = BIO_new_fp(stdout, BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
        {
            BIO *tmpbio = BIO_new(BIO_f_linebuffer());
            out = BIO_push(tmpbio, out);
        }
#endif
    }

    if (!out) {
        BIO_printf(bio_err, "Error opening output file\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    if (!noout)
        NETSCAPE_SPKI_print(out, spki);
    pkey = NETSCAPE_SPKI_get_pubkey(spki);
    if (verify) {
        i = NETSCAPE_SPKI_verify(spki, pkey);
        if (i > 0)
            BIO_printf(bio_err, "Signature OK\n");
        else {
            BIO_printf(bio_err, "Signature Failure\n");
            ERR_print_errors(bio_err);
            goto end;
        }
    }
    if (pubkey)
        PEM_write_bio_PUBKEY(out, pkey);

    ret = 0;

 end:
    NCONF_free(conf);
    NETSCAPE_SPKI_free(spki);
    BIO_free(in);
    BIO_free_all(out);
    EVP_PKEY_free(pkey);
    if (passin)
        OPENSSL_free(passin);
    apps_shutdown();
    OPENSSL_EXIT(ret);
}
