// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/code-stub-assembler.h"
#include "src/microtask-queue.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/promise.h"
#include "src/objects/smi-inl.h"

namespace v8 {
namespace internal {

template <typename T>
using TNode = compiler::TNode<T>;

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

  void PrepareForContext(TNode<Context> microtask_context, Label* bailout);
  void RunSingleMicrotask(TNode<Context> current_context,
                          TNode<Microtask> microtask);
  void IncrementFinishedMicrotaskCount(TNode<RawPtrT> microtask_queue);

  TNode<Context> GetCurrentContext();
  void SetCurrentContext(TNode<Context> context);

  TNode<IntPtrT> GetEnteredContextCount();
  void EnterMicrotaskContext(TNode<Context> native_context);
  void RewindEnteredContext(TNode<IntPtrT> saved_entered_context_count);

  void RunPromiseHook(Runtime::FunctionId id, TNode<Context> context,
                      SloppyTNode<HeapObject> promise_or_capability);
};

TNode<RawPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueue(
    TNode<Context> native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  return LoadObjectField<RawPtrT>(native_context,
                                  NativeContext::kMicrotaskQueueOffset);
}

TNode<RawPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskRingBuffer(
    TNode<RawPtrT> microtask_queue) {
  return UncheckedCast<RawPtrT>(
      Load(MachineType::Pointer(), microtask_queue,
           IntPtrConstant(MicrotaskQueue::kRingBufferOffset)));
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueueCapacity(
    TNode<RawPtrT> microtask_queue) {
  return UncheckedCast<IntPtrT>(
      Load(MachineType::IntPtr(), microtask_queue,
           IntPtrConstant(MicrotaskQueue::kCapacityOffset)));
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueueSize(
    TNode<RawPtrT> microtask_queue) {
  return UncheckedCast<IntPtrT>(
      Load(MachineType::IntPtr(), microtask_queue,
           IntPtrConstant(MicrotaskQueue::kSizeOffset)));
}

void MicrotaskQueueBuiltinsAssembler::SetMicrotaskQueueSize(
    TNode<RawPtrT> microtask_queue, TNode<IntPtrT> new_size) {
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), microtask_queue,
                      IntPtrConstant(MicrotaskQueue::kSizeOffset), new_size);
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueueStart(
    TNode<RawPtrT> microtask_queue) {
  return UncheckedCast<IntPtrT>(
      Load(MachineType::IntPtr(), microtask_queue,
           IntPtrConstant(MicrotaskQueue::kStartOffset)));
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

void MicrotaskQueueBuiltinsAssembler::PrepareForContext(
    TNode<Context> native_context, Label* bailout) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  // Skip the microtask execution if the associated context is shutdown.
  GotoIf(WordEqual(GetMicrotaskQueue(native_context), IntPtrConstant(0)),
         bailout);

  EnterMicrotaskContext(native_context);
  SetCurrentContext(native_context);
}

void MicrotaskQueueBuiltinsAssembler::RunSingleMicrotask(
    TNode<Context> current_context, TNode<Microtask> microtask) {
  CSA_ASSERT(this, TaggedIsNotSmi(microtask));

  StoreRoot(RootIndex::kCurrentMicrotask, microtask);
  TNode<IntPtrT> saved_entered_context_count = GetEnteredContextCount();
  TNode<Map> microtask_map = LoadMap(microtask);
  TNode<Int32T> microtask_type = LoadMapInstanceType(microtask_map);

  VARIABLE(var_exception, MachineRepresentation::kTagged, TheHoleConstant());
  Label if_exception(this, Label::kDeferred);
  Label is_callable(this), is_callback(this),
      is_promise_fulfill_reaction_job(this),
      is_promise_reject_reaction_job(this),
      is_promise_resolve_thenable_job(this),
      is_finalization_group_cleanup_job(this),
      is_unreachable(this, Label::kDeferred), done(this);

  int32_t case_values[] = {CALLABLE_TASK_TYPE,
                           CALLBACK_TASK_TYPE,
                           PROMISE_FULFILL_REACTION_JOB_TASK_TYPE,
                           PROMISE_REJECT_REACTION_JOB_TASK_TYPE,
                           PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE,
                           FINALIZATION_GROUP_CLEANUP_JOB_TASK_TYPE};
  Label* case_labels[] = {&is_callable,
                          &is_callback,
                          &is_promise_fulfill_reaction_job,
                          &is_promise_reject_reaction_job,
                          &is_promise_resolve_thenable_job,
                          &is_finalization_group_cleanup_job};
  static_assert(arraysize(case_values) == arraysize(case_labels), "");
  Switch(microtask_type, &is_unreachable, case_values, case_labels,
         arraysize(case_labels));

  BIND(&is_callable);
  {
    // Enter the context of the {microtask}.
    TNode<Context> microtask_context =
        LoadObjectField<Context>(microtask, CallableTask::kContextOffset);
    TNode<Context> native_context = LoadNativeContext(microtask_context);
    PrepareForContext(native_context, &done);

    TNode<JSReceiver> callable =
        LoadObjectField<JSReceiver>(microtask, CallableTask::kCallableOffset);
    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
        microtask_context, callable, UndefinedConstant());
    GotoIfException(result, &if_exception, &var_exception);
    RewindEnteredContext(saved_entered_context_count);
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&is_callback);
  {
    Node* const microtask_callback =
        LoadObjectField(microtask, CallbackTask::kCallbackOffset);
    Node* const microtask_data =
        LoadObjectField(microtask, CallbackTask::kDataOffset);

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
    Node* const result =
        CallRuntime(Runtime::kRunMicrotaskCallback, current_context,
                    microtask_callback, microtask_data);
    GotoIfException(result, &if_exception, &var_exception);
    Goto(&done);
  }

  BIND(&is_promise_resolve_thenable_job);
  {
    // Enter the context of the {microtask}.
    TNode<Context> microtask_context = LoadObjectField<Context>(
        microtask, PromiseResolveThenableJobTask::kContextOffset);
    TNode<Context> native_context = LoadNativeContext(microtask_context);
    PrepareForContext(native_context, &done);

    Node* const promise_to_resolve = LoadObjectField(
        microtask, PromiseResolveThenableJobTask::kPromiseToResolveOffset);
    Node* const then =
        LoadObjectField(microtask, PromiseResolveThenableJobTask::kThenOffset);
    Node* const thenable = LoadObjectField(
        microtask, PromiseResolveThenableJobTask::kThenableOffset);

    Node* const result =
        CallBuiltin(Builtins::kPromiseResolveThenableJob, native_context,
                    promise_to_resolve, thenable, then);
    GotoIfException(result, &if_exception, &var_exception);
    RewindEnteredContext(saved_entered_context_count);
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&is_promise_fulfill_reaction_job);
  {
    // Enter the context of the {microtask}.
    TNode<Context> microtask_context = LoadObjectField<Context>(
        microtask, PromiseReactionJobTask::kContextOffset);
    TNode<Context> native_context = LoadNativeContext(microtask_context);
    PrepareForContext(native_context, &done);

    Node* const argument =
        LoadObjectField(microtask, PromiseReactionJobTask::kArgumentOffset);
    Node* const handler =
        LoadObjectField(microtask, PromiseReactionJobTask::kHandlerOffset);
    Node* const promise_or_capability = LoadObjectField(
        microtask, PromiseReactionJobTask::kPromiseOrCapabilityOffset);

    // Run the promise before/debug hook if enabled.
    RunPromiseHook(Runtime::kPromiseHookBefore, microtask_context,
                   promise_or_capability);

    Node* const result =
        CallBuiltin(Builtins::kPromiseFulfillReactionJob, microtask_context,
                    argument, handler, promise_or_capability);
    GotoIfException(result, &if_exception, &var_exception);

    // Run the promise after/debug hook if enabled.
    RunPromiseHook(Runtime::kPromiseHookAfter, microtask_context,
                   promise_or_capability);

    RewindEnteredContext(saved_entered_context_count);
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&is_promise_reject_reaction_job);
  {
    // Enter the context of the {microtask}.
    TNode<Context> microtask_context = LoadObjectField<Context>(
        microtask, PromiseReactionJobTask::kContextOffset);
    TNode<Context> native_context = LoadNativeContext(microtask_context);
    PrepareForContext(native_context, &done);

    Node* const argument =
        LoadObjectField(microtask, PromiseReactionJobTask::kArgumentOffset);
    Node* const handler =
        LoadObjectField(microtask, PromiseReactionJobTask::kHandlerOffset);
    Node* const promise_or_capability = LoadObjectField(
        microtask, PromiseReactionJobTask::kPromiseOrCapabilityOffset);

    // Run the promise before/debug hook if enabled.
    RunPromiseHook(Runtime::kPromiseHookBefore, microtask_context,
                   promise_or_capability);

    Node* const result =
        CallBuiltin(Builtins::kPromiseRejectReactionJob, microtask_context,
                    argument, handler, promise_or_capability);
    GotoIfException(result, &if_exception, &var_exception);

    // Run the promise after/debug hook if enabled.
    RunPromiseHook(Runtime::kPromiseHookAfter, microtask_context,
                   promise_or_capability);

    RewindEnteredContext(saved_entered_context_count);
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&is_finalization_group_cleanup_job);
  {
    // Enter the context of the {finalization_group}.
    TNode<JSFinalizationGroup> finalization_group =
        LoadObjectField<JSFinalizationGroup>(
            microtask,
            FinalizationGroupCleanupJobTask::kFinalizationGroupOffset);
    TNode<Context> native_context = LoadObjectField<Context>(
        finalization_group, JSFinalizationGroup::kNativeContextOffset);
    PrepareForContext(native_context, &done);

    Node* const result = CallRuntime(Runtime::kFinalizationGroupCleanupJob,
                                     native_context, finalization_group);

    GotoIfException(result, &if_exception, &var_exception);
    RewindEnteredContext(saved_entered_context_count);
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&is_unreachable);
  Unreachable();

  BIND(&if_exception);
  {
    // Report unhandled exceptions from microtasks.
    CallRuntime(Runtime::kReportMessage, current_context,
                var_exception.value());
    RewindEnteredContext(saved_entered_context_count);
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&done);
}

void MicrotaskQueueBuiltinsAssembler::IncrementFinishedMicrotaskCount(
    TNode<RawPtrT> microtask_queue) {
  TNode<IntPtrT> count = UncheckedCast<IntPtrT>(
      Load(MachineType::IntPtr(), microtask_queue,
           IntPtrConstant(MicrotaskQueue::kFinishedMicrotaskCountOffset)));
  TNode<IntPtrT> new_count = IntPtrAdd(count, IntPtrConstant(1));
  StoreNoWriteBarrier(
      MachineType::PointerRepresentation(), microtask_queue,
      IntPtrConstant(MicrotaskQueue::kFinishedMicrotaskCountOffset), new_count);
}

TNode<Context> MicrotaskQueueBuiltinsAssembler::GetCurrentContext() {
  auto ref = ExternalReference::Create(kContextAddress, isolate());
  return TNode<Context>::UncheckedCast(LoadFullTagged(ExternalConstant(ref)));
}

void MicrotaskQueueBuiltinsAssembler::SetCurrentContext(
    TNode<Context> context) {
  auto ref = ExternalReference::Create(kContextAddress, isolate());
  StoreFullTaggedNoWriteBarrier(ExternalConstant(ref), context);
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetEnteredContextCount() {
  auto ref = ExternalReference::handle_scope_implementer_address(isolate());
  Node* hsi = Load(MachineType::Pointer(), ExternalConstant(ref));

  using ContextStack = DetachableVector<Context>;
  TNode<IntPtrT> size_offset =
      IntPtrConstant(HandleScopeImplementer::kEnteredContextsOffset +
                     ContextStack::kSizeOffset);
  TNode<IntPtrT> size =
      UncheckedCast<IntPtrT>(Load(MachineType::IntPtr(), hsi, size_offset));
  return size;
}

void MicrotaskQueueBuiltinsAssembler::EnterMicrotaskContext(
    TNode<Context> native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  auto ref = ExternalReference::handle_scope_implementer_address(isolate());
  Node* hsi = Load(MachineType::Pointer(), ExternalConstant(ref));

  using ContextStack = DetachableVector<Context>;
  TNode<IntPtrT> capacity_offset =
      IntPtrConstant(HandleScopeImplementer::kEnteredContextsOffset +
                     ContextStack::kCapacityOffset);
  TNode<IntPtrT> size_offset =
      IntPtrConstant(HandleScopeImplementer::kEnteredContextsOffset +
                     ContextStack::kSizeOffset);

  TNode<IntPtrT> capacity =
      UncheckedCast<IntPtrT>(Load(MachineType::IntPtr(), hsi, capacity_offset));
  TNode<IntPtrT> size =
      UncheckedCast<IntPtrT>(Load(MachineType::IntPtr(), hsi, size_offset));

  Label if_append(this), if_grow(this, Label::kDeferred), done(this);
  Branch(WordEqual(size, capacity), &if_grow, &if_append);
  BIND(&if_append);
  {
    TNode<IntPtrT> data_offset =
        IntPtrConstant(HandleScopeImplementer::kEnteredContextsOffset +
                       ContextStack::kDataOffset);
    Node* data = Load(MachineType::Pointer(), hsi, data_offset);
    StoreFullTaggedNoWriteBarrier(data, TimesSystemPointerSize(size),
                                  native_context);

    TNode<IntPtrT> new_size = IntPtrAdd(size, IntPtrConstant(1));
    StoreNoWriteBarrier(MachineType::PointerRepresentation(), hsi, size_offset,
                        new_size);

    using FlagStack = DetachableVector<int8_t>;
    TNode<IntPtrT> flag_data_offset =
        IntPtrConstant(HandleScopeImplementer::kIsMicrotaskContextOffset +
                       FlagStack::kDataOffset);
    Node* flag_data = Load(MachineType::Pointer(), hsi, flag_data_offset);
    StoreNoWriteBarrier(MachineRepresentation::kWord8, flag_data, size,
                        BoolConstant(true));
    StoreNoWriteBarrier(
        MachineType::PointerRepresentation(), hsi,
        IntPtrConstant(HandleScopeImplementer::kIsMicrotaskContextOffset +
                       FlagStack::kSizeOffset),
        new_size);

    Goto(&done);
  }

  BIND(&if_grow);
  {
    Node* function =
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
  auto ref = ExternalReference::handle_scope_implementer_address(isolate());
  Node* hsi = Load(MachineType::Pointer(), ExternalConstant(ref));

  using ContextStack = DetachableVector<Context>;
  TNode<IntPtrT> size_offset =
      IntPtrConstant(HandleScopeImplementer::kEnteredContextsOffset +
                     ContextStack::kSizeOffset);

#ifdef ENABLE_VERIFY_CSA
  TNode<IntPtrT> size =
      UncheckedCast<IntPtrT>(Load(MachineType::IntPtr(), hsi, size_offset));
  CSA_ASSERT(this, IntPtrLessThan(IntPtrConstant(0), size));
  CSA_ASSERT(this, IntPtrLessThanOrEqual(saved_entered_context_count, size));
#endif

  StoreNoWriteBarrier(MachineType::PointerRepresentation(), hsi, size_offset,
                      saved_entered_context_count);

  using FlagStack = DetachableVector<int8_t>;
  StoreNoWriteBarrier(
      MachineType::PointerRepresentation(), hsi,
      IntPtrConstant(HandleScopeImplementer::kIsMicrotaskContextOffset +
                     FlagStack::kSizeOffset),
      saved_entered_context_count);
}

void MicrotaskQueueBuiltinsAssembler::RunPromiseHook(
    Runtime::FunctionId id, TNode<Context> context,
    SloppyTNode<HeapObject> promise_or_capability) {
  Label hook(this, Label::kDeferred), done_hook(this);
  Branch(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(), &hook,
         &done_hook);
  BIND(&hook);
  {
    // Get to the underlying JSPromise instance.
    TNode<HeapObject> promise = Select<HeapObject>(
        IsPromiseCapability(promise_or_capability),
        [=] {
          return CAST(LoadObjectField(promise_or_capability,
                                      PromiseCapability::kPromiseOffset));
        },

        [=] { return promise_or_capability; });
    GotoIf(IsUndefined(promise), &done_hook);
    CallRuntime(id, context, promise);
    Goto(&done_hook);
  }
  BIND(&done_hook);
}

TF_BUILTIN(EnqueueMicrotask, MicrotaskQueueBuiltinsAssembler) {
  TNode<Microtask> microtask =
      UncheckedCast<Microtask>(Parameter(Descriptor::kMicrotask));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Context> native_context = LoadNativeContext(context);
  TNode<RawPtrT> microtask_queue = GetMicrotaskQueue(native_context);

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
    Node* isolate_constant =
        ExternalConstant(ExternalReference::isolate_address(isolate()));
    Node* function =
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

TF_BUILTIN(RunMicrotasks, MicrotaskQueueBuiltinsAssembler) {
  // Load the current context from the isolate.
  TNode<Context> current_context = GetCurrentContext();

  TNode<RawPtrT> microtask_queue =
      UncheckedCast<RawPtrT>(Parameter(Descriptor::kMicrotaskQueue));

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
      CalculateRingBufferOffset(capacity, start, IntPtrConstant(0));
  TNode<RawPtrT> microtask_pointer =
      UncheckedCast<RawPtrT>(Load(MachineType::Pointer(), ring_buffer, offset));
  TNode<Microtask> microtask = CAST(BitcastWordToTagged(microtask_pointer));

  TNode<IntPtrT> new_size = IntPtrSub(size, IntPtrConstant(1));
  TNode<IntPtrT> new_start = WordAnd(IntPtrAdd(start, IntPtrConstant(1)),
                                     IntPtrSub(capacity, IntPtrConstant(1)));

  // Remove |microtask| from |ring_buffer| before running it, since its
  // invocation may add another microtask into |ring_buffer|.
  SetMicrotaskQueueSize(microtask_queue, new_size);
  SetMicrotaskQueueStart(microtask_queue, new_start);

  RunSingleMicrotask(current_context, microtask);
  IncrementFinishedMicrotaskCount(microtask_queue);
  Goto(&loop);

  BIND(&done);
  {
    // Reset the "current microtask" on the isolate.
    StoreRoot(RootIndex::kCurrentMicrotask, UndefinedConstant());
    Return(UndefinedConstant());
  }
}

}  // namespace internal
}  // namespace v8
