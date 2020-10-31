/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/trace_processor/importers/syscalls/syscall_tracker.h"

#include <type_traits>
#include <utility>

#include <inttypes.h>

#include "src/trace_processor/storage/stats.h"

#include "src/trace_processor/importers/syscalls/syscalls_aarch32.h"
#include "src/trace_processor/importers/syscalls/syscalls_aarch64.h"
#include "src/trace_processor/importers/syscalls/syscalls_armeabi.h"
#include "src/trace_processor/importers/syscalls/syscalls_x86.h"
#include "src/trace_processor/importers/syscalls/syscalls_x86_64.h"

namespace perfetto {
namespace trace_processor {
namespace {

template <typename T>
constexpr size_t GetSyscalls(const T&) {
  static_assert(std::extent<T>::value <= kMaxSyscalls,
                "kMaxSyscalls too small");
  return std::extent<T>::value;
}

}  // namespace

// TODO(primiano): The current design is broken in case of 32-bit processes
// running on 64-bit kernel. At least on ARM, the syscal numbers don't match
// and we should use the kSyscalls_Aarch32 table for those processes. But this
// means that the architecture is not a global property but is per-process.
// Which in turn means that somehow we need to figure out what is the bitness
// of each process from the trace.
SyscallTracker::SyscallTracker(TraceProcessorContext* context)
    : context_(context) {
  SetArchitecture(kUnknown);
}

SyscallTracker::~SyscallTracker() = default;

void SyscallTracker::SetArchitecture(Architecture arch) {
  const char* kSyscalls_Unknown[] = {nullptr};
  size_t num_syscalls = 0;
  const char* const* syscall_table = nullptr;

  switch (arch) {
    case kArmEabi:
      num_syscalls = GetSyscalls(kSyscalls_ArmEabi);
      syscall_table = &kSyscalls_ArmEabi[0];
      break;
    case kAarch32:
      num_syscalls = GetSyscalls(kSyscalls_Aarch32);
      syscall_table = &kSyscalls_Aarch32[0];
      break;
    case kAarch64:
      num_syscalls = GetSyscalls(kSyscalls_Aarch64);
      syscall_table = &kSyscalls_Aarch64[0];
      break;
    case kX86_64:
      num_syscalls = GetSyscalls(kSyscalls_x86_64);
      syscall_table = &kSyscalls_x86_64[0];
      break;
    case kX86:
      num_syscalls = GetSyscalls(kSyscalls_x86);
      syscall_table = &kSyscalls_x86[0];
      break;
    case kUnknown:
      num_syscalls = 0;
      syscall_table = &kSyscalls_Unknown[0];
      break;
  }

  for (size_t i = 0; i < kMaxSyscalls; i++) {
    StringId id = kNullStringId;
    if (i < num_syscalls && syscall_table[i] && *syscall_table[i]) {
      const char* name = syscall_table[i];
      id = context_->storage->InternString(name);
      if (!strcmp(name, "sys_write"))
        sys_write_string_id_ = id;
    } else {
      char unknown_str[64];
      sprintf(unknown_str, "sys_%zu", i);
      id = context_->storage->InternString(unknown_str);
    }
    arch_syscall_to_string_id_[i] = id;
  }
}

}  // namespace trace_processor
}  // namespace perfetto
