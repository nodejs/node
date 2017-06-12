// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-promise-gen.h"

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

using compiler::Node;

Node* PromiseBuiltinsAssembler::AllocateJSPromise(Node* context) {
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  Node* const initial_map =
      LoadObjectField(promise_fun, JSFunction::kPrototypeOrInitialMapOffset);
  Node* const instance = AllocateJSObjectFromMap(initial_map);
  return instance;
}

void PromiseBuiltinsAssembler::PromiseInit(Node* promise) {
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kStatusOffset,
                                 SmiConstant(v8::Promise::kPending));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset,
                                 SmiConstant(0));
  for (int i = 0; i < v8::Promise::kEmbedderFieldCount; i++) {
    int offset = JSPromise::kSize + i * kPointerSize;
    StoreObjectFieldNoWriteBarrier(promise, offset, SmiConstant(Smi::kZero));
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
  GotoIfNot(IsPromiseHookEnabledOrDebugIsActive(), &out);
  CallRuntime(Runtime::kPromiseHookInit, context, instance, parent);
  Goto(&out);

  BIND(&out);
  return instance;
}

Node* PromiseBuiltinsAssembler::AllocateAndSetJSPromise(Node* context,
                                                        Node* status,
                                                        Node* result) {
  CSA_ASSERT(this, TaggedIsSmi(status));

  Node* const instance = AllocateJSPromise(context);

  StoreObjectFieldNoWriteBarrier(instance, JSPromise::kStatusOffset, status);
  StoreObjectFieldNoWriteBarrier(instance, JSPromise::kResultOffset, result);
  StoreObjectFieldNoWriteBarrier(instance, JSPromise::kFlagsOffset,
                                 SmiConstant(0));
  for (int i = 0; i < v8::Promise::kEmbedderFieldCount; i++) {
    int offset = JSPromise::kSize + i * kPointerSize;
    StoreObjectFieldNoWriteBarrier(instance, offset, SmiConstant(Smi::kZero));
  }

  Label out(this);
  GotoIfNot(IsPromiseHookEnabledOrDebugIsActive(), &out);
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
  Node* const resolve_info =
      LoadContextElement(native_context, Context::PROMISE_RESOLVE_SHARED_FUN);
  Node* const resolve =
      AllocateFunctionWithMapAndContext(map, resolve_info, promise_context);
  Node* const reject_info =
      LoadContextElement(native_context, Context::PROMISE_REJECT_SHARED_FUN);
  Node* const reject =
      AllocateFunctionWithMapAndContext(map, reject_info, promise_context);
  return std::make_pair(resolve, reject);
}

Node* PromiseBuiltinsAssembler::NewPromiseCapability(Node* context,
                                                     Node* constructor,
                                                     Node* debug_event) {
  if (debug_event == nullptr) {
    debug_event = TrueConstant();
  }

  Label if_not_constructor(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(constructor), &if_not_constructor);
  GotoIfNot(IsConstructorMap(LoadMap(constructor)), &if_not_constructor);

  Node* native_context = LoadNativeContext(context);

  Node* map = LoadRoot(Heap::kJSPromiseCapabilityMapRootIndex);
  Node* capability = AllocateJSObjectFromMap(map);

  StoreObjectFieldNoWriteBarrier(
      capability, JSPromiseCapability::kPromiseOffset, UndefinedConstant());
  StoreObjectFieldNoWriteBarrier(
      capability, JSPromiseCapability::kResolveOffset, UndefinedConstant());
  StoreObjectFieldNoWriteBarrier(capability, JSPromiseCapability::kRejectOffset,
                                 UndefinedConstant());

  VARIABLE(var_result, MachineRepresentation::kTagged);
  var_result.Bind(capability);

  Label if_builtin_promise(this), if_custom_promise(this, Label::kDeferred),
      out(this);
  Branch(WordEqual(constructor,
                   LoadContextElement(native_context,
                                      Context::PROMISE_FUNCTION_INDEX)),
         &if_builtin_promise, &if_custom_promise);

  BIND(&if_builtin_promise);
  {
    Node* promise = AllocateJSPromise(context);
    PromiseInit(promise);
    StoreObjectField(capability, JSPromiseCapability::kPromiseOffset, promise);

    Node* resolve = nullptr;
    Node* reject = nullptr;

    std::tie(resolve, reject) =
        CreatePromiseResolvingFunctions(promise, debug_event, native_context);
    StoreObjectField(capability, JSPromiseCapability::kResolveOffset, resolve);
    StoreObjectField(capability, JSPromiseCapability::kRejectOffset, reject);

    GotoIfNot(IsPromiseHookEnabledOrDebugIsActive(), &out);
    CallRuntime(Runtime::kPromiseHookInit, context, promise,
                UndefinedConstant());
    Goto(&out);
  }

  BIND(&if_custom_promise);
  {
    Label if_notcallable(this, Label::kDeferred);
    Node* executor_context =
        CreatePromiseGetCapabilitiesExecutorContext(capability, native_context);
    Node* executor_info = LoadContextElement(
        native_context, Context::PROMISE_GET_CAPABILITIES_EXECUTOR_SHARED_FUN);
    Node* function_map = LoadContextElement(
        native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
    Node* executor = AllocateFunctionWithMapAndContext(
        function_map, executor_info, executor_context);

    Node* promise = ConstructJS(CodeFactory::Construct(isolate()), context,
                                constructor, executor);

    Node* resolve =
        LoadObjectField(capability, JSPromiseCapability::kResolveOffset);
    GotoIf(TaggedIsSmi(resolve), &if_notcallable);
    GotoIfNot(IsCallableMap(LoadMap(resolve)), &if_notcallable);

    Node* reject =
        LoadObjectField(capability, JSPromiseCapability::kRejectOffset);
    GotoIf(TaggedIsSmi(reject), &if_notcallable);
    GotoIfNot(IsCallableMap(LoadMap(reject)), &if_notcallable);

    StoreObjectField(capability, JSPromiseCapability::kPromiseOffset, promise);

    Goto(&out);

    BIND(&if_notcallable);
    Node* message = SmiConstant(MessageTemplate::kPromiseNonCallable);
    StoreObjectField(capability, JSPromiseCapability::kPromiseOffset,
                     UndefinedConstant());
    StoreObjectField(capability, JSPromiseCapability::kResolveOffset,
                     UndefinedConstant());
    StoreObjectField(capability, JSPromiseCapability::kRejectOffset,
                     UndefinedConstant());
    CallRuntime(Runtime::kThrowTypeError, context, message);
    Unreachable();
  }

  BIND(&if_not_constructor);
  {
    Node* const message_id = SmiConstant(MessageTemplate::kNotConstructor);
    CallRuntime(Runtime::kThrowTypeError, context, message_id, constructor);
    Unreachable();
  }

  BIND(&out);
  return var_result.value();
}

Node* PromiseBuiltinsAssembler::CreatePromiseContext(Node* native_context,
                                                     int slots) {
  DCHECK_GE(slots, Context::MIN_CONTEXT_SLOTS);

  Node* const context = Allocate(FixedArray::SizeFor(slots));
  StoreMapNoWriteBarrier(context, Heap::kFunctionContextMapRootIndex);
  StoreObjectFieldNoWriteBarrier(context, FixedArray::kLengthOffset,
                                 SmiConstant(slots));

  Node* const empty_fn =
      LoadContextElement(native_context, Context::CLOSURE_INDEX);
  StoreContextElementNoWriteBarrier(context, Context::CLOSURE_INDEX, empty_fn);
  StoreContextElementNoWriteBarrier(context, Context::PREVIOUS_INDEX,
                                    UndefinedConstant());
  StoreContextElementNoWriteBarrier(context, Context::EXTENSION_INDEX,
                                    TheHoleConstant());
  StoreContextElementNoWriteBarrier(context, Context::NATIVE_CONTEXT_INDEX,
                                    native_context);
  return context;
}

Node* PromiseBuiltinsAssembler::CreatePromiseResolvingFunctionsContext(
    Node* promise, Node* debug_event, Node* native_context) {
  Node* const context =
      CreatePromiseContext(native_context, kPromiseContextLength);
  StoreContextElementNoWriteBarrier(context, kAlreadyVisitedSlot,
                                    SmiConstant(0));
  StoreContextElementNoWriteBarrier(context, kPromiseSlot, promise);
  StoreContextElementNoWriteBarrier(context, kDebugEventSlot, debug_event);
  return context;
}

Node* PromiseBuiltinsAssembler::CreatePromiseGetCapabilitiesExecutorContext(
    Node* promise_capability, Node* native_context) {
  int kContextLength = kCapabilitiesContextLength;
  Node* context = CreatePromiseContext(native_context, kContextLength);
  StoreContextElementNoWriteBarrier(context, kCapabilitySlot,
                                    promise_capability);
  return context;
}

Node* PromiseBuiltinsAssembler::ThrowIfNotJSReceiver(
    Node* context, Node* value, MessageTemplate::Template msg_template,
    const char* method_name) {
  Label out(this), throw_exception(this, Label::kDeferred);
  VARIABLE(var_value_map, MachineRepresentation::kTagged);

  GotoIf(TaggedIsSmi(value), &throw_exception);

  // Load the instance type of the {value}.
  var_value_map.Bind(LoadMap(value));
  Node* const value_instance_type = LoadMapInstanceType(var_value_map.value());

  Branch(IsJSReceiverInstanceType(value_instance_type), &out, &throw_exception);

  // The {value} is not a compatible receiver for this method.
  BIND(&throw_exception);
  {
    Node* const method =
        method_name == nullptr
            ? UndefinedConstant()
            : HeapConstant(
                  isolate()->factory()->NewStringFromAsciiChecked(method_name));
    Node* const message_id = SmiConstant(msg_template);
    CallRuntime(Runtime::kThrowTypeError, context, message_id, method);
    Unreachable();
  }

  BIND(&out);
  return var_value_map.value();
}

Node* PromiseBuiltinsAssembler::PromiseHasHandler(Node* promise) {
  Node* const flags = LoadObjectField(promise, JSPromise::kFlagsOffset);
  return IsSetWord(SmiUntag(flags), 1 << JSPromise::kHasHandlerBit);
}

void PromiseBuiltinsAssembler::PromiseSetHasHandler(Node* promise) {
  Node* const flags = LoadObjectField(promise, JSPromise::kFlagsOffset);
  Node* const new_flags =
      SmiOr(flags, SmiConstant(1 << JSPromise::kHasHandlerBit));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset, new_flags);
}

