// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-async-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/heap/factory-inl.h"
#include "src/objects/js-generator.h"
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

TNode<Object> AsyncBuiltinsAssembler::AwaitOld(
    TNode<Context> context, TNode<JSGeneratorObject> generator,
    TNode<Object> value, TNode<JSPromise> outer_promise,
    TNode<SharedFunctionInfo> on_resolve_sfi,
    TNode<SharedFunctionInfo> on_reject_sfi,
    TNode<Oddball> is_predicted_as_caught) {
  const TNode<NativeContext> native_context = LoadNativeContext(context);

  static const int kWrappedPromiseOffset =
      FixedArray::SizeFor(Context::MIN_CONTEXT_EXTENDED_SLOTS);
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
    TNode<Map> map = CAST(
        LoadContextElement(native_context, Context::AWAIT_CONTEXT_MAP_INDEX));
    StoreMapNoWriteBarrier(closure_context, map);
    StoreObjectFieldNoWriteBarrier(
        closure_context, Context::kLengthOffset,
        SmiConstant(Context::MIN_CONTEXT_EXTENDED_SLOTS));
    const TNode<Object> empty_scope_info =
        LoadContextElement(native_context, Context::SCOPE_INFO_INDEX);
    StoreContextElementNoWriteBarrier(
        closure_context, Context::SCOPE_INFO_INDEX, empty_scope_info);
    StoreContextElementNoWriteBarrier(closure_context, Context::PREVIOUS_INDEX,
                                      native_context);
    StoreContextElementNoWriteBarrier(closure_context, Context::EXTENSION_INDEX,
                                      generator);
  }

  // Let promiseCapability be ! NewPromiseCapability(%Promise%).
  const TNode<JSFunction> promise_fun =
      CAST(LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX));
  CSA_ASSERT(this, IsFunctionWithPrototypeSlotMap(LoadMap(promise_fun)));
  const TNode<Map> promise_map = CAST(
      LoadObjectField(promise_fun, JSFunction::kPrototypeOrInitialMapOffset));
  // Assert that the JSPromise map has an instance size is
  // JSPromise::kSizeWithEmbedderFields.
  CSA_ASSERT(this,
             IntPtrEqual(LoadMapInstanceSizeInWords(promise_map),
                         IntPtrConstant(JSPromise::kSizeWithEmbedderFields /
                                        kTaggedSize)));
  TNode<JSPromise> promise;
  {
    // Initialize Promise
    TNode<HeapObject> wrapped_value =
        InnerAllocate(base, kWrappedPromiseOffset);
    StoreMapNoWriteBarrier(wrapped_value, promise_map);
    StoreObjectFieldRoot(wrapped_value, JSPromise::kPropertiesOrHashOffset,
                         RootIndex::kEmptyFixedArray);
    StoreObjectFieldRoot(wrapped_value, JSPromise::kElementsOffset,
                         RootIndex::kEmptyFixedArray);
    promise = CAST(wrapped_value);
    PromiseInit(promise);
  }

  // Initialize resolve handler
  TNode<HeapObject> on_resolve = InnerAllocate(base, kResolveClosureOffset);
  InitializeNativeClosure(closure_context, native_context, on_resolve,
                          on_resolve_sfi);

  // Initialize reject handler
  TNode<HeapObject> on_reject = InnerAllocate(base, kRejectClosureOffset);
  InitializeNativeClosure(closure_context, native_context, on_reject,
                          on_reject_sfi);

  TVARIABLE(HeapObject, var_throwaway, UndefinedConstant());

  // Deal with PromiseHooks and debug support in the runtime. This
  // also allocates the throwaway promise, which is only needed in
  // case of PromiseHooks or debugging.
  Label if_debugging(this, Label::kDeferred), do_resolve_promise(this);
  Branch(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_debugging, &do_resolve_promise);
  BIND(&if_debugging);
  var_throwaway =
      CAST(CallRuntime(Runtime::kAwaitPromisesInitOld, context, value, promise,
                       outer_promise, on_reject, is_predicted_as_caught));
  Goto(&do_resolve_promise);
  BIND(&do_resolve_promise);

  // Perform ! Call(promiseCapability.[[Resolve]], undefined, « promise »).
  CallBuiltin(Builtins::kResolvePromise, context, promise, value);

  return CallBuiltin(Builtins::kPerformPromiseThen, context, promise,
                     on_resolve, on_reject, var_throwaway.value());
}

