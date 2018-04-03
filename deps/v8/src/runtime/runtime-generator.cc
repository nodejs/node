// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/factory.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CreateJSGeneratorObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 1);
  CHECK(IsResumableFunction(function->shared()->kind()));

  // Underlying function needs to have bytecode available.
  DCHECK(function->shared()->HasBytecodeArray());
  int size = function->shared()->bytecode_array()->register_count();
  Handle<FixedArray> register_file = isolate->factory()->NewFixedArray(size);

  Handle<JSGeneratorObject> generator =
      isolate->factory()->NewJSGeneratorObject(function);
  generator->set_function(*function);
  generator->set_context(isolate->context());
  generator->set_receiver(*receiver);
  generator->set_register_file(*register_file);
  generator->set_continuation(JSGeneratorObject::kGeneratorExecuting);
  return *generator;
}

RUNTIME_FUNCTION(Runtime_GeneratorClose) {
  // Runtime call is implemented in InterpreterIntrinsics and lowered in
  // JSIntrinsicLowering
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->function();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetReceiver) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->receiver();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetContext) {
  // Runtime call is implemented in InterpreterIntrinsics and lowered in
  // JSIntrinsicLowering
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetInputOrDebugPos) {
  // Runtime call is implemented in InterpreterIntrinsics and lowered in
  // JSIntrinsicLowering
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_AsyncGeneratorResolve) {
  // Runtime call is implemented in InterpreterIntrinsics and lowered in
  // JSIntrinsicLowering
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_AsyncGeneratorReject) {
  // Runtime call is implemented in InterpreterIntrinsics and lowered in
  // JSIntrinsicLowering
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_AsyncGeneratorYield) {
  // Runtime call is implemented in InterpreterIntrinsics and lowered in
  // JSIntrinsicLowering
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetResumeMode) {
  // Runtime call is implemented in InterpreterIntrinsics and lowered in
  // JSIntrinsicLowering
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetContinuation) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return Smi::FromInt(generator->continuation());
}

RUNTIME_FUNCTION(Runtime_GeneratorGetSourcePosition) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  if (!generator->is_suspended()) return isolate->heap()->undefined_value();
  return Smi::FromInt(generator->source_position());
}

// Return true if {generator}'s PC has a catch handler. This allows
// catch prediction to happen from the AsyncGeneratorResumeNext stub.
RUNTIME_FUNCTION(Runtime_AsyncGeneratorHasCatchHandlerForPC) {
  DisallowHeapAllocation no_allocation_scope;
  DCHECK_EQ(1, args.length());
  DCHECK(args[0]->IsJSAsyncGeneratorObject());
  JSAsyncGeneratorObject* generator = JSAsyncGeneratorObject::cast(args[0]);

  int state = generator->continuation();
  DCHECK_NE(state, JSAsyncGeneratorObject::kGeneratorExecuting);

  // If state is 0 ("suspendedStart"), there is guaranteed to be no catch
  // handler. Otherwise, if state is below 0, the generator is closed and will
  // not reach a catch handler.
  if (state < 1) return isolate->heap()->false_value();

  SharedFunctionInfo* shared = generator->function()->shared();
  DCHECK(shared->HasBytecodeArray());
  HandlerTable* handler_table =
      HandlerTable::cast(shared->bytecode_array()->handler_table());

  int pc = Smi::cast(generator->input_or_debug_pos())->value();
  HandlerTable::CatchPrediction catch_prediction = HandlerTable::ASYNC_AWAIT;
  handler_table->LookupRange(pc, nullptr, &catch_prediction);
  return isolate->heap()->ToBoolean(catch_prediction == HandlerTable::CAUGHT);
}

}  // namespace internal
}  // namespace v8
