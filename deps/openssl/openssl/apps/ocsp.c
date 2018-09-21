/*
 * Copyright 2001-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/opensslconf.h>

#ifdef OPENSSL_NO_OCSP
NON_EMPTY_TRANSLATION_UNIT
#else
# ifdef OPENSSL_SYS_VMS
#  define _XOPEN_SOURCE_EXTENDED/* So fd_set and friends get properly defined
                                 * on OpenVMS */
# endif

# define USE_SOCKETS

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <ctype.h>

/* Needs to be included before the openssl headers */
# include "apps.h"
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
# define MAX_VALIDITY_PERIOD    (5 * 60)

static int add_ocsp_cert(OCSP_REQUEST **req, X509 *cert,
                         const EVP_MD *cert_id_md, X509 *issuer,
                         STACK_OF(OCSP_CERTID) *ids);
static int add_ocsp_serial(OCSP_REQUEST **req, char *serial,
                           const EVP_MD *cert_id_md, X509 *issuer,
                           STACK_OF(OCSP_CERTID) *ids);
static void print_ocsp_summary(BIO *out, OCSP_BASICRESP *bs, OCSP_REQUEST *req,
                              STACK_OF(OPENSSL_STRING) *names,
                              STACK_OF(OCSP_CERTID) *ids, long nsec,
                              long maxage);
static void make_ocsp_response(OCSP_RESPONSE **resp, OCSP_REQUEST *req,
                              CA_DB *db, X509 *ca, X509 *rcert,
                              EVP_PKEY *rkey, const EVP_MD *md,
                              STACK_OF(X509) *rother, unsigned long flags,
                              int nmin, int ndays, int badsig);

static char **lookup_serial(CA_DB *db, ASN1_INTEGER *ser);
static BIO *init_responder(const char *port);
static int do_responder(OCSP_REQUEST **preq, BIO **pcbio, BIO *acbio);
static int send_ocsp_response(BIO *cbio, OCSP_RESPONSE *resp);

# ifndef OPENSSL_NO_SOCK
static OCSP_RESPONSE *query_responder(BIO *cbio, const char *host,
                                      const char *path,
                                      const STACK_OF(CONF_VALUE) *headers,
                                      OCSP_REQUEST *req, int req_timeout);
# endif

typedef enum OPTION_choice {
    OPT_ERR = -1, OPT_EOF = 0, OPT_HELP,
    OPT_OUTFILE, OPT_TIMEOUT, OPT_URL, OPT_HOST, OPT_PORT,
    OPT_IGNORE_ERR, OPT_NOVERIFY, OPT_NONCE, OPT_NO_NONCE,
    OPT_RESP_NO_CERTS, OPT_RESP_KEY_ID, OPT_NO_CERTS,
    OPT_NO_SIGNATURE_VERIFY, OPT_NO_CERT_VERIFY, OPT_NO_CHAIN,
    OPT_NO_CERT_CHECKS, OPT_NO_EXPLICIT, OPT_TRUST_OTHER,
    OPT_NO_INTERN, OPT_BADSIG, OPT_TEXT, OPT_REQ_TEXT, OPT_RESP_TEXT,
    OPT_REQIN, OPT_RESPIN, OPT_SIGNER, OPT_VAFILE, OPT_SIGN_OTHER,
    OPT_VERIFY_OTHER, OPT_CAFILE, OPT_CAPATH, OPT_NOCAFILE, OPT_NOCAPATH,
    OPT_VALIDITY_PERIOD, OPT_STATUS_AGE, OPT_SIGNKEY, OPT_REQOUT,
    OPT_RESPOUT, OPT_PATH, OPT_ISSUER, OPT_CERT, OPT_SERIAL,
    OPT_INDEX, OPT_CA, OPT_NMIN, OPT_REQUEST, OPT_NDAYS, OPT_RSIGNER,
    OPT_RKEY, OPT_ROTHER, OPT_RMD, OPT_HEADER,
    OPT_V_ENUM,
    OPT_MD
} OPTION_CHOICE;

