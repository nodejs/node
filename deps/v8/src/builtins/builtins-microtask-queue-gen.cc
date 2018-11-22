// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/code-stub-assembler.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/microtask-queue.h"

namespace v8 {
namespace internal {

template <typename T>
using TNode = compiler::TNode<T>;

class MicrotaskQueueBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit MicrotaskQueueBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<MicrotaskQueue> GetDefaultMicrotaskQueue();
  TNode<IntPtrT> GetPendingMicrotaskCount(
      TNode<MicrotaskQueue> microtask_queue);
  void SetPendingMicrotaskCount(TNode<MicrotaskQueue> microtask_queue,
                                TNode<IntPtrT> new_num_tasks);
  TNode<FixedArray> GetQueuedMicrotasks(TNode<MicrotaskQueue> microtask_queue);
  void SetQueuedMicrotasks(TNode<MicrotaskQueue> microtask_queue,
                           TNode<FixedArray> new_queue);
  void RunSingleMicrotask(TNode<Context> current_context,
                          TNode<Microtask> microtask);

  TNode<Context> GetCurrentContext();
  void SetCurrentContext(TNode<Context> context);

  void EnterMicrotaskContext(TNode<Context> context);
  void LeaveMicrotaskContext();

  void RunPromiseHook(Runtime::FunctionId id, TNode<Context> context,
                      SloppyTNode<HeapObject> promise_or_capability);

  TNode<Object> GetPendingException() {
    auto ref = ExternalReference::Create(kPendingExceptionAddress, isolate());
    return TNode<Object>::UncheckedCast(
        Load(MachineType::AnyTagged(), ExternalConstant(ref)));
  }
  void ClearPendingException() {
    auto ref = ExternalReference::Create(kPendingExceptionAddress, isolate());
    StoreNoWriteBarrier(MachineRepresentation::kTagged, ExternalConstant(ref),
                        TheHoleConstant());
  }

