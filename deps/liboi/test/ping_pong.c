#include "test/common.c"

#define PING "PING"
#define PONG "PONG"
#define EXCHANGES 100

int successful_ping_count; 

static void 
on_peer_read(oi_socket *socket, const void *base, size_t len)
{
  if(len == 0) 
    return;

  char buf[2000];
  strncpy(buf, base, len);
  buf[len] = 0;
  //printf("server got message: %s\n", buf);

  oi_socket_write_simple(socket, PONG, sizeof PONG);
}

static void 
on_peer_error(oi_socket *socket, struct oi_error e)
{
  assert(0);
}

static void 
on_client_close(oi_socket *socket)
{
  //printf("client connection closed\n");
  ev_unloop(socket->loop, EVUNLOOP_ALL);
}

static oi_socket* 
on_server_connection(oi_server *server, struct sockaddr *addr, socklen_t len)
{
  oi_socket *socket = malloc(sizeof(oi_socket));
  oi_socket_init(socket, 5.0);
  socket->on_read = on_peer_read;
  socket->on_error = on_peer_error;
  socket->on_close = on_peer_close;
  socket->on_timeout = on_peer_timeout;

  nconnections++;

#if HAVE_GNUTLS
# if SECURE
  anon_tls_server(socket);
# endif
#endif

  //printf("on server connection\n");

  return socket;
}

static void 
on_client_connect(oi_socket *socket)
{
  //printf("client connected. sending ping\n");
  oi_socket_write_simple(socket, PING, sizeof PING);
}

static void 
on_client_read(oi_socket *socket, const void *base, size_t len)
{
  char buf[200000];
  strncpy(buf, base, len);
  buf[len] = 0;
  //printf("client got message: %s\n", buf);
  
  if(strcmp(buf, PONG) == 0) {

    if(++successful_ping_count > EXCHANGES) {
      oi_socket_close(socket);
      return;
    } 
    oi_socket_write_simple(socket, PING, sizeof PING);
  } else {
    assert(0);
  }
}

int 
main(int argc, const char *argv[])
{
  int r;
  struct ev_loop *loop = ev_default_loop(0);
  oi_server server;
  oi_socket client;

  //printf("sizeof(oi_server): %d\n", sizeof(oi_server));
  //printf("sizeof(oi_socket): %d\n", sizeof(oi_socket));

  oi_server_init(&server, 10);
  server.on_connection = on_server_connection;

#if HAVE_GNUTLS
# if SECURE
  anon_tls_init();
# endif
#endif

  struct addrinfo *servinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
#if TCP
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    r = getaddrinfo(NULL, PORT, &hints, &servinfo);
    assert(r == 0);
#else
    struct stat tstat;
    if (lstat(SOCKFILE, &tstat) == 0) {
      if (S_ISSOCK(tstat.st_mode))
        unlink(SOCKFILE);
    }

    servinfo = malloc(sizeof(struct addrinfo));
    servinfo->ai_family = AF_UNIX;
    servinfo->ai_socktype = SOCK_STREAM;
    servinfo->ai_protocol = 0;

    struct sockaddr_un *sockaddr = calloc(sizeof(struct sockaddr_un), 1);
    sockaddr->sun_family = AF_UNIX;
    strcpy(sockaddr->sun_path, SOCKFILE);

    servinfo->ai_addr = (struct sockaddr*)sockaddr;
    servinfo->ai_addrlen = sizeof(struct sockaddr_un);
#endif
  r = oi_server_listen(&server, servinfo);
  assert(r == 0);
  oi_server_attach(&server, loop);

  oi_socket_init(&client, 5.0);
  client.on_read    = on_client_read;
  client.on_error   = on_client_error;
  client.on_connect = on_client_connect;
  client.on_close   = on_client_close;
  client.on_timeout = on_client_timeout;

#if HAVE_GNUTLS
# if SECURE
  anon_tls_client(&client);
# endif
#endif

  r = oi_socket_connect(&client, servinfo);
  assert(r == 0 && "problem connecting");
  oi_socket_attach(&client, loop);

  ev_loop(loop, 0);

  assert(successful_ping_count == EXCHANGES + 1);
  assert(nconnections == 1);

#if TCP
  freeaddrinfo(servinfo);
#endif

  return 0;
}
