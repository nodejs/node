/* apps/s_server.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
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
 * ECC cipher suite support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project.
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

/*
 * Until the key-gen callbacks are modified to use newer prototypes, we allow
 * deprecated functions for openssl-internal code
 */
#ifdef OPENSSL_NO_DEPRECATED
# undef OPENSSL_NO_DEPRECATED
#endif

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/e_os2.h>
#ifdef OPENSSL_NO_STDIO
# define APPS_WIN16
#endif

/* conflicts with winsock2 stuff on netware */
#if !defined(OPENSSL_SYS_NETWARE)
# include <sys/types.h>
#endif

/*
 * With IPv6, it looks like Digital has mixed up the proper order of
 * recursive header file inclusion, resulting in the compiler complaining
 * that u_int isn't defined, but only if _POSIX_C_SOURCE is defined, which is
 * needed to have fileno() declared correctly...  So let's define u_int
 */
#if defined(OPENSSL_SYS_VMS_DECC) && !defined(__U_INT)
# define __U_INT
typedef unsigned int u_int;
#endif

#include <openssl/lhash.h>
#include <openssl/bn.h>
#define USE_SOCKETS
#include "apps.h"
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/ocsp.h>
#ifndef OPENSSL_NO_DH
# include <openssl/dh.h>
#endif
#ifndef OPENSSL_NO_RSA
# include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_SRP
# include <openssl/srp.h>
#endif
#include "s_apps.h"
#include "timeouts.h"

#if (defined(OPENSSL_SYS_VMS) && __VMS_VER < 70000000)
/* FIONBIO used as a switch to enable ioctl, and that isn't in VMS < 7.0 */
# undef FIONBIO
#endif

#if defined(OPENSSL_SYS_BEOS_R5)
# include <fcntl.h>
#endif

#ifndef OPENSSL_NO_RSA
static RSA MS_CALLBACK *tmp_rsa_cb(SSL *s, int is_export, int keylength);
#endif
static int sv_body(char *hostname, int s, int stype, unsigned char *context);
static int www_body(char *hostname, int s, int stype, unsigned char *context);
static int rev_body(char *hostname, int s, int stype, unsigned char *context);
static void close_accept_socket(void);
static void sv_usage(void);
static int init_ssl_connection(SSL *s);
static void print_stats(BIO *bp, SSL_CTX *ctx);
static int generate_session_id(const SSL *ssl, unsigned char *id,
                               unsigned int *id_len);
static void init_session_cache_ctx(SSL_CTX *sctx);
static void free_sessions(void);
#ifndef OPENSSL_NO_DH
static DH *load_dh_param(const char *dhfile);
static DH *get_dh2048(void);
#endif

#ifdef MONOLITH
static void s_server_init(void);
#endif

#ifndef OPENSSL_NO_DH
static unsigned char dh2048_p[] = {
    0xF6,0x42,0x57,0xB7,0x08,0x7F,0x08,0x17,0x72,0xA2,0xBA,0xD6,
    0xA9,0x42,0xF3,0x05,0xE8,0xF9,0x53,0x11,0x39,0x4F,0xB6,0xF1,
    0x6E,0xB9,0x4B,0x38,0x20,0xDA,0x01,0xA7,0x56,0xA3,0x14,0xE9,
    0x8F,0x40,0x55,0xF3,0xD0,0x07,0xC6,0xCB,0x43,0xA9,0x94,0xAD,
    0xF7,0x4C,0x64,0x86,0x49,0xF8,0x0C,0x83,0xBD,0x65,0xE9,0x17,
    0xD4,0xA1,0xD3,0x50,0xF8,0xF5,0x59,0x5F,0xDC,0x76,0x52,0x4F,
    0x3D,0x3D,0x8D,0xDB,0xCE,0x99,0xE1,0x57,0x92,0x59,0xCD,0xFD,
    0xB8,0xAE,0x74,0x4F,0xC5,0xFC,0x76,0xBC,0x83,0xC5,0x47,0x30,
    0x61,0xCE,0x7C,0xC9,0x66,0xFF,0x15,0xF9,0xBB,0xFD,0x91,0x5E,
    0xC7,0x01,0xAA,0xD3,0x5B,0x9E,0x8D,0xA0,0xA5,0x72,0x3A,0xD4,
    0x1A,0xF0,0xBF,0x46,0x00,0x58,0x2B,0xE5,0xF4,0x88,0xFD,0x58,
    0x4E,0x49,0xDB,0xCD,0x20,0xB4,0x9D,0xE4,0x91,0x07,0x36,0x6B,
    0x33,0x6C,0x38,0x0D,0x45,0x1D,0x0F,0x7C,0x88,0xB3,0x1C,0x7C,
    0x5B,0x2D,0x8E,0xF6,0xF3,0xC9,0x23,0xC0,0x43,0xF0,0xA5,0x5B,
    0x18,0x8D,0x8E,0xBB,0x55,0x8C,0xB8,0x5D,0x38,0xD3,0x34,0xFD,
    0x7C,0x17,0x57,0x43,0xA3,0x1D,0x18,0x6C,0xDE,0x33,0x21,0x2C,
    0xB5,0x2A,0xFF,0x3C,0xE1,0xB1,0x29,0x40,0x18,0x11,0x8D,0x7C,
    0x84,0xA7,0x0A,0x72,0xD6,0x86,0xC4,0x03,0x19,0xC8,0x07,0x29,
    0x7A,0xCA,0x95,0x0C,0xD9,0x96,0x9F,0xAB,0xD0,0x0A,0x50,0x9B,
    0x02,0x46,0xD3,0x08,0x3D,0x66,0xA4,0x5D,0x41,0x9F,0x9C,0x7C,
    0xBD,0x89,0x4B,0x22,0x19,0x26,0xBA,0xAB,0xA2,0x5E,0xC3,0x55,
    0xE9,0x32,0x0B,0x3B,
};

static unsigned char dh2048_g[] = {
    0x02,
};

DH *get_dh2048()
{
    DH *dh;

    if ((dh = DH_new()) == NULL)
        return NULL;
    dh->p=BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
    dh->g=BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
    if (dh->p == NULL || dh->g == NULL) {
        DH_free(dh);
        return NULL;
    }
    return dh;
}
#endif

/* static int load_CA(SSL_CTX *ctx, char *file);*/

#undef BUFSIZZ
#define BUFSIZZ 16*1024
static int bufsize = BUFSIZZ;
static int accept_socket = -1;

#define TEST_CERT       "server.pem"
#ifndef OPENSSL_NO_TLSEXT
# define TEST_CERT2      "server2.pem"
#endif
#undef PROG
#define PROG            s_server_main

extern int verify_depth, verify_return_error, verify_quiet;

static int s_server_verify = SSL_VERIFY_NONE;
static int s_server_session_id_context = 1; /* anything will do */
static const char *s_cert_file = TEST_CERT, *s_key_file =
    NULL, *s_chain_file = NULL;
#ifndef OPENSSL_NO_TLSEXT
static const char *s_cert_file2 = TEST_CERT2, *s_key_file2 = NULL;
#endif
static char *s_dcert_file = NULL, *s_dkey_file = NULL, *s_dchain_file = NULL;
#ifdef FIONBIO
static int s_nbio = 0;
#endif
static int s_nbio_test = 0;
int s_crlf = 0;
static SSL_CTX *ctx = NULL;
#ifndef OPENSSL_NO_TLSEXT
static SSL_CTX *ctx2 = NULL;
#endif
static int www = 0;

static BIO *bio_s_out = NULL;
static BIO *bio_s_msg = NULL;
static int s_debug = 0;
#ifndef OPENSSL_NO_TLSEXT
static int s_tlsextdebug = 0;
static int s_tlsextstatus = 0;
static int cert_status_cb(SSL *s, void *arg);
#endif
static int no_resume_ephemeral = 0;
static int s_msg = 0;
static int s_quiet = 0;
static int s_ign_eof = 0;
static int s_brief = 0;

static char *keymatexportlabel = NULL;
static int keymatexportlen = 20;

static int hack = 0;
#ifndef OPENSSL_NO_ENGINE
static char *engine_id = NULL;
#endif
static const char *session_id_prefix = NULL;

static int enable_timeouts = 0;
static long socket_mtu;
#ifndef OPENSSL_NO_DTLS1
static int cert_chain = 0;
#endif

#ifndef OPENSSL_NO_TLSEXT
static BIO *serverinfo_in = NULL;
static const char *s_serverinfo_file = NULL;

#endif

#ifndef OPENSSL_NO_PSK
static char *psk_identity = "Client_identity";
char *psk_key = NULL;           /* by default PSK is not used */

static unsigned int psk_server_cb(SSL *ssl, const char *identity,
                                  unsigned char *psk,
                                  unsigned int max_psk_len)
{
    unsigned int psk_len = 0;
    int ret;
    BIGNUM *bn = NULL;

    if (s_debug)
        BIO_printf(bio_s_out, "psk_server_cb\n");
    if (!identity) {
        BIO_printf(bio_err, "Error: client did not send PSK identity\n");
        goto out_err;
    }
    if (s_debug)
        BIO_printf(bio_s_out, "identity_len=%d identity=%s\n",
                   (int)strlen(identity), identity);

    /* here we could lookup the given identity e.g. from a database */
    if (strcmp(identity, psk_identity) != 0) {
        BIO_printf(bio_s_out, "PSK error: client identity not found"
                   " (got '%s' expected '%s')\n", identity, psk_identity);
        goto out_err;
    }
    if (s_debug)
        BIO_printf(bio_s_out, "PSK client identity found\n");

    /* convert the PSK key to binary */
    ret = BN_hex2bn(&bn, psk_key);
    if (!ret) {
        BIO_printf(bio_err, "Could not convert PSK key '%s' to BIGNUM\n",
                   psk_key);
        if (bn)
            BN_free(bn);
        return 0;
    }
    if (BN_num_bytes(bn) > (int)max_psk_len) {
        BIO_printf(bio_err,
                   "psk buffer of callback is too small (%d) for key (%d)\n",
                   max_psk_len, BN_num_bytes(bn));
        BN_free(bn);
        return 0;
    }

    ret = BN_bn2bin(bn, psk);
    BN_free(bn);

    if (ret < 0)
        goto out_err;
    psk_len = (unsigned int)ret;

    if (s_debug)
        BIO_printf(bio_s_out, "fetched PSK len=%d\n", psk_len);
    return psk_len;
 out_err:
    if (s_debug)
        BIO_printf(bio_err, "Error in PSK server callback\n");
    return 0;
}
#endif

#ifndef OPENSSL_NO_SRP
/* This is a context that we pass to callbacks */
typedef struct srpsrvparm_st {
    char *login;
    SRP_VBASE *vb;
    SRP_user_pwd *user;
} srpsrvparm;

/*
 * This callback pretends to require some asynchronous logic in order to
 * obtain a verifier. When the callback is called for a new connection we
 * return with a negative value. This will provoke the accept etc to return
 * with an LOOKUP_X509. The main logic of the reinvokes the suspended call
 * (which would normally occur after a worker has finished) and we set the
 * user parameters.
 */
static int MS_CALLBACK ssl_srp_server_param_cb(SSL *s, int *ad, void *arg)
{
    srpsrvparm *p = (srpsrvparm *) arg;
    if (p->login == NULL && p->user == NULL) {
        p->login = SSL_get_srp_username(s);
        BIO_printf(bio_err, "SRP username = \"%s\"\n", p->login);
        return (-1);
    }

    if (p->user == NULL) {
        BIO_printf(bio_err, "User %s doesn't exist\n", p->login);
        return SSL3_AL_FATAL;
    }
    if (SSL_set_srp_server_param
        (s, p->user->N, p->user->g, p->user->s, p->user->v,
         p->user->info) < 0) {
        *ad = SSL_AD_INTERNAL_ERROR;
        return SSL3_AL_FATAL;
    }
    BIO_printf(bio_err,
               "SRP parameters set: username = \"%s\" info=\"%s\" \n",
               p->login, p->user->info);
    /* need to check whether there are memory leaks */
    p->user = NULL;
    p->login = NULL;
    return SSL_ERROR_NONE;
}

#endif

#ifdef MONOLITH
static void s_server_init(void)
{
    accept_socket = -1;
    s_server_verify = SSL_VERIFY_NONE;
    s_dcert_file = NULL;
    s_dkey_file = NULL;
    s_dchain_file = NULL;
    s_cert_file = TEST_CERT;
    s_key_file = NULL;
    s_chain_file = NULL;
# ifndef OPENSSL_NO_TLSEXT
    s_cert_file2 = TEST_CERT2;
    s_key_file2 = NULL;
    ctx2 = NULL;
# endif
# ifdef FIONBIO
    s_nbio = 0;
# endif
    s_nbio_test = 0;
    ctx = NULL;
    www = 0;

    bio_s_out = NULL;
    s_debug = 0;
    s_msg = 0;
    s_quiet = 0;
    s_brief = 0;
    hack = 0;
# ifndef OPENSSL_NO_ENGINE
    engine_id = NULL;
# endif
}
#endif

