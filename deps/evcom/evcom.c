/* Copyright (c) 2008,2009 Ryan Dahl
 *
 * evcom_queue comes from ngx_queue.h 
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h> /* memset */

#include <netinet/tcp.h> /* TCP_NODELAY */
#include <sys/types.h>
#include <sys/socket.h> /* shutdown */
#include <sys/un.h>

#include <ev.h>
#include <evcom.h>

#if EV_MULTIPLICITY
# define STREAM_LOOP_ stream->loop, 
# define SERVER_LOOP_ server->loop, 
#else
# define STREAM_LOOP_ 
# define SERVER_LOOP_
#endif // EV_MULTIPLICITY

#if EVCOM_HAVE_GNUTLS
static int secure_full_goodbye (evcom_stream *stream);
static int secure_half_goodbye (evcom_stream *stream);
#endif

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0
#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define OKAY  0
#define AGAIN 1
#define ERROR 2 

#define ATTACHED(s)         ((s)->flags & EVCOM_ATTACHED)
#define LISTENING(s)        ((s)->flags & EVCOM_LISTENING)
#define CONNECTED(s)        ((s)->flags & EVCOM_CONNECTED)
#define SECURE(s)           ((s)->flags & EVCOM_SECURE)
#define GOT_HALF_CLOSE(s)   ((s)->flags & EVCOM_GOT_HALF_CLOSE)
#define GOT_FULL_CLOSE(s)   ((s)->flags & EVCOM_GOT_FULL_CLOSE)
#define TOO_MANY_CONN(s)    ((s)->flags & EVCOM_TOO_MANY_CONN)
#define READ_PAUSED(s)      ((s)->flags & EVCOM_READ_PAUSED)


static void
accept_new_connections (evcom_server *server)
{
  if (TOO_MANY_CONN(server)) {
#if EV_MULTIPLICITY
    evcom_server_attach(server->loop, server);
#else 
    evcom_server_attach(server);
#endif 
    server->flags &= ~EVCOM_TOO_MANY_CONN;
  }
}

static void
dont_accept_new_connections (evcom_server *server)
{
  evcom_server_detach(server);
  server->flags |= EVCOM_TOO_MANY_CONN;
}

EV_INLINE int
set_nonblock (int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return -1;

  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (r == -1) return -1;

  return 0;
}

void
evcom_buf_destroy (evcom_buf *buf)
{
  free(buf->base);
  free(buf);
}

evcom_buf *
evcom_buf_new2 (size_t len)
{
  evcom_buf *buf = malloc(sizeof(evcom_buf));
  if (!buf) return NULL;
  buf->base = malloc(len);
  if (!buf->base) {
    free(buf);
    return NULL; 
  }
  buf->len = len;
  buf->release = evcom_buf_destroy;
  return buf;
}

evcom_buf *
evcom_buf_new (const char *base, size_t len)
{
  evcom_buf *buf = evcom_buf_new2(len);
  if (!buf) return NULL;
  memcpy(buf->base, base, len);

  return buf;
}

static int 
full_close (evcom_stream *stream)
{
  //printf("close(%d)\n", stream->fd);
  if (close(stream->fd) == -1) {
    if (errno == EINTR) {
      return AGAIN;
    } else {
      stream->errorno = errno;
      return ERROR;
    }
  }

  stream->read_action = NULL;
  stream->write_action = NULL;
  stream->fd = -1;

  return OKAY;
}

static inline void
close_asap (evcom_stream *stream)
{
  stream->read_action = full_close;
  stream->write_action = full_close;
}

static int 
half_close (evcom_stream *stream)
{
  int r = shutdown(stream->fd, SHUT_WR);
  if (r == -1) {
    switch (errno) {
      case ENOTCONN:
        stream->errorno = errno;
        return ERROR;

      default:
        perror("shutdown()");
        stream->errorno = errno;
        assert(0 && "Shouldn't get an error on shutdown");
        return ERROR;
    }
  }
  stream->write_action = NULL;
  if (stream->read_action == NULL)
    stream->fd = -1;
  return OKAY;
}

