// Copyright 2012 the V8 project authors. All rights reserved.
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
#include "codegen.h"
#include "full-codegen.h"
#include "hashmap.h"
#include "hydrogen-environment-liveness.h"
#include "hydrogen-escape-analysis.h"
#include "hydrogen-infer-representation.h"
#include "hydrogen-gvn.h"
#include "hydrogen-osr.h"
#include "hydrogen-range-analysis.h"
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
      is_deoptimizing_(false),
      dominates_loop_successors_(false),
      is_osr_entry_(false) { }


Isolate* HBasicBlock::isolate() const {
  return graph_->isolate();
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


void HBasicBlock::AddInstruction(HInstruction* instr) {
  ASSERT(!IsStartBlock() || !IsFinished());
  ASSERT(!instr->IsLinked());
  ASSERT(!IsFinished());

  if (first_ == NULL) {
    ASSERT(last_environment() != NULL);
    ASSERT(!last_environment()->ast_id().IsNone());
    HBlockEntry* entry = new(zone()) HBlockEntry();
    entry->InitializeAsFirst(this);
    first_ = last_ = entry;
  }
  instr->InsertAfter(last_);
}


HDeoptimize* HBasicBlock::CreateDeoptimize(
    HDeoptimize::UseEnvironment has_uses) {
  ASSERT(HasEnvironment());
  if (has_uses == HDeoptimize::kNoUses)
    return new(zone()) HDeoptimize(0, 0, 0, zone());

  HEnvironment* environment = last_environment();
  int first_local_index = environment->first_local_index();
  int first_expression_index = environment->first_expression_index();
  HDeoptimize* instr = new(zone()) HDeoptimize(
      environment->length(), first_local_index, first_expression_index, zone());
  for (int i = 0; i < environment->length(); i++) {
    HValue* val = environment->values()->at(i);
    instr->AddEnvironmentValue(val, zone());
  }

  return instr;
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


void HBasicBlock::Finish(HControlInstruction* end) {
  ASSERT(!IsFinished());
  AddInstruction(end);
  end_ = end;
  for (HSuccessorIterator it(end); !it.Done(); it.Advance()) {
    it.Current()->RegisterPredecessor(this);
  }
}


void HBasicBlock::Goto(HBasicBlock* block,
                       FunctionState* state,
                       bool add_simulate) {
  bool drop_extra = state != NULL &&
      state->inlining_kind() == DROP_EXTRA_ON_RETURN;

  if (block->IsInlineReturnTarget()) {
    AddInstruction(new(zone()) HLeaveInlined());
    UpdateEnvironment(last_environment()->DiscardInlined(drop_extra));
  }

  if (add_simulate) AddSimulate(BailoutId::None());
  HGoto* instr = new(zone()) HGoto(block);
  Finish(instr);
}


void HBasicBlock::AddLeaveInlined(HValue* return_value,
                                  FunctionState* state) {
  HBasicBlock* target = state->function_return();
  bool drop_extra = state->inlining_kind() == DROP_EXTRA_ON_RETURN;

  ASSERT(target->IsInlineReturnTarget());
  ASSERT(return_value != NULL);
  AddInstruction(new(zone()) HLeaveInlined());
  UpdateEnvironment(last_environment()->DiscardInlined(drop_extra));
  last_environment()->Push(return_value);
  AddSimulate(BailoutId::None());
  HGoto* instr = new(zone()) HGoto(target);
  Finish(instr);
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
    HConstant* constant = new(zone()) HConstant(value);
    constant->InsertAfter(GetConstantUndefined());
    pointer->set(constant);
  }
  return pointer->get();
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
        isolate()->factory()->name##_value(),                                  \
        UniqueValueId(isolate()->heap()->name##_value()),                      \
        Representation::Tagged(),                                              \
        htype,                                                                 \
        false,                                                                 \
        true,                                                                  \
        boolean_value);                                                        \
    constant->InsertAfter(GetConstantUndefined());                             \
    constant_##name##_.set(constant);                                          \
  }                                                                            \
  return constant_##name##_.get();                                             \
}


DEFINE_GET_CONSTANT(True, true, HType::Boolean(), true)
DEFINE_GET_CONSTANT(False, false, HType::Boolean(), false)
DEFINE_GET_CONSTANT(Hole, the_hole, HType::Tagged(), false)
DEFINE_GET_CONSTANT(Null, null, HType::Tagged(), false)


#undef DEFINE_GET_CONSTANT


HConstant* HGraph::GetInvalidContext() {
  return GetConstant(&constant_invalid_context_, 0xFFFFC0C7);
}


HGraphBuilder::IfBuilder::IfBuilder(HGraphBuilder* builder, int position)
    : builder_(builder),
      position_(position),
      finished_(false),
      did_then_(false),
      did_else_(false),
      did_and_(false),
      did_or_(false),
      captured_(false),
      needs_compare_(true),
      split_edge_merge_block_(NULL) {
  HEnvironment* env = builder->environment();
  first_true_block_ = builder->CreateBasicBlock(env->Copy());
  last_true_block_ = NULL;
  first_false_block_ = builder->CreateBasicBlock(env->Copy());
}


HGraphBuilder::IfBuilder::IfBuilder(
    HGraphBuilder* builder,
    HIfContinuation* continuation)
    : builder_(builder),
      position_(RelocInfo::kNoPosition),
      finished_(false),
      did_then_(false),
      did_else_(false),
      did_and_(false),
      did_or_(false),
      captured_(false),
      needs_compare_(false),
      first_true_block_(NULL),
      first_false_block_(NULL),
      split_edge_merge_block_(NULL),
      merge_block_(NULL) {
  continuation->Continue(&first_true_block_,
                         &first_false_block_,
                         &position_);
}


HInstruction* HGraphBuilder::IfBuilder::IfCompare(
    HValue* left,
    HValue* right,
    Token::Value token) {
  HCompareIDAndBranch* compare =
      new(zone()) HCompareIDAndBranch(left, right, token);
  AddCompare(compare);
  return compare;
}


HInstruction* HGraphBuilder::IfBuilder::IfCompareMap(HValue* left,
                                                     Handle<Map> map) {
  HCompareMap* compare =
      new(zone()) HCompareMap(left, map, first_true_block_, first_false_block_);
  AddCompare(compare);
  return compare;
}


void HGraphBuilder::IfBuilder::AddCompare(HControlInstruction* compare) {
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
    split_edge->GotoNoSimulate(split_edge_merge_block_);
  } else {
    compare->SetSuccessorAt(0, first_true_block_);
    compare->SetSuccessorAt(1, first_false_block_);
  }
  builder_->current_block()->Finish(compare);
  needs_compare_ = false;
}


void HGraphBuilder::IfBuilder::Or() {
  ASSERT(!did_and_);
  did_or_ = true;
  HEnvironment* env = first_false_block_->last_environment();
  if (split_edge_merge_block_ == NULL) {
    split_edge_merge_block_ =
        builder_->CreateBasicBlock(env->Copy());
    first_true_block_->GotoNoSimulate(split_edge_merge_block_);
    first_true_block_ = split_edge_merge_block_;
  }
  builder_->set_current_block(first_false_block_);
  first_false_block_ = builder_->CreateBasicBlock(env->Copy());
}


void HGraphBuilder::IfBuilder::And() {
  ASSERT(!did_or_);
  did_and_ = true;
  HEnvironment* env = first_false_block_->last_environment();
  if (split_edge_merge_block_ == NULL) {
    split_edge_merge_block_ = builder_->CreateBasicBlock(env->Copy());
    first_false_block_->GotoNoSimulate(split_edge_merge_block_);
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
  continuation->Capture(true_block, false_block, position_);
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
    HBranch* branch =
        new(zone()) HBranch(constant_false, first_true_block_,
                            first_false_block_, boolean_type);
    builder_->current_block()->Finish(branch);
  }
  builder_->set_current_block(first_true_block_);
}


void HGraphBuilder::IfBuilder::Else() {
  ASSERT(did_then_);
  ASSERT(!captured_);
  ASSERT(!finished_);
  last_true_block_ = builder_->current_block();
  ASSERT(first_true_block_ == NULL || !last_true_block_->IsFinished());
  builder_->set_current_block(first_false_block_);
  did_else_ = true;
}


void HGraphBuilder::IfBuilder::Deopt() {
  HBasicBlock* block = builder_->current_block();
  block->FinishExitWithDeoptimization(HDeoptimize::kUseAll);
  builder_->set_current_block(NULL);
  if (did_else_) {
    first_false_block_ = NULL;
  } else {
    first_true_block_ = NULL;
  }
}


void HGraphBuilder::IfBuilder::Return(HValue* value) {
  HBasicBlock* block = builder_->current_block();
  HValue* context = builder_->environment()->LookupContext();
  HValue* parameter_count = builder_->graph()->GetConstantMinus1();
  block->FinishExit(new(zone()) HReturn(value, context, parameter_count));
  builder_->set_current_block(NULL);
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
    if (first_true_block_ == NULL) {
      // Deopt on true. Nothing to do, just continue the false block.
    } else if (first_false_block_ == NULL) {
      // Deopt on false. Nothing to do except switching to the true block.
      builder_->set_current_block(last_true_block_);
    } else {
      HEnvironment* merge_env = last_true_block_->last_environment()->Copy();
      merge_block_ = builder_->CreateBasicBlock(merge_env);
      ASSERT(!finished_);
      if (!did_else_) Else();
      ASSERT(!last_true_block_->IsFinished());
      HBasicBlock* last_false_block = builder_->current_block();
      ASSERT(!last_false_block->IsFinished());
      last_true_block_->GotoNoSimulate(merge_block_);
      last_false_block->GotoNoSimulate(merge_block_);
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
}


HValue* HGraphBuilder::LoopBuilder::BeginBody(
    HValue* initial,
    HValue* terminating,
    Token::Value token) {
  HEnvironment* env = builder_->environment();
  phi_ = new(zone()) HPhi(env->values()->length(), zone());
  header_block_->AddPhi(phi_);
  phi_->AddInput(initial);
  env->Push(initial);
  builder_->current_block()->GotoNoSimulate(header_block_);

  HEnvironment* body_env = env->Copy();
  HEnvironment* exit_env = env->Copy();
  body_block_ = builder_->CreateBasicBlock(body_env);
  exit_block_ = builder_->CreateBasicBlock(exit_env);
  // Remove the phi from the expression stack
  body_env->Pop();

  builder_->set_current_block(header_block_);
  HCompareIDAndBranch* compare =
      new(zone()) HCompareIDAndBranch(phi_, terminating, token);
  compare->SetSuccessorAt(0, body_block_);
  compare->SetSuccessorAt(1, exit_block_);
  builder_->current_block()->Finish(compare);

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


void HGraphBuilder::LoopBuilder::EndBody() {
  ASSERT(!finished_);

  if (direction_ == kPostIncrement || direction_ == kPostDecrement) {
    HValue* one = builder_->graph()->GetConstant1();
    if (direction_ == kPostIncrement) {
      increment_ = HAdd::New(zone(), context_, phi_, one);
    } else {
      increment_ = HSub::New(zone(), context_, phi_, one);
    }
    increment_->ClearFlag(HValue::kCanOverflow);
    builder_->AddInstruction(increment_);
  }

  // Push the new increment value on the expression stack to merge into the phi.
  builder_->environment()->Push(increment_);
  builder_->current_block()->GotoNoSimulate(header_block_);
  header_block_->loop_information()->RegisterBackEdge(body_block_);

  builder_->set_current_block(exit_block_);
  // Pop the phi from the expression stack
  builder_->environment()->Pop();
  finished_ = true;
}


HGraph* HGraphBuilder::CreateGraph() {
  graph_ = new(zone()) HGraph(info_);
  if (FLAG_hydrogen_stats) isolate()->GetHStatistics()->Initialize(info_);
  CompilationPhase phase("H_Block building", info_);
  set_current_block(graph()->entry_block());
  if (!BuildGraph()) return NULL;
  graph()->FinalizeUniqueValueIds();
  return graph_;
}


HInstruction* HGraphBuilder::AddInstruction(HInstruction* instr) {
  ASSERT(current_block() != NULL);
  current_block()->AddInstruction(instr);
  if (no_side_effects_scope_count_ > 0) {
    instr->SetFlag(HValue::kHasNoObservableSideEffects);
  }
  return instr;
}


void HGraphBuilder::AddSimulate(BailoutId id,
                                RemovableSimulate removable) {
  ASSERT(current_block() != NULL);
  ASSERT(no_side_effects_scope_count_ == 0);
  current_block()->AddSimulate(id, removable);
}


HReturn* HGraphBuilder::AddReturn(HValue* value) {
  HValue* context = environment()->LookupContext();
  int num_parameters = graph()->info()->num_parameters();
  HValue* params = Add<HConstant>(num_parameters);
  HReturn* return_instruction = new(graph()->zone())
      HReturn(value, context, params);
  current_block()->FinishExit(return_instruction);
  return return_instruction;
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


HValue* HGraphBuilder::BuildCheckMap(HValue* obj,
                                              Handle<Map> map) {
  HCheckMaps* check = HCheckMaps::New(obj, map, zone());
  AddInstruction(check);
  return check;
}


HInstruction* HGraphBuilder::BuildExternalArrayElementAccess(
    HValue* external_elements,
    HValue* checked_key,
    HValue* val,
    HValue* dependency,
    ElementsKind elements_kind,
    bool is_store) {
  Zone* zone = this->zone();
  if (is_store) {
    ASSERT(val != NULL);
    switch (elements_kind) {
      case EXTERNAL_PIXEL_ELEMENTS: {
        val = Add<HClampToUint8>(val);
        break;
      }
      case EXTERNAL_BYTE_ELEMENTS:
      case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      case EXTERNAL_SHORT_ELEMENTS:
      case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      case EXTERNAL_INT_ELEMENTS:
      case EXTERNAL_UNSIGNED_INT_ELEMENTS: {
        break;
      }
      case EXTERNAL_FLOAT_ELEMENTS:
      case EXTERNAL_DOUBLE_ELEMENTS:
        break;
      case FAST_SMI_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
      case DICTIONARY_ELEMENTS:
      case NON_STRICT_ARGUMENTS_ELEMENTS:
        UNREACHABLE();
        break;
    }
    return new(zone) HStoreKeyed(external_elements, checked_key,
                                 val, elements_kind);
  } else {
    ASSERT(val == NULL);
    HLoadKeyed* load =
       new(zone) HLoadKeyed(
           external_elements, checked_key, dependency, elements_kind);
    if (FLAG_opt_safe_uint32_operations &&
        elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS) {
      graph()->RecordUint32Instruction(load);
    }
    return load;
  }
}


HInstruction* HGraphBuilder::BuildFastElementAccess(
    HValue* elements,
    HValue* checked_key,
    HValue* val,
    HValue* load_dependency,
    ElementsKind elements_kind,
    bool is_store,
    LoadKeyedHoleMode load_mode,
    KeyedAccessStoreMode store_mode) {
  Zone* zone = this->zone();
  if (is_store) {
    ASSERT(val != NULL);
    switch (elements_kind) {
      case FAST_SMI_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
        return new(zone) HStoreKeyed(elements, checked_key, val, elements_kind);
      default:
        UNREACHABLE();
        return NULL;
    }
  }
  // It's an element load (!is_store).
  return new(zone) HLoadKeyed(elements,
                              checked_key,
                              load_dependency,
                              elements_kind,
                              load_mode);
}


HValue* HGraphBuilder::BuildCheckForCapacityGrow(HValue* object,
                                                 HValue* elements,
                                                 ElementsKind kind,
                                                 HValue* length,
                                                 HValue* key,
                                                 bool is_js_array) {
  Zone* zone = this->zone();
  IfBuilder length_checker(this);

  length_checker.IfCompare(length, key, Token::EQ);
  length_checker.Then();

  HValue* current_capacity = AddLoadFixedArrayLength(elements);

  IfBuilder capacity_checker(this);

  capacity_checker.IfCompare(length, current_capacity, Token::EQ);
  capacity_checker.Then();

  HValue* context = environment()->LookupContext();

  HValue* new_capacity =
      BuildNewElementsCapacity(context, current_capacity);

  HValue* new_elements = BuildGrowElementsCapacity(object, elements,
                                                   kind, length,
                                                   new_capacity);

  environment()->Push(new_elements);
  capacity_checker.Else();

  environment()->Push(elements);
  capacity_checker.End();

  if (is_js_array) {
    HValue* new_length = AddInstruction(
        HAdd::New(zone, context, length, graph_->GetConstant1()));
    new_length->ClearFlag(HValue::kCanOverflow);

    Representation representation = IsFastElementsKind(kind)
        ? Representation::Smi() : Representation::Tagged();
    AddStore(object, HObjectAccess::ForArrayLength(), new_length,
        representation);
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
  Heap* heap = isolate()->heap();

  IfBuilder cow_checker(this);

  cow_checker.IfCompareMap(elements,
                           Handle<Map>(heap->fixed_cow_array_map()));
  cow_checker.Then();

  HValue* capacity = AddLoadFixedArrayLength(elements);

  HValue* new_elements = BuildGrowElementsCapacity(object, elements,
                                                   kind, length, capacity);

  environment()->Push(new_elements);

  cow_checker.Else();

  environment()->Push(elements);

  cow_checker.End();

  return environment()->Pop();
}


HInstruction* HGraphBuilder::BuildUncheckedMonomorphicElementAccess(
    HValue* object,
    HValue* key,
    HValue* val,
    HCheckMaps* mapcheck,
    bool is_js_array,
    ElementsKind elements_kind,
    bool is_store,
    LoadKeyedHoleMode load_mode,
    KeyedAccessStoreMode store_mode) {
  ASSERT(!IsExternalArrayElementsKind(elements_kind) || !is_js_array);
  Zone* zone = this->zone();
  // No GVNFlag is necessary for ElementsKind if there is an explicit dependency
  // on a HElementsTransition instruction. The flag can also be removed if the
  // map to check has FAST_HOLEY_ELEMENTS, since there can be no further
  // ElementsKind transitions. Finally, the dependency can be removed for stores
  // for FAST_ELEMENTS, since a transition to HOLEY elements won't change the
  // generated store code.
  if ((elements_kind == FAST_HOLEY_ELEMENTS) ||
      (elements_kind == FAST_ELEMENTS && is_store)) {
    if (mapcheck != NULL) {
      mapcheck->ClearGVNFlag(kDependsOnElementsKind);
    }
  }
  bool fast_smi_only_elements = IsFastSmiElementsKind(elements_kind);
  bool fast_elements = IsFastObjectElementsKind(elements_kind);
  HValue* elements = AddLoadElements(object, mapcheck);
  if (is_store && (fast_elements || fast_smi_only_elements) &&
      store_mode != STORE_NO_TRANSITION_HANDLE_COW) {
    HCheckMaps* check_cow_map = HCheckMaps::New(
        elements, isolate()->factory()->fixed_array_map(), zone);
    check_cow_map->ClearGVNFlag(kDependsOnElementsKind);
    AddInstruction(check_cow_map);
  }
  HInstruction* length = NULL;
  if (is_js_array) {
    length = AddLoad(object, HObjectAccess::ForArrayLength(), mapcheck,
        Representation::Smi());
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
      length_checker.IfCompare(key, length, Token::LT);
      length_checker.Then();
      IfBuilder negative_checker(this);
      HValue* bounds_check = negative_checker.IfCompare(
          key, graph()->GetConstant0(), Token::GTE);
      negative_checker.Then();
      HInstruction* result = BuildExternalArrayElementAccess(
          external_elements, key, val, bounds_check,
          elements_kind, is_store);
      AddInstruction(result);
      negative_checker.ElseDeopt();
      length_checker.End();
      return result;
    } else {
      ASSERT(store_mode == STANDARD_STORE);
      checked_key = Add<HBoundsCheck>(key, length);
      HLoadExternalArrayPointer* external_elements =
          Add<HLoadExternalArrayPointer>(elements);
      return AddInstruction(BuildExternalArrayElementAccess(
          external_elements, checked_key, val, mapcheck,
          elements_kind, is_store));
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
    elements = BuildCheckForCapacityGrow(object, elements, elements_kind,
                                         length, key, is_js_array);
    checked_key = key;
  } else {
    checked_key = Add<HBoundsCheck>(key, length);

    if (is_store && (fast_elements || fast_smi_only_elements)) {
      if (store_mode == STORE_NO_TRANSITION_HANDLE_COW) {
        NoObservableSideEffectsScope no_effects(this);

        elements = BuildCopyElementsOnWrite(object, elements, elements_kind,
                                            length);
      } else {
        HCheckMaps* check_cow_map = HCheckMaps::New(
            elements, isolate()->factory()->fixed_array_map(), zone);
        check_cow_map->ClearGVNFlag(kDependsOnElementsKind);
        AddInstruction(check_cow_map);
      }
    }
  }
  return AddInstruction(
      BuildFastElementAccess(elements, checked_key, val, mapcheck,
                             elements_kind, is_store, load_mode, store_mode));
}


HValue* HGraphBuilder::BuildAllocateElements(HValue* context,
                                             ElementsKind kind,
                                             HValue* capacity) {
  Zone* zone = this->zone();

  int elements_size = IsFastDoubleElementsKind(kind)
      ? kDoubleSize : kPointerSize;
  HConstant* elements_size_value = Add<HConstant>(elements_size);
  HValue* mul = AddInstruction(
                    HMul::New(zone, context, capacity, elements_size_value));
  mul->ClearFlag(HValue::kCanOverflow);

  HConstant* header_size = Add<HConstant>(FixedArray::kHeaderSize);
  HValue* total_size = AddInstruction(
                           HAdd::New(zone, context, mul, header_size));
  total_size->ClearFlag(HValue::kCanOverflow);

  HAllocate::Flags flags = HAllocate::DefaultFlags(kind);
  if (isolate()->heap()->ShouldGloballyPretenure()) {
    // TODO(hpayer): When pretenuring can be internalized, flags can become
    // private to HAllocate.
    if (IsFastDoubleElementsKind(kind)) {
      flags = static_cast<HAllocate::Flags>(
          flags | HAllocate::CAN_ALLOCATE_IN_OLD_DATA_SPACE);
    } else {
      flags = static_cast<HAllocate::Flags>(
          flags | HAllocate::CAN_ALLOCATE_IN_OLD_POINTER_SPACE);
    }
  }

  return Add<HAllocate>(context, total_size, HType::JSArray(), flags);
}


void HGraphBuilder::BuildInitializeElementsHeader(HValue* elements,
                                                  ElementsKind kind,
                                                  HValue* capacity) {
  Factory* factory = isolate()->factory();
  Handle<Map> map = IsFastDoubleElementsKind(kind)
      ? factory->fixed_double_array_map()
      : factory->fixed_array_map();

  AddStoreMapConstant(elements, map);
  Representation representation = IsFastElementsKind(kind)
      ? Representation::Smi() : Representation::Tagged();
  AddStore(elements, HObjectAccess::ForFixedArrayLength(), capacity,
      representation);
}


HValue* HGraphBuilder::BuildAllocateElementsAndInitializeElementsHeader(
    HValue* context,
    ElementsKind kind,
    HValue* capacity) {
  HValue* new_elements = BuildAllocateElements(context, kind, capacity);
  BuildInitializeElementsHeader(new_elements, kind, capacity);
  return new_elements;
}


HInnerAllocatedObject* HGraphBuilder::BuildJSArrayHeader(HValue* array,
    HValue* array_map,
    AllocationSiteMode mode,
    HValue* allocation_site_payload,
    HValue* length_field) {

  AddStore(array, HObjectAccess::ForMap(), array_map);

  HConstant* empty_fixed_array =
      Add<HConstant>(isolate()->factory()->empty_fixed_array());

  HObjectAccess access = HObjectAccess::ForPropertiesPointer();
  AddStore(array, access, empty_fixed_array);
  AddStore(array, HObjectAccess::ForArrayLength(), length_field);

  if (mode == TRACK_ALLOCATION_SITE) {
    BuildCreateAllocationSiteInfo(array,
                                  JSArray::kSize,
                                  allocation_site_payload);
  }

  int elements_location = JSArray::kSize;
  if (mode == TRACK_ALLOCATION_SITE) {
    elements_location += AllocationSiteInfo::kSize;
  }

  HInnerAllocatedObject* elements =
      Add<HInnerAllocatedObject>(array, elements_location);
  AddStore(array, HObjectAccess::ForElementsPointer(), elements);
  return elements;
}


HLoadNamedField* HGraphBuilder::AddLoadElements(HValue* object,
                                                HValue* typecheck) {
  return AddLoad(object, HObjectAccess::ForElementsPointer(), typecheck);
}


HLoadNamedField* HGraphBuilder::AddLoadFixedArrayLength(HValue* object) {
  HLoadNamedField* instr = AddLoad(object, HObjectAccess::ForFixedArrayLength(),
                                   NULL, Representation::Smi());
  instr->set_type(HType::Smi());
  return instr;
}


HValue* HGraphBuilder::BuildNewElementsCapacity(HValue* context,
                                                HValue* old_capacity) {
  Zone* zone = this->zone();
  HValue* half_old_capacity =
      AddInstruction(HShr::New(zone, context, old_capacity,
                               graph_->GetConstant1()));
  half_old_capacity->ClearFlag(HValue::kCanOverflow);

  HValue* new_capacity = AddInstruction(
      HAdd::New(zone, context, half_old_capacity, old_capacity));
  new_capacity->ClearFlag(HValue::kCanOverflow);

  HValue* min_growth = Add<HConstant>(16);

  new_capacity = AddInstruction(
      HAdd::New(zone, context, new_capacity, min_growth));
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
  // Since we're forcing Integer32 representation for this HBoundsCheck,
  // there's no need to Smi-check the index.
  Add<HBoundsCheck>(length, max_size_constant);
}


HValue* HGraphBuilder::BuildGrowElementsCapacity(HValue* object,
                                                 HValue* elements,
                                                 ElementsKind kind,
                                                 HValue* length,
                                                 HValue* new_capacity) {
  HValue* context = environment()->LookupContext();

  BuildNewSpaceArrayCheck(new_capacity, kind);

  HValue* new_elements = BuildAllocateElementsAndInitializeElementsHeader(
      context, kind, new_capacity);

  BuildCopyElements(context, elements, kind,
                    new_elements, kind,
                    length, new_capacity);

  AddStore(object, HObjectAccess::ForElementsPointer(), new_elements);

  return new_elements;
}


void HGraphBuilder::BuildFillElementsWithHole(HValue* context,
                                              HValue* elements,
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
    LoopBuilder builder(this, context, LoopBuilder::kPostIncrement);

    HValue* key = builder.BeginBody(from, to, Token::LT);

    Add<HStoreKeyed>(elements, key, hole, elements_kind);

    builder.EndBody();
  }
}


void HGraphBuilder::BuildCopyElements(HValue* context,
                                      HValue* from_elements,
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
    BuildFillElementsWithHole(context, to_elements, to_elements_kind,
                              graph()->GetConstant0(), capacity);
  }

  LoopBuilder builder(this, context, LoopBuilder::kPostIncrement);

  HValue* key = builder.BeginBody(graph()->GetConstant0(), length, Token::LT);

  HValue* element = Add<HLoadKeyed>(from_elements, key,
                                    static_cast<HValue*>(NULL),
                                    from_elements_kind,
                                    ALLOW_RETURN_HOLE);

  ElementsKind holey_kind = IsFastSmiElementsKind(to_elements_kind)
      ? FAST_HOLEY_ELEMENTS : to_elements_kind;
  HInstruction* holey_store = Add<HStoreKeyed>(to_elements, key,
                                               element, holey_kind);
  // Allow NaN hole values to converted to their tagged counterparts.
  if (IsFastHoleyElementsKind(to_elements_kind)) {
    holey_store->SetFlag(HValue::kAllowUndefinedAsNaN);
  }

  builder.EndBody();

  if (!pre_fill_with_holes && length != capacity) {
    // Fill unused capacity with the hole.
    BuildFillElementsWithHole(context, to_elements, to_elements_kind,
                              key, capacity);
  }
}


HValue* HGraphBuilder::BuildCloneShallowArray(HContext* context,
                                              HValue* boilerplate,
                                              AllocationSiteMode mode,
                                              ElementsKind kind,
                                              int length) {
  NoObservableSideEffectsScope no_effects(this);

  // All sizes here are multiples of kPointerSize.
  int size = JSArray::kSize;
  if (mode == TRACK_ALLOCATION_SITE) {
    size += AllocationSiteInfo::kSize;
  }
  int elems_offset = size;
  if (length > 0) {
    size += IsFastDoubleElementsKind(kind)
        ? FixedDoubleArray::SizeFor(length)
        : FixedArray::SizeFor(length);
  }

  HAllocate::Flags allocate_flags = HAllocate::DefaultFlags(kind);
  // Allocate both the JS array and the elements array in one big
  // allocation. This avoids multiple limit checks.
  HValue* size_in_bytes = Add<HConstant>(size);
  HInstruction* object = Add<HAllocate>(context,
                                        size_in_bytes,
                                        HType::JSObject(),
                                        allocate_flags);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length == 0)) {
      HObjectAccess access = HObjectAccess::ForJSArrayOffset(i);
      AddStore(object, access, AddLoad(boilerplate, access));
    }
  }

  // Create an allocation site info if requested.
  if (mode == TRACK_ALLOCATION_SITE) {
    BuildCreateAllocationSiteInfo(object, JSArray::kSize, boilerplate);
  }

  if (length > 0) {
    // Get hold of the elements array of the boilerplate and setup the
    // elements pointer in the resulting object.
    HValue* boilerplate_elements = AddLoadElements(boilerplate);
    HValue* object_elements = Add<HInnerAllocatedObject>(object, elems_offset);
    AddStore(object, HObjectAccess::ForElementsPointer(), object_elements);

    // Copy the elements array header.
    for (int i = 0; i < FixedArrayBase::kHeaderSize; i += kPointerSize) {
      HObjectAccess access = HObjectAccess::ForFixedArrayHeader(i);
      AddStore(object_elements, access, AddLoad(boilerplate_elements, access));
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
    int position,
    HIfContinuation* continuation) {
  IfBuilder if_nil(this, position);
  bool needs_or = false;
  if (type->Maybe(Type::Null())) {
    if (needs_or) if_nil.Or();
    if_nil.If<HCompareObjectEqAndBranch>(value, graph()->GetConstantNull());
    needs_or = true;
  }
  if (type->Maybe(Type::Undefined())) {
    if (needs_or) if_nil.Or();
    if_nil.If<HCompareObjectEqAndBranch>(value,
                                         graph()->GetConstantUndefined());
    needs_or = true;
  }
  if (type->Maybe(Type::Undetectable())) {
    if (needs_or) if_nil.Or();
    if_nil.If<HIsUndetectableAndBranch>(value);
  } else {
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
      if_nil.Deopt();
    }
  }

  if_nil.CaptureContinuation(continuation);
}


