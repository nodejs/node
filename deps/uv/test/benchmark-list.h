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

BENCHMARK_DECLARE (sizes)
BENCHMARK_DECLARE (ping_pongs)
BENCHMARK_DECLARE (pump100_client)
BENCHMARK_DECLARE (pump1_client)
BENCHMARK_DECLARE (gethostbyname)
BENCHMARK_DECLARE (getaddrinfo)
HELPER_DECLARE    (pump_server)
HELPER_DECLARE    (echo_server)
HELPER_DECLARE    (dns_server)

TASK_LIST_START
  BENCHMARK_ENTRY  (sizes)

  BENCHMARK_ENTRY  (ping_pongs)
  BENCHMARK_HELPER (ping_pongs, echo_server)

  BENCHMARK_ENTRY  (pump100_client)
  BENCHMARK_HELPER (pump100_client, pump_server)

  BENCHMARK_ENTRY  (pump1_client)
  BENCHMARK_HELPER (pump1_client, pump_server)

  BENCHMARK_ENTRY  (gethostbyname)
  BENCHMARK_HELPER (gethostbyname, dns_server)

  BENCHMARK_ENTRY  (getaddrinfo)
TASK_LIST_END
