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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include <libgen.h>

#include <sys/protosw.h>
#include <procinfo.h>
#include <sys/proc.h>
#include <sys/procfs.h>

#include <ctype.h>

#include <sys/mntctl.h>
#include <sys/vmount.h>
#include <limits.h>
#include <strings.h>
#include <sys/vnode.h>

#include <as400_protos.h>
#include <as400_types.h>

char* original_exepath = NULL;
uv_mutex_t process_title_mutex;
uv_once_t process_title_mutex_once = UV_ONCE_INIT;

typedef struct {
  int bytes_available;
  int bytes_returned;
  char current_date_and_time[8];
  char system_name[8];
  char elapsed_time[6];
  char restricted_state_flag;
  char reserved;
  int percent_processing_unit_used;
  int jobs_in_system;
  int percent_permanent_addresses;
  int percent_temporary_addresses;
  int system_asp;
  int percent_system_asp_used;
  int total_auxiliary_storage;
  int current_unprotected_storage_used;
  int maximum_unprotected_storage_used;
  int percent_db_capability;
  int main_storage_size;
  int number_of_partitions;
  int partition_identifier;
  int reserved1;
  int current_processing_capacity;
  char processor_sharing_attribute;
  char reserved2[3];
  int number_of_processors;
  int active_jobs_in_system;
  int active_threads_in_system;
  int maximum_jobs_in_system;
  int percent_temporary_256mb_segments_used;
  int percent_temporary_4gb_segments_used;
  int percent_permanent_256mb_segments_used;
  int percent_permanent_4gb_segments_used;
  int percent_current_interactive_performance;
  int percent_uncapped_cpu_capacity_used;
  int percent_shared_processor_pool_used;
  long main_storage_size_long;
} SSTS0200;


typedef struct {
  char header[208];
  unsigned char loca_adapter_address[12];
} LIND0500;


typedef struct {
  int bytes_provided;
  int bytes_available;
  char msgid[7];
} errcode_s;


static const unsigned char e2a[256] = {
    0, 1, 2, 3, 156, 9, 134, 127, 151, 141, 142, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 157, 133, 8, 135, 24, 25, 146, 143, 28, 29, 30, 31,
    128, 129, 130, 131, 132, 10, 23, 27, 136, 137, 138, 139, 140, 5, 6, 7,
    144, 145, 22, 147, 148, 149, 150, 4, 152, 153, 154, 155, 20, 21, 158, 26,
    32, 160, 161, 162, 163, 164, 165, 166, 167, 168, 91, 46, 60, 40, 43, 33,
    38, 169, 170, 171, 172, 173, 174, 175, 176, 177, 93, 36, 42, 41, 59, 94,
    45, 47, 178, 179, 180, 181, 182, 183, 184, 185, 124, 44, 37, 95, 62, 63,
    186, 187, 188, 189, 190, 191, 192, 193, 194, 96, 58, 35, 64, 39, 61, 34,
    195, 97, 98, 99, 100, 101, 102, 103, 104, 105, 196, 197, 198, 199, 200, 201,
    202, 106, 107, 108, 109, 110, 111, 112, 113, 114, 203, 204, 205, 206, 207, 208,
    209, 126, 115, 116, 117, 118, 119, 120, 121, 122, 210, 211, 212, 213, 214, 215,
    216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231,
    123, 65, 66, 67, 68, 69, 70, 71, 72, 73, 232, 233, 234, 235, 236, 237,
    125, 74, 75, 76, 77, 78, 79, 80, 81, 82, 238, 239, 240, 241, 242, 243,
    92, 159, 83, 84, 85, 86, 87, 88, 89, 90, 244, 245, 246, 247, 248, 249,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 250, 251, 252, 253, 254, 255};


static const unsigned char a2e[256] = {
    0, 1, 2, 3, 55, 45, 46, 47, 22, 5, 37, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 60, 61, 50, 38, 24, 25, 63, 39, 28, 29, 30, 31,
    64, 79, 127, 123, 91, 108, 80, 125, 77, 93, 92, 78, 107, 96, 75, 97,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 122, 94, 76, 126, 110, 111,
    124, 193, 194, 195, 196, 197, 198, 199, 200, 201, 209, 210, 211, 212, 213, 214,
    215, 216, 217, 226, 227, 228, 229, 230, 231, 232, 233, 74, 224, 90, 95, 109,
    121, 129, 130, 131, 132, 133, 134, 135, 136, 137, 145, 146, 147, 148, 149, 150,
    151, 152, 153, 162, 163, 164, 165, 166, 167, 168, 169, 192, 106, 208, 161, 7,
    32, 33, 34, 35, 36, 21, 6, 23, 40, 41, 42, 43, 44, 9, 10, 27,
    48, 49, 26, 51, 52, 53, 54, 8, 56, 57, 58, 59, 4, 20, 62, 225,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 81, 82, 83, 84, 85, 86, 87,
    88, 89, 98, 99, 100, 101, 102, 103, 104, 105, 112, 113, 114, 115, 116, 117,
    118, 119, 120, 128, 138, 139, 140, 141, 142, 143, 144, 154, 155, 156, 157, 158,
    159, 160, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
    184, 185, 186, 187, 188, 189, 190, 191, 202, 203, 204, 205, 206, 207, 218, 219,
    220, 221, 222, 223, 234, 235, 236, 237, 238, 239, 250, 251, 252, 253, 254, 255};


