/*
 *  Message Processing Stack, Reader implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)

#include "mps_reader.h"
#include "mps_common.h"
#include "mps_trace.h"

#include <string.h>

#if defined(MBEDTLS_MPS_ENABLE_TRACE)
static int mbedtls_mps_trace_id = MBEDTLS_MPS_TRACE_BIT_READER;
#endif /* MBEDTLS_MPS_ENABLE_TRACE */

/*
 * GENERAL NOTE ON CODING STYLE
 *
 * The following code intentionally separates memory loads
 * and stores from other operations (arithmetic or branches).
 * This leads to the introduction of many local variables
 * and significantly increases the C-code line count, but
 * should not increase the size of generated assembly.
 *
 * The reason for this is twofold:
 * (1) It will ease verification efforts using the VST
 *     (Verified Software Toolchain)
 *     whose program logic cannot directly reason
 *     about instructions containing a load or store in
 *     addition to other operations (e.g. *p = *q or
 *     tmp = *p + 42).
 * (2) Operating on local variables and writing the results
 *     back to the target contexts on success only
 *     allows to maintain structure invariants even
 *     on failure - this in turn has two benefits:
 *     (2.a) If for some reason an error code is not caught
 *           and operation continues, functions are nonetheless
 *           called with sane contexts, reducing the risk
 *           of dangerous behavior.
 *     (2.b) Randomized testing is easier if structures
 *           remain intact even in the face of failing
 *           and/or non-sensical calls.
 *     Moreover, it might even reduce code-size because
 *     the compiler need not write back temporary results
 *     to memory in case of failure.
 *
 */

static inline int mps_reader_is_accumulating(
    mbedtls_mps_reader const *rd)
{
    mbedtls_mps_size_t acc_remaining;
    if (rd->acc == NULL) {
        return 0;
    }

    acc_remaining = rd->acc_share.acc_remaining;
    return acc_remaining > 0;
}

static inline int mps_reader_is_producing(
    mbedtls_mps_reader const *rd)
{
    unsigned char *frag = rd->frag;
    return frag == NULL;
}

static inline int mps_reader_is_consuming(
    mbedtls_mps_reader const *rd)
{
    return !mps_reader_is_producing(rd);
}

static inline mbedtls_mps_size_t mps_reader_get_fragment_offset(
    mbedtls_mps_reader const *rd)
{
    unsigned char *acc = rd->acc;
    mbedtls_mps_size_t frag_offset;

    if (acc == NULL) {
        return 0;
    }

    frag_offset = rd->acc_share.frag_offset;
    return frag_offset;
}

static inline mbedtls_mps_size_t mps_reader_serving_from_accumulator(
    mbedtls_mps_reader const *rd)
{
    mbedtls_mps_size_t frag_offset, end;

    frag_offset = mps_reader_get_fragment_offset(rd);
    end = rd->end;

    return end < frag_offset;
}

static inline void mps_reader_zero(mbedtls_mps_reader *rd)
{
    /* A plain memset() would likely be more efficient,
     * but the current way of zeroing makes it harder
     * to overlook fields which should not be zero-initialized.
     * It's also more suitable for FV efforts since it
     * doesn't require reasoning about structs being
     * interpreted as unstructured binary blobs. */
    static mbedtls_mps_reader const zero =
    { .frag          = NULL,
      .frag_len      = 0,
      .commit        = 0,
      .end           = 0,
      .pending       = 0,
      .acc           = NULL,
      .acc_len       = 0,
      .acc_available = 0,
      .acc_share     = { .acc_remaining = 0 } };
    *rd = zero;
}

int mbedtls_mps_reader_init(mbedtls_mps_reader *rd,
                            unsigned char *acc,
                            mbedtls_mps_size_t acc_len)
{
    MBEDTLS_MPS_TRACE_INIT("mbedtls_mps_reader_init");
    MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                      "* Accumulator size: %u bytes", (unsigned) acc_len);
    mps_reader_zero(rd);
    rd->acc = acc;
    rd->acc_len = acc_len;
    MBEDTLS_MPS_TRACE_RETURN(0);
}

int mbedtls_mps_reader_free(mbedtls_mps_reader *rd)
{
    MBEDTLS_MPS_TRACE_INIT("mbedtls_mps_reader_free");
    mps_reader_zero(rd);
    MBEDTLS_MPS_TRACE_RETURN(0);
}

