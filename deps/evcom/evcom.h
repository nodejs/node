/* Copyright (c) 2008,2009 Ryan Dahl
 *
 * evcom_queue comes from Nginx, ngx_queue.h
 * Copyright (C) 2002-2009 Igor Sysoev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <netdb.h>
#include <ev.h>
#include <stddef.h> /* offsetof() */

#ifndef evcom_h
#define evcom_h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EVCOM_HAVE_GNUTLS
# define EVCOM_HAVE_GNUTLS 0
#endif
#if EVCOM_HAVE_GNUTLS
# include <gnutls/gnutls.h>
#endif

/* The maximum evcom_stream will try to read in one callback */
#ifndef EVCOM_CHUNKSIZE
# define EVCOM_CHUNKSIZE (8*1024)
#endif

/* flags for stream and server */
#define EVCOM_ATTACHED          0x0001
#define EVCOM_LISTENING         0x0002
#define EVCOM_CONNECTED         0x0004
#define EVCOM_SECURE            0x0008
#define EVCOM_DUPLEX            0x0010
#define EVCOM_GOT_CLOSE         0x0020
#define EVCOM_PAUSED            0x0040
#define EVCOM_READABLE          0x0080
#define EVCOM_WRITABLE          0x0100
#define EVCOM_TOO_MANY_CONN     0x0200

enum evcom_stream_state { EVCOM_INITIALIZED
                        , EVCOM_CONNECTING
                        , EVCOM_CONNECTED_RW /* read write */
                        , EVCOM_CONNECTED_RO /* read only  */
                        , EVCOM_CONNECTED_WO /* write only */
                        , EVCOM_CLOSING
                        , EVCOM_CLOSED
                        };

typedef struct evcom_queue {
  struct evcom_queue  *prev;
  struct evcom_queue  *next;
} evcom_queue;

typedef struct evcom_buf {
  /* public */
  char *base;
  size_t len;
  void (*release) (struct evcom_buf *); /* called when oi is done with the object */
  void *data;

  /* private */
  size_t written;
  evcom_queue queue;
} evcom_buf;

#if EV_MULTIPLICITY
#  define EVCOM_LOOP struct ev_loop *loop;
#else
#  define EVCOM_LOOP
#endif

#define EVCOM_DESCRIPTOR(type)                                \
  /* private   */  unsigned int flags;                        \
  /* private   */  int (*action) (struct evcom_descriptor*);  \
  /* read-only */  int errorno;                               \
  /* read-only */  int fd;                                    \
  /* read-only */  EVCOM_LOOP                                 \
  /* public    */  void *data;                                \
  /* public    */  void (*on_close) (struct type*);

/* abstract base class */
typedef struct evcom_descriptor {
  EVCOM_DESCRIPTOR(evcom_descriptor)
} evcom_descriptor;

typedef struct evcom_reader {
  EVCOM_DESCRIPTOR(evcom_reader)
  ev_io read_watcher; /* private */
  void (*on_read) (struct evcom_reader*, const void* buf, size_t len); /* public */
} evcom_reader;

typedef struct evcom_writer {
  EVCOM_DESCRIPTOR(evcom_writer)
  ev_io write_watcher; /* private */
  evcom_queue out; /* private */
} evcom_writer;

typedef struct evcom_stream {
  /* PRIVATE */
  EVCOM_LOOP
  int errorno;
  unsigned int flags;
  evcom_queue out;
  ev_io read_watcher;
  ev_io write_watcher;
  int (*send_action) (struct evcom_stream*);
  int (*recv_action) (struct evcom_stream*);
  ev_timer timeout_watcher;
#if EVCOM_HAVE_GNUTLS
  gnutls_session_t session;
#endif

  /* READ-ONLY */
  int recvfd;
  int sendfd;
  struct evcom_server *server;
#if EVCOM_HAVE_GNUTLS
  int gnutls_errorno;
#endif

  /* PUBLIC */
  void (*on_connect) (struct evcom_stream *);
  void (*on_timeout) (struct evcom_stream *);
  void (*on_read)    (struct evcom_stream *, const void* buf, size_t len);
  void (*on_drain)   (struct evcom_stream *);
  void (*on_close)   (struct evcom_stream *);
  void *data;
} evcom_stream;