// This is to be called when ever the out_stream is empty
// and we need to change state.
static void
change_state_for_empty_out_stream (evcom_stream *stream)
{
  /*
   * a very complicated bunch of close logic!
   * XXX this is awful. FIXME
   */
  if (stream->write_action == full_close || stream->read_action == full_close) {
    return;
  }

  if (!GOT_HALF_CLOSE(stream)) {
    if (!GOT_FULL_CLOSE(stream)) {
      /* Normal situation. Didn't get any close signals. */
      ev_io_stop(STREAM_LOOP_ &stream->write_watcher);
    } else {
      /* Got Full Close. */
      if (stream->read_action) {
#if EVCOM_HAVE_GNUTLS
        stream->read_action = SECURE(stream) ? secure_full_goodbye : full_close;
#else 
        stream->read_action = full_close;
#endif
      }

      if (stream->write_action) {
#if EVCOM_HAVE_GNUTLS
        stream->write_action = SECURE(stream) ? secure_full_goodbye : full_close;
#else 
        stream->write_action = full_close;
#endif
      }
    }
  } else {
    /* Got Half Close. */
    if (stream->write_action) {
#if EVCOM_HAVE_GNUTLS
      stream->write_action = SECURE(stream) ? secure_half_goodbye : half_close;
#else 
      stream->write_action = half_close;
#endif
      ev_io_start(STREAM_LOOP_ &stream->write_watcher);
    }
  }
}

static void
update_write_buffer_after_send (evcom_stream *stream, ssize_t sent)
{
  evcom_queue *q = evcom_queue_last(&stream->out_stream);
  evcom_buf *to_write = evcom_queue_data(q, evcom_buf, queue);
  to_write->written += sent;
  stream->written += sent;

  if (to_write->written == to_write->len) {

    evcom_queue_remove(q);

    if (to_write->release) {
      to_write->release(to_write);
    }  

    if (evcom_queue_empty(&stream->out_stream)) {
      change_state_for_empty_out_stream(stream);
      if (stream->on_drain)
        stream->on_drain(stream);
    }
  }
}

#if EVCOM_HAVE_GNUTLS
static int secure_stream_send (evcom_stream *stream);
static int secure_stream_recv (evcom_stream *stream);

/* TODO can this be done without ignoring SIGPIPE?  */
static ssize_t 
nosigpipe_push (gnutls_transport_ptr_t data, const void *buf, size_t len)
{
  evcom_stream *stream = (evcom_stream*)data;
  assert(SECURE(stream));
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
  flags |= MSG_DONTWAIT;
#endif
  int r = send(stream->fd, buf, len, flags);

  if (r == -1) {
    stream->errorno = errno;
  }

  return r;
}

static int
secure_handshake (evcom_stream *stream)
{
  assert(SECURE(stream));

  int r = gnutls_handshake(stream->session);

  if (gnutls_error_is_fatal(r)) {
    stream->gnutls_errorno = r;
    return ERROR;
  }

  if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN)
    return AGAIN;

  evcom_stream_reset_timeout(stream);

  if (!CONNECTED(stream)) {
    stream->flags |= EVCOM_CONNECTED;
    if (stream->on_connect) stream->on_connect(stream);
  }

  if (stream->read_action == secure_handshake) {
    stream->read_action = secure_stream_recv;
  } 

  if (stream->write_action == secure_handshake) {
    stream->write_action = secure_stream_send;
  }

  return OKAY;
}

