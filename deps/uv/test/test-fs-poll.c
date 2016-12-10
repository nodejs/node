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

#define FIXTURE "testfile"

static void timer_cb(uv_timer_t* handle);
static void close_cb(uv_handle_t* handle);
static void poll_cb(uv_fs_poll_t* handle,
                    int status,
                    const uv_stat_t* prev,
                    const uv_stat_t* curr);

static void poll_cb_fail(uv_fs_poll_t* handle,
                         int status,
                         const uv_stat_t* prev,
                         const uv_stat_t* curr);

static uv_fs_poll_t poll_handle;
static uv_timer_t timer_handle;
static uv_loop_t* loop;

static int poll_cb_called;
static int timer_cb_called;
static int close_cb_called;


static void touch_file(const char* path) {
  static int count;
  FILE* fp;
  int i;

  ASSERT((fp = fopen(FIXTURE, "w+")));

  /* Need to change the file size because the poller may not pick up
   * sub-second mtime changes.
   */
  i = ++count;

  while (i--)
    fputc('*', fp);

  fclose(fp);
}


static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}


static void timer_cb(uv_timer_t* handle) {
  touch_file(FIXTURE);
  timer_cb_called++;
}


static void poll_cb_fail(uv_fs_poll_t* handle,
                         int status,
                         const uv_stat_t* prev,
                         const uv_stat_t* curr) {
  ASSERT(0 && "fail_cb called");
}


static void poll_cb(uv_fs_poll_t* handle,
                    int status,
                    const uv_stat_t* prev,
                    const uv_stat_t* curr) {
  uv_stat_t zero_statbuf;

  memset(&zero_statbuf, 0, sizeof(zero_statbuf));

  ASSERT(handle == &poll_handle);
  ASSERT(1 == uv_is_active((uv_handle_t*) handle));
  ASSERT(prev != NULL);
  ASSERT(curr != NULL);

  switch (poll_cb_called++) {
  case 0:
    ASSERT(status == UV_ENOENT);
    ASSERT(0 == memcmp(prev, &zero_statbuf, sizeof(zero_statbuf)));
    ASSERT(0 == memcmp(curr, &zero_statbuf, sizeof(zero_statbuf)));
    touch_file(FIXTURE);
    break;

  case 1:
    ASSERT(status == 0);
    ASSERT(0 == memcmp(prev, &zero_statbuf, sizeof(zero_statbuf)));
    ASSERT(0 != memcmp(curr, &zero_statbuf, sizeof(zero_statbuf)));
    ASSERT(0 == uv_timer_start(&timer_handle, timer_cb, 20, 0));
    break;

  case 2:
    ASSERT(status == 0);
    ASSERT(0 != memcmp(prev, &zero_statbuf, sizeof(zero_statbuf)));
    ASSERT(0 != memcmp(curr, &zero_statbuf, sizeof(zero_statbuf)));
    ASSERT(0 == uv_timer_start(&timer_handle, timer_cb, 200, 0));
    break;

  case 3:
    ASSERT(status == 0);
    ASSERT(0 != memcmp(prev, &zero_statbuf, sizeof(zero_statbuf)));
    ASSERT(0 != memcmp(curr, &zero_statbuf, sizeof(zero_statbuf)));
    remove(FIXTURE);
    break;

  case 4:
    ASSERT(status == UV_ENOENT);
    ASSERT(0 != memcmp(prev, &zero_statbuf, sizeof(zero_statbuf)));
    ASSERT(0 == memcmp(curr, &zero_statbuf, sizeof(zero_statbuf)));
    uv_close((uv_handle_t*)handle, close_cb);
    break;

  default:
    ASSERT(0);
  }
}


TEST_IMPL(fs_poll) {
  loop = uv_default_loop();

  remove(FIXTURE);

  ASSERT(0 == uv_timer_init(loop, &timer_handle));
  ASSERT(0 == uv_fs_poll_init(loop, &poll_handle));
  ASSERT(0 == uv_fs_poll_start(&poll_handle, poll_cb, FIXTURE, 100));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  ASSERT(poll_cb_called == 5);
  ASSERT(timer_cb_called == 2);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_poll_getpath) {
  char buf[1024];
  size_t len;
  loop = uv_default_loop();

  remove(FIXTURE);

  ASSERT(0 == uv_fs_poll_init(loop, &poll_handle));
  len = sizeof buf;
  ASSERT(UV_EINVAL == uv_fs_poll_getpath(&poll_handle, buf, &len));
  ASSERT(0 == uv_fs_poll_start(&poll_handle, poll_cb_fail, FIXTURE, 100));
  len = sizeof buf;
  ASSERT(0 == uv_fs_poll_getpath(&poll_handle, buf, &len));
  ASSERT(buf[len - 1] != 0);
  ASSERT(buf[len] == '\0');
  ASSERT(0 == memcmp(buf, FIXTURE, len));

  uv_close((uv_handle_t*) &poll_handle, close_cb);

  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
