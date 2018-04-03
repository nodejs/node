// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_CONTROL_FLOW_BUILDERS_H_
#define V8_INTERPRETER_CONTROL_FLOW_BUILDERS_H_

#include "src/interpreter/bytecode-array-builder.h"

#include "src/ast/ast-source-ranges.h"
#include "src/interpreter/block-coverage-builder.h"
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
  BreakableControlFlowBuilder(BytecodeArrayBuilder* builder,
                              BlockCoverageBuilder* block_coverage_builder,
                              AstNode* node)
      : ControlFlowBuilder(builder),
        break_labels_(builder->zone()),
        node_(node),
        block_coverage_builder_(block_coverage_builder) {}
  virtual ~BreakableControlFlowBuilder();

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

  void set_needs_continuation_counter() { needs_continuation_counter_ = true; }
  bool needs_continuation_counter() const {
    return needs_continuation_counter_;
  }

 protected:
  void EmitJump(BytecodeLabels* labels);
  void EmitJumpIfTrue(BytecodeArrayBuilder::ToBooleanMode mode,
                      BytecodeLabels* labels);
  void EmitJumpIfFalse(BytecodeArrayBuilder::ToBooleanMode mode,
                       BytecodeLabels* labels);
  void EmitJumpIfUndefined(BytecodeLabels* labels);
  void EmitJumpIfNull(BytecodeLabels* labels);

  // Called from the destructor to update sites that emit jumps for break.
  void BindBreakTarget();

  // Unbound labels that identify jumps for break statements in the code.
  BytecodeLabels break_labels_;

  // A continuation counter (for block coverage) is needed e.g. when
  // encountering a break statement.
  AstNode* node_;
  bool needs_continuation_counter_ = false;
  BlockCoverageBuilder* block_coverage_builder_;
};


// Class to track control flow for block statements (which can break in JS).
class V8_EXPORT_PRIVATE BlockBuilder final
    : public BreakableControlFlowBuilder {
 public:
  BlockBuilder(BytecodeArrayBuilder* builder,
               BlockCoverageBuilder* block_coverage_builder,
               BreakableStatement* statement)
      : BreakableControlFlowBuilder(builder, block_coverage_builder,
                                    statement) {}
};


// A class to help with co-ordinating break and continue statements with
// their loop.
class V8_EXPORT_PRIVATE LoopBuilder final : public BreakableControlFlowBuilder {
 public:
  LoopBuilder(BytecodeArrayBuilder* builder,
              BlockCoverageBuilder* block_coverage_builder, AstNode* node)
      : BreakableControlFlowBuilder(builder, block_coverage_builder, node),
        continue_labels_(builder->zone()),
        generator_jump_table_location_(nullptr),
        parent_generator_jump_table_(nullptr) {
    if (block_coverage_builder_ != nullptr) {
      set_needs_continuation_counter();
      block_coverage_body_slot_ =
          block_coverage_builder_->AllocateBlockCoverageSlot(
              node, SourceRangeKind::kBody);
    }
  }
  ~LoopBuilder();

  void LoopHeader();
  void LoopHeaderInGenerator(BytecodeJumpTable** parent_generator_jump_table,
                             int first_resume_id, int resume_count);
  void LoopBody();
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

  int block_coverage_body_slot_;
};


// A class to help with co-ordinating break statements with their switch.
class V8_EXPORT_PRIVATE SwitchBuilder final
    : public BreakableControlFlowBuilder {
 public:
  SwitchBuilder(BytecodeArrayBuilder* builder,
                BlockCoverageBuilder* block_coverage_builder,
                SwitchStatement* statement, int number_of_cases)
      : BreakableControlFlowBuilder(builder, block_coverage_builder, statement),
        case_sites_(builder->zone()) {
    case_sites_.resize(number_of_cases);
  }
  ~SwitchBuilder();

  // This method should be called by the SwitchBuilder owner when the case
  // statement with |index| is emitted to update the case jump site.
  void SetCaseTarget(int index, CaseClause* clause);

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
  TryCatchBuilder(BytecodeArrayBuilder* builder,
                  BlockCoverageBuilder* block_coverage_builder,
                  TryCatchStatement* statement,
                  HandlerTable::CatchPrediction catch_prediction)
      : ControlFlowBuilder(builder),
        handler_id_(builder->NewHandlerEntry()),
        catch_prediction_(catch_prediction),
        block_coverage_builder_(block_coverage_builder),
        statement_(statement) {}

  ~TryCatchBuilder();

  void BeginTry(Register context);
  void EndTry();
  void EndCatch();

 private:
  int handler_id_;
  HandlerTable::CatchPrediction catch_prediction_;
  BytecodeLabel handler_;
  BytecodeLabel exit_;

  BlockCoverageBuilder* block_coverage_builder_;
  TryCatchStatement* statement_;
};


// A class to help with co-ordinating control flow in try-finally statements.
class V8_EXPORT_PRIVATE TryFinallyBuilder final : public ControlFlowBuilder {
 public:
  TryFinallyBuilder(BytecodeArrayBuilder* builder,
                    BlockCoverageBuilder* block_coverage_builder,
                    TryFinallyStatement* statement,
                    HandlerTable::CatchPrediction catch_prediction)
      : ControlFlowBuilder(builder),
        handler_id_(builder->NewHandlerEntry()),
        catch_prediction_(catch_prediction),
        finalization_sites_(builder->zone()),
        block_coverage_builder_(block_coverage_builder),
        statement_(statement) {}

  ~TryFinallyBuilder();

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

  BlockCoverageBuilder* block_coverage_builder_;
  TryFinallyStatement* statement_;
};

class V8_EXPORT_PRIVATE ConditionalControlFlowBuilder final
    : public ControlFlowBuilder {
 public:
  ConditionalControlFlowBuilder(BytecodeArrayBuilder* builder,
                                BlockCoverageBuilder* block_coverage_builder,
                                AstNode* node)
      : ControlFlowBuilder(builder),
        end_labels_(builder->zone()),
        then_labels_(builder->zone()),
        else_labels_(builder->zone()),
        node_(node),
        block_coverage_builder_(block_coverage_builder) {
    DCHECK(node->IsIfStatement() || node->IsConditional());
    if (block_coverage_builder != nullptr) {
      block_coverage_then_slot_ =
          block_coverage_builder->AllocateBlockCoverageSlot(
              node, SourceRangeKind::kThen);
      block_coverage_else_slot_ =
          block_coverage_builder->AllocateBlockCoverageSlot(
              node, SourceRangeKind::kElse);
    }
  }
  ~ConditionalControlFlowBuilder();

  BytecodeLabels* then_labels() { return &then_labels_; }
  BytecodeLabels* else_labels() { return &else_labels_; }

  void Then();
  void Else();

  void JumpToEnd();

 private:
  BytecodeLabels end_labels_;
  BytecodeLabels then_labels_;
  BytecodeLabels else_labels_;

  AstNode* node_;
  int block_coverage_then_slot_;
  int block_coverage_else_slot_;
  BlockCoverageBuilder* block_coverage_builder_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_CONTROL_FLOW_BUILDERS_H_
