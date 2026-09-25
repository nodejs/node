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

struct uv__process_title {
  char* str;
  size_t len;  /* Length of the current process title. */
  size_t cap;  /* Maximum capacity. Computed once in uv_setup_args(). */
};

extern void uv__set_process_title(const char* title);

static uv_mutex_t process_title_mutex;
static uv_once_t process_title_mutex_once = UV_ONCE_INIT;
static struct uv__process_title process_title;
static void* args_mem;


static void init_process_title_mutex_once(void) {
  uv_mutex_init(&process_title_mutex);
}


char** uv_setup_args(int argc, char** argv) {
  struct uv__process_title pt;
  char** new_argv;
  size_t size;
  char* s;
  int i;

  if (argc <= 0)
    return argv;

  pt.str = argv[0];
  pt.len = strlen(argv[0]);
  pt.cap = pt.len + 1;

  /* Calculate how much memory we need for the argv strings. */
  size = pt.cap;
  for (i = 1; i < argc; i++)
    size += strlen(argv[i]) + 1;

  /* Add space for the argv pointers. */
  size += (argc + 1) * sizeof(char*);

  new_argv = uv__malloc(size);
  if (new_argv == NULL)
    return argv;

  /* Copy over the strings and set up the pointer table. */
  i = 0;
  s = (char*) &new_argv[argc + 1];
  size = pt.cap;
  goto loop;

  for (/* empty */; i < argc; i++) {
    size = strlen(argv[i]) + 1;
  loop:
    memcpy(s, argv[i], size);
    new_argv[i] = s;
    s += size;
  }
  new_argv[i] = NULL;

  pt.cap = argv[i - 1] + size - argv[0];

  args_mem = new_argv;
  process_title = pt;

  return new_argv;
}


int uv_set_process_title(const char* title) {
  struct uv__process_title* pt;
  size_t len;

  /* If uv_setup_args wasn't called or failed, we can't continue. */
  if (args_mem == NULL)
    return UV_ENOBUFS;

  pt = &process_title;
  len = strlen(title);

  uv_once(&process_title_mutex_once, init_process_title_mutex_once);
  uv_mutex_lock(&process_title_mutex);

  if (len >= pt->cap) {
    len = 0;
    if (pt->cap > 0)
      len = pt->cap - 1;
  }

  memcpy(pt->str, title, len);
  memset(pt->str + len, '\0', pt->cap - len);
  pt->len = len;
  uv__set_process_title(pt->str);

  uv_mutex_unlock(&process_title_mutex);

  return 0;
}


int uv_get_process_title(char* buffer, size_t size) {
  if (buffer == NULL || size == 0)
    return UV_EINVAL;

  /* If uv_setup_args wasn't called or failed, we can't continue. */
  if (args_mem == NULL)
    return UV_ENOBUFS;

  uv_once(&process_title_mutex_once, init_process_title_mutex_once);
  uv_mutex_lock(&process_title_mutex);

  if (size <= process_title.len) {
    uv_mutex_unlock(&process_title_mutex);
    return UV_ENOBUFS;
  }

  if (process_title.len != 0)
    memcpy(buffer, process_title.str, process_title.len + 1);

  buffer[process_title.len] = '\0';

  uv_mutex_unlock(&process_title_mutex);

  return 0;
}


void uv__process_title_cleanup(void) {
  uv__free(args_mem);  /* Keep valgrind happy. */
  args_mem = NULL;
}
