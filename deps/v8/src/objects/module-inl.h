// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MODULE_INL_H_
#define V8_OBJECTS_MODULE_INL_H_

#include "src/objects/module.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/scope-info.h"
#include "src/objects/source-text-module.h"
#include "src/objects/string-inl.h"
#include "src/objects/synthetic-module.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/module-tq-inl.inc"

Tagged<Object> ScriptOrModule::resource_name() const {
  return resource_name_.load();
}
void ScriptOrModule::set_resource_name(Tagged<Object> value,
                                       WriteBarrierMode mode) {
  resource_name_.store(this, value, mode);
}

Tagged<FixedArray> ScriptOrModule::host_defined_options() const {
  return host_defined_options_.load();
}
void ScriptOrModule::set_host_defined_options(Tagged<FixedArray> value,
                                              WriteBarrierMode mode) {
  host_defined_options_.store(this, value, mode);
}

BOOL_ACCESSORS(SourceTextModule, flags, has_toplevel_await,
               HasToplevelAwaitBit::kShift)
BIT_FIELD_ACCESSORS(SourceTextModule, flags, async_evaluation_ordinal,
                    SourceTextModule::AsyncEvaluationOrdinalBits)

BIT_FIELD_ACCESSORS(ModuleRequest, flags, position, ModuleRequest::PositionBits)

inline void ModuleRequest::set_phase(ModuleImportPhase phase) {
  DCHECK(PhaseBits::is_valid(phase));
  uint32_t hints = flags();
  hints = PhaseBits::update(hints, phase);
  set_flags(hints);
}

inline ModuleImportPhase ModuleRequest::phase() const {
  ModuleImportPhase value = PhaseBits::decode(flags());
  DCHECK(value == ModuleImportPhase::kDefer ||
         value == ModuleImportPhase::kEvaluation ||
         value == ModuleImportPhase::kSource);
  return static_cast<ModuleImportPhase>(value);
}

struct Module::Hash {
  V8_INLINE size_t operator()(Tagged<Module> module) const {
    return module->hash();
  }
};

Tagged<SourceTextModuleInfo> SourceTextModule::info() const {
  return GetSharedFunctionInfo()->scope_info()->ModuleDescriptorInfo();
}

Tagged<FixedArray> SourceTextModuleInfo::module_requests() const {
  return Cast<FixedArray>(get(kModuleRequestsIndex));
}

Tagged<FixedArray> SourceTextModuleInfo::special_exports() const {
  return Cast<FixedArray>(get(kSpecialExportsIndex));
}

Tagged<FixedArray> SourceTextModuleInfo::regular_exports() const {
  return Cast<FixedArray>(get(kRegularExportsIndex));
}

Tagged<FixedArray> SourceTextModuleInfo::regular_imports() const {
  return Cast<FixedArray>(get(kRegularImportsIndex));
}

Tagged<FixedArray> SourceTextModuleInfo::namespace_imports() const {
  return Cast<FixedArray>(get(kNamespaceImportsIndex));
}

// TODO(crbug.com/401059828): make it DEBUG only, once investigation is over.
bool SourceTextModuleInfo::Equals(Tagged<SourceTextModuleInfo> other) const {
  return regular_exports() == other->regular_exports() &&
         regular_imports() == other->regular_imports() &&
         special_exports() == other->special_exports() &&
         namespace_imports() == other->namespace_imports() &&
         module_requests() == other->module_requests();
}

struct ModuleHandleHash {
  V8_INLINE size_t operator()(DirectHandle<Module> module) const {
    return module->hash();
  }
};

struct ModuleHandleEqual {
  V8_INLINE bool operator()(DirectHandle<Module> lhs,
                            DirectHandle<Module> rhs) const {
    return *lhs == *rhs;
  }
};

