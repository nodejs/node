/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACED_PROBES_FTRACE_FTRACE_METADATA_H_
#define SRC_TRACED_PROBES_FTRACE_FTRACE_METADATA_H_

#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include <bitset>

#include "perfetto/base/flat_set.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/traced/data_source_types.h"

namespace perfetto {

using BlockDeviceID = decltype(stat::st_dev);
using Inode = decltype(stat::st_ino);

// Container for tracking miscellaneous information while parsing ftrace events,
// scoped to an individual data source.
struct FtraceMetadata {
  FtraceMetadata() {
    // A sched_switch is 64 bytes, a page is 4096 bytes and we expect
    // 2 pid's per sched_switch. 4096/64*2=128. Give it a 2x margin.
    pids.reserve(256);

    // We expect to see only a small number of task rename events.
    rename_pids.reserve(32);
  }

  void AddDevice(BlockDeviceID device_id) {
    last_seen_device_id = device_id;
#if PERFETTO_DCHECK_IS_ON()
    seen_device_id = true;
#endif
  }

  void AddInode(Inode inode_number) {
#if PERFETTO_DCHECK_IS_ON()
    PERFETTO_DCHECK(seen_device_id);
#endif
    static int32_t cached_pid = 0;
    if (!cached_pid)
      cached_pid = getpid();

    PERFETTO_DCHECK(last_seen_common_pid);
    PERFETTO_DCHECK(cached_pid == getpid());
    // Ignore own scanning activity.
    if (cached_pid != last_seen_common_pid) {
      inode_and_device.insert(
          std::make_pair(inode_number, last_seen_device_id));
    }
  }

  void AddRenamePid(int32_t pid) { rename_pids.insert(pid); }

  void AddPid(int32_t pid) {
    const size_t pid_bit = static_cast<size_t>(pid);
    if (PERFETTO_LIKELY(pid_bit < pids_cache.size())) {
      if (pids_cache.test(pid_bit))
        return;
      pids_cache.set(pid_bit);
    }
    pids.insert(pid);
  }

  void AddCommonPid(int32_t pid) {
    last_seen_common_pid = pid;
    AddPid(pid);
  }

  void Clear() {
    inode_and_device.clear();
    rename_pids.clear();
    pids.clear();
    pids_cache.reset();
    FinishEvent();
  }

  void FinishEvent() {
    last_seen_device_id = 0;
    last_seen_common_pid = 0;
#if PERFETTO_DCHECK_IS_ON()
    seen_device_id = false;
#endif
  }

  BlockDeviceID last_seen_device_id = 0;
#if PERFETTO_DCHECK_IS_ON()
  bool seen_device_id = false;
#endif
  int32_t last_seen_common_pid = 0;

  base::FlatSet<InodeBlockPair> inode_and_device;
  base::FlatSet<int32_t> rename_pids;
  base::FlatSet<int32_t> pids;

  // This bitmap is a cache for |pids|. It speculates on the fact that on most
  // Android kernels, PID_MAX=32768. It saves ~1-2% cpu time on high load
  // scenarios, as AddPid() is a very hot path.
  std::bitset<32768> pids_cache;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_FTRACE_METADATA_H_
