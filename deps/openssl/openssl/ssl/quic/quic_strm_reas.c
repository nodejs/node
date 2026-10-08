/*
 * Copyright 2022-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/uint_set.h"
#include "internal/common.h"
#include "internal/quic_stream.h"
#include "internal/quic_strm_reas.h"
#include "internal/list.h"
#include "internal/quic_channel.h"

#define STDERR NULL
/* ARGSUSED */
static void print_foo(void *f, ...)
{
}

#define DEBUG_PRINT print_foo

#define DIRECT_STORAGE_SZ (2 * sizeof(void *))

/*
 * Maximal allocation overhead in packet buffers is ~64kB for
 * connection. If ~64kB limit is exceeded, then the newly received
 * chunks are moved from the packet to the stream buffer.
 */
#define PKT_BUFFER_OVERHEAD_TRESHOLD (65535)

/*
 * storage type indicates where stream data bytes
 * are stored.
 */
enum {
    ST_TYPE_DIRECT, /* in chunk structure itself (sc_dstorage) */
    ST_TYPE_PKT, /* bytes are stored in attached pkt (sc_pkt) */
    ST_TYPE_HEAP /* data are stored on memory heap buffer */
};

/*
 * Stream chunk keeps stream bytes as received from QUIC STREAM_FRAME.
 * Each chunk of stream data by [start, end).
 */
struct stream_chunk_t {
    OSSL_LIST_MEMBER(sc, struct stream_chunk_t);
    UINT_RANGE sc_range;
    int sc_st; /* storage type */
    union {
        const unsigned char *u_data;
        unsigned char *u_data_w;
    } sc_data_u;
    union {
        OSSL_QRX_PKT *u_sc_pkt;
        unsigned char *u_sc_buf;
        unsigned char u_sc_dstorage[DIRECT_STORAGE_SZ];
    } sc_storage_u;
};

struct quic_rstream_qparm_st {
    size_t rsqp_pkt_overhead_treshold;
    size_t rsqp_pkt_overhead_sz;
    QUIC_CHANNEL *rsqp_ch;
};

#define sc_data sc_data_u.u_data
#define sc_data_w sc_data_u.u_data_w

#define sc_pkt sc_storage_u.u_sc_pkt
#define sc_buf sc_storage_u.u_sc_buf
#define sc_dstorage sc_storage_u.u_sc_dstorage

DEFINE_LIST_OF(sc, struct stream_chunk_t);

#define SCHUNK_SIZE(_sc) ((_sc)->sc_range.end - (_sc)->sc_range.start)
#define SRANGE_SIZE(_sr) ((_sr)->sr_range.end - (_sr)->sr_range.start)
#define SCHUNK_OVERHEAD(_pkt, _sc) ((_pkt)->datagram_len - SCHUNK_SIZE(_sc))

/*
 * Stream range keeps list of continuous stream chunks. The range
 * is also defined by [start, end) interval. For every chunk
 * in range this assertion must hold:
 *    sc->sc_range.end == sc->sc_next->sc_range.start
 *
 * If newly arriving stream chunk can not be inserted to existing
 * stream range, then new range must be created.
 */
struct stream_range_t {
    OSSL_LIST(sc)
    sr_chunks;
    OSSL_RBT_ENTRY(stream_range_t)
    sr_rbe;
    UINT_RANGE sr_range;
    struct stream_chunk_t *sr_it_sc; /* iterator */
};

static int srange_cmp(const struct stream_range_t *, const struct stream_range_t *);

OSSL_RBT_PROTOTYPE(srange, stream_range_t, sr_rbe, srange_cmp)

OSSL_RBT_GENERATE(srange, stream_range_t, sr_rbe, srange_cmp);

#define UINT64_TO_SIZE_T(_x) ((size_t)(((_x) > SIZE_MAX) ? SIZE_MAX : (_x)))

static void rsqp_add_overhead(QUIC_RSTREAM_QPARM *rsqp, size_t sc_overhead)
{
    rsqp->rsqp_pkt_overhead_sz += sc_overhead;
}

static void rsqp_sub_overhead(QUIC_RSTREAM_QPARM *rsqp, size_t sc_overhead)
{
    rsqp->rsqp_pkt_overhead_sz -= sc_overhead;
}

/*
 * Cleansing (SSL_OP_CLEANSE_PLAINTEXT) must write through the const
 * data pointers received from ossl_sframe_set_insert(), which may
 * point into a shared packet buffer. That is safe: each chunk
 * references the disjoint payload slice of its own frame and a
 * processed packet is kept alive only by the chunks stored on it,
 * so nobody else reads the wiped bytes.
 *
 * The const should eventually be dropped from the prototypes
 * instead; until then deconst() is used.
 */
static unsigned char *deconst(const unsigned char *data)
{
    union {
        const unsigned char *u_data;
        unsigned char *u_data_w;
    } data_u;

    data_u.u_data = data;

    return data_u.u_data_w;
}

static void sc_data_trim_left(struct stream_chunk_t *sc, size_t trim_sz,
    int cleanse)
{
    if (sc->sc_st == ST_TYPE_DIRECT) {
        assert(SCHUNK_SIZE(sc) >= trim_sz);
        memmove(sc->sc_data_w, &sc->sc_data[trim_sz],
            UINT64_TO_SIZE_T((SCHUNK_SIZE(sc) - trim_sz)));
        if (cleanse && trim_sz > 0) {
            OPENSSL_cleanse(
                sc->sc_data_w + UINT64_TO_SIZE_T(SCHUNK_SIZE(sc) - trim_sz),
                UINT64_TO_SIZE_T(trim_sz));
        }
    } else {
        if (cleanse && trim_sz > 0) {
            OPENSSL_cleanse(sc->sc_data_w, trim_sz);
        }
        sc->sc_data += trim_sz;
    }
}

static void sc_data_trim_right(struct stream_chunk_t *sc, size_t trim_sz,
    int cleanse)
{
    unsigned char *data_realloc;
    size_t w_offset;

    assert(SCHUNK_SIZE(sc) >= trim_sz);

    if (cleanse)
        OPENSSL_cleanse(
            &sc->sc_data_w[sc->sc_range.end - trim_sz - sc->sc_range.start],
            trim_sz);

    if (sc->sc_st == ST_TYPE_HEAP) {
        /*
         * this is a shrinking realloc() here, so it should not fail.
         * even if it fails, we still don't care, the worst outcome
         * of such failure is waste of memory.
         */
        assert(sc->sc_data_w >= sc->sc_buf);
        w_offset = sc->sc_data_w - sc->sc_buf;
        if (trim_sz > 0) {
            assert(SCHUNK_SIZE(sc) > trim_sz);
            data_realloc = OPENSSL_realloc(sc->sc_buf,
                w_offset + UINT64_TO_SIZE_T(SCHUNK_SIZE(sc)) - trim_sz);
            if (data_realloc != NULL) {
                sc->sc_buf = data_realloc;
                sc->sc_data_w = sc->sc_buf + w_offset;
            }
        }
    }
}

