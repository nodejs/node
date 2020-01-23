// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>
#include <unordered_set>

#include "src/objects/module.h"

#include "src/api/api-inl.h"
#include "src/ast/modules.h"
#include "src/builtins/accessors.h"
#include "src/heap/heap-inl.h"
#include "src/objects/cell-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
void Module::PrintStatusTransition(Status new_status) {
  if (FLAG_trace_module_status) {
    StdoutStream os;
    os << "Changing module status from " << status() << " to " << new_status
       << " for ";
    if (this->IsSourceTextModule()) {
      Handle<Script> script(SourceTextModule::cast(*this).script(),
                            GetIsolate());
      script->GetNameOrSourceURL().Print(os);
    } else {
      SyntheticModule::cast(*this).name().Print(os);
    }
#ifndef OBJECT_PRINT
    os << "\n";
#endif  // OBJECT_PRINT
  }
}
#endif  // DEBUG

void Module::SetStatus(Status new_status) {
  DisallowHeapAllocation no_alloc;
  DCHECK_LE(status(), new_status);
  DCHECK_NE(new_status, Module::kErrored);
#ifdef DEBUG
  PrintStatusTransition(new_status);
#endif  // DEBUG
  set_status(new_status);
}

void Module::RecordErrorUsingPendingException(Isolate* isolate) {
  Handle<Object> the_exception(isolate->pending_exception(), isolate);
  RecordError(isolate, the_exception);
}

void Module::RecordError(Isolate* isolate, Handle<Object> error) {
  DCHECK(exception().IsTheHole(isolate));
  DCHECK(!error->IsTheHole(isolate));
  if (this->IsSourceTextModule()) {
    Handle<SourceTextModule> self(SourceTextModule::cast(*this), GetIsolate());
    self->set_code(self->info());
  }
#ifdef DEBUG
  PrintStatusTransition(Module::kErrored);
#endif  // DEBUG
  set_status(Module::kErrored);
  set_exception(*error);
}

void Module::ResetGraph(Isolate* isolate, Handle<Module> module) {
  DCHECK_NE(module->status(), kInstantiating);
  DCHECK_NE(module->status(), kEvaluating);
  if (module->status() != kPreInstantiating) return;

  Handle<FixedArray> requested_modules =
      module->IsSourceTextModule()
          ? Handle<FixedArray>(
                Handle<SourceTextModule>::cast(module)->requested_modules(),
                isolate)
          : Handle<FixedArray>();
  Reset(isolate, module);
  if (module->IsSourceTextModule()) {
    for (int i = 0; i < requested_modules->length(); ++i) {
      Handle<Object> descendant(requested_modules->get(i), isolate);
      if (descendant->IsModule()) {
        ResetGraph(isolate, Handle<Module>::cast(descendant));
      } else {
        DCHECK(descendant->IsUndefined(isolate));
      }
    }
  } else {
    DCHECK(module->IsSyntheticModule());
    // Nothing else to do here.
  }
}

void Module::Reset(Isolate* isolate, Handle<Module> module) {
  DCHECK(module->status() == kPreInstantiating ||
         module->status() == kInstantiating);
  DCHECK(module->exception().IsTheHole(isolate));
  // The namespace object cannot exist, because it would have been created
  // by RunInitializationCode, which is called only after this module's SCC
  // succeeds instantiation.
  DCHECK(!module->module_namespace().IsJSModuleNamespace());

#ifdef DEBUG
  module->PrintStatusTransition(kUninstantiated);
#endif  // DEBUG

  const int export_count =
      module->IsSourceTextModule()
          ? Handle<SourceTextModule>::cast(module)->regular_exports().length()
          : Handle<SyntheticModule>::cast(module)->export_names().length();
  Handle<ObjectHashTable> exports = ObjectHashTable::New(isolate, export_count);

  if (module->IsSourceTextModule()) {
    SourceTextModule::Reset(isolate, Handle<SourceTextModule>::cast(module));
  } else {
    // Nothing to do here.
  }

  module->set_exports(*exports);
  module->set_status(kUninstantiated);
}

Object Module::GetException() {
  DisallowHeapAllocation no_alloc;
  DCHECK_EQ(status(), Module::kErrored);
  DCHECK(!exception().IsTheHole());
  return exception();
}

MaybeHandle<Cell> Module::ResolveExport(Isolate* isolate, Handle<Module> module,
                                        Handle<String> module_specifier,
                                        Handle<String> export_name,
                                        MessageLocation loc, bool must_resolve,
                                        Module::ResolveSet* resolve_set) {
  DCHECK_GE(module->status(), kPreInstantiating);
  DCHECK_NE(module->status(), kEvaluating);

  if (module->IsSourceTextModule()) {
    return SourceTextModule::ResolveExport(
        isolate, Handle<SourceTextModule>::cast(module), module_specifier,
        export_name, loc, must_resolve, resolve_set);
  } else {
    return SyntheticModule::ResolveExport(
        isolate, Handle<SyntheticModule>::cast(module), module_specifier,
        export_name, loc, must_resolve);
  }
}

