/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include "internal/common.h"
#include "internal/dgram_conn_lookup.h"
#include "crypto/siphash.h"

#ifndef OPENSSL_NO_DTLS

/*
 * Internal entry structure for address-based connection lookup.
 * Stores a connection keyed by peer address.
 */
typedef struct dgram_conn_entry_st {
    BIO_ADDR peer; /* Peer address (key) */
    SSL *ssl; /* The SSL connection object */
    uint8_t *hashkey; /* Pre-built hash key (family + port + addr) */
    size_t hashkey_len; /* Length of the hash key */
    uint64_t *siphash_key; /* Pointer to shared SipHash key (hash-flooding defense) */
} DGRAM_CONN_ENTRY;

DEFINE_LHASH_OF_EX(DGRAM_CONN_ENTRY);

/*
 * Wrapper structure for address-based lookup implementation data.
 * Contains the hash table and SipHash key for hash-flooding defense.
 */
typedef struct addr_lookup_data_st {
    LHASH_OF(DGRAM_CONN_ENTRY) *htable;
    uint64_t hash_key[2]; /* 128-bit SipHash key */
} ADDR_LOOKUP_DATA;

/*
 * Build a hash key from a BIO_ADDR.
 * The key format is: family (int) + port (uint16_t) + raw address bytes.
 *
 * Returns allocated hashkey buffer on success, NULL on failure.
 */
static uint8_t *build_hashkey(const BIO_ADDR *peer, size_t *out_len)
{
    size_t hashkey_len = 0;
    size_t addr_len = 0;
    int family;
    uint16_t port;
    uint8_t *hashkey;
    int *famptr;
    uint16_t *portptr;
    uint8_t *addrptr;

    family = BIO_ADDR_family(peer);

    /* For AF_UNSPEC (no peer address), use a minimal key */
    if (family == AF_UNSPEC) {
        hashkey_len = sizeof(int);
        hashkey = OPENSSL_zalloc(hashkey_len);
        if (hashkey == NULL)
            return NULL;
        famptr = (int *)hashkey;
        *famptr = family;
        *out_len = hashkey_len;
        return hashkey;
    }

    if (!BIO_ADDR_rawaddress(peer, NULL, &addr_len))
        return NULL;

    port = BIO_ADDR_rawport(peer);

    hashkey_len += sizeof(int); /* family */
    hashkey_len += sizeof(uint16_t); /* port */
    hashkey_len += addr_len; /* address */

    hashkey = OPENSSL_zalloc(hashkey_len);
    if (hashkey == NULL)
        return NULL;

    famptr = (int *)hashkey;
    portptr = (uint16_t *)(famptr + 1);
    addrptr = (uint8_t *)(portptr + 1);

    *famptr = family;
    *portptr = port;
    if (!BIO_ADDR_rawaddress(peer, addrptr, NULL)) {
        OPENSSL_free(hashkey);
        return NULL;
    }

    *out_len = hashkey_len;
    return hashkey;
}

/*
 * Hash function for DGRAM_CONN_ENTRY.
 * Uses SipHash with a per-instance random key to defend against
 * hash-flooding CPU DoS attacks
 */
static unsigned long conn_entry_hash(const DGRAM_CONN_ENTRY *e)
{
    SIPHASH siphash = { 0 };
    unsigned long hashval = 0;

    if (e->hashkey == NULL || e->hashkey_len == 0 || e->siphash_key == NULL)
        return 0;

    if (!SipHash_set_hash_size(&siphash, sizeof(unsigned long)))
        return 0;
    if (!SipHash_Init(&siphash, (const unsigned char *)e->siphash_key, 0, 0))
        return 0;
    SipHash_Update(&siphash, e->hashkey, e->hashkey_len);
    if (!SipHash_Final(&siphash, (unsigned char *)&hashval, sizeof(unsigned long)))
        return 0;

    return hashval;
}

/*
 * Compare function for DGRAM_CONN_ENTRY - compares pre-built hashkeys.
 * Returns 0 if equal, non-zero otherwise.
 */
static int conn_entry_cmp(const DGRAM_CONN_ENTRY *a, const DGRAM_CONN_ENTRY *b)
{
    if (a->hashkey_len != b->hashkey_len)
        return 1;
    if (a->hashkey == NULL || b->hashkey == NULL)
        return (a->hashkey == b->hashkey) ? 0 : 1;
    return memcmp(a->hashkey, b->hashkey, a->hashkey_len);
}

