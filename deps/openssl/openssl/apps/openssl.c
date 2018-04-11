/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/lhash.h>
#include <openssl/conf.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#ifndef OPENSSL_NO_ENGINE
# include <openssl/engine.h>
#endif
#include <openssl/err.h>
#ifdef OPENSSL_FIPS
# include <openssl/fips.h>
#endif
#define USE_SOCKETS /* needed for the _O_BINARY defs in the MS world */
#include "s_apps.h"
/* Needed to get the other O_xxx flags. */
#ifdef OPENSSL_SYS_VMS
# include <unixio.h>
#endif
#define INCLUDE_FUNCTION_TABLE
#include "apps.h"


#ifdef OPENSSL_NO_CAMELLIA
# define FORMAT "%-15s"
# define COLUMNS 5
#else
# define FORMAT "%-18s"
# define COLUMNS 4
#endif

/* Special sentinel to exit the program. */
#define EXIT_THE_PROGRAM (-1)

/*
 * The LHASH callbacks ("hash" & "cmp") have been replaced by functions with
 * the base prototypes (we cast each variable inside the function to the
 * required type of "FUNCTION*"). This removes the necessity for
 * macro-generated wrapper functions.
 */
static LHASH_OF(FUNCTION) *prog_init(void);
static int do_cmd(LHASH_OF(FUNCTION) *prog, int argc, char *argv[]);
static void list_pkey(void);
static void list_type(FUNC_TYPE ft);
static void list_disabled(void);
char *default_config_file = NULL;

BIO *bio_in = NULL;
BIO *bio_out = NULL;
BIO *bio_err = NULL;

static int apps_startup()
{
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    /* Set non-default library initialisation settings */
    if (!OPENSSL_init_ssl(OPENSSL_INIT_ENGINE_ALL_BUILTIN
                          | OPENSSL_INIT_LOAD_CONFIG, NULL))
        return 0;

#ifndef OPENSSL_NO_UI
    setup_ui_method();
#endif

    return 1;
}

static void apps_shutdown()
{
#ifndef OPENSSL_NO_UI
    destroy_ui_method();
#endif
}

static char *make_config_name()
{
    const char *t;
    size_t len;
    char *p;

    if ((t = getenv("OPENSSL_CONF")) != NULL)
        return OPENSSL_strdup(t);

    t = X509_get_default_cert_area();
    len = strlen(t) + 1 + strlen(OPENSSL_CONF) + 1;
    p = app_malloc(len, "config filename buffer");
    strcpy(p, t);
#ifndef OPENSSL_SYS_VMS
    strcat(p, "/");
#endif
    strcat(p, OPENSSL_CONF);

    return p;
}