static int
secure_stream_send (evcom_stream *stream)
{
  ssize_t sent;

  if (evcom_queue_empty(&stream->out_stream)) {
    ev_io_stop(STREAM_LOOP_ &stream->write_watcher);
    return AGAIN;
  }

  evcom_queue *q = evcom_queue_last(&stream->out_stream);
  evcom_buf *to_write = evcom_queue_data(q, evcom_buf, queue);

  assert(SECURE(stream));

  sent = gnutls_record_send(stream->session,
    to_write->base + to_write->written,
    to_write->len - to_write->written); 

  if (gnutls_error_is_fatal(sent)) {
    stream->gnutls_errorno = sent;
    return ERROR;
  }

  if (sent == 0) return AGAIN;

  evcom_stream_reset_timeout(stream);

  if (sent == GNUTLS_E_INTERRUPTED || sent == GNUTLS_E_AGAIN) {
    return AGAIN;
  }

  if (sent > 0) {
    /* make sure the callbacks are correct */
    if (stream->read_action)
      stream->read_action = secure_stream_recv;
    update_write_buffer_after_send(stream, sent);
    return OKAY;
  }

  assert(0 && "Unhandled return code from gnutls_record_send()!");
  return ERROR;
}

static int
secure_stream_recv (evcom_stream *stream)
{
  char recv_buffer[stream->chunksize];
  size_t recv_buffer_size = stream->chunksize;
  ssize_t recved;

  assert(SECURE(stream));

  recved = gnutls_record_recv(stream->session, recv_buffer, recv_buffer_size);

  //printf("secure stream recv %d %p\n", recved, stream->on_connect);

  if (gnutls_error_is_fatal(recved)) {
    stream->gnutls_errorno = recved;
    return ERROR;
  }

  if (recved == GNUTLS_E_INTERRUPTED || recved == GNUTLS_E_AGAIN)  {
    return AGAIN;
  }

  evcom_stream_reset_timeout(stream);

  /* A server may also receive GNUTLS_E_REHANDSHAKE when a client has
   * initiated a handshake. In that case the server can only initiate a
   * handshake or terminate the connection. */
  if (recved == GNUTLS_E_REHANDSHAKE) {
    if (stream->write_action) {
      stream->read_action = secure_handshake;
      stream->write_action = secure_handshake;
      return OKAY;
    } else {
      stream->read_action = full_close;
      // set error
      return ERROR;
    }
  }

  if (recved >= 0) {
    /* Got EOF */
    if (recved == 0) {
      stream->read_action = NULL;
      if (stream->write_action == NULL) close_asap(stream);
    }

    if (stream->write_action) {
      stream->write_action = secure_stream_send;
    }

    if (stream->on_read) {
      stream->on_read(stream, recv_buffer, recved);
    }

    return OKAY;
  }

  assert(0 && "Unhandled return code from gnutls_record_send()!");
  return ERROR;
}

static int
secure_full_goodbye (evcom_stream *stream)
{
  assert(SECURE(stream));

  int r = gnutls_bye(stream->session, GNUTLS_SHUT_RDWR);

  if (gnutls_error_is_fatal(r))  {
    stream->gnutls_errorno = r;
    return ERROR;
  }

  if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) return AGAIN;

  close_asap(stream);

  return OKAY;
}

static int
secure_half_goodbye (evcom_stream *stream)
{
  assert(SECURE(stream));

  int r = gnutls_bye(stream->session, GNUTLS_SHUT_WR);

  if (gnutls_error_is_fatal(r))  {
    stream->gnutls_errorno = r;
    return ERROR;
  }

  if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) return AGAIN;

  if (stream->write_action) stream->write_action = half_close;

  return OKAY;
}

void
evcom_stream_set_secure_session (evcom_stream *stream, gnutls_session_t session)
{
  stream->session = session;
  stream->flags |= EVCOM_SECURE;
}
#endif /* HAVE GNUTLS */

static int
stream_send (evcom_stream *stream)
{
  ssize_t sent;

  assert(!SECURE(stream));

  if (evcom_queue_empty(&stream->out_stream)) {
    ev_io_stop(STREAM_LOOP_ &stream->write_watcher);
    return AGAIN;
  }

  evcom_queue *q = evcom_queue_last(&stream->out_stream);
  evcom_buf *to_write = evcom_queue_data(q, evcom_buf, queue);
  
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
  flags |= MSG_DONTWAIT;
#endif

  /* TODO use writev() here */

  sent = send(stream->fd,
    to_write->base + to_write->written,
    to_write->len - to_write->written,
    flags);

  if (sent < 0) {
    switch (errno) {
#ifdef EWOULDBLOCK
      case EWOULDBLOCK:
#else
      case EAGAIN:
#endif 
        return AGAIN;

      default:
        stream->errorno = errno;
        return ERROR;
    }
  }

  evcom_stream_reset_timeout(stream);

  if (!CONNECTED(stream)) {
    stream->flags |= EVCOM_CONNECTED;
    if (stream->on_connect) stream->on_connect(stream);
  }

  update_write_buffer_after_send(stream, sent);

  return OKAY;
}

