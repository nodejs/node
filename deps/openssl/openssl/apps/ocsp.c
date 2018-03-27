/* ocsp.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2000.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
#ifndef OPENSSL_NO_OCSP

# ifdef OPENSSL_SYS_VMS
#  define _XOPEN_SOURCE_EXTENDED/* So fd_set and friends get properly defined
                                 * on OpenVMS */
# endif

# define USE_SOCKETS

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include "apps.h"              /* needs to be included before the openssl
                                 * headers! */
# include <openssl/e_os2.h>
# include <openssl/crypto.h>
# include <openssl/err.h>
# include <openssl/ssl.h>
# include <openssl/evp.h>
# include <openssl/bn.h>
# include <openssl/x509v3.h>

# if defined(NETWARE_CLIB)
#  ifdef NETWARE_BSDSOCK
#   include <sys/socket.h>
#   include <sys/bsdskt.h>
#  else
#   include <novsock2.h>
#  endif
# elif defined(NETWARE_LIBC)
#  ifdef NETWARE_BSDSOCK
#   include <sys/select.h>
#  else
#   include <novsock2.h>
#  endif
# endif

/* Maximum leeway in validity period: default 5 minutes */
# define MAX_VALIDITY_PERIOD     (5 * 60)

static int add_ocsp_cert(OCSP_REQUEST **req, X509 *cert,
                         const EVP_MD *cert_id_md, X509 *issuer,
                         STACK_OF(OCSP_CERTID) *ids);
static int add_ocsp_serial(OCSP_REQUEST **req, char *serial,
                           const EVP_MD *cert_id_md, X509 *issuer,
                           STACK_OF(OCSP_CERTID) *ids);
static int print_ocsp_summary(BIO *out, OCSP_BASICRESP *bs, OCSP_REQUEST *req,
                              STACK_OF(OPENSSL_STRING) *names,
                              STACK_OF(OCSP_CERTID) *ids, long nsec,
                              long maxage);

static int make_ocsp_response(OCSP_RESPONSE **resp, OCSP_REQUEST *req,
                              CA_DB *db, X509 *ca, X509 *rcert,
                              EVP_PKEY *rkey, const EVP_MD *md,
                              STACK_OF(X509) *rother, unsigned long flags,
                              int nmin, int ndays, int badsig);

static char **lookup_serial(CA_DB *db, ASN1_INTEGER *ser);
static BIO *init_responder(const char *port);
static int do_responder(OCSP_REQUEST **preq, BIO **pcbio, BIO *acbio,
                        const char *port);
static int send_ocsp_response(BIO *cbio, OCSP_RESPONSE *resp);
static OCSP_RESPONSE *query_responder(BIO *err, BIO *cbio, const char *path,
                                      const STACK_OF(CONF_VALUE) *headers,
                                      OCSP_REQUEST *req, int req_timeout);

