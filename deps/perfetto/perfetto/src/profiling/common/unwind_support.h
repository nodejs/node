/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_PROFILING_COMMON_UNWIND_SUPPORT_H_
#define SRC_PROFILING_COMMON_UNWIND_SUPPORT_H_

// defines PERFETTO_BUILDFLAG
#include "perfetto/base/build_config.h"

#include <memory>
#include <string>

#include <unwindstack/Maps.h>
#include <unwindstack/Unwinder.h>
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
#include <unwindstack/DexFiles.h>
#include <unwindstack/JitDebug.h>
#endif

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {
namespace profiling {

// libunwindstack's FrameData annotated with the build_id.
struct FrameData {
  FrameData(unwindstack::FrameData f, std::string id)
      : frame(std::move(f)), build_id(std::move(id)) {}

  unwindstack::FrameData frame;
  std::string build_id;
};

// Read /proc/[pid]/maps from an open file descriptor.
// TODO(fmayer): Figure out deduplication to other maps.
class FDMaps : public unwindstack::Maps {
 public:
  FDMaps(base::ScopedFile fd);

  FDMaps(const FDMaps&) = delete;
  FDMaps& operator=(const FDMaps&) = delete;

  FDMaps(FDMaps&& m) : Maps(std::move(m)) { fd_ = std::move(m.fd_); }

  FDMaps& operator=(FDMaps&& m) {
    if (&m != this)
      fd_ = std::move(m.fd_);
    Maps::operator=(std::move(m));
    return *this;
  }

  virtual ~FDMaps() override = default;

  bool Parse() override;
  void Reset();

 private:
  base::ScopedFile fd_;
};

class FDMemory : public unwindstack::Memory {
 public:
  FDMemory(base::ScopedFile mem_fd);
  size_t Read(uint64_t addr, void* dst, size_t size) override;

 private:
  base::ScopedFile mem_fd_;
};

// Overlays size bytes pointed to by stack for addresses in [sp, sp + size).
// Addresses outside of that range are read from mem_fd, which should be an fd
// that opened /proc/[pid]/mem.
class StackOverlayMemory : public unwindstack::Memory {
 public:
  StackOverlayMemory(std::shared_ptr<unwindstack::Memory> mem,
                     uint64_t sp,
                     const uint8_t* stack,
                     size_t size);
  size_t Read(uint64_t addr, void* dst, size_t size) override;

 private:
  std::shared_ptr<unwindstack::Memory> mem_;
  const uint64_t sp_;
  const uint64_t stack_end_;
  const uint8_t* const stack_;
};

struct UnwindingMetadata {
  UnwindingMetadata(base::ScopedFile maps_fd, base::ScopedFile mem_fd);

  // move-only
  UnwindingMetadata(const UnwindingMetadata&) = delete;
  UnwindingMetadata& operator=(const UnwindingMetadata&) = delete;

  UnwindingMetadata(UnwindingMetadata&&) = default;
  UnwindingMetadata& operator=(UnwindingMetadata&&) = default;

  void ReparseMaps();

  FrameData AnnotateFrame(unwindstack::FrameData frame);

  FDMaps fd_maps;
  // The API of libunwindstack expects shared_ptr for Memory.
  std::shared_ptr<unwindstack::Memory> fd_mem;
  uint64_t reparses = 0;
  base::TimeMillis last_maps_reparse_time{0};
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  std::unique_ptr<unwindstack::JitDebug> jit_debug;
  std::unique_ptr<unwindstack::DexFiles> dex_files;
#endif
};

std::string StringifyLibUnwindstackError(unwindstack::ErrorCode);

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_COMMON_UNWIND_SUPPORT_H_
