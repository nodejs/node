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

#include <signal.h>

#include <netinet/tcp.h> /* TCP_NODELAY */
#include <sys/types.h>
#include <sys/socket.h> /* shutdown */
#include <sys/un.h>
#include <netinet/in.h> /* sockaddr_in, sockaddr_in6 */

#include <ev.h>
#include <evcom.h>

#if EV_MULTIPLICITY
# define D_LOOP_(d)  (d)->loop,
# define D_LOOP_SET(d, _loop)  do { (d)->loop = (_loop); } while (0)
#else
# define D_LOOP_(d)
# define D_LOOP_SET(d, _loop)
#endif // EV_MULTIPLICITY


/* SEND STATES */
static int stream_send__wait_for_connection (evcom_stream*);
static int stream_send__data                (evcom_stream*);
static int stream_send__drain               (evcom_stream*);
static int stream_send__wait_for_eof        (evcom_stream*);
static int stream_send__wait_for_buf        (evcom_stream*);
static int stream_send__shutdown            (evcom_stream*);
#if EVCOM_HAVE_GNUTLS
static int stream_send__gnutls_bye          (evcom_stream*);
#endif
static int stream_send__close_one           (evcom_stream*);
static int stream_send__close               (evcom_stream*);

/* RECV STATES */
static int stream_recv__data                (evcom_stream*);
static int stream_recv__wait_for_resume     (evcom_stream*);
static int stream_recv__wait_for_close      (evcom_stream*);
static int stream_recv__close_one           (evcom_stream*);
static int stream_recv__close               (evcom_stream*);

/* COMMON STATES */
#if EVCOM_HAVE_GNUTLS
static int stream__handshake                (evcom_stream*);
#endif
static int stream__close_both               (evcom_stream*);

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0
#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define OKAY  0
#define AGAIN 1

#define ATTACHED(s)         ((s)->flags & EVCOM_ATTACHED)
#define LISTENING(s)        ((s)->flags & EVCOM_LISTENING)
#define CONNECTED(s)        ((s)->flags & EVCOM_CONNECTED)
#define SECURE(s)           ((s)->flags & EVCOM_SECURE)
#define DUPLEX(s)           ((s)->flags & EVCOM_DUPLEX)
#define GOT_CLOSE(s)        ((s)->flags & EVCOM_GOT_CLOSE)
#define PAUSED(s)           ((s)->flags & EVCOM_PAUSED)
#define READABLE(s)         ((s)->flags & EVCOM_READABLE)
#define WRITABLE(s)         ((s)->flags & EVCOM_WRITABLE)

static int too_many_connections = 0;

EV_INLINE int
set_nonblock (int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return -1;

  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (r == -1) return -1;

  return 0;
}

evcom_buf *
evcom_buf_new2 (size_t len)
{
  void *data = malloc(sizeof(evcom_buf) + len);
  if (!data) return NULL;

  evcom_buf *buf = data;
  buf->len = len;
  buf->release = (void (*)(evcom_buf*))free;
  buf->base = data + sizeof(evcom_buf);

  return buf;
}

evcom_buf *
evcom_buf_new (const char *base, size_t len)
{
  evcom_buf* buf = evcom_buf_new2(len);
  if (!buf) return NULL;

  memcpy(buf->base, base, len);

  return buf;
}

void evcom_ignore_sigpipe (void)
{
  struct sigaction sa;
  bzero(&sa, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, NULL);
}

static int
close_asap (evcom_descriptor *d)
{
  if (d->fd < 0) return OKAY;

  int r = close(d->fd);

  if (r < 0) {
    if (errno == EINTR) {
      d->action = close_asap;
      return AGAIN;
    } else {
      d->errorno = errno;
    }
  }

  d->action = NULL;
  d->fd = -1;

  return OKAY;
}

#define release_write_buffer(writer) \
do { \
  while (!evcom_queue_empty(&(writer)->out)) { \
    evcom_queue *q = evcom_queue_last(&(writer)->out); \
    evcom_buf *buf = evcom_queue_data(q, evcom_buf, queue); \
    evcom_queue_remove(q); \
    if (buf->release) buf->release(buf); \
  } \
} while (0)

static int
close_writer_asap (evcom_writer *writer)
{
  release_write_buffer(writer);
  ev_feed_event(D_LOOP_(writer) &writer->write_watcher, EV_WRITE);
  return close_asap((evcom_descriptor*)writer);
}

static inline void
evcom_perror (const char *msg, int errorno)
{
  fprintf(stderr, "(evcom) %s %s\n", msg, strerror(errorno));
}

static int
stream_send__wait_for_buf (evcom_stream *stream)
{
  if (evcom_queue_empty(&stream->out)) {
    if (GOT_CLOSE(stream)) {
      stream->send_action = stream_send__drain;
      return OKAY;
    }
    ev_io_stop(D_LOOP_(stream) &stream->write_watcher);
    return AGAIN;
  }

  stream->send_action = stream_send__data;
  return OKAY;
}

static inline void
stream__set_recv_closed (evcom_stream *stream)
{
  stream->flags &= ~EVCOM_READABLE;
  stream->recvfd = -1;
  stream->recv_action = NULL;
  ev_io_stop(D_LOOP_(stream) &stream->read_watcher);
}

