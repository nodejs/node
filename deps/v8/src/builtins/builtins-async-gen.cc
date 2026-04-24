// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-async-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/heap/factory-inl.h"
#include "src/objects/js-generator.h"
#include "src/objects/js-promise.h"
#include "src/objects/microtask.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

#include "src/codegen/define-code-stub-assembler-macros.inc"

namespace {
// Describe fields of Context associated with the AsyncIterator unwrap closure.
class ValueUnwrapContext {
 public:
  enum Fields { kDoneSlot = Context::MIN_CONTEXT_SLOTS, kLength };
};

}  // namespace

TNode<Context> AsyncBuiltinsAssembler::AllocateAwaitContext(
    TNode<NativeContext> native_context, TNode<JSGeneratorObject> generator) {
  static const int kAwaitContextSize =
      FixedArray::SizeFor(Context::MIN_CONTEXT_EXTENDED_SLOTS);
  TNode<Context> await_context =
      UncheckedCast<Context>(AllocateInNewSpace(kAwaitContextSize));
  TNode<Map> map = CAST(LoadContextElementNoCell(
      native_context, Context::AWAIT_CONTEXT_MAP_INDEX));
  StoreMapNoWriteBarrier(await_context, map);
  StoreObjectFieldNoWriteBarrier(
      await_context, Context::kLengthOffset,
      SmiConstant(Context::MIN_CONTEXT_EXTENDED_SLOTS));
  const TNode<Object> empty_scope_info =
      LoadContextElementNoCell(native_context, Context::SCOPE_INFO_INDEX);
  StoreContextElementNoWriteBarrier(await_context, Context::SCOPE_INFO_INDEX,
                                    empty_scope_info);
  StoreContextElementNoWriteBarrier(await_context, Context::PREVIOUS_INDEX,
                                    native_context);
  StoreContextElementNoWriteBarrier(await_context, Context::EXTENSION_INDEX,
                                    generator);
  return await_context;
}

TNode<Object> AsyncBuiltinsAssembler::Await(TNode<Context> context,
                                            TNode<JSGeneratorObject> generator,
                                            TNode<JSAny> value,
                                            TNode<JSPromise> outer_promise,
                                            RootIndex on_resolve_sfi,
                                            RootIndex on_reject_sfi) {
  return Await(context, generator, value, outer_promise,
               [&](TNode<NativeContext> native_context) {
                 TNode<Context> await_context =
                     AllocateAwaitContext(native_context, generator);
                 auto on_resolve = AllocateRootFunctionWithContext(
                     on_resolve_sfi, await_context, native_context);
                 auto on_reject = AllocateRootFunctionWithContext(
                     on_reject_sfi, await_context, native_context);
                 return std::make_pair(on_resolve, on_reject);
               });
}

void AsyncBuiltinsAssembler::BranchIfNonThenable(TNode<Context> context,
                                                 TNode<Object> value,
                                                 Label* if_non_thenable,
                                                 Label* if_slow) {
  // Fast-check that no promise hooks or debug machinery is active.
  TNode<Uint32T> promiseHookFlags = PromiseHookFlags();
  GotoIf(IsIsolatePromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(
             promiseHookFlags),
         if_slow);
#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
  GotoIf(IsContextPromiseHookEnabled(promiseHookFlags), if_slow);
#endif

  // Non-JSReceiver values (Smis, strings, etc.) are guaranteed non-thenable
  // per ECMA-262 CreateResolvingFunctions step 2.d.
  GotoIf(TaggedIsSmi(value), if_non_thenable);
  TNode<HeapObject> value_heap_object = CAST(value);
  TNode<Map> value_map = LoadMap(value_heap_object);
  GotoIfNot(IsJSReceiverMap(value_map), if_non_thenable);

  // JSReceiver: "then" is an interesting property, so the map's
  // may_have_interesting_properties bit tells us whether the object could
  // have a "then" own property. When the bit is not set, we know:
  // - No named interceptors (cuts off proxies, API objects)
  // - Not a dictionary-mode object
  // - No access checks needed
  // - No "then" in own descriptors
  // We still need to verify the prototype chain doesn't have "then".
  TNode<Uint32T> bit_field3 = LoadMapBitField3(value_map);
  GotoIf(IsSetWord32<Map::Bits3::MayHaveInterestingPropertiesBit>(bit_field3),
         if_slow);
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Object> object_prototype = LoadContextElementNoCell(
      native_context, Context::INITIAL_OBJECT_PROTOTYPE_INDEX);
  GotoIfNot(TaggedEqual(LoadMapPrototype(value_map), object_prototype),
            if_slow);
  Branch(IsPromiseThenProtectorCellInvalid(), if_slow, if_non_thenable);
}

