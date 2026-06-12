/*
 *  Copyright 2024-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 *  Licensed under the Apache License 2.0 (the "License").  You may not use
 *  this file except in compliance with the License.  You can obtain a copy
 *  in the file LICENSE in the source distribution or at
 *  https://www.openssl.org/source/license.html
 */

/*
 * Implementation of hq-interop and http/1.0 QUIC server built on top of
 * SSL_poll(3ossl). The server is able to deal with simultaneous connections
 * All I/O operations are non-blocking.
 *
 * The only supported request is get. No HTTP errors are sent back. If
 * request is not understood the channel is reset.
 *
 * If client sends request for example GET /foo_1024.txt the server
 * replies with HTTP/1.0 200 OK with plain-text body. The body is
 * 1k of text foo_1024.txtfoo_1024,txtfoo...
 *
 * To run the server use command as follows:
 *    ./quic-server-ssl-poll-http 8080 chain.pem pkey.pem
 * command above starts server which listens to port localhost:8080
 * Although the server is simple one can use it with tquic_client
 * found at https://tquic.net/. Other 3rd party clients were not tested yet.
 *
 * The main() function creates instance of poll_manager which manages
 * all events (poll_event). If all is good execution calls to
 * run_quic_server() function where QUIC listener is created and passed
 * to manager. Then the polling loop is entered to serve remote clients.
 *
 * Some naming conventions are followed in code:
 *    - pe is used for poll event, it can be associated with listener,
 *      connection or stream.
 *    - pec is used for poll event which handles connection
 *    - pes is used for poll event which handles stream
 *
 * there is also response buffer which helps server to dispatch reply:
 *    - rb is used for any response buffer type
 *    - rts is used for simple text buffer (just data with no HTTP/1.0
 *      headers)
 *    - rtf is used for response text buffers which carry HTTP/1.0 headers
 *      (response text full)
 */
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>

/* Include the appropriate header file for SOCK_STREAM */
#ifdef _WIN32 /* Windows */
# include <stdarg.h>
# include <winsock2.h>
#else /* Linux/Unix */
# include <err.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <netinet/in.h>
# include <unistd.h>
# include <poll.h>
# include <libgen.h>
#endif

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/quic.h>

#include "internal/list.h"

/*
 * This is a simple non-blocking QUIC HTTP/1.0 server application.
 * Server accepts QUIC connections. It then accepts bi-directional
 * stream from client and reads request. By default it sends
 * 12345 bytes back as HTTP/1.0 response to any GET request.
 * If GET request comes with URL for example as follows:
 *     /foo/bar/file_65535.txt
 * then the server sends 64kB of data in HTTP/1.0 response.
 */

#ifdef DEBUG
# define DPRINTF fprintf
#else
# define DPRINTF(...) (void)(0)
#endif

/*
 * format string so we can print SSL_poll() events in more informative
 * way. To print events one does this:
 *   int events = SSL_POLL_EVENT_F | SSL_POLL_EVENT_R;
 *   printf("%s We got events: " POLL_FMT "\n", __func__, POLL_PRINTA(events));
 */
#define POLL_FMT "%s%s%s%s%s%s%s%s%s%s%s%s%s"
#define POLL_PRINTA(_revents_) \
    (_revents_) & SSL_POLL_EVENT_F ? "SSL_POLL_EVENT_F " : "", \
    (_revents_) & SSL_POLL_EVENT_EL ? "SSL_POLL_EVENT_EL" : "", \
    (_revents_) & SSL_POLL_EVENT_EC ? "SSL_POLL_EVENT_EC " : "", \
    (_revents_) & SSL_POLL_EVENT_ECD ? "SSL_POLL_EVENT_ECD " : "", \
    (_revents_) & SSL_POLL_EVENT_ER ? "SSL_POLL_EVENT_ER " : "", \
    (_revents_) & SSL_POLL_EVENT_EW ? "SSL_POLL_EVENT_EW " : "", \
    (_revents_) & SSL_POLL_EVENT_R ? "SSL_POLL_EVENT_R " : "", \
    (_revents_) & SSL_POLL_EVENT_W ? "SSL_POLL_EVENT_W " : "", \
    (_revents_) & SSL_POLL_EVENT_IC ? "SSL_POLL_EVENT_IC " : "", \
    (_revents_) & SSL_POLL_EVENT_ISB ? "SSL_POLL_EVENT_ISB " : "", \
    (_revents_) & SSL_POLL_EVENT_ISU ? "SSL_POLL_EVENT_ISU " : "", \
    (_revents_) & SSL_POLL_EVENT_OSB ? "SSL_POLL_EVENT_OSB " : "", \
    (_revents_) & SSL_POLL_EVENT_OSU ? "SSL_POLL_EVENT_OSU " : ""

/*
 * every poll_event structure has members enumerated here in poll_event_base
 * The poll events are kept in list which is managed by poll_manager instance.
 * However SSL_poll(9ossl) expects an array of SSL_POLL_ITEM structures. Therefore
 * the poll manager needs to construct array of poll_event structures from list.
 * We get two copies of poll_event structures:
 *    - one copy is held in list (original)
 *    - one copy is held in array (copy for SSL_poll())
 * We use pe_self member to easily reach from SSL_poll() copy instance back to
 * original managed by poll manager and used by our application.
 * There are four callbacks:
 *    - pe_cb_in() - triggered when inbound data/stream/connection is coming
 *    - pe_cb_out() - triggered when outbound data/stream/connection is coming
 *    - pe_cb_error() - triggered when polled object enters an error state
 *    - pe_cb_destroy() - this a destructor, application destroy pe_appdata
 * The remaining members are rather self explanatory.
 */
#define poll_event_base \
    SSL_POLL_ITEM pe_poll_item; \
    OSSL_LIST_MEMBER(pe, struct poll_event);\
    uint64_t pe_want_events; \
    uint64_t pe_want_mask; \
    struct poll_manager *pe_my_pm; \
    unsigned char pe_type; \
    struct poll_event *pe_self; \
    void *pe_appdata; \
    int(*pe_cb_in)(struct poll_event *); \
    int(*pe_cb_out)(struct poll_event *); \
    int(*pe_cb_error)(struct poll_event *); \
    void(*pe_cb_ondestroy)(struct poll_event *)

struct poll_event {
    poll_event_base;
};

struct poll_event_listener {
    poll_event_base;
};

/*
 * Each poll_event holds pe_appdata context. The poll_event is associated with
 * SSL object which is typically QUIC stream. There are two types of streams
 * in QUIC:
 *    - uni-directional (simplex)
 *    - bi-directional (duplex)
 * bi-directional are easy to handle: we create them, then we read request
 * from client and write reply back. Then stream gets destroyed.
 * This request-reply handling is more tricky with uni-directional streams.
 * We need a pair of streams: server reads a request from one stream and
 * then must create a stream for reply. For echo-reply server we need to
 * pass data we read from input stream to output stream. The poll_event_context
 * here is to do it for us. The echo-reply server handling with simplex
 * (unidirectional) streams goes as follows:
 *    - we read data from input stream and parse request
 *    - then we request poll manager to create an outbound stream,
 *      at this point we also create a response (response_buffer).
 *      the response buffer is added to connection.
 *    - connection keeps list of responses to be dispatched because
 *      client may establish more streams to send more requests
 *    - once outbound stream is created, poll manager moves response
 *      connection to outbound stream.
 */
struct poll_event_context {
    OSSL_LIST_MEMBER(peccx, struct poll_event_context);
    void *peccx;
    void(*peccx_cb_ondestroy)(void *);
};

DEFINE_LIST_OF(pe, struct poll_event);

DEFINE_LIST_OF(peccx, struct poll_event_context);

