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
#include "src/objects/trusted-object-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/call-site-info-tq-inl.inc"

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
  // The field can contain either a Code or a BytecodeArray, so we need to use
  // the kUnknownIndirectPointerTag. Since we can then no longer rely on the
  // type-checking mechanism of trusted pointers we need to perform manual type
  // checks afterwards.
  Tagged<Object> object = code_object_.load_maybe_empty(isolate, kAcquireLoad);
  return CheckedCast<Union<Code, BytecodeArray>>(object);
}

void CallSiteInfo::set_code_object(Tagged<HeapObject> maybe_code,
                                   WriteBarrierMode mode) {
  DCHECK(IsCode(maybe_code) || IsBytecodeArray(maybe_code) ||
         IsUndefined(maybe_code));
  if (Tagged<Union<Code, BytecodeArray>> code; TryCast(maybe_code, &code)) {
    code_object_.store(this, code, mode);
  } else {
    DCHECK(IsUndefined(maybe_code));
    code_object_.clear(this);
  }
}

Tagged<JSAny> CallSiteInfo::receiver_or_instance() const {
  return receiver_or_instance_.load();
}
void CallSiteInfo::set_receiver_or_instance(Tagged<JSAny> value,
                                            WriteBarrierMode mode) {
  receiver_or_instance_.store(this, value, mode);
}

Tagged<Union<JSFunction, Smi>> CallSiteInfo::function() const {
  return function_.load();
}
void CallSiteInfo::set_function(Tagged<Union<JSFunction, Smi>> value,
                                WriteBarrierMode mode) {
  function_.store(this, value, mode);
}

int CallSiteInfo::code_offset_or_source_position() const {
  return code_offset_or_source_position_.load().value();
}
void CallSiteInfo::set_code_offset_or_source_position(int value,
                                                      WriteBarrierMode mode) {
  code_offset_or_source_position_.store(this, Smi::FromInt(value), mode);
}

int CallSiteInfo::flags() const { return flags_.load().value(); }
void CallSiteInfo::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

Tagged<FixedArray> CallSiteInfo::parameters() const {
  return parameters_.load();
}
void CallSiteInfo::set_parameters(Tagged<FixedArray> value,
                                  WriteBarrierMode mode) {
  parameters_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CALL_SITE_INFO_INL_H_
