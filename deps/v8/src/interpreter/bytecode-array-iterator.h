// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_ITERATOR_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_ITERATOR_H_

#include "src/interpreter/bytecode-array-accessor.h"

namespace v8 {
namespace internal {
namespace interpreter {

class V8_EXPORT_PRIVATE BytecodeArrayIterator final
    : public BytecodeArrayAccessor {
 public:
  explicit BytecodeArrayIterator(Handle<BytecodeArray> bytecode_array);

  void Advance();
  bool done() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(BytecodeArrayIterator);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_ITERATOR_H_