/*
 * It facilitates transfer of app data from one stream to the other.
 * There are two lists:
 *    - pec_stream_cx for bi-directioanl streams (this list is unused
 *      currently).
 *    - pec_unistream_cx for uni-direcitonal (simplex) streams.
 *
 * Then there are two counters:
 *    - pec_want_stream bumbped up when application requests duplex stream,
 *      bumped down when stream is created
 *    - pec_want_unistream bumped up when application requests simplex stream.
 *      bumped down when stream is created
 */
struct poll_event_connection {
    poll_event_base;
    OSSL_LIST(peccx) pec_stream_cx;
    OSSL_LIST(peccx) pec_unistream_cx;
    uint64_t pec_want_stream;
    uint64_t pec_want_unistream;
};

/*
 * We always allocate more slots than we need. If POLL_GROW slots get
 * deplepted then we allocate POLL_GROW more for the next one.
 * Downsizing is similar. This is very naive and leads to oscillations
 * (where we may end up freeing and reallocating poll event set) we need to
 * figure out better strategy.
 */
#define POLL_GROW 20
#define POLL_DOWNSIZ 20

/*
 * Members in poll manager deserve some explanation:
 *    - pm_head, holds a list of poll_event structures (connections and
 *      streams)
 *    - pm_event_count number of events to monitor in SSL_poll(3ossl)
 *    - pm_poll_set array of events to poll on
 *    - pm_poll_set_sz number of slots (space) available in pm_poll_set
 *    - pm_need_rebuild whenever list of events to monitor in a list changes
 *      poll mamnager must rebuild pm_poll_set
 *    - pm_continue a flag indicates whether event loop should continue to
 *      run or if it should be terminated (pm_continue == 0)
 *    - pm_wconn_in() callback fires when there is a new connection
 *    - pm qconn
 */
struct poll_manager {
    OSSL_LIST(pe) pm_head;
    unsigned int pm_event_count;
    struct poll_event *pm_poll_set;
    unsigned int pm_poll_set_sz;
    int pm_need_rebuild;
    int pm_continue;
};

#define SSL_POLL_ERROR (SSL_POLL_EVENT_F | SSL_POLL_EVENT_EL | \
                        SSL_POLL_EVENT_EC | SSL_POLL_EVENT_ECD | \
                        SSL_POLL_EVENT_ER | SSL_POLL_EVENT_EW)

#define SSL_POLL_IN (SSL_POLL_EVENT_R | SSL_POLL_EVENT_IC | \
                     SSL_POLL_EVENT_ISB | SSL_POLL_EVENT_ISU)

#define SSL_POLL_OUT (SSL_POLL_EVENT_W | SSL_POLL_EVENT_OSB | \
                      SSL_POLL_EVENT_OSU)

struct poll_event_stream {
    poll_event_base;
    struct poll_event_connection *pes_conn;
    char *pes_wpos;
    unsigned int pes_wpos_sz;
    int pes_got_request;
    char pes_reqbuf[8192];
};

/*
 * Response buffer.
 */
enum {
    RB_TYPE_NONE,
    RB_TYPE_TEXT_SIMPLE,
    RB_TYPE_TEXT_FULL
};
#define response_buffer_base \
    unsigned char rb_type; \
    unsigned int rb_rpos; \
    void (*rb_advrpos_cb)(struct response_buffer *, unsigned int);\
    unsigned int (*rb_read_cb)(struct response_buffer *, char *, \
                               unsigned int); \
    int (*rb_eof_cb)(struct response_buffer *); \
    void (*rb_ondestroy_cb)(struct response_buffer *)

struct response_buffer {
    response_buffer_base;
};

struct response_txt_simple {
    response_buffer_base;
    char *rts_pattern;
    unsigned int rts_pattern_len;
    unsigned int rts_len;
};

struct response_txt_full {
    response_buffer_base;
    char rtf_headers[1024];
    char *rtf_pattern;
    unsigned int rtf_pattern_len;
    unsigned int rtf_hdr_len;
    unsigned int rtf_len; /* headers + data */
};

static void destroy_pe(struct poll_event *);
static int pe_return_error(struct poll_event *);
static void pe_return_void(struct poll_event *);

#ifdef _WIN32
static const char *progname;

static void
vwarnx(const char *fmt, va_list ap)
{
    if (progname != NULL)
        fprintf(stderr, "%s: ", progname);
    vfprintf(stderr, fmt, ap);
    putc('\n', stderr);
}

static void
errx(int status, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vwarnx(fmt, ap);
    va_end(ap);
    exit(status);
}

static void
warnx(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vwarnx(fmt, ap);
    va_end(ap);
}

/*
 * we can get away with this mock-up on windows.
 * we generate payload for any URL we obtain in
 * GET request. mock-up is good enough for us.
 */
static char *
basename(char *path)
{
    return path;
}

# define strncasecmp(_a_, _b_, _c_) _strnicmp((_a_), (_b_), (_c_))

# define ctime_r(_t_, _b_) ctime_s((_b_), sizeof ((_b_)), (_t_))

#endif

enum pe_types {
    PE_NONE,
    PE_LISTENER,
    PE_CONNECTION,
    PE_STREAM,
    PE_STREAM_UNI_IN,
    PE_STREAM_UNI_OUT,
    PE_INVALID
};

static struct response_txt_simple *
rb_to_txt_simple(struct response_buffer *rb)
{
    if (rb == NULL || rb->rb_type != RB_TYPE_TEXT_SIMPLE)
        return NULL;

    return (struct response_txt_simple *)rb;
}

static struct response_txt_full *
rb_to_txt_full(struct response_buffer *rb)
{
    if (rb == NULL || rb->rb_type != RB_TYPE_TEXT_FULL)
        return NULL;

    return (struct response_txt_full *)rb;
}

static void
rb_advrpos_cb(struct response_buffer *rb, unsigned int rpos)
{
    /* we assume base response_buffer is unlimited */
    rb->rb_rpos += rpos;
}

static void
rb_ondestroy_cb(struct response_buffer *rb)
{
    OPENSSL_free(rb);
}

static unsigned int
rb_null_read_cb(struct response_buffer *rb, char *buf, unsigned int buf_sz)
{
    return 0;
}

static int
rb_eof_cb(struct response_buffer *rb)
{
    return 1;
}

static void
rb_init(struct response_buffer *rb)
{
    rb->rb_type = RB_TYPE_NONE;
    rb->rb_advrpos_cb = rb_advrpos_cb;
    rb->rb_read_cb = rb_null_read_cb;
    rb->rb_eof_cb = rb_eof_cb;
    rb->rb_ondestroy_cb = rb_ondestroy_cb;
    rb->rb_rpos = 0;
}

static void
rb_advrpos(struct response_buffer *rb, unsigned int rpos)
{
    if (rb != NULL)
        rb->rb_advrpos_cb(rb, rpos);
}

static unsigned int
rb_read(struct response_buffer *rb, char *buf, unsigned int buf_sz)
{
    if (rb != NULL)
        return rb->rb_read_cb(rb, buf, buf_sz);
    else
        return 0;
}

static unsigned int
rb_eof(struct response_buffer *rb)
{
    if (rb != NULL)
        return rb->rb_eof_cb(rb);
    else
        return 1;
}

static void
rb_destroy(struct response_buffer *rb)
{
    if (rb != NULL)
        rb->rb_ondestroy_cb(rb);
}

static int
rb_txt_simple_eof_cb(struct response_buffer *rb)
{
    struct response_txt_simple *rts = rb_to_txt_simple(rb);

    if (rb_to_txt_full(rb) == NULL)
        return 1;

    if (rb->rb_rpos >= rts->rts_len)
        return 1;
    else
        return 0;
}

static unsigned int
rb_txt_simple_read_cb(struct response_buffer *rb, char *buf,
                      unsigned int buf_sz)
{
    struct response_txt_simple *rts = rb_to_txt_simple(rb);
    unsigned int i = rb->rb_rpos;
    unsigned int rv = 0;

    if (rts == NULL || rb_eof(rb))
        return 0;

    while ((i < rts->rts_len) && (rv < buf_sz)) {
        *buf++ = rts->rts_pattern[i % rts->rts_pattern_len];
        i++;
        rv++;
    }

    return rv;
}

static void
rb_txt_simple_ondestroy_cb(struct response_buffer *rb)
{
    struct response_txt_simple *rts = rb_to_txt_simple(rb);

    if (rts != NULL) {
        OPENSSL_free(rts->rts_pattern);
        OPENSSL_free(rts);
    }
}

static void
rb_txt_simple_advrpos_cb(struct response_buffer *rb, unsigned int sz)
{
    struct response_txt_simple *rts = rb_to_txt_simple(rb);

    if (rts != NULL) {
        rb->rb_rpos += sz;
        if (rb->rb_rpos >= rts->rts_len)
            rb->rb_rpos = rts->rts_len;
    }
}

static ossl_unused struct response_txt_simple *
new_txt_simple_respoonse(const char *fill_pattern, unsigned int fsize)
{
    struct response_txt_simple *rts;
    struct response_buffer *rb;

    rts = OPENSSL_malloc(sizeof (struct response_txt_simple));
    if (rts == NULL)
        return NULL;

    if ((rts->rts_pattern = OPENSSL_strdup(fill_pattern)) == NULL) {
        OPENSSL_free(rts);
        return NULL;
    }
    rts->rts_pattern_len = (unsigned int)strlen(fill_pattern);
    rts->rts_len = fsize;

    rb = (struct response_buffer *)rts;
    rb_init(rb);
    rb->rb_type = RB_TYPE_TEXT_SIMPLE;
    rb->rb_eof_cb = rb_txt_simple_eof_cb;
    rb->rb_read_cb = rb_txt_simple_read_cb;
    rb->rb_ondestroy_cb = rb_txt_simple_ondestroy_cb;
    rb->rb_advrpos_cb = rb_txt_simple_advrpos_cb;

    return rts;
}

static int
rb_txt_full_eof_cb(struct response_buffer *rb)
{
    struct response_txt_full *rtf = rb_to_txt_full(rb);

    if (rtf == NULL)
        return 1;

    if (rb->rb_rpos >= rtf->rtf_len)
        return 1;
    else
        return 0;
}

static void
rb_txt_full_ondestroy_cb(struct response_buffer *rb)
{
    struct response_txt_full *rtf = rb_to_txt_full(rb);

    if (rtf != NULL) {
        OPENSSL_free(rtf->rtf_pattern);
        OPENSSL_free(rtf);
    }
}

static unsigned int
rb_txt_full_read_cb(struct response_buffer *rb, char *buf, unsigned int buf_sz)
{
    struct response_txt_full *rtf = rb_to_txt_full(rb);
    unsigned int i = rb->rb_rpos;
    unsigned int j;
    unsigned int rv = 0;

    if (rtf == NULL || rb_eof(rb))
        return 0;

    while ((i < rtf->rtf_hdr_len) && (rv < buf_sz)) {
        *buf++ = rtf->rtf_headers[i++];
        rv++;
    }

    j = i - rtf->rtf_hdr_len;
    while ((i < rtf->rtf_len) && (rv < buf_sz)) {
        *buf++ = rtf->rtf_pattern[j % rtf->rtf_pattern_len];
        j++;
        i++;
        rv++;
    }

    return rv;
}

static void
rb_txt_full_advrpos_cb(struct response_buffer *rb, unsigned int sz)
{
    struct response_txt_full *rtf = rb_to_txt_full(rb);

    if (rtf != NULL) {
        rb->rb_rpos += sz;
        if (rb->rb_rpos >= rtf->rtf_len)
            rb->rb_rpos = rtf->rtf_len;
    }
}

static struct response_txt_full *
new_txt_full_respoonse(const char *fill_pattern, unsigned int fsize)
{
    struct response_txt_full *rtf;
    struct response_buffer *rb;
    char date_str[80];
    int hlen;
    time_t t;

    rtf = OPENSSL_malloc(sizeof (struct response_txt_full));
    if (rtf == NULL)
        return NULL;

    if ((rtf->rtf_pattern = OPENSSL_strdup(fill_pattern)) == NULL) {
        OPENSSL_free(rtf);
        return NULL;
    }
    rtf->rtf_pattern_len = (unsigned int)strlen(fill_pattern);

    t = time(&t);
    ctime_r(&t, date_str);
    /* TODO check headers if they confirm to HTTP/1.0 */
    hlen = snprintf(rtf->rtf_headers, sizeof (rtf->rtf_headers),
                    "HTTP/1.0 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %u\r\n"
                    "Date: %s\r\n"
                    "\r\n", fsize, date_str);
    if (hlen >= (int)sizeof (rtf->rtf_headers)) {
        OPENSSL_free(rtf->rtf_pattern);
        OPENSSL_free(rtf);
        return NULL;
    }
    rtf->rtf_hdr_len = (unsigned int)hlen;

    rtf->rtf_len = rtf->rtf_hdr_len + fsize;

    rb = (struct response_buffer *)rtf;
    rb_init(rb);
    rb->rb_type = RB_TYPE_TEXT_FULL;
    rb->rb_eof_cb = rb_txt_full_eof_cb;
    rb->rb_read_cb = rb_txt_full_read_cb;
    rb->rb_ondestroy_cb = rb_txt_full_ondestroy_cb;
    rb->rb_advrpos_cb = rb_txt_full_advrpos_cb;

    return rtf;
}

static ossl_unused const char *
pe_type_to_name(const struct poll_event *pe)
{
    static const char *names[] = {
        "none",
        "listener",
        "connection",
        "stream (bidi)",
        "stream (in)",
        "stream (out)",
        "invalid"
    };

    if (pe->pe_type >= PE_INVALID)
        return (names[PE_INVALID]);

    return names[pe->pe_type];
}

static struct poll_event_connection *
pe_to_connection(struct poll_event *pe)
{
    if ((pe == NULL) || (pe->pe_type != PE_CONNECTION))
        return NULL;

    return ((struct poll_event_connection *)pe);
}

static void
init_pe(struct poll_event *pe, SSL *ssl)
{
    pe->pe_poll_item.desc = SSL_as_poll_descriptor(ssl);
    pe->pe_cb_in = pe_return_error;
    pe->pe_cb_out = pe_return_error;
    pe->pe_cb_error = pe_return_error;
    pe->pe_cb_ondestroy = pe_return_void;
    pe->pe_self = pe;
    pe->pe_type = PE_NONE;
    pe->pe_want_mask = ~0;
}

static struct poll_event *
new_pe(SSL *ssl)
{
    struct poll_event *pe;

    if (ssl == NULL)
        return NULL;

    pe = OPENSSL_zalloc(sizeof (struct poll_event));
    if (pe != NULL)
        init_pe(pe, ssl);

    return pe;
}

static struct poll_event_listener *
new_listener_pe(SSL *ssl_listener)
{
    struct poll_event *listener_pe = new_pe(ssl_listener);

    if (listener_pe != NULL) {
        listener_pe->pe_type = PE_LISTENER;
        listener_pe->pe_want_events = SSL_POLL_EVENT_IC | SSL_POLL_EVENT_EL;
    }

    return (struct poll_event_listener *)listener_pe;
}

static struct poll_event *
new_qconn_pe(SSL *ssl_qconn)
{
    struct poll_event *qconn_pe;
    struct poll_event_connection *pec;

    qconn_pe = OPENSSL_zalloc(sizeof (struct poll_event_connection));

    if (qconn_pe != NULL) {
        init_pe(qconn_pe, ssl_qconn);
        qconn_pe->pe_type = PE_CONNECTION;
        qconn_pe->pe_want_events = SSL_POLL_EVENT_ISB | SSL_POLL_EVENT_ISU;
        qconn_pe->pe_want_events |= SSL_POLL_EVENT_EC | SSL_POLL_EVENT_ECD;
        /*
         * SSL_POLL_EVENT_OSB (or SSL_POLL_EVENT_OSU) must be monitored once
         * there is a request for outbound stream created by app.
         */
        pec = (struct poll_event_connection *)qconn_pe;
        ossl_list_peccx_init(&pec->pec_unistream_cx);
        ossl_list_peccx_init(&pec->pec_stream_cx);
    }

    return qconn_pe;
}

static struct poll_event_stream *
new_stream_pe(SSL *ssl_qs)
{
    struct poll_event_stream *pes;

    pes = OPENSSL_zalloc(sizeof (struct poll_event_stream));

    if (pes != NULL) {
        init_pe((struct poll_event *)pes, ssl_qs);
        pes->pes_wpos = pes->pes_reqbuf;
        pes->pes_wpos_sz = sizeof (pes->pes_reqbuf) - 1;
    }

    return (pes);
}

static SSL *
get_ssl_from_pe(struct poll_event *pe)
{
    SSL *ssl = NULL;

    if (pe != NULL)
        ssl = pe->pe_poll_item.desc.value.ssl;

    return ssl;
}

static void
pe_pause_read(struct poll_event *pe)
{
    pe->pe_want_events &= ~SSL_POLL_EVENT_R;
    pe->pe_my_pm->pm_need_rebuild = 1;
}

static void
pe_resume_read(struct poll_event *pe)
{
    pe->pe_want_events |= (SSL_POLL_EVENT_R & pe->pe_want_mask);
    pe->pe_my_pm->pm_need_rebuild = 1;
}

static void
pe_pause_write(struct poll_event *pe)
{
    pe->pe_want_events &= ~SSL_POLL_EVENT_W;
    pe->pe_my_pm->pm_need_rebuild = 1;
}

static void
pe_resume_write(struct poll_event *pe)
{
    pe->pe_want_events |= (SSL_POLL_EVENT_W & pe->pe_want_mask);
    pe->pe_my_pm->pm_need_rebuild = 1;
}

/*
 * like pause, but is permanent,
 */
static void
pe_disable_read(struct poll_event *pe)
{
    pe_pause_read(pe);
    pe->pe_want_mask &= ~SSL_POLL_EVENT_R;
}

static void
pe_disable_write(struct poll_event *pe)
{
    pe_pause_write(pe);
    pe->pe_want_mask &= ~SSL_POLL_EVENT_W;
}

/*
 * handle_ssl_error() diagnoses error from SSL/QUIC stack and
 * decides if it is temporal error (in that case it returns zero)
 * or error is permanent. In case of permanent error the
 * poll event pe should be removed from poll manager and destroyed.
 */
static ossl_unused const char *
err_str_n(unsigned long e, char *buf, size_t buf_sz)
{
    ERR_error_string_n(e, buf, buf_sz);
    return buf;
}

static int
handle_ssl_error(struct poll_event *pe, int rc, const char *caller)
{
    SSL *ssl = get_ssl_from_pe(pe);
    int ssl_error, rv;
#ifdef DEBUG
    char err_str[120];
#endif

    /* may be we should use SSL_shutdown_ex() to signal peer what's going on */
    ssl_error = SSL_get_error(ssl, rc);
    if (rc <= 0) {
        switch (ssl_error) {
        case SSL_ERROR_SYSCALL:
        case SSL_ERROR_SSL:
            DPRINTF(stderr, "%s permanent error on %p (%s) [ %s ]\n",
                    caller, pe, pe_type_to_name(pe),
                    err_str_n(ssl_error, err_str, sizeof (err_str)));
            rv = -1;
            break;
        case SSL_ERROR_ZERO_RETURN:
        default:
            DPRINTF(stderr, "%s temporal error on %p (%s) [ %s ]\n",
                    caller, pe, pe_type_to_name(pe),
                    err_str_n(ssl_error, err_str, sizeof (err_str)));
            rv = 0; /* maybe return -1 here too */
        }
    } else if (rc == 0) {
        DPRINTF(stderr, "%s temporal error on  %p (%s) [ %s ]\n",
                caller, pe, pe_type_to_name(pe),
                err_str_n(ssl_error, err_str, sizeof (err_str)));
        rv = 0;
    } else if (rc == 1) {
        DPRINTF(stderr, "%s no error on %p (%s) [ ??? ]\n", caller, pe,
                pe_type_to_name(pe));
        rv = -1; /* complete, stop polling for event */
    } else {
        DPRINTF(stderr, "%s ?unexpected? error on %p (%s) [ %s ]\n",
                caller, pe, pe_type_to_name(pe),
                err_str_n(ssl_error, err_str, sizeof (err_str)));
        rv = -1; /* stop polling */
    }

    return rv;
}

static ossl_unused const char *
stream_state_str(int stream_state)
{
    const char *rv;

    switch (stream_state) {
    case SSL_STREAM_STATE_NONE:
        rv = "SSL_STREAM_STATE_NONE";
        break;
    case SSL_STREAM_STATE_OK:
        rv = "SSL_STREAM_STATE_OK";
        break;
    case SSL_STREAM_STATE_WRONG_DIR:
        rv = "SSL_STREAM_STATE_WRONG_DIR";
        break;
    case SSL_STREAM_STATE_FINISHED:
        rv = "SSL_STREAM_STATE_FINISHED";
        break;
    case SSL_STREAM_STATE_RESET_LOCAL:
        rv = "SSL_STREAM_STATE_RESET_LOCAL";
        break;
    case SSL_STREAM_STATE_RESET_REMOTE:
        rv = "SSL_STREAM_STATE_RESET_REMOTE";
        break;
    case SSL_STREAM_STATE_CONN_CLOSED:
        rv = "SSL_STREAM_STATE_CONN_CLOSED";
        break;
    default:
        rv = "???";
    }

    return rv;
}

static int
handle_read_stream_state(struct poll_event *pe)
{
    int stream_state = SSL_get_stream_read_state(get_ssl_from_pe(pe));
    int rv;

    switch (stream_state) {
    case SSL_STREAM_STATE_FINISHED:
        DPRINTF(stderr, "%s remote peer concluded the stream\n", __func__);
        pe_disable_read(pe);
        /* FALLTHRU */
    case SSL_STREAM_STATE_OK:
        rv = 0;
        break;
    default:
        DPRINTF(stderr,
                "%s error %s on stream, the %p (%s) should be destroyed\n",
                __func__, stream_state_str(stream_state), pe,
                pe_type_to_name(pe));
        rv = -1;
    }

    return rv;
}

static int
handle_write_stream_state(struct poll_event *pe)
{
    int state = SSL_get_stream_write_state(get_ssl_from_pe(pe));
    int rv;

    switch (state) {
    case SSL_STREAM_STATE_FINISHED:
        DPRINTF(stderr, "%s remote peer concluded the stream\n", __func__);
        /* FALLTHRU */
    case SSL_STREAM_STATE_OK:
        rv = 0;
        break;
    default:
        DPRINTF(stderr,
                "%s error %s on stream, the %p (%s) should be destroyed\n",
                __func__, stream_state_str(state), pe, pe_type_to_name(pe));
        rv = -1;
    }

    return rv;
}

static void
add_pe_to_pm(struct poll_manager *pm, struct poll_event *pe)
{
    if (pe->pe_my_pm == NULL) {
        ossl_list_pe_insert_head(&pm->pm_head, pe);
        pm->pm_need_rebuild = 1;
        pe->pe_my_pm = pm;
    }
}

static void
remove_pe_from_pm(struct poll_manager *pm, struct poll_event *pe)
{
    if (pe->pe_my_pm == pm) {
        ossl_list_pe_remove(&pm->pm_head, pe);
        pm->pm_need_rebuild = 1;
        pe->pe_my_pm = NULL;
    }
}

