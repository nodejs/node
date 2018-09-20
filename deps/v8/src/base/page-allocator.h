// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PAGE_ALLOCATOR_H_
#define V8_BASE_PAGE_ALLOCATOR_H_

#include "include/v8-platform.h"
#include "src/base/base-export.h"
#include "src/base/compiler-specific.h"

namespace v8 {
namespace base {

class V8_BASE_EXPORT PageAllocator
    : public NON_EXPORTED_BASE(::v8::PageAllocator) {
 public:
  virtual ~PageAllocator() = default;

  size_t AllocatePageSize() override;

  size_t CommitPageSize() override;

  void SetRandomMmapSeed(int64_t seed) override;

  void* GetRandomMmapAddr() override;

  void* AllocatePages(void* address, size_t size, size_t alignment,
                      PageAllocator::Permission access) override;

  bool FreePages(void* address, size_t size) override;

  bool ReleasePages(void* address, size_t size, size_t new_size) override;

  bool SetPermissions(void* address, size_t size,
                      PageAllocator::Permission access) override;
};

}  // namespace base
}  // namespace v8
#endif  // V8_BASE_PAGE_ALLOCATOR_H_
