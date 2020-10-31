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

#include <stddef.h>
#include <stdint.h>

#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "src/profiling/common/unwind_support.h"
#include "src/profiling/memory/shared_ring_buffer.h"
#include "src/profiling/memory/unwinding.h"
#include "src/profiling/memory/unwound_messages.h"

namespace perfetto {
namespace profiling {
namespace {

class NopDelegate : public UnwindingWorker::Delegate {
  void PostAllocRecord(AllocRecord) override {}
  void PostFreeRecord(FreeRecord) override {}
  void PostSocketDisconnected(DataSourceInstanceID,
                              pid_t,
                              SharedRingBuffer::Stats) override {}
};

int FuzzUnwinding(const uint8_t* data, size_t size) {
  SharedRingBuffer::Buffer buf(const_cast<uint8_t*>(data), size);

  pid_t self_pid = getpid();
  DataSourceInstanceID id = 0;
  UnwindingMetadata metadata(base::OpenFile("/proc/self/maps", O_RDONLY),
                             base::OpenFile("/proc/self/mem", O_RDONLY));

  NopDelegate nop_delegate;
  UnwindingWorker::HandleBuffer(buf, &metadata, id, self_pid, &nop_delegate);
  return 0;
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return perfetto::profiling::FuzzUnwinding(data, size);
}
