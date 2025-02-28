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

#include "task.h"
#include "uv.h"

#include <stdio.h>
#include <stdlib.h>

#define NUM_SYNC_REQS         (10 * 1e5)
#define NUM_ASYNC_REQS        (1 * (int) 1e5)
#define MAX_CONCURRENT_REQS   32

#define sync_stat(req, path)                                                  \
  do {                                                                        \
    uv_fs_stat(NULL, (req), (path), NULL);                                    \
    uv_fs_req_cleanup((req));                                                 \
  }                                                                           \
  while (0)

struct async_req {
  const char* path;
  uv_fs_t fs_req;
  int* count;
};


static void warmup(const char* path) {
  uv_fs_t reqs[MAX_CONCURRENT_REQS];
  unsigned int i;

  /* warm up the thread pool */
  for (i = 0; i < ARRAY_SIZE(reqs); i++)
    uv_fs_stat(uv_default_loop(), reqs + i, path, uv_fs_req_cleanup);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  /* warm up the OS dirent cache */
  for (i = 0; i < 16; i++)
    sync_stat(reqs + 0, path);
}


static void sync_bench(const char* path) {
  char fmtbuf[2][32];
  uint64_t before;
  uint64_t after;
  uv_fs_t req;
  int i;

  /* do the sync benchmark */
  before = uv_hrtime();

  for (i = 0; i < NUM_SYNC_REQS; i++)
    sync_stat(&req, path);

  after = uv_hrtime();

  printf("%s stats (sync): %.2fs (%s/s)\n",
         fmt(&fmtbuf[0], 1.0 * NUM_SYNC_REQS),
         (after - before) / 1e9,
         fmt(&fmtbuf[1], (1.0 * NUM_SYNC_REQS) / ((after - before) / 1e9)));
  fflush(stdout);
}


static void stat_cb(uv_fs_t* fs_req) {
  struct async_req* req = container_of(fs_req, struct async_req, fs_req);
  uv_fs_req_cleanup(&req->fs_req);
  if (*req->count == 0) return;
  uv_fs_stat(uv_default_loop(), &req->fs_req, req->path, stat_cb);
  (*req->count)--;
}


static void async_bench(const char* path) {
  struct async_req reqs[MAX_CONCURRENT_REQS];
  struct async_req* req;
  char fmtbuf[2][32];
  uint64_t before;
  uint64_t after;
  int count;
  int i;

  for (i = 1; i <= MAX_CONCURRENT_REQS; i++) {
    count = NUM_ASYNC_REQS;

    for (req = reqs; req < reqs + i; req++) {
      req->path = path;
      req->count = &count;
      uv_fs_stat(uv_default_loop(), &req->fs_req, req->path, stat_cb);
    }

    before = uv_hrtime();
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    after = uv_hrtime();

    printf("%s stats (%d concurrent): %.2fs (%s/s)\n",
           fmt(&fmtbuf[0], 1.0 * NUM_ASYNC_REQS),
           i,
           (after - before) / 1e9,
           fmt(&fmtbuf[1], (1.0 * NUM_ASYNC_REQS) / ((after - before) / 1e9)));
    fflush(stdout);
  }
}


/* This benchmark aims to measure the overhead of doing I/O syscalls from
 * the thread pool. The stat() syscall was chosen because its results are
 * easy for the operating system to cache, taking the actual I/O overhead
 * out of the equation.
 */
BENCHMARK_IMPL(fs_stat) {
  const char path[] = ".";
  warmup(path);
  sync_bench(path);
  async_bench(path);
  MAKE_VALGRIND_HAPPY(uv_default_loop());
  return 0;
}
