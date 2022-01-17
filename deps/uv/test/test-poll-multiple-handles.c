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

#include <errno.h>

#ifndef _WIN32
# include <fcntl.h>
# include <sys/socket.h>
# include <unistd.h>
#endif

#include "uv.h"
#include "task.h"


static int close_cb_called = 0;


static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}

static void poll_cb(uv_poll_t* handle, int status, int events) {
  /* Not a bound socket, linux immediately reports UV_READABLE, other OS do not */
  ASSERT(events == UV_READABLE);
}

TEST_IMPL(poll_multiple_handles) {
  uv_os_sock_t sock;
  uv_poll_t first_poll_handle, second_poll_handle;

#ifdef _WIN32
  {
    struct WSAData wsa_data;
    int r = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    ASSERT(r == 0);
  }
#endif

  sock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
  ASSERT(sock != INVALID_SOCKET);
#else
  ASSERT(sock != -1);
#endif
  ASSERT(0 == uv_poll_init_socket(uv_default_loop(), &first_poll_handle, sock));
  ASSERT(0 == uv_poll_init_socket(uv_default_loop(), &second_poll_handle, sock));

  ASSERT(0 == uv_poll_start(&first_poll_handle, UV_READABLE, poll_cb));

  /* We may not start polling while another polling handle is active
   * on that fd.
   */
#ifndef _WIN32
  /* We do not track handles in an O(1) lookupable way on Windows,
   * so not checking that here.
   */
  ASSERT(uv_poll_start(&second_poll_handle, UV_READABLE, poll_cb) == UV_EEXIST);
#endif

  /* After stopping the other polling handle, we now should be able to poll */
  ASSERT(0 == uv_poll_stop(&first_poll_handle));
  ASSERT(0 == uv_poll_start(&second_poll_handle, UV_READABLE, poll_cb));

  /* Closing an already stopped polling handle is safe in any case */
  uv_close((uv_handle_t*) &first_poll_handle, close_cb);

  uv_unref((uv_handle_t*) &second_poll_handle);
  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT(close_cb_called == 1);
  uv_ref((uv_handle_t*) &second_poll_handle);

  ASSERT(uv_is_active((uv_handle_t*) &second_poll_handle));
  uv_close((uv_handle_t*) &second_poll_handle, close_cb);

  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT(close_cb_called == 2);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
