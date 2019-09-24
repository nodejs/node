// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-async-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/objects/js-generator.h"
#include "src/objects/js-promise.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

class AsyncFunctionBuiltinsAssembler : public AsyncBuiltinsAssembler {
 public:
  explicit AsyncFunctionBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : AsyncBuiltinsAssembler(state) {}

 protected:
  template <typename Descriptor>
  void AsyncFunctionAwait(const bool is_predicted_as_caught);

  void AsyncFunctionAwaitResumeClosure(
      Node* const context, Node* const sent_value,
      JSGeneratorObject::ResumeMode resume_mode);
};

void AsyncFunctionBuiltinsAssembler::AsyncFunctionAwaitResumeClosure(
    Node* context, Node* sent_value,
    JSGeneratorObject::ResumeMode resume_mode) {
  DCHECK(resume_mode == JSGeneratorObject::kNext ||
         resume_mode == JSGeneratorObject::kThrow);

  TNode<JSAsyncFunctionObject> async_function_object =
      CAST(LoadContextElement(context, Context::EXTENSION_INDEX));

  // Push the promise for the {async_function_object} back onto the catch
  // prediction stack to handle exceptions thrown after resuming from the
  // await properly.
  Label if_instrumentation(this, Label::kDeferred),
      if_instrumentation_done(this);
  Branch(IsDebugActive(), &if_instrumentation, &if_instrumentation_done);
  BIND(&if_instrumentation);
  {
    TNode<JSPromise> promise = LoadObjectField<JSPromise>(
        async_function_object, JSAsyncFunctionObject::kPromiseOffset);
    CallRuntime(Runtime::kDebugAsyncFunctionResumed, context, promise);
    Goto(&if_instrumentation_done);
  }
  BIND(&if_instrumentation_done);

  // Inline version of GeneratorPrototypeNext / GeneratorPrototypeReturn with
  // unnecessary runtime checks removed.

  // Ensure that the {async_function_object} is neither closed nor running.
  CSA_SLOW_ASSERT(
      this, SmiGreaterThan(
                LoadObjectField<Smi>(async_function_object,
                                     JSGeneratorObject::kContinuationOffset),
                SmiConstant(JSGeneratorObject::kGeneratorClosed)));

  // Remember the {resume_mode} for the {async_function_object}.
  StoreObjectFieldNoWriteBarrier(async_function_object,
                                 JSGeneratorObject::kResumeModeOffset,
                                 SmiConstant(resume_mode));

  // Resume the {receiver} using our trampoline.
  Callable callable = CodeFactory::ResumeGenerator(isolate());
  CallStub(callable, context, sent_value, async_function_object);

  // The resulting Promise is a throwaway, so it doesn't matter what it
  // resolves to. What is important is that we don't end up keeping the
  // whole chain of intermediate Promises alive by returning the return value
  // of ResumeGenerator, as that would create a memory leak.
}

