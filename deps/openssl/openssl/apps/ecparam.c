/* apps/ecparam.c */
/*
 * Written by Nils Larsch for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

#include <openssl/opensslconf.h>
#ifndef OPENSSL_NO_EC
# include <assert.h>
# include <stdio.h>
# include <stdlib.h>
# include <time.h>
# include <string.h>
# include "apps.h"
# include <openssl/bio.h>
# include <openssl/err.h>
# include <openssl/bn.h>
# include <openssl/ec.h>
# include <openssl/x509.h>
# include <openssl/pem.h>

# undef PROG
# define PROG    ecparam_main

/*-
 * -inform arg      - input format - default PEM (DER or PEM)
 * -outform arg     - output format - default PEM
 * -in  arg         - input file  - default stdin
 * -out arg         - output file - default stdout
 * -noout           - do not print the ec parameter
 * -text            - print the ec parameters in text form
 * -check           - validate the ec parameters
 * -C               - print a 'C' function creating the parameters
 * -name arg        - use the ec parameters with 'short name' name
 * -list_curves     - prints a list of all currently available curve 'short names'
 * -conv_form arg   - specifies the point conversion form
 *                  - possible values: compressed
 *                                     uncompressed (default)
 *                                     hybrid
 * -param_enc arg   - specifies the way the ec parameters are encoded
 *                    in the asn1 der encoding
 *                    possible values: named_curve (default)
 *                                     explicit
 * -no_seed         - if 'explicit' parameters are chosen do not use the seed
 * -genkey          - generate ec key
 * -rand file       - files to use for random number input
 * -engine e        - use engine e, possibly a hardware device
 */

