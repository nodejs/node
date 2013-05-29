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

/*
 * This file is private to libuv. It provides common functionality to both
 * Windows and Unix backends.
 */

#ifndef UV_COMMON_H_
#define UV_COMMON_H_

#include <assert.h>
#include <stddef.h>

#if defined(_MSC_VER) && _MSC_VER < 1600
# include "uv-private/stdint-msvc2008.h"
#else
# include <stdint.h>
#endif

#include "uv.h"
#include "tree.h"
#include "queue.h"


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define container_of(ptr, type, member) \
  ((type *) ((char *) (ptr) - offsetof(type, member)))

#ifdef _MSC_VER
# define UNUSED /* empty */
# define INLINE __inline
#else
# define UNUSED __attribute__((unused))
# define INLINE inline
#endif


#ifndef _WIN32
enum {
  UV__HANDLE_INTERNAL = 0x8000,
  UV__HANDLE_ACTIVE   = 0x4000,
  UV__HANDLE_REF      = 0x2000,
  UV__HANDLE_CLOSING  = 0 /* no-op on unix */
};
#else
# define UV__HANDLE_INTERNAL  0x80
# define UV__HANDLE_ACTIVE    0x40
# define UV__HANDLE_REF       0x20
# define UV__HANDLE_CLOSING   0x01
#endif

extern const uv_err_t uv_ok_;

uv_err_code uv_translate_sys_error(int sys_errno);
int uv__set_error(uv_loop_t* loop, uv_err_code code, int sys_error);
int uv__set_sys_error(uv_loop_t* loop, int sys_error);
int uv__set_artificial_error(uv_loop_t* loop, uv_err_code code);
uv_err_t uv__new_sys_error(int sys_error);
uv_err_t uv__new_artificial_error(uv_err_code code);

int uv__tcp_bind(uv_tcp_t* handle, struct sockaddr_in addr);
int uv__tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6 addr);

int uv__udp_bind(uv_udp_t* handle, struct sockaddr_in addr, unsigned flags);
int uv__udp_bind6(uv_udp_t* handle, struct sockaddr_in6 addr, unsigned flags);

int uv__tcp_connect(uv_connect_t* req,
                   uv_tcp_t* handle,
                   struct sockaddr_in address,
                   uv_connect_cb cb);

int uv__tcp_connect6(uv_connect_t* req,
                    uv_tcp_t* handle,
                    struct sockaddr_in6 address,
                    uv_connect_cb cb);

int uv__udp_send(uv_udp_send_t* req,
                 uv_udp_t* handle,
                 uv_buf_t bufs[],
                 int bufcnt,
                 struct sockaddr_in addr,
                 uv_udp_send_cb send_cb);

int uv__udp_send6(uv_udp_send_t* req,
                  uv_udp_t* handle,
                  uv_buf_t bufs[],
                  int bufcnt,
                  struct sockaddr_in6 addr,
                  uv_udp_send_cb send_cb);

int uv__udp_recv_start(uv_udp_t* handle, uv_alloc_cb alloccb,
                       uv_udp_recv_cb recv_cb);

int uv__udp_recv_stop(uv_udp_t* handle);

void uv__fs_poll_close(uv_fs_poll_t* handle);


#define uv__has_active_reqs(loop)                                             \
  (QUEUE_EMPTY(&(loop)->active_reqs) == 0)

#define uv__req_register(loop, req)                                           \
  do {                                                                        \
    QUEUE_INSERT_TAIL(&(loop)->active_reqs, &(req)->active_queue);            \
  }                                                                           \
  while (0)

#define uv__req_unregister(loop, req)                                         \
  do {                                                                        \
    assert(uv__has_active_reqs(loop));                                        \
    QUEUE_REMOVE(&(req)->active_queue);                                       \
  }                                                                           \
  while (0)

#define uv__has_active_handles(loop)                                          \
  ((loop)->active_handles > 0)

#define uv__active_handle_add(h)                                              \
  do {                                                                        \
    (h)->loop->active_handles++;                                              \
  }                                                                           \
  while (0)

#define uv__active_handle_rm(h)                                               \
  do {                                                                        \
    (h)->loop->active_handles--;                                              \
  }                                                                           \
  while (0)

#define uv__is_active(h)                                                      \
  (((h)->flags & UV__HANDLE_ACTIVE) != 0)

#define uv__is_closing(h)                                                     \
  (((h)->flags & (UV_CLOSING |  UV_CLOSED)) != 0)

#define uv__handle_start(h)                                                   \
  do {                                                                        \
    assert(((h)->flags & UV__HANDLE_CLOSING) == 0);                           \
    if (((h)->flags & UV__HANDLE_ACTIVE) != 0) break;                         \
    (h)->flags |= UV__HANDLE_ACTIVE;                                          \
    if (((h)->flags & UV__HANDLE_REF) != 0) uv__active_handle_add(h);         \
  }                                                                           \
  while (0)

#define uv__handle_stop(h)                                                    \
  do {                                                                        \
    assert(((h)->flags & UV__HANDLE_CLOSING) == 0);                           \
    if (((h)->flags & UV__HANDLE_ACTIVE) == 0) break;                         \
    (h)->flags &= ~UV__HANDLE_ACTIVE;                                         \
    if (((h)->flags & UV__HANDLE_REF) != 0) uv__active_handle_rm(h);          \
  }                                                                           \
  while (0)

#define uv__handle_ref(h)                                                     \
  do {                                                                        \
    if (((h)->flags & UV__HANDLE_REF) != 0) break;                            \
    (h)->flags |= UV__HANDLE_REF;                                             \
    if (((h)->flags & UV__HANDLE_CLOSING) != 0) break;                        \
    if (((h)->flags & UV__HANDLE_ACTIVE) != 0) uv__active_handle_add(h);      \
  }                                                                           \
  while (0)

#define uv__handle_unref(h)                                                   \
  do {                                                                        \
    if (((h)->flags & UV__HANDLE_REF) == 0) break;                            \
    (h)->flags &= ~UV__HANDLE_REF;                                            \
    if (((h)->flags & UV__HANDLE_CLOSING) != 0) break;                        \
    if (((h)->flags & UV__HANDLE_ACTIVE) != 0) uv__active_handle_rm(h);       \
  }                                                                           \
  while (0)

#define uv__has_ref(h)                                                        \
  (((h)->flags & UV__HANDLE_REF) != 0)

#if defined(_WIN32)
# define uv__handle_platform_init(h)
#else
# define uv__handle_platform_init(h) ((h)->next_closing = NULL)
#endif

#define uv__handle_init(loop_, h, type_)                                      \
  do {                                                                        \
    (h)->loop = (loop_);                                                      \
    (h)->type = (type_);                                                      \
    (h)->flags = UV__HANDLE_REF;  /* Ref the loop when active. */             \
    QUEUE_INSERT_TAIL(&(loop_)->handle_queue, &(h)->handle_queue);            \
    uv__handle_platform_init(h);                                              \
  }                                                                           \
  while (0)

#endif /* UV_COMMON_H_ */
