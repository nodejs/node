// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SOURCE_TEXT_MODULE_INL_H_
#define V8_OBJECTS_SOURCE_TEXT_MODULE_INL_H_

#include "src/objects/source-text-module.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/js-function-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball-predicates-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/struct-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

ModuleRequest::ModuleRequest(const AllocationWitness& witness,
                             Tagged<String> specifier, ModuleImportPhase phase,
                             Tagged<FixedArray> import_attributes, int position)
    : Struct(witness.roots().module_request_map()),
      specifier_(witness, specifier),
      import_attributes_(witness, import_attributes),
      flags_(Smi::From31BitPattern(PhaseBits::encode(phase) |
                                   PositionBits::encode(position))) {
  DCHECK_GE(position, 0);
}

Tagged<String> ModuleRequest::specifier() const { return specifier_.load(); }

Tagged<FixedArray> ModuleRequest::import_attributes() const {
  return import_attributes_.load();
}

uint32_t ModuleRequest::flags() const {
  return static_cast<uint32_t>(flags_.load().value());
}

SourceTextModuleInfoEntry::SourceTextModuleInfoEntry(
    const AllocationWitness& witness,
    Tagged<UnionOf<String, Undefined>> export_name,
    Tagged<UnionOf<String, Undefined>> local_name,
    Tagged<UnionOf<String, Undefined>> import_name, int module_request,
    int cell_index, int beg_pos, int end_pos)
    : Struct(witness.roots().module_info_entry_map()),
      export_name_(witness, export_name),
      local_name_(witness, local_name),
      import_name_(witness, import_name),
      module_request_(Smi::FromInt(module_request)),
      cell_index_(Smi::FromInt(cell_index)),
      beg_pos_(Smi::FromInt(beg_pos)),
      end_pos_(Smi::FromInt(end_pos)) {}

Tagged<UnionOf<String, Undefined>> SourceTextModuleInfoEntry::export_name()
    const {
  return export_name_.load();
}

Tagged<UnionOf<String, Undefined>> SourceTextModuleInfoEntry::local_name()
    const {
  return local_name_.load();
}

Tagged<UnionOf<String, Undefined>> SourceTextModuleInfoEntry::import_name()
    const {
  return import_name_.load();
}

int SourceTextModuleInfoEntry::module_request() const {
  return module_request_.load().value();
}

int SourceTextModuleInfoEntry::cell_index() const {
  return cell_index_.load().value();
}

int SourceTextModuleInfoEntry::beg_pos() const {
  return beg_pos_.load().value();
}

int SourceTextModuleInfoEntry::end_pos() const {
  return end_pos_.load().value();
}

Tagged<UnionOf<SharedFunctionInfo, JSFunction, JSGeneratorObject>>
SourceTextModule::code() const {
  return code_.load();
}
void SourceTextModule::set_code(
    Tagged<UnionOf<SharedFunctionInfo, JSFunction, JSGeneratorObject>> value,
    WriteBarrierMode mode) {
  code_.store(this, value, mode);
}

Tagged<FixedArray> SourceTextModule::regular_exports() const {
  return regular_exports_.load();
}
void SourceTextModule::set_regular_exports(Tagged<FixedArray> value,
                                           WriteBarrierMode mode) {
  regular_exports_.store(this, value, mode);
}

Tagged<FixedArray> SourceTextModule::regular_imports() const {
  return regular_imports_.load();
}
void SourceTextModule::set_regular_imports(Tagged<FixedArray> value,
                                           WriteBarrierMode mode) {
  regular_imports_.store(this, value, mode);
}

Tagged<FixedArray> SourceTextModule::requested_modules() const {
  return requested_modules_.load();
}
void SourceTextModule::set_requested_modules(Tagged<FixedArray> value,
                                             WriteBarrierMode mode) {
  requested_modules_.store(this, value, mode);
}

Tagged<UnionOf<TheHole, JSObject>> SourceTextModule::import_meta(
    AcquireLoadTag tag) const {
  return import_meta_.Acquire_Load();
}
void SourceTextModule::set_import_meta(Tagged<UnionOf<TheHole, JSObject>> value,
                                       ReleaseStoreTag tag,
                                       WriteBarrierMode mode) {
  import_meta_.Release_Store(this, value, mode);
}

Tagged<UnionOf<SourceTextModule, TheHole>> SourceTextModule::cycle_root()
    const {
  return cycle_root_.load();
}
void SourceTextModule::set_cycle_root(
    Tagged<UnionOf<SourceTextModule, TheHole>> value, WriteBarrierMode mode) {
  cycle_root_.store(this, value, mode);
}

Tagged<ArrayList> SourceTextModule::async_parent_modules() const {
  return async_parent_modules_.load();
}
void SourceTextModule::set_async_parent_modules(Tagged<ArrayList> value,
                                                WriteBarrierMode mode) {
  async_parent_modules_.store(this, value, mode);
}

int SourceTextModule::dfs_index() const { return dfs_index_.load().value(); }
void SourceTextModule::set_dfs_index(int value) {
  dfs_index_.store(this, Smi::FromInt(value));
}

int SourceTextModule::dfs_ancestor_index() const {
  return dfs_ancestor_index_.load().value();
}
void SourceTextModule::set_dfs_ancestor_index(int value) {
  dfs_ancestor_index_.store(this, Smi::FromInt(value));
}

int SourceTextModule::pending_async_dependencies() const {
  return pending_async_dependencies_.load().value();
}
void SourceTextModule::set_pending_async_dependencies(int value) {
  pending_async_dependencies_.store(this, Smi::FromInt(value));
}

uint32_t SourceTextModule::flags() const { return flags_.load().value(); }
void SourceTextModule::set_flags(uint32_t value) {
  flags_.store(this, Smi::FromInt(value));
}
}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SOURCE_TEXT_MODULE_INL_H_
