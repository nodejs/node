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
  switch (status()) {
    case kUninstantiated:
    case kPreInstantiating:
      DCHECK(code().IsSharedFunctionInfo());
      return SharedFunctionInfo::cast(code());
    case kInstantiating:
      DCHECK(code().IsJSFunction());
      return JSFunction::cast(code()).shared();
    case kInstantiated:
    case kEvaluating:
    case kEvaluated:
      DCHECK(code().IsJSGeneratorObject());
      return JSGeneratorObject::cast(code()).function().shared();
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

  Handle<ScopeInfo> scope_info(function->shared().scope_info(), isolate);
  Handle<Context> context = isolate->factory()->NewModuleContext(
      module, isolate->native_context(), scope_info);
  function->set_context(*context);

  MaybeHandle<Object> maybe_generator =
      Execution::Call(isolate, function, receiver, 0, {});
  Handle<Object> generator;
  if (!maybe_generator.ToHandle(&generator)) {
    DCHECK(isolate->has_pending_exception());
    return false;
  }
  DCHECK_EQ(*function, Handle<JSGeneratorObject>::cast(generator)->function());
  module->set_code(JSGeneratorObject::cast(*generator));
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
    for (InternalIndex i : requested_exports->IterateEntries()) {
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

MaybeHandle<Object> SourceTextModule::EvaluateMaybeAsync(
    Isolate* isolate, Handle<SourceTextModule> module) {
  // In the event of errored evaluation, return a rejected promise.
  if (module->status() == kErrored) {
    // If we have a top level capability we assume it has already been
    // rejected, and return it here. Otherwise create a new promise and
    // reject it with the module's exception.
    if (module->top_level_capability().IsJSPromise()) {
      Handle<JSPromise> top_level_capability(
          JSPromise::cast(module->top_level_capability()), isolate);
      DCHECK(top_level_capability->status() == Promise::kRejected &&
             top_level_capability->result() == module->exception());
      return top_level_capability;
    }
    Handle<JSPromise> capability = isolate->factory()->NewJSPromise();
    JSPromise::Reject(capability, handle(module->exception(), isolate));
    return capability;
  }

  // Start of Evaluate () Concrete Method
  // 2. Assert: module.[[Status]] is "linked" or "evaluated".
  CHECK(module->status() == kInstantiated || module->status() == kEvaluated);

  // 3. If module.[[Status]] is "evaluated", set module to
  //    GetAsyncCycleRoot(module).
  if (module->status() == kEvaluated) {
    module = GetAsyncCycleRoot(isolate, module);
  }

  // 4. If module.[[TopLevelCapability]] is not undefined, then
  //    a. Return module.[[TopLevelCapability]].[[Promise]].
  if (module->top_level_capability().IsJSPromise()) {
    return handle(JSPromise::cast(module->top_level_capability()), isolate);
  }
  DCHECK(module->top_level_capability().IsUndefined());

  // 6. Let capability be ! NewPromiseCapability(%Promise%).
  Handle<JSPromise> capability = isolate->factory()->NewJSPromise();

  // 7. Set module.[[TopLevelCapability]] to capability.
  module->set_top_level_capability(*capability);
  DCHECK(module->top_level_capability().IsJSPromise());

  // 9. If result is an abrupt completion, then
  Handle<Object> unused_result;
  if (!Evaluate(isolate, module).ToHandle(&unused_result)) {
    // If the exception was a termination exception, rejecting the promise
    // would resume execution, and our API contract is to return an empty
    // handle. The module's status should be set to kErrored and the
    // exception field should be set to `null`.
    if (!isolate->is_catchable_by_javascript(isolate->pending_exception())) {
      DCHECK_EQ(module->status(), kErrored);
      DCHECK_EQ(module->exception(), *isolate->factory()->null_value());
      return {};
    }

    //  d. Perform ! Call(capability.[[Reject]], undefined,
    //                    «result.[[Value]]»).
    isolate->clear_pending_exception();
    JSPromise::Reject(capability, handle(module->exception(), isolate));
  } else {
    // 10. Otherwise,
    //  a. Assert: module.[[Status]] is "evaluated"...
    CHECK_EQ(module->status(), kEvaluated);

    //  b. If module.[[AsyncEvaluating]] is false, then
    if (!module->async_evaluating()) {
      //   i. Perform ! Call(capability.[[Resolve]], undefined,
      //                     «undefined»).
      JSPromise::Resolve(capability, isolate->factory()->undefined_value())
          .ToHandleChecked();
    }
  }

  // 11. Return capability.[[Promise]].
  return capability;
}

MaybeHandle<Object> SourceTextModule::Evaluate(
    Isolate* isolate, Handle<SourceTextModule> module) {
  // Evaluate () Concrete Method continued from EvaluateMaybeAsync.
  CHECK(module->status() == kInstantiated || module->status() == kEvaluated);

  // 5. Let stack be a new empty List.
  Zone zone(isolate->allocator(), ZONE_NAME);
  ZoneForwardList<Handle<SourceTextModule>> stack(&zone);
  unsigned dfs_index = 0;

  // 8. Let result be InnerModuleEvaluation(module, stack, 0).
  // 9. If result is an abrupt completion, then
  Handle<Object> result;
  if (!InnerModuleEvaluation(isolate, module, &stack, &dfs_index)
           .ToHandle(&result)) {
    //  a. For each Cyclic Module Record m in stack, do
    for (auto& descendant : stack) {
      //   i. Assert: m.[[Status]] is "evaluating".
      CHECK_EQ(descendant->status(), kEvaluating);
      //  ii. Set m.[[Status]] to "evaluated".
      // iii. Set m.[[EvaluationError]] to result.
      descendant->RecordErrorUsingPendingException(isolate);
    }

#ifdef DEBUG
    if (isolate->is_catchable_by_javascript(isolate->pending_exception())) {
      CHECK_EQ(module->exception(), isolate->pending_exception());
    } else {
      CHECK_EQ(module->exception(), *isolate->factory()->null_value());
    }
#endif  // DEBUG
  } else {
    // 10. Otherwise,
    //  c. Assert: stack is empty.
    DCHECK(stack.empty());
  }
  return result;
}

void SourceTextModule::AsyncModuleExecutionFulfilled(
    Isolate* isolate, Handle<SourceTextModule> module) {
  // 1. Assert: module.[[Status]] is "evaluated".
  CHECK(module->status() == kEvaluated || module->status() == kErrored);

  // 2. If module.[[AsyncEvaluating]] is false,
  if (!module->async_evaluating()) {
    //  a. Assert: module.[[EvaluationError]] is not undefined.
    CHECK_EQ(module->status(), kErrored);

    //  b. Return undefined.
    return;
  }

  // 3. Assert: module.[[EvaluationError]] is undefined.
  CHECK_EQ(module->status(), kEvaluated);

  // 4. Set module.[[AsyncEvaluating]] to false.
  module->set_async_evaluating(false);

  // 5. For each Module m of module.[[AsyncParentModules]], do
  for (int i = 0; i < module->AsyncParentModuleCount(); i++) {
    Handle<SourceTextModule> m = module->GetAsyncParentModule(isolate, i);

    //  a. If module.[[DFSIndex]] is not equal to module.[[DFSAncestorIndex]],
    //     then
    if (module->dfs_index() != module->dfs_ancestor_index()) {
      //   i. Assert: m.[[DFSAncestorIndex]] is equal to
      //      module.[[DFSAncestorIndex]].
      DCHECK_LE(m->dfs_ancestor_index(), module->dfs_ancestor_index());
    }
    //  b. Decrement m.[[PendingAsyncDependencies]] by 1.
    m->DecrementPendingAsyncDependencies();

    //  c. If m.[[PendingAsyncDependencies]] is 0 and m.[[EvaluationError]] is
    //     undefined, then
    if (!m->HasPendingAsyncDependencies() && m->status() == kEvaluated) {
      //   i. Assert: m.[[AsyncEvaluating]] is true.
      DCHECK(m->async_evaluating());

      //  ii. Let cycleRoot be ! GetAsyncCycleRoot(m).
      auto cycle_root = GetAsyncCycleRoot(isolate, m);

      // iii. If cycleRoot.[[EvaluationError]] is not undefined,
      //      return undefined.
      if (cycle_root->status() == kErrored) {
        return;
      }

      //  iv. If m.[[Async]] is true, then
      if (m->async()) {
        //    1. Perform ! ExecuteAsyncModule(m).
        ExecuteAsyncModule(isolate, m);
      } else {
        // v. Otherwise,
        //    1. Let result be m.ExecuteModule().
        //    2. If result is a normal completion,
        Handle<Object> unused_result;
        if (ExecuteModule(isolate, m).ToHandle(&unused_result)) {
          //     a. Perform ! AsyncModuleExecutionFulfilled(m).
          AsyncModuleExecutionFulfilled(isolate, m);
        } else {
          //  3. Otherwise,
          //     a. Perform ! AsyncModuleExecutionRejected(m,
          //                                               result.[[Value]]).
          Handle<Object> exception(isolate->pending_exception(), isolate);
          isolate->clear_pending_exception();
          AsyncModuleExecutionRejected(isolate, m, exception);
        }
      }
    }
  }

  // 6. If module.[[TopLevelCapability]] is not undefined, then
  if (!module->top_level_capability().IsUndefined(isolate)) {
    //  a. Assert: module.[[DFSIndex]] is equal to module.[[DFSAncestorIndex]].
    DCHECK_EQ(module->dfs_index(), module->dfs_ancestor_index());

    //  b. Perform ! Call(module.[[TopLevelCapability]].[[Resolve]],
    //                    undefined, «undefined»).
    Handle<JSPromise> capability(
        JSPromise::cast(module->top_level_capability()), isolate);
    JSPromise::Resolve(capability, isolate->factory()->undefined_value())
        .ToHandleChecked();
  }

  // 7. Return undefined.
}

void SourceTextModule::AsyncModuleExecutionRejected(
    Isolate* isolate, Handle<SourceTextModule> module,
    Handle<Object> exception) {
  DCHECK(isolate->is_catchable_by_javascript(*exception));

  // 1. Assert: module.[[Status]] is "evaluated".
  CHECK(module->status() == kEvaluated || module->status() == kErrored);

  // 2. If module.[[AsyncEvaluating]] is false,
  if (!module->async_evaluating()) {
    //  a. Assert: module.[[EvaluationError]] is not undefined.
    CHECK_EQ(module->status(), kErrored);

    //  b. Return undefined.
    return;
  }

  // 4. Set module.[[EvaluationError]] to ThrowCompletion(error).
  module->RecordError(isolate, exception);

  // 5. Set module.[[AsyncEvaluating]] to false.
  module->set_async_evaluating(false);

  // 6. For each Module m of module.[[AsyncParentModules]], do
  for (int i = 0; i < module->AsyncParentModuleCount(); i++) {
    Handle<SourceTextModule> m = module->GetAsyncParentModule(isolate, i);

    //  a. If module.[[DFSIndex]] is not equal to module.[[DFSAncestorIndex]],
    //     then
    if (module->dfs_index() != module->dfs_ancestor_index()) {
      //   i. Assert: m.[[DFSAncestorIndex]] is equal to
      //      module.[[DFSAncestorIndex]].
      DCHECK_EQ(m->dfs_ancestor_index(), module->dfs_ancestor_index());
    }
    //  b. Perform ! AsyncModuleExecutionRejected(m, error).
    AsyncModuleExecutionRejected(isolate, m, exception);
  }

  // 7. If module.[[TopLevelCapability]] is not undefined, then
  if (!module->top_level_capability().IsUndefined(isolate)) {
    //  a. Assert: module.[[DFSIndex]] is equal to module.[[DFSAncestorIndex]].
    DCHECK(module->dfs_index() == module->dfs_ancestor_index());

    //  b. Perform ! Call(module.[[TopLevelCapability]].[[Reject]],
    //                    undefined, «error»).
    Handle<JSPromise> capability(
        JSPromise::cast(module->top_level_capability()), isolate);
    JSPromise::Reject(capability, exception);
  }

  // 8. Return undefined.
}

void SourceTextModule::ExecuteAsyncModule(Isolate* isolate,
                                          Handle<SourceTextModule> module) {
  // 1. Assert: module.[[Status]] is "evaluating" or "evaluated".
  CHECK(module->status() == kEvaluating || module->status() == kEvaluated);

  // 2. Assert: module.[[Async]] is true.
  DCHECK(module->async());

  // 3. Set module.[[AsyncEvaluating]] to true.
  module->set_async_evaluating(true);

  // 4. Let capability be ! NewPromiseCapability(%Promise%).
  Handle<JSPromise> capability = isolate->factory()->NewJSPromise();

  // 5. Let stepsFulfilled be the steps of a CallAsyncModuleFulfilled
  Handle<JSFunction> steps_fulfilled(
      isolate->native_context()->call_async_module_fulfilled(), isolate);

  ScopedVector<Handle<Object>> empty_argv(0);

  // 6. Let onFulfilled be CreateBuiltinFunction(stepsFulfilled,
  //                                             «[[Module]]»).
  // 7. Set onFulfilled.[[Module]] to module.
  Handle<JSBoundFunction> on_fulfilled =
      isolate->factory()
          ->NewJSBoundFunction(steps_fulfilled, module, empty_argv)
          .ToHandleChecked();

  // 8. Let stepsRejected be the steps of a CallAsyncModuleRejected.
  Handle<JSFunction> steps_rejected(
      isolate->native_context()->call_async_module_rejected(), isolate);

  // 9. Let onRejected be CreateBuiltinFunction(stepsRejected, «[[Module]]»).
  // 10. Set onRejected.[[Module]] to module.
  Handle<JSBoundFunction> on_rejected =
      isolate->factory()
          ->NewJSBoundFunction(steps_rejected, module, empty_argv)
          .ToHandleChecked();

  // 11. Perform ! PerformPromiseThen(capability.[[Promise]],
  //                                  onFulfilled, onRejected).
  Handle<Object> argv[] = {on_fulfilled, on_rejected};
  Execution::CallBuiltin(isolate, isolate->promise_then(), capability,
                         arraysize(argv), argv)
      .ToHandleChecked();

  // 12. Perform ! module.ExecuteModule(capability).
  // Note: In V8 we have broken module.ExecuteModule into
  // ExecuteModule for synchronous module execution and
  // InnerExecuteAsyncModule for asynchronous execution.
  InnerExecuteAsyncModule(isolate, module, capability).ToHandleChecked();

  // 13. Return.
}

MaybeHandle<Object> SourceTextModule::InnerExecuteAsyncModule(
    Isolate* isolate, Handle<SourceTextModule> module,
    Handle<JSPromise> capability) {
  // If we have an async module, then it has an associated
  // JSAsyncFunctionObject, which we then evaluate with the passed in promise
  // capability.
  Handle<JSAsyncFunctionObject> async_function_object(
      JSAsyncFunctionObject::cast(module->code()), isolate);
  async_function_object->set_promise(*capability);
  Handle<JSFunction> resume(
      isolate->native_context()->async_module_evaluate_internal(), isolate);
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      Execution::TryCall(isolate, resume, async_function_object, 0, nullptr,
                         Execution::MessageHandling::kKeepPending, nullptr,
                         false),
      Object);
  return result;
}

MaybeHandle<Object> SourceTextModule::ExecuteModule(
    Isolate* isolate, Handle<SourceTextModule> module) {
  // Synchronous modules have an associated JSGeneratorObject.
  Handle<JSGeneratorObject> generator(JSGeneratorObject::cast(module->code()),
                                      isolate);
  Handle<JSFunction> resume(
      isolate->native_context()->generator_next_internal(), isolate);
  Handle<Object> result;

  // With top_level_await, we need to catch any exceptions and reject
  // the top level capability.
  if (FLAG_harmony_top_level_await) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::TryCall(isolate, resume, generator, 0, nullptr,
                           Execution::MessageHandling::kKeepPending, nullptr,
                           false),
        Object);
  } else {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, resume, generator, 0, nullptr), Object);
  }
  DCHECK(JSIteratorResult::cast(*result).done().BooleanValue(isolate));
  return handle(JSIteratorResult::cast(*result).value(), isolate);
}