static inline void
stream__set_send_closed (evcom_stream *stream)
{
  release_write_buffer(stream);
  stream->flags &= ~EVCOM_WRITABLE;
  stream->sendfd = -1;
  stream->send_action = NULL;
  ev_io_stop(D_LOOP_(stream) &stream->write_watcher);
}

static int
stream_send__close_one (evcom_stream *stream)
{
  assert(stream->sendfd >= 0);

  close(stream->sendfd);

  /* TODO recover from EINTR */

  stream__set_send_closed(stream);

  if (DUPLEX(stream) || stream->recv_action == stream_recv__wait_for_close) {
    stream__set_recv_closed(stream);
  }

  return OKAY;
}

static int
stream__close_both (evcom_stream *stream)
{
  assert(stream->sendfd != stream->recvfd);

  assert(stream->sendfd >= 0);
  assert(stream->recvfd >= 0);

  close(stream->recvfd);
  close(stream->sendfd);

  /* TODO recover from EINTR */

  stream__set_send_closed(stream);
  stream__set_recv_closed(stream);

  return OKAY;
}

static int
stream_send__close (evcom_stream *stream)
{
  if (DUPLEX(stream) || stream->recvfd < 0) {
    stream->send_action = stream_send__close_one;
  } else {
    stream->send_action = stream__close_both;
  }
  return OKAY;
}

static int
stream_recv__close_one (evcom_stream *stream)
{
  assert(stream->recvfd >= 0);

  close(stream->recvfd);

  /* TODO recover from EINTR */

  stream__set_recv_closed(stream);

  if (DUPLEX(stream)) {
    stream__set_send_closed(stream);
  }

  return OKAY;
}

static int
stream_recv__close (evcom_stream *stream)
{
  if (DUPLEX(stream) || stream->sendfd < 0) {
    stream->recv_action = stream_recv__close_one;
  } else {
    stream->recv_action = stream__close_both;
  }
  return OKAY;
}

static int
stream_send__drain (evcom_stream *stream)
{
  if (stream->on_drain) {
    stream->on_drain(stream);
  }

  if (!GOT_CLOSE(stream)) {
    stream->send_action = stream_send__wait_for_buf;
    return OKAY;
  }

#if EVCOM_HAVE_GNUTLS
  if (SECURE(stream)) {
    stream->send_action = stream_send__gnutls_bye;
    return OKAY;
  }
#endif

  if (DUPLEX(stream)) {
    stream->send_action = stream_send__shutdown;
    return OKAY;
  }

  stream->send_action = stream_send__close_one;
  return OKAY;
}

static int
stream_send__wait_for_eof (evcom_stream *stream)
{
  if (READABLE(stream)) {
    ev_io_stop(D_LOOP_(stream) &stream->write_watcher);
    assert(stream->send_action == stream_send__wait_for_eof);
    return AGAIN;
  }

  stream->send_action = stream_send__close_one;
  return OKAY;
}

static inline ssize_t
nosigpipe_send (int fd, const void *buf, size_t len)
{
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
  flags |= MSG_DONTWAIT;
#endif
  return send(fd, buf, len, flags);
}

static inline ssize_t
nosigpipe_stream_send (evcom_stream *stream, const void *buf, size_t len)
{
  return write(stream->sendfd, buf, len);
}

#if EVCOM_HAVE_GNUTLS
static ssize_t
nosigpipe_push (gnutls_transport_ptr_t data, const void *buf, size_t len)
{
  evcom_stream *stream = (evcom_stream*)data;
  assert(SECURE(stream));

  return nosigpipe_stream_send(stream, buf, len);
}

static ssize_t
pull (gnutls_transport_ptr_t data, void* buf, size_t len)
{
  evcom_stream *stream = (evcom_stream*)data;
  assert(SECURE(stream));

  return read(stream->recvfd, buf, len);
}

static int
stream__handshake (evcom_stream *stream)
{
  assert(SECURE(stream));

  int r = gnutls_handshake(stream->session);

  if (gnutls_error_is_fatal(r)) {
    stream->gnutls_errorno = r;
    stream->send_action = stream_send__close;
    stream->recv_action = stream_recv__close;
    return OKAY;
  }

  ev_timer_again(D_LOOP_(stream) &stream->timeout_watcher);

  if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
    if (0 == gnutls_record_get_direction((stream)->session)) {
      ev_io_start(D_LOOP_(stream) &(stream)->read_watcher); 
      ev_io_stop(D_LOOP_(stream) &(stream)->write_watcher); 
    } else {
      ev_io_stop(D_LOOP_(stream) &(stream)->read_watcher); 
      ev_io_start(D_LOOP_(stream) &(stream)->write_watcher);
    }
    assert(stream->recv_action == stream__handshake);
    assert(stream->send_action == stream__handshake);
    return AGAIN;
  }

  assert(!CONNECTED(stream));
  stream->flags |= EVCOM_CONNECTED;
  if (stream->on_connect) stream->on_connect(stream);

  /* evcom_stream_force_close might have been called. */
  if (stream->recvfd >= 0 && stream->sendfd >= 0) {
    ev_io_start(D_LOOP_(stream) &stream->read_watcher);
    ev_io_start(D_LOOP_(stream) &stream->write_watcher);

    stream->send_action = stream_send__data;
    stream->recv_action = stream_recv__data;
  }

  return OKAY;
}