int main(int argc, char *argv[])
{
    FUNCTION f, *fp;
    LHASH_OF(FUNCTION) *prog = NULL;
    char **copied_argv = NULL;
    char *p, *pname;
    char buf[1024];
    const char *prompt;
    ARGS arg;
    int first, n, i, ret = 0;

    arg.argv = NULL;
    arg.size = 0;

    /* Set up some of the environment. */
    default_config_file = make_config_name();
    bio_in = dup_bio_in(FORMAT_TEXT);
    bio_out = dup_bio_out(FORMAT_TEXT);
    bio_err = dup_bio_err(FORMAT_TEXT);

#if defined(OPENSSL_SYS_VMS) && defined(__DECC)
    copied_argv = argv = copy_argv(&argc, argv);
#elif defined(_WIN32)
    /*
     * Replace argv[] with UTF-8 encoded strings.
     */
    win32_utf8argv(&argc, &argv);
#endif

    p = getenv("OPENSSL_DEBUG_MEMORY");
    if (p != NULL && strcmp(p, "on") == 0)
        CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    if (getenv("OPENSSL_FIPS")) {
#ifdef OPENSSL_FIPS
        if (!FIPS_mode_set(1)) {
            ERR_print_errors(bio_err);
            return 1;
        }
#else
        BIO_printf(bio_err, "FIPS mode not supported.\n");
        return 1;
#endif
    }

    if (!apps_startup()) {
        BIO_printf(bio_err,
                   "FATAL: Startup failure (dev note: apps_startup() failed)\n");
        ERR_print_errors(bio_err);
        ret = 1;
        goto end;
    }

    prog = prog_init();
    pname = opt_progname(argv[0]);

    /* first check the program name */
    f.name = pname;
    fp = lh_FUNCTION_retrieve(prog, &f);
    if (fp != NULL) {
        argv[0] = pname;
        ret = fp->func(argc, argv);
        goto end;
    }

    /* If there is stuff on the command line, run with that. */
    if (argc != 1) {
        argc--;
        argv++;
        ret = do_cmd(prog, argc, argv);
        if (ret < 0)
            ret = 0;
        goto end;
    }

    /* ok, lets enter interactive mode */
    for (;;) {
        ret = 0;
        /* Read a line, continue reading if line ends with \ */
        for (p = buf, n = sizeof(buf), i = 0, first = 1; n > 0; first = 0) {
            prompt = first ? "OpenSSL> " : "> ";
            p[0] = '\0';
#ifndef READLINE
            fputs(prompt, stdout);
            fflush(stdout);
            if (!fgets(p, n, stdin))
                goto end;
            if (p[0] == '\0')
                goto end;
            i = strlen(p);
            if (i <= 1)
                break;
            if (p[i - 2] != '\\')
                break;
            i -= 2;
            p += i;
            n -= i;
#else
            {
                extern char *readline(const char *);
                extern void add_history(const char *cp);
                char *text;

                text = readline(prompt);
                if (text == NULL)
                    goto end;
                i = strlen(text);
                if (i == 0 || i > n)
                    break;
                if (text[i - 1] != '\\') {
                    p += strlen(strcpy(p, text));
                    free(text);
                    add_history(buf);
                    break;
                }

                text[i - 1] = '\0';
                p += strlen(strcpy(p, text));
                free(text);
                n -= i;
            }
#endif
        }

        if (!chopup_args(&arg, buf)) {
            BIO_printf(bio_err, "Can't parse (no memory?)\n");
            break;
        }

        ret = do_cmd(prog, arg.argc, arg.argv);
        if (ret == EXIT_THE_PROGRAM) {
            ret = 0;
            goto end;
        }
        if (ret != 0)
            BIO_printf(bio_err, "error in %s\n", arg.argv[0]);
        (void)BIO_flush(bio_out);
        (void)BIO_flush(bio_err);
    }
    ret = 1;
 end:
    OPENSSL_free(copied_argv);
    OPENSSL_free(default_config_file);
    lh_FUNCTION_free(prog);
    OPENSSL_free(arg.argv);

    BIO_free(bio_in);
    BIO_free_all(bio_out);
    apps_shutdown();
#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks(bio_err) <= 0)
        ret = 1;
#endif
    BIO_free(bio_err);
    EXIT(ret);
}

OPTIONS exit_options[] = {
    {NULL}
};

static void list_cipher_fn(const EVP_CIPHER *c,
                           const char *from, const char *to, void *arg)
{
    if (c)
        BIO_printf(arg, "%s\n", EVP_CIPHER_name(c));
    else {
        if (!from)
            from = "<undefined>";
        if (!to)
            to = "<undefined>";
        BIO_printf(arg, "%s => %s\n", from, to);
    }
}

static void list_md_fn(const EVP_MD *m,
                       const char *from, const char *to, void *arg)
{
    if (m)
        BIO_printf(arg, "%s\n", EVP_MD_name(m));
    else {
        if (!from)
            from = "<undefined>";
        if (!to)
            to = "<undefined>";
        BIO_printf((BIO *)arg, "%s => %s\n", from, to);
    }
}

/* Unified enum for help and list commands. */
typedef enum HELPLIST_CHOICE {
    OPT_ERR = -1, OPT_EOF = 0, OPT_HELP,
    OPT_COMMANDS, OPT_DIGEST_COMMANDS,
    OPT_DIGEST_ALGORITHMS, OPT_CIPHER_COMMANDS, OPT_CIPHER_ALGORITHMS,
    OPT_PK_ALGORITHMS, OPT_DISABLED
} HELPLIST_CHOICE;

