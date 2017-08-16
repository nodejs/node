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

#define INIT_CANCEL_INFO(ci, what)                                            \
  do {                                                                        \
    (ci)->reqs = (what);                                                      \
    (ci)->nreqs = ARRAY_SIZE(what);                                           \
    (ci)->stride = sizeof((what)[0]);                                         \
  }                                                                           \
  while (0)

struct cancel_info {
  void* reqs;
  unsigned nreqs;
  unsigned stride;
  uv_timer_t timer_handle;
};

static unsigned fs_cb_called;
static unsigned done_cb_called;
static unsigned done2_cb_called;
static unsigned timer_cb_called;
static uv_work_t pause_reqs[4];
static uv_sem_t pause_sems[ARRAY_SIZE(pause_reqs)];


static void work_cb(uv_work_t* req) {
  uv_sem_wait(pause_sems + (req - pause_reqs));
}


static void done_cb(uv_work_t* req, int status) {
  uv_sem_destroy(pause_sems + (req - pause_reqs));
}


static void saturate_threadpool(void) {
  uv_loop_t* loop;
  char buf[64];
  size_t i;

  snprintf(buf,
           sizeof(buf),
           "UV_THREADPOOL_SIZE=%lu",
           (unsigned long)ARRAY_SIZE(pause_reqs));
  putenv(buf);

  loop = uv_default_loop();
  for (i = 0; i < ARRAY_SIZE(pause_reqs); i += 1) {
    ASSERT(0 == uv_sem_init(pause_sems + i, 0));
    ASSERT(0 == uv_queue_work(loop, pause_reqs + i, work_cb, done_cb));
  }
}


static void unblock_threadpool(void) {
  size_t i;

  for (i = 0; i < ARRAY_SIZE(pause_reqs); i += 1)
    uv_sem_post(pause_sems + i);
}


static void fs_cb(uv_fs_t* req) {
  ASSERT(req->result == UV_ECANCELED);
  uv_fs_req_cleanup(req);
  fs_cb_called++;
}


static void getaddrinfo_cb(uv_getaddrinfo_t* req,
                           int status,
                           struct addrinfo* res) {
  ASSERT(status == UV_EAI_CANCELED);
  ASSERT(res == NULL);
  uv_freeaddrinfo(res);  /* Should not crash. */
}


static void getnameinfo_cb(uv_getnameinfo_t* handle,
                           int status,
                           const char* hostname,
                           const char* service) {
  ASSERT(status == UV_EAI_CANCELED);
  ASSERT(hostname == NULL);
  ASSERT(service == NULL);
}


static void work2_cb(uv_work_t* req) {
  ASSERT(0 && "work2_cb called");
}


static void done2_cb(uv_work_t* req, int status) {
  ASSERT(status == UV_ECANCELED);
  done2_cb_called++;
}


static void timer_cb(uv_timer_t* handle) {
  struct cancel_info* ci;
  uv_req_t* req;
  unsigned i;

  ci = container_of(handle, struct cancel_info, timer_handle);

  for (i = 0; i < ci->nreqs; i++) {
    req = (uv_req_t*) ((char*) ci->reqs + i * ci->stride);
    ASSERT(0 == uv_cancel(req));
  }

  uv_close((uv_handle_t*) &ci->timer_handle, NULL);
  unblock_threadpool();
  timer_cb_called++;
}


static void nop_done_cb(uv_work_t* req, int status) {
  ASSERT(status == UV_ECANCELED);
  done_cb_called++;
}


