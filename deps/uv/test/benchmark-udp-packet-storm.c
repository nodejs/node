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

#include "task.h"
#include "uv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECTED "RANG TANG DING DONG I AM THE JAPANESE SANDMAN" /* "Take eight!" */

#define TEST_DURATION 5000 /* ms */

#define MAX_SENDERS   1000
#define MAX_RECEIVERS 1000

#define BASE_PORT 12345

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))

static uv_loop_t* loop;

static int n_senders_;
static int n_receivers_;
static uv_udp_t senders[MAX_SENDERS];
static uv_udp_t receivers[MAX_RECEIVERS];
static uv_buf_t bufs[5];

static int send_cb_called;
static int recv_cb_called;
static int close_cb_called;
static int stopping = 0;

typedef struct {
  struct sockaddr_in addr;
} sender_state_t;


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size) {
  static char slab[65536];
  ASSERT(suggested_size <= sizeof slab);
  return uv_buf_init(slab, sizeof slab);
}


static void send_cb(uv_udp_send_t* req, int status) {
  sender_state_t* ss;
  int r;

  if (stopping) {
    return;
  }

  ASSERT(req != NULL);
  ASSERT(status == 0);

  ss = req->data;

  r = uv_udp_send(req, req->handle, bufs, ARRAY_SIZE(bufs), ss->addr, send_cb);
  ASSERT(r == 0);

  req->data = ss;

  send_cb_called++;
}


static void recv_cb(uv_udp_t* handle,
                    ssize_t nread,
                    uv_buf_t buf,
                    struct sockaddr* addr,
                    unsigned flags) {
  if (nread == 0)
    return;

  if (nread == -1) {
    ASSERT(uv_last_error(loop).code == UV_EINTR); /* FIXME change error code */
    return;
  }

  ASSERT(addr->sa_family == AF_INET);
  ASSERT(!memcmp(buf.base, EXPECTED, nread));

  recv_cb_called++;
}


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);
  close_cb_called++;
}


static void timeout_cb(uv_timer_t* timer, int status) {
  int i;

  stopping = 1;

  for (i = 0; i < n_senders_; i++)
    uv_close((uv_handle_t*)&senders[i], close_cb);

  for (i = 0; i < n_receivers_; i++)
    uv_close((uv_handle_t*)&receivers[i], close_cb);
}


static int do_packet_storm(int n_senders, int n_receivers) {
  uv_timer_t timeout;
  sender_state_t *ss;
  uv_udp_send_t* req;
  uv_udp_t* handle;
  int i;
  int r;

  ASSERT(n_senders <= MAX_SENDERS);
  ASSERT(n_receivers <= MAX_RECEIVERS);

  loop = uv_default_loop();

  n_senders_ = n_senders;
  n_receivers_ = n_receivers;

  r = uv_timer_init(loop, &timeout);
  ASSERT(r == 0);

  r = uv_timer_start(&timeout, timeout_cb, TEST_DURATION, 0);
  ASSERT(r == 0);

  /* Timer should not keep loop alive. */
  uv_unref(loop);

  for (i = 0; i < n_receivers; i++) {
    struct sockaddr_in addr;
    handle = &receivers[i];

    r = uv_udp_init(loop, handle);
    ASSERT(r == 0);

    addr = uv_ip4_addr("0.0.0.0", BASE_PORT + i);

    r = uv_udp_bind(handle, addr, 0);
    ASSERT(r == 0);

    r = uv_udp_recv_start(handle, alloc_cb, recv_cb);
    ASSERT(r == 0);
  }

  bufs[0] = uv_buf_init(EXPECTED + 0,  10);
  bufs[1] = uv_buf_init(EXPECTED + 10, 10);
  bufs[2] = uv_buf_init(EXPECTED + 20, 10);
  bufs[3] = uv_buf_init(EXPECTED + 30, 10);
  bufs[4] = uv_buf_init(EXPECTED + 40, 5);

  for (i = 0; i < n_senders; i++) {
    handle = &senders[i];

    r = uv_udp_init(loop, handle);
    ASSERT(r == 0);

    req = malloc(sizeof(*req) + sizeof(*ss));

    ss = (void*)(req + 1);
    ss->addr = uv_ip4_addr("127.0.0.1", BASE_PORT + (i % n_receivers));

    r = uv_udp_send(req, handle, bufs, ARRAY_SIZE(bufs), ss->addr, send_cb);
    ASSERT(r == 0);

    req->data = ss;
  }

  uv_run(loop);

  printf("udp_packet_storm_%dv%d: %.0f/s received, %.0f/s sent\n",
         n_receivers,
         n_senders,
         recv_cb_called / (TEST_DURATION / 1000.0),
         send_cb_called / (TEST_DURATION / 1000.0));

  return 0;
}


BENCHMARK_IMPL(udp_packet_storm_1v1) {
  return do_packet_storm(1, 1);
}


BENCHMARK_IMPL(udp_packet_storm_1v10) {
  return do_packet_storm(1, 10);
}


BENCHMARK_IMPL(udp_packet_storm_1v100) {
  return do_packet_storm(1, 100);
}


BENCHMARK_IMPL(udp_packet_storm_1v1000) {
  return do_packet_storm(1, 1000);
}


BENCHMARK_IMPL(udp_packet_storm_10v10) {
  return do_packet_storm(10, 10);
}


BENCHMARK_IMPL(udp_packet_storm_10v100) {
  return do_packet_storm(10, 100);
}


BENCHMARK_IMPL(udp_packet_storm_10v1000) {
  return do_packet_storm(10, 1000);
}


BENCHMARK_IMPL(udp_packet_storm_100v100) {
  return do_packet_storm(100, 100);
}


BENCHMARK_IMPL(udp_packet_storm_100v1000) {
  return do_packet_storm(100, 1000);
}


BENCHMARK_IMPL(udp_packet_storm_1000v1000) {
  return do_packet_storm(1000, 1000);
}