void AsyncBuiltinsAssembler::EnqueueAsyncResumeTask(
    TNode<NativeContext> native_context, TNode<JSGeneratorObject> generator,
    TNode<Object> value, int kind) {
  TNode<HeapObject> task = Allocate(sizeof(AsyncResumeTask));
  StoreMapNoWriteBarrier(task, RootIndex::kAsyncResumeTaskMap);
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  StoreObjectField(
      task, ObjectTraits<Microtask>::kContinuationPreservedEmbedderDataOffset,
      GetContinuationPreservedEmbedderData());
#endif
  using Traits = ObjectTraits<AsyncResumeTask>;
  StoreObjectFieldNoWriteBarrier(task, Traits::kGeneratorOffset, generator);
  StoreObjectFieldNoWriteBarrier(task, Traits::kValueOffset, value);
  StoreObjectFieldNoWriteBarrier(task, Traits::kKindOffset, SmiConstant(kind));
  CallBuiltin(Builtin::kEnqueueMicrotask, native_context, task);
}

TNode<Object> AsyncBuiltinsAssembler::Await(TNode<Context> context,
                                            TNode<JSGeneratorObject> generator,
                                            TNode<JSAny> value,
                                            TNode<JSPromise> outer_promise,
                                            const GetClosures& get_closures) {
  const TNode<NativeContext> native_context = LoadNativeContext(context);

  // Fast path for non-thenable values: skip creating a wrapper promise and
  // directly enqueue a microtask that will call the resolve handler.
  Label if_non_thenable_fast_path(this), if_thenable_slow_path(this),
      done(this);
  TVARIABLE(Object, var_result);
  BranchIfNonThenable(context, value, &if_non_thenable_fast_path,
                      &if_thenable_slow_path);

  BIND(&if_non_thenable_fast_path);
  {
    // Get or allocate resolve and reject handlers.
    auto [on_resolve, on_reject] = get_closures(native_context);
    // Directly enqueue a microtask for the non-thenable value.
    // We pass Undefined as throwaway since no hooks are enabled.
    CallBuiltin(Builtin::kAsyncAwaitNonThenableFastPath, native_context, value,
                on_resolve, UndefinedConstant());
    // Return undefined - the caller will handle the actual return value.
    // This matches what PerformPromiseThen returns for the throwaway promise.
    var_result = UndefinedConstant();
    Goto(&done);
  }

  BIND(&if_thenable_slow_path);

  // We do the `PromiseResolve(%Promise%,value)` avoiding to unnecessarily
  // create wrapper promises. Now if {value} is already a promise with the
  // intrinsics %Promise% constructor as its "constructor", we don't need
  // to allocate the wrapper promise.
  {
    TVARIABLE(JSAny, var_value, value);
    Label if_slow_path(this, Label::kDeferred), if_done(this),
        if_slow_constructor(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(value), &if_slow_path);
    TNode<JSAnyNotSmi> value_object = CAST(value);
    const TNode<Map> value_map = LoadMap(value_object);
    GotoIfNot(IsJSPromiseMap(value_map), &if_slow_path);
    // We can skip the "constructor" lookup on {value} if it's [[Prototype]]
    // is the (initial) Promise.prototype and the @@species protector is
    // intact, as that guards the lookup path for "constructor" on
    // JSPromise instances which have the (initial) Promise.prototype.
    const TNode<Object> promise_prototype = LoadContextElementNoCell(
        native_context, Context::PROMISE_PROTOTYPE_INDEX);
    GotoIfNot(TaggedEqual(LoadMapPrototype(value_map), promise_prototype),
              &if_slow_constructor);
    Branch(IsPromiseSpeciesProtectorCellInvalid(), &if_slow_constructor,
           &if_done);

    // At this point, {value} doesn't have the initial promise prototype or
    // the promise @@species protector was invalidated, but {value} could still
    // have the %Promise% as its "constructor", so we need to check that as
    // well.
    BIND(&if_slow_constructor);
    {
      const TNode<Object> value_constructor = GetProperty(
          context, value, isolate()->factory()->constructor_string());
      const TNode<Object> promise_function = LoadContextElementNoCell(
          native_context, Context::PROMISE_FUNCTION_INDEX);
      Branch(TaggedEqual(value_constructor, promise_function), &if_done,
             &if_slow_path);
    }

    BIND(&if_slow_path);
    {
      // We need to mark the {value} wrapper as having {outer_promise}
      // as its parent, which is why we need to inline a good chunk of
      // logic from the `PromiseResolve` builtin here.
      var_value = NewJSPromise(native_context, outer_promise);
      CallBuiltin(Builtin::kResolvePromise, native_context, var_value.value(),
                  value);
      Goto(&if_done);
    }

    BIND(&if_done);
    value = var_value.value();
  }

  // Get or allocate resolve and reject handlers
  auto [on_resolve, on_reject] = get_closures(native_context);

  // Deal with PromiseHooks and debug support in the runtime. This
  // also allocates the throwaway promise, which is only needed in
  // case of PromiseHooks or debugging.
  TVARIABLE(Object, var_throwaway, UndefinedConstant());
  Label if_instrumentation(this, Label::kDeferred),
      if_instrumentation_done(this);
  TNode<Uint32T> promiseHookFlags = PromiseHookFlags();
  GotoIf(IsIsolatePromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(
             promiseHookFlags),
         &if_instrumentation);
#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
  // This call to NewJSPromise is to keep behaviour parity with what happens
  // in Runtime::kDebugAsyncFunctionSuspended below if native hooks are set.
  // It creates a throwaway promise that will trigger an init event and get
  // passed into Builtin::kPerformPromiseThen below.
  GotoIfNot(IsContextPromiseHookEnabled(promiseHookFlags),
            &if_instrumentation_done);
  var_throwaway = NewJSPromise(context, value);
#endif  // V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
  Goto(&if_instrumentation_done);
  BIND(&if_instrumentation);
  {
    var_throwaway =
        CallRuntime(Runtime::kDebugAsyncFunctionSuspended, native_context,
                    value, outer_promise, on_reject, generator);
    Goto(&if_instrumentation_done);
  }
  BIND(&if_instrumentation_done);

  // Fast-path: if outer promise hooks are disabled, and value is a fulfilled
  // JSPromise, we can skip PerformPromiseThen and directly enqueue a Fulfill
  // reaction job.
  Label if_perform_promise_then(this);
  GotoIfNot(IsUndefined(var_throwaway.value()), &if_perform_promise_then);
  {
    TNode<JSPromise> js_promise = CAST(value);
    TNode<Int32T> promise_flags =
        SmiToInt32(LoadObjectField<Smi>(js_promise, JSPromise::kFlagsOffset));

    TNode<Int32T> status =
        Word32And(promise_flags, Int32Constant(JSPromise::StatusBits::kMask));
    GotoIfNot(Word32Equal(status, Int32Constant(v8::Promise::kFulfilled)),
              &if_perform_promise_then);

    // Mark the promise as handled.
    TNode<Int32T> new_flags =
        Word32Or(promise_flags, Int32Constant(JSPromise::HasHandlerBit::kMask));
    StoreObjectFieldNoWriteBarrier(js_promise, JSPromise::kFlagsOffset,
                                   SmiFromInt32(new_flags));

    // Extract the fulfilled value.
    TNode<Object> fulfilled_value =
        LoadObjectField(js_promise, JSPromise::kReactionsOrResultOffset);

    // Enqueue the reaction job natively.
    CallBuiltin(Builtin::kAsyncAwaitNonThenableFastPath, native_context,
                fulfilled_value, on_resolve, UndefinedConstant());
    var_result = UndefinedConstant();
    Goto(&done);
  }

  BIND(&if_perform_promise_then);
  var_result = CallBuiltin(Builtin::kPerformPromiseThen, native_context, value,
                           on_resolve, on_reject, var_throwaway.value());
  Goto(&done);

  BIND(&done);
  return var_result.value();
}

