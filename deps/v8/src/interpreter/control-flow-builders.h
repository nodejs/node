// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_CONTROL_FLOW_BUILDERS_H_
#define V8_INTERPRETER_CONTROL_FLOW_BUILDERS_H_

#include "src/interpreter/bytecode-array-builder.h"

#include "src/interpreter/bytecode-label.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace interpreter {

class V8_EXPORT_PRIVATE ControlFlowBuilder BASE_EMBEDDED {
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

class V8_EXPORT_PRIVATE BreakableControlFlowBuilder
    : public ControlFlowBuilder {
 public:
  explicit BreakableControlFlowBuilder(BytecodeArrayBuilder* builder)
      : ControlFlowBuilder(builder), break_labels_(builder->zone()) {}
  virtual ~BreakableControlFlowBuilder();

  // This method should be called by the control flow owner before
  // destruction to update sites that emit jumps for break.
  void BindBreakTarget();

  // This method is called when visiting break statements in the AST.
  // Inserts a jump to an unbound label that is patched when the corresponding
  // BindBreakTarget is called.
  void Break() { EmitJump(&break_labels_); }
  void BreakIfTrue(BytecodeArrayBuilder::ToBooleanMode mode) {
    EmitJumpIfTrue(mode, &break_labels_);
  }
  void BreakIfFalse(BytecodeArrayBuilder::ToBooleanMode mode) {
    EmitJumpIfFalse(mode, &break_labels_);
  }
  void BreakIfUndefined() { EmitJumpIfUndefined(&break_labels_); }
  void BreakIfNull() { EmitJumpIfNull(&break_labels_); }

  BytecodeLabels* break_labels() { return &break_labels_; }

 protected:
  void EmitJump(BytecodeLabels* labels);
  void EmitJumpIfTrue(BytecodeArrayBuilder::ToBooleanMode mode,
                      BytecodeLabels* labels);
  void EmitJumpIfFalse(BytecodeArrayBuilder::ToBooleanMode mode,
                       BytecodeLabels* labels);
  void EmitJumpIfUndefined(BytecodeLabels* labels);
  void EmitJumpIfNull(BytecodeLabels* labels);

  // Unbound labels that identify jumps for break statements in the code.
  BytecodeLabels break_labels_;
};


// Class to track control flow for block statements (which can break in JS).
class V8_EXPORT_PRIVATE BlockBuilder final
    : public BreakableControlFlowBuilder {
 public:
  explicit BlockBuilder(BytecodeArrayBuilder* builder)
      : BreakableControlFlowBuilder(builder) {}

  void EndBlock();

 private:
  BytecodeLabel block_end_;
};


// A class to help with co-ordinating break and continue statements with
// their loop.
class V8_EXPORT_PRIVATE LoopBuilder final : public BreakableControlFlowBuilder {
 public:
  explicit LoopBuilder(BytecodeArrayBuilder* builder)
      : BreakableControlFlowBuilder(builder),
        continue_labels_(builder->zone()),
        generator_jump_table_location_(nullptr),
        parent_generator_jump_table_(nullptr) {}
  ~LoopBuilder();

  void LoopHeader();
  void LoopHeaderInGenerator(BytecodeJumpTable** parent_generator_jump_table,
                             int first_resume_id, int resume_count);
  void JumpToHeader(int loop_depth);
  void BindContinueTarget();

  // This method is called when visiting continue statements in the AST.
  // Inserts a jump to an unbound label that is patched when BindContinueTarget
  // is called.
  void Continue() { EmitJump(&continue_labels_); }
  void ContinueIfUndefined() { EmitJumpIfUndefined(&continue_labels_); }
  void ContinueIfNull() { EmitJumpIfNull(&continue_labels_); }

 private:
  BytecodeLabel loop_header_;

  // Unbound labels that identify jumps for continue statements in the code and
  // jumps from checking the loop condition to the header for do-while loops.
  BytecodeLabels continue_labels_;

  // While we're in the loop, we want to have a different jump table for
  // generator switch statements. We restore it at the end of the loop.
  // TODO(leszeks): Storing a pointer to the BytecodeGenerator's jump table
  // field is ugly, figure out a better way to do this.
  BytecodeJumpTable** generator_jump_table_location_;
  BytecodeJumpTable* parent_generator_jump_table_;
};


// A class to help with co-ordinating break statements with their switch.
class V8_EXPORT_PRIVATE SwitchBuilder final
    : public BreakableControlFlowBuilder {
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
  // Inserts a JumpIfTrue with ToBooleanMode |mode| to a unbound label that is
  // patched when the corresponding SetCaseTarget is called.
  void Case(BytecodeArrayBuilder::ToBooleanMode mode, int index) {
    builder()->JumpIfTrue(mode, &case_sites_.at(index));
  }

  // This method is called when all cases comparisons have been emitted if there
  // is a default case statement. Inserts a Jump to a unbound label that is
  // patched when the corresponding SetCaseTarget is called.
  void DefaultAt(int index) { builder()->Jump(&case_sites_.at(index)); }

 private:
  // Unbound labels that identify jumps for case statements in the code.
  ZoneVector<BytecodeLabel> case_sites_;
};


// A class to help with co-ordinating control flow in try-catch statements.
class V8_EXPORT_PRIVATE TryCatchBuilder final : public ControlFlowBuilder {
 public:
  explicit TryCatchBuilder(BytecodeArrayBuilder* builder,
                           HandlerTable::CatchPrediction catch_prediction)
      : ControlFlowBuilder(builder),
        handler_id_(builder->NewHandlerEntry()),
        catch_prediction_(catch_prediction) {}

  void BeginTry(Register context);
  void EndTry();
  void EndCatch();

 private:
  int handler_id_;
  HandlerTable::CatchPrediction catch_prediction_;
  BytecodeLabel handler_;
  BytecodeLabel exit_;
};


// A class to help with co-ordinating control flow in try-finally statements.
class V8_EXPORT_PRIVATE TryFinallyBuilder final : public ControlFlowBuilder {
 public:
  explicit TryFinallyBuilder(BytecodeArrayBuilder* builder,
                             HandlerTable::CatchPrediction catch_prediction)
      : ControlFlowBuilder(builder),
        handler_id_(builder->NewHandlerEntry()),
        catch_prediction_(catch_prediction),
        finalization_sites_(builder->zone()) {}

  void BeginTry(Register context);
  void LeaveTry();
  void EndTry();
  void BeginHandler();
  void BeginFinally();
  void EndFinally();

 private:
  int handler_id_;
  HandlerTable::CatchPrediction catch_prediction_;
  BytecodeLabel handler_;

  // Unbound labels that identify jumps to the finally block in the code.
  BytecodeLabels finalization_sites_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_CONTROL_FLOW_BUILDERS_H_