void PromiseBuiltinsAssembler::PromiseSetHandledHint(Node* promise) {
  Node* const flags = LoadObjectField(promise, JSPromise::kFlagsOffset);
  Node* const new_flags =
      SmiOr(flags, SmiConstant(1 << JSPromise::kHandledHintBit));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset, new_flags);
}

Node* PromiseBuiltinsAssembler::SpeciesConstructor(Node* context, Node* object,
                                                   Node* default_constructor) {
  Isolate* isolate = this->isolate();
  VARIABLE(var_result, MachineRepresentation::kTagged);
  var_result.Bind(default_constructor);

  // 2. Let C be ? Get(O, "constructor").
  Node* const constructor =
      GetProperty(context, object, isolate->factory()->constructor_string());

  // 3. If C is undefined, return defaultConstructor.
  Label out(this);
  GotoIf(IsUndefined(constructor), &out);

  // 4. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, constructor,
                       MessageTemplate::kConstructorNotReceiver);

  // 5. Let S be ? Get(C, @@species).
  Node* const species =
      GetProperty(context, constructor, isolate->factory()->species_symbol());

  // 6. If S is either undefined or null, return defaultConstructor.
  GotoIf(IsUndefined(species), &out);
  GotoIf(WordEqual(species, NullConstant()), &out);

  // 7. If IsConstructor(S) is true, return S.
  Label throw_error(this);
  GotoIf(TaggedIsSmi(species), &throw_error);
  Node* species_bitfield = LoadMapBitField(LoadMap(species));
  GotoIfNot(Word32Equal(Word32And(species_bitfield,
                                  Int32Constant((1 << Map::kIsConstructor))),
                        Int32Constant(1 << Map::kIsConstructor)),
            &throw_error);
  var_result.Bind(species);
  Goto(&out);

  // 8. Throw a TypeError exception.
  BIND(&throw_error);
  {
    Node* const message_id =
        SmiConstant(MessageTemplate::kSpeciesNotConstructor);
    CallRuntime(Runtime::kThrowTypeError, context, message_id);
    Unreachable();
  }

  BIND(&out);
  return var_result.value();
}

void PromiseBuiltinsAssembler::AppendPromiseCallback(int offset, Node* promise,
                                                     Node* value) {
  Node* elements = LoadObjectField(promise, offset);
  Node* length = LoadFixedArrayBaseLength(elements);
  CodeStubAssembler::ParameterMode mode = OptimalParameterMode();
  length = TaggedToParameter(length, mode);

  Node* delta = IntPtrOrSmiConstant(1, mode);
  Node* new_capacity = IntPtrOrSmiAdd(length, delta, mode);

  const ElementsKind kind = FAST_ELEMENTS;
  const WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER;
  const CodeStubAssembler::AllocationFlags flags =
      CodeStubAssembler::kAllowLargeObjectAllocation;
  int additional_offset = 0;

  Node* new_elements = AllocateFixedArray(kind, new_capacity, mode, flags);

  CopyFixedArrayElements(kind, elements, new_elements, length, barrier_mode,
                         mode);
  StoreFixedArrayElement(new_elements, length, value, barrier_mode,
                         additional_offset, mode);

  StoreObjectField(promise, offset, new_elements);
}

Node* PromiseBuiltinsAssembler::InternalPromiseThen(Node* context,
                                                    Node* promise,
                                                    Node* on_resolve,
                                                    Node* on_reject) {
  Isolate* isolate = this->isolate();

  // 2. If IsPromise(promise) is false, throw a TypeError exception.
  ThrowIfNotInstanceType(context, promise, JS_PROMISE_TYPE,
                         "Promise.prototype.then");

  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);

  // 3. Let C be ? SpeciesConstructor(promise, %Promise%).
  Node* constructor = SpeciesConstructor(context, promise, promise_fun);

  // 4. Let resultCapability be ? NewPromiseCapability(C).
  Callable call_callable = CodeFactory::Call(isolate);
  Label fast_promise_capability(this), promise_capability(this),
      perform_promise_then(this);
  VARIABLE(var_deferred_promise, MachineRepresentation::kTagged);
  VARIABLE(var_deferred_on_resolve, MachineRepresentation::kTagged);
  VARIABLE(var_deferred_on_reject, MachineRepresentation::kTagged);

  Branch(WordEqual(promise_fun, constructor), &fast_promise_capability,
         &promise_capability);

  BIND(&fast_promise_capability);
  {
    Node* const deferred_promise = AllocateAndInitJSPromise(context, promise);
    var_deferred_promise.Bind(deferred_promise);
    var_deferred_on_resolve.Bind(UndefinedConstant());
    var_deferred_on_reject.Bind(UndefinedConstant());
    Goto(&perform_promise_then);
  }

  BIND(&promise_capability);
  {
    Node* const capability = NewPromiseCapability(context, constructor);
    var_deferred_promise.Bind(
        LoadObjectField(capability, JSPromiseCapability::kPromiseOffset));
    var_deferred_on_resolve.Bind(
        LoadObjectField(capability, JSPromiseCapability::kResolveOffset));
    var_deferred_on_reject.Bind(
        LoadObjectField(capability, JSPromiseCapability::kRejectOffset));
    Goto(&perform_promise_then);
  }

  // 5. Return PerformPromiseThen(promise, onFulfilled, onRejected,
  //    resultCapability).
  BIND(&perform_promise_then);
  Node* const result = InternalPerformPromiseThen(
      context, promise, on_resolve, on_reject, var_deferred_promise.value(),
      var_deferred_on_resolve.value(), var_deferred_on_reject.value());
  return result;
}