static struct poll_manager *
create_poll_manager(void)
{
    struct poll_manager *pm = NULL;

    pm = OPENSSL_zalloc(sizeof (struct poll_manager));
    if (pm == NULL)
        return NULL;

    ossl_list_pe_init(&pm->pm_head);
    pm->pm_poll_set = OPENSSL_malloc_array(POLL_GROW,
                                           sizeof (struct poll_event));
    if (pm->pm_poll_set != NULL) {
        pm->pm_poll_set_sz = POLL_GROW;
        pm->pm_event_count = 0;
    } else {
        OPENSSL_free(pm);
        return NULL;
    }

    return pm;
}

static int
rebuild_poll_set(struct poll_manager *pm)
{
    struct poll_event *new_poll_set;
    struct poll_event *pe;
    size_t pe_num;
    size_t i;

    if (pm->pm_need_rebuild == 0)
        return 0;

    pe_num = ossl_list_pe_num(&pm->pm_head);
    if (pe_num > pm->pm_poll_set_sz) {
        /*
         * grow poll set by POLL_GROW
         */
        new_poll_set = OPENSSL_realloc_array(pm->pm_poll_set,
                                             pm->pm_poll_set_sz + POLL_GROW,
                                             sizeof (struct poll_event));
        if (new_poll_set == NULL)
            return -1;
        pm->pm_poll_set = new_poll_set;
        pm->pm_poll_set_sz += POLL_GROW;

    } else if ((pe_num + POLL_DOWNSIZ) < pm->pm_poll_set_sz) {
        /*
         * shrink poll set by POLL_DOWNSIZ
         */
        new_poll_set = OPENSSL_realloc_array(pm->pm_poll_set,
                                             pm->pm_poll_set_sz - POLL_DOWNSIZ,
                                             sizeof (struct poll_event));
        if (new_poll_set == NULL)
            return -1;
        pm->pm_poll_set = new_poll_set;
        pm->pm_poll_set_sz -= POLL_GROW;
    }

    i = 0;
    DPRINTF(stderr, "%s there %zu events to poll\n", __func__,
            ossl_list_pe_num(&pm->pm_head));
    OSSL_LIST_FOREACH(pe, pe, &pm->pm_head) {
        pe->pe_poll_item.events = pe->pe_want_events;
        pm->pm_poll_set[i++] = *pe;
        DPRINTF(stderr, "\t%p (%s) " POLL_FMT " (disabled: " POLL_FMT ")\n",
                pe, pe_type_to_name(pe),
                POLL_PRINTA(pe->pe_poll_item.events),
                POLL_PRINTA(~pe->pe_want_mask));
    }
    pm->pm_event_count = (unsigned int)i;
    pm->pm_need_rebuild = 0;

    return 0;
}

static void
destroy_poll_manager(struct poll_manager *pm)
{
    struct poll_event *pe, *pe_safe;

    if (pm == NULL)
        return;

    OSSL_LIST_FOREACH_DELSAFE(pe, pe_safe, pe, &pm->pm_head)
        destroy_pe(pe);

    OPENSSL_free(pm->pm_poll_set);
    OPENSSL_free(pm);
}

static void
destroy_pe(struct poll_event *pe)
{
    SSL *ssl;

    if (pe == NULL)
        return;

    DPRINTF(stderr, "%s %p (%s)\n", __func__, pe, pe_type_to_name(pe));
    ssl = get_ssl_from_pe(pe);
    if (pe->pe_my_pm)
        remove_pe_from_pm(pe->pe_my_pm, pe);

    pe->pe_cb_ondestroy(pe);

    OPENSSL_free(pe);

    SSL_free(ssl);
}

static int
pe_return_error(struct poll_event *pe)
{
    return -1;
}

static void
pe_return_void(struct poll_event *ctx)
{
    return;
}

static int
pe_handle_listener_error(struct poll_event *pe)
{
    pe->pe_my_pm->pm_continue = 0;
    if (pe->pe_poll_item.revents & SSL_POLL_EVENT_EL)
        return -1;

    DPRINTF(stderr, "%s unexpected error on %p (%s) " POLL_FMT "\n", __func__,
            pe, pe_type_to_name(pe), POLL_PRINTA(pe->pe_poll_item.revents));

    return -1;
}

static struct poll_event_stream *
pe_to_stream(struct poll_event *pe)
{
    switch (pe->pe_type) {
    case PE_STREAM:
    case PE_STREAM_UNI_IN:
    case PE_STREAM_UNI_OUT:
        return ((struct poll_event_stream *)pe);
    default:
        return NULL;
    }
}

/*
 * non-blocking variant for new_stream(). Creating outbound stream
 * is two step process when using non-blocking I/O.
 *    application starts polling for SSL_POLL_EVENT_OS* to check
 *    if outbound streams are available.
 *
 *    as soon as SSL_POLL_EVENT_OS comes back from SSL_poll() application
 *    should call SSL-new_stream() to create a stream object and
 *    add its poll descriptor to SSL_poll() events. The stream object
 *    should be monitored for SSL_POLL_EVENT_{W,R}
 *
 * new_stream() function below is supposed to be called by application
 * which uses SSL_poll()  to manage I/O. We expect there might be more
 * than 1 stream request.
 */
static int
request_new_stream(struct poll_event_connection *pec, uint64_t qsflag,
                   void *peccx_arg)
{
    struct poll_event_context *peccx;
    struct poll_event *qconn_pe = (struct poll_event *)pec;

    if (peccx_arg == NULL)
        return -1;

    peccx = OPENSSL_malloc(sizeof (struct poll_event_context));
    if (peccx == NULL)
        return -1;
    peccx->peccx = peccx_arg;

    if (qsflag & SSL_STREAM_FLAG_UNI) {
        pec->pec_want_unistream++;
        qconn_pe->pe_want_events |= SSL_POLL_EVENT_OSU;
        ossl_list_peccx_insert_tail(&pec->pec_unistream_cx, peccx);
    } else {
        pec->pec_want_stream++;
        qconn_pe->pe_want_events |= SSL_POLL_EVENT_OSB;
        ossl_list_peccx_insert_tail(&pec->pec_stream_cx, peccx);
    }

    /*
     * We are changing poll events, so SSL_poll() array needs be rebuilt.
     */
    qconn_pe->pe_my_pm->pm_need_rebuild = 1;

    return 0;
}

static void *
get_response_from_pec(struct poll_event_connection *pec, int stype)
{
    struct poll_event_context *peccx;
    void *rv;

    switch (stype) {
    case PE_STREAM_UNI_OUT:
        peccx = ossl_list_peccx_head(&pec->pec_unistream_cx);
        if (peccx != NULL) {
            pec->pec_want_unistream--;
            ossl_list_peccx_remove(&pec->pec_unistream_cx, peccx);
            rv = peccx->peccx;
            OPENSSL_free(peccx);
        } else {
            rv = NULL;
        }
        break;
    case PE_STREAM:
        peccx = ossl_list_peccx_head(&pec->pec_stream_cx);
        if (peccx != NULL) {
            pec->pec_want_stream--;
            ossl_list_peccx_remove(&pec->pec_stream_cx, peccx);
            rv = peccx->peccx;
            OPENSSL_free(peccx);
        } else {
            rv = NULL;
        }
        break;
    default:
        rv = NULL;
    }

    return rv;
}

