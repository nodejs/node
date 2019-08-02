// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/tracing/perfetto-shared-memory.h"

#include "src/base/platform/platform.h"
#include "src/base/template-utils.h"

namespace v8 {
namespace platform {
namespace tracing {

PerfettoSharedMemory::PerfettoSharedMemory(size_t size)
    : size_(size),
      paged_memory_(::perfetto::base::PagedMemory::Allocate(size)) {
  // TODO(956543): Find a cross-platform solution.
  // TODO(petermarshall): Don't assume that size is page-aligned.
}

std::unique_ptr<::perfetto::SharedMemory>
PerfettoSharedMemoryFactory::CreateSharedMemory(size_t size) {
  return base::make_unique<PerfettoSharedMemory>(size);
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
