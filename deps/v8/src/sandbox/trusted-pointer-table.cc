// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/trusted-pointer-table.h"

#include "src/execution/isolate.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/sandbox/trusted-pointer-table-inl.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

uint32_t TrustedPointerTable::Sweep(Space* space, Counters* counters) {
  uint32_t num_live_entries = GenericSweep(space);
  counters->trusted_pointers_count()->AddSample(num_live_entries);
  return num_live_entries;
}

void TrustedPointerTable::Verify(Isolate* isolate, Space* space) {
  IterateEntriesIn(space, [&](uint32_t index) {
    auto& entry = at(index);
    IndirectPointerTag tag = entry.GetTag();
    if (tag == kIndirectPointerFreeEntryTag ||
        tag == kIndirectPointerZappedEntryTag ||
        tag == kUnpublishedIndirectPointerTag) {
      return;
    }

    Address pointer = entry.GetPointer(tag);

    // 1. The pointer must point outside of the sandbox.
    CHECK(OutsideSandbox(pointer));

    // 2. The object must be a valid HeapObject.
    Tagged<Object> obj_ptr(pointer);
    CHECK(Is<HeapObject>(obj_ptr));
    Tagged<HeapObject> obj = TrustedCast<HeapObject>(obj_ptr);
#ifdef VERIFY_HEAP
    Object::ObjectVerify(obj, isolate);
#endif

    // 3. The tag must match the object's instance type.
    SharedFlag is_shared =
        SharedFlag(this == &isolate->shared_trusted_pointer_table());
    IndirectPointerTag expected_tag = IndirectPointerTagFromInstanceType(
        obj->map()->instance_type(), is_shared);
    CHECK_EQ(tag, expected_tag);
  });
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX
