// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCRIPT_INL_H_
#define V8_OBJECTS_SCRIPT_INL_H_

#include "src/objects/script.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/smi-inl.h"
#include "src/objects/string-inl.h"
#include "src/objects/struct-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/script-tq-inl.inc"

Tagged<UnionOf<String, Undefined>> Script::source() const {
  return source_.load();
}
void Script::set_source(Tagged<UnionOf<String, Undefined>> value,
                        WriteBarrierMode mode) {
  source_.store(this, value, mode);
}

Tagged<Object> Script::name() const { return name_.load(); }
void Script::set_name(Tagged<Object> value, WriteBarrierMode mode) {
  name_.store(this, value, mode);
}

int Script::line_offset() const { return line_offset_.load().value(); }
void Script::set_line_offset(int value) {
  line_offset_.store(this, Smi::FromInt(value));
}

int Script::column_offset() const { return column_offset_.load().value(); }
void Script::set_column_offset(int value) {
  column_offset_.store(this, Smi::FromInt(value));
}

Tagged<UnionOf<Smi, Undefined, Symbol>> Script::context_data() const {
  return context_data_.load();
}
void Script::set_context_data(Tagged<UnionOf<Smi, Undefined, Symbol>> value,
                              WriteBarrierMode mode) {
  context_data_.store(this, value, mode);
}

Script::Type Script::type() const {
  return static_cast<Type>(script_type_.load().value());
}
void Script::set_type(Type value) {
  script_type_.store(this, Smi::FromInt(static_cast<int>(value)));
}

Tagged<UnionOf<FixedArray, Smi>> Script::line_ends() const {
  return line_ends_.load();
}
void Script::set_line_ends(Tagged<UnionOf<FixedArray, Smi>> value,
                           WriteBarrierMode mode) {
  line_ends_.store(this, value, mode);
}

int Script::id() const { return id_.load().value(); }
void Script::set_id(int value) { id_.store(this, Smi::FromInt(value)); }

Tagged<Object> Script::eval_from_shared_or_wrapped_arguments() const {
  return eval_from_shared_or_wrapped_arguments_.load();
}
void Script::set_eval_from_shared_or_wrapped_arguments(Tagged<Object> value,
                                                       WriteBarrierMode mode) {
  eval_from_shared_or_wrapped_arguments_.store(this, value, mode);
}

#if V8_ENABLE_WEBASSEMBLY
Tagged<FixedArray> Script::wasm_breakpoint_infos() const {
  DCHECK_EQ(type(), Type::kWasm);
  return Cast<FixedArray>(eval_from_shared_or_wrapped_arguments());
}
void Script::set_wasm_breakpoint_infos(Tagged<FixedArray> value,
                                       WriteBarrierMode mode) {
  DCHECK_EQ(type(), Type::kWasm);
  set_eval_from_shared_or_wrapped_arguments(value, mode);
}

Tagged<Object> Script::wasm_managed_native_module() const {
  DCHECK_EQ(type(), Type::kWasm);
  return eval_from_position_.load();
}
void Script::set_wasm_managed_native_module(Tagged<Object> value,
                                            WriteBarrierMode mode) {
  DCHECK_EQ(type(), Type::kWasm);
  eval_from_position_.store(this, Cast<UnionOf<Smi, Foreign>>(value), mode);
}

Tagged<WeakArrayList> Script::wasm_weak_instance_list() const {
  DCHECK_EQ(type(), Type::kWasm);
  return Cast<WeakArrayList>(infos_.load());
}
void Script::set_wasm_weak_instance_list(Tagged<WeakArrayList> value,
                                         WriteBarrierMode mode) {
  DCHECK_EQ(type(), Type::kWasm);
  infos_.store(this, value, mode);
}
#endif  // V8_ENABLE_WEBASSEMBLY

bool Script::is_wrapped() const {
  return IsFixedArray(eval_from_shared_or_wrapped_arguments());
}

bool Script::has_eval_from_shared() const {
  return IsSharedFunctionInfo(eval_from_shared_or_wrapped_arguments());
}