static int srange_cmp(const struct stream_range_t *a_sr,
    const struct stream_range_t *b_sr)
{
    assert(a_sr->sr_range.start < a_sr->sr_range.end);
    assert(b_sr->sr_range.start < b_sr->sr_range.end);
    /*
     * no overlap, A precedes B
     */
    if (a_sr->sr_range.end < b_sr->sr_range.start)
        return -1;

    /*
     * no overlap, A follows B
     */
    if (a_sr->sr_range.start > b_sr->sr_range.end)
        return 1;

    /*
     * partial or full overlap or ranges are adjacent.
     * the program needs to do close examination on
     * how to add new chunk to existing stream range.
     */
    return 0;
}

static int keep_schunk_data_on_packet(SFRAME_SET *fs, OSSL_QRX_PKT *pkt,
    size_t overhead)
{
    if ((fs->rsqp->rsqp_pkt_overhead_sz + overhead) >= fs->rsqp->rsqp_pkt_overhead_treshold)
        return 0;

    return 1;
}

static struct stream_chunk_t *new_schunk(SFRAME_SET *fs, OSSL_QRX_PKT *pkt,
    UINT_RANGE *r, const unsigned char *data)
{
    struct stream_chunk_t *sc;
    uint64_t rsize;
    size_t overhead;

    if (pkt == NULL)
        return NULL;

    sc = OPENSSL_zalloc(sizeof(*sc));
    if (sc == NULL)
        return NULL;

    rsize = r->end - r->start;
    assert(rsize <= pkt->datagram_len);
    overhead = UINT64_TO_SIZE_T(pkt->datagram_len - rsize);

    if (keep_schunk_data_on_packet(fs, pkt, overhead) == 1) {
        sc->sc_st = ST_TYPE_PKT;
        sc->sc_pkt = pkt;
        ossl_qrx_pkt_up_ref(pkt);
        rsqp_add_overhead(fs->rsqp, overhead);
        sc->sc_data = data;
        sc->sc_range = *r;
        DEBUG_PRINT(STDERR,
            "%s sc: %p sc overhead: %llu pkt_buf_overhead_sz: %llu -> %zu\n",
            OPENSSL_FUNC, (void *)sc, SCHUNK_OVERHEAD(pkt, sc),
            fs->rsqp->rsqp_pkt_overhead_sz - SCHUNK_OVERHEAD(pkt, sc),
            fs->rsqp->rsqp_pkt_overhead_sz);
    } else {
        if (rsize <= DIRECT_STORAGE_SZ) {
            DEBUG_PRINT(STDERR, "%s ST_TYPE_DIRECT sc: %p %llu\n", OPENSSL_FUNC,
                (void *)sc, rsize);
            sc->sc_st = ST_TYPE_DIRECT;
            sc->sc_data_w = sc->sc_dstorage;
        } else {
            DEBUG_PRINT(STDERR, "%s ST_TYPE_HEAP sc: %p %llu\n", OPENSSL_FUNC,
                (void *)sc, rsize);
            sc->sc_st = ST_TYPE_HEAP;
            sc->sc_buf = OPENSSL_malloc(UINT64_TO_SIZE_T(rsize));
            if (sc->sc_buf == NULL) {
                OPENSSL_free(sc);
                return NULL;
            }
            sc->sc_data_w = sc->sc_buf;
        }
        sc->sc_range = *r;
        memcpy(sc->sc_data_w, data, UINT64_TO_SIZE_T(rsize));

        if (fs->cleanse)
            OPENSSL_cleanse(deconst(data), UINT64_TO_SIZE_T(rsize));
    }

    return sc;
}

static void destroy_schunk(SFRAME_SET *fs, struct stream_chunk_t *sc)
{
    if (sc == NULL)
        return;

    if (fs->cleanse)
        OPENSSL_cleanse(sc->sc_data_w,
            UINT64_TO_SIZE_T(sc->sc_st == ST_TYPE_DIRECT
                    ? DIRECT_STORAGE_SZ
                    : SCHUNK_SIZE(sc)));

    switch (sc->sc_st) {
    case ST_TYPE_PKT:
        assert(fs->rsqp->rsqp_pkt_overhead_sz >= SCHUNK_OVERHEAD(sc->sc_pkt, sc));
        DEBUG_PRINT(STDERR,
            "%s sc: %p sc overhead: %llu pkt_buf_overhead_sz: %zu -> %llu\n",
            OPENSSL_FUNC, (void *)sc, SCHUNK_OVERHEAD(sc->sc_pkt, sc),
            fs->rsqp->rsqp_pkt_overhead_sz,
            fs->rsqp->rsqp_pkt_overhead_sz - SCHUNK_OVERHEAD(sc->sc_pkt, sc));
        rsqp_sub_overhead(fs->rsqp,
            UINT64_TO_SIZE_T(SCHUNK_OVERHEAD(sc->sc_pkt, sc)));
        ossl_qrx_pkt_release(sc->sc_pkt);
        break;
    case ST_TYPE_HEAP:
        OPENSSL_free(sc->sc_buf);
        break;
    default:
        assert(sc->sc_st == ST_TYPE_DIRECT);
    }

    OPENSSL_free(sc);
}

static struct stream_range_t *new_srange(void)
{
    struct stream_range_t *sr;

    sr = OPENSSL_zalloc(sizeof(*sr));
    if (sr != NULL) {
        ossl_list_sc_init(&sr->sr_chunks);
    }

    return sr;
}

static void destroy_srange(SFRAME_SET *fs, struct stream_range_t *sr)
{
    struct stream_chunk_t *sc;

    if (sr == NULL)
        return;

    assert(sr->sr_rbe.rb_parent == NULL);
    assert(sr->sr_rbe.rb_left == NULL);
    assert(sr->sr_rbe.rb_right == NULL);

    while ((sc = ossl_list_sc_head(&sr->sr_chunks)) != NULL) {
        ossl_list_sc_remove(&sr->sr_chunks, sc);
        fs->stream_chunks--;
        destroy_schunk(fs, sc);
    }

    OPENSSL_free(sr);
}

