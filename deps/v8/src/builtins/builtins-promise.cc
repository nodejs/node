// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-promise.h"
#include "src/builtins/builtins-constructor.h"
#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;
typedef CodeStubAssembler::ParameterMode ParameterMode;
typedef compiler::CodeAssemblerState CodeAssemblerState;

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
  StoreObjectField(promise, JSPromise::kStatusOffset,
                   SmiConstant(v8::Promise::kPending));
  StoreObjectField(promise, JSPromise::kFlagsOffset, SmiConstant(0));
}

Node* PromiseBuiltinsAssembler::AllocateAndInitJSPromise(Node* context) {
  return AllocateAndInitJSPromise(context, UndefinedConstant());
}

Node* PromiseBuiltinsAssembler::AllocateAndInitJSPromise(Node* context,
                                                         Node* parent) {
  Node* const instance = AllocateJSPromise(context);
  PromiseInit(instance);

  Label out(this);
  GotoUnless(IsPromiseHookEnabled(), &out);
  CallRuntime(Runtime::kPromiseHookInit, context, instance, parent);
  Goto(&out);

  Bind(&out);
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

  Label out(this);
  GotoUnless(IsPromiseHookEnabled(), &out);
  CallRuntime(Runtime::kPromiseHookInit, context, instance,
              UndefinedConstant());
  Goto(&out);

  Bind(&out);
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

  Node* native_context = LoadNativeContext(context);

  Node* map = LoadRoot(Heap::kJSPromiseCapabilityMapRootIndex);
  Node* capability = AllocateJSObjectFromMap(map);

  StoreObjectFieldNoWriteBarrier(
      capability, JSPromiseCapability::kPromiseOffset, UndefinedConstant());
  StoreObjectFieldNoWriteBarrier(
      capability, JSPromiseCapability::kResolveOffset, UndefinedConstant());
  StoreObjectFieldNoWriteBarrier(capability, JSPromiseCapability::kRejectOffset,
                                 UndefinedConstant());

  Variable var_result(this, MachineRepresentation::kTagged);
  var_result.Bind(capability);

  Label if_builtin_promise(this), if_custom_promise(this, Label::kDeferred),
      out(this);
  Branch(WordEqual(constructor,
                   LoadContextElement(native_context,
                                      Context::PROMISE_FUNCTION_INDEX)),
         &if_builtin_promise, &if_custom_promise);

  Bind(&if_builtin_promise);
  {
    Node* promise = AllocateJSPromise(context);
    PromiseInit(promise);
    StoreObjectFieldNoWriteBarrier(
        capability, JSPromiseCapability::kPromiseOffset, promise);

    Node* resolve = nullptr;
    Node* reject = nullptr;

    std::tie(resolve, reject) =
        CreatePromiseResolvingFunctions(promise, debug_event, native_context);
    StoreObjectField(capability, JSPromiseCapability::kResolveOffset, resolve);
    StoreObjectField(capability, JSPromiseCapability::kRejectOffset, reject);

    GotoUnless(IsPromiseHookEnabled(), &out);
    CallRuntime(Runtime::kPromiseHookInit, context, promise,
                UndefinedConstant());
    Goto(&out);
  }

  Bind(&if_custom_promise);
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
    GotoUnless(IsCallableMap(LoadMap(resolve)), &if_notcallable);

    Node* reject =
        LoadObjectField(capability, JSPromiseCapability::kRejectOffset);
    GotoIf(TaggedIsSmi(reject), &if_notcallable);
    GotoUnless(IsCallableMap(LoadMap(reject)), &if_notcallable);

    StoreObjectField(capability, JSPromiseCapability::kPromiseOffset, promise);

    Goto(&out);

    Bind(&if_notcallable);
    Node* message = SmiConstant(MessageTemplate::kPromiseNonCallable);
    StoreObjectField(capability, JSPromiseCapability::kPromiseOffset,
                     UndefinedConstant());
    StoreObjectField(capability, JSPromiseCapability::kResolveOffset,
                     UndefinedConstant());
    StoreObjectField(capability, JSPromiseCapability::kRejectOffset,
                     UndefinedConstant());
    CallRuntime(Runtime::kThrowTypeError, context, message);
    var_result.Bind(UndefinedConstant());
    Goto(&out);
  }

  Bind(&out);
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
  Variable var_value_map(this, MachineRepresentation::kTagged);

  GotoIf(TaggedIsSmi(value), &throw_exception);

  // Load the instance type of the {value}.
  var_value_map.Bind(LoadMap(value));
  Node* const value_instance_type = LoadMapInstanceType(var_value_map.value());

  Branch(IsJSReceiverInstanceType(value_instance_type), &out, &throw_exception);

  // The {value} is not a compatible receiver for this method.
  Bind(&throw_exception);
  {
    Node* const method =
        method_name == nullptr
            ? UndefinedConstant()
            : HeapConstant(
                  isolate()->factory()->NewStringFromAsciiChecked(method_name));
    Node* const message_id = SmiConstant(msg_template);
    CallRuntime(Runtime::kThrowTypeError, context, message_id, method);
    var_value_map.Bind(UndefinedConstant());
    Goto(&out);  // Never reached.
  }

  Bind(&out);
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

