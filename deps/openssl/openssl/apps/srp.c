/* apps/srp.c */
/*
 * Written by Peter Sylvester (peter.sylvester@edelweb.fr) for the EdelKey
 * project and contributed to the OpenSSL project 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_SRP
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <openssl/conf.h>
# include <openssl/bio.h>
# include <openssl/err.h>
# include <openssl/txt_db.h>
# include <openssl/buffer.h>
# include <openssl/srp.h>

# include "apps.h"

# undef PROG
# define PROG srp_main

# define BASE_SECTION    "srp"
# define CONFIG_FILE "openssl.cnf"

# define ENV_RANDFILE            "RANDFILE"

# define ENV_DATABASE            "srpvfile"
# define ENV_DEFAULT_SRP         "default_srp"

static char *srp_usage[] = {
    "usage: srp [args] [user] \n",
    "\n",
    " -verbose        Talk alot while doing things\n",
    " -config file    A config file\n",
    " -name arg       The particular srp definition to use\n",
    " -srpvfile arg   The srp verifier file name\n",
    " -add            add an user and srp verifier\n",
    " -modify         modify the srp verifier of an existing user\n",
    " -delete         delete user from verifier file\n",
    " -list           list user\n",
    " -gn arg         g and N values to be used for new verifier\n",
    " -userinfo arg   additional info to be set for user\n",
    " -passin arg     input file pass phrase source\n",
    " -passout arg    output file pass phrase source\n",
# ifndef OPENSSL_NO_ENGINE
    " -engine e         - use engine e, possibly a hardware device.\n",
# endif
    NULL
};

# ifdef EFENCE
extern int EF_PROTECT_FREE;
extern int EF_PROTECT_BELOW;
extern int EF_ALIGNMENT;
# endif

static CONF *conf = NULL;
static char *section = NULL;

# define VERBOSE if (verbose)
# define VVERBOSE if (verbose>1)

int MAIN(int, char **);

static int get_index(CA_DB *db, char *id, char type)
{
    char **pp;
    int i;
    if (id == NULL)
        return -1;
    if (type == DB_SRP_INDEX) {
        for (i = 0; i < sk_OPENSSL_PSTRING_num(db->db->data); i++) {
            pp = sk_OPENSSL_PSTRING_value(db->db->data, i);
            if (pp[DB_srptype][0] == DB_SRP_INDEX
                && !strcmp(id, pp[DB_srpid]))
                return i;
        }
    } else {
        for (i = 0; i < sk_OPENSSL_PSTRING_num(db->db->data); i++) {
            pp = sk_OPENSSL_PSTRING_value(db->db->data, i);

            if (pp[DB_srptype][0] != DB_SRP_INDEX
                && !strcmp(id, pp[DB_srpid]))
                return i;
        }
    }

    return -1;
}

static void print_entry(CA_DB *db, BIO *bio, int indx, int verbose, char *s)
{
    if (indx >= 0 && verbose) {
        int j;
        char **pp = sk_OPENSSL_PSTRING_value(db->db->data, indx);
        BIO_printf(bio, "%s \"%s\"\n", s, pp[DB_srpid]);
        for (j = 0; j < DB_NUMBER; j++) {
            BIO_printf(bio_err, "  %d = \"%s\"\n", j, pp[j]);
        }
    }
}

static void print_index(CA_DB *db, BIO *bio, int indexindex, int verbose)
{
    print_entry(db, bio, indexindex, verbose, "g N entry");
}

static void print_user(CA_DB *db, BIO *bio, int userindex, int verbose)
{
    if (verbose > 0) {
        char **pp = sk_OPENSSL_PSTRING_value(db->db->data, userindex);

        if (pp[DB_srptype][0] != 'I') {
            print_entry(db, bio, userindex, verbose, "User entry");
            print_entry(db, bio, get_index(db, pp[DB_srpgN], 'I'), verbose,
                        "g N entry");
        }

    }
}

static int update_index(CA_DB *db, BIO *bio, char **row)
{
    char **irow;
    int i;

    irow = (char **)OPENSSL_malloc(sizeof(char *) * (DB_NUMBER + 1));
    if (irow == NULL) {
        BIO_printf(bio_err, "Memory allocation failure\n");
        return 0;
    }

    for (i = 0; i < DB_NUMBER; i++)
        irow[i] = row[i];
    irow[DB_NUMBER] = NULL;

    if (!TXT_DB_insert(db->db, irow)) {
        BIO_printf(bio, "failed to update srpvfile\n");
        BIO_printf(bio, "TXT_DB error number %ld\n", db->db->error);
        OPENSSL_free(irow);
        return 0;
    }
    return 1;
}

static void lookup_fail(const char *name, char *tag)
{
    BIO_printf(bio_err, "variable lookup failed for %s::%s\n", name, tag);
}

static char *srp_verify_user(const char *user, const char *srp_verifier,
                             char *srp_usersalt, const char *g, const char *N,
                             const char *passin, BIO *bio, int verbose)
{
    char password[1025];
    PW_CB_DATA cb_tmp;
    char *verifier = NULL;
    char *gNid = NULL;
    int len;

    cb_tmp.prompt_info = user;
    cb_tmp.password = passin;

    len = password_callback(password, sizeof(password)-1, 0, &cb_tmp);
    if (len > 0) {
        password[len] = 0;
        VERBOSE BIO_printf(bio,
                           "Validating\n   user=\"%s\"\n srp_verifier=\"%s\"\n srp_usersalt=\"%s\"\n g=\"%s\"\n N=\"%s\"\n",
                           user, srp_verifier, srp_usersalt, g, N);
        VVERBOSE BIO_printf(bio, "Pass %s\n", password);

        if (!(gNid = SRP_create_verifier(user, password, &srp_usersalt,
                                         &verifier, N, g))) {
            BIO_printf(bio, "Internal error validating SRP verifier\n");
        } else {
            if (strcmp(verifier, srp_verifier))
                gNid = NULL;
            OPENSSL_free(verifier);
        }
        OPENSSL_cleanse(password, len);
    }
    return gNid;
}

static char *srp_create_user(char *user, char **srp_verifier,
                             char **srp_usersalt, char *g, char *N,
                             char *passout, BIO *bio, int verbose)
{
    char password[1025];
    PW_CB_DATA cb_tmp;
    char *gNid = NULL;
    char *salt = NULL;
    int len;
    cb_tmp.prompt_info = user;
    cb_tmp.password = passout;

    len = password_callback(password, sizeof(password)-1, 1, &cb_tmp);
    if (len > 0) {
        password[len] = 0;
        VERBOSE BIO_printf(bio,
                           "Creating\n user=\"%s\"\n g=\"%s\"\n N=\"%s\"\n",
                           user, g, N);
        if (!(gNid = SRP_create_verifier(user, password, &salt,
                                         srp_verifier, N, g))) {
            BIO_printf(bio, "Internal error creating SRP verifier\n");
        } else {
            *srp_usersalt = salt;
        }
        OPENSSL_cleanse(password, len);
        VVERBOSE BIO_printf(bio, "gNid=%s salt =\"%s\"\n verifier =\"%s\"\n",
                            gNid, salt, *srp_verifier);

    }
    return gNid;
}

int MAIN(int argc, char **argv)
{
    int add_user = 0;
    int list_user = 0;
    int delete_user = 0;
    int modify_user = 0;
    char *user = NULL;

    char *passargin = NULL, *passargout = NULL;
    char *passin = NULL, *passout = NULL;
    char *gN = NULL;
    int gNindex = -1;
    char **gNrow = NULL;
    int maxgN = -1;

    char *userinfo = NULL;

    int badops = 0;
    int ret = 1;
    int errors = 0;
    int verbose = 0;
    int doupdatedb = 0;
    char *configfile = NULL;
    char *dbfile = NULL;
    CA_DB *db = NULL;
    char **pp;
    int i;
    long errorline = -1;
    char *randfile = NULL;
    ENGINE *e = NULL;
    char *engine = NULL;
    char *tofree = NULL;
    DB_ATTR db_attr;

# ifdef EFENCE
    EF_PROTECT_FREE = 1;
    EF_PROTECT_BELOW = 1;
    EF_ALIGNMENT = 0;
# endif

    apps_startup();

    conf = NULL;
    section = NULL;

    if (bio_err == NULL)
        if ((bio_err = BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_err, stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    argc--;
    argv++;
    while (argc >= 1 && badops == 0) {
        if (strcmp(*argv, "-verbose") == 0) {
            verbose++;
        } else if (strcmp(*argv, "-config") == 0) {
            if (--argc < 1)
                goto bad;
            configfile = *(++argv);
        } else if (strcmp(*argv, "-name") == 0) {
            if (--argc < 1)
                goto bad;
            section = *(++argv);
        } else if (strcmp(*argv, "-srpvfile") == 0) {
            if (--argc < 1)
                goto bad;
            dbfile = *(++argv);
        } else if (strcmp(*argv, "-add") == 0) {
            add_user = 1;
        } else if (strcmp(*argv, "-delete") == 0) {
            delete_user = 1;
        } else if (strcmp(*argv, "-modify") == 0) {
            modify_user = 1;
        } else if (strcmp(*argv, "-list") == 0) {
            list_user = 1;
        } else if (strcmp(*argv, "-gn") == 0) {
            if (--argc < 1)
                goto bad;
            gN = *(++argv);
        } else if (strcmp(*argv, "-userinfo") == 0) {
            if (--argc < 1)
                goto bad;
            userinfo = *(++argv);
        } else if (strcmp(*argv, "-passin") == 0) {
            if (--argc < 1)
                goto bad;
            passargin = *(++argv);
        } else if (strcmp(*argv, "-passout") == 0) {
            if (--argc < 1)
                goto bad;
            passargout = *(++argv);
        }
# ifndef OPENSSL_NO_ENGINE
        else if (strcmp(*argv, "-engine") == 0) {
            if (--argc < 1)
                goto bad;
            engine = *(++argv);
        }
# endif

        else if (**argv == '-') {
 bad:
            BIO_printf(bio_err, "unknown option %s\n", *argv);
            badops = 1;
            break;
        } else {
            break;
        }

        argc--;
        argv++;
    }

    if (dbfile && configfile) {
        BIO_printf(bio_err,
                   "-dbfile and -configfile cannot be specified together.\n");
        badops = 1;
    }
    if (add_user + delete_user + modify_user + list_user != 1) {
        BIO_printf(bio_err,
                   "Exactly one of the options -add, -delete, -modify -list must be specified.\n");
        badops = 1;
    }
    if (delete_user + modify_user + delete_user == 1 && argc <= 0) {
        BIO_printf(bio_err,
                   "Need at least one user for options -add, -delete, -modify. \n");
        badops = 1;
    }
    if ((passargin || passargout) && argc != 1) {
        BIO_printf(bio_err,
                   "-passin, -passout arguments only valid with one user.\n");
        badops = 1;
    }

    if (badops) {
        for (pp = srp_usage; (*pp != NULL); pp++)
            BIO_printf(bio_err, "%s", *pp);

        BIO_printf(bio_err, " -rand file%cfile%c...\n", LIST_SEPARATOR_CHAR,
                   LIST_SEPARATOR_CHAR);
        BIO_printf(bio_err,
                   "                 load the file (or the files in the directory) into\n");
        BIO_printf(bio_err, "                 the random number generator\n");
        goto err;
    }

    ERR_load_crypto_strings();

    e = setup_engine(bio_err, engine, 0);

    if (!app_passwd(bio_err, passargin, passargout, &passin, &passout)) {
        BIO_printf(bio_err, "Error getting passwords\n");
        goto err;
    }

    if (!dbfile) {

        /*****************************************************************/
        tofree = NULL;
        if (configfile == NULL)
            configfile = getenv("OPENSSL_CONF");
        if (configfile == NULL)
            configfile = getenv("SSLEAY_CONF");
        if (configfile == NULL) {
            const char *s = X509_get_default_cert_area();
            size_t len;

# ifdef OPENSSL_SYS_VMS
            len = strlen(s) + sizeof(CONFIG_FILE);
            tofree = OPENSSL_malloc(len);
            if (!tofree) {
                BIO_printf(bio_err, "Out of memory\n");
                goto err;
            }
            strcpy(tofree, s);
# else
            len = strlen(s) + sizeof(CONFIG_FILE) + 1;
            tofree = OPENSSL_malloc(len);
            if (!tofree) {
                BIO_printf(bio_err, "Out of memory\n");
                goto err;
            }
            BUF_strlcpy(tofree, s, len);
            BUF_strlcat(tofree, "/", len);
# endif
            BUF_strlcat(tofree, CONFIG_FILE, len);
            configfile = tofree;
        }

        VERBOSE BIO_printf(bio_err, "Using configuration from %s\n",
                           configfile);
        conf = NCONF_new(NULL);
        if (NCONF_load(conf, configfile, &errorline) <= 0) {
            if (errorline <= 0)
                BIO_printf(bio_err, "error loading the config file '%s'\n",
                           configfile);
            else
                BIO_printf(bio_err, "error on line %ld of config file '%s'\n",
                           errorline, configfile);
            goto err;
        }
        if (tofree) {
            OPENSSL_free(tofree);
            tofree = NULL;
        }

        if (!load_config(bio_err, conf))
            goto err;

        /* Lets get the config section we are using */
        if (section == NULL) {
            VERBOSE BIO_printf(bio_err,
                               "trying to read " ENV_DEFAULT_SRP
                               " in \" BASE_SECTION \"\n");

            section = NCONF_get_string(conf, BASE_SECTION, ENV_DEFAULT_SRP);
            if (section == NULL) {
                lookup_fail(BASE_SECTION, ENV_DEFAULT_SRP);
                goto err;
            }
        }

        if (randfile == NULL && conf)
            randfile = NCONF_get_string(conf, BASE_SECTION, "RANDFILE");

        VERBOSE BIO_printf(bio_err,
                           "trying to read " ENV_DATABASE
                           " in section \"%s\"\n", section);

        if ((dbfile = NCONF_get_string(conf, section, ENV_DATABASE)) == NULL) {
            lookup_fail(section, ENV_DATABASE);
            goto err;
        }

    }
    if (randfile == NULL)
        ERR_clear_error();
    else
        app_RAND_load_file(randfile, bio_err, 0);

    VERBOSE BIO_printf(bio_err, "Trying to read SRP verifier file \"%s\"\n",
                       dbfile);

    db = load_index(dbfile, &db_attr);
    if (db == NULL)
        goto err;

    /* Lets check some fields */
    for (i = 0; i < sk_OPENSSL_PSTRING_num(db->db->data); i++) {
        pp = sk_OPENSSL_PSTRING_value(db->db->data, i);

        if (pp[DB_srptype][0] == DB_SRP_INDEX) {
            maxgN = i;
            if (gNindex < 0 && gN != NULL && !strcmp(gN, pp[DB_srpid]))
                gNindex = i;

            print_index(db, bio_err, i, verbose > 1);
        }
    }

    VERBOSE BIO_printf(bio_err, "Database initialised\n");

    if (gNindex >= 0) {
        gNrow = sk_OPENSSL_PSTRING_value(db->db->data, gNindex);
        print_entry(db, bio_err, gNindex, verbose > 1, "Default g and N");
    } else if (maxgN > 0 && !SRP_get_default_gN(gN)) {
        BIO_printf(bio_err, "No g and N value for index \"%s\"\n", gN);
        goto err;
    } else {
        VERBOSE BIO_printf(bio_err, "Database has no g N information.\n");
        gNrow = NULL;
    }

    VVERBOSE BIO_printf(bio_err, "Starting user processing\n");

    if (argc > 0)
        user = *(argv++);

    while (list_user || user) {
        int userindex = -1;
        if (user)
            VVERBOSE BIO_printf(bio_err, "Processing user \"%s\"\n", user);
        if ((userindex = get_index(db, user, 'U')) >= 0) {
            print_user(db, bio_err, userindex, (verbose > 0) || list_user);
        }

        if (list_user) {
            if (user == NULL) {
                BIO_printf(bio_err, "List all users\n");

                for (i = 0; i < sk_OPENSSL_PSTRING_num(db->db->data); i++) {
                    print_user(db, bio_err, i, 1);
                }
                list_user = 0;
            } else if (userindex < 0) {
                BIO_printf(bio_err,
                           "user \"%s\" does not exist, ignored. t\n", user);
                errors++;
            }
        } else if (add_user) {
            if (userindex >= 0) {
                /* reactivation of a new user */
                char **row =
                    sk_OPENSSL_PSTRING_value(db->db->data, userindex);
                BIO_printf(bio_err, "user \"%s\" reactivated.\n", user);
                row[DB_srptype][0] = 'V';

                doupdatedb = 1;
            } else {
                char *row[DB_NUMBER];
                char *gNid;
                row[DB_srpverifier] = NULL;
                row[DB_srpsalt] = NULL;
                row[DB_srpinfo] = NULL;
                if (!
                    (gNid =
                     srp_create_user(user, &(row[DB_srpverifier]),
                                     &(row[DB_srpsalt]),
                                     gNrow ? gNrow[DB_srpsalt] : gN,
                                     gNrow ? gNrow[DB_srpverifier] : NULL,
                                     passout, bio_err, verbose))) {
                    BIO_printf(bio_err,
                               "Cannot create srp verifier for user \"%s\", operation abandoned .\n",
                               user);
                    errors++;
                    goto err;
                }
                row[DB_srpid] = BUF_strdup(user);
                row[DB_srptype] = BUF_strdup("v");
                row[DB_srpgN] = BUF_strdup(gNid);

                if (!row[DB_srpid] || !row[DB_srpgN] || !row[DB_srptype]
                    || !row[DB_srpverifier] || !row[DB_srpsalt] || (userinfo
                                                                    &&
                                                                    (!(row
                                                                       [DB_srpinfo]
                                                                       =
                                                                       BUF_strdup
                                                                       (userinfo))))
                    || !update_index(db, bio_err, row)) {
                    if (row[DB_srpid])
                        OPENSSL_free(row[DB_srpid]);
                    if (row[DB_srpgN])
                        OPENSSL_free(row[DB_srpgN]);
                    if (row[DB_srpinfo])
                        OPENSSL_free(row[DB_srpinfo]);
                    if (row[DB_srptype])
                        OPENSSL_free(row[DB_srptype]);
                    if (row[DB_srpverifier])
                        OPENSSL_free(row[DB_srpverifier]);
                    if (row[DB_srpsalt])
                        OPENSSL_free(row[DB_srpsalt]);
                    goto err;
                }
                doupdatedb = 1;
            }
        } else if (modify_user) {
            if (userindex < 0) {
                BIO_printf(bio_err,
                           "user \"%s\" does not exist, operation ignored.\n",
                           user);
                errors++;
            } else {

                char **row =
                    sk_OPENSSL_PSTRING_value(db->db->data, userindex);
                char type = row[DB_srptype][0];
                if (type == 'v') {
                    BIO_printf(bio_err,
                               "user \"%s\" already updated, operation ignored.\n",
                               user);
                    errors++;
                } else {
                    char *gNid;

                    if (row[DB_srptype][0] == 'V') {
                        int user_gN;
                        char **irow = NULL;
                        VERBOSE BIO_printf(bio_err,
                                           "Verifying password for user \"%s\"\n",
                                           user);
                        if ((user_gN =
                             get_index(db, row[DB_srpgN], DB_SRP_INDEX)) >= 0)
                            irow =
                                (char **)sk_OPENSSL_PSTRING_value(db->
                                                                  db->data,
                                                                  userindex);

                        if (!srp_verify_user
                            (user, row[DB_srpverifier], row[DB_srpsalt],
                             irow ? irow[DB_srpsalt] : row[DB_srpgN],
                             irow ? irow[DB_srpverifier] : NULL, passin,
                             bio_err, verbose)) {
                            BIO_printf(bio_err,
                                       "Invalid password for user \"%s\", operation abandoned.\n",
                                       user);
                            errors++;
                            goto err;
                        }
                    }
                    VERBOSE BIO_printf(bio_err,
                                       "Password for user \"%s\" ok.\n",
                                       user);

                    if (!
                        (gNid =
                         srp_create_user(user, &(row[DB_srpverifier]),
                                         &(row[DB_srpsalt]),
                                         gNrow ? gNrow[DB_srpsalt] : NULL,
                                         gNrow ? gNrow[DB_srpverifier] : NULL,
                                         passout, bio_err, verbose))) {
                        BIO_printf(bio_err,
                                   "Cannot create srp verifier for user \"%s\", operation abandoned.\n",
                                   user);
                        errors++;
                        goto err;
                    }

                    row[DB_srptype][0] = 'v';
                    row[DB_srpgN] = BUF_strdup(gNid);

                    if (!row[DB_srpid] || !row[DB_srpgN] || !row[DB_srptype]
                        || !row[DB_srpverifier] || !row[DB_srpsalt]
                        || (userinfo
                            && (!(row[DB_srpinfo] = BUF_strdup(userinfo)))))
                        goto err;

                    doupdatedb = 1;
                }
            }
        } else if (delete_user) {
            if (userindex < 0) {
                BIO_printf(bio_err,
                           "user \"%s\" does not exist, operation ignored. t\n",
                           user);
                errors++;
            } else {
                char **xpp =
                    sk_OPENSSL_PSTRING_value(db->db->data, userindex);
                BIO_printf(bio_err, "user \"%s\" revoked. t\n", user);

                xpp[DB_srptype][0] = 'R';

                doupdatedb = 1;
            }
        }
        if (--argc > 0) {
            user = *(argv++);
        } else {
            user = NULL;
            list_user = 0;
        }
    }

    VERBOSE BIO_printf(bio_err, "User procession done.\n");

    if (doupdatedb) {
        /* Lets check some fields */
        for (i = 0; i < sk_OPENSSL_PSTRING_num(db->db->data); i++) {
            pp = sk_OPENSSL_PSTRING_value(db->db->data, i);

            if (pp[DB_srptype][0] == 'v') {
                pp[DB_srptype][0] = 'V';
                print_user(db, bio_err, i, verbose);
            }
        }

        VERBOSE BIO_printf(bio_err, "Trying to update srpvfile.\n");
        if (!save_index(dbfile, "new", db))
            goto err;

        VERBOSE BIO_printf(bio_err, "Temporary srpvfile created.\n");
        if (!rotate_index(dbfile, "new", "old"))
            goto err;

        VERBOSE BIO_printf(bio_err, "srpvfile updated.\n");
    }

    ret = (errors != 0);
 err:
    if (errors != 0)
        VERBOSE BIO_printf(bio_err, "User errors %d.\n", errors);

    VERBOSE BIO_printf(bio_err, "SRP terminating with code %d.\n", ret);
    if (tofree)
        OPENSSL_free(tofree);
    if (ret)
        ERR_print_errors(bio_err);
    if (randfile)
        app_RAND_write_file(randfile, bio_err);
    if (conf)
        NCONF_free(conf);
    if (db)
        free_index(db);

    release_engine(e);
    OBJ_cleanup();
    apps_shutdown();
    OPENSSL_EXIT(ret);
}

#else
static void *dummy = &dummy;
#endif
