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
#include <oi.h>
#include <gnutls/gnutls.h>

#define HOST "127.0.0.1"
#define SOCKFILE "/tmp/oi.sock"
#define PORT "5000"

int nconnections; 

static void 
on_peer_close(oi_socket *socket)
{
  //printf("server connection closed\n");
#if HAVE_GNUTLS
#if SECURE
  gnutls_deinit(socket->session);
#endif
#endif
  free(socket);
}

static void 
on_client_timeout(oi_socket *socket)
{
  printf("client connection timeout\n");
  assert(0);
}

static void 
on_peer_timeout(oi_socket *socket)
{
  fprintf(stderr, "peer connection timeout\n");
  assert(0);
}

static void 
on_client_error(oi_socket *socket, struct oi_error e)
{
  assert(0);
}


#if HAVE_GNUTLS
#if SECURE
#define DH_BITS 768
gnutls_anon_server_credentials_t server_credentials;
const int kx_prio[] = { GNUTLS_KX_ANON_DH, 0 };
static gnutls_dh_params_t dh_params;

void anon_tls_init()
{
  gnutls_global_init();

  gnutls_dh_params_init (&dh_params);

  fprintf(stderr, "..");
  fsync((int)stderr);
  gnutls_dh_params_generate2 (dh_params, DH_BITS);
  fprintf(stderr, ".");

  gnutls_anon_allocate_server_credentials (&server_credentials);
  gnutls_anon_set_server_dh_params (server_credentials, dh_params);
}

void anon_tls_server(oi_socket *socket)
{
  gnutls_session_t session;
  socket->data = session;

  int r = gnutls_init(&session, GNUTLS_SERVER);
  assert(r == 0);
  gnutls_set_default_priority(session);
  gnutls_kx_set_priority (session, kx_prio);
  gnutls_credentials_set(session, GNUTLS_CRD_ANON, server_credentials);
  gnutls_dh_set_prime_bits(session, DH_BITS);

  oi_socket_set_secure_session(socket, session);
}

void anon_tls_client(oi_socket *socket)
{
  gnutls_session_t client_session;
  gnutls_anon_client_credentials_t client_credentials;

  gnutls_anon_allocate_client_credentials (&client_credentials);
  gnutls_init (&client_session, GNUTLS_CLIENT);
  gnutls_set_default_priority(client_session);
  gnutls_kx_set_priority(client_session, kx_prio);
  /* Need to enable anonymous KX specifically. */
  gnutls_credentials_set (client_session, GNUTLS_CRD_ANON, client_credentials);

  oi_socket_set_secure_session(socket, client_session);
  assert(socket->secure);
}

#endif // SECURE
#endif // HAVE_GNUTLS
