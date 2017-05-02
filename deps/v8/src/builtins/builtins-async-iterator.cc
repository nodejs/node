// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-async.h"
#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/frames-inl.h"

namespace v8 {
namespace internal {

namespace {

// Describe fields of Context associated with the AsyncIterator unwrap closure.
class ValueUnwrapContext {
 public:
  enum Fields { kDoneSlot = Context::MIN_CONTEXT_SLOTS, kLength };
};

class AsyncFromSyncBuiltinsAssembler : public AsyncBuiltinsAssembler {
 public:
  explicit AsyncFromSyncBuiltinsAssembler(CodeAssemblerState* state)
      : AsyncBuiltinsAssembler(state) {}

  void ThrowIfNotAsyncFromSyncIterator(Node* const context, Node* const object,
                                       Label* if_exception,
                                       Variable* var_exception,
                                       const char* method_name);

  typedef std::function<void(Node* const context, Node* const promise,
                             Label* if_exception)>
      UndefinedMethodHandler;
  void Generate_AsyncFromSyncIteratorMethod(
      Node* const context, Node* const iterator, Node* const sent_value,
      Handle<Name> method_name, UndefinedMethodHandler&& if_method_undefined,
      const char* operation_name,
      Label::Type reject_label_type = Label::kDeferred,
      Node* const initial_exception_value = nullptr);

  Node* AllocateAsyncIteratorValueUnwrapContext(Node* native_context,
                                                Node* done);

  // Load "value" and "done" from an iterator result object. If an exception
  // is thrown at any point, jumps to te `if_exception` label with exception
  // stored in `var_exception`.
  //
  // Returns a Pair of Nodes, whose first element is the value of the "value"
  // property, and whose second element is the value of the "done" property,
  // converted to a Boolean if needed.
  std::pair<Node*, Node*> LoadIteratorResult(Node* const context,
                                             Node* const native_context,
                                             Node* const iter_result,
                                             Label* if_exception,
                                             Variable* var_exception);

