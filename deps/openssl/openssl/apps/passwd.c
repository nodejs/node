/*
 * Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if defined OPENSSL_NO_MD5 || defined CHARSET_EBCDIC
# define NO_MD5CRYPT_1
#endif

#if !defined(OPENSSL_NO_DES) || !defined(NO_MD5CRYPT_1)

# include <string.h>

# include "apps.h"

# include <openssl/bio.h>
# include <openssl/err.h>
# include <openssl/evp.h>
# include <openssl/rand.h>
# ifndef OPENSSL_NO_DES
#  include <openssl/des.h>
# endif
# ifndef NO_MD5CRYPT_1
#  include <openssl/md5.h>
# endif

static unsigned const char cov_2char[64] = {
    /* from crypto/des/fcrypt.c */
    0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
    0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C,
    0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54,
    0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x61, 0x62,
    0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
    0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72,
    0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A
};

static int do_passwd(int passed_salt, char **salt_p, char **salt_malloc_p,
                     char *passwd, BIO *out, int quiet, int table,
                     int reverse, size_t pw_maxlen, int usecrypt, int use1,
                     int useapr1);

typedef enum OPTION_choice {
    OPT_ERR = -1, OPT_EOF = 0, OPT_HELP,
    OPT_IN,
    OPT_NOVERIFY, OPT_QUIET, OPT_TABLE, OPT_REVERSE, OPT_APR1,
    OPT_1, OPT_CRYPT, OPT_SALT, OPT_STDIN
} OPTION_CHOICE;

OPTIONS passwd_options[] = {
    {"help", OPT_HELP, '-', "Display this summary"},
    {"in", OPT_IN, '<', "Read passwords from file"},
    {"noverify", OPT_NOVERIFY, '-',
     "Never verify when reading password from terminal"},
    {"quiet", OPT_QUIET, '-', "No warnings"},
    {"table", OPT_TABLE, '-', "Format output as table"},
    {"reverse", OPT_REVERSE, '-', "Switch table columns"},
    {"salt", OPT_SALT, 's', "Use provided salt"},
    {"stdin", OPT_STDIN, '-', "Read passwords from stdin"},
# ifndef NO_MD5CRYPT_1
    {"apr1", OPT_APR1, '-', "MD5-based password algorithm, Apache variant"},
    {"1", OPT_1, '-', "MD5-based password algorithm"},
# endif
# ifndef OPENSSL_NO_DES
    {"crypt", OPT_CRYPT, '-', "Standard Unix password algorithm (default)"},
# endif
    {NULL}
};

int passwd_main(int argc, char **argv)
{
    BIO *in = NULL;
    char *infile = NULL, *salt = NULL, *passwd = NULL, **passwds = NULL;
    char *salt_malloc = NULL, *passwd_malloc = NULL, *prog;
    OPTION_CHOICE o;
    int in_stdin = 0, pw_source_defined = 0;
# ifndef OPENSSL_NO_UI
    int in_noverify = 0;
# endif
    int passed_salt = 0, quiet = 0, table = 0, reverse = 0;
    int ret = 1, usecrypt = 0, use1 = 0, useapr1 = 0;
    size_t passwd_malloc_size = 0, pw_maxlen = 256;

    prog = opt_init(argc, argv, passwd_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_EOF:
        case OPT_ERR:
 opthelp:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            goto end;
        case OPT_HELP:
            opt_help(passwd_options);
            ret = 0;
            goto end;
        case OPT_IN:
            if (pw_source_defined)
                goto opthelp;
            infile = opt_arg();
            pw_source_defined = 1;
            break;
        case OPT_NOVERIFY:
# ifndef OPENSSL_NO_UI
            in_noverify = 1;
# endif
            break;
        case OPT_QUIET:
            quiet = 1;
            break;
        case OPT_TABLE:
            table = 1;
            break;
        case OPT_REVERSE:
            reverse = 1;
            break;
        case OPT_1:
            use1 = 1;
            break;
        case OPT_APR1:
            useapr1 = 1;
            break;
        case OPT_CRYPT:
            usecrypt = 1;
            break;
        case OPT_SALT:
            passed_salt = 1;
            salt = opt_arg();
            break;
        case OPT_STDIN:
            if (pw_source_defined)
                goto opthelp;
            in_stdin = 1;
            pw_source_defined = 1;
            break;
        }
    }
    argc = opt_num_rest();
    argv = opt_rest();

    if (*argv) {
        if (pw_source_defined)
            goto opthelp;
        pw_source_defined = 1;
        passwds = argv;
    }

    if (!usecrypt && !use1 && !useapr1) {
        /* use default */
        usecrypt = 1;
    }
    if (usecrypt + use1 + useapr1 > 1) {
        /* conflict */
        goto opthelp;
    }

# ifdef OPENSSL_NO_DES
    if (usecrypt)
        goto opthelp;
# endif
# ifdef NO_MD5CRYPT_1
    if (use1 || useapr1)
        goto opthelp;
# endif

    if (infile != NULL && in_stdin) {
        BIO_printf(bio_err, "%s: Can't combine -in and -stdin\n", prog);
        goto end;
    }

    if (infile != NULL || in_stdin) {
        /*
         * If in_stdin is true, we know that infile is NULL, and that
         * bio_open_default() will give us back an alias for stdin.
         */
        in = bio_open_default(infile, 'r', FORMAT_TEXT);
        if (in == NULL)
            goto end;
    }

    if (usecrypt)
        pw_maxlen = 8;
    else if (use1 || useapr1)
        pw_maxlen = 256;        /* arbitrary limit, should be enough for most
                                 * passwords */

    if (passwds == NULL) {
        /* no passwords on the command line */

        passwd_malloc_size = pw_maxlen + 2;
        /* longer than necessary so that we can warn about truncation */
        passwd = passwd_malloc =
            app_malloc(passwd_malloc_size, "password buffer");
    }

    if ((in == NULL) && (passwds == NULL)) {
        /*
         * we use the following method to make sure what
         * in the 'else' section is always compiled, to
         * avoid rot of not-frequently-used code.
         */
        if (1) {
# ifndef OPENSSL_NO_UI
            /* build a null-terminated list */
            static char *passwds_static[2] = { NULL, NULL };

            passwds = passwds_static;
            if (in == NULL) {
                if (EVP_read_pw_string
                    (passwd_malloc, passwd_malloc_size, "Password: ",
                     !(passed_salt || in_noverify)) != 0)
                    goto end;
            }
            passwds[0] = passwd_malloc;
        } else {
# endif
            BIO_printf(bio_err, "password required\n");
            goto end;
        }
    }

    if (in == NULL) {
        assert(passwds != NULL);
        assert(*passwds != NULL);

        do {                    /* loop over list of passwords */
            passwd = *passwds++;
            if (!do_passwd(passed_salt, &salt, &salt_malloc, passwd, bio_out,
                           quiet, table, reverse, pw_maxlen, usecrypt, use1,
                           useapr1))
                goto end;
        } while (*passwds != NULL);
    } else {
        /* in != NULL */
        int done;

        assert(passwd != NULL);
        do {
            int r = BIO_gets(in, passwd, pw_maxlen + 1);
            if (r > 0) {
                char *c = (strchr(passwd, '\n'));
                if (c != NULL) {
                    *c = 0;     /* truncate at newline */
                } else {
                    /* ignore rest of line */
                    char trash[BUFSIZ];
                    do
                        r = BIO_gets(in, trash, sizeof(trash));
                    while ((r > 0) && (!strchr(trash, '\n')));
                }

                if (!do_passwd
                    (passed_salt, &salt, &salt_malloc, passwd, bio_out, quiet,
                     table, reverse, pw_maxlen, usecrypt, use1, useapr1))
                    goto end;
            }
            done = (r <= 0);
        } while (!done);
    }
    ret = 0;

 end:
    ERR_print_errors(bio_err);
    OPENSSL_free(salt_malloc);
    OPENSSL_free(passwd_malloc);
    BIO_free(in);
    return (ret);
}

# ifndef NO_MD5CRYPT_1
/*
 * MD5-based password algorithm (should probably be available as a library
 * function; then the static buffer would not be acceptable). For magic
 * string "1", this should be compatible to the MD5-based BSD password
 * algorithm. For 'magic' string "apr1", this is compatible to the MD5-based
 * Apache password algorithm. (Apparently, the Apache password algorithm is
 * identical except that the 'magic' string was changed -- the laziest
 * application of the NIH principle I've ever encountered.)
 */