OPTIONS ocsp_options[] = {
    {"help", OPT_HELP, '-', "Display this summary"},
    {"out", OPT_OUTFILE, '>', "Output filename"},
    {"timeout", OPT_TIMEOUT, 'p',
     "Connection timeout (in seconds) to the OCSP responder"},
    {"url", OPT_URL, 's', "Responder URL"},
    {"host", OPT_HOST, 's', "TCP/IP hostname:port to connect to"},
    {"port", OPT_PORT, 'p', "Port to run responder on"},
    {"ignore_err", OPT_IGNORE_ERR, '-',
     "Ignore Error response from OCSP responder, and retry "},
    {"noverify", OPT_NOVERIFY, '-', "Don't verify response at all"},
    {"nonce", OPT_NONCE, '-', "Add OCSP nonce to request"},
    {"no_nonce", OPT_NO_NONCE, '-', "Don't add OCSP nonce to request"},
    {"resp_no_certs", OPT_RESP_NO_CERTS, '-',
     "Don't include any certificates in response"},
    {"resp_key_id", OPT_RESP_KEY_ID, '-',
     "Identify response by signing certificate key ID"},
    {"no_certs", OPT_NO_CERTS, '-',
     "Don't include any certificates in signed request"},
    {"no_signature_verify", OPT_NO_SIGNATURE_VERIFY, '-',
     "Don't check signature on response"},
    {"no_cert_verify", OPT_NO_CERT_VERIFY, '-',
     "Don't check signing certificate"},
    {"no_chain", OPT_NO_CHAIN, '-', "Don't chain verify response"},
    {"no_cert_checks", OPT_NO_CERT_CHECKS, '-',
     "Don't do additional checks on signing certificate"},
    {"no_explicit", OPT_NO_EXPLICIT, '-',
     "Do not explicitly check the chain, just verify the root"},
    {"trust_other", OPT_TRUST_OTHER, '-',
     "Don't verify additional certificates"},
    {"no_intern", OPT_NO_INTERN, '-',
     "Don't search certificates contained in response for signer"},
    {"badsig", OPT_BADSIG, '-',
        "Corrupt last byte of loaded OSCP response signature (for test)"},
    {"text", OPT_TEXT, '-', "Print text form of request and response"},
    {"req_text", OPT_REQ_TEXT, '-', "Print text form of request"},
    {"resp_text", OPT_RESP_TEXT, '-', "Print text form of response"},
    {"reqin", OPT_REQIN, 's', "File with the DER-encoded request"},
    {"respin", OPT_RESPIN, 's', "File with the DER-encoded response"},
    {"signer", OPT_SIGNER, '<', "Certificate to sign OCSP request with"},
    {"VAfile", OPT_VAFILE, '<', "Validator certificates file"},
    {"sign_other", OPT_SIGN_OTHER, '<',
     "Additional certificates to include in signed request"},
    {"verify_other", OPT_VERIFY_OTHER, '<',
     "Additional certificates to search for signer"},
    {"CAfile", OPT_CAFILE, '<', "Trusted certificates file"},
    {"CApath", OPT_CAPATH, '<', "Trusted certificates directory"},
    {"no-CAfile", OPT_NOCAFILE, '-',
     "Do not load the default certificates file"},
    {"no-CApath", OPT_NOCAPATH, '-',
     "Do not load certificates from the default certificates directory"},
    {"validity_period", OPT_VALIDITY_PERIOD, 'u',
     "Maximum validity discrepancy in seconds"},
    {"status_age", OPT_STATUS_AGE, 'p', "Maximum status age in seconds"},
    {"signkey", OPT_SIGNKEY, 's', "Private key to sign OCSP request with"},
    {"reqout", OPT_REQOUT, 's', "Output file for the DER-encoded request"},
    {"respout", OPT_RESPOUT, 's', "Output file for the DER-encoded response"},
    {"path", OPT_PATH, 's', "Path to use in OCSP request"},
    {"issuer", OPT_ISSUER, '<', "Issuer certificate"},
    {"cert", OPT_CERT, '<', "Certificate to check"},
    {"serial", OPT_SERIAL, 's', "Serial number to check"},
    {"index", OPT_INDEX, '<', "Certificate status index file"},
    {"CA", OPT_CA, '<', "CA certificate"},
    {"nmin", OPT_NMIN, 'p', "Number of minutes before next update"},
    {"nrequest", OPT_REQUEST, 'p',
     "Number of requests to accept (default unlimited)"},
    {"ndays", OPT_NDAYS, 'p', "Number of days before next update"},
    {"rsigner", OPT_RSIGNER, '<',
     "Responder certificate to sign responses with"},
    {"rkey", OPT_RKEY, '<', "Responder key to sign responses with"},
    {"rother", OPT_ROTHER, '<', "Other certificates to include in response"},
    {"rmd", OPT_RMD, 's', "Digest Algorithm to use in signature of OCSP response"},
    {"header", OPT_HEADER, 's', "key=value header to add"},
    {"", OPT_MD, '-', "Any supported digest algorithm (sha1,sha256, ... )"},
    OPT_V_OPTIONS,
    {NULL}
};

