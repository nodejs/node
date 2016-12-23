// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/assembler.h"
#include "src/compiler/wasm-compiler.h"
#include "src/conversions.h"
#include "src/debug/debug.h"
#include "src/factory.h"
#include "src/frames-inl.h"
#include "src/objects-inl.h"
#include "src/v8memory.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_WasmMemorySize) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());

  Handle<JSObject> module_instance;
  {
    // Get the module JSObject
    DisallowHeapAllocation no_allocation;
    const Address entry = Isolate::c_entry_fp(isolate->thread_local_top());
    Address pc =
        Memory::Address_at(entry + StandardFrameConstants::kCallerPCOffset);
    Code* code =
        isolate->inner_pointer_to_code_cache()->GetCacheEntry(pc)->code;
    Object* owning_instance = wasm::GetOwningWasmInstance(code);
    CHECK_NOT_NULL(owning_instance);
    module_instance = handle(JSObject::cast(owning_instance), isolate);
  }
  return *isolate->factory()->NewNumberFromInt(
      wasm::GetInstanceMemorySize(isolate, module_instance));
}

RUNTIME_FUNCTION(Runtime_WasmGrowMemory) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_UINT32_ARG_CHECKED(delta_pages, 0);
  Handle<JSObject> module_instance;
  {
    // Get the module JSObject
    DisallowHeapAllocation no_allocation;
    const Address entry = Isolate::c_entry_fp(isolate->thread_local_top());
    Address pc =
        Memory::Address_at(entry + StandardFrameConstants::kCallerPCOffset);
    Code* code =
        isolate->inner_pointer_to_code_cache()->GetCacheEntry(pc)->code;
    Object* owning_instance = wasm::GetOwningWasmInstance(code);
    CHECK_NOT_NULL(owning_instance);
    module_instance = handle(JSObject::cast(owning_instance), isolate);
  }
  return *isolate->factory()->NewNumberFromInt(
      wasm::GrowInstanceMemory(isolate, module_instance, delta_pages));
}

RUNTIME_FUNCTION(Runtime_WasmThrowTypeError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kWasmTrapTypeError));
}

RUNTIME_FUNCTION(Runtime_WasmThrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(lower, 0);
  CONVERT_SMI_ARG_CHECKED(upper, 1);

  const int32_t thrown_value = (upper << 16) | lower;

  return isolate->Throw(*isolate->factory()->NewNumberFromInt(thrown_value));
}

RUNTIME_FUNCTION(Runtime_WasmGetCaughtExceptionValue) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Object* exception = args[0];
  // The unwinder will only deliver exceptions to wasm if the exception is a
  // Number or a Smi (which we have just converted to a Number.) This logic
  // lives in Isolate::is_catchable_by_wasm(Object*).
  CHECK(exception->IsNumber());
  return exception;
}

}  // namespace internal
}  // namespace v8
