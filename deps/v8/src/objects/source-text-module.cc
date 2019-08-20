// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/source-text-module.h"

#include "src/api/api-inl.h"
#include "src/ast/modules.h"
#include "src/builtins/accessors.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

struct StringHandleHash {
  V8_INLINE size_t operator()(Handle<String> string) const {
    return string->Hash();
  }
};

struct StringHandleEqual {
  V8_INLINE bool operator()(Handle<String> lhs, Handle<String> rhs) const {
    return lhs->Equals(*rhs);
  }
};

class UnorderedStringSet
    : public std::unordered_set<Handle<String>, StringHandleHash,
                                StringHandleEqual,
                                ZoneAllocator<Handle<String>>> {
 public:
  explicit UnorderedStringSet(Zone* zone)
      : std::unordered_set<Handle<String>, StringHandleHash, StringHandleEqual,
                           ZoneAllocator<Handle<String>>>(
            2 /* bucket count */, StringHandleHash(), StringHandleEqual(),
            ZoneAllocator<Handle<String>>(zone)) {}
};

class UnorderedStringMap
    : public std::unordered_map<
          Handle<String>, Handle<Object>, StringHandleHash, StringHandleEqual,
          ZoneAllocator<std::pair<const Handle<String>, Handle<Object>>>> {
 public:
  explicit UnorderedStringMap(Zone* zone)
      : std::unordered_map<
            Handle<String>, Handle<Object>, StringHandleHash, StringHandleEqual,
            ZoneAllocator<std::pair<const Handle<String>, Handle<Object>>>>(
            2 /* bucket count */, StringHandleHash(), StringHandleEqual(),
            ZoneAllocator<std::pair<const Handle<String>, Handle<Object>>>(
                zone)) {}
};

class Module::ResolveSet
    : public std::unordered_map<
          Handle<Module>, UnorderedStringSet*, ModuleHandleHash,
          ModuleHandleEqual,
          ZoneAllocator<std::pair<const Handle<Module>, UnorderedStringSet*>>> {
 public:
  explicit ResolveSet(Zone* zone)
      : std::unordered_map<Handle<Module>, UnorderedStringSet*,
                           ModuleHandleHash, ModuleHandleEqual,
                           ZoneAllocator<std::pair<const Handle<Module>,
                                                   UnorderedStringSet*>>>(
            2 /* bucket count */, ModuleHandleHash(), ModuleHandleEqual(),
            ZoneAllocator<std::pair<const Handle<Module>, UnorderedStringSet*>>(
                zone)),
        zone_(zone) {}

  Zone* zone() const { return zone_; }

 private:
  Zone* zone_;
};

SharedFunctionInfo SourceTextModule::GetSharedFunctionInfo() const {
  DisallowHeapAllocation no_alloc;
  DCHECK_NE(status(), Module::kEvaluating);
  DCHECK_NE(status(), Module::kEvaluated);
  switch (status()) {
    case kUninstantiated:
    case kPreInstantiating:
      DCHECK(code().IsSharedFunctionInfo());
      return SharedFunctionInfo::cast(code());
    case kInstantiating:
      DCHECK(code().IsJSFunction());
      return JSFunction::cast(code()).shared();
    case kInstantiated:
      DCHECK(code().IsJSGeneratorObject());
      return JSGeneratorObject::cast(code()).function().shared();
    case kEvaluating:
    case kEvaluated:
    case kErrored:
      UNREACHABLE();
  }

  UNREACHABLE();
}

int SourceTextModule::ExportIndex(int cell_index) {
  DCHECK_EQ(SourceTextModuleDescriptor::GetCellIndexKind(cell_index),
            SourceTextModuleDescriptor::kExport);
  return cell_index - 1;
}

int SourceTextModule::ImportIndex(int cell_index) {
  DCHECK_EQ(SourceTextModuleDescriptor::GetCellIndexKind(cell_index),
            SourceTextModuleDescriptor::kImport);
  return -cell_index - 1;
}

