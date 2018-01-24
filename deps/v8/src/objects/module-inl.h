// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MODULE_INL_H_
#define V8_OBJECTS_MODULE_INL_H_

#include "src/objects/module.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

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

ModuleInfo* Module::info() const {
  if (status() >= kEvaluating) {
    return ModuleInfo::cast(code());
  }
  ScopeInfo* scope_info =
      status() == kInstantiated
          ? JSGeneratorObject::cast(code())->function()->shared()->scope_info()
          : status() == kInstantiating
                ? JSFunction::cast(code())->shared()->scope_info()
                : SharedFunctionInfo::cast(code())->scope_info();
  return scope_info->ModuleDescriptorInfo();
}

TYPE_CHECKER(JSModuleNamespace, JS_MODULE_NAMESPACE_TYPE)
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

CAST_ACCESSOR(ModuleInfo)

bool HeapObject::IsModuleInfo() const {
  return map() == GetHeap()->module_info_map();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MODULE_INL_H_
