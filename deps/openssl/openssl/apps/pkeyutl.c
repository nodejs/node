/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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

#include "apps.h"
#include <string.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

#define KEY_PRIVKEY     1
#define KEY_PUBKEY      2
#define KEY_CERT        3

static void usage(void);

#undef PROG

#define PROG pkeyutl_main

static EVP_PKEY_CTX *init_ctx(int *pkeysize,
                              const char *keyfile, int keyform, int key_type,
                              char *passargin, int pkey_op, ENGINE *e,
                              int   impl);

static int setup_peer(BIO *err, EVP_PKEY_CTX *ctx, int peerform,
                      const char *file, ENGINE* e);

static int do_keyop(EVP_PKEY_CTX *ctx, int pkey_op,
                    unsigned char *out, size_t *poutlen,
                    unsigned char *in, size_t inlen);

int MAIN(int argc, char **);

int MAIN(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL;
    char *infile = NULL, *outfile = NULL, *sigfile = NULL;
    ENGINE *e = NULL;
    int pkey_op = EVP_PKEY_OP_SIGN, key_type = KEY_PRIVKEY;
    int keyform = FORMAT_PEM, peerform = FORMAT_PEM;
    char badarg = 0, rev = 0;
    char hexdump = 0, asn1parse = 0;
    EVP_PKEY_CTX *ctx = NULL;
    char *passargin = NULL;
    int keysize = -1;
    int engine_impl = 0;
    unsigned char *buf_in = NULL, *buf_out = NULL, *sig = NULL;
    size_t buf_outlen = 0;
    int buf_inlen = 0, siglen = -1;
    const char *inkey = NULL;
    const char *peerkey = NULL;
    STACK_OF(OPENSSL_STRING) *pkeyopts = NULL;

    int ret = 1, rv = -1;

    argc--;
    argv++;

    if (!bio_err)
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

    if (!load_config(bio_err, NULL))
        goto end;
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();

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
        } else if (!strcmp(*argv, "-sigfile")) {
            if (--argc < 1)
                badarg = 1;
            else
                sigfile = *(++argv);
        } else if (!strcmp(*argv, "-inkey")) {
            if (--argc < 1)
                badarg = 1;
            else
                inkey = *++argv;
        } else if (!strcmp(*argv, "-peerkey")) {
            if (--argc < 1)
                badarg = 1;
            else
                peerkey = *++argv;
        } else if (!strcmp(*argv, "-passin")) {
            if (--argc < 1)
                badarg = 1;
            else
                passargin = *(++argv);
        } else if (strcmp(*argv, "-peerform") == 0) {
            if (--argc < 1)
                badarg = 1;
            else
                peerform = str2fmt(*(++argv));
        } else if (strcmp(*argv, "-keyform") == 0) {
            if (--argc < 1)
                badarg = 1;
            else
                keyform = str2fmt(*(++argv));
        }
#ifndef OPENSSL_NO_ENGINE
        else if (!strcmp(*argv, "-engine")) {
            if (--argc < 1)
                badarg = 1;
            else
                e = setup_engine(bio_err, *(++argv), 0);
        } else if (!strcmp(*argv, "-engine_impl")) {
                engine_impl = 1;
        }