OPTIONS list_options[] = {
    {"help", OPT_HELP, '-', "Display this summary"},
    {"commands", OPT_COMMANDS, '-', "List of standard commands"},
    {"digest-commands", OPT_DIGEST_COMMANDS, '-',
     "List of message digest commands"},
    {"digest-algorithms", OPT_DIGEST_ALGORITHMS, '-',
     "List of message digest algorithms"},
    {"cipher-commands", OPT_CIPHER_COMMANDS, '-', "List of cipher commands"},
    {"cipher-algorithms", OPT_CIPHER_ALGORITHMS, '-',
     "List of cipher algorithms"},
    {"public-key-algorithms", OPT_PK_ALGORITHMS, '-',
     "List of public key algorithms"},
    {"disabled", OPT_DISABLED, '-',
     "List of disabled features"},
    {NULL}
};

int list_main(int argc, char **argv)
{
    char *prog;
    HELPLIST_CHOICE o;
    int done = 0;

    prog = opt_init(argc, argv, list_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_EOF:  /* Never hit, but suppresses warning */
        case OPT_ERR:
opthelp:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            return 1;
        case OPT_HELP:
            opt_help(list_options);
            break;
        case OPT_COMMANDS:
            list_type(FT_general);
            break;
        case OPT_DIGEST_COMMANDS:
            list_type(FT_md);
            break;
        case OPT_DIGEST_ALGORITHMS:
            EVP_MD_do_all_sorted(list_md_fn, bio_out);
            break;
        case OPT_CIPHER_COMMANDS:
            list_type(FT_cipher);
            break;
        case OPT_CIPHER_ALGORITHMS:
            EVP_CIPHER_do_all_sorted(list_cipher_fn, bio_out);
            break;
        case OPT_PK_ALGORITHMS:
            list_pkey();
            break;
        case OPT_DISABLED:
            list_disabled();
            break;
        }
        done = 1;
    }
    if (opt_num_rest() != 0) {
        BIO_printf(bio_err, "Extra arguments given.\n");
        goto opthelp;
    }

    if (!done)
        goto opthelp;

    return 0;
}

OPTIONS help_options[] = {
    {"help", OPT_HELP, '-', "Display this summary"},
    {NULL}
};

int help_main(int argc, char **argv)
{
    FUNCTION *fp;
    int i, nl;
    FUNC_TYPE tp;
    char *prog;
    HELPLIST_CHOICE o;

    prog = opt_init(argc, argv, help_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        default:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            return 1;
        case OPT_HELP:
            opt_help(help_options);
            return 0;
        }
    }

    if (opt_num_rest() != 0) {
        BIO_printf(bio_err, "Usage: %s\n", prog);
        return 1;
    }

    BIO_printf(bio_err, "\nStandard commands");
    i = 0;
    tp = FT_none;
    for (fp = functions; fp->name != NULL; fp++) {
        nl = 0;
        if (((i++) % COLUMNS) == 0) {
            BIO_printf(bio_err, "\n");
            nl = 1;
        }
        if (fp->type != tp) {
            tp = fp->type;
            if (!nl)
                BIO_printf(bio_err, "\n");
            if (tp == FT_md) {
                i = 1;
                BIO_printf(bio_err,
                           "\nMessage Digest commands (see the `dgst' command for more details)\n");
            } else if (tp == FT_cipher) {
                i = 1;
                BIO_printf(bio_err,
                           "\nCipher commands (see the `enc' command for more details)\n");
            }
        }
        BIO_printf(bio_err, FORMAT, fp->name);
    }
    BIO_printf(bio_err, "\n\n");
    return 0;
}

int exit_main(int argc, char **argv)
{
    return EXIT_THE_PROGRAM;
}

static void list_type(FUNC_TYPE ft)
{
    FUNCTION *fp;
    int i = 0;

    for (fp = functions; fp->name != NULL; fp++)
        if (fp->type == ft) {
            if ((i++ % COLUMNS) == 0)
                BIO_printf(bio_out, "\n");
            BIO_printf(bio_out, FORMAT, fp->name);
        }
    BIO_printf(bio_out, "\n");
}

