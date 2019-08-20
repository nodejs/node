// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-async-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/heap/factory-inl.h"
#include "src/objects/js-promise.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

using compiler::Node;

namespace {
// Describe fields of Context associated with the AsyncIterator unwrap closure.
class ValueUnwrapContext {
 public:
  enum Fields { kDoneSlot = Context::MIN_CONTEXT_SLOTS, kLength };
};

}  // namespace

Node* AsyncBuiltinsAssembler::AwaitOld(Node* context, Node* generator,
                                       Node* value, Node* outer_promise,
                                       Node* on_resolve_context_index,
                                       Node* on_reject_context_index,
                                       Node* is_predicted_as_caught) {
  Node* const native_context = LoadNativeContext(context);

  static const int kWrappedPromiseOffset =
      FixedArray::SizeFor(Context::MIN_CONTEXT_SLOTS);
  static const int kResolveClosureOffset =
      kWrappedPromiseOffset + JSPromise::kSizeWithEmbedderFields;
  static const int kRejectClosureOffset =
      kResolveClosureOffset + JSFunction::kSizeWithoutPrototype;
  static const int kTotalSize =
      kRejectClosureOffset + JSFunction::kSizeWithoutPrototype;

  TNode<HeapObject> base = AllocateInNewSpace(kTotalSize);
  TNode<Context> closure_context = UncheckedCast<Context>(base);
  {
    // Initialize the await context, storing the {generator} as extension.
    StoreMapNoWriteBarrier(closure_context, RootIndex::kAwaitContextMap);
    StoreObjectFieldNoWriteBarrier(closure_context, Context::kLengthOffset,
                                   SmiConstant(Context::MIN_CONTEXT_SLOTS));
    Node* const empty_scope_info =
        LoadContextElement(native_context, Context::SCOPE_INFO_INDEX);
    StoreContextElementNoWriteBarrier(
        closure_context, Context::SCOPE_INFO_INDEX, empty_scope_info);
    StoreContextElementNoWriteBarrier(closure_context, Context::PREVIOUS_INDEX,
                                      native_context);
    StoreContextElementNoWriteBarrier(closure_context, Context::EXTENSION_INDEX,
                                      generator);
    StoreContextElementNoWriteBarrier(
        closure_context, Context::NATIVE_CONTEXT_INDEX, native_context);
  }

  // Let promiseCapability be ! NewPromiseCapability(%Promise%).
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  CSA_ASSERT(this, IsFunctionWithPrototypeSlotMap(LoadMap(promise_fun)));
  Node* const promise_map =
      LoadObjectField(promise_fun, JSFunction::kPrototypeOrInitialMapOffset);
  // Assert that the JSPromise map has an instance size is
  // JSPromise::kSizeWithEmbedderFields.
  CSA_ASSERT(this, WordEqual(LoadMapInstanceSizeInWords(promise_map),
                             IntPtrConstant(JSPromise::kSizeWithEmbedderFields /
                                            kTaggedSize)));
  TNode<HeapObject> wrapped_value = InnerAllocate(base, kWrappedPromiseOffset);
  {
    // Initialize Promise
    StoreMapNoWriteBarrier(wrapped_value, promise_map);
    StoreObjectFieldRoot(wrapped_value, JSPromise::kPropertiesOrHashOffset,
                         RootIndex::kEmptyFixedArray);
    StoreObjectFieldRoot(wrapped_value, JSPromise::kElementsOffset,
                         RootIndex::kEmptyFixedArray);
    PromiseInit(wrapped_value);
  }

  // Initialize resolve handler
  TNode<HeapObject> on_resolve = InnerAllocate(base, kResolveClosureOffset);
  InitializeNativeClosure(closure_context, native_context, on_resolve,
                          on_resolve_context_index);

  // Initialize reject handler
  TNode<HeapObject> on_reject = InnerAllocate(base, kRejectClosureOffset);
  InitializeNativeClosure(closure_context, native_context, on_reject,
                          on_reject_context_index);

  VARIABLE(var_throwaway, MachineRepresentation::kTaggedPointer,
           UndefinedConstant());

  // Deal with PromiseHooks and debug support in the runtime. This
  // also allocates the throwaway promise, which is only needed in
  // case of PromiseHooks or debugging.
  Label if_debugging(this, Label::kDeferred), do_resolve_promise(this);
  Branch(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_debugging, &do_resolve_promise);
  BIND(&if_debugging);
  var_throwaway.Bind(CallRuntime(Runtime::kAwaitPromisesInitOld, context, value,
                                 wrapped_value, outer_promise, on_reject,
                                 is_predicted_as_caught));
  Goto(&do_resolve_promise);
  BIND(&do_resolve_promise);

  // Perform ! Call(promiseCapability.[[Resolve]], undefined, « promise »).
  CallBuiltin(Builtins::kResolvePromise, context, wrapped_value, value);

  return CallBuiltin(Builtins::kPerformPromiseThen, context, wrapped_value,
                     on_resolve, on_reject, var_throwaway.value());
}

