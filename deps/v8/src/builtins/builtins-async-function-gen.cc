// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-async-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler-inl.h"
#include "src/objects/js-generator.h"
#include "src/objects/js-promise.h"
#include "src/objects/microtask.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

#include "src/codegen/define-code-stub-assembler-macros.inc"

class AsyncFunctionBuiltinsAssembler : public AsyncBuiltinsAssembler {
 public:
  explicit AsyncFunctionBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : AsyncBuiltinsAssembler(state) {}

 protected:
  template <typename Descriptor>
  void AsyncFunctionAwait();

  void AsyncFunctionAwaitResumeClosure(
      const TNode<Context> context, const TNode<Object> sent_value,
      JSGeneratorObject::ResumeMode resume_mode);
};

void AsyncFunctionBuiltinsAssembler::AsyncFunctionAwaitResumeClosure(
    TNode<Context> context, TNode<Object> sent_value,
    JSGeneratorObject::ResumeMode resume_mode) {
  DCHECK(resume_mode == JSGeneratorObject::kNext ||
         resume_mode == JSGeneratorObject::kThrow);

  TNode<JSAsyncFunctionObject> async_function_object =
      CAST(LoadContextElementNoCell(context, Context::EXTENSION_INDEX));

  // Inline version of GeneratorPrototypeNext / GeneratorPrototypeReturn with
  // unnecessary runtime checks removed.

  // Ensure that the {async_function_object} is neither closed nor running.
  CSA_SLOW_DCHECK(
      this, SmiGreaterThan(LoadObjectField<Smi>(
                               async_function_object,
                               offsetof(JSGeneratorObject, continuation_)),
                           SmiConstant(JSGeneratorObject::kGeneratorClosed)));

  // Remember the {resume_mode} for the {async_function_object}.
  StoreObjectFieldNoWriteBarrier(async_function_object,
                                 offsetof(JSGeneratorObject, resume_mode_),
                                 SmiConstant(resume_mode));

  // Resume the {receiver} using our trampoline.
  CallBuiltin(Builtin::kResumeGeneratorTrampoline, context, sent_value,
              async_function_object);

  // The resulting Promise is a throwaway, so it doesn't matter what it
  // resolves to. What is important is that we don't end up keeping the
  // whole chain of intermediate Promises alive by returning the return value
  // of ResumeGenerator, as that would create a memory leak.
}

