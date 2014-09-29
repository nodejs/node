// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/structured-machine-assembler.h"

namespace v8 {
namespace internal {
namespace compiler {

Node* Variable::Get() const { return smasm_->GetVariable(offset_); }


void Variable::Set(Node* value) const { smasm_->SetVariable(offset_, value); }


StructuredMachineAssembler::StructuredMachineAssembler(
    Graph* graph, MachineCallDescriptorBuilder* call_descriptor_builder,
    MachineType word)
    : GraphBuilder(graph),
      schedule_(new (zone()) Schedule(zone())),
      machine_(zone(), word),
      common_(zone()),
      call_descriptor_builder_(call_descriptor_builder),
      parameters_(NULL),
      current_environment_(new (zone())
                           Environment(zone(), schedule()->entry(), false)),
      number_of_variables_(0) {
  Node* s = graph->NewNode(common_.Start(parameter_count()));
  graph->SetStart(s);
  if (parameter_count() == 0) return;
  parameters_ = zone()->NewArray<Node*>(parameter_count());
  for (int i = 0; i < parameter_count(); ++i) {
    parameters_[i] = NewNode(common()->Parameter(i), graph->start());
  }
}


Schedule* StructuredMachineAssembler::Export() {
  // Compute the correct codegen order.
  DCHECK(schedule_->rpo_order()->empty());
  Scheduler::ComputeSpecialRPO(schedule_);
  // Invalidate MachineAssembler.
  Schedule* schedule = schedule_;
  schedule_ = NULL;
  return schedule;
}


Node* StructuredMachineAssembler::Parameter(int index) {
  DCHECK(0 <= index && index < parameter_count());
  return parameters_[index];
}


Node* StructuredMachineAssembler::MakeNode(Operator* op, int input_count,
                                           Node** inputs) {
  DCHECK(ScheduleValid());
  DCHECK(current_environment_ != NULL);
  Node* node = graph()->NewNode(op, input_count, inputs);
  BasicBlock* block = NULL;
  switch (op->opcode()) {
    case IrOpcode::kParameter:
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kFloat64Constant:
    case IrOpcode::kExternalConstant:
    case IrOpcode::kNumberConstant:
    case IrOpcode::kHeapConstant:
      // Parameters and constants must be in start.
      block = schedule()->start();
      break;
    default:
      // Verify all leaf nodes handled above.
      DCHECK((op->OutputCount() == 0) == (op->opcode() == IrOpcode::kStore));
      block = current_environment_->block_;
      break;
  }
  if (block != NULL) {
    schedule()->AddNode(block, node);
  }
  return node;
}


Variable StructuredMachineAssembler::NewVariable(Node* initial_value) {
  CHECK(initial_value != NULL);
  int offset = number_of_variables_++;
  // Extend current environment to correct number of values.
  NodeVector* variables = CurrentVars();
  size_t to_add = number_of_variables_ - variables->size();
  if (to_add != 0) {
    variables->reserve(number_of_variables_);
    variables->insert(variables->end(), to_add, NULL);
  }
  variables->at(offset) = initial_value;
  return Variable(this, offset);
}


Node* StructuredMachineAssembler::GetVariable(int offset) {
  DCHECK(ScheduleValid());
  return VariableAt(current_environment_, offset);
}


void StructuredMachineAssembler::SetVariable(int offset, Node* value) {
  DCHECK(ScheduleValid());
  Node*& ref = VariableAt(current_environment_, offset);
  ref = value;
}


Node*& StructuredMachineAssembler::VariableAt(Environment* environment,
                                              int32_t offset) {
  // Variable used out of scope.
  CHECK(static_cast<size_t>(offset) < environment->variables_.size());
  Node*& value = environment->variables_.at(offset);
  CHECK(value != NULL);  // Variable used out of scope.
  return value;
}


void StructuredMachineAssembler::Return(Node* value) {
  BasicBlock* block = current_environment_->block_;
  if (block != NULL) {
    schedule()->AddReturn(block, value);
  }
  CopyCurrentAsDead();
}


void StructuredMachineAssembler::CopyCurrentAsDead() {
  DCHECK(current_environment_ != NULL);
  bool is_dead = current_environment_->is_dead_;
  current_environment_->is_dead_ = true;
  Environment* next = Copy(current_environment_);
  current_environment_->is_dead_ = is_dead;
  current_environment_ = next;
}


StructuredMachineAssembler::Environment* StructuredMachineAssembler::Copy(
    Environment* env, int truncate_at) {
  Environment* new_env = new (zone()) Environment(zone(), NULL, env->is_dead_);
  if (!new_env->is_dead_) {
    new_env->block_ = schedule()->NewBasicBlock();
  }
  new_env->variables_.reserve(truncate_at);
  NodeVectorIter end = env->variables_.end();
  DCHECK(truncate_at <= static_cast<int>(env->variables_.size()));
  end -= static_cast<int>(env->variables_.size()) - truncate_at;
  new_env->variables_.insert(new_env->variables_.begin(),
                             env->variables_.begin(), end);
  return new_env;
}


StructuredMachineAssembler::Environment*
StructuredMachineAssembler::CopyForLoopHeader(Environment* env) {
  Environment* new_env = new (zone()) Environment(zone(), NULL, env->is_dead_);
  if (!new_env->is_dead_) {
    new_env->block_ = schedule()->NewBasicBlock();
  }
  new_env->variables_.reserve(env->variables_.size());
  for (NodeVectorIter i = env->variables_.begin(); i != env->variables_.end();
       ++i) {
    Node* phi = NULL;
    if (*i != NULL) {
      phi = graph()->NewNode(common()->Phi(1), *i);
      if (new_env->block_ != NULL) {
        schedule()->AddNode(new_env->block_, phi);
      }
    }
    new_env->variables_.push_back(phi);
  }
  return new_env;
}


void StructuredMachineAssembler::MergeBackEdgesToLoopHeader(
    Environment* header, EnvironmentVector* environments) {
  // Only merge as many variables are were declared before this loop.
  int n = static_cast<int>(header->variables_.size());
  // TODO(dcarney): invert loop order and extend phis once.
  for (EnvironmentVector::iterator i = environments->begin();
       i != environments->end(); ++i) {
    Environment* from = *i;
    if (from->is_dead_) continue;
    AddGoto(from, header);
    for (int i = 0; i < n; ++i) {
      Node* phi = header->variables_[i];
      if (phi == NULL) continue;
      phi->set_op(common()->Phi(phi->InputCount() + 1));
      phi->AppendInput(zone(), VariableAt(from, i));
    }
  }
}


void StructuredMachineAssembler::Merge(EnvironmentVector* environments,
                                       int truncate_at) {
  DCHECK(current_environment_ == NULL || current_environment_->is_dead_);
  Environment* next = new (zone()) Environment(zone(), NULL, false);
  current_environment_ = next;
  size_t n_vars = number_of_variables_;
  NodeVector& vars = next->variables_;
  vars.reserve(n_vars);
  Node** scratch = NULL;
  size_t n_envs = environments->size();
  Environment** live_environments = reinterpret_cast<Environment**>(
      alloca(sizeof(environments->at(0)) * n_envs));
  size_t n_live = 0;
  for (size_t i = 0; i < n_envs; i++) {
    if (environments->at(i)->is_dead_) continue;
    live_environments[n_live++] = environments->at(i);
  }
  n_envs = n_live;
  if (n_live == 0) next->is_dead_ = true;
  if (!next->is_dead_) {
    next->block_ = schedule()->NewBasicBlock();
  }
  for (size_t j = 0; j < n_vars; ++j) {
    Node* resolved = NULL;
    // Find first non equal variable.
    size_t i = 0;
    for (; i < n_envs; i++) {
      DCHECK(live_environments[i]->variables_.size() <= n_vars);
      Node* val = NULL;
      if (j < static_cast<size_t>(truncate_at)) {
        val = live_environments[i]->variables_.at(j);
        // TODO(dcarney): record start position at time of split.
        //                all variables after this should not be NULL.
        if (val != NULL) {
          val = VariableAt(live_environments[i], static_cast<int>(j));
        }
      }
      if (val == resolved) continue;
      if (i != 0) break;
      resolved = val;
    }
    // Have to generate a phi.
    if (i < n_envs) {
      // All values thus far uninitialized, variable used out of scope.
      CHECK(resolved != NULL);
      // Init scratch buffer.
      if (scratch == NULL) {
        scratch = static_cast<Node**>(alloca(n_envs * sizeof(resolved)));
      }
      for (size_t k = 0; k < i; k++) {
        scratch[k] = resolved;
      }
      for (; i < n_envs; i++) {
        scratch[i] = live_environments[i]->variables_[j];
      }
      resolved = graph()->NewNode(common()->Phi(static_cast<int>(n_envs)),
                                  static_cast<int>(n_envs), scratch);
      if (next->block_ != NULL) {
        schedule()->AddNode(next->block_, resolved);
      }
    }
    vars.push_back(resolved);
  }
}


void StructuredMachineAssembler::AddGoto(Environment* from, Environment* to) {
  if (to->is_dead_) {
    DCHECK(from->is_dead_);
    return;
  }
  DCHECK(!from->is_dead_);
  schedule()->AddGoto(from->block_, to->block_);
}


// TODO(dcarney): add pass before rpo to schedule to compute these.
BasicBlock* StructuredMachineAssembler::TrampolineFor(BasicBlock* block) {
  BasicBlock* trampoline = schedule()->NewBasicBlock();
  schedule()->AddGoto(trampoline, block);
  return trampoline;
}


void StructuredMachineAssembler::AddBranch(Environment* environment,
                                           Node* condition,
                                           Environment* true_val,
                                           Environment* false_val) {
  DCHECK(environment->is_dead_ == true_val->is_dead_);
  DCHECK(environment->is_dead_ == false_val->is_dead_);
  if (true_val->block_ == false_val->block_) {
    if (environment->is_dead_) return;
    AddGoto(environment, true_val);
    return;
  }
  Node* branch = graph()->NewNode(common()->Branch(), condition);
  if (environment->is_dead_) return;
  BasicBlock* true_block = TrampolineFor(true_val->block_);
  BasicBlock* false_block = TrampolineFor(false_val->block_);
  schedule()->AddBranch(environment->block_, branch, true_block, false_block);
}


StructuredMachineAssembler::Environment::Environment(Zone* zone,
                                                     BasicBlock* block,
                                                     bool is_dead)
    : block_(block),
      variables_(NodeVector::allocator_type(zone)),
      is_dead_(is_dead) {}


StructuredMachineAssembler::IfBuilder::IfBuilder(
    StructuredMachineAssembler* smasm)
    : smasm_(smasm),
      if_clauses_(IfClauses::allocator_type(smasm_->zone())),
      pending_exit_merges_(EnvironmentVector::allocator_type(smasm_->zone())) {
  DCHECK(smasm_->current_environment_ != NULL);
  PushNewIfClause();
  DCHECK(!IsDone());
}


StructuredMachineAssembler::IfBuilder&
StructuredMachineAssembler::IfBuilder::If() {
  DCHECK(smasm_->current_environment_ != NULL);
  IfClause* clause = CurrentClause();
  if (clause->then_environment_ != NULL || clause->else_environment_ != NULL) {
    PushNewIfClause();
  }
  return *this;
}


StructuredMachineAssembler::IfBuilder&
StructuredMachineAssembler::IfBuilder::If(Node* condition) {
  If();
  IfClause* clause = CurrentClause();
  // Store branch for future resolution.
  UnresolvedBranch* next = new (smasm_->zone())
      UnresolvedBranch(smasm_->current_environment_, condition, NULL);
  if (clause->unresolved_list_tail_ != NULL) {
    clause->unresolved_list_tail_->next_ = next;
  }
  clause->unresolved_list_tail_ = next;
  // Push onto merge queues.
  clause->pending_else_merges_.push_back(next);
  clause->pending_then_merges_.push_back(next);
  smasm_->current_environment_ = NULL;
  return *this;
}


void StructuredMachineAssembler::IfBuilder::And() {
  CurrentClause()->ResolvePendingMerges(smasm_, kCombineThen, kExpressionTerm);
}


void StructuredMachineAssembler::IfBuilder::Or() {
  CurrentClause()->ResolvePendingMerges(smasm_, kCombineElse, kExpressionTerm);
}


void StructuredMachineAssembler::IfBuilder::Then() {
  CurrentClause()->ResolvePendingMerges(smasm_, kCombineThen, kExpressionDone);
}


void StructuredMachineAssembler::IfBuilder::Else() {
  AddCurrentToPending();
  CurrentClause()->ResolvePendingMerges(smasm_, kCombineElse, kExpressionDone);
}


void StructuredMachineAssembler::IfBuilder::AddCurrentToPending() {
  if (smasm_->current_environment_ != NULL &&
      !smasm_->current_environment_->is_dead_) {
    pending_exit_merges_.push_back(smasm_->current_environment_);
  }
  smasm_->current_environment_ = NULL;
}


void StructuredMachineAssembler::IfBuilder::PushNewIfClause() {
  int curr_size =
      static_cast<int>(smasm_->current_environment_->variables_.size());
  IfClause* clause = new (smasm_->zone()) IfClause(smasm_->zone(), curr_size);
  if_clauses_.push_back(clause);
}


StructuredMachineAssembler::IfBuilder::IfClause::IfClause(
    Zone* zone, int initial_environment_size)
    : unresolved_list_tail_(NULL),
      initial_environment_size_(initial_environment_size),
      expression_states_(ExpressionStates::allocator_type(zone)),
      pending_then_merges_(PendingMergeStack::allocator_type(zone)),
      pending_else_merges_(PendingMergeStack::allocator_type(zone)),
      then_environment_(NULL),
      else_environment_(NULL) {
  PushNewExpressionState();
}


StructuredMachineAssembler::IfBuilder::PendingMergeStackRange
StructuredMachineAssembler::IfBuilder::IfClause::ComputeRelevantMerges(
    CombineType combine_type) {
  DCHECK(!expression_states_.empty());
  PendingMergeStack* stack;
  int start;
  if (combine_type == kCombineThen) {
    stack = &pending_then_merges_;
    start = expression_states_.back().pending_then_size_;
  } else {
    DCHECK(combine_type == kCombineElse);
    stack = &pending_else_merges_;
    start = expression_states_.back().pending_else_size_;
  }
  PendingMergeStackRange data;
  data.merge_stack_ = stack;
  data.start_ = start;
  data.size_ = static_cast<int>(stack->size()) - start;
  return data;
}


void StructuredMachineAssembler::IfBuilder::IfClause::ResolvePendingMerges(
    StructuredMachineAssembler* smasm, CombineType combine_type,
    ResolutionType resolution_type) {
  DCHECK(smasm->current_environment_ == NULL);
  PendingMergeStackRange data = ComputeRelevantMerges(combine_type);
  DCHECK_EQ(data.merge_stack_->back(), unresolved_list_tail_);
  DCHECK(data.size_ > 0);
  // TODO(dcarney): assert no new variables created during expression building.
  int truncate_at = initial_environment_size_;
  if (data.size_ == 1) {
    // Just copy environment in common case.
    smasm->current_environment_ =
        smasm->Copy(unresolved_list_tail_->environment_, truncate_at);
  } else {
    EnvironmentVector environments(
        EnvironmentVector::allocator_type(smasm->zone()));
    environments.reserve(data.size_);
    CopyEnvironments(data, &environments);
    DCHECK(static_cast<int>(environments.size()) == data.size_);
    smasm->Merge(&environments, truncate_at);
  }
  Environment* then_environment = then_environment_;
  Environment* else_environment = NULL;
  if (resolution_type == kExpressionDone) {
    DCHECK(expression_states_.size() == 1);
    // Set the current then_ or else_environment_ to the new merged environment.
    if (combine_type == kCombineThen) {
      DCHECK(then_environment_ == NULL && else_environment_ == NULL);
      this->then_environment_ = smasm->current_environment_;
    } else {
      DCHECK(else_environment_ == NULL);
      this->else_environment_ = smasm->current_environment_;
    }
  } else {
    DCHECK(resolution_type == kExpressionTerm);
    DCHECK(then_environment_ == NULL && else_environment_ == NULL);
  }
  if (combine_type == kCombineThen) {
    then_environment = smasm->current_environment_;
  } else {
    DCHECK(combine_type == kCombineElse);
    else_environment = smasm->current_environment_;
  }
  // Finalize branches and clear the pending stack.
  FinalizeBranches(smasm, data, combine_type, then_environment,
                   else_environment);
}


void StructuredMachineAssembler::IfBuilder::IfClause::CopyEnvironments(
    const PendingMergeStackRange& data, EnvironmentVector* environments) {
  PendingMergeStack::iterator i = data.merge_stack_->begin();
  PendingMergeStack::iterator end = data.merge_stack_->end();
  for (i += data.start_; i != end; ++i) {
    environments->push_back((*i)->environment_);
  }
}


void StructuredMachineAssembler::IfBuilder::IfClause::PushNewExpressionState() {
  ExpressionState next;
  next.pending_then_size_ = static_cast<int>(pending_then_merges_.size());
  next.pending_else_size_ = static_cast<int>(pending_else_merges_.size());
  expression_states_.push_back(next);
}


void StructuredMachineAssembler::IfBuilder::IfClause::PopExpressionState() {
  expression_states_.pop_back();
  DCHECK(!expression_states_.empty());
}


void StructuredMachineAssembler::IfBuilder::IfClause::FinalizeBranches(
    StructuredMachineAssembler* smasm, const PendingMergeStackRange& data,
    CombineType combine_type, Environment* const then_environment,
    Environment* const else_environment) {
  DCHECK(unresolved_list_tail_ != NULL);
  DCHECK(smasm->current_environment_ != NULL);
  if (data.size_ == 0) return;
  PendingMergeStack::iterator curr = data.merge_stack_->begin();
  PendingMergeStack::iterator end = data.merge_stack_->end();
  // Finalize everything but the head first,
  // in the order the branches enter the merge block.
  end -= 1;
  Environment* true_val = then_environment;
  Environment* false_val = else_environment;
  Environment** next;
  if (combine_type == kCombineThen) {
    next = &false_val;
  } else {
    DCHECK(combine_type == kCombineElse);
    next = &true_val;
  }
  for (curr += data.start_; curr != end; ++curr) {
    UnresolvedBranch* branch = *curr;
    *next = branch->next_->environment_;
    smasm->AddBranch(branch->environment_, branch->condition_, true_val,
                     false_val);
  }
  DCHECK(curr + 1 == data.merge_stack_->end());
  // Now finalize the tail if possible.
  if (then_environment != NULL && else_environment != NULL) {
    UnresolvedBranch* branch = *curr;
    smasm->AddBranch(branch->environment_, branch->condition_, then_environment,
                     else_environment);
  }
  // Clear the merge stack.
  PendingMergeStack::iterator begin = data.merge_stack_->begin();
  begin += data.start_;
  data.merge_stack_->erase(begin, data.merge_stack_->end());
  DCHECK_EQ(static_cast<int>(data.merge_stack_->size()), data.start_);
}


void StructuredMachineAssembler::IfBuilder::End() {
  DCHECK(!IsDone());
  AddCurrentToPending();
  size_t current_pending = pending_exit_merges_.size();
  // All unresolved branch edges are now set to pending.
  for (IfClauses::iterator i = if_clauses_.begin(); i != if_clauses_.end();
       ++i) {
    IfClause* clause = *i;
    DCHECK(clause->expression_states_.size() == 1);
    PendingMergeStackRange data;
    // Copy then environments.
    data = clause->ComputeRelevantMerges(kCombineThen);
    clause->CopyEnvironments(data, &pending_exit_merges_);
    Environment* head = NULL;
    // Will resolve the head node in the else_merge
    if (data.size_ > 0 && clause->then_environment_ == NULL &&
        clause->else_environment_ == NULL) {
      head = pending_exit_merges_.back();
      pending_exit_merges_.pop_back();
    }
    // Copy else environments.
    data = clause->ComputeRelevantMerges(kCombineElse);
    clause->CopyEnvironments(data, &pending_exit_merges_);
    if (head != NULL) {
      // Must have data to merge, or else head will never get a branch.
      DCHECK(data.size_ != 0);
      pending_exit_merges_.push_back(head);
    }
  }
  smasm_->Merge(&pending_exit_merges_,
                if_clauses_[0]->initial_environment_size_);
  // Anything initally pending jumps into the new environment.
  for (size_t i = 0; i < current_pending; ++i) {
    smasm_->AddGoto(pending_exit_merges_[i], smasm_->current_environment_);
  }
  // Resolve all branches.
  for (IfClauses::iterator i = if_clauses_.begin(); i != if_clauses_.end();
       ++i) {
    IfClause* clause = *i;
    // Must finalize all environments, so ensure they are set correctly.
    Environment* then_environment = clause->then_environment_;
    if (then_environment == NULL) {
      then_environment = smasm_->current_environment_;
    }
    Environment* else_environment = clause->else_environment_;
    PendingMergeStackRange data;
    // Finalize then environments.
    data = clause->ComputeRelevantMerges(kCombineThen);
    clause->FinalizeBranches(smasm_, data, kCombineThen, then_environment,
                             else_environment);
    // Finalize else environments.
    // Now set the else environment so head is finalized for edge case above.
    if (else_environment == NULL) {
      else_environment = smasm_->current_environment_;
    }
    data = clause->ComputeRelevantMerges(kCombineElse);
    clause->FinalizeBranches(smasm_, data, kCombineElse, then_environment,
                             else_environment);
  }
  // Future accesses to this builder should crash immediately.
  pending_exit_merges_.clear();
  if_clauses_.clear();
  DCHECK(IsDone());
}


StructuredMachineAssembler::LoopBuilder::LoopBuilder(
    StructuredMachineAssembler* smasm)
    : smasm_(smasm),
      header_environment_(NULL),
      pending_header_merges_(EnvironmentVector::allocator_type(smasm_->zone())),
      pending_exit_merges_(EnvironmentVector::allocator_type(smasm_->zone())) {
  DCHECK(smasm_->current_environment_ != NULL);
  // Create header environment.
  header_environment_ = smasm_->CopyForLoopHeader(smasm_->current_environment_);
  smasm_->AddGoto(smasm_->current_environment_, header_environment_);
  // Create body environment.
  Environment* body = smasm_->Copy(header_environment_);
  smasm_->AddGoto(header_environment_, body);
  smasm_->current_environment_ = body;
  DCHECK(!IsDone());
}


void StructuredMachineAssembler::LoopBuilder::Continue() {
  DCHECK(!IsDone());
  pending_header_merges_.push_back(smasm_->current_environment_);
  smasm_->CopyCurrentAsDead();
}


void StructuredMachineAssembler::LoopBuilder::Break() {
  DCHECK(!IsDone());
  pending_exit_merges_.push_back(smasm_->current_environment_);
  smasm_->CopyCurrentAsDead();
}


void StructuredMachineAssembler::LoopBuilder::End() {
  DCHECK(!IsDone());
  if (smasm_->current_environment_ != NULL) {
    Continue();
  }
  // Do loop header merges.
  smasm_->MergeBackEdgesToLoopHeader(header_environment_,
                                     &pending_header_merges_);
  int initial_size = static_cast<int>(header_environment_->variables_.size());
  // Do loop exit merges, truncating loop variables away.
  smasm_->Merge(&pending_exit_merges_, initial_size);
  for (EnvironmentVector::iterator i = pending_exit_merges_.begin();
       i != pending_exit_merges_.end(); ++i) {
    smasm_->AddGoto(*i, smasm_->current_environment_);
  }
  pending_header_merges_.clear();
  pending_exit_merges_.clear();
  header_environment_ = NULL;
  DCHECK(IsDone());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
