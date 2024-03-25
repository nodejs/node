// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TRUSTED_OBJECT_INL_H_
#define V8_OBJECTS_TRUSTED_OBJECT_INL_H_

#include "src/objects/instance-type-inl.h"
#include "src/objects/trusted-object.h"
#include "src/sandbox/sandbox.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(TrustedObject)
OBJECT_CONSTRUCTORS_IMPL(TrustedObject, HeapObject)

ProtectedPointerSlot TrustedObject::RawProtectedPointerField(
    int byte_offset) const {
#ifdef V8_ENABLE_SANDBOX
  // These slots must only occur on trusted objects, and those must live
  // outside of the sandbox. However, it is currently possible for an attacker
  // to craft a fake trusted object inside the sandbox (by crafting a fake Map
  // with the right instance type). In that case, bad things might happen if
  // these objects are e.g. processed by a Visitor as they can typically assume
  // that these slots are trusted. The following check defends against that by
  // ensuring that the host object is outside of the sandbox.
  // See also crbug.com/1505089.
  SBXCHECK(!InsideSandbox(address()));
#endif
  return ProtectedPointerSlot(field_address(byte_offset));
}

CAST_ACCESSOR(ExposedTrustedObject)
OBJECT_CONSTRUCTORS_IMPL(ExposedTrustedObject, TrustedObject)

void ExposedTrustedObject::init_self_indirect_pointer(
    IsolateForSandbox isolate) {
#ifdef V8_ENABLE_SANDBOX
  InitSelfIndirectPointerField(kSelfIndirectPointerOffset, isolate);
#endif
}

IndirectPointerHandle ExposedTrustedObject::self_indirect_pointer_handle()
    const {
#ifdef V8_ENABLE_SANDBOX
  return Relaxed_ReadField<IndirectPointerHandle>(kSelfIndirectPointerOffset);
#else
  UNREACHABLE();
#endif
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TRUSTED_OBJECT_INL_H_
