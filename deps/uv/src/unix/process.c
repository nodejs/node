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

#include <assert.h>
#include <errno.h>
#include <sys/wait.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#ifdef __APPLE__
# include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && !TARGET_OS_IPHONE
# include <crt_externs.h>
# define environ (*_NSGetEnviron())
#else
extern char **environ;
#endif


static void uv__chld(EV_P_ ev_child* watcher, int revents) {
  int status = watcher->rstatus;
  int exit_status = 0;
  int term_signal = 0;
  uv_process_t *process = watcher->data;

  assert(&process->child_watcher == watcher);
  assert(revents & EV_CHILD);

  ev_child_stop(EV_A_ &process->child_watcher);

  if (WIFEXITED(status)) {
    exit_status = WEXITSTATUS(status);
  }

  if (WIFSIGNALED(status)) {
    term_signal = WTERMSIG(status);
  }

  if (process->exit_cb) {
    process->exit_cb(process, exit_status, term_signal);
  }
}


int uv__make_socketpair(int fds[2], int flags) {
#if __linux__
  if (socketpair(AF_UNIX, SOCK_STREAM | UV__SOCK_CLOEXEC | flags, 0, fds) == 0)
    return 0;

  /* Retry on EINVAL, it means SOCK_CLOEXEC is not supported.
   * Anything else is a genuine error.
   */
  if (errno != EINVAL)
    return -1;
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
#if __linux__
  if (uv__pipe2(fds, flags | UV__O_CLOEXEC) == 0)
    return 0;

  if (errno != ENOSYS)
    return -1;
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
 * zero on success.
 */
static int uv__process_init_stdio(uv_stdio_container_t* container, int fds[2],
                                  int writable) {
  int fd = -1;
  switch (container->flags & (UV_IGNORE | UV_CREATE_PIPE | UV_INHERIT_FD |
                              UV_INHERIT_STREAM)) {
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
      if (container->flags & UV_INHERIT_FD) {
        fd = container->data.fd;
      } else {
        fd = container->data.stream->fd;
      }

      if (fd == -1) {
        errno = EINVAL;
        return -1;
      }

      fds[writable ? 1 : 0] = fd;

      return 0;
    default:
      assert(0 && "Unexpected flags");
      return -1;
  }
}


static int uv__process_stdio_flags(uv_stdio_container_t* container,
                                   int writable) {
  if (container->data.stream->type == UV_NAMED_PIPE &&
      ((uv_pipe_t*)container->data.stream)->ipc) {
    return UV_STREAM_READABLE | UV_STREAM_WRITABLE;
  } else if (writable) {
    return UV_STREAM_WRITABLE;
  } else {
    return UV_STREAM_READABLE;
  }
}


static int uv__process_open_stream(uv_stdio_container_t* container, int fds[2],
                                   int writable) {
  int fd = fds[writable ? 1 : 0];
  int child_fd = fds[writable ? 0 : 1];
  int flags;

  /* No need to create stream */
  if (!(container->flags & UV_CREATE_PIPE) || fd < 0) {
    return 0;
  }

  assert(child_fd >= 0);
  close(child_fd);

  uv__nonblock(fd, 1);
  flags = uv__process_stdio_flags(container, writable);

  return uv__stream_open((uv_stream_t*)container->data.stream, fd, flags);
}


static void uv__process_close_stream(uv_stdio_container_t* container) {
  if (!(container->flags & UV_CREATE_PIPE)) return;

  uv__stream_close((uv_stream_t*)container->data.stream);
}


static void uv__process_child_init(uv_process_options_t options,
                                   int stdio_count,
                                   int* pipes) {
  int fd;

  if (options.flags & UV_PROCESS_DETACHED) {
    setsid();
  }

  /* Dup fds */
  for (fd = 0; fd < stdio_count; fd++) {
    /*
     * stdin has swapped ends of pipe
     * (it's the only one readable stream)
     */
    int close_fd = fd == 0 ? pipes[fd * 2 + 1] : pipes[fd * 2];
    int use_fd = fd == 0 ? pipes[fd * 2] : pipes[fd * 2 + 1];

    if (use_fd >= 0) {
      close(close_fd);
    } else if (fd < 3) {
      /* `/dev/null` stdin, stdout, stderr even if they've flag UV_IGNORE */
      use_fd = open("/dev/null", fd == 0 ? O_RDONLY : O_RDWR);

      if (use_fd < 0) {
        perror("failed to open stdio");
        _exit(127);
      }
    } else {
      continue;
    }

    if (fd != use_fd) {
      dup2(use_fd, fd);
      close(use_fd);
    } else {
      uv__cloexec(use_fd, 0);
    }

    if (fd <= 2)
      uv__nonblock(fd, 0);
  }

  if (options.cwd && chdir(options.cwd)) {
    perror("chdir()");
    _exit(127);
  }

  if ((options.flags & UV_PROCESS_SETGID) && setgid(options.gid)) {
    perror("setgid()");
    _exit(127);
  }

  if ((options.flags & UV_PROCESS_SETUID) && setuid(options.uid)) {
    perror("setuid()");
    _exit(127);
  }

  environ = options.env;

  execvp(options.file, options.args);
  perror("execvp()");
  _exit(127);
}


#ifndef SPAWN_WAIT_EXEC
# define SPAWN_WAIT_EXEC 1
#endif

int uv_spawn(uv_loop_t* loop, uv_process_t* process,
    uv_process_options_t options) {
  /*
   * Save environ in the case that we get it clobbered
   * by the child process.
   */
  char** save_our_env = environ;

  int stdio_count = options.stdio_count < 3 ? 3 : options.stdio_count;
  int* pipes = malloc(2 * stdio_count * sizeof(int));

#if SPAWN_WAIT_EXEC
  int signal_pipe[2] = { -1, -1 };
  struct pollfd pfd;
#endif
  int status;
  pid_t pid;
  int i;

  if (pipes == NULL) {
    errno = ENOMEM;
    goto error;
  }

  assert(options.file != NULL);
  assert(!(options.flags & ~(UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS |
                             UV_PROCESS_DETACHED |
                             UV_PROCESS_SETGID |
                             UV_PROCESS_SETUID)));


  uv__handle_init(loop, (uv_handle_t*)process, UV_PROCESS);
  loop->counters.process_init++;
  uv__handle_start(process);

  process->exit_cb = options.exit_cb;

  /* Init pipe pairs */
  for (i = 0; i < stdio_count; i++) {
    pipes[i * 2] = -1;
    pipes[i * 2 + 1] = -1;
  }

  /* Create socketpairs/pipes, or use raw fd */
  for (i = 0; i < options.stdio_count; i++) {
    if (uv__process_init_stdio(&options.stdio[i], pipes + i * 2, i != 0)) {
      goto error;
    }
  }

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
   * the parent polls the read end until it sees POLLHUP.
   */
#if SPAWN_WAIT_EXEC
  if (uv__make_pipe(signal_pipe, UV__F_NONBLOCK))
    goto error;
#endif

  pid = fork();

  if (pid == -1) {
#if SPAWN_WAIT_EXEC
    close(signal_pipe[0]);
    close(signal_pipe[1]);
#endif
    environ = save_our_env;
    goto error;
  }

  if (pid == 0) {
    /* Child */
    uv__process_child_init(options, stdio_count, pipes);

    /* Execution never reaches here. */
  }

  /* Parent. */

  /* Restore environment. */
  environ = save_our_env;

#if SPAWN_WAIT_EXEC
  /* POLLHUP signals child has exited or execve()'d. */
  close(signal_pipe[1]);
  do {
    pfd.fd = signal_pipe[0];
    pfd.events = POLLIN|POLLHUP;
    pfd.revents = 0;
    errno = 0, status = poll(&pfd, 1, -1);
  }
  while (status == -1 && (errno == EINTR || errno == ENOMEM));

  assert((status == 1) && "poll() on pipe read end failed");
  close(signal_pipe[0]);
#endif

  process->pid = pid;

  ev_child_init(&process->child_watcher, uv__chld, pid, 0);
  ev_child_start(process->loop->ev, &process->child_watcher);
  process->child_watcher.data = process;

  for (i = 0; i < options.stdio_count; i++) {
    if (uv__process_open_stream(&options.stdio[i], pipes + i * 2, i == 0)) {
      int j;
      /* Close all opened streams */
      for (j = 0; j < i; j++) {
        uv__process_close_stream(&options.stdio[j]);
      }

      goto error;
    }
  }

  free(pipes);

  return 0;

error:
  uv__set_sys_error(process->loop, errno);

  for (i = 0; i < stdio_count; i++) {
    close(pipes[i * 2]);
    close(pipes[i * 2 + 1]);
  }

  free(pipes);

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
  ev_child_stop(handle->loop->ev, &handle->child_watcher);
  uv__handle_stop(handle);
}