class UnorderedModuleSet
    : public std::unordered_set<Handle<Module>, ModuleHandleHash,
                                ModuleHandleEqual,
                                ZoneAllocator<Handle<Module>>> {
 public:
  explicit UnorderedModuleSet(Zone* zone)
      : std::unordered_set<Handle<Module>, ModuleHandleHash, ModuleHandleEqual,
                           ZoneAllocator<Handle<Module>>>(
            2 /* bucket count */, ModuleHandleHash(), ModuleHandleEqual(),
            ZoneAllocator<Handle<Module>>(zone)) {}
};

Handle<SourceTextModule> SourceTextModule::GetCycleRoot(
    Isolate* isolate) const {
  CHECK_GE(status(), kEvaluatingAsync);
  DCHECK(!IsTheHole(cycle_root(), isolate));
  Handle<SourceTextModule> root(Cast<SourceTextModule>(cycle_root()), isolate);
  return root;
}

void SourceTextModule::AddAsyncParentModule(
    Isolate* isolate, DirectHandle<SourceTextModule> module,
    DirectHandle<SourceTextModule> parent) {
  DirectHandle<ArrayList> async_parent_modules(module->async_parent_modules(),
                                               isolate);
  DirectHandle<ArrayList> new_array_list =
      ArrayList::Add(isolate, async_parent_modules, parent);
  module->set_async_parent_modules(*new_array_list);
}

Handle<SourceTextModule> SourceTextModule::GetAsyncParentModule(
    Isolate* isolate, uint32_t index) {
  Handle<SourceTextModule> module(
      Cast<SourceTextModule>(async_parent_modules()->get(index)), isolate);
  return module;
}

uint32_t SourceTextModule::AsyncParentModuleCount() {
  return async_parent_modules()->ulength().value();
}

bool SourceTextModule::HasAsyncEvaluationOrdinal() const {
  return async_evaluation_ordinal() >= kFirstAsyncEvaluationOrdinal;
}

bool SourceTextModule::HasPendingAsyncDependencies() {
  DCHECK_GE(pending_async_dependencies(), 0);
  return pending_async_dependencies() > 0;
}

void SourceTextModule::IncrementPendingAsyncDependencies() {
  set_pending_async_dependencies(pending_async_dependencies() + 1);
}

void SourceTextModule::DecrementPendingAsyncDependencies() {
  set_pending_async_dependencies(pending_async_dependencies() - 1);
}

Tagged<ObjectHashTable> Module::exports() const { return exports_.load(); }
void Module::set_exports(Tagged<ObjectHashTable> value, WriteBarrierMode mode) {
  exports_.store(this, value, mode);
}

int Module::hash() const { return hash_.load().value(); }
void Module::set_hash(int value) { hash_.store(this, Smi::FromInt(value)); }

int Module::status() const { return status_.load().value(); }
void Module::set_status(int value) { status_.store(this, Smi::FromInt(value)); }

Tagged<UnionOf<Cell, Undefined>> Module::module_namespace() const {
  return module_namespace_.load();
}
void Module::set_module_namespace(Tagged<UnionOf<Cell, Undefined>> value,
                                  WriteBarrierMode mode) {
  module_namespace_.store(this, value, mode);
}

Tagged<UnionOf<Cell, Undefined>> Module::deferred_module_namespace() const {
  return deferred_module_namespace_.load();
}
void Module::set_deferred_module_namespace(
    Tagged<UnionOf<Cell, Undefined>> value, WriteBarrierMode mode) {
  deferred_module_namespace_.store(this, value, mode);
}

Tagged<Module> JSModuleNamespace::module() const { return module_.load(); }

void JSModuleNamespace::set_module(Tagged<Module> value,
                                   WriteBarrierMode mode) {
  module_.store(this, value, mode);
}

Tagged<Object> Module::exception() const { return exception_.load(); }
void Module::set_exception(Tagged<Object> value, WriteBarrierMode mode) {
  exception_.store(this, value, mode);
}

Tagged<UnionOf<JSPromise, Undefined>> Module::top_level_capability() const {
  return top_level_capability_.load();
}
void Module::set_top_level_capability(
    Tagged<UnionOf<JSPromise, Undefined>> value, WriteBarrierMode mode) {
  top_level_capability_.store(this, value, mode);
}
}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MODULE_INL_H_