  TNode<Object> GetScheduledException() {
    auto ref = ExternalReference::scheduled_exception_address(isolate());
    return TNode<Object>::UncheckedCast(
        Load(MachineType::AnyTagged(), ExternalConstant(ref)));
  }
  void ClearScheduledException() {
    auto ref = ExternalReference::scheduled_exception_address(isolate());
    StoreNoWriteBarrier(MachineRepresentation::kTagged, ExternalConstant(ref),
                        TheHoleConstant());
  }
};

TNode<MicrotaskQueue>
MicrotaskQueueBuiltinsAssembler::GetDefaultMicrotaskQueue() {
  return TNode<MicrotaskQueue>::UncheckedCast(
      LoadRoot(RootIndex::kDefaultMicrotaskQueue));
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetPendingMicrotaskCount(
    TNode<MicrotaskQueue> microtask_queue) {
  TNode<IntPtrT> result = LoadAndUntagObjectField(
      microtask_queue, MicrotaskQueue::kPendingMicrotaskCountOffset);
  return result;
}

void MicrotaskQueueBuiltinsAssembler::SetPendingMicrotaskCount(
    TNode<MicrotaskQueue> microtask_queue, TNode<IntPtrT> new_num_tasks) {
  StoreObjectField(microtask_queue,
                   MicrotaskQueue::kPendingMicrotaskCountOffset,
                   SmiFromIntPtr(new_num_tasks));
}

TNode<FixedArray> MicrotaskQueueBuiltinsAssembler::GetQueuedMicrotasks(
    TNode<MicrotaskQueue> microtask_queue) {
  return LoadObjectField<FixedArray>(microtask_queue,
                                     MicrotaskQueue::kQueueOffset);
}

void MicrotaskQueueBuiltinsAssembler::SetQueuedMicrotasks(
    TNode<MicrotaskQueue> microtask_queue, TNode<FixedArray> new_queue) {
  StoreObjectField(microtask_queue, MicrotaskQueue::kQueueOffset, new_queue);
}

void MicrotaskQueueBuiltinsAssembler::RunSingleMicrotask(
    TNode<Context> current_context, TNode<Microtask> microtask) {
  CSA_ASSERT(this, TaggedIsNotSmi(microtask));

  StoreRoot(RootIndex::kCurrentMicrotask, microtask);
  TNode<Map> microtask_map = LoadMap(microtask);
  TNode<Int32T> microtask_type = LoadMapInstanceType(microtask_map);

  VARIABLE(var_exception, MachineRepresentation::kTagged, TheHoleConstant());
  Label if_exception(this, Label::kDeferred);
  Label is_callable(this), is_callback(this),
      is_promise_fulfill_reaction_job(this),
      is_promise_reject_reaction_job(this),
      is_promise_resolve_thenable_job(this), is_weak_factory_cleanup_job(this),
      is_unreachable(this, Label::kDeferred), done(this);

  int32_t case_values[] = {CALLABLE_TASK_TYPE,
                           CALLBACK_TASK_TYPE,
                           PROMISE_FULFILL_REACTION_JOB_TASK_TYPE,
                           PROMISE_REJECT_REACTION_JOB_TASK_TYPE,
                           PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE,
                           WEAK_FACTORY_CLEANUP_JOB_TASK_TYPE};
  Label* case_labels[] = {&is_callable,
                          &is_callback,
                          &is_promise_fulfill_reaction_job,
                          &is_promise_reject_reaction_job,
                          &is_promise_resolve_thenable_job,
                          &is_weak_factory_cleanup_job};
  static_assert(arraysize(case_values) == arraysize(case_labels), "");
  Switch(microtask_type, &is_unreachable, case_values, case_labels,
         arraysize(case_labels));

  BIND(&is_callable);
  {
    // Enter the context of the {microtask}.
    TNode<Context> microtask_context =
        LoadObjectField<Context>(microtask, CallableTask::kContextOffset);
    TNode<Context> native_context = LoadNativeContext(microtask_context);

    CSA_ASSERT(this, IsNativeContext(native_context));
    EnterMicrotaskContext(native_context);
    SetCurrentContext(native_context);

    TNode<JSReceiver> callable =
        LoadObjectField<JSReceiver>(microtask, CallableTask::kCallableOffset);
    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
        microtask_context, callable, UndefinedConstant());
    GotoIfException(result, &if_exception, &var_exception);
    LeaveMicrotaskContext();
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
    CSA_ASSERT(this, IsNativeContext(native_context));
    EnterMicrotaskContext(native_context);
    SetCurrentContext(native_context);

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
    LeaveMicrotaskContext();
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&is_promise_fulfill_reaction_job);
  {
    // Enter the context of the {microtask}.
    TNode<Context> microtask_context = LoadObjectField<Context>(
        microtask, PromiseReactionJobTask::kContextOffset);
    TNode<Context> native_context = LoadNativeContext(microtask_context);
    CSA_ASSERT(this, IsNativeContext(native_context));
    EnterMicrotaskContext(native_context);
    SetCurrentContext(native_context);

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

    LeaveMicrotaskContext();
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&is_promise_reject_reaction_job);
  {
    // Enter the context of the {microtask}.
    TNode<Context> microtask_context = LoadObjectField<Context>(
        microtask, PromiseReactionJobTask::kContextOffset);
    TNode<Context> native_context = LoadNativeContext(microtask_context);
    CSA_ASSERT(this, IsNativeContext(native_context));
    EnterMicrotaskContext(native_context);
    SetCurrentContext(native_context);

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

    LeaveMicrotaskContext();
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&is_weak_factory_cleanup_job);
  {
    // Enter the context of the {weak_factory}.
    TNode<JSWeakFactory> weak_factory = LoadObjectField<JSWeakFactory>(
        microtask, WeakFactoryCleanupJobTask::kFactoryOffset);
    TNode<Context> native_context = LoadObjectField<Context>(
        weak_factory, JSWeakFactory::kNativeContextOffset);
    CSA_ASSERT(this, IsNativeContext(native_context));
    EnterMicrotaskContext(native_context);
    SetCurrentContext(native_context);

    Node* const result = CallRuntime(Runtime::kWeakFactoryCleanupJob,
                                     native_context, weak_factory);

    GotoIfException(result, &if_exception, &var_exception);
    LeaveMicrotaskContext();
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
    LeaveMicrotaskContext();
    SetCurrentContext(current_context);
    Goto(&done);
  }

  BIND(&done);
}

TNode<Context> MicrotaskQueueBuiltinsAssembler::GetCurrentContext() {
  auto ref = ExternalReference::Create(kContextAddress, isolate());
  return TNode<Context>::UncheckedCast(
      Load(MachineType::AnyTagged(), ExternalConstant(ref)));
}

void MicrotaskQueueBuiltinsAssembler::SetCurrentContext(
    TNode<Context> context) {
  auto ref = ExternalReference::Create(kContextAddress, isolate());
  StoreNoWriteBarrier(MachineRepresentation::kTagged, ExternalConstant(ref),
                      context);
}

void MicrotaskQueueBuiltinsAssembler::EnterMicrotaskContext(
    TNode<Context> native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  auto ref = ExternalReference::handle_scope_implementer_address(isolate());
  Node* const hsi = Load(MachineType::Pointer(), ExternalConstant(ref));
  StoreNoWriteBarrier(
      MachineType::PointerRepresentation(), hsi,
      IntPtrConstant(HandleScopeImplementerOffsets::kMicrotaskContext),
      BitcastTaggedToWord(native_context));

  // Load mirrored std::vector length from
  // HandleScopeImplementer::entered_contexts_count_
  auto type = kSizetSize == 8 ? MachineType::Uint64() : MachineType::Uint32();
  Node* entered_contexts_length = Load(
      type, hsi,
      IntPtrConstant(HandleScopeImplementerOffsets::kEnteredContextsCount));

  auto rep = kSizetSize == 8 ? MachineRepresentation::kWord64
                             : MachineRepresentation::kWord32;

  StoreNoWriteBarrier(
      rep, hsi,
      IntPtrConstant(
          HandleScopeImplementerOffsets::kEnteredContextCountDuringMicrotasks),
      entered_contexts_length);
}

void MicrotaskQueueBuiltinsAssembler::LeaveMicrotaskContext() {
  auto ref = ExternalReference::handle_scope_implementer_address(isolate());

  Node* const hsi = Load(MachineType::Pointer(), ExternalConstant(ref));
  StoreNoWriteBarrier(
      MachineType::PointerRepresentation(), hsi,
      IntPtrConstant(HandleScopeImplementerOffsets::kMicrotaskContext),
      IntPtrConstant(0));
  if (kSizetSize == 4) {
    StoreNoWriteBarrier(
        MachineRepresentation::kWord32, hsi,
        IntPtrConstant(HandleScopeImplementerOffsets::
                           kEnteredContextCountDuringMicrotasks),
        Int32Constant(0));
  } else {
    StoreNoWriteBarrier(
        MachineRepresentation::kWord64, hsi,
        IntPtrConstant(HandleScopeImplementerOffsets::
                           kEnteredContextCountDuringMicrotasks),
        Int64Constant(0));
  }
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
  Node* microtask = Parameter(Descriptor::kMicrotask);

  TNode<MicrotaskQueue> microtask_queue = GetDefaultMicrotaskQueue();
  TNode<IntPtrT> num_tasks = GetPendingMicrotaskCount(microtask_queue);
  TNode<IntPtrT> new_num_tasks = IntPtrAdd(num_tasks, IntPtrConstant(1));
  TNode<FixedArray> queue = GetQueuedMicrotasks(microtask_queue);
  TNode<IntPtrT> queue_length = LoadAndUntagFixedArrayBaseLength(queue);

  Label if_append(this), if_grow(this), done(this);
  Branch(WordEqual(num_tasks, queue_length), &if_grow, &if_append);

  BIND(&if_grow);
  {
    // Determine the new queue length and check if we need to allocate
    // in large object space (instead of just going to new space, where
    // we also know that we don't need any write barriers for setting
    // up the new queue object).
    Label if_newspace(this), if_lospace(this, Label::kDeferred);
    TNode<IntPtrT> new_queue_length =
        IntPtrMax(IntPtrConstant(8), IntPtrAdd(num_tasks, num_tasks));
    Branch(IntPtrLessThanOrEqual(new_queue_length,
                                 IntPtrConstant(FixedArray::kMaxRegularLength)),
           &if_newspace, &if_lospace);

    BIND(&if_newspace);
    {
      // This is the likely case where the new queue fits into new space,
      // and thus we don't need any write barriers for initializing it.
      TNode<FixedArray> new_queue =
          CAST(AllocateFixedArray(PACKED_ELEMENTS, new_queue_length));
      CopyFixedArrayElements(PACKED_ELEMENTS, queue, new_queue, num_tasks,
                             SKIP_WRITE_BARRIER);
      StoreFixedArrayElement(new_queue, num_tasks, microtask,
                             SKIP_WRITE_BARRIER);
      FillFixedArrayWithValue(PACKED_ELEMENTS, new_queue, new_num_tasks,
                              new_queue_length, RootIndex::kUndefinedValue);
      SetQueuedMicrotasks(microtask_queue, new_queue);
      Goto(&done);
    }

    BIND(&if_lospace);
    {
      // The fallback case where the new queue ends up in large object space.
      TNode<FixedArray> new_queue = CAST(AllocateFixedArray(
          PACKED_ELEMENTS, new_queue_length, INTPTR_PARAMETERS,
          AllocationFlag::kAllowLargeObjectAllocation));
      CopyFixedArrayElements(PACKED_ELEMENTS, queue, new_queue, num_tasks);
      StoreFixedArrayElement(new_queue, num_tasks, microtask);
      FillFixedArrayWithValue(PACKED_ELEMENTS, new_queue, new_num_tasks,
                              new_queue_length, RootIndex::kUndefinedValue);
      SetQueuedMicrotasks(microtask_queue, new_queue);
      Goto(&done);
    }
  }

  BIND(&if_append);
  {
    StoreFixedArrayElement(queue, num_tasks, microtask);
    Goto(&done);
  }

  BIND(&done);
  SetPendingMicrotaskCount(microtask_queue, new_num_tasks);
  Return(UndefinedConstant());
}

TF_BUILTIN(RunMicrotasks, MicrotaskQueueBuiltinsAssembler) {
  // Load the current context from the isolate.
  TNode<Context> current_context = GetCurrentContext();
  TNode<MicrotaskQueue> microtask_queue = GetDefaultMicrotaskQueue();

  Label init_queue_loop(this), done_init_queue_loop(this);
  Goto(&init_queue_loop);
  BIND(&init_queue_loop);
  {
    TVARIABLE(IntPtrT, index, IntPtrConstant(0));
    Label loop(this, &index);

    TNode<IntPtrT> num_tasks = GetPendingMicrotaskCount(microtask_queue);
    GotoIf(IntPtrEqual(num_tasks, IntPtrConstant(0)), &done_init_queue_loop);

    TNode<FixedArray> queue = GetQueuedMicrotasks(microtask_queue);

    CSA_ASSERT(this, IntPtrGreaterThanOrEqual(
                         LoadAndUntagFixedArrayBaseLength(queue), num_tasks));
    CSA_ASSERT(this, IntPtrGreaterThan(num_tasks, IntPtrConstant(0)));

    SetQueuedMicrotasks(microtask_queue, EmptyFixedArrayConstant());
    SetPendingMicrotaskCount(microtask_queue, IntPtrConstant(0));

    Goto(&loop);
    BIND(&loop);
    {
      TNode<Microtask> microtask =
          CAST(LoadFixedArrayElement(queue, index.value()));
      index = IntPtrAdd(index.value(), IntPtrConstant(1));

      CSA_ASSERT(this, TaggedIsNotSmi(microtask));
      RunSingleMicrotask(current_context, microtask);
      Branch(IntPtrLessThan(index.value(), num_tasks), &loop, &init_queue_loop);
    }
  }

  BIND(&done_init_queue_loop);
  {
    // Reset the "current microtask" on the isolate.
    StoreRoot(RootIndex::kCurrentMicrotask, UndefinedConstant());
    Return(UndefinedConstant());
  }
}

}  // namespace internal
}  // namespace v8
