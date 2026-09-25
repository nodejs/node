// Copyright 2024 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/perf_counters.h"

#include "hwy/detect_compiler_arch.h"  // HWY_OS_LINUX

#if HWY_OS_LINUX || HWY_IDE
#include <errno.h>
#include <fcntl.h>  // open
#include <linux/perf_event.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>  // strcmp
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>  // O_RDONLY
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "hwy/base.h"  // HWY_ASSERT
#include "hwy/bit_set.h"
#include "hwy/timer.h"

#endif  // HWY_OS_LINUX || HWY_IDE

namespace hwy {
namespace platform {

#if HWY_OS_LINUX || HWY_IDE

namespace {

bool PerfCountersSupported() {
  // This is the documented way.
  struct stat s;
  return stat("/proc/sys/kernel/perf_event_paranoid", &s) == 0;
}

// If we detect Linux < 6.9 and AMD EPYC, use cycles instead of ref-cycles
// because the latter is not supported and returns 0, see
// https://lwn.net/Articles/967791/.
uint64_t RefCyclesOrCycles() {
  const uint32_t ref_cycles = PERF_COUNT_HW_REF_CPU_CYCLES;

  utsname buf;
  if (uname(&buf) != 0) return ref_cycles;
  if (std::string(buf.sysname) != "Linux") return ref_cycles;
  int major, minor;
  if (sscanf(buf.release, "%d.%d", &major, &minor) != 2) return ref_cycles;
  if (major > 6 || (major == 6 && minor >= 9)) return ref_cycles;

  // AMD Zen4 CPU
  char cpu100[100];
  if (!GetCpuString(cpu100)) return ref_cycles;
  if (std::string(cpu100).rfind("AMD EPYC", 0) != 0) return ref_cycles;

  return PERF_COUNT_HW_CPU_CYCLES;
}

struct CounterConfig {  // for perf_event_open
  uint64_t config;
  uint32_t type;
  PerfCounters::Counter c;
};

std::vector<CounterConfig> AllCounterConfigs() {
  constexpr uint32_t kHW = PERF_TYPE_HARDWARE;
  constexpr uint32_t kSW = PERF_TYPE_SOFTWARE;
  constexpr uint32_t kC = PERF_TYPE_HW_CACHE;
  constexpr uint64_t kL3 = PERF_COUNT_HW_CACHE_LL;
  constexpr uint64_t kLoad = uint64_t{PERF_COUNT_HW_CACHE_OP_READ} << 8;
  constexpr uint64_t kStore = uint64_t{PERF_COUNT_HW_CACHE_OP_WRITE} << 8;
  constexpr uint64_t kAcc = uint64_t{PERF_COUNT_HW_CACHE_RESULT_ACCESS} << 16;

  // Order is important for bin-packing event groups. x86 can only handle two
  // LLC-related events per group, so spread them out and arrange SW events
  // such that do not start a new group. This list of counters may change.
  return {{RefCyclesOrCycles(), kHW, PerfCounters::kRefCycles},
          {PERF_COUNT_HW_INSTRUCTIONS, kHW, PerfCounters::kInstructions},
          {PERF_COUNT_SW_PAGE_FAULTS, kSW, PerfCounters::kPageFaults},
          {kL3 | kLoad | kAcc, kC, PerfCounters::kL3Loads},
          {kL3 | kStore | kAcc, kC, PerfCounters::kL3Stores},
          {PERF_COUNT_HW_BRANCH_INSTRUCTIONS, kHW, PerfCounters::kBranches},
          {PERF_COUNT_HW_BRANCH_MISSES, kHW, PerfCounters::kBranchMispredicts},
          // Second group:
          {PERF_COUNT_HW_BUS_CYCLES, kHW, PerfCounters::kBusCycles},
          {PERF_COUNT_SW_CPU_MIGRATIONS, kSW, PerfCounters::kMigrations},
          {PERF_COUNT_HW_CACHE_REFERENCES, kHW, PerfCounters::kCacheRefs},
          {PERF_COUNT_HW_CACHE_MISSES, kHW, PerfCounters::kCacheMisses}};
}

size_t& PackedIdx(PerfCounters::Counter c) {
  static size_t packed_idx[64];
  return packed_idx[static_cast<size_t>(c)];
}

class PMU {
  static perf_event_attr MakeAttr(const CounterConfig& cc) {
    perf_event_attr attr = {};
    attr.type = cc.type;
    attr.size = sizeof(attr);
    attr.config = cc.config;
    // We request more counters than the HW may support. If so, they are
    // multiplexed and only active for a fraction of the runtime. Recording the
    // times lets us extrapolate. GROUP enables a single syscall to reduce the
    // cost of reading.
    attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
                       PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_GROUP;
    // Do not set inherit=1 because that conflicts with PERF_FORMAT_GROUP.
    // Do not set disable=1, so that perf_event_open verifies all events in the
    // group can be scheduled together.
    attr.exclude_kernel = 1;  // required if perf_event_paranoid == 1
    attr.exclude_hv = 1;      // = hypervisor
    return attr;
  }

  static int SysPerfEventOpen(const CounterConfig& cc, int leader_fd) {
    perf_event_attr attr = MakeAttr(cc);
    const int pid = 0;   // current process (cannot also be -1)
    const int cpu = -1;  // any CPU
    // Retry if interrupted by signals; this actually happens (b/64774091).
    for (int retry = 0; retry < 10; ++retry) {
      const int flags = 0;
      const int fd = static_cast<int>(
          syscall(__NR_perf_event_open, &attr, pid, cpu, leader_fd, flags));
      if (!(fd == -1 && errno == EINTR)) return fd;
    }
    HWY_WARN("perf_event_open retries were insufficient.");
    return -1;
  }

  // Reads from `fd`; recovers from interruptions before/during the read.
  static bool ReadBytes(int fd, ssize_t size, void* to) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(to);
    ssize_t pos = 0;
    for (int retry = 0; retry < 10; ++retry) {
      const ssize_t bytes_read =
          read(fd, bytes + pos, static_cast<size_t>(size - pos));
      if (HWY_UNLIKELY(bytes_read <= 0)) {
        if (errno == EINTR) continue;
        HWY_WARN("perf read() failed, errno %d.", errno);
        return false;
      }
      pos += bytes_read;
      HWY_ASSERT(pos <= size);
      if (HWY_LIKELY(pos == size)) return true;  // success
    }
    HWY_WARN("perf read() wanted %d bytes, got %d.", static_cast<int>(size),
             static_cast<int>(pos));
    return false;
  }

  // Array size in Buf; this is another upper bound on group size. It should be
  // loose because it only wastes a bit of stack space, whereas an unnecessary
  // extra group decreases coverage. Most HW supports 4-8 counters per group.
  static constexpr size_t kMaxEventsPerGroup = PerfCounters::kCapacity;

#pragma pack(push, 1)
  struct Buf {
    uint64_t num_events;
    uint64_t time_enabled;
    uint64_t time_running;
    uint64_t values[kMaxEventsPerGroup];
  };
#pragma pack(pop)

  // Returns false on error, otherwise sets `extrapolate` and `values`.
  static bool ReadAndExtrapolate(int fd, size_t num_events, double& extrapolate,
                                 double* HWY_RESTRICT values) {
    Buf buf;
    const ssize_t want_bytes =  // size of var-len `Buf`
        static_cast<ssize_t>(24 + num_events * sizeof(uint64_t));
    if (HWY_UNLIKELY(!ReadBytes(fd, want_bytes, &buf))) return false;

    HWY_DASSERT(num_events == buf.num_events);
    HWY_DASSERT(buf.time_running <= buf.time_enabled);
    // If the group was not yet scheduled, we must avoid division by zero.
    // In case counters were previously running and not reset, their current
    // values may be nonzero. Returning zero could be interpreted as counters
    // running backwards, so we instead treat this as a failure and mark the
    // counters as invalid.
    if (HWY_UNLIKELY(buf.time_running == 0)) return false;

    // Extrapolate each value.
    extrapolate = static_cast<double>(buf.time_enabled) /
                  static_cast<double>(buf.time_running);
    for (size_t i = 0; i < buf.num_events; ++i) {
      values[i] = static_cast<double>(buf.values[i]) * extrapolate;
    }
    return true;
  }

