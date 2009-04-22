#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h> /* close() */
#include <fcntl.h>  /* fcntl() */
#include <errno.h> /* for the default methods */
#include <string.h> /* memset */

#include <netinet/tcp.h> /* TCP_NODELAY */

#include <ev.h>
#include <oi_socket.h>

#if HAVE_GNUTLS
# include <gnutls/gnutls.h>
# define GNUTLS_NEED_WRITE (gnutls_record_get_direction(socket->session) == 1)
# define GNUTLS_NEED_READ (gnutls_record_get_direction(socket->session) == 0)
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

#define RAISE_ERROR(s, _domain, _code) do { \
  if(s->on_error) { \
    struct oi_error __oi_error; \
    __oi_error.domain = _domain; \
    __oi_error.code = _code; \
    s->on_error(s, __oi_error); \
  } \
} while(0) \

static int 
full_close(oi_socket *socket)
{
  if(-1 == close(socket->fd) && errno == EINTR) {
    /* TODO fd still open. next loop call close again? */
    assert(0 && "implement me");  
    return ERROR;
  }

  socket->read_action = NULL;
  socket->write_action = NULL;

  if(socket->loop) {
    ev_feed_event(socket->loop, &socket->read_watcher, EV_READ);
  }
  return OKAY;
}

static int 
half_close(oi_socket *socket)
{
  int r = shutdown(socket->fd, SHUT_WR);

  if(r == -1) {
    RAISE_ERROR(socket, OI_ERROR_SHUTDOWN, errno);
    return ERROR;
  }

  socket->write_action = NULL;

  /* TODO set timer to zero  so we get a callback soon */
  return OKAY;
}

static void
update_write_buffer_after_send(oi_socket *socket, ssize_t sent)
{
  oi_queue *q = oi_queue_last(&socket->out_stream);
  oi_buf *to_write = oi_queue_data(q, oi_buf, queue);
  to_write->written += sent;
  socket->written += sent;

  if(to_write->written == to_write->len) {

    oi_queue_remove(q);

    if(to_write->release) {
      to_write->release(to_write);
    }  

    if(oi_queue_empty(&socket->out_stream)) {
      ev_io_stop(socket->loop, &socket->write_watcher);
      if(socket->on_drain)
        socket->on_drain(socket);
    }
  }
}


#if HAVE_GNUTLS
static int secure_socket_send(oi_socket *socket);
static int secure_socket_recv(oi_socket *socket);

/* TODO can this be done without ignoring SIGPIPE?  */
static ssize_t 
nosigpipe_push(gnutls_transport_ptr_t data, const void *buf, size_t len)
{
  oi_socket *socket = (oi_socket*)data;
  assert(socket->secure);
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
  flags |= MSG_DONTWAIT;
#endif
  int r = send(socket->fd, buf, len, flags);

  if(r == -1) {
    gnutls_transport_set_errno(socket->session, errno); /* necessary ? */
  }

  return r;
}

static int
secure_handshake(oi_socket *socket)
{
  assert(socket->secure);

  int r = gnutls_handshake(socket->session);

  if(gnutls_error_is_fatal(r)) {
    RAISE_ERROR(socket, OI_ERROR_GNUTLS, r);
    return ERROR;
  }

  if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN)
    return AGAIN;

  oi_socket_reset_timeout(socket);

  if(!socket->connected) {
    socket->connected = TRUE;
    if(socket->on_connect)
      socket->on_connect(socket);
  }

  if(socket->read_action)
    socket->read_action = secure_socket_recv;
 
  if(socket->write_action)
    socket->write_action = secure_socket_send;

  return OKAY;
}

static int
secure_socket_send(oi_socket *socket)
{
  ssize_t sent;

  if(oi_queue_empty(&socket->out_stream)) {
    ev_io_stop(socket->loop, &socket->write_watcher);
    return AGAIN;
  }

  oi_queue *q = oi_queue_last(&socket->out_stream);
  oi_buf *to_write = oi_queue_data(q, oi_buf, queue);

  assert(socket->secure);

  sent = gnutls_record_send( socket->session
                           , to_write->base + to_write->written
                           , to_write->len - to_write->written
                           ); 

  if(gnutls_error_is_fatal(sent)) {
    RAISE_ERROR(socket, OI_ERROR_GNUTLS, sent);
    return ERROR;
  }

  if(sent == 0)
    return AGAIN;

  oi_socket_reset_timeout(socket);

  if(sent == GNUTLS_E_INTERRUPTED || sent == GNUTLS_E_AGAIN) {
    if(GNUTLS_NEED_READ) {
      if(socket->read_action) {
        socket->read_action = secure_socket_send;
      } else {
        /* TODO GnuTLS needs read but already got EOF */
        assert(0 && "needs read but already got EOF");
        return ERROR;
      }
    }
    return AGAIN;
  }

  if(sent > 0) {
    /* make sure the callbacks are correct */
    if(socket->read_action)
      socket->read_action = secure_socket_recv;
    update_write_buffer_after_send(socket, sent);
    return OKAY;
  }

  assert(0 && "Unhandled return code from gnutls_record_send()!");
  return ERROR;
}

static int
secure_socket_recv(oi_socket *socket)
{
  char recv_buffer[TCP_MAXWIN];
  size_t recv_buffer_size = MIN(TCP_MAXWIN, socket->chunksize);
  ssize_t recved;

  assert(socket->secure);

  recved = gnutls_record_recv(socket->session, recv_buffer, recv_buffer_size);

  //printf("secure socket recv %d %p\n", recved, socket->on_connect);

  if(gnutls_error_is_fatal(recved)) {
    RAISE_ERROR(socket, OI_ERROR_GNUTLS, recved);
    return ERROR;
  }

  if(recved == GNUTLS_E_INTERRUPTED || recved == GNUTLS_E_AGAIN)  {
    if(GNUTLS_NEED_WRITE) {
      if(socket->write_action) {
        printf("need write\n");
        socket->write_action = secure_socket_recv;
      } else {
        /* TODO GnuTLS needs send but already closed write end */
        assert(0 && "needs read but cannot");
        return ERROR;
      }
    }
    return AGAIN;
  }

  oi_socket_reset_timeout(socket);

  /* A server may also receive GNUTLS_E_REHANDSHAKE when a client has
   * initiated a handshake. In that case the server can only initiate a
   * handshake or terminate the connection. */
  if(recved == GNUTLS_E_REHANDSHAKE) {
    if(socket->write_action) {
      socket->read_action = secure_handshake;
      socket->write_action = secure_handshake;
      return OKAY;
    } else {
      /* TODO */
      assert(0 && "needs read but cannot");
      return ERROR;
    }
  }

  if(recved >= 0) {
    /* Got EOF */
    if(recved == 0)
      socket->read_action = NULL;

    if(socket->write_action) 
      socket->write_action = secure_socket_send;

    if(socket->on_read) { socket->on_read(socket, recv_buffer, recved); }

    return OKAY;
  }

  assert(0 && "Unhandled return code from gnutls_record_send()!");
  return ERROR;
}

static int
secure_goodbye(oi_socket *socket, gnutls_close_request_t how)
{
  assert(socket->secure);

  int r = gnutls_bye(socket->session, how);

  if(gnutls_error_is_fatal(r))  {
    RAISE_ERROR(socket, OI_ERROR_GNUTLS, r);
    return ERROR;
  }

  if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN)
    return AGAIN;

  return OKAY;
}

static int
secure_full_goodbye(oi_socket *socket)
{
  int r = secure_goodbye(socket, GNUTLS_SHUT_RDWR);
  if(OKAY == r) {
    return full_close(socket);
  }
  return r;
}