static int
stream_recv (evcom_stream *stream)
{
  char buf[stream->chunksize];
  size_t buf_size = stream->chunksize;
  ssize_t recved;

  assert(!SECURE(stream));

  if (!CONNECTED(stream)) {
    stream->flags |= EVCOM_CONNECTED;
    if (stream->on_connect) stream->on_connect(stream);
    //return OKAY;
  }

  recved = recv(stream->fd, buf, buf_size, 0);

  if (recved < 0) {
    switch (errno) {
#ifdef EWOULDBLOCK
      case EWOULDBLOCK:
#else 
      case EAGAIN:
#endif
        return AGAIN;

      case EINTR:  
        return AGAIN;

      default:
        stream->errorno = errno;
        return ERROR;
    }
  }

  evcom_stream_reset_timeout(stream);

  if (recved == 0) {
    evcom_stream_read_pause(stream);
    stream->read_action = NULL;
    if (stream->write_action == NULL) close_asap(stream);
  }

  /* NOTE: EOF is signaled with recved == 0 on callback */
  if (stream->on_read) stream->on_read(stream, buf, recved);

  return OKAY;
}

static void
assign_file_descriptor (evcom_stream *stream, int fd)
{
  stream->fd = fd;

  ev_io_set (&stream->read_watcher, fd, EV_READ);
  ev_io_set (&stream->write_watcher, fd, EV_WRITE);

  stream->read_action = stream_recv;
  stream->write_action = stream_send;

#if EVCOM_HAVE_GNUTLS
  if (SECURE(stream)) {
    gnutls_transport_set_lowat(stream->session, 0); 
    gnutls_transport_set_push_function(stream->session, nosigpipe_push);
    gnutls_transport_set_ptr2(stream->session,
        (gnutls_transport_ptr_t)(intptr_t)fd, /* recv */
        stream); /* send */   
    stream->read_action = secure_handshake;
    stream->write_action = secure_handshake;
  }
#endif 
}

static void
server_close_with_error (evcom_server *server, int errorno)
{
  if (LISTENING(server)) {
    evcom_server_detach(server);
    close(server->fd); /* TODO do this on the loop? check return value? */
    server->fd = -1;
    server->flags &= ~EVCOM_LISTENING;

    if (server->on_close) {
      server->on_close(server, errorno);
    }
  }
}


/* Retruns evcom_stream if a connection could be accepted. 
 * The returned stream is not yet attached to the event loop.
 * Otherwise NULL
 */
static evcom_stream*
accept_connection (evcom_server *server)
{
  struct sockaddr address; /* connector's address information */
  socklen_t addr_len = sizeof(address);
  
  int fd = accept(server->fd, (struct sockaddr*)&address, &addr_len);
  if (fd < 0) {
    switch (errno) {
      case EMFILE:
        dont_accept_new_connections(server);
        return NULL;

      case EAGAIN:
        return NULL;

      default:
        goto error;
    }
  }

  evcom_stream *stream = NULL;

  if (server->on_connection) {
    stream = server->on_connection(server, (struct sockaddr*)&address);
  }

  if (stream == NULL) {
    close(fd);
    return NULL;
  }
  
  if (set_nonblock(fd) != 0) goto error;
  
#ifdef SO_NOSIGPIPE
  int flags = 1;
  int r = setsockopt(fd, SOL_STREAM, SO_NOSIGPIPE, &flags, sizeof(flags));
  if (r < 0) goto error;
#endif

  stream->server = server;
  assign_file_descriptor(stream, fd);

  return stream;

error:
  server_close_with_error(server, errno);
  return NULL;
}