Node* PromiseBuiltinsAssembler::SpeciesConstructor(Node* context, Node* object,
                                                   Node* default_constructor) {
  Isolate* isolate = this->isolate();
  Variable var_result(this, MachineRepresentation::kTagged);
  var_result.Bind(default_constructor);

  // 2. Let C be ? Get(O, "constructor").
  Node* const constructor_str =
      HeapConstant(isolate->factory()->constructor_string());
  Callable getproperty_callable = CodeFactory::GetProperty(isolate);
  Node* const constructor =
      CallStub(getproperty_callable, context, object, constructor_str);

  // 3. If C is undefined, return defaultConstructor.
  Label out(this);
  GotoIf(IsUndefined(constructor), &out);

  // 4. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, constructor,
                       MessageTemplate::kConstructorNotReceiver);

  // 5. Let S be ? Get(C, @@species).
  Node* const species_symbol =
      HeapConstant(isolate->factory()->species_symbol());
  Node* const species =
      CallStub(getproperty_callable, context, constructor, species_symbol);

  // 6. If S is either undefined or null, return defaultConstructor.
  GotoIf(IsUndefined(species), &out);
  GotoIf(WordEqual(species, NullConstant()), &out);

  // 7. If IsConstructor(S) is true, return S.
  Label throw_error(this);
  Node* species_bitfield = LoadMapBitField(LoadMap(species));
  GotoUnless(Word32Equal(Word32And(species_bitfield,
                                   Int32Constant((1 << Map::kIsConstructor))),
                         Int32Constant(1 << Map::kIsConstructor)),
             &throw_error);
  var_result.Bind(species);
  Goto(&out);

  // 8. Throw a TypeError exception.
  Bind(&throw_error);
  {
    Node* const message_id =
        SmiConstant(MessageTemplate::kSpeciesNotConstructor);
    CallRuntime(Runtime::kThrowTypeError, context, message_id);
    Goto(&out);
  }

  Bind(&out);
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
  Variable var_deferred_promise(this, MachineRepresentation::kTagged),
      var_deferred_on_resolve(this, MachineRepresentation::kTagged),
      var_deferred_on_reject(this, MachineRepresentation::kTagged);

  Branch(WordEqual(promise_fun, constructor), &fast_promise_capability,
         &promise_capability);

  Bind(&fast_promise_capability);
  {
    Node* const deferred_promise = AllocateAndInitJSPromise(context, promise);
    var_deferred_promise.Bind(deferred_promise);
    var_deferred_on_resolve.Bind(UndefinedConstant());
    var_deferred_on_reject.Bind(UndefinedConstant());
    Goto(&perform_promise_then);
  }

  Bind(&promise_capability);
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
  Bind(&perform_promise_then);
  Node* const result = InternalPerformPromiseThen(
      context, promise, on_resolve, on_reject, var_deferred_promise.value(),
      var_deferred_on_resolve.value(), var_deferred_on_reject.value());
  return result;
}

