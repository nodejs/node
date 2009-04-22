/* This test uses most of oi's facilities. It starts by opening a new file
 * and writing N characters to it. Once the first chunk is written, it opens
 * that file with another handle 
 *
 * /tmp/oi_test_src == /tmp/oi_test_dst
 *        |                  ^
 *        |                  |
 * stream |                  | write
 *        |                  |
 *        V                  |
 *     client  ----------> server/connection
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

#include <oi.h>
#include <oi_file.h>

#define PORT "5555"

static struct ev_loop *loop;
static oi_file file_src;
static oi_file file_dst;
static oi_socket client;
static oi_server server;
static oi_socket connection;
static int got_connection = 0;

static char *src_filename;
static char *dst_filename;

static void
file_error(oi_file *_)
{
  assert(0);
}

static void
on_file_dst_open (oi_file *_)
{
  assert(connection.fd > 0);
  oi_socket_read_start(&connection);
  //printf("file_dst is open\n");
}

static void
on_file_dst_close (oi_file *_)
{
  //printf("file dst closed\n");
  oi_file_detach(&file_dst);
}

static void
on_connection_read (oi_socket *_, const void *buf, size_t count)
{
  assert(file_dst.fd > 0);
  if(count == 0) {
    file_dst.on_drain = oi_file_close;
    oi_socket_close(&connection);
  } else {
    oi_file_write_simple(&file_dst, buf, count);
  }
}

static void
on_connection_connect (oi_socket *_)
{
  oi_file_init(&file_dst);
  file_dst.on_open  = on_file_dst_open;
  file_dst.on_close = on_file_dst_close;
  oi_file_open_path(&file_dst, dst_filename, O_WRONLY | O_CREAT, 0644);
  oi_file_attach(&file_dst, loop);

  oi_socket_read_stop(&connection);
}

static void
on_connection_timeout (oi_socket *_)
{
  assert(0);
}

static void
on_connection_error (oi_socket *_, struct oi_error e)
{
  assert(0);
}

static void
on_connection_close (oi_socket *_)
{
  oi_server_close(&server);
  oi_server_detach(&server);
}

static oi_socket* 
on_server_connection(oi_server *_, struct sockaddr *addr, socklen_t len)
{
  assert(got_connection == FALSE);
  oi_socket_init(&connection, 5.0);
  connection.on_connect = on_connection_connect;
  connection.on_read    = on_connection_read;
  connection.on_error   = on_connection_error;
  connection.on_close   = on_connection_close;
  connection.on_timeout = on_connection_timeout;
  connection.on_drain   = oi_socket_close;
  got_connection = TRUE;
  //printf("on server connection\n");
  return &connection;
}

static void
on_client_read (oi_socket *_, const void *buf, size_t count)
{
  assert(0);
}

static void
on_client_error (oi_socket *_, struct oi_error e)
{
  assert(0);
}

static void
on_client_connect (oi_socket *_)
{
  if(file_src.fd > 0) {
    oi_file_send(&file_src, &client, 0, 50*1024); 
  }
}

static void
on_client_drain (oi_socket *_)
{
  oi_socket_close(&client);
  oi_file_close(&file_src);
}

static void
on_file_src_open (oi_file *_)
{
  if(client.fd > 0)  {
    oi_file_send(&file_src, &client, 0, 50*1024); 
  }
}

static void
on_client_timeout (oi_socket *_)
{
  assert(0);
}

int
main(int argc, char *argv[])
{
  int r; 
  loop = ev_default_loop(0);

  assert(argc == 3);
  src_filename = argv[1];
  dst_filename = argv[2];

  assert(strlen(src_filename) > 0);
  assert(strlen(dst_filename) > 0);

  oi_server_init(&server, 10);
  server.on_connection = on_server_connection;

  struct addrinfo *servinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  r = getaddrinfo(NULL, PORT, &hints, &servinfo);
  assert(r == 0);

  r = oi_server_listen(&server, servinfo);
  assert(r >= 0 && "problem listening");
  oi_server_attach(&server, loop);

  oi_socket_init(&client, 5.0);
  client.on_read    = on_client_read;
  client.on_error   = on_client_error;
  client.on_connect = on_client_connect;
  client.on_timeout = on_client_timeout;
  //client.on_close   = oi_socket_detach;
  client.on_drain   = on_client_drain;
  r = oi_socket_connect(&client, servinfo);
  assert(r == 0 && "problem connecting");
  oi_socket_attach(&client, loop);

  oi_file_init(&file_src);
  file_src.on_open = on_file_src_open;
  file_src.on_drain = file_error;
  file_src.on_close = oi_file_detach;
  oi_file_open_path(&file_src, src_filename, O_RDONLY, 0700);
  oi_file_attach(&file_src, loop);

  ev_loop(loop, 0);

  printf("\noi_file: %d bytes\n", sizeof(oi_file));
  printf("oi_socket: %d bytes\n", sizeof(oi_socket));

  return 0;
}

