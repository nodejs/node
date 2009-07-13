/* Copyright (c) 2008,2009 Ryan Dahl
 *
 * evnet_queue comes from Nginx, ngx_queue.h
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

#ifndef evnet_h
#define evnet_h

#ifdef __cplusplus
extern "C" {
#endif 

#ifndef EVNET_HAVE_GNUTLS
# define EVNET_HAVE_GNUTLS 0
#endif
#if EVNET_HAVE_GNUTLS
# include <gnutls/gnutls.h>
#endif

typedef struct evnet_queue   evnet_queue;
typedef struct evnet_buf     evnet_buf;
typedef struct evnet_server  evnet_server;
typedef struct evnet_socket  evnet_socket;

void evnet_server_init          (evnet_server *);
 int evnet_server_listen        (evnet_server *, struct addrinfo *addrinfo, int backlog);
void evnet_server_attach        (EV_P_ evnet_server *);
void evnet_server_detach        (evnet_server *);
void evnet_server_close         (evnet_server *);  // synchronous

void evnet_socket_init          (evnet_socket *, float timeout);
 int evnet_socket_connect       (evnet_socket *, struct addrinfo *addrinfo);
void evnet_socket_attach        (EV_P_ evnet_socket *);
void evnet_socket_detach        (evnet_socket *);
void evnet_socket_read_start    (evnet_socket *);
void evnet_socket_read_stop     (evnet_socket *);

/* Resets the timeout to stay alive for another socket->timeout seconds
 */
void evnet_socket_reset_timeout (evnet_socket *);

/* Writes a buffer to the socket. 
 * (Do not send a NULL evnet_buf or a buffer with evnet_buf->base == NULL.)
 */
void evnet_socket_write         (evnet_socket *, evnet_buf *);

void evnet_socket_write_simple  (evnet_socket *, const char *str, size_t len);

/* Once the write buffer is drained, evnet_socket_close will shutdown the
 * writing end of the socket and will close the read end once the server
 * replies with an EOF. 
 */
void evnet_socket_close         (evnet_socket *);

/* Do not wait for the server to reply with EOF.  This will only be called
 * once the write buffer is drained.  
 * Warning: For TCP socket, the OS kernel may (should) reply with RST
 * packets if this is called when data is still being received from the
 * server.
 */
void evnet_socket_full_close    (evnet_socket *);

/* The most extreme measure. 
 * Will not wait for the write queue to complete. 
 */
void evnet_socket_force_close (evnet_socket *);


#if EVNET_HAVE_GNUTLS
/* Tells the socket to use transport layer security (SSL). evnet_socket does
 * not want to make any decisions about security requirements, so the
 * majoirty of GnuTLS configuration is left to the user. Only the transport
 * layer of GnuTLS is controlled by evnet_socket. That is, do not use
 * gnutls_transport_* functions.  Do use the rest of GnuTLS's API.
 */
void evnet_socket_set_secure_session (evnet_socket *, gnutls_session_t);
#endif

evnet_buf * evnet_buf_new     (const char* base, size_t len);
evnet_buf * evnet_buf_new2    (size_t len);
void        evnet_buf_destroy (evnet_buf *);


struct evnet_queue {
  evnet_queue  *prev;
  evnet_queue  *next;
};

struct evnet_buf {
  /* public */
  char *base;
  size_t len;
  void (*release) (evnet_buf *); /* called when oi is done with the object */
  void *data;

  /* private */
  size_t written;
  evnet_queue queue;
};

struct evnet_server {
  /* read only */
  int fd;
#if EV_MULTIPLICITY
  struct ev_loop *loop;
#endif
  unsigned attached:1;
  unsigned listening:1;

  /* PRIVATE */
  ev_io connection_watcher;

  /* PUBLIC */

  evnet_socket* (*on_connection) (evnet_server *, struct sockaddr *remote_addr);

  /* Executed when a server is closed. 
   * If evnet_server_close() was called errorno will be 0.
   * An libev error is indicated with errorno == 1
   * Otherwise errorno is a stdlib errno from a system call, e.g. accept()
   */
  void (*on_close) (evnet_server *, int errorno);

  void *data;
};

struct evnet_socket {
  /* read only */
  int fd;
#if EV_MULTIPLICITY
  struct ev_loop *loop;
#endif
  evnet_server *server;
  evnet_queue out_stream;
  size_t written;
  unsigned attached:1;
  unsigned connected:1;
  unsigned secure:1;
  unsigned got_full_close:1;
  unsigned got_half_close:1;

  /* NULL = that end of the socket is closed. */
  int (*read_action)  (evnet_socket *);
  int (*write_action) (evnet_socket *);

  /* ERROR CODES. 0 = no error. Check on_close. */
  int errorno; 
#if EVNET_HAVE_GNUTLS
  int gnutls_errorno;
#endif

  /* private */  
  ev_io write_watcher;
  ev_io read_watcher;
  ev_timer timeout_watcher;
#if EVNET_HAVE_GNUTLS
  gnutls_session_t session;
#endif
  
  /* public */
  size_t chunksize; /* the maximum chunk that on_read() will return */
  void (*on_connect)   (evnet_socket *);
  void (*on_read)      (evnet_socket *, const void *buf, size_t count);
  void (*on_drain)     (evnet_socket *);
  void (*on_close)     (evnet_socket *);
  void (*on_timeout)   (evnet_socket *);
  void *data;
};

EV_INLINE void
evnet_queue_init (evnet_queue *q)
{
  q->prev = q;
  q->next = q;
}

EV_INLINE void
evnet_queue_insert_head (evnet_queue *h, evnet_queue *x)
{
  (x)->next = (h)->next;
  (x)->next->prev = x;
  (x)->prev = h;
  (h)->next = x;
}

EV_INLINE void
evnet_queue_remove (evnet_queue *x)
{
  (x)->next->prev = (x)->prev;
  (x)->prev->next = (x)->next;
#ifndef NDEBUG
  (x)->prev = NULL;
  (x)->next = NULL;
#endif
}

#define evnet_queue_empty(h) (h == (h)->prev)
#define evnet_queue_head(h) (h)->next
#define evnet_queue_last(h) (h)->prev
#define evnet_queue_data(q, type, link) \
    (type *) ((unsigned char *) q - offsetof(type, link))


#ifdef __cplusplus
}
#endif 
#endif /* evnet_h */
