/*
 * Copyright 2022-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <openssl/err.h>
#include "internal/common.h"
#include "internal/time.h"
#include "internal/quic_stream.h"
#include "internal/quic_strm_reas.h"
#include "internal/ring_buf.h"

struct quic_rstream_st {
    SFRAME_SET fs;
    QUIC_RXFC *rxfc;
    OSSL_STATM *statm;
    UINT_RANGE head_range;
};

#if !defined(NDEBUG) && defined(WITH_RSTREAM_DEBUG)
#include <stdio.h>
#define DEBUG_PRINT(...) fprintf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) (void)(0)
#endif

QUIC_RSTREAM *ossl_quic_rstream_new(QUIC_RXFC *rxfc,
    OSSL_STATM *statm, QUIC_RSTREAM_QPARM *rsqp)
{
    QUIC_RSTREAM *ret = OPENSSL_zalloc(sizeof(*ret));

    if (ret == NULL)
        return NULL;

    ossl_sframe_set_init(&ret->fs, rsqp);
    ret->rxfc = rxfc;
    ret->statm = statm;
    return ret;
}

void ossl_quic_rstream_free(QUIC_RSTREAM *qrs)
{
    if (qrs == NULL)
        return;

    ossl_sframe_set_destroy_ranges(&qrs->fs);
    OPENSSL_free(qrs);
}

int ossl_quic_rstream_queue_data(QUIC_RSTREAM *qrs, OSSL_QRX_PKT *pkt,
    uint64_t offset,
    const unsigned char *data, uint64_t data_len,
    int fin)
{
    UINT_RANGE range;

    if ((data == NULL && data_len != 0) || (data_len == 0 && fin == 0)) {
        /* empty frame allowed only at the end of the stream */
        ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    range.start = offset;
    range.end = offset + data_len;

    return ossl_sframe_set_insert(&qrs->fs, &range, pkt, data, fin);
}

static int read_internal(QUIC_RSTREAM *qrs, unsigned char *buf, size_t size,
    size_t *readbytes, int *fin, int drop)
{
    void *iter = NULL;
    UINT_RANGE range;
    const unsigned char *data;
    uint64_t offset = 0;
    size_t readbytes_ = 0;
    int fin_ = 0, ret = 1;

    DEBUG_PRINT(stderr, "%s want: %zu\n", OPENSSL_FUNC, size);
    while (ossl_sframe_set_peek(&qrs->fs, &iter, &range, &data, &fin_)) {
        size_t l = (size_t)(range.end - range.start);

        DEBUG_PRINT(stderr, "\t[ %llu, %llu ]\n", range.start, range.end);
        if (l > size) {
            l = size;
            fin_ = 0;
        }
        offset = range.start + l;
        if (l == 0)
            break;

        memcpy(buf, data, l);
        size -= l;
        buf += l;
        readbytes_ += l;
        if (size == 0)
            break;
    }

    if (drop && offset != 0)
        ret = ossl_sframe_set_move_offset(&qrs->fs, offset);

    if (ret) {
        DEBUG_PRINT(stderr, "%s got: %zu\n", OPENSSL_FUNC, readbytes_);
        *readbytes = readbytes_;
        *fin = fin_;
    } else {
        DEBUG_PRINT(stderr, "%s got: nothing\n", OPENSSL_FUNC);
    }

    return ret;
}

static OSSL_TIME get_rtt(QUIC_RSTREAM *qrs)
{
    OSSL_TIME rtt;

    if (qrs->statm != NULL) {
        OSSL_RTT_INFO rtt_info;

        ossl_statm_get_rtt_info(qrs->statm, &rtt_info);
        rtt = rtt_info.smoothed_rtt;
    } else {
        rtt = ossl_time_zero();
    }
    return rtt;
}

int ossl_quic_rstream_read(QUIC_RSTREAM *qrs, unsigned char *buf, size_t size,
    size_t *readbytes, int *fin)
{
    OSSL_TIME rtt = get_rtt(qrs);

    if (!read_internal(qrs, buf, size, readbytes, fin, 1))
        return 0;

    if (qrs->rxfc != NULL
        && !ossl_quic_rxfc_on_retire(qrs->rxfc, *readbytes, rtt))
        return 0;

    return 1;
}

int ossl_quic_rstream_peek(QUIC_RSTREAM *qrs, unsigned char *buf, size_t size,
    size_t *readbytes, int *fin)
{
    return read_internal(qrs, buf, size, readbytes, fin, 0);
}

int ossl_quic_rstream_available(QUIC_RSTREAM *qrs, size_t *avail, int *fin)
{
    uint64_t avail_ = 0;

    ossl_sframe_set_avail(&qrs->fs, &avail_, fin);

#if SIZE_MAX < UINT64_MAX
    *avail = avail_ > SIZE_MAX ? SIZE_MAX : (size_t)avail_;
#else
    *avail = (size_t)avail_;
#endif
    return 1;
}

int ossl_quic_rstream_get_record(QUIC_RSTREAM *qrs,
    const unsigned char **record, size_t *rec_len,
    int *fin)
{
    const unsigned char *record_ = NULL;
    void *iterator = NULL;
    size_t rec_len_;
    int ok;

    ok = ossl_sframe_set_peek(&qrs->fs, &iterator, &qrs->head_range, &record_,
        fin);
    if (ok == 0) {
        *record = NULL;
        *rec_len = 0;
        return 1;
    }

    DEBUG_PRINT(stderr, "%s head: [ %llu, %llu ]\n", OPENSSL_FUNC,
        qrs->head_range.start, qrs->head_range.end);
    /* if final empty frame, we drop it immediately */
    if (qrs->head_range.end == qrs->head_range.start) {
        if (!ossl_assert(*fin))
            return 0;
        if (!ossl_sframe_set_move_offset(&qrs->fs, qrs->head_range.end))
            return 0;
    }

    rec_len_ = (size_t)(qrs->head_range.end - qrs->head_range.start);
    *rec_len = rec_len_;
    *record = record_;

    return 1;
}

int ossl_quic_rstream_release_record(QUIC_RSTREAM *qrs, size_t read_len)
{
    uint64_t offset;

    if (read_len > qrs->head_range.end - qrs->head_range.start) {
        if (read_len != SIZE_MAX)
            return 0;
        offset = qrs->head_range.end;
    } else {
        offset = qrs->head_range.start + read_len;
    }

    if (!ossl_sframe_set_move_offset(&qrs->fs, offset))
        return 0;

    if (qrs->rxfc != NULL) {
        OSSL_TIME rtt = get_rtt(qrs);

        if (!ossl_quic_rxfc_on_retire(qrs->rxfc, offset, rtt))
            return 0;
    }

    return 1;
}

void ossl_quic_rstream_set_cleanse(QUIC_RSTREAM *qrs, int cleanse)
{
    qrs->fs.cleanse = cleanse;
}

size_t ossl_quic_rstream_get_chunk_count(QUIC_RSTREAM *qrs)
{
    return qrs->fs.stream_chunks;
}

size_t ossl_quic_rstream_get_range_count(QUIC_RSTREAM *qrs)
{
    return qrs->fs.stream_ranges;
}
