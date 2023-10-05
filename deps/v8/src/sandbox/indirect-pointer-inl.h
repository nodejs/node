// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_INDIRECT_POINTER_INL_H_
#define V8_SANDBOX_INDIRECT_POINTER_INL_H_

#include "include/v8-internal.h"
#include "src/base/atomic-utils.h"
#include "src/execution/isolate.h"
#include "src/sandbox/code-pointer-table-inl.h"
#include "src/sandbox/code-pointer.h"

namespace v8 {
namespace internal {

V8_INLINE Tagged<Object> ReadIndirectPointerField(Address field_address) {
#ifdef V8_CODE_POINTER_SANDBOXING
  // Here we assume that the load from the table cannot be reordered before the
  // load of the code object pointer due to the data dependency between the two
  // loads and therefore use relaxed memory ordering, but technically we should
  // use memory_order_consume here.
  auto location = reinterpret_cast<IndirectPointerHandle*>(field_address);
  IndirectPointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  static_assert(kAllIndirectPointerObjectsAreCode);
  return Object(GetProcessWideCodePointerTable()->GetCodeObject(handle));
#else
  UNREACHABLE();
#endif  // V8_CODE_POINTER_SANDBOXING
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_INDIRECT_POINTER_INL_H_
