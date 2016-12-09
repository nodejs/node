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

/* this test is Unix only */
#ifndef _WIN32

#include "uv.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

static struct sockaddr_in addr;
static uv_tcp_t tcp_server;
static uv_tcp_t tcp_outgoing[2];
static uv_tcp_t tcp_incoming[ARRAY_SIZE(tcp_outgoing)];
static uv_connect_t connect_reqs[ARRAY_SIZE(tcp_outgoing)];
static uv_tcp_t tcp_check;
static uv_connect_t tcp_check_req;
static uv_write_t write_reqs[ARRAY_SIZE(tcp_outgoing)];
static unsigned int got_connections;
static unsigned int close_cb_called;
static unsigned int write_cb_called;
static unsigned int read_cb_called;
static unsigned int pending_incoming;

static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}

static void write_cb(uv_write_t* req, int status) {
  ASSERT(status == 0);
  write_cb_called++;
}

static void connect_cb(uv_connect_t* req, int status) {
  unsigned int i;
  uv_buf_t buf;
  uv_stream_t* outgoing;

  if (req == &tcp_check_req) {
    ASSERT(status != 0);

    /*
     * Time to finish the test: close both the check and pending incoming
     * connections
     */
    uv_close((uv_handle_t*) &tcp_incoming[pending_incoming], close_cb);
    uv_close((uv_handle_t*) &tcp_check, close_cb);
    return;
  }

  ASSERT(status == 0);
  ASSERT(connect_reqs <= req);
  ASSERT(req <= connect_reqs + ARRAY_SIZE(connect_reqs));
  i = req - connect_reqs;

  buf = uv_buf_init("x", 1);
  outgoing = (uv_stream_t*) &tcp_outgoing[i];
  ASSERT(0 == uv_write(&write_reqs[i], outgoing, &buf, 1, write_cb));
}

static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
  static char slab[1];
  buf->base = slab;
  buf->len = sizeof(slab);
}

static void read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  uv_loop_t* loop;
  unsigned int i;

  pending_incoming = (uv_tcp_t*) stream - &tcp_incoming[0];
  ASSERT(pending_incoming < got_connections);
  ASSERT(0 == uv_read_stop(stream));
  ASSERT(1 == nread);

  loop = stream->loop;
  read_cb_called++;

  /* Close all active incomings, except current one */
  for (i = 0; i < got_connections; i++) {
    if (i != pending_incoming)
      uv_close((uv_handle_t*) &tcp_incoming[i], close_cb);
  }

  /* Close server, so no one will connect to it */
  uv_close((uv_handle_t*) &tcp_server, close_cb);

  /* Create new fd that should be one of the closed incomings */
  ASSERT(0 == uv_tcp_init(loop, &tcp_check));
  ASSERT(0 == uv_tcp_connect(&tcp_check_req,
                             &tcp_check,
                             (const struct sockaddr*) &addr,
                             connect_cb));
  ASSERT(0 == uv_read_start((uv_stream_t*) &tcp_check, alloc_cb, read_cb));
}

static void connection_cb(uv_stream_t* server, int status) {
  unsigned int i;
  uv_tcp_t* incoming;

  ASSERT(server == (uv_stream_t*) &tcp_server);

  /* Ignore tcp_check connection */
  if (got_connections == ARRAY_SIZE(tcp_incoming))
    return;

  /* Accept everyone */
  incoming = &tcp_incoming[got_connections++];
  ASSERT(0 == uv_tcp_init(server->loop, incoming));
  ASSERT(0 == uv_accept(server, (uv_stream_t*) incoming));

  if (got_connections != ARRAY_SIZE(tcp_incoming))
    return;

  /* Once all clients are accepted - start reading */
  for (i = 0; i < ARRAY_SIZE(tcp_incoming); i++) {
    incoming = &tcp_incoming[i];
    ASSERT(0 == uv_read_start((uv_stream_t*) incoming, alloc_cb, read_cb));
  }
}

TEST_IMPL(tcp_close_accept) {
  unsigned int i;
  uv_loop_t* loop;
  uv_tcp_t* client;

  /*
   * A little explanation of what goes on below:
   *
   * We'll create server and connect to it using two clients, each writing one
   * byte once connected.
   *
   * When all clients will be accepted by server - we'll start reading from them
   * and, on first client's first byte, will close second client and server.
   * After that, we'll immediately initiate new connection to server using
   * tcp_check handle (thus, reusing fd from second client).
   *
   * In this situation uv__io_poll()'s event list should still contain read
   * event for second client, and, if not cleaned up properly, `tcp_check` will
   * receive stale event of second incoming and invoke `connect_cb` with zero
   * status.
   */

  loop = uv_default_loop();
  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));

  ASSERT(0 == uv_tcp_init(loop, &tcp_server));
  ASSERT(0 == uv_tcp_bind(&tcp_server, (const struct sockaddr*) &addr, 0));
  ASSERT(0 == uv_listen((uv_stream_t*) &tcp_server,
                        ARRAY_SIZE(tcp_outgoing),
                        connection_cb));

  for (i = 0; i < ARRAY_SIZE(tcp_outgoing); i++) {
    client = tcp_outgoing + i;

    ASSERT(0 == uv_tcp_init(loop, client));
    ASSERT(0 == uv_tcp_connect(&connect_reqs[i],
                               client,
                               (const struct sockaddr*) &addr,
                               connect_cb));
  }

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(ARRAY_SIZE(tcp_outgoing) == got_connections);
  ASSERT((ARRAY_SIZE(tcp_outgoing) + 2) == close_cb_called);
  ASSERT(ARRAY_SIZE(tcp_outgoing) == write_cb_called);
  ASSERT(1 == read_cb_called);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#endif  /* !_WIN32 */