Node* PromiseBuiltinsAssembler::InternalPerformPromiseThen(
    Node* context, Node* promise, Node* on_resolve, Node* on_reject,
    Node* deferred_promise, Node* deferred_on_resolve,
    Node* deferred_on_reject) {
  VARIABLE(var_on_resolve, MachineRepresentation::kTagged);
  VARIABLE(var_on_reject, MachineRepresentation::kTagged);

  var_on_resolve.Bind(on_resolve);
  var_on_reject.Bind(on_reject);

  Label out(this), if_onresolvenotcallable(this), onrejectcheck(this),
      append_callbacks(this);
  GotoIf(TaggedIsSmi(on_resolve), &if_onresolvenotcallable);

  Isolate* isolate = this->isolate();
  Node* const on_resolve_map = LoadMap(on_resolve);
  Branch(IsCallableMap(on_resolve_map), &onrejectcheck,
         &if_onresolvenotcallable);

  BIND(&if_onresolvenotcallable);
  {
    Node* const default_resolve_handler_symbol = HeapConstant(
        isolate->factory()->promise_default_resolve_handler_symbol());
    var_on_resolve.Bind(default_resolve_handler_symbol);
    Goto(&onrejectcheck);
  }

  BIND(&onrejectcheck);
  {
    Label if_onrejectnotcallable(this);
    GotoIf(TaggedIsSmi(on_reject), &if_onrejectnotcallable);

    Node* const on_reject_map = LoadMap(on_reject);
    Branch(IsCallableMap(on_reject_map), &append_callbacks,
           &if_onrejectnotcallable);

    BIND(&if_onrejectnotcallable);
    {
      Node* const default_reject_handler_symbol = HeapConstant(
          isolate->factory()->promise_default_reject_handler_symbol());
      var_on_reject.Bind(default_reject_handler_symbol);
      Goto(&append_callbacks);
    }
  }

  BIND(&append_callbacks);
  {
    Label fulfilled_check(this);
    Node* const status = LoadObjectField(promise, JSPromise::kStatusOffset);
    GotoIfNot(SmiEqual(status, SmiConstant(v8::Promise::kPending)),
              &fulfilled_check);

    Node* const existing_deferred_promise =
        LoadObjectField(promise, JSPromise::kDeferredPromiseOffset);

    Label if_noexistingcallbacks(this), if_existingcallbacks(this);
    Branch(IsUndefined(existing_deferred_promise), &if_noexistingcallbacks,
           &if_existingcallbacks);

    BIND(&if_noexistingcallbacks);
    {
      // Store callbacks directly in the slots.
      StoreObjectField(promise, JSPromise::kDeferredPromiseOffset,
                       deferred_promise);
      StoreObjectField(promise, JSPromise::kDeferredOnResolveOffset,
                       deferred_on_resolve);
      StoreObjectField(promise, JSPromise::kDeferredOnRejectOffset,
                       deferred_on_reject);
      StoreObjectField(promise, JSPromise::kFulfillReactionsOffset,
                       var_on_resolve.value());
      StoreObjectField(promise, JSPromise::kRejectReactionsOffset,
                       var_on_reject.value());
      Goto(&out);
    }

    BIND(&if_existingcallbacks);
    {
      Label if_singlecallback(this), if_multiplecallbacks(this);
      BranchIfJSObject(existing_deferred_promise, &if_singlecallback,
                       &if_multiplecallbacks);

      BIND(&if_singlecallback);
      {
        // Create new FixedArrays to store callbacks, and migrate
        // existing callbacks.
        Node* const deferred_promise_arr =
            AllocateFixedArray(FAST_ELEMENTS, IntPtrConstant(2));
        StoreFixedArrayElement(deferred_promise_arr, 0,
                               existing_deferred_promise);
        StoreFixedArrayElement(deferred_promise_arr, 1, deferred_promise);

        Node* const deferred_on_resolve_arr =
            AllocateFixedArray(FAST_ELEMENTS, IntPtrConstant(2));
        StoreFixedArrayElement(
            deferred_on_resolve_arr, 0,
            LoadObjectField(promise, JSPromise::kDeferredOnResolveOffset));
        StoreFixedArrayElement(deferred_on_resolve_arr, 1, deferred_on_resolve);

        Node* const deferred_on_reject_arr =
            AllocateFixedArray(FAST_ELEMENTS, IntPtrConstant(2));
        StoreFixedArrayElement(
            deferred_on_reject_arr, 0,
            LoadObjectField(promise, JSPromise::kDeferredOnRejectOffset));
        StoreFixedArrayElement(deferred_on_reject_arr, 1, deferred_on_reject);

        Node* const fulfill_reactions =
            AllocateFixedArray(FAST_ELEMENTS, IntPtrConstant(2));
        StoreFixedArrayElement(
            fulfill_reactions, 0,
            LoadObjectField(promise, JSPromise::kFulfillReactionsOffset));
        StoreFixedArrayElement(fulfill_reactions, 1, var_on_resolve.value());

        Node* const reject_reactions =
            AllocateFixedArray(FAST_ELEMENTS, IntPtrConstant(2));
        StoreFixedArrayElement(
            reject_reactions, 0,
            LoadObjectField(promise, JSPromise::kRejectReactionsOffset));
        StoreFixedArrayElement(reject_reactions, 1, var_on_reject.value());

        // Store new FixedArrays in promise.
        StoreObjectField(promise, JSPromise::kDeferredPromiseOffset,
                         deferred_promise_arr);
        StoreObjectField(promise, JSPromise::kDeferredOnResolveOffset,
                         deferred_on_resolve_arr);
        StoreObjectField(promise, JSPromise::kDeferredOnRejectOffset,
                         deferred_on_reject_arr);
        StoreObjectField(promise, JSPromise::kFulfillReactionsOffset,
                         fulfill_reactions);
        StoreObjectField(promise, JSPromise::kRejectReactionsOffset,
                         reject_reactions);
        Goto(&out);
      }

      BIND(&if_multiplecallbacks);
      {
        AppendPromiseCallback(JSPromise::kDeferredPromiseOffset, promise,
                              deferred_promise);
        AppendPromiseCallback(JSPromise::kDeferredOnResolveOffset, promise,
                              deferred_on_resolve);
        AppendPromiseCallback(JSPromise::kDeferredOnRejectOffset, promise,
                              deferred_on_reject);
        AppendPromiseCallback(JSPromise::kFulfillReactionsOffset, promise,
                              var_on_resolve.value());
        AppendPromiseCallback(JSPromise::kRejectReactionsOffset, promise,
                              var_on_reject.value());
        Goto(&out);
      }
    }

    BIND(&fulfilled_check);
    {
      Label reject(this);
      Node* const result = LoadObjectField(promise, JSPromise::kResultOffset);
      GotoIfNot(WordEqual(status, SmiConstant(v8::Promise::kFulfilled)),
                &reject);

      Node* info = AllocatePromiseReactionJobInfo(
          result, var_on_resolve.value(), deferred_promise, deferred_on_resolve,
          deferred_on_reject, context);
      // TODO(gsathya): Move this to TF
      CallRuntime(Runtime::kEnqueuePromiseReactionJob, context, info);
      Goto(&out);

      BIND(&reject);
      {
        Node* const has_handler = PromiseHasHandler(promise);
        Label enqueue(this);

        // TODO(gsathya): Fold these runtime calls and move to TF.
        GotoIf(has_handler, &enqueue);
        CallRuntime(Runtime::kPromiseRevokeReject, context, promise);
        Goto(&enqueue);

        BIND(&enqueue);
        {
          Node* info = AllocatePromiseReactionJobInfo(
              result, var_on_reject.value(), deferred_promise,
              deferred_on_resolve, deferred_on_reject, context);
          // TODO(gsathya): Move this to TF
          CallRuntime(Runtime::kEnqueuePromiseReactionJob, context, info);
          Goto(&out);
        }
      }
    }
  }

  BIND(&out);
  PromiseSetHasHandler(promise);
  return deferred_promise;
}

