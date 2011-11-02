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

#include "uv.h"

#define COUNTOF(a) (sizeof(a) / sizeof(a[0]))


struct uv_ares_task_s {
  UV_HANDLE_FIELDS
  UV_ARES_TASK_PRIVATE_FIELDS
  uv_ares_task_t* ares_prev;
  uv_ares_task_t* ares_next;
};


void uv_remove_ares_handle(uv_ares_task_t* handle);
uv_ares_task_t* uv_find_ares_handle(uv_loop_t*, ares_socket_t sock);

/* TODO Rename to uv_ares_task_init? */
void uv_add_ares_handle(uv_loop_t* loop, uv_ares_task_t* handle);

int uv_ares_handles_empty(uv_loop_t* loop);

extern const uv_err_t uv_ok_;

uv_err_code uv_translate_sys_error(int sys_errno);
void uv__set_error(uv_loop_t* loop, uv_err_code code, int sys_error);
void uv__set_sys_error(uv_loop_t* loop, int sys_error);
void uv__set_artificial_error(uv_loop_t* loop, uv_err_code code);
uv_err_t uv__new_sys_error(int sys_error);

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


#endif /* UV_COMMON_H_ */
