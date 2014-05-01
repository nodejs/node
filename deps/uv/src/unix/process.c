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
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#if defined(__APPLE__) && !TARGET_OS_IPHONE
# include <crt_externs.h>
# define environ (*_NSGetEnviron())
#else
extern char **environ;
#endif


static ngx_queue_t* uv__process_queue(uv_loop_t* loop, int pid) {
  assert(pid > 0);
  return loop->process_handles + pid % ARRAY_SIZE(loop->process_handles);
}


static uv_process_t* uv__process_find(uv_loop_t* loop, int pid) {
  uv_process_t* handle;
  ngx_queue_t* h;
  ngx_queue_t* q;

  h = uv__process_queue(loop, pid);

  ngx_queue_foreach(q, h) {
    handle = ngx_queue_data(q, uv_process_t, queue);
    if (handle->pid == pid) return handle;
  }

  return NULL;
}


static void uv__chld(uv_signal_t* handle, int signum) {
  uv_process_t* process;
  int exit_status;
  int term_signal;
  int status;
  pid_t pid;

  assert(signum == SIGCHLD);

  for (;;) {
    do
      pid = waitpid(-1, &status, WNOHANG);
    while (pid == -1 && errno == EINTR);

    if (pid == 0)
      return;

    if (pid == -1) {
      if (errno == ECHILD)
        return; /* XXX stop signal watcher? */
      else
        abort();
    }

    process = uv__process_find(handle->loop, pid);
    if (process == NULL)
      continue; /* XXX bug? abort? */

    uv__handle_stop(process);

    if (process->exit_cb == NULL)
      continue;

    exit_status = 0;
    term_signal = 0;

    if (WIFEXITED(status))
      exit_status = WEXITSTATUS(status);

    if (WIFSIGNALED(status))
      term_signal = WTERMSIG(status);

    if (process->errorno) {
      uv__set_sys_error(process->loop, process->errorno);
      exit_status = -1; /* execve() failed */
    }

    process->exit_cb(process, exit_status, term_signal);
  }
}


int uv__make_socketpair(int fds[2], int flags) {
#if defined(__linux__)
  static int no_cloexec;

  if (no_cloexec)
    goto skip;

  if (socketpair(AF_UNIX, SOCK_STREAM | UV__SOCK_CLOEXEC | flags, 0, fds) == 0)
    return 0;

  /* Retry on EINVAL, it means SOCK_CLOEXEC is not supported.
   * Anything else is a genuine error.
   */
  if (errno != EINVAL)
    return -1;

  no_cloexec = 1;

skip:
#endif

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds))
    return -1;

  uv__cloexec(fds[0], 1);
  uv__cloexec(fds[1], 1);

  if (flags & UV__F_NONBLOCK) {
    uv__nonblock(fds[0], 1);
    uv__nonblock(fds[1], 1);
  }

  return 0;
}


int uv__make_pipe(int fds[2], int flags) {
#if defined(__linux__)
  static int no_pipe2;

  if (no_pipe2)
    goto skip;

  if (uv__pipe2(fds, flags | UV__O_CLOEXEC) == 0)
    return 0;

  if (errno != ENOSYS)
    return -1;

  no_pipe2 = 1;

skip:
#endif

  if (pipe(fds))
    return -1;

  uv__cloexec(fds[0], 1);
  uv__cloexec(fds[1], 1);

  if (flags & UV__F_NONBLOCK) {
    uv__nonblock(fds[0], 1);
    uv__nonblock(fds[1], 1);
  }

  return 0;
}


/*
 * Used for initializing stdio streams like options.stdin_stream. Returns
 * zero on success. See also the cleanup section in uv_spawn().
 */
