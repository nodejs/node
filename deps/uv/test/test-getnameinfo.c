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
#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const char* address_ip4 = "127.0.0.1";
static const char* address_ip6 = "::1";
static const int port = 80;

static struct sockaddr_in addr4;
static struct sockaddr_in6 addr6;
static uv_getnameinfo_t req;

static void getnameinfo_req(uv_getnameinfo_t* handle,
                            int status,
                            const char* hostname,
                            const char* service) {
  ASSERT(handle != NULL);
  ASSERT(status == 0);
  ASSERT(hostname != NULL);
  ASSERT(service != NULL);
}


TEST_IMPL(getnameinfo_basic_ip4) {
/* TODO(gengjiawen): Fix test on QEMU. */
#if defined(__QEMU__)
  RETURN_SKIP("Test does not currently work in QEMU");
#endif

  int r;

  r = uv_ip4_addr(address_ip4, port, &addr4);
  ASSERT(r == 0);

  r = uv_getnameinfo(uv_default_loop(),
                     &req,
                     &getnameinfo_req,
                     (const struct sockaddr*)&addr4,
                     0);
  ASSERT(r == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(getnameinfo_basic_ip4_sync) {
/* TODO(gengjiawen): Fix test on QEMU. */
#if defined(__QEMU__)
  RETURN_SKIP("Test does not currently work in QEMU");
#endif

  ASSERT(0 == uv_ip4_addr(address_ip4, port, &addr4));

  ASSERT(0 == uv_getnameinfo(uv_default_loop(),
                             &req,
                             NULL,
                             (const struct sockaddr*)&addr4,
                             0));
  ASSERT(req.host[0] != '\0');
  ASSERT(req.service[0] != '\0');

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(getnameinfo_basic_ip6) {
/* TODO(gengjiawen): Fix test on QEMU. */
#if defined(__QEMU__)
  RETURN_SKIP("Test does not currently work in QEMU");
#endif
  
  int r;

  r = uv_ip6_addr(address_ip6, port, &addr6);
  ASSERT(r == 0);

  r = uv_getnameinfo(uv_default_loop(),
                     &req,
                     &getnameinfo_req,
                     (const struct sockaddr*)&addr6,
                     0);
  ASSERT(r == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
