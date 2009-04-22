#include <netdb.h>
#include <ev.h>
#include <oi_queue.h>
#include <oi_error.h>
#include <oi_buf.h>

#ifndef oi_socket_h
#define oi_socket_h
#ifdef __cplusplus
extern "C" {
#endif 

#ifndef HAVE_GNUTLS
# define HAVE_GNUTLS 0
#endif
#if HAVE_GNUTLS
# include <gnutls/gnutls.h>
#endif

typedef struct oi_server  oi_server;
typedef struct oi_socket  oi_socket;

void oi_server_init          (oi_server *, int backlog);
 int oi_server_listen        (oi_server *, struct addrinfo *addrinfo);
void oi_server_attach        (oi_server *, struct ev_loop *loop);
void oi_server_detach        (oi_server *);
void oi_server_close         (oi_server *); 

void oi_socket_init          (oi_socket *, float timeout);
 int oi_socket_pair          (oi_socket *a, oi_socket *b); /* TODO */
 int oi_socket_connect       (oi_socket *, struct addrinfo *addrinfo);
void oi_socket_attach        (oi_socket *, struct ev_loop *loop);
void oi_socket_detach        (oi_socket *);
void oi_socket_read_start    (oi_socket *);
void oi_socket_read_stop     (oi_socket *);
void oi_socket_reset_timeout (oi_socket *);
void oi_socket_write         (oi_socket *, oi_buf *);
void oi_socket_write_simple  (oi_socket *, const char *str, size_t len);
void oi_socket_write_eof     (oi_socket *);
void oi_socket_close         (oi_socket *);
#if HAVE_GNUTLS
void oi_socket_set_secure_session (oi_socket *, gnutls_session_t);
#endif

struct oi_server {
  /* read only */
  int fd;
  int backlog;
  struct ev_loop *loop;
  unsigned listening:1;

  /* private */
  ev_io connection_watcher;

  /* public */
  oi_socket* (*on_connection) (oi_server *, struct sockaddr *remote_addr, socklen_t remove_addr_len);
  void       (*on_error)      (oi_server *, struct oi_error e);
  void *data;
};

struct oi_socket {
  /* read only */
  int fd;
  struct ev_loop *loop;
  oi_server *server;
  oi_queue out_stream;
  size_t written;
  unsigned connected:1;
  unsigned secure:1;
  unsigned wait_for_secure_hangup:1;

  /* if these are NULL then it means that end of the socket is closed. */
  int (*read_action)  (oi_socket *);
  int (*write_action) (oi_socket *);

  /* private */  
  ev_io write_watcher;
  ev_io read_watcher;
  ev_timer timeout_watcher;
#if HAVE_GNUTLS
  gnutls_session_t session;
#endif
  
  /* public */
  size_t chunksize; /* the maximum chunk that on_read() will return */
  void (*on_connect)   (oi_socket *);
  void (*on_read)      (oi_socket *, const void *buf, size_t count);
  void (*on_drain)     (oi_socket *);
  void (*on_error)     (oi_socket *, struct oi_error e);
  void (*on_close)     (oi_socket *);
  void (*on_timeout)   (oi_socket *);
  void *data;
};

#ifdef __cplusplus
}
#endif 
#endif /* oi_socket_h */
