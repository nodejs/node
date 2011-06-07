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

#include <stdint.h> /* uintptr_t */

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
#include <pthread.h>

#ifdef __APPLE__
#include <mach-o/dyld.h> /* _NSGetExecutablePath */

static void get_executable_path() {
  uint32_t bufsize = sizeof(executable_path);
  _NSGetExecutablePath(executable_path, &bufsize);
}
#endif

#ifdef __linux__
static void get_executable_path() {
  if (!executable_path[0]) {
    readlink("/proc/self/exe", executable_path, PATHMAX - 1);
  }
}
#endif


/* Do platform-specific initialization. */
void platform_init(int argc, char **argv) {
  /* Disable stdio output buffering. */
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
#ifdef get_executable_path
  get_executable_path();
#else
  strcpy(executable_path, argv[0]);
#endif
}


/* Invoke "arv[0] test-name". Store process info in *p. */
/* Make sure that all stdio output of the processes is buffered up. */
int process_start(char* name, process_info_t* p) {
  FILE* stdout_file = tmpfile();
  if (!stdout_file) {
    perror("tmpfile");
    return -1;
  }

  p->terminated = 0;
  p->status = 0;

  pid_t pid = fork();

  if (pid < 0) {
    perror("vfork");
    return -1;
  }

  if (pid == 0) {
    /* child */
    dup2(fileno(stdout_file), STDOUT_FILENO);
    dup2(fileno(stdout_file), STDERR_FILENO);

    char* args[3] = { executable_path, name, NULL };
    execvp(executable_path, args);
    perror("execvp()");
    _exit(127);
  }

  /* parent */
  p->pid = pid;
  p->name = strdup(name);
  p->stdout_file = stdout_file;

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

  int i, status, r;
  process_info_t* p;

  for (i = 0; i < args->n; i++) {
    p = (process_info_t*)(args->vec + i * sizeof(process_info_t));
    if (p->terminated) continue;
    status = 0;
    r = waitpid(p->pid, &p->status, 0);
    if (r < 0) {
      perror("waitpid");
      return NULL;
    }
    p->terminated = 1;
  }

  if (args->pipe[1] >= 0) {
    /* Write a character to the main thread to notify it about this. */
    char c = 0;
    write(args->pipe[1], &c, 1);
  }

  return NULL;
}


/* Wait for all `n` processes in `vec` to terminate. */
/* Time out after `timeout` msec, or never if timeout == -1 */
/* Return 0 if all processes are terminated, -1 on error, -2 on timeout. */
int process_wait(process_info_t* vec, int n, int timeout) {
  int i;
  process_info_t* p;
  dowait_args args;
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

  pthread_t tid;
  int retval;

  int r = pipe((int*)&(args.pipe));
  if (r) {
    perror("pipe()");
    return -1;
  }

  r = pthread_create(&tid, NULL, dowait, &args);
  if (r) {
    perror("pthread_create()");
    retval = -1;
    goto terminate;
  }

  struct timeval tv;
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = 0;

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(args.pipe[0], &fds);

  r = select(args.pipe[0] + 1, &fds, NULL, NULL, &tv);

  if (r == -1) {
    perror("select()");
    retval = -1;

  } else if (r) {
    /* The thread completed successfully. */
    retval = 0;

  } else {
    /* Timeout. Kill all the children. */
    for (i = 0; i < n; i++) {
      p = (process_info_t*)(vec + i * sizeof(process_info_t));
      kill(p->pid, SIGTERM);
    }
    retval = -2;

    /* Wait for thread to finish. */
    r = pthread_join(tid, NULL);
    if (r) {
      perror("pthread_join");
      retval = -1;
    }
  }

terminate:
  close(args.pipe[0]);
  close(args.pipe[1]);
  return retval;
}


/* Returns the number of bytes in the stdio output buffer for process `p`. */
long int process_output_size(process_info_t *p) {
  /* Size of the p->stdout_file */
  struct stat buf;

  int r = fstat(fileno(p->stdout_file), &buf);
  if (r < 0) {
    return -1;
  }

  return (long)buf.st_size;
}


/* Copy the contents of the stdio output buffer to `fd`. */
int process_copy_output(process_info_t *p, int fd) {
  int r = fseek(p->stdout_file, 0, SEEK_SET);
  if (r < 0) {
    perror("fseek");
    return -1;
  }

  size_t nread, nwritten;
  char buf[1024];

  while ((nread = read(fileno(p->stdout_file), buf, 1024)) > 0) {
    nwritten = write(fd, buf, nread);
    /* TODO: what if write doesn't write the whole buffer... */
    if (nwritten < 0) {
      perror("write");
      return -1;
    }
  }

  if (nread < 0) {
    perror("read");
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


/* Return the exit code of process p. */
/* On error, return -1. */
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
void rewind_cursor() {
  fprintf(stderr, "\033[2K\r");
}


typedef void* (*uv_thread_cb)(void* arg);


uintptr_t uv_create_thread(void (*entry)(void* arg), void* arg) {
  pthread_t t;
  uv_thread_cb cb = (uv_thread_cb)entry;
  int r = pthread_create(&t, NULL, cb, arg);

  if (r) {
    return 0;
  }

  return (uintptr_t)t;
}


/* Wait for a thread to terminate. Should return 0 if the thread ended, -1 on
 * error.
 */
int uv_wait_thread(uintptr_t thread_id) {
  return pthread_join((pthread_t)thread_id, NULL);
}


/* Pause the calling thread for a number of milliseconds. */
void uv_sleep(int msec) {
  usleep(msec);
}