static struct stream_range_t *create_range(SFRAME_SET *fs,
    struct stream_chunk_t *sc)
{
    struct stream_range_t *sr;

    assert(sc != NULL);

    sr = new_srange();
    if (sr != NULL) {
        ossl_list_sc_insert_head(&sr->sr_chunks, sc);
        sr->sr_range = sc->sc_range;
        fs->stream_chunks++;
    }

    return sr;
}

void ossl_sframe_set_init(SFRAME_SET *fs, QUIC_RSTREAM_QPARM *rsqp)
{
    assert(rsqp != NULL);

    memset(fs, 0, sizeof(*fs));
    OSSL_RBT_INIT(srange, &fs->ranges);
    fs->rsqp = rsqp;
}

static uint64_t get_sc_dstorage_sz(struct stream_chunk_t *sc)
{
    uint64_t sz = 0;

    if (sc->sc_st == ST_TYPE_DIRECT && SCHUNK_SIZE(sc) < DIRECT_STORAGE_SZ)
        sz = DIRECT_STORAGE_SZ - SCHUNK_SIZE(sc);

    return sz;
}

static int try_dstorage(SFRAME_SET *fs, OSSL_QRX_PKT *pkt,
    struct stream_range_t *sr, UINT_RANGE *r, const unsigned char **data)
{
    struct stream_chunk_t *head_sc, *tail_sc, *new_sc;
    uint64_t rsize;
    uint64_t offset;
    uint64_t dsize;

    /*
     * full overlap which spans over more range with more than 1 chunk,
     * nothing to be done here, caller will handle that.
     */
    rsize = r->end - r->start;
    if (r->start < sr->sr_range.start && r->end > sr->sr_range.end
        && ossl_list_sc_num(&sr->sr_chunks) > 1)
        return 0;

    head_sc = ossl_list_sc_head(&sr->sr_chunks);
    assert(head_sc != NULL);
    tail_sc = ossl_list_sc_tail(&sr->sr_chunks);
    assert(tail_sc != NULL);

    /*
     * full overlap of direct storage can be treated when range contains
     * exactly one chunk.
     */
    if (head_sc == tail_sc
        && head_sc->sc_range.start > r->start
        && head_sc->sc_range.end < r->end) {
        /*
         * can deal with full overlap
         */
        if (head_sc->sc_st != ST_TYPE_DIRECT)
            return 0;

        DEBUG_PRINT(STDERR, "%s @in %p [ %llu, %llu ]\n",
            OPENSSL_FUNC, *data, r->start, r->end);
        rsize = r->end - r->start;
        if (rsize <= DIRECT_STORAGE_SZ) {
            /*
             * update existing chunk
             */
            DEBUG_PRINT(STDERR,
                "%s overwrite dstorage %p [ %llu, %llu ] -> [ %llu, %llu ] "
                "sr: %p [ %llu, %llu ]\n",
                OPENSSL_FUNC, (void *)head_sc,
                head_sc->sc_range.start, head_sc->sc_range.end,
                r->start, r->end,
                (void *)sr, sr->sr_range.start, sr->sr_range.end);
            memcpy(head_sc->sc_data_w, *data, UINT64_TO_SIZE_T(rsize));

            if (fs->cleanse)
                OPENSSL_cleanse(deconst(*data), UINT64_TO_SIZE_T(rsize));

            *data += rsize;
            head_sc->sc_range.start = r->start;
            head_sc->sc_range.end = r->end;
        } else {
            /*
             * try to replace existing chunk
             */
            new_sc = new_schunk(fs, pkt, r, *data);
            if (new_sc == NULL) {
                DEBUG_PRINT(STDERR, "%s new_chunk() alloc failed\n",
                    OPENSSL_FUNC);
                return -1;
            }

            DEBUG_PRINT(STDERR,
                "%s replace chunk %p [ %llu, %llu ] -> %p [ %llu, %llu ]\n",
                OPENSSL_FUNC,
                (void *)head_sc, head_sc->sc_range.start, head_sc->sc_range.end,
                (void *)new_sc, new_sc->sc_range.start, new_sc->sc_range.end);
            ossl_list_sc_remove(&sr->sr_chunks, head_sc);
            ossl_list_sc_insert_head(&sr->sr_chunks, new_sc);
            destroy_schunk(fs, head_sc);
        }
        DEBUG_PRINT(STDERR, "\trange: %p [ %llu, %llu ] -> [ %llu, %llu ]\n",
            (void *)sr, sr->sr_range.start, sr->sr_range.end, r->start, r->end);
        sr->sr_range.start = r->start;
        sr->sr_range.end = r->end;
        /*
         * indicate that while range got consumed.
         */
        r->start = 0;
        r->end = 0;
    } else if (tail_sc->sc_range.end < r->end) {
        /*
         * append only
         */
        if (tail_sc->sc_st != ST_TYPE_DIRECT)
            return 0;

        dsize = get_sc_dstorage_sz(tail_sc);
        if (dsize == 0)
            return 0;

        DEBUG_PRINT(STDERR, "%s append: @in %p [ %llu, %llu ] dsize: %llu "
                            "tail_sc: %p [ %llu, %llu] sr: %p [ %llu, %llu ]\n",
            OPENSSL_FUNC, *data, r->start, r->end, dsize,
            (void *)tail_sc, tail_sc->sc_range.start, tail_sc->sc_range.end,
            (void *)sr, sr->sr_range.start, sr->sr_range.end);

        if (r->start < tail_sc->sc_range.end) {
            rsize = tail_sc->sc_range.end - r->start;

            if (fs->cleanse)
                OPENSSL_cleanse(deconst(*data), UINT64_TO_SIZE_T(rsize));

            *data += rsize;

            r->start = tail_sc->sc_range.end;
        }

        rsize = r->end - r->start;
        /*
         * earlier check done in ossl_sframe_set_insert() ensures
         * there is at least some data to append.
         */
        assert(rsize > 0);
        rsize = (rsize < dsize) ? rsize : dsize;
        offset = SCHUNK_SIZE(tail_sc);
        memcpy(&tail_sc->sc_data_w[offset], *data, UINT64_TO_SIZE_T(rsize));
        DEBUG_PRINT(STDERR, "%s append tail_sc: %p [ %llu, %llu ] -> ",
            OPENSSL_FUNC, (void *)tail_sc,
            tail_sc->sc_range.start, tail_sc->sc_range.end);
        tail_sc->sc_range.end += rsize;
        DEBUG_PRINT(STDERR, "[ %llu, %llu ]\n",
            tail_sc->sc_range.start, tail_sc->sc_range.end);
        assert(SCHUNK_SIZE(tail_sc) <= DIRECT_STORAGE_SZ);
        DEBUG_PRINT(STDERR, "\trange: %p [ %llu, %llu ] -> ",
            (void *)sr, sr->sr_range.start, sr->sr_range.end);
        sr->sr_range.end = tail_sc->sc_range.end;
        DEBUG_PRINT(STDERR, "[ %llu, %llu ]\n",
            sr->sr_range.start, sr->sr_range.end);

        if (fs->cleanse)
            OPENSSL_cleanse(deconst(*data), UINT64_TO_SIZE_T(rsize));

        *data += rsize;
        r->start = tail_sc->sc_range.end;
    } else if (head_sc->sc_range.start > r->start) {
        const unsigned char *data_buf;
        /*
         * prepend only
         */
        if (head_sc->sc_st != ST_TYPE_DIRECT)
            return 0;

        dsize = get_sc_dstorage_sz(head_sc);
        if (dsize == 0)
            return 0;

        DEBUG_PRINT(STDERR, "%s prepend: @in %p [ %llu, %llu ] dsize: %llu "
                            "sr: %p [ %llu, %llu ]\n",
            OPENSSL_FUNC, *data, r->start, r->end, dsize,
            (void *)sr, sr->sr_range.start, sr->sr_range.end);

        if (r->end > head_sc->sc_range.start) {
            if (fs->cleanse)
                OPENSSL_cleanse(
                    deconst(*data + (head_sc->sc_range.start - r->start)),
                    UINT64_TO_SIZE_T(r->end - head_sc->sc_range.start));
            r->end = head_sc->sc_range.start;
        }

        rsize = r->end - r->start;
        /*
         * earlier check done in ossl_sframe_set_insert() ensures
         * there is at least some data to append.
         */
        assert(rsize > 0);
        rsize = (rsize < dsize) ? rsize : dsize;
        memmove(&head_sc->sc_data_w[rsize], head_sc->sc_data,
            UINT64_TO_SIZE_T(SCHUNK_SIZE(head_sc)));
        offset = r->end - rsize - r->start;
        assert(offset < r->end - r->start);
        data_buf = *data;
        memcpy(head_sc->sc_data_w, &data_buf[offset], UINT64_TO_SIZE_T(rsize));
        DEBUG_PRINT(STDERR, "%s prepend head_sc: %p [ %llu, %llu ] -> ",
            OPENSSL_FUNC, (void *)head_sc,
            head_sc->sc_range.start, head_sc->sc_range.end);
        head_sc->sc_range.start -= rsize;
        DEBUG_PRINT(STDERR, "[ %llu, %llu ]\n",
            head_sc->sc_range.start, head_sc->sc_range.end);
        assert(SCHUNK_SIZE(head_sc) <= DIRECT_STORAGE_SZ);
        DEBUG_PRINT(STDERR, "\trange: %p [ %llu, %llu ] -> ",
            (void *)sr, sr->sr_range.start, sr->sr_range.end);
        sr->sr_range.start = head_sc->sc_range.start;
        DEBUG_PRINT(STDERR, "[ %llu, %llu ]\n",
            sr->sr_range.start, sr->sr_range.end);
        assert(r->end - r->start >= rsize);

        if (fs->cleanse)
            OPENSSL_cleanse(deconst(&data_buf[offset]),
                UINT64_TO_SIZE_T(rsize));

        r->end -= rsize;
    } else {
        assert(0);
        return -1;
    }

    /*
     * returns 1 if all data were consumed
     */
    assert(r->end >= r->start);
    DEBUG_PRINT(STDERR, "%s @out %p [ %llu, %llu ]\n",
        OPENSSL_FUNC, *data, r->start, r->end);

    return ((r->end - r->start) == 0) ? 1 : 0;
}

/*
 * If there is partial overlap between newly received data `r` and
 * existing stream range `sr`, then this function trims overlapping
 * bytes from `r`.
 *    sr   - pointer to stream range
 *    r    - pointer to range of bytes received in stream frame
 *    data - pointer to data bytes delivered in stream frame
 * function updates r so there is no partial overlap between and sr
 * after function returns. Function returns pointer to the first
 * data byte in stream after `r` is adjusted. Function returns `data`
 * when no trimming happened.
 */
static const unsigned char *trim_partial_overlap(SFRAME_SET *fs,
    struct stream_range_t *sr, UINT_RANGE *r, const unsigned char *data)
{
    uint64_t unused_sz;

    if (!(r->start < sr->sr_range.start && r->end > sr->sr_range.end)) {
        if (r->end > sr->sr_range.end && r->start < sr->sr_range.end) {
            unused_sz = sr->sr_range.end - r->start;
            if (fs->cleanse)
                OPENSSL_cleanse(deconst(data), UINT64_TO_SIZE_T(unused_sz));

            DEBUG_PRINT(STDERR, "%s right overlap %p [ %llu, %llu ]:\n\t"
                                "r: [ %llu, %llu ] -> [ %llu, %llu ]\n",
                OPENSSL_FUNC,
                (void *)sr, sr->sr_range.start, sr->sr_range.end,
                r->start, r->end,
                sr->sr_range.end, r->end);

            data += unused_sz;
            r->start = sr->sr_range.end;
        } else if (r->start < sr->sr_range.start
            && r->end > sr->sr_range.start) {
            unused_sz = r->end - sr->sr_range.start;
            if (fs->cleanse)
                OPENSSL_cleanse(deconst(data
                                    + (sr->sr_range.start - r->start)),
                    UINT64_TO_SIZE_T(unused_sz));

            DEBUG_PRINT(STDERR, "%s left overlap %p [ %llu, %llu]:\n\t"
                                "r: [ %llu, %llu ] -> [ %llu, %llu ]\n",
                OPENSSL_FUNC,
                (void *)sr, sr->sr_range.start, sr->sr_range.end,
                r->start, r->end,
                r->start, sr->sr_range.start);
            r->end = sr->sr_range.start;
        }
    }

    return data;
}

/*
 * Inserts a newly received chunk to the head of the chunk list.
 */
static void prepend_chunk(SFRAME_SET *fs, struct stream_range_t *sr,
    struct stream_chunk_t *sc)
{
    DEBUG_PRINT(STDERR, "%s %p [ %llu, %llu ] add to head %p [ %llu, %llu ] "
                        "-> [ %llu, %llu ]\n",
        OPENSSL_FUNC,
        (void *)sc, sc->sc_range.start, sc->sc_range.end,
        (void *)sr, sr->sr_range.start, sr->sr_range.end,
        sc->sc_range.start, sr->sr_range.end);

