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
#define UNIX (defined(__unix__) || defined(__POSIX__) || defined(__APPLE__))
#include "task.h"
#include "uv.h"
#include <fcntl.h>

#if UNIX
#include <unistd.h> /* unlink, rmdir, etc. */
#else
# include <direct.h>
# include <io.h>
# define unlink _unlink
# define rmdir _rmdir
# define stat _stati64
# define open _open
# define write _write
# define lseek _lseek
# define close _close
#endif

static char exepath[1024];
static size_t exepath_size = 1024;
static char* args[3];
static uv_fs_t open_req;
static uv_tcp_t tcp;
static uv_udp_t udp;
static uv_pipe_t uvpipe;
static uv_tty_t tty;
static uv_prepare_t prepare;
static uv_check_t check;
static uv_idle_t idle;
static uv_async_t async;
static uv_timer_t timer;
static uv_fs_event_t fs_event;
static uv_process_t process;
static uv_process_options_t options;
static uv_fs_t fs_req;

static void exit_cb(uv_process_t* process, int exit_status, int term_signal) {
  ASSERT(exit_status == 1);
  ASSERT(term_signal == 0);
  uv_close((uv_handle_t*)process, NULL);
}

static void init_process_options(char* test, uv_exit_cb exit_cb) {
  int r = uv_exepath(exepath, &exepath_size);
  ASSERT(r == 0);
  exepath[exepath_size] = '\0';
  args[0] = exepath;
  args[1] = test;
  args[2] = NULL;
  options.file = exepath;
  options.args = args;
  options.exit_cb = exit_cb;
}

static void create_dir(uv_loop_t* loop, const char* name) {
  int r;
  uv_fs_t req;
  r = uv_fs_rmdir(loop, &req, name, NULL);
  r = uv_fs_mkdir(loop, &req, name, 0755, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);
}

static void create_cb(uv_fs_t* req) {
  ASSERT(req == &open_req);
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result != -1);
  uv_fs_req_cleanup(req);
  unlink("test_file");
}

TEST_IMPL(counters_init) {
  int r;
  int eio_init_prev;
  int req_init_prev;
  int handle_init_prev;
  int stream_init_prev;
  int tcp_init_prev;
  int udp_init_prev;
  int pipe_init_prev;
  int tty_init_prev;
  int prepare_init_prev;
  int check_init_prev;
  int idle_init_prev;
  int async_init_prev;
  int timer_init_prev;
  int process_init_prev;
  int fs_event_init_prev;

  /* req_init and eio_init test by uv_fs_open() */
  unlink("test_file");
  req_init_prev = uv_default_loop()->counters.req_init;
  eio_init_prev = uv_default_loop()->counters.eio_init;
  r = uv_fs_open(uv_default_loop(), &open_req, "test_file", O_WRONLY | O_CREAT,
                 S_IREAD | S_IWRITE, create_cb);
  ASSERT(r == 0);
  ASSERT(open_req.result == 0);
  ASSERT(uv_default_loop()->counters.req_init == ++req_init_prev);
#ifndef _WIN32
  ASSERT(uv_default_loop()->counters.eio_init == ++eio_init_prev);
#endif

  /* tcp_init, stream_init and handle_init test by uv_tcp_init() */
  tcp_init_prev = uv_default_loop()->counters.tcp_init;
  stream_init_prev = uv_default_loop()->counters.stream_init;
  handle_init_prev = uv_default_loop()->counters.handle_init;
  r = uv_tcp_init(uv_default_loop(), &tcp);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.tcp_init == ++tcp_init_prev);
  ASSERT(uv_default_loop()->counters.stream_init == ++stream_init_prev);
  ASSERT(uv_default_loop()->counters.handle_init == ++handle_init_prev);
  uv_close((uv_handle_t*)&tcp, NULL);

  /* udp_init test by uv_udp_init() */
  udp_init_prev = uv_default_loop()->counters.udp_init;
  r = uv_udp_init(uv_default_loop(), &udp);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.udp_init == ++udp_init_prev);
  uv_close((uv_handle_t*)&udp, NULL);

  /* pipe_init uv_pipe_init() */
  pipe_init_prev = uv_default_loop()->counters.pipe_init;
  uv_pipe_init(uv_default_loop(), &uvpipe, 0);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.pipe_init == ++pipe_init_prev);
  uv_close((uv_handle_t*)&uvpipe, NULL);

  /* tty_init test by uv_tty_init()*/
  tty_init_prev = uv_default_loop()->counters.tty_init;
  r = uv_tty_init(uv_default_loop(), &tty, 1, 0);
  /* uv_tty_init() always returns -1 in run_test in Windows
     so that we avoid to check return value.
   */
#ifndef _WIN32
  ASSERT(r == 0);
  uv_close((uv_handle_t*)&tty, NULL);
#endif
  ASSERT(uv_default_loop()->counters.tty_init == ++tty_init_prev);

  /* prepare_init test by uv_prepare_init() */
  prepare_init_prev = uv_default_loop()->counters.prepare_init;
  r = uv_prepare_init(uv_default_loop(), &prepare);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.prepare_init == ++prepare_init_prev);
  uv_close((uv_handle_t*)&prepare, NULL);

  /* check_init test by uv_check_init() */
  check_init_prev = uv_default_loop()->counters.check_init;
  r = uv_check_init(uv_default_loop(), &check);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.check_init == ++check_init_prev);
  uv_close((uv_handle_t*)&check, NULL);

  /* idle_init test by uv_idle_init() */
  idle_init_prev = uv_default_loop()->counters.idle_init;
  r = uv_idle_init(uv_default_loop(), &idle);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.idle_init == ++idle_init_prev);
  uv_close((uv_handle_t*)&idle, NULL);

  /* async_init test by uv_async_init() */
  async_init_prev = uv_default_loop()->counters.async_init;
  r = uv_async_init(uv_default_loop(), &async, NULL);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.async_init == ++async_init_prev);
  uv_close((uv_handle_t*)&async, NULL);

  /* timer_init test by uv_timer_init() */
  timer_init_prev = uv_default_loop()->counters.timer_init;
  r = uv_timer_init(uv_default_loop(), &timer);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.timer_init == ++timer_init_prev);
  uv_close((uv_handle_t*)&timer, NULL);

  /* process_init test by uv_spawn() */
  process_init_prev = uv_default_loop()->counters.process_init;
  init_process_options("spawn_helper1", exit_cb);
  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.process_init == ++process_init_prev);
  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  /* fs_event_init test by uv_fs_event_init() */
  create_dir(uv_default_loop(), "watch_dir");
  fs_event_init_prev = uv_default_loop()->counters.fs_event_init;
  r = uv_fs_event_init(uv_default_loop(), &fs_event, "watch_dir", NULL, 0);
  ASSERT(r == 0);
  ASSERT(uv_default_loop()->counters.fs_event_init == ++fs_event_init_prev);
  r = uv_fs_rmdir(uv_default_loop(), &fs_req, "watch_dir", NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&fs_req);

  return 0;
}
