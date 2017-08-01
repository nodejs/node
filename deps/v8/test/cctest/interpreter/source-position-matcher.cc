// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/interpreter/source-position-matcher.h"

#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace interpreter {

// Comparer for PositionTableEntry instances.
struct PositionTableEntryComparer {
  bool operator()(const PositionTableEntry& lhs,
                  const PositionTableEntry& rhs) const {
    int lhs_type_score = type_score(lhs);
    int rhs_type_score = type_score(rhs);
    if (lhs_type_score == rhs_type_score) {
      return lhs.source_position < rhs.source_position;
    } else {
      return lhs_type_score < rhs_type_score;
    }
  }

  int type_score(const PositionTableEntry& entry) const {
    return entry.is_statement ? 1 : 0;
  }
};

//
// The principles for comparing source positions in bytecode arrays
// are:
//
// 1. The number of statement positions must be the same in both.
//
// 2. Statement positions may be moved provide they do not affect the
//    debuggers causal view of the v8 heap and local state. This means
//    statement positions may be moved when their initial position is
//    on bytecodes that manipulate the accumulator and temporary
//    registers.
//
// 3. When duplicate expression positions are present, either may
//    be dropped.
//
// 4. Expression positions may be applied to later bytecodes in the
//    bytecode array if the current bytecode does not throw.
//
// 5. Expression positions may be dropped when they are applied to
//    bytecodes that manipulate local frame state and immediately
//    proceeded by another source position.
//
// 6. The relative ordering of source positions must be preserved.
//
bool SourcePositionMatcher::Match(Handle<BytecodeArray> original_bytecode,
                                  Handle<BytecodeArray> optimized_bytecode) {
  SourcePositionTableIterator original(
      original_bytecode->SourcePositionTable());
  SourcePositionTableIterator optimized(
      optimized_bytecode->SourcePositionTable());

  int last_original_bytecode_offset = 0;
  int last_optimized_bytecode_offset = 0;

  // Ordered lists of expression positions immediately before the
  // latest statements in each bytecode array.
  std::vector<PositionTableEntry> original_expression_entries;
  std::vector<PositionTableEntry> optimized_expression_entries;

  while (true) {
    MoveToNextStatement(&original, &original_expression_entries);
    MoveToNextStatement(&optimized, &optimized_expression_entries);

    if (original.done() && optimized.done()) {
      return true;
    } else if (original.done()) {
      return false;
    } else if (optimized.done()) {
      return false;
    }

    if (HasNewExpressionPositionsInOptimized(&original_expression_entries,
                                             &optimized_expression_entries)) {
      return false;
    }

    StripUnneededExpressionPositions(original_bytecode,
                                     &original_expression_entries,
                                     original.code_offset());
    StripUnneededExpressionPositions(optimized_bytecode,
                                     &optimized_expression_entries,
                                     optimized.code_offset());

    if (!CompareExpressionPositions(&original_expression_entries,
                                    &optimized_expression_entries)) {
      // Message logged in CompareExpressionPositions().
      return false;
    }

    // Check original and optimized have matching source positions.
    if (original.source_position() != optimized.source_position()) {
      return false;
    }

    if (original.code_offset() < last_original_bytecode_offset) {
      return false;
    }
    last_original_bytecode_offset = original.code_offset();

    if (optimized.code_offset() < last_optimized_bytecode_offset) {
      return false;
    }
    last_optimized_bytecode_offset = optimized.code_offset();

    // TODO(oth): Can we compare statement positions are semantically
    // equivalent? e.g. before a bytecode that has debugger observable
    // effects. This is likely non-trivial.
  }

  return true;
}

bool SourcePositionMatcher::HasNewExpressionPositionsInOptimized(
    const std::vector<PositionTableEntry>* const original_positions,
    const std::vector<PositionTableEntry>* const optimized_positions) {
  std::set<PositionTableEntry, PositionTableEntryComparer> original_set(
      original_positions->begin(), original_positions->end());

  bool retval = false;
  for (auto optimized_position : *optimized_positions) {
    if (original_set.find(optimized_position) == original_set.end()) {
      retval = true;
    }
  }
  return retval;
}

bool SourcePositionMatcher::CompareExpressionPositions(
    const std::vector<PositionTableEntry>* const original_positions,
    const std::vector<PositionTableEntry>* const optimized_positions) {
  if (original_positions->size() != optimized_positions->size()) {
    return false;
  }

  if (original_positions->size() == 0) {
    return true;
  }

  for (size_t i = 0; i < original_positions->size(); ++i) {
    PositionTableEntry original = original_positions->at(i);
    PositionTableEntry optimized = original_positions->at(i);
    CHECK(original.source_position > 0);
    if ((original.is_statement || optimized.is_statement) ||
        (original.source_position != optimized.source_position) ||
        (original.source_position < 0)) {
      return false;
    }
  }
  return true;
}

void SourcePositionMatcher::StripUnneededExpressionPositions(
    Handle<BytecodeArray> bytecode_array,
    std::vector<PositionTableEntry>* expression_positions,
    int next_statement_bytecode_offset) {
  size_t j = 0;
  for (size_t i = 0; i < expression_positions->size(); ++i) {
    CHECK(expression_positions->at(i).source_position > 0 &&
          !expression_positions->at(i).is_statement);
    int bytecode_end = (i == expression_positions->size() - 1)
                           ? next_statement_bytecode_offset
                           : expression_positions->at(i + 1).code_offset;
    if (ExpressionPositionIsNeeded(bytecode_array,
                                   expression_positions->at(i).code_offset,
                                   bytecode_end)) {
      expression_positions->at(j++) = expression_positions->at(i);
    }
  }
  expression_positions->resize(j);
}

void SourcePositionMatcher::AdvanceBytecodeIterator(
    BytecodeArrayIterator* iterator, int bytecode_offset) {
  while (iterator->current_offset() != bytecode_offset) {
    iterator->Advance();
  }
}

bool SourcePositionMatcher::ExpressionPositionIsNeeded(
    Handle<BytecodeArray> bytecode_array, int start_offset, int end_offset) {
  CHECK_GT(end_offset, start_offset);
  BytecodeArrayIterator iterator(bytecode_array);
  AdvanceBytecodeIterator(&iterator, start_offset);

  while (iterator.current_offset() != end_offset) {
    if (Bytecodes::IsWithoutExternalSideEffects(iterator.current_bytecode())) {
      iterator.Advance();
    } else {
      // Bytecode could throw so need an expression position.
      return true;
    }
  }
  return false;
}

void SourcePositionMatcher::MoveToNextStatement(
    SourcePositionTableIterator* iterator,
    std::vector<PositionTableEntry>* positions) {
  iterator->Advance();
  positions->clear();
  while (!iterator->done()) {
    if (iterator->is_statement()) {
      break;
    }
    positions->push_back({iterator->code_offset(),
                          iterator->source_position().raw(),
                          iterator->is_statement()});
    iterator->Advance();
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