Node* PromiseBuiltinsAssembler::InternalPerformPromiseThen(
    Node* context, Node* promise, Node* on_resolve, Node* on_reject,
    Node* deferred_promise, Node* deferred_on_resolve,
    Node* deferred_on_reject) {
  Node* const native_context = LoadNativeContext(context);

  Variable var_on_resolve(this, MachineRepresentation::kTagged),
      var_on_reject(this, MachineRepresentation::kTagged);

  var_on_resolve.Bind(on_resolve);
  var_on_reject.Bind(on_reject);

  Label out(this), if_onresolvenotcallable(this), onrejectcheck(this),
      append_callbacks(this);
  GotoIf(TaggedIsSmi(on_resolve), &if_onresolvenotcallable);

  Node* const on_resolve_map = LoadMap(on_resolve);
  Branch(IsCallableMap(on_resolve_map), &onrejectcheck,
         &if_onresolvenotcallable);

  Bind(&if_onresolvenotcallable);
  {
    var_on_resolve.Bind(LoadContextElement(
        native_context, Context::PROMISE_ID_RESOLVE_HANDLER_INDEX));
    Goto(&onrejectcheck);
  }

  Bind(&onrejectcheck);
  {
    Label if_onrejectnotcallable(this);
    GotoIf(TaggedIsSmi(on_reject), &if_onrejectnotcallable);

    Node* const on_reject_map = LoadMap(on_reject);
    Branch(IsCallableMap(on_reject_map), &append_callbacks,
           &if_onrejectnotcallable);

    Bind(&if_onrejectnotcallable);
    {
      var_on_reject.Bind(LoadContextElement(
          native_context, Context::PROMISE_ID_REJECT_HANDLER_INDEX));
      Goto(&append_callbacks);
    }
  }

  Bind(&append_callbacks);
  {
    Label fulfilled_check(this);
    Node* const status = LoadObjectField(promise, JSPromise::kStatusOffset);
    GotoUnless(SmiEqual(status, SmiConstant(v8::Promise::kPending)),
               &fulfilled_check);

    Node* const existing_deferred_promise =
        LoadObjectField(promise, JSPromise::kDeferredPromiseOffset);

    Label if_noexistingcallbacks(this), if_existingcallbacks(this);
    Branch(IsUndefined(existing_deferred_promise), &if_noexistingcallbacks,
           &if_existingcallbacks);

    Bind(&if_noexistingcallbacks);
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

    Bind(&if_existingcallbacks);
    {
      Label if_singlecallback(this), if_multiplecallbacks(this);
      BranchIfJSObject(existing_deferred_promise, &if_singlecallback,
                       &if_multiplecallbacks);

      Bind(&if_singlecallback);
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

      Bind(&if_multiplecallbacks);
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

    Bind(&fulfilled_check);
    {
      Label reject(this);
      Node* const result = LoadObjectField(promise, JSPromise::kResultOffset);
      GotoUnless(WordEqual(status, SmiConstant(v8::Promise::kFulfilled)),
                 &reject);

      Node* info = AllocatePromiseReactionJobInfo(
          result, var_on_resolve.value(), deferred_promise, deferred_on_resolve,
          deferred_on_reject, context);
      // TODO(gsathya): Move this to TF
      CallRuntime(Runtime::kEnqueuePromiseReactionJob, context, promise, info,
                  SmiConstant(v8::Promise::kFulfilled));
      Goto(&out);

      Bind(&reject);
      {
        Node* const has_handler = PromiseHasHandler(promise);
        Label enqueue(this);

        // TODO(gsathya): Fold these runtime calls and move to TF.
        GotoIf(has_handler, &enqueue);
        CallRuntime(Runtime::kPromiseRevokeReject, context, promise);
        Goto(&enqueue);

        Bind(&enqueue);
        {
          Node* info = AllocatePromiseReactionJobInfo(
              result, var_on_reject.value(), deferred_promise,
              deferred_on_resolve, deferred_on_reject, context);
          // TODO(gsathya): Move this to TF
          CallRuntime(Runtime::kEnqueuePromiseReactionJob, context, promise,
                      info, SmiConstant(v8::Promise::kRejected));
          Goto(&out);
        }
      }
    }
  }

  Bind(&out);
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

  GotoUnless(has_initialmap, if_ismodified);

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
  StoreObjectFieldNoWriteBarrier(info,
                                 PromiseResolveThenableJobInfo::kDebugIdOffset,
                                 SmiConstant(kDebugPromiseNoID));
  StoreObjectFieldNoWriteBarrier(
      info, PromiseResolveThenableJobInfo::kContextOffset, context);
  return info;
}

void PromiseBuiltinsAssembler::InternalResolvePromise(Node* context,
                                                      Node* promise,
                                                      Node* result) {
  Isolate* isolate = this->isolate();

  Variable var_reason(this, MachineRepresentation::kTagged),
      var_then(this, MachineRepresentation::kTagged);

  Label do_enqueue(this), fulfill(this), if_cycle(this, Label::kDeferred),
      if_rejectpromise(this, Label::kDeferred), out(this);

  Label cycle_check(this);
  GotoUnless(IsPromiseHookEnabled(), &cycle_check);
  CallRuntime(Runtime::kPromiseHookResolve, context, promise);
  Goto(&cycle_check);

  Bind(&cycle_check);
  // 6. If SameValue(resolution, promise) is true, then
  GotoIf(SameValue(promise, result, context), &if_cycle);

  // 7. If Type(resolution) is not Object, then
  GotoIf(TaggedIsSmi(result), &fulfill);
  GotoUnless(IsJSReceiver(result), &fulfill);

  Label if_nativepromise(this), if_notnativepromise(this, Label::kDeferred);
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  BranchIfFastPath(native_context, promise_fun, result, &if_nativepromise,
                   &if_notnativepromise);

  // Resolution is a native promise and if it's already resolved or
  // rejected, shortcircuit the resolution procedure by directly
  // reusing the value from the promise.
  Bind(&if_nativepromise);
  {
    Node* const thenable_status =
        LoadObjectField(result, JSPromise::kStatusOffset);
    Node* const thenable_value =
        LoadObjectField(result, JSPromise::kResultOffset);

    Label if_isnotpending(this);
    GotoUnless(SmiEqual(SmiConstant(v8::Promise::kPending), thenable_status),
               &if_isnotpending);

    // TODO(gsathya): Use a marker here instead of the actual then
    // callback, and check for the marker in PromiseResolveThenableJob
    // and perform PromiseThen.
    Node* const then =
        LoadContextElement(native_context, Context::PROMISE_THEN_INDEX);
    var_then.Bind(then);
    Goto(&do_enqueue);

    Bind(&if_isnotpending);
    {
      Label if_fulfilled(this), if_rejected(this);
      Branch(SmiEqual(SmiConstant(v8::Promise::kFulfilled), thenable_status),
             &if_fulfilled, &if_rejected);

      Bind(&if_fulfilled);
      {
        PromiseFulfill(context, promise, thenable_value,
                       v8::Promise::kFulfilled);
        PromiseSetHasHandler(promise);
        Goto(&out);
      }

      Bind(&if_rejected);
      {
        Label reject(this);
        Node* const has_handler = PromiseHasHandler(result);

        // Promise has already been rejected, but had no handler.
        // Revoke previously triggered reject event.
        GotoIf(has_handler, &reject);
        CallRuntime(Runtime::kPromiseRevokeReject, context, result);
        Goto(&reject);

        Bind(&reject);
        // Don't cause a debug event as this case is forwarding a rejection
        InternalPromiseReject(context, promise, thenable_value, false);
        PromiseSetHasHandler(result);
        Goto(&out);
      }
    }
  }

  Bind(&if_notnativepromise);
  {
    // 8. Let then be Get(resolution, "then").
    Node* const then_str = HeapConstant(isolate->factory()->then_string());
    Callable getproperty_callable = CodeFactory::GetProperty(isolate);
    Node* const then =
        CallStub(getproperty_callable, context, result, then_str);

    // 9. If then is an abrupt completion, then
    GotoIfException(then, &if_rejectpromise, &var_reason);

    // 11. If IsCallable(thenAction) is false, then
    GotoIf(TaggedIsSmi(then), &fulfill);
    Node* const then_map = LoadMap(then);
    GotoUnless(IsCallableMap(then_map), &fulfill);
    var_then.Bind(then);
    Goto(&do_enqueue);
  }

  Bind(&do_enqueue);
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
    GotoUnless(IsDebugActive(), &enqueue);

    Node* const debug_id =
        CallRuntime(Runtime::kDebugNextAsyncTaskId, context, promise);
    StoreObjectField(info, PromiseResolveThenableJobInfo::kDebugIdOffset,
                     debug_id);

    GotoIf(TaggedIsSmi(result), &enqueue);
    GotoUnless(HasInstanceType(result, JS_PROMISE_TYPE), &enqueue);

    // Mark the dependency of the new promise on the resolution
    Node* const key =
        HeapConstant(isolate->factory()->promise_handled_by_symbol());
    CallRuntime(Runtime::kSetProperty, context, result, key, promise,
                SmiConstant(STRICT));
    Goto(&enqueue);

    // 12. Perform EnqueueJob("PromiseJobs",
    // PromiseResolveThenableJob, « promise, resolution, thenAction»).
    Bind(&enqueue);
    // TODO(gsathya): Move this to TF
    CallRuntime(Runtime::kEnqueuePromiseResolveThenableJob, context, info);
    Goto(&out);
  }

  // 7.b Return FulfillPromise(promise, resolution).
  Bind(&fulfill);
  {
    PromiseFulfill(context, promise, result, v8::Promise::kFulfilled);
    Goto(&out);
  }

  Bind(&if_cycle);
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
  Bind(&if_rejectpromise);
  {
    InternalPromiseReject(context, promise, var_reason.value(), true);
    Goto(&out);
  }

  Bind(&out);
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

  CallRuntime(Runtime::kEnqueuePromiseReactionJob, context, promise, info,
              status_smi);
  Goto(&debug_async_event_enqueue_recurring);

  Bind(&debug_async_event_enqueue_recurring);
  {
    GotoUnless(IsDebugActive(), &do_promisereset);
    CallRuntime(Runtime::kDebugAsyncEventEnqueueRecurring, context, promise,
                status_smi);
    Goto(&do_promisereset);
  }

  Bind(&do_promisereset);
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
  Variable var_executor(this, MachineRepresentation::kTagged);
  var_executor.Bind(executor);
  Label has_access(this), call_runtime(this, Label::kDeferred);

  // If executor is a bound function, load the bound function until we've
  // reached an actual function.
  Label found_function(this), loop_over_bound_function(this, &var_executor);
  Goto(&loop_over_bound_function);
  Bind(&loop_over_bound_function);
  {
    Node* executor_type = LoadInstanceType(var_executor.value());
    GotoIf(InstanceTypeEqual(executor_type, JS_FUNCTION_TYPE), &found_function);
    GotoUnless(InstanceTypeEqual(executor_type, JS_BOUND_FUNCTION_TYPE),
               &call_runtime);
    var_executor.Bind(LoadObjectField(
        var_executor.value(), JSBoundFunction::kBoundTargetFunctionOffset));
    Goto(&loop_over_bound_function);
  }

  // Load the context from the function and compare it to the Promise
  // constructor's context. If they match, everything is fine, otherwise, bail
  // out to the runtime.
  Bind(&found_function);
  {
    Node* function_context =
        LoadObjectField(var_executor.value(), JSFunction::kContextOffset);
    Node* native_function_context = LoadNativeContext(function_context);
    Branch(WordEqual(native_context, native_function_context), &has_access,
           &call_runtime);
  }

  Bind(&call_runtime);
  {
    Branch(WordEqual(CallRuntime(Runtime::kAllowDynamicFunction, context,
                                 promise_constructor),
                     BooleanConstant(true)),
           &has_access, if_noaccess);
  }

  Bind(&has_access);
}

