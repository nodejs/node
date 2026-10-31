/* Copyright libuv project contributors. All rights reserved.
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

#include <string.h>


TEST_IMPL(udp_send_fail_nbufs) {
  struct sockaddr_in addr;
  uv_udp_send_t req;
  uv_udp_t client;
  uv_buf_t buf;
  int r;

  ASSERT_OK(uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));
  ASSERT_OK(uv_udp_init(uv_default_loop(), &client));

  buf = uv_buf_init("PING", 4);

  /* nbufs=0 should be rejected. */
  r = uv_udp_send(&req,
                   &client,
                   &buf,
                   0,
                   (const struct sockaddr*) &addr,
                   NULL);
  ASSERT_EQ(UV_EINVAL, r);

  /* Negative nbufs undergoes sign conversion to a large unsigned value. */
  r = uv_udp_send(&req,
                   &client,
                   &buf,
                   -1,
                   (const struct sockaddr*) &addr,
                   NULL);
  ASSERT_EQ(UV_EINVAL, r);

  /* Same checks for uv_udp_try_send. */
  r = uv_udp_try_send(&client, &buf, 0, (const struct sockaddr*) &addr);
  ASSERT_EQ(UV_EINVAL, r);

  r = uv_udp_try_send(&client, &buf, -1, (const struct sockaddr*) &addr);
  ASSERT_EQ(UV_EINVAL, r);

  uv_close((uv_handle_t*) &client, NULL);
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  MAKE_VALGRIND_HAPPY(uv_default_loop());
  return 0;
}
