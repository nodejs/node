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
  size_t len;
} process_title;


char** uv_setup_args(int argc, char** argv) {
  char** new_argv;
  size_t size;
  char* s;
  int i;

  if (argc <= 0)
    return argv;

  /* Calculate how much memory we need for the argv strings. */
  size = 0;
  for (i = 0; i < argc; i++)
    size += strlen(argv[i]) + 1;

  process_title.str = argv[0];
  process_title.len = argv[argc - 1] + strlen(argv[argc - 1]) - argv[0];
  assert(process_title.len + 1 == size);  /* argv memory should be adjacent. */

  /* Add space for the argv pointers. */
  size += (argc + 1) * sizeof(char*);

  new_argv = uv__malloc(size);
  if (new_argv == NULL)
    return argv;
  args_mem = new_argv;

  /* Copy over the strings and set up the pointer table. */
  s = (char*) &new_argv[argc + 1];
  for (i = 0; i < argc; i++) {
    size = strlen(argv[i]) + 1;
    memcpy(s, argv[i], size);
    new_argv[i] = s;
    s += size;
  }
  new_argv[i] = NULL;

  return new_argv;
}


int uv_set_process_title(const char* title) {
  if (process_title.len == 0)
    return 0;

  /* No need to terminate, byte after is always '\0'. */
  strncpy(process_title.str, title, process_title.len);
  uv__set_process_title(title);

  return 0;
}


int uv_get_process_title(char* buffer, size_t size) {
  if (process_title.len > 0)
    strncpy(buffer, process_title.str, size);
  else if (size > 0)
    buffer[0] = '\0';

  return 0;
}


UV_DESTRUCTOR(static void free_args_mem(void)) {
  uv__free(args_mem);  /* Keep valgrind happy. */
  args_mem = NULL;
}
