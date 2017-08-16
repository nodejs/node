/* apps/genrsa.c */
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

#include <openssl/opensslconf.h>
/*
 * Until the key-gen callbacks are modified to use newer prototypes, we allow
 * deprecated functions for openssl-internal code
 */
#ifdef OPENSSL_NO_DEPRECATED
# undef OPENSSL_NO_DEPRECATED
#endif

#ifndef OPENSSL_NO_RSA
# include <stdio.h>
# include <string.h>
# include <sys/types.h>
# include <sys/stat.h>
# include "apps.h"
# include <openssl/bio.h>
# include <openssl/err.h>
# include <openssl/bn.h>
# include <openssl/rsa.h>
# include <openssl/evp.h>
# include <openssl/x509.h>
# include <openssl/pem.h>
# include <openssl/rand.h>

# define DEFBITS 2048
# undef PROG
# define PROG genrsa_main

static int MS_CALLBACK genrsa_cb(int p, int n, BN_GENCB *cb);

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    BN_GENCB cb;
    ENGINE *e = NULL;
    int ret = 1;
    int i, num = DEFBITS;
    long l;
    const EVP_CIPHER *enc = NULL;
    unsigned long f4 = RSA_F4;
    char *outfile = NULL;
    char *passargout = NULL, *passout = NULL;
    char *engine = NULL;
    char *inrand = NULL;
    BIO *out = NULL;
    BIGNUM *bn = BN_new();
    RSA *rsa = NULL;

    if (!bn)
        goto err;

    apps_startup();
    BN_GENCB_set(&cb, genrsa_cb, bio_err);

    if (bio_err == NULL)
        if ((bio_err = BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_err, stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    if (!load_config(bio_err, NULL))
        goto err;
    if ((out = BIO_new(BIO_s_file())) == NULL) {
        BIO_printf(bio_err, "unable to create BIO for output\n");
        goto err;
    }

    argv++;
    argc--;
    for (;;) {
        if (argc <= 0)
            break;
        if (strcmp(*argv, "-out") == 0) {
            if (--argc < 1)
                goto bad;
            outfile = *(++argv);
        } else if (strcmp(*argv, "-3") == 0)
            f4 = 3;
        else if (strcmp(*argv, "-F4") == 0 || strcmp(*argv, "-f4") == 0)
            f4 = RSA_F4;
# ifndef OPENSSL_NO_ENGINE
        else if (strcmp(*argv, "-engine") == 0) {
            if (--argc < 1)
                goto bad;
            engine = *(++argv);
        }
# endif
        else if (strcmp(*argv, "-rand") == 0) {
            if (--argc < 1)
                goto bad;
            inrand = *(++argv);
        }
# ifndef OPENSSL_NO_DES
        else if (strcmp(*argv, "-des") == 0)
            enc = EVP_des_cbc();
        else if (strcmp(*argv, "-des3") == 0)
            enc = EVP_des_ede3_cbc();
# endif
# ifndef OPENSSL_NO_IDEA
        else if (strcmp(*argv, "-idea") == 0)
            enc = EVP_idea_cbc();
# endif
# ifndef OPENSSL_NO_SEED
        else if (strcmp(*argv, "-seed") == 0)
            enc = EVP_seed_cbc();
# endif
# ifndef OPENSSL_NO_AES
        else if (strcmp(*argv, "-aes128") == 0)
            enc = EVP_aes_128_cbc();
        else if (strcmp(*argv, "-aes192") == 0)
            enc = EVP_aes_192_cbc();
        else if (strcmp(*argv, "-aes256") == 0)
            enc = EVP_aes_256_cbc();
# endif
# ifndef OPENSSL_NO_CAMELLIA
        else if (strcmp(*argv, "-camellia128") == 0)
            enc = EVP_camellia_128_cbc();
        else if (strcmp(*argv, "-camellia192") == 0)
            enc = EVP_camellia_192_cbc();
        else if (strcmp(*argv, "-camellia256") == 0)
            enc = EVP_camellia_256_cbc();
# endif
        else if (strcmp(*argv, "-passout") == 0) {
            if (--argc < 1)
                goto bad;
            passargout = *(++argv);
        } else
            break;
        argv++;
        argc--;
    }
    if ((argc >= 1) && ((sscanf(*argv, "%d", &num) == 0) || (num < 0))) {
 bad:
        BIO_printf(bio_err, "usage: genrsa [args] [numbits]\n");
        BIO_printf(bio_err,
                   " -des            encrypt the generated key with DES in cbc mode\n");
        BIO_printf(bio_err,
                   " -des3           encrypt the generated key with DES in ede cbc mode (168 bit key)\n");
# ifndef OPENSSL_NO_IDEA
        BIO_printf(bio_err,
                   " -idea           encrypt the generated key with IDEA in cbc mode\n");
# endif
# ifndef OPENSSL_NO_SEED
        BIO_printf(bio_err, " -seed\n");
        BIO_printf(bio_err,
                   "                 encrypt PEM output with cbc seed\n");
# endif
# ifndef OPENSSL_NO_AES
        BIO_printf(bio_err, " -aes128, -aes192, -aes256\n");
        BIO_printf(bio_err,
                   "                 encrypt PEM output with cbc aes\n");
# endif
# ifndef OPENSSL_NO_CAMELLIA
        BIO_printf(bio_err, " -camellia128, -camellia192, -camellia256\n");
        BIO_printf(bio_err,
                   "                 encrypt PEM output with cbc camellia\n");
# endif
        BIO_printf(bio_err, " -out file       output the key to 'file\n");
        BIO_printf(bio_err,
                   " -passout arg    output file pass phrase source\n");
        BIO_printf(bio_err,
                   " -f4             use F4 (0x10001) for the E value\n");
        BIO_printf(bio_err, " -3              use 3 for the E value\n");
# ifndef OPENSSL_NO_ENGINE
        BIO_printf(bio_err,
                   " -engine e       use engine e, possibly a hardware device.\n");
# endif
        BIO_printf(bio_err, " -rand file%cfile%c...\n", LIST_SEPARATOR_CHAR,
                   LIST_SEPARATOR_CHAR);
        BIO_printf(bio_err,
                   "                 load the file (or the files in the directory) into\n");
        BIO_printf(bio_err, "                 the random number generator\n");
        goto err;
    }

    ERR_load_crypto_strings();

    if (!app_passwd(bio_err, NULL, passargout, NULL, &passout)) {
        BIO_printf(bio_err, "Error getting password\n");
        goto err;
    }
    e = setup_engine(bio_err, engine, 0);

    if (outfile == NULL) {
        BIO_set_fp(out, stdout, BIO_NOCLOSE);
# ifdef OPENSSL_SYS_VMS
        {
            BIO *tmpbio = BIO_new(BIO_f_linebuffer());
            out = BIO_push(tmpbio, out);
        }
# endif
    } else {
        if (BIO_write_filename(out, outfile) <= 0) {
            perror(outfile);
            goto err;
        }
    }

    if (!app_RAND_load_file(NULL, bio_err, 1) && inrand == NULL
        && !RAND_status()) {
        BIO_printf(bio_err,
                   "warning, not much extra random data, consider using the -rand option\n");
    }
    if (inrand != NULL)
        BIO_printf(bio_err, "%ld semi-random bytes loaded\n",
                   app_RAND_load_files(inrand));

    BIO_printf(bio_err, "Generating RSA private key, %d bit long modulus\n",
               num);
# ifdef OPENSSL_NO_ENGINE
    rsa = RSA_new();
# else
    rsa = RSA_new_method(e);
# endif
    if (!rsa)
        goto err;

    if (!BN_set_word(bn, f4) || !RSA_generate_key_ex(rsa, num, bn, &cb))
        goto err;

    app_RAND_write_file(NULL, bio_err);

    /*
     * We need to do the following for when the base number size is < long,
     * esp windows 3.1 :-(.
     */
    l = 0L;
    for (i = 0; i < rsa->e->top; i++) {
# ifndef SIXTY_FOUR_BIT
        l <<= BN_BITS4;
        l <<= BN_BITS4;
# endif
        l += rsa->e->d[i];
    }
    BIO_printf(bio_err, "e is %ld (0x%lX)\n", l, l);
    {
        PW_CB_DATA cb_data;
        cb_data.password = passout;
        cb_data.prompt_info = outfile;
        if (!PEM_write_bio_RSAPrivateKey(out, rsa, enc, NULL, 0,
                                         (pem_password_cb *)password_callback,
                                         &cb_data))
            goto err;
    }

    ret = 0;
 err:
    if (bn)
        BN_free(bn);
    if (rsa)
        RSA_free(rsa);
    if (out)
        BIO_free_all(out);
    release_engine(e);
    if (passout)
        OPENSSL_free(passout);
    if (ret != 0)
        ERR_print_errors(bio_err);
    apps_shutdown();
    OPENSSL_EXIT(ret);
}

static int MS_CALLBACK genrsa_cb(int p, int n, BN_GENCB *cb)
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
    BIO_write(cb->arg, &c, 1);
    (void)BIO_flush(cb->arg);
# ifdef LINT
    p = n;
# endif
    return 1;
}
#else                           /* !OPENSSL_NO_RSA */

# if PEDANTIC
static void *dummy = &dummy;
# endif

#endif