TNode<Object> AsyncBuiltinsAssembler::AwaitOptimized(
    TNode<Context> context, TNode<JSGeneratorObject> generator,
    TNode<JSPromise> promise, TNode<JSPromise> outer_promise,
    TNode<SharedFunctionInfo> on_resolve_sfi,
    TNode<SharedFunctionInfo> on_reject_sfi,
    TNode<Oddball> is_predicted_as_caught) {
  const TNode<NativeContext> native_context = LoadNativeContext(context);

  static const int kResolveClosureOffset =
      FixedArray::SizeFor(Context::MIN_CONTEXT_EXTENDED_SLOTS);
  static const int kRejectClosureOffset =
      kResolveClosureOffset + JSFunction::kSizeWithoutPrototype;
  static const int kTotalSize =
      kRejectClosureOffset + JSFunction::kSizeWithoutPrototype;

  // 2. Let promise be ? PromiseResolve(« promise »).
  // We skip this step, because promise is already guaranteed to be a
  // JSPRomise at this point.

  TNode<HeapObject> base = AllocateInNewSpace(kTotalSize);
  TNode<Context> closure_context = UncheckedCast<Context>(base);
  {
    // Initialize the await context, storing the {generator} as extension.
    TNode<Map> map = CAST(
        LoadContextElement(native_context, Context::AWAIT_CONTEXT_MAP_INDEX));
    StoreMapNoWriteBarrier(closure_context, map);
    StoreObjectFieldNoWriteBarrier(
        closure_context, Context::kLengthOffset,
        SmiConstant(Context::MIN_CONTEXT_EXTENDED_SLOTS));
    const TNode<Object> empty_scope_info =
        LoadContextElement(native_context, Context::SCOPE_INFO_INDEX);
    StoreContextElementNoWriteBarrier(
        closure_context, Context::SCOPE_INFO_INDEX, empty_scope_info);
    StoreContextElementNoWriteBarrier(closure_context, Context::PREVIOUS_INDEX,
                                      native_context);
    StoreContextElementNoWriteBarrier(closure_context, Context::EXTENSION_INDEX,
                                      generator);
  }

  // Initialize resolve handler
  TNode<HeapObject> on_resolve = InnerAllocate(base, kResolveClosureOffset);
  InitializeNativeClosure(closure_context, native_context, on_resolve,
                          on_resolve_sfi);

  // Initialize reject handler
  TNode<HeapObject> on_reject = InnerAllocate(base, kRejectClosureOffset);
  InitializeNativeClosure(closure_context, native_context, on_reject,
                          on_reject_sfi);

  TVARIABLE(HeapObject, var_throwaway, UndefinedConstant());

  // Deal with PromiseHooks and debug support in the runtime. This
  // also allocates the throwaway promise, which is only needed in
  // case of PromiseHooks or debugging.
  Label if_debugging(this, Label::kDeferred), do_perform_promise_then(this);
  Branch(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_debugging, &do_perform_promise_then);
  BIND(&if_debugging);
  var_throwaway =
      CAST(CallRuntime(Runtime::kAwaitPromisesInit, context, promise, promise,
                       outer_promise, on_reject, is_predicted_as_caught));
  Goto(&do_perform_promise_then);
  BIND(&do_perform_promise_then);

  return CallBuiltin(Builtins::kPerformPromiseThen, native_context, promise,
                     on_resolve, on_reject, var_throwaway.value());
}

