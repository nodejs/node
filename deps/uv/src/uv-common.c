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

#include "uv.h"
#include "uv-common.h"

#include <stdio.h>
#include <assert.h>
#include <stddef.h> /* NULL */
#include <stdlib.h> /* malloc */
#include <string.h> /* memset */


#define XX(uc, lc) case UV_##uc: return sizeof(uv_##lc##_t);

size_t uv_handle_size(uv_handle_type type) {
  switch (type) {
    UV_HANDLE_TYPE_MAP(XX)
    default:
      return -1;
  }
}

size_t uv_req_size(uv_req_type type) {
  switch(type) {
    UV_REQ_TYPE_MAP(XX)
    default:
      return -1;
  }
}

#undef XX

size_t uv_strlcpy(char* dst, const char* src, size_t size) {
  size_t n;

  if (size == 0)
    return 0;

  for (n = 0; n < (size - 1) && *src != '\0'; n++)
    *dst++ = *src++;

  *dst = '\0';

  return n;
}


size_t uv_strlcat(char* dst, const char* src, size_t size) {
  size_t n;

  if (size == 0)
    return 0;

  for (n = 0; n < size && *dst != '\0'; n++, dst++);

  if (n == size)
    return n;

  while (n < (size - 1) && *src != '\0')
    n++, *dst++ = *src++;

  *dst = '\0';

  return n;
}


uv_buf_t uv_buf_init(char* base, unsigned int len) {
  uv_buf_t buf;
  buf.base = base;
  buf.len = len;
  return buf;
}


const uv_err_t uv_ok_ = { UV_OK, 0 };

#define UV_ERR_NAME_GEN(val, name, s) case UV_##name : return #name;
const char* uv_err_name(uv_err_t err) {
  switch (err.code) {
    UV_ERRNO_MAP(UV_ERR_NAME_GEN)
    default:
      assert(0);
      return NULL;
  }
}
#undef UV_ERR_NAME_GEN


#define UV_STRERROR_GEN(val, name, s) case UV_##name : return s;
const char* uv_strerror(uv_err_t err) {
  switch (err.code) {
    UV_ERRNO_MAP(UV_STRERROR_GEN)
    default:
      return "Unknown system error";
  }
}
#undef UV_STRERROR_GEN


int uv__set_error(uv_loop_t* loop, uv_err_code code, int sys_error) {
  loop->last_err.code = code;
  loop->last_err.sys_errno_ = sys_error;
  return -1;
}


int uv__set_sys_error(uv_loop_t* loop, int sys_error) {
  loop->last_err.code = uv_translate_sys_error(sys_error);
  loop->last_err.sys_errno_ = sys_error;
  return -1;
}


int uv__set_artificial_error(uv_loop_t* loop, uv_err_code code) {
  loop->last_err = uv__new_artificial_error(code);
  return -1;
}


uv_err_t uv__new_sys_error(int sys_error) {
  uv_err_t error;
  error.code = uv_translate_sys_error(sys_error);
  error.sys_errno_ = sys_error;
  return error;
}


uv_err_t uv__new_artificial_error(uv_err_code code) {
  uv_err_t error;
  error.code = code;
  error.sys_errno_ = 0;
  return error;
}


uv_err_t uv_last_error(uv_loop_t* loop) {
  return loop->last_err;
}


struct sockaddr_in uv_ip4_addr(const char* ip, int port) {
  struct sockaddr_in addr;

  memset(&addr, 0, sizeof(struct sockaddr_in));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(ip);

  return addr;
}


struct sockaddr_in6 uv_ip6_addr(const char* ip, int port) {
  struct sockaddr_in6 addr;

  memset(&addr, 0, sizeof(struct sockaddr_in6));

  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(port);
  uv_inet_pton(AF_INET6, ip, &addr.sin6_addr);

  return addr;
}


int uv_ip4_name(struct sockaddr_in* src, char* dst, size_t size) {
  uv_err_t err = uv_inet_ntop(AF_INET, &src->sin_addr, dst, size);
  return err.code != UV_OK;
}


int uv_ip6_name(struct sockaddr_in6* src, char* dst, size_t size) {
  uv_err_t err = uv_inet_ntop(AF_INET6, &src->sin6_addr, dst, size);
  return err.code != UV_OK;
}


int uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in addr) {
  if (handle->type != UV_TCP || addr.sin_family != AF_INET)
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  else
    return uv__tcp_bind(handle, addr);
}


int uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6 addr) {
  if (handle->type != UV_TCP || addr.sin6_family != AF_INET6)
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  else
    return uv__tcp_bind6(handle, addr);
}


int uv_udp_bind(uv_udp_t* handle,
                struct sockaddr_in addr,
                unsigned int flags) {
  if (handle->type != UV_UDP || addr.sin_family != AF_INET)
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  else
    return uv__udp_bind(handle, addr, flags);
}


int uv_udp_bind6(uv_udp_t* handle,
                 struct sockaddr_in6 addr,
                 unsigned int flags) {
  if (handle->type != UV_UDP || addr.sin6_family != AF_INET6)
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  else
    return uv__udp_bind6(handle, addr, flags);
}


