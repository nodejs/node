// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_BYTECODE_ITERATOR_H_
#define V8_REGEXP_REGEXP_BYTECODE_ITERATOR_H_

#include <vector>

#include "src/handles/handles.h"
#include "src/regexp/regexp-bytecodes.h"

namespace v8 {
namespace internal {

class TrustedByteArray;

class RegExpBytecodeIterator {
 public:
  explicit RegExpBytecodeIterator(DirectHandle<TrustedByteArray> bytecode);
  ~RegExpBytecodeIterator();

  inline bool done() const;
  inline void advance();
  inline RegExpBytecode current_bytecode() const;
  inline uint8_t current_size() const;
  inline int current_offset() const;
  inline uint8_t* current_address() const;
  inline void reset();

  // Calls |fun| templatized by RegExpBytecode for each bytecode in the
  // Bytecode Array.
  template <typename Func>
  inline void ForEachBytecode(Func&& fun);

  static void UpdatePointersCallback(void* iterator);

 private:
  void UpdatePointers();

  DirectHandle<TrustedByteArray> bytecode_;
  uint8_t* start_;
  uint8_t* end_;
  uint8_t* cursor_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODE_ITERATOR_H_