void Script::set_eval_from_shared(Tagged<SharedFunctionInfo> shared,
                                  WriteBarrierMode mode) {
  DCHECK(!is_wrapped());
  set_eval_from_shared_or_wrapped_arguments(shared, mode);
}

Tagged<SharedFunctionInfo> Script::eval_from_shared() const {
  DCHECK(has_eval_from_shared());
  return Cast<SharedFunctionInfo>(eval_from_shared_or_wrapped_arguments());
}

void Script::set_wrapped_arguments(Tagged<FixedArray> value,
                                   WriteBarrierMode mode) {
  DCHECK(!has_eval_from_shared());
  set_eval_from_shared_or_wrapped_arguments(value, mode);
}

Tagged<FixedArray> Script::wrapped_arguments() const {
  DCHECK(is_wrapped());
  return Cast<FixedArray>(eval_from_shared_or_wrapped_arguments());
}

int Script::eval_from_position() const {
#if V8_ENABLE_WEBASSEMBLY
  DCHECK_NE(type(), Type::kWasm);
#endif  // V8_ENABLE_WEBASSEMBLY
  return Cast<Smi>(eval_from_position_.load()).value();
}
void Script::set_eval_from_position(int value) {
#if V8_ENABLE_WEBASSEMBLY
  DCHECK_NE(type(), Type::kWasm);
#endif  // V8_ENABLE_WEBASSEMBLY
  eval_from_position_.store(this, Smi::FromInt(value));
}

Tagged<Object> Script::eval_from_scope_info() const {
  return eval_from_scope_info_.load();
}
void Script::set_eval_from_scope_info(Tagged<Object> value,
                                      WriteBarrierMode mode) {
  eval_from_scope_info_.store(this, Cast<UnionOf<ScopeInfo, Undefined>>(value),
                              mode);
}

bool Script::has_eval_from_scope_info() const {
  return IsScopeInfo(eval_from_scope_info());
}

