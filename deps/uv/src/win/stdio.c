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

#include <assert.h>
#include <string.h>

#include "uv.h"
#include "../uv-common.h"
#include "internal.h"


static uv_pipe_t* uv_make_pipe_for_std_handle(uv_loop_t* loop, HANDLE handle) {
  uv_pipe_t* pipe = NULL;

  pipe = (uv_pipe_t*)malloc(sizeof(uv_pipe_t));
  if (!pipe) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  if (uv_pipe_init_with_handle(loop, pipe, handle)) {
    free(pipe);
    return NULL;
  }

  pipe->flags |= UV_HANDLE_UV_ALLOCED;
  return pipe;
}


uv_stream_t* uv_std_handle(uv_loop_t* loop, uv_std_type type) {
  HANDLE handle;

  switch (type) {
    case UV_STDIN:
      handle = GetStdHandle(STD_INPUT_HANDLE);
      if (handle == INVALID_HANDLE_VALUE) {
        return NULL;
      }

      /* Assume only named pipes for now. */
      return (uv_stream_t*)uv_make_pipe_for_std_handle(loop, handle);
      break;

    case UV_STDOUT:
      return NULL;
      break;

    case UV_STDERR:
      return NULL;
      break;

    default:
      assert(0);
      uv_set_error(loop, UV_EINVAL, 0);
      return NULL;
  }
}