void PromiseBuiltinsAssembler::InternalPromiseReject(Node* context,
                                                     Node* promise, Node* value,
                                                     Node* debug_event) {
  Label out(this);
  GotoUnless(IsDebugActive(), &out);
  GotoUnless(WordEqual(TrueConstant(), debug_event), &out);
  CallRuntime(Runtime::kDebugPromiseReject, context, promise, value);
  Goto(&out);

  Bind(&out);
  InternalPromiseReject(context, promise, value, false);
}

// This duplicates a lot of logic from PromiseRejectEvent in
// runtime-promise.cc
void PromiseBuiltinsAssembler::InternalPromiseReject(Node* context,
                                                     Node* promise, Node* value,
                                                     bool debug_event) {
  Label fulfill(this), report_unhandledpromise(this), run_promise_hook(this);

  if (debug_event) {
    GotoUnless(IsDebugActive(), &run_promise_hook);
    CallRuntime(Runtime::kDebugPromiseReject, context, promise, value);
    Goto(&run_promise_hook);
  } else {
    Goto(&run_promise_hook);
  }

  Bind(&run_promise_hook);
  {
    GotoUnless(IsPromiseHookEnabled(), &report_unhandledpromise);
    CallRuntime(Runtime::kPromiseHookResolve, context, promise);
    Goto(&report_unhandledpromise);
  }

  Bind(&report_unhandledpromise);
  {
    GotoIf(PromiseHasHandler(promise), &fulfill);
    CallRuntime(Runtime::kReportPromiseReject, context, promise, value);
    Goto(&fulfill);
  }

  Bind(&fulfill);
  PromiseFulfill(context, promise, value, v8::Promise::kRejected);
}