TNode<Object> AsyncBuiltinsAssembler::AwaitWithReusableClosures(
    TNode<Context> context, TNode<JSAsyncFunctionObject> async_function_object,
    TNode<JSAny> value, TNode<JSPromise> outer_promise) {
  return Await(
      context, async_function_object, value, outer_promise,
      [&](TNode<NativeContext> native_context) {
        // Lazily allocate closures on first await, then reuse them for
        // subsequent awaits.
        TVARIABLE(JSFunction, var_on_resolve);
        TVARIABLE(JSFunction, var_on_reject);
        Label closures_ready(this), allocate_closures(this, Label::kDeferred);

        TNode<HeapObject> maybe_resolve = LoadObjectField<HeapObject>(
            async_function_object,
            JSAsyncFunctionObject::kAwaitResolveClosureOffset);
        GotoIf(IsUndefined(maybe_resolve), &allocate_closures);

        var_on_resolve = CAST(maybe_resolve);
        var_on_reject = LoadObjectField<JSFunction>(
            async_function_object,
            JSAsyncFunctionObject::kAwaitRejectClosureOffset);
        Goto(&closures_ready);

        BIND(&allocate_closures);
        {
          TNode<Context> await_context =
              AllocateAwaitContext(native_context, async_function_object);

          TNode<JSFunction> resolve_closure = AllocateRootFunctionWithContext(
              RootIndex::kAsyncFunctionAwaitResolveClosureSharedFun,
              await_context, native_context);
          TNode<JSFunction> reject_closure = AllocateRootFunctionWithContext(
              RootIndex::kAsyncFunctionAwaitRejectClosureSharedFun,
              await_context, native_context);

          StoreObjectField(async_function_object,
                           JSAsyncFunctionObject::kAwaitResolveClosureOffset,
                           resolve_closure);
          StoreObjectField(async_function_object,
                           JSAsyncFunctionObject::kAwaitRejectClosureOffset,
                           reject_closure);

          var_on_resolve = resolve_closure;
          var_on_reject = reject_closure;
          Goto(&closures_ready);
        }

        BIND(&closures_ready);
        return std::make_pair(var_on_resolve.value(), var_on_reject.value());
      });
}

