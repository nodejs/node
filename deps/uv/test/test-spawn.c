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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif


static int close_cb_called;
static int exit_cb_called;
static uv_process_t process;
static uv_timer_t timer;
static uv_process_options_t options;
static char exepath[1024];
static size_t exepath_size = 1024;
static char* args[3];
static int no_term_signal;

#define OUTPUT_SIZE 1024
static char output[OUTPUT_SIZE];
static int output_used;


static void close_cb(uv_handle_t* handle) {
  printf("close_cb\n");
  close_cb_called++;
}


static void exit_cb(uv_process_t* process, int exit_status, int term_signal) {
  printf("exit_cb\n");
  exit_cb_called++;
  ASSERT(exit_status == 1);
  ASSERT(term_signal == 0);
  uv_close((uv_handle_t*)process, close_cb);
}


static void exit_cb_failure_expected(uv_process_t* process, int exit_status,
    int term_signal) {
  printf("exit_cb\n");
  exit_cb_called++;
  ASSERT(exit_status == -1);
  ASSERT(term_signal == 0);
  uv_close((uv_handle_t*)process, close_cb);
}


static void kill_cb(uv_process_t* process, int exit_status, int term_signal) {
  uv_err_t err;

  printf("exit_cb\n");
  exit_cb_called++;
#ifdef _WIN32
  ASSERT(exit_status == 1);
#else
  ASSERT(exit_status == 0);
#endif
  ASSERT(no_term_signal || term_signal == 15);
  uv_close((uv_handle_t*)process, close_cb);

  /*
   * Sending signum == 0 should check if the
   * child process is still alive, not kill it.
   * This process should be dead.
   */
  err = uv_kill(process->pid, 0);
  ASSERT(err.code == UV_ESRCH);
}

static void detach_failure_cb(uv_process_t* process, int exit_status, int term_signal) {
  printf("detach_cb\n");
  exit_cb_called++;
}

static uv_buf_t on_alloc(uv_handle_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = output + output_used;
  buf.len = OUTPUT_SIZE - output_used;
  return buf;
}


static void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  uv_err_t err = uv_last_error(uv_default_loop());

  if (nread > 0) {
    output_used += nread;
  } else if (nread < 0) {
    ASSERT(err.code == UV_EOF);
    uv_close((uv_handle_t*)tcp, close_cb);
  }
}


static void write_cb(uv_write_t* req, int status) {
  ASSERT(status == 0);
  uv_close((uv_handle_t*)req->handle, close_cb);
}


static void init_process_options(char* test, uv_exit_cb exit_cb) {
  /* Note spawn_helper1 defined in test/run-tests.c */
  int r = uv_exepath(exepath, &exepath_size);
  ASSERT(r == 0);
  exepath[exepath_size] = '\0';
  args[0] = exepath;
  args[1] = test;
  args[2] = NULL;
  options.file = exepath;
  options.args = args;
  options.exit_cb = exit_cb;
  options.flags = 0;
}


static void timer_cb(uv_timer_t* handle, int status) {
  uv_process_kill(&process, /* SIGTERM */ 15);
  uv_close((uv_handle_t*)handle, close_cb);
}