static int
secure_half_goodbye(oi_socket *socket)
{
  int r = secure_goodbye(socket, GNUTLS_SHUT_WR);
  if(OKAY == r) {
    return half_close(socket);
  }
  return r;
}

/* Tells the socket to use transport layer security (SSL). liboi does not
 * want to make any decisions about security requirements, so the
 * majoirty of GnuTLS configuration is left to the user. Only the transport
 * layer of GnuTLS is controlled by liboi.
 *
 * That is, do not use gnutls_transport_* functions. 
 * Do use the rest of GnuTLS's API.
 */
void
oi_socket_set_secure_session (oi_socket *socket, gnutls_session_t session)
{
  socket->session = session;
  socket->secure = TRUE;
}
#endif /* HAVE GNUTLS */

static int
socket_send(oi_socket *socket)
{
  ssize_t sent;

  assert(socket->secure == FALSE);

  if(oi_queue_empty(&socket->out_stream)) {
    ev_io_stop(socket->loop, &socket->write_watcher);
    return AGAIN;
  }

  oi_queue *q = oi_queue_last(&socket->out_stream);
  oi_buf *to_write = oi_queue_data(q, oi_buf, queue);
  
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
  flags |= MSG_DONTWAIT;
#endif

  /* TODO use writev() here */

  sent = send( socket->fd
             , to_write->base + to_write->written
             , to_write->len - to_write->written
             , flags
             );

  if(sent < 0) {
    switch(errno) {
      case EAGAIN:
        return AGAIN;

      case ECONNREFUSED:
      case ECONNRESET:
        socket->write_action = NULL;
        /* TODO maybe just clear write buffer instead of error? 
         * They should be able to read still from the socket. 
         */
        RAISE_ERROR(socket, OI_ERROR_SEND, errno);
        return ERROR;

      default:
        perror("send()");
        assert(0 && "oi shouldn't let this happen.");
        return ERROR;
    }
  }

  oi_socket_reset_timeout(socket);

  if(!socket->connected) {
    socket->connected = TRUE;
    if(socket->on_connect) { socket->on_connect(socket); }
  }

  update_write_buffer_after_send(socket, sent);

  return OKAY;
}

static int
socket_recv(oi_socket *socket)
{
  char buf[TCP_MAXWIN];
  size_t buf_size = TCP_MAXWIN;
  ssize_t recved;

  assert(socket->secure == FALSE);

  if(!socket->connected) {
    socket->connected = TRUE;
    if(socket->on_connect) { socket->on_connect(socket); }
    return OKAY;
  }

  recved = recv(socket->fd, buf, buf_size, 0);

  if(recved < 0) {
    switch(errno) {
      case EAGAIN: 
      case EINTR:  
        return AGAIN;

      /* A remote host refused to allow the network connection (typically
       * because it is not running the requested service). */
      case ECONNREFUSED:
        RAISE_ERROR(socket, OI_ERROR_RECV, errno);
        return ERROR; 

      case ECONNRESET:
        RAISE_ERROR(socket, OI_ERROR_RECV, errno);
        return ERROR; 

      default:
        perror("recv()");
        printf("unmatched errno %d\n", errno);
        assert(0 && "recv returned error that oi should have caught before.");
        return ERROR;
    }
  }

  oi_socket_reset_timeout(socket);

  if(recved == 0) {
    oi_socket_read_stop(socket);
    socket->read_action = NULL;
  }

  /* NOTE: EOF is signaled with recved == 0 on callback */
  if(socket->on_read) { socket->on_read(socket, buf, recved); }

  return OKAY;
}

static void
assign_file_descriptor(oi_socket *socket, int fd)
{
  socket->fd = fd;

  ev_io_set (&socket->read_watcher, fd, EV_READ);
  ev_io_set (&socket->write_watcher, fd, EV_WRITE);

  socket->read_action = socket_recv;
  socket->write_action = socket_send;

#if HAVE_GNUTLS
  if(socket->secure) {
    gnutls_transport_set_lowat(socket->session, 0); 
    gnutls_transport_set_push_function(socket->session, nosigpipe_push);
    gnutls_transport_set_ptr2 ( socket->session
                 /* recv */   , (gnutls_transport_ptr_t)fd 
                 /* send */   , socket 
                              );
    socket->read_action = secure_handshake;
    socket->write_action = secure_handshake;
  }
#endif 
}