    /*
     * the new chunk must not be empty
     */
    assert(sc->sc_range.end > sc->sc_range.start);
    /*
     * and must not overlap range.
     */
    assert(sc->sc_range.end == sr->sr_range.start);

    ossl_list_sc_insert_head(&sr->sr_chunks, sc);
    sr->sr_range.start = sc->sc_range.start;
    fs->stream_chunks++;
}

/*
 * Inserts a newly received chunk to the tail of the chunk list.
 */
static void append_chunk(SFRAME_SET *fs, struct stream_range_t *sr,
    struct stream_chunk_t *sc)
{
    DEBUG_PRINT(STDERR, "%s %p [ %llu, %llu ] add to tail %p [ %llu, %llu ] "
                        "-> [ %llu, %llu ]\n",
        OPENSSL_FUNC,
        (void *)sc, sc->sc_range.start, sc->sc_range.end,
        (void *)sr, sr->sr_range.start, sr->sr_range.end,
        sr->sr_range.start, sc->sc_range.end);

    /*
     * the new chunk must not be empty
     */
    assert(sc->sc_range.end > sc->sc_range.start);
    /*
     * and must not overlap range
     */
    assert(sc->sc_range.start == sr->sr_range.end);

    ossl_list_sc_insert_tail(&sr->sr_chunks, sc);
    sr->sr_range.end = sc->sc_range.end;
    fs->stream_chunks++;
}

static void replace_chunks_in_range(SFRAME_SET *fs, struct stream_range_t *sr,
    struct stream_chunk_t *sc)
{
    struct stream_chunk_t *destroy_sc;

    while ((destroy_sc = ossl_list_sc_head(&sr->sr_chunks)) != NULL) {
        ossl_list_sc_remove(&sr->sr_chunks, destroy_sc);
        fs->stream_chunks--;
        destroy_schunk(fs, destroy_sc);
    }

    ossl_list_sc_insert_head(&sr->sr_chunks, sc);
    fs->stream_chunks++;
    DEBUG_PRINT(STDERR, "%s range: %p [ %llu, %llu ] -> [ %llu, %llu ]\n",
        OPENSSL_FUNC, (void *)sr, sr->sr_range.start, sr->sr_range.end,
        sc->sc_range.start, sc->sc_range.end);
    sr->sr_range.start = sc->sc_range.start;
    sr->sr_range.end = sc->sc_range.end;
}

static struct stream_range_t *find_range(SFRAME_SET *fs,
    struct stream_range_t *key)
{
    struct stream_range_t *sr = NULL;

    if (!OSSL_RBT_EMPTY(srange, &fs->ranges))
        sr = OSSL_RBT_FIND(srange, &fs->ranges, key);

    return sr;
}

/*
 * This function help us to merge two ranges (list of chunks)
 * into single range. Function moves the end of the range
 * towards start. It effectively chops n last chunks until
 * new_end is found.
 */
static int chop_range(SFRAME_SET *fs, struct stream_range_t *sr,
    uint64_t new_end)
{
    struct stream_chunk_t *sc;
    size_t unused_sz;

    assert(sr->sr_range.end >= new_end);

    while ((sc = ossl_list_sc_tail(&sr->sr_chunks)) != NULL) {
        if (sc->sc_range.start >= new_end) {
            ossl_list_sc_remove(&sr->sr_chunks, sc);
            fs->stream_chunks--;
            destroy_schunk(fs, sc);
        } else {
            break;
        }
    }

    if (sc == NULL)
        return 0;

    assert(new_end <= sc->sc_range.end);
    assert(sc->sc_range.start < new_end);

    unused_sz = UINT64_TO_SIZE_T(sc->sc_range.end - new_end);
    if (unused_sz == 0) {
        sr->sr_range.end = new_end;
        return 1;
    }

    sc_data_trim_right(sc, unused_sz, fs->cleanse);
    sc->sc_range.end = new_end;
    sr->sr_range.end = new_end;

    if (sc->sc_st == ST_TYPE_PKT) {
        rsqp_add_overhead(fs->rsqp, unused_sz);
        DEBUG_PRINT(STDERR, "%s sc: %p unused_sz: %zu %zu -> %zu\n",
            OPENSSL_FUNC, (void *)sc, unused_sz,
            fs->rsqp->rsqp_pkt_overhead_sz - unused_sz,
            fs->rsqp->rsqp_pkt_overhead_sz);
    }

    return 1;
}

/*
 * function merges two with full overlap. The super_sr range
 * contains the whole sub_sr range. The function destroys
 * sub_sr and returns super_sr.
 */
static struct stream_range_t *merge_ranges(SFRAME_SET *fs,
    struct stream_range_t *super_sr, struct stream_range_t *sub_sr)
{
    /*
     * both ranges must not be empty
     */
    assert(super_sr->sr_range.start < super_sr->sr_range.end);
    assert(sub_sr->sr_range.start < sub_sr->sr_range.end);
    /*
     * sub_sr and super_sr are equal ranges (sets)  super_sr
     * sub_sr is subset of super_sr (super_sr includes sub_sr).
     */
    assert(super_sr->sr_range.start <= sub_sr->sr_range.start
        && super_sr->sr_range.end >= sub_sr->sr_range.end);

    DEBUG_PRINT(STDERR, "%s super: %p [ %llu, %llu ], sub: %p [ %llu, %llu]\n",
        OPENSSL_FUNC, (void *)super_sr, super_sr->sr_range.start,
        super_sr->sr_range.end, (void *)sub_sr, sub_sr->sr_range.start,
        sub_sr->sr_range.end);
    destroy_srange(fs, sub_sr);

    return super_sr;
}

/*
 * The ranges are either adjacent
 * (left_sr->sr_range.end == right_sr->sr_range.end) or there
 * is partial overlap between left_sr and right_sr(
 * (left_sr->sr_range.end >= right_sr->sr_range.start).
 * If there is partial overlap, then the left range is chopped
 * so its end is aligned with start of right_sr.
 */
static struct stream_range_t *append_range(SFRAME_SET *fs,
    struct stream_range_t *left_sr, struct stream_range_t *right_sr)
{
    /*
     * both ranges must not be empty
     */
    assert(left_sr->sr_range.start < left_sr->sr_range.end);
    assert(right_sr->sr_range.start < right_sr->sr_range.end);
    /*
     * right range follows left range (left < right)
     */
    assert(left_sr->sr_range.end >= right_sr->sr_range.start);