# undef PROG
# define PROG ocsp_main

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
    ENGINE *e = NULL;
    char **args;
    char *host = NULL, *port = NULL, *path = "/";
    char *thost = NULL, *tport = NULL, *tpath = NULL;
    char *reqin = NULL, *respin = NULL;
    char *reqout = NULL, *respout = NULL;
    char *signfile = NULL, *keyfile = NULL;
    char *rsignfile = NULL, *rkeyfile = NULL;
    char *outfile = NULL;
    int add_nonce = 1, noverify = 0, use_ssl = -1;
    STACK_OF(CONF_VALUE) *headers = NULL;
    OCSP_REQUEST *req = NULL;
    OCSP_RESPONSE *resp = NULL;
    OCSP_BASICRESP *bs = NULL;
    X509 *issuer = NULL, *cert = NULL;
    X509 *signer = NULL, *rsigner = NULL;
    EVP_PKEY *key = NULL, *rkey = NULL;
    BIO *acbio = NULL, *cbio = NULL;
    BIO *derbio = NULL;
    BIO *out = NULL;
    int req_timeout = -1;
    int req_text = 0, resp_text = 0;
    long nsec = MAX_VALIDITY_PERIOD, maxage = -1;
    char *CAfile = NULL, *CApath = NULL;
    X509_STORE *store = NULL;
    X509_VERIFY_PARAM *vpm = NULL;
    STACK_OF(X509) *sign_other = NULL, *verify_other = NULL, *rother = NULL;
    char *sign_certfile = NULL, *verify_certfile = NULL, *rcertfile = NULL;
    unsigned long sign_flags = 0, verify_flags = 0, rflags = 0;
    int ret = 1;
    int accept_count = -1;
    int badarg = 0;
    int badsig = 0;
    int i;
    int ignore_err = 0;
    STACK_OF(OPENSSL_STRING) *reqnames = NULL;
    STACK_OF(OCSP_CERTID) *ids = NULL;

    X509 *rca_cert = NULL;
    char *ridx_filename = NULL;
    char *rca_filename = NULL;
    CA_DB *rdb = NULL;
    int nmin = 0, ndays = -1;
    const EVP_MD *cert_id_md = NULL, *rsign_md = NULL;

    if (bio_err == NULL)
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

    if (!load_config(bio_err, NULL))
        goto end;
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    args = argv + 1;
    reqnames = sk_OPENSSL_STRING_new_null();
    ids = sk_OCSP_CERTID_new_null();
    while (!badarg && *args && *args[0] == '-') {
        if (!strcmp(*args, "-out")) {
            if (args[1]) {
                args++;
                outfile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-timeout")) {
            if (args[1]) {
                args++;
                req_timeout = atol(*args);
                if (req_timeout < 0) {
                    BIO_printf(bio_err, "Illegal timeout value %s\n", *args);
                    badarg = 1;
                }
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-url")) {
            if (thost)
                OPENSSL_free(thost);
            if (tport)
                OPENSSL_free(tport);
            if (tpath)
                OPENSSL_free(tpath);
            thost = tport = tpath = NULL;
            if (args[1]) {
                args++;
                if (!OCSP_parse_url(*args, &host, &port, &path, &use_ssl)) {
                    BIO_printf(bio_err, "Error parsing URL\n");
                    badarg = 1;
                }
                thost = host;
                tport = port;
                tpath = path;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-host")) {
            if (args[1]) {
                args++;
                host = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-port")) {
            if (args[1]) {
                args++;
                port = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-header")) {
            if (args[1] && args[2]) {
                if (!X509V3_add_value(args[1], args[2], &headers))
                    goto end;
                args += 2;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-ignore_err"))
            ignore_err = 1;
        else if (!strcmp(*args, "-noverify"))
            noverify = 1;
        else if (!strcmp(*args, "-nonce"))
            add_nonce = 2;
        else if (!strcmp(*args, "-no_nonce"))
            add_nonce = 0;
        else if (!strcmp(*args, "-resp_no_certs"))
            rflags |= OCSP_NOCERTS;
        else if (!strcmp(*args, "-resp_key_id"))
            rflags |= OCSP_RESPID_KEY;
        else if (!strcmp(*args, "-no_certs"))
            sign_flags |= OCSP_NOCERTS;
        else if (!strcmp(*args, "-no_signature_verify"))
            verify_flags |= OCSP_NOSIGS;
        else if (!strcmp(*args, "-no_cert_verify"))
            verify_flags |= OCSP_NOVERIFY;
        else if (!strcmp(*args, "-no_chain"))
            verify_flags |= OCSP_NOCHAIN;
        else if (!strcmp(*args, "-no_cert_checks"))
            verify_flags |= OCSP_NOCHECKS;
        else if (!strcmp(*args, "-no_explicit"))
            verify_flags |= OCSP_NOEXPLICIT;
        else if (!strcmp(*args, "-trust_other"))
            verify_flags |= OCSP_TRUSTOTHER;
        else if (!strcmp(*args, "-no_intern"))
            verify_flags |= OCSP_NOINTERN;
        else if (!strcmp(*args, "-badsig"))
            badsig = 1;
        else if (!strcmp(*args, "-text")) {
            req_text = 1;
            resp_text = 1;
        } else if (!strcmp(*args, "-req_text"))
            req_text = 1;
        else if (!strcmp(*args, "-resp_text"))
            resp_text = 1;
        else if (!strcmp(*args, "-reqin")) {
            if (args[1]) {
                args++;
                reqin = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-respin")) {
            if (args[1]) {
                args++;
                respin = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-signer")) {
            if (args[1]) {
                args++;
                signfile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-VAfile")) {
            if (args[1]) {
                args++;
                verify_certfile = *args;
                verify_flags |= OCSP_TRUSTOTHER;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-sign_other")) {
            if (args[1]) {
                args++;
                sign_certfile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-verify_other")) {
            if (args[1]) {
                args++;
                verify_certfile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-CAfile")) {
            if (args[1]) {
                args++;
                CAfile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-CApath")) {
            if (args[1]) {
                args++;
                CApath = *args;
            } else
                badarg = 1;
        } else if (args_verify(&args, NULL, &badarg, bio_err, &vpm)) {
            if (badarg)
                goto end;
            continue;
        } else if (!strcmp(*args, "-validity_period")) {
            if (args[1]) {
                args++;
                nsec = atol(*args);
                if (nsec < 0) {
                    BIO_printf(bio_err,
                               "Illegal validity period %s\n", *args);
                    badarg = 1;
                }
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-status_age")) {
            if (args[1]) {
                args++;
                maxage = atol(*args);
                if (maxage < 0) {
                    BIO_printf(bio_err, "Illegal validity age %s\n", *args);
                    badarg = 1;
                }
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-signkey")) {
            if (args[1]) {
                args++;
                keyfile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-reqout")) {
            if (args[1]) {
                args++;
                reqout = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-respout")) {
            if (args[1]) {
                args++;
                respout = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-path")) {
            if (args[1]) {
                args++;
                path = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-issuer")) {
            if (args[1]) {
                args++;
                X509_free(issuer);
                issuer = load_cert(bio_err, *args, FORMAT_PEM,
                                   NULL, e, "issuer certificate");
                if (!issuer)
                    goto end;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-cert")) {
            if (args[1]) {
                args++;
                X509_free(cert);
                cert = load_cert(bio_err, *args, FORMAT_PEM,
                                 NULL, e, "certificate");
                if (!cert)
                    goto end;
                if (!cert_id_md)
                    cert_id_md = EVP_sha1();
                if (!add_ocsp_cert(&req, cert, cert_id_md, issuer, ids))
                    goto end;
                if (!sk_OPENSSL_STRING_push(reqnames, *args))
                    goto end;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-serial")) {
            if (args[1]) {
                args++;
                if (!cert_id_md)
                    cert_id_md = EVP_sha1();
                if (!add_ocsp_serial(&req, *args, cert_id_md, issuer, ids))
                    goto end;
                if (!sk_OPENSSL_STRING_push(reqnames, *args))
                    goto end;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-index")) {
            if (args[1]) {
                args++;
                ridx_filename = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-CA")) {
            if (args[1]) {
                args++;
                rca_filename = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-nmin")) {
            if (args[1]) {
                args++;
                nmin = atol(*args);
                if (nmin < 0) {
                    BIO_printf(bio_err, "Illegal update period %s\n", *args);
                    badarg = 1;
                }
            }
            if (ndays == -1)
                ndays = 0;
            else
                badarg = 1;
        } else if (!strcmp(*args, "-nrequest")) {
            if (args[1]) {
                args++;
                accept_count = atol(*args);
                if (accept_count < 0) {
                    BIO_printf(bio_err, "Illegal accept count %s\n", *args);
                    badarg = 1;
                }
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-ndays")) {
            if (args[1]) {
                args++;
                ndays = atol(*args);
                if (ndays < 0) {
                    BIO_printf(bio_err, "Illegal update period %s\n", *args);
                    badarg = 1;
                }
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-rsigner")) {
            if (args[1]) {
                args++;
                rsignfile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-rkey")) {
            if (args[1]) {
                args++;
                rkeyfile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-rother")) {
            if (args[1]) {
                args++;
                rcertfile = *args;
            } else
                badarg = 1;
        } else if (!strcmp(*args, "-rmd")) {
            if (args[1]) {
                args++;
                rsign_md = EVP_get_digestbyname(*args);
                if (!rsign_md)
                    badarg = 1;
            } else
                badarg = 1;
        } else if ((cert_id_md = EVP_get_digestbyname((*args) + 1)) == NULL) {
            badarg = 1;
        }
        args++;
    }

    /* Have we anything to do? */
    if (!req && !reqin && !respin && !(port && ridx_filename))
        badarg = 1;

    if (badarg) {
        BIO_printf(bio_err, "OCSP utility\n");
        BIO_printf(bio_err, "Usage ocsp [options]\n");
        BIO_printf(bio_err, "where options are\n");
        BIO_printf(bio_err, "-out file            output filename\n");
        BIO_printf(bio_err, "-issuer file         issuer certificate\n");
        BIO_printf(bio_err, "-cert file           certificate to check\n");
        BIO_printf(bio_err, "-serial n            serial number to check\n");
        BIO_printf(bio_err,
                   "-signer file         certificate to sign OCSP request with\n");
        BIO_printf(bio_err,
                   "-signkey file        private key to sign OCSP request with\n");
        BIO_printf(bio_err,
                   "-sign_other file     additional certificates to include in signed request\n");
        BIO_printf(bio_err,
                   "-no_certs            don't include any certificates in signed request\n");
        BIO_printf(bio_err,
                   "-req_text            print text form of request\n");
        BIO_printf(bio_err,
                   "-resp_text           print text form of response\n");
        BIO_printf(bio_err,
                   "-text                print text form of request and response\n");
        BIO_printf(bio_err,
                   "-reqout file         write DER encoded OCSP request to \"file\"\n");
        BIO_printf(bio_err,
                   "-respout file        write DER encoded OCSP reponse to \"file\"\n");
        BIO_printf(bio_err,
                   "-reqin file          read DER encoded OCSP request from \"file\"\n");
        BIO_printf(bio_err,
                   "-respin file         read DER encoded OCSP reponse from \"file\"\n");
        BIO_printf(bio_err,
                   "-nonce               add OCSP nonce to request\n");
        BIO_printf(bio_err,
                   "-no_nonce            don't add OCSP nonce to request\n");
        BIO_printf(bio_err, "-url URL             OCSP responder URL\n");
        BIO_printf(bio_err,
                   "-host host:n         send OCSP request to host on port n\n");
        BIO_printf(bio_err,
                   "-path                path to use in OCSP request\n");
        BIO_printf(bio_err,
                   "-CApath dir          trusted certificates directory\n");
        BIO_printf(bio_err,
                   "-CAfile file         trusted certificates file\n");
        BIO_printf(bio_err,
                   "-no_alt_chains       only ever use the first certificate chain found\n");
        BIO_printf(bio_err,
                   "-VAfile file         validator certificates file\n");
        BIO_printf(bio_err,
                   "-validity_period n   maximum validity discrepancy in seconds\n");
        BIO_printf(bio_err,
                   "-status_age n        maximum status age in seconds\n");
        BIO_printf(bio_err,
                   "-noverify            don't verify response at all\n");
        BIO_printf(bio_err,
                   "-verify_other file   additional certificates to search for signer\n");
        BIO_printf(bio_err,
                   "-trust_other         don't verify additional certificates\n");
        BIO_printf(bio_err,
                   "-no_intern           don't search certificates contained in response for signer\n");
        BIO_printf(bio_err,
                   "-no_signature_verify don't check signature on response\n");
        BIO_printf(bio_err,
                   "-no_cert_verify      don't check signing certificate\n");
        BIO_printf(bio_err,
                   "-no_chain            don't chain verify response\n");
        BIO_printf(bio_err,
                   "-no_cert_checks      don't do additional checks on signing certificate\n");
        BIO_printf(bio_err,
                   "-port num            port to run responder on\n");
        BIO_printf(bio_err,
                   "-index file          certificate status index file\n");
        BIO_printf(bio_err, "-CA file             CA certificate\n");
        BIO_printf(bio_err,
                   "-rsigner file        responder certificate to sign responses with\n");
        BIO_printf(bio_err,
                   "-rkey file           responder key to sign responses with\n");
        BIO_printf(bio_err,
                   "-rother file         other certificates to include in response\n");
        BIO_printf(bio_err,
                   "-resp_no_certs       don't include any certificates in response\n");
        BIO_printf(bio_err,
                   "-nmin n              number of minutes before next update\n");
        BIO_printf(bio_err,
                   "-ndays n             number of days before next update\n");
        BIO_printf(bio_err,
                   "-resp_key_id         identify reponse by signing certificate key ID\n");
        BIO_printf(bio_err,
                   "-nrequest n          number of requests to accept (default unlimited)\n");
        BIO_printf(bio_err,
                   "-<dgst alg>          use specified digest in the request\n");
        BIO_printf(bio_err,
                   "-timeout n           timeout connection to OCSP responder after n seconds\n");
        goto end;
    }

    if (outfile)
        out = BIO_new_file(outfile, "w");
    else
        out = BIO_new_fp(stdout, BIO_NOCLOSE);

    if (!out) {
        BIO_printf(bio_err, "Error opening output file\n");
        goto end;
    }

    if (!req && (add_nonce != 2))
        add_nonce = 0;

    if (!req && reqin) {
        if (!strcmp(reqin, "-"))
            derbio = BIO_new_fp(stdin, BIO_NOCLOSE);
        else
            derbio = BIO_new_file(reqin, "rb");
        if (!derbio) {
            BIO_printf(bio_err, "Error Opening OCSP request file\n");
            goto end;
        }
        req = d2i_OCSP_REQUEST_bio(derbio, NULL);
        BIO_free(derbio);
        if (!req) {
            BIO_printf(bio_err, "Error reading OCSP request\n");
            goto end;
        }
    }

    if (!req && port) {
        acbio = init_responder(port);
        if (!acbio)
            goto end;
    }

    if (rsignfile && !rdb) {
        if (!rkeyfile)
            rkeyfile = rsignfile;
        rsigner = load_cert(bio_err, rsignfile, FORMAT_PEM,
                            NULL, e, "responder certificate");
        if (!rsigner) {
            BIO_printf(bio_err, "Error loading responder certificate\n");
            goto end;
        }
        rca_cert = load_cert(bio_err, rca_filename, FORMAT_PEM,
                             NULL, e, "CA certificate");
        if (rcertfile) {
            rother = load_certs(bio_err, rcertfile, FORMAT_PEM,
                                NULL, e, "responder other certificates");
            if (!rother)
                goto end;
        }
        rkey = load_key(bio_err, rkeyfile, FORMAT_PEM, 0, NULL, NULL,
                        "responder private key");
        if (!rkey)
            goto end;
    }
    if (acbio)
        BIO_printf(bio_err, "Waiting for OCSP client connections...\n");

 redo_accept:

    if (acbio) {
        if (!do_responder(&req, &cbio, acbio, port))
            goto end;
        if (!req) {
            resp =
                OCSP_response_create(OCSP_RESPONSE_STATUS_MALFORMEDREQUEST,
                                     NULL);
            send_ocsp_response(cbio, resp);
            goto done_resp;
        }
    }

    if (!req && (signfile || reqout || host || add_nonce || ridx_filename)) {
        BIO_printf(bio_err, "Need an OCSP request for this operation!\n");
        goto end;
    }

    if (req && add_nonce)
        OCSP_request_add1_nonce(req, NULL, -1);

    if (signfile) {
        if (!keyfile)
            keyfile = signfile;
        signer = load_cert(bio_err, signfile, FORMAT_PEM,
                           NULL, e, "signer certificate");
        if (!signer) {
            BIO_printf(bio_err, "Error loading signer certificate\n");
            goto end;
        }
        if (sign_certfile) {
            sign_other = load_certs(bio_err, sign_certfile, FORMAT_PEM,
                                    NULL, e, "signer certificates");
            if (!sign_other)
                goto end;
        }
        key = load_key(bio_err, keyfile, FORMAT_PEM, 0, NULL, NULL,
                       "signer private key");
        if (!key)
            goto end;

        if (!OCSP_request_sign
            (req, signer, key, NULL, sign_other, sign_flags)) {
            BIO_printf(bio_err, "Error signing OCSP request\n");
            goto end;
        }
    }

    if (req_text && req)
        OCSP_REQUEST_print(out, req, 0);

    if (reqout) {
        if (!strcmp(reqout, "-"))
            derbio = BIO_new_fp(stdout, BIO_NOCLOSE);
        else
            derbio = BIO_new_file(reqout, "wb");
        if (!derbio) {
            BIO_printf(bio_err, "Error opening file %s\n", reqout);
            goto end;
        }
        i2d_OCSP_REQUEST_bio(derbio, req);
        BIO_free(derbio);
    }

    if (ridx_filename && (!rkey || !rsigner || !rca_cert)) {
        BIO_printf(bio_err,
                   "Need a responder certificate, key and CA for this operation!\n");
        goto end;
    }

    if (ridx_filename && !rdb) {
        rdb = load_index(ridx_filename, NULL);
        if (!rdb)
            goto end;
        if (!index_index(rdb))
            goto end;
    }

    if (rdb) {
        i = make_ocsp_response(&resp, req, rdb, rca_cert, rsigner, rkey,
                               rsign_md, rother, rflags, nmin, ndays, badsig);
        if (cbio)
            send_ocsp_response(cbio, resp);
    } else if (host) {
# ifndef OPENSSL_NO_SOCK
        resp = process_responder(bio_err, req, host, path,
                                 port, use_ssl, headers, req_timeout);
        if (!resp)
            goto end;
# else
        BIO_printf(bio_err,
                   "Error creating connect BIO - sockets not supported.\n");
        goto end;
# endif
    } else if (respin) {
        if (!strcmp(respin, "-"))
            derbio = BIO_new_fp(stdin, BIO_NOCLOSE);
        else
            derbio = BIO_new_file(respin, "rb");
        if (!derbio) {
            BIO_printf(bio_err, "Error Opening OCSP response file\n");
            goto end;
        }
        resp = d2i_OCSP_RESPONSE_bio(derbio, NULL);
        BIO_free(derbio);
        if (!resp) {
            BIO_printf(bio_err, "Error reading OCSP response\n");
            goto end;
        }

    } else {
        ret = 0;
        goto end;
    }

 done_resp:

    if (respout) {
        if (!strcmp(respout, "-"))
            derbio = BIO_new_fp(stdout, BIO_NOCLOSE);
        else
            derbio = BIO_new_file(respout, "wb");
        if (!derbio) {
            BIO_printf(bio_err, "Error opening file %s\n", respout);
            goto end;
        }
        i2d_OCSP_RESPONSE_bio(derbio, resp);
        BIO_free(derbio);
    }

    i = OCSP_response_status(resp);

    if (i != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
        BIO_printf(out, "Responder Error: %s (%d)\n",
                   OCSP_response_status_str(i), i);
        if (ignore_err)
            goto redo_accept;
        ret = 0;
        goto end;
    }

    if (resp_text)
        OCSP_RESPONSE_print(out, resp, 0);

    /* If running as responder don't verify our own response */
    if (cbio) {
        if (accept_count > 0)
            accept_count--;
        /* Redo if more connections needed */
        if (accept_count) {
            BIO_free_all(cbio);
            cbio = NULL;
            OCSP_REQUEST_free(req);
            req = NULL;
            OCSP_RESPONSE_free(resp);
            resp = NULL;
            goto redo_accept;
        }
        ret = 0;
        goto end;
    } else if (ridx_filename) {
        ret = 0;
        goto end;
    }

    if (!store)
        store = setup_verify(bio_err, CAfile, CApath);
    if (!store)
        goto end;
    if (vpm)
        X509_STORE_set1_param(store, vpm);
    if (verify_certfile) {
        verify_other = load_certs(bio_err, verify_certfile, FORMAT_PEM,
                                  NULL, e, "validator certificate");
        if (!verify_other)
            goto end;
    }

    bs = OCSP_response_get1_basic(resp);

    if (!bs) {
        BIO_printf(bio_err, "Error parsing response\n");
        goto end;
    }

    ret = 0;

    if (!noverify) {
        if (req && ((i = OCSP_check_nonce(req, bs)) <= 0)) {
            if (i == -1)
                BIO_printf(bio_err, "WARNING: no nonce in response\n");
            else {
                BIO_printf(bio_err, "Nonce Verify error\n");
                ret = 1;
                goto end;
            }
        }

        i = OCSP_basic_verify(bs, verify_other, store, verify_flags);
        if (i <= 0) {
            BIO_printf(bio_err, "Response Verify Failure\n");
            ERR_print_errors(bio_err);
            ret = 1;
        } else
            BIO_printf(bio_err, "Response verify OK\n");

    }

    if (!print_ocsp_summary(out, bs, req, reqnames, ids, nsec, maxage))
        ret = 1;

 end:
    ERR_print_errors(bio_err);
    X509_free(signer);
    X509_STORE_free(store);
    if (vpm)
        X509_VERIFY_PARAM_free(vpm);
    EVP_PKEY_free(key);
    EVP_PKEY_free(rkey);
    X509_free(issuer);
    X509_free(cert);
    X509_free(rsigner);
    X509_free(rca_cert);
    free_index(rdb);
    BIO_free_all(cbio);
    BIO_free_all(acbio);
    BIO_free(out);
    OCSP_REQUEST_free(req);
    OCSP_RESPONSE_free(resp);
    OCSP_BASICRESP_free(bs);
    sk_OPENSSL_STRING_free(reqnames);
    sk_OCSP_CERTID_free(ids);
    sk_X509_pop_free(sign_other, X509_free);
    sk_X509_pop_free(verify_other, X509_free);
    sk_CONF_VALUE_pop_free(headers, X509V3_conf_free);

    if (thost)
        OPENSSL_free(thost);
    if (tport)
        OPENSSL_free(tport);
    if (tpath)
        OPENSSL_free(tpath);

    OPENSSL_EXIT(ret);
}

static int add_ocsp_cert(OCSP_REQUEST **req, X509 *cert,
                         const EVP_MD *cert_id_md, X509 *issuer,
                         STACK_OF(OCSP_CERTID) *ids)
{
    OCSP_CERTID *id;
    if (!issuer) {
        BIO_printf(bio_err, "No issuer certificate specified\n");
        return 0;
    }
    if (!*req)
        *req = OCSP_REQUEST_new();
    if (!*req)
        goto err;
    id = OCSP_cert_to_id(cert_id_md, cert, issuer);
    if (!id || !sk_OCSP_CERTID_push(ids, id))
        goto err;
    if (!OCSP_request_add0_id(*req, id))
        goto err;
    return 1;

 err:
    BIO_printf(bio_err, "Error Creating OCSP request\n");
    return 0;
}

static int add_ocsp_serial(OCSP_REQUEST **req, char *serial,
                           const EVP_MD *cert_id_md, X509 *issuer,
                           STACK_OF(OCSP_CERTID) *ids)
{
    OCSP_CERTID *id;
    X509_NAME *iname;
    ASN1_BIT_STRING *ikey;
    ASN1_INTEGER *sno;
    if (!issuer) {
        BIO_printf(bio_err, "No issuer certificate specified\n");
        return 0;
    }
    if (!*req)
        *req = OCSP_REQUEST_new();
    if (!*req)
        goto err;
    iname = X509_get_subject_name(issuer);
    ikey = X509_get0_pubkey_bitstr(issuer);
    sno = s2i_ASN1_INTEGER(NULL, serial);
    if (!sno) {
        BIO_printf(bio_err, "Error converting serial number %s\n", serial);
        return 0;
    }
    id = OCSP_cert_id_new(cert_id_md, iname, ikey, sno);
    ASN1_INTEGER_free(sno);
    if (!id || !sk_OCSP_CERTID_push(ids, id))
        goto err;
    if (!OCSP_request_add0_id(*req, id))
        goto err;
    return 1;

 err:
    BIO_printf(bio_err, "Error Creating OCSP request\n");
    return 0;
}

static int print_ocsp_summary(BIO *out, OCSP_BASICRESP *bs, OCSP_REQUEST *req,
                              STACK_OF(OPENSSL_STRING) *names,
                              STACK_OF(OCSP_CERTID) *ids, long nsec,
                              long maxage)
{
    OCSP_CERTID *id;
    char *name;
    int i;

    int status, reason;

    ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;

    if (!bs || !req || !sk_OPENSSL_STRING_num(names)
        || !sk_OCSP_CERTID_num(ids))
        return 1;

    for (i = 0; i < sk_OCSP_CERTID_num(ids); i++) {
        id = sk_OCSP_CERTID_value(ids, i);
        name = sk_OPENSSL_STRING_value(names, i);
        BIO_printf(out, "%s: ", name);

        if (!OCSP_resp_find_status(bs, id, &status, &reason,
                                   &rev, &thisupd, &nextupd)) {
            BIO_puts(out, "ERROR: No Status found.\n");
            continue;
        }

        /*
         * Check validity: if invalid write to output BIO so we know which
         * response this refers to.
         */
        if (!OCSP_check_validity(thisupd, nextupd, nsec, maxage)) {
            BIO_puts(out, "WARNING: Status times invalid.\n");
            ERR_print_errors(out);
        }
        BIO_printf(out, "%s\n", OCSP_cert_status_str(status));

        BIO_puts(out, "\tThis Update: ");
        ASN1_GENERALIZEDTIME_print(out, thisupd);
        BIO_puts(out, "\n");

        if (nextupd) {
            BIO_puts(out, "\tNext Update: ");
            ASN1_GENERALIZEDTIME_print(out, nextupd);
            BIO_puts(out, "\n");
        }

        if (status != V_OCSP_CERTSTATUS_REVOKED)
            continue;

        if (reason != -1)
            BIO_printf(out, "\tReason: %s\n", OCSP_crl_reason_str(reason));

        BIO_puts(out, "\tRevocation Time: ");
        ASN1_GENERALIZEDTIME_print(out, rev);
        BIO_puts(out, "\n");
    }

    return 1;
}

static int make_ocsp_response(OCSP_RESPONSE **resp, OCSP_REQUEST *req,
                              CA_DB *db, X509 *ca, X509 *rcert,
                              EVP_PKEY *rkey, const EVP_MD *rmd,
                              STACK_OF(X509) *rother, unsigned long flags,
                              int nmin, int ndays, int badsig)
{
    ASN1_TIME *thisupd = NULL, *nextupd = NULL;
    OCSP_CERTID *cid, *ca_id = NULL;
    OCSP_BASICRESP *bs = NULL;
    int i, id_count, ret = 1;

    id_count = OCSP_request_onereq_count(req);

    if (id_count <= 0) {
        *resp =
            OCSP_response_create(OCSP_RESPONSE_STATUS_MALFORMEDREQUEST, NULL);
        goto end;
    }

    bs = OCSP_BASICRESP_new();
    thisupd = X509_gmtime_adj(NULL, 0);
    if (ndays != -1)
        nextupd = X509_time_adj_ex(NULL, ndays, nmin * 60, NULL);

    /* Examine each certificate id in the request */
    for (i = 0; i < id_count; i++) {
        OCSP_ONEREQ *one;
        ASN1_INTEGER *serial;
        char **inf;
        ASN1_OBJECT *cert_id_md_oid;
        const EVP_MD *cert_id_md;
        one = OCSP_request_onereq_get0(req, i);
        cid = OCSP_onereq_get0_id(one);

        OCSP_id_get0_info(NULL, &cert_id_md_oid, NULL, NULL, cid);

        cert_id_md = EVP_get_digestbyobj(cert_id_md_oid);
        if (!cert_id_md) {
            *resp = OCSP_response_create(OCSP_RESPONSE_STATUS_INTERNALERROR,
                                         NULL);
            goto end;
        }
        if (ca_id)
            OCSP_CERTID_free(ca_id);
        ca_id = OCSP_cert_to_id(cert_id_md, NULL, ca);

        /* Is this request about our CA? */
        if (OCSP_id_issuer_cmp(ca_id, cid)) {
            OCSP_basic_add1_status(bs, cid,
                                   V_OCSP_CERTSTATUS_UNKNOWN,
                                   0, NULL, thisupd, nextupd);
            continue;
        }
        OCSP_id_get0_info(NULL, NULL, NULL, &serial, cid);
        inf = lookup_serial(db, serial);
        if (!inf)
            OCSP_basic_add1_status(bs, cid,
                                   V_OCSP_CERTSTATUS_UNKNOWN,
                                   0, NULL, thisupd, nextupd);
        else if (inf[DB_type][0] == DB_TYPE_VAL)
            OCSP_basic_add1_status(bs, cid,
                                   V_OCSP_CERTSTATUS_GOOD,
                                   0, NULL, thisupd, nextupd);
        else if (inf[DB_type][0] == DB_TYPE_REV) {
            ASN1_OBJECT *inst = NULL;
            ASN1_TIME *revtm = NULL;
            ASN1_GENERALIZEDTIME *invtm = NULL;
            OCSP_SINGLERESP *single;
            int reason = -1;
            unpack_revinfo(&revtm, &reason, &inst, &invtm, inf[DB_rev_date]);
            single = OCSP_basic_add1_status(bs, cid,
                                            V_OCSP_CERTSTATUS_REVOKED,
                                            reason, revtm, thisupd, nextupd);
            if (invtm)
                OCSP_SINGLERESP_add1_ext_i2d(single, NID_invalidity_date,
                                             invtm, 0, 0);
            else if (inst)
                OCSP_SINGLERESP_add1_ext_i2d(single,
                                             NID_hold_instruction_code, inst,
                                             0, 0);
            ASN1_OBJECT_free(inst);
            ASN1_TIME_free(revtm);
            ASN1_GENERALIZEDTIME_free(invtm);
        }
    }

    OCSP_copy_nonce(bs, req);

    OCSP_basic_sign(bs, rcert, rkey, rmd, rother, flags);

    if (badsig)
        bs->signature->data[bs->signature->length - 1] ^= 0x1;

    *resp = OCSP_response_create(OCSP_RESPONSE_STATUS_SUCCESSFUL, bs);

 end:
    ASN1_TIME_free(thisupd);
    ASN1_TIME_free(nextupd);
    OCSP_CERTID_free(ca_id);
    OCSP_BASICRESP_free(bs);
    return ret;

}

static char **lookup_serial(CA_DB *db, ASN1_INTEGER *ser)
{
    int i;
    BIGNUM *bn = NULL;
    char *itmp, *row[DB_NUMBER], **rrow;
    for (i = 0; i < DB_NUMBER; i++)
        row[i] = NULL;
    bn = ASN1_INTEGER_to_BN(ser, NULL);
    OPENSSL_assert(bn);         /* FIXME: should report an error at this
                                 * point and abort */
    if (BN_is_zero(bn))
        itmp = BUF_strdup("00");
    else
        itmp = BN_bn2hex(bn);
    row[DB_serial] = itmp;
    BN_free(bn);
    rrow = TXT_DB_get_by_index(db->db, DB_serial, row);
    OPENSSL_free(itmp);
    return rrow;
}

/* Quick and dirty OCSP server: read in and parse input request */

static BIO *init_responder(const char *port)
{
    BIO *acbio = NULL, *bufbio = NULL;
    bufbio = BIO_new(BIO_f_buffer());
    if (!bufbio)
        goto err;
# ifndef OPENSSL_NO_SOCK
    acbio = BIO_new_accept(port);
# else
    BIO_printf(bio_err,
               "Error setting up accept BIO - sockets not supported.\n");
# endif
    if (!acbio)
        goto err;
    BIO_set_accept_bios(acbio, bufbio);
    bufbio = NULL;

    if (BIO_do_accept(acbio) <= 0) {
        BIO_printf(bio_err, "Error setting up accept BIO\n");
        ERR_print_errors(bio_err);
        goto err;
    }

    return acbio;

 err:
    BIO_free_all(acbio);
    BIO_free(bufbio);
    return NULL;
}

static int do_responder(OCSP_REQUEST **preq, BIO **pcbio, BIO *acbio,
                        const char *port)
{
    int have_post = 0, len;
    OCSP_REQUEST *req = NULL;
    char inbuf[1024];
    BIO *cbio = NULL;

    if (BIO_do_accept(acbio) <= 0) {
        BIO_printf(bio_err, "Error accepting connection\n");
        ERR_print_errors(bio_err);
        return 0;
    }

    cbio = BIO_pop(acbio);
    *pcbio = cbio;

    for (;;) {
        len = BIO_gets(cbio, inbuf, sizeof(inbuf));
        if (len <= 0)
            return 1;
        /* Look for "POST" signalling start of query */
        if (!have_post) {
            if (strncmp(inbuf, "POST", 4)) {
                BIO_printf(bio_err, "Invalid request\n");
                return 1;
            }
            have_post = 1;
        }
        /* Look for end of headers */
        if ((inbuf[0] == '\r') || (inbuf[0] == '\n'))
            break;
    }

    /* Try to read OCSP request */

    req = d2i_OCSP_REQUEST_bio(cbio, NULL);

    if (!req) {
        BIO_printf(bio_err, "Error parsing OCSP request\n");
        ERR_print_errors(bio_err);
    }

    *preq = req;

    return 1;

}

static int send_ocsp_response(BIO *cbio, OCSP_RESPONSE *resp)
{
    char http_resp[] =
        "HTTP/1.0 200 OK\r\nContent-type: application/ocsp-response\r\n"
        "Content-Length: %d\r\n\r\n";
    if (!cbio)
        return 0;
    BIO_printf(cbio, http_resp, i2d_OCSP_RESPONSE(resp, NULL));
    i2d_OCSP_RESPONSE_bio(cbio, resp);
    (void)BIO_flush(cbio);
    return 1;
}

static OCSP_RESPONSE *query_responder(BIO *err, BIO *cbio, const char *path,
                                      const STACK_OF(CONF_VALUE) *headers,
                                      OCSP_REQUEST *req, int req_timeout)
{
    int fd;
    int rv;
    int i;
    OCSP_REQ_CTX *ctx = NULL;
    OCSP_RESPONSE *rsp = NULL;
    fd_set confds;
    struct timeval tv;

    if (req_timeout != -1)
        BIO_set_nbio(cbio, 1);

    rv = BIO_do_connect(cbio);

    if ((rv <= 0) && ((req_timeout == -1) || !BIO_should_retry(cbio))) {
        BIO_puts(err, "Error connecting BIO\n");
        return NULL;
    }

    if (BIO_get_fd(cbio, &fd) < 0) {
        BIO_puts(bio_err, "Can't get connection fd\n");
        goto err;
    }

    if (req_timeout != -1 && rv <= 0) {
        FD_ZERO(&confds);
        openssl_fdset(fd, &confds);
        tv.tv_usec = 0;
        tv.tv_sec = req_timeout;
        rv = select(fd + 1, NULL, (void *)&confds, NULL, &tv);
        if (rv == 0) {
            BIO_puts(err, "Timeout on connect\n");
            return NULL;
        }
    }

    ctx = OCSP_sendreq_new(cbio, path, NULL, -1);
    if (!ctx)
        return NULL;

    for (i = 0; i < sk_CONF_VALUE_num(headers); i++) {
        CONF_VALUE *hdr = sk_CONF_VALUE_value(headers, i);
        if (!OCSP_REQ_CTX_add1_header(ctx, hdr->name, hdr->value))
            goto err;
    }

    if (!OCSP_REQ_CTX_set1_req(ctx, req))
        goto err;

    for (;;) {
        rv = OCSP_sendreq_nbio(&rsp, ctx);
        if (rv != -1)
            break;
        if (req_timeout == -1)
            continue;
        FD_ZERO(&confds);
        openssl_fdset(fd, &confds);
        tv.tv_usec = 0;
        tv.tv_sec = req_timeout;
        if (BIO_should_read(cbio))
            rv = select(fd + 1, (void *)&confds, NULL, NULL, &tv);
        else if (BIO_should_write(cbio))
            rv = select(fd + 1, NULL, (void *)&confds, NULL, &tv);
        else {
            BIO_puts(err, "Unexpected retry condition\n");
            goto err;
        }
        if (rv == 0) {
            BIO_puts(err, "Timeout on request\n");
            break;
        }
        if (rv == -1) {
            BIO_puts(err, "Select error\n");
            break;
        }

    }
 err:
    if (ctx)
        OCSP_REQ_CTX_free(ctx);

    return rsp;
}

OCSP_RESPONSE *process_responder(BIO *err, OCSP_REQUEST *req,
                                 const char *host, const char *path,
                                 const char *port, int use_ssl,
                                 const STACK_OF(CONF_VALUE) *headers,
                                 int req_timeout)
{
    BIO *cbio = NULL;
    SSL_CTX *ctx = NULL;
    OCSP_RESPONSE *resp = NULL;
    cbio = BIO_new_connect(host);
    if (!cbio) {
        BIO_printf(err, "Error creating connect BIO\n");
        goto end;
    }
    if (port)
        BIO_set_conn_port(cbio, port);
    if (use_ssl == 1) {
        BIO *sbio;
        ctx = SSL_CTX_new(SSLv23_client_method());
        if (ctx == NULL) {
            BIO_printf(err, "Error creating SSL context.\n");
            goto end;
        }
        SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
        sbio = BIO_new_ssl(ctx, 1);
        cbio = BIO_push(sbio, cbio);
    }
    resp = query_responder(err, cbio, path, headers, req, req_timeout);
    if (!resp)
        BIO_printf(bio_err, "Error querying OCSP responder\n");
 end:
    if (cbio)
        BIO_free_all(cbio);
    if (ctx)
        SSL_CTX_free(ctx);
    return resp;
}

#endif
