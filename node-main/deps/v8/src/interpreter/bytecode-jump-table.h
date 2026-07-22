// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_JUMP_TABLE_H_
#define V8_INTERPRETER_BYTECODE_JUMP_TABLE_H_

#include "src/utils/bit-vector.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace interpreter {

class ConstantArrayBuilder;

// A jump table for a set of targets in a bytecode array. When an entry in the
// table is bound, it represents a known position in the bytecode array. If no
// entries match, the switch falls through.
class V8_EXPORT_PRIVATE BytecodeJumpTable final : public ZoneObject {
 public:
  // Constructs a new BytecodeJumpTable starting at |constant_pool_index|, with
  // the given |size|, where the case values of the table start at
  // |case_value_base|.
  BytecodeJumpTable(size_t constant_pool_index, int size, int case_value_base,
                    Zone* zone)
      :
#ifdef DEBUG
        bound_(size, zone),
#endif
        constant_pool_index_(constant_pool_index),
        switch_bytecode_offset_(kInvalidOffset),
        size_(size),
        case_value_base_(case_value_base) {
  }

  size_t constant_pool_index() const { return constant_pool_index_; }
  size_t switch_bytecode_offset() const { return switch_bytecode_offset_; }
  int case_value_base() const { return case_value_base_; }
  int size() const { return size_; }
#ifdef DEBUG
  bool is_bound(int case_value) const {
    DCHECK_GE(case_value, case_value_base_);
    DCHECK_LT(case_value, case_value_base_ + size());
    return bound_.Contains(case_value - case_value_base_);
  }
#endif

  size_t ConstantPoolEntryFor(int case_value) {
    DCHECK_GE(case_value, case_value_base_);
    return constant_pool_index_ + case_value - case_value_base_;
  }

 private:
  static const size_t kInvalidIndex = static_cast<size_t>(-1);
  static const size_t kInvalidOffset = static_cast<size_t>(-1);

  void mark_bound(int case_value) {
#ifdef DEBUG
    DCHECK_GE(case_value, case_value_base_);
    DCHECK_LT(case_value, case_value_base_ + size());
    bound_.Add(case_value - case_value_base_);
#endif
  }

  void set_switch_bytecode_offset(size_t offset) {
    DCHECK_EQ(switch_bytecode_offset_, kInvalidOffset);
    switch_bytecode_offset_ = offset;
  }

#ifdef DEBUG
  // This bit vector is only used for DCHECKS, so only store the field in debug
  // builds.
  BitVector bound_;
#endif
  size_t constant_pool_index_;
  size_t switch_bytecode_offset_;
  int size_;
  int case_value_base_;

  friend class BytecodeArrayWriter;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_JUMP_TABLE_H_
