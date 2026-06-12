/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/safestack.h>

#include "apps.h"
#include "progs.h"

/**
 * Print the given value escaped for the OpenSSL configuration file format.
 */
static void print_escaped_value(BIO *out, const char *value)
{
    const char *p;

    for (p = value; *p != '\0'; p++) {
        switch (*p) {
        case '"':
        case '\'':
        case '#':
        case '\\':
        case '$':
            BIO_printf(out, "\\");
            BIO_write(out, p, 1);
            break;
        case '\n':
            BIO_printf(out, "%s", "\\n");
            break;
        case '\r':
            BIO_printf(out, "%s", "\\r");
            break;
        case '\b':
            BIO_printf(out, "%s", "\\b");
            break;
        case '\t':
            BIO_printf(out, "%s", "\\t");
            break;
        case ' ':
            if (p == value || p[1] == '\0') {
                /*
                 * Quote spaces if they are the first or last char of the
                 * value. We could quote the entire string (and it would
                 * certainly produce nicer output), but in quoted strings
                 * the escape sequences for \n, \r, \t, and \b do not work.
                 * To make sure we're producing correct results we'd thus
                 * have to selectively not use those in quoted strings and
                 * close and re-open the quotes if they appear, which is
                 * more trouble than adding the quotes just around the
                 * first and last leading and trailing space.
                 */
                BIO_printf(out, "%s", "\" \"");
                break;
            }
            /* FALLTHROUGH */
        default:
            BIO_write(out, p, 1);
            break;
        }
    }
}

/**
 * Print all values in the configuration section identified by section_name
 */
static void print_section(BIO *out, const CONF *cnf, OPENSSL_CSTRING section_name)
{
    STACK_OF(CONF_VALUE) *values = NCONF_get_section(cnf, section_name);
    int idx;

    for (idx = 0; idx < sk_CONF_VALUE_num(values); idx++) {
        CONF_VALUE *value = sk_CONF_VALUE_value(values, idx);

        BIO_printf(out, "%s = ", value->name);
        print_escaped_value(out, value->value);
        BIO_printf(out, "\n");
    }
}

typedef enum OPTION_choice {
    OPT_COMMON,
    OPT_OUT,
    OPT_NOHEADER,
    OPT_CONFIG
} OPTION_CHOICE;

const OPTIONS configutl_options[] = {
    OPT_SECTION("General"),
    {"help", OPT_HELP, '-', "Display this summary"},
    {"config", OPT_CONFIG, 's', "Config file to deal with (the default one if omitted)"},
    OPT_SECTION("Output"),
    {"out", OPT_OUT, '>', "Output to filename rather than stdout"},
    {"noheader", OPT_NOHEADER, '-', "Don't print the information about original config"},
    {NULL}
};

/**
 * Parse the passed OpenSSL configuration file (or the default one/specified in the
 * OPENSSL_CONF environment variable) and write it back in
 * a canonical format with all includes and variables expanded.
 */
int configutl_main(int argc, char *argv[])
{
    int ret = 1;
    char *prog, *configfile = NULL;
    OPTION_CHOICE o;
    CONF *cnf = NULL;
    long eline = 0;
    int default_section_idx, idx;
    int no_header = 0;
    STACK_OF(OPENSSL_CSTRING) *sections = NULL;
    BIO *out = NULL;
    const char *outfile = NULL;

    prog = opt_init(argc, argv, configutl_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_HELP:
            opt_help(configutl_options);
            ret = 0;
            goto end;
            break;
        case OPT_NOHEADER:
            no_header = 1;
            break;
        case OPT_CONFIG:
            /*
             * In case multiple OPT_CONFIG options are passed, we need to free
             * the previous one before assigning the new one.
             */
            OPENSSL_free(configfile);
            configfile = OPENSSL_strdup(opt_arg());
            break;
        case OPT_OUT:
            outfile = opt_arg();
            break;
        case OPT_ERR:
        /*
         * default needed for OPT_EOF which might never happen.
         */
        default:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            goto end;
        }
    }

    out = bio_open_default(outfile, 'w', FORMAT_TEXT);
    if (out == NULL)
        goto end;

    if (configfile == NULL)
        configfile = CONF_get1_default_config_file();

    if (configfile == NULL)
        goto end;

    if ((cnf = NCONF_new(NULL)) == NULL)
        goto end;

    if (NCONF_load(cnf, configfile, &eline) == 0) {
        BIO_printf(bio_err, "Error on line %ld of configuration file\n", eline + 1);
        goto end;
    }

    if ((sections = NCONF_get_section_names(cnf)) == NULL)
        goto end;

    if (no_header == 0)
        BIO_printf(out, "# This configuration file was linearized and expanded from %s\n",
                   configfile);

    default_section_idx = sk_OPENSSL_CSTRING_find(sections, "default");
    if (default_section_idx != -1)
        print_section(out, cnf, "default");

    for (idx = 0; idx < sk_OPENSSL_CSTRING_num(sections); idx++) {
        OPENSSL_CSTRING section_name = sk_OPENSSL_CSTRING_value(sections, idx);

        if (idx == default_section_idx)
            continue;

        BIO_printf(out, "\n[%s]\n", section_name);
        print_section(out, cnf, section_name);
    }

    ret = 0;

end:
    ERR_print_errors(bio_err);
    BIO_free(out);
    OPENSSL_free(configfile);
    NCONF_free(cnf);
    sk_OPENSSL_CSTRING_free(sections);
    return ret;
}
