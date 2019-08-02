// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MODULE_INL_H_
#define V8_OBJECTS_MODULE_INL_H_

#include "src/objects/module.h"

#include "src/objects/objects-inl.h"  // Needed for write barriers
#include "src/objects/scope-info.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(Module, Struct)
OBJECT_CONSTRUCTORS_IMPL(ModuleInfoEntry, Struct)
OBJECT_CONSTRUCTORS_IMPL(JSModuleNamespace, JSObject)

NEVER_READ_ONLY_SPACE_IMPL(Module)

CAST_ACCESSOR(Module)
ACCESSORS(Module, code, Object, kCodeOffset)
ACCESSORS(Module, exports, ObjectHashTable, kExportsOffset)
ACCESSORS(Module, regular_exports, FixedArray, kRegularExportsOffset)
ACCESSORS(Module, regular_imports, FixedArray, kRegularImportsOffset)
ACCESSORS(Module, module_namespace, HeapObject, kModuleNamespaceOffset)
ACCESSORS(Module, requested_modules, FixedArray, kRequestedModulesOffset)
ACCESSORS(Module, script, Script, kScriptOffset)
ACCESSORS(Module, exception, Object, kExceptionOffset)
ACCESSORS(Module, import_meta, Object, kImportMetaOffset)
SMI_ACCESSORS(Module, status, kStatusOffset)
SMI_ACCESSORS(Module, dfs_index, kDfsIndexOffset)
SMI_ACCESSORS(Module, dfs_ancestor_index, kDfsAncestorIndexOffset)
SMI_ACCESSORS(Module, hash, kHashOffset)

ModuleInfo Module::info() const {
  return (status() >= kEvaluating)
             ? ModuleInfo::cast(code())
             : GetSharedFunctionInfo().scope_info().ModuleDescriptorInfo();
}

CAST_ACCESSOR(JSModuleNamespace)
ACCESSORS(JSModuleNamespace, module, Module, kModuleOffset)

CAST_ACCESSOR(ModuleInfoEntry)
ACCESSORS(ModuleInfoEntry, export_name, Object, kExportNameOffset)
ACCESSORS(ModuleInfoEntry, local_name, Object, kLocalNameOffset)
ACCESSORS(ModuleInfoEntry, import_name, Object, kImportNameOffset)
SMI_ACCESSORS(ModuleInfoEntry, module_request, kModuleRequestOffset)
SMI_ACCESSORS(ModuleInfoEntry, cell_index, kCellIndexOffset)
SMI_ACCESSORS(ModuleInfoEntry, beg_pos, kBegPosOffset)
SMI_ACCESSORS(ModuleInfoEntry, end_pos, kEndPosOffset)

OBJECT_CONSTRUCTORS_IMPL(ModuleInfo, FixedArray)
CAST_ACCESSOR(ModuleInfo)

FixedArray ModuleInfo::module_requests() const {
  return FixedArray::cast(get(kModuleRequestsIndex));
}

FixedArray ModuleInfo::special_exports() const {
  return FixedArray::cast(get(kSpecialExportsIndex));
}

FixedArray ModuleInfo::regular_exports() const {
  return FixedArray::cast(get(kRegularExportsIndex));
}

FixedArray ModuleInfo::regular_imports() const {
  return FixedArray::cast(get(kRegularImportsIndex));
}

FixedArray ModuleInfo::namespace_imports() const {
  return FixedArray::cast(get(kNamespaceImportsIndex));
}

FixedArray ModuleInfo::module_request_positions() const {
  return FixedArray::cast(get(kModuleRequestPositionsIndex));
}

#ifdef DEBUG
bool ModuleInfo::Equals(ModuleInfo other) const {
  return regular_exports() == other.regular_exports() &&
         regular_imports() == other.regular_imports() &&
         special_exports() == other.special_exports() &&
         namespace_imports() == other.namespace_imports() &&
         module_requests() == other.module_requests() &&
         module_request_positions() == other.module_request_positions();
}
#endif

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MODULE_INL_H_
