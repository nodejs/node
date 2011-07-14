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

TEST_DECLARE   (tcp_ping_pong)
TEST_DECLARE   (tcp_ping_pong_v6)
TEST_DECLARE   (pipe_ping_pong)
TEST_DECLARE   (delayed_accept)
TEST_DECLARE   (tcp_writealot)
TEST_DECLARE   (bind_error_addrinuse)
TEST_DECLARE   (bind_error_addrnotavail_1)
TEST_DECLARE   (bind_error_addrnotavail_2)
TEST_DECLARE   (bind_error_fault)
TEST_DECLARE   (bind_error_inval)
TEST_DECLARE   (bind_localhost_ok)
TEST_DECLARE   (bind6_error_addrinuse)
TEST_DECLARE   (bind6_error_addrnotavail)
TEST_DECLARE   (bind6_error_fault)
TEST_DECLARE   (bind6_error_inval)
TEST_DECLARE   (bind6_localhost_ok)
TEST_DECLARE   (connection_fail)
TEST_DECLARE   (connection_fail_doesnt_auto_close)
TEST_DECLARE   (shutdown_eof)
TEST_DECLARE   (callback_stack)
TEST_DECLARE   (timer)
TEST_DECLARE   (timer_again)
TEST_DECLARE   (loop_handles)
TEST_DECLARE   (ref)
TEST_DECLARE   (idle_ref)
TEST_DECLARE   (async_ref)
TEST_DECLARE   (prepare_ref)
TEST_DECLARE   (check_ref)
TEST_DECLARE   (async)
TEST_DECLARE   (get_currentexe)
TEST_DECLARE   (hrtime)
TEST_DECLARE   (getaddrinfo_basic)
TEST_DECLARE   (getaddrinfo_concurrent)
TEST_DECLARE   (gethostbyname)
TEST_DECLARE   (getsockname)
TEST_DECLARE   (fail_always)
TEST_DECLARE   (pass_always)
HELPER_DECLARE (tcp4_echo_server)
HELPER_DECLARE (tcp6_echo_server)
HELPER_DECLARE (pipe_echo_server)


TASK_LIST_START
  TEST_ENTRY  (tcp_ping_pong)
  TEST_HELPER (tcp_ping_pong, tcp4_echo_server)

  TEST_ENTRY  (tcp_ping_pong_v6)
  TEST_HELPER (tcp_ping_pong_v6, tcp6_echo_server)

  TEST_ENTRY  (pipe_ping_pong)
  TEST_HELPER (pipe_ping_pong, pipe_echo_server)

  TEST_ENTRY  (delayed_accept)

  TEST_ENTRY  (tcp_writealot)
  TEST_HELPER (tcp_writealot, tcp4_echo_server)

  TEST_ENTRY  (bind_error_addrinuse)
  TEST_ENTRY  (bind_error_addrnotavail_1)
  TEST_ENTRY  (bind_error_addrnotavail_2)
  TEST_ENTRY  (bind_error_fault)
  TEST_ENTRY  (bind_error_inval)
  TEST_ENTRY  (bind_localhost_ok)

  TEST_ENTRY  (bind6_error_addrinuse)
  TEST_ENTRY  (bind6_error_addrnotavail)
  TEST_ENTRY  (bind6_error_fault)
  TEST_ENTRY  (bind6_error_inval)
  TEST_ENTRY  (bind6_localhost_ok)

  TEST_ENTRY  (connection_fail)
  TEST_ENTRY  (connection_fail_doesnt_auto_close)

  TEST_ENTRY  (shutdown_eof)
  TEST_HELPER (shutdown_eof, tcp4_echo_server)

  TEST_ENTRY  (callback_stack)
  TEST_HELPER (callback_stack, tcp4_echo_server)

  TEST_ENTRY  (timer)

  TEST_ENTRY  (timer_again)

  TEST_ENTRY  (ref)
  TEST_ENTRY  (idle_ref)
  TEST_ENTRY  (async_ref)
  TEST_ENTRY  (prepare_ref)
  TEST_ENTRY  (check_ref)

  TEST_ENTRY  (loop_handles)

  TEST_ENTRY  (async)

  TEST_ENTRY  (get_currentexe)

  TEST_ENTRY  (hrtime)

  TEST_ENTRY  (getaddrinfo_basic)
  TEST_ENTRY  (getaddrinfo_concurrent)

  TEST_ENTRY  (gethostbyname)
  TEST_HELPER (gethostbyname, tcp4_echo_server)

  TEST_ENTRY  (getsockname)

#if 0
  /* These are for testing the test runner. */
  TEST_ENTRY  (fail_always)
  TEST_ENTRY  (pass_always)
#endif
TASK_LIST_END