int mbedtls_mps_reader_feed(mbedtls_mps_reader *rd,
                            unsigned char *new_frag,
                            mbedtls_mps_size_t new_frag_len)
{
    mbedtls_mps_size_t copy_to_acc;
    MBEDTLS_MPS_TRACE_INIT("mbedtls_mps_reader_feed");
    MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                      "* Fragment length: %u bytes", (unsigned) new_frag_len);

    if (new_frag == NULL) {
        MBEDTLS_MPS_TRACE_RETURN(MBEDTLS_ERR_MPS_READER_INVALID_ARG);
    }

    MBEDTLS_MPS_STATE_VALIDATE_RAW(mps_reader_is_producing(
                                       rd),
                                   "mbedtls_mps_reader_feed() requires reader to be in producing mode");

    if (mps_reader_is_accumulating(rd)) {
        unsigned char *acc    = rd->acc;
        mbedtls_mps_size_t acc_remaining = rd->acc_share.acc_remaining;
        mbedtls_mps_size_t acc_available = rd->acc_available;

        /* Skip over parts of the accumulator that have already been filled. */
        acc += acc_available;

        copy_to_acc = acc_remaining;
        if (copy_to_acc > new_frag_len) {
            copy_to_acc = new_frag_len;
        }

        /* Copy new contents to accumulator. */
        memcpy(acc, new_frag, copy_to_acc);

        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                          "Copy new data of size %u of %u into accumulator at offset %u",
                          (unsigned) copy_to_acc, (unsigned) new_frag_len,
                          (unsigned) acc_available);

        /* Check if, with the new fragment, we have enough data. */
        acc_remaining -= copy_to_acc;
        if (acc_remaining > 0) {
            /* We need to accumulate more data. Stay in producing mode. */
            acc_available += copy_to_acc;
            rd->acc_share.acc_remaining = acc_remaining;
            rd->acc_available = acc_available;
            MBEDTLS_MPS_TRACE_RETURN(MBEDTLS_ERR_MPS_READER_NEED_MORE);
        }

        /* We have filled the accumulator: Move to consuming mode. */

        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                          "Enough data available to serve user request");

        /* Remember overlap of accumulator and fragment. */
        rd->acc_share.frag_offset = acc_available;
        acc_available += copy_to_acc;
        rd->acc_available = acc_available;
    } else { /* Not accumulating */
        rd->acc_share.frag_offset = 0;
    }

    rd->frag = new_frag;
    rd->frag_len = new_frag_len;
    rd->commit = 0;
    rd->end = 0;
    MBEDTLS_MPS_TRACE_RETURN(0);
}


