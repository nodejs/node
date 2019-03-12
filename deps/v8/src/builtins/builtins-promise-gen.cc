// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-promise-gen.h"

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-promise.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/objects-inl.h"
#include "src/objects/js-promise.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

using compiler::Node;
using IteratorRecord = IteratorBuiltinsAssembler::IteratorRecord;

Node* PromiseBuiltinsAssembler::AllocateJSPromise(Node* context) {
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  CSA_ASSERT(this, IsFunctionWithPrototypeSlotMap(LoadMap(promise_fun)));
  Node* const promise_map =
      LoadObjectField(promise_fun, JSFunction::kPrototypeOrInitialMapOffset);
  Node* const promise = Allocate(JSPromise::kSizeWithEmbedderFields);
  StoreMapNoWriteBarrier(promise, promise_map);
  StoreObjectFieldRoot(promise, JSPromise::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(promise, JSPromise::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  return promise;
}

void PromiseBuiltinsAssembler::PromiseInit(Node* promise) {
  STATIC_ASSERT(v8::Promise::kPending == 0);
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kReactionsOrResultOffset,
                                 SmiConstant(Smi::zero()));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset,
                                 SmiConstant(Smi::zero()));
  for (int offset = JSPromise::kSize;
       offset < JSPromise::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectFieldNoWriteBarrier(promise, offset, SmiConstant(Smi::zero()));
  }
}

Node* PromiseBuiltinsAssembler::AllocateAndInitJSPromise(Node* context) {
  return AllocateAndInitJSPromise(context, UndefinedConstant());
}

Node* PromiseBuiltinsAssembler::AllocateAndInitJSPromise(Node* context,
                                                         Node* parent) {
  Node* const instance = AllocateJSPromise(context);
  PromiseInit(instance);

  Label out(this);
  GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &out);
  CallRuntime(Runtime::kPromiseHookInit, context, instance, parent);
  Goto(&out);

  BIND(&out);
  return instance;
}

Node* PromiseBuiltinsAssembler::AllocateAndSetJSPromise(
    Node* context, v8::Promise::PromiseState status, Node* result) {
  DCHECK_NE(Promise::kPending, status);

  Node* const instance = AllocateJSPromise(context);
  StoreObjectFieldNoWriteBarrier(instance, JSPromise::kReactionsOrResultOffset,
                                 result);
  STATIC_ASSERT(JSPromise::kStatusShift == 0);
  StoreObjectFieldNoWriteBarrier(instance, JSPromise::kFlagsOffset,
                                 SmiConstant(status));
  for (int offset = JSPromise::kSize;
       offset < JSPromise::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectFieldNoWriteBarrier(instance, offset, SmiConstant(0));
  }

  Label out(this);
  GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &out);
  CallRuntime(Runtime::kPromiseHookInit, context, instance,
              UndefinedConstant());
  Goto(&out);

  BIND(&out);
  return instance;
}

std::pair<Node*, Node*>
PromiseBuiltinsAssembler::CreatePromiseResolvingFunctions(
    Node* promise, Node* debug_event, Node* native_context) {
  Node* const promise_context = CreatePromiseResolvingFunctionsContext(
      promise, debug_event, native_context);
  Node* const map = LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
  Node* const resolve_info = LoadContextElement(
      native_context,
      Context::PROMISE_CAPABILITY_DEFAULT_RESOLVE_SHARED_FUN_INDEX);
  Node* const resolve =
      AllocateFunctionWithMapAndContext(map, resolve_info, promise_context);
  Node* const reject_info = LoadContextElement(
      native_context,
      Context::PROMISE_CAPABILITY_DEFAULT_REJECT_SHARED_FUN_INDEX);
  Node* const reject =
      AllocateFunctionWithMapAndContext(map, reject_info, promise_context);
  return std::make_pair(resolve, reject);
}

// ES #sec-newpromisecapability
TF_BUILTIN(NewPromiseCapability, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const constructor = Parameter(Descriptor::kConstructor);
  Node* const debug_event = Parameter(Descriptor::kDebugEvent);
  TNode<Context> const native_context = LoadNativeContext(context);

  Label if_not_constructor(this, Label::kDeferred),
      if_notcallable(this, Label::kDeferred), if_fast_promise_capability(this),
      if_slow_promise_capability(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(constructor), &if_not_constructor);
  GotoIfNot(IsConstructorMap(LoadMap(constructor)), &if_not_constructor);
  Branch(WordEqual(constructor,
                   LoadContextElement(native_context,
                                      Context::PROMISE_FUNCTION_INDEX)),
         &if_fast_promise_capability, &if_slow_promise_capability);

  BIND(&if_fast_promise_capability);
  {
    Node* promise =
        AllocateAndInitJSPromise(native_context, UndefinedConstant());

    Node* resolve = nullptr;
    Node* reject = nullptr;
    std::tie(resolve, reject) =
        CreatePromiseResolvingFunctions(promise, debug_event, native_context);

    Node* capability = Allocate(PromiseCapability::kSize);
    StoreMapNoWriteBarrier(capability, RootIndex::kPromiseCapabilityMap);
    StoreObjectFieldNoWriteBarrier(capability,
                                   PromiseCapability::kPromiseOffset, promise);
    StoreObjectFieldNoWriteBarrier(capability,
                                   PromiseCapability::kResolveOffset, resolve);
    StoreObjectFieldNoWriteBarrier(capability, PromiseCapability::kRejectOffset,
                                   reject);
    Return(capability);
  }

  BIND(&if_slow_promise_capability);
  {
    Node* capability = Allocate(PromiseCapability::kSize);
    StoreMapNoWriteBarrier(capability, RootIndex::kPromiseCapabilityMap);
    StoreObjectFieldRoot(capability, PromiseCapability::kPromiseOffset,
                         RootIndex::kUndefinedValue);
    StoreObjectFieldRoot(capability, PromiseCapability::kResolveOffset,
                         RootIndex::kUndefinedValue);
    StoreObjectFieldRoot(capability, PromiseCapability::kRejectOffset,
                         RootIndex::kUndefinedValue);

    Node* executor_context =
        CreatePromiseGetCapabilitiesExecutorContext(capability, native_context);
    Node* executor_info = LoadContextElement(
        native_context, Context::PROMISE_GET_CAPABILITIES_EXECUTOR_SHARED_FUN);
    Node* function_map = LoadContextElement(
        native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
    TNode<JSFunction> executor = CAST(AllocateFunctionWithMapAndContext(
        function_map, executor_info, executor_context));

    Node* promise = Construct(native_context, CAST(constructor), executor);
    StoreObjectField(capability, PromiseCapability::kPromiseOffset, promise);

    Node* resolve =
        LoadObjectField(capability, PromiseCapability::kResolveOffset);
    GotoIf(TaggedIsSmi(resolve), &if_notcallable);
    GotoIfNot(IsCallable(resolve), &if_notcallable);

    Node* reject =
        LoadObjectField(capability, PromiseCapability::kRejectOffset);
    GotoIf(TaggedIsSmi(reject), &if_notcallable);
    GotoIfNot(IsCallable(reject), &if_notcallable);
    Return(capability);
  }

  BIND(&if_not_constructor);
  ThrowTypeError(context, MessageTemplate::kNotConstructor, constructor);

  BIND(&if_notcallable);
  ThrowTypeError(context, MessageTemplate::kPromiseNonCallable);
}

Node* PromiseBuiltinsAssembler::CreatePromiseContext(Node* native_context,
                                                     int slots) {
  DCHECK_GE(slots, Context::MIN_CONTEXT_SLOTS);

  Node* const context = AllocateInNewSpace(FixedArray::SizeFor(slots));
  InitializeFunctionContext(native_context, context, slots);
  return context;
}

Node* PromiseBuiltinsAssembler::CreatePromiseAllResolveElementContext(
    Node* promise_capability, Node* native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  // TODO(bmeurer): Manually fold this into a single allocation.
  TNode<Map> array_map = CAST(LoadContextElement(
      native_context, Context::JS_ARRAY_PACKED_ELEMENTS_MAP_INDEX));
  TNode<JSArray> values_array = AllocateJSArray(
      PACKED_ELEMENTS, array_map, IntPtrConstant(0), SmiConstant(0));

  Node* const context = CreatePromiseContext(
      native_context, PromiseBuiltins::kPromiseAllResolveElementLength);
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
      SmiConstant(1));
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kPromiseAllResolveElementCapabilitySlot,
      promise_capability);
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot,
      values_array);

  return context;
}

Node* PromiseBuiltinsAssembler::CreatePromiseAllResolveElementFunction(
    Node* context, TNode<Smi> index, Node* native_context) {
  CSA_ASSERT(this, SmiGreaterThan(index, SmiConstant(0)));
  CSA_ASSERT(this, SmiLessThanOrEqual(
                       index, SmiConstant(PropertyArray::HashField::kMax)));
  CSA_ASSERT(this, IsNativeContext(native_context));

  Node* const map = LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
  Node* const resolve_info = LoadContextElement(
      native_context, Context::PROMISE_ALL_RESOLVE_ELEMENT_SHARED_FUN);
  Node* const resolve =
      AllocateFunctionWithMapAndContext(map, resolve_info, context);

  STATIC_ASSERT(PropertyArray::kNoHashSentinel == 0);
  StoreObjectFieldNoWriteBarrier(resolve, JSFunction::kPropertiesOrHashOffset,
                                 index);

  return resolve;
}

Node* PromiseBuiltinsAssembler::CreatePromiseResolvingFunctionsContext(
    Node* promise, Node* debug_event, Node* native_context) {
  Node* const context = CreatePromiseContext(
      native_context, PromiseBuiltins::kPromiseContextLength);
  StoreContextElementNoWriteBarrier(context, PromiseBuiltins::kPromiseSlot,
                                    promise);
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kAlreadyResolvedSlot, FalseConstant());
  StoreContextElementNoWriteBarrier(context, PromiseBuiltins::kDebugEventSlot,
                                    debug_event);
  return context;
}

Node* PromiseBuiltinsAssembler::CreatePromiseGetCapabilitiesExecutorContext(
    Node* promise_capability, Node* native_context) {
  int kContextLength = PromiseBuiltins::kCapabilitiesContextLength;
  Node* context = CreatePromiseContext(native_context, kContextLength);
  StoreContextElementNoWriteBarrier(context, PromiseBuiltins::kCapabilitySlot,
                                    promise_capability);
  return context;
}

Node* PromiseBuiltinsAssembler::PromiseHasHandler(Node* promise) {
  Node* const flags = LoadObjectField(promise, JSPromise::kFlagsOffset);
  return IsSetWord(SmiUntag(flags), 1 << JSPromise::kHasHandlerBit);
}

void PromiseBuiltinsAssembler::PromiseSetHasHandler(Node* promise) {
  TNode<Smi> const flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  TNode<Smi> const new_flags =
      SmiOr(flags, SmiConstant(1 << JSPromise::kHasHandlerBit));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset, new_flags);
}

Node* PromiseBuiltinsAssembler::IsPromiseStatus(
    Node* actual, v8::Promise::PromiseState expected) {
  return Word32Equal(actual, Int32Constant(expected));
}

Node* PromiseBuiltinsAssembler::PromiseStatus(Node* promise) {
  STATIC_ASSERT(JSPromise::kStatusShift == 0);
  TNode<Smi> const flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  return Word32And(SmiToInt32(flags), Int32Constant(JSPromise::kStatusMask));
}

