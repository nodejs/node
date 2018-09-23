/* pkcs12.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 1999-2006 The OpenSSL Project.  All rights reserved.
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
#if !defined(OPENSSL_NO_DES) && !defined(OPENSSL_NO_SHA1)

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include "apps.h"
# include <openssl/crypto.h>
# include <openssl/err.h>
# include <openssl/pem.h>
# include <openssl/pkcs12.h>

# define PROG pkcs12_main

const EVP_CIPHER *enc;

# define NOKEYS          0x1
# define NOCERTS         0x2
# define INFO            0x4
# define CLCERTS         0x8
# define CACERTS         0x10

int get_cert_chain(X509 *cert, X509_STORE *store, STACK_OF(X509) **chain);
int dump_certs_keys_p12(BIO *out, PKCS12 *p12, char *pass, int passlen,
                        int options, char *pempass);
int dump_certs_pkeys_bags(BIO *out, STACK_OF(PKCS12_SAFEBAG) *bags,
                          char *pass, int passlen, int options,
                          char *pempass);
int dump_certs_pkeys_bag(BIO *out, PKCS12_SAFEBAG *bags, char *pass,
                         int passlen, int options, char *pempass);
int print_attribs(BIO *out, STACK_OF(X509_ATTRIBUTE) *attrlst,
                  const char *name);
void hex_prin(BIO *out, unsigned char *buf, int len);
int alg_print(BIO *x, X509_ALGOR *alg);
int cert_load(BIO *in, STACK_OF(X509) *sk);
static int set_pbe(BIO *err, int *ppbe, const char *str);

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    ENGINE *e = NULL;
    char *infile = NULL, *outfile = NULL, *keyname = NULL;
    char *certfile = NULL;
    BIO *in = NULL, *out = NULL;
    char **args;
    char *name = NULL;
    char *csp_name = NULL;
    int add_lmk = 0;
    PKCS12 *p12 = NULL;
    char pass[50], macpass[50];
    int export_cert = 0;
    int options = 0;
    int chain = 0;
    int badarg = 0;
    int iter = PKCS12_DEFAULT_ITER;
    int maciter = PKCS12_DEFAULT_ITER;
    int twopass = 0;
    int keytype = 0;
    int cert_pbe;
    int key_pbe = NID_pbe_WithSHA1And3_Key_TripleDES_CBC;
    int ret = 1;
    int macver = 1;
    int noprompt = 0;
    STACK_OF(OPENSSL_STRING) *canames = NULL;
    char *cpass = NULL, *mpass = NULL;
    char *passargin = NULL, *passargout = NULL, *passarg = NULL;
    char *passin = NULL, *passout = NULL;
    char *inrand = NULL;
    char *macalg = NULL;
    char *CApath = NULL, *CAfile = NULL;
# ifndef OPENSSL_NO_ENGINE
    char *engine = NULL;
# endif

    apps_startup();

# ifdef OPENSSL_FIPS
    if (FIPS_mode())
        cert_pbe = NID_pbe_WithSHA1And3_Key_TripleDES_CBC;
    else
# endif
        cert_pbe = NID_pbe_WithSHA1And40BitRC2_CBC;

    enc = EVP_des_ede3_cbc();
    if (bio_err == NULL)
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

    if (!load_config(bio_err, NULL))
        goto end;

    args = argv + 1;

    while (*args) {
        if (*args[0] == '-') {
            if (!strcmp(*args, "-nokeys"))
                options |= NOKEYS;
            else if (!strcmp(*args, "-keyex"))
                keytype = KEY_EX;
            else if (!strcmp(*args, "-keysig"))
                keytype = KEY_SIG;
            else if (!strcmp(*args, "-nocerts"))
                options |= NOCERTS;
            else if (!strcmp(*args, "-clcerts"))
                options |= CLCERTS;
            else if (!strcmp(*args, "-cacerts"))
                options |= CACERTS;
            else if (!strcmp(*args, "-noout"))
                options |= (NOKEYS | NOCERTS);
            else if (!strcmp(*args, "-info"))
                options |= INFO;
            else if (!strcmp(*args, "-chain"))
                chain = 1;
            else if (!strcmp(*args, "-twopass"))
                twopass = 1;
            else if (!strcmp(*args, "-nomacver"))
                macver = 0;
            else if (!strcmp(*args, "-descert"))
                cert_pbe = NID_pbe_WithSHA1And3_Key_TripleDES_CBC;
            else if (!strcmp(*args, "-export"))
                export_cert = 1;
            else if (!strcmp(*args, "-des"))
                enc = EVP_des_cbc();
            else if (!strcmp(*args, "-des3"))
                enc = EVP_des_ede3_cbc();
# ifndef OPENSSL_NO_IDEA
            else if (!strcmp(*args, "-idea"))
                enc = EVP_idea_cbc();
# endif
# ifndef OPENSSL_NO_SEED
            else if (!strcmp(*args, "-seed"))
                enc = EVP_seed_cbc();
# endif
# ifndef OPENSSL_NO_AES
            else if (!strcmp(*args, "-aes128"))
                enc = EVP_aes_128_cbc();
            else if (!strcmp(*args, "-aes192"))
                enc = EVP_aes_192_cbc();
            else if (!strcmp(*args, "-aes256"))
                enc = EVP_aes_256_cbc();
# endif
# ifndef OPENSSL_NO_CAMELLIA
            else if (!strcmp(*args, "-camellia128"))
                enc = EVP_camellia_128_cbc();
            else if (!strcmp(*args, "-camellia192"))
                enc = EVP_camellia_192_cbc();
            else if (!strcmp(*args, "-camellia256"))
                enc = EVP_camellia_256_cbc();
# endif
            else if (!strcmp(*args, "-noiter"))
                iter = 1;
            else if (!strcmp(*args, "-maciter"))
                maciter = PKCS12_DEFAULT_ITER;
            else if (!strcmp(*args, "-nomaciter"))
                maciter = 1;
            else if (!strcmp(*args, "-nomac"))
                maciter = -1;
            else if (!strcmp(*args, "-macalg"))
                if (args[1]) {
                    args++;
                    macalg = *args;
                } else
                    badarg = 1;
            else if (!strcmp(*args, "-nodes"))
                enc = NULL;
            else if (!strcmp(*args, "-certpbe")) {
                if (!set_pbe(bio_err, &cert_pbe, *++args))
                    badarg = 1;
            } else if (!strcmp(*args, "-keypbe")) {
                if (!set_pbe(bio_err, &key_pbe, *++args))
                    badarg = 1;
            } else if (!strcmp(*args, "-rand")) {
                if (args[1]) {
                    args++;
                    inrand = *args;
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-inkey")) {
                if (args[1]) {
                    args++;
                    keyname = *args;
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-certfile")) {
                if (args[1]) {
                    args++;
                    certfile = *args;
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-name")) {
                if (args[1]) {
                    args++;
                    name = *args;
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-LMK"))
                add_lmk = 1;
            else if (!strcmp(*args, "-CSP")) {
                if (args[1]) {
                    args++;
                    csp_name = *args;
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-caname")) {
                if (args[1]) {
                    args++;
                    if (!canames)
                        canames = sk_OPENSSL_STRING_new_null();
                    sk_OPENSSL_STRING_push(canames, *args);
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-in")) {
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
            } else if (!strcmp(*args, "-passin")) {
                if (args[1]) {
                    args++;
                    passargin = *args;
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-passout")) {
                if (args[1]) {
                    args++;
                    passargout = *args;
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-password")) {
                if (args[1]) {
                    args++;
                    passarg = *args;
                    noprompt = 1;
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-CApath")) {
                if (args[1]) {
                    args++;
                    CApath = *args;
                } else
                    badarg = 1;
            } else if (!strcmp(*args, "-CAfile")) {
                if (args[1]) {
                    args++;
                    CAfile = *args;
                } else
                    badarg = 1;
# ifndef OPENSSL_NO_ENGINE
            } else if (!strcmp(*args, "-engine")) {
                if (args[1]) {
                    args++;
                    engine = *args;
                } else
                    badarg = 1;
# endif
            } else
                badarg = 1;

        } else
            badarg = 1;
        args++;
    }

    if (badarg) {
        BIO_printf(bio_err, "Usage: pkcs12 [options]\n");
        BIO_printf(bio_err, "where options are\n");
        BIO_printf(bio_err, "-export       output PKCS12 file\n");
        BIO_printf(bio_err, "-chain        add certificate chain\n");
        BIO_printf(bio_err, "-inkey file   private key if not infile\n");
        BIO_printf(bio_err, "-certfile f   add all certs in f\n");
        BIO_printf(bio_err, "-CApath arg   - PEM format directory of CA's\n");
        BIO_printf(bio_err, "-CAfile arg   - PEM format file of CA's\n");
        BIO_printf(bio_err, "-name \"name\"  use name as friendly name\n");
        BIO_printf(bio_err,
                   "-caname \"nm\"  use nm as CA friendly name (can be used more than once).\n");
        BIO_printf(bio_err, "-in  infile   input filename\n");
        BIO_printf(bio_err, "-out outfile  output filename\n");
        BIO_printf(bio_err,
                   "-noout        don't output anything, just verify.\n");
        BIO_printf(bio_err, "-nomacver     don't verify MAC.\n");
        BIO_printf(bio_err, "-nocerts      don't output certificates.\n");
        BIO_printf(bio_err,
                   "-clcerts      only output client certificates.\n");
        BIO_printf(bio_err, "-cacerts      only output CA certificates.\n");
        BIO_printf(bio_err, "-nokeys       don't output private keys.\n");
        BIO_printf(bio_err,
                   "-info         give info about PKCS#12 structure.\n");
        BIO_printf(bio_err, "-des          encrypt private keys with DES\n");
        BIO_printf(bio_err,
                   "-des3         encrypt private keys with triple DES (default)\n");
# ifndef OPENSSL_NO_IDEA
        BIO_printf(bio_err, "-idea         encrypt private keys with idea\n");
# endif
# ifndef OPENSSL_NO_SEED
        BIO_printf(bio_err, "-seed         encrypt private keys with seed\n");
# endif
# ifndef OPENSSL_NO_AES
        BIO_printf(bio_err, "-aes128, -aes192, -aes256\n");
        BIO_printf(bio_err,
                   "              encrypt PEM output with cbc aes\n");
# endif
# ifndef OPENSSL_NO_CAMELLIA
        BIO_printf(bio_err, "-camellia128, -camellia192, -camellia256\n");
        BIO_printf(bio_err,
                   "              encrypt PEM output with cbc camellia\n");
# endif
        BIO_printf(bio_err, "-nodes        don't encrypt private keys\n");
        BIO_printf(bio_err, "-noiter       don't use encryption iteration\n");
        BIO_printf(bio_err, "-nomaciter    don't use MAC iteration\n");
        BIO_printf(bio_err, "-maciter      use MAC iteration\n");
        BIO_printf(bio_err, "-nomac        don't generate MAC\n");
        BIO_printf(bio_err,
                   "-twopass      separate MAC, encryption passwords\n");
        BIO_printf(bio_err,
                   "-descert      encrypt PKCS#12 certificates with triple DES (default RC2-40)\n");
        BIO_printf(bio_err,
                   "-certpbe alg  specify certificate PBE algorithm (default RC2-40)\n");
        BIO_printf(bio_err,
                   "-keypbe alg   specify private key PBE algorithm (default 3DES)\n");
        BIO_printf(bio_err,
                   "-macalg alg   digest algorithm used in MAC (default SHA1)\n");
        BIO_printf(bio_err, "-keyex        set MS key exchange type\n");
        BIO_printf(bio_err, "-keysig       set MS key signature type\n");
        BIO_printf(bio_err,
                   "-password p   set import/export password source\n");
        BIO_printf(bio_err, "-passin p     input file pass phrase source\n");
        BIO_printf(bio_err, "-passout p    output file pass phrase source\n");
# ifndef OPENSSL_NO_ENGINE
        BIO_printf(bio_err,
                   "-engine e     use engine e, possibly a hardware device.\n");
# endif
        BIO_printf(bio_err, "-rand file%cfile%c...\n", LIST_SEPARATOR_CHAR,
                   LIST_SEPARATOR_CHAR);
        BIO_printf(bio_err,
                   "              load the file (or the files in the directory) into\n");
        BIO_printf(bio_err, "              the random number generator\n");
        BIO_printf(bio_err, "-CSP name     Microsoft CSP name\n");
        BIO_printf(bio_err,
                   "-LMK          Add local machine keyset attribute to private key\n");
        goto end;
    }
# ifndef OPENSSL_NO_ENGINE
    e = setup_engine(bio_err, engine, 0);
# endif

    if (passarg) {
        if (export_cert)
            passargout = passarg;
        else
            passargin = passarg;
    }

    if (!app_passwd(bio_err, passargin, passargout, &passin, &passout)) {
        BIO_printf(bio_err, "Error getting passwords\n");
        goto end;
    }

    if (!cpass) {
        if (export_cert)
            cpass = passout;
        else
            cpass = passin;
    }

    if (cpass) {
        mpass = cpass;
        noprompt = 1;
    } else {
        cpass = pass;
        mpass = macpass;
    }

    if (export_cert || inrand) {
        app_RAND_load_file(NULL, bio_err, (inrand != NULL));
        if (inrand != NULL)
            BIO_printf(bio_err, "%ld semi-random bytes loaded\n",
                       app_RAND_load_files(inrand));
    }
    ERR_load_crypto_strings();

# ifdef CRYPTO_MDEBUG
    CRYPTO_push_info("read files");
# endif

    if (!infile)
        in = BIO_new_fp(stdin, BIO_NOCLOSE);
    else
        in = BIO_new_file(infile, "rb");
    if (!in) {
        BIO_printf(bio_err, "Error opening input file %s\n",
                   infile ? infile : "<stdin>");
        perror(infile);
        goto end;
    }
# ifdef CRYPTO_MDEBUG
    CRYPTO_pop_info();
    CRYPTO_push_info("write files");
# endif

    if (!outfile) {
        out = BIO_new_fp(stdout, BIO_NOCLOSE);
# ifdef OPENSSL_SYS_VMS
        {
            BIO *tmpbio = BIO_new(BIO_f_linebuffer());
            out = BIO_push(tmpbio, out);
        }
# endif
    } else
        out = BIO_new_file(outfile, "wb");
    if (!out) {
        BIO_printf(bio_err, "Error opening output file %s\n",
                   outfile ? outfile : "<stdout>");
        perror(outfile);
        goto end;
    }
    if (twopass) {
# ifdef CRYPTO_MDEBUG
        CRYPTO_push_info("read MAC password");
# endif
        if (EVP_read_pw_string
            (macpass, sizeof macpass, "Enter MAC Password:", export_cert)) {
            BIO_printf(bio_err, "Can't read Password\n");
            goto end;
        }
# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
# endif
    }

    if (export_cert) {
        EVP_PKEY *key = NULL;
        X509 *ucert = NULL, *x = NULL;
        STACK_OF(X509) *certs = NULL;
        const EVP_MD *macmd = NULL;
        unsigned char *catmp = NULL;
        int i;

        if ((options & (NOCERTS | NOKEYS)) == (NOCERTS | NOKEYS)) {
            BIO_printf(bio_err, "Nothing to do!\n");
            goto export_end;
        }

        if (options & NOCERTS)
            chain = 0;

# ifdef CRYPTO_MDEBUG
        CRYPTO_push_info("process -export_cert");
        CRYPTO_push_info("reading private key");
# endif
        if (!(options & NOKEYS)) {
            key = load_key(bio_err, keyname ? keyname : infile,
                           FORMAT_PEM, 1, passin, e, "private key");
            if (!key)
                goto export_end;
        }
# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
        CRYPTO_push_info("reading certs from input");
# endif

        /* Load in all certs in input file */
        if (!(options & NOCERTS)) {
            certs = load_certs(bio_err, infile, FORMAT_PEM, NULL, e,
                               "certificates");
            if (!certs)
                goto export_end;

            if (key) {
                /* Look for matching private key */
                for (i = 0; i < sk_X509_num(certs); i++) {
                    x = sk_X509_value(certs, i);
                    if (X509_check_private_key(x, key)) {
                        ucert = x;
                        /* Zero keyid and alias */
                        X509_keyid_set1(ucert, NULL, 0);
                        X509_alias_set1(ucert, NULL, 0);
                        /* Remove from list */
                        (void)sk_X509_delete(certs, i);
                        break;
                    }
                }
                if (!ucert) {
                    BIO_printf(bio_err,
                               "No certificate matches private key\n");
                    goto export_end;
                }
            }

        }
# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
        CRYPTO_push_info("reading certs from input 2");
# endif

        /* Add any more certificates asked for */
        if (certfile) {
            STACK_OF(X509) *morecerts = NULL;
            if (!(morecerts = load_certs(bio_err, certfile, FORMAT_PEM,
                                         NULL, e,
                                         "certificates from certfile")))
                goto export_end;
            while (sk_X509_num(morecerts) > 0)
                sk_X509_push(certs, sk_X509_shift(morecerts));
            sk_X509_free(morecerts);
        }
# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
        CRYPTO_push_info("reading certs from certfile");
# endif

# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
        CRYPTO_push_info("building chain");
# endif

        /* If chaining get chain from user cert */
        if (chain) {
            int vret;
            STACK_OF(X509) *chain2;
            X509_STORE *store = X509_STORE_new();
            if (!store) {
                BIO_printf(bio_err, "Memory allocation error\n");
                goto export_end;
            }
            if (!X509_STORE_load_locations(store, CAfile, CApath))
                X509_STORE_set_default_paths(store);

            vret = get_cert_chain(ucert, store, &chain2);
            X509_STORE_free(store);

            if (!vret) {
                /* Exclude verified certificate */
                for (i = 1; i < sk_X509_num(chain2); i++)
                    sk_X509_push(certs, sk_X509_value(chain2, i));
                /* Free first certificate */
                X509_free(sk_X509_value(chain2, 0));
                sk_X509_free(chain2);
            } else {
                if (vret >= 0)
                    BIO_printf(bio_err, "Error %s getting chain.\n",
                               X509_verify_cert_error_string(vret));
                else
                    ERR_print_errors(bio_err);
                goto export_end;
            }
        }

        /* Add any CA names */

        for (i = 0; i < sk_OPENSSL_STRING_num(canames); i++) {
            catmp = (unsigned char *)sk_OPENSSL_STRING_value(canames, i);
            X509_alias_set1(sk_X509_value(certs, i), catmp, -1);
        }

        if (csp_name && key)
            EVP_PKEY_add1_attr_by_NID(key, NID_ms_csp_name,
                                      MBSTRING_ASC, (unsigned char *)csp_name,
                                      -1);

        if (add_lmk && key)
            EVP_PKEY_add1_attr_by_NID(key, NID_LocalKeySet, 0, NULL, -1);

# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
        CRYPTO_push_info("reading password");
# endif

        if (!noprompt &&
            EVP_read_pw_string(pass, sizeof pass, "Enter Export Password:",
                               1)) {
            BIO_printf(bio_err, "Can't read Password\n");
            goto export_end;
        }
        if (!twopass)
            BUF_strlcpy(macpass, pass, sizeof macpass);

# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
        CRYPTO_push_info("creating PKCS#12 structure");
# endif

        p12 = PKCS12_create(cpass, name, key, ucert, certs,
                            key_pbe, cert_pbe, iter, -1, keytype);

        if (!p12) {
            ERR_print_errors(bio_err);
            goto export_end;
        }

        if (macalg) {
            macmd = EVP_get_digestbyname(macalg);
            if (!macmd) {
                BIO_printf(bio_err, "Unknown digest algorithm %s\n", macalg);
            }
        }

        if (maciter != -1)
            PKCS12_set_mac(p12, mpass, -1, NULL, 0, maciter, macmd);

# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
        CRYPTO_push_info("writing pkcs12");
# endif

        i2d_PKCS12_bio(out, p12);

        ret = 0;

 export_end:
# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
        CRYPTO_pop_info();
        CRYPTO_push_info("process -export_cert: freeing");
# endif

        if (key)
            EVP_PKEY_free(key);
        if (certs)
            sk_X509_pop_free(certs, X509_free);
        if (ucert)
            X509_free(ucert);

# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
# endif
        goto end;

    }

    if (!(p12 = d2i_PKCS12_bio(in, NULL))) {
        ERR_print_errors(bio_err);
        goto end;
    }
# ifdef CRYPTO_MDEBUG
    CRYPTO_push_info("read import password");
# endif
    if (!noprompt
        && EVP_read_pw_string(pass, sizeof pass, "Enter Import Password:",
                              0)) {
        BIO_printf(bio_err, "Can't read Password\n");
        goto end;
    }
# ifdef CRYPTO_MDEBUG
    CRYPTO_pop_info();
# endif

    if (!twopass)
        BUF_strlcpy(macpass, pass, sizeof macpass);

    if ((options & INFO) && p12->mac)
        BIO_printf(bio_err, "MAC Iteration %ld\n",
                   p12->mac->iter ? ASN1_INTEGER_get(p12->mac->iter) : 1);
    if (macver) {
# ifdef CRYPTO_MDEBUG
        CRYPTO_push_info("verify MAC");
# endif
        /* If we enter empty password try no password first */
        if (!mpass[0] && PKCS12_verify_mac(p12, NULL, 0)) {
            /* If mac and crypto pass the same set it to NULL too */
            if (!twopass)
                cpass = NULL;
        } else if (!PKCS12_verify_mac(p12, mpass, -1)) {
            BIO_printf(bio_err, "Mac verify error: invalid password?\n");
            ERR_print_errors(bio_err);
            goto end;
        }
        BIO_printf(bio_err, "MAC verified OK\n");
# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
# endif
    }
