/* This file is part of libebb.
 *
 * Copyright (c) 2008 Ryan Dahl (ry@ndahl.us)
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 */
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h> /* TCP_NODELAY */
#include <netinet/in.h>  /* inet_ntoa */
#include <arpa/inet.h>   /* inet_ntoa */
#include <unistd.h>
#include <stdio.h>      /* perror */
#include <errno.h>      /* perror */
#include <stdlib.h> /* for the default methods */
#include <ev.h>

#include "ebb.h"
#include "ebb_request_parser.h"
#ifdef HAVE_GNUTLS
# include <gnutls/gnutls.h>
# include "rbtree.h" /* for session_cache */
#endif

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif 
#ifndef MIN
# define MIN(a,b) (a < b ? a : b)
#endif

#define error(FORMAT, ...) fprintf(stderr, "error: " FORMAT "\n", ##__VA_ARGS__)

#define CONNECTION_HAS_SOMETHING_TO_WRITE (connection->to_write != NULL)

static void 
set_nonblock (int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  assert(0 <= r && "Setting socket non-block failed!");
}

static ssize_t 
nosigpipe_push(void *data, const void *buf, size_t len)
{
  int fd = (int)data;
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags = MSG_NOSIGNAL;
#endif
  return send(fd, buf, len, flags);
}

static void 
close_connection(ebb_connection *connection)
{
#ifdef HAVE_GNUTLS
  if(connection->server->secure)
    ev_io_stop(connection->server->loop, &connection->handshake_watcher);
#endif
  ev_io_stop(connection->server->loop, &connection->read_watcher);
  ev_io_stop(connection->server->loop, &connection->write_watcher);
  ev_timer_stop(connection->server->loop, &connection->timeout_watcher);

  if(0 > close(connection->fd))
    error("problem closing connection fd");

  connection->open = FALSE;

  if(connection->on_close)
    connection->on_close(connection);
  /* No access to the connection past this point! 
   * The user is allowed to free in the callback
   */
}

#ifdef HAVE_GNUTLS
#define GNUTLS_NEED_WRITE (gnutls_record_get_direction(connection->session) == 1)
#define GNUTLS_NEED_READ (gnutls_record_get_direction(connection->session) == 0)

#define EBB_MAX_SESSION_KEY 32
#define EBB_MAX_SESSION_VALUE 512

struct session_cache {
  struct rbtree_node_t node;

  gnutls_datum_t key;
  gnutls_datum_t value;

  char key_storage[EBB_MAX_SESSION_KEY];
  char value_storage[EBB_MAX_SESSION_VALUE];
};

static int 
session_cache_compare (void *left, void *right) 
{
  gnutls_datum_t *left_key = left;
  gnutls_datum_t *right_key = right;
  if(left_key->size < right_key->size)
    return -1;
  else if(left_key->size > right_key->size)
    return 1;
  else
    return memcmp( left_key->data
                 , right_key->data
                 , MIN(left_key->size, right_key->size)
                 );
}

static int
session_cache_store(void *data, gnutls_datum_t key, gnutls_datum_t value)
{
  rbtree tree = data;

  if( tree == NULL
   || key.size > EBB_MAX_SESSION_KEY
   || value.size > EBB_MAX_SESSION_VALUE
    ) return -1;

  struct session_cache *cache = gnutls_malloc(sizeof(struct session_cache));

  memcpy (cache->key_storage, key.data, key.size);
  cache->key.size = key.size;
  cache->key.data = (void*)cache->key_storage;

  memcpy (cache->value_storage, value.data, value.size);
  cache->value.size = value.size;
  cache->value.data = (void*)cache->value_storage;

  cache->node.key = &cache->key;
  cache->node.value = &cache;

  rbtree_insert(tree, (rbtree_node)cache);

  //printf("session_cache_store\n");

  return 0;
}

static gnutls_datum_t
session_cache_retrieve (void *data, gnutls_datum_t key)
{
  rbtree tree = data;
  gnutls_datum_t res = { NULL, 0 };
  struct session_cache *cache = rbtree_lookup(tree, &key);

  if(cache == NULL)
    return res;

  res.size = cache->value.size;
  res.data = gnutls_malloc (res.size);
  if(res.data == NULL)
    return res;

  memcpy(res.data, cache->value.data, res.size);

  //printf("session_cache_retrieve\n");

  return res;
}

