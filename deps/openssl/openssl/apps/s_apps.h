/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

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
# define FD_ZERO(p)      memset((p), 0, sizeof(*(p)))
#endif

#define PORT            "4433"
#define PROTOCOL        "tcp"

typedef int (*do_server_cb)(int s, int stype, unsigned char *context);
int do_server(int *accept_sock, const char *host, const char *port,
              int family, int type,
              do_server_cb cb,
              unsigned char *context, int naccept);
#ifdef HEADER_X509_H
int verify_callback(int ok, X509_STORE_CTX *ctx);
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
int init_client(int *sock, const char *host, const char *port,
                int family, int type);
int should_retry(int i);

long bio_dump_callback(BIO *bio, int cmd, const char *argp,
                       int argi, long argl, long ret);

#ifdef HEADER_SSL_H
void apps_ssl_info_callback(const SSL *s, int where, int ret);
void msg_cb(int write_p, int version, int content_type, const void *buf,
            size_t len, SSL *ssl, void *arg);
void tlsext_cb(SSL *s, int client_server, int type, const unsigned char *data,
               int len, void *arg);
#endif

int generate_cookie_callback(SSL *ssl, unsigned char *cookie,
                             unsigned int *cookie_len);
int verify_cookie_callback(SSL *ssl, const unsigned char *cookie,
                           unsigned int cookie_len);

typedef struct ssl_excert_st SSL_EXCERT;

void ssl_ctx_set_excert(SSL_CTX *ctx, SSL_EXCERT *exc);
void ssl_excert_free(SSL_EXCERT *exc);
int args_excert(int option, SSL_EXCERT **pexc);
int load_excert(SSL_EXCERT **pexc);
void print_verify_detail(SSL *s, BIO *bio);
void print_ssl_summary(SSL *s);
#ifdef HEADER_SSL_H
int config_ctx(SSL_CONF_CTX *cctx, STACK_OF(OPENSSL_STRING) *str, SSL_CTX *ctx);
int ssl_ctx_add_crls(SSL_CTX *ctx, STACK_OF(X509_CRL) *crls,
                     int crl_download);
int ssl_load_stores(SSL_CTX *ctx, const char *vfyCApath,
                    const char *vfyCAfile, const char *chCApath,
                    const char *chCAfile, STACK_OF(X509_CRL) *crls,
                    int crl_download);
void ssl_ctx_security_debug(SSL_CTX *ctx, int verbose);
#endif
