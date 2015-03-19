/* apps/crl2p7.c */
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

/*
 * This was written by Gordon Chaffee <chaffee@plateau.cs.berkeley.edu> and
 * donated 'to the cause' along with lots and lots of other fixes to the
 * library.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "apps.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pkcs7.h>
#include <openssl/pem.h>
#include <openssl/objects.h>

static int add_certs_from_file(STACK_OF(X509) *stack, char *certfile);
#undef PROG
#define PROG    crl2pkcs7_main

/*-
 * -inform arg  - input format - default PEM (DER or PEM)
 * -outform arg - output format - default PEM
 * -in arg      - input file - default stdin
 * -out arg     - output file - default stdout
 */

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    int i, badops = 0;
    BIO *in = NULL, *out = NULL;
    int informat, outformat;
    char *infile, *outfile, *prog, *certfile;
    PKCS7 *p7 = NULL;
    PKCS7_SIGNED *p7s = NULL;
    X509_CRL *crl = NULL;
    STACK_OF(OPENSSL_STRING) *certflst = NULL;
    STACK_OF(X509_CRL) *crl_stack = NULL;
    STACK_OF(X509) *cert_stack = NULL;
    int ret = 1, nocrl = 0;

    apps_startup();

    if (bio_err == NULL)
        if ((bio_err = BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_err, stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    infile = NULL;
    outfile = NULL;
    informat = FORMAT_PEM;
    outformat = FORMAT_PEM;

    prog = argv[0];
    argc--;
    argv++;
    while (argc >= 1) {
        if (strcmp(*argv, "-inform") == 0) {
            if (--argc < 1)
                goto bad;
            informat = str2fmt(*(++argv));
        } else if (strcmp(*argv, "-outform") == 0) {
            if (--argc < 1)
                goto bad;
            outformat = str2fmt(*(++argv));
        } else if (strcmp(*argv, "-in") == 0) {
            if (--argc < 1)
                goto bad;
            infile = *(++argv);
        } else if (strcmp(*argv, "-nocrl") == 0) {
            nocrl = 1;
        } else if (strcmp(*argv, "-out") == 0) {
            if (--argc < 1)
                goto bad;
            outfile = *(++argv);
        } else if (strcmp(*argv, "-certfile") == 0) {
            if (--argc < 1)
                goto bad;
            if (!certflst)
                certflst = sk_OPENSSL_STRING_new_null();
            if (!certflst)
                goto end;
            if (!sk_OPENSSL_STRING_push(certflst, *(++argv))) {
                sk_OPENSSL_STRING_free(certflst);
                goto end;
            }
        } else {
            BIO_printf(bio_err, "unknown option %s\n", *argv);
            badops = 1;
            break;
        }
        argc--;
        argv++;
    }

    if (badops) {
 bad:
        BIO_printf(bio_err, "%s [options] <infile >outfile\n", prog);
        BIO_printf(bio_err, "where options are\n");
        BIO_printf(bio_err, " -inform arg    input format - DER or PEM\n");
        BIO_printf(bio_err, " -outform arg   output format - DER or PEM\n");
        BIO_printf(bio_err, " -in arg        input file\n");
        BIO_printf(bio_err, " -out arg       output file\n");
        BIO_printf(bio_err,
                   " -certfile arg  certificates file of chain to a trusted CA\n");
        BIO_printf(bio_err, "                (can be used more than once)\n");
        BIO_printf(bio_err,
                   " -nocrl         no crl to load, just certs from '-certfile'\n");
        ret = 1;
        goto end;
    }

    ERR_load_crypto_strings();

    in = BIO_new(BIO_s_file());
    out = BIO_new(BIO_s_file());
    if ((in == NULL) || (out == NULL)) {
        ERR_print_errors(bio_err);
        goto end;
    }

    if (!nocrl) {
        if (infile == NULL)
            BIO_set_fp(in, stdin, BIO_NOCLOSE);
        else {
            if (BIO_read_filename(in, infile) <= 0) {
                perror(infile);
                goto end;
            }
        }

        if (informat == FORMAT_ASN1)
            crl = d2i_X509_CRL_bio(in, NULL);
        else if (informat == FORMAT_PEM)
            crl = PEM_read_bio_X509_CRL(in, NULL, NULL, NULL);
        else {
            BIO_printf(bio_err, "bad input format specified for input crl\n");
            goto end;
        }
        if (crl == NULL) {
            BIO_printf(bio_err, "unable to load CRL\n");
            ERR_print_errors(bio_err);
            goto end;
        }
    }

    if ((p7 = PKCS7_new()) == NULL)
        goto end;
    if ((p7s = PKCS7_SIGNED_new()) == NULL)
        goto end;
    p7->type = OBJ_nid2obj(NID_pkcs7_signed);
    p7->d.sign = p7s;
    p7s->contents->type = OBJ_nid2obj(NID_pkcs7_data);

    if (!ASN1_INTEGER_set(p7s->version, 1))
        goto end;
    if ((crl_stack = sk_X509_CRL_new_null()) == NULL)
        goto end;
    p7s->crl = crl_stack;
    if (crl != NULL) {
        sk_X509_CRL_push(crl_stack, crl);
        crl = NULL;             /* now part of p7 for OPENSSL_freeing */
    }

    if ((cert_stack = sk_X509_new_null()) == NULL)
        goto end;
    p7s->cert = cert_stack;

    if (certflst)
        for (i = 0; i < sk_OPENSSL_STRING_num(certflst); i++) {
            certfile = sk_OPENSSL_STRING_value(certflst, i);
            if (add_certs_from_file(cert_stack, certfile) < 0) {
                BIO_printf(bio_err, "error loading certificates\n");
                ERR_print_errors(bio_err);
                goto end;
            }
        }

    sk_OPENSSL_STRING_free(certflst);

    if (outfile == NULL) {
        BIO_set_fp(out, stdout, BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
        {
            BIO *tmpbio = BIO_new(BIO_f_linebuffer());
            out = BIO_push(tmpbio, out);
        }
#endif
    } else {
        if (BIO_write_filename(out, outfile) <= 0) {
            perror(outfile);
            goto end;
        }
    }

    if (outformat == FORMAT_ASN1)
        i = i2d_PKCS7_bio(out, p7);
    else if (outformat == FORMAT_PEM)
        i = PEM_write_bio_PKCS7(out, p7);
    else {
        BIO_printf(bio_err, "bad output format specified for outfile\n");
        goto end;
    }
    if (!i) {
        BIO_printf(bio_err, "unable to write pkcs7 object\n");
        ERR_print_errors(bio_err);
        goto end;
    }
    ret = 0;
 end:
    if (in != NULL)
        BIO_free(in);
    if (out != NULL)
        BIO_free_all(out);
    if (p7 != NULL)
        PKCS7_free(p7);
    if (crl != NULL)
        X509_CRL_free(crl);

    apps_shutdown();
    OPENSSL_EXIT(ret);
}

/*-
 *----------------------------------------------------------------------
 * int add_certs_from_file
 *
 *      Read a list of certificates to be checked from a file.
 *
 * Results:
 *      number of certs added if successful, -1 if not.
 *----------------------------------------------------------------------
 */
static int add_certs_from_file(STACK_OF(X509) *stack, char *certfile)
{
    BIO *in = NULL;
    int count = 0;
    int ret = -1;
    STACK_OF(X509_INFO) *sk = NULL;
    X509_INFO *xi;

    in = BIO_new(BIO_s_file());
    if ((in == NULL) || (BIO_read_filename(in, certfile) <= 0)) {
        BIO_printf(bio_err, "error opening the file, %s\n", certfile);
        goto end;
    }

    /* This loads from a file, a stack of x509/crl/pkey sets */
    sk = PEM_X509_INFO_read_bio(in, NULL, NULL, NULL);
    if (sk == NULL) {
        BIO_printf(bio_err, "error reading the file, %s\n", certfile);
        goto end;
    }

    /* scan over it and pull out the CRL's */
    while (sk_X509_INFO_num(sk)) {
        xi = sk_X509_INFO_shift(sk);
        if (xi->x509 != NULL) {
            sk_X509_push(stack, xi->x509);
            xi->x509 = NULL;
            count++;
        }
        X509_INFO_free(xi);
    }

    ret = count;
 end:
    /* never need to OPENSSL_free x */
    if (in != NULL)
        BIO_free(in);
    if (sk != NULL)
        sk_X509_INFO_free(sk);
    return (ret);
}
