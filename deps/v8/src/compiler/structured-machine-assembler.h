// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_STRUCTURED_MACHINE_ASSEMBLER_H_
#define V8_COMPILER_STRUCTURED_MACHINE_ASSEMBLER_H_

#include "src/v8.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-builder.h"
#include "src/compiler/machine-node-factory.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"


namespace v8 {
namespace internal {
namespace compiler {

class BasicBlock;
class Schedule;
class StructuredMachineAssembler;


class Variable : public ZoneObject {
 public:
  Node* Get() const;
  void Set(Node* value) const;

 private:
  Variable(StructuredMachineAssembler* smasm, int offset)
      : smasm_(smasm), offset_(offset) {}

  friend class StructuredMachineAssembler;
  friend class StructuredMachineAssemblerFriend;
  StructuredMachineAssembler* const smasm_;
  const int offset_;
};


class StructuredMachineAssembler
    : public GraphBuilder,
      public MachineNodeFactory<StructuredMachineAssembler> {
 public:
  class Environment : public ZoneObject {
   public:
    Environment(Zone* zone, BasicBlock* block, bool is_dead_);

   private:
    BasicBlock* block_;
    NodeVector variables_;
    bool is_dead_;
    friend class StructuredMachineAssembler;
    DISALLOW_COPY_AND_ASSIGN(Environment);
  };

  class IfBuilder;
  friend class IfBuilder;
  class LoopBuilder;
  friend class LoopBuilder;

  StructuredMachineAssembler(
      Graph* graph, MachineCallDescriptorBuilder* call_descriptor_builder,
      MachineType word = MachineOperatorBuilder::pointer_rep());
  virtual ~StructuredMachineAssembler() {}

  Isolate* isolate() const { return zone()->isolate(); }
  Zone* zone() const { return graph()->zone(); }
  MachineOperatorBuilder* machine() { return &machine_; }
  CommonOperatorBuilder* common() { return &common_; }
  CallDescriptor* call_descriptor() const {
    return call_descriptor_builder_->BuildCallDescriptor(zone());
  }
  int parameter_count() const {
    return call_descriptor_builder_->parameter_count();
  }
  const MachineType* parameter_types() const {
    return call_descriptor_builder_->parameter_types();
  }

  // Parameters.
  Node* Parameter(int index);
  // Variables.
  Variable NewVariable(Node* initial_value);
  // Control flow.
  void Return(Node* value);

  // MachineAssembler is invalid after export.
  Schedule* Export();

 protected:
  virtual Node* MakeNode(Operator* op, int input_count, Node** inputs);

  Schedule* schedule() {
    DCHECK(ScheduleValid());
    return schedule_;
  }

 private:
  bool ScheduleValid() { return schedule_ != NULL; }

  typedef std::vector<Environment*, zone_allocator<Environment*> >
      EnvironmentVector;

  NodeVector* CurrentVars() { return &current_environment_->variables_; }
  Node*& VariableAt(Environment* environment, int offset);
  Node* GetVariable(int offset);
  void SetVariable(int offset, Node* value);

  void AddBranch(Environment* environment, Node* condition,
                 Environment* true_val, Environment* false_val);
  void AddGoto(Environment* from, Environment* to);
  BasicBlock* TrampolineFor(BasicBlock* block);

  void CopyCurrentAsDead();
  Environment* Copy(Environment* environment) {
    return Copy(environment, static_cast<int>(environment->variables_.size()));
  }
  Environment* Copy(Environment* environment, int truncate_at);
  void Merge(EnvironmentVector* environments, int truncate_at);
  Environment* CopyForLoopHeader(Environment* environment);
  void MergeBackEdgesToLoopHeader(Environment* header,
                                  EnvironmentVector* environments);

  typedef std::vector<MachineType, zone_allocator<MachineType> >
      RepresentationVector;

  Schedule* schedule_;
  MachineOperatorBuilder machine_;
  CommonOperatorBuilder common_;
  MachineCallDescriptorBuilder* call_descriptor_builder_;
  Node** parameters_;
  Environment* current_environment_;
  int number_of_variables_;

  friend class Variable;
  // For testing only.
  friend class StructuredMachineAssemblerFriend;
  DISALLOW_COPY_AND_ASSIGN(StructuredMachineAssembler);
};

// IfBuilder constructs of nested if-else expressions which more or less follow
// C semantics.  Foe example:
//
//  if (x) {do_x} else if (y) {do_y} else {do_z}
//
// would look like this:
//
//  IfBuilder b;
//  b.If(x).Then();
//  do_x
//  b.Else();
//  b.If().Then();
//  do_y
//  b.Else();
//  do_z
//  b.End();
//
// Then() and Else() can be skipped, representing an empty block in C.
// Combinations like If(x).Then().If(x).Then() are legitimate, but
// Else().Else() is not. That is, once you've nested an If(), you can't get to a
// higher level If() branch.
// TODO(dcarney): describe expressions once the api is finalized.
class StructuredMachineAssembler::IfBuilder {
 public:
  explicit IfBuilder(StructuredMachineAssembler* smasm);
  ~IfBuilder() {
    if (!IsDone()) End();
  }