static int
session_cache_remove (void *data, gnutls_datum_t key)
{
  rbtree tree = data;

  if(tree == NULL)
    return -1;

  struct session_cache *cache = (struct session_cache *)rbtree_delete(tree, &key);
  if(cache == NULL)
    return -1;

  gnutls_free(cache);

  //printf("session_cache_remove\n");

  return 0;
}

static void 
on_handshake(struct ev_loop *loop ,ev_io *watcher, int revents)
{
  ebb_connection *connection = watcher->data;

  //printf("on_handshake\n");

  assert(ev_is_active(&connection->timeout_watcher));
  assert(!ev_is_active(&connection->read_watcher));
  assert(!ev_is_active(&connection->write_watcher));

  if(EV_ERROR & revents) {
    error("on_handshake() got error event, closing connection.n");
    goto error;
  }

  int r = gnutls_handshake(connection->session);
  if(r < 0) {
    if(gnutls_error_is_fatal(r)) goto error;
    if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN)
      ev_io_set( watcher
               , connection->fd
               , (GNUTLS_NEED_WRITE ? EV_WRITE : EV_READ)
               );
    return;
  }

  ebb_connection_reset_timeout(connection);
  ev_io_stop(loop, watcher);

  ev_io_start(loop, &connection->read_watcher);
  if(CONNECTION_HAS_SOMETHING_TO_WRITE)
    ev_io_start(loop, &connection->write_watcher);

  return;
error:
  close_connection(connection);
}

#endif /* HAVE_GNUTLS */


/* Internal callback 
 * called by connection->timeout_watcher
 */
static void 
on_timeout(struct ev_loop *loop, ev_timer *watcher, int revents)
{
  ebb_connection *connection = watcher->data;

  assert(watcher == &connection->timeout_watcher);

  //printf("on_timeout\n");

  /* if on_timeout returns true, we don't time out */
  if(connection->on_timeout) {
    int r = connection->on_timeout(connection);

    if(r == EBB_AGAIN) {
      ebb_connection_reset_timeout(connection);
      return;
    }
  }

  ebb_connection_schedule_close(connection);
}

/* Internal callback 
 * called by connection->read_watcher
 */
static void 
on_readable(struct ev_loop *loop, ev_io *watcher, int revents)
{
  ebb_connection *connection = watcher->data;
  char recv_buffer[TCP_MAXWIN];
  ssize_t recved;

  //printf("on_readable\n");
  // TODO -- why is this broken?
  //assert(ev_is_active(&connection->timeout_watcher));
  assert(watcher == &connection->read_watcher);

  if(EV_ERROR & revents) {
    error("on_readable() got error event, closing connection.");
    goto error;
  }

#ifdef HAVE_GNUTLS
  assert(!ev_is_active(&connection->handshake_watcher));

  if(connection->server->secure) {
    recved = gnutls_record_recv( connection->session
                               , recv_buffer
                               , TCP_MAXWIN
                               );
    if(recved <= 0) {
      if(gnutls_error_is_fatal(recved)) goto error;
      if( (recved == GNUTLS_E_INTERRUPTED || recved == GNUTLS_E_AGAIN)
       && GNUTLS_NEED_WRITE
        ) ev_io_start(loop, &connection->write_watcher);
      return; 
    } 
  } else {
#endif /* HAVE_GNUTLS */

    recved = recv(connection->fd, recv_buffer, TCP_MAXWIN, 0);
    if(recved <= 0) goto error;

#ifdef HAVE_GNUTLS
  }
#endif /* HAVE_GNUTLS */

  ebb_connection_reset_timeout(connection);

  ebb_request_parser_execute(&connection->parser, recv_buffer, recved);

  /* parse error? just drop the client. screw the 400 response */
  if(ebb_request_parser_has_error(&connection->parser)) goto error;
  return;
error:
  ebb_connection_schedule_close(connection);
}

/* Internal callback 
 * called by connection->write_watcher
 */
