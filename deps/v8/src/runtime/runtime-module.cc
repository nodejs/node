// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/counters.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_DynamicImportCall) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, specifier, 1);

  Handle<JSPromise> promise = isolate->factory()->NewJSPromise();

  Handle<String> specifier_str;
  MaybeHandle<String> maybe_specifier = Object::ToString(isolate, specifier);
  if (!maybe_specifier.ToHandle(&specifier_str)) {
    DCHECK(isolate->has_pending_exception());
    Handle<Object> reason(isolate->pending_exception(), isolate);
    isolate->clear_pending_exception();

    Handle<Object> argv[] = {promise, reason,
                             isolate->factory()->ToBoolean(false)};

    RETURN_FAILURE_ON_EXCEPTION(
        isolate, Execution::Call(isolate, isolate->promise_internal_reject(),
                                 isolate->factory()->undefined_value(),
                                 arraysize(argv), argv))
    return *promise;
  }
  DCHECK(!isolate->has_pending_exception());

  Handle<Script> script(Script::cast(function->shared()->script()));
  Handle<String> source_url(String::cast(script->name()));

  isolate->RunHostImportModuleDynamicallyCallback(source_url, specifier_str,
                                                  promise);
  return *promise;
}

RUNTIME_FUNCTION(Runtime_GetModuleNamespace) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(module_request, 0);
  Handle<Module> module(isolate->context()->module());
  return *Module::GetModuleNamespace(module, module_request);
}

RUNTIME_FUNCTION(Runtime_LoadModuleVariable) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(index, 0);
  Handle<Module> module(isolate->context()->module());
  return *Module::LoadVariable(module, index);
}

RUNTIME_FUNCTION(Runtime_StoreModuleVariable) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  Handle<Module> module(isolate->context()->module());
  Module::StoreVariable(module, index, value);
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8
