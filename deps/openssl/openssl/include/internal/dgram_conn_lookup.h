/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_INTERNAL_DGRAM_CONN_LOOKUP_H
#define OSSL_INTERNAL_DGRAM_CONN_LOOKUP_H
#pragma once

#include <openssl/ssl.h>
#include "internal/dgram_demux.h"

#ifndef OPENSSL_NO_DTLS

/*
 * DGRAM_CONN_LOOKUP - Address-based connection lookup for DTLS
 * =============================================================
 *
 * This provides connection lookup by peer address (IP:port) for DTLS.
 * QUIC uses its own QUIC_LCIDM for CID-based lookup and does not use
 * this interface.
 *
 * The lookup uses the peer address from the DGRAM_URXE to find the
 * associated connection.
 */

/* Forward declarations */
typedef struct dgram_conn_lookup_st DGRAM_CONN_LOOKUP;
typedef struct dgram_conn_lookup_methods_st DGRAM_CONN_LOOKUP_METHODS;

/* Callback type for iterating over connections */
typedef void (*ossl_dgram_conn_lookup_iter_fn)(SSL *ssl, const BIO_ADDR *peer,
    void *arg);

struct dgram_conn_lookup_methods_st {
    SSL *(*lookup)(DGRAM_CONN_LOOKUP *lookup, const DGRAM_URXE *e);
    int (*register_conn)(DGRAM_CONN_LOOKUP *lookup, const DGRAM_URXE *e, SSL *ssl);
    int (*register_conn_addr)(DGRAM_CONN_LOOKUP *lookup, const BIO_ADDR *peer, SSL *ssl);
    int (*unregister_conn)(DGRAM_CONN_LOOKUP *lookup, const BIO_ADDR *peer);
    void (*foreach)(DGRAM_CONN_LOOKUP *lookup, ossl_dgram_conn_lookup_iter_fn cb,
        void *arg);
    void (*free)(DGRAM_CONN_LOOKUP *lookup);
    size_t (*num_items)(const DGRAM_CONN_LOOKUP *lookup);
};

struct dgram_conn_lookup_st {
    const DGRAM_CONN_LOOKUP_METHODS *methods;
    void *impl_data;
};

/* Factory function for address-based lookup (DTLS) */
DGRAM_CONN_LOOKUP *ossl_dgram_conn_lookup_new_addr(void);

/*
 * Public API - calls through methods table.
 */
SSL *ossl_dgram_conn_lookup_find(DGRAM_CONN_LOOKUP *lookup,
    const DGRAM_URXE *e);
int ossl_dgram_conn_lookup_register(DGRAM_CONN_LOOKUP *lookup,
    const DGRAM_URXE *e, SSL *ssl);
int ossl_dgram_conn_lookup_register_addr(DGRAM_CONN_LOOKUP *lookup,
    const BIO_ADDR *peer,
    SSL *ssl);
int ossl_dgram_conn_lookup_unregister(DGRAM_CONN_LOOKUP *lookup,
    const BIO_ADDR *peer);
void ossl_dgram_conn_lookup_foreach(DGRAM_CONN_LOOKUP *lookup,
    ossl_dgram_conn_lookup_iter_fn cb,
    void *arg);
void ossl_dgram_conn_lookup_free(DGRAM_CONN_LOOKUP *lookup);
size_t ossl_dgram_conn_lookup_num_items(const DGRAM_CONN_LOOKUP *lookup);

#endif /* OPENSSL_NO_DTLS */
#endif /* OSSL_INTERNAL_DGRAM_CONN_LOOKUP_H */