static int uv__process_init_stdio(uv_stdio_container_t* container, int fds[2]) {
  int mask;
  int fd;

  mask = UV_IGNORE | UV_CREATE_PIPE | UV_INHERIT_FD | UV_INHERIT_STREAM;

  switch (container->flags & mask) {
  case UV_IGNORE:
    return 0;

  case UV_CREATE_PIPE:
    assert(container->data.stream != NULL);
    if (container->data.stream->type != UV_NAMED_PIPE) {
      errno = EINVAL;
      return -1;
    }
    return uv__make_socketpair(fds, 0);

  case UV_INHERIT_FD:
  case UV_INHERIT_STREAM:
    if (container->flags & UV_INHERIT_FD)
      fd = container->data.fd;
    else
      fd = uv__stream_fd(container->data.stream);

    if (fd == -1) {
      errno = EINVAL;
      return -1;
    }

    fds[1] = fd;
    return 0;

  default:
    assert(0 && "Unexpected flags");
    return -1;
  }
}


static int uv__process_open_stream(uv_stdio_container_t* container,
                                   int pipefds[2],
                                   int writable) {
  int flags;

  if (!(container->flags & UV_CREATE_PIPE) || pipefds[0] < 0)
    return 0;

  if (close(pipefds[1]))
    if (errno != EINTR && errno != EINPROGRESS)
      abort();

  pipefds[1] = -1;
  uv__nonblock(pipefds[0], 1);

  if (container->data.stream->type == UV_NAMED_PIPE &&
      ((uv_pipe_t*)container->data.stream)->ipc)
    flags = UV_STREAM_READABLE | UV_STREAM_WRITABLE;
  else if (writable)
    flags = UV_STREAM_WRITABLE;
  else
    flags = UV_STREAM_READABLE;

  return uv__stream_open(container->data.stream, pipefds[0], flags);
}


static void uv__process_close_stream(uv_stdio_container_t* container) {
  if (!(container->flags & UV_CREATE_PIPE)) return;
  uv__stream_close((uv_stream_t*)container->data.stream);
}


static void uv__write_int(int fd, int val) {
  ssize_t n;

  do
    n = write(fd, &val, sizeof(val));
  while (n == -1 && errno == EINTR);

  if (n == -1 && errno == EPIPE)
    return; /* parent process has quit */

  assert(n == sizeof(val));
}


static void uv__process_child_init(uv_process_options_t options,
                                   int stdio_count,
                                   int (*pipes)[2],
                                   int error_fd) {
  int close_fd;
  int use_fd;
  int fd;

  if (options.flags & UV_PROCESS_DETACHED)
    setsid();

  for (fd = 0; fd < stdio_count; fd++) {
    close_fd = pipes[fd][0];
    use_fd = pipes[fd][1];

    if (use_fd < 0) {
      if (fd >= 3)
        continue;
      else {
        /* redirect stdin, stdout and stderr to /dev/null even if UV_IGNORE is
         * set
         */
        use_fd = open("/dev/null", fd == 0 ? O_RDONLY : O_RDWR);
        close_fd = use_fd;

        if (use_fd == -1) {
          uv__write_int(error_fd, errno);
          _exit(127);
        }
      }
    }

    if (fd == use_fd)
      uv__cloexec(use_fd, 0);
    else
      dup2(use_fd, fd);

    if (fd <= 2)
      uv__nonblock(fd, 0);

    if (close_fd >= stdio_count)
      close(close_fd);
  }

  for (fd = 0; fd < stdio_count; fd++) {
    use_fd = pipes[fd][1];

    if (use_fd >= 0 && fd != use_fd)
      close(use_fd);
  }

  if (options.cwd && chdir(options.cwd)) {
    uv__write_int(error_fd, errno);
    _exit(127);
  }

  if ((options.flags & UV_PROCESS_SETGID) && setgid(options.gid)) {
    uv__write_int(error_fd, errno);
    _exit(127);
  }

  if ((options.flags & UV_PROCESS_SETUID) && setuid(options.uid)) {
    uv__write_int(error_fd, errno);
    _exit(127);
  }

  if (options.env) {
    environ = options.env;
  }

  execvp(options.file, options.args);
  uv__write_int(error_fd, errno);
  _exit(127);
}