/*
 * Free a connection entry (but not the connection itself).
 */
static void conn_entry_free(DGRAM_CONN_ENTRY *e)
{
    if (e == NULL)
        return;
    OPENSSL_free(e->hashkey);
    OPENSSL_free(e);
}

/*
 * Callback for lh_DGRAM_CONN_ENTRY_doall to free all entries.
 */
static void conn_entry_free_cb(DGRAM_CONN_ENTRY *e)
{
    conn_entry_free(e);
}

/*
 * Lookup a connection by peer address from URXE.
 */
static SSL *addr_lookup(DGRAM_CONN_LOOKUP *lookup, const DGRAM_URXE *e)
{
    ADDR_LOOKUP_DATA *data;
    DGRAM_CONN_ENTRY key;
    DGRAM_CONN_ENTRY *result;

    if (lookup == NULL || lookup->impl_data == NULL || e == NULL)
        return NULL;

    data = (ADDR_LOOKUP_DATA *)lookup->impl_data;

    memset(&key, 0, sizeof(key));
    BIO_ADDR_copy(&key.peer, &e->peer);

    /* Build hashkey for the lookup key */
    key.hashkey = build_hashkey(&e->peer, &key.hashkey_len);
    if (key.hashkey == NULL)
        return NULL;

    /* Set SipHash key pointer for hash function */
    key.siphash_key = data->hash_key;

    result = lh_DGRAM_CONN_ENTRY_retrieve(data->htable, &key);

    OPENSSL_free(key.hashkey);

    if (result == NULL)
        return NULL;

    return result->ssl;
}

/*
 * Register a connection with peer address from URXE.
 */
static int addr_register_conn(DGRAM_CONN_LOOKUP *lookup, const DGRAM_URXE *e,
    SSL *ssl)
{
    ADDR_LOOKUP_DATA *data;
    DGRAM_CONN_ENTRY *entry, *old;

    if (lookup == NULL || lookup->impl_data == NULL || e == NULL || ssl == NULL)
        return 0;

    data = (ADDR_LOOKUP_DATA *)lookup->impl_data;

    entry = OPENSSL_zalloc(sizeof(*entry));
    if (entry == NULL)
        return 0;

    BIO_ADDR_copy(&entry->peer, &e->peer);
    entry->ssl = ssl;
    entry->hashkey = build_hashkey(&e->peer, &entry->hashkey_len);
    if (entry->hashkey == NULL) {
        OPENSSL_free(entry);
        return 0;
    }

    /* Set SipHash key pointer for hash function */
    entry->siphash_key = data->hash_key;

    old = lh_DGRAM_CONN_ENTRY_insert(data->htable, entry);

    /* Check if insert failed due to allocation error */
    if (lh_DGRAM_CONN_ENTRY_error(data->htable)) {
        conn_entry_free(entry);
        /* Don't free old since it is still in the hash table since insert failed */
        return 0;
    }

    /*
     * A non-NULL return from lh_DGRAM_CONN_ENTRY_insert means an entry with the
     * same key (peer address) was already present and has just been replaced.
     * This must not happen: callers always look up an address and only register
     * when no entry exists, all while holding the listener mutex, so the same peer
     * is never registered twice. We therefore expect old == NULL here.
     *
     * We deliberately do NOT free old->ssl: this lookup table is an
     * ownership-agnostic index and does not own the SSL objects it stores.
     * The owning layer (the DTLS listener) is responsible for the lifecycle of
     * those SSL objects. Freeing one here would be a use-after-free for the
     * non-owning established_conns table.
     */
    if (!ossl_assert(old == NULL))
        conn_entry_free(old);

    return 1;
}

/*
 * Register a connection by peer address from BIO_ADDR directly.
 * This is used when we don't have a URXE available.
 */
