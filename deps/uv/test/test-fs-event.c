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

static uv_fs_event_t fs_event;
static uv_timer_t timer;
static int timer_cb_called = 0;
static int close_cb_called = 0;
static int fs_event_cb_called = 0;
static int timer_cb_touch_called = 0;

static void create_dir(uv_loop_t* loop, const char* name) {
  int r;
  uv_fs_t req;
  r = uv_fs_mkdir(loop, &req, name, 0755, NULL);
  ASSERT(r == 0 || uv_last_error(loop).code == UV_EEXIST);
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
  ASSERT(filename == NULL || strcmp(filename, "file1") == 0);
  uv_close((uv_handle_t*)handle, close_cb);
}

static void fs_event_cb_file(uv_fs_event_t* handle, const char* filename,
  int events, int status) {
  ++fs_event_cb_called;
  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_CHANGE);
  ASSERT(filename == NULL || strcmp(filename, "file2") == 0);
  uv_close((uv_handle_t*)handle, close_cb);
}

static void fs_event_cb_file_current_dir(uv_fs_event_t* handle,
  const char* filename, int events, int status) {
  ++fs_event_cb_called;
  ASSERT(handle == &fs_event);
  ASSERT(status == 0);
  ASSERT(events == UV_CHANGE);
  ASSERT(filename == NULL || strcmp(filename, "watch_file") == 0);
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

static void timer_cb_touch(uv_timer_t* timer, int status) {
  ASSERT(status == 0);
  uv_close((uv_handle_t*)timer, NULL);
  touch_file(timer->loop, "watch_file");
  timer_cb_touch_called++;
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

  r = uv_fs_event_init(loop, &fs_event, "watch_dir", fs_event_cb_dir, 0);
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

  r = uv_fs_event_init(loop, &fs_event, "watch_dir/file2", fs_event_cb_file, 0);
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
  uv_timer_t timer;
  uv_loop_t* loop;
  uv_fs_t fs_req;
  int r;

  loop = uv_default_loop();

  /* Setup */
  uv_fs_unlink(loop, &fs_req, "watch_file", NULL);
  create_file(loop, "watch_file");

  r = uv_fs_event_init(loop, &fs_event, "watch_file",
    fs_event_cb_file_current_dir, 0);
  ASSERT(r != -1);

  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);

  r = uv_timer_start(&timer, timer_cb_touch, 1, 0);
  ASSERT(r == 0);

  ASSERT(timer_cb_touch_called == 0);
  ASSERT(fs_event_cb_called == 0);
  ASSERT(close_cb_called == 0);

  uv_run(loop);

  ASSERT(timer_cb_touch_called == 1);
  ASSERT(fs_event_cb_called == 1);
  ASSERT(close_cb_called == 1);

  /* Cleanup */
  r = uv_fs_unlink(loop, &fs_req, "watch_file", NULL);
  return 0;
}


TEST_IMPL(fs_event_no_callback_on_close) {
  uv_fs_t fs_req;
  uv_loop_t* loop = uv_default_loop();
  int r;

  /* Setup */
  uv_fs_unlink(loop, &fs_req, "watch_dir/file1", NULL);
  uv_fs_rmdir(loop, &fs_req, "watch_dir", NULL);
  create_dir(loop, "watch_dir");
  create_file(loop, "watch_dir/file1");

  r = uv_fs_event_init(loop,
                       &fs_event,
                       "watch_dir/file1",
                       fs_event_cb_file,
                       0);
  ASSERT(r != -1);

  uv_close((uv_handle_t*)&fs_event, close_cb);

  uv_run(loop);

  ASSERT(fs_event_cb_called == 0);
  ASSERT(close_cb_called == 1);

  /* Cleanup */
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file1", NULL);
  r = uv_fs_rmdir(loop, &fs_req, "watch_dir", NULL);

  return 0;
}


static void fs_event_fail(uv_fs_event_t* handle, const char* filename,
    int events, int status) {
  ASSERT(0 && "should never be called");
}


static void timer_cb(uv_timer_t* handle, int status) {
  int r;

  ASSERT(status == 0);

  r = uv_fs_event_init(handle->loop, &fs_event, ".", fs_event_fail, 0);
  ASSERT(r == 0);

  uv_close((uv_handle_t*)&fs_event, close_cb);
  uv_close((uv_handle_t*)handle, close_cb);
}


TEST_IMPL(fs_event_immediate_close) {
  uv_timer_t timer;
  uv_loop_t* loop;
  int r;

  loop = uv_default_loop();

  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);

  r = uv_timer_start(&timer, timer_cb, 1, 0);
  ASSERT(r == 0);

  uv_run(loop);

  ASSERT(close_cb_called == 2);

  return 0;
}


TEST_IMPL(fs_event_close_with_pending_event) {
  uv_loop_t* loop;
  uv_fs_t fs_req;
  int r;

  loop = uv_default_loop();

  create_dir(loop, "watch_dir");
  create_file(loop, "watch_dir/file");

  r = uv_fs_event_init(loop, &fs_event, "watch_dir", fs_event_fail, 0);
  ASSERT(r == 0);

  /* Generate an fs event. */
  touch_file(loop, "watch_dir/file");

  uv_close((uv_handle_t*)&fs_event, close_cb);

  uv_run(loop);

  ASSERT(close_cb_called == 1);

  /* Clean up */
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file", NULL);
  ASSERT(r == 0);
  r = uv_fs_rmdir(loop, &fs_req, "watch_dir", NULL);
  ASSERT(r == 0);

  return 0;
}


static void fs_event_cb_close(uv_fs_event_t* handle, const char* filename,
    int events, int status) {
  ASSERT(status == 0);

  ASSERT(fs_event_cb_called < 3);
  ++fs_event_cb_called;

  if (fs_event_cb_called == 3) {
    uv_close((uv_handle_t*) handle, close_cb);
  }
}


TEST_IMPL(fs_event_close_in_callback) {
  uv_loop_t* loop;
  uv_fs_t fs_req;
  int r;

  loop = uv_default_loop();

  create_dir(loop, "watch_dir");
  create_file(loop, "watch_dir/file1");
  create_file(loop, "watch_dir/file2");
  create_file(loop, "watch_dir/file3");
  create_file(loop, "watch_dir/file4");
  create_file(loop, "watch_dir/file5");

  r = uv_fs_event_init(loop, &fs_event, "watch_dir", fs_event_cb_close, 0);
  ASSERT(r == 0);

  /* Generate a couple of fs events. */
  touch_file(loop, "watch_dir/file1");
  touch_file(loop, "watch_dir/file2");
  touch_file(loop, "watch_dir/file3");
  touch_file(loop, "watch_dir/file4");
  touch_file(loop, "watch_dir/file5");

  uv_run(loop);

  ASSERT(close_cb_called == 1);
  ASSERT(fs_event_cb_called == 3);

  /* Clean up */
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file1", NULL);
  ASSERT(r == 0);
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file2", NULL);
  ASSERT(r == 0);
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file3", NULL);
  ASSERT(r == 0);
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file4", NULL);
  ASSERT(r == 0);
  r = uv_fs_unlink(loop, &fs_req, "watch_dir/file5", NULL);
  ASSERT(r == 0);
  r = uv_fs_rmdir(loop, &fs_req, "watch_dir", NULL);
  ASSERT(r == 0);

  return 0;
}