HValue* HGraphBuilder::BuildCreateAllocationSiteInfo(HValue* previous_object,
                                                     int previous_object_size,
                                                     HValue* payload) {
  HInnerAllocatedObject* alloc_site = Add<HInnerAllocatedObject>(
      previous_object, previous_object_size);
  Handle<Map> alloc_site_map(isolate()->heap()->allocation_site_info_map());
  AddStoreMapConstant(alloc_site, alloc_site_map);
  HObjectAccess access = HObjectAccess::ForAllocationSitePayload();
  AddStore(alloc_site, access, payload);
  return alloc_site;
}


HInstruction* HGraphBuilder::BuildGetNativeContext(HValue* context) {
  // Get the global context, then the native context
  HInstruction* global_object = Add<HGlobalObject>(context);
  HObjectAccess access = HObjectAccess::ForJSObjectOffset(
      GlobalObject::kNativeContextOffset);
  return AddLoad(global_object, access);
}


HInstruction* HGraphBuilder::BuildGetArrayFunction(HValue* context) {
  HInstruction* native_context = BuildGetNativeContext(context);
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
      : AllocationSiteInfo::GetMode(kind);
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


HValue* HGraphBuilder::JSArrayBuilder::EmitMapCode(HValue* context) {
  if (kind_ == GetInitialFastElementsKind()) {
    // No need for a context lookup if the kind_ matches the initial
    // map, because we can just load the map in that case.
    HObjectAccess access = HObjectAccess::ForPrototypeOrInitialMap();
    HInstruction* load =
        builder()->BuildLoadNamedField(constructor_function_,
                                       access,
                                       Representation::Tagged());
    return builder()->AddInstruction(load);
  }

  HInstruction* native_context = builder()->BuildGetNativeContext(context);
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
  return builder()->AddInstruction(
      builder()->BuildLoadNamedField(constructor_function_,
                                     access,
                                     Representation::Tagged()));
}


HValue* HGraphBuilder::JSArrayBuilder::EstablishAllocationSize(
    HValue* length_node) {
  HValue* context = builder()->environment()->LookupContext();
  ASSERT(length_node != NULL);

  int base_size = JSArray::kSize;
  if (mode_ == TRACK_ALLOCATION_SITE) {
    base_size += AllocationSiteInfo::kSize;
  }

  if (IsFastDoubleElementsKind(kind_)) {
    base_size += FixedDoubleArray::kHeaderSize;
  } else {
    base_size += FixedArray::kHeaderSize;
  }

  HInstruction* elements_size_value =
      builder()->Add<HConstant>(elements_size());
  HInstruction* mul = HMul::New(zone(), context, length_node,
                                elements_size_value);
  mul->ClearFlag(HValue::kCanOverflow);
  builder()->AddInstruction(mul);

  HInstruction* base = builder()->Add<HConstant>(base_size);
  HInstruction* total_size = HAdd::New(zone(), context, base, mul);
  total_size->ClearFlag(HValue::kCanOverflow);
  builder()->AddInstruction(total_size);
  return total_size;
}


HValue* HGraphBuilder::JSArrayBuilder::EstablishEmptyArrayAllocationSize() {
  int base_size = JSArray::kSize;
  if (mode_ == TRACK_ALLOCATION_SITE) {
    base_size += AllocationSiteInfo::kSize;
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
  HValue* context = builder()->environment()->LookupContext();

  // Allocate (dealing with failure appropriately)
  HAllocate::Flags flags = HAllocate::DefaultFlags(kind_);
  HAllocate* new_object = builder()->Add<HAllocate>(context, size_in_bytes,
                                                    HType::JSArray(), flags);

  // Fill in the fields: map, properties, length
  HValue* map;
  if (allocation_site_payload_ == NULL) {
    map = EmitInternalMapCode();
  } else {
    map = EmitMapCode(context);
  }
  elements_location_ = builder()->BuildJSArrayHeader(new_object,
                                                     map,
                                                     mode_,
                                                     allocation_site_payload_,
                                                     length_field);

  // Initialize the elements
  builder()->BuildInitializeElementsHeader(elements_location_, kind_, capacity);

  if (fill_with_hole) {
    builder()->BuildFillElementsWithHole(context, elements_location_, kind_,
                                         graph()->GetConstant0(), capacity);
  }

  return new_object;
}


HStoreNamedField* HGraphBuilder::AddStore(HValue *object,
                                          HObjectAccess access,
                                          HValue *val,
                                          Representation representation) {
  return Add<HStoreNamedField>(object, access, val, representation);
}


HLoadNamedField* HGraphBuilder::AddLoad(HValue *object,
                                        HObjectAccess access,
                                        HValue *typecheck,
                                        Representation representation) {
  return Add<HLoadNamedField>(object, access, typecheck, representation);
}


HStoreNamedField* HGraphBuilder::AddStoreMapConstant(HValue *object,
                                                     Handle<Map> map) {
  return Add<HStoreNamedField>(object, HObjectAccess::ForMap(),
                               Add<HConstant>(map));
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
  InitializeAstVisitor();
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
    first->Goto(join_block);
    second->Goto(join_block);
    join_block->SetJoinId(join_id);
    return join_block;
  }
}


HBasicBlock* HOptimizedGraphBuilder::JoinContinue(IterationStatement* statement,
                                                  HBasicBlock* exit_block,
                                                  HBasicBlock* continue_block) {
  if (continue_block != NULL) {
    if (exit_block != NULL) exit_block->Goto(continue_block);
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
  if (body_exit != NULL) body_exit->Goto(loop_entry);
  loop_entry->PostProcessLoopHeader(statement);
  if (break_block != NULL) {
    if (loop_successor != NULL) loop_successor->Goto(break_block);
    break_block->SetJoinId(statement->ExitId());
    return break_block;
  }
  return loop_successor;
}


void HBasicBlock::FinishExit(HControlInstruction* instruction) {
  Finish(instruction);
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
      has_soft_deoptimize_(false),
      depends_on_empty_array_proto_elements_(false),
      type_change_checksum_(0),
      maximum_environment_size_(0) {
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


void HGraph::FinalizeUniqueValueIds() {
  DisallowHeapAllocation no_gc;
  ASSERT(!isolate()->optimizing_compiler_thread()->IsOptimizerThread());
  for (int i = 0; i < blocks()->length(); ++i) {
    for (HInstructionIterator it(blocks()->at(i)); !it.Done(); it.Advance()) {
      it.Current()->FinalizeUniqueValueId();
    }
  }
}


void HGraph::Canonicalize() {
  HPhase phase("H_Canonicalize", this);
  // Before removing no-op instructions, save their semantic value.
  // We must be careful not to set the flag unnecessarily, because GVN
  // cannot identify two instructions when their flag value differs.
  for (int i = 0; i < blocks()->length(); ++i) {
    for (HInstructionIterator it(blocks()->at(i)); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->IsArithmeticBinaryOperation() &&
          instr->representation().IsInteger32() &&
          instr->HasAtLeastOneUseWithFlagAndNoneWithout(
              HInstruction::kTruncatingToInt32)) {
        instr->SetFlag(HInstruction::kAllUsesTruncatingToInt32);
      }
    }
  }
  // Perform actual Canonicalization pass.
  for (int i = 0; i < blocks()->length(); ++i) {
    for (HInstructionIterator it(blocks()->at(i)); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      HValue* value = instr->Canonicalize();
      if (value != instr) instr->DeleteAndReplaceWith(value);
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


// Mark all blocks that are dominated by an unconditional soft deoptimize to
// prevent code motion across those blocks.
void HGraph::PropagateDeoptimizingMark() {
  HPhase phase("H_Propagate deoptimizing mark", this);
  // Skip this phase if there is nothing to be done anyway.
  if (!has_soft_deoptimize()) return;
  MarkAsDeoptimizingRecursively(entry_block());
  NullifyUnreachableInstructions();
}


void HGraph::MarkAsDeoptimizingRecursively(HBasicBlock* block) {
  for (int i = 0; i < block->dominated_blocks()->length(); ++i) {
    HBasicBlock* dominated = block->dominated_blocks()->at(i);
    if (block->IsDeoptimizing()) dominated->MarkAsDeoptimizing();
    MarkAsDeoptimizingRecursively(dominated);
  }
}


void HGraph::NullifyUnreachableInstructions() {
  if (!FLAG_unreachable_code_elimination) return;
  int block_count = blocks_.length();
  for (int i = 0; i < block_count; ++i) {
    HBasicBlock* block = blocks_.at(i);
    bool nullify = false;
    const ZoneList<HBasicBlock*>* predecessors = block->predecessors();
    int predecessors_length = predecessors->length();
    bool all_predecessors_deoptimizing = (predecessors_length > 0);
    for (int j = 0; j < predecessors_length; ++j) {
      if (!predecessors->at(j)->IsDeoptimizing()) {
        all_predecessors_deoptimizing = false;
        break;
      }
    }
    if (all_predecessors_deoptimizing) nullify = true;
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      // Leave the basic structure of the graph intact.
      if (instr->IsBlockEntry()) continue;
      if (instr->IsControlInstruction()) continue;
      if (instr->IsSimulate()) continue;
      if (instr->IsEnterInlined()) continue;
      if (instr->IsLeaveInlined()) continue;
      if (nullify) {
        HInstruction* last_dummy = NULL;
        for (int j = 0; j < instr->OperandCount(); ++j) {
          HValue* operand = instr->OperandAt(j);
          // Insert an HDummyUse for each operand, unless the operand
          // is an HDummyUse itself. If it's even from the same block,
          // remember it as a potential replacement for the instruction.
          if (operand->IsDummyUse()) {
            if (operand->block() == instr->block() &&
                last_dummy == NULL) {
              last_dummy = HInstruction::cast(operand);
            }
            continue;
          }
          if (operand->IsControlInstruction()) {
            // Inserting a dummy use for a value that's not defined anywhere
            // will fail. Some instructions define fake inputs on such
            // values as control flow dependencies.
            continue;
          }
          HDummyUse* dummy = new(zone()) HDummyUse(operand);
          dummy->InsertBefore(instr);
          last_dummy = dummy;
        }
        if (last_dummy == NULL) last_dummy = GetConstant1();
        instr->DeleteAndReplaceWith(last_dummy);
        continue;
      }
      if (instr->IsSoftDeoptimize()) {
        ASSERT(block->IsDeoptimizing());
        nullify = true;
      }
    }
  }
}


// Replace all phis consisting of a single non-loop operand plus any number of
// loop operands by that single non-loop operand.
void HGraph::EliminateRedundantPhis() {
  HPhase phase("H_Redundant phi elimination", this);

  // We do a simple fixed point iteration without any work list, because
  // machine-generated JavaScript can lead to a very dense Hydrogen graph with
  // an enormous work list and will consequently result in OOM. Experiments
  // showed that this simple algorithm is good enough, and even e.g. tracking
  // the set or range of blocks to consider is not a real improvement.
  bool need_another_iteration;
  ZoneList<HPhi*> redundant_phis(blocks_.length(), zone());
  do {
    need_another_iteration = false;
    for (int i = 0; i < blocks_.length(); ++i) {
      HBasicBlock* block = blocks_[i];
      for (int j = 0; j < block->phis()->length(); j++) {
        HPhi* phi = block->phis()->at(j);
        HValue* replacement = phi->GetRedundantReplacement();
        if (replacement != NULL) {
          // Remember phi to avoid concurrent modification of the block's phis.
          redundant_phis.Add(phi, zone());
          for (HUseIterator it(phi->uses()); !it.Done(); it.Advance()) {
            HValue* value = it.value();
            value->SetOperandAt(it.index(), replacement);
            need_another_iteration |= value->IsPhi();
          }
        }
      }
      for (int i = 0; i < redundant_phis.length(); i++) {
        block->RemovePhi(redundant_phis[i]);
      }
      redundant_phis.Clear();
    }
  } while (need_another_iteration);

#if DEBUG
  // Make sure that we *really* removed all redundant phis.
  for (int i = 0; i < blocks_.length(); ++i) {
    for (int j = 0; j < blocks_[i]->phis()->length(); j++) {
      ASSERT(blocks_[i]->phis()->at(j)->GetRedundantReplacement() == NULL);
    }
  }
#endif
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


void HGraph::InferTypes(ZoneList<HValue*>* worklist) {
  BitVector in_worklist(GetMaximumValueID(), zone());
  for (int i = 0; i < worklist->length(); ++i) {
    ASSERT(!in_worklist.Contains(worklist->at(i)->id()));
    in_worklist.Add(worklist->at(i)->id());
  }

  while (!worklist->is_empty()) {
    HValue* current = worklist->RemoveLast();
    in_worklist.Remove(current->id());
    if (current->UpdateInferredType()) {
      for (HUseIterator it(current->uses()); !it.Done(); it.Advance()) {
        HValue* use = it.value();
        if (!in_worklist.Contains(use->id())) {
          in_worklist.Add(use->id());
          worklist->Add(use, zone());
        }
      }
    }
  }
}


class HStackCheckEliminator BASE_EMBEDDED {
 public:
  explicit HStackCheckEliminator(HGraph* graph) : graph_(graph) { }

  void Process();

 private:
  HGraph* graph_;
};


void HStackCheckEliminator::Process() {
  HPhase phase("H_Stack check elimination", graph_);
  // For each loop block walk the dominator tree from the backwards branch to
  // the loop header. If a call instruction is encountered the backwards branch
  // is dominated by a call and the stack check in the backwards branch can be
  // removed.
  for (int i = 0; i < graph_->blocks()->length(); i++) {
    HBasicBlock* block = graph_->blocks()->at(i);
    if (block->IsLoopHeader()) {
      HBasicBlock* back_edge = block->loop_information()->GetLastBackEdge();
      HBasicBlock* dominator = back_edge;
      while (true) {
        for (HInstructionIterator it(dominator); !it.Done(); it.Advance()) {
          if (it.Current()->IsCall()) {
            block->loop_information()->stack_check()->Eliminate();
            break;
          }
        }

        // Done when the loop header is processed.
        if (dominator == block) break;

        // Move up the dominator tree.
        dominator = dominator->dominator();
      }
    }
  }
}


void HGraph::MergeRemovableSimulates() {
  HPhase phase("H_Merge removable simulates", this);
  ZoneList<HSimulate*> mergelist(2, zone());
  for (int i = 0; i < blocks()->length(); ++i) {
    HBasicBlock* block = blocks()->at(i);
    // Make sure the merge list is empty at the start of a block.
    ASSERT(mergelist.is_empty());
    // Nasty heuristic: Never remove the first simulate in a block. This
    // just so happens to have a beneficial effect on register allocation.
    bool first = true;
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* current = it.Current();
      if (current->IsLeaveInlined()) {
        // Never fold simulates from inlined environments into simulates
        // in the outer environment.
        // (Before each HEnterInlined, there is a non-foldable HSimulate
        // anyway, so we get the barrier in the other direction for free.)
        // Simply remove all accumulated simulates without merging.  This
        // is safe because simulates after instructions with side effects
        // are never added to the merge list.
        while (!mergelist.is_empty()) {
          mergelist.RemoveLast()->DeleteAndReplaceWith(NULL);
        }
        continue;
      }
      if (current->IsReturn()) {
        // Drop mergeable simulates in the list. This is safe because
        // simulates after instructions with side effects are never added
        // to the merge list.
        while (!mergelist.is_empty()) {
          mergelist.RemoveLast()->DeleteAndReplaceWith(NULL);
        }
        continue;
      }
      // Skip the non-simulates and the first simulate.
      if (!current->IsSimulate()) continue;
      if (first) {
        first = false;
        continue;
      }
      HSimulate* current_simulate = HSimulate::cast(current);
      if ((current_simulate->previous()->HasObservableSideEffects() &&
           !current_simulate->next()->IsSimulate()) ||
          !current_simulate->is_candidate_for_removal()) {
        // This simulate is not suitable for folding.
        // Fold the ones accumulated so far.
        current_simulate->MergeWith(&mergelist);
        continue;
      } else {
        // Accumulate this simulate for folding later on.
        mergelist.Add(current_simulate, zone());
      }
    }

    if (!mergelist.is_empty()) {
      // Merge the accumulated simulates at the end of the block.
      HSimulate* last = mergelist.RemoveLast();
      last->MergeWith(&mergelist);
    }
  }
}


void HGraph::InitializeInferredTypes() {
  HPhase phase("H_Inferring types", this);
  InitializeInferredTypes(0, this->blocks_.length() - 1);
}


void HGraph::InitializeInferredTypes(int from_inclusive, int to_inclusive) {
  for (int i = from_inclusive; i <= to_inclusive; ++i) {
    HBasicBlock* block = blocks_[i];

    const ZoneList<HPhi*>* phis = block->phis();
    for (int j = 0; j < phis->length(); j++) {
      phis->at(j)->UpdateInferredType();
    }

    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      it.Current()->UpdateInferredType();
    }

    if (block->IsLoopHeader()) {
      HBasicBlock* last_back_edge =
          block->loop_information()->GetLastBackEdge();
      InitializeInferredTypes(i + 1, last_back_edge->block_id());
      // Skip all blocks already processed by the recursive call.
      i = last_back_edge->block_id();
      // Update phis of the loop header now after the whole loop body is
      // guaranteed to be processed.
      ZoneList<HValue*> worklist(block->phis()->length(), zone());
      for (int j = 0; j < block->phis()->length(); ++j) {
        worklist.Add(block->phis()->at(j), zone());
      }
      InferTypes(&worklist);
    }
  }
}


void HGraph::PropagateMinusZeroChecks(HValue* value, BitVector* visited) {
  HValue* current = value;
  while (current != NULL) {
    if (visited->Contains(current->id())) return;

    // For phis, we must propagate the check to all of its inputs.
    if (current->IsPhi()) {
      visited->Add(current->id());
      HPhi* phi = HPhi::cast(current);
      for (int i = 0; i < phi->OperandCount(); ++i) {
        PropagateMinusZeroChecks(phi->OperandAt(i), visited);
      }
      break;
    }

    // For multiplication, division, and Math.min/max(), we must propagate
    // to the left and the right side.
    if (current->IsMul()) {
      HMul* mul = HMul::cast(current);
      mul->EnsureAndPropagateNotMinusZero(visited);
      PropagateMinusZeroChecks(mul->left(), visited);
      PropagateMinusZeroChecks(mul->right(), visited);
    } else if (current->IsDiv()) {
      HDiv* div = HDiv::cast(current);
      div->EnsureAndPropagateNotMinusZero(visited);
      PropagateMinusZeroChecks(div->left(), visited);
      PropagateMinusZeroChecks(div->right(), visited);
    } else if (current->IsMathMinMax()) {
      HMathMinMax* minmax = HMathMinMax::cast(current);
      visited->Add(minmax->id());
      PropagateMinusZeroChecks(minmax->left(), visited);
      PropagateMinusZeroChecks(minmax->right(), visited);
    }

    current = current->EnsureAndPropagateNotMinusZero(visited);
  }
}


void HGraph::InsertRepresentationChangeForUse(HValue* value,
                                              HValue* use_value,
                                              int use_index,
                                              Representation to) {
  // Insert the representation change right before its use. For phi-uses we
  // insert at the end of the corresponding predecessor.
  HInstruction* next = NULL;
  if (use_value->IsPhi()) {
    next = use_value->block()->predecessors()->at(use_index)->end();
  } else {
    next = HInstruction::cast(use_value);
  }
  // For constants we try to make the representation change at compile
  // time. When a representation change is not possible without loss of
  // information we treat constants like normal instructions and insert the
  // change instructions for them.
  HInstruction* new_value = NULL;
  bool is_truncating = use_value->CheckFlag(HValue::kTruncatingToInt32);
  bool allow_undefined_as_nan =
      use_value->CheckFlag(HValue::kAllowUndefinedAsNaN);
  if (value->IsConstant()) {
    HConstant* constant = HConstant::cast(value);
    // Try to create a new copy of the constant with the new representation.
    new_value = (is_truncating && to.IsInteger32())
        ? constant->CopyToTruncatedInt32(zone())
        : constant->CopyToRepresentation(to, zone());
  }

  if (new_value == NULL) {
    new_value = new(zone()) HChange(value, to,
                                    is_truncating, allow_undefined_as_nan);
  }

  new_value->InsertBefore(next);
  use_value->SetOperandAt(use_index, new_value);
}


void HGraph::InsertRepresentationChangesForValue(HValue* value) {
  Representation r = value->representation();
  if (r.IsNone()) return;
  if (value->HasNoUses()) return;

  for (HUseIterator it(value->uses()); !it.Done(); it.Advance()) {
    HValue* use_value = it.value();
    int use_index = it.index();
    Representation req = use_value->RequiredInputRepresentation(use_index);
    if (req.IsNone() || req.Equals(r)) continue;
    InsertRepresentationChangeForUse(value, use_value, use_index, req);
  }
  if (value->HasNoUses()) {
    ASSERT(value->IsConstant());
    value->DeleteAndReplaceWith(NULL);
  }

  // The only purpose of a HForceRepresentation is to represent the value
  // after the (possible) HChange instruction.  We make it disappear.
  if (value->IsForceRepresentation()) {
    value->DeleteAndReplaceWith(HForceRepresentation::cast(value)->value());
  }
}


void HGraph::InsertRepresentationChanges() {
  HPhase phase("H_Representation changes", this);

  // Compute truncation flag for phis: Initially assume that all
  // int32-phis allow truncation and iteratively remove the ones that
  // are used in an operation that does not allow a truncating
  // conversion.
  ZoneList<HPhi*> worklist(8, zone());

  for (int i = 0; i < phi_list()->length(); i++) {
    HPhi* phi = phi_list()->at(i);
    if (phi->representation().IsInteger32()) {
      phi->SetFlag(HValue::kTruncatingToInt32);
    }
  }

  for (int i = 0; i < phi_list()->length(); i++) {
    HPhi* phi = phi_list()->at(i);
    for (HUseIterator it(phi->uses()); !it.Done(); it.Advance()) {
      // If a Phi is used as a non-truncating int32 or as a double,
      // clear its "truncating" flag.
      HValue* use = it.value();
      Representation input_representation =
          use->RequiredInputRepresentation(it.index());
      if (!input_representation.IsInteger32() ||
          !use->CheckFlag(HValue::kTruncatingToInt32)) {
        if (FLAG_trace_representation) {
          PrintF("#%d Phi is not truncating because of #%d %s\n",
                 phi->id(), it.value()->id(), it.value()->Mnemonic());
        }
        phi->ClearFlag(HValue::kTruncatingToInt32);
        worklist.Add(phi, zone());
        break;
      }
    }
  }

  while (!worklist.is_empty()) {
    HPhi* current = worklist.RemoveLast();
    for (int i = 0; i < current->OperandCount(); ++i) {
      HValue* input = current->OperandAt(i);
      if (input->IsPhi() &&
          input->representation().IsInteger32() &&
          input->CheckFlag(HValue::kTruncatingToInt32)) {
        if (FLAG_trace_representation) {
          PrintF("#%d Phi is not truncating because of #%d %s\n",
                 input->id(), current->id(), current->Mnemonic());
        }
        input->ClearFlag(HValue::kTruncatingToInt32);
        worklist.Add(HPhi::cast(input), zone());
      }
    }
  }

  for (int i = 0; i < blocks_.length(); ++i) {
    // Process phi instructions first.
    const ZoneList<HPhi*>* phis = blocks_[i]->phis();
    for (int j = 0; j < phis->length(); j++) {
      InsertRepresentationChangesForValue(phis->at(j));
    }

    // Process normal instructions.
    HInstruction* current = blocks_[i]->first();
    while (current != NULL) {
      HInstruction* next = current->next();
      InsertRepresentationChangesForValue(current);
      current = next;
    }
  }
}


void HGraph::RecursivelyMarkPhiDeoptimizeOnUndefined(HPhi* phi) {
  if (!phi->CheckFlag(HValue::kAllowUndefinedAsNaN)) return;
  phi->ClearFlag(HValue::kAllowUndefinedAsNaN);
  for (int i = 0; i < phi->OperandCount(); ++i) {
    HValue* input = phi->OperandAt(i);
    if (input->IsPhi()) {
      RecursivelyMarkPhiDeoptimizeOnUndefined(HPhi::cast(input));
    }
  }
}


void HGraph::MarkDeoptimizeOnUndefined() {
  HPhase phase("H_MarkDeoptimizeOnUndefined", this);
  // Compute DeoptimizeOnUndefined flag for phis.
  // Any phi that can reach a use with DeoptimizeOnUndefined set must
  // have DeoptimizeOnUndefined set.  Currently only HCompareIDAndBranch, with
  // double input representation, has this flag set.
  // The flag is used by HChange tagged->double, which must deoptimize
  // if one of its uses has this flag set.
  for (int i = 0; i < phi_list()->length(); i++) {
    HPhi* phi = phi_list()->at(i);
    for (HUseIterator it(phi->uses()); !it.Done(); it.Advance()) {
      HValue* use_value = it.value();
      if (!use_value->CheckFlag(HValue::kAllowUndefinedAsNaN)) {
        RecursivelyMarkPhiDeoptimizeOnUndefined(phi);
        break;
      }
    }
  }
}


void HGraph::ComputeMinusZeroChecks() {
  HPhase phase("H_Compute minus zero checks", this);
  BitVector visited(GetMaximumValueID(), zone());
  for (int i = 0; i < blocks_.length(); ++i) {
    for (HInstructionIterator it(blocks_[i]); !it.Done(); it.Advance()) {
      HInstruction* current = it.Current();
      if (current->IsChange()) {
        HChange* change = HChange::cast(current);
        // Propagate flags for negative zero checks upwards from conversions
        // int32-to-tagged and int32-to-double.
        Representation from = change->value()->representation();
        ASSERT(from.Equals(change->from()));
        if (from.IsInteger32()) {
          ASSERT(change->to().IsTagged() ||
                 change->to().IsDouble() ||
                 change->to().IsSmi());
          ASSERT(visited.IsEmpty());
          PropagateMinusZeroChecks(change->value(), &visited);
          visited.Clear();
        }
      }
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
    owner()->Bailout("bad value context for arguments value");
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
    owner()->AddSimulate(ast_id, REMOVABLE_SIMULATE);
  }
}


void EffectContext::ReturnControl(HControlInstruction* instr,
                                  BailoutId ast_id) {
  ASSERT(!instr->HasObservableSideEffects());
  HBasicBlock* empty_true = owner()->graph()->CreateBasicBlock();
  HBasicBlock* empty_false = owner()->graph()->CreateBasicBlock();
  instr->SetSuccessorAt(0, empty_true);
  instr->SetSuccessorAt(1, empty_false);
  owner()->current_block()->Finish(instr);
  HBasicBlock* join = owner()->CreateJoin(empty_true, empty_false, ast_id);
  owner()->set_current_block(join);
}


void EffectContext::ReturnContinuation(HIfContinuation* continuation,
                                       BailoutId ast_id) {
  HBasicBlock* true_branch = NULL;
  HBasicBlock* false_branch = NULL;
  continuation->Continue(&true_branch, &false_branch, NULL);
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
    return owner()->Bailout("bad value context for arguments object value");
  }
  owner()->AddInstruction(instr);
  owner()->Push(instr);
  if (instr->HasObservableSideEffects()) {
    owner()->AddSimulate(ast_id, REMOVABLE_SIMULATE);
  }
}


void ValueContext::ReturnControl(HControlInstruction* instr, BailoutId ast_id) {
  ASSERT(!instr->HasObservableSideEffects());
  if (!arguments_allowed() && instr->CheckFlag(HValue::kIsArguments)) {
    return owner()->Bailout("bad value context for arguments object value");
  }
  HBasicBlock* materialize_false = owner()->graph()->CreateBasicBlock();
  HBasicBlock* materialize_true = owner()->graph()->CreateBasicBlock();
  instr->SetSuccessorAt(0, materialize_true);
  instr->SetSuccessorAt(1, materialize_false);
  owner()->current_block()->Finish(instr);
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
  continuation->Continue(&materialize_true, &materialize_false, NULL);
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
    builder->AddSimulate(ast_id, REMOVABLE_SIMULATE);
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
  owner()->current_block()->Finish(instr);
  empty_true->Goto(if_true(), owner()->function_state());
  empty_false->Goto(if_false(), owner()->function_state());
  owner()->set_current_block(NULL);
}


void TestContext::ReturnContinuation(HIfContinuation* continuation,
                                     BailoutId ast_id) {
  HBasicBlock* true_branch = NULL;
  HBasicBlock* false_branch = NULL;
  continuation->Continue(&true_branch, &false_branch, NULL);
  if (continuation->IsTrueReachable()) {
    true_branch->Goto(if_true(), owner()->function_state());
  }
  if (continuation->IsFalseReachable()) {
    false_branch->Goto(if_false(), owner()->function_state());
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
    builder->Bailout("arguments object value in a test context");
  }
  if (value->IsConstant()) {
    HConstant* constant_value = HConstant::cast(value);
    if (constant_value->BooleanValue()) {
      builder->current_block()->Goto(if_true(), builder->function_state());
    } else {
      builder->current_block()->Goto(if_false(), builder->function_state());
    }
    builder->set_current_block(NULL);
    return;
  }
  HBasicBlock* empty_true = builder->graph()->CreateBasicBlock();
  HBasicBlock* empty_false = builder->graph()->CreateBasicBlock();
  ToBooleanStub::Types expected(condition()->to_boolean_types());
  HBranch* test = new(zone()) HBranch(value, empty_true, empty_false, expected);
  builder->current_block()->Finish(test);

  empty_true->Goto(if_true(), builder->function_state());
  empty_false->Goto(if_false(), builder->function_state());
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


void HOptimizedGraphBuilder::Bailout(const char* reason) {
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
    Bailout("function is a generator");
    return false;
  }
  Scope* scope = current_info()->scope();
  if (scope->HasIllegalRedeclaration()) {
    Bailout("function with illegal redeclaration");
    return false;
  }
  if (scope->calls_eval()) {
    Bailout("function calls eval");
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
  current_block()->Goto(body_entry);
  body_entry->SetJoinId(BailoutId::FunctionEntry());
  set_current_block(body_entry);

  // Handle implicit declaration of the function name in named function
  // expressions before other declarations.
  if (scope->is_function_scope() && scope->function() != NULL) {
    VisitVariableDeclaration(scope->function());
  }
  VisitDeclarations(scope->declarations());
  AddSimulate(BailoutId::Declarations());

  HValue* context = environment()->LookupContext();
  Add<HStackCheck>(context, HStackCheck::kFunctionEntry);

  VisitStatements(current_info()->function()->body());
  if (HasStackOverflow()) return false;

  if (current_block() != NULL) {
    AddReturn(graph()->GetConstantUndefined());
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
  osr_->FinishGraph();

  return true;
}


bool HGraph::Optimize(SmartArrayPointer<char>* bailout_reason) {
  *bailout_reason = SmartArrayPointer<char>();
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

  PropagateDeoptimizingMark();
  if (!CheckConstPhiUses()) {
    *bailout_reason = SmartArrayPointer<char>(StrDup(
        "Unsupported phi use of const variable"));
    return false;
  }
  EliminateRedundantPhis();
  if (!CheckArgumentsPhiUses()) {
    *bailout_reason = SmartArrayPointer<char>(StrDup(
        "Unsupported phi use of arguments"));
    return false;
  }

  // Remove dead code and phis
  if (FLAG_dead_code_elimination) {
    DeadCodeElimination("H_Eliminate early dead code");
  }
  CollectPhis();

  if (has_osr()) osr()->FinishOsrValues();

  Run<HInferRepresentationPhase>();

  // Remove HSimulate instructions that have turned out not to be needed
  // after all by folding them into the following HSimulate.
  // This must happen after inferring representations.
  MergeRemovableSimulates();

  MarkDeoptimizeOnUndefined();
  InsertRepresentationChanges();

  InitializeInferredTypes();

  // Must be performed before canonicalization to ensure that Canonicalize
  // will not remove semantically meaningful ToInt32 operations e.g. BIT_OR with
  // zero.
  if (FLAG_opt_safe_uint32_operations) Run<HUint32AnalysisPhase>();

  if (FLAG_use_canonicalizing) Canonicalize();

  if (FLAG_use_escape_analysis) Run<HEscapeAnalysisPhase>();

  if (FLAG_use_gvn) Run<HGlobalValueNumberingPhase>();

  if (FLAG_use_range) Run<HRangeAnalysisPhase>();

  ComputeMinusZeroChecks();

  // Eliminate redundant stack checks on backwards branches.
  HStackCheckEliminator sce(this);
  sce.Process();

  if (FLAG_idefs) SetupInformativeDefinitions();
  if (FLAG_array_bounds_checks_elimination && !FLAG_idefs) {
    EliminateRedundantBoundsChecks();
  }
  if (FLAG_array_index_dehoisting) DehoistSimpleArrayIndexComputations();
  if (FLAG_dead_code_elimination) {
    DeadCodeElimination("H_Eliminate late dead code");
  }

  RestoreActualValues();

  return true;
}


void HGraph::SetupInformativeDefinitionsInBlock(HBasicBlock* block) {
  for (int phi_index = 0; phi_index < block->phis()->length(); phi_index++) {
    HPhi* phi = block->phis()->at(phi_index);
    phi->AddInformativeDefinitions();
    phi->SetFlag(HValue::kIDefsProcessingDone);
    // We do not support phis that "redefine just one operand".
    ASSERT(!phi->IsInformativeDefinition());
  }

  for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
    HInstruction* i = it.Current();
    i->AddInformativeDefinitions();
    i->SetFlag(HValue::kIDefsProcessingDone);
    i->UpdateRedefinedUsesWhileSettingUpInformativeDefinitions();
  }
}


// This method is recursive, so if its stack frame is large it could
// cause a stack overflow.
// To keep the individual stack frames small we do the actual work inside
// SetupInformativeDefinitionsInBlock();
void HGraph::SetupInformativeDefinitionsRecursively(HBasicBlock* block) {
  SetupInformativeDefinitionsInBlock(block);
  for (int i = 0; i < block->dominated_blocks()->length(); ++i) {
    SetupInformativeDefinitionsRecursively(block->dominated_blocks()->at(i));
  }

  for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
    HInstruction* i = it.Current();
    if (i->IsBoundsCheck()) {
      HBoundsCheck* check = HBoundsCheck::cast(i);
      check->ApplyIndexChange();
    }
  }
}


void HGraph::SetupInformativeDefinitions() {
  HPhase phase("H_Setup informative definitions", this);
  SetupInformativeDefinitionsRecursively(entry_block());
}


// We try to "factor up" HBoundsCheck instructions towards the root of the
// dominator tree.
// For now we handle checks where the index is like "exp + int32value".
// If in the dominator tree we check "exp + v1" and later (dominated)
// "exp + v2", if v2 <= v1 we can safely remove the second check, and if
// v2 > v1 we can use v2 in the 1st check and again remove the second.
// To do so we keep a dictionary of all checks where the key if the pair
// "exp, length".
// The class BoundsCheckKey represents this key.
class BoundsCheckKey : public ZoneObject {
 public:
  HValue* IndexBase() const { return index_base_; }
  HValue* Length() const { return length_; }

  uint32_t Hash() {
    return static_cast<uint32_t>(index_base_->Hashcode() ^ length_->Hashcode());
  }

  static BoundsCheckKey* Create(Zone* zone,
                                HBoundsCheck* check,
                                int32_t* offset) {
    if (!check->index()->representation().IsSmiOrInteger32()) return NULL;

    HValue* index_base = NULL;
    HConstant* constant = NULL;
    bool is_sub = false;

    if (check->index()->IsAdd()) {
      HAdd* index = HAdd::cast(check->index());
      if (index->left()->IsConstant()) {
        constant = HConstant::cast(index->left());
        index_base = index->right();
      } else if (index->right()->IsConstant()) {
        constant = HConstant::cast(index->right());
        index_base = index->left();
      }
    } else if (check->index()->IsSub()) {
      HSub* index = HSub::cast(check->index());
      is_sub = true;
      if (index->left()->IsConstant()) {
        constant = HConstant::cast(index->left());
        index_base = index->right();
      } else if (index->right()->IsConstant()) {
        constant = HConstant::cast(index->right());
        index_base = index->left();
      }
    }

    if (constant != NULL && constant->HasInteger32Value()) {
      *offset = is_sub ? - constant->Integer32Value()
                       : constant->Integer32Value();
    } else {
      *offset = 0;
      index_base = check->index();
    }

    return new(zone) BoundsCheckKey(index_base, check->length());
  }

 private:
  BoundsCheckKey(HValue* index_base, HValue* length)
    : index_base_(index_base),
      length_(length) { }

  HValue* index_base_;
  HValue* length_;
};


// Data about each HBoundsCheck that can be eliminated or moved.
// It is the "value" in the dictionary indexed by "base-index, length"
// (the key is BoundsCheckKey).
// We scan the code with a dominator tree traversal.
// Traversing the dominator tree we keep a stack (implemented as a singly
// linked list) of "data" for each basic block that contains a relevant check
// with the same key (the dictionary holds the head of the list).
// We also keep all the "data" created for a given basic block in a list, and
// use it to "clean up" the dictionary when backtracking in the dominator tree
// traversal.
// Doing this each dictionary entry always directly points to the check that
// is dominating the code being examined now.
// We also track the current "offset" of the index expression and use it to
// decide if any check is already "covered" (so it can be removed) or not.
class BoundsCheckBbData: public ZoneObject {
 public:
  BoundsCheckKey* Key() const { return key_; }
  int32_t LowerOffset() const { return lower_offset_; }
  int32_t UpperOffset() const { return upper_offset_; }
  HBasicBlock* BasicBlock() const { return basic_block_; }
  HBoundsCheck* LowerCheck() const { return lower_check_; }
  HBoundsCheck* UpperCheck() const { return upper_check_; }
  BoundsCheckBbData* NextInBasicBlock() const { return next_in_bb_; }
  BoundsCheckBbData* FatherInDominatorTree() const { return father_in_dt_; }

  bool OffsetIsCovered(int32_t offset) const {
    return offset >= LowerOffset() && offset <= UpperOffset();
  }

  bool HasSingleCheck() { return lower_check_ == upper_check_; }

  // The goal of this method is to modify either upper_offset_ or
  // lower_offset_ so that also new_offset is covered (the covered
  // range grows).
  //
  // The precondition is that new_check follows UpperCheck() and
  // LowerCheck() in the same basic block, and that new_offset is not
  // covered (otherwise we could simply remove new_check).
  //
  // If HasSingleCheck() is true then new_check is added as "second check"
  // (either upper or lower; note that HasSingleCheck() becomes false).
  // Otherwise one of the current checks is modified so that it also covers
  // new_offset, and new_check is removed.
  //
  // If the check cannot be modified because the context is unknown it
  // returns false, otherwise it returns true.
  bool CoverCheck(HBoundsCheck* new_check,
                  int32_t new_offset) {
    ASSERT(new_check->index()->representation().IsSmiOrInteger32());
    bool keep_new_check = false;

    if (new_offset > upper_offset_) {
      upper_offset_ = new_offset;
      if (HasSingleCheck()) {
        keep_new_check = true;
        upper_check_ = new_check;
      } else {
        bool result = BuildOffsetAdd(upper_check_,
                                     &added_upper_index_,
                                     &added_upper_offset_,
                                     Key()->IndexBase(),
                                     new_check->index()->representation(),
                                     new_offset);
        if (!result) return false;
        upper_check_->ReplaceAllUsesWith(upper_check_->index());
        upper_check_->SetOperandAt(0, added_upper_index_);
      }
    } else if (new_offset < lower_offset_) {
      lower_offset_ = new_offset;
      if (HasSingleCheck()) {
        keep_new_check = true;
        lower_check_ = new_check;
      } else {
        bool result = BuildOffsetAdd(lower_check_,
                                     &added_lower_index_,
                                     &added_lower_offset_,
                                     Key()->IndexBase(),
                                     new_check->index()->representation(),
                                     new_offset);
        if (!result) return false;
        lower_check_->ReplaceAllUsesWith(lower_check_->index());
        lower_check_->SetOperandAt(0, added_lower_index_);
      }
    } else {
      ASSERT(false);
    }

    if (!keep_new_check) {
      new_check->DeleteAndReplaceWith(new_check->ActualValue());
    }

    return true;
  }

  void RemoveZeroOperations() {
    RemoveZeroAdd(&added_lower_index_, &added_lower_offset_);
    RemoveZeroAdd(&added_upper_index_, &added_upper_offset_);
  }

  BoundsCheckBbData(BoundsCheckKey* key,
                    int32_t lower_offset,
                    int32_t upper_offset,
                    HBasicBlock* bb,
                    HBoundsCheck* lower_check,
                    HBoundsCheck* upper_check,
                    BoundsCheckBbData* next_in_bb,
                    BoundsCheckBbData* father_in_dt)
  : key_(key),
    lower_offset_(lower_offset),
    upper_offset_(upper_offset),
    basic_block_(bb),
    lower_check_(lower_check),
    upper_check_(upper_check),
    added_lower_index_(NULL),
    added_lower_offset_(NULL),
    added_upper_index_(NULL),
    added_upper_offset_(NULL),
    next_in_bb_(next_in_bb),
    father_in_dt_(father_in_dt) { }

 private:
  BoundsCheckKey* key_;
  int32_t lower_offset_;
  int32_t upper_offset_;
  HBasicBlock* basic_block_;
  HBoundsCheck* lower_check_;
  HBoundsCheck* upper_check_;
  HInstruction* added_lower_index_;
  HConstant* added_lower_offset_;
  HInstruction* added_upper_index_;
  HConstant* added_upper_offset_;
  BoundsCheckBbData* next_in_bb_;
  BoundsCheckBbData* father_in_dt_;

  // Given an existing add instruction and a bounds check it tries to
  // find the current context (either of the add or of the check index).
  HValue* IndexContext(HInstruction* add, HBoundsCheck* check) {
    if (add != NULL && add->IsAdd()) {
      return HAdd::cast(add)->context();
    }
    if (check->index()->IsBinaryOperation()) {
      return HBinaryOperation::cast(check->index())->context();
    }
    return NULL;
  }

  // This function returns false if it cannot build the add because the
  // current context cannot be determined.
  bool BuildOffsetAdd(HBoundsCheck* check,
                      HInstruction** add,
                      HConstant** constant,
                      HValue* original_value,
                      Representation representation,
                      int32_t new_offset) {
    HValue* index_context = IndexContext(*add, check);
    if (index_context == NULL) return false;

    HConstant* new_constant = new(BasicBlock()->zone()) HConstant(
        new_offset, representation);
    if (*add == NULL) {
      new_constant->InsertBefore(check);
      (*add) = HAdd::New(
          BasicBlock()->zone(), index_context, original_value, new_constant);
      (*add)->AssumeRepresentation(representation);
      (*add)->InsertBefore(check);
    } else {
      new_constant->InsertBefore(*add);
      (*constant)->DeleteAndReplaceWith(new_constant);
    }
    *constant = new_constant;
    return true;
  }

  void RemoveZeroAdd(HInstruction** add, HConstant** constant) {
    if (*add != NULL && (*add)->IsAdd() && (*constant)->Integer32Value() == 0) {
      (*add)->DeleteAndReplaceWith(HAdd::cast(*add)->left());
      (*constant)->DeleteAndReplaceWith(NULL);
    }
  }
};


static bool BoundsCheckKeyMatch(void* key1, void* key2) {
  BoundsCheckKey* k1 = static_cast<BoundsCheckKey*>(key1);
  BoundsCheckKey* k2 = static_cast<BoundsCheckKey*>(key2);
  return k1->IndexBase() == k2->IndexBase() && k1->Length() == k2->Length();
}


class BoundsCheckTable : private ZoneHashMap {
 public:
  BoundsCheckBbData** LookupOrInsert(BoundsCheckKey* key, Zone* zone) {
    return reinterpret_cast<BoundsCheckBbData**>(
        &(Lookup(key, key->Hash(), true, ZoneAllocationPolicy(zone))->value));
  }

  void Insert(BoundsCheckKey* key, BoundsCheckBbData* data, Zone* zone) {
    Lookup(key, key->Hash(), true, ZoneAllocationPolicy(zone))->value = data;
  }

  void Delete(BoundsCheckKey* key) {
    Remove(key, key->Hash());
  }

  explicit BoundsCheckTable(Zone* zone)
      : ZoneHashMap(BoundsCheckKeyMatch, ZoneHashMap::kDefaultHashMapCapacity,
                    ZoneAllocationPolicy(zone)) { }
};


// Eliminates checks in bb and recursively in the dominated blocks.
// Also replace the results of check instructions with the original value, if
// the result is used. This is safe now, since we don't do code motion after
// this point. It enables better register allocation since the value produced
// by check instructions is really a copy of the original value.
void HGraph::EliminateRedundantBoundsChecks(HBasicBlock* bb,
                                            BoundsCheckTable* table) {
  BoundsCheckBbData* bb_data_list = NULL;

  for (HInstructionIterator it(bb); !it.Done(); it.Advance()) {
    HInstruction* i = it.Current();
    if (!i->IsBoundsCheck()) continue;

    HBoundsCheck* check = HBoundsCheck::cast(i);
    int32_t offset;
    BoundsCheckKey* key =
        BoundsCheckKey::Create(zone(), check, &offset);
    if (key == NULL) continue;
    BoundsCheckBbData** data_p = table->LookupOrInsert(key, zone());
    BoundsCheckBbData* data = *data_p;
    if (data == NULL) {
      bb_data_list = new(zone()) BoundsCheckBbData(key,
                                                   offset,
                                                   offset,
                                                   bb,
                                                   check,
                                                   check,
                                                   bb_data_list,
                                                   NULL);
      *data_p = bb_data_list;
    } else if (data->OffsetIsCovered(offset)) {
      check->DeleteAndReplaceWith(check->ActualValue());
    } else if (data->BasicBlock() != bb ||
               !data->CoverCheck(check, offset)) {
      // If the check is in the current BB we try to modify it by calling
      // "CoverCheck", but if also that fails we record the current offsets
      // in a new data instance because from now on they are covered.
      int32_t new_lower_offset = offset < data->LowerOffset()
          ? offset
          : data->LowerOffset();
      int32_t new_upper_offset = offset > data->UpperOffset()
          ? offset
          : data->UpperOffset();
      bb_data_list = new(zone()) BoundsCheckBbData(key,
                                                   new_lower_offset,
                                                   new_upper_offset,
                                                   bb,
                                                   data->LowerCheck(),
                                                   data->UpperCheck(),
                                                   bb_data_list,
                                                   data);
      table->Insert(key, bb_data_list, zone());
    }
  }

  for (int i = 0; i < bb->dominated_blocks()->length(); ++i) {
    EliminateRedundantBoundsChecks(bb->dominated_blocks()->at(i), table);
  }

  for (BoundsCheckBbData* data = bb_data_list;
       data != NULL;
       data = data->NextInBasicBlock()) {
    data->RemoveZeroOperations();
    if (data->FatherInDominatorTree()) {
      table->Insert(data->Key(), data->FatherInDominatorTree(), zone());
    } else {
      table->Delete(data->Key());
    }
  }
}


void HGraph::EliminateRedundantBoundsChecks() {
  HPhase phase("H_Eliminate bounds checks", this);
  BoundsCheckTable checks_table(zone());
  EliminateRedundantBoundsChecks(entry_block(), &checks_table);
}


static void DehoistArrayIndex(ArrayInstructionInterface* array_operation) {
  HValue* index = array_operation->GetKey()->ActualValue();
  if (!index->representation().IsSmiOrInteger32()) return;

  HConstant* constant;
  HValue* subexpression;
  int32_t sign;
  if (index->IsAdd()) {
    sign = 1;
    HAdd* add = HAdd::cast(index);
    if (add->left()->IsConstant()) {
      subexpression = add->right();
      constant = HConstant::cast(add->left());
    } else if (add->right()->IsConstant()) {
      subexpression = add->left();
      constant = HConstant::cast(add->right());
    } else {
      return;
    }
  } else if (index->IsSub()) {
    sign = -1;
    HSub* sub = HSub::cast(index);
    if (sub->left()->IsConstant()) {
      subexpression = sub->right();
      constant = HConstant::cast(sub->left());
    } else if (sub->right()->IsConstant()) {
      subexpression = sub->left();
      constant = HConstant::cast(sub->right());
    } else {
      return;
    }
  } else {
    return;
  }

  if (!constant->HasInteger32Value()) return;
  int32_t value = constant->Integer32Value() * sign;
  // We limit offset values to 30 bits because we want to avoid the risk of
  // overflows when the offset is added to the object header size.
  if (value >= 1 << 30 || value < 0) return;
  array_operation->SetKey(subexpression);
  if (index->HasNoUses()) {
    index->DeleteAndReplaceWith(NULL);
  }
  ASSERT(value >= 0);
  array_operation->SetIndexOffset(static_cast<uint32_t>(value));
  array_operation->SetDehoisted(true);
}


void HGraph::DehoistSimpleArrayIndexComputations() {
  HPhase phase("H_Dehoist index computations", this);
  for (int i = 0; i < blocks()->length(); ++i) {
    for (HInstructionIterator it(blocks()->at(i)); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      ArrayInstructionInterface* array_instruction = NULL;
      if (instr->IsLoadKeyed()) {
        HLoadKeyed* op = HLoadKeyed::cast(instr);
        array_instruction = static_cast<ArrayInstructionInterface*>(op);
      } else if (instr->IsStoreKeyed()) {
        HStoreKeyed* op = HStoreKeyed::cast(instr);
        array_instruction = static_cast<ArrayInstructionInterface*>(op);
      } else {
        continue;
      }
      DehoistArrayIndex(array_instruction);
    }
  }
}


void HGraph::DeadCodeElimination(const char* phase_name) {
  HPhase phase(phase_name, this);
  MarkLiveInstructions();
  RemoveDeadInstructions();
}


void HGraph::MarkLiveInstructions() {
  ZoneList<HValue*> worklist(blocks_.length(), zone());

  // Mark initial root instructions for dead code elimination.
  for (int i = 0; i < blocks()->length(); ++i) {
    HBasicBlock* block = blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->CannotBeEliminated()) MarkLive(NULL, instr, &worklist);
    }
    for (int j = 0; j < block->phis()->length(); j++) {
      HPhi* phi = block->phis()->at(j);
      if (phi->CannotBeEliminated()) MarkLive(NULL, phi, &worklist);
    }
  }

  // Transitively mark all inputs of live instructions live.
  while (!worklist.is_empty()) {
    HValue* instr = worklist.RemoveLast();
    for (int i = 0; i < instr->OperandCount(); ++i) {
      MarkLive(instr, instr->OperandAt(i), &worklist);
    }
  }
}


void HGraph::MarkLive(HValue* ref, HValue* instr, ZoneList<HValue*>* worklist) {
  if (!instr->CheckFlag(HValue::kIsLive)) {
    instr->SetFlag(HValue::kIsLive);
    worklist->Add(instr, zone());

    if (FLAG_trace_dead_code_elimination) {
      HeapStringAllocator allocator;
      StringStream stream(&allocator);
      if (ref != NULL) {
        ref->PrintTo(&stream);
      } else {
        stream.Add("root ");
      }
      stream.Add(" -> ");
      instr->PrintTo(&stream);
      PrintF("[MarkLive %s]\n", *stream.ToCString());
    }
  }
}


void HGraph::RemoveDeadInstructions() {
  ZoneList<HPhi*> dead_phis(blocks_.length(), zone());

  // Remove any instruction not marked kIsLive.
  for (int i = 0; i < blocks()->length(); ++i) {
    HBasicBlock* block = blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (!instr->CheckFlag(HValue::kIsLive)) {
        // Instruction has not been marked live; assume it is dead and remove.
        // TODO(titzer): we don't remove constants because some special ones
        // might be used by later phases and are assumed to be in the graph
        if (!instr->IsConstant()) instr->DeleteAndReplaceWith(NULL);
      } else {
        // Clear the liveness flag to leave the graph clean for the next DCE.
        instr->ClearFlag(HValue::kIsLive);
      }
    }
    // Collect phis that are dead and remove them in the next pass.
    for (int j = 0; j < block->phis()->length(); j++) {
      HPhi* phi = block->phis()->at(j);
      if (!phi->CheckFlag(HValue::kIsLive)) {
        dead_phis.Add(phi, zone());
      } else {
        phi->ClearFlag(HValue::kIsLive);
      }
    }
  }

  // Process phis separately to avoid simultaneously mutating the phi list.
  while (!dead_phis.is_empty()) {
    HPhi* phi = dead_phis.RemoveLast();
    HBasicBlock* block = phi->block();
    phi->DeleteAndReplaceWith(NULL);
    block->RecordDeletedPhi(phi->merged_index());
  }
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


void HOptimizedGraphBuilder::PushAndAdd(HInstruction* instr) {
  Push(instr);
  AddInstruction(instr);
}


void HOptimizedGraphBuilder::AddSoftDeoptimize() {
  isolate()->counters()->soft_deopts_requested()->Increment();
  if (FLAG_always_opt) return;
  if (current_block()->IsDeoptimizing()) return;
  Add<HSoftDeoptimize>();
  isolate()->counters()->soft_deopts_inserted()->Increment();
  current_block()->MarkAsDeoptimizing();
  graph()->set_has_soft_deoptimize(true);
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
  HConstant* undefined_constant = Add<HConstant>(
      isolate()->factory()->undefined_value());
  graph()->set_undefined_constant(undefined_constant);

  // Create an arguments object containing the initial parameters.  Set the
  // initial values of parameters including "this" having parameter index 0.
  ASSERT_EQ(scope->num_parameters() + 1, environment()->parameter_count());
  HArgumentsObject* arguments_object =
      new(zone()) HArgumentsObject(environment()->parameter_count(), zone());
  for (int i = 0; i < environment()->parameter_count(); ++i) {
    HInstruction* parameter = Add<HParameter>(i);
    arguments_object->AddArgument(parameter, zone());
    environment()->Bind(i, parameter);
  }
  AddInstruction(arguments_object);
  graph()->SetArgumentsObject(arguments_object);

  // First special is HContext.
  HInstruction* context = Add<HContext>();
  environment()->BindContext(context);

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
      return Bailout("context-allocated arguments");
    }

    environment()->Bind(scope->arguments(),
                        graph()->GetArgumentsObject());
  }
}


void HOptimizedGraphBuilder::VisitStatements(ZoneList<Statement*>* statements) {
  for (int i = 0; i < statements->length(); i++) {
    CHECK_ALIVE(Visit(statements->at(i)));
  }
}


void HOptimizedGraphBuilder::VisitBlock(Block* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (stmt->scope() != NULL) {
    return Bailout("ScopedBlock");
  }
  BreakAndContinueInfo break_info(stmt);
  { BreakAndContinueScope push(&break_info, this);
    CHECK_BAILOUT(VisitStatements(stmt->statements()));
  }
  HBasicBlock* break_block = break_info.break_block();
  if (break_block != NULL) {
    if (current_block() != NULL) current_block()->Goto(break_block);
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
    AddSimulate(stmt->ThenId());
    Visit(stmt->then_statement());
  } else if (stmt->condition()->ToBooleanIsFalse()) {
    AddSimulate(stmt->ElseId());
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
  current_block()->Goto(continue_block);
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
  current_block()->Goto(break_block);
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
    AddReturn(result);
  } else if (state->inlining_kind() == CONSTRUCT_CALL_RETURN) {
    // Return from an inlined construct call. In a test context the return value
    // will always evaluate to true, in a value context the return value needs
    // to be a JSObject.
    if (context->IsTest()) {
      TestContext* test = TestContext::cast(context);
      CHECK_ALIVE(VisitForEffect(stmt->expression()));
      current_block()->Goto(test->if_true(), state);
    } else if (context->IsEffect()) {
      CHECK_ALIVE(VisitForEffect(stmt->expression()));
      current_block()->Goto(function_return(), state);
    } else {
      ASSERT(context->IsValue());
      CHECK_ALIVE(VisitForValue(stmt->expression()));
      HValue* return_value = Pop();
      HValue* receiver = environment()->arguments_environment()->Lookup(0);
      HHasInstanceTypeAndBranch* typecheck =
          new(zone()) HHasInstanceTypeAndBranch(return_value,
                                                FIRST_SPEC_OBJECT_TYPE,
                                                LAST_SPEC_OBJECT_TYPE);
      HBasicBlock* if_spec_object = graph()->CreateBasicBlock();
      HBasicBlock* not_spec_object = graph()->CreateBasicBlock();
      typecheck->SetSuccessorAt(0, if_spec_object);
      typecheck->SetSuccessorAt(1, not_spec_object);
      current_block()->Finish(typecheck);
      if_spec_object->AddLeaveInlined(return_value, state);
      not_spec_object->AddLeaveInlined(receiver, state);
    }
  } else if (state->inlining_kind() == SETTER_CALL_RETURN) {
    // Return from an inlined setter call. The returned value is never used, the
    // value of an assignment is always the value of the RHS of the assignment.
    CHECK_ALIVE(VisitForEffect(stmt->expression()));
    if (context->IsTest()) {
      HValue* rhs = environment()->arguments_environment()->Lookup(1);
      context->ReturnValue(rhs);
    } else if (context->IsEffect()) {
      current_block()->Goto(function_return(), state);
    } else {
      ASSERT(context->IsValue());
      HValue* rhs = environment()->arguments_environment()->Lookup(1);
      current_block()->AddLeaveInlined(rhs, state);
    }
  } else {
    // Return from a normal inlined function. Visit the subexpression in the
    // expression context of the call.
    if (context->IsTest()) {
      TestContext* test = TestContext::cast(context);
      VisitForControl(stmt->expression(), test->if_true(), test->if_false());
    } else if (context->IsEffect()) {
      CHECK_ALIVE(VisitForEffect(stmt->expression()));
      current_block()->Goto(function_return(), state);
    } else {
      ASSERT(context->IsValue());
      CHECK_ALIVE(VisitForValue(stmt->expression()));
      current_block()->AddLeaveInlined(Pop(), state);
    }
  }
  set_current_block(NULL);
}


void HOptimizedGraphBuilder::VisitWithStatement(WithStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout("WithStatement");
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
    return Bailout("SwitchStatement: too many clauses");
  }

  ASSERT(stmt->switch_type() != SwitchStatement::UNKNOWN_SWITCH);
  if (stmt->switch_type() == SwitchStatement::GENERIC_SWITCH) {
    return Bailout("SwitchStatement: mixed or non-literal switch labels");
  }

  HValue* context = environment()->LookupContext();

  CHECK_ALIVE(VisitForValue(stmt->tag()));
  AddSimulate(stmt->EntryId());
  HValue* tag_value = Pop();
  HBasicBlock* first_test_block = current_block();

  HUnaryControlInstruction* string_check = NULL;
  HBasicBlock* not_string_block = NULL;

  // Test switch's tag value if all clauses are string literals
  if (stmt->switch_type() == SwitchStatement::STRING_SWITCH) {
    string_check = new(zone()) HIsStringAndBranch(tag_value);
    first_test_block = graph()->CreateBasicBlock();
    not_string_block = graph()->CreateBasicBlock();

    string_check->SetSuccessorAt(0, first_test_block);
    string_check->SetSuccessorAt(1, not_string_block);
    current_block()->Finish(string_check);

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
        AddSoftDeoptimize();
      }

      HCompareIDAndBranch* compare_ =
          new(zone()) HCompareIDAndBranch(tag_value,
                                          label_value,
                                          Token::EQ_STRICT);
      compare_->set_observed_input_representation(
          Representation::Smi(), Representation::Smi());
      compare = compare_;
    } else {
      compare = new(zone()) HStringCompareAndBranch(context, tag_value,
                                                    label_value,
                                                    Token::EQ_STRICT);
    }

    compare->SetSuccessorAt(0, body_block);
    compare->SetSuccessorAt(1, next_test_block);
    current_block()->Finish(compare);

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
      } else if (!curr_test_block->end()->IsDeoptimize()) {
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
    if (fall_through_block != NULL) fall_through_block->Goto(break_block);
    if (last_block != NULL) last_block->Goto(break_block);
    break_block->SetJoinId(stmt->ExitId());
    set_current_block(break_block);
  }
}


void HOptimizedGraphBuilder::VisitLoopBody(IterationStatement* stmt,
                                           HBasicBlock* loop_entry,
                                           BreakAndContinueInfo* break_info) {
  BreakAndContinueScope push(break_info, this);
  AddSimulate(stmt->StackCheckId());
  HValue* context = environment()->LookupContext();
  HStackCheck* stack_check = Add<HStackCheck>(
      context, HStackCheck::kBackwardsBranch);
  ASSERT(loop_entry->IsLoopHeader());
  loop_entry->loop_information()->set_stack_check(stack_check);
  CHECK_BAILOUT(Visit(stmt->body()));
}


void HOptimizedGraphBuilder::VisitDoWhileStatement(DoWhileStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  ASSERT(current_block() != NULL);
  HBasicBlock* loop_entry = osr_->BuildPossibleOsrLoopEntry(stmt);

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
  HBasicBlock* loop_entry = osr_->BuildPossibleOsrLoopEntry(stmt);

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
  HBasicBlock* loop_entry = osr_->BuildPossibleOsrLoopEntry(stmt);

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
    return Bailout("ForInStatement optimization is disabled");
  }

  if (stmt->for_in_type() != ForInStatement::FAST_FOR_IN) {
    return Bailout("ForInStatement is not fast case");
  }

  if (!stmt->each()->IsVariableProxy() ||
      !stmt->each()->AsVariableProxy()->var()->IsStackLocal()) {
    return Bailout("ForInStatement with non-local each variable");
  }

  Variable* each_var = stmt->each()->AsVariableProxy()->var();

  CHECK_ALIVE(VisitForValue(stmt->enumerable()));
  HValue* enumerable = Top();  // Leave enumerable at the top.

  HInstruction* map = Add<HForInPrepareMap>(
      environment()->LookupContext(), enumerable);
  AddSimulate(stmt->PrepareId());

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

  HBasicBlock* loop_entry = osr_->BuildPossibleOsrLoopEntry(stmt);

  HValue* index = environment()->ExpressionStackAt(0);
  HValue* limit = environment()->ExpressionStackAt(1);

  // Check that we still have more keys.
  HCompareIDAndBranch* compare_index =
      new(zone()) HCompareIDAndBranch(index, limit, Token::LT);
  compare_index->set_observed_input_representation(
      Representation::Smi(), Representation::Smi());

  HBasicBlock* loop_body = graph()->CreateBasicBlock();
  HBasicBlock* loop_successor = graph()->CreateBasicBlock();

  compare_index->SetSuccessorAt(0, loop_body);
  compare_index->SetSuccessorAt(1, loop_successor);
  current_block()->Finish(compare_index);

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
    HInstruction* new_index = HAdd::New(zone(),
                                        environment()->LookupContext(),
                                        current_index,
                                        graph()->GetConstant1());
    new_index->AssumeRepresentation(Representation::Integer32());
    PushAndAdd(new_index);
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
  return Bailout("ForOfStatement");
}


void HOptimizedGraphBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout("TryCatchStatement");
}


void HOptimizedGraphBuilder::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout("TryFinallyStatement");
}


void HOptimizedGraphBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout("DebuggerStatement");
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
  HValue* context = environment()->LookupContext();
  HFunctionLiteral* instr =
      new(zone()) HFunctionLiteral(context, shared_info, expr->pretenure());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  return Bailout("SharedFunctionInfoLiteral");
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
  HValue* context = environment()->LookupContext();
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
        return Bailout("reference to global lexical variable");
      }
      // Handle known global constants like 'undefined' specially to avoid a
      // load from a global cell for them.
      Handle<Object> constant_value =
          isolate()->factory()->GlobalConstantFor(variable->name());
      if (!constant_value.is_null()) {
        HConstant* instr = new(zone()) HConstant(constant_value);
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
        HLoadGlobalCell* instr =
            new(zone()) HLoadGlobalCell(cell, lookup.GetPropertyDetails());
        return ast_context()->ReturnInstruction(instr, expr->id());
      } else {
        HValue* context = environment()->LookupContext();
        HGlobalObject* global_object = new(zone()) HGlobalObject(context);
        AddInstruction(global_object);
        HLoadGlobalGeneric* instr =
            new(zone()) HLoadGlobalGeneric(context,
                                           global_object,
                                           variable->name(),
                                           ast_context()->is_for_typeof());
        instr->set_position(expr->position());
        return ast_context()->ReturnInstruction(instr, expr->id());
      }
    }

    case Variable::PARAMETER:
    case Variable::LOCAL: {
      HValue* value = LookupAndMakeLive(variable);
      if (value == graph()->GetConstantHole()) {
        ASSERT(IsDeclaredVariableMode(variable->mode()) &&
               variable->mode() != VAR);
        return Bailout("reference to uninitialized variable");
      }
      return ast_context()->ReturnValue(value);
    }

    case Variable::CONTEXT: {
      HValue* context = BuildContextChainWalk(variable);
      HLoadContextSlot* instr = new(zone()) HLoadContextSlot(context, variable);
      return ast_context()->ReturnInstruction(instr, expr->id());
    }

    case Variable::LOOKUP:
      return Bailout("reference to a variable which requires dynamic lookup");
  }
}


void HOptimizedGraphBuilder::VisitLiteral(Literal* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  HConstant* instr = new(zone()) HConstant(expr->value());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitRegExpLiteral(RegExpLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  Handle<JSFunction> closure = function_state()->compilation_info()->closure();
  Handle<FixedArray> literals(closure->literals());
  HValue* context = environment()->LookupContext();

  HRegExpLiteral* instr = new(zone()) HRegExpLiteral(context,
                                                     literals,
                                                     expr->pattern(),
                                                     expr->flags(),
                                                     expr->literal_index());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


static void LookupInPrototypes(Handle<Map> map,
                               Handle<String> name,
                               LookupResult* lookup) {
  while (map->prototype()->IsJSObject()) {
    Handle<JSObject> holder(JSObject::cast(map->prototype()));
    if (!holder->HasFastProperties()) break;
    map = Handle<Map>(holder->map());
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


static bool LookupGetter(Handle<Map> map,
                         Handle<String> name,
                         Handle<JSFunction>* getter,
                         Handle<JSObject>* holder) {
  Handle<AccessorPair> accessors;
  if (LookupAccessorPair(map, name, &accessors, holder) &&
      accessors->getter()->IsJSFunction()) {
    *getter = Handle<JSFunction>(JSFunction::cast(accessors->getter()));
    return true;
  }
  return false;
}


static bool LookupSetter(Handle<Map> map,
                         Handle<String> name,
                         Handle<JSFunction>* setter,
                         Handle<JSObject>* holder) {
  Handle<AccessorPair> accessors;
  if (LookupAccessorPair(map, name, &accessors, holder) &&
      accessors->setter()->IsJSFunction()) {
    *setter = Handle<JSFunction>(JSFunction::cast(accessors->setter()));
    return true;
  }
  return false;
}


// Determines whether the given array or object literal boilerplate satisfies
// all limits to be considered for fast deep-copying and computes the total
// size of all objects that are part of the graph.
static bool IsFastLiteral(Handle<JSObject> boilerplate,
                          int max_depth,
                          int* max_properties,
                          int* data_size,
                          int* pointer_size) {
  if (boilerplate->map()->is_deprecated()) {
    Handle<Object> result =
        JSObject::TryMigrateInstance(boilerplate);
    if (result->IsSmi()) return false;
  }

  ASSERT(max_depth >= 0 && *max_properties >= 0);
  if (max_depth == 0) return false;

  Isolate* isolate = boilerplate->GetIsolate();
  Handle<FixedArrayBase> elements(boilerplate->elements());
  if (elements->length() > 0 &&
      elements->map() != isolate->heap()->fixed_cow_array_map()) {
    if (boilerplate->HasFastDoubleElements()) {
      *data_size += FixedDoubleArray::SizeFor(elements->length());
    } else if (boilerplate->HasFastObjectElements()) {
      Handle<FixedArray> fast_elements = Handle<FixedArray>::cast(elements);
      int length = elements->length();
      for (int i = 0; i < length; i++) {
        if ((*max_properties)-- == 0) return false;
        Handle<Object> value(fast_elements->get(i), isolate);
        if (value->IsJSObject()) {
          Handle<JSObject> value_object = Handle<JSObject>::cast(value);
          if (!IsFastLiteral(value_object,
                             max_depth - 1,
                             max_properties,
                             data_size,
                             pointer_size)) {
            return false;
          }
        }
      }
      *pointer_size += FixedArray::SizeFor(length);
    } else {
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
      Representation representation = details.representation();
      int index = descriptors->GetFieldIndex(i);
      if ((*max_properties)-- == 0) return false;
      Handle<Object> value(boilerplate->InObjectPropertyAt(index), isolate);
      if (value->IsJSObject()) {
        Handle<JSObject> value_object = Handle<JSObject>::cast(value);
        if (!IsFastLiteral(value_object,
                           max_depth - 1,
                           max_properties,
                           data_size,
                           pointer_size)) {
          return false;
        }
      } else if (representation.IsDouble()) {
        *data_size += HeapNumber::kSize;
      }
    }
  }

  *pointer_size += boilerplate->map()->instance_size();
  return true;
}


void HOptimizedGraphBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  Handle<JSFunction> closure = function_state()->compilation_info()->closure();
  HValue* context = environment()->LookupContext();
  HInstruction* literal;

  // Check whether to use fast or slow deep-copying for boilerplate.
  int data_size = 0;
  int pointer_size = 0;
  int max_properties = kMaxFastLiteralProperties;
  Handle<Object> original_boilerplate(closure->literals()->get(
      expr->literal_index()), isolate());
  if (original_boilerplate->IsJSObject() &&
      IsFastLiteral(Handle<JSObject>::cast(original_boilerplate),
                    kMaxFastLiteralDepth,
                    &max_properties,
                    &data_size,
                    &pointer_size)) {
    Handle<JSObject> original_boilerplate_object =
        Handle<JSObject>::cast(original_boilerplate);
    Handle<JSObject> boilerplate_object =
        DeepCopy(original_boilerplate_object);

    literal = BuildFastLiteral(context,
                               boilerplate_object,
                               original_boilerplate_object,
                               data_size,
                               pointer_size,
                               DONT_TRACK_ALLOCATION_SITE);
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

    Runtime::FunctionId function_id =
        (expr->depth() > 1 || expr->may_store_doubles())
        ? Runtime::kCreateObjectLiteral : Runtime::kCreateObjectLiteralShallow;
    literal = Add<HCallRuntime>(context,
                                isolate()->factory()->empty_string(),
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
              AddSimulate(key->id(), REMOVABLE_SIMULATE);
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
        return Bailout("Object literal with complex property");
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
  HValue* context = environment()->LookupContext();
  HInstruction* literal;

  Handle<FixedArray> literals(environment()->closure()->literals(), isolate());
  Handle<Object> raw_boilerplate(literals->get(expr->literal_index()),
                                 isolate());

  bool uninitialized = false;
  if (raw_boilerplate->IsUndefined()) {
    uninitialized = true;
    raw_boilerplate = Runtime::CreateArrayLiteralBoilerplate(
        isolate(), literals, expr->constant_elements());
    if (raw_boilerplate.is_null()) {
      return Bailout("array boilerplate creation failed");
    }
    literals->set(expr->literal_index(), *raw_boilerplate);
    if (JSObject::cast(*raw_boilerplate)->elements()->map() ==
        isolate()->heap()->fixed_cow_array_map()) {
      isolate()->counters()->cow_arrays_created_runtime()->Increment();
    }
  }

  Handle<JSObject> original_boilerplate_object =
      Handle<JSObject>::cast(raw_boilerplate);
  ElementsKind boilerplate_elements_kind =
      Handle<JSObject>::cast(original_boilerplate_object)->GetElementsKind();

  // TODO(mvstanton): This heuristic is only a temporary solution.  In the
  // end, we want to quit creating allocation site info after a certain number
  // of GCs for a call site.
  AllocationSiteMode mode = AllocationSiteInfo::GetMode(
      boilerplate_elements_kind);

  // Check whether to use fast or slow deep-copying for boilerplate.
  int data_size = 0;
  int pointer_size = 0;
  int max_properties = kMaxFastLiteralProperties;
  if (IsFastLiteral(original_boilerplate_object,
                    kMaxFastLiteralDepth,
                    &max_properties,
                    &data_size,
                    &pointer_size)) {
    if (mode == TRACK_ALLOCATION_SITE) {
      pointer_size += AllocationSiteInfo::kSize;
    }

    Handle<JSObject> boilerplate_object = DeepCopy(original_boilerplate_object);
    literal = BuildFastLiteral(context,
                               boilerplate_object,
                               original_boilerplate_object,
                               data_size,
                               pointer_size,
                               mode);
  } else {
    NoObservableSideEffectsScope no_effects(this);
    // Boilerplate already exists and constant elements are never accessed,
    // pass an empty fixed array to the runtime function instead.
    Handle<FixedArray> constants = isolate()->factory()->empty_fixed_array();
    int literal_index = expr->literal_index();

    Add<HPushArgument>(Add<HConstant>(literals));
    Add<HPushArgument>(Add<HConstant>(literal_index));
    Add<HPushArgument>(Add<HConstant>(constants));

    Runtime::FunctionId function_id = (expr->depth() > 1)
        ? Runtime::kCreateArrayLiteral : Runtime::kCreateArrayLiteralShallow;
    literal = Add<HCallRuntime>(context,
                                isolate()->factory()->empty_string(),
                                Runtime::FunctionForId(function_id),
                                3);

    // De-opt if elements kind changed from boilerplate_elements_kind.
    Handle<Map> map = Handle<Map>(original_boilerplate_object->map(),
                                  isolate());
    AddInstruction(HCheckMaps::New(literal, map, zone()));
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
    if (!Smi::IsValid(i)) return Bailout("Non-smi key in array literal");

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

    AddSimulate(expr->GetIdForElement(i));
  }

  Drop(1);  // array literal index
  return ast_context()->ReturnValue(Pop());
}


// Sets the lookup result and returns true if the load/store can be inlined.
static bool ComputeLoadStoreField(Handle<Map> type,
                                  Handle<String> name,
                                  LookupResult* lookup,
                                  bool is_store) {
  if (type->has_named_interceptor()) {
    lookup->InterceptorResult(NULL);
    return false;
  }
  // If we directly find a field, the access can be inlined.
  type->LookupDescriptor(NULL, *name, lookup);
  if (lookup->IsField()) return true;

  // For a load, we are out of luck if there is no such field.
  if (!is_store) return false;

  // 2nd chance: A store into a non-existent field can still be inlined if we
  // have a matching transition and some room left in the object.
  type->LookupTransition(NULL, *name, lookup);
  return lookup->IsTransitionToField(*type) &&
      (type->unused_property_fields() > 0);
}


static Representation ComputeLoadStoreRepresentation(Handle<Map> type,
                                                     LookupResult* lookup) {
  if (lookup->IsField()) {
    return lookup->representation();
  } else {
    Map* transition = lookup->GetTransitionMapFromMap(*type);
    int descriptor = transition->LastAdded();
    PropertyDetails details =
        transition->instance_descriptors()->GetDetails(descriptor);
    return details.representation();
  }
}


void HOptimizedGraphBuilder::AddCheckMap(HValue* object, Handle<Map> map) {
  BuildCheckHeapObject(object);
  AddInstruction(HCheckMaps::New(object, map, zone()));
}


void HOptimizedGraphBuilder::AddCheckMapsWithTransitions(HValue* object,
                                                         Handle<Map> map) {
  BuildCheckHeapObject(object);
  AddInstruction(HCheckMaps::NewWithTransitions(object, map, zone()));
}


HInstruction* HOptimizedGraphBuilder::BuildStoreNamedField(
    HValue* object,
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
        Bailout("improper object on prototype chain for store");
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
    Add<HCheckPrototypeMaps>(Handle<JSObject>(JSObject::cast(map->prototype())),
                             Handle<JSObject>(JSObject::cast(proto)),
                             zone(), top_info());
  }

  HObjectAccess field_access = HObjectAccess::ForField(map, lookup, name);
  Representation representation = ComputeLoadStoreRepresentation(map, lookup);
  bool transition_to_field = lookup->IsTransitionToField(*map);

  HStoreNamedField *instr;
  if (FLAG_track_double_fields && representation.IsDouble()) {
    if (transition_to_field) {
      // The store requires a mutable HeapNumber to be allocated.
      NoObservableSideEffectsScope no_side_effects(this);
      HInstruction* heap_number_size = Add<HConstant>(HeapNumber::kSize);
      HInstruction* double_box = Add<HAllocate>(
          environment()->LookupContext(), heap_number_size,
          HType::HeapNumber(), HAllocate::CAN_ALLOCATE_IN_NEW_SPACE);
      AddStoreMapConstant(double_box, isolate()->factory()->heap_number_map());
      AddStore(double_box, HObjectAccess::ForHeapNumberValue(),
          value, Representation::Double());
      instr = new(zone()) HStoreNamedField(object, field_access, double_box);
    } else {
      // Already holds a HeapNumber; load the box and write its value field.
      HInstruction* double_box = AddLoad(object, field_access);
      double_box->set_type(HType::HeapNumber());
      instr = new(zone()) HStoreNamedField(double_box,
          HObjectAccess::ForHeapNumberValue(), value, Representation::Double());
    }
  } else {
    // This is a non-double store.
    instr = new(zone()) HStoreNamedField(
        object, field_access, value, representation);
  }

  if (transition_to_field) {
    Handle<Map> transition(lookup->GetTransitionMapFromMap(*map));
    instr->SetTransition(transition, top_info());
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
  HValue* context = environment()->LookupContext();
  return new(zone()) HStoreNamedGeneric(
                         context,
                         object,
                         name,
                         value,
                         function_strict_mode_flag());
}


HInstruction* HOptimizedGraphBuilder::BuildCallSetter(
    HValue* object,
    HValue* value,
    Handle<Map> map,
    Handle<JSFunction> setter,
    Handle<JSObject> holder) {
  AddCheckConstantFunction(holder, object, map);
  Add<HPushArgument>(object);
  Add<HPushArgument>(value);
  return new(zone()) HCallConstantFunction(setter, 2);
}


HInstruction* HOptimizedGraphBuilder::BuildStoreNamedMonomorphic(
    HValue* object,
    Handle<String> name,
    HValue* value,
    Handle<Map> map) {
  // Handle a store to a known field.
  LookupResult lookup(isolate());
  if (ComputeLoadStoreField(map, name, &lookup, true)) {
    AddCheckMapsWithTransitions(object, map);
    return BuildStoreNamedField(object, name, value, map, &lookup);
  }

  // No luck, do a generic store.
  return BuildStoreNamedGeneric(object, name, value);
}


HInstruction* HOptimizedGraphBuilder::TryLoadPolymorphicAsMonomorphic(
    Property* expr,
    HValue* object,
    SmallMapList* types,
    Handle<String> name) {
  // Use monomorphic load if property lookup results in the same field index
  // for all maps. Requires special map check on the set of all handled maps.
  if (types->length() > kMaxLoadPolymorphism) return NULL;

  LookupResult lookup(isolate());
  int count;
  Representation representation = Representation::None();
  HObjectAccess access = HObjectAccess::ForMap();  // initial value unused.
  for (count = 0; count < types->length(); ++count) {
    Handle<Map> map = types->at(count);
    if (!ComputeLoadStoreField(map, name, &lookup, false)) break;

    HObjectAccess new_access = HObjectAccess::ForField(map, &lookup, name);
    Representation new_representation =
        ComputeLoadStoreRepresentation(map, &lookup);

    if (count == 0) {
      // First time through the loop; set access and representation.
      access = new_access;
    } else if (!representation.IsCompatibleForLoad(new_representation)) {
      // Representations did not match.
      break;
    } else if (access.offset() != new_access.offset()) {
      // Offsets did not match.
      break;
    } else if (access.IsInobject() != new_access.IsInobject()) {
      // In-objectness did not match.
      break;
    }
    representation = representation.generalize(new_representation);
  }

  if (count != types->length()) return NULL;

  // Everything matched; can use monomorphic load.
  BuildCheckHeapObject(object);
  AddInstruction(HCheckMaps::New(object, types, zone()));
  return BuildLoadNamedField(object, access, representation);
}


void HOptimizedGraphBuilder::HandlePolymorphicLoadNamedField(
    Property* expr,
    HValue* object,
    SmallMapList* types,
    Handle<String> name) {
  HInstruction* instr = TryLoadPolymorphicAsMonomorphic(
      expr, object, types, name);
  if (instr == NULL) {
    // Something did not match; must use a polymorphic load.
    BuildCheckHeapObject(object);
    HValue* context = environment()->LookupContext();
    instr = new(zone()) HLoadNamedFieldPolymorphic(
        context, object, types, name, zone());
  }

  instr->set_position(expr->position());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


bool HOptimizedGraphBuilder::TryStorePolymorphicAsMonomorphic(
    int position,
    BailoutId assignment_id,
    HValue* object,
    HValue* value,
    SmallMapList* types,
    Handle<String> name) {
  // Use monomorphic store if property lookup results in the same field index
  // for all maps. Requires special map check on the set of all handled maps.
  if (types->length() > kMaxStorePolymorphism) return false;

  // TODO(verwaest): Merge the checking logic with the code in
  // TryLoadPolymorphicAsMonomorphic.
  LookupResult lookup(isolate());
  int count;
  Representation representation = Representation::None();
  HObjectAccess access = HObjectAccess::ForMap();  // initial value unused.
  for (count = 0; count < types->length(); ++count) {
    Handle<Map> map = types->at(count);
    // Pass false to ignore transitions.
    if (!ComputeLoadStoreField(map, name, &lookup, false)) break;

    HObjectAccess new_access = HObjectAccess::ForField(map, &lookup, name);
    Representation new_representation =
        ComputeLoadStoreRepresentation(map, &lookup);

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
  AddInstruction(HCheckMaps::New(object, types, zone()));
  HInstruction* store;
  CHECK_ALIVE_OR_RETURN(
      store = BuildStoreNamedField(
          object, name, value, types->at(count - 1), &lookup),
      true);
  Push(value);
  store->set_position(position);
  AddInstruction(store);
  AddSimulate(assignment_id);
  ast_context()->ReturnValue(Pop());
  return true;
}


void HOptimizedGraphBuilder::HandlePolymorphicStoreNamedField(
    BailoutId id,
    int position,
    BailoutId assignment_id,
    HValue* object,
    HValue* value,
    SmallMapList* types,
    Handle<String> name) {
  if (TryStorePolymorphicAsMonomorphic(
          position, assignment_id, object, value, types, name)) {
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
    if (ComputeLoadStoreField(map, name, &lookup, true)) {
      if (count == 0) {
        BuildCheckHeapObject(object);
        join = graph()->CreateBasicBlock();
      }
      ++count;
      HBasicBlock* if_true = graph()->CreateBasicBlock();
      HBasicBlock* if_false = graph()->CreateBasicBlock();
      HCompareMap* compare =
          new(zone()) HCompareMap(object, map, if_true, if_false);
      current_block()->Finish(compare);

      set_current_block(if_true);
      HInstruction* instr;
      CHECK_ALIVE(
          instr = BuildStoreNamedField(object, name, value, map, &lookup));
      instr->set_position(position);
      // Goto will add the HSimulate for the store.
      AddInstruction(instr);
      if (!ast_context()->IsEffect()) Push(value);
      current_block()->Goto(join);

      set_current_block(if_false);
    }
  }

  // Finish up.  Unconditionally deoptimize if we've handled all the maps we
  // know about and do not want to handle ones we've never seen.  Otherwise
  // use a generic IC.
  if (count == types->length() && FLAG_deoptimize_uncommon_cases) {
    current_block()->FinishExitWithDeoptimization(HDeoptimize::kNoUses);
  } else {
    HInstruction* instr = BuildStoreNamedGeneric(object, name, value);
    instr->set_position(position);
    AddInstruction(instr);

    if (join != NULL) {
      if (!ast_context()->IsEffect()) Push(value);
      current_block()->Goto(join);
    } else {
      // The HSimulate for the store should not see the stored value in
      // effect contexts (it is not materialized at expr->id() in the
      // unoptimized code).
      if (instr->HasObservableSideEffects()) {
        if (ast_context()->IsEffect()) {
          AddSimulate(id, REMOVABLE_SIMULATE);
        } else {
          Push(value);
          AddSimulate(id, REMOVABLE_SIMULATE);
          Drop(1);
        }
      }
      return ast_context()->ReturnValue(value);
    }
  }

  ASSERT(join != NULL);
  join->SetJoinId(id);
  set_current_block(join);
  if (!ast_context()->IsEffect()) ast_context()->ReturnValue(Pop());
}


void HOptimizedGraphBuilder::HandlePropertyAssignment(Assignment* expr) {
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  CHECK_ALIVE(VisitForValue(prop->obj()));

  if (prop->key()->IsPropertyName()) {
    // Named store.
    CHECK_ALIVE(VisitForValue(expr->value()));
    HValue* value = environment()->ExpressionStackAt(0);
    HValue* object = environment()->ExpressionStackAt(1);

    if (expr->IsUninitialized()) AddSoftDeoptimize();
    return BuildStoreNamed(expr, expr->id(), expr->position(),
                           expr->AssignmentId(), prop, object, value);
  } else {
    // Keyed store.
    CHECK_ALIVE(VisitForValue(prop->key()));
    CHECK_ALIVE(VisitForValue(expr->value()));
    HValue* value = environment()->ExpressionStackAt(0);
    HValue* key = environment()->ExpressionStackAt(1);
    HValue* object = environment()->ExpressionStackAt(2);
    bool has_side_effects = false;
    HandleKeyedElementAccess(object, key, value, expr, expr->AssignmentId(),
                             expr->position(),
                             true,  // is_store
                             &has_side_effects);
    Drop(3);
    Push(value);
    AddSimulate(expr->AssignmentId(), REMOVABLE_SIMULATE);
    return ast_context()->ReturnValue(Pop());
  }
}


// Because not every expression has a position and there is not common
// superclass of Assignment and CountOperation, we cannot just pass the
// owning expression instead of position and ast_id separately.
void HOptimizedGraphBuilder::HandleGlobalVariableAssignment(
    Variable* var,
    HValue* value,
    int position,
    BailoutId ast_id) {
  LookupResult lookup(isolate());
  GlobalPropertyAccess type = LookupGlobalProperty(var, &lookup, true);
  if (type == kUseCell) {
    Handle<GlobalObject> global(current_info()->global_object());
    Handle<PropertyCell> cell(global->GetPropertyCell(&lookup));
    HInstruction* instr = Add<HStoreGlobalCell>(value, cell,
                                                lookup.GetPropertyDetails());
    instr->set_position(position);
    if (instr->HasObservableSideEffects()) {
      AddSimulate(ast_id, REMOVABLE_SIMULATE);
    }
  } else {
    HValue* context =  environment()->LookupContext();
    HGlobalObject* global_object = Add<HGlobalObject>(context);
    HStoreGlobalGeneric* instr =
        Add<HStoreGlobalGeneric>(context, global_object, var->name(),
                                 value, function_strict_mode_flag());
    instr->set_position(position);
    ASSERT(instr->HasObservableSideEffects());
    AddSimulate(ast_id, REMOVABLE_SIMULATE);
  }
}


void HOptimizedGraphBuilder::BuildStoreNamed(Expression* expr,
                                             BailoutId id,
                                             int position,
                                             BailoutId assignment_id,
                                             Property* prop,
                                             HValue* object,
                                             HValue* value) {
  Literal* key = prop->key()->AsLiteral();
  Handle<String> name = Handle<String>::cast(key->value());
  ASSERT(!name.is_null());

  HInstruction* instr = NULL;
  SmallMapList* types = expr->GetReceiverTypes();
  bool monomorphic = expr->IsMonomorphic();
  Handle<Map> map;
  if (monomorphic) {
    map = types->first();
    if (map->is_dictionary_map()) monomorphic = false;
  }
  if (monomorphic) {
    Handle<JSFunction> setter;
    Handle<JSObject> holder;
    if (LookupSetter(map, name, &setter, &holder)) {
      AddCheckConstantFunction(holder, object, map);
      if (FLAG_inline_accessors &&
          TryInlineSetter(setter, id, assignment_id, value)) {
        return;
      }
      Drop(2);
      Add<HPushArgument>(object);
      Add<HPushArgument>(value);
      instr = new(zone()) HCallConstantFunction(setter, 2);
    } else {
      Drop(2);
      CHECK_ALIVE(instr = BuildStoreNamedMonomorphic(object,
                                                     name,
                                                     value,
                                                     map));
    }

  } else if (types != NULL && types->length() > 1) {
    Drop(2);
    return HandlePolymorphicStoreNamedField(
        id, position, assignment_id, object, value, types, name);
  } else {
    Drop(2);
    instr = BuildStoreNamedGeneric(object, name, value);
  }

  Push(value);
  instr->set_position(position);
  AddInstruction(instr);
  if (instr->HasObservableSideEffects()) {
    AddSimulate(assignment_id, REMOVABLE_SIMULATE);
  }
  return ast_context()->ReturnValue(Pop());
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
      return Bailout("unsupported let compound assignment");
    }

    CHECK_ALIVE(VisitForValue(operation));

    switch (var->location()) {
      case Variable::UNALLOCATED:
        HandleGlobalVariableAssignment(var,
                                       Top(),
                                       expr->position(),
                                       expr->AssignmentId());
        break;

      case Variable::PARAMETER:
      case Variable::LOCAL:
        if (var->mode() == CONST)  {
          return Bailout("unsupported const compound assignment");
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
              Bailout(
                  "assignment to parameter, function uses arguments object");
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
        HStoreContextSlot* instr = Add<HStoreContextSlot>(context, var->index(),
                                                          mode, Top());
        if (instr->HasObservableSideEffects()) {
          AddSimulate(expr->AssignmentId(), REMOVABLE_SIMULATE);
        }
        break;
      }

      case Variable::LOOKUP:
        return Bailout("compound assignment to lookup slot");
    }
    return ast_context()->ReturnValue(Pop());

  } else if (prop != NULL) {
    if (prop->key()->IsPropertyName()) {
      // Named property.
      CHECK_ALIVE(VisitForValue(prop->obj()));
      HValue* object = Top();

      Handle<String> name = prop->key()->AsLiteral()->AsPropertyName();
      Handle<Map> map;
      HInstruction* load = NULL;
      SmallMapList* types = prop->GetReceiverTypes();
      bool monomorphic = prop->IsMonomorphic();
      if (monomorphic) {
        map = types->first();
        // We can't generate code for a monomorphic dict mode load so
        // just pretend it is not monomorphic.
        if (map->is_dictionary_map()) monomorphic = false;
      }
      if (monomorphic) {
        Handle<JSFunction> getter;
        Handle<JSObject> holder;
        if (LookupGetter(map, name, &getter, &holder)) {
          load = BuildCallGetter(object, map, getter, holder);
        } else {
          load = BuildLoadNamedMonomorphic(object, name, prop, map);
        }
      } else if (types != NULL && types->length() > 1) {
        load = TryLoadPolymorphicAsMonomorphic(prop, object, types, name);
      }
      if (load == NULL) load = BuildLoadNamedGeneric(object, name, prop);
      PushAndAdd(load);
      if (load->HasObservableSideEffects()) {
        AddSimulate(prop->LoadId(), REMOVABLE_SIMULATE);
      }

      CHECK_ALIVE(VisitForValue(expr->value()));
      HValue* right = Pop();
      HValue* left = Pop();

      HInstruction* instr = BuildBinaryOperation(operation, left, right);
      PushAndAdd(instr);
      if (instr->HasObservableSideEffects()) {
        AddSimulate(operation->id(), REMOVABLE_SIMULATE);
      }

      return BuildStoreNamed(prop, expr->id(), expr->position(),
                             expr->AssignmentId(), prop, object, instr);
    } else {
      // Keyed property.
      CHECK_ALIVE(VisitForValue(prop->obj()));
      CHECK_ALIVE(VisitForValue(prop->key()));
      HValue* obj = environment()->ExpressionStackAt(1);
      HValue* key = environment()->ExpressionStackAt(0);

      bool has_side_effects = false;
      HValue* load = HandleKeyedElementAccess(
          obj, key, NULL, prop, prop->LoadId(), RelocInfo::kNoPosition,
          false,  // is_store
          &has_side_effects);
      Push(load);
      if (has_side_effects) AddSimulate(prop->LoadId(), REMOVABLE_SIMULATE);

      CHECK_ALIVE(VisitForValue(expr->value()));
      HValue* right = Pop();
      HValue* left = Pop();

      HInstruction* instr = BuildBinaryOperation(operation, left, right);
      PushAndAdd(instr);
      if (instr->HasObservableSideEffects()) {
        AddSimulate(operation->id(), REMOVABLE_SIMULATE);
      }

      HandleKeyedElementAccess(obj, key, instr, expr, expr->AssignmentId(),
                               RelocInfo::kNoPosition,
                               true,  // is_store
                               &has_side_effects);

      // Drop the simulated receiver, key, and value.  Return the value.
      Drop(3);
      Push(instr);
      ASSERT(has_side_effects);  // Stores always have side effects.
      AddSimulate(expr->AssignmentId(), REMOVABLE_SIMULATE);
      return ast_context()->ReturnValue(Pop());
    }

  } else {
    return Bailout("invalid lhs in compound assignment");
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
        return Bailout("non-initializer assignment to const");
      }
    }

    if (proxy->IsArguments()) return Bailout("assignment to arguments");

    // Handle the assignment.
    switch (var->location()) {
      case Variable::UNALLOCATED:
        CHECK_ALIVE(VisitForValue(expr->value()));
        HandleGlobalVariableAssignment(var,
                                       Top(),
                                       expr->position(),
                                       expr->AssignmentId());
        return ast_context()->ReturnValue(Pop());

      case Variable::PARAMETER:
      case Variable::LOCAL: {
        // Perform an initialization check for let declared variables
        // or parameters.
        if (var->mode() == LET && expr->op() == Token::ASSIGN) {
          HValue* env_value = environment()->Lookup(var);
          if (env_value == graph()->GetConstantHole()) {
            return Bailout("assignment to let variable before initialization");
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
              return Bailout("assignment to parameter in arguments object");
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
        HStoreContextSlot* instr = Add<HStoreContextSlot>(context, var->index(),
                                                          mode, Top());
        if (instr->HasObservableSideEffects()) {
          AddSimulate(expr->AssignmentId(), REMOVABLE_SIMULATE);
        }
        return ast_context()->ReturnValue(Pop());
      }

      case Variable::LOOKUP:
        return Bailout("assignment to LOOKUP variable");
    }
  } else {
    return Bailout("invalid left-hand side in assignment");
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

  HValue* context = environment()->LookupContext();
  HValue* value = environment()->Pop();
  HThrow* instr = Add<HThrow>(context, value);
  instr->set_position(expr->position());
  AddSimulate(expr->id());
  current_block()->FinishExit(new(zone()) HAbnormalExit);
  set_current_block(NULL);
}


HLoadNamedField* HGraphBuilder::BuildLoadNamedField(
    HValue* object,
    HObjectAccess access,
    Representation representation) {
  bool load_double = false;
  if (representation.IsDouble()) {
    representation = Representation::Tagged();
    load_double = FLAG_track_double_fields;
  }
  HLoadNamedField* field =
      new(zone()) HLoadNamedField(object, access, NULL, representation);
  if (load_double) {
    AddInstruction(field);
    field->set_type(HType::HeapNumber());
    return new(zone()) HLoadNamedField(field,
        HObjectAccess::ForHeapNumberValue(), NULL, Representation::Double());
  }
  return field;
}


HInstruction* HOptimizedGraphBuilder::BuildLoadNamedGeneric(
    HValue* object,
    Handle<String> name,
    Property* expr) {
  if (expr->IsUninitialized()) {
    AddSoftDeoptimize();
  }
  HValue* context = environment()->LookupContext();
  return new(zone()) HLoadNamedGeneric(context, object, name);
}


HInstruction* HOptimizedGraphBuilder::BuildCallGetter(
    HValue* object,
    Handle<Map> map,
    Handle<JSFunction> getter,
    Handle<JSObject> holder) {
  AddCheckConstantFunction(holder, object, map);
  Add<HPushArgument>(object);
  return new(zone()) HCallConstantFunction(getter, 1);
}


HInstruction* HOptimizedGraphBuilder::BuildLoadNamedMonomorphic(
    HValue* object,
    Handle<String> name,
    Property* expr,
    Handle<Map> map) {
  // Handle a load from a known field.
  ASSERT(!map->is_dictionary_map());

  // Handle access to various length properties
  if (name->Equals(isolate()->heap()->length_string())) {
    if (map->instance_type() == JS_ARRAY_TYPE) {
      AddCheckMapsWithTransitions(object, map);
      return new(zone()) HLoadNamedField(object,
          HObjectAccess::ForArrayLength());
    }
  }

  LookupResult lookup(isolate());
  map->LookupDescriptor(NULL, *name, &lookup);
  if (lookup.IsField()) {
    AddCheckMap(object, map);
    return BuildLoadNamedField(object,
        HObjectAccess::ForField(map, &lookup, name),
        ComputeLoadStoreRepresentation(map, &lookup));
  }

  // Handle a load of a constant known function.
  if (lookup.IsConstantFunction()) {
    AddCheckMap(object, map);
    Handle<JSFunction> function(lookup.GetConstantFunctionFromMap(*map));
    return new(zone()) HConstant(function);
  }

  // Handle a load from a known field somewhere in the prototype chain.
  LookupInPrototypes(map, name, &lookup);
  if (lookup.IsField()) {
    Handle<JSObject> prototype(JSObject::cast(map->prototype()));
    Handle<JSObject> holder(lookup.holder());
    Handle<Map> holder_map(holder->map());
    AddCheckMap(object, map);
    Add<HCheckPrototypeMaps>(prototype, holder, zone(), top_info());
    HValue* holder_value = Add<HConstant>(holder);
    return BuildLoadNamedField(holder_value,
        HObjectAccess::ForField(holder_map, &lookup, name),
        ComputeLoadStoreRepresentation(map, &lookup));
  }

  // Handle a load of a constant function somewhere in the prototype chain.
  if (lookup.IsConstantFunction()) {
    Handle<JSObject> prototype(JSObject::cast(map->prototype()));
    Handle<JSObject> holder(lookup.holder());
    Handle<Map> holder_map(holder->map());
    AddCheckMap(object, map);
    Add<HCheckPrototypeMaps>(prototype, holder, zone(), top_info());
    Handle<JSFunction> function(lookup.GetConstantFunctionFromMap(*holder_map));
    return new(zone()) HConstant(function);
  }

  // No luck, do a generic load.
  return BuildLoadNamedGeneric(object, name, expr);
}


HInstruction* HOptimizedGraphBuilder::BuildLoadKeyedGeneric(HValue* object,
                                                            HValue* key) {
  HValue* context = environment()->LookupContext();
  return new(zone()) HLoadKeyedGeneric(context, object, key);
}


HInstruction* HOptimizedGraphBuilder::BuildMonomorphicElementAccess(
    HValue* object,
    HValue* key,
    HValue* val,
    HValue* dependency,
    Handle<Map> map,
    bool is_store,
    KeyedAccessStoreMode store_mode) {
  HCheckMaps* mapcheck = HCheckMaps::New(object, map, zone(), dependency);
  AddInstruction(mapcheck);
  if (dependency) {
    mapcheck->ClearGVNFlag(kDependsOnElementsKind);
  }

  // Loads from a "stock" fast holey double arrays can elide the hole check.
  LoadKeyedHoleMode load_mode = NEVER_RETURN_HOLE;
  if (*map == isolate()->get_initial_js_array_map(FAST_HOLEY_DOUBLE_ELEMENTS) &&
      isolate()->IsFastArrayConstructorPrototypeChainIntact()) {
    Handle<JSObject> prototype(JSObject::cast(map->prototype()), isolate());
    Handle<JSObject> object_prototype = isolate()->initial_object_prototype();
    Add<HCheckPrototypeMaps>(prototype, object_prototype, zone(), top_info());
    load_mode = ALLOW_RETURN_HOLE;
    graph()->MarkDependsOnEmptyArrayProtoElements();
  }

  return BuildUncheckedMonomorphicElementAccess(
      object, key, val,
      mapcheck, map->instance_type() == JS_ARRAY_TYPE,
      map->elements_kind(), is_store, load_mode, store_mode);
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
  Handle<Map> most_general_consolidated_map;
  for (int i = 0; i < maps->length(); ++i) {
    Handle<Map> map = maps->at(i);
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
    // Remember the most general elements kind, the code for its load will
    // properly handle all of the more specific cases.
    if ((i == 0) || IsMoreGeneralElementsKindTransition(
            most_general_consolidated_map->elements_kind(),
            map->elements_kind())) {
      most_general_consolidated_map = map;
    }
  }
  if (!has_double_maps && !has_smi_or_object_maps) return NULL;

  HCheckMaps* check_maps = HCheckMaps::New(object, maps, zone());
  AddInstruction(check_maps);
  HInstruction* instr = BuildUncheckedMonomorphicElementAccess(
      object, key, val, check_maps,
      most_general_consolidated_map->instance_type() == JS_ARRAY_TYPE,
      most_general_consolidated_map->elements_kind(),
      false, NEVER_RETURN_HOLE, STANDARD_STORE);
  return instr;
}


HValue* HOptimizedGraphBuilder::HandlePolymorphicElementAccess(
    HValue* object,
    HValue* key,
    HValue* val,
    Expression* prop,
    BailoutId ast_id,
    int position,
    bool is_store,
    KeyedAccessStoreMode store_mode,
    bool* has_side_effects) {
  *has_side_effects = false;
  BuildCheckHeapObject(object);
  SmallMapList* maps = prop->GetReceiverTypes();
  bool todo_external_array = false;

  if (!is_store) {
    HInstruction* consolidated_load =
        TryBuildConsolidatedElementLoad(object, key, val, maps);
    if (consolidated_load != NULL) {
      *has_side_effects |= consolidated_load->HasObservableSideEffects();
      if (position != RelocInfo::kNoPosition) {
        consolidated_load->set_position(position);
      }
      return consolidated_load;
    }
  }

  static const int kNumElementTypes = kElementsKindCount;
  bool type_todo[kNumElementTypes];
  for (int i = 0; i < kNumElementTypes; ++i) {
    type_todo[i] = false;
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

  int num_untransitionable_maps = 0;
  Handle<Map> untransitionable_map;
  HTransitionElementsKind* transition = NULL;
  for (int i = 0; i < maps->length(); ++i) {
    Handle<Map> map = maps->at(i);
    ASSERT(map->IsMap());
    if (!transition_target.at(i).is_null()) {
      ASSERT(Map::IsValidElementsTransition(
          map->elements_kind(),
          transition_target.at(i)->elements_kind()));
      HValue* context = environment()->LookupContext();
      transition = Add<HTransitionElementsKind>(context, object, map,
                                                transition_target.at(i));
    } else {
      type_todo[map->elements_kind()] = true;
      if (IsExternalArrayElementsKind(map->elements_kind())) {
        todo_external_array = true;
      }
      num_untransitionable_maps++;
      untransitionable_map = map;
    }
  }

  // If only one map is left after transitioning, handle this case
  // monomorphically.
  ASSERT(num_untransitionable_maps >= 1);
  if (num_untransitionable_maps == 1) {
    HInstruction* instr = NULL;
    if (untransitionable_map->has_slow_elements_kind()) {
      instr = AddInstruction(is_store ? BuildStoreKeyedGeneric(object, key, val)
                                      : BuildLoadKeyedGeneric(object, key));
    } else {
      instr = BuildMonomorphicElementAccess(
          object, key, val, transition, untransitionable_map, is_store,
          store_mode);
    }
    *has_side_effects |= instr->HasObservableSideEffects();
    if (position != RelocInfo::kNoPosition) instr->set_position(position);
    return is_store ? NULL : instr;
  }

  HInstruction* checkspec =
      AddInstruction(HCheckInstanceType::NewIsSpecObject(object, zone()));
  HBasicBlock* join = graph()->CreateBasicBlock();

  HInstruction* elements_kind_instr = Add<HElementsKind>(object);
  HInstruction* elements = AddLoadElements(object, checkspec);
  HLoadExternalArrayPointer* external_elements = NULL;
  HInstruction* checked_key = NULL;

  // Generated code assumes that FAST_* and DICTIONARY_ELEMENTS ElementsKinds
  // are handled before external arrays.
  STATIC_ASSERT(FAST_SMI_ELEMENTS < FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS < FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND);
  STATIC_ASSERT(FAST_DOUBLE_ELEMENTS < FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND);
  STATIC_ASSERT(DICTIONARY_ELEMENTS < FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND);

  for (ElementsKind elements_kind = FIRST_ELEMENTS_KIND;
       elements_kind <= LAST_ELEMENTS_KIND;
       elements_kind = ElementsKind(elements_kind + 1)) {
    // After having handled FAST_* and DICTIONARY_ELEMENTS, we need to add some
    // code that's executed for all external array cases.
    STATIC_ASSERT(LAST_EXTERNAL_ARRAY_ELEMENTS_KIND ==
                  LAST_ELEMENTS_KIND);
    if (elements_kind == FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND
        && todo_external_array) {
      HInstruction* length = AddLoadFixedArrayLength(elements);
      checked_key = Add<HBoundsCheck>(key, length);
      external_elements = Add<HLoadExternalArrayPointer>(elements);
    }
    if (type_todo[elements_kind]) {
      HBasicBlock* if_true = graph()->CreateBasicBlock();
      HBasicBlock* if_false = graph()->CreateBasicBlock();
      HCompareConstantEqAndBranch* elements_kind_branch =
          new(zone()) HCompareConstantEqAndBranch(
              elements_kind_instr, elements_kind, Token::EQ_STRICT);
      elements_kind_branch->SetSuccessorAt(0, if_true);
      elements_kind_branch->SetSuccessorAt(1, if_false);
      current_block()->Finish(elements_kind_branch);

      set_current_block(if_true);
      HInstruction* access;
      if (IsFastElementsKind(elements_kind)) {
        if (is_store && !IsFastDoubleElementsKind(elements_kind)) {
          AddInstruction(HCheckMaps::New(
              elements, isolate()->factory()->fixed_array_map(),
              zone(), elements_kind_branch));
        }
        // TODO(jkummerow): The need for these two blocks could be avoided
        // in one of two ways:
        // (1) Introduce ElementsKinds for JSArrays that are distinct from
        //     those for fast objects.
        // (2) Put the common instructions into a third "join" block. This
        //     requires additional AST IDs that we can deopt to from inside
        //     that join block. They must be added to the Property class (when
        //     it's a keyed property) and registered in the full codegen.
        HBasicBlock* if_jsarray = graph()->CreateBasicBlock();
        HBasicBlock* if_fastobject = graph()->CreateBasicBlock();
        HHasInstanceTypeAndBranch* typecheck =
            new(zone()) HHasInstanceTypeAndBranch(object, JS_ARRAY_TYPE);
        typecheck->SetSuccessorAt(0, if_jsarray);
        typecheck->SetSuccessorAt(1, if_fastobject);
        current_block()->Finish(typecheck);

        set_current_block(if_jsarray);
        HInstruction* length = AddLoad(object, HObjectAccess::ForArrayLength(),
            typecheck, Representation::Smi());
        length->set_type(HType::Smi());

        checked_key = Add<HBoundsCheck>(key, length);
        access = AddInstruction(BuildFastElementAccess(
            elements, checked_key, val, elements_kind_branch,
            elements_kind, is_store, NEVER_RETURN_HOLE, STANDARD_STORE));
        if (!is_store) {
          Push(access);
        }

        *has_side_effects |= access->HasObservableSideEffects();
        // The caller will use has_side_effects and add correct Simulate.
        access->SetFlag(HValue::kHasNoObservableSideEffects);
        if (position != -1) {
          access->set_position(position);
        }
        if_jsarray->GotoNoSimulate(join);

        set_current_block(if_fastobject);
        length = AddLoadFixedArrayLength(elements);
        checked_key = Add<HBoundsCheck>(key, length);
        access = AddInstruction(BuildFastElementAccess(
            elements, checked_key, val, elements_kind_branch,
            elements_kind, is_store, NEVER_RETURN_HOLE, STANDARD_STORE));
      } else if (elements_kind == DICTIONARY_ELEMENTS) {
        if (is_store) {
          access = AddInstruction(BuildStoreKeyedGeneric(object, key, val));
        } else {
          access = AddInstruction(BuildLoadKeyedGeneric(object, key));
        }
      } else {  // External array elements.
        access = AddInstruction(BuildExternalArrayElementAccess(
            external_elements, checked_key, val,
            elements_kind_branch, elements_kind, is_store));
      }
      *has_side_effects |= access->HasObservableSideEffects();
      // The caller will use has_side_effects and add correct Simulate.
      access->SetFlag(HValue::kHasNoObservableSideEffects);
      if (position != RelocInfo::kNoPosition) access->set_position(position);
      if (!is_store) {
        Push(access);
      }
      current_block()->GotoNoSimulate(join);
      set_current_block(if_false);
    }
  }

  // Deopt if none of the cases matched.
  current_block()->FinishExitWithDeoptimization(HDeoptimize::kNoUses);
  set_current_block(join);
  return is_store ? NULL : Pop();
}


HValue* HOptimizedGraphBuilder::HandleKeyedElementAccess(
    HValue* obj,
    HValue* key,
    HValue* val,
    Expression* expr,
    BailoutId ast_id,
    int position,
    bool is_store,
    bool* has_side_effects) {
  ASSERT(!expr->IsPropertyName());
  HInstruction* instr = NULL;
  if (expr->IsMonomorphic()) {
    Handle<Map> map = expr->GetMonomorphicReceiverType();
    if (map->has_slow_elements_kind()) {
      instr = is_store ? BuildStoreKeyedGeneric(obj, key, val)
                       : BuildLoadKeyedGeneric(obj, key);
      AddInstruction(instr);
    } else {
      BuildCheckHeapObject(obj);
      instr = BuildMonomorphicElementAccess(
          obj, key, val, NULL, map, is_store, expr->GetStoreMode());
    }
  } else if (expr->GetReceiverTypes() != NULL &&
             !expr->GetReceiverTypes()->is_empty()) {
    return HandlePolymorphicElementAccess(
        obj, key, val, expr, ast_id, position, is_store,
        expr->GetStoreMode(), has_side_effects);
  } else {
    if (is_store) {
      instr = BuildStoreKeyedGeneric(obj, key, val);
    } else {
      instr = BuildLoadKeyedGeneric(obj, key);
    }
    AddInstruction(instr);
  }
  if (position != RelocInfo::kNoPosition) instr->set_position(position);
  *has_side_effects = instr->HasObservableSideEffects();
  return instr;
}


HInstruction* HOptimizedGraphBuilder::BuildStoreKeyedGeneric(
    HValue* object,
    HValue* key,
    HValue* value) {
  HValue* context = environment()->LookupContext();
  return new(zone()) HStoreKeyedGeneric(
                         context,
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
    HInstruction* push_argument = new(zone()) HPushArgument(argument);
    push_argument->InsertAfter(insert_after);
    insert_after = push_argument;
  }

  HArgumentsElements* arguments_elements =
      new(zone()) HArgumentsElements(true);
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
      result = new(zone()) HArgumentsLength(elements);
    } else {
      // Number of arguments without receiver.
      int argument_count = environment()->
          arguments_environment()->parameter_count() - 1;
      result = new(zone()) HConstant(argument_count);
    }
  } else {
    Push(graph()->GetArgumentsObject());
    VisitForValue(expr->key());
    if (HasStackOverflow() || current_block() == NULL) return true;
    HValue* key = Pop();
    Drop(1);  // Arguments object.
    if (function_state()->outer() == NULL) {
      HInstruction* elements = Add<HArgumentsElements>(false);
      HInstruction* length = Add<HArgumentsLength>(elements);
      HInstruction* checked_key = Add<HBoundsCheck>(key, length);
      result = new(zone()) HAccessArgumentsAt(elements, length, checked_key);
    } else {
      EnsureArgumentsArePushedForAccess();

      // Number of arguments without receiver.
      HInstruction* elements = function_state()->arguments_elements();
      int argument_count = environment()->
          arguments_environment()->parameter_count() - 1;
      HInstruction* length = Add<HConstant>(argument_count);
      HInstruction* checked_key = Add<HBoundsCheck>(key, length);
      result = new(zone()) HAccessArgumentsAt(elements, length, checked_key);
    }
  }
  ast_context()->ReturnInstruction(result, expr->id());
  return true;
}


void HOptimizedGraphBuilder::VisitProperty(Property* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());

  if (TryArgumentsAccess(expr)) return;

  CHECK_ALIVE(VisitForValue(expr->obj()));

  HInstruction* instr = NULL;
  if (expr->IsStringLength()) {
    HValue* string = Pop();
    BuildCheckHeapObject(string);
    AddInstruction(HCheckInstanceType::NewIsString(string, zone()));
    instr = HStringLength::New(zone(), string);
  } else if (expr->IsStringAccess()) {
    CHECK_ALIVE(VisitForValue(expr->key()));
    HValue* index = Pop();
    HValue* string = Pop();
    HValue* context = environment()->LookupContext();
    HInstruction* char_code =
      BuildStringCharCodeAt(context, string, index);
    AddInstruction(char_code);
    instr = HStringCharFromCode::New(zone(), context, char_code);

  } else if (expr->IsFunctionPrototype()) {
    HValue* function = Pop();
    BuildCheckHeapObject(function);
    instr = new(zone()) HLoadFunctionPrototype(function);

  } else if (expr->key()->IsPropertyName()) {
    Handle<String> name = expr->key()->AsLiteral()->AsPropertyName();
    SmallMapList* types = expr->GetReceiverTypes();
    HValue* object = Top();

    Handle<Map> map;
    bool monomorphic = false;
    if (expr->IsMonomorphic()) {
      map = types->first();
      monomorphic = !map->is_dictionary_map();
    } else if (object->HasMonomorphicJSObjectType()) {
      map = object->GetMonomorphicJSObjectMap();
      monomorphic = !map->is_dictionary_map();
    }
    if (monomorphic) {
      Handle<JSFunction> getter;
      Handle<JSObject> holder;
      if (LookupGetter(map, name, &getter, &holder)) {
        AddCheckConstantFunction(holder, Top(), map);
        if (FLAG_inline_accessors && TryInlineGetter(getter, expr)) return;
        Add<HPushArgument>(Pop());
        instr = new(zone()) HCallConstantFunction(getter, 1);
      } else {
        instr = BuildLoadNamedMonomorphic(Pop(), name, expr, map);
      }
    } else if (types != NULL && types->length() > 1) {
      return HandlePolymorphicLoadNamedField(expr, Pop(), types, name);
    } else {
      instr = BuildLoadNamedGeneric(Pop(), name, expr);
    }

  } else {
    CHECK_ALIVE(VisitForValue(expr->key()));

    HValue* key = Pop();
    HValue* obj = Pop();

    bool has_side_effects = false;
    HValue* load = HandleKeyedElementAccess(
        obj, key, NULL, expr, expr->id(), expr->position(),
        false,  // is_store
        &has_side_effects);
    if (has_side_effects) {
      if (ast_context()->IsEffect()) {
        AddSimulate(expr->id(), REMOVABLE_SIMULATE);
      } else {
        Push(load);
        AddSimulate(expr->id(), REMOVABLE_SIMULATE);
        Drop(1);
      }
    }
    return ast_context()->ReturnValue(load);
  }
  instr->set_position(expr->position());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::AddCheckPrototypeMaps(Handle<JSObject> holder,
                                                   Handle<Map> receiver_map) {
  if (!holder.is_null()) {
    Handle<JSObject> prototype(JSObject::cast(receiver_map->prototype()));
    Add<HCheckPrototypeMaps>(prototype, holder, zone(), top_info());
  }
}


void HOptimizedGraphBuilder::AddCheckConstantFunction(
    Handle<JSObject> holder,
    HValue* receiver,
    Handle<Map> receiver_map) {
  // Constant functions have the nice property that the map will change if they
  // are overwritten.  Therefore it is enough to check the map of the holder and
  // its prototypes.
  AddCheckMapsWithTransitions(receiver, receiver_map);
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


void HOptimizedGraphBuilder::HandlePolymorphicCallNamed(
    Call* expr,
    HValue* receiver,
    SmallMapList* types,
    Handle<String> name) {
  // TODO(ager): We should recognize when the prototype chains for different
  // maps are identical. In that case we can avoid repeatedly generating the
  // same prototype map checks.
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
        HIsSmiAndBranch* smicheck = new(zone()) HIsSmiAndBranch(receiver);
        smicheck->SetSuccessorAt(0, empty_smi_block);
        smicheck->SetSuccessorAt(1, not_smi_block);
        current_block()->Finish(smicheck);
        empty_smi_block->Goto(number_block);
        set_current_block(not_smi_block);
      } else {
        BuildCheckHeapObject(receiver);
      }
    }
    HBasicBlock* if_true = graph()->CreateBasicBlock();
    HBasicBlock* if_false = graph()->CreateBasicBlock();
    HUnaryControlInstruction* compare;

    if (handle_smi && map.is_identical_to(number_marker_map)) {
      compare = new(zone()) HCompareMap(
          receiver, heap_number_map, if_true, if_false);
      map = initial_number_map;
      expr->set_number_check(
          Handle<JSObject>(JSObject::cast(map->prototype())));
    } else if (map.is_identical_to(string_marker_map)) {
      compare = new(zone()) HIsStringAndBranch(receiver);
      compare->SetSuccessorAt(0, if_true);
      compare->SetSuccessorAt(1, if_false);
      map = initial_string_map;
      expr->set_string_check(
          Handle<JSObject>(JSObject::cast(map->prototype())));
    } else {
      compare = new(zone()) HCompareMap(receiver, map, if_true, if_false);
      expr->set_map_check();
    }

    current_block()->Finish(compare);

    if (expr->check_type() == NUMBER_CHECK) {
      if_true->Goto(number_block);
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
          new(zone()) HCallConstantFunction(expr->target(), argument_count);
      call->set_position(expr->position());
      PreProcessCall(call);
      AddInstruction(call);
      if (!ast_context()->IsEffect()) Push(call);
    }

    if (current_block() != NULL) current_block()->Goto(join);
    set_current_block(if_false);
  }

  // Finish up.  Unconditionally deoptimize if we've handled all the maps we
  // know about and do not want to handle ones we've never seen.  Otherwise
  // use a generic IC.
  if (ordered_functions == types->length() && FLAG_deoptimize_uncommon_cases) {
    current_block()->FinishExitWithDeoptimization(HDeoptimize::kNoUses);
  } else {
    HValue* context = environment()->LookupContext();
    HCallNamed* call = new(zone()) HCallNamed(context, name, argument_count);
    call->set_position(expr->position());
    PreProcessCall(call);

    if (join != NULL) {
      AddInstruction(call);
      if (!ast_context()->IsEffect()) Push(call);
      current_block()->Goto(join);
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

#if !V8_TARGET_ARCH_IA32
  // Target must be able to use caller's context.
  CompilationInfo* outer_info = current_info();
  if (target->context() != outer_info->closure()->context() ||
      outer_info->scope()->contains_with() ||
      outer_info->scope()->num_heap_slots() > 0) {
    TraceInline(target, caller, "target requires context change");
    return false;
  }
#endif


  // Don't inline deeper than kMaxInliningLevels calls.
  HEnvironment* env = environment();
  int current_level = 1;
  while (env->outer() != NULL) {
    if (current_level == Compiler::kMaxInliningLevels) {
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
      target_shared->DisableOptimization("parse/scope error");
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
  if (flags->Contains(kDontInline) || flags->Contains(kDontOptimize)) {
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
#if V8_TARGET_ARCH_IA32
  // IA32 only, overwrite the caller's context in the deoptimization
  // environment with the correct one.
  //
  // TODO(kmillikin): implement the same inlining on other platforms so we
  // can remove the unsightly ifdefs in this function.
  HConstant* context = Add<HConstant>(Handle<Context>(target->context()));
  inner_env->BindContext(context);
#endif

  AddSimulate(return_id);
  current_block()->UpdateEnvironment(inner_env);
  HArgumentsObject* arguments_object = NULL;

  // If the function uses arguments object create and bind one, also copy
  // current arguments values to use them for materialization.
  if (function->scope()->arguments() != NULL) {
    ASSERT(function->scope()->arguments()->IsStackAllocated());
    HEnvironment* arguments_env = inner_env->arguments_environment();
    int arguments_count = arguments_env->parameter_count();
    arguments_object = Add<HArgumentsObject>(arguments_count, zone());
    inner_env->Bind(function->scope()->arguments(), arguments_object);
    for (int i = 0; i < arguments_count; i++) {
      arguments_object->AddArgument(arguments_env->Lookup(i), zone());
    }
  }

  HEnterInlined* enter_inlined =
      Add<HEnterInlined>(target, arguments_count, function,
                         function_state()->inlining_kind(),
                         function->scope()->arguments(),
                         arguments_object, undefined_receiver, zone());
  function_state()->set_entry(enter_inlined);

  VisitDeclarations(target_info.scope()->declarations());
  VisitStatements(function->body());
  if (HasStackOverflow()) {
    // Bail out if the inline function did, as we cannot residualize a call
    // instead.
    TraceInline(target, caller, "inline graph construction failed");
    target_shared->DisableOptimization("inlining bailed out");
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
        current_block()->Goto(inlined_test_context()->if_true(), state);
      } else if (call_context()->IsEffect()) {
        current_block()->Goto(function_return(), state);
      } else {
        ASSERT(call_context()->IsValue());
        current_block()->AddLeaveInlined(implicit_return_value, state);
      }
    } else if (state->inlining_kind() == SETTER_CALL_RETURN) {
      // Falling off the end of an inlined setter call. The returned value is
      // never used, the value of an assignment is always the value of the RHS
      // of the assignment.
      if (call_context()->IsTest()) {
        inlined_test_context()->ReturnValue(implicit_return_value);
      } else if (call_context()->IsEffect()) {
        current_block()->Goto(function_return(), state);
      } else {
        ASSERT(call_context()->IsValue());
        current_block()->AddLeaveInlined(implicit_return_value, state);
      }
    } else {
      // Falling off the end of a normal inlined function. This basically means
      // returning undefined.
      if (call_context()->IsTest()) {
        current_block()->Goto(inlined_test_context()->if_false(), state);
      } else if (call_context()->IsEffect()) {
        current_block()->Goto(function_return(), state);
      } else {
        ASSERT(call_context()->IsValue());
        current_block()->AddLeaveInlined(undefined, state);
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
      if_true->Goto(true_target, function_state());
    }
    if (if_false->HasPredecessor()) {
      entry->RegisterReturnTarget(if_false, zone());
      if_false->SetJoinId(ast_id);
      HBasicBlock* false_target = TestContext::cast(ast_context())->if_false();
      if_false->Goto(false_target, function_state());
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
                                             Property* prop) {
  return TryInline(CALL_AS_METHOD,
                   getter,
                   0,
                   NULL,
                   prop->id(),
                   prop->LoadId(),
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
        HValue* context = environment()->LookupContext();
        Drop(1);  // Receiver.
        HInstruction* op =
            HUnaryMathOperation::New(zone(), context, argument, id);
        op->set_position(expr->position());
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
        HValue* context = environment()->LookupContext();
        HInstruction* op = HMul::NewImul(zone(), context, left, right);
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
        HValue* context = environment()->LookupContext();
        ASSERT(!expr->holder().is_null());
        Add<HCheckPrototypeMaps>(Call::GetPrototypeForPrimitiveCheck(
                STRING_CHECK, expr->holder()->GetIsolate()),
            expr->holder(), zone(), top_info());
        HInstruction* char_code =
            BuildStringCharCodeAt(context, string, index);
        if (id == kStringCharCodeAt) {
          ast_context()->ReturnInstruction(char_code, expr->id());
          return true;
        }
        AddInstruction(char_code);
        HInstruction* result =
            HStringCharFromCode::New(zone(), context, char_code);
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    case kStringFromCharCode:
      if (argument_count == 2 && check_type == RECEIVER_MAP_CHECK) {
        AddCheckConstantFunction(expr->holder(), receiver, receiver_map);
        HValue* argument = Pop();
        HValue* context = environment()->LookupContext();
        Drop(1);  // Receiver.
        HInstruction* result =
            HStringCharFromCode::New(zone(), context, argument);
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
        HValue* context = environment()->LookupContext();
        Drop(1);  // Receiver.
        HInstruction* op =
            HUnaryMathOperation::New(zone(), context, argument, id);
        op->set_position(expr->position());
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
        HValue* context = environment()->LookupContext();
        HInstruction* result = NULL;
        // Use sqrt() if exponent is 0.5 or -0.5.
        if (right->IsConstant() && HConstant::cast(right)->HasDoubleValue()) {
          double exponent = HConstant::cast(right)->DoubleValue();
          if (exponent == 0.5) {
            result =
                HUnaryMathOperation::New(zone(), context, left, kMathPowHalf);
          } else if (exponent == -0.5) {
            HValue* one = graph()->GetConstant1();
            HInstruction* sqrt =
                HUnaryMathOperation::New(zone(), context, left, kMathPowHalf);
            AddInstruction(sqrt);
            // MathPowHalf doesn't have side effects so there's no need for
            // an environment simulation here.
            ASSERT(!sqrt->HasObservableSideEffects());
            result = HDiv::New(zone(), context, one, sqrt);
          } else if (exponent == 2.0) {
            result = HMul::New(zone(), context, left, left);
          }
        } else if (right->EqualsInteger32Constant(2)) {
          result = HMul::New(zone(), context, left, left);
        }

        if (result == NULL) {
          result = HPower::New(zone(), left, right);
        }
        ast_context()->ReturnInstruction(result, expr->id());
        return true;
      }
      break;
    case kMathRandom:
      if (argument_count == 1 && check_type == RECEIVER_MAP_CHECK) {
        AddCheckConstantFunction(expr->holder(), receiver, receiver_map);
        Drop(1);  // Receiver.
        HValue* context = environment()->LookupContext();
        HGlobalObject* global_object = Add<HGlobalObject>(context);
        HRandom* result = new(zone()) HRandom(global_object);
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
        HValue* context = environment()->LookupContext();
        HMathMinMax::Operation op = (id == kMathMin) ? HMathMinMax::kMathMin
                                                     : HMathMinMax::kMathMax;
        HInstruction* result =
            HMathMinMax::New(zone(), context, left, right, op);
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
        HValue* context = environment()->LookupContext();
        HInstruction* result = HMul::NewImul(zone(), context, left, right);
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
  VisitForValue(prop->obj());
  if (HasStackOverflow() || current_block() == NULL) return true;
  HValue* function = Top();
  AddCheckConstantFunction(expr->holder(), function, function_map);
  Drop(1);

  VisitForValue(args->at(0));
  if (HasStackOverflow() || current_block() == NULL) return true;
  HValue* receiver = Pop();

  if (function_state()->outer() == NULL) {
    HInstruction* elements = Add<HArgumentsElements>(false);
    HInstruction* length = Add<HArgumentsLength>(elements);
    HValue* wrapped_receiver = Add<HWrapReceiver>(receiver, function);
    HInstruction* result =
        new(zone()) HApplyArguments(function,
                                    wrapped_receiver,
                                    length,
                                    elements);
    result->set_position(expr->position());
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
    PushAndAdd(new(zone()) HWrapReceiver(receiver, function));
    for (int i = 1; i < arguments_count; i++) {
      Push(arguments_values->at(i));
    }

    Handle<JSFunction> known_function;
    if (function->IsConstant()) {
      HConstant* constant_function = HConstant::cast(function);
      known_function = Handle<JSFunction>::cast(constant_function->handle());
      int args_count = arguments_count - 1;  // Excluding receiver.
      if (TryInlineApply(known_function, expr, args_count)) return true;
    }

    Drop(arguments_count - 1);
    PushAndAdd(new(zone()) HPushArgument(Pop()));
    for (int i = 1; i < arguments_count; i++) {
      PushAndAdd(new(zone()) HPushArgument(arguments_values->at(i)));
    }

    HValue* context = environment()->LookupContext();
    HInvokeFunction* call = new(zone()) HInvokeFunction(
        context,
        function,
        known_function,
        arguments_count);
    Drop(arguments_count);
    call->set_position(expr->position());
    ast_context()->ReturnInstruction(call, expr->id());
    return true;
  }
}


// Checks if all maps in |types| are from the same family, i.e., are elements
// transitions of each other. Returns either NULL if they are not from the same
// family, or a Map* indicating the map with the first elements kind of the
// family that is in the list.
static Map* CheckSameElementsFamily(SmallMapList* types) {
  if (types->length() <= 1) return NULL;
  // Check if all maps belong to the same transition family.
  Map* kinds[kFastElementsKindCount];
  Map* first_map = *types->first();
  ElementsKind first_kind = first_map->elements_kind();
  if (!IsFastElementsKind(first_kind)) return NULL;
  int first_index = GetSequenceIndexFromFastElementsKind(first_kind);
  int last_index = first_index;

  for (int i = 0; i < kFastElementsKindCount; i++) kinds[i] = NULL;

  kinds[first_index] = first_map;

  for (int i = 1; i < types->length(); ++i) {
    Map* map = *types->at(i);
    ElementsKind elements_kind = map->elements_kind();
    if (!IsFastElementsKind(elements_kind)) return NULL;
    int index = GetSequenceIndexFromFastElementsKind(elements_kind);
    if (index < first_index) {
      first_index = index;
    } else if (index > last_index) {
      last_index = index;
    } else if (kinds[index] != map) {
      return NULL;
    }
    kinds[index] = map;
  }

  Map* current = kinds[first_index];
  for (int i = first_index + 1; i <= last_index; i++) {
    Map* next = kinds[i];
    if (next != NULL) {
      ElementsKind current_kind = next->elements_kind();
      if (next != current->LookupElementsTransitionMap(current_kind)) {
        return NULL;
      }
      current = next;
    }
  }

  return kinds[first_index];
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

      HValue* context = environment()->LookupContext();
      call = new(zone()) HCallKeyed(context, key, argument_count);
      call->set_position(expr->position());
      Drop(argument_count + 1);  // 1 is the key.
      return ast_context()->ReturnInstruction(call, expr->id());
    }

    // Named function call.
    if (TryCallApply(expr)) return;

    CHECK_ALIVE(VisitForValue(prop->obj()));
    CHECK_ALIVE(VisitExpressions(expr->arguments()));

    Handle<String> name = prop->key()->AsLiteral()->AsPropertyName();
    SmallMapList* types = expr->GetReceiverTypes();

    bool monomorphic = expr->IsMonomorphic();
    Handle<Map> receiver_map;
    if (monomorphic) {
      receiver_map = (types == NULL || types->is_empty())
          ? Handle<Map>::null()
          : types->first();
    } else {
      Map* family_map = CheckSameElementsFamily(types);
      if (family_map != NULL) {
        receiver_map = Handle<Map>(family_map);
        monomorphic = expr->ComputeTarget(receiver_map, name);
      }
    }

    HValue* receiver =
        environment()->ExpressionStackAt(expr->arguments()->length());
    if (monomorphic) {
      if (TryInlineBuiltinMethodCall(expr,
                                     receiver,
                                     receiver_map,
                                     expr->check_type())) {
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
        HValue* context = environment()->LookupContext();
        call = PreProcessCall(
            new(zone()) HCallNamed(context, name, argument_count));
      } else {
        AddCheckConstantFunction(expr->holder(), receiver, receiver_map);

        if (TryInlineCall(expr)) return;
        call = PreProcessCall(
            new(zone()) HCallConstantFunction(expr->target(),
                                              argument_count));
      }
    } else if (types != NULL && types->length() > 1) {
      ASSERT(expr->check_type() == RECEIVER_MAP_CHECK);
      HandlePolymorphicCallNamed(expr, receiver, types, name);
      return;

    } else {
      HValue* context = environment()->LookupContext();
      call = PreProcessCall(
          new(zone()) HCallNamed(context, name, argument_count));
    }

  } else {
    VariableProxy* proxy = expr->expression()->AsVariableProxy();
    bool global_call = proxy != NULL && proxy->var()->IsUnallocated();

    if (proxy != NULL && proxy->var()->is_possibly_eval(isolate())) {
      return Bailout("possible direct call to eval");
    }

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
        HValue* context = environment()->LookupContext();
        HGlobalObject* global_object = new(zone()) HGlobalObject(context);
        PushAndAdd(global_object);
        CHECK_ALIVE(VisitExpressions(expr->arguments()));

        CHECK_ALIVE(VisitForValue(expr->expression()));
        HValue* function = Pop();
        Add<HCheckFunction>(function, expr->target());

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
          HValue* context = environment()->LookupContext();
          call = PreProcessCall(
              new(zone()) HCallNamed(context, var->name(), argument_count));
        } else {
          call = PreProcessCall(new(zone()) HCallKnownGlobal(expr->target(),
                                                           argument_count));
        }
      } else {
        HValue* context = environment()->LookupContext();
        HGlobalObject* receiver = Add<HGlobalObject>(context);
        PushAndAdd(new(zone()) HPushArgument(receiver));
        CHECK_ALIVE(VisitArgumentList(expr->arguments()));

        call = new(zone()) HCallGlobal(context, var->name(), argument_count);
        Drop(argument_count);
      }

    } else if (expr->IsMonomorphic()) {
      // The function is on the stack in the unoptimized code during
      // evaluation of the arguments.
      CHECK_ALIVE(VisitForValue(expr->expression()));
      HValue* function = Top();
      HValue* context = environment()->LookupContext();
      HGlobalObject* global = Add<HGlobalObject>(context);
      HGlobalReceiver* receiver = new(zone()) HGlobalReceiver(global);
      PushAndAdd(receiver);
      CHECK_ALIVE(VisitExpressions(expr->arguments()));
      Add<HCheckFunction>(function, expr->target());

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
        call = PreProcessCall(
            new(zone()) HInvokeFunction(context,
                                        function,
                                        expr->target(),
                                        argument_count));
        Drop(1);  // The function.
      }

    } else {
      CHECK_ALIVE(VisitForValue(expr->expression()));
      HValue* function = Top();
      HValue* context = environment()->LookupContext();
      HGlobalObject* global_object = Add<HGlobalObject>(context);
      HGlobalReceiver* receiver = Add<HGlobalReceiver>(global_object);
      PushAndAdd(new(zone()) HPushArgument(receiver));
      CHECK_ALIVE(VisitArgumentList(expr->arguments()));

      call = new(zone()) HCallFunction(context, function, argument_count);
      Drop(argument_count + 1);
    }
  }

  call->set_position(expr->position());
  return ast_context()->ReturnInstruction(call, expr->id());
}


// Checks whether allocation using the given constructor can be inlined.
static bool IsAllocationInlineable(Handle<JSFunction> constructor) {
  return constructor->has_initial_map() &&
      constructor->initial_map()->instance_type() == JS_OBJECT_TYPE &&
      constructor->initial_map()->instance_size() < HAllocateObject::kMaxSize;
}


void HOptimizedGraphBuilder::VisitCallNew(CallNew* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  int argument_count = expr->arguments()->length() + 1;  // Plus constructor.
  HValue* context = environment()->LookupContext();

  if (FLAG_inline_construct &&
      expr->IsMonomorphic() &&
      IsAllocationInlineable(expr->target())) {
    // The constructor function is on the stack in the unoptimized code
    // during evaluation of the arguments.
    CHECK_ALIVE(VisitForValue(expr->expression()));
    HValue* function = Top();
    CHECK_ALIVE(VisitExpressions(expr->arguments()));
    Handle<JSFunction> constructor = expr->target();
    HValue* check = Add<HCheckFunction>(function, constructor);

    // Force completion of inobject slack tracking before generating
    // allocation code to finalize instance size.
    if (constructor->shared()->IsInobjectSlackTrackingInProgress()) {
      constructor->shared()->CompleteInobjectSlackTracking();
    }

    // Replace the constructor function with a newly allocated receiver.
    HInstruction* receiver = Add<HAllocateObject>(context, constructor);
    // Index of the receiver from the top of the expression stack.
    const int receiver_index = argument_count - 1;
    ASSERT(environment()->ExpressionStackAt(receiver_index) == function);
    environment()->SetExpressionStackAt(receiver_index, receiver);

    if (TryInlineConstruct(expr, receiver)) return;

    // TODO(mstarzinger): For now we remove the previous HAllocateObject and
    // add HPushArgument for the arguments in case inlining failed.  What we
    // actually should do is emit HInvokeFunction on the constructor instead
    // of using HCallNew as a fallback.
    receiver->DeleteAndReplaceWith(NULL);
    check->DeleteAndReplaceWith(NULL);
    environment()->SetExpressionStackAt(receiver_index, function);
    HInstruction* call = PreProcessCall(
        new(zone()) HCallNew(context, function, argument_count));
    call->set_position(expr->position());
    return ast_context()->ReturnInstruction(call, expr->id());
  } else {
    // The constructor function is both an operand to the instruction and an
    // argument to the construct call.
    Handle<JSFunction> array_function(
        isolate()->global_context()->array_function(), isolate());
    CHECK_ALIVE(VisitArgument(expr->expression()));
    HValue* constructor = HPushArgument::cast(Top())->argument();
    CHECK_ALIVE(VisitArgumentList(expr->arguments()));
    HCallNew* call;
    if (expr->target().is_identical_to(array_function)) {
      Handle<Cell> cell = expr->allocation_info_cell();
      Add<HCheckFunction>(constructor, array_function);
      call = new(zone()) HCallNewArray(context, constructor, argument_count,
                                       cell, expr->elements_kind());
    } else {
      call = new(zone()) HCallNew(context, constructor, argument_count);
    }
    Drop(argument_count);
    call->set_position(expr->position());
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
    return Bailout("call to a JavaScript runtime function");
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

    HValue* context = environment()->LookupContext();
    Handle<String> name = expr->name();
    int argument_count = expr->arguments()->length();
    HCallRuntime* call =
        new(zone()) HCallRuntime(context, name, function, argument_count);
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
    case Token::SUB: return VisitSub(expr);
    case Token::BIT_NOT: return VisitBitNot(expr);
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
    HValue* context = environment()->LookupContext();
    HDeleteProperty* instr = new(zone()) HDeleteProperty(context, obj, key);
    return ast_context()->ReturnInstruction(instr, expr->id());
  } else if (proxy != NULL) {
    Variable* var = proxy->var();
    if (var->IsUnallocated()) {
      Bailout("delete with global variable");
    } else if (var->IsStackAllocated() || var->IsContextSlot()) {
      // Result of deleting non-global variables is false.  'this' is not
      // really a variable, though we implement it as one.  The
      // subexpression does not have side effects.
      HValue* value = var->is_this()
          ? graph()->GetConstantTrue()
          : graph()->GetConstantFalse();
      return ast_context()->ReturnValue(value);
    } else {
      Bailout("delete with non-global variable");
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
  HValue* context = environment()->LookupContext();
  HInstruction* instr = new(zone()) HTypeof(context, value);
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitSub(UnaryOperation* expr) {
  CHECK_ALIVE(VisitForValue(expr->expression()));
  HValue* value = Pop();
  HValue* context = environment()->LookupContext();
  HInstruction* instr =
      HMul::New(zone(), context, value, graph()->GetConstantMinus1());
  Handle<Type> operand_type = expr->expression()->lower_type();
  Representation rep = ToRepresentation(operand_type);
  if (operand_type->Is(Type::None())) {
    AddSoftDeoptimize();
  }
  if (instr->IsBinaryOperation()) {
    HBinaryOperation::cast(instr)->set_observed_input_representation(1, rep);
    HBinaryOperation::cast(instr)->set_observed_input_representation(2, rep);
  }
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitBitNot(UnaryOperation* expr) {
  CHECK_ALIVE(VisitForValue(expr->expression()));
  HValue* value = Pop();
  Handle<Type> operand_type = expr->expression()->lower_type();
  if (operand_type->Is(Type::None())) {
    AddSoftDeoptimize();
  }
  HInstruction* instr = new(zone()) HBitNot(value);
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
  TypeInfo info = expr->type();
  Representation rep = ToRepresentation(info);
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
  HValue* context = environment()->LookupContext();
  HInstruction* instr = HAdd::New(zone(), context, Top(), delta);
  instr->SetFlag(HInstruction::kCannotBeTagged);
  instr->ClearAllSideEffects();
  AddInstruction(instr);
  return instr;
}


void HOptimizedGraphBuilder::VisitCountOperation(CountOperation* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  Expression* target = expr->expression();
  VariableProxy* proxy = target->AsVariableProxy();
  Property* prop = target->AsProperty();
  if (proxy == NULL && prop == NULL) {
    return Bailout("invalid lhs in count operation");
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
      return Bailout("unsupported count operation with const");
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
                                       expr->position(),
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
              return Bailout("assignment to parameter in arguments object");
            }
          }
        }

        HValue* context = BuildContextChainWalk(var);
        HStoreContextSlot::Mode mode = IsLexicalVariableMode(var->mode())
            ? HStoreContextSlot::kCheckDeoptimize : HStoreContextSlot::kNoCheck;
        HStoreContextSlot* instr = Add<HStoreContextSlot>(context, var->index(),
                                                          mode, after);
        if (instr->HasObservableSideEffects()) {
          AddSimulate(expr->AssignmentId(), REMOVABLE_SIMULATE);
        }
        break;
      }

      case Variable::LOOKUP:
        return Bailout("lookup variable in count operation");
    }

  } else {
    // Argument of the count operation is a property.
    ASSERT(prop != NULL);

    if (prop->key()->IsPropertyName()) {
      // Named property.
      if (returns_original_input) Push(graph()->GetConstantUndefined());

      CHECK_ALIVE(VisitForValue(prop->obj()));
      HValue* object = Top();

      Handle<String> name = prop->key()->AsLiteral()->AsPropertyName();
      Handle<Map> map;
      HInstruction* load = NULL;
      bool monomorphic = prop->IsMonomorphic();
      SmallMapList* types = prop->GetReceiverTypes();
      if (monomorphic) {
        map = types->first();
        if (map->is_dictionary_map()) monomorphic = false;
      }
      if (monomorphic) {
        Handle<JSFunction> getter;
        Handle<JSObject> holder;
        if (LookupGetter(map, name, &getter, &holder)) {
          load = BuildCallGetter(object, map, getter, holder);
        } else {
          load = BuildLoadNamedMonomorphic(object, name, prop, map);
        }
      } else if (types != NULL && types->length() > 1) {
        load = TryLoadPolymorphicAsMonomorphic(prop, object, types, name);
      }
      if (load == NULL) load = BuildLoadNamedGeneric(object, name, prop);
      PushAndAdd(load);
      if (load->HasObservableSideEffects()) {
        AddSimulate(prop->LoadId(), REMOVABLE_SIMULATE);
      }

      after = BuildIncrement(returns_original_input, expr);
      input = Pop();

      HInstruction* store;
      if (!monomorphic || map->is_observed()) {
        // If we don't know the monomorphic type, do a generic store.
        CHECK_ALIVE(store = BuildStoreNamedGeneric(object, name, after));
      } else {
        Handle<JSFunction> setter;
        Handle<JSObject> holder;
        if (LookupSetter(map, name, &setter, &holder)) {
          store = BuildCallSetter(object, after, map, setter, holder);
        } else {
          CHECK_ALIVE(store = BuildStoreNamedMonomorphic(object,
                                                         name,
                                                         after,
                                                         map));
        }
      }
      AddInstruction(store);

      // Overwrite the receiver in the bailout environment with the result
      // of the operation, and the placeholder with the original value if
      // necessary.
      environment()->SetExpressionStackAt(0, after);
      if (returns_original_input) environment()->SetExpressionStackAt(1, input);
      if (store->HasObservableSideEffects()) {
        AddSimulate(expr->AssignmentId(), REMOVABLE_SIMULATE);
      }

    } else {
      // Keyed property.
      if (returns_original_input) Push(graph()->GetConstantUndefined());

      CHECK_ALIVE(VisitForValue(prop->obj()));
      CHECK_ALIVE(VisitForValue(prop->key()));
      HValue* obj = environment()->ExpressionStackAt(1);
      HValue* key = environment()->ExpressionStackAt(0);

      bool has_side_effects = false;
      HValue* load = HandleKeyedElementAccess(
          obj, key, NULL, prop, prop->LoadId(), RelocInfo::kNoPosition,
          false,  // is_store
          &has_side_effects);
      Push(load);
      if (has_side_effects) AddSimulate(prop->LoadId(), REMOVABLE_SIMULATE);

      after = BuildIncrement(returns_original_input, expr);
      input = environment()->ExpressionStackAt(0);

      HandleKeyedElementAccess(obj, key, after, expr, expr->AssignmentId(),
                               RelocInfo::kNoPosition,
                               true,  // is_store
                               &has_side_effects);

      // Drop the key and the original value from the bailout environment.
      // Overwrite the receiver with the result of the operation, and the
      // placeholder with the original value if necessary.
      Drop(2);
      environment()->SetExpressionStackAt(0, after);
      if (returns_original_input) environment()->SetExpressionStackAt(1, input);
      ASSERT(has_side_effects);  // Stores always have side effects.
      AddSimulate(expr->AssignmentId(), REMOVABLE_SIMULATE);
    }
  }

  Drop(returns_original_input ? 2 : 1);
  return ast_context()->ReturnValue(expr->is_postfix() ? input : after);
}


HInstruction* HOptimizedGraphBuilder::BuildStringCharCodeAt(
    HValue* context,
    HValue* string,
    HValue* index) {
  if (string->IsConstant() && index->IsConstant()) {
    HConstant* c_string = HConstant::cast(string);
    HConstant* c_index = HConstant::cast(index);
    if (c_string->HasStringValue() && c_index->HasNumberValue()) {
      int32_t i = c_index->NumberValueAsInteger32();
      Handle<String> s = c_string->StringValue();
      if (i < 0 || i >= s->length()) {
        return new(zone()) HConstant(OS::nan_value());
      }
      return new(zone()) HConstant(s->Get(i));
    }
  }
  BuildCheckHeapObject(string);
  AddInstruction(HCheckInstanceType::NewIsString(string, zone()));
  HInstruction* length = HStringLength::New(zone(), string);
  AddInstruction(length);
  HInstruction* checked_index = Add<HBoundsCheck>(index, length);
  return new(zone()) HStringCharCodeAt(context, string, checked_index);
}

// Checks if the given shift amounts have form: (sa) and (32 - sa).
static bool ShiftAmountsAllowReplaceByRotate(HValue* sa,
                                             HValue* const32_minus_sa) {
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
bool HOptimizedGraphBuilder::MatchRotateRight(HValue* left,
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


HInstruction* HOptimizedGraphBuilder::BuildBinaryOperation(
    BinaryOperation* expr,
    HValue* left,
    HValue* right) {
  HValue* context = environment()->LookupContext();
  Handle<Type> left_type = expr->left()->lower_type();
  Handle<Type> right_type = expr->right()->lower_type();
  Handle<Type> result_type = expr->result_type();
  Maybe<int> fixed_right_arg = expr->fixed_right_arg();
  Representation left_rep = ToRepresentation(left_type);
  Representation right_rep = ToRepresentation(right_type);
  Representation result_rep = ToRepresentation(result_type);
  if (left_type->Is(Type::None())) {
    AddSoftDeoptimize();
    // TODO(rossberg): we should be able to get rid of non-continuous defaults.
    left_type = handle(Type::Any(), isolate());
  }
  if (right_type->Is(Type::None())) {
    AddSoftDeoptimize();
    right_type = handle(Type::Any(), isolate());
  }
  HInstruction* instr = NULL;
  switch (expr->op()) {
    case Token::ADD:
      if (left_type->Is(Type::String()) && right_type->Is(Type::String())) {
        BuildCheckHeapObject(left);
        AddInstruction(HCheckInstanceType::NewIsString(left, zone()));
        BuildCheckHeapObject(right);
        AddInstruction(HCheckInstanceType::NewIsString(right, zone()));
        instr = HStringAdd::New(zone(), context, left, right);
      } else {
        instr = HAdd::New(zone(), context, left, right);
      }
      break;
    case Token::SUB:
      instr = HSub::New(zone(), context, left, right);
      break;
    case Token::MUL:
      instr = HMul::New(zone(), context, left, right);
      break;
    case Token::MOD:
      instr = HMod::New(zone(), context, left, right, fixed_right_arg);
      break;
    case Token::DIV:
      instr = HDiv::New(zone(), context, left, right);
      break;
    case Token::BIT_XOR:
    case Token::BIT_AND:
      instr = HBitwise::New(zone(), expr->op(), context, left, right);
      break;
    case Token::BIT_OR: {
      HValue* operand, *shift_amount;
      if (left_type->Is(Type::Signed32()) &&
          right_type->Is(Type::Signed32()) &&
          MatchRotateRight(left, right, &operand, &shift_amount)) {
        instr = new(zone()) HRor(context, operand, shift_amount);
      } else {
        instr = HBitwise::New(zone(), expr->op(), context, left, right);
      }
      break;
    }
    case Token::SAR:
      instr = HSar::New(zone(), context, left, right);
      break;
    case Token::SHR:
      instr = HShr::New(zone(), context, left, right);
      if (FLAG_opt_safe_uint32_operations && instr->IsShr() &&
          CanBeZero(right)) {
        graph()->RecordUint32Instruction(instr);
      }
      break;
    case Token::SHL:
      instr = HShl::New(zone(), context, left, right);
      break;
    default:
      UNREACHABLE();
  }

  if (instr->IsBinaryOperation()) {
    HBinaryOperation* binop = HBinaryOperation::cast(instr);
    binop->set_observed_input_representation(1, left_rep);
    binop->set_observed_input_representation(2, right_rep);
    binop->initialize_output_representation(result_rep);
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
      ? new(zone()) HBranch(left_value, eval_right, empty_block, expected)
      : new(zone()) HBranch(left_value, empty_block, eval_right, expected);
    current_block()->Finish(test);

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
  HValue* right = Pop();
  HValue* left = Pop();
  HInstruction* instr = BuildBinaryOperation(expr, left, right);
  instr->set_position(expr->position());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


// TODO(rossberg): this should die eventually.
Representation HOptimizedGraphBuilder::ToRepresentation(TypeInfo info) {
  if (info.IsUninitialized()) return Representation::None();
  // TODO(verwaest): Return Smi rather than Integer32.
  if (info.IsSmi()) return Representation::Integer32();
  if (info.IsInteger32()) return Representation::Integer32();
  if (info.IsDouble()) return Representation::Double();
  if (info.IsNumber()) return Representation::Double();
  return Representation::Tagged();
}


Representation HOptimizedGraphBuilder::ToRepresentation(Handle<Type> type) {
  if (type->Is(Type::None())) return Representation::None();
  if (type->Is(Type::Signed32())) return Representation::Integer32();
  if (type->Is(Type::Number())) return Representation::Double();
  return Representation::Tagged();
}


void HOptimizedGraphBuilder::HandleLiteralCompareTypeof(CompareOperation* expr,
                                                        HTypeof* typeof_expr,
                                                        Handle<String> check) {
  // Note: The HTypeof itself is removed during canonicalization, if possible.
  HValue* value = typeof_expr->value();
  HTypeofIsAndBranch* instr = new(zone()) HTypeofIsAndBranch(value, check);
  instr->set_position(expr->position());
  return ast_context()->ReturnControl(instr, expr->id());
}


static bool MatchLiteralCompareNil(HValue* left,
                                   Token::Value op,
                                   HValue* right,
                                   Handle<Object> nil,
                                   HValue** expr) {
  if (left->IsConstant() &&
      HConstant::cast(left)->handle().is_identical_to(nil) &&
      Token::IsEqualityOp(op)) {
    *expr = right;
    return true;
  }
  return false;
}


static bool MatchLiteralCompareTypeof(HValue* left,
                                      Token::Value op,
                                      HValue* right,
                                      HTypeof** typeof_expr,
                                      Handle<String>* check) {
  if (left->IsTypeof() &&
      Token::IsEqualityOp(op) &&
      right->IsConstant() &&
      HConstant::cast(right)->handle()->IsString()) {
    *typeof_expr = HTypeof::cast(left);
    *check = Handle<String>::cast(HConstant::cast(right)->handle());
    return true;
  }
  return false;
}


static bool IsLiteralCompareTypeof(HValue* left,
                                   Token::Value op,
                                   HValue* right,
                                   HTypeof** typeof_expr,
                                   Handle<String>* check) {
  return MatchLiteralCompareTypeof(left, op, right, typeof_expr, check) ||
      MatchLiteralCompareTypeof(right, op, left, typeof_expr, check);
}


static bool IsLiteralCompareNil(HValue* left,
                                Token::Value op,
                                HValue* right,
                                Handle<Object> nil,
                                HValue** expr) {
  return MatchLiteralCompareNil(left, op, right, nil, expr) ||
      MatchLiteralCompareNil(right, op, left, nil, expr);
}


static bool IsLiteralCompareBool(HValue* left,
                                 Token::Value op,
                                 HValue* right) {
  return op == Token::EQ_STRICT &&
      ((left->IsConstant() && HConstant::cast(left)->handle()->IsBoolean()) ||
       (right->IsConstant() && HConstant::cast(right)->handle()->IsBoolean()));
}


void HOptimizedGraphBuilder::VisitCompareOperation(CompareOperation* expr) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  if (IsClassOfTest(expr)) {
    CallRuntime* call = expr->left()->AsCallRuntime();
    ASSERT(call->arguments()->length() == 1);
    CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
    HValue* value = Pop();
    Literal* literal = expr->right()->AsLiteral();
    Handle<String> rhs = Handle<String>::cast(literal->value());
    HClassOfTestAndBranch* instr =
        new(zone()) HClassOfTestAndBranch(value, rhs);
    instr->set_position(expr->position());
    return ast_context()->ReturnControl(instr, expr->id());
  }

  Handle<Type> left_type = expr->left()->lower_type();
  Handle<Type> right_type = expr->right()->lower_type();
  Handle<Type> combined_type = expr->combined_type();
  Representation combined_rep = ToRepresentation(combined_type);
  Representation left_rep = ToRepresentation(left_type);
  Representation right_rep = ToRepresentation(right_type);

  CHECK_ALIVE(VisitForValue(expr->left()));
  CHECK_ALIVE(VisitForValue(expr->right()));

  HValue* context = environment()->LookupContext();
  HValue* right = Pop();
  HValue* left = Pop();
  Token::Value op = expr->op();

  HTypeof* typeof_expr = NULL;
  Handle<String> check;
  if (IsLiteralCompareTypeof(left, op, right, &typeof_expr, &check)) {
    return HandleLiteralCompareTypeof(expr, typeof_expr, check);
  }
  HValue* sub_expr = NULL;
  Factory* f = isolate()->factory();
  if (IsLiteralCompareNil(left, op, right, f->undefined_value(), &sub_expr)) {
    return HandleLiteralCompareNil(expr, sub_expr, kUndefinedValue);
  }
  if (IsLiteralCompareNil(left, op, right, f->null_value(), &sub_expr)) {
    return HandleLiteralCompareNil(expr, sub_expr, kNullValue);
  }
  if (IsLiteralCompareBool(left, op, right)) {
    HCompareObjectEqAndBranch* result =
        new(zone()) HCompareObjectEqAndBranch(left, right);
    result->set_position(expr->position());
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
      HInstanceOf* result = new(zone()) HInstanceOf(context, left, right);
      result->set_position(expr->position());
      return ast_context()->ReturnInstruction(result, expr->id());
    } else {
      Add<HCheckFunction>(right, target);
      HInstanceOfKnownGlobal* result =
          new(zone()) HInstanceOfKnownGlobal(context, left, target);
      result->set_position(expr->position());
      return ast_context()->ReturnInstruction(result, expr->id());
    }

    // Code below assumes that we don't fall through.
    UNREACHABLE();
  } else if (op == Token::IN) {
    HIn* result = new(zone()) HIn(context, left, right);
    result->set_position(expr->position());
    return ast_context()->ReturnInstruction(result, expr->id());
  }

  // Cases handled below depend on collected type feedback. They should
  // soft deoptimize when there is no type feedback.
  if (combined_type->Is(Type::None())) {
    AddSoftDeoptimize();
    combined_type = left_type = right_type = handle(Type::Any(), isolate());
  }

  if (combined_type->Is(Type::Receiver())) {
    switch (op) {
      case Token::EQ:
      case Token::EQ_STRICT: {
        // Can we get away with map check and not instance type check?
        if (combined_type->IsClass()) {
          Handle<Map> map = combined_type->AsClass();
          AddCheckMapsWithTransitions(left, map);
          AddCheckMapsWithTransitions(right, map);
          HCompareObjectEqAndBranch* result =
              new(zone()) HCompareObjectEqAndBranch(left, right);
          result->set_position(expr->position());
          return ast_context()->ReturnControl(result, expr->id());
        } else {
          BuildCheckHeapObject(left);
          AddInstruction(HCheckInstanceType::NewIsSpecObject(left, zone()));
          BuildCheckHeapObject(right);
          AddInstruction(HCheckInstanceType::NewIsSpecObject(right, zone()));
          HCompareObjectEqAndBranch* result =
              new(zone()) HCompareObjectEqAndBranch(left, right);
          result->set_position(expr->position());
          return ast_context()->ReturnControl(result, expr->id());
        }
      }
      default:
        return Bailout("Unsupported non-primitive compare");
    }
  } else if (combined_type->Is(Type::InternalizedString()) &&
             Token::IsEqualityOp(op)) {
    BuildCheckHeapObject(left);
    AddInstruction(HCheckInstanceType::NewIsInternalizedString(left, zone()));
    BuildCheckHeapObject(right);
    AddInstruction(HCheckInstanceType::NewIsInternalizedString(right, zone()));
    HCompareObjectEqAndBranch* result =
        new(zone()) HCompareObjectEqAndBranch(left, right);
    result->set_position(expr->position());
    return ast_context()->ReturnControl(result, expr->id());
  } else {
    if (combined_rep.IsTagged() || combined_rep.IsNone()) {
      HCompareGeneric* result =
          new(zone()) HCompareGeneric(context, left, right, op);
      result->set_observed_input_representation(1, left_rep);
      result->set_observed_input_representation(2, right_rep);
      result->set_position(expr->position());
      return ast_context()->ReturnInstruction(result, expr->id());
    } else {
      // TODO(verwaest): Remove once ToRepresentation properly returns Smi when
      // the IC measures Smi.
      if (left_type->Is(Type::Smi())) left_rep = Representation::Smi();
      if (right_type->Is(Type::Smi())) right_rep = Representation::Smi();
      HCompareIDAndBranch* result =
          new(zone()) HCompareIDAndBranch(left, right, op);
      result->set_observed_input_representation(left_rep, right_rep);
      result->set_position(expr->position());
      return ast_context()->ReturnControl(result, expr->id());
    }
  }
}


void HOptimizedGraphBuilder::HandleLiteralCompareNil(CompareOperation* expr,
                                                     HValue* value,
                                                     NilValue nil) {
  ASSERT(!HasStackOverflow());
  ASSERT(current_block() != NULL);
  ASSERT(current_block()->HasPredecessor());
  ASSERT(expr->op() == Token::EQ || expr->op() == Token::EQ_STRICT);
  HIfContinuation continuation;
  if (expr->op() == Token::EQ_STRICT) {
    IfBuilder if_nil(this);
    if_nil.If<HCompareObjectEqAndBranch>(
        value, (nil == kNullValue) ? graph()->GetConstantNull()
                                   : graph()->GetConstantUndefined());
    if_nil.Then();
    if_nil.Else();
    if_nil.CaptureContinuation(&continuation);
    return ast_context()->ReturnContinuation(&continuation, expr->id());
  }
  Handle<Type> type = expr->combined_type()->Is(Type::None())
      ? handle(Type::Any(), isolate_) : expr->combined_type();
  BuildCompareNil(value, type, expr->position(), &continuation);
  return ast_context()->ReturnContinuation(&continuation, expr->id());
}


HInstruction* HOptimizedGraphBuilder::BuildThisFunction() {
  // If we share optimized code between different closures, the
  // this-function is not a constant, except inside an inlined body.
  if (function_state()->outer() != NULL) {
      return new(zone()) HConstant(
          function_state()->compilation_info()->closure());
  } else {
      return new(zone()) HThisFunction;
  }
}


HInstruction* HOptimizedGraphBuilder::BuildFastLiteral(
    HValue* context,
    Handle<JSObject> boilerplate_object,
    Handle<JSObject> original_boilerplate_object,
    int data_size,
    int pointer_size,
    AllocationSiteMode mode) {
  NoObservableSideEffectsScope no_effects(this);

  HInstruction* target = NULL;
  HInstruction* data_target = NULL;

  HAllocate::Flags flags = HAllocate::DefaultFlags();

  if (isolate()->heap()->ShouldGloballyPretenure()) {
    if (data_size != 0) {
      HAllocate::Flags data_flags =
          static_cast<HAllocate::Flags>(HAllocate::DefaultFlags() |
              HAllocate::CAN_ALLOCATE_IN_OLD_DATA_SPACE);
      HValue* size_in_bytes = Add<HConstant>(data_size);
      data_target = Add<HAllocate>(context, size_in_bytes,
                                   HType::JSObject(), data_flags);
      Handle<Map> free_space_map = isolate()->factory()->free_space_map();
      AddStoreMapConstant(data_target, free_space_map);
      HObjectAccess access =
          HObjectAccess::ForJSObjectOffset(FreeSpace::kSizeOffset);
      AddStore(data_target, access, size_in_bytes);
    }
    if (pointer_size != 0) {
      flags = static_cast<HAllocate::Flags>(
          flags | HAllocate::CAN_ALLOCATE_IN_OLD_POINTER_SPACE);
      HValue* size_in_bytes = Add<HConstant>(pointer_size);
      target = Add<HAllocate>(context, size_in_bytes, HType::JSObject(), flags);
    }
  } else {
    HValue* size_in_bytes = Add<HConstant>(data_size + pointer_size);
    target = Add<HAllocate>(context, size_in_bytes, HType::JSObject(), flags);
  }

  int offset = 0;
  int data_offset = 0;
  BuildEmitDeepCopy(boilerplate_object, original_boilerplate_object, target,
                    &offset, data_target, &data_offset, mode);
  return target;
}


void HOptimizedGraphBuilder::BuildEmitDeepCopy(
    Handle<JSObject> boilerplate_object,
    Handle<JSObject> original_boilerplate_object,
    HInstruction* target,
    int* offset,
    HInstruction* data_target,
    int* data_offset,
    AllocationSiteMode mode) {
  Handle<FixedArrayBase> elements(boilerplate_object->elements());
  Handle<FixedArrayBase> original_elements(
      original_boilerplate_object->elements());
  ElementsKind kind = boilerplate_object->map()->elements_kind();

  int object_offset = *offset;
  int object_size = boilerplate_object->map()->instance_size();
  int elements_size = (elements->length() > 0 &&
      elements->map() != isolate()->heap()->fixed_cow_array_map()) ?
          elements->Size() : 0;
  int elements_offset = 0;

  if (data_target != NULL && boilerplate_object->HasFastDoubleElements()) {
    elements_offset = *data_offset;
    *data_offset += elements_size;
  } else {
    // Place elements right after this object.
    elements_offset = *offset + object_size;
    *offset += elements_size;
  }
  // Increase the offset so that subsequent objects end up right after this
  // object (and it's elements if they are allocated in the same space).
  *offset += object_size;

  // Copy object elements if non-COW.
  HValue* object_elements = BuildEmitObjectHeader(boilerplate_object, target,
      data_target, object_offset, elements_offset, elements_size);
  if (object_elements != NULL) {
    BuildEmitElements(elements, original_elements, kind, object_elements,
        target, offset, data_target, data_offset);
  }

  // Copy in-object properties.
  if (boilerplate_object->map()->NumberOfFields() != 0) {
    HValue* object_properties =
        Add<HInnerAllocatedObject>(target, object_offset);
    BuildEmitInObjectProperties(boilerplate_object, original_boilerplate_object,
        object_properties, target, offset, data_target, data_offset);
  }

  // Create allocation site info.
  if (mode == TRACK_ALLOCATION_SITE &&
      boilerplate_object->map()->CanTrackAllocationSite()) {
    elements_offset += AllocationSiteInfo::kSize;
    *offset += AllocationSiteInfo::kSize;
    HInstruction* original_boilerplate =
        Add<HConstant>(original_boilerplate_object);
    BuildCreateAllocationSiteInfo(target, JSArray::kSize, original_boilerplate);
  }
}


HValue* HOptimizedGraphBuilder::BuildEmitObjectHeader(
    Handle<JSObject> boilerplate_object,
    HInstruction* target,
    HInstruction* data_target,
    int object_offset,
    int elements_offset,
    int elements_size) {
  ASSERT(boilerplate_object->properties()->length() == 0);
  HValue* result = NULL;

  HValue* object_header = Add<HInnerAllocatedObject>(target, object_offset);
  Handle<Map> boilerplate_object_map(boilerplate_object->map());
  AddStoreMapConstant(object_header, boilerplate_object_map);

  HInstruction* elements;
  if (elements_size == 0) {
    Handle<Object> elements_field =
        Handle<Object>(boilerplate_object->elements(), isolate());
    elements = Add<HConstant>(elements_field);
  } else {
    if (data_target != NULL && boilerplate_object->HasFastDoubleElements()) {
      elements = Add<HInnerAllocatedObject>(data_target, elements_offset);
    } else {
      elements = Add<HInnerAllocatedObject>(target, elements_offset);
    }
    result = elements;
  }
  AddStore(object_header, HObjectAccess::ForElementsPointer(), elements);

  Handle<Object> properties_field =
      Handle<Object>(boilerplate_object->properties(), isolate());
  ASSERT(*properties_field == isolate()->heap()->empty_fixed_array());
  HInstruction* properties = Add<HConstant>(properties_field);
  HObjectAccess access = HObjectAccess::ForPropertiesPointer();
  AddStore(object_header, access, properties);

  if (boilerplate_object->IsJSArray()) {
    Handle<JSArray> boilerplate_array =
        Handle<JSArray>::cast(boilerplate_object);
    Handle<Object> length_field =
        Handle<Object>(boilerplate_array->length(), isolate());
    HInstruction* length = Add<HConstant>(length_field);

    ASSERT(boilerplate_array->length()->IsSmi());
    Representation representation =
        IsFastElementsKind(boilerplate_array->GetElementsKind())
        ? Representation::Smi() : Representation::Tagged();
    AddStore(object_header, HObjectAccess::ForArrayLength(),
        length, representation);
  }

  return result;
}


void HOptimizedGraphBuilder::BuildEmitInObjectProperties(
    Handle<JSObject> boilerplate_object,
    Handle<JSObject> original_boilerplate_object,
    HValue* object_properties,
    HInstruction* target,
    int* offset,
    HInstruction* data_target,
    int* data_offset) {
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
      Handle<JSObject> original_value_object = Handle<JSObject>::cast(
          Handle<Object>(original_boilerplate_object->InObjectPropertyAt(index),
              isolate()));
      HInstruction* value_instruction = Add<HInnerAllocatedObject>(target,
                                                                   *offset);

      AddStore(object_properties, access, value_instruction);

      BuildEmitDeepCopy(value_object, original_value_object, target,
          offset, data_target, data_offset, DONT_TRACK_ALLOCATION_SITE);
    } else {
      Representation representation = details.representation();
      HInstruction* value_instruction = Add<HConstant>(value);

      if (representation.IsDouble()) {
        // Allocate a HeapNumber box and store the value into it.
        HInstruction* double_box;
        if (data_target != NULL) {
          double_box = Add<HInnerAllocatedObject>(data_target, *data_offset);
          *data_offset += HeapNumber::kSize;
        } else {
          double_box = Add<HInnerAllocatedObject>(target, *offset);
          *offset += HeapNumber::kSize;
        }
        AddStoreMapConstant(double_box,
            isolate()->factory()->heap_number_map());
        AddStore(double_box, HObjectAccess::ForHeapNumberValue(),
            value_instruction, Representation::Double());
        value_instruction = double_box;
      }

      AddStore(object_properties, access, value_instruction);
    }
  }

  int inobject_properties = boilerplate_object->map()->inobject_properties();
  HInstruction* value_instruction =
      Add<HConstant>(isolate()->factory()->one_pointer_filler_map());
  for (int i = copied_fields; i < inobject_properties; i++) {
    ASSERT(boilerplate_object->IsJSObject());
    int property_offset = boilerplate_object->GetInObjectPropertyOffset(i);
    HObjectAccess access = HObjectAccess::ForJSObjectOffset(property_offset);
    AddStore(object_properties, access, value_instruction);
  }
}


void HOptimizedGraphBuilder::BuildEmitElements(
    Handle<FixedArrayBase> elements,
    Handle<FixedArrayBase> original_elements,
    ElementsKind kind,
    HValue* object_elements,
    HInstruction* target,
    int* offset,
    HInstruction* data_target,
    int* data_offset) {
  int elements_length = elements->length();
  HValue* object_elements_length = Add<HConstant>(elements_length);

  BuildInitializeElementsHeader(object_elements, kind, object_elements_length);

  // Copy elements backing store content.
  if (elements->IsFixedDoubleArray()) {
    BuildEmitFixedDoubleArray(elements, kind, object_elements);
  } else if (elements->IsFixedArray()) {
    BuildEmitFixedArray(elements, original_elements, kind, object_elements,
        target, offset, data_target, data_offset);
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
    Handle<FixedArrayBase> original_elements,
    ElementsKind kind,
    HValue* object_elements,
    HInstruction* target,
    int* offset,
    HInstruction* data_target,
    int* data_offset) {
  HInstruction* boilerplate_elements = Add<HConstant>(elements);
  int elements_length = elements->length();
  Handle<FixedArray> fast_elements = Handle<FixedArray>::cast(elements);
  Handle<FixedArray> original_fast_elements =
      Handle<FixedArray>::cast(original_elements);
  for (int i = 0; i < elements_length; i++) {
    Handle<Object> value(fast_elements->get(i), isolate());
    HValue* key_constant = Add<HConstant>(i);
    if (value->IsJSObject()) {
      Handle<JSObject> value_object = Handle<JSObject>::cast(value);
      Handle<JSObject> original_value_object = Handle<JSObject>::cast(
          Handle<Object>(original_fast_elements->get(i), isolate()));
      HInstruction* value_instruction = Add<HInnerAllocatedObject>(target,
                                                                   *offset);
      Add<HStoreKeyed>(object_elements, key_constant, value_instruction, kind);
      BuildEmitDeepCopy(value_object, original_value_object, target,
          offset, data_target, data_offset, DONT_TRACK_ALLOCATION_SITE);
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
    Add<HDeclareGlobals>(environment()->LookupContext(), array, flags);
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
        HValue* context = environment()->LookupContext();
        HStoreContextSlot* store = Add<HStoreContextSlot>(
            context, variable->index(), HStoreContextSlot::kNoCheck, value);
        if (store->HasObservableSideEffects()) {
          AddSimulate(proxy->id(), REMOVABLE_SIMULATE);
        }
      }
      break;
    case Variable::LOOKUP:
      return Bailout("unsupported lookup slot in declaration");
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
      HValue* context = environment()->LookupContext();
      HStoreContextSlot* store = Add<HStoreContextSlot>(
          context, variable->index(), HStoreContextSlot::kNoCheck, value);
      if (store->HasObservableSideEffects()) {
        AddSimulate(proxy->id(), REMOVABLE_SIMULATE);
      }
      break;
    }
    case Variable::LOOKUP:
      return Bailout("unsupported lookup slot in declaration");
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
  HIsSmiAndBranch* result = new(zone()) HIsSmiAndBranch(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsSpecObject(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      new(zone()) HHasInstanceTypeAndBranch(value,
                                            FIRST_SPEC_OBJECT_TYPE,
                                            LAST_SPEC_OBJECT_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsFunction(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      new(zone()) HHasInstanceTypeAndBranch(value, JS_FUNCTION_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateHasCachedArrayIndex(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasCachedArrayIndexAndBranch* result =
      new(zone()) HHasCachedArrayIndexAndBranch(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsArray(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      new(zone()) HHasInstanceTypeAndBranch(value, JS_ARRAY_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsRegExp(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      new(zone()) HHasInstanceTypeAndBranch(value, JS_REGEXP_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsObject(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HIsObjectAndBranch* result = new(zone()) HIsObjectAndBranch(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsNonNegativeSmi(CallRuntime* call) {
  return Bailout("inlined runtime function: IsNonNegativeSmi");
}


void HOptimizedGraphBuilder::GenerateIsUndetectableObject(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HIsUndetectableAndBranch* result =
      new(zone()) HIsUndetectableAndBranch(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsStringWrapperSafeForDefaultValueOf(
    CallRuntime* call) {
  return Bailout(
      "inlined runtime function: IsStringWrapperSafeForDefaultValueOf");
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
    return ast_context()->ReturnControl(new(zone()) HIsConstructCallAndBranch,
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
  HArgumentsLength* result = new(zone()) HArgumentsLength(elements);
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
  HAccessArgumentsAt* result =
      new(zone()) HAccessArgumentsAt(elements, length, checked_index);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Support for accessing the class and value fields of an object.
void HOptimizedGraphBuilder::GenerateClassOf(CallRuntime* call) {
  // The special form detected by IsClassOfTest is detected before we get here
  // and does not cause a bailout.
  return Bailout("inlined runtime function: ClassOf");
}


void HOptimizedGraphBuilder::GenerateValueOf(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HValueOf* result = new(zone()) HValueOf(value);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateDateField(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 2);
  ASSERT_NE(NULL, call->arguments()->at(1)->AsLiteral());
  Smi* index = Smi::cast(*(call->arguments()->at(1)->AsLiteral()->value()));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* date = Pop();
  HDateField* result = new(zone()) HDateField(date, index);
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
  HSeqStringSetChar* result = new(zone()) HSeqStringSetChar(
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
  HSeqStringSetChar* result = new(zone()) HSeqStringSetChar(
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
  HIsSmiAndBranch* smicheck = new(zone()) HIsSmiAndBranch(object);
  HBasicBlock* if_smi = graph()->CreateBasicBlock();
  HBasicBlock* if_heap_object = graph()->CreateBasicBlock();
  HBasicBlock* join = graph()->CreateBasicBlock();
  smicheck->SetSuccessorAt(0, if_smi);
  smicheck->SetSuccessorAt(1, if_heap_object);
  current_block()->Finish(smicheck);
  if_smi->Goto(join);

  // Check if object is a JSValue.
  set_current_block(if_heap_object);
  HHasInstanceTypeAndBranch* typecheck =
      new(zone()) HHasInstanceTypeAndBranch(object, JS_VALUE_TYPE);
  HBasicBlock* if_js_value = graph()->CreateBasicBlock();
  HBasicBlock* not_js_value = graph()->CreateBasicBlock();
  typecheck->SetSuccessorAt(0, if_js_value);
  typecheck->SetSuccessorAt(1, not_js_value);
  current_block()->Finish(typecheck);
  not_js_value->Goto(join);

  // Create in-object property store to kValueOffset.
  set_current_block(if_js_value);
  AddStore(object,
      HObjectAccess::ForJSObjectOffset(JSValue::kValueOffset), value);
  if_js_value->Goto(join);
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
  HValue* context = environment()->LookupContext();
  HInstruction* result = BuildStringCharCodeAt(context, string, index);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for string.charAt(n) and string[n].
void HOptimizedGraphBuilder::GenerateStringCharFromCode(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* char_code = Pop();
  HValue* context = environment()->LookupContext();
  HInstruction* result = HStringCharFromCode::New(zone(), context, char_code);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for string.charAt(n) and string[n].
void HOptimizedGraphBuilder::GenerateStringCharAt(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 2);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* index = Pop();
  HValue* string = Pop();
  HValue* context = environment()->LookupContext();
  HInstruction* char_code = BuildStringCharCodeAt(context, string, index);
  AddInstruction(char_code);
  HInstruction* result = HStringCharFromCode::New(zone(), context, char_code);
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
      new(zone()) HCompareObjectEqAndBranch(left, right);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateLog(CallRuntime* call) {
  // %_Log is ignored in optimized code.
  return ast_context()->ReturnValue(graph()->GetConstantUndefined());
}


// Fast support for Math.random().
void HOptimizedGraphBuilder::GenerateRandomHeapNumber(CallRuntime* call) {
  HValue* context = environment()->LookupContext();
  HGlobalObject* global_object = Add<HGlobalObject>(context);
  HRandom* result = new(zone()) HRandom(global_object);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for StringAdd.
void HOptimizedGraphBuilder::GenerateStringAdd(CallRuntime* call) {
  ASSERT_EQ(2, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result = new(zone()) HCallStub(context, CodeStub::StringAdd, 2);
  Drop(2);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for SubString.
void HOptimizedGraphBuilder::GenerateSubString(CallRuntime* call) {
  ASSERT_EQ(3, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result = new(zone()) HCallStub(context, CodeStub::SubString, 3);
  Drop(3);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for StringCompare.
void HOptimizedGraphBuilder::GenerateStringCompare(CallRuntime* call) {
  ASSERT_EQ(2, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result =
      new(zone()) HCallStub(context, CodeStub::StringCompare, 2);
  Drop(2);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Support for direct calls from JavaScript to native RegExp code.
void HOptimizedGraphBuilder::GenerateRegExpExec(CallRuntime* call) {
  ASSERT_EQ(4, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result = new(zone()) HCallStub(context, CodeStub::RegExpExec, 4);
  Drop(4);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Construct a RegExp exec result with two in-object properties.
void HOptimizedGraphBuilder::GenerateRegExpConstructResult(CallRuntime* call) {
  ASSERT_EQ(3, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result =
      new(zone()) HCallStub(context, CodeStub::RegExpConstructResult, 3);
  Drop(3);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Support for fast native caches.
void HOptimizedGraphBuilder::GenerateGetFromCache(CallRuntime* call) {
  return Bailout("inlined runtime function: GetFromCache");
}


// Fast support for number to string.
void HOptimizedGraphBuilder::GenerateNumberToString(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result =
      new(zone()) HCallStub(context, CodeStub::NumberToString, 1);
  Drop(1);
  return ast_context()->ReturnInstruction(result, call->id());
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
  HValue* context = environment()->LookupContext();

  // Branch for function proxies, or other non-functions.
  HHasInstanceTypeAndBranch* typecheck =
      new(zone()) HHasInstanceTypeAndBranch(function, JS_FUNCTION_TYPE);
  HBasicBlock* if_jsfunction = graph()->CreateBasicBlock();
  HBasicBlock* if_nonfunction = graph()->CreateBasicBlock();
  HBasicBlock* join = graph()->CreateBasicBlock();
  typecheck->SetSuccessorAt(0, if_jsfunction);
  typecheck->SetSuccessorAt(1, if_nonfunction);
  current_block()->Finish(typecheck);

  set_current_block(if_jsfunction);
  HInstruction* invoke_result =
      Add<HInvokeFunction>(context, function, arg_count);
  Drop(arg_count);
  Push(invoke_result);
  if_jsfunction->Goto(join);

  set_current_block(if_nonfunction);
  HInstruction* call_result = Add<HCallFunction>(context, function, arg_count);
  Drop(arg_count);
  Push(call_result);
  if_nonfunction->Goto(join);

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
  HInstruction* result = HPower::New(zone(), left, right);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathSin(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result =
      new(zone()) HCallStub(context, CodeStub::TranscendentalCache, 1);
  result->set_transcendental_type(TranscendentalCache::SIN);
  Drop(1);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathCos(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result =
      new(zone()) HCallStub(context, CodeStub::TranscendentalCache, 1);
  result->set_transcendental_type(TranscendentalCache::COS);
  Drop(1);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathTan(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result =
      new(zone()) HCallStub(context, CodeStub::TranscendentalCache, 1);
  result->set_transcendental_type(TranscendentalCache::TAN);
  Drop(1);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathLog(CallRuntime* call) {
  ASSERT_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitArgumentList(call->arguments()));
  HValue* context = environment()->LookupContext();
  HCallStub* result =
      new(zone()) HCallStub(context, CodeStub::TranscendentalCache, 1);
  result->set_transcendental_type(TranscendentalCache::LOG);
  Drop(1);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateMathSqrt(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HValue* context = environment()->LookupContext();
  HInstruction* result =
      HUnaryMathOperation::New(zone(), context, value, kMathSqrt);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Check whether two RegExps are equivalent
void HOptimizedGraphBuilder::GenerateIsRegExpEquivalent(CallRuntime* call) {
  return Bailout("inlined runtime function: IsRegExpEquivalent");
}


void HOptimizedGraphBuilder::GenerateGetCachedArrayIndex(CallRuntime* call) {
  ASSERT(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HGetCachedArrayIndex* result = new(zone()) HGetCachedArrayIndex(value);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateFastAsciiArrayJoin(CallRuntime* call) {
  return Bailout("inlined runtime function: FastAsciiArrayJoin");
}


// Support for generators.
void HOptimizedGraphBuilder::GenerateGeneratorNext(CallRuntime* call) {
  return Bailout("inlined runtime function: GeneratorNext");
}


void HOptimizedGraphBuilder::GenerateGeneratorThrow(CallRuntime* call) {
  return Bailout("inlined runtime function: GeneratorThrow");
}


void HOptimizedGraphBuilder::GenerateDebugBreakInOptimizedCode(
    CallRuntime* call) {
  AddInstruction(new(zone()) HDebugBreak());
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
      ASSERT(phi->merged_index() == i);
      ASSERT(phi->OperandCount() == block->predecessors()->length());
      phi->AddInput(other->values_[i]);
    } else if (values_[i] != other->values_[i]) {
      // There is a fresh value on the incoming edge, a phi is needed.
      ASSERT(values_[i] != NULL && other->values_[i] != NULL);
      HPhi* phi = new(zone()) HPhi(i, zone());
      HValue* old_value = values_[i];
      for (int j = 0; j < block->predecessors()->length(); j++) {
        phi->AddInput(old_value);
      }
      phi->AddInput(other->values_[i]);
      this->values_[i] = phi;
      block->AddPhi(phi);
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
    HPhi* phi = new(zone()) HPhi(i, zone());
    phi->AddInput(values_[i]);
    new_env->values_[i] = phi;
    loop_header->AddPhi(phi);
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
  inner->SetValueAt(arity + 1, LookupContext());
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
  ASSERT(!FLAG_parallel_recompilation);
  AllowHandleDereference allow_deref;
  AllowDeferredHandleDereference allow_deferred_deref;
  Trace(name, chunk->graph(), chunk);
}


void HTracer::TraceHydrogen(const char* name, HGraph* graph) {
  ASSERT(!FLAG_parallel_recompilation);
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
  int64_t sum = 0;
  for (int i = 0; i < timing_.length(); ++i) {
    sum += timing_[i];
  }

  for (int i = 0; i < names_.length(); ++i) {
    PrintF("%32s", names_[i]);
    double ms = static_cast<double>(timing_[i]) / 1000;
    double percent = static_cast<double>(timing_[i]) * 100 / sum;
    PrintF(" %8.3f ms / %4.1f %% ", ms, percent);

    unsigned size = sizes_[i];
    double size_percent = static_cast<double>(size) * 100 / total_size_;
    PrintF(" %9u bytes / %4.1f %%\n", size, size_percent);
  }

  PrintF("----------------------------------------"
         "---------------------------------------\n");
  int64_t total = create_graph_ + optimize_graph_ + generate_code_;
  PrintF("%32s %8.3f ms / %4.1f %% \n",
         "Create graph",
         static_cast<double>(create_graph_) / 1000,
         static_cast<double>(create_graph_) * 100 / total);
  PrintF("%32s %8.3f ms / %4.1f %% \n",
         "Optimize graph",
         static_cast<double>(optimize_graph_) / 1000,
         static_cast<double>(optimize_graph_) * 100 / total);
  PrintF("%32s %8.3f ms / %4.1f %% \n",
         "Generate and install code",
         static_cast<double>(generate_code_) / 1000,
         static_cast<double>(generate_code_) * 100 / total);
  PrintF("----------------------------------------"
         "---------------------------------------\n");
  PrintF("%32s %8.3f ms (%.1f times slower than full code gen)\n",
         "Total",
         static_cast<double>(total) / 1000,
         static_cast<double>(total) / full_code_gen_);

  double source_size_in_kb = static_cast<double>(source_size_) / 1024;
  double normalized_time =  source_size_in_kb > 0
      ? (static_cast<double>(total) / 1000) / source_size_in_kb
      : 0;
  double normalized_size_in_kb = source_size_in_kb > 0
      ? total_size_ / 1024 / source_size_in_kb
      : 0;
  PrintF("%32s %8.3f ms           %7.3f kB allocated\n",
         "Average per kB source",
         normalized_time, normalized_size_in_kb);
}


void HStatistics::SaveTiming(const char* name, int64_t ticks, unsigned size) {
  total_size_ += size;
  for (int i = 0; i < names_.length(); ++i) {
    if (strcmp(names_[i], name) == 0) {
      timing_[i] += ticks;
      sizes_[i] += size;
      return;
    }
  }
  names_.Add(name);
  timing_.Add(ticks);
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