void SourceTextModule::CreateIndirectExport(
    Isolate* isolate, Handle<SourceTextModule> module, Handle<String> name,
    Handle<SourceTextModuleInfoEntry> entry) {
  Handle<ObjectHashTable> exports(module->exports(), isolate);
  DCHECK(exports->Lookup(name).IsTheHole(isolate));
  exports = ObjectHashTable::Put(exports, name, entry);
  module->set_exports(*exports);
}

void SourceTextModule::CreateExport(Isolate* isolate,
                                    Handle<SourceTextModule> module,
                                    int cell_index, Handle<FixedArray> names) {
  DCHECK_LT(0, names->length());
  Handle<Cell> cell =
      isolate->factory()->NewCell(isolate->factory()->undefined_value());
  module->regular_exports().set(ExportIndex(cell_index), *cell);

  Handle<ObjectHashTable> exports(module->exports(), isolate);
  for (int i = 0, n = names->length(); i < n; ++i) {
    Handle<String> name(String::cast(names->get(i)), isolate);
    DCHECK(exports->Lookup(name).IsTheHole(isolate));
    exports = ObjectHashTable::Put(exports, name, cell);
  }
  module->set_exports(*exports);
}

Cell SourceTextModule::GetCell(int cell_index) {
  DisallowHeapAllocation no_gc;
  Object cell;
  switch (SourceTextModuleDescriptor::GetCellIndexKind(cell_index)) {
    case SourceTextModuleDescriptor::kImport:
      cell = regular_imports().get(ImportIndex(cell_index));
      break;
    case SourceTextModuleDescriptor::kExport:
      cell = regular_exports().get(ExportIndex(cell_index));
      break;
    case SourceTextModuleDescriptor::kInvalid:
      UNREACHABLE();
      break;
  }
  return Cell::cast(cell);
}

Handle<Object> SourceTextModule::LoadVariable(Isolate* isolate,
                                              Handle<SourceTextModule> module,
                                              int cell_index) {
  return handle(module->GetCell(cell_index).value(), isolate);
}

void SourceTextModule::StoreVariable(Handle<SourceTextModule> module,
                                     int cell_index, Handle<Object> value) {
  DisallowHeapAllocation no_gc;
  DCHECK_EQ(SourceTextModuleDescriptor::GetCellIndexKind(cell_index),
            SourceTextModuleDescriptor::kExport);
  module->GetCell(cell_index).set_value(*value);
}

MaybeHandle<Cell> SourceTextModule::ResolveExport(
    Isolate* isolate, Handle<SourceTextModule> module,
    Handle<String> module_specifier, Handle<String> export_name,
    MessageLocation loc, bool must_resolve, Module::ResolveSet* resolve_set) {
  Handle<Object> object(module->exports().Lookup(export_name), isolate);
  if (object->IsCell()) {
    // Already resolved (e.g. because it's a local export).
    return Handle<Cell>::cast(object);
  }

  // Check for cycle before recursing.
  {
    // Attempt insertion with a null string set.
    auto result = resolve_set->insert({module, nullptr});
    UnorderedStringSet*& name_set = result.first->second;
    if (result.second) {
      // |module| wasn't in the map previously, so allocate a new name set.
      Zone* zone = resolve_set->zone();
      name_set =
          new (zone->New(sizeof(UnorderedStringSet))) UnorderedStringSet(zone);
    } else if (name_set->count(export_name)) {
      // Cycle detected.
      if (must_resolve) {
        return isolate->Throw<Cell>(
            isolate->factory()->NewSyntaxError(
                MessageTemplate::kCyclicModuleDependency, export_name,
                module_specifier),
            &loc);
      }
      return MaybeHandle<Cell>();
    }
    name_set->insert(export_name);
  }

  if (object->IsSourceTextModuleInfoEntry()) {
    // Not yet resolved indirect export.
    Handle<SourceTextModuleInfoEntry> entry =
        Handle<SourceTextModuleInfoEntry>::cast(object);
    Handle<String> import_name(String::cast(entry->import_name()), isolate);
    Handle<Script> script(module->script(), isolate);
    MessageLocation new_loc(script, entry->beg_pos(), entry->end_pos());

    Handle<Cell> cell;
    if (!ResolveImport(isolate, module, import_name, entry->module_request(),
                       new_loc, true, resolve_set)
             .ToHandle(&cell)) {
      DCHECK(isolate->has_pending_exception());
      return MaybeHandle<Cell>();
    }

    // The export table may have changed but the entry in question should be
    // unchanged.
    Handle<ObjectHashTable> exports(module->exports(), isolate);
    DCHECK(exports->Lookup(export_name).IsSourceTextModuleInfoEntry());

    exports = ObjectHashTable::Put(exports, export_name, cell);
    module->set_exports(*exports);
    return cell;
  }

  DCHECK(object->IsTheHole(isolate));
  return SourceTextModule::ResolveExportUsingStarExports(
      isolate, module, module_specifier, export_name, loc, must_resolve,
      resolve_set);
}