static void iconv_e2a(unsigned char src[], unsigned char dst[], size_t length) {
  size_t i;
  for (i = 0; i < length; i++)
    dst[i] = e2a[src[i]];
}


static void iconv_a2e(const char* src, unsigned char dst[], size_t length) {
  size_t srclen;
  size_t i;

  srclen = strlen(src);
  if (srclen > length)
    srclen = length;
  for (i = 0; i < srclen; i++)
    dst[i] = a2e[src[i]];
  /* padding the remaining part with spaces */
  for (; i < length; i++)
    dst[i] = a2e[' '];
}

void init_process_title_mutex_once(void) {
  uv_mutex_init(&process_title_mutex);
}

static int get_ibmi_system_status(SSTS0200* rcvr) {
  /* rcvrlen is input parameter 2 to QWCRSSTS */
  unsigned int rcvrlen = sizeof(*rcvr);
  unsigned char format[8], reset_status[10];

  /* format is input parameter 3 to QWCRSSTS */
  iconv_a2e("SSTS0200", format, sizeof(format));
  /* reset_status is input parameter 4 */
  iconv_a2e("*NO", reset_status, sizeof(reset_status));

  /* errcode is input parameter 5 to QWCRSSTS */
  errcode_s errcode;

  /* qwcrssts_pointer is the 16-byte tagged system pointer to QWCRSSTS */
  ILEpointer __attribute__((aligned(16))) qwcrssts_pointer;

  /* qwcrssts_argv is the array of argument pointers to QWCRSSTS */
  void* qwcrssts_argv[6];

  /* Set the IBM i pointer to the QSYS/QWCRSSTS *PGM object */
  int rc = _RSLOBJ2(&qwcrssts_pointer, RSLOBJ_TS_PGM, "QWCRSSTS", "QSYS");

  if (rc != 0)
    return rc;

  /* initialize the QWCRSSTS returned info structure */
  memset(rcvr, 0, sizeof(*rcvr));

  /* initialize the QWCRSSTS error code structure */
  memset(&errcode, 0, sizeof(errcode));
  errcode.bytes_provided = sizeof(errcode);

  /* initialize the array of argument pointers for the QWCRSSTS API */
  qwcrssts_argv[0] = rcvr;
  qwcrssts_argv[1] = &rcvrlen;
  qwcrssts_argv[2] = &format;
  qwcrssts_argv[3] = &reset_status;
  qwcrssts_argv[4] = &errcode;
  qwcrssts_argv[5] = NULL;

  /* Call the IBM i QWCRSSTS API from PASE */
  rc = _PGMCALL(&qwcrssts_pointer, qwcrssts_argv, 0);

  return rc;
}


uint64_t uv_get_free_memory(void) {
  SSTS0200 rcvr;

  if (get_ibmi_system_status(&rcvr))
    return 0;

  return (uint64_t)rcvr.main_storage_size * 1024ULL;
}


uint64_t uv_get_total_memory(void) {
  SSTS0200 rcvr;

  if (get_ibmi_system_status(&rcvr))
    return 0;

  return (uint64_t)rcvr.main_storage_size * 1024ULL;
}


uint64_t uv_get_constrained_memory(void) {
  return 0;  /* Memory constraints are unknown. */
}


uint64_t uv_get_available_memory(void) {
  return uv_get_free_memory();
}


void uv_loadavg(double avg[3]) {
  SSTS0200 rcvr;

  if (get_ibmi_system_status(&rcvr)) {
    avg[0] = avg[1] = avg[2] = 0;
    return;
  }

  /* The average (in tenths) of the elapsed time during which the processing
   * units were in use. For example, a value of 411 in binary would be 41.1%.
   * This percentage could be greater than 100% for an uncapped partition.
   */
  double processing_unit_used_percent =
    rcvr.percent_processing_unit_used / 1000.0;

  avg[0] = avg[1] = avg[2] = processing_unit_used_percent;
}