static char *md5crypt(const char *passwd, const char *magic, const char *salt)
{
    /* "$apr1$..salt..$.......md5hash..........\0" */
    static char out_buf[6 + 9 + 24 + 2];
    unsigned char buf[MD5_DIGEST_LENGTH];
    char *salt_out;
    int n;
    unsigned int i;
    EVP_MD_CTX *md = NULL, *md2 = NULL;
    size_t passwd_len, salt_len, magic_len;

    passwd_len = strlen(passwd);
    out_buf[0] = '$';
    out_buf[1] = 0;
    magic_len = strlen(magic);

    if (magic_len > 4)    /* assert it's  "1" or "apr1" */
        return NULL;

    OPENSSL_strlcat(out_buf, magic, sizeof(out_buf));
    OPENSSL_strlcat(out_buf, "$", sizeof(out_buf));
    OPENSSL_strlcat(out_buf, salt, sizeof(out_buf));

    if (strlen(out_buf) > 6 + 8) /* assert "$apr1$..salt.." */
        return NULL;

    salt_out = out_buf + 2 + magic_len;
    salt_len = strlen(salt_out);

    if (salt_len > 8)
        return NULL;

    md = EVP_MD_CTX_new();
    if (md == NULL
        || !EVP_DigestInit_ex(md, EVP_md5(), NULL)
        || !EVP_DigestUpdate(md, passwd, passwd_len)
        || !EVP_DigestUpdate(md, "$", 1)
        || !EVP_DigestUpdate(md, magic, magic_len)
        || !EVP_DigestUpdate(md, "$", 1)
        || !EVP_DigestUpdate(md, salt_out, salt_len))
        goto err;

    md2 = EVP_MD_CTX_new();
    if (md2 == NULL
        || !EVP_DigestInit_ex(md2, EVP_md5(), NULL)
        || !EVP_DigestUpdate(md2, passwd, passwd_len)
        || !EVP_DigestUpdate(md2, salt_out, salt_len)
        || !EVP_DigestUpdate(md2, passwd, passwd_len)
        || !EVP_DigestFinal_ex(md2, buf, NULL))
        goto err;

    for (i = passwd_len; i > sizeof(buf); i -= sizeof(buf)) {
        if (!EVP_DigestUpdate(md, buf, sizeof(buf)))
            goto err;
    }
    if (!EVP_DigestUpdate(md, buf, i))
        goto err;

    n = passwd_len;
    while (n) {
        if (!EVP_DigestUpdate(md, (n & 1) ? "\0" : passwd, 1))
            goto err;
        n >>= 1;
    }
    if (!EVP_DigestFinal_ex(md, buf, NULL))
        return NULL;

    for (i = 0; i < 1000; i++) {
        if (!EVP_DigestInit_ex(md2, EVP_md5(), NULL))
            goto err;
        if (!EVP_DigestUpdate(md2,
                              (i & 1) ? (unsigned const char *)passwd : buf,
                              (i & 1) ? passwd_len : sizeof(buf)))
            goto err;
        if (i % 3) {
            if (!EVP_DigestUpdate(md2, salt_out, salt_len))
                goto err;
        }
        if (i % 7) {
            if (!EVP_DigestUpdate(md2, passwd, passwd_len))
                goto err;
        }
        if (!EVP_DigestUpdate(md2,
                              (i & 1) ? buf : (unsigned const char *)passwd,
                              (i & 1) ? sizeof(buf) : passwd_len))
                goto err;
        if (!EVP_DigestFinal_ex(md2, buf, NULL))
                goto err;
    }
    EVP_MD_CTX_free(md2);
    EVP_MD_CTX_free(md);
    md2 = NULL;
    md = NULL;

    {
        /* transform buf into output string */
        unsigned char buf_perm[sizeof(buf)];
        int dest, source;
        char *output;

        /* silly output permutation */
        for (dest = 0, source = 0; dest < 14;
             dest++, source = (source + 6) % 17)
            buf_perm[dest] = buf[source];
        buf_perm[14] = buf[5];
        buf_perm[15] = buf[11];
#  ifndef PEDANTIC              /* Unfortunately, this generates a "no
                                 * effect" warning */
        assert(16 == sizeof(buf_perm));
#  endif

        output = salt_out + salt_len;
        assert(output == out_buf + strlen(out_buf));

        *output++ = '$';

        for (i = 0; i < 15; i += 3) {
            *output++ = cov_2char[buf_perm[i + 2] & 0x3f];
            *output++ = cov_2char[((buf_perm[i + 1] & 0xf) << 2) |
                                  (buf_perm[i + 2] >> 6)];
            *output++ = cov_2char[((buf_perm[i] & 3) << 4) |
                                  (buf_perm[i + 1] >> 4)];
            *output++ = cov_2char[buf_perm[i] >> 2];
        }
        assert(i == 15);
        *output++ = cov_2char[buf_perm[i] & 0x3f];
        *output++ = cov_2char[buf_perm[i] >> 6];
        *output = 0;
        assert(strlen(out_buf) < sizeof(out_buf));
    }

    return out_buf;

 err:
    EVP_MD_CTX_free(md2);
    EVP_MD_CTX_free(md);
    return NULL;
}
# endif

