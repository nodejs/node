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


static char exepath[1024];
static size_t exepath_size = 1024;
static char* args[3];
static uv_process_options_t options;
static int close_cb_called;
static int exit_cb_called;
static int on_read_cb_called;
static int after_write_cb_called;
uv_pipe_t out, in;
static uv_loop_t* loop;
#define OUTPUT_SIZE 1024
static char output[OUTPUT_SIZE];
static int output_used;

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;


static void close_cb(uv_handle_t* handle) {
  printf("close_cb\n");
  close_cb_called++;
}


static void exit_cb(uv_process_t* process, int exit_status, int term_signal) {
  printf("exit_cb\n");
  exit_cb_called++;
  ASSERT(exit_status == 0);
  ASSERT(term_signal == 0);
  uv_close((uv_handle_t*)process, close_cb);
  uv_close((uv_handle_t*)&in, close_cb);
  uv_close((uv_handle_t*)&out, close_cb);
}


static void init_process_options(char* test, uv_exit_cb exit_cb) {
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


static uv_buf_t on_alloc(uv_handle_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = output + output_used;
  buf.len = OUTPUT_SIZE - output_used;
  return buf;
}


static void after_write(uv_write_t* req, int status) {
  write_req_t* wr;

  if (status) {
    uv_err_t err = uv_last_error(loop);
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(err));
    ASSERT(0);
  }

  wr = (write_req_t*) req;

  /* Free the read/write buffer and the request */
  free(wr);

  after_write_cb_called++;
}


static void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  write_req_t* write_req;
  int r;
  uv_err_t err = uv_last_error(uv_default_loop());

  ASSERT(nread > 0 || err.code == UV_EOF);

  if (nread > 0) {
    output_used += nread;
    if (output_used == 12) {
      ASSERT(memcmp("hello world\n", output, 12) == 0);
      write_req = (write_req_t*)malloc(sizeof(*write_req));
      write_req->buf = uv_buf_init(output, output_used);
      r = uv_write(&write_req->req, (uv_stream_t*)&in, &write_req->buf, 1, after_write);
      ASSERT(r == 0);
    }
  }

  on_read_cb_called++;
}


TEST_IMPL(stdio_over_pipes) {
  int r;
  uv_process_t process;
  loop = uv_default_loop();

  init_process_options("stdio_over_pipes_helper", exit_cb);

  uv_pipe_init(loop, &out, 0);
  options.stdout_stream = &out;
  uv_pipe_init(loop, &in, 0);
  options.stdin_stream = &in;

  r = uv_spawn(loop, &process, options);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  ASSERT(on_read_cb_called > 1);
  ASSERT(after_write_cb_called == 1);
  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 3);
  ASSERT(memcmp("hello world\n", output, 12) == 0);
  ASSERT(output_used == 12);

  return 0;
}