static int
app_handle_stream_error(struct poll_event *pe)
{
    int rv = 0;

    if (pe->pe_poll_item.revents & SSL_POLL_EVENT_ER) {

        if ((pe->pe_poll_item.events & SSL_POLL_EVENT_R) == 0) {
            DPRINTF(stderr, "%s unexpected failure on reader %p (%s) "
                    POLL_FMT "\n", __func__, pe, pe_type_to_name(pe),
                    POLL_PRINTA(pe->pe_poll_item.revents));
        }

        (void) handle_read_stream_state(pe);
        rv = -1; /* tell pm to stop polling and destroy stream/event */
    } else if (pe->pe_poll_item.revents & SSL_POLL_EVENT_EW) {

        if ((pe->pe_poll_item.events & SSL_POLL_EVENT_W) == 0) {
            DPRINTF(stderr, "%s unexpected failure on writer %p (%s) "
                    POLL_FMT "\n", __func__, pe, pe_type_to_name(pe),
                    POLL_PRINTA(pe->pe_poll_item.revents));
        }
        (void) handle_write_stream_state(pe);

        rv = -1; /* tell pm to stop polling and destroy stream/event */
    } else {
        DPRINTF(stderr, "%s unexpected failure on writer/reader %p (%s) "
                POLL_FMT "\n", __func__, pe, pe_type_to_name(pe),
                POLL_PRINTA(pe->pe_poll_item.revents));
        rv = -1; /* tell pm to stop polling and destroy stream/event */
    }

    return rv;
}

/*
 * app_write_cb() callback notifies application the QUIC stack
 * is ready to send data. The write callback attempts to process
 * all buffers in write queue.
 * if write queue becomes empty, stream is concluded.
 */
static int
app_write_cb(struct poll_event *pe)
{
    struct response_buffer *rb = (struct response_buffer *)pe->pe_appdata;
    char buf[4096];
    size_t written;
    unsigned int wlen;
    int rv;

    if (rb == NULL) {
        DPRINTF(stderr, "%s no response buffer\n", __func__);
        return -1;
    }

    wlen = rb_read(rb, buf, sizeof (buf));
    if (wlen == 0) {
        DPRINTF(stderr, "%s no more data to write to %p (%s)\n", __func__,
                pe, pe_type_to_name(pe));
        rv = SSL_stream_conclude(get_ssl_from_pe(pe), 0);
        pe_disable_write(pe);
        /*
         * we deliberately override return value of SSL_stream_conclude() here
         * to keep CI build happy. -1 means we are going to kill poll event
         * anyway.
         *
         * another option would be to return 0 and let poll manager wait
         * for confirmation of FIN packet sent on behalf of
         * SSL_stream_conclude(). At the moment it does not seem necessary.
         * More details can be found here:
         *     https://github.com/openssl/project/issues/1160
         */
        rv = -1;
    } else {
        rv = SSL_write_ex(get_ssl_from_pe(pe), buf, wlen, &written);
        if (rv == 1) {
            rb_advrpos(rb, (unsigned int)written);
            rv = 0;
        } else {
            rv = handle_ssl_error(pe, rv, __func__);
        }
    }

    return rv;
}

static int
app_setup_response(struct poll_event_stream *pes)
{
    struct poll_event *pe = (struct poll_event *)pes;
    int rv;

    switch (pe->pe_type) {
    case PE_STREAM_UNI_IN:
        rv = request_new_stream(pes->pes_conn, SSL_STREAM_FLAG_UNI,
                                pe->pe_appdata);
        break;
    case PE_STREAM:
        pe->pe_cb_out = app_write_cb;
        rv = 0;
        pe_resume_write(pe);
        break;
    default:
        rv = -1;
    }

    return rv;
}

static unsigned int
get_fsize(const char *file_name)
{
    const char *digit = file_name;
    unsigned int fsize;

    /* any number we find in filename is desired size */
    fsize = 0;

    while (*digit && !isdigit((int)*digit))
        digit++;

    while (*digit && isdigit((int)*digit)) {
        fsize = fsize * 10;
        fsize = fsize + (*digit - 0x30);
        digit++;
    }

    if (fsize == 0)
        fsize = 12345; /* ? may be random ? */

    return fsize;
}

static int
parse_request(struct poll_event_stream *pes)
{
    const char *pos = pes->pes_reqbuf;
    char file_name_buf[4096];
    char *dst = file_name_buf;
    char *end = &file_name_buf[4096];
    char *file_name;
    struct poll_event *pe = (struct poll_event *)pes;
    int rv;

    /* got request already */
    if (pe->pe_appdata != NULL)
        return -1;

    while (*pos && isspace((int)*pos))
        pos++;

    if (strncasecmp(pos, "GET", 3) != 0)
        return -1; /* this will reset the stream */
    pos += 3;

    while (*pos && isspace((int)*pos))
        pos++;

    if (*pos != '/')
        return -1; /* this will reset the stream */

    /* strip leading slashes */
    while (*pos == '/')
        pos++;

    while ((isalnum((int)*pos) || ispunct((int)*pos)) && (dst < end))
        *dst++ = *pos++;
    if (dst == end)
        dst--;
    *dst = '\0';
    /*
     * if request is something like 'GET / HTTP/1.0...' we assume /index.html
     * otherwise take the last component
     */
    if (file_name_buf[0] == '\0') {
        file_name = "index.html";
    } else {
        file_name = basename(file_name_buf);
        /*
         * I'm not sure what happens when file_name_buf looks for example
         * like that: /foo/bar/nothing/
         * (the basename component is missing/is empty).
         */
        if (file_name == NULL || *file_name == '\0')
            file_name = "foo";
    }

    assert(pe->pe_appdata == NULL);
    pe->pe_appdata = new_txt_full_respoonse(file_name, get_fsize(file_name));

    if (pe->pe_appdata == NULL)
        rv = -1;
    else
        rv = app_setup_response(pes);

    return rv;
}

static int
wrap_around(struct poll_event_stream *pes)
{
    int rv = 0;

    /* we can wrap the buffer iff we got request */
    if (pes->pes_wpos_sz == 0) {
        if (((struct poll_event *)pes)->pe_appdata != NULL) {
            pes->pes_wpos = pes->pes_reqbuf;
            pes->pes_wpos_sz = sizeof (pes->pes_reqbuf) - 1;
        } else {
            rv = -1;
        }
    }

    return rv;
}

/*
 * app_read_cb() callback notifies application there are data
 * waiting to be read from stream. The callback allocates
 * new linked buffer and reads data from stream to newly allocated
 * buffer. It then uses request_write() to put the buffer to write
 * queue so data can be echoed back to client.
 */
static int
app_read_cb(struct poll_event *pe)
{
    struct poll_event_stream *pes = pe_to_stream(pe);
    size_t read_len;
    int rv;

    if (pes == NULL)
        return -1;

    /*
     * if we could not parse the request in the first chunk (8k), then just
     * fail the stream with reset. If we got request then finish reading
     * data from client.
     */
    if (wrap_around(pes) == -1)
        return -1;

    rv = SSL_read_ex(get_ssl_from_pe(pe), pes->pes_wpos, pes->pes_wpos_sz,
                     &read_len);
    if (rv == 0) {
        pe_disable_read(pe);
        /*
         * May be it's over cautious, we should just examine stream state and
         * decide if we can continue with poll (rv == 0) or we should stop
         * polling (rv == -1).
         */
        rv = handle_ssl_error(pe, rv, __func__);
        if (rv == 0)
            rv = handle_read_stream_state(pe);
        return rv;
    }
    pes->pes_wpos += read_len;
    pes->pes_wpos_sz -= (unsigned int)read_len;

    rv = parse_request(pes);

    return rv;
}

static void
app_ondestroy_cb(struct poll_event *pe)
{
    rb_destroy((struct response_buffer *)pe->pe_appdata);
}

/*
 * create new outbound stream
 */