void PromiseBuiltinsAssembler::PromiseSetStatus(
    Node* promise, v8::Promise::PromiseState const status) {
  CSA_ASSERT(this,
             IsPromiseStatus(PromiseStatus(promise), v8::Promise::kPending));
  CHECK_NE(status, v8::Promise::kPending);

  TNode<Smi> mask = SmiConstant(status);
  TNode<Smi> const flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset,
                                 SmiOr(flags, mask));
}

void PromiseBuiltinsAssembler::PromiseSetHandledHint(Node* promise) {
  TNode<Smi> const flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  TNode<Smi> const new_flags =
      SmiOr(flags, SmiConstant(1 << JSPromise::kHandledHintBit));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset, new_flags);
}

// ES #sec-performpromisethen
void PromiseBuiltinsAssembler::PerformPromiseThen(
    Node* context, Node* promise, Node* on_fulfilled, Node* on_rejected,
    Node* result_promise_or_capability) {
  CSA_ASSERT(this, TaggedIsNotSmi(promise));
  CSA_ASSERT(this, IsJSPromise(promise));
  CSA_ASSERT(this,
             Word32Or(IsCallable(on_fulfilled), IsUndefined(on_fulfilled)));
  CSA_ASSERT(this, Word32Or(IsCallable(on_rejected), IsUndefined(on_rejected)));
  CSA_ASSERT(this, TaggedIsNotSmi(result_promise_or_capability));
  CSA_ASSERT(
      this,
      Word32Or(Word32Or(IsJSPromise(result_promise_or_capability),
                        IsPromiseCapability(result_promise_or_capability)),
               IsUndefined(result_promise_or_capability)));

  Label if_pending(this), if_notpending(this), done(this);
  Node* const status = PromiseStatus(promise);
  Branch(IsPromiseStatus(status, v8::Promise::kPending), &if_pending,
         &if_notpending);

  BIND(&if_pending);
  {
    // The {promise} is still in "Pending" state, so we just record a new
    // PromiseReaction holding both the onFulfilled and onRejected callbacks.
    // Once the {promise} is resolved we decide on the concrete handler to
    // push onto the microtask queue.
    Node* const promise_reactions =
        LoadObjectField(promise, JSPromise::kReactionsOrResultOffset);
    Node* const reaction =
        AllocatePromiseReaction(promise_reactions, result_promise_or_capability,
                                on_fulfilled, on_rejected);
    StoreObjectField(promise, JSPromise::kReactionsOrResultOffset, reaction);
    Goto(&done);
  }

  BIND(&if_notpending);
  {
    VARIABLE(var_map, MachineRepresentation::kTagged);
    VARIABLE(var_handler, MachineRepresentation::kTagged);
    Label if_fulfilled(this), if_rejected(this, Label::kDeferred),
        enqueue(this);
    Branch(IsPromiseStatus(status, v8::Promise::kFulfilled), &if_fulfilled,
           &if_rejected);

    BIND(&if_fulfilled);
    {
      var_map.Bind(LoadRoot(RootIndex::kPromiseFulfillReactionJobTaskMap));
      var_handler.Bind(on_fulfilled);
      Goto(&enqueue);
    }

    BIND(&if_rejected);
    {
      CSA_ASSERT(this, IsPromiseStatus(status, v8::Promise::kRejected));
      var_map.Bind(LoadRoot(RootIndex::kPromiseRejectReactionJobTaskMap));
      var_handler.Bind(on_rejected);
      GotoIf(PromiseHasHandler(promise), &enqueue);
      CallRuntime(Runtime::kPromiseRevokeReject, context, promise);
      Goto(&enqueue);
    }

    BIND(&enqueue);
    Node* argument =
        LoadObjectField(promise, JSPromise::kReactionsOrResultOffset);
    Node* microtask = AllocatePromiseReactionJobTask(
        var_map.value(), context, argument, var_handler.value(),
        result_promise_or_capability);
    CallBuiltin(Builtins::kEnqueueMicrotask, context, microtask);
    Goto(&done);
  }

  BIND(&done);
  PromiseSetHasHandler(promise);
}

// ES #sec-performpromisethen
TF_BUILTIN(PerformPromiseThen, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const on_fulfilled = Parameter(Descriptor::kOnFulfilled);
  Node* const on_rejected = Parameter(Descriptor::kOnRejected);
  Node* const result_promise = Parameter(Descriptor::kResultPromise);

  CSA_ASSERT(this, TaggedIsNotSmi(result_promise));
  CSA_ASSERT(
      this, Word32Or(IsJSPromise(result_promise), IsUndefined(result_promise)));

  PerformPromiseThen(context, promise, on_fulfilled, on_rejected,
                     result_promise);
  Return(result_promise);
}

Node* PromiseBuiltinsAssembler::AllocatePromiseReaction(
    Node* next, Node* promise_or_capability, Node* fulfill_handler,
    Node* reject_handler) {
  Node* const reaction = Allocate(PromiseReaction::kSize);
  StoreMapNoWriteBarrier(reaction, RootIndex::kPromiseReactionMap);
  StoreObjectFieldNoWriteBarrier(reaction, PromiseReaction::kNextOffset, next);
  StoreObjectFieldNoWriteBarrier(reaction,
                                 PromiseReaction::kPromiseOrCapabilityOffset,
                                 promise_or_capability);
  StoreObjectFieldNoWriteBarrier(
      reaction, PromiseReaction::kFulfillHandlerOffset, fulfill_handler);
  StoreObjectFieldNoWriteBarrier(
      reaction, PromiseReaction::kRejectHandlerOffset, reject_handler);
  return reaction;
}

Node* PromiseBuiltinsAssembler::AllocatePromiseReactionJobTask(
    Node* map, Node* context, Node* argument, Node* handler,
    Node* promise_or_capability) {
  Node* const microtask = Allocate(PromiseReactionJobTask::kSize);
  StoreMapNoWriteBarrier(microtask, map);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kArgumentOffset, argument);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kHandlerOffset, handler);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kPromiseOrCapabilityOffset,
      promise_or_capability);
  return microtask;
}

Node* PromiseBuiltinsAssembler::AllocatePromiseReactionJobTask(
    RootIndex map_root_index, Node* context, Node* argument, Node* handler,
    Node* promise_or_capability) {
  DCHECK(map_root_index == RootIndex::kPromiseFulfillReactionJobTaskMap ||
         map_root_index == RootIndex::kPromiseRejectReactionJobTaskMap);
  Node* const map = LoadRoot(map_root_index);
  return AllocatePromiseReactionJobTask(map, context, argument, handler,
                                        promise_or_capability);
}

Node* PromiseBuiltinsAssembler::AllocatePromiseResolveThenableJobTask(
    Node* promise_to_resolve, Node* then, Node* thenable, Node* context) {
  Node* const microtask = Allocate(PromiseResolveThenableJobTask::kSize);
  StoreMapNoWriteBarrier(microtask,
                         RootIndex::kPromiseResolveThenableJobTaskMap);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kPromiseToResolveOffset,
      promise_to_resolve);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kThenOffset, then);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kThenableOffset, thenable);
  return microtask;
}

// ES #sec-triggerpromisereactions
Node* PromiseBuiltinsAssembler::TriggerPromiseReactions(
    Node* context, Node* reactions, Node* argument,
    PromiseReaction::Type type) {
  // We need to reverse the {reactions} here, since we record them on the
  // JSPromise in the reverse order.
  {
    VARIABLE(var_current, MachineRepresentation::kTagged, reactions);
    VARIABLE(var_reversed, MachineRepresentation::kTagged,
             SmiConstant(Smi::zero()));

    Label loop(this, {&var_current, &var_reversed}), done_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      Node* current = var_current.value();
      GotoIf(TaggedIsSmi(current), &done_loop);
      var_current.Bind(LoadObjectField(current, PromiseReaction::kNextOffset));
      StoreObjectField(current, PromiseReaction::kNextOffset,
                       var_reversed.value());
      var_reversed.Bind(current);
      Goto(&loop);
    }
    BIND(&done_loop);
    reactions = var_reversed.value();
  }

  // Morph the {reactions} into PromiseReactionJobTasks and push them
  // onto the microtask queue.
  {
    VARIABLE(var_current, MachineRepresentation::kTagged, reactions);

    Label loop(this, {&var_current}), done_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      Node* current = var_current.value();
      GotoIf(TaggedIsSmi(current), &done_loop);
      var_current.Bind(LoadObjectField(current, PromiseReaction::kNextOffset));

      // Morph {current} from a PromiseReaction into a PromiseReactionJobTask
      // and schedule that on the microtask queue. We try to minimize the number
      // of stores here to avoid screwing up the store buffer.
      STATIC_ASSERT(static_cast<int>(PromiseReaction::kSize) ==
                    static_cast<int>(PromiseReactionJobTask::kSize));
      if (type == PromiseReaction::kFulfill) {
        StoreMapNoWriteBarrier(current,
                               RootIndex::kPromiseFulfillReactionJobTaskMap);
        StoreObjectField(current, PromiseReactionJobTask::kArgumentOffset,
                         argument);
        StoreObjectField(current, PromiseReactionJobTask::kContextOffset,
                         context);
        STATIC_ASSERT(
            static_cast<int>(PromiseReaction::kFulfillHandlerOffset) ==
            static_cast<int>(PromiseReactionJobTask::kHandlerOffset));
        STATIC_ASSERT(
            static_cast<int>(PromiseReaction::kPromiseOrCapabilityOffset) ==
            static_cast<int>(
                PromiseReactionJobTask::kPromiseOrCapabilityOffset));
      } else {
        Node* handler =
            LoadObjectField(current, PromiseReaction::kRejectHandlerOffset);
        StoreMapNoWriteBarrier(current,
                               RootIndex::kPromiseRejectReactionJobTaskMap);
        StoreObjectField(current, PromiseReactionJobTask::kArgumentOffset,
                         argument);
        StoreObjectField(current, PromiseReactionJobTask::kContextOffset,
                         context);
        StoreObjectField(current, PromiseReactionJobTask::kHandlerOffset,
                         handler);
        STATIC_ASSERT(
            static_cast<int>(PromiseReaction::kPromiseOrCapabilityOffset) ==
            static_cast<int>(
                PromiseReactionJobTask::kPromiseOrCapabilityOffset));
      }
      CallBuiltin(Builtins::kEnqueueMicrotask, context, current);
      Goto(&loop);
    }
    BIND(&done_loop);
  }

  return UndefinedConstant();
}

template <typename... TArgs>
Node* PromiseBuiltinsAssembler::InvokeThen(Node* native_context, Node* receiver,
                                           TArgs... args) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label if_fast(this), if_slow(this, Label::kDeferred), done(this, &var_result);
  GotoIf(TaggedIsSmi(receiver), &if_slow);
  Node* const receiver_map = LoadMap(receiver);
  // We can skip the "then" lookup on {receiver} if it's [[Prototype]]
  // is the (initial) Promise.prototype and the Promise#then protector
  // is intact, as that guards the lookup path for the "then" property
  // on JSPromise instances which have the (initial) %PromisePrototype%.
  BranchIfPromiseThenLookupChainIntact(native_context, receiver_map, &if_fast,
                                       &if_slow);

  BIND(&if_fast);
  {
    Node* const then =
        LoadContextElement(native_context, Context::PROMISE_THEN_INDEX);
    Node* const result =
        CallJS(CodeFactory::CallFunction(
                   isolate(), ConvertReceiverMode::kNotNullOrUndefined),
               native_context, then, receiver, args...);
    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&if_slow);
  {
    Node* const then = GetProperty(native_context, receiver,
                                   isolate()->factory()->then_string());
    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        native_context, then, receiver, args...);
    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

