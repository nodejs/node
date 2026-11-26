// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCRIPT_INL_H_
#define V8_OBJECTS_SCRIPT_INL_H_

#include "src/objects/script.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/heap-object-inl.h"
#include "src/objects/managed-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/smi-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/tagged-field-inl.h"
#include "src/roots/roots-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Script::Script(const AllocationWitness& witness,
               Tagged<UnionOf<String, Undefined>> source, int script_id)
    : Struct(witness.roots().script_map()),
      source_(witness, source),
      name_(witness.roots().undefined_value()),
      line_offset_(Smi::zero()),
      column_offset_(Smi::zero()),
      script_type_(Smi::FromEnum(Script::Type::kNormal)),
      line_ends_(Smi::zero()),
      id_(Smi::FromInt(script_id)),
      eval_from_shared_or_wrapped_arguments_(witness.roots().undefined_value()),
      eval_from_position_(Smi::zero()),
      eval_from_scope_info_(witness.roots().undefined_value()),
      infos_(witness.roots().empty_weak_fixed_array()),
      compiled_lazy_function_positions_(witness.roots().undefined_value()),
      flags_(Flags{}, kRelaxedStore),
      source_url_(witness.roots().undefined_value()),
      source_mapping_url_(witness.roots().undefined_value()),
      debug_id_(witness.roots().undefined_value()),
      host_defined_options_(witness.roots().empty_fixed_array()),
#ifdef V8_SCRIPTORMODULE_LEGACY_LIFETIME
      script_or_modules_(witness.roots().empty_array_list()),
#endif
      source_hash_(witness.roots().undefined_value()) {
}

Script::Script(const AllocationWitness& witness, Tagged<Script> old_script,
               Tagged<String> source, int script_id
#ifdef V8_SCRIPTORMODULE_LEGACY_LIFETIME
               ,
               Tagged<ArrayList> script_or_modules
#endif
               )
    : Struct(witness.roots().script_map()),
      source_(witness, source),
      name_(witness, old_script->name()),
      line_offset_(Smi::FromInt(old_script->line_offset())),
      column_offset_(Smi::FromInt(old_script->column_offset())),
      script_type_(Smi::FromEnum(old_script->type())),
      line_ends_(Smi::zero()),
      id_(Smi::FromInt(script_id)),
      eval_from_shared_or_wrapped_arguments_(
          witness, old_script->eval_from_shared_or_wrapped_arguments()),
      eval_from_position_(Smi::FromInt(old_script->eval_from_position())),
      eval_from_scope_info_(witness, old_script->eval_from_scope_info_.load()),
      infos_(witness.roots().empty_weak_fixed_array()),
      compiled_lazy_function_positions_(witness.roots().undefined_value()),
      flags_(old_script->flags(), kRelaxedStore),
      source_url_(witness.roots().undefined_value()),
      source_mapping_url_(witness.roots().undefined_value()),
      debug_id_(witness.roots().undefined_value()),
      host_defined_options_(witness, old_script->host_defined_options()),
#ifdef V8_SCRIPTORMODULE_LEGACY_LIFETIME
      script_or_modules_(witness, script_or_modules),
#endif
      source_hash_(witness.roots().undefined_value()) {
}

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

Tagged<UnionOf<FixedArray, SharedFunctionInfo, Undefined>>
Script::eval_from_shared_or_wrapped_arguments() const {
  return eval_from_shared_or_wrapped_arguments_.load();
}
void Script::set_eval_from_shared_or_wrapped_arguments(
    Tagged<UnionOf<FixedArray, SharedFunctionInfo, Undefined>> value,
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
  bool is_wrapped = compilation_kind() == CompilationKind::kWrapped;
  DCHECK_EQ(is_wrapped, IsFixedArray(eval_from_shared_or_wrapped_arguments()));
  return is_wrapped;
}

bool Script::is_eval() const {
  return compilation_kind() == CompilationKind::kDirectEval ||
         compilation_kind() == CompilationKind::kIndirectEval;
}

bool Script::has_eval_origin() const {
  return is_eval() ||
         compilation_kind() == CompilationKind::kFunctionConstructor;
}

bool Script::is_host() const {
  return compilation_kind() == CompilationKind::kHost;
}

LanguageMode Script::outer_language_mode() const {
  return flags().outer_language_mode();
}
void Script::set_outer_language_mode(LanguageMode mode) {
  set_flags(flags().with_outer_language_mode(mode));
}

bool Script::has_eval_from_shared() const {
  return IsSharedFunctionInfo(eval_from_shared_or_wrapped_arguments());
}

void Script::set_wrapped_arguments(Tagged<FixedArray> value,
                                   WriteBarrierMode mode) {
  DCHECK_EQ(compilation_kind(), CompilationKind::kWrapped);
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

bool Script::break_on_entry() const { return flags().break_on_entry(); }

void Script::set_break_on_entry(bool value) {
  set_flags(flags().with_break_on_entry(value));
}
#endif  // V8_ENABLE_WEBASSEMBLY

Script::Flags Script::flags() const {
  // Use a relaxed load since background compile threads read the
  // {compilation_kind()} while the foreground thread might update e.g. the
  // {origin_options}.
  return flags_.Relaxed_Load();
}

void Script::set_flags(Flags new_flags) { flags_.Relaxed_Store(new_flags); }

Script::CompilationKind Script::compilation_kind() const {
  return flags().compilation_kind();
}
void Script::set_compilation_kind(CompilationKind kind) {
  set_flags(flags().with_compilation_kind(kind));
}

Script::CompilationState Script::compilation_state() {
  return flags().compilation_state();
}
void Script::set_compilation_state(CompilationState state) {
  set_flags(flags().with_compilation_state(state));
}

bool Script::produce_compile_hints() const {
  return flags().produce_compile_hints();
}

void Script::set_produce_compile_hints(bool produce_compile_hints) {
  set_flags(flags().with_produce_compile_hints(produce_compile_hints));
}

bool Script::deserialized() const { return flags().deserialized(); }

void Script::set_deserialized(bool value) {
  set_flags(flags().with_deserialized(value));
}

bool Script::is_embedded() const { return flags().is_embedded(); }

void Script::set_is_embedded(bool value) {
  set_flags(flags().with_is_embedded(value));
}

bool Script::is_repl_mode() const { return flags().is_repl_mode(); }

void Script::set_is_repl_mode(bool value) {
  set_flags(flags().with_is_repl_mode(value));
}

ScriptOriginOptions Script::origin_options() {
  return ScriptOriginOptions(flags().origin_options());
}
void Script::set_origin_options(ScriptOriginOptions origin_options) {
  set_flags(flags().with_origin_options(origin_options.Flags()));
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCRIPT_INL_H_
