/* Copyright (c) 2008,2009 Ryan Dahl
 *
 * evnet_queue comes from ngx_queue.h 
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

#include <ev.h>
#include <evnet.h>

#if EV_MULTIPLICITY
# define SOCKET_LOOP_ socket->loop, 
# define SERVER_LOOP_ server->loop, 
#else
# define SOCKET_LOOP_ 
# define SERVER_LOOP_
#endif // EV_MULTIPLICITY

#if EVNET_HAVE_GNUTLS
static int secure_full_goodbye (evnet_socket *socket);
static int secure_half_goodbye (evnet_socket *socket);
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
evnet_buf_destroy (evnet_buf *buf)
{
  free(buf->base);
  free(buf);
}

evnet_buf *
evnet_buf_new2 (size_t len)
{
  evnet_buf *buf = malloc(sizeof(evnet_buf));
  if (!buf) return NULL;
  buf->base = malloc(len);
  if (!buf->base) {
    free(buf);
    return NULL; 
  }
  buf->len = len;
  buf->release = evnet_buf_destroy;
  return buf;
}

evnet_buf *
evnet_buf_new (const char *base, size_t len)
{
  evnet_buf *buf = evnet_buf_new2(len);
  if (!buf) return NULL;
  memcpy(buf->base, base, len);

  return buf;
}

#define CLOSE_ASAP(socket) do {         \
  (socket)->read_action = full_close;   \
  (socket)->write_action = full_close;  \
} while (0)

static int 
full_close (evnet_socket *socket)
{
  //printf("close(%d)\n", socket->fd);
  if (close(socket->fd) == -1) {
    if (errno == EINTR) {
      return AGAIN;
    } else {
      socket->errorno = errno;
      return ERROR;
    }
  }

  socket->read_action = NULL;
  socket->write_action = NULL;
  socket->fd = -1;

  return OKAY;
}

static int 
half_close (evnet_socket *socket)
{
  int r = shutdown(socket->fd, SHUT_WR);
  if (r == -1) {
    switch (errno) {
      case ENOTCONN:
        socket->errorno = errno;
        return ERROR;

      default:
        perror("shutdown()");
        socket->errorno = errno;
        assert(0 && "Shouldn't get an error on shutdown");
        return ERROR;
    }
  }
  socket->write_action = NULL;
  if (socket->read_action == NULL)
    socket->fd = -1;
  return OKAY;
}

// This is to be called when ever the out_stream is empty
// and we need to change state.
static void
change_state_for_empty_out_stream (evnet_socket *socket)
{
  /*
   * a very complicated bunch of close logic!
   * XXX this is awful. FIXME
   */
  if (socket->write_action == full_close || socket->read_action == full_close) {
    return;
  }

  if (socket->got_half_close == FALSE) {
    if (socket->got_full_close == FALSE) {
      /* Normal situation. Didn't get any close signals. */
      ev_io_stop(SOCKET_LOOP_ &socket->write_watcher);
    } else {
      /* Got Full Close. */
      if (socket->read_action) {
#if EVNET_HAVE_GNUTLS
        socket->read_action = socket->secure ? secure_full_goodbye : full_close;
#else 
        socket->read_action = full_close;
#endif
      }

      if (socket->write_action) {
#if EVNET_HAVE_GNUTLS
        socket->write_action = socket->secure ? secure_full_goodbye : full_close;
#else 
        socket->write_action = full_close;
#endif
      }
    }
  } else {
    /* Got Half Close. */
    if (socket->write_action) {
#if EVNET_HAVE_GNUTLS
      socket->write_action = socket->secure ? secure_half_goodbye : half_close;
#else 
      socket->write_action = half_close;
#endif
      ev_io_start(SOCKET_LOOP_ &socket->write_watcher);
    }
  }
}

