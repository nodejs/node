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
#ifndef EBB_H
#define EBB_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <ev.h>
#ifdef HAVE_GNUTLS
# include <gnutls/gnutls.h>
# include "rbtree.h" /* for ebb_server.session_cache */
#endif 
#include "ebb_request_parser.h"

#define EBB_MAX_CONNECTIONS 1024
#define EBB_DEFAULT_TIMEOUT 30.0

#define EBB_AGAIN 0
#define EBB_STOP 1

typedef struct ebb_server     ebb_server;
typedef struct ebb_connection ebb_connection;
typedef void (*ebb_after_write_cb) (ebb_connection *connection); 
typedef void (*ebb_connection_cb)(ebb_connection *connection, void *data);

struct ebb_server {
  int fd;                                       /* ro */
  struct sockaddr_in sockaddr;                  /* ro */
  socklen_t socklen;                            /* ro */
  char port[6];                                 /* ro */
  struct ev_loop *loop;                         /* ro */
  unsigned listening:1;                         /* ro */
  unsigned secure:1;                            /* ro */
#ifdef HAVE_GNUTLS
  gnutls_certificate_credentials_t credentials; /* private */
  struct rbtree_t session_cache;                /* private */
#endif
  ev_io connection_watcher;                     /* private */

  /* Public */

  /* Allocates and initializes an ebb_connection.  NULL by default. */
  ebb_connection* (*new_connection) (ebb_server*, struct sockaddr_in*);

  void *data;
};

struct ebb_connection {
  int fd;                      /* ro */
  struct sockaddr_in sockaddr; /* ro */
  socklen_t socklen;           /* ro */ 
  ebb_server *server;          /* ro */
  char *ip;                    /* ro */
  unsigned open:1;             /* ro */

  const char *to_write;              /* ro */
  size_t to_write_len;               /* ro */
  size_t written;                    /* ro */ 
  ebb_after_write_cb after_write_cb; /* ro */

  ebb_request_parser parser;   /* private */
  ev_io write_watcher;         /* private */
  ev_io read_watcher;          /* private */
  ev_timer timeout_watcher;    /* private */
  ev_timer goodbye_watcher;    /* private */
#ifdef HAVE_GNUTLS
  ev_io handshake_watcher;     /* private */
  gnutls_session_t session;    /* private */
  ev_io goodbye_tls_watcher;       /* private */
#endif

  /* Public */

  ebb_request* (*new_request) (ebb_connection*); 

  /* Returns EBB_STOP or EBB_AGAIN. NULL by default.  */
  int (*on_timeout) (ebb_connection*); 

  void (*on_close) (ebb_connection*); 

  void *data;
};

void ebb_server_init (ebb_server *server, struct ev_loop *loop);
#ifdef HAVE_GNUTLS
int ebb_server_set_secure (ebb_server *server, const char *cert_file, 
                           const char *key_file);
#endif
int ebb_server_listen_on_port (ebb_server *server, const int port);
int ebb_server_listen_on_fd (ebb_server *server, const int sfd);
void ebb_server_unlisten (ebb_server *server);

void ebb_connection_init (ebb_connection *);
void ebb_connection_schedule_close (ebb_connection *);
void ebb_connection_reset_timeout (ebb_connection *);
int ebb_connection_write (ebb_connection *, const char *buf, size_t len, ebb_after_write_cb);

#endif