static void sv_usage(void)
{
    BIO_printf(bio_err, "usage: s_server [args ...]\n");
    BIO_printf(bio_err, "\n");
    BIO_printf(bio_err,
               " -accept arg   - port to accept on (default is %d)\n", PORT);
    BIO_printf(bio_err,
               " -verify_host host - check peer certificate matches \"host\"\n");
    BIO_printf(bio_err,
               " -verify_email email - check peer certificate matches \"email\"\n");
    BIO_printf(bio_err,
               " -verify_ip ipaddr - check peer certificate matches \"ipaddr\"\n");
    BIO_printf(bio_err, " -context arg  - set session ID context\n");
    BIO_printf(bio_err,
               " -verify arg   - turn on peer certificate verification\n");
    BIO_printf(bio_err,
               " -Verify arg   - turn on peer certificate verification, must have a cert.\n");
    BIO_printf(bio_err,
               " -verify_return_error - return verification errors\n");
    BIO_printf(bio_err, " -cert arg     - certificate file to use\n");
    BIO_printf(bio_err, "                 (default is %s)\n", TEST_CERT);
#ifndef OPENSSL_NO_TLSEXT
    BIO_printf(bio_err,
               " -serverinfo arg - PEM serverinfo file for certificate\n");
    BIO_printf(bio_err,
               " -auth               - send and receive RFC 5878 TLS auth extensions and supplemental data\n");
    BIO_printf(bio_err,
               " -auth_require_reneg - Do not send TLS auth extensions until renegotiation\n");
#endif
    BIO_printf(bio_err,
               " -no_resumption_on_reneg - set SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION flag\n");
    BIO_printf(bio_err,
               " -crl_check    - check the peer certificate has not been revoked by its CA.\n"
               "                 The CRL(s) are appended to the certificate file\n");
    BIO_printf(bio_err,
               " -crl_check_all - check the peer certificate has not been revoked by its CA\n"
               "                 or any other CRL in the CA chain. CRL(s) are appened to the\n"
               "                 the certificate file.\n");
    BIO_printf(bio_err,
               " -certform arg - certificate format (PEM or DER) PEM default\n");
    BIO_printf(bio_err,
               " -key arg      - Private Key file to use, in cert file if\n");
    BIO_printf(bio_err, "                 not specified (default is %s)\n",
               TEST_CERT);
    BIO_printf(bio_err,
               " -keyform arg  - key format (PEM, DER or ENGINE) PEM default\n");
    BIO_printf(bio_err,
               " -pass arg     - private key file pass phrase source\n");
    BIO_printf(bio_err,
               " -dcert arg    - second certificate file to use (usually for DSA)\n");
    BIO_printf(bio_err,
               " -dcertform x  - second certificate format (PEM or DER) PEM default\n");
    BIO_printf(bio_err,
               " -dkey arg     - second private key file to use (usually for DSA)\n");
    BIO_printf(bio_err,
               " -dkeyform arg - second key format (PEM, DER or ENGINE) PEM default\n");
    BIO_printf(bio_err,
               " -dpass arg    - second private key file pass phrase source\n");
    BIO_printf(bio_err,
               " -dhparam arg  - DH parameter file to use, in cert file if not specified\n");
    BIO_printf(bio_err,
               "                 or a default set of parameters is used\n");
#ifndef OPENSSL_NO_ECDH
    BIO_printf(bio_err,
               " -named_curve arg  - Elliptic curve name to use for ephemeral ECDH keys.\n"
               "                 Use \"openssl ecparam -list_curves\" for all names\n"
               "                 (default is nistp256).\n");
#endif
#ifdef FIONBIO
    BIO_printf(bio_err, " -nbio         - Run with non-blocking IO\n");
#endif
    BIO_printf(bio_err,
               " -nbio_test    - test with the non-blocking test bio\n");
    BIO_printf(bio_err,
               " -crlf         - convert LF from terminal into CRLF\n");
    BIO_printf(bio_err, " -debug        - Print more output\n");
    BIO_printf(bio_err, " -msg          - Show protocol messages\n");
    BIO_printf(bio_err, " -state        - Print the SSL states\n");
    BIO_printf(bio_err, " -CApath arg   - PEM format directory of CA's\n");
    BIO_printf(bio_err, " -CAfile arg   - PEM format file of CA's\n");
    BIO_printf(bio_err,
               " -no_alt_chains - only ever use the first certificate chain found\n");
    BIO_printf(bio_err,
               " -nocert       - Don't use any certificates (Anon-DH)\n");
    BIO_printf(bio_err,
               " -cipher arg   - play with 'openssl ciphers' to see what goes here\n");
    BIO_printf(bio_err, " -serverpref   - Use server's cipher preferences\n");
    BIO_printf(bio_err, " -quiet        - No server output\n");
    BIO_printf(bio_err, " -no_tmp_rsa   - Do not generate a tmp RSA key\n");
#ifndef OPENSSL_NO_PSK
    BIO_printf(bio_err, " -psk_hint arg - PSK identity hint to use\n");
    BIO_printf(bio_err, " -psk arg      - PSK in hex (without 0x)\n");
# ifndef OPENSSL_NO_JPAKE
    BIO_printf(bio_err, " -jpake arg    - JPAKE secret to use\n");
# endif
#endif
#ifndef OPENSSL_NO_SRP
    BIO_printf(bio_err, " -srpvfile file      - The verifier file for SRP\n");
    BIO_printf(bio_err,
               " -srpuserseed string - A seed string for a default user salt.\n");
#endif
    BIO_printf(bio_err, " -ssl2         - Just talk SSLv2\n");
#ifndef OPENSSL_NO_SSL3_METHOD
    BIO_printf(bio_err, " -ssl3         - Just talk SSLv3\n");
#endif
    BIO_printf(bio_err, " -tls1_2       - Just talk TLSv1.2\n");
    BIO_printf(bio_err, " -tls1_1       - Just talk TLSv1.1\n");
    BIO_printf(bio_err, " -tls1         - Just talk TLSv1\n");
    BIO_printf(bio_err, " -dtls1        - Just talk DTLSv1\n");
    BIO_printf(bio_err, " -dtls1_2      - Just talk DTLSv1.2\n");
    BIO_printf(bio_err, " -timeout      - Enable timeouts\n");
    BIO_printf(bio_err, " -mtu          - Set link layer MTU\n");
    BIO_printf(bio_err, " -chain        - Read a certificate chain\n");
    BIO_printf(bio_err, " -no_ssl2      - Just disable SSLv2\n");
    BIO_printf(bio_err, " -no_ssl3      - Just disable SSLv3\n");
    BIO_printf(bio_err, " -no_tls1      - Just disable TLSv1\n");
    BIO_printf(bio_err, " -no_tls1_1    - Just disable TLSv1.1\n");
    BIO_printf(bio_err, " -no_tls1_2    - Just disable TLSv1.2\n");
#ifndef OPENSSL_NO_DH
    BIO_printf(bio_err, " -no_dhe       - Disable ephemeral DH\n");
#endif
#ifndef OPENSSL_NO_ECDH
    BIO_printf(bio_err, " -no_ecdhe     - Disable ephemeral ECDH\n");
#endif
    BIO_printf(bio_err, " -bugs         - Turn on SSL bug compatibility\n");
    BIO_printf(bio_err,
               " -hack         - workaround for early Netscape code\n");
    BIO_printf(bio_err,
               " -www          - Respond to a 'GET /' with a status page\n");
    BIO_printf(bio_err,
               " -WWW          - Respond to a 'GET /<path> HTTP/1.0' with file ./<path>\n");
    BIO_printf(bio_err,
               " -HTTP         - Respond to a 'GET /<path> HTTP/1.0' with file ./<path>\n");
    BIO_printf(bio_err,
               "                 with the assumption it contains a complete HTTP response.\n");
#ifndef OPENSSL_NO_ENGINE
    BIO_printf(bio_err,
               " -engine id    - Initialise and use the specified engine\n");
#endif
    BIO_printf(bio_err,
               " -id_prefix arg - Generate SSL/TLS session IDs prefixed by 'arg'\n");
    BIO_printf(bio_err, " -rand file%cfile%c...\n", LIST_SEPARATOR_CHAR,
               LIST_SEPARATOR_CHAR);
#ifndef OPENSSL_NO_TLSEXT
    BIO_printf(bio_err,
               " -servername host - servername for HostName TLS extension\n");
    BIO_printf(bio_err,
               " -servername_fatal - on mismatch send fatal alert (default warning alert)\n");
    BIO_printf(bio_err,
               " -cert2 arg    - certificate file to use for servername\n");
    BIO_printf(bio_err, "                 (default is %s)\n", TEST_CERT2);
    BIO_printf(bio_err,
               " -key2 arg     - Private Key file to use for servername, in cert file if\n");
    BIO_printf(bio_err, "                 not specified (default is %s)\n",
               TEST_CERT2);
    BIO_printf(bio_err,
               " -tlsextdebug  - hex dump of all TLS extensions received\n");
    BIO_printf(bio_err,
               " -no_ticket    - disable use of RFC4507bis session tickets\n");
    BIO_printf(bio_err,
               " -legacy_renegotiation - enable use of legacy renegotiation (dangerous)\n");
    BIO_printf(bio_err,
               " -sigalgs arg      - Signature algorithms to support (colon-separated list)\n");
    BIO_printf(bio_err,
               " -client_sigalgs arg  - Signature algorithms to support for client \n");
    BIO_printf(bio_err,
               "                        certificate authentication (colon-separated list)\n");
# ifndef OPENSSL_NO_NEXTPROTONEG
    BIO_printf(bio_err,
               " -nextprotoneg arg - set the advertised protocols for the NPN extension (comma-separated list)\n");
# endif
# ifndef OPENSSL_NO_SRTP
    BIO_printf(bio_err,
               " -use_srtp profiles - Offer SRTP key management with a colon-separated profile list\n");
# endif
    BIO_printf(bio_err,
               " -alpn arg  - set the advertised protocols for the ALPN extension (comma-separated list)\n");
#endif
    BIO_printf(bio_err,
               " -keymatexport label   - Export keying material using label\n");
    BIO_printf(bio_err,
               " -keymatexportlen len  - Export len bytes of keying material (default 20)\n");
    BIO_printf(bio_err,
               " -status           - respond to certificate status requests\n");
    BIO_printf(bio_err,
               " -status_verbose   - enable status request verbose printout\n");
    BIO_printf(bio_err,
               " -status_timeout n - status request responder timeout\n");
    BIO_printf(bio_err, " -status_url URL   - status request fallback URL\n");
}

static int local_argc = 0;
static char **local_argv;

#ifdef CHARSET_EBCDIC
static int ebcdic_new(BIO *bi);
static int ebcdic_free(BIO *a);
static int ebcdic_read(BIO *b, char *out, int outl);
static int ebcdic_write(BIO *b, const char *in, int inl);
static long ebcdic_ctrl(BIO *b, int cmd, long num, void *ptr);
static int ebcdic_gets(BIO *bp, char *buf, int size);
static int ebcdic_puts(BIO *bp, const char *str);

# define BIO_TYPE_EBCDIC_FILTER  (18|0x0200)
static BIO_METHOD methods_ebcdic = {
    BIO_TYPE_EBCDIC_FILTER,
    "EBCDIC/ASCII filter",
    ebcdic_write,
    ebcdic_read,
    ebcdic_puts,
    ebcdic_gets,
    ebcdic_ctrl,
    ebcdic_new,
    ebcdic_free,
};

typedef struct {
    size_t alloced;
    char buff[1];
} EBCDIC_OUTBUFF;

BIO_METHOD *BIO_f_ebcdic_filter()
{
    return (&methods_ebcdic);
}

static int ebcdic_new(BIO *bi)
{
    EBCDIC_OUTBUFF *wbuf;

    wbuf = (EBCDIC_OUTBUFF *) OPENSSL_malloc(sizeof(EBCDIC_OUTBUFF) + 1024);
    if (!wbuf)
        return 0;
    wbuf->alloced = 1024;
    wbuf->buff[0] = '\0';

    bi->ptr = (char *)wbuf;
    bi->init = 1;
    bi->flags = 0;
    return (1);
}

static int ebcdic_free(BIO *a)
{
    if (a == NULL)
        return (0);
    if (a->ptr != NULL)
        OPENSSL_free(a->ptr);
    a->ptr = NULL;
    a->init = 0;
    a->flags = 0;
    return (1);
}

static int ebcdic_read(BIO *b, char *out, int outl)
{
    int ret = 0;

    if (out == NULL || outl == 0)
        return (0);
    if (b->next_bio == NULL)
        return (0);

    ret = BIO_read(b->next_bio, out, outl);
    if (ret > 0)
        ascii2ebcdic(out, out, ret);
    return (ret);
}

static int ebcdic_write(BIO *b, const char *in, int inl)
{
    EBCDIC_OUTBUFF *wbuf;
    int ret = 0;
    int num;
    unsigned char n;

    if ((in == NULL) || (inl <= 0))
        return (0);
    if (b->next_bio == NULL)
        return (0);

    wbuf = (EBCDIC_OUTBUFF *) b->ptr;

    if (inl > (num = wbuf->alloced)) {
        num = num + num;        /* double the size */
        if (num < inl)
            num = inl;
        wbuf =
            (EBCDIC_OUTBUFF *) OPENSSL_malloc(sizeof(EBCDIC_OUTBUFF) + num);
        if (!wbuf)
            return 0;
        OPENSSL_free(b->ptr);

        wbuf->alloced = num;
        wbuf->buff[0] = '\0';

        b->ptr = (char *)wbuf;
    }

    ebcdic2ascii(wbuf->buff, in, inl);

    ret = BIO_write(b->next_bio, wbuf->buff, inl);

    return (ret);
}

static long ebcdic_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    long ret;

    if (b->next_bio == NULL)
        return (0);
    switch (cmd) {
    case BIO_CTRL_DUP:
        ret = 0L;
        break;
    default:
        ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
        break;
    }
    return (ret);
}

static int ebcdic_gets(BIO *bp, char *buf, int size)
{
    int i, ret = 0;
    if (bp->next_bio == NULL)
        return (0);
/*      return(BIO_gets(bp->next_bio,buf,size));*/
    for (i = 0; i < size - 1; ++i) {
        ret = ebcdic_read(bp, &buf[i], 1);
        if (ret <= 0)
            break;
        else if (buf[i] == '\n') {
            ++i;
            break;
        }
    }
    if (i < size)
        buf[i] = '\0';
    return (ret < 0 && i == 0) ? ret : i;
}

static int ebcdic_puts(BIO *bp, const char *str)
{
    if (bp->next_bio == NULL)
        return (0);
    return ebcdic_write(bp, str, strlen(str));
}
#endif

#ifndef OPENSSL_NO_TLSEXT

/* This is a context that we pass to callbacks */
typedef struct tlsextctx_st {
    char *servername;
    BIO *biodebug;
    int extension_error;
} tlsextctx;

static int MS_CALLBACK ssl_servername_cb(SSL *s, int *ad, void *arg)
{
    tlsextctx *p = (tlsextctx *) arg;
    const char *servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);
    if (servername && p->biodebug)
        BIO_printf(p->biodebug, "Hostname in TLS extension: \"%s\"\n",
                   servername);

    if (!p->servername)
        return SSL_TLSEXT_ERR_NOACK;

    if (servername) {
        if (strcasecmp(servername, p->servername))
            return p->extension_error;
        if (ctx2) {
            BIO_printf(p->biodebug, "Switching server context.\n");
            SSL_set_SSL_CTX(s, ctx2);
        }
    }
    return SSL_TLSEXT_ERR_OK;
}

/* Structure passed to cert status callback */

typedef struct tlsextstatusctx_st {
    /* Default responder to use */
    char *host, *path, *port;
    int use_ssl;
    int timeout;
    BIO *err;
    int verbose;
} tlsextstatusctx;

static tlsextstatusctx tlscstatp = { NULL, NULL, NULL, 0, -1, NULL, 0 };

/*
 * Certificate Status callback. This is called when a client includes a
 * certificate status request extension. This is a simplified version. It
 * examines certificates each time and makes one OCSP responder query for
 * each request. A full version would store details such as the OCSP
 * certificate IDs and minimise the number of OCSP responses by caching them
 * until they were considered "expired".
 */