    DEBUG_PRINT(STDERR, "%s %p [ %llu, %llu ] + %p [ %llu, %llu ] = %p "
                        "[ %llu, %llu ]\n",
        OPENSSL_FUNC, (void *)left_sr, left_sr->sr_range.start,
        left_sr->sr_range.end, (void *)right_sr, right_sr->sr_range.start,
        right_sr->sr_range.end, (void *)left_sr,
        left_sr->sr_range.start, right_sr->sr_range.end);

    /*
     * make sure there is no overlap between ranges
     *    (right_sr->sr_range.start == left_sr->sr_range.end)
     */
    if (chop_range(fs, left_sr, right_sr->sr_range.start) == 0)
        return NULL;

    ossl_list_sc_join(&left_sr->sr_chunks, &right_sr->sr_chunks);
    left_sr->sr_range.end = right_sr->sr_range.end;

    destroy_srange(fs, right_sr);

    return left_sr;
}

/*
 * receives a chunk of data from stream frame.
 * note there is a tri-state return value:
 */
int ossl_sframe_set_insert(SFRAME_SET *fs, UINT_RANGE *r, OSSL_QRX_PKT *pkt,
    const unsigned char *data, int fin)
{
    struct stream_range_t *sr = NULL;
    struct stream_range_t *adjacent_sr = NULL;
    struct stream_range_t *joined_sr = NULL;
    struct stream_chunk_t *sc = NULL;
    struct stream_range_t key_sr = { 0 };

    assert(r->start <= r->end);

    /*
     * receive the FIN frame. If FIN was not seen yet, then record
     * FIN's offset (r->end). If FIN was received then verify FIN's
     * offset match, error out on mismatch.
     */
    if (fin != 0) {
        if (fs->fin == 0) {
            sr = OSSL_RBT_MAX(srange, &fs->ranges);
            if (r->end < fs->offset
                || (sr != NULL && sr->sr_range.end > r->end)) {
                ossl_quic_channel_raise_protocol_error(fs->rsqp->rsqp_ch,
                    OSSL_QUIC_ERR_FINAL_SIZE_ERROR,
                    OSSL_QUIC_FRAME_TYPE_STREAM_FIN,
                    "stream final size error");
                return 0;
            }
            fs->fin = 1;
            fs->fin_off = r->end;
        } else if (fs->fin_off != r->end) {
            ossl_quic_channel_raise_protocol_error(fs->rsqp->rsqp_ch,
                OSSL_QUIC_ERR_FINAL_SIZE_ERROR,
                OSSL_QUIC_FRAME_TYPE_STREAM_FIN,
                "stream final size error");
            return 0;
        }
    }

    /*
     * reject any data at or past the FIN offset (if FIN offset is set).
     */
    if (fs->fin != 0) {
        if (fs->fin_off < r->end) {
            ossl_quic_channel_raise_protocol_error(fs->rsqp->rsqp_ch,
                OSSL_QUIC_ERR_FINAL_SIZE_ERROR,
                (fin == 0) ? OSSL_QUIC_FRAME_TYPE_STREAM
                           : OSSL_QUIC_FRAME_TYPE_STREAM_FIN,
                "stream final size error");
            return 0;
        }
    }

    if (r->end <= fs->offset) {
        /*
         * retransmitted range got consumed already.
         */
        DEBUG_PRINT(STDERR, "%s [ %llu, %llu ] <= %llu\n", OPENSSL_FUNC,
            r->start, r->end, fs->offset);
        if (fs->cleanse && data != NULL)
            OPENSSL_cleanse(deconst(data), UINT64_TO_SIZE_T(r->end - r->start));
        return 1;
    }

    if (r->start < fs->offset) {
        /*
         * Make sure retransmitted chunk does not reintroduce
         * bytes which were consumed already.
         * Make sure retransmitted chunk does not reintroduce
         * bytes which were consumed already.
         */
        DEBUG_PRINT(STDERR, "%s [ %llu, %llu ] -> [ %llu, %llu ]\n", OPENSSL_FUNC,
            r->start, r->end, fs->offset, r->end);
        if (fs->cleanse)
            OPENSSL_cleanse(deconst(data),
                UINT64_TO_SIZE_T(fs->offset - r->start));
        data += fs->offset - r->start;
        r->start = fs->offset;
    }

    key_sr.sr_range = *r;

    /*
     * Empty, 0 size chunk can carry the FIN bit only,
     * and that has been just handled above.
     */
    if (r->start == r->end)
        return 1;

    assert(r->start < r->end);

    if ((sr = find_range(fs, &key_sr)) == NULL) {
        sc = new_schunk(fs, pkt, r, data);
        if (sc == NULL)
            goto err;

        sr = create_range(fs, sc);
        if (sr == NULL)
            goto err;
        DEBUG_PRINT(STDERR, "%s chunk: %p [ %llu, %llu ] new range: %p\n",
            OPENSSL_FUNC, (void *)sc, sc->sc_range.start, sc->sc_range.end,
            (void *)sr);
        sc = NULL;
        OSSL_RBT_INSERT(srange, &fs->ranges, sr);
        fs->stream_ranges++;
    } else {
        /*
         * retransmission, the whole chunk is found in existing range already
         */
        if (r->start >= sr->sr_range.start && r->end <= sr->sr_range.end) {
            DEBUG_PRINT(STDERR,
                "%s [ %llu, %llu ] found in %p [ %llu, %llu ]\n", OPENSSL_FUNC,
                r->start, r->end, (void *)sr, sr->sr_range.start,
                sr->sr_range.end);
            if (fs->cleanse)
                OPENSSL_cleanse(deconst(data),
                    UINT64_TO_SIZE_T(r->end - r->start));
            goto done; /* Range is present already. */
        }

        switch (try_dstorage(fs, pkt, sr, r, &data)) {
        case 0:
            break;
        case 1:
            /*
             * all data were consumed, range is updated.
             */
            goto range_updated;
        default:
            /*
             * malloc error. forget the range we found.
             */
            sr = NULL;
            goto err;
        }

        /*
         * full overlap between sr and r is handled by replace_chunks_in_range()
         * we call after we allocate stream chunk sc for newly received range r.
         */
        data = trim_partial_overlap(fs, sr, r, data);

        sc = new_schunk(fs, pkt, r, data);
        if (sc == NULL) {
            sr = NULL;
            goto err;
        }
        DEBUG_PRINT(STDERR, "%s %p [ %llu, %llu ] -> %p [ %llu, %llu ]\n",
            OPENSSL_FUNC, (void *)sc, sc->sc_range.start, sc->sc_range.end,
            (void *)sr, sr->sr_range.start, sr->sr_range.end);

        /*
         * Following calls can still be improved to handle
         * chunks with direct storage better, but I don't think it's
         * worth the effort. out of order short data chunks (less
         * than DIRECT_STORAGE_SZ) should be considered exceptional.
         */
        if (sc->sc_range.start < sr->sr_range.start
            && sc->sc_range.end > sr->sr_range.end) {
            /* new chunk includes the whole range */
            replace_chunks_in_range(fs, sr, sc);
            sc = NULL; /* chunk got consumed */
        } else if (sc->sc_range.end > sr->sr_range.end
            && sc->sc_range.start <= sr->sr_range.end) {
            append_chunk(fs, sr, sc);
            sc = NULL; /* chunk got consumed */
        } else if (sc->sc_range.start < sr->sr_range.start
            && sc->sc_range.end >= sr->sr_range.start) {
            prepend_chunk(fs, sr, sc);
            sc = NULL; /* chunk got consumed */
        } else {
            assert(NULL); /* unreachable */
            sr = NULL;
            goto err;
        }

    range_updated:
        /*
         * Range got updated we may need to join updated range with
         * another ranges which exist in tree. The current range
         * is removed here and used as a search key. If nothing is found
         * range is inserted back to tree.
         *
         * If another range is found the ranges are merged to single
         * range. The process repeats (merging ranges may be cascade effect,
         * where more ranges collapse to single range).  The merge result is
         * removed from tree and used as a search key to find another range.
         * If nothing is found then update is done. otherwise the ranges
         * are merged again.
         */
        OSSL_RBT_REMOVE(srange, &fs->ranges, sr);
        fs->stream_ranges--;
        /*
         * _INSERT() returns range where sr needs to be joined
         */
        adjacent_sr = OSSL_RBT_INSERT(srange, &fs->ranges, sr);

        while (adjacent_sr != NULL) {
            OSSL_RBT_REMOVE(srange, &fs->ranges, adjacent_sr);
            DEBUG_PRINT(STDERR, "%s %p [ %llu, %llu ] >< %p [ %llu, %llu ]\n",
                OPENSSL_FUNC, (void *)sr, sr->sr_range.start, sr->sr_range.end,
                (void *)adjacent_sr, adjacent_sr->sr_range.start,
                adjacent_sr->sr_range.end);
            fs->stream_ranges--;

            if (sr->sr_range.start <= adjacent_sr->sr_range.start
                && sr->sr_range.end >= adjacent_sr->sr_range.end) {
                /*
                 *  adjacent_sr subset of sr
                 */
                joined_sr = merge_ranges(fs, sr, adjacent_sr);
            } else if (sr->sr_range.start >= adjacent_sr->sr_range.start
                && sr->sr_range.end <= adjacent_sr->sr_range.end) {
                /*
                 *  sr subset of adjacent_sr
                 */
                joined_sr = merge_ranges(fs, adjacent_sr, sr);
            } else if (sr->sr_range.start < adjacent_sr->sr_range.start
                && sr->sr_range.end >= adjacent_sr->sr_range.start) {
                /*
                 * adjacent_sr follows sr
                 */
                assert(sr->sr_range.end < adjacent_sr->sr_range.end);
                joined_sr = append_range(fs, sr, adjacent_sr);
            } else if (sr->sr_range.start <= adjacent_sr->sr_range.end
                && sr->sr_range.end > adjacent_sr->sr_range.end) {
                /*
                 *  sr follows adjacent_sr
                 */
                assert(sr->sr_range.end > adjacent_sr->sr_range.end);
                joined_sr = append_range(fs, adjacent_sr, sr);
            } else {
                assert(NULL); /* never happens */
                joined_sr = NULL;
            }
            if (joined_sr == NULL)
                goto err;

            sr = joined_sr;
            adjacent_sr = OSSL_RBT_INSERT(srange, &fs->ranges, sr);
        }
        fs->stream_ranges++;
    }

done:
    return 1;

err:
    destroy_schunk(fs, sc);
    destroy_srange(fs, sr);
    destroy_srange(fs, adjacent_sr);
    /*
     * not enough memory (or another serious error) has occurred,
     * any error here is fatal as some stream chunks could be ACKed
     * already (RFC 9000, 31.1 Packet processing). At least stream
     * needs to be reset. Preferred action is to close connection.
     */

    return 0;
}

