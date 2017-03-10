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
# include <stdlib.h>
# include <time.h>
# include <string.h>
# include "apps.h"
# include <openssl/bio.h>
# include <openssl/err.h>
# include <openssl/bn.h>
# include <openssl/dsa.h>
# include <openssl/x509.h>
# include <openssl/pem.h>

# ifdef GENCB_TEST

static int stop_keygen_flag = 0;

static void timebomb_sigalarm(int foo)
{
    stop_keygen_flag = 1;
}

# endif

static int dsa_cb(int p, int n, BN_GENCB *cb);

typedef enum OPTION_choice {
    OPT_ERR = -1, OPT_EOF = 0, OPT_HELP,
    OPT_INFORM, OPT_OUTFORM, OPT_IN, OPT_OUT, OPT_TEXT, OPT_C,
    OPT_NOOUT, OPT_GENKEY, OPT_RAND, OPT_ENGINE,
    OPT_TIMEBOMB
} OPTION_CHOICE;

OPTIONS dsaparam_options[] = {
    {"help", OPT_HELP, '-', "Display this summary"},
    {"inform", OPT_INFORM, 'F', "Input format - DER or PEM"},
    {"in", OPT_IN, '<', "Input file"},
    {"outform", OPT_OUTFORM, 'F', "Output format - DER or PEM"},
    {"out", OPT_OUT, '>', "Output file"},
    {"text", OPT_TEXT, '-', "Print as text"},
    {"C", OPT_C, '-', "Output C code"},
    {"noout", OPT_NOOUT, '-', "No output"},
    {"genkey", OPT_GENKEY, '-', "Generate a DSA key"},
    {"rand", OPT_RAND, 's', "Files to use for random number input"},
# ifdef GENCB_TEST
    {"timebomb", OPT_TIMEBOMB, 'p', "Interrupt keygen after 'pnum' seconds"},
# endif
# ifndef OPENSSL_NO_ENGINE
    {"engine", OPT_ENGINE, 's', "Use engine e, possibly a hardware device"},
# endif
    {NULL}
};

