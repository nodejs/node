// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ITERATOR_GEN_H_
#define V8_BUILTINS_BUILTINS_ITERATOR_GEN_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

using compiler::Node;

class IteratorBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit IteratorBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  using IteratorRecord = TorqueStructIteratorRecord;

  // Returns object[Symbol.iterator].
  TNode<Object> GetIteratorMethod(Node* context, Node* object);

  // https://tc39.github.io/ecma262/#sec-getiterator --- never used for
  // @@asyncIterator.
  IteratorRecord GetIterator(Node* context, Node* object,
                             Label* if_exception = nullptr,
                             Variable* exception = nullptr);
  IteratorRecord GetIterator(Node* context, Node* object, Node* method,
                             Label* if_exception = nullptr,
                             Variable* exception = nullptr);

  // https://tc39.github.io/ecma262/#sec-iteratorstep
  // If the iterator is done, goto {if_done}, otherwise returns an iterator
  // result.
  // `fast_iterator_result_map` refers to the map for the JSIteratorResult
  // object, loaded from the native context.
  TNode<JSReceiver> IteratorStep(
      TNode<Context> context, const IteratorRecord& iterator, Label* if_done,
      base::Optional<TNode<Map>> fast_iterator_result_map = base::nullopt,
      Label* if_exception = nullptr, Variable* exception = nullptr);

  TNode<JSReceiver> IteratorStep(
      TNode<Context> context, const IteratorRecord& iterator,
      base::Optional<TNode<Map>> fast_iterator_result_map, Label* if_done) {
    return IteratorStep(context, iterator, if_done, fast_iterator_result_map);
  }

  // https://tc39.github.io/ecma262/#sec-iteratorvalue
  // Return the `value` field from an iterator.
  // `fast_iterator_result_map` refers to the map for the JSIteratorResult
  // object, loaded from the native context.
  TNode<Object> IteratorValue(
      TNode<Context> context, TNode<JSReceiver> result,
      base::Optional<TNode<Map>> fast_iterator_result_map = base::nullopt,
      Label* if_exception = nullptr, Variable* exception = nullptr);

  // https://tc39.github.io/ecma262/#sec-iteratorclose
  void IteratorCloseOnException(Node* context, const IteratorRecord& iterator,
                                Label* if_exception, Variable* exception);
  void IteratorCloseOnException(Node* context, const IteratorRecord& iterator,
                                TNode<Object> exception);

  // #sec-iterabletolist
  // Build a JSArray by iterating over {iterable} using {iterator_fn},
  // following the ECMAscript operation with the same name.
  TNode<JSArray> IterableToList(TNode<Context> context, TNode<Object> iterable,
                                TNode<Object> iterator_fn);

  // Currently at https://tc39.github.io/proposal-intl-list-format/
  // #sec-createstringlistfromiterable
  TNode<JSArray> StringListFromIterable(TNode<Context> context,
                                        TNode<Object> iterable);

  void FastIterableToList(TNode<Context> context, TNode<Object> iterable,
                          TVariable<Object>* var_result, Label* slow);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ITERATOR_GEN_H_