static void
update_write_buffer_after_send (evnet_socket *socket, ssize_t sent)
{
  evnet_queue *q = evnet_queue_last(&socket->out_stream);
  evnet_buf *to_write = evnet_queue_data(q, evnet_buf, queue);
  to_write->written += sent;
  socket->written += sent;

  if (to_write->written == to_write->len) {

    evnet_queue_remove(q);

    if (to_write->release) {
      to_write->release(to_write);
    }  

    if (evnet_queue_empty(&socket->out_stream)) {
      change_state_for_empty_out_stream(socket);
      if (socket->on_drain)
        socket->on_drain(socket);
    }
  }
}

#if EVNET_HAVE_GNUTLS
static int secure_socket_send (evnet_socket *socket);
static int secure_socket_recv (evnet_socket *socket);

/* TODO can this be done without ignoring SIGPIPE?  */
static ssize_t 
nosigpipe_push (gnutls_transport_ptr_t data, const void *buf, size_t len)
{
  evnet_socket *socket = (evnet_socket*)data;
  assert(socket->secure);
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
  flags |= MSG_DONTWAIT;
#endif
  int r = send(socket->fd, buf, len, flags);

  if (r == -1) {
    gnutls_transport_set_errno(socket->session, errno); /* necessary ? */
  }

  return r;
}

static int
secure_handshake (evnet_socket *socket)
{
  assert(socket->secure);

  int r = gnutls_handshake(socket->session);

  if (gnutls_error_is_fatal(r)) {
    socket->gnutls_errorno = r;
    return ERROR;
  }

  if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN)
    return AGAIN;

  evnet_socket_reset_timeout(socket);

  if (!socket->connected) {
    socket->connected = TRUE;
    if (socket->on_connect) socket->on_connect(socket);
  }

  if (socket->read_action == secure_handshake) {
    socket->read_action = secure_socket_recv;
  } 

  if (socket->write_action == secure_handshake) {
    socket->write_action = secure_socket_send;
  }

  return OKAY;
}

static int
secure_socket_send (evnet_socket *socket)
{
  ssize_t sent;

  if (evnet_queue_empty(&socket->out_stream)) {
    ev_io_stop(SOCKET_LOOP_ &socket->write_watcher);
    return AGAIN;
  }

  evnet_queue *q = evnet_queue_last(&socket->out_stream);
  evnet_buf *to_write = evnet_queue_data(q, evnet_buf, queue);

  assert(socket->secure);

  sent = gnutls_record_send(socket->session,
    to_write->base + to_write->written,
    to_write->len - to_write->written); 

  if (gnutls_error_is_fatal(sent)) {
    socket->gnutls_errorno = sent;
    return ERROR;
  }

  if (sent == 0) return AGAIN;

  evnet_socket_reset_timeout(socket);

  if (sent == GNUTLS_E_INTERRUPTED || sent == GNUTLS_E_AGAIN) {
    return AGAIN;
  }

  if (sent > 0) {
    /* make sure the callbacks are correct */
    if (socket->read_action)
      socket->read_action = secure_socket_recv;
    update_write_buffer_after_send(socket, sent);
    return OKAY;
  }

  assert(0 && "Unhandled return code from gnutls_record_send()!");
  return ERROR;
}

static int
secure_socket_recv (evnet_socket *socket)
{
  char recv_buffer[socket->chunksize];
  size_t recv_buffer_size = socket->chunksize;
  ssize_t recved;

  assert(socket->secure);

  recved = gnutls_record_recv(socket->session, recv_buffer, recv_buffer_size);

  //printf("secure socket recv %d %p\n", recved, socket->on_connect);

  if (gnutls_error_is_fatal(recved)) {
    socket->gnutls_errorno = recved;
    return ERROR;
  }

  if (recved == GNUTLS_E_INTERRUPTED || recved == GNUTLS_E_AGAIN)  {
    return AGAIN;
  }

  evnet_socket_reset_timeout(socket);

  /* A server may also receive GNUTLS_E_REHANDSHAKE when a client has
   * initiated a handshake. In that case the server can only initiate a
   * handshake or terminate the connection. */
  if (recved == GNUTLS_E_REHANDSHAKE) {
    if (socket->write_action) {
      socket->read_action = secure_handshake;
      socket->write_action = secure_handshake;
      return OKAY;
    } else {
      socket->read_action = full_close;
      // set error
      return ERROR;
    }
  }

  if (recved >= 0) {
    /* Got EOF */
    if (recved == 0) {
      socket->read_action = NULL;
      if (socket->write_action == NULL) CLOSE_ASAP(socket);
    }

    if (socket->write_action) {
      socket->write_action = secure_socket_send;
    }

    if (socket->on_read) {
      socket->on_read(socket, recv_buffer, recved);
    }

    return OKAY;
  }

  assert(0 && "Unhandled return code from gnutls_record_send()!");
  return ERROR;
}

