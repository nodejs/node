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


TEST_IMPL(udp_options) {
  static int invalid_ttls[] = { -1, 0, 256 };
  uv_loop_t* loop;
  uv_udp_t h;
  int i, r;

  loop = uv_default_loop();

  r = uv_udp_init(loop, &h);
  ASSERT(r == 0);

  uv_unref((uv_handle_t*)&h); /* don't keep the loop alive */

  r = uv_udp_bind(&h, uv_ip4_addr("0.0.0.0", TEST_PORT), 0);
  ASSERT(r == 0);

  r = uv_udp_set_broadcast(&h, 1);
  r |= uv_udp_set_broadcast(&h, 1);
  r |= uv_udp_set_broadcast(&h, 0);
  r |= uv_udp_set_broadcast(&h, 0);
  ASSERT(r == 0);

  /* values 1-255 should work */
  for (i = 1; i <= 255; i++) {
    r = uv_udp_set_ttl(&h, i);
    ASSERT(r == 0);
  }

  for (i = 0; i < (int) ARRAY_SIZE(invalid_ttls); i++) {
    r = uv_udp_set_ttl(&h, invalid_ttls[i]);
    ASSERT(r == -1);
    ASSERT(uv_last_error(loop).code == UV_EINVAL);
  }

  r = uv_udp_set_multicast_loop(&h, 1);
  r |= uv_udp_set_multicast_loop(&h, 1);
  r |= uv_udp_set_multicast_loop(&h, 0);
  r |= uv_udp_set_multicast_loop(&h, 0);
  ASSERT(r == 0);

  /* values 0-255 should work */
  for (i = 0; i <= 255; i++) {
    r = uv_udp_set_multicast_ttl(&h, i);
    ASSERT(r == 0);
  }

  /* anything >255 should fail */
  r = uv_udp_set_multicast_ttl(&h, 256);
  ASSERT(r == -1);
  ASSERT(uv_last_error(loop).code == UV_EINVAL);
  /* don't test ttl=-1, it's a valid value on some platforms */

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
