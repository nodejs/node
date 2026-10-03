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


#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif


static int write_cb_called;
static int close_cb_called;


static void close_cb(uv_handle_t* handle) {
  ASSERT_NOT_NULL(handle);
  close_cb_called++;
}


static void write_cb(uv_write_t* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT_OK(status);
  write_cb_called++;
  uv_close((uv_handle_t*) req->handle, close_cb);
}


TEST_IMPL(pipe_write_trailing_empty_buf) {
#ifdef _WIN32
  RETURN_SKIP("Unix only test");
#else
  uv_pipe_t pipe_handle;
  uv_write_t write_req;
  uv_buf_t bufs[2];
  int fd;

  fd = open("/dev/null", O_WRONLY);
  ASSERT_GE(fd, 0);

  ASSERT_OK(uv_pipe_init(uv_default_loop(), &pipe_handle, 0));
  ASSERT_OK(uv_pipe_open(&pipe_handle, fd));
  fd = -1; /* fd is owned by pipe_handle now. */

  bufs[0] = uv_buf_init("hello\n", 6);
  bufs[1] = uv_buf_init(NULL, 0);
  ASSERT_OK(uv_write(&write_req,
                     (uv_stream_t*) &pipe_handle,
                     bufs,
                     ARRAY_SIZE(bufs),
                     write_cb));

  ASSERT_OK(uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT_EQ(1, write_cb_called);
  ASSERT_EQ(1, close_cb_called);

  MAKE_VALGRIND_HAPPY(uv_default_loop());
  return 0;
#endif
}
