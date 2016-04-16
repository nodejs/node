/* */
/*-
 * easy-tls.h -- generic TLS proxy.
 * $Id: easy-tls.h,v 1.1 2001/09/17 19:06:59 bodo Exp $
 */
/*
 * (c) Copyright 1999 Bodo Moeller.  All rights reserved.
 */

#ifndef HEADER_TLS_H
# define HEADER_TLS_H

# ifndef HEADER_SSL_H
typedef struct ssl_ctx_st SSL_CTX;
# endif

# define TLS_INFO_SIZE 512      /* max. # of bytes written to infofd */

void tls_set_dhe1024(int i, void *apparg);
/*
 * Generate DHE parameters: i >= 0 deterministic (i selects seed), i < 0
 * random (may take a while). tls_create_ctx calls this with random
 * non-negative i if the application has never called it.
 */

void tls_rand_seed(void);
int tls_rand_seed_from_file(const char *filename, size_t n, void *apparg);
void tls_rand_seed_from_memory(const void *buf, size_t n);

struct tls_create_ctx_args {
    int client_p;
    const char *certificate_file;
    const char *key_file;
    const char *ca_file;
    int verify_depth;
    int fail_unless_verified;
    int export_p;
};
struct tls_create_ctx_args tls_create_ctx_defaultargs(void);
/*
 * struct tls_create_ctx_args is similar to a conventional argument list, but
 * it can provide default values and allows for future extension.
 */
SSL_CTX *tls_create_ctx(struct tls_create_ctx_args, void *apparg);

struct tls_start_proxy_args {
    int fd;
    int client_p;
    SSL_CTX *ctx;
    pid_t *pid;
    int *infofd;
};
struct tls_start_proxy_args tls_start_proxy_defaultargs(void);
/*
 * tls_start_proxy return value *MUST* be checked! 0 means ok, otherwise
 * we've probably run out of some resources.
 */
int tls_start_proxy(struct tls_start_proxy_args, void *apparg);

#endif
