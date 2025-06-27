/*
 * Copyright 1998-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "apps.h"
#include "progs.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>

typedef enum OPTION_choice {
    OPT_COMMON,
    OPT_OUT, OPT_ENGINE, OPT_BASE64, OPT_HEX,
    OPT_R_ENUM, OPT_PROV_ENUM
} OPTION_CHOICE;

const OPTIONS rand_options[] = {
    {OPT_HELP_STR, 1, '-', "Usage: %s [options] num\n"},

    OPT_SECTION("General"),
    {"help", OPT_HELP, '-', "Display this summary"},
#ifndef OPENSSL_NO_ENGINE
    {"engine", OPT_ENGINE, 's', "Use engine, possibly a hardware device"},
#endif

    OPT_SECTION("Output"),
    {"out", OPT_OUT, '>', "Output file"},
    {"base64", OPT_BASE64, '-', "Base64 encode output"},
    {"hex", OPT_HEX, '-', "Hex encode output"},

    OPT_R_OPTIONS,
    OPT_PROV_OPTIONS,

    OPT_PARAMETERS(),
    {"num", 0, 0, "Number of bytes to generate"},
    {NULL}
};

int rand_main(int argc, char **argv)
{
    ENGINE *e = NULL;
    BIO *out = NULL;
    char *outfile = NULL, *prog;
    OPTION_CHOICE o;
    int format = FORMAT_BINARY, i, num = -1, r, ret = 1;

    prog = opt_init(argc, argv, rand_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_EOF:
        case OPT_ERR:
 opthelp:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            goto end;
        case OPT_HELP:
            opt_help(rand_options);
            ret = 0;
            goto end;
        case OPT_OUT:
            outfile = opt_arg();
            break;
        case OPT_ENGINE:
            e = setup_engine(opt_arg(), 0);
            break;
        case OPT_R_CASES:
            if (!opt_rand(o))
                goto end;
            break;
        case OPT_BASE64:
            format = FORMAT_BASE64;
            break;
        case OPT_HEX:
            format = FORMAT_TEXT;
            break;
        case OPT_PROV_CASES:
            if (!opt_provider(o))
                goto end;
            break;
        }
    }

    /* Optional argument is number of bytes to generate. */
    argc = opt_num_rest();
    argv = opt_rest();
    if (argc == 1) {
        if (!opt_int(argv[0], &num) || num <= 0)
            goto opthelp;
    } else if (argc != 0) {
        goto opthelp;
    }

    if (!app_RAND_load())
        goto end;

    out = bio_open_default(outfile, 'w', format);
    if (out == NULL)
        goto end;

    if (format == FORMAT_BASE64) {
        BIO *b64 = BIO_new(BIO_f_base64());
        if (b64 == NULL)
            goto end;
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
            goto end;
        if (format != FORMAT_TEXT) {
            if (BIO_write(out, buf, chunk) != chunk)
                goto end;
        } else {
            for (i = 0; i < chunk; i++)
                if (BIO_printf(out, "%02x", buf[i]) != 2)
                    goto end;
        }
        num -= chunk;
    }
    if (format == FORMAT_TEXT)
        BIO_puts(out, "\n");
    if (BIO_flush(out) <= 0)
        goto end;

    ret = 0;

 end:
    if (ret != 0)
        ERR_print_errors(bio_err);
    release_engine(e);
    BIO_free_all(out);
    return ret;
}
