/* apps/enc.c */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apps.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#ifndef OPENSSL_NO_COMP
# include <openssl/comp.h>
#endif
#include <ctype.h>

int set_hex(char *in, unsigned char *out, int size);
#undef SIZE
#undef BSIZE
#undef PROG

#define SIZE    (512)
#define BSIZE   (8*1024)
#define PROG    enc_main

static void show_ciphers(const OBJ_NAME *name, void *bio_)
{
    BIO *bio = bio_;
    static int n;

    if (!islower((unsigned char)*name->name))
        return;

    BIO_printf(bio, "-%-25s", name->name);
    if (++n == 3) {
        BIO_printf(bio, "\n");
        n = 0;
    } else
        BIO_printf(bio, " ");
}

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    static const char magic[] = "Salted__";
    char mbuf[sizeof magic - 1];
    char *strbuf = NULL;
    unsigned char *buff = NULL, *bufsize = NULL;
    int bsize = BSIZE, verbose = 0;
    int ret = 1, inl;
    int nopad = 0;
    unsigned char key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];
    unsigned char salt[PKCS5_SALT_LEN];
    char *str = NULL, *passarg = NULL, *pass = NULL;
    char *hkey = NULL, *hiv = NULL, *hsalt = NULL;
    char *md = NULL;
    int enc = 1, printkey = 0, i, base64 = 0;
#ifdef ZLIB
    int do_zlib = 0;
    BIO *bzl = NULL;
#endif
    int debug = 0, olb64 = 0, nosalt = 0;
    const EVP_CIPHER *cipher = NULL, *c;
    EVP_CIPHER_CTX *ctx = NULL;
    char *inf = NULL, *outf = NULL;
    BIO *in = NULL, *out = NULL, *b64 = NULL, *benc = NULL, *rbio =
        NULL, *wbio = NULL;
#define PROG_NAME_SIZE  39
    char pname[PROG_NAME_SIZE + 1];
#ifndef OPENSSL_NO_ENGINE
    char *engine = NULL;