static int
stream_send__gnutls_bye (evcom_stream *stream)
{
  assert(SECURE(stream));

  int r = gnutls_bye(stream->session, GNUTLS_SHUT_WR);

  if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
    assert(1 == gnutls_record_get_direction((stream)->session));
    assert(stream->send_action == stream_send__gnutls_bye);
    return AGAIN;
  }

  if (gnutls_error_is_fatal(r)) {
    stream->gnutls_errorno = r;
    stream->send_action = stream_send__close;
    return OKAY;
  }

  stream->flags &= ~EVCOM_WRITABLE;

  stream->send_action = stream_send__wait_for_eof;
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
stream_recv__wait_for_close (evcom_stream *stream)
{
  assert(!READABLE(stream));

  if (!WRITABLE(stream)) {
    stream->recv_action = stream_recv__close;
    return OKAY;
  }

  ev_io_stop(D_LOOP_(stream) &stream->read_watcher);
  return AGAIN;
}

static int
stream_recv__wait_for_resume (evcom_stream *stream)
{
  stream->flags |= EVCOM_PAUSED;
  ev_io_stop(D_LOOP_(stream) &stream->read_watcher);
  assert(stream->recv_action == stream_recv__wait_for_resume);
  return AGAIN;
}

static int
stream_recv__data (evcom_stream *stream)
{
  char buf[EVCOM_CHUNKSIZE];
  size_t buf_size = EVCOM_CHUNKSIZE;
  ssize_t recved;

  while (READABLE(stream)) {
    assert(CONNECTED(stream));

    if (PAUSED(stream)) {
      stream->recv_action = stream_recv__wait_for_resume;
      return OKAY;
    }


#if EVCOM_HAVE_GNUTLS
    if (SECURE(stream)) {
      recved = gnutls_record_recv(stream->session, buf, buf_size);

      if (gnutls_error_is_fatal(recved)) {
        stream->gnutls_errorno = recved;
        stream->recv_action = stream_recv__close;
        return OKAY;
      }

      if (recved == GNUTLS_E_INTERRUPTED || recved == GNUTLS_E_AGAIN) {
        if (1 == gnutls_record_get_direction((stream)->session)) {
          fprintf(stderr, "(evcom) gnutls recv: unexpected switch direction!\n");
          ev_io_stop(D_LOOP_(stream) &(stream)->read_watcher); 
          ev_io_start(D_LOOP_(stream) &(stream)->write_watcher);
        }
        return AGAIN;
      }

      /* A server may also receive GNUTLS_E_REHANDSHAKE when a client has
       * initiated a andshake. In that case the server can only initiate a
       * handshake or terminate the connection. */
      if (recved == GNUTLS_E_REHANDSHAKE) {
        assert(WRITABLE(stream));
        stream->recv_action = stream__handshake;
        stream->send_action = stream__handshake;
        return OKAY;
      }
    } else 
#endif /* EVCOM_HAVE_GNUTLS */
    {
      recved = read(stream->recvfd, buf, buf_size);
    }

    if (recved < 0) {
      if (errno == EAGAIN || errno == EINTR) {
        assert(stream->recv_action == stream_recv__data);
        return AGAIN;
      } 
      
      if (errno != ECONNRESET) {
        evcom_perror("recv()", stream->errorno);
      }

      stream->errorno = errno;
      stream->recv_action = stream_recv__close;
      return OKAY;
    }

    ev_timer_again(D_LOOP_(stream) &stream->timeout_watcher);

    assert(recved >= 0);

    if (recved == 0) {
      stream->flags &= ~EVCOM_READABLE;
      ev_io_stop(D_LOOP_(stream) &stream->read_watcher);
      stream->recv_action = stream_recv__wait_for_close;
    }

    /* NOTE: EOF is signaled with recved == 0 on callback */
    if (stream->on_read) stream->on_read(stream, buf, recved);

    if (recved == 0) {
      return OKAY;
    }
  }
  return AGAIN;
}

