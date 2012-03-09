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

#include <stdlib.h>
#include <string.h>


static uv_write_t write_req;
static uv_shutdown_t shutdown_req;
static uv_connect_t connect_req;

static char buffer[32767];


static void fail_cb(void) {
  FATAL("fail_cb should not have been called");
}


static void write_unref_cb(uv_connect_t* req, int status) {
  uv_buf_t buf = uv_buf_init(buffer, sizeof buffer);

  ASSERT(req == &connect_req);
  ASSERT(status == 0);

  uv_write(&write_req, req->handle, &buf, 1, (uv_write_cb) fail_cb);
  uv_unref(uv_default_loop()); /* uv_write refs the loop */
}



static void shutdown_unref_cb(uv_connect_t* req, int status) {
  ASSERT(req == &connect_req);
  ASSERT(status == 0);

  uv_shutdown(&shutdown_req, req->handle, (uv_shutdown_cb) fail_cb);
  uv_unref(uv_default_loop()); /* uv_shutdown refs the loop */
}


TEST_IMPL(ref) {
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(idle_ref) {
  uv_idle_t h;
  uv_idle_init(uv_default_loop(), &h);
  uv_idle_start(&h, NULL);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(async_ref) {
  uv_async_t h;
  uv_async_init(uv_default_loop(), &h, NULL);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(prepare_ref) {
  uv_prepare_t h;
  uv_prepare_init(uv_default_loop(), &h);
  uv_prepare_start(&h, NULL);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(check_ref) {
  uv_check_t h;
  uv_check_init(uv_default_loop(), &h);
  uv_check_start(&h, NULL);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


static void prepare_cb(uv_prepare_t* handle, int status) {
  ASSERT(handle != NULL);
  ASSERT(status == 0);

  uv_unref(uv_default_loop());
}


TEST_IMPL(unref_in_prepare_cb) {
  uv_prepare_t h;
  uv_prepare_init(uv_default_loop(), &h);
  uv_prepare_start(&h, prepare_cb);
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(timer_ref) {
  uv_timer_t h;
  uv_timer_init(uv_default_loop(), &h);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(timer_ref2) {
  uv_timer_t h;
  uv_timer_init(uv_default_loop(), &h);
  uv_timer_start(&h, (uv_timer_cb) fail_cb, 42, 42);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(fs_event_ref) {
  uv_fs_event_t h;
  uv_fs_event_init(uv_default_loop(), &h, ".", (uv_fs_event_cb) fail_cb, 0);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(tcp_ref) {
  uv_tcp_t h;
  uv_tcp_init(uv_default_loop(), &h);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(tcp_ref2) {
  uv_tcp_t h;
  uv_tcp_init(uv_default_loop(), &h);
  uv_listen((uv_stream_t*)&h, 128, (uv_connection_cb)fail_cb);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(tcp_ref3) {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_tcp_t h;
  uv_tcp_init(uv_default_loop(), &h);
  uv_tcp_connect(&connect_req, &h, addr, (uv_connect_cb)fail_cb);
  uv_unref(uv_default_loop());
  uv_unref(uv_default_loop()); /* connect req refs the loop */
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(tcp_ref4) {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_tcp_t h;
  uv_tcp_init(uv_default_loop(), &h);
  uv_tcp_connect(&connect_req, &h, addr, write_unref_cb);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(tcp_ref5) {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_tcp_t h;
  uv_tcp_init(uv_default_loop(), &h);
  uv_tcp_connect(&connect_req, &h, addr, shutdown_unref_cb);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(udp_ref) {
  uv_udp_t h;
  uv_udp_init(uv_default_loop(), &h);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(udp_ref2) {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_udp_t h;
  uv_udp_init(uv_default_loop(), &h);
  uv_udp_bind(&h, addr, 0);
  uv_udp_recv_start(&h, (uv_alloc_cb)fail_cb, (uv_udp_recv_cb)fail_cb);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(udp_ref3) {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_buf_t buf = uv_buf_init("PING", 4);
  uv_udp_send_t req;
  uv_udp_t h;

  uv_udp_init(uv_default_loop(), &h);
  uv_udp_send(&req, &h, &buf, 1, addr, (uv_udp_send_cb)fail_cb);
  uv_unref(uv_default_loop());
  uv_unref(uv_default_loop()); /* send req refs the loop */
  uv_run(uv_default_loop());

  return 0;
}


TEST_IMPL(pipe_ref) {
  uv_pipe_t h;
  uv_pipe_init(uv_default_loop(), &h, 0);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(pipe_ref2) {
  uv_pipe_t h;
  uv_pipe_init(uv_default_loop(), &h, 0);
  uv_listen((uv_stream_t*)&h, 128, (uv_connection_cb)fail_cb);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(pipe_ref3) {
  uv_pipe_t h;
  uv_pipe_init(uv_default_loop(), &h, 0);
  uv_pipe_connect(&connect_req, &h, TEST_PIPENAME, (uv_connect_cb)fail_cb);
  uv_unref(uv_default_loop());
  uv_unref(uv_default_loop()); /* connect req refs the loop */
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(pipe_ref4) {
  uv_pipe_t h;
  uv_pipe_init(uv_default_loop(), &h, 0);
  uv_pipe_connect(&connect_req, &h, TEST_PIPENAME, write_unref_cb);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(pipe_ref5) {
  uv_pipe_t h;
  uv_pipe_init(uv_default_loop(), &h, 0);
  uv_pipe_connect(&connect_req, &h, TEST_PIPENAME, shutdown_unref_cb);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(process_ref) {
  /* spawn_helper4 blocks indefinitely. */
  char *argv[] = { NULL, "spawn_helper4", NULL };
  uv_process_options_t options;
  size_t exepath_size;
  char exepath[256];
  uv_process_t h;
  int r;

  memset(&options, 0, sizeof(options));
  exepath_size = sizeof(exepath);

  r = uv_exepath(exepath, &exepath_size);
  ASSERT(r == 0);

  argv[0] = exepath;
  options.file = exepath;
  options.args = argv;
  options.exit_cb = NULL;

  r = uv_spawn(uv_default_loop(), &h, options);
  ASSERT(r == 0);

  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());

  r = uv_process_kill(&h, /* SIGTERM */ 15);
  ASSERT(r == 0);

  return 0;
}
