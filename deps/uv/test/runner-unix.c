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

#include "runner-unix.h"
#include "runner.h"

#include <limits.h>
#include <stdint.h> /* uintptr_t */

#include <errno.h>
#include <unistd.h> /* usleep */
#include <string.h> /* strdup */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <assert.h>

#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

extern char** environ;

static void closefd(int fd) {
  if (close(fd) == 0 || errno == EINTR || errno == EINPROGRESS)
    return;

  perror("close");
  abort();
}


void notify_parent_process(void) {
  char* arg;
  int fd;

  arg = getenv("UV_TEST_RUNNER_FD");
  if (arg == NULL)
    return;

  fd = atoi(arg);
  assert(fd > STDERR_FILENO);
  unsetenv("UV_TEST_RUNNER_FD");
  closefd(fd);
}


/* Do platform-specific initialization. */
void platform_init(int argc, char **argv) {
  /* Disable stdio output buffering. */
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  signal(SIGPIPE, SIG_IGN);
  snprintf(executable_path, sizeof(executable_path), "%s", argv[0]);
}


/* Invoke "argv[0] test-name [test-part]". Store process info in *p. Make sure
 * that all stdio output of the processes is buffered up. */
int process_start(char* name, char* part, process_info_t* p, int is_helper) {
  FILE* stdout_file;
  int stdout_fd;
  const char* arg;
  char* args[16];
  int pipefd[2];
  char fdstr[8];
  ssize_t rc;
  int n;
  pid_t pid;

  arg = getenv("UV_USE_VALGRIND");
  n = 0;

  /* Disable valgrind for helpers, it complains about helpers leaking memory.
   * They're killed after the test and as such never get a chance to clean up.
   */
  if (is_helper == 0 && arg != NULL && atoi(arg) != 0) {
    args[n++] = "valgrind";
    args[n++] = "--quiet";
    args[n++] = "--leak-check=full";
    args[n++] = "--show-reachable=yes";
    args[n++] = "--error-exitcode=125";
  }

  args[n++] = executable_path;
  args[n++] = name;
  args[n++] = part;
  args[n++] = NULL;

  stdout_file = tmpfile();
  stdout_fd = fileno(stdout_file);
  if (!stdout_file) {
    perror("tmpfile");
    return -1;
  }

  if (is_helper) {
    if (pipe(pipefd)) {
      perror("pipe");
      return -1;
    }

    snprintf(fdstr, sizeof(fdstr), "%d", pipefd[1]);
    if (setenv("UV_TEST_RUNNER_FD", fdstr, /* overwrite */ 1)) {
      perror("setenv");
      return -1;
    }
  }

  p->terminated = 0;
  p->status = 0;

#if defined(__APPLE__) && (TARGET_OS_TV || TARGET_OS_WATCH)
  pid = -1;
#else
  pid = fork();
#endif

  if (pid < 0) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    /* child */
    if (is_helper)
      closefd(pipefd[0]);
    dup2(stdout_fd, STDOUT_FILENO);
    dup2(stdout_fd, STDERR_FILENO);
#if !(defined(__APPLE__) && (TARGET_OS_TV || TARGET_OS_WATCH))
    execve(args[0], args, environ);
#endif
    perror("execve()");
    _exit(127);
  }

  /* parent */
  p->pid = pid;
  p->name = strdup(name);
  p->stdout_file = stdout_file;

  if (!is_helper)
    return 0;

  closefd(pipefd[1]);
  unsetenv("UV_TEST_RUNNER_FD");

  do
    rc = read(pipefd[0], &n, 1);
  while (rc == -1 && errno == EINTR);

  closefd(pipefd[0]);

  if (rc == -1) {
    perror("read");
    return -1;
  }

  if (rc > 0) {
    fprintf(stderr, "EOF expected but got data.\n");
    return -1;
  }

  return 0;
}


typedef struct {
  int pipe[2];
  process_info_t* vec;
  int n;
} dowait_args;


/* This function is run inside a pthread. We do this so that we can possibly
 * timeout.
 */
static void* dowait(void* data) {
  dowait_args* args = data;

  int i, r;
  process_info_t* p;

  for (i = 0; i < args->n; i++) {
    p = &args->vec[i];
    if (p->terminated) continue;
    r = waitpid(p->pid, &p->status, 0);
    if (r < 0) {
      perror("waitpid");
      return NULL;
    }
    p->terminated = 1;
  }

  if (args->pipe[1] >= 0) {
    /* Write a character to the main thread to notify it about this. */
    ssize_t r;

    do
      r = write(args->pipe[1], "", 1);
    while (r == -1 && errno == EINTR);
  }

  return NULL;
}


/* Wait for all `n` processes in `vec` to terminate. Time out after `timeout`
 * msec, or never if timeout == -1. Return 0 if all processes are terminated,
 * -1 on error, -2 on timeout. */
