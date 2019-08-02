// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/modules.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/scopes.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/parsing/pending-compilation-error-handler.h"

namespace v8 {
namespace internal {

bool ModuleDescriptor::AstRawStringComparer::operator()(
    const AstRawString* lhs, const AstRawString* rhs) const {
  // Fast path for equal pointers: a pointer is not strictly less than itself.
  if (lhs == rhs) return false;

  // Order by contents (ordering by hash is unstable across runs).
  if (lhs->is_one_byte() != rhs->is_one_byte()) {
    return lhs->is_one_byte();
  }
  if (lhs->byte_length() != rhs->byte_length()) {
    return lhs->byte_length() < rhs->byte_length();
  }
  return memcmp(lhs->raw_data(), rhs->raw_data(), lhs->byte_length()) < 0;
}

void ModuleDescriptor::AddImport(const AstRawString* import_name,
                                 const AstRawString* local_name,
                                 const AstRawString* module_request,
                                 const Scanner::Location loc,
                                 const Scanner::Location specifier_loc,
                                 Zone* zone) {
  Entry* entry = new (zone) Entry(loc);
  entry->local_name = local_name;
  entry->import_name = import_name;
  entry->module_request = AddModuleRequest(module_request, specifier_loc);
  AddRegularImport(entry);
}

void ModuleDescriptor::AddStarImport(const AstRawString* local_name,
                                     const AstRawString* module_request,
                                     const Scanner::Location loc,
                                     const Scanner::Location specifier_loc,
                                     Zone* zone) {
  Entry* entry = new (zone) Entry(loc);
  entry->local_name = local_name;
  entry->module_request = AddModuleRequest(module_request, specifier_loc);
  AddNamespaceImport(entry, zone);
}

void ModuleDescriptor::AddEmptyImport(const AstRawString* module_request,
                                      const Scanner::Location specifier_loc) {
  AddModuleRequest(module_request, specifier_loc);
}


void ModuleDescriptor::AddExport(
    const AstRawString* local_name, const AstRawString* export_name,
    Scanner::Location loc, Zone* zone) {
  Entry* entry = new (zone) Entry(loc);
  entry->export_name = export_name;
  entry->local_name = local_name;
  AddRegularExport(entry);
}

void ModuleDescriptor::AddExport(const AstRawString* import_name,
                                 const AstRawString* export_name,
                                 const AstRawString* module_request,
                                 const Scanner::Location loc,
                                 const Scanner::Location specifier_loc,
                                 Zone* zone) {
  DCHECK_NOT_NULL(import_name);
  DCHECK_NOT_NULL(export_name);
  Entry* entry = new (zone) Entry(loc);
  entry->export_name = export_name;
  entry->import_name = import_name;
  entry->module_request = AddModuleRequest(module_request, specifier_loc);
  AddSpecialExport(entry, zone);
}

void ModuleDescriptor::AddStarExport(const AstRawString* module_request,
                                     const Scanner::Location loc,
                                     const Scanner::Location specifier_loc,
                                     Zone* zone) {
  Entry* entry = new (zone) Entry(loc);
  entry->module_request = AddModuleRequest(module_request, specifier_loc);
  AddSpecialExport(entry, zone);
}

namespace {
Handle<Object> ToStringOrUndefined(Isolate* isolate, const AstRawString* s) {
  return (s == nullptr)
             ? Handle<Object>::cast(isolate->factory()->undefined_value())
             : Handle<Object>::cast(s->string());
}
}  // namespace

Handle<ModuleInfoEntry> ModuleDescriptor::Entry::Serialize(
    Isolate* isolate) const {
  CHECK(Smi::IsValid(module_request));  // TODO(neis): Check earlier?
  return ModuleInfoEntry::New(
      isolate, ToStringOrUndefined(isolate, export_name),
      ToStringOrUndefined(isolate, local_name),
      ToStringOrUndefined(isolate, import_name), module_request, cell_index,
      location.beg_pos, location.end_pos);
}

Handle<FixedArray> ModuleDescriptor::SerializeRegularExports(Isolate* isolate,
                                                             Zone* zone) const {
  // We serialize regular exports in a way that lets us later iterate over their
  // local names and for each local name immediately access all its export
  // names.  (Regular exports have neither import name nor module request.)

  ZoneVector<Handle<Object>> data(
      ModuleInfo::kRegularExportLength * regular_exports_.size(), zone);
  int index = 0;

  for (auto it = regular_exports_.begin(); it != regular_exports_.end();) {
    // Find out how many export names this local name has.
    auto next = it;
    int count = 0;
    do {
      DCHECK_EQ(it->second->local_name, next->second->local_name);
      DCHECK_EQ(it->second->cell_index, next->second->cell_index);
      ++next;
      ++count;
    } while (next != regular_exports_.end() && next->first == it->first);

    Handle<FixedArray> export_names = isolate->factory()->NewFixedArray(count);
    data[index + ModuleInfo::kRegularExportLocalNameOffset] =
        it->second->local_name->string();
    data[index + ModuleInfo::kRegularExportCellIndexOffset] =
        handle(Smi::FromInt(it->second->cell_index), isolate);
    data[index + ModuleInfo::kRegularExportExportNamesOffset] = export_names;
    index += ModuleInfo::kRegularExportLength;

    // Collect the export names.
    int i = 0;
    for (; it != next; ++it) {
      export_names->set(i++, *it->second->export_name->string());
    }
    DCHECK_EQ(i, count);

    // Continue with the next distinct key.
    DCHECK(it == next);
  }
  DCHECK_LE(index, static_cast<int>(data.size()));
  data.resize(index);

  // We cannot create the FixedArray earlier because we only now know the
  // precise size.
  Handle<FixedArray> result = isolate->factory()->NewFixedArray(index);
  for (int i = 0; i < index; ++i) {
    result->set(i, *data[i]);
  }
  return result;
}

void ModuleDescriptor::MakeIndirectExportsExplicit(Zone* zone) {
  for (auto it = regular_exports_.begin(); it != regular_exports_.end();) {
    Entry* entry = it->second;
    DCHECK_NOT_NULL(entry->local_name);
    auto import = regular_imports_.find(entry->local_name);
    if (import != regular_imports_.end()) {
      // Found an indirect export.  Patch export entry and move it from regular
      // to special.
      DCHECK_NULL(entry->import_name);
      DCHECK_LT(entry->module_request, 0);
      DCHECK_NOT_NULL(import->second->import_name);
      DCHECK_LE(0, import->second->module_request);
      DCHECK_LT(import->second->module_request,
                static_cast<int>(module_requests_.size()));
      entry->import_name = import->second->import_name;
      entry->module_request = import->second->module_request;
      // Hack: When the indirect export cannot be resolved, we want the error
      // message to point at the import statement, not at the export statement.
      // Therefore we overwrite [entry]'s location here.  Note that Validate()
      // has already checked for duplicate exports, so it's guaranteed that we
      // won't need to report any error pointing at the (now lost) export
      // location.
      entry->location = import->second->location;
      entry->local_name = nullptr;
      AddSpecialExport(entry, zone);
      it = regular_exports_.erase(it);
    } else {
      it++;
    }
  }
}

ModuleDescriptor::CellIndexKind ModuleDescriptor::GetCellIndexKind(
    int cell_index) {
  if (cell_index > 0) return kExport;
  if (cell_index < 0) return kImport;
  return kInvalid;
}

void ModuleDescriptor::AssignCellIndices() {
  int export_index = 1;
  for (auto it = regular_exports_.begin(); it != regular_exports_.end();) {
    auto current_key = it->first;
    // This local name may be exported under multiple export names.  Assign the
    // same index to each such entry.
    do {
      Entry* entry = it->second;
      DCHECK_NOT_NULL(entry->local_name);
      DCHECK_NULL(entry->import_name);
      DCHECK_LT(entry->module_request, 0);
      DCHECK_EQ(entry->cell_index, 0);
      entry->cell_index = export_index;
      it++;
    } while (it != regular_exports_.end() && it->first == current_key);
    export_index++;
  }

  int import_index = -1;
  for (const auto& elem : regular_imports_) {
    Entry* entry = elem.second;
    DCHECK_NOT_NULL(entry->local_name);
    DCHECK_NOT_NULL(entry->import_name);
    DCHECK_LE(0, entry->module_request);
    DCHECK_EQ(entry->cell_index, 0);
    entry->cell_index = import_index;
    import_index--;
  }
}

namespace {

const ModuleDescriptor::Entry* BetterDuplicate(
    const ModuleDescriptor::Entry* candidate,
    ZoneMap<const AstRawString*, const ModuleDescriptor::Entry*>& export_names,
    const ModuleDescriptor::Entry* current_duplicate) {
  DCHECK_NOT_NULL(candidate->export_name);
  DCHECK(candidate->location.IsValid());
  auto insert_result =
      export_names.insert(std::make_pair(candidate->export_name, candidate));
  if (insert_result.second) return current_duplicate;
  if (current_duplicate == nullptr) {
    current_duplicate = insert_result.first->second;
  }
  return (candidate->location.beg_pos > current_duplicate->location.beg_pos)
             ? candidate
             : current_duplicate;
}

}  // namespace

const ModuleDescriptor::Entry* ModuleDescriptor::FindDuplicateExport(
    Zone* zone) const {
  const ModuleDescriptor::Entry* duplicate = nullptr;
  ZoneMap<const AstRawString*, const ModuleDescriptor::Entry*> export_names(
      zone);
  for (const auto& elem : regular_exports_) {
    duplicate = BetterDuplicate(elem.second, export_names, duplicate);
  }
  for (auto entry : special_exports_) {
    if (entry->export_name == nullptr) continue;  // Star export.
    duplicate = BetterDuplicate(entry, export_names, duplicate);
  }
  return duplicate;
}

bool ModuleDescriptor::Validate(ModuleScope* module_scope,
                                PendingCompilationErrorHandler* error_handler,
                                Zone* zone) {
  DCHECK_EQ(this, module_scope->module());
  DCHECK_NOT_NULL(error_handler);

  // Report error iff there are duplicate exports.
  {
    const Entry* entry = FindDuplicateExport(zone);
    if (entry != nullptr) {
      error_handler->ReportMessageAt(
          entry->location.beg_pos, entry->location.end_pos,
          MessageTemplate::kDuplicateExport, entry->export_name);
      return false;
    }
  }

  // Report error iff there are exports of non-existent local names.
  for (const auto& elem : regular_exports_) {
    const Entry* entry = elem.second;
    DCHECK_NOT_NULL(entry->local_name);
    if (module_scope->LookupLocal(entry->local_name) == nullptr) {
      error_handler->ReportMessageAt(
          entry->location.beg_pos, entry->location.end_pos,
          MessageTemplate::kModuleExportUndefined, entry->local_name);
      return false;
    }
  }

  MakeIndirectExportsExplicit(zone);
  AssignCellIndices();
  return true;
}

}  // namespace internal
}  // namespace v8