// Promise fast path implementations rely on unmodified JSPromise instances.
// We use a fairly coarse granularity for this and simply check whether both
// the promise itself is unmodified (i.e. its map has not changed) and its
// prototype is unmodified.
// TODO(gsathya): Refactor this out to prevent code dupe with builtins-regexp
void PromiseBuiltinsAssembler::BranchIfFastPath(Node* context, Node* promise,
                                                Label* if_isunmodified,
                                                Label* if_ismodified) {
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  BranchIfFastPath(native_context, promise_fun, promise, if_isunmodified,
                   if_ismodified);
}

void PromiseBuiltinsAssembler::BranchIfFastPath(Node* native_context,
                                                Node* promise_fun,
                                                Node* promise,
                                                Label* if_isunmodified,
                                                Label* if_ismodified) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  CSA_ASSERT(this,
             WordEqual(promise_fun,
                       LoadContextElement(native_context,
                                          Context::PROMISE_FUNCTION_INDEX)));

  Node* const map = LoadMap(promise);
  Node* const initial_map =
      LoadObjectField(promise_fun, JSFunction::kPrototypeOrInitialMapOffset);
  Node* const has_initialmap = WordEqual(map, initial_map);

  GotoIfNot(has_initialmap, if_ismodified);

  Node* const initial_proto_initial_map =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_MAP_INDEX);
  Node* const proto_map = LoadMap(LoadMapPrototype(map));
  Node* const proto_has_initialmap =
      WordEqual(proto_map, initial_proto_initial_map);

  Branch(proto_has_initialmap, if_isunmodified, if_ismodified);
}

Node* PromiseBuiltinsAssembler::AllocatePromiseResolveThenableJobInfo(
    Node* thenable, Node* then, Node* resolve, Node* reject, Node* context) {
  Node* const info = Allocate(PromiseResolveThenableJobInfo::kSize);
  StoreMapNoWriteBarrier(info,
                         Heap::kPromiseResolveThenableJobInfoMapRootIndex);
  StoreObjectFieldNoWriteBarrier(
      info, PromiseResolveThenableJobInfo::kThenableOffset, thenable);
  StoreObjectFieldNoWriteBarrier(
      info, PromiseResolveThenableJobInfo::kThenOffset, then);
  StoreObjectFieldNoWriteBarrier(
      info, PromiseResolveThenableJobInfo::kResolveOffset, resolve);
  StoreObjectFieldNoWriteBarrier(
      info, PromiseResolveThenableJobInfo::kRejectOffset, reject);
  StoreObjectFieldNoWriteBarrier(
      info, PromiseResolveThenableJobInfo::kContextOffset, context);
  return info;
}

void PromiseBuiltinsAssembler::InternalResolvePromise(Node* context,
                                                      Node* promise,
                                                      Node* result) {
  Isolate* isolate = this->isolate();

  VARIABLE(var_reason, MachineRepresentation::kTagged);
  VARIABLE(var_then, MachineRepresentation::kTagged);

  Label do_enqueue(this), fulfill(this), if_cycle(this, Label::kDeferred),
      if_rejectpromise(this, Label::kDeferred), out(this);

  Label cycle_check(this);
  GotoIfNot(IsPromiseHookEnabledOrDebugIsActive(), &cycle_check);
  CallRuntime(Runtime::kPromiseHookResolve, context, promise);
  Goto(&cycle_check);

  BIND(&cycle_check);
  // 6. If SameValue(resolution, promise) is true, then
  GotoIf(SameValue(promise, result), &if_cycle);

  // 7. If Type(resolution) is not Object, then
  GotoIf(TaggedIsSmi(result), &fulfill);
  GotoIfNot(IsJSReceiver(result), &fulfill);

  Label if_nativepromise(this), if_notnativepromise(this, Label::kDeferred);
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  BranchIfFastPath(native_context, promise_fun, result, &if_nativepromise,
                   &if_notnativepromise);

  // Resolution is a native promise and if it's already resolved or
  // rejected, shortcircuit the resolution procedure by directly
  // reusing the value from the promise.
  BIND(&if_nativepromise);
  {
    Node* const thenable_status =
        LoadObjectField(result, JSPromise::kStatusOffset);
    Node* const thenable_value =
        LoadObjectField(result, JSPromise::kResultOffset);

    Label if_isnotpending(this);
    GotoIfNot(SmiEqual(SmiConstant(v8::Promise::kPending), thenable_status),
              &if_isnotpending);

    // TODO(gsathya): Use a marker here instead of the actual then
    // callback, and check for the marker in PromiseResolveThenableJob
    // and perform PromiseThen.
    Node* const then =
        LoadContextElement(native_context, Context::PROMISE_THEN_INDEX);
    var_then.Bind(then);
    Goto(&do_enqueue);

    BIND(&if_isnotpending);
    {
      Label if_fulfilled(this), if_rejected(this);
      Branch(SmiEqual(SmiConstant(v8::Promise::kFulfilled), thenable_status),
             &if_fulfilled, &if_rejected);

      BIND(&if_fulfilled);
      {
        PromiseFulfill(context, promise, thenable_value,
                       v8::Promise::kFulfilled);
        PromiseSetHasHandler(promise);
        Goto(&out);
      }

      BIND(&if_rejected);
      {
        Label reject(this);
        Node* const has_handler = PromiseHasHandler(result);

        // Promise has already been rejected, but had no handler.
        // Revoke previously triggered reject event.
        GotoIf(has_handler, &reject);
        CallRuntime(Runtime::kPromiseRevokeReject, context, result);
        Goto(&reject);

        BIND(&reject);
        // Don't cause a debug event as this case is forwarding a rejection.
        InternalPromiseReject(context, promise, thenable_value, false);
        PromiseSetHasHandler(result);
        Goto(&out);
      }
    }
  }

  BIND(&if_notnativepromise);
  {
    // 8. Let then be Get(resolution, "then").
    Node* const then =
        GetProperty(context, result, isolate->factory()->then_string());

    // 9. If then is an abrupt completion, then
    GotoIfException(then, &if_rejectpromise, &var_reason);

    // 11. If IsCallable(thenAction) is false, then
    GotoIf(TaggedIsSmi(then), &fulfill);
    Node* const then_map = LoadMap(then);
    GotoIfNot(IsCallableMap(then_map), &fulfill);
    var_then.Bind(then);
    Goto(&do_enqueue);
  }

  BIND(&do_enqueue);
  {
    // TODO(gsathya): Add fast path for native promises with unmodified
    // PromiseThen (which don't need these resolving functions, but
    // instead can just call resolve/reject directly).
    Node* resolve = nullptr;
    Node* reject = nullptr;
    std::tie(resolve, reject) = CreatePromiseResolvingFunctions(
        promise, FalseConstant(), native_context);

    Node* const info = AllocatePromiseResolveThenableJobInfo(
        result, var_then.value(), resolve, reject, context);

    Label enqueue(this);
    GotoIfNot(IsDebugActive(), &enqueue);

    GotoIf(TaggedIsSmi(result), &enqueue);
    GotoIfNot(HasInstanceType(result, JS_PROMISE_TYPE), &enqueue);

    // Mark the dependency of the new promise on the resolution
    Node* const key =
        HeapConstant(isolate->factory()->promise_handled_by_symbol());
    CallRuntime(Runtime::kSetProperty, context, result, key, promise,
                SmiConstant(STRICT));
    Goto(&enqueue);

    // 12. Perform EnqueueJob("PromiseJobs",
    // PromiseResolveThenableJob, « promise, resolution, thenAction»).
    BIND(&enqueue);
    // TODO(gsathya): Move this to TF
    CallRuntime(Runtime::kEnqueuePromiseResolveThenableJob, context, info);
    Goto(&out);
  }

  // 7.b Return FulfillPromise(promise, resolution).
  BIND(&fulfill);
  {
    PromiseFulfill(context, promise, result, v8::Promise::kFulfilled);
    Goto(&out);
  }

  BIND(&if_cycle);
  {
    // 6.a Let selfResolutionError be a newly created TypeError object.
    Node* const message_id = SmiConstant(MessageTemplate::kPromiseCyclic);
    Node* const error =
        CallRuntime(Runtime::kNewTypeError, context, message_id, result);
    var_reason.Bind(error);

    // 6.b Return RejectPromise(promise, selfResolutionError).
    Goto(&if_rejectpromise);
  }

  // 9.a Return RejectPromise(promise, then.[[Value]]).
  BIND(&if_rejectpromise);
  {
    // Don't cause a debug event as this case is forwarding a rejection.
    InternalPromiseReject(context, promise, var_reason.value(), false);
    Goto(&out);
  }

  BIND(&out);
}