static void 
on_writable(struct ev_loop *loop, ev_io *watcher, int revents)
{
  ebb_connection *connection = watcher->data;
  ssize_t sent;
  
  //printf("on_writable\n");

  assert(CONNECTION_HAS_SOMETHING_TO_WRITE);
  assert(connection->written <= connection->to_write_len);
  // TODO -- why is this broken?
  //assert(ev_is_active(&connection->timeout_watcher));
  assert(watcher == &connection->write_watcher);

  if(connection->to_write == 0)
    goto stop_writing;

#ifdef HAVE_GNUTLS
  assert(!ev_is_active(&connection->handshake_watcher));

  if(connection->server->secure) {
    sent = gnutls_record_send( connection->session
                             , connection->to_write + connection->written
                             , connection->to_write_len - connection->written
                             ); 
    if(sent < 0) {
      if(gnutls_error_is_fatal(sent)) goto error;
      if( (sent == GNUTLS_E_INTERRUPTED || sent == GNUTLS_E_AGAIN)
       && GNUTLS_NEED_READ
        ) ev_io_stop(loop, watcher);
      return; 
    }
  } else {
#endif /* HAVE_GNUTLS */

    sent = nosigpipe_push( (void*)connection->fd
                         , connection->to_write + connection->written
                         , connection->to_write_len - connection->written
                         );
    if(sent < 0) goto error;
    if(sent == 0) return;

#ifdef HAVE_GNUTLS
  }
#endif /* HAVE_GNUTLS */

  ebb_connection_reset_timeout(connection);

  connection->written += sent;

  if(connection->written == connection->to_write_len) {
    goto stop_writing;
  }
  return;
stop_writing:
  ev_io_stop(loop, watcher);
  connection->to_write = NULL;

  if(connection->after_write_cb)
    connection->after_write_cb(connection);
  return;
error:
  error("close connection on write.");
  ebb_connection_schedule_close(connection);
}

#ifdef HAVE_GNUTLS

static void 
on_goodbye_tls(struct ev_loop *loop, ev_io *watcher, int revents)
{
  ebb_connection *connection = watcher->data;
  assert(watcher == &connection->goodbye_tls_watcher);

  if(EV_ERROR & revents) {
    error("on_goodbye() got error event, closing connection.");
    goto die;
  }

  int r = gnutls_bye(connection->session, GNUTLS_SHUT_RDWR);
  if(r < 0) {
    if(gnutls_error_is_fatal(r)) goto die;
    if(r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN)
      ev_io_set( watcher
               , connection->fd
               , (GNUTLS_NEED_WRITE ? EV_WRITE : EV_READ)
               );
    return;
  }

die:
  ev_io_stop(loop, watcher);
  if(connection->session) 
    gnutls_deinit(connection->session);
  close_connection(connection);
}
#endif /* HAVE_GNUTLS*/

static void 
on_goodbye(struct ev_loop *loop, ev_timer *watcher, int revents)
{
  ebb_connection *connection = watcher->data;
  assert(watcher == &connection->goodbye_watcher);

  close_connection(connection);
}


static ebb_request* 
new_request_wrapper(void *data)
{
  ebb_connection *connection = data;
  if(connection->new_request)
    return connection->new_request(connection);
  return NULL;
}

/* Internal callback 
 * Called by server->connection_watcher.
 */
static void 
on_connection(struct ev_loop *loop, ev_io *watcher, int revents)
{
  ebb_server *server = watcher->data;

  //printf("on connection!\n");

  assert(server->listening);
  assert(server->loop == loop);
  assert(&server->connection_watcher == watcher);
  
  if(EV_ERROR & revents) {
    error("on_connection() got error event, closing server.");
    ebb_server_unlisten(server);
    return;
  }
  
  struct sockaddr_in addr; // connector's address information
  socklen_t addr_len = sizeof(addr); 
  int fd = accept( server->fd
                 , (struct sockaddr*) & addr
                 , & addr_len
                 );
  if(fd < 0) {
    perror("accept()");
    return;
  }

  ebb_connection *connection = NULL;
  if(server->new_connection)
    connection = server->new_connection(server, &addr);
  if(connection == NULL) {
    close(fd);
    return;
  } 
  
  set_nonblock(fd);
  connection->fd = fd;
  connection->open = TRUE;
  connection->server = server;
  memcpy(&connection->sockaddr, &addr, addr_len);
  if(server->port[0] != '\0')
    connection->ip = inet_ntoa(connection->sockaddr.sin_addr);  

#ifdef SO_NOSIGPIPE
  int arg = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &arg, sizeof(int));
#endif

