/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apps.h"
#include "progs.h"
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/hpke.h>

#include <openssl/objects.h>
#include <openssl/x509.h>

#ifndef OPENSSL_NO_ECH

#define OSSL_ECH_KEYGEN_MODE 0 /* default: generate a key pair/ECHConfig */
#define OSSL_ECH_SELPRINT_MODE 1 /* we can print/down-select ECHConfigList */
#define OSSL_ECH_MAXINFILES 5 /* we'll only take this many inputs */

typedef enum OPTION_choice {
    /* standard openssl options */
    OPT_ERR = -1,
    OPT_EOF = 0,
    OPT_HELP,
    OPT_VERBOSE,
    OPT_TEXT,
    OPT_OUT,
    OPT_IN,
    /* ECHConfig specifics */
    OPT_PUBLICNAME,
    OPT_ECHVERSION,
    OPT_MAXNAMELENGTH,
    OPT_HPKESUITE,
    OPT_SELECT
} OPTION_CHOICE;

const OPTIONS ech_options[] = {
    OPT_SECTION("General options"),
    { "help", OPT_HELP, '-', "Display this summary" },
    { "verbose", OPT_VERBOSE, '-', "Provide additional output" },
    { "text", OPT_TEXT, '-', "Provide human-readable output" },
    OPT_SECTION("Key generation"),
    { "out", OPT_OUT, '>',
        "Private key and/or ECHConfig [default: echconfig.pem]" },
    { "public_name", OPT_PUBLICNAME, 's', "public_name value" },
    { "max_name_len", OPT_MAXNAMELENGTH, 'n',
        "Maximum host name length value [default: 0]" },
    { "suite", OPT_HPKESUITE, 's', "HPKE ciphersuite: e.g. \"0x20,1,3\"" },
    { "ech_version", OPT_ECHVERSION, 'n',
        "ECHConfig version [default: 0xff0d (13)]" },
    OPT_SECTION("ECH PEM file downselect/display"),
    { "in", OPT_IN, '<', "An ECH PEM file" },
    { "select", OPT_SELECT, 'n', "Downselect to the numbered ECH config" },
    { NULL }
};

/**
 * @brief map version string like 0xff01 or 65291 to uint16_t
 * @param arg is the version string, from command line
 * @return is the uint16_t value (with zero for error cases)
 */
static uint16_t verstr2us(char *arg)
{
    long lv = strtol(arg, NULL, 0);
    uint16_t rv = 0;

    if (lv < 0xffff && lv > 0)
        rv = (uint16_t)lv;
    return rv;
}