Node* AsyncBuiltinsAssembler::AwaitOptimized(Node* context, Node* generator,
                                             Node* promise, Node* outer_promise,
                                             Node* on_resolve_context_index,
                                             Node* on_reject_context_index,
                                             Node* is_predicted_as_caught) {
  Node* const native_context = LoadNativeContext(context);
  CSA_ASSERT(this, IsJSPromise(promise));

  static const int kResolveClosureOffset =
      FixedArray::SizeFor(Context::MIN_CONTEXT_SLOTS);
  static const int kRejectClosureOffset =
      kResolveClosureOffset + JSFunction::kSizeWithoutPrototype;
  static const int kTotalSize =
      kRejectClosureOffset + JSFunction::kSizeWithoutPrototype;

  // 2. Let promise be ? PromiseResolve(« promise »).
  // Node* const promise =
  // CallBuiltin(Builtins::kPromiseResolve, context, promise_fun, value);

  TNode<HeapObject> base = AllocateInNewSpace(kTotalSize);
  TNode<Context> closure_context = UncheckedCast<Context>(base);
  {
    // Initialize the await context, storing the {generator} as extension.
    StoreMapNoWriteBarrier(closure_context, RootIndex::kAwaitContextMap);
    StoreObjectFieldNoWriteBarrier(closure_context, Context::kLengthOffset,
                                   SmiConstant(Context::MIN_CONTEXT_SLOTS));
    Node* const empty_scope_info =
        LoadContextElement(native_context, Context::SCOPE_INFO_INDEX);
    StoreContextElementNoWriteBarrier(
        closure_context, Context::SCOPE_INFO_INDEX, empty_scope_info);
    StoreContextElementNoWriteBarrier(closure_context, Context::PREVIOUS_INDEX,
                                      native_context);
    StoreContextElementNoWriteBarrier(closure_context, Context::EXTENSION_INDEX,
                                      generator);
    StoreContextElementNoWriteBarrier(
        closure_context, Context::NATIVE_CONTEXT_INDEX, native_context);
  }

  // Initialize resolve handler
  TNode<HeapObject> on_resolve = InnerAllocate(base, kResolveClosureOffset);
  InitializeNativeClosure(closure_context, native_context, on_resolve,
                          on_resolve_context_index);

  // Initialize reject handler
  TNode<HeapObject> on_reject = InnerAllocate(base, kRejectClosureOffset);
  InitializeNativeClosure(closure_context, native_context, on_reject,
                          on_reject_context_index);

  VARIABLE(var_throwaway, MachineRepresentation::kTaggedPointer,
           UndefinedConstant());

  // Deal with PromiseHooks and debug support in the runtime. This
  // also allocates the throwaway promise, which is only needed in
  // case of PromiseHooks or debugging.
  Label if_debugging(this, Label::kDeferred), do_perform_promise_then(this);
  Branch(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_debugging, &do_perform_promise_then);
  BIND(&if_debugging);
  var_throwaway.Bind(CallRuntime(Runtime::kAwaitPromisesInit, context, promise,
                                 promise, outer_promise, on_reject,
                                 is_predicted_as_caught));
  Goto(&do_perform_promise_then);
  BIND(&do_perform_promise_then);

  return CallBuiltin(Builtins::kPerformPromiseThen, native_context, promise,
                     on_resolve, on_reject, var_throwaway.value());
}