MaybeHandle<Cell> SourceTextModule::ResolveImport(
    Isolate* isolate, Handle<SourceTextModule> module, Handle<String> name,
    int module_request, MessageLocation loc, bool must_resolve,
    Module::ResolveSet* resolve_set) {
  Handle<Module> requested_module(
      Module::cast(module->requested_modules().get(module_request)), isolate);
  Handle<String> specifier(
      String::cast(module->info().module_requests().get(module_request)),
      isolate);
  MaybeHandle<Cell> result =
      Module::ResolveExport(isolate, requested_module, specifier, name, loc,
                            must_resolve, resolve_set);
  DCHECK_IMPLIES(isolate->has_pending_exception(), result.is_null());
  return result;
}

MaybeHandle<Cell> SourceTextModule::ResolveExportUsingStarExports(
    Isolate* isolate, Handle<SourceTextModule> module,
    Handle<String> module_specifier, Handle<String> export_name,
    MessageLocation loc, bool must_resolve, Module::ResolveSet* resolve_set) {
  if (!export_name->Equals(ReadOnlyRoots(isolate).default_string())) {
    // Go through all star exports looking for the given name.  If multiple star
    // exports provide the name, make sure they all map it to the same cell.
    Handle<Cell> unique_cell;
    Handle<FixedArray> special_exports(module->info().special_exports(),
                                       isolate);
    for (int i = 0, n = special_exports->length(); i < n; ++i) {
      i::Handle<i::SourceTextModuleInfoEntry> entry(
          i::SourceTextModuleInfoEntry::cast(special_exports->get(i)), isolate);
      if (!entry->export_name().IsUndefined(isolate)) {
        continue;  // Indirect export.
      }

      Handle<Script> script(module->script(), isolate);
      MessageLocation new_loc(script, entry->beg_pos(), entry->end_pos());

      Handle<Cell> cell;
      if (ResolveImport(isolate, module, export_name, entry->module_request(),
                        new_loc, false, resolve_set)
              .ToHandle(&cell)) {
        if (unique_cell.is_null()) unique_cell = cell;
        if (*unique_cell != *cell) {
          return isolate->Throw<Cell>(isolate->factory()->NewSyntaxError(
                                          MessageTemplate::kAmbiguousExport,
                                          module_specifier, export_name),
                                      &loc);
        }
      } else if (isolate->has_pending_exception()) {
        return MaybeHandle<Cell>();
      }
    }

    if (!unique_cell.is_null()) {
      // Found a unique star export for this name.
      Handle<ObjectHashTable> exports(module->exports(), isolate);
      DCHECK(exports->Lookup(export_name).IsTheHole(isolate));
      exports = ObjectHashTable::Put(exports, export_name, unique_cell);
      module->set_exports(*exports);
      return unique_cell;
    }
  }

  // Unresolvable.
  if (must_resolve) {
    return isolate->Throw<Cell>(
        isolate->factory()->NewSyntaxError(MessageTemplate::kUnresolvableExport,
                                           module_specifier, export_name),
        &loc);
  }
  return MaybeHandle<Cell>();
}

