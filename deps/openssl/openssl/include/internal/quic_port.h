/*
 * Copyright 2023-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#ifndef OSSL_QUIC_PORT_H
#define OSSL_QUIC_PORT_H

#include <openssl/ssl.h>
#include "internal/quic_types.h"
#include "internal/quic_reactor.h"
#include "internal/quic_demux.h"
#include "internal/quic_predef.h"
#include "internal/thread_arch.h"

#ifndef OPENSSL_NO_QUIC

/*
 * QUIC Port
 * =========
 *
 * A QUIC Port (QUIC_PORT) represents a single UDP network socket and contains
 * zero or more subsidiary QUIC_CHANNEL instances, each of which represents a
 * single QUIC connection. All QUIC_CHANNEL instances must belong to a
 * QUIC_PORT.
 *
 * A QUIC port is responsible for managing a set of channels which all use the
 * same UDP socket, and (in future) for automatically creating new channels when
 * incoming connections are received.
 *
 * In order to retain compatibility with QUIC_TSERVER, it also supports a point
 * of legacy compatibility where a caller can create an incoming (server role)
 * channel and that channel will be automatically be bound to the next incoming
 * connection. In the future this will go away once QUIC_TSERVER is removed.
 *
 * All QUIC_PORT instances are created by a QUIC_ENGINE.
 */
typedef struct quic_port_args_st {
    /* The engine which the QUIC port is to be a child of. */
    QUIC_ENGINE *engine;

    /*
     * This callback allows port_new_handshake_layer to pre-create a quic
     * connection object for the incoming channel
     * user_ssl_arg is expected to point to a quic listener object
     */
    SSL *(*get_conn_user_ssl)(QUIC_CHANNEL *ch, QUIC_LISTENER *ql);
    QUIC_LISTENER *ql;

    /*
     * This SSL_CTX will be used when constructing the handshake layer object
     * inside newly created channels.
     */
    SSL_CTX *channel_ctx;

    /*
     * If 1, this port is to be used for multiple connections, so
     * non-zero-length CIDs should be used. If 0, this port will only be used
     * for a single connection, so a zero-length local CID can be used.
     */
    int is_multi_conn;

    /*
     * if 1, this port should do server address validation
     */
    int do_addr_validation;
} QUIC_PORT_ARGS;

/* Only QUIC_ENGINE should use this function. */
QUIC_PORT *ossl_quic_port_new(const QUIC_PORT_ARGS *args);

void ossl_quic_port_free(QUIC_PORT *port);

/*
 * Operations
 * ==========
 */

/* Create an outgoing channel using this port. */
QUIC_CHANNEL *ossl_quic_port_create_outgoing(QUIC_PORT *port, SSL *tls);

/*
 * Create an incoming channel using this port.
 *
 * TODO(QUIC FUTURE): temporary TSERVER use only - will be removed.
 */
QUIC_CHANNEL *ossl_quic_port_create_incoming(QUIC_PORT *port, SSL *tls);

/*
 * Pop an incoming channel from the incoming channel queue. Returns NULL if
 * there are no pending incoming channels.
 */
QUIC_CHANNEL *ossl_quic_port_pop_incoming(QUIC_PORT *port);

/* Returns the first incoming channel without removing it, or NULL. */
QUIC_CHANNEL *ossl_quic_port_peek_incoming(QUIC_PORT *port);

/* Returns 1 if there is at least one connection incoming. */
int ossl_quic_port_have_incoming(QUIC_PORT *port);

/*
 * Delete any channels which are pending acceptance.
 */
void ossl_quic_port_drop_incoming(QUIC_PORT *port);

/*
 * Queries and Accessors
 * =====================
 */

/* Gets/sets the underlying network read and write BIO. */
BIO *ossl_quic_port_get_net_rbio(QUIC_PORT *port);
BIO *ossl_quic_port_get_net_wbio(QUIC_PORT *port);
int ossl_quic_port_set_net_rbio(QUIC_PORT *port, BIO *net_rbio);
int ossl_quic_port_set_net_wbio(QUIC_PORT *port, BIO *net_wbio);
SSL_CTX *ossl_quic_port_get_channel_ctx(QUIC_PORT *port);

/*
 * Re-poll the network BIOs already set to determine if their support for
 * polling has changed. If force is 0, only check again if the BIOs have been
 * changed.
 */
int ossl_quic_port_update_poll_descriptors(QUIC_PORT *port, int force);

/* Gets the engine which this port is a child of. */
QUIC_ENGINE *ossl_quic_port_get0_engine(QUIC_PORT *port);

/* Gets the reactor which can be used to tick/poll on the port. */
QUIC_REACTOR *ossl_quic_port_get0_reactor(QUIC_PORT *port);

/* Gets the demuxer belonging to the port. */
QUIC_DEMUX *ossl_quic_port_get0_demux(QUIC_PORT *port);

/* Gets the mutex used by the port. */
CRYPTO_MUTEX *ossl_quic_port_get0_mutex(QUIC_PORT *port);

/* Gets the current time. */
OSSL_TIME ossl_quic_port_get_time(QUIC_PORT *port);

int ossl_quic_port_get_rx_short_dcid_len(const QUIC_PORT *port);
int ossl_quic_port_get_tx_init_dcid_len(const QUIC_PORT *port);