TEST_IMPL(spawn_fails) {
  init_process_options("", exit_cb_failure_expected);
  options.file = options.args[0] = "program-that-had-better-not-exist";
  ASSERT(0 == uv_spawn(uv_default_loop(), &process, options));
  ASSERT(0 != uv_is_active((uv_handle_t*)&process));
  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT(uv_last_error(uv_default_loop()).code == UV_ENOENT);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_exit_code) {
  int r;

  init_process_options("spawn_helper1", exit_cb);

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_stdout) {
  int r;
  uv_pipe_t out;
  uv_stdio_container_t stdio[2];

  init_process_options("spawn_helper2", exit_cb);

  uv_pipe_init(uv_default_loop(), &out, 0);
  options.stdio = stdio;
  options.stdio[0].flags = UV_IGNORE;
  options.stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  options.stdio[1].data.stream = (uv_stream_t*)&out;
  options.stdio_count = 2;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process once for the pipe. */
  printf("output is: %s", output);
  ASSERT(strcmp("hello world\n", output) == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_stdout_to_file) {
  int r;
  uv_file file;
  uv_fs_t fs_req;
  uv_stdio_container_t stdio[2];

  /* Setup. */
  unlink("stdout_file");

  init_process_options("spawn_helper2", exit_cb);

  r = uv_fs_open(uv_default_loop(), &fs_req, "stdout_file", O_CREAT | O_RDWR,
      S_IREAD | S_IWRITE, NULL);
  ASSERT(r != -1);
  uv_fs_req_cleanup(&fs_req);

  file = r;

  options.stdio = stdio;
  options.stdio[0].flags = UV_IGNORE;
  options.stdio[1].flags = UV_INHERIT_FD;
  options.stdio[1].data.fd = file;
  options.stdio_count = 2;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 1);

  r = uv_fs_read(uv_default_loop(), &fs_req, file, output, sizeof(output),
      0, NULL);
  ASSERT(r == 12);
  uv_fs_req_cleanup(&fs_req);

  r = uv_fs_close(uv_default_loop(), &fs_req, file, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&fs_req);

  printf("output is: %s", output);
  ASSERT(strcmp("hello world\n", output) == 0);

  /* Cleanup. */
  unlink("stdout_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_stdin) {
  int r;
  uv_pipe_t out;
  uv_pipe_t in;
  uv_write_t write_req;
  uv_buf_t buf;
  uv_stdio_container_t stdio[2];
  char buffer[] = "hello-from-spawn_stdin";

  init_process_options("spawn_helper3", exit_cb);

  uv_pipe_init(uv_default_loop(), &out, 0);
  uv_pipe_init(uv_default_loop(), &in, 0);
  options.stdio = stdio;
  options.stdio[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
  options.stdio[0].data.stream = (uv_stream_t*)&in;
  options.stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  options.stdio[1].data.stream = (uv_stream_t*)&out;
  options.stdio_count = 2;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  buf.base = buffer;
  buf.len = sizeof(buffer);
  r = uv_write(&write_req, (uv_stream_t*)&in, &buf, 1, write_cb);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 3); /* Once for process twice for the pipe. */
  ASSERT(strcmp(buffer, output) == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_stdio_greater_than_3) {
  int r;
  uv_pipe_t pipe;
  uv_stdio_container_t stdio[4];

  init_process_options("spawn_helper5", exit_cb);

  uv_pipe_init(uv_default_loop(), &pipe, 0);
  options.stdio = stdio;
  options.stdio[0].flags = UV_IGNORE;
  options.stdio[1].flags = UV_IGNORE;
  options.stdio[2].flags = UV_IGNORE;
  options.stdio[3].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  options.stdio[3].data.stream = (uv_stream_t*)&pipe;
  options.stdio_count = 4;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &pipe, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process once for the pipe. */
  printf("output from stdio[3] is: %s", output);
  ASSERT(strcmp("fourth stdio!\n", output) == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_ignored_stdio) {
  int r;

  init_process_options("spawn_helper6", exit_cb);

  options.stdio = NULL;
  options.stdio_count = 0;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_and_kill) {
  int r;

  init_process_options("spawn_helper4", kill_cb);

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_timer_init(uv_default_loop(), &timer);
  ASSERT(r == 0);

  r = uv_timer_start(&timer, timer_cb, 500, 0);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process and once for timer. */

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_preserve_env) {
  int r;
  uv_pipe_t out;
  uv_stdio_container_t stdio[2];

  init_process_options("spawn_helper7", exit_cb);

  uv_pipe_init(uv_default_loop(), &out, 0);
  options.stdio = stdio;
  options.stdio[0].flags = UV_IGNORE;
  options.stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  options.stdio[1].data.stream = (uv_stream_t*) &out;
  options.stdio_count = 2;

  r = putenv("ENV_TEST=testval");
  ASSERT(r == 0);

  /* Explicitly set options.env to NULL to test for env clobbering. */
  options.env = NULL;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2);

  printf("output is: %s", output);
  ASSERT(strcmp("testval", output) == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_detached) {
  int r;
  uv_err_t err;

  init_process_options("spawn_helper4", detach_failure_cb);

  options.flags |= UV_PROCESS_DETACHED;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  uv_unref((uv_handle_t*)&process);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 0);

  err = uv_kill(process.pid, 0);
  ASSERT(err.code == 0);

  err = uv_kill(process.pid, 15);
  ASSERT(err.code == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(spawn_and_kill_with_std) {
  int r;
  uv_pipe_t in, out, err;
  uv_write_t write;
  char message[] = "Nancy's joining me because the message this evening is "
                   "not my message but ours.";
  uv_buf_t buf;
  uv_stdio_container_t stdio[3];

  init_process_options("spawn_helper4", kill_cb);

  r = uv_pipe_init(uv_default_loop(), &in, 0);
  ASSERT(r == 0);

  r = uv_pipe_init(uv_default_loop(), &out, 0);
  ASSERT(r == 0);

  r = uv_pipe_init(uv_default_loop(), &err, 0);
  ASSERT(r == 0);

  options.stdio = stdio;
  options.stdio[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
  options.stdio[0].data.stream = (uv_stream_t*)&in;
  options.stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  options.stdio[1].data.stream = (uv_stream_t*)&out;
  options.stdio[2].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  options.stdio[2].data.stream = (uv_stream_t*)&err;
  options.stdio_count = 3;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  buf = uv_buf_init(message, sizeof message);
  r = uv_write(&write, (uv_stream_t*) &in, &buf, 1, write_cb);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &err, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_timer_init(uv_default_loop(), &timer);
  ASSERT(r == 0);

  r = uv_timer_start(&timer, timer_cb, 500, 0);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 5); /* process x 1, timer x 1, stdio x 3. */

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_and_ping) {
  uv_write_t write_req;
  uv_pipe_t in, out;
  uv_buf_t buf;
  uv_stdio_container_t stdio[2];
  int r;

  init_process_options("spawn_helper3", exit_cb);
  buf = uv_buf_init("TEST", 4);

  uv_pipe_init(uv_default_loop(), &out, 0);
  uv_pipe_init(uv_default_loop(), &in, 0);
  options.stdio = stdio;
  options.stdio[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
  options.stdio[0].data.stream = (uv_stream_t*)&in;
  options.stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  options.stdio[1].data.stream = (uv_stream_t*)&out;
  options.stdio_count = 2;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  /* Sending signum == 0 should check if the
   * child process is still alive, not kill it.
   */
  r = uv_process_kill(&process, 0);
  ASSERT(r == 0);

  r = uv_write(&write_req, (uv_stream_t*)&in, &buf, 1, write_cb);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*)&out, on_alloc, on_read);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(strcmp(output, "TEST") == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(kill) {
  int r;
  uv_err_t err;

#ifdef _WIN32
  no_term_signal = 1;
#endif

  init_process_options("spawn_helper4", kill_cb);

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  /* Sending signum == 0 should check if the
   * child process is still alive, not kill it.
   */
  err = uv_kill(process.pid, 0);
  ASSERT(err.code == UV_OK);

  /* Kill the process. */
  err = uv_kill(process.pid, /* SIGTERM */ 15);
  ASSERT(err.code == UV_OK);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


#ifdef _WIN32
TEST_IMPL(spawn_detect_pipe_name_collisions_on_windows) {
  int r;
  uv_pipe_t out;
  char name[64];
  HANDLE pipe_handle;
  uv_stdio_container_t stdio[2];

  init_process_options("spawn_helper2", exit_cb);

  uv_pipe_init(uv_default_loop(), &out, 0);
  options.stdio = stdio;
  options.stdio[0].flags = UV_IGNORE;
  options.stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  options.stdio[1].data.stream = (uv_stream_t*)&out;
  options.stdio_count = 2;

  /* Create a pipe that'll cause a collision. */
  _snprintf(name, sizeof(name), "\\\\.\\pipe\\uv\\%p-%d", &out, GetCurrentProcessId());
  pipe_handle = CreateNamedPipeA(name,
                                PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                10,
                                65536,
                                65536,
                                0,
                                NULL);
  ASSERT(pipe_handle != INVALID_HANDLE_VALUE);

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process once for the pipe. */
  printf("output is: %s", output);
  ASSERT(strcmp("hello world\n", output) == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


uv_err_t make_program_args(char** args, int verbatim_arguments, WCHAR** dst_ptr);
WCHAR* quote_cmd_arg(const WCHAR *source, WCHAR *target);

TEST_IMPL(argument_escaping) {
  const WCHAR* test_str[] = {
    L"HelloWorld",
    L"Hello World",
    L"Hello\"World",
    L"Hello World\\",
    L"Hello\\\"World",
    L"Hello\\World",
    L"Hello\\\\World",
    L"Hello World\\",
    L"c:\\path\\to\\node.exe --eval \"require('c:\\\\path\\\\to\\\\test.js')\""
  };
  const int count = sizeof(test_str) / sizeof(*test_str);
  WCHAR** test_output;
  WCHAR* command_line;
  WCHAR** cracked;
  size_t total_size = 0;
  int i;
  int num_args;
  uv_err_t result;

  char* verbatim[] = {
    "cmd.exe",
    "/c",
    "c:\\path\\to\\node.exe --eval \"require('c:\\\\path\\\\to\\\\test.js')\"",
    NULL
  };
  WCHAR* verbatim_output;
  WCHAR* non_verbatim_output;

  test_output = calloc(count, sizeof(WCHAR*));
  for (i = 0; i < count; ++i) {
    test_output[i] = calloc(2 * (wcslen(test_str[i]) + 2), sizeof(WCHAR));
    quote_cmd_arg(test_str[i], test_output[i]);
    wprintf(L"input : %s\n", test_str[i]);
    wprintf(L"output: %s\n", test_output[i]);
    total_size += wcslen(test_output[i]) + 1;
  }
  command_line = calloc(total_size + 1, sizeof(WCHAR));
  for (i = 0; i < count; ++i) {
    wcscat(command_line, test_output[i]);
    wcscat(command_line, L" ");
  }
  command_line[total_size - 1] = L'\0';

  wprintf(L"command_line: %s\n", command_line);

  cracked = CommandLineToArgvW(command_line, &num_args);
  for (i = 0; i < num_args; ++i) {
    wprintf(L"%d: %s\t%s\n", i, test_str[i], cracked[i]);
    ASSERT(wcscmp(test_str[i], cracked[i]) == 0);
  }

  LocalFree(cracked);
  for (i = 0; i < count; ++i) {
    free(test_output[i]);
  }

  result = make_program_args(verbatim, 1, &verbatim_output);
  ASSERT(result.code == UV_OK);
  result = make_program_args(verbatim, 0, &non_verbatim_output);
  ASSERT(result.code == UV_OK);

  wprintf(L"    verbatim_output: %s\n", verbatim_output);
  wprintf(L"non_verbatim_output: %s\n", non_verbatim_output);

  ASSERT(wcscmp(verbatim_output, L"cmd.exe /c c:\\path\\to\\node.exe --eval \"require('c:\\\\path\\\\to\\\\test.js')\"") == 0);
  ASSERT(wcscmp(non_verbatim_output, L"cmd.exe /c \"c:\\path\\to\\node.exe --eval \\\"require('c:\\\\path\\\\to\\\\test.js')\\\"\"") == 0);

  free(verbatim_output);
  free(non_verbatim_output);

  return 0;
}

uv_err_t make_program_env(char** env_block, WCHAR** dst_ptr);

TEST_IMPL(environment_creation) {
  int i;
  char* environment[] = {
    "FOO=BAR",
    "SYSTEM=ROOT", /* substring of a supplied var name */
    "SYSTEMROOTED=OMG", /* supplied var name is a substring */
    "TEMP=C:\\Temp",
    "BAZ=QUX",
    NULL
  };

  WCHAR expected[512];
  WCHAR* ptr = expected;
  uv_err_t result;
  WCHAR* str;
  WCHAR* env;

  for (i = 0; i < sizeof(environment) / sizeof(environment[0]) - 1; i++) {
    ptr += uv_utf8_to_utf16(environment[i], ptr, expected + sizeof(expected) - ptr);
  }

  memcpy(ptr, L"SYSTEMROOT=", sizeof(L"SYSTEMROOT="));
  ptr += sizeof(L"SYSTEMROOT=")/sizeof(WCHAR) - 1;
  ptr += GetEnvironmentVariableW(L"SYSTEMROOT", ptr, expected + sizeof(expected) - ptr);
  ++ptr;

  memcpy(ptr, L"SYSTEMDRIVE=", sizeof(L"SYSTEMDRIVE="));
  ptr += sizeof(L"SYSTEMDRIVE=")/sizeof(WCHAR) - 1;
  ptr += GetEnvironmentVariableW(L"SYSTEMDRIVE", ptr, expected + sizeof(expected) - ptr);
  ++ptr;
  *ptr = '\0';

  result = make_program_env(environment, &env);
  ASSERT(result.code == UV_OK);

  for (str = env; *str; str += wcslen(str) + 1) {
    wprintf(L"%s\n", str);
  }

  ASSERT(wcscmp(expected, env) == 0);

  return 0;
}
#endif

#ifndef _WIN32
TEST_IMPL(spawn_setuid_setgid) {
  int r;

  /* if not root, then this will fail. */
  uv_uid_t uid = getuid();
  if (uid != 0) {
    fprintf(stderr, "spawn_setuid_setgid skipped: not root\n");
    return 0;
  }

  init_process_options("spawn_helper1", exit_cb);

  /* become the "nobody" user. */
  struct passwd* pw;
  pw = getpwnam("nobody");
  ASSERT(pw != NULL);
  options.uid = pw->pw_uid;
  options.gid = pw->pw_gid;
  options.flags = UV_PROCESS_SETUID | UV_PROCESS_SETGID;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif


#ifndef _WIN32
TEST_IMPL(spawn_setuid_fails) {
  int r;

  /* if root, become nobody. */
  uv_uid_t uid = getuid();
  if (uid == 0) {
    struct passwd* pw;
    pw = getpwnam("nobody");
    ASSERT(pw != NULL);
    r = setuid(pw->pw_uid);
    ASSERT(r == 0);
  }

  init_process_options("spawn_helper1", exit_cb_failure_expected);

  options.flags |= UV_PROCESS_SETUID;
  options.uid = (uv_uid_t) -42424242;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_setgid_fails) {
  int r;

  /* if root, become nobody. */
  uv_uid_t uid = getuid();
  if (uid == 0) {
    struct passwd* pw;
    pw = getpwnam("nobody");
    ASSERT(pw != NULL);
    r = setuid(pw->pw_uid);
    ASSERT(r == 0);
  }

  init_process_options("spawn_helper1", exit_cb_failure_expected);

  options.flags |= UV_PROCESS_SETGID;
  options.gid = (uv_gid_t) -42424242;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif


#ifdef _WIN32

static void exit_cb_unexpected(uv_process_t* process,
                               int exit_status,
                               int term_signal) {
  ASSERT(0 && "should not have been called");
}


TEST_IMPL(spawn_setuid_fails) {
  int r;

  init_process_options("spawn_helper1", exit_cb_unexpected);

  options.flags |= UV_PROCESS_SETUID;
  options.uid = (uv_uid_t) -42424242;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == -1);
  ASSERT(uv_last_error(uv_default_loop()).code == UV_ENOTSUP);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(spawn_setgid_fails) {
  int r;

  init_process_options("spawn_helper1", exit_cb_unexpected);

  options.flags |= UV_PROCESS_SETGID;
  options.gid = (uv_gid_t) -42424242;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == -1);
  ASSERT(uv_last_error(uv_default_loop()).code == UV_ENOTSUP);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif


TEST_IMPL(spawn_auto_unref) {
  init_process_options("spawn_helper1", NULL);
  ASSERT(0 == uv_spawn(uv_default_loop(), &process, options));
  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT(0 == uv_is_closing((uv_handle_t*) &process));
  uv_close((uv_handle_t*) &process, NULL);
  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT(0 != uv_is_closing((uv_handle_t*) &process));
  MAKE_VALGRIND_HAPPY();
  return 0;
}
