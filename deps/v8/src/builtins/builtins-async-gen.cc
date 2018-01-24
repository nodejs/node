// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-async-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/factory-inl.h"
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

Node* AsyncBuiltinsAssembler::Await(
    Node* context, Node* generator, Node* value, Node* outer_promise,
    int context_length, const ContextInitializer& init_closure_context,
    Node* on_resolve_context_index, Node* on_reject_context_index,
    Node* is_predicted_as_caught) {
  DCHECK_GE(context_length, Context::MIN_CONTEXT_SLOTS);

  Node* const native_context = LoadNativeContext(context);

  static const int kWrappedPromiseOffset = FixedArray::SizeFor(context_length);
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
    // Initialize closure context
    InitializeFunctionContext(native_context, closure_context, context_length);
    init_closure_context(closure_context);
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
    GotoIfNot(IsPromiseHookEnabledOrDebugIsActive(), &next);
    CallRuntime(Runtime::kPromiseHookInit, context, wrapped_value,
                outer_promise);
    CallRuntime(Runtime::kPromiseHookInit, context, throwaway, wrapped_value);
    Goto(&next);
    BIND(&next);
  }

  // Perform ! Call(promiseCapability.[[Resolve]], undefined, « promise »).
  CallBuiltin(Builtins::kResolveNativePromise, context, wrapped_value, value);

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
      CallRuntime(Runtime::kSetProperty, context, on_reject, key,
                  TrueConstant(), SmiConstant(LanguageMode::kStrict));

      GotoIf(IsFalse(is_predicted_as_caught), &common);
      PromiseSetHandledHint(value);
    }

    Goto(&common);
    BIND(&common);
    // Mark the dependency to outer Promise in case the throwaway Promise is
    // found on the Promise stack
    CSA_SLOW_ASSERT(this, HasInstanceType(outer_promise, JS_PROMISE_TYPE));

    Node* const key = HeapConstant(factory()->promise_handled_by_symbol());
    CallRuntime(Runtime::kSetProperty, context, throwaway, key, outer_promise,
                SmiConstant(LanguageMode::kStrict));
  }

  Goto(&do_perform_promise_then);
  BIND(&do_perform_promise_then);

  CallBuiltin(Builtins::kPerformNativePromiseThen, context, wrapped_value,
              on_resolve, on_reject, throwaway);

  return wrapped_value;
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
  StoreMapNoWriteBarrier(function, function_map);
  StoreObjectFieldRoot(function, JSObject::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(function, JSObject::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(function, JSFunction::kFeedbackVectorOffset,
                       Heap::kUndefinedCellRootIndex);

  Node* shared_info = LoadContextElement(native_context, context_index);
  CSA_ASSERT(this, IsSharedFunctionInfo(shared_info));
  StoreObjectFieldNoWriteBarrier(
      function, JSFunction::kSharedFunctionInfoOffset, shared_info);
  StoreObjectFieldNoWriteBarrier(function, JSFunction::kContextOffset, context);

  Node* const code =
      LoadObjectField(shared_info, SharedFunctionInfo::kCodeOffset);
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
