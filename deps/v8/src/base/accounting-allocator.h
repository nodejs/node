// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_ACCOUNTING_ALLOCATOR_H_
#define V8_BASE_ACCOUNTING_ALLOCATOR_H_

#include "src/base/atomicops.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

class AccountingAllocator final {
 public:
  AccountingAllocator() = default;
  ~AccountingAllocator() = default;

  // Returns nullptr on failed allocation.
  void* Allocate(size_t bytes);
  void Free(void* memory, size_t bytes);

  size_t GetCurrentMemoryUsage() const;

 private:
  AtomicWord current_memory_usage_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AccountingAllocator);
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ACCOUNTING_ALLOCATOR_H_
