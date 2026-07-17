/* Copyright libuv project contributors. All rights reserved.
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

#include <stdlib.h>
#include <string.h>

static uv_mutex_t process_title_mutex;
static uv_once_t process_title_mutex_once = UV_ONCE_INIT;
static char* process_title = NULL;
static void* args_mem = NULL;


static void init_process_title_mutex_once(void) {
  uv_mutex_init(&process_title_mutex);
}


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

  /* Add space for the argv pointers. */
  size += (argc + 1) * sizeof(char*);

  new_argv = uv__malloc(size);
  if (new_argv == NULL)
    return argv;

  /* Copy over the strings and set up the pointer table. */
  s = (char*) &new_argv[argc + 1];
  for (i = 0; i < argc; i++) {
    size = strlen(argv[i]) + 1;
    memcpy(s, argv[i], size);
    new_argv[i] = s;
    s += size;
  }
  new_argv[i] = NULL;

  args_mem = new_argv;
  process_title = uv__strdup(argv[0]);

  return new_argv;
}


int uv_set_process_title(const char* title) {
  char* new_title;

  /* If uv_setup_args wasn't called or failed, we can't continue. */
  if (args_mem == NULL)
    return UV_ENOBUFS;

  /* We cannot free this pointer when libuv shuts down,
   * the process may still be using it.
   */
  new_title = uv__strdup(title);
  if (new_title == NULL)
    return UV_ENOMEM;

  uv_once(&process_title_mutex_once, init_process_title_mutex_once);
  uv_mutex_lock(&process_title_mutex);

  if (process_title != NULL)
    uv__free(process_title);

  process_title = new_title;

  uv_mutex_unlock(&process_title_mutex);

  return 0;
}


int uv_get_process_title(char* buffer, size_t size) {
  size_t len;

  if (buffer == NULL || size == 0)
    return UV_EINVAL;

  /* If uv_setup_args wasn't called or failed, we can't continue. */
  if (args_mem == NULL || process_title == NULL)
    return UV_ENOBUFS;

  uv_once(&process_title_mutex_once, init_process_title_mutex_once);
  uv_mutex_lock(&process_title_mutex);

  len = strlen(process_title);

  if (size <= len) {
    uv_mutex_unlock(&process_title_mutex);
    return UV_ENOBUFS;
  }

  strcpy(buffer, process_title);

  uv_mutex_unlock(&process_title_mutex);

  return 0;
}


void uv__process_title_cleanup(void) {
  uv__free(args_mem);  /* Keep valgrind happy. */
  args_mem = NULL;
}
