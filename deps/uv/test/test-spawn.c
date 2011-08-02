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

static int close_cb_called;
static int exit_cb_called;
static uv_process_t process;
static uv_timer_t timer;
static uv_process_options_t options = { 0 };
static char exepath[1024];
static size_t exepath_size = 1024;
static char* args[3];

#define OUTPUT_SIZE 1024
static char output[OUTPUT_SIZE];
static int output_used;


static void close_cb(uv_handle_t* handle) {
  printf("close_cb\n");
  close_cb_called++;
}


static void exit_cb(uv_process_t* process, int exit_status, int term_signal) {
  printf("exit_cb\n");
  exit_cb_called++;
  ASSERT(exit_status == 1);
  ASSERT(term_signal == 0);
  uv_close((uv_handle_t*)process, close_cb);
}


static void kill_cb(uv_process_t* process, int exit_status, int term_signal) {
  printf("exit_cb\n");
  exit_cb_called++;
#ifdef _WIN32
  ASSERT(exit_status == 1);
#else
  ASSERT(exit_status == 0);
#endif
  ASSERT(term_signal == 15);
  uv_close((uv_handle_t*)process, close_cb);
}


uv_buf_t on_alloc(uv_stream_t* tcp, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = output + output_used;
  buf.len = OUTPUT_SIZE - output_used;
  return buf;
}


void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  uv_err_t err = uv_last_error();

  if (nread > 0) {
    output_used += nread;
  } else if (nread < 0) {
    if (err.code == UV_EOF) {
      uv_close((uv_handle_t*)tcp, close_cb);
    }
  }
}


void write_cb(uv_write_t* req, int status) {
  ASSERT(status == 0);
  uv_close((uv_handle_t*)req->handle, close_cb);
}


static void init_process_options(char* test, uv_exit_cb exit_cb) {
  /* Note spawn_helper1 defined in test/run-tests.c */
  int r = uv_exepath(exepath, &exepath_size);
  ASSERT(r == 0);
  exepath[exepath_size] = '\0';
  args[0] = exepath;
  args[1] = test;
  args[2] = NULL;
  options.file = exepath;
  options.args = args;
  options.exit_cb = exit_cb;
}


static void timer_cb(uv_timer_t* handle, int status) {
  uv_process_kill(&process, /* SIGTERM */ 15);
  uv_close((uv_handle_t*)handle, close_cb);
}


TEST_IMPL(spawn_exit_code) {
  int r;

  uv_init();

  init_process_options("spawn_helper1", exit_cb);

  r = uv_spawn(&process, options);
  ASSERT(r == 0);

  r = uv_run();
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 1);

  return 0;
}


TEST_IMPL(spawn_stdout) {
  int r;
  uv_pipe_t out;

  uv_init();

  init_process_options("spawn_helper2", exit_cb);

  uv_pipe_init(&out);
  options.stdout_stream = &out;

  r = uv_spawn(&process, options);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run();
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process once for the pipe. */
  printf("output is: %s", output);
  ASSERT(strcmp("hello world\n", output) == 0 || strcmp("hello world\r\n", output) == 0);

  return 0;
}


TEST_IMPL(spawn_stdin) {
int r;
  uv_pipe_t out;
  uv_pipe_t in;
  uv_write_t write_req;
  uv_buf_t buf;
  char buffer[] = "hello-from-spawn_stdin";

  uv_init();

  init_process_options("spawn_helper3", exit_cb);

  uv_pipe_init(&out);
  uv_pipe_init(&in);
  options.stdout_stream = &out;
  options.stdin_stream = &in;

  r = uv_spawn(&process, options);
  ASSERT(r == 0);

  buf.base = buffer;
  buf.len = sizeof(buffer);
  r = uv_write(&write_req, (uv_stream_t*)&in, &buf, 1, write_cb);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run();
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 3); /* Once for process twice for the pipe. */
  ASSERT(strcmp(buffer, output) == 0);

  return 0;
}


TEST_IMPL(spawn_and_kill) {
  int r;

  uv_init();

  init_process_options("spawn_helper4", kill_cb);

  r = uv_spawn(&process, options);
  ASSERT(r == 0);

  r = uv_timer_init(&timer);
  ASSERT(r == 0);

  r = uv_timer_start(&timer, timer_cb, 500, 0);
  ASSERT(r == 0);

  r = uv_run();
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process and once for timer. */

  return 0;
}


#ifdef _WIN32
TEST_IMPL(spawn_detect_pipe_name_collisions_on_windows) {
  int r;
  uv_pipe_t out;
  char name[64];
  HANDLE pipe_handle;

  uv_init();

  init_process_options("spawn_helper2", exit_cb);

  uv_pipe_init(&out);
  options.stdout_stream = &out;

  /* Create a pipe that'll cause a collision. */
  _snprintf(name, sizeof(name), "\\\\.\\pipe\\uv\\%p-%d", &out, GetCurrentProcessId());
  pipe_handle = CreateNamedPipeA(name,
                                PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                10,
                                65536,
                                65536,
                                0,
                                NULL);
  ASSERT(pipe_handle != INVALID_HANDLE_VALUE);

  r = uv_spawn(&process, options);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run();
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process once for the pipe. */
  printf("output is: %s", output);
  ASSERT(strcmp("hello world\n", output) == 0 || strcmp("hello world\r\n", output) == 0);

  return 0;
}
#endif