static int
stream_send__data (evcom_stream *stream)
{
  ssize_t sent;

  while (!evcom_queue_empty(&stream->out)) {
    assert(WRITABLE(stream));

    evcom_queue *q = evcom_queue_last(&stream->out);
    evcom_buf *buf = evcom_queue_data(q, evcom_buf, queue);

#if EVCOM_HAVE_GNUTLS
    if (SECURE(stream)) {
      sent = gnutls_record_send(stream->session,
          buf->base + buf->written,
          buf->len - buf->written);

      if (sent == GNUTLS_E_INTERRUPTED || sent == GNUTLS_E_AGAIN) {
        if (0 == gnutls_record_get_direction((stream)->session)) {
          fprintf(stderr, "(evcom) gnutls send: unexpected switch direction!\n");
          ev_io_start(D_LOOP_(stream) &(stream)->read_watcher); 
          ev_io_stop(D_LOOP_(stream) &(stream)->write_watcher);
        }
        return AGAIN;
      }

      if (gnutls_error_is_fatal(sent)) {
        stream->gnutls_errorno = sent;
        stream->send_action = stream_send__close;
        return OKAY;
      }
    } else
#endif // EVCOM_HAVE_GNUTLS
    {
      /* TODO use writev() here? */
      sent = nosigpipe_stream_send(stream,
          buf->base + buf->written,
          buf->len - buf->written);
    }

    if (sent <= 0) {
      switch (errno) {
        case EAGAIN:
        case EINTR:
          assert(stream->send_action == stream_send__data);
          return AGAIN;

        default:
          stream->errorno = errno;
          evcom_perror("send()", errno);
          /* pass through */
        case EPIPE:
          stream->send_action = stream_send__close;
          return OKAY;
      }
    }

    ev_timer_again(D_LOOP_(stream) &stream->timeout_watcher);

    assert(sent >= 0);

    buf->written += sent;

    if (buf->written == buf->len) {
      evcom_queue_remove(q);
      if (buf->release) buf->release(buf);
    }
  }

  assert(evcom_queue_empty(&stream->out));

  stream->send_action = stream_send__drain;
  return OKAY;
}

static int
stream_send__shutdown (evcom_stream *stream)
{
  int r = shutdown(stream->sendfd, SHUT_WR);

  if (r < 0) {
    switch (errno) {
      case EINTR:
        assert(stream->send_action == stream_send__shutdown);
        return OKAY;

      case ENOTCONN:
        break;

      default: 
        stream->errorno = errno;
        evcom_perror("shutdown()", errno);
        break;
    }
    stream->send_action = stream_send__close;
    return OKAY;
  }

  stream->flags &= ~EVCOM_WRITABLE;
  stream->send_action = stream_send__wait_for_eof;
  return OKAY;
}

static int
stream__connection_established (evcom_stream *stream)
{
  assert(!CONNECTED(stream));

#if EVCOM_HAVE_GNUTLS
  if (SECURE(stream)) {
    stream->send_action = stream__handshake;
    stream->recv_action = stream__handshake;
  } else
#endif
  {
    stream->flags |= EVCOM_CONNECTED;
    if (stream->on_connect) stream->on_connect(stream);

    stream->send_action = stream_send__data;
    stream->recv_action = stream_recv__data;
  }

  ev_io_start(D_LOOP_(stream) &stream->write_watcher);
  ev_io_start(D_LOOP_(stream) &stream->read_watcher);

  return OKAY;
}

static int
stream_send__wait_for_connection (evcom_stream *stream)
{
  assert(DUPLEX(stream));

  int connect_error;
  socklen_t len = sizeof(int);
  int r = getsockopt(stream->sendfd, SOL_SOCKET, SO_ERROR, &connect_error, &len);

  if (r < 0) {
    stream->errorno = r;
    stream->send_action = stream_send__close;
    return OKAY;
  }

  if (connect_error == 0) {
    stream->send_action = stream__connection_established;
    return OKAY;

  } else if (connect_error == EINPROGRESS || connect_error == EINTR) {
    assert(stream->send_action == stream_send__wait_for_connection);
    return AGAIN;
  }

  stream->errorno = connect_error;
  stream->send_action = stream_send__close;
  return OKAY;
}

void
evcom_stream_assign_fds (evcom_stream *stream, int recvfd, int sendfd)
{
  assert(recvfd >= 0);
  assert(sendfd >= 0);

  if (recvfd == sendfd) stream->flags |= EVCOM_DUPLEX;

  if (set_nonblock(recvfd) != 0) {
    evcom_perror("set_nonblock(recvfd)", errno);
  }

  if (set_nonblock(sendfd) != 0) {
    evcom_perror("set_nonblock(sendfd)", errno);
  }

#ifdef SO_NOSIGPIPE
  if (DUPLEX(stream)) {
    int flags = 1;
    int r = setsockopt(sendfd, SOL_SOCKET, SO_NOSIGPIPE, &flags, sizeof(flags));
    if (r < 0) {
      evcom_perror("setsockopt(SO_NOSIGPIPE)", errno);
    }
  }
#endif

  ev_io_set(&stream->read_watcher, recvfd, EV_READ);
  ev_io_set(&stream->write_watcher, sendfd, EV_WRITE);

  stream->recvfd = recvfd;
  stream->sendfd = sendfd;

  stream->send_action = stream__connection_established;
  stream->recv_action = stream__connection_established;

  stream->flags |= EVCOM_READABLE;
  stream->flags |= EVCOM_WRITABLE;

#if EVCOM_HAVE_GNUTLS
  if (SECURE(stream)) {
    gnutls_transport_set_lowat(stream->session, 0);
    gnutls_transport_set_push_function(stream->session, nosigpipe_push);
    gnutls_transport_set_pull_function(stream->session, pull);
    gnutls_transport_set_ptr2(stream->session, stream, stream);
  }
#endif
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

  int fd = accept(server->fd, &address, &addr_len);
  if (fd < 0) {
    switch (errno) {
      case EMFILE:
      case ENFILE:
        too_many_connections = 1;
        server->flags |= EVCOM_TOO_MANY_CONN;
        evcom_server_detach(server);
        return NULL;

      case EINTR:
      case EAGAIN:
        return NULL;

      default:
        evcom_perror("accept()", errno);
        return NULL;
    }
    assert(0 && "no reach");
  }

  evcom_stream *stream = NULL;

  if (server->on_connection) {
    stream = server->on_connection(server, &address);
  }

  if (stream == NULL) {
    close(fd);
    return NULL;
  }

  if (set_nonblock(fd) != 0) {
    evcom_perror("set_nonblock()", errno);
    return NULL;
  }

  stream->server = server;
  evcom_stream_assign_fds(stream, fd, fd);

  return stream;
}