// ES#sec-promise-reject-functions
// Promise Reject Functions
TF_BUILTIN(PromiseRejectClosure, PromiseBuiltinsAssembler) {
  Node* const value = Parameter(1);
  Node* const context = Parameter(4);

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

  Bind(&out);
  Return(UndefinedConstant());
}

TF_BUILTIN(PromiseConstructor, PromiseBuiltinsAssembler) {
  Node* const executor = Parameter(1);
  Node* const new_target = Parameter(2);
  Node* const context = Parameter(4);
  Isolate* isolate = this->isolate();

  Label if_targetisundefined(this, Label::kDeferred);

  GotoIf(IsUndefined(new_target), &if_targetisundefined);

  Label if_notcallable(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(executor), &if_notcallable);

  Node* const executor_map = LoadMap(executor);
  GotoUnless(IsCallableMap(executor_map), &if_notcallable);

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

  Variable var_result(this, MachineRepresentation::kTagged),
      var_reject_call(this, MachineRepresentation::kTagged),
      var_reason(this, MachineRepresentation::kTagged);

  Bind(&if_targetisnotmodified);
  {
    Node* const instance = AllocateAndInitJSPromise(context);
    var_result.Bind(instance);
    Goto(&debug_push);
  }

  Bind(&if_targetismodified);
  {
    ConstructorBuiltinsAssembler constructor_assembler(this->state());
    Node* const instance = constructor_assembler.EmitFastNewObject(
        context, promise_fun, new_target);
    PromiseInit(instance);
    var_result.Bind(instance);

    GotoUnless(IsPromiseHookEnabled(), &debug_push);
    CallRuntime(Runtime::kPromiseHookInit, context, instance,
                UndefinedConstant());
    Goto(&debug_push);
  }

  Bind(&debug_push);
  {
    GotoUnless(is_debug_active, &run_executor);
    CallRuntime(Runtime::kDebugPushPromise, context, var_result.value());
    Goto(&run_executor);
  }

  Bind(&run_executor);
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

    Bind(&if_rejectpromise);
    {
      Callable call_callable = CodeFactory::Call(isolate);
      CallJS(call_callable, context, reject, UndefinedConstant(),
             var_reason.value());
      Branch(is_debug_active, &debug_pop, &out);
    }

    Bind(&debug_pop);
    {
      CallRuntime(Runtime::kDebugPopPromise, context);
      Goto(&out);
    }
    Bind(&out);
    Return(var_result.value());
  }

  // 1. If NewTarget is undefined, throw a TypeError exception.
  Bind(&if_targetisundefined);
  {
    Node* const message_id = SmiConstant(MessageTemplate::kNotAPromise);
    CallRuntime(Runtime::kThrowTypeError, context, message_id, new_target);
    Return(UndefinedConstant());  // Never reached.
  }

  // 2. If IsCallable(executor) is false, throw a TypeError exception.
  Bind(&if_notcallable);
  {
    Node* const message_id =
        SmiConstant(MessageTemplate::kResolverNotAFunction);
    CallRuntime(Runtime::kThrowTypeError, context, message_id, executor);
    Return(UndefinedConstant());  // Never reached.
  }

  // Silently fail if the stack looks fishy.
  Bind(&if_noaccess);
  {
    Node* const counter_id =
        SmiConstant(v8::Isolate::kPromiseConstructorReturnedUndefined);
    CallRuntime(Runtime::kIncrementUseCounter, context, counter_id);
    Return(UndefinedConstant());
  }
}

