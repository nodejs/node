// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_REGISTER_OPTIMIZER_H_
#define V8_INTERPRETER_BYTECODE_REGISTER_OPTIMIZER_H_

#include "src/interpreter/bytecode-pipeline.h"

namespace v8 {
namespace internal {
namespace interpreter {

// An optimization stage for eliminating unnecessary transfers between
// registers. The bytecode generator uses temporary registers
// liberally for correctness and convenience and this stage removes
// transfers that are not required and preserves correctness.
class BytecodeRegisterOptimizer final
    : public BytecodePipelineStage,
      public BytecodeRegisterAllocator::Observer,
      public ZoneObject {
 public:
  BytecodeRegisterOptimizer(Zone* zone,
                            BytecodeRegisterAllocator* register_allocator,
                            int fixed_registers_count, int parameter_count,
                            BytecodePipelineStage* next_stage);
  virtual ~BytecodeRegisterOptimizer() {}

  // BytecodePipelineStage interface.
  void Write(BytecodeNode* node) override;
  void WriteJump(BytecodeNode* node, BytecodeLabel* label) override;
  void BindLabel(BytecodeLabel* label) override;
  void BindLabel(const BytecodeLabel& target, BytecodeLabel* label) override;
  Handle<BytecodeArray> ToBytecodeArray(
      Isolate* isolate, int register_count, int parameter_count,
      Handle<FixedArray> handler_table) override;

 private:
  static const uint32_t kInvalidEquivalenceId = kMaxUInt32;

  class RegisterInfo;

  // BytecodeRegisterAllocator::Observer interface.
  void RegisterAllocateEvent(Register reg) override;
  void RegisterListAllocateEvent(RegisterList reg_list) override;
  void RegisterListFreeEvent(RegisterList reg) override;

  // Helpers for BytecodePipelineStage interface.
  void FlushState();

  // Update internal state for register transfer from |input| to
  // |output| using |source_info| as source position information if
  // any bytecodes are emitted due to transfer.
  void RegisterTransfer(RegisterInfo* input, RegisterInfo* output,
                        BytecodeSourceInfo* source_info);

  // Emit a register transfer bytecode from |input| to |output|.
  void OutputRegisterTransfer(RegisterInfo* input, RegisterInfo* output,
                              BytecodeSourceInfo* source_info = nullptr);

  // Emits a Nop to preserve source position information in the
  // bytecode pipeline.
  void EmitNopForSourceInfo(BytecodeSourceInfo* source_info) const;

  // Handlers for bytecode nodes for register to register transfers.
  void DoLdar(BytecodeNode* node);
  void DoMov(BytecodeNode* node);
  void DoStar(BytecodeNode* node);

  // Operand processing methods for bytecodes other than those
  // performing register to register transfers.
  void PrepareOperands(BytecodeNode* const node);
  void PrepareAccumulator(BytecodeNode* const node);
  void PrepareRegisterOperands(BytecodeNode* const node);

  void PrepareRegisterOutputOperand(RegisterInfo* reg_info);
  void PrepareRegisterRangeOutputOperand(Register start, int count);
  void PrepareRegisterInputOperand(BytecodeNode* const node, Register reg,
                                   int operand_index);
  void PrepareRegisterRangeInputOperand(Register start, int count);

  Register GetEquivalentRegisterForInputOperand(Register reg);

  static Register GetRegisterInputOperand(int index, Bytecode bytecode,
                                          const uint32_t* operands,
                                          int operand_count);
  static Register GetRegisterOutputOperand(int index, Bytecode bytecode,
                                           const uint32_t* operands,
                                           int operand_count);

  void CreateMaterializedEquivalent(RegisterInfo* info);
  RegisterInfo* GetMaterializedEquivalent(RegisterInfo* info);
  RegisterInfo* GetMaterializedEquivalentNotAccumulator(RegisterInfo* info);
  void Materialize(RegisterInfo* info);
  void AddToEquivalenceSet(RegisterInfo* set_member,
                           RegisterInfo* non_set_member);

  // Methods for finding and creating metadata for each register.
  RegisterInfo* GetOrCreateRegisterInfo(Register reg);
  RegisterInfo* GetRegisterInfo(Register reg);
  RegisterInfo* NewRegisterInfo(Register reg);
  void GrowRegisterMap(Register reg);

  bool RegisterIsTemporary(Register reg) const {
    return reg >= temporary_base_;
  }

  bool RegisterIsObservable(Register reg) const {
    return reg != accumulator_ && !RegisterIsTemporary(reg);
  }

  static Register OperandToRegister(uint32_t operand) {
    return Register::FromOperand(static_cast<int32_t>(operand));
  }

  size_t GetRegisterInfoTableIndex(Register reg) const {
    return static_cast<size_t>(reg.index() + register_info_table_offset_);
  }

  Register RegisterFromRegisterInfoTableIndex(size_t index) const {
    return Register(static_cast<int>(index) - register_info_table_offset_);
  }

  uint32_t NextEquivalenceId() {
    equivalence_id_++;
    CHECK_NE(equivalence_id_, kInvalidEquivalenceId);
    return equivalence_id_;
  }

  Zone* zone() { return zone_; }

  const Register accumulator_;
  RegisterInfo* accumulator_info_;
  const Register temporary_base_;
  int max_register_index_;

  // Direct mapping to register info.
  ZoneVector<RegisterInfo*> register_info_table_;
  int register_info_table_offset_;

  // Counter for equivalence sets identifiers.
  int equivalence_id_;

  BytecodePipelineStage* next_stage_;
  bool flush_required_;
  Zone* zone_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeRegisterOptimizer);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_REGISTER_OPTIMIZER_H_
