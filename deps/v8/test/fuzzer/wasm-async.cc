// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "src/api.h"
#include "src/heap/factory.h"
#include "src/isolate-inl.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

namespace v8 {
namespace internal {
class WasmModuleObject;

namespace wasm {
namespace fuzzer {

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

namespace {
// We need this helper function because we cannot use
// Handle<WasmModuleObject>::cast here. To use this function we would have to
// mark it with V8_EXPORT_PRIVATE, which is quite ugly in this case.
Handle<WasmModuleObject> ToWasmModuleObjectUnchecked(Handle<Object> that) {
  return handle(reinterpret_cast<WasmModuleObject*>(*that));
}
}

void InstantiateCallback(const FunctionCallbackInfo<Value>& args) {
  DCHECK_GE(args.Length(), 1);
  v8::Isolate* isolate = args.GetIsolate();
  MicrotasksScope does_not_run_microtasks(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::HandleScope scope(isolate);

  Local<v8::Value> module = args[0];

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  Handle<WasmModuleObject> module_obj =
      ToWasmModuleObjectUnchecked(Utils::OpenHandle(v8::Object::Cast(*module)));
  InterpretAndExecuteModule(i_isolate, module_obj);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FlagScope<bool> turn_on_async_compile(
      &v8::internal::FLAG_wasm_async_compilation, true);
  FlagScope<uint32_t> max_mem_flag_scope(&v8::internal::FLAG_wasm_max_mem_pages,
                                         32);
  FlagScope<uint32_t> max_table_size_scope(
      &v8::internal::FLAG_wasm_max_table_size, 100);
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<v8::internal::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  i::HandleScope internal_scope(i_isolate);
  v8::Context::Scope context_scope(support->GetContext());
  TryCatch try_catch(isolate);
  testing::SetupIsolateForWasmModule(i_isolate);

  // Get the promise for async compilation.
  ASSIGN(Promise::Resolver, resolver,
         Promise::Resolver::New(support->GetContext()));
  Local<Promise> promise = resolver->GetPromise();

  i_isolate->wasm_engine()->AsyncCompile(i_isolate, Utils::OpenHandle(*promise),
                                         ModuleWireBytes(data, data + size),
                                         false);

  ASSIGN(Function, instantiate_impl,
         Function::New(support->GetContext(), &InstantiateCallback,
                       Undefined(isolate)));

  ASSIGN(Promise, result,
         promise->Then(support->GetContext(), instantiate_impl));

  // Wait for the promise to resolve.
  while (result->State() == Promise::kPending) {
    support->PumpMessageLoop(platform::MessageLoopBehavior::kWaitForWork);
    isolate->RunMicrotasks();
  }
  return 0;
}

#undef ASSIGN

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
