// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-async-gen.h"
#include "src/builtins/builtins-utils-gen.h"

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
    const NodeGenerator1& create_closure_context, int on_resolve_context_index,
    int on_reject_context_index, bool is_predicted_as_caught) {
  // Let promiseCapability be ! NewPromiseCapability(%Promise%).
  Node* const wrapped_value = AllocateAndInitJSPromise(context);

  // Perform ! Call(promiseCapability.[[Resolve]], undefined, « promise »).
  CallBuiltin(Builtins::kResolveNativePromise, context, wrapped_value, value);

  Node* const native_context = LoadNativeContext(context);

  Node* const closure_context = create_closure_context(native_context);
  Node* const map = LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);

  // Load and allocate on_resolve closure
  Node* const on_resolve_shared_fun =
      LoadContextElement(native_context, on_resolve_context_index);
  CSA_SLOW_ASSERT(
      this, HasInstanceType(on_resolve_shared_fun, SHARED_FUNCTION_INFO_TYPE));
  Node* const on_resolve = AllocateFunctionWithMapAndContext(
      map, on_resolve_shared_fun, closure_context);

  // Load and allocate on_reject closure
  Node* const on_reject_shared_fun =
      LoadContextElement(native_context, on_reject_context_index);
  CSA_SLOW_ASSERT(
      this, HasInstanceType(on_reject_shared_fun, SHARED_FUNCTION_INFO_TYPE));
  Node* const on_reject = AllocateFunctionWithMapAndContext(
      map, on_reject_shared_fun, closure_context);

  Node* const throwaway_promise =
      AllocateAndInitJSPromise(context, wrapped_value);

  // The Promise will be thrown away and not handled, but it shouldn't trigger
  // unhandled reject events as its work is done
  PromiseSetHasHandler(throwaway_promise);

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
                  TrueConstant(), SmiConstant(STRICT));

      if (is_predicted_as_caught) PromiseSetHandledHint(value);
    }

    Goto(&common);
    BIND(&common);
    // Mark the dependency to outer Promise in case the throwaway Promise is
    // found on the Promise stack
    CSA_SLOW_ASSERT(this, HasInstanceType(outer_promise, JS_PROMISE_TYPE));

    Node* const key = HeapConstant(factory()->promise_handled_by_symbol());
    CallRuntime(Runtime::kSetProperty, context, throwaway_promise, key,
                outer_promise, SmiConstant(STRICT));
  }

  Goto(&do_perform_promise_then);
  BIND(&do_perform_promise_then);
  CallBuiltin(Builtins::kPerformNativePromiseThen, context, wrapped_value,
              on_resolve, on_reject, throwaway_promise);

  return wrapped_value;
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

  Node* const unwrapped_value = CallStub(
      CodeFactory::CreateIterResultObject(isolate()), context, value, done);

  Return(unwrapped_value);
}

}  // namespace internal
}  // namespace v8