static int addr_register_conn_addr(DGRAM_CONN_LOOKUP *lookup, const BIO_ADDR *peer,
    SSL *ssl)
{
    ADDR_LOOKUP_DATA *data;
    DGRAM_CONN_ENTRY *entry, *old;

    if (lookup == NULL || lookup->impl_data == NULL || peer == NULL || ssl == NULL)
        return 0;

    data = (ADDR_LOOKUP_DATA *)lookup->impl_data;

    entry = OPENSSL_zalloc(sizeof(*entry));
    if (entry == NULL)
        return 0;

    BIO_ADDR_copy(&entry->peer, peer);
    entry->ssl = ssl;
    entry->hashkey = build_hashkey(peer, &entry->hashkey_len);
    if (entry->hashkey == NULL) {
        OPENSSL_free(entry);
        return 0;
    }

    /* Set SipHash key pointer for hash function */
    entry->siphash_key = data->hash_key;

    old = lh_DGRAM_CONN_ENTRY_insert(data->htable, entry);

    /* Check if insert failed due to allocation error */
    if (lh_DGRAM_CONN_ENTRY_error(data->htable)) {
        conn_entry_free(entry);
        /* Don't free old since it is still in the hash table since insert failed */
        return 0;
    }

    /*
     * A non-NULL return from lh_DGRAM_CONN_ENTRY_insert means an entry with the
     * same key(peer address) was already present and has just been replaced. This
     * must not happen: callers always look up an address and only register when no
     * entry exists, all while holding the listener mutex, so the same peer is
     * never registered twice. We therefore expect old == NULL here.
     *
     * We deliberately do NOT free old->ssl: this lookup table is an
     * ownership-agnostic index and does not own the SSL objects it stores.
     * The owning layer (the DTLS listener) is responsible for the lifecycle of
     * those SSL objects. Freeing one here would be a use-after-free for the
     * non-owning established_conns table.
     */
    if (!ossl_assert(old == NULL))
        conn_entry_free(old);

    return 1;
}

/*
 * Unregister a connection by peer address.
 */
static int addr_unregister_conn(DGRAM_CONN_LOOKUP *lookup, const BIO_ADDR *peer)
{
    ADDR_LOOKUP_DATA *data;
    DGRAM_CONN_ENTRY lookup_key;
    DGRAM_CONN_ENTRY *removed;

    if (lookup == NULL || lookup->impl_data == NULL || peer == NULL)
        return 0;

    data = (ADDR_LOOKUP_DATA *)lookup->impl_data;

    memset(&lookup_key, 0, sizeof(lookup_key));
    BIO_ADDR_copy(&lookup_key.peer, peer);

    lookup_key.hashkey = build_hashkey(peer, &lookup_key.hashkey_len);
    if (lookup_key.hashkey == NULL)
        return 0;

    /* Set SipHash key pointer for hash function */
    lookup_key.siphash_key = data->hash_key;

    removed = lh_DGRAM_CONN_ENTRY_delete(data->htable, &lookup_key);

    OPENSSL_free(lookup_key.hashkey);

    if (removed != NULL) {
        conn_entry_free(removed);
        return 1;
    }

    return 0;
}

/*
 * Free the lookup structure and all entries.
 */
static void addr_free(DGRAM_CONN_LOOKUP *lookup)
{
    ADDR_LOOKUP_DATA *data;

    if (lookup == NULL)
        return;

    if (lookup->impl_data != NULL) {
        data = (ADDR_LOOKUP_DATA *)lookup->impl_data;
        if (data->htable != NULL) {
            lh_DGRAM_CONN_ENTRY_doall(data->htable, conn_entry_free_cb);
            lh_DGRAM_CONN_ENTRY_free(data->htable);
        }
        OPENSSL_free(data);
    }

    OPENSSL_free(lookup);
}

/*
 * Context structure for iteration callback.
 */
typedef struct {
    ossl_dgram_conn_lookup_iter_fn user_cb;
    void *user_arg;
} ADDR_FOREACH_CTX;

/*
 * Internal callback for lh_doall_arg that invokes the user's callback.
 */
static void addr_foreach_cb(DGRAM_CONN_ENTRY *e, void *arg)
{
    ADDR_FOREACH_CTX *ctx = arg;

    if (ctx->user_cb != NULL)
        ctx->user_cb(e->ssl, &e->peer, ctx->user_arg);
}

IMPLEMENT_LHASH_DOALL_ARG(DGRAM_CONN_ENTRY, void);

/*
 * Iterate over all connections, calling the callback for each.
 */
static void addr_foreach(DGRAM_CONN_LOOKUP *lookup,
    ossl_dgram_conn_lookup_iter_fn cb, void *arg)
{
    ADDR_LOOKUP_DATA *data;
    ADDR_FOREACH_CTX ctx;

    if (lookup == NULL || lookup->impl_data == NULL || cb == NULL)
        return;

    data = (ADDR_LOOKUP_DATA *)lookup->impl_data;
    ctx.user_cb = cb;
    ctx.user_arg = arg;

    lh_DGRAM_CONN_ENTRY_doall_void(data->htable, addr_foreach_cb, &ctx);
}

