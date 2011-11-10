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

#include <assert.h>
#include <stddef.h> /* NULL */
#include <string.h> /* memset */

/* use inet_pton from c-ares if necessary */
#include "ares_config.h"
#include "ares/inet_net_pton.h"
#include "ares/inet_ntop.h"


static uv_counters_t counters;


uv_counters_t* uv_counters() {
  return &counters;
}


uv_buf_t uv_buf_init(char* base, size_t len) {
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


void uv__set_error(uv_loop_t* loop, uv_err_code code, int sys_error) {
  loop->last_err.code = code;
  loop->last_err.sys_errno_ = sys_error;
}


void uv__set_sys_error(uv_loop_t* loop, int sys_error) {
  loop->last_err.code = uv_translate_sys_error(sys_error);
  loop->last_err.sys_errno_ = sys_error;
}


void uv__set_artificial_error(uv_loop_t* loop, uv_err_code code) {
  loop->last_err = uv__new_artificial_error(code);
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
  ares_inet_pton(AF_INET6, ip, &addr.sin6_addr);

  return addr;
}


int uv_ip4_name(struct sockaddr_in* src, char* dst, size_t size) {
  const char* d = ares_inet_ntop(AF_INET, &src->sin_addr, dst, size);
  return d != dst;
}


int uv_ip6_name(struct sockaddr_in6* src, char* dst, size_t size) {
  const char* d = ares_inet_ntop(AF_INET6, &src->sin6_addr, dst, size);
  return d != dst;
}


/* find matching ares handle in list */
void uv_add_ares_handle(uv_loop_t* loop, uv_ares_task_t* handle) {
  handle->loop = loop;
  handle->ares_next = loop->uv_ares_handles_;
  handle->ares_prev = NULL;

  if (loop->uv_ares_handles_) {
    loop->uv_ares_handles_->ares_prev = handle;
  }

  loop->uv_ares_handles_ = handle;
}

/* find matching ares handle in list */
/* TODO: faster lookup */
uv_ares_task_t* uv_find_ares_handle(uv_loop_t* loop, ares_socket_t sock) {
  uv_ares_task_t* handle = loop->uv_ares_handles_;

  while (handle != NULL) {
    if (handle->sock == sock) {
      break;
    }
    handle = handle->ares_next;
  }

  return handle;
}

/* remove ares handle in list */
void uv_remove_ares_handle(uv_ares_task_t* handle) {
  uv_loop_t* loop = handle->loop;

  if (handle == loop->uv_ares_handles_) {
    loop->uv_ares_handles_ = handle->ares_next;
  }

  if (handle->ares_next) {
    handle->ares_next->ares_prev = handle->ares_prev;
  }

  if (handle->ares_prev) {
    handle->ares_prev->ares_next = handle->ares_next;
  }
}


/* Returns 1 if the uv_ares_handles_ list is empty. 0 otherwise. */
int uv_ares_handles_empty(uv_loop_t* loop) {
  return loop->uv_ares_handles_ ? 0 : 1;
}

int uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in addr) {
  if (handle->type != UV_TCP || addr.sin_family != AF_INET) {
    uv__set_artificial_error(handle->loop, UV_EFAULT);
    return -1;
  }

  return uv__tcp_bind(handle, addr);
}

int uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6 addr) {
  if (handle->type != UV_TCP || addr.sin6_family != AF_INET6) {
    uv__set_artificial_error(handle->loop, UV_EFAULT);
    return -1;
  }

  return uv__tcp_bind6(handle, addr);
}

int uv_udp_bind(uv_udp_t* handle, struct sockaddr_in addr,
    unsigned int flags) {
  if (handle->type != UV_UDP || addr.sin_family != AF_INET) {
    uv__set_artificial_error(handle->loop, UV_EFAULT);
    return -1;
  }

  return uv__udp_bind(handle, addr, flags);
}

int uv_udp_bind6(uv_udp_t* handle, struct sockaddr_in6 addr,
    unsigned int flags) {
  if (handle->type != UV_UDP || addr.sin6_family != AF_INET6) {
    uv__set_artificial_error(handle->loop, UV_EFAULT);
    return -1;
  }

  return uv__udp_bind6(handle, addr, flags);
}

int uv_tcp_connect(uv_connect_t* req,
                   uv_tcp_t* handle,
                   struct sockaddr_in address,
                   uv_connect_cb cb) {
  if (handle->type != UV_TCP || address.sin_family != AF_INET) {
    uv__set_artificial_error(handle->loop, UV_EINVAL);
    return -1;
  }

  return uv__tcp_connect(req, handle, address, cb);
}

int uv_tcp_connect6(uv_connect_t* req,
                    uv_tcp_t* handle,
                    struct sockaddr_in6 address,
                    uv_connect_cb cb) {
  if (handle->type != UV_TCP || address.sin6_family != AF_INET6) {
    uv__set_artificial_error(handle->loop, UV_EINVAL);
    return -1;
  }

  return uv__tcp_connect6(req, handle, address, cb);
}