Node* PromiseBuiltinsAssembler::InvokeResolve(Node* native_context,
                                              Node* constructor, Node* value,
                                              Label* if_exception,
                                              Variable* var_exception) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label if_fast(this), if_slow(this, Label::kDeferred), done(this, &var_result);
  // We can skip the "resolve" lookup on {constructor} if it's the
  // Promise constructor and the Promise.resolve protector is intact,
  // as that guards the lookup path for the "resolve" property on the
  // Promise constructor.
  BranchIfPromiseResolveLookupChainIntact(native_context, constructor, &if_fast,
                                          &if_slow);

  BIND(&if_fast);
  {
    Node* const result = CallBuiltin(Builtins::kPromiseResolve, native_context,
                                     constructor, value);
    GotoIfException(result, if_exception, var_exception);

    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&if_slow);
  {
    Node* const resolve =
        GetProperty(native_context, constructor, factory()->resolve_string());
    GotoIfException(resolve, if_exception, var_exception);

    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        native_context, resolve, constructor, value);
    GotoIfException(result, if_exception, var_exception);

    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

void PromiseBuiltinsAssembler::BranchIfPromiseResolveLookupChainIntact(
    Node* native_context, Node* constructor, Label* if_fast, Label* if_slow) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  GotoIfForceSlowPath(if_slow);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  GotoIfNot(WordEqual(promise_fun, constructor), if_slow);
  Branch(IsPromiseResolveProtectorCellInvalid(), if_slow, if_fast);
}

void PromiseBuiltinsAssembler::GotoIfNotPromiseResolveLookupChainIntact(
    Node* native_context, Node* constructor, Label* if_slow) {
  Label if_fast(this);
  BranchIfPromiseResolveLookupChainIntact(native_context, constructor, &if_fast,
                                          if_slow);
  BIND(&if_fast);
}

void PromiseBuiltinsAssembler::BranchIfPromiseSpeciesLookupChainIntact(
    Node* native_context, Node* promise_map, Label* if_fast, Label* if_slow) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  CSA_ASSERT(this, IsJSPromiseMap(promise_map));

  Node* const promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfForceSlowPath(if_slow);
  GotoIfNot(WordEqual(LoadMapPrototype(promise_map), promise_prototype),
            if_slow);
  Branch(IsPromiseSpeciesProtectorCellInvalid(), if_slow, if_fast);
}

void PromiseBuiltinsAssembler::BranchIfPromiseThenLookupChainIntact(
    Node* native_context, Node* receiver_map, Label* if_fast, Label* if_slow) {
  CSA_ASSERT(this, IsMap(receiver_map));
  CSA_ASSERT(this, IsNativeContext(native_context));

  GotoIfForceSlowPath(if_slow);
  GotoIfNot(IsJSPromiseMap(receiver_map), if_slow);
  Node* const promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfNot(WordEqual(LoadMapPrototype(receiver_map), promise_prototype),
            if_slow);
  Branch(IsPromiseThenProtectorCellInvalid(), if_slow, if_fast);
}

void PromiseBuiltinsAssembler::BranchIfAccessCheckFailed(
    Node* context, Node* native_context, Node* promise_constructor,
    Node* executor, Label* if_noaccess) {
  VARIABLE(var_executor, MachineRepresentation::kTagged);
  var_executor.Bind(executor);
  Label has_access(this), call_runtime(this, Label::kDeferred);

  // If executor is a bound function, load the bound function until we've
  // reached an actual function.
  Label found_function(this), loop_over_bound_function(this, &var_executor);
  Goto(&loop_over_bound_function);
  BIND(&loop_over_bound_function);
  {
    Node* executor_type = LoadInstanceType(var_executor.value());
    GotoIf(InstanceTypeEqual(executor_type, JS_FUNCTION_TYPE), &found_function);
    GotoIfNot(InstanceTypeEqual(executor_type, JS_BOUND_FUNCTION_TYPE),
              &call_runtime);
    var_executor.Bind(LoadObjectField(
        var_executor.value(), JSBoundFunction::kBoundTargetFunctionOffset));
    Goto(&loop_over_bound_function);
  }

  // Load the context from the function and compare it to the Promise
  // constructor's context. If they match, everything is fine, otherwise, bail
  // out to the runtime.
  BIND(&found_function);
  {
    Node* function_context =
        LoadObjectField(var_executor.value(), JSFunction::kContextOffset);
    Node* native_function_context = LoadNativeContext(function_context);
    Branch(WordEqual(native_context, native_function_context), &has_access,
           &call_runtime);
  }

  BIND(&call_runtime);
  {
    Branch(WordEqual(CallRuntime(Runtime::kAllowDynamicFunction, context,
                                 promise_constructor),
                     TrueConstant()),
           &has_access, if_noaccess);
  }

  BIND(&has_access);
}

void PromiseBuiltinsAssembler::SetForwardingHandlerIfTrue(
    Node* context, Node* condition, const NodeGenerator& object) {
  Label done(this);
  GotoIfNot(condition, &done);
  SetPropertyStrict(
      CAST(context), CAST(object()),
      HeapConstant(factory()->promise_forwarding_handler_symbol()),
      TrueConstant());
  Goto(&done);
  BIND(&done);
}

void PromiseBuiltinsAssembler::SetPromiseHandledByIfTrue(
    Node* context, Node* condition, Node* promise,
    const NodeGenerator& handled_by) {
  Label done(this);
  GotoIfNot(condition, &done);
  GotoIf(TaggedIsSmi(promise), &done);
  GotoIfNot(HasInstanceType(promise, JS_PROMISE_TYPE), &done);
  SetPropertyStrict(CAST(context), CAST(promise),
                    HeapConstant(factory()->promise_handled_by_symbol()),
                    CAST(handled_by()));
  Goto(&done);
  BIND(&done);
}

// ES #sec-promise-reject-functions
TF_BUILTIN(PromiseCapabilityDefaultReject, PromiseBuiltinsAssembler) {
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. Let promise be F.[[Promise]].
  Node* const promise =
      LoadContextElement(context, PromiseBuiltins::kPromiseSlot);

  // 3. Let alreadyResolved be F.[[AlreadyResolved]].
  Label if_already_resolved(this, Label::kDeferred);
  Node* const already_resolved =
      LoadContextElement(context, PromiseBuiltins::kAlreadyResolvedSlot);

  // 4. If alreadyResolved.[[Value]] is true, return undefined.
  GotoIf(IsTrue(already_resolved), &if_already_resolved);

  // 5. Set alreadyResolved.[[Value]] to true.
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kAlreadyResolvedSlot, TrueConstant());

  // 6. Return RejectPromise(promise, reason).
  Node* const debug_event =
      LoadContextElement(context, PromiseBuiltins::kDebugEventSlot);
  Return(CallBuiltin(Builtins::kRejectPromise, context, promise, reason,
                     debug_event));

  BIND(&if_already_resolved);
  {
    Return(CallRuntime(Runtime::kPromiseRejectAfterResolved, context, promise,
                       reason));
  }
}

// ES #sec-promise-resolve-functions
TF_BUILTIN(PromiseCapabilityDefaultResolve, PromiseBuiltinsAssembler) {
  Node* const resolution = Parameter(Descriptor::kResolution);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. Let promise be F.[[Promise]].
  Node* const promise =
      LoadContextElement(context, PromiseBuiltins::kPromiseSlot);

  // 3. Let alreadyResolved be F.[[AlreadyResolved]].
  Label if_already_resolved(this, Label::kDeferred);
  Node* const already_resolved =
      LoadContextElement(context, PromiseBuiltins::kAlreadyResolvedSlot);

  // 4. If alreadyResolved.[[Value]] is true, return undefined.
  GotoIf(IsTrue(already_resolved), &if_already_resolved);

  // 5. Set alreadyResolved.[[Value]] to true.
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kAlreadyResolvedSlot, TrueConstant());

  // The rest of the logic (and the catch prediction) is
  // encapsulated in the dedicated ResolvePromise builtin.
  Return(CallBuiltin(Builtins::kResolvePromise, context, promise, resolution));

  BIND(&if_already_resolved);
  {
    Return(CallRuntime(Runtime::kPromiseResolveAfterResolved, context, promise,
                       resolution));
  }
}

TF_BUILTIN(PromiseConstructorLazyDeoptContinuation, PromiseBuiltinsAssembler) {
  Node* promise = Parameter(Descriptor::kPromise);
  Node* reject = Parameter(Descriptor::kReject);
  Node* exception = Parameter(Descriptor::kException);
  Node* const context = Parameter(Descriptor::kContext);

  Label finally(this);

  GotoIf(IsTheHole(exception), &finally);
  CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
         context, reject, UndefinedConstant(), exception);
  Goto(&finally);

  BIND(&finally);
  Return(promise);
}

// ES6 #sec-promise-executor
TF_BUILTIN(PromiseConstructor, PromiseBuiltinsAssembler) {
  Node* const executor = Parameter(Descriptor::kExecutor);
  Node* const new_target = Parameter(Descriptor::kJSNewTarget);
  Node* const context = Parameter(Descriptor::kContext);
  Isolate* isolate = this->isolate();

  Label if_targetisundefined(this, Label::kDeferred);

  GotoIf(IsUndefined(new_target), &if_targetisundefined);

  Label if_notcallable(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(executor), &if_notcallable);

  Node* const executor_map = LoadMap(executor);
  GotoIfNot(IsCallableMap(executor_map), &if_notcallable);

  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  Node* const is_debug_active = IsDebugActive();
  Label if_targetisnotmodified(this),
      if_targetismodified(this, Label::kDeferred), run_executor(this),
      debug_push(this), if_noaccess(this, Label::kDeferred);

  BranchIfAccessCheckFailed(context, native_context, promise_fun, executor,
                            &if_noaccess);

  Branch(WordEqual(promise_fun, new_target), &if_targetisnotmodified,
         &if_targetismodified);

  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_reject_call, MachineRepresentation::kTagged);
  VARIABLE(var_reason, MachineRepresentation::kTagged);

  BIND(&if_targetisnotmodified);
  {
    Node* const instance = AllocateAndInitJSPromise(context);
    var_result.Bind(instance);
    Goto(&debug_push);
  }

  BIND(&if_targetismodified);
  {
    ConstructorBuiltinsAssembler constructor_assembler(this->state());
    Node* const instance = constructor_assembler.EmitFastNewObject(
        context, promise_fun, new_target);
    PromiseInit(instance);
    var_result.Bind(instance);

    GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &debug_push);
    CallRuntime(Runtime::kPromiseHookInit, context, instance,
                UndefinedConstant());
    Goto(&debug_push);
  }

  BIND(&debug_push);
  {
    GotoIfNot(is_debug_active, &run_executor);
    CallRuntime(Runtime::kDebugPushPromise, context, var_result.value());
    Goto(&run_executor);
  }

  BIND(&run_executor);
  {
    Label out(this), if_rejectpromise(this), debug_pop(this, Label::kDeferred);

    Node *resolve, *reject;
    std::tie(resolve, reject) = CreatePromiseResolvingFunctions(
        var_result.value(), TrueConstant(), native_context);

    Node* const maybe_exception = CallJS(
        CodeFactory::Call(isolate, ConvertReceiverMode::kNullOrUndefined),
        context, executor, UndefinedConstant(), resolve, reject);

    GotoIfException(maybe_exception, &if_rejectpromise, &var_reason);
    Branch(is_debug_active, &debug_pop, &out);

    BIND(&if_rejectpromise);
    {
      CallJS(CodeFactory::Call(isolate, ConvertReceiverMode::kNullOrUndefined),
             context, reject, UndefinedConstant(), var_reason.value());
      Branch(is_debug_active, &debug_pop, &out);
    }

    BIND(&debug_pop);
    {
      CallRuntime(Runtime::kDebugPopPromise, context);
      Goto(&out);
    }
    BIND(&out);
    Return(var_result.value());
  }

  // 1. If NewTarget is undefined, throw a TypeError exception.
  BIND(&if_targetisundefined);
  ThrowTypeError(context, MessageTemplate::kNotAPromise, new_target);

  // 2. If IsCallable(executor) is false, throw a TypeError exception.
  BIND(&if_notcallable);
  ThrowTypeError(context, MessageTemplate::kResolverNotAFunction, executor);

  // Silently fail if the stack looks fishy.
  BIND(&if_noaccess);
  {
    Node* const counter_id =
        SmiConstant(v8::Isolate::kPromiseConstructorReturnedUndefined);
    CallRuntime(Runtime::kIncrementUseCounter, context, counter_id);
    Return(UndefinedConstant());
  }
}

