#include "uv.h"
#include "internal.h"

#include <assert.h>
#include <string.h>

#include <zircon/syscalls.h>

int uv_exepath(char* buffer, size_t* size) {
  if (buffer == NULL || size == NULL || *size == 0) return UV_EINVAL;
  const char* path = "/pkg/";
  if (*size < strlen(path) + 1) return UV_EINVAL;
  strcpy(buffer, "/pkg/");
  return 0;
}

void uv_loadavg(double avg[3]) {
  // Not implemented. As in the case of Windows, it returns [0, 0, 0].
  avg[0] = avg[1] = avg[2] = 0;
}

int uv_uptime(double* uptime) {
  if (uptime == NULL) return UV_EINVAL;
  // TODO(victor): This is the number of nanoseconds since the system was
  // powered on. It does not always reset on reboot and does not adjust during
  // sleep, and thus should not be used as a reliable source of uptime.
  zx_time_t time_ns = zx_clock_get_monotonic();
  *uptime = time_ns / 1000000000.0;  // in seconds
  return 0;
}

int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  *cpu_infos = NULL;
  *count = 0;
  return UV_ENOSYS;
}

uint64_t uv_get_free_memory(void) {
  assert(0 && "uv_get_free_memory not supported on Fuchsia.");
  return 0;
}

uint64_t uv_get_constrained_memory(void) {
  assert(0 && "uv_get_constrained_memory not supported on Fuchsia.");
  return 0;
}

uint64_t uv_get_total_memory(void) {
  assert(0 && "uv_get_total_memory not supported on Fuchsia.");
  return 0;
}

int uv_resident_set_memory(size_t* rss) {
  assert(0 && "uv_resident_set_memory not supported on Fuchsia.");
  return 0;
}

int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  *count = 0;
  *addresses = NULL;
  return UV_ENOSYS;
}

void uv_free_interface_addresses(uv_interface_address_t* addresses, int count) {
  for (int i = 0; i < count; i++) {
    uv__free(addresses[i].name);
  }
  uv__free(addresses);
}
