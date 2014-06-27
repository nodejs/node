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
#include <malloc.h>
#include <stdio.h>

#include "uv.h"
#include "internal.h"
#include "req-inl.h"


/* getnameinfo worker thread implementation */
static DWORD WINAPI getnameinfo_thread_proc(void* parameter) {
  uv_getnameinfo_t* req = (uv_getnameinfo_t*)parameter;
  uv_loop_t* loop = req->loop;
  WCHAR host[NI_MAXHOST];
  WCHAR service[NI_MAXSERV];
  int ret = 0;

  assert(req != NULL);

  ret = GetNameInfoW((struct sockaddr*)&req->storage,
                     sizeof(req->storage),
                     host,
                     sizeof(host),
                     service,
                     sizeof(service),
                     req->flags);
  req->retcode = uv__getaddrinfo_translate_error(ret);

  /* convert results to UTF-8 */
  WideCharToMultiByte(CP_UTF8,
                      0,
                      host,
                      -1,
                      req->host,
                      sizeof(req->host),
                      NULL,
                      NULL);

  WideCharToMultiByte(CP_UTF8,
                      0,
                      service,
                      -1,
                      req->service,
                      sizeof(req->service),
                      NULL,
                      NULL);

  /* post getnameinfo completed */
  POST_COMPLETION_FOR_REQ(loop, req);

  return 0;
}


/*
* Called from uv_run when complete.
*/
void uv_process_getnameinfo_req(uv_loop_t* loop, uv_getnameinfo_t* req) {
  char* host;
  char* service;

  if (req->retcode == 0) {
    host = req->host;
    service = req->service;
  } else {
    host = NULL;
    service = NULL;
  }

  uv__req_unregister(loop, req);
  req->getnameinfo_cb(req, req->retcode, host, service);
}


/*
* Entry point for getnameinfo
* return 0 if a callback will be made
* return error code if validation fails
*/
int uv_getnameinfo(uv_loop_t* loop,
                   uv_getnameinfo_t* req,
                   uv_getnameinfo_cb getnameinfo_cb,
                   const struct sockaddr* addr,
                   int flags) {
  if (req == NULL || getnameinfo_cb == NULL || addr == NULL)
    return UV_EINVAL;

  if (addr->sa_family == AF_INET) {
    memcpy(&req->storage,
           addr,
           sizeof(struct sockaddr_in));
  } else if (addr->sa_family == AF_INET6) {
    memcpy(&req->storage,
           addr,
           sizeof(struct sockaddr_in6));
  } else {
    return UV_EINVAL;
  }

  uv_req_init(loop, (uv_req_t*)req);

  req->getnameinfo_cb = getnameinfo_cb;
  req->flags = flags;
  req->type = UV_GETNAMEINFO;
  req->loop = loop;

  /* Ask thread to run. Treat this as a long operation. */
  if (QueueUserWorkItem(&getnameinfo_thread_proc,
                        req,
                        WT_EXECUTELONGFUNCTION) == 0) {
    return uv_translate_sys_error(GetLastError());
  }

  uv__req_register(loop, req);

  return 0;
}
