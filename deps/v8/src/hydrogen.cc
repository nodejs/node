// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "hydrogen.h"

#include <algorithm>

#include "v8.h"
#include "allocation-site-scopes.h"
#include "codegen.h"
#include "full-codegen.h"
#include "hashmap.h"
#include "hydrogen-bce.h"
#include "hydrogen-bch.h"
#include "hydrogen-canonicalize.h"
#include "hydrogen-check-elimination.h"
#include "hydrogen-dce.h"
#include "hydrogen-dehoist.h"
#include "hydrogen-environment-liveness.h"
#include "hydrogen-escape-analysis.h"
#include "hydrogen-infer-representation.h"
#include "hydrogen-infer-types.h"
#include "hydrogen-load-elimination.h"
#include "hydrogen-gvn.h"
#include "hydrogen-mark-deoptimize.h"
#include "hydrogen-mark-unreachable.h"
#include "hydrogen-minus-zero.h"
#include "hydrogen-osr.h"
#include "hydrogen-range-analysis.h"
#include "hydrogen-redundant-phi.h"
#include "hydrogen-removable-simulates.h"
#include "hydrogen-representation-changes.h"
#include "hydrogen-sce.h"
#include "hydrogen-uint32-analysis.h"
#include "lithium-allocator.h"
#include "parser.h"
#include "runtime.h"
#include "scopeinfo.h"
#include "scopes.h"
#include "stub-cache.h"
#include "typing.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/lithium-codegen-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/lithium-codegen-x64.h"
#elif V8_TARGET_ARCH_A64
#include "a64/lithium-codegen-a64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/lithium-codegen-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/lithium-codegen-mips.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {

HBasicBlock::HBasicBlock(HGraph* graph)
    : block_id_(graph->GetNextBlockID()),
      graph_(graph),
      phis_(4, graph->zone()),
      first_(NULL),
      last_(NULL),
      end_(NULL),
      loop_information_(NULL),
      predecessors_(2, graph->zone()),
      dominator_(NULL),
      dominated_blocks_(4, graph->zone()),
      last_environment_(NULL),
      argument_count_(-1),
      first_instruction_index_(-1),
      last_instruction_index_(-1),
      deleted_phis_(4, graph->zone()),
      parent_loop_header_(NULL),
      inlined_entry_block_(NULL),
      is_inline_return_target_(false),
      is_reachable_(true),
      dominates_loop_successors_(false),
      is_osr_entry_(false) { }


Isolate* HBasicBlock::isolate() const {
  return graph_->isolate();
}


void HBasicBlock::MarkUnreachable() {
  is_reachable_ = false;
}


void HBasicBlock::AttachLoopInformation() {
  ASSERT(!IsLoopHeader());
  loop_information_ = new(zone()) HLoopInformation(this, zone());
}


void HBasicBlock::DetachLoopInformation() {
  ASSERT(IsLoopHeader());
  loop_information_ = NULL;
}


void HBasicBlock::AddPhi(HPhi* phi) {
  ASSERT(!IsStartBlock());
  phis_.Add(phi, zone());
  phi->SetBlock(this);
}


void HBasicBlock::RemovePhi(HPhi* phi) {
  ASSERT(phi->block() == this);
  ASSERT(phis_.Contains(phi));
  phi->Kill();
  phis_.RemoveElement(phi);
  phi->SetBlock(NULL);
}


void HBasicBlock::AddInstruction(HInstruction* instr,
                                 HSourcePosition position) {
  ASSERT(!IsStartBlock() || !IsFinished());
  ASSERT(!instr->IsLinked());
  ASSERT(!IsFinished());

  if (!position.IsUnknown()) {
    instr->set_position(position);
  }
  if (first_ == NULL) {
    ASSERT(last_environment() != NULL);
    ASSERT(!last_environment()->ast_id().IsNone());
    HBlockEntry* entry = new(zone()) HBlockEntry();
    entry->InitializeAsFirst(this);
    if (!position.IsUnknown()) {
      entry->set_position(position);
    } else {
      ASSERT(!FLAG_hydrogen_track_positions ||
             !graph()->info()->IsOptimizing());
    }
    first_ = last_ = entry;
  }
  instr->InsertAfter(last_);
}


HPhi* HBasicBlock::AddNewPhi(int merged_index) {
  if (graph()->IsInsideNoSideEffectsScope()) {
    merged_index = HPhi::kInvalidMergedIndex;
  }
  HPhi* phi = new(zone()) HPhi(merged_index, zone());
  AddPhi(phi);
  return phi;
}


HSimulate* HBasicBlock::CreateSimulate(BailoutId ast_id,
                                       RemovableSimulate removable) {
  ASSERT(HasEnvironment());
  HEnvironment* environment = last_environment();
  ASSERT(ast_id.IsNone() ||
         ast_id == BailoutId::StubEntry() ||
         environment->closure()->shared()->VerifyBailoutId(ast_id));

  int push_count = environment->push_count();
  int pop_count = environment->pop_count();

  HSimulate* instr =
      new(zone()) HSimulate(ast_id, pop_count, zone(), removable);
#ifdef DEBUG
  instr->set_closure(environment->closure());
#endif
  // Order of pushed values: newest (top of stack) first. This allows
  // HSimulate::MergeWith() to easily append additional pushed values
  // that are older (from further down the stack).
  for (int i = 0; i < push_count; ++i) {
    instr->AddPushedValue(environment->ExpressionStackAt(i));
  }
  for (GrowableBitVector::Iterator it(environment->assigned_variables(),
                                      zone());
       !it.Done();
       it.Advance()) {
    int index = it.Current();
    instr->AddAssignedValue(index, environment->Lookup(index));
  }
  environment->ClearHistory();
  return instr;
}


void HBasicBlock::Finish(HControlInstruction* end, HSourcePosition position) {
  ASSERT(!IsFinished());
  AddInstruction(end, position);
  end_ = end;
  for (HSuccessorIterator it(end); !it.Done(); it.Advance()) {
    it.Current()->RegisterPredecessor(this);
  }
}


void HBasicBlock::Goto(HBasicBlock* block,
                       HSourcePosition position,
                       FunctionState* state,
                       bool add_simulate) {
  bool drop_extra = state != NULL &&
      state->inlining_kind() == NORMAL_RETURN;

  if (block->IsInlineReturnTarget()) {
    HEnvironment* env = last_environment();
    int argument_count = env->arguments_environment()->parameter_count();
    AddInstruction(new(zone())
                   HLeaveInlined(state->entry(), argument_count),
                   position);
    UpdateEnvironment(last_environment()->DiscardInlined(drop_extra));
  }

  if (add_simulate) AddNewSimulate(BailoutId::None(), position);
  HGoto* instr = new(zone()) HGoto(block);
  Finish(instr, position);
}


void HBasicBlock::AddLeaveInlined(HValue* return_value,
                                  FunctionState* state,
                                  HSourcePosition position) {
  HBasicBlock* target = state->function_return();
  bool drop_extra = state->inlining_kind() == NORMAL_RETURN;

  ASSERT(target->IsInlineReturnTarget());
  ASSERT(return_value != NULL);
  HEnvironment* env = last_environment();
  int argument_count = env->arguments_environment()->parameter_count();
  AddInstruction(new(zone()) HLeaveInlined(state->entry(), argument_count),
                 position);
  UpdateEnvironment(last_environment()->DiscardInlined(drop_extra));
  last_environment()->Push(return_value);
  AddNewSimulate(BailoutId::None(), position);
  HGoto* instr = new(zone()) HGoto(target);
  Finish(instr, position);
}


void HBasicBlock::SetInitialEnvironment(HEnvironment* env) {
  ASSERT(!HasEnvironment());
  ASSERT(first() == NULL);
  UpdateEnvironment(env);
}


void HBasicBlock::UpdateEnvironment(HEnvironment* env) {
  last_environment_ = env;
  graph()->update_maximum_environment_size(env->first_expression_index());
}


void HBasicBlock::SetJoinId(BailoutId ast_id) {
  int length = predecessors_.length();
  ASSERT(length > 0);
  for (int i = 0; i < length; i++) {
    HBasicBlock* predecessor = predecessors_[i];
    ASSERT(predecessor->end()->IsGoto());
    HSimulate* simulate = HSimulate::cast(predecessor->end()->previous());
    ASSERT(i != 0 ||
           (predecessor->last_environment()->closure().is_null() ||
            predecessor->last_environment()->closure()->shared()
              ->VerifyBailoutId(ast_id)));
    simulate->set_ast_id(ast_id);
    predecessor->last_environment()->set_ast_id(ast_id);
  }
}


bool HBasicBlock::Dominates(HBasicBlock* other) const {
  HBasicBlock* current = other->dominator();
  while (current != NULL) {
    if (current == this) return true;
    current = current->dominator();
  }
  return false;
}


bool HBasicBlock::EqualToOrDominates(HBasicBlock* other) const {
  if (this == other) return true;
  return Dominates(other);
}


int HBasicBlock::LoopNestingDepth() const {
  const HBasicBlock* current = this;
  int result  = (current->IsLoopHeader()) ? 1 : 0;
  while (current->parent_loop_header() != NULL) {
    current = current->parent_loop_header();
    result++;
  }
  return result;
}


void HBasicBlock::PostProcessLoopHeader(IterationStatement* stmt) {
  ASSERT(IsLoopHeader());

  SetJoinId(stmt->EntryId());
  if (predecessors()->length() == 1) {
    // This is a degenerated loop.
    DetachLoopInformation();
    return;
  }

  // Only the first entry into the loop is from outside the loop. All other
  // entries must be back edges.
  for (int i = 1; i < predecessors()->length(); ++i) {
    loop_information()->RegisterBackEdge(predecessors()->at(i));
  }
}


void HBasicBlock::MarkSuccEdgeUnreachable(int succ) {
  ASSERT(IsFinished());
  HBasicBlock* succ_block = end()->SuccessorAt(succ);

  ASSERT(succ_block->predecessors()->length() == 1);
  succ_block->MarkUnreachable();
}


void HBasicBlock::RegisterPredecessor(HBasicBlock* pred) {
  if (HasPredecessor()) {
    // Only loop header blocks can have a predecessor added after
    // instructions have been added to the block (they have phis for all
    // values in the environment, these phis may be eliminated later).
    ASSERT(IsLoopHeader() || first_ == NULL);
    HEnvironment* incoming_env = pred->last_environment();
    if (IsLoopHeader()) {
      ASSERT(phis()->length() == incoming_env->length());
      for (int i = 0; i < phis_.length(); ++i) {
        phis_[i]->AddInput(incoming_env->values()->at(i));
      }
    } else {
      last_environment()->AddIncomingEdge(this, pred->last_environment());
    }
  } else if (!HasEnvironment() && !IsFinished()) {
    ASSERT(!IsLoopHeader());
    SetInitialEnvironment(pred->last_environment()->Copy());
  }

  predecessors_.Add(pred, zone());
}


void HBasicBlock::AddDominatedBlock(HBasicBlock* block) {
  ASSERT(!dominated_blocks_.Contains(block));
  // Keep the list of dominated blocks sorted such that if there is two
  // succeeding block in this list, the predecessor is before the successor.
  int index = 0;
  while (index < dominated_blocks_.length() &&
         dominated_blocks_[index]->block_id() < block->block_id()) {
    ++index;
  }
  dominated_blocks_.InsertAt(index, block, zone());
}


void HBasicBlock::AssignCommonDominator(HBasicBlock* other) {
  if (dominator_ == NULL) {
    dominator_ = other;
    other->AddDominatedBlock(this);
  } else if (other->dominator() != NULL) {
    HBasicBlock* first = dominator_;
    HBasicBlock* second = other;

    while (first != second) {
      if (first->block_id() > second->block_id()) {
        first = first->dominator();
      } else {
        second = second->dominator();
      }
      ASSERT(first != NULL && second != NULL);
    }

    if (dominator_ != first) {
      ASSERT(dominator_->dominated_blocks_.Contains(this));
      dominator_->dominated_blocks_.RemoveElement(this);
      dominator_ = first;
      first->AddDominatedBlock(this);
    }
  }
}


void HBasicBlock::AssignLoopSuccessorDominators() {
  // Mark blocks that dominate all subsequent reachable blocks inside their
  // loop. Exploit the fact that blocks are sorted in reverse post order. When
  // the loop is visited in increasing block id order, if the number of
  // non-loop-exiting successor edges at the dominator_candidate block doesn't
  // exceed the number of previously encountered predecessor edges, there is no
  // path from the loop header to any block with higher id that doesn't go
  // through the dominator_candidate block. In this case, the
  // dominator_candidate block is guaranteed to dominate all blocks reachable
  // from it with higher ids.
  HBasicBlock* last = loop_information()->GetLastBackEdge();
  int outstanding_successors = 1;  // one edge from the pre-header
  // Header always dominates everything.
  MarkAsLoopSuccessorDominator();
  for (int j = block_id(); j <= last->block_id(); ++j) {
    HBasicBlock* dominator_candidate = graph_->blocks()->at(j);
    for (HPredecessorIterator it(dominator_candidate); !it.Done();
         it.Advance()) {
      HBasicBlock* predecessor = it.Current();
      // Don't count back edges.
      if (predecessor->block_id() < dominator_candidate->block_id()) {
        outstanding_successors--;
      }
    }

    // If more successors than predecessors have been seen in the loop up to
    // now, it's not possible to guarantee that the current block dominates
    // all of the blocks with higher IDs. In this case, assume conservatively
    // that those paths through loop that don't go through the current block
    // contain all of the loop's dependencies. Also be careful to record
    // dominator information about the current loop that's being processed,
    // and not nested loops, which will be processed when
    // AssignLoopSuccessorDominators gets called on their header.
    ASSERT(outstanding_successors >= 0);
    HBasicBlock* parent_loop_header = dominator_candidate->parent_loop_header();
    if (outstanding_successors == 0 &&
        (parent_loop_header == this && !dominator_candidate->IsLoopHeader())) {
      dominator_candidate->MarkAsLoopSuccessorDominator();
    }
    HControlInstruction* end = dominator_candidate->end();
    for (HSuccessorIterator it(end); !it.Done(); it.Advance()) {
      HBasicBlock* successor = it.Current();
      // Only count successors that remain inside the loop and don't loop back
      // to a loop header.
      if (successor->block_id() > dominator_candidate->block_id() &&
          successor->block_id() <= last->block_id()) {
        // Backwards edges must land on loop headers.
        ASSERT(successor->block_id() > dominator_candidate->block_id() ||
               successor->IsLoopHeader());
        outstanding_successors++;
      }
    }
  }
}


int HBasicBlock::PredecessorIndexOf(HBasicBlock* predecessor) const {
  for (int i = 0; i < predecessors_.length(); ++i) {
    if (predecessors_[i] == predecessor) return i;
  }
  UNREACHABLE();
  return -1;
}


#ifdef DEBUG
void HBasicBlock::Verify() {
  // Check that every block is finished.
  ASSERT(IsFinished());
  ASSERT(block_id() >= 0);

  // Check that the incoming edges are in edge split form.
  if (predecessors_.length() > 1) {
    for (int i = 0; i < predecessors_.length(); ++i) {
      ASSERT(predecessors_[i]->end()->SecondSuccessor() == NULL);
    }
  }
}
#endif


void HLoopInformation::RegisterBackEdge(HBasicBlock* block) {
  this->back_edges_.Add(block, block->zone());
  AddBlock(block);
}


HBasicBlock* HLoopInformation::GetLastBackEdge() const {
  int max_id = -1;
  HBasicBlock* result = NULL;
  for (int i = 0; i < back_edges_.length(); ++i) {
    HBasicBlock* cur = back_edges_[i];
    if (cur->block_id() > max_id) {
      max_id = cur->block_id();
      result = cur;
    }
  }
  return result;
}


void HLoopInformation::AddBlock(HBasicBlock* block) {
  if (block == loop_header()) return;
  if (block->parent_loop_header() == loop_header()) return;
  if (block->parent_loop_header() != NULL) {
    AddBlock(block->parent_loop_header());
  } else {
    block->set_parent_loop_header(loop_header());
    blocks_.Add(block, block->zone());
    for (int i = 0; i < block->predecessors()->length(); ++i) {
      AddBlock(block->predecessors()->at(i));
    }
  }
}


#ifdef DEBUG

// Checks reachability of the blocks in this graph and stores a bit in
// the BitVector "reachable()" for every block that can be reached
// from the start block of the graph. If "dont_visit" is non-null, the given
// block is treated as if it would not be part of the graph. "visited_count()"
// returns the number of reachable blocks.
class ReachabilityAnalyzer BASE_EMBEDDED {
 public:
  ReachabilityAnalyzer(HBasicBlock* entry_block,
                       int block_count,
                       HBasicBlock* dont_visit)
      : visited_count_(0),
        stack_(16, entry_block->zone()),
        reachable_(block_count, entry_block->zone()),
        dont_visit_(dont_visit) {
    PushBlock(entry_block);
    Analyze();
  }

  int visited_count() const { return visited_count_; }
  const BitVector* reachable() const { return &reachable_; }

 private:
  void PushBlock(HBasicBlock* block) {
    if (block != NULL && block != dont_visit_ &&
        !reachable_.Contains(block->block_id())) {
      reachable_.Add(block->block_id());
      stack_.Add(block, block->zone());
      visited_count_++;
    }
  }

  void Analyze() {
    while (!stack_.is_empty()) {
      HControlInstruction* end = stack_.RemoveLast()->end();
      for (HSuccessorIterator it(end); !it.Done(); it.Advance()) {
        PushBlock(it.Current());
      }
    }
  }

  int visited_count_;
  ZoneList<HBasicBlock*> stack_;
  BitVector reachable_;
  HBasicBlock* dont_visit_;
};


void HGraph::Verify(bool do_full_verify) const {
  Heap::RelocationLock relocation_lock(isolate()->heap());
  AllowHandleDereference allow_deref;
  AllowDeferredHandleDereference allow_deferred_deref;
  for (int i = 0; i < blocks_.length(); i++) {
    HBasicBlock* block = blocks_.at(i);

    block->Verify();

    // Check that every block contains at least one node and that only the last
    // node is a control instruction.
    HInstruction* current = block->first();
    ASSERT(current != NULL && current->IsBlockEntry());
    while (current != NULL) {
      ASSERT((current->next() == NULL) == current->IsControlInstruction());
      ASSERT(current->block() == block);
      current->Verify();
      current = current->next();
    }

    // Check that successors are correctly set.
    HBasicBlock* first = block->end()->FirstSuccessor();
    HBasicBlock* second = block->end()->SecondSuccessor();
    ASSERT(second == NULL || first != NULL);

    // Check that the predecessor array is correct.
    if (first != NULL) {
      ASSERT(first->predecessors()->Contains(block));
      if (second != NULL) {
        ASSERT(second->predecessors()->Contains(block));
      }
    }

    // Check that phis have correct arguments.
    for (int j = 0; j < block->phis()->length(); j++) {
      HPhi* phi = block->phis()->at(j);
      phi->Verify();
    }

    // Check that all join blocks have predecessors that end with an
    // unconditional goto and agree on their environment node id.
    if (block->predecessors()->length() >= 2) {
      BailoutId id =
          block->predecessors()->first()->last_environment()->ast_id();
      for (int k = 0; k < block->predecessors()->length(); k++) {
        HBasicBlock* predecessor = block->predecessors()->at(k);
        ASSERT(predecessor->end()->IsGoto() ||
               predecessor->end()->IsDeoptimize());
        ASSERT(predecessor->last_environment()->ast_id() == id);
      }
    }
  }

  // Check special property of first block to have no predecessors.
  ASSERT(blocks_.at(0)->predecessors()->is_empty());

  if (do_full_verify) {
    // Check that the graph is fully connected.
    ReachabilityAnalyzer analyzer(entry_block_, blocks_.length(), NULL);
    ASSERT(analyzer.visited_count() == blocks_.length());

    // Check that entry block dominator is NULL.
    ASSERT(entry_block_->dominator() == NULL);

    // Check dominators.
    for (int i = 0; i < blocks_.length(); ++i) {
      HBasicBlock* block = blocks_.at(i);
      if (block->dominator() == NULL) {
        // Only start block may have no dominator assigned to.
        ASSERT(i == 0);
      } else {
        // Assert that block is unreachable if dominator must not be visited.
        ReachabilityAnalyzer dominator_analyzer(entry_block_,
                                                blocks_.length(),
                                                block->dominator());
        ASSERT(!dominator_analyzer.reachable()->Contains(block->block_id()));
      }
    }
  }
}

#endif


HConstant* HGraph::GetConstant(SetOncePointer<HConstant>* pointer,
                               int32_t value) {
  if (!pointer->is_set()) {
    // Can't pass GetInvalidContext() to HConstant::New, because that will
    // recursively call GetConstant
    HConstant* constant = HConstant::New(zone(), NULL, value);
    constant->InsertAfter(entry_block()->first());
    pointer->set(constant);
    return constant;
  }
  return ReinsertConstantIfNecessary(pointer->get());
}


HConstant* HGraph::ReinsertConstantIfNecessary(HConstant* constant) {
  if (!constant->IsLinked()) {
    // The constant was removed from the graph. Reinsert.
    constant->ClearFlag(HValue::kIsDead);
    constant->InsertAfter(entry_block()->first());
  }
  return constant;
}


HConstant* HGraph::GetConstant0() {
  return GetConstant(&constant_0_, 0);
}


HConstant* HGraph::GetConstant1() {
  return GetConstant(&constant_1_, 1);
}


HConstant* HGraph::GetConstantMinus1() {
  return GetConstant(&constant_minus1_, -1);
}


#define DEFINE_GET_CONSTANT(Name, name, htype, boolean_value)                  \
HConstant* HGraph::GetConstant##Name() {                                       \
  if (!constant_##name##_.is_set()) {                                          \
    HConstant* constant = new(zone()) HConstant(                               \
        Unique<Object>::CreateImmovable(isolate()->factory()->name##_value()), \
        Representation::Tagged(),                                              \
        htype,                                                                 \
        false,                                                                 \
        true,                                                                  \
        false,                                                                 \
        boolean_value);                                                        \
    constant->InsertAfter(entry_block()->first());                             \
    constant_##name##_.set(constant);                                          \
  }                                                                            \
  return ReinsertConstantIfNecessary(constant_##name##_.get());                \
}


DEFINE_GET_CONSTANT(Undefined, undefined, HType::Tagged(), false)
DEFINE_GET_CONSTANT(True, true, HType::Boolean(), true)
DEFINE_GET_CONSTANT(False, false, HType::Boolean(), false)
DEFINE_GET_CONSTANT(Hole, the_hole, HType::Tagged(), false)
DEFINE_GET_CONSTANT(Null, null, HType::Tagged(), false)


#undef DEFINE_GET_CONSTANT

#define DEFINE_IS_CONSTANT(Name, name)                                         \
bool HGraph::IsConstant##Name(HConstant* constant) {                           \
  return constant_##name##_.is_set() && constant == constant_##name##_.get();  \
}
DEFINE_IS_CONSTANT(Undefined, undefined)
DEFINE_IS_CONSTANT(0, 0)
DEFINE_IS_CONSTANT(1, 1)
DEFINE_IS_CONSTANT(Minus1, minus1)
DEFINE_IS_CONSTANT(True, true)
DEFINE_IS_CONSTANT(False, false)
DEFINE_IS_CONSTANT(Hole, the_hole)
DEFINE_IS_CONSTANT(Null, null)

#undef DEFINE_IS_CONSTANT


HConstant* HGraph::GetInvalidContext() {
  return GetConstant(&constant_invalid_context_, 0xFFFFC0C7);
}


bool HGraph::IsStandardConstant(HConstant* constant) {
  if (IsConstantUndefined(constant)) return true;
  if (IsConstant0(constant)) return true;
  if (IsConstant1(constant)) return true;
  if (IsConstantMinus1(constant)) return true;
  if (IsConstantTrue(constant)) return true;
  if (IsConstantFalse(constant)) return true;
  if (IsConstantHole(constant)) return true;
  if (IsConstantNull(constant)) return true;
  return false;
}


HGraphBuilder::IfBuilder::IfBuilder(HGraphBuilder* builder)
    : builder_(builder),
      finished_(false),
      did_then_(false),
      did_else_(false),
      did_else_if_(false),
      did_and_(false),
      did_or_(false),
      captured_(false),
      needs_compare_(true),
      pending_merge_block_(false),
      split_edge_merge_block_(NULL),
      merge_at_join_blocks_(NULL),
      normal_merge_at_join_block_count_(0),
      deopt_merge_at_join_block_count_(0) {
  HEnvironment* env = builder->environment();
  first_true_block_ = builder->CreateBasicBlock(env->Copy());
  first_false_block_ = builder->CreateBasicBlock(env->Copy());
}


HGraphBuilder::IfBuilder::IfBuilder(
    HGraphBuilder* builder,
    HIfContinuation* continuation)
    : builder_(builder),
      finished_(false),
      did_then_(false),
      did_else_(false),
      did_else_if_(false),
      did_and_(false),
      did_or_(false),
      captured_(false),
      needs_compare_(false),
      pending_merge_block_(false),
      first_true_block_(NULL),
      first_false_block_(NULL),
      split_edge_merge_block_(NULL),
      merge_at_join_blocks_(NULL),
      normal_merge_at_join_block_count_(0),
      deopt_merge_at_join_block_count_(0) {
  continuation->Continue(&first_true_block_,
                         &first_false_block_);
}


HControlInstruction* HGraphBuilder::IfBuilder::AddCompare(
    HControlInstruction* compare) {
  ASSERT(did_then_ == did_else_);
  if (did_else_) {
    // Handle if-then-elseif
    did_else_if_ = true;
    did_else_ = false;
    did_then_ = false;
    did_and_ = false;
    did_or_ = false;
    pending_merge_block_ = false;
    split_edge_merge_block_ = NULL;
    HEnvironment* env = builder_->environment();
    first_true_block_ = builder_->CreateBasicBlock(env->Copy());
    first_false_block_ = builder_->CreateBasicBlock(env->Copy());
  }
  if (split_edge_merge_block_ != NULL) {
    HEnvironment* env = first_false_block_->last_environment();
    HBasicBlock* split_edge =
        builder_->CreateBasicBlock(env->Copy());
    if (did_or_) {
      compare->SetSuccessorAt(0, split_edge);
      compare->SetSuccessorAt(1, first_false_block_);
    } else {
      compare->SetSuccessorAt(0, first_true_block_);
      compare->SetSuccessorAt(1, split_edge);
    }
    builder_->GotoNoSimulate(split_edge, split_edge_merge_block_);
  } else {
    compare->SetSuccessorAt(0, first_true_block_);
    compare->SetSuccessorAt(1, first_false_block_);
  }
  builder_->FinishCurrentBlock(compare);
  needs_compare_ = false;
  return compare;
}


void HGraphBuilder::IfBuilder::Or() {
  ASSERT(!needs_compare_);
  ASSERT(!did_and_);
  did_or_ = true;
  HEnvironment* env = first_false_block_->last_environment();
  if (split_edge_merge_block_ == NULL) {
    split_edge_merge_block_ =
        builder_->CreateBasicBlock(env->Copy());
    builder_->GotoNoSimulate(first_true_block_, split_edge_merge_block_);
    first_true_block_ = split_edge_merge_block_;
  }
  builder_->set_current_block(first_false_block_);
  first_false_block_ = builder_->CreateBasicBlock(env->Copy());
}


void HGraphBuilder::IfBuilder::And() {
  ASSERT(!needs_compare_);
  ASSERT(!did_or_);
  did_and_ = true;
  HEnvironment* env = first_false_block_->last_environment();
  if (split_edge_merge_block_ == NULL) {
    split_edge_merge_block_ = builder_->CreateBasicBlock(env->Copy());
    builder_->GotoNoSimulate(first_false_block_, split_edge_merge_block_);
    first_false_block_ = split_edge_merge_block_;
  }
  builder_->set_current_block(first_true_block_);
  first_true_block_ = builder_->CreateBasicBlock(env->Copy());
}


void HGraphBuilder::IfBuilder::CaptureContinuation(
    HIfContinuation* continuation) {
  ASSERT(!did_else_if_);
  ASSERT(!finished_);
  ASSERT(!captured_);

  HBasicBlock* true_block = NULL;
  HBasicBlock* false_block = NULL;
  Finish(&true_block, &false_block);
  ASSERT(true_block != NULL);
  ASSERT(false_block != NULL);
  continuation->Capture(true_block, false_block);
  captured_ = true;
  builder_->set_current_block(NULL);
  End();
}


void HGraphBuilder::IfBuilder::JoinContinuation(HIfContinuation* continuation) {
  ASSERT(!did_else_if_);
  ASSERT(!finished_);
  ASSERT(!captured_);
  HBasicBlock* true_block = NULL;
  HBasicBlock* false_block = NULL;
  Finish(&true_block, &false_block);
  merge_at_join_blocks_ = NULL;
  if (true_block != NULL && !true_block->IsFinished()) {
    ASSERT(continuation->IsTrueReachable());
    builder_->GotoNoSimulate(true_block, continuation->true_branch());
  }
  if (false_block != NULL && !false_block->IsFinished()) {
    ASSERT(continuation->IsFalseReachable());
    builder_->GotoNoSimulate(false_block, continuation->false_branch());
  }
  captured_ = true;
  End();
}


void HGraphBuilder::IfBuilder::Then() {
  ASSERT(!captured_);
  ASSERT(!finished_);
  did_then_ = true;
  if (needs_compare_) {
    // Handle if's without any expressions, they jump directly to the "else"
    // branch. However, we must pretend that the "then" branch is reachable,
    // so that the graph builder visits it and sees any live range extending
    // constructs within it.
    HConstant* constant_false = builder_->graph()->GetConstantFalse();
    ToBooleanStub::Types boolean_type = ToBooleanStub::Types();
    boolean_type.Add(ToBooleanStub::BOOLEAN);
    HBranch* branch = builder()->New<HBranch>(
        constant_false, boolean_type, first_true_block_, first_false_block_);
    builder_->FinishCurrentBlock(branch);
  }
  builder_->set_current_block(first_true_block_);
  pending_merge_block_ = true;
}


void HGraphBuilder::IfBuilder::Else() {
  ASSERT(did_then_);
  ASSERT(!captured_);
  ASSERT(!finished_);
  AddMergeAtJoinBlock(false);
  builder_->set_current_block(first_false_block_);
  pending_merge_block_ = true;
  did_else_ = true;
}


void HGraphBuilder::IfBuilder::Deopt(const char* reason) {
  ASSERT(did_then_);
  builder_->Add<HDeoptimize>(reason, Deoptimizer::EAGER);
  AddMergeAtJoinBlock(true);
}


void HGraphBuilder::IfBuilder::Return(HValue* value) {
  HValue* parameter_count = builder_->graph()->GetConstantMinus1();
  builder_->FinishExitCurrentBlock(
      builder_->New<HReturn>(value, parameter_count));
  AddMergeAtJoinBlock(false);
}


void HGraphBuilder::IfBuilder::AddMergeAtJoinBlock(bool deopt) {
  if (!pending_merge_block_) return;
  HBasicBlock* block = builder_->current_block();
  ASSERT(block == NULL || !block->IsFinished());
  MergeAtJoinBlock* record =
      new(builder_->zone()) MergeAtJoinBlock(block, deopt,
                                             merge_at_join_blocks_);
  merge_at_join_blocks_ = record;
  if (block != NULL) {
    ASSERT(block->end() == NULL);
    if (deopt) {
      normal_merge_at_join_block_count_++;
    } else {
      deopt_merge_at_join_block_count_++;
    }
  }
  builder_->set_current_block(NULL);
  pending_merge_block_ = false;
}


void HGraphBuilder::IfBuilder::Finish() {
  ASSERT(!finished_);
  if (!did_then_) {
    Then();
  }
  AddMergeAtJoinBlock(false);
  if (!did_else_) {
    Else();
    AddMergeAtJoinBlock(false);
  }
  finished_ = true;
}


void HGraphBuilder::IfBuilder::Finish(HBasicBlock** then_continuation,
                                      HBasicBlock** else_continuation) {
  Finish();

  MergeAtJoinBlock* else_record = merge_at_join_blocks_;
  if (else_continuation != NULL) {
    *else_continuation = else_record->block_;
  }
  MergeAtJoinBlock* then_record = else_record->next_;
  if (then_continuation != NULL) {
    *then_continuation = then_record->block_;
  }
  ASSERT(then_record->next_ == NULL);
}


void HGraphBuilder::IfBuilder::End() {
  if (captured_) return;
  Finish();

  int total_merged_blocks = normal_merge_at_join_block_count_ +
    deopt_merge_at_join_block_count_;
  ASSERT(total_merged_blocks >= 1);
  HBasicBlock* merge_block = total_merged_blocks == 1
      ? NULL : builder_->graph()->CreateBasicBlock();

  // Merge non-deopt blocks first to ensure environment has right size for
  // padding.
  MergeAtJoinBlock* current = merge_at_join_blocks_;
  while (current != NULL) {
    if (!current->deopt_ && current->block_ != NULL) {
      // If there is only one block that makes it through to the end of the
      // if, then just set it as the current block and continue rather then
      // creating an unnecessary merge block.
      if (total_merged_blocks == 1) {
        builder_->set_current_block(current->block_);
        return;
      }
      builder_->GotoNoSimulate(current->block_, merge_block);
    }
    current = current->next_;
  }

  // Merge deopt blocks, padding when necessary.
  current = merge_at_join_blocks_;
  while (current != NULL) {
    if (current->deopt_ && current->block_ != NULL) {
      current->block_->FinishExit(
          HAbnormalExit::New(builder_->zone(), NULL),
          HSourcePosition::Unknown());
    }
    current = current->next_;
  }
  builder_->set_current_block(merge_block);
}


HGraphBuilder::LoopBuilder::LoopBuilder(HGraphBuilder* builder,
                                        HValue* context,
                                        LoopBuilder::Direction direction)
    : builder_(builder),
      context_(context),
      direction_(direction),
      finished_(false) {
  header_block_ = builder->CreateLoopHeaderBlock();
  body_block_ = NULL;
  exit_block_ = NULL;
  exit_trampoline_block_ = NULL;
  increment_amount_ = builder_->graph()->GetConstant1();
}


HGraphBuilder::LoopBuilder::LoopBuilder(HGraphBuilder* builder,
                                        HValue* context,
                                        LoopBuilder::Direction direction,
                                        HValue* increment_amount)
    : builder_(builder),
      context_(context),
      direction_(direction),
      finished_(false) {
  header_block_ = builder->CreateLoopHeaderBlock();
  body_block_ = NULL;
  exit_block_ = NULL;
  exit_trampoline_block_ = NULL;
  increment_amount_ = increment_amount;
}


HValue* HGraphBuilder::LoopBuilder::BeginBody(
    HValue* initial,
    HValue* terminating,
    Token::Value token) {
  HEnvironment* env = builder_->environment();
  phi_ = header_block_->AddNewPhi(env->values()->length());
  phi_->AddInput(initial);
  env->Push(initial);
  builder_->GotoNoSimulate(header_block_);

  HEnvironment* body_env = env->Copy();
  HEnvironment* exit_env = env->Copy();
  // Remove the phi from the expression stack
  body_env->Pop();
  exit_env->Pop();
  body_block_ = builder_->CreateBasicBlock(body_env);
  exit_block_ = builder_->CreateBasicBlock(exit_env);

  builder_->set_current_block(header_block_);
  env->Pop();
  builder_->FinishCurrentBlock(builder_->New<HCompareNumericAndBranch>(
          phi_, terminating, token, body_block_, exit_block_));

  builder_->set_current_block(body_block_);
  if (direction_ == kPreIncrement || direction_ == kPreDecrement) {
    HValue* one = builder_->graph()->GetConstant1();
    if (direction_ == kPreIncrement) {
      increment_ = HAdd::New(zone(), context_, phi_, one);
    } else {
      increment_ = HSub::New(zone(), context_, phi_, one);
    }
    increment_->ClearFlag(HValue::kCanOverflow);
    builder_->AddInstruction(increment_);
    return increment_;
  } else {
    return phi_;
  }
}


void HGraphBuilder::LoopBuilder::Break() {
  if (exit_trampoline_block_ == NULL) {
    // Its the first time we saw a break.
    HEnvironment* env = exit_block_->last_environment()->Copy();
    exit_trampoline_block_ = builder_->CreateBasicBlock(env);
    builder_->GotoNoSimulate(exit_block_, exit_trampoline_block_);
  }

  builder_->GotoNoSimulate(exit_trampoline_block_);
  builder_->set_current_block(NULL);
}


void HGraphBuilder::LoopBuilder::EndBody() {
  ASSERT(!finished_);

  if (direction_ == kPostIncrement || direction_ == kPostDecrement) {
    if (direction_ == kPostIncrement) {
      increment_ = HAdd::New(zone(), context_, phi_, increment_amount_);
    } else {
      increment_ = HSub::New(zone(), context_, phi_, increment_amount_);
    }
    increment_->ClearFlag(HValue::kCanOverflow);
    builder_->AddInstruction(increment_);
  }

  // Push the new increment value on the expression stack to merge into the phi.
  builder_->environment()->Push(increment_);
  HBasicBlock* last_block = builder_->current_block();
  builder_->GotoNoSimulate(last_block, header_block_);
  header_block_->loop_information()->RegisterBackEdge(last_block);

  if (exit_trampoline_block_ != NULL) {
    builder_->set_current_block(exit_trampoline_block_);
  } else {
    builder_->set_current_block(exit_block_);
  }
  finished_ = true;
}


HGraph* HGraphBuilder::CreateGraph() {
  graph_ = new(zone()) HGraph(info_);
  if (FLAG_hydrogen_stats) isolate()->GetHStatistics()->Initialize(info_);
  CompilationPhase phase("H_Block building", info_);
  set_current_block(graph()->entry_block());
  if (!BuildGraph()) return NULL;
  graph()->FinalizeUniqueness();
  return graph_;
}


HInstruction* HGraphBuilder::AddInstruction(HInstruction* instr) {
  ASSERT(current_block() != NULL);
  ASSERT(!FLAG_hydrogen_track_positions ||
         !position_.IsUnknown() ||
         !info_->IsOptimizing());
  current_block()->AddInstruction(instr, source_position());
  if (graph()->IsInsideNoSideEffectsScope()) {
    instr->SetFlag(HValue::kHasNoObservableSideEffects);
  }
  return instr;
}


void HGraphBuilder::FinishCurrentBlock(HControlInstruction* last) {
  ASSERT(!FLAG_hydrogen_track_positions ||
         !info_->IsOptimizing() ||
         !position_.IsUnknown());
  current_block()->Finish(last, source_position());
  if (last->IsReturn() || last->IsAbnormalExit()) {
    set_current_block(NULL);
  }
}


void HGraphBuilder::FinishExitCurrentBlock(HControlInstruction* instruction) {
  ASSERT(!FLAG_hydrogen_track_positions || !info_->IsOptimizing() ||
         !position_.IsUnknown());
  current_block()->FinishExit(instruction, source_position());
  if (instruction->IsReturn() || instruction->IsAbnormalExit()) {
    set_current_block(NULL);
  }
}


void HGraphBuilder::AddIncrementCounter(StatsCounter* counter) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    HValue* reference = Add<HConstant>(ExternalReference(counter));
    HValue* old_value = Add<HLoadNamedField>(
        reference, static_cast<HValue*>(NULL), HObjectAccess::ForCounter());
    HValue* new_value = AddUncasted<HAdd>(old_value, graph()->GetConstant1());
    new_value->ClearFlag(HValue::kCanOverflow);  // Ignore counter overflow
    Add<HStoreNamedField>(reference, HObjectAccess::ForCounter(),
                          new_value, STORE_TO_INITIALIZED_ENTRY);
  }
}


void HGraphBuilder::AddSimulate(BailoutId id,
                                RemovableSimulate removable) {
  ASSERT(current_block() != NULL);
  ASSERT(!graph()->IsInsideNoSideEffectsScope());
  current_block()->AddNewSimulate(id, source_position(), removable);
}


HBasicBlock* HGraphBuilder::CreateBasicBlock(HEnvironment* env) {
  HBasicBlock* b = graph()->CreateBasicBlock();
  b->SetInitialEnvironment(env);
  return b;
}


HBasicBlock* HGraphBuilder::CreateLoopHeaderBlock() {
  HBasicBlock* header = graph()->CreateBasicBlock();
  HEnvironment* entry_env = environment()->CopyAsLoopHeader(header);
  header->SetInitialEnvironment(entry_env);
  header->AttachLoopInformation();
  return header;
}


HValue* HGraphBuilder::BuildCheckHeapObject(HValue* obj) {
  if (obj->type().IsHeapObject()) return obj;
  return Add<HCheckHeapObject>(obj);
}


void HGraphBuilder::FinishExitWithHardDeoptimization(const char* reason) {
  Add<HDeoptimize>(reason, Deoptimizer::EAGER);
  FinishExitCurrentBlock(New<HAbnormalExit>());
}


HValue* HGraphBuilder::BuildCheckMap(HValue* obj, Handle<Map> map) {
  return Add<HCheckMaps>(obj, map, top_info());
}


HValue* HGraphBuilder::BuildCheckString(HValue* string) {
  if (!string->type().IsString()) {
    ASSERT(!string->IsConstant() ||
           !HConstant::cast(string)->HasStringValue());
    BuildCheckHeapObject(string);
    return Add<HCheckInstanceType>(string, HCheckInstanceType::IS_STRING);
  }
  return string;
}


HValue* HGraphBuilder::BuildWrapReceiver(HValue* object, HValue* function) {
  if (object->type().IsJSObject()) return object;
  if (function->IsConstant() &&
      HConstant::cast(function)->handle(isolate())->IsJSFunction()) {
    Handle<JSFunction> f = Handle<JSFunction>::cast(
        HConstant::cast(function)->handle(isolate()));
    SharedFunctionInfo* shared = f->shared();
    if (!shared->is_classic_mode() || shared->native()) return object;
  }
  return Add<HWrapReceiver>(object, function);
}


HValue* HGraphBuilder::BuildCheckForCapacityGrow(
    HValue* object,
    HValue* elements,
    ElementsKind kind,
    HValue* length,
    HValue* key,
    bool is_js_array,
    PropertyAccessType access_type) {
  IfBuilder length_checker(this);

  Token::Value token = IsHoleyElementsKind(kind) ? Token::GTE : Token::EQ;
  length_checker.If<HCompareNumericAndBranch>(key, length, token);

  length_checker.Then();

  HValue* current_capacity = AddLoadFixedArrayLength(elements);

  IfBuilder capacity_checker(this);

  capacity_checker.If<HCompareNumericAndBranch>(key, current_capacity,
                                                Token::GTE);
  capacity_checker.Then();

  HValue* max_gap = Add<HConstant>(static_cast<int32_t>(JSObject::kMaxGap));
  HValue* max_capacity = AddUncasted<HAdd>(current_capacity, max_gap);
  IfBuilder key_checker(this);
  key_checker.If<HCompareNumericAndBranch>(key, max_capacity, Token::LT);
  key_checker.Then();
  key_checker.ElseDeopt("Key out of capacity range");
  key_checker.End();

  HValue* new_capacity = BuildNewElementsCapacity(key);
  HValue* new_elements = BuildGrowElementsCapacity(object, elements,
                                                   kind, kind, length,
                                                   new_capacity);

  environment()->Push(new_elements);
  capacity_checker.Else();

  environment()->Push(elements);
  capacity_checker.End();

  if (is_js_array) {
    HValue* new_length = AddUncasted<HAdd>(key, graph_->GetConstant1());
    new_length->ClearFlag(HValue::kCanOverflow);

    Add<HStoreNamedField>(object, HObjectAccess::ForArrayLength(kind),
                          new_length);
  }

  if (access_type == STORE && kind == FAST_SMI_ELEMENTS) {
    HValue* checked_elements = environment()->Top();

    // Write zero to ensure that the new element is initialized with some smi.
    Add<HStoreKeyed>(checked_elements, key, graph()->GetConstant0(), kind);
  }

  length_checker.Else();
  Add<HBoundsCheck>(key, length);

  environment()->Push(elements);
  length_checker.End();

  return environment()->Pop();
}


HValue* HGraphBuilder::BuildCopyElementsOnWrite(HValue* object,
                                                HValue* elements,
                                                ElementsKind kind,
                                                HValue* length) {
  Factory* factory = isolate()->factory();

  IfBuilder cow_checker(this);

  cow_checker.If<HCompareMap>(
      elements, factory->fixed_cow_array_map(), top_info());
  cow_checker.Then();

  HValue* capacity = AddLoadFixedArrayLength(elements);

  HValue* new_elements = BuildGrowElementsCapacity(object, elements, kind,
                                                   kind, length, capacity);

  environment()->Push(new_elements);

  cow_checker.Else();

  environment()->Push(elements);

  cow_checker.End();

  return environment()->Pop();
}


void HGraphBuilder::BuildTransitionElementsKind(HValue* object,
                                                HValue* map,
                                                ElementsKind from_kind,
                                                ElementsKind to_kind,
                                                bool is_jsarray) {
  ASSERT(!IsFastHoleyElementsKind(from_kind) ||
         IsFastHoleyElementsKind(to_kind));

  if (AllocationSite::GetMode(from_kind, to_kind) == TRACK_ALLOCATION_SITE) {
    Add<HTrapAllocationMemento>(object);
  }

  if (!IsSimpleMapChangeTransition(from_kind, to_kind)) {
    HInstruction* elements = AddLoadElements(object);

    HInstruction* empty_fixed_array = Add<HConstant>(
        isolate()->factory()->empty_fixed_array());

    IfBuilder if_builder(this);

    if_builder.IfNot<HCompareObjectEqAndBranch>(elements, empty_fixed_array);

    if_builder.Then();

    HInstruction* elements_length = AddLoadFixedArrayLength(elements);

    HInstruction* array_length = is_jsarray
        ? Add<HLoadNamedField>(object, static_cast<HValue*>(NULL),
                               HObjectAccess::ForArrayLength(from_kind))
        : elements_length;

    BuildGrowElementsCapacity(object, elements, from_kind, to_kind,
                              array_length, elements_length);

    if_builder.End();
  }

  Add<HStoreNamedField>(object, HObjectAccess::ForMap(), map);
}


HValue* HGraphBuilder::BuildUncheckedDictionaryElementLoadHelper(
    HValue* elements,
    HValue* key,
    HValue* hash,
    HValue* mask,
    int current_probe) {
  if (current_probe == kNumberDictionaryProbes) {
    return NULL;
  }

  int32_t offset = SeededNumberDictionary::GetProbeOffset(current_probe);
  HValue* raw_index = (current_probe == 0)
      ? hash
      : AddUncasted<HAdd>(hash, Add<HConstant>(offset));
  raw_index = AddUncasted<HBitwise>(Token::BIT_AND, raw_index, mask);
  int32_t entry_size = SeededNumberDictionary::kEntrySize;
  raw_index = AddUncasted<HMul>(raw_index, Add<HConstant>(entry_size));
  raw_index->ClearFlag(HValue::kCanOverflow);

  int32_t base_offset = SeededNumberDictionary::kElementsStartIndex;
  HValue* key_index = AddUncasted<HAdd>(raw_index, Add<HConstant>(base_offset));
  key_index->ClearFlag(HValue::kCanOverflow);

  HValue* candidate_key = Add<HLoadKeyed>(elements, key_index,
                                          static_cast<HValue*>(NULL),
                                          FAST_ELEMENTS);

  IfBuilder key_compare(this);
  key_compare.IfNot<HCompareObjectEqAndBranch>(key, candidate_key);
  key_compare.Then();
  {
    // Key at the current probe doesn't match, try at the next probe.
    HValue* result = BuildUncheckedDictionaryElementLoadHelper(
        elements, key, hash, mask, current_probe + 1);
    if (result == NULL) {
      key_compare.Deopt("probes exhausted in keyed load dictionary lookup");
      result = graph()->GetConstantUndefined();
    } else {
      Push(result);
    }
  }
  key_compare.Else();
  {
    // Key at current probe matches. Details must be zero, otherwise the
    // dictionary element requires special handling.
    HValue* details_index = AddUncasted<HAdd>(
        raw_index, Add<HConstant>(base_offset + 2));
    details_index->ClearFlag(HValue::kCanOverflow);

    HValue* details = Add<HLoadKeyed>(elements, details_index,
                                      static_cast<HValue*>(NULL),
                                      FAST_ELEMENTS);
    IfBuilder details_compare(this);
    details_compare.If<HCompareNumericAndBranch>(details,
                                                 graph()->GetConstant0(),
                                                 Token::NE);
    details_compare.ThenDeopt("keyed load dictionary element not fast case");

    details_compare.Else();
    {
      // Key matches and details are zero --> fast case. Load and return the
      // value.
      HValue* result_index = AddUncasted<HAdd>(
          raw_index, Add<HConstant>(base_offset + 1));
      result_index->ClearFlag(HValue::kCanOverflow);

      Push(Add<HLoadKeyed>(elements, result_index,
                           static_cast<HValue*>(NULL),
                           FAST_ELEMENTS));
    }
    details_compare.End();
  }
  key_compare.End();

  return Pop();
}


HValue* HGraphBuilder::BuildElementIndexHash(HValue* index) {
  int32_t seed_value = static_cast<uint32_t>(isolate()->heap()->HashSeed());
  HValue* seed = Add<HConstant>(seed_value);
  HValue* hash = AddUncasted<HBitwise>(Token::BIT_XOR, index, seed);

  // hash = ~hash + (hash << 15);
  HValue* shifted_hash = AddUncasted<HShl>(hash, Add<HConstant>(15));
  HValue* not_hash = AddUncasted<HBitwise>(Token::BIT_XOR, hash,
                                           graph()->GetConstantMinus1());
  hash = AddUncasted<HAdd>(shifted_hash, not_hash);

  // hash = hash ^ (hash >> 12);
  shifted_hash = AddUncasted<HShr>(hash, Add<HConstant>(12));
  hash = AddUncasted<HBitwise>(Token::BIT_XOR, hash, shifted_hash);

  // hash = hash + (hash << 2);
  shifted_hash = AddUncasted<HShl>(hash, Add<HConstant>(2));
  hash = AddUncasted<HAdd>(hash, shifted_hash);

  // hash = hash ^ (hash >> 4);
  shifted_hash = AddUncasted<HShr>(hash, Add<HConstant>(4));
  hash = AddUncasted<HBitwise>(Token::BIT_XOR, hash, shifted_hash);

  // hash = hash * 2057;
  hash = AddUncasted<HMul>(hash, Add<HConstant>(2057));
  hash->ClearFlag(HValue::kCanOverflow);

  // hash = hash ^ (hash >> 16);
  shifted_hash = AddUncasted<HShr>(hash, Add<HConstant>(16));
  return AddUncasted<HBitwise>(Token::BIT_XOR, hash, shifted_hash);
}


HValue* HGraphBuilder::BuildUncheckedDictionaryElementLoad(HValue* receiver,
                                                           HValue* key) {
  HValue* elements = AddLoadElements(receiver);

  HValue* hash = BuildElementIndexHash(key);

  HValue* capacity = Add<HLoadKeyed>(
      elements,
      Add<HConstant>(NameDictionary::kCapacityIndex),
      static_cast<HValue*>(NULL),
      FAST_ELEMENTS);

  HValue* mask = AddUncasted<HSub>(capacity, graph()->GetConstant1());
  mask->ChangeRepresentation(Representation::Integer32());
  mask->ClearFlag(HValue::kCanOverflow);

  return BuildUncheckedDictionaryElementLoadHelper(elements, key,
                                                   hash, mask, 0);
}


HValue* HGraphBuilder::BuildRegExpConstructResult(HValue* length,
                                                  HValue* index,
                                                  HValue* input) {
  NoObservableSideEffectsScope scope(this);

  // Compute the size of the RegExpResult followed by FixedArray with length.
  HValue* size = length;
  size = AddUncasted<HShl>(size, Add<HConstant>(kPointerSizeLog2));
  size = AddUncasted<HAdd>(size, Add<HConstant>(static_cast<int32_t>(
              JSRegExpResult::kSize + FixedArray::kHeaderSize)));

  // Make sure size does not exceeds max regular heap object size.
  Add<HBoundsCheck>(size, Add<HConstant>(Page::kMaxRegularHeapObjectSize));

  // Allocate the JSRegExpResult and the FixedArray in one step.
  HValue* result = Add<HAllocate>(
      size, HType::JSArray(), NOT_TENURED, JS_ARRAY_TYPE);

  // Determine the elements FixedArray.
  HValue* elements = Add<HInnerAllocatedObject>(
      result, Add<HConstant>(JSRegExpResult::kSize));

  // Initialize the JSRegExpResult header.
  HValue* global_object = Add<HLoadNamedField>(
      context(), static_cast<HValue*>(NULL),
      HObjectAccess::ForContextSlot(Context::GLOBAL_OBJECT_INDEX));
  HValue* native_context = Add<HLoadNamedField>(
      global_object, static_cast<HValue*>(NULL),
      HObjectAccess::ForGlobalObjectNativeContext());
  AddStoreMapNoWriteBarrier(result, Add<HLoadNamedField>(
          native_context, static_cast<HValue*>(NULL),
          HObjectAccess::ForContextSlot(Context::REGEXP_RESULT_MAP_INDEX)));
  Add<HStoreNamedField>(
      result, HObjectAccess::ForJSArrayOffset(JSArray::kPropertiesOffset),
      Add<HConstant>(isolate()->factory()->empty_fixed_array()));
  Add<HStoreNamedField>(
      result, HObjectAccess::ForJSArrayOffset(JSArray::kElementsOffset),
      elements);
  Add<HStoreNamedField>(
      result, HObjectAccess::ForJSArrayOffset(JSArray::kLengthOffset), length);

  // Initialize the additional fields.
  Add<HStoreNamedField>(
      result, HObjectAccess::ForJSArrayOffset(JSRegExpResult::kIndexOffset),
      index);
  Add<HStoreNamedField>(
      result, HObjectAccess::ForJSArrayOffset(JSRegExpResult::kInputOffset),
      input);

  // Initialize the elements header.
  AddStoreMapConstantNoWriteBarrier(elements,
                                    isolate()->factory()->fixed_array_map());
  Add<HStoreNamedField>(elements, HObjectAccess::ForFixedArrayLength(), length);

  // Initialize the elements contents with undefined.
  LoopBuilder loop(this, context(), LoopBuilder::kPostIncrement);
  index = loop.BeginBody(graph()->GetConstant0(), length, Token::LT);
  {
    Add<HStoreKeyed>(elements, index, graph()->GetConstantUndefined(),
                     FAST_ELEMENTS);
  }
  loop.EndBody();

  return result;
}


HValue* HGraphBuilder::BuildNumberToString(HValue* object, Type* type) {
  NoObservableSideEffectsScope scope(this);

  // Convert constant numbers at compile time.
  if (object->IsConstant() && HConstant::cast(object)->HasNumberValue()) {
    Handle<Object> number = HConstant::cast(object)->handle(isolate());
    Handle<String> result = isolate()->factory()->NumberToString(number);
    return Add<HConstant>(result);
  }

  // Create a joinable continuation.
  HIfContinuation found(graph()->CreateBasicBlock(),
                        graph()->CreateBasicBlock());

  // Load the number string cache.
  HValue* number_string_cache =
      Add<HLoadRoot>(Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  HValue* mask = AddLoadFixedArrayLength(number_string_cache);
  mask->set_type(HType::Smi());
  mask = AddUncasted<HSar>(mask, graph()->GetConstant1());
  mask = AddUncasted<HSub>(mask, graph()->GetConstant1());

  // Check whether object is a smi.
  IfBuilder if_objectissmi(this);
  if_objectissmi.If<HIsSmiAndBranch>(object);
  if_objectissmi.Then();
  {
    // Compute hash for smi similar to smi_get_hash().
    HValue* hash = AddUncasted<HBitwise>(Token::BIT_AND, object, mask);

    // Load the key.
    HValue* key_index = AddUncasted<HShl>(hash, graph()->GetConstant1());
    HValue* key = Add<HLoadKeyed>(number_string_cache, key_index,
                                  static_cast<HValue*>(NULL),
                                  FAST_ELEMENTS, ALLOW_RETURN_HOLE);

    // Check if object == key.
    IfBuilder if_objectiskey(this);
    if_objectiskey.If<HCompareObjectEqAndBranch>(object, key);
    if_objectiskey.Then();
    {
      // Make the key_index available.
      Push(key_index);
    }
    if_objectiskey.JoinContinuation(&found);
  }
  if_objectissmi.Else();
  {
    if (type->Is(Type::Smi())) {
      if_objectissmi.Deopt("Expected smi");
    } else {
      // Check if the object is a heap number.
      IfBuilder if_objectisnumber(this);
      HValue* objectisnumber = if_objectisnumber.If<HCompareMap>(
          object, isolate()->factory()->heap_number_map(), top_info());
      if_objectisnumber.Then();
      {
        // Compute hash for heap number similar to double_get_hash().
        HValue* low = Add<HLoadNamedField>(
            object, objectisnumber,
            HObjectAccess::ForHeapNumberValueLowestBits());
        HValue* high = Add<HLoadNamedField>(
            object, objectisnumber,
            HObjectAccess::ForHeapNumberValueHighestBits());
        HValue* hash = AddUncasted<HBitwise>(Token::BIT_XOR, low, high);
        hash = AddUncasted<HBitwise>(Token::BIT_AND, hash, mask);

        // Load the key.
        HValue* key_index = AddUncasted<HShl>(hash, graph()->GetConstant1());
        HValue* key = Add<HLoadKeyed>(number_string_cache, key_index,
                                      static_cast<HValue*>(NULL),
                                      FAST_ELEMENTS, ALLOW_RETURN_HOLE);

        // Check if key is a heap number (the number string cache contains only
        // SMIs and heap number, so it is sufficient to do a SMI check here).
        IfBuilder if_keyisnotsmi(this);
        HValue* keyisnotsmi = if_keyisnotsmi.IfNot<HIsSmiAndBranch>(key);
        if_keyisnotsmi.Then();
        {
          // Check if values of key and object match.
          IfBuilder if_keyeqobject(this);
          if_keyeqobject.If<HCompareNumericAndBranch>(
              Add<HLoadNamedField>(key, keyisnotsmi,
                                   HObjectAccess::ForHeapNumberValue()),
              Add<HLoadNamedField>(object, objectisnumber,
                                   HObjectAccess::ForHeapNumberValue()),
              Token::EQ);
          if_keyeqobject.Then();
          {
            // Make the key_index available.
            Push(key_index);
          }
          if_keyeqobject.JoinContinuation(&found);
        }
        if_keyisnotsmi.JoinContinuation(&found);
      }
      if_objectisnumber.Else();
      {
        if (type->Is(Type::Number())) {
          if_objectisnumber.Deopt("Expected heap number");
        }
      }
      if_objectisnumber.JoinContinuation(&found);
    }
  }
  if_objectissmi.JoinContinuation(&found);

  // Check for cache hit.
  IfBuilder if_found(this, &found);
  if_found.Then();
  {
    // Count number to string operation in native code.
    AddIncrementCounter(isolate()->counters()->number_to_string_native());

    // Load the value in case of cache hit.
    HValue* key_index = Pop();
    HValue* value_index = AddUncasted<HAdd>(key_index, graph()->GetConstant1());
    Push(Add<HLoadKeyed>(number_string_cache, value_index,
                         static_cast<HValue*>(NULL),
                         FAST_ELEMENTS, ALLOW_RETURN_HOLE));
  }
  if_found.Else();
  {
    // Cache miss, fallback to runtime.
    Add<HPushArgument>(object);
    Push(Add<HCallRuntime>(
            isolate()->factory()->empty_string(),
            Runtime::FunctionForId(Runtime::kNumberToStringSkipCache),
            1));
  }
  if_found.End();

  return Pop();
}


HAllocate* HGraphBuilder::BuildAllocate(
    HValue* object_size,
    HType type,
    InstanceType instance_type,
    HAllocationMode allocation_mode) {
  // Compute the effective allocation size.
  HValue* size = object_size;
  if (allocation_mode.CreateAllocationMementos()) {
    size = AddUncasted<HAdd>(size, Add<HConstant>(AllocationMemento::kSize));
    size->ClearFlag(HValue::kCanOverflow);
  }

  // Perform the actual allocation.
  HAllocate* object = Add<HAllocate>(
      size, type, allocation_mode.GetPretenureMode(),
      instance_type, allocation_mode.feedback_site());

  // Setup the allocation memento.
  if (allocation_mode.CreateAllocationMementos()) {
    BuildCreateAllocationMemento(
        object, object_size, allocation_mode.current_site());
  }

  return object;
}


HValue* HGraphBuilder::BuildAddStringLengths(HValue* left_length,
                                             HValue* right_length) {
  // Compute the combined string length. If the result is larger than the max
  // supported string length, we bailout to the runtime. This is done implicitly
  // when converting the result back to a smi in case the max string length
  // equals the max smi value. Otherwise, for platforms with 32-bit smis, we do
  HValue* length = AddUncasted<HAdd>(left_length, right_length);
  STATIC_ASSERT(String::kMaxLength <= Smi::kMaxValue);
  if (String::kMaxLength != Smi::kMaxValue) {
    IfBuilder if_nooverflow(this);
    if_nooverflow.If<HCompareNumericAndBranch>(
        length, Add<HConstant>(String::kMaxLength), Token::LTE);
    if_nooverflow.Then();
    if_nooverflow.ElseDeopt("String length exceeds limit");
  }
  return length;
}


HValue* HGraphBuilder::BuildCreateConsString(
    HValue* length,
    HValue* left,
    HValue* right,
    HAllocationMode allocation_mode) {
  // Determine the string instance types.
  HInstruction* left_instance_type = AddLoadStringInstanceType(left);
  HInstruction* right_instance_type = AddLoadStringInstanceType(right);

  // Allocate the cons string object. HAllocate does not care whether we
  // pass CONS_STRING_TYPE or CONS_ASCII_STRING_TYPE here, so we just use
  // CONS_STRING_TYPE here. Below we decide whether the cons string is
  // one-byte or two-byte and set the appropriate map.
  ASSERT(HAllocate::CompatibleInstanceTypes(CONS_STRING_TYPE,
                                            CONS_ASCII_STRING_TYPE));
  HAllocate* result = BuildAllocate(Add<HConstant>(ConsString::kSize),
                                    HType::String(), CONS_STRING_TYPE,
                                    allocation_mode);

  // Compute intersection and difference of instance types.
  HValue* anded_instance_types = AddUncasted<HBitwise>(
      Token::BIT_AND, left_instance_type, right_instance_type);
  HValue* xored_instance_types = AddUncasted<HBitwise>(
      Token::BIT_XOR, left_instance_type, right_instance_type);

  // We create a one-byte cons string if
  // 1. both strings are one-byte, or
  // 2. at least one of the strings is two-byte, but happens to contain only
  //    one-byte characters.
  // To do this, we check
  // 1. if both strings are one-byte, or if the one-byte data hint is set in
  //    both strings, or
  // 2. if one of the strings has the one-byte data hint set and the other
  //    string is one-byte.
  IfBuilder if_onebyte(this);
  STATIC_ASSERT(kOneByteStringTag != 0);
  STATIC_ASSERT(kOneByteDataHintMask != 0);
  if_onebyte.If<HCompareNumericAndBranch>(
      AddUncasted<HBitwise>(
          Token::BIT_AND, anded_instance_types,
          Add<HConstant>(static_cast<int32_t>(
                  kStringEncodingMask | kOneByteDataHintMask))),
      graph()->GetConstant0(), Token::NE);
  if_onebyte.Or();
  STATIC_ASSERT(kOneByteStringTag != 0 &&
                kOneByteDataHintTag != 0 &&
                kOneByteDataHintTag != kOneByteStringTag);
  if_onebyte.If<HCompareNumericAndBranch>(
      AddUncasted<HBitwise>(
          Token::BIT_AND, xored_instance_types,
          Add<HConstant>(static_cast<int32_t>(
                  kOneByteStringTag | kOneByteDataHintTag))),
      Add<HConstant>(static_cast<int32_t>(
              kOneByteStringTag | kOneByteDataHintTag)), Token::EQ);
  if_onebyte.Then();
  {
    // We can safely skip the write barrier for storing the map here.
    Handle<Map> map = isolate()->factory()->cons_ascii_string_map();
    AddStoreMapConstantNoWriteBarrier(result, map);
  }
  if_onebyte.Else();
  {
    // We can safely skip the write barrier for storing the map here.
    Handle<Map> map = isolate()->factory()->cons_string_map();
    AddStoreMapConstantNoWriteBarrier(result, map);
  }
  if_onebyte.End();

  // Initialize the cons string fields.
  Add<HStoreNamedField>(result, HObjectAccess::ForStringHashField(),
                        Add<HConstant>(String::kEmptyHashField));
  Add<HStoreNamedField>(result, HObjectAccess::ForStringLength(), length);
  Add<HStoreNamedField>(result, HObjectAccess::ForConsStringFirst(), left);
  Add<HStoreNamedField>(result, HObjectAccess::ForConsStringSecond(), right);

  // Count the native string addition.
  AddIncrementCounter(isolate()->counters()->string_add_native());

  return result;
}


void HGraphBuilder::BuildCopySeqStringChars(HValue* src,
                                            HValue* src_offset,
                                            String::Encoding src_encoding,
                                            HValue* dst,
                                            HValue* dst_offset,
                                            String::Encoding dst_encoding,
                                            HValue* length) {
  ASSERT(dst_encoding != String::ONE_BYTE_ENCODING ||
         src_encoding == String::ONE_BYTE_ENCODING);
  LoopBuilder loop(this, context(), LoopBuilder::kPostIncrement);
  HValue* index = loop.BeginBody(graph()->GetConstant0(), length, Token::LT);
  {
    HValue* src_index = AddUncasted<HAdd>(src_offset, index);
    HValue* value =
        AddUncasted<HSeqStringGetChar>(src_encoding, src, src_index);
    HValue* dst_index = AddUncasted<HAdd>(dst_offset, index);
    Add<HSeqStringSetChar>(dst_encoding, dst, dst_index, value);
  }
  loop.EndBody();
}


HValue* HGraphBuilder::BuildUncheckedStringAdd(
    HValue* left,
    HValue* right,
    HAllocationMode allocation_mode) {
  // Determine the string lengths.
  HValue* left_length = AddLoadStringLength(left);
  HValue* right_length = AddLoadStringLength(right);

  // Compute the combined string length.
  HValue* length = BuildAddStringLengths(left_length, right_length);

  // Do some manual constant folding here.
  if (left_length->IsConstant()) {
    HConstant* c_left_length = HConstant::cast(left_length);
    ASSERT_NE(0, c_left_length->Integer32Value());
    if (c_left_length->Integer32Value() + 1 >= ConsString::kMinLength) {
      // The right string contains at least one character.
      return BuildCreateConsString(length, left, right, allocation_mode);
    }
  } else if (right_length->IsConstant()) {
    HConstant* c_right_length = HConstant::cast(right_length);
    ASSERT_NE(0, c_right_length->Integer32Value());
    if (c_right_length->Integer32Value() + 1 >= ConsString::kMinLength) {
      // The left string contains at least one character.
      return BuildCreateConsString(length, left, right, allocation_mode);
    }
  }

  // Check if we should create a cons string.
  IfBuilder if_createcons(this);
  if_createcons.If<HCompareNumericAndBranch>(
      length, Add<HConstant>(ConsString::kMinLength), Token::GTE);
  if_createcons.Then();
  {
    // Create a cons string.
    Push(BuildCreateConsString(length, left, right, allocation_mode));
  }
  if_createcons.Else();
  {
    // Determine the string instance types.
    HValue* left_instance_type = AddLoadStringInstanceType(left);
    HValue* right_instance_type = AddLoadStringInstanceType(right);

    // Compute union and difference of instance types.
    HValue* ored_instance_types = AddUncasted<HBitwise>(
        Token::BIT_OR, left_instance_type, right_instance_type);
    HValue* xored_instance_types = AddUncasted<HBitwise>(
        Token::BIT_XOR, left_instance_type, right_instance_type);

    // Check if both strings have the same encoding and both are
    // sequential.
    IfBuilder if_sameencodingandsequential(this);
    if_sameencodingandsequential.If<HCompareNumericAndBranch>(
        AddUncasted<HBitwise>(
            Token::BIT_AND, xored_instance_types,
            Add<HConstant>(static_cast<int32_t>(kStringEncodingMask))),
        graph()->GetConstant0(), Token::EQ);
    if_sameencodingandsequential.And();
    STATIC_ASSERT(kSeqStringTag == 0);
    if_sameencodingandsequential.If<HCompareNumericAndBranch>(
        AddUncasted<HBitwise>(
            Token::BIT_AND, ored_instance_types,
            Add<HConstant>(static_cast<int32_t>(kStringRepresentationMask))),
        graph()->GetConstant0(), Token::EQ);
    if_sameencodingandsequential.Then();
    {
      HConstant* string_map =
          Add<HConstant>(isolate()->factory()->string_map());
      HConstant* ascii_string_map =
          Add<HConstant>(isolate()->factory()->ascii_string_map());

      // Determine map and size depending on whether result is one-byte string.
      IfBuilder if_onebyte(this);
      STATIC_ASSERT(kOneByteStringTag != 0);
      if_onebyte.If<HCompareNumericAndBranch>(
          AddUncasted<HBitwise>(
              Token::BIT_AND, ored_instance_types,
              Add<HConstant>(static_cast<int32_t>(kStringEncodingMask))),
          graph()->GetConstant0(), Token::NE);
      if_onebyte.Then();
      {
        // Allocate sequential one-byte string object.
        Push(length);
        Push(ascii_string_map);
      }
      if_onebyte.Else();
      {
        // Allocate sequential two-byte string object.
        HValue* size = AddUncasted<HShl>(length, graph()->GetConstant1());
        size->ClearFlag(HValue::kCanOverflow);
        size->SetFlag(HValue::kUint32);
        Push(size);
        Push(string_map);
      }
      if_onebyte.End();
      HValue* map = Pop();

      // Calculate the number of bytes needed for the characters in the
      // string while observing object alignment.
      STATIC_ASSERT((SeqString::kHeaderSize & kObjectAlignmentMask) == 0);
      HValue* size = Pop();
      size = AddUncasted<HAdd>(size, Add<HConstant>(static_cast<int32_t>(
                  SeqString::kHeaderSize + kObjectAlignmentMask)));
      size->ClearFlag(HValue::kCanOverflow);
      size = AddUncasted<HBitwise>(
          Token::BIT_AND, size, Add<HConstant>(static_cast<int32_t>(
                  ~kObjectAlignmentMask)));

      // Allocate the string object. HAllocate does not care whether we pass
      // STRING_TYPE or ASCII_STRING_TYPE here, so we just use STRING_TYPE here.
      HAllocate* result = BuildAllocate(
          size, HType::String(), STRING_TYPE, allocation_mode);

      // We can safely skip the write barrier for storing map here.
      AddStoreMapNoWriteBarrier(result, map);

      // Initialize the string fields.
      Add<HStoreNamedField>(result, HObjectAccess::ForStringHashField(),
                            Add<HConstant>(String::kEmptyHashField));
      Add<HStoreNamedField>(result, HObjectAccess::ForStringLength(), length);

      // Copy characters to the result string.
      IfBuilder if_twobyte(this);
      if_twobyte.If<HCompareObjectEqAndBranch>(map, string_map);
      if_twobyte.Then();
      {
        // Copy characters from the left string.
        BuildCopySeqStringChars(
            left, graph()->GetConstant0(), String::TWO_BYTE_ENCODING,
            result, graph()->GetConstant0(), String::TWO_BYTE_ENCODING,
            left_length);

        // Copy characters from the right string.
        BuildCopySeqStringChars(
            right, graph()->GetConstant0(), String::TWO_BYTE_ENCODING,
            result, left_length, String::TWO_BYTE_ENCODING,
            right_length);
      }
      if_twobyte.Else();
      {
        // Copy characters from the left string.
        BuildCopySeqStringChars(
            left, graph()->GetConstant0(), String::ONE_BYTE_ENCODING,
            result, graph()->GetConstant0(), String::ONE_BYTE_ENCODING,
            left_length);

        // Copy characters from the right string.
        BuildCopySeqStringChars(
            right, graph()->GetConstant0(), String::ONE_BYTE_ENCODING,
            result, left_length, String::ONE_BYTE_ENCODING,
            right_length);
      }
      if_twobyte.End();

      // Count the native string addition.
      AddIncrementCounter(isolate()->counters()->string_add_native());

      // Return the sequential string.
      Push(result);
    }
    if_sameencodingandsequential.Else();
    {
      // Fallback to the runtime to add the two strings.
      Add<HPushArgument>(left);
      Add<HPushArgument>(right);
      Push(Add<HCallRuntime>(isolate()->factory()->empty_string(),
                             Runtime::FunctionForId(Runtime::kStringAdd),
                             2));
    }
    if_sameencodingandsequential.End();
  }
  if_createcons.End();

  return Pop();
}


HValue* HGraphBuilder::BuildStringAdd(
    HValue* left,
    HValue* right,
    HAllocationMode allocation_mode) {
  NoObservableSideEffectsScope no_effects(this);

  // Determine string lengths.
  HValue* left_length = AddLoadStringLength(left);
  HValue* right_length = AddLoadStringLength(right);

  // Check if left string is empty.
  IfBuilder if_leftempty(this);
  if_leftempty.If<HCompareNumericAndBranch>(
      left_length, graph()->GetConstant0(), Token::EQ);
  if_leftempty.Then();
  {
    // Count the native string addition.
    AddIncrementCounter(isolate()->counters()->string_add_native());

    // Just return the right string.
    Push(right);
  }
  if_leftempty.Else();
  {
    // Check if right string is empty.
    IfBuilder if_rightempty(this);
    if_rightempty.If<HCompareNumericAndBranch>(
        right_length, graph()->GetConstant0(), Token::EQ);
    if_rightempty.Then();
    {
      // Count the native string addition.
      AddIncrementCounter(isolate()->counters()->string_add_native());

      // Just return the left string.
      Push(left);
    }
    if_rightempty.Else();
    {
      // Add the two non-empty strings.
      Push(BuildUncheckedStringAdd(left, right, allocation_mode));
    }
    if_rightempty.End();
  }
  if_leftempty.End();

  return Pop();
}


HInstruction* HGraphBuilder::BuildUncheckedMonomorphicElementAccess(
    HValue* checked_object,
    HValue* key,
    HValue* val,
    bool is_js_array,
    ElementsKind elements_kind,
    PropertyAccessType access_type,
    LoadKeyedHoleMode load_mode,
    KeyedAccessStoreMode store_mode) {
  ASSERT((!IsExternalArrayElementsKind(elements_kind) &&
              !IsFixedTypedArrayElementsKind(elements_kind)) ||
         !is_js_array);
  // No GVNFlag is necessary for ElementsKind if there is an explicit dependency
  // on a HElementsTransition instruction. The flag can also be removed if the
  // map to check has FAST_HOLEY_ELEMENTS, since there can be no further
  // ElementsKind transitions. Finally, the dependency can be removed for stores
  // for FAST_ELEMENTS, since a transition to HOLEY elements won't change the
  // generated store code.
  if ((elements_kind == FAST_HOLEY_ELEMENTS) ||
      (elements_kind == FAST_ELEMENTS && access_type == STORE)) {
    checked_object->ClearDependsOnFlag(kElementsKind);
  }

  bool fast_smi_only_elements = IsFastSmiElementsKind(elements_kind);
  bool fast_elements = IsFastObjectElementsKind(elements_kind);
  HValue* elements = AddLoadElements(checked_object);
  if (access_type == STORE && (fast_elements || fast_smi_only_elements) &&
      store_mode != STORE_NO_TRANSITION_HANDLE_COW) {
    HCheckMaps* check_cow_map = Add<HCheckMaps>(
        elements, isolate()->factory()->fixed_array_map(), top_info());
    check_cow_map->ClearDependsOnFlag(kElementsKind);
  }
  HInstruction* length = NULL;
  if (is_js_array) {
    length = Add<HLoadNamedField>(
        checked_object, static_cast<HValue*>(NULL),
        HObjectAccess::ForArrayLength(elements_kind));
  } else {
    length = AddLoadFixedArrayLength(elements);
  }
  length->set_type(HType::Smi());
  HValue* checked_key = NULL;
  if (IsExternalArrayElementsKind(elements_kind) ||
      IsFixedTypedArrayElementsKind(elements_kind)) {
    HValue* backing_store;
    if (IsExternalArrayElementsKind(elements_kind)) {
      backing_store = Add<HLoadNamedField>(
          elements, static_cast<HValue*>(NULL),
          HObjectAccess::ForExternalArrayExternalPointer());
    } else {
      backing_store = elements;
    }
    if (store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
      NoObservableSideEffectsScope no_effects(this);
      IfBuilder length_checker(this);
      length_checker.If<HCompareNumericAndBranch>(key, length, Token::LT);
      length_checker.Then();
      IfBuilder negative_checker(this);
      HValue* bounds_check = negative_checker.If<HCompareNumericAndBranch>(
          key, graph()->GetConstant0(), Token::GTE);
      negative_checker.Then();
      HInstruction* result = AddElementAccess(
          backing_store, key, val, bounds_check, elements_kind, access_type);
      negative_checker.ElseDeopt("Negative key encountered");
      negative_checker.End();
      length_checker.End();
      return result;
    } else {
      ASSERT(store_mode == STANDARD_STORE);
      checked_key = Add<HBoundsCheck>(key, length);
      return AddElementAccess(
          backing_store, checked_key, val,
          checked_object, elements_kind, access_type);
    }
  }
  ASSERT(fast_smi_only_elements ||
         fast_elements ||
         IsFastDoubleElementsKind(elements_kind));

  // In case val is stored into a fast smi array, assure that the value is a smi
  // before manipulating the backing store. Otherwise the actual store may
  // deopt, leaving the backing store in an invalid state.
  if (access_type == STORE && IsFastSmiElementsKind(elements_kind) &&
      !val->type().IsSmi()) {
    val = AddUncasted<HForceRepresentation>(val, Representation::Smi());
  }

  if (IsGrowStoreMode(store_mode)) {
    NoObservableSideEffectsScope no_effects(this);
    elements = BuildCheckForCapacityGrow(checked_object, elements,
                                         elements_kind, length, key,
                                         is_js_array, access_type);
    checked_key = key;
  } else {
    checked_key = Add<HBoundsCheck>(key, length);

    if (access_type == STORE && (fast_elements || fast_smi_only_elements)) {
      if (store_mode == STORE_NO_TRANSITION_HANDLE_COW) {
        NoObservableSideEffectsScope no_effects(this);
        elements = BuildCopyElementsOnWrite(checked_object, elements,
                                            elements_kind, length);
      } else {
        HCheckMaps* check_cow_map = Add<HCheckMaps>(
            elements, isolate()->factory()->fixed_array_map(), top_info());
        check_cow_map->ClearDependsOnFlag(kElementsKind);
      }
    }
  }
  return AddElementAccess(elements, checked_key, val, checked_object,
                          elements_kind, access_type, load_mode);
}



HValue* HGraphBuilder::BuildAllocateArrayFromLength(
    JSArrayBuilder* array_builder,
    HValue* length_argument) {
  if (length_argument->IsConstant() &&
      HConstant::cast(length_argument)->HasSmiValue()) {
    int array_length = HConstant::cast(length_argument)->Integer32Value();
    HValue* new_object = array_length == 0
        ? array_builder->AllocateEmptyArray()
        : array_builder->AllocateArray(length_argument, length_argument);
    return new_object;
  }

  HValue* constant_zero = graph()->GetConstant0();
  HConstant* max_alloc_length =
      Add<HConstant>(JSObject::kInitialMaxFastElementArray);
  HInstruction* checked_length = Add<HBoundsCheck>(length_argument,
                                                   max_alloc_length);
  IfBuilder if_builder(this);
  if_builder.If<HCompareNumericAndBranch>(checked_length, constant_zero,
                                          Token::EQ);
  if_builder.Then();
  const int initial_capacity = JSArray::kPreallocatedArrayElements;
  HConstant* initial_capacity_node = Add<HConstant>(initial_capacity);
  Push(initial_capacity_node);  // capacity
  Push(constant_zero);          // length
  if_builder.Else();
  if (!(top_info()->IsStub()) &&
      IsFastPackedElementsKind(array_builder->kind())) {
    // We'll come back later with better (holey) feedback.
    if_builder.Deopt("Holey array despite packed elements_kind feedback");
  } else {
    Push(checked_length);         // capacity
    Push(checked_length);         // length
  }
  if_builder.End();

  // Figure out total size
  HValue* length = Pop();
  HValue* capacity = Pop();
  return array_builder->AllocateArray(capacity, length);
}

HValue* HGraphBuilder::BuildAllocateElements(ElementsKind kind,
                                             HValue* capacity) {
  int elements_size;
  InstanceType instance_type;

  if (IsFastDoubleElementsKind(kind)) {
    elements_size = kDoubleSize;
    instance_type = FIXED_DOUBLE_ARRAY_TYPE;
  } else {
    elements_size = kPointerSize;
    instance_type = FIXED_ARRAY_TYPE;
  }

  HConstant* elements_size_value = Add<HConstant>(elements_size);
  HValue* mul = AddUncasted<HMul>(capacity, elements_size_value);
  mul->ClearFlag(HValue::kCanOverflow);

  HConstant* header_size = Add<HConstant>(FixedArray::kHeaderSize);
  HValue* total_size = AddUncasted<HAdd>(mul, header_size);
  total_size->ClearFlag(HValue::kCanOverflow);

  PretenureFlag pretenure_flag = !FLAG_allocation_site_pretenuring ?
      isolate()->heap()->GetPretenureMode() : NOT_TENURED;

  return Add<HAllocate>(total_size, HType::JSArray(), pretenure_flag,
      instance_type);
}


void HGraphBuilder::BuildInitializeElementsHeader(HValue* elements,
                                                  ElementsKind kind,
                                                  HValue* capacity) {
  Factory* factory = isolate()->factory();
  Handle<Map> map = IsFastDoubleElementsKind(kind)
      ? factory->fixed_double_array_map()
      : factory->fixed_array_map();

  AddStoreMapConstant(elements, map);
  Add<HStoreNamedField>(elements, HObjectAccess::ForFixedArrayLength(),
                        capacity);
}


HValue* HGraphBuilder::BuildAllocateElementsAndInitializeElementsHeader(
    ElementsKind kind,
    HValue* capacity) {
  // The HForceRepresentation is to prevent possible deopt on int-smi
  // conversion after allocation but before the new object fields are set.
  capacity = AddUncasted<HForceRepresentation>(capacity, Representation::Smi());
  HValue* new_elements = BuildAllocateElements(kind, capacity);
  BuildInitializeElementsHeader(new_elements, kind, capacity);
  return new_elements;
}


HInnerAllocatedObject* HGraphBuilder::BuildJSArrayHeader(HValue* array,
    HValue* array_map,
    AllocationSiteMode mode,
    ElementsKind elements_kind,
    HValue* allocation_site_payload,
    HValue* length_field) {

  Add<HStoreNamedField>(array, HObjectAccess::ForMap(), array_map);

  HConstant* empty_fixed_array =
    Add<HConstant>(isolate()->factory()->empty_fixed_array());

  HObjectAccess access = HObjectAccess::ForPropertiesPointer();
  Add<HStoreNamedField>(array, access, empty_fixed_array);
  Add<HStoreNamedField>(array, HObjectAccess::ForArrayLength(elements_kind),
                        length_field);

  if (mode == TRACK_ALLOCATION_SITE) {
    BuildCreateAllocationMemento(
        array, Add<HConstant>(JSArray::kSize), allocation_site_payload);
  }

  int elements_location = JSArray::kSize;
  if (mode == TRACK_ALLOCATION_SITE) {
    elements_location += AllocationMemento::kSize;
  }

  HInnerAllocatedObject* elements = Add<HInnerAllocatedObject>(
      array, Add<HConstant>(elements_location));
  Add<HStoreNamedField>(array, HObjectAccess::ForElementsPointer(), elements);
  return elements;
}


HInstruction* HGraphBuilder::AddElementAccess(
    HValue* elements,
    HValue* checked_key,
    HValue* val,
    HValue* dependency,
    ElementsKind elements_kind,
    PropertyAccessType access_type,
    LoadKeyedHoleMode load_mode) {
  if (access_type == STORE) {
    ASSERT(val != NULL);
    if (elements_kind == EXTERNAL_UINT8_CLAMPED_ELEMENTS ||
        elements_kind == UINT8_CLAMPED_ELEMENTS) {
      val = Add<HClampToUint8>(val);
    }
    return Add<HStoreKeyed>(elements, checked_key, val, elements_kind,
                            elements_kind == FAST_SMI_ELEMENTS
                              ? STORE_TO_INITIALIZED_ENTRY
                              : INITIALIZING_STORE);
  }

  ASSERT(access_type == LOAD);
  ASSERT(val == NULL);
  HLoadKeyed* load = Add<HLoadKeyed>(
      elements, checked_key, dependency, elements_kind, load_mode);
  if (FLAG_opt_safe_uint32_operations &&
      (elements_kind == EXTERNAL_UINT32_ELEMENTS ||
       elements_kind == UINT32_ELEMENTS)) {
    graph()->RecordUint32Instruction(load);
  }
  return load;
}


HLoadNamedField* HGraphBuilder::AddLoadElements(HValue* object) {
  return Add<HLoadNamedField>(
      object, static_cast<HValue*>(NULL), HObjectAccess::ForElementsPointer());
}


HLoadNamedField* HGraphBuilder::AddLoadFixedArrayLength(HValue* object) {
  return Add<HLoadNamedField>(
      object, static_cast<HValue*>(NULL), HObjectAccess::ForFixedArrayLength());
}


HValue* HGraphBuilder::BuildNewElementsCapacity(HValue* old_capacity) {
  HValue* half_old_capacity = AddUncasted<HShr>(old_capacity,
                                                graph_->GetConstant1());

  HValue* new_capacity = AddUncasted<HAdd>(half_old_capacity, old_capacity);
  new_capacity->ClearFlag(HValue::kCanOverflow);

  HValue* min_growth = Add<HConstant>(16);

  new_capacity = AddUncasted<HAdd>(new_capacity, min_growth);
  new_capacity->ClearFlag(HValue::kCanOverflow);

  return new_capacity;
}


void HGraphBuilder::BuildNewSpaceArrayCheck(HValue* length, ElementsKind kind) {
  int element_size = IsFastDoubleElementsKind(kind) ? kDoubleSize
                                                    : kPointerSize;
  int max_size = Page::kMaxRegularHeapObjectSize / element_size;
  max_size -= JSArray::kSize / element_size;
  HConstant* max_size_constant = Add<HConstant>(max_size);
  Add<HBoundsCheck>(length, max_size_constant);
}


HValue* HGraphBuilder::BuildGrowElementsCapacity(HValue* object,
                                                 HValue* elements,
                                                 ElementsKind kind,
                                                 ElementsKind new_kind,
                                                 HValue* length,
                                                 HValue* new_capacity) {
  BuildNewSpaceArrayCheck(new_capacity, new_kind);

  HValue* new_elements = BuildAllocateElementsAndInitializeElementsHeader(
      new_kind, new_capacity);

  BuildCopyElements(elements, kind,
                    new_elements, new_kind,
                    length, new_capacity);

  Add<HStoreNamedField>(object, HObjectAccess::ForElementsPointer(),
                        new_elements);

  return new_elements;
}


void HGraphBuilder::BuildFillElementsWithHole(HValue* elements,
                                              ElementsKind elements_kind,
                                              HValue* from,
                                              HValue* to) {
  // Fast elements kinds need to be initialized in case statements below cause
  // a garbage collection.
  Factory* factory = isolate()->factory();

  double nan_double = FixedDoubleArray::hole_nan_as_double();
  HValue* hole = IsFastSmiOrObjectElementsKind(elements_kind)
      ? Add<HConstant>(factory->the_hole_value())
      : Add<HConstant>(nan_double);

  // Special loop unfolding case
  static const int kLoopUnfoldLimit = 8;
  STATIC_ASSERT(JSArray::kPreallocatedArrayElements <= kLoopUnfoldLimit);
  int initial_capacity = -1;
  if (from->IsInteger32Constant() && to->IsInteger32Constant()) {
    int constant_from = from->GetInteger32Constant();
    int constant_to = to->GetInteger32Constant();

    if (constant_from == 0 && constant_to <= kLoopUnfoldLimit) {
      initial_capacity = constant_to;
    }
  }

  // Since we're about to store a hole value, the store instruction below must
  // assume an elements kind that supports heap object values.
  if (IsFastSmiOrObjectElementsKind(elements_kind)) {
    elements_kind = FAST_HOLEY_ELEMENTS;
  }

  if (initial_capacity >= 0) {
    for (int i = 0; i < initial_capacity; i++) {
      HInstruction* key = Add<HConstant>(i);
      Add<HStoreKeyed>(elements, key, hole, elements_kind);
    }
  } else {
    LoopBuilder builder(this, context(), LoopBuilder::kPostIncrement);

    HValue* key = builder.BeginBody(from, to, Token::LT);

    Add<HStoreKeyed>(elements, key, hole, elements_kind);

    builder.EndBody();
  }
}


void HGraphBuilder::BuildCopyElements(HValue* from_elements,
                                      ElementsKind from_elements_kind,
                                      HValue* to_elements,
                                      ElementsKind to_elements_kind,
                                      HValue* length,
                                      HValue* capacity) {
  bool pre_fill_with_holes =
      IsFastDoubleElementsKind(from_elements_kind) &&
      IsFastObjectElementsKind(to_elements_kind);

  if (pre_fill_with_holes) {
    // If the copy might trigger a GC, make sure that the FixedArray is
    // pre-initialized with holes to make sure that it's always in a consistent
    // state.
    BuildFillElementsWithHole(to_elements, to_elements_kind,
                              graph()->GetConstant0(), capacity);
  }

  LoopBuilder builder(this, context(), LoopBuilder::kPostIncrement);

  HValue* key = builder.BeginBody(graph()->GetConstant0(), length, Token::LT);

  HValue* element = Add<HLoadKeyed>(from_elements, key,
                                    static_cast<HValue*>(NULL),
                                    from_elements_kind,
                                    ALLOW_RETURN_HOLE);

  ElementsKind kind = (IsHoleyElementsKind(from_elements_kind) &&
                       IsFastSmiElementsKind(to_elements_kind))
      ? FAST_HOLEY_ELEMENTS : to_elements_kind;

  if (IsHoleyElementsKind(from_elements_kind) &&
      from_elements_kind != to_elements_kind) {
    IfBuilder if_hole(this);
    if_hole.If<HCompareHoleAndBranch>(element);
    if_hole.Then();
    HConstant* hole_constant = IsFastDoubleElementsKind(to_elements_kind)
        ? Add<HConstant>(FixedDoubleArray::hole_nan_as_double())
        : graph()->GetConstantHole();
    Add<HStoreKeyed>(to_elements, key, hole_constant, kind);
    if_hole.Else();
    HStoreKeyed* store = Add<HStoreKeyed>(to_elements, key, element, kind);
    store->SetFlag(HValue::kAllowUndefinedAsNaN);
    if_hole.End();
  } else {
    HStoreKeyed* store = Add<HStoreKeyed>(to_elements, key, element, kind);
    store->SetFlag(HValue::kAllowUndefinedAsNaN);
  }

  builder.EndBody();

  if (!pre_fill_with_holes && length != capacity) {
    // Fill unused capacity with the hole.
    BuildFillElementsWithHole(to_elements, to_elements_kind,
                              key, capacity);
  }
}


HValue* HGraphBuilder::BuildCloneShallowArray(HValue* boilerplate,
                                              HValue* allocation_site,
                                              AllocationSiteMode mode,
                                              ElementsKind kind,
                                              int length) {
  NoObservableSideEffectsScope no_effects(this);

  // All sizes here are multiples of kPointerSize.
  int size = JSArray::kSize;
  if (mode == TRACK_ALLOCATION_SITE) {
    size += AllocationMemento::kSize;
  }

  HValue* size_in_bytes = Add<HConstant>(size);
  HInstruction* object = Add<HAllocate>(size_in_bytes,
                                        HType::JSObject(),
                                        NOT_TENURED,
                                        JS_OBJECT_TYPE);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length == 0)) {
      HObjectAccess access = HObjectAccess::ForJSArrayOffset(i);
      Add<HStoreNamedField>(
          object, access, Add<HLoadNamedField>(
              boilerplate, static_cast<HValue*>(NULL), access));
    }
  }

  // Create an allocation site info if requested.
  if (mode == TRACK_ALLOCATION_SITE) {
    BuildCreateAllocationMemento(
        object, Add<HConstant>(JSArray::kSize), allocation_site);
  }

  if (length > 0) {
    HValue* boilerplate_elements = AddLoadElements(boilerplate);
    HValue* object_elements;
    if (IsFastDoubleElementsKind(kind)) {
      HValue* elems_size = Add<HConstant>(FixedDoubleArray::SizeFor(length));
      object_elements = Add<HAllocate>(elems_size, HType::JSArray(),
          NOT_TENURED, FIXED_DOUBLE_ARRAY_TYPE);
    } else {
      HValue* elems_size = Add<HConstant>(FixedArray::SizeFor(length));
      object_elements = Add<HAllocate>(elems_size, HType::JSArray(),
          NOT_TENURED, FIXED_ARRAY_TYPE);
    }
    Add<HStoreNamedField>(object, HObjectAccess::ForElementsPointer(),
                          object_elements);

    // Copy the elements array header.
    for (int i = 0; i < FixedArrayBase::kHeaderSize; i += kPointerSize) {
      HObjectAccess access = HObjectAccess::ForFixedArrayHeader(i);
      Add<HStoreNamedField>(
          object_elements, access, Add<HLoadNamedField>(
              boilerplate_elements, static_cast<HValue*>(NULL), access));
    }

    // Copy the elements array contents.
    // TODO(mstarzinger): Teach HGraphBuilder::BuildCopyElements to unfold
    // copying loops with constant length up to a given boundary and use this
    // helper here instead.
    for (int i = 0; i < length; i++) {
      HValue* key_constant = Add<HConstant>(i);
      HInstruction* value = Add<HLoadKeyed>(boilerplate_elements, key_constant,
                                            static_cast<HValue*>(NULL), kind);
      Add<HStoreKeyed>(object_elements, key_constant, value, kind);
    }
  }

  return object;
}


void HGraphBuilder::BuildCompareNil(
    HValue* value,
    Type* type,
    HIfContinuation* continuation) {
  IfBuilder if_nil(this);
  bool some_case_handled = false;
  bool some_case_missing = false;

  if (type->Maybe(Type::Null())) {
    if (some_case_handled) if_nil.Or();
    if_nil.If<HCompareObjectEqAndBranch>(value, graph()->GetConstantNull());
    some_case_handled = true;
  } else {
    some_case_missing = true;
  }

  if (type->Maybe(Type::Undefined())) {
    if (some_case_handled) if_nil.Or();
    if_nil.If<HCompareObjectEqAndBranch>(value,
                                         graph()->GetConstantUndefined());
    some_case_handled = true;
  } else {
    some_case_missing = true;
  }

  if (type->Maybe(Type::Undetectable())) {
    if (some_case_handled) if_nil.Or();
    if_nil.If<HIsUndetectableAndBranch>(value);
    some_case_handled = true;
  } else {
    some_case_missing = true;
  }

  if (some_case_missing) {
    if_nil.Then();
    if_nil.Else();
    if (type->NumClasses() == 1) {
      BuildCheckHeapObject(value);
      // For ICs, the map checked below is a sentinel map that gets replaced by
      // the monomorphic map when the code is used as a template to generate a
      // new IC. For optimized functions, there is no sentinel map, the map
      // emitted below is the actual monomorphic map.
      BuildCheckMap(value, type->Classes().Current());
    } else {
      if_nil.Deopt("Too many undetectable types");
    }
  }

  if_nil.CaptureContinuation(continuation);
}


void HGraphBuilder::BuildCreateAllocationMemento(
    HValue* previous_object,
    HValue* previous_object_size,
    HValue* allocation_site) {
  ASSERT(allocation_site != NULL);
  HInnerAllocatedObject* allocation_memento = Add<HInnerAllocatedObject>(
      previous_object, previous_object_size);
  AddStoreMapConstant(
      allocation_memento, isolate()->factory()->allocation_memento_map());
  Add<HStoreNamedField>(
      allocation_memento,
      HObjectAccess::ForAllocationMementoSite(),
      allocation_site);
  if (FLAG_allocation_site_pretenuring) {
    HValue* memento_create_count = Add<HLoadNamedField>(
        allocation_site, static_cast<HValue*>(NULL),
        HObjectAccess::ForAllocationSiteOffset(
            AllocationSite::kPretenureCreateCountOffset));
    memento_create_count = AddUncasted<HAdd>(
        memento_create_count, graph()->GetConstant1());
    // This smi value is reset to zero after every gc, overflow isn't a problem
    // since the counter is bounded by the new space size.
    memento_create_count->ClearFlag(HValue::kCanOverflow);
    HStoreNamedField* store = Add<HStoreNamedField>(
        allocation_site, HObjectAccess::ForAllocationSiteOffset(
            AllocationSite::kPretenureCreateCountOffset), memento_create_count);
    // No write barrier needed to store a smi.
    store->SkipWriteBarrier();
  }
}


HInstruction* HGraphBuilder::BuildGetNativeContext(HValue* closure) {
  // Get the global context, then the native context
  HInstruction* context =
      Add<HLoadNamedField>(closure, static_cast<HValue*>(NULL),
                           HObjectAccess::ForFunctionContextPointer());
  HInstruction* global_object = Add<HLoadNamedField>(
      context, static_cast<HValue*>(NULL),
      HObjectAccess::ForContextSlot(Context::GLOBAL_OBJECT_INDEX));
  HObjectAccess access = HObjectAccess::ForObservableJSObjectOffset(
      GlobalObject::kNativeContextOffset);
  return Add<HLoadNamedField>(
      global_object, static_cast<HValue*>(NULL), access);
}


HInstruction* HGraphBuilder::BuildGetNativeContext() {
  // Get the global context, then the native context
  HValue* global_object = Add<HLoadNamedField>(
      context(), static_cast<HValue*>(NULL),
      HObjectAccess::ForContextSlot(Context::GLOBAL_OBJECT_INDEX));
  return Add<HLoadNamedField>(
      global_object, static_cast<HValue*>(NULL),
      HObjectAccess::ForObservableJSObjectOffset(
          GlobalObject::kNativeContextOffset));
}


HInstruction* HGraphBuilder::BuildGetArrayFunction() {
  HInstruction* native_context = BuildGetNativeContext();
  HInstruction* index =
      Add<HConstant>(static_cast<int32_t>(Context::ARRAY_FUNCTION_INDEX));
  return Add<HLoadKeyed>(
      native_context, index, static_cast<HValue*>(NULL), FAST_ELEMENTS);
}


HGraphBuilder::JSArrayBuilder::JSArrayBuilder(HGraphBuilder* builder,
    ElementsKind kind,
    HValue* allocation_site_payload,
    HValue* constructor_function,
    AllocationSiteOverrideMode override_mode) :
        builder_(builder),
        kind_(kind),
        allocation_site_payload_(allocation_site_payload),
        constructor_function_(constructor_function) {
  ASSERT(!allocation_site_payload->IsConstant() ||
         HConstant::cast(allocation_site_payload)->handle(
             builder_->isolate())->IsAllocationSite());
  mode_ = override_mode == DISABLE_ALLOCATION_SITES
      ? DONT_TRACK_ALLOCATION_SITE
      : AllocationSite::GetMode(kind);
}


HGraphBuilder::JSArrayBuilder::JSArrayBuilder(HGraphBuilder* builder,
                                              ElementsKind kind,
                                              HValue* constructor_function) :
    builder_(builder),
    kind_(kind),
    mode_(DONT_TRACK_ALLOCATION_SITE),
    allocation_site_payload_(NULL),
    constructor_function_(constructor_function) {
}


HValue* HGraphBuilder::JSArrayBuilder::EmitMapCode() {
  if (!builder()->top_info()->IsStub()) {
    // A constant map is fine.
    Handle<Map> map(builder()->isolate()->get_initial_js_array_map(kind_),
                    builder()->isolate());
    return builder()->Add<HConstant>(map);
  }

  if (constructor_function_ != NULL && kind_ == GetInitialFastElementsKind()) {
    // No need for a context lookup if the kind_ matches the initial
    // map, because we can just load the map in that case.
    HObjectAccess access = HObjectAccess::ForPrototypeOrInitialMap();
    return builder()->Add<HLoadNamedField>(
        constructor_function_, static_cast<HValue*>(NULL), access);
  }

  // TODO(mvstanton): we should always have a constructor function if we
  // are creating a stub.
  HInstruction* native_context = constructor_function_ != NULL
      ? builder()->BuildGetNativeContext(constructor_function_)
      : builder()->BuildGetNativeContext();

  HInstruction* index = builder()->Add<HConstant>(
      static_cast<int32_t>(Context::JS_ARRAY_MAPS_INDEX));

  HInstruction* map_array = builder()->Add<HLoadKeyed>(
      native_context, index, static_cast<HValue*>(NULL), FAST_ELEMENTS);

  HInstruction* kind_index = builder()->Add<HConstant>(kind_);

  return builder()->Add<HLoadKeyed>(
      map_array, kind_index, static_cast<HValue*>(NULL), FAST_ELEMENTS);
}


HValue* HGraphBuilder::JSArrayBuilder::EmitInternalMapCode() {
  // Find the map near the constructor function
  HObjectAccess access = HObjectAccess::ForPrototypeOrInitialMap();
  return builder()->Add<HLoadNamedField>(
      constructor_function_, static_cast<HValue*>(NULL), access);
}


HValue* HGraphBuilder::JSArrayBuilder::EstablishAllocationSize(
    HValue* length_node) {
  ASSERT(length_node != NULL);

  int base_size = JSArray::kSize;
  if (mode_ == TRACK_ALLOCATION_SITE) {
    base_size += AllocationMemento::kSize;
  }

  STATIC_ASSERT(FixedDoubleArray::kHeaderSize == FixedArray::kHeaderSize);
  base_size += FixedArray::kHeaderSize;

  HInstruction* elements_size_value =
      builder()->Add<HConstant>(elements_size());
  HInstruction* mul = HMul::NewImul(builder()->zone(), builder()->context(),
                                    length_node, elements_size_value);
  builder()->AddInstruction(mul);
  HInstruction* base = builder()->Add<HConstant>(base_size);
  HInstruction* total_size = HAdd::New(builder()->zone(), builder()->context(),
                                       base, mul);
  total_size->ClearFlag(HValue::kCanOverflow);
  builder()->AddInstruction(total_size);
  return total_size;
}


HValue* HGraphBuilder::JSArrayBuilder::EstablishEmptyArrayAllocationSize() {
  int base_size = JSArray::kSize;
  if (mode_ == TRACK_ALLOCATION_SITE) {
    base_size += AllocationMemento::kSize;
  }

  base_size += IsFastDoubleElementsKind(kind_)
      ? FixedDoubleArray::SizeFor(initial_capacity())
      : FixedArray::SizeFor(initial_capacity());

  return builder()->Add<HConstant>(base_size);
}


HValue* HGraphBuilder::JSArrayBuilder::AllocateEmptyArray() {
  HValue* size_in_bytes = EstablishEmptyArrayAllocationSize();
  HConstant* capacity = builder()->Add<HConstant>(initial_capacity());
  return AllocateArray(size_in_bytes,
                       capacity,
                       builder()->graph()->GetConstant0());
}


HValue* HGraphBuilder::JSArrayBuilder::AllocateArray(HValue* capacity,
                                                     HValue* length_field,
                                                     FillMode fill_mode) {
  HValue* size_in_bytes = EstablishAllocationSize(capacity);
  return AllocateArray(size_in_bytes, capacity, length_field, fill_mode);
}


HValue* HGraphBuilder::JSArrayBuilder::AllocateArray(HValue* size_in_bytes,
                                                     HValue* capacity,
                                                     HValue* length_field,
                                                     FillMode fill_mode) {
  // These HForceRepresentations are because we store these as fields in the
  // objects we construct, and an int32-to-smi HChange could deopt. Accept
  // the deopt possibility now, before allocation occurs.
  capacity =
      builder()->AddUncasted<HForceRepresentation>(capacity,
                                                   Representation::Smi());
  length_field =
      builder()->AddUncasted<HForceRepresentation>(length_field,
                                                   Representation::Smi());
  // Allocate (dealing with failure appropriately)
  HAllocate* new_object = builder()->Add<HAllocate>(size_in_bytes,
      HType::JSArray(), NOT_TENURED, JS_ARRAY_TYPE);

  // Folded array allocation should be aligned if it has fast double elements.
  if (IsFastDoubleElementsKind(kind_)) {
     new_object->MakeDoubleAligned();
  }

  // Fill in the fields: map, properties, length
  HValue* map;
  if (allocation_site_payload_ == NULL) {
    map = EmitInternalMapCode();
  } else {
    map = EmitMapCode();
  }
  elements_location_ = builder()->BuildJSArrayHeader(new_object,
                                                     map,
                                                     mode_,
                                                     kind_,
                                                     allocation_site_payload_,
                                                     length_field);

  // Initialize the elements
  builder()->BuildInitializeElementsHeader(elements_location_, kind_, capacity);

  if (fill_mode == FILL_WITH_HOLE) {
    builder()->BuildFillElementsWithHole(elements_location_, kind_,
                                         graph()->GetConstant0(), capacity);
  }

  return new_object;
}


HStoreNamedField* HGraphBuilder::AddStoreMapConstant(HValue *object,
                                                     Handle<Map> map) {
  return Add<HStoreNamedField>(object, HObjectAccess::ForMap(),
                               Add<HConstant>(map));
}


HValue* HGraphBuilder::AddLoadJSBuiltin(Builtins::JavaScript builtin) {
  HValue* global_object = Add<HLoadNamedField>(
      context(), static_cast<HValue*>(NULL),
      HObjectAccess::ForContextSlot(Context::GLOBAL_OBJECT_INDEX));
  HObjectAccess access = HObjectAccess::ForObservableJSObjectOffset(
      GlobalObject::kBuiltinsOffset);
  HValue* builtins = Add<HLoadNamedField>(
      global_object, static_cast<HValue*>(NULL), access);
  HObjectAccess function_access = HObjectAccess::ForObservableJSObjectOffset(
          JSBuiltinsObject::OffsetOfFunctionWithId(builtin));
  return Add<HLoadNamedField>(
      builtins, static_cast<HValue*>(NULL), function_access);
}


HOptimizedGraphBuilder::HOptimizedGraphBuilder(CompilationInfo* info)
    : HGraphBuilder(info),
      function_state_(NULL),
      initial_function_state_(this, info, NORMAL_RETURN, 0),
      ast_context_(NULL),
      break_scope_(NULL),
      inlined_count_(0),
      globals_(10, info->zone()),
      inline_bailout_(false),
      osr_(new(info->zone()) HOsrBuilder(this)) {
  // This is not initialized in the initializer list because the
  // constructor for the initial state relies on function_state_ == NULL
  // to know it's the initial state.
  function_state_= &initial_function_state_;
  InitializeAstVisitor(info->zone());
  if (FLAG_hydrogen_track_positions) {
    SetSourcePosition(info->shared_info()->start_position());
  }
}


HBasicBlock* HOptimizedGraphBuilder::CreateJoin(HBasicBlock* first,
                                                HBasicBlock* second,
                                                BailoutId join_id) {
  if (first == NULL) {
    return second;
  } else if (second == NULL) {
    return first;
  } else {
    HBasicBlock* join_block = graph()->CreateBasicBlock();
    Goto(first, join_block);
    Goto(second, join_block);
    join_block->SetJoinId(join_id);
    return join_block;
  }
}


HBasicBlock* HOptimizedGraphBuilder::JoinContinue(IterationStatement* statement,
                                                  HBasicBlock* exit_block,
                                                  HBasicBlock* continue_block) {
  if (continue_block != NULL) {
    if (exit_block != NULL) Goto(exit_block, continue_block);
    continue_block->SetJoinId(statement->ContinueId());
    return continue_block;
  }
  return exit_block;
}


HBasicBlock* HOptimizedGraphBuilder::CreateLoop(IterationStatement* statement,
                                                HBasicBlock* loop_entry,
                                                HBasicBlock* body_exit,
                                                HBasicBlock* loop_successor,
                                                HBasicBlock* break_block) {
  if (body_exit != NULL) Goto(body_exit, loop_entry);
  loop_entry->PostProcessLoopHeader(statement);
  if (break_block != NULL) {
    if (loop_successor != NULL) Goto(loop_successor, break_block);
    break_block->SetJoinId(statement->ExitId());
    return break_block;
  }
  return loop_successor;
}


// Build a new loop header block and set it as the current block.
HBasicBlock* HOptimizedGraphBuilder::BuildLoopEntry() {
  HBasicBlock* loop_entry = CreateLoopHeaderBlock();
  Goto(loop_entry);
  set_current_block(loop_entry);
  return loop_entry;
}


HBasicBlock* HOptimizedGraphBuilder::BuildLoopEntry(
    IterationStatement* statement) {
  HBasicBlock* loop_entry = osr()->HasOsrEntryAt(statement)
      ? osr()->BuildOsrLoopEntry(statement)
      : BuildLoopEntry();
  return loop_entry;
}


void HBasicBlock::FinishExit(HControlInstruction* instruction,
                             HSourcePosition position) {
  Finish(instruction, position);
  ClearEnvironment();
}


HGraph::HGraph(CompilationInfo* info)
    : isolate_(info->isolate()),
      next_block_id_(0),
      entry_block_(NULL),
      blocks_(8, info->zone()),
      values_(16, info->zone()),
      phi_list_(NULL),
      uint32_instructions_(NULL),
      osr_(NULL),
      info_(info),
      zone_(info->zone()),
      is_recursive_(false),
      use_optimistic_licm_(false),
      depends_on_empty_array_proto_elements_(false),
      type_change_checksum_(0),
      maximum_environment_size_(0),
      no_side_effects_scope_count_(0),
      disallow_adding_new_values_(false),
      next_inline_id_(0),
      inlined_functions_(5, info->zone()) {
  if (info->IsStub()) {
    HydrogenCodeStub* stub = info->code_stub();
    CodeStubInterfaceDescriptor* descriptor =
        stub->GetInterfaceDescriptor(isolate_);
    start_environment_ =
        new(zone_) HEnvironment(zone_, descriptor->environment_length());
  } else {
    TraceInlinedFunction(info->shared_info(), HSourcePosition::Unknown());
    start_environment_ =
        new(zone_) HEnvironment(NULL, info->scope(), info->closure(), zone_);
  }
  start_environment_->set_ast_id(BailoutId::FunctionEntry());
  entry_block_ = CreateBasicBlock();
  entry_block_->SetInitialEnvironment(start_environment_);
}


HBasicBlock* HGraph::CreateBasicBlock() {
  HBasicBlock* result = new(zone()) HBasicBlock(this);
  blocks_.Add(result, zone());
  return result;
}


void HGraph::FinalizeUniqueness() {
  DisallowHeapAllocation no_gc;
  ASSERT(!OptimizingCompilerThread::IsOptimizerThread(isolate()));
  for (int i = 0; i < blocks()->length(); ++i) {
    for (HInstructionIterator it(blocks()->at(i)); !it.Done(); it.Advance()) {
      it.Current()->FinalizeUniqueness();
    }
  }
}


int HGraph::TraceInlinedFunction(
    Handle<SharedFunctionInfo> shared,
    HSourcePosition position) {
  if (!FLAG_hydrogen_track_positions) {
    return 0;
  }

  int id = 0;
  for (; id < inlined_functions_.length(); id++) {
    if (inlined_functions_[id].shared().is_identical_to(shared)) {
      break;
    }
  }

  if (id == inlined_functions_.length()) {
    inlined_functions_.Add(InlinedFunctionInfo(shared), zone());

    if (!shared->script()->IsUndefined()) {
      Handle<Script> script(Script::cast(shared->script()));
      if (!script->source()->IsUndefined()) {
        CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
        PrintF(tracing_scope.file(),
               "--- FUNCTION SOURCE (%s) id{%d,%d} ---\n",
               shared->DebugName()->ToCString().get(),
               info()->optimization_id(),
               id);

        {
          ConsStringIteratorOp op;
          StringCharacterStream stream(String::cast(script->source()),
                                       &op,
                                       shared->start_position());
          // fun->end_position() points to the last character in the stream. We
          // need to compensate by adding one to calculate the length.
          int source_len =
              shared->end_position() - shared->start_position() + 1;
          for (int i = 0; i < source_len; i++) {
            if (stream.HasMore()) {
              PrintF(tracing_scope.file(), "%c", stream.GetNext());
            }
          }
        }

        PrintF(tracing_scope.file(), "\n--- END ---\n");
      }
    }
  }

  int inline_id = next_inline_id_++;

  if (inline_id != 0) {
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    PrintF(tracing_scope.file(), "INLINE (%s) id{%d,%d} AS %d AT ",
           shared->DebugName()->ToCString().get(),
           info()->optimization_id(),
           id,
           inline_id);
    position.PrintTo(tracing_scope.file());
    PrintF(tracing_scope.file(), "\n");
  }

  return inline_id;
}


int HGraph::SourcePositionToScriptPosition(HSourcePosition pos) {
  if (!FLAG_hydrogen_track_positions || pos.IsUnknown()) {
    return pos.raw();
  }

  return inlined_functions_[pos.inlining_id()].start_position() +
      pos.position();
}


// Block ordering was implemented with two mutually recursive methods,
// HGraph::Postorder and HGraph::PostorderLoopBlocks.
// The recursion could lead to stack overflow so the algorithm has been
// implemented iteratively.
// At a high level the algorithm looks like this:
//
// Postorder(block, loop_header) : {
//   if (block has already been visited or is of another loop) return;
//   mark block as visited;
//   if (block is a loop header) {
//     VisitLoopMembers(block, loop_header);
//     VisitSuccessorsOfLoopHeader(block);
//   } else {
//     VisitSuccessors(block)
//   }
//   put block in result list;
// }
//
// VisitLoopMembers(block, outer_loop_header) {
//   foreach (block b in block loop members) {
//     VisitSuccessorsOfLoopMember(b, outer_loop_header);
//     if (b is loop header) VisitLoopMembers(b);
//   }
// }
//
// VisitSuccessorsOfLoopMember(block, outer_loop_header) {
//   foreach (block b in block successors) Postorder(b, outer_loop_header)
// }
//
// VisitSuccessorsOfLoopHeader(block) {
//   foreach (block b in block successors) Postorder(b, block)
// }
//
// VisitSuccessors(block, loop_header) {
//   foreach (block b in block successors) Postorder(b, loop_header)
// }
//
// The ordering is started calling Postorder(entry, NULL).
//
// Each instance of PostorderProcessor represents the "stack frame" of the
// recursion, and particularly keeps the state of the loop (iteration) of the
// "Visit..." function it represents.
// To recycle memory we keep all the frames in a double linked list but
// this means that we cannot use constructors to initialize the frames.
//
class PostorderProcessor : public ZoneObject {
 public:
  // Back link (towards the stack bottom).
  PostorderProcessor* parent() {return father_; }
  // Forward link (towards the stack top).
  PostorderProcessor* child() {return child_; }
  HBasicBlock* block() { return block_; }
  HLoopInformation* loop() { return loop_; }
  HBasicBlock* loop_header() { return loop_header_; }

  static PostorderProcessor* CreateEntryProcessor(Zone* zone,
                                                  HBasicBlock* block,
                                                  BitVector* visited) {
    PostorderProcessor* result = new(zone) PostorderProcessor(NULL);
    return result->SetupSuccessors(zone, block, NULL, visited);
  }

  PostorderProcessor* PerformStep(Zone* zone,
                                  BitVector* visited,
                                  ZoneList<HBasicBlock*>* order) {
    PostorderProcessor* next =
        PerformNonBacktrackingStep(zone, visited, order);
    if (next != NULL) {
      return next;
    } else {
      return Backtrack(zone, visited, order);
    }
  }

 private:
  explicit PostorderProcessor(PostorderProcessor* father)
      : father_(father), child_(NULL), successor_iterator(NULL) { }

  // Each enum value states the cycle whose state is kept by this instance.
  enum LoopKind {
    NONE,
    SUCCESSORS,
    SUCCESSORS_OF_LOOP_HEADER,
    LOOP_MEMBERS,
    SUCCESSORS_OF_LOOP_MEMBER
  };

  // Each "Setup..." method is like a constructor for a cycle state.
  PostorderProcessor* SetupSuccessors(Zone* zone,
                                      HBasicBlock* block,
                                      HBasicBlock* loop_header,
                                      BitVector* visited) {
    if (block == NULL || visited->Contains(block->block_id()) ||
        block->parent_loop_header() != loop_header) {
      kind_ = NONE;
      block_ = NULL;
      loop_ = NULL;
      loop_header_ = NULL;
      return this;
    } else {
      block_ = block;
      loop_ = NULL;
      visited->Add(block->block_id());

      if (block->IsLoopHeader()) {
        kind_ = SUCCESSORS_OF_LOOP_HEADER;
        loop_header_ = block;
        InitializeSuccessors();
        PostorderProcessor* result = Push(zone);
        return result->SetupLoopMembers(zone, block, block->loop_information(),
                                        loop_header);
      } else {
        ASSERT(block->IsFinished());
        kind_ = SUCCESSORS;
        loop_header_ = loop_header;
        InitializeSuccessors();
        return this;
      }
    }
  }

  PostorderProcessor* SetupLoopMembers(Zone* zone,
                                       HBasicBlock* block,
                                       HLoopInformation* loop,
                                       HBasicBlock* loop_header) {
    kind_ = LOOP_MEMBERS;
    block_ = block;
    loop_ = loop;
    loop_header_ = loop_header;
    InitializeLoopMembers();
    return this;
  }

  PostorderProcessor* SetupSuccessorsOfLoopMember(
      HBasicBlock* block,
      HLoopInformation* loop,
      HBasicBlock* loop_header) {
    kind_ = SUCCESSORS_OF_LOOP_MEMBER;
    block_ = block;
    loop_ = loop;
    loop_header_ = loop_header;
    InitializeSuccessors();
    return this;
  }

  // This method "allocates" a new stack frame.
  PostorderProcessor* Push(Zone* zone) {
    if (child_ == NULL) {
      child_ = new(zone) PostorderProcessor(this);
    }
    return child_;
  }

  void ClosePostorder(ZoneList<HBasicBlock*>* order, Zone* zone) {
    ASSERT(block_->end()->FirstSuccessor() == NULL ||
           order->Contains(block_->end()->FirstSuccessor()) ||
           block_->end()->FirstSuccessor()->IsLoopHeader());
    ASSERT(block_->end()->SecondSuccessor() == NULL ||
           order->Contains(block_->end()->SecondSuccessor()) ||
           block_->end()->SecondSuccessor()->IsLoopHeader());
    order->Add(block_, zone);
  }

  // This method is the basic block to walk up the stack.
  PostorderProcessor* Pop(Zone* zone,
                          BitVector* visited,
                          ZoneList<HBasicBlock*>* order) {
    switch (kind_) {
      case SUCCESSORS:
      case SUCCESSORS_OF_LOOP_HEADER:
        ClosePostorder(order, zone);
        return father_;
      case LOOP_MEMBERS:
        return father_;
      case SUCCESSORS_OF_LOOP_MEMBER:
        if (block()->IsLoopHeader() && block() != loop_->loop_header()) {
          // In this case we need to perform a LOOP_MEMBERS cycle so we
          // initialize it and return this instead of father.
          return SetupLoopMembers(zone, block(),
                                  block()->loop_information(), loop_header_);
        } else {
          return father_;
        }
      case NONE:
        return father_;
    }
    UNREACHABLE();
    return NULL;
  }

  // Walks up the stack.
  PostorderProcessor* Backtrack(Zone* zone,
                                BitVector* visited,
                                ZoneList<HBasicBlock*>* order) {
    PostorderProcessor* parent = Pop(zone, visited, order);
    while (parent != NULL) {
      PostorderProcessor* next =
          parent->PerformNonBacktrackingStep(zone, visited, order);
      if (next != NULL) {
        return next;
      } else {
        parent = parent->Pop(zone, visited, order);
      }
    }
    return NULL;
  }

  PostorderProcessor* PerformNonBacktrackingStep(
      Zone* zone,
      BitVector* visited,
      ZoneList<HBasicBlock*>* order) {
    HBasicBlock* next_block;
    switch (kind_) {
      case SUCCESSORS:
        next_block = AdvanceSuccessors();
        if (next_block != NULL) {
          PostorderProcessor* result = Push(zone);
          return result->SetupSuccessors(zone, next_block,
                                         loop_header_, visited);
        }
        break;
      case SUCCESSORS_OF_LOOP_HEADER:
        next_block = AdvanceSuccessors();
        if (next_block != NULL) {
          PostorderProcessor* result = Push(zone);
          return result->SetupSuccessors(zone, next_block,
                                         block(), visited);
        }
        break;
      case LOOP_MEMBERS:
        next_block = AdvanceLoopMembers();
        if (next_block != NULL) {
          PostorderProcessor* result = Push(zone);
          return result->SetupSuccessorsOfLoopMember(next_block,
                                                     loop_, loop_header_);
        }
        break;
      case SUCCESSORS_OF_LOOP_MEMBER:
        next_block = AdvanceSuccessors();
        if (next_block != NULL) {
          PostorderProcessor* result = Push(zone);
          return result->SetupSuccessors(zone, next_block,
                                         loop_header_, visited);
        }
        break;
      case NONE:
        return NULL;
    }
    return NULL;
  }

  // The following two methods implement a "foreach b in successors" cycle.
  void InitializeSuccessors() {
    loop_index = 0;
    loop_length = 0;
    successor_iterator = HSuccessorIterator(block_->end());
  }

  HBasicBlock* AdvanceSuccessors() {
    if (!successor_iterator.Done()) {
      HBasicBlock* result = successor_iterator.Current();
      successor_iterator.Advance();
      return result;
    }
    return NULL;
  }

  // The following two methods implement a "foreach b in loop members" cycle.
  void InitializeLoopMembers() {
    loop_index = 0;
    loop_length = loop_->blocks()->length();
  }

  HBasicBlock* AdvanceLoopMembers() {
    if (loop_index < loop_length) {
      HBasicBlock* result = loop_->blocks()->at(loop_index);
      loop_index++;
      return result;
    } else {
      return NULL;
    }
  }

  LoopKind kind_;
  PostorderProcessor* father_;
  PostorderProcessor* child_;
  HLoopInformation* loop_;
  HBasicBlock* block_;
  HBasicBlock* loop_header_;
  int loop_index;
  int loop_length;
  HSuccessorIterator successor_iterator;
};


void HGraph::OrderBlocks() {
  CompilationPhase phase("H_Block ordering", info());
  BitVector visited(blocks_.length(), zone());

  ZoneList<HBasicBlock*> reverse_result(8, zone());
  HBasicBlock* start = blocks_[0];
  PostorderProcessor* postorder =
      PostorderProcessor::CreateEntryProcessor(zone(), start, &visited);
  while (postorder != NULL) {
    postorder = postorder->PerformStep(zone(), &visited, &reverse_result);
  }
  blocks_.Rewind(0);
  int index = 0;
  for (int i = reverse_result.length() - 1; i >= 0; --i) {
    HBasicBlock* b = reverse_result[i];
    blocks_.Add(b, zone());
    b->set_block_id(index++);
  }
}


void HGraph::AssignDominators() {
  HPhase phase("H_Assign dominators", this);
  for (int i = 0; i < blocks_.length(); ++i) {
    HBasicBlock* block = blocks_[i];
    if (block->IsLoopHeader()) {
      // Only the first predecessor of a loop header is from outside the loop.
      // All others are back edges, and thus cannot dominate the loop header.
      block->AssignCommonDominator(block->predecessors()->first());
      block->AssignLoopSuccessorDominators();
    } else {
      for (int j = blocks_[i]->predecessors()->length() - 1; j >= 0; --j) {
        blocks_[i]->AssignCommonDominator(blocks_[i]->predecessors()->at(j));
      }
    }
  }
}


bool HGraph::CheckArgumentsPhiUses() {
  int block_count = blocks_.length();
  for (int i = 0; i < block_count; ++i) {
    for (int j = 0; j < blocks_[i]->phis()->length(); ++j) {
      HPhi* phi = blocks_[i]->phis()->at(j);
      // We don't support phi uses of arguments for now.
      if (phi->CheckFlag(HValue::kIsArguments)) return false;
    }
  }
  return true;
}


bool HGraph::CheckConstPhiUses() {
  int block_count = blocks_.length();
  for (int i = 0; i < block_count; ++i) {
    for (int j = 0; j < blocks_[i]->phis()->length(); ++j) {
      HPhi* phi = blocks_[i]->phis()->at(j);
      // Check for the hole value (from an uninitialized const).
      for (int k = 0; k < phi->OperandCount(); k++) {
        if (phi->OperandAt(k) == GetConstantHole()) return false;
      }
    }
  }
  return true;
}


void HGraph::CollectPhis() {
  int block_count = blocks_.length();
  phi_list_ = new(zone()) ZoneList<HPhi*>(block_count, zone());
  for (int i = 0; i < block_count; ++i) {
    for (int j = 0; j < blocks_[i]->phis()->length(); ++j) {
      HPhi* phi = blocks_[i]->phis()->at(j);
      phi_list_->Add(phi, zone());
    }
  }
}


// Implementation of utility class to encapsulate the translation state for
// a (possibly inlined) function.
FunctionState::FunctionState(HOptimizedGraphBuilder* owner,
                             CompilationInfo* info,
                             InliningKind inlining_kind,
                             int inlining_id)
    : owner_(owner),
      compilation_info_(info),
      call_context_(NULL),
      inlining_kind_(inlining_kind),
      function_return_(NULL),
      test_context_(NULL),
      entry_(NULL),
      arguments_object_(NULL),
      arguments_elements_(NULL),
      inlining_id_(inlining_id),
      outer_source_position_(HSourcePosition::Unknown()),
      outer_(owner->function_state()) {
  if (outer_ != NULL) {
    // State for an inline function.
    if (owner->ast_context()->IsTest()) {
      HBasicBlock* if_true = owner->graph()->CreateBasicBlock();
      HBasicBlock* if_false = owner->graph()->CreateBasicBlock();
      if_true->MarkAsInlineReturnTarget(owner->current_block());
      if_false->MarkAsInlineReturnTarget(owner->current_block());
      TestContext* outer_test_context = TestContext::cast(owner->ast_context());
      Expression* cond = outer_test_context->condition();
      // The AstContext constructor pushed on the context stack.  This newed
      // instance is the reason that AstContext can't be BASE_EMBEDDED.
      test_context_ = new TestContext(owner, cond, if_true, if_false);
    } else {
      function_return_ = owner->graph()->CreateBasicBlock();
      function_return()->MarkAsInlineReturnTarget(owner->current_block());
    }
    // Set this after possibly allocating a new TestContext above.
    call_context_ = owner->ast_context();
  }

  // Push on the state stack.
  owner->set_function_state(this);

  if (FLAG_hydrogen_track_positions) {
    outer_source_position_ = owner->source_position();
    owner->EnterInlinedSource(
      info->shared_info()->start_position(),
      inlining_id);
    owner->SetSourcePosition(info->shared_info()->start_position());
  }
}


FunctionState::~FunctionState() {
  delete test_context_;
  owner_->set_function_state(outer_);

  if (FLAG_hydrogen_track_positions) {
    owner_->set_source_position(outer_source_position_);
    owner_->EnterInlinedSource(
      outer_->compilation_info()->shared_info()->start_position(),
      outer_->inlining_id());
  }
}


// Implementation of utility classes to represent an expression's context in
// the AST.
AstContext::AstContext(HOptimizedGraphBuilder* owner, Expression::Context kind)
    : owner_(owner),
      kind_(kind),
      outer_(owner->ast_context()),
      for_typeof_(false) {
  owner->set_ast_context(this);  // Push.
#ifdef DEBUG
  ASSERT(owner->environment()->frame_type() == JS_FUNCTION);
  original_length_ = owner->environment()->length();
#endif
}


AstContext::~AstContext() {
  owner_->set_ast_context(outer_);  // Pop.
}


EffectContext::~EffectContext() {
  ASSERT(owner()->HasStackOverflow() ||
         owner()->current_block() == NULL ||
         (owner()->environment()->length() == original_length_ &&
          owner()->environment()->frame_type() == JS_FUNCTION));
}


ValueContext::~ValueContext() {
  ASSERT(owner()->HasStackOverflow() ||
         owner()->current_block() == NULL ||
         (owner()->environment()->length() == original_length_ + 1 &&
          owner()->environment()->frame_type() == JS_FUNCTION));
}


void EffectContext::ReturnValue(HValue* value) {
  // The value is simply ignored.
}


void ValueContext::ReturnValue(HValue* value) {
  // The value is tracked in the bailout environment, and communicated
  // through the environment as the result of the expression.
  if (!arguments_allowed() && value->CheckFlag(HValue::kIsArguments)) {
    owner()->Bailout(kBadValueContextForArgumentsValue);
  }
  owner()->Push(value);
}


void TestContext::ReturnValue(HValue* value) {
  BuildBranch(value);
}


void EffectContext::ReturnInstruction(HInstruction* instr, BailoutId ast_id) {
  ASSERT(!instr->IsControlInstruction());
  owner()->AddInstruction(instr);
  if (instr->HasObservableSideEffects()) {
    owner()->Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
  }
}


void EffectContext::ReturnControl(HControlInstruction* instr,
                                  BailoutId ast_id) {
  ASSERT(!instr->HasObservableSideEffects());
  HBasicBlock* empty_true = owner()->graph()->CreateBasicBlock();
  HBasicBlock* empty_false = owner()->graph()->CreateBasicBlock();
  instr->SetSuccessorAt(0, empty_true);
  instr->SetSuccessorAt(1, empty_false);
  owner()->FinishCurrentBlock(instr);
  HBasicBlock* join = owner()->CreateJoin(empty_true, empty_false, ast_id);
  owner()->set_current_block(join);
}


void EffectContext::ReturnContinuation(HIfContinuation* continuation,
                                       BailoutId ast_id) {
  HBasicBlock* true_branch = NULL;
  HBasicBlock* false_branch = NULL;
  continuation->Continue(&true_branch, &false_branch);
  if (!continuation->IsTrueReachable()) {
    owner()->set_current_block(false_branch);
  } else if (!continuation->IsFalseReachable()) {
    owner()->set_current_block(true_branch);
  } else {
    HBasicBlock* join = owner()->CreateJoin(true_branch, false_branch, ast_id);
    owner()->set_current_block(join);
  }
}


void ValueContext::ReturnInstruction(HInstruction* instr, BailoutId ast_id) {
  ASSERT(!instr->IsControlInstruction());
  if (!arguments_allowed() && instr->CheckFlag(HValue::kIsArguments)) {
    return owner()->Bailout(kBadValueContextForArgumentsObjectValue);
  }
  owner()->AddInstruction(instr);
  owner()->Push(instr);
  if (instr->HasObservableSideEffects()) {
    owner()->Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
  }
}


void ValueContext::ReturnControl(HControlInstruction* instr, BailoutId ast_id) {
  ASSERT(!instr->HasObservableSideEffects());
  if (!arguments_allowed() && instr->CheckFlag(HValue::kIsArguments)) {
    return owner()->Bailout(kBadValueContextForArgumentsObjectValue);
  }
  HBasicBlock* materialize_false = owner()->graph()->CreateBasicBlock();
  HBasicBlock* materialize_true = owner()->graph()->CreateBasicBlock();
  instr->SetSuccessorAt(0, materialize_true);
  instr->SetSuccessorAt(1, materialize_false);
  owner()->FinishCurrentBlock(instr);
  owner()->set_current_block(materialize_true);
  owner()->Push(owner()->graph()->GetConstantTrue());
  owner()->set_current_block(materialize_false);
  owner()->Push(owner()->graph()->GetConstantFalse());
  HBasicBlock* join =
    owner()->CreateJoin(materialize_true, materialize_false, ast_id);
  owner()->set_current_block(join);
}


void ValueContext::ReturnContinuation(HIfContinuation* continuation,
                                      BailoutId ast_id) {
  HBasicBlock* materialize_true = NULL;
  HBasicBlock* materialize_false = NULL;
  continuation->Continue(&materialize_true, &materialize_false);
  if (continuation->IsTrueReachable()) {
    owner()->set_current_block(materialize_true);
    owner()->Push(owner()->graph()->GetConstantTrue());
    owner()->set_current_block(materialize_true);
  }
  if (continuation->IsFalseReachable()) {
    owner()->set_current_block(materialize_false);
    owner()->Push(owner()->graph()->GetConstantFalse());
    owner()->set_current_block(materialize_false);
  }
  if (continuation->TrueAndFalseReachable()) {
    HBasicBlock* join =
        owner()->CreateJoin(materialize_true, materialize_false, ast_id);
    owner()->set_current_block(join);
  }
}


void TestContext::ReturnInstruction(HInstruction* instr, BailoutId ast_id) {
  ASSERT(!instr->IsControlInstruction());
  HOptimizedGraphBuilder* builder = owner();
  builder->AddInstruction(instr);
  // We expect a simulate after every expression with side effects, though
  // this one isn't actually needed (and wouldn't work if it were targeted).
  if (instr->HasObservableSideEffects()) {
    builder->Push(instr);
    builder->Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
    builder->Pop();
  }
  BuildBranch(instr);
}


void TestContext::ReturnControl(HControlInstruction* instr, BailoutId ast_id) {
  ASSERT(!instr->HasObservableSideEffects());
  HBasicBlock* empty_true = owner()->graph()->CreateBasicBlock();
  HBasicBlock* empty_false = owner()->graph()->CreateBasicBlock();
  instr->SetSuccessorAt(0, empty_true);
  instr->SetSuccessorAt(1, empty_false);
  owner()->FinishCurrentBlock(instr);
  owner()->Goto(empty_true, if_true(), owner()->function_state());
  owner()->Goto(empty_false, if_false(), owner()->function_state());
  owner()->set_current_block(NULL);
}


void TestContext::ReturnContinuation(HIfContinuation* continuation,
                                     BailoutId ast_id) {
  HBasicBlock* true_branch = NULL;
  HBasicBlock* false_branch = NULL;
  continuation->Continue(&true_branch, &false_branch);
  if (continuation->IsTrueReachable()) {
    owner()->Goto(true_branch, if_true(), owner()->function_state());
  }
  if (continuation->IsFalseReachable()) {
    owner()->Goto(false_branch, if_false(), owner()->function_state());
  }
  owner()->set_current_block(NULL);
}


void TestContext::BuildBranch(HValue* value) {
  // We expect the graph to be in edge-split form: there is no edge that
  // connects a branch node to a join node.  We conservatively ensure that
  // property by always adding an empty block on the outgoing edges of this
  // branch.
  HOptimizedGraphBuilder* builder = owner();
  if (value != NULL && value->CheckFlag(HValue::kIsArguments)) {
    builder->Bailout(kArgumentsObjectValueInATestContext);
  }
  ToBooleanStub::Types expected(condition()->to_boolean_types());
  ReturnControl(owner()->New<HBranch>(value, expected), BailoutId::None());
}


// HOptimizedGraphBuilder infrastructure for bailing out and checking bailouts.
#define CHECK_BAILOUT(call)                     \
  do {                                          \
    call;                                       \
    if (HasStackOverflow()) return;             \
  } while (false)


#define CHECK_ALIVE(call)                                       \
  do {                                                          \
    call;                                                       \
    if (HasStackOverflow() || current_block() == NULL) return;  \
  } while (false)


#define CHECK_ALIVE_OR_RETURN(call, value)                            \
  do {                                                                \
    call;                                                             \
    if (HasStackOverflow() || current_block() == NULL) return value;  \
  } while (false)


void HOptimizedGraphBuilder::Bailout(BailoutReason reason) {
  current_info()->set_bailout_reason(reason);
  SetStackOverflow();
}


void HOptimizedGraphBuilder::VisitForEffect(Expression* expr) {
  EffectContext for_effect(this);
  Visit(expr);
}


void HOptimizedGraphBuilder::VisitForValue(Expression* expr,
                                           ArgumentsAllowedFlag flag) {
  ValueContext for_value(this, flag);
  Visit(expr);
}


void HOptimizedGraphBuilder::VisitForTypeOf(Expression* expr) {
  ValueContext for_value(this, ARGUMENTS_NOT_ALLOWED);
  for_value.set_for_typeof(true);
  Visit(expr);
}



void HOptimizedGraphBuilder::VisitForControl(Expression* expr,
                                             HBasicBlock* true_block,
                                             HBasicBlock* false_block) {
  TestContext for_test(this, expr, true_block, false_block);
  Visit(expr);
}


void HOptimizedGraphBuilder::VisitArgument(Expression* expr) {
  CHECK_ALIVE(VisitForValue(expr));
  Push(Add<HPushArgument>(Pop()));
}


void HOptimizedGraphBuilder::VisitArgumentList(
    ZoneList<Expression*>* arguments) {
  for (int i = 0; i < arguments->length(); i++) {
    CHECK_ALIVE(VisitArgument(arguments->at(i)));
  }
}


void HOptimizedGraphBuilder::VisitExpressions(
    ZoneList<Expression*>* exprs) {
  for (int i = 0; i < exprs->length(); ++i) {
    CHECK_ALIVE(VisitForValue(exprs->at(i)));
  }
}


bool HOptimizedGraphBuilder::BuildGraph() {
  if (current_info()->function()->is_generator()) {
    Bailout(kFunctionIsAGenerator);
    return false;
  }
  Scope* scope = current_info()->scope();
  if (scope->HasIllegalRedeclaration()) {
    Bailout(kFunctionWithIllegalRedeclaration);
    return false;
  }
  if (scope->calls_eval()) {
    Bailout(kFunctionCallsEval);
    return false;
  }
  SetUpScope(scope);

  // Add an edge to the body entry.  This is warty: the graph's start
  // environment will be used by the Lithium translation as the initial
  // environment on graph entry, but it has now been mutated by the
  // Hydrogen translation of the instructions in the start block.  This
  // environment uses values which have not been defined yet.  These
  // Hydrogen instructions will then be replayed by the Lithium
  // translation, so they cannot have an environment effect.  The edge to
  // the body's entry block (along with some special logic for the start
  // block in HInstruction::InsertAfter) seals the start block from
  // getting unwanted instructions inserted.
  //
  // TODO(kmillikin): Fix this.  Stop mutating the initial environment.
  // Make the Hydrogen instructions in the initial block into Hydrogen
  // values (but not instructions), present in the initial environment and
  // not replayed by the Lithium translation.
  HEnvironment* initial_env = environment()->CopyWithoutHistory();
  HBasicBlock* body_entry = CreateBasicBlock(initial_env);
  Goto(body_entry);
  body_entry->SetJoinId(BailoutId::FunctionEntry());
  set_current_block(body_entry);

  // Handle implicit declaration of the function name in named function
  // expressions before other declarations.
  if (scope->is_function_scope() && scope->function() != NULL) {
    VisitVariableDeclaration(scope->function());
  }
  VisitDeclarations(scope->declarations());
  Add<HSimulate>(BailoutId::Declarations());

  Add<HStackCheck>(HStackCheck::kFunctionEntry);

  VisitStatements(current_info()->function()->body());
  if (HasStackOverflow()) return false;

  if (current_block() != NULL) {
    Add<HReturn>(graph()->GetConstantUndefined());
    set_current_block(NULL);
  }

  // If the checksum of the number of type info changes is the same as the
  // last time this function was compiled, then this recompile is likely not
  // due to missing/inadequate type feedback, but rather too aggressive
  // optimization. Disable optimistic LICM in that case.
  Handle<Code> unoptimized_code(current_info()->shared_info()->code());
  ASSERT(unoptimized_code->kind() == Code::FUNCTION);
  Handle<TypeFeedbackInfo> type_info(
      TypeFeedbackInfo::cast(unoptimized_code->type_feedback_info()));
  int checksum = type_info->own_type_change_checksum();
  int composite_checksum = graph()->update_type_change_checksum(checksum);
  graph()->set_use_optimistic_licm(
      !type_info->matches_inlined_type_change_checksum(composite_checksum));
  type_info->set_inlined_type_change_checksum(composite_checksum);

  // Perform any necessary OSR-specific cleanups or changes to the graph.
  osr()->FinishGraph();

  return true;
}


bool HGraph::Optimize(BailoutReason* bailout_reason) {
  OrderBlocks();
  AssignDominators();

  // We need to create a HConstant "zero" now so that GVN will fold every
  // zero-valued constant in the graph together.
  // The constant is needed to make idef-based bounds check work: the pass
  // evaluates relations with "zero" and that zero cannot be created after GVN.
  GetConstant0();

#ifdef DEBUG
  // Do a full verify after building the graph and computing dominators.
  Verify(true);
#endif

  if (FLAG_analyze_environment_liveness && maximum_environment_size() != 0) {
    Run<HEnvironmentLivenessAnalysisPhase>();
  }

  if (!CheckConstPhiUses()) {
    *bailout_reason = kUnsupportedPhiUseOfConstVariable;
    return false;
  }
  Run<HRedundantPhiEliminationPhase>();
  if (!CheckArgumentsPhiUses()) {
    *bailout_reason = kUnsupportedPhiUseOfArguments;
    return false;
  }

  // Find and mark unreachable code to simplify optimizations, especially gvn,
  // where unreachable code could unnecessarily defeat LICM.
  Run<HMarkUnreachableBlocksPhase>();

  if (FLAG_dead_code_elimination) Run<HDeadCodeEliminationPhase>();
  if (FLAG_use_escape_analysis) Run<HEscapeAnalysisPhase>();

  if (FLAG_load_elimination) Run<HLoadEliminationPhase>();

  CollectPhis();

  if (has_osr()) osr()->FinishOsrValues();

  Run<HInferRepresentationPhase>();

  // Remove HSimulate instructions that have turned out not to be needed
  // after all by folding them into the following HSimulate.
  // This must happen after inferring representations.
  Run<HMergeRemovableSimulatesPhase>();

  Run<HMarkDeoptimizeOnUndefinedPhase>();
  Run<HRepresentationChangesPhase>();

  Run<HInferTypesPhase>();

  // Must be performed before canonicalization to ensure that Canonicalize
  // will not remove semantically meaningful ToInt32 operations e.g. BIT_OR with
  // zero.
  if (FLAG_opt_safe_uint32_operations) Run<HUint32AnalysisPhase>();

  if (FLAG_use_canonicalizing) Run<HCanonicalizePhase>();

  if (FLAG_use_gvn) Run<HGlobalValueNumberingPhase>();

  if (FLAG_check_elimination) Run<HCheckEliminationPhase>();

  if (FLAG_use_range) Run<HRangeAnalysisPhase>();

  Run<HComputeChangeUndefinedToNaN>();
  Run<HComputeMinusZeroChecksPhase>();

  // Eliminate redundant stack checks on backwards branches.
  Run<HStackCheckEliminationPhase>();

  if (FLAG_array_bounds_checks_elimination) Run<HBoundsCheckEliminationPhase>();
  if (FLAG_array_bounds_checks_hoisting) Run<HBoundsCheckHoistingPhase>();
  if (FLAG_array_index_dehoisting) Run<HDehoistIndexComputationsPhase>();
  if (FLAG_dead_code_elimination) Run<HDeadCodeEliminationPhase>();

  RestoreActualValues();

  // Find unreachable code a second time, GVN and other optimizations may have
  // made blocks unreachable that were previously reachable.
  Run<HMarkUnreachableBlocksPhase>();

  return true;
}


void HGraph::RestoreActualValues() {
  HPhase phase("H_Restore actual values", this);

  for (int block_index = 0; block_index < blocks()->length(); block_index++) {
    HBasicBlock* block = blocks()->at(block_index);

#ifdef DEBUG
    for (int i = 0; i < block->phis()->length(); i++) {
      HPhi* phi = block->phis()->at(i);
      ASSERT(phi->ActualValue() == phi);
    }
#endif

    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (instruction->ActualValue() == instruction) continue;
      if (instruction->CheckFlag(HValue::kIsDead)) {
        // The instruction was marked as deleted but left in the graph
        // as a control flow dependency point for subsequent
        // instructions.
        instruction->DeleteAndReplaceWith(instruction->ActualValue());
      } else {
        ASSERT(instruction->IsInformativeDefinition());
        if (instruction->IsPurelyInformativeDefinition()) {
          instruction->DeleteAndReplaceWith(instruction->RedefinedOperand());
        } else {
          instruction->ReplaceAllUsesWith(instruction->ActualValue());
        }
      }
    }
  }
}


void HOptimizedGraphBuilder::PushArgumentsFromEnvironment(int count) {
  ZoneList<HValue*> arguments(count, zone());
  for (int i = 0; i < count; ++i) {
    arguments.Add(Pop(), zone());
  }

  while (!arguments.is_empty()) {
    Add<HPushArgument>(arguments.RemoveLast());
  }
}


template <class Instruction>
HInstruction* HOptimizedGraphBuilder::PreProcessCall(Instruction* call) {
  PushArgumentsFromEnvironment(call->argument_count());
  return call;
}


void HOptimizedGraphBuilder::SetUpScope(Scope* scope) {
  // First special is HContext.
  HInstruction* context = Add<HContext>();
  environment()->BindContext(context);

  // Create an arguments object containing the initial parameters.  Set the
  // initial values of parameters including "this" having parameter index 0.
  ASSERT_EQ(scope->num_parameters() + 1, environment()->parameter_count());
  HArgumentsObject* arguments_object =
      New<HArgumentsObject>(environment()->parameter_count());
  for (int i = 0; i < environment()->parameter_count(); ++i) {
    HInstruction* parameter = Add<HParameter>(i);
    arguments_object->AddArgument(parameter, zone());
    environment()->Bind(i, parameter);
  }
  AddInstruction(arguments_object);
  graph()->SetArgumentsObject(arguments_object);

  HConstant* undefined_constant = graph()->GetConstantUndefined();
  // Initialize specials and locals to undefined.
  for (int i = environment()->parameter_count() + 1;
       i < environment()->length();
       ++i) {
    environment()->Bind(i, undefined_constant);
  }

  // Handle the arguments and arguments shadow variables specially (they do
  // not have declarations).
  if (scope->arguments() != NULL) {
    if (!scope->arguments()->IsStackAllocated()) {
      return Bailout(kContextAllocatedArguments);
    }

    environment()->Bind(scope->arguments(),
                        graph()->GetArgumentsObject());
  }
}


void HOptimizedGraphBuilder::VisitStatements(ZoneList<Statement*>* statements) {
  for (int i = 0; i < statements->length(); i++) {
    Statement* stmt = statements->at(i);
    CHECK_ALIVE(Visit(stmt));
    if (stmt->IsJump()) break;
  }
}


void HOptimizedGraphBuilder::VisitBlock(Block* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (stmt->scope() != NULL) {
    return Bailout(kScopedBlock);
  }
  BreakAndContinueInfo break_info(stmt);
  { BreakAndContinueScope push(&break_info, this);
    CHECK_BAILOUT(VisitStatements(stmt->statements()));
  }
  HBasicBlock* break_block = break_info.break_block();
  if (break_block != NULL) {
    if (current_block() != NULL) Goto(break_block);
    break_block->SetJoinId(stmt->ExitId());
    set_current_block(break_block);
  }
}


void HOptimizedGraphBuilder::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  VisitForEffect(stmt->expression());
}


void HOptimizedGraphBuilder::VisitEmptyStatement(EmptyStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
}


void HOptimizedGraphBuilder::VisitIfStatement(IfStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (stmt->condition()->ToBooleanIsTrue()) {
    Add<HSimulate>(stmt->ThenId());
    Visit(stmt->then_statement());
  } else if (stmt->condition()->ToBooleanIsFalse()) {
    Add<HSimulate>(stmt->ElseId());
    Visit(stmt->else_statement());
  } else {
    HBasicBlock* cond_true = graph()->CreateBasicBlock();
    HBasicBlock* cond_false = graph()->CreateBasicBlock();
    CHECK_BAILOUT(VisitForControl(stmt->condition(), cond_true, cond_false));

    if (cond_true->HasPredecessor()) {
      cond_true->SetJoinId(stmt->ThenId());
      set_current_block(cond_true);
      CHECK_BAILOUT(Visit(stmt->then_statement()));
      cond_true = current_block();
    } else {
      cond_true = NULL;
    }

    if (cond_false->HasPredecessor()) {
      cond_false->SetJoinId(stmt->ElseId());
      set_current_block(cond_false);
      CHECK_BAILOUT(Visit(stmt->else_statement()));
      cond_false = current_block();
    } else {
      cond_false = NULL;
    }

    HBasicBlock* join = CreateJoin(cond_true, cond_false, stmt->IfId());
    set_current_block(join);
  }
}


HBasicBlock* HOptimizedGraphBuilder::BreakAndContinueScope::Get(
    BreakableStatement* stmt,
    BreakType type,
    int* drop_extra) {
  *drop_extra = 0;
  BreakAndContinueScope* current = this;
  while (current != NULL && current->info()->target() != stmt) {
    *drop_extra += current->info()->drop_extra();
    current = current->next();
  }
  ASSERT(current != NULL);  // Always found (unless stack is malformed).

  if (type == BREAK) {
    *drop_extra += current->info()->drop_extra();
  }

  HBasicBlock* block = NULL;
  switch (type) {
    case BREAK:
      block = current->info()->break_block();
      if (block == NULL) {
        block = current->owner()->graph()->CreateBasicBlock();
        current->info()->set_break_block(block);
      }
      break;

    case CONTINUE:
      block = current->info()->continue_block();
      if (block == NULL) {
        block = current->owner()->graph()->CreateBasicBlock();
        current->info()->set_continue_block(block);
      }
      break;
  }

  return block;
}


void HOptimizedGraphBuilder::VisitContinueStatement(
    ContinueStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  int drop_extra = 0;
  HBasicBlock* continue_block = break_scope()->Get(
      stmt->target(), BreakAndContinueScope::CONTINUE, &drop_extra);
  Drop(drop_extra);
  Goto(continue_block);
  set_current_block(NULL);
}


void HOptimizedGraphBuilder::VisitBreakStatement(BreakStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  int drop_extra = 0;
  HBasicBlock* break_block = break_scope()->Get(
      stmt->target(), BreakAndContinueScope::BREAK, &drop_extra);
  Drop(drop_extra);
  Goto(break_block);
  set_current_block(NULL);
}


void HOptimizedGraphBuilder::VisitReturnStatement(ReturnStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  FunctionState* state = function_state();
  AstContext* context = call_context();
  if (context == NULL) {
    // Not an inlined return, so an actual one.
    CHECK_ALIVE(VisitForValue(stmt->expression()));
    HValue* result = environment()->Pop();
    Add<HReturn>(result);
  } else if (state->inlining_kind() == CONSTRUCT_CALL_RETURN) {
    // Return from an inlined construct call. In a test context the return value
    // will always evaluate to true, in a value context the return value needs
    // to be a JSObject.
    if (context->IsTest()) {
      TestContext* test = TestContext::cast(context);
      CHECK_ALIVE(VisitForEffect(stmt->expression()));
      Goto(test->if_true(), state);
    } else if (context->IsEffect()) {
      CHECK_ALIVE(VisitForEffect(stmt->expression()));
      Goto(function_return(), state);
    } else {
      ASSERT(context->IsValue());
      CHECK_ALIVE(VisitForValue(stmt->expression()));
      HValue* return_value = Pop();
      HValue* receiver = environment()->arguments_environment()->Lookup(0);
      HHasInstanceTypeAndBranch* typecheck =
          New<HHasInstanceTypeAndBranch>(return_value,
                                         FIRST_SPEC_OBJECT_TYPE,
                                         LAST_SPEC_OBJECT_TYPE);
      HBasicBlock* if_spec_object = graph()->CreateBasicBlock();
      HBasicBlock* not_spec_object = graph()->CreateBasicBlock();
      typecheck->SetSuccessorAt(0, if_spec_object);
      typecheck->SetSuccessorAt(1, not_spec_object);
      FinishCurrentBlock(typecheck);
      AddLeaveInlined(if_spec_object, return_value, state);
      AddLeaveInlined(not_spec_object, receiver, state);
    }
  } else if (state->inlining_kind() == SETTER_CALL_RETURN) {
    // Return from an inlined setter call. The returned value is never used, the
    // value of an assignment is always the value of the RHS of the assignment.
    CHECK_ALIVE(VisitForEffect(stmt->expression()));
    if (context->IsTest()) {
      HValue* rhs = environment()->arguments_environment()->Lookup(1);
      context->ReturnValue(rhs);
    } else if (context->IsEffect()) {
      Goto(function_return(), state);
    } else {
      ASSERT(context->IsValue());
      HValue* rhs = environment()->arguments_environment()->Lookup(1);
      AddLeaveInlined(rhs, state);
    }
  } else {
    // Return from a normal inlined function. Visit the subexpression in the
    // expression context of the call.
    if (context->IsTest()) {
      TestContext* test = TestContext::cast(context);
      VisitForControl(stmt->expression(), test->if_true(), test->if_false());
    } else if (context->IsEffect()) {
      CHECK_ALIVE(VisitForEffect(stmt->expression()));
      Goto(function_return(), state);
    } else {
      ASSERT(context->IsValue());
      CHECK_ALIVE(VisitForValue(stmt->expression()));
      AddLeaveInlined(Pop(), state);
    }
  }
  set_current_block(NULL);
}


void HOptimizedGraphBuilder::VisitWithStatement(WithStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout(kWithStatement);
}


void HOptimizedGraphBuilder::VisitSwitchStatement(SwitchStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());

  // We only optimize switch statements with a bounded number of clauses.
  const int kCaseClauseLimit = 128;
  ZoneList<CaseClause*>* clauses = stmt->cases();
  int clause_count = clauses->length();
  ZoneList<HBasicBlock*> body_blocks(clause_count, zone());
  if (clause_count > kCaseClauseLimit) {
    return Bailout(kSwitchStatementTooManyClauses);
  }

  CHECK_ALIVE(VisitForValue(stmt->tag()));
  Add<HSimulate>(stmt->EntryId());
  HValue* tag_value = Top();
  Type* tag_type = stmt->tag()->bounds().lower;

  // 1. Build all the tests, with dangling true branches
  BailoutId default_id = BailoutId::None();
  for (int i = 0; i < clause_count; ++i) {
    CaseClause* clause = clauses->at(i);
    if (clause->is_default()) {
      body_blocks.Add(NULL, zone());
      if (default_id.IsNone()) default_id = clause->EntryId();
      continue;
    }

    // Generate a compare and branch.
    CHECK_ALIVE(VisitForValue(clause->label()));
    HValue* label_value = Pop();

    Type* label_type = clause->label()->bounds().lower;
    Type* combined_type = clause->compare_type();
    HControlInstruction* compare = BuildCompareInstruction(
        Token::EQ_STRICT, tag_value, label_value, tag_type, label_type,
        combined_type,
        ScriptPositionToSourcePosition(stmt->tag()->position()),
        ScriptPositionToSourcePosition(clause->label()->position()),
        PUSH_BEFORE_SIMULATE, clause->id());

    HBasicBlock* next_test_block = graph()->CreateBasicBlock();
    HBasicBlock* body_block = graph()->CreateBasicBlock();
    body_blocks.Add(body_block, zone());
    compare->SetSuccessorAt(0, body_block);
    compare->SetSuccessorAt(1, next_test_block);
    FinishCurrentBlock(compare);

    set_current_block(body_block);
    Drop(1);  // tag_value

    set_current_block(next_test_block);
  }

  // Save the current block to use for the default or to join with the
  // exit.
  HBasicBlock* last_block = current_block();
  Drop(1);  // tag_value

  // 2. Loop over the clauses and the linked list of tests in lockstep,
  // translating the clause bodies.
  HBasicBlock* fall_through_block = NULL;

  BreakAndContinueInfo break_info(stmt);
  { BreakAndContinueScope push(&break_info, this);
    for (int i = 0; i < clause_count; ++i) {
      CaseClause* clause = clauses->at(i);

      // Identify the block where normal (non-fall-through) control flow
      // goes to.
      HBasicBlock* normal_block = NULL;
      if (clause->is_default()) {
        if (last_block == NULL) continue;
        normal_block = last_block;
        last_block = NULL;  // Cleared to indicate we've handled it.
      } else {
        normal_block = body_blocks[i];
      }

      if (fall_through_block == NULL) {
        set_current_block(normal_block);
      } else {
        HBasicBlock* join = CreateJoin(fall_through_block,
                                       normal_block,
                                       clause->EntryId());
        set_current_block(join);
      }

      CHECK_BAILOUT(VisitStatements(clause->statements()));
      fall_through_block = current_block();
    }
  }

  // Create an up-to-3-way join.  Use the break block if it exists since
  // it's already a join block.
  HBasicBlock* break_block = break_info.break_block();
  if (break_block == NULL) {
    set_current_block(CreateJoin(fall_through_block,
                                 last_block,
                                 stmt->ExitId()));
  } else {
    if (fall_through_block != NULL) Goto(fall_through_block, break_block);
    if (last_block != NULL) Goto(last_block, break_block);
    break_block->SetJoinId(stmt->ExitId());
    set_current_block(break_block);
  }
}


void HOptimizedGraphBuilder::VisitLoopBody(IterationStatement* stmt,
                                           HBasicBlock* loop_entry,
                                           BreakAndContinueInfo* break_info) {
  BreakAndContinueScope push(break_info, this);
  Add<HSimulate>(stmt->StackCheckId());
  HStackCheck* stack_check =
      HStackCheck::cast(Add<HStackCheck>(HStackCheck::kBackwardsBranch));
  ASSERT(loop_entry->IsLoopHeader());
  loop_entry->loop_information()->set_stack_check(stack_check);
  CHECK_BAILOUT(Visit(stmt->body()));
}


void HOptimizedGraphBuilder::VisitDoWhileStatement(DoWhileStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  ASSERT(current_block() != NULL);
  HBasicBlock* loop_entry = BuildLoopEntry(stmt);

  BreakAndContinueInfo break_info(stmt);
  CHECK_BAILOUT(VisitLoopBody(stmt, loop_entry, &break_info));
  HBasicBlock* body_exit =
      JoinContinue(stmt, current_block(), break_info.continue_block());
  HBasicBlock* loop_successor = NULL;
  if (body_exit != NULL && !stmt->cond()->ToBooleanIsTrue()) {
    set_current_block(body_exit);
    loop_successor = graph()->CreateBasicBlock();
    if (stmt->cond()->ToBooleanIsFalse()) {
      Goto(loop_successor);
      body_exit = NULL;
    } else {
      // The block for a true condition, the actual predecessor block of the
      // back edge.
      body_exit = graph()->CreateBasicBlock();
      CHECK_BAILOUT(VisitForControl(stmt->cond(), body_exit, loop_successor));
    }
    if (body_exit != NULL && body_exit->HasPredecessor()) {
      body_exit->SetJoinId(stmt->BackEdgeId());
    } else {
      body_exit = NULL;
    }
    if (loop_successor->HasPredecessor()) {
      loop_successor->SetJoinId(stmt->ExitId());
    } else {
      loop_successor = NULL;
    }
  }
  HBasicBlock* loop_exit = CreateLoop(stmt,
                                      loop_entry,
                                      body_exit,
                                      loop_successor,
                                      break_info.break_block());
  set_current_block(loop_exit);
}


void HOptimizedGraphBuilder::VisitWhileStatement(WhileStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  ASSERT(current_block() != NULL);
  HBasicBlock* loop_entry = BuildLoopEntry(stmt);

  // If the condition is constant true, do not generate a branch.
  HBasicBlock* loop_successor = NULL;
  if (!stmt->cond()->ToBooleanIsTrue()) {
    HBasicBlock* body_entry = graph()->CreateBasicBlock();
    loop_successor = graph()->CreateBasicBlock();
    CHECK_BAILOUT(VisitForControl(stmt->cond(), body_entry, loop_successor));
    if (body_entry->HasPredecessor()) {
      body_entry->SetJoinId(stmt->BodyId());
      set_current_block(body_entry);
    }
    if (loop_successor->HasPredecessor()) {
      loop_successor->SetJoinId(stmt->ExitId());
    } else {
      loop_successor = NULL;
    }
  }

  BreakAndContinueInfo break_info(stmt);
  if (current_block() != NULL) {
    CHECK_BAILOUT(VisitLoopBody(stmt, loop_entry, &break_info));
  }
  HBasicBlock* body_exit =
      JoinContinue(stmt, current_block(), break_info.continue_block());
  HBasicBlock* loop_exit = CreateLoop(stmt,
                                      loop_entry,
                                      body_exit,
                                      loop_successor,
                                      break_info.break_block());
  set_current_block(loop_exit);
}


void HOptimizedGraphBuilder::VisitForStatement(ForStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (stmt->init() != NULL) {
    CHECK_ALIVE(Visit(stmt->init()));
  }
  ASSERT(current_block() != NULL);
  HBasicBlock* loop_entry = BuildLoopEntry(stmt);

  HBasicBlock* loop_successor = NULL;
  if (stmt->cond() != NULL) {
    HBasicBlock* body_entry = graph()->CreateBasicBlock();
    loop_successor = graph()->CreateBasicBlock();
    CHECK_BAILOUT(VisitForControl(stmt->cond(), body_entry, loop_successor));
    if (body_entry->HasPredecessor()) {
      body_entry->SetJoinId(stmt->BodyId());
      set_current_block(body_entry);
    }
    if (loop_successor->HasPredecessor()) {
      loop_successor->SetJoinId(stmt->ExitId());
    } else {
      loop_successor = NULL;
    }
  }

  BreakAndContinueInfo break_info(stmt);
  if (current_block() != NULL) {
    CHECK_BAILOUT(VisitLoopBody(stmt, loop_entry, &break_info));
  }
  HBasicBlock* body_exit =
      JoinContinue(stmt, current_block(), break_info.continue_block());

  if (stmt->next() != NULL && body_exit != NULL) {
    set_current_block(body_exit);
    CHECK_BAILOUT(Visit(stmt->next()));
    body_exit = current_block();
  }

  HBasicBlock* loop_exit = CreateLoop(stmt,
                                      loop_entry,
                                      body_exit,
                                      loop_successor,
                                      break_info.break_block());
  set_current_block(loop_exit);
}


void HOptimizedGraphBuilder::VisitForInStatement(ForInStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());

  if (!FLAG_optimize_for_in) {
    return Bailout(kForInStatementOptimizationIsDisabled);
  }

  if (stmt->for_in_type() != ForInStatement::FAST_FOR_IN) {
    return Bailout(kForInStatementIsNotFastCase);
  }

  if (!stmt->each()->IsVariableProxy() ||
      !stmt->each()->AsVariableProxy()->var()->IsStackLocal()) {
    return Bailout(kForInStatementWithNonLocalEachVariable);
  }

  Variable* each_var = stmt->each()->AsVariableProxy()->var();

  CHECK_ALIVE(VisitForValue(stmt->enumerable()));
  HValue* enumerable = Top();  // Leave enumerable at the top.

  HInstruction* map = Add<HForInPrepareMap>(enumerable);
  Add<HSimulate>(stmt->PrepareId());

  HInstruction* array = Add<HForInCacheArray>(
      enumerable, map, DescriptorArray::kEnumCacheBridgeCacheIndex);

  HInstruction* enum_length = Add<HMapEnumLength>(map);

  HInstruction* start_index = Add<HConstant>(0);

  Push(map);
  Push(array);
  Push(enum_length);
  Push(start_index);

  HInstruction* index_cache = Add<HForInCacheArray>(
      enumerable, map, DescriptorArray::kEnumCacheBridgeIndicesCacheIndex);
  HForInCacheArray::cast(array)->set_index_cache(
      HForInCacheArray::cast(index_cache));

  HBasicBlock* loop_entry = BuildLoopEntry(stmt);

  HValue* index = environment()->ExpressionStackAt(0);
  HValue* limit = environment()->ExpressionStackAt(1);

  // Check that we still have more keys.
  HCompareNumericAndBranch* compare_index =
      New<HCompareNumericAndBranch>(index, limit, Token::LT);
  compare_index->set_observed_input_representation(
      Representation::Smi(), Representation::Smi());

  HBasicBlock* loop_body = graph()->CreateBasicBlock();
  HBasicBlock* loop_successor = graph()->CreateBasicBlock();

  compare_index->SetSuccessorAt(0, loop_body);
  compare_index->SetSuccessorAt(1, loop_successor);
  FinishCurrentBlock(compare_index);

  set_current_block(loop_successor);
  Drop(5);

  set_current_block(loop_body);

  HValue* key = Add<HLoadKeyed>(
      environment()->ExpressionStackAt(2),  // Enum cache.
      environment()->ExpressionStackAt(0),  // Iteration index.
      environment()->ExpressionStackAt(0),
      FAST_ELEMENTS);

  // Check if the expected map still matches that of the enumerable.
  // If not just deoptimize.
  Add<HCheckMapValue>(environment()->ExpressionStackAt(4),
                      environment()->ExpressionStackAt(3));

  Bind(each_var, key);

  BreakAndContinueInfo break_info(stmt, 5);
  CHECK_BAILOUT(VisitLoopBody(stmt, loop_entry, &break_info));

  HBasicBlock* body_exit =
      JoinContinue(stmt, current_block(), break_info.continue_block());

  if (body_exit != NULL) {
    set_current_block(body_exit);

    HValue* current_index = Pop();
    Push(AddUncasted<HAdd>(current_index, graph()->GetConstant1()));
    body_exit = current_block();
  }

  HBasicBlock* loop_exit = CreateLoop(stmt,
                                      loop_entry,
                                      body_exit,
                                      loop_successor,
                                      break_info.break_block());

  set_current_block(loop_exit);
}


void HOptimizedGraphBuilder::VisitForOfStatement(ForOfStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout(kForOfStatement);
}


void HOptimizedGraphBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout(kTryCatchStatement);
}


void HOptimizedGraphBuilder::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout(kTryFinallyStatement);
}


void HOptimizedGraphBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout(kDebuggerStatement);
}


void HOptimizedGraphBuilder::VisitCaseClause(CaseClause* clause) {
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  Handle<SharedFunctionInfo> shared_info = expr->shared_info();
  if (shared_info.is_null()) {
    shared_info = Compiler::BuildFunctionInfo(expr, current_info()->script());
  }
  // We also have a stack overflow if the recursive compilation did.
  if (HasStackOverflow()) return;
  HFunctionLiteral* instr =
      New<HFunctionLiteral>(shared_info, expr->pretenure());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout(kNativeFunctionLiteral);
}


void HOptimizedGraphBuilder::VisitConditional(Conditional* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  HBasicBlock* cond_true = graph()->CreateBasicBlock();
  HBasicBlock* cond_false = graph()->CreateBasicBlock();
  CHECK_BAILOUT(VisitForControl(expr->condition(), cond_true, cond_false));

  // Visit the true and false subexpressions in the same AST context as the
  // whole expression.
  if (cond_true->HasPredecessor()) {
    cond_true->SetJoinId(expr->ThenId());
    set_current_block(cond_true);
    CHECK_BAILOUT(Visit(expr->then_expression()));
    cond_true = current_block();
  } else {
    cond_true = NULL;
  }

  if (cond_false->HasPredecessor()) {
    cond_false->SetJoinId(expr->ElseId());
    set_current_block(cond_false);
    CHECK_BAILOUT(Visit(expr->else_expression()));
    cond_false = current_block();
  } else {
    cond_false = NULL;
  }

  if (!ast_context()->IsTest()) {
    HBasicBlock* join = CreateJoin(cond_true, cond_false, expr->id());
    set_current_block(join);
    if (join != NULL && !ast_context()->IsEffect()) {
      return ast_context()->ReturnValue(Pop());
    }
  }
}


HOptimizedGraphBuilder::GlobalPropertyAccess
    HOptimizedGraphBuilder::LookupGlobalProperty(
        Variable* var, LookupResult* lookup, PropertyAccessType access_type) {
  if (var->is_this() || !current_info()->has_global_object()) {
    return kUseGeneric;
  }
  Handle<GlobalObject> global(current_info()->global_object());
  global->Lookup(*var->name(), lookup);
  if (!lookup->IsNormal() ||
      (access_type == STORE && lookup->IsReadOnly()) ||
      lookup->holder() != *global) {
    return kUseGeneric;
  }

  return kUseCell;
}


HValue* HOptimizedGraphBuilder::BuildContextChainWalk(Variable* var) {
  ASSERT(var->IsContextSlot());
  HValue* context = environment()->context();
  int length = current_info()->scope()->ContextChainLength(var->scope());
  while (length-- > 0) {
    context = Add<HLoadNamedField>(
        context, static_cast<HValue*>(NULL),
        HObjectAccess::ForContextSlot(Context::PREVIOUS_INDEX));
  }
  return context;
}


void HOptimizedGraphBuilder::VisitVariableProxy(VariableProxy* expr) {
  if (expr->is_this()) {
    current_info()->set_this_has_uses(true);
  }

  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  Variable* variable = expr->var();
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      if (IsLexicalVariableMode(variable->mode())) {
        // TODO(rossberg): should this be an ASSERT?
        return Bailout(kReferenceToGlobalLexicalVariable);
      }
      // Handle known global constants like 'undefined' specially to avoid a
      // load from a global cell for them.
      Handle<Object> constant_value =
          isolate()->factory()->GlobalConstantFor(variable->name());
      if (!constant_value.is_null()) {
        HConstant* instr = New<HConstant>(constant_value);
        return ast_context()->ReturnInstruction(instr, expr->id());
      }

      LookupResult lookup(isolate());
      GlobalPropertyAccess type = LookupGlobalProperty(variable, &lookup, LOAD);

      if (type == kUseCell &&
          current_info()->global_object()->IsAccessCheckNeeded()) {
        type = kUseGeneric;
      }

      if (type == kUseCell) {
        Handle<GlobalObject> global(current_info()->global_object());
        Handle<PropertyCell> cell(global->GetPropertyCell(&lookup));
        if (cell->type()->IsConstant()) {
          cell->AddDependentCompilationInfo(top_info());
          Handle<Object> constant_object = cell->type()->AsConstant();
          if (constant_object->IsConsString()) {
            constant_object =
                FlattenGetString(Handle<String>::cast(constant_object));
          }
          HConstant* constant = New<HConstant>(constant_object);
          return ast_context()->ReturnInstruction(constant, expr->id());
        } else {
          HLoadGlobalCell* instr =
              New<HLoadGlobalCell>(cell, lookup.GetPropertyDetails());
          return ast_context()->ReturnInstruction(instr, expr->id());
        }
      } else {
        HValue* global_object = Add<HLoadNamedField>(
            context(), static_cast<HValue*>(NULL),
            HObjectAccess::ForContextSlot(Context::GLOBAL_OBJECT_INDEX));
        HLoadGlobalGeneric* instr =
            New<HLoadGlobalGeneric>(global_object,
                                    variable->name(),
                                    ast_context()->is_for_typeof());
        return ast_context()->ReturnInstruction(instr, expr->id());
      }
    }

    case Variable::PARAMETER:
    case Variable::LOCAL: {
      HValue* value = LookupAndMakeLive(variable);
      if (value == graph()->GetConstantHole()) {
        ASSERT(IsDeclaredVariableMode(variable->mode()) &&
               variable->mode() != VAR);
        return Bailout(kReferenceToUninitializedVariable);
      }
      return ast_context()->ReturnValue(value);
    }

    case Variable::CONTEXT: {
      HValue* context = BuildContextChainWalk(variable);
      HLoadContextSlot* instr = new(zone()) HLoadContextSlot(context, variable);
      return ast_context()->ReturnInstruction(instr, expr->id());
    }

    case Variable::LOOKUP:
      return Bailout(kReferenceToAVariableWhichRequiresDynamicLookup);
  }
}


void HOptimizedGraphBuilder::VisitLiteral(Literal* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  HConstant* instr = New<HConstant>(expr->value());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitRegExpLiteral(RegExpLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  Handle<JSFunction> closure = function_state()->compilation_info()->closure();
  Handle<FixedArray> literals(closure->literals());
  HRegExpLiteral* instr = New<HRegExpLiteral>(literals,
                                              expr->pattern(),
                                              expr->flags(),
                                              expr->literal_index());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


static bool CanInlinePropertyAccess(Type* type) {
  if (type->Is(Type::NumberOrString())) return true;
  if (!type->IsClass()) return false;
  Handle<Map> map = type->AsClass();
  return map->IsJSObjectMap() &&
      !map->is_dictionary_map() &&
      !map->has_named_interceptor();
}


// Determines whether the given array or object literal boilerplate satisfies
// all limits to be considered for fast deep-copying and computes the total
// size of all objects that are part of the graph.
static bool IsFastLiteral(Handle<JSObject> boilerplate,
                          int max_depth,
                          int* max_properties) {
  if (boilerplate->map()->is_deprecated()) {
    Handle<Object> result = JSObject::TryMigrateInstance(boilerplate);
    if (result.is_null()) return false;
  }

  ASSERT(max_depth >= 0 && *max_properties >= 0);
  if (max_depth == 0) return false;

  Isolate* isolate = boilerplate->GetIsolate();
  Handle<FixedArrayBase> elements(boilerplate->elements());
  if (elements->length() > 0 &&
      elements->map() != isolate->heap()->fixed_cow_array_map()) {
    if (boilerplate->HasFastObjectElements()) {
      Handle<FixedArray> fast_elements = Handle<FixedArray>::cast(elements);
      int length = elements->length();
      for (int i = 0; i < length; i++) {
        if ((*max_properties)-- == 0) return false;
        Handle<Object> value(fast_elements->get(i), isolate);
        if (value->IsJSObject()) {
          Handle<JSObject> value_object = Handle<JSObject>::cast(value);
          if (!IsFastLiteral(value_object,
                             max_depth - 1,
                             max_properties)) {
            return false;
          }
        }
      }
    } else if (!boilerplate->HasFastDoubleElements()) {
      return false;
    }
  }

  Handle<FixedArray> properties(boilerplate->properties());
  if (properties->length() > 0) {
    return false;
  } else {
    Handle<DescriptorArray> descriptors(
        boilerplate->map()->instance_descriptors());
    int limit = boilerplate->map()->NumberOfOwnDescriptors();
    for (int i = 0; i < limit; i++) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (details.type() != FIELD) continue;
      int index = descriptors->GetFieldIndex(i);
      if ((*max_properties)-- == 0) return false;
      Handle<Object> value(boilerplate->InObjectPropertyAt(index), isolate);
      if (value->IsJSObject()) {
        Handle<JSObject> value_object = Handle<JSObject>::cast(value);
        if (!IsFastLiteral(value_object,
                           max_depth - 1,
                           max_properties)) {
          return false;
        }
      }
    }
  }
  return true;
}


void HOptimizedGraphBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  expr->BuildConstantProperties(isolate());
  Handle<JSFunction> closure = function_state()->compilation_info()->closure();
  HInstruction* literal;

  // Check whether to use fast or slow deep-copying for boilerplate.
  int max_properties = kMaxFastLiteralProperties;
  Handle<Object> literals_cell(closure->literals()->get(expr->literal_index()),
                               isolate());
  Handle<AllocationSite> site;
  Handle<JSObject> boilerplate;
  if (!literals_cell->IsUndefined()) {
    // Retrieve the boilerplate
    site = Handle<AllocationSite>::cast(literals_cell);
    boilerplate = Handle<JSObject>(JSObject::cast(site->transition_info()),
                                   isolate());
  }

  if (!boilerplate.is_null() &&
      IsFastLiteral(boilerplate, kMaxFastLiteralDepth, &max_properties)) {
    AllocationSiteUsageContext usage_context(isolate(), site, false);
    usage_context.EnterNewScope();
    literal = BuildFastLiteral(boilerplate, &usage_context);
    usage_context.ExitScope(site, boilerplate);
  } else {
    NoObservableSideEffectsScope no_effects(this);
    Handle<FixedArray> closure_literals(closure->literals(), isolate());
    Handle<FixedArray> constant_properties = expr->constant_properties();
    int literal_index = expr->literal_index();
    int flags = expr->fast_elements()
        ? ObjectLiteral::kFastElements : ObjectLiteral::kNoFlags;
    flags |= expr->has_function()
        ? ObjectLiteral::kHasFunction : ObjectLiteral::kNoFlags;

    Add<HPushArgument>(Add<HConstant>(closure_literals));
    Add<HPushArgument>(Add<HConstant>(literal_index));
    Add<HPushArgument>(Add<HConstant>(constant_properties));
    Add<HPushArgument>(Add<HConstant>(flags));

    // TODO(mvstanton): Add a flag to turn off creation of any
    // AllocationMementos for this call: we are in crankshaft and should have
    // learned enough about transition behavior to stop emitting mementos.
    Runtime::FunctionId function_id = Runtime::kCreateObjectLiteral;
    literal = Add<HCallRuntime>(isolate()->factory()->empty_string(),
                                Runtime::FunctionForId(function_id),
                                4);
  }

  // The object is expected in the bailout environment during computation
  // of the property values and is the value of the entire expression.
  Push(literal);

  expr->CalculateEmitStore(zone());

  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key();
    Expression* value = property->value();

    switch (property->kind()) {
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        ASSERT(!CompileTimeValue::IsCompileTimeValue(value));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            CHECK_ALIVE(VisitForValue(value));
            HValue* value = Pop();
            Handle<Map> map = property->GetReceiverType();
            Handle<String> name = property->key()->AsPropertyName();
            HInstruction* store;
            if (map.is_null()) {
              // If we don't know the monomorphic type, do a generic store.
              CHECK_ALIVE(store = BuildNamedGeneric(
                  STORE, literal, name, value));
            } else {
              PropertyAccessInfo info(this, STORE, ToType(map), name);
              if (info.CanAccessMonomorphic()) {
                HValue* checked_literal = BuildCheckMap(literal, map);
                ASSERT(!info.lookup()->IsPropertyCallbacks());
                store = BuildMonomorphicAccess(
                    &info, literal, checked_literal, value,
                    BailoutId::None(), BailoutId::None());
              } else {
                CHECK_ALIVE(store = BuildNamedGeneric(
                    STORE, literal, name, value));
              }
            }
            AddInstruction(store);
            if (store->HasObservableSideEffects()) {
              Add<HSimulate>(key->id(), REMOVABLE_SIMULATE);
            }
          } else {
            CHECK_ALIVE(VisitForEffect(value));
          }
          break;
        }
        // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
      case ObjectLiteral::Property::SETTER:
      case ObjectLiteral::Property::GETTER:
        return Bailout(kObjectLiteralWithComplexProperty);
      default: UNREACHABLE();
    }
  }

  if (expr->has_function()) {
    // Return the result of the transformation to fast properties
    // instead of the original since this operation changes the map
    // of the object. This makes sure that the original object won't
    // be used by other optimized code before it is transformed
    // (e.g. because of code motion).
    HToFastProperties* result = Add<HToFastProperties>(Pop());
    return ast_context()->ReturnValue(result);
  } else {
    return ast_context()->ReturnValue(Pop());
  }
}


void HOptimizedGraphBuilder::VisitArrayLiteral(ArrayLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  expr->BuildConstantElements(isolate());
  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();
  HInstruction* literal;

  Handle<AllocationSite> site;
  Handle<FixedArray> literals(environment()->closure()->literals(), isolate());
  bool uninitialized = false;
  Handle<Object> literals_cell(literals->get(expr->literal_index()),
                               isolate());
  Handle<JSObject> boilerplate_object;
  if (literals_cell->IsUndefined()) {
    uninitialized = true;
    Handle<Object> raw_boilerplate = Runtime::CreateArrayLiteralBoilerplate(
        isolate(), literals, expr->constant_elements());
    if (raw_boilerplate.is_null()) {
      return Bailout(kArrayBoilerplateCreationFailed);
    }

    boilerplate_object = Handle<JSObject>::cast(raw_boilerplate);
    AllocationSiteCreationContext creation_context(isolate());
    site = creation_context.EnterNewScope();
    if (JSObject::DeepWalk(boilerplate_object, &creation_context).is_null()) {
      return Bailout(kArrayBoilerplateCreationFailed);
    }
    creation_context.ExitScope(site, boilerplate_object);
    literals->set(expr->literal_index(), *site);

    if (boilerplate_object->elements()->map() ==
        isolate()->heap()->fixed_cow_array_map()) {
      isolate()->counters()->cow_arrays_created_runtime()->Increment();
    }
  } else {
    ASSERT(literals_cell->IsAllocationSite());
    site = Handle<AllocationSite>::cast(literals_cell);
    boilerplate_object = Handle<JSObject>(
        JSObject::cast(site->transition_info()), isolate());
  }

  ASSERT(!boilerplate_object.is_null());
  ASSERT(site->SitePointsToLiteral());

  ElementsKind boilerplate_elements_kind =
      boilerplate_object->GetElementsKind();

  // Check whether to use fast or slow deep-copying for boilerplate.
  int max_properties = kMaxFastLiteralProperties;
  if (IsFastLiteral(boilerplate_object,
                    kMaxFastLiteralDepth,
                    &max_properties)) {
    AllocationSiteUsageContext usage_context(isolate(), site, false);
    usage_context.EnterNewScope();
    literal = BuildFastLiteral(boilerplate_object, &usage_context);
    usage_context.ExitScope(site, boilerplate_object);
  } else {
    NoObservableSideEffectsScope no_effects(this);
    // Boilerplate already exists and constant elements are never accessed,
    // pass an empty fixed array to the runtime function instead.
    Handle<FixedArray> constants = isolate()->factory()->empty_fixed_array();
    int literal_index = expr->literal_index();
    int flags = expr->depth() == 1
        ? ArrayLiteral::kShallowElements
        : ArrayLiteral::kNoFlags;
    flags |= ArrayLiteral::kDisableMementos;

    Add<HPushArgument>(Add<HConstant>(literals));
    Add<HPushArgument>(Add<HConstant>(literal_index));
    Add<HPushArgument>(Add<HConstant>(constants));
    Add<HPushArgument>(Add<HConstant>(flags));

    // TODO(mvstanton): Consider a flag to turn off creation of any
    // AllocationMementos for this call: we are in crankshaft and should have
    // learned enough about transition behavior to stop emitting mementos.
    Runtime::FunctionId function_id = Runtime::kCreateArrayLiteral;
    literal = Add<HCallRuntime>(isolate()->factory()->empty_string(),
                                Runtime::FunctionForId(function_id),
                                4);

    // De-opt if elements kind changed from boilerplate_elements_kind.
    Handle<Map> map = Handle<Map>(boilerplate_object->map(), isolate());
    literal = Add<HCheckMaps>(literal, map, top_info());
  }

  // The array is expected in the bailout environment during computation
  // of the property values and is the value of the entire expression.
  Push(literal);
  // The literal index is on the stack, too.
  Push(Add<HConstant>(expr->literal_index()));

  HInstruction* elements = NULL;

  for (int i = 0; i < length; i++) {
    Expression* subexpr = subexprs->at(i);
    // If the subexpression is a literal or a simple materialized literal it
    // is already set in the cloned array.
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    CHECK_ALIVE(VisitForValue(subexpr));
    HValue* value = Pop();
    if (!Smi::IsValid(i)) return Bailout(kNonSmiKeyInArrayLiteral);

    elements = AddLoadElements(literal);

    HValue* key = Add<HConstant>(i);

    switch (boilerplate_elements_kind) {
      case FAST_SMI_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS: {
        HStoreKeyed* instr = Add<HStoreKeyed>(elements, key, value,
                                              boilerplate_elements_kind);
        instr->SetUninitialized(uninitialized);
        break;
      }
      default:
        UNREACHABLE();
        break;
    }

    Add<HSimulate>(expr->GetIdForElement(i));
  }

  Drop(1);  // array literal index
  return ast_context()->ReturnValue(Pop());
}


HCheckMaps* HOptimizedGraphBuilder::AddCheckMap(HValue* object,
                                                Handle<Map> map) {
  BuildCheckHeapObject(object);
  return Add<HCheckMaps>(object, map, top_info());
}


HInstruction* HOptimizedGraphBuilder::BuildLoadNamedField(
    PropertyAccessInfo* info,
    HValue* checked_object) {
  HObjectAccess access = info->access();
  if (access.representation().IsDouble()) {
    // Load the heap number.
    checked_object = Add<HLoadNamedField>(
        checked_object, static_cast<HValue*>(NULL),
        access.WithRepresentation(Representation::Tagged()));
    checked_object->set_type(HType::HeapNumber());
    // Load the double value from it.
    access = HObjectAccess::ForHeapNumberValue();
  }
  return New<HLoadNamedField>(
      checked_object, static_cast<HValue*>(NULL), access);
}


HInstruction* HOptimizedGraphBuilder::BuildStoreNamedField(
    PropertyAccessInfo* info,
    HValue* checked_object,
    HValue* value) {
  bool transition_to_field = info->lookup()->IsTransition();
  // TODO(verwaest): Move this logic into PropertyAccessInfo.
  HObjectAccess field_access = HObjectAccess::ForField(
      info->map(), info->lookup(), info->name());

  HStoreNamedField *instr;
  if (field_access.representation().IsDouble()) {
    HObjectAccess heap_number_access =
        field_access.WithRepresentation(Representation::Tagged());
    if (transition_to_field) {
      // The store requires a mutable HeapNumber to be allocated.
      NoObservableSideEffectsScope no_side_effects(this);
      HInstruction* heap_number_size = Add<HConstant>(HeapNumber::kSize);

      PretenureFlag pretenure_flag = !FLAG_allocation_site_pretenuring ?
          isolate()->heap()->GetPretenureMode() : NOT_TENURED;

      HInstruction* heap_number = Add<HAllocate>(heap_number_size,
          HType::HeapNumber(),
          pretenure_flag,
          HEAP_NUMBER_TYPE);
      AddStoreMapConstant(heap_number, isolate()->factory()->heap_number_map());
      Add<HStoreNamedField>(heap_number, HObjectAccess::ForHeapNumberValue(),
                            value);
      instr = New<HStoreNamedField>(checked_object->ActualValue(),
                                    heap_number_access,
                                    heap_number);
    } else {
      // Already holds a HeapNumber; load the box and write its value field.
      HInstruction* heap_number = Add<HLoadNamedField>(
          checked_object, static_cast<HValue*>(NULL), heap_number_access);
      heap_number->set_type(HType::HeapNumber());
      instr = New<HStoreNamedField>(heap_number,
                                    HObjectAccess::ForHeapNumberValue(),
                                    value, STORE_TO_INITIALIZED_ENTRY);
    }
  } else {
    // This is a normal store.
    instr = New<HStoreNamedField>(
        checked_object->ActualValue(), field_access, value,
        transition_to_field ? INITIALIZING_STORE : STORE_TO_INITIALIZED_ENTRY);
  }

  if (transition_to_field) {
    HConstant* transition_constant = Add<HConstant>(info->transition());
    instr->SetTransition(transition_constant, top_info());
    instr->SetChangesFlag(kMaps);
  }
  return instr;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::IsCompatible(
    PropertyAccessInfo* info) {
  if (!CanInlinePropertyAccess(type_)) return false;

  // Currently only handle Type::Number as a polymorphic case.
  // TODO(verwaest): Support monomorphic handling of numbers with a HCheckNumber
  // instruction.
  if (type_->Is(Type::Number())) return false;

  // Values are only compatible for monomorphic load if they all behave the same
  // regarding value wrappers.
  if (type_->Is(Type::NumberOrString())) {
    if (!info->type_->Is(Type::NumberOrString())) return false;
  } else {
    if (info->type_->Is(Type::NumberOrString())) return false;
  }

  if (!LookupDescriptor()) return false;

  if (!lookup_.IsFound()) {
    return (!info->lookup_.IsFound() || info->has_holder()) &&
        map()->prototype() == info->map()->prototype();
  }

  // Mismatch if the other access info found the property in the prototype
  // chain.
  if (info->has_holder()) return false;

  if (lookup_.IsPropertyCallbacks()) {
    return accessor_.is_identical_to(info->accessor_) &&
        api_holder_.is_identical_to(info->api_holder_);
  }

  if (lookup_.IsConstant()) {
    return constant_.is_identical_to(info->constant_);
  }

  ASSERT(lookup_.IsField());
  if (!info->lookup_.IsField()) return false;

  Representation r = access_.representation();
  if (IsLoad()) {
    if (!info->access_.representation().IsCompatibleForLoad(r)) return false;
  } else {
    if (!info->access_.representation().IsCompatibleForStore(r)) return false;
  }
  if (info->access_.offset() != access_.offset()) return false;
  if (info->access_.IsInobject() != access_.IsInobject()) return false;
  info->GeneralizeRepresentation(r);
  return true;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LookupDescriptor() {
  if (!type_->IsClass()) return true;
  map()->LookupDescriptor(NULL, *name_, &lookup_);
  return LoadResult(map());
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LoadResult(Handle<Map> map) {
  if (!IsLoad() && lookup_.IsProperty() &&
      (lookup_.IsReadOnly() || !lookup_.IsCacheable())) {
    return false;
  }

  if (lookup_.IsField()) {
    access_ = HObjectAccess::ForField(map, &lookup_, name_);
  } else if (lookup_.IsPropertyCallbacks()) {
    Handle<Object> callback(lookup_.GetValueFromMap(*map), isolate());
    if (!callback->IsAccessorPair()) return false;
    Object* raw_accessor = IsLoad()
        ? Handle<AccessorPair>::cast(callback)->getter()
        : Handle<AccessorPair>::cast(callback)->setter();
    if (!raw_accessor->IsJSFunction()) return false;
    Handle<JSFunction> accessor = handle(JSFunction::cast(raw_accessor));
    if (accessor->shared()->IsApiFunction()) {
      CallOptimization call_optimization(accessor);
      if (!call_optimization.is_simple_api_call()) return false;
      CallOptimization::HolderLookup holder_lookup;
      api_holder_ = call_optimization.LookupHolderOfExpectedType(
          map, &holder_lookup);
      switch (holder_lookup) {
        case CallOptimization::kHolderNotFound:
          return false;
        case CallOptimization::kHolderIsReceiver:
        case CallOptimization::kHolderFound:
          break;
      }
    }
    accessor_ = accessor;
  } else if (lookup_.IsConstant()) {
    constant_ = handle(lookup_.GetConstantFromMap(*map), isolate());
  }

  return true;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LookupInPrototypes() {
  Handle<Map> map = this->map();

  while (map->prototype()->IsJSObject()) {
    holder_ = handle(JSObject::cast(map->prototype()));
    if (holder_->map()->is_deprecated()) {
      JSObject::TryMigrateInstance(holder_);
    }
    map = Handle<Map>(holder_->map());
    if (!CanInlinePropertyAccess(ToType(map))) {
      lookup_.NotFound();
      return false;
    }
    map->LookupDescriptor(*holder_, *name_, &lookup_);
    if (lookup_.IsFound()) return LoadResult(map);
  }
  lookup_.NotFound();
  return true;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::CanAccessMonomorphic() {
  if (!CanInlinePropertyAccess(type_)) return false;
  if (IsJSObjectFieldAccessor()) return IsLoad();
  if (!LookupDescriptor()) return false;
  if (lookup_.IsFound()) {
    if (IsLoad()) return true;
    return !lookup_.IsReadOnly() && lookup_.IsCacheable();
  }
  if (!LookupInPrototypes()) return false;
  if (IsLoad()) return true;

  if (lookup_.IsPropertyCallbacks()) return true;
  Handle<Map> map = this->map();
  map->LookupTransition(NULL, *name_, &lookup_);
  if (lookup_.IsTransitionToField() && map->unused_property_fields() > 0) {
    return true;
  }
  return false;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::CanAccessAsMonomorphic(
    SmallMapList* types) {
  ASSERT(type_->Is(ToType(types->first())));
  if (!CanAccessMonomorphic()) return false;
  STATIC_ASSERT(kMaxLoadPolymorphism == kMaxStorePolymorphism);
  if (types->length() > kMaxLoadPolymorphism) return false;

  HObjectAccess access = HObjectAccess::ForMap();  // bogus default
  if (GetJSObjectFieldAccess(&access)) {
    for (int i = 1; i < types->length(); ++i) {
      PropertyAccessInfo test_info(
          builder_, access_type_, ToType(types->at(i)), name_);
      HObjectAccess test_access = HObjectAccess::ForMap();  // bogus default
      if (!test_info.GetJSObjectFieldAccess(&test_access)) return false;
      if (!access.Equals(test_access)) return false;
    }
    return true;
  }

  // Currently only handle Type::Number as a polymorphic case.
  // TODO(verwaest): Support monomorphic handling of numbers with a HCheckNumber
  // instruction.
  if (type_->Is(Type::Number())) return false;

  // Multiple maps cannot transition to the same target map.
  ASSERT(!IsLoad() || !lookup_.IsTransition());
  if (lookup_.IsTransition() && types->length() > 1) return false;

  for (int i = 1; i < types->length(); ++i) {
    PropertyAccessInfo test_info(
        builder_, access_type_, ToType(types->at(i)), name_);
    if (!test_info.IsCompatible(this)) return false;
  }

  return true;
}


static bool NeedsWrappingFor(Type* type, Handle<JSFunction> target) {
  return type->Is(Type::NumberOrString()) &&
      target->shared()->is_classic_mode() &&
      !target->shared()->native();
}


HInstruction* HOptimizedGraphBuilder::BuildMonomorphicAccess(
    PropertyAccessInfo* info,
    HValue* object,
    HValue* checked_object,
    HValue* value,
    BailoutId ast_id,
    BailoutId return_id,
    bool can_inline_accessor) {

  HObjectAccess access = HObjectAccess::ForMap();  // bogus default
  if (info->GetJSObjectFieldAccess(&access)) {
    ASSERT(info->IsLoad());
    return New<HLoadNamedField>(object, checked_object, access);
  }

  HValue* checked_holder = checked_object;
  if (info->has_holder()) {
    Handle<JSObject> prototype(JSObject::cast(info->map()->prototype()));
    checked_holder = BuildCheckPrototypeMaps(prototype, info->holder());
  }

  if (!info->lookup()->IsFound()) {
    ASSERT(info->IsLoad());
    return graph()->GetConstantUndefined();
  }

  if (info->lookup()->IsField()) {
    if (info->IsLoad()) {
      return BuildLoadNamedField(info, checked_holder);
    } else {
      return BuildStoreNamedField(info, checked_object, value);
    }
  }

  if (info->lookup()->IsTransition()) {
    ASSERT(!info->IsLoad());
    return BuildStoreNamedField(info, checked_object, value);
  }

  if (info->lookup()->IsPropertyCallbacks()) {
    Push(checked_object);
    int argument_count = 1;
    if (!info->IsLoad()) {
      argument_count = 2;
      Push(value);
    }

    if (NeedsWrappingFor(info->type(), info->accessor())) {
      HValue* function = Add<HConstant>(info->accessor());
      PushArgumentsFromEnvironment(argument_count);
      return New<HCallFunction>(function, argument_count, WRAP_AND_CALL);
    } else if (FLAG_inline_accessors && can_inline_accessor) {
      bool success = info->IsLoad()
          ? TryInlineGetter(info->accessor(), info->map(), ast_id, return_id)
          : TryInlineSetter(
              info->accessor(), info->map(), ast_id, return_id, value);
      if (success) return NULL;
    }

    PushArgumentsFromEnvironment(argument_count);
    return BuildCallConstantFunction(info->accessor(), argument_count);
  }

  ASSERT(info->lookup()->IsConstant());
  if (info->IsLoad()) {
    return New<HConstant>(info->constant());
  } else {
    return New<HCheckValue>(value, Handle<JSFunction>::cast(info->constant()));
  }
}


void HOptimizedGraphBuilder::HandlePolymorphicNamedFieldAccess(
    PropertyAccessType access_type,
    BailoutId ast_id,
    BailoutId return_id,
    HValue* object,
    HValue* value,
    SmallMapList* types,
    Handle<String> name) {
  // Something did not match; must use a polymorphic load.
  int count = 0;
  HBasicBlock* join = NULL;
  HBasicBlock* number_block = NULL;
  bool handled_string = false;

  bool handle_smi = false;
  STATIC_ASSERT(kMaxLoadPolymorphism == kMaxStorePolymorphism);
  for (int i = 0; i < types->length() && count < kMaxLoadPolymorphism; ++i) {
    PropertyAccessInfo info(this, access_type, ToType(types->at(i)), name);
    if (info.type()->Is(Type::String())) {
      if (handled_string) continue;
      handled_string = true;
    }
    if (info.CanAccessMonomorphic()) {
      count++;
      if (info.type()->Is(Type::Number())) {
        handle_smi = true;
        break;
      }
    }
  }

  count = 0;
  HControlInstruction* smi_check = NULL;
  handled_string = false;

  for (int i = 0; i < types->length() && count < kMaxLoadPolymorphism; ++i) {
    PropertyAccessInfo info(this, access_type, ToType(types->at(i)), name);
    if (info.type()->Is(Type::String())) {
      if (handled_string) continue;
      handled_string = true;
    }
    if (!info.CanAccessMonomorphic()) continue;

    if (count == 0) {
      join = graph()->CreateBasicBlock();
      if (handle_smi) {
        HBasicBlock* empty_smi_block = graph()->CreateBasicBlock();
        HBasicBlock* not_smi_block = graph()->CreateBasicBlock();
        number_block = graph()->CreateBasicBlock();
        smi_check = New<HIsSmiAndBranch>(
            object, empty_smi_block, not_smi_block);
        FinishCurrentBlock(smi_check);
        GotoNoSimulate(empty_smi_block, number_block);
        set_current_block(not_smi_block);
      } else {
        BuildCheckHeapObject(object);
      }
    }
    ++count;
    HBasicBlock* if_true = graph()->CreateBasicBlock();
    HBasicBlock* if_false = graph()->CreateBasicBlock();
    HUnaryControlInstruction* compare;

    HValue* dependency;
    if (info.type()->Is(Type::Number())) {
      Handle<Map> heap_number_map = isolate()->factory()->heap_number_map();
      compare = New<HCompareMap>(object, heap_number_map, top_info(),
                                 if_true, if_false);
      dependency = smi_check;
    } else if (info.type()->Is(Type::String())) {
      compare = New<HIsStringAndBranch>(object, if_true, if_false);
      dependency = compare;
    } else {
      compare = New<HCompareMap>(object, info.map(), top_info(),
                                 if_true, if_false);
      dependency = compare;
    }
    FinishCurrentBlock(compare);

    if (info.type()->Is(Type::Number())) {
      GotoNoSimulate(if_true, number_block);
      if_true = number_block;
    }

    set_current_block(if_true);

    HInstruction* access = BuildMonomorphicAccess(
        &info, object, dependency, value, ast_id,
        return_id, FLAG_polymorphic_inlining);

    HValue* result = NULL;
    switch (access_type) {
      case LOAD:
        result = access;
        break;
      case STORE:
        result = value;
        break;
    }

    if (access == NULL) {
      if (HasStackOverflow()) return;
    } else {
      if (!access->IsLinked()) AddInstruction(access);
      if (!ast_context()->IsEffect()) Push(result);
    }

    if (current_block() != NULL) Goto(join);
    set_current_block(if_false);
  }

  // Finish up.  Unconditionally deoptimize if we've handled all the maps we
  // know about and do not want to handle ones we've never seen.  Otherwise
  // use a generic IC.
  if (count == types->length() && FLAG_deoptimize_uncommon_cases) {
    FinishExitWithHardDeoptimization("Uknown map in polymorphic access");
  } else {
    HInstruction* instr = BuildNamedGeneric(access_type, object, name, value);
    AddInstruction(instr);
    if (!ast_context()->IsEffect()) Push(access_type == LOAD ? instr : value);

    if (join != NULL) {
      Goto(join);
    } else {
      Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
      if (!ast_context()->IsEffect()) ast_context()->ReturnValue(Pop());
      return;
    }
  }

  ASSERT(join != NULL);
  if (join->HasPredecessor()) {
    join->SetJoinId(ast_id);
    set_current_block(join);
    if (!ast_context()->IsEffect()) ast_context()->ReturnValue(Pop());
  } else {
    set_current_block(NULL);
  }
}


static bool ComputeReceiverTypes(Expression* expr,
                                 HValue* receiver,
                                 SmallMapList** t,
                                 Zone* zone) {
  SmallMapList* types = expr->GetReceiverTypes();
  *t = types;
  bool monomorphic = expr->IsMonomorphic();
  if (types != NULL && receiver->HasMonomorphicJSObjectType()) {
    Map* root_map = receiver->GetMonomorphicJSObjectMap()->FindRootMap();
    types->FilterForPossibleTransitions(root_map);
    monomorphic = types->length() == 1;
  }
  return monomorphic && CanInlinePropertyAccess(
      IC::MapToType<Type>(types->first(), zone));
}


static bool AreStringTypes(SmallMapList* types) {
  for (int i = 0; i < types->length(); i++) {
    if (types->at(i)->instance_type() >= FIRST_NONSTRING_TYPE) return false;
  }
  return true;
}


void HOptimizedGraphBuilder::BuildStore(Expression* expr,
                                        Property* prop,
                                        BailoutId ast_id,
                                        BailoutId return_id,
                                        bool is_uninitialized) {
  if (!prop->key()->IsPropertyName()) {
    // Keyed store.
    HValue* value = environment()->ExpressionStackAt(0);
    HValue* key = environment()->ExpressionStackAt(1);
    HValue* object = environment()->ExpressionStackAt(2);
    bool has_side_effects = false;
    HandleKeyedElementAccess(object, key, value, expr,
                             STORE, &has_side_effects);
    Drop(3);
    Push(value);
    Add<HSimulate>(return_id, REMOVABLE_SIMULATE);
    return ast_context()->ReturnValue(Pop());
  }

  // Named store.
  HValue* value = Pop();
  HValue* object = Pop();

  Literal* key = prop->key()->AsLiteral();
  Handle<String> name = Handle<String>::cast(key->value());
  ASSERT(!name.is_null());

  HInstruction* instr = BuildNamedAccess(STORE, ast_id, return_id, expr,
                                         object, name, value, is_uninitialized);
  if (instr == NULL) return;

  if (!ast_context()->IsEffect()) Push(value);
  AddInstruction(instr);
  if (instr->HasObservableSideEffects()) {
    Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
  }
  if (!ast_context()->IsEffect()) Drop(1);
  return ast_context()->ReturnValue(value);
}


void HOptimizedGraphBuilder::HandlePropertyAssignment(Assignment* expr) {
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  CHECK_ALIVE(VisitForValue(prop->obj()));
  if (!prop->key()->IsPropertyName()) {
    CHECK_ALIVE(VisitForValue(prop->key()));
  }
  CHECK_ALIVE(VisitForValue(expr->value()));
  BuildStore(expr, prop, expr->id(),
             expr->AssignmentId(), expr->IsUninitialized());
}


// Because not every expression has a position and there is not common
// superclass of Assignment and CountOperation, we cannot just pass the
// owning expression instead of position and ast_id separately.
void HOptimizedGraphBuilder::HandleGlobalVariableAssignment(
    Variable* var,
    HValue* value,
    BailoutId ast_id) {
  LookupResult lookup(isolate());
  GlobalPropertyAccess type = LookupGlobalProperty(var, &lookup, STORE);
  if (type == kUseCell) {
    Handle<GlobalObject> global(current_info()->global_object());
    Handle<PropertyCell> cell(global->GetPropertyCell(&lookup));
    if (cell->type()->IsConstant()) {
      IfBuilder builder(this);
      HValue* constant = Add<HConstant>(cell->type()->AsConstant());
      if (cell->type()->AsConstant()->IsNumber()) {
        builder.If<HCompareNumericAndBranch>(value, constant, Token::EQ);
      } else {
        builder.If<HCompareObjectEqAndBranch>(value, constant);
      }
      builder.Then();
      builder.Else();
      Add<HDeoptimize>("Constant global variable assignment",
                       Deoptimizer::EAGER);
      builder.End();
    }
    HInstruction* instr =
        Add<HStoreGlobalCell>(value, cell, lookup.GetPropertyDetails());
    if (instr->HasObservableSideEffects()) {
      Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
    }
  } else {
    HValue* global_object = Add<HLoadNamedField>(
        context(), static_cast<HValue*>(NULL),
        HObjectAccess::ForContextSlot(Context::GLOBAL_OBJECT_INDEX));
    HStoreNamedGeneric* instr =
        Add<HStoreNamedGeneric>(global_object, var->name(),
                                 value, function_strict_mode_flag());
    USE(instr);
    ASSERT(instr->HasObservableSideEffects());
    Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
  }
}


void HOptimizedGraphBuilder::HandleCompoundAssignment(Assignment* expr) {
  Expression* target = expr->target();
  VariableProxy* proxy = target->AsVariableProxy();
  Property* prop = target->AsProperty();
  ASSERT(proxy == NULL || prop == NULL);

  // We have a second position recorded in the FullCodeGenerator to have
  // type feedback for the binary operation.
  BinaryOperation* operation = expr->binary_operation();

  if (proxy != NULL) {
    Variable* var = proxy->var();
    if (var->mode() == LET)  {
      return Bailout(kUnsupportedLetCompoundAssignment);
    }

    CHECK_ALIVE(VisitForValue(operation));

    switch (var->location()) {
      case Variable::UNALLOCATED:
        HandleGlobalVariableAssignment(var,
                                       Top(),
                                       expr->AssignmentId());
        break;

      case Variable::PARAMETER:
      case Variable::LOCAL:
        if (var->mode() == CONST)  {
          return Bailout(kUnsupportedConstCompoundAssignment);
        }
        BindIfLive(var, Top());
        break;

      case Variable::CONTEXT: {
        // Bail out if we try to mutate a parameter value in a function
        // using the arguments object.  We do not (yet) correctly handle the
        // arguments property of the function.
        if (current_info()->scope()->arguments() != NULL) {
          // Parameters will be allocated to context slots.  We have no
          // direct way to detect that the variable is a parameter so we do
          // a linear search of the parameter variables.
          int count = current_info()->scope()->num_parameters();
          for (int i = 0; i < count; ++i) {
            if (var == current_info()->scope()->parameter(i)) {
              Bailout(kAssignmentToParameterFunctionUsesArgumentsObject);
            }
          }
        }

        HStoreContextSlot::Mode mode;

        switch (var->mode()) {
          case LET:
            mode = HStoreContextSlot::kCheckDeoptimize;
            break;
          case CONST:
            return ast_context()->ReturnValue(Pop());
          case CONST_HARMONY:
            // This case is checked statically so no need to
            // perform checks here
            UNREACHABLE();
          default:
            mode = HStoreContextSlot::kNoCheck;
        }

        HValue* context = BuildContextChainWalk(var);
        HStoreContextSlot* instr = Add<HStoreContextSlot>(
            context, var->index(), mode, Top());
        if (instr->HasObservableSideEffects()) {
          Add<HSimulate>(expr->AssignmentId(), REMOVABLE_SIMULATE);
        }
        break;
      }

      case Variable::LOOKUP:
        return Bailout(kCompoundAssignmentToLookupSlot);
    }
    return ast_context()->ReturnValue(Pop());

  } else if (prop != NULL) {
    CHECK_ALIVE(VisitForValue(prop->obj()));
    HValue* object = Top();
    HValue* key = NULL;
    if ((!prop->IsFunctionPrototype() && !prop->key()->IsPropertyName()) ||
        prop->IsStringAccess()) {
      CHECK_ALIVE(VisitForValue(prop->key()));
      key = Top();
    }

    CHECK_ALIVE(PushLoad(prop, object, key));

    CHECK_ALIVE(VisitForValue(expr->value()));
    HValue* right = Pop();
    HValue* left = Pop();

    Push(BuildBinaryOperation(operation, left, right, PUSH_BEFORE_SIMULATE));

    BuildStore(expr, prop, expr->id(),
               expr->AssignmentId(), expr->IsUninitialized());
  } else {
    return Bailout(kInvalidLhsInCompoundAssignment);
  }
}


void HOptimizedGraphBuilder::VisitAssignment(Assignment* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  VariableProxy* proxy = expr->target()->AsVariableProxy();
  Property* prop = expr->target()->AsProperty();
  ASSERT(proxy == NULL || prop == NULL);

  if (expr->is_compound()) {
    HandleCompoundAssignment(expr);
    return;
  }

  if (prop != NULL) {
    HandlePropertyAssignment(expr);
  } else if (proxy != NULL) {
    Variable* var = proxy->var();

    if (var->mode() == CONST) {
      if (expr->op() != Token::INIT_CONST) {
        CHECK_ALIVE(VisitForValue(expr->value()));
        return ast_context()->ReturnValue(Pop());
      }

      if (var->IsStackAllocated()) {
        // We insert a use of the old value to detect unsupported uses of const
        // variables (e.g. initialization inside a loop).
        HValue* old_value = environment()->Lookup(var);
        Add<HUseConst>(old_value);
      }
    } else if (var->mode() == CONST_HARMONY) {
      if (expr->op() != Token::INIT_CONST_HARMONY) {
        return Bailout(kNonInitializerAssignmentToConst);
      }
    }

    if (proxy->IsArguments()) return Bailout(kAssignmentToArguments);

    // Handle the assignment.
    switch (var->location()) {
      case Variable::UNALLOCATED:
        CHECK_ALIVE(VisitForValue(expr->value()));
        HandleGlobalVariableAssignment(var,
                                       Top(),
                                       expr->AssignmentId());
        return ast_context()->ReturnValue(Pop());

      case Variable::PARAMETER:
      case Variable::LOCAL: {
        // Perform an initialization check for let declared variables
        // or parameters.
        if (var->mode() == LET && expr->op() == Token::ASSIGN) {
          HValue* env_value = environment()->Lookup(var);
          if (env_value == graph()->GetConstantHole()) {
            return Bailout(kAssignmentToLetVariableBeforeInitialization);
          }
        }
        // We do not allow the arguments object to occur in a context where it
        // may escape, but assignments to stack-allocated locals are
        // permitted.
        CHECK_ALIVE(VisitForValue(expr->value(), ARGUMENTS_ALLOWED));
        HValue* value = Pop();
        BindIfLive(var, value);
        return ast_context()->ReturnValue(value);
      }

      case Variable::CONTEXT: {
        // Bail out if we try to mutate a parameter value in a function using
        // the arguments object.  We do not (yet) correctly handle the
        // arguments property of the function.
        if (current_info()->scope()->arguments() != NULL) {
          // Parameters will rewrite to context slots.  We have no direct way
          // to detect that the variable is a parameter.
          int count = current_info()->scope()->num_parameters();
          for (int i = 0; i < count; ++i) {
            if (var == current_info()->scope()->parameter(i)) {
              return Bailout(kAssignmentToParameterInArgumentsObject);
            }
          }
        }

        CHECK_ALIVE(VisitForValue(expr->value()));
        HStoreContextSlot::Mode mode;
        if (expr->op() == Token::ASSIGN) {
          switch (var->mode()) {
            case LET:
              mode = HStoreContextSlot::kCheckDeoptimize;
              break;
            case CONST:
              return ast_context()->ReturnValue(Pop());
            case CONST_HARMONY:
              // This case is checked statically so no need to
              // perform checks here
              UNREACHABLE();
            default:
              mode = HStoreContextSlot::kNoCheck;
          }
        } else if (expr->op() == Token::INIT_VAR ||
                   expr->op() == Token::INIT_LET ||
                   expr->op() == Token::INIT_CONST_HARMONY) {
          mode = HStoreContextSlot::kNoCheck;
        } else {
          ASSERT(expr->op() == Token::INIT_CONST);

          mode = HStoreContextSlot::kCheckIgnoreAssignment;
        }

        HValue* context = BuildContextChainWalk(var);
        HStoreContextSlot* instr = Add<HStoreContextSlot>(
            context, var->index(), mode, Top());
        if (instr->HasObservableSideEffects()) {
          Add<HSimulate>(expr->AssignmentId(), REMOVABLE_SIMULATE);
        }
        return ast_context()->ReturnValue(Pop());
      }

      case Variable::LOOKUP:
        return Bailout(kAssignmentToLOOKUPVariable);
    }
  } else {
    return Bailout(kInvalidLeftHandSideInAssignment);
  }
}


void HOptimizedGraphBuilder::VisitYield(Yield* expr) {
  // Generators are not optimized, so we should never get here.
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitThrow(Throw* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  // We don't optimize functions with invalid left-hand sides in
  // assignments, count operations, or for-in.  Consequently throw can
  // currently only occur in an effect context.
  ASSERT(ast_context()->IsEffect());
  CHECK_ALIVE(VisitForValue(expr->exception()));

  HValue* value = environment()->Pop();
  if (!FLAG_hydrogen_track_positions) SetSourcePosition(expr->position());
  Add<HPushArgument>(value);
  Add<HCallRuntime>(isolate()->factory()->empty_string(),
                    Runtime::FunctionForId(Runtime::kThrow), 1);
  Add<HSimulate>(expr->id());

  // If the throw definitely exits the function, we can finish with a dummy
  // control flow at this point.  This is not the case if the throw is inside
  // an inlined function which may be replaced.
  if (call_context() == NULL) {
    FinishExitCurrentBlock(New<HAbnormalExit>());
  }
}


HInstruction* HGraphBuilder::AddLoadStringInstanceType(HValue* string) {
  if (string->IsConstant()) {
    HConstant* c_string = HConstant::cast(string);
    if (c_string->HasStringValue()) {
      return Add<HConstant>(c_string->StringValue()->map()->instance_type());
    }
  }
  return Add<HLoadNamedField>(
      Add<HLoadNamedField>(string, static_cast<HValue*>(NULL),
                           HObjectAccess::ForMap()),
      static_cast<HValue*>(NULL), HObjectAccess::ForMapInstanceType());
}


HInstruction* HGraphBuilder::AddLoadStringLength(HValue* string) {
  if (string->IsConstant()) {
    HConstant* c_string = HConstant::cast(string);
    if (c_string->HasStringValue()) {
      return Add<HConstant>(c_string->StringValue()->length());
    }
  }
  return Add<HLoadNamedField>(string, static_cast<HValue*>(NULL),
                              HObjectAccess::ForStringLength());
}


HInstruction* HOptimizedGraphBuilder::BuildNamedGeneric(
    PropertyAccessType access_type,
    HValue* object,
    Handle<String> name,
    HValue* value,
    bool is_uninitialized) {
  if (is_uninitialized) {
    Add<HDeoptimize>("Insufficient type feedback for generic named access",
                     Deoptimizer::SOFT);
  }
  if (access_type == LOAD) {
    return New<HLoadNamedGeneric>(object, name);
  } else {
    return New<HStoreNamedGeneric>(
        object, name, value, function_strict_mode_flag());
  }
}



HInstruction* HOptimizedGraphBuilder::BuildKeyedGeneric(
    PropertyAccessType access_type,
    HValue* object,
    HValue* key,
    HValue* value) {
  if (access_type == LOAD) {
    return New<HLoadKeyedGeneric>(object, key);
  } else {
    return New<HStoreKeyedGeneric>(
        object, key, value, function_strict_mode_flag());
  }
}


LoadKeyedHoleMode HOptimizedGraphBuilder::BuildKeyedHoleMode(Handle<Map> map) {
  // Loads from a "stock" fast holey double arrays can elide the hole check.
  LoadKeyedHoleMode load_mode = NEVER_RETURN_HOLE;
  if (*map == isolate()->get_initial_js_array_map(FAST_HOLEY_DOUBLE_ELEMENTS) &&
      isolate()->IsFastArrayConstructorPrototypeChainIntact()) {
    Handle<JSObject> prototype(JSObject::cast(map->prototype()), isolate());
    Handle<JSObject> object_prototype = isolate()->initial_object_prototype();
    BuildCheckPrototypeMaps(prototype, object_prototype);
    load_mode = ALLOW_RETURN_HOLE;
    graph()->MarkDependsOnEmptyArrayProtoElements();
  }

  return load_mode;
}


HInstruction* HOptimizedGraphBuilder::BuildMonomorphicElementAccess(
    HValue* object,
    HValue* key,
    HValue* val,
    HValue* dependency,
    Handle<Map> map,
    PropertyAccessType access_type,
    KeyedAccessStoreMode store_mode) {
  HCheckMaps* checked_object = Add<HCheckMaps>(object, map, top_info(),
                                               dependency);
  if (dependency) {
    checked_object->ClearDependsOnFlag(kElementsKind);
  }

  if (access_type == STORE && map->prototype()->IsJSObject()) {
    // monomorphic stores need a prototype chain check because shape
    // changes could allow callbacks on elements in the chain that
    // aren't compatible with monomorphic keyed stores.
    Handle<JSObject> prototype(JSObject::cast(map->prototype()));
    Object* holder = map->prototype();
    while (holder->GetPrototype(isolate())->IsJSObject()) {
      holder = holder->GetPrototype(isolate());
    }
    ASSERT(holder->GetPrototype(isolate())->IsNull());

    BuildCheckPrototypeMaps(prototype,
                            Handle<JSObject>(JSObject::cast(holder)));
  }

  LoadKeyedHoleMode load_mode = BuildKeyedHoleMode(map);
  return BuildUncheckedMonomorphicElementAccess(
      checked_object, key, val,
      map->instance_type() == JS_ARRAY_TYPE,
      map->elements_kind(), access_type,
      load_mode, store_mode);
}


HInstruction* HOptimizedGraphBuilder::TryBuildConsolidatedElementLoad(
    HValue* object,
    HValue* key,
    HValue* val,
    SmallMapList* maps) {
  // For polymorphic loads of similar elements kinds (i.e. all tagged or all
  // double), always use the "worst case" code without a transition.  This is
  // much faster than transitioning the elements to the worst case, trading a
  // HTransitionElements for a HCheckMaps, and avoiding mutation of the array.
  bool has_double_maps = false;
  bool has_smi_or_object_maps = false;
  bool has_js_array_access = false;
  bool has_non_js_array_access = false;
  bool has_seen_holey_elements = false;
  Handle<Map> most_general_consolidated_map;
  for (int i = 0; i < maps->length(); ++i) {
    Handle<Map> map = maps->at(i);
    if (!map->IsJSObjectMap()) return NULL;
    // Don't allow mixing of JSArrays with JSObjects.
    if (map->instance_type() == JS_ARRAY_TYPE) {
      if (has_non_js_array_access) return NULL;
      has_js_array_access = true;
    } else if (has_js_array_access) {
      return NULL;
    } else {
      has_non_js_array_access = true;
    }
    // Don't allow mixed, incompatible elements kinds.
    if (map->has_fast_double_elements()) {
      if (has_smi_or_object_maps) return NULL;
      has_double_maps = true;
    } else if (map->has_fast_smi_or_object_elements()) {
      if (has_double_maps) return NULL;
      has_smi_or_object_maps = true;
    } else {
      return NULL;
    }
    // Remember if we've ever seen holey elements.
    if (IsHoleyElementsKind(map->elements_kind())) {
      has_seen_holey_elements = true;
    }
    // Remember the most general elements kind, the code for its load will
    // properly handle all of the more specific cases.
    if ((i == 0) || IsMoreGeneralElementsKindTransition(
            most_general_consolidated_map->elements_kind(),
            map->elements_kind())) {
      most_general_consolidated_map = map;
    }
  }
  if (!has_double_maps && !has_smi_or_object_maps) return NULL;

  HCheckMaps* checked_object = Add<HCheckMaps>(object, maps, top_info());
  // FAST_ELEMENTS is considered more general than FAST_HOLEY_SMI_ELEMENTS.
  // If we've seen both, the consolidated load must use FAST_HOLEY_ELEMENTS.
  ElementsKind consolidated_elements_kind = has_seen_holey_elements
      ? GetHoleyElementsKind(most_general_consolidated_map->elements_kind())
      : most_general_consolidated_map->elements_kind();
  HInstruction* instr = BuildUncheckedMonomorphicElementAccess(
      checked_object, key, val,
      most_general_consolidated_map->instance_type() == JS_ARRAY_TYPE,
      consolidated_elements_kind,
      LOAD, NEVER_RETURN_HOLE, STANDARD_STORE);
  return instr;
}


HValue* HOptimizedGraphBuilder::HandlePolymorphicElementAccess(
    HValue* object,
    HValue* key,
    HValue* val,
    SmallMapList* maps,
    PropertyAccessType access_type,
    KeyedAccessStoreMode store_mode,
    bool* has_side_effects) {
  *has_side_effects = false;
  BuildCheckHeapObject(object);

  if (access_type == LOAD) {
    HInstruction* consolidated_load =
        TryBuildConsolidatedElementLoad(object, key, val, maps);
    if (consolidated_load != NULL) {
      *has_side_effects |= consolidated_load->HasObservableSideEffects();
      return consolidated_load;
    }
  }

  // Elements_kind transition support.
  MapHandleList transition_target(maps->length());
  // Collect possible transition targets.
  MapHandleList possible_transitioned_maps(maps->length());
  for (int i = 0; i < maps->length(); ++i) {
    Handle<Map> map = maps->at(i);
    ElementsKind elements_kind = map->elements_kind();
    if (IsFastElementsKind(elements_kind) &&
        elements_kind != GetInitialFastElementsKind()) {
      possible_transitioned_maps.Add(map);
    }
  }
  // Get transition target for each map (NULL == no transition).
  for (int i = 0; i < maps->length(); ++i) {
    Handle<Map> map = maps->at(i);
    Handle<Map> transitioned_map =
        map->FindTransitionedMap(&possible_transitioned_maps);
    transition_target.Add(transitioned_map);
  }

  MapHandleList untransitionable_maps(maps->length());
  HTransitionElementsKind* transition = NULL;
  for (int i = 0; i < maps->length(); ++i) {
    Handle<Map> map = maps->at(i);
    ASSERT(map->IsMap());
    if (!transition_target.at(i).is_null()) {
      ASSERT(Map::IsValidElementsTransition(
          map->elements_kind(),
          transition_target.at(i)->elements_kind()));
      transition = Add<HTransitionElementsKind>(object, map,
                                                transition_target.at(i));
    } else {
      untransitionable_maps.Add(map);
    }
  }

  // If only one map is left after transitioning, handle this case
  // monomorphically.
  ASSERT(untransitionable_maps.length() >= 1);
  if (untransitionable_maps.length() == 1) {
    Handle<Map> untransitionable_map = untransitionable_maps[0];
    HInstruction* instr = NULL;
    if (untransitionable_map->has_slow_elements_kind() ||
        !untransitionable_map->IsJSObjectMap()) {
      instr = AddInstruction(BuildKeyedGeneric(access_type, object, key, val));
    } else {
      instr = BuildMonomorphicElementAccess(
          object, key, val, transition, untransitionable_map, access_type,
          store_mode);
    }
    *has_side_effects |= instr->HasObservableSideEffects();
    return access_type == STORE ? NULL : instr;
  }

  HBasicBlock* join = graph()->CreateBasicBlock();

  for (int i = 0; i < untransitionable_maps.length(); ++i) {
    Handle<Map> map = untransitionable_maps[i];
    if (!map->IsJSObjectMap()) continue;
    ElementsKind elements_kind = map->elements_kind();
    HBasicBlock* this_map = graph()->CreateBasicBlock();
    HBasicBlock* other_map = graph()->CreateBasicBlock();
    HCompareMap* mapcompare =
        New<HCompareMap>(object, map, top_info(), this_map, other_map);
    FinishCurrentBlock(mapcompare);

    set_current_block(this_map);
    HInstruction* access = NULL;
    if (IsDictionaryElementsKind(elements_kind)) {
      access = AddInstruction(BuildKeyedGeneric(access_type, object, key, val));
    } else {
      ASSERT(IsFastElementsKind(elements_kind) ||
             IsExternalArrayElementsKind(elements_kind));
      LoadKeyedHoleMode load_mode = BuildKeyedHoleMode(map);
      // Happily, mapcompare is a checked object.
      access = BuildUncheckedMonomorphicElementAccess(
          mapcompare, key, val,
          map->instance_type() == JS_ARRAY_TYPE,
          elements_kind, access_type,
          load_mode,
          store_mode);
    }
    *has_side_effects |= access->HasObservableSideEffects();
    // The caller will use has_side_effects and add a correct Simulate.
    access->SetFlag(HValue::kHasNoObservableSideEffects);
    if (access_type == LOAD) {
      Push(access);
    }
    NoObservableSideEffectsScope scope(this);
    GotoNoSimulate(join);
    set_current_block(other_map);
  }

  // Ensure that we visited at least one map above that goes to join. This is
  // necessary because FinishExitWithHardDeoptimization does an AbnormalExit
  // rather than joining the join block. If this becomes an issue, insert a
  // generic access in the case length() == 0.
  ASSERT(join->predecessors()->length() > 0);
  // Deopt if none of the cases matched.
  NoObservableSideEffectsScope scope(this);
  FinishExitWithHardDeoptimization("Unknown map in polymorphic element access");
  set_current_block(join);
  return access_type == STORE ? NULL : Pop();
}


HValue* HOptimizedGraphBuilder::HandleKeyedElementAccess(
    HValue* obj,
    HValue* key,
    HValue* val,
    Expression* expr,
    PropertyAccessType access_type,
    bool* has_side_effects) {
  ASSERT(!expr->IsPropertyName());
  HInstruction* instr = NULL;

  SmallMapList* types;
  bool monomorphic = ComputeReceiverTypes(expr, obj, &types, zone());

  bool force_generic = false;
  if (access_type == STORE &&
      (monomorphic || (types != NULL && !types->is_empty()))) {
    // Stores can't be mono/polymorphic if their prototype chain has dictionary
    // elements. However a receiver map that has dictionary elements itself
    // should be left to normal mono/poly behavior (the other maps may benefit
    // from highly optimized stores).
    for (int i = 0; i < types->length(); i++) {
      Handle<Map> current_map = types->at(i);
      if (current_map->DictionaryElementsInPrototypeChainOnly()) {
        force_generic = true;
        monomorphic = false;
        break;
      }
    }
  }

  if (monomorphic) {
    Handle<Map> map = types->first();
    if (map->has_slow_elements_kind() || !map->IsJSObjectMap()) {
      instr = AddInstruction(BuildKeyedGeneric(access_type, obj, key, val));
    } else {
      BuildCheckHeapObject(obj);
      instr = BuildMonomorphicElementAccess(
          obj, key, val, NULL, map, access_type, expr->GetStoreMode());
    }
  } else if (!force_generic && (types != NULL && !types->is_empty())) {
    return HandlePolymorphicElementAccess(
        obj, key, val, types, access_type,
        expr->GetStoreMode(), has_side_effects);
  } else {
    if (access_type == STORE) {
      if (expr->IsAssignment() &&
          expr->AsAssignment()->HasNoTypeInformation()) {
        Add<HDeoptimize>("Insufficient type feedback for keyed store",
                         Deoptimizer::SOFT);
      }
    } else {
      if (expr->AsProperty()->HasNoTypeInformation()) {
        Add<HDeoptimize>("Insufficient type feedback for keyed load",
                         Deoptimizer::SOFT);
      }
    }
    instr = AddInstruction(BuildKeyedGeneric(access_type, obj, key, val));
  }
  *has_side_effects = instr->HasObservableSideEffects();
  return instr;
}


void HOptimizedGraphBuilder::EnsureArgumentsArePushedForAccess() {
  // Outermost function already has arguments on the stack.
  if (function_state()->outer() == NULL) return;

  if (function_state()->arguments_pushed()) return;

  // Push arguments when entering inlined function.
  HEnterInlined* entry = function_state()->entry();
  entry->set_arguments_pushed();

  HArgumentsObject* arguments = entry->arguments_object();
  const ZoneList<HValue*>* arguments_values = arguments->arguments_values();

  HInstruction* insert_after = entry;
  for (int i = 0; i < arguments_values->length(); i++) {
    HValue* argument = arguments_values->at(i);
    HInstruction* push_argument = New<HPushArgument>(argument);
    push_argument->InsertAfter(insert_after);
    insert_after = push_argument;
  }

  HArgumentsElements* arguments_elements = New<HArgumentsElements>(true);
  arguments_elements->ClearFlag(HValue::kUseGVN);
  arguments_elements->InsertAfter(insert_after);
  function_state()->set_arguments_elements(arguments_elements);
}


bool HOptimizedGraphBuilder::TryArgumentsAccess(Property* expr) {
  VariableProxy* proxy = expr->obj()->AsVariableProxy();
  if (proxy == NULL) return false;
  if (!proxy->var()->IsStackAllocated()) return false;
  if (!environment()->Lookup(proxy->var())->CheckFlag(HValue::kIsArguments)) {
    return false;
  }

  HInstruction* result = NULL;
  if (expr->key()->IsPropertyName()) {
    Handle<String> name = expr->key()->AsLiteral()->AsPropertyName();
    if (!name->IsOneByteEqualTo(STATIC_ASCII_VECTOR("length"))) return false;

    if (function_state()->outer() == NULL) {
      HInstruction* elements = Add<HArgumentsElements>(false);
      result = New<HArgumentsLength>(elements);
    } else {
      // Number of arguments without receiver.
      int argument_count = environment()->
          arguments_environment()->parameter_count() - 1;
      result = New<HConstant>(argument_count);
    }
  } else {
    Push(graph()->GetArgumentsObject());
    CHECK_ALIVE_OR_RETURN(VisitForValue(expr->key()), true);
    HValue* key = Pop();
    Drop(1);  // Arguments object.
    if (function_state()->outer() == NULL) {
      HInstruction* elements = Add<HArgumentsElements>(false);
      HInstruction* length = Add<HArgumentsLength>(elements);
      HInstruction* checked_key = Add<HBoundsCheck>(key, length);
      result = New<HAccessArgumentsAt>(elements, length, checked_key);
    } else {
      EnsureArgumentsArePushedForAccess();

      // Number of arguments without receiver.
      HInstruction* elements = function_state()->arguments_elements();
      int argument_count = environment()->
          arguments_environment()->parameter_count() - 1;
      HInstruction* length = Add<HConstant>(argument_count);
      HInstruction* checked_key = Add<HBoundsCheck>(key, length);
      result = New<HAccessArgumentsAt>(elements, length, checked_key);
    }
  }
  ast_context()->ReturnInstruction(result, expr->id());
  return true;
}


HInstruction* HOptimizedGraphBuilder::BuildNamedAccess(
    PropertyAccessType access,
    BailoutId ast_id,
    BailoutId return_id,
    Expression* expr,
    HValue* object,
    Handle<String> name,
    HValue* value,
    bool is_uninitialized) {
  SmallMapList* types;
  ComputeReceiverTypes(expr, object, &types, zone());
  ASSERT(types != NULL);

  if (types->length() > 0) {
    PropertyAccessInfo info(this, access, ToType(types->first()), name);
    if (!info.CanAccessAsMonomorphic(types)) {
      HandlePolymorphicNamedFieldAccess(
          access, ast_id, return_id, object, value, types, name);
      return NULL;
    }

    HValue* checked_object;
    // Type::Number() is only supported by polymorphic load/call handling.
    ASSERT(!info.type()->Is(Type::Number()));
    BuildCheckHeapObject(object);
    if (AreStringTypes(types)) {
      checked_object =
          Add<HCheckInstanceType>(object, HCheckInstanceType::IS_STRING);
    } else {
      checked_object = Add<HCheckMaps>(object, types, top_info());
    }
    return BuildMonomorphicAccess(
        &info, object, checked_object, value, ast_id, return_id);
  }

  return BuildNamedGeneric(access, object, name, value, is_uninitialized);
}


void HOptimizedGraphBuilder::PushLoad(Property* expr,
                                      HValue* object,
                                      HValue* key) {
  ValueContext for_value(this, ARGUMENTS_NOT_ALLOWED);
  Push(object);
  if (key != NULL) Push(key);
  BuildLoad(expr, expr->LoadId());
}


void HOptimizedGraphBuilder::BuildLoad(Property* expr,
                                       BailoutId ast_id) {
  HInstruction* instr = NULL;
  if (expr->IsStringAccess()) {
    HValue* index = Pop();
    HValue* string = Pop();
    HInstruction* char_code = BuildStringCharCodeAt(string, index);
    AddInstruction(char_code);
    instr = NewUncasted<HStringCharFromCode>(char_code);

  } else if (expr->IsFunctionPrototype()) {
    HValue* function = Pop();
    BuildCheckHeapObject(function);
    instr = New<HLoadFunctionPrototype>(function);

  } else if (expr->key()->IsPropertyName()) {
    Handle<String> name = expr->key()->AsLiteral()->AsPropertyName();
    HValue* object = Pop();

    instr = BuildNamedAccess(LOAD, ast_id, expr->LoadId(), expr,
                             object, name, NULL, expr->IsUninitialized());
    if (instr == NULL) return;
    if (instr->IsLinked()) return ast_context()->ReturnValue(instr);

  } else {
    HValue* key = Pop();
    HValue* obj = Pop();

    bool has_side_effects = false;
    HValue* load = HandleKeyedElementAccess(
        obj, key, NULL, expr, LOAD, &has_side_effects);
    if (has_side_effects) {
      if (ast_context()->IsEffect()) {
        Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
      } else {
        Push(load);
        Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
        Drop(1);
      }
    }
    return ast_context()->ReturnValue(load);
  }
  return ast_context()->ReturnInstruction(instr, ast_id);
}


void HOptimizedGraphBuilder::VisitProperty(Property* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());

  if (TryArgumentsAccess(expr)) return;

  CHECK_ALIVE(VisitForValue(expr->obj()));
  if ((!expr->IsFunctionPrototype() && !expr->key()->IsPropertyName()) ||
      expr->IsStringAccess()) {
    CHECK_ALIVE(VisitForValue(expr->key()));
  }

  BuildLoad(expr, expr->id());
}


HInstruction* HGraphBuilder::BuildConstantMapCheck(Handle<JSObject> constant,
                                                   CompilationInfo* info) {
  HConstant* constant_value = New<HConstant>(constant);

  if (constant->map()->CanOmitMapChecks()) {
    constant->map()->AddDependentCompilationInfo(
        DependentCode::kPrototypeCheckGroup, info);
    return constant_value;
  }

  AddInstruction(constant_value);
  HCheckMaps* check =
      Add<HCheckMaps>(constant_value, handle(constant->map()), info);
  check->ClearDependsOnFlag(kElementsKind);
  return check;
}


HInstruction* HGraphBuilder::BuildCheckPrototypeMaps(Handle<JSObject> prototype,
                                                     Handle<JSObject> holder) {
  while (!prototype.is_identical_to(holder)) {
    BuildConstantMapCheck(prototype, top_info());
    prototype = handle(JSObject::cast(prototype->GetPrototype()));
  }

  HInstruction* checked_object = BuildConstantMapCheck(prototype, top_info());
  if (!checked_object->IsLinked()) AddInstruction(checked_object);
  return checked_object;
}


void HOptimizedGraphBuilder::AddCheckPrototypeMaps(Handle<JSObject> holder,
                                                   Handle<Map> receiver_map) {
  if (!holder.is_null()) {
    Handle<JSObject> prototype(JSObject::cast(receiver_map->prototype()));
    BuildCheckPrototypeMaps(prototype, holder);
  }
}


HInstruction* HOptimizedGraphBuilder::NewPlainFunctionCall(
    HValue* fun, int argument_count, bool pass_argument_count) {
  return New<HCallJSFunction>(
      fun, argument_count, pass_argument_count);
}


HInstruction* HOptimizedGraphBuilder::NewArgumentAdaptorCall(
    HValue* fun, HValue* context,
    int argument_count, HValue* expected_param_count) {
  CallInterfaceDescriptor* descriptor =
      isolate()->call_descriptor(Isolate::ArgumentAdaptorCall);

  HValue* arity = Add<HConstant>(argument_count - 1);

  HValue* op_vals[] = { fun, context, arity, expected_param_count };

  Handle<Code> adaptor =
      isolate()->builtins()->ArgumentsAdaptorTrampoline();
  HConstant* adaptor_value = Add<HConstant>(adaptor);

  return New<HCallWithDescriptor>(
      adaptor_value, argument_count, descriptor,
      Vector<HValue*>(op_vals, descriptor->environment_length()));
}


HInstruction* HOptimizedGraphBuilder::BuildCallConstantFunction(
    Handle<JSFunction> jsfun, int argument_count) {
  HValue* target = Add<HConstant>(jsfun);
  // For constant functions, we try to avoid calling the
  // argument adaptor and instead call the function directly
  int formal_parameter_count = jsfun->shared()->formal_parameter_count();
  bool dont_adapt_arguments =
      (formal_parameter_count ==
       SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  int arity = argument_count - 1;
  bool can_invoke_directly =
      dont_adapt_arguments || formal_parameter_count == arity;
  if (can_invoke_directly) {
    if (jsfun.is_identical_to(current_info()->closure())) {
      graph()->MarkRecursive();
    }
    return NewPlainFunctionCall(target, argument_count, dont_adapt_arguments);
  } else {
    HValue* param_count_value = Add<HConstant>(formal_parameter_count);
    HValue* context = Add<HLoadNamedField>(
        target, static_cast<HValue*>(NULL),
        HObjectAccess::ForFunctionContextPointer());
    return NewArgumentAdaptorCall(target, context,
        argument_count, param_count_value);
  }
  UNREACHABLE();
  return NULL;
}


void HOptimizedGraphBuilder::HandlePolymorphicCallNamed(
    Call* expr,
    HValue* receiver,
    SmallMapList* types,
    Handle<String> name) {
  int argument_count = expr->arguments()->length() + 1;  // Includes receiver.
  int order[kMaxCallPolymorphism];

  bool handle_smi = false;
  bool handled_string = false;
  int ordered_functions = 0;

  for (int i = 0;
       i < types->length() && ordered_functions < kMaxCallPolymorphism;
       ++i) {
    PropertyAccessInfo info(this, LOAD, ToType(types->at(i)), name);
    if (info.CanAccessMonomorphic() &&
        info.lookup()->IsConstant() &&
        info.constant()->IsJSFunction()) {
      if (info.type()->Is(Type::String())) {
        if (handled_string) continue;
        handled_string = true;
      }
      Handle<JSFunction> target = Handle<JSFunction>::cast(info.constant());
      if (info.type()->Is(Type::Number())) {
        handle_smi = true;
      }
      expr->set_target(target);
      order[ordered_functions++] = i;
    }
  }

  HBasicBlock* number_block = NULL;
  HBasicBlock* join = NULL;
  handled_string = false;
  int count = 0;

  for (int fn = 0; fn < ordered_functions; ++fn) {
    int i = order[fn];
    PropertyAccessInfo info(this, LOAD, ToType(types->at(i)), name);
    if (info.type()->Is(Type::String())) {
      if (handled_string) continue;
      handled_string = true;
    }
    // Reloads the target.
    info.CanAccessMonomorphic();
    Handle<JSFunction> target = Handle<JSFunction>::cast(info.constant());

    expr->set_target(target);
    if (count == 0) {
      // Only needed once.
      join = graph()->CreateBasicBlock();
      if (handle_smi) {
        HBasicBlock* empty_smi_block = graph()->CreateBasicBlock();
        HBasicBlock* not_smi_block = graph()->CreateBasicBlock();
        number_block = graph()->CreateBasicBlock();
        FinishCurrentBlock(New<HIsSmiAndBranch>(
                receiver, empty_smi_block, not_smi_block));
        GotoNoSimulate(empty_smi_block, number_block);
        set_current_block(not_smi_block);
      } else {
        BuildCheckHeapObject(receiver);
      }
    }
    ++count;
    HBasicBlock* if_true = graph()->CreateBasicBlock();
    HBasicBlock* if_false = graph()->CreateBasicBlock();
    HUnaryControlInstruction* compare;

    Handle<Map> map = info.map();
    if (info.type()->Is(Type::Number())) {
      Handle<Map> heap_number_map = isolate()->factory()->heap_number_map();
      compare = New<HCompareMap>(receiver, heap_number_map, top_info(),
                                 if_true, if_false);
    } else if (info.type()->Is(Type::String())) {
      compare = New<HIsStringAndBranch>(receiver, if_true, if_false);
    } else {
      compare = New<HCompareMap>(receiver, map, top_info(),
                                 if_true, if_false);
    }
    FinishCurrentBlock(compare);

    if (info.type()->Is(Type::Number())) {
      GotoNoSimulate(if_true, number_block);
      if_true = number_block;
    }

    set_current_block(if_true);

    AddCheckPrototypeMaps(info.holder(), map);

    HValue* function = Add<HConstant>(expr->target());
    environment()->SetExpressionStackAt(0, function);
    Push(receiver);
    CHECK_ALIVE(VisitExpressions(expr->arguments()));
    bool needs_wrapping = NeedsWrappingFor(info.type(), target);
    bool try_inline = FLAG_polymorphic_inlining && !needs_wrapping;
    if (FLAG_trace_inlining && try_inline) {
      Handle<JSFunction> caller = current_info()->closure();
      SmartArrayPointer<char> caller_name =
          caller->shared()->DebugName()->ToCString();
      PrintF("Trying to inline the polymorphic call to %s from %s\n",
             name->ToCString().get(),
             caller_name.get());
    }
    if (try_inline && TryInlineCall(expr)) {
      // Trying to inline will signal that we should bailout from the
      // entire compilation by setting stack overflow on the visitor.
      if (HasStackOverflow()) return;
    } else {
      // Since HWrapReceiver currently cannot actually wrap numbers and strings,
      // use the regular CallFunctionStub for method calls to wrap the receiver.
      // TODO(verwaest): Support creation of value wrappers directly in
      // HWrapReceiver.
      HInstruction* call = needs_wrapping
          ? NewUncasted<HCallFunction>(
              function, argument_count, WRAP_AND_CALL)
          : BuildCallConstantFunction(target, argument_count);
      PushArgumentsFromEnvironment(argument_count);
      AddInstruction(call);
      Drop(1);  // Drop the function.
      if (!ast_context()->IsEffect()) Push(call);
    }

    if (current_block() != NULL) Goto(join);
    set_current_block(if_false);
  }

  // Finish up.  Unconditionally deoptimize if we've handled all the maps we
  // know about and do not want to handle ones we've never seen.  Otherwise
  // use a generic IC.
  if (ordered_functions == types->length() && FLAG_deoptimize_uncommon_cases) {
    FinishExitWithHardDeoptimization("Unknown map in polymorphic call");
  } else {
    Property* prop = expr->expression()->AsProperty();
    HInstruction* function = BuildNamedGeneric(
        LOAD, receiver, name, NULL, prop->IsUninitialized());
    AddInstruction(function);
    Push(function);
    AddSimulate(prop->LoadId(), REMOVABLE_SIMULATE);

    environment()->SetExpressionStackAt(1, function);
    environment()->SetExpressionStackAt(0, receiver);
    CHECK_ALIVE(VisitExpressions(expr->arguments()));

    CallFunctionFlags flags = receiver->type().IsJSObject()
        ? NO_CALL_FUNCTION_FLAGS : CALL_AS_METHOD;
    HInstruction* call = New<HCallFunction>(
        function, argument_count, flags);

    PushArgumentsFromEnvironment(argument_count);

    Drop(1);  // Function.

    if (join != NULL) {
      AddInstruction(call);
      if (!ast_context()->IsEffect()) Push(call);
      Goto(join);
    } else {
      return ast_context()->ReturnInstruction(call, expr->id());
    }
  }

  // We assume that control flow is always live after an expression.  So
  // even without predecessors to the join block, we set it as the exit
  // block and continue by adding instructions there.
  ASSERT(join != NULL);
  if (join->HasPredecessor()) {
    set_current_block(join);
    join->SetJoinId(expr->id());
    if (!ast_context()->IsEffect()) return ast_context()->ReturnValue(Pop());
  } else {
    set_current_block(NULL);
  }
}


void HOptimizedGraphBuilder::TraceInline(Handle<JSFunction> target,
                                         Handle<JSFunction> caller,
                                         const char* reason) {
  if (FLAG_trace_inlining) {
    SmartArrayPointer<char> target_name =
        target->shared()->DebugName()->ToCString();
    SmartArrayPointer<char> caller_name =
        caller->shared()->DebugName()->ToCString();
    if (reason == NULL) {
      PrintF("Inlined %s called from %s.\n", target_name.get(),
             caller_name.get());
    } else {
      PrintF("Did not inline %s called from %s (%s).\n",
             target_name.get(), caller_name.get(), reason);
    }
  }
}


static const int kNotInlinable = 1000000000;


int HOptimizedGraphBuilder::InliningAstSize(Handle<JSFunction> target) {
  if (!FLAG_use_inlining) return kNotInlinable;

  // Precondition: call is monomorphic and we have found a target with the
  // appropriate arity.
  Handle<JSFunction> caller = current_info()->closure();
  Handle<SharedFunctionInfo> target_shared(target->shared());

  // Always inline builtins marked for inlining.
  if (target->IsBuiltin()) {
    return target_shared->inline_builtin() ? 0 : kNotInlinable;
  }

  // Do a quick check on source code length to avoid parsing large
  // inlining candidates.
  if (target_shared->SourceSize() >
      Min(FLAG_max_inlined_source_size, kUnlimitedMaxInlinedSourceSize)) {
    TraceInline(target, caller, "target text too big");
    return kNotInlinable;
  }

  // Target must be inlineable.
  if (!target_shared->IsInlineable()) {
    TraceInline(target, caller, "target not inlineable");
    return kNotInlinable;
  }
  if (target_shared->dont_inline() || target_shared->dont_optimize()) {
    TraceInline(target, caller, "target contains unsupported syntax [early]");
    return kNotInlinable;
  }

  int nodes_added = target_shared->ast_node_count();
  return nodes_added;
}


bool HOptimizedGraphBuilder::TryInline(Handle<JSFunction> target,
                                       int arguments_count,
                                       HValue* implicit_return_value,
                                       BailoutId ast_id,
                                       BailoutId return_id,
                                       InliningKind inlining_kind,
                                       HSourcePosition position) {
  int nodes_added = InliningAstSize(target);
  if (nodes_added == kNotInlinable) return false;

  Handle<JSFunction> caller = current_info()->closure();

  if (nodes_added > Min(FLAG_max_inlined_nodes, kUnlimitedMaxInlinedNodes)) {
    TraceInline(target, caller, "target AST is too large [early]");
    return false;
  }

  // Don't inline deeper than the maximum number of inlining levels.
  HEnvironment* env = environment();
  int current_level = 1;
  while (env->outer() != NULL) {
    if (current_level == FLAG_max_inlining_levels) {
      TraceInline(target, caller, "inline depth limit reached");
      return false;
    }
    if (env->outer()->frame_type() == JS_FUNCTION) {
      current_level++;
    }
    env = env->outer();
  }

  // Don't inline recursive functions.
  for (FunctionState* state = function_state();
       state != NULL;
       state = state->outer()) {
    if (*state->compilation_info()->closure() == *target) {
      TraceInline(target, caller, "target is recursive");
      return false;
    }
  }

  // We don't want to add more than a certain number of nodes from inlining.
  if (inlined_count_ > Min(FLAG_max_inlined_nodes_cumulative,
                           kUnlimitedMaxInlinedNodesCumulative)) {
    TraceInline(target, caller, "cumulative AST node limit reached");
    return false;
  }

  // Parse and allocate variables.
  CompilationInfo target_info(target, zone());
  Handle<SharedFunctionInfo> target_shared(target->shared());
  if (!Parser::Parse(&target_info) || !Scope::Analyze(&target_info)) {
    if (target_info.isolate()->has_pending_exception()) {
      // Parse or scope error, never optimize this function.
      SetStackOverflow();
      target_shared->DisableOptimization(kParseScopeError);
    }
    TraceInline(target, caller, "parse failure");
    return false;
  }

  if (target_info.scope()->num_heap_slots() > 0) {
    TraceInline(target, caller, "target has context-allocated variables");
    return false;
  }
  FunctionLiteral* function = target_info.function();

  // The following conditions must be checked again after re-parsing, because
  // earlier the information might not have been complete due to lazy parsing.
  nodes_added = function->ast_node_count();
  if (nodes_added > Min(FLAG_max_inlined_nodes, kUnlimitedMaxInlinedNodes)) {
    TraceInline(target, caller, "target AST is too large [late]");
    return false;
  }
  AstProperties::Flags* flags(function->flags());
  if (flags->Contains(kDontInline) || function->dont_optimize()) {
    TraceInline(target, caller, "target contains unsupported syntax [late]");
    return false;
  }

  // If the function uses the arguments object check that inlining of functions
  // with arguments object is enabled and the arguments-variable is
  // stack allocated.
  if (function->scope()->arguments() != NULL) {
    if (!FLAG_inline_arguments) {
      TraceInline(target, caller, "target uses arguments object");
      return false;
    }

    if (!function->scope()->arguments()->IsStackAllocated()) {
      TraceInline(target,
                  caller,
                  "target uses non-stackallocated arguments object");
      return false;
    }
  }

  // All declarations must be inlineable.
  ZoneList<Declaration*>* decls = target_info.scope()->declarations();
  int decl_count = decls->length();
  for (int i = 0; i < decl_count; ++i) {
    if (!decls->at(i)->IsInlineable()) {
      TraceInline(target, caller, "target has non-trivial declaration");
      return false;
    }
  }

  // Generate the deoptimization data for the unoptimized version of
  // the target function if we don't already have it.
  if (!target_shared->has_deoptimization_support()) {
    // Note that we compile here using the same AST that we will use for
    // generating the optimized inline code.
    target_info.EnableDeoptimizationSupport();
    if (!FullCodeGenerator::MakeCode(&target_info)) {
      TraceInline(target, caller, "could not generate deoptimization info");
      return false;
    }
    if (target_shared->scope_info() == ScopeInfo::Empty(isolate())) {
      // The scope info might not have been set if a lazily compiled
      // function is inlined before being called for the first time.
      Handle<ScopeInfo> target_scope_info =
          ScopeInfo::Create(target_info.scope(), zone());
      target_shared->set_scope_info(*target_scope_info);
    }
    target_shared->EnableDeoptimizationSupport(*target_info.code());
    Compiler::RecordFunctionCompilation(Logger::FUNCTION_TAG,
                                        &target_info,
                                        target_shared);
  }

  // ----------------------------------------------------------------
  // After this point, we've made a decision to inline this function (so
  // TryInline should always return true).

  // Type-check the inlined function.
  ASSERT(target_shared->has_deoptimization_support());
  AstTyper::Run(&target_info);

  int function_id = graph()->TraceInlinedFunction(target_shared, position);

  // Save the pending call context. Set up new one for the inlined function.
  // The function state is new-allocated because we need to delete it
  // in two different places.
  FunctionState* target_state = new FunctionState(
      this, &target_info, inlining_kind, function_id);

  HConstant* undefined = graph()->GetConstantUndefined();

  HEnvironment* inner_env =
      environment()->CopyForInlining(target,
                                     arguments_count,
                                     function,
                                     undefined,
                                     function_state()->inlining_kind());

  HConstant* context = Add<HConstant>(Handle<Context>(target->context()));
  inner_env->BindContext(context);

  Add<HSimulate>(return_id);
  current_block()->UpdateEnvironment(inner_env);
  HArgumentsObject* arguments_object = NULL;

  // If the function uses arguments object create and bind one, also copy
  // current arguments values to use them for materialization.
  if (function->scope()->arguments() != NULL) {
    ASSERT(function->scope()->arguments()->IsStackAllocated());
    HEnvironment* arguments_env = inner_env->arguments_environment();
    int arguments_count = arguments_env->parameter_count();
    arguments_object = Add<HArgumentsObject>(arguments_count);
    inner_env->Bind(function->scope()->arguments(), arguments_object);
    for (int i = 0; i < arguments_count; i++) {
      arguments_object->AddArgument(arguments_env->Lookup(i), zone());
    }
  }

  HEnterInlined* enter_inlined =
      Add<HEnterInlined>(target, arguments_count, function,
                         function_state()->inlining_kind(),
                         function->scope()->arguments(),
                         arguments_object);
  function_state()->set_entry(enter_inlined);

  VisitDeclarations(target_info.scope()->declarations());
  VisitStatements(function->body());
  if (HasStackOverflow()) {
    // Bail out if the inline function did, as we cannot residualize a call
    // instead.
    TraceInline(target, caller, "inline graph construction failed");
    target_shared->DisableOptimization(kInliningBailedOut);
    inline_bailout_ = true;
    delete target_state;
    return true;
  }

  // Update inlined nodes count.
  inlined_count_ += nodes_added;

  Handle<Code> unoptimized_code(target_shared->code());
  ASSERT(unoptimized_code->kind() == Code::FUNCTION);
  Handle<TypeFeedbackInfo> type_info(
      TypeFeedbackInfo::cast(unoptimized_code->type_feedback_info()));
  graph()->update_type_change_checksum(type_info->own_type_change_checksum());

  TraceInline(target, caller, NULL);

  if (current_block() != NULL) {
    FunctionState* state = function_state();
    if (state->inlining_kind() == CONSTRUCT_CALL_RETURN) {
      // Falling off the end of an inlined construct call. In a test context the
      // return value will always evaluate to true, in a value context the
      // return value is the newly allocated receiver.
      if (call_context()->IsTest()) {
        Goto(inlined_test_context()->if_true(), state);
      } else if (call_context()->IsEffect()) {
        Goto(function_return(), state);
      } else {
        ASSERT(call_context()->IsValue());
        AddLeaveInlined(implicit_return_value, state);
      }
    } else if (state->inlining_kind() == SETTER_CALL_RETURN) {
      // Falling off the end of an inlined setter call. The returned value is
      // never used, the value of an assignment is always the value of the RHS
      // of the assignment.
      if (call_context()->IsTest()) {
        inlined_test_context()->ReturnValue(implicit_return_value);
      } else if (call_context()->IsEffect()) {
        Goto(function_return(), state);
      } else {
        ASSERT(call_context()->IsValue());
        AddLeaveInlined(implicit_return_value, state);
      }
    } else {
      // Falling off the end of a normal inlined function. This basically means
      // returning undefined.
      if (call_context()->IsTest()) {
        Goto(inlined_test_context()->if_false(), state);
      } else if (call_context()->IsEffect()) {
        Goto(function_return(), state);
      } else {
        ASSERT(call_context()->IsValue());
        AddLeaveInlined(undefined, state);
      }
    }
  }

  // Fix up the function exits.
  if (inlined_test_context() != NULL) {
    HBasicBlock* if_true = inlined_test_context()->if_true();
    HBasicBlock* if_false = inlined_test_context()->if_false();

    HEnterInlined* entry = function_state()->entry();

    // Pop the return test context from the expression context stack.
    ASSERT(ast_context() == inlined_test_context());
    ClearInlinedTestContext();
    delete target_state;

    // Forward to the real test context.
    if (if_true->HasPredecessor()) {
      entry->RegisterReturnTarget(if_true, zone());
      if_true->SetJoinId(ast_id);
      HBasicBlock* true_target = TestContext::cast(ast_context())->if_true();
      Goto(if_true, true_target, function_state());
    }
    if (if_false->HasPredecessor()) {
      entry->RegisterReturnTarget(if_false, zone());
      if_false->SetJoinId(ast_id);
      HBasicBlock* false_target = TestContext::cast(ast_context())->if_false();
      Goto(if_false, false_target, function_state());
    }
    set_current_block(NULL);
    return true;

  } else if (function_return()->HasPredecessor()) {
    function_state()->entry()->RegisterReturnTarget(function_return(), zone());
    function_return()->SetJoinId(ast_id);
    set_current_block(function_return());
  } else {
    set_current_block(NULL);
  }
  delete target_state;
  return true;
}


bool HOptimizedGraphBuilder::TryInlineCall(Call* expr) {
  return TryInline(expr->target(),
                   expr->arguments()->length(),
                   NULL,
                   expr->id(),
                   expr->ReturnId(),
                   NORMAL_RETURN,
                   ScriptPositionToSourcePosition(expr->position()));
}


bool HOptimizedGraphBuilder::TryInlineConstruct(CallNew* expr,
                                                HValue* implicit_return_value) {
  return TryInline(expr->target(),
                   expr->arguments()->length(),
                   implicit_return_value,
                   expr->id(),
                   expr->ReturnId(),
                   CONSTRUCT_CALL_RETURN,
                   ScriptPositionToSourcePosition(expr->position()));
}


bool HOptimizedGraphBuilder::TryInlineGetter(Handle<JSFunction> getter,
                                             Handle<Map> receiver_map,
                                             BailoutId ast_id,
                                             BailoutId return_id) {
  if (TryInlineApiGetter(getter, receiver_map, ast_id)) return true;
  return TryInline(getter,
                   0,
                   NULL,
                   ast_id,
                   return_id,
                   GETTER_CALL_RETURN,
                   source_position());
}


bool HOptimizedGraphBuilder::TryInlineSetter(Handle<JSFunction> setter,
                                             Handle<Map> receiver_map,
                                             BailoutId id,
                                             BailoutId assignment_id,
                                             HValue* implicit_return_value) {
  if (TryInlineApiSetter(setter, receiver_map, id)) return true;
  return TryInline(setter,
                   1,
                   implicit_return_value,
                   id, assignment_id,
                   SETTER_CALL_RETURN,
                   source_position());
}


bool HOptimizedGraphBuilder::TryInlineApply(Handle<JSFunction> function,
                                            Call* expr,
                                            int arguments_count) {
  return TryInline(function,
                   arguments_count,
                   NULL,
                   expr->id(),
                   expr->ReturnId(),
                   NORMAL_RETURN,
                   ScriptPositionToSourcePosition(expr->position()));
}


bool HOptimizedGraphBuilder::TryInlineBuiltinFunctionCall(Call* expr) {
  if (!expr->target()->shared()->HasBuiltinFunctionId()) return false;
  BuiltinFunctionId id = expr->target()->shared()->builtin_function_id();
  switch (id) {
    case kMathExp:
      if (!FLAG_fast_math) break;
      // Fall through if FLAG_fast_math.
    case kMathRound:
    case kMathFloor:
    case kMathAbs:
    case kMathSqrt:
    case kMathLog:
      if (expr->arguments()->length() == 1) {
        HValue* argument = Pop();
        Drop(2);  // Receiver and function.
        HInstruction* op = NewUncasted<HUnaryMathOperation>(argument, id);
        ast_context()->ReturnInstruction(op, expr->id());
        return true;
      }
      break;
    case kMathImul:
      if (expr->arguments()->length() == 2) {
        HValue* right = Pop();
        HValue* left = Pop();
        Drop(2);  // Receiver and function.
        HInstruction* op = HMul::NewImul(zone(), context(), left, right);
        ast_context()->ReturnInstruction(op, expr->id());
        return true;
      }
      break;
    default:
      // Not supported for inlining yet.
      break;
  }
  return false;
}


bool HOptimizedGraphBuilder::TryInlineBuiltinMethodCall(
    Call* expr,
    HValue* receiver,
    Handle<Map> receiver_map) {
  // Try to inline calls like Math.* as operations in the calling function.
  if (!expr->target()->shared()->HasBuiltinFunctionId()) return false;
  BuiltinFunctionId id = expr->target()->shared()->builtin_function_id();
  int argument_count = expr->arguments()->length() + 1;  // Plus receiver.
  switch (id) {
    case kStringCharCodeAt:
    case kStringCharAt:
      if (argument_count == 2) {
        HValue* index = Pop();
        HValue* string = Pop();
        Drop(1);  // Function.
        HInstruction* char_code =
            BuildStringCharCodeAt(string, index);
        if (id == kStringCharCodeAt) {
          ast_context()->ReturnInstruction(char_code, expr->id());
          return true;
        }
        AddInstruction(char_code);
        HInstruction* result = NewUncasted<HStringCharFromCode>(char_code);
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    case kStringFromCharCode:
      if (argument_count == 2) {
        HValue* argument = Pop();
        Drop(2);  // Receiver and function.
        HInstruction* result = NewUncasted<HStringCharFromCode>(argument);
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    case kMathExp:
      if (!FLAG_fast_math) break;
      // Fall through if FLAG_fast_math.
    case kMathRound:
    case kMathFloor:
    case kMathAbs:
    case kMathSqrt:
    case kMathLog:
      if (argument_count == 2) {
        HValue* argument = Pop();
        Drop(2);  // Receiver and function.
        HInstruction* op = NewUncasted<HUnaryMathOperation>(argument, id);
        ast_context()->ReturnInstruction(op, expr->id());
        return true;
      }
      break;
    case kMathPow:
      if (argument_count == 3) {
        HValue* right = Pop();
        HValue* left = Pop();
        Drop(2);  // Receiver and function.
        HInstruction* result = NULL;
        // Use sqrt() if exponent is 0.5 or -0.5.
        if (right->IsConstant() && HConstant::cast(right)->HasDoubleValue()) {
          double exponent = HConstant::cast(right)->DoubleValue();
          if (exponent == 0.5) {
            result = NewUncasted<HUnaryMathOperation>(left, kMathPowHalf);
          } else if (exponent == -0.5) {
            HValue* one = graph()->GetConstant1();
            HInstruction* sqrt = AddUncasted<HUnaryMathOperation>(
                left, kMathPowHalf);
            // MathPowHalf doesn't have side effects so there's no need for
            // an environment simulation here.
            ASSERT(!sqrt->HasObservableSideEffects());
            result = NewUncasted<HDiv>(one, sqrt);
          } else if (exponent == 2.0) {
            result = NewUncasted<HMul>(left, left);
          }
        }

        if (result == NULL) {
          result = NewUncasted<HPower>(left, right);
        }
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    case kMathMax:
    case kMathMin:
      if (argument_count == 3) {
        HValue* right = Pop();
        HValue* left = Pop();
        Drop(2);  // Receiver and function.
        HMathMinMax::Operation op = (id == kMathMin) ? HMathMinMax::kMathMin
                                                     : HMathMinMax::kMathMax;
        HInstruction* result = NewUncasted<HMathMinMax>(left, right, op);
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    case kMathImul:
      if (argument_count == 3) {
        HValue* right = Pop();
        HValue* left = Pop();
        Drop(2);  // Receiver and function.
        HInstruction* result = HMul::NewImul(zone(), context(), left, right);
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    case kArrayPop: {
      if (receiver_map.is_null()) return false;
      if (receiver_map->instance_type() != JS_ARRAY_TYPE) return false;
      ElementsKind elements_kind = receiver_map->elements_kind();
      if (!IsFastElementsKind(elements_kind)) return false;

      Drop(expr->arguments()->length());
      HValue* result;
      HValue* reduced_length;
      HValue* receiver = Pop();

      HValue* checked_object = AddCheckMap(receiver, receiver_map);
      HValue* length = Add<HLoadNamedField>(
          checked_object, static_cast<HValue*>(NULL),
          HObjectAccess::ForArrayLength(elements_kind));

      Drop(1);  // Function.

      { NoObservableSideEffectsScope scope(this);
        IfBuilder length_checker(this);

        HValue* bounds_check = length_checker.If<HCompareNumericAndBranch>(
            length, graph()->GetConstant0(), Token::EQ);
        length_checker.Then();

        if (!ast_context()->IsEffect()) Push(graph()->GetConstantUndefined());

        length_checker.Else();
        HValue* elements = AddLoadElements(checked_object);
        // Ensure that we aren't popping from a copy-on-write array.
        if (IsFastSmiOrObjectElementsKind(elements_kind)) {
          elements = BuildCopyElementsOnWrite(checked_object, elements,
                                              elements_kind, length);
        }
        reduced_length = AddUncasted<HSub>(length, graph()->GetConstant1());
        result = AddElementAccess(elements, reduced_length, NULL,
                                  bounds_check, elements_kind, LOAD);
        Factory* factory = isolate()->factory();
        double nan_double = FixedDoubleArray::hole_nan_as_double();
        HValue* hole = IsFastSmiOrObjectElementsKind(elements_kind)
            ? Add<HConstant>(factory->the_hole_value())
            : Add<HConstant>(nan_double);
        if (IsFastSmiOrObjectElementsKind(elements_kind)) {
          elements_kind = FAST_HOLEY_ELEMENTS;
        }
        AddElementAccess(
            elements, reduced_length, hole, bounds_check, elements_kind, STORE);
        Add<HStoreNamedField>(
            checked_object, HObjectAccess::ForArrayLength(elements_kind),
            reduced_length, STORE_TO_INITIALIZED_ENTRY);

        if (!ast_context()->IsEffect()) Push(result);

        length_checker.End();
      }
      result = ast_context()->IsEffect() ? graph()->GetConstant0() : Top();
      Add<HSimulate>(expr->id(), REMOVABLE_SIMULATE);
      if (!ast_context()->IsEffect()) Drop(1);

      ast_context()->ReturnValue(result);
      return true;
    }
    case kArrayPush: {
      if (receiver_map.is_null()) return false;
      if (receiver_map->instance_type() != JS_ARRAY_TYPE) return false;
      ElementsKind elements_kind = receiver_map->elements_kind();
      if (!IsFastElementsKind(elements_kind)) return false;

      HValue* op_vals[] = {
        context(),
        // Receiver.
        environment()->ExpressionStackAt(expr->arguments()->length())
      };

      const int argc = expr->arguments()->length();
      // Includes receiver.
      PushArgumentsFromEnvironment(argc + 1);

      CallInterfaceDescriptor* descriptor =
          isolate()->call_descriptor(Isolate::CallHandler);

      ArrayPushStub stub(receiver_map->elements_kind(), argc);
      Handle<Code> code = stub.GetCode(isolate());
      HConstant* code_value = Add<HConstant>(code);

      ASSERT((sizeof(op_vals) / kPointerSize) ==
             descriptor->environment_length());

      HInstruction* call = New<HCallWithDescriptor>(
          code_value, argc + 1, descriptor,
          Vector<HValue*>(op_vals, descriptor->environment_length()));
      Drop(1);  // Drop function.
      ast_context()->ReturnInstruction(call, expr->id());
      return true;
    }
    default:
      // Not yet supported for inlining.
      break;
  }
  return false;
}


bool HOptimizedGraphBuilder::TryInlineApiFunctionCall(Call* expr,
                                                      HValue* receiver) {
  Handle<JSFunction> function = expr->target();
  int argc = expr->arguments()->length();
  SmallMapList receiver_maps;
  return TryInlineApiCall(function,
                          receiver,
                          &receiver_maps,
                          argc,
                          expr->id(),
                          kCallApiFunction);
}


bool HOptimizedGraphBuilder::TryInlineApiMethodCall(
    Call* expr,
    HValue* receiver,
    SmallMapList* receiver_maps) {
  Handle<JSFunction> function = expr->target();
  int argc = expr->arguments()->length();
  return TryInlineApiCall(function,
                          receiver,
                          receiver_maps,
                          argc,
                          expr->id(),
                          kCallApiMethod);
}


bool HOptimizedGraphBuilder::TryInlineApiGetter(Handle<JSFunction> function,
                                                Handle<Map> receiver_map,
                                                BailoutId ast_id) {
  SmallMapList receiver_maps(1, zone());
  receiver_maps.Add(receiver_map, zone());
  return TryInlineApiCall(function,
                          NULL,  // Receiver is on expression stack.
                          &receiver_maps,
                          0,
                          ast_id,
                          kCallApiGetter);
}


bool HOptimizedGraphBuilder::TryInlineApiSetter(Handle<JSFunction> function,
                                                Handle<Map> receiver_map,
                                                BailoutId ast_id) {
  SmallMapList receiver_maps(1, zone());
  receiver_maps.Add(receiver_map, zone());
  return TryInlineApiCall(function,
                          NULL,  // Receiver is on expression stack.
                          &receiver_maps,
                          1,
                          ast_id,
                          kCallApiSetter);
}


bool HOptimizedGraphBuilder::TryInlineApiCall(Handle<JSFunction> function,
                                               HValue* receiver,
                                               SmallMapList* receiver_maps,
                                               int argc,
                                               BailoutId ast_id,
                                               ApiCallType call_type) {
  CallOptimization optimization(function);
  if (!optimization.is_simple_api_call()) return false;
  Handle<Map> holder_map;
  if (call_type == kCallApiFunction) {
    // Cannot embed a direct reference to the global proxy map
    // as it maybe dropped on deserialization.
    CHECK(!Serializer::enabled());
    ASSERT_EQ(0, receiver_maps->length());
    receiver_maps->Add(handle(
        function->context()->global_object()->global_receiver()->map()),
        zone());
  }
  CallOptimization::HolderLookup holder_lookup =
      CallOptimization::kHolderNotFound;
  Handle<JSObject> api_holder = optimization.LookupHolderOfExpectedType(
      receiver_maps->first(), &holder_lookup);
  if (holder_lookup == CallOptimization::kHolderNotFound) return false;

  if (FLAG_trace_inlining) {
    PrintF("Inlining api function ");
    function->ShortPrint();
    PrintF("\n");
  }

  bool drop_extra = false;
  bool is_store = false;
  switch (call_type) {
    case kCallApiFunction:
    case kCallApiMethod:
      // Need to check that none of the receiver maps could have changed.
      Add<HCheckMaps>(receiver, receiver_maps, top_info());
      // Need to ensure the chain between receiver and api_holder is intact.
      if (holder_lookup == CallOptimization::kHolderFound) {
        AddCheckPrototypeMaps(api_holder, receiver_maps->first());
      } else {
        ASSERT_EQ(holder_lookup, CallOptimization::kHolderIsReceiver);
      }
      // Includes receiver.
      PushArgumentsFromEnvironment(argc + 1);
      // Drop function after call.
      drop_extra = true;
      break;
    case kCallApiGetter:
      // Receiver and prototype chain cannot have changed.
      ASSERT_EQ(0, argc);
      ASSERT_EQ(NULL, receiver);
      // Receiver is on expression stack.
      receiver = Pop();
      Add<HPushArgument>(receiver);
      break;
    case kCallApiSetter:
      {
        is_store = true;
        // Receiver and prototype chain cannot have changed.
        ASSERT_EQ(1, argc);
        ASSERT_EQ(NULL, receiver);
        // Receiver and value are on expression stack.
        HValue* value = Pop();
        receiver = Pop();
        Add<HPushArgument>(receiver);
        Add<HPushArgument>(value);
        break;
     }
  }

  HValue* holder = NULL;
  switch (holder_lookup) {
    case CallOptimization::kHolderFound:
      holder = Add<HConstant>(api_holder);
      break;
    case CallOptimization::kHolderIsReceiver:
      holder = receiver;
      break;
    case CallOptimization::kHolderNotFound:
      UNREACHABLE();
      break;
  }
  Handle<CallHandlerInfo> api_call_info = optimization.api_call_info();
  Handle<Object> call_data_obj(api_call_info->data(), isolate());
  bool call_data_is_undefined = call_data_obj->IsUndefined();
  HValue* call_data = Add<HConstant>(call_data_obj);
  ApiFunction fun(v8::ToCData<Address>(api_call_info->callback()));
  ExternalReference ref = ExternalReference(&fun,
                                            ExternalReference::DIRECT_API_CALL,
                                            isolate());
  HValue* api_function_address = Add<HConstant>(ExternalReference(ref));

  HValue* op_vals[] = {
    Add<HConstant>(function),
    call_data,
    holder,
    api_function_address,
    context()
  };

  CallInterfaceDescriptor* descriptor =
      isolate()->call_descriptor(Isolate::ApiFunctionCall);

  CallApiFunctionStub stub(is_store, call_data_is_undefined, argc);
  Handle<Code> code = stub.GetCode(isolate());
  HConstant* code_value = Add<HConstant>(code);

  ASSERT((sizeof(op_vals) / kPointerSize) ==
         descriptor->environment_length());

  HInstruction* call = New<HCallWithDescriptor>(
      code_value, argc + 1, descriptor,
      Vector<HValue*>(op_vals, descriptor->environment_length()));

  if (drop_extra) Drop(1);  // Drop function.
  ast_context()->ReturnInstruction(call, ast_id);
  return true;
}


bool HOptimizedGraphBuilder::TryCallApply(Call* expr) {
  ASSERT(expr->expression()->IsProperty());

  if (!expr->IsMonomorphic()) {
    return false;
  }
  Handle<Map> function_map = expr->GetReceiverTypes()->first();
  if (function_map->instance_type() != JS_FUNCTION_TYPE ||
      !expr->target()->shared()->HasBuiltinFunctionId() ||
      expr->target()->shared()->builtin_function_id() != kFunctionApply) {
    return false;
  }

  if (current_info()->scope()->arguments() == NULL) return false;

  ZoneList<Expression*>* args = expr->arguments();
  if (args->length() != 2) return false;

  VariableProxy* arg_two = args->at(1)->AsVariableProxy();
  if (arg_two == NULL || !arg_two->var()->IsStackAllocated()) return false;
  HValue* arg_two_value = LookupAndMakeLive(arg_two->var());
  if (!arg_two_value->CheckFlag(HValue::kIsArguments)) return false;

  // Found pattern f.apply(receiver, arguments).
  CHECK_ALIVE_OR_RETURN(VisitForValue(args->at(0)), true);
  HValue* receiver = Pop();  // receiver
  HValue* function = Pop();  // f
  Drop(1);  // apply

  if (function_state()->outer() == NULL) {
    HInstruction* elements = Add<HArgumentsElements>(false);
    HInstruction* length = Add<HArgumentsLength>(elements);
    HValue* wrapped_receiver = BuildWrapReceiver(receiver, function);
    HInstruction* result = New<HApplyArguments>(function,
                                                wrapped_receiver,
                                                length,
                                                elements);
    ast_context()->ReturnInstruction(result, expr->id());
    return true;
  } else {
    // We are inside inlined function and we know exactly what is inside
    // arguments object. But we need to be able to materialize at deopt.
    ASSERT_EQ(environment()->arguments_environment()->parameter_count(),
              function_state()->entry()->arguments_object()->arguments_count());
    HArgumentsObject* args = function_state()->entry()->arguments_object();
    const ZoneList<HValue*>* arguments_values = args->arguments_values();
    int arguments_count = arguments_values->length();
    Push(function);
    Push(BuildWrapReceiver(receiver, function));
    for (int i = 1; i < arguments_count; i++) {
      Push(arguments_values->at(i));
    }

    Handle<JSFunction> known_function;
    if (function->IsConstant() &&
        HConstant::cast(function)->handle(isolate())->IsJSFunction()) {
      known_function = Handle<JSFunction>::cast(
          HConstant::cast(function)->handle(isolate()));
      int args_count = arguments_count - 1;  // Excluding receiver.
      if (TryInlineApply(known_function, expr, args_count)) return true;
    }

    PushArgumentsFromEnvironment(arguments_count);
    HInvokeFunction* call = New<HInvokeFunction>(
        function, known_function, arguments_count);
    Drop(1);  // Function.
    ast_context()->ReturnInstruction(call, expr->id());
    return true;
  }
}


HValue* HOptimizedGraphBuilder::ImplicitReceiverFor(HValue* function,
                                                    Handle<JSFunction> target) {
  SharedFunctionInfo* shared = target->shared();
  if (shared->is_classic_mode() && !shared->native()) {
    // Cannot embed a direct reference to the global proxy
    // as is it dropped on deserialization.
    CHECK(!Serializer::enabled());
    Handle<JSObject> global_receiver(
        target->context()->global_object()->global_receiver());
    return Add<HConstant>(global_receiver);
  }
  return graph()->GetConstantUndefined();
}


void HOptimizedGraphBuilder::VisitCall(Call* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  Expression* callee = expr->expression();
  int argument_count = expr->arguments()->length() + 1;  // Plus receiver.
  HInstruction* call = NULL;

  Property* prop = callee->AsProperty();
  if (prop != NULL) {
    CHECK_ALIVE(VisitForValue(prop->obj()));
    HValue* receiver = Top();

    SmallMapList* types;
    ComputeReceiverTypes(expr, receiver, &types, zone());

    if (prop->key()->IsPropertyName() && types->length() > 0) {
      Handle<String> name = prop->key()->AsLiteral()->AsPropertyName();
      PropertyAccessInfo info(this, LOAD, ToType(types->first()), name);
      if (!info.CanAccessAsMonomorphic(types)) {
        HandlePolymorphicCallNamed(expr, receiver, types, name);
        return;
      }
    }

    HValue* key = NULL;
    if (!prop->key()->IsPropertyName()) {
      CHECK_ALIVE(VisitForValue(prop->key()));
      key = Pop();
    }

    CHECK_ALIVE(PushLoad(prop, receiver, key));
    HValue* function = Pop();

    if (FLAG_hydrogen_track_positions) SetSourcePosition(expr->position());

    // Push the function under the receiver.
    environment()->SetExpressionStackAt(0, function);

    Push(receiver);

    if (function->IsConstant() &&
        HConstant::cast(function)->handle(isolate())->IsJSFunction()) {
      Handle<JSFunction> known_function = Handle<JSFunction>::cast(
          HConstant::cast(function)->handle(isolate()));
      expr->set_target(known_function);

      if (TryCallApply(expr)) return;
      CHECK_ALIVE(VisitExpressions(expr->arguments()));

      Handle<Map> map = types->length() == 1 ? types->first() : Handle<Map>();
      if (TryInlineBuiltinMethodCall(expr, receiver, map)) {
        if (FLAG_trace_inlining) {
          PrintF("Inlining builtin ");
          known_function->ShortPrint();
          PrintF("\n");
        }
        return;
      }
      if (TryInlineApiMethodCall(expr, receiver, types)) return;

      // Wrap the receiver if necessary.
      if (NeedsWrappingFor(ToType(types->first()), known_function)) {
        // Since HWrapReceiver currently cannot actually wrap numbers and
        // strings, use the regular CallFunctionStub for method calls to wrap
        // the receiver.
        // TODO(verwaest): Support creation of value wrappers directly in
        // HWrapReceiver.
        call = New<HCallFunction>(
            function, argument_count, WRAP_AND_CALL);
      } else if (TryInlineCall(expr)) {
        return;
      } else {
        call = BuildCallConstantFunction(known_function, argument_count);
      }

    } else {
      CHECK_ALIVE(VisitExpressions(expr->arguments()));
      CallFunctionFlags flags = receiver->type().IsJSObject()
          ? NO_CALL_FUNCTION_FLAGS : CALL_AS_METHOD;
      call = New<HCallFunction>(function, argument_count, flags);
    }
    PushArgumentsFromEnvironment(argument_count);

  } else {
    VariableProxy* proxy = expr->expression()->AsVariableProxy();
    if (proxy != NULL && proxy->var()->is_possibly_eval(isolate())) {
      return Bailout(kPossibleDirectCallToEval);
    }

    bool global_call = proxy != NULL && proxy->var()->IsUnallocated();
    if (global_call) {
      Variable* var = proxy->var();
      bool known_global_function = false;
      // If there is a global property cell for the name at compile time and
      // access check is not enabled we assume that the function will not change
      // and generate optimized code for calling the function.
      LookupResult lookup(isolate());
      GlobalPropertyAccess type = LookupGlobalProperty(var, &lookup, LOAD);
      if (type == kUseCell &&
          !current_info()->global_object()->IsAccessCheckNeeded()) {
        Handle<GlobalObject> global(current_info()->global_object());
        known_global_function = expr->ComputeGlobalTarget(global, &lookup);
      }
      CHECK_ALIVE(VisitForValue(expr->expression()));
      HValue* function = Top();
      if (known_global_function) {
        Add<HCheckValue>(function, expr->target());

        // Placeholder for the receiver.
        Push(graph()->GetConstantUndefined());
        CHECK_ALIVE(VisitExpressions(expr->arguments()));

        // Patch the global object on the stack by the expected receiver.
        HValue* receiver = ImplicitReceiverFor(function, expr->target());
        const int receiver_index = argument_count - 1;
        environment()->SetExpressionStackAt(receiver_index, receiver);

        if (TryInlineBuiltinFunctionCall(expr)) {
          if (FLAG_trace_inlining) {
            PrintF("Inlining builtin ");
            expr->target()->ShortPrint();
            PrintF("\n");
          }
          return;
        }
        if (TryInlineApiFunctionCall(expr, receiver)) return;
        if (TryInlineCall(expr)) return;

        PushArgumentsFromEnvironment(argument_count);
        call = BuildCallConstantFunction(expr->target(), argument_count);
      } else {
        Push(Add<HPushArgument>(graph()->GetConstantUndefined()));
        CHECK_ALIVE(VisitArgumentList(expr->arguments()));
        call = New<HCallFunction>(function, argument_count);
        Drop(argument_count);
      }

    } else if (expr->IsMonomorphic()) {
      // The function is on the stack in the unoptimized code during
      // evaluation of the arguments.
      CHECK_ALIVE(VisitForValue(expr->expression()));
      HValue* function = Top();

      Add<HCheckValue>(function, expr->target());

      Push(graph()->GetConstantUndefined());
      CHECK_ALIVE(VisitExpressions(expr->arguments()));

      HValue* receiver = ImplicitReceiverFor(function, expr->target());
      const int receiver_index = argument_count - 1;
      environment()->SetExpressionStackAt(receiver_index, receiver);

      if (TryInlineBuiltinFunctionCall(expr)) {
        if (FLAG_trace_inlining) {
          PrintF("Inlining builtin ");
          expr->target()->ShortPrint();
          PrintF("\n");
        }
        return;
      }
      if (TryInlineApiFunctionCall(expr, receiver)) return;

      if (TryInlineCall(expr)) return;

      call = PreProcessCall(New<HInvokeFunction>(
          function, expr->target(), argument_count));

    } else {
      CHECK_ALIVE(VisitForValue(expr->expression()));
      HValue* function = Top();
      HValue* receiver = graph()->GetConstantUndefined();
      Push(Add<HPushArgument>(receiver));
      CHECK_ALIVE(VisitArgumentList(expr->arguments()));
      call = New<HCallFunction>(function, argument_count);
      Drop(argument_count);
    }
  }

  Drop(1);  // Drop the function.
  return ast_context()->ReturnInstruction(call, expr->id());
}


void HOptimizedGraphBuilder::BuildInlinedCallNewArray(CallNew* expr) {
  NoObservableSideEffectsScope no_effects(this);

  int argument_count = expr->arguments()->length();
  // We should at least have the constructor on the expression stack.
  HValue* constructor = environment()->ExpressionStackAt(argument_count);

  ElementsKind kind = expr->elements_kind();
  Handle<AllocationSite> site = expr->allocation_site();
  ASSERT(!site.is_null());

  // Register on the site for deoptimization if the transition feedback changes.
  AllocationSite::AddDependentCompilationInfo(
      site, AllocationSite::TRANSITIONS, top_info());
  HInstruction* site_instruction = Add<HConstant>(site);

  // In the single constant argument case, we may have to adjust elements kind
  // to avoid creating a packed non-empty array.
  if (argument_count == 1 && !IsHoleyElementsKind(kind)) {
    HValue* argument = environment()->Top();
    if (argument->IsConstant()) {
      HConstant* constant_argument = HConstant::cast(argument);
      ASSERT(constant_argument->HasSmiValue());
      int constant_array_size = constant_argument->Integer32Value();
      if (constant_array_size != 0) {
        kind = GetHoleyElementsKind(kind);
      }
    }
  }

  // Build the array.
  JSArrayBuilder array_builder(this,
                               kind,
                               site_instruction,
                               constructor,
                               DISABLE_ALLOCATION_SITES);
  HValue* new_object;
  if (argument_count == 0) {
    new_object = array_builder.AllocateEmptyArray();
  } else if (argument_count == 1) {
    HValue* argument = environment()->Top();
    new_object = BuildAllocateArrayFromLength(&array_builder, argument);
  } else {
    HValue* length = Add<HConstant>(argument_count);
    // Smi arrays need to initialize array elements with the hole because
    // bailout could occur if the arguments don't fit in a smi.
    //
    // TODO(mvstanton): If all the arguments are constants in smi range, then
    // we could set fill_with_hole to false and save a few instructions.
    JSArrayBuilder::FillMode fill_mode = IsFastSmiElementsKind(kind)
        ? JSArrayBuilder::FILL_WITH_HOLE
        : JSArrayBuilder::DONT_FILL_WITH_HOLE;
    new_object = array_builder.AllocateArray(length, length, fill_mode);
    HValue* elements = array_builder.GetElementsLocation();
    for (int i = 0; i < argument_count; i++) {
      HValue* value = environment()->ExpressionStackAt(argument_count - i - 1);
      HValue* constant_i = Add<HConstant>(i);
      Add<HStoreKeyed>(elements, constant_i, value, kind);
    }
  }

  Drop(argument_count + 1);  // drop constructor and args.
  ast_context()->ReturnValue(new_object);
}


// Checks whether allocation using the given constructor can be inlined.
static bool IsAllocationInlineable(Handle<JSFunction> constructor) {
  return constructor->has_initial_map() &&
      constructor->initial_map()->instance_type() == JS_OBJECT_TYPE &&
      constructor->initial_map()->instance_size() < HAllocate::kMaxInlineSize &&
      constructor->initial_map()->InitialPropertiesLength() == 0;
}


bool HOptimizedGraphBuilder::IsCallNewArrayInlineable(CallNew* expr) {
  bool inline_ok = false;
  Handle<JSFunction> caller = current_info()->closure();
  Handle<JSFunction> target(isolate()->global_context()->array_function(),
                            isolate());
  int argument_count = expr->arguments()->length();
  // We should have the function plus array arguments on the environment stack.
  ASSERT(environment()->length() >= (argument_count + 1));
  Handle<AllocationSite> site = expr->allocation_site();
  ASSERT(!site.is_null());

  if (site->CanInlineCall()) {
    // We also want to avoid inlining in certain 1 argument scenarios.
    if (argument_count == 1) {
      HValue* argument = Top();
      if (argument->IsConstant()) {
        // Do not inline if the constant length argument is not a smi or
        // outside the valid range for a fast array.
        HConstant* constant_argument = HConstant::cast(argument);
        if (constant_argument->HasSmiValue()) {
          int value = constant_argument->Integer32Value();
          inline_ok = value >= 0 &&
              value < JSObject::kInitialMaxFastElementArray;
          if (!inline_ok) {
            TraceInline(target, caller,
                        "Length outside of valid array range");
          }
        }
      } else {
        inline_ok = true;
      }
    } else {
      inline_ok = true;
    }
  } else {
    TraceInline(target, caller, "AllocationSite requested no inlining.");
  }

  if (inline_ok) {
    TraceInline(target, caller, NULL);
  }
  return inline_ok;
}


void HOptimizedGraphBuilder::VisitCallNew(CallNew* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (!FLAG_hydrogen_track_positions) SetSourcePosition(expr->position());
  int argument_count = expr->arguments()->length() + 1;  // Plus constructor.
  Factory* factory = isolate()->factory();

  // The constructor function is on the stack in the unoptimized code
  // during evaluation of the arguments.
  CHECK_ALIVE(VisitForValue(expr->expression()));
  HValue* function = Top();
  CHECK_ALIVE(VisitExpressions(expr->arguments()));

  if (FLAG_inline_construct &&
      expr->IsMonomorphic() &&
      IsAllocationInlineable(expr->target())) {
    Handle<JSFunction> constructor = expr->target();
    HValue* check = Add<HCheckValue>(function, constructor);

    // Force completion of inobject slack tracking before generating
    // allocation code to finalize instance size.
    if (constructor->shared()->IsInobjectSlackTrackingInProgress()) {
      constructor->shared()->CompleteInobjectSlackTracking();
    }

    // Calculate instance size from initial map of constructor.
    ASSERT(constructor->has_initial_map());
    Handle<Map> initial_map(constructor->initial_map());
    int instance_size = initial_map->instance_size();
    ASSERT(initial_map->InitialPropertiesLength() == 0);

    // Allocate an instance of the implicit receiver object.
    HValue* size_in_bytes = Add<HConstant>(instance_size);
    PretenureFlag pretenure_flag =
        (FLAG_pretenuring_call_new && !FLAG_allocation_site_pretenuring) ?
            isolate()->heap()->GetPretenureMode() : NOT_TENURED;
    HAllocate* receiver =
        Add<HAllocate>(size_in_bytes, HType::JSObject(), pretenure_flag,
        JS_OBJECT_TYPE);
    receiver->set_known_initial_map(initial_map);

    // Load the initial map from the constructor.
    HValue* constructor_value = Add<HConstant>(constructor);
    HValue* initial_map_value =
      Add<HLoadNamedField>(constructor_value, static_cast<HValue*>(NULL),
                           HObjectAccess::ForMapAndOffset(
                               handle(constructor->map()),
                               JSFunction::kPrototypeOrInitialMapOffset));

    // Initialize map and fields of the newly allocated object.
    { NoObservableSideEffectsScope no_effects(this);
      ASSERT(initial_map->instance_type() == JS_OBJECT_TYPE);
      Add<HStoreNamedField>(receiver,
          HObjectAccess::ForMapAndOffset(initial_map, JSObject::kMapOffset),
          initial_map_value);
      HValue* empty_fixed_array = Add<HConstant>(factory->empty_fixed_array());
      Add<HStoreNamedField>(receiver,
          HObjectAccess::ForMapAndOffset(initial_map,
                                         JSObject::kPropertiesOffset),
          empty_fixed_array);
      Add<HStoreNamedField>(receiver,
          HObjectAccess::ForMapAndOffset(initial_map,
                                         JSObject::kElementsOffset),
          empty_fixed_array);
      if (initial_map->inobject_properties() != 0) {
        HConstant* undefined = graph()->GetConstantUndefined();
        for (int i = 0; i < initial_map->inobject_properties(); i++) {
          int property_offset = initial_map->GetInObjectPropertyOffset(i);
          Add<HStoreNamedField>(receiver,
              HObjectAccess::ForMapAndOffset(initial_map, property_offset),
              undefined);
        }
      }
    }

    // Replace the constructor function with a newly allocated receiver using
    // the index of the receiver from the top of the expression stack.
    const int receiver_index = argument_count - 1;
    ASSERT(environment()->ExpressionStackAt(receiver_index) == function);
    environment()->SetExpressionStackAt(receiver_index, receiver);

    if (TryInlineConstruct(expr, receiver)) return;

    // TODO(mstarzinger): For now we remove the previous HAllocate and all
    // corresponding instructions and instead add HPushArgument for the
    // arguments in case inlining failed.  What we actually should do is for
    // inlining to try to build a subgraph without mutating the parent graph.
    HInstruction* instr = current_block()->last();
    while (instr != initial_map_value) {
      HInstruction* prev_instr = instr->previous();
      instr->DeleteAndReplaceWith(NULL);
      instr = prev_instr;
    }
    initial_map_value->DeleteAndReplaceWith(NULL);
    receiver->DeleteAndReplaceWith(NULL);
    check->DeleteAndReplaceWith(NULL);
    environment()->SetExpressionStackAt(receiver_index, function);
    HInstruction* call =
      PreProcessCall(New<HCallNew>(function, argument_count));
    return ast_context()->ReturnInstruction(call, expr->id());
  } else {
    // The constructor function is both an operand to the instruction and an
    // argument to the construct call.
    Handle<JSFunction> array_function(
        isolate()->global_context()->array_function(), isolate());
    bool use_call_new_array = expr->target().is_identical_to(array_function);
    if (use_call_new_array && IsCallNewArrayInlineable(expr)) {
      // Verify we are still calling the array function for our native context.
      Add<HCheckValue>(function, array_function);
      BuildInlinedCallNewArray(expr);
      return;
    }

    HBinaryCall* call;
    if (use_call_new_array) {
      Add<HCheckValue>(function, array_function);
      call = New<HCallNewArray>(function, argument_count,
                                expr->elements_kind());
    } else {
      call = New<HCallNew>(function, argument_count);
    }
    PreProcessCall(call);
    return ast_context()->ReturnInstruction(call, expr->id());
  }
}


// Support for generating inlined runtime functions.

// Lookup table for generators for runtime calls that are generated inline.
// Elements of the table are member pointers to functions of
// HOptimizedGraphBuilder.
#define INLINE_FUNCTION_GENERATOR_ADDRESS(Name, argc, ressize)  \
    &HOptimizedGraphBuilder::Generate##Name,

const HOptimizedGraphBuilder::InlineFunctionGenerator
    HOptimizedGraphBuilder::kInlineFunctionGenerators[] = {
        INLINE_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_ADDRESS)
        INLINE_RUNTIME_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_ADDRESS)
};
#undef INLINE_FUNCTION_GENERATOR_ADDRESS


template <class ViewClass>
void HGraphBuilder::BuildArrayBufferViewInitialization(
    HValue* obj,
    HValue* buffer,
    HValue* byte_offset,
    HValue* byte_length) {

  for (int offset = ViewClass::kSize;
       offset < ViewClass::kSizeWithInternalFields;
       offset += kPointerSize) {
    Add<HStoreNamedField>(obj,
        HObjectAccess::ForObservableJSObjectOffset(offset),
        graph()->GetConstant0());
  }

  Add<HStoreNamedField>(
      obj,
      HObjectAccess::ForJSArrayBufferViewBuffer(), buffer);
  Add<HStoreNamedField>(
      obj,
      HObjectAccess::ForJSArrayBufferViewByteOffset(),
      byte_offset);
  Add<HStoreNamedField>(
      obj,
      HObjectAccess::ForJSArrayBufferViewByteLength(),
      byte_length);

  HObjectAccess weak_first_view_access =
      HObjectAccess::ForJSArrayBufferWeakFirstView();
  Add<HStoreNamedField>(obj,
      HObjectAccess::ForJSArrayBufferViewWeakNext(),
      Add<HLoadNamedField>(buffer, static_cast<HValue*>(NULL),
                           weak_first_view_access));
  Add<HStoreNamedField>(
      buffer, weak_first_view_access, obj);
}


void HOptimizedGraphBuilder::VisitDataViewInitialize(
    CallRuntime* expr) {
  ZoneList<Expression*>* arguments = expr->arguments();

  NoObservableSideEffectsScope scope(this);
  ASSERT(arguments->length()== 4);
  CHECK_ALIVE(VisitForValue(arguments->at(0)));
  HValue* obj = Pop();

  CHECK_ALIVE(VisitForValue(arguments->at(1)));
  HValue* buffer = Pop();

  CHECK_ALIVE(VisitForValue(arguments->at(2)));
  HValue* byte_offset = Pop();

  CHECK_ALIVE(VisitForValue(arguments->at(3)));
  HValue* byte_length = Pop();

  BuildArrayBufferViewInitialization<JSDataView>(
      obj, buffer, byte_offset, byte_length);
}


void HOptimizedGraphBuilder::VisitTypedArrayInitialize(
    CallRuntime* expr) {
  ZoneList<Expression*>* arguments = expr->arguments();

  NoObservableSideEffectsScope scope(this);
  static const int kObjectArg = 0;
  static const int kArrayIdArg = 1;
  static const int kBufferArg = 2;
  static const int kByteOffsetArg = 3;
  static const int kByteLengthArg = 4;
  static const int kArgsLength = 5;
  ASSERT(arguments->length() == kArgsLength);


  CHECK_ALIVE(VisitForValue(arguments->at(kObjectArg)));
  HValue* obj = Pop();

  ASSERT(arguments->at(kArrayIdArg)->node_type() == AstNode::kLiteral);
  Handle<Object> value =
      static_cast<Literal*>(arguments->at(kArrayIdArg))->value();
  ASSERT(value->IsSmi());
  int array_id = Smi::cast(*value)->value();

  CHECK_ALIVE(VisitForValue(arguments->at(kBufferArg)));
  HValue* buffer = Pop();

  HValue* byte_offset;
  bool is_zero_byte_offset;

  if (arguments->at(kByteOffsetArg)->node_type() == AstNode::kLiteral
      && Smi::FromInt(0) ==
      *static_cast<Literal*>(arguments->at(kByteOffsetArg))->value()) {
    byte_offset = Add<HConstant>(static_cast<int32_t>(0));
    is_zero_byte_offset = true;
  } else {
    CHECK_ALIVE(VisitForValue(arguments->at(kByteOffsetArg)));
    byte_offset = Pop();
    is_zero_byte_offset = false;
  }

  CHECK_ALIVE(VisitForValue(arguments->at(kByteLengthArg)));
  HValue* byte_length = Pop();

  IfBuilder byte_offset_smi(this);

  if (!is_zero_byte_offset) {
    byte_offset_smi.If<HIsSmiAndBranch>(byte_offset);
    byte_offset_smi.Then();
  }

  { //  byte_offset is Smi.
    BuildArrayBufferViewInitialization<JSTypedArray>(
        obj, buffer, byte_offset, byte_length);

    ExternalArrayType array_type = kExternalInt8Array;  // Bogus initialization.
    size_t element_size = 1;  // Bogus initialization.
    Runtime::ArrayIdToTypeAndSize(array_id, &array_type, &element_size);

    HInstruction* length = AddUncasted<HDiv>(byte_length,
        Add<HConstant>(static_cast<int32_t>(element_size)));

    Add<HStoreNamedField>(obj,
        HObjectAccess::ForJSTypedArrayLength(),
        length);

    Handle<Map> external_array_map(
        isolate()->heap()->MapForExternalArrayType(array_type));

    HValue* elements =
        Add<HAllocate>(
            Add<HConstant>(ExternalArray::kAlignedSize),
            HType::JSArray(),
            NOT_TENURED,
            external_array_map->instance_type());

    AddStoreMapConstant(elements, external_array_map);

    HValue* backing_store = Add<HLoadNamedField>(
        buffer, static_cast<HValue*>(NULL),
        HObjectAccess::ForJSArrayBufferBackingStore());

    HValue* typed_array_start;
    if (is_zero_byte_offset) {
      typed_array_start = backing_store;
    } else {
      HInstruction* external_pointer =
          AddUncasted<HAdd>(backing_store, byte_offset);
      // Arguments are checked prior to call to TypedArrayInitialize,
      // including byte_offset.
      external_pointer->ClearFlag(HValue::kCanOverflow);
      typed_array_start = external_pointer;
    }

    Add<HStoreNamedField>(elements,
        HObjectAccess::ForExternalArrayExternalPointer(),
        typed_array_start);
    Add<HStoreNamedField>(elements,
        HObjectAccess::ForFixedArrayLength(),
        length);
    Add<HStoreNamedField>(
        obj, HObjectAccess::ForElementsPointer(), elements);
  }

  if (!is_zero_byte_offset) {
    byte_offset_smi.Else();
    { //  byte_offset is not Smi.
      Push(Add<HPushArgument>(obj));
      VisitArgument(arguments->at(kArrayIdArg));
      Push(Add<HPushArgument>(buffer));
      Push(Add<HPushArgument>(byte_offset));
      Push(Add<HPushArgument>(byte_length));
      Add<HCallRuntime>(expr->name(), expr->function(), kArgsLength);
      Drop(kArgsLength);
    }
  }
  byte_offset_smi.End();
}


void HOptimizedGraphBuilder::VisitCallRuntime(CallRuntime* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (expr->is_jsruntime()) {
    return Bailout(kCallToAJavaScriptRuntimeFunction);
  }

  const Runtime::Function* function = expr->function();
  ASSERT(function != NULL);

  if (function->function_id == Runtime::kDataViewInitialize) {
      return VisitDataViewInitialize(expr);
  }

  if (function->function_id == Runtime::kTypedArrayInitialize) {
    return VisitTypedArrayInitialize(expr);
  }

  if (function->function_id == Runtime::kMaxSmi) {
    ASSERT(expr->arguments()->length() == 0);
    HConstant* max_smi = New<HConstant>(static_cast<int32_t>(Smi::kMaxValue));
    return ast_context()->ReturnInstruction(max_smi, expr->id());
  }

  if (function->intrinsic_type == Runtime::INLINE) {
    ASSERT(expr->name()->length() > 0);
    ASSERT(expr->name()->Get(0) == '_');
    // Call to an inline function.
    int lookup_index = static_cast<int>(function->function_id) -
        static_cast<int>(Runtime::kFirstInlineFunction);
    ASSERT(lookup_index >= 0);
    ASSERT(static_cast<size_t>(lookup_index) <
           ARRAY_SIZE(kInlineFunctionGenerators));
    InlineFunctionGenerator generator = kInlineFunctionGenerators[lookup_index];

    // Call the inline code generator using the pointer-to-member.
    (this->*generator)(expr);
  } else {
    ASSERT(function->intrinsic_type == Runtime::RUNTIME);
    CHECK_ALIVE(VisitArgumentList(expr->arguments()));

    Handle<String> name = expr->name();
    int argument_count = expr->arguments()->length();
    HCallRuntime* call = New<HCallRuntime>(name, function,
                                           argument_count);
    Drop(argument_count);
    return ast_context()->ReturnInstruction(call, expr->id());
  }
}


void HOptimizedGraphBuilder::VisitUnaryOperation(UnaryOperation* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  switch (expr->op()) {
    case Token::DELETE: return VisitDelete(expr);
    case Token::VOID: return VisitVoid(expr);
    case Token::TYPEOF: return VisitTypeof(expr);
    case Token::NOT: return VisitNot(expr);
    default: UNREACHABLE();
  }
}


void HOptimizedGraphBuilder::VisitDelete(UnaryOperation* expr) {
  Property* prop = expr->expression()->AsProperty();
  VariableProxy* proxy = expr->expression()->AsVariableProxy();
  if (prop != NULL) {
    CHECK_ALIVE(VisitForValue(prop->obj()));
    CHECK_ALIVE(VisitForValue(prop->key()));
    HValue* key = Pop();
    HValue* obj = Pop();
    HValue* function = AddLoadJSBuiltin(Builtins::DELETE);
    Add<HPushArgument>(obj);
    Add<HPushArgument>(key);
    Add<HPushArgument>(Add<HConstant>(function_strict_mode_flag()));
    // TODO(olivf) InvokeFunction produces a check for the parameter count,
    // even though we are certain to pass the correct number of arguments here.
    HInstruction* instr = New<HInvokeFunction>(function, 3);
    return ast_context()->ReturnInstruction(instr, expr->id());
  } else if (proxy != NULL) {
    Variable* var = proxy->var();
    if (var->IsUnallocated()) {
      Bailout(kDeleteWithGlobalVariable);
    } else if (var->IsStackAllocated() || var->IsContextSlot()) {
      // Result of deleting non-global variables is false.  'this' is not
      // really a variable, though we implement it as one.  The
      // subexpression does not have side effects.
      HValue* value = var->is_this()
          ? graph()->GetConstantTrue()
          : graph()->GetConstantFalse();
      return ast_context()->ReturnValue(value);
    } else {
      Bailout(kDeleteWithNonGlobalVariable);
    }
  } else {
    // Result of deleting non-property, non-variable reference is true.
    // Evaluate the subexpression for side effects.
    CHECK_ALIVE(VisitForEffect(expr->expression()));
    return ast_context()->ReturnValue(graph()->GetConstantTrue());
  }
}


void HOptimizedGraphBuilder::VisitVoid(UnaryOperation* expr) {
  CHECK_ALIVE(VisitForEffect(expr->expression()));
  return ast_context()->ReturnValue(graph()->GetConstantUndefined());
}


void HOptimizedGraphBuilder::VisitTypeof(UnaryOperation* expr) {
  CHECK_ALIVE(VisitForTypeOf(expr->expression()));
  HValue* value = Pop();
  HInstruction* instr = New<HTypeof>(value);
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitNot(UnaryOperation* expr) {
  if (ast_context()->IsTest()) {
    TestContext* context = TestContext::cast(ast_context());
    VisitForControl(expr->expression(),
                    context->if_false(),
                    context->if_true());
    return;
  }

  if (ast_context()->IsEffect()) {
    VisitForEffect(expr->expression());
    return;
  }

  ASSERT(ast_context()->IsValue());
  HBasicBlock* materialize_false = graph()->CreateBasicBlock();
  HBasicBlock* materialize_true = graph()->CreateBasicBlock();
  CHECK_BAILOUT(VisitForControl(expr->expression(),
                                materialize_false,
                                materialize_true));

  if (materialize_false->HasPredecessor()) {
    materialize_false->SetJoinId(expr->MaterializeFalseId());
    set_current_block(materialize_false);
    Push(graph()->GetConstantFalse());
  } else {
    materialize_false = NULL;
  }

  if (materialize_true->HasPredecessor()) {
    materialize_true->SetJoinId(expr->MaterializeTrueId());
    set_current_block(materialize_true);
    Push(graph()->GetConstantTrue());
  } else {
    materialize_true = NULL;
  }

  HBasicBlock* join =
    CreateJoin(materialize_false, materialize_true, expr->id());
  set_current_block(join);
  if (join != NULL) return ast_context()->ReturnValue(Pop());
}


HInstruction* HOptimizedGraphBuilder::BuildIncrement(
    bool returns_original_input,
    CountOperation* expr) {
  // The input to the count operation is on top of the expression stack.
  Representation rep = Representation::FromType(expr->type());
  if (rep.IsNone() || rep.IsTagged()) {
    rep = Representation::Smi();
  }

  if (returns_original_input) {
    // We need an explicit HValue representing ToNumber(input).  The
    // actual HChange instruction we need is (sometimes) added in a later
    // phase, so it is not available now to be used as an input to HAdd and
    // as the return value.
    HInstruction* number_input = AddUncasted<HForceRepresentation>(Pop(), rep);
    if (!rep.IsDouble()) {
      number_input->SetFlag(HInstruction::kFlexibleRepresentation);
      number_input->SetFlag(HInstruction::kCannotBeTagged);
    }
    Push(number_input);
  }

  // The addition has no side effects, so we do not need
  // to simulate the expression stack after this instruction.
  // Any later failures deopt to the load of the input or earlier.
  HConstant* delta = (expr->op() == Token::INC)
      ? graph()->GetConstant1()
      : graph()->GetConstantMinus1();
  HInstruction* instr = AddUncasted<HAdd>(Top(), delta);
  if (instr->IsAdd()) {
    HAdd* add = HAdd::cast(instr);
    add->set_observed_input_representation(1, rep);
    add->set_observed_input_representation(2, Representation::Smi());
  }
  instr->SetFlag(HInstruction::kCannotBeTagged);
  instr->ClearAllSideEffects();
  return instr;
}


void HOptimizedGraphBuilder::BuildStoreForEffect(Expression* expr,
                                                 Property* prop,
                                                 BailoutId ast_id,
                                                 BailoutId return_id,
                                                 HValue* object,
                                                 HValue* key,
                                                 HValue* value) {
  EffectContext for_effect(this);
  Push(object);
  if (key != NULL) Push(key);
  Push(value);
  BuildStore(expr, prop, ast_id, return_id);
}


void HOptimizedGraphBuilder::VisitCountOperation(CountOperation* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (!FLAG_hydrogen_track_positions) SetSourcePosition(expr->position());
  Expression* target = expr->expression();
  VariableProxy* proxy = target->AsVariableProxy();
  Property* prop = target->AsProperty();
  if (proxy == NULL && prop == NULL) {
    return Bailout(kInvalidLhsInCountOperation);
  }

  // Match the full code generator stack by simulating an extra stack
  // element for postfix operations in a non-effect context.  The return
  // value is ToNumber(input).
  bool returns_original_input =
      expr->is_postfix() && !ast_context()->IsEffect();
  HValue* input = NULL;  // ToNumber(original_input).
  HValue* after = NULL;  // The result after incrementing or decrementing.

  if (proxy != NULL) {
    Variable* var = proxy->var();
    if (var->mode() == CONST)  {
      return Bailout(kUnsupportedCountOperationWithConst);
    }
    // Argument of the count operation is a variable, not a property.
    ASSERT(prop == NULL);
    CHECK_ALIVE(VisitForValue(target));

    after = BuildIncrement(returns_original_input, expr);
    input = returns_original_input ? Top() : Pop();
    Push(after);

    switch (var->location()) {
      case Variable::UNALLOCATED:
        HandleGlobalVariableAssignment(var,
                                       after,
                                       expr->AssignmentId());
        break;

      case Variable::PARAMETER:
      case Variable::LOCAL:
        BindIfLive(var, after);
        break;

      case Variable::CONTEXT: {
        // Bail out if we try to mutate a parameter value in a function
        // using the arguments object.  We do not (yet) correctly handle the
        // arguments property of the function.
        if (current_info()->scope()->arguments() != NULL) {
          // Parameters will rewrite to context slots.  We have no direct
          // way to detect that the variable is a parameter so we use a
          // linear search of the parameter list.
          int count = current_info()->scope()->num_parameters();
          for (int i = 0; i < count; ++i) {
            if (var == current_info()->scope()->parameter(i)) {
              return Bailout(kAssignmentToParameterInArgumentsObject);
            }
          }
        }

        HValue* context = BuildContextChainWalk(var);
        HStoreContextSlot::Mode mode = IsLexicalVariableMode(var->mode())
            ? HStoreContextSlot::kCheckDeoptimize : HStoreContextSlot::kNoCheck;
        HStoreContextSlot* instr = Add<HStoreContextSlot>(context, var->index(),
                                                          mode, after);
        if (instr->HasObservableSideEffects()) {
          Add<HSimulate>(expr->AssignmentId(), REMOVABLE_SIMULATE);
        }
        break;
      }

      case Variable::LOOKUP:
        return Bailout(kLookupVariableInCountOperation);
    }

    Drop(returns_original_input ? 2 : 1);
    return ast_context()->ReturnValue(expr->is_postfix() ? input : after);
  }

  // Argument of the count operation is a property.
  ASSERT(prop != NULL);
  if (returns_original_input) Push(graph()->GetConstantUndefined());

  CHECK_ALIVE(VisitForValue(prop->obj()));
  HValue* object = Top();

  HValue* key = NULL;
  if ((!prop->IsFunctionPrototype() && !prop->key()->IsPropertyName()) ||
      prop->IsStringAccess()) {
    CHECK_ALIVE(VisitForValue(prop->key()));
    key = Top();
  }

  CHECK_ALIVE(PushLoad(prop, object, key));

  after = BuildIncrement(returns_original_input, expr);

  if (returns_original_input) {
    input = Pop();
    // Drop object and key to push it again in the effect context below.
    Drop(key == NULL ? 1 : 2);
    environment()->SetExpressionStackAt(0, input);
    CHECK_ALIVE(BuildStoreForEffect(
        expr, prop, expr->id(), expr->AssignmentId(), object, key, after));
    return ast_context()->ReturnValue(Pop());
  }

  environment()->SetExpressionStackAt(0, after);
  return BuildStore(expr, prop, expr->id(), expr->AssignmentId());
}


HInstruction* HOptimizedGraphBuilder::BuildStringCharCodeAt(
    HValue* string,
    HValue* index) {
  if (string->IsConstant() && index->IsConstant()) {
    HConstant* c_string = HConstant::cast(string);
    HConstant* c_index = HConstant::cast(index);
    if (c_string->HasStringValue() && c_index->HasNumberValue()) {
      int32_t i = c_index->NumberValueAsInteger32();
      Handle<String> s = c_string->StringValue();
      if (i < 0 || i >= s->length()) {
        return New<HConstant>(OS::nan_value());
      }
      return New<HConstant>(s->Get(i));
    }
  }
  string = BuildCheckString(string);
  index = Add<HBoundsCheck>(index, AddLoadStringLength(string));
  return New<HStringCharCodeAt>(string, index);
}


// Checks if the given shift amounts have following forms:
// (N1) and (N2) with N1 + N2 = 32; (sa) and (32 - sa).
static bool ShiftAmountsAllowReplaceByRotate(HValue* sa,
                                             HValue* const32_minus_sa) {
  if (sa->IsConstant() && const32_minus_sa->IsConstant()) {
    const HConstant* c1 = HConstant::cast(sa);
    const HConstant* c2 = HConstant::cast(const32_minus_sa);
    return c1->HasInteger32Value() && c2->HasInteger32Value() &&
        (c1->Integer32Value() + c2->Integer32Value() == 32);
  }
  if (!const32_minus_sa->IsSub()) return false;
  HSub* sub = HSub::cast(const32_minus_sa);
  if (sa != sub->right()) return false;
  HValue* const32 = sub->left();
  if (!const32->IsConstant() ||
      HConstant::cast(const32)->Integer32Value() != 32) {
    return false;
  }
  return (sub->right() == sa);
}


// Checks if the left and the right are shift instructions with the oposite
// directions that can be replaced by one rotate right instruction or not.
// Returns the operand and the shift amount for the rotate instruction in the
// former case.
bool HGraphBuilder::MatchRotateRight(HValue* left,
                                     HValue* right,
                                     HValue** operand,
                                     HValue** shift_amount) {
  HShl* shl;
  HShr* shr;
  if (left->IsShl() && right->IsShr()) {
    shl = HShl::cast(left);
    shr = HShr::cast(right);
  } else if (left->IsShr() && right->IsShl()) {
    shl = HShl::cast(right);
    shr = HShr::cast(left);
  } else {
    return false;
  }
  if (shl->left() != shr->left()) return false;

  if (!ShiftAmountsAllowReplaceByRotate(shl->right(), shr->right()) &&
      !ShiftAmountsAllowReplaceByRotate(shr->right(), shl->right())) {
    return false;
  }
  *operand= shr->left();
  *shift_amount = shr->right();
  return true;
}


bool CanBeZero(HValue* right) {
  if (right->IsConstant()) {
    HConstant* right_const = HConstant::cast(right);
    if (right_const->HasInteger32Value() &&
       (right_const->Integer32Value() & 0x1f) != 0) {
      return false;
    }
  }
  return true;
}


HValue* HGraphBuilder::EnforceNumberType(HValue* number,
                                         Type* expected) {
  if (expected->Is(Type::Smi())) {
    return AddUncasted<HForceRepresentation>(number, Representation::Smi());
  }
  if (expected->Is(Type::Signed32())) {
    return AddUncasted<HForceRepresentation>(number,
                                             Representation::Integer32());
  }
  return number;
}


HValue* HGraphBuilder::TruncateToNumber(HValue* value, Type** expected) {
  if (value->IsConstant()) {
    HConstant* constant = HConstant::cast(value);
    Maybe<HConstant*> number = constant->CopyToTruncatedNumber(zone());
    if (number.has_value) {
      *expected = Type::Number(zone());
      return AddInstruction(number.value);
    }
  }

  // We put temporary values on the stack, which don't correspond to anything
  // in baseline code. Since nothing is observable we avoid recording those
  // pushes with a NoObservableSideEffectsScope.
  NoObservableSideEffectsScope no_effects(this);

  Type* expected_type = *expected;

  // Separate the number type from the rest.
  Type* expected_obj =
      Type::Intersect(expected_type, Type::NonNumber(zone()), zone());
  Type* expected_number =
      Type::Intersect(expected_type, Type::Number(zone()), zone());

  // We expect to get a number.
  // (We need to check first, since Type::None->Is(Type::Any()) == true.
  if (expected_obj->Is(Type::None())) {
    ASSERT(!expected_number->Is(Type::None(zone())));
    return value;
  }

  if (expected_obj->Is(Type::Undefined(zone()))) {
    // This is already done by HChange.
    *expected = Type::Union(expected_number, Type::Double(zone()), zone());
    return value;
  }

  return value;
}


HValue* HOptimizedGraphBuilder::BuildBinaryOperation(
    BinaryOperation* expr,
    HValue* left,
    HValue* right,
    PushBeforeSimulateBehavior push_sim_result) {
  Type* left_type = expr->left()->bounds().lower;
  Type* right_type = expr->right()->bounds().lower;
  Type* result_type = expr->bounds().lower;
  Maybe<int> fixed_right_arg = expr->fixed_right_arg();
  Handle<AllocationSite> allocation_site = expr->allocation_site();

  PretenureFlag pretenure_flag = !FLAG_allocation_site_pretenuring ?
      isolate()->heap()->GetPretenureMode() : NOT_TENURED;

  HAllocationMode allocation_mode =
      FLAG_allocation_site_pretenuring
      ? (allocation_site.is_null()
         ? HAllocationMode(NOT_TENURED)
         : HAllocationMode(allocation_site))
      : HAllocationMode(pretenure_flag);

  HValue* result = HGraphBuilder::BuildBinaryOperation(
      expr->op(), left, right, left_type, right_type, result_type,
      fixed_right_arg, allocation_mode);
  // Add a simulate after instructions with observable side effects, and
  // after phis, which are the result of BuildBinaryOperation when we
  // inlined some complex subgraph.
  if (result->HasObservableSideEffects() || result->IsPhi()) {
    if (push_sim_result == PUSH_BEFORE_SIMULATE) {
      Push(result);
      Add<HSimulate>(expr->id(), REMOVABLE_SIMULATE);
      Drop(1);
    } else {
      Add<HSimulate>(expr->id(), REMOVABLE_SIMULATE);
    }
  }
  return result;
}


HValue* HGraphBuilder::BuildBinaryOperation(
    Token::Value op,
    HValue* left,
    HValue* right,
    Type* left_type,
    Type* right_type,
    Type* result_type,
    Maybe<int> fixed_right_arg,
    HAllocationMode allocation_mode) {

  Representation left_rep = Representation::FromType(left_type);
  Representation right_rep = Representation::FromType(right_type);

  bool maybe_string_add = op == Token::ADD &&
                          (left_type->Maybe(Type::String()) ||
                           right_type->Maybe(Type::String()));

  if (left_type->Is(Type::None())) {
    Add<HDeoptimize>("Insufficient type feedback for LHS of binary operation",
                     Deoptimizer::SOFT);
    // TODO(rossberg): we should be able to get rid of non-continuous
    // defaults.
    left_type = Type::Any(zone());
  } else {
    if (!maybe_string_add) left = TruncateToNumber(left, &left_type);
    left_rep = Representation::FromType(left_type);
  }

  if (right_type->Is(Type::None())) {
    Add<HDeoptimize>("Insufficient type feedback for RHS of binary operation",
                     Deoptimizer::SOFT);
    right_type = Type::Any(zone());
  } else {
    if (!maybe_string_add) right = TruncateToNumber(right, &right_type);
    right_rep = Representation::FromType(right_type);
  }

  // Special case for string addition here.
  if (op == Token::ADD &&
      (left_type->Is(Type::String()) || right_type->Is(Type::String()))) {
    // Validate type feedback for left argument.
    if (left_type->Is(Type::String())) {
      left = BuildCheckString(left);
    }

    // Validate type feedback for right argument.
    if (right_type->Is(Type::String())) {
      right = BuildCheckString(right);
    }

    // Convert left argument as necessary.
    if (left_type->Is(Type::Number())) {
      ASSERT(right_type->Is(Type::String()));
      left = BuildNumberToString(left, left_type);
    } else if (!left_type->Is(Type::String())) {
      ASSERT(right_type->Is(Type::String()));
      HValue* function = AddLoadJSBuiltin(Builtins::STRING_ADD_RIGHT);
      Add<HPushArgument>(left);
      Add<HPushArgument>(right);
      return AddUncasted<HInvokeFunction>(function, 2);
    }

    // Convert right argument as necessary.
    if (right_type->Is(Type::Number())) {
      ASSERT(left_type->Is(Type::String()));
      right = BuildNumberToString(right, right_type);
    } else if (!right_type->Is(Type::String())) {
      ASSERT(left_type->Is(Type::String()));
      HValue* function = AddLoadJSBuiltin(Builtins::STRING_ADD_LEFT);
      Add<HPushArgument>(left);
      Add<HPushArgument>(right);
      return AddUncasted<HInvokeFunction>(function, 2);
    }

    // Fast path for empty constant strings.
    if (left->IsConstant() &&
        HConstant::cast(left)->HasStringValue() &&
        HConstant::cast(left)->StringValue()->length() == 0) {
      return right;
    }
    if (right->IsConstant() &&
        HConstant::cast(right)->HasStringValue() &&
        HConstant::cast(right)->StringValue()->length() == 0) {
      return left;
    }

    // Register the dependent code with the allocation site.
    if (!allocation_mode.feedback_site().is_null()) {
      ASSERT(!graph()->info()->IsStub());
      Handle<AllocationSite> site(allocation_mode.feedback_site());
      AllocationSite::AddDependentCompilationInfo(
          site, AllocationSite::TENURING, top_info());
    }

    // Inline the string addition into the stub when creating allocation
    // mementos to gather allocation site feedback, or if we can statically
    // infer that we're going to create a cons string.
    if ((graph()->info()->IsStub() &&
         allocation_mode.CreateAllocationMementos()) ||
        (left->IsConstant() &&
         HConstant::cast(left)->HasStringValue() &&
         HConstant::cast(left)->StringValue()->length() + 1 >=
           ConsString::kMinLength) ||
        (right->IsConstant() &&
         HConstant::cast(right)->HasStringValue() &&
         HConstant::cast(right)->StringValue()->length() + 1 >=
           ConsString::kMinLength)) {
      return BuildStringAdd(left, right, allocation_mode);
    }

    // Fallback to using the string add stub.
    return AddUncasted<HStringAdd>(
        left, right, allocation_mode.GetPretenureMode(),
        STRING_ADD_CHECK_NONE, allocation_mode.feedback_site());
  }

  if (graph()->info()->IsStub()) {
    left = EnforceNumberType(left, left_type);
    right = EnforceNumberType(right, right_type);
  }

  Representation result_rep = Representation::FromType(result_type);

  bool is_non_primitive = (left_rep.IsTagged() && !left_rep.IsSmi()) ||
                          (right_rep.IsTagged() && !right_rep.IsSmi());

  HInstruction* instr = NULL;
  // Only the stub is allowed to call into the runtime, since otherwise we would
  // inline several instructions (including the two pushes) for every tagged
  // operation in optimized code, which is more expensive, than a stub call.
  if (graph()->info()->IsStub() && is_non_primitive) {
    HValue* function = AddLoadJSBuiltin(BinaryOpIC::TokenToJSBuiltin(op));
    Add<HPushArgument>(left);
    Add<HPushArgument>(right);
    instr = AddUncasted<HInvokeFunction>(function, 2);
  } else {
    switch (op) {
      case Token::ADD:
        instr = AddUncasted<HAdd>(left, right);
        break;
      case Token::SUB:
        instr = AddUncasted<HSub>(left, right);
        break;
      case Token::MUL:
        instr = AddUncasted<HMul>(left, right);
        break;
      case Token::MOD: {
        if (fixed_right_arg.has_value) {
          if (right->IsConstant()) {
            HConstant* c_right = HConstant::cast(right);
            if (c_right->HasInteger32Value()) {
              ASSERT_EQ(fixed_right_arg.value, c_right->Integer32Value());
            }
          } else {
            HConstant* fixed_right = Add<HConstant>(
                static_cast<int>(fixed_right_arg.value));
            IfBuilder if_same(this);
            if_same.If<HCompareNumericAndBranch>(right, fixed_right, Token::EQ);
            if_same.Then();
            if_same.ElseDeopt("Unexpected RHS of binary operation");
            right = fixed_right;
          }
        }
        instr = AddUncasted<HMod>(left, right);
        break;
      }
      case Token::DIV:
        instr = AddUncasted<HDiv>(left, right);
        break;
      case Token::BIT_XOR:
      case Token::BIT_AND:
        instr = AddUncasted<HBitwise>(op, left, right);
        break;
      case Token::BIT_OR: {
        HValue* operand, *shift_amount;
        if (left_type->Is(Type::Signed32()) &&
            right_type->Is(Type::Signed32()) &&
            MatchRotateRight(left, right, &operand, &shift_amount)) {
          instr = AddUncasted<HRor>(operand, shift_amount);
        } else {
          instr = AddUncasted<HBitwise>(op, left, right);
        }
        break;
      }
      case Token::SAR:
        instr = AddUncasted<HSar>(left, right);
        break;
      case Token::SHR:
        instr = AddUncasted<HShr>(left, right);
        if (FLAG_opt_safe_uint32_operations && instr->IsShr() &&
            CanBeZero(right)) {
          graph()->RecordUint32Instruction(instr);
        }
        break;
      case Token::SHL:
        instr = AddUncasted<HShl>(left, right);
        break;
      default:
        UNREACHABLE();
    }
  }

  if (instr->IsBinaryOperation()) {
    HBinaryOperation* binop = HBinaryOperation::cast(instr);
    binop->set_observed_input_representation(1, left_rep);
    binop->set_observed_input_representation(2, right_rep);
    binop->initialize_output_representation(result_rep);
    if (graph()->info()->IsStub()) {
      // Stub should not call into stub.
      instr->SetFlag(HValue::kCannotBeTagged);
      // And should truncate on HForceRepresentation already.
      if (left->IsForceRepresentation()) {
        left->CopyFlag(HValue::kTruncatingToSmi, instr);
        left->CopyFlag(HValue::kTruncatingToInt32, instr);
      }
      if (right->IsForceRepresentation()) {
        right->CopyFlag(HValue::kTruncatingToSmi, instr);
        right->CopyFlag(HValue::kTruncatingToInt32, instr);
      }
    }
  }
  return instr;
}


// Check for the form (%_ClassOf(foo) === 'BarClass').
static bool IsClassOfTest(CompareOperation* expr) {
  if (expr->op() != Token::EQ_STRICT) return false;
  CallRuntime* call = expr->left()->AsCallRuntime();
  if (call == NULL) return false;
  Literal* literal = expr->right()->AsLiteral();
  if (literal == NULL) return false;
  if (!literal->value()->IsString()) return false;
  if (!call->name()->IsOneByteEqualTo(STATIC_ASCII_VECTOR("_ClassOf"))) {
    return false;
  }
  ASSERT(call->arguments()->length() == 1);
  return true;
}


void HOptimizedGraphBuilder::VisitBinaryOperation(BinaryOperation* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  switch (expr->op()) {
    case Token::COMMA:
      return VisitComma(expr);
    case Token::OR:
    case Token::AND:
      return VisitLogicalExpression(expr);
    default:
      return VisitArithmeticExpression(expr);
  }
}


void HOptimizedGraphBuilder::VisitComma(BinaryOperation* expr) {
  CHECK_ALIVE(VisitForEffect(expr->left()));
  // Visit the right subexpression in the same AST context as the entire
  // expression.
  Visit(expr->right());
}


void HOptimizedGraphBuilder::VisitLogicalExpression(BinaryOperation* expr) {
  bool is_logical_and = expr->op() == Token::AND;
  if (ast_context()->IsTest()) {
    TestContext* context = TestContext::cast(ast_context());
    // Translate left subexpression.
    HBasicBlock* eval_right = graph()->CreateBasicBlock();
    if (is_logical_and) {
      CHECK_BAILOUT(VisitForControl(expr->left(),
                                    eval_right,
                                    context->if_false()));
    } else {
      CHECK_BAILOUT(VisitForControl(expr->left(),
                                    context->if_true(),
                                    eval_right));
    }

    // Translate right subexpression by visiting it in the same AST
    // context as the entire expression.
    if (eval_right->HasPredecessor()) {
      eval_right->SetJoinId(expr->RightId());
      set_current_block(eval_right);
      Visit(expr->right());
    }

  } else if (ast_context()->IsValue()) {
    CHECK_ALIVE(VisitForValue(expr->left()));
    ASSERT(current_block() != NULL);
    HValue* left_value = Top();

    // Short-circuit left values that always evaluate to the same boolean value.
    if (expr->left()->ToBooleanIsTrue() || expr->left()->ToBooleanIsFalse()) {
      // l (evals true)  && r -> r
      // l (evals true)  || r -> l
      // l (evals false) && r -> l
      // l (evals false) || r -> r
      if (is_logical_and == expr->left()->ToBooleanIsTrue()) {
        Drop(1);
        CHECK_ALIVE(VisitForValue(expr->right()));
      }
      return ast_context()->ReturnValue(Pop());
    }

    // We need an extra block to maintain edge-split form.
    HBasicBlock* empty_block = graph()->CreateBasicBlock();
    HBasicBlock* eval_right = graph()->CreateBasicBlock();
    ToBooleanStub::Types expected(expr->left()->to_boolean_types());
    HBranch* test = is_logical_and
        ? New<HBranch>(left_value, expected, eval_right, empty_block)
        : New<HBranch>(left_value, expected, empty_block, eval_right);
    FinishCurrentBlock(test);

    set_current_block(eval_right);
    Drop(1);  // Value of the left subexpression.
    CHECK_BAILOUT(VisitForValue(expr->right()));

    HBasicBlock* join_block =
      CreateJoin(empty_block, current_block(), expr->id());
    set_current_block(join_block);
    return ast_context()->ReturnValue(Pop());

  } else {
    ASSERT(ast_context()->IsEffect());
    // In an effect context, we don't need the value of the left subexpression,
    // only its control flow and side effects.  We need an extra block to
    // maintain edge-split form.
    HBasicBlock* empty_block = graph()->CreateBasicBlock();
    HBasicBlock* right_block = graph()->CreateBasicBlock();
    if (is_logical_and) {
      CHECK_BAILOUT(VisitForControl(expr->left(), right_block, empty_block));
    } else {
      CHECK_BAILOUT(VisitForControl(expr->left(), empty_block, right_block));
    }

    // TODO(kmillikin): Find a way to fix this.  It's ugly that there are
    // actually two empty blocks (one here and one inserted by
    // TestContext::BuildBranch, and that they both have an HSimulate though the
    // second one is not a merge node, and that we really have no good AST ID to
    // put on that first HSimulate.

    if (empty_block->HasPredecessor()) {
      empty_block->SetJoinId(expr->id());
    } else {
      empty_block = NULL;
    }

    if (right_block->HasPredecessor()) {
      right_block->SetJoinId(expr->RightId());
      set_current_block(right_block);
      CHECK_BAILOUT(VisitForEffect(expr->right()));
      right_block = current_block();
    } else {
      right_block = NULL;
    }

    HBasicBlock* join_block =
      CreateJoin(empty_block, right_block, expr->id());
    set_current_block(join_block);
    // We did not materialize any value in the predecessor environments,
    // so there is no need to handle it here.
  }
}


void HOptimizedGraphBuilder::VisitArithmeticExpression(BinaryOperation* expr) {
  CHECK_ALIVE(VisitForValue(expr->left()));
  CHECK_ALIVE(VisitForValue(expr->right()));
  SetSourcePosition(expr->position());
  HValue* right = Pop();
  HValue* left = Pop();
  HValue* result =
      BuildBinaryOperation(expr, left, right,
          ast_context()->IsEffect() ? NO_PUSH_BEFORE_SIMULATE
                                    : PUSH_BEFORE_SIMULATE);
  if (FLAG_hydrogen_track_positions && result->IsBinaryOperation()) {
    HBinaryOperation::cast(result)->SetOperandPositions(
        zone(),
        ScriptPositionToSourcePosition(expr->left()->position()),
        ScriptPositionToSourcePosition(expr->right()->position()));
  }
  return ast_context()->ReturnValue(result);
}


void HOptimizedGraphBuilder::HandleLiteralCompareTypeof(CompareOperation* expr,
                                                        Expression* sub_expr,
                                                        Handle<String> check) {
  CHECK_ALIVE(VisitForTypeOf(sub_expr));
  SetSourcePosition(expr->position());
  HValue* value = Pop();
  HTypeofIsAndBranch* instr = New<HTypeofIsAndBranch>(value, check);
  return ast_context()->ReturnControl(instr, expr->id());
}


static bool IsLiteralCompareBool(Isolate* isolate,
                                 HValue* left,
                                 Token::Value op,
                                 HValue* right) {
  return op == Token::EQ_STRICT &&
      ((left->IsConstant() &&
          HConstant::cast(left)->handle(isolate)->IsBoolean()) ||
       (right->IsConstant() &&
           HConstant::cast(right)->handle(isolate)->IsBoolean()));
}


void HOptimizedGraphBuilder::VisitCompareOperation(CompareOperation* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());

  if (!FLAG_hydrogen_track_positions) SetSourcePosition(expr->position());

  // Check for a few fast cases. The AST visiting behavior must be in sync
  // with the full codegen: We don't push both left and right values onto
  // the expression stack when one side is a special-case literal.
  Expression* sub_expr = NULL;
  Handle<String> check;
  if (expr->IsLiteralCompareTypeof(&sub_expr, &check)) {
    return HandleLiteralCompareTypeof(expr, sub_expr, check);
  }
  if (expr->IsLiteralCompareUndefined(&sub_expr, isolate())) {
    return HandleLiteralCompareNil(expr, sub_expr, kUndefinedValue);
  }
  if (expr->IsLiteralCompareNull(&sub_expr)) {
    return HandleLiteralCompareNil(expr, sub_expr, kNullValue);
  }

  if (IsClassOfTest(expr)) {
    CallRuntime* call = expr->left()->AsCallRuntime();
    ASSERT(call->arguments()->length() == 1);
    CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
    HValue* value = Pop();
    Literal* literal = expr->right()->AsLiteral();
    Handle<String> rhs = Handle<String>::cast(literal->value());
    HClassOfTestAndBranch* instr = New<HClassOfTestAndBranch>(value, rhs);
    return ast_context()->ReturnControl(instr, expr->id());
  }

  Type* left_type = expr->left()->bounds().lower;
  Type* right_type = expr->right()->bounds().lower;
  Type* combined_type = expr->combined_type();

  CHECK_ALIVE(VisitForValue(expr->left()));
  CHECK_ALIVE(VisitForValue(expr->right()));

  if (FLAG_hydrogen_track_positions) SetSourcePosition(expr->position());

  HValue* right = Pop();
  HValue* left = Pop();
  Token::Value op = expr->op();

  if (IsLiteralCompareBool(isolate(), left, op, right)) {
    HCompareObjectEqAndBranch* result =
        New<HCompareObjectEqAndBranch>(left, right);
    return ast_context()->ReturnControl(result, expr->id());
  }

  if (op == Token::INSTANCEOF) {
    // Check to see if the rhs of the instanceof is a global function not
    // residing in new space. If it is we assume that the function will stay the
    // same.
    Handle<JSFunction> target = Handle<JSFunction>::null();
    VariableProxy* proxy = expr->right()->AsVariableProxy();
    bool global_function = (proxy != NULL) && proxy->var()->IsUnallocated();
    if (global_function &&
        current_info()->has_global_object() &&
        !current_info()->global_object()->IsAccessCheckNeeded()) {
      Handle<String> name = proxy->name();
      Handle<GlobalObject> global(current_info()->global_object());
      LookupResult lookup(isolate());
      global->Lookup(*name, &lookup);
      if (lookup.IsNormal() && lookup.GetValue()->IsJSFunction()) {
        Handle<JSFunction> candidate(JSFunction::cast(lookup.GetValue()));
        // If the function is in new space we assume it's more likely to
        // change and thus prefer the general IC code.
        if (!isolate()->heap()->InNewSpace(*candidate)) {
          target = candidate;
        }
      }
    }

    // If the target is not null we have found a known global function that is
    // assumed to stay the same for this instanceof.
    if (target.is_null()) {
      HInstanceOf* result = New<HInstanceOf>(left, right);
      return ast_context()->ReturnInstruction(result, expr->id());
    } else {
      Add<HCheckValue>(right, target);
      HInstanceOfKnownGlobal* result =
        New<HInstanceOfKnownGlobal>(left, target);
      return ast_context()->ReturnInstruction(result, expr->id());
    }

    // Code below assumes that we don't fall through.
    UNREACHABLE();
  } else if (op == Token::IN) {
    HValue* function = AddLoadJSBuiltin(Builtins::IN);
    Add<HPushArgument>(left);
    Add<HPushArgument>(right);
    // TODO(olivf) InvokeFunction produces a check for the parameter count,
    // even though we are certain to pass the correct number of arguments here.
    HInstruction* result = New<HInvokeFunction>(function, 2);
    return ast_context()->ReturnInstruction(result, expr->id());
  }

  PushBeforeSimulateBehavior push_behavior =
    ast_context()->IsEffect() ? NO_PUSH_BEFORE_SIMULATE
                              : PUSH_BEFORE_SIMULATE;
  HControlInstruction* compare = BuildCompareInstruction(
      op, left, right, left_type, right_type, combined_type,
      ScriptPositionToSourcePosition(expr->left()->position()),
      ScriptPositionToSourcePosition(expr->right()->position()),
      push_behavior, expr->id());
  if (compare == NULL) return;  // Bailed out.
  return ast_context()->ReturnControl(compare, expr->id());
}


HControlInstruction* HOptimizedGraphBuilder::BuildCompareInstruction(
    Token::Value op,
    HValue* left,
    HValue* right,
    Type* left_type,
    Type* right_type,
    Type* combined_type,
    HSourcePosition left_position,
    HSourcePosition right_position,
    PushBeforeSimulateBehavior push_sim_result,
    BailoutId bailout_id) {
  // Cases handled below depend on collected type feedback. They should
  // soft deoptimize when there is no type feedback.
  if (combined_type->Is(Type::None())) {
    Add<HDeoptimize>("Insufficient type feedback for combined type "
                     "of binary operation",
                     Deoptimizer::SOFT);
    combined_type = left_type = right_type = Type::Any(zone());
  }

  Representation left_rep = Representation::FromType(left_type);
  Representation right_rep = Representation::FromType(right_type);
  Representation combined_rep = Representation::FromType(combined_type);

  if (combined_type->Is(Type::Receiver())) {
    if (Token::IsEqualityOp(op)) {
      // Can we get away with map check and not instance type check?
      HValue* operand_to_check =
          left->block()->block_id() < right->block()->block_id() ? left : right;
      if (combined_type->IsClass()) {
        Handle<Map> map = combined_type->AsClass();
        AddCheckMap(operand_to_check, map);
        HCompareObjectEqAndBranch* result =
            New<HCompareObjectEqAndBranch>(left, right);
        if (FLAG_hydrogen_track_positions) {
          result->set_operand_position(zone(), 0, left_position);
          result->set_operand_position(zone(), 1, right_position);
        }
        return result;
      } else {
        BuildCheckHeapObject(operand_to_check);
        Add<HCheckInstanceType>(operand_to_check,
                                HCheckInstanceType::IS_SPEC_OBJECT);
        HCompareObjectEqAndBranch* result =
            New<HCompareObjectEqAndBranch>(left, right);
        return result;
      }
    } else {
      Bailout(kUnsupportedNonPrimitiveCompare);
      return NULL;
    }
  } else if (combined_type->Is(Type::InternalizedString()) &&
             Token::IsEqualityOp(op)) {
    BuildCheckHeapObject(left);
    Add<HCheckInstanceType>(left, HCheckInstanceType::IS_INTERNALIZED_STRING);
    BuildCheckHeapObject(right);
    Add<HCheckInstanceType>(right, HCheckInstanceType::IS_INTERNALIZED_STRING);
    HCompareObjectEqAndBranch* result =
        New<HCompareObjectEqAndBranch>(left, right);
    return result;
  } else if (combined_type->Is(Type::String())) {
    BuildCheckHeapObject(left);
    Add<HCheckInstanceType>(left, HCheckInstanceType::IS_STRING);
    BuildCheckHeapObject(right);
    Add<HCheckInstanceType>(right, HCheckInstanceType::IS_STRING);
    HStringCompareAndBranch* result =
        New<HStringCompareAndBranch>(left, right, op);
    return result;
  } else {
    if (combined_rep.IsTagged() || combined_rep.IsNone()) {
      HCompareGeneric* result = Add<HCompareGeneric>(left, right, op);
      result->set_observed_input_representation(1, left_rep);
      result->set_observed_input_representation(2, right_rep);
      if (result->HasObservableSideEffects()) {
        if (push_sim_result == PUSH_BEFORE_SIMULATE) {
          Push(result);
          AddSimulate(bailout_id, REMOVABLE_SIMULATE);
          Drop(1);
        } else {
          AddSimulate(bailout_id, REMOVABLE_SIMULATE);
        }
      }
      // TODO(jkummerow): Can we make this more efficient?
      HBranch* branch = New<HBranch>(result);
      return branch;
    } else {
      HCompareNumericAndBranch* result =
          New<HCompareNumericAndBranch>(left, right, op);
      result->set_observed_input_representation(left_rep, right_rep);
      if (FLAG_hydrogen_track_positions) {
        result->SetOperandPositions(zone(), left_position, right_position);
      }
      return result;
    }
  }
}


void HOptimizedGraphBuilder::HandleLiteralCompareNil(CompareOperation* expr,
                                                     Expression* sub_expr,
                                                     NilValue nil) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  ASSERT(expr->op() == Token::EQ || expr->op() == Token::EQ_STRICT);
  if (!FLAG_hydrogen_track_positions) SetSourcePosition(expr->position());
  CHECK_ALIVE(VisitForValue(sub_expr));
  HValue* value = Pop();
  if (expr->op() == Token::EQ_STRICT) {
    HConstant* nil_constant = nil == kNullValue
        ? graph()->GetConstantNull()
        : graph()->GetConstantUndefined();
    HCompareObjectEqAndBranch* instr =
        New<HCompareObjectEqAndBranch>(value, nil_constant);
    return ast_context()->ReturnControl(instr, expr->id());
  } else {
    ASSERT_EQ(Token::EQ, expr->op());
    Type* type = expr->combined_type()->Is(Type::None())
        ? Type::Any(zone()) : expr->combined_type();
    HIfContinuation continuation;
    BuildCompareNil(value, type, &continuation);
    return ast_context()->ReturnContinuation(&continuation, expr->id());
  }
}


HInstruction* HOptimizedGraphBuilder::BuildThisFunction() {
  // If we share optimized code between different closures, the
  // this-function is not a constant, except inside an inlined body.
  if (function_state()->outer() != NULL) {
      return New<HConstant>(
          function_state()->compilation_info()->closure());
  } else {
      return New<HThisFunction>();
  }
}


HInstruction* HOptimizedGraphBuilder::BuildFastLiteral(
    Handle<JSObject> boilerplate_object,
    AllocationSiteUsageContext* site_context) {
  NoObservableSideEffectsScope no_effects(this);
  InstanceType instance_type = boilerplate_object->map()->instance_type();
  ASSERT(instance_type == JS_ARRAY_TYPE || instance_type == JS_OBJECT_TYPE);

  HType type = instance_type == JS_ARRAY_TYPE
      ? HType::JSArray() : HType::JSObject();
  HValue* object_size_constant = Add<HConstant>(
      boilerplate_object->map()->instance_size());

  PretenureFlag pretenure_flag = isolate()->heap()->GetPretenureMode();
  if (FLAG_allocation_site_pretenuring) {
    pretenure_flag = site_context->current()->GetPretenureMode();
    Handle<AllocationSite> site(site_context->current());
    AllocationSite::AddDependentCompilationInfo(
        site, AllocationSite::TENURING, top_info());
  }

  HInstruction* object = Add<HAllocate>(object_size_constant, type,
      pretenure_flag, instance_type, site_context->current());

  BuildEmitObjectHeader(boilerplate_object, object);

  Handle<FixedArrayBase> elements(boilerplate_object->elements());
  int elements_size = (elements->length() > 0 &&
      elements->map() != isolate()->heap()->fixed_cow_array_map()) ?
          elements->Size() : 0;

  HInstruction* object_elements = NULL;
  if (elements_size > 0) {
    HValue* object_elements_size = Add<HConstant>(elements_size);
    if (boilerplate_object->HasFastDoubleElements()) {
      // Allocation folding will not be able to fold |object| and
      // |object_elements| together if they are pre-tenured.
      if (pretenure_flag == TENURED) {
        HConstant* empty_fixed_array = Add<HConstant>(
            isolate()->factory()->empty_fixed_array());
        Add<HStoreNamedField>(object, HObjectAccess::ForElementsPointer(),
                              empty_fixed_array);
      }
      object_elements = Add<HAllocate>(object_elements_size, HType::JSObject(),
          pretenure_flag, FIXED_DOUBLE_ARRAY_TYPE, site_context->current());
    } else {
      object_elements = Add<HAllocate>(object_elements_size, HType::JSObject(),
          pretenure_flag, FIXED_ARRAY_TYPE, site_context->current());
    }
  }
  BuildInitElementsInObjectHeader(boilerplate_object, object, object_elements);

  // Copy object elements if non-COW.
  if (object_elements != NULL) {
    BuildEmitElements(boilerplate_object, elements, object_elements,
                      site_context);
  }

  // Copy in-object properties.
  if (boilerplate_object->map()->NumberOfFields() != 0) {
    BuildEmitInObjectProperties(boilerplate_object, object, site_context,
                                pretenure_flag);
  }
  return object;
}


void HOptimizedGraphBuilder::BuildEmitObjectHeader(
    Handle<JSObject> boilerplate_object,
    HInstruction* object) {
  ASSERT(boilerplate_object->properties()->length() == 0);

  Handle<Map> boilerplate_object_map(boilerplate_object->map());
  AddStoreMapConstant(object, boilerplate_object_map);

  Handle<Object> properties_field =
      Handle<Object>(boilerplate_object->properties(), isolate());
  ASSERT(*properties_field == isolate()->heap()->empty_fixed_array());
  HInstruction* properties = Add<HConstant>(properties_field);
  HObjectAccess access = HObjectAccess::ForPropertiesPointer();
  Add<HStoreNamedField>(object, access, properties);

  if (boilerplate_object->IsJSArray()) {
    Handle<JSArray> boilerplate_array =
        Handle<JSArray>::cast(boilerplate_object);
    Handle<Object> length_field =
        Handle<Object>(boilerplate_array->length(), isolate());
    HInstruction* length = Add<HConstant>(length_field);

    ASSERT(boilerplate_array->length()->IsSmi());
    Add<HStoreNamedField>(object, HObjectAccess::ForArrayLength(
        boilerplate_array->GetElementsKind()), length);
  }
}


void HOptimizedGraphBuilder::BuildInitElementsInObjectHeader(
    Handle<JSObject> boilerplate_object,
    HInstruction* object,
    HInstruction* object_elements) {
  ASSERT(boilerplate_object->properties()->length() == 0);
  if (object_elements == NULL) {
    Handle<Object> elements_field =
        Handle<Object>(boilerplate_object->elements(), isolate());
    object_elements = Add<HConstant>(elements_field);
  }
  Add<HStoreNamedField>(object, HObjectAccess::ForElementsPointer(),
      object_elements);
}


void HOptimizedGraphBuilder::BuildEmitInObjectProperties(
    Handle<JSObject> boilerplate_object,
    HInstruction* object,
    AllocationSiteUsageContext* site_context,
    PretenureFlag pretenure_flag) {
  Handle<Map> boilerplate_map(boilerplate_object->map());
  Handle<DescriptorArray> descriptors(boilerplate_map->instance_descriptors());
  int limit = boilerplate_map->NumberOfOwnDescriptors();

  int copied_fields = 0;
  for (int i = 0; i < limit; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.type() != FIELD) continue;
    copied_fields++;
    int index = descriptors->GetFieldIndex(i);
    int property_offset = boilerplate_object->GetInObjectPropertyOffset(index);
    Handle<Name> name(descriptors->GetKey(i));
    Handle<Object> value =
        Handle<Object>(boilerplate_object->InObjectPropertyAt(index),
        isolate());

    // The access for the store depends on the type of the boilerplate.
    HObjectAccess access = boilerplate_object->IsJSArray() ?
        HObjectAccess::ForJSArrayOffset(property_offset) :
        HObjectAccess::ForMapAndOffset(boilerplate_map, property_offset);

    if (value->IsJSObject()) {
      Handle<JSObject> value_object = Handle<JSObject>::cast(value);
      Handle<AllocationSite> current_site = site_context->EnterNewScope();
      HInstruction* result =
          BuildFastLiteral(value_object, site_context);
      site_context->ExitScope(current_site, value_object);
      Add<HStoreNamedField>(object, access, result);
    } else {
      Representation representation = details.representation();
      HInstruction* value_instruction;

      if (representation.IsDouble()) {
        // Allocate a HeapNumber box and store the value into it.
        HValue* heap_number_constant = Add<HConstant>(HeapNumber::kSize);
        // This heap number alloc does not have a corresponding
        // AllocationSite. That is okay because
        // 1) it's a child object of another object with a valid allocation site
        // 2) we can just use the mode of the parent object for pretenuring
        HInstruction* double_box =
            Add<HAllocate>(heap_number_constant, HType::HeapNumber(),
                pretenure_flag, HEAP_NUMBER_TYPE);
        AddStoreMapConstant(double_box,
            isolate()->factory()->heap_number_map());
        Add<HStoreNamedField>(double_box, HObjectAccess::ForHeapNumberValue(),
                              Add<HConstant>(value));
        value_instruction = double_box;
      } else if (representation.IsSmi() && value->IsUninitialized()) {
        value_instruction = graph()->GetConstant0();
        // Ensure that Constant0 is stored as smi.
        access = access.WithRepresentation(representation);
      } else {
        value_instruction = Add<HConstant>(value);
      }

      Add<HStoreNamedField>(object, access, value_instruction);
    }
  }

  int inobject_properties = boilerplate_object->map()->inobject_properties();
  HInstruction* value_instruction =
      Add<HConstant>(isolate()->factory()->one_pointer_filler_map());
  for (int i = copied_fields; i < inobject_properties; i++) {
    ASSERT(boilerplate_object->IsJSObject());
    int property_offset = boilerplate_object->GetInObjectPropertyOffset(i);
    HObjectAccess access =
        HObjectAccess::ForMapAndOffset(boilerplate_map, property_offset);
    Add<HStoreNamedField>(object, access, value_instruction);
  }
}


void HOptimizedGraphBuilder::BuildEmitElements(
    Handle<JSObject> boilerplate_object,
    Handle<FixedArrayBase> elements,
    HValue* object_elements,
    AllocationSiteUsageContext* site_context) {
  ElementsKind kind = boilerplate_object->map()->elements_kind();
  int elements_length = elements->length();
  HValue* object_elements_length = Add<HConstant>(elements_length);
  BuildInitializeElementsHeader(object_elements, kind, object_elements_length);

  // Copy elements backing store content.
  if (elements->IsFixedDoubleArray()) {
    BuildEmitFixedDoubleArray(elements, kind, object_elements);
  } else if (elements->IsFixedArray()) {
    BuildEmitFixedArray(elements, kind, object_elements,
                        site_context);
  } else {
    UNREACHABLE();
  }
}


void HOptimizedGraphBuilder::BuildEmitFixedDoubleArray(
    Handle<FixedArrayBase> elements,
    ElementsKind kind,
    HValue* object_elements) {
  HInstruction* boilerplate_elements = Add<HConstant>(elements);
  int elements_length = elements->length();
  for (int i = 0; i < elements_length; i++) {
    HValue* key_constant = Add<HConstant>(i);
    HInstruction* value_instruction =
        Add<HLoadKeyed>(boilerplate_elements, key_constant,
                        static_cast<HValue*>(NULL), kind,
                        ALLOW_RETURN_HOLE);
    HInstruction* store = Add<HStoreKeyed>(object_elements, key_constant,
                                           value_instruction, kind);
    store->SetFlag(HValue::kAllowUndefinedAsNaN);
  }
}


void HOptimizedGraphBuilder::BuildEmitFixedArray(
    Handle<FixedArrayBase> elements,
    ElementsKind kind,
    HValue* object_elements,
    AllocationSiteUsageContext* site_context) {
  HInstruction* boilerplate_elements = Add<HConstant>(elements);
  int elements_length = elements->length();
  Handle<FixedArray> fast_elements = Handle<FixedArray>::cast(elements);
  for (int i = 0; i < elements_length; i++) {
    Handle<Object> value(fast_elements->get(i), isolate());
    HValue* key_constant = Add<HConstant>(i);
    if (value->IsJSObject()) {
      Handle<JSObject> value_object = Handle<JSObject>::cast(value);
      Handle<AllocationSite> current_site = site_context->EnterNewScope();
      HInstruction* result =
          BuildFastLiteral(value_object, site_context);
      site_context->ExitScope(current_site, value_object);
      Add<HStoreKeyed>(object_elements, key_constant, result, kind);
    } else {
      HInstruction* value_instruction =
          Add<HLoadKeyed>(boilerplate_elements, key_constant,
                          static_cast<HValue*>(NULL), kind,
                          ALLOW_RETURN_HOLE);
      Add<HStoreKeyed>(object_elements, key_constant, value_instruction, kind);
    }
  }
}


void HOptimizedGraphBuilder::VisitThisFunction(ThisFunction* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  HInstruction* instr = BuildThisFunction();
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
  ASSERT(globals_.is_empty());
  AstVisitor::VisitDeclarations(declarations);
  if (!globals_.is_empty()) {
    Handle<FixedArray> array =
       isolate()->factory()->NewFixedArray(globals_.length(), TENURED);
    for (int i = 0; i < globals_.length(); ++i) array->set(i, *globals_.at(i));
    int flags = DeclareGlobalsEvalFlag::encode(current_info()->is_eval()) |
        DeclareGlobalsNativeFlag::encode(current_info()->is_native()) |
        DeclareGlobalsLanguageMode::encode(current_info()->language_mode());
    Add<HDeclareGlobals>(array, flags);
    globals_.Clear();
  }
}


void HOptimizedGraphBuilder::VisitVariableDeclaration(
    VariableDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  VariableMode mode = declaration->mode();
  Variable* variable = proxy->var();
  bool hole_init = mode == CONST || mode == CONST_HARMONY || mode == LET;
  switch (variable->location()) {
    case Variable::UNALLOCATED:
      globals_.Add(variable->name(), zone());
      globals_.Add(variable->binding_needs_init()
                       ? isolate()->factory()->the_hole_value()
                       : isolate()->factory()->undefined_value(), zone());
      return;
    case Variable::PARAMETER:
    case Variable::LOCAL:
      if (hole_init) {
        HValue* value = graph()->GetConstantHole();
        environment()->Bind(variable, value);
      }
      break;
    case Variable::CONTEXT:
      if (hole_init) {
        HValue* value = graph()->GetConstantHole();
        HValue* context = environment()->context();
        HStoreContextSlot* store = Add<HStoreContextSlot>(
            context, variable->index(), HStoreContextSlot::kNoCheck, value);
        if (store->HasObservableSideEffects()) {
          Add<HSimulate>(proxy->id(), REMOVABLE_SIMULATE);
        }
      }
      break;
    case Variable::LOOKUP:
      return Bailout(kUnsupportedLookupSlotInDeclaration);
  }
}


void HOptimizedGraphBuilder::VisitFunctionDeclaration(
    FunctionDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      globals_.Add(variable->name(), zone());
      Handle<SharedFunctionInfo> function = Compiler::BuildFunctionInfo(
          declaration->fun(), current_info()->script());
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals_.Add(function, zone());
      return;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL: {
      CHECK_ALIVE(VisitForValue(declaration->fun()));
      HValue* value = Pop();
      BindIfLive(variable, value);
      break;
    }
    case Variable::CONTEXT: {
      CHECK_ALIVE(VisitForValue(declaration->fun()));
      HValue* value = Pop();
      HValue* context = environment()->context();
      HStoreContextSlot* store = Add<HStoreContextSlot>(
          context, variable->index(), HStoreContextSlot::kNoCheck, value);
      if (store->HasObservableSideEffects()) {
        Add<HSimulate>(proxy->id(), REMOVABLE_SIMULATE);
      }
      break;
    }
    case Variable::LOOKUP:
      return Bailout(kUnsupportedLookupSlotInDeclaration);
  }
}


void HOptimizedGraphBuilder::VisitModuleDeclaration(
    ModuleDeclaration* declaration) {
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitImportDeclaration(
    ImportDeclaration* declaration) {
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitExportDeclaration(
    ExportDeclaration* declaration) {
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitModuleLiteral(ModuleLiteral* module) {
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitModuleVariable(ModuleVariable* module) {
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitModulePath(ModulePath* module) {
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitModuleUrl(ModuleUrl* module) {
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitModuleStatement(ModuleStatement* stmt) {
  UNREACHABLE();
}


// Generators for inline runtime functions.
// Support for types.
void HOptimizedGraphBuilder::GenerateIsSmi(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HIsSmiAndBranch* result = New<HIsSmiAndBranch>(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsSpecObject(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      New<HHasInstanceTypeAndBranch>(value,
                                     FIRST_SPEC_OBJECT_TYPE,
                                     LAST_SPEC_OBJECT_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsFunction(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      New<HHasInstanceTypeAndBranch>(value, JS_FUNCTION_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsMinusZero(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HCompareMinusZeroAndBranch* result = New<HCompareMinusZeroAndBranch>(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateHasCachedArrayIndex(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasCachedArrayIndexAndBranch* result =
      New<HHasCachedArrayIndexAndBranch>(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsArray(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      New<HHasInstanceTypeAndBranch>(value, JS_ARRAY_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsRegExp(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      New<HHasInstanceTypeAndBranch>(value, JS_REGEXP_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsObject(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HIsObjectAndBranch* result = New<HIsObjectAndBranch>(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsNonNegativeSmi(CallRuntime* call) {
  return Bailout(kInlinedRuntimeFunctionIsNonNegativeSmi);
}


void HOptimizedGraphBuilder::GenerateIsUndetectableObject(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HIsUndetectableAndBranch* result = New<HIsUndetectableAndBranch>(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsStringWrapperSafeForDefaultValueOf(
    CallRuntime* call) {
  return Bailout(kInlinedRuntimeFunctionIsStringWrapperSafeForDefaultValueOf);
}


// Support for construct call checks.
void HOptimizedGraphBuilder::GenerateIsConstructCall(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 0);
  if (function_state()->outer() != NULL) {
    // We are generating graph for inlined function.
    HValue* value = function_state()->inlining_kind() == CONSTRUCT_CALL_RETURN
        ? graph()->GetConstantTrue()
        : graph()->GetConstantFalse();
    return ast_context()->ReturnValue(value);
  } else {
    return ast_context()->ReturnControl(New<HIsConstructCallAndBranch>(),
                                        call->id());
  }
}


// Support for arguments.length and arguments[?].
void HOptimizedGraphBuilder::GenerateArgumentsLength(CallRuntime* call) {
  // Our implementation of arguments (based on this stack frame or an
  // adapter below it) does not work for inlined functions.  This runtime
  // function is blacklisted by AstNode::IsInlineable.
  ASSERT(function_state()->outer() == NULL);
  ASSERT(call->arguments()->length() == 0);
  HInstruction* elements = Add<HArgumentsElements>(false);
  HArgumentsLength* result = New<HArgumentsLength>(elements);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateArguments(CallRuntime* call) {
  // Our implementation of arguments (based on this stack frame or an
  // adapter below it) does not work for inlined functions.  This runtime
  // function is blacklisted by AstNode::IsInlineable.
  ASSERT(function_state()->outer() == NULL);
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* index = Pop();
  HInstruction* elements = Add<HArgumentsElements>(false);
  HInstruction* length = Add<HArgumentsLength>(elements);
  HInstruction* checked_index = Add<HBoundsCheck>(index, length);
  HAccessArgumentsAt* result = New<HAccessArgumentsAt>(
      elements, length, checked_index);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Support for accessing the class and value fields of an object.
void HOptimizedGraphBuilder::GenerateClassOf(CallRuntime* call) {
  // The special form detected by IsClassOfTest is detected before we get here
  // and does not cause a bailout.
  return Bailout(kInlinedRuntimeFunctionClassOf);
}


void HOptimizedGraphBuilder::GenerateValueOf(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* object = Pop();

  IfBuilder if_objectisvalue(this);
  HValue* objectisvalue = if_objectisvalue.If<HHasInstanceTypeAndBranch>(
      object, JS_VALUE_TYPE);
  if_objectisvalue.Then();
  {
    // Return the actual value.
    Push(Add<HLoadNamedField>(
            object, objectisvalue,
            HObjectAccess::ForObservableJSObjectOffset(
                JSValue::kValueOffset)));
    Add<HSimulate>(call->id(), FIXED_SIMULATE);
  }
  if_objectisvalue.Else();
  {
    // If the object is not a value return the object.
    Push(object);
    Add<HSimulate>(call->id(), FIXED_SIMULATE);
  }
  if_objectisvalue.End();
  return ast_context()->ReturnValue(Pop());
}


void HOptimizedGraphBuilder::GenerateDateField(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 2);
  ASSERT_NE(NULL, call->arguments()->at(1)->AsLiteral());
  Smi* index = Smi::cast(*(call->arguments()->at(1)->AsLiteral()->value()));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* date = Pop();
  HDateField* result = New<HDateField>(date, index);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateOneByteSeqStringSetChar(
    CallRuntime* call) {
  ASSERT(call->arguments()->length() == 3);
  // We need to follow the evaluation order of full codegen.
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(2)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* string = Pop();
  HValue* value = Pop();
  HValue* index = Pop();
  Add<HSeqStringSetChar>(String::ONE_BYTE_ENCODING, string,
                         index, value);
  Add<HSimulate>(call->id(), FIXED_SIMULATE);
  return ast_context()->ReturnValue(graph()->GetConstantUndefined());
}


void HOptimizedGraphBuilder::GenerateTwoByteSeqStringSetChar(
    CallRuntime* call) {
  ASSERT(call->arguments()->length() == 3);
  // We need to follow the evaluation order of full codegen.
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(2)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* string = Pop();
  HValue* value = Pop();
  HValue* index = Pop();
  Add<HSeqStringSetChar>(String::TWO_BYTE_ENCODING, string,
                         index, value);
  Add<HSimulate>(call->id(), FIXED_SIMULATE);
  return ast_context()->ReturnValue(graph()->GetConstantUndefined());
}


void HOptimizedGraphBuilder::GenerateSetValueOf(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 2);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* value = Pop();
  HValue* object = Pop();

  // Check if object is a JSValue.
  IfBuilder if_objectisvalue(this);
  if_objectisvalue.If<HHasInstanceTypeAndBranch>(object, JS_VALUE_TYPE);
  if_objectisvalue.Then();
  {
    // Create in-object property store to kValueOffset.
    Add<HStoreNamedField>(object,
        HObjectAccess::ForObservableJSObjectOffset(JSValue::kValueOffset),
        value);
    if (!ast_context()->IsEffect()) {
      Push(value);
    }
    Add<HSimulate>(call->id(), FIXED_SIMULATE);
  }
  if_objectisvalue.Else();
  {
    // Nothing to do in this case.
    if (!ast_context()->IsEffect()) {
      Push(value);
    }
    Add<HSimulate>(call->id(), FIXED_SIMULATE);
  }
  if_objectisvalue.End();
  if (!ast_context()->IsEffect()) {
    Drop(1);
  }
  return ast_context()->ReturnValue(value);
}


// Fast support for charCodeAt(n).
void HOptimizedGraphBuilder::GenerateStringCharCodeAt(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 2);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* index = Pop();
  HValue* string = Pop();
  HInstruction* result = BuildStringCharCodeAt(string, index);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for string.charAt(n) and string[n].
void HOptimizedGraphBuilder::GenerateStringCharFromCode(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* char_code = Pop();
  HInstruction* result = NewUncasted<HStringCharFromCode>(char_code);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for string.charAt(n) and string[n].
void HOptimizedGraphBuilder::GenerateStringCharAt(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 2);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* index = Pop();
  HValue* string = Pop();
  HInstruction* char_code = BuildStringCharCodeAt(string, index);
  AddInstruction(char_code);
  HInstruction* result = NewUncasted<HStringCharFromCode>(char_code);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for object equality testing.
void HOptimizedGraphBuilder::GenerateObjectEquals(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 2);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* right = Pop();
  HValue* left = Pop();
  HCompareObjectEqAndBranch* result =
      New<HCompareObjectEqAndBranch>(left, right);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateLog(CallRuntime* call) {
  // %_Log is ignored in optimized code.
  return ast_context()->ReturnValue(graph()->GetConstantUndefined());
}


// Fast support for StringAdd.
void HOptimizedGraphBuilder::GenerateStringAdd(CallRuntime* call) {
  ASSERT_EQ(2, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* right = Pop();
  HValue* left = Pop();
  HInstruction* result = NewUncasted<HStringAdd>(left, right);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for SubString.
void HOptimizedGraphBuilder::GenerateSubString(CallRuntime* call) {
  ASSERT_EQ(3, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HCallStub* result = New<HCallStub>(CodeStub::SubString, 3);
  Drop(3);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for StringCompare.
void HOptimizedGraphBuilder::GenerateStringCompare(CallRuntime* call) {
  ASSERT_EQ(2, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HCallStub* result = New<HCallStub>(CodeStub::StringCompare, 2);
  Drop(2);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Support for direct calls from JavaScript to native RegExp code.
void HOptimizedGraphBuilder::GenerateRegExpExec(CallRuntime* call) {
  ASSERT_EQ(4, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HCallStub* result = New<HCallStub>(CodeStub::RegExpExec, 4);
  Drop(4);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Construct a RegExp exec result with two in-object properties.
void HOptimizedGraphBuilder::GenerateRegExpConstructResult(CallRuntime* call) {
  ASSERT_EQ(3, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(2)));
  HValue* input = Pop();
  HValue* index = Pop();
  HValue* length = Pop();
  HValue* result = BuildRegExpConstructResult(length, index, input);
  return ast_context()->ReturnValue(result);
}


// Support for fast native caches.
void HOptimizedGraphBuilder::GenerateGetFromCache(CallRuntime* call) {
  return Bailout(kInlinedRuntimeFunctionGetFromCache);
}


// Fast support for number to string.
void HOptimizedGraphBuilder::GenerateNumberToString(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* number = Pop();
  HValue* result = BuildNumberToString(number, Type::Any(zone()));
  return ast_context()->ReturnValue(result);
}


// Fast call for custom callbacks.
void HOptimizedGraphBuilder::GenerateCallFunction(CallRuntime* call) {
  // 1 ~ The function to call is not itself an argument to the call.
  int arg_count = call->arguments()->length() - 1;
  ASSERT(arg_count >= 1);  // There's always at least a receiver.

  for (int i = 0; i < arg_count; ++i) {
    CHECK_ALIVE(VisitArgument(call->arguments()->at(i)));
  }
  CHECK_ALIVE(VisitForValue(call->arguments()->last()));

  HValue* function = Pop();

  IfBuilder if_is_jsfunction(this);
  if_is_jsfunction.If<HHasInstanceTypeAndBranch>(function, JS_FUNCTION_TYPE);

  if_is_jsfunction.Then();
  {
    HInstruction* invoke_result =
        Add<HInvokeFunction>(function, arg_count);
    Drop(arg_count);
    if (!ast_context()->IsEffect()) {
      Push(invoke_result);
    }
    Add<HSimulate>(call->id(), FIXED_SIMULATE);
  }

  if_is_jsfunction.Else();
  {
    HInstruction* call_result =
        Add<HCallFunction>(function, arg_count);
    Drop(arg_count);
    if (!ast_context()->IsEffect()) {
      Push(call_result);
    }
    Add<HSimulate>(call->id(), FIXED_SIMULATE);
  }
  if_is_jsfunction.End();

  if (ast_context()->IsEffect()) {
    // EffectContext::ReturnValue ignores the value, so we can just pass
    // 'undefined' (as we do not have the call result anymore).
    return ast_context()->ReturnValue(graph()->GetConstantUndefined());
  } else {
    return ast_context()->ReturnValue(Pop());
  }
}


// Fast call to math functions.
void HOptimizedGraphBuilder::GenerateMathPow(CallRuntime* call) {
  ASSERT_EQ(2, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* right = Pop();
  HValue* left = Pop();
  HInstruction* result = NewUncasted<HPower>(left, right);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathLog(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HInstruction* result = NewUncasted<HUnaryMathOperation>(value, kMathLog);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathSqrt(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HInstruction* result = NewUncasted<HUnaryMathOperation>(value, kMathSqrt);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateGetCachedArrayIndex(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HGetCachedArrayIndex* result = New<HGetCachedArrayIndex>(value);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateFastAsciiArrayJoin(CallRuntime* call) {
  return Bailout(kInlinedRuntimeFunctionFastAsciiArrayJoin);
}


// Support for generators.
void HOptimizedGraphBuilder::GenerateGeneratorNext(CallRuntime* call) {
  return Bailout(kInlinedRuntimeFunctionGeneratorNext);
}


void HOptimizedGraphBuilder::GenerateGeneratorThrow(CallRuntime* call) {
  return Bailout(kInlinedRuntimeFunctionGeneratorThrow);
}


void HOptimizedGraphBuilder::GenerateDebugBreakInOptimizedCode(
    CallRuntime* call) {
  Add<HDebugBreak>();
  return ast_context()->ReturnValue(graph()->GetConstant0());
}


#undef CHECK_BAILOUT
#undef CHECK_ALIVE


HEnvironment::HEnvironment(HEnvironment* outer,
                           Scope* scope,
                           Handle<JSFunction> closure,
                           Zone* zone)
    : closure_(closure),
      values_(0, zone),
      frame_type_(JS_FUNCTION),
      parameter_count_(0),
      specials_count_(1),
      local_count_(0),
      outer_(outer),
      entry_(NULL),
      pop_count_(0),
      push_count_(0),
      ast_id_(BailoutId::None()),
      zone_(zone) {
  Initialize(scope->num_parameters() + 1, scope->num_stack_slots(), 0);
}


HEnvironment::HEnvironment(Zone* zone, int parameter_count)
    : values_(0, zone),
      frame_type_(STUB),
      parameter_count_(parameter_count),
      specials_count_(1),
      local_count_(0),
      outer_(NULL),
      entry_(NULL),
      pop_count_(0),
      push_count_(0),
      ast_id_(BailoutId::None()),
      zone_(zone) {
  Initialize(parameter_count, 0, 0);
}


HEnvironment::HEnvironment(const HEnvironment* other, Zone* zone)
    : values_(0, zone),
      frame_type_(JS_FUNCTION),
      parameter_count_(0),
      specials_count_(0),
      local_count_(0),
      outer_(NULL),
      entry_(NULL),
      pop_count_(0),
      push_count_(0),
      ast_id_(other->ast_id()),
      zone_(zone) {
  Initialize(other);
}


HEnvironment::HEnvironment(HEnvironment* outer,
                           Handle<JSFunction> closure,
                           FrameType frame_type,
                           int arguments,
                           Zone* zone)
    : closure_(closure),
      values_(arguments, zone),
      frame_type_(frame_type),
      parameter_count_(arguments),
      specials_count_(0),
      local_count_(0),
      outer_(outer),
      entry_(NULL),
      pop_count_(0),
      push_count_(0),
      ast_id_(BailoutId::None()),
      zone_(zone) {
}


void HEnvironment::Initialize(int parameter_count,
                              int local_count,
                              int stack_height) {
  parameter_count_ = parameter_count;
  local_count_ = local_count;

  // Avoid reallocating the temporaries' backing store on the first Push.
  int total = parameter_count + specials_count_ + local_count + stack_height;
  values_.Initialize(total + 4, zone());
  for (int i = 0; i < total; ++i) values_.Add(NULL, zone());
}


void HEnvironment::Initialize(const HEnvironment* other) {
  closure_ = other->closure();
  values_.AddAll(other->values_, zone());
  assigned_variables_.Union(other->assigned_variables_, zone());
  frame_type_ = other->frame_type_;
  parameter_count_ = other->parameter_count_;
  local_count_ = other->local_count_;
  if (other->outer_ != NULL) outer_ = other->outer_->Copy();  // Deep copy.
  entry_ = other->entry_;
  pop_count_ = other->pop_count_;
  push_count_ = other->push_count_;
  specials_count_ = other->specials_count_;
  ast_id_ = other->ast_id_;
}


void HEnvironment::AddIncomingEdge(HBasicBlock* block, HEnvironment* other) {
  ASSERT(!block->IsLoopHeader());
  ASSERT(values_.length() == other->values_.length());

  int length = values_.length();
  for (int i = 0; i < length; ++i) {
    HValue* value = values_[i];
    if (value != NULL && value->IsPhi() && value->block() == block) {
      // There is already a phi for the i'th value.
      HPhi* phi = HPhi::cast(value);
      // Assert index is correct and that we haven't missed an incoming edge.
      ASSERT(phi->merged_index() == i || !phi->HasMergedIndex());
      ASSERT(phi->OperandCount() == block->predecessors()->length());
      phi->AddInput(other->values_[i]);
    } else if (values_[i] != other->values_[i]) {
      // There is a fresh value on the incoming edge, a phi is needed.
      ASSERT(values_[i] != NULL && other->values_[i] != NULL);
      HPhi* phi = block->AddNewPhi(i);
      HValue* old_value = values_[i];
      for (int j = 0; j < block->predecessors()->length(); j++) {
        phi->AddInput(old_value);
      }
      phi->AddInput(other->values_[i]);
      this->values_[i] = phi;
    }
  }
}


void HEnvironment::Bind(int index, HValue* value) {
  ASSERT(value != NULL);
  assigned_variables_.Add(index, zone());
  values_[index] = value;
}


bool HEnvironment::HasExpressionAt(int index) const {
  return index >= parameter_count_ + specials_count_ + local_count_;
}


bool HEnvironment::ExpressionStackIsEmpty() const {
  ASSERT(length() >= first_expression_index());
  return length() == first_expression_index();
}


void HEnvironment::SetExpressionStackAt(int index_from_top, HValue* value) {
  int count = index_from_top + 1;
  int index = values_.length() - count;
  ASSERT(HasExpressionAt(index));
  // The push count must include at least the element in question or else
  // the new value will not be included in this environment's history.
  if (push_count_ < count) {
    // This is the same effect as popping then re-pushing 'count' elements.
    pop_count_ += (count - push_count_);
    push_count_ = count;
  }
  values_[index] = value;
}


void HEnvironment::Drop(int count) {
  for (int i = 0; i < count; ++i) {
    Pop();
  }
}


HEnvironment* HEnvironment::Copy() const {
  return new(zone()) HEnvironment(this, zone());
}


HEnvironment* HEnvironment::CopyWithoutHistory() const {
  HEnvironment* result = Copy();
  result->ClearHistory();
  return result;
}


HEnvironment* HEnvironment::CopyAsLoopHeader(HBasicBlock* loop_header) const {
  HEnvironment* new_env = Copy();
  for (int i = 0; i < values_.length(); ++i) {
    HPhi* phi = loop_header->AddNewPhi(i);
    phi->AddInput(values_[i]);
    new_env->values_[i] = phi;
  }
  new_env->ClearHistory();
  return new_env;
}


HEnvironment* HEnvironment::CreateStubEnvironment(HEnvironment* outer,
                                                  Handle<JSFunction> target,
                                                  FrameType frame_type,
                                                  int arguments) const {
  HEnvironment* new_env =
      new(zone()) HEnvironment(outer, target, frame_type,
                               arguments + 1, zone());
  for (int i = 0; i <= arguments; ++i) {  // Include receiver.
    new_env->Push(ExpressionStackAt(arguments - i));
  }
  new_env->ClearHistory();
  return new_env;
}


HEnvironment* HEnvironment::CopyForInlining(
    Handle<JSFunction> target,
    int arguments,
    FunctionLiteral* function,
    HConstant* undefined,
    InliningKind inlining_kind) const {
  ASSERT(frame_type() == JS_FUNCTION);

  // Outer environment is a copy of this one without the arguments.
  int arity = function->scope()->num_parameters();

  HEnvironment* outer = Copy();
  outer->Drop(arguments + 1);  // Including receiver.
  outer->ClearHistory();

  if (inlining_kind == CONSTRUCT_CALL_RETURN) {
    // Create artificial constructor stub environment.  The receiver should
    // actually be the constructor function, but we pass the newly allocated
    // object instead, DoComputeConstructStubFrame() relies on that.
    outer = CreateStubEnvironment(outer, target, JS_CONSTRUCT, arguments);
  } else if (inlining_kind == GETTER_CALL_RETURN) {
    // We need an additional StackFrame::INTERNAL frame for restoring the
    // correct context.
    outer = CreateStubEnvironment(outer, target, JS_GETTER, arguments);
  } else if (inlining_kind == SETTER_CALL_RETURN) {
    // We need an additional StackFrame::INTERNAL frame for temporarily saving
    // the argument of the setter, see StoreStubCompiler::CompileStoreViaSetter.
    outer = CreateStubEnvironment(outer, target, JS_SETTER, arguments);
  }

  if (arity != arguments) {
    // Create artificial arguments adaptation environment.
    outer = CreateStubEnvironment(outer, target, ARGUMENTS_ADAPTOR, arguments);
  }

  HEnvironment* inner =
      new(zone()) HEnvironment(outer, function->scope(), target, zone());
  // Get the argument values from the original environment.
  for (int i = 0; i <= arity; ++i) {  // Include receiver.
    HValue* push = (i <= arguments) ?
        ExpressionStackAt(arguments - i) : undefined;
    inner->SetValueAt(i, push);
  }
  inner->SetValueAt(arity + 1, context());
  for (int i = arity + 2; i < inner->length(); ++i) {
    inner->SetValueAt(i, undefined);
  }

  inner->set_ast_id(BailoutId::FunctionEntry());
  return inner;
}


void HEnvironment::PrintTo(StringStream* stream) {
  for (int i = 0; i < length(); i++) {
    if (i == 0) stream->Add("parameters\n");
    if (i == parameter_count()) stream->Add("specials\n");
    if (i == parameter_count() + specials_count()) stream->Add("locals\n");
    if (i == parameter_count() + specials_count() + local_count()) {
      stream->Add("expressions\n");
    }
    HValue* val = values_.at(i);
    stream->Add("%d: ", i);
    if (val != NULL) {
      val->PrintNameTo(stream);
    } else {
      stream->Add("NULL");
    }
    stream->Add("\n");
  }
  PrintF("\n");
}


void HEnvironment::PrintToStd() {
  HeapStringAllocator string_allocator;
  StringStream trace(&string_allocator);
  PrintTo(&trace);
  PrintF("%s", trace.ToCString().get());
}


void HTracer::TraceCompilation(CompilationInfo* info) {
  Tag tag(this, "compilation");
  if (info->IsOptimizing()) {
    Handle<String> name = info->function()->debug_name();
    PrintStringProperty("name", name->ToCString().get());
    PrintIndent();
    trace_.Add("method \"%s:%d\"\n",
               name->ToCString().get(),
               info->optimization_id());
  } else {
    CodeStub::Major major_key = info->code_stub()->MajorKey();
    PrintStringProperty("name", CodeStub::MajorName(major_key, false));
    PrintStringProperty("method", "stub");
  }
  PrintLongProperty("date", static_cast<int64_t>(OS::TimeCurrentMillis()));
}


void HTracer::TraceLithium(const char* name, LChunk* chunk) {
  ASSERT(!chunk->isolate()->concurrent_recompilation_enabled());
  AllowHandleDereference allow_deref;
  AllowDeferredHandleDereference allow_deferred_deref;
  Trace(name, chunk->graph(), chunk);
}


void HTracer::TraceHydrogen(const char* name, HGraph* graph) {
  ASSERT(!graph->isolate()->concurrent_recompilation_enabled());
  AllowHandleDereference allow_deref;
  AllowDeferredHandleDereference allow_deferred_deref;
  Trace(name, graph, NULL);
}


void HTracer::Trace(const char* name, HGraph* graph, LChunk* chunk) {
  Tag tag(this, "cfg");
  PrintStringProperty("name", name);
  const ZoneList<HBasicBlock*>* blocks = graph->blocks();
  for (int i = 0; i < blocks->length(); i++) {
    HBasicBlock* current = blocks->at(i);
    Tag block_tag(this, "block");
    PrintBlockProperty("name", current->block_id());
    PrintIntProperty("from_bci", -1);
    PrintIntProperty("to_bci", -1);

    if (!current->predecessors()->is_empty()) {
      PrintIndent();
      trace_.Add("predecessors");
      for (int j = 0; j < current->predecessors()->length(); ++j) {
        trace_.Add(" \"B%d\"", current->predecessors()->at(j)->block_id());
      }
      trace_.Add("\n");
    } else {
      PrintEmptyProperty("predecessors");
    }

    if (current->end()->SuccessorCount() == 0) {
      PrintEmptyProperty("successors");
    } else  {
      PrintIndent();
      trace_.Add("successors");
      for (HSuccessorIterator it(current->end()); !it.Done(); it.Advance()) {
        trace_.Add(" \"B%d\"", it.Current()->block_id());
      }
      trace_.Add("\n");
    }

    PrintEmptyProperty("xhandlers");

    {
      PrintIndent();
      trace_.Add("flags");
      if (current->IsLoopSuccessorDominator()) {
        trace_.Add(" \"dom-loop-succ\"");
      }
      if (current->IsUnreachable()) {
        trace_.Add(" \"dead\"");
      }
      if (current->is_osr_entry()) {
        trace_.Add(" \"osr\"");
      }
      trace_.Add("\n");
    }

    if (current->dominator() != NULL) {
      PrintBlockProperty("dominator", current->dominator()->block_id());
    }

    PrintIntProperty("loop_depth", current->LoopNestingDepth());

    if (chunk != NULL) {
      int first_index = current->first_instruction_index();
      int last_index = current->last_instruction_index();
      PrintIntProperty(
          "first_lir_id",
          LifetimePosition::FromInstructionIndex(first_index).Value());
      PrintIntProperty(
          "last_lir_id",
          LifetimePosition::FromInstructionIndex(last_index).Value());
    }

    {
      Tag states_tag(this, "states");
      Tag locals_tag(this, "locals");
      int total = current->phis()->length();
      PrintIntProperty("size", current->phis()->length());
      PrintStringProperty("method", "None");
      for (int j = 0; j < total; ++j) {
        HPhi* phi = current->phis()->at(j);
        PrintIndent();
        trace_.Add("%d ", phi->merged_index());
        phi->PrintNameTo(&trace_);
        trace_.Add(" ");
        phi->PrintTo(&trace_);
        trace_.Add("\n");
      }
    }

    {
      Tag HIR_tag(this, "HIR");
      for (HInstructionIterator it(current); !it.Done(); it.Advance()) {
        HInstruction* instruction = it.Current();
        int uses = instruction->UseCount();
        PrintIndent();
        trace_.Add("0 %d ", uses);
        instruction->PrintNameTo(&trace_);
        trace_.Add(" ");
        instruction->PrintTo(&trace_);
        if (FLAG_hydrogen_track_positions &&
            instruction->has_position() &&
            instruction->position().raw() != 0) {
          const HSourcePosition pos = instruction->position();
          trace_.Add(" pos:");
          if (pos.inlining_id() != 0) {
            trace_.Add("%d_", pos.inlining_id());
          }
          trace_.Add("%d", pos.position());
        }
        trace_.Add(" <|@\n");
      }
    }


    if (chunk != NULL) {
      Tag LIR_tag(this, "LIR");
      int first_index = current->first_instruction_index();
      int last_index = current->last_instruction_index();
      if (first_index != -1 && last_index != -1) {
        const ZoneList<LInstruction*>* instructions = chunk->instructions();
        for (int i = first_index; i <= last_index; ++i) {
          LInstruction* linstr = instructions->at(i);
          if (linstr != NULL) {
            PrintIndent();
            trace_.Add("%d ",
                       LifetimePosition::FromInstructionIndex(i).Value());
            linstr->PrintTo(&trace_);
            trace_.Add(" [hir:");
            linstr->hydrogen_value()->PrintNameTo(&trace_);
            trace_.Add("]");
            trace_.Add(" <|@\n");
          }
        }
      }
    }
  }
}


void HTracer::TraceLiveRanges(const char* name, LAllocator* allocator) {
  Tag tag(this, "intervals");
  PrintStringProperty("name", name);

  const Vector<LiveRange*>* fixed_d = allocator->fixed_double_live_ranges();
  for (int i = 0; i < fixed_d->length(); ++i) {
    TraceLiveRange(fixed_d->at(i), "fixed", allocator->zone());
  }

  const Vector<LiveRange*>* fixed = allocator->fixed_live_ranges();
  for (int i = 0; i < fixed->length(); ++i) {
    TraceLiveRange(fixed->at(i), "fixed", allocator->zone());
  }

  const ZoneList<LiveRange*>* live_ranges = allocator->live_ranges();
  for (int i = 0; i < live_ranges->length(); ++i) {
    TraceLiveRange(live_ranges->at(i), "object", allocator->zone());
  }
}


void HTracer::TraceLiveRange(LiveRange* range, const char* type,
                             Zone* zone) {
  if (range != NULL && !range->IsEmpty()) {
    PrintIndent();
    trace_.Add("%d %s", range->id(), type);
    if (range->HasRegisterAssigned()) {
      LOperand* op = range->CreateAssignedOperand(zone);
      int assigned_reg = op->index();
      if (op->IsDoubleRegister()) {
        trace_.Add(" \"%s\"",
                   DoubleRegister::AllocationIndexToString(assigned_reg));
      } else {
        ASSERT(op->IsRegister());
        trace_.Add(" \"%s\"", Register::AllocationIndexToString(assigned_reg));
      }
    } else if (range->IsSpilled()) {
      LOperand* op = range->TopLevel()->GetSpillOperand();
      if (op->IsDoubleStackSlot()) {
        trace_.Add(" \"double_stack:%d\"", op->index());
      } else {
        ASSERT(op->IsStackSlot());
        trace_.Add(" \"stack:%d\"", op->index());
      }
    }
    int parent_index = -1;
    if (range->IsChild()) {
      parent_index = range->parent()->id();
    } else {
      parent_index = range->id();
    }
    LOperand* op = range->FirstHint();
    int hint_index = -1;
    if (op != NULL && op->IsUnallocated()) {
      hint_index = LUnallocated::cast(op)->virtual_register();
    }
    trace_.Add(" %d %d", parent_index, hint_index);
    UseInterval* cur_interval = range->first_interval();
    while (cur_interval != NULL && range->Covers(cur_interval->start())) {
      trace_.Add(" [%d, %d[",
                 cur_interval->start().Value(),
                 cur_interval->end().Value());
      cur_interval = cur_interval->next();
    }

    UsePosition* current_pos = range->first_pos();
    while (current_pos != NULL) {
      if (current_pos->RegisterIsBeneficial() || FLAG_trace_all_uses) {
        trace_.Add(" %d M", current_pos->pos().Value());
      }
      current_pos = current_pos->next();
    }

    trace_.Add(" \"\"\n");
  }
}


void HTracer::FlushToFile() {
  AppendChars(filename_.start(), trace_.ToCString().get(), trace_.length(),
              false);
  trace_.Reset();
}


void HStatistics::Initialize(CompilationInfo* info) {
  if (info->shared_info().is_null()) return;
  source_size_ += info->shared_info()->SourceSize();
}


void HStatistics::Print() {
  PrintF("Timing results:\n");
  TimeDelta sum;
  for (int i = 0; i < times_.length(); ++i) {
    sum += times_[i];
  }

  for (int i = 0; i < names_.length(); ++i) {
    PrintF("%32s", names_[i]);
    double ms = times_[i].InMillisecondsF();
    double percent = times_[i].PercentOf(sum);
    PrintF(" %8.3f ms / %4.1f %% ", ms, percent);

    unsigned size = sizes_[i];
    double size_percent = static_cast<double>(size) * 100 / total_size_;
    PrintF(" %9u bytes / %4.1f %%\n", size, size_percent);
  }

  PrintF("----------------------------------------"
         "---------------------------------------\n");
  TimeDelta total = create_graph_ + optimize_graph_ + generate_code_;
  PrintF("%32s %8.3f ms / %4.1f %% \n",
         "Create graph",
         create_graph_.InMillisecondsF(),
         create_graph_.PercentOf(total));
  PrintF("%32s %8.3f ms / %4.1f %% \n",
         "Optimize graph",
         optimize_graph_.InMillisecondsF(),
         optimize_graph_.PercentOf(total));
  PrintF("%32s %8.3f ms / %4.1f %% \n",
         "Generate and install code",
         generate_code_.InMillisecondsF(),
         generate_code_.PercentOf(total));
  PrintF("----------------------------------------"
         "---------------------------------------\n");
  PrintF("%32s %8.3f ms (%.1f times slower than full code gen)\n",
         "Total",
         total.InMillisecondsF(),
         total.TimesOf(full_code_gen_));

  double source_size_in_kb = static_cast<double>(source_size_) / 1024;
  double normalized_time =  source_size_in_kb > 0
      ? total.InMillisecondsF() / source_size_in_kb
      : 0;
  double normalized_size_in_kb = source_size_in_kb > 0
      ? total_size_ / 1024 / source_size_in_kb
      : 0;
  PrintF("%32s %8.3f ms           %7.3f kB allocated\n",
         "Average per kB source",
         normalized_time, normalized_size_in_kb);
}


void HStatistics::SaveTiming(const char* name, TimeDelta time, unsigned size) {
  total_size_ += size;
  for (int i = 0; i < names_.length(); ++i) {
    if (strcmp(names_[i], name) == 0) {
      times_[i] += time;
      sizes_[i] += size;
      return;
    }
  }
  names_.Add(name);
  times_.Add(time);
  sizes_.Add(size);
}


HPhase::~HPhase() {
  if (ShouldProduceTraceOutput()) {
    isolate()->GetHTracer()->TraceHydrogen(name(), graph_);
  }

#ifdef DEBUG
  graph_->Verify(false);  // No full verify.
#endif
}

} }  // namespace v8::internal