#ifdef HAVE_GNUTLS
  if(server->secure) {
    gnutls_init(&connection->session, GNUTLS_SERVER);
    gnutls_transport_set_lowat(connection->session, 0); 
    gnutls_set_default_priority(connection->session);
    gnutls_credentials_set(connection->session, GNUTLS_CRD_CERTIFICATE, connection->server->credentials);

    gnutls_transport_set_ptr(connection->session, (gnutls_transport_ptr) fd); 
    gnutls_transport_set_push_function(connection->session, nosigpipe_push);

    gnutls_db_set_ptr (connection->session, &server->session_cache);
    gnutls_db_set_store_function (connection->session, session_cache_store);
    gnutls_db_set_retrieve_function (connection->session, session_cache_retrieve);
    gnutls_db_set_remove_function (connection->session, session_cache_remove);
  }

  ev_io_set(&connection->handshake_watcher, connection->fd, EV_READ | EV_WRITE);
#endif /* HAVE_GNUTLS */

  /* Note: not starting the write watcher until there is data to be written */
  ev_io_set(&connection->write_watcher, connection->fd, EV_WRITE);
  ev_io_set(&connection->read_watcher, connection->fd, EV_READ);
  /* XXX: seperate error watcher? */

  ev_timer_again(loop, &connection->timeout_watcher);

#ifdef HAVE_GNUTLS
  if(server->secure) {
    ev_io_start(loop, &connection->handshake_watcher);
    return;
  }
#endif

  ev_io_start(loop, &connection->read_watcher);
}

/**
 * Begin the server listening on a file descriptor.  This DOES NOT start the
 * event loop.  Start the event loop after making this call.
 */
int 
ebb_server_listen_on_fd(ebb_server *server, const int fd)
{
  assert(server->listening == FALSE);

  if (listen(fd, EBB_MAX_CONNECTIONS) < 0) {
    perror("listen()");
    return -1;
  }
  
  set_nonblock(fd); /* XXX superfluous? */
  
  server->fd = fd;
  server->listening = TRUE;
  
  ev_io_set (&server->connection_watcher, server->fd, EV_READ);
  ev_io_start (server->loop, &server->connection_watcher);
  
  return server->fd;
}


/**
 * Begin the server listening on a file descriptor This DOES NOT start the
 * event loop. Start the event loop after making this call.
 */
int 
ebb_server_listen_on_port(ebb_server *server, const int port)
{
  int fd = -1;
  struct linger ling = {0, 0};
  struct sockaddr_in addr;
  int flags = 1;
  
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket()");
    goto error;
  }
  
  flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
  setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));

  /* XXX: Sending single byte chunks in a response body? Perhaps there is a
   * need to enable the Nagel algorithm dynamically. For now disabling.
   */
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));
  
  /* the memset call clears nonstandard fields in some impementations that
   * otherwise mess things up.
   */
  memset(&addr, 0, sizeof(addr));
  
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind()");
    goto error;
  }
  
  int ret = ebb_server_listen_on_fd(server, fd);
  if (ret >= 0) {
    sprintf(server->port, "%d", port);
  }
  return ret;
error:
  if(fd > 0) close(fd);
  return -1;
}

/**
 * Stops the server. Will not accept new connections.  Does not drop
 * existing connections.
 */
void 
ebb_server_unlisten(ebb_server *server)
{
  if(server->listening) {
    ev_io_stop(server->loop, &server->connection_watcher);
    close(server->fd);
    server->port[0] = '\0';
    server->listening = FALSE;
  }
}

/**
 * Initialize an ebb_server structure.  After calling ebb_server_init set
 * the callback server->new_connection and, optionally, callback data
 * server->data.  The new connection MUST be initialized with
 * ebb_connection_init before returning it to the server.
 *
 * @param server the server to initialize
 * @param loop a libev loop
 */
void 
ebb_server_init(ebb_server *server, struct ev_loop *loop)
{
  server->loop = loop;
  server->listening = FALSE;
  server->port[0] = '\0';
  server->fd = -1;
  server->connection_watcher.data = server;
  ev_init (&server->connection_watcher, on_connection);
  server->secure = FALSE;

#ifdef HAVE_GNUTLS
  rbtree_init(&server->session_cache, session_cache_compare);
  server->credentials = NULL;
#endif

  server->new_connection = NULL;
  server->data = NULL;
}


#ifdef HAVE_GNUTLS
/* similar to server_init. 
 *
 * the user of secure server might want to set additional callbacks from
 * GNUTLS. In particular 
 * gnutls_global_set_mem_functions() 
 * gnutls_global_set_log_function()
 * Also see the note above ebb_connection_init() about setting gnutls cache
 * access functions
 *
 * cert_file: the filename of a PEM certificate file
 *
 * key_file: the filename of a private key. Currently only PKCS-1 encoded
 * RSA and DSA private keys are accepted. 
 */