static int
secure_full_goodbye (evnet_socket *socket)
{
  assert(socket->secure);

  int r = gnutls_bye(socket->session, GNUTLS_SHUT_RDWR);

  if (gnutls_error_is_fatal(r))  {
    socket->gnutls_errorno = r;
    return ERROR;
  }

  if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) return AGAIN;

  CLOSE_ASAP(socket);

  return OKAY;
}

static int
secure_half_goodbye (evnet_socket *socket)
{
  assert(socket->secure);

  int r = gnutls_bye(socket->session, GNUTLS_SHUT_WR);

  if (gnutls_error_is_fatal(r))  {
    socket->gnutls_errorno = r;
    return ERROR;
  }

  if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) return AGAIN;

  if (socket->write_action) socket->write_action = half_close;

  return OKAY;
}

void
evnet_socket_set_secure_session (evnet_socket *socket, gnutls_session_t session)
{
  socket->session = session;
  socket->secure = TRUE;
}
#endif /* HAVE GNUTLS */

static int
socket_send (evnet_socket *socket)
{
  ssize_t sent;

  assert(socket->secure == FALSE);

  if (evnet_queue_empty(&socket->out_stream)) {
    ev_io_stop(SOCKET_LOOP_ &socket->write_watcher);
    return AGAIN;
  }

  evnet_queue *q = evnet_queue_last(&socket->out_stream);
  evnet_buf *to_write = evnet_queue_data(q, evnet_buf, queue);
  
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
  flags |= MSG_DONTWAIT;
#endif

  /* TODO use writev() here */

  sent = send(socket->fd,
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
        socket->errorno = errno;
        return ERROR;
    }
  }

  evnet_socket_reset_timeout(socket);

  if (!socket->connected) {
    socket->connected = TRUE;
    if (socket->on_connect) socket->on_connect(socket);
  }

  update_write_buffer_after_send(socket, sent);

  return OKAY;
}

static int
socket_recv (evnet_socket *socket)
{
  char buf[TCP_MAXWIN];
  size_t buf_size = TCP_MAXWIN;
  ssize_t recved;

  assert(socket->secure == FALSE);

  if (!socket->connected) {
    socket->connected = TRUE;
    if (socket->on_connect) socket->on_connect(socket);
    //return OKAY;
  }

  recved = recv(socket->fd, buf, buf_size, 0);

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
        socket->errorno = errno;
        return ERROR;
    }
  }

  evnet_socket_reset_timeout(socket);

  if (recved == 0) {
    evnet_socket_read_stop(socket);
    socket->read_action = NULL;
    if (socket->write_action == NULL) CLOSE_ASAP(socket);
  }

  /* NOTE: EOF is signaled with recved == 0 on callback */
  if (socket->on_read) socket->on_read(socket, buf, recved);

  return OKAY;
}

static void
assign_file_descriptor (evnet_socket *socket, int fd)
{
  socket->fd = fd;

  ev_io_set (&socket->read_watcher, fd, EV_READ);
  ev_io_set (&socket->write_watcher, fd, EV_WRITE);

  socket->read_action = socket_recv;
  socket->write_action = socket_send;

#if EVNET_HAVE_GNUTLS
  if (socket->secure) {
    gnutls_transport_set_lowat(socket->session, 0); 
    gnutls_transport_set_push_function(socket->session, nosigpipe_push);
    gnutls_transport_set_ptr2(socket->session,
        (gnutls_transport_ptr_t)fd, /* recv */
        socket); /* send */   
    socket->read_action = secure_handshake;
    socket->write_action = secure_handshake;
  }
#endif 
}