Node* AsyncBuiltinsAssembler::Await(Node* context, Node* generator, Node* value,
                                    Node* outer_promise,
                                    Node* on_resolve_context_index,
                                    Node* on_reject_context_index,
                                    Node* is_predicted_as_caught) {
  VARIABLE(result, MachineRepresentation::kTagged);
  Label if_old(this), if_new(this), done(this),
      if_slow_constructor(this, Label::kDeferred);

  // We do the `PromiseResolve(%Promise%,value)` avoiding to unnecessarily
  // create wrapper promises. Now if {value} is already a promise with the
  // intrinsics %Promise% constructor as its "constructor", we don't need
  // to allocate the wrapper promise and can just use the `AwaitOptimized`
  // logic.
  GotoIf(TaggedIsSmi(value), &if_old);
  Node* const value_map = LoadMap(value);
  GotoIfNot(IsJSPromiseMap(value_map), &if_old);
  // We can skip the "constructor" lookup on {value} if it's [[Prototype]]
  // is the (initial) Promise.prototype and the @@species protector is
  // intact, as that guards the lookup path for "constructor" on
  // JSPromise instances which have the (initial) Promise.prototype.
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfNot(WordEqual(LoadMapPrototype(value_map), promise_prototype),
            &if_slow_constructor);
  Branch(IsPromiseSpeciesProtectorCellInvalid(), &if_slow_constructor, &if_new);

  // At this point, {value} doesn't have the initial promise prototype or
  // the promise @@species protector was invalidated, but {value} could still
  // have the %Promise% as its "constructor", so we need to check that as well.
  BIND(&if_slow_constructor);
  {
    Node* const value_constructor =
        GetProperty(context, value, isolate()->factory()->constructor_string());
    Node* const promise_function =
        LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
    Branch(WordEqual(value_constructor, promise_function), &if_new, &if_old);
  }

  BIND(&if_old);
  result.Bind(AwaitOld(context, generator, value, outer_promise,
                       on_resolve_context_index, on_reject_context_index,
                       is_predicted_as_caught));
  Goto(&done);

  BIND(&if_new);
  result.Bind(AwaitOptimized(context, generator, value, outer_promise,
                             on_resolve_context_index, on_reject_context_index,
                             is_predicted_as_caught));
  Goto(&done);

  BIND(&done);
  return result.value();
}

void AsyncBuiltinsAssembler::InitializeNativeClosure(Node* context,
                                                     Node* native_context,
                                                     Node* function,
                                                     Node* context_index) {
  TNode<Map> function_map = CAST(LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
  // Ensure that we don't have to initialize prototype_or_initial_map field of
  // JSFunction.
  CSA_ASSERT(this, WordEqual(LoadMapInstanceSizeInWords(function_map),
                             IntPtrConstant(JSFunction::kSizeWithoutPrototype /
                                            kTaggedSize)));
  STATIC_ASSERT(JSFunction::kSizeWithoutPrototype == 7 * kTaggedSize);
  StoreMapNoWriteBarrier(function, function_map);
  StoreObjectFieldRoot(function, JSObject::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(function, JSObject::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(function, JSFunction::kFeedbackCellOffset,
                       RootIndex::kManyClosuresCell);

  TNode<SharedFunctionInfo> shared_info =
      CAST(LoadContextElement(native_context, context_index));
  StoreObjectFieldNoWriteBarrier(
      function, JSFunction::kSharedFunctionInfoOffset, shared_info);
  StoreObjectFieldNoWriteBarrier(function, JSFunction::kContextOffset, context);

  // For the native closures that are initialized here (for `await`)
  // we know that their SharedFunctionInfo::function_data() slot
  // contains a builtin index (as Smi), so there's no need to use
  // CodeStubAssembler::GetSharedFunctionInfoCode() helper here,
  // which almost doubles the size of `await` builtins (unnecessarily).
  TNode<Smi> builtin_id = LoadObjectField<Smi>(
      shared_info, SharedFunctionInfo::kFunctionDataOffset);
  TNode<Code> code = LoadBuiltin(builtin_id);
  StoreObjectFieldNoWriteBarrier(function, JSFunction::kCodeOffset, code);
}

Node* AsyncBuiltinsAssembler::CreateUnwrapClosure(Node* native_context,
                                                  Node* done) {
  Node* const map = LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
  Node* const on_fulfilled_shared = LoadContextElement(
      native_context, Context::ASYNC_ITERATOR_VALUE_UNWRAP_SHARED_FUN);
  CSA_ASSERT(this,
             HasInstanceType(on_fulfilled_shared, SHARED_FUNCTION_INFO_TYPE));
  Node* const closure_context =
      AllocateAsyncIteratorValueUnwrapContext(native_context, done);
  return AllocateFunctionWithMapAndContext(map, on_fulfilled_shared,
                                           closure_context);
}

Node* AsyncBuiltinsAssembler::AllocateAsyncIteratorValueUnwrapContext(
    Node* native_context, Node* done) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  CSA_ASSERT(this, IsBoolean(done));

  Node* const context =
      CreatePromiseContext(native_context, ValueUnwrapContext::kLength);
  StoreContextElementNoWriteBarrier(context, ValueUnwrapContext::kDoneSlot,
                                    done);
  return context;
}

TF_BUILTIN(AsyncIteratorValueUnwrap, AsyncBuiltinsAssembler) {
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const done = LoadContextElement(context, ValueUnwrapContext::kDoneSlot);
  CSA_ASSERT(this, IsBoolean(done));

  Node* const unwrapped_value =
      CallBuiltin(Builtins::kCreateIterResultObject, context, value, done);

  Return(unwrapped_value);
}

}  // namespace internal
}  // namespace v8
