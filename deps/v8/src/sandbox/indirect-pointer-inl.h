// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_INDIRECT_POINTER_INL_H_
#define V8_SANDBOX_INDIRECT_POINTER_INL_H_

#include "include/v8-internal.h"
#include "src/base/atomic-utils.h"
#include "src/execution/isolate.h"
#include "src/sandbox/code-pointer-table-inl.h"
#include "src/sandbox/indirect-pointer-table-inl.h"
#include "src/sandbox/indirect-pointer.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX

template <IndirectPointerTag tag>
V8_INLINE Tagged<Object> ReadIndirectPointerField(Address field_address,
                                                  const Isolate* isolate) {
  auto location = reinterpret_cast<IndirectPointerHandle*>(field_address);
  IndirectPointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  DCHECK_NE(handle, kNullIndirectPointerHandle);

  if constexpr (tag == kCodeIndirectPointerTag) {
    // These are special as they use the code pointer table, not the indirect
    // pointer table.
    // Here we assume that the load from the table cannot be reordered before
    // the load of the code object pointer due to the data dependency between
    // the two loads and therefore use relaxed memory ordering, but technically
    // we should use memory_order_consume here.
    CodePointerTable* table = GetProcessWideCodePointerTable();
    return Tagged<Object>(table->GetCodeObject(handle));
  }

  const IndirectPointerTable& table = isolate->indirect_pointer_table();
  return Tagged<Object>(table.Get(handle));
}

#endif  // V8_ENABLE_SANDBOX
        //
}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_INDIRECT_POINTER_INL_H_