MaybeHandle<Object> SourceTextModule::InnerModuleEvaluation(
    Isolate* isolate, Handle<SourceTextModule> module,
    ZoneForwardList<Handle<SourceTextModule>>* stack, unsigned* dfs_index) {
  STACK_CHECK(isolate, MaybeHandle<Object>());

  // InnerModuleEvaluation(module, stack, index)
  // 2. If module.[[Status]] is "evaluated", then
  //    a. If module.[[EvaluationError]] is undefined, return index.
  //       (We return undefined instead)
  if (module->status() == kEvaluated || module->status() == kEvaluating) {
    return isolate->factory()->undefined_value();
  }

  //    b. Otherwise return module.[[EvaluationError]].
  //       (We throw on isolate and return a MaybeHandle<Object>
  //        instead)
  if (module->status() == kErrored) {
    isolate->Throw(module->exception());
    return MaybeHandle<Object>();
  }

  // 4. Assert: module.[[Status]] is "linked".
  CHECK_EQ(module->status(), kInstantiated);

  // 5. Set module.[[Status]] to "evaluating".
  module->SetStatus(kEvaluating);

  // 6. Set module.[[DFSIndex]] to index.
  module->set_dfs_index(*dfs_index);

  // 7. Set module.[[DFSAncestorIndex]] to index.
  module->set_dfs_ancestor_index(*dfs_index);

  // 8. Set module.[[PendingAsyncDependencies]] to 0.
  DCHECK(!module->HasPendingAsyncDependencies());

  // 9. Set module.[[AsyncParentModules]] to a new empty List.
  Handle<ArrayList> async_parent_modules = ArrayList::New(isolate, 0);
  module->set_async_parent_modules(*async_parent_modules);

  // 10. Set index to index + 1.
  (*dfs_index)++;

  // 11. Append module to stack.
  stack->push_front(module);

  // Recursion.
  Handle<FixedArray> requested_modules(module->requested_modules(), isolate);

  // 12. For each String required that is an element of
  //     module.[[RequestedModules]], do
  for (int i = 0, length = requested_modules->length(); i < length; ++i) {
    Handle<Module> requested_module(Module::cast(requested_modules->get(i)),
                                    isolate);
    //   d. If requiredModule is a Cyclic Module Record, then
    if (requested_module->IsSourceTextModule()) {
      Handle<SourceTextModule> required_module(
          SourceTextModule::cast(*requested_module), isolate);
      RETURN_ON_EXCEPTION(
          isolate,
          InnerModuleEvaluation(isolate, required_module, stack, dfs_index),
          Object);

      //    i. Assert: requiredModule.[[Status]] is either "evaluating" or
      //       "evaluated".
      //       (We also assert the module cannot be errored, because if it was
      //        we would have already returned from InnerModuleEvaluation)
      CHECK_GE(required_module->status(), kEvaluating);
      CHECK_NE(required_module->status(), kErrored);

      //   ii.  Assert: requiredModule.[[Status]] is "evaluating" if and
      //        only if requiredModule is in stack.
      SLOW_DCHECK(
          (requested_module->status() == kEvaluating) ==
          std::count_if(stack->begin(), stack->end(), [&](Handle<Module> m) {
            return *m == *requested_module;
          }));

      //  iii.  If requiredModule.[[Status]] is "evaluating", then
      if (required_module->status() == kEvaluating) {
        //      1. Set module.[[DFSAncestorIndex]] to
        //         min(
        //           module.[[DFSAncestorIndex]],
        //           requiredModule.[[DFSAncestorIndex]]).
        module->set_dfs_ancestor_index(
            std::min(module->dfs_ancestor_index(),
                     required_module->dfs_ancestor_index()));
      } else {
        //   iv. Otherwise,
        //      1. Set requiredModule to GetAsyncCycleRoot(requiredModule).
        required_module = GetAsyncCycleRoot(isolate, required_module);

        //      2. Assert: requiredModule.[[Status]] is "evaluated".
        CHECK_GE(required_module->status(), kEvaluated);

        //      3. If requiredModule.[[EvaluationError]] is not undefined,
        //         return module.[[EvaluationError]].
        //         (If there was an exception on the original required module
        //          we would have already returned. This check handles the case
        //          where the AsyncCycleRoot has an error. Instead of returning
        //          the exception, we throw on isolate and return a
        //          MaybeHandle<Object>)
        if (required_module->status() == kErrored) {
          isolate->Throw(required_module->exception());
          return MaybeHandle<Object>();
        }
      }
      //     v. If requiredModule.[[AsyncEvaluating]] is true, then
      if (required_module->async_evaluating()) {
        //      1. Set module.[[PendingAsyncDependencies]] to
        //         module.[[PendingAsyncDependencies]] + 1.
        module->IncrementPendingAsyncDependencies();

        //      2. Append module to requiredModule.[[AsyncParentModules]].
        AddAsyncParentModule(isolate, required_module, module);
      }
    } else {
      RETURN_ON_EXCEPTION(isolate, Module::Evaluate(isolate, requested_module),
                          Object);
    }
  }

  // The spec returns the module index for proper numbering of dependencies.
  // However, we pass the module index by pointer instead.
  //
  // Before async modules v8 returned the value result from calling next
  // on the module's implicit iterator. We preserve this behavior for
  // synchronous modules, but return undefined for AsyncModules.
  Handle<Object> result = isolate->factory()->undefined_value();

  // 14. If module.[[PendingAsyncDependencies]] is > 0, set
  //     module.[[AsyncEvaluating]] to true.
  if (module->HasPendingAsyncDependencies()) {
    module->set_async_evaluating(true);
  } else if (module->async()) {
    // 15. Otherwise, if module.[[Async]] is true,
    //     perform ! ExecuteAsyncModule(module).
    SourceTextModule::ExecuteAsyncModule(isolate, module);
  } else {
    // 16. Otherwise, perform ? module.ExecuteModule().
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result, ExecuteModule(isolate, module),
                               Object);
  }

  CHECK(MaybeTransitionComponent(isolate, module, stack, kEvaluated));
  return result;
}

