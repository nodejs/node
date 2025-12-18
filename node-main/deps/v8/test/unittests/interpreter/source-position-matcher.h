// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_UNITTESTS_INTERPRETER_SOURCE_POSITION_COMPARER_H_
#define TEST_UNITTESTS_INTERPRETER_SOURCE_POSITION_COMPARER_H_

#include "src/codegen/source-position-table.h"
#include "src/init/v8.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {
namespace interpreter {

class SourcePositionMatcher final {
 public:
  bool Match(Handle<BytecodeArray> original, Handle<BytecodeArray> optimized);

 private:
  bool HasNewExpressionPositionsInOptimized(
      const std::vector<PositionTableEntry>* const original_positions,
      const std::vector<PositionTableEntry>* const optimized_positions);

  bool CompareExpressionPositions(
      const std::vector<PositionTableEntry>* const original_positions,
      const std::vector<PositionTableEntry>* const optimized_positions);

  void StripUnneededExpressionPositions(
      Handle<BytecodeArray> bytecode_array,
      std::vector<PositionTableEntry>* positions,
      int next_statement_bytecode_offset);

  bool ExpressionPositionIsNeeded(Handle<BytecodeArray> bytecode_array,
                                  int start_offset, int end_offset);

  void MoveToNextStatement(
      SourcePositionTableIterator* iterator,
      std::vector<PositionTableEntry>* expression_positions);

  void AdvanceBytecodeIterator(BytecodeArrayIterator* iterator,
                               int bytecode_offset);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // TEST_UNITTESTS_INTERPRETER_SOURCE_POSITION_COMPARER_H_
