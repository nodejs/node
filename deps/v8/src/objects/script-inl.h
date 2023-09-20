// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCRIPT_INL_H_
#define V8_OBJECTS_SCRIPT_INL_H_

#include "src/objects/managed.h"
#include "src/objects/script.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/smi-inl.h"
#include "src/objects/string-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/script-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(Script)

NEVER_READ_ONLY_SPACE_IMPL(Script)

#if V8_ENABLE_WEBASSEMBLY
ACCESSORS_CHECKED(Script, wasm_breakpoint_infos, FixedArray,
                  kEvalFromSharedOrWrappedArgumentsOffset,
                  this->type() == TYPE_WASM)
ACCESSORS_CHECKED(Script, wasm_managed_native_module, Object,
                  kEvalFromPositionOffset, this->type() == TYPE_WASM)
ACCESSORS_CHECKED(Script, wasm_weak_instance_list, WeakArrayList,
                  kSharedFunctionInfosOffset, this->type() == TYPE_WASM)
#define CHECK_SCRIPT_NOT_WASM this->type() != TYPE_WASM
#else
#define CHECK_SCRIPT_NOT_WASM true
#endif  // V8_ENABLE_WEBASSEMBLY

SMI_ACCESSORS(Script, type, kScriptTypeOffset)
ACCESSORS_CHECKED(Script, eval_from_shared_or_wrapped_arguments, Object,
                  kEvalFromSharedOrWrappedArgumentsOffset,
                  CHECK_SCRIPT_NOT_WASM)
SMI_ACCESSORS_CHECKED(Script, eval_from_position, kEvalFromPositionOffset,
                      CHECK_SCRIPT_NOT_WASM)
#undef CHECK_SCRIPT_NOT_WASM

ACCESSORS(Script, compiled_lazy_function_positions, Object,
          kCompiledLazyFunctionPositionsOffset)

bool Script::is_wrapped() const {
  return eval_from_shared_or_wrapped_arguments().IsFixedArray();
}

bool Script::has_eval_from_shared() const {
  return eval_from_shared_or_wrapped_arguments().IsSharedFunctionInfo();
}

void Script::set_eval_from_shared(SharedFunctionInfo shared,
                                  WriteBarrierMode mode) {
  DCHECK(!is_wrapped());
  set_eval_from_shared_or_wrapped_arguments(shared, mode);
}

SharedFunctionInfo Script::eval_from_shared() const {
  DCHECK(has_eval_from_shared());
  return SharedFunctionInfo::cast(eval_from_shared_or_wrapped_arguments());
}

void Script::set_wrapped_arguments(FixedArray value, WriteBarrierMode mode) {
  DCHECK(!has_eval_from_shared());
  set_eval_from_shared_or_wrapped_arguments(value, mode);
}

FixedArray Script::wrapped_arguments() const {
  DCHECK(is_wrapped());
  return FixedArray::cast(eval_from_shared_or_wrapped_arguments());
}

DEF_GETTER(Script, shared_function_infos, WeakFixedArray) {
#if V8_ENABLE_WEBASSEMBLY
  if (type() == TYPE_WASM) {
    return ReadOnlyRoots(GetHeap()).empty_weak_fixed_array();
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  return TaggedField<WeakFixedArray, kSharedFunctionInfosOffset>::load(*this);
}

void Script::set_shared_function_infos(WeakFixedArray value,
                                       WriteBarrierMode mode) {
#if V8_ENABLE_WEBASSEMBLY
  DCHECK_NE(TYPE_WASM, type());
#endif  // V8_ENABLE_WEBASSEMBLY
  TaggedField<WeakFixedArray, kSharedFunctionInfosOffset>::store(*this, value);
  CONDITIONAL_WRITE_BARRIER(*this, kSharedFunctionInfosOffset, value, mode);
}

int Script::shared_function_info_count() const {
  return shared_function_infos().length();
}

#if V8_ENABLE_WEBASSEMBLY
bool Script::has_wasm_breakpoint_infos() const {
  return type() == TYPE_WASM && wasm_breakpoint_infos().length() > 0;
}

wasm::NativeModule* Script::wasm_native_module() const {
  return Managed<wasm::NativeModule>::cast(wasm_managed_native_module()).raw();
}

bool Script::break_on_entry() const { return BreakOnEntryBit::decode(flags()); }

void Script::set_break_on_entry(bool value) {
  set_flags(BreakOnEntryBit::update(flags(), value));
}
#endif  // V8_ENABLE_WEBASSEMBLY

Script::CompilationType Script::compilation_type() {
  return CompilationTypeBit::decode(flags());
}
void Script::set_compilation_type(CompilationType type) {
  set_flags(CompilationTypeBit::update(flags(), type));
}
Script::CompilationState Script::compilation_state() {
  return CompilationStateBit::decode(flags());
}
void Script::set_compilation_state(CompilationState state) {
  set_flags(CompilationStateBit::update(flags(), state));
}

bool Script::produce_compile_hints() const {
  return ProduceCompileHintsBit::decode(flags());
}

void Script::set_produce_compile_hints(bool produce_compile_hints) {
  set_flags(ProduceCompileHintsBit::update(flags(), produce_compile_hints));
}

bool Script::is_repl_mode() const { return IsReplModeBit::decode(flags()); }

void Script::set_is_repl_mode(bool value) {
  set_flags(IsReplModeBit::update(flags(), value));
}

ScriptOriginOptions Script::origin_options() {
  return ScriptOriginOptions(OriginOptionsBits::decode(flags()));
}
void Script::set_origin_options(ScriptOriginOptions origin_options) {
  DCHECK(!(origin_options.Flags() & ~((1 << OriginOptionsBits::kSize) - 1)));
  set_flags(OriginOptionsBits::update(flags(), origin_options.Flags()));
}

bool Script::HasValidSource() {
  Object src = this->source();
  if (!src.IsString()) return true;
  String src_str = String::cast(src);
  if (!StringShape(src_str).IsExternal()) return true;
  if (src_str.IsOneByteRepresentation()) {
    return ExternalOneByteString::cast(src).resource() != nullptr;
  } else if (src_str.IsTwoByteRepresentation()) {
    return ExternalTwoByteString::cast(src).resource() != nullptr;
  }
  return true;
}

bool Script::HasSourceURLComment() const {
  return source_url().IsString() && String::cast(source_url()).length() != 0;
}

bool Script::IsMaybeUnfinalized(Isolate* isolate) const {
  // TODO(v8:12051): A more robust detection, e.g. with a dedicated sentinel
  // value.
  return source().IsUndefined(isolate) || String::cast(source()).length() == 0;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCRIPT_INL_H_