TEST_IMPL(threadpool_cancel_getaddrinfo) {
  uv_getaddrinfo_t reqs[4];
  struct cancel_info ci;
  struct addrinfo hints;
  uv_loop_t* loop;
  int r;

  INIT_CANCEL_INFO(&ci, reqs);
  loop = uv_default_loop();
  saturate_threadpool();

  r = uv_getaddrinfo(loop, reqs + 0, getaddrinfo_cb, "fail", NULL, NULL);
  ASSERT(r == 0);

  r = uv_getaddrinfo(loop, reqs + 1, getaddrinfo_cb, NULL, "fail", NULL);
  ASSERT(r == 0);

  r = uv_getaddrinfo(loop, reqs + 2, getaddrinfo_cb, "fail", "fail", NULL);
  ASSERT(r == 0);

  r = uv_getaddrinfo(loop, reqs + 3, getaddrinfo_cb, "fail", NULL, &hints);
  ASSERT(r == 0);

  ASSERT(0 == uv_timer_init(loop, &ci.timer_handle));
  ASSERT(0 == uv_timer_start(&ci.timer_handle, timer_cb, 10, 0));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == timer_cb_called);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(threadpool_cancel_getnameinfo) {
  uv_getnameinfo_t reqs[4];
  struct sockaddr_in addr4;
  struct cancel_info ci;
  uv_loop_t* loop;
  int r;

  r = uv_ip4_addr("127.0.0.1", 80, &addr4);
  ASSERT(r == 0);

  INIT_CANCEL_INFO(&ci, reqs);
  loop = uv_default_loop();
  saturate_threadpool();

  r = uv_getnameinfo(loop, reqs + 0, getnameinfo_cb, (const struct sockaddr*)&addr4, 0);
  ASSERT(r == 0);

  r = uv_getnameinfo(loop, reqs + 1, getnameinfo_cb, (const struct sockaddr*)&addr4, 0);
  ASSERT(r == 0);

  r = uv_getnameinfo(loop, reqs + 2, getnameinfo_cb, (const struct sockaddr*)&addr4, 0);
  ASSERT(r == 0);

  r = uv_getnameinfo(loop, reqs + 3, getnameinfo_cb, (const struct sockaddr*)&addr4, 0);
  ASSERT(r == 0);

  ASSERT(0 == uv_timer_init(loop, &ci.timer_handle));
  ASSERT(0 == uv_timer_start(&ci.timer_handle, timer_cb, 10, 0));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == timer_cb_called);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(threadpool_cancel_work) {
  struct cancel_info ci;
  uv_work_t reqs[16];
  uv_loop_t* loop;
  unsigned i;

  INIT_CANCEL_INFO(&ci, reqs);
  loop = uv_default_loop();
  saturate_threadpool();

  for (i = 0; i < ARRAY_SIZE(reqs); i++)
    ASSERT(0 == uv_queue_work(loop, reqs + i, work2_cb, done2_cb));

  ASSERT(0 == uv_timer_init(loop, &ci.timer_handle));
  ASSERT(0 == uv_timer_start(&ci.timer_handle, timer_cb, 10, 0));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == timer_cb_called);
  ASSERT(ARRAY_SIZE(reqs) == done2_cb_called);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(threadpool_cancel_fs) {
  struct cancel_info ci;
  uv_fs_t reqs[26];
  uv_loop_t* loop;
  unsigned n;
  uv_buf_t iov;

  INIT_CANCEL_INFO(&ci, reqs);
  loop = uv_default_loop();
  saturate_threadpool();
  iov = uv_buf_init(NULL, 0);

  /* Needs to match ARRAY_SIZE(fs_reqs). */
  n = 0;
  ASSERT(0 == uv_fs_chmod(loop, reqs + n++, "/", 0, fs_cb));
  ASSERT(0 == uv_fs_chown(loop, reqs + n++, "/", 0, 0, fs_cb));
  ASSERT(0 == uv_fs_close(loop, reqs + n++, 0, fs_cb));
  ASSERT(0 == uv_fs_fchmod(loop, reqs + n++, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_fchown(loop, reqs + n++, 0, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_fdatasync(loop, reqs + n++, 0, fs_cb));
  ASSERT(0 == uv_fs_fstat(loop, reqs + n++, 0, fs_cb));
  ASSERT(0 == uv_fs_fsync(loop, reqs + n++, 0, fs_cb));
  ASSERT(0 == uv_fs_ftruncate(loop, reqs + n++, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_futime(loop, reqs + n++, 0, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_link(loop, reqs + n++, "/", "/", fs_cb));
  ASSERT(0 == uv_fs_lstat(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_mkdir(loop, reqs + n++, "/", 0, fs_cb));
  ASSERT(0 == uv_fs_open(loop, reqs + n++, "/", 0, 0, fs_cb));
  ASSERT(0 == uv_fs_read(loop, reqs + n++, 0, &iov, 1, 0, fs_cb));
  ASSERT(0 == uv_fs_scandir(loop, reqs + n++, "/", 0, fs_cb));
  ASSERT(0 == uv_fs_readlink(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_realpath(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_rename(loop, reqs + n++, "/", "/", fs_cb));
  ASSERT(0 == uv_fs_mkdir(loop, reqs + n++, "/", 0, fs_cb));
  ASSERT(0 == uv_fs_sendfile(loop, reqs + n++, 0, 0, 0, 0, fs_cb));
  ASSERT(0 == uv_fs_stat(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_symlink(loop, reqs + n++, "/", "/", 0, fs_cb));
  ASSERT(0 == uv_fs_unlink(loop, reqs + n++, "/", fs_cb));
  ASSERT(0 == uv_fs_utime(loop, reqs + n++, "/", 0, 0, fs_cb));
  ASSERT(0 == uv_fs_write(loop, reqs + n++, 0, &iov, 1, 0, fs_cb));
  ASSERT(n == ARRAY_SIZE(reqs));

  ASSERT(0 == uv_timer_init(loop, &ci.timer_handle));
  ASSERT(0 == uv_timer_start(&ci.timer_handle, timer_cb, 10, 0));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(n == fs_cb_called);
  ASSERT(1 == timer_cb_called);


  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(threadpool_cancel_single) {
  uv_loop_t* loop;
  uv_work_t req;

  saturate_threadpool();
  loop = uv_default_loop();
  ASSERT(0 == uv_queue_work(loop, &req, (uv_work_cb) abort, nop_done_cb));
  ASSERT(0 == uv_cancel((uv_req_t*) &req));
  ASSERT(0 == done_cb_called);
  unblock_threadpool();
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(1 == done_cb_called);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