// V8 Extras: v8.createPromise(parent)
TF_BUILTIN(PromiseInternalConstructor, PromiseBuiltinsAssembler) {
  Node* const parent = Parameter(Descriptor::kParent);
  Node* const context = Parameter(Descriptor::kContext);
  Return(AllocateAndInitJSPromise(context, parent));
}

// V8 Extras: v8.rejectPromise(promise, reason)
TF_BUILTIN(PromiseInternalReject, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const context = Parameter(Descriptor::kContext);
  // We pass true to trigger the debugger's on exception handler.
  Return(CallBuiltin(Builtins::kRejectPromise, context, promise, reason,
                     TrueConstant()));
}

// V8 Extras: v8.resolvePromise(promise, resolution)
TF_BUILTIN(PromiseInternalResolve, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const resolution = Parameter(Descriptor::kResolution);
  Node* const context = Parameter(Descriptor::kContext);
  Return(CallBuiltin(Builtins::kResolvePromise, context, promise, resolution));
}

// ES#sec-promise.prototype.then
// Promise.prototype.then ( onFulfilled, onRejected )
TF_BUILTIN(PromisePrototypeThen, PromiseBuiltinsAssembler) {
  // 1. Let promise be the this value.
  Node* const promise = Parameter(Descriptor::kReceiver);
  Node* const on_fulfilled = Parameter(Descriptor::kOnFulfilled);
  Node* const on_rejected = Parameter(Descriptor::kOnRejected);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. If IsPromise(promise) is false, throw a TypeError exception.
  ThrowIfNotInstanceType(context, promise, JS_PROMISE_TYPE,
                         "Promise.prototype.then");

  // 3. Let C be ? SpeciesConstructor(promise, %Promise%).
  Label fast_promise_capability(this), slow_constructor(this, Label::kDeferred),
      slow_promise_capability(this, Label::kDeferred);
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  Node* const promise_map = LoadMap(promise);
  BranchIfPromiseSpeciesLookupChainIntact(
      native_context, promise_map, &fast_promise_capability, &slow_constructor);

  BIND(&slow_constructor);
  Node* const constructor =
      SpeciesConstructor(native_context, promise, promise_fun);
  Branch(WordEqual(constructor, promise_fun), &fast_promise_capability,
         &slow_promise_capability);

  // 4. Let resultCapability be ? NewPromiseCapability(C).
  Label perform_promise_then(this);
  VARIABLE(var_result_promise, MachineRepresentation::kTagged);
  VARIABLE(var_result_promise_or_capability, MachineRepresentation::kTagged);

  BIND(&fast_promise_capability);
  {
    Node* const result_promise = AllocateAndInitJSPromise(context, promise);
    var_result_promise_or_capability.Bind(result_promise);
    var_result_promise.Bind(result_promise);
    Goto(&perform_promise_then);
  }

  BIND(&slow_promise_capability);
  {
    Node* const debug_event = TrueConstant();
    Node* const capability = CallBuiltin(Builtins::kNewPromiseCapability,
                                         context, constructor, debug_event);
    var_result_promise.Bind(
        LoadObjectField(capability, PromiseCapability::kPromiseOffset));
    var_result_promise_or_capability.Bind(capability);
    Goto(&perform_promise_then);
  }

  // 5. Return PerformPromiseThen(promise, onFulfilled, onRejected,
  //    resultCapability).
  BIND(&perform_promise_then);
  {
    // We do some work of the PerformPromiseThen operation here, in that
    // we check the handlers and turn non-callable handlers into undefined.
    // This is because this is the one and only callsite of PerformPromiseThen
    // that has to do this.

    // 3. If IsCallable(onFulfilled) is false, then
    //    a. Set onFulfilled to undefined.
    VARIABLE(var_on_fulfilled, MachineRepresentation::kTagged, on_fulfilled);
    Label if_fulfilled_done(this), if_fulfilled_notcallable(this);
    GotoIf(TaggedIsSmi(on_fulfilled), &if_fulfilled_notcallable);
    Branch(IsCallable(on_fulfilled), &if_fulfilled_done,
           &if_fulfilled_notcallable);
    BIND(&if_fulfilled_notcallable);
    var_on_fulfilled.Bind(UndefinedConstant());
    Goto(&if_fulfilled_done);
    BIND(&if_fulfilled_done);

    // 4. If IsCallable(onRejected) is false, then
    //    a. Set onRejected to undefined.
    VARIABLE(var_on_rejected, MachineRepresentation::kTagged, on_rejected);
    Label if_rejected_done(this), if_rejected_notcallable(this);
    GotoIf(TaggedIsSmi(on_rejected), &if_rejected_notcallable);
    Branch(IsCallable(on_rejected), &if_rejected_done,
           &if_rejected_notcallable);
    BIND(&if_rejected_notcallable);
    var_on_rejected.Bind(UndefinedConstant());
    Goto(&if_rejected_done);
    BIND(&if_rejected_done);

    PerformPromiseThen(context, promise, var_on_fulfilled.value(),
                       var_on_rejected.value(),
                       var_result_promise_or_capability.value());
    Return(var_result_promise.value());
  }
}

// ES#sec-promise.prototype.catch
// Promise.prototype.catch ( onRejected )
TF_BUILTIN(PromisePrototypeCatch, PromiseBuiltinsAssembler) {
  // 1. Let promise be the this value.
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const on_fulfilled = UndefinedConstant();
  Node* const on_rejected = Parameter(Descriptor::kOnRejected);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. Return ? Invoke(promise, "then", « undefined, onRejected »).
  Node* const native_context = LoadNativeContext(context);
  Return(InvokeThen(native_context, receiver, on_fulfilled, on_rejected));
}

// ES #sec-promiseresolvethenablejob
TF_BUILTIN(PromiseResolveThenableJob, PromiseBuiltinsAssembler) {
  Node* const native_context = Parameter(Descriptor::kContext);
  Node* const promise_to_resolve = Parameter(Descriptor::kPromiseToResolve);
  Node* const thenable = Parameter(Descriptor::kThenable);
  Node* const then = Parameter(Descriptor::kThen);

  CSA_ASSERT(this, TaggedIsNotSmi(thenable));
  CSA_ASSERT(this, IsJSReceiver(thenable));
  CSA_ASSERT(this, IsJSPromise(promise_to_resolve));
  CSA_ASSERT(this, IsNativeContext(native_context));

  // We can use a simple optimization here if we know that {then} is the initial
  // Promise.prototype.then method, and {thenable} is a JSPromise whose
  // @@species lookup chain is intact: We can connect {thenable} and
  // {promise_to_resolve} directly in that case and avoid the allocation of a
  // temporary JSPromise and the closures plus context.
  //
  // We take the generic (slow-)path if a PromiseHook is enabled or the debugger
  // is active, to make sure we expose spec compliant behavior.
  Label if_fast(this), if_slow(this, Label::kDeferred);
  Node* const promise_then =
      LoadContextElement(native_context, Context::PROMISE_THEN_INDEX);
  GotoIfNot(WordEqual(then, promise_then), &if_slow);
  Node* const thenable_map = LoadMap(thenable);
  GotoIfNot(IsJSPromiseMap(thenable_map), &if_slow);
  GotoIf(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_slow);
  BranchIfPromiseSpeciesLookupChainIntact(native_context, thenable_map,
                                          &if_fast, &if_slow);

  BIND(&if_fast);
  {
    // We know that the {thenable} is a JSPromise, which doesn't require
    // any special treatment and that {then} corresponds to the initial
    // Promise.prototype.then method. So instead of allocating a temporary
    // JSPromise to connect the {thenable} with the {promise_to_resolve},
    // we can directly schedule the {promise_to_resolve} with default
    // handlers onto the {thenable} promise. This does not only save the
    // JSPromise allocation, but also avoids the allocation of the two
    // resolving closures and the shared context.
    //
    // What happens normally in this case is
    //
    //   resolve, reject = CreateResolvingFunctions(promise_to_resolve)
    //   result_capability = NewPromiseCapability(%Promise%)
    //   PerformPromiseThen(thenable, resolve, reject, result_capability)
    //
    // which means that PerformPromiseThen will either schedule a new
    // PromiseReaction with resolve and reject or a PromiseReactionJob
    // with resolve or reject based on the state of {thenable}. And
    // resolve or reject will just invoke the default [[Resolve]] or
    // [[Reject]] functions on the {promise_to_resolve}.
    //
    // This is the same as just doing
    //
    //   PerformPromiseThen(thenable, undefined, undefined, promise_to_resolve)
    //
    // which performs exactly the same (observable) steps.
    TailCallBuiltin(Builtins::kPerformPromiseThen, native_context, thenable,
                    UndefinedConstant(), UndefinedConstant(),
                    promise_to_resolve);
  }

  BIND(&if_slow);
  {
    Node* resolve = nullptr;
    Node* reject = nullptr;
    std::tie(resolve, reject) = CreatePromiseResolvingFunctions(
        promise_to_resolve, FalseConstant(), native_context);

    Label if_exception(this, Label::kDeferred);
    VARIABLE(var_exception, MachineRepresentation::kTagged, TheHoleConstant());
    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        native_context, then, thenable, resolve, reject);
    GotoIfException(result, &if_exception, &var_exception);
    Return(result);

    BIND(&if_exception);
    {
      // We need to reject the {thenable}.
      Node* const result = CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          native_context, reject, UndefinedConstant(), var_exception.value());
      Return(result);
    }
  }
}

