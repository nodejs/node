// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/synthetic-module.h"

#include "src/api/api-inl.h"
#include "src/base/macros.h"
#include "src/builtins/accessors.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/synthetic-module-inl.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

// Implements SetSyntheticModuleBinding:
// https://heycam.github.io/webidl/#setsyntheticmoduleexport
Maybe<bool> SyntheticModule::SetExport(Isolate* isolate,
                                       DirectHandle<SyntheticModule> module,
                                       DirectHandle<String> export_name,
                                       DirectHandle<Object> export_value) {
  DirectHandle<ObjectHashTable> exports(module->exports(), isolate);
  DirectHandle<Object> export_object(exports->Lookup(export_name), isolate);

  if (!IsCell(*export_object)) {
    isolate->Throw(*isolate->factory()->NewReferenceError(
        MessageTemplate::kModuleExportUndefined, export_name));
    return Nothing<bool>();
  }

  // Spec step 2: Set the mutable binding of export_name to export_value
  Cast<Cell>(*export_object)->set_value(*export_value);

  return Just(true);
}

void SyntheticModule::SetExportStrict(Isolate* isolate,
                                      DirectHandle<SyntheticModule> module,
                                      DirectHandle<String> export_name,
                                      DirectHandle<Object> export_value) {
  DirectHandle<ObjectHashTable> exports(module->exports(), isolate);
  DirectHandle<Object> export_object(exports->Lookup(export_name), isolate);
  CHECK(IsCell(*export_object));
  Maybe<bool> set_export_result =
      SetExport(isolate, module, export_name, export_value);
  CHECK(set_export_result.FromJust());
}

// Implements Synthetic Module Record's ResolveExport concrete method:
// https://heycam.github.io/webidl/#smr-resolveexport
MaybeHandle<Cell> SyntheticModule::ResolveExport(
    Isolate* isolate, DirectHandle<SyntheticModule> module,
    DirectHandle<String> module_specifier, DirectHandle<String> export_name,
    MessageLocation loc, bool must_resolve) {
  Handle<Object> object(module->exports()->Lookup(export_name), isolate);
  if (IsCell(*object)) return Cast<Cell>(object);

  if (!must_resolve) return kNullMaybeHandle;

  isolate->ThrowAt(
      isolate->factory()->NewSyntaxError(MessageTemplate::kUnresolvableExport,
                                         module_specifier, export_name),
      &loc);
  return kNullMaybeHandle;
}

// Implements Synthetic Module Record's Instantiate concrete method :
// https://heycam.github.io/webidl/#smr-instantiate
bool SyntheticModule::PrepareInstantiate(Isolate* isolate,
                                         DirectHandle<SyntheticModule> module,
                                         v8::Local<v8::Context> context) {
  Handle<ObjectHashTable> exports(module->exports(), isolate);
  DirectHandle<FixedArray> export_names(module->export_names(), isolate);
  // Spec step 7: For each export_name in module->export_names...
  const uint32_t export_names_len = export_names->ulength().value();
  for (uint32_t i = 0; i < export_names_len; ++i) {
    // Spec step 7.1: Create a new mutable binding for export_name.
    // Spec step 7.2: Initialize the new mutable binding to undefined.
    DirectHandle<Cell> cell = isolate->factory()->NewCell();
    DirectHandle<String> name(Cast<String>(export_names->get(i)), isolate);
    CHECK(IsTheHole(exports->Lookup(name)));
    exports = ObjectHashTable::Put(isolate, exports, name, cell);
  }
  module->set_exports(*exports);
  return true;
}

// Second step of module instantiation.  No real work to do for SyntheticModule
// as there are no imports or indirect exports to resolve;
// just update status.
bool SyntheticModule::FinishInstantiate(Isolate* isolate,
                                        DirectHandle<SyntheticModule> module) {
  module->SetStatus(kLinked);

  // Ensure that if the namespace binding was created it is not empty.
  if (!IsUndefined(module->module_namespace())) {
    Module::GetModuleNamespace(isolate, handle(*module, isolate));
    DCHECK(!IsUndefined(Cast<Cell>(module->module_namespace())->value()));
  }

  return true;
}

// Implements Synthetic Module Record's Evaluate concrete method:
// https://heycam.github.io/webidl/#smr-evaluate
// The callback may have been created through the deprecated
// v8::Module::LegacySyntheticModuleEvaluationSteps overload, in which case it
// actually returns a v8::MaybeLocal<v8::Value> and is called here through a
// mismatching signature. Both return types are pointer-sized, trivially
// copyable handle wrappers, so this is safe in practice, but it does trip
// CFI's and UBSan's indirect call checks.
// TODO(https://crbug.com/545375591): Remove DISABLE_CFI_ICALL once the
// deprecated overload is gone.
DISABLE_CFI_ICALL
MaybeDirectHandle<JSPromise> SyntheticModule::Evaluate(
    Isolate* isolate, DirectHandle<SyntheticModule> module) {
  module->SetStatus(kEvaluating);

  v8::Module::SyntheticModuleEvaluationSteps evaluation_steps =
      FUNCTION_CAST<v8::Module::SyntheticModuleEvaluationSteps>(
          module->evaluation_steps()->foreign_address<kSyntheticModuleTag>());
  // Deliberately received as a v8::Local<v8::Value>: the deprecated callback
  // signature only promises a Promise, it doesn't guarantee one.
  v8::Local<v8::Value> result;
  if (!evaluation_steps(Utils::ToLocal(isolate->native_context()),
                        Utils::ToLocal(Cast<Module>(module)))
           .ToLocal(&result)) {
    module->RecordError(isolate, isolate->exception());
    return MaybeDirectHandle<JSPromise>();
  }

  module->SetStatus(kEvaluated);

  DirectHandle<Object> result_from_callback = Utils::OpenDirectHandle(*result);
  CHECK(IsJSPromise(*result_from_callback));
  DirectHandle<JSPromise> capability = Cast<JSPromise>(result_from_callback);
  module->set_top_level_capability(*capability);
  return capability;
}

}  // namespace internal
}  // namespace v8
