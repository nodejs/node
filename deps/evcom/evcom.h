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

typedef struct evcom_queue   evcom_queue;
typedef struct evcom_buf     evcom_buf;
typedef struct evcom_server  evcom_server;
typedef struct evcom_stream  evcom_stream;

/* flags for stream and server */
#define EVCOM_ATTACHED          0x0001
#define EVCOM_LISTENING         0x0002
#define EVCOM_CONNECTED         0x0004
#define EVCOM_SECURE            0x0008
#define EVCOM_GOT_HALF_CLOSE    0x0010
#define EVCOM_GOT_FULL_CLOSE    0x0020
#define EVCOM_TOO_MANY_CONN     0x0040
#define EVCOM_READ_PAUSED       0x0080

void evcom_server_init          (evcom_server *);
 int evcom_server_listen        (evcom_server *, struct sockaddr *address, int backlog);
void evcom_server_attach        (EV_P_ evcom_server *);
void evcom_server_detach        (evcom_server *);
void evcom_server_close         (evcom_server *);  // synchronous

void evcom_stream_init          (evcom_stream *, float timeout);
 int evcom_stream_connect       (evcom_stream *, struct sockaddr *address);
void evcom_stream_attach        (EV_P_ evcom_stream *);
void evcom_stream_detach        (evcom_stream *);
void evcom_stream_read_resume   (evcom_stream *);
void evcom_stream_read_pause    (evcom_stream *);

/* Resets the timeout to stay alive for another stream->timeout seconds
 */
void evcom_stream_reset_timeout (evcom_stream *);

/* Writes a buffer to the stream. 
 */
void evcom_stream_write         (evcom_stream *, evcom_buf *);

void evcom_stream_write_simple  (evcom_stream *, const char *str, size_t len);

/* Once the write buffer is drained, evcom_stream_close will shutdown the
 * writing end of the stream and will close the read end once the server
 * replies with an EOF. 
 */
void evcom_stream_close         (evcom_stream *);

/* Do not wait for the server to reply with EOF.  This will only be called
 * once the write buffer is drained.  
 * Warning: For TCP stream, the OS kernel may (should) reply with RST
 * packets if this is called when data is still being received from the
 * server.
 */
void evcom_stream_full_close    (evcom_stream *);

/* The most extreme measure. 
 * Will not wait for the write queue to complete. 
 */
void evcom_stream_force_close (evcom_stream *);


#if EVCOM_HAVE_GNUTLS
/* Tells the stream to use transport layer security (SSL). evcom_stream does
 * not want to make any decisions about security requirements, so the
 * majoirty of GnuTLS configuration is left to the user. Only the transport
 * layer of GnuTLS is controlled by evcom_stream. That is, do not use
 * gnutls_transport_* functions.  Do use the rest of GnuTLS's API.
 */
void evcom_stream_set_secure_session (evcom_stream *, gnutls_session_t);
#endif

evcom_buf * evcom_buf_new     (const char* base, size_t len);
evcom_buf * evcom_buf_new2    (size_t len);
void        evcom_buf_destroy (evcom_buf *);


struct evcom_queue {
  evcom_queue  *prev;
  evcom_queue  *next;
};

struct evcom_buf {
  /* public */
  char *base;
  size_t len;
  void (*release) (evcom_buf *); /* called when oi is done with the object */
  void *data;

  /* private */
  size_t written;
  evcom_queue queue;
};

struct evcom_server {
  /* read only */
  int fd;
#if EV_MULTIPLICITY
  struct ev_loop *loop;
#endif
  unsigned flags;

  /* PRIVATE */
  ev_io connection_watcher;

  /* PUBLIC */

  evcom_stream* (*on_connection) (evcom_server *, struct sockaddr *remote_addr);

  /* Executed when a server is closed. 
   * If evcom_server_close() was called errorno will be 0.
   * An libev error is indicated with errorno == 1
   * Otherwise errorno is a stdlib errno from a system call, e.g. accept()
   */
  void (*on_close) (evcom_server *, int errorno);

  void *data;
};

struct evcom_stream {
  /* read only */
  int fd;
#if EV_MULTIPLICITY
  struct ev_loop *loop;
#endif
  evcom_server *server;
  evcom_queue out_stream;
  size_t written;
  unsigned flags;

  /* NULL = that end of the stream is closed. */
  int (*read_action)  (evcom_stream *);
  int (*write_action) (evcom_stream *);

  /* ERROR CODES. 0 = no error. Check on_close. */
  int errorno; 
#if EVCOM_HAVE_GNUTLS
  int gnutls_errorno;
#endif

  /* private */  
  ev_io write_watcher;
  ev_io read_watcher;
  ev_timer timeout_watcher;
#if EVCOM_HAVE_GNUTLS
  gnutls_session_t session;
#endif
  
  /* public */
  size_t chunksize; /* the maximum chunk that on_read() will return */
  void (*on_connect)   (evcom_stream *);
  void (*on_read)      (evcom_stream *, const void *buf, size_t count);
  void (*on_drain)     (evcom_stream *);
  void (*on_close)     (evcom_stream *);
  void (*on_timeout)   (evcom_stream *);
  void *data;
};

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