// ES #sec-promisereactionjob
void PromiseBuiltinsAssembler::PromiseReactionJob(Node* context, Node* argument,
                                                  Node* handler,
                                                  Node* promise_or_capability,
                                                  PromiseReaction::Type type) {
  CSA_ASSERT(this, TaggedIsNotSmi(handler));
  CSA_ASSERT(this, Word32Or(IsUndefined(handler), IsCallable(handler)));
  CSA_ASSERT(this, TaggedIsNotSmi(promise_or_capability));
  CSA_ASSERT(this,
             Word32Or(Word32Or(IsJSPromise(promise_or_capability),
                               IsPromiseCapability(promise_or_capability)),
                      IsUndefined(promise_or_capability)));

  VARIABLE(var_handler_result, MachineRepresentation::kTagged, argument);
  Label if_handler_callable(this), if_fulfill(this), if_reject(this),
      if_internal(this);
  Branch(IsUndefined(handler),
         type == PromiseReaction::kFulfill ? &if_fulfill : &if_reject,
         &if_handler_callable);

  BIND(&if_handler_callable);
  {
    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
        context, handler, UndefinedConstant(), argument);
    GotoIfException(result, &if_reject, &var_handler_result);
    var_handler_result.Bind(result);
    Branch(IsUndefined(promise_or_capability), &if_internal, &if_fulfill);
  }

  BIND(&if_internal);
  {
    // There's no [[Capability]] for this promise reaction job, which
    // means that this is a specification-internal operation (aka await)
    // where the result does not matter (see the specification change in
    // https://github.com/tc39/ecma262/pull/1146 for details).
    Return(UndefinedConstant());
  }

  BIND(&if_fulfill);
  {
    Label if_promise(this), if_promise_capability(this, Label::kDeferred);
    Node* const value = var_handler_result.value();
    Branch(IsPromiseCapability(promise_or_capability), &if_promise_capability,
           &if_promise);

    BIND(&if_promise);
    {
      // For fast native promises we can skip the indirection
      // via the promiseCapability.[[Resolve]] function and
      // run the resolve logic directly from here.
      TailCallBuiltin(Builtins::kResolvePromise, context, promise_or_capability,
                      value);
    }

    BIND(&if_promise_capability);
    {
      // In the general case we need to call the (user provided)
      // promiseCapability.[[Resolve]] function.
      Node* const resolve = LoadObjectField(promise_or_capability,
                                            PromiseCapability::kResolveOffset);
      Node* const result = CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          context, resolve, UndefinedConstant(), value);
      GotoIfException(result, &if_reject, &var_handler_result);
      Return(result);
    }
  }

  BIND(&if_reject);
  if (type == PromiseReaction::kReject) {
    Label if_promise(this), if_promise_capability(this, Label::kDeferred);
    Node* const reason = var_handler_result.value();
    Branch(IsPromiseCapability(promise_or_capability), &if_promise_capability,
           &if_promise);

    BIND(&if_promise);
    {
      // For fast native promises we can skip the indirection
      // via the promiseCapability.[[Reject]] function and
      // run the resolve logic directly from here.
      TailCallBuiltin(Builtins::kRejectPromise, context, promise_or_capability,
                      reason, FalseConstant());
    }

    BIND(&if_promise_capability);
    {
      // In the general case we need to call the (user provided)
      // promiseCapability.[[Reject]] function.
      Label if_exception(this, Label::kDeferred);
      VARIABLE(var_exception, MachineRepresentation::kTagged,
               TheHoleConstant());
      Node* const reject = LoadObjectField(promise_or_capability,
                                           PromiseCapability::kRejectOffset);
      Node* const result = CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          context, reject, UndefinedConstant(), reason);
      GotoIfException(result, &if_exception, &var_exception);
      Return(result);

      // Swallow the exception here.
      BIND(&if_exception);
      TailCallRuntime(Runtime::kReportMessage, context, var_exception.value());
    }
  } else {
    // We have to call out to the dedicated PromiseRejectReactionJob builtin
    // here, instead of just doing the work inline, as otherwise the catch
    // predictions in the debugger will be wrong, which just walks the stack
    // and checks for certain builtins.
    TailCallBuiltin(Builtins::kPromiseRejectReactionJob, context,
                    var_handler_result.value(), UndefinedConstant(),
                    promise_or_capability);
  }
}

// ES #sec-promisereactionjob
TF_BUILTIN(PromiseFulfillReactionJob, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const handler = Parameter(Descriptor::kHandler);
  Node* const promise_or_capability =
      Parameter(Descriptor::kPromiseOrCapability);

  PromiseReactionJob(context, value, handler, promise_or_capability,
                     PromiseReaction::kFulfill);
}

// ES #sec-promisereactionjob
TF_BUILTIN(PromiseRejectReactionJob, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const handler = Parameter(Descriptor::kHandler);
  Node* const promise_or_capability =
      Parameter(Descriptor::kPromiseOrCapability);

  PromiseReactionJob(context, reason, handler, promise_or_capability,
                     PromiseReaction::kReject);
}

TF_BUILTIN(PromiseResolveTrampoline, PromiseBuiltinsAssembler) {
  //  1. Let C be the this value.
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);

  // 2. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "PromiseResolve");

  // 3. Return ? PromiseResolve(C, x).
  Return(CallBuiltin(Builtins::kPromiseResolve, context, receiver, value));
}

TF_BUILTIN(PromiseResolve, PromiseBuiltinsAssembler) {
  Node* constructor = Parameter(Descriptor::kConstructor);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, IsJSReceiver(constructor));

  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);

  Label if_slow_constructor(this, Label::kDeferred), if_need_to_allocate(this);

  // Check if {value} is a JSPromise.
  GotoIf(TaggedIsSmi(value), &if_need_to_allocate);
  Node* const value_map = LoadMap(value);
  GotoIfNot(IsJSPromiseMap(value_map), &if_need_to_allocate);

  // We can skip the "constructor" lookup on {value} if it's [[Prototype]]
  // is the (initial) Promise.prototype and the @@species protector is
  // intact, as that guards the lookup path for "constructor" on
  // JSPromise instances which have the (initial) Promise.prototype.
  Node* const promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfNot(WordEqual(LoadMapPrototype(value_map), promise_prototype),
            &if_slow_constructor);
  GotoIf(IsPromiseSpeciesProtectorCellInvalid(), &if_slow_constructor);

  // If the {constructor} is the Promise function, we just immediately
  // return the {value} here and don't bother wrapping it into a
  // native Promise.
  GotoIfNot(WordEqual(promise_fun, constructor), &if_slow_constructor);
  Return(value);

  // At this point, value or/and constructor are not native promises, but
  // they could be of the same subclass.
  BIND(&if_slow_constructor);
  {
    Node* const value_constructor =
        GetProperty(context, value, isolate()->factory()->constructor_string());
    GotoIfNot(WordEqual(value_constructor, constructor), &if_need_to_allocate);
    Return(value);
  }

  BIND(&if_need_to_allocate);
  {
    Label if_nativepromise(this), if_notnativepromise(this, Label::kDeferred);
    Branch(WordEqual(promise_fun, constructor), &if_nativepromise,
           &if_notnativepromise);

    // This adds a fast path for native promises that don't need to
    // create NewPromiseCapability.
    BIND(&if_nativepromise);
    {
      Node* const result = AllocateAndInitJSPromise(context);
      CallBuiltin(Builtins::kResolvePromise, context, result, value);
      Return(result);
    }

    BIND(&if_notnativepromise);
    {
      Node* const debug_event = TrueConstant();
      Node* const capability = CallBuiltin(Builtins::kNewPromiseCapability,
                                           context, constructor, debug_event);

      Node* const resolve =
          LoadObjectField(capability, PromiseCapability::kResolveOffset);
      CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          context, resolve, UndefinedConstant(), value);

      Node* const result =
          LoadObjectField(capability, PromiseCapability::kPromiseOffset);
      Return(result);
    }
  }
}

// ES6 #sec-getcapabilitiesexecutor-functions
TF_BUILTIN(PromiseGetCapabilitiesExecutor, PromiseBuiltinsAssembler) {
  Node* const resolve = Parameter(Descriptor::kResolve);
  Node* const reject = Parameter(Descriptor::kReject);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const capability =
      LoadContextElement(context, PromiseBuiltins::kCapabilitySlot);

  Label if_alreadyinvoked(this, Label::kDeferred);
  GotoIfNot(IsUndefined(
                LoadObjectField(capability, PromiseCapability::kResolveOffset)),
            &if_alreadyinvoked);
  GotoIfNot(IsUndefined(
                LoadObjectField(capability, PromiseCapability::kRejectOffset)),
            &if_alreadyinvoked);

  StoreObjectField(capability, PromiseCapability::kResolveOffset, resolve);
  StoreObjectField(capability, PromiseCapability::kRejectOffset, reject);

  Return(UndefinedConstant());

  BIND(&if_alreadyinvoked);
  ThrowTypeError(context, MessageTemplate::kPromiseExecutorAlreadyInvoked);
}

TF_BUILTIN(PromiseReject, PromiseBuiltinsAssembler) {
  // 1. Let C be the this value.
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "PromiseReject");

  Label if_nativepromise(this), if_custompromise(this, Label::kDeferred);
  Node* const native_context = LoadNativeContext(context);

  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  Branch(WordEqual(promise_fun, receiver), &if_nativepromise,
         &if_custompromise);

  BIND(&if_nativepromise);
  {
    Node* const promise =
        AllocateAndSetJSPromise(context, v8::Promise::kRejected, reason);
    CallRuntime(Runtime::kPromiseRejectEventFromStack, context, promise,
                reason);
    Return(promise);
  }

  BIND(&if_custompromise);
  {
    // 3. Let promiseCapability be ? NewPromiseCapability(C).
    Node* const debug_event = TrueConstant();
    Node* const capability = CallBuiltin(Builtins::kNewPromiseCapability,
                                         context, receiver, debug_event);

    // 4. Perform ? Call(promiseCapability.[[Reject]], undefined, « r »).
    Node* const reject =
        LoadObjectField(capability, PromiseCapability::kRejectOffset);
    CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
           context, reject, UndefinedConstant(), reason);

    // 5. Return promiseCapability.[[Promise]].
    Node* const promise =
        LoadObjectField(capability, PromiseCapability::kPromiseOffset);
    Return(promise);
  }
}

std::pair<Node*, Node*> PromiseBuiltinsAssembler::CreatePromiseFinallyFunctions(
    Node* on_finally, Node* constructor, Node* native_context) {
  Node* const promise_context = CreatePromiseContext(
      native_context, PromiseBuiltins::kPromiseFinallyContextLength);
  StoreContextElementNoWriteBarrier(
      promise_context, PromiseBuiltins::kOnFinallySlot, on_finally);
  StoreContextElementNoWriteBarrier(
      promise_context, PromiseBuiltins::kConstructorSlot, constructor);
  Node* const map = LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
  Node* const then_finally_info = LoadContextElement(
      native_context, Context::PROMISE_THEN_FINALLY_SHARED_FUN);
  Node* const then_finally = AllocateFunctionWithMapAndContext(
      map, then_finally_info, promise_context);
  Node* const catch_finally_info = LoadContextElement(
      native_context, Context::PROMISE_CATCH_FINALLY_SHARED_FUN);
  Node* const catch_finally = AllocateFunctionWithMapAndContext(
      map, catch_finally_info, promise_context);
  return std::make_pair(then_finally, catch_finally);
}

TF_BUILTIN(PromiseValueThunkFinally, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);

  Node* const value = LoadContextElement(context, PromiseBuiltins::kValueSlot);
  Return(value);
}

Node* PromiseBuiltinsAssembler::CreateValueThunkFunction(Node* value,
                                                         Node* native_context) {
  Node* const value_thunk_context = CreatePromiseContext(
      native_context, PromiseBuiltins::kPromiseValueThunkOrReasonContextLength);
  StoreContextElementNoWriteBarrier(value_thunk_context,
                                    PromiseBuiltins::kValueSlot, value);
  Node* const map = LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
  Node* const value_thunk_info = LoadContextElement(
      native_context, Context::PROMISE_VALUE_THUNK_FINALLY_SHARED_FUN);
  Node* const value_thunk = AllocateFunctionWithMapAndContext(
      map, value_thunk_info, value_thunk_context);
  return value_thunk;
}