static void
server_close_with_error (evnet_server *server, int errorno)
{
  if (server->listening) {
    evnet_server_detach(server);
    close(server->fd); /* TODO do this on the loop? check return value? */
    server->fd = -1;
    server->listening = FALSE;

    if (server->on_close) {
      server->on_close(server, errorno);
    }
  }
}


/* Retruns evnet_socket if a connection could be accepted. 
 * The returned socket is not yet attached to the event loop.
 * Otherwise NULL
 */
static evnet_socket*
accept_connection (evnet_server *server)
{
  struct sockaddr address; /* connector's address information */
  socklen_t addr_len = sizeof(address);
  
  int fd = accept(server->fd, (struct sockaddr*)&address, &addr_len);
  if (fd < 0) {
#ifdef EWOULDBLOCK
    if (errno == EWOULDBLOCK) return NULL;
#else
    if (errno == EAGAIN) return NULL;
#endif 
    goto error;
  }

  evnet_socket *socket = NULL;

  if (server->on_connection) {
    socket = server->on_connection(server, (struct sockaddr*)&address);
  }

  if (socket == NULL) {
    close(fd);
    return NULL;
  }
  
  if (set_nonblock(fd) != 0) goto error;
  
#ifdef SO_NOSIGPIPE
  flags = 1;
  r = setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &flags, sizeof(flags));
  if (r < 0) goto error;
#endif

  socket->server = server;
  assign_file_descriptor(socket, fd);

  return socket;

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
  evnet_server *server = watcher->data;

  assert(server->listening);
#if EV_MULTIPLICITY
  assert(server->loop == loop);
#endif 
  assert(&server->connection_watcher == watcher);
  
  if (EV_ERROR & revents) {
    server_close_with_error(server, 1);
    return;
  }

  /* accept as many connections as possible */
  evnet_socket *socket;
  while ((socket = accept_connection(server))) {
    evnet_socket_attach(EV_A_ socket);
  }
}

