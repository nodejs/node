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

TEST_DECLARE   (ping_pong)
TEST_DECLARE   (delayed_accept)
TEST_DECLARE   (tcp_writealot)
TEST_DECLARE   (bind_error_addrinuse)
TEST_DECLARE   (bind_error_addrnotavail_1)
TEST_DECLARE   (bind_error_addrnotavail_2)
TEST_DECLARE   (bind_error_fault)
TEST_DECLARE   (bind_error_inval)
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
TEST_DECLARE   (fail_always)
TEST_DECLARE   (pass_always)
HELPER_DECLARE (echo_server)

TASK_LIST_START
  TEST_ENTRY  (ping_pong)
  TEST_HELPER (ping_pong, echo_server)

  TEST_ENTRY  (delayed_accept)

  TEST_ENTRY  (tcp_writealot)
  TEST_HELPER (tcp_writealot, echo_server)

  TEST_ENTRY  (bind_error_addrinuse)

  TEST_ENTRY  (bind_error_addrnotavail_1)

  TEST_ENTRY  (bind_error_addrnotavail_2)

  TEST_ENTRY  (bind_error_fault)

  TEST_ENTRY  (bind_error_inval)

  TEST_ENTRY  (connection_fail)
  TEST_ENTRY  (connection_fail_doesnt_auto_close)

  TEST_ENTRY  (shutdown_eof)
  TEST_HELPER (shutdown_eof, echo_server)

  TEST_ENTRY  (callback_stack)
  TEST_HELPER (callback_stack, echo_server)

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

#if 0
  /* These are for testing the test runner. */
  TEST_ENTRY  (fail_always)
  TEST_ENTRY  (pass_always)
#endif
TASK_LIST_END

