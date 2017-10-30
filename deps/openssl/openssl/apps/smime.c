/* smime.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 1999-2004 The OpenSSL Project.  All rights reserved.
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

/* S/MIME utility function */

#include <stdio.h>
#include <string.h>
#include "apps.h"
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#undef PROG
#define PROG smime_main
static int save_certs(char *signerfile, STACK_OF(X509) *signers);
static int smime_cb(int ok, X509_STORE_CTX *ctx);

#define SMIME_OP        0x10
#define SMIME_IP        0x20
#define SMIME_SIGNERS   0x40
#define SMIME_ENCRYPT   (1 | SMIME_OP)
#define SMIME_DECRYPT   (2 | SMIME_IP)
#define SMIME_SIGN      (3 | SMIME_OP | SMIME_SIGNERS)
#define SMIME_VERIFY    (4 | SMIME_IP)
#define SMIME_PK7OUT    (5 | SMIME_IP | SMIME_OP)
#define SMIME_RESIGN    (6 | SMIME_IP | SMIME_OP | SMIME_SIGNERS)

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    ENGINE *e = NULL;
    int operation = 0;
    int ret = 0;
    char **args;
    const char *inmode = "r", *outmode = "w";
    char *infile = NULL, *outfile = NULL;
    char *signerfile = NULL, *recipfile = NULL;
    STACK_OF(OPENSSL_STRING) *sksigners = NULL, *skkeys = NULL;
    char *certfile = NULL, *keyfile = NULL, *contfile = NULL;
    const EVP_CIPHER *cipher = NULL;
    PKCS7 *p7 = NULL;
    X509_STORE *store = NULL;
    X509 *cert = NULL, *recip = NULL, *signer = NULL;
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *encerts = NULL, *other = NULL;
    BIO *in = NULL, *out = NULL, *indata = NULL;
    int badarg = 0;
    int flags = PKCS7_DETACHED;
    char *to = NULL, *from = NULL, *subject = NULL;
    char *CAfile = NULL, *CApath = NULL;
    char *passargin = NULL, *passin = NULL;
    char *inrand = NULL;
    int need_rand = 0;
    int indef = 0;
    const EVP_MD *sign_md = NULL;
    int informat = FORMAT_SMIME, outformat = FORMAT_SMIME;
    int keyform = FORMAT_PEM;
    char *engine = NULL;

    X509_VERIFY_PARAM *vpm = NULL;

    args = argv + 1;
    ret = 1;

    apps_startup();

    if (bio_err == NULL) {
        if ((bio_err = BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_err, stderr, BIO_NOCLOSE | BIO_FP_TEXT);
    }

    if (!load_config(bio_err, NULL))
        goto end;

    while (!badarg && *args && *args[0] == '-') {
        if (!strcmp(*args, "-encrypt"))
            operation = SMIME_ENCRYPT;
        else if (!strcmp(*args, "-decrypt"))
            operation = SMIME_DECRYPT;
        else if (!strcmp(*args, "-sign"))
            operation = SMIME_SIGN;
        else if (!strcmp(*args, "-resign"))
            operation = SMIME_RESIGN;
        else if (!strcmp(*args, "-verify"))
            operation = SMIME_VERIFY;
        else if (!strcmp(*args, "-pk7out"))
            operation = SMIME_PK7OUT;
#ifndef OPENSSL_NO_DES
        else if (!strcmp(*args, "-des3"))
            cipher = EVP_des_ede3_cbc();
        else if (!strcmp(*args, "-des"))
            cipher = EVP_des_cbc();
#endif
#ifndef OPENSSL_NO_SEED
        else if (!strcmp(*args, "-seed"))
            cipher = EVP_seed_cbc();
#endif
#ifndef OPENSSL_NO_RC2
        else if (!strcmp(*args, "-rc2-40"))
            cipher = EVP_rc2_40_cbc();
        else if (!strcmp(*args, "-rc2-128"))
            cipher = EVP_rc2_cbc();
        else if (!strcmp(*args, "-rc2-64"))
            cipher = EVP_rc2_64_cbc();
#endif
#ifndef OPENSSL_NO_AES
        else if (!strcmp(*args, "-aes128"))
            cipher = EVP_aes_128_cbc();
        else if (!strcmp(*args, "-aes192"))
            cipher = EVP_aes_192_cbc();
        else if (!strcmp(*args, "-aes256"))
            cipher = EVP_aes_256_cbc();
#endif
#ifndef OPENSSL_NO_CAMELLIA
        else if (!strcmp(*args, "-camellia128"))
            cipher = EVP_camellia_128_cbc();
        else if (!strcmp(*args, "-camellia192"))
            cipher = EVP_camellia_192_cbc();
        else if (!strcmp(*args, "-camellia256"))
            cipher = EVP_camellia_256_cbc();
#endif
        else if (!strcmp(*args, "-text"))
            flags |= PKCS7_TEXT;
        else if (!strcmp(*args, "-nointern"))
            flags |= PKCS7_NOINTERN;
        else if (!strcmp(*args, "-noverify"))
            flags |= PKCS7_NOVERIFY;
        else if (!strcmp(*args, "-nochain"))
            flags |= PKCS7_NOCHAIN;
        else if (!strcmp(*args, "-nocerts"))
            flags |= PKCS7_NOCERTS;
        else if (!strcmp(*args, "-noattr"))
            flags |= PKCS7_NOATTR;
        else if (!strcmp(*args, "-nodetach"))
            flags &= ~PKCS7_DETACHED;
        else if (!strcmp(*args, "-nosmimecap"))
            flags |= PKCS7_NOSMIMECAP;
        else if (!strcmp(*args, "-binary"))
            flags |= PKCS7_BINARY;
        else if (!strcmp(*args, "-nosigs"))
            flags |= PKCS7_NOSIGS;
        else if (!strcmp(*args, "-stream"))
            indef = 1;
        else if (!strcmp(*args, "-indef"))
            indef = 1;
        else if (!strcmp(*args, "-noindef"))
            indef = 0;
        else if (!strcmp(*args, "-nooldmime"))
            flags |= PKCS7_NOOLDMIMETYPE;
        else if (!strcmp(*args, "-crlfeol"))
            flags |= PKCS7_CRLFEOL;
        else if (!strcmp(*args, "-rand")) {
            if (!args[1])
                goto argerr;
            args++;
            inrand = *args;
            need_rand = 1;
        }
#ifndef OPENSSL_NO_ENGINE
        else if (!strcmp(*args, "-engine")) {
            if (!args[1])
                goto argerr;
            engine = *++args;
        }
#endif
        else if (!strcmp(*args, "-passin")) {
            if (!args[1])
                goto argerr;
            passargin = *++args;
        } else if (!strcmp(*args, "-to")) {
            if (!args[1])
                goto argerr;
            to = *++args;
        } else if (!strcmp(*args, "-from")) {
            if (!args[1])
                goto argerr;
            from = *++args;
        } else if (!strcmp(*args, "-subject")) {
            if (!args[1])
                goto argerr;
            subject = *++args;
        } else if (!strcmp(*args, "-signer")) {
            if (!args[1])
                goto argerr;
            /* If previous -signer argument add signer to list */

            if (signerfile) {
                if (!sksigners)
                    sksigners = sk_OPENSSL_STRING_new_null();
                sk_OPENSSL_STRING_push(sksigners, signerfile);
                if (!keyfile)
                    keyfile = signerfile;
                if (!skkeys)
                    skkeys = sk_OPENSSL_STRING_new_null();
                sk_OPENSSL_STRING_push(skkeys, keyfile);
                keyfile = NULL;
            }
            signerfile = *++args;
        } else if (!strcmp(*args, "-recip")) {
            if (!args[1])
                goto argerr;
            recipfile = *++args;
        } else if (!strcmp(*args, "-md")) {
            if (!args[1])
                goto argerr;
            sign_md = EVP_get_digestbyname(*++args);
            if (sign_md == NULL) {
                BIO_printf(bio_err, "Unknown digest %s\n", *args);
                goto argerr;
            }
        } else if (!strcmp(*args, "-inkey")) {
            if (!args[1])
                goto argerr;
            /* If previous -inkey arument add signer to list */
            if (keyfile) {
                if (!signerfile) {
                    BIO_puts(bio_err, "Illegal -inkey without -signer\n");
                    goto argerr;
                }
                if (!sksigners)
                    sksigners = sk_OPENSSL_STRING_new_null();
                sk_OPENSSL_STRING_push(sksigners, signerfile);
                signerfile = NULL;
                if (!skkeys)
                    skkeys = sk_OPENSSL_STRING_new_null();
                sk_OPENSSL_STRING_push(skkeys, keyfile);
            }
            keyfile = *++args;
        } else if (!strcmp(*args, "-keyform")) {
            if (!args[1])
                goto argerr;
            keyform = str2fmt(*++args);
        } else if (!strcmp(*args, "-certfile")) {
            if (!args[1])
                goto argerr;
            certfile = *++args;
        } else if (!strcmp(*args, "-CAfile")) {
            if (!args[1])
                goto argerr;
            CAfile = *++args;
        } else if (!strcmp(*args, "-CApath")) {
            if (!args[1])
                goto argerr;
            CApath = *++args;
        } else if (!strcmp(*args, "-in")) {
            if (!args[1])
                goto argerr;
            infile = *++args;
        } else if (!strcmp(*args, "-inform")) {
            if (!args[1])
                goto argerr;
            informat = str2fmt(*++args);
        } else if (!strcmp(*args, "-outform")) {
            if (!args[1])
                goto argerr;
            outformat = str2fmt(*++args);
        } else if (!strcmp(*args, "-out")) {
            if (!args[1])
                goto argerr;
            outfile = *++args;
        } else if (!strcmp(*args, "-content")) {
            if (!args[1])
                goto argerr;
            contfile = *++args;
        } else if (args_verify(&args, NULL, &badarg, bio_err, &vpm))
            continue;
        else if ((cipher = EVP_get_cipherbyname(*args + 1)) == NULL)
            badarg = 1;
        args++;
    }

    if (!(operation & SMIME_SIGNERS) && (skkeys || sksigners)) {
        BIO_puts(bio_err, "Multiple signers or keys not allowed\n");
        goto argerr;
    }

    if (operation & SMIME_SIGNERS) {
        /* Check to see if any final signer needs to be appended */
        if (keyfile && !signerfile) {
            BIO_puts(bio_err, "Illegal -inkey without -signer\n");
            goto argerr;
        }
        if (signerfile) {
            if (!sksigners)
                sksigners = sk_OPENSSL_STRING_new_null();
            sk_OPENSSL_STRING_push(sksigners, signerfile);
            if (!skkeys)
                skkeys = sk_OPENSSL_STRING_new_null();
            if (!keyfile)
                keyfile = signerfile;
            sk_OPENSSL_STRING_push(skkeys, keyfile);
        }
        if (!sksigners) {
            BIO_printf(bio_err, "No signer certificate specified\n");
            badarg = 1;
        }
        signerfile = NULL;
        keyfile = NULL;
        need_rand = 1;
    } else if (operation == SMIME_DECRYPT) {
        if (!recipfile && !keyfile) {
            BIO_printf(bio_err,
                       "No recipient certificate or key specified\n");
            badarg = 1;
        }
    } else if (operation == SMIME_ENCRYPT) {
        if (!*args) {
            BIO_printf(bio_err, "No recipient(s) certificate(s) specified\n");
            badarg = 1;
        }
        need_rand = 1;
    } else if (!operation)
        badarg = 1;

    if (badarg) {
 argerr:
        BIO_printf(bio_err, "Usage smime [options] cert.pem ...\n");
        BIO_printf(bio_err, "where options are\n");
        BIO_printf(bio_err, "-encrypt       encrypt message\n");
        BIO_printf(bio_err, "-decrypt       decrypt encrypted message\n");
        BIO_printf(bio_err, "-sign          sign message\n");
        BIO_printf(bio_err, "-verify        verify signed message\n");
        BIO_printf(bio_err, "-pk7out        output PKCS#7 structure\n");
#ifndef OPENSSL_NO_DES
        BIO_printf(bio_err, "-des3          encrypt with triple DES\n");
        BIO_printf(bio_err, "-des           encrypt with DES\n");
#endif
#ifndef OPENSSL_NO_SEED
        BIO_printf(bio_err, "-seed          encrypt with SEED\n");
#endif
#ifndef OPENSSL_NO_RC2
        BIO_printf(bio_err, "-rc2-40        encrypt with RC2-40 (default)\n");
        BIO_printf(bio_err, "-rc2-64        encrypt with RC2-64\n");
        BIO_printf(bio_err, "-rc2-128       encrypt with RC2-128\n");
#endif
#ifndef OPENSSL_NO_AES
        BIO_printf(bio_err, "-aes128, -aes192, -aes256\n");
        BIO_printf(bio_err,
                   "               encrypt PEM output with cbc aes\n");
#endif
#ifndef OPENSSL_NO_CAMELLIA
        BIO_printf(bio_err, "-camellia128, -camellia192, -camellia256\n");
        BIO_printf(bio_err,
                   "               encrypt PEM output with cbc camellia\n");
#endif
        BIO_printf(bio_err,
                   "-nointern      don't search certificates in message for signer\n");
        BIO_printf(bio_err,
                   "-nosigs        don't verify message signature\n");
        BIO_printf(bio_err,
                   "-noverify      don't verify signers certificate\n");
        BIO_printf(bio_err,
                   "-nocerts       don't include signers certificate when signing\n");
        BIO_printf(bio_err, "-nodetach      use opaque signing\n");
        BIO_printf(bio_err,
                   "-noattr        don't include any signed attributes\n");
        BIO_printf(bio_err,
                   "-binary        don't translate message to text\n");
        BIO_printf(bio_err, "-certfile file other certificates file\n");
        BIO_printf(bio_err, "-signer file   signer certificate file\n");
        BIO_printf(bio_err,
                   "-recip  file   recipient certificate file for decryption\n");
        BIO_printf(bio_err, "-in file       input file\n");
        BIO_printf(bio_err,
                   "-inform arg    input format SMIME (default), PEM or DER\n");
        BIO_printf(bio_err,
                   "-inkey file    input private key (if not signer or recipient)\n");
        BIO_printf(bio_err,
                   "-keyform arg   input private key format (PEM or ENGINE)\n");
        BIO_printf(bio_err, "-out file      output file\n");
        BIO_printf(bio_err,
                   "-outform arg   output format SMIME (default), PEM or DER\n");
        BIO_printf(bio_err,
                   "-content file  supply or override content for detached signature\n");
        BIO_printf(bio_err, "-to addr       to address\n");
        BIO_printf(bio_err, "-from ad       from address\n");
        BIO_printf(bio_err, "-subject s     subject\n");
        BIO_printf(bio_err,
                   "-text          include or delete text MIME headers\n");
        BIO_printf(bio_err,
                   "-CApath dir    trusted certificates directory\n");
        BIO_printf(bio_err, "-CAfile file   trusted certificates file\n");
        BIO_printf(bio_err,
                   "-no_alt_chains only ever use the first certificate chain found\n");
        BIO_printf(bio_err,
                   "-crl_check     check revocation status of signer's certificate using CRLs\n");
        BIO_printf(bio_err,
                   "-crl_check_all check revocation status of signer's certificate chain using CRLs\n");
#ifndef OPENSSL_NO_ENGINE
        BIO_printf(bio_err,
                   "-engine e      use engine e, possibly a hardware device.\n");
#endif
        BIO_printf(bio_err, "-passin arg    input file pass phrase source\n");
        BIO_printf(bio_err, "-rand file%cfile%c...\n", LIST_SEPARATOR_CHAR,
                   LIST_SEPARATOR_CHAR);
        BIO_printf(bio_err,
                   "               load the file (or the files in the directory) into\n");
        BIO_printf(bio_err, "               the random number generator\n");
        BIO_printf(bio_err,
                   "cert.pem       recipient certificate(s) for encryption\n");
        goto end;
    }
    e = setup_engine(bio_err, engine, 0);

    if (!app_passwd(bio_err, passargin, NULL, &passin, NULL)) {
        BIO_printf(bio_err, "Error getting password\n");
        goto end;
    }

    if (need_rand) {
        app_RAND_load_file(NULL, bio_err, (inrand != NULL));
        if (inrand != NULL)
            BIO_printf(bio_err, "%ld semi-random bytes loaded\n",
                       app_RAND_load_files(inrand));
    }

    ret = 2;

    if (!(operation & SMIME_SIGNERS))
        flags &= ~PKCS7_DETACHED;

    if (operation & SMIME_OP) {
        if (outformat == FORMAT_ASN1)
            outmode = "wb";
    } else {
        if (flags & PKCS7_BINARY)
            outmode = "wb";
    }

    if (operation & SMIME_IP) {
        if (informat == FORMAT_ASN1)
            inmode = "rb";
    } else {
        if (flags & PKCS7_BINARY)
            inmode = "rb";
    }

    if (operation == SMIME_ENCRYPT) {
        if (!cipher) {
#ifndef OPENSSL_NO_DES
            cipher = EVP_des_ede3_cbc();
#else
            BIO_printf(bio_err, "No cipher selected\n");
            goto end;
#endif
        }
        encerts = sk_X509_new_null();
        while (*args) {
            if (!(cert = load_cert(bio_err, *args, FORMAT_PEM,
                                   NULL, e, "recipient certificate file"))) {
#if 0                           /* An appropriate message is already printed */
                BIO_printf(bio_err,
                           "Can't read recipient certificate file %s\n",
                           *args);
#endif
                goto end;
            }
            sk_X509_push(encerts, cert);
            cert = NULL;
            args++;
        }
    }

    if (certfile) {
        if (!(other = load_certs(bio_err, certfile, FORMAT_PEM, NULL,
                                 e, "certificate file"))) {
            ERR_print_errors(bio_err);
            goto end;
        }
    }

    if (recipfile && (operation == SMIME_DECRYPT)) {
        if (!(recip = load_cert(bio_err, recipfile, FORMAT_PEM, NULL,
                                e, "recipient certificate file"))) {
            ERR_print_errors(bio_err);
            goto end;
        }
    }

    if (operation == SMIME_DECRYPT) {
        if (!keyfile)
            keyfile = recipfile;
    } else if (operation == SMIME_SIGN) {
        if (!keyfile)
            keyfile = signerfile;
    } else
        keyfile = NULL;

    if (keyfile) {
        key = load_key(bio_err, keyfile, keyform, 0, passin, e,
                       "signing key file");
        if (!key)
            goto end;
    }

    if (infile) {
        if (!(in = BIO_new_file(infile, inmode))) {
            BIO_printf(bio_err, "Can't open input file %s\n", infile);
            goto end;
        }
    } else
        in = BIO_new_fp(stdin, BIO_NOCLOSE);

    if (operation & SMIME_IP) {
        if (informat == FORMAT_SMIME)
            p7 = SMIME_read_PKCS7(in, &indata);
        else if (informat == FORMAT_PEM)
            p7 = PEM_read_bio_PKCS7(in, NULL, NULL, NULL);
        else if (informat == FORMAT_ASN1)
            p7 = d2i_PKCS7_bio(in, NULL);
        else {
            BIO_printf(bio_err, "Bad input format for PKCS#7 file\n");
            goto end;
        }

        if (!p7) {
            BIO_printf(bio_err, "Error reading S/MIME message\n");
            goto end;
        }
        if (contfile) {
            BIO_free(indata);
            if (!(indata = BIO_new_file(contfile, "rb"))) {
                BIO_printf(bio_err, "Can't read content file %s\n", contfile);
                goto end;
            }
        }
    }

    if (outfile) {
        if (!(out = BIO_new_file(outfile, outmode))) {
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

    if (operation == SMIME_VERIFY) {
        if (!(store = setup_verify(bio_err, CAfile, CApath)))
            goto end;
        X509_STORE_set_verify_cb(store, smime_cb);
        if (vpm)
            X509_STORE_set1_param(store, vpm);
    }

    ret = 3;

    if (operation == SMIME_ENCRYPT) {
        if (indef)
            flags |= PKCS7_STREAM;
        p7 = PKCS7_encrypt(encerts, in, cipher, flags);
    } else if (operation & SMIME_SIGNERS) {
        int i;
        /*
         * If detached data content we only enable streaming if S/MIME output
         * format.
         */
        if (operation == SMIME_SIGN) {
            if (flags & PKCS7_DETACHED) {
                if (outformat == FORMAT_SMIME)
                    flags |= PKCS7_STREAM;
            } else if (indef)
                flags |= PKCS7_STREAM;
            flags |= PKCS7_PARTIAL;
            p7 = PKCS7_sign(NULL, NULL, other, in, flags);
            if (!p7)
                goto end;
            if (flags & PKCS7_NOCERTS) {
                for (i = 0; i < sk_X509_num(other); i++) {
                    X509 *x = sk_X509_value(other, i);
                    PKCS7_add_certificate(p7, x);
                }
            }
        } else
            flags |= PKCS7_REUSE_DIGEST;
        for (i = 0; i < sk_OPENSSL_STRING_num(sksigners); i++) {
            signerfile = sk_OPENSSL_STRING_value(sksigners, i);
            keyfile = sk_OPENSSL_STRING_value(skkeys, i);
            signer = load_cert(bio_err, signerfile, FORMAT_PEM, NULL,
                               e, "signer certificate");
            if (!signer)
                goto end;
            key = load_key(bio_err, keyfile, keyform, 0, passin, e,
                           "signing key file");
            if (!key)
                goto end;
            if (!PKCS7_sign_add_signer(p7, signer, key, sign_md, flags))
                goto end;
            X509_free(signer);
            signer = NULL;
            EVP_PKEY_free(key);
            key = NULL;
        }
        /* If not streaming or resigning finalize structure */
        if ((operation == SMIME_SIGN) && !(flags & PKCS7_STREAM)) {
            if (!PKCS7_final(p7, in, flags))
                goto end;
        }
    }

    if (!p7) {
        BIO_printf(bio_err, "Error creating PKCS#7 structure\n");
        goto end;
    }

    ret = 4;
    if (operation == SMIME_DECRYPT) {
        if (!PKCS7_decrypt(p7, key, recip, out, flags)) {
            BIO_printf(bio_err, "Error decrypting PKCS#7 structure\n");
            goto end;
        }
    } else if (operation == SMIME_VERIFY) {
        STACK_OF(X509) *signers;
        if (PKCS7_verify(p7, other, store, indata, out, flags))
            BIO_printf(bio_err, "Verification successful\n");
        else {
            BIO_printf(bio_err, "Verification failure\n");
            goto end;
        }
        signers = PKCS7_get0_signers(p7, other, flags);
        if (!save_certs(signerfile, signers)) {
            BIO_printf(bio_err, "Error writing signers to %s\n", signerfile);
            ret = 5;
            goto end;
        }
        sk_X509_free(signers);
    } else if (operation == SMIME_PK7OUT)
        PEM_write_bio_PKCS7(out, p7);
    else {
        if (to)
            BIO_printf(out, "To: %s\n", to);
        if (from)
            BIO_printf(out, "From: %s\n", from);
        if (subject)
            BIO_printf(out, "Subject: %s\n", subject);
        if (outformat == FORMAT_SMIME) {
            if (operation == SMIME_RESIGN)
                SMIME_write_PKCS7(out, p7, indata, flags);
            else
                SMIME_write_PKCS7(out, p7, in, flags);
        } else if (outformat == FORMAT_PEM)
            PEM_write_bio_PKCS7_stream(out, p7, in, flags);
        else if (outformat == FORMAT_ASN1)
            i2d_PKCS7_bio_stream(out, p7, in, flags);
        else {
            BIO_printf(bio_err, "Bad output format for PKCS#7 file\n");
            goto end;
        }
    }
    ret = 0;
 end:
    if (need_rand)
        app_RAND_write_file(NULL, bio_err);
    if (ret)
        ERR_print_errors(bio_err);
    sk_X509_pop_free(encerts, X509_free);
    sk_X509_pop_free(other, X509_free);
    if (vpm)
        X509_VERIFY_PARAM_free(vpm);
    if (sksigners)
        sk_OPENSSL_STRING_free(sksigners);
    if (skkeys)
        sk_OPENSSL_STRING_free(skkeys);
    X509_STORE_free(store);
    X509_free(cert);
    X509_free(recip);
    X509_free(signer);
    EVP_PKEY_free(key);
    PKCS7_free(p7);
    release_engine(e);
    BIO_free(in);
    BIO_free(indata);
    BIO_free_all(out);
    if (passin)
        OPENSSL_free(passin);
    return (ret);
}

static int save_certs(char *signerfile, STACK_OF(X509) *signers)
{
    int i;
    BIO *tmp;
    if (!signerfile)
        return 1;
    tmp = BIO_new_file(signerfile, "w");
    if (!tmp)
        return 0;
    for (i = 0; i < sk_X509_num(signers); i++)
        PEM_write_bio_X509(tmp, sk_X509_value(signers, i));
    BIO_free(tmp);
    return 1;
}

/* Minimal callback just to output policy info (if any) */

static int smime_cb(int ok, X509_STORE_CTX *ctx)
{
    int error;

    error = X509_STORE_CTX_get_error(ctx);

    if ((error != X509_V_ERR_NO_EXPLICIT_POLICY)
        && ((error != X509_V_OK) || (ok != 2)))
        return ok;

    policies_print(NULL, ctx);

    return ok;

}
