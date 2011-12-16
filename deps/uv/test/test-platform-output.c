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
#include <string.h>


TEST_IMPL(platform_output) {
  char buffer[512];
  size_t rss;
  double uptime;
  uv_cpu_info_t* cpus;
  uv_interface_address_t* interfaces;
  int count;
  int i;
  uv_err_t err;

  err = uv_get_process_title(buffer, sizeof(buffer));
  ASSERT(UV_OK == err.code);
  fprintf(stderr, "uv_get_process_title: %s\n", buffer);

  err = uv_resident_set_memory(&rss);
  ASSERT(UV_OK == err.code);
  fprintf(stderr, "uv_resident_set_memory: %d\n", rss);

  err = uv_uptime(&uptime);
  ASSERT(UV_OK == err.code);
  fprintf(stderr, "uv_uptime: %f\n", uptime);

  err = uv_cpu_info(&cpus, &count);
  ASSERT(UV_OK == err.code);

  fprintf(stderr, "uv_cpu_info:\n");
  for (i = 0; i < count; i++) {
    fprintf(stderr, "  model: %s\n", cpus[i].model);
    fprintf(stderr, "  speed: %d\n", cpus[i].speed);
    fprintf(stderr, "  times.sys: %llu\n", cpus[i].cpu_times.sys);
    fprintf(stderr, "  times.user: %llu\n", cpus[i].cpu_times.user);
    fprintf(stderr, "  times.idle: %llu\n", cpus[i].cpu_times.idle);
    fprintf(stderr, "  times.irq: %llu\n", cpus[i].cpu_times.irq);
    fprintf(stderr, "  times.nice: %llu\n", cpus[i].cpu_times.nice);
  }
  uv_free_cpu_info(cpus, count);

  err = uv_interface_addresses(&interfaces, &count);
  ASSERT(UV_OK == err.code);

  fprintf(stderr, "uv_interface_addresses:\n");
  for (i = 0; i < count; i++) {
    fprintf(stderr, "  name: %s\n", interfaces[i].name);
    fprintf(stderr, "  internal: %d\n", interfaces[i].is_internal);

    if (interfaces[i].address.address4.sin_family == AF_INET) {
      uv_ip4_name(&interfaces[i].address.address4, buffer, sizeof(buffer));
    } else if (interfaces[i].address.address4.sin_family == AF_INET6) {
      uv_ip6_name(&interfaces[i].address.address6, buffer, sizeof(buffer));
    }

    fprintf(stderr, "  address: %s\n", buffer);
  }
  uv_free_interface_addresses(interfaces, count);

  return 0;
}
