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


static void init_process_options(char* test) {
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


TEST_IMPL(spawn_exit_code) {
  int r;

  uv_init();

  init_process_options("spawn_helper1");

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

  init_process_options("spawn_helper2");
  options.stdout_stream = &out;

  r = uv_spawn(&process, options);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run();
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process once for the pipe. */
  ASSERT(strcmp("hello world\n", output) == 0);

  return 0;
}