static int cert_status_cb(SSL *s, void *arg)
{
    tlsextstatusctx *srctx = arg;
    BIO *err = srctx->err;
    char *host, *port, *path;
    int use_ssl;
    unsigned char *rspder = NULL;
    int rspderlen;
    STACK_OF(OPENSSL_STRING) *aia = NULL;
    X509 *x = NULL;
    X509_STORE_CTX inctx;
    X509_OBJECT obj;
    OCSP_REQUEST *req = NULL;
    OCSP_RESPONSE *resp = NULL;
    OCSP_CERTID *id = NULL;
    STACK_OF(X509_EXTENSION) *exts;
    int ret = SSL_TLSEXT_ERR_NOACK;
    int i;
# if 0
    STACK_OF(OCSP_RESPID) *ids;
    SSL_get_tlsext_status_ids(s, &ids);
    BIO_printf(err, "cert_status: received %d ids\n",
               sk_OCSP_RESPID_num(ids));
# endif
    if (srctx->verbose)
        BIO_puts(err, "cert_status: callback called\n");
    /* Build up OCSP query from server certificate */
    x = SSL_get_certificate(s);
    aia = X509_get1_ocsp(x);
    if (aia) {
        if (!OCSP_parse_url(sk_OPENSSL_STRING_value(aia, 0),
                            &host, &port, &path, &use_ssl)) {
            BIO_puts(err, "cert_status: can't parse AIA URL\n");
            goto err;
        }
        if (srctx->verbose)
            BIO_printf(err, "cert_status: AIA URL: %s\n",
                       sk_OPENSSL_STRING_value(aia, 0));
    } else {
        if (!srctx->host) {
            BIO_puts(srctx->err,
                     "cert_status: no AIA and no default responder URL\n");
            goto done;
        }
        host = srctx->host;
        path = srctx->path;
        port = srctx->port;
        use_ssl = srctx->use_ssl;
    }

    if (!X509_STORE_CTX_init(&inctx,
                             SSL_CTX_get_cert_store(SSL_get_SSL_CTX(s)),
                             NULL, NULL))
        goto err;
    if (X509_STORE_get_by_subject(&inctx, X509_LU_X509,
                                  X509_get_issuer_name(x), &obj) <= 0) {
        BIO_puts(err, "cert_status: Can't retrieve issuer certificate.\n");
        X509_STORE_CTX_cleanup(&inctx);
        goto done;
    }
    req = OCSP_REQUEST_new();
    if (!req)
        goto err;
    id = OCSP_cert_to_id(NULL, x, obj.data.x509);
    X509_free(obj.data.x509);
    X509_STORE_CTX_cleanup(&inctx);
    if (!id)
        goto err;
    if (!OCSP_request_add0_id(req, id))
        goto err;
    id = NULL;
    /* Add any extensions to the request */
    SSL_get_tlsext_status_exts(s, &exts);
    for (i = 0; i < sk_X509_EXTENSION_num(exts); i++) {
        X509_EXTENSION *ext = sk_X509_EXTENSION_value(exts, i);
        if (!OCSP_REQUEST_add_ext(req, ext, -1))
            goto err;
    }
    resp = process_responder(err, req, host, path, port, use_ssl, NULL,
                             srctx->timeout);
    if (!resp) {
        BIO_puts(err, "cert_status: error querying responder\n");
        goto done;
    }
    rspderlen = i2d_OCSP_RESPONSE(resp, &rspder);
    if (rspderlen <= 0)
        goto err;
    SSL_set_tlsext_status_ocsp_resp(s, rspder, rspderlen);
    if (srctx->verbose) {
        BIO_puts(err, "cert_status: ocsp response sent:\n");
        OCSP_RESPONSE_print(err, resp, 2);
    }
    ret = SSL_TLSEXT_ERR_OK;
 done:
    if (ret != SSL_TLSEXT_ERR_OK)
        ERR_print_errors(err);
    if (aia) {
        OPENSSL_free(host);
        OPENSSL_free(path);
        OPENSSL_free(port);
        X509_email_free(aia);
    }
    if (id)
        OCSP_CERTID_free(id);
    if (req)
        OCSP_REQUEST_free(req);
    if (resp)
        OCSP_RESPONSE_free(resp);
    return ret;
 err:
    ret = SSL_TLSEXT_ERR_ALERT_FATAL;
    goto done;
}

# ifndef OPENSSL_NO_NEXTPROTONEG
/* This is the context that we pass to next_proto_cb */
typedef struct tlsextnextprotoctx_st {
    unsigned char *data;
    unsigned int len;
} tlsextnextprotoctx;

static int next_proto_cb(SSL *s, const unsigned char **data,
                         unsigned int *len, void *arg)
{
    tlsextnextprotoctx *next_proto = arg;

    *data = next_proto->data;
    *len = next_proto->len;

    return SSL_TLSEXT_ERR_OK;
}
# endif                         /* ndef OPENSSL_NO_NEXTPROTONEG */

/* This the context that we pass to alpn_cb */
typedef struct tlsextalpnctx_st {
    unsigned char *data;
    unsigned short len;
} tlsextalpnctx;

static int alpn_cb(SSL *s, const unsigned char **out, unsigned char *outlen,
                   const unsigned char *in, unsigned int inlen, void *arg)
{
    tlsextalpnctx *alpn_ctx = arg;

    if (!s_quiet) {
        /* We can assume that |in| is syntactically valid. */
        unsigned i;
        BIO_printf(bio_s_out, "ALPN protocols advertised by the client: ");
        for (i = 0; i < inlen;) {
            if (i)
                BIO_write(bio_s_out, ", ", 2);
            BIO_write(bio_s_out, &in[i + 1], in[i]);
            i += in[i] + 1;
        }
        BIO_write(bio_s_out, "\n", 1);
    }

    if (SSL_select_next_proto
        ((unsigned char **)out, outlen, alpn_ctx->data, alpn_ctx->len, in,
         inlen) != OPENSSL_NPN_NEGOTIATED) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    if (!s_quiet) {
        BIO_printf(bio_s_out, "ALPN protocols selected: ");
        BIO_write(bio_s_out, *out, *outlen);
        BIO_write(bio_s_out, "\n", 1);
    }

    return SSL_TLSEXT_ERR_OK;
}
#endif                          /* ndef OPENSSL_NO_TLSEXT */

int MAIN(int, char **);

#ifndef OPENSSL_NO_JPAKE
static char *jpake_secret = NULL;
# define no_jpake !jpake_secret
#else
# define no_jpake 1
#endif
#ifndef OPENSSL_NO_SRP
static srpsrvparm srp_callback_parm;
#endif
#ifndef OPENSSL_NO_SRTP
static char *srtp_profiles = NULL;
#endif

int MAIN(int argc, char *argv[])
{
    X509_VERIFY_PARAM *vpm = NULL;
    int badarg = 0;
    short port = PORT;
    char *CApath = NULL, *CAfile = NULL;
    char *chCApath = NULL, *chCAfile = NULL;
    char *vfyCApath = NULL, *vfyCAfile = NULL;
    unsigned char *context = NULL;
    char *dhfile = NULL;
    int badop = 0;
    int ret = 1;
    int build_chain = 0;
    int no_tmp_rsa = 0, no_dhe = 0, no_ecdhe = 0, nocert = 0;
    int state = 0;
    const SSL_METHOD *meth = NULL;
    int socket_type = SOCK_STREAM;
    ENGINE *e = NULL;
    char *inrand = NULL;
    int s_cert_format = FORMAT_PEM, s_key_format = FORMAT_PEM;
    char *passarg = NULL, *pass = NULL;
    char *dpassarg = NULL, *dpass = NULL;
    int s_dcert_format = FORMAT_PEM, s_dkey_format = FORMAT_PEM;
    X509 *s_cert = NULL, *s_dcert = NULL;
    STACK_OF(X509) *s_chain = NULL, *s_dchain = NULL;
    EVP_PKEY *s_key = NULL, *s_dkey = NULL;
    int no_cache = 0, ext_cache = 0;
    int rev = 0, naccept = -1;
#ifndef OPENSSL_NO_TLSEXT
    EVP_PKEY *s_key2 = NULL;
    X509 *s_cert2 = NULL;
    tlsextctx tlsextcbp = { NULL, NULL, SSL_TLSEXT_ERR_ALERT_WARNING };
# ifndef OPENSSL_NO_NEXTPROTONEG
    const char *next_proto_neg_in = NULL;
    tlsextnextprotoctx next_proto = { NULL, 0 };
# endif
    const char *alpn_in = NULL;
    tlsextalpnctx alpn_ctx = { NULL, 0 };
#endif
#ifndef OPENSSL_NO_PSK
    /* by default do not send a PSK identity hint */
    static char *psk_identity_hint = NULL;
#endif
#ifndef OPENSSL_NO_SRP
    char *srpuserseed = NULL;
    char *srp_verifier_file = NULL;
#endif
    SSL_EXCERT *exc = NULL;
    SSL_CONF_CTX *cctx = NULL;
    STACK_OF(OPENSSL_STRING) *ssl_args = NULL;

    char *crl_file = NULL;
    int crl_format = FORMAT_PEM;
    int crl_download = 0;
    STACK_OF(X509_CRL) *crls = NULL;

    meth = SSLv23_server_method();

    local_argc = argc;
    local_argv = argv;

    apps_startup();
#ifdef MONOLITH
    s_server_init();
#endif

    if (bio_err == NULL)
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

    if (!load_config(bio_err, NULL))
        goto end;

    cctx = SSL_CONF_CTX_new();
    if (!cctx)
        goto end;
    SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_SERVER);
    SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_CMDLINE);

    verify_depth = 0;
#ifdef FIONBIO
    s_nbio = 0;
