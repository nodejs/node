// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CONTROL_BUILDERS_H_
#define V8_COMPILER_CONTROL_BUILDERS_H_

#include "src/compiler/ast-graph-builder.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

// Base class for all control builders. Also provides a common interface for
// control builders to handle 'break' statements when they are used to model
// breakable statements.
class ControlBuilder {
 public:
  explicit ControlBuilder(AstGraphBuilder* builder) : builder_(builder) {}
  virtual ~ControlBuilder() {}

  // Interface for break.
  virtual void Break() { UNREACHABLE(); }

 protected:
  typedef AstGraphBuilder Builder;
  typedef AstGraphBuilder::Environment Environment;

  Zone* zone() const { return builder_->local_zone(); }
  Environment* environment() { return builder_->environment(); }
  void set_environment(Environment* env) { builder_->set_environment(env); }
  Node* the_hole() const { return builder_->jsgraph()->TheHoleConstant(); }

  Builder* builder_;
};


// Tracks control flow for a conditional statement.
class IfBuilder final : public ControlBuilder {
 public:
  explicit IfBuilder(AstGraphBuilder* builder)
      : ControlBuilder(builder),
        then_environment_(NULL),
        else_environment_(NULL) {}

  // Primitive control commands.
  void If(Node* condition, BranchHint hint = BranchHint::kNone);
  void Then();
  void Else();
  void End();

 private:
  Environment* then_environment_;  // Environment after the 'then' body.
  Environment* else_environment_;  // Environment for the 'else' body.
};


// Tracks control flow for an iteration statement.
class LoopBuilder final : public ControlBuilder {
 public:
  explicit LoopBuilder(AstGraphBuilder* builder)
      : ControlBuilder(builder),
        loop_environment_(NULL),
        continue_environment_(NULL),
        break_environment_(NULL) {}

  // Primitive control commands.
  void BeginLoop(BitVector* assigned, bool is_osr = false);
  void Continue();
  void EndBody();
  void EndLoop();

  // Primitive support for break.
  void Break() final;

  // Compound control commands for conditional break.
  void BreakUnless(Node* condition);
  void BreakWhen(Node* condition);

 private:
  Environment* loop_environment_;      // Environment of the loop header.
  Environment* continue_environment_;  // Environment after the loop body.
  Environment* break_environment_;     // Environment after the loop exits.
};


// Tracks control flow for a switch statement.
class SwitchBuilder final : public ControlBuilder {
 public:
  explicit SwitchBuilder(AstGraphBuilder* builder, int case_count)
      : ControlBuilder(builder),
        body_environment_(NULL),
        label_environment_(NULL),
        break_environment_(NULL),
        body_environments_(case_count, zone()) {}

  // Primitive control commands.
  void BeginSwitch();
  void BeginLabel(int index, Node* condition);
  void EndLabel();
  void DefaultAt(int index);
  void BeginCase(int index);
  void EndCase();
  void EndSwitch();

  // Primitive support for break.
  void Break() final;

  // The number of cases within a switch is statically known.
  size_t case_count() const { return body_environments_.size(); }

 private:
  Environment* body_environment_;   // Environment after last case body.
  Environment* label_environment_;  // Environment for next label condition.
  Environment* break_environment_;  // Environment after the switch exits.
  ZoneVector<Environment*> body_environments_;
};


// Tracks control flow for a block statement.
class BlockBuilder final : public ControlBuilder {
 public:
  explicit BlockBuilder(AstGraphBuilder* builder)
      : ControlBuilder(builder), break_environment_(NULL) {}

  // Primitive control commands.
  void BeginBlock();
  void EndBlock();

  // Primitive support for break.
  void Break() final;

  // Compound control commands for conditional break.
  void BreakWhen(Node* condition, BranchHint = BranchHint::kNone);

 private:
  Environment* break_environment_;  // Environment after the block exits.
};


// Tracks control flow for a try-catch statement.
class TryCatchBuilder final : public ControlBuilder {
 public:
  explicit TryCatchBuilder(AstGraphBuilder* builder)
      : ControlBuilder(builder),
        catch_environment_(NULL),
        exit_environment_(NULL),
        exception_node_(NULL) {}

  // Primitive control commands.
  void BeginTry();
  void Throw(Node* exception);
  void EndTry();
  void EndCatch();

  // Returns the exception value inside the 'catch' body.
  Node* GetExceptionNode() const { return exception_node_; }

 private:
  Environment* catch_environment_;  // Environment for the 'catch' body.
  Environment* exit_environment_;   // Environment after the statement.
  Node* exception_node_;            // Node for exception in 'catch' body.
};


// Tracks control flow for a try-finally statement.
class TryFinallyBuilder final : public ControlBuilder {
 public:
  explicit TryFinallyBuilder(AstGraphBuilder* builder)
      : ControlBuilder(builder),
        finally_environment_(NULL),
        token_node_(NULL),
        value_node_(NULL) {}

  // Primitive control commands.
  void BeginTry();
  void LeaveTry(Node* token, Node* value);
  void EndTry(Node* token, Node* value);
  void EndFinally();

  // Returns the dispatch token value inside the 'finally' body.
  Node* GetDispatchTokenNode() const { return token_node_; }

  // Returns the saved result value inside the 'finally' body.
  Node* GetResultValueNode() const { return value_node_; }

 private:
  Environment* finally_environment_;  // Environment for the 'finally' body.
  Node* token_node_;                  // Node for token in 'finally' body.
  Node* value_node_;                  // Node for value in 'finally' body.
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CONTROL_BUILDERS_H_
