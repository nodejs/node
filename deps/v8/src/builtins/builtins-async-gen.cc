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

void AsyncBuiltinsAssembler::Await(Node* context, Node* generator, Node* value,
                                   Node* outer_promise,
                                   Builtins::Name fulfill_builtin,
                                   Builtins::Name reject_builtin,
                                   Node* is_predicted_as_caught) {
  CSA_SLOW_ASSERT(this, Word32Or(IsJSAsyncGeneratorObject(generator),
                                 IsJSGeneratorObject(generator)));
  CSA_SLOW_ASSERT(this, IsJSPromise(outer_promise));
  CSA_SLOW_ASSERT(this, IsBoolean(is_predicted_as_caught));

  Node* const native_context = LoadNativeContext(context);

  // TODO(bmeurer): This could be optimized and folded into a single allocation.
  Node* const promise = AllocateAndInitJSPromise(native_context);
  Node* const promise_reactions =
      LoadObjectField(promise, JSPromise::kReactionsOrResultOffset);
  Node* const fulfill_handler =
      HeapConstant(Builtins::CallableFor(isolate(), fulfill_builtin).code());
  Node* const reject_handler =
      HeapConstant(Builtins::CallableFor(isolate(), reject_builtin).code());
  Node* const reaction = AllocatePromiseReaction(
      promise_reactions, generator, fulfill_handler, reject_handler);
  StoreObjectField(promise, JSPromise::kReactionsOrResultOffset, reaction);
  PromiseSetHasHandler(promise);

  // Perform ! Call(promiseCapability.[[Resolve]], undefined, « value »).
  CallBuiltin(Builtins::kResolvePromise, native_context, promise, value);

  // When debugging, we need to link from the {generator} to the
  // {outer_promise} of the async function/generator.
  Label done(this);
  GotoIfNot(IsDebugActive(), &done);
  CallRuntime(Runtime::kSetProperty, native_context, generator,
              LoadRoot(Heap::kgenerator_outer_promise_symbolRootIndex),
              outer_promise, SmiConstant(LanguageMode::kStrict));
  GotoIf(IsFalse(is_predicted_as_caught), &done);
  GotoIf(TaggedIsSmi(value), &done);
  GotoIfNot(IsJSPromise(value), &done);
  PromiseSetHandledHint(value);
  Goto(&done);
  BIND(&done);
}

void AsyncBuiltinsAssembler::Await(Node* context, Node* generator, Node* value,
                                   Node* outer_promise,
                                   Builtins::Name fulfill_builtin,
                                   Builtins::Name reject_builtin,
                                   bool is_predicted_as_caught) {
  return Await(context, generator, value, outer_promise, fulfill_builtin,
               reject_builtin, BooleanConstant(is_predicted_as_caught));
}

namespace {
// Describe fields of Context associated with the AsyncIterator unwrap closure.
class ValueUnwrapContext {
 public:
  enum Fields { kDoneSlot = Context::MIN_CONTEXT_SLOTS, kLength };
};

}  // namespace

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