bool Module::Instantiate(Isolate* isolate, Handle<Module> module,
                         v8::Local<v8::Context> context,
                         v8::Module::ResolveCallback callback) {
#ifdef DEBUG
  if (FLAG_trace_module_status) {
    StdoutStream os;
    os << "Instantiating module ";
    if (module->IsSourceTextModule()) {
      Handle<SourceTextModule>::cast(module)
          ->script()
          .GetNameOrSourceURL()
          .Print(os);
    } else {
      Handle<SyntheticModule>::cast(module)->name().Print(os);
    }
#ifndef OBJECT_PRINT
    os << "\n";
#endif  // OBJECT_PRINT
  }
#endif  // DEBUG

  if (!PrepareInstantiate(isolate, module, context, callback)) {
    ResetGraph(isolate, module);
    return false;
  }
  Zone zone(isolate->allocator(), ZONE_NAME);
  ZoneForwardList<Handle<SourceTextModule>> stack(&zone);
  unsigned dfs_index = 0;
  if (!FinishInstantiate(isolate, module, &stack, &dfs_index, &zone)) {
    for (auto& descendant : stack) {
      Reset(isolate, descendant);
    }
    DCHECK_EQ(module->status(), kUninstantiated);
    return false;
  }
  DCHECK(module->status() == kInstantiated || module->status() == kEvaluated ||
         module->status() == kErrored);
  DCHECK(stack.empty());
  return true;
}

bool Module::PrepareInstantiate(Isolate* isolate, Handle<Module> module,
                                v8::Local<v8::Context> context,
                                v8::Module::ResolveCallback callback) {
  DCHECK_NE(module->status(), kEvaluating);
  DCHECK_NE(module->status(), kInstantiating);
  if (module->status() >= kPreInstantiating) return true;
  module->SetStatus(kPreInstantiating);
  STACK_CHECK(isolate, false);

  if (module->IsSourceTextModule()) {
    return SourceTextModule::PrepareInstantiate(
        isolate, Handle<SourceTextModule>::cast(module), context, callback);
  } else {
    return SyntheticModule::PrepareInstantiate(
        isolate, Handle<SyntheticModule>::cast(module), context, callback);
  }
}

bool Module::FinishInstantiate(Isolate* isolate, Handle<Module> module,
                               ZoneForwardList<Handle<SourceTextModule>>* stack,
                               unsigned* dfs_index, Zone* zone) {
  DCHECK_NE(module->status(), kEvaluating);
  if (module->status() >= kInstantiating) return true;
  DCHECK_EQ(module->status(), kPreInstantiating);
  STACK_CHECK(isolate, false);

  if (module->IsSourceTextModule()) {
    return SourceTextModule::FinishInstantiate(
        isolate, Handle<SourceTextModule>::cast(module), stack, dfs_index,
        zone);
  } else {
    return SyntheticModule::FinishInstantiate(
        isolate, Handle<SyntheticModule>::cast(module));
  }
}

MaybeHandle<Object> Module::Evaluate(Isolate* isolate, Handle<Module> module) {
#ifdef DEBUG
  if (FLAG_trace_module_status) {
    StdoutStream os;
    os << "Evaluating module ";
    if (module->IsSourceTextModule()) {
      Handle<SourceTextModule>::cast(module)
          ->script()
          .GetNameOrSourceURL()
          .Print(os);
    } else {
      Handle<SyntheticModule>::cast(module)->name().Print(os);
    }
#ifndef OBJECT_PRINT
    os << "\n";
#endif  // OBJECT_PRINT
  }
#endif  // DEBUG
  STACK_CHECK(isolate, MaybeHandle<Object>());
  if (FLAG_harmony_top_level_await && module->IsSourceTextModule()) {
    return SourceTextModule::EvaluateMaybeAsync(
        isolate, Handle<SourceTextModule>::cast(module));
  } else {
    return Module::InnerEvaluate(isolate, module);
  }
}

MaybeHandle<Object> Module::InnerEvaluate(Isolate* isolate,
                                          Handle<Module> module) {
  if (module->status() == kErrored) {
    isolate->Throw(module->GetException());
    return MaybeHandle<Object>();
  } else if (module->status() == kEvaluated) {
    return isolate->factory()->undefined_value();
  }

  // InnerEvaluate can be called both to evaluate top level modules without
  // the harmony_top_level_await flag and recursively to evaluate
  // SyntheticModules in the dependency graphs of SourceTextModules.
  //
  // However, SyntheticModules transition directly to 'Evaluated,' so we should
  // never see an 'Evaluating' module at this point.
  CHECK_EQ(module->status(), kInstantiated);

  if (module->IsSourceTextModule()) {
    return SourceTextModule::Evaluate(isolate,
                                      Handle<SourceTextModule>::cast(module));
  } else {
    return SyntheticModule::Evaluate(isolate,
                                     Handle<SyntheticModule>::cast(module));
  }
}