TNode<JSFunction> AsyncBuiltinsAssembler::CreateUnwrapClosure(
    TNode<NativeContext> native_context, TNode<Boolean> done) {
  const TNode<Context> closure_context =
      AllocateAsyncIteratorValueUnwrapContext(native_context, done);
  return AllocateRootFunctionWithContext(
      RootIndex::kAsyncIteratorValueUnwrapSharedFun, closure_context,
      native_context);
}

TNode<Context> AsyncBuiltinsAssembler::AllocateAsyncIteratorValueUnwrapContext(
    TNode<NativeContext> native_context, TNode<Boolean> done) {
  CSA_DCHECK(this, IsBoolean(done));

  TNode<Context> context = AllocateSyntheticFunctionContext(
      native_context, ValueUnwrapContext::kLength);
  StoreContextElementNoWriteBarrier(context, ValueUnwrapContext::kDoneSlot,
                                    done);
  return context;
}

// https://tc39.es/ecma262/#sec-async-iterator-value-unwrap-functions
TF_BUILTIN(AsyncIteratorValueUnwrap, AsyncBuiltinsAssembler) {
  auto value = Parameter<Object>(Descriptor::kValue);
  auto context = Parameter<Context>(Descriptor::kContext);

  const TNode<Object> done =
      LoadContextElementNoCell(context, ValueUnwrapContext::kDoneSlot);
  CSA_DCHECK(this, IsBoolean(CAST(done)));

  const TNode<Object> unwrapped_value =
      CallBuiltin(Builtin::kCreateIterResultObject, context, value, done);

  Return(unwrapped_value);
}

#include "src/codegen/undef-code-stub-assembler-macros.inc"

}  // namespace internal
}  // namespace v8
