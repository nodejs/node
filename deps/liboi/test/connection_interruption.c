#include "test/common.c"
#define NCONN 100
#define TIMEOUT 1000.0

static oi_server server;

static void 
on_peer_read(oi_socket *socket, const void *base, size_t len)
{
  assert(len == 0);
  oi_socket_write_simple(socket, "BYE", 3);
  //printf("server wrote bye\n");
}

static void 
on_peer_drain(oi_socket *socket)
{
  oi_socket_close(socket);
}

static void 
on_peer_error2(oi_socket *socket, struct oi_error e)
{
  if(e.domain == OI_ERROR_GNUTLS) return;
  assert(0);
}

static oi_socket* 
on_server_connection(oi_server *server, struct sockaddr *addr, socklen_t len)
{
  oi_socket *socket = malloc(sizeof(oi_socket));
  oi_socket_init(socket, TIMEOUT);
  socket->on_read    = on_peer_read;
  socket->on_drain   = on_peer_drain;
  socket->on_error   = on_peer_error2;
  socket->on_close   = on_peer_close;
  socket->on_timeout = on_peer_timeout;

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
  //printf("on client connection\n");
  oi_socket_write_eof(socket);
}

static void 
on_client_close(oi_socket *socket)
{
  oi_socket_close(socket); // already closed, but it shouldn't crash if we try to do it again

  //printf("client connection closed\n");
  if(++nconnections == NCONN) {
    oi_server_detach(&server);
    //printf("detaching server\n");
  }
}

static void 
on_client_read(oi_socket *socket, const void *base, size_t len)
{
  char buf[200000];
  strncpy(buf, base, len);
  buf[len] = 0;

  //printf("client got message: %s\n", buf);
  
  if(strcmp(buf, "BYE") == 0) {
    oi_socket_close(socket);
  } else {
    assert(0);
  }
}

int 
main(int argc, const char *argv[])
{
  int r;
  struct ev_loop *loop = ev_default_loop(0);

  oi_server_init(&server, 1000);
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

  oi_server_listen(&server, servinfo);
  oi_server_attach(&server, loop);

  int i;
  for(i = 0; i < NCONN; i++) {
    oi_socket *client = malloc(sizeof(oi_socket));
    oi_socket_init(client, TIMEOUT);
    client->on_read    = on_client_read;
    client->on_error   = on_client_error;
    client->on_connect = on_client_connect;
    client->on_close   = on_client_close;
    client->on_timeout = on_client_timeout;
#if HAVE_GNUTLS
#if SECURE
    anon_tls_client(client);
#endif
#endif
    r = oi_socket_connect(client, servinfo);
    assert(r == 0 && "problem connecting");
    oi_socket_attach(client, loop);
  }

  ev_loop(loop, 0);

  assert(nconnections == NCONN);

#if TCP
  freeaddrinfo(servinfo);
#endif

  return 0;
}
