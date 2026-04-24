// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/code-pointer-table.h"

#include "src/common/code-memory-access-inl.h"
#include "src/execution/isolate.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/sandbox/code-pointer-table-inl.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

uint32_t CodePointerTable::Sweep(Space* space, Counters* counters) {
  uint32_t num_live_entries = GenericSweep(space);
  counters->code_pointers_count()->AddSample(num_live_entries);
  return num_live_entries;
}

void CodePointerTable::Verify(Isolate* isolate, Space* space) {
  IterateEntriesIn(space, [&](uint32_t index) {
    auto& entry = at(index);
    if (entry.IsFreelistEntry()) return;

    Address code_ptr = entry.GetCodeObject();

    // 1. The object must be a valid Code object.
    Tagged<Object> obj(code_ptr);
    CHECK(Is<Code>(obj));
    Tagged<Code> code = TrustedCast<Code>(obj);
#ifdef VERIFY_HEAP
    Object::ObjectVerify(code, isolate);
#endif

    // 2. The entrypoint must match the code's instruction start.
    Address entrypoint = entry.GetEntrypoint(code->entrypoint_tag());
    CHECK_EQ(entrypoint, code->instruction_start());
  });
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS
