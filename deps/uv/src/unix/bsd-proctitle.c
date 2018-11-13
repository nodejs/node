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

#include <sys/types.h>
#include <unistd.h>


static uv_mutex_t process_title_mutex;
static uv_once_t process_title_mutex_once = UV_ONCE_INIT;
static char* process_title;


static void init_process_title_mutex_once(void) {
  if (uv_mutex_init(&process_title_mutex))
    abort();
}


char** uv_setup_args(int argc, char** argv) {
  process_title = argc > 0 ? uv__strdup(argv[0]) : NULL;
  return argv;
}


int uv_set_process_title(const char* title) {
  char* new_title;

  new_title = uv__strdup(title);
  if (new_title == NULL)
    return UV_ENOMEM;

  uv_once(&process_title_mutex_once, init_process_title_mutex_once);
  uv_mutex_lock(&process_title_mutex);

  uv__free(process_title);
  process_title = new_title;
  setproctitle("%s", title);

  uv_mutex_unlock(&process_title_mutex);

  return 0;
}


int uv_get_process_title(char* buffer, size_t size) {
  size_t len;

  if (buffer == NULL || size == 0)
    return UV_EINVAL;

  uv_once(&process_title_mutex_once, init_process_title_mutex_once);
  uv_mutex_lock(&process_title_mutex);

  if (process_title != NULL) {
    len = strlen(process_title) + 1;

    if (size < len) {
      uv_mutex_unlock(&process_title_mutex);
      return UV_ENOBUFS;
    }

    memcpy(buffer, process_title, len);
  } else {
    len = 0;
  }

  uv_mutex_unlock(&process_title_mutex);

  buffer[len] = '\0';

  return 0;
}
