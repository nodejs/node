#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>


#include <ev.h>
#include <evnet.h>
#include <gnutls/gnutls.h>

#define HOST "127.0.0.1"
#define SOCKFILE "/tmp/oi.sock"
#define PORT "5000"

static int nconnections; 

static void 
on_peer_close (evnet_socket *socket)
{
  assert(socket->errorno == 0);
  //printf("server connection closed\n");
  free(socket);
}

static void 
on_peer_timeout (evnet_socket *socket)
{
  assert(socket);
  fprintf(stderr, "peer connection timeout\n");
  assert(0);
}



// timeout must match the timeout in timeout.rb
#define TIMEOUT 5.0

static void 
on_peer_read (evnet_socket *socket, const void *base, size_t len)
{
  if(len == 0) return;

  evnet_socket_write_simple(socket, base, len);
}

static evnet_socket* 
on_server_connection (evnet_server *server, struct sockaddr *addr)
{
  assert(server);
  assert(addr);

  evnet_socket *socket = malloc(sizeof(evnet_socket));
  evnet_socket_init(socket, TIMEOUT);
  socket->on_read = on_peer_read;
  socket->on_close = on_peer_close;
  socket->on_timeout = on_peer_timeout;

  nconnections++;


  //printf("on server connection\n");

  return socket;
}

int 
main (void)
{
  int r;
  evnet_server server;

  //printf("sizeof(evnet_server): %d\n", sizeof(evnet_server));
  //printf("sizeof(evnet_socket): %d\n", sizeof(evnet_socket));

  evnet_server_init(&server);
  server.on_connection = on_server_connection;

  struct addrinfo *servinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  r = getaddrinfo(NULL, PORT, &hints, &servinfo);
  assert(r == 0);

  r = evnet_server_listen(&server, servinfo, 10);
  assert(r == 0);
  evnet_server_attach(EV_DEFAULT_ &server);

  ev_loop(EV_DEFAULT_ 0);

  freeaddrinfo(servinfo);

  return 0;
}