/* Configures the idle timeout to request from peer (milliseconds, 0=no timeout). */
void ossl_quic_port_set_max_idle_timeout(QUIC_PORT *port, uint64_t ms);
/* Gets the configured idle timeout to request from peer. */
uint64_t ossl_quic_port_get_max_idle_timeout(const QUIC_PORT *port);

/* Configures the maximum UDP payload size to advertise to the peer (bytes). */
void ossl_quic_port_set_max_udp_payload_size(QUIC_PORT *port, uint64_t size);
/* Gets the configured maximum UDP payload size to advertise to the peer. */
uint64_t ossl_quic_port_get_max_udp_payload_size(const QUIC_PORT *port);

/* Configures the maximum data to advertise to the peer (bytes). */
void ossl_quic_port_set_init_max_data(QUIC_PORT *port, uint64_t max_data);
/* Gets the configured maximum data to advertise to the peer. */
uint64_t ossl_quic_port_get_init_max_data(const QUIC_PORT *port);

/* Configures the maximum stream data for a bidi/uni remote/local stream to advertise to the peer (bytes). */
void ossl_quic_port_set_init_max_stream_data(QUIC_PORT *port, uint64_t max_data, int is_uni, int is_remote);
/* Gets the configured maximum stream data for a bidi/uni remote/local stream to advertise to the peer. */
uint64_t ossl_quic_port_get_init_max_stream_data(const QUIC_PORT *port, int is_uni, int is_remote);

/* Configures the maximum bidi/uni streams to advertise to the peer. */
void ossl_quic_port_set_init_max_streams(QUIC_PORT *port, uint64_t max_streams, int is_uni);
/* Gets the configured maximum bidi/uni streams to advertise to the peer. */
uint64_t ossl_quic_port_get_init_max_streams(const QUIC_PORT *port, int is_uni);

/* Configures the ACK delay exponent to advertise to the peer. */
void ossl_quic_port_set_ack_delay_exponent(QUIC_PORT *port, uint64_t exp);
/* Gets the configured ACK delay exponent to advertise to the peer. */
uint64_t ossl_quic_port_get_ack_delay_exponent(const QUIC_PORT *port);

/* Configures the maximum ACK delay to advertise to the peer (milliseconds). */
void ossl_quic_port_set_max_ack_delay(QUIC_PORT *port, uint64_t ms);
/* Gets the configured maximum ACK delay to advertise to the peer. */
uint64_t ossl_quic_port_get_max_ack_delay(const QUIC_PORT *port);

/* Configures the disable active migration flag to advertise to the peer. */
void ossl_quic_port_set_disable_active_migration(QUIC_PORT *port, uint64_t disable);
/* Gets the configured disable active migration flag to advertise to the peer. */
uint64_t ossl_quic_port_get_disable_active_migration(const QUIC_PORT *port);

/* Configures the active connection ID limit to advertise to the peer. */
void ossl_quic_port_set_active_conn_id_limit(QUIC_PORT *port, uint64_t limit);
/* Gets the configured active connection ID limit to advertise to the peer. */
uint64_t ossl_quic_port_get_active_conn_id_limit(const QUIC_PORT *port);

/* Returns 1 if the port is running/healthy, 0 if it has failed. */
int ossl_quic_port_is_running(const QUIC_PORT *port);

/*
 * Restores port-level error to the error stack. To be called only if
 * the port is no longer running.
 */
void ossl_quic_port_restore_err_state(const QUIC_PORT *port);

/* For use by QUIC_ENGINE. You should not need to call this directly. */
void ossl_quic_port_subtick(QUIC_PORT *port, QUIC_TICK_RESULT *r,
    uint32_t flags);

/* Returns the number of queued incoming channels. */
size_t ossl_quic_port_get_num_incoming_channels(const QUIC_PORT *port);

/* Sets if incoming connections should currently be allowed. */
void ossl_quic_port_set_allow_incoming(QUIC_PORT *port, int allow_incoming);

#define PEELOFF_LISTEN -1
#define PEELOFF_ACCEPT 1
#define PEELOFF_UNSET 0
/*
 * Sets flag to indicate we are using SSL_listen_ex to get connections
 * returns 1 if set was successful, or 0 if the set fails
 */
int ossl_quic_port_test_and_set_peeloff(QUIC_PORT *port, int using_peeloff);

/* Returns 1 if we are using addressed mode on the read side. */
int ossl_quic_port_is_addressed_r(const QUIC_PORT *port);

/* Returns 1 if we are using addressed mode on the write side. */
int ossl_quic_port_is_addressed_w(const QUIC_PORT *port);

/* Returns 1 if we are using addressed mode. */
int ossl_quic_port_is_addressed(const QUIC_PORT *port);

/*
 * Returns the current network BIO epoch. This increments whenever the network
 * BIO configuration changes.
 */
uint64_t ossl_quic_port_get_net_bio_epoch(const QUIC_PORT *port);

/*
 * Events
 * ======
 */

/*
 * Called if a permanent network error occurs. Terminates all channels
 * immediately. triggering_ch is an optional argument designating
 * a channel which encountered the network error.
 */
void ossl_quic_port_raise_net_error(QUIC_PORT *port,
    QUIC_CHANNEL *triggering_ch);

uint64_t ossl_quic_port_get_max_pending_channels(const QUIC_PORT *port);

void ossl_quic_port_set_max_pending_channels(QUIC_PORT *port, uint64_t max_pending_channels);

#endif

#endif
