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
BENCHMARK_DECLARE (tcp4_pound_100)
BENCHMARK_DECLARE (tcp4_pound_1000)
BENCHMARK_DECLARE (pipe_pound_100)
BENCHMARK_DECLARE (pipe_pound_1000)
BENCHMARK_DECLARE (tcp_pump100_client)
BENCHMARK_DECLARE (tcp_pump1_client)
BENCHMARK_DECLARE (pipe_pump100_client)
BENCHMARK_DECLARE (pipe_pump1_client)
BENCHMARK_DECLARE (udp_packet_storm_1v1)
BENCHMARK_DECLARE (udp_packet_storm_1v10)
BENCHMARK_DECLARE (udp_packet_storm_1v100)
BENCHMARK_DECLARE (udp_packet_storm_1v1000)
BENCHMARK_DECLARE (udp_packet_storm_10v10)
BENCHMARK_DECLARE (udp_packet_storm_10v100)
BENCHMARK_DECLARE (udp_packet_storm_10v1000)
BENCHMARK_DECLARE (udp_packet_storm_100v100)
BENCHMARK_DECLARE (udp_packet_storm_100v1000)
BENCHMARK_DECLARE (udp_packet_storm_1000v1000)
BENCHMARK_DECLARE (gethostbyname)
BENCHMARK_DECLARE (getaddrinfo)
BENCHMARK_DECLARE (spawn)
HELPER_DECLARE    (tcp_pump_server)
HELPER_DECLARE    (pipe_pump_server)
HELPER_DECLARE    (tcp4_echo_server)
HELPER_DECLARE    (pipe_echo_server)
HELPER_DECLARE    (dns_server)

TASK_LIST_START
  BENCHMARK_ENTRY  (sizes)

  BENCHMARK_ENTRY  (ping_pongs)
  BENCHMARK_HELPER (ping_pongs, tcp4_echo_server)

  BENCHMARK_ENTRY  (tcp_pump100_client)
  BENCHMARK_HELPER (tcp_pump100_client, tcp_pump_server)

  BENCHMARK_ENTRY  (tcp_pump1_client)
  BENCHMARK_HELPER (tcp_pump1_client, tcp_pump_server)

  BENCHMARK_ENTRY  (tcp4_pound_100)
  BENCHMARK_HELPER (tcp4_pound_100, tcp4_echo_server)

  BENCHMARK_ENTRY  (tcp4_pound_1000)
  BENCHMARK_HELPER (tcp4_pound_1000, tcp4_echo_server)

  BENCHMARK_ENTRY  (pipe_pump100_client)
  BENCHMARK_HELPER (pipe_pump100_client, pipe_pump_server)

  BENCHMARK_ENTRY  (pipe_pump1_client)
  BENCHMARK_HELPER (pipe_pump1_client, pipe_pump_server)

  BENCHMARK_ENTRY  (pipe_pound_100)
  BENCHMARK_HELPER (pipe_pound_100, pipe_echo_server)

  BENCHMARK_ENTRY  (pipe_pound_1000)
  BENCHMARK_HELPER (pipe_pound_1000, pipe_echo_server)

  BENCHMARK_ENTRY  (udp_packet_storm_1v1)
  BENCHMARK_ENTRY  (udp_packet_storm_1v10)
  BENCHMARK_ENTRY  (udp_packet_storm_1v100)
  BENCHMARK_ENTRY  (udp_packet_storm_1v1000)
  BENCHMARK_ENTRY  (udp_packet_storm_10v10)
  BENCHMARK_ENTRY  (udp_packet_storm_10v100)
  BENCHMARK_ENTRY  (udp_packet_storm_10v1000)
  BENCHMARK_ENTRY  (udp_packet_storm_100v100)
  BENCHMARK_ENTRY  (udp_packet_storm_100v1000)
  BENCHMARK_ENTRY  (udp_packet_storm_1000v1000)

  BENCHMARK_ENTRY  (gethostbyname)
  BENCHMARK_HELPER (gethostbyname, dns_server)

  BENCHMARK_ENTRY  (getaddrinfo)

  BENCHMARK_ENTRY  (spawn)
TASK_LIST_END
