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
#include <assert.h>
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


static int get_ibmi_system_status(SSTS0200* rcvr) {
  /* rcvrlen is input parameter 2 to QWCRSSTS */
  unsigned int rcvrlen = sizeof(*rcvr);

  /* format is input parameter 3 to QWCRSSTS ("SSTS0200" in EBCDIC) */
  unsigned char format[] = {0xE2, 0xE2, 0xE3, 0xE2, 0xF0, 0xF2, 0xF0, 0xF0};

  /* reset_status is input parameter 4 to QWCRSSTS ("*NO       " in EBCDIC) */
  unsigned char reset_status[] = {
    0x5C, 0xD5, 0xD6, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40
  };

  /* errcode is input parameter 5 to QWCRSSTS */
  struct _errcode {
    int bytes_provided;
    int bytes_available;
    char msgid[7];
  } errcode;

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
  rc = _PGMCALL(&qwcrssts_pointer, (void**)&qwcrssts_argv, 0);

  return rc;
}


uint64_t uv_get_free_memory(void) {
  SSTS0200 rcvr;

  if (get_ibmi_system_status(&rcvr))
    return 0;

  /* The amount of main storage, in kilobytes, in the system. */
  uint64_t main_storage_size = rcvr.main_storage_size;

  /* The current amount of storage in use for temporary objects.
   * in millions (M) of bytes.
   */
  uint64_t current_unprotected_storage_used =
    rcvr.current_unprotected_storage_used * 1024ULL;

  uint64_t free_storage_size =
    (main_storage_size - current_unprotected_storage_used) * 1024ULL;

  return free_storage_size < 0 ? 0 : free_storage_size;
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