static int do_passwd(int passed_salt, char **salt_p, char **salt_malloc_p,
                     char *passwd, BIO *out, int quiet, int table,
                     int reverse, size_t pw_maxlen, int usecrypt, int use1,
                     int useapr1)
{
    char *hash = NULL;

    assert(salt_p != NULL);
    assert(salt_malloc_p != NULL);

    /* first make sure we have a salt */
    if (!passed_salt) {
# ifndef OPENSSL_NO_DES
        if (usecrypt) {
            if (*salt_malloc_p == NULL)
                *salt_p = *salt_malloc_p = app_malloc(3, "salt buffer");
            if (RAND_bytes((unsigned char *)*salt_p, 2) <= 0)
                goto end;
            (*salt_p)[0] = cov_2char[(*salt_p)[0] & 0x3f]; /* 6 bits */
            (*salt_p)[1] = cov_2char[(*salt_p)[1] & 0x3f]; /* 6 bits */
            (*salt_p)[2] = 0;
#  ifdef CHARSET_EBCDIC
            ascii2ebcdic(*salt_p, *salt_p, 2); /* des_crypt will convert back
                                                * to ASCII */
#  endif
        }
# endif                         /* !OPENSSL_NO_DES */

# ifndef NO_MD5CRYPT_1
        if (use1 || useapr1) {
            int i;

            if (*salt_malloc_p == NULL)
                *salt_p = *salt_malloc_p = app_malloc(9, "salt buffer");
            if (RAND_bytes((unsigned char *)*salt_p, 8) <= 0)
                goto end;

            for (i = 0; i < 8; i++)
                (*salt_p)[i] = cov_2char[(*salt_p)[i] & 0x3f]; /* 6 bits */
            (*salt_p)[8] = 0;
        }
# endif                         /* !NO_MD5CRYPT_1 */
    }

    assert(*salt_p != NULL);

    /* truncate password if necessary */
    if ((strlen(passwd) > pw_maxlen)) {
        if (!quiet)
            /*
             * XXX: really we should know how to print a size_t, not cast it
             */
            BIO_printf(bio_err,
                       "Warning: truncating password to %u characters\n",
                       (unsigned)pw_maxlen);
        passwd[pw_maxlen] = 0;
    }
    assert(strlen(passwd) <= pw_maxlen);

    /* now compute password hash */
# ifndef OPENSSL_NO_DES
    if (usecrypt)
        hash = DES_crypt(passwd, *salt_p);
# endif
# ifndef NO_MD5CRYPT_1
    if (use1 || useapr1)
        hash = md5crypt(passwd, (use1 ? "1" : "apr1"), *salt_p);
# endif
    assert(hash != NULL);

    if (table && !reverse)
        BIO_printf(out, "%s\t%s\n", passwd, hash);
    else if (table && reverse)
        BIO_printf(out, "%s\t%s\n", hash, passwd);
    else
        BIO_printf(out, "%s\n", hash);
    return 1;

 end:
    return 0;
}
#else

int passwd_main(int argc, char **argv)
{
    BIO_printf(bio_err, "Program not available.\n");
    return (1);
}
#endif
