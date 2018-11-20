// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_SOURCE_INFO_H_
#define V8_INTERPRETER_BYTECODE_SOURCE_INFO_H_

#include "src/globals.h"

namespace v8 {
namespace internal {
namespace interpreter {

// Source code position information.
class BytecodeSourceInfo final {
 public:
  static const int kUninitializedPosition = -1;

  BytecodeSourceInfo()
      : position_type_(PositionType::kNone),
        source_position_(kUninitializedPosition) {}

  BytecodeSourceInfo(int source_position, bool is_statement)
      : position_type_(is_statement ? PositionType::kStatement
                                    : PositionType::kExpression),
        source_position_(source_position) {
    DCHECK_GE(source_position, 0);
  }

  // Makes instance into a statement position.
  void MakeStatementPosition(int source_position) {
    // Statement positions can be replaced by other statement
    // positions. For example , "for (x = 0; x < 3; ++x) 7;" has a
    // statement position associated with 7 but no bytecode associated
    // with it. Then Next is emitted after the body and has
    // statement position and overrides the existing one.
    position_type_ = PositionType::kStatement;
    source_position_ = source_position;
  }

  // Makes instance into an expression position. Instance should not
  // be a statement position otherwise it could be lost and impair the
  // debugging experience.
  void MakeExpressionPosition(int source_position) {
    DCHECK(!is_statement());
    position_type_ = PositionType::kExpression;
    source_position_ = source_position;
  }

  // Forces an instance into an expression position.
  void ForceExpressionPosition(int source_position) {
    position_type_ = PositionType::kExpression;
    source_position_ = source_position;
  }

  int source_position() const {
    DCHECK(is_valid());
    return source_position_;
  }

  bool is_statement() const {
    return position_type_ == PositionType::kStatement;
  }
  bool is_expression() const {
    return position_type_ == PositionType::kExpression;
  }

  bool is_valid() const { return position_type_ != PositionType::kNone; }
  void set_invalid() {
    position_type_ = PositionType::kNone;
    source_position_ = kUninitializedPosition;
  }

  bool operator==(const BytecodeSourceInfo& other) const {
    return position_type_ == other.position_type_ &&
           source_position_ == other.source_position_;
  }

  bool operator!=(const BytecodeSourceInfo& other) const {
    return position_type_ != other.position_type_ ||
           source_position_ != other.source_position_;
  }

 private:
  enum class PositionType : uint8_t { kNone, kExpression, kStatement };

  PositionType position_type_;
  int source_position_;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const BytecodeSourceInfo& info);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_SOURCE_INFO_H_