void PromiseBuiltinsAssembler::PromiseFulfill(
    Node* context, Node* promise, Node* result,
    v8::Promise::PromiseState status) {
  Label do_promisereset(this), debug_async_event_enqueue_recurring(this);

  Node* const status_smi = SmiConstant(static_cast<int>(status));
  Node* const deferred_promise =
      LoadObjectField(promise, JSPromise::kDeferredPromiseOffset);

  GotoIf(IsUndefined(deferred_promise), &debug_async_event_enqueue_recurring);

  Node* const tasks =
      status == v8::Promise::kFulfilled
          ? LoadObjectField(promise, JSPromise::kFulfillReactionsOffset)
          : LoadObjectField(promise, JSPromise::kRejectReactionsOffset);

  Node* const deferred_on_resolve =
      LoadObjectField(promise, JSPromise::kDeferredOnResolveOffset);
  Node* const deferred_on_reject =
      LoadObjectField(promise, JSPromise::kDeferredOnRejectOffset);

  Node* const info = AllocatePromiseReactionJobInfo(
      result, tasks, deferred_promise, deferred_on_resolve, deferred_on_reject,
      context);

  CallRuntime(Runtime::kEnqueuePromiseReactionJob, context, info);
  Goto(&debug_async_event_enqueue_recurring);

  BIND(&debug_async_event_enqueue_recurring);
  {
    GotoIfNot(IsDebugActive(), &do_promisereset);
    CallRuntime(Runtime::kDebugAsyncEventEnqueueRecurring, context, promise,
                status_smi);
    Goto(&do_promisereset);
  }

  BIND(&do_promisereset);
  {
    StoreObjectField(promise, JSPromise::kStatusOffset, status_smi);
    StoreObjectField(promise, JSPromise::kResultOffset, result);
    StoreObjectFieldRoot(promise, JSPromise::kDeferredPromiseOffset,
                         Heap::kUndefinedValueRootIndex);
    StoreObjectFieldRoot(promise, JSPromise::kDeferredOnResolveOffset,
                         Heap::kUndefinedValueRootIndex);
    StoreObjectFieldRoot(promise, JSPromise::kDeferredOnRejectOffset,
                         Heap::kUndefinedValueRootIndex);
    StoreObjectFieldRoot(promise, JSPromise::kFulfillReactionsOffset,
                         Heap::kUndefinedValueRootIndex);
    StoreObjectFieldRoot(promise, JSPromise::kRejectReactionsOffset,
                         Heap::kUndefinedValueRootIndex);
  }
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
                     BooleanConstant(true)),
           &has_access, if_noaccess);
  }

  BIND(&has_access);
}

void PromiseBuiltinsAssembler::InternalPromiseReject(Node* context,
                                                     Node* promise, Node* value,
                                                     Node* debug_event) {
  Label out(this);
  GotoIfNot(IsDebugActive(), &out);
  GotoIfNot(WordEqual(TrueConstant(), debug_event), &out);
  CallRuntime(Runtime::kDebugPromiseReject, context, promise, value);
  Goto(&out);

  BIND(&out);
  InternalPromiseReject(context, promise, value, false);
}

// This duplicates a lot of logic from PromiseRejectEvent in
// runtime-promise.cc
void PromiseBuiltinsAssembler::InternalPromiseReject(Node* context,
                                                     Node* promise, Node* value,
                                                     bool debug_event) {
  Label fulfill(this), report_unhandledpromise(this), run_promise_hook(this);

  if (debug_event) {
    GotoIfNot(IsDebugActive(), &run_promise_hook);
    CallRuntime(Runtime::kDebugPromiseReject, context, promise, value);
    Goto(&run_promise_hook);
  } else {
    Goto(&run_promise_hook);
  }

  BIND(&run_promise_hook);
  {
    GotoIfNot(IsPromiseHookEnabledOrDebugIsActive(), &report_unhandledpromise);
    CallRuntime(Runtime::kPromiseHookResolve, context, promise);
    Goto(&report_unhandledpromise);
  }

  BIND(&report_unhandledpromise);
  {
    GotoIf(PromiseHasHandler(promise), &fulfill);
    CallRuntime(Runtime::kReportPromiseReject, context, promise, value);
    Goto(&fulfill);
  }

  BIND(&fulfill);
  PromiseFulfill(context, promise, value, v8::Promise::kRejected);
}

// ES#sec-promise-reject-functions
// Promise Reject Functions
TF_BUILTIN(PromiseRejectClosure, PromiseBuiltinsAssembler) {
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  Label out(this);

  // 3. Let alreadyResolved be F.[[AlreadyResolved]].
  int has_already_visited_slot = kAlreadyVisitedSlot;

  Node* const has_already_visited =
      LoadContextElement(context, has_already_visited_slot);

  // 4. If alreadyResolved.[[Value]] is true, return undefined.
  GotoIf(SmiEqual(has_already_visited, SmiConstant(1)), &out);

  // 5.Set alreadyResolved.[[Value]] to true.
  StoreContextElementNoWriteBarrier(context, has_already_visited_slot,
                                    SmiConstant(1));

  // 2. Let promise be F.[[Promise]].
  Node* const promise =
      LoadContextElement(context, IntPtrConstant(kPromiseSlot));
  Node* const debug_event =
      LoadContextElement(context, IntPtrConstant(kDebugEventSlot));

  InternalPromiseReject(context, promise, value, debug_event);
  Return(UndefinedConstant());

  BIND(&out);
  Return(UndefinedConstant());
}