int uv_tcp_connect(uv_connect_t* req,
                   uv_tcp_t* handle,
                   struct sockaddr_in address,
                   uv_connect_cb cb) {
  if (handle->type != UV_TCP || address.sin_family != AF_INET)
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  else
    return uv__tcp_connect(req, handle, address, cb);
}


int uv_tcp_connect6(uv_connect_t* req,
                    uv_tcp_t* handle,
                    struct sockaddr_in6 address,
                    uv_connect_cb cb) {
  if (handle->type != UV_TCP || address.sin6_family != AF_INET6)
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  else
    return uv__tcp_connect6(req, handle, address, cb);
}


int uv_udp_send(uv_udp_send_t* req,
                uv_udp_t* handle,
                uv_buf_t bufs[],
                int bufcnt,
                struct sockaddr_in addr,
                uv_udp_send_cb send_cb) {
  if (handle->type != UV_UDP || addr.sin_family != AF_INET) {
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  }

  return uv__udp_send(req, handle, bufs, bufcnt, addr, send_cb);
}


int uv_udp_send6(uv_udp_send_t* req,
                 uv_udp_t* handle,
                 uv_buf_t bufs[],
                 int bufcnt,
                 struct sockaddr_in6 addr,
                 uv_udp_send_cb send_cb) {
  if (handle->type != UV_UDP || addr.sin6_family != AF_INET6) {
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  }

  return uv__udp_send6(req, handle, bufs, bufcnt, addr, send_cb);
}


int uv_udp_recv_start(uv_udp_t* handle,
                      uv_alloc_cb alloc_cb,
                      uv_udp_recv_cb recv_cb) {
  if (handle->type != UV_UDP || alloc_cb == NULL || recv_cb == NULL) {
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  }

  return uv__udp_recv_start(handle, alloc_cb, recv_cb);
}


int uv_udp_recv_stop(uv_udp_t* handle) {
  if (handle->type != UV_UDP) {
    return uv__set_artificial_error(handle->loop, UV_EINVAL);
  }

  return uv__udp_recv_stop(handle);
}

#ifdef _WIN32
static UINT __stdcall uv__thread_start(void *ctx_v)
#else
static void *uv__thread_start(void *ctx_v)
#endif
{
  void (*entry)(void *arg);
  void *arg;

  struct {
    void (*entry)(void *arg);
    void *arg;
  } *ctx;

  ctx = ctx_v;
  arg = ctx->arg;
  entry = ctx->entry;
  free(ctx);
  entry(arg);

  return 0;
}


int uv_thread_create(uv_thread_t *tid, void (*entry)(void *arg), void *arg) {
  struct {
    void (*entry)(void *arg);
    void *arg;
  } *ctx;

  if ((ctx = malloc(sizeof *ctx)) == NULL)
    return -1;

  ctx->entry = entry;
  ctx->arg = arg;

#ifdef _WIN32
  *tid = (HANDLE) _beginthreadex(NULL, 0, uv__thread_start, ctx, 0, NULL);
  if (*tid == 0) {
#else
  if (pthread_create(tid, NULL, uv__thread_start, ctx)) {
#endif
    free(ctx);
    return -1;
  }

  return 0;
}


unsigned long uv_thread_self(void) {
#ifdef _WIN32
  return (unsigned long) GetCurrentThreadId();
#else
  return (unsigned long) pthread_self();
#endif
}


void uv_walk(uv_loop_t* loop, uv_walk_cb walk_cb, void* arg) {
  ngx_queue_t* q;
  uv_handle_t* h;

  ngx_queue_foreach(q, &loop->handle_queue) {
    h = ngx_queue_data(q, uv_handle_t, handle_queue);
    if (h->flags & UV__HANDLE_INTERNAL) continue;
    walk_cb(h, arg);
  }
}


#ifndef NDEBUG
static void uv__print_handles(uv_loop_t* loop, int only_active) {
  const char* type;
  ngx_queue_t* q;
  uv_handle_t* h;

  if (loop == NULL)
    loop = uv_default_loop();

  ngx_queue_foreach(q, &loop->handle_queue) {
    h = ngx_queue_data(q, uv_handle_t, handle_queue);

    if (only_active && !uv__is_active(h))
      continue;

    switch (h->type) {
#define X(uc, lc) case UV_##uc: type = #lc; break;
      UV_HANDLE_TYPE_MAP(X)
#undef X
      default: type = "<unknown>";
    }

    fprintf(stderr,
            "[%c%c%c] %-8s %p\n",
            "R-"[!(h->flags & UV__HANDLE_REF)],
            "A-"[!(h->flags & UV__HANDLE_ACTIVE)],
            "I-"[!(h->flags & UV__HANDLE_INTERNAL)],
            type,
            (void*)h);
  }
}


void uv_print_all_handles(uv_loop_t* loop) {
  uv__print_handles(loop, 0);
}


void uv_print_active_handles(uv_loop_t* loop) {
  uv__print_handles(loop, 1);
}
#endif


void uv_ref(uv_handle_t* handle) {
  uv__handle_ref(handle);
}


void uv_unref(uv_handle_t* handle) {
  uv__handle_unref(handle);
}


void uv_stop(uv_loop_t* loop) {
  loop->stop_flag = 1;
}


uint64_t uv_now(uv_loop_t* loop) {
  return loop->time;
}
