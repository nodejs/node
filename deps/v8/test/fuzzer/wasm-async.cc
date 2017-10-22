// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/wasm/wasm-api.h"
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

#if __clang__
// TODO(mostynb@opera.com): remove the using statements and these pragmas.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wheader-hygiene"
#endif

using namespace v8::internal;
using namespace v8::internal::wasm;
using namespace v8::internal::wasm::testing;

#if __clang__
// TODO(mostynb@opera.com): remove the using statements and these pragmas.
#pragma clang diagnostic pop
#endif

#define ASSIGN(type, var, expr)                      \
  v8::Local<type> var;                               \
  do {                                               \
    if (!expr.ToLocal(&var)) {                       \
      DCHECK(i_isolate->has_scheduled_exception());  \
      return 0;                                      \
    } else {                                         \
      DCHECK(!i_isolate->has_scheduled_exception()); \
    }                                                \
  } while (false)

namespace v8 {
namespace internal {
class WasmModuleObject;
}
}

namespace {
// We need this helper function because we cannot use
// Handle<WasmModuleObject>::cast here. To use this function we would have to
// mark it with V8_EXPORT_PRIVATE, which is quite ugly in this case.
Handle<WasmModuleObject> ToWasmModuleObjectUnchecked(Handle<Object> that) {
  return handle(reinterpret_cast<WasmModuleObject*>(*that));
}
}

void InstantiateCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_GE(args.Length(), 1);
  v8::Isolate* isolate = args.GetIsolate();
  v8::MicrotasksScope does_not_run_microtasks(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::HandleScope scope(isolate);

  v8::Local<v8::Value> module = args[0];

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  ScheduledErrorThrower thrower(i_isolate, "WebAssembly Instantiation");

  i::Handle<i::WasmModuleObject> module_obj = ToWasmModuleObjectUnchecked(
      v8::Utils::OpenHandle(v8::Object::Cast(*module)));
  i::MaybeHandle<WasmInstanceObject> maybe_instance =
      i::wasm::SyncInstantiate(i_isolate, &thrower, module_obj,
                               Handle<JSReceiver>::null(),     // imports
                               MaybeHandle<JSArrayBuffer>());  // memory
  Handle<WasmInstanceObject> instance;
  if (!maybe_instance.ToHandle(&instance)) {
    return;
  }
  RunWasmModuleForTesting(i_isolate, instance, 0, nullptr);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  v8::internal::FlagScope<bool> turn_on_async_compile(
      &v8::internal::FLAG_wasm_async_compilation, true);
  v8::internal::FlagScope<uint32_t> max_mem_flag_scope(
      &v8::internal::FLAG_wasm_max_mem_pages, 32);
  v8::internal::FlagScope<uint32_t> max_table_size_scope(
      &v8::internal::FLAG_wasm_max_table_size, 100);
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  v8::internal::Isolate* i_isolate =
      reinterpret_cast<v8::internal::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  HandleScope internal_scope(i_isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);
  v8::internal::wasm::testing::SetupIsolateForWasmModule(i_isolate);

  // Get the promise for async compilation.
  ASSIGN(v8::Promise::Resolver, resolver,
         v8::Promise::Resolver::New(support->GetContext()));
  v8::Local<v8::Promise> promise = resolver->GetPromise();

  AsyncCompile(i_isolate, v8::Utils::OpenHandle(*promise),
               ModuleWireBytes(data, data + size));

  ASSIGN(v8::Function, instantiate_impl,
         v8::Function::New(support->GetContext(), &InstantiateCallback,
                           v8::Undefined(isolate)));

  ASSIGN(v8::Promise, result,
         promise->Then(support->GetContext(), instantiate_impl));

  // Wait for the promise to resolve.
  while (result->State() == v8::Promise::kPending) {
    support->PumpMessageLoop(v8::platform::MessageLoopBehavior::kWaitForWork);
    isolate->RunMicrotasks();
  }
  return 0;
}