TF_BUILTIN(AsyncFunctionEnter, AsyncFunctionBuiltinsAssembler) {
  TNode<JSFunction> closure = CAST(Parameter(Descriptor::kClosure));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // Compute the number of registers and parameters.
  TNode<SharedFunctionInfo> shared = LoadObjectField<SharedFunctionInfo>(
      closure, JSFunction::kSharedFunctionInfoOffset);
  TNode<IntPtrT> formal_parameter_count = ChangeInt32ToIntPtr(
      LoadObjectField(shared, SharedFunctionInfo::kFormalParameterCountOffset,
                      MachineType::Uint16()));
  TNode<BytecodeArray> bytecode_array =
      LoadSharedFunctionInfoBytecodeArray(shared);
  TNode<IntPtrT> frame_size = ChangeInt32ToIntPtr(LoadObjectField(
      bytecode_array, BytecodeArray::kFrameSizeOffset, MachineType::Int32()));
  TNode<IntPtrT> parameters_and_register_length =
      Signed(IntPtrAdd(WordSar(frame_size, IntPtrConstant(kTaggedSizeLog2)),
                       formal_parameter_count));

  // Allocate and initialize the register file.
  TNode<FixedArrayBase> parameters_and_registers =
      AllocateFixedArray(HOLEY_ELEMENTS, parameters_and_register_length,
                         INTPTR_PARAMETERS, kAllowLargeObjectAllocation);
  FillFixedArrayWithValue(HOLEY_ELEMENTS, parameters_and_registers,
                          IntPtrConstant(0), parameters_and_register_length,
                          RootIndex::kUndefinedValue);

  // Allocate space for the promise, the async function object.
  TNode<IntPtrT> size = IntPtrConstant(JSPromise::kSizeWithEmbedderFields +
                                       JSAsyncFunctionObject::kSize);
  TNode<HeapObject> base = AllocateInNewSpace(size);

  // Initialize the promise.
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<JSFunction> promise_function =
      CAST(LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX));
  TNode<Map> promise_map = LoadObjectField<Map>(
      promise_function, JSFunction::kPrototypeOrInitialMapOffset);
  TNode<JSPromise> promise = UncheckedCast<JSPromise>(
      InnerAllocate(base, JSAsyncFunctionObject::kSize));
  StoreMapNoWriteBarrier(promise, promise_map);
  StoreObjectFieldRoot(promise, JSPromise::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(promise, JSPromise::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  PromiseInit(promise);

  // Initialize the async function object.
  TNode<Map> async_function_object_map = CAST(LoadContextElement(
      native_context, Context::ASYNC_FUNCTION_OBJECT_MAP_INDEX));
  TNode<JSAsyncFunctionObject> async_function_object =
      UncheckedCast<JSAsyncFunctionObject>(base);
  StoreMapNoWriteBarrier(async_function_object, async_function_object_map);
  StoreObjectFieldRoot(async_function_object,
                       JSAsyncFunctionObject::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(async_function_object,
                       JSAsyncFunctionObject::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(
      async_function_object, JSAsyncFunctionObject::kFunctionOffset, closure);
  StoreObjectFieldNoWriteBarrier(
      async_function_object, JSAsyncFunctionObject::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(
      async_function_object, JSAsyncFunctionObject::kReceiverOffset, receiver);
  StoreObjectFieldNoWriteBarrier(async_function_object,
                                 JSAsyncFunctionObject::kInputOrDebugPosOffset,
                                 SmiConstant(0));
  StoreObjectFieldNoWriteBarrier(async_function_object,
                                 JSAsyncFunctionObject::kResumeModeOffset,
                                 SmiConstant(JSAsyncFunctionObject::kNext));
  StoreObjectFieldNoWriteBarrier(
      async_function_object, JSAsyncFunctionObject::kContinuationOffset,
      SmiConstant(JSAsyncFunctionObject::kGeneratorExecuting));
  StoreObjectFieldNoWriteBarrier(
      async_function_object,
      JSAsyncFunctionObject::kParametersAndRegistersOffset,
      parameters_and_registers);
  StoreObjectFieldNoWriteBarrier(
      async_function_object, JSAsyncFunctionObject::kPromiseOffset, promise);

  // Fire promise hooks if enabled and push the Promise under construction
  // in an async function on the catch prediction stack to handle exceptions
  // thrown before the first await.
  Label if_instrumentation(this, Label::kDeferred),
      if_instrumentation_done(this);
  Branch(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_instrumentation, &if_instrumentation_done);
  BIND(&if_instrumentation);
  {
    CallRuntime(Runtime::kDebugAsyncFunctionEntered, context, promise);
    Goto(&if_instrumentation_done);
  }
  BIND(&if_instrumentation_done);

  Return(async_function_object);
}

TF_BUILTIN(AsyncFunctionReject, AsyncFunctionBuiltinsAssembler) {
  TNode<JSAsyncFunctionObject> async_function_object =
      CAST(Parameter(Descriptor::kAsyncFunctionObject));
  TNode<Object> reason = CAST(Parameter(Descriptor::kReason));
  TNode<Oddball> can_suspend = CAST(Parameter(Descriptor::kCanSuspend));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSPromise> promise = LoadObjectField<JSPromise>(
      async_function_object, JSAsyncFunctionObject::kPromiseOffset);

  // Reject the {promise} for the given {reason}, disabling the
  // additional debug event for the rejection since a debug event
  // already happend for the exception that got us here.
  CallBuiltin(Builtins::kRejectPromise, context, promise, reason,
              FalseConstant());

  Label if_debugging(this, Label::kDeferred);
  GotoIf(HasAsyncEventDelegate(), &if_debugging);
  GotoIf(IsDebugActive(), &if_debugging);
  Return(promise);

  BIND(&if_debugging);
  TailCallRuntime(Runtime::kDebugAsyncFunctionFinished, context, can_suspend,
                  promise);
}

TF_BUILTIN(AsyncFunctionResolve, AsyncFunctionBuiltinsAssembler) {
  TNode<JSAsyncFunctionObject> async_function_object =
      CAST(Parameter(Descriptor::kAsyncFunctionObject));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Oddball> can_suspend = CAST(Parameter(Descriptor::kCanSuspend));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSPromise> promise = LoadObjectField<JSPromise>(
      async_function_object, JSAsyncFunctionObject::kPromiseOffset);

  CallBuiltin(Builtins::kResolvePromise, context, promise, value);

  Label if_debugging(this, Label::kDeferred);
  GotoIf(HasAsyncEventDelegate(), &if_debugging);
  GotoIf(IsDebugActive(), &if_debugging);
  Return(promise);

  BIND(&if_debugging);
  TailCallRuntime(Runtime::kDebugAsyncFunctionFinished, context, can_suspend,
                  promise);
}

// AsyncFunctionReject and AsyncFunctionResolve are both required to return
// the promise instead of the result of RejectPromise or ResolvePromise
// respectively from a lazy deoptimization.
TF_BUILTIN(AsyncFunctionLazyDeoptContinuation, AsyncFunctionBuiltinsAssembler) {
  TNode<JSPromise> promise = CAST(Parameter(Descriptor::kPromise));
  Return(promise);
}

TF_BUILTIN(AsyncFunctionAwaitRejectClosure, AsyncFunctionBuiltinsAssembler) {
  CSA_ASSERT_JS_ARGC_EQ(this, 1);
  Node* const sentError = Parameter(Descriptor::kSentError);
  Node* const context = Parameter(Descriptor::kContext);

  AsyncFunctionAwaitResumeClosure(context, sentError,
                                  JSGeneratorObject::kThrow);
  Return(UndefinedConstant());
}

TF_BUILTIN(AsyncFunctionAwaitResolveClosure, AsyncFunctionBuiltinsAssembler) {
  CSA_ASSERT_JS_ARGC_EQ(this, 1);
  Node* const sentValue = Parameter(Descriptor::kSentValue);
  Node* const context = Parameter(Descriptor::kContext);

  AsyncFunctionAwaitResumeClosure(context, sentValue, JSGeneratorObject::kNext);
  Return(UndefinedConstant());
}

// ES#abstract-ops-async-function-await
// AsyncFunctionAwait ( value )
// Shared logic for the core of await. The parser desugars
//   await value
// into
//   yield AsyncFunctionAwait{Caught,Uncaught}(.generator_object, value)
// The 'value' parameter is the value; the .generator_object stands in
// for the asyncContext.
template <typename Descriptor>
void AsyncFunctionBuiltinsAssembler::AsyncFunctionAwait(
    const bool is_predicted_as_caught) {
  TNode<JSAsyncFunctionObject> async_function_object =
      CAST(Parameter(Descriptor::kAsyncFunctionObject));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  TNode<Object> outer_promise = LoadObjectField(
      async_function_object, JSAsyncFunctionObject::kPromiseOffset);

  Label after_debug_hook(this), call_debug_hook(this, Label::kDeferred);
  GotoIf(HasAsyncEventDelegate(), &call_debug_hook);
  Goto(&after_debug_hook);
  BIND(&after_debug_hook);

  Await(context, async_function_object, value, outer_promise,
        Context::ASYNC_FUNCTION_AWAIT_RESOLVE_SHARED_FUN,
        Context::ASYNC_FUNCTION_AWAIT_REJECT_SHARED_FUN,
        is_predicted_as_caught);

  // Return outer promise to avoid adding an load of the outer promise before
  // suspending in BytecodeGenerator.
  Return(outer_promise);

  BIND(&call_debug_hook);
  CallRuntime(Runtime::kDebugAsyncFunctionSuspended, context, outer_promise);
  Goto(&after_debug_hook);
}

// Called by the parser from the desugaring of 'await' when catch
// prediction indicates that there is a locally surrounding catch block.
TF_BUILTIN(AsyncFunctionAwaitCaught, AsyncFunctionBuiltinsAssembler) {
  static const bool kIsPredictedAsCaught = true;
  AsyncFunctionAwait<Descriptor>(kIsPredictedAsCaught);
}

// Called by the parser from the desugaring of 'await' when catch
// prediction indicates no locally surrounding catch block.
TF_BUILTIN(AsyncFunctionAwaitUncaught, AsyncFunctionBuiltinsAssembler) {
  static const bool kIsPredictedAsCaught = false;
  AsyncFunctionAwait<Descriptor>(kIsPredictedAsCaught);
}

}  // namespace internal
}  // namespace v8
