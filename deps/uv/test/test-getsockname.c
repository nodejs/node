/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int server_port = TEST_PORT;
/* Will be updated right after making the uv_connect_call */
static int connect_port = -1;

static int getsocknamecount = 0;
static int getpeernamecount = 0;

static uv_loop_t* loop;
static uv_tcp_t tcp;
static uv_udp_t udp;
static uv_connect_t connect_req;
static uv_tcp_t tcpServer;
static uv_udp_t udpServer;
static uv_udp_send_t send_req;


static uv_buf_t alloc(uv_handle_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = (char*) malloc(suggested_size);
  buf.len = suggested_size;
  return buf;
}


static void on_close(uv_handle_t* peer) {
  free(peer);
  uv_close((uv_handle_t*)&tcpServer, NULL);
}


static void after_shutdown(uv_shutdown_t* req, int status) {
  uv_close((uv_handle_t*) req->handle, on_close);
  free(req);
}


static void after_read(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
  uv_shutdown_t* req;
  int r;

  if (buf.base) {
    free(buf.base);
  }

  req = (uv_shutdown_t*) malloc(sizeof *req);
  r = uv_shutdown(req, handle, after_shutdown);
  ASSERT(r == 0);
}


static void check_sockname(struct sockaddr* addr, const char* compare_ip,
  int compare_port, const char* context) {
  struct sockaddr_in check_addr = *(struct sockaddr_in*) addr;
  struct sockaddr_in compare_addr = uv_ip4_addr(compare_ip, compare_port);
  char check_ip[17];
  int r;

  /* Both addresses should be ipv4 */
  ASSERT(check_addr.sin_family == AF_INET);
  ASSERT(compare_addr.sin_family == AF_INET);

  /* Check if the ip matches */
  ASSERT(memcmp(&check_addr.sin_addr,
         &compare_addr.sin_addr,
         sizeof compare_addr.sin_addr) == 0);

  /* Check if the port matches. If port == 0 anything goes. */
  ASSERT(compare_port == 0 || check_addr.sin_port == compare_addr.sin_port);

  r = uv_ip4_name(&check_addr, (char*) check_ip, sizeof check_ip);
  ASSERT(r == 0);

  printf("%s: %s:%d\n", context, check_ip, ntohs(check_addr.sin_port));
}


static void on_connection(uv_stream_t* server, int status) {
  struct sockaddr sockname, peername;
  int namelen;
  uv_handle_t* handle;
  int r;

  if (status != 0) {
    fprintf(stderr, "Connect error %d\n",
        uv_last_error(loop).code);
  }
  ASSERT(status == 0);

  handle = (uv_handle_t*) malloc(sizeof(uv_tcp_t));
  ASSERT(handle != NULL);

  r = uv_tcp_init(loop, (uv_tcp_t*)handle);
  ASSERT(r == 0);

  /* associate server with stream */
  handle->data = server;

  r = uv_accept(server, (uv_stream_t*)handle);
  ASSERT(r == 0);

  namelen = sizeof sockname;
  r = uv_tcp_getsockname((uv_tcp_t*) handle, &sockname, &namelen);
  ASSERT(r == 0);
  check_sockname(&sockname, "127.0.0.1", server_port, "accepted socket");
  getsocknamecount++;

  namelen = sizeof peername;
  r = uv_tcp_getpeername((uv_tcp_t*) handle, &peername, &namelen);
  ASSERT(r == 0);
  check_sockname(&peername, "127.0.0.1", connect_port, "accepted socket peer");
  getpeernamecount++;

  r = uv_read_start((uv_stream_t*)handle, alloc, after_read);
  ASSERT(r == 0);
}


static void on_connect(uv_connect_t* req, int status) {
  struct sockaddr sockname, peername;
  int r, namelen;

  ASSERT(status == 0);

  namelen = sizeof sockname;
  r = uv_tcp_getsockname((uv_tcp_t*) req->handle, &sockname, &namelen);
  ASSERT(r == 0);
  check_sockname(&sockname, "127.0.0.1", 0, "connected socket");
  getsocknamecount++;

  namelen = sizeof peername;
  r = uv_tcp_getpeername((uv_tcp_t*) req->handle, &peername, &namelen);
  ASSERT(r == 0);
  check_sockname(&peername, "127.0.0.1", server_port, "connected socket peer");
  getpeernamecount++;

  uv_close((uv_handle_t*)&tcp, NULL);
}