#endif
        else if (!strcmp(*argv, "-pubin"))
            key_type = KEY_PUBKEY;
        else if (!strcmp(*argv, "-certin"))
            key_type = KEY_CERT;
        else if (!strcmp(*argv, "-asn1parse"))
            asn1parse = 1;
        else if (!strcmp(*argv, "-hexdump"))
            hexdump = 1;
        else if (!strcmp(*argv, "-sign"))
            pkey_op = EVP_PKEY_OP_SIGN;
        else if (!strcmp(*argv, "-verify"))
            pkey_op = EVP_PKEY_OP_VERIFY;
        else if (!strcmp(*argv, "-verifyrecover"))
            pkey_op = EVP_PKEY_OP_VERIFYRECOVER;
        else if (!strcmp(*argv, "-encrypt"))
            pkey_op = EVP_PKEY_OP_ENCRYPT;
        else if (!strcmp(*argv, "-decrypt"))
            pkey_op = EVP_PKEY_OP_DECRYPT;
        else if (!strcmp(*argv, "-derive"))
            pkey_op = EVP_PKEY_OP_DERIVE;
        else if (!strcmp(*argv, "-rev"))
            rev = 1;
        else if (strcmp(*argv, "-pkeyopt") == 0) {
            if (--argc < 1)
                badarg = 1;
            else if ((pkeyopts == NULL &&
                     (pkeyopts = sk_OPENSSL_STRING_new_null()) == NULL) ||
                    sk_OPENSSL_STRING_push(pkeyopts, *++argv) == 0) {
                BIO_puts(bio_err, "out of memory\n");
                goto end;
            }
        } else
            badarg = 1;
        if (badarg) {
            usage();
            goto end;
        }
        argc--;
        argv++;
    }

    if (inkey == NULL ||
        (peerkey != NULL && pkey_op != EVP_PKEY_OP_DERIVE)) {
        usage();
        goto end;
    }
    ctx = init_ctx(&keysize, inkey, keyform, key_type,
                   passargin, pkey_op, e, engine_impl);
    if (!ctx) {
        BIO_puts(bio_err, "Error initializing context\n");
        ERR_print_errors(bio_err);
        goto end;
    }
    if (peerkey != NULL && !setup_peer(bio_err, ctx, peerform, peerkey, e)) {
        BIO_puts(bio_err, "Error setting up peer key\n");
        ERR_print_errors(bio_err);
        goto end;
    }
    if (pkeyopts != NULL) {
        int num = sk_OPENSSL_STRING_num(pkeyopts);
        int i;

        for (i = 0; i < num; ++i) {
            const char *opt = sk_OPENSSL_STRING_value(pkeyopts, i);

            if (pkey_ctrl_string(ctx, opt) <= 0) {
                BIO_puts(bio_err, "parameter setting error\n");
                ERR_print_errors(bio_err);
                goto end;
            }
        }
    }

    if (sigfile && (pkey_op != EVP_PKEY_OP_VERIFY)) {
        BIO_puts(bio_err, "Signature file specified for non verify\n");
        goto end;
    }

    if (!sigfile && (pkey_op == EVP_PKEY_OP_VERIFY)) {
        BIO_puts(bio_err, "No signature file specified for verify\n");
        goto end;
    }

