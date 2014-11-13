// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CONTROL_BUILDERS_H_
#define V8_COMPILER_CONTROL_BUILDERS_H_

#include "src/v8.h"

#include "src/compiler/graph-builder.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

// Base class for all control builders. Also provides a common interface for
// control builders to handle 'break' and 'continue' statements when they are
// used to model breakable statements.
class ControlBuilder {
 public:
  explicit ControlBuilder(StructuredGraphBuilder* builder)
      : builder_(builder) {}
  virtual ~ControlBuilder() {}

  // Interface for break and continue.
  virtual void Break() { UNREACHABLE(); }
  virtual void Continue() { UNREACHABLE(); }

 protected:
  typedef StructuredGraphBuilder Builder;
  typedef StructuredGraphBuilder::Environment Environment;

  Zone* zone() const { return builder_->local_zone(); }
  Environment* environment() { return builder_->environment(); }
  void set_environment(Environment* env) { builder_->set_environment(env); }

  Builder* builder_;
};


// Tracks control flow for a conditional statement.
class IfBuilder : public ControlBuilder {
 public:
  explicit IfBuilder(StructuredGraphBuilder* builder)
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
class LoopBuilder : public ControlBuilder {
 public:
  explicit LoopBuilder(StructuredGraphBuilder* builder)
      : ControlBuilder(builder),
        loop_environment_(NULL),
        continue_environment_(NULL),
        break_environment_(NULL) {}

  // Primitive control commands.
  void BeginLoop(BitVector* assigned);
  void EndBody();
  void EndLoop();

  // Primitive support for break and continue.
  virtual void Continue();
  virtual void Break();

  // Compound control command for conditional break.
  void BreakUnless(Node* condition);

 private:
  Environment* loop_environment_;      // Environment of the loop header.
  Environment* continue_environment_;  // Environment after the loop body.
  Environment* break_environment_;     // Environment after the loop exits.
};


// Tracks control flow for a switch statement.
class SwitchBuilder : public ControlBuilder {
 public:
  explicit SwitchBuilder(StructuredGraphBuilder* builder, int case_count)
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
  virtual void Break();

  // The number of cases within a switch is statically known.
  int case_count() const { return body_environments_.capacity(); }

 private:
  Environment* body_environment_;   // Environment after last case body.
  Environment* label_environment_;  // Environment for next label condition.
  Environment* break_environment_;  // Environment after the switch exits.
  ZoneList<Environment*> body_environments_;
};


// Tracks control flow for a block statement.
class BlockBuilder : public ControlBuilder {
 public:
  explicit BlockBuilder(StructuredGraphBuilder* builder)
      : ControlBuilder(builder), break_environment_(NULL) {}

  // Primitive control commands.
  void BeginBlock();
  void EndBlock();

  // Primitive support for break.
  virtual void Break();

 private:
  Environment* break_environment_;  // Environment after the block exits.
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_CONTROL_BUILDERS_H_
