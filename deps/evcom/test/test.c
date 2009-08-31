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

#undef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MARK_PROGRESS(c,cur,max) \
  if (cur % (MAX(max,50)/50) == 0) write(STDERR_FILENO, c, 1)

#define SOCKFILE "/tmp/oi.sock"
#define PORT 5000

static evcom_server server;
static int nconnections;
static int use_tls;
static int got_server_close;

static void
common_on_server_close (evcom_server *s)
{
  printf("server on_close\n");
  assert(s == &server);
  assert(s->errorno == 0);
  got_server_close = 1;
  evcom_server_detach(s);
}

static void
common_on_peer_close (evcom_stream *stream)
{
  assert(EVCOM_CLOSED == evcom_stream_state(stream));
  assert(stream->errorno == 0);
  printf("server connection closed\n");
#if EVCOM_HAVE_GNUTLS
  assert(stream->gnutls_errorno == 0);
  if (use_tls) gnutls_deinit(stream->session);
#endif
  evcom_stream_detach(stream);
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
#define EXCHANGES 500
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

  evcom_stream_write(stream, PONG, sizeof PONG);
}

static void
pingpong_on_client_close (evcom_stream *stream)
{
  assert(EVCOM_CLOSED == evcom_stream_state(stream));
  assert(stream);
  printf("client connection closed\n");
  evcom_server_close(&server);
  evcom_stream_detach(stream);
}

static evcom_stream*
pingpong_on_server_connection (evcom_server *_server, struct sockaddr *addr)
{
  assert(_server == &server);
  assert(addr);

  evcom_stream *stream = malloc(sizeof(evcom_stream));
  evcom_stream_init(stream);
  stream->on_read = pingpong_on_peer_read;
  stream->on_close = common_on_peer_close;
  stream->on_timeout = common_on_peer_timeout;
  evcom_stream_reset_timeout(stream, PINGPONG_TIMEOUT);

  assert(EVCOM_INITIALIZED == evcom_stream_state(stream));

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
  evcom_stream_write(stream, PING, sizeof PING);
  assert(EVCOM_CONNECTED_RW == evcom_stream_state(stream));
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

  MARK_PROGRESS(".", successful_ping_count, EXCHANGES);

  evcom_stream_write(stream, PING, sizeof PING);
}

