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

#if defined(__linux__)
  #include <sys/socket.h>
  #include <sys/un.h>
#endif

#ifndef _WIN32
# include <unistd.h>  /* close */
#else
# include <fcntl.h>
#endif


static int close_cb_called = 0;


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);
  close_cb_called++;
}


TEST_IMPL(pipe_getsockname) {
  uv_pipe_t server;
  char buf[1024];
  size_t len;
  int r;

  r = uv_pipe_init(uv_default_loop(), &server, 0);
  ASSERT(r == 0);
  r = uv_pipe_bind(&server, TEST_PIPENAME);
  ASSERT(r == 0);

  len = sizeof buf;
  r = uv_pipe_getsockname(&server, buf, &len);
  ASSERT(r == 0);

  ASSERT(memcmp(buf, TEST_PIPENAME, len) == 0);

  uv_close((uv_handle_t*)&server, close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(pipe_getsockname_abstract) {
#if defined(__linux__)
  uv_pipe_t server;
  char buf[1024];
  size_t len;
  int r;
  int sock;
  struct sockaddr_un sun;
  socklen_t sun_len;
  char abstract_pipe[] = "\0test-pipe";

  sock = socket(AF_LOCAL, SOCK_STREAM, 0);
  ASSERT(sock != -1);

  sun_len = sizeof sun;
  memset(&sun, 0, sun_len);
  sun.sun_family = AF_UNIX;
  memcpy(sun.sun_path, abstract_pipe, sizeof abstract_pipe);

  r = bind(sock, (struct sockaddr*)&sun, sun_len);
  ASSERT(r == 0);

  r = uv_pipe_init(uv_default_loop(), &server, 0);
  ASSERT(r == 0);
  r = uv_pipe_open(&server, sock);
  ASSERT(r == 0);

  len = sizeof buf;
  r = uv_pipe_getsockname(&server, buf, &len);
  ASSERT(r == 0);

  ASSERT(memcmp(buf, abstract_pipe, sizeof abstract_pipe) == 0);

  uv_close((uv_handle_t*)&server, close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  close(sock);

  ASSERT(close_cb_called == 1);
  MAKE_VALGRIND_HAPPY();
  return 0;
#else
  MAKE_VALGRIND_HAPPY();
  return 0;
#endif
}

TEST_IMPL(pipe_getsockname_blocking) {
#ifdef _WIN32
  uv_pipe_t reader;
  HANDLE readh, writeh;
  int readfd;
  char buf1[1024], buf2[1024];
  size_t len1, len2;
  int r;

  r = CreatePipe(&readh, &writeh, NULL, 65536);
  ASSERT(r != 0);

  r = uv_pipe_init(uv_default_loop(), &reader, 0);
  ASSERT(r == 0);
  readfd = _open_osfhandle((intptr_t)readh, _O_RDONLY);
  ASSERT(r != -1);
  r = uv_pipe_open(&reader, readfd);
  ASSERT(r == 0);
  r = uv_read_start((uv_stream_t*)&reader, NULL, NULL);
  ASSERT(r == 0);
  Sleep(100);
  r = uv_read_stop((uv_stream_t*)&reader);
  ASSERT(r == 0);

  len1 = sizeof buf1;
  r = uv_pipe_getsockname(&reader, buf1, &len1);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*)&reader, NULL, NULL);
  ASSERT(r == 0);
  Sleep(100);

  len2 = sizeof buf2;
  r = uv_pipe_getsockname(&reader, buf2, &len2);
  ASSERT(r == 0);

  r = uv_read_stop((uv_stream_t*)&reader);
  ASSERT(r == 0);

  ASSERT(len1 == len2);
  ASSERT(memcmp(buf1, buf2, len1) == 0);

  close_cb_called = 0;
  uv_close((uv_handle_t*)&reader, close_cb);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  _close(readfd);
  CloseHandle(writeh);
#endif

  MAKE_VALGRIND_HAPPY();
  return 0;
}