/* Internal callback
 * Called by server->watcher.
 */
static int
accept_connections (evcom_descriptor *d)
{
  evcom_server *server = (evcom_server *)d;

  assert(LISTENING(server));

  /* accept as many connections as possible */
  evcom_stream *stream;
  while (server->fd >= 0 && (stream = accept_connection(server))) {
    evcom_stream_attach(D_LOOP_(server) stream);
  }

  return AGAIN;
}

static inline socklen_t
address_length (struct sockaddr *address)
{
  struct sockaddr_un* unix_address = (struct sockaddr_un*)address;
  int length = 0;

  switch (address->sa_family) {
    case AF_INET:
      length = sizeof(struct sockaddr_in);
      break;
    case AF_INET6:
      length = sizeof(struct sockaddr_in6);
      break;
    case AF_UNIX:
#ifdef SUN_LEN
      length = SUN_LEN(unix_address);
#else
      length = strlen(unix_address->sun_path) + sizeof(unix_address->sun_family);
#endif
      break;

    default:
      assert(0 && "Unsupported socket family");
  }
  return length;
}

int
evcom_server_listen (evcom_server *server, struct sockaddr *address, int backlog)
{
  assert(!LISTENING(server));

  int fd = socket(address->sa_family, SOCK_STREAM, 0);
  if (fd < 0) {
    server->errorno = errno;
    evcom_perror("socket()", errno);
    return -1;
  }

  server->fd = fd;
  ev_io_set(&server->watcher, server->fd, EV_READ);

  if (set_nonblock(fd) != 0) {
    server->errorno = errno;
    evcom_perror("set_nonblock()", errno);
    close(fd);
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
    server->errorno = errno;
    evcom_perror("bind()", errno);
    close(fd);
    return -1;
  }

  if (listen(fd, backlog) < 0) {
    server->errorno = errno;
    evcom_perror("listen()", errno);
    close(fd);
    return -1;
  }

  server->flags |= EVCOM_LISTENING;
  server->action = accept_connections;

  return 0;
}

/**
 * Stops the server. Will not accept new connections.  Does not drop
 * existing connections.
 */
void
evcom_server_close (evcom_server *server)
{
  ev_io_start(D_LOOP_(server) &server->watcher);
  ev_feed_event(D_LOOP_(server) &server->watcher, EV_READ);

  close_asap((evcom_descriptor*)server);
}

void
evcom_server_attach (EV_P_ evcom_server *server)
{
  ev_io_start (EV_A_ &server->watcher);
  D_LOOP_SET(server, EV_A);
  server->flags |= EVCOM_ATTACHED;
}

void
evcom_server_detach (evcom_server *server)
{
  ev_io_stop (D_LOOP_(server) &server->watcher);
  D_LOOP_SET(server, NULL);
  server->flags &= ~EVCOM_ATTACHED;
}

static void
io_event(EV_P_ ev_io *watcher, int revents)
{
  evcom_descriptor *d = watcher->data;

#if EV_MULTIPLICITY
  assert(d->loop == loop);
#endif

  int r = OKAY;

  if (revents & EV_ERROR) {
    d->errorno = 1;
    r = close_asap(d);
  }

  while (r == OKAY && d->action && d->fd >= 0) {
    r = d->action(d);
  }

  if (d->fd < 0) {
    assert(d->action == NULL);
    ev_clear_pending(EV_A_ watcher);
    ev_io_stop(EV_A_ watcher);
    if (d->on_close) d->on_close(d);
  }
}

static void
evcom_descriptor_init (evcom_descriptor *d)
{
  d->fd = -1;
  D_LOOP_SET(d, NULL);
  d->flags = 0;
  d->errorno = 0;
  d->action = NULL;
}

void
evcom_server_init (evcom_server *server)
{
  evcom_descriptor_init((evcom_descriptor*)server);
  ev_init (&server->watcher, io_event);
  server->watcher.data = server;
  server->on_connection = NULL;
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

  if (PAUSED(stream)) {
    ev_timer_again(D_LOOP_(stream) &stream->timeout_watcher);
    return;
  }

  if (stream->on_timeout) stream->on_timeout(stream);

  // Hack to get error in Node on 'close' event.
  // should probably be made into a proper error code.
  stream->errorno = 1;

  ev_timer_stop(EV_A_ watcher);
  evcom_stream_force_close(stream);

  if (stream->on_close) stream->on_close(stream);
}