# ifdef CRYPTO_MDEBUG
    CRYPTO_push_info("output keys and certificates");
# endif
    if (!dump_certs_keys_p12(out, p12, cpass, -1, options, passout)) {
        BIO_printf(bio_err, "Error outputting keys and certificates\n");
        ERR_print_errors(bio_err);
        goto end;
    }
# ifdef CRYPTO_MDEBUG
    CRYPTO_pop_info();
# endif
    ret = 0;
 end:
    if (p12)
        PKCS12_free(p12);
    if (export_cert || inrand)
        app_RAND_write_file(NULL, bio_err);
# ifdef CRYPTO_MDEBUG
    CRYPTO_remove_all_info();
# endif
    BIO_free(in);
    BIO_free_all(out);
    if (canames)
        sk_OPENSSL_STRING_free(canames);
    if (passin)
        OPENSSL_free(passin);
    if (passout)
        OPENSSL_free(passout);
    apps_shutdown();
    OPENSSL_EXIT(ret);
}

int dump_certs_keys_p12(BIO *out, PKCS12 *p12, char *pass,
                        int passlen, int options, char *pempass)
{
    STACK_OF(PKCS7) *asafes = NULL;
    STACK_OF(PKCS12_SAFEBAG) *bags;
    int i, bagnid;
    int ret = 0;
    PKCS7 *p7;

    if (!(asafes = PKCS12_unpack_authsafes(p12)))
        return 0;
    for (i = 0; i < sk_PKCS7_num(asafes); i++) {
        p7 = sk_PKCS7_value(asafes, i);
        bagnid = OBJ_obj2nid(p7->type);
        if (bagnid == NID_pkcs7_data) {
            bags = PKCS12_unpack_p7data(p7);
            if (options & INFO)
                BIO_printf(bio_err, "PKCS7 Data\n");
        } else if (bagnid == NID_pkcs7_encrypted) {
            if (options & INFO) {
                BIO_printf(bio_err, "PKCS7 Encrypted data: ");
                alg_print(bio_err, p7->d.encrypted->enc_data->algorithm);
            }
            bags = PKCS12_unpack_p7encdata(p7, pass, passlen);
        } else
            continue;
        if (!bags)
            goto err;
        if (!dump_certs_pkeys_bags(out, bags, pass, passlen,
                                   options, pempass)) {
            sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
            goto err;
        }
        sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
        bags = NULL;
    }
    ret = 1;

 err:

    if (asafes)
        sk_PKCS7_pop_free(asafes, PKCS7_free);
    return ret;
}