/*
 * peeks over the continuous range which is ready to
 * read. ready to read means the fs->offset must be
 * found in range. Also fs->offset can not reach past
 * the first gap in stream data received so far, thus
 * the only range we can use for peek operation is
 * OSSL_RBT_MIN(&fs->ranges).
 *
 * NOTE: it is unsafe to carry more _peek() operations
 * over single SFRMAE_SET.
 */
int ossl_sframe_set_peek(SFRAME_SET *fs, void **iterator,
    UINT_RANGE *range, const unsigned char **data,
    int *fin)
{
    uint64_t start;
    struct stream_range_t *sr = (struct stream_range_t *)*iterator;
    struct stream_chunk_t *sc = NULL;

    if (sr == NULL) {
        sr = OSSL_RBT_MIN(srange, &fs->ranges);
        start = fs->offset;
        if (sr != NULL) {
            sc = ossl_list_sc_head(&sr->sr_chunks);
            sr->sr_it_sc = NULL;
            assert(sc->sc_range.start == sr->sr_range.start);
        }
        /*
         * no chunks are ready to be consumed, if there is a gap.
         */
        if (sc != NULL && sc->sc_range.start > start) {
            DEBUG_PRINT(STDERR, "%s sc: %p sr: %p sc->start %llu, fs->offset: %llu\n",
                OPENSSL_FUNC, (void *)sc, (void *)sr, sc->sc_range.start, start);
            sc = NULL;
        }
    } else if (sr == OSSL_RBT_MIN(srange, &fs->ranges) && sr->sr_it_sc != NULL) {
        /*
         * sr == _RB_MIN(), revalidates iterator in case the range we
         * work with disappears because it's got joined with other range
         * after new chunk arrival. perhaps not issue now as those operations
         * are mutually exclusive now.
         *
         * sr->sr_it_sc becomes NULL on _move() or _flatten() operation.
         *
         * We may need to revisit iterator implementation as current
         * iterator supports one caller only.
         */
        start = sr->sr_it_sc->sc_range.end;
        sc = ossl_list_sc_next(sr->sr_it_sc);
        assert(sc == NULL || sc->sc_range.start == start);
        assert(sc == NULL || sc->sc_range.start < sc->sc_range.end);
    } else {
        /* iterator got invalidated by move/flatten operation on range */
        DEBUG_PRINT(STDERR, "%s iterator got invalidated\n", OPENSSL_FUNC);
        return 0;
    }

    range->start = start;

    if (sc == NULL) {
        range->end = start;
        *data = NULL;
        *iterator = NULL;

        /*
         * set fin only if we are at the end of the stream and application
         * can read from the stream. In other words: there must be no gap
         * between FIN offset and offset where application reads from stream.
         */
        if (fs->fin && start == fs->fin_off)
            *fin = fs->fin;
        else
            *fin = 0;

        DEBUG_PRINT(STDERR, "%s no more chunks\n", OPENSSL_FUNC);

        return 0;
    }

    range->end = sc->sc_range.end;
    /* chunk keeps data always attached, data dies with chunk */
    assert(sc->sc_data != NULL);
    assert(sc->sc_range.start <= start);
    *data = sc->sc_data + (start - sc->sc_range.start);
    *fin = fs->fin && sc->sc_range.end == fs->fin_off;

    if (sr->sr_it_sc != NULL)
        DEBUG_PRINT(STDERR, "%s %p [ %llu, %llu ] %p [ %llu, %llu ]\n",
            OPENSSL_FUNC, (void *)sr->sr_it_sc,
            sr->sr_it_sc->sc_range.start, sr->sr_it_sc->sc_range.end,
            (void *)sc, sc->sc_range.start, sc->sc_range.end);

    sr->sr_it_sc = sc;
    *iterator = sr;

    /*
     * peek operation indicates error if there are no data to read
     * in range.
     */
    DEBUG_PRINT(STDERR,
        "%s peek range: [ %llu, %llu ] range: %p [ %llu, %llu ]\n", OPENSSL_FUNC,
        range->start, range->end, (void *)sr, sr->sr_range.start,
        sr->sr_range.end);

    return (range->start == range->end) ? 0 : 1;
}