TF_BUILTIN(PromiseInternalConstructor, PromiseBuiltinsAssembler) {
  Node* const parent = Parameter(1);
  Node* const context = Parameter(4);
  Return(AllocateAndInitJSPromise(context, parent));
}

TF_BUILTIN(IsPromise, PromiseBuiltinsAssembler) {
  Node* const maybe_promise = Parameter(1);
  Label if_notpromise(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(maybe_promise), &if_notpromise);

  Node* const result =
      SelectBooleanConstant(HasInstanceType(maybe_promise, JS_PROMISE_TYPE));
  Return(result);

  Bind(&if_notpromise);
  Return(FalseConstant());
}

TF_BUILTIN(PerformPromiseThen, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(1);
  Node* const on_resolve = Parameter(2);
  Node* const on_reject = Parameter(3);
  Node* const deferred_promise = Parameter(4);
  Node* const context = Parameter(7);

  // No deferred_on_resolve/deferred_on_reject because this is just an
  // internal promise created by async-await.
  Node* const result = InternalPerformPromiseThen(
      context, promise, on_resolve, on_reject, deferred_promise,
      UndefinedConstant(), UndefinedConstant());

  // TODO(gsathya): This is unused, but value is returned according to spec.
  Return(result);
}

// ES#sec-promise.prototype.then
// Promise.prototype.catch ( onFulfilled, onRejected )
TF_BUILTIN(PromiseThen, PromiseBuiltinsAssembler) {
  // 1. Let promise be the this value.
  Node* const promise = Parameter(0);
  Node* const on_resolve = Parameter(1);
  Node* const on_reject = Parameter(2);
  Node* const context = Parameter(5);

  Node* const result =
      InternalPromiseThen(context, promise, on_resolve, on_reject);
  Return(result);
}

// ES#sec-promise-resolve-functions
// Promise Resolve Functions
TF_BUILTIN(PromiseResolveClosure, PromiseBuiltinsAssembler) {
  Node* const value = Parameter(1);
  Node* const context = Parameter(4);

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

  Bind(&out);
  Return(UndefinedConstant());
}

TF_BUILTIN(ResolvePromise, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(1);
  Node* const result = Parameter(2);
  Node* const context = Parameter(5);

  InternalResolvePromise(context, promise, result);
  Return(UndefinedConstant());
}

