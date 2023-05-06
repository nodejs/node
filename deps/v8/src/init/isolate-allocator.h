// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_ISOLATE_ALLOCATOR_H_
#define V8_INIT_ISOLATE_ALLOCATOR_H_

#include <memory>

#include "src/base/page-allocator.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

// TODO(v8:13788): remove IsolateAllocator, as it's no longer needed.
//
// IsolateAllocator object is responsible for allocating memory for one (!)
// Isolate object. Currently, the memory is always allocated in the C++ heap.
//
// Isolate::New() first creates IsolateAllocator object which allocates the
// memory and then it constructs Isolate object in this memory. Once it's done
// the Isolate object takes ownership of the IsolateAllocator object to keep
// the memory alive.
// Isolate::Delete() takes care of the proper order of the objects destruction.
class V8_EXPORT_PRIVATE IsolateAllocator final {
 public:
  IsolateAllocator();
  ~IsolateAllocator();
  IsolateAllocator(const IsolateAllocator&) = delete;
  IsolateAllocator& operator=(const IsolateAllocator&) = delete;

  void* isolate_memory() const { return isolate_memory_; }

  v8::PageAllocator* page_allocator() const { return page_allocator_; }

  Address GetPtrComprCageBase() const {
    return COMPRESS_POINTERS_BOOL ? GetPtrComprCage()->base() : kNullAddress;
  }

  // When pointer compression is on, return the pointer compression
  // cage. Otherwise return nullptr.
  VirtualMemoryCage* GetPtrComprCage();
  const VirtualMemoryCage* GetPtrComprCage() const;

  static void InitializeOncePerProcess();

 private:
  friend class SequentialUnmapperTest;
  // Only used for testing.
  static void FreeProcessWidePtrComprCageForTesting();

  // The allocated memory for Isolate instance.
  void* isolate_memory_ = nullptr;
  v8::PageAllocator* page_allocator_ = nullptr;
#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
  VirtualMemoryCage isolate_ptr_compr_cage_;
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INIT_ISOLATE_ALLOCATOR_H_
