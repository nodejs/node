// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8/cov.h"

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_SIZE 0x100000
#define MAX_EDGES ((SHM_SIZE - 4) * 8)

struct shmem_data {
  uint32_t num_edges;
  unsigned char edges[];
};

struct shmem_data* shmem;

uint32_t *__edges_start, *__edges_stop;
void __sanitizer_cov_reset_edgeguards() {
  uint32_t N = 0;
  for (uint32_t* x = __edges_start; x < __edges_stop && N < MAX_EDGES; x++)
    *x = ++N;
}

extern "C" void __sanitizer_cov_trace_pc_guard_init(uint32_t* start,
                                                    uint32_t* stop) {
  // Map the shared memory region
  const char* shm_key = getenv("SHM_ID");
  if (!shm_key) {
    puts("[COV] no shared memory bitmap available, skipping");
    shmem = (struct shmem_data*)malloc(SHM_SIZE);
  } else {
    int fd = shm_open(shm_key, O_RDWR, S_IREAD | S_IWRITE);
    if (fd <= -1) {
      fprintf(stderr, "[COV] Failed to open shared memory region\n");
      _exit(-1);
    }

    shmem = (struct shmem_data*)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd, 0);
    if (shmem == MAP_FAILED) {
      fprintf(stderr, "[COV] Failed to mmap shared memory region\n");
      _exit(-1);
    }
  }

  __edges_start = start;
  __edges_stop = stop;
  __sanitizer_cov_reset_edgeguards();

  shmem->num_edges = static_cast<uint32_t>(stop - start);
  printf("[COV] edge counters initialized. Shared memory: %s with %u edges\n",
         shm_key, shmem->num_edges);
}

extern "C" void __sanitizer_cov_trace_pc_guard(uint32_t* guard) {
  // There's a small race condition here: if this function executes in two
  // threads for the same edge at the same time, the first thread might disable
  // the edge (by setting the guard to zero) before the second thread fetches
  // the guard value (and thus the index). However, our instrumentation ignores
  // the first edge (see libcoverage.c) and so the race is unproblematic.
  uint32_t index = *guard;
  shmem->edges[index / 8] |= 1 << (index % 8);
  *guard = 0;
}
