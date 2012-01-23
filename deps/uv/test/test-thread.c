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
#include <string.h> /* memset */

struct getaddrinfo_req {
  uv_thread_t thread_id;
  unsigned int counter;
  uv_loop_t* loop;
  uv_getaddrinfo_t handle;
};


struct fs_req {
  uv_thread_t thread_id;
  unsigned int counter;
  uv_loop_t* loop;
  uv_fs_t handle;
};


struct thread {
  uv_thread_t thread_id;
  volatile int thread_called;
};

static void getaddrinfo_do(struct getaddrinfo_req* req);
static void getaddrinfo_cb(uv_getaddrinfo_t* handle,
                           int status,
                           struct addrinfo* res);
static void fs_do(struct fs_req* req);
static void fs_cb(uv_fs_t* handle);

static volatile int thread_called;


static void getaddrinfo_do(struct getaddrinfo_req* req) {
  int r;

  r = uv_getaddrinfo(req->loop,
                     &req->handle,
                     getaddrinfo_cb,
                     "localhost",
                     NULL,
                     NULL);
  ASSERT(r == 0);
}


static void getaddrinfo_cb(uv_getaddrinfo_t* handle,
                           int status,
                           struct addrinfo* res) {
  struct getaddrinfo_req* req;

  ASSERT(status == 0);

  req = container_of(handle, struct getaddrinfo_req, handle);
  uv_freeaddrinfo(res);

  if (--req->counter)
    getaddrinfo_do(req);
}


static void fs_do(struct fs_req* req) {
  int r;

  r = uv_fs_stat(req->loop, &req->handle, ".", fs_cb);
  ASSERT(r == 0);
}


static void fs_cb(uv_fs_t* handle) {
  struct fs_req* req = container_of(handle, struct fs_req, handle);

  uv_fs_req_cleanup(handle);

  if (--req->counter)
    fs_do(req);
}


static void do_work(void* arg) {
  struct getaddrinfo_req getaddrinfo_reqs[16];
  struct fs_req fs_reqs[16];
  uv_loop_t* loop;
  size_t i;
  int r;
  struct thread* thread = arg;

  loop = uv_loop_new();
  ASSERT(loop != NULL);

  for (i = 0; i < ARRAY_SIZE(getaddrinfo_reqs); i++) {
    struct getaddrinfo_req* req = getaddrinfo_reqs + i;
    req->counter = 16;
    req->loop = loop;
    getaddrinfo_do(req);
  }

  for (i = 0; i < ARRAY_SIZE(fs_reqs); i++) {
    struct fs_req* req = fs_reqs + i;
    req->counter = 16;
    req->loop = loop;
    fs_do(req);
  }

  r = uv_run(loop);
  ASSERT(r == 0);

  uv_loop_delete(loop);
  thread->thread_called = 1;
}


static void thread_entry(void* arg) {
  ASSERT(arg == (void *) 42);
  thread_called++;
}


TEST_IMPL(thread_create) {
  uv_thread_t tid;
  int r;

  r = uv_thread_create(&tid, thread_entry, (void *) 42);
  ASSERT(r == 0);

  r = uv_thread_join(&tid);
  ASSERT(r == 0);

  ASSERT(thread_called == 1);

  return 0;
}


/* Hilariously bad test name. Run a lot of tasks in the thread pool and verify
 * that each "finished" callback is run in its originating thread.
 */
TEST_IMPL(threadpool_multiple_event_loops) {
  struct thread threads[8];
  size_t i;
  int r;

  memset(threads, 0, sizeof(threads));

  for (i = 0; i < ARRAY_SIZE(threads); i++) {
    r = uv_thread_create(&threads[i].thread_id, do_work, &threads[i]);
    ASSERT(r == 0);
  }

  for (i = 0; i < ARRAY_SIZE(threads); i++) {
    r = uv_thread_join(&threads[i].thread_id);
    ASSERT(r == 0);
    ASSERT(threads[i].thread_called);
  }

  return 0;
}