bool SourceTextModule::PrepareInstantiate(
    Isolate* isolate, Handle<SourceTextModule> module,
    v8::Local<v8::Context> context, v8::Module::ResolveCallback callback) {
  // Obtain requested modules.
  Handle<SourceTextModuleInfo> module_info(module->info(), isolate);
  Handle<FixedArray> module_requests(module_info->module_requests(), isolate);
  Handle<FixedArray> requested_modules(module->requested_modules(), isolate);
  for (int i = 0, length = module_requests->length(); i < length; ++i) {
    Handle<String> specifier(String::cast(module_requests->get(i)), isolate);
    v8::Local<v8::Module> api_requested_module;
    if (!callback(context, v8::Utils::ToLocal(specifier),
                  v8::Utils::ToLocal(Handle<Module>::cast(module)))
             .ToLocal(&api_requested_module)) {
      isolate->PromoteScheduledException();
      return false;
    }
    Handle<Module> requested_module = Utils::OpenHandle(*api_requested_module);
    requested_modules->set(i, *requested_module);
  }

  // Recurse.
  for (int i = 0, length = requested_modules->length(); i < length; ++i) {
    Handle<Module> requested_module(Module::cast(requested_modules->get(i)),
                                    isolate);
    if (!Module::PrepareInstantiate(isolate, requested_module, context,
                                    callback)) {
      return false;
    }
  }

  // Set up local exports.
  // TODO(neis): Create regular_exports array here instead of in factory method?
  for (int i = 0, n = module_info->RegularExportCount(); i < n; ++i) {
    int cell_index = module_info->RegularExportCellIndex(i);
    Handle<FixedArray> export_names(module_info->RegularExportExportNames(i),
                                    isolate);
    CreateExport(isolate, module, cell_index, export_names);
  }

  // Partially set up indirect exports.
  // For each indirect export, we create the appropriate slot in the export
  // table and store its SourceTextModuleInfoEntry there.  When we later find
  // the correct Cell in the module that actually provides the value, we replace
  // the SourceTextModuleInfoEntry by that Cell (see ResolveExport).
  Handle<FixedArray> special_exports(module_info->special_exports(), isolate);
  for (int i = 0, n = special_exports->length(); i < n; ++i) {
    Handle<SourceTextModuleInfoEntry> entry(
        SourceTextModuleInfoEntry::cast(special_exports->get(i)), isolate);
    Handle<Object> export_name(entry->export_name(), isolate);
    if (export_name->IsUndefined(isolate)) continue;  // Star export.
    CreateIndirectExport(isolate, module, Handle<String>::cast(export_name),
                         entry);
  }

  DCHECK_EQ(module->status(), kPreInstantiating);
  return true;
}

bool SourceTextModule::RunInitializationCode(Isolate* isolate,
                                             Handle<SourceTextModule> module) {
  DCHECK_EQ(module->status(), kInstantiating);
  Handle<JSFunction> function(JSFunction::cast(module->code()), isolate);
  DCHECK_EQ(MODULE_SCOPE, function->shared().scope_info().scope_type());
  Handle<Object> receiver = isolate->factory()->undefined_value();
  Handle<Object> argv[] = {module};
  MaybeHandle<Object> maybe_generator =
      Execution::Call(isolate, function, receiver, arraysize(argv), argv);
  Handle<Object> generator;
  if (!maybe_generator.ToHandle(&generator)) {
    DCHECK(isolate->has_pending_exception());
    return false;
  }
  DCHECK_EQ(*function, Handle<JSGeneratorObject>::cast(generator)->function());
  module->set_code(*generator);
  return true;
}