TF_BUILTIN(PromiseThenFinally, PromiseBuiltinsAssembler) {
  CSA_ASSERT_JS_ARGC_EQ(this, 1);

  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  // 1. Let onFinally be F.[[OnFinally]].
  Node* const on_finally =
      LoadContextElement(context, PromiseBuiltins::kOnFinallySlot);

  // 2.  Assert: IsCallable(onFinally) is true.
  CSA_ASSERT(this, IsCallable(on_finally));

  // 3. Let result be ?  Call(onFinally).
  Node* const result = CallJS(
      CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
      context, on_finally, UndefinedConstant());

  // 4. Let C be F.[[Constructor]].
  Node* const constructor =
      LoadContextElement(context, PromiseBuiltins::kConstructorSlot);

  // 5. Assert: IsConstructor(C) is true.
  CSA_ASSERT(this, IsConstructor(constructor));

  // 6. Let promise be ? PromiseResolve(C, result).
  Node* const promise =
    CallBuiltin(Builtins::kPromiseResolve, context, constructor, result);

  // 7. Let valueThunk be equivalent to a function that returns value.
  Node* const native_context = LoadNativeContext(context);
  Node* const value_thunk = CreateValueThunkFunction(value, native_context);

  // 8. Return ? Invoke(promise, "then", « valueThunk »).
  Return(InvokeThen(native_context, promise, value_thunk));
}

TF_BUILTIN(PromiseThrowerFinally, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);

  Node* const reason = LoadContextElement(context, PromiseBuiltins::kValueSlot);
  CallRuntime(Runtime::kThrow, context, reason);
  Unreachable();
}

Node* PromiseBuiltinsAssembler::CreateThrowerFunction(Node* reason,
                                                      Node* native_context) {
  Node* const thrower_context = CreatePromiseContext(
      native_context, PromiseBuiltins::kPromiseValueThunkOrReasonContextLength);
  StoreContextElementNoWriteBarrier(thrower_context,
                                    PromiseBuiltins::kValueSlot, reason);
  Node* const map = LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
  Node* const thrower_info = LoadContextElement(
      native_context, Context::PROMISE_THROWER_FINALLY_SHARED_FUN);
  Node* const thrower =
      AllocateFunctionWithMapAndContext(map, thrower_info, thrower_context);
  return thrower;
}

TF_BUILTIN(PromiseCatchFinally, PromiseBuiltinsAssembler) {
  CSA_ASSERT_JS_ARGC_EQ(this, 1);

  Node* const reason = Parameter(Descriptor::kReason);
  Node* const context = Parameter(Descriptor::kContext);

  // 1. Let onFinally be F.[[OnFinally]].
  Node* const on_finally =
      LoadContextElement(context, PromiseBuiltins::kOnFinallySlot);

  // 2. Assert: IsCallable(onFinally) is true.
  CSA_ASSERT(this, IsCallable(on_finally));

  // 3. Let result be ? Call(onFinally).
  Node* result = CallJS(
      CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
      context, on_finally, UndefinedConstant());

  // 4. Let C be F.[[Constructor]].
  Node* const constructor =
      LoadContextElement(context, PromiseBuiltins::kConstructorSlot);

  // 5. Assert: IsConstructor(C) is true.
  CSA_ASSERT(this, IsConstructor(constructor));

  // 6. Let promise be ? PromiseResolve(C, result).
  Node* const promise =
    CallBuiltin(Builtins::kPromiseResolve, context, constructor, result);

  // 7. Let thrower be equivalent to a function that throws reason.
  Node* const native_context = LoadNativeContext(context);
  Node* const thrower = CreateThrowerFunction(reason, native_context);

  // 8. Return ? Invoke(promise, "then", « thrower »).
  Return(InvokeThen(native_context, promise, thrower));
}

TF_BUILTIN(PromisePrototypeFinally, PromiseBuiltinsAssembler) {
  CSA_ASSERT_JS_ARGC_EQ(this, 1);

  // 1.  Let promise be the this value.
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const on_finally = Parameter(Descriptor::kOnFinally);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. If Type(promise) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "Promise.prototype.finally");

  // 3. Let C be ? SpeciesConstructor(promise, %Promise%).
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  VARIABLE(var_constructor, MachineRepresentation::kTagged, promise_fun);
  Label slow_constructor(this, Label::kDeferred), done_constructor(this);
  Node* const receiver_map = LoadMap(receiver);
  GotoIfNot(IsJSPromiseMap(receiver_map), &slow_constructor);
  BranchIfPromiseSpeciesLookupChainIntact(native_context, receiver_map,
                                          &done_constructor, &slow_constructor);
  BIND(&slow_constructor);
  {
    Node* const constructor =
        SpeciesConstructor(context, receiver, promise_fun);
    var_constructor.Bind(constructor);
    Goto(&done_constructor);
  }
  BIND(&done_constructor);
  Node* const constructor = var_constructor.value();

  // 4. Assert: IsConstructor(C) is true.
  CSA_ASSERT(this, IsConstructor(constructor));

  VARIABLE(var_then_finally, MachineRepresentation::kTagged);
  VARIABLE(var_catch_finally, MachineRepresentation::kTagged);

  Label if_notcallable(this, Label::kDeferred), perform_finally(this);

  GotoIf(TaggedIsSmi(on_finally), &if_notcallable);
  GotoIfNot(IsCallable(on_finally), &if_notcallable);

  // 6. Else,
  //   a. Let thenFinally be a new built-in function object as defined
  //   in ThenFinally Function.
  //   b. Let catchFinally be a new built-in function object as
  //   defined in CatchFinally Function.
  //   c. Set thenFinally and catchFinally's [[Constructor]] internal
  //   slots to C.
  //   d. Set thenFinally and catchFinally's [[OnFinally]] internal
  //   slots to onFinally.
  Node* then_finally = nullptr;
  Node* catch_finally = nullptr;
  std::tie(then_finally, catch_finally) =
    CreatePromiseFinallyFunctions(on_finally, constructor, native_context);
  var_then_finally.Bind(then_finally);
  var_catch_finally.Bind(catch_finally);
  Goto(&perform_finally);

  // 5. If IsCallable(onFinally) is not true,
  //    a. Let thenFinally be onFinally.
  //    b. Let catchFinally be onFinally.
  BIND(&if_notcallable);
  {
    var_then_finally.Bind(on_finally);
    var_catch_finally.Bind(on_finally);
    Goto(&perform_finally);
  }

  // 7. Return ? Invoke(promise, "then", « thenFinally, catchFinally »).
  BIND(&perform_finally);
  Return(InvokeThen(native_context, receiver, var_then_finally.value(),
                    var_catch_finally.value()));
}

// ES #sec-fulfillpromise
TF_BUILTIN(FulfillPromise, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, TaggedIsNotSmi(promise));
  CSA_ASSERT(this, IsJSPromise(promise));

  // 2. Let reactions be promise.[[PromiseFulfillReactions]].
  Node* const reactions =
      LoadObjectField(promise, JSPromise::kReactionsOrResultOffset);

  // 3. Set promise.[[PromiseResult]] to value.
  // 4. Set promise.[[PromiseFulfillReactions]] to undefined.
  // 5. Set promise.[[PromiseRejectReactions]] to undefined.
  StoreObjectField(promise, JSPromise::kReactionsOrResultOffset, value);

  // 6. Set promise.[[PromiseState]] to "fulfilled".
  PromiseSetStatus(promise, Promise::kFulfilled);

  // 7. Return TriggerPromiseReactions(reactions, value).
  Return(TriggerPromiseReactions(context, reactions, value,
                                 PromiseReaction::kFulfill));
}

// ES #sec-rejectpromise
TF_BUILTIN(RejectPromise, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const debug_event = Parameter(Descriptor::kDebugEvent);
  Node* const context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, TaggedIsNotSmi(promise));
  CSA_ASSERT(this, IsJSPromise(promise));
  CSA_ASSERT(this, IsBoolean(debug_event));
  Label if_runtime(this, Label::kDeferred);

  // If promise hook is enabled or the debugger is active, let
  // the runtime handle this operation, which greatly reduces
  // the complexity here and also avoids a couple of back and
  // forth between JavaScript and C++ land.
  GotoIf(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_runtime);

  // 7. If promise.[[PromiseIsHandled]] is false, perform
  //    HostPromiseRejectionTracker(promise, "reject").
  // We don't try to handle rejecting {promise} without handler
  // here, but we let the C++ code take care of this completely.
  GotoIfNot(PromiseHasHandler(promise), &if_runtime);

  // 2. Let reactions be promise.[[PromiseRejectReactions]].
  Node* reactions =
      LoadObjectField(promise, JSPromise::kReactionsOrResultOffset);

  // 3. Set promise.[[PromiseResult]] to reason.
  // 4. Set promise.[[PromiseFulfillReactions]] to undefined.
  // 5. Set promise.[[PromiseRejectReactions]] to undefined.
  StoreObjectField(promise, JSPromise::kReactionsOrResultOffset, reason);

  // 6. Set promise.[[PromiseState]] to "rejected".
  PromiseSetStatus(promise, Promise::kRejected);

  // 7. Return TriggerPromiseReactions(reactions, reason).
  Return(TriggerPromiseReactions(context, reactions, reason,
                                 PromiseReaction::kReject));

  BIND(&if_runtime);
  TailCallRuntime(Runtime::kRejectPromise, context, promise, reason,
                  debug_event);
}

