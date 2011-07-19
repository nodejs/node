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

#include "uv.h"
#include "../uv-common.h"
#include "internal.h"


void uv_loop_watcher_endgame(uv_handle_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb(handle);
    }

    uv_unref();
  }
}


#define UV_LOOP_WATCHER_DEFINE(name, NAME)                                    \
  int uv_##name##_init(uv_##name##_t* handle) {                               \
    handle->type = UV_##NAME;                                                 \
    handle->flags = 0;                                                        \
    handle->error = uv_ok_;                                                   \
                                                                              \
    uv_ref();                                                                 \
                                                                              \
    uv_counters()->handle_init++;                                             \
    uv_counters()->prepare_init++;                                            \
                                                                              \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
                                                                              \
  int uv_##name##_start(uv_##name##_t* handle, uv_##name##_cb cb) {           \
    uv_##name##_t* old_head;                                                  \
                                                                              \
    assert(handle->type == UV_##NAME);                                        \
                                                                              \
    if (handle->flags & UV_HANDLE_ACTIVE)                                     \
      return 0;                                                               \
                                                                              \
    old_head = LOOP->name##_handles;                                          \
                                                                              \
    handle->name##_next = old_head;                                           \
    handle->name##_prev = NULL;                                               \
                                                                              \
    if (old_head) {                                                           \
      old_head->name##_prev = handle;                                         \
    }                                                                         \
                                                                              \
    LOOP->name##_handles = handle;                                            \
                                                                              \
    handle->name##_cb = cb;                                                   \
    handle->flags |= UV_HANDLE_ACTIVE;                                        \
                                                                              \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
                                                                              \
  int uv_##name##_stop(uv_##name##_t* handle) {                               \
    assert(handle->type == UV_##NAME);                                        \
                                                                              \
    if (!(handle->flags & UV_HANDLE_ACTIVE))                                  \
      return 0;                                                               \
                                                                              \
    /* Update loop head if needed */                                          \
    if (LOOP->name##_handles == handle) {                                     \
      LOOP->name##_handles = handle->name##_next;                             \
    }                                                                         \
                                                                              \
    /* Update the iterator-next pointer of needed */                          \
    if (LOOP->next_##name##_handle == handle) {                               \
      LOOP->next_##name##_handle = handle->name##_next;                       \
    }                                                                         \
                                                                              \
    if (handle->name##_prev) {                                                \
      handle->name##_prev->name##_next = handle->name##_next;                 \
    }                                                                         \
    if (handle->name##_next) {                                                \
      handle->name##_next->name##_prev = handle->name##_prev;                 \
    }                                                                         \
                                                                              \
    handle->flags &= ~UV_HANDLE_ACTIVE;                                       \
                                                                              \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
                                                                              \
  void uv_##name##_invoke() {                                                 \
    uv_##name##_t* handle;                                                    \
                                                                              \
    LOOP->next_##name##_handle = LOOP->name##_handles;                        \
                                                                              \
    while (LOOP->next_##name##_handle != NULL) {                              \
      handle = LOOP->next_##name##_handle;                                    \
      LOOP->next_##name##_handle = handle->name##_next;                       \
                                                                              \
      handle->name##_cb(handle, 0);                                           \
    }                                                                         \
  }

UV_LOOP_WATCHER_DEFINE(prepare, PREPARE)
UV_LOOP_WATCHER_DEFINE(check, CHECK)
UV_LOOP_WATCHER_DEFINE(idle, IDLE)