int 
ebb_server_set_secure (ebb_server *server, const char *cert_file, const char *key_file)
{
  server->secure = TRUE;
  gnutls_global_init();
  gnutls_certificate_allocate_credentials(&server->credentials);
  /* todo gnutls_certificate_free_credentials */
  int r = gnutls_certificate_set_x509_key_file( server->credentials
                                              , cert_file
                                              , key_file
                                              , GNUTLS_X509_FMT_PEM
                                              );
  if(r < 0) {
    error("loading certificates");
    return -1;
  }
  return 1;
}
#endif /* HAVE_GNUTLS */

/**
 * Initialize an ebb_connection structure. After calling this function you
 * must setup callbacks for the different actions the server can take. See
 * server.h for which callbacks are availible. 
 * 
 * This should be called immediately after allocating space for a new
 * ebb_connection structure. Most likely, this will only be called within
 * the ebb_server->new_connection callback which you supply. 
 *
 * If using SSL do consider setting
 *   gnutls_db_set_retrieve_function (connection->session, _);
 *   gnutls_db_set_remove_function (connection->session, _);
 *   gnutls_db_set_store_function (connection->session, _);
 *   gnutls_db_set_ptr (connection->session, _);
 * To provide a better means of storing SSL session caches. libebb provides
 * only a simple default implementation. 
 *
 * @param connection the connection to initialize
 * @param timeout    the timeout in seconds
 */
void 
ebb_connection_init(ebb_connection *connection)
{
  connection->fd = -1;
  connection->server = NULL;
  connection->ip = NULL;
  connection->open = FALSE;

  ebb_request_parser_init( &connection->parser );
  connection->parser.data = connection;
  connection->parser.new_request = new_request_wrapper;
  
  ev_init (&connection->write_watcher, on_writable);
  connection->write_watcher.data = connection;
  connection->to_write = NULL;

  ev_init(&connection->read_watcher, on_readable);
  connection->read_watcher.data = connection;

#ifdef HAVE_GNUTLS
  connection->handshake_watcher.data = connection;
  ev_init(&connection->handshake_watcher, on_handshake);

  ev_init(&connection->goodbye_tls_watcher, on_goodbye_tls);
  connection->goodbye_tls_watcher.data = connection;

  connection->session = NULL;
#endif /* HAVE_GNUTLS */

  ev_timer_init(&connection->goodbye_watcher, on_goodbye, 0., 0.);
  connection->goodbye_watcher.data = connection;  

  ev_timer_init(&connection->timeout_watcher, on_timeout, 0., EBB_DEFAULT_TIMEOUT);
  connection->timeout_watcher.data = connection;  

  connection->new_request = NULL;
  connection->on_timeout = NULL;
  connection->on_close = NULL;
  connection->data = NULL;
}

void 
ebb_connection_schedule_close (ebb_connection *connection)
{
#ifdef HAVE_GNUTLS
  if(connection->server->secure) {
    ev_io_set(&connection->goodbye_tls_watcher, connection->fd, EV_READ | EV_WRITE);
    ev_io_start(connection->server->loop, &connection->goodbye_tls_watcher);
    return;
  }
#endif
  ev_timer_start(connection->server->loop, &connection->goodbye_watcher);
}

/* 
 * Resets the timeout to stay alive for another connection->timeout seconds
 */
void 
ebb_connection_reset_timeout(ebb_connection *connection)
{
  ev_timer_again(connection->server->loop, &connection->timeout_watcher);
}

/**
 * Writes a string to the socket. This is actually sets a watcher
 * which may take multiple iterations to write the entire string.
 *
 * This can only be called once at a time. If you call it again
 * while the connection is writing another buffer the ebb_connection_write
 * will return FALSE and ignore the request.
 */
int 
ebb_connection_write (ebb_connection *connection, const char *buf, size_t len, ebb_after_write_cb cb)
{
  if(ev_is_active(&connection->write_watcher))
    return FALSE;
  assert(!CONNECTION_HAS_SOMETHING_TO_WRITE);
  connection->to_write = buf;
  connection->to_write_len = len;
  connection->written = 0;
  connection->after_write_cb = cb;
  ev_io_start(connection->server->loop, &connection->write_watcher);
  return TRUE;
}