// ES #sec-promise-resolve-functions
TF_BUILTIN(ResolvePromise, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const resolution = Parameter(Descriptor::kResolution);
  Node* const context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, TaggedIsNotSmi(promise));
  CSA_ASSERT(this, IsJSPromise(promise));

  Label do_enqueue(this), if_fulfill(this), if_reject(this, Label::kDeferred),
      if_runtime(this, Label::kDeferred);
  VARIABLE(var_reason, MachineRepresentation::kTagged);
  VARIABLE(var_then, MachineRepresentation::kTagged);

  // If promise hook is enabled or the debugger is active, let
  // the runtime handle this operation, which greatly reduces
  // the complexity here and also avoids a couple of back and
  // forth between JavaScript and C++ land.
  GotoIf(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_runtime);

  // 6. If SameValue(resolution, promise) is true, then
  // We can use pointer comparison here, since the {promise} is guaranteed
  // to be a JSPromise inside this function and thus is reference comparable.
  GotoIf(WordEqual(promise, resolution), &if_runtime);

  // 7. If Type(resolution) is not Object, then
  GotoIf(TaggedIsSmi(resolution), &if_fulfill);
  Node* const resolution_map = LoadMap(resolution);
  GotoIfNot(IsJSReceiverMap(resolution_map), &if_fulfill);

  // We can skip the "then" lookup on {resolution} if its [[Prototype]]
  // is the (initial) Promise.prototype and the Promise#then protector
  // is intact, as that guards the lookup path for the "then" property
  // on JSPromise instances which have the (initial) %PromisePrototype%.
  Label if_fast(this), if_receiver(this), if_slow(this, Label::kDeferred);
  Node* const native_context = LoadNativeContext(context);
  GotoIfForceSlowPath(&if_slow);
  GotoIf(IsPromiseThenProtectorCellInvalid(), &if_slow);
  GotoIfNot(IsJSPromiseMap(resolution_map), &if_receiver);
  Node* const promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  Branch(WordEqual(LoadMapPrototype(resolution_map), promise_prototype),
         &if_fast, &if_slow);

  BIND(&if_fast);
  {
    // The {resolution} is a native Promise in this case.
    Node* const then =
        LoadContextElement(native_context, Context::PROMISE_THEN_INDEX);
    var_then.Bind(then);
    Goto(&do_enqueue);
  }

  BIND(&if_receiver);
  {
    // We can skip the lookup of "then" if the {resolution} is a (newly
    // created) IterResultObject, as the Promise#then() protector also
    // ensures that the intrinsic %ObjectPrototype% doesn't contain any
    // "then" property. This helps to avoid negative lookups on iterator
    // results from async generators.
    CSA_ASSERT(this, IsJSReceiverMap(resolution_map));
    CSA_ASSERT(this, Word32BinaryNot(IsPromiseThenProtectorCellInvalid()));
    Node* const iterator_result_map =
        LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
    Branch(WordEqual(resolution_map, iterator_result_map), &if_fulfill,
           &if_slow);
  }

  BIND(&if_slow);
  {
    // 8. Let then be Get(resolution, "then").
    Node* const then =
        GetProperty(context, resolution, isolate()->factory()->then_string());

    // 9. If then is an abrupt completion, then
    GotoIfException(then, &if_reject, &var_reason);

    // 11. If IsCallable(thenAction) is false, then
    GotoIf(TaggedIsSmi(then), &if_fulfill);
    Node* const then_map = LoadMap(then);
    GotoIfNot(IsCallableMap(then_map), &if_fulfill);
    var_then.Bind(then);
    Goto(&do_enqueue);
  }

  BIND(&do_enqueue);
  {
    // 12. Perform EnqueueJob("PromiseJobs", PromiseResolveThenableJob,
    //                        «promise, resolution, thenAction»).
    Node* const task = AllocatePromiseResolveThenableJobTask(
        promise, var_then.value(), resolution, native_context);
    TailCallBuiltin(Builtins::kEnqueueMicrotask, native_context, task);
  }

  BIND(&if_fulfill);
  {
    // 7.b Return FulfillPromise(promise, resolution).
    TailCallBuiltin(Builtins::kFulfillPromise, context, promise, resolution);
  }

  BIND(&if_runtime);
  Return(CallRuntime(Runtime::kResolvePromise, context, promise, resolution));

  BIND(&if_reject);
  {
    // 9.a Return RejectPromise(promise, then.[[Value]]).
    TailCallBuiltin(Builtins::kRejectPromise, context, promise,
                    var_reason.value(), FalseConstant());
  }
}

Node* PromiseBuiltinsAssembler::PerformPromiseAll(
    Node* context, Node* constructor, Node* capability,
    const IteratorRecord& iterator, Label* if_exception,
    Variable* var_exception) {
  IteratorBuiltinsAssembler iter_assembler(state());

  Node* const native_context = LoadNativeContext(context);

  // For catch prediction, don't treat the .then calls as handling it;
  // instead, recurse outwards.
  SetForwardingHandlerIfTrue(
      native_context, IsDebugActive(),
      LoadObjectField(capability, PromiseCapability::kRejectOffset));

  Node* const resolve_element_context =
      CreatePromiseAllResolveElementContext(capability, native_context);

  TVARIABLE(Smi, var_index, SmiConstant(1));
  Label loop(this, &var_index), done_loop(this),
      too_many_elements(this, Label::kDeferred),
      close_iterator(this, Label::kDeferred);
  Goto(&loop);
  BIND(&loop);
  {
    // Let next be IteratorStep(iteratorRecord.[[Iterator]]).
    // If next is an abrupt completion, set iteratorRecord.[[Done]] to true.
    // ReturnIfAbrupt(next).
    Node* const fast_iterator_result_map =
        LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
    Node* const next = iter_assembler.IteratorStep(
        native_context, iterator, &done_loop, fast_iterator_result_map,
        if_exception, var_exception);

    // Let nextValue be IteratorValue(next).
    // If nextValue is an abrupt completion, set iteratorRecord.[[Done]] to
    //     true.
    // ReturnIfAbrupt(nextValue).
    Node* const next_value = iter_assembler.IteratorValue(
        native_context, next, fast_iterator_result_map, if_exception,
        var_exception);

    // Check if we reached the limit.
    TNode<Smi> const index = var_index.value();
    GotoIf(SmiEqual(index, SmiConstant(PropertyArray::HashField::kMax)),
           &too_many_elements);

    // Set index to index + 1.
    var_index = SmiAdd(index, SmiConstant(1));

    // Set remainingElementsCount.[[Value]] to
    //     remainingElementsCount.[[Value]] + 1.
    TNode<Smi> const remaining_elements_count = CAST(LoadContextElement(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot));
    StoreContextElementNoWriteBarrier(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
        SmiAdd(remaining_elements_count, SmiConstant(1)));

    // Let resolveElement be CreateBuiltinFunction(steps,
    //                                             « [[AlreadyCalled]],
    //                                               [[Index]],
    //                                               [[Values]],
    //                                               [[Capability]],
    //                                               [[RemainingElements]] »).
    // Set resolveElement.[[AlreadyCalled]] to a Record { [[Value]]: false }.
    // Set resolveElement.[[Index]] to index.
    // Set resolveElement.[[Values]] to values.
    // Set resolveElement.[[Capability]] to resultCapability.
    // Set resolveElement.[[RemainingElements]] to remainingElementsCount.
    Node* const resolve_element_fun = CreatePromiseAllResolveElementFunction(
        resolve_element_context, index, native_context);

    // We can skip the "resolve" lookup on the {constructor} as well as the
    // "then" lookup on the result of the "resolve" call, and immediately
    // chain continuation onto the {next_value} if:
    //
    //   (a) The {constructor} is the intrinsic %Promise% function, and
    //       looking up "resolve" on {constructor} yields the initial
    //       Promise.resolve() builtin, and
    //   (b) the promise @@species protector cell is valid, meaning that
    //       no one messed with the Symbol.species property on any
    //       intrinsic promise or on the Promise.prototype, and
    //   (c) the {next_value} is a JSPromise whose [[Prototype]] field
    //       contains the intrinsic %PromisePrototype%, and
    //   (d) we're not running with async_hooks or DevTools enabled.
    //
    // In that case we also don't need to allocate a chained promise for
    // the PromiseReaction (aka we can pass undefined to PerformPromiseThen),
    // since this is only necessary for DevTools and PromiseHooks.
    Label if_fast(this), if_slow(this);
    GotoIfNotPromiseResolveLookupChainIntact(native_context, constructor,
                                             &if_slow);
    GotoIf(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
           &if_slow);
    GotoIf(IsPromiseSpeciesProtectorCellInvalid(), &if_slow);
    GotoIf(TaggedIsSmi(next_value), &if_slow);
    Node* const next_value_map = LoadMap(next_value);
    BranchIfPromiseThenLookupChainIntact(native_context, next_value_map,
                                         &if_fast, &if_slow);

    BIND(&if_fast);
    {
      // Register the PromiseReaction immediately on the {next_value}, not
      // passing any chained promise since neither async_hooks nor DevTools
      // are enabled, so there's no use of the resulting promise.
      PerformPromiseThen(
          native_context, next_value, resolve_element_fun,
          LoadObjectField(capability, PromiseCapability::kRejectOffset),
          UndefinedConstant());
      Goto(&loop);
    }

    BIND(&if_slow);
    {
      // Let nextPromise be ? Invoke(constructor, "resolve", « nextValue »).
      Node* const next_promise =
          InvokeResolve(native_context, constructor, next_value,
                        &close_iterator, var_exception);

      // Perform ? Invoke(nextPromise, "then", « resolveElement,
      //                  resultCapability.[[Reject]] »).
      Node* const then =
          GetProperty(native_context, next_promise, factory()->then_string());
      GotoIfException(then, &close_iterator, var_exception);

      Node* const then_call =
          CallJS(CodeFactory::Call(isolate(),
                                   ConvertReceiverMode::kNotNullOrUndefined),
                 native_context, then, next_promise, resolve_element_fun,
                 LoadObjectField(capability, PromiseCapability::kRejectOffset));
      GotoIfException(then_call, &close_iterator, var_exception);

      // For catch prediction, mark that rejections here are semantically
      // handled by the combined Promise.
      SetPromiseHandledByIfTrue(
          native_context, IsDebugActive(), then_call, [=]() {
            // Load promiseCapability.[[Promise]]
            return LoadObjectField(capability,
                                   PromiseCapability::kPromiseOffset);
          });

      Goto(&loop);
    }
  }

  BIND(&too_many_elements);
  {
    // If there are too many elements (currently more than 2**21-1), raise a
    // RangeError here (which is caught directly and turned into a rejection)
    // of the resulting promise. We could gracefully handle this case as well
    // and support more than this number of elements by going to a separate
    // function and pass the larger indices via a separate context, but it
    // doesn't seem likely that we need this, and it's unclear how the rest
    // of the system deals with 2**21 live Promises anyways.
    Node* const result =
        CallRuntime(Runtime::kThrowRangeError, native_context,
                    SmiConstant(MessageTemplate::kTooManyElementsInPromiseAll));
    GotoIfException(result, &close_iterator, var_exception);
    Unreachable();
  }

  BIND(&close_iterator);
  {
    // Exception must be bound to a JS value.
    CSA_ASSERT(this, IsNotTheHole(var_exception->value()));
    iter_assembler.IteratorCloseOnException(native_context, iterator,
                                            if_exception, var_exception);
  }

  BIND(&done_loop);
  {
    Label resolve_promise(this, Label::kDeferred), return_promise(this);
    // Set iteratorRecord.[[Done]] to true.
    // Set remainingElementsCount.[[Value]] to
    //    remainingElementsCount.[[Value]] - 1.
    TNode<Smi> remaining_elements_count = CAST(LoadContextElement(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot));
    remaining_elements_count = SmiSub(remaining_elements_count, SmiConstant(1));
    StoreContextElementNoWriteBarrier(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
        remaining_elements_count);
    GotoIf(SmiEqual(remaining_elements_count, SmiConstant(0)),
           &resolve_promise);

    // Pre-allocate the backing store for the {values_array} to the desired
    // capacity here. We may already have elements here in case of some
    // fancy Thenable that calls the resolve callback immediately, so we need
    // to handle that correctly here.
    Node* const values_array = LoadContextElement(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot);
    Node* const old_elements = LoadElements(values_array);
    TNode<Smi> const old_capacity = LoadFixedArrayBaseLength(old_elements);
    TNode<Smi> const new_capacity = var_index.value();
    GotoIf(SmiGreaterThanOrEqual(old_capacity, new_capacity), &return_promise);
    Node* const new_elements =
        AllocateFixedArray(PACKED_ELEMENTS, new_capacity, SMI_PARAMETERS,
                           AllocationFlag::kAllowLargeObjectAllocation);
    CopyFixedArrayElements(PACKED_ELEMENTS, old_elements, PACKED_ELEMENTS,
                           new_elements, SmiConstant(0), old_capacity,
                           new_capacity, UPDATE_WRITE_BARRIER, SMI_PARAMETERS);
    StoreObjectField(values_array, JSArray::kElementsOffset, new_elements);
    Goto(&return_promise);

    // If remainingElementsCount.[[Value]] is 0, then
    //     Let valuesArray be CreateArrayFromList(values).
    //     Perform ? Call(resultCapability.[[Resolve]], undefined,
    //                    « valuesArray »).
    BIND(&resolve_promise);
    {
      Node* const resolve =
          LoadObjectField(capability, PromiseCapability::kResolveOffset);
      Node* const values_array = LoadContextElement(
          resolve_element_context,
          PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot);
      Node* const resolve_call = CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          native_context, resolve, UndefinedConstant(), values_array);
      GotoIfException(resolve_call, if_exception, var_exception);
      Goto(&return_promise);
    }

    // Return resultCapability.[[Promise]].
    BIND(&return_promise);
  }

  Node* const promise =
      LoadObjectField(capability, PromiseCapability::kPromiseOffset);
  return promise;
}