bool SourceTextModule::MaybeTransitionComponent(
    Isolate* isolate, Handle<SourceTextModule> module,
    ZoneForwardList<Handle<SourceTextModule>>* stack, Status new_status) {
  DCHECK(new_status == kInstantiated || new_status == kEvaluated);
  SLOW_DCHECK(
      // {module} is on the {stack}.
      std::count_if(stack->begin(), stack->end(),
                    [&](Handle<Module> m) { return *m == *module; }) == 1);
  DCHECK_LE(module->dfs_ancestor_index(), module->dfs_index());
  if (module->dfs_ancestor_index() == module->dfs_index()) {
    // This is the root of its strongly connected component.
    Handle<SourceTextModule> ancestor;
    do {
      ancestor = stack->front();
      stack->pop_front();
      DCHECK_EQ(ancestor->status(),
                new_status == kInstantiated ? kInstantiating : kEvaluating);
      if (new_status == kInstantiated) {
        if (!SourceTextModule::RunInitializationCode(isolate, ancestor))
          return false;
      }
      ancestor->SetStatus(new_status);
    } while (*ancestor != *module);
  }
  return true;
}

bool SourceTextModule::FinishInstantiate(
    Isolate* isolate, Handle<SourceTextModule> module,
    ZoneForwardList<Handle<SourceTextModule>>* stack, unsigned* dfs_index,
    Zone* zone) {
  // Instantiate SharedFunctionInfo and mark module as instantiating for
  // the recursion.
  Handle<SharedFunctionInfo> shared(SharedFunctionInfo::cast(module->code()),
                                    isolate);
  Handle<JSFunction> function =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared, isolate->native_context());
  module->set_code(*function);
  module->SetStatus(kInstantiating);
  module->set_dfs_index(*dfs_index);
  module->set_dfs_ancestor_index(*dfs_index);
  stack->push_front(module);
  (*dfs_index)++;

  // Recurse.
  Handle<FixedArray> requested_modules(module->requested_modules(), isolate);
  for (int i = 0, length = requested_modules->length(); i < length; ++i) {
    Handle<Module> requested_module(Module::cast(requested_modules->get(i)),
                                    isolate);
    if (!Module::FinishInstantiate(isolate, requested_module, stack, dfs_index,
                                   zone)) {
      return false;
    }

    DCHECK_NE(requested_module->status(), kEvaluating);
    DCHECK_GE(requested_module->status(), kInstantiating);
    SLOW_DCHECK(
        // {requested_module} is instantiating iff it's on the {stack}.
        (requested_module->status() == kInstantiating) ==
        std::count_if(stack->begin(), stack->end(), [&](Handle<Module> m) {
          return *m == *requested_module;
        }));

    if (requested_module->status() == kInstantiating) {
      // SyntheticModules go straight to kInstantiated so this must be a
      // SourceTextModule
      module->set_dfs_ancestor_index(
          std::min(module->dfs_ancestor_index(),
                   Handle<SourceTextModule>::cast(requested_module)
                       ->dfs_ancestor_index()));
    }
  }

  Handle<Script> script(module->script(), isolate);
  Handle<SourceTextModuleInfo> module_info(module->info(), isolate);

  // Resolve imports.
  Handle<FixedArray> regular_imports(module_info->regular_imports(), isolate);
  for (int i = 0, n = regular_imports->length(); i < n; ++i) {
    Handle<SourceTextModuleInfoEntry> entry(
        SourceTextModuleInfoEntry::cast(regular_imports->get(i)), isolate);
    Handle<String> name(String::cast(entry->import_name()), isolate);
    MessageLocation loc(script, entry->beg_pos(), entry->end_pos());
    ResolveSet resolve_set(zone);
    Handle<Cell> cell;
    if (!ResolveImport(isolate, module, name, entry->module_request(), loc,
                       true, &resolve_set)
             .ToHandle(&cell)) {
      return false;
    }
    module->regular_imports().set(ImportIndex(entry->cell_index()), *cell);
  }

  // Resolve indirect exports.
  Handle<FixedArray> special_exports(module_info->special_exports(), isolate);
  for (int i = 0, n = special_exports->length(); i < n; ++i) {
    Handle<SourceTextModuleInfoEntry> entry(
        SourceTextModuleInfoEntry::cast(special_exports->get(i)), isolate);
    Handle<Object> name(entry->export_name(), isolate);
    if (name->IsUndefined(isolate)) continue;  // Star export.
    MessageLocation loc(script, entry->beg_pos(), entry->end_pos());
    ResolveSet resolve_set(zone);
    if (ResolveExport(isolate, module, Handle<String>(),
                      Handle<String>::cast(name), loc, true, &resolve_set)
            .is_null()) {
      return false;
    }
  }

  return MaybeTransitionComponent(isolate, module, stack, kInstantiated);
}

