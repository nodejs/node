/* apps/s_apps.h */
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
 * Copyright (c) 1998-2018 The OpenSSL Project.  All rights reserved.
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
/* conflicts with winsock2 stuff on netware */
#if !defined(OPENSSL_SYS_NETWARE)
# include <sys/types.h>
#endif
#include <openssl/opensslconf.h>

#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS)
# include <conio.h>
#endif

#if defined(OPENSSL_SYS_MSDOS) && !defined(_WIN32)
# define _kbhit kbhit
#endif

#if defined(OPENSSL_SYS_VMS) && !defined(FD_SET)
/*
 * VAX C does not defined fd_set and friends, but it's actually quite simple
 */
/* These definitions are borrowed from SOCKETSHR.       /Richard Levitte */
# define MAX_NOFILE      32
# define NBBY             8     /* number of bits in a byte */

# ifndef FD_SETSIZE
#  define FD_SETSIZE      MAX_NOFILE
# endif                         /* FD_SETSIZE */

/* How many things we'll allow select to use. 0 if unlimited */
# define MAXSELFD        MAX_NOFILE
typedef int fd_mask;            /* int here! VMS prototypes int, not long */
# define NFDBITS (sizeof(fd_mask) * NBBY)/* bits per mask (power of 2!) */
# define NFDSHIFT 5             /* Shift based on above */

typedef fd_mask fd_set;
# define FD_SET(n, p)    (*(p) |= (1 << ((n) % NFDBITS)))
# define FD_CLR(n, p)    (*(p) &= ~(1 << ((n) % NFDBITS)))
# define FD_ISSET(n, p)  (*(p) & (1 << ((n) % NFDBITS)))
# define FD_ZERO(p)      memset((char *)(p), 0, sizeof(*(p)))
#endif

#define PORT            4433
#define PORT_STR        "4433"
#define PROTOCOL        "tcp"

int do_server(int port, int type, int *ret,
              int (*cb) (int s, int stype, unsigned char *context),
              unsigned char *context, int naccept);
#ifdef HEADER_X509_H
int MS_CALLBACK verify_callback(int ok, X509_STORE_CTX *ctx);
#endif
#ifdef HEADER_SSL_H
int set_cert_stuff(SSL_CTX *ctx, char *cert_file, char *key_file);
int set_cert_key_stuff(SSL_CTX *ctx, X509 *cert, EVP_PKEY *key,
                       STACK_OF(X509) *chain, int build_chain);
int ssl_print_sigalgs(BIO *out, SSL *s);
int ssl_print_point_formats(BIO *out, SSL *s);
int ssl_print_curves(BIO *out, SSL *s, int noshared);
#endif
int ssl_print_tmp_key(BIO *out, SSL *s);
int init_client(int *sock, char *server, int port, int type);
int should_retry(int i);
int extract_port(char *str, short *port_ptr);
int extract_host_port(char *str, char **host_ptr, unsigned char *ip,
                      short *p);

long MS_CALLBACK bio_dump_callback(BIO *bio, int cmd, const char *argp,
                                   int argi, long argl, long ret);

#ifdef HEADER_SSL_H
void MS_CALLBACK apps_ssl_info_callback(const SSL *s, int where, int ret);
void MS_CALLBACK msg_cb(int write_p, int version, int content_type,
                        const void *buf, size_t len, SSL *ssl, void *arg);
void MS_CALLBACK tlsext_cb(SSL *s, int client_server, int type,
                           unsigned char *data, int len, void *arg);
#endif

int MS_CALLBACK generate_cookie_callback(SSL *ssl, unsigned char *cookie,
                                         unsigned int *cookie_len);
int MS_CALLBACK verify_cookie_callback(SSL *ssl, unsigned char *cookie,
                                       unsigned int cookie_len);

typedef struct ssl_excert_st SSL_EXCERT;

void ssl_ctx_set_excert(SSL_CTX *ctx, SSL_EXCERT *exc);
void ssl_excert_free(SSL_EXCERT *exc);
int args_excert(char ***pargs, int *pargc,
                int *badarg, BIO *err, SSL_EXCERT **pexc);
int load_excert(SSL_EXCERT **pexc, BIO *err);
void print_ssl_summary(BIO *bio, SSL *s);
#ifdef HEADER_SSL_H
int args_ssl(char ***pargs, int *pargc, SSL_CONF_CTX *cctx,
             int *badarg, BIO *err, STACK_OF(OPENSSL_STRING) **pstr,
             int *no_prot_opt);
int args_ssl_call(SSL_CTX *ctx, BIO *err, SSL_CONF_CTX *cctx,
                  STACK_OF(OPENSSL_STRING) *str, int no_ecdhe, int no_jpake);
int ssl_ctx_add_crls(SSL_CTX *ctx, STACK_OF(X509_CRL) *crls,
                     int crl_download);
int ssl_load_stores(SSL_CTX *ctx, const char *vfyCApath,
                    const char *vfyCAfile, const char *chCApath,
                    const char *chCAfile, STACK_OF(X509_CRL) *crls,
                    int crl_download);
#endif