Tagged<WeakFixedArray> Script::infos() const {
#if V8_ENABLE_WEBASSEMBLY
  if (type() == Type::kWasm) {
    return GetReadOnlyRoots().empty_weak_fixed_array();
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  return Cast<WeakFixedArray>(infos_.load());
}

void Script::set_infos(Tagged<WeakFixedArray> value, WriteBarrierMode mode) {
#if V8_ENABLE_WEBASSEMBLY
  DCHECK_NE(Type::kWasm, type());
#endif  // V8_ENABLE_WEBASSEMBLY
  infos_.store(this, value, mode);
}

#if V8_ENABLE_WEBASSEMBLY
bool Script::has_wasm_breakpoint_infos() const {
  return type() == Type::kWasm &&
         wasm_breakpoint_infos()->ulength().value() > 0;
}

Managed<wasm::NativeModule>::Ptr Script::wasm_native_module() const {
  return Cast<Managed<wasm::NativeModule>>(wasm_managed_native_module())->ptr();
}

bool Script::break_on_entry() const { return BreakOnEntryBit::decode(flags()); }

void Script::set_break_on_entry(bool value) {
  set_flags(BreakOnEntryBit::update(flags(), value));
}
#endif  // V8_ENABLE_WEBASSEMBLY

uint32_t Script::flags() const {
  // Use a relaxed load since background compile threads read the
  // {compilation_type()} while the foreground thread might update e.g. the
  // {origin_options}.
  return flags_.Relaxed_Load().value();
}

void Script::set_flags(uint32_t new_flags) {
  DCHECK(is_int31(new_flags));
  flags_.Relaxed_Store(this, Smi::FromInt(new_flags));
}

Script::CompilationType Script::compilation_type() const {
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

bool Script::deserialized() const { return DeserializedBit::decode(flags()); }

void Script::set_deserialized(bool value) {
  set_flags(DeserializedBit::update(flags(), value));
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

Tagged<UnionOf<ArrayList, Undefined>> Script::compiled_lazy_function_positions()
    const {
  return compiled_lazy_function_positions_.load();
}
void Script::set_compiled_lazy_function_positions(
    Tagged<UnionOf<ArrayList, Undefined>> value, WriteBarrierMode mode) {
  compiled_lazy_function_positions_.store(this, value, mode);
}

Tagged<UnionOf<String, Undefined>> Script::source_url() const {
  return source_url_.load();
}
void Script::set_source_url(Tagged<UnionOf<String, Undefined>> value,
                            WriteBarrierMode mode) {
  source_url_.store(this, value, mode);
}

Tagged<Object> Script::source_mapping_url() const {
  return source_mapping_url_.load();
}
void Script::set_source_mapping_url(Tagged<Object> value,
                                    WriteBarrierMode mode) {
  source_mapping_url_.store(this, value, mode);
}

Tagged<UnionOf<String, Undefined>> Script::debug_id() const {
  return debug_id_.load();
}
void Script::set_debug_id(Tagged<UnionOf<String, Undefined>> value,
                          WriteBarrierMode mode) {
  debug_id_.store(this, value, mode);
}

Tagged<FixedArray> Script::host_defined_options() const {
  return host_defined_options_.load();
}
void Script::set_host_defined_options(Tagged<FixedArray> value,
                                      WriteBarrierMode mode) {
  host_defined_options_.store(this, value, mode);
}

#if V8_SCRIPTORMODULE_LEGACY_LIFETIME
Tagged<ArrayList> Script::script_or_modules() const {
  return script_or_modules_.load();
}
void Script::set_script_or_modules(Tagged<ArrayList> value,
                                   WriteBarrierMode mode) {
  script_or_modules_.store(this, value, mode);
}
#endif

Tagged<UnionOf<String, Undefined>> Script::source_hash() const {
  return source_hash_.load();
}
void Script::set_source_hash(Tagged<UnionOf<String, Undefined>> value,
                             WriteBarrierMode mode) {
  source_hash_.store(this, value, mode);
}

bool Script::HasValidSource() {
  Tagged<Object> src = this->source();
  if (!IsString(src)) return true;
  Tagged<String> src_str = Cast<String>(src);
  if (!StringShape(src_str).IsExternal()) return true;
  if (src_str->IsOneByteRepresentation()) {
    return Cast<ExternalOneByteString>(src)->resource() != nullptr;
  } else if (src_str->IsTwoByteRepresentation()) {
    return Cast<ExternalTwoByteString>(src)->resource() != nullptr;
  }
  return true;
}

bool Script::has_line_ends() const { return line_ends() != Smi::zero(); }

bool Script::CanHaveLineEnds() const {
#if V8_ENABLE_WEBASSEMBLY
  return type() != Script::Type::kWasm;
#else
  return true;
#endif  // V8_ENABLE_WEBASSEMBLY
}

// static
void Script::InitLineEnds(Isolate* isolate, DirectHandle<Script> script) {
  if (script->has_line_ends()) return;
  Script::InitLineEndsInternal(isolate, script);
}
// static
void Script::InitLineEnds(LocalIsolate* isolate, DirectHandle<Script> script) {
  if (script->has_line_ends()) return;
  Script::InitLineEndsInternal(isolate, script);
}

bool Script::HasSourceURLComment() const {
  return IsString(source_url()) && Cast<String>(source_url())->length() != 0;
}

bool Script::HasSourceMappingURLComment() const {
  return IsString(source_mapping_url()) &&
         Cast<String>(source_mapping_url())->length() != 0;
}

bool Script::IsMaybeUnfinalized(Isolate* isolate) const {
  // TODO(v8:12051): A more robust detection, e.g. with a dedicated sentinel
  // value.
  return IsUndefined(source(), isolate) ||
         Cast<String>(source())->length() == 0;
}

Tagged<Script> Script::GetEvalOrigin() {
  DisallowGarbageCollection no_gc;
  Tagged<Script> origin_script = this;
  while (origin_script->has_eval_from_shared()) {
    Tagged<HeapObject> maybe_script =
        origin_script->eval_from_shared()->script();
    CHECK(IsScript(maybe_script));
    origin_script = Cast<Script>(maybe_script);
  }
  return origin_script;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCRIPT_INL_H_