TNode<Object> AsyncBuiltinsAssembler::Await(
    TNode<Context> context, TNode<JSGeneratorObject> generator,
    TNode<Object> value, TNode<JSPromise> outer_promise,
    TNode<SharedFunctionInfo> on_resolve_sfi,
    TNode<SharedFunctionInfo> on_reject_sfi,
    TNode<Oddball> is_predicted_as_caught) {
  TVARIABLE(Object, result);
  Label if_old(this), if_new(this), done(this),
      if_slow_constructor(this, Label::kDeferred);

  // We do the `PromiseResolve(%Promise%,value)` avoiding to unnecessarily
  // create wrapper promises. Now if {value} is already a promise with the
  // intrinsics %Promise% constructor as its "constructor", we don't need
  // to allocate the wrapper promise and can just use the `AwaitOptimized`
  // logic.
  GotoIf(TaggedIsSmi(value), &if_old);
  TNode<HeapObject> value_object = CAST(value);
  const TNode<Map> value_map = LoadMap(value_object);
  GotoIfNot(IsJSPromiseMap(value_map), &if_old);
  // We can skip the "constructor" lookup on {value} if it's [[Prototype]]
  // is the (initial) Promise.prototype and the @@species protector is
  // intact, as that guards the lookup path for "constructor" on
  // JSPromise instances which have the (initial) Promise.prototype.
  const TNode<NativeContext> native_context = LoadNativeContext(context);
  const TNode<Object> promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfNot(TaggedEqual(LoadMapPrototype(value_map), promise_prototype),
            &if_slow_constructor);
  Branch(IsPromiseSpeciesProtectorCellInvalid(), &if_slow_constructor, &if_new);

  // At this point, {value} doesn't have the initial promise prototype or
  // the promise @@species protector was invalidated, but {value} could still
  // have the %Promise% as its "constructor", so we need to check that as well.
  BIND(&if_slow_constructor);
  {
    const TNode<Object> value_constructor =
        GetProperty(context, value, isolate()->factory()->constructor_string());
    const TNode<Object> promise_function =
        LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
    Branch(TaggedEqual(value_constructor, promise_function), &if_new, &if_old);
  }

  BIND(&if_old);
  result = AwaitOld(context, generator, value, outer_promise, on_resolve_sfi,
                    on_reject_sfi, is_predicted_as_caught);
  Goto(&done);

  BIND(&if_new);
  result =
      AwaitOptimized(context, generator, CAST(value), outer_promise,
                     on_resolve_sfi, on_reject_sfi, is_predicted_as_caught);
  Goto(&done);

  BIND(&done);
  return result.value();
}

void AsyncBuiltinsAssembler::InitializeNativeClosure(
    TNode<Context> context, TNode<NativeContext> native_context,
    TNode<HeapObject> function, TNode<SharedFunctionInfo> shared_info) {
  TNode<Map> function_map = CAST(LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
  // Ensure that we don't have to initialize prototype_or_initial_map field of
  // JSFunction.
  CSA_ASSERT(this,
             IntPtrEqual(LoadMapInstanceSizeInWords(function_map),
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

  StoreObjectFieldNoWriteBarrier(
      function, JSFunction::kSharedFunctionInfoOffset, shared_info);
  StoreObjectFieldNoWriteBarrier(function, JSFunction::kContextOffset, context);

  // For the native closures that are initialized here (for `await`)
  // we know that their SharedFunctionInfo::function_data(kAcquireLoad) slot
  // contains a builtin index (as Smi), so there's no need to use
  // CodeStubAssembler::GetSharedFunctionInfoCode() helper here,
  // which almost doubles the size of `await` builtins (unnecessarily).
  TNode<Smi> builtin_id = LoadObjectField<Smi>(
      shared_info, SharedFunctionInfo::kFunctionDataOffset);
  TNode<Code> code = LoadBuiltin(builtin_id);
  StoreObjectFieldNoWriteBarrier(function, JSFunction::kCodeOffset, code);
}

TNode<JSFunction> AsyncBuiltinsAssembler::CreateUnwrapClosure(
    TNode<NativeContext> native_context, TNode<Oddball> done) {
  const TNode<Map> map = CAST(LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
  const TNode<SharedFunctionInfo> on_fulfilled_shared =
      AsyncIteratorValueUnwrapSharedFunConstant();
  const TNode<Context> closure_context =
      AllocateAsyncIteratorValueUnwrapContext(native_context, done);
  return AllocateFunctionWithMapAndContext(map, on_fulfilled_shared,
                                           closure_context);
}

TNode<Context> AsyncBuiltinsAssembler::AllocateAsyncIteratorValueUnwrapContext(
    TNode<NativeContext> native_context, TNode<Oddball> done) {
  CSA_ASSERT(this, IsBoolean(done));

  TNode<Context> context = AllocateSyntheticFunctionContext(
      native_context, ValueUnwrapContext::kLength);
  StoreContextElementNoWriteBarrier(context, ValueUnwrapContext::kDoneSlot,
                                    done);
  return context;
}

TF_BUILTIN(AsyncIteratorValueUnwrap, AsyncBuiltinsAssembler) {
  auto value = Parameter<Object>(Descriptor::kValue);
  auto context = Parameter<Context>(Descriptor::kContext);

  const TNode<Object> done =
      LoadContextElement(context, ValueUnwrapContext::kDoneSlot);
  CSA_ASSERT(this, IsBoolean(CAST(done)));

  const TNode<Object> unwrapped_value =
      CallBuiltin(Builtins::kCreateIterResultObject, context, value, done);

  Return(unwrapped_value);
}

}  // namespace internal
}  // namespace v8