static int do_cmd(LHASH_OF(FUNCTION) *prog, int argc, char *argv[])
{
    FUNCTION f, *fp;

    if (argc <= 0 || argv[0] == NULL)
        return (0);
    f.name = argv[0];
    fp = lh_FUNCTION_retrieve(prog, &f);
    if (fp == NULL) {
        if (EVP_get_digestbyname(argv[0])) {
            f.type = FT_md;
            f.func = dgst_main;
            fp = &f;
        } else if (EVP_get_cipherbyname(argv[0])) {
            f.type = FT_cipher;
            f.func = enc_main;
            fp = &f;
        }
    }
    if (fp != NULL) {
        return (fp->func(argc, argv));
    }
    if ((strncmp(argv[0], "no-", 3)) == 0) {
        /*
         * User is asking if foo is unsupported, by trying to "run" the
         * no-foo command.  Strange.
         */
        f.name = argv[0] + 3;
        if (lh_FUNCTION_retrieve(prog, &f) == NULL) {
            BIO_printf(bio_out, "%s\n", argv[0]);
            return (0);
        }
        BIO_printf(bio_out, "%s\n", argv[0] + 3);
        return 1;
    }
    if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "q") == 0 ||
        strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "bye") == 0)
        /* Special value to mean "exit the program. */
        return EXIT_THE_PROGRAM;

    BIO_printf(bio_err, "Invalid command '%s'; type \"help\" for a list.\n",
               argv[0]);
    return (1);
}

static void list_pkey(void)
{
    int i;

    for (i = 0; i < EVP_PKEY_asn1_get_count(); i++) {
        const EVP_PKEY_ASN1_METHOD *ameth;
        int pkey_id, pkey_base_id, pkey_flags;
        const char *pinfo, *pem_str;
        ameth = EVP_PKEY_asn1_get0(i);
        EVP_PKEY_asn1_get0_info(&pkey_id, &pkey_base_id, &pkey_flags,
                                &pinfo, &pem_str, ameth);
        if (pkey_flags & ASN1_PKEY_ALIAS) {
            BIO_printf(bio_out, "Name: %s\n", OBJ_nid2ln(pkey_id));
            BIO_printf(bio_out, "\tAlias for: %s\n",
                       OBJ_nid2ln(pkey_base_id));
        } else {
            BIO_printf(bio_out, "Name: %s\n", pinfo);
            BIO_printf(bio_out, "\tType: %s Algorithm\n",
                       pkey_flags & ASN1_PKEY_DYNAMIC ?
                       "External" : "Builtin");
            BIO_printf(bio_out, "\tOID: %s\n", OBJ_nid2ln(pkey_id));
            if (pem_str == NULL)
                pem_str = "(none)";
            BIO_printf(bio_out, "\tPEM string: %s\n", pem_str);
        }

    }
}

static int function_cmp(const FUNCTION * a, const FUNCTION * b)
{
    return strncmp(a->name, b->name, 8);
}

static unsigned long function_hash(const FUNCTION * a)
{
    return OPENSSL_LH_strhash(a->name);
}

static int SortFnByName(const void *_f1, const void *_f2)
{
    const FUNCTION *f1 = _f1;
    const FUNCTION *f2 = _f2;

    if (f1->type != f2->type)
        return f1->type - f2->type;
    return strcmp(f1->name, f2->name);
}