/* Internal callback 
 * Called by server->connection_watcher.
 */
static void 
on_connection(struct ev_loop *loop, ev_io *watcher, int revents)
{
  oi_server *server = watcher->data;

 // printf("on connection!\n");

  assert(server->listening);
  assert(server->loop == loop);
  assert(&server->connection_watcher == watcher);
  
  if(EV_ERROR & revents) {
    oi_server_close(server);
    return;
  }
  
  struct sockaddr address; /* connector's address information */
  socklen_t addr_len = sizeof(address);
  
  /* TODO accept all possible connections? currently: just one */
  int fd = accept(server->fd, (struct sockaddr*)&address, &addr_len);
  if(fd < 0) {
    perror("accept()");
    return;
  }

  oi_socket *socket = NULL;
  if(server->on_connection)
    socket = server->on_connection(server, (struct sockaddr*)&address, addr_len);

  if(socket == NULL) {
    close(fd);
    return;
  } 
  
  int flags = fcntl(fd, F_GETFL, 0);
  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if(r < 0) {
    /* TODO error report */
  }
  
#ifdef SO_NOSIGPIPE
  flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &flags, sizeof(flags));
#endif

  socket->server = server;
  assign_file_descriptor(socket, fd);
  oi_socket_attach(socket, loop);
}

