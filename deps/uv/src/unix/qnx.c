/* Copyright libuv contributors. All rights reserved.
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
#include "internal.h"

#include <string.h>
#include <sys/process.h>
#include <sys/neutrino.h>
#include <sys/memmsg.h>
#include <sys/syspage.h>
#include <sys/procfs.h>

static void
get_mem_info(uint64_t* totalmem, uint64_t* freemem) {
  mem_info_t msg;

  memset(&msg, 0, sizeof(msg));
  msg.i.type = _MEM_INFO;
  msg.i.fd = -1;

  if (MsgSend(MEMMGR_COID, &msg.i, sizeof(msg.i), &msg.o, sizeof(msg.o))
      != -1) {
    *totalmem = msg.o.info.__posix_tmi_total;
    *freemem = msg.o.info.posix_tmi_length;
  } else {
    *totalmem = 0;
    *freemem = 0;
  }
}


void uv_loadavg(double avg[3]) {
  avg[0] = 0.0;
  avg[1] = 0.0;
  avg[2] = 0.0;
}


int uv_exepath(char* buffer, size_t* size) {
  char path[PATH_MAX];
  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  realpath(_cmdname(NULL), path);
  strlcpy(buffer, path, *size);
  *size = strlen(buffer);
  return 0;
}


uint64_t uv_get_free_memory(void) {
  uint64_t totalmem;
  uint64_t freemem;
  get_mem_info(&totalmem, &freemem);
  return freemem;
}


uint64_t uv_get_total_memory(void) {
  uint64_t totalmem;
  uint64_t freemem;
  get_mem_info(&totalmem, &freemem);
  return totalmem;
}


uint64_t uv_get_constrained_memory(void) {
  return 0;
}


uint64_t uv_get_available_memory(void) {
  return uv_get_free_memory();
}


int uv_resident_set_memory(size_t* rss) {
  int fd;
  procfs_asinfo asinfo;

  fd = uv__open_cloexec("/proc/self/ctl", O_RDONLY);
  if (fd == -1)
    return UV__ERR(errno);

  if (devctl(fd, DCMD_PROC_ASINFO, &asinfo, sizeof(asinfo), 0) == -1) {
    uv__close(fd);
    return UV__ERR(errno);
  }

  uv__close(fd);
  *rss = asinfo.rss;
  return 0;
}


int uv_uptime(double* uptime) {
  struct qtime_entry* qtime = _SYSPAGE_ENTRY(_syspage_ptr, qtime);
  *uptime = (qtime->nsec / 1000000000.0);
  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  struct cpuinfo_entry* cpuinfo =
    (struct cpuinfo_entry*)_SYSPAGE_ENTRY(_syspage_ptr, new_cpuinfo);
  size_t cpuinfo_size = _SYSPAGE_ELEMENT_SIZE(_syspage_ptr, cpuinfo);
  struct strings_entry* strings = _SYSPAGE_ENTRY(_syspage_ptr, strings);
  int num_cpus = _syspage_ptr->num_cpu;
  int i;

  *count = num_cpus;
  *cpu_infos = uv__malloc(num_cpus * sizeof(**cpu_infos));
  if (*cpu_infos == NULL)
    return UV_ENOMEM;

  for (i = 0; i < num_cpus; i++) {
    (*cpu_infos)[i].model = strdup(&strings->data[cpuinfo->name]);
    (*cpu_infos)[i].speed = cpuinfo->speed;
    SYSPAGE_ARRAY_ADJ_OFFSET(cpuinfo, cpuinfo, cpuinfo_size);
  }

  return 0;
}