#endif
    s_nbio_test = 0;

    argc--;
    argv++;

    while (argc >= 1) {
        if ((strcmp(*argv, "-port") == 0) || (strcmp(*argv, "-accept") == 0)) {
            if (--argc < 1)
                goto bad;
            if (!extract_port(*(++argv), &port))
                goto bad;
        } else if (strcmp(*argv, "-naccept") == 0) {
            if (--argc < 1)
                goto bad;
            naccept = atol(*(++argv));
            if (naccept <= 0) {
                BIO_printf(bio_err, "bad accept value %s\n", *argv);
                goto bad;
            }
        } else if (strcmp(*argv, "-verify") == 0) {
            s_server_verify = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
            if (--argc < 1)
                goto bad;
            verify_depth = atoi(*(++argv));
            if (!s_quiet)
                BIO_printf(bio_err, "verify depth is %d\n", verify_depth);
        } else if (strcmp(*argv, "-Verify") == 0) {
            s_server_verify =
                SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT |
                SSL_VERIFY_CLIENT_ONCE;
            if (--argc < 1)
                goto bad;
            verify_depth = atoi(*(++argv));
            if (!s_quiet)
                BIO_printf(bio_err,
                           "verify depth is %d, must return a certificate\n",
                           verify_depth);
        } else if (strcmp(*argv, "-context") == 0) {
            if (--argc < 1)
                goto bad;
            context = (unsigned char *)*(++argv);
        } else if (strcmp(*argv, "-cert") == 0) {
            if (--argc < 1)
                goto bad;
            s_cert_file = *(++argv);
        } else if (strcmp(*argv, "-CRL") == 0) {
            if (--argc < 1)
                goto bad;
            crl_file = *(++argv);
        } else if (strcmp(*argv, "-crl_download") == 0)
            crl_download = 1;
#ifndef OPENSSL_NO_TLSEXT
        else if (strcmp(*argv, "-serverinfo") == 0) {
            if (--argc < 1)
                goto bad;
            s_serverinfo_file = *(++argv);
        }
#endif
        else if (strcmp(*argv, "-certform") == 0) {
            if (--argc < 1)
                goto bad;
            s_cert_format = str2fmt(*(++argv));
        } else if (strcmp(*argv, "-key") == 0) {
            if (--argc < 1)
                goto bad;
            s_key_file = *(++argv);
        } else if (strcmp(*argv, "-keyform") == 0) {
            if (--argc < 1)
                goto bad;
            s_key_format = str2fmt(*(++argv));
        } else if (strcmp(*argv, "-pass") == 0) {
            if (--argc < 1)
                goto bad;
            passarg = *(++argv);
        } else if (strcmp(*argv, "-cert_chain") == 0) {
            if (--argc < 1)
                goto bad;
            s_chain_file = *(++argv);
        } else if (strcmp(*argv, "-dhparam") == 0) {
            if (--argc < 1)
                goto bad;
            dhfile = *(++argv);
        } else if (strcmp(*argv, "-dcertform") == 0) {
            if (--argc < 1)
                goto bad;
            s_dcert_format = str2fmt(*(++argv));
        } else if (strcmp(*argv, "-dcert") == 0) {
            if (--argc < 1)
                goto bad;
            s_dcert_file = *(++argv);
        } else if (strcmp(*argv, "-dkeyform") == 0) {
            if (--argc < 1)
                goto bad;
            s_dkey_format = str2fmt(*(++argv));
        } else if (strcmp(*argv, "-dpass") == 0) {
            if (--argc < 1)
                goto bad;
            dpassarg = *(++argv);
        } else if (strcmp(*argv, "-dkey") == 0) {
            if (--argc < 1)
                goto bad;
            s_dkey_file = *(++argv);
        } else if (strcmp(*argv, "-dcert_chain") == 0) {
            if (--argc < 1)
                goto bad;
            s_dchain_file = *(++argv);
        } else if (strcmp(*argv, "-nocert") == 0) {
            nocert = 1;
        } else if (strcmp(*argv, "-CApath") == 0) {
            if (--argc < 1)
                goto bad;
            CApath = *(++argv);
        } else if (strcmp(*argv, "-chainCApath") == 0) {
            if (--argc < 1)
                goto bad;
            chCApath = *(++argv);
        } else if (strcmp(*argv, "-verifyCApath") == 0) {
            if (--argc < 1)
                goto bad;
            vfyCApath = *(++argv);
        } else if (strcmp(*argv, "-no_cache") == 0)
            no_cache = 1;
        else if (strcmp(*argv, "-ext_cache") == 0)
            ext_cache = 1;
        else if (strcmp(*argv, "-CRLform") == 0) {
            if (--argc < 1)
                goto bad;
            crl_format = str2fmt(*(++argv));
        } else if (args_verify(&argv, &argc, &badarg, bio_err, &vpm)) {
            if (badarg)
                goto bad;
            continue;
        } else if (args_excert(&argv, &argc, &badarg, bio_err, &exc)) {
            if (badarg)
                goto bad;
            continue;
        } else if (args_ssl(&argv, &argc, cctx, &badarg, bio_err, &ssl_args)) {
            if (badarg)
                goto bad;
            continue;
        } else if (strcmp(*argv, "-verify_return_error") == 0)
            verify_return_error = 1;
        else if (strcmp(*argv, "-verify_quiet") == 0)
            verify_quiet = 1;
        else if (strcmp(*argv, "-build_chain") == 0)
            build_chain = 1;
        else if (strcmp(*argv, "-CAfile") == 0) {
            if (--argc < 1)
                goto bad;
            CAfile = *(++argv);
        } else if (strcmp(*argv, "-chainCAfile") == 0) {
            if (--argc < 1)
                goto bad;
            chCAfile = *(++argv);
        } else if (strcmp(*argv, "-verifyCAfile") == 0) {
            if (--argc < 1)
                goto bad;
            vfyCAfile = *(++argv);
        }
#ifdef FIONBIO
        else if (strcmp(*argv, "-nbio") == 0) {
            s_nbio = 1;
        }
#endif
        else if (strcmp(*argv, "-nbio_test") == 0) {
#ifdef FIONBIO
            s_nbio = 1;
#endif
            s_nbio_test = 1;
        } else if (strcmp(*argv, "-ign_eof") == 0)
            s_ign_eof = 1;
        else if (strcmp(*argv, "-no_ign_eof") == 0)
            s_ign_eof = 0;
        else if (strcmp(*argv, "-debug") == 0) {
            s_debug = 1;
        }
#ifndef OPENSSL_NO_TLSEXT
        else if (strcmp(*argv, "-tlsextdebug") == 0)
            s_tlsextdebug = 1;
        else if (strcmp(*argv, "-status") == 0)
            s_tlsextstatus = 1;
        else if (strcmp(*argv, "-status_verbose") == 0) {
            s_tlsextstatus = 1;
            tlscstatp.verbose = 1;
        } else if (!strcmp(*argv, "-status_timeout")) {
            s_tlsextstatus = 1;
            if (--argc < 1)
                goto bad;
            tlscstatp.timeout = atoi(*(++argv));
        } else if (!strcmp(*argv, "-status_url")) {
            s_tlsextstatus = 1;
            if (--argc < 1)
                goto bad;
            if (!OCSP_parse_url(*(++argv),
                                &tlscstatp.host,
                                &tlscstatp.port,
                                &tlscstatp.path, &tlscstatp.use_ssl)) {
                BIO_printf(bio_err, "Error parsing URL\n");
                goto bad;
            }
        }
#endif
        else if (strcmp(*argv, "-msg") == 0) {
            s_msg = 1;
        } else if (strcmp(*argv, "-msgfile") == 0) {
            if (--argc < 1)
                goto bad;
            bio_s_msg = BIO_new_file(*(++argv), "w");
        }
#ifndef OPENSSL_NO_SSL_TRACE
        else if (strcmp(*argv, "-trace") == 0) {
            s_msg = 2;
        }
#endif
        else if (strcmp(*argv, "-hack") == 0) {
            hack = 1;
        } else if (strcmp(*argv, "-state") == 0) {
            state = 1;
        } else if (strcmp(*argv, "-crlf") == 0) {
            s_crlf = 1;
        } else if (strcmp(*argv, "-quiet") == 0) {
            s_quiet = 1;
        } else if (strcmp(*argv, "-brief") == 0) {
            s_quiet = 1;
            s_brief = 1;
            verify_quiet = 1;
        } else if (strcmp(*argv, "-no_tmp_rsa") == 0) {
            no_tmp_rsa = 1;
        } else if (strcmp(*argv, "-no_dhe") == 0) {
            no_dhe = 1;
        } else if (strcmp(*argv, "-no_ecdhe") == 0) {
            no_ecdhe = 1;
        } else if (strcmp(*argv, "-no_resume_ephemeral") == 0) {
            no_resume_ephemeral = 1;
        }
#ifndef OPENSSL_NO_PSK
        else if (strcmp(*argv, "-psk_hint") == 0) {
            if (--argc < 1)
                goto bad;
            psk_identity_hint = *(++argv);
        } else if (strcmp(*argv, "-psk") == 0) {
            size_t i;

            if (--argc < 1)
                goto bad;
            psk_key = *(++argv);
            for (i = 0; i < strlen(psk_key); i++) {
                if (isxdigit((unsigned char)psk_key[i]))
                    continue;
                BIO_printf(bio_err, "Not a hex number '%s'\n", *argv);
                goto bad;
            }
        }
#endif
#ifndef OPENSSL_NO_SRP
        else if (strcmp(*argv, "-srpvfile") == 0) {
            if (--argc < 1)
                goto bad;
            srp_verifier_file = *(++argv);
            meth = TLSv1_server_method();
        } else if (strcmp(*argv, "-srpuserseed") == 0) {
            if (--argc < 1)
                goto bad;
            srpuserseed = *(++argv);
            meth = TLSv1_server_method();
        }
#endif
        else if (strcmp(*argv, "-rev") == 0) {
            rev = 1;
        } else if (strcmp(*argv, "-www") == 0) {
            www = 1;
        } else if (strcmp(*argv, "-WWW") == 0) {
            www = 2;
        } else if (strcmp(*argv, "-HTTP") == 0) {
            www = 3;
        }
#ifndef OPENSSL_NO_SSL2
        else if (strcmp(*argv, "-ssl2") == 0) {
            no_ecdhe = 1;
            meth = SSLv2_server_method();
        }
#endif
#ifndef OPENSSL_NO_SSL3_METHOD
        else if (strcmp(*argv, "-ssl3") == 0) {
            meth = SSLv3_server_method();
        }
#endif
#ifndef OPENSSL_NO_TLS1
        else if (strcmp(*argv, "-tls1") == 0) {
            meth = TLSv1_server_method();
        } else if (strcmp(*argv, "-tls1_1") == 0) {
            meth = TLSv1_1_server_method();
        } else if (strcmp(*argv, "-tls1_2") == 0) {
            meth = TLSv1_2_server_method();
        }
#endif
#ifndef OPENSSL_NO_DTLS1
        else if (strcmp(*argv, "-dtls") == 0) {
            meth = DTLS_server_method();
            socket_type = SOCK_DGRAM;
        } else if (strcmp(*argv, "-dtls1") == 0) {
            meth = DTLSv1_server_method();
            socket_type = SOCK_DGRAM;
        } else if (strcmp(*argv, "-dtls1_2") == 0) {
            meth = DTLSv1_2_server_method();
            socket_type = SOCK_DGRAM;
        } else if (strcmp(*argv, "-timeout") == 0)
            enable_timeouts = 1;
        else if (strcmp(*argv, "-mtu") == 0) {
            if (--argc < 1)
                goto bad;
            socket_mtu = atol(*(++argv));
        } else if (strcmp(*argv, "-chain") == 0)
            cert_chain = 1;
#endif
        else if (strcmp(*argv, "-id_prefix") == 0) {
            if (--argc < 1)
                goto bad;
            session_id_prefix = *(++argv);
        }
#ifndef OPENSSL_NO_ENGINE
        else if (strcmp(*argv, "-engine") == 0) {
            if (--argc < 1)
                goto bad;
            engine_id = *(++argv);
        }
#endif
        else if (strcmp(*argv, "-rand") == 0) {
            if (--argc < 1)
                goto bad;
            inrand = *(++argv);
        }
#ifndef OPENSSL_NO_TLSEXT
        else if (strcmp(*argv, "-servername") == 0) {
            if (--argc < 1)
                goto bad;
            tlsextcbp.servername = *(++argv);
        } else if (strcmp(*argv, "-servername_fatal") == 0) {
            tlsextcbp.extension_error = SSL_TLSEXT_ERR_ALERT_FATAL;
        } else if (strcmp(*argv, "-cert2") == 0) {
            if (--argc < 1)
                goto bad;
            s_cert_file2 = *(++argv);
        } else if (strcmp(*argv, "-key2") == 0) {
            if (--argc < 1)
                goto bad;
            s_key_file2 = *(++argv);
        }
# ifndef OPENSSL_NO_NEXTPROTONEG
        else if (strcmp(*argv, "-nextprotoneg") == 0) {
            if (--argc < 1)
                goto bad;
            next_proto_neg_in = *(++argv);
        }
# endif
        else if (strcmp(*argv, "-alpn") == 0) {
            if (--argc < 1)
                goto bad;
            alpn_in = *(++argv);
        }
#endif
#if !defined(OPENSSL_NO_JPAKE) && !defined(OPENSSL_NO_PSK)
        else if (strcmp(*argv, "-jpake") == 0) {
            if (--argc < 1)
                goto bad;
            jpake_secret = *(++argv);
        }
#endif
#ifndef OPENSSL_NO_SRTP
        else if (strcmp(*argv, "-use_srtp") == 0) {
            if (--argc < 1)
                goto bad;
            srtp_profiles = *(++argv);
        }
#endif
        else if (strcmp(*argv, "-keymatexport") == 0) {
            if (--argc < 1)
                goto bad;
            keymatexportlabel = *(++argv);
        } else if (strcmp(*argv, "-keymatexportlen") == 0) {
            if (--argc < 1)
                goto bad;
            keymatexportlen = atoi(*(++argv));
            if (keymatexportlen == 0)
                goto bad;
        } else {
            BIO_printf(bio_err, "unknown option %s\n", *argv);
            badop = 1;
            break;
        }
        argc--;
        argv++;
    }
    if (badop) {
 bad:
        sv_usage();
        goto end;
    }
#ifndef OPENSSL_NO_DTLS1
    if (www && socket_type == SOCK_DGRAM) {
        BIO_printf(bio_err, "Can't use -HTTP, -www or -WWW with DTLS\n");
        goto end;
    }
#endif

#if !defined(OPENSSL_NO_JPAKE) && !defined(OPENSSL_NO_PSK)
    if (jpake_secret) {
        if (psk_key) {
            BIO_printf(bio_err, "Can't use JPAKE and PSK together\n");
            goto end;
        }
        psk_identity = "JPAKE";
    }
#endif

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

#ifndef OPENSSL_NO_ENGINE
    e = setup_engine(bio_err, engine_id, 1);
#endif

    if (!app_passwd(bio_err, passarg, dpassarg, &pass, &dpass)) {
        BIO_printf(bio_err, "Error getting password\n");
        goto end;
    }

    if (s_key_file == NULL)
        s_key_file = s_cert_file;
#ifndef OPENSSL_NO_TLSEXT
    if (s_key_file2 == NULL)
        s_key_file2 = s_cert_file2;
#endif

    if (!load_excert(&exc, bio_err))
        goto end;

    if (nocert == 0) {
        s_key = load_key(bio_err, s_key_file, s_key_format, 0, pass, e,
                         "server certificate private key file");
        if (!s_key) {
            ERR_print_errors(bio_err);
            goto end;
        }

        s_cert = load_cert(bio_err, s_cert_file, s_cert_format,
                           NULL, e, "server certificate file");

        if (!s_cert) {
            ERR_print_errors(bio_err);
            goto end;
        }
        if (s_chain_file) {
            s_chain = load_certs(bio_err, s_chain_file, FORMAT_PEM,
                                 NULL, e, "server certificate chain");
            if (!s_chain)
                goto end;
        }
#ifndef OPENSSL_NO_TLSEXT
        if (tlsextcbp.servername) {
            s_key2 = load_key(bio_err, s_key_file2, s_key_format, 0, pass, e,
                              "second server certificate private key file");
            if (!s_key2) {
                ERR_print_errors(bio_err);
                goto end;
            }

            s_cert2 = load_cert(bio_err, s_cert_file2, s_cert_format,
                                NULL, e, "second server certificate file");

            if (!s_cert2) {
                ERR_print_errors(bio_err);
                goto end;
            }
        }
#endif                          /* OPENSSL_NO_TLSEXT */
    }
#if !defined(OPENSSL_NO_TLSEXT)
# if !defined(OPENSSL_NO_NEXTPROTONEG)
    if (next_proto_neg_in) {
        unsigned short len;
        next_proto.data = next_protos_parse(&len, next_proto_neg_in);
        if (next_proto.data == NULL)
            goto end;
        next_proto.len = len;
    } else {
        next_proto.data = NULL;
    }
# endif
    alpn_ctx.data = NULL;
    if (alpn_in) {
        unsigned short len;
        alpn_ctx.data = next_protos_parse(&len, alpn_in);
        if (alpn_ctx.data == NULL)
            goto end;
        alpn_ctx.len = len;
    }
#endif

    if (crl_file) {
        X509_CRL *crl;
        crl = load_crl(crl_file, crl_format);
        if (!crl) {
            BIO_puts(bio_err, "Error loading CRL\n");
            ERR_print_errors(bio_err);
            goto end;
        }
        crls = sk_X509_CRL_new_null();
        if (!crls || !sk_X509_CRL_push(crls, crl)) {
            BIO_puts(bio_err, "Error adding CRL\n");
            ERR_print_errors(bio_err);
            X509_CRL_free(crl);
            goto end;
        }
    }

    if (s_dcert_file) {

        if (s_dkey_file == NULL)
            s_dkey_file = s_dcert_file;

        s_dkey = load_key(bio_err, s_dkey_file, s_dkey_format,
                          0, dpass, e, "second certificate private key file");
        if (!s_dkey) {
            ERR_print_errors(bio_err);
            goto end;
        }

        s_dcert = load_cert(bio_err, s_dcert_file, s_dcert_format,
                            NULL, e, "second server certificate file");

        if (!s_dcert) {
            ERR_print_errors(bio_err);
            goto end;
        }
        if (s_dchain_file) {
            s_dchain = load_certs(bio_err, s_dchain_file, FORMAT_PEM,
                                  NULL, e, "second server certificate chain");
            if (!s_dchain)
                goto end;
        }

    }

    if (!app_RAND_load_file(NULL, bio_err, 1) && inrand == NULL
        && !RAND_status()) {
        BIO_printf(bio_err,
                   "warning, not much extra random data, consider using the -rand option\n");
    }
    if (inrand != NULL)
        BIO_printf(bio_err, "%ld semi-random bytes loaded\n",
                   app_RAND_load_files(inrand));

    if (bio_s_out == NULL) {
        if (s_quiet && !s_debug) {
            bio_s_out = BIO_new(BIO_s_null());
            if (s_msg && !bio_s_msg)
                bio_s_msg = BIO_new_fp(stdout, BIO_NOCLOSE);
        } else {
            if (bio_s_out == NULL)
                bio_s_out = BIO_new_fp(stdout, BIO_NOCLOSE);
        }
    }
#if !defined(OPENSSL_NO_RSA) || !defined(OPENSSL_NO_DSA) || !defined(OPENSSL_NO_ECDSA)
    if (nocert)
#endif
    {
        s_cert_file = NULL;
        s_key_file = NULL;
        s_dcert_file = NULL;
        s_dkey_file = NULL;
#ifndef OPENSSL_NO_TLSEXT
        s_cert_file2 = NULL;
        s_key_file2 = NULL;
#endif
    }

    ctx = SSL_CTX_new(meth);
    if (ctx == NULL) {
        ERR_print_errors(bio_err);
        goto end;
    }
    if (session_id_prefix) {
        if (strlen(session_id_prefix) >= 32)
            BIO_printf(bio_err,
                       "warning: id_prefix is too long, only one new session will be possible\n");
        else if (strlen(session_id_prefix) >= 16)
            BIO_printf(bio_err,
                       "warning: id_prefix is too long if you use SSLv2\n");
        if (!SSL_CTX_set_generate_session_id(ctx, generate_session_id)) {
            BIO_printf(bio_err, "error setting 'id_prefix'\n");
            ERR_print_errors(bio_err);
            goto end;
        }
        BIO_printf(bio_err, "id_prefix '%s' set.\n", session_id_prefix);
    }
    SSL_CTX_set_quiet_shutdown(ctx, 1);
    if (hack)
        SSL_CTX_set_options(ctx, SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG);
    if (exc)
        ssl_ctx_set_excert(ctx, exc);

    if (state)
        SSL_CTX_set_info_callback(ctx, apps_ssl_info_callback);
    if (no_cache)
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
    else if (ext_cache)
        init_session_cache_ctx(ctx);
    else
        SSL_CTX_sess_set_cache_size(ctx, 128);

#ifndef OPENSSL_NO_SRTP
    if (srtp_profiles != NULL)
        SSL_CTX_set_tlsext_use_srtp(ctx, srtp_profiles);
#endif

