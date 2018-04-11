// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ITERATOR_GEN_H_
#define V8_BUILTINS_BUILTINS_ITERATOR_GEN_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

using compiler::Node;

class IteratorBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit IteratorBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  // Returns object[Symbol.iterator].
  Node* GetIteratorMethod(Node* context, Node* object);

  // https://tc39.github.io/ecma262/#sec-getiterator --- never used for
  // @@asyncIterator.
  IteratorRecord GetIterator(Node* context, Node* object,
                             Label* if_exception = nullptr,
                             Variable* exception = nullptr);
  IteratorRecord GetIterator(Node* context, Node* object, Node* method,
                             Label* if_exception = nullptr,
                             Variable* exception = nullptr);

  // https://tc39.github.io/ecma262/#sec-iteratorstep
  // Returns `false` if the iterator is done, otherwise returns an
  // iterator result.
  // `fast_iterator_result_map` refers to the map for the JSIteratorResult
  // object, loaded from the native context.
  Node* IteratorStep(Node* context, const IteratorRecord& iterator,
                     Label* if_done, Node* fast_iterator_result_map = nullptr,
                     Label* if_exception = nullptr,
                     Variable* exception = nullptr);

  // https://tc39.github.io/ecma262/#sec-iteratorvalue
  // Return the `value` field from an iterator.
  // `fast_iterator_result_map` refers to the map for the JSIteratorResult
  // object, loaded from the native context.
  Node* IteratorValue(Node* context, Node* result,
                      Node* fast_iterator_result_map = nullptr,
                      Label* if_exception = nullptr,
                      Variable* exception = nullptr);

  // https://tc39.github.io/ecma262/#sec-iteratorclose
  void IteratorCloseOnException(Node* context, const IteratorRecord& iterator,
                                Label* if_exception, Variable* exception);
  void IteratorCloseOnException(Node* context, const IteratorRecord& iterator,
                                Variable* exception);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ITERATOR_GEN_H_
