/*
 * ngtcp2
 *
 * Copyright (c) 2025 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGTCP2_DCIDTR_H
#define NGTCP2_DCIDTR_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <ngtcp2/ngtcp2.h>

#include "ngtcp2_cid.h"
#include "ngtcp2_ringbuf.h"

/* NGTCP2_DCIDTR_MAX_BOUND_DCID_SIZE is the maximum number of
   Destination Connection ID which has been bound to a particular
   path, but not yet used as primary path, and path validation is not
   performed from the local endpoint.  It must be the power of 2. */
#define NGTCP2_DCIDTR_MAX_BOUND_DCID_SIZE 4
/* NGTCP2_DCIDTR_MAX_UNUSED_DCID_SIZE is the maximum number of
   Destination Connection ID the remote endpoint provides to store.
   It must be the power of 2. */
#define NGTCP2_DCIDTR_MAX_UNUSED_DCID_SIZE 8
/* NGTCP2_DCIDTR_MAX_RETIRED_DCID_SIZE is the maximum number of
   retired Destination Connection ID kept to catch in-flight packet on
   a retired path.  It must be the power of 2. */
#define NGTCP2_DCIDTR_MAX_RETIRED_DCID_SIZE 2

ngtcp2_static_ringbuf_def(dcid_bound, NGTCP2_DCIDTR_MAX_BOUND_DCID_SIZE,
                          sizeof(ngtcp2_dcid))
ngtcp2_static_ringbuf_def(dcid_unused, NGTCP2_DCIDTR_MAX_UNUSED_DCID_SIZE,
                          sizeof(ngtcp2_dcid))
ngtcp2_static_ringbuf_def(dcid_retired, NGTCP2_DCIDTR_MAX_RETIRED_DCID_SIZE,
                          sizeof(ngtcp2_dcid))

/*
 * ngtcp2_dcidtr stores unused, bound, and retired Destination
 * Connection IDs.
 */
typedef struct ngtcp2_dcidtr {
  /* unused is a set of unused Destination Connection ID received from
     a remote endpoint.  They are considered inactive. */
  ngtcp2_static_ringbuf_dcid_unused unused;
  /* bound is a set of Destination Connection IDs which are bound to
     particular paths.  These paths are not validated yet.  They are
     considered inactive. */
  ngtcp2_static_ringbuf_dcid_bound bound;
  /* retired is a set of Destination Connection ID retired by local
     endpoint.  Keep them in 3*PTO to catch packets in flight along
     the old path.  They are considered active. */
  ngtcp2_static_ringbuf_dcid_retired retired;
  struct {
    /* seqs contains sequence number of Destination Connection ID
       whose retirement is not acknowledged by the remote endpoint
       yet. */
    uint64_t seqs[NGTCP2_DCIDTR_MAX_UNUSED_DCID_SIZE * 2];
    /* len is the number of sequence numbers that seq contains. */
    size_t len;
  } retire_unacked;
} ngtcp2_dcidtr;

typedef int (*ngtcp2_dcidtr_cb)(const ngtcp2_dcid *dcid, void *user_data);

/*
 * ngtcp2_dcidtr_init initializes |dtr|.
 */
void ngtcp2_dcidtr_init(ngtcp2_dcidtr *dtr);

/*
 * ngtcp2_dcidtr_track_retired_seq tracks the sequence number |seq| of
 * unacknowledged retiring Destination Connection ID.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_CONNECTION_ID_LIMIT
 *     The number of unacknowledged retirement exceeds the limit.
 */
int ngtcp2_dcidtr_track_retired_seq(ngtcp2_dcidtr *dtr, uint64_t seq);

/*
 * ngtcp2_dcidtr_untrack_retired_seq deletes the sequence number |seq|
 * of unacknowledged retiring Destination Connection ID.  It is fine
 * if such sequence number is not found.
 */
void ngtcp2_dcidtr_untrack_retired_seq(ngtcp2_dcidtr *dtr, uint64_t seq);

/*
 * ngtcp2_dcidtr_check_retired_seq_tracked returns nonzero if |seq|
 * has already been tracked.
 */
int ngtcp2_dcidtr_check_retired_seq_tracked(const ngtcp2_dcidtr *dtr,
                                            uint64_t seq);

/*
 * ngtcp2_dcidtr_find_bound_dcid returns the pointer to ngtcp2_dcid
 * that bound to |path|.  It returns NULL if there is no such
 * ngtcp2_dcid.
 */
ngtcp2_dcid *ngtcp2_dcidtr_find_bound_dcid(const ngtcp2_dcidtr *dtr,
                                           const ngtcp2_path *path);