int mbedtls_mps_reader_get(mbedtls_mps_reader *rd,
                           mbedtls_mps_size_t desired,
                           unsigned char **buffer,
                           mbedtls_mps_size_t *buflen)
{
    unsigned char *frag;
    mbedtls_mps_size_t frag_len, frag_offset, end, frag_fetched, frag_remaining;
    MBEDTLS_MPS_TRACE_INIT("mbedtls_mps_reader_get");
    MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                      "* Bytes requested: %u", (unsigned) desired);

    MBEDTLS_MPS_STATE_VALIDATE_RAW(mps_reader_is_consuming(
                                       rd),
                                   "mbedtls_mps_reader_get() requires reader to be in consuming mode");

    end = rd->end;
    frag_offset = mps_reader_get_fragment_offset(rd);

    /* Check if we're still serving from the accumulator. */
    if (mps_reader_serving_from_accumulator(rd)) {
        /* Illustration of supported and unsupported cases:
         *
         * - Allowed #1
         *
         *                          +-----------------------------------+
         *                          |               frag                |
         *                          +-----------------------------------+
         *
         *             end end+desired
         *              |       |
         *        +-----v-------v-------------+
         *        |          acc              |
         *        +---------------------------+
         *                          |         |
         *                     frag_offset  acc_available
         *
         * - Allowed #2
         *
         *                          +-----------------------------------+
         *                          |               frag                |
         *                          +-----------------------------------+
         *
         *                  end          end+desired
         *                   |                |
         *        +----------v----------------v
         *        |          acc              |
         *        +---------------------------+
         *                          |         |
         *                   frag_offset acc_available
         *
         * - Not allowed #1 (could be served, but we don't actually use it):
         *
         *                      +-----------------------------------+
         *                      |               frag                |
         *                      +-----------------------------------+
         *
         *              end        end+desired
         *               |             |
         *        +------v-------------v------+
         *        |          acc              |
         *        +---------------------------+
         *                      |             |
         *                frag_offset   acc_available
         *
         *
         * - Not allowed #2 (can't be served with a contiguous buffer):
         *
         *                      +-----------------------------------+
         *                      |               frag                |
         *                      +-----------------------------------+
         *
         *              end                 end + desired
         *               |                        |
         *        +------v--------------------+   v
         *        |            acc            |
         *        +---------------------------+
         *                      |             |
         *                frag_offset   acc_available
         *
         * In case of Allowed #2 we're switching to serve from
         * `frag` starting from the next call to mbedtls_mps_reader_get().
         */

        unsigned char *acc;

        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                          "Serve the request from the accumulator");
        if (frag_offset - end < desired) {
            mbedtls_mps_size_t acc_available;
            acc_available = rd->acc_available;
            if (acc_available - end != desired) {
                /* It might be possible to serve some of these situations by
                 * making additional space in the accumulator, removing those
                 * parts that have already been committed.
                 * On the other hand, this brings additional complexity and
                 * enlarges the code size, while there doesn't seem to be a use
                 * case where we don't attempt exactly the same `get` calls when
                 * resuming on a reader than what we tried before pausing it.
                 * If we believe we adhere to this restricted usage throughout
                 * the library, this check is a good opportunity to
                 * validate this. */
                MBEDTLS_MPS_TRACE_RETURN(
                    MBEDTLS_ERR_MPS_READER_INCONSISTENT_REQUESTS);
            }
        }

        acc = rd->acc;
        acc += end;

        *buffer = acc;
        if (buflen != NULL) {
            *buflen = desired;
        }

        end += desired;
        rd->end = end;
        rd->pending = 0;

        MBEDTLS_MPS_TRACE_RETURN(0);
    }

    /* Attempt to serve the request from the current fragment */
    MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                      "Serve the request from the current fragment.");

    frag_len = rd->frag_len;
    frag_fetched = end - frag_offset; /* The amount of data from the current
                                       * fragment that has already been passed
                                       * to the user. */
    frag_remaining = frag_len - frag_fetched; /* Remaining data in fragment */

    /* Check if we can serve the read request from the fragment. */
    if (frag_remaining < desired) {
        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                          "There's not enough data in the current fragment "
                          "to serve the request.");
        /* There's not enough data in the current fragment,
         * so either just RETURN what we have or fail. */
        if (buflen == NULL) {
            if (frag_remaining > 0) {
                rd->pending = desired - frag_remaining;
                MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                                  "Remember to collect %u bytes before re-opening",
                                  (unsigned) rd->pending);
            }
            MBEDTLS_MPS_TRACE_RETURN(MBEDTLS_ERR_MPS_READER_OUT_OF_DATA);
        }

        desired = frag_remaining;
    }

    /* There's enough data in the current fragment to serve the
     * (potentially modified) read request. */

    frag = rd->frag;
    frag += frag_fetched;

    *buffer = frag;
    if (buflen != NULL) {
        *buflen = desired;
    }

    end += desired;
    rd->end = end;
    rd->pending = 0;
    MBEDTLS_MPS_TRACE_RETURN(0);
}

int mbedtls_mps_reader_commit(mbedtls_mps_reader *rd)
{
    mbedtls_mps_size_t end;
    MBEDTLS_MPS_TRACE_INIT("mbedtls_mps_reader_commit");
    MBEDTLS_MPS_STATE_VALIDATE_RAW(mps_reader_is_consuming(
                                       rd),
                                   "mbedtls_mps_reader_commit() requires reader to be in consuming mode");

    end = rd->end;
    rd->commit = end;

    MBEDTLS_MPS_TRACE_RETURN(0);
}

