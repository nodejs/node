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
  regular_exports_.insert(std::make_pair(entry->local_name, entry));
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
  special_exports_.Add(entry, zone);
}


void ModuleDescriptor::AddStarExport(
    const AstRawString* module_request, Scanner::Location loc, Zone* zone) {
  DCHECK_NOT_NULL(module_request);
  ModuleEntry* entry = new (zone) ModuleEntry(loc);
  entry->module_request = module_request;
  special_exports_.Add(entry, zone);
}

void ModuleDescriptor::MakeIndirectExportsExplicit(Zone* zone) {
  for (auto it = regular_exports_.begin(); it != regular_exports_.end();) {
    ModuleEntry* entry = it->second;
    DCHECK_NOT_NULL(entry->local_name);
    auto import = regular_imports_.find(entry->local_name);
    if (import != regular_imports_.end()) {
      // Found an indirect export.  Patch export entry and move it from regular
      // to special.
      DCHECK_NULL(entry->import_name);
      DCHECK_NULL(entry->module_request);
      DCHECK_NOT_NULL(import->second->import_name);
      DCHECK_NOT_NULL(import->second->module_request);
      entry->import_name = import->second->import_name;
      entry->module_request = import->second->module_request;
      entry->local_name = nullptr;
      special_exports_.Add(entry, zone);
      it = regular_exports_.erase(it);
    } else {
      it++;
    }
  }
}

const ModuleDescriptor::ModuleEntry* ModuleDescriptor::FindDuplicateExport(
    Zone* zone) const {
  ZoneSet<const AstRawString*> export_names(zone);
  for (const auto& it : regular_exports_) {
    const ModuleEntry* entry = it.second;
    DCHECK_NOT_NULL(entry->export_name);
    if (!export_names.insert(entry->export_name).second) return entry;
  }
  for (auto entry : special_exports_) {
    if (entry->export_name == nullptr) continue;  // Star export.
    if (!export_names.insert(entry->export_name).second) return entry;
  }
  return nullptr;
}

bool ModuleDescriptor::Validate(ModuleScope* module_scope,
                                PendingCompilationErrorHandler* error_handler,
                                Zone* zone) {
  DCHECK_EQ(this, module_scope->module());
  DCHECK_NOT_NULL(error_handler);

  // Report error iff there are duplicate exports.
  {
    const ModuleEntry* entry = FindDuplicateExport(zone);
    if (entry != nullptr) {
      error_handler->ReportMessageAt(
          entry->location.beg_pos, entry->location.end_pos,
          MessageTemplate::kDuplicateExport, entry->export_name);
      return false;
    }
  }

  // Report error iff there are exports of non-existent local names.
  for (const auto& it : regular_exports_) {
    const ModuleEntry* entry = it.second;
    DCHECK_NOT_NULL(entry->local_name);
    if (module_scope->LookupLocal(entry->local_name) == nullptr) {
      error_handler->ReportMessageAt(
          entry->location.beg_pos, entry->location.end_pos,
          MessageTemplate::kModuleExportUndefined, entry->local_name);
      return false;
    }
  }

  MakeIndirectExportsExplicit(zone);
  return true;
}

}  // namespace internal
}  // namespace v8
