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


static int getsocknamecount = 0;

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


static void on_connection(uv_stream_t* server, int status) {
  struct sockaddr sockname;
  int namelen = sizeof(sockname);
  uv_handle_t* handle;
  int r;

  if (status != 0) {
    fprintf(stderr, "Connect error %d\n", uv_last_error().code);
  }
  ASSERT(status == 0);

  handle = (uv_handle_t*) malloc(sizeof(uv_tcp_t));
  ASSERT(handle != NULL);

  uv_tcp_init((uv_tcp_t*)handle);

  /* associate server with stream */
  handle->data = server;

  r = uv_accept(server, (uv_stream_t*)handle);
  ASSERT(r == 0);

  status = uv_getsockname(handle, &sockname, &namelen);
  if (status != 0) {
    fprintf(stderr, "uv_getsockname error (accepted) %d\n", uv_last_error().code);
  }
  ASSERT(status == 0);

  getsocknamecount++;

  r = uv_read_start((uv_stream_t*)handle, alloc, after_read);
  ASSERT(r == 0);
}


static void on_connect(uv_connect_t* req, int status) {
  struct sockaddr sockname;
  int namelen = sizeof(sockname);
  int r;

  ASSERT(status == 0);

  r = uv_getsockname((uv_handle_t*)&tcp, &sockname, &namelen);
  if (r != 0) {
    fprintf(stderr, "uv_getsockname error (connector) %d\n", uv_last_error().code);
  }
  ASSERT(r == 0);

  getsocknamecount++;

  uv_close((uv_handle_t*)&tcp, NULL);
}


static int tcp_listener(int port) {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", port);
  struct sockaddr sockname;
  int namelen = sizeof(sockname);
  char ip[20];
  int r;

  r = uv_tcp_init(&tcpServer);
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

  r = uv_getsockname((uv_handle_t*)&tcpServer, &sockname, &namelen);
  if (r != 0) {
    fprintf(stderr, "uv_getsockname error (listening) %d\n", uv_last_error().code);
  }
  ASSERT(r == 0);

  r = uv_ip4_name((struct sockaddr_in*)&sockname, ip, 20);
  ASSERT(r == 0);
  ASSERT(ip[0] == '0');
  ASSERT(ip[1] == '.');
  ASSERT(ip[2] == '0');
  printf("sockname = %s\n", ip);

  getsocknamecount++;

  return 0;
}


static void tcp_connector() {
  int r;
  struct sockaddr_in server_addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

  r = uv_tcp_init(&tcp);
  tcp.data = &connect_req;
  ASSERT(!r);

  r = uv_tcp_connect(&connect_req, &tcp, server_addr, on_connect);
  ASSERT(!r);
}


static void udp_recv(uv_udp_t* handle,
                     ssize_t nread,
                     uv_buf_t buf,
                     struct sockaddr* addr,
                     unsigned flags) {
  struct sockaddr sockname;
  char ip[20];
  int namelen;
  int r;

  ASSERT(nread >= 0);

  if (nread == 0) {
    uv_close((uv_handle_t*)handle, NULL);
    free(buf.base);
    return;
  }

  namelen = sizeof(sockname);
  r = uv_getsockname((uv_handle_t*)&udp, &sockname, &namelen);
  if (r != 0) {
    fprintf(stderr, "uv_getsockname error (connector) %d\n", uv_last_error().code);
  }
  ASSERT(r == 0);

  r = uv_ip4_name((struct sockaddr_in*)&sockname, ip, 20);
  ASSERT(r == 0);
  printf("sockname = %s\n", ip);

  getsocknamecount++;

  uv_close((uv_handle_t*)&udp, NULL);
}


static void udp_send(uv_udp_send_t* req, int status) {

}


static int udp_listener(int port) {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", port);
  struct sockaddr sockname;
  int namelen = sizeof(sockname);
  char ip[20];
  int r;

  r = uv_udp_init(&udpServer);
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

  r = uv_getsockname((uv_handle_t*)&udpServer, &sockname, &namelen);
  if (r != 0) {
    fprintf(stderr, "uv_getsockname error (listening) %d\n", uv_last_error().code);
  }
  ASSERT(r == 0);

  r = uv_ip4_name((struct sockaddr_in*)&sockname, ip, 20);
  ASSERT(r == 0);
  ASSERT(ip[0] == '0');
  ASSERT(ip[1] == '.');
  ASSERT(ip[2] == '0');
  printf("sockname = %s\n", ip);

  getsocknamecount++;

  r = uv_udp_recv_start(&udpServer, alloc, udp_recv);
  ASSERT(r == 0);

  return 0;
}


static void udp_sender(void) {
  struct sockaddr_in server_addr;
  uv_buf_t buf;
  int r;

  r = uv_udp_init(&udp);
  ASSERT(!r);

  buf = uv_buf_init("PING", 4);
  server_addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

  r = uv_udp_send(&send_req, &udp, &buf, 1, server_addr, udp_send);
  ASSERT(!r);
}


TEST_IMPL(getsockname_tcp) {
  uv_init();

  if (tcp_listener(TEST_PORT))
    return 1;

  tcp_connector();

  uv_run();

  ASSERT(getsocknamecount == 3);

  return 0;
}


TEST_IMPL(getsockname_udp) {
  uv_init();

  if (udp_listener(TEST_PORT))
    return 1;

  udp_sender();

  uv_run();

  ASSERT(getsocknamecount == 2);

  return 0;
}
