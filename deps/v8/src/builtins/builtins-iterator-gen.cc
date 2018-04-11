// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-iterator-gen.h"

#include "src/factory-inl.h"

namespace v8 {
namespace internal {

using compiler::Node;

IteratorRecord IteratorBuiltinsAssembler::GetIterator(Node* context,
                                                      Node* object,
                                                      Label* if_exception,
                                                      Variable* exception) {
  Node* method = GetProperty(context, object, factory()->iterator_symbol());
  GotoIfException(method, if_exception, exception);

  Callable callable = CodeFactory::Call(isolate());
  Node* iterator = CallJS(callable, context, method, object);
  GotoIfException(iterator, if_exception, exception);

  Label get_next(this), if_notobject(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(iterator), &if_notobject);
  Branch(IsJSReceiver(iterator), &get_next, &if_notobject);

  BIND(&if_notobject);
  {
    Node* ret =
        CallRuntime(Runtime::kThrowTypeError, context,
                    SmiConstant(MessageTemplate::kNotAnIterator), iterator);
    GotoIfException(ret, if_exception, exception);
    Unreachable();
  }

  BIND(&get_next);
  Node* const next = GetProperty(context, iterator, factory()->next_string());
  GotoIfException(next, if_exception, exception);

  return IteratorRecord{TNode<JSReceiver>::UncheckedCast(iterator),
                        TNode<Object>::UncheckedCast(next)};
}

Node* IteratorBuiltinsAssembler::IteratorStep(
    Node* context, const IteratorRecord& iterator, Label* if_done,
    Node* fast_iterator_result_map, Label* if_exception, Variable* exception) {
  DCHECK_NOT_NULL(if_done);
  // 1. a. Let result be ? Invoke(iterator, "next", « »).
  Callable callable = CodeFactory::Call(isolate());
  Node* result = CallJS(callable, context, iterator.next, iterator.object);
  GotoIfException(result, if_exception, exception);

  // 3. If Type(result) is not Object, throw a TypeError exception.
  Label if_notobject(this, Label::kDeferred), return_result(this);
  GotoIf(TaggedIsSmi(result), &if_notobject);
  Node* result_map = LoadMap(result);

  if (fast_iterator_result_map != nullptr) {
    // Fast iterator result case:
    Label if_generic(this);

    // 4. Return result.
    GotoIfNot(WordEqual(result_map, fast_iterator_result_map), &if_generic);

    // IteratorComplete
    // 2. Return ToBoolean(? Get(iterResult, "done")).
    Node* done = LoadObjectField(result, JSIteratorResult::kDoneOffset);
    BranchIfToBooleanIsTrue(done, if_done, &return_result);

    BIND(&if_generic);
  }

  // Generic iterator result case:
  {
    // 3. If Type(result) is not Object, throw a TypeError exception.
    GotoIfNot(IsJSReceiverMap(result_map), &if_notobject);

    // IteratorComplete
    // 2. Return ToBoolean(? Get(iterResult, "done")).
    Node* done = GetProperty(context, result, factory()->done_string());
    GotoIfException(done, if_exception, exception);
    BranchIfToBooleanIsTrue(done, if_done, &return_result);
  }

  BIND(&if_notobject);
  {
    Node* ret =
        CallRuntime(Runtime::kThrowIteratorResultNotAnObject, context, result);
    GotoIfException(ret, if_exception, exception);
    Unreachable();
  }

  BIND(&return_result);
  return result;
}

Node* IteratorBuiltinsAssembler::IteratorValue(Node* context, Node* result,
                                               Node* fast_iterator_result_map,
                                               Label* if_exception,
                                               Variable* exception) {
  CSA_ASSERT(this, IsJSReceiver(result));

  Label exit(this);
  VARIABLE(var_value, MachineRepresentation::kTagged);
  if (fast_iterator_result_map != nullptr) {
    // Fast iterator result case:
    Label if_generic(this);
    Node* map = LoadMap(result);
    GotoIfNot(WordEqual(map, fast_iterator_result_map), &if_generic);
    var_value.Bind(LoadObjectField(result, JSIteratorResult::kValueOffset));
    Goto(&exit);

    BIND(&if_generic);
  }

  // Generic iterator result case:
  {
    Node* value = GetProperty(context, result, factory()->value_string());
    GotoIfException(value, if_exception, exception);
    var_value.Bind(value);
    Goto(&exit);
  }

  BIND(&exit);
  return var_value.value();
}

void IteratorBuiltinsAssembler::IteratorCloseOnException(
    Node* context, const IteratorRecord& iterator, Label* if_exception,
    Variable* exception) {
  // Perform ES #sec-iteratorclose when an exception occurs. This simpler
  // algorithm does not include redundant steps which are never reachable from
  // the spec IteratorClose algorithm.
  DCHECK_NOT_NULL(if_exception);
  DCHECK_NOT_NULL(exception);
  CSA_ASSERT(this, IsNotTheHole(exception->value()));
  CSA_ASSERT(this, IsJSReceiver(iterator.object));

  // Let return be ? GetMethod(iterator, "return").
  Node* method =
      GetProperty(context, iterator.object, factory()->return_string());
  GotoIfException(method, if_exception, exception);

  // If return is undefined, return Completion(completion).
  GotoIf(Word32Or(IsUndefined(method), IsNull(method)), if_exception);

  {
    // Let innerResult be Call(return, iterator, « »).
    // If an exception occurs, the original exception remains bound
    Node* inner_result =
        CallJS(CodeFactory::Call(isolate()), context, method, iterator.object);
    GotoIfException(inner_result, if_exception, nullptr);

    // (If completion.[[Type]] is throw) return Completion(completion).
    Goto(if_exception);
  }
}

void IteratorBuiltinsAssembler::IteratorCloseOnException(
    Node* context, const IteratorRecord& iterator, Variable* exception) {
  Label rethrow(this, Label::kDeferred);
  IteratorCloseOnException(context, iterator, &rethrow, exception);

  BIND(&rethrow);
  CallRuntime(Runtime::kReThrow, context, exception->value());
  Unreachable();
}

}  // namespace internal
}  // namespace v8