static int tcp_listener() {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", server_port);
  struct sockaddr sockname, peername;
  int namelen;
  int r;

  r = uv_tcp_init(loop, &tcpServer);
  if (r) {
    fprintf(stderr, "Socket creation error\n");
    return 1;
  }

  r = uv_tcp_bind(&tcpServer, addr);
  if (r) {
    fprintf(stderr, "Bind error\n");
    return 1;
  }

  r = uv_listen((uv_stream_t*)&tcpServer, 128, on_connection);
  if (r) {
    fprintf(stderr, "Listen error\n");
    return 1;
  }

  memset(&sockname, -1, sizeof sockname);
  namelen = sizeof sockname;
  r = uv_tcp_getsockname(&tcpServer, &sockname, &namelen);
  ASSERT(r == 0);
  check_sockname(&sockname, "0.0.0.0", server_port, "server socket");
  getsocknamecount++;

  namelen = sizeof sockname;
  r = uv_tcp_getpeername(&tcpServer, &peername, &namelen);
  ASSERT(r == -1);
  ASSERT(uv_last_error(loop).code == UV_ENOTCONN);
  getpeernamecount++;

  return 0;
}


static void tcp_connector() {
  struct sockaddr_in server_addr = uv_ip4_addr("127.0.0.1", server_port);
  struct sockaddr sockname;
  int r, namelen;

  r = uv_tcp_init(loop, &tcp);
  tcp.data = &connect_req;
  ASSERT(!r);

  r = uv_tcp_connect(&connect_req, &tcp, server_addr, on_connect);
  ASSERT(!r);

  /* Fetch the actual port used by the connecting socket. */
  namelen = sizeof sockname;
  r = uv_tcp_getsockname(&tcp, &sockname, &namelen);
  ASSERT(!r);
  ASSERT(sockname.sa_family == AF_INET);
  connect_port = ntohs(((struct sockaddr_in*) &sockname)->sin_port);
  ASSERT(connect_port > 0);
}


static void udp_recv(uv_udp_t* handle,
                     ssize_t nread,
                     uv_buf_t buf,
                     struct sockaddr* addr,
                     unsigned flags) {
  struct sockaddr sockname;
  int namelen;
  int r;

  ASSERT(nread >= 0);

  if (nread == 0) {
    free(buf.base);
    return;
  }

  memset(&sockname, -1, sizeof sockname);
  namelen = sizeof(sockname);
  r = uv_udp_getsockname(&udp, &sockname, &namelen);
  ASSERT(r == 0);
  check_sockname(&sockname, "0.0.0.0", 0, "udp receiving socket");
  getsocknamecount++;

  uv_close((uv_handle_t*) &udp, NULL);
  uv_close((uv_handle_t*) handle, NULL);
}


static void udp_send(uv_udp_send_t* req, int status) {

}


static int udp_listener() {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", server_port);
  struct sockaddr sockname;
  int namelen;
  int r;

  r = uv_udp_init(loop, &udpServer);
  if (r) {
    fprintf(stderr, "Socket creation error\n");
    return 1;
  }

  r = uv_udp_bind(&udpServer, addr, 0);
  if (r) {
    fprintf(stderr, "Bind error\n");
    return 1;
  }

  memset(&sockname, -1, sizeof sockname);
  namelen = sizeof sockname;
  r = uv_udp_getsockname(&udpServer, &sockname, &namelen);
  ASSERT(r == 0);
  check_sockname(&sockname, "0.0.0.0", server_port, "udp listener socket");
  getsocknamecount++;

  r = uv_udp_recv_start(&udpServer, alloc, udp_recv);
  ASSERT(r == 0);

  return 0;
}


static void udp_sender(void) {
  struct sockaddr_in server_addr;
  uv_buf_t buf;
  int r;

  r = uv_udp_init(loop, &udp);
  ASSERT(!r);

  buf = uv_buf_init("PING", 4);
  server_addr = uv_ip4_addr("127.0.0.1", server_port);

  r = uv_udp_send(&send_req, &udp, &buf, 1, server_addr, udp_send);
  ASSERT(!r);
}


TEST_IMPL(getsockname_tcp) {
  loop = uv_default_loop();

  if (tcp_listener())
    return 1;

  tcp_connector();

  uv_run(loop);

  ASSERT(getsocknamecount == 3);
  ASSERT(getpeernamecount == 3);

  return 0;
}


TEST_IMPL(getsockname_udp) {
  loop = uv_default_loop();

  if (udp_listener())
    return 1;

  udp_sender();

  uv_run(loop);

  ASSERT(getsocknamecount == 2);

  return 0;
}