/* FIXME: seed PRNG only if needed */
    app_RAND_load_file(NULL, bio_err, 0);

    if (pkey_op != EVP_PKEY_OP_DERIVE) {
        if (infile) {
            if (!(in = BIO_new_file(infile, "rb"))) {
                BIO_puts(bio_err, "Error Opening Input File\n");
                ERR_print_errors(bio_err);
                goto end;
            }
        } else
            in = BIO_new_fp(stdin, BIO_NOCLOSE);
    }

    if (outfile) {
        if (!(out = BIO_new_file(outfile, "wb"))) {
            BIO_printf(bio_err, "Error Creating Output File\n");
            ERR_print_errors(bio_err);
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

    if (sigfile) {
        BIO *sigbio = BIO_new_file(sigfile, "rb");
        if (!sigbio) {
            BIO_printf(bio_err, "Can't open signature file %s\n", sigfile);
            goto end;
        }
        siglen = bio_to_mem(&sig, keysize * 10, sigbio);
        BIO_free(sigbio);
        if (siglen < 0) {
            BIO_printf(bio_err, "Error reading signature data\n");
            goto end;
        }
    }

    if (in) {
        /* Read the input data */
        buf_inlen = bio_to_mem(&buf_in, keysize * 10, in);
        if (buf_inlen < 0) {
            BIO_printf(bio_err, "Error reading input Data\n");
            exit(1);
        }
        if (rev) {
            size_t i;
            unsigned char ctmp;
            size_t l = (size_t)buf_inlen;
            for (i = 0; i < l / 2; i++) {
                ctmp = buf_in[i];
                buf_in[i] = buf_in[l - 1 - i];
                buf_in[l - 1 - i] = ctmp;
            }
        }
    }

    if (pkey_op == EVP_PKEY_OP_VERIFY) {
        rv = EVP_PKEY_verify(ctx, sig, (size_t)siglen,
                             buf_in, (size_t)buf_inlen);
        if (rv == 0)
            BIO_puts(out, "Signature Verification Failure\n");
        else if (rv == 1) {
            BIO_puts(out, "Signature Verified Successfully\n");
            ret = 0;
        }
        if (rv >= 0)
            goto end;
    } else {
        rv = do_keyop(ctx, pkey_op, NULL, (size_t *)&buf_outlen,
                      buf_in, (size_t)buf_inlen);
        if (rv > 0 && buf_outlen != 0) {
            buf_out = OPENSSL_malloc(buf_outlen);
            if (!buf_out)
                rv = -1;
            else
                rv = do_keyop(ctx, pkey_op,
                              buf_out, (size_t *)&buf_outlen,
                              buf_in, (size_t)buf_inlen);
        }
    }

    if (rv <= 0) {
        BIO_printf(bio_err, "Public Key operation error\n");
        ERR_print_errors(bio_err);
        goto end;
    }
    ret = 0;
    if (asn1parse) {
        if (!ASN1_parse_dump(out, buf_out, buf_outlen, 1, -1))
            ERR_print_errors(bio_err);
    } else if (hexdump)
        BIO_dump(out, (char *)buf_out, buf_outlen);
    else
        BIO_write(out, buf_out, buf_outlen);

 end:
    if (ctx)
        EVP_PKEY_CTX_free(ctx);
    release_engine(e);
    BIO_free(in);
    BIO_free_all(out);
    if (buf_in != NULL)
        OPENSSL_free(buf_in);
    if (buf_out != NULL)
        OPENSSL_free(buf_out);
    if (sig != NULL)
        OPENSSL_free(sig);
    if (pkeyopts != NULL)
        sk_OPENSSL_STRING_free(pkeyopts);
    return ret;
}

static void usage()
{
    BIO_printf(bio_err, "Usage: pkeyutl [options]\n");
    BIO_printf(bio_err, "-in file        input file\n");
    BIO_printf(bio_err, "-out file       output file\n");
    BIO_printf(bio_err,
               "-sigfile file signature file (verify operation only)\n");
    BIO_printf(bio_err, "-inkey file     input key\n");
    BIO_printf(bio_err, "-keyform arg    private key format - default PEM\n");
    BIO_printf(bio_err, "-pubin          input is a public key\n");
    BIO_printf(bio_err,
               "-certin         input is a certificate carrying a public key\n");
    BIO_printf(bio_err, "-pkeyopt X:Y    public key options\n");
    BIO_printf(bio_err, "-sign           sign with private key\n");
    BIO_printf(bio_err, "-verify         verify with public key\n");
    BIO_printf(bio_err,
               "-verifyrecover  verify with public key, recover original data\n");
    BIO_printf(bio_err, "-encrypt        encrypt with public key\n");
    BIO_printf(bio_err, "-decrypt        decrypt with private key\n");
    BIO_printf(bio_err, "-derive         derive shared secret\n");
    BIO_printf(bio_err, "-hexdump        hex dump output\n");
#ifndef OPENSSL_NO_ENGINE
    BIO_printf(bio_err,
               "-engine e       use engine e, maybe a hardware device, for loading keys.\n");
    BIO_printf(bio_err, "-engine_impl    also use engine given by -engine for crypto operations\n");
#endif
    BIO_printf(bio_err, "-passin arg     pass phrase source\n");

}

static EVP_PKEY_CTX *init_ctx(int *pkeysize,
                              const char *keyfile, int keyform, int key_type,
                              char *passargin, int pkey_op, ENGINE *e,
                              int   engine_impl)
{
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    ENGINE *impl = NULL;
    char *passin = NULL;
    int rv = -1;
    X509 *x;
    if (((pkey_op == EVP_PKEY_OP_SIGN) || (pkey_op == EVP_PKEY_OP_DECRYPT)
         || (pkey_op == EVP_PKEY_OP_DERIVE))
        && (key_type != KEY_PRIVKEY)) {
        BIO_printf(bio_err, "A private key is needed for this operation\n");
        goto end;
    }
    if (!app_passwd(bio_err, passargin, NULL, &passin, NULL)) {
        BIO_printf(bio_err, "Error getting password\n");
        goto end;
    }
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

    *pkeysize = EVP_PKEY_size(pkey);

    if (!pkey)
        goto end;
        
#ifndef OPENSSL_NO_ENGINE
    if (engine_impl)
	impl = e;
#endif
            
    ctx = EVP_PKEY_CTX_new(pkey, impl);
    
    EVP_PKEY_free(pkey);

    if (!ctx)
        goto end;

    switch (pkey_op) {
    case EVP_PKEY_OP_SIGN:
        rv = EVP_PKEY_sign_init(ctx);
        break;

    case EVP_PKEY_OP_VERIFY:
        rv = EVP_PKEY_verify_init(ctx);
        break;

    case EVP_PKEY_OP_VERIFYRECOVER:
        rv = EVP_PKEY_verify_recover_init(ctx);
        break;

    case EVP_PKEY_OP_ENCRYPT:
        rv = EVP_PKEY_encrypt_init(ctx);
        break;

    case EVP_PKEY_OP_DECRYPT:
        rv = EVP_PKEY_decrypt_init(ctx);
        break;

    case EVP_PKEY_OP_DERIVE:
        rv = EVP_PKEY_derive_init(ctx);
        break;
    }

    if (rv <= 0) {
        EVP_PKEY_CTX_free(ctx);
        ctx = NULL;
    }

 end:

    if (passin)
        OPENSSL_free(passin);

    return ctx;

}

static int setup_peer(BIO *err, EVP_PKEY_CTX *ctx, int peerform,
                      const char *file, ENGINE* e)
{
    EVP_PKEY *peer = NULL;
    ENGINE* engine = NULL;
    int ret;

    if (peerform == FORMAT_ENGINE)
        engine = e;
    peer = load_pubkey(bio_err, file, peerform, 0, NULL, engine, "Peer Key");

    if (!peer) {
        BIO_printf(bio_err, "Error reading peer key %s\n", file);
        ERR_print_errors(err);
        return 0;
    }

    ret = EVP_PKEY_derive_set_peer(ctx, peer);

    EVP_PKEY_free(peer);
    if (ret <= 0)
        ERR_print_errors(err);
    return ret;
}

static int do_keyop(EVP_PKEY_CTX *ctx, int pkey_op,
                    unsigned char *out, size_t *poutlen,
                    unsigned char *in, size_t inlen)
{
    int rv = 0;
    switch (pkey_op) {
    case EVP_PKEY_OP_VERIFYRECOVER:
        rv = EVP_PKEY_verify_recover(ctx, out, poutlen, in, inlen);
        break;

    case EVP_PKEY_OP_SIGN:
        rv = EVP_PKEY_sign(ctx, out, poutlen, in, inlen);
        break;

    case EVP_PKEY_OP_ENCRYPT:
        rv = EVP_PKEY_encrypt(ctx, out, poutlen, in, inlen);
        break;

    case EVP_PKEY_OP_DECRYPT:
        rv = EVP_PKEY_decrypt(ctx, out, poutlen, in, inlen);
        break;

    case EVP_PKEY_OP_DERIVE:
        rv = EVP_PKEY_derive(ctx, out, poutlen);
        break;

    }
    return rv;
}
