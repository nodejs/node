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

void uv__fs_poll_close(uv_fs_poll_t* handle);


UNUSED static int uv__has_active_reqs(const uv_loop_t* loop) {
  return !ngx_queue_empty(&loop->active_reqs);
}

UNUSED static void uv__req_register(uv_loop_t* loop, uv_req_t* req) {
  ngx_queue_insert_tail(&loop->active_reqs, &req->active_queue);
}

UNUSED static void uv__req_unregister(uv_loop_t* loop, uv_req_t* req) {
  assert(uv__has_active_reqs(loop));
  ngx_queue_remove(&req->active_queue);
}


UNUSED static int uv__has_active_handles(const uv_loop_t* loop) {
  return loop->active_handles > 0;
}

UNUSED static void uv__active_handle_add(uv_handle_t* h) {
  h->loop->active_handles++;
}

UNUSED static void uv__active_handle_rm(uv_handle_t* h) {
  h->loop->active_handles--;
}


#define uv__active_handle_add(h) uv__active_handle_add((uv_handle_t*)(h))
#define uv__active_handle_rm(h) uv__active_handle_rm((uv_handle_t*)(h))

#define uv__req_register(loop, req) uv__req_register((loop), (uv_req_t*)(req))
#define uv__req_unregister(loop, req) uv__req_unregister((loop), (uv_req_t*)(req))

UNUSED static int uv__is_active(const uv_handle_t* h) {
  return !!(h->flags & UV__HANDLE_ACTIVE);
}
#define uv__is_active(h) uv__is_active((const uv_handle_t*)(h))

UNUSED static void uv__handle_start(uv_handle_t* h) {
  assert(!(h->flags & UV__HANDLE_CLOSING));
  if (h->flags & UV__HANDLE_ACTIVE)
    return;
  h->flags |= UV__HANDLE_ACTIVE;
  if (h->flags & UV__HANDLE_REF)
    uv__active_handle_add(h);
}
#define uv__handle_start(h) uv__handle_start((uv_handle_t*)(h))

UNUSED static void uv__handle_stop(uv_handle_t* h) {
  assert(!(h->flags & UV__HANDLE_CLOSING));
  if (!(h->flags & UV__HANDLE_ACTIVE))
    return;
  h->flags &= ~UV__HANDLE_ACTIVE;
  if (h->flags & UV__HANDLE_REF)
    uv__active_handle_rm(h);
}
#define uv__handle_stop(h) uv__handle_stop((uv_handle_t*)(h))

UNUSED static void uv__handle_ref(uv_handle_t* h) {
  if (h->flags & UV__HANDLE_REF)
    return;
  h->flags |= UV__HANDLE_REF;
  if (h->flags & UV__HANDLE_CLOSING)
    return;
  if (h->flags & UV__HANDLE_ACTIVE)
    uv__active_handle_add(h);
}
#define uv__handle_ref(h) uv__handle_ref((uv_handle_t*)(h))

UNUSED static void uv__handle_unref(uv_handle_t* h) {
  if (!(h->flags & UV__HANDLE_REF))
    return;
  h->flags &= ~UV__HANDLE_REF;
  if (h->flags & UV__HANDLE_CLOSING)
    return;
  if (h->flags & UV__HANDLE_ACTIVE)
    uv__active_handle_rm(h);
}
#define uv__handle_unref(h) uv__handle_unref((uv_handle_t*)(h))

UNUSED static void uv__handle_init(uv_loop_t* loop,
                                   uv_handle_t* handle,
                                   uv_handle_type type) {
  handle->loop = loop;
  handle->type = type;
  handle->flags = UV__HANDLE_REF; /* ref the loop when active */
  ngx_queue_insert_tail(&loop->handle_queue, &handle->handle_queue);
#ifndef _WIN32
  handle->next_closing = NULL;
#endif
}

#endif /* UV_COMMON_H_ */