TF_BUILTIN(PromiseHandleReject, PromiseBuiltinsAssembler) {
  typedef PromiseHandleRejectDescriptor Descriptor;

  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const on_reject = Parameter(Descriptor::kOnReject);
  Node* const exception = Parameter(Descriptor::kException);
  Node* const context = Parameter(Descriptor::kContext);

  Callable call_callable = CodeFactory::Call(isolate());
  Variable var_unused(this, MachineRepresentation::kTagged);

  Label if_internalhandler(this), if_customhandler(this, Label::kDeferred);
  Branch(IsUndefined(on_reject), &if_internalhandler, &if_customhandler);

  Bind(&if_internalhandler);
  {
    InternalPromiseReject(context, promise, exception, false);
    Return(UndefinedConstant());
  }

  Bind(&if_customhandler);
  {
    CallJS(call_callable, context, on_reject, UndefinedConstant(), exception);
    Return(UndefinedConstant());
  }
}

TF_BUILTIN(PromiseHandle, PromiseBuiltinsAssembler) {
  Node* const value = Parameter(1);
  Node* const handler = Parameter(2);
  Node* const deferred_promise = Parameter(3);
  Node* const deferred_on_resolve = Parameter(4);
  Node* const deferred_on_reject = Parameter(5);
  Node* const context = Parameter(8);
  Isolate* isolate = this->isolate();

  Variable var_reason(this, MachineRepresentation::kTagged);

  Node* const is_debug_active = IsDebugActive();
  Label run_handler(this), if_rejectpromise(this), promisehook_before(this),
      promisehook_after(this), debug_pop(this);

  GotoUnless(is_debug_active, &promisehook_before);
  CallRuntime(Runtime::kDebugPushPromise, context, deferred_promise);
  Goto(&promisehook_before);

  Bind(&promisehook_before);
  {
    GotoUnless(IsPromiseHookEnabled(), &run_handler);
    CallRuntime(Runtime::kPromiseHookBefore, context, deferred_promise);
    Goto(&run_handler);
  }

  Bind(&run_handler);
  {
    Callable call_callable = CodeFactory::Call(isolate);
    Node* const result =
        CallJS(call_callable, context, handler, UndefinedConstant(), value);

    GotoIfException(result, &if_rejectpromise, &var_reason);

    Label if_internalhandler(this), if_customhandler(this, Label::kDeferred);
    Branch(IsUndefined(deferred_on_resolve), &if_internalhandler,
           &if_customhandler);

    Bind(&if_internalhandler);
    InternalResolvePromise(context, deferred_promise, result);
    Goto(&promisehook_after);

    Bind(&if_customhandler);
    {
      Node* const maybe_exception =
          CallJS(call_callable, context, deferred_on_resolve,
                 UndefinedConstant(), result);
      GotoIfException(maybe_exception, &if_rejectpromise, &var_reason);
      Goto(&promisehook_after);
    }
  }

  Bind(&if_rejectpromise);
  {
    Callable promise_handle_reject = CodeFactory::PromiseHandleReject(isolate);
    CallStub(promise_handle_reject, context, deferred_promise,
             deferred_on_reject, var_reason.value());
    Goto(&promisehook_after);
  }

  Bind(&promisehook_after);
  {
    GotoUnless(IsPromiseHookEnabled(), &debug_pop);
    CallRuntime(Runtime::kPromiseHookAfter, context, deferred_promise);
    Goto(&debug_pop);
  }

  Bind(&debug_pop);
  {
    Label out(this);

    GotoUnless(is_debug_active, &out);
    CallRuntime(Runtime::kDebugPopPromise, context);
    Goto(&out);

    Bind(&out);
    Return(UndefinedConstant());
  }
}

// ES#sec-promise.prototype.catch
// Promise.prototype.catch ( onRejected )
TF_BUILTIN(PromiseCatch, PromiseBuiltinsAssembler) {
  // 1. Let promise be the this value.
  Node* const promise = Parameter(0);
  Node* const on_resolve = UndefinedConstant();
  Node* const on_reject = Parameter(1);
  Node* const context = Parameter(4);

  Label if_internalthen(this), if_customthen(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(promise), &if_customthen);
  BranchIfFastPath(context, promise, &if_internalthen, &if_customthen);

  Bind(&if_internalthen);
  {
    Node* const result =
        InternalPromiseThen(context, promise, on_resolve, on_reject);
    Return(result);
  }

  Bind(&if_customthen);
  {
    Isolate* isolate = this->isolate();
    Node* const then_str = HeapConstant(isolate->factory()->then_string());
    Callable getproperty_callable = CodeFactory::GetProperty(isolate);
    Node* const then =
        CallStub(getproperty_callable, context, promise, then_str);
    Callable call_callable = CodeFactory::Call(isolate);
    Node* const result =
        CallJS(call_callable, context, then, promise, on_resolve, on_reject);
    Return(result);
  }
}