int dsaparam_main(int argc, char **argv)
{
    ENGINE *e = NULL;
    DSA *dsa = NULL;
    BIO *in = NULL, *out = NULL;
    BN_GENCB *cb = NULL;
    int numbits = -1, num = 0, genkey = 0, need_rand = 0;
    int informat = FORMAT_PEM, outformat = FORMAT_PEM, noout = 0, C = 0;
    int ret = 1, i, text = 0, private = 0;
# ifdef GENCB_TEST
    int timebomb = 0;
# endif
    char *infile = NULL, *outfile = NULL, *prog, *inrand = NULL;
    OPTION_CHOICE o;

    prog = opt_init(argc, argv, dsaparam_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_EOF:
        case OPT_ERR:
 opthelp:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            goto end;
        case OPT_HELP:
            opt_help(dsaparam_options);
            ret = 0;
            goto end;
        case OPT_INFORM:
            if (!opt_format(opt_arg(), OPT_FMT_PEMDER, &informat))
                goto opthelp;
            break;
        case OPT_IN:
            infile = opt_arg();
            break;
        case OPT_OUTFORM:
            if (!opt_format(opt_arg(), OPT_FMT_PEMDER, &outformat))
                goto opthelp;
            break;
        case OPT_OUT:
            outfile = opt_arg();
            break;
        case OPT_ENGINE:
            e = setup_engine(opt_arg(), 0);
            break;
        case OPT_TIMEBOMB:
# ifdef GENCB_TEST
            timebomb = atoi(opt_arg());
            break;
# endif
        case OPT_TEXT:
            text = 1;
            break;
        case OPT_C:
            C = 1;
            break;
        case OPT_GENKEY:
            genkey = need_rand = 1;
            break;
        case OPT_RAND:
            inrand = opt_arg();
            need_rand = 1;
            break;
        case OPT_NOOUT:
            noout = 1;
            break;
        }
    }
    argc = opt_num_rest();
    argv = opt_rest();

    if (argc == 1) {
        if (!opt_int(argv[0], &num) || num < 0)
            goto end;
        /* generate a key */
        numbits = num;
        need_rand = 1;
    }
    private = genkey ? 1 : 0;

    in = bio_open_default(infile, 'r', informat);
    if (in == NULL)
        goto end;
    out = bio_open_owner(outfile, outformat, private);
    if (out == NULL)
        goto end;

    if (need_rand) {
        app_RAND_load_file(NULL, (inrand != NULL));
        if (inrand != NULL)
            BIO_printf(bio_err, "%ld semi-random bytes loaded\n",
                       app_RAND_load_files(inrand));
    }

    if (numbits > 0) {
        cb = BN_GENCB_new();
        if (cb == NULL) {
            BIO_printf(bio_err, "Error allocating BN_GENCB object\n");
            goto end;
        }
        BN_GENCB_set(cb, dsa_cb, bio_err);
        assert(need_rand);
        dsa = DSA_new();
        if (dsa == NULL) {
            BIO_printf(bio_err, "Error allocating DSA object\n");
            goto end;
        }
        BIO_printf(bio_err, "Generating DSA parameters, %d bit long prime\n",
                   num);
        BIO_printf(bio_err, "This could take some time\n");
# ifdef GENCB_TEST
        if (timebomb > 0) {
            struct sigaction act;
            act.sa_handler = timebomb_sigalarm;
            act.sa_flags = 0;
            BIO_printf(bio_err,
                       "(though I'll stop it if not done within %d secs)\n",
                       timebomb);
            if (sigaction(SIGALRM, &act, NULL) != 0) {
                BIO_printf(bio_err, "Error, couldn't set SIGALRM handler\n");
                goto end;
            }
            alarm(timebomb);
        }
# endif
        if (!DSA_generate_parameters_ex(dsa, num, NULL, 0, NULL, NULL, cb)) {
# ifdef GENCB_TEST
            if (stop_keygen_flag) {
                BIO_printf(bio_err, "DSA key generation time-stopped\n");
                /* This is an asked-for behaviour! */
                ret = 0;
                goto end;
            }
# endif
            ERR_print_errors(bio_err);
            BIO_printf(bio_err, "Error, DSA key generation failed\n");
            goto end;
        }
    } else if (informat == FORMAT_ASN1)
        dsa = d2i_DSAparams_bio(in, NULL);
    else
        dsa = PEM_read_bio_DSAparams(in, NULL, NULL, NULL);
    if (dsa == NULL) {
        BIO_printf(bio_err, "unable to load DSA parameters\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    if (text) {
        DSAparams_print(out, dsa);
    }

    if (C) {
        const BIGNUM *p = NULL, *q = NULL, *g = NULL;
        unsigned char *data;
        int len, bits_p;

        DSA_get0_pqg(dsa, &p, &q, &g);
        len = BN_num_bytes(p);
        bits_p = BN_num_bits(p);

        data = app_malloc(len + 20, "BN space");

        BIO_printf(bio_out, "DSA *get_dsa%d()\n{\n", bits_p);
        print_bignum_var(bio_out, p, "dsap", len, data);
        print_bignum_var(bio_out, q, "dsaq", len, data);
        print_bignum_var(bio_out, g, "dsag", len, data);
        BIO_printf(bio_out, "    DSA *dsa = DSA_new();\n"
                            "\n");
        BIO_printf(bio_out, "    if (dsa == NULL)\n"
                            "        return NULL;\n");
        BIO_printf(bio_out, "    dsa->p = BN_bin2bn(dsap_%d, sizeof (dsap_%d), NULL);\n",
               bits_p, bits_p);
        BIO_printf(bio_out, "    dsa->q = BN_bin2bn(dsaq_%d, sizeof (dsaq_%d), NULL);\n",
               bits_p, bits_p);
        BIO_printf(bio_out, "    dsa->g = BN_bin2bn(dsag_%d, sizeof (dsag_%d), NULL);\n",
               bits_p, bits_p);
        BIO_printf(bio_out, "    if (!dsa->p || !dsa->q || !dsa->g) {\n"
                            "        DSA_free(dsa);\n"
                            "        return NULL;\n"
                            "    }\n"
                            "    return(dsa);\n}\n");
        OPENSSL_free(data);
    }

    if (!noout) {
        if (outformat == FORMAT_ASN1)
            i = i2d_DSAparams_bio(out, dsa);
        else
            i = PEM_write_bio_DSAparams(out, dsa);
        if (!i) {
            BIO_printf(bio_err, "unable to write DSA parameters\n");
            ERR_print_errors(bio_err);
            goto end;
        }
    }
    if (genkey) {
        DSA *dsakey;

        assert(need_rand);
        if ((dsakey = DSAparams_dup(dsa)) == NULL)
            goto end;
        if (!DSA_generate_key(dsakey)) {
            ERR_print_errors(bio_err);
            DSA_free(dsakey);
            goto end;
        }
        assert(private);
        if (outformat == FORMAT_ASN1)
            i = i2d_DSAPrivateKey_bio(out, dsakey);
        else
            i = PEM_write_bio_DSAPrivateKey(out, dsakey, NULL, NULL, 0, NULL,
                                            NULL);
        DSA_free(dsakey);
    }
    if (need_rand)
        app_RAND_write_file(NULL);
    ret = 0;
 end:
    BN_GENCB_free(cb);
    BIO_free(in);
    BIO_free_all(out);
    DSA_free(dsa);
    release_engine(e);
    return (ret);
}

static int dsa_cb(int p, int n, BN_GENCB *cb)
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
    BIO_write(BN_GENCB_get_arg(cb), &c, 1);
    (void)BIO_flush(BN_GENCB_get_arg(cb));
# ifdef GENCB_TEST
    if (stop_keygen_flag)
        return 0;
# endif
    return 1;
}
#endif
