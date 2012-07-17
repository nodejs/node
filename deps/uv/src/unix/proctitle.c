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
#include <assert.h>

/* NOTE: FreeBSD is using it's own implementation of those functions */

static struct {
  char *str;
  size_t len;
} process_title;

extern char** environ;


char** uv_setup_args(int argc, char** argv) {
  char **new_argv;
  char **new_env;
  size_t size;
  int envc;
  char *s;
  int i;

  for (envc = 0; environ[envc]; envc++);

  s = envc ? environ[envc - 1] : argv[argc - 1];

  process_title.str = argv[0];
  process_title.len = s + strlen(s) + 1 - argv[0];

  size = process_title.len;
  size += (argc + 1) * sizeof(char **);
  size += (envc + 1) * sizeof(char **);

  if ((s = (char *) malloc(size)) == NULL) {
    process_title.str = NULL;
    process_title.len = 0;
    return argv;
  }

  new_argv = (char **) s;
  new_env = new_argv + argc + 1;
  s = (char *) (new_env + envc + 1);
  memcpy(s, process_title.str, process_title.len);

  for (i = 0; i < argc; i++)
    new_argv[i] = s + (argv[i] - argv[0]);
  new_argv[argc] = NULL;

  s += environ[0] - argv[0];

  for (i = 0; i < envc; i++)
    new_env[i] = s + (environ[i] - environ[0]);
  new_env[envc] = NULL;

  environ = new_env;
  return new_argv;
}


uv_err_t uv_set_process_title(const char* title) {
  uv_strlcpy(process_title.str, title, process_title.len);
  return uv_ok_;
}


uv_err_t uv_get_process_title(char* buffer, size_t size) {
  uv_strlcpy(buffer, process_title.str ? process_title.str : "", size);
  return uv_ok_;
}