// ES6 #sec-promise-executor
TF_BUILTIN(PromiseConstructor, PromiseBuiltinsAssembler) {
  Node* const executor = Parameter(Descriptor::kExecutor);
  Node* const new_target = Parameter(Descriptor::kNewTarget);
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

    GotoIfNot(IsPromiseHookEnabledOrDebugIsActive(), &debug_push);
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
    Callable call_callable = CodeFactory::Call(isolate);

    Node* const maybe_exception = CallJS(call_callable, context, executor,
                                         UndefinedConstant(), resolve, reject);

    GotoIfException(maybe_exception, &if_rejectpromise, &var_reason);
    Branch(is_debug_active, &debug_pop, &out);

    BIND(&if_rejectpromise);
    {
      Callable call_callable = CodeFactory::Call(isolate);
      CallJS(call_callable, context, reject, UndefinedConstant(),
             var_reason.value());
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
  {
    Node* const message_id = SmiConstant(MessageTemplate::kNotAPromise);
    CallRuntime(Runtime::kThrowTypeError, context, message_id, new_target);
    Unreachable();
  }

  // 2. If IsCallable(executor) is false, throw a TypeError exception.
  BIND(&if_notcallable);
  {
    Node* const message_id =
        SmiConstant(MessageTemplate::kResolverNotAFunction);
    CallRuntime(Runtime::kThrowTypeError, context, message_id, executor);
    Unreachable();
  }

  // Silently fail if the stack looks fishy.
  BIND(&if_noaccess);
  {
    Node* const counter_id =
        SmiConstant(v8::Isolate::kPromiseConstructorReturnedUndefined);
    CallRuntime(Runtime::kIncrementUseCounter, context, counter_id);
    Return(UndefinedConstant());
  }
}

TF_BUILTIN(PromiseInternalConstructor, PromiseBuiltinsAssembler) {
  Node* const parent = Parameter(Descriptor::kParent);
  Node* const context = Parameter(Descriptor::kContext);
  Return(AllocateAndInitJSPromise(context, parent));
}

TF_BUILTIN(IsPromise, PromiseBuiltinsAssembler) {
  Node* const maybe_promise = Parameter(Descriptor::kObject);
  Label if_notpromise(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(maybe_promise), &if_notpromise);

  Node* const result =
      SelectBooleanConstant(HasInstanceType(maybe_promise, JS_PROMISE_TYPE));
  Return(result);

  BIND(&if_notpromise);
  Return(FalseConstant());
}

// ES#sec-promise.prototype.then
// Promise.prototype.catch ( onFulfilled, onRejected )
TF_BUILTIN(PromiseThen, PromiseBuiltinsAssembler) {
  // 1. Let promise be the this value.
  Node* const promise = Parameter(Descriptor::kReceiver);
  Node* const on_resolve = Parameter(Descriptor::kOnFullfilled);
  Node* const on_reject = Parameter(Descriptor::kOnRejected);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const result =
      InternalPromiseThen(context, promise, on_resolve, on_reject);
  Return(result);
}

// ES#sec-promise-resolve-functions
// Promise Resolve Functions
TF_BUILTIN(PromiseResolveClosure, PromiseBuiltinsAssembler) {
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  Label out(this);

  // 3. Let alreadyResolved be F.[[AlreadyResolved]].
  int has_already_visited_slot = kAlreadyVisitedSlot;

  Node* const has_already_visited =
      LoadContextElement(context, has_already_visited_slot);

  // 4. If alreadyResolved.[[Value]] is true, return undefined.
  GotoIf(SmiEqual(has_already_visited, SmiConstant(1)), &out);

  // 5.Set alreadyResolved.[[Value]] to true.
  StoreContextElementNoWriteBarrier(context, has_already_visited_slot,
                                    SmiConstant(1));

  // 2. Let promise be F.[[Promise]].
  Node* const promise =
      LoadContextElement(context, IntPtrConstant(kPromiseSlot));

  InternalResolvePromise(context, promise, value);
  Return(UndefinedConstant());

  BIND(&out);
  Return(UndefinedConstant());
}

// ES #sec-fulfillpromise
TF_BUILTIN(ResolvePromise, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const result = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  InternalResolvePromise(context, promise, result);
  Return(UndefinedConstant());
}

TF_BUILTIN(PromiseHandleReject, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const on_reject = Parameter(Descriptor::kOnReject);
  Node* const exception = Parameter(Descriptor::kException);
  Node* const context = Parameter(Descriptor::kContext);

  Callable call_callable = CodeFactory::Call(isolate());
  VARIABLE(var_unused, MachineRepresentation::kTagged);

  Label if_internalhandler(this), if_customhandler(this, Label::kDeferred);
  Branch(IsUndefined(on_reject), &if_internalhandler, &if_customhandler);

  BIND(&if_internalhandler);
  {
    InternalPromiseReject(context, promise, exception, false);
    Return(UndefinedConstant());
  }

  BIND(&if_customhandler);
  {
    CallJS(call_callable, context, on_reject, UndefinedConstant(), exception);
    Return(UndefinedConstant());
  }
}

TF_BUILTIN(PromiseHandle, PromiseBuiltinsAssembler) {
  Node* const value = Parameter(Descriptor::kValue);
  Node* const handler = Parameter(Descriptor::kHandler);
  Node* const deferred_promise = Parameter(Descriptor::kDeferredPromise);
  Node* const deferred_on_resolve = Parameter(Descriptor::kDeferredOnResolve);
  Node* const deferred_on_reject = Parameter(Descriptor::kDeferredOnReject);
  Node* const context = Parameter(Descriptor::kContext);
  Isolate* isolate = this->isolate();

  VARIABLE(var_reason, MachineRepresentation::kTagged);

  Node* const is_debug_active = IsDebugActive();
  Label run_handler(this), if_rejectpromise(this), promisehook_before(this),
      promisehook_after(this), debug_pop(this);

  GotoIfNot(is_debug_active, &promisehook_before);
  CallRuntime(Runtime::kDebugPushPromise, context, deferred_promise);
  Goto(&promisehook_before);

  BIND(&promisehook_before);
  {
    GotoIfNot(IsPromiseHookEnabledOrDebugIsActive(), &run_handler);
    CallRuntime(Runtime::kPromiseHookBefore, context, deferred_promise);
    Goto(&run_handler);
  }

  BIND(&run_handler);
  {
    Label if_defaulthandler(this), if_callablehandler(this),
        if_internalhandler(this), if_customhandler(this, Label::kDeferred);
    VARIABLE(var_result, MachineRepresentation::kTagged);

    Branch(IsSymbol(handler), &if_defaulthandler, &if_callablehandler);

    BIND(&if_defaulthandler);
    {
      Label if_resolve(this), if_reject(this);
      Node* const default_resolve_handler_symbol = HeapConstant(
          isolate->factory()->promise_default_resolve_handler_symbol());
      Branch(WordEqual(default_resolve_handler_symbol, handler), &if_resolve,
             &if_reject);

      BIND(&if_resolve);
      {
        var_result.Bind(value);
        Branch(IsUndefined(deferred_on_resolve), &if_internalhandler,
               &if_customhandler);
      }

      BIND(&if_reject);
      {
        var_reason.Bind(value);
        Goto(&if_rejectpromise);
      }
    }

    BIND(&if_callablehandler);
    {
      Callable call_callable = CodeFactory::Call(isolate);
      Node* const result =
          CallJS(call_callable, context, handler, UndefinedConstant(), value);
      var_result.Bind(result);
      GotoIfException(result, &if_rejectpromise, &var_reason);
      Branch(IsUndefined(deferred_on_resolve), &if_internalhandler,
             &if_customhandler);
    }

    BIND(&if_internalhandler);
    InternalResolvePromise(context, deferred_promise, var_result.value());
    Goto(&promisehook_after);

    BIND(&if_customhandler);
    {
      Callable call_callable = CodeFactory::Call(isolate);
      Node* const maybe_exception =
          CallJS(call_callable, context, deferred_on_resolve,
                 UndefinedConstant(), var_result.value());
      GotoIfException(maybe_exception, &if_rejectpromise, &var_reason);
      Goto(&promisehook_after);
    }
  }

  BIND(&if_rejectpromise);
  {
    Callable promise_handle_reject = CodeFactory::PromiseHandleReject(isolate);
    CallStub(promise_handle_reject, context, deferred_promise,
             deferred_on_reject, var_reason.value());
    Goto(&promisehook_after);
  }

  BIND(&promisehook_after);
  {
    GotoIfNot(IsPromiseHookEnabledOrDebugIsActive(), &debug_pop);
    CallRuntime(Runtime::kPromiseHookAfter, context, deferred_promise);
    Goto(&debug_pop);
  }

  BIND(&debug_pop);
  {
    Label out(this);

    GotoIfNot(is_debug_active, &out);
    CallRuntime(Runtime::kDebugPopPromise, context);
    Goto(&out);

    BIND(&out);
    Return(UndefinedConstant());
  }
}

// ES#sec-promise.prototype.catch
// Promise.prototype.catch ( onRejected )
TF_BUILTIN(PromiseCatch, PromiseBuiltinsAssembler) {
  // 1. Let promise be the this value.
  Node* const promise = Parameter(Descriptor::kReceiver);
  Node* const on_resolve = UndefinedConstant();
  Node* const on_reject = Parameter(Descriptor::kOnRejected);
  Node* const context = Parameter(Descriptor::kContext);

  Label if_internalthen(this), if_customthen(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(promise), &if_customthen);
  BranchIfFastPath(context, promise, &if_internalthen, &if_customthen);

  BIND(&if_internalthen);
  {
    Node* const result =
        InternalPromiseThen(context, promise, on_resolve, on_reject);
    Return(result);
  }

  BIND(&if_customthen);
  {
    Node* const then =
        GetProperty(context, promise, isolate()->factory()->then_string());
    Callable call_callable = CodeFactory::Call(isolate());
    Node* const result =
        CallJS(call_callable, context, then, promise, on_resolve, on_reject);
    Return(result);
  }
}

TF_BUILTIN(PromiseResolve, PromiseBuiltinsAssembler) {
  //  1. Let C be the this value.
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  Isolate* isolate = this->isolate();

  // 2. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "PromiseResolve");

  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);

  Label if_valueisnativepromise(this), if_valueisnotnativepromise(this),
      if_valueisnotpromise(this);

  // 3.If IsPromise(x) is true, then
  GotoIf(TaggedIsSmi(value), &if_valueisnotpromise);

  // This shortcircuits the constructor lookups.
  GotoIfNot(HasInstanceType(value, JS_PROMISE_TYPE), &if_valueisnotpromise);

  // This adds a fast path as non-subclassed native promises don't have
  // an observable constructor lookup.
  BranchIfFastPath(native_context, promise_fun, value, &if_valueisnativepromise,
                   &if_valueisnotnativepromise);

  BIND(&if_valueisnativepromise);
  {
    GotoIfNot(WordEqual(promise_fun, receiver), &if_valueisnotnativepromise);
    Return(value);
  }

  // At this point, value or/and receiver are not native promises, but
  // they could be of the same subclass.
  BIND(&if_valueisnotnativepromise);
  {
    // 3.a Let xConstructor be ? Get(x, "constructor").
    // The constructor lookup is observable.
    Node* const constructor =
        GetProperty(context, value, isolate->factory()->constructor_string());

    // 3.b If SameValue(xConstructor, C) is true, return x.
    GotoIfNot(SameValue(constructor, receiver), &if_valueisnotpromise);

    Return(value);
  }

  BIND(&if_valueisnotpromise);
  {
    Label if_nativepromise(this), if_notnativepromise(this);
    Branch(WordEqual(promise_fun, receiver), &if_nativepromise,
           &if_notnativepromise);

    // This adds a fast path for native promises that don't need to
    // create NewPromiseCapability.
    BIND(&if_nativepromise);
    {
      Node* const result = AllocateAndInitJSPromise(context);
      InternalResolvePromise(context, result, value);
      Return(result);
    }

    BIND(&if_notnativepromise);
    {
      // 4. Let promiseCapability be ? NewPromiseCapability(C).
      Node* const capability = NewPromiseCapability(context, receiver);

      // 5. Perform ? Call(promiseCapability.[[Resolve]], undefined, « x »).
      Callable call_callable = CodeFactory::Call(isolate);
      Node* const resolve =
          LoadObjectField(capability, JSPromiseCapability::kResolveOffset);
      CallJS(call_callable, context, resolve, UndefinedConstant(), value);

      // 6. Return promiseCapability.[[Promise]].
      Node* const result =
          LoadObjectField(capability, JSPromiseCapability::kPromiseOffset);
      Return(result);
    }
  }
}