#if 0
    if (cipher == NULL)
        cipher = getenv("SSL_CIPHER");
#endif

#if 0
    if (s_cert_file == NULL) {
        BIO_printf(bio_err,
                   "You must specify a certificate file for the server to use\n");
        goto end;
    }
#endif

    if ((!SSL_CTX_load_verify_locations(ctx, CAfile, CApath)) ||
        (!SSL_CTX_set_default_verify_paths(ctx))) {
        /* BIO_printf(bio_err,"X509_load_verify_locations\n"); */
        ERR_print_errors(bio_err);
        /* goto end; */
    }
    if (vpm)
        SSL_CTX_set1_param(ctx, vpm);

    ssl_ctx_add_crls(ctx, crls, 0);

    if (!args_ssl_call(ctx, bio_err, cctx, ssl_args, no_ecdhe, no_jpake))
        goto end;

    if (!ssl_load_stores(ctx, vfyCApath, vfyCAfile, chCApath, chCAfile,
                         crls, crl_download)) {
        BIO_printf(bio_err, "Error loading store locations\n");
        ERR_print_errors(bio_err);
        goto end;
    }
#ifndef OPENSSL_NO_TLSEXT
    if (s_cert2) {
        ctx2 = SSL_CTX_new(meth);
        if (ctx2 == NULL) {
            ERR_print_errors(bio_err);
            goto end;
        }
    }

    if (ctx2) {
        BIO_printf(bio_s_out, "Setting secondary ctx parameters\n");

        if (session_id_prefix) {
            if (strlen(session_id_prefix) >= 32)
                BIO_printf(bio_err,
                           "warning: id_prefix is too long, only one new session will be possible\n");
            else if (strlen(session_id_prefix) >= 16)
                BIO_printf(bio_err,
                           "warning: id_prefix is too long if you use SSLv2\n");
            if (!SSL_CTX_set_generate_session_id(ctx2, generate_session_id)) {
                BIO_printf(bio_err, "error setting 'id_prefix'\n");
                ERR_print_errors(bio_err);
                goto end;
            }
            BIO_printf(bio_err, "id_prefix '%s' set.\n", session_id_prefix);
        }
        SSL_CTX_set_quiet_shutdown(ctx2, 1);
        if (hack)
            SSL_CTX_set_options(ctx2, SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG);
        if (exc)
            ssl_ctx_set_excert(ctx2, exc);

        if (state)
            SSL_CTX_set_info_callback(ctx2, apps_ssl_info_callback);

        if (no_cache)
            SSL_CTX_set_session_cache_mode(ctx2, SSL_SESS_CACHE_OFF);
        else if (ext_cache)
            init_session_cache_ctx(ctx2);
        else
            SSL_CTX_sess_set_cache_size(ctx2, 128);

        if ((!SSL_CTX_load_verify_locations(ctx2, CAfile, CApath)) ||
            (!SSL_CTX_set_default_verify_paths(ctx2))) {
            ERR_print_errors(bio_err);
        }
        if (vpm)
            SSL_CTX_set1_param(ctx2, vpm);

        ssl_ctx_add_crls(ctx2, crls, 0);

        if (!args_ssl_call(ctx2, bio_err, cctx, ssl_args, no_ecdhe, no_jpake))
            goto end;

    }
# ifndef OPENSSL_NO_NEXTPROTONEG
    if (next_proto.data)
        SSL_CTX_set_next_protos_advertised_cb(ctx, next_proto_cb,
                                              &next_proto);
# endif
    if (alpn_ctx.data)
        SSL_CTX_set_alpn_select_cb(ctx, alpn_cb, &alpn_ctx);
#endif

#ifndef OPENSSL_NO_DH
    if (!no_dhe) {
        DH *dh = NULL;

        if (dhfile)
            dh = load_dh_param(dhfile);
        else if (s_cert_file)
            dh = load_dh_param(s_cert_file);

        if (dh != NULL) {
            BIO_printf(bio_s_out, "Setting temp DH parameters\n");
        } else {
            BIO_printf(bio_s_out, "Using default temp DH parameters\n");
            dh = get_dh2048();
            if (dh == NULL) {
                ERR_print_errors(bio_err);
                goto end;
            }
        }
        (void)BIO_flush(bio_s_out);

        SSL_CTX_set_tmp_dh(ctx, dh);
# ifndef OPENSSL_NO_TLSEXT
        if (ctx2) {
            if (!dhfile) {
                DH *dh2 = load_dh_param(s_cert_file2);
                if (dh2 != NULL) {
                    BIO_printf(bio_s_out, "Setting temp DH parameters\n");
                    (void)BIO_flush(bio_s_out);

                    DH_free(dh);
                    dh = dh2;
                }
            }
            SSL_CTX_set_tmp_dh(ctx2, dh);
        }
# endif
        DH_free(dh);
    }
#endif

    if (!set_cert_key_stuff(ctx, s_cert, s_key, s_chain, build_chain))
        goto end;
#ifndef OPENSSL_NO_TLSEXT
    if (s_serverinfo_file != NULL
        && !SSL_CTX_use_serverinfo_file(ctx, s_serverinfo_file)) {
        ERR_print_errors(bio_err);
        goto end;
    }
#endif
#ifndef OPENSSL_NO_TLSEXT
    if (ctx2 && !set_cert_key_stuff(ctx2, s_cert2, s_key2, NULL, build_chain))
        goto end;
#endif
    if (s_dcert != NULL) {
        if (!set_cert_key_stuff(ctx, s_dcert, s_dkey, s_dchain, build_chain))
            goto end;
    }
#ifndef OPENSSL_NO_RSA
# if 1
    if (!no_tmp_rsa) {
        SSL_CTX_set_tmp_rsa_callback(ctx, tmp_rsa_cb);
#  ifndef OPENSSL_NO_TLSEXT
        if (ctx2)
            SSL_CTX_set_tmp_rsa_callback(ctx2, tmp_rsa_cb);
#  endif
    }
# else
    if (!no_tmp_rsa && SSL_CTX_need_tmp_RSA(ctx)) {
        RSA *rsa;

        BIO_printf(bio_s_out, "Generating temp (512 bit) RSA key...");
        BIO_flush(bio_s_out);

        rsa = RSA_generate_key(512, RSA_F4, NULL);

        if (!SSL_CTX_set_tmp_rsa(ctx, rsa)) {
            ERR_print_errors(bio_err);
            goto end;
        }
#  ifndef OPENSSL_NO_TLSEXT
        if (ctx2) {
            if (!SSL_CTX_set_tmp_rsa(ctx2, rsa)) {
                ERR_print_errors(bio_err);
                goto end;
            }
        }
#  endif
        RSA_free(rsa);
        BIO_printf(bio_s_out, "\n");
    }
# endif
#endif

#ifndef OPENSSL_NO_PSK
# ifdef OPENSSL_NO_JPAKE
    if (psk_key != NULL)
# else
    if (psk_key != NULL || jpake_secret)
# endif
    {
        if (s_debug)
            BIO_printf(bio_s_out,
                       "PSK key given or JPAKE in use, setting server callback\n");
        SSL_CTX_set_psk_server_callback(ctx, psk_server_cb);
    }

    if (!SSL_CTX_use_psk_identity_hint(ctx, psk_identity_hint)) {
        BIO_printf(bio_err, "error setting PSK identity hint to context\n");
        ERR_print_errors(bio_err);
        goto end;
    }
#endif

    SSL_CTX_set_verify(ctx, s_server_verify, verify_callback);
    SSL_CTX_set_session_id_context(ctx, (void *)&s_server_session_id_context,
                                   sizeof s_server_session_id_context);

    /* Set DTLS cookie generation and verification callbacks */
    SSL_CTX_set_cookie_generate_cb(ctx, generate_cookie_callback);
    SSL_CTX_set_cookie_verify_cb(ctx, verify_cookie_callback);

#ifndef OPENSSL_NO_TLSEXT
    if (ctx2) {
        SSL_CTX_set_verify(ctx2, s_server_verify, verify_callback);
        SSL_CTX_set_session_id_context(ctx2,
                                       (void *)&s_server_session_id_context,
                                       sizeof s_server_session_id_context);

        tlsextcbp.biodebug = bio_s_out;
        SSL_CTX_set_tlsext_servername_callback(ctx2, ssl_servername_cb);
        SSL_CTX_set_tlsext_servername_arg(ctx2, &tlsextcbp);
        SSL_CTX_set_tlsext_servername_callback(ctx, ssl_servername_cb);
        SSL_CTX_set_tlsext_servername_arg(ctx, &tlsextcbp);
    }
#endif

#ifndef OPENSSL_NO_SRP
    if (srp_verifier_file != NULL) {
        srp_callback_parm.vb = SRP_VBASE_new(srpuserseed);
        srp_callback_parm.user = NULL;
        srp_callback_parm.login = NULL;
        if ((ret =
             SRP_VBASE_init(srp_callback_parm.vb,
                            srp_verifier_file)) != SRP_NO_ERROR) {
            BIO_printf(bio_err,
                       "Cannot initialize SRP verifier file \"%s\":ret=%d\n",
                       srp_verifier_file, ret);
            goto end;
        }
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, verify_callback);
        SSL_CTX_set_srp_cb_arg(ctx, &srp_callback_parm);
        SSL_CTX_set_srp_username_callback(ctx, ssl_srp_server_param_cb);
    } else
#endif
    if (CAfile != NULL) {
        SSL_CTX_set_client_CA_list(ctx, SSL_load_client_CA_file(CAfile));
#ifndef OPENSSL_NO_TLSEXT
        if (ctx2)
            SSL_CTX_set_client_CA_list(ctx2, SSL_load_client_CA_file(CAfile));
#endif
    }

    BIO_printf(bio_s_out, "ACCEPT\n");
    (void)BIO_flush(bio_s_out);
    if (rev)
        do_server(port, socket_type, &accept_socket, rev_body, context,
                  naccept);
    else if (www)
        do_server(port, socket_type, &accept_socket, www_body, context,
                  naccept);
    else
        do_server(port, socket_type, &accept_socket, sv_body, context,
                  naccept);
    print_stats(bio_s_out, ctx);
    ret = 0;
 end:
    if (ctx != NULL)
        SSL_CTX_free(ctx);
    if (s_cert)
        X509_free(s_cert);
    if (crls)
        sk_X509_CRL_pop_free(crls, X509_CRL_free);
    if (s_dcert)
        X509_free(s_dcert);
    if (s_key)
        EVP_PKEY_free(s_key);
    if (s_dkey)
        EVP_PKEY_free(s_dkey);
    if (s_chain)
        sk_X509_pop_free(s_chain, X509_free);
    if (s_dchain)
        sk_X509_pop_free(s_dchain, X509_free);
    if (pass)
        OPENSSL_free(pass);
    if (dpass)
        OPENSSL_free(dpass);
    if (vpm)
        X509_VERIFY_PARAM_free(vpm);
    free_sessions();
#ifndef OPENSSL_NO_TLSEXT
    if (tlscstatp.host)
        OPENSSL_free(tlscstatp.host);
    if (tlscstatp.port)
        OPENSSL_free(tlscstatp.port);
    if (tlscstatp.path)
        OPENSSL_free(tlscstatp.path);
    if (ctx2 != NULL)
        SSL_CTX_free(ctx2);
    if (s_cert2)
        X509_free(s_cert2);
    if (s_key2)
        EVP_PKEY_free(s_key2);
    if (serverinfo_in != NULL)
        BIO_free(serverinfo_in);
# ifndef OPENSSL_NO_NEXTPROTONEG
    if (next_proto.data)
        OPENSSL_free(next_proto.data);
# endif
    if (alpn_ctx.data)
        OPENSSL_free(alpn_ctx.data);
#endif
    ssl_excert_free(exc);
    if (ssl_args)
        sk_OPENSSL_STRING_free(ssl_args);
    if (cctx)
        SSL_CONF_CTX_free(cctx);
#ifndef OPENSSL_NO_JPAKE
    if (jpake_secret && psk_key)
        OPENSSL_free(psk_key);
#endif
    if (bio_s_out != NULL) {
        BIO_free(bio_s_out);
        bio_s_out = NULL;
    }
    if (bio_s_msg != NULL) {
        BIO_free(bio_s_msg);
        bio_s_msg = NULL;
    }
    apps_shutdown();
    OPENSSL_EXIT(ret);
}

static void print_stats(BIO *bio, SSL_CTX *ssl_ctx)
{
    BIO_printf(bio, "%4ld items in the session cache\n",
               SSL_CTX_sess_number(ssl_ctx));
    BIO_printf(bio, "%4ld client connects (SSL_connect())\n",
               SSL_CTX_sess_connect(ssl_ctx));
    BIO_printf(bio, "%4ld client renegotiates (SSL_connect())\n",
               SSL_CTX_sess_connect_renegotiate(ssl_ctx));
    BIO_printf(bio, "%4ld client connects that finished\n",
               SSL_CTX_sess_connect_good(ssl_ctx));
    BIO_printf(bio, "%4ld server accepts (SSL_accept())\n",
               SSL_CTX_sess_accept(ssl_ctx));
    BIO_printf(bio, "%4ld server renegotiates (SSL_accept())\n",
               SSL_CTX_sess_accept_renegotiate(ssl_ctx));
    BIO_printf(bio, "%4ld server accepts that finished\n",
               SSL_CTX_sess_accept_good(ssl_ctx));
    BIO_printf(bio, "%4ld session cache hits\n", SSL_CTX_sess_hits(ssl_ctx));
    BIO_printf(bio, "%4ld session cache misses\n",
               SSL_CTX_sess_misses(ssl_ctx));
    BIO_printf(bio, "%4ld session cache timeouts\n",
               SSL_CTX_sess_timeouts(ssl_ctx));
    BIO_printf(bio, "%4ld callback cache hits\n",
               SSL_CTX_sess_cb_hits(ssl_ctx));
    BIO_printf(bio, "%4ld cache full overflows (%ld allowed)\n",
               SSL_CTX_sess_cache_full(ssl_ctx),
               SSL_CTX_sess_get_cache_size(ssl_ctx));
}

