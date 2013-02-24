/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#include <stdlib.h>
#include <string.h>

extern void uv__set_process_title(const char* title);

static void* args_mem;

static struct {
  char* str;
  int len;
} process_title;


char** uv_setup_args(int argc, char** argv) {
  char** new_argv;
  char** new_env;
  size_t size;
  int envc;
  char* s;
  int i;

#if defined(__APPLE__)
  char*** _NSGetArgv(void);
  char*** _NSGetEnviron(void);
  char** environ = *_NSGetEnviron();
#else
  extern char** environ;
#endif

  for (envc = 0; environ[envc]; envc++);

  if (envc == 0)
    s = argv[argc - 1];
  else
    s = environ[envc - 1];

  process_title.str = argv[0];
  process_title.len = s + strlen(s) + 1 - argv[0];

  size = process_title.len;
  size += (argc + 1) * sizeof(char**);
  size += (envc + 1) * sizeof(char**);
  s = args_mem = malloc(size);

  if (s == NULL) {
    process_title.str = NULL;
    process_title.len = 0;
    return argv;
  }

  new_argv = (char**) s;
  new_env = new_argv + argc + 1;
  s = (char*) (new_env + envc + 1);
  memcpy(s, process_title.str, process_title.len);

  for (i = 0; i < argc; i++)
    new_argv[i] = s + (argv[i] - argv[0]);
  new_argv[argc] = NULL;

  s += environ[0] - argv[0];

  for (i = 0; i < envc; i++)
    new_env[i] = s + (environ[i] - environ[0]);
  new_env[envc] = NULL;

#if defined(__APPLE__)
  *_NSGetArgv() = new_argv;
  *_NSGetEnviron() = new_env;
#else
  environ = new_env;
#endif

  return new_argv;
}


uv_err_t uv_set_process_title(const char* title) {
  if (process_title.len == 0)
    return uv_ok_;

  /* No need to terminate, last char is always '\0'. */
  strncpy(process_title.str, title, process_title.len - 1);
  uv__set_process_title(title);

  return uv_ok_;
}


uv_err_t uv_get_process_title(char* buffer, size_t size) {
  if (process_title.len > 0)
    strncpy(buffer, process_title.str, size);
  else if (size > 0)
    buffer[0] = '\0';

  return uv_ok_;
}


__attribute__((destructor))
static void free_args_mem(void) {
  free(args_mem);  /* Keep valgrind happy. */
  args_mem = NULL;
}
