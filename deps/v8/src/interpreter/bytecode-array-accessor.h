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
namespace interpreter {

class V8_EXPORT_PRIVATE BytecodeArrayAccessor {
 public:
  BytecodeArrayAccessor(Handle<BytecodeArray> bytecode_array,
                        int initial_offset);

  void SetOffset(int offset);

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
  uint32_t GetRegisterCountOperand(int operand_index) const;
  Register GetRegisterOperand(int operand_index) const;
  int GetRegisterOperandRange(int operand_index) const;
  Runtime::FunctionId GetRuntimeIdOperand(int operand_index) const;
  Runtime::FunctionId GetIntrinsicIdOperand(int operand_index) const;
  Handle<Object> GetConstantForIndexOperand(int operand_index) const;

  // Returns the absolute offset of the branch target at the current
  // bytecode. It is an error to call this method if the bytecode is
  // not for a jump or conditional jump.
  int GetJumpTargetOffset() const;

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

#endif  // V8_INTERPRETER_BYTECODE_GRAPH_ACCESSOR_H_
