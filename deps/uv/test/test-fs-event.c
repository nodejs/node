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

#include <string.h>
#include <fcntl.h>

uv_fs_event_t fs_event;
uv_timer_t timer;
int timer_cb_called;
int close_cb_called;
int fs_event_cb_called;

static void create_dir(uv_loop_t* loop, const char* name) {
  int r;
  uv_fs_t req;
  r = uv_fs_mkdir(loop, &req, name, 0755, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);
}

static void create_file(uv_loop_t* loop, const char* name) {
  int r;
  uv_file file;
  uv_fs_t req;

  r = uv_fs_open(loop, &req, name, O_WRONLY | O_CREAT,
      S_IWRITE | S_IREAD, NULL);
  ASSERT(r != -1);
  file = r;
  uv_fs_req_cleanup(&req);
  r = uv_fs_close(loop, &req, file, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);
}

static void touch_file(uv_loop_t* loop, const char* name) {
  int r;
  uv_file file;
  uv_fs_t req;

  r = uv_fs_open(loop, &req, name, O_RDWR, 0, NULL);
  ASSERT(r != -1);
  file = r;
  uv_fs_req_cleanup(&req);

  r = uv_fs_write(loop, &req, file, "foo", 4, -1, NULL);
  ASSERT(r != -1);
  uv_fs_req_cleanup(&req);

  r = uv_fs_close(loop, &req, file, NULL);
  ASSERT(r != -1);
  uv_fs_req_cleanup(&req);
}

static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);
  close_cb_called++;
}

static void fs_event_cb_dir(uv_fs_event_t* handle, const char* filename,
  int events, int status) {
  ++fs_event_cb_called;
  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_RENAME);
  ASSERT(strcmp(filename, "file1") == 0);
  uv_close((uv_handle_t*)handle, close_cb);
}

static void fs_event_cb_file(uv_fs_event_t* handle, const char* filename,
  int events, int status) {
  ++fs_event_cb_called;
  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_CHANGE);
  ASSERT(strcmp(filename, "file2") == 0);
  uv_close((uv_handle_t*)handle, close_cb);
}

static void fs_event_cb_file_current_dir(uv_fs_event_t* handle,
  const char* filename, int events, int status) {
  ++fs_event_cb_called;
  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_CHANGE);
  ASSERT(strcmp(filename, "watch_file") == 0);
  uv_close((uv_handle_t*)handle, close_cb);
}

static void timer_cb_dir(uv_timer_t* handle, int status) {
  ++timer_cb_called;
  create_file(handle->loop, "watch_dir/file1");
  uv_close((uv_handle_t*)handle, close_cb);
}

static void timer_cb_file(uv_timer_t* handle, int status) {
  ++timer_cb_called;
  
  if (timer_cb_called == 1) {
    touch_file(handle->loop, "watch_dir/file1");
  } else {
    touch_file(handle->loop, "watch_dir/file2");
    uv_close((uv_handle_t*)handle, close_cb);
  }
}

TEST_IMPL(fs_event_watch_dir) {
  uv_fs_t fs_req;
  uv_loop_t* loop = uv_default_loop();
  int r;

  /* Setup */
  uv_fs_unlink(loop, &fs_req, "watch_dir/file1", NULL);
  uv_fs_unlink(loop, &fs_req, "watch_dir/file2", NULL);
  uv_fs_rmdir(loop, &fs_req, "watch_dir", NULL);
  create_dir(loop, "watch_dir");

  r = uv_fs_event_init(loop, &fs_event, "watch_dir", fs_event_cb_dir);
  ASSERT(r != -1);
  r = uv_timer_init(loop, &timer);
  ASSERT(r != -1);
  r = uv_timer_start(&timer, timer_cb_dir, 100, 0);
  ASSERT(r != -1);

  uv_run(loop);

  ASSERT(fs_event_cb_called == 1);
  ASSERT(timer_cb_called == 1);
  ASSERT(close_cb_called == 2);

  /* Cleanup */
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file1", NULL);
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file2", NULL);
  r = uv_fs_rmdir(loop, &fs_req, "watch_dir", NULL);

  return 0;
}

TEST_IMPL(fs_event_watch_file) {
  uv_fs_t fs_req;
  uv_loop_t* loop = uv_default_loop();
  int r;

  /* Setup */
  uv_fs_unlink(loop, &fs_req, "watch_dir/file1", NULL);
  uv_fs_unlink(loop, &fs_req, "watch_dir/file2", NULL);
  uv_fs_rmdir(loop, &fs_req, "watch_dir", NULL);
  create_dir(loop, "watch_dir");
  create_file(loop, "watch_dir/file1");
  create_file(loop, "watch_dir/file2");

  r = uv_fs_event_init(loop, &fs_event, "watch_dir/file2", fs_event_cb_file);
  ASSERT(r != -1);
  r = uv_timer_init(loop, &timer);
  ASSERT(r != -1);
  r = uv_timer_start(&timer, timer_cb_file, 100, 100);
  ASSERT(r != -1);

  uv_run(loop);

  ASSERT(fs_event_cb_called == 1);
  ASSERT(timer_cb_called == 2);
  ASSERT(close_cb_called == 2);

  /* Cleanup */
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file1", NULL);
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file2", NULL);
  r = uv_fs_rmdir(loop, &fs_req, "watch_dir", NULL);

  return 0;
}

TEST_IMPL(fs_event_watch_file_current_dir) {
  uv_fs_t fs_req;
  uv_loop_t* loop = uv_default_loop();
  int r;

  /* Setup */
  uv_fs_unlink(loop, &fs_req, "watch_file", NULL);
  create_file(loop, "watch_file");

  r = uv_fs_event_init(loop, &fs_event, "watch_file",
    fs_event_cb_file_current_dir);
  ASSERT(r != -1);
  
  touch_file(loop, "watch_file");

  uv_run(loop);

  ASSERT(fs_event_cb_called == 1);
  ASSERT(close_cb_called == 1);

  /* Cleanup */
  r = uv_fs_unlink(loop, &fs_req, "watch_file", NULL);
  return 0;
}