int dump_certs_pkeys_bags(BIO *out, STACK_OF(PKCS12_SAFEBAG) *bags,
                          char *pass, int passlen, int options, char *pempass)
{
    int i;
    for (i = 0; i < sk_PKCS12_SAFEBAG_num(bags); i++) {
        if (!dump_certs_pkeys_bag(out,
                                  sk_PKCS12_SAFEBAG_value(bags, i),
                                  pass, passlen, options, pempass))
            return 0;
    }
    return 1;
}

int dump_certs_pkeys_bag(BIO *out, PKCS12_SAFEBAG *bag, char *pass,
                         int passlen, int options, char *pempass)
{
    EVP_PKEY *pkey;
    PKCS8_PRIV_KEY_INFO *p8;
    X509 *x509;

    switch (M_PKCS12_bag_type(bag)) {
    case NID_keyBag:
        if (options & INFO)
            BIO_printf(bio_err, "Key bag\n");
        if (options & NOKEYS)
            return 1;
        print_attribs(out, bag->attrib, "Bag Attributes");
        p8 = bag->value.keybag;
        if (!(pkey = EVP_PKCS82PKEY(p8)))
            return 0;
        print_attribs(out, p8->attributes, "Key Attributes");
        PEM_write_bio_PrivateKey(out, pkey, enc, NULL, 0, NULL, pempass);
        EVP_PKEY_free(pkey);
        break;

    case NID_pkcs8ShroudedKeyBag:
        if (options & INFO) {
            BIO_printf(bio_err, "Shrouded Keybag: ");
            alg_print(bio_err, bag->value.shkeybag->algor);
        }
        if (options & NOKEYS)
            return 1;
        print_attribs(out, bag->attrib, "Bag Attributes");
        if (!(p8 = PKCS12_decrypt_skey(bag, pass, passlen)))
            return 0;
        if (!(pkey = EVP_PKCS82PKEY(p8))) {
            PKCS8_PRIV_KEY_INFO_free(p8);
            return 0;
        }
        print_attribs(out, p8->attributes, "Key Attributes");
        PKCS8_PRIV_KEY_INFO_free(p8);
        PEM_write_bio_PrivateKey(out, pkey, enc, NULL, 0, NULL, pempass);
        EVP_PKEY_free(pkey);
        break;

    case NID_certBag:
        if (options & INFO)
            BIO_printf(bio_err, "Certificate bag\n");
        if (options & NOCERTS)
            return 1;
        if (PKCS12_get_attr(bag, NID_localKeyID)) {
            if (options & CACERTS)
                return 1;
        } else if (options & CLCERTS)
            return 1;
        print_attribs(out, bag->attrib, "Bag Attributes");
        if (M_PKCS12_cert_bag_type(bag) != NID_x509Certificate)
            return 1;
        if (!(x509 = PKCS12_certbag2x509(bag)))
            return 0;
        dump_cert_text(out, x509);
        PEM_write_bio_X509(out, x509);
        X509_free(x509);
        break;

    case NID_safeContentsBag:
        if (options & INFO)
            BIO_printf(bio_err, "Safe Contents bag\n");
        print_attribs(out, bag->attrib, "Bag Attributes");
        return dump_certs_pkeys_bags(out, bag->value.safes, pass,
                                     passlen, options, pempass);

    default:
        BIO_printf(bio_err, "Warning unsupported bag type: ");
        i2a_ASN1_OBJECT(bio_err, bag->type);
        BIO_printf(bio_err, "\n");
        return 1;
        break;
    }
    return 1;
}