int process_wait(process_info_t* vec, int n, int timeout) {
  int i;
  int r;
  int retval;
  process_info_t* p;
  dowait_args args;
  pthread_t tid;
  pthread_attr_t attr;
  unsigned int elapsed_ms;
  struct timeval timebase;
  struct timeval tv;
  fd_set fds;

  args.vec = vec;
  args.n = n;
  args.pipe[0] = -1;
  args.pipe[1] = -1;

  /* The simple case is where there is no timeout */
  if (timeout == -1) {
    dowait(&args);
    return 0;
  }

  /* Hard case. Do the wait with a timeout.
   *
   * Assumption: we are the only ones making this call right now. Otherwise
   * we'd need to lock vec.
   */

  r = pipe((int*)&(args.pipe));
  if (r) {
    perror("pipe()");
    return -1;
  }

  if (pthread_attr_init(&attr))
    abort();

#if defined(__MVS__)
  if (pthread_attr_setstacksize(&attr, 1024 * 1024))
#else
  if (pthread_attr_setstacksize(&attr, 256 * 1024))
#endif
    abort();

  r = pthread_create(&tid, &attr, dowait, &args);

  if (pthread_attr_destroy(&attr))
    abort();

  if (r) {
    perror("pthread_create()");
    retval = -1;
    goto terminate;
  }

  if (gettimeofday(&timebase, NULL))
    abort();

  tv = timebase;
  for (;;) {
    /* Check that gettimeofday() doesn't jump back in time. */
    assert(tv.tv_sec > timebase.tv_sec ||
           (tv.tv_sec == timebase.tv_sec && tv.tv_usec >= timebase.tv_usec));

    elapsed_ms =
        (tv.tv_sec - timebase.tv_sec) * 1000 +
        (tv.tv_usec / 1000) -
        (timebase.tv_usec / 1000);

    r = 0;  /* Timeout. */
    if (elapsed_ms >= (unsigned) timeout)
      break;

    tv.tv_sec = (timeout - elapsed_ms) / 1000;
    tv.tv_usec = (timeout - elapsed_ms) % 1000 * 1000;

    FD_ZERO(&fds);
    FD_SET(args.pipe[0], &fds);

    r = select(args.pipe[0] + 1, &fds, NULL, NULL, &tv);
    if (!(r == -1 && errno == EINTR))
      break;

    if (gettimeofday(&tv, NULL))
      abort();
  }

  if (r == -1) {
    perror("select()");
    retval = -1;

  } else if (r) {
    /* The thread completed successfully. */
    retval = 0;

  } else {
    /* Timeout. Kill all the children. */
    for (i = 0; i < n; i++) {
      p = &vec[i];
      kill(p->pid, SIGTERM);
    }
    retval = -2;
  }

  if (pthread_join(tid, NULL))
    abort();

terminate:
  closefd(args.pipe[0]);
  closefd(args.pipe[1]);
  return retval;
}


/* Returns the number of bytes in the stdio output buffer for process `p`. */
long int process_output_size(process_info_t *p) {
  /* Size of the p->stdout_file */
  struct stat buf;

  memset(&buf, 0, sizeof(buf));
  int r = fstat(fileno(p->stdout_file), &buf);
  if (r < 0) {
    return -1;
  }

  return (long)buf.st_size;
}


/* Copy the contents of the stdio output buffer to `fd`. */
int process_copy_output(process_info_t* p, FILE* stream) {
  char buf[1024];
  int partial;
  int r;

  r = fseek(p->stdout_file, 0, SEEK_SET);
  if (r < 0) {
    perror("fseek");
    return -1;
  }

  partial = 0;
  while ((r = fread(buf, 1, sizeof(buf), p->stdout_file)) != 0)
    partial = print_lines(buf, r, stream, partial);

  if (ferror(p->stdout_file)) {
    perror("read");
    return -1;
  }

  return 0;
}


/* Copy the last line of the stdio output buffer to `buffer` */
int process_read_last_line(process_info_t *p,
                           char* buffer,
                           size_t buffer_len) {
  char* ptr;

  int r = fseek(p->stdout_file, 0, SEEK_SET);
  if (r < 0) {
    perror("fseek");
    return -1;
  }

  buffer[0] = '\0';

  while (fgets(buffer, buffer_len, p->stdout_file) != NULL) {
    for (ptr = buffer; *ptr && *ptr != '\r' && *ptr != '\n'; ptr++)
      ;
    *ptr = '\0';
  }

  if (ferror(p->stdout_file)) {
    perror("read");
    buffer[0] = '\0';
    return -1;
  }
  return 0;
}


/* Return the name that was specified when `p` was started by process_start */
char* process_get_name(process_info_t *p) {
  return p->name;
}


/* Terminate process `p`. */
int process_terminate(process_info_t *p) {
  return kill(p->pid, SIGTERM);
}


/* Return the exit code of process p. On error, return -1. */
int process_reap(process_info_t *p) {
  if (WIFEXITED(p->status)) {
    return WEXITSTATUS(p->status);
  } else  {
    return p->status; /* ? */
  }
}


/* Clean up after terminating process `p` (e.g. free the output buffer etc.). */
void process_cleanup(process_info_t *p) {
  fclose(p->stdout_file);
  free(p->name);
}


/* Move the console cursor one line up and back to the first column. */
void rewind_cursor(void) {
#if defined(__MVS__)
  fprintf(stderr, "\047[2K\r");
#else
  fprintf(stderr, "\033[2K\r");
#endif
}