Handle<JSModuleNamespace> Module::GetModuleNamespace(Isolate* isolate,
                                                     Handle<Module> module) {
  Handle<HeapObject> object(module->module_namespace(), isolate);
  ReadOnlyRoots roots(isolate);
  if (!object->IsUndefined(roots)) {
    // Namespace object already exists.
    return Handle<JSModuleNamespace>::cast(object);
  }

  // Collect the export names.
  Zone zone(isolate->allocator(), ZONE_NAME);
  UnorderedModuleSet visited(&zone);

  if (module->IsSourceTextModule()) {
    SourceTextModule::FetchStarExports(
        isolate, Handle<SourceTextModule>::cast(module), &zone, &visited);
  }

  Handle<ObjectHashTable> exports(module->exports(), isolate);
  ZoneVector<Handle<String>> names(&zone);
  names.reserve(exports->NumberOfElements());
  for (int i = 0, n = exports->Capacity(); i < n; ++i) {
    Object key;
    if (!exports->ToKey(roots, i, &key)) continue;
    names.push_back(handle(String::cast(key), isolate));
  }
  DCHECK_EQ(static_cast<int>(names.size()), exports->NumberOfElements());

  // Sort them alphabetically.
  std::sort(names.begin(), names.end(),
            [&isolate](Handle<String> a, Handle<String> b) {
              return String::Compare(isolate, a, b) ==
                     ComparisonResult::kLessThan;
            });

  // Create the namespace object (initially empty).
  Handle<JSModuleNamespace> ns = isolate->factory()->NewJSModuleNamespace();
  ns->set_module(*module);
  module->set_module_namespace(*ns);

  // Create the properties in the namespace object. Transition the object
  // to dictionary mode so that property addition is faster.
  PropertyAttributes attr = DONT_DELETE;
  JSObject::NormalizeProperties(isolate, ns, CLEAR_INOBJECT_PROPERTIES,
                                static_cast<int>(names.size()),
                                "JSModuleNamespace");
  for (const auto& name : names) {
    JSObject::SetNormalizedProperty(
        ns, name, Accessors::MakeModuleNamespaceEntryInfo(isolate, name),
        PropertyDetails(kAccessor, attr, PropertyCellType::kMutable));
  }
  JSObject::PreventExtensions(ns, kThrowOnError).ToChecked();

  // Optimize the namespace object as a prototype, for two reasons:
  // - The object's map is guaranteed not to be shared. ICs rely on this.
  // - We can store a pointer from the map back to the namespace object.
  //   Turbofan can use this for inlining the access.
  JSObject::OptimizeAsPrototype(ns);

  Handle<PrototypeInfo> proto_info =
      Map::GetOrCreatePrototypeInfo(Handle<JSObject>::cast(ns), isolate);
  proto_info->set_module_namespace(*ns);
  return ns;
}

MaybeHandle<Object> JSModuleNamespace::GetExport(Isolate* isolate,
                                                 Handle<String> name) {
  Handle<Object> object(module().exports().Lookup(name), isolate);
  if (object->IsTheHole(isolate)) {
    return isolate->factory()->undefined_value();
  }

  Handle<Object> value(Handle<Cell>::cast(object)->value(), isolate);
  if (value->IsTheHole(isolate)) {
    THROW_NEW_ERROR(
        isolate, NewReferenceError(MessageTemplate::kNotDefined, name), Object);
  }

  return value;
}

Maybe<PropertyAttributes> JSModuleNamespace::GetPropertyAttributes(
    LookupIterator* it) {
  Handle<JSModuleNamespace> object = it->GetHolder<JSModuleNamespace>();
  Handle<String> name = Handle<String>::cast(it->GetName());
  DCHECK_EQ(it->state(), LookupIterator::ACCESSOR);

  Isolate* isolate = it->isolate();

  Handle<Object> lookup(object->module().exports().Lookup(name), isolate);
  if (lookup->IsTheHole(isolate)) {
    return Just(ABSENT);
  }

  Handle<Object> value(Handle<Cell>::cast(lookup)->value(), isolate);
  if (value->IsTheHole(isolate)) {
    isolate->Throw(*isolate->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, name));
    return Nothing<PropertyAttributes>();
  }

  return Just(it->property_attributes());
}

}  // namespace internal
}  // namespace v8