typedef struct evcom_server {
  EVCOM_DESCRIPTOR(evcom_server)

  /* PRIVATE */
  ev_io watcher;

  /* PUBLIC */
  struct evcom_stream*
    (*on_connection)(struct evcom_server *, struct sockaddr *remote_addr);
} evcom_server;

/* Highly recommended to ignore SIGPIPE! */
void evcom_ignore_sigpipe (void);

void evcom_reader_init          (evcom_reader*);
void evcom_reader_set           (evcom_reader*, int fd);
void evcom_reader_attach        (EV_P_ evcom_reader*);
void evcom_reader_detach        (evcom_reader*);
void evcom_reader_close         (evcom_reader*);

void evcom_writer_init          (evcom_writer*);
void evcom_writer_set           (evcom_writer*, int fd);
void evcom_writer_attach        (EV_P_ evcom_writer*);
void evcom_writer_detach        (evcom_writer*);
void evcom_writer_write         (evcom_writer*, const char *str, size_t len);
void evcom_writer_close         (evcom_writer*);

void evcom_server_init          (evcom_server *);
 int evcom_server_listen        (evcom_server *, struct sockaddr *address, int backlog);
void evcom_server_attach        (EV_P_ evcom_server *);
void evcom_server_detach        (evcom_server *);
void evcom_server_close         (evcom_server *);

void evcom_stream_init          (evcom_stream *);

 int evcom_stream_pair          (evcom_stream *a, evcom_stream *b);
 int evcom_stream_connect       (evcom_stream *, struct sockaddr *address);
void evcom_stream_assign_fds    (evcom_stream *, int recvfd, int sendfd);

void evcom_stream_attach        (EV_P_ evcom_stream *);
void evcom_stream_detach        (evcom_stream *);
void evcom_stream_read_resume   (evcom_stream *);
void evcom_stream_read_pause    (evcom_stream *);
void evcom_stream_reset_timeout (evcom_stream *, float timeout);
void evcom_stream_set_no_delay  (evcom_stream *, int no_delay);
ssize_t evcom_stream_write      (evcom_stream *, const char *str, size_t len);
/* Once the write buffer is drained, evcom_stream_close will shutdown the
 * writing end of the stream and will close the read end once the server
 * replies with an EOF.
 */
void evcom_stream_close         (evcom_stream *);

/* Will not wait for the write queue to complete. Closes both directions */
void evcom_stream_force_close   (evcom_stream *);


#if EVCOM_HAVE_GNUTLS
/* Tells the stream to use transport layer security (SSL). evcom_stream does
 * not want to make any decisions about security requirements, so the
 * majoirty of GnuTLS configuration is left to the user. Only the transport
 * layer of GnuTLS is controlled by evcom_stream. That is, do not use
 * gnutls_transport_* functions.  Do use the rest of GnuTLS's API.
 */
void evcom_stream_set_secure_session (evcom_stream *, gnutls_session_t);
#endif

enum evcom_stream_state evcom_stream_state (evcom_stream *stream);

evcom_buf* evcom_buf_new     (const char* base, size_t len);
evcom_buf* evcom_buf_new2    (size_t len);

EV_INLINE void
evcom_queue_init (evcom_queue *q)
{
  q->prev = q;
  q->next = q;
}

EV_INLINE void
evcom_queue_insert_head (evcom_queue *h, evcom_queue *x)
{
  (x)->next = (h)->next;
  (x)->next->prev = x;
  (x)->prev = h;
  (h)->next = x;
}

EV_INLINE void
evcom_queue_remove (evcom_queue *x)
{
  (x)->next->prev = (x)->prev;
  (x)->prev->next = (x)->next;
#ifndef NDEBUG
  (x)->prev = NULL;
  (x)->next = NULL;
#endif
}

#define evcom_queue_empty(h) (h == (h)->prev)
#define evcom_queue_head(h) (h)->next
#define evcom_queue_last(h) (h)->prev
#define evcom_queue_data(q, type, link) \
    (type *) ((unsigned char *) q - offsetof(type, link))


#ifdef __cplusplus
}
#endif
#endif /* evcom_h */