static int sv_body(char *hostname, int s, int stype, unsigned char *context)
{
    char *buf = NULL;
    fd_set readfds;
    int ret = 1, width;
    int k, i;
    unsigned long l;
    SSL *con = NULL;
    BIO *sbio;
#ifndef OPENSSL_NO_KRB5
    KSSL_CTX *kctx;
#endif
    struct timeval timeout;
#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS) || defined(OPENSSL_SYS_NETWARE) || defined(OPENSSL_SYS_BEOS_R5)
    struct timeval tv;
#else
    struct timeval *timeoutp;
#endif

    if ((buf = OPENSSL_malloc(bufsize)) == NULL) {
        BIO_printf(bio_err, "out of memory\n");
        goto err;
    }
#ifdef FIONBIO
    if (s_nbio) {
        unsigned long sl = 1;

        if (!s_quiet)
            BIO_printf(bio_err, "turning on non blocking io\n");
        if (BIO_socket_ioctl(s, FIONBIO, &sl) < 0)
            ERR_print_errors(bio_err);
    }
#endif

    if (con == NULL) {
        con = SSL_new(ctx);
#ifndef OPENSSL_NO_TLSEXT
        if (s_tlsextdebug) {
            SSL_set_tlsext_debug_callback(con, tlsext_cb);
            SSL_set_tlsext_debug_arg(con, bio_s_out);
        }
        if (s_tlsextstatus) {
            SSL_CTX_set_tlsext_status_cb(ctx, cert_status_cb);
            tlscstatp.err = bio_err;
            SSL_CTX_set_tlsext_status_arg(ctx, &tlscstatp);
        }
#endif
#ifndef OPENSSL_NO_KRB5
        if ((kctx = kssl_ctx_new()) != NULL) {
            SSL_set0_kssl_ctx(con, kctx);
            kssl_ctx_setstring(kctx, KSSL_SERVICE, KRB5SVC);
            kssl_ctx_setstring(kctx, KSSL_KEYTAB, KRB5KEYTAB);
        }
#endif                          /* OPENSSL_NO_KRB5 */
        if (context)
            SSL_set_session_id_context(con, context, strlen((char *)context));
    }
    SSL_clear(con);
#if 0
# ifdef TLSEXT_TYPE_opaque_prf_input
    SSL_set_tlsext_opaque_prf_input(con, "Test server", 11);
# endif
#endif

    if (stype == SOCK_DGRAM) {

        sbio = BIO_new_dgram(s, BIO_NOCLOSE);

        if (enable_timeouts) {
            timeout.tv_sec = 0;
            timeout.tv_usec = DGRAM_RCV_TIMEOUT;
            BIO_ctrl(sbio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

            timeout.tv_sec = 0;
            timeout.tv_usec = DGRAM_SND_TIMEOUT;
            BIO_ctrl(sbio, BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &timeout);
        }

        if (socket_mtu) {
            if (socket_mtu < DTLS_get_link_min_mtu(con)) {
                BIO_printf(bio_err, "MTU too small. Must be at least %ld\n",
                           DTLS_get_link_min_mtu(con));
                ret = -1;
                BIO_free(sbio);
                goto err;
            }
            SSL_set_options(con, SSL_OP_NO_QUERY_MTU);
            if (!DTLS_set_link_mtu(con, socket_mtu)) {
                BIO_printf(bio_err, "Failed to set MTU\n");
                ret = -1;
                BIO_free(sbio);
                goto err;
            }
        } else
            /* want to do MTU discovery */
            BIO_ctrl(sbio, BIO_CTRL_DGRAM_MTU_DISCOVER, 0, NULL);

        /* turn on cookie exchange */
        SSL_set_options(con, SSL_OP_COOKIE_EXCHANGE);
    } else
        sbio = BIO_new_socket(s, BIO_NOCLOSE);

    if (s_nbio_test) {
        BIO *test;

        test = BIO_new(BIO_f_nbio_test());
        sbio = BIO_push(test, sbio);
    }
#ifndef OPENSSL_NO_JPAKE
    if (jpake_secret)
        jpake_server_auth(bio_s_out, sbio, jpake_secret);
#endif

    SSL_set_bio(con, sbio, sbio);
    SSL_set_accept_state(con);
    /* SSL_set_fd(con,s); */

    if (s_debug) {
        SSL_set_debug(con, 1);
        BIO_set_callback(SSL_get_rbio(con), bio_dump_callback);
        BIO_set_callback_arg(SSL_get_rbio(con), (char *)bio_s_out);
    }
    if (s_msg) {
#ifndef OPENSSL_NO_SSL_TRACE
        if (s_msg == 2)
            SSL_set_msg_callback(con, SSL_trace);
        else
#endif
            SSL_set_msg_callback(con, msg_cb);
        SSL_set_msg_callback_arg(con, bio_s_msg ? bio_s_msg : bio_s_out);
    }
#ifndef OPENSSL_NO_TLSEXT
    if (s_tlsextdebug) {
        SSL_set_tlsext_debug_callback(con, tlsext_cb);
        SSL_set_tlsext_debug_arg(con, bio_s_out);
    }
#endif

    width = s + 1;
    for (;;) {
        int read_from_terminal;
        int read_from_sslcon;

        read_from_terminal = 0;
        read_from_sslcon = SSL_pending(con);

        if (!read_from_sslcon) {
            FD_ZERO(&readfds);
#if !defined(OPENSSL_SYS_WINDOWS) && !defined(OPENSSL_SYS_MSDOS) && !defined(OPENSSL_SYS_NETWARE) && !defined(OPENSSL_SYS_BEOS_R5)
            openssl_fdset(fileno(stdin), &readfds);
#endif
            openssl_fdset(s, &readfds);
            /*
             * Note: under VMS with SOCKETSHR the second parameter is
             * currently of type (int *) whereas under other systems it is
             * (void *) if you don't have a cast it will choke the compiler:
             * if you do have a cast then you can either go for (int *) or
             * (void *).
             */
#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS) || defined(OPENSSL_SYS_NETWARE)
            /*
             * Under DOS (non-djgpp) and Windows we can't select on stdin:
             * only on sockets. As a workaround we timeout the select every
             * second and check for any keypress. In a proper Windows
             * application we wouldn't do this because it is inefficient.
             */
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            i = select(width, (void *)&readfds, NULL, NULL, &tv);
            if ((i < 0) || (!i && !_kbhit()))
                continue;
            if (_kbhit())
                read_from_terminal = 1;
#elif defined(OPENSSL_SYS_BEOS_R5)
            /* Under BeOS-R5 the situation is similar to DOS */
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            (void)fcntl(fileno(stdin), F_SETFL, O_NONBLOCK);
            i = select(width, (void *)&readfds, NULL, NULL, &tv);
            if ((i < 0) || (!i && read(fileno(stdin), buf, 0) < 0))
                continue;
            if (read(fileno(stdin), buf, 0) >= 0)
                read_from_terminal = 1;
            (void)fcntl(fileno(stdin), F_SETFL, 0);
#else
            if ((SSL_version(con) == DTLS1_VERSION) &&
                DTLSv1_get_timeout(con, &timeout))
                timeoutp = &timeout;
            else
                timeoutp = NULL;

            i = select(width, (void *)&readfds, NULL, NULL, timeoutp);

            if ((SSL_version(con) == DTLS1_VERSION)
                && DTLSv1_handle_timeout(con) > 0) {
                BIO_printf(bio_err, "TIMEOUT occured\n");
            }

            if (i <= 0)
                continue;
            if (FD_ISSET(fileno(stdin), &readfds))
                read_from_terminal = 1;
#endif
            if (FD_ISSET(s, &readfds))
                read_from_sslcon = 1;
        }
        if (read_from_terminal) {
            if (s_crlf) {
                int j, lf_num;

                i = raw_read_stdin(buf, bufsize / 2);
                lf_num = 0;
                /* both loops are skipped when i <= 0 */
                for (j = 0; j < i; j++)
                    if (buf[j] == '\n')
                        lf_num++;
                for (j = i - 1; j >= 0; j--) {
                    buf[j + lf_num] = buf[j];
                    if (buf[j] == '\n') {
                        lf_num--;
                        i++;
                        buf[j + lf_num] = '\r';
                    }
                }
                assert(lf_num == 0);
            } else
                i = raw_read_stdin(buf, bufsize);
            if (!s_quiet && !s_brief) {
                if ((i <= 0) || (buf[0] == 'Q')) {
                    BIO_printf(bio_s_out, "DONE\n");
                    SHUTDOWN(s);
                    close_accept_socket();
                    ret = -11;
                    goto err;
                }
                if ((i <= 0) || (buf[0] == 'q')) {
                    BIO_printf(bio_s_out, "DONE\n");
                    if (SSL_version(con) != DTLS1_VERSION)
                        SHUTDOWN(s);
                    /*
                     * close_accept_socket(); ret= -11;
                     */
                    goto err;
                }
#ifndef OPENSSL_NO_HEARTBEATS
                if ((buf[0] == 'B') && ((buf[1] == '\n') || (buf[1] == '\r'))) {
                    BIO_printf(bio_err, "HEARTBEATING\n");
                    SSL_heartbeat(con);
                    i = 0;
                    continue;
                }
#endif
                if ((buf[0] == 'r') && ((buf[1] == '\n') || (buf[1] == '\r'))) {
                    SSL_renegotiate(con);
                    i = SSL_do_handshake(con);
                    printf("SSL_do_handshake -> %d\n", i);
                    i = 0;      /* 13; */
                    continue;
                    /*
                     * strcpy(buf,"server side RE-NEGOTIATE\n");
                     */
                }
                if ((buf[0] == 'R') && ((buf[1] == '\n') || (buf[1] == '\r'))) {
                    SSL_set_verify(con,
                                   SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE,
                                   NULL);
                    SSL_renegotiate(con);
                    i = SSL_do_handshake(con);
                    printf("SSL_do_handshake -> %d\n", i);
                    i = 0;      /* 13; */
                    continue;
                    /*
                     * strcpy(buf,"server side RE-NEGOTIATE asking for client
                     * cert\n");
                     */
                }
                if (buf[0] == 'P') {
                    static const char *str = "Lets print some clear text\n";
                    BIO_write(SSL_get_wbio(con), str, strlen(str));
                }
                if (buf[0] == 'S') {
                    print_stats(bio_s_out, SSL_get_SSL_CTX(con));
                }
            }
#ifdef CHARSET_EBCDIC
            ebcdic2ascii(buf, buf, i);
#endif
            l = k = 0;
            for (;;) {
                /* should do a select for the write */
#ifdef RENEG
                {
                    static count = 0;
                    if (++count == 100) {
                        count = 0;
                        SSL_renegotiate(con);
                    }
                }
#endif
                k = SSL_write(con, &(buf[l]), (unsigned int)i);
#ifndef OPENSSL_NO_SRP
                while (SSL_get_error(con, k) == SSL_ERROR_WANT_X509_LOOKUP) {
                    BIO_printf(bio_s_out, "LOOKUP renego during write\n");
                    srp_callback_parm.user =
                        SRP_VBASE_get_by_user(srp_callback_parm.vb,
                                              srp_callback_parm.login);
                    if (srp_callback_parm.user)
                        BIO_printf(bio_s_out, "LOOKUP done %s\n",
                                   srp_callback_parm.user->info);
                    else
                        BIO_printf(bio_s_out, "LOOKUP not successful\n");
                    k = SSL_write(con, &(buf[l]), (unsigned int)i);
                }
#endif
                switch (SSL_get_error(con, k)) {
                case SSL_ERROR_NONE:
                    break;
                case SSL_ERROR_WANT_WRITE:
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_X509_LOOKUP:
                    BIO_printf(bio_s_out, "Write BLOCK\n");
                    break;
                case SSL_ERROR_SYSCALL:
                case SSL_ERROR_SSL:
                    BIO_printf(bio_s_out, "ERROR\n");
                    ERR_print_errors(bio_err);
                    ret = 1;
                    goto err;
                    /* break; */
                case SSL_ERROR_ZERO_RETURN:
                    BIO_printf(bio_s_out, "DONE\n");
                    ret = 1;
                    goto err;
                }
                if (k > 0) {
                    l += k;
                    i -= k;
                }
                if (i <= 0)
                    break;
            }
        }
        if (read_from_sslcon) {
            if (!SSL_is_init_finished(con)) {
                i = init_ssl_connection(con);

                if (i < 0) {
                    ret = 0;
                    goto err;
                } else if (i == 0) {
                    ret = 1;
                    goto err;
                }
            } else {
 again:
                i = SSL_read(con, (char *)buf, bufsize);
#ifndef OPENSSL_NO_SRP
                while (SSL_get_error(con, i) == SSL_ERROR_WANT_X509_LOOKUP) {
                    BIO_printf(bio_s_out, "LOOKUP renego during read\n");
                    srp_callback_parm.user =
                        SRP_VBASE_get_by_user(srp_callback_parm.vb,
                                              srp_callback_parm.login);
                    if (srp_callback_parm.user)
                        BIO_printf(bio_s_out, "LOOKUP done %s\n",
                                   srp_callback_parm.user->info);
                    else
                        BIO_printf(bio_s_out, "LOOKUP not successful\n");
                    i = SSL_read(con, (char *)buf, bufsize);
                }
#endif
                switch (SSL_get_error(con, i)) {
                case SSL_ERROR_NONE:
#ifdef CHARSET_EBCDIC
                    ascii2ebcdic(buf, buf, i);
#endif
                    raw_write_stdout(buf, (unsigned int)i);
                    if (SSL_pending(con))
                        goto again;
                    break;
                case SSL_ERROR_WANT_WRITE:
                case SSL_ERROR_WANT_READ:
                    BIO_printf(bio_s_out, "Read BLOCK\n");
                    break;
                case SSL_ERROR_SYSCALL:
                case SSL_ERROR_SSL:
                    BIO_printf(bio_s_out, "ERROR\n");
                    ERR_print_errors(bio_err);
                    ret = 1;
                    goto err;
                case SSL_ERROR_ZERO_RETURN:
                    BIO_printf(bio_s_out, "DONE\n");
                    ret = 1;
                    goto err;
                }
            }
        }
    }
 err:
    if (con != NULL) {
        BIO_printf(bio_s_out, "shutting down SSL\n");
#if 1
        SSL_set_shutdown(con, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
#else
        SSL_shutdown(con);
#endif
        SSL_free(con);
    }
    BIO_printf(bio_s_out, "CONNECTION CLOSED\n");
    if (buf != NULL) {
        OPENSSL_cleanse(buf, bufsize);
        OPENSSL_free(buf);
    }
    if (ret >= 0)
        BIO_printf(bio_s_out, "ACCEPT\n");
    return (ret);
}

static void close_accept_socket(void)
{
    BIO_printf(bio_err, "shutdown accept socket\n");
    if (accept_socket >= 0) {
        SHUTDOWN2(accept_socket);
    }
}

static int init_ssl_connection(SSL *con)
{
    int i;
    const char *str;
    X509 *peer;
    long verify_error;
    MS_STATIC char buf[BUFSIZ];
#ifndef OPENSSL_NO_KRB5
    char *client_princ;
#endif
#if !defined(OPENSSL_NO_TLSEXT) && !defined(OPENSSL_NO_NEXTPROTONEG)
    const unsigned char *next_proto_neg;
    unsigned next_proto_neg_len;
#endif
    unsigned char *exportedkeymat;

    i = SSL_accept(con);
#ifdef CERT_CB_TEST_RETRY
    {
        while (i <= 0 && SSL_get_error(con, i) == SSL_ERROR_WANT_X509_LOOKUP
               && SSL_state(con) == SSL3_ST_SR_CLNT_HELLO_C) {
            fprintf(stderr,
                    "LOOKUP from certificate callback during accept\n");
            i = SSL_accept(con);
        }
    }
#endif
#ifndef OPENSSL_NO_SRP
    while (i <= 0 && SSL_get_error(con, i) == SSL_ERROR_WANT_X509_LOOKUP) {
        BIO_printf(bio_s_out, "LOOKUP during accept %s\n",
                   srp_callback_parm.login);
        srp_callback_parm.user =
            SRP_VBASE_get_by_user(srp_callback_parm.vb,
                                  srp_callback_parm.login);
        if (srp_callback_parm.user)
            BIO_printf(bio_s_out, "LOOKUP done %s\n",
                       srp_callback_parm.user->info);
        else
            BIO_printf(bio_s_out, "LOOKUP not successful\n");
        i = SSL_accept(con);
    }
#endif

    if (i <= 0) {
        if (BIO_sock_should_retry(i)) {
            BIO_printf(bio_s_out, "DELAY\n");
            return (1);
        }

        BIO_printf(bio_err, "ERROR\n");
        verify_error = SSL_get_verify_result(con);
        if (verify_error != X509_V_OK) {
            BIO_printf(bio_err, "verify error:%s\n",
                       X509_verify_cert_error_string(verify_error));
        }
        /* Always print any error messages */
        ERR_print_errors(bio_err);
        return (0);
    }

    if (s_brief)
        print_ssl_summary(bio_err, con);

    PEM_write_bio_SSL_SESSION(bio_s_out, SSL_get_session(con));

    peer = SSL_get_peer_certificate(con);
    if (peer != NULL) {
        BIO_printf(bio_s_out, "Client certificate\n");
        PEM_write_bio_X509(bio_s_out, peer);
        X509_NAME_oneline(X509_get_subject_name(peer), buf, sizeof buf);
        BIO_printf(bio_s_out, "subject=%s\n", buf);
        X509_NAME_oneline(X509_get_issuer_name(peer), buf, sizeof buf);
        BIO_printf(bio_s_out, "issuer=%s\n", buf);
        X509_free(peer);
    }

    if (SSL_get_shared_ciphers(con, buf, sizeof buf) != NULL)
        BIO_printf(bio_s_out, "Shared ciphers:%s\n", buf);
    str = SSL_CIPHER_get_name(SSL_get_current_cipher(con));
    ssl_print_sigalgs(bio_s_out, con);
#ifndef OPENSSL_NO_EC
    ssl_print_point_formats(bio_s_out, con);
    ssl_print_curves(bio_s_out, con, 0);
#endif
    BIO_printf(bio_s_out, "CIPHER is %s\n", (str != NULL) ? str : "(NONE)");

#if !defined(OPENSSL_NO_TLSEXT) && !defined(OPENSSL_NO_NEXTPROTONEG)
    SSL_get0_next_proto_negotiated(con, &next_proto_neg, &next_proto_neg_len);
    if (next_proto_neg) {
        BIO_printf(bio_s_out, "NEXTPROTO is ");
        BIO_write(bio_s_out, next_proto_neg, next_proto_neg_len);
        BIO_printf(bio_s_out, "\n");
    }
#endif
#ifndef OPENSSL_NO_SRTP
    {
        SRTP_PROTECTION_PROFILE *srtp_profile
            = SSL_get_selected_srtp_profile(con);

        if (srtp_profile)
            BIO_printf(bio_s_out, "SRTP Extension negotiated, profile=%s\n",
                       srtp_profile->name);
    }
#endif
    if (SSL_cache_hit(con))
        BIO_printf(bio_s_out, "Reused session-id\n");
    if (SSL_ctrl(con, SSL_CTRL_GET_FLAGS, 0, NULL) &
        TLS1_FLAGS_TLS_PADDING_BUG)
        BIO_printf(bio_s_out, "Peer has incorrect TLSv1 block padding\n");
#ifndef OPENSSL_NO_KRB5
    client_princ = kssl_ctx_get0_client_princ(SSL_get0_kssl_ctx(con));
    if (client_princ != NULL) {
        BIO_printf(bio_s_out, "Kerberos peer principal is %s\n",
                   client_princ);
    }
#endif                          /* OPENSSL_NO_KRB5 */
    BIO_printf(bio_s_out, "Secure Renegotiation IS%s supported\n",
               SSL_get_secure_renegotiation_support(con) ? "" : " NOT");
    if (keymatexportlabel != NULL) {
        BIO_printf(bio_s_out, "Keying material exporter:\n");
        BIO_printf(bio_s_out, "    Label: '%s'\n", keymatexportlabel);
        BIO_printf(bio_s_out, "    Length: %i bytes\n", keymatexportlen);
        exportedkeymat = OPENSSL_malloc(keymatexportlen);
        if (exportedkeymat != NULL) {
            if (!SSL_export_keying_material(con, exportedkeymat,
                                            keymatexportlen,
                                            keymatexportlabel,
                                            strlen(keymatexportlabel),
                                            NULL, 0, 0)) {
                BIO_printf(bio_s_out, "    Error\n");
            } else {
                BIO_printf(bio_s_out, "    Keying material: ");
                for (i = 0; i < keymatexportlen; i++)
                    BIO_printf(bio_s_out, "%02X", exportedkeymat[i]);
                BIO_printf(bio_s_out, "\n");
            }
            OPENSSL_free(exportedkeymat);
        }
    }

    return (1);
}

#ifndef OPENSSL_NO_DH
static DH *load_dh_param(const char *dhfile)
{
    DH *ret = NULL;
    BIO *bio;

    if ((bio = BIO_new_file(dhfile, "r")) == NULL)
        goto err;
    ret = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
 err:
    if (bio != NULL)
        BIO_free(bio);
    return (ret);
}
#endif
#ifndef OPENSSL_NO_KRB5
char *client_princ;
#endif

#if 0
static int load_CA(SSL_CTX *ctx, char *file)
{
    FILE *in;
    X509 *x = NULL;

    if ((in = fopen(file, "r")) == NULL)
        return (0);

    for (;;) {
        if (PEM_read_X509(in, &x, NULL) == NULL)
            break;
        SSL_CTX_add_client_CA(ctx, x);
    }
    if (x != NULL)
        X509_free(x);
    fclose(in);
    return (1);
}
#endif

static int www_body(char *hostname, int s, int stype, unsigned char *context)
{
    char *buf = NULL;
    int ret = 1;
    int i, j, k, dot;
    SSL *con;
    const SSL_CIPHER *c;
    BIO *io, *ssl_bio, *sbio;
#ifndef OPENSSL_NO_KRB5
    KSSL_CTX *kctx;
#endif

    buf = OPENSSL_malloc(bufsize);
    if (buf == NULL)
        return (0);
    io = BIO_new(BIO_f_buffer());
    ssl_bio = BIO_new(BIO_f_ssl());
    if ((io == NULL) || (ssl_bio == NULL))
        goto err;

#ifdef FIONBIO
    if (s_nbio) {
        unsigned long sl = 1;

        if (!s_quiet)
            BIO_printf(bio_err, "turning on non blocking io\n");
        if (BIO_socket_ioctl(s, FIONBIO, &sl) < 0)
            ERR_print_errors(bio_err);
    }
#endif

    /* lets make the output buffer a reasonable size */
    if (!BIO_set_write_buffer_size(io, bufsize))
        goto err;

    if ((con = SSL_new(ctx)) == NULL)
        goto err;
#ifndef OPENSSL_NO_TLSEXT
    if (s_tlsextdebug) {
        SSL_set_tlsext_debug_callback(con, tlsext_cb);
        SSL_set_tlsext_debug_arg(con, bio_s_out);
    }
#endif
#ifndef OPENSSL_NO_KRB5
    if ((kctx = kssl_ctx_new()) != NULL) {
        kssl_ctx_setstring(kctx, KSSL_SERVICE, KRB5SVC);
        kssl_ctx_setstring(kctx, KSSL_KEYTAB, KRB5KEYTAB);
    }
#endif                          /* OPENSSL_NO_KRB5 */
    if (context)
        SSL_set_session_id_context(con, context, strlen((char *)context));

    sbio = BIO_new_socket(s, BIO_NOCLOSE);
    if (s_nbio_test) {
        BIO *test;

        test = BIO_new(BIO_f_nbio_test());
        sbio = BIO_push(test, sbio);
    }
    SSL_set_bio(con, sbio, sbio);
    SSL_set_accept_state(con);

    /* SSL_set_fd(con,s); */
    BIO_set_ssl(ssl_bio, con, BIO_CLOSE);
    BIO_push(io, ssl_bio);
#ifdef CHARSET_EBCDIC
    io = BIO_push(BIO_new(BIO_f_ebcdic_filter()), io);
#endif

    if (s_debug) {
        SSL_set_debug(con, 1);
        BIO_set_callback(SSL_get_rbio(con), bio_dump_callback);
        BIO_set_callback_arg(SSL_get_rbio(con), (char *)bio_s_out);
    }
    if (s_msg) {
#ifndef OPENSSL_NO_SSL_TRACE
        if (s_msg == 2)
            SSL_set_msg_callback(con, SSL_trace);
        else
#endif
            SSL_set_msg_callback(con, msg_cb);
        SSL_set_msg_callback_arg(con, bio_s_msg ? bio_s_msg : bio_s_out);
    }

    for (;;) {
        if (hack) {
            i = SSL_accept(con);
#ifndef OPENSSL_NO_SRP
            while (i <= 0
                   && SSL_get_error(con, i) == SSL_ERROR_WANT_X509_LOOKUP) {
                BIO_printf(bio_s_out, "LOOKUP during accept %s\n",
                           srp_callback_parm.login);
                srp_callback_parm.user =
                    SRP_VBASE_get_by_user(srp_callback_parm.vb,
                                          srp_callback_parm.login);
                if (srp_callback_parm.user)
                    BIO_printf(bio_s_out, "LOOKUP done %s\n",
                               srp_callback_parm.user->info);
                else
                    BIO_printf(bio_s_out, "LOOKUP not successful\n");
                i = SSL_accept(con);
            }
#endif
            switch (SSL_get_error(con, i)) {
            case SSL_ERROR_NONE:
                break;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_X509_LOOKUP:
                continue;
            case SSL_ERROR_SYSCALL:
            case SSL_ERROR_SSL:
            case SSL_ERROR_ZERO_RETURN:
                ret = 1;
                goto err;
                /* break; */
            }

            SSL_renegotiate(con);
            SSL_write(con, NULL, 0);
        }

        i = BIO_gets(io, buf, bufsize - 1);
        if (i < 0) {            /* error */
            if (!BIO_should_retry(io)) {
                if (!s_quiet)
                    ERR_print_errors(bio_err);
                goto err;
            } else {
                BIO_printf(bio_s_out, "read R BLOCK\n");
#ifndef OPENSSL_NO_SRP
                if (BIO_should_io_special(io)
                    && BIO_get_retry_reason(io) == BIO_RR_SSL_X509_LOOKUP) {
                    BIO_printf(bio_s_out, "LOOKUP renego during read\n");
                    srp_callback_parm.user =
                        SRP_VBASE_get_by_user(srp_callback_parm.vb,
                                              srp_callback_parm.login);
                    if (srp_callback_parm.user)
                        BIO_printf(bio_s_out, "LOOKUP done %s\n",
                                   srp_callback_parm.user->info);
                    else
                        BIO_printf(bio_s_out, "LOOKUP not successful\n");
                    continue;
                }
#endif
#if defined(OPENSSL_SYS_NETWARE)
                delay(1000);
#elif !defined(OPENSSL_SYS_MSDOS) && !defined(__DJGPP__)
                sleep(1);
#endif
                continue;
            }
        } else if (i == 0) {    /* end of input */
            ret = 1;
            goto end;
        }

        /* else we have data */
        if (((www == 1) && (strncmp("GET ", buf, 4) == 0)) ||
            ((www == 2) && (strncmp("GET /stats ", buf, 11) == 0))) {
            char *p;
            X509 *peer;
            STACK_OF(SSL_CIPHER) *sk;
            static const char *space = "                          ";

            BIO_puts(io,
                     "HTTP/1.0 200 ok\r\nContent-type: text/html\r\n\r\n");
            BIO_puts(io, "<HTML><BODY BGCOLOR=\"#ffffff\">\n");
            BIO_puts(io, "<pre>\n");
/*                      BIO_puts(io,SSLeay_version(SSLEAY_VERSION));*/
            BIO_puts(io, "\n");
            for (i = 0; i < local_argc; i++) {
                BIO_puts(io, local_argv[i]);
                BIO_write(io, " ", 1);
            }
            BIO_puts(io, "\n");

            BIO_printf(io,
                       "Secure Renegotiation IS%s supported\n",
                       SSL_get_secure_renegotiation_support(con) ?
                       "" : " NOT");

            /*
             * The following is evil and should not really be done
             */
            BIO_printf(io, "Ciphers supported in s_server binary\n");
            sk = SSL_get_ciphers(con);
            j = sk_SSL_CIPHER_num(sk);
            for (i = 0; i < j; i++) {
                c = sk_SSL_CIPHER_value(sk, i);
                BIO_printf(io, "%-11s:%-25s",
                           SSL_CIPHER_get_version(c), SSL_CIPHER_get_name(c));
                if ((((i + 1) % 2) == 0) && (i + 1 != j))
                    BIO_puts(io, "\n");
            }
            BIO_puts(io, "\n");
            p = SSL_get_shared_ciphers(con, buf, bufsize);
            if (p != NULL) {
                BIO_printf(io,
                           "---\nCiphers common between both SSL end points:\n");
                j = i = 0;
                while (*p) {
                    if (*p == ':') {
                        BIO_write(io, space, 26 - j);
                        i++;
                        j = 0;
                        BIO_write(io, ((i % 3) ? " " : "\n"), 1);
                    } else {
                        BIO_write(io, p, 1);
                        j++;
                    }
                    p++;
                }
                BIO_puts(io, "\n");
            }
            ssl_print_sigalgs(io, con);
#ifndef OPENSSL_NO_EC
            ssl_print_curves(io, con, 0);
#endif
            BIO_printf(io, (SSL_cache_hit(con)
                            ? "---\nReused, " : "---\nNew, "));
            c = SSL_get_current_cipher(con);
            BIO_printf(io, "%s, Cipher is %s\n",
                       SSL_CIPHER_get_version(c), SSL_CIPHER_get_name(c));
            SSL_SESSION_print(io, SSL_get_session(con));
            BIO_printf(io, "---\n");
            print_stats(io, SSL_get_SSL_CTX(con));
            BIO_printf(io, "---\n");
            peer = SSL_get_peer_certificate(con);
            if (peer != NULL) {
                BIO_printf(io, "Client certificate\n");
                X509_print(io, peer);
                PEM_write_bio_X509(io, peer);
            } else
                BIO_puts(io, "no client certificate available\n");
            BIO_puts(io, "</BODY></HTML>\r\n\r\n");
            break;
        } else if ((www == 2 || www == 3)
                   && (strncmp("GET /", buf, 5) == 0)) {
            BIO *file;
            char *p, *e;
            static const char *text =
                "HTTP/1.0 200 ok\r\nContent-type: text/plain\r\n\r\n";

            /* skip the '/' */
            p = &(buf[5]);

            dot = 1;
            for (e = p; *e != '\0'; e++) {
                if (e[0] == ' ')
                    break;

                switch (dot) {
                case 1:
                    dot = (e[0] == '.') ? 2 : 0;
                    break;
                case 2:
                    dot = (e[0] == '.') ? 3 : 0;
                    break;
                case 3:
                    dot = (e[0] == '/') ? -1 : 0;
                    break;
                }
                if (dot == 0)
                    dot = (e[0] == '/') ? 1 : 0;
            }
            dot = (dot == 3) || (dot == -1); /* filename contains ".."
                                              * component */

            if (*e == '\0') {
                BIO_puts(io, text);
                BIO_printf(io, "'%s' is an invalid file name\r\n", p);
                break;
            }
            *e = '\0';

            if (dot) {
                BIO_puts(io, text);
                BIO_printf(io, "'%s' contains '..' reference\r\n", p);
                break;
            }

            if (*p == '/') {
                BIO_puts(io, text);
                BIO_printf(io, "'%s' is an invalid path\r\n", p);
                break;
            }
#if 0
            /* append if a directory lookup */
            if (e[-1] == '/')
                strcat(p, "index.html");
#endif

            /* if a directory, do the index thang */
            if (app_isdir(p) > 0) {
#if 0                           /* must check buffer size */
                strcat(p, "/index.html");
#else
                BIO_puts(io, text);
                BIO_printf(io, "'%s' is a directory\r\n", p);
                break;
#endif
            }

            if ((file = BIO_new_file(p, "r")) == NULL) {
                BIO_puts(io, text);
                BIO_printf(io, "Error opening '%s'\r\n", p);
                ERR_print_errors(io);
                break;
            }

            if (!s_quiet)
                BIO_printf(bio_err, "FILE:%s\n", p);

            if (www == 2) {
                i = strlen(p);
                if (((i > 5) && (strcmp(&(p[i - 5]), ".html") == 0)) ||
                    ((i > 4) && (strcmp(&(p[i - 4]), ".php") == 0)) ||
                    ((i > 4) && (strcmp(&(p[i - 4]), ".htm") == 0)))
                    BIO_puts(io,
                             "HTTP/1.0 200 ok\r\nContent-type: text/html\r\n\r\n");
                else
                    BIO_puts(io,
                             "HTTP/1.0 200 ok\r\nContent-type: text/plain\r\n\r\n");
            }
            /* send the file */
            for (;;) {
                i = BIO_read(file, buf, bufsize);
                if (i <= 0)
                    break;

#ifdef RENEG
                total_bytes += i;
                fprintf(stderr, "%d\n", i);
                if (total_bytes > 3 * 1024) {
                    total_bytes = 0;
                    fprintf(stderr, "RENEGOTIATE\n");
                    SSL_renegotiate(con);
                }
#endif

                for (j = 0; j < i;) {
#ifdef RENEG
                    {
                        static count = 0;
                        if (++count == 13) {
                            SSL_renegotiate(con);
                        }
                    }
#endif
                    k = BIO_write(io, &(buf[j]), i - j);
                    if (k <= 0) {
                        if (!BIO_should_retry(io))
                            goto write_error;
                        else {
                            BIO_printf(bio_s_out, "rwrite W BLOCK\n");
                        }
                    } else {
                        j += k;
                    }
                }
            }
 write_error:
            BIO_free(file);
            break;
        }
    }

    for (;;) {
        i = (int)BIO_flush(io);
        if (i <= 0) {
            if (!BIO_should_retry(io))
                break;
        } else
            break;
    }
 end:
#if 1
    /* make sure we re-use sessions */
    SSL_set_shutdown(con, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
#else
    /* This kills performance */
    /*
     * SSL_shutdown(con); A shutdown gets sent in the BIO_free_all(io)
     * procession
     */
#endif

 err:

    if (ret >= 0)
        BIO_printf(bio_s_out, "ACCEPT\n");

    if (buf != NULL)
        OPENSSL_free(buf);
    if (io != NULL)
        BIO_free_all(io);
/*      if (ssl_bio != NULL) BIO_free(ssl_bio);*/
    return (ret);
}

static int rev_body(char *hostname, int s, int stype, unsigned char *context)
{
    char *buf = NULL;
    int i;
    int ret = 1;
    SSL *con;
    BIO *io, *ssl_bio, *sbio;
#ifndef OPENSSL_NO_KRB5
    KSSL_CTX *kctx;
#endif

    buf = OPENSSL_malloc(bufsize);
    if (buf == NULL)
        return (0);
    io = BIO_new(BIO_f_buffer());
    ssl_bio = BIO_new(BIO_f_ssl());
    if ((io == NULL) || (ssl_bio == NULL))
        goto err;

    /* lets make the output buffer a reasonable size */
    if (!BIO_set_write_buffer_size(io, bufsize))
        goto err;

    if ((con = SSL_new(ctx)) == NULL)
        goto err;
#ifndef OPENSSL_NO_TLSEXT
    if (s_tlsextdebug) {
        SSL_set_tlsext_debug_callback(con, tlsext_cb);
        SSL_set_tlsext_debug_arg(con, bio_s_out);
    }
#endif
#ifndef OPENSSL_NO_KRB5
    if ((kctx = kssl_ctx_new()) != NULL) {
        kssl_ctx_setstring(kctx, KSSL_SERVICE, KRB5SVC);
        kssl_ctx_setstring(kctx, KSSL_KEYTAB, KRB5KEYTAB);
    }
#endif                          /* OPENSSL_NO_KRB5 */
    if (context)
        SSL_set_session_id_context(con, context, strlen((char *)context));

    sbio = BIO_new_socket(s, BIO_NOCLOSE);
    SSL_set_bio(con, sbio, sbio);
    SSL_set_accept_state(con);

    BIO_set_ssl(ssl_bio, con, BIO_CLOSE);
    BIO_push(io, ssl_bio);
#ifdef CHARSET_EBCDIC
    io = BIO_push(BIO_new(BIO_f_ebcdic_filter()), io);
#endif

    if (s_debug) {
        SSL_set_debug(con, 1);
        BIO_set_callback(SSL_get_rbio(con), bio_dump_callback);
        BIO_set_callback_arg(SSL_get_rbio(con), (char *)bio_s_out);
    }
    if (s_msg) {
#ifndef OPENSSL_NO_SSL_TRACE
        if (s_msg == 2)
            SSL_set_msg_callback(con, SSL_trace);
        else
#endif
            SSL_set_msg_callback(con, msg_cb);
        SSL_set_msg_callback_arg(con, bio_s_msg ? bio_s_msg : bio_s_out);
    }

    for (;;) {
        i = BIO_do_handshake(io);
        if (i > 0)
            break;
        if (!BIO_should_retry(io)) {
            BIO_puts(bio_err, "CONNECTION FAILURE\n");
            ERR_print_errors(bio_err);
            goto end;
        }
#ifndef OPENSSL_NO_SRP
        if (BIO_should_io_special(io)
            && BIO_get_retry_reason(io) == BIO_RR_SSL_X509_LOOKUP) {
            BIO_printf(bio_s_out, "LOOKUP renego during accept\n");
            srp_callback_parm.user =
                SRP_VBASE_get_by_user(srp_callback_parm.vb,
                                      srp_callback_parm.login);
            if (srp_callback_parm.user)
                BIO_printf(bio_s_out, "LOOKUP done %s\n",
                           srp_callback_parm.user->info);
            else
                BIO_printf(bio_s_out, "LOOKUP not successful\n");
            continue;
        }
#endif
    }
    BIO_printf(bio_err, "CONNECTION ESTABLISHED\n");
    print_ssl_summary(bio_err, con);

    for (;;) {
        i = BIO_gets(io, buf, bufsize - 1);
        if (i < 0) {            /* error */
            if (!BIO_should_retry(io)) {
                if (!s_quiet)
                    ERR_print_errors(bio_err);
                goto err;
            } else {
                BIO_printf(bio_s_out, "read R BLOCK\n");
#ifndef OPENSSL_NO_SRP
                if (BIO_should_io_special(io)
                    && BIO_get_retry_reason(io) == BIO_RR_SSL_X509_LOOKUP) {
                    BIO_printf(bio_s_out, "LOOKUP renego during read\n");
                    srp_callback_parm.user =
                        SRP_VBASE_get_by_user(srp_callback_parm.vb,
                                              srp_callback_parm.login);
                    if (srp_callback_parm.user)
                        BIO_printf(bio_s_out, "LOOKUP done %s\n",
                                   srp_callback_parm.user->info);
                    else
                        BIO_printf(bio_s_out, "LOOKUP not successful\n");
                    continue;
                }
#endif
#if defined(OPENSSL_SYS_NETWARE)
                delay(1000);
#elif !defined(OPENSSL_SYS_MSDOS) && !defined(__DJGPP__)
                sleep(1);
#endif
                continue;
            }
        } else if (i == 0) {    /* end of input */
            ret = 1;
            BIO_printf(bio_err, "CONNECTION CLOSED\n");
            goto end;
        } else {
            char *p = buf + i - 1;
            while (i && (*p == '\n' || *p == '\r')) {
                p--;
                i--;
            }
            if (!s_ign_eof && i == 5 && !strncmp(buf, "CLOSE", 5)) {
                ret = 1;
                BIO_printf(bio_err, "CONNECTION CLOSED\n");
                goto end;
            }
            BUF_reverse((unsigned char *)buf, NULL, i);
            buf[i] = '\n';
            BIO_write(io, buf, i + 1);
            for (;;) {
                i = BIO_flush(io);
                if (i > 0)
                    break;
                if (!BIO_should_retry(io))
                    goto end;
            }
        }
    }
 end:
    /* make sure we re-use sessions */
    SSL_set_shutdown(con, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);

 err:

    if (buf != NULL)
        OPENSSL_free(buf);
    if (io != NULL)
        BIO_free_all(io);
    return (ret);
}

#ifndef OPENSSL_NO_RSA
static RSA MS_CALLBACK *tmp_rsa_cb(SSL *s, int is_export, int keylength)
{
    BIGNUM *bn = NULL;
    static RSA *rsa_tmp = NULL;

    if (!rsa_tmp && ((bn = BN_new()) == NULL))
        BIO_printf(bio_err, "Allocation error in generating RSA key\n");
    if (!rsa_tmp && bn) {
        if (!s_quiet) {
            BIO_printf(bio_err, "Generating temp (%d bit) RSA key...",
                       keylength);
            (void)BIO_flush(bio_err);
        }
        if (!BN_set_word(bn, RSA_F4) || ((rsa_tmp = RSA_new()) == NULL) ||
            !RSA_generate_key_ex(rsa_tmp, keylength, bn, NULL)) {
            if (rsa_tmp)
                RSA_free(rsa_tmp);
            rsa_tmp = NULL;
        }
        if (!s_quiet) {
            BIO_printf(bio_err, "\n");
            (void)BIO_flush(bio_err);
        }
        BN_free(bn);
    }
    return (rsa_tmp);
}
#endif

#define MAX_SESSION_ID_ATTEMPTS 10
static int generate_session_id(const SSL *ssl, unsigned char *id,
                               unsigned int *id_len)
{
    unsigned int count = 0;
    do {
        if (RAND_pseudo_bytes(id, *id_len) < 0)
            return 0;
        /*
         * Prefix the session_id with the required prefix. NB: If our prefix
         * is too long, clip it - but there will be worse effects anyway, eg.
         * the server could only possibly create 1 session ID (ie. the
         * prefix!) so all future session negotiations will fail due to
         * conflicts.
         */
        memcpy(id, session_id_prefix,
               (strlen(session_id_prefix) < *id_len) ?
               strlen(session_id_prefix) : *id_len);
    }
    while (SSL_has_matching_session_id(ssl, id, *id_len) &&
           (++count < MAX_SESSION_ID_ATTEMPTS));
    if (count >= MAX_SESSION_ID_ATTEMPTS)
        return 0;
    return 1;
}

/*
 * By default s_server uses an in-memory cache which caches SSL_SESSION
 * structures without any serialisation. This hides some bugs which only
 * become apparent in deployed servers. By implementing a basic external
 * session cache some issues can be debugged using s_server.
 */

typedef struct simple_ssl_session_st {
    unsigned char *id;
    unsigned int idlen;
    unsigned char *der;
    int derlen;
    struct simple_ssl_session_st *next;
} simple_ssl_session;

static simple_ssl_session *first = NULL;

static int add_session(SSL *ssl, SSL_SESSION *session)
{
    simple_ssl_session *sess;
    unsigned char *p;

    sess = OPENSSL_malloc(sizeof(simple_ssl_session));
    if (!sess) {
        BIO_printf(bio_err, "Out of memory adding session to external cache\n");
        return 0;
    }

    SSL_SESSION_get_id(session, &sess->idlen);
    sess->derlen = i2d_SSL_SESSION(session, NULL);

    sess->id = BUF_memdup(SSL_SESSION_get_id(session, NULL), sess->idlen);

    sess->der = OPENSSL_malloc(sess->derlen);
    if (!sess->id || !sess->der) {
        BIO_printf(bio_err, "Out of memory adding session to external cache\n");

        if (sess->id)
            OPENSSL_free(sess->id);
        if (sess->der)
            OPENSSL_free(sess->der);
        OPENSSL_free(sess);
        return 0;
    }
    p = sess->der;
    i2d_SSL_SESSION(session, &p);

    sess->next = first;
    first = sess;
    BIO_printf(bio_err, "New session added to external cache\n");
    return 0;
}

static SSL_SESSION *get_session(SSL *ssl, unsigned char *id, int idlen,
                                int *do_copy)
{
    simple_ssl_session *sess;
    *do_copy = 0;
    for (sess = first; sess; sess = sess->next) {
        if (idlen == (int)sess->idlen && !memcmp(sess->id, id, idlen)) {
            const unsigned char *p = sess->der;
            BIO_printf(bio_err, "Lookup session: cache hit\n");
            return d2i_SSL_SESSION(NULL, &p, sess->derlen);
        }
    }
    BIO_printf(bio_err, "Lookup session: cache miss\n");
    return NULL;
}

static void del_session(SSL_CTX *sctx, SSL_SESSION *session)
{
    simple_ssl_session *sess, *prev = NULL;
    const unsigned char *id;
    unsigned int idlen;
    id = SSL_SESSION_get_id(session, &idlen);
    for (sess = first; sess; sess = sess->next) {
        if (idlen == sess->idlen && !memcmp(sess->id, id, idlen)) {
            if (prev)
                prev->next = sess->next;
            else
                first = sess->next;
            OPENSSL_free(sess->id);
            OPENSSL_free(sess->der);
            OPENSSL_free(sess);
            return;
        }
        prev = sess;
    }
}

static void init_session_cache_ctx(SSL_CTX *sctx)
{
    SSL_CTX_set_session_cache_mode(sctx,
                                   SSL_SESS_CACHE_NO_INTERNAL |
                                   SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_new_cb(sctx, add_session);
    SSL_CTX_sess_set_get_cb(sctx, get_session);
    SSL_CTX_sess_set_remove_cb(sctx, del_session);
}

static void free_sessions(void)
{
    simple_ssl_session *sess, *tsess;
    for (sess = first; sess;) {
        OPENSSL_free(sess->id);
        OPENSSL_free(sess->der);
        tsess = sess;
        sess = sess->next;
        OPENSSL_free(tsess);
    }
    first = NULL;
}
