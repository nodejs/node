// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CALL_SITE_INFO_INL_H_
#define V8_OBJECTS_CALL_SITE_INFO_INL_H_

#include "src/objects/call-site-info.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/struct-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/call-site-info-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(CallSiteInfo)

#if V8_ENABLE_WEBASSEMBLY
BOOL_GETTER(CallSiteInfo, flags, IsWasm, IsWasmBit::kShift)
BOOL_GETTER(CallSiteInfo, flags, IsAsmJsWasm, IsAsmJsWasmBit::kShift)
BOOL_GETTER(CallSiteInfo, flags, IsAsmJsAtNumberConversion,
            IsAsmJsAtNumberConversionBit::kShift)
#if V8_ENABLE_DRUMBRAKE
BOOL_GETTER(CallSiteInfo, flags, IsWasmInterpretedFrame,
            IsWasmInterpretedFrameBit::kShift)
#endif  // V8_ENABLE_DRUMBRAKE
BOOL_GETTER(CallSiteInfo, flags, IsBuiltin, IsBuiltinBit::kShift)
#endif  // V8_ENABLE_WEBASSEMBLY
BOOL_GETTER(CallSiteInfo, flags, IsStrict, IsStrictBit::kShift)
BOOL_GETTER(CallSiteInfo, flags, IsConstructor, IsConstructorBit::kShift)
BOOL_GETTER(CallSiteInfo, flags, IsAsync, IsAsyncBit::kShift)

Tagged<HeapObject> CallSiteInfo::code_object(IsolateForSandbox isolate) const {
  DCHECK(!IsTrustedPointerFieldEmpty(kCodeObjectOffset));
  // The field can contain either a Code or a BytecodeArray, so we need to use
  // the kUnknownIndirectPointerTag. Since we can then no longer rely on the
  // type-checking mechanism of trusted pointers we need to perform manual type
  // checks afterwards.
  Tagged<Object> object =
      ReadMaybeEmptyTrustedPointerField<kUnknownIndirectPointerTag>(
          kCodeObjectOffset, isolate, kAcquireLoad);
  return CheckedCast<Union<Code, BytecodeArray>>(object);
}

void CallSiteInfo::set_code_object(Tagged<HeapObject> maybe_code,
                                   WriteBarrierMode mode) {
  DCHECK(IsCode(maybe_code) || IsBytecodeArray(maybe_code) ||
         IsUndefined(maybe_code));
  if (Tagged<Union<Code, BytecodeArray>> code; TryCast(maybe_code, &code)) {
    WriteTrustedPointerField<kUnknownIndirectPointerTag>(kCodeObjectOffset,
                                                         code);
    CONDITIONAL_TRUSTED_POINTER_WRITE_BARRIER(
        *this, kCodeObjectOffset, kUnknownIndirectPointerTag, code, mode);
  } else {
    DCHECK(IsUndefined(maybe_code));
    ClearTrustedPointerField(kCodeObjectOffset);
  }
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CALL_SITE_INFO_INL_H_