/*
 * ngtcp2_dcidtr_bind_zerolen_dcid binds zero-length Destination
 * Connection ID to |path|, and returns the pointer to the bound
 * ngtcp2_dcid.
 */
ngtcp2_dcid *ngtcp2_dcidtr_bind_zerolen_dcid(ngtcp2_dcidtr *dtr,
                                             const ngtcp2_path *path);

/*
 * ngtcp2_dcidtr_bind_dcid binds non-zero Destination Connection ID to
 * |path|.  |ts| is the current timestamp.  The buffer space of bound
 * Destination Connection ID is limited.  If it is full, the earliest
 * one is removed.  |on_retire|, if specified, is called for the
 * removed ngtcp2_dcid with |user_data|.  This function assigns the
 * pointer to bound ngtcp2_dcid to |*pdest|.
 *
 * This function returns 0 if it succeeds, or negative error code that
 * |on_retire| returns.
 */
int ngtcp2_dcidtr_bind_dcid(ngtcp2_dcidtr *dtr, ngtcp2_dcid **pdest,
                            const ngtcp2_path *path, ngtcp2_tstamp ts,
                            ngtcp2_dcidtr_cb on_retire, void *user_data);

/*
 * ngtcp2_dcidtr_verify_stateless_reset verifies the stateless reset
 * token |token| received from |path|.  It returns 0 if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     There is no Destination Connection ID that matches the given
 *     |path| and |token|.
 */
int ngtcp2_dcidtr_verify_stateless_reset(const ngtcp2_dcidtr *dtr,
                                         const ngtcp2_path *path,
                                         const uint8_t *token);

/*
 * ngtcp2_dcidtr_verify_token_uniqueness verifies that the uniqueness
 * of the combination of |seq|, |cid|, and |token| against the exiting
 * Destination Connection IDs.  That is:
 *
 * - If they do not share the same seq, then their Connection IDs must
 *   be different.
 *
 * - If they share the same seq, then their Connection IDs and tokens
 *   must be the same.
 *
 * If this function succeeds, and there is Destination Connection ID
 * which shares |seq|, |cid|, and |token|, |*pfound| is set to
 * nonzero.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_PROTO
 *     The given combination of values does not satisfy the above
 *     conditions.
 */
int ngtcp2_dcidtr_verify_token_uniqueness(const ngtcp2_dcidtr *dtr, int *pfound,
                                          uint64_t seq, const ngtcp2_cid *cid,
                                          const uint8_t *token);

/*
 * ngtcp2_dcidtr_retire_inactive_dcid_prior_to retires inactive
 * Destination Connection IDs (unused or bound) whose seq is less than
 * |seq|.  For each retired ngtcp2_dcid, |on_retire|, if specified, is
 * called with |user_data|.
 *
 * This function returns 0 if it succeeds, or negative error code that
 * |on_retire| returns.
 */
int ngtcp2_dcidtr_retire_inactive_dcid_prior_to(ngtcp2_dcidtr *dtr,
                                                uint64_t seq,
                                                ngtcp2_dcidtr_cb on_retire,
                                                void *user_data);

/*
 * ngtcp2_dcidtr_retire_active_dcid adds an active |dcid| to the
 * retired Destination Connection ID buffer.  The buffer space of
 * retired Destination Connection ID is limited.  If it is full, the
 * earliest one is removed.  |on_deactivate| is called for the removed
 * ngtcp2_dcid with |user_data|.
 *
 * This function returns 0 if it succeeds, or negative error code that
 * |on_deactivate| returns.
 */
int ngtcp2_dcidtr_retire_active_dcid(ngtcp2_dcidtr *dtr,
                                     const ngtcp2_dcid *dcid, ngtcp2_tstamp ts,
                                     ngtcp2_dcidtr_cb on_deactivate,
                                     void *user_data);

/*
 * ngtcp2_dcidtr_retire_stale_bound_dcid retires stale bound
 * Destination Connection ID.  For each retired ngtcp2_dcid,
 * |on_retire|, if specified, is called with |user_data|.
 */
int ngtcp2_dcidtr_retire_stale_bound_dcid(ngtcp2_dcidtr *dtr,
                                          ngtcp2_duration timeout,
                                          ngtcp2_tstamp ts,
                                          ngtcp2_dcidtr_cb on_retire,
                                          void *user_data);

/*
 * ngtcp2_dcidtr_remove_stale_retired_dcid removes stale retired
 * Destination Connection ID.  For each removed ngtcp2_dcid,
 * |on_deactivate| is called with |user_data|.
 *
 * This function returns 0 if it succeeds, or negative error code that
 * |on_deactivate| returns.
 */