 public:
  bool Init() {
    // Allow callers who do not know about each other to each call `Init`.
    // If this already succeeded, we're done; if not, we will try again.
    if (HWY_UNLIKELY(!fds_.empty())) return true;
    if (HWY_UNLIKELY(!PerfCountersSupported())) {
      HWY_WARN(
          "This Linux does not support perf counters. The program will"
          "continue, but counters will return zero.");
      return false;
    }

    groups_.push_back(Group());
    fds_.reserve(PerfCounters::kCapacity);

    for (const CounterConfig& config : AllCounterConfigs()) {
      // If the group is limited by our buffer size, add a new one.
      if (HWY_UNLIKELY(groups_.back().num_events == kMaxEventsPerGroup)) {
        groups_.push_back(Group());
      }

      int fd = SysPerfEventOpen(config, groups_.back().leader_fd);
      // Retry in case the group is limited by HW capacity. Do not check
      // errno because it is too inconsistent (ENOSPC, EINVAL, others?).
      if (HWY_UNLIKELY(fd < 0)) {
        fd = SysPerfEventOpen(config, /*leader_fd=*/-1);
        if (fd >= 0 && groups_.back().num_events != 0) {
          groups_.push_back(Group());
        }
      }

      if (HWY_UNLIKELY(fd < 0)) {
        HWY_WARN("perf_event_open %d errno %d for counter %s.", fd, errno,
                 PerfCounters::Name(config.c));
      } else {
        // Add to group and set as leader if empty.
        if (groups_.back().leader_fd == -1) {
          groups_.back().leader_fd = fd;

          // Ensure the leader is not a SW event, because adding an HW
          // event to a group with only SW events is slow, and starting
          // with SW may trigger a bug, see
          // https://lore.kernel.org/lkml/tip-a1150c202207cc8501bebc45b63c264f91959260@git.kernel.org/
          if (HWY_UNLIKELY(config.type == PERF_TYPE_SOFTWARE)) {
            HWY_WARN("SW event %s should not be leader.",
                     PerfCounters::Name(config.c));
          }
        }

        PackedIdx(config.c) = fds_.size();
        groups_.back().num_events += 1;
        valid_.Set(static_cast<size_t>(config.c));
        fds_.push_back(fd);
      }
    }

    // If no counters are available, remove the empty group.
    if (HWY_UNLIKELY(fds_.empty())) {
      HWY_ASSERT(groups_.size() == 1);
      HWY_ASSERT(groups_.back().num_events == 0);
      HWY_ASSERT(groups_.back().leader_fd == -1);
      groups_.clear();
    }

    size_t num_valid = 0;
    for (const Group& group : groups_) {
      num_valid += group.num_events;
      // All groups have a leader and are not empty.
      HWY_ASSERT(group.leader_fd >= 0);
      HWY_ASSERT(0 != group.num_events &&
                 group.num_events <= kMaxEventsPerGroup);
    }
    // Total `num_events` matches `fds_` and `Valid()`.
    HWY_ASSERT(num_valid == fds_.size());
    HWY_ASSERT(num_valid == valid_.Count());
    HWY_ASSERT(num_valid <= PerfCounters::kCapacity);

    if (num_valid) {
      StopAllAndReset();
      return true;
    } else {
      HWY_WARN("No valid counters found.");
      return true;
    }
  }

  bool StartAll() {
    if (HWY_UNLIKELY(fds_.empty())) return false;
    HWY_ASSERT(prctl(PR_TASK_PERF_EVENTS_ENABLE) == 0);
    return true;
  }

  void StopAllAndReset() {
    HWY_ASSERT(prctl(PR_TASK_PERF_EVENTS_DISABLE) == 0);
    for (int fd : fds_) {
      HWY_ASSERT(ioctl(fd, PERF_EVENT_IOC_RESET, 0) == 0);
    }
  }

  // Returns false on error, otherwise sets `valid`, `max_extrapolate`, and
  // `values`.
  bool Read(BitSet64& valid, double& max_extrapolate, double* values) {
    if (HWY_UNLIKELY(!valid_.Any())) return false;

    // Read all counters into buffer in the order in which they were opened.
    max_extrapolate = 1.0;
    double* pos = values;
    for (const Group& group : groups_) {
      double extrapolate;
      if (HWY_UNLIKELY(!ReadAndExtrapolate(group.leader_fd, group.num_events,
                                           extrapolate, pos))) {
        return false;
      }
      max_extrapolate = HWY_MAX(max_extrapolate, extrapolate);
      pos += group.num_events;
    }

    valid = valid_;
    HWY_DASSERT(pos == values + valid.Count());
    return true;
  }

 private:
  std::vector<int> fds_;  // one per valid_
  BitSet64 valid_;

  struct Group {
    size_t num_events = 0;
    int leader_fd = -1;
  };
  std::vector<Group> groups_;
};

// Monostate, see header.
PMU& GetPMU() {
  static PMU pmu;
  return pmu;
}

}  // namespace

HWY_DLLEXPORT bool PerfCounters::Init() { return GetPMU().Init(); }
HWY_DLLEXPORT bool PerfCounters::StartAll() { return GetPMU().StartAll(); }
HWY_DLLEXPORT void PerfCounters::StopAllAndReset() {
  GetPMU().StopAllAndReset();
}
HWY_DLLEXPORT PerfCounters::PerfCounters() {
  if (HWY_UNLIKELY(!GetPMU().Read(valid_, max_extrapolate_, values_))) {
    valid_ = BitSet64();
    max_extrapolate_ = 0.0;
    hwy::ZeroBytes(values_, sizeof(values_));
  }
}
HWY_DLLEXPORT size_t PerfCounters::IndexForCounter(Counter c) {
  return PackedIdx(c);
}
#else
HWY_DLLEXPORT bool PerfCounters::Init() { return false; }
HWY_DLLEXPORT bool PerfCounters::StartAll() { return false; }
HWY_DLLEXPORT void PerfCounters::StopAllAndReset() {}
HWY_DLLEXPORT PerfCounters::PerfCounters()
    : max_extrapolate_(1.0), values_{0.0} {}
HWY_DLLEXPORT size_t PerfCounters::IndexForCounter(Counter) { return 0; }
#endif  // HWY_OS_LINUX || HWY_IDE

}  // namespace platform
}  // namespace hwy
