// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate-utils-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/third-party/heap-api.h"

namespace v8 {
namespace internal {

Isolate* Heap::GetIsolateFromWritableObject(Tagged<HeapObject> object) {
  return GetHeapFromWritableObject(object)->isolate();
}

}  // namespace internal
}  // namespace v8

namespace v8 {
namespace internal {
namespace third_party_heap {

class Impl {};

// static
std::unique_ptr<Heap> Heap::New(v8::internal::Isolate*) { return nullptr; }

// static
v8::internal::Isolate* Heap::GetIsolate(Address) { return nullptr; }

AllocationResult Heap::Allocate(size_t, AllocationType, AllocationAlignment) {
  return AllocationResult();
}

Address Heap::GetObjectFromInnerPointer(Address) { return 0; }

const base::AddressRegion& Heap::GetCodeRange() {
  static const base::AddressRegion no_region(0, 0);
  return no_region;
}

bool Heap::IsPendingAllocation(Tagged<HeapObject>) { return false; }

// static
bool Heap::InSpace(Address, AllocationSpace) { return false; }

// static
bool Heap::InOldSpace(Address) { return false; }

// static
bool Heap::InReadOnlySpace(Address) { return false; }

// static
bool Heap::InLargeObjectSpace(Address address) { return false; }

// static
bool Heap::IsValidHeapObject(Tagged<HeapObject>) { return false; }

// static
bool Heap::IsImmovable(Tagged<HeapObject>) { return false; }

// static
bool Heap::IsValidCodeObject(Tagged<HeapObject>) { return false; }

void Heap::ResetIterator() {}

Tagged<HeapObject> Heap::NextObject() { return HeapObject(); }

bool Heap::CollectGarbage() { return false; }

size_t Heap::Capacity() { return 0; }

}  // namespace third_party_heap
}  // namespace internal
}  // namespace v8
