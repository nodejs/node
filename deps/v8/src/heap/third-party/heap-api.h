// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_THIRD_PARTY_HEAP_API_H_
#define V8_HEAP_THIRD_PARTY_HEAP_API_H_

#include "include/v8.h"
#include "src/base/address-region.h"
#include "src/heap/heap.h"

namespace v8 {
namespace internal {
namespace third_party_heap {

class Impl;

class Heap {
 public:
  static std::unique_ptr<Heap> New(v8::internal::Isolate* isolate);

  static v8::internal::Isolate* GetIsolate(Address address);

  AllocationResult Allocate(size_t size_in_bytes, AllocationType type,
                            AllocationAlignment align);

  Address GetObjectFromInnerPointer(Address inner_pointer);

  const base::AddressRegion& GetCodeRange();

  bool IsPendingAllocation(HeapObject object);

  static bool InSpace(Address address, AllocationSpace space);

  static bool InOldSpace(Address address);

  static bool InReadOnlySpace(Address address);

  static bool InLargeObjectSpace(Address address);

  static bool IsValidHeapObject(HeapObject object);

  static bool IsImmovable(HeapObject object);

  static bool IsValidCodeObject(HeapObject object);

  void ResetIterator();
  HeapObject NextObject();

  bool CollectGarbage();

  size_t Capacity();

  V8_INLINE Impl* impl() { return impl_; }

 private:
  Impl* impl_ = nullptr;
};

}  // namespace third_party_heap
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_THIRD_PARTY_HEAP_API_H_
