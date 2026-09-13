// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/code-stub-assembler-inl.h"
#include "src/execution/microtask-queue.h"
#include "src/handles/handle-scope-implementer.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/promise.h"
#include "src/objects/smi-inl.h"

namespace v8 {
namespace internal {

#include "src/codegen/define-code-stub-assembler-macros.inc"

using compiler::ScopedExceptionHandler;

class MicrotaskQueueBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit MicrotaskQueueBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<RawPtrT> GetMicrotaskQueue(TNode<Context> context);
  TNode<RawPtrT> GetMicrotaskRingBuffer(TNode<RawPtrT> microtask_queue);
  TNode<IntPtrT> GetMicrotaskQueueCapacity(TNode<RawPtrT> microtask_queue);
  TNode<IntPtrT> GetMicrotaskQueueSize(TNode<RawPtrT> microtask_queue);
  void SetMicrotaskQueueSize(TNode<RawPtrT> microtask_queue,
                             TNode<IntPtrT> new_size);
  TNode<IntPtrT> GetMicrotaskQueueStart(TNode<RawPtrT> microtask_queue);
  void SetMicrotaskQueueStart(TNode<RawPtrT> microtask_queue,
                              TNode<IntPtrT> new_start);
  TNode<IntPtrT> CalculateRingBufferOffset(TNode<IntPtrT> capacity,
                                           TNode<IntPtrT> start,
                                           TNode<IntPtrT> index);

  TNode<IntPtrT> CalculateRingBufferStartOffsetWithoutCapacityMask(
      TNode<IntPtrT> start);

  void PrepareForContext(TNode<Context> microtask_context,
                         TNode<IntPtrT> baseline_entered_context_count,
                         Label* bailout);
  void RunSingleMicrotask(TNode<Context> current_context,
                          TNode<IntPtrT> baseline_entered_context_count,
                          TNode<Microtask> microtask);
  void IncrementFinishedMicrotaskCount(TNode<RawPtrT> microtask_queue);

  TNode<Context> GetCurrentContext();
  void SetCurrentContext(TNode<Context> context);

  TNode<IntPtrT> GetEnteredContextCount();
  void EnterContext(TNode<Context> native_context);
  void RewindEnteredContext(TNode<IntPtrT> saved_entered_context_count);

