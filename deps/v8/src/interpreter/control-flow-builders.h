// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_CONTROL_FLOW_BUILDERS_H_
#define V8_INTERPRETER_CONTROL_FLOW_BUILDERS_H_

#include "src/interpreter/bytecode-array-builder.h"

#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace interpreter {

class ControlFlowBuilder BASE_EMBEDDED {
 public:
  explicit ControlFlowBuilder(BytecodeArrayBuilder* builder)
      : builder_(builder) {}
  virtual ~ControlFlowBuilder() {}

 protected:
  BytecodeArrayBuilder* builder() const { return builder_; }

 private:
  BytecodeArrayBuilder* builder_;

  DISALLOW_COPY_AND_ASSIGN(ControlFlowBuilder);
};

class BreakableControlFlowBuilder : public ControlFlowBuilder {
 public:
  explicit BreakableControlFlowBuilder(BytecodeArrayBuilder* builder)
      : ControlFlowBuilder(builder),
        break_sites_(builder->zone()) {}
  virtual ~BreakableControlFlowBuilder();

  // This method should be called by the control flow owner before
  // destruction to update sites that emit jumps for break.
  void SetBreakTarget(const BytecodeLabel& break_target);

  // This method is called when visiting break statements in the AST.
  // Inserts a jump to a unbound label that is patched when the corresponding
  // SetBreakTarget is called.
  void Break() { EmitJump(&break_sites_); }
  void BreakIfTrue() { EmitJumpIfTrue(&break_sites_); }
  void BreakIfFalse() { EmitJumpIfFalse(&break_sites_); }
  void BreakIfUndefined() { EmitJumpIfUndefined(&break_sites_); }
  void BreakIfNull() { EmitJumpIfNull(&break_sites_); }

 protected:
  void EmitJump(ZoneVector<BytecodeLabel>* labels);
  void EmitJump(ZoneVector<BytecodeLabel>* labels, int index);
  void EmitJumpIfTrue(ZoneVector<BytecodeLabel>* labels);
  void EmitJumpIfTrue(ZoneVector<BytecodeLabel>* labels, int index);
  void EmitJumpIfFalse(ZoneVector<BytecodeLabel>* labels);
  void EmitJumpIfFalse(ZoneVector<BytecodeLabel>* labels, int index);
  void EmitJumpIfUndefined(ZoneVector<BytecodeLabel>* labels);
  void EmitJumpIfNull(ZoneVector<BytecodeLabel>* labels);

  void BindLabels(const BytecodeLabel& target, ZoneVector<BytecodeLabel>* site);

  // Unbound labels that identify jumps for break statements in the code.
  ZoneVector<BytecodeLabel> break_sites_;
};


// Class to track control flow for block statements (which can break in JS).
class BlockBuilder final : public BreakableControlFlowBuilder {
 public:
  explicit BlockBuilder(BytecodeArrayBuilder* builder)
      : BreakableControlFlowBuilder(builder) {}

  void EndBlock();

 private:
  BytecodeLabel block_end_;
};


// A class to help with co-ordinating break and continue statements with
// their loop.
class LoopBuilder final : public BreakableControlFlowBuilder {
 public:
  explicit LoopBuilder(BytecodeArrayBuilder* builder)
      : BreakableControlFlowBuilder(builder),
        continue_sites_(builder->zone()) {}
  ~LoopBuilder();

  void LoopHeader();
  void Condition() { builder()->Bind(&condition_); }
  void Next() { builder()->Bind(&next_); }
  void JumpToHeader() { builder()->Jump(&loop_header_); }
  void JumpToHeaderIfTrue() { builder()->JumpIfTrue(&loop_header_); }
  void EndLoop();

  // This method is called when visiting continue statements in the AST.
  // Inserts a jump to a unbound label that is patched when the corresponding
  // SetContinueTarget is called.
  void Continue() { EmitJump(&continue_sites_); }
  void ContinueIfTrue() { EmitJumpIfTrue(&continue_sites_); }
  void ContinueIfUndefined() { EmitJumpIfUndefined(&continue_sites_); }
  void ContinueIfNull() { EmitJumpIfNull(&continue_sites_); }

 private:
  void SetContinueTarget(const BytecodeLabel& continue_target);

  BytecodeLabel loop_header_;
  BytecodeLabel condition_;
  BytecodeLabel next_;
  BytecodeLabel loop_end_;

  // Unbound labels that identify jumps for continue statements in the code.
  ZoneVector<BytecodeLabel> continue_sites_;
};


// A class to help with co-ordinating break statements with their switch.
class SwitchBuilder final : public BreakableControlFlowBuilder {
 public:
  explicit SwitchBuilder(BytecodeArrayBuilder* builder, int number_of_cases)
      : BreakableControlFlowBuilder(builder),
        case_sites_(builder->zone()) {
    case_sites_.resize(number_of_cases);
  }
  ~SwitchBuilder();

  // This method should be called by the SwitchBuilder owner when the case
  // statement with |index| is emitted to update the case jump site.
  void SetCaseTarget(int index);

  // This method is called when visiting case comparison operation for |index|.
  // Inserts a JumpIfTrue to a unbound label that is patched when the
  // corresponding SetCaseTarget is called.
  void Case(int index) { EmitJumpIfTrue(&case_sites_, index); }

  // This method is called when all cases comparisons have been emitted if there
  // is a default case statement. Inserts a Jump to a unbound label that is
  // patched when the corresponding SetCaseTarget is called.
  void DefaultAt(int index) { EmitJump(&case_sites_, index); }

 private:
  // Unbound labels that identify jumps for case statements in the code.
  ZoneVector<BytecodeLabel> case_sites_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_CONTROL_FLOW_BUILDERS_H_
