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
  Entry* entry = new (zone) Entry(loc);
  entry->local_name = local_name;
  entry->import_name = import_name;
  entry->module_request = AddModuleRequest(module_request);
  AddRegularImport(entry);
}


void ModuleDescriptor::AddStarImport(
    const AstRawString* local_name, const AstRawString* module_request,
    Scanner::Location loc, Zone* zone) {
  Entry* entry = new (zone) Entry(loc);
  entry->local_name = local_name;
  entry->module_request = AddModuleRequest(module_request);
  AddNamespaceImport(entry, zone);
}

void ModuleDescriptor::AddEmptyImport(const AstRawString* module_request) {
  AddModuleRequest(module_request);
}


void ModuleDescriptor::AddExport(
    const AstRawString* local_name, const AstRawString* export_name,
    Scanner::Location loc, Zone* zone) {
  Entry* entry = new (zone) Entry(loc);
  entry->export_name = export_name;
  entry->local_name = local_name;
  AddRegularExport(entry);
}


void ModuleDescriptor::AddExport(
    const AstRawString* import_name, const AstRawString* export_name,
    const AstRawString* module_request, Scanner::Location loc, Zone* zone) {
  DCHECK_NOT_NULL(import_name);
  DCHECK_NOT_NULL(export_name);
  Entry* entry = new (zone) Entry(loc);
  entry->export_name = export_name;
  entry->import_name = import_name;
  entry->module_request = AddModuleRequest(module_request);
  AddSpecialExport(entry, zone);
}


void ModuleDescriptor::AddStarExport(
    const AstRawString* module_request, Scanner::Location loc, Zone* zone) {
  Entry* entry = new (zone) Entry(loc);
  entry->module_request = AddModuleRequest(module_request);
  AddSpecialExport(entry, zone);
}

namespace {

Handle<Object> ToStringOrUndefined(Isolate* isolate, const AstRawString* s) {
  return (s == nullptr)
             ? Handle<Object>::cast(isolate->factory()->undefined_value())
             : Handle<Object>::cast(s->string());
}

const AstRawString* FromStringOrUndefined(Isolate* isolate,
                                          AstValueFactory* avfactory,
                                          Handle<Object> object) {
  if (object->IsUndefined(isolate)) return nullptr;
  return avfactory->GetString(Handle<String>::cast(object));
}

}  // namespace

Handle<ModuleInfoEntry> ModuleDescriptor::Entry::Serialize(
    Isolate* isolate) const {
  CHECK(Smi::IsValid(module_request));  // TODO(neis): Check earlier?
  return ModuleInfoEntry::New(
      isolate, ToStringOrUndefined(isolate, export_name),
      ToStringOrUndefined(isolate, local_name),
      ToStringOrUndefined(isolate, import_name),
      Handle<Object>(Smi::FromInt(module_request), isolate));
}

ModuleDescriptor::Entry* ModuleDescriptor::Entry::Deserialize(
    Isolate* isolate, AstValueFactory* avfactory,
    Handle<ModuleInfoEntry> entry) {
  Entry* result = new (avfactory->zone()) Entry(Scanner::Location::invalid());
  result->export_name = FromStringOrUndefined(
      isolate, avfactory, handle(entry->export_name(), isolate));
  result->local_name = FromStringOrUndefined(
      isolate, avfactory, handle(entry->local_name(), isolate));
  result->import_name = FromStringOrUndefined(
      isolate, avfactory, handle(entry->import_name(), isolate));
  result->module_request = Smi::cast(entry->module_request())->value();
  return result;
}

Handle<FixedArray> ModuleDescriptor::SerializeRegularExports(Isolate* isolate,
                                                             Zone* zone) const {
  // We serialize regular exports in a way that lets us later iterate over their
  // local names and for each local name immediately access all its export
  // names.  (Regular exports have neither import name nor module request.)

  ZoneVector<Handle<Object>> data(zone);
  data.reserve(2 * regular_exports_.size());

  for (auto it = regular_exports_.begin(); it != regular_exports_.end();) {
    // Find out how many export names this local name has.
    auto next = it;
    int size = 0;
    do {
      ++next;
      ++size;
    } while (next != regular_exports_.end() && next->first == it->first);

    Handle<FixedArray> export_names = isolate->factory()->NewFixedArray(size);
    data.push_back(it->second->local_name->string());
    data.push_back(export_names);

    // Collect the export names.
    int i = 0;
    for (; it != next; ++it) {
      export_names->set(i++, *it->second->export_name->string());
    }
    DCHECK_EQ(i, size);

    // Continue with the next distinct key.
    DCHECK(it == next);
  }

  // We cannot create the FixedArray earlier because we only now know the
  // precise size (the number of unique keys in regular_exports).
  int size = static_cast<int>(data.size());
  Handle<FixedArray> result = isolate->factory()->NewFixedArray(size);
  for (int i = 0; i < size; ++i) {
    result->set(i, *data[i]);
  }
  return result;
}

void ModuleDescriptor::DeserializeRegularExports(Isolate* isolate,
                                                 AstValueFactory* avfactory,
                                                 Handle<FixedArray> data) {
  for (int i = 0, length_i = data->length(); i < length_i;) {
    Handle<String> local_name(String::cast(data->get(i++)), isolate);
    Handle<FixedArray> export_names(FixedArray::cast(data->get(i++)), isolate);

    for (int j = 0, length_j = export_names->length(); j < length_j; ++j) {
      Handle<String> export_name(String::cast(export_names->get(j)), isolate);

      Entry* entry =
          new (avfactory->zone()) Entry(Scanner::Location::invalid());
      entry->local_name = avfactory->GetString(local_name);
      entry->export_name = avfactory->GetString(export_name);

      AddRegularExport(entry);
    }
  }
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
      entry->local_name = nullptr;
      AddSpecialExport(entry, zone);
      it = regular_exports_.erase(it);
    } else {
      it++;
    }
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
  return true;
}

}  // namespace internal
}  // namespace v8
