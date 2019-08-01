// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_PERFETTO_SHARED_MEMORY_H_
#define V8_LIBPLATFORM_TRACING_PERFETTO_SHARED_MEMORY_H_

#include "perfetto/tracing/core/shared_memory.h"

#include "third_party/perfetto/include/perfetto/base/paged_memory.h"

namespace v8 {
namespace platform {
namespace tracing {

// Perfetto requires a shared memory implementation for multi-process embedders
// but V8 is single process. We implement it here using PagedMemory from
// perfetto.
class PerfettoSharedMemory : public ::perfetto::SharedMemory {
 public:
  explicit PerfettoSharedMemory(size_t size);

  // The PagedMemory destructor will free the underlying memory when this object
  // is destroyed.

  void* start() const override { return paged_memory_.Get(); }
  size_t size() const override { return size_; }

 private:
  size_t size_;
  ::perfetto::base::PagedMemory paged_memory_;
};

class PerfettoSharedMemoryFactory : public ::perfetto::SharedMemory::Factory {
 public:
  ~PerfettoSharedMemoryFactory() override = default;
  std::unique_ptr<::perfetto::SharedMemory> CreateSharedMemory(
      size_t size) override;
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_PERFETTO_SHARED_MEMORY_H_
