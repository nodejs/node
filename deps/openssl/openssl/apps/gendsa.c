/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/opensslconf.h>
#ifdef OPENSSL_NO_DSA
NON_EMPTY_TRANSLATION_UNIT
#else

# include <stdio.h>
# include <string.h>
# include <sys/types.h>
# include <sys/stat.h>
# include "apps.h"
# include <openssl/bio.h>
# include <openssl/err.h>
# include <openssl/bn.h>
# include <openssl/dsa.h>
# include <openssl/x509.h>
# include <openssl/pem.h>

typedef enum OPTION_choice {
    OPT_ERR = -1, OPT_EOF = 0, OPT_HELP,
    OPT_OUT, OPT_PASSOUT, OPT_ENGINE, OPT_RAND, OPT_CIPHER
} OPTION_CHOICE;

OPTIONS gendsa_options[] = {
    {OPT_HELP_STR, 1, '-', "Usage: %s [args] dsaparam-file\n"},
    {OPT_HELP_STR, 1, '-', "Valid options are:\n"},
    {"help", OPT_HELP, '-', "Display this summary"},
    {"out", OPT_OUT, '>', "Output the key to the specified file"},
    {"passout", OPT_PASSOUT, 's', "Output file pass phrase source"},
    {"rand", OPT_RAND, 's',
     "Load the file(s) into the random number generator"},
    {"", OPT_CIPHER, '-', "Encrypt the output with any supported cipher"},
# ifndef OPENSSL_NO_ENGINE
    {"engine", OPT_ENGINE, 's', "Use engine, possibly a hardware device"},
# endif
    {NULL}
};

int gendsa_main(int argc, char **argv)
{
    ENGINE *e = NULL;
    BIO *out = NULL, *in = NULL;
    DSA *dsa = NULL;
    const EVP_CIPHER *enc = NULL;
    char *inrand = NULL, *dsaparams = NULL;
    char *outfile = NULL, *passoutarg = NULL, *passout = NULL, *prog;
    OPTION_CHOICE o;
    int ret = 1, private = 0;
    const BIGNUM *p = NULL;

    prog = opt_init(argc, argv, gendsa_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_EOF:
        case OPT_ERR:
 opthelp:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            goto end;
        case OPT_HELP:
            ret = 0;
            opt_help(gendsa_options);
            goto end;
        case OPT_OUT:
            outfile = opt_arg();
            break;
        case OPT_PASSOUT:
            passoutarg = opt_arg();
            break;
        case OPT_ENGINE:
            e = setup_engine(opt_arg(), 0);
            break;
        case OPT_RAND:
            inrand = opt_arg();
            break;
        case OPT_CIPHER:
            if (!opt_cipher(opt_unknown(), &enc))
                goto end;
            break;
        }
    }
    argc = opt_num_rest();
    argv = opt_rest();
    private = 1;

    if (argc != 1)
        goto opthelp;
    dsaparams = *argv;

    if (!app_passwd(NULL, passoutarg, NULL, &passout)) {
        BIO_printf(bio_err, "Error getting password\n");
        goto end;
    }

    in = bio_open_default(dsaparams, 'r', FORMAT_PEM);
    if (in == NULL)
        goto end2;

    if ((dsa = PEM_read_bio_DSAparams(in, NULL, NULL, NULL)) == NULL) {
        BIO_printf(bio_err, "unable to load DSA parameter file\n");
        goto end;
    }
    BIO_free(in);
    in = NULL;

    out = bio_open_owner(outfile, FORMAT_PEM, private);
    if (out == NULL)
        goto end2;

    if (!app_RAND_load_file(NULL, 1) && inrand == NULL) {
        BIO_printf(bio_err,
                   "warning, not much extra random data, consider using the -rand option\n");
    }
    if (inrand != NULL)
        BIO_printf(bio_err, "%ld semi-random bytes loaded\n",
                   app_RAND_load_files(inrand));

    DSA_get0_pqg(dsa, &p, NULL, NULL);
    BIO_printf(bio_err, "Generating DSA key, %d bits\n", BN_num_bits(p));
    if (!DSA_generate_key(dsa))
        goto end;

    app_RAND_write_file(NULL);

    assert(private);
    if (!PEM_write_bio_DSAPrivateKey(out, dsa, enc, NULL, 0, NULL, passout))
        goto end;
    ret = 0;
 end:
    if (ret != 0)
        ERR_print_errors(bio_err);
 end2:
    BIO_free(in);
    BIO_free_all(out);
    DSA_free(dsa);
    release_engine(e);
    OPENSSL_free(passout);
    return (ret);
}
#endif
