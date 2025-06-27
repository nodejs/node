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

#include "task.h"
#include "uv.h"


BENCHMARK_IMPL(sizes) {
  fprintf(stderr, "uv_shutdown_t: %u bytes\n", (unsigned int) sizeof(uv_shutdown_t));
  fprintf(stderr, "uv_write_t: %u bytes\n", (unsigned int) sizeof(uv_write_t));
  fprintf(stderr, "uv_connect_t: %u bytes\n", (unsigned int) sizeof(uv_connect_t));
  fprintf(stderr, "uv_udp_send_t: %u bytes\n", (unsigned int) sizeof(uv_udp_send_t));
  fprintf(stderr, "uv_tcp_t: %u bytes\n", (unsigned int) sizeof(uv_tcp_t));
  fprintf(stderr, "uv_pipe_t: %u bytes\n", (unsigned int) sizeof(uv_pipe_t));
  fprintf(stderr, "uv_tty_t: %u bytes\n", (unsigned int) sizeof(uv_tty_t));
  fprintf(stderr, "uv_prepare_t: %u bytes\n", (unsigned int) sizeof(uv_prepare_t));
  fprintf(stderr, "uv_check_t: %u bytes\n", (unsigned int) sizeof(uv_check_t));
  fprintf(stderr, "uv_idle_t: %u bytes\n", (unsigned int) sizeof(uv_idle_t));
  fprintf(stderr, "uv_async_t: %u bytes\n", (unsigned int) sizeof(uv_async_t));
  fprintf(stderr, "uv_timer_t: %u bytes\n", (unsigned int) sizeof(uv_timer_t));
  fprintf(stderr, "uv_fs_poll_t: %u bytes\n", (unsigned int) sizeof(uv_fs_poll_t));
  fprintf(stderr, "uv_fs_event_t: %u bytes\n", (unsigned int) sizeof(uv_fs_event_t));
  fprintf(stderr, "uv_process_t: %u bytes\n", (unsigned int) sizeof(uv_process_t));
  fprintf(stderr, "uv_poll_t: %u bytes\n", (unsigned int) sizeof(uv_poll_t));
  fprintf(stderr, "uv_loop_t: %u bytes\n", (unsigned int) sizeof(uv_loop_t));
  fflush(stderr);
  return 0;
}
