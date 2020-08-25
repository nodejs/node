/* Copyright libuv project contributors. All rights reserved.
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
#include <string.h>
#include <sys/stat.h>

int cookie1;
int cookie2;
int cookie3;


TEST_IMPL(handle_type_name) {
  ASSERT(strcmp(uv_handle_type_name(UV_NAMED_PIPE), "pipe") == 0);
  ASSERT(strcmp(uv_handle_type_name(UV_UDP), "udp") == 0);
  ASSERT(strcmp(uv_handle_type_name(UV_FILE), "file") == 0);
  ASSERT(uv_handle_type_name(UV_HANDLE_TYPE_MAX) == NULL);
  ASSERT(uv_handle_type_name(UV_HANDLE_TYPE_MAX + 1) == NULL);
  ASSERT(uv_handle_type_name(UV_UNKNOWN_HANDLE) == NULL);
  return 0;
}


TEST_IMPL(req_type_name) {
  ASSERT(strcmp(uv_req_type_name(UV_REQ), "req") == 0);
  ASSERT(strcmp(uv_req_type_name(UV_UDP_SEND), "udp_send") == 0);
  ASSERT(strcmp(uv_req_type_name(UV_WORK), "work") == 0);
  ASSERT(uv_req_type_name(UV_REQ_TYPE_MAX) == NULL);
  ASSERT(uv_req_type_name(UV_REQ_TYPE_MAX + 1) == NULL);
  ASSERT(uv_req_type_name(UV_UNKNOWN_REQ) == NULL);
  return 0;
}


TEST_IMPL(getters_setters) {
  uv_loop_t* loop;
  uv_pipe_t* pipe;
  uv_fs_t* fs;
  int r;

  loop = malloc(uv_loop_size());
  ASSERT(loop != NULL);
  r = uv_loop_init(loop);
  ASSERT(r == 0);

  uv_loop_set_data(loop, &cookie1);
  ASSERT(loop->data == &cookie1);
  ASSERT(uv_loop_get_data(loop) == &cookie1);

  pipe = malloc(uv_handle_size(UV_NAMED_PIPE));
  r = uv_pipe_init(loop, pipe, 0);
  ASSERT(uv_handle_get_type((uv_handle_t*)pipe) == UV_NAMED_PIPE);

  ASSERT(uv_handle_get_loop((uv_handle_t*)pipe) == loop);
  pipe->data = &cookie2;
  ASSERT(uv_handle_get_data((uv_handle_t*)pipe) == &cookie2);
  uv_handle_set_data((uv_handle_t*)pipe, &cookie1);
  ASSERT(uv_handle_get_data((uv_handle_t*)pipe) == &cookie1);
  ASSERT(pipe->data == &cookie1);

  ASSERT(uv_stream_get_write_queue_size((uv_stream_t*)pipe) == 0);
  pipe->write_queue_size++;
  ASSERT(uv_stream_get_write_queue_size((uv_stream_t*)pipe) == 1);
  pipe->write_queue_size--;
  uv_close((uv_handle_t*)pipe, NULL);

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  fs = malloc(uv_req_size(UV_FS));
  uv_fs_stat(loop, fs, ".", NULL);

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(uv_fs_get_type(fs) == UV_FS_STAT);
  ASSERT(uv_fs_get_result(fs) == 0);
  ASSERT(uv_fs_get_ptr(fs) == uv_fs_get_statbuf(fs));
  ASSERT(uv_fs_get_statbuf(fs)->st_mode & S_IFDIR);
  ASSERT(strcmp(uv_fs_get_path(fs), ".") == 0);
  uv_fs_req_cleanup(fs);

  r = uv_loop_close(loop);
  ASSERT(r == 0);

  free(pipe);
  free(fs);
  free(loop);
  return 0;
}