void SourceTextModule::FetchStarExports(Isolate* isolate,
                                        Handle<SourceTextModule> module,
                                        Zone* zone,
                                        UnorderedModuleSet* visited) {
  DCHECK_GE(module->status(), Module::kInstantiating);

  if (module->module_namespace().IsJSModuleNamespace()) return;  // Shortcut.

  bool cycle = !visited->insert(module).second;
  if (cycle) return;
  Handle<ObjectHashTable> exports(module->exports(), isolate);
  UnorderedStringMap more_exports(zone);

  // TODO(neis): Only allocate more_exports if there are star exports.
  // Maybe split special_exports into indirect_exports and star_exports.

  ReadOnlyRoots roots(isolate);
  Handle<FixedArray> special_exports(module->info().special_exports(), isolate);
  for (int i = 0, n = special_exports->length(); i < n; ++i) {
    Handle<SourceTextModuleInfoEntry> entry(
        SourceTextModuleInfoEntry::cast(special_exports->get(i)), isolate);
    if (!entry->export_name().IsUndefined(roots)) {
      continue;  // Indirect export.
    }

    Handle<Module> requested_module(
        Module::cast(module->requested_modules().get(entry->module_request())),
        isolate);

    // Recurse.
    if (requested_module->IsSourceTextModule())
      FetchStarExports(isolate,
                       Handle<SourceTextModule>::cast(requested_module), zone,
                       visited);

    // Collect all of [requested_module]'s exports that must be added to
    // [module]'s exports (i.e. to [exports]).  We record these in
    // [more_exports].  Ambiguities (conflicting exports) are marked by mapping
    // the name to undefined instead of a Cell.
    Handle<ObjectHashTable> requested_exports(requested_module->exports(),
                                              isolate);
    for (int i = 0, n = requested_exports->Capacity(); i < n; ++i) {
      Object key;
      if (!requested_exports->ToKey(roots, i, &key)) continue;
      Handle<String> name(String::cast(key), isolate);

      if (name->Equals(roots.default_string())) continue;
      if (!exports->Lookup(name).IsTheHole(roots)) continue;

      Handle<Cell> cell(Cell::cast(requested_exports->ValueAt(i)), isolate);
      auto insert_result = more_exports.insert(std::make_pair(name, cell));
      if (!insert_result.second) {
        auto it = insert_result.first;
        if (*it->second == *cell || it->second->IsUndefined(roots)) {
          // We already recorded this mapping before, or the name is already
          // known to be ambiguous.  In either case, there's nothing to do.
        } else {
          DCHECK(it->second->IsCell());
          // Different star exports provide different cells for this name, hence
          // mark the name as ambiguous.
          it->second = roots.undefined_value_handle();
        }
      }
    }
  }

  // Copy [more_exports] into [exports].
  for (const auto& elem : more_exports) {
    if (elem.second->IsUndefined(isolate)) continue;  // Ambiguous export.
    DCHECK(!elem.first->Equals(ReadOnlyRoots(isolate).default_string()));
    DCHECK(elem.second->IsCell());
    exports = ObjectHashTable::Put(exports, elem.first, elem.second);
  }
  module->set_exports(*exports);
}