int uv_spawn(uv_loop_t* loop,
             uv_process_t* process,
             const uv_process_options_t options) {
  int signal_pipe[2] = { -1, -1 };
  int (*pipes)[2];
  int stdio_count;
  ngx_queue_t* q;
  ssize_t r;
  pid_t pid;
  int i;

  assert(options.file != NULL);
  assert(!(options.flags & ~(UV_PROCESS_DETACHED |
                             UV_PROCESS_SETGID |
                             UV_PROCESS_SETUID |
                             UV_PROCESS_WINDOWS_HIDE |
                             UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS)));

  uv__handle_init(loop, (uv_handle_t*)process, UV_PROCESS);
  ngx_queue_init(&process->queue);

  stdio_count = options.stdio_count;
  if (stdio_count < 3)
    stdio_count = 3;

  pipes = malloc(stdio_count * sizeof(*pipes));
  if (pipes == NULL) {
    errno = ENOMEM;
    goto error;
  }

  for (i = 0; i < stdio_count; i++) {
    pipes[i][0] = -1;
    pipes[i][1] = -1;
  }

  for (i = 0; i < options.stdio_count; i++)
    if (uv__process_init_stdio(options.stdio + i, pipes[i]))
      goto error;

  /* This pipe is used by the parent to wait until
   * the child has called `execve()`. We need this
   * to avoid the following race condition:
   *
   *    if ((pid = fork()) > 0) {
   *      kill(pid, SIGTERM);
   *    }
   *    else if (pid == 0) {
   *      execve("/bin/cat", argp, envp);
   *    }
   *
   * The parent sends a signal immediately after forking.
   * Since the child may not have called `execve()` yet,
   * there is no telling what process receives the signal,
   * our fork or /bin/cat.
   *
   * To avoid ambiguity, we create a pipe with both ends
   * marked close-on-exec. Then, after the call to `fork()`,
   * the parent polls the read end until it EOFs or errors with EPIPE.
   */
  if (uv__make_pipe(signal_pipe, 0))
    goto error;

  uv_signal_start(&loop->child_watcher, uv__chld, SIGCHLD);

  pid = fork();

  if (pid == -1) {
    close(signal_pipe[0]);
    close(signal_pipe[1]);
    goto error;
  }

  if (pid == 0) {
    uv__process_child_init(options, stdio_count, pipes, signal_pipe[1]);
    abort();
  }

  close(signal_pipe[1]);

  process->errorno = 0;
  do
    r = read(signal_pipe[0], &process->errorno, sizeof(process->errorno));
  while (r == -1 && errno == EINTR);

  if (r == 0)
    ; /* okay, EOF */
  else if (r == sizeof(process->errorno))
    ; /* okay, read errorno */
  else if (r == -1 && errno == EPIPE)
    ; /* okay, got EPIPE */
  else
    abort();

  close(signal_pipe[0]);

  for (i = 0; i < options.stdio_count; i++) {
    if (uv__process_open_stream(options.stdio + i, pipes[i], i == 0)) {
      while (i--) uv__process_close_stream(options.stdio + i);
      goto error;
    }
  }

  q = uv__process_queue(loop, pid);
  ngx_queue_insert_tail(q, &process->queue);

  process->pid = pid;
  process->exit_cb = options.exit_cb;
  uv__handle_start(process);

  free(pipes);
  return 0;

error:
  uv__set_sys_error(process->loop, errno);

  if (pipes != NULL) {
    for (i = 0; i < stdio_count; i++) {
      if (i < options.stdio_count)
        if (options.stdio[i].flags & (UV_INHERIT_FD | UV_INHERIT_STREAM))
          continue;
      if (pipes[i][0] != -1)
        close(pipes[i][0]);
      if (pipes[i][1] != -1)
        close(pipes[i][1]);
    }
    free(pipes);
  }

  return -1;
}


int uv_process_kill(uv_process_t* process, int signum) {
  int r = kill(process->pid, signum);

  if (r) {
    uv__set_sys_error(process->loop, errno);
    return -1;
  } else {
    return 0;
  }
}


uv_err_t uv_kill(int pid, int signum) {
  int r = kill(pid, signum);

  if (r) {
    return uv__new_sys_error(errno);
  } else {
    return uv_ok_;
  }
}


void uv__process_close(uv_process_t* handle) {
  /* TODO stop signal watcher when this is the last handle */
  ngx_queue_remove(&handle->queue);
  uv__handle_stop(handle);
}
