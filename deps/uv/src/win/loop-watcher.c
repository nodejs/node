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
#include "internal.h"
#include "handle-inl.h"


void uv_loop_watcher_endgame(uv_loop_t* loop, uv_handle_t* handle) {
  if (handle->flags & UV__HANDLE_CLOSING) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;
    uv__handle_close(handle);
  }
}


#define UV_LOOP_WATCHER_DEFINE(name, NAME)                                    \
  int uv_##name##_init(uv_loop_t* loop, uv_##name##_t* handle) {              \
    uv__handle_init(loop, (uv_handle_t*) handle, UV_##NAME);                  \
                                                                              \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
                                                                              \
  int uv_##name##_start(uv_##name##_t* handle, uv_##name##_cb cb) {           \
    uv_loop_t* loop = handle->loop;                                           \
    uv_##name##_t* old_head;                                                  \
                                                                              \
    assert(handle->type == UV_##NAME);                                        \
                                                                              \
    if (handle->flags & UV_HANDLE_ACTIVE)                                     \
      return 0;                                                               \
                                                                              \
    if (cb == NULL)                                                           \
      return UV_EINVAL;                                                       \
                                                                              \
    old_head = loop->name##_handles;                                          \
                                                                              \
    handle->name##_next = old_head;                                           \
    handle->name##_prev = NULL;                                               \
                                                                              \
    if (old_head) {                                                           \
      old_head->name##_prev = handle;                                         \
    }                                                                         \
                                                                              \
    loop->name##_handles = handle;                                            \
                                                                              \
    handle->name##_cb = cb;                                                   \
    handle->flags |= UV_HANDLE_ACTIVE;                                        \
    uv__handle_start(handle);                                                 \
                                                                              \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
                                                                              \
  int uv_##name##_stop(uv_##name##_t* handle) {                               \
    uv_loop_t* loop = handle->loop;                                           \
                                                                              \
    assert(handle->type == UV_##NAME);                                        \
                                                                              \
    if (!(handle->flags & UV_HANDLE_ACTIVE))                                  \
      return 0;                                                               \
                                                                              \
    /* Update loop head if needed */                                          \
    if (loop->name##_handles == handle) {                                     \
      loop->name##_handles = handle->name##_next;                             \
    }                                                                         \
                                                                              \
    /* Update the iterator-next pointer of needed */                          \
    if (loop->next_##name##_handle == handle) {                               \
      loop->next_##name##_handle = handle->name##_next;                       \
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
    uv__handle_stop(handle);                                                  \
                                                                              \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
                                                                              \
  void uv_##name##_invoke(uv_loop_t* loop) {                                  \
    uv_##name##_t* handle;                                                    \
                                                                              \
    (loop)->next_##name##_handle = (loop)->name##_handles;                    \
                                                                              \
    while ((loop)->next_##name##_handle != NULL) {                            \
      handle = (loop)->next_##name##_handle;                                  \
      (loop)->next_##name##_handle = handle->name##_next;                     \
                                                                              \
      handle->name##_cb(handle, 0);                                           \
    }                                                                         \
  }

UV_LOOP_WATCHER_DEFINE(prepare, PREPARE)
UV_LOOP_WATCHER_DEFINE(check, CHECK)
UV_LOOP_WATCHER_DEFINE(idle, IDLE)