int
oi_server_listen(oi_server *server, struct addrinfo *addrinfo)
{
  int fd = -1;
  struct linger ling = {0, 0};
  assert(server->listening == FALSE);

  fd = socket( addrinfo->ai_family
             , addrinfo->ai_socktype
             , addrinfo->ai_protocol
             );
  if(fd < 0) {
    perror("socket()");
    return -1;
  }

  int flags = fcntl(fd, F_GETFL, 0);
  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if(r < 0) {
    perror("fcntl()");
    return -1;
  }

  flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));

  /* XXX: Sending single byte chunks in a response body? Perhaps there is a
   * need to enable the Nagel algorithm dynamically. For now disabling.
   */
  //setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));

  if (bind(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) < 0) {
    perror("bind()");
    close(fd);
    return -1;
  }
  
  if (listen(fd, server->backlog) < 0) {
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
oi_server_close(oi_server *server)
{
  if(server->listening) {
    oi_server_detach(server);
    close(server->fd);
    /* TODO do this on the loop? check return value? */
    server->listening = FALSE;
  }
}

void
oi_server_attach (oi_server *server, struct ev_loop *loop)
{
  ev_io_start (loop, &server->connection_watcher);
  server->loop = loop;
}

void
oi_server_detach (oi_server *server)
{
  ev_io_stop (server->loop, &server->connection_watcher);
  server->loop = NULL;
}

void 
oi_server_init(oi_server *server, int backlog)
{
  server->backlog = backlog;
  server->listening = FALSE;
  server->fd = -1;
  server->connection_watcher.data = server;
  ev_init (&server->connection_watcher, on_connection);

  server->on_connection = NULL;
  server->on_error = NULL;
  server->data = NULL;
}

/* Internal callback. called by socket->timeout_watcher */
static void 
on_timeout(struct ev_loop *loop, ev_timer *watcher, int revents)
{
  oi_socket *socket = watcher->data;

  assert(watcher == &socket->timeout_watcher);

 // printf("on_timeout\n");

  if(socket->on_timeout) { socket->on_timeout(socket); }


  /* TODD set timer to zero */
  full_close(socket);
}

static void
release_write_buffer(oi_socket *socket)
{
  while(!oi_queue_empty(&socket->out_stream)) {
    oi_queue *q = oi_queue_last(&socket->out_stream);
    oi_buf *buf = oi_queue_data(q, oi_buf, queue);
    oi_queue_remove(q);
    if(buf->release) { buf->release(buf); }
  }
}

/* Internal callback. called by socket->read_watcher */
static void 
on_io_event(struct ev_loop *loop, ev_io *watcher, int revents)
{
  oi_socket *socket = watcher->data;

  if(revents & EV_ERROR) {
    RAISE_ERROR(socket, OI_ERROR_EV, 0);
    goto close;
  }

  int r;
  int have_read_event = TRUE;
  int have_write_event = TRUE;

  while(have_read_event || have_write_event) {

    if(socket->read_action) {
      r = socket->read_action(socket);
      if(r == ERROR) goto close;
      if(r == AGAIN) have_read_event = FALSE;
    } else {
      have_read_event = FALSE;
    }

    if(socket->write_action) {
      r = socket->write_action(socket);
      if(r == ERROR) goto close;
      if(r == AGAIN) have_write_event = FALSE;
    } else {
      have_write_event = FALSE;
    }


    if(socket->read_watcher.active == FALSE)
      have_read_event = FALSE;
    if(socket->write_watcher.active == FALSE)
      have_write_event = FALSE;
  }

  if(socket->write_action == NULL && socket->read_action == NULL)
    goto close;

  return;

close:
  release_write_buffer(socket);

  ev_clear_pending (socket->loop, &socket->write_watcher);
  ev_clear_pending (socket->loop, &socket->read_watcher);
  ev_clear_pending (socket->loop, &socket->timeout_watcher);

  oi_socket_detach(socket);

  if(socket->on_close) { socket->on_close(socket); }
  /* WARNING: user can free socket in on_close so no more 
   * access beyond this point. */
}

/**
 * If using SSL do consider setting
 *   gnutls_db_set_retrieve_function (socket->session, _);
 *   gnutls_db_set_remove_function (socket->session, _);
 *   gnutls_db_set_store_function (socket->session, _);
 *   gnutls_db_set_ptr (socket->session, _);
 */
void 
oi_socket_init(oi_socket *socket, float timeout)
{
  socket->fd = -1;
  socket->server = NULL;
  socket->loop = NULL;
  socket->connected = FALSE;

  oi_queue_init(&socket->out_stream);

  ev_init (&socket->write_watcher, on_io_event);
  socket->write_watcher.data = socket;

  ev_init(&socket->read_watcher, on_io_event);
  socket->read_watcher.data = socket;

  socket->secure = FALSE;
  socket->wait_for_secure_hangup = FALSE;
#if HAVE_GNUTLS
  socket->session = NULL;
#endif 

  /* TODO higher resolution timer */
  ev_timer_init(&socket->timeout_watcher, on_timeout, 0., timeout);
  socket->timeout_watcher.data = socket;  

  socket->read_action = NULL;
  socket->write_action = NULL;

  socket->chunksize = TCP_MAXWIN; 
  socket->on_connect = NULL;
  socket->on_read = NULL;
  socket->on_drain = NULL;
  socket->on_error = NULL;
  socket->on_timeout = NULL;
}

void 
oi_socket_write_eof (oi_socket *socket)
{
#if HAVE_GNUTLS
  /* try to hang up properly for secure connections */
  if(socket->secure) 
  {
    if( socket->connected /* completed handshake */ 
     && socket->write_action /* write end is open */
      ) 
    {
      socket->write_action = secure_half_goodbye;
      if(socket->loop)
        ev_io_start(socket->loop, &socket->write_watcher);
      return;
    }
    /* secure servers cannot handle half-closed connections? */
    full_close(socket); 
    return;
  }
#endif // HAVE_GNUTLS

  if(socket->write_action)
    half_close(socket);
  else
    full_close(socket);
}

void 
oi_socket_close (oi_socket *socket)
{
#if HAVE_GNUTLS
  /* try to hang up properly for secure connections */
  if( socket->secure 
   && socket->connected /* completed handshake */ 
   && socket->write_action /* write end is open */
    ) 
  {
    if(socket->wait_for_secure_hangup && socket->read_action) {
      socket->write_action = secure_full_goodbye;
      socket->read_action = secure_full_goodbye;
    } else {
      socket->write_action = secure_half_goodbye;
      socket->read_action = NULL;
    }

    if(socket->loop)
      ev_io_start(socket->loop, &socket->write_watcher);

    return;
  }
#endif // HAVE_GNUTLS

  full_close(socket);
}

/* 
 * Resets the timeout to stay alive for another socket->timeout seconds
 */
void 
oi_socket_reset_timeout(oi_socket *socket)
{
  ev_timer_again(socket->loop, &socket->timeout_watcher);
}

/**
 * Writes a string to the socket. This is actually sets a watcher which may
 * take multiple iterations to write the entire string.
 */
void 
oi_socket_write(oi_socket *socket, oi_buf *buf)
{
  if(socket->write_action == NULL)
    return;

  oi_queue_insert_head(&socket->out_stream, &buf->queue);

  buf->written = 0;
  ev_io_start(socket->loop, &socket->write_watcher);
}

static void
free_simple_buf ( oi_buf *buf )
{
  free(buf->base);
  free(buf);
}

/* Writes a string to the socket. 
 * NOTE: Allocates memory. Avoid for performance applications.
 */ 
void
oi_socket_write_simple(oi_socket *socket, const char *str, size_t len)
{
  oi_buf *buf = malloc(sizeof(oi_buf));
  buf->release = free_simple_buf;
  buf->base = strdup(str);
  buf->len = len;

  oi_socket_write(socket, buf);
}

void
oi_socket_attach(oi_socket *socket, struct ev_loop *loop)
{
  socket->loop = loop;

  ev_timer_again(loop, &socket->timeout_watcher);

  if(socket->read_action) 
    ev_io_start(loop, &socket->read_watcher);

  if(socket->write_action) 
    ev_io_start(loop, &socket->write_watcher);

  /* make sure the io_event happens soon  in the case we're being reattached */
  ev_feed_event(loop, &socket->read_watcher, EV_READ);
}

void
oi_socket_detach(oi_socket *socket)
{
  if(socket->loop) {
    ev_io_stop(socket->loop, &socket->write_watcher);
    ev_io_stop(socket->loop, &socket->read_watcher);
    ev_timer_stop(socket->loop, &socket->timeout_watcher);
    socket->loop = NULL;
  }
}

void
oi_socket_read_stop (oi_socket *socket)
{
  ev_io_stop(socket->loop, &socket->read_watcher);
  ev_clear_pending (socket->loop, &socket->read_watcher);
}

void
oi_socket_read_start (oi_socket *socket)
{
  if(socket->read_action) {
    ev_io_start(socket->loop, &socket->read_watcher);
    /* XXX feed event? */
  }
}

int
oi_socket_connect(oi_socket *s, struct addrinfo *addrinfo)
{
  int fd = socket( addrinfo->ai_family
                 , addrinfo->ai_socktype
                 , addrinfo->ai_protocol
                 );
  if(fd < 0) {
    perror("socket()");
    return -1;
  }

  int flags = fcntl(fd, F_GETFL, 0);
  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if(r < 0) {
    perror("fcntl()");
    return -1;
  }
      
#ifdef SO_NOSIGPIPE
  flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &flags, sizeof(flags));
#endif

  r = connect( fd
             , addrinfo->ai_addr
             , addrinfo->ai_addrlen
             );

  if(r < 0 && errno != EINPROGRESS) {
    perror("connect");
    close(fd);
    return -1;
  }

  assign_file_descriptor(s, fd);

  return 0;
}

