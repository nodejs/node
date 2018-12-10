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

namespace v8 {
namespace internal {

template <typename T>
using TNode = compiler::TNode<T>;

class MicrotaskQueueBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit MicrotaskQueueBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<IntPtrT> GetDefaultMicrotaskQueue();
  TNode<IntPtrT> GetMicrotaskQueue(TNode<Context> context);
  TNode<IntPtrT> GetMicrotaskRingBuffer(TNode<IntPtrT> microtask_queue);
  TNode<IntPtrT> GetMicrotaskQueueCapacity(TNode<IntPtrT> microtask_queue);
  TNode<IntPtrT> GetMicrotaskQueueSize(TNode<IntPtrT> microtask_queue);
  void SetMicrotaskQueueSize(TNode<IntPtrT> microtask_queue,
                             TNode<IntPtrT> new_size);
  TNode<IntPtrT> GetMicrotaskQueueStart(TNode<IntPtrT> microtask_queue);
  void SetMicrotaskQueueStart(TNode<IntPtrT> microtask_queue,
                              TNode<IntPtrT> new_start);
  TNode<IntPtrT> CalculateRingBufferOffset(TNode<IntPtrT> capacity,
                                           TNode<IntPtrT> start,
                                           TNode<IntPtrT> index);
  void RunSingleMicrotask(TNode<Context> current_context,
                          TNode<Microtask> microtask);

  TNode<Context> GetCurrentContext();
  void SetCurrentContext(TNode<Context> context);

  void EnterMicrotaskContext(TNode<Context> native_context);
  void LeaveMicrotaskContext();

  void RunPromiseHook(Runtime::FunctionId id, TNode<Context> context,
                      SloppyTNode<HeapObject> promise_or_capability);
};

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetDefaultMicrotaskQueue() {
  auto ref = ExternalReference::default_microtask_queue_address(isolate());
  return UncheckedCast<IntPtrT>(
      Load(MachineType::Pointer(), ExternalConstant(ref)));
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueue(
    TNode<Context> native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  return LoadObjectField<IntPtrT>(native_context,
                                  NativeContext::kMicrotaskQueueOffset);
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskRingBuffer(
    TNode<IntPtrT> microtask_queue) {
  return UncheckedCast<IntPtrT>(
      Load(MachineType::IntPtr(), microtask_queue,
           IntPtrConstant(MicrotaskQueue::kRingBufferOffset)));
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueueCapacity(
    TNode<IntPtrT> microtask_queue) {
  return UncheckedCast<IntPtrT>(
      Load(MachineType::IntPtr(), microtask_queue,
           IntPtrConstant(MicrotaskQueue::kCapacityOffset)));
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueueSize(
    TNode<IntPtrT> microtask_queue) {
  return UncheckedCast<IntPtrT>(
      Load(MachineType::IntPtr(), microtask_queue,
           IntPtrConstant(MicrotaskQueue::kSizeOffset)));
}

void MicrotaskQueueBuiltinsAssembler::SetMicrotaskQueueSize(
    TNode<IntPtrT> microtask_queue, TNode<IntPtrT> new_size) {
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), microtask_queue,
                      IntPtrConstant(MicrotaskQueue::kSizeOffset), new_size);
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::GetMicrotaskQueueStart(
    TNode<IntPtrT> microtask_queue) {
  return UncheckedCast<IntPtrT>(
      Load(MachineType::IntPtr(), microtask_queue,
           IntPtrConstant(MicrotaskQueue::kStartOffset)));
}

void MicrotaskQueueBuiltinsAssembler::SetMicrotaskQueueStart(
    TNode<IntPtrT> microtask_queue, TNode<IntPtrT> new_start) {
  StoreNoWriteBarrier(MachineType::PointerRepresentation(), microtask_queue,
                      IntPtrConstant(MicrotaskQueue::kStartOffset), new_start);
}

TNode<IntPtrT> MicrotaskQueueBuiltinsAssembler::CalculateRingBufferOffset(
    TNode<IntPtrT> capacity, TNode<IntPtrT> start, TNode<IntPtrT> index) {
  return TimesPointerSize(
      WordAnd(IntPtrAdd(start, index), IntPtrSub(capacity, IntPtrConstant(1))));
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
  TNode<Microtask> microtask =
      UncheckedCast<Microtask>(Parameter(Descriptor::kMicrotask));
  TNode<Context> context =
      UncheckedCast<Context>(Parameter(Descriptor::kContext));
  TNode<Context> native_context = LoadNativeContext(context);
  TNode<IntPtrT> microtask_queue = GetMicrotaskQueue(native_context);

  TNode<IntPtrT> ring_buffer = GetMicrotaskRingBuffer(microtask_queue);
  TNode<IntPtrT> capacity = GetMicrotaskQueueCapacity(microtask_queue);
  TNode<IntPtrT> size = GetMicrotaskQueueSize(microtask_queue);
  TNode<IntPtrT> start = GetMicrotaskQueueStart(microtask_queue);

  Label if_grow(this);
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
    CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                   MachineType::IntPtr(), MachineType::AnyTagged(), function,
                   isolate_constant, microtask_queue, microtask);
    Return(UndefinedConstant());
  }
}

TF_BUILTIN(RunMicrotasks, MicrotaskQueueBuiltinsAssembler) {
  // Load the current context from the isolate.
  TNode<Context> current_context = GetCurrentContext();

  // TODO(tzik): Take a MicrotaskQueue parameter to support non-default queue.
  TNode<IntPtrT> microtask_queue = GetDefaultMicrotaskQueue();

  Label loop(this), done(this);
  Goto(&loop);
  BIND(&loop);

  TNode<IntPtrT> size = GetMicrotaskQueueSize(microtask_queue);

  // Exit if the queue is empty.
  GotoIf(WordEqual(size, IntPtrConstant(0)), &done);

  TNode<IntPtrT> ring_buffer = GetMicrotaskRingBuffer(microtask_queue);
  TNode<IntPtrT> capacity = GetMicrotaskQueueCapacity(microtask_queue);
  TNode<IntPtrT> start = GetMicrotaskQueueStart(microtask_queue);

  TNode<IntPtrT> offset =
      CalculateRingBufferOffset(capacity, start, IntPtrConstant(0));
  TNode<IntPtrT> microtask_pointer =
      UncheckedCast<IntPtrT>(Load(MachineType::Pointer(), ring_buffer, offset));
  TNode<Microtask> microtask =
      UncheckedCast<Microtask>(BitcastWordToTagged(microtask_pointer));

  TNode<IntPtrT> new_size = IntPtrSub(size, IntPtrConstant(1));
  TNode<IntPtrT> new_start = WordAnd(IntPtrAdd(start, IntPtrConstant(1)),
                                     IntPtrSub(capacity, IntPtrConstant(1)));

  // Remove |microtask| from |ring_buffer| before running it, since its
  // invocation may add another microtask into |ring_buffer|.
  SetMicrotaskQueueSize(microtask_queue, new_size);
  SetMicrotaskQueueStart(microtask_queue, new_start);

  RunSingleMicrotask(current_context, microtask);
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