#endif
    const EVP_MD *dgst = NULL;
    int non_fips_allow = 0;

    apps_startup();

    if (bio_err == NULL)
        if ((bio_err = BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_err, stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    if (!load_config(bio_err, NULL))
        goto end;

    /* first check the program name */
    program_name(argv[0], pname, sizeof pname);
    if (strcmp(pname, "base64") == 0)
        base64 = 1;
#ifdef ZLIB
    if (strcmp(pname, "zlib") == 0)
        do_zlib = 1;
#endif

    cipher = EVP_get_cipherbyname(pname);
#ifdef ZLIB
    if (!do_zlib && !base64 && (cipher == NULL)
        && (strcmp(pname, "enc") != 0))
#else
    if (!base64 && (cipher == NULL) && (strcmp(pname, "enc") != 0))
#endif
    {
        BIO_printf(bio_err, "%s is an unknown cipher\n", pname);
        goto bad;
    }

    argc--;
    argv++;
    while (argc >= 1) {
        if (strcmp(*argv, "-e") == 0)
            enc = 1;
        else if (strcmp(*argv, "-in") == 0) {
            if (--argc < 1)
                goto bad;
            inf = *(++argv);
        } else if (strcmp(*argv, "-out") == 0) {
            if (--argc < 1)
                goto bad;
            outf = *(++argv);
        } else if (strcmp(*argv, "-pass") == 0) {
            if (--argc < 1)
                goto bad;
            passarg = *(++argv);
        }
#ifndef OPENSSL_NO_ENGINE
        else if (strcmp(*argv, "-engine") == 0) {
            if (--argc < 1)
                goto bad;
            engine = *(++argv);
        }
#endif
        else if (strcmp(*argv, "-d") == 0)
            enc = 0;
        else if (strcmp(*argv, "-p") == 0)
            printkey = 1;
        else if (strcmp(*argv, "-v") == 0)
            verbose = 1;
        else if (strcmp(*argv, "-nopad") == 0)
            nopad = 1;
        else if (strcmp(*argv, "-salt") == 0)
            nosalt = 0;
        else if (strcmp(*argv, "-nosalt") == 0)
            nosalt = 1;
        else if (strcmp(*argv, "-debug") == 0)
            debug = 1;
        else if (strcmp(*argv, "-P") == 0)
            printkey = 2;
        else if (strcmp(*argv, "-A") == 0)
            olb64 = 1;
        else if (strcmp(*argv, "-a") == 0)
            base64 = 1;
        else if (strcmp(*argv, "-base64") == 0)
            base64 = 1;
#ifdef ZLIB
        else if (strcmp(*argv, "-z") == 0)
            do_zlib = 1;
#endif
        else if (strcmp(*argv, "-bufsize") == 0) {
            if (--argc < 1)
                goto bad;
            bufsize = (unsigned char *)*(++argv);
        } else if (strcmp(*argv, "-k") == 0) {
            if (--argc < 1)
                goto bad;
            str = *(++argv);
        } else if (strcmp(*argv, "-kfile") == 0) {
            static char buf[128];
            FILE *infile;
            char *file;

            if (--argc < 1)
                goto bad;
            file = *(++argv);
            infile = fopen(file, "r");
            if (infile == NULL) {
                BIO_printf(bio_err, "unable to read key from '%s'\n", file);
                goto bad;
            }
            buf[0] = '\0';
            if (!fgets(buf, sizeof buf, infile)) {
                BIO_printf(bio_err, "unable to read key from '%s'\n", file);
                goto bad;
            }
            fclose(infile);
            i = strlen(buf);
            if ((i > 0) && ((buf[i - 1] == '\n') || (buf[i - 1] == '\r')))
                buf[--i] = '\0';
            if ((i > 0) && ((buf[i - 1] == '\n') || (buf[i - 1] == '\r')))
                buf[--i] = '\0';
            if (i < 1) {
                BIO_printf(bio_err, "zero length password\n");
                goto bad;
            }
            str = buf;
        } else if (strcmp(*argv, "-K") == 0) {
            if (--argc < 1)
                goto bad;
            hkey = *(++argv);
        } else if (strcmp(*argv, "-S") == 0) {
            if (--argc < 1)
                goto bad;
            hsalt = *(++argv);
        } else if (strcmp(*argv, "-iv") == 0) {
            if (--argc < 1)
                goto bad;
            hiv = *(++argv);
        } else if (strcmp(*argv, "-md") == 0) {
            if (--argc < 1)
                goto bad;
            md = *(++argv);
        } else if (strcmp(*argv, "-non-fips-allow") == 0)
            non_fips_allow = 1;
        else if ((argv[0][0] == '-') &&
                 ((c = EVP_get_cipherbyname(&(argv[0][1]))) != NULL)) {
            cipher = c;
        } else if (strcmp(*argv, "-none") == 0)
            cipher = NULL;
        else {
            BIO_printf(bio_err, "unknown option '%s'\n", *argv);
 bad:
            BIO_printf(bio_err, "options are\n");
            BIO_printf(bio_err, "%-14s input file\n", "-in <file>");
            BIO_printf(bio_err, "%-14s output file\n", "-out <file>");
            BIO_printf(bio_err, "%-14s pass phrase source\n", "-pass <arg>");
            BIO_printf(bio_err, "%-14s encrypt\n", "-e");
            BIO_printf(bio_err, "%-14s decrypt\n", "-d");
            BIO_printf(bio_err,
                       "%-14s base64 encode/decode, depending on encryption flag\n",
                       "-a/-base64");
            BIO_printf(bio_err, "%-14s passphrase is the next argument\n",
                       "-k");
            BIO_printf(bio_err,
                       "%-14s passphrase is the first line of the file argument\n",
                       "-kfile");
            BIO_printf(bio_err,
                       "%-14s the next argument is the md to use to create a key\n",
                       "-md");
            BIO_printf(bio_err,
                       "%-14s   from a passphrase.  One of md2, md5, sha or sha1\n",
                       "");
            BIO_printf(bio_err, "%-14s salt in hex is the next argument\n",
                       "-S");
            BIO_printf(bio_err, "%-14s key/iv in hex is the next argument\n",
                       "-K/-iv");
            BIO_printf(bio_err, "%-14s print the iv/key (then exit if -P)\n",
                       "-[pP]");
            BIO_printf(bio_err, "%-14s buffer size\n", "-bufsize <n>");
            BIO_printf(bio_err, "%-14s disable standard block padding\n",
                       "-nopad");
#ifndef OPENSSL_NO_ENGINE
            BIO_printf(bio_err,
                       "%-14s use engine e, possibly a hardware device.\n",
                       "-engine e");
#endif

            BIO_printf(bio_err, "Cipher Types\n");
            OBJ_NAME_do_all_sorted(OBJ_NAME_TYPE_CIPHER_METH,
                                   show_ciphers, bio_err);
            BIO_printf(bio_err, "\n");

            goto end;
        }
        argc--;
        argv++;
    }

#ifndef OPENSSL_NO_ENGINE
    setup_engine(bio_err, engine, 0);
#endif

    if (cipher && EVP_CIPHER_flags(cipher) & EVP_CIPH_FLAG_AEAD_CIPHER) {
        BIO_printf(bio_err,
                   "AEAD ciphers not supported by the enc utility\n");
        goto end;
    }

    if (cipher && (EVP_CIPHER_mode(cipher) == EVP_CIPH_XTS_MODE)) {
        BIO_printf(bio_err,
                   "Ciphers in XTS mode are not supported by the enc utility\n");
        goto end;
    }

    if (md && (dgst = EVP_get_digestbyname(md)) == NULL) {
        BIO_printf(bio_err, "%s is an unsupported message digest type\n", md);
        goto end;
    }

    if (dgst == NULL) {
        dgst = EVP_md5();
    }

    if (bufsize != NULL) {
        unsigned long n;

        for (n = 0; *bufsize; bufsize++) {
            i = *bufsize;
            if ((i <= '9') && (i >= '0'))
                n = n * 10 + i - '0';
            else if (i == 'k') {
                n *= 1024;
                bufsize++;
                break;
            }
        }
        if (*bufsize != '\0') {
            BIO_printf(bio_err, "invalid 'bufsize' specified.\n");
            goto end;
        }

        /* It must be large enough for a base64 encoded line */
        if (base64 && n < 80)
            n = 80;

        bsize = (int)n;
        if (verbose)
            BIO_printf(bio_err, "bufsize=%d\n", bsize);
    }

    strbuf = OPENSSL_malloc(SIZE);
    buff = (unsigned char *)OPENSSL_malloc(EVP_ENCODE_LENGTH(bsize));
    if ((buff == NULL) || (strbuf == NULL)) {
        BIO_printf(bio_err, "OPENSSL_malloc failure %ld\n",
                   (long)EVP_ENCODE_LENGTH(bsize));
        goto end;
    }

    in = BIO_new(BIO_s_file());
    out = BIO_new(BIO_s_file());
    if ((in == NULL) || (out == NULL)) {
        ERR_print_errors(bio_err);
        goto end;
    }
    if (debug) {
        BIO_set_callback(in, BIO_debug_callback);
        BIO_set_callback(out, BIO_debug_callback);
        BIO_set_callback_arg(in, (char *)bio_err);
        BIO_set_callback_arg(out, (char *)bio_err);
    }

    if (inf == NULL) {
#ifndef OPENSSL_NO_SETVBUF_IONBF
        if (bufsize != NULL)
            setvbuf(stdin, (char *)NULL, _IONBF, 0);
#endif                          /* ndef OPENSSL_NO_SETVBUF_IONBF */
        BIO_set_fp(in, stdin, BIO_NOCLOSE);
    } else {
        if (BIO_read_filename(in, inf) <= 0) {
            perror(inf);
            goto end;
        }
    }

    if (!str && passarg) {
        if (!app_passwd(bio_err, passarg, NULL, &pass, NULL)) {
            BIO_printf(bio_err, "Error getting password\n");
            goto end;
        }
        str = pass;
    }

    if ((str == NULL) && (cipher != NULL) && (hkey == NULL)) {
        for (;;) {
            char buf[200];

            BIO_snprintf(buf, sizeof buf, "enter %s %s password:",
                         OBJ_nid2ln(EVP_CIPHER_nid(cipher)),
                         (enc) ? "encryption" : "decryption");
            strbuf[0] = '\0';
            i = EVP_read_pw_string((char *)strbuf, SIZE, buf, enc);
            if (i == 0) {
                if (strbuf[0] == '\0') {
                    ret = 1;
                    goto end;
                }
                str = strbuf;
                break;
            }
            if (i < 0) {
                BIO_printf(bio_err, "bad password read\n");
                goto end;
            }
        }
    }

    if (outf == NULL) {
        BIO_set_fp(out, stdout, BIO_NOCLOSE);
#ifndef OPENSSL_NO_SETVBUF_IONBF
        if (bufsize != NULL)
            setvbuf(stdout, (char *)NULL, _IONBF, 0);
#endif                          /* ndef OPENSSL_NO_SETVBUF_IONBF */
#ifdef OPENSSL_SYS_VMS
        {
            BIO *tmpbio = BIO_new(BIO_f_linebuffer());
            out = BIO_push(tmpbio, out);
        }
#endif
    } else {
        if (BIO_write_filename(out, outf) <= 0) {
            perror(outf);
            goto end;
        }
    }

    rbio = in;
    wbio = out;

#ifdef ZLIB

    if (do_zlib) {
        if ((bzl = BIO_new(BIO_f_zlib())) == NULL)
            goto end;
        if (enc)
            wbio = BIO_push(bzl, wbio);
        else
            rbio = BIO_push(bzl, rbio);
    }
#endif

    if (base64) {
        if ((b64 = BIO_new(BIO_f_base64())) == NULL)
            goto end;
        if (debug) {
            BIO_set_callback(b64, BIO_debug_callback);
            BIO_set_callback_arg(b64, (char *)bio_err);
        }
        if (olb64)
            BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        if (enc)
            wbio = BIO_push(b64, wbio);
        else
            rbio = BIO_push(b64, rbio);
    }

    if (cipher != NULL) {
        /*
         * Note that str is NULL if a key was passed on the command line, so
         * we get no salt in that case. Is this a bug?
         */
        if (str != NULL) {
            /*
             * Salt handling: if encrypting generate a salt and write to
             * output BIO. If decrypting read salt from input BIO.
             */
            unsigned char *sptr;
            if (nosalt)
                sptr = NULL;
            else {
                if (enc) {
                    if (hsalt) {
                        if (!set_hex(hsalt, salt, sizeof salt)) {
                            BIO_printf(bio_err, "invalid hex salt value\n");
                            goto end;
                        }
                    } else if (RAND_bytes(salt, sizeof salt) <= 0)
                        goto end;
                    /*
                     * If -P option then don't bother writing
                     */
                    if ((printkey != 2)
                        && (BIO_write(wbio, magic,
                                      sizeof magic - 1) != sizeof magic - 1
                            || BIO_write(wbio,
                                         (char *)salt,
                                         sizeof salt) != sizeof salt)) {
                        BIO_printf(bio_err, "error writing output file\n");
                        goto end;
                    }
                } else if (BIO_read(rbio, mbuf, sizeof mbuf) != sizeof mbuf
                           || BIO_read(rbio,
                                       (unsigned char *)salt,
                                       sizeof salt) != sizeof salt) {
                    BIO_printf(bio_err, "error reading input file\n");
                    goto end;
                } else if (memcmp(mbuf, magic, sizeof magic - 1)) {
                    BIO_printf(bio_err, "bad magic number\n");
                    goto end;
                }

                sptr = salt;
            }

            EVP_BytesToKey(cipher, dgst, sptr,
                           (unsigned char *)str, strlen(str), 1, key, iv);
            /*
             * zero the complete buffer or the string passed from the command
             * line bug picked up by Larry J. Hughes Jr. <hughes@indiana.edu>
             */
            if (str == strbuf)
                OPENSSL_cleanse(str, SIZE);
            else
                OPENSSL_cleanse(str, strlen(str));
        }
        if (hiv != NULL) {
            int siz = EVP_CIPHER_iv_length(cipher);
            if (siz == 0) {
                BIO_printf(bio_err, "warning: iv not use by this cipher\n");
            } else if (!set_hex(hiv, iv, sizeof iv)) {
                BIO_printf(bio_err, "invalid hex iv value\n");
                goto end;
            }
        }
        if ((hiv == NULL) && (str == NULL)
            && EVP_CIPHER_iv_length(cipher) != 0) {
            /*
             * No IV was explicitly set and no IV was generated during
             * EVP_BytesToKey. Hence the IV is undefined, making correct
             * decryption impossible.
             */
            BIO_printf(bio_err, "iv undefined\n");
            goto end;
        }
        if ((hkey != NULL) && !set_hex(hkey, key, EVP_CIPHER_key_length(cipher))) {
            BIO_printf(bio_err, "invalid hex key value\n");
            goto end;
        }

        if ((benc = BIO_new(BIO_f_cipher())) == NULL)
            goto end;

        /*
         * Since we may be changing parameters work on the encryption context
         * rather than calling BIO_set_cipher().
         */

        BIO_get_cipher_ctx(benc, &ctx);

        if (non_fips_allow)
            EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_NON_FIPS_ALLOW);

        if (!EVP_CipherInit_ex(ctx, cipher, NULL, NULL, NULL, enc)) {
            BIO_printf(bio_err, "Error setting cipher %s\n",
                       EVP_CIPHER_name(cipher));
            ERR_print_errors(bio_err);
            goto end;
        }

        if (nopad)
            EVP_CIPHER_CTX_set_padding(ctx, 0);

        if (!EVP_CipherInit_ex(ctx, NULL, NULL, key, iv, enc)) {
            BIO_printf(bio_err, "Error setting cipher %s\n",
                       EVP_CIPHER_name(cipher));
            ERR_print_errors(bio_err);
            goto end;
        }

        if (debug) {
            BIO_set_callback(benc, BIO_debug_callback);
            BIO_set_callback_arg(benc, (char *)bio_err);
        }

        if (printkey) {
            if (!nosalt) {
                printf("salt=");
                for (i = 0; i < (int)sizeof(salt); i++)
                    printf("%02X", salt[i]);
                printf("\n");
            }
            if (cipher->key_len > 0) {
                printf("key=");
                for (i = 0; i < cipher->key_len; i++)
                    printf("%02X", key[i]);
                printf("\n");
            }
            if (cipher->iv_len > 0) {
                printf("iv =");
                for (i = 0; i < cipher->iv_len; i++)
                    printf("%02X", iv[i]);
                printf("\n");
            }
            if (printkey == 2) {
                ret = 0;
                goto end;
            }
        }
    }

    /* Only encrypt/decrypt as we write the file */
    if (benc != NULL)
        wbio = BIO_push(benc, wbio);

    for (;;) {
        inl = BIO_read(rbio, (char *)buff, bsize);
        if (inl <= 0)
            break;
        if (BIO_write(wbio, (char *)buff, inl) != inl) {
            BIO_printf(bio_err, "error writing output file\n");
            goto end;
        }
    }
    if (!BIO_flush(wbio)) {
        BIO_printf(bio_err, "bad decrypt\n");
        goto end;
    }

    ret = 0;
    if (verbose) {
        BIO_printf(bio_err, "bytes read   :%8ld\n", BIO_number_read(in));
        BIO_printf(bio_err, "bytes written:%8ld\n", BIO_number_written(out));
    }
 end:
    ERR_print_errors(bio_err);
    if (strbuf != NULL)
        OPENSSL_free(strbuf);
    if (buff != NULL)
        OPENSSL_free(buff);
    if (in != NULL)
        BIO_free(in);
    if (out != NULL)
        BIO_free_all(out);
    if (benc != NULL)
        BIO_free(benc);
    if (b64 != NULL)
        BIO_free(b64);
#ifdef ZLIB
    if (bzl != NULL)
        BIO_free(bzl);
#endif
    if (pass)
        OPENSSL_free(pass);
    apps_shutdown();
    OPENSSL_EXIT(ret);
}

int set_hex(char *in, unsigned char *out, int size)
{
    int i, n;
    unsigned char j;

    n = strlen(in);
    if (n > (size * 2)) {
        BIO_printf(bio_err, "hex string is too long\n");
        return (0);
    }
    memset(out, 0, size);
    for (i = 0; i < n; i++) {
        j = (unsigned char)*in;
        *(in++) = '\0';
        if (j == 0)
            break;
        if ((j >= '0') && (j <= '9'))
            j -= '0';
        else if ((j >= 'A') && (j <= 'F'))
            j = j - 'A' + 10;
        else if ((j >= 'a') && (j <= 'f'))
            j = j - 'a' + 10;
        else {
            BIO_printf(bio_err, "non-hex digit\n");
            return (0);
        }
        if (i & 1)
            out[i / 2] |= j;
        else
            out[i / 2] = (j << 4);
    }
    return (1);
}