// ES6 #sec-getcapabilitiesexecutor-functions
TF_BUILTIN(PromiseGetCapabilitiesExecutor, PromiseBuiltinsAssembler) {
  Node* const resolve = Parameter(Descriptor::kResolve);
  Node* const reject = Parameter(Descriptor::kReject);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const capability = LoadContextElement(context, kCapabilitySlot);

  Label if_alreadyinvoked(this, Label::kDeferred);
  GotoIf(WordNotEqual(
             LoadObjectField(capability, JSPromiseCapability::kResolveOffset),
             UndefinedConstant()),
         &if_alreadyinvoked);
  GotoIf(WordNotEqual(
             LoadObjectField(capability, JSPromiseCapability::kRejectOffset),
             UndefinedConstant()),
         &if_alreadyinvoked);

  StoreObjectField(capability, JSPromiseCapability::kResolveOffset, resolve);
  StoreObjectField(capability, JSPromiseCapability::kRejectOffset, reject);

  Return(UndefinedConstant());

  BIND(&if_alreadyinvoked);
  Node* message = SmiConstant(MessageTemplate::kPromiseExecutorAlreadyInvoked);
  CallRuntime(Runtime::kThrowTypeError, context, message);
  Unreachable();
}

// ES6 #sec-newpromisecapability
TF_BUILTIN(NewPromiseCapability, PromiseBuiltinsAssembler) {
  Node* constructor = Parameter(Descriptor::kConstructor);
  Node* debug_event = Parameter(Descriptor::kDebugEvent);
  Node* context = Parameter(Descriptor::kContext);

  CSA_ASSERT_JS_ARGC_EQ(this, 2);

  Return(NewPromiseCapability(context, constructor, debug_event));
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
    Node* const promise = AllocateAndSetJSPromise(
        context, SmiConstant(v8::Promise::kRejected), reason);
    CallRuntime(Runtime::kPromiseRejectEventFromStack, context, promise,
                reason);
    Return(promise);
  }

  BIND(&if_custompromise);
  {
    // 3. Let promiseCapability be ? NewPromiseCapability(C).
    Node* const capability = NewPromiseCapability(context, receiver);

    // 4. Perform ? Call(promiseCapability.[[Reject]], undefined, « r »).
    Node* const reject =
        LoadObjectField(capability, JSPromiseCapability::kRejectOffset);
    Callable call_callable = CodeFactory::Call(isolate());
    CallJS(call_callable, context, reject, UndefinedConstant(), reason);

    // 5. Return promiseCapability.[[Promise]].
    Node* const promise =
        LoadObjectField(capability, JSPromiseCapability::kPromiseOffset);
    Return(promise);
  }
}

TF_BUILTIN(InternalPromiseReject, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const debug_event = Parameter(Descriptor::kDebugEvent);
  Node* const context = Parameter(Descriptor::kContext);

  InternalPromiseReject(context, promise, reason, debug_event);
  Return(UndefinedConstant());
}

Node* PromiseBuiltinsAssembler::CreatePromiseFinallyContext(
    Node* on_finally, Node* native_context) {
  Node* const context =
      CreatePromiseContext(native_context, kOnFinallyContextLength);
  StoreContextElementNoWriteBarrier(context, kOnFinallySlot, on_finally);
  return context;
}

