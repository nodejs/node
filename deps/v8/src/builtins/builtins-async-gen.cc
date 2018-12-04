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
  static const int kThrowawayPromiseOffset =
      kWrappedPromiseOffset + JSPromise::kSizeWithEmbedderFields;
  static const int kResolveClosureOffset =
      kThrowawayPromiseOffset + JSPromise::kSizeWithEmbedderFields;
  static const int kRejectClosureOffset =
      kResolveClosureOffset + JSFunction::kSizeWithoutPrototype;
  static const int kTotalSize =
      kRejectClosureOffset + JSFunction::kSizeWithoutPrototype;

  Node* const base = AllocateInNewSpace(kTotalSize);
  Node* const closure_context = base;
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
                                            kPointerSize)));
  Node* const wrapped_value = InnerAllocate(base, kWrappedPromiseOffset);
  {
    // Initialize Promise
    StoreMapNoWriteBarrier(wrapped_value, promise_map);
    InitializeJSObjectFromMap(
        wrapped_value, promise_map,
        IntPtrConstant(JSPromise::kSizeWithEmbedderFields));
    PromiseInit(wrapped_value);
  }

  Node* const throwaway = InnerAllocate(base, kThrowawayPromiseOffset);
  {
    // Initialize throwawayPromise
    StoreMapNoWriteBarrier(throwaway, promise_map);
    InitializeJSObjectFromMap(
        throwaway, promise_map,
        IntPtrConstant(JSPromise::kSizeWithEmbedderFields));
    PromiseInit(throwaway);
  }

  Node* const on_resolve = InnerAllocate(base, kResolveClosureOffset);
  {
    // Initialize resolve handler
    InitializeNativeClosure(closure_context, native_context, on_resolve,
                            on_resolve_context_index);
  }

  Node* const on_reject = InnerAllocate(base, kRejectClosureOffset);
  {
    // Initialize reject handler
    InitializeNativeClosure(closure_context, native_context, on_reject,
                            on_reject_context_index);
  }

  {
    // Add PromiseHooks if needed
    Label next(this);
    GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &next);
    CallRuntime(Runtime::kAwaitPromisesInit, context, wrapped_value,
                outer_promise, throwaway);
    Goto(&next);
    BIND(&next);
  }

  // Perform ! Call(promiseCapability.[[Resolve]], undefined, « promise »).
  CallBuiltin(Builtins::kResolvePromise, context, wrapped_value, value);

  // The Promise will be thrown away and not handled, but it shouldn't trigger
  // unhandled reject events as its work is done
  PromiseSetHasHandler(throwaway);

  Label do_perform_promise_then(this);
  GotoIfNot(IsDebugActive(), &do_perform_promise_then);
  {
    Label common(this);
    GotoIf(TaggedIsSmi(value), &common);
    GotoIfNot(HasInstanceType(value, JS_PROMISE_TYPE), &common);
    {
      // Mark the reject handler callback to be a forwarding edge, rather
      // than a meaningful catch handler
      Node* const key =
          HeapConstant(factory()->promise_forwarding_handler_symbol());
      SetPropertyStrict(CAST(context), CAST(on_reject), CAST(key),
                        TrueConstant());

      GotoIf(IsFalse(is_predicted_as_caught), &common);
      PromiseSetHandledHint(value);
    }

    Goto(&common);
    BIND(&common);
    // Mark the dependency to outer Promise in case the throwaway Promise is
    // found on the Promise stack
    CSA_SLOW_ASSERT(this, HasInstanceType(outer_promise, JS_PROMISE_TYPE));

    Node* const key = HeapConstant(factory()->promise_handled_by_symbol());
    SetPropertyStrict(CAST(context), CAST(throwaway), CAST(key),
                      CAST(outer_promise));
  }

  Goto(&do_perform_promise_then);
  BIND(&do_perform_promise_then);
  return CallBuiltin(Builtins::kPerformPromiseThen, context, wrapped_value,
                     on_resolve, on_reject, throwaway);
}