/* Given a single certificate return a verified chain or NULL if error */

/* Hope this is OK .... */

int get_cert_chain(X509 *cert, X509_STORE *store, STACK_OF(X509) **chain)
{
    X509_STORE_CTX store_ctx;
    STACK_OF(X509) *chn;
    int i = 0;

    /*
     * FIXME: Should really check the return status of X509_STORE_CTX_init
     * for an error, but how that fits into the return value of this function
     * is less obvious.
     */
    X509_STORE_CTX_init(&store_ctx, store, cert, NULL);
    if (X509_verify_cert(&store_ctx) <= 0) {
        i = X509_STORE_CTX_get_error(&store_ctx);
        if (i == 0)
            /*
             * avoid returning 0 if X509_verify_cert() did not set an
             * appropriate error value in the context
             */
            i = -1;
        chn = NULL;
        goto err;
    } else
        chn = X509_STORE_CTX_get1_chain(&store_ctx);
 err:
    X509_STORE_CTX_cleanup(&store_ctx);
    *chain = chn;

    return i;
}

int alg_print(BIO *x, X509_ALGOR *alg)
{
    PBEPARAM *pbe;
    const unsigned char *p;
    p = alg->parameter->value.sequence->data;
    pbe = d2i_PBEPARAM(NULL, &p, alg->parameter->value.sequence->length);
    if (!pbe)
        return 1;
    BIO_printf(bio_err, "%s, Iteration %ld\n",
               OBJ_nid2ln(OBJ_obj2nid(alg->algorithm)),
               ASN1_INTEGER_get(pbe->iter));
    PBEPARAM_free(pbe);
    return 1;
}

