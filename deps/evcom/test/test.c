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
# include <gnutls/gnutls.h>
#endif

#define MARK_PROGRESS write(STDERR_FILENO, ".", 1)

#define SOCKFILE "/tmp/oi.sock"
#define PORT 5000

static evcom_server server;
static int nconnections; 
static int use_tls;
static int got_server_close;

static void
common_on_server_close (evcom_server *server, int errorno)
{
  assert(server);
  assert(errorno == 0);
  got_server_close = 1;
}

static void 
common_on_peer_close (evcom_stream *stream)
{
  assert(stream->errorno == 0);
  printf("server connection closed\n");
#if EVCOM_HAVE_GNUTLS
  assert(stream->gnutls_errorno == 0);
  if (use_tls) gnutls_deinit(stream->session);
#endif
  free(stream);
}

static void 
common_on_client_timeout (evcom_stream *stream)
{
  assert(stream);
  printf("client connection timeout\n");
}

static void 
common_on_peer_timeout (evcom_stream *stream)
{
  assert(stream);
  fprintf(stderr, "peer connection timeout\n");
  assert(0);
}

#if EVCOM_HAVE_GNUTLS
#define DH_BITS 768
static gnutls_anon_server_credentials_t server_credentials;
const int kx_prio[] = { GNUTLS_KX_ANON_DH, 0 };
static gnutls_dh_params_t dh_params;

void anon_tls_server (evcom_stream *stream)
{
  gnutls_session_t session;
  stream->data = session;

  int r = gnutls_init(&session, GNUTLS_SERVER);
  assert(r == 0);
  gnutls_set_default_priority(session);
  gnutls_kx_set_priority (session, kx_prio);
  gnutls_credentials_set(session, GNUTLS_CRD_ANON, server_credentials);
  gnutls_dh_set_prime_bits(session, DH_BITS);

  evcom_stream_set_secure_session(stream, session);
}

void anon_tls_client (evcom_stream *stream)
{
  gnutls_session_t client_session;
  gnutls_anon_client_credentials_t client_credentials;

  gnutls_anon_allocate_client_credentials (&client_credentials);
  gnutls_init(&client_session, GNUTLS_CLIENT);
  gnutls_set_default_priority(client_session);
  gnutls_kx_set_priority(client_session, kx_prio);
  /* Need to enable anonymous KX specifically. */
  gnutls_credentials_set(client_session, GNUTLS_CRD_ANON, client_credentials);

  evcom_stream_set_secure_session(stream, client_session);
  assert(stream->flags & EVCOM_SECURE);
}

#endif // EVCOM_HAVE_GNUTLS





#define PING "PING"
#define PONG "PONG"
#define EXCHANGES 5000
#define PINGPONG_TIMEOUT 5.0

static int successful_ping_count; 

static void 
pingpong_on_peer_read (evcom_stream *stream, const void *base, size_t len)
{
  if (len == 0) {
    evcom_stream_close(stream);
    return;
  }

  char buf[2000];
  strncpy(buf, base, len);
  buf[len] = 0;
  printf("server got message: %s\n", buf);

  evcom_stream_write_simple(stream, PONG, sizeof PONG);
}

static void 
pingpong_on_client_close (evcom_stream *stream)
{
  assert(stream);
  printf("client connection closed\n");
  evcom_server_close(&server);
}

static evcom_stream* 
pingpong_on_server_connection (evcom_server *_server, struct sockaddr *addr)
{
  assert(_server == &server);
  assert(addr);

  evcom_stream *stream = malloc(sizeof(evcom_stream));
  evcom_stream_init(stream, PINGPONG_TIMEOUT);
  stream->on_read = pingpong_on_peer_read;
  stream->on_close = common_on_peer_close;
  stream->on_timeout = common_on_peer_timeout;

  nconnections++;

#if EVCOM_HAVE_GNUTLS
  if (use_tls) anon_tls_server(stream);
#endif

  printf("on server connection\n");

  return stream;
}