TF_BUILTIN(PromiseResolve, PromiseBuiltinsAssembler) {
  //  1. Let C be the this value.
  Node* receiver = Parameter(0);
  Node* value = Parameter(1);
  Node* context = Parameter(4);
  Isolate* isolate = this->isolate();

  // 2. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "PromiseResolve");

  Label if_valueisnativepromise(this), if_valueisnotnativepromise(this),
      if_valueisnotpromise(this);

  // 3.If IsPromise(x) is true, then
  GotoIf(TaggedIsSmi(value), &if_valueisnotpromise);

  // This shortcircuits the constructor lookups.
  GotoUnless(HasInstanceType(value, JS_PROMISE_TYPE), &if_valueisnotpromise);

  // This adds a fast path as non-subclassed native promises don't have
  // an observable constructor lookup.
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  BranchIfFastPath(native_context, promise_fun, value, &if_valueisnativepromise,
                   &if_valueisnotnativepromise);

  Bind(&if_valueisnativepromise);
  {
    GotoUnless(WordEqual(promise_fun, receiver), &if_valueisnotnativepromise);
    Return(value);
  }

  // At this point, value or/and receiver are not native promises, but
  // they could be of the same subclass.
  Bind(&if_valueisnotnativepromise);
  {
    // 3.a Let xConstructor be ? Get(x, "constructor").
    // The constructor lookup is observable.
    Node* const constructor_str =
        HeapConstant(isolate->factory()->constructor_string());
    Callable getproperty_callable = CodeFactory::GetProperty(isolate);
    Node* const constructor =
        CallStub(getproperty_callable, context, value, constructor_str);

    // 3.b If SameValue(xConstructor, C) is true, return x.
    GotoUnless(SameValue(constructor, receiver, context),
               &if_valueisnotpromise);

    Return(value);
  }

  Bind(&if_valueisnotpromise);
  {
    Label if_nativepromise(this), if_notnativepromise(this);
    BranchIfFastPath(context, receiver, &if_nativepromise,
                     &if_notnativepromise);

    // This adds a fast path for native promises that don't need to
    // create NewPromiseCapability.
    Bind(&if_nativepromise);
    {
      Label do_resolve(this);

      Node* const result = AllocateAndInitJSPromise(context);
      InternalResolvePromise(context, result, value);
      Return(result);
    }

    Bind(&if_notnativepromise);
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

TF_BUILTIN(PromiseGetCapabilitiesExecutor, PromiseBuiltinsAssembler) {
  Node* const resolve = Parameter(1);
  Node* const reject = Parameter(2);
  Node* const context = Parameter(5);

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

  Bind(&if_alreadyinvoked);
  Node* message = SmiConstant(MessageTemplate::kPromiseExecutorAlreadyInvoked);
  Return(CallRuntime(Runtime::kThrowTypeError, context, message));
}

TF_BUILTIN(NewPromiseCapability, PromiseBuiltinsAssembler) {
  Node* constructor = Parameter(1);
  Node* debug_event = Parameter(2);
  Node* context = Parameter(5);

  CSA_ASSERT_JS_ARGC_EQ(this, 2);

  Return(NewPromiseCapability(context, constructor, debug_event));
}

TF_BUILTIN(PromiseReject, PromiseBuiltinsAssembler) {
  // 1. Let C be the this value.
  Node* const receiver = Parameter(0);
  Node* const reason = Parameter(1);
  Node* const context = Parameter(4);

  // 2. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "PromiseReject");

  Label if_nativepromise(this), if_custompromise(this, Label::kDeferred);
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  Branch(WordEqual(promise_fun, receiver), &if_nativepromise,
         &if_custompromise);

  Bind(&if_nativepromise);
  {
    Node* const promise = AllocateAndSetJSPromise(
        context, SmiConstant(v8::Promise::kRejected), reason);
    CallRuntime(Runtime::kPromiseRejectEventFromStack, context, promise,
                reason);
    Return(promise);
  }

  Bind(&if_custompromise);
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
  Node* const promise = Parameter(1);
  Node* const reason = Parameter(2);
  Node* const debug_event = Parameter(3);
  Node* const context = Parameter(6);

  InternalPromiseReject(context, promise, reason, debug_event);
  Return(UndefinedConstant());
}

}  // namespace internal
}  // namespace v8
