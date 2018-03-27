/* apps/rand.c */
/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#undef PROG
#define PROG rand_main

/*-
 * -out file         - write to file
 * -rand file:file   - PRNG seed files
 * -base64           - base64 encode output
 * -hex              - hex encode output
 * num               - write 'num' bytes
 */

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    int i, r, ret = 1;
    int badopt;
    char *outfile = NULL;
    char *inrand = NULL;
    int base64 = 0;
    int hex = 0;
    BIO *out = NULL;
    int num = -1;
    ENGINE *e = NULL;
    char *engine = NULL;

    apps_startup();

    if (bio_err == NULL)
        if ((bio_err = BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_err, stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    if (!load_config(bio_err, NULL))
        goto err;

    badopt = 0;
    i = 0;
    while (!badopt && argv[++i] != NULL) {
        if (strcmp(argv[i], "-out") == 0) {
            if ((argv[i + 1] != NULL) && (outfile == NULL))
                outfile = argv[++i];
            else
                badopt = 1;
        }
#ifndef OPENSSL_NO_ENGINE
        else if (strcmp(argv[i], "-engine") == 0) {
            if ((argv[i + 1] != NULL) && (engine == NULL))
                engine = argv[++i];
            else
                badopt = 1;
        }
#endif
        else if (strcmp(argv[i], "-rand") == 0) {
            if ((argv[i + 1] != NULL) && (inrand == NULL))
                inrand = argv[++i];
            else
                badopt = 1;
        } else if (strcmp(argv[i], "-base64") == 0) {
            if (!base64)
                base64 = 1;
            else
                badopt = 1;
        } else if (strcmp(argv[i], "-hex") == 0) {
            if (!hex)
                hex = 1;
            else
                badopt = 1;
        } else if (isdigit((unsigned char)argv[i][0])) {
            if (num < 0) {
                r = sscanf(argv[i], "%d", &num);
                if (r == 0 || num < 0)
                    badopt = 1;
            } else
                badopt = 1;
        } else
            badopt = 1;
    }

    if (hex && base64)
        badopt = 1;

    if (num < 0)
        badopt = 1;

    if (badopt) {
        BIO_printf(bio_err, "Usage: rand [options] num\n");
        BIO_printf(bio_err, "where options are\n");
        BIO_printf(bio_err, "-out file             - write to file\n");
#ifndef OPENSSL_NO_ENGINE
        BIO_printf(bio_err,
                   "-engine e             - use engine e, possibly a hardware device.\n");
#endif
        BIO_printf(bio_err, "-rand file%cfile%c... - seed PRNG from files\n",
                   LIST_SEPARATOR_CHAR, LIST_SEPARATOR_CHAR);
        BIO_printf(bio_err, "-base64               - base64 encode output\n");
        BIO_printf(bio_err, "-hex                  - hex encode output\n");
        goto err;
    }
    e = setup_engine(bio_err, engine, 0);

    app_RAND_load_file(NULL, bio_err, (inrand != NULL));
    if (inrand != NULL)
        BIO_printf(bio_err, "%ld semi-random bytes loaded\n",
                   app_RAND_load_files(inrand));

    out = BIO_new(BIO_s_file());
    if (out == NULL)
        goto err;
    if (outfile != NULL)
        r = BIO_write_filename(out, outfile);
    else {
        r = BIO_set_fp(out, stdout, BIO_NOCLOSE | BIO_FP_TEXT);
#ifdef OPENSSL_SYS_VMS
        {
            BIO *tmpbio = BIO_new(BIO_f_linebuffer());
            out = BIO_push(tmpbio, out);
        }
#endif
    }
    if (r <= 0)
        goto err;

    if (base64) {
        BIO *b64 = BIO_new(BIO_f_base64());
        if (b64 == NULL)
            goto err;
        out = BIO_push(b64, out);
    }

    while (num > 0) {
        unsigned char buf[4096];
        int chunk;

        chunk = num;
        if (chunk > (int)sizeof(buf))
            chunk = sizeof(buf);
        r = RAND_bytes(buf, chunk);
        if (r <= 0)
            goto err;
        if (!hex)
            BIO_write(out, buf, chunk);
        else {
            for (i = 0; i < chunk; i++)
                BIO_printf(out, "%02x", buf[i]);
        }
        num -= chunk;
    }
    if (hex)
        BIO_puts(out, "\n");
    (void)BIO_flush(out);

    app_RAND_write_file(NULL, bio_err);
    ret = 0;

 err:
    ERR_print_errors(bio_err);
    release_engine(e);
    if (out)
        BIO_free_all(out);
    apps_shutdown();
    OPENSSL_EXIT(ret);
}
