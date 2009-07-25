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
#include <evcom.h>
#include <gnutls/gnutls.h>

#define HOST "127.0.0.1"
#define SOCKFILE "/tmp/oi.sock"
#define PORT "5000"

static int nconnections; 

static void 
on_peer_close (evcom_socket *socket)
{
  assert(socket->errorno == 0);
  //printf("server connection closed\n");
  free(socket);
}

static void 
on_peer_timeout (evcom_socket *socket)
{
  assert(socket);
  fprintf(stderr, "peer connection timeout\n");
  assert(0);
}



// timeout must match the timeout in timeout.rb
#define TIMEOUT 5.0

static void 
on_peer_read (evcom_socket *socket, const void *base, size_t len)
{
  if(len == 0) return;

  evcom_socket_write_simple(socket, base, len);
}

static evcom_socket* 
on_server_connection (evcom_server *server, struct sockaddr *addr)
{
  assert(server);
  assert(addr);

  evcom_socket *socket = malloc(sizeof(evcom_socket));
  evcom_socket_init(socket, TIMEOUT);
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
  evcom_server server;

  //printf("sizeof(evcom_server): %d\n", sizeof(evcom_server));
  //printf("sizeof(evcom_socket): %d\n", sizeof(evcom_socket));

  evcom_server_init(&server);
  server.on_connection = on_server_connection;

  struct addrinfo *servinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  r = getaddrinfo(NULL, PORT, &hints, &servinfo);
  assert(r == 0);

  r = evcom_server_listen(&server, servinfo, 10);
  assert(r == 0);
  evcom_server_attach(EV_DEFAULT_ &server);

  ev_loop(EV_DEFAULT_ 0);

  freeaddrinfo(servinfo);

  return 0;
}