/* Internal callback 
 * Called by server->connection_watcher.
 */
static void 
on_connection (EV_P_ ev_io *watcher, int revents)
{
  evcom_server *server = watcher->data;

  assert(LISTENING(server));
#if EV_MULTIPLICITY
  assert(server->loop == loop);
#endif 
  assert(&server->connection_watcher == watcher);
  
  if (EV_ERROR & revents) {
    server_close_with_error(server, 1);
    return;
  }

  /* accept as many connections as possible */
  evcom_stream *stream;
  while ((stream = accept_connection(server))) {
    evcom_stream_attach(EV_A_ stream);
  }
}

static inline socklen_t
address_length (struct sockaddr *address)
{
  struct sockaddr_un* unix_address = (struct sockaddr_un*)address;

  switch (address->sa_family) {
    case AF_INET:
      return sizeof(struct sockaddr_in);

    case AF_INET6:
      return sizeof(struct sockaddr_in6);

    case AF_UNIX:
      return strlen(unix_address->sun_path) + sizeof(unix_address->sun_family);

    default:
      assert(0 && "Unsupported socket family");
  }
  return 0;
}

int
evcom_server_listen (evcom_server *server, struct sockaddr *address, int backlog)
{
  int fd = -1;
  assert(!LISTENING(server));

  fd = socket(address->sa_family, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("stream()");
    return -1;
  }

  if (set_nonblock(fd) != 0) {
    perror("set_nonblock()");
    return -1;
  }

  int flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));

  /* XXX: Sending single byte chunks in a response body? Perhaps there is a
   * need to enable the Nagel algorithm dynamically. For now disabling.
   */
  //setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));

  if (bind(fd, address, address_length(address)) < 0) {
    perror("bind()");
    close(fd);
    return -1;
  }
  
  if (listen(fd, backlog) < 0) {
    perror("listen()");
    close(fd);
    return -1;
  }
  
  server->fd = fd;
  server->flags |= EVCOM_LISTENING;
  ev_io_set (&server->connection_watcher, server->fd, EV_READ);
  
  return 0;
}

/**
 * Stops the server. Will not accept new connections.  Does not drop
 * existing connections.
 */
void 
evcom_server_close (evcom_server *server)
{
  server_close_with_error(server, 0);
}

void
evcom_server_attach (EV_P_ evcom_server *server)
{
  ev_io_start (EV_A_ &server->connection_watcher);
#if EV_MULTIPLICITY
  server->loop = loop;
#endif
  server->flags |= EVCOM_ATTACHED;
}

void
evcom_server_detach (evcom_server *server)
{
  ev_io_stop (SERVER_LOOP_ &server->connection_watcher);
#if EV_MULTIPLICITY
  server->loop = NULL;
#endif
  server->flags &= ~EVCOM_ATTACHED;
}

void 
evcom_server_init (evcom_server *server)
{
  server->flags = 0;
  server->fd = -1;
  server->connection_watcher.data = server;
  ev_init (&server->connection_watcher, on_connection);
  server->on_connection = NULL;
  server->on_close = NULL;
}

/* Internal callback. called by stream->timeout_watcher */
static void 
on_timeout (EV_P_ ev_timer *watcher, int revents)
{
  evcom_stream *stream = watcher->data;

#if EV_MULTIPLICITY
  assert(stream->loop == loop);
#endif
  assert(revents == EV_TIMEOUT);
  assert(watcher == &stream->timeout_watcher);

  if (READ_PAUSED(stream)) {
    evcom_stream_reset_timeout(stream);
    return;
  }

  if (stream->on_timeout) stream->on_timeout(stream);
  // timeout does not automatically kill your connection. you must!
}

static void
release_write_buffer(evcom_stream *stream)
{
  while (!evcom_queue_empty(&stream->out_stream)) {
    evcom_queue *q = evcom_queue_last(&stream->out_stream);
    evcom_buf *buf = evcom_queue_data(q, evcom_buf, queue);
    evcom_queue_remove(q);
    if (buf->release) buf->release(buf);
  }
}