Node* AsyncBuiltinsAssembler::AwaitOptimized(
    Node* context, Node* generator, Node* value, Node* outer_promise,
    Node* on_resolve_context_index, Node* on_reject_context_index,
    Node* is_predicted_as_caught) {
  Node* const native_context = LoadNativeContext(context);
  Node* const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  CSA_ASSERT(this, IsFunctionWithPrototypeSlotMap(LoadMap(promise_fun)));
  CSA_ASSERT(this, IsConstructor(promise_fun));

  static const int kThrowawayPromiseOffset =
      FixedArray::SizeFor(Context::MIN_CONTEXT_SLOTS);
  static const int kResolveClosureOffset =
      kThrowawayPromiseOffset + JSPromise::kSizeWithEmbedderFields;
  static const int kRejectClosureOffset =
      kResolveClosureOffset + JSFunction::kSizeWithoutPrototype;
  static const int kTotalSize =
      kRejectClosureOffset + JSFunction::kSizeWithoutPrototype;

  // 2. Let promise be ? PromiseResolve(« promise »).
  Node* const promise =
      CallBuiltin(Builtins::kPromiseResolve, context, promise_fun, value);

  Node* const base = AllocateInNewSpace(kTotalSize);
  Node* const closure_context = base;
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

  Node* const promise_map =
      LoadObjectField(promise_fun, JSFunction::kPrototypeOrInitialMapOffset);
  // Assert that the JSPromise map has an instance size is
  // JSPromise::kSizeWithEmbedderFields.
  CSA_ASSERT(this, WordEqual(LoadMapInstanceSizeInWords(promise_map),
                             IntPtrConstant(JSPromise::kSizeWithEmbedderFields /
                                            kPointerSize)));
  Node* const throwaway = InnerAllocate(base, kThrowawayPromiseOffset);
  {
    // Initialize throwawayPromise
    StoreMapNoWriteBarrier(throwaway, promise_map);
    InitializeJSObjectFromMap(
        throwaway, promise_map,
        IntPtrConstant(JSPromise::kSizeWithEmbedderFields));
    PromiseInit(throwaway);
  }

  Node* const on_resolve = InnerAllocate(base, kResolveClosureOffset);
  {
    // Initialize resolve handler
    InitializeNativeClosure(closure_context, native_context, on_resolve,
                            on_resolve_context_index);
  }

  Node* const on_reject = InnerAllocate(base, kRejectClosureOffset);
  {
    // Initialize reject handler
    InitializeNativeClosure(closure_context, native_context, on_reject,
                            on_reject_context_index);
  }

  {
    // Add PromiseHooks if needed
    Label next(this);
    GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &next);
    CallRuntime(Runtime::kAwaitPromisesInit, context, promise, outer_promise,
                throwaway);
    Goto(&next);
    BIND(&next);
  }

  // The Promise will be thrown away and not handled, but it shouldn't trigger
  // unhandled reject events as its work is done
  PromiseSetHasHandler(throwaway);

  Label do_perform_promise_then(this);
  GotoIfNot(IsDebugActive(), &do_perform_promise_then);
  {
    Label common(this);
    GotoIf(TaggedIsSmi(value), &common);
    GotoIfNot(HasInstanceType(value, JS_PROMISE_TYPE), &common);
    {
      // Mark the reject handler callback to be a forwarding edge, rather
      // than a meaningful catch handler
      Node* const key =
          HeapConstant(factory()->promise_forwarding_handler_symbol());
      SetPropertyStrict(CAST(context), CAST(on_reject), CAST(key),
                        TrueConstant());

      GotoIf(IsFalse(is_predicted_as_caught), &common);
      PromiseSetHandledHint(value);
    }

    Goto(&common);
    BIND(&common);
    // Mark the dependency to outer Promise in case the throwaway Promise is
    // found on the Promise stack
    CSA_SLOW_ASSERT(this, HasInstanceType(outer_promise, JS_PROMISE_TYPE));

    Node* const key = HeapConstant(factory()->promise_handled_by_symbol());
    SetPropertyStrict(CAST(context), CAST(throwaway), CAST(key),
                      CAST(outer_promise));
  }

  Goto(&do_perform_promise_then);
  BIND(&do_perform_promise_then);
  return CallBuiltin(Builtins::kPerformPromiseThen, native_context, promise,
                     on_resolve, on_reject, throwaway);
}

Node* AsyncBuiltinsAssembler::Await(Node* context, Node* generator, Node* value,
                                    Node* outer_promise,
                                    Node* on_resolve_context_index,
                                    Node* on_reject_context_index,
                                    Node* is_predicted_as_caught) {
  VARIABLE(result, MachineRepresentation::kTagged);
  Label if_old(this), if_new(this), done(this);

  STATIC_ASSERT(sizeof(FLAG_harmony_await_optimization) == 1);

  TNode<Word32T> flag_value = UncheckedCast<Word32T>(Load(
      MachineType::Uint8(),
      ExternalConstant(
          ExternalReference::address_of_harmony_await_optimization_flag())));

  Branch(Word32Equal(flag_value, Int32Constant(0)), &if_old, &if_new);

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
  Node* const function_map = LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
  // Ensure that we don't have to initialize prototype_or_initial_map field of
  // JSFunction.
  CSA_ASSERT(this, WordEqual(LoadMapInstanceSizeInWords(function_map),
                             IntPtrConstant(JSFunction::kSizeWithoutPrototype /
                                            kPointerSize)));
  STATIC_ASSERT(JSFunction::kSizeWithoutPrototype == 7 * kPointerSize);
  StoreMapNoWriteBarrier(function, function_map);
  StoreObjectFieldRoot(function, JSObject::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(function, JSObject::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(function, JSFunction::kFeedbackCellOffset,
                       RootIndex::kManyClosuresCell);

  Node* shared_info = LoadContextElement(native_context, context_index);
  CSA_ASSERT(this, IsSharedFunctionInfo(shared_info));
  StoreObjectFieldNoWriteBarrier(
      function, JSFunction::kSharedFunctionInfoOffset, shared_info);
  StoreObjectFieldNoWriteBarrier(function, JSFunction::kContextOffset, context);

  Node* const code = GetSharedFunctionInfoCode(shared_info);
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
