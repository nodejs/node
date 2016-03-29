// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_SOURCE_POSITION_TABLE_H_
#define V8_INTERPRETER_SOURCE_POSITION_TABLE_H_

#include "src/assert-scope.h"
#include "src/handles.h"
#include "src/zone.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {

class BytecodeArray;
class FixedArray;
class Isolate;

namespace interpreter {

class SourcePositionTableBuilder {
 public:
  explicit SourcePositionTableBuilder(Isolate* isolate, Zone* zone)
      : isolate_(isolate), entries_(zone) {}

  void AddStatementPosition(size_t bytecode_offset, int source_position);
  void AddExpressionPosition(size_t bytecode_offset, int source_position);
  void RevertPosition(size_t bytecode_offset);
  Handle<FixedArray> ToFixedArray();

 private:
  struct Entry {
    int bytecode_offset;
    uint32_t source_position_and_type;
  };

  bool CodeOffsetHasPosition(int bytecode_offset) {
    // Return whether bytecode offset already has a position assigned.
    return entries_.size() > 0 &&
           entries_.back().bytecode_offset == bytecode_offset;
  }

  Isolate* isolate_;
  ZoneVector<Entry> entries_;
};

class SourcePositionTableIterator {
 public:
  explicit SourcePositionTableIterator(BytecodeArray* bytecode_array);

  void Advance();

  int bytecode_offset() const {
    DCHECK(!done());
    return bytecode_offset_;
  }
  int source_position() const {
    DCHECK(!done());
    return source_position_;
  }
  bool is_statement() const {
    DCHECK(!done());
    return is_statement_;
  }
  bool done() const { return index_ > length_; }

 private:
  FixedArray* table_;
  int index_;
  int length_;
  bool is_statement_;
  int bytecode_offset_;
  int source_position_;
  DisallowHeapAllocation no_gc;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_SOURCE_POSITION_TABLE_H_
