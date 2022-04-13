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


#ifdef _WIN32
# define BAD_PIPENAME "bad-pipe"
#else
# define BAD_PIPENAME "/path/to/unix/socket/that/really/should/not/be/there"
#endif


static int close_cb_called = 0;


static void close_cb(uv_handle_t* handle) {
  ASSERT_NOT_NULL(handle);
  close_cb_called++;
}


TEST_IMPL(pipe_bind_error_addrinuse) {
  uv_pipe_t server1, server2;
  int r;

  r = uv_pipe_init(uv_default_loop(), &server1, 0);
  ASSERT(r == 0);
  r = uv_pipe_bind(&server1, TEST_PIPENAME);
  ASSERT(r == 0);

  r = uv_pipe_init(uv_default_loop(), &server2, 0);
  ASSERT(r == 0);
  r = uv_pipe_bind(&server2, TEST_PIPENAME);
  ASSERT(r == UV_EADDRINUSE);

  r = uv_listen((uv_stream_t*)&server1, SOMAXCONN, NULL);
  ASSERT(r == 0);
  r = uv_listen((uv_stream_t*)&server2, SOMAXCONN, NULL);
  ASSERT(r == UV_EINVAL);

  uv_close((uv_handle_t*)&server1, close_cb);
  uv_close((uv_handle_t*)&server2, close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 2);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(pipe_bind_error_addrnotavail) {
  uv_pipe_t server;
  int r;

  r = uv_pipe_init(uv_default_loop(), &server, 0);
  ASSERT(r == 0);

  r = uv_pipe_bind(&server, BAD_PIPENAME);
  ASSERT(r == UV_EACCES);

  uv_close((uv_handle_t*)&server, close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(pipe_bind_error_inval) {
  uv_pipe_t server;
  int r;

  r = uv_pipe_init(uv_default_loop(), &server, 0);
  ASSERT(r == 0);
  r = uv_pipe_bind(&server, TEST_PIPENAME);
  ASSERT(r == 0);
  r = uv_pipe_bind(&server, TEST_PIPENAME_2);
  ASSERT(r == UV_EINVAL);

  uv_close((uv_handle_t*)&server, close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(pipe_listen_without_bind) {
#if defined(NO_SELF_CONNECT)
  RETURN_SKIP(NO_SELF_CONNECT);
#endif
  uv_pipe_t server;
  int r;

  r = uv_pipe_init(uv_default_loop(), &server, 0);
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)&server, SOMAXCONN, NULL);
  ASSERT(r == UV_EINVAL);

  uv_close((uv_handle_t*)&server, close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
