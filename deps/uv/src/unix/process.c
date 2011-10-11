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
#include <fcntl.h> /* O_CLOEXEC, O_NONBLOCK */
#include <poll.h>
#include <unistd.h>
#include <stdio.h>

#ifdef __APPLE__
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


/*
 * Used for initializing stdio streams like options.stdin_stream. Returns
 * zero on success.
 */
static int uv__process_init_pipe(uv_pipe_t* handle, int fds[2]) {
  if (handle->type != UV_NAMED_PIPE) {
    errno = EINVAL;
    return -1;
  }

  if (handle->ipc) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
      return -1;
    }
  } else {
    if (pipe(fds) < 0) {
      return -1;
    }
  }

  uv__cloexec(fds[0], 1);
  uv__cloexec(fds[1], 1);

  return 0;
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
  int stdin_pipe[2] = { -1, -1 };
  int stdout_pipe[2] = { -1, -1 };
  int stderr_pipe[2] = { -1, -1 };
#if SPAWN_WAIT_EXEC
  int signal_pipe[2] = { -1, -1 };
  struct pollfd pfd;
#endif
  int status;
  pid_t pid;
  int flags;

  uv__handle_init(loop, (uv_handle_t*)process, UV_PROCESS);
  loop->counters.process_init++;

  process->exit_cb = options.exit_cb;

  if (options.stdin_stream &&
      uv__process_init_pipe(options.stdin_stream, stdin_pipe)) {
    goto error;
  }

  if (options.stdout_stream &&
      uv__process_init_pipe(options.stdout_stream, stdout_pipe)) {
    goto error;
  }

  if (options.stderr_stream &&
      uv__process_init_pipe(options.stderr_stream, stderr_pipe)) {
    goto error;
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
# ifdef HAVE_PIPE2
  if (pipe2(signal_pipe, O_CLOEXEC | O_NONBLOCK) < 0) {
    goto error;
  }
# else
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, signal_pipe) < 0) {
    goto error;
  }
  uv__cloexec(signal_pipe[0], 1);
  uv__cloexec(signal_pipe[1], 1);
  uv__nonblock(signal_pipe[0], 1);
  uv__nonblock(signal_pipe[1], 1);
# endif
#endif

  pid = fork();

  if (pid == -1) {
#if SPAWN_WAIT_EXEC
    uv__close(signal_pipe[0]);
    uv__close(signal_pipe[1]);
#endif
    environ = save_our_env;
    goto error;
  }

  if (pid == 0) {
    if (stdin_pipe[0] >= 0) {
      uv__close(stdin_pipe[1]);
      dup2(stdin_pipe[0],  STDIN_FILENO);
    } else {
      /* Reset flags that might be set by Node */
      uv__cloexec(STDIN_FILENO, 0);
      uv__nonblock(STDIN_FILENO, 0);
    }

    if (stdout_pipe[1] >= 0) {
      uv__close(stdout_pipe[0]);
      dup2(stdout_pipe[1], STDOUT_FILENO);
    } else {
      /* Reset flags that might be set by Node */
      uv__cloexec(STDOUT_FILENO, 0);
      uv__nonblock(STDOUT_FILENO, 0);
    }

    if (stderr_pipe[1] >= 0) {
      uv__close(stderr_pipe[0]);
      dup2(stderr_pipe[1], STDERR_FILENO);
    } else {
      /* Reset flags that might be set by Node */
      uv__cloexec(STDERR_FILENO, 0);
      uv__nonblock(STDERR_FILENO, 0);
    }

    if (options.cwd && chdir(options.cwd)) {
      perror("chdir()");
      _exit(127);
    }

    environ = options.env;

    execvp(options.file, options.args);
    perror("execvp()");
    _exit(127);
    /* Execution never reaches here. */
  }

  /* Parent. */

  /* Restore environment. */
  environ = save_our_env;

#if SPAWN_WAIT_EXEC
  /* POLLHUP signals child has exited or execve()'d. */
  uv__close(signal_pipe[1]);
  do {
    pfd.fd = signal_pipe[0];
    pfd.events = POLLIN|POLLHUP;
    pfd.revents = 0;
    errno = 0, status = poll(&pfd, 1, -1);
  }
  while (status == -1 && (errno == EINTR || errno == ENOMEM));

  assert((status == 1) && "poll() on pipe read end failed");
  uv__close(signal_pipe[0]);
#endif

  process->pid = pid;

  ev_child_init(&process->child_watcher, uv__chld, pid, 0);
  ev_child_start(process->loop->ev, &process->child_watcher);
  process->child_watcher.data = process;

  if (stdin_pipe[1] >= 0) {
    assert(options.stdin_stream);
    assert(stdin_pipe[0] >= 0);
    uv__close(stdin_pipe[0]);
    uv__nonblock(stdin_pipe[1], 1);
    flags = UV_WRITABLE | (options.stdin_stream->ipc ? UV_READABLE : 0);
    uv__stream_open((uv_stream_t*)options.stdin_stream, stdin_pipe[1],
        flags);
  }

  if (stdout_pipe[0] >= 0) {
    assert(options.stdout_stream);
    assert(stdout_pipe[1] >= 0);
    uv__close(stdout_pipe[1]);
    uv__nonblock(stdout_pipe[0], 1);
    flags = UV_READABLE | (options.stdout_stream->ipc ? UV_WRITABLE : 0);
    uv__stream_open((uv_stream_t*)options.stdout_stream, stdout_pipe[0],
        flags);
  }

  if (stderr_pipe[0] >= 0) {
    assert(options.stderr_stream);
    assert(stderr_pipe[1] >= 0);
    uv__close(stderr_pipe[1]);
    uv__nonblock(stderr_pipe[0], 1);
    flags = UV_READABLE | (options.stderr_stream->ipc ? UV_WRITABLE : 0);
    uv__stream_open((uv_stream_t*)options.stderr_stream, stderr_pipe[0],
        flags);
  }

  return 0;

error:
  uv__set_sys_error(process->loop, errno);
  uv__close(stdin_pipe[0]);
  uv__close(stdin_pipe[1]);
  uv__close(stdout_pipe[0]);
  uv__close(stdout_pipe[1]);
  uv__close(stderr_pipe[0]);
  uv__close(stderr_pipe[1]);
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