std::pair<Node*, Node*> PromiseBuiltinsAssembler::CreatePromiseFinallyFunctions(
    Node* on_finally, Node* native_context) {
  Node* const promise_context =
      CreatePromiseFinallyContext(on_finally, native_context);
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

  Node* const value = LoadContextElement(context, kOnFinallySlot);
  Return(value);
}

Node* PromiseBuiltinsAssembler::CreateValueThunkFunctionContext(
    Node* value, Node* native_context) {
  Node* const context =
      CreatePromiseContext(native_context, kOnFinallyContextLength);
  StoreContextElementNoWriteBarrier(context, kOnFinallySlot, value);
  return context;
}

Node* PromiseBuiltinsAssembler::CreateValueThunkFunction(Node* value,
                                                         Node* native_context) {
  Node* const value_thunk_context =
      CreateValueThunkFunctionContext(value, native_context);
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

  Node* const on_finally = LoadContextElement(context, kOnFinallySlot);

  // 2.a Let result be ?  Call(onFinally, undefined).
  Callable call_callable = CodeFactory::Call(isolate());
  Node* result =
      CallJS(call_callable, context, on_finally, UndefinedConstant());

  // 2.b Let promise be !  PromiseResolve( %Promise%, result).
  Node* const promise = AllocateAndInitJSPromise(context);
  InternalResolvePromise(context, promise, result);

  // 2.c Let valueThunk be equivalent to a function that returns value.
  Node* native_context = LoadNativeContext(context);
  Node* const value_thunk = CreateValueThunkFunction(value, native_context);

  // 2.d Let promiseCapability be !  NewPromiseCapability( %Promise%).
  Node* const promise_capability = AllocateAndInitJSPromise(context, promise);

  // 2.e Return PerformPromiseThen(promise, valueThunk, undefined,
  // promiseCapability).
  InternalPerformPromiseThen(context, promise, value_thunk, UndefinedConstant(),
                             promise_capability, UndefinedConstant(),
                             UndefinedConstant());
  Return(promise_capability);
}

TF_BUILTIN(PromiseThrowerFinally, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);

  Node* const reason = LoadContextElement(context, kOnFinallySlot);
  CallRuntime(Runtime::kThrow, context, reason);
  Unreachable();
}

Node* PromiseBuiltinsAssembler::CreateThrowerFunctionContext(
    Node* reason, Node* native_context) {
  Node* const context =
      CreatePromiseContext(native_context, kOnFinallyContextLength);
  StoreContextElementNoWriteBarrier(context, kOnFinallySlot, reason);
  return context;
}

Node* PromiseBuiltinsAssembler::CreateThrowerFunction(Node* reason,
                                                      Node* native_context) {
  Node* const thrower_context =
      CreateThrowerFunctionContext(reason, native_context);
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

  Node* const on_finally = LoadContextElement(context, kOnFinallySlot);

  // 2.a Let result be ?  Call(onFinally, undefined).
  Callable call_callable = CodeFactory::Call(isolate());
  Node* result =
      CallJS(call_callable, context, on_finally, UndefinedConstant());

  // 2.b Let promise be !  PromiseResolve( %Promise%, result).
  Node* const promise = AllocateAndInitJSPromise(context);
  InternalResolvePromise(context, promise, result);

  // 2.c Let thrower be equivalent to a function that throws reason.
  Node* native_context = LoadNativeContext(context);
  Node* const thrower = CreateThrowerFunction(reason, native_context);

  // 2.d Let promiseCapability be !  NewPromiseCapability( %Promise%).
  Node* const promise_capability = AllocateAndInitJSPromise(context, promise);

  // 2.e Return PerformPromiseThen(promise, thrower, undefined,
  // promiseCapability).
  InternalPerformPromiseThen(context, promise, thrower, UndefinedConstant(),
                             promise_capability, UndefinedConstant(),
                             UndefinedConstant());
  Return(promise_capability);
}

TF_BUILTIN(PromiseFinally, PromiseBuiltinsAssembler) {
  CSA_ASSERT_JS_ARGC_EQ(this, 1);

  // 1.  Let promise be the this value.
  Node* const promise = Parameter(Descriptor::kReceiver);
  Node* const on_finally = Parameter(Descriptor::kOnFinally);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. If IsPromise(promise) is false, throw a TypeError exception.
  ThrowIfNotInstanceType(context, promise, JS_PROMISE_TYPE,
                         "Promise.prototype.finally");

  VARIABLE(var_then_finally, MachineRepresentation::kTagged);
  VARIABLE(var_catch_finally, MachineRepresentation::kTagged);

  Label if_notcallable(this, Label::kDeferred), perform_finally(this);

  // 3. Let thenFinally be !  CreateThenFinally(onFinally).
  // 4. Let catchFinally be !  CreateCatchFinally(onFinally).
  GotoIf(TaggedIsSmi(on_finally), &if_notcallable);
  Node* const on_finally_map = LoadMap(on_finally);
  GotoIfNot(IsCallableMap(on_finally_map), &if_notcallable);

  Node* const native_context = LoadNativeContext(context);
  Node* then_finally = nullptr;
  Node* catch_finally = nullptr;
  std::tie(then_finally, catch_finally) =
      CreatePromiseFinallyFunctions(on_finally, native_context);
  var_then_finally.Bind(then_finally);
  var_catch_finally.Bind(catch_finally);
  Goto(&perform_finally);

  BIND(&if_notcallable);
  {
    var_then_finally.Bind(on_finally);
    var_catch_finally.Bind(on_finally);
    Goto(&perform_finally);
  }

  // 5. Return PerformPromiseThen(promise, valueThunk, undefined,
  // promiseCapability).
  BIND(&perform_finally);
  Label if_nativepromise(this), if_custompromise(this, Label::kDeferred);
  BranchIfFastPath(context, promise, &if_nativepromise, &if_custompromise);

  BIND(&if_nativepromise);
  {
    Node* deferred_promise = AllocateAndInitJSPromise(context, promise);
    InternalPerformPromiseThen(context, promise, var_then_finally.value(),
                               var_catch_finally.value(), deferred_promise,
                               UndefinedConstant(), UndefinedConstant());
    Return(deferred_promise);
  }

  BIND(&if_custompromise);
  {
    Node* const then =
        GetProperty(context, promise, isolate()->factory()->then_string());
    Callable call_callable = CodeFactory::Call(isolate());
    // 5. Return ?  Invoke(promise, "then", « thenFinally, catchFinally »).
    Node* const result =
        CallJS(call_callable, context, then, promise, var_then_finally.value(),
               var_catch_finally.value());
    Return(result);
  }
}

TF_BUILTIN(ResolveNativePromise, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, HasInstanceType(promise, JS_PROMISE_TYPE));
  InternalResolvePromise(context, promise, value);
  Return(UndefinedConstant());
}

TF_BUILTIN(RejectNativePromise, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const debug_event = Parameter(Descriptor::kDebugEvent);
  Node* const context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, HasInstanceType(promise, JS_PROMISE_TYPE));
  CSA_ASSERT(this, IsBoolean(debug_event));
  InternalPromiseReject(context, promise, value, debug_event);
  Return(UndefinedConstant());
}

TF_BUILTIN(PerformNativePromiseThen, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const resolve_reaction = Parameter(Descriptor::kResolveReaction);
  Node* const reject_reaction = Parameter(Descriptor::kRejectReaction);
  Node* const result_promise = Parameter(Descriptor::kResultPromise);
  Node* const context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, HasInstanceType(result_promise, JS_PROMISE_TYPE));

  InternalPerformPromiseThen(context, promise, resolve_reaction,
                             reject_reaction, result_promise,
                             UndefinedConstant(), UndefinedConstant());
  Return(result_promise);
}

}  // namespace internal
}  // namespace v8
