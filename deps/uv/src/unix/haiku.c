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

#include "uv.h"
#include "internal.h"

#include <FindDirectory.h> /* find_path() */
#include <OS.h>


void uv_loadavg(double avg[3]) {
  avg[0] = 0;
  avg[1] = 0;
  avg[2] = 0;
}


int uv_exepath(char* buffer, size_t* size) {
  char abspath[B_PATH_NAME_LENGTH];
  status_t status;
  ssize_t abspath_len;

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  status = find_path(B_APP_IMAGE_SYMBOL, B_FIND_PATH_IMAGE_PATH, NULL, abspath,
                     sizeof(abspath));
  if (status != B_OK)
    return UV__ERR(status);

  abspath_len = uv__strscpy(buffer, abspath, *size);
  *size -= 1;
  if (abspath_len >= 0 && *size > (size_t)abspath_len)
    *size = (size_t)abspath_len;

  return 0;
}


uint64_t uv_get_free_memory(void) {
  status_t status;
  system_info sinfo;

  status = get_system_info(&sinfo);
  if (status != B_OK)
    return 0;

  return (sinfo.max_pages - sinfo.used_pages) * B_PAGE_SIZE;
}


uint64_t uv_get_total_memory(void) {
  status_t status;
  system_info sinfo;

  status = get_system_info(&sinfo);
  if (status != B_OK)
    return 0;

  return sinfo.max_pages * B_PAGE_SIZE;
}


uint64_t uv_get_constrained_memory(void) {
  return 0;  /* Memory constraints are unknown. */
}


int uv_resident_set_memory(size_t* rss) {
  area_info area;
  ssize_t cookie;
  status_t status;
  thread_info thread;

  status = get_thread_info(find_thread(NULL), &thread);
  if (status != B_OK)
    return UV__ERR(status);

  cookie = 0;
  *rss = 0;
  while (get_next_area_info(thread.team, &cookie, &area) == B_OK)
    *rss += area.ram_size;

  return 0;
}


int uv_uptime(double* uptime) {
  /* system_time() returns time since booting in microseconds */
  *uptime = (double)system_time() / 1000000;
  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  cpu_topology_node_info* topology_infos;
  int i;
  status_t status;
  system_info system;
  uint32_t topology_count;
  uint64_t cpuspeed;
  uv_cpu_info_t* cpu_info;

  if (cpu_infos == NULL || count == NULL)
    return UV_EINVAL;

  status = get_cpu_topology_info(NULL, &topology_count);
  if (status != B_OK)
    return UV__ERR(status);

  topology_infos = uv__malloc(topology_count * sizeof(*topology_infos));
  if (topology_infos == NULL)
    return UV_ENOMEM;

  status = get_cpu_topology_info(topology_infos, &topology_count);
  if (status != B_OK) {
    uv__free(topology_infos);
    return UV__ERR(status);
  }

  cpuspeed = 0;
  for (i = 0; i < (int)topology_count; i++) {
    if (topology_infos[i].type == B_TOPOLOGY_CORE) {
      cpuspeed = topology_infos[i].data.core.default_frequency;
      break;
    }
  }

  uv__free(topology_infos);

  status = get_system_info(&system);
  if (status != B_OK)
    return UV__ERR(status);

  *cpu_infos = uv__calloc(system.cpu_count, sizeof(**cpu_infos));
  if (*cpu_infos == NULL)
    return UV_ENOMEM;

  /* CPU time and model are not exposed by Haiku. */
  cpu_info = *cpu_infos;
  for (i = 0; i < (int)system.cpu_count; i++) {
    cpu_info->model = uv__strdup("unknown");
    cpu_info->speed = (int)(cpuspeed / 1000000);
    cpu_info++;
  }
  *count = system.cpu_count;

  return 0;
}

void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++)
    uv__free(cpu_infos[i].model);

  uv__free(cpu_infos);
}
