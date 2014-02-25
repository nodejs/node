// Copyright (C) 2013 - Will Glozer.  All rights reserved.

#include <pthread.h>

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "ssl.h"

static pthread_mutex_t *locks;

static void ssl_lock(int mode, int n, const char *file, int line) {
    pthread_mutex_t *lock = &locks[n];
    if (mode & CRYPTO_LOCK) {
        pthread_mutex_lock(lock);
    } else {
        pthread_mutex_unlock(lock);
    }
}

static unsigned long ssl_id() {
    return (unsigned long) pthread_self();
}

SSL_CTX *ssl_init() {
    SSL_CTX *ctx = NULL;

    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    if ((locks = calloc(CRYPTO_num_locks(), sizeof(pthread_mutex_t)))) {
        for (int i = 0; i < CRYPTO_num_locks(); i++) {
            pthread_mutex_init(&locks[i], NULL);
        }

        CRYPTO_set_locking_callback(ssl_lock);
        CRYPTO_set_id_callback(ssl_id);

        if ((ctx = SSL_CTX_new(SSLv23_client_method()))) {
            SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
            SSL_CTX_set_verify_depth(ctx, 0);
            SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
            SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_CLIENT);
        }
    }

    return ctx;
}

status ssl_connect(connection *c) {
    int r;
    SSL_set_fd(c->ssl, c->fd);
    if ((r = SSL_connect(c->ssl)) != 1) {
        switch (SSL_get_error(c->ssl, r)) {
            case SSL_ERROR_WANT_READ:  return RETRY;
            case SSL_ERROR_WANT_WRITE: return RETRY;
            default:                   return ERROR;
        }
    }
    return OK;
}

status ssl_close(connection *c) {
    SSL_shutdown(c->ssl);
    SSL_clear(c->ssl);
    return OK;
}

status ssl_read(connection *c, size_t *n) {
    int r;
    if ((r = SSL_read(c->ssl, c->buf, sizeof(c->buf))) <= 0) {
        switch (SSL_get_error(c->ssl, r)) {
            case SSL_ERROR_WANT_READ:  return RETRY;
            case SSL_ERROR_WANT_WRITE: return RETRY;
            default:                   return ERROR;
        }
    }
    *n = (size_t) r;
    return OK;
}

status ssl_write(connection *c, char *buf, size_t len, size_t *n) {
    int r;
    if ((r = SSL_write(c->ssl, buf, len)) <= 0) {
        switch (SSL_get_error(c->ssl, r)) {
            case SSL_ERROR_WANT_READ:  return RETRY;
            case SSL_ERROR_WANT_WRITE: return RETRY;
            default:                   return ERROR;
        }
    }
    *n = (size_t) r;
    return OK;
}

size_t ssl_readable(connection *c) {
    return SSL_pending(c->ssl);
}
