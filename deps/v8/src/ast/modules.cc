// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/modules.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/scopes.h"

namespace v8 {
namespace internal {

void ModuleDescriptor::AddImport(
    const AstRawString* import_name, const AstRawString* local_name,
    const AstRawString* module_request, Scanner::Location loc, Zone* zone) {
  DCHECK_NOT_NULL(import_name);
  DCHECK_NOT_NULL(local_name);
  DCHECK_NOT_NULL(module_request);
  ModuleEntry* entry = new (zone) ModuleEntry(loc);
  entry->local_name = local_name;
  entry->import_name = import_name;
  entry->module_request = module_request;
  regular_imports_.insert(std::make_pair(entry->local_name, entry));
  // We don't care if there's already an entry for this local name, as in that
  // case we will report an error when declaring the variable.
}


void ModuleDescriptor::AddStarImport(
    const AstRawString* local_name, const AstRawString* module_request,
    Scanner::Location loc, Zone* zone) {
  DCHECK_NOT_NULL(local_name);
  DCHECK_NOT_NULL(module_request);
  ModuleEntry* entry = new (zone) ModuleEntry(loc);
  entry->local_name = local_name;
  entry->module_request = module_request;
  special_imports_.Add(entry, zone);
}


void ModuleDescriptor::AddEmptyImport(
    const AstRawString* module_request, Scanner::Location loc, Zone* zone) {
  DCHECK_NOT_NULL(module_request);
  ModuleEntry* entry = new (zone) ModuleEntry(loc);
  entry->module_request = module_request;
  special_imports_.Add(entry, zone);
}


void ModuleDescriptor::AddExport(
    const AstRawString* local_name, const AstRawString* export_name,
    Scanner::Location loc, Zone* zone) {
  DCHECK_NOT_NULL(local_name);
  DCHECK_NOT_NULL(export_name);
  ModuleEntry* entry = new (zone) ModuleEntry(loc);
  entry->export_name = export_name;
  entry->local_name = local_name;
  exports_.Add(entry, zone);
}


void ModuleDescriptor::AddExport(
    const AstRawString* import_name, const AstRawString* export_name,
    const AstRawString* module_request, Scanner::Location loc, Zone* zone) {
  DCHECK_NOT_NULL(import_name);
  DCHECK_NOT_NULL(export_name);
  DCHECK_NOT_NULL(module_request);
  ModuleEntry* entry = new (zone) ModuleEntry(loc);
  entry->export_name = export_name;
  entry->import_name = import_name;
  entry->module_request = module_request;
  exports_.Add(entry, zone);
}


void ModuleDescriptor::AddStarExport(
    const AstRawString* module_request, Scanner::Location loc, Zone* zone) {
  DCHECK_NOT_NULL(module_request);
  ModuleEntry* entry = new (zone) ModuleEntry(loc);
  entry->module_request = module_request;
  exports_.Add(entry, zone);
}

void ModuleDescriptor::MakeIndirectExportsExplicit() {
  for (auto entry : exports_) {
    if (entry->export_name == nullptr) continue;
    if (entry->import_name != nullptr) continue;
    DCHECK_NOT_NULL(entry->local_name);
    auto it = regular_imports_.find(entry->local_name);
    if (it != regular_imports_.end()) {
      // Found an indirect export.
      DCHECK_NOT_NULL(it->second->module_request);
      DCHECK_NOT_NULL(it->second->import_name);
      entry->import_name = it->second->import_name;
      entry->module_request = it->second->module_request;
      entry->local_name = nullptr;
    }
  }
}

bool ModuleDescriptor::Validate(ModuleScope* module_scope,
                                PendingCompilationErrorHandler* error_handler,
                                Zone* zone) {
  DCHECK_EQ(this, module_scope->module());
  DCHECK_NOT_NULL(error_handler);

  // Report error iff there are duplicate exports.
  {
    ZoneAllocationPolicy allocator(zone);
    ZoneHashMap* export_names = new (zone->New(sizeof(ZoneHashMap)))
        ZoneHashMap(ZoneHashMap::PointersMatch,
                    ZoneHashMap::kDefaultHashMapCapacity, allocator);
    for (auto entry : exports_) {
      if (entry->export_name == nullptr) continue;
      AstRawString* key = const_cast<AstRawString*>(entry->export_name);
      ZoneHashMap::Entry* p =
          export_names->LookupOrInsert(key, key->hash(), allocator);
      DCHECK_NOT_NULL(p);
      if (p->value != nullptr) {
        error_handler->ReportMessageAt(
            entry->location.beg_pos, entry->location.end_pos,
            MessageTemplate::kDuplicateExport, entry->export_name);
        return false;
      }
      p->value = key;  // Anything but nullptr.
    }
  }

  // Report error iff there are exports of non-existent local names.
  for (auto entry : exports_) {
    if (entry->local_name == nullptr) continue;
    if (module_scope->LookupLocal(entry->local_name) == nullptr) {
      error_handler->ReportMessageAt(
          entry->location.beg_pos, entry->location.end_pos,
          MessageTemplate::kModuleExportUndefined, entry->local_name);
      return false;
    }
  }

  MakeIndirectExportsExplicit();
  return true;
}

}  // namespace internal
}  // namespace v8