static void list_disabled(void)
{
    BIO_puts(bio_out, "Disabled algorithms:\n");
#ifdef OPENSSL_NO_BF
    BIO_puts(bio_out, "BF\n");
#endif
#ifdef OPENSSL_NO_BLAKE2
    BIO_puts(bio_out, "BLAKE2\n");
#endif
#ifdef OPENSSL_NO_CAMELLIA
    BIO_puts(bio_out, "CAMELLIA\n");
#endif
#ifdef OPENSSL_NO_CAST
    BIO_puts(bio_out, "CAST\n");
#endif
#ifdef OPENSSL_NO_CMAC
    BIO_puts(bio_out, "CMAC\n");
#endif
#ifdef OPENSSL_NO_CMS
    BIO_puts(bio_out, "CMS\n");
#endif
#ifdef OPENSSL_NO_COMP
    BIO_puts(bio_out, "COMP\n");
#endif
#ifdef OPENSSL_NO_DES
    BIO_puts(bio_out, "DES\n");
#endif
#ifdef OPENSSL_NO_DGRAM
    BIO_puts(bio_out, "DGRAM\n");
#endif
#ifdef OPENSSL_NO_DH
    BIO_puts(bio_out, "DH\n");
#endif
#ifdef OPENSSL_NO_DSA
    BIO_puts(bio_out, "DSA\n");
#endif
#if defined(OPENSSL_NO_DTLS)
    BIO_puts(bio_out, "DTLS\n");
#endif
#if defined(OPENSSL_NO_DTLS1)
    BIO_puts(bio_out, "DTLS1\n");
#endif
#if defined(OPENSSL_NO_DTLS1_2)
    BIO_puts(bio_out, "DTLS1_2\n");
#endif
#ifdef OPENSSL_NO_EC
    BIO_puts(bio_out, "EC\n");
#endif
#ifdef OPENSSL_NO_EC2M
    BIO_puts(bio_out, "EC2M\n");
#endif
#ifdef OPENSSL_NO_ENGINE
    BIO_puts(bio_out, "ENGINE\n");
#endif
#ifdef OPENSSL_NO_GOST
    BIO_puts(bio_out, "GOST\n");
#endif
#ifdef OPENSSL_NO_HEARTBEATS
    BIO_puts(bio_out, "HEARTBEATS\n");
#endif
#ifdef OPENSSL_NO_IDEA
    BIO_puts(bio_out, "IDEA\n");
#endif
#ifdef OPENSSL_NO_MD2
    BIO_puts(bio_out, "MD2\n");
#endif
#ifdef OPENSSL_NO_MD4
    BIO_puts(bio_out, "MD4\n");
#endif
#ifdef OPENSSL_NO_MD5
    BIO_puts(bio_out, "MD5\n");
#endif
#ifdef OPENSSL_NO_MDC2
    BIO_puts(bio_out, "MDC2\n");
#endif
#ifdef OPENSSL_NO_OCB
    BIO_puts(bio_out, "OCB\n");
#endif
#ifdef OPENSSL_NO_OCSP
    BIO_puts(bio_out, "OCSP\n");
#endif
#ifdef OPENSSL_NO_PSK
    BIO_puts(bio_out, "PSK\n");
#endif
#ifdef OPENSSL_NO_RC2
    BIO_puts(bio_out, "RC2\n");
#endif
#ifdef OPENSSL_NO_RC4
    BIO_puts(bio_out, "RC4\n");
#endif
#ifdef OPENSSL_NO_RC5
    BIO_puts(bio_out, "RC5\n");
#endif
#ifdef OPENSSL_NO_RMD160
    BIO_puts(bio_out, "RMD160\n");
#endif
#ifdef OPENSSL_NO_RSA
    BIO_puts(bio_out, "RSA\n");
#endif
#ifdef OPENSSL_NO_SCRYPT
    BIO_puts(bio_out, "SCRYPT\n");
#endif
#ifdef OPENSSL_NO_SCTP
    BIO_puts(bio_out, "SCTP\n");
#endif
#ifdef OPENSSL_NO_SEED
    BIO_puts(bio_out, "SEED\n");
#endif
#ifdef OPENSSL_NO_SOCK
    BIO_puts(bio_out, "SOCK\n");
#endif
#ifdef OPENSSL_NO_SRP
    BIO_puts(bio_out, "SRP\n");
#endif
#ifdef OPENSSL_NO_SRTP
    BIO_puts(bio_out, "SRTP\n");
#endif
#ifdef OPENSSL_NO_SSL3
    BIO_puts(bio_out, "SSL3\n");
#endif
#ifdef OPENSSL_NO_TLS1
    BIO_puts(bio_out, "TLS1\n");
#endif
#ifdef OPENSSL_NO_TLS1_1
    BIO_puts(bio_out, "TLS1_1\n");
#endif
#ifdef OPENSSL_NO_TLS1_2
    BIO_puts(bio_out, "TLS1_2\n");
#endif
#ifdef OPENSSL_NO_WHIRLPOOL
    BIO_puts(bio_out, "WHIRLPOOL\n");
#endif
#ifndef ZLIB
    BIO_puts(bio_out, "ZLIB\n");
#endif
}

static LHASH_OF(FUNCTION) *prog_init(void)
{
    LHASH_OF(FUNCTION) *ret;
    FUNCTION *f;
    size_t i;

    /* Sort alphabetically within category. For nicer help displays. */
    for (i = 0, f = functions; f->name != NULL; ++f, ++i) ;
    qsort(functions, i, sizeof(*functions), SortFnByName);

    if ((ret = lh_FUNCTION_new(function_hash, function_cmp)) == NULL)
        return (NULL);

    for (f = functions; f->name != NULL; f++)
        (void)lh_FUNCTION_insert(ret, f);
    return (ret);
}