int mbedtls_mps_reader_reclaim(mbedtls_mps_reader *rd,
                               int *paused)
{
    unsigned char *frag, *acc;
    mbedtls_mps_size_t pending, commit;
    mbedtls_mps_size_t acc_len, frag_offset, frag_len;
    MBEDTLS_MPS_TRACE_INIT("mbedtls_mps_reader_reclaim");

    if (paused != NULL) {
        *paused = 0;
    }

    MBEDTLS_MPS_STATE_VALIDATE_RAW(mps_reader_is_consuming(
                                       rd),
                                   "mbedtls_mps_reader_reclaim() requires reader to be in consuming mode");

    frag     = rd->frag;
    acc      = rd->acc;
    pending  = rd->pending;
    commit   = rd->commit;
    frag_len = rd->frag_len;

    frag_offset = mps_reader_get_fragment_offset(rd);

    if (pending == 0) {
        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                          "No unsatisfied read-request has been logged.");

        /* Check if there's data left to be consumed. */
        if (commit < frag_offset || commit - frag_offset < frag_len) {
            MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                              "There is data left to be consumed.");
            rd->end = commit;
            MBEDTLS_MPS_TRACE_RETURN(MBEDTLS_ERR_MPS_READER_DATA_LEFT);
        }

        rd->acc_available = 0;
        rd->acc_share.acc_remaining = 0;

        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                          "Fragment has been fully processed and committed.");
    } else {
        int overflow;

        mbedtls_mps_size_t acc_backup_offset;
        mbedtls_mps_size_t acc_backup_len;
        mbedtls_mps_size_t frag_backup_offset;
        mbedtls_mps_size_t frag_backup_len;

        mbedtls_mps_size_t backup_len;
        mbedtls_mps_size_t acc_len_needed;

        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                          "There has been an unsatisfied read with %u bytes overhead.",
                          (unsigned) pending);

        if (acc == NULL) {
            MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                              "No accumulator present");
            MBEDTLS_MPS_TRACE_RETURN(
                MBEDTLS_ERR_MPS_READER_NEED_ACCUMULATOR);
        }
        acc_len = rd->acc_len;

        /* Check if the upper layer has already fetched
         * and committed the contents of the accumulator. */
        if (commit < frag_offset) {
            /* No, accumulator is still being processed. */
            frag_backup_offset = 0;
            frag_backup_len = frag_len;
            acc_backup_offset = commit;
            acc_backup_len = frag_offset - commit;
        } else {
            /* Yes, the accumulator is already processed. */
            frag_backup_offset = commit - frag_offset;
            frag_backup_len = frag_len - frag_backup_offset;
            acc_backup_offset = 0;
            acc_backup_len = 0;
        }

        backup_len = acc_backup_len + frag_backup_len;
        acc_len_needed = backup_len + pending;

        overflow  = 0;
        overflow |= (backup_len     < acc_backup_len);
        overflow |= (acc_len_needed < backup_len);

        if (overflow || acc_len < acc_len_needed) {
            /* Except for the different return code, we behave as if
             * there hadn't been a call to mbedtls_mps_reader_get()
             * since the last commit. */
            rd->end = commit;
            rd->pending = 0;
            MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_ERROR,
                              "The accumulator is too small to handle the backup.");
            MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_ERROR,
                              "* Size: %u", (unsigned) acc_len);
            MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_ERROR,
                              "* Needed: %u (%u + %u)",
                              (unsigned) acc_len_needed,
                              (unsigned) backup_len, (unsigned) pending);
            MBEDTLS_MPS_TRACE_RETURN(
                MBEDTLS_ERR_MPS_READER_ACCUMULATOR_TOO_SMALL);
        }

        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                          "Fragment backup: %u", (unsigned) frag_backup_len);
        MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                          "Accumulator backup: %u", (unsigned) acc_backup_len);

        /* Move uncommitted parts from the accumulator to the front
         * of the accumulator. */
        memmove(acc, acc + acc_backup_offset, acc_backup_len);

        /* Copy uncommitted parts of the current fragment to the
         * accumulator. */
        memcpy(acc + acc_backup_len,
               frag + frag_backup_offset, frag_backup_len);

        rd->acc_available = backup_len;
        rd->acc_share.acc_remaining = pending;

        if (paused != NULL) {
            *paused = 1;
        }
    }

    rd->frag     = NULL;
    rd->frag_len = 0;

    rd->commit  = 0;
    rd->end     = 0;
    rd->pending = 0;

    MBEDTLS_MPS_TRACE(MBEDTLS_MPS_TRACE_TYPE_COMMENT,
                      "Final state: aa %u, al %u, ar %u",
                      (unsigned) rd->acc_available, (unsigned) rd->acc_len,
                      (unsigned) rd->acc_share.acc_remaining);
    MBEDTLS_MPS_TRACE_RETURN(0);
}

#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */
