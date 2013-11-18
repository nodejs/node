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
#include "scopeinfo.h"
#include "scopes.h"
#include "stub-cache.h"
#include "typing.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/lithium-codegen-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/lithium-codegen-x64.h"
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


void HBasicBlock::AddInstruction(HInstruction* instr, int position) {
  ASSERT(!IsStartBlock() || !IsFinished());
  ASSERT(!instr->IsLinked());
  ASSERT(!IsFinished());

  if (position != RelocInfo::kNoPosition) {
    instr->set_position(position);
  }
  if (first_ == NULL) {
    ASSERT(last_environment() != NULL);
    ASSERT(!last_environment()->ast_id().IsNone());
    HBlockEntry* entry = new(zone()) HBlockEntry();
    entry->InitializeAsFirst(this);
    if (position != RelocInfo::kNoPosition) {
      entry->set_position(position);
    } else {
      ASSERT(!FLAG_emit_opt_code_positions ||
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


void HBasicBlock::Finish(HControlInstruction* end, int position) {
  ASSERT(!IsFinished());
  AddInstruction(end, position);
  end_ = end;
  for (HSuccessorIterator it(end); !it.Done(); it.Advance()) {
    it.Current()->RegisterPredecessor(this);
  }
}


void HBasicBlock::Goto(HBasicBlock* block,
                       int position,
                       FunctionState* state,
                       bool add_simulate) {
  bool drop_extra = state != NULL &&
      state->inlining_kind() == DROP_EXTRA_ON_RETURN;

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
                                  int position) {
  HBasicBlock* target = state->function_return();
  bool drop_extra = state->inlining_kind() == DROP_EXTRA_ON_RETURN;

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
        ASSERT(predecessor->end()->IsGoto());
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
      deopt_then_(false),
      deopt_else_(false),
      did_then_(false),
      did_else_(false),
      did_and_(false),
      did_or_(false),
      captured_(false),
      needs_compare_(true),
      split_edge_merge_block_(NULL),
      merge_block_(NULL) {
  HEnvironment* env = builder->environment();
  first_true_block_ = builder->CreateBasicBlock(env->Copy());
  last_true_block_ = NULL;
  first_false_block_ = builder->CreateBasicBlock(env->Copy());
}


HGraphBuilder::IfBuilder::IfBuilder(
    HGraphBuilder* builder,
    HIfContinuation* continuation)
    : builder_(builder),
      finished_(false),
      deopt_then_(false),
      deopt_else_(false),
      did_then_(false),
      did_else_(false),
      did_and_(false),
      did_or_(false),
      captured_(false),
      needs_compare_(false),
      first_true_block_(NULL),
      last_true_block_(NULL),
      first_false_block_(NULL),
      split_edge_merge_block_(NULL),
      merge_block_(NULL) {
  continuation->Continue(&first_true_block_,
                         &first_false_block_);
}


HControlInstruction* HGraphBuilder::IfBuilder::AddCompare(
    HControlInstruction* compare) {
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
  ASSERT(!finished_);
  ASSERT(!captured_);
  HBasicBlock* true_block = last_true_block_ == NULL
      ? first_true_block_
      : last_true_block_;
  HBasicBlock* false_block = did_else_ && (first_false_block_ != NULL)
      ? builder_->current_block()
      : first_false_block_;
  continuation->Capture(true_block, false_block);
  captured_ = true;
  End();
}


void HGraphBuilder::IfBuilder::JoinContinuation(HIfContinuation* continuation) {
  ASSERT(!finished_);
  ASSERT(!captured_);
  HBasicBlock* true_block = last_true_block_ == NULL
      ? first_true_block_
      : last_true_block_;
  HBasicBlock* false_block = did_else_ && (first_false_block_ != NULL)
      ? builder_->current_block()
      : first_false_block_;
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
}


void HGraphBuilder::IfBuilder::Else() {
  ASSERT(did_then_);
  ASSERT(!captured_);
  ASSERT(!finished_);
  last_true_block_ = builder_->current_block();
  builder_->set_current_block(first_false_block_);
  did_else_ = true;
}


void HGraphBuilder::IfBuilder::Deopt(const char* reason) {
  ASSERT(did_then_);
  if (did_else_) {
    deopt_else_ = true;
  } else {
    deopt_then_ = true;
  }
  builder_->Add<HDeoptimize>(reason, Deoptimizer::EAGER);
}


void HGraphBuilder::IfBuilder::Return(HValue* value) {
  HValue* parameter_count = builder_->graph()->GetConstantMinus1();
  builder_->FinishExitCurrentBlock(
      builder_->New<HReturn>(value, parameter_count));
  if (did_else_) {
    first_false_block_ = NULL;
  } else {
    first_true_block_ = NULL;
  }
}


void HGraphBuilder::IfBuilder::End() {
  if (!captured_) {
    ASSERT(did_then_);
    if (!did_else_) {
      last_true_block_ = builder_->current_block();
    }
    if (last_true_block_ == NULL || last_true_block_->IsFinished()) {
      ASSERT(did_else_);
      // Return on true. Nothing to do, just continue the false block.
    } else if (first_false_block_ == NULL ||
               (did_else_ && builder_->current_block()->IsFinished())) {
      // Deopt on false. Nothing to do except switching to the true block.
      builder_->set_current_block(last_true_block_);
    } else {
      merge_block_ = builder_->graph()->CreateBasicBlock();
      ASSERT(!finished_);
      if (!did_else_) Else();
      ASSERT(!last_true_block_->IsFinished());
      HBasicBlock* last_false_block = builder_->current_block();
      ASSERT(!last_false_block->IsFinished());
      if (deopt_then_) {
        builder_->GotoNoSimulate(last_false_block, merge_block_);
        builder_->PadEnvironmentForContinuation(last_true_block_,
                                                merge_block_);
        builder_->GotoNoSimulate(last_true_block_, merge_block_);
      } else {
        builder_->GotoNoSimulate(last_true_block_, merge_block_);
        if (deopt_else_) {
          builder_->PadEnvironmentForContinuation(last_false_block,
                                                  merge_block_);
        }
        builder_->GotoNoSimulate(last_false_block, merge_block_);
      }
      builder_->set_current_block(merge_block_);
    }
  }
  finished_ = true;
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
  ASSERT(!FLAG_emit_opt_code_positions ||
         position_ != RelocInfo::kNoPosition || !info_->IsOptimizing());
  current_block()->AddInstruction(instr, position_);
  if (graph()->IsInsideNoSideEffectsScope()) {
    instr->SetFlag(HValue::kHasNoObservableSideEffects);
  }
  return instr;
}


void HGraphBuilder::FinishCurrentBlock(HControlInstruction* last) {
  ASSERT(!FLAG_emit_opt_code_positions || !info_->IsOptimizing() ||
         position_ != RelocInfo::kNoPosition);
  current_block()->Finish(last, position_);
  if (last->IsReturn() || last->IsAbnormalExit()) {
    set_current_block(NULL);
  }
}


void HGraphBuilder::FinishExitCurrentBlock(HControlInstruction* instruction) {
  ASSERT(!FLAG_emit_opt_code_positions || !info_->IsOptimizing() ||
         position_ != RelocInfo::kNoPosition);
  current_block()->FinishExit(instruction, position_);
  if (instruction->IsReturn() || instruction->IsAbnormalExit()) {
    set_current_block(NULL);
  }
}


void HGraphBuilder::AddIncrementCounter(StatsCounter* counter) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    HValue* reference = Add<HConstant>(ExternalReference(counter));
    HValue* old_value = Add<HLoadNamedField>(reference,
                                             HObjectAccess::ForCounter());
    HValue* new_value = Add<HAdd>(old_value, graph()->GetConstant1());
    new_value->ClearFlag(HValue::kCanOverflow);  // Ignore counter overflow
    Add<HStoreNamedField>(reference, HObjectAccess::ForCounter(),
                          new_value);
  }
}


void HGraphBuilder::AddSimulate(BailoutId id,
                                RemovableSimulate removable) {
  ASSERT(current_block() != NULL);
  ASSERT(!graph()->IsInsideNoSideEffectsScope());
  current_block()->AddNewSimulate(id, removable);
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


void HGraphBuilder::FinishExitWithHardDeoptimization(
    const char* reason, HBasicBlock* continuation) {
  PadEnvironmentForContinuation(current_block(), continuation);
  Add<HDeoptimize>(reason, Deoptimizer::EAGER);
  if (graph()->IsInsideNoSideEffectsScope()) {
    GotoNoSimulate(continuation);
  } else {
    Goto(continuation);
  }
}


void HGraphBuilder::PadEnvironmentForContinuation(
    HBasicBlock* from,
    HBasicBlock* continuation) {
  if (continuation->last_environment() != NULL) {
    // When merging from a deopt block to a continuation, resolve differences in
    // environment by pushing constant 0 and popping extra values so that the
    // environments match during the join. Push 0 since it has the most specific
    // representation, and will not influence representation inference of the
    // phi.
    int continuation_env_length = continuation->last_environment()->length();
    while (continuation_env_length != from->last_environment()->length()) {
      if (continuation_env_length > from->last_environment()->length()) {
        from->last_environment()->Push(graph()->GetConstant0());
      } else {
        from->last_environment()->Pop();
      }
    }
  } else {
    ASSERT(continuation->predecessors()->length() == 0);
  }
}


HValue* HGraphBuilder::BuildCheckMap(HValue* obj, Handle<Map> map) {
  return Add<HCheckMaps>(obj, map, top_info());
}


HValue* HGraphBuilder::BuildWrapReceiver(HValue* object, HValue* function) {
  if (object->type().IsJSObject()) return object;
  return Add<HWrapReceiver>(object, function);
}


HValue* HGraphBuilder::BuildCheckForCapacityGrow(HValue* object,
                                                 HValue* elements,
                                                 ElementsKind kind,
                                                 HValue* length,
                                                 HValue* key,
                                                 bool is_js_array) {
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
  HValue* max_capacity = Add<HAdd>(current_capacity, max_gap);
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

  cow_checker.If<HCompareMap>(elements, factory->fixed_cow_array_map());
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
        ? Add<HLoadNamedField>(object, HObjectAccess::ForArrayLength(from_kind))
        : elements_length;

    BuildGrowElementsCapacity(object, elements, from_kind, to_kind,
                              array_length, elements_length);

    if_builder.End();
  }

  Add<HStoreNamedField>(object, HObjectAccess::ForMap(), map);
}


HValue* HGraphBuilder::BuildNumberToString(HValue* object,
                                           Handle<Type> type) {
  NoObservableSideEffectsScope scope(this);

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
  mask = Add<HSar>(mask, graph()->GetConstant1());
  mask = Add<HSub>(mask, graph()->GetConstant1());

  // Check whether object is a smi.
  IfBuilder if_objectissmi(this);
  if_objectissmi.If<HIsSmiAndBranch>(object);
  if_objectissmi.Then();
  {
    // Compute hash for smi similar to smi_get_hash().
    HValue* hash = Add<HBitwise>(Token::BIT_AND, object, mask);

    // Load the key.
    HValue* key_index = Add<HShl>(hash, graph()->GetConstant1());
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
      if_objectissmi.Deopt("Excepted smi");
    } else {
      // Check if the object is a heap number.
      IfBuilder if_objectisnumber(this);
      if_objectisnumber.If<HCompareMap>(
          object, isolate()->factory()->heap_number_map());
      if_objectisnumber.Then();
      {
        // Compute hash for heap number similar to double_get_hash().
        HValue* low = Add<HLoadNamedField>(
            object, HObjectAccess::ForHeapNumberValueLowestBits());
        HValue* high = Add<HLoadNamedField>(
            object, HObjectAccess::ForHeapNumberValueHighestBits());
        HValue* hash = Add<HBitwise>(Token::BIT_XOR, low, high);
        hash = Add<HBitwise>(Token::BIT_AND, hash, mask);

        // Load the key.
        HValue* key_index = Add<HShl>(hash, graph()->GetConstant1());
        HValue* key = Add<HLoadKeyed>(number_string_cache, key_index,
                                      static_cast<HValue*>(NULL),
                                      FAST_ELEMENTS, ALLOW_RETURN_HOLE);

        // Check if key is a heap number (the number string cache contains only
        // SMIs and heap number, so it is sufficient to do a SMI check here).
        IfBuilder if_keyisnotsmi(this);
        if_keyisnotsmi.IfNot<HIsSmiAndBranch>(key);
        if_keyisnotsmi.Then();
        {
          // Check if values of key and object match.
          IfBuilder if_keyeqobject(this);
          if_keyeqobject.If<HCompareNumericAndBranch>(
              Add<HLoadNamedField>(key, HObjectAccess::ForHeapNumberValue()),
              Add<HLoadNamedField>(object, HObjectAccess::ForHeapNumberValue()),
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
    HValue* value_index = Add<HAdd>(key_index, graph()->GetConstant1());
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


HInstruction* HGraphBuilder::BuildUncheckedMonomorphicElementAccess(
    HValue* checked_object,
    HValue* key,
    HValue* val,
    bool is_js_array,
    ElementsKind elements_kind,
    bool is_store,
    LoadKeyedHoleMode load_mode,
    KeyedAccessStoreMode store_mode) {
  ASSERT(!IsExternalArrayElementsKind(elements_kind) || !is_js_array);
  // No GVNFlag is necessary for ElementsKind if there is an explicit dependency
  // on a HElementsTransition instruction. The flag can also be removed if the
  // map to check has FAST_HOLEY_ELEMENTS, since there can be no further
  // ElementsKind transitions. Finally, the dependency can be removed for stores
  // for FAST_ELEMENTS, since a transition to HOLEY elements won't change the
  // generated store code.
  if ((elements_kind == FAST_HOLEY_ELEMENTS) ||
      (elements_kind == FAST_ELEMENTS && is_store)) {
    checked_object->ClearGVNFlag(kDependsOnElementsKind);
  }

  bool fast_smi_only_elements = IsFastSmiElementsKind(elements_kind);
  bool fast_elements = IsFastObjectElementsKind(elements_kind);
  HValue* elements = AddLoadElements(checked_object);
  if (is_store && (fast_elements || fast_smi_only_elements) &&
      store_mode != STORE_NO_TRANSITION_HANDLE_COW) {
    HCheckMaps* check_cow_map = Add<HCheckMaps>(
        elements, isolate()->factory()->fixed_array_map(), top_info());
    check_cow_map->ClearGVNFlag(kDependsOnElementsKind);
  }
  HInstruction* length = NULL;
  if (is_js_array) {
    length = Add<HLoadNamedField>(
        checked_object, HObjectAccess::ForArrayLength(elements_kind));
  } else {
    length = AddLoadFixedArrayLength(elements);
  }
  length->set_type(HType::Smi());
  HValue* checked_key = NULL;
  if (IsExternalArrayElementsKind(elements_kind)) {
    if (store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
      NoObservableSideEffectsScope no_effects(this);
       HLoadExternalArrayPointer* external_elements =
           Add<HLoadExternalArrayPointer>(elements);
      IfBuilder length_checker(this);
      length_checker.If<HCompareNumericAndBranch>(key, length, Token::LT);
      length_checker.Then();
      IfBuilder negative_checker(this);
      HValue* bounds_check = negative_checker.If<HCompareNumericAndBranch>(
          key, graph()->GetConstant0(), Token::GTE);
      negative_checker.Then();
      HInstruction* result = AddElementAccess(
          external_elements, key, val, bounds_check, elements_kind, is_store);
      negative_checker.ElseDeopt("Negative key encountered");
      length_checker.End();
      return result;
    } else {
      ASSERT(store_mode == STANDARD_STORE);
      checked_key = Add<HBoundsCheck>(key, length);
      HLoadExternalArrayPointer* external_elements =
          Add<HLoadExternalArrayPointer>(elements);
      return AddElementAccess(
          external_elements, checked_key, val,
          checked_object, elements_kind, is_store);
    }
  }
  ASSERT(fast_smi_only_elements ||
         fast_elements ||
         IsFastDoubleElementsKind(elements_kind));

  // In case val is stored into a fast smi array, assure that the value is a smi
  // before manipulating the backing store. Otherwise the actual store may
  // deopt, leaving the backing store in an invalid state.
  if (is_store && IsFastSmiElementsKind(elements_kind) &&
      !val->type().IsSmi()) {
    val = Add<HForceRepresentation>(val, Representation::Smi());
  }

  if (IsGrowStoreMode(store_mode)) {
    NoObservableSideEffectsScope no_effects(this);
    elements = BuildCheckForCapacityGrow(checked_object, elements,
                                         elements_kind, length, key,
                                         is_js_array);
    checked_key = key;
  } else {
    checked_key = Add<HBoundsCheck>(key, length);

    if (is_store && (fast_elements || fast_smi_only_elements)) {
      if (store_mode == STORE_NO_TRANSITION_HANDLE_COW) {
        NoObservableSideEffectsScope no_effects(this);
        elements = BuildCopyElementsOnWrite(checked_object, elements,
                                            elements_kind, length);
      } else {
        HCheckMaps* check_cow_map = Add<HCheckMaps>(
            elements, isolate()->factory()->fixed_array_map(), top_info());
        check_cow_map->ClearGVNFlag(kDependsOnElementsKind);
      }
    }
  }
  return AddElementAccess(elements, checked_key, val, checked_object,
                          elements_kind, is_store, load_mode);
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
  HValue* mul = Add<HMul>(capacity, elements_size_value);
  mul->ClearFlag(HValue::kCanOverflow);

  HConstant* header_size = Add<HConstant>(FixedArray::kHeaderSize);
  HValue* total_size = Add<HAdd>(mul, header_size);
  total_size->ClearFlag(HValue::kCanOverflow);

  return Add<HAllocate>(total_size, HType::JSArray(),
      isolate()->heap()->GetPretenureMode(), instance_type);
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
  capacity = Add<HForceRepresentation>(capacity, Representation::Smi());
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
    BuildCreateAllocationMemento(array,
                                 JSArray::kSize,
                                 allocation_site_payload);
  }

  int elements_location = JSArray::kSize;
  if (mode == TRACK_ALLOCATION_SITE) {
    elements_location += AllocationMemento::kSize;
  }

  HValue* elements = Add<HInnerAllocatedObject>(array, elements_location);
  Add<HStoreNamedField>(array, HObjectAccess::ForElementsPointer(), elements);
  return static_cast<HInnerAllocatedObject*>(elements);
}


HInstruction* HGraphBuilder::AddElementAccess(
    HValue* elements,
    HValue* checked_key,
    HValue* val,
    HValue* dependency,
    ElementsKind elements_kind,
    bool is_store,
    LoadKeyedHoleMode load_mode) {
  if (is_store) {
    ASSERT(val != NULL);
    if (elements_kind == EXTERNAL_PIXEL_ELEMENTS) {
      val = Add<HClampToUint8>(val);
    }
    return Add<HStoreKeyed>(elements, checked_key, val, elements_kind);
  }

  ASSERT(!is_store);
  ASSERT(val == NULL);
  HLoadKeyed* load = Add<HLoadKeyed>(
      elements, checked_key, dependency, elements_kind, load_mode);
  if (FLAG_opt_safe_uint32_operations &&
      elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS) {
    graph()->RecordUint32Instruction(load);
  }
  return load;
}


HLoadNamedField* HGraphBuilder::AddLoadElements(HValue* object) {
  return Add<HLoadNamedField>(object, HObjectAccess::ForElementsPointer());
}


HLoadNamedField* HGraphBuilder::AddLoadFixedArrayLength(HValue* object) {
  return Add<HLoadNamedField>(object,
                              HObjectAccess::ForFixedArrayLength());
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
  Heap* heap = isolate()->heap();
  int element_size = IsFastDoubleElementsKind(kind) ? kDoubleSize
                                                    : kPointerSize;
  int max_size = heap->MaxRegularSpaceAllocationSize() / element_size;
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
  static const int kLoopUnfoldLimit = 4;
  bool unfold_loop = false;
  int initial_capacity = JSArray::kPreallocatedArrayElements;
  if (from->IsConstant() && to->IsConstant() &&
      initial_capacity <= kLoopUnfoldLimit) {
    HConstant* constant_from = HConstant::cast(from);
    HConstant* constant_to = HConstant::cast(to);

    if (constant_from->HasInteger32Value() &&
        constant_from->Integer32Value() == 0 &&
        constant_to->HasInteger32Value() &&
        constant_to->Integer32Value() == initial_capacity) {
      unfold_loop = true;
    }
  }

  // Since we're about to store a hole value, the store instruction below must
  // assume an elements kind that supports heap object values.
  if (IsFastSmiOrObjectElementsKind(elements_kind)) {
    elements_kind = FAST_HOLEY_ELEMENTS;
  }

  if (unfold_loop) {
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
      Add<HStoreNamedField>(object, access,
                            Add<HLoadNamedField>(boilerplate, access));
    }
  }

  // Create an allocation site info if requested.
  if (mode == TRACK_ALLOCATION_SITE) {
    BuildCreateAllocationMemento(object, JSArray::kSize, allocation_site);
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
      Add<HStoreNamedField>(object_elements, access,
                            Add<HLoadNamedField>(boilerplate_elements, access));
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
    Handle<Type> type,
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


HValue* HGraphBuilder::BuildCreateAllocationMemento(HValue* previous_object,
                                                    int previous_object_size,
                                                    HValue* alloc_site) {
  ASSERT(alloc_site != NULL);
  HInnerAllocatedObject* alloc_memento = Add<HInnerAllocatedObject>(
      previous_object, previous_object_size);
  Handle<Map> alloc_memento_map =
      isolate()->factory()->allocation_memento_map();
  AddStoreMapConstant(alloc_memento, alloc_memento_map);
  HObjectAccess access = HObjectAccess::ForAllocationMementoSite();
  Add<HStoreNamedField>(alloc_memento, access, alloc_site);
  return alloc_memento;
}


HInstruction* HGraphBuilder::BuildGetNativeContext() {
  // Get the global context, then the native context
  HInstruction* global_object = Add<HGlobalObject>();
  HObjectAccess access = HObjectAccess::ForJSObjectOffset(
      GlobalObject::kNativeContextOffset);
  return Add<HLoadNamedField>(global_object, access);
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
  if (kind_ == GetInitialFastElementsKind()) {
    // No need for a context lookup if the kind_ matches the initial
    // map, because we can just load the map in that case.
    HObjectAccess access = HObjectAccess::ForPrototypeOrInitialMap();
    return builder()->AddLoadNamedField(constructor_function_, access);
  }

  HInstruction* native_context = builder()->BuildGetNativeContext();
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
  return builder()->AddLoadNamedField(constructor_function_, access);
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
  HInstruction* mul = builder()->Add<HMul>(length_node, elements_size_value);
  mul->ClearFlag(HValue::kCanOverflow);

  HInstruction* base = builder()->Add<HConstant>(base_size);
  HInstruction* total_size = builder()->Add<HAdd>(base, mul);
  total_size->ClearFlag(HValue::kCanOverflow);
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
                       builder()->graph()->GetConstant0(),
                       true);
}


HValue* HGraphBuilder::JSArrayBuilder::AllocateArray(HValue* capacity,
                                                     HValue* length_field,
                                                     bool fill_with_hole) {
  HValue* size_in_bytes = EstablishAllocationSize(capacity);
  return AllocateArray(size_in_bytes, capacity, length_field, fill_with_hole);
}


HValue* HGraphBuilder::JSArrayBuilder::AllocateArray(HValue* size_in_bytes,
                                                     HValue* capacity,
                                                     HValue* length_field,
                                                     bool fill_with_hole) {
  // These HForceRepresentations are because we store these as fields in the
  // objects we construct, and an int32-to-smi HChange could deopt. Accept
  // the deopt possibility now, before allocation occurs.
  capacity = builder()->Add<HForceRepresentation>(capacity,
                                                  Representation::Smi());
  length_field = builder()->Add<HForceRepresentation>(length_field,
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

  if (fill_with_hole) {
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
  HGlobalObject* global_object = Add<HGlobalObject>();
  HObjectAccess access = HObjectAccess::ForJSObjectOffset(
      GlobalObject::kBuiltinsOffset);
  HValue* builtins = Add<HLoadNamedField>(global_object, access);
  HObjectAccess function_access = HObjectAccess::ForJSObjectOffset(
      JSBuiltinsObject::OffsetOfFunctionWithId(builtin));
  return Add<HLoadNamedField>(builtins, function_access);
}


HOptimizedGraphBuilder::HOptimizedGraphBuilder(CompilationInfo* info)
    : HGraphBuilder(info),
      function_state_(NULL),
      initial_function_state_(this, info, NORMAL_RETURN),
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
  InitializeAstVisitor(info->isolate());
  if (FLAG_emit_opt_code_positions) {
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


void HBasicBlock::FinishExit(HControlInstruction* instruction, int position) {
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
      disallow_adding_new_values_(false) {
  if (info->IsStub()) {
    HydrogenCodeStub* stub = info->code_stub();
    CodeStubInterfaceDescriptor* descriptor =
        stub->GetInterfaceDescriptor(isolate_);
    start_environment_ =
        new(zone_) HEnvironment(zone_, descriptor->environment_length());
  } else {
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
  ASSERT(!isolate()->optimizing_compiler_thread()->IsOptimizerThread());
  for (int i = 0; i < blocks()->length(); ++i) {
    for (HInstructionIterator it(blocks()->at(i)); !it.Done(); it.Advance()) {
      it.Current()->FinalizeUniqueness();
    }
  }
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
                             InliningKind inlining_kind)
    : owner_(owner),
      compilation_info_(info),
      call_context_(NULL),
      inlining_kind_(inlining_kind),
      function_return_(NULL),
      test_context_(NULL),
      entry_(NULL),
      arguments_object_(NULL),
      arguments_elements_(NULL),
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
}


FunctionState::~FunctionState() {
  delete test_context_;
  owner_->set_function_state(outer_);
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
  HBasicBlock* empty_true = builder->graph()->CreateBasicBlock();
  HBasicBlock* empty_false = builder->graph()->CreateBasicBlock();
  ToBooleanStub::Types expected(condition()->to_boolean_types());
  builder->FinishCurrentBlock(builder->New<HBranch>(
          value, expected, empty_true, empty_false));

  owner()->Goto(empty_true, if_true(), builder->function_state());
  owner()->Goto(empty_false , if_false(), builder->function_state());
  builder->set_current_block(NULL);
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

  if (FLAG_check_elimination) Run<HCheckEliminationPhase>();
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
      if (instruction->ActualValue() != instruction) {
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


template <class Instruction>
HInstruction* HOptimizedGraphBuilder::PreProcessCall(Instruction* call) {
  int count = call->argument_count();
  ZoneList<HValue*> arguments(count, zone());
  for (int i = 0; i < count; ++i) {
    arguments.Add(Pop(), zone());
  }

  while (!arguments.is_empty()) {
    Add<HPushArgument>(arguments.RemoveLast());
  }
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

  // We only optimize switch statements with smi-literal smi comparisons,
  // with a bounded number of clauses.
  const int kCaseClauseLimit = 128;
  ZoneList<CaseClause*>* clauses = stmt->cases();
  int clause_count = clauses->length();
  if (clause_count > kCaseClauseLimit) {
    return Bailout(kSwitchStatementTooManyClauses);
  }

  ASSERT(stmt->switch_type() != SwitchStatement::UNKNOWN_SWITCH);
  if (stmt->switch_type() == SwitchStatement::GENERIC_SWITCH) {
    return Bailout(kSwitchStatementMixedOrNonLiteralSwitchLabels);
  }

  CHECK_ALIVE(VisitForValue(stmt->tag()));
  Add<HSimulate>(stmt->EntryId());
  HValue* tag_value = Pop();
  HBasicBlock* first_test_block = current_block();

  HUnaryControlInstruction* string_check = NULL;
  HBasicBlock* not_string_block = NULL;

  // Test switch's tag value if all clauses are string literals
  if (stmt->switch_type() == SwitchStatement::STRING_SWITCH) {
    first_test_block = graph()->CreateBasicBlock();
    not_string_block = graph()->CreateBasicBlock();
    string_check = New<HIsStringAndBranch>(
        tag_value, first_test_block, not_string_block);
    FinishCurrentBlock(string_check);

    set_current_block(first_test_block);
  }

  // 1. Build all the tests, with dangling true branches
  BailoutId default_id = BailoutId::None();
  for (int i = 0; i < clause_count; ++i) {
    CaseClause* clause = clauses->at(i);
    if (clause->is_default()) {
      default_id = clause->EntryId();
      continue;
    }

    // Generate a compare and branch.
    CHECK_ALIVE(VisitForValue(clause->label()));
    HValue* label_value = Pop();

    HBasicBlock* next_test_block = graph()->CreateBasicBlock();
    HBasicBlock* body_block = graph()->CreateBasicBlock();

    HControlInstruction* compare;

    if (stmt->switch_type() == SwitchStatement::SMI_SWITCH) {
      if (!clause->compare_type()->Is(Type::Smi())) {
        Add<HDeoptimize>("Non-smi switch type", Deoptimizer::SOFT);
      }

      HCompareNumericAndBranch* compare_ =
          New<HCompareNumericAndBranch>(tag_value,
                                        label_value,
                                        Token::EQ_STRICT);
      compare_->set_observed_input_representation(
          Representation::Smi(), Representation::Smi());
      compare = compare_;
    } else {
      compare = New<HStringCompareAndBranch>(tag_value,
                                             label_value,
                                             Token::EQ_STRICT);
    }

    compare->SetSuccessorAt(0, body_block);
    compare->SetSuccessorAt(1, next_test_block);
    FinishCurrentBlock(compare);

    set_current_block(next_test_block);
  }

  // Save the current block to use for the default or to join with the
  // exit.
  HBasicBlock* last_block = current_block();

  if (not_string_block != NULL) {
    BailoutId join_id = !default_id.IsNone() ? default_id : stmt->ExitId();
    last_block = CreateJoin(last_block, not_string_block, join_id);
  }

  // 2. Loop over the clauses and the linked list of tests in lockstep,
  // translating the clause bodies.
  HBasicBlock* curr_test_block = first_test_block;
  HBasicBlock* fall_through_block = NULL;

  BreakAndContinueInfo break_info(stmt);
  { BreakAndContinueScope push(&break_info, this);
    for (int i = 0; i < clause_count; ++i) {
      CaseClause* clause = clauses->at(i);

      // Identify the block where normal (non-fall-through) control flow
      // goes to.
      HBasicBlock* normal_block = NULL;
      if (clause->is_default()) {
        if (last_block != NULL) {
          normal_block = last_block;
          last_block = NULL;  // Cleared to indicate we've handled it.
        }
      } else {
        // If the current test block is deoptimizing due to an unhandled clause
        // of the switch, the test instruction is in the next block since the
        // deopt must end the current block.
        if (curr_test_block->IsDeoptimizing()) {
          ASSERT(curr_test_block->end()->SecondSuccessor() == NULL);
          curr_test_block = curr_test_block->end()->FirstSuccessor();
        }
        normal_block = curr_test_block->end()->FirstSuccessor();
        curr_test_block = curr_test_block->end()->SecondSuccessor();
      }

      // Identify a block to emit the body into.
      if (normal_block == NULL) {
        if (fall_through_block == NULL) {
          // (a) Unreachable.
          if (clause->is_default()) {
            continue;  // Might still be reachable clause bodies.
          } else {
            break;
          }
        } else {
          // (b) Reachable only as fall through.
          set_current_block(fall_through_block);
        }
      } else if (fall_through_block == NULL) {
        // (c) Reachable only normally.
        set_current_block(normal_block);
      } else {
        // (d) Reachable both ways.
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
    // The block for a true condition, the actual predecessor block of the
    // back edge.
    body_exit = graph()->CreateBasicBlock();
    loop_successor = graph()->CreateBasicBlock();
    CHECK_BAILOUT(VisitForControl(stmt->cond(), body_exit, loop_successor));
    if (body_exit->HasPredecessor()) {
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
    Push(Add<HAdd>(current_index, graph()->GetConstant1()));
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


static Handle<SharedFunctionInfo> SearchSharedFunctionInfo(
    Code* unoptimized_code, FunctionLiteral* expr) {
  int start_position = expr->start_position();
  for (RelocIterator it(unoptimized_code); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    if (rinfo->rmode() != RelocInfo::EMBEDDED_OBJECT) continue;
    Object* obj = rinfo->target_object();
    if (obj->IsSharedFunctionInfo()) {
      SharedFunctionInfo* shared = SharedFunctionInfo::cast(obj);
      if (shared->start_position() == start_position) {
        return Handle<SharedFunctionInfo>(shared);
      }
    }
  }

  return Handle<SharedFunctionInfo>();
}


void HOptimizedGraphBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  Handle<SharedFunctionInfo> shared_info =
      SearchSharedFunctionInfo(current_info()->shared_info()->code(), expr);
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
        Variable* var, LookupResult* lookup, bool is_store) {
  if (var->is_this() || !current_info()->has_global_object()) {
    return kUseGeneric;
  }
  Handle<GlobalObject> global(current_info()->global_object());
  global->Lookup(*var->name(), lookup);
  if (!lookup->IsNormal() ||
      (is_store && lookup->IsReadOnly()) ||
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
    context = Add<HOuterContext>(context);
  }
  return context;
}


void HOptimizedGraphBuilder::VisitVariableProxy(VariableProxy* expr) {
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
      GlobalPropertyAccess type =
          LookupGlobalProperty(variable, &lookup, false);

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
        HGlobalObject* global_object = Add<HGlobalObject>();
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


static bool CanInlinePropertyAccess(Map* type) {
  return type->IsJSObjectMap() &&
      !type->is_dictionary_map() &&
      !type->has_named_interceptor();
}


static void LookupInPrototypes(Handle<Map> map,
                               Handle<String> name,
                               LookupResult* lookup) {
  while (map->prototype()->IsJSObject()) {
    Handle<JSObject> holder(JSObject::cast(map->prototype()));
    map = Handle<Map>(holder->map());
    if (!CanInlinePropertyAccess(*map)) break;
    map->LookupDescriptor(*holder, *name, lookup);
    if (lookup->IsFound()) return;
  }
  lookup->NotFound();
}


// Tries to find a JavaScript accessor of the given name in the prototype chain
// starting at the given map. Return true iff there is one, including the
// corresponding AccessorPair plus its holder (which could be null when the
// accessor is found directly in the given map).
static bool LookupAccessorPair(Handle<Map> map,
                               Handle<String> name,
                               Handle<AccessorPair>* accessors,
                               Handle<JSObject>* holder) {
  Isolate* isolate = map->GetIsolate();
  LookupResult lookup(isolate);

  // Check for a JavaScript accessor directly in the map.
  map->LookupDescriptor(NULL, *name, &lookup);
  if (lookup.IsPropertyCallbacks()) {
    Handle<Object> callback(lookup.GetValueFromMap(*map), isolate);
    if (!callback->IsAccessorPair()) return false;
    *accessors = Handle<AccessorPair>::cast(callback);
    *holder = Handle<JSObject>();
    return true;
  }

  // Everything else, e.g. a field, can't be an accessor call.
  if (lookup.IsFound()) return false;

  // Check for a JavaScript accessor somewhere in the proto chain.
  LookupInPrototypes(map, name, &lookup);
  if (lookup.IsPropertyCallbacks()) {
    Handle<Object> callback(lookup.GetValue(), isolate);
    if (!callback->IsAccessorPair()) return false;
    *accessors = Handle<AccessorPair>::cast(callback);
    *holder = Handle<JSObject>(lookup.holder());
    return true;
  }

  // We haven't found a JavaScript accessor anywhere.
  return false;
}


static bool LookupSetter(Handle<Map> map,
                         Handle<String> name,
                         Handle<JSFunction>* setter,
                         Handle<JSObject>* holder) {
  Handle<AccessorPair> accessors;
  if (LookupAccessorPair(map, name, &accessors, holder) &&
      accessors->setter()->IsJSFunction()) {
    Handle<JSFunction> func(JSFunction::cast(accessors->setter()));
    CallOptimization call_optimization(func);
    // TODO(dcarney): temporary hack unless crankshaft can handle api calls.
    if (call_optimization.is_simple_api_call()) return false;
    *setter = func;
    return true;
  }
  return false;
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
              CHECK_ALIVE(store = BuildStoreNamedGeneric(literal, name, value));
            } else {
#if DEBUG
              Handle<JSFunction> setter;
              Handle<JSObject> holder;
              ASSERT(!LookupSetter(map, name, &setter, &holder));
#endif
              CHECK_ALIVE(store = BuildStoreNamedMonomorphic(literal,
                                                             name,
                                                             value,
                                                             map));
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

    Add<HPushArgument>(Add<HConstant>(literals));
    Add<HPushArgument>(Add<HConstant>(literal_index));
    Add<HPushArgument>(Add<HConstant>(constants));

    // TODO(mvstanton): Consider a flag to turn off creation of any
    // AllocationMementos for this call: we are in crankshaft and should have
    // learned enough about transition behavior to stop emitting mementos.
    Runtime::FunctionId function_id = (expr->depth() > 1)
        ? Runtime::kCreateArrayLiteral : Runtime::kCreateArrayLiteralShallow;
    literal = Add<HCallRuntime>(isolate()->factory()->empty_string(),
                                Runtime::FunctionForId(function_id),
                                3);

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


HInstruction* HOptimizedGraphBuilder::BuildStoreNamedField(
    HValue* checked_object,
    Handle<String> name,
    HValue* value,
    Handle<Map> map,
    LookupResult* lookup) {
  ASSERT(lookup->IsFound());
  // If the property does not exist yet, we have to check that it wasn't made
  // readonly or turned into a setter by some meanwhile modifications on the
  // prototype chain.
  if (!lookup->IsProperty() && map->prototype()->IsJSReceiver()) {
    Object* proto = map->prototype();
    // First check that the prototype chain isn't affected already.
    LookupResult proto_result(isolate());
    proto->Lookup(*name, &proto_result);
    if (proto_result.IsProperty()) {
      // If the inherited property could induce readonly-ness, bail out.
      if (proto_result.IsReadOnly() || !proto_result.IsCacheable()) {
        Bailout(kImproperObjectOnPrototypeChainForStore);
        return NULL;
      }
      // We only need to check up to the preexisting property.
      proto = proto_result.holder();
    } else {
      // Otherwise, find the top prototype.
      while (proto->GetPrototype(isolate())->IsJSObject()) {
        proto = proto->GetPrototype(isolate());
      }
      ASSERT(proto->GetPrototype(isolate())->IsNull());
    }
    ASSERT(proto->IsJSObject());
    BuildCheckPrototypeMaps(
        Handle<JSObject>(JSObject::cast(map->prototype())),
        Handle<JSObject>(JSObject::cast(proto)));
  }

  HObjectAccess field_access = HObjectAccess::ForField(map, lookup, name);
  bool transition_to_field = lookup->IsTransitionToField(*map);

  HStoreNamedField *instr;
  if (FLAG_track_double_fields && field_access.representation().IsDouble()) {
    HObjectAccess heap_number_access =
        field_access.WithRepresentation(Representation::Tagged());
    if (transition_to_field) {
      // The store requires a mutable HeapNumber to be allocated.
      NoObservableSideEffectsScope no_side_effects(this);
      HInstruction* heap_number_size = Add<HConstant>(HeapNumber::kSize);
      HInstruction* heap_number = Add<HAllocate>(heap_number_size,
          HType::HeapNumber(), isolate()->heap()->GetPretenureMode(),
          HEAP_NUMBER_TYPE);
      AddStoreMapConstant(heap_number, isolate()->factory()->heap_number_map());
      Add<HStoreNamedField>(heap_number, HObjectAccess::ForHeapNumberValue(),
                            value);
      instr = New<HStoreNamedField>(checked_object->ActualValue(),
                                    heap_number_access,
                                    heap_number);
    } else {
      // Already holds a HeapNumber; load the box and write its value field.
      HInstruction* heap_number = Add<HLoadNamedField>(checked_object,
                                                       heap_number_access);
      heap_number->set_type(HType::HeapNumber());
      instr = New<HStoreNamedField>(heap_number,
                                    HObjectAccess::ForHeapNumberValue(),
                                    value);
    }
  } else {
    // This is a normal store.
    instr = New<HStoreNamedField>(checked_object->ActualValue(),
                                  field_access,
                                  value);
  }

  if (transition_to_field) {
    Handle<Map> transition(lookup->GetTransitionMapFromMap(*map));
    HConstant* transition_constant = Add<HConstant>(transition);
    instr->SetTransition(transition_constant, top_info());
    // TODO(fschneider): Record the new map type of the object in the IR to
    // enable elimination of redundant checks after the transition store.
    instr->SetGVNFlag(kChangesMaps);
  }
  return instr;
}


HInstruction* HOptimizedGraphBuilder::BuildStoreNamedGeneric(
    HValue* object,
    Handle<String> name,
    HValue* value) {
  return New<HStoreNamedGeneric>(
                         object,
                         name,
                         value,
                         function_strict_mode_flag());
}


// Sets the lookup result and returns true if the load/store can be inlined.
static bool ComputeStoreField(Handle<Map> type,
                              Handle<String> name,
                              LookupResult* lookup,
                              bool lookup_transition = true) {
  ASSERT(!type->is_observed());
  if (!CanInlinePropertyAccess(*type)) {
    lookup->NotFound();
    return false;
  }
  // If we directly find a field, the access can be inlined.
  type->LookupDescriptor(NULL, *name, lookup);
  if (lookup->IsField()) return true;

  if (!lookup_transition) return false;

  type->LookupTransition(NULL, *name, lookup);
  return lookup->IsTransitionToField(*type) &&
      (type->unused_property_fields() > 0);
}


HInstruction* HOptimizedGraphBuilder::BuildStoreNamedMonomorphic(
    HValue* object,
    Handle<String> name,
    HValue* value,
    Handle<Map> map) {
  // Handle a store to a known field.
  LookupResult lookup(isolate());
  if (ComputeStoreField(map, name, &lookup)) {
    HCheckMaps* checked_object = AddCheckMap(object, map);
    return BuildStoreNamedField(checked_object, name, value, map, &lookup);
  }

  // No luck, do a generic store.
  return BuildStoreNamedGeneric(object, name, value);
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::IsCompatibleForLoad(
    PropertyAccessInfo* info) {
  if (!CanInlinePropertyAccess(*map_)) return false;

  if (!LookupDescriptor()) return false;

  if (!lookup_.IsFound()) {
    return (!info->lookup_.IsFound() || info->has_holder()) &&
        map_->prototype() == info->map_->prototype();
  }

  // Mismatch if the other access info found the property in the prototype
  // chain.
  if (info->has_holder()) return false;

  if (lookup_.IsPropertyCallbacks()) {
    return accessor_.is_identical_to(info->accessor_);
  }

  if (lookup_.IsConstant()) {
    return constant_.is_identical_to(info->constant_);
  }

  ASSERT(lookup_.IsField());
  if (!info->lookup_.IsField()) return false;

  Representation r = access_.representation();
  if (!info->access_.representation().IsCompatibleForLoad(r)) return false;
  if (info->access_.offset() != access_.offset()) return false;
  if (info->access_.IsInobject() != access_.IsInobject()) return false;
  info->GeneralizeRepresentation(r);
  return true;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LookupDescriptor() {
  map_->LookupDescriptor(NULL, *name_, &lookup_);
  return LoadResult(map_);
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LoadResult(Handle<Map> map) {
  if (lookup_.IsField()) {
    access_ = HObjectAccess::ForField(map, &lookup_, name_);
  } else if (lookup_.IsPropertyCallbacks()) {
    Handle<Object> callback(lookup_.GetValueFromMap(*map), isolate());
    if (!callback->IsAccessorPair()) return false;
    Object* getter = Handle<AccessorPair>::cast(callback)->getter();
    if (!getter->IsJSFunction()) return false;
    Handle<JSFunction> accessor = handle(JSFunction::cast(getter));
    CallOptimization call_optimization(accessor);
    // TODO(dcarney): temporary hack unless crankshaft can handle api calls.
    if (call_optimization.is_simple_api_call()) return false;
    accessor_ = accessor;
  } else if (lookup_.IsConstant()) {
    constant_ = handle(lookup_.GetConstantFromMap(*map), isolate());
  }

  return true;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LookupInPrototypes() {
  Handle<Map> map = map_;
  while (map->prototype()->IsJSObject()) {
    holder_ = handle(JSObject::cast(map->prototype()));
    if (holder_->map()->is_deprecated()) {
      JSObject::TryMigrateInstance(holder_);
    }
    map = Handle<Map>(holder_->map());
    if (!CanInlinePropertyAccess(*map)) {
      lookup_.NotFound();
      return false;
    }
    map->LookupDescriptor(*holder_, *name_, &lookup_);
    if (lookup_.IsFound()) return LoadResult(map);
  }
  lookup_.NotFound();
  return true;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::CanLoadMonomorphic() {
  if (!CanInlinePropertyAccess(*map_)) return IsStringLength();
  if (IsJSObjectFieldAccessor()) return true;
  if (!LookupDescriptor()) return false;
  if (lookup_.IsFound()) return true;
  return LookupInPrototypes();
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::CanLoadAsMonomorphic(
    SmallMapList* types) {
  ASSERT(map_.is_identical_to(types->first()));
  if (!CanLoadMonomorphic()) return false;
  if (types->length() > kMaxLoadPolymorphism) return false;

  if (IsStringLength()) {
    for (int i = 1; i < types->length(); ++i) {
      if (types->at(i)->instance_type() >= FIRST_NONSTRING_TYPE) return false;
    }
    return true;
  }

  if (IsArrayLength()) {
    bool is_fast = IsFastElementsKind(map_->elements_kind());
    for (int i = 1; i < types->length(); ++i) {
      Handle<Map> test_map = types->at(i);
      if (test_map->instance_type() != JS_ARRAY_TYPE) return false;
      if (IsFastElementsKind(test_map->elements_kind()) != is_fast) {
        return false;
      }
    }
    return true;
  }

  if (IsJSObjectFieldAccessor()) {
    InstanceType instance_type = map_->instance_type();
    for (int i = 1; i < types->length(); ++i) {
      if (types->at(i)->instance_type() != instance_type) return false;
    }
    return true;
  }

  for (int i = 1; i < types->length(); ++i) {
    PropertyAccessInfo test_info(isolate(), types->at(i), name_);
    if (!test_info.IsCompatibleForLoad(this)) return false;
  }

  return true;
}


HInstruction* HOptimizedGraphBuilder::BuildLoadMonomorphic(
    PropertyAccessInfo* info,
    HValue* object,
    HInstruction* checked_object,
    BailoutId ast_id,
    BailoutId return_id,
    bool can_inline_accessor) {

  HObjectAccess access = HObjectAccess::ForMap();  // bogus default
  if (info->GetJSObjectFieldAccess(&access)) {
    return New<HLoadNamedField>(checked_object, access);
  }

  HValue* checked_holder = checked_object;
  if (info->has_holder()) {
    Handle<JSObject> prototype(JSObject::cast(info->map()->prototype()));
    checked_holder = BuildCheckPrototypeMaps(prototype, info->holder());
  }

  if (!info->lookup()->IsFound()) return graph()->GetConstantUndefined();

  if (info->lookup()->IsField()) {
    return BuildLoadNamedField(checked_holder, info->access());
  }

  if (info->lookup()->IsPropertyCallbacks()) {
    Push(checked_object);
    if (FLAG_inline_accessors &&
        can_inline_accessor &&
        TryInlineGetter(info->accessor(), ast_id, return_id)) {
      return NULL;
    }
    Add<HPushArgument>(Pop());
    return New<HCallConstantFunction>(info->accessor(), 1);
  }

  ASSERT(info->lookup()->IsConstant());
  return New<HConstant>(info->constant());
}


void HOptimizedGraphBuilder::HandlePolymorphicLoadNamedField(
    BailoutId ast_id,
    BailoutId return_id,
    HValue* object,
    SmallMapList* types,
    Handle<String> name) {
  // Something did not match; must use a polymorphic load.
  int count = 0;
  HBasicBlock* join = NULL;
  for (int i = 0; i < types->length() && count < kMaxLoadPolymorphism; ++i) {
    PropertyAccessInfo info(isolate(), types->at(i), name);
    if (info.CanLoadMonomorphic()) {
      if (count == 0) {
        BuildCheckHeapObject(object);
        join = graph()->CreateBasicBlock();
      }
      ++count;
      HBasicBlock* if_true = graph()->CreateBasicBlock();
      HBasicBlock* if_false = graph()->CreateBasicBlock();
      HCompareMap* compare = New<HCompareMap>(
          object, info.map(),  if_true, if_false);
      FinishCurrentBlock(compare);

      set_current_block(if_true);

      HInstruction* load = BuildLoadMonomorphic(
          &info, object, compare, ast_id, return_id, FLAG_polymorphic_inlining);
      if (load == NULL) {
        if (HasStackOverflow()) return;
      } else {
        if (!load->IsLinked()) {
          AddInstruction(load);
        }
        if (!ast_context()->IsEffect()) Push(load);
      }

      if (current_block() != NULL) Goto(join);
      set_current_block(if_false);
    }
  }

  // Finish up.  Unconditionally deoptimize if we've handled all the maps we
  // know about and do not want to handle ones we've never seen.  Otherwise
  // use a generic IC.
  if (count == types->length() && FLAG_deoptimize_uncommon_cases) {
    // Because the deopt may be the only path in the polymorphic load, make sure
    // that the environment stack matches the depth on deopt that it otherwise
    // would have had after a successful load.
    if (!ast_context()->IsEffect()) Push(graph()->GetConstant0());
    FinishExitWithHardDeoptimization("Unknown map in polymorphic load", join);
  } else {
    HInstruction* load = Add<HLoadNamedGeneric>(object, name);
    if (!ast_context()->IsEffect()) Push(load);

    if (join != NULL) {
      Goto(join);
    } else {
      Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
      if (!ast_context()->IsEffect()) ast_context()->ReturnValue(Pop());
      return;
    }
  }

  ASSERT(join != NULL);
  join->SetJoinId(ast_id);
  set_current_block(join);
  if (!ast_context()->IsEffect()) ast_context()->ReturnValue(Pop());
}


bool HOptimizedGraphBuilder::TryStorePolymorphicAsMonomorphic(
    BailoutId assignment_id,
    HValue* object,
    HValue* value,
    SmallMapList* types,
    Handle<String> name) {
  // Use monomorphic store if property lookup results in the same field index
  // for all maps. Requires special map check on the set of all handled maps.
  if (types->length() > kMaxStorePolymorphism) return false;

  LookupResult lookup(isolate());
  int count;
  Representation representation = Representation::None();
  HObjectAccess access = HObjectAccess::ForMap();  // initial value unused.
  for (count = 0; count < types->length(); ++count) {
    Handle<Map> map = types->at(count);
    // Pass false to ignore transitions.
    if (!ComputeStoreField(map, name, &lookup, false)) break;
    ASSERT(!map->is_observed());

    HObjectAccess new_access = HObjectAccess::ForField(map, &lookup, name);
    Representation new_representation = new_access.representation();

    if (count == 0) {
      // First time through the loop; set access and representation.
      access = new_access;
      representation = new_representation;
    } else if (!representation.IsCompatibleForStore(new_representation)) {
      // Representations did not match.
      break;
    } else if (access.offset() != new_access.offset()) {
      // Offsets did not match.
      break;
    } else if (access.IsInobject() != new_access.IsInobject()) {
      // In-objectness did not match.
      break;
    }
  }

  if (count != types->length()) return false;

  // Everything matched; can use monomorphic store.
  BuildCheckHeapObject(object);
  HCheckMaps* checked_object = Add<HCheckMaps>(object, types);
  HInstruction* store;
  CHECK_ALIVE_OR_RETURN(
      store = BuildStoreNamedField(
          checked_object, name, value, types->at(count - 1), &lookup),
      true);
  if (!ast_context()->IsEffect()) Push(value);
  AddInstruction(store);
  Add<HSimulate>(assignment_id);
  if (!ast_context()->IsEffect()) Drop(1);
  ast_context()->ReturnValue(value);
  return true;
}


void HOptimizedGraphBuilder::HandlePolymorphicStoreNamedField(
    BailoutId assignment_id,
    HValue* object,
    HValue* value,
    SmallMapList* types,
    Handle<String> name) {
  if (TryStorePolymorphicAsMonomorphic(
          assignment_id, object, value, types, name)) {
    return;
  }

  // TODO(ager): We should recognize when the prototype chains for different
  // maps are identical. In that case we can avoid repeatedly generating the
  // same prototype map checks.
  int count = 0;
  HBasicBlock* join = NULL;
  for (int i = 0; i < types->length() && count < kMaxStorePolymorphism; ++i) {
    Handle<Map> map = types->at(i);
    LookupResult lookup(isolate());
    if (ComputeStoreField(map, name, &lookup)) {
      if (count == 0) {
        BuildCheckHeapObject(object);
        join = graph()->CreateBasicBlock();
      }
      ++count;
      HBasicBlock* if_true = graph()->CreateBasicBlock();
      HBasicBlock* if_false = graph()->CreateBasicBlock();
      HCompareMap* compare = New<HCompareMap>(object, map,  if_true, if_false);
      FinishCurrentBlock(compare);

      set_current_block(if_true);
      HInstruction* instr;
      CHECK_ALIVE(instr = BuildStoreNamedField(
          compare, name, value, map, &lookup));
      // Goto will add the HSimulate for the store.
      AddInstruction(instr);
      if (!ast_context()->IsEffect()) Push(value);
      Goto(join);

      set_current_block(if_false);
    }
  }

  // Finish up.  Unconditionally deoptimize if we've handled all the maps we
  // know about and do not want to handle ones we've never seen.  Otherwise
  // use a generic IC.
  if (count == types->length() && FLAG_deoptimize_uncommon_cases) {
    FinishExitWithHardDeoptimization("Unknown map in polymorphic store", join);
  } else {
    HInstruction* instr = BuildStoreNamedGeneric(object, name, value);
    AddInstruction(instr);

    if (join != NULL) {
      if (!ast_context()->IsEffect()) {
        Push(value);
      }
      Goto(join);
    } else {
      // The HSimulate for the store should not see the stored value in
      // effect contexts (it is not materialized at expr->id() in the
      // unoptimized code).
      if (instr->HasObservableSideEffects()) {
        if (ast_context()->IsEffect()) {
          Add<HSimulate>(assignment_id, REMOVABLE_SIMULATE);
        } else {
          Push(value);
          Add<HSimulate>(assignment_id, REMOVABLE_SIMULATE);
          Drop(1);
        }
      }
      return ast_context()->ReturnValue(value);
    }
  }

  ASSERT(join != NULL);
  join->SetJoinId(assignment_id);
  set_current_block(join);
  if (!ast_context()->IsEffect()) {
    ast_context()->ReturnValue(Pop());
  }
}


static bool ComputeReceiverTypes(Expression* expr,
                                 HValue* receiver,
                                 SmallMapList** t) {
  SmallMapList* types = expr->GetReceiverTypes();
  *t = types;
  bool monomorphic = expr->IsMonomorphic();
  if (types != NULL && receiver->HasMonomorphicJSObjectType()) {
    Map* root_map = receiver->GetMonomorphicJSObjectMap()->FindRootMap();
    types->FilterForPossibleTransitions(root_map);
    monomorphic = types->length() == 1;
  }
  return monomorphic && CanInlinePropertyAccess(*types->first());
}


void HOptimizedGraphBuilder::BuildStore(Expression* expr,
                                        Property* prop,
                                        BailoutId ast_id,
                                        BailoutId return_id,
                                        bool is_uninitialized) {
  HValue* value = environment()->ExpressionStackAt(0);

  if (!prop->key()->IsPropertyName()) {
    // Keyed store.
    HValue* key = environment()->ExpressionStackAt(1);
    HValue* object = environment()->ExpressionStackAt(2);
    bool has_side_effects = false;
    HandleKeyedElementAccess(object, key, value, expr,
                             true,  // is_store
                             &has_side_effects);
    Drop(3);
    Push(value);
    Add<HSimulate>(return_id, REMOVABLE_SIMULATE);
    return ast_context()->ReturnValue(Pop());
  }

  // Named store.
  HValue* object = environment()->ExpressionStackAt(1);

  if (is_uninitialized) {
    Add<HDeoptimize>("Insufficient type feedback for property assignment",
                     Deoptimizer::SOFT);
  }

  Literal* key = prop->key()->AsLiteral();
  Handle<String> name = Handle<String>::cast(key->value());
  ASSERT(!name.is_null());

  HInstruction* instr = NULL;

  SmallMapList* types;
  bool monomorphic = ComputeReceiverTypes(expr, object, &types);

  if (monomorphic) {
    Handle<Map> map = types->first();
    Handle<JSFunction> setter;
    Handle<JSObject> holder;
    if (LookupSetter(map, name, &setter, &holder)) {
      AddCheckConstantFunction(holder, object, map);
      if (FLAG_inline_accessors &&
          TryInlineSetter(setter, ast_id, return_id, value)) {
        return;
      }
      Drop(2);
      Add<HPushArgument>(object);
      Add<HPushArgument>(value);
      instr = New<HCallConstantFunction>(setter, 2);
    } else {
      Drop(2);
      CHECK_ALIVE(instr = BuildStoreNamedMonomorphic(object,
                                                     name,
                                                     value,
                                                     map));
    }
  } else if (types != NULL && types->length() > 1) {
    Drop(2);
    return HandlePolymorphicStoreNamedField(ast_id, object, value, types, name);
  } else {
    Drop(2);
    instr = BuildStoreNamedGeneric(object, name, value);
  }

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
  GlobalPropertyAccess type = LookupGlobalProperty(var, &lookup, true);
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
    HGlobalObject* global_object = Add<HGlobalObject>();
    HStoreGlobalGeneric* instr =
        Add<HStoreGlobalGeneric>(global_object, var->name(),
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

    HInstruction* instr = BuildBinaryOperation(operation, left, right);
    AddInstruction(instr);
    Push(instr);
    if (instr->HasObservableSideEffects()) {
      Add<HSimulate>(operation->id(), REMOVABLE_SIMULATE);
    }
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
  if (!FLAG_emit_opt_code_positions) SetSourcePosition(expr->position());
  Add<HThrow>(value);
  Add<HSimulate>(expr->id());

  // If the throw definitely exits the function, we can finish with a dummy
  // control flow at this point.  This is not the case if the throw is inside
  // an inlined function which may be replaced.
  if (call_context() == NULL) {
    FinishExitCurrentBlock(New<HAbnormalExit>());
  }
}


HLoadNamedField* HGraphBuilder::BuildLoadNamedField(HValue* object,
                                                    HObjectAccess access) {
  if (FLAG_track_double_fields && access.representation().IsDouble()) {
    // load the heap number
    HLoadNamedField* heap_number = Add<HLoadNamedField>(
        object, access.WithRepresentation(Representation::Tagged()));
    heap_number->set_type(HType::HeapNumber());
    // load the double value from it
    return New<HLoadNamedField>(
        heap_number, HObjectAccess::ForHeapNumberValue());
  }
  return New<HLoadNamedField>(object, access);
}


HInstruction* HGraphBuilder::AddLoadNamedField(HValue* object,
                                               HObjectAccess access) {
  return AddInstruction(BuildLoadNamedField(object, access));
}


HInstruction* HGraphBuilder::BuildLoadStringLength(HValue* object,
                                                   HValue* checked_string) {
  if (FLAG_fold_constants && object->IsConstant()) {
    HConstant* constant = HConstant::cast(object);
    if (constant->HasStringValue()) {
      return New<HConstant>(constant->StringValue()->length());
    }
  }
  return BuildLoadNamedField(checked_string, HObjectAccess::ForStringLength());
}


HInstruction* HOptimizedGraphBuilder::BuildLoadNamedGeneric(
    HValue* object,
    Handle<String> name,
    Property* expr) {
  if (expr->IsUninitialized()) {
    Add<HDeoptimize>("Insufficient type feedback for generic named load",
                     Deoptimizer::SOFT);
  }
  return New<HLoadNamedGeneric>(object, name);
}



HInstruction* HOptimizedGraphBuilder::BuildLoadKeyedGeneric(HValue* object,
                                                            HValue* key) {
  return New<HLoadKeyedGeneric>(object, key);
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
    bool is_store,
    KeyedAccessStoreMode store_mode) {
  HCheckMaps* checked_object = Add<HCheckMaps>(object, map, top_info(),
                                               dependency);
  if (dependency) {
    checked_object->ClearGVNFlag(kDependsOnElementsKind);
  }

  LoadKeyedHoleMode load_mode = BuildKeyedHoleMode(map);
  return BuildUncheckedMonomorphicElementAccess(
      checked_object, key, val,
      map->instance_type() == JS_ARRAY_TYPE,
      map->elements_kind(), is_store,
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

  HCheckMaps* checked_object = Add<HCheckMaps>(object, maps);
  // FAST_ELEMENTS is considered more general than FAST_HOLEY_SMI_ELEMENTS.
  // If we've seen both, the consolidated load must use FAST_HOLEY_ELEMENTS.
  ElementsKind consolidated_elements_kind = has_seen_holey_elements
      ? GetHoleyElementsKind(most_general_consolidated_map->elements_kind())
      : most_general_consolidated_map->elements_kind();
  HInstruction* instr = BuildUncheckedMonomorphicElementAccess(
      checked_object, key, val,
      most_general_consolidated_map->instance_type() == JS_ARRAY_TYPE,
      consolidated_elements_kind,
      false, NEVER_RETURN_HOLE, STANDARD_STORE);
  return instr;
}


HValue* HOptimizedGraphBuilder::HandlePolymorphicElementAccess(
    HValue* object,
    HValue* key,
    HValue* val,
    SmallMapList* maps,
    bool is_store,
    KeyedAccessStoreMode store_mode,
    bool* has_side_effects) {
  *has_side_effects = false;
  BuildCheckHeapObject(object);

  if (!is_store) {
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
      instr = AddInstruction(is_store ? BuildStoreKeyedGeneric(object, key, val)
                                      : BuildLoadKeyedGeneric(object, key));
    } else {
      instr = BuildMonomorphicElementAccess(
          object, key, val, transition, untransitionable_map, is_store,
          store_mode);
    }
    *has_side_effects |= instr->HasObservableSideEffects();
    return is_store ? NULL : instr;
  }

  HBasicBlock* join = graph()->CreateBasicBlock();

  for (int i = 0; i < untransitionable_maps.length(); ++i) {
    Handle<Map> map = untransitionable_maps[i];
    if (!map->IsJSObjectMap()) continue;
    ElementsKind elements_kind = map->elements_kind();
    HBasicBlock* this_map = graph()->CreateBasicBlock();
    HBasicBlock* other_map = graph()->CreateBasicBlock();
    HCompareMap* mapcompare =
        New<HCompareMap>(object, map, this_map, other_map);
    FinishCurrentBlock(mapcompare);

    set_current_block(this_map);
    HInstruction* access = NULL;
    if (IsDictionaryElementsKind(elements_kind)) {
      access = is_store
          ? AddInstruction(BuildStoreKeyedGeneric(object, key, val))
          : AddInstruction(BuildLoadKeyedGeneric(object, key));
    } else {
      ASSERT(IsFastElementsKind(elements_kind) ||
             IsExternalArrayElementsKind(elements_kind));
      LoadKeyedHoleMode load_mode = BuildKeyedHoleMode(map);
      // Happily, mapcompare is a checked object.
      access = BuildUncheckedMonomorphicElementAccess(
          mapcompare, key, val,
          map->instance_type() == JS_ARRAY_TYPE,
          elements_kind, is_store,
          load_mode,
          store_mode);
    }
    *has_side_effects |= access->HasObservableSideEffects();
    // The caller will use has_side_effects and add a correct Simulate.
    access->SetFlag(HValue::kHasNoObservableSideEffects);
    if (!is_store) {
      Push(access);
    }
    NoObservableSideEffectsScope scope(this);
    GotoNoSimulate(join);
    set_current_block(other_map);
  }

  // Deopt if none of the cases matched.
  NoObservableSideEffectsScope scope(this);
  FinishExitWithHardDeoptimization("Unknown map in polymorphic element access",
                                   join);
  set_current_block(join);
  return is_store ? NULL : Pop();
}


HValue* HOptimizedGraphBuilder::HandleKeyedElementAccess(
    HValue* obj,
    HValue* key,
    HValue* val,
    Expression* expr,
    bool is_store,
    bool* has_side_effects) {
  ASSERT(!expr->IsPropertyName());
  HInstruction* instr = NULL;

  SmallMapList* types;
  bool monomorphic = ComputeReceiverTypes(expr, obj, &types);

  if (monomorphic) {
    Handle<Map> map = types->first();
    if (map->has_slow_elements_kind()) {
      instr = is_store ? BuildStoreKeyedGeneric(obj, key, val)
                       : BuildLoadKeyedGeneric(obj, key);
      AddInstruction(instr);
    } else {
      BuildCheckHeapObject(obj);
      instr = BuildMonomorphicElementAccess(
          obj, key, val, NULL, map, is_store, expr->GetStoreMode());
    }
  } else if (types != NULL && !types->is_empty()) {
    return HandlePolymorphicElementAccess(
        obj, key, val, types, is_store,
        expr->GetStoreMode(), has_side_effects);
  } else {
    if (is_store) {
      if (expr->IsAssignment() &&
          expr->AsAssignment()->HasNoTypeInformation()) {
        Add<HDeoptimize>("Insufficient type feedback for keyed store",
                         Deoptimizer::SOFT);
      }
      instr = BuildStoreKeyedGeneric(obj, key, val);
    } else {
      if (expr->AsProperty()->HasNoTypeInformation()) {
        Add<HDeoptimize>("Insufficient type feedback for keyed load",
                         Deoptimizer::SOFT);
      }
      instr = BuildLoadKeyedGeneric(obj, key);
    }
    AddInstruction(instr);
  }
  *has_side_effects = instr->HasObservableSideEffects();
  return instr;
}


HInstruction* HOptimizedGraphBuilder::BuildStoreKeyedGeneric(
    HValue* object,
    HValue* key,
    HValue* value) {
  return New<HStoreKeyedGeneric>(
                         object,
                         key,
                         value,
                         function_strict_mode_flag());
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


void HOptimizedGraphBuilder::PushLoad(Property* expr,
                                      HValue* object,
                                      HValue* key) {
  ValueContext for_value(this, ARGUMENTS_NOT_ALLOWED);
  Push(object);
  if (key != NULL) Push(key);
  BuildLoad(expr, expr->LoadId());
}


static bool AreStringTypes(SmallMapList* types) {
  for (int i = 0; i < types->length(); i++) {
    if (types->at(i)->instance_type() >= FIRST_NONSTRING_TYPE) return false;
  }
  return true;
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

    SmallMapList* types;
    ComputeReceiverTypes(expr, object, &types);
    ASSERT(types != NULL);

    if (types->length() > 0) {
      PropertyAccessInfo info(isolate(), types->first(), name);
      if (!info.CanLoadAsMonomorphic(types)) {
        return HandlePolymorphicLoadNamedField(
            ast_id, expr->LoadId(), object, types, name);
      }

      BuildCheckHeapObject(object);
      HInstruction* checked_object;
      if (AreStringTypes(types)) {
        checked_object =
            Add<HCheckInstanceType>(object, HCheckInstanceType::IS_STRING);
      } else {
        checked_object = Add<HCheckMaps>(object, types);
      }
      instr = BuildLoadMonomorphic(
          &info, object, checked_object, ast_id, expr->LoadId());
      if (instr == NULL) return;
      if (instr->IsLinked()) return ast_context()->ReturnValue(instr);
    } else {
      instr = BuildLoadNamedGeneric(object, name, expr);
    }

  } else {
    HValue* key = Pop();
    HValue* obj = Pop();

    bool has_side_effects = false;
    HValue* load = HandleKeyedElementAccess(
        obj, key, NULL, expr,
        false,  // is_store
        &has_side_effects);
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
  check->ClearGVNFlag(kDependsOnElementsKind);
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


void HOptimizedGraphBuilder::AddCheckConstantFunction(
    Handle<JSObject> holder,
    HValue* receiver,
    Handle<Map> receiver_map) {
  // Constant functions have the nice property that the map will change if they
  // are overwritten.  Therefore it is enough to check the map of the holder and
  // its prototypes.
  AddCheckMap(receiver, receiver_map);
  AddCheckPrototypeMaps(holder, receiver_map);
}


class FunctionSorter {
 public:
  FunctionSorter() : index_(0), ticks_(0), ast_length_(0), src_length_(0) { }
  FunctionSorter(int index, int ticks, int ast_length, int src_length)
      : index_(index),
        ticks_(ticks),
        ast_length_(ast_length),
        src_length_(src_length) { }

  int index() const { return index_; }
  int ticks() const { return ticks_; }
  int ast_length() const { return ast_length_; }
  int src_length() const { return src_length_; }

 private:
  int index_;
  int ticks_;
  int ast_length_;
  int src_length_;
};


inline bool operator<(const FunctionSorter& lhs, const FunctionSorter& rhs) {
  int diff = lhs.ticks() - rhs.ticks();
  if (diff != 0) return diff > 0;
  diff = lhs.ast_length() - rhs.ast_length();
  if (diff != 0) return diff < 0;
  return lhs.src_length() < rhs.src_length();
}


bool HOptimizedGraphBuilder::TryCallPolymorphicAsMonomorphic(
    Call* expr,
    HValue* receiver,
    SmallMapList* types,
    Handle<String> name) {
  if (types->length() > kMaxCallPolymorphism) return false;

  PropertyAccessInfo info(isolate(), types->at(0), name);
  if (!info.CanLoadAsMonomorphic(types)) return false;
  if (!expr->ComputeTarget(info.map(), name)) return false;

  BuildCheckHeapObject(receiver);
  Add<HCheckMaps>(receiver, types);
  AddCheckPrototypeMaps(expr->holder(), info.map());
  if (FLAG_trace_inlining) {
    Handle<JSFunction> caller = current_info()->closure();
    SmartArrayPointer<char> caller_name =
        caller->shared()->DebugName()->ToCString();
    PrintF("Trying to inline the polymorphic call to %s from %s\n",
           *name->ToCString(), *caller_name);
  }

  if (!TryInlineCall(expr)) {
    int argument_count = expr->arguments()->length() + 1;  // Includes receiver.
    HCallConstantFunction* call =
      New<HCallConstantFunction>(expr->target(), argument_count);
    PreProcessCall(call);
    AddInstruction(call);
    if (!ast_context()->IsEffect()) Push(call);
    Add<HSimulate>(expr->id(), REMOVABLE_SIMULATE);
    if (!ast_context()->IsEffect()) ast_context()->ReturnValue(Pop());
  }

  return true;
}


void HOptimizedGraphBuilder::HandlePolymorphicCallNamed(
    Call* expr,
    HValue* receiver,
    SmallMapList* types,
    Handle<String> name) {
  if (TryCallPolymorphicAsMonomorphic(expr, receiver, types, name)) return;

  int argument_count = expr->arguments()->length() + 1;  // Includes receiver.
  HBasicBlock* join = NULL;
  FunctionSorter order[kMaxCallPolymorphism];
  int ordered_functions = 0;

  Handle<Map> initial_string_map(
      isolate()->native_context()->string_function()->initial_map());
  Handle<Map> string_marker_map(
      JSObject::cast(initial_string_map->prototype())->map());
  Handle<Map> initial_number_map(
      isolate()->native_context()->number_function()->initial_map());
  Handle<Map> number_marker_map(
      JSObject::cast(initial_number_map->prototype())->map());
  Handle<Map> heap_number_map = isolate()->factory()->heap_number_map();

  bool handle_smi = false;

  for (int i = 0;
       i < types->length() && ordered_functions < kMaxCallPolymorphism;
       ++i) {
    Handle<Map> map = types->at(i);
    if (expr->ComputeTarget(map, name)) {
      if (map.is_identical_to(number_marker_map)) handle_smi = true;
      order[ordered_functions++] =
          FunctionSorter(i,
                         expr->target()->shared()->profiler_ticks(),
                         InliningAstSize(expr->target()),
                         expr->target()->shared()->SourceSize());
    }
  }

  std::sort(order, order + ordered_functions);

  HBasicBlock* number_block = NULL;

  for (int fn = 0; fn < ordered_functions; ++fn) {
    int i = order[fn].index();
    Handle<Map> map = types->at(i);
    if (fn == 0) {
      // Only needed once.
      join = graph()->CreateBasicBlock();
      if (handle_smi) {
        HBasicBlock* empty_smi_block = graph()->CreateBasicBlock();
        HBasicBlock* not_smi_block = graph()->CreateBasicBlock();
        number_block = graph()->CreateBasicBlock();
        FinishCurrentBlock(New<HIsSmiAndBranch>(
                receiver, empty_smi_block, not_smi_block));
        Goto(empty_smi_block, number_block);
        set_current_block(not_smi_block);
      } else {
        BuildCheckHeapObject(receiver);
      }
    }
    HBasicBlock* if_true = graph()->CreateBasicBlock();
    HBasicBlock* if_false = graph()->CreateBasicBlock();
    HUnaryControlInstruction* compare;

    if (handle_smi && map.is_identical_to(number_marker_map)) {
      compare = New<HCompareMap>(receiver, heap_number_map, if_true, if_false);
      map = initial_number_map;
      expr->set_number_check(
          Handle<JSObject>(JSObject::cast(map->prototype())));
    } else if (map.is_identical_to(string_marker_map)) {
      compare = New<HIsStringAndBranch>(receiver, if_true, if_false);
      map = initial_string_map;
      expr->set_string_check(
          Handle<JSObject>(JSObject::cast(map->prototype())));
    } else {
      compare = New<HCompareMap>(receiver, map, if_true, if_false);
      expr->set_map_check();
    }

    FinishCurrentBlock(compare);

    if (expr->check_type() == NUMBER_CHECK) {
      Goto(if_true, number_block);
      if_true = number_block;
      number_block->SetJoinId(expr->id());
    }
    set_current_block(if_true);

    expr->ComputeTarget(map, name);
    AddCheckPrototypeMaps(expr->holder(), map);
    if (FLAG_trace_inlining && FLAG_polymorphic_inlining) {
      Handle<JSFunction> caller = current_info()->closure();
      SmartArrayPointer<char> caller_name =
          caller->shared()->DebugName()->ToCString();
      PrintF("Trying to inline the polymorphic call to %s from %s\n",
             *name->ToCString(),
             *caller_name);
    }
    if (FLAG_polymorphic_inlining && TryInlineCall(expr)) {
      // Trying to inline will signal that we should bailout from the
      // entire compilation by setting stack overflow on the visitor.
      if (HasStackOverflow()) return;
    } else {
      HCallConstantFunction* call =
          New<HCallConstantFunction>(expr->target(), argument_count);
      PreProcessCall(call);
      AddInstruction(call);
      if (!ast_context()->IsEffect()) Push(call);
    }

    if (current_block() != NULL) Goto(join);
    set_current_block(if_false);
  }

  // Finish up.  Unconditionally deoptimize if we've handled all the maps we
  // know about and do not want to handle ones we've never seen.  Otherwise
  // use a generic IC.
  if (ordered_functions == types->length() && FLAG_deoptimize_uncommon_cases) {
    // Because the deopt may be the only path in the polymorphic call, make sure
    // that the environment stack matches the depth on deopt that it otherwise
    // would have had after a successful call.
    Drop(argument_count);
    if (!ast_context()->IsEffect()) Push(graph()->GetConstant0());
    FinishExitWithHardDeoptimization("Unknown map in polymorphic call", join);
  } else {
    HCallNamed* call = New<HCallNamed>(name, argument_count);
    PreProcessCall(call);

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
      PrintF("Inlined %s called from %s.\n", *target_name, *caller_name);
    } else {
      PrintF("Did not inline %s called from %s (%s).\n",
             *target_name, *caller_name, reason);
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

  // Do a quick check on source code length to avoid parsing large
  // inlining candidates.
  if (target_shared->SourceSize() >
      Min(FLAG_max_inlined_source_size, kUnlimitedMaxInlinedSourceSize)) {
    TraceInline(target, caller, "target text too big");
    return kNotInlinable;
  }

  // Target must be inlineable.
  if (!target->IsInlineable()) {
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


bool HOptimizedGraphBuilder::TryInline(CallKind call_kind,
                                       Handle<JSFunction> target,
                                       int arguments_count,
                                       HValue* implicit_return_value,
                                       BailoutId ast_id,
                                       BailoutId return_id,
                                       InliningKind inlining_kind) {
  int nodes_added = InliningAstSize(target);
  if (nodes_added == kNotInlinable) return false;

  Handle<JSFunction> caller = current_info()->closure();

  if (nodes_added > Min(FLAG_max_inlined_nodes, kUnlimitedMaxInlinedNodes)) {
    TraceInline(target, caller, "target AST is too large [early]");
    return false;
  }

#if !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_MIPS
  // Target must be able to use caller's context.
  CompilationInfo* outer_info = current_info();
  if (target->context() != outer_info->closure()->context() ||
      outer_info->scope()->contains_with() ||
      outer_info->scope()->num_heap_slots() > 0) {
    TraceInline(target, caller, "target requires context change");
    return false;
  }
#endif


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

  // Save the pending call context. Set up new one for the inlined function.
  // The function state is new-allocated because we need to delete it
  // in two different places.
  FunctionState* target_state = new FunctionState(
      this, &target_info, inlining_kind);

  HConstant* undefined = graph()->GetConstantUndefined();
  bool undefined_receiver = HEnvironment::UseUndefinedReceiver(
      target, function, call_kind, inlining_kind);
  HEnvironment* inner_env =
      environment()->CopyForInlining(target,
                                     arguments_count,
                                     function,
                                     undefined,
                                     function_state()->inlining_kind(),
                                     undefined_receiver);
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_MIPS
  // IA32, ARM and MIPS only, overwrite the caller's context in the
  // deoptimization environment with the correct one.
  //
  // TODO(kmillikin): implement the same inlining on other platforms so we
  // can remove the unsightly ifdefs in this function.
  HConstant* context = Add<HConstant>(Handle<Context>(target->context()));
  inner_env->BindContext(context);
#endif

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
                         arguments_object, undefined_receiver);
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


bool HOptimizedGraphBuilder::TryInlineCall(Call* expr, bool drop_extra) {
  // The function call we are inlining is a method call if the call
  // is a property call.
  CallKind call_kind = (expr->expression()->AsProperty() == NULL)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;

  return TryInline(call_kind,
                   expr->target(),
                   expr->arguments()->length(),
                   NULL,
                   expr->id(),
                   expr->ReturnId(),
                   drop_extra ? DROP_EXTRA_ON_RETURN : NORMAL_RETURN);
}


bool HOptimizedGraphBuilder::TryInlineConstruct(CallNew* expr,
                                                HValue* implicit_return_value) {
  return TryInline(CALL_AS_FUNCTION,
                   expr->target(),
                   expr->arguments()->length(),
                   implicit_return_value,
                   expr->id(),
                   expr->ReturnId(),
                   CONSTRUCT_CALL_RETURN);
}


bool HOptimizedGraphBuilder::TryInlineGetter(Handle<JSFunction> getter,
                                             BailoutId ast_id,
                                             BailoutId return_id) {
  return TryInline(CALL_AS_METHOD,
                   getter,
                   0,
                   NULL,
                   ast_id,
                   return_id,
                   GETTER_CALL_RETURN);
}


bool HOptimizedGraphBuilder::TryInlineSetter(Handle<JSFunction> setter,
                                             BailoutId id,
                                             BailoutId assignment_id,
                                             HValue* implicit_return_value) {
  return TryInline(CALL_AS_METHOD,
                   setter,
                   1,
                   implicit_return_value,
                   id, assignment_id,
                   SETTER_CALL_RETURN);
}


bool HOptimizedGraphBuilder::TryInlineApply(Handle<JSFunction> function,
                                            Call* expr,
                                            int arguments_count) {
  return TryInline(CALL_AS_METHOD,
                   function,
                   arguments_count,
                   NULL,
                   expr->id(),
                   expr->ReturnId(),
                   NORMAL_RETURN);
}


bool HOptimizedGraphBuilder::TryInlineBuiltinFunctionCall(Call* expr,
                                                          bool drop_extra) {
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
    case kMathSin:
    case kMathCos:
    case kMathTan:
      if (expr->arguments()->length() == 1) {
        HValue* argument = Pop();
        Drop(1);  // Receiver.
        HInstruction* op = NewUncasted<HUnaryMathOperation>(argument, id);
        if (drop_extra) Drop(1);  // Optionally drop the function.
        ast_context()->ReturnInstruction(op, expr->id());
        return true;
      }
      break;
    case kMathImul:
      if (expr->arguments()->length() == 2) {
        HValue* right = Pop();
        HValue* left = Pop();
        Drop(1);  // Receiver.
        HInstruction* op = HMul::NewImul(zone(), context(), left, right);
        if (drop_extra) Drop(1);  // Optionally drop the function.
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
    Handle<Map> receiver_map,
    CheckType check_type) {
  ASSERT(check_type != RECEIVER_MAP_CHECK || !receiver_map.is_null());
  // Try to inline calls like Math.* as operations in the calling function.
  if (!expr->target()->shared()->HasBuiltinFunctionId()) return false;
  BuiltinFunctionId id = expr->target()->shared()->builtin_function_id();
  int argument_count = expr->arguments()->length() + 1;  // Plus receiver.
  switch (id) {
    case kStringCharCodeAt:
    case kStringCharAt:
      if (argument_count == 2 && check_type == STRING_CHECK) {
        HValue* index = Pop();
        HValue* string = Pop();
        ASSERT(!expr->holder().is_null());
        BuildCheckPrototypeMaps(Call::GetPrototypeForPrimitiveCheck(
                STRING_CHECK, expr->holder()->GetIsolate()),
            expr->holder());
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
      if (argument_count == 2 && check_type == RECEIVER_MAP_CHECK) {
        AddCheckConstantFunction(expr->holder(), receiver, receiver_map);
        HValue* argument = Pop();
        Drop(1);  // Receiver.
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
    case kMathSin:
    case kMathCos:
    case kMathTan:
      if (argument_count == 2 && check_type == RECEIVER_MAP_CHECK) {
        AddCheckConstantFunction(expr->holder(), receiver, receiver_map);
        HValue* argument = Pop();
        Drop(1);  // Receiver.
        HInstruction* op = NewUncasted<HUnaryMathOperation>(argument, id);
        ast_context()->ReturnInstruction(op, expr->id());
        return true;
      }
      break;
    case kMathPow:
      if (argument_count == 3 && check_type == RECEIVER_MAP_CHECK) {
        AddCheckConstantFunction(expr->holder(), receiver, receiver_map);
        HValue* right = Pop();
        HValue* left = Pop();
        Pop();  // Pop receiver.
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
    case kMathRandom:
      if (argument_count == 1 && check_type == RECEIVER_MAP_CHECK) {
        AddCheckConstantFunction(expr->holder(), receiver, receiver_map);
        Drop(1);  // Receiver.
        HGlobalObject* global_object = Add<HGlobalObject>();
        HRandom* result = New<HRandom>(global_object);
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    case kMathMax:
    case kMathMin:
      if (argument_count == 3 && check_type == RECEIVER_MAP_CHECK) {
        AddCheckConstantFunction(expr->holder(), receiver, receiver_map);
        HValue* right = Pop();
        HValue* left = Pop();
        Drop(1);  // Receiver.
        HMathMinMax::Operation op = (id == kMathMin) ? HMathMinMax::kMathMin
                                                     : HMathMinMax::kMathMax;
        HInstruction* result = NewUncasted<HMathMinMax>(left, right, op);
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    case kMathImul:
      if (argument_count == 3 && check_type == RECEIVER_MAP_CHECK) {
        AddCheckConstantFunction(expr->holder(), receiver, receiver_map);
        HValue* right = Pop();
        HValue* left = Pop();
        Drop(1);  // Receiver.
        HInstruction* result = HMul::NewImul(zone(), context(), left, right);
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    default:
      // Not yet supported for inlining.
      break;
  }
  return false;
}


bool HOptimizedGraphBuilder::TryCallApply(Call* expr) {
  Expression* callee = expr->expression();
  Property* prop = callee->AsProperty();
  ASSERT(prop != NULL);

  if (!expr->IsMonomorphic() || expr->check_type() != RECEIVER_MAP_CHECK) {
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
  CHECK_ALIVE_OR_RETURN(VisitForValue(prop->obj()), true);
  HValue* function = Top();
  AddCheckConstantFunction(expr->holder(), function, function_map);
  Drop(1);

  CHECK_ALIVE_OR_RETURN(VisitForValue(args->at(0)), true);
  HValue* receiver = Pop();

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
    Push(BuildWrapReceiver(receiver, function));
    for (int i = 1; i < arguments_count; i++) {
      Push(arguments_values->at(i));
    }

    Handle<JSFunction> known_function;
    if (function->IsConstant()) {
      HConstant* constant_function = HConstant::cast(function);
      known_function = Handle<JSFunction>::cast(
          constant_function->handle(isolate()));
      int args_count = arguments_count - 1;  // Excluding receiver.
      if (TryInlineApply(known_function, expr, args_count)) return true;
    }

    Drop(arguments_count - 1);
    Push(Add<HPushArgument>(Pop()));
    for (int i = 1; i < arguments_count; i++) {
      Push(Add<HPushArgument>(arguments_values->at(i)));
    }

    HInvokeFunction* call = New<HInvokeFunction>(function,
                                                 known_function,
                                                 arguments_count);
    Drop(arguments_count);
    ast_context()->ReturnInstruction(call, expr->id());
    return true;
  }
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
    if (!prop->key()->IsPropertyName()) {
      // Keyed function call.
      CHECK_ALIVE(VisitArgument(prop->obj()));

      CHECK_ALIVE(VisitForValue(prop->key()));
      // Push receiver and key like the non-optimized code generator expects it.
      HValue* key = Pop();
      HValue* receiver = Pop();
      Push(key);
      Push(receiver);

      CHECK_ALIVE(VisitArgumentList(expr->arguments()));

      call = New<HCallKeyed>(key, argument_count);
      Drop(argument_count + 1);  // 1 is the key.
      return ast_context()->ReturnInstruction(call, expr->id());
    }

    // Named function call.
    if (TryCallApply(expr)) return;

    CHECK_ALIVE(VisitForValue(prop->obj()));
    CHECK_ALIVE(VisitExpressions(expr->arguments()));

    Handle<String> name = prop->key()->AsLiteral()->AsPropertyName();
    HValue* receiver =
        environment()->ExpressionStackAt(expr->arguments()->length());

    SmallMapList* types;
    bool was_monomorphic = expr->IsMonomorphic();
    bool monomorphic = ComputeReceiverTypes(expr, receiver, &types);
    if (!was_monomorphic && monomorphic) {
      monomorphic = expr->ComputeTarget(types->first(), name);
    }

    if (monomorphic) {
      Handle<Map> map = types->first();
      if (TryInlineBuiltinMethodCall(expr, receiver, map, expr->check_type())) {
        if (FLAG_trace_inlining) {
          PrintF("Inlining builtin ");
          expr->target()->ShortPrint();
          PrintF("\n");
        }
        return;
      }

      if (CallStubCompiler::HasCustomCallGenerator(expr->target()) ||
          expr->check_type() != RECEIVER_MAP_CHECK) {
        // When the target has a custom call IC generator, use the IC,
        // because it is likely to generate better code.  Also use the IC
        // when a primitive receiver check is required.
        call = PreProcessCall(New<HCallNamed>(name, argument_count));
      } else {
        AddCheckConstantFunction(expr->holder(), receiver, map);

        if (TryInlineCall(expr)) return;
        call = PreProcessCall(
            New<HCallConstantFunction>(expr->target(), argument_count));
      }
    } else if (types != NULL && types->length() > 1) {
      ASSERT(expr->check_type() == RECEIVER_MAP_CHECK);
      HandlePolymorphicCallNamed(expr, receiver, types, name);
      return;

    } else {
      call = PreProcessCall(New<HCallNamed>(name, argument_count));
    }
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
      GlobalPropertyAccess type = LookupGlobalProperty(var, &lookup, false);
      if (type == kUseCell &&
          !current_info()->global_object()->IsAccessCheckNeeded()) {
        Handle<GlobalObject> global(current_info()->global_object());
        known_global_function = expr->ComputeGlobalTarget(global, &lookup);
      }
      if (known_global_function) {
        // Push the global object instead of the global receiver because
        // code generated by the full code generator expects it.
        HGlobalObject* global_object = Add<HGlobalObject>();
        Push(global_object);
        CHECK_ALIVE(VisitExpressions(expr->arguments()));

        CHECK_ALIVE(VisitForValue(expr->expression()));
        HValue* function = Pop();
        Add<HCheckValue>(function, expr->target());

        // Replace the global object with the global receiver.
        HGlobalReceiver* global_receiver = Add<HGlobalReceiver>(global_object);
        // Index of the receiver from the top of the expression stack.
        const int receiver_index = argument_count - 1;
        ASSERT(environment()->ExpressionStackAt(receiver_index)->
               IsGlobalObject());
        environment()->SetExpressionStackAt(receiver_index, global_receiver);

        if (TryInlineBuiltinFunctionCall(expr, false)) {  // Nothing to drop.
          if (FLAG_trace_inlining) {
            PrintF("Inlining builtin ");
            expr->target()->ShortPrint();
            PrintF("\n");
          }
          return;
        }
        if (TryInlineCall(expr)) return;

        if (expr->target().is_identical_to(current_info()->closure())) {
          graph()->MarkRecursive();
        }

        if (CallStubCompiler::HasCustomCallGenerator(expr->target())) {
          // When the target has a custom call IC generator, use the IC,
          // because it is likely to generate better code.
          call = PreProcessCall(New<HCallNamed>(var->name(), argument_count));
        } else {
          call = PreProcessCall(New<HCallKnownGlobal>(
              expr->target(), argument_count));
        }
      } else {
        HGlobalObject* receiver = Add<HGlobalObject>();
        Push(Add<HPushArgument>(receiver));
        CHECK_ALIVE(VisitArgumentList(expr->arguments()));

        call = New<HCallGlobal>(var->name(), argument_count);
        Drop(argument_count);
      }

    } else if (expr->IsMonomorphic()) {
      // The function is on the stack in the unoptimized code during
      // evaluation of the arguments.
      CHECK_ALIVE(VisitForValue(expr->expression()));
      HValue* function = Top();
      HGlobalObject* global = Add<HGlobalObject>();
      HGlobalReceiver* receiver = Add<HGlobalReceiver>(global);
      Push(receiver);
      CHECK_ALIVE(VisitExpressions(expr->arguments()));
      Add<HCheckValue>(function, expr->target());

      if (TryInlineBuiltinFunctionCall(expr, true)) {  // Drop the function.
        if (FLAG_trace_inlining) {
          PrintF("Inlining builtin ");
          expr->target()->ShortPrint();
          PrintF("\n");
        }
        return;
      }

      if (TryInlineCall(expr, true)) {   // Drop function from environment.
        return;
      } else {
        call = PreProcessCall(New<HInvokeFunction>(function, expr->target(),
                                                   argument_count));
        Drop(1);  // The function.
      }

    } else {
      CHECK_ALIVE(VisitForValue(expr->expression()));
      HValue* function = Top();
      HGlobalObject* global_object = Add<HGlobalObject>();
      HGlobalReceiver* receiver = Add<HGlobalReceiver>(global_object);
      Push(Add<HPushArgument>(receiver));
      CHECK_ALIVE(VisitArgumentList(expr->arguments()));

      call = New<HCallFunction>(function, argument_count);
      Drop(argument_count + 1);
    }
  }

  return ast_context()->ReturnInstruction(call, expr->id());
}


// Checks whether allocation using the given constructor can be inlined.
static bool IsAllocationInlineable(Handle<JSFunction> constructor) {
  return constructor->has_initial_map() &&
      constructor->initial_map()->instance_type() == JS_OBJECT_TYPE &&
      constructor->initial_map()->instance_size() < HAllocate::kMaxInlineSize &&
      constructor->initial_map()->InitialPropertiesLength() == 0;
}


void HOptimizedGraphBuilder::VisitCallNew(CallNew* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (!FLAG_emit_opt_code_positions) SetSourcePosition(expr->position());
  int argument_count = expr->arguments()->length() + 1;  // Plus constructor.
  Factory* factory = isolate()->factory();

  if (FLAG_inline_construct &&
      expr->IsMonomorphic() &&
      IsAllocationInlineable(expr->target())) {
    // The constructor function is on the stack in the unoptimized code
    // during evaluation of the arguments.
    CHECK_ALIVE(VisitForValue(expr->expression()));
    HValue* function = Top();
    CHECK_ALIVE(VisitExpressions(expr->arguments()));
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
        (FLAG_pretenuring_call_new &&
            isolate()->heap()->GetPretenureMode() == TENURED)
                ? TENURED : NOT_TENURED;
    HAllocate* receiver =
        Add<HAllocate>(size_in_bytes, HType::JSObject(), pretenure_flag,
        JS_OBJECT_TYPE);
    receiver->set_known_initial_map(initial_map);

    // Load the initial map from the constructor.
    HValue* constructor_value = Add<HConstant>(constructor);
    HValue* initial_map_value =
      Add<HLoadNamedField>(constructor_value, HObjectAccess::ForJSObjectOffset(
            JSFunction::kPrototypeOrInitialMapOffset));

    // Initialize map and fields of the newly allocated object.
    { NoObservableSideEffectsScope no_effects(this);
      ASSERT(initial_map->instance_type() == JS_OBJECT_TYPE);
      Add<HStoreNamedField>(receiver,
          HObjectAccess::ForJSObjectOffset(JSObject::kMapOffset),
          initial_map_value);
      HValue* empty_fixed_array = Add<HConstant>(factory->empty_fixed_array());
      Add<HStoreNamedField>(receiver,
          HObjectAccess::ForJSObjectOffset(JSObject::kPropertiesOffset),
          empty_fixed_array);
      Add<HStoreNamedField>(receiver,
          HObjectAccess::ForJSObjectOffset(JSObject::kElementsOffset),
          empty_fixed_array);
      if (initial_map->inobject_properties() != 0) {
        HConstant* undefined = graph()->GetConstantUndefined();
        for (int i = 0; i < initial_map->inobject_properties(); i++) {
          int property_offset = JSObject::kHeaderSize + i * kPointerSize;
          Add<HStoreNamedField>(receiver,
              HObjectAccess::ForJSObjectOffset(property_offset),
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
    CHECK_ALIVE(VisitArgument(expr->expression()));
    HValue* constructor = HPushArgument::cast(Top())->argument();
    CHECK_ALIVE(VisitArgumentList(expr->arguments()));
    HBinaryCall* call;
    if (expr->target().is_identical_to(array_function)) {
      Handle<Cell> cell = expr->allocation_info_cell();
      Add<HCheckValue>(constructor, array_function);
      call = New<HCallNewArray>(constructor, argument_count,
                                cell, expr->elements_kind());
    } else {
      call = New<HCallNew>(constructor, argument_count);
    }
    Drop(argument_count);
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


void HOptimizedGraphBuilder::VisitCallRuntime(CallRuntime* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (expr->is_jsruntime()) {
    return Bailout(kCallToAJavaScriptRuntimeFunction);
  }

  const Runtime::Function* function = expr->function();
  ASSERT(function != NULL);
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
  Handle<Type> info = expr->type();
  Representation rep = Representation::FromType(info);
  if (rep.IsNone() || rep.IsTagged()) {
    rep = Representation::Smi();
  }

  if (returns_original_input) {
    // We need an explicit HValue representing ToNumber(input).  The
    // actual HChange instruction we need is (sometimes) added in a later
    // phase, so it is not available now to be used as an input to HAdd and
    // as the return value.
    HInstruction* number_input = Add<HForceRepresentation>(Pop(), rep);
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
  if (!FLAG_emit_opt_code_positions) SetSourcePosition(expr->position());
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
  BuildCheckHeapObject(string);
  HValue* checkstring =
      Add<HCheckInstanceType>(string, HCheckInstanceType::IS_STRING);
  HInstruction* length = BuildLoadStringLength(string, checkstring);
  AddInstruction(length);
  HInstruction* checked_index = Add<HBoundsCheck>(index, length);
  return New<HStringCharCodeAt>(string, checked_index);
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
                                         Handle<Type> expected) {
  if (expected->Is(Type::Smi())) {
    return Add<HForceRepresentation>(number, Representation::Smi());
  }
  if (expected->Is(Type::Signed32())) {
    return Add<HForceRepresentation>(number, Representation::Integer32());
  }
  return number;
}


HValue* HGraphBuilder::TruncateToNumber(HValue* value, Handle<Type>* expected) {
  if (value->IsConstant()) {
    HConstant* constant = HConstant::cast(value);
    Maybe<HConstant*> number = constant->CopyToTruncatedNumber(zone());
    if (number.has_value) {
      *expected = handle(Type::Number(), isolate());
      return AddInstruction(number.value);
    }
  }

  // We put temporary values on the stack, which don't correspond to anything
  // in baseline code. Since nothing is observable we avoid recording those
  // pushes with a NoObservableSideEffectsScope.
  NoObservableSideEffectsScope no_effects(this);

  Handle<Type> expected_type = *expected;

  // Separate the number type from the rest.
  Handle<Type> expected_obj = handle(Type::Intersect(
      expected_type, handle(Type::NonNumber(), isolate())), isolate());
  Handle<Type> expected_number = handle(Type::Intersect(
      expected_type, handle(Type::Number(), isolate())), isolate());

  // We expect to get a number.
  // (We need to check first, since Type::None->Is(Type::Any()) == true.
  if (expected_obj->Is(Type::None())) {
    ASSERT(!expected_number->Is(Type::None()));
    return value;
  }

  if (expected_obj->Is(Type::Undefined())) {
    // This is already done by HChange.
    *expected = handle(Type::Union(
          expected_number, handle(Type::Double(), isolate())), isolate());
    return value;
  }

  return value;
}


HInstruction* HOptimizedGraphBuilder::BuildBinaryOperation(
    BinaryOperation* expr,
    HValue* left,
    HValue* right) {
  Handle<Type> left_type = expr->left()->bounds().lower;
  Handle<Type> right_type = expr->right()->bounds().lower;
  Handle<Type> result_type = expr->bounds().lower;
  Maybe<int> fixed_right_arg = expr->fixed_right_arg();

  return HGraphBuilder::BuildBinaryOperation(expr->op(), left, right,
      left_type, right_type, result_type, fixed_right_arg);
}


HInstruction* HGraphBuilder::BuildBinaryOperation(
    Token::Value op,
    HValue* left,
    HValue* right,
    Handle<Type> left_type,
    Handle<Type> right_type,
    Handle<Type> result_type,
    Maybe<int> fixed_right_arg,
    bool binop_stub) {

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
    left_type = handle(Type::Any(), isolate());
  } else {
    if (!maybe_string_add) left = TruncateToNumber(left, &left_type);
    left_rep = Representation::FromType(left_type);
  }

  if (right_type->Is(Type::None())) {
    Add<HDeoptimize>("Insufficient type feedback for RHS of binary operation",
                     Deoptimizer::SOFT);
    right_type = handle(Type::Any(), isolate());
  } else {
    if (!maybe_string_add) right = TruncateToNumber(right, &right_type);
    right_rep = Representation::FromType(right_type);
  }

  // Special case for string addition here.
  if (op == Token::ADD &&
      (left_type->Is(Type::String()) || right_type->Is(Type::String()))) {
    // Validate type feedback for left argument.
    if (left_type->Is(Type::String())) {
      IfBuilder if_isstring(this);
      if_isstring.If<HIsStringAndBranch>(left);
      if_isstring.Then();
      if_isstring.ElseDeopt("Expected string for LHS of binary operation");
    }

    // Validate type feedback for right argument.
    if (right_type->Is(Type::String())) {
      IfBuilder if_isstring(this);
      if_isstring.If<HIsStringAndBranch>(right);
      if_isstring.Then();
      if_isstring.ElseDeopt("Expected string for RHS of binary operation");
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
      return NewUncasted<HInvokeFunction>(function, 2);
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
      return NewUncasted<HInvokeFunction>(function, 2);
    }

    return NewUncasted<HStringAdd>(left, right, STRING_ADD_CHECK_NONE);
  }

  if (binop_stub) {
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
  if (binop_stub && is_non_primitive) {
    HValue* function = AddLoadJSBuiltin(BinaryOpIC::TokenToJSBuiltin(op));
    Add<HPushArgument>(left);
    Add<HPushArgument>(right);
    instr = NewUncasted<HInvokeFunction>(function, 2);
  } else {
    switch (op) {
      case Token::ADD:
        instr = NewUncasted<HAdd>(left, right);
        break;
      case Token::SUB:
        instr = NewUncasted<HSub>(left, right);
        break;
      case Token::MUL:
        instr = NewUncasted<HMul>(left, right);
        break;
      case Token::MOD:
        instr = NewUncasted<HMod>(left, right, fixed_right_arg);
        break;
      case Token::DIV:
        instr = NewUncasted<HDiv>(left, right);
        break;
      case Token::BIT_XOR:
      case Token::BIT_AND:
        instr = NewUncasted<HBitwise>(op, left, right);
        break;
      case Token::BIT_OR: {
        HValue* operand, *shift_amount;
        if (left_type->Is(Type::Signed32()) &&
            right_type->Is(Type::Signed32()) &&
            MatchRotateRight(left, right, &operand, &shift_amount)) {
          instr = NewUncasted<HRor>(operand, shift_amount);
        } else {
          instr = NewUncasted<HBitwise>(op, left, right);
        }
        break;
      }
      case Token::SAR:
        instr = NewUncasted<HSar>(left, right);
        break;
      case Token::SHR:
        instr = NewUncasted<HShr>(left, right);
        if (FLAG_opt_safe_uint32_operations && instr->IsShr() &&
            CanBeZero(right)) {
          graph()->RecordUint32Instruction(instr);
        }
        break;
      case Token::SHL:
        instr = NewUncasted<HShl>(left, right);
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
    if (binop_stub) {
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

    if (left_value->IsConstant()) {
      HConstant* left_constant = HConstant::cast(left_value);
      if ((is_logical_and && left_constant->BooleanValue()) ||
          (!is_logical_and && !left_constant->BooleanValue())) {
        Drop(1);  // left_value.
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
  if (!FLAG_emit_opt_code_positions) SetSourcePosition(expr->position());
  HValue* right = Pop();
  HValue* left = Pop();
  HInstruction* instr = BuildBinaryOperation(expr, left, right);
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::HandleLiteralCompareTypeof(CompareOperation* expr,
                                                        Expression* sub_expr,
                                                        Handle<String> check) {
  CHECK_ALIVE(VisitForTypeOf(sub_expr));
  if (!FLAG_emit_opt_code_positions) SetSourcePosition(expr->position());
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

  if (!FLAG_emit_opt_code_positions) SetSourcePosition(expr->position());

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

  Handle<Type> left_type = expr->left()->bounds().lower;
  Handle<Type> right_type = expr->right()->bounds().lower;
  Handle<Type> combined_type = expr->combined_type();
  Representation combined_rep = Representation::FromType(combined_type);
  Representation left_rep = Representation::FromType(left_type);
  Representation right_rep = Representation::FromType(right_type);

  CHECK_ALIVE(VisitForValue(expr->left()));
  CHECK_ALIVE(VisitForValue(expr->right()));

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

  // Cases handled below depend on collected type feedback. They should
  // soft deoptimize when there is no type feedback.
  if (combined_type->Is(Type::None())) {
    Add<HDeoptimize>("Insufficient type feedback for combined type "
                     "of binary operation",
                     Deoptimizer::SOFT);
    combined_type = left_type = right_type = handle(Type::Any(), isolate());
  }

  if (combined_type->Is(Type::Receiver())) {
    switch (op) {
      case Token::EQ:
      case Token::EQ_STRICT: {
        // Can we get away with map check and not instance type check?
        if (combined_type->IsClass()) {
          Handle<Map> map = combined_type->AsClass();
          AddCheckMap(left, map);
          AddCheckMap(right, map);
          HCompareObjectEqAndBranch* result =
              New<HCompareObjectEqAndBranch>(left, right);
          return ast_context()->ReturnControl(result, expr->id());
        } else {
          BuildCheckHeapObject(left);
          Add<HCheckInstanceType>(left, HCheckInstanceType::IS_SPEC_OBJECT);
          BuildCheckHeapObject(right);
          Add<HCheckInstanceType>(right, HCheckInstanceType::IS_SPEC_OBJECT);
          HCompareObjectEqAndBranch* result =
              New<HCompareObjectEqAndBranch>(left, right);
          return ast_context()->ReturnControl(result, expr->id());
        }
      }
      default:
        return Bailout(kUnsupportedNonPrimitiveCompare);
    }
  } else if (combined_type->Is(Type::InternalizedString()) &&
             Token::IsEqualityOp(op)) {
    BuildCheckHeapObject(left);
    Add<HCheckInstanceType>(left, HCheckInstanceType::IS_INTERNALIZED_STRING);
    BuildCheckHeapObject(right);
    Add<HCheckInstanceType>(right, HCheckInstanceType::IS_INTERNALIZED_STRING);
    HCompareObjectEqAndBranch* result =
        New<HCompareObjectEqAndBranch>(left, right);
    return ast_context()->ReturnControl(result, expr->id());
  } else if (combined_type->Is(Type::String())) {
    BuildCheckHeapObject(left);
    Add<HCheckInstanceType>(left, HCheckInstanceType::IS_STRING);
    BuildCheckHeapObject(right);
    Add<HCheckInstanceType>(right, HCheckInstanceType::IS_STRING);
    HStringCompareAndBranch* result =
        New<HStringCompareAndBranch>(left, right, op);
    return ast_context()->ReturnControl(result, expr->id());
  } else {
    if (combined_rep.IsTagged() || combined_rep.IsNone()) {
      HCompareGeneric* result = New<HCompareGeneric>(left, right, op);
      result->set_observed_input_representation(1, left_rep);
      result->set_observed_input_representation(2, right_rep);
      return ast_context()->ReturnInstruction(result, expr->id());
    } else {
      HCompareNumericAndBranch* result =
          New<HCompareNumericAndBranch>(left, right, op);
      result->set_observed_input_representation(left_rep, right_rep);
      return ast_context()->ReturnControl(result, expr->id());
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
  if (!FLAG_emit_opt_code_positions) SetSourcePosition(expr->position());
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
    Handle<Type> type = expr->combined_type()->Is(Type::None())
        ? handle(Type::Any(), isolate_)
        : expr->combined_type();
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
    AllocationSiteContext* site_context) {
  NoObservableSideEffectsScope no_effects(this);
  InstanceType instance_type = boilerplate_object->map()->instance_type();
  ASSERT(instance_type == JS_ARRAY_TYPE || instance_type == JS_OBJECT_TYPE);

  HType type = instance_type == JS_ARRAY_TYPE
      ? HType::JSArray() : HType::JSObject();
  HValue* object_size_constant = Add<HConstant>(
      boilerplate_object->map()->instance_size());
  HInstruction* object = Add<HAllocate>(object_size_constant, type,
      isolate()->heap()->GetPretenureMode(), instance_type);

  BuildEmitObjectHeader(boilerplate_object, object);

  Handle<FixedArrayBase> elements(boilerplate_object->elements());
  int elements_size = (elements->length() > 0 &&
      elements->map() != isolate()->heap()->fixed_cow_array_map()) ?
          elements->Size() : 0;

  HInstruction* object_elements = NULL;
  if (elements_size > 0) {
    HValue* object_elements_size = Add<HConstant>(elements_size);
    if (boilerplate_object->HasFastDoubleElements()) {
      object_elements = Add<HAllocate>(object_elements_size, HType::JSObject(),
          isolate()->heap()->GetPretenureMode(), FIXED_DOUBLE_ARRAY_TYPE);
    } else {
      object_elements = Add<HAllocate>(object_elements_size, HType::JSObject(),
          isolate()->heap()->GetPretenureMode(), FIXED_ARRAY_TYPE);
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
    BuildEmitInObjectProperties(boilerplate_object, object, site_context);
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
    AllocationSiteContext* site_context) {
  Handle<DescriptorArray> descriptors(
      boilerplate_object->map()->instance_descriptors());
  int limit = boilerplate_object->map()->NumberOfOwnDescriptors();

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
        HObjectAccess::ForJSObjectOffset(property_offset);

    if (value->IsJSObject()) {
      Handle<JSObject> value_object = Handle<JSObject>::cast(value);
      Handle<AllocationSite> current_site = site_context->EnterNewScope();
      HInstruction* result =
          BuildFastLiteral(value_object, site_context);
      site_context->ExitScope(current_site, value_object);
      Add<HStoreNamedField>(object, access, result);
    } else {
      Representation representation = details.representation();
      HInstruction* value_instruction = Add<HConstant>(value);

      if (representation.IsDouble()) {
        // Allocate a HeapNumber box and store the value into it.
        HValue* heap_number_constant = Add<HConstant>(HeapNumber::kSize);
        // TODO(mvstanton): This heap number alloc does not have a corresponding
        // AllocationSite. That is okay because
        // 1) it's a child object of another object with a valid allocation site
        // 2) we can just use the mode of the parent object for pretenuring
        // The todo is replace GetPretenureMode() with
        // site_context->top()->GetPretenureMode().
        HInstruction* double_box =
            Add<HAllocate>(heap_number_constant, HType::HeapNumber(),
                isolate()->heap()->GetPretenureMode(), HEAP_NUMBER_TYPE);
        AddStoreMapConstant(double_box,
            isolate()->factory()->heap_number_map());
        Add<HStoreNamedField>(double_box, HObjectAccess::ForHeapNumberValue(),
            value_instruction);
        value_instruction = double_box;
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
    HObjectAccess access = HObjectAccess::ForJSObjectOffset(property_offset);
    Add<HStoreNamedField>(object, access, value_instruction);
  }
}


void HOptimizedGraphBuilder::BuildEmitElements(
    Handle<JSObject> boilerplate_object,
    Handle<FixedArrayBase> elements,
    HValue* object_elements,
    AllocationSiteContext* site_context) {
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
    AllocationSiteContext* site_context) {
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
  HValue* value = Pop();
  HValueOf* result = New<HValueOf>(value);
  return ast_context()->ReturnInstruction(result, call->id());
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
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(2)));
  HValue* value = Pop();
  HValue* index = Pop();
  HValue* string = Pop();
  HSeqStringSetChar* result = New<HSeqStringSetChar>(
      String::ONE_BYTE_ENCODING, string, index, value);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateTwoByteSeqStringSetChar(
    CallRuntime* call) {
  ASSERT(call->arguments()->length() == 3);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(2)));
  HValue* value = Pop();
  HValue* index = Pop();
  HValue* string = Pop();
  HSeqStringSetChar* result = New<HSeqStringSetChar>(
      String::TWO_BYTE_ENCODING, string, index, value);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateSetValueOf(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 2);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* value = Pop();
  HValue* object = Pop();
  // Check if object is a not a smi.
  HBasicBlock* if_smi = graph()->CreateBasicBlock();
  HBasicBlock* if_heap_object = graph()->CreateBasicBlock();
  HBasicBlock* join = graph()->CreateBasicBlock();
  FinishCurrentBlock(New<HIsSmiAndBranch>(object, if_smi, if_heap_object));
  Goto(if_smi, join);

  // Check if object is a JSValue.
  set_current_block(if_heap_object);
  HHasInstanceTypeAndBranch* typecheck =
      New<HHasInstanceTypeAndBranch>(object, JS_VALUE_TYPE);
  HBasicBlock* if_js_value = graph()->CreateBasicBlock();
  HBasicBlock* not_js_value = graph()->CreateBasicBlock();
  typecheck->SetSuccessorAt(0, if_js_value);
  typecheck->SetSuccessorAt(1, not_js_value);
  FinishCurrentBlock(typecheck);
  Goto(not_js_value, join);

  // Create in-object property store to kValueOffset.
  set_current_block(if_js_value);
  Add<HStoreNamedField>(object,
      HObjectAccess::ForJSObjectOffset(JSValue::kValueOffset), value);
  Goto(if_js_value, join);
  join->SetJoinId(call->id());
  set_current_block(join);
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


// Fast support for Math.random().
void HOptimizedGraphBuilder::GenerateRandomHeapNumber(CallRuntime* call) {
  HGlobalObject* global_object = Add<HGlobalObject>();
  HRandom* result = New<HRandom>(global_object);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for StringAdd.
void HOptimizedGraphBuilder::GenerateStringAdd(CallRuntime* call) {
  ASSERT_EQ(2, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* right = Pop();
  HValue* left = Pop();
  HInstruction* result = New<HStringAdd>(left, right, STRING_ADD_CHECK_BOTH);
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
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HCallStub* result = New<HCallStub>(CodeStub::RegExpConstructResult, 3);
  Drop(3);
  return ast_context()->ReturnInstruction(result, call->id());
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
  HValue* result = BuildNumberToString(
      number, handle(Type::Number(), isolate()));
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

  // Branch for function proxies, or other non-functions.
  HHasInstanceTypeAndBranch* typecheck =
      New<HHasInstanceTypeAndBranch>(function, JS_FUNCTION_TYPE);
  HBasicBlock* if_jsfunction = graph()->CreateBasicBlock();
  HBasicBlock* if_nonfunction = graph()->CreateBasicBlock();
  HBasicBlock* join = graph()->CreateBasicBlock();
  typecheck->SetSuccessorAt(0, if_jsfunction);
  typecheck->SetSuccessorAt(1, if_nonfunction);
  FinishCurrentBlock(typecheck);

  set_current_block(if_jsfunction);
  HInstruction* invoke_result = Add<HInvokeFunction>(function, arg_count);
  Drop(arg_count);
  Push(invoke_result);
  Goto(if_jsfunction, join);

  set_current_block(if_nonfunction);
  HInstruction* call_result = Add<HCallFunction>(function, arg_count);
  Drop(arg_count);
  Push(call_result);
  Goto(if_nonfunction, join);

  set_current_block(join);
  join->SetJoinId(call->id());
  return ast_context()->ReturnValue(Pop());
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


void HOptimizedGraphBuilder::GenerateMathSin(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HCallStub* result = New<HCallStub>(CodeStub::TranscendentalCache, 1);
  result->set_transcendental_type(TranscendentalCache::SIN);
  Drop(1);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathCos(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HCallStub* result = New<HCallStub>(CodeStub::TranscendentalCache, 1);
  result->set_transcendental_type(TranscendentalCache::COS);
  Drop(1);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathTan(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HCallStub* result = New<HCallStub>(CodeStub::TranscendentalCache, 1);
  result->set_transcendental_type(TranscendentalCache::TAN);
  Drop(1);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathLog(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HCallStub* result = New<HCallStub>(CodeStub::TranscendentalCache, 1);
  result->set_transcendental_type(TranscendentalCache::LOG);
  Drop(1);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathSqrt(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HInstruction* result = New<HUnaryMathOperation>(value, kMathSqrt);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Check whether two RegExps are equivalent
void HOptimizedGraphBuilder::GenerateIsRegExpEquivalent(CallRuntime* call) {
  return Bailout(kInlinedRuntimeFunctionIsRegExpEquivalent);
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
    InliningKind inlining_kind,
    bool undefined_receiver) const {
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
  // If the function we are inlining is a strict mode function or a
  // builtin function, pass undefined as the receiver for function
  // calls (instead of the global receiver).
  if (undefined_receiver) {
    inner->SetValueAt(0, undefined);
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
  PrintF("%s", *trace.ToCString());
}


void HTracer::TraceCompilation(CompilationInfo* info) {
  Tag tag(this, "compilation");
  if (info->IsOptimizing()) {
    Handle<String> name = info->function()->debug_name();
    PrintStringProperty("name", *name->ToCString());
    PrintStringProperty("method", *name->ToCString());
  } else {
    CodeStub::Major major_key = info->code_stub()->MajorKey();
    PrintStringProperty("name", CodeStub::MajorName(major_key, false));
    PrintStringProperty("method", "stub");
  }
  PrintLongProperty("date", static_cast<int64_t>(OS::TimeCurrentMillis()));
}


void HTracer::TraceLithium(const char* name, LChunk* chunk) {
  ASSERT(!FLAG_concurrent_recompilation);
  AllowHandleDereference allow_deref;
  AllowDeferredHandleDereference allow_deferred_deref;
  Trace(name, chunk->graph(), chunk);
}


void HTracer::TraceHydrogen(const char* name, HGraph* graph) {
  ASSERT(!FLAG_concurrent_recompilation);
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
    const char* flags = current->IsLoopSuccessorDominator()
        ? "dom-loop-succ"
        : "";
    PrintStringProperty("flags", flags);

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
        int bci = 0;
        int uses = instruction->UseCount();
        PrintIndent();
        trace_.Add("%d %d ", bci, uses);
        instruction->PrintNameTo(&trace_);
        trace_.Add(" ");
        instruction->PrintTo(&trace_);
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
  AppendChars(filename_.start(), *trace_.ToCString(), trace_.length(), false);
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