/* Load all certificates from a given file */

int cert_load(BIO *in, STACK_OF(X509) *sk)
{
    int ret;
    X509 *cert;
    ret = 0;
# ifdef CRYPTO_MDEBUG
    CRYPTO_push_info("cert_load(): reading one cert");
# endif
    while ((cert = PEM_read_bio_X509(in, NULL, NULL, NULL))) {
# ifdef CRYPTO_MDEBUG
        CRYPTO_pop_info();
# endif
        ret = 1;
        sk_X509_push(sk, cert);
# ifdef CRYPTO_MDEBUG
        CRYPTO_push_info("cert_load(): reading one cert");
# endif
    }
# ifdef CRYPTO_MDEBUG
    CRYPTO_pop_info();
# endif
    if (ret)
        ERR_clear_error();
    return ret;
}

/* Generalised attribute print: handle PKCS#8 and bag attributes */

int print_attribs(BIO *out, STACK_OF(X509_ATTRIBUTE) *attrlst,
                  const char *name)
{
    X509_ATTRIBUTE *attr;
    ASN1_TYPE *av;
    char *value;
    int i, attr_nid;
    if (!attrlst) {
        BIO_printf(out, "%s: <No Attributes>\n", name);
        return 1;
    }
    if (!sk_X509_ATTRIBUTE_num(attrlst)) {
        BIO_printf(out, "%s: <Empty Attributes>\n", name);
        return 1;
    }
    BIO_printf(out, "%s\n", name);
    for (i = 0; i < sk_X509_ATTRIBUTE_num(attrlst); i++) {
        attr = sk_X509_ATTRIBUTE_value(attrlst, i);
        attr_nid = OBJ_obj2nid(attr->object);
        BIO_printf(out, "    ");
        if (attr_nid == NID_undef) {
            i2a_ASN1_OBJECT(out, attr->object);
            BIO_printf(out, ": ");
        } else
            BIO_printf(out, "%s: ", OBJ_nid2ln(attr_nid));

        if (sk_ASN1_TYPE_num(attr->value.set)) {
            av = sk_ASN1_TYPE_value(attr->value.set, 0);
            switch (av->type) {
            case V_ASN1_BMPSTRING:
                value = OPENSSL_uni2asc(av->value.bmpstring->data,
                                        av->value.bmpstring->length);
                BIO_printf(out, "%s\n", value);
                OPENSSL_free(value);
                break;

            case V_ASN1_OCTET_STRING:
                hex_prin(out, av->value.octet_string->data,
                         av->value.octet_string->length);
                BIO_printf(out, "\n");
                break;

            case V_ASN1_BIT_STRING:
                hex_prin(out, av->value.bit_string->data,
                         av->value.bit_string->length);
                BIO_printf(out, "\n");
                break;

            default:
                BIO_printf(out, "<Unsupported tag %d>\n", av->type);
                break;
            }
        } else
            BIO_printf(out, "<No Values>\n");
    }
    return 1;
}

void hex_prin(BIO *out, unsigned char *buf, int len)
{
    int i;
    for (i = 0; i < len; i++)
        BIO_printf(out, "%02X ", buf[i]);
}

static int set_pbe(BIO *err, int *ppbe, const char *str)
{
    if (!str)
        return 0;
    if (!strcmp(str, "NONE")) {
        *ppbe = -1;
        return 1;
    }
    *ppbe = OBJ_txt2nid(str);
    if (*ppbe == NID_undef) {
        BIO_printf(bio_err, "Unknown PBE algorithm %s\n", str);
        return 0;
    }
    return 1;
}

#endif
