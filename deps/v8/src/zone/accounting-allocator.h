// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ACCOUNTING_ALLOCATOR_H_
#define V8_ZONE_ACCOUNTING_ALLOCATOR_H_

#include "include/v8-platform.h"
#include "src/base/atomic-utils.h"
#include "src/base/atomicops.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/base/platform/time.h"
#include "src/zone/zone-segment.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE AccountingAllocator {
 public:
  AccountingAllocator() = default;
  virtual ~AccountingAllocator() = default;

  virtual Segment* AllocateSegment(size_t bytes);
  virtual void FreeSegment(Segment* memory);

  size_t GetCurrentMemoryUsage() const;
  size_t GetMaxMemoryUsage() const;

 private:
  base::AtomicWord current_memory_usage_ = 0;
  base::AtomicWord max_memory_usage_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AccountingAllocator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ACCOUNTING_ALLOCATOR_H_
