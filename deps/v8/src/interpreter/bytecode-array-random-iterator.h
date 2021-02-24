// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_RANDOM_ITERATOR_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_RANDOM_ITERATOR_H_

#include <memory>

#include "src/interpreter/bytecode-array-accessor.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace interpreter {

class V8_EXPORT_PRIVATE BytecodeArrayRandomIterator final
    : public BytecodeArrayAccessor {
 public:
  BytecodeArrayRandomIterator(Handle<BytecodeArray> bytecode_array, Zone* zone);

  BytecodeArrayRandomIterator(const BytecodeArrayRandomIterator&) = delete;
  BytecodeArrayRandomIterator& operator=(const BytecodeArrayRandomIterator&) =
      delete;

  BytecodeArrayRandomIterator& operator++() {
    ++current_index_;
    UpdateOffsetFromIndex();
    return *this;
  }
  BytecodeArrayRandomIterator& operator--() {
    --current_index_;
    UpdateOffsetFromIndex();
    return *this;
  }

  BytecodeArrayRandomIterator& operator+=(int offset) {
    current_index_ += offset;
    UpdateOffsetFromIndex();
    return *this;
  }

  BytecodeArrayRandomIterator& operator-=(int offset) {
    current_index_ -= offset;
    UpdateOffsetFromIndex();
    return *this;
  }

  int current_index() const { return current_index_; }

  size_t size() const { return offsets_.size(); }

  void GoToIndex(int index) {
    current_index_ = index;
    UpdateOffsetFromIndex();
  }
  void GoToStart() {
    current_index_ = 0;
    UpdateOffsetFromIndex();
  }
  void GoToEnd() {
    DCHECK_LT(offsets_.size() - 1, static_cast<size_t>(INT_MAX));
    current_index_ = static_cast<int>(offsets_.size() - 1);
    UpdateOffsetFromIndex();
  }

  bool IsValid() const;

 private:
  ZoneVector<int> offsets_;
  int current_index_;

  void Initialize();
  void UpdateOffsetFromIndex();
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_RANDOM_ITERATOR_H_