void ossl_sframe_set_destroy_ranges(SFRAME_SET *fs)
{
    struct stream_range_t *sr, *save_sr;

    OSSL_RBT_FOREACH_SAFE (sr, srange, &fs->ranges, save_sr) {
        OSSL_RBT_REMOVE(srange, &fs->ranges, sr);
        fs->stream_ranges--;
        destroy_srange(fs, sr);
    }
}

/*
 * moves the read offset, freeing all chunks which end offset
 * is less than new_offset
 *   sc->sc_range.end < new_offset
 */
int ossl_sframe_set_move_offset(SFRAME_SET *fs, uint64_t new_offset)
{
    struct stream_range_t *sr = OSSL_RBT_MIN(srange, &fs->ranges);
    struct stream_chunk_t *sc, *save_sc;
    size_t unused_sz;

    if (new_offset == fs->offset)
        return 1;

    /*
     * Offset can move forward within the continuous head range only.
     * It can not move backward, into a gap or past the head range end.
     */
    if (sr == NULL || new_offset < fs->offset
        || new_offset < sr->sr_range.start || new_offset > sr->sr_range.end)
        return 0;

    fs->offset = new_offset;

    OSSL_LIST_FOREACH_DELSAFE (sc, save_sc, sc, &sr->sr_chunks) {
        if (new_offset >= sc->sc_range.end) {
            ossl_list_sc_remove(&sr->sr_chunks, sc);
            fs->stream_chunks--;
            if (sr->sr_it_sc == sc)
                sr->sr_it_sc = NULL; /* invalidate iterator chunk */
            destroy_schunk(fs, sc);
        } else {
            break;
        }
    }

    DEBUG_PRINT(STDERR, "%s offset: %llu -> %llu range: %p [ %llu, %llu ] -> ",
        OPENSSL_FUNC, fs->offset - new_offset, new_offset,
        (void *)sr, sr->sr_range.start, sr->sr_range.end);

    if (sc == NULL) {
        /*
         * the whole range was consumed.
         * this step invalidates iterator we use in ossl_sframe_peek()
         */
        OSSL_RBT_REMOVE(srange, &fs->ranges, sr);
        destroy_srange(fs, sr);
        fs->stream_ranges--;
        DEBUG_PRINT(STDERR, "[ NULL ]\n");
    } else {
        unused_sz = UINT64_TO_SIZE_T(new_offset - sc->sc_range.start);
        sc_data_trim_left(sc, unused_sz, fs->cleanse);
        sc->sc_range.start = new_offset;
        sr->sr_range.start = new_offset;
        DEBUG_PRINT(STDERR, "[ %lli, %llu ]\n",
            sr->sr_range.start, sr->sr_range.end);

        if (sc->sc_st == ST_TYPE_PKT) {
            rsqp_add_overhead(fs->rsqp, unused_sz);
            DEBUG_PRINT(STDERR, "%s sc: %p unused_sz: %zu %zu -> %zu\n",
                OPENSSL_FUNC, (void *)sc, unused_sz,
                fs->rsqp->rsqp_pkt_overhead_sz - unused_sz,
                fs->rsqp->rsqp_pkt_overhead_sz);
        }
    }

    return 1;
}

/* Contiguous bytes available from fs->offset, in O(log n): the first range. */
int ossl_sframe_set_avail(SFRAME_SET *fs, uint64_t *avail, int *fin)
{
    struct stream_range_t *sr = OSSL_RBT_MIN(srange, &fs->ranges);

    if (sr != NULL && sr->sr_range.start <= fs->offset
        && sr->sr_range.end > fs->offset)
        *avail = sr->sr_range.end - fs->offset;
    else
        *avail = 0;
    *fin = (fs->fin && fs->offset + *avail == fs->fin_off) ? 1 : 0;
    return 1;
}

QUIC_RSTREAM_QPARM *ossl_quic_rstream_qparm_new(QUIC_CHANNEL *ch)
{
    QUIC_RSTREAM_QPARM *rsqp;

    rsqp = OPENSSL_malloc(sizeof(QUIC_RSTREAM_QPARM));
    if (rsqp != NULL) {
        rsqp->rsqp_pkt_overhead_treshold = PKT_BUFFER_OVERHEAD_TRESHOLD;
        rsqp->rsqp_pkt_overhead_sz = 0;
        rsqp->rsqp_ch = ch;
    }

    return rsqp;
}

void ossl_quic_rstream_qparm_destroy(QUIC_RSTREAM_QPARM *rsqp)
{
    if (rsqp != NULL) {
        assert(rsqp->rsqp_pkt_overhead_sz == 0);
        OPENSSL_free(rsqp);
    }
}