/* Internal callback. called by stream->read_watcher */
static void 
on_io_event(EV_P_ ev_io *watcher, int revents)
{
  evcom_stream *stream = watcher->data;

  if (revents & EV_ERROR) {
    stream->errorno = 1;
    close_asap(stream);
  }

  int r;
  int have_read_event  = (stream->read_action  != NULL);
  int have_write_event = (stream->write_action != NULL);

  while (have_read_event || have_write_event) {
    /* RECV LOOP - TRY TO CLEAR THE BUFFER */
    if (stream->read_action == NULL) {
      have_read_event = FALSE;
    } else { 
      r = stream->read_action(stream);

      if (r == AGAIN) {
        have_read_event = FALSE;
      } else { 
        if (r == ERROR) close_asap(stream);
      }
    }

    /* SEND LOOP - TRY TO CLEAR THE BUFFER */
    if (stream->write_action == NULL) {
      have_write_event = FALSE;
    } else {
      r = stream->write_action(stream);

      if (r == AGAIN) {
        have_write_event = FALSE;
      } else {
        if (r == ERROR) close_asap(stream);
      }
    }
  }

  // Close when both sides of the stream are closed.
  if (stream->write_action == NULL && stream->read_action == NULL) {
    release_write_buffer(stream);

    ev_clear_pending (EV_A_ &stream->write_watcher);
    ev_clear_pending (EV_A_ &stream->read_watcher);
    ev_clear_pending (EV_A_ &stream->timeout_watcher);

    evcom_stream_detach(stream);
    assert(stream->fd == -1);

    if (stream->server) {
      accept_new_connections(stream->server);
    }

    if (stream->on_close) stream->on_close(stream);
    /* WARNING: user can free stream in on_close so no more 
     * access beyond this point. */
  }
}

/**
 * If using SSL do consider setting
 *   gnutls_db_set_retrieve_function (stream->session, _);
 *   gnutls_db_set_remove_function (stream->session, _);
 *   gnutls_db_set_store_function (stream->session, _);
 *   gnutls_db_set_ptr (stream->session, _);
 */
void 
evcom_stream_init (evcom_stream *stream, float timeout)
{
  stream->fd = -1;
  stream->server = NULL;
#if EV_MULTIPLICITY
  stream->loop = NULL;
#endif
  stream->flags = 0;

  evcom_queue_init(&stream->out_stream);

  ev_init(&stream->write_watcher, on_io_event);
  stream->write_watcher.data = stream;

  ev_init(&stream->read_watcher, on_io_event);
  stream->read_watcher.data = stream;

  stream->errorno = 0;

#if EVCOM_HAVE_GNUTLS
  stream->gnutls_errorno = 0;
  stream->session = NULL;
#endif 

  ev_timer_init(&stream->timeout_watcher, on_timeout, 0., timeout);
  stream->timeout_watcher.data = stream;  

  stream->read_action = NULL;
  stream->write_action = NULL;

  stream->chunksize = 8*1024; 

  stream->on_connect = NULL;
  stream->on_read = NULL;
  stream->on_drain = NULL;
  stream->on_timeout = NULL;
}

void 
evcom_stream_close (evcom_stream *stream)
{
  stream->flags |= EVCOM_GOT_HALF_CLOSE;
  if (evcom_queue_empty(&stream->out_stream)) {
    change_state_for_empty_out_stream(stream);
  }
}

void 
evcom_stream_full_close (evcom_stream *stream)
{
  stream->flags |= EVCOM_GOT_FULL_CLOSE;
  if (evcom_queue_empty(&stream->out_stream)) {
    change_state_for_empty_out_stream(stream);
  }
}

