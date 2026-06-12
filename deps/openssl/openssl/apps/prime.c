/*
 * Copyright 2004-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>

#include "apps.h"
#include "progs.h"
#include <openssl/bn.h>

/* Consistent with RSA modulus size limit and the size of plausible individual primes */
#define BUFSIZE 4098

typedef enum OPTION_choice {
    OPT_COMMON,
    OPT_HEX, OPT_GENERATE, OPT_BITS, OPT_SAFE, OPT_CHECKS,
    OPT_PROV_ENUM,
    OPT_IN_FILE
} OPTION_CHOICE;

static int check_num(const char *s, const int is_hex)
{
    int i;
    /*
     * It would make sense to use ossl_isxdigit and ossl_isdigit here,
     * but ossl_ctype_check is a local symbol in libcrypto.so.
     */
    if (is_hex) {
        for (i = 0; ('0' <= s[i] && s[i] <= '9')
                    || ('A' <= s[i] && s[i] <= 'F')
                    || ('a' <= s[i] && s[i] <= 'f'); i++);
    } else {
        for (i = 0;  '0' <= s[i] && s[i] <= '9'; i++);
    }
    return s[i] == 0;
}

static void process_num(const char *s, const int is_hex)
{
    int r;
    BIGNUM *bn = NULL;

    r = check_num(s, is_hex);

    if (r)
        r = is_hex ? BN_hex2bn(&bn, s) : BN_dec2bn(&bn, s);

    if (!r) {
        BIO_printf(bio_err, "Failed to process value (%s)\n", s);
        BN_free(bn);
        return;
    }

    BN_print(bio_out, bn);
    r = BN_check_prime(bn, NULL, NULL);
    BN_free(bn);
    if (r < 0) {
        BIO_printf(bio_err, "Error checking prime\n");
        return;
    }

    BIO_printf(bio_out, " (%s) %s prime\n", s, r == 1 ? "is" : "is not");
}

const OPTIONS prime_options[] = {
    {OPT_HELP_STR, 1, '-', "Usage: %s [options] [number...]\n"},

    OPT_SECTION("General"),
    {"help", OPT_HELP, '-', "Display this summary"},
    {"bits", OPT_BITS, 'p', "Size of number in bits"},
    {"checks", OPT_CHECKS, 'p', "Number of checks"},
    {"hex", OPT_HEX, '-',
     "Enables hex format for output from prime generation or input to primality checking"},
    {"in", OPT_IN_FILE, '-', "Provide file names containing numbers for primality checking"},

    OPT_SECTION("Output"),
    {"generate", OPT_GENERATE, '-', "Generate a prime"},
    {"safe", OPT_SAFE, '-',
     "When used with -generate, generate a safe prime"},

    OPT_PROV_OPTIONS,

    OPT_PARAMETERS(),
    {"number", 0, 0, "Number(s) to check for primality if not generating"},
    {NULL}
};

int prime_main(int argc, char **argv)
{
    BIGNUM *bn = NULL;
    int hex = 0, generate = 0, bits = 0, safe = 0, ret = 1, in_file = 0;
    char *prog;
    OPTION_CHOICE o;
    char file_read_buf[BUFSIZE] = { 0 };
    BIO *in = NULL;

    prog = opt_init(argc, argv, prime_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_EOF:
        case OPT_ERR:
opthelp:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            goto end;
        case OPT_HELP:
            opt_help(prime_options);
            ret = 0;
            goto end;
        case OPT_HEX:
            hex = 1;
            break;
        case OPT_GENERATE:
            generate = 1;
            break;
        case OPT_BITS:
            bits = atoi(opt_arg());
            break;
        case OPT_SAFE:
            safe = 1;
            break;
        case OPT_CHECKS:
            /* ignore parameter and argument */
            opt_arg();
            break;
        case OPT_PROV_CASES:
            if (!opt_provider(o))
                goto end;
            break;
        case OPT_IN_FILE:
            in_file = 1;
            break;
        }
    }

    /* Optional arguments are numbers to check. */
    if (generate && !opt_check_rest_arg(NULL))
        goto opthelp;
    argc = opt_num_rest();
    argv = opt_rest();
    if (!generate && argc == 0) {
        BIO_printf(bio_err, "Missing number (s) to check\n");
        goto opthelp;
    }

    if (generate) {
        char *s;

        if (!bits) {
            BIO_printf(bio_err, "Specify the number of bits.\n");
            goto end;
        }
        bn = BN_new();
        if (bn == NULL) {
            BIO_printf(bio_err, "Out of memory.\n");
            goto end;
        }
        if (!BN_generate_prime_ex(bn, bits, safe, NULL, NULL, NULL)) {
            BIO_printf(bio_err, "Failed to generate prime.\n");
            goto end;
        }
        s = hex ? BN_bn2hex(bn) : BN_bn2dec(bn);
        if (s == NULL) {
            BIO_printf(bio_err, "Out of memory.\n");
            goto end;
        }
        BIO_printf(bio_out, "%s\n", s);
        OPENSSL_free(s);
    } else {
        for ( ; *argv; argv++) {
            int bytes_read = 0;

            if (!in_file) {
                process_num(argv[0], hex);
            } else {
                in = bio_open_default_quiet(argv[0], 'r', 0);
                if (in == NULL) {
                    BIO_printf(bio_err, "Error opening file %s\n", argv[0]);
                    continue;
                }

                while ((bytes_read = BIO_get_line(in, file_read_buf, BUFSIZE)) > 0) {
                    size_t valid_digits_length;

                    /* Number is too long. Discard remainder of the line */
                    if (bytes_read == BUFSIZE - 1 && file_read_buf[BUFSIZE - 2] != '\n') {
                        BIO_printf(bio_err, "Value in %s is over the maximum size (%d digits)\n",
                                   argv[0], BUFSIZE - 2);
                        while (BIO_get_line(in, file_read_buf, BUFSIZE) == BUFSIZE - 1);
                        continue;
                    }

                    valid_digits_length = strspn(file_read_buf, "1234567890abcdefABCDEF");
                    file_read_buf[valid_digits_length] = '\0';

                    process_num(file_read_buf, hex);
                }

                if (bytes_read < 0)
                    BIO_printf(bio_err, "Read error in %s\n", argv[0]);

                BIO_free(in);
            }
        }
    }

    ret = 0;
 end:
    BN_free(bn);
    return ret;
}