static size_t addr_num_items(const DGRAM_CONN_LOOKUP *lookup)
{
    ADDR_LOOKUP_DATA *data;

    if (lookup == NULL || lookup->impl_data == NULL)
        return 0;

    data = (ADDR_LOOKUP_DATA *)lookup->impl_data;
    return lh_DGRAM_CONN_ENTRY_num_items(data->htable);
}

static const DGRAM_CONN_LOOKUP_METHODS addr_methods = {
    addr_lookup,
    addr_register_conn,
    addr_register_conn_addr,
    addr_unregister_conn,
    addr_foreach,
    addr_free,
    addr_num_items
};

/*
 * Create a new address-based connection lookup for DTLS.
 */
DGRAM_CONN_LOOKUP *ossl_dgram_conn_lookup_new_addr(void)
{
    DGRAM_CONN_LOOKUP *lookup;
    ADDR_LOOKUP_DATA *data;

    lookup = OPENSSL_zalloc(sizeof(*lookup));
    if (lookup == NULL)
        return NULL;

    data = OPENSSL_zalloc(sizeof(*data));
    if (data == NULL) {
        OPENSSL_free(lookup);
        return NULL;
    }

    /* Generate random SipHash key for hash-flooding defense */
    if (RAND_priv_bytes((unsigned char *)data->hash_key,
            sizeof(data->hash_key))
        <= 0) {
        OPENSSL_free(data);
        OPENSSL_free(lookup);
        return NULL;
    }

    data->htable = lh_DGRAM_CONN_ENTRY_new(conn_entry_hash, conn_entry_cmp);
    if (data->htable == NULL) {
        OPENSSL_free(data);
        OPENSSL_free(lookup);
        return NULL;
    }

    lookup->methods = &addr_methods;
    lookup->impl_data = data;

    return lookup;
}

/*
 * Public API wrappers - call through methods table.
 */
SSL *ossl_dgram_conn_lookup_find(DGRAM_CONN_LOOKUP *lookup, const DGRAM_URXE *e)
{
    if (lookup == NULL || lookup->methods == NULL
        || lookup->methods->lookup == NULL)
        return NULL;
    return lookup->methods->lookup(lookup, e);
}

int ossl_dgram_conn_lookup_register(DGRAM_CONN_LOOKUP *lookup,
    const DGRAM_URXE *e, SSL *ssl)
{
    if (lookup == NULL || lookup->methods == NULL
        || lookup->methods->register_conn == NULL)
        return 0;
    return lookup->methods->register_conn(lookup, e, ssl);
}

int ossl_dgram_conn_lookup_register_addr(DGRAM_CONN_LOOKUP *lookup,
    const BIO_ADDR *peer, SSL *ssl)
{
    if (lookup == NULL || lookup->methods == NULL
        || lookup->methods->register_conn_addr == NULL)
        return 0;
    return lookup->methods->register_conn_addr(lookup, peer, ssl);
}

int ossl_dgram_conn_lookup_unregister(DGRAM_CONN_LOOKUP *lookup,
    const BIO_ADDR *peer)
{
    if (lookup == NULL || lookup->methods == NULL
        || lookup->methods->unregister_conn == NULL)
        return 0;
    return lookup->methods->unregister_conn(lookup, peer);
}

void ossl_dgram_conn_lookup_foreach(DGRAM_CONN_LOOKUP *lookup,
    ossl_dgram_conn_lookup_iter_fn cb, void *arg)
{
    if (lookup == NULL || lookup->methods == NULL
        || lookup->methods->foreach == NULL)
        return;
    lookup->methods->foreach(lookup, cb, arg);
}

void ossl_dgram_conn_lookup_free(DGRAM_CONN_LOOKUP *lookup)
{
    if (lookup == NULL || lookup->methods == NULL
        || lookup->methods->free == NULL)
        return;
    lookup->methods->free(lookup);
}

/*
 * Return the number of entries currently registered.
 *
 * The lookup is not internally synchronised: like the other lookup
 * operations, the caller must hold the lock that serialises access to it.
 * Reading the count without that lock held races with concurrent
 * register/unregister.
 */
size_t ossl_dgram_conn_lookup_num_items(const DGRAM_CONN_LOOKUP *lookup)
{
    if (lookup == NULL || lookup->methods == NULL
        || lookup->methods->num_items == NULL)
        return 0;
    return lookup->methods->num_items(lookup);
}

#endif /* OPENSSL_NO_DTLS */