static void 
pingpong_on_client_connect (evcom_stream *stream)
{
  printf("client connected. sending ping\n");
  evcom_stream_write_simple(stream, PING, sizeof PING);
}

static void 
pingpong_on_client_read (evcom_stream *stream, const void *base, size_t len)
{
  if(len == 0) {
    evcom_stream_close(stream);
    return;
  }

  assert(len = strlen(PONG));

  char buf[len+1];
  strncpy(buf, base, len);
  buf[len] = 0;
  printf("client got message: %s\n", buf);
  
  assert(strcmp(buf, PONG) == 0);

  if (++successful_ping_count > EXCHANGES) {
    evcom_stream_close(stream);
    return;
  } 

  if (successful_ping_count % (EXCHANGES/20) == 0) MARK_PROGRESS;

  evcom_stream_write_simple(stream, PING, sizeof PING);
}

int
pingpong (struct sockaddr *address)
{
  int r;
  evcom_stream client;
  
  successful_ping_count = 0;
  nconnections = 0;
  got_server_close = 0;

  printf("sizeof(evcom_server): %d\n", sizeof(evcom_server));
  printf("sizeof(evcom_stream): %d\n", sizeof(evcom_stream));

  evcom_server_init(&server);
  server.on_connection = pingpong_on_server_connection;
  server.on_close = common_on_server_close;

  r = evcom_server_listen(&server, address, 10);
  assert(r == 0);
  evcom_server_attach(EV_DEFAULT_ &server);

  evcom_stream_init(&client, PINGPONG_TIMEOUT);
  client.on_read    = pingpong_on_client_read;
  client.on_connect = pingpong_on_client_connect;
  client.on_close   = pingpong_on_client_close;
  client.on_timeout = common_on_client_timeout;

#if EVCOM_HAVE_GNUTLS
  if (use_tls) anon_tls_client(&client);
#endif

  r = evcom_stream_connect(&client, address);
  assert(r == 0 && "problem connecting");
  evcom_stream_attach(EV_DEFAULT_ &client);

  ev_loop(EV_DEFAULT_ 0);

  printf("successful_ping_count = %d\n", successful_ping_count);
  assert(successful_ping_count == EXCHANGES + 1);
  assert(nconnections == 1);
  assert(got_server_close);

  return 0;
}




#define NCONN 100
#define CONNINT_TIMEOUT 1000.0

static void 
connint_on_peer_read(evcom_stream *stream, const void *base, size_t len)
{
  assert(base);
  assert(len == 0);
  evcom_stream_write_simple(stream, "BYE", 3);
  printf("server wrote bye\n");
}

static void 
connint_on_peer_drain(evcom_stream *stream)
{
  evcom_stream_close(stream);
}

static evcom_stream* 
connint_on_server_connection(evcom_server *_server, struct sockaddr *addr)
{
  assert(_server == &server);
  assert(addr);

  evcom_stream *stream = malloc(sizeof(evcom_stream));
  evcom_stream_init(stream, CONNINT_TIMEOUT);
  stream->on_read    = connint_on_peer_read;
  stream->on_drain   = connint_on_peer_drain;
  stream->on_close   = common_on_peer_close;
  stream->on_timeout = common_on_peer_timeout;

#if EVCOM_HAVE_GNUTLS
  if (use_tls) anon_tls_server(stream);
#endif

  printf("on server connection\n");

  return stream;
}

static void 
connint_on_client_connect (evcom_stream *stream)
{
  printf("on client connection\n");
  evcom_stream_close(stream);
}

static void 
connint_on_client_close (evcom_stream *stream)
{
  evcom_stream_close(stream); // already closed, but it shouldn't crash if we try to do it again

  printf("client connection closed\n");

  if (nconnections % (NCONN/20) == 0) MARK_PROGRESS;

  if(++nconnections == NCONN) {
    evcom_server_close(&server);
    printf("closing server\n");
  }
}