int
evnet_server_listen (evnet_server *server, struct addrinfo *addrinfo, int backlog)
{
  int fd = -1;
  assert(server->listening == FALSE);

  fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
      addrinfo->ai_protocol);
  if (fd < 0) {
    perror("socket()");
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

  if (bind(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) < 0) {
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
  server->listening = TRUE;
  ev_io_set (&server->connection_watcher, server->fd, EV_READ);
  
  return 0;
}

/**
 * Stops the server. Will not accept new connections.  Does not drop
 * existing connections.
 */
void 
evnet_server_close (evnet_server *server)
{
  server_close_with_error(server, 0);
}

void
evnet_server_attach (EV_P_ evnet_server *server)
{
  ev_io_start (EV_A_ &server->connection_watcher);
#if EV_MULTIPLICITY
  server->loop = loop;
#endif
  server->attached = TRUE;
}

void
evnet_server_detach (evnet_server *server)
{
  ev_io_stop (SERVER_LOOP_ &server->connection_watcher);
#if EV_MULTIPLICITY
  server->loop = NULL;
#endif
  server->attached = FALSE;
}

void 
evnet_server_init (evnet_server *server)
{
  server->attached = FALSE;
  server->listening = FALSE;
  server->fd = -1;
  server->connection_watcher.data = server;
  ev_init (&server->connection_watcher, on_connection);
  server->on_connection = NULL;
  server->on_close = NULL;
}

/* Internal callback. called by socket->timeout_watcher */
static void 
on_timeout (EV_P_ ev_timer *watcher, int revents)
{
  evnet_socket *socket = watcher->data;

#if EV_MULTIPLICITY
  assert(socket->loop == loop);
#endif
  assert(revents == EV_TIMEOUT);
  assert(watcher == &socket->timeout_watcher);

  // printf("on_timeout\n");

  if (socket->on_timeout) socket->on_timeout(socket);
  // timeout does not automatically kill your connection. you must!
}

static void
release_write_buffer(evnet_socket *socket)
{
  while (!evnet_queue_empty(&socket->out_stream)) {
    evnet_queue *q = evnet_queue_last(&socket->out_stream);
    evnet_buf *buf = evnet_queue_data(q, evnet_buf, queue);
    evnet_queue_remove(q);
    if (buf->release) buf->release(buf);
  }
}

/* Internal callback. called by socket->read_watcher */
static void 
on_io_event(EV_P_ ev_io *watcher, int revents)
{
  evnet_socket *socket = watcher->data;

  if (revents & EV_ERROR) {
    socket->errorno = 1;
    CLOSE_ASAP(socket);
  }

  int r;
  int have_read_event  = (socket->read_action  != NULL);
  int have_write_event = (socket->write_action != NULL);

  while (have_read_event || have_write_event) {
    /* RECV LOOP - TRY TO CLEAR THE BUFFER */
    if (socket->read_action == NULL) {
      have_read_event = FALSE;
    } else { 
      r = socket->read_action(socket);

      if (r == AGAIN) {
        have_read_event = FALSE;
      } else { 
        if (r == ERROR) CLOSE_ASAP(socket);
      }
    }

    /* SEND LOOP - TRY TO CLEAR THE BUFFER */
    if (socket->write_action == NULL) {
      have_write_event = FALSE;
    } else {
      r = socket->write_action(socket);

      if (r == AGAIN) {
        have_write_event = FALSE;
      } else {
        if (r == ERROR) CLOSE_ASAP(socket);
      }
    }
  }

  // Close when both sides of the stream are closed.
  if (socket->write_action == NULL && socket->read_action == NULL) {
    release_write_buffer(socket);

    ev_clear_pending (EV_A_ &socket->write_watcher);
    ev_clear_pending (EV_A_ &socket->read_watcher);
    ev_clear_pending (EV_A_ &socket->timeout_watcher);

    evnet_socket_detach(socket);
    assert(socket->fd == -1);

    if (socket->on_close) socket->on_close(socket);
    /* WARNING: user can free socket in on_close so no more 
     * access beyond this point. */
  }
}

/**
 * If using SSL do consider setting
 *   gnutls_db_set_retrieve_function (socket->session, _);
 *   gnutls_db_set_remove_function (socket->session, _);
 *   gnutls_db_set_store_function (socket->session, _);
 *   gnutls_db_set_ptr (socket->session, _);
 */
void 
evnet_socket_init (evnet_socket *socket, float timeout)
{
  socket->fd = -1;
  socket->server = NULL;
#if EV_MULTIPLICITY
  socket->loop = NULL;
#endif
  socket->attached = FALSE;
  socket->connected = FALSE;

  evnet_queue_init(&socket->out_stream);

  ev_init(&socket->write_watcher, on_io_event);
  socket->write_watcher.data = socket;

  ev_init(&socket->read_watcher, on_io_event);
  socket->read_watcher.data = socket;

  socket->got_full_close = FALSE;
  socket->got_half_close = FALSE;

  socket->errorno = 0;

  socket->secure = FALSE;
#if EVNET_HAVE_GNUTLS
  socket->gnutls_errorno = 0;
  socket->session = NULL;
#endif 

  ev_timer_init(&socket->timeout_watcher, on_timeout, 0., timeout);
  socket->timeout_watcher.data = socket;  

  socket->read_action = NULL;
  socket->write_action = NULL;

  socket->chunksize = 8192; 

  socket->on_connect = NULL;
  socket->on_read = NULL;
  socket->on_drain = NULL;
  socket->on_timeout = NULL;
}

void 
evnet_socket_close (evnet_socket *socket)
{
  socket->got_half_close = TRUE;
  if (evnet_queue_empty(&socket->out_stream)) {
    change_state_for_empty_out_stream(socket);
  }
}

void 
evnet_socket_full_close (evnet_socket *socket)
{
  socket->got_full_close = TRUE;
  if (evnet_queue_empty(&socket->out_stream)) {
    change_state_for_empty_out_stream(socket);
  }
}

void evnet_socket_force_close (evnet_socket *socket)
{
  release_write_buffer(socket);

  ev_clear_pending (SOCKET_LOOP_ &socket->write_watcher);
  ev_clear_pending (SOCKET_LOOP_ &socket->read_watcher);
  ev_clear_pending (SOCKET_LOOP_ &socket->timeout_watcher);

  socket->write_action = socket->read_action = NULL;
  // socket->errorno = EVNET_SOCKET_ERROR_FORCE_CLOSE
  
  if (socket->fd > 0) close(socket->fd);
  socket->fd = -1;

  evnet_socket_detach(socket);
}

void 
evnet_socket_write (evnet_socket *socket, evnet_buf *buf)
{
  if (socket->write_action == NULL) {
    assert(0 && "Do not write to a closed socket"); 
    goto error;
  }
  if (socket->got_full_close == TRUE || socket->got_half_close == TRUE) {
    assert(0 && "Do not write to a closing socket");
    goto error;
  }

  evnet_queue_insert_head(&socket->out_stream, &buf->queue);
  buf->written = 0;

  if (socket->attached) {
    ev_io_start(SOCKET_LOOP_ &socket->write_watcher);
  }
  return;

error:
  if (buf->release) buf->release(buf);
}

void 
evnet_socket_reset_timeout (evnet_socket *socket)
{
  ev_timer_again(SOCKET_LOOP_ &socket->timeout_watcher);
}

static void
free_simple_buf (evnet_buf *buf)
{
  free(buf->base);
  free(buf);
}

/* Writes a string to the socket. 
 * NOTE: Allocates memory. Avoid for performance applications.
 */ 
void
evnet_socket_write_simple (evnet_socket *socket, const char *str, size_t len)
{
  evnet_buf *buf = malloc(sizeof(evnet_buf));
  buf->release = free_simple_buf;
  buf->base = strdup(str);
  buf->len = len;

  evnet_socket_write(socket, buf);
}

void
evnet_socket_attach (EV_P_ evnet_socket *socket)
{
#if EV_MULTIPLICITY
  socket->loop = loop;
#endif 
  socket->attached = TRUE;

  ev_timer_again(EV_A_ &socket->timeout_watcher);

  if (socket->read_action) {
    ev_io_start(EV_A_ &socket->read_watcher);
  }

  if (socket->write_action) {
    ev_io_start(EV_A_ &socket->write_watcher);
  }
}

void
evnet_socket_detach (evnet_socket *socket)
{
  if (socket->attached) {
    ev_io_stop(SOCKET_LOOP_ &socket->write_watcher);
    ev_io_stop(SOCKET_LOOP_ &socket->read_watcher);
    ev_timer_stop(SOCKET_LOOP_ &socket->timeout_watcher);
#if EV_MULTIPLICITY
    socket->loop = NULL;
#endif
    socket->attached = FALSE;
  }
}

void
evnet_socket_read_stop (evnet_socket *socket)
{
  ev_io_stop(SOCKET_LOOP_ &socket->read_watcher);
  ev_clear_pending(SOCKET_LOOP_ &socket->read_watcher);
}

void
evnet_socket_read_start (evnet_socket *socket)
{
  if (socket->read_action) {
    ev_io_start(SOCKET_LOOP_ &socket->read_watcher);
    /* XXX feed event? */
  }
}

int
evnet_socket_connect (evnet_socket *s, struct addrinfo *addrinfo)
{
  int fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
      addrinfo->ai_protocol);
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
  flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &flags, sizeof(flags));
#endif

  r = connect(fd, addrinfo->ai_addr, addrinfo->ai_addrlen);

  if (r < 0 && errno != EINPROGRESS) {
    perror("connect");
    close(fd);
    return -1;
  }

  assign_file_descriptor(s, fd);

  return 0;
}