Handle<JSModuleNamespace> SourceTextModule::GetModuleNamespace(
    Isolate* isolate, Handle<SourceTextModule> module, int module_request) {
  Handle<Module> requested_module(
      Module::cast(module->requested_modules().get(module_request)), isolate);
  return Module::GetModuleNamespace(isolate, requested_module);
}

MaybeHandle<Object> SourceTextModule::Evaluate(
    Isolate* isolate, Handle<SourceTextModule> module,
    ZoneForwardList<Handle<SourceTextModule>>* stack, unsigned* dfs_index) {
  Handle<JSGeneratorObject> generator(JSGeneratorObject::cast(module->code()),
                                      isolate);
  module->set_code(
      generator->function().shared().scope_info().ModuleDescriptorInfo());
  module->SetStatus(kEvaluating);
  module->set_dfs_index(*dfs_index);
  module->set_dfs_ancestor_index(*dfs_index);
  stack->push_front(module);
  (*dfs_index)++;

  // Recursion.
  Handle<FixedArray> requested_modules(module->requested_modules(), isolate);
  for (int i = 0, length = requested_modules->length(); i < length; ++i) {
    Handle<Module> requested_module(Module::cast(requested_modules->get(i)),
                                    isolate);
    RETURN_ON_EXCEPTION(
        isolate, Module::Evaluate(isolate, requested_module, stack, dfs_index),
        Object);

    DCHECK_GE(requested_module->status(), kEvaluating);
    DCHECK_NE(requested_module->status(), kErrored);
    SLOW_DCHECK(
        // {requested_module} is evaluating iff it's on the {stack}.
        (requested_module->status() == kEvaluating) ==
        std::count_if(stack->begin(), stack->end(), [&](Handle<Module> m) {
          return *m == *requested_module;
        }));

    if (requested_module->status() == kEvaluating) {
      // SyntheticModules go straight to kEvaluated so this must be a
      // SourceTextModule
      module->set_dfs_ancestor_index(
          std::min(module->dfs_ancestor_index(),
                   Handle<SourceTextModule>::cast(requested_module)
                       ->dfs_ancestor_index()));
    }
  }

  // Evaluation of module body.
  Handle<JSFunction> resume(
      isolate->native_context()->generator_next_internal(), isolate);
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result, Execution::Call(isolate, resume, generator, 0, nullptr),
      Object);
  DCHECK(JSIteratorResult::cast(*result).done().BooleanValue(isolate));

  CHECK(MaybeTransitionComponent(isolate, module, stack, kEvaluated));
  return handle(JSIteratorResult::cast(*result).value(), isolate);
}

void SourceTextModule::Reset(Isolate* isolate,
                             Handle<SourceTextModule> module) {
  Factory* factory = isolate->factory();

  DCHECK(module->import_meta().IsTheHole(isolate));

  Handle<FixedArray> regular_exports =
      factory->NewFixedArray(module->regular_exports().length());
  Handle<FixedArray> regular_imports =
      factory->NewFixedArray(module->regular_imports().length());
  Handle<FixedArray> requested_modules =
      factory->NewFixedArray(module->requested_modules().length());

  if (module->status() == kInstantiating) {
    module->set_code(JSFunction::cast(module->code()).shared());
  }
  module->set_regular_exports(*regular_exports);
  module->set_regular_imports(*regular_imports);
  module->set_requested_modules(*requested_modules);
  module->set_dfs_index(-1);
  module->set_dfs_ancestor_index(-1);
}

}  // namespace internal
}  // namespace v8
