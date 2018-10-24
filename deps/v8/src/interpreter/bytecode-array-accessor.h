// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_ACCESSOR_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_ACCESSOR_H_

#include "src/globals.h"
#include "src/handles.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

class BytecodeArray;

namespace interpreter {

class BytecodeArrayAccessor;

struct V8_EXPORT_PRIVATE JumpTableTargetOffset {
  int case_value;
  int target_offset;
};

class V8_EXPORT_PRIVATE JumpTableTargetOffsets final {
 public:
  // Minimal iterator implementation for use in ranged-for.
  class V8_EXPORT_PRIVATE iterator final {
   public:
    iterator(int case_value, int table_offset, int table_end,
             const BytecodeArrayAccessor* accessor);

    JumpTableTargetOffset operator*();
    iterator& operator++();
    bool operator!=(const iterator& other);

   private:
    void UpdateAndAdvanceToValid();

    const BytecodeArrayAccessor* accessor_;
    Smi* current_;
    int index_;
    int table_offset_;
    int table_end_;
  };

  JumpTableTargetOffsets(const BytecodeArrayAccessor* accessor, int table_start,
                         int table_size, int case_value_base);

  iterator begin() const;
  iterator end() const;

  int size() const;

 private:
  const BytecodeArrayAccessor* accessor_;
  int table_start_;
  int table_size_;
  int case_value_base_;
};

class V8_EXPORT_PRIVATE BytecodeArrayAccessor {
 public:
  BytecodeArrayAccessor(Handle<BytecodeArray> bytecode_array,
                        int initial_offset);

  void SetOffset(int offset);

  void ApplyDebugBreak();

  Bytecode current_bytecode() const;
  int current_bytecode_size() const;
  int current_offset() const { return bytecode_offset_; }
  OperandScale current_operand_scale() const { return operand_scale_; }
  int current_prefix_offset() const { return prefix_offset_; }
  const Handle<BytecodeArray>& bytecode_array() const {
    return bytecode_array_;
  }

  uint32_t GetFlagOperand(int operand_index) const;
  uint32_t GetUnsignedImmediateOperand(int operand_index) const;
  int32_t GetImmediateOperand(int operand_index) const;
  uint32_t GetIndexOperand(int operand_index) const;
  FeedbackSlot GetSlotOperand(int operand_index) const;
  uint32_t GetRegisterCountOperand(int operand_index) const;
  Register GetRegisterOperand(int operand_index) const;
  int GetRegisterOperandRange(int operand_index) const;
  Runtime::FunctionId GetRuntimeIdOperand(int operand_index) const;
  Runtime::FunctionId GetIntrinsicIdOperand(int operand_index) const;
  uint32_t GetNativeContextIndexOperand(int operand_index) const;
  Object* GetConstantAtIndex(int offset) const;
  Object* GetConstantForIndexOperand(int operand_index) const;

  // Returns the absolute offset of the branch target at the current bytecode.
  // It is an error to call this method if the bytecode is not for a jump or
  // conditional jump.
  int GetJumpTargetOffset() const;
  // Returns an iterator over the absolute offsets of the targets of the current
  // switch bytecode's jump table. It is an error to call this method if the
  // bytecode is not a switch.
  JumpTableTargetOffsets GetJumpTableTargetOffsets() const;

  // Returns the absolute offset of the bytecode at the given relative offset
  // from the current bytecode.
  int GetAbsoluteOffset(int relative_offset) const;

  bool OffsetWithinBytecode(int offset) const;

  std::ostream& PrintTo(std::ostream& os) const;

 private:
  bool OffsetInBounds() const;

  uint32_t GetUnsignedOperand(int operand_index,
                              OperandType operand_type) const;
  int32_t GetSignedOperand(int operand_index, OperandType operand_type) const;

  void UpdateOperandScale();

  Handle<BytecodeArray> bytecode_array_;
  int bytecode_offset_;
  OperandScale operand_scale_;
  int prefix_offset_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeArrayAccessor);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_ACCESSOR_H_