int
pingpong (struct sockaddr *address)
{
  int r;
  evcom_stream client;

  successful_ping_count = 0;
  nconnections = 0;
  got_server_close = 0;

  evcom_server_init(&server);
  server.on_connection = pingpong_on_server_connection;
  server.on_close = common_on_server_close;

  r = evcom_server_listen(&server, address, 10);
  assert(r == 0);
  evcom_server_attach(EV_DEFAULT_ &server);

  evcom_stream_init(&client);
  client.on_read    = pingpong_on_client_read;
  client.on_connect = pingpong_on_client_connect;
  client.on_close   = pingpong_on_client_close;
  client.on_timeout = common_on_client_timeout;
  evcom_stream_reset_timeout(&client, PINGPONG_TIMEOUT);

  assert(EVCOM_INITIALIZED == evcom_stream_state(&client));

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



#define NCONN 50
#define CONNINT_TIMEOUT 10.0

static void
send_bye_and_close(evcom_stream *stream, const void *base, size_t len)
{
  assert(base);
  assert(len == 0);
  evcom_stream_write(stream, "BYE", 3);
  printf("server wrote bye\n");
  evcom_stream_close(stream);
}

static evcom_stream*
connint_on_connection(evcom_server *_server, struct sockaddr *addr)
{
  assert(_server == &server);
  assert(addr);

  evcom_stream *stream = malloc(sizeof(evcom_stream));
  evcom_stream_init(stream);
  stream->on_read    = send_bye_and_close;
  stream->on_close   = common_on_peer_close;
  stream->on_timeout = common_on_peer_timeout;
  evcom_stream_reset_timeout(stream, CONNINT_TIMEOUT);

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

  MARK_PROGRESS(".", nconnections, NCONN);

  if(++nconnections == NCONN) {
    evcom_server_close(&server);
    printf("closing server\n");
  }

  evcom_stream_detach(stream);
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
  server.on_connection = connint_on_connection;
  server.on_close = common_on_server_close;

  evcom_server_listen(&server, address, 1000);
  evcom_server_attach(EV_DEFAULT_ &server);

  evcom_stream clients[NCONN];
  int i;
  for (i = 0; i < NCONN; i++) {
    evcom_stream *client = &clients[i];
    evcom_stream_init(client);
    client->on_read    = connint_on_client_read;
    client->on_connect = connint_on_client_connect;
    client->on_close   = connint_on_client_close;
    client->on_timeout = common_on_client_timeout;
    evcom_stream_reset_timeout(client, CONNINT_TIMEOUT);
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


static evcom_reader reader;
static evcom_writer writer;
static int reader_got_close = 0;
static int reader_got_eof = 0;
static int reader_got_hello = 0;
static int reader_cnt = 0;
static int writer_got_close = 0;
#define PIPE_MSG "hello world"
#define PIPE_CNT 5000

static void
reader_read (evcom_reader *r, const void *str, size_t len)
{
  assert(r == &reader);

  if (len == 0) {
    reader_got_eof = 1;
    return;
  }

  assert(len == strlen(PIPE_MSG));

  if (strncmp(str, PIPE_MSG, strlen(PIPE_MSG)) == 0) {
    reader_got_hello = 1;
  }

  if (++reader_cnt < PIPE_CNT) {
    MARK_PROGRESS(".", reader_cnt, PIPE_CNT);
    evcom_writer_write(&writer, PIPE_MSG, strlen(PIPE_MSG));
  } else {
    evcom_writer_close(&writer);
  }
}

static void
reader_close (evcom_reader *r)
{
  assert(r == &reader);
  reader_got_close = 1;
  evcom_reader_detach(r);
}

static void
writer_close (evcom_writer *w)
{
  assert(w == &writer);
  writer_got_close = 1;
  evcom_writer_detach(w);
}

int
pipe_stream (void)
{
  reader_cnt = 0;
  reader_got_close = 0;
  reader_got_hello = 0;
  reader_got_eof   = 0;
  writer_got_close = 0;

  int pipefd[2];
  int r = pipe(pipefd);
  if (r < 0) {
    perror("pipe()");
    return -1;
  }

  evcom_reader_init(&reader);
  reader.on_read = reader_read;
  reader.on_close = reader_close;
  evcom_reader_set(&reader, pipefd[0]);
  evcom_reader_attach(EV_DEFAULT_ &reader);

  evcom_writer_init(&writer);
  writer.on_close = writer_close;
  evcom_writer_set(&writer, pipefd[1]);
  evcom_writer_attach(EV_DEFAULT_ &writer);

  evcom_writer_write(&writer, PIPE_MSG, strlen(PIPE_MSG));

  ev_loop(EV_DEFAULT_ 0);

  assert(reader_got_close);
  assert(reader_got_hello);
  assert(reader_got_eof);
  assert(writer_got_close);
  assert(reader_cnt == PIPE_CNT);

  return 0;
}

#define PAIR_PINGPONG_TIMEOUT 5000.0
#define PAIR_PINGPONG_EXCHANGES 50
static int a_got_close;
static int a_got_connect;
static int b_got_close;
static int b_got_connect;
static int pair_pingpong_cnt;
static evcom_stream a, b;

void a_connect (evcom_stream *stream)
{
  assert(stream == &a);
  a_got_connect = 1;
}

void a_close (evcom_stream *stream)
{
  evcom_stream_detach(stream);
  assert(stream == &a);
  a_got_close = 1;

  assert(stream->errorno == 0);
#if EVCOM_HAVE_GNUTLS
  if (stream->gnutls_errorno) {
    fprintf(stderr, "\nGNUTLS ERROR: %s\n", gnutls_strerror(stream->gnutls_errorno));
  }
  assert(stream->gnutls_errorno == 0);
  if (use_tls) gnutls_deinit(stream->session);
#endif
}

void a_read (evcom_stream *stream, const void *buf, size_t len)
{
  assert(stream == &a);
  if (len == 0) return;

  assert(len == strlen(PONG));
  assert(strncmp(buf, PONG, strlen(PONG)) == 0);

  if (++pair_pingpong_cnt < PAIR_PINGPONG_EXCHANGES) {
    evcom_stream_write(&a, PING, strlen(PING));
  } else if (pair_pingpong_cnt == PAIR_PINGPONG_EXCHANGES) {
    evcom_stream_close(stream);
  }

  MARK_PROGRESS(".", pair_pingpong_cnt, PAIR_PINGPONG_EXCHANGES);
}

void b_connect (evcom_stream *stream)
{
  assert(stream == &b);
  b_got_connect = 1;
}

void b_close (evcom_stream *stream)
{
  evcom_stream_detach(stream);
  assert(stream == &b);
  b_got_close = 1;

  assert(stream->errorno == 0);
#if EVCOM_HAVE_GNUTLS
  if (stream->gnutls_errorno) {
    fprintf(stderr, "\nGNUTLS ERROR: %s\n", gnutls_strerror(stream->gnutls_errorno));
  }
  assert(stream->gnutls_errorno == 0);
  if (use_tls) gnutls_deinit(stream->session);
#endif
}

void b_read (evcom_stream *stream, const void *buf, size_t len)
{
  assert(stream == &b);
  if (len == 0) {
    evcom_stream_close(stream);
    return;
  }

  assert(len == strlen(PING));
  assert(strncmp(buf, PING, strlen(PING)) == 0);

  evcom_stream_write(&b, PONG, strlen(PONG));
}

int
pair_pingpong (int use_pipe)
{
  a_got_close = 0;
  a_got_connect = 0;
  b_got_close = 0;
  b_got_connect = 0;
  pair_pingpong_cnt = 0;

  evcom_stream_init(&a);
  a.on_close = a_close;
  a.on_connect = a_connect;
  a.on_read = a_read;
  evcom_stream_reset_timeout(&a, PAIR_PINGPONG_TIMEOUT);
#if EVCOM_HAVE_GNUTLS
  if (use_tls) anon_tls_client(&a);
#endif

  evcom_stream_init(&b);
  b.on_close = b_close;
  b.on_connect = b_connect;
  b.on_read = b_read;
  evcom_stream_reset_timeout(&b, PAIR_PINGPONG_TIMEOUT);
#if EVCOM_HAVE_GNUTLS
  if (use_tls) anon_tls_server(&b);
#endif

  if (use_pipe) {
    int pipeA[2], pipeB[2];
    assert(0 == pipe(pipeA));
    assert(0 == pipe(pipeB));

    evcom_stream_assign_fds(&a, pipeA[0], pipeB[1]);
    evcom_stream_assign_fds(&b, pipeB[0], pipeA[1]);

  } else {
    int r = evcom_stream_pair(&a, &b);
    assert(r == 0);
  }

  evcom_stream_attach(EV_DEFAULT_ &a);
  evcom_stream_attach(EV_DEFAULT_ &b);

  evcom_stream_write(&a, PING, strlen(PING));

  ev_loop(EV_DEFAULT_ 0);

  assert(a_got_close);
  assert(a_got_connect);
  assert(b_got_close);
  assert(b_got_connect);
  assert(pair_pingpong_cnt == PAIR_PINGPONG_EXCHANGES);

  return 0;
}


static void
free_stream (evcom_stream *stream)
{
  assert(stream->errorno == 0);
  free(stream);
}

#define ZERO_TIMEOUT 50.0
static size_t zero_to_write = 0;
static size_t zero_written = 0;
static size_t zero_read = 0;
static size_t zero_client_closed = 0;

static void
error_out (evcom_stream *stream)
{
  assert(stream);
  fprintf(stderr, "peer connection timeout\n");
  assert(0);
}

static void
echo (evcom_stream *stream, const void *base, size_t len)
{
  if(len == 0) {
    fprintf(stderr, "close");
    evcom_stream_close(stream);
  } else {
    evcom_stream_write(stream, base, len);
  }
}

static evcom_stream*
make_echo_connection (evcom_server *server, struct sockaddr *addr)
{
  assert(server);
  assert(addr);

  evcom_stream *stream = malloc(sizeof(evcom_stream));
  evcom_stream_init(stream);
  stream->on_read = echo;
  stream->on_close = free_stream;
  stream->on_timeout = error_out;
  evcom_stream_reset_timeout(stream, ZERO_TIMEOUT);

#if EVCOM_HAVE_GNUTLS
  if (use_tls) anon_tls_server(stream);
#endif

  return stream;
}


static void
zero_start (evcom_stream *stream)
{
  evcom_stream_write(stream, "0", 1);
  zero_written++;
}

static void
zero_close (evcom_stream *stream)
{
  assert(stream);
  zero_client_closed = 1;
}

static void
zero_recv (evcom_stream *stream, const void *buf, size_t len)
{
  MARK_PROGRESS("-", zero_read, zero_to_write);
  zero_read += len;

  size_t i;

  for (i = 0; i < len; i++) {
    assert(((char*)buf)[i] == '0');
  }

  for (i = 0; i < MIN(zero_to_write - zero_written, 90000); i++) {
    evcom_stream_write(stream, "0", 1);
    zero_written++;

    MARK_PROGRESS(".", zero_written, zero_to_write);

    if (zero_written == zero_to_write) {

      fprintf(stderr, "CLOSE");
      evcom_stream_close(stream);
    }
  }

  if (len == 0) {
    fprintf(stderr, "finish");
    evcom_server_close(&server);
  }
}

int
zero_stream (struct sockaddr *address, size_t to_write)
{
  int r;

  assert(to_write >= 1024); // should be kind of big at least.
  zero_to_write = to_write;
  got_server_close = 0;
  zero_written = 0;
  zero_read = 0;
  zero_client_closed = 0;

  evcom_server_init(&server);
  server.on_connection = make_echo_connection;
  server.on_close      = common_on_server_close;

  evcom_server_listen(&server, address, 1000);
  evcom_server_attach(EV_DEFAULT_ &server);

  evcom_stream client;
  evcom_stream_init(&client);
  client.on_read    = zero_recv;
  client.on_connect = zero_start;
  client.on_close   = zero_close;
  client.on_timeout = error_out;
  evcom_stream_reset_timeout(&client, ZERO_TIMEOUT);
#if EVCOM_HAVE_GNUTLS
  if (use_tls) anon_tls_client(&client);
#endif
  r = evcom_stream_connect(&client, address);
  assert(r == 0 && "problem connecting");
  evcom_stream_attach(EV_DEFAULT_ &client);

  ev_loop(EV_DEFAULT_ 0);

  assert(got_server_close);
  assert(zero_written == zero_to_write);
  assert(zero_read == zero_to_write);
  assert(zero_client_closed) ;

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
  struct stat tstat;
  if (lstat(SOCKFILE, &tstat) == 0) {
    assert(S_ISSOCK(tstat.st_mode));
    unlink(SOCKFILE);
  }
  free(address);
}


int
main (void)
{
  fprintf(stderr, "sizeof(evcom_server): %d\n", (int)sizeof(evcom_server));
  fprintf(stderr, "sizeof(evcom_stream): %d\n", (int)sizeof(evcom_stream));
  fprintf(stderr, "sizeof(evcom_reader): %d\n", (int)sizeof(evcom_reader));
  fprintf(stderr, "sizeof(evcom_writer): %d\n", (int)sizeof(evcom_writer));

  evcom_ignore_sigpipe();

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

  fprintf(stderr, "pair_pingpong use_pipe=1: ");
  assert(pair_pingpong(1) == 0);
  fprintf(stderr, "\n");

  fprintf(stderr, "pair_pingpong use_pipe=0: ");
  assert(pair_pingpong(0) == 0);
  fprintf(stderr, "\n");

  fprintf(stderr, "zero_stream tcp: ");
  assert(zero_stream((struct sockaddr*)&tcp_address, 5*1024*1024) == 0);
  fprintf(stderr, "\n");

  fprintf(stderr, "pipe_stream: ");
  assert(pipe_stream() == 0);
  fprintf(stderr, "\n");

  fprintf(stderr, "pingpong tcp: ");
  assert(pingpong((struct sockaddr*)&tcp_address) == 0);
  fprintf(stderr, "\n");

  fprintf(stderr, "connint tcp: ");
  assert(connint((struct sockaddr*)&tcp_address) == 0);
  fprintf(stderr, "\n");

#if EVCOM_HAVE_GNUTLS
  use_tls = 1;

  fprintf(stderr, "zero_stream ssl: ");
  assert(zero_stream((struct sockaddr*)&tcp_address, 50*1024) == 0);
  fprintf(stderr, "\n");

  fprintf(stderr, "pair_pingpong ssl use_pipe=1: ");
  assert(pair_pingpong(1) == 0);
  fprintf(stderr, "\n");

  fprintf(stderr, "pair_pingpong ssl use_pipe=0: ");
  assert(pair_pingpong(0) == 0);
  fprintf(stderr, "\n");

  fprintf(stderr, "pingpong ssl: ");
  assert(pingpong((struct sockaddr*)&tcp_address) == 0);
  fprintf(stderr, "\n");

  fprintf(stderr, "connint ssl: ");
  assert(connint((struct sockaddr*)&tcp_address) == 0);
  fprintf(stderr, "\n");

#endif

  struct sockaddr *unix_address;

  use_tls = 0;

  fprintf(stderr, "pingpong unix: ");
  unix_address = create_unix_address();
  assert(pingpong(unix_address) == 0);
  free_unix_address(unix_address);
  fprintf(stderr, "\n");

  fprintf(stderr, "connint unix: ");
  unix_address = create_unix_address();
  assert(connint(unix_address) == 0);
  free_unix_address(unix_address);
  fprintf(stderr, "\n");

#if EVCOM_HAVE_GNUTLS
  use_tls = 1;

  fprintf(stderr, "pingpong unix ssl: ");
  unix_address = create_unix_address();
  assert(pingpong(unix_address) == 0);
  free_unix_address(unix_address);
  fprintf(stderr, "\n");

  fprintf(stderr, "connint unix ssl: ");
  unix_address = create_unix_address();
  assert(connint(unix_address) == 0);
  free_unix_address(unix_address);
  fprintf(stderr, "\n");
#endif

  return 0;
}
