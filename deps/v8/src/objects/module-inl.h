// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MODULE_INL_H_
#define V8_OBJECTS_MODULE_INL_H_

#include "src/objects/module.h"
#include "src/objects/source-text-module.h"
#include "src/objects/synthetic-module.h"

#include "src/objects/objects-inl.h"  // Needed for write barriers
#include "src/objects/scope-info.h"
#include "src/objects/string-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(Module, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(SourceTextModule, Module)
OBJECT_CONSTRUCTORS_IMPL(SourceTextModuleInfoEntry, Struct)
OBJECT_CONSTRUCTORS_IMPL(SyntheticModule, Module)
OBJECT_CONSTRUCTORS_IMPL(JSModuleNamespace, JSObject)

NEVER_READ_ONLY_SPACE_IMPL(Module)
NEVER_READ_ONLY_SPACE_IMPL(SourceTextModule)
NEVER_READ_ONLY_SPACE_IMPL(SyntheticModule)

CAST_ACCESSOR(Module)
CAST_ACCESSOR(SourceTextModule)
CAST_ACCESSOR(SyntheticModule)
ACCESSORS(Module, exports, ObjectHashTable, kExportsOffset)
ACCESSORS(Module, module_namespace, HeapObject, kModuleNamespaceOffset)
ACCESSORS(Module, exception, Object, kExceptionOffset)
SMI_ACCESSORS(Module, status, kStatusOffset)
SMI_ACCESSORS(Module, hash, kHashOffset)

ACCESSORS(SourceTextModule, code, Object, kCodeOffset)
ACCESSORS(SourceTextModule, regular_exports, FixedArray, kRegularExportsOffset)
ACCESSORS(SourceTextModule, regular_imports, FixedArray, kRegularImportsOffset)
ACCESSORS(SourceTextModule, requested_modules, FixedArray,
          kRequestedModulesOffset)
ACCESSORS(SourceTextModule, script, Script, kScriptOffset)
ACCESSORS(SourceTextModule, import_meta, Object, kImportMetaOffset)
SMI_ACCESSORS(SourceTextModule, dfs_index, kDfsIndexOffset)
SMI_ACCESSORS(SourceTextModule, dfs_ancestor_index, kDfsAncestorIndexOffset)

ACCESSORS(SyntheticModule, name, String, kNameOffset)
ACCESSORS(SyntheticModule, export_names, FixedArray, kExportNamesOffset)
ACCESSORS(SyntheticModule, evaluation_steps, Foreign, kEvaluationStepsOffset)

SourceTextModuleInfo SourceTextModule::info() const {
  return (status() >= kEvaluating)
             ? SourceTextModuleInfo::cast(code())
             : GetSharedFunctionInfo().scope_info().ModuleDescriptorInfo();
}

CAST_ACCESSOR(JSModuleNamespace)
ACCESSORS(JSModuleNamespace, module, Module, kModuleOffset)

CAST_ACCESSOR(SourceTextModuleInfoEntry)
ACCESSORS(SourceTextModuleInfoEntry, export_name, Object, kExportNameOffset)
ACCESSORS(SourceTextModuleInfoEntry, local_name, Object, kLocalNameOffset)
ACCESSORS(SourceTextModuleInfoEntry, import_name, Object, kImportNameOffset)
SMI_ACCESSORS(SourceTextModuleInfoEntry, module_request, kModuleRequestOffset)
SMI_ACCESSORS(SourceTextModuleInfoEntry, cell_index, kCellIndexOffset)
SMI_ACCESSORS(SourceTextModuleInfoEntry, beg_pos, kBegPosOffset)
SMI_ACCESSORS(SourceTextModuleInfoEntry, end_pos, kEndPosOffset)

OBJECT_CONSTRUCTORS_IMPL(SourceTextModuleInfo, FixedArray)
CAST_ACCESSOR(SourceTextModuleInfo)

FixedArray SourceTextModuleInfo::module_requests() const {
  return FixedArray::cast(get(kModuleRequestsIndex));
}

FixedArray SourceTextModuleInfo::special_exports() const {
  return FixedArray::cast(get(kSpecialExportsIndex));
}

FixedArray SourceTextModuleInfo::regular_exports() const {
  return FixedArray::cast(get(kRegularExportsIndex));
}

FixedArray SourceTextModuleInfo::regular_imports() const {
  return FixedArray::cast(get(kRegularImportsIndex));
}

FixedArray SourceTextModuleInfo::namespace_imports() const {
  return FixedArray::cast(get(kNamespaceImportsIndex));
}

FixedArray SourceTextModuleInfo::module_request_positions() const {
  return FixedArray::cast(get(kModuleRequestPositionsIndex));
}

#ifdef DEBUG
bool SourceTextModuleInfo::Equals(SourceTextModuleInfo other) const {
  return regular_exports() == other.regular_exports() &&
         regular_imports() == other.regular_imports() &&
         special_exports() == other.special_exports() &&
         namespace_imports() == other.namespace_imports() &&
         module_requests() == other.module_requests() &&
         module_request_positions() == other.module_request_positions();
}
#endif

struct ModuleHandleHash {
  V8_INLINE size_t operator()(Handle<Module> module) const {
    return module->hash();
  }
};

struct ModuleHandleEqual {
  V8_INLINE bool operator()(Handle<Module> lhs, Handle<Module> rhs) const {
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MODULE_INL_H_