int ech_main(int argc, char **argv)
{
    char *prog = NULL;
    OPTION_CHOICE o;
    int i, rv = 1, verbose = 0, text = 0, outsupp = 0;
    int select = OSSL_ECHSTORE_ALL, numinfiles = 0;
    char *outfile = NULL, *infile = NULL;
    char *infiles[OSSL_ECH_MAXINFILES] = { NULL };
    char *public_name = NULL, *suitestr = NULL;
    uint16_t ech_version = OSSL_ECH_CURRENT_VERSION;
    uint8_t max_name_length = 0;
    OSSL_HPKE_SUITE hpke_suite = OSSL_HPKE_SUITE_DEFAULT;
    int mode = OSSL_ECH_KEYGEN_MODE; /* key generation */
    OSSL_ECHSTORE *es = NULL;
    BIO *ecf = NULL;

    prog = opt_init(argc, argv, ech_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_EOF:
        case OPT_ERR:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            goto end;
        case OPT_HELP:
            opt_help(ech_options);
            rv = 0;
            goto end;
        case OPT_VERBOSE:
            verbose = 1;
            break;
        case OPT_TEXT:
            text = 1;
            break;
        case OPT_SELECT:
            mode = OSSL_ECH_SELPRINT_MODE;
            select = strtol(opt_arg(), NULL, 10);
            break;
        case OPT_OUT:
            outfile = opt_arg();
            outsupp = 1;
            break;
        case OPT_IN:
            mode = OSSL_ECH_SELPRINT_MODE;
            infile = opt_arg();
            if (numinfiles >= OSSL_ECH_MAXINFILES) {
                BIO_printf(bio_err, "too many input files, only %d allowed\n",
                    OSSL_ECH_MAXINFILES);
                goto opthelp;
            }
            infiles[numinfiles] = infile;
            numinfiles++;
            break;
        case OPT_PUBLICNAME:
            public_name = opt_arg();
            break;
        case OPT_ECHVERSION:
            ech_version = verstr2us(opt_arg());
            break;
        case OPT_MAXNAMELENGTH: {
            long tmp = strtol(opt_arg(), NULL, 10);

            if (tmp < 0 || tmp > OSSL_ECH_MAX_MAXNAMELEN) {
                BIO_printf(bio_err,
                    "max name length out of range [0,%d] (%ld)\n",
                    OSSL_ECH_MAX_MAXNAMELEN, tmp);
                goto opthelp;
            } else {
                max_name_length = (uint8_t)tmp;
            }
        } break;
        case OPT_HPKESUITE:
            suitestr = opt_arg();
            break;
        }
    }
    argc = opt_num_rest();
    argv = opt_rest();
    if (argc != 0) {
        BIO_printf(bio_err, "%s: Unknown parameter %s\n", prog, argv[0]);
        goto opthelp;
    }
    /* Check ECH-specific inputs */
    switch (ech_version) {
    case OSSL_ECH_RFC9849_VERSION: /* fall through */
    case 13:
        ech_version = OSSL_ECH_RFC9849_VERSION;
        break;
    default:
        BIO_printf(bio_err, "Un-supported version (0x%04x)\n", ech_version);
        goto end;
    }
    if (suitestr != NULL) {
        if (OSSL_HPKE_str2suite(suitestr, &hpke_suite) != 1) {
            BIO_printf(bio_err, "Bad OSSL_HPKE_SUITE (%s)\n", suitestr);
            ERR_print_errors(bio_err);
            goto end;
        }
    }
    /* Set default if needed */
    if (outfile == NULL)
        outfile = "echconfig.pem";
    es = OSSL_ECHSTORE_new(NULL, NULL);
    if (es == NULL)
        goto end;
    if (mode == OSSL_ECH_KEYGEN_MODE) {
        if (public_name == NULL) {
            BIO_printf(bio_err, "public_name required\n");
            goto end;
        }
        if (verbose)
            BIO_printf(bio_err, "Calling OSSL_ECHSTORE_new_config\n");
        if ((ecf = bio_open_owner(outfile, FORMAT_PEM, 1)) == NULL
            || OSSL_ECHSTORE_new_config(es, ech_version, max_name_length,
                   public_name, hpke_suite)
                != 1
            || OSSL_ECHSTORE_write_pem(es, 0, ecf) != 1) {
            BIO_printf(bio_err, "OSSL_ECHSTORE_new_config error\n");
            goto end;
        }
        if (verbose)
            BIO_printf(bio_err, "OSSL_ECHSTORE_new_config success\n");
        rv = 0;
    }
    if (mode == OSSL_ECH_SELPRINT_MODE) {
        if (numinfiles == 0)
            goto opthelp;
        for (i = 0; i != numinfiles; i++) {
            if ((ecf = BIO_new_file(infiles[i], "r")) == NULL
                || OSSL_ECHSTORE_read_pem(es, ecf, OSSL_ECH_FOR_RETRY) != 1) {
                BIO_printf(bio_err, "OSSL_ECHSTORE_read_pem error: %s\n",
                    infiles[i]);
                goto end;
            }
            BIO_free(ecf);
            ecf = NULL;
        }
        if (verbose)
            BIO_printf(bio_err, "Success reading %d files\n", numinfiles);
        if (outsupp == 1) {
            /* write result to that, with downselection if required */
            if (verbose)
                BIO_printf(bio_err, "Will write to %s\n", outfile);
            if (verbose && select != OSSL_ECHSTORE_ALL)
                BIO_printf(bio_err, "Selected entry: %d\n", select);
            if ((ecf = BIO_new_file(outfile, "w")) == NULL
                || OSSL_ECHSTORE_write_pem(es, select, ecf) != 1) {
                BIO_printf(bio_err, "OSSL_ECHSTORE_write_pem error: %s\n",
                    outfile);
                goto end;
            }
            if (verbose)
                BIO_printf(bio_err, "Success writing to %s\n", outfile);
        }
        rv = 0;
    }
    if (text) {
        int oi_ind, oi_cnt = 0;

        if (OSSL_ECHSTORE_num_entries(es, &oi_cnt) != 1)
            goto end;
        if (verbose)
            BIO_printf(bio_err, "Printing %d ECHConfig values\n", oi_cnt);
        for (oi_ind = 0; oi_ind != oi_cnt; oi_ind++) {
            time_t secs = 0;
            char *pn = NULL, *ec = NULL;
            int has_priv, for_retry;

            if (OSSL_ECHSTORE_get1_info(es, oi_ind, &secs, &pn, &ec,
                    &has_priv, &for_retry)
                != 1) {
                OPENSSL_free(pn); /* just in case */
                OPENSSL_free(ec);
                goto end;
            }
            BIO_printf(bio_err, "ECH entry: %d public_name: %s age: %lld%s%s\n",
                oi_ind, pn, (long long)secs,
                has_priv ? " (has private key)" : "",
                for_retry ? " (will be sent in retry-configs)" : "");
            BIO_printf(bio_err, "\t%s\n", ec);
            OPENSSL_free(pn);
            OPENSSL_free(ec);
        }
        if (verbose)
            BIO_printf(bio_err, "Success printing %d ECHConfigList\n", oi_cnt);
        rv = 0;
    }
end:
    OSSL_ECHSTORE_free(es);
    BIO_free_all(ecf);
    return rv;
opthelp:
    BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
    BIO_printf(bio_err, "\tup to %d -in instances allowed\n", OSSL_ECH_MAXINFILES);
    OSSL_ECHSTORE_free(es);
    BIO_free_all(ecf);
    return rv;
}

#endif