// ES#sec-promise.all
// Promise.all ( iterable )
TF_BUILTIN(PromiseAll, PromiseBuiltinsAssembler) {
  IteratorBuiltinsAssembler iter_assembler(state());

  // Let C be the this value.
  // If Type(C) is not Object, throw a TypeError exception.
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "Promise.all");

  // Let promiseCapability be ? NewPromiseCapability(C).
  // Don't fire debugEvent so that forwarding the rejection through all does not
  // trigger redundant ExceptionEvents
  Node* const debug_event = FalseConstant();
  Node* const capability = CallBuiltin(Builtins::kNewPromiseCapability, context,
                                       receiver, debug_event);

  VARIABLE(var_exception, MachineRepresentation::kTagged, TheHoleConstant());
  Label reject_promise(this, &var_exception, Label::kDeferred);

  // Let iterator be GetIterator(iterable).
  // IfAbruptRejectPromise(iterator, promiseCapability).
  Node* const iterable = Parameter(Descriptor::kIterable);
  IteratorRecord iterator = iter_assembler.GetIterator(
      context, iterable, &reject_promise, &var_exception);

  // Let result be PerformPromiseAll(iteratorRecord, C, promiseCapability).
  // If result is an abrupt completion, then
  //   If iteratorRecord.[[Done]] is false, let result be
  //       IteratorClose(iterator, result).
  //    IfAbruptRejectPromise(result, promiseCapability).
  Node* const result = PerformPromiseAll(
      context, receiver, capability, iterator, &reject_promise, &var_exception);

  Return(result);

  BIND(&reject_promise);
  {
    // Exception must be bound to a JS value.
    CSA_SLOW_ASSERT(this, IsNotTheHole(var_exception.value()));
    Node* const reject =
        LoadObjectField(capability, PromiseCapability::kRejectOffset);
    CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
           context, reject, UndefinedConstant(), var_exception.value());

    Node* const promise =
        LoadObjectField(capability, PromiseCapability::kPromiseOffset);
    Return(promise);
  }
}

TF_BUILTIN(PromiseAllResolveElementClosure, PromiseBuiltinsAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kJSTarget));

  Label already_called(this, Label::kDeferred), resolve_promise(this);

  // We use the {function}s context as the marker to remember whether this
  // resolve element closure was already called. It points to the resolve
  // element context (which is a FunctionContext) until it was called the
  // first time, in which case we make it point to the native context here
  // to mark this resolve element closure as done.
  GotoIf(IsNativeContext(context), &already_called);
  CSA_ASSERT(
      this,
      SmiEqual(LoadObjectField<Smi>(context, Context::kLengthOffset),
               SmiConstant(PromiseBuiltins::kPromiseAllResolveElementLength)));
  TNode<Context> native_context = LoadNativeContext(context);
  StoreObjectField(function, JSFunction::kContextOffset, native_context);

  // Determine the index from the {function}.
  Label unreachable(this, Label::kDeferred);
  STATIC_ASSERT(PropertyArray::kNoHashSentinel == 0);
  TNode<IntPtrT> identity_hash =
      LoadJSReceiverIdentityHash(function, &unreachable);
  CSA_ASSERT(this, IntPtrGreaterThan(identity_hash, IntPtrConstant(0)));
  TNode<IntPtrT> index = IntPtrSub(identity_hash, IntPtrConstant(1));

  // Check if we need to grow the [[ValuesArray]] to store {value} at {index}.
  TNode<JSArray> values_array = CAST(LoadContextElement(
      context, PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot));
  TNode<FixedArray> elements = CAST(LoadElements(values_array));
  TNode<IntPtrT> values_length =
      LoadAndUntagObjectField(values_array, JSArray::kLengthOffset);
  Label if_inbounds(this), if_outofbounds(this), done(this);
  Branch(IntPtrLessThan(index, values_length), &if_inbounds, &if_outofbounds);

  BIND(&if_outofbounds);
  {
    // Check if we need to grow the backing store.
    TNode<IntPtrT> new_length = IntPtrAdd(index, IntPtrConstant(1));
    TNode<IntPtrT> elements_length =
        LoadAndUntagObjectField(elements, FixedArray::kLengthOffset);
    Label if_grow(this, Label::kDeferred), if_nogrow(this);
    Branch(IntPtrLessThan(index, elements_length), &if_nogrow, &if_grow);

    BIND(&if_grow);
    {
      // We need to grow the backing store to fit the {index} as well.
      TNode<IntPtrT> new_elements_length =
          IntPtrMin(CalculateNewElementsCapacity(new_length),
                    IntPtrConstant(PropertyArray::HashField::kMax + 1));
      CSA_ASSERT(this, IntPtrLessThan(index, new_elements_length));
      CSA_ASSERT(this, IntPtrLessThan(elements_length, new_elements_length));
      TNode<FixedArray> new_elements =
          CAST(AllocateFixedArray(PACKED_ELEMENTS, new_elements_length,
                                  AllocationFlag::kAllowLargeObjectAllocation));
      CopyFixedArrayElements(PACKED_ELEMENTS, elements, PACKED_ELEMENTS,
                             new_elements, elements_length,
                             new_elements_length);
      StoreFixedArrayElement(new_elements, index, value);

      // Update backing store and "length" on {values_array}.
      StoreObjectField(values_array, JSArray::kElementsOffset, new_elements);
      StoreObjectFieldNoWriteBarrier(values_array, JSArray::kLengthOffset,
                                     SmiTag(new_length));
      Goto(&done);
    }

    BIND(&if_nogrow);
    {
      // The {index} is within bounds of the {elements} backing store, so
      // just store the {value} and update the "length" of the {values_array}.
      StoreObjectFieldNoWriteBarrier(values_array, JSArray::kLengthOffset,
                                     SmiTag(new_length));
      StoreFixedArrayElement(elements, index, value);
      Goto(&done);
    }
  }

  BIND(&if_inbounds);
  {
    // The {index} is in bounds of the {values_array},
    // just store the {value} and continue.
    StoreFixedArrayElement(elements, index, value);
    Goto(&done);
  }

  BIND(&done);
  TNode<Smi> remaining_elements_count = CAST(LoadContextElement(
      context, PromiseBuiltins::kPromiseAllResolveElementRemainingSlot));
  remaining_elements_count = SmiSub(remaining_elements_count, SmiConstant(1));
  StoreContextElement(context,
                      PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
                      remaining_elements_count);
  GotoIf(SmiEqual(remaining_elements_count, SmiConstant(0)), &resolve_promise);
  Return(UndefinedConstant());

  BIND(&resolve_promise);
  TNode<PromiseCapability> capability = CAST(LoadContextElement(
      context, PromiseBuiltins::kPromiseAllResolveElementCapabilitySlot));
  TNode<Object> resolve =
      LoadObjectField(capability, PromiseCapability::kResolveOffset);
  CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
         context, resolve, UndefinedConstant(), values_array);
  Return(UndefinedConstant());

  BIND(&already_called);
  Return(UndefinedConstant());

  BIND(&unreachable);
  Unreachable();
}

// ES#sec-promise.race
// Promise.race ( iterable )
TF_BUILTIN(PromiseRace, PromiseBuiltinsAssembler) {
  IteratorBuiltinsAssembler iter_assembler(state());
  VARIABLE(var_exception, MachineRepresentation::kTagged, TheHoleConstant());

  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "Promise.race");

  // Let promiseCapability be ? NewPromiseCapability(C).
  // Don't fire debugEvent so that forwarding the rejection through all does not
  // trigger redundant ExceptionEvents
  Node* const debug_event = FalseConstant();
  Node* const capability = CallBuiltin(Builtins::kNewPromiseCapability, context,
                                       receiver, debug_event);

  Node* const resolve =
      LoadObjectField(capability, PromiseCapability::kResolveOffset);
  Node* const reject =
      LoadObjectField(capability, PromiseCapability::kRejectOffset);

  Label close_iterator(this, Label::kDeferred);
  Label reject_promise(this, Label::kDeferred);

  // For catch prediction, don't treat the .then calls as handling it;
  // instead, recurse outwards.
  SetForwardingHandlerIfTrue(context, IsDebugActive(), reject);

  // Let iterator be GetIterator(iterable).
  // IfAbruptRejectPromise(iterator, promiseCapability).
  Node* const iterable = Parameter(Descriptor::kIterable);
  IteratorRecord iterator = iter_assembler.GetIterator(
      context, iterable, &reject_promise, &var_exception);

  // Let result be PerformPromiseRace(iteratorRecord, C, promiseCapability).
  {
    Label loop(this), break_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      Node* const native_context = LoadNativeContext(context);
      Node* const fast_iterator_result_map = LoadContextElement(
          native_context, Context::ITERATOR_RESULT_MAP_INDEX);

      // Let next be IteratorStep(iteratorRecord.[[Iterator]]).
      // If next is an abrupt completion, set iteratorRecord.[[Done]] to true.
      // ReturnIfAbrupt(next).
      Node* const next = iter_assembler.IteratorStep(
          context, iterator, &break_loop, fast_iterator_result_map,
          &reject_promise, &var_exception);

      // Let nextValue be IteratorValue(next).
      // If nextValue is an abrupt completion, set iteratorRecord.[[Done]] to
      //     true.
      // ReturnIfAbrupt(nextValue).
      Node* const next_value =
          iter_assembler.IteratorValue(context, next, fast_iterator_result_map,
                                       &reject_promise, &var_exception);

      // Let nextPromise be ? Invoke(constructor, "resolve", « nextValue »).
      Node* const next_promise =
          InvokeResolve(native_context, receiver, next_value, &close_iterator,
                        &var_exception);

      // Perform ? Invoke(nextPromise, "then", « resolveElement,
      //                  resultCapability.[[Reject]] »).
      Node* const then =
          GetProperty(context, next_promise, factory()->then_string());
      GotoIfException(then, &close_iterator, &var_exception);

      Node* const then_call =
          CallJS(CodeFactory::Call(isolate(),
                                   ConvertReceiverMode::kNotNullOrUndefined),
                 context, then, next_promise, resolve, reject);
      GotoIfException(then_call, &close_iterator, &var_exception);

      // For catch prediction, mark that rejections here are semantically
      // handled by the combined Promise.
      SetPromiseHandledByIfTrue(context, IsDebugActive(), then_call, [=]() {
        // Load promiseCapability.[[Promise]]
        return LoadObjectField(capability, PromiseCapability::kPromiseOffset);
      });
      Goto(&loop);
    }

    BIND(&break_loop);
    Return(LoadObjectField(capability, PromiseCapability::kPromiseOffset));
  }

  BIND(&close_iterator);
  {
    CSA_ASSERT(this, IsNotTheHole(var_exception.value()));
    iter_assembler.IteratorCloseOnException(context, iterator, &reject_promise,
                                            &var_exception);
  }

  BIND(&reject_promise);
  {
    Node* const reject =
        LoadObjectField(capability, PromiseCapability::kRejectOffset);
    CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
           context, reject, UndefinedConstant(), var_exception.value());

    Node* const promise =
        LoadObjectField(capability, PromiseCapability::kPromiseOffset);
    Return(promise);
  }
}

}  // namespace internal
}  // namespace v8