int ocsp_main(int argc, char **argv)
{
    BIO *acbio = NULL, *cbio = NULL, *derbio = NULL, *out = NULL;
    const EVP_MD *cert_id_md = NULL, *rsign_md = NULL;
    int trailing_md = 0;
    CA_DB *rdb = NULL;
    EVP_PKEY *key = NULL, *rkey = NULL;
    OCSP_BASICRESP *bs = NULL;
    OCSP_REQUEST *req = NULL;
    OCSP_RESPONSE *resp = NULL;
    STACK_OF(CONF_VALUE) *headers = NULL;
    STACK_OF(OCSP_CERTID) *ids = NULL;
    STACK_OF(OPENSSL_STRING) *reqnames = NULL;
    STACK_OF(X509) *sign_other = NULL, *verify_other = NULL, *rother = NULL;
    STACK_OF(X509) *issuers = NULL;
    X509 *issuer = NULL, *cert = NULL, *rca_cert = NULL;
    X509 *signer = NULL, *rsigner = NULL;
    X509_STORE *store = NULL;
    X509_VERIFY_PARAM *vpm = NULL;
    const char *CAfile = NULL, *CApath = NULL;
    char *header, *value;
    char *host = NULL, *port = NULL, *path = "/", *outfile = NULL;
    char *rca_filename = NULL, *reqin = NULL, *respin = NULL;
    char *reqout = NULL, *respout = NULL, *ridx_filename = NULL;
    char *rsignfile = NULL, *rkeyfile = NULL;
    char *sign_certfile = NULL, *verify_certfile = NULL, *rcertfile = NULL;
    char *signfile = NULL, *keyfile = NULL;
    char *thost = NULL, *tport = NULL, *tpath = NULL;
    int noCAfile = 0, noCApath = 0;
    int accept_count = -1, add_nonce = 1, noverify = 0, use_ssl = -1;
    int vpmtouched = 0, badsig = 0, i, ignore_err = 0, nmin = 0, ndays = -1;
    int req_text = 0, resp_text = 0, ret = 1;
#ifndef OPENSSL_NO_SOCK
    int req_timeout = -1;
#endif
    long nsec = MAX_VALIDITY_PERIOD, maxage = -1;
    unsigned long sign_flags = 0, verify_flags = 0, rflags = 0;
    OPTION_CHOICE o;
    char *prog;

    reqnames = sk_OPENSSL_STRING_new_null();
    if (!reqnames)
        goto end;
    ids = sk_OCSP_CERTID_new_null();
    if (!ids)
        goto end;
    if ((vpm = X509_VERIFY_PARAM_new()) == NULL)
        return 1;

    prog = opt_init(argc, argv, ocsp_options);
    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_EOF:
        case OPT_ERR:
 opthelp:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            goto end;
        case OPT_HELP:
            ret = 0;
            opt_help(ocsp_options);
            goto end;
        case OPT_OUTFILE:
            outfile = opt_arg();
            break;
        case OPT_TIMEOUT:
#ifndef OPENSSL_NO_SOCK
            req_timeout = atoi(opt_arg());
#endif
            break;
        case OPT_URL:
            OPENSSL_free(thost);
            OPENSSL_free(tport);
            OPENSSL_free(tpath);
            thost = tport = tpath = NULL;
            if (!OCSP_parse_url(opt_arg(), &host, &port, &path, &use_ssl)) {
                BIO_printf(bio_err, "%s Error parsing URL\n", prog);
                goto end;
            }
            thost = host;
            tport = port;
            tpath = path;
            break;
        case OPT_HOST:
            host = opt_arg();
            break;
        case OPT_PORT:
            port = opt_arg();
            break;
        case OPT_IGNORE_ERR:
            ignore_err = 1;
            break;
        case OPT_NOVERIFY:
            noverify = 1;
            break;
        case OPT_NONCE:
            add_nonce = 2;
            break;
        case OPT_NO_NONCE:
            add_nonce = 0;
            break;
        case OPT_RESP_NO_CERTS:
            rflags |= OCSP_NOCERTS;
            break;
        case OPT_RESP_KEY_ID:
            rflags |= OCSP_RESPID_KEY;
            break;
        case OPT_NO_CERTS:
            sign_flags |= OCSP_NOCERTS;
            break;
        case OPT_NO_SIGNATURE_VERIFY:
            verify_flags |= OCSP_NOSIGS;
            break;
        case OPT_NO_CERT_VERIFY:
            verify_flags |= OCSP_NOVERIFY;
            break;
        case OPT_NO_CHAIN:
            verify_flags |= OCSP_NOCHAIN;
            break;
        case OPT_NO_CERT_CHECKS:
            verify_flags |= OCSP_NOCHECKS;
            break;
        case OPT_NO_EXPLICIT:
            verify_flags |= OCSP_NOEXPLICIT;
            break;
        case OPT_TRUST_OTHER:
            verify_flags |= OCSP_TRUSTOTHER;
            break;
        case OPT_NO_INTERN:
            verify_flags |= OCSP_NOINTERN;
            break;
        case OPT_BADSIG:
            badsig = 1;
            break;
        case OPT_TEXT:
            req_text = resp_text = 1;
            break;
        case OPT_REQ_TEXT:
            req_text = 1;
            break;
        case OPT_RESP_TEXT:
            resp_text = 1;
            break;
        case OPT_REQIN:
            reqin = opt_arg();
            break;
        case OPT_RESPIN:
            respin = opt_arg();
            break;
        case OPT_SIGNER:
            signfile = opt_arg();
            break;
        case OPT_VAFILE:
            verify_certfile = opt_arg();
            verify_flags |= OCSP_TRUSTOTHER;
            break;
        case OPT_SIGN_OTHER:
            sign_certfile = opt_arg();
            break;
        case OPT_VERIFY_OTHER:
            verify_certfile = opt_arg();
            break;
        case OPT_CAFILE:
            CAfile = opt_arg();
            break;
        case OPT_CAPATH:
            CApath = opt_arg();
            break;
        case OPT_NOCAFILE:
            noCAfile = 1;
            break;
        case OPT_NOCAPATH:
            noCApath = 1;
            break;
        case OPT_V_CASES:
            if (!opt_verify(o, vpm))
                goto end;
            vpmtouched++;
            break;
        case OPT_VALIDITY_PERIOD:
            opt_long(opt_arg(), &nsec);
            break;
        case OPT_STATUS_AGE:
            opt_long(opt_arg(), &maxage);
            break;
        case OPT_SIGNKEY:
            keyfile = opt_arg();
            break;
        case OPT_REQOUT:
            reqout = opt_arg();
            break;
        case OPT_RESPOUT:
            respout = opt_arg();
            break;
        case OPT_PATH:
            path = opt_arg();
            break;
        case OPT_ISSUER:
            issuer = load_cert(opt_arg(), FORMAT_PEM, "issuer certificate");
            if (issuer == NULL)
                goto end;
            if (issuers == NULL) {
                if ((issuers = sk_X509_new_null()) == NULL)
                    goto end;
            }
            sk_X509_push(issuers, issuer);
            break;
        case OPT_CERT:
            X509_free(cert);
            cert = load_cert(opt_arg(), FORMAT_PEM, "certificate");
            if (cert == NULL)
                goto end;
            if (cert_id_md == NULL)
                cert_id_md = EVP_sha1();
            if (!add_ocsp_cert(&req, cert, cert_id_md, issuer, ids))
                goto end;
            if (!sk_OPENSSL_STRING_push(reqnames, opt_arg()))
                goto end;
            trailing_md = 0;
            break;
        case OPT_SERIAL:
            if (cert_id_md == NULL)
                cert_id_md = EVP_sha1();
            if (!add_ocsp_serial(&req, opt_arg(), cert_id_md, issuer, ids))
                goto end;
            if (!sk_OPENSSL_STRING_push(reqnames, opt_arg()))
                goto end;
            trailing_md = 0;
            break;
        case OPT_INDEX:
            ridx_filename = opt_arg();
            break;
        case OPT_CA:
            rca_filename = opt_arg();
            break;
        case OPT_NMIN:
            opt_int(opt_arg(), &nmin);
            if (ndays == -1)
                ndays = 0;
            break;
        case OPT_REQUEST:
            opt_int(opt_arg(), &accept_count);
            break;
        case OPT_NDAYS:
            ndays = atoi(opt_arg());
            break;
        case OPT_RSIGNER:
            rsignfile = opt_arg();
            break;
        case OPT_RKEY:
            rkeyfile = opt_arg();
            break;
        case OPT_ROTHER:
            rcertfile = opt_arg();
            break;
        case OPT_RMD:   /* Response MessageDigest */
            if (!opt_md(opt_arg(), &rsign_md))
                goto end;
            break;
        case OPT_HEADER:
            header = opt_arg();
            value = strchr(header, '=');
            if (value == NULL) {
                BIO_printf(bio_err, "Missing = in header key=value\n");
                goto opthelp;
            }
            *value++ = '\0';
            if (!X509V3_add_value(header, value, &headers))
                goto end;
            break;
        case OPT_MD:
            if (trailing_md) {
                BIO_printf(bio_err,
                           "%s: Digest must be before -cert or -serial\n",
                           prog);
                goto opthelp;
            }
            if (!opt_md(opt_unknown(), &cert_id_md))
                goto opthelp;
            trailing_md = 1;
            break;
        }
    }

    if (trailing_md) {
        BIO_printf(bio_err, "%s: Digest must be before -cert or -serial\n",
                   prog);
        goto opthelp;
    }
    argc = opt_num_rest();
    if (argc != 0)
        goto opthelp;

    /* Have we anything to do? */
    if (!req && !reqin && !respin && !(port && ridx_filename))
        goto opthelp;

    out = bio_open_default(outfile, 'w', FORMAT_TEXT);
    if (out == NULL)
        goto end;

    if (!req && (add_nonce != 2))
        add_nonce = 0;

    if (!req && reqin) {
        derbio = bio_open_default(reqin, 'r', FORMAT_ASN1);
        if (derbio == NULL)
            goto end;
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

    if (rsignfile) {
        if (!rkeyfile)
            rkeyfile = rsignfile;
        rsigner = load_cert(rsignfile, FORMAT_PEM, "responder certificate");
        if (!rsigner) {
            BIO_printf(bio_err, "Error loading responder certificate\n");
            goto end;
        }
        rca_cert = load_cert(rca_filename, FORMAT_PEM, "CA certificate");
        if (rcertfile) {
            if (!load_certs(rcertfile, &rother, FORMAT_PEM, NULL,
                            "responder other certificates"))
                goto end;
        }
        rkey = load_key(rkeyfile, FORMAT_PEM, 0, NULL, NULL,
                        "responder private key");
        if (!rkey)
            goto end;
    }
    if (acbio)
        BIO_printf(bio_err, "Waiting for OCSP client connections...\n");

 redo_accept:

    if (acbio) {
        if (!do_responder(&req, &cbio, acbio))
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
        signer = load_cert(signfile, FORMAT_PEM, "signer certificate");
        if (!signer) {
            BIO_printf(bio_err, "Error loading signer certificate\n");
            goto end;
        }
        if (sign_certfile) {
            if (!load_certs(sign_certfile, &sign_other, FORMAT_PEM, NULL,
                            "signer certificates"))
                goto end;
        }
        key = load_key(keyfile, FORMAT_PEM, 0, NULL, NULL,
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
        derbio = bio_open_default(reqout, 'w', FORMAT_ASN1);
        if (derbio == NULL)
            goto end;
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
        make_ocsp_response(&resp, req, rdb, rca_cert, rsigner, rkey,
                               rsign_md, rother, rflags, nmin, ndays, badsig);
        if (cbio)
            send_ocsp_response(cbio, resp);
    } else if (host) {
# ifndef OPENSSL_NO_SOCK
        resp = process_responder(req, host, path,
                                 port, use_ssl, headers, req_timeout);
        if (!resp)
            goto end;
# else
        BIO_printf(bio_err,
                   "Error creating connect BIO - sockets not supported.\n");
        goto end;
# endif
    } else if (respin) {
        derbio = bio_open_default(respin, 'r', FORMAT_ASN1);
        if (derbio == NULL)
            goto end;
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
        derbio = bio_open_default(respout, 'w', FORMAT_ASN1);
        if (derbio == NULL)
            goto end;
        i2d_OCSP_RESPONSE_bio(derbio, resp);
        BIO_free(derbio);
    }

    i = OCSP_response_status(resp);
    if (i != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
        BIO_printf(out, "Responder Error: %s (%d)\n",
                   OCSP_response_status_str(i), i);
        if (ignore_err)
            goto redo_accept;
        goto end;
    }

    if (resp_text)
        OCSP_RESPONSE_print(out, resp, 0);

    /* If running as responder don't verify our own response */
    if (cbio) {
        /* If not unlimited, see if we took all we should. */
        if (accept_count != -1 && --accept_count <= 0) {
            ret = 0;
            goto end;
        }
        BIO_free_all(cbio);
        cbio = NULL;
        OCSP_REQUEST_free(req);
        req = NULL;
        OCSP_RESPONSE_free(resp);
        resp = NULL;
        goto redo_accept;
    }
    if (ridx_filename) {
        ret = 0;
        goto end;
    }

    if (!store) {
        store = setup_verify(CAfile, CApath, noCAfile, noCApath);
        if (!store)
            goto end;
    }
    if (vpmtouched)
        X509_STORE_set1_param(store, vpm);
    if (verify_certfile) {
        if (!load_certs(verify_certfile, &verify_other, FORMAT_PEM, NULL,
                        "validator certificate"))
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
        if (i <= 0 && issuers) {
            i = OCSP_basic_verify(bs, issuers, store, OCSP_TRUSTOTHER);
            if (i > 0)
                ERR_clear_error();
        }
        if (i <= 0) {
            BIO_printf(bio_err, "Response Verify Failure\n");
            ERR_print_errors(bio_err);
            ret = 1;
        } else
            BIO_printf(bio_err, "Response verify OK\n");

    }

    print_ocsp_summary(out, bs, req, reqnames, ids, nsec, maxage);

 end:
    ERR_print_errors(bio_err);
    X509_free(signer);
    X509_STORE_free(store);
    X509_VERIFY_PARAM_free(vpm);
    EVP_PKEY_free(key);
    EVP_PKEY_free(rkey);
    X509_free(cert);
    sk_X509_pop_free(issuers, X509_free);
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
    OPENSSL_free(thost);
    OPENSSL_free(tport);
    OPENSSL_free(tpath);

    return (ret);
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
    if (*req == NULL)
        *req = OCSP_REQUEST_new();
    if (*req == NULL)
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
    if (*req == NULL)
        *req = OCSP_REQUEST_new();
    if (*req == NULL)
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
    if (id == NULL || !sk_OCSP_CERTID_push(ids, id))
        goto err;
    if (!OCSP_request_add0_id(*req, id))
        goto err;
    return 1;

 err:
    BIO_printf(bio_err, "Error Creating OCSP request\n");
    return 0;
}

static void print_ocsp_summary(BIO *out, OCSP_BASICRESP *bs, OCSP_REQUEST *req,
                              STACK_OF(OPENSSL_STRING) *names,
                              STACK_OF(OCSP_CERTID) *ids, long nsec,
                              long maxage)
{
    OCSP_CERTID *id;
    const char *name;
    int i, status, reason;
    ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;

    if (!bs || !req || !sk_OPENSSL_STRING_num(names)
        || !sk_OCSP_CERTID_num(ids))
        return;

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
}

static void make_ocsp_response(OCSP_RESPONSE **resp, OCSP_REQUEST *req,
                              CA_DB *db, X509 *ca, X509 *rcert,
                              EVP_PKEY *rkey, const EVP_MD *rmd,
                              STACK_OF(X509) *rother, unsigned long flags,
                              int nmin, int ndays, int badsig)
{
    ASN1_TIME *thisupd = NULL, *nextupd = NULL;
    OCSP_CERTID *cid, *ca_id = NULL;
    OCSP_BASICRESP *bs = NULL;
    int i, id_count;

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

    if (badsig) {
        const ASN1_OCTET_STRING *sig = OCSP_resp_get0_signature(bs);
        corrupt_signature(sig);
    }

    *resp = OCSP_response_create(OCSP_RESPONSE_STATUS_SUCCESSFUL, bs);

 end:
    ASN1_TIME_free(thisupd);
    ASN1_TIME_free(nextupd);
    OCSP_CERTID_free(ca_id);
    OCSP_BASICRESP_free(bs);
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
        itmp = OPENSSL_strdup("00");
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
# ifdef OPENSSL_NO_SOCK
    BIO_printf(bio_err,
               "Error setting up accept BIO - sockets not supported.\n");
    return NULL;
# else
    BIO *acbio = NULL, *bufbio = NULL;

    bufbio = BIO_new(BIO_f_buffer());
    if (bufbio == NULL)
        goto err;
    acbio = BIO_new(BIO_s_accept());
    if (acbio == NULL
        || BIO_set_bind_mode(acbio, BIO_BIND_REUSEADDR) < 0
        || BIO_set_accept_port(acbio, port) < 0) {
        BIO_printf(bio_err, "Error setting up accept BIO\n");
        ERR_print_errors(bio_err);
        goto err;
    }

    BIO_set_accept_bios(acbio, bufbio);
    bufbio = NULL;
    if (BIO_do_accept(acbio) <= 0) {
        BIO_printf(bio_err, "Error starting accept\n");
        ERR_print_errors(bio_err);
        goto err;
    }

    return acbio;

 err:
    BIO_free_all(acbio);
    BIO_free(bufbio);
    return NULL;
# endif
}

# ifndef OPENSSL_NO_SOCK
/*
 * Decode %xx URL-decoding in-place. Ignores mal-formed sequences.
 */
static int urldecode(char *p)
{
    unsigned char *out = (unsigned char *)p;
    unsigned char *save = out;

    for (; *p; p++) {
        if (*p != '%')
            *out++ = *p;
        else if (isxdigit(_UC(p[1])) && isxdigit(_UC(p[2]))) {
            /* Don't check, can't fail because of ixdigit() call. */
            *out++ = (OPENSSL_hexchar2int(p[1]) << 4)
                   | OPENSSL_hexchar2int(p[2]);
            p += 2;
        }
        else
            return -1;
    }
    *out = '\0';
    return (int)(out - save);
}
# endif

static int do_responder(OCSP_REQUEST **preq, BIO **pcbio, BIO *acbio)
{
# ifdef OPENSSL_NO_SOCK
    return 0;
# else
    int len;
    OCSP_REQUEST *req = NULL;
    char inbuf[2048], reqbuf[2048];
    char *p, *q;
    BIO *cbio = NULL, *getbio = NULL, *b64 = NULL;

    if (BIO_do_accept(acbio) <= 0) {
        BIO_printf(bio_err, "Error accepting connection\n");
        ERR_print_errors(bio_err);
        return 0;
    }

    cbio = BIO_pop(acbio);
    *pcbio = cbio;

    /* Read the request line. */
    len = BIO_gets(cbio, reqbuf, sizeof(reqbuf));
    if (len <= 0)
        return 1;
    if (strncmp(reqbuf, "GET ", 4) == 0) {
        /* Expecting GET {sp} /URL {sp} HTTP/1.x */
        for (p = reqbuf + 4; *p == ' '; ++p)
            continue;
        if (*p != '/') {
            BIO_printf(bio_err, "Invalid request -- bad URL\n");
            return 1;
        }
        p++;

        /* Splice off the HTTP version identifier. */
        for (q = p; *q; q++)
            if (*q == ' ')
                break;
        if (strncmp(q, " HTTP/1.", 8) != 0) {
            BIO_printf(bio_err, "Invalid request -- bad HTTP vesion\n");
            return 1;
        }
        *q = '\0';
        len = urldecode(p);
        if (len <= 0) {
            BIO_printf(bio_err, "Invalid request -- bad URL encoding\n");
            return 1;
        }
        if ((getbio = BIO_new_mem_buf(p, len)) == NULL
            || (b64 = BIO_new(BIO_f_base64())) == NULL) {
            BIO_printf(bio_err, "Could not allocate memory\n");
            ERR_print_errors(bio_err);
            return 1;
        }
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        getbio = BIO_push(b64, getbio);
    } else if (strncmp(reqbuf, "POST ", 5) != 0) {
        BIO_printf(bio_err, "Invalid request -- bad HTTP verb\n");
        return 1;
    }

    /* Read and skip past the headers. */
    for (;;) {
        len = BIO_gets(cbio, inbuf, sizeof(inbuf));
        if (len <= 0)
            return 1;
        if ((inbuf[0] == '\r') || (inbuf[0] == '\n'))
            break;
    }

    /* Try to read OCSP request */
    if (getbio) {
        req = d2i_OCSP_REQUEST_bio(getbio, NULL);
        BIO_free_all(getbio);
    } else
        req = d2i_OCSP_REQUEST_bio(cbio, NULL);

    if (!req) {
        BIO_printf(bio_err, "Error parsing OCSP request\n");
        ERR_print_errors(bio_err);
    }

    *preq = req;

    return 1;
# endif
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

# ifndef OPENSSL_NO_SOCK
static OCSP_RESPONSE *query_responder(BIO *cbio, const char *host,
                                      const char *path,
                                      const STACK_OF(CONF_VALUE) *headers,
                                      OCSP_REQUEST *req, int req_timeout)
{
    int fd;
    int rv;
    int i;
    int add_host = 1;
    OCSP_REQ_CTX *ctx = NULL;
    OCSP_RESPONSE *rsp = NULL;
    fd_set confds;
    struct timeval tv;

    if (req_timeout != -1)
        BIO_set_nbio(cbio, 1);

    rv = BIO_do_connect(cbio);

    if ((rv <= 0) && ((req_timeout == -1) || !BIO_should_retry(cbio))) {
        BIO_puts(bio_err, "Error connecting BIO\n");
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
            BIO_puts(bio_err, "Timeout on connect\n");
            return NULL;
        }
    }

    ctx = OCSP_sendreq_new(cbio, path, NULL, -1);
    if (ctx == NULL)
        return NULL;

    for (i = 0; i < sk_CONF_VALUE_num(headers); i++) {
        CONF_VALUE *hdr = sk_CONF_VALUE_value(headers, i);
        if (add_host == 1 && strcasecmp("host", hdr->name) == 0)
            add_host = 0;
        if (!OCSP_REQ_CTX_add1_header(ctx, hdr->name, hdr->value))
            goto err;
    }

    if (add_host == 1 && OCSP_REQ_CTX_add1_header(ctx, "Host", host) == 0)
        goto err;

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
            BIO_puts(bio_err, "Unexpected retry condition\n");
            goto err;
        }
        if (rv == 0) {
            BIO_puts(bio_err, "Timeout on request\n");
            break;
        }
        if (rv == -1) {
            BIO_puts(bio_err, "Select error\n");
            break;
        }

    }
 err:
    OCSP_REQ_CTX_free(ctx);

    return rsp;
}

OCSP_RESPONSE *process_responder(OCSP_REQUEST *req,
                                 const char *host, const char *path,
                                 const char *port, int use_ssl,
                                 STACK_OF(CONF_VALUE) *headers,
                                 int req_timeout)
{
    BIO *cbio = NULL;
    SSL_CTX *ctx = NULL;
    OCSP_RESPONSE *resp = NULL;

    cbio = BIO_new_connect(host);
    if (!cbio) {
        BIO_printf(bio_err, "Error creating connect BIO\n");
        goto end;
    }
    if (port)
        BIO_set_conn_port(cbio, port);
    if (use_ssl == 1) {
        BIO *sbio;
        ctx = SSL_CTX_new(TLS_client_method());
        if (ctx == NULL) {
            BIO_printf(bio_err, "Error creating SSL context.\n");
            goto end;
        }
        SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
        sbio = BIO_new_ssl(ctx, 1);
        cbio = BIO_push(sbio, cbio);
    }

    resp = query_responder(cbio, host, path, headers, req, req_timeout);
    if (!resp)
        BIO_printf(bio_err, "Error querying OCSP responder\n");
 end:
    BIO_free_all(cbio);
    SSL_CTX_free(ctx);
    return resp;
}
# endif

#endif