static void 
connint_on_client_read (evcom_stream *stream, const void *base, size_t len)
{
  if (len == 0) {
    evcom_stream_close(stream);
    return;
  }

  char buf[200000];
  strncpy(buf, base, len);
  buf[len] = 0;

  printf("client got message: %s\n", buf);
  
  assert(strcmp(buf, "BYE") == 0);
  evcom_stream_close(stream);
}

int 
connint (struct sockaddr *address)
{
  int r;

  nconnections = 0;
  got_server_close = 0;

  evcom_server_init(&server);
  server.on_connection = connint_on_server_connection;
  server.on_close = common_on_server_close;


  evcom_server_listen(&server, address, 1000);
  evcom_server_attach(EV_DEFAULT_ &server);

  evcom_stream clients[NCONN];
  int i;
  for (i = 0; i < NCONN; i++) {
    evcom_stream *client = &clients[i];
    evcom_stream_init(client, CONNINT_TIMEOUT);
    client->on_read    = connint_on_client_read;
    client->on_connect = connint_on_client_connect;
    client->on_close   = connint_on_client_close;
    client->on_timeout = common_on_client_timeout;
#if EVCOM_HAVE_GNUTLS
    if (use_tls) anon_tls_client(client);
#endif
    r = evcom_stream_connect(client, address);
    assert(r == 0 && "problem connecting");
    evcom_stream_attach(EV_DEFAULT_ client);
  }

  ev_loop(EV_DEFAULT_ 0);

  assert(nconnections == NCONN);
  assert(got_server_close);

  return 0;
}


struct sockaddr *
create_unix_address (void)
{
  struct stat tstat;
  if (lstat(SOCKFILE, &tstat) == 0) {
    assert(S_ISSOCK(tstat.st_mode));
    unlink(SOCKFILE);
  }

  struct sockaddr_un *address = calloc(1, sizeof(struct sockaddr_un));
  address->sun_family = AF_UNIX;
  strcpy(address->sun_path, SOCKFILE);

  return (struct sockaddr*)address;
}

void
free_unix_address (struct sockaddr *address)
{
  free(address);
}


int
main (void)
{
#if EVCOM_HAVE_GNUTLS
  gnutls_global_init();

  gnutls_dh_params_init (&dh_params);

  fsync((int)stderr);
  gnutls_dh_params_generate2 (dh_params, DH_BITS);

  gnutls_anon_allocate_server_credentials (&server_credentials);
  gnutls_anon_set_server_dh_params (server_credentials, dh_params);
#endif

  struct sockaddr_in tcp_address;
  memset(&tcp_address, 0, sizeof(struct sockaddr_in));
  tcp_address.sin_family = AF_INET;
  tcp_address.sin_port = htons(PORT);
  tcp_address.sin_addr.s_addr = INADDR_ANY;

  
  use_tls = 0;
  assert(pingpong((struct sockaddr*)&tcp_address) == 0);
  assert(connint((struct sockaddr*)&tcp_address) == 0);

#if EVCOM_HAVE_GNUTLS
  use_tls = 1;
  assert(pingpong((struct sockaddr*)&tcp_address) == 0);
  assert(connint((struct sockaddr*)&tcp_address) == 0);
#endif 


  struct sockaddr *unix_address;

  
  use_tls = 0;

  unix_address = create_unix_address();
  assert(pingpong(unix_address) == 0);
  free_unix_address(unix_address);

  unix_address = create_unix_address();
  assert(connint(unix_address) == 0);
  free_unix_address(unix_address);

#if EVCOM_HAVE_GNUTLS
  use_tls = 1;

  unix_address = create_unix_address();
  assert(pingpong(unix_address) == 0);
  free_unix_address(unix_address);

  unix_address = create_unix_address();
  assert(connint(unix_address) == 0);
  free_unix_address(unix_address);
#endif 


  return 0;
}