int uv_resident_set_memory(size_t* rss) {
  *rss = 0;
  return 0;
}


int uv_uptime(double* uptime) {
  return UV_ENOSYS;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  unsigned int numcpus, idx = 0;
  uv_cpu_info_t* cpu_info;

  *cpu_infos = NULL;
  *count = 0;

  numcpus = sysconf(_SC_NPROCESSORS_ONLN);

  *cpu_infos = uv__malloc(numcpus * sizeof(uv_cpu_info_t));
  if (!*cpu_infos) {
    return UV_ENOMEM;
  }

  cpu_info = *cpu_infos;
  for (idx = 0; idx < numcpus; idx++) {
    cpu_info->speed = 0;
    cpu_info->model = uv__strdup("unknown");
    cpu_info->cpu_times.user = 0;
    cpu_info->cpu_times.sys = 0;
    cpu_info->cpu_times.idle = 0;
    cpu_info->cpu_times.irq = 0;
    cpu_info->cpu_times.nice = 0;
    cpu_info++;
  }
  *count = numcpus;

  return 0;
}


static int get_ibmi_physical_address(const char* line, char (*phys_addr)[6]) {
  LIND0500 rcvr;
  /* rcvrlen is input parameter 2 to QDCRLIND */
  unsigned int rcvrlen = sizeof(rcvr);
  unsigned char format[8], line_name[10];
  unsigned char mac_addr[sizeof(rcvr.loca_adapter_address)];
  int c[6];

  /* format is input parameter 3 to QDCRLIND */
  iconv_a2e("LIND0500", format, sizeof(format));

  /* line_name is input parameter 4 to QDCRLIND */
  iconv_a2e(line, line_name, sizeof(line_name));

  /* err is input parameter 5 to QDCRLIND */
  errcode_s err;

  /* qwcrssts_pointer is the 16-byte tagged system pointer to QDCRLIND */
  ILEpointer __attribute__((aligned(16))) qdcrlind_pointer;

  /* qwcrssts_argv is the array of argument pointers to QDCRLIND */
  void* qdcrlind_argv[6];

  /* Set the IBM i pointer to the QSYS/QDCRLIND *PGM object */
  int rc = _RSLOBJ2(&qdcrlind_pointer, RSLOBJ_TS_PGM, "QDCRLIND", "QSYS");

  if (rc != 0)
    return rc;

  /* initialize the QDCRLIND returned info structure */
  memset(&rcvr, 0, sizeof(rcvr));

  /* initialize the QDCRLIND error code structure */
  memset(&err, 0, sizeof(err));
  err.bytes_provided = sizeof(err);

  /* initialize the array of argument pointers for the QDCRLIND API */
  qdcrlind_argv[0] = &rcvr;
  qdcrlind_argv[1] = &rcvrlen;
  qdcrlind_argv[2] = &format;
  qdcrlind_argv[3] = &line_name;
  qdcrlind_argv[4] = &err;
  qdcrlind_argv[5] = NULL;

  /* Call the IBM i QDCRLIND API from PASE */
  rc = _PGMCALL(&qdcrlind_pointer, qdcrlind_argv, 0);
  if (rc != 0)
    return rc;

  if (err.bytes_available > 0) {
    return -1;
  }

  /* convert ebcdic loca_adapter_address to ascii first */
  iconv_e2a(rcvr.loca_adapter_address, mac_addr,
            sizeof(rcvr.loca_adapter_address));

  /* convert loca_adapter_address(char[12]) to phys_addr(char[6]) */
  int r = sscanf(mac_addr, "%02x%02x%02x%02x%02x%02x",
                &c[0], &c[1], &c[2], &c[3], &c[4], &c[5]);

  if (r == ARRAY_SIZE(c)) {
    (*phys_addr)[0] = c[0];
    (*phys_addr)[1] = c[1];
    (*phys_addr)[2] = c[2];
    (*phys_addr)[3] = c[3];
    (*phys_addr)[4] = c[4];
    (*phys_addr)[5] = c[5];
  } else {
    memset(*phys_addr, 0, sizeof(*phys_addr));
    rc = -1;
  }
  return rc;
}