TF_BUILTIN(AsyncFunctionEnter, AsyncFunctionBuiltinsAssembler) {
  auto closure = Parameter<JSFunction>(Descriptor::kClosure);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  auto context = Parameter<Context>(Descriptor::kContext);

  // Compute the number of registers and parameters.
  TNode<SharedFunctionInfo> shared = LoadObjectField<SharedFunctionInfo>(
      closure, JSFunction::kSharedFunctionInfoOffset);
  TNode<IntPtrT> formal_parameter_count = ChangeInt32ToIntPtr(
      LoadSharedFunctionInfoFormalParameterCountWithoutReceiver(shared));
  TNode<BytecodeArray> bytecode_array =
      LoadSharedFunctionInfoBytecodeArray(shared);
  TNode<IntPtrT> frame_size = ChangeInt32ToIntPtr(LoadObjectField<Uint32T>(
      bytecode_array, BytecodeArray::kFrameSizeOffset));
  TNode<IntPtrT> parameters_and_register_length =
      Signed(IntPtrAdd(WordSar(frame_size, IntPtrConstant(kTaggedSizeLog2)),
                       formal_parameter_count));

  // Allocate and initialize the register file.
  TNode<FixedArrayBase> parameters_and_registers =
      AllocateFixedArray(HOLEY_ELEMENTS, parameters_and_register_length);
  FillFixedArrayWithValue(HOLEY_ELEMENTS, parameters_and_registers,
                          IntPtrConstant(0), parameters_and_register_length,
                          RootIndex::kUndefinedValue);

  // Allocate and initialize the promise.
  TNode<JSPromise> promise = NewJSPromise(context);

  // Allocate and initialize the async function object.
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Map> async_function_object_map = CAST(LoadContextElementNoCell(
      native_context, Context::ASYNC_FUNCTION_OBJECT_MAP_INDEX));
  TNode<JSAsyncFunctionObject> async_function_object =
      UncheckedCast<JSAsyncFunctionObject>(
          AllocateInNewSpace(sizeof(JSAsyncFunctionObject)));
  StoreMapNoWriteBarrier(async_function_object, async_function_object_map);
  StoreObjectFieldRoot(async_function_object,
                       offsetof(JSAsyncFunctionObject, properties_or_hash_),
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(async_function_object,
                       offsetof(JSAsyncFunctionObject, elements_),
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(async_function_object,
                                 offsetof(JSAsyncFunctionObject, function_),
                                 closure);
  StoreObjectFieldNoWriteBarrier(async_function_object,
                                 offsetof(JSAsyncFunctionObject, context_),
                                 context);
  StoreObjectFieldNoWriteBarrier(async_function_object,
                                 offsetof(JSAsyncFunctionObject, receiver_),
                                 receiver);
  StoreObjectFieldNoWriteBarrier(
      async_function_object,
      offsetof(JSAsyncFunctionObject, input_or_debug_pos_), SmiConstant(0));
  StoreObjectFieldNoWriteBarrier(async_function_object,
                                 offsetof(JSAsyncFunctionObject, resume_mode_),
                                 SmiConstant(JSAsyncFunctionObject::kNext));
  StoreObjectFieldNoWriteBarrier(
      async_function_object, offsetof(JSAsyncFunctionObject, continuation_),
      SmiConstant(JSAsyncFunctionObject::kGeneratorExecuting));
  StoreObjectFieldNoWriteBarrier(
      async_function_object,
      offsetof(JSAsyncFunctionObject, parameters_and_registers_),
      parameters_and_registers);
  StoreObjectFieldNoWriteBarrier(async_function_object,
                                 offsetof(JSAsyncFunctionObject, promise_),
                                 promise);

  // Initialize closure fields to undefined. They will be lazily allocated
  // on first await. This saves memory for async functions that never suspend
  // (e.g., conditional awaits, early returns).
  StoreObjectFieldRoot(async_function_object,
                       offsetof(JSAsyncFunctionObject, await_resolve_closure_),
                       RootIndex::kUndefinedValue);
  StoreObjectFieldRoot(async_function_object,
                       offsetof(JSAsyncFunctionObject, await_reject_closure_),
                       RootIndex::kUndefinedValue);

  Return(async_function_object);
}

TF_BUILTIN(AsyncFunctionReject, AsyncFunctionBuiltinsAssembler) {
  auto async_function_object =
      Parameter<JSAsyncFunctionObject>(Descriptor::kAsyncFunctionObject);
  auto reason = Parameter<Object>(Descriptor::kReason);
  auto context = Parameter<Context>(Descriptor::kContext);
  TNode<JSPromise> promise = LoadObjectField<JSPromise>(
      async_function_object, offsetof(JSAsyncFunctionObject, promise_));

  // Reject the {promise} for the given {reason}, disabling the
  // additional debug event for the rejection since a debug event
  // already happend for the exception that got us here.
  CallBuiltin(Builtin::kRejectPromise, context, promise, reason,
              FalseConstant());

  Return(promise);
}

TF_BUILTIN(AsyncFunctionResolve, AsyncFunctionBuiltinsAssembler) {
  auto async_function_object =
      Parameter<JSAsyncFunctionObject>(Descriptor::kAsyncFunctionObject);
  auto value = Parameter<Object>(Descriptor::kValue);
  auto context = Parameter<Context>(Descriptor::kContext);
  TNode<JSPromise> promise = LoadObjectField<JSPromise>(
      async_function_object, offsetof(JSAsyncFunctionObject, promise_));

  CallBuiltin(Builtin::kResolvePromise, context, promise, value);

  Return(promise);
}

// AsyncFunctionReject and AsyncFunctionResolve are both required to return
// the promise instead of the result of RejectPromise or ResolvePromise
// respectively from a lazy deoptimization.
TF_BUILTIN(AsyncFunctionLazyDeoptContinuation, AsyncFunctionBuiltinsAssembler) {
  auto promise = Parameter<JSPromise>(Descriptor::kPromise);
  Return(promise);
}

TF_BUILTIN(AsyncFunctionAwaitRejectClosure, AsyncFunctionBuiltinsAssembler) {
  CSA_DCHECK_JS_ARGC_EQ(this, 1);
  const auto sentError = Parameter<Object>(Descriptor::kSentError);
  const auto context = Parameter<Context>(Descriptor::kContext);

  AsyncFunctionAwaitResumeClosure(context, sentError,
                                  JSGeneratorObject::kThrow);
  Return(UndefinedConstant());
}

TF_BUILTIN(AsyncFunctionAwaitResolveClosure, AsyncFunctionBuiltinsAssembler) {
  CSA_DCHECK_JS_ARGC_EQ(this, 1);
  const auto sentValue = Parameter<Object>(Descriptor::kSentValue);
  const auto context = Parameter<Context>(Descriptor::kContext);

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
void AsyncFunctionBuiltinsAssembler::AsyncFunctionAwait() {
  auto async_function_object =
      Parameter<JSAsyncFunctionObject>(Descriptor::kAsyncFunctionObject);
  auto value = Parameter<JSAny>(Descriptor::kValue);
  auto context = Parameter<Context>(Descriptor::kContext);

  TNode<JSPromise> outer_promise = LoadObjectField<JSPromise>(
      async_function_object, offsetof(JSAsyncFunctionObject, promise_));

  // Fast path: if value is an already-fulfilled JSPromise with intact species
  // protector and no hooks/debug active, skip closures entirely and enqueue
  // an AsyncResumeTask directly.  This eliminates the AwaitContext, closure
  // allocation, PromiseFulfillReactionJobTask allocation, and the indirect
  // closure dispatch (AsyncFunctionAwaitResolveClosure).
  Label slow_path(this);
  {
    // Must be a heap object.
    GotoIf(TaggedIsSmi(value), &slow_path);
    TNode<HeapObject> value_obj = CAST(value);

    // No hooks or debug support active.
    // TODO(jgruber): fold the species-protector bit into PromiseHookFlags()
    // so both checks can be combined into a single load.
    TNode<Uint32T> promiseHookFlags = PromiseHookFlags();
    GotoIf(IsIsolatePromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(
               promiseHookFlags),
           &slow_path);
#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
    GotoIf(IsContextPromiseHookEnabled(promiseHookFlags), &slow_path);
#endif

    // value must be a JSPromise on the canonical initial map for this
    // native context.  Checking the initial map (rather than just the
    // instance type) guarantees:
    //   - no custom "constructor" property on the instance, and
    //   - the promise was created in the current native context (initial
    //     maps are per-context), so microtasks enqueue on the right queue.
    TNode<NativeContext> native_context = LoadNativeContext(context);
    TNode<Map> value_map = LoadMap(value_obj);
    TNode<JSFunction> promise_fun = CAST(LoadContextElementNoCell(
        native_context, Context::PROMISE_FUNCTION_INDEX));
    TNode<Map> initial_promise_map =
        CAST(LoadJSFunctionPrototypeOrInitialMap(promise_fun));
    GotoIfNot(TaggedEqual(value_map, initial_promise_map), &slow_path);
    TNode<JSPromise> js_promise = CAST(value_obj);

    // Promise must be fulfilled.
    TNode<Smi> promise_flags =
        LoadObjectField<Smi>(js_promise, offsetof(JSPromise, flags_));
    GotoIfNot(IsSetSmi(promise_flags, v8::Promise::kFulfilled), &slow_path);

    // Species protector must be intact.  Together with the initial map check
    // above this ensures the "constructor" lookup resolves to %Promise%, so
    // PromiseResolve(Promise, value) returns value unchanged.
    GotoIf(IsPromiseSpeciesProtectorCellInvalid(), &slow_path);

    // Mark the promise as handled.
    StoreObjectFieldNoWriteBarrier(
        js_promise, offsetof(JSPromise, flags_),
        SmiOr(promise_flags, SmiConstant(JSPromise::HasHandlerBit::kMask)));

    // Extract the fulfilled value.
    TNode<Object> fulfilled_value =
        LoadObjectField(js_promise, offsetof(JSPromise, reactions_or_result_));

    EnqueueAsyncResumeTask(native_context, async_function_object,
                           fulfilled_value,
                           AsyncResumeTask::kAsyncFunctionAwait);
    Return(outer_promise);
  }

  BIND(&slow_path);
  AwaitWithReusableClosures(context, async_function_object, value,
                            outer_promise);

  // Return outer promise to avoid adding a load of the outer promise before
  // suspending in BytecodeGenerator.
  Return(outer_promise);
}

// Called by the parser from the desugaring of 'await'.
TF_BUILTIN(AsyncFunctionAwait, AsyncFunctionBuiltinsAssembler) {
  AsyncFunctionAwait<Descriptor>();
}

#include "src/codegen/undef-code-stub-assembler-macros.inc"

}  // namespace internal
}  // namespace v8