  Node* CreateUnwrapClosure(Node* const native_context, Node* const done);
};

void AsyncFromSyncBuiltinsAssembler::ThrowIfNotAsyncFromSyncIterator(
    Node* const context, Node* const object, Label* if_exception,
    Variable* var_exception, const char* method_name) {
  Label if_receiverisincompatible(this, Label::kDeferred), done(this);

  GotoIf(TaggedIsSmi(object), &if_receiverisincompatible);
  Branch(HasInstanceType(object, JS_ASYNC_FROM_SYNC_ITERATOR_TYPE), &done,
         &if_receiverisincompatible);

  Bind(&if_receiverisincompatible);
  {
    // If Type(O) is not Object, or if O does not have a [[SyncIterator]]
    // internal slot, then

    // Let badIteratorError be a new TypeError exception.
    Node* const error =
        MakeTypeError(MessageTemplate::kIncompatibleMethodReceiver, context,
                      CStringConstant(method_name), object);

    // Perform ! Call(promiseCapability.[[Reject]], undefined,
    //                « badIteratorError »).
    var_exception->Bind(error);
    Goto(if_exception);
  }

  Bind(&done);
}

void AsyncFromSyncBuiltinsAssembler::Generate_AsyncFromSyncIteratorMethod(
    Node* const context, Node* const iterator, Node* const sent_value,
    Handle<Name> method_name, UndefinedMethodHandler&& if_method_undefined,
    const char* operation_name, Label::Type reject_label_type,
    Node* const initial_exception_value) {
  Node* const native_context = LoadNativeContext(context);
  Node* const promise = AllocateAndInitJSPromise(context);

  Variable var_exception(this, MachineRepresentation::kTagged,
                         initial_exception_value == nullptr
                             ? UndefinedConstant()
                             : initial_exception_value);
  Label reject_promise(this, reject_label_type);

  ThrowIfNotAsyncFromSyncIterator(context, iterator, &reject_promise,
                                  &var_exception, operation_name);

  Node* const sync_iterator =
      LoadObjectField(iterator, JSAsyncFromSyncIterator::kSyncIteratorOffset);

  Node* const method = GetProperty(context, sync_iterator, method_name);

  if (if_method_undefined) {
    Label if_isnotundefined(this);

    GotoIfNot(IsUndefined(method), &if_isnotundefined);
    if_method_undefined(native_context, promise, &reject_promise);

    Bind(&if_isnotundefined);
  }

  Node* const iter_result = CallJS(CodeFactory::Call(isolate()), context,
                                   method, sync_iterator, sent_value);
  GotoIfException(iter_result, &reject_promise, &var_exception);

  Node* value;
  Node* done;
  std::tie(value, done) = LoadIteratorResult(
      context, native_context, iter_result, &reject_promise, &var_exception);
  Node* const wrapper = AllocateAndInitJSPromise(context);

  // Perform ! Call(valueWrapperCapability.[[Resolve]], undefined, «
  // throwValue »).
  InternalResolvePromise(context, wrapper, value);

  // Let onFulfilled be a new built-in function object as defined in
  // Async Iterator Value Unwrap Functions.
  // Set onFulfilled.[[Done]] to throwDone.
  Node* const on_fulfilled = CreateUnwrapClosure(native_context, done);

  // Perform ! PerformPromiseThen(valueWrapperCapability.[[Promise]],
  //     onFulfilled, undefined, promiseCapability).
  Node* const undefined = UndefinedConstant();
  InternalPerformPromiseThen(context, wrapper, on_fulfilled, undefined, promise,
                             undefined, undefined);
  Return(promise);

  Bind(&reject_promise);
  {
    Node* const exception = var_exception.value();
    InternalPromiseReject(context, promise, exception, TrueConstant());

    Return(promise);
  }
}

std::pair<Node*, Node*> AsyncFromSyncBuiltinsAssembler::LoadIteratorResult(
    Node* const context, Node* const native_context, Node* const iter_result,
    Label* if_exception, Variable* var_exception) {
  Label if_fastpath(this), if_slowpath(this), merge(this), to_boolean(this),
      done(this), if_notanobject(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(iter_result), &if_notanobject);

  Node* const iter_result_map = LoadMap(iter_result);
  GotoIfNot(IsJSReceiverMap(iter_result_map), &if_notanobject);

  Node* const fast_iter_result_map =
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);

  Variable var_value(this, MachineRepresentation::kTagged);
  Variable var_done(this, MachineRepresentation::kTagged);
  Branch(WordEqual(iter_result_map, fast_iter_result_map), &if_fastpath,
         &if_slowpath);

  Bind(&if_fastpath);
  {
    var_value.Bind(
        LoadObjectField(iter_result, JSIteratorResult::kValueOffset));
    var_done.Bind(LoadObjectField(iter_result, JSIteratorResult::kDoneOffset));
    Goto(&merge);
  }

  Bind(&if_slowpath);
  {
    // Let nextValue be IteratorValue(nextResult).
    // IfAbruptRejectPromise(nextValue, promiseCapability).
    Node* const value =
        GetProperty(context, iter_result, factory()->value_string());
    GotoIfException(value, if_exception, var_exception);

    // Let nextDone be IteratorComplete(nextResult).
    // IfAbruptRejectPromise(nextDone, promiseCapability).
    Node* const done =
        GetProperty(context, iter_result, factory()->done_string());
    GotoIfException(done, if_exception, var_exception);

    var_value.Bind(value);
    var_done.Bind(done);
    Goto(&merge);
  }

  Bind(&if_notanobject);
  {
    // Sync iterator result is not an object --- Produce a TypeError and jump
    // to the `if_exception` path.
    Node* const error = MakeTypeError(
        MessageTemplate::kIteratorResultNotAnObject, context, iter_result);
    var_exception->Bind(error);
    Goto(if_exception);
  }

  Bind(&merge);
  // Ensure `iterResult.done` is a Boolean.
  GotoIf(TaggedIsSmi(var_done.value()), &to_boolean);
  Branch(IsBoolean(var_done.value()), &done, &to_boolean);