int ngtcp2_dcidtr_remove_stale_retired_dcid(ngtcp2_dcidtr *dtr,
                                            ngtcp2_duration timeout,
                                            ngtcp2_tstamp ts,
                                            ngtcp2_dcidtr_cb on_deactivate,
                                            void *user_data);

/*
 * ngtcp2_dcidtr_pop_bound_dcid removes Destination Connection ID that
 * is bound to |path|, and copies it into the object pointed by
 * |dest|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     No ngtcp2_dcid bound to |path| found.
 */
int ngtcp2_dcidtr_pop_bound_dcid(ngtcp2_dcidtr *dtr, ngtcp2_dcid *dest,
                                 const ngtcp2_path *path);

/*
 * ngtcp2_dcidtr_earliest_bound_ts returns earliest timestamp when a
 * Destination Connection ID is bound.  If there is no bound
 * Destination Connection ID, this function returns UINT64_MAX.
 */
ngtcp2_tstamp ngtcp2_dcidtr_earliest_bound_ts(const ngtcp2_dcidtr *dtr);

/*
 * ngtcp2_dcidtr_earliest_retired_ts returns earliest timestamp when a
 * Destination Connection ID is retired.  If there is no retired
 * Destination Connection ID, this function returns UINT64_MAX.
 */
ngtcp2_tstamp ngtcp2_dcidtr_earliest_retired_ts(const ngtcp2_dcidtr *dtr);

/*
 * ngtcp2_dcidtr_push_unused adds new Destination Connection ID to the
 * unused buffer.  |seq| is its sequence number, |cid| is its
 * Connection ID, and |token| is its stateless reset token.  If the
 * buffer space is full, the earliest ngtcp2_dcid is removed.
 */
void ngtcp2_dcidtr_push_unused(ngtcp2_dcidtr *dtr, uint64_t seq,
                               const ngtcp2_cid *cid, const uint8_t *token);

/*
 * ngtcp2_dcidtr_pop_unused_cid_token removes an unused Destination
 * Connection ID, and copies it into the object pointed by |dcid| with
 * ngtcp2_dcid_copy_cid_token.  This function assumes that there is at
 * least one unused Destination Connection ID.
 */
void ngtcp2_dcidtr_pop_unused_cid_token(ngtcp2_dcidtr *dtr, ngtcp2_dcid *dcid);

/*
 * ngtcp2_dcidtr_pop_unused removes an unused Destination Connection
 * ID, and copies it into the object pointed by |dcid| with
 * ngtcp2_dcid_copy.  This function assumes that there is at least one
 * unused Destination Connection ID.
 */
void ngtcp2_dcidtr_pop_unused(ngtcp2_dcidtr *dtr, ngtcp2_dcid *dcid);

/*
 * ngtcp2_dcidtr_check_path_retired returns nonzero if |path| is
 * included in retired Destination Connection IDs.
 */
int ngtcp2_dcidtr_check_path_retired(const ngtcp2_dcidtr *dtr,
                                     const ngtcp2_path *path);

/*
 * ngtcp2_dcidtr_unused_len returns the number of unused Destination
 * Connection ID.
 */
size_t ngtcp2_dcidtr_unused_len(const ngtcp2_dcidtr *dtr);

/*
 * ngtcp2_dcidtr_bound_len returns the number of bound Destination
 * Connection ID.
 */
size_t ngtcp2_dcidtr_bound_len(const ngtcp2_dcidtr *dtr);

/*
 * ngtcp2_dcidtr_retired_len returns the number of retired Destination
 * Connection ID.
 */
size_t ngtcp2_dcidtr_retired_len(const ngtcp2_dcidtr *dtr);

/*
 * ngtcp2_dcidtr_inactive_len returns the number of unused and bound
 * Destination Connection ID.
 */
size_t ngtcp2_dcidtr_inactive_len(const ngtcp2_dcidtr *dtr);

/*
 * ngtcp2_dcidtr_unused_full returns nonzero if the buffer of unused
 * Destination Connection ID is full.
 */
int ngtcp2_dcidtr_unused_full(const ngtcp2_dcidtr *dtr);

/*
 * ngtcp2_dcidtr_unused_empty returns nonzero if the buffer of unused
 * Destination Connection ID is empty.
 */
int ngtcp2_dcidtr_unused_empty(const ngtcp2_dcidtr *dtr);

/*
 * ngtcp2_dcidtr_bound_full returns nonzero if the buffer of bound
 * Destination Connection ID is full.
 */
int ngtcp2_dcidtr_bound_full(const ngtcp2_dcidtr *dtr);

#endif /* NGTCP2_DCIDTR_H */