static void 
stream_event (EV_P_ ev_io *w, int revents)
{
  evcom_stream *stream = w->data;

  if (revents & EV_READ) {
    while (stream->recv_action) {
      int r = stream->recv_action(stream);
      if (r == AGAIN) break;
    }
  }

  if (revents & EV_WRITE) {
    while (stream->send_action) {
      int r = stream->send_action(stream);
      if (r == AGAIN) break;
    }
  }

  if (stream->send_action == NULL) {
    ev_io_stop(EV_A_ &stream->write_watcher);
  }

  if (stream->recv_action == NULL) {
    ev_io_stop(EV_A_ &stream->read_watcher);
  }

  if (stream->sendfd < 0 && stream->recvfd < 0) {
    ev_timer_stop(EV_A_ &stream->timeout_watcher);

    if (stream->server && (stream->server->flags & EVCOM_TOO_MANY_CONN)) {
#if EV_MULTIPLICITY
      struct ev_loop *loop = stream->server->loop;
#endif
      stream->server->flags &= ~EVCOM_TOO_MANY_CONN;
      evcom_server_attach(EV_A_ stream->server);
    }
    too_many_connections = 0;

    if (stream->on_close) stream->on_close(stream);
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
evcom_stream_init (evcom_stream *stream)
{
  stream->flags = 0;
  stream->errorno = 0;
  stream->recvfd = -1;
  stream->sendfd = -1;

  // reader things
  ev_init(&stream->read_watcher, stream_event);
  stream->read_watcher.data = stream;
  stream->recv_action = NULL;

  // writer things
  ev_init(&stream->write_watcher, stream_event);
  stream->write_watcher.data = stream;
  evcom_queue_init(&stream->out);
  stream->send_action = NULL;

  // stream things
  stream->server = NULL;
#if EVCOM_HAVE_GNUTLS
  stream->gnutls_errorno = 0;
  stream->session = NULL;
#endif
  ev_timer_init(&stream->timeout_watcher, on_timeout, 0., 60.);
  stream->timeout_watcher.data = stream;

  stream->on_connect = NULL;
  stream->on_timeout = NULL;
  stream->on_read = NULL;
  stream->on_drain = NULL;
  stream->on_close = NULL;
}

void
evcom_stream_close (evcom_stream *stream)
{
  stream->flags |= EVCOM_GOT_CLOSE;
  if (ATTACHED(stream)) {
    // start the watchers if attached.
    evcom_stream_attach(D_LOOP_(stream) stream); 
  }
}

void evcom_stream_force_close (evcom_stream *stream)
{
  if (stream->recvfd >= 0) {
    close(stream->recvfd);
    /* XXX What to do on EINTR? */
    stream__set_recv_closed(stream);
  }

  if (!DUPLEX(stream) && stream->sendfd >= 0) {
    close(stream->sendfd);
  }
  stream__set_send_closed(stream);

  evcom_stream_detach(stream);
}

/* Returns the number of bytes flushed to the buffer */
ssize_t
evcom_stream_write (evcom_stream *stream, const char *str, size_t len)
{
  if (!WRITABLE(stream) || GOT_CLOSE(stream)) {
    assert(0 && "Do not write to a closed stream");
    return -1;
  }

  ssize_t sent = 0;

  if ( stream->send_action == stream_send__wait_for_buf
    && evcom_queue_empty(&stream->out)
     ) 
  {
    assert(CONNECTED(stream));
#if EVCOM_HAVE_GNUTLS
    if (SECURE(stream)) {
      sent = gnutls_record_send(stream->session, str, len);

      if (gnutls_error_is_fatal(sent)) {
        stream->gnutls_errorno = sent;
        goto close;
      }
    } else
#endif // EVCOM_HAVE_GNUTLS
    {
      /* TODO use writev() here? */
      sent = nosigpipe_stream_send(stream, str, len);
    }

    if (sent < 0) {
      switch (errno) {
        case EPIPE:
          goto close;

        case EINTR:
        case EAGAIN:
          sent = 0;
          break;

        default:
          stream->errorno = errno;
          evcom_perror("send()", stream->errorno);
          goto close;
      }
    }
  } /* TODO else { memcpy to last buffer on head } */

  assert(sent >= 0);
  if ((size_t)sent == len) return sent; /* sent the whole buffer */

  len -= sent;
  str += sent;

  evcom_buf *b = evcom_buf_new(str, len);
  evcom_queue_insert_head(&stream->out, &b->queue);
  b->written = 0;

  assert(stream->sendfd >= 0);

  if (ATTACHED(stream)) {
    ev_io_start(D_LOOP_(stream) &stream->write_watcher);
  }
  return sent;

close:
  stream->send_action = stream_send__close;
  stream->recv_action = stream_recv__close;
  if (ATTACHED(stream)) {
    ev_io_start(D_LOOP_(stream) &stream->write_watcher);
  }
  return -1;
}

void
evcom_stream_reset_timeout (evcom_stream *stream, float timeout)
{
  stream->timeout_watcher.repeat = timeout;
  if (ATTACHED(stream)) {
    ev_timer_again(D_LOOP_(stream) &stream->timeout_watcher);
  }
}

void
evcom_stream_set_no_delay (evcom_stream *stream, int no_delay)
{
  if (DUPLEX(stream)) {
    int flags = no_delay ? 1 : 0;
    setsockopt(stream->recvfd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));
  }
}

void
evcom_stream_attach (EV_P_ evcom_stream *stream)
{
  D_LOOP_SET(stream, EV_A);
  stream->flags |= EVCOM_ATTACHED;

  ev_timer_again(EV_A_ &stream->timeout_watcher);

  if (READABLE(stream)) {
    ev_io_start(EV_A_ &stream->read_watcher);
  }

  if (WRITABLE(stream)) {
    ev_io_start(EV_A_ &stream->write_watcher);
  }
}

void
evcom_stream_detach (evcom_stream *stream)
{
  ev_io_stop(D_LOOP_(stream) &stream->write_watcher);
  ev_io_stop(D_LOOP_(stream) &stream->read_watcher);
  ev_timer_stop(D_LOOP_(stream) &stream->timeout_watcher);
  D_LOOP_SET(stream, NULL);
  stream->flags &= ~EVCOM_ATTACHED;
}

void
evcom_stream_read_pause (evcom_stream *stream)
{
  stream->flags |= EVCOM_PAUSED;
  ev_timer_stop(D_LOOP_(stream) &stream->timeout_watcher);
  if (stream->recv_action == stream_recv__data) {
    ev_io_stop(D_LOOP_(stream) &stream->read_watcher);
    stream->recv_action = stream_recv__wait_for_resume;
  }
}

void
evcom_stream_read_resume (evcom_stream *stream)
{
  stream->flags &= ~EVCOM_PAUSED;
  ev_timer_again(D_LOOP_(stream) &stream->timeout_watcher);
  if (stream->recv_action == stream_recv__wait_for_resume) {
    stream->recv_action = stream_recv__data;
  }
  if (ATTACHED(stream)) {
    ev_timer_again(D_LOOP_(stream) &stream->timeout_watcher);
    if (READABLE(stream)) ev_io_start(D_LOOP_(stream) &stream->read_watcher);
  }
}

int
evcom_stream_connect (evcom_stream *stream, struct sockaddr *address)
{
  int fd = socket(address->sa_family, SOCK_STREAM, 0);

  if (fd < 0) {
    evcom_perror("socket()", errno);
    return -1;
  }

  int r = set_nonblock(fd);
  if (r < 0) {
    stream->errorno = errno;
    evcom_perror("set_nonblock()", errno);
    close(fd);
    return -1;
  }

  r = connect(fd, address, address_length(address));

  if (r < 0 && errno != EINPROGRESS) {
    stream->errorno = errno;
    evcom_perror("connect", errno);
    close(fd);
    return -1;
  }

  evcom_stream_assign_fds(stream, fd, fd);

  stream->send_action = stream_send__wait_for_connection;
  stream->recv_action = NULL;

  return 0;
}

int evcom_stream_pair (evcom_stream *a, evcom_stream *b)
{
  int sv[2];
  int old_errno;

  int r = socketpair(PF_UNIX, SOCK_STREAM, 0, sv);
  if (r < 0) return -1;

  r = set_nonblock(sv[0]);
  if (r < 0) goto set_nonblock_error;
  r = set_nonblock(sv[1]);
  if (r < 0) goto set_nonblock_error;

  evcom_stream_assign_fds(a, sv[0], sv[0]);
  evcom_stream_assign_fds(b, sv[1], sv[1]);

  return 0;

set_nonblock_error:
  old_errno = errno;
  evcom_perror("set_nonblock()", errno);
  close(sv[0]);
  close(sv[1]);
  errno = old_errno;
  return -1;
}

enum evcom_stream_state
evcom_stream_state (evcom_stream *stream)
{
  if (stream->recvfd < 0 && stream->sendfd && stream->flags == 0) {
    return EVCOM_INITIALIZED;
  }

  if (stream->recvfd < 0 && stream->sendfd < 0) return EVCOM_CLOSED;

  if (!CONNECTED(stream)) return EVCOM_CONNECTING;

  if (GOT_CLOSE(stream)) {
    if (READABLE(stream)) {
      return EVCOM_CONNECTED_RO;
    } else {
      return EVCOM_CLOSING;
    }
  }

  if (READABLE(stream) && WRITABLE(stream)) return EVCOM_CONNECTED_RW;

  if (WRITABLE(stream)) return EVCOM_CONNECTED_WO;

  if (READABLE(stream)) return EVCOM_CONNECTED_RO;

  return EVCOM_CLOSING;
}

static int
reader_recv (evcom_descriptor *d)
{
  evcom_reader* reader = (evcom_reader*) d;

  char buf[EVCOM_CHUNKSIZE];
  size_t buf_size = EVCOM_CHUNKSIZE;
  ssize_t recved;

  while (reader->fd >= 0) {
    recved = read(reader->fd, buf, buf_size);

    if (recved < 0) {
      if (errno == EAGAIN || errno == EINTR) return AGAIN;
      reader->errorno = errno;
      evcom_perror("read()", reader->errorno);
      return close_asap(d);
    }

    /* NOTE: EOF is signaled with recved == 0 on callback */
    if (reader->on_read) reader->on_read(reader, buf, recved);

    if (recved == 0) return close_asap(d);
  }
  return AGAIN;
}

void
evcom_reader_init (evcom_reader *reader)
{
  evcom_descriptor_init((evcom_descriptor*)reader);

  reader->on_close = NULL;
  reader->on_read = NULL;

  ev_init(&reader->read_watcher, io_event);
  reader->read_watcher.data = reader;
}

void
evcom_reader_set (evcom_reader *reader, int fd)
{
  assert(fd >= 0);
  reader->fd = fd;

  ev_io_set(&reader->read_watcher, fd, EV_READ);
  reader->action = reader_recv;
}

void
evcom_reader_attach (EV_P_ evcom_reader *reader)
{
  ev_io_start(EV_A_ &reader->read_watcher);
  D_LOOP_SET(reader, EV_A);
}

void
evcom_reader_detach (evcom_reader *reader)
{
  ev_io_stop(D_LOOP_(reader) &reader->read_watcher);
  D_LOOP_SET(reader, NULL);
}

void
evcom_reader_close (evcom_reader *reader)
{
  ev_io_start(D_LOOP_(reader) &reader->read_watcher);
  ev_feed_event(D_LOOP_(reader) &reader->read_watcher, EV_READ);

  close_asap((evcom_descriptor*)reader);
}

static int
writer_send (evcom_descriptor *d)
{
  evcom_writer* writer = (evcom_writer*) d;
  assert(writer->fd >= 0);

  while (!evcom_queue_empty(&writer->out)) {
    evcom_queue *q = evcom_queue_last(&writer->out);
    evcom_buf *buf = evcom_queue_data(q, evcom_buf, queue);

    ssize_t sent = write(writer->fd, buf->base + buf->written,
        buf->len - buf->written);

    if (sent < 0) {
      switch (errno) {
        case ECONNRESET:
        case EPIPE:
          return close_writer_asap(writer);

        case EINTR:
        case EAGAIN:
          sent = 0;
          return AGAIN;

        default:
          writer->errorno = errno;
          evcom_perror("send()", writer->errorno);
          return close_writer_asap(writer);
      }
    }
    assert(sent >= 0);

    buf->written += sent;

    if (buf->written == buf->len) {
      evcom_queue_remove(q);
      if (buf->release) buf->release(buf);
    }
  }

  if (GOT_CLOSE(writer)) {
    assert(evcom_queue_empty(&writer->out));
    return close_writer_asap(writer);
  } else {
    ev_io_stop(D_LOOP_(writer) &writer->write_watcher);
    return AGAIN;
  }
}

void
evcom_writer_init (evcom_writer* writer)
{
  evcom_descriptor_init((evcom_descriptor*)writer);

  writer->on_close = NULL;

  ev_init(&writer->write_watcher, io_event);
  writer->write_watcher.data = writer;

  evcom_queue_init(&writer->out);
}

void
evcom_writer_set (evcom_writer* writer, int fd)
{
  assert(fd >= 0);
  writer->fd = fd;

  ev_io_set(&writer->write_watcher, fd, EV_WRITE);
  writer->action = writer_send;
}

void
evcom_writer_attach (EV_P_ evcom_writer* writer)
{
  if (!evcom_queue_empty(&writer->out)) {
    ev_io_start (EV_A_ &writer->write_watcher);
  }
  D_LOOP_SET(writer, EV_A);
}

void
evcom_writer_detach (evcom_writer* writer)
{
  ev_io_stop(D_LOOP_(writer) &writer->write_watcher);
  D_LOOP_SET(writer, NULL);
}

void
evcom_writer_write (evcom_writer* writer, const char* buf, size_t len)
{
  assert(writer->fd >= 0);

  ssize_t sent = 0;

  if (evcom_queue_empty(&writer->out)) {
    sent = write(writer->fd, buf, len);

    if (sent < 0) {
      switch (errno) {
        case ECONNRESET:
        case EPIPE:
          goto close;

        case EINTR:
        case EAGAIN:
          sent = 0;
          break;

        default:
          writer->errorno = errno;
          evcom_perror("send()", writer->errorno);
          goto close;
      }
    }
  } /* TODO else { memcpy to last buffer on head } */

  assert(sent >= 0);
  if ((size_t)sent == len) return; /* sent the whole buffer */

  len -= sent;
  buf += sent;

  evcom_buf *b = evcom_buf_new(buf, len);
  evcom_queue_insert_head(&writer->out, &b->queue);
  b->written = 0;

  assert(writer->fd >= 0);
  ev_io_start(D_LOOP_(writer) &writer->write_watcher);
  return;

close:
  close_writer_asap(writer);
}

void
evcom_writer_close (evcom_writer* writer)
{
  writer->flags |= EVCOM_GOT_CLOSE;
  if (evcom_queue_empty(&writer->out)) close_writer_asap(writer);
}