  void RunAllPromiseHooks(PromiseHookType type, TNode<Context> context,
                          TNode<HeapObject> promise_or_capability);
  void RunPromiseHook(Runtime::FunctionId id, TNode<Context> context,
                      TNode<HeapObject> promise_or_capability,
                      TNode<Uint32T> promiseHookFlags);
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  void SetupContinuationPreservedEmbedderData(TNode<Microtask> microtask);
  void ClearContinuationPreservedEmbedderData();
#endif
};

TNode<RawPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueue(
    TNode<Context> native_context) {
  CSA_DCHECK(this, IsNativeContext(native_context));
#ifdef V8_CPPGC_MICROTASK_QUEUE
  return LoadCppHeapPointerFromObject(native_context,
                                      NativeContext::kMicrotaskQueueOffset,
                                      CppHeapPointerTag::kMicrotaskQueueTag);
#else
  return LoadExternalPointerFromObject(native_context,
                                       NativeContext::kMicrotaskQueueOffset,
                                       kNativeContextMicrotaskQueueTag);
#endif
}

TNode<RawPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskRingBuffer(
    TNode<RawPtrT> microtask_queue) {
  return Load<RawPtrT>(microtask_queue,
                       IntPtrConstant(MicrotaskQueue::kRingBufferOffset));
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueueCapacity(
    TNode<RawPtrT> microtask_queue) {
  return Load<IntPtrT>(microtask_queue,
                       IntPtrConstant(MicrotaskQueue::kCapacityOffset));
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueueSize(
    TNode<RawPtrT> microtask_queue) {
  return Load<IntPtrT>(microtask_queue,
                       IntPtrConstant(MicrotaskQueue::kSizeOffset));
}

void MicrotaskQueueBuiltinsAssembler::SetMicrotaskQueueSize(
    TNode<RawPtrT> microtask_queue, TNode<IntPtrT> new_size) {
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), microtask_queue,
                      IntPtrConstant(MicrotaskQueue::kSizeOffset), new_size);
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueueStart(
    TNode<RawPtrT> microtask_queue) {
  return Load<IntPtrT>(microtask_queue,
                       IntPtrConstant(MicrotaskQueue::kStartOffset));
}

void MicrotaskQueueBuiltinsAssembler::SetMicrotaskQueueStart(
    TNode<RawPtrT> microtask_queue, TNode<IntPtrT> new_start) {
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), microtask_queue,
                      IntPtrConstant(MicrotaskQueue::kStartOffset), new_start);
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::CalculateRingBufferOffset(
    TNode<IntPtrT> capacity, TNode<IntPtrT> start, TNode<IntPtrT> index) {
  return TimesSystemPointerSize(
      WordAnd(IntPtrAdd(start, index), IntPtrSub(capacity, IntPtrConstant(1))));
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::
    CalculateRingBufferStartOffsetWithoutCapacityMask(TNode<IntPtrT> start) {
  return TimesSystemPointerSize(start);
}

void MicrotaskQueueBuiltinsAssembler::PrepareForContext(
    TNode<Context> native_context,
    TNode<IntPtrT> baseline_entered_context_count, Label* bailout) {
  CSA_DCHECK(this, IsNativeContext(native_context));

  // Skip the microtask execution if the associated context is shutdown.
  GotoIf(WordEqual(GetMicrotaskQueue(native_context), IntPtrConstant(0)),
         bailout);

  Label done_context(this);
  TNode<Object> last_entered_context =
      LoadFullTagged(IsolateField(IsolateFieldId::kLastEnteredContext));

  // Fast path: if the context is already active, we don't need to do anything.
  // This optimization is described in the beginning of
  // MicrotaskQueue::RunMicrotasks().
  GotoIf(TaggedEqual(native_context, last_entered_context), &done_context);

  // Slow path: context switch required.
  // Rewind to the baseline before entering the new context.
  RewindEnteredContext(baseline_entered_context_count);
  EnterContext(native_context);
  SetCurrentContext(native_context);
  Goto(&done_context);

  BIND(&done_context);
  if (DEBUG_BOOL) {
    // Postcondition: entered_contexts_.back() == native_context, i.e. the
    // context the microtask is about to run in is visible to API callers.
    TNode<IntPtrT> size =
        Load<IntPtrT>(IsolateField(IsolateFieldId::kEnteredContextCount));
    CSA_CHECK(this, IntPtrGreaterThan(size, IntPtrConstant(0)));
    TNode<RawPtrT> data =
        Load<RawPtrT>(IsolateField(IsolateFieldId::kEnteredContextData));
    TNode<Object> back = LoadFullTagged(
        data, TimesSystemPointerSize(IntPtrSub(size, IntPtrConstant(1))));
    CSA_CHECK(this, TaggedEqual(back, native_context));
  }
}

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
void MicrotaskQueueBuiltinsAssembler::SetupContinuationPreservedEmbedderData(
    TNode<Microtask> microtask) {
  TNode<Object> continuation_preserved_embedder_data = LoadObjectField(
      microtask, offsetof(Microtask, continuation_preserved_embedder_data_));
  Label continuation_preserved_data_done(this);
  // The isolate's continuation preserved embedder data is cleared at the start
  // of RunMicrotasks and after each microtask, so it only needs to be set if
  // it's not undefined.
  GotoIf(IsUndefined(continuation_preserved_embedder_data),
         &continuation_preserved_data_done);
  SetContinuationPreservedEmbedderData(continuation_preserved_embedder_data);
  Goto(&continuation_preserved_data_done);
  BIND(&continuation_preserved_data_done);
}

void MicrotaskQueueBuiltinsAssembler::ClearContinuationPreservedEmbedderData() {
  SetContinuationPreservedEmbedderData(UndefinedConstant());
}
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

void MicrotaskQueueBuiltinsAssembler::RunSingleMicrotask(
    TNode<Context> current_context,
    TNode<IntPtrT> baseline_entered_context_count, TNode<Microtask> microtask) {
  CSA_DCHECK(this, TaggedIsNotSmi(microtask));
  CSA_DCHECK(this, Word32BinaryNot(IsExecutionTerminating()));

  StoreRoot(RootIndex::kCurrentMicrotask, microtask);

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  SetupContinuationPreservedEmbedderData(microtask);
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

  TNode<Map> microtask_map = LoadMap(microtask);
  TNode<Uint16T> microtask_type = LoadMapInstanceType(microtask_map);

  Label is_callable(this), is_callback(this),
      is_promise_fulfill_reaction_job(this),
      is_promise_reject_reaction_job(this),
      is_promise_resolve_thenable_job(this), is_async_resume(this),
      is_unreachable(this, Label::kDeferred), done(this);

  int32_t case_values[] = {CALLABLE_TASK_TYPE,
                           CALLBACK_TASK_TYPE,
                           PROMISE_FULFILL_REACTION_JOB_TASK_TYPE,
                           PROMISE_REJECT_REACTION_JOB_TASK_TYPE,
                           PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE,
                           ASYNC_RESUME_TASK_TYPE};
  Label* case_labels[] = {&is_callable,
                          &is_callback,
                          &is_promise_fulfill_reaction_job,
                          &is_promise_reject_reaction_job,
                          &is_promise_resolve_thenable_job,
                          &is_async_resume};
  static_assert(arraysize(case_values) == arraysize(case_labels), "");
  Switch(microtask_type, &is_unreachable, case_values, case_labels,
         arraysize(case_labels));

  BIND(&is_callable);
  {
    // Enter the context of the {microtask}.
    TNode<NativeContext> microtask_context = LoadObjectField<NativeContext>(
        microtask, offsetof(CallableTask, context_));
    PrepareForContext(microtask_context, baseline_entered_context_count, &done);

    TNode<JSReceiver> callable = LoadObjectField<JSReceiver>(
        microtask, offsetof(CallableTask, callable_));

    TVARIABLE(Object, var_exception);
    Label if_exception(this, Label::kDeferred);
    {
      ScopedExceptionHandler handler(this, &if_exception, &var_exception);
      Call(microtask_context, callable, UndefinedConstant());
    }

    Goto(&done);

    BIND(&if_exception);
    {
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
      ClearContinuationPreservedEmbedderData();
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

      // Report unhandled microtask exceptions in respective native context.
      CallRuntime(Runtime::kReportMessageFromMicrotask, microtask_context,
                  var_exception.value());
      Goto(&done);
    }
  }

  BIND(&is_callback);
  {
    const TNode<Object> microtask_callback =
        LoadObjectField(microtask, offsetof(CallbackTask, callback_));
    const TNode<Object> microtask_data =
        LoadObjectField(microtask, offsetof(CallbackTask, data_));

    // For C++ microtasks we can use an arbitrary context, but
    // setting it to NoContext currently breaks things on Blink side.
    // Therefore we use the current context.
    TNode<Context> microtask_context = current_context;

    // If this turns out to become a bottleneck because of the calls
    // to C++ via CEntry, we can choose to speed them up using a
    // similar mechanism that we use for the CallApiFunction stub,
    // except that calling the MicrotaskCallback is even easier, since
    // it doesn't accept any tagged parameters, doesn't return a value
    // and ignores exceptions.
    //
    // But from our current measurements it doesn't seem to be a
    // serious performance problem, even if the microtask is full
    // of CallHandlerTasks (which is not a realistic use case anyways).

    // Don't create a try-catch block around this call because this function
    // is guaranteed to throw only termination exception which would anyway
    // bubble up to Execution::TryRunMicrotasks() to shut it down.
    CallRuntime(Runtime::kRunMicrotaskCallback, microtask_context,
                microtask_callback, microtask_data);

    Goto(&done);
  }

  BIND(&is_promise_resolve_thenable_job);
  {
    // Enter the context of the {microtask}.
    TNode<NativeContext> microtask_context =
        LoadNativeContext(LoadObjectField<Context>(
            microtask, offsetof(PromiseResolveThenableJobTask, context_)));
    PrepareForContext(microtask_context, baseline_entered_context_count, &done);

    const TNode<Object> promise_to_resolve = LoadObjectField(
        microtask,
        offsetof(PromiseResolveThenableJobTask, promise_to_resolve_));
    const TNode<Object> then = LoadObjectField(
        microtask, offsetof(PromiseResolveThenableJobTask, then_));
    const TNode<Object> thenable = LoadObjectField(
        microtask, offsetof(PromiseResolveThenableJobTask, thenable_));

    RunAllPromiseHooks(PromiseHookType::kBefore, microtask_context,
                       CAST(promise_to_resolve));

    TVARIABLE(Object, var_exception);
    Label if_exception(this, Label::kDeferred);
    {
      ScopedExceptionHandler handler(this, &if_exception, &var_exception);
      Label if_slow(this, Label::kDeferred), call_after_hooks(this);

      PromiseResolveThenableJobFast(microtask_context, CAST(promise_to_resolve),
                                    CAST(thenable), CAST(then), &if_slow);
      Goto(&call_after_hooks);

      BIND(&if_slow);
      {
        CallBuiltin(Builtin::kPromiseResolveThenableJob, microtask_context,
                    promise_to_resolve, thenable, then);
        Goto(&call_after_hooks);
      }

      BIND(&call_after_hooks);
    }

    RunAllPromiseHooks(PromiseHookType::kAfter, microtask_context,
                       CAST(promise_to_resolve));

    Goto(&done);

    BIND(&if_exception);
    {
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
      ClearContinuationPreservedEmbedderData();
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

      // Report unhandled microtask exceptions in respective native context.
      CallRuntime(Runtime::kReportMessageFromMicrotask, microtask_context,
                  var_exception.value());
      Goto(&done);
    }
  }

  BIND(&is_promise_fulfill_reaction_job);
  {
    // Enter the context of the {microtask}.
    TNode<NativeContext> microtask_context =
        LoadNativeContext(LoadObjectField<Context>(
            microtask, offsetof(PromiseReactionJobTask, context_)));
    PrepareForContext(microtask_context, baseline_entered_context_count, &done);

    const TNode<Object> argument =
        LoadObjectField(microtask, offsetof(PromiseReactionJobTask, argument_));
    const TNode<Object> job_handler =
        LoadObjectField(microtask, offsetof(PromiseReactionJobTask, handler_));
    const TNode<HeapObject> promise_or_capability = CAST(LoadObjectField(
        microtask, offsetof(PromiseReactionJobTask, promise_or_capability_)));

    // Run the promise before/debug hook if enabled.
    RunAllPromiseHooks(PromiseHookType::kBefore, microtask_context,
                       promise_or_capability);

    TVARIABLE(Object, var_exception);
    Label if_exception(this, Label::kDeferred);
    {
      ScopedExceptionHandler handler(this, &if_exception, &var_exception);
      Label if_slow(this, Label::kDeferred), call_after_hooks(this);
      PromiseFulfillReactionJobInline(microtask_context, CAST(argument),
                                      CAST(job_handler), promise_or_capability,
                                      &if_slow);
      Goto(&call_after_hooks);

      BIND(&if_slow);
      {
        CallBuiltin(Builtin::kPromiseFulfillReactionJob, microtask_context,
                    argument, job_handler, promise_or_capability);
        Goto(&call_after_hooks);
      }

      BIND(&call_after_hooks);
    }

    // Run the promise after/debug hook if enabled.
    RunAllPromiseHooks(PromiseHookType::kAfter, microtask_context,
                       promise_or_capability);

    Goto(&done);

    BIND(&if_exception);
    {
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
      ClearContinuationPreservedEmbedderData();
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

      // Report unhandled microtask exceptions in respective native context.
      CallRuntime(Runtime::kReportMessageFromMicrotask, microtask_context,
                  var_exception.value());
      Goto(&done);
    }
  }

  BIND(&is_promise_reject_reaction_job);
  {
    // Enter the context of the {microtask}.
    TNode<NativeContext> microtask_context =
        LoadNativeContext(LoadObjectField<Context>(
            microtask, offsetof(PromiseReactionJobTask, context_)));
    PrepareForContext(microtask_context, baseline_entered_context_count, &done);

    const TNode<Object> argument =
        LoadObjectField(microtask, offsetof(PromiseReactionJobTask, argument_));
    const TNode<Object> job_handler =
        LoadObjectField(microtask, offsetof(PromiseReactionJobTask, handler_));
    const TNode<HeapObject> promise_or_capability = CAST(LoadObjectField(
        microtask, offsetof(PromiseReactionJobTask, promise_or_capability_)));

    // Run the promise before/debug hook if enabled.
    RunAllPromiseHooks(PromiseHookType::kBefore, microtask_context,
                       promise_or_capability);

    TVARIABLE(Object, var_exception);
    Label if_exception(this, Label::kDeferred);
    {
      ScopedExceptionHandler handler(this, &if_exception, &var_exception);
      Label if_slow(this, Label::kDeferred), call_after_hooks(this);
      PromiseRejectReactionJobInline(microtask_context, CAST(argument),
                                     CAST(job_handler), promise_or_capability,
                                     &if_slow);
      Goto(&call_after_hooks);

      BIND(&if_slow);
      {
        CallBuiltin(Builtin::kPromiseRejectReactionJob, microtask_context,
                    argument, job_handler, promise_or_capability);
        Goto(&call_after_hooks);
      }

      BIND(&call_after_hooks);
    }

    // Run the promise after/debug hook if enabled.
    RunAllPromiseHooks(PromiseHookType::kAfter, microtask_context,
                       promise_or_capability);

    Goto(&done);

    BIND(&if_exception);
    {
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
      ClearContinuationPreservedEmbedderData();
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

      // Report unhandled microtask exceptions in respective native context.
      CallRuntime(Runtime::kReportMessageFromMicrotask, microtask_context,
                  var_exception.value());
      Goto(&done);
    }
  }

  BIND(&is_async_resume);
  {
    // Specialized handler for resuming async generators/functions when the
    // awaited/yielded value is a non-thenable.  Avoids closure allocation
    // and indirect Call dispatch.
    using Traits = ObjectTraits<AsyncResumeTask>;
    const TNode<JSGeneratorObject> generator =
        CAST(LoadObjectField(microtask, Traits::kGeneratorOffset));
    const TNode<Object> value =
        LoadObjectField(microtask, Traits::kValueOffset);

    TNode<NativeContext> microtask_context =
        GetCreationContextUnchecked(generator);
    PrepareForContext(microtask_context, baseline_entered_context_count, &done);

    TVARIABLE(Object, var_exception);
    Label if_exception(this, Label::kDeferred);
    Label kind_async_function_await(this);
    TNode<Smi> kind = LoadObjectField<Smi>(microtask, Traits::kKindOffset);
    GotoIf(SmiEqual(kind, SmiConstant(AsyncResumeTask::kAsyncFunctionAwait)),
           &kind_async_function_await);
    // kYield: AsyncGeneratorYieldWithAwaitResolveClosure logic.
    CSA_DCHECK(this, SmiEqual(kind, SmiConstant(AsyncResumeTask::kYield)));
    {
      ScopedExceptionHandler handler(this, &if_exception, &var_exception);
      // 1. SetGeneratorNotAwaiting
      StoreObjectFieldNoWriteBarrier(
          generator, offsetof(JSAsyncGeneratorObject, is_awaiting_),
          SmiConstant(0));
      // 2. AsyncGeneratorResolve(generator, value, false)
      CallBuiltin(Builtin::kAsyncGeneratorResolve, microtask_context, generator,
                  value, FalseConstant());
      // 3. AsyncGeneratorResumeNext(generator)
      CallBuiltin(Builtin::kAsyncGeneratorResumeNext, microtask_context,
                  generator);
    }
    Goto(&done);

    // kAsyncFunctionAwait: resume the async function with kNext.
    BIND(&kind_async_function_await);
    {
      ScopedExceptionHandler handler(this, &if_exception, &var_exception);
      StoreObjectFieldNoWriteBarrier(generator,
                                     offsetof(JSGeneratorObject, resume_mode_),
                                     SmiConstant(JSGeneratorObject::kNext));
      CallBuiltin(Builtin::kResumeGeneratorTrampoline, microtask_context, value,
                  generator);
    }
    Goto(&done);

    BIND(&if_exception);
    {
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
      ClearContinuationPreservedEmbedderData();
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

      CallRuntime(Runtime::kReportMessageFromMicrotask, microtask_context,
                  var_exception.value());
      Goto(&done);
    }
  }

  BIND(&is_unreachable);
  Unreachable();

  BIND(&done);
  {
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
    ClearContinuationPreservedEmbedderData();
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  }
}

void MicrotaskQueueBuiltinsAssembler::IncrementFinishedMicrotaskCount(
    TNode<RawPtrT> microtask_queue) {
  TNode<IntPtrT> count = Load<IntPtrT>(
      microtask_queue,
      IntPtrConstant(MicrotaskQueue::kFinishedMicrotaskCountOffset));
  TNode<IntPtrT> new_count = IntPtrAdd(count, IntPtrConstant(1));
  StoreNoWriteBarrier(
      MachineType::PointerRepresentation(), microtask_queue,
      IntPtrConstant(MicrotaskQueue::kFinishedMicrotaskCountOffset), new_count);
}

TNode<Context> MicrotaskQueueBuiltinsAssembler::GetCurrentContext() {
  // TODO(delphick): Add a checked cast. For now this is not possible as context
  // can actually be Tagged<Smi>(0).
  return TNode<Context>::UncheckedCast(
      LoadFullTagged(IsolateField(IsolateFieldId::kContext)));
}

void MicrotaskQueueBuiltinsAssembler::SetCurrentContext(
    TNode<Context> context) {
  StoreFullTaggedNoWriteBarrier(IsolateField(IsolateFieldId::kContext),
                                context);
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetEnteredContextCount() {
  return Load<IntPtrT>(IsolateField(IsolateFieldId::kEnteredContextCount));
}

void MicrotaskQueueBuiltinsAssembler::EnterContext(
    TNode<Context> native_context) {
  CSA_DCHECK(this, IsNativeContext(native_context));

  TNode<RawPtrT> hsi = ReinterpretCast<RawPtrT>(
      IsolateField(IsolateFieldId::kHandleScopeImplementer));

  using ContextStack = DetachableVector<Context>;
  TNode<IntPtrT> size_offset =
      IntPtrConstant(HandleScopeImplementer::kEnteredContextsOffset +
                     ContextStack::kSizeOffset);

  TNode<IntPtrT> capacity =
      Load<IntPtrT>(IsolateField(IsolateFieldId::kEnteredContextCapacity));
  TNode<IntPtrT> size =
      Load<IntPtrT>(IsolateField(IsolateFieldId::kEnteredContextCount));

  Label if_append(this), if_grow(this, Label::kDeferred), done(this);
  Branch(WordEqual(size, capacity), &if_grow, &if_append);
  BIND(&if_append);
  {
    TNode<RawPtrT> data =
        Load<RawPtrT>(IsolateField(IsolateFieldId::kEnteredContextData));
    StoreFullTaggedNoWriteBarrier(data, TimesSystemPointerSize(size),
                                  native_context);

    TNode<IntPtrT> new_size = IntPtrAdd(size, IntPtrConstant(1));
    StoreNoWriteBarrier(MachineType::PointerRepresentation(), hsi, size_offset,
                        new_size);

    StoreFullTaggedNoWriteBarrier(
        IsolateField(IsolateFieldId::kLastEnteredContext), native_context);
    Goto(&done);
  }

  BIND(&if_grow);
  {
    TNode<ExternalReference> function =
        ExternalConstant(ExternalReference::call_enter_context_function());
    CallCFunction(function, MachineType::Int32(),
                  std::make_pair(MachineType::Pointer(), hsi),
                  std::make_pair(MachineType::Pointer(),
                                 BitcastTaggedToWord(native_context)));
    Goto(&done);
  }

  BIND(&done);
}

void MicrotaskQueueBuiltinsAssembler::RewindEnteredContext(
    TNode<IntPtrT> saved_entered_context_count) {
  TNode<RawPtrT> hsi = ReinterpretCast<RawPtrT>(
      IsolateField(IsolateFieldId::kHandleScopeImplementer));

  using ContextStack = DetachableVector<Context>;
  TNode<IntPtrT> size_offset =
      IntPtrConstant(HandleScopeImplementer::kEnteredContextsOffset +
                     ContextStack::kSizeOffset);

  if (DEBUG_BOOL) {
    TNode<IntPtrT> size =
        Load<IntPtrT>(IsolateField(IsolateFieldId::kEnteredContextCount));
    CSA_CHECK(this, IntPtrLessThanOrEqual(saved_entered_context_count, size));
  }

  StoreNoWriteBarrier(MachineType::PointerRepresentation(), hsi, size_offset,
                      saved_entered_context_count);

  Label if_empty(this), if_not_empty(this), done_rewind(this);
  Branch(IntPtrEqual(saved_entered_context_count, IntPtrConstant(0)), &if_empty,
         &if_not_empty);
  BIND(&if_empty);
  {
    StoreFullTaggedNoWriteBarrier(
        IsolateField(IsolateFieldId::kLastEnteredContext), SmiConstant(0));
    Goto(&done_rewind);
  }
  BIND(&if_not_empty);
  {
    TNode<RawPtrT> data =
        Load<RawPtrT>(IsolateField(IsolateFieldId::kEnteredContextData));
    TNode<IntPtrT> index =
        IntPtrSub(saved_entered_context_count, IntPtrConstant(1));
    TNode<Object> last_context =
        LoadFullTagged(data, TimesSystemPointerSize(index));
    StoreFullTaggedNoWriteBarrier(
        IsolateField(IsolateFieldId::kLastEnteredContext), last_context);
    Goto(&done_rewind);
  }
  BIND(&done_rewind);
}

void MicrotaskQueueBuiltinsAssembler::RunAllPromiseHooks(
    PromiseHookType type, TNode<Context> context,
    TNode<HeapObject> promise_or_capability) {
  TNode<Uint32T> promiseHookFlags = PromiseHookFlags();
#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
  Label hook(this, Label::kDeferred), done_hook(this);
  Branch(NeedsAnyPromiseHooks(promiseHookFlags), &hook, &done_hook);
  BIND(&hook);
  {
#endif
    switch (type) {
      case PromiseHookType::kBefore:
#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
        RunContextPromiseHookBefore(context, CAST(promise_or_capability),
                                    promiseHookFlags);
#endif
        RunPromiseHook(Runtime::kPromiseHookBefore, context,
                       promise_or_capability, promiseHookFlags);
        break;
      case PromiseHookType::kAfter:
#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
        RunContextPromiseHookAfter(context, CAST(promise_or_capability),
                                   promiseHookFlags);
#endif
        RunPromiseHook(Runtime::kPromiseHookAfter, context,
                       promise_or_capability, promiseHookFlags);
        break;
      default:
        UNREACHABLE();
    }
#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
    Goto(&done_hook);
  }
  BIND(&done_hook);
#endif
}

void MicrotaskQueueBuiltinsAssembler::RunPromiseHook(
    Runtime::FunctionId id, TNode<Context> context,
    TNode<HeapObject> promise_or_capability,
    TNode<Uint32T> promiseHookFlags) {
  Label hook(this, Label::kDeferred), done_hook(this);
  Branch(IsIsolatePromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(
      promiseHookFlags), &hook, &done_hook);
  BIND(&hook);
  {
    // Get to the underlying JSPromise instance.
    TNode<HeapObject> promise = Select<HeapObject>(
        IsPromiseCapability(promise_or_capability),
        [=, this] {
          return CAST(LoadObjectField(promise_or_capability,
                                      offsetof(PromiseCapability, promise_)));
        },

        [=] { return promise_or_capability; });
    GotoIf(IsUndefined(promise), &done_hook);
    CallRuntime(id, context, promise);
    Goto(&done_hook);
  }
  BIND(&done_hook);
}

TF_BUILTIN(EnqueueMicrotask, MicrotaskQueueBuiltinsAssembler) {
  auto microtask = Parameter<Microtask>(Descriptor::kMicrotask);
  auto context = Parameter<Context>(Descriptor::kContext);

  // TODO(https://crbug.com/508092629): fix the issue and reenable fast path.
#if 0
  // Fast path: if the NativeContext matches the one cached on IsolateData,
  // reuse the cached MicrotaskQueue pointer to avoid the expensive
  // NativeContext → ExternalPointerTable → MicrotaskQueue chain.
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TVARIABLE(RawPtrT, var_microtask_queue);
  Label load_from_context(this), have_queue(this);
  TNode<Object> cached_context = LoadFullTagged(
      IsolateField(IsolateFieldId::kCurrentMicrotaskNativeContext));
  GotoIfNot(TaggedEqual(cached_context, native_context), &load_from_context);
  var_microtask_queue =
      Load<RawPtrT>(IsolateField(IsolateFieldId::kCurrentMicrotaskQueue));
  Goto(&have_queue);

  BIND(&load_from_context);
  {
    var_microtask_queue = GetMicrotaskQueue(native_context);
    // Update the cache so subsequent calls with the same context are fast.
    StoreFullTaggedNoWriteBarrier(
        IsolateField(IsolateFieldId::kCurrentMicrotaskNativeContext),
        native_context);
    StoreNoWriteBarrier(MachineType::PointerRepresentation(),
                        IsolateField(IsolateFieldId::kCurrentMicrotaskQueue),
                        var_microtask_queue.value());
    Goto(&have_queue);
  }

  BIND(&have_queue);
  TNode<RawPtrT> microtask_queue = var_microtask_queue.value();
#else
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<RawPtrT> microtask_queue = GetMicrotaskQueue(native_context);
#endif  // https://crbug.com/508092629

  // Do not store the microtask if MicrotaskQueue is not available, that may
  // happen when the context shutdown.
  Label if_shutdown(this, Label::kDeferred);
  GotoIf(WordEqual(microtask_queue, IntPtrConstant(0)), &if_shutdown);

  TNode<RawPtrT> ring_buffer = GetMicrotaskRingBuffer(microtask_queue);
  TNode<IntPtrT> capacity = GetMicrotaskQueueCapacity(microtask_queue);
  TNode<IntPtrT> size = GetMicrotaskQueueSize(microtask_queue);
  TNode<IntPtrT> start = GetMicrotaskQueueStart(microtask_queue);

  Label if_grow(this, Label::kDeferred);
  GotoIf(IntPtrEqual(size, capacity), &if_grow);

  // |microtask_queue| has an unused slot to store |microtask|.
  {
    StoreNoWriteBarrier(MachineType::PointerRepresentation(), ring_buffer,
                        CalculateRingBufferOffset(capacity, start, size),
                        BitcastTaggedToWord(microtask));
    StoreNoWriteBarrier(MachineType::PointerRepresentation(), microtask_queue,
                        IntPtrConstant(MicrotaskQueue::kSizeOffset),
                        IntPtrAdd(size, IntPtrConstant(1)));
    Return(UndefinedConstant());
  }

  // |microtask_queue| has no space to store |microtask|. Fall back to C++
  // implementation to grow the buffer.
  BIND(&if_grow);
  {
    TNode<ExternalReference> isolate_constant =
        ExternalConstant(ExternalReference::isolate_address());
    TNode<ExternalReference> function =
        ExternalConstant(ExternalReference::call_enqueue_microtask_function());
    CallCFunction(function, MachineType::AnyTagged(),
                  std::make_pair(MachineType::Pointer(), isolate_constant),
                  std::make_pair(MachineType::IntPtr(), microtask_queue),
                  std::make_pair(MachineType::AnyTagged(), microtask));
    Return(UndefinedConstant());
  }

  Bind(&if_shutdown);
  Return(UndefinedConstant());
}

// Implements the HTML queueMicrotask API
// https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#microtask-queuing
TF_BUILTIN(GlobalQueueMicrotask, MicrotaskQueueBuiltinsAssembler) {
  auto callback = Parameter<Object>(Descriptor::kCallback);
  auto context = Parameter<Context>(Descriptor::kContext);

  Label callback_is_not_callable(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(callback), &callback_is_not_callable);
  GotoIfNot(IsCallable(CAST(callback)), &callback_is_not_callable);

  // Use the callback's context as the microtask context, per
  // https://webidl.spec.whatwg.org/#js-invoking-callback-functions
  // specifically, getting the "associated realm" of the callback. Per-spec,
  // this is underspecified for non-platform objects, but for function objects
  // we're expected to use the [[Realm]] slot (which is the creation context),
  // and we can do the same for other incoming callbacks.
  Label callback_has_no_context(this, Label::kDeferred);
  TNode<NativeContext> callback_context =
      GetCreationContext(CAST(callback), &callback_has_no_context);

  TNode<CallableTask> microtask = TNode<CallableTask>::UncheckedCast(
      AllocateInNewSpace(sizeof(CallableTask)));
  StoreMapNoWriteBarrier(microtask, RootIndex::kCallableTaskMap);
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  StoreObjectFieldNoWriteBarrier(
      microtask, offsetof(CallableTask, continuation_preserved_embedder_data_),
      GetContinuationPreservedEmbedderData());
#endif
  StoreObjectFieldNoWriteBarrier(microtask, offsetof(CallableTask, callable_),
                                 callback);
  StoreObjectFieldNoWriteBarrier(microtask, offsetof(CallableTask, context_),
                                 callback_context);
  // TODO(leszeks): Tailcall this builtin, which would require the right
  // argument teardown.
  Return(CallBuiltin(Builtin::kEnqueueMicrotask, context, microtask));

  BIND(&callback_is_not_callable);
  ThrowTypeError(context, MessageTemplate::kNotCallable, callback);

  BIND(&callback_has_no_context);
  // Callables should always have a creation context.
  Unreachable();
}

TF_BUILTIN(RunMicrotasks, MicrotaskQueueBuiltinsAssembler) {
  // Load the current context from the isolate.
  TNode<Context> current_context = GetCurrentContext();
  TNode<IntPtrT> baseline_entered_context_count = GetEnteredContextCount();

  auto microtask_queue =
      UncheckedParameter<RawPtrT>(Descriptor::kMicrotaskQueue);

  Label loop(this), done(this);
  Goto(&loop);
  BIND(&loop);

  TNode<IntPtrT> size = GetMicrotaskQueueSize(microtask_queue);

  // Exit if the queue is empty.
  GotoIf(WordEqual(size, IntPtrConstant(0)), &done);

  TNode<RawPtrT> ring_buffer = GetMicrotaskRingBuffer(microtask_queue);
  TNode<IntPtrT> capacity = GetMicrotaskQueueCapacity(microtask_queue);
  TNode<IntPtrT> start = GetMicrotaskQueueStart(microtask_queue);

  TNode<IntPtrT> offset =
      CalculateRingBufferStartOffsetWithoutCapacityMask(start);
  TNode<RawPtrT> microtask_pointer = Load<RawPtrT>(ring_buffer, offset);
  TNode<Microtask> microtask = CAST(BitcastWordToTagged(microtask_pointer));

  TNode<IntPtrT> new_size = IntPtrSub(size, IntPtrConstant(1));
  TNode<IntPtrT> new_start = WordAnd(IntPtrAdd(start, IntPtrConstant(1)),
                                     IntPtrSub(capacity, IntPtrConstant(1)));

  // Remove |microtask| from |ring_buffer| before running it, since its
  // invocation may add another microtask into |ring_buffer|.
  SetMicrotaskQueueSize(microtask_queue, new_size);
  SetMicrotaskQueueStart(microtask_queue, new_start);

  RunSingleMicrotask(current_context, baseline_entered_context_count,
                     microtask);
  IncrementFinishedMicrotaskCount(microtask_queue);
  Goto(&loop);

  BIND(&done);
  {
    // Reset the "current microtask" on the isolate.
    StoreRoot(RootIndex::kCurrentMicrotask, UndefinedConstant());
    // Restore the original context and rewind.
    RewindEnteredContext(baseline_entered_context_count);
    SetCurrentContext(current_context);
    Return(UndefinedConstant());
  }
}

#include "src/codegen/undef-code-stub-assembler-macros.inc"

}  // namespace internal
}  // namespace v8