int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  uv_interface_address_t* address;
  struct ifaddrs_pase *ifap = NULL, *cur;
  int inet6, r = 0;

  *count = 0;
  *addresses = NULL;

  if (Qp2getifaddrs(&ifap))
    return UV_ENOSYS;

  /* The first loop to get the size of the array to be allocated */
  for (cur = ifap; cur; cur = cur->ifa_next) {
    if (!(cur->ifa_addr->sa_family == AF_INET6 ||
          cur->ifa_addr->sa_family == AF_INET))
      continue;

    if (!(cur->ifa_flags & IFF_UP && cur->ifa_flags & IFF_RUNNING))
      continue;

    (*count)++;
  }

  if (*count == 0) {
    Qp2freeifaddrs(ifap);
    return 0;
  }

  /* Alloc the return interface structs */
  *addresses = uv__calloc(*count, sizeof(**addresses));
  if (*addresses == NULL) {
    Qp2freeifaddrs(ifap);
    return UV_ENOMEM;
  }
  address = *addresses;

  /* The second loop to fill in the array */
  for (cur = ifap; cur; cur = cur->ifa_next) {
    if (!(cur->ifa_addr->sa_family == AF_INET6 ||
          cur->ifa_addr->sa_family == AF_INET))
      continue;

    if (!(cur->ifa_flags & IFF_UP && cur->ifa_flags & IFF_RUNNING))
      continue;

    address->name = uv__strdup(cur->ifa_name);

    inet6 = (cur->ifa_addr->sa_family == AF_INET6);

    if (inet6) {
      address->address.address6 = *((struct sockaddr_in6*)cur->ifa_addr);
      address->netmask.netmask6 = *((struct sockaddr_in6*)cur->ifa_netmask);
      address->netmask.netmask6.sin6_family = AF_INET6;
    } else {
      address->address.address4 = *((struct sockaddr_in*)cur->ifa_addr);
      address->netmask.netmask4 = *((struct sockaddr_in*)cur->ifa_netmask);
      address->netmask.netmask4.sin_family = AF_INET;
    }
    address->is_internal = cur->ifa_flags & IFF_LOOPBACK ? 1 : 0;
    if (!address->is_internal) {
      int rc = -1;
      size_t name_len = strlen(address->name);
      /* To get the associated MAC address, we must convert the address to a
       * line description. Normally, the name field contains the line
       * description name, but for VLANs it has the VLAN appended with a
       * period. Since object names can also contain periods and numbers, there
       * is no way to know if a returned name is for a VLAN or not. eg.
       * *LIND ETH1.1 and *LIND ETH1, VLAN 1 both have the same name: ETH1.1
       *
       * Instead, we apply the same heuristic used by some of the XPF ioctls:
       * - names > 10 *must* contain a VLAN
       * - assume names <= 10 do not contain a VLAN and try directly
       * - if >10 or QDCRLIND returned an error, try to strip off a VLAN
       *   and try again
       * - if we still get an error or couldn't find a period, leave the MAC as
       *   00:00:00:00:00:00
       */
      if (name_len <= 10) {
        /* Assume name does not contain a VLAN ID */
        rc = get_ibmi_physical_address(address->name, &address->phys_addr);
      }

      if (name_len > 10 || rc != 0) {
        /* The interface name must contain a VLAN ID suffix. Attempt to strip
         * it off so we can get the line description to pass to QDCRLIND.
         */
        char* temp_name = uv__strdup(address->name);
        char* dot = strrchr(temp_name, '.');
        if (dot != NULL) {
          *dot = '\0';
          if (strlen(temp_name) <= 10) {
            rc = get_ibmi_physical_address(temp_name, &address->phys_addr);
          }
        }
        uv__free(temp_name);
      }
    }

    address++;
  }

  Qp2freeifaddrs(ifap);
  return r;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses, int count) {
  int i;

  for (i = 0; i < count; ++i) {
    uv__free(addresses[i].name);
  }

  uv__free(addresses);
}

char** uv_setup_args(int argc, char** argv) {
  char exepath[UV__PATH_MAX];
  char* s;
  size_t size;

  if (argc > 0) {
    /* Use argv[0] to determine value for uv_exepath(). */
    size = sizeof(exepath);
    if (uv__search_path(argv[0], exepath, &size) == 0) {
      uv_once(&process_title_mutex_once, init_process_title_mutex_once);
      uv_mutex_lock(&process_title_mutex);
      original_exepath = uv__strdup(exepath);
      uv_mutex_unlock(&process_title_mutex);
    }
  }

  return argv;
}

int uv_set_process_title(const char* title) {
  return 0;
}

int uv_get_process_title(char* buffer, size_t size) {
  if (buffer == NULL || size == 0)
    return UV_EINVAL;

  buffer[0] = '\0';
  return 0;
}

void uv__process_title_cleanup(void) {
}