Handle<SourceTextModule> SourceTextModule::GetAsyncCycleRoot(
    Isolate* isolate, Handle<SourceTextModule> module) {
  // 1. Assert: module.[[Status]] is "evaluated".
  CHECK_GE(module->status(), kEvaluated);

  // 2. If module.[[AsyncParentModules]] is an empty List, return module.
  if (module->AsyncParentModuleCount() == 0) {
    return module;
  }

  // 3. Repeat, while module.[[DFSIndex]] is greater than
  //    module.[[DFSAncestorIndex]],
  while (module->dfs_index() > module->dfs_ancestor_index()) {
    //  a. Assert: module.[[AsyncParentModules]] is a non-empty List.
    DCHECK_GT(module->AsyncParentModuleCount(), 0);

    //  b. Let nextCycleModule be the first element of
    //     module.[[AsyncParentModules]].
    Handle<SourceTextModule> next_cycle_module =
        module->GetAsyncParentModule(isolate, 0);

    //  c. Assert: nextCycleModule.[[DFSAncestorIndex]] is less than or equal
    //     to module.[[DFSAncestorIndex]].
    DCHECK_LE(next_cycle_module->dfs_ancestor_index(),
              module->dfs_ancestor_index());

    //  d. Set module to nextCycleModule
    module = next_cycle_module;
  }

  // 4. Assert: module.[[DFSIndex]] is equal to module.[[DFSAncestorIndex]].
  DCHECK_EQ(module->dfs_index(), module->dfs_ancestor_index());

  // 5. Return module.
  return module;
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
