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
#if EVCOM_HAVE_GNUTLS
#  include <gnutls/gnutls.h>
#endif

#define HOST "127.0.0.1"
#define SOCKFILE "/tmp/oi.sock"
#define PORT 5000

static int nconnections; 

static void 
on_peer_close (evcom_stream *stream)
{
  assert(stream->errorno == 0);
  //printf("server connection closed\n");
  free(stream);
}

static void 
on_peer_timeout (evcom_stream *stream)
{
  assert(stream);
  fprintf(stderr, "peer connection timeout\n");
  assert(0);
}



// timeout must match the timeout in timeout.rb
#define TIMEOUT 5.0

static void 
on_peer_read (evcom_stream *stream, const void *base, size_t len)
{
  if(len == 0) return;

  evcom_stream_write(stream, base, len);
}

static evcom_stream* 
on_server_connection (evcom_server *server, struct sockaddr *addr)
{
  assert(server);
  assert(addr);

  evcom_stream *stream = malloc(sizeof(evcom_stream));
  evcom_stream_init(stream);
  stream->on_read = on_peer_read;
  stream->on_close = on_peer_close;
  stream->on_timeout = on_peer_timeout;
  evcom_stream_reset_timeout(stream, TIMEOUT);

  nconnections++;


  //printf("on server connection\n");

  return stream;
}

int 
main (void)
{
  int r;
  evcom_server server;

  //printf("sizeof(evcom_server): %d\n", sizeof(evcom_server));
  //printf("sizeof(evcom_stream): %d\n", sizeof(evcom_stream));

  evcom_server_init(&server);
  server.on_connection = on_server_connection;

  struct sockaddr_in address;
  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_port = htons(PORT);
  address.sin_addr.s_addr = INADDR_ANY;

  r = evcom_server_listen(&server, (struct sockaddr*)&address, 10);
  assert(r == 0);
  evcom_server_attach(EV_DEFAULT_ &server);

  ev_loop(EV_DEFAULT_ 0);

  return 0;
}