static int ecparam_print_var(BIO *, BIGNUM *, const char *, int,
                             unsigned char *);

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    EC_GROUP *group = NULL;
    point_conversion_form_t form = POINT_CONVERSION_UNCOMPRESSED;
    int new_form = 0;
    int asn1_flag = OPENSSL_EC_NAMED_CURVE;
    int new_asn1_flag = 0;
    char *curve_name = NULL, *inrand = NULL;
    int list_curves = 0, no_seed = 0, check = 0,
        badops = 0, text = 0, i, need_rand = 0, genkey = 0;
    char *infile = NULL, *outfile = NULL, *prog;
    BIO *in = NULL, *out = NULL;
    int informat, outformat, noout = 0, C = 0, ret = 1;
    char *engine = NULL;
    ENGINE *e = NULL;

    BIGNUM *ec_p = NULL, *ec_a = NULL, *ec_b = NULL,
        *ec_gen = NULL, *ec_order = NULL, *ec_cofactor = NULL;
    unsigned char *buffer = NULL;

    apps_startup();

    if (bio_err == NULL)
        if ((bio_err = BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_err, stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    if (!load_config(bio_err, NULL))
        goto end;

    informat = FORMAT_PEM;
    outformat = FORMAT_PEM;

    prog = argv[0];
    argc--;
    argv++;
    while (argc >= 1) {
        if (strcmp(*argv, "-inform") == 0) {
            if (--argc < 1)
                goto bad;
            informat = str2fmt(*(++argv));
        } else if (strcmp(*argv, "-outform") == 0) {
            if (--argc < 1)
                goto bad;
            outformat = str2fmt(*(++argv));
        } else if (strcmp(*argv, "-in") == 0) {
            if (--argc < 1)
                goto bad;
            infile = *(++argv);
        } else if (strcmp(*argv, "-out") == 0) {
            if (--argc < 1)
                goto bad;
            outfile = *(++argv);
        } else if (strcmp(*argv, "-text") == 0)
            text = 1;
        else if (strcmp(*argv, "-C") == 0)
            C = 1;
        else if (strcmp(*argv, "-check") == 0)
            check = 1;
        else if (strcmp(*argv, "-name") == 0) {
            if (--argc < 1)
                goto bad;
            curve_name = *(++argv);
        } else if (strcmp(*argv, "-list_curves") == 0)
            list_curves = 1;
        else if (strcmp(*argv, "-conv_form") == 0) {
            if (--argc < 1)
                goto bad;
            ++argv;
            new_form = 1;
            if (strcmp(*argv, "compressed") == 0)
                form = POINT_CONVERSION_COMPRESSED;
            else if (strcmp(*argv, "uncompressed") == 0)
                form = POINT_CONVERSION_UNCOMPRESSED;
            else if (strcmp(*argv, "hybrid") == 0)
                form = POINT_CONVERSION_HYBRID;
            else
                goto bad;
        } else if (strcmp(*argv, "-param_enc") == 0) {
            if (--argc < 1)
                goto bad;
            ++argv;
            new_asn1_flag = 1;
            if (strcmp(*argv, "named_curve") == 0)
                asn1_flag = OPENSSL_EC_NAMED_CURVE;
            else if (strcmp(*argv, "explicit") == 0)
                asn1_flag = 0;
            else
                goto bad;
        } else if (strcmp(*argv, "-no_seed") == 0)
            no_seed = 1;
        else if (strcmp(*argv, "-noout") == 0)
            noout = 1;
        else if (strcmp(*argv, "-genkey") == 0) {
            genkey = 1;
            need_rand = 1;
        } else if (strcmp(*argv, "-rand") == 0) {
            if (--argc < 1)
                goto bad;
            inrand = *(++argv);
            need_rand = 1;
        } else if (strcmp(*argv, "-engine") == 0) {
            if (--argc < 1)
                goto bad;
            engine = *(++argv);
        } else {
            BIO_printf(bio_err, "unknown option %s\n", *argv);
            badops = 1;
            break;
        }
        argc--;
        argv++;
    }

    if (badops) {
 bad:
        BIO_printf(bio_err, "%s [options] <infile >outfile\n", prog);
        BIO_printf(bio_err, "where options are\n");
        BIO_printf(bio_err, " -inform arg       input format - "
                   "default PEM (DER or PEM)\n");
        BIO_printf(bio_err, " -outform arg      output format - "
                   "default PEM\n");
        BIO_printf(bio_err, " -in  arg          input file  - "
                   "default stdin\n");
        BIO_printf(bio_err, " -out arg          output file - "
                   "default stdout\n");
        BIO_printf(bio_err, " -noout            do not print the "
                   "ec parameter\n");
        BIO_printf(bio_err, " -text             print the ec "
                   "parameters in text form\n");
        BIO_printf(bio_err, " -check            validate the ec "
                   "parameters\n");
        BIO_printf(bio_err, " -C                print a 'C' "
                   "function creating the parameters\n");
        BIO_printf(bio_err, " -name arg         use the "
                   "ec parameters with 'short name' name\n");
        BIO_printf(bio_err, " -list_curves      prints a list of "
                   "all currently available curve 'short names'\n");
        BIO_printf(bio_err, " -conv_form arg    specifies the "
                   "point conversion form \n");
        BIO_printf(bio_err, "                   possible values:"
                   " compressed\n");
        BIO_printf(bio_err, "                                   "
                   " uncompressed (default)\n");
        BIO_printf(bio_err, "                                   "
                   " hybrid\n");
        BIO_printf(bio_err, " -param_enc arg    specifies the way"
                   " the ec parameters are encoded\n");
        BIO_printf(bio_err, "                   in the asn1 der "
                   "encoding\n");
        BIO_printf(bio_err, "                   possible values:"
                   " named_curve (default)\n");
        BIO_printf(bio_err, "                                   "
                   " explicit\n");
        BIO_printf(bio_err, " -no_seed          if 'explicit'"
                   " parameters are chosen do not" " use the seed\n");
        BIO_printf(bio_err, " -genkey           generate ec" " key\n");
        BIO_printf(bio_err, " -rand file        files to use for"
                   " random number input\n");
        BIO_printf(bio_err, " -engine e         use engine e, "
                   "possibly a hardware device\n");
        goto end;
    }

    ERR_load_crypto_strings();

    in = BIO_new(BIO_s_file());
    out = BIO_new(BIO_s_file());
    if ((in == NULL) || (out == NULL)) {
        ERR_print_errors(bio_err);
        goto end;
    }

    if (infile == NULL)
        BIO_set_fp(in, stdin, BIO_NOCLOSE);
    else {
        if (BIO_read_filename(in, infile) <= 0) {
            perror(infile);
            goto end;
        }
    }
    if (outfile == NULL) {
        BIO_set_fp(out, stdout, BIO_NOCLOSE);
# ifdef OPENSSL_SYS_VMS
        {
            BIO *tmpbio = BIO_new(BIO_f_linebuffer());
            out = BIO_push(tmpbio, out);
        }
# endif
    } else {
        if (BIO_write_filename(out, outfile) <= 0) {
            perror(outfile);
            goto end;
        }
    }

    e = setup_engine(bio_err, engine, 0);

    if (list_curves) {
        EC_builtin_curve *curves = NULL;
        size_t crv_len = 0;
        size_t n = 0;

        crv_len = EC_get_builtin_curves(NULL, 0);

        curves = OPENSSL_malloc((int)(sizeof(EC_builtin_curve) * crv_len));

        if (curves == NULL)
            goto end;

        if (!EC_get_builtin_curves(curves, crv_len)) {
            OPENSSL_free(curves);
            goto end;
        }

        for (n = 0; n < crv_len; n++) {
            const char *comment;
            const char *sname;
            comment = curves[n].comment;
            sname = OBJ_nid2sn(curves[n].nid);
            if (comment == NULL)
                comment = "CURVE DESCRIPTION NOT AVAILABLE";
            if (sname == NULL)
                sname = "";

            BIO_printf(out, "  %-10s: ", sname);
            BIO_printf(out, "%s\n", comment);
        }

        OPENSSL_free(curves);
        ret = 0;
        goto end;
    }

    if (curve_name != NULL) {
        int nid;

        /*
         * workaround for the SECG curve names secp192r1 and secp256r1 (which
         * are the same as the curves prime192v1 and prime256v1 defined in
         * X9.62)
         */
        if (!strcmp(curve_name, "secp192r1")) {
            BIO_printf(bio_err, "using curve name prime192v1 "
                       "instead of secp192r1\n");
            nid = NID_X9_62_prime192v1;
        } else if (!strcmp(curve_name, "secp256r1")) {
            BIO_printf(bio_err, "using curve name prime256v1 "
                       "instead of secp256r1\n");
            nid = NID_X9_62_prime256v1;
        } else
            nid = OBJ_sn2nid(curve_name);

        if (nid == 0)
            nid = EC_curve_nist2nid(curve_name);

        if (nid == 0) {
            BIO_printf(bio_err, "unknown curve name (%s)\n", curve_name);
            goto end;
        }

        group = EC_GROUP_new_by_curve_name(nid);
        if (group == NULL) {
            BIO_printf(bio_err, "unable to create curve (%s)\n", curve_name);
            goto end;
        }
        EC_GROUP_set_asn1_flag(group, asn1_flag);
        EC_GROUP_set_point_conversion_form(group, form);
    } else if (informat == FORMAT_ASN1) {
        group = d2i_ECPKParameters_bio(in, NULL);
    } else if (informat == FORMAT_PEM) {
        group = PEM_read_bio_ECPKParameters(in, NULL, NULL, NULL);
    } else {
        BIO_printf(bio_err, "bad input format specified\n");
        goto end;
    }

    if (group == NULL) {
        BIO_printf(bio_err, "unable to load elliptic curve parameters\n");
        ERR_print_errors(bio_err);
        goto end;
    }

    if (new_form)
        EC_GROUP_set_point_conversion_form(group, form);

    if (new_asn1_flag)
        EC_GROUP_set_asn1_flag(group, asn1_flag);

    if (no_seed) {
        EC_GROUP_set_seed(group, NULL, 0);
    }

    if (text) {
        if (!ECPKParameters_print(out, group, 0))
            goto end;
    }

    if (check) {
        BIO_printf(bio_err, "checking elliptic curve parameters: ");
        if (!EC_GROUP_check(group, NULL)) {
            BIO_printf(bio_err, "failed\n");
            ERR_print_errors(bio_err);
            goto end;
        }
        BIO_printf(bio_err, "ok\n");

    }

    if (C) {
        size_t buf_len = 0, tmp_len = 0;
        const EC_POINT *point;
        int is_prime, len = 0;
        const EC_METHOD *meth = EC_GROUP_method_of(group);

        if ((ec_p = BN_new()) == NULL || (ec_a = BN_new()) == NULL ||
            (ec_b = BN_new()) == NULL || (ec_gen = BN_new()) == NULL ||
            (ec_order = BN_new()) == NULL ||
            (ec_cofactor = BN_new()) == NULL) {
            perror("OPENSSL_malloc");
            goto end;
        }

        is_prime = (EC_METHOD_get_field_type(meth) == NID_X9_62_prime_field);

        if (is_prime) {
            if (!EC_GROUP_get_curve_GFp(group, ec_p, ec_a, ec_b, NULL))
                goto end;
        } else {
            /* TODO */
            goto end;
        }

        if ((point = EC_GROUP_get0_generator(group)) == NULL)
            goto end;
        if (!EC_POINT_point2bn(group, point,
                               EC_GROUP_get_point_conversion_form(group),
                               ec_gen, NULL))
            goto end;
        if (!EC_GROUP_get_order(group, ec_order, NULL))
            goto end;
        if (!EC_GROUP_get_cofactor(group, ec_cofactor, NULL))
            goto end;

        if (!ec_p || !ec_a || !ec_b || !ec_gen || !ec_order || !ec_cofactor)
            goto end;

        len = BN_num_bits(ec_order);

        if ((tmp_len = (size_t)BN_num_bytes(ec_p)) > buf_len)
            buf_len = tmp_len;
        if ((tmp_len = (size_t)BN_num_bytes(ec_a)) > buf_len)
            buf_len = tmp_len;
        if ((tmp_len = (size_t)BN_num_bytes(ec_b)) > buf_len)
            buf_len = tmp_len;
        if ((tmp_len = (size_t)BN_num_bytes(ec_gen)) > buf_len)
            buf_len = tmp_len;
        if ((tmp_len = (size_t)BN_num_bytes(ec_order)) > buf_len)
            buf_len = tmp_len;
        if ((tmp_len = (size_t)BN_num_bytes(ec_cofactor)) > buf_len)
            buf_len = tmp_len;

        buffer = (unsigned char *)OPENSSL_malloc(buf_len);

        if (buffer == NULL) {
            perror("OPENSSL_malloc");
            goto end;
        }

        ecparam_print_var(out, ec_p, "ec_p", len, buffer);
        ecparam_print_var(out, ec_a, "ec_a", len, buffer);
        ecparam_print_var(out, ec_b, "ec_b", len, buffer);
        ecparam_print_var(out, ec_gen, "ec_gen", len, buffer);
        ecparam_print_var(out, ec_order, "ec_order", len, buffer);
        ecparam_print_var(out, ec_cofactor, "ec_cofactor", len, buffer);

        BIO_printf(out, "\n\n");

        BIO_printf(out, "EC_GROUP *get_ec_group_%d(void)\n\t{\n", len);
        BIO_printf(out, "\tint ok=0;\n");
        BIO_printf(out, "\tEC_GROUP *group = NULL;\n");
        BIO_printf(out, "\tEC_POINT *point = NULL;\n");
        BIO_printf(out, "\tBIGNUM   *tmp_1 = NULL, *tmp_2 = NULL, "
                   "*tmp_3 = NULL;\n\n");
        BIO_printf(out, "\tif ((tmp_1 = BN_bin2bn(ec_p_%d, "
                   "sizeof(ec_p_%d), NULL)) == NULL)\n\t\t"
                   "goto err;\n", len, len);
        BIO_printf(out, "\tif ((tmp_2 = BN_bin2bn(ec_a_%d, "
                   "sizeof(ec_a_%d), NULL)) == NULL)\n\t\t"
                   "goto err;\n", len, len);
        BIO_printf(out, "\tif ((tmp_3 = BN_bin2bn(ec_b_%d, "
                   "sizeof(ec_b_%d), NULL)) == NULL)\n\t\t"
                   "goto err;\n", len, len);
        if (is_prime) {
            BIO_printf(out, "\tif ((group = EC_GROUP_new_curve_"
                       "GFp(tmp_1, tmp_2, tmp_3, NULL)) == NULL)"
                       "\n\t\tgoto err;\n\n");
        } else {
            /* TODO */
            goto end;
        }
        BIO_printf(out, "\t/* build generator */\n");
        BIO_printf(out, "\tif ((tmp_1 = BN_bin2bn(ec_gen_%d, "
                   "sizeof(ec_gen_%d), tmp_1)) == NULL)"
                   "\n\t\tgoto err;\n", len, len);
        BIO_printf(out, "\tpoint = EC_POINT_bn2point(group, tmp_1, "
                   "NULL, NULL);\n");
        BIO_printf(out, "\tif (point == NULL)\n\t\tgoto err;\n");
        BIO_printf(out, "\tif ((tmp_2 = BN_bin2bn(ec_order_%d, "
                   "sizeof(ec_order_%d), tmp_2)) == NULL)"
                   "\n\t\tgoto err;\n", len, len);
        BIO_printf(out, "\tif ((tmp_3 = BN_bin2bn(ec_cofactor_%d, "
                   "sizeof(ec_cofactor_%d), tmp_3)) == NULL)"
                   "\n\t\tgoto err;\n", len, len);
        BIO_printf(out, "\tif (!EC_GROUP_set_generator(group, point,"
                   " tmp_2, tmp_3))\n\t\tgoto err;\n");
        BIO_printf(out, "\n\tok=1;\n");
        BIO_printf(out, "err:\n");
        BIO_printf(out, "\tif (tmp_1)\n\t\tBN_free(tmp_1);\n");
        BIO_printf(out, "\tif (tmp_2)\n\t\tBN_free(tmp_2);\n");
        BIO_printf(out, "\tif (tmp_3)\n\t\tBN_free(tmp_3);\n");
        BIO_printf(out, "\tif (point)\n\t\tEC_POINT_free(point);\n");
        BIO_printf(out, "\tif (!ok)\n");
        BIO_printf(out, "\t\t{\n");
        BIO_printf(out, "\t\tEC_GROUP_free(group);\n");
        BIO_printf(out, "\t\tgroup = NULL;\n");
        BIO_printf(out, "\t\t}\n");
        BIO_printf(out, "\treturn(group);\n\t}\n");
    }

    if (!noout) {
        if (outformat == FORMAT_ASN1)
            i = i2d_ECPKParameters_bio(out, group);
        else if (outformat == FORMAT_PEM)
            i = PEM_write_bio_ECPKParameters(out, group);
        else {
            BIO_printf(bio_err, "bad output format specified for"
                       " outfile\n");
            goto end;
        }
        if (!i) {
            BIO_printf(bio_err, "unable to write elliptic "
                       "curve parameters\n");
            ERR_print_errors(bio_err);
            goto end;
        }
    }

    if (need_rand) {
        app_RAND_load_file(NULL, bio_err, (inrand != NULL));
        if (inrand != NULL)
            BIO_printf(bio_err, "%ld semi-random bytes loaded\n",
                       app_RAND_load_files(inrand));
    }

    if (genkey) {
        EC_KEY *eckey = EC_KEY_new();

        if (eckey == NULL)
            goto end;

        assert(need_rand);

        if (EC_KEY_set_group(eckey, group) == 0)
            goto end;

        if (!EC_KEY_generate_key(eckey)) {
            EC_KEY_free(eckey);
            goto end;
        }
        if (outformat == FORMAT_ASN1)
            i = i2d_ECPrivateKey_bio(out, eckey);
        else if (outformat == FORMAT_PEM)
            i = PEM_write_bio_ECPrivateKey(out, eckey, NULL,
                                           NULL, 0, NULL, NULL);
        else {
            BIO_printf(bio_err, "bad output format specified "
                       "for outfile\n");
            EC_KEY_free(eckey);
            goto end;
        }
        EC_KEY_free(eckey);
    }

    if (need_rand)
        app_RAND_write_file(NULL, bio_err);

    ret = 0;
 end:
    if (ec_p)
        BN_free(ec_p);
    if (ec_a)
        BN_free(ec_a);
    if (ec_b)
        BN_free(ec_b);
    if (ec_gen)
        BN_free(ec_gen);
    if (ec_order)
        BN_free(ec_order);
    if (ec_cofactor)
        BN_free(ec_cofactor);
    if (buffer)
        OPENSSL_free(buffer);
    if (group != NULL)
        EC_GROUP_free(group);
    release_engine(e);
    if (in != NULL)
        BIO_free(in);
    if (out != NULL)
        BIO_free_all(out);
    apps_shutdown();
    OPENSSL_EXIT(ret);
}

static int ecparam_print_var(BIO *out, BIGNUM *in, const char *var,
                             int len, unsigned char *buffer)
{
    BIO_printf(out, "static unsigned char %s_%d[] = {", var, len);
    if (BN_is_zero(in))
        BIO_printf(out, "\n\t0x00");
    else {
        int i, l;

        l = BN_bn2bin(in, buffer);
        for (i = 0; i < l - 1; i++) {
            if ((i % 12) == 0)
                BIO_printf(out, "\n\t");
            BIO_printf(out, "0x%02X,", buffer[i]);
        }
        if ((i % 12) == 0)
            BIO_printf(out, "\n\t");
        BIO_printf(out, "0x%02X", buffer[i]);
    }
    BIO_printf(out, "\n\t};\n\n");
    return 1;
}
#else                           /* !OPENSSL_NO_EC */

# if PEDANTIC
static void *dummy = &dummy;
# endif

#endif
