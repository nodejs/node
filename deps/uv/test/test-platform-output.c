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
  size_t size;
  double uptime;
  uv_pid_t pid;
  uv_pid_t ppid;
  uv_rusage_t rusage;
  uv_cpu_info_t* cpus;
  uv_interface_address_t* interfaces;
  uv_passwd_t pwd;
  uv_utsname_t uname;
  int count;
  int i;
  int err;

  err = uv_get_process_title(buffer, sizeof(buffer));
  ASSERT(err == 0);
  printf("uv_get_process_title: %s\n", buffer);

  size = sizeof(buffer);
  err = uv_cwd(buffer, &size);
  ASSERT(err == 0);
  printf("uv_cwd: %s\n", buffer);

  err = uv_resident_set_memory(&rss);
#if defined(__MSYS__)
  ASSERT(err == UV_ENOSYS);
#else
  ASSERT(err == 0);
  printf("uv_resident_set_memory: %llu\n", (unsigned long long) rss);
#endif

  err = uv_uptime(&uptime);
#if defined(__PASE__)
  ASSERT(err == UV_ENOSYS);
#else
  ASSERT(err == 0);
  ASSERT(uptime > 0);
  printf("uv_uptime: %f\n", uptime);
#endif

  err = uv_getrusage(&rusage);
  ASSERT(err == 0);
  ASSERT(rusage.ru_utime.tv_sec >= 0);
  ASSERT(rusage.ru_utime.tv_usec >= 0);
  ASSERT(rusage.ru_stime.tv_sec >= 0);
  ASSERT(rusage.ru_stime.tv_usec >= 0);
  printf("uv_getrusage:\n");
  printf("  user: %llu sec %llu microsec\n",
         (unsigned long long) rusage.ru_utime.tv_sec,
         (unsigned long long) rusage.ru_utime.tv_usec);
  printf("  system: %llu sec %llu microsec\n",
         (unsigned long long) rusage.ru_stime.tv_sec,
         (unsigned long long) rusage.ru_stime.tv_usec);
  printf("  page faults: %llu\n", (unsigned long long) rusage.ru_majflt);
  printf("  maximum resident set size: %llu\n",
         (unsigned long long) rusage.ru_maxrss);

  err = uv_cpu_info(&cpus, &count);
#if defined(__CYGWIN__) || defined(__MSYS__)
  ASSERT(err == UV_ENOSYS);
#else
  ASSERT(err == 0);

  printf("uv_cpu_info:\n");
  for (i = 0; i < count; i++) {
    printf("  model: %s\n", cpus[i].model);
    printf("  speed: %d\n", cpus[i].speed);
    printf("  times.sys: %llu\n", (unsigned long long) cpus[i].cpu_times.sys);
    printf("  times.user: %llu\n",
           (unsigned long long) cpus[i].cpu_times.user);
    printf("  times.idle: %llu\n",
           (unsigned long long) cpus[i].cpu_times.idle);
    printf("  times.irq: %llu\n",  (unsigned long long) cpus[i].cpu_times.irq);
    printf("  times.nice: %llu\n",
           (unsigned long long) cpus[i].cpu_times.nice);
  }
#endif
  uv_free_cpu_info(cpus, count);

  err = uv_interface_addresses(&interfaces, &count);
  ASSERT(err == 0);

  printf("uv_interface_addresses:\n");
  for (i = 0; i < count; i++) {
    printf("  name: %s\n", interfaces[i].name);
    printf("  internal: %d\n", interfaces[i].is_internal);
    printf("  physical address: ");
    printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
           (unsigned char)interfaces[i].phys_addr[0],
           (unsigned char)interfaces[i].phys_addr[1],
           (unsigned char)interfaces[i].phys_addr[2],
           (unsigned char)interfaces[i].phys_addr[3],
           (unsigned char)interfaces[i].phys_addr[4],
           (unsigned char)interfaces[i].phys_addr[5]);

    if (interfaces[i].address.address4.sin_family == AF_INET) {
      uv_ip4_name(&interfaces[i].address.address4, buffer, sizeof(buffer));
    } else if (interfaces[i].address.address4.sin_family == AF_INET6) {
      uv_ip6_name(&interfaces[i].address.address6, buffer, sizeof(buffer));
    }

    printf("  address: %s\n", buffer);

    if (interfaces[i].netmask.netmask4.sin_family == AF_INET) {
      uv_ip4_name(&interfaces[i].netmask.netmask4, buffer, sizeof(buffer));
      printf("  netmask: %s\n", buffer);
    } else if (interfaces[i].netmask.netmask4.sin_family == AF_INET6) {
      uv_ip6_name(&interfaces[i].netmask.netmask6, buffer, sizeof(buffer));
      printf("  netmask: %s\n", buffer);
    } else {
      printf("  netmask: none\n");
    }
  }
  uv_free_interface_addresses(interfaces, count);

  err = uv_os_get_passwd(&pwd);
  ASSERT(err == 0);

  printf("uv_os_get_passwd:\n");
  printf("  euid: %ld\n", pwd.uid);
  printf("  gid: %ld\n", pwd.gid);
  printf("  username: %s\n", pwd.username);
  printf("  shell: %s\n", pwd.shell);
  printf("  home directory: %s\n", pwd.homedir);

  pid = uv_os_getpid();
  ASSERT(pid > 0);
  printf("uv_os_getpid: %d\n", (int) pid);
  ppid = uv_os_getppid();
  ASSERT(ppid > 0);
  printf("uv_os_getppid: %d\n", (int) ppid);

  err = uv_os_uname(&uname);
  ASSERT(err == 0);
  printf("uv_os_uname:\n");
  printf("  sysname: %s\n", uname.sysname);
  printf("  release: %s\n", uname.release);
  printf("  version: %s\n", uname.version);
  printf("  machine: %s\n", uname.machine);

  return 0;
}