static int
app_new_stream_cb(struct poll_event *qconn_pe)
{
    SSL *qconn;
    SSL *qs;
    struct poll_event_connection *pec;
    struct poll_event *qs_pe;
    struct poll_event_stream *pes;
    int rv = 0;

    assert(qconn_pe->pe_poll_item.revents & SSL_POLL_EVENT_OS);
    pec = pe_to_connection(qconn_pe);
    assert(pec != NULL);

    qconn = get_ssl_from_pe(qconn_pe);

    if (qconn_pe->pe_poll_item.revents & SSL_POLL_EVENT_OSU)
        qs = SSL_new_stream(qconn, SSL_STREAM_FLAG_UNI);
    else
        qs = SSL_new_stream(qconn, 0);
    if (qs == NULL)
        return -1;

    pes = new_stream_pe(qs);
    qs_pe = (struct poll_event *)pes;
    if (qconn_pe != NULL) {
        qs_pe->pe_cb_error = app_handle_stream_error;
        qs_pe->pe_cb_out = app_write_cb; /* unidirectional stream is outbound */
        qs_pe->pe_cb_ondestroy = app_ondestroy_cb;
        qs_pe->pe_want_events = SSL_POLL_EVENT_EW;

        if (qconn_pe->pe_poll_item.revents & SSL_POLL_EVENT_OSU) {
            qs_pe->pe_type = PE_STREAM_UNI_OUT;
        } else if (qconn_pe->pe_poll_item.revents & SSL_POLL_EVENT_OSB) {
            /* we will enable read side for bi-directional stream */
            qs_pe->pe_type = PE_STREAM;
            qs_pe->pe_cb_out = app_read_cb;
            qs_pe->pe_want_events = SSL_POLL_EVENT_ER;
        }

        qs_pe->pe_appdata = get_response_from_pec(pec, qs_pe->pe_type);
        if (qs_pe->pe_appdata == NULL) {
            rv = -1;
            destroy_pe(qs_pe);
        } else {
            add_pe_to_pm(qconn_pe->pe_my_pm, qs_pe);
            pe_resume_write(qs_pe);
            /* enable read side on bidirectional outbound streams */
            if (qconn_pe->pe_poll_item.revents & SSL_POLL_EVENT_OSB)
                pe_resume_read(qs_pe);
        }
    } else {
        SSL_free(qs);
        rv = -1;
    }

    return rv;
}

static int
app_handle_qconn_error(struct poll_event *pe)
{
    int rv = -2;

    if (pe->pe_poll_item.revents & SSL_POLL_EVENT_EC) {
        DPRINTF(stderr,
                "%s connection shutdown started on %p (%s), keep polling\n",
                __func__, pe, pe_type_to_name(pe));
        /*
         * shutdown has started, Not sure what we should be doing here.
         * So the plan is to call SSL_shutdown() here and stop monitoring
         * _EVENT_EC here. We will keep _EVENT_ECD monitored.
         * Shall we call shutdown too?
         */
        SSL_shutdown(get_ssl_from_pe(pe));
        /*
         * adjust _want_events, don't forget to ask poll manager to rebuild
         * poll set so _want_events can take effect in next loop iteration
         */
        pe->pe_want_events &= ~SSL_POLL_EVENT_EC;
        pe->pe_my_pm->pm_need_rebuild = 1;
        rv = 0;
    }

    if (pe->pe_poll_item.revents & SSL_POLL_EVENT_ECD) {
        DPRINTF(stderr,
                "%s connection shutdown done on %p (%s), stop polling\n",
                __func__, pe, pe_type_to_name(pe));
        rv = -1; /* shutdown is complete stop polling let pe to be destroyed */
    }

    if (rv == -2) {
        DPRINTF(stderr, "%s unexpected event on %p (%s)" POLL_FMT "\n",
                __func__, pe, pe_type_to_name(pe),
                POLL_PRINTA(pe->pe_poll_item.revents));
        rv = -1;
    }

    return rv;
}

/*
 * accept stream from remote peer
 */
static int
app_accept_stream_cb(struct poll_event *qconn_pe)
{
    SSL *qconn;
    SSL *qs;
    struct poll_event *qs_pe;
    int rv = 0;
#ifdef DEBUG
    struct poll_event_connection *pec;

    assert(qconn_pe->pe_poll_item.revents & SSL_POLL_EVENT_IS);
    pec = pe_to_connection(qconn_pe);
    assert(pec != NULL);
#endif

    qconn = get_ssl_from_pe(qconn_pe);

    if (qconn_pe->pe_poll_item.revents & SSL_POLL_EVENT_ISU)
        qs = SSL_accept_stream(qconn, SSL_STREAM_FLAG_UNI);
    else
        qs = SSL_accept_stream(qconn, SSL_STREAM_FLAG_UNI);
    if (qs == NULL)
        return -1;

    qs_pe = (struct poll_event *)new_stream_pe(qs);
    if (qs_pe != NULL) {
        qs_pe->pe_cb_error = app_handle_stream_error;
        qs_pe->pe_cb_in = app_read_cb;
        qs_pe->pe_cb_ondestroy = app_ondestroy_cb;
        qs_pe->pe_want_events = SSL_POLL_EVENT_ER;
        add_pe_to_pm(qconn_pe->pe_my_pm, qs_pe);

        if (qconn_pe->pe_poll_item.revents & SSL_POLL_EVENT_ISU) {
            qs_pe->pe_type = PE_STREAM_UNI_IN;
        } else if (qconn_pe->pe_poll_item.revents & SSL_POLL_EVENT_ISB) {
            qs_pe->pe_type = PE_STREAM;
            /*
             * disable write side on duplex (bi-directional) stream,
             * because we need to read response from client first.
             */
            pe_pause_write(qs_pe);
        }
        qs_pe->pe_appdata = NULL;
        pe_resume_read(qs_pe);
    } else {
        SSL_free(qs);
        rv = -1;
    }

    return rv;
}

static void
app_destroy_qconn(struct poll_event *pe)
{
    struct poll_event_connection *pec;
    struct poll_event_context *peccx, *peccx_save;

    pec = pe_to_connection(pe);
    if (pec == NULL)
        return;

    OSSL_LIST_FOREACH_DELSAFE(peccx, peccx_save, peccx, &pec->pec_unistream_cx) {
        peccx->peccx_cb_ondestroy(peccx->peccx);
        OPENSSL_free(peccx);
    }

    OSSL_LIST_FOREACH_DELSAFE(peccx, peccx_save, peccx, &pec->pec_stream_cx) {
        peccx->peccx_cb_ondestroy(peccx->peccx);
        OPENSSL_free(peccx);
    }
}

static int
app_accept_qconn(struct poll_event *listener_pe)
{
    SSL *listener;
    SSL *qconn;
    struct poll_event *qc_pe;

    listener = get_ssl_from_pe(listener_pe);
    qconn = SSL_accept_connection(listener, 0);
    if (qconn == NULL)
        return -1;

    qc_pe = new_qconn_pe(qconn);
    if (qc_pe != NULL) {
        qc_pe->pe_cb_in = app_accept_stream_cb;
        qc_pe->pe_cb_out = app_new_stream_cb;
        qc_pe->pe_cb_error = app_handle_qconn_error;
        qc_pe->pe_cb_ondestroy = app_destroy_qconn;
        add_pe_to_pm(listener_pe->pe_my_pm, qc_pe);
    } else {
        SSL_free(qconn);
        return -1;
    }

    return 0;
}

/*
 * Main loop for server to accept QUIC connections.
 * Echo every request back to the client.
 */
static int
run_quic_server(SSL_CTX *ctx, struct poll_manager *pm, int fd)
{
    int ok = -1;
    int e = 0;
    unsigned int i;
    SSL *listener;
    struct poll_event *pe;
    struct poll_event_listener *listener_pe = NULL;
    size_t poll_items;

    /* Create a new QUIC listener */
    if ((listener = SSL_new_listener(ctx, 0)) == NULL)
        goto err;

    if (!SSL_set_fd(listener, fd))
        goto err;

    /*
     * Set the listener mode to non-blocking, which is inherited by
     * child objects.
     */
    if (!SSL_set_blocking_mode(listener, 0))
        goto err;

    /*
     * Begin listening. Note that is not usually needed as SSL_accept_connection
     * will implicitly start listening. It is only needed if a server wishes to
     * ensure it has started to accept incoming connections but does not wish to
     * actually call SSL_accept_connection yet.
     */
    if (!SSL_listen(listener))
        goto err;

    listener_pe = new_listener_pe(listener);
    if (listener_pe == NULL)
        goto err;
    listener = NULL; /* listener_pe took ownership */

    pe = (struct poll_event *)listener_pe;
    pe->pe_cb_in = app_accept_qconn;
    pe->pe_cb_error = pe_handle_listener_error;

    add_pe_to_pm(pm, pe);
    listener_pe = NULL; /* listener is owned by pm now */

    /*
     * Begin an infinite loop of listening for connections. We will only
     * exit this loop if we encounter an error.
     */
    pm->pm_continue = 1;
    while (pm->pm_continue) {
        rebuild_poll_set(pm);
        ok = SSL_poll((SSL_POLL_ITEM *)pm->pm_poll_set, pm->pm_event_count,
                      sizeof (struct poll_event), NULL, 0, &poll_items);

        if (ok == 0 && poll_items == 0)
            break;

        for (i = 0; i < pm->pm_event_count; i++) {
            pe = &pm->pm_poll_set[i];
            if (pe->pe_poll_item.revents == 0)
                continue;
            DPRINTF(stderr, "%s %s (%p) " POLL_FMT "\n", __func__,
                    pe_type_to_name(pe), pe,
                    POLL_PRINTA(pe->pe_poll_item.revents));
            pe->pe_self->pe_poll_item.revents = pe->pe_poll_item.revents;
            if (pe->pe_poll_item.revents & SSL_POLL_ERROR)
                e = pe->pe_cb_error(pe->pe_self);
            else if (pe->pe_poll_item.revents & SSL_POLL_IN)
                e = pe->pe_cb_in(pe->pe_self);
            else if (pe->pe_poll_item.revents & SSL_POLL_OUT)
                e = pe->pe_cb_out(pe->pe_self);

            if (e == -1) {
                pe = pm->pm_poll_set[i].pe_self;
                destroy_pe(pe);
            }
        }
    }

    ok = EXIT_SUCCESS;
err:
    SSL_free(listener);
    destroy_pe((struct poll_event *)listener_pe);
    destroy_poll_manager(pm);
    return ok;
}

/*
 * ALPN strings for TLS handshake. Only 'http/1.0' and 'hq-interop'
 * are accepted.
 */
static const unsigned char alpn_ossltest[] = {
    8,  'h', 't', 't', 'p', '/', '1', '.', '0',
    10, 'h', 'q', '-', 'i', 'n', 't', 'e', 'r', 'o', 'p',
};

/*
 * This callback validates and negotiates the desired ALPN on the server side.
 */
static int
select_alpn(SSL *ssl, const unsigned char **out, unsigned char *out_len,
            const unsigned char *in, unsigned int in_len, void *arg)
{
    if (SSL_select_next_proto((unsigned char **)out, out_len, alpn_ossltest,
                              sizeof(alpn_ossltest), in,
                              in_len) == OPENSSL_NPN_NEGOTIATED)
        return SSL_TLSEXT_ERR_OK;
    return SSL_TLSEXT_ERR_ALERT_FATAL;
}

/* Create SSL_CTX. */
static SSL_CTX *
create_ctx(const char *cert_path, const char *key_path)
{
    SSL_CTX *ctx;

    /*
     * An SSL_CTX holds shared configuration information for multiple
     * subsequent per-client connections. We specifically load a QUIC
     * server method here.
     */
    ctx = SSL_CTX_new(OSSL_QUIC_server_method());
    if (ctx == NULL)
        goto err;

    /*
     * Load the server's certificate *chain* file (PEM format), which includes
     * not only the leaf (end-entity) server certificate, but also any
     * intermediate issuer-CA certificates.  The leaf certificate must be the
     * first certificate in the file.
     *
     * In advanced use-cases this can be called multiple times, once per public
     * key algorithm for which the server has a corresponding certificate.
     * However, the corresponding private key (see below) must be loaded first,
     * *before* moving on to the next chain file.
     *
     * The requisite files "chain.pem" and "pkey.pem" can be generated by running
     * "make chain" in this directory.  If the server will be executed from some
     * other directory, move or copy the files there.
     */
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_path) <= 0) {
        DPRINTF(stderr, "couldn't load certificate file: %s\n", cert_path);
        goto err;
    }

    /*
     * Load the corresponding private key, this also checks that the private
     * key matches the just loaded end-entity certificate.  It does not check
     * whether the certificate chain is valid, the certificates could be
     * expired, or may otherwise fail to form a chain that a client can validate.
     */
    if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0) {
        DPRINTF(stderr, "couldn't load key file: %s\n", key_path);
        goto err;
    }

    /*
     * Clients rarely employ certificate-based authentication, and so we don't
     * require "mutual" TLS authentication (indeed there's no way to know
     * whether or how the client authenticated the server, so the term "mutual"
     * is potentially misleading).
     *
     * Since we're not soliciting or processing client certificates, we don't
     * need to configure a trusted-certificate store, so no call to
     * SSL_CTX_set_default_verify_paths() is needed.  The server's own
     * certificate chain is assumed valid.
     */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    /* Setup ALPN negotiation callback to decide which ALPN is accepted. */
    SSL_CTX_set_alpn_select_cb(ctx, select_alpn, NULL);

    return ctx;

err:
    SSL_CTX_free(ctx);
    return NULL;
}

/* Create UDP socket on the given port. */
static int
create_socket(uint16_t port)
{
    int fd;
    struct sockaddr_in sa = {0};

    /* Retrieve the file descriptor for a new UDP socket */
    if ((fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        DPRINTF(stderr, "cannot create socket");
        return -1;
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    /* Bind to the new UDP socket on localhost */
    if (bind(fd, (const struct sockaddr *)&sa, sizeof(sa)) < 0) {
        DPRINTF(stderr, "cannot bind to %u\n", port);
        BIO_closesocket(fd);
        return -1;
    }

    /* Set port to nonblocking mode */
    if (BIO_socket_nbio(fd, 1) <= 0) {
        DPRINTF(stderr, "Unable to set port to nonblocking mode");
        BIO_closesocket(fd);
        return -1;
    }

    return fd;
}

/* Minimal QUIC HTTP/1.0 server. */
int
main(int argc, char *argv[])
{
    int res = EXIT_FAILURE;
    SSL_CTX *ctx = NULL;
    int fd;
    unsigned long port;
    struct poll_manager *pm;

#ifdef _WIN32
    progname = argv[0];
#endif

    if (argc != 4)
        errx(res, "usage: %s <port> <server.crt> <server.key>", argv[0]);

    /* Create SSL_CTX that supports QUIC. */
    if ((ctx = create_ctx(argv[2], argv[3])) == NULL) {
        ERR_print_errors_fp(stderr);
        errx(res, "Failed to create context");
    }

    /* Parse port number from command line arguments. */
    port = strtoul(argv[1], NULL, 0);
    if (port == 0 || port > UINT16_MAX) {
        SSL_CTX_free(ctx);
        errx(res, "Failed to parse port number");
    }

    /* Create and bind a UDP socket. */
    if ((fd = create_socket((uint16_t)port)) < 0) {
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        errx(res, "Failed to create socket");
    }

    pm = create_poll_manager();
    if (pm == NULL) {
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        errx(res, "Failed to create socket");
    }

    /* QUIC server connection acceptance loop. */
    if (run_quic_server(ctx, pm, fd) < 0) {
        SSL_CTX_free(ctx);
        BIO_closesocket(fd);
        ERR_print_errors_fp(stderr);
        errx(res, "Error in QUIC server loop");
    }

    destroy_poll_manager(pm);
    /* Free resources. */
    SSL_CTX_free(ctx);
    BIO_closesocket(fd);
    res = EXIT_SUCCESS;
    return res;
}
