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
#include <string.h>

static int close_cb_called;
static int exit_cb_called;
static uv_process_t process;
static uv_timer_t timer;
static uv_process_options_t options;
static char exepath[1024];
static size_t exepath_size = 1024;
static char* args[3];

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


static void kill_cb(uv_process_t* process, int exit_status, int term_signal) {
  printf("exit_cb\n");
  exit_cb_called++;
#ifdef _WIN32
  ASSERT(exit_status == 1);
#else
  ASSERT(exit_status == 0);
#endif
  ASSERT(term_signal == 15);
  uv_close((uv_handle_t*)process, close_cb);
}


uv_buf_t on_alloc(uv_handle_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = output + output_used;
  buf.len = OUTPUT_SIZE - output_used;
  return buf;
}


void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  uv_err_t err = uv_last_error(uv_default_loop());

  if (nread > 0) {
    output_used += nread;
  } else if (nread < 0) {
    if (err.code == UV_EOF) {
      uv_close((uv_handle_t*)tcp, close_cb);
    }
  }
}


void write_cb(uv_write_t* req, int status) {
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
}


static void timer_cb(uv_timer_t* handle, int status) {
  uv_process_kill(&process, /* SIGTERM */ 15);
  uv_close((uv_handle_t*)handle, close_cb);
}


TEST_IMPL(spawn_exit_code) {
  int r;

  init_process_options("spawn_helper1", exit_cb);

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 1);

  return 0;
}


TEST_IMPL(spawn_stdout) {
  int r;
  uv_pipe_t out;

  init_process_options("spawn_helper2", exit_cb);

  uv_pipe_init(uv_default_loop(), &out);
  options.stdout_stream = &out;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process once for the pipe. */
  printf("output is: %s", output);
  ASSERT(strcmp("hello world\n", output) == 0 || strcmp("hello world\r\n", output) == 0);

  return 0;
}


TEST_IMPL(spawn_stdin) {
int r;
  uv_pipe_t out;
  uv_pipe_t in;
  uv_write_t write_req;
  uv_buf_t buf;
  char buffer[] = "hello-from-spawn_stdin";

  init_process_options("spawn_helper3", exit_cb);

  uv_pipe_init(uv_default_loop(), &out);
  uv_pipe_init(uv_default_loop(), &in);
  options.stdout_stream = &out;
  options.stdin_stream = &in;

  r = uv_spawn(uv_default_loop(), &process, options);
  ASSERT(r == 0);

  buf.base = buffer;
  buf.len = sizeof(buffer);
  r = uv_write(&write_req, (uv_stream_t*)&in, &buf, 1, write_cb);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*) &out, on_alloc, on_read);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 3); /* Once for process twice for the pipe. */
  ASSERT(strcmp(buffer, output) == 0);

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

  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process and once for timer. */

  return 0;
}


#ifdef _WIN32
TEST_IMPL(spawn_detect_pipe_name_collisions_on_windows) {
  int r;
  uv_pipe_t out;
  char name[64];
  HANDLE pipe_handle;

  init_process_options("spawn_helper2", exit_cb);

  uv_pipe_init(uv_default_loop(), &out);
  options.stdout_stream = &out;

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

  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  ASSERT(exit_cb_called == 1);
  ASSERT(close_cb_called == 2); /* Once for process once for the pipe. */
  printf("output is: %s", output);
  ASSERT(strcmp("hello world\n", output) == 0 || strcmp("hello world\r\n", output) == 0);

  return 0;
}


wchar_t* make_program_args(char** args, int verbatim_arguments);
wchar_t* quote_cmd_arg(const wchar_t *source, wchar_t *target);

TEST_IMPL(argument_escaping) {
  const wchar_t* test_str[] = {
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
  wchar_t** test_output;
  wchar_t* command_line;
  wchar_t** cracked;
  size_t total_size = 0;
  int i;
  int num_args;

  char* verbatim[] = {
    "cmd.exe",
    "/c",
    "c:\\path\\to\\node.exe --eval \"require('c:\\\\path\\\\to\\\\test.js')\"",
    NULL
  };
  wchar_t* verbatim_output;
  wchar_t* non_verbatim_output;

  test_output = calloc(count, sizeof(wchar_t*));
  for (i = 0; i < count; ++i) {
    test_output[i] = calloc(2 * (wcslen(test_str[i]) + 2), sizeof(wchar_t));
    quote_cmd_arg(test_str[i], test_output[i]);
    wprintf(L"input : %s\n", test_str[i]);
    wprintf(L"output: %s\n", test_output[i]);
    total_size += wcslen(test_output[i]) + 1;
  }
  command_line = calloc(total_size + 1, sizeof(wchar_t));
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

  verbatim_output = make_program_args(verbatim, 1);
  non_verbatim_output = make_program_args(verbatim, 0);

  wprintf(L"    verbatim_output: %s\n", verbatim_output);
  wprintf(L"non_verbatim_output: %s\n", non_verbatim_output);

  ASSERT(wcscmp(verbatim_output, L"cmd.exe /c c:\\path\\to\\node.exe --eval \"require('c:\\\\path\\\\to\\\\test.js')\"") == 0);
  ASSERT(wcscmp(non_verbatim_output, L"cmd.exe /c \"c:\\path\\to\\node.exe --eval \\\"require('c:\\\\path\\\\to\\\\test.js')\\\"\"") == 0);

  free(verbatim_output);
  free(non_verbatim_output);

  return 0;
}

wchar_t* make_program_env(char** env_block);

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

  wchar_t expected[512];
  wchar_t* ptr = expected;
  wchar_t* result;
  wchar_t* str;

  for (i = 0; i < sizeof(environment) / sizeof(environment[0]) - 1; i++) {
    ptr += uv_utf8_to_utf16(environment[i], ptr, expected + sizeof(expected) - ptr);
  }

  memcpy(ptr, L"SYSTEMROOT=", sizeof(L"SYSTEMROOT="));
  ptr += sizeof(L"SYSTEMROOT=")/sizeof(wchar_t) - 1;
  ptr += GetEnvironmentVariableW(L"SYSTEMROOT", ptr, expected + sizeof(expected) - ptr);
  ++ptr;

  memcpy(ptr, L"SYSTEMDRIVE=", sizeof(L"SYSTEMDRIVE="));
  ptr += sizeof(L"SYSTEMDRIVE=")/sizeof(wchar_t) - 1;
  ptr += GetEnvironmentVariableW(L"SYSTEMDRIVE", ptr, expected + sizeof(expected) - ptr);
  ++ptr;
  *ptr = '\0';
  
  result = make_program_env(environment);

  for (str = result; *str; str += wcslen(str) + 1) {
    wprintf(L"%s\n", str);
  }

  ASSERT(wcscmp(expected, result) == 0);
 
  return 0;
}
#endif