  IfBuilder& If();  // TODO(dcarney): this should take an expression.
  IfBuilder& If(Node* condition);
  void Then();
  void Else();
  void End();

  // The next 4 functions are exposed for expression support.
  // They will be private once I have a nice expression api.
  void And();
  void Or();
  IfBuilder& OpenParen() {
    DCHECK(smasm_->current_environment_ != NULL);
    CurrentClause()->PushNewExpressionState();
    return *this;
  }
  IfBuilder& CloseParen() {
    DCHECK(smasm_->current_environment_ == NULL);
    CurrentClause()->PopExpressionState();
    return *this;
  }

 private:
  // UnresolvedBranch represents the chain of environments created while
  // generating an expression.  At this point, a branch Node
  // cannot be created, as the target environments of the branch are not yet
  // available, so everything required to create the branch Node is
  // stored in this structure until the target environments are resolved.
  struct UnresolvedBranch : public ZoneObject {
    UnresolvedBranch(Environment* environment, Node* condition,
                     UnresolvedBranch* next)
        : environment_(environment), condition_(condition), next_(next) {}
    // environment_ will eventually be terminated by a branch on condition_.
    Environment* environment_;
    Node* condition_;
    // next_ is the next link in the UnresolvedBranch chain, and will be
    // either the true or false branch jumped to from environment_.
    UnresolvedBranch* next_;
  };

  struct ExpressionState {
    int pending_then_size_;
    int pending_else_size_;
  };

  typedef std::vector<ExpressionState, zone_allocator<ExpressionState> >
      ExpressionStates;
  typedef std::vector<UnresolvedBranch*, zone_allocator<UnresolvedBranch*> >
      PendingMergeStack;
  struct IfClause;
  typedef std::vector<IfClause*, zone_allocator<IfClause*> > IfClauses;

  struct PendingMergeStackRange {
    PendingMergeStack* merge_stack_;
    int start_;
    int size_;
  };

  enum CombineType { kCombineThen, kCombineElse };
  enum ResolutionType { kExpressionTerm, kExpressionDone };

  // IfClause represents one level of if-then-else nesting plus the associated
  // expression.
  // A call to If() triggers creation of a new nesting level after expression
  // creation is complete - ie Then() or Else() has been called.
  struct IfClause : public ZoneObject {
    IfClause(Zone* zone, int initial_environment_size);
    void CopyEnvironments(const PendingMergeStackRange& data,
                          EnvironmentVector* environments);
    void ResolvePendingMerges(StructuredMachineAssembler* smasm,
                              CombineType combine_type,
                              ResolutionType resolution_type);
    PendingMergeStackRange ComputeRelevantMerges(CombineType combine_type);
    void FinalizeBranches(StructuredMachineAssembler* smasm,
                          const PendingMergeStackRange& offset_data,
                          CombineType combine_type,
                          Environment* then_environment,
                          Environment* else_environment);
    void PushNewExpressionState();
    void PopExpressionState();

    // Each invocation of And or Or creates a new UnresolvedBranch.
    // These form a singly-linked list, of which we only need to keep track of
    // the tail.  On creation of an UnresolvedBranch, pending_then_merges_ and
    // pending_else_merges_ each push a copy, which are removed on merges to the
    // respective environment.
    UnresolvedBranch* unresolved_list_tail_;
    int initial_environment_size_;
    // expression_states_ keeps track of the state of pending_*_merges_,
    // pushing and popping the lengths of these on
    // OpenParend() and CloseParend() respectively.
    ExpressionStates expression_states_;
    PendingMergeStack pending_then_merges_;
    PendingMergeStack pending_else_merges_;
    // then_environment_ is created iff there is a call to Then(), otherwise
    // branches which would merge to it merge to the exit environment instead.
    // Likewise for else_environment_.
    Environment* then_environment_;
    Environment* else_environment_;
  };

  IfClause* CurrentClause() { return if_clauses_.back(); }
  void AddCurrentToPending();
  void PushNewIfClause();
  bool IsDone() { return if_clauses_.empty(); }

  StructuredMachineAssembler* smasm_;
  IfClauses if_clauses_;
  EnvironmentVector pending_exit_merges_;
  DISALLOW_COPY_AND_ASSIGN(IfBuilder);
};


class StructuredMachineAssembler::LoopBuilder {
 public:
  explicit LoopBuilder(StructuredMachineAssembler* smasm);
  ~LoopBuilder() {
    if (!IsDone()) End();
  }

  void Break();
  void Continue();
  void End();

 private:
  friend class StructuredMachineAssembler;
  bool IsDone() { return header_environment_ == NULL; }

  StructuredMachineAssembler* smasm_;
  Environment* header_environment_;
  EnvironmentVector pending_header_merges_;
  EnvironmentVector pending_exit_merges_;
  DISALLOW_COPY_AND_ASSIGN(LoopBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_STRUCTURED_MACHINE_ASSEMBLER_H_