void evcom_stream_force_close (evcom_stream *stream)
{
  release_write_buffer(stream);

  ev_clear_pending (STREAM_LOOP_ &stream->write_watcher);
  ev_clear_pending (STREAM_LOOP_ &stream->read_watcher);
  ev_clear_pending (STREAM_LOOP_ &stream->timeout_watcher);

  stream->write_action = stream->read_action = NULL;
  // stream->errorno = EVCOM_STREAM_ERROR_FORCE_CLOSE
  
  if (stream->fd > 0) {
    close(stream->fd);
    if (stream->server) {
      accept_new_connections(stream->server);
    }
  }

  stream->fd = -1;

  evcom_stream_detach(stream);
}

void 
evcom_stream_write (evcom_stream *stream, evcom_buf *buf)
{
  if (stream->write_action == NULL) {
    assert(0 && "Do not write to a closed stream"); 
    goto error;
  }
  if (GOT_FULL_CLOSE(stream) || GOT_HALF_CLOSE(stream)) {
    assert(0 && "Do not write to a closing stream");
    goto error;
  }

  evcom_queue_insert_head(&stream->out_stream, &buf->queue);
  buf->written = 0;

  if (ATTACHED(stream)) {
    ev_io_start(STREAM_LOOP_ &stream->write_watcher);
  }
  return;

error:
  if (buf->release) buf->release(buf);
}

void 
evcom_stream_reset_timeout (evcom_stream *stream)
{
  ev_timer_again(STREAM_LOOP_ &stream->timeout_watcher);
}

static void
free_simple_buf (evcom_buf *buf)
{
  free(buf->base);
  free(buf);
}

/* Writes a string to the stream. 
 * NOTE: Allocates memory. Avoid for performance applications.
 */ 
void
evcom_stream_write_simple (evcom_stream *stream, const char *str, size_t len)
{
  evcom_buf *buf = malloc(sizeof(evcom_buf));
  buf->release = free_simple_buf;
  buf->base = strdup(str);
  buf->len = len;

  evcom_stream_write(stream, buf);
}

void
evcom_stream_attach (EV_P_ evcom_stream *stream)
{
#if EV_MULTIPLICITY
  stream->loop = loop;
#endif 
  stream->flags |= EVCOM_ATTACHED;

  ev_timer_again(EV_A_ &stream->timeout_watcher);

  if (stream->read_action) {
    ev_io_start(EV_A_ &stream->read_watcher);
  }

  if (stream->write_action) {
    ev_io_start(EV_A_ &stream->write_watcher);
  }
}

void
evcom_stream_detach (evcom_stream *stream)
{
  if (ATTACHED(stream)) {
    ev_io_stop(STREAM_LOOP_ &stream->write_watcher);
    ev_io_stop(STREAM_LOOP_ &stream->read_watcher);
    ev_timer_stop(STREAM_LOOP_ &stream->timeout_watcher);
#if EV_MULTIPLICITY
    stream->loop = NULL;
#endif
    stream->flags &= ~EVCOM_ATTACHED;
  }
}

void
evcom_stream_read_pause (evcom_stream *stream)
{
  ev_io_stop(STREAM_LOOP_ &stream->read_watcher);
  ev_clear_pending(STREAM_LOOP_ &stream->read_watcher);
  stream->flags |= EVCOM_READ_PAUSED;
}

void
evcom_stream_read_resume (evcom_stream *stream)
{
  stream->flags &= ~EVCOM_READ_PAUSED;

  if (stream->read_action) {
    ev_io_start(STREAM_LOOP_ &stream->read_watcher);
    /* XXX feed event? */
  }
}

int
evcom_stream_connect (evcom_stream *s, struct sockaddr *address)
{
  int fd = socket(address->sa_family, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket()");
    return -1;
  }

  int r = set_nonblock(fd);
  if (r < 0) {
    perror("set_nonblock()");
    return -1;
  }
      
#ifdef SO_NOSIGPIPE
  int flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &flags, sizeof(flags));
#endif

  r = connect(fd, address, address_length(address));

  if (r < 0 && errno != EINPROGRESS) {
    perror("connect");
    close(fd);
    return -1;
  }

  assign_file_descriptor(s, fd);

  return 0;
}

