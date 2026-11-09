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

#if V8_ENABLE_WEBASSEMBLY
BOOL_GETTER(CallSiteInfo, flags, IsWasm, IsWasmBit::kShift)
#if V8_ENABLE_DRUMBRAKE
BOOL_GETTER(CallSiteInfo, flags, IsWasmInterpretedFrame,
            IsWasmInterpretedFrameBit::kShift)
#endif  // V8_ENABLE_DRUMBRAKE
BOOL_GETTER(CallSiteInfo, flags, IsBuiltin, IsBuiltinBit::kShift)
#endif  // V8_ENABLE_WEBASSEMBLY
BOOL_GETTER(CallSiteInfo, flags, IsStrict, IsStrictBit::kShift)
BOOL_GETTER(CallSiteInfo, flags, IsConstructor, IsConstructorBit::kShift)
BOOL_GETTER(CallSiteInfo, flags, IsAsync, IsAsyncBit::kShift)

CallSiteInfo::CallSiteInfo(
    const AllocationWitness& witness, Tagged<JSAny> receiver_or_instance,
    Tagged<Union<JSFunction, Smi>> function,
    Tagged<Union<Code, BytecodeArray, Undefined>> code_object,
    int code_offset_or_source_position, int flags)
    : Struct(witness.roots().call_site_info_map()),
      code_object_(witness, code_object),
      receiver_or_instance_(witness, receiver_or_instance),
      function_(witness, function),
      code_offset_or_source_position_(
          Smi::FromInt(code_offset_or_source_position)),
      flags_(Smi::FromInt(flags)) {}

Tagged<HeapObject> CallSiteInfo::code_object(IsolateForSandbox isolate) const {
  // The field can contain either a Code or a BytecodeArray, so we need to use
  // the kUnknownIndirectPointerTag. Since we can then no longer rely on the
  // type-checking mechanism of trusted pointers we need to perform manual type
  // checks afterwards.
  Tagged<Object> object = code_object_.Acquire_Load_maybe_empty(isolate);
  return CheckedCast<Union<Code, BytecodeArray>>(object);
}

Tagged<JSAny> CallSiteInfo::receiver_or_instance() const {
  return receiver_or_instance_.load();
}

Tagged<Union<JSFunction, Smi>> CallSiteInfo::function() const {
  return function_.load();
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CALL_SITE_INFO_INL_H_