  Bind(&to_boolean);
  {
    Node* const result =
        CallStub(CodeFactory::ToBoolean(isolate()), context, var_done.value());
    var_done.Bind(result);
    Goto(&done);
  }

  Bind(&done);
  return std::make_pair(var_value.value(), var_done.value());
}

Node* AsyncFromSyncBuiltinsAssembler::CreateUnwrapClosure(Node* native_context,
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

Node* AsyncFromSyncBuiltinsAssembler::AllocateAsyncIteratorValueUnwrapContext(
    Node* native_context, Node* done) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  CSA_ASSERT(this, IsBoolean(done));

  Node* const context =
      CreatePromiseContext(native_context, ValueUnwrapContext::kLength);
  StoreContextElementNoWriteBarrier(context, ValueUnwrapContext::kDoneSlot,
                                    done);
  return context;
}
}  // namespace

// https://tc39.github.io/proposal-async-iteration/
// Section #sec-%asyncfromsynciteratorprototype%.next
TF_BUILTIN(AsyncFromSyncIteratorPrototypeNext, AsyncFromSyncBuiltinsAssembler) {
  Node* const iterator = Parameter(0);
  Node* const value = Parameter(1);
  Node* const context = Parameter(4);

  Generate_AsyncFromSyncIteratorMethod(
      context, iterator, value, factory()->next_string(),
      UndefinedMethodHandler(), "[Async-from-Sync Iterator].prototype.next");
}

// https://tc39.github.io/proposal-async-iteration/
// Section #sec-%asyncfromsynciteratorprototype%.return
TF_BUILTIN(AsyncFromSyncIteratorPrototypeReturn,
           AsyncFromSyncBuiltinsAssembler) {
  Node* const iterator = Parameter(0);
  Node* const value = Parameter(1);
  Node* const context = Parameter(4);

  auto if_return_undefined = [=](Node* const native_context,
                                 Node* const promise, Label* if_exception) {
    // If return is undefined, then
    // Let iterResult be ! CreateIterResultObject(value, true)
    Node* const iter_result =
        CallStub(CodeFactory::CreateIterResultObject(isolate()), context, value,
                 TrueConstant());

    // Perform ! Call(promiseCapability.[[Resolve]], undefined, « iterResult »).
    // IfAbruptRejectPromise(nextDone, promiseCapability).
    // Return promiseCapability.[[Promise]].
    PromiseFulfill(context, promise, iter_result, v8::Promise::kFulfilled);
    Return(promise);
  };

  Generate_AsyncFromSyncIteratorMethod(
      context, iterator, value, factory()->return_string(), if_return_undefined,
      "[Async-from-Sync Iterator].prototype.return");
}

// https://tc39.github.io/proposal-async-iteration/
// Section #sec-%asyncfromsynciteratorprototype%.throw
TF_BUILTIN(AsyncFromSyncIteratorPrototypeThrow,
           AsyncFromSyncBuiltinsAssembler) {
  Node* const iterator = Parameter(0);
  Node* const reason = Parameter(1);
  Node* const context = Parameter(4);

  auto if_throw_undefined = [=](Node* const native_context, Node* const promise,
                                Label* if_exception) { Goto(if_exception); };

  Generate_AsyncFromSyncIteratorMethod(
      context, iterator, reason, factory()->throw_string(), if_throw_undefined,
      "[Async-from-Sync Iterator].prototype.throw", Label::kNonDeferred,
      reason);
}

TF_BUILTIN(AsyncIteratorValueUnwrap, AsyncFromSyncBuiltinsAssembler) {
  Node* const value = Parameter(1);
  Node* const context = Parameter(4);

  Node* const done = LoadContextElement(context, ValueUnwrapContext::kDoneSlot);
  CSA_ASSERT(this, IsBoolean(done));

  Node* const unwrapped_value = CallStub(
      CodeFactory::CreateIterResultObject(isolate()), context, value, done);

  Return(unwrapped_value);
}

}  // namespace internal
}  // namespace v8
