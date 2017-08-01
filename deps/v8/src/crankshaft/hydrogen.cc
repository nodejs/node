// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen.h"

#include <memory>
#include <sstream>

#include "src/allocation-site-scopes.h"
#include "src/ast/ast-numbering.h"
#include "src/ast/compile-time-value.h"
#include "src/ast/scopes.h"
#include "src/code-factory.h"
#include "src/crankshaft/hydrogen-bce.h"
#include "src/crankshaft/hydrogen-canonicalize.h"
#include "src/crankshaft/hydrogen-check-elimination.h"
#include "src/crankshaft/hydrogen-dce.h"
#include "src/crankshaft/hydrogen-dehoist.h"
#include "src/crankshaft/hydrogen-environment-liveness.h"
#include "src/crankshaft/hydrogen-escape-analysis.h"
#include "src/crankshaft/hydrogen-gvn.h"
#include "src/crankshaft/hydrogen-infer-representation.h"
#include "src/crankshaft/hydrogen-infer-types.h"
#include "src/crankshaft/hydrogen-load-elimination.h"
#include "src/crankshaft/hydrogen-mark-unreachable.h"
#include "src/crankshaft/hydrogen-osr.h"
#include "src/crankshaft/hydrogen-range-analysis.h"
#include "src/crankshaft/hydrogen-redundant-phi.h"
#include "src/crankshaft/hydrogen-removable-simulates.h"
#include "src/crankshaft/hydrogen-representation-changes.h"
#include "src/crankshaft/hydrogen-sce.h"
#include "src/crankshaft/hydrogen-store-elimination.h"
#include "src/crankshaft/hydrogen-uint32-analysis.h"
#include "src/crankshaft/lithium-allocator.h"
#include "src/crankshaft/typing.h"
#include "src/field-type.h"
#include "src/full-codegen/full-codegen.h"
#include "src/globals.h"
#include "src/ic/call-optimization.h"
#include "src/ic/ic.h"
// GetRootConstructor
#include "src/ic/ic-inl.h"
#include "src/isolate-inl.h"
#include "src/objects/map.h"
#include "src/runtime/runtime.h"

#if V8_TARGET_ARCH_IA32
#include "src/crankshaft/ia32/lithium-codegen-ia32.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/crankshaft/x64/lithium-codegen-x64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/crankshaft/arm64/lithium-codegen-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/crankshaft/arm/lithium-codegen-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_PPC
#include "src/crankshaft/ppc/lithium-codegen-ppc.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/crankshaft/mips/lithium-codegen-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/crankshaft/mips64/lithium-codegen-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_S390
#include "src/crankshaft/s390/lithium-codegen-s390.h"  // NOLINT
#elif V8_TARGET_ARCH_X87
#include "src/crankshaft/x87/lithium-codegen-x87.h"  // NOLINT
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {

const auto GetRegConfig = RegisterConfiguration::Crankshaft;

class HOptimizedGraphBuilderWithPositions : public HOptimizedGraphBuilder {
 public:
  explicit HOptimizedGraphBuilderWithPositions(CompilationInfo* info)
      : HOptimizedGraphBuilder(info, true) {
    SetSourcePosition(info->shared_info()->start_position());
  }

#define DEF_VISIT(type)                                      \
  void Visit##type(type* node) override {                    \
    SourcePosition old_position = SourcePosition::Unknown(); \
    if (node->position() != kNoSourcePosition) {             \
      old_position = source_position();                      \
      SetSourcePosition(node->position());                   \
    }                                                        \
    HOptimizedGraphBuilder::Visit##type(node);               \
    if (old_position.IsKnown()) {                            \
      set_source_position(old_position);                     \
    }                                                        \
  }
  EXPRESSION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

#define DEF_VISIT(type)                                      \
  void Visit##type(type* node) override {                    \
    SourcePosition old_position = SourcePosition::Unknown(); \
    if (node->position() != kNoSourcePosition) {             \
      old_position = source_position();                      \
      SetSourcePosition(node->position());                   \
    }                                                        \
    HOptimizedGraphBuilder::Visit##type(node);               \
    if (old_position.IsKnown()) {                            \
      set_source_position(old_position);                     \
    }                                                        \
  }
  STATEMENT_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

#define DEF_VISIT(type)                        \
  void Visit##type(type* node) override {      \
    HOptimizedGraphBuilder::Visit##type(node); \
  }
  DECLARATION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT
};

HCompilationJob::Status HCompilationJob::PrepareJobImpl() {
  if (!isolate()->use_optimizer() ||
      info()->shared_info()->must_use_ignition_turbo()) {
    // Crankshaft is entirely disabled.
    return FAILED;
  }

  // Optimization requires a version of fullcode with deoptimization support.
  // Recompile the unoptimized version of the code if the current version
  // doesn't have deoptimization support already.
  // Otherwise, if we are gathering compilation time and space statistics
  // for hydrogen, gather baseline statistics for a fullcode compilation.
  bool should_recompile = !info()->shared_info()->has_deoptimization_support();
  if (should_recompile || FLAG_hydrogen_stats) {
    base::ElapsedTimer timer;
    if (FLAG_hydrogen_stats) {
      timer.Start();
    }
    if (!Compiler::EnsureDeoptimizationSupport(info())) {
      return FAILED;
    }
    if (FLAG_hydrogen_stats) {
      isolate()->GetHStatistics()->IncrementFullCodeGen(timer.Elapsed());
    }
  }
  DCHECK(info()->shared_info()->has_deoptimization_support());

  // Check the whitelist for Crankshaft.
  if (!info()->shared_info()->PassesFilter(FLAG_hydrogen_filter)) {
    return AbortOptimization(kHydrogenFilter);
  }

  Scope* scope = info()->scope();
  if (LUnallocated::TooManyParameters(scope->num_parameters())) {
    // Crankshaft would require too many Lithium operands.
    return AbortOptimization(kTooManyParameters);
  }

  if (info()->is_osr() &&
      LUnallocated::TooManyParametersOrStackSlots(scope->num_parameters(),
                                                  scope->num_stack_slots())) {
    // Crankshaft would require too many Lithium operands.
    return AbortOptimization(kTooManyParametersLocals);
  }

  if (IsGeneratorFunction(info()->shared_info()->kind())) {
    // Crankshaft does not support generators.
    return AbortOptimization(kGenerator);
  }

  if (FLAG_trace_hydrogen) {
    isolate()->GetHTracer()->TraceCompilation(info());
  }

  // Optimization could have been disabled by the parser. Note that this check
  // is only needed because the Hydrogen graph builder is missing some bailouts.
  if (info()->shared_info()->optimization_disabled()) {
    return AbortOptimization(
        info()->shared_info()->disable_optimization_reason());
  }

  HOptimizedGraphBuilder* graph_builder =
      (FLAG_hydrogen_track_positions || isolate()->is_profiling() ||
       FLAG_trace_ic)
          ? new (info()->zone()) HOptimizedGraphBuilderWithPositions(info())
          : new (info()->zone()) HOptimizedGraphBuilder(info(), false);

  // Type-check the function.
  AstTyper(info()->isolate(), info()->zone(), info()->closure(),
           info()->scope(), info()->osr_ast_id(), info()->literal(),
           graph_builder->bounds())
      .Run();

  graph_ = graph_builder->CreateGraph();

  if (isolate()->has_pending_exception()) {
    return FAILED;
  }

  if (graph_ == NULL) return FAILED;

  if (info()->dependencies()->HasAborted()) {
    // Dependency has changed during graph creation. Let's try again later.
    return RetryOptimization(kBailedOutDueToDependencyChange);
  }

  return SUCCEEDED;
}

HCompilationJob::Status HCompilationJob::ExecuteJobImpl() {
  DCHECK(graph_ != NULL);
  BailoutReason bailout_reason = kNoReason;

  if (graph_->Optimize(&bailout_reason)) {
    chunk_ = LChunk::NewChunk(graph_);
    if (chunk_ != NULL) return SUCCEEDED;
  } else if (bailout_reason != kNoReason) {
    info()->AbortOptimization(bailout_reason);
  }

  return FAILED;
}

HCompilationJob::Status HCompilationJob::FinalizeJobImpl() {
  DCHECK(chunk_ != NULL);
  DCHECK(graph_ != NULL);
  {
    // Deferred handles reference objects that were accessible during
    // graph creation.  To make sure that we don't encounter inconsistencies
    // between graph creation and code generation, we disallow accessing
    // objects through deferred handles during the latter, with exceptions.
    DisallowDeferredHandleDereference no_deferred_handle_deref;
    Handle<Code> optimized_code = chunk_->Codegen();
    if (optimized_code.is_null()) {
      if (info()->bailout_reason() == kNoReason) {
        return AbortOptimization(kCodeGenerationFailed);
      }
      return FAILED;
    }
    RegisterWeakObjectsInOptimizedCode(optimized_code);
    info()->SetCode(optimized_code);
  }
  // Add to the weak list of optimized code objects.
  info()->context()->native_context()->AddOptimizedCode(*info()->code());
  return SUCCEEDED;
}

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
      is_osr_entry_(false),
      is_ordered_(false) { }


Isolate* HBasicBlock::isolate() const {
  return graph_->isolate();
}


void HBasicBlock::MarkUnreachable() {
  is_reachable_ = false;
}


void HBasicBlock::AttachLoopInformation() {
  DCHECK(!IsLoopHeader());
  loop_information_ = new(zone()) HLoopInformation(this, zone());
}


void HBasicBlock::DetachLoopInformation() {
  DCHECK(IsLoopHeader());
  loop_information_ = NULL;
}


void HBasicBlock::AddPhi(HPhi* phi) {
  DCHECK(!IsStartBlock());
  phis_.Add(phi, zone());
  phi->SetBlock(this);
}


void HBasicBlock::RemovePhi(HPhi* phi) {
  DCHECK(phi->block() == this);
  DCHECK(phis_.Contains(phi));
  phi->Kill();
  phis_.RemoveElement(phi);
  phi->SetBlock(NULL);
}


void HBasicBlock::AddInstruction(HInstruction* instr, SourcePosition position) {
  DCHECK(!IsStartBlock() || !IsFinished());
  DCHECK(!instr->IsLinked());
  DCHECK(!IsFinished());

  if (position.IsKnown()) {
    instr->set_position(position);
  }
  if (first_ == NULL) {
    DCHECK(last_environment() != NULL);
    DCHECK(!last_environment()->ast_id().IsNone());
    HBlockEntry* entry = new(zone()) HBlockEntry();
    entry->InitializeAsFirst(this);
    if (position.IsKnown()) {
      entry->set_position(position);
    } else {
      DCHECK(!FLAG_hydrogen_track_positions ||
             !graph()->info()->IsOptimizing() || instr->IsAbnormalExit());
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
  DCHECK(HasEnvironment());
  HEnvironment* environment = last_environment();
  DCHECK(ast_id.IsNone() ||
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


void HBasicBlock::Finish(HControlInstruction* end, SourcePosition position) {
  DCHECK(!IsFinished());
  AddInstruction(end, position);
  end_ = end;
  for (HSuccessorIterator it(end); !it.Done(); it.Advance()) {
    it.Current()->RegisterPredecessor(this);
  }
}


void HBasicBlock::Goto(HBasicBlock* block, SourcePosition position,
                       FunctionState* state, bool add_simulate) {
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


void HBasicBlock::AddLeaveInlined(HValue* return_value, FunctionState* state,
                                  SourcePosition position) {
  HBasicBlock* target = state->function_return();
  bool drop_extra = state->inlining_kind() == NORMAL_RETURN;

  DCHECK(target->IsInlineReturnTarget());
  DCHECK(return_value != NULL);
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
  DCHECK(!HasEnvironment());
  DCHECK(first() == NULL);
  UpdateEnvironment(env);
}


void HBasicBlock::UpdateEnvironment(HEnvironment* env) {
  last_environment_ = env;
  graph()->update_maximum_environment_size(env->first_expression_index());
}


void HBasicBlock::SetJoinId(BailoutId ast_id) {
  int length = predecessors_.length();
  DCHECK(length > 0);
  for (int i = 0; i < length; i++) {
    HBasicBlock* predecessor = predecessors_[i];
    DCHECK(predecessor->end()->IsGoto());
    HSimulate* simulate = HSimulate::cast(predecessor->end()->previous());
    DCHECK(i != 0 ||
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
  DCHECK(IsLoopHeader());

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
  DCHECK(IsFinished());
  HBasicBlock* succ_block = end()->SuccessorAt(succ);

  DCHECK(succ_block->predecessors()->length() == 1);
  succ_block->MarkUnreachable();
}


void HBasicBlock::RegisterPredecessor(HBasicBlock* pred) {
  if (HasPredecessor()) {
    // Only loop header blocks can have a predecessor added after
    // instructions have been added to the block (they have phis for all
    // values in the environment, these phis may be eliminated later).
    DCHECK(IsLoopHeader() || first_ == NULL);
    HEnvironment* incoming_env = pred->last_environment();
    if (IsLoopHeader()) {
      DCHECK_EQ(phis()->length(), incoming_env->length());
      for (int i = 0; i < phis_.length(); ++i) {
        phis_[i]->AddInput(incoming_env->values()->at(i));
      }
    } else {
      last_environment()->AddIncomingEdge(this, pred->last_environment());
    }
  } else if (!HasEnvironment() && !IsFinished()) {
    DCHECK(!IsLoopHeader());
    SetInitialEnvironment(pred->last_environment()->Copy());
  }

  predecessors_.Add(pred, zone());
}


void HBasicBlock::AddDominatedBlock(HBasicBlock* block) {
  DCHECK(!dominated_blocks_.Contains(block));
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
      DCHECK(first != NULL && second != NULL);
    }

    if (dominator_ != first) {
      DCHECK(dominator_->dominated_blocks_.Contains(this));
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
    DCHECK(outstanding_successors >= 0);
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
        DCHECK(successor->block_id() > dominator_candidate->block_id() ||
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
  DCHECK(IsFinished());
  DCHECK(block_id() >= 0);

  // Check that the incoming edges are in edge split form.
  if (predecessors_.length() > 1) {
    for (int i = 0; i < predecessors_.length(); ++i) {
      DCHECK(predecessors_[i]->end()->SecondSuccessor() == NULL);
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
  base::LockGuard<base::Mutex> guard(isolate()->heap()->relocation_mutex());
  AllowHandleDereference allow_deref;
  AllowDeferredHandleDereference allow_deferred_deref;
  for (int i = 0; i < blocks_.length(); i++) {
    HBasicBlock* block = blocks_.at(i);

    block->Verify();

    // Check that every block contains at least one node and that only the last
    // node is a control instruction.
    HInstruction* current = block->first();
    DCHECK(current != NULL && current->IsBlockEntry());
    while (current != NULL) {
      DCHECK((current->next() == NULL) == current->IsControlInstruction());
      DCHECK(current->block() == block);
      current->Verify();
      current = current->next();
    }

    // Check that successors are correctly set.
    HBasicBlock* first = block->end()->FirstSuccessor();
    HBasicBlock* second = block->end()->SecondSuccessor();
    DCHECK(second == NULL || first != NULL);

    // Check that the predecessor array is correct.
    if (first != NULL) {
      DCHECK(first->predecessors()->Contains(block));
      if (second != NULL) {
        DCHECK(second->predecessors()->Contains(block));
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
        DCHECK(predecessor->end()->IsGoto() ||
               predecessor->end()->IsDeoptimize());
        DCHECK(predecessor->last_environment()->ast_id() == id);
      }
    }
  }

  // Check special property of first block to have no predecessors.
  DCHECK(blocks_.at(0)->predecessors()->is_empty());

  if (do_full_verify) {
    // Check that the graph is fully connected.
    ReachabilityAnalyzer analyzer(entry_block_, blocks_.length(), NULL);
    DCHECK(analyzer.visited_count() == blocks_.length());

    // Check that entry block dominator is NULL.
    DCHECK(entry_block_->dominator() == NULL);

    // Check dominators.
    for (int i = 0; i < blocks_.length(); ++i) {
      HBasicBlock* block = blocks_.at(i);
      if (block->dominator() == NULL) {
        // Only start block may have no dominator assigned to.
        DCHECK(i == 0);
      } else {
        // Assert that block is unreachable if dominator must not be visited.
        ReachabilityAnalyzer dominator_analyzer(entry_block_,
                                                blocks_.length(),
                                                block->dominator());
        DCHECK(!dominator_analyzer.reachable()->Contains(block->block_id()));
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
    HConstant* constant = HConstant::New(isolate(), zone(), NULL, value);
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


HConstant* HGraph::GetConstantBool(bool value) {
  return value ? GetConstantTrue() : GetConstantFalse();
}

#define DEFINE_GET_CONSTANT(Name, name, constant, type, htype, boolean_value, \
                            undetectable)                                     \
  HConstant* HGraph::GetConstant##Name() {                                    \
    if (!constant_##name##_.is_set()) {                                       \
      HConstant* constant = new (zone()) HConstant(                           \
          Unique<Object>::CreateImmovable(isolate()->factory()->constant()),  \
          Unique<Map>::CreateImmovable(isolate()->factory()->type##_map()),   \
          false, Representation::Tagged(), htype, true, boolean_value,        \
          undetectable, ODDBALL_TYPE);                                        \
      constant->InsertAfter(entry_block()->first());                          \
      constant_##name##_.set(constant);                                       \
    }                                                                         \
    return ReinsertConstantIfNecessary(constant_##name##_.get());             \
  }

DEFINE_GET_CONSTANT(Undefined, undefined, undefined_value, undefined,
                    HType::Undefined(), false, true)
DEFINE_GET_CONSTANT(True, true, true_value, boolean, HType::Boolean(), true,
                    false)
DEFINE_GET_CONSTANT(False, false, false_value, boolean, HType::Boolean(), false,
                    false)
DEFINE_GET_CONSTANT(Hole, the_hole, the_hole_value, the_hole, HType::None(),
                    false, false)
DEFINE_GET_CONSTANT(Null, null, null_value, null, HType::Null(), false, true)
DEFINE_GET_CONSTANT(OptimizedOut, optimized_out, optimized_out, optimized_out,
                    HType::None(), false, false)

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


HGraphBuilder::IfBuilder::IfBuilder() : builder_(NULL), needs_compare_(true) {}


HGraphBuilder::IfBuilder::IfBuilder(HGraphBuilder* builder)
    : needs_compare_(true) {
  Initialize(builder);
}


HGraphBuilder::IfBuilder::IfBuilder(HGraphBuilder* builder,
                                    HIfContinuation* continuation)
    : needs_compare_(false), first_true_block_(NULL), first_false_block_(NULL) {
  InitializeDontCreateBlocks(builder);
  continuation->Continue(&first_true_block_, &first_false_block_);
}


void HGraphBuilder::IfBuilder::InitializeDontCreateBlocks(
    HGraphBuilder* builder) {
  builder_ = builder;
  finished_ = false;
  did_then_ = false;
  did_else_ = false;
  did_else_if_ = false;
  did_and_ = false;
  did_or_ = false;
  captured_ = false;
  pending_merge_block_ = false;
  split_edge_merge_block_ = NULL;
  merge_at_join_blocks_ = NULL;
  normal_merge_at_join_block_count_ = 0;
  deopt_merge_at_join_block_count_ = 0;
}


void HGraphBuilder::IfBuilder::Initialize(HGraphBuilder* builder) {
  InitializeDontCreateBlocks(builder);
  HEnvironment* env = builder->environment();
  first_true_block_ = builder->CreateBasicBlock(env->Copy());
  first_false_block_ = builder->CreateBasicBlock(env->Copy());
}


HControlInstruction* HGraphBuilder::IfBuilder::AddCompare(
    HControlInstruction* compare) {
  DCHECK(did_then_ == did_else_);
  if (did_else_) {
    // Handle if-then-elseif
    did_else_if_ = true;
    did_else_ = false;
    did_then_ = false;
    did_and_ = false;
    did_or_ = false;
    pending_merge_block_ = false;
    split_edge_merge_block_ = NULL;
    HEnvironment* env = builder()->environment();
    first_true_block_ = builder()->CreateBasicBlock(env->Copy());
    first_false_block_ = builder()->CreateBasicBlock(env->Copy());
  }
  if (split_edge_merge_block_ != NULL) {
    HEnvironment* env = first_false_block_->last_environment();
    HBasicBlock* split_edge = builder()->CreateBasicBlock(env->Copy());
    if (did_or_) {
      compare->SetSuccessorAt(0, split_edge);
      compare->SetSuccessorAt(1, first_false_block_);
    } else {
      compare->SetSuccessorAt(0, first_true_block_);
      compare->SetSuccessorAt(1, split_edge);
    }
    builder()->GotoNoSimulate(split_edge, split_edge_merge_block_);
  } else {
    compare->SetSuccessorAt(0, first_true_block_);
    compare->SetSuccessorAt(1, first_false_block_);
  }
  builder()->FinishCurrentBlock(compare);
  needs_compare_ = false;
  return compare;
}


void HGraphBuilder::IfBuilder::Or() {
  DCHECK(!needs_compare_);
  DCHECK(!did_and_);
  did_or_ = true;
  HEnvironment* env = first_false_block_->last_environment();
  if (split_edge_merge_block_ == NULL) {
    split_edge_merge_block_ = builder()->CreateBasicBlock(env->Copy());
    builder()->GotoNoSimulate(first_true_block_, split_edge_merge_block_);
    first_true_block_ = split_edge_merge_block_;
  }
  builder()->set_current_block(first_false_block_);
  first_false_block_ = builder()->CreateBasicBlock(env->Copy());
}


void HGraphBuilder::IfBuilder::And() {
  DCHECK(!needs_compare_);
  DCHECK(!did_or_);
  did_and_ = true;
  HEnvironment* env = first_false_block_->last_environment();
  if (split_edge_merge_block_ == NULL) {
    split_edge_merge_block_ = builder()->CreateBasicBlock(env->Copy());
    builder()->GotoNoSimulate(first_false_block_, split_edge_merge_block_);
    first_false_block_ = split_edge_merge_block_;
  }
  builder()->set_current_block(first_true_block_);
  first_true_block_ = builder()->CreateBasicBlock(env->Copy());
}


void HGraphBuilder::IfBuilder::CaptureContinuation(
    HIfContinuation* continuation) {
  DCHECK(!did_else_if_);
  DCHECK(!finished_);
  DCHECK(!captured_);

  HBasicBlock* true_block = NULL;
  HBasicBlock* false_block = NULL;
  Finish(&true_block, &false_block);
  DCHECK(true_block != NULL);
  DCHECK(false_block != NULL);
  continuation->Capture(true_block, false_block);
  captured_ = true;
  builder()->set_current_block(NULL);
  End();
}


void HGraphBuilder::IfBuilder::JoinContinuation(HIfContinuation* continuation) {
  DCHECK(!did_else_if_);
  DCHECK(!finished_);
  DCHECK(!captured_);
  HBasicBlock* true_block = NULL;
  HBasicBlock* false_block = NULL;
  Finish(&true_block, &false_block);
  merge_at_join_blocks_ = NULL;
  if (true_block != NULL && !true_block->IsFinished()) {
    DCHECK(continuation->IsTrueReachable());
    builder()->GotoNoSimulate(true_block, continuation->true_branch());
  }
  if (false_block != NULL && !false_block->IsFinished()) {
    DCHECK(continuation->IsFalseReachable());
    builder()->GotoNoSimulate(false_block, continuation->false_branch());
  }
  captured_ = true;
  End();
}


void HGraphBuilder::IfBuilder::Then() {
  DCHECK(!captured_);
  DCHECK(!finished_);
  did_then_ = true;
  if (needs_compare_) {
    // Handle if's without any expressions, they jump directly to the "else"
    // branch. However, we must pretend that the "then" branch is reachable,
    // so that the graph builder visits it and sees any live range extending
    // constructs within it.
    HConstant* constant_false = builder()->graph()->GetConstantFalse();
    ToBooleanHints boolean_type = ToBooleanHint::kBoolean;
    HBranch* branch = builder()->New<HBranch>(
        constant_false, boolean_type, first_true_block_, first_false_block_);
    builder()->FinishCurrentBlock(branch);
  }
  builder()->set_current_block(first_true_block_);
  pending_merge_block_ = true;
}


void HGraphBuilder::IfBuilder::Else() {
  DCHECK(did_then_);
  DCHECK(!captured_);
  DCHECK(!finished_);
  AddMergeAtJoinBlock(false);
  builder()->set_current_block(first_false_block_);
  pending_merge_block_ = true;
  did_else_ = true;
}

void HGraphBuilder::IfBuilder::Deopt(DeoptimizeReason reason) {
  DCHECK(did_then_);
  builder()->Add<HDeoptimize>(reason, Deoptimizer::EAGER);
  AddMergeAtJoinBlock(true);
}


void HGraphBuilder::IfBuilder::Return(HValue* value) {
  HValue* parameter_count = builder()->graph()->GetConstantMinus1();
  builder()->FinishExitCurrentBlock(
      builder()->New<HReturn>(value, parameter_count));
  AddMergeAtJoinBlock(false);
}


void HGraphBuilder::IfBuilder::AddMergeAtJoinBlock(bool deopt) {
  if (!pending_merge_block_) return;
  HBasicBlock* block = builder()->current_block();
  DCHECK(block == NULL || !block->IsFinished());
  MergeAtJoinBlock* record = new (builder()->zone())
      MergeAtJoinBlock(block, deopt, merge_at_join_blocks_);
  merge_at_join_blocks_ = record;
  if (block != NULL) {
    DCHECK(block->end() == NULL);
    if (deopt) {
      normal_merge_at_join_block_count_++;
    } else {
      deopt_merge_at_join_block_count_++;
    }
  }
  builder()->set_current_block(NULL);
  pending_merge_block_ = false;
}


void HGraphBuilder::IfBuilder::Finish() {
  DCHECK(!finished_);
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
  DCHECK(then_record->next_ == NULL);
}


void HGraphBuilder::IfBuilder::EndUnreachable() {
  if (captured_) return;
  Finish();
  builder()->set_current_block(nullptr);
}


void HGraphBuilder::IfBuilder::End() {
  if (captured_) return;
  Finish();

  int total_merged_blocks = normal_merge_at_join_block_count_ +
    deopt_merge_at_join_block_count_;
  DCHECK(total_merged_blocks >= 1);
  HBasicBlock* merge_block =
      total_merged_blocks == 1 ? NULL : builder()->graph()->CreateBasicBlock();

  // Merge non-deopt blocks first to ensure environment has right size for
  // padding.
  MergeAtJoinBlock* current = merge_at_join_blocks_;
  while (current != NULL) {
    if (!current->deopt_ && current->block_ != NULL) {
      // If there is only one block that makes it through to the end of the
      // if, then just set it as the current block and continue rather then
      // creating an unnecessary merge block.
      if (total_merged_blocks == 1) {
        builder()->set_current_block(current->block_);
        return;
      }
      builder()->GotoNoSimulate(current->block_, merge_block);
    }
    current = current->next_;
  }

  // Merge deopt blocks, padding when necessary.
  current = merge_at_join_blocks_;
  while (current != NULL) {
    if (current->deopt_ && current->block_ != NULL) {
      current->block_->FinishExit(
          HAbnormalExit::New(builder()->isolate(), builder()->zone(), NULL),
          SourcePosition::Unknown());
    }
    current = current->next_;
  }
  builder()->set_current_block(merge_block);
}


HGraphBuilder::LoopBuilder::LoopBuilder(HGraphBuilder* builder) {
  Initialize(builder, NULL, kWhileTrue, NULL);
}


HGraphBuilder::LoopBuilder::LoopBuilder(HGraphBuilder* builder, HValue* context,
                                        LoopBuilder::Direction direction) {
  Initialize(builder, context, direction, builder->graph()->GetConstant1());
}


HGraphBuilder::LoopBuilder::LoopBuilder(HGraphBuilder* builder, HValue* context,
                                        LoopBuilder::Direction direction,
                                        HValue* increment_amount) {
  Initialize(builder, context, direction, increment_amount);
  increment_amount_ = increment_amount;
}


void HGraphBuilder::LoopBuilder::Initialize(HGraphBuilder* builder,
                                            HValue* context,
                                            Direction direction,
                                            HValue* increment_amount) {
  builder_ = builder;
  context_ = context;
  direction_ = direction;
  increment_amount_ = increment_amount;

  finished_ = false;
  header_block_ = builder->CreateLoopHeaderBlock();
  body_block_ = NULL;
  exit_block_ = NULL;
  exit_trampoline_block_ = NULL;
}


HValue* HGraphBuilder::LoopBuilder::BeginBody(
    HValue* initial,
    HValue* terminating,
    Token::Value token) {
  DCHECK(direction_ != kWhileTrue);
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
    Isolate* isolate = builder_->isolate();
    HValue* one = builder_->graph()->GetConstant1();
    if (direction_ == kPreIncrement) {
      increment_ = HAdd::New(isolate, zone(), context_, phi_, one);
    } else {
      increment_ = HSub::New(isolate, zone(), context_, phi_, one);
    }
    increment_->ClearFlag(HValue::kCanOverflow);
    builder_->AddInstruction(increment_);
    return increment_;
  } else {
    return phi_;
  }
}


void HGraphBuilder::LoopBuilder::BeginBody(int drop_count) {
  DCHECK(direction_ == kWhileTrue);
  HEnvironment* env = builder_->environment();
  builder_->GotoNoSimulate(header_block_);
  builder_->set_current_block(header_block_);
  env->Drop(drop_count);
}


void HGraphBuilder::LoopBuilder::Break() {
  if (exit_trampoline_block_ == NULL) {
    // Its the first time we saw a break.
    if (direction_ == kWhileTrue) {
      HEnvironment* env = builder_->environment()->Copy();
      exit_trampoline_block_ = builder_->CreateBasicBlock(env);
    } else {
      HEnvironment* env = exit_block_->last_environment()->Copy();
      exit_trampoline_block_ = builder_->CreateBasicBlock(env);
      builder_->GotoNoSimulate(exit_block_, exit_trampoline_block_);
    }
  }

  builder_->GotoNoSimulate(exit_trampoline_block_);
  builder_->set_current_block(NULL);
}


void HGraphBuilder::LoopBuilder::EndBody() {
  DCHECK(!finished_);

  if (direction_ == kPostIncrement || direction_ == kPostDecrement) {
    Isolate* isolate = builder_->isolate();
    if (direction_ == kPostIncrement) {
      increment_ =
          HAdd::New(isolate, zone(), context_, phi_, increment_amount_);
    } else {
      increment_ =
          HSub::New(isolate, zone(), context_, phi_, increment_amount_);
    }
    increment_->ClearFlag(HValue::kCanOverflow);
    builder_->AddInstruction(increment_);
  }

  if (direction_ != kWhileTrue) {
    // Push the new increment value on the expression stack to merge into
    // the phi.
    builder_->environment()->Push(increment_);
  }
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
  DCHECK(!FLAG_minimal);
  graph_ = new (zone()) HGraph(info_, descriptor_);
  if (FLAG_hydrogen_stats) isolate()->GetHStatistics()->Initialize(info_);
  CompilationPhase phase("H_Block building", info_);
  set_current_block(graph()->entry_block());
  if (!BuildGraph()) return NULL;
  graph()->FinalizeUniqueness();
  return graph_;
}


HInstruction* HGraphBuilder::AddInstruction(HInstruction* instr) {
  DCHECK(current_block() != NULL);
  DCHECK(!FLAG_hydrogen_track_positions || position_.IsKnown() ||
         !info_->IsOptimizing());
  current_block()->AddInstruction(instr, source_position());
  if (graph()->IsInsideNoSideEffectsScope()) {
    instr->SetFlag(HValue::kHasNoObservableSideEffects);
  }
  return instr;
}


void HGraphBuilder::FinishCurrentBlock(HControlInstruction* last) {
  DCHECK(!FLAG_hydrogen_track_positions || !info_->IsOptimizing() ||
         position_.IsKnown());
  current_block()->Finish(last, source_position());
  if (last->IsReturn() || last->IsAbnormalExit()) {
    set_current_block(NULL);
  }
}


void HGraphBuilder::FinishExitCurrentBlock(HControlInstruction* instruction) {
  DCHECK(!FLAG_hydrogen_track_positions || !info_->IsOptimizing() ||
         position_.IsKnown());
  current_block()->FinishExit(instruction, source_position());
  if (instruction->IsReturn() || instruction->IsAbnormalExit()) {
    set_current_block(NULL);
  }
}


void HGraphBuilder::AddIncrementCounter(StatsCounter* counter) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    HValue* reference = Add<HConstant>(ExternalReference(counter));
    HValue* old_value =
        Add<HLoadNamedField>(reference, nullptr, HObjectAccess::ForCounter());
    HValue* new_value = AddUncasted<HAdd>(old_value, graph()->GetConstant1());
    new_value->ClearFlag(HValue::kCanOverflow);  // Ignore counter overflow
    Add<HStoreNamedField>(reference, HObjectAccess::ForCounter(),
                          new_value, STORE_TO_INITIALIZED_ENTRY);
  }
}


void HGraphBuilder::AddSimulate(BailoutId id,
                                RemovableSimulate removable) {
  DCHECK(current_block() != NULL);
  DCHECK(!graph()->IsInsideNoSideEffectsScope());
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


HValue* HGraphBuilder::BuildGetElementsKind(HValue* object) {
  HValue* map = Add<HLoadNamedField>(object, nullptr, HObjectAccess::ForMap());

  HValue* bit_field2 =
      Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapBitField2());
  return BuildDecodeField<Map::ElementsKindBits>(bit_field2);
}


HValue* HGraphBuilder::BuildEnumLength(HValue* map) {
  NoObservableSideEffectsScope scope(this);
  HValue* bit_field3 =
      Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapBitField3());
  return BuildDecodeField<Map::EnumLengthBits>(bit_field3);
}


HValue* HGraphBuilder::BuildCheckHeapObject(HValue* obj) {
  if (obj->type().IsHeapObject()) return obj;
  return Add<HCheckHeapObject>(obj);
}

void HGraphBuilder::FinishExitWithHardDeoptimization(DeoptimizeReason reason) {
  Add<HDeoptimize>(reason, Deoptimizer::EAGER);
  FinishExitCurrentBlock(New<HAbnormalExit>());
}


HValue* HGraphBuilder::BuildCheckString(HValue* string) {
  if (!string->type().IsString()) {
    DCHECK(!string->IsConstant() ||
           !HConstant::cast(string)->HasStringValue());
    BuildCheckHeapObject(string);
    return Add<HCheckInstanceType>(string, HCheckInstanceType::IS_STRING);
  }
  return string;
}

HValue* HGraphBuilder::BuildWrapReceiver(HValue* object, HValue* checked) {
  if (object->type().IsJSObject()) return object;
  HValue* function = checked->ActualValue();
  if (function->IsConstant() &&
      HConstant::cast(function)->handle(isolate())->IsJSFunction()) {
    Handle<JSFunction> f = Handle<JSFunction>::cast(
        HConstant::cast(function)->handle(isolate()));
    SharedFunctionInfo* shared = f->shared();
    if (is_strict(shared->language_mode()) || shared->native()) return object;
  }
  return Add<HWrapReceiver>(object, checked);
}


HValue* HGraphBuilder::BuildCheckAndGrowElementsCapacity(
    HValue* object, HValue* elements, ElementsKind kind, HValue* length,
    HValue* capacity, HValue* key) {
  HValue* max_gap = Add<HConstant>(static_cast<int32_t>(JSObject::kMaxGap));
  HValue* max_capacity = AddUncasted<HAdd>(capacity, max_gap);
  Add<HBoundsCheck>(key, max_capacity);

  HValue* new_capacity = BuildNewElementsCapacity(key);
  HValue* new_elements = BuildGrowElementsCapacity(object, elements, kind, kind,
                                                   length, new_capacity);
  return new_elements;
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

  if (top_info()->IsStub()) {
    IfBuilder capacity_checker(this);
    capacity_checker.If<HCompareNumericAndBranch>(key, current_capacity,
                                                  Token::GTE);
    capacity_checker.Then();
    HValue* new_elements = BuildCheckAndGrowElementsCapacity(
        object, elements, kind, length, current_capacity, key);
    environment()->Push(new_elements);
    capacity_checker.Else();
    environment()->Push(elements);
    capacity_checker.End();
  } else {
    HValue* result = Add<HMaybeGrowElements>(
        object, elements, key, current_capacity, is_js_array, kind);
    environment()->Push(result);
  }

  if (is_js_array) {
    HValue* new_length = AddUncasted<HAdd>(key, graph_->GetConstant1());
    new_length->ClearFlag(HValue::kCanOverflow);

    Add<HStoreNamedField>(object, HObjectAccess::ForArrayLength(kind),
                          new_length);
  }

  if (access_type == STORE && kind == FAST_SMI_ELEMENTS) {
    HValue* checked_elements = environment()->Top();

    // Write zero to ensure that the new element is initialized with some smi.
    Add<HStoreKeyed>(checked_elements, key, graph()->GetConstant0(), nullptr,
                     kind);
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

HValue* HGraphBuilder::BuildCreateIterResultObject(HValue* value,
                                                   HValue* done) {
  NoObservableSideEffectsScope scope(this);

  // Allocate the JSIteratorResult object.
  HValue* result =
      Add<HAllocate>(Add<HConstant>(JSIteratorResult::kSize), HType::JSObject(),
                     NOT_TENURED, JS_OBJECT_TYPE, graph()->GetConstant0());

  // Initialize the JSIteratorResult object.
  HValue* native_context = BuildGetNativeContext();
  HValue* map = Add<HLoadNamedField>(
      native_context, nullptr,
      HObjectAccess::ForContextSlot(Context::ITERATOR_RESULT_MAP_INDEX));
  Add<HStoreNamedField>(result, HObjectAccess::ForMap(), map);
  HValue* empty_fixed_array = Add<HLoadRoot>(Heap::kEmptyFixedArrayRootIndex);
  Add<HStoreNamedField>(result, HObjectAccess::ForPropertiesPointer(),
                        empty_fixed_array);
  Add<HStoreNamedField>(result, HObjectAccess::ForElementsPointer(),
                        empty_fixed_array);
  Add<HStoreNamedField>(result, HObjectAccess::ForObservableJSObjectOffset(
                                    JSIteratorResult::kValueOffset),
                        value);
  Add<HStoreNamedField>(result, HObjectAccess::ForObservableJSObjectOffset(
                                    JSIteratorResult::kDoneOffset),
                        done);
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  return result;
}


HValue* HGraphBuilder::BuildNumberToString(HValue* object, AstType* type) {
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
    HValue* key = Add<HLoadKeyed>(number_string_cache, key_index, nullptr,
                                  nullptr, FAST_ELEMENTS, ALLOW_RETURN_HOLE);

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
    if (type->Is(AstType::SignedSmall())) {
      if_objectissmi.Deopt(DeoptimizeReason::kExpectedSmi);
    } else {
      // Check if the object is a heap number.
      IfBuilder if_objectisnumber(this);
      HValue* objectisnumber = if_objectisnumber.If<HCompareMap>(
          object, isolate()->factory()->heap_number_map());
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
        HValue* key =
            Add<HLoadKeyed>(number_string_cache, key_index, nullptr, nullptr,
                            FAST_ELEMENTS, ALLOW_RETURN_HOLE);

        // Check if the key is a heap number and compare it with the object.
        IfBuilder if_keyisnotsmi(this);
        HValue* keyisnotsmi = if_keyisnotsmi.IfNot<HIsSmiAndBranch>(key);
        if_keyisnotsmi.Then();
        {
          IfBuilder if_keyisheapnumber(this);
          if_keyisheapnumber.If<HCompareMap>(
              key, isolate()->factory()->heap_number_map());
          if_keyisheapnumber.Then();
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
          if_keyisheapnumber.JoinContinuation(&found);
        }
        if_keyisnotsmi.JoinContinuation(&found);
      }
      if_objectisnumber.Else();
      {
        if (type->Is(AstType::Number())) {
          if_objectisnumber.Deopt(DeoptimizeReason::kExpectedHeapNumber);
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
    Push(Add<HLoadKeyed>(number_string_cache, value_index, nullptr, nullptr,
                         FAST_ELEMENTS, ALLOW_RETURN_HOLE));
  }
  if_found.Else();
  {
    // Cache miss, fallback to runtime.
    Add<HPushArguments>(object);
    Push(Add<HCallRuntime>(
            Runtime::FunctionForId(Runtime::kNumberToStringSkipCache),
            1));
  }
  if_found.End();

  return Pop();
}

HValue* HGraphBuilder::BuildToNumber(HValue* input) {
  if (input->type().IsTaggedNumber() ||
      input->representation().IsSpecialization()) {
    return input;
  }
  Callable callable = CodeFactory::ToNumber(isolate());
  HValue* stub = Add<HConstant>(callable.code());
  HValue* values[] = {input};
  HCallWithDescriptor* instr = Add<HCallWithDescriptor>(
      stub, 0, callable.descriptor(), ArrayVector(values));
  instr->set_type(HType::TaggedNumber());
  return instr;
}


HValue* HGraphBuilder::BuildToObject(HValue* receiver) {
  NoObservableSideEffectsScope scope(this);

  // Create a joinable continuation.
  HIfContinuation wrap(graph()->CreateBasicBlock(),
                       graph()->CreateBasicBlock());

  // Determine the proper global constructor function required to wrap
  // {receiver} into a JSValue, unless {receiver} is already a {JSReceiver}, in
  // which case we just return it.  Deopts to Runtime::kToObject if {receiver}
  // is undefined or null.
  IfBuilder receiver_is_smi(this);
  receiver_is_smi.If<HIsSmiAndBranch>(receiver);
  receiver_is_smi.Then();
  {
    // Use global Number function.
    Push(Add<HConstant>(Context::NUMBER_FUNCTION_INDEX));
  }
  receiver_is_smi.Else();
  {
    // Determine {receiver} map and instance type.
    HValue* receiver_map =
        Add<HLoadNamedField>(receiver, nullptr, HObjectAccess::ForMap());
    HValue* receiver_instance_type = Add<HLoadNamedField>(
        receiver_map, nullptr, HObjectAccess::ForMapInstanceType());

    // First check whether {receiver} is already a spec object (fast case).
    IfBuilder receiver_is_not_spec_object(this);
    receiver_is_not_spec_object.If<HCompareNumericAndBranch>(
        receiver_instance_type, Add<HConstant>(FIRST_JS_RECEIVER_TYPE),
        Token::LT);
    receiver_is_not_spec_object.Then();
    {
      // Load the constructor function index from the {receiver} map.
      HValue* constructor_function_index = Add<HLoadNamedField>(
          receiver_map, nullptr,
          HObjectAccess::ForMapInObjectPropertiesOrConstructorFunctionIndex());

      // Check if {receiver} has a constructor (null and undefined have no
      // constructors, so we deoptimize to the runtime to throw an exception).
      IfBuilder constructor_function_index_is_invalid(this);
      constructor_function_index_is_invalid.If<HCompareNumericAndBranch>(
          constructor_function_index,
          Add<HConstant>(Map::kNoConstructorFunctionIndex), Token::EQ);
      constructor_function_index_is_invalid.ThenDeopt(
          DeoptimizeReason::kUndefinedOrNullInToObject);
      constructor_function_index_is_invalid.End();

      // Use the global constructor function.
      Push(constructor_function_index);
    }
    receiver_is_not_spec_object.JoinContinuation(&wrap);
  }
  receiver_is_smi.JoinContinuation(&wrap);

  // Wrap the receiver if necessary.
  IfBuilder if_wrap(this, &wrap);
  if_wrap.Then();
  {
    // Grab the constructor function index.
    HValue* constructor_index = Pop();

    // Load native context.
    HValue* native_context = BuildGetNativeContext();

    // Determine the initial map for the global constructor.
    HValue* constructor = Add<HLoadKeyed>(native_context, constructor_index,
                                          nullptr, nullptr, FAST_ELEMENTS);
    HValue* constructor_initial_map = Add<HLoadNamedField>(
        constructor, nullptr, HObjectAccess::ForPrototypeOrInitialMap());
    // Allocate and initialize a JSValue wrapper.
    HValue* value =
        BuildAllocate(Add<HConstant>(JSValue::kSize), HType::JSObject(),
                      JS_VALUE_TYPE, HAllocationMode());
    Add<HStoreNamedField>(value, HObjectAccess::ForMap(),
                          constructor_initial_map);
    HValue* empty_fixed_array = Add<HLoadRoot>(Heap::kEmptyFixedArrayRootIndex);
    Add<HStoreNamedField>(value, HObjectAccess::ForPropertiesPointer(),
                          empty_fixed_array);
    Add<HStoreNamedField>(value, HObjectAccess::ForElementsPointer(),
                          empty_fixed_array);
    Add<HStoreNamedField>(value, HObjectAccess::ForObservableJSObjectOffset(
                                     JSValue::kValueOffset),
                          receiver);
    Push(value);
  }
  if_wrap.Else();
  { Push(receiver); }
  if_wrap.End();
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
      size, type, allocation_mode.GetPretenureMode(), instance_type,
      graph()->GetConstant0(), allocation_mode.feedback_site());

  // Setup the allocation memento.
  if (allocation_mode.CreateAllocationMementos()) {
    BuildCreateAllocationMemento(
        object, object_size, allocation_mode.current_site());
  }

  return object;
}


HValue* HGraphBuilder::BuildAddStringLengths(HValue* left_length,
                                             HValue* right_length) {
  // Compute the combined string length and check against max string length.
  HValue* length = AddUncasted<HAdd>(left_length, right_length);
  // Check that length <= kMaxLength <=> length < MaxLength + 1.
  HValue* max_length = Add<HConstant>(String::kMaxLength + 1);
  if (top_info()->IsStub() || !isolate()->IsStringLengthOverflowIntact()) {
    // This is a mitigation for crbug.com/627934; the real fix
    // will be to migrate the StringAddStub to TurboFan one day.
    IfBuilder if_invalid(this);
    if_invalid.If<HCompareNumericAndBranch>(length, max_length, Token::GT);
    if_invalid.Then();
    {
      Add<HCallRuntime>(
          Runtime::FunctionForId(Runtime::kThrowInvalidStringLength), 0);
    }
    if_invalid.End();
  } else {
    graph()->MarkDependsOnStringLengthOverflow();
    Add<HBoundsCheck>(length, max_length);
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
  // pass CONS_STRING_TYPE or CONS_ONE_BYTE_STRING_TYPE here, so we just use
  // CONS_STRING_TYPE here. Below we decide whether the cons string is
  // one-byte or two-byte and set the appropriate map.
  DCHECK(HAllocate::CompatibleInstanceTypes(CONS_STRING_TYPE,
                                            CONS_ONE_BYTE_STRING_TYPE));
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
    Add<HStoreNamedField>(
        result, HObjectAccess::ForMap(),
        Add<HConstant>(isolate()->factory()->cons_one_byte_string_map()));
  }
  if_onebyte.Else();
  {
    // We can safely skip the write barrier for storing the map here.
    Add<HStoreNamedField>(
        result, HObjectAccess::ForMap(),
        Add<HConstant>(isolate()->factory()->cons_string_map()));
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
  DCHECK(dst_encoding != String::ONE_BYTE_ENCODING ||
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


HValue* HGraphBuilder::BuildObjectSizeAlignment(
    HValue* unaligned_size, int header_size) {
  DCHECK((header_size & kObjectAlignmentMask) == 0);
  HValue* size = AddUncasted<HAdd>(
      unaligned_size, Add<HConstant>(static_cast<int32_t>(
          header_size + kObjectAlignmentMask)));
  size->ClearFlag(HValue::kCanOverflow);
  return AddUncasted<HBitwise>(
      Token::BIT_AND, size, Add<HConstant>(static_cast<int32_t>(
          ~kObjectAlignmentMask)));
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
    DCHECK_NE(0, c_left_length->Integer32Value());
    if (c_left_length->Integer32Value() + 1 >= ConsString::kMinLength) {
      // The right string contains at least one character.
      return BuildCreateConsString(length, left, right, allocation_mode);
    }
  } else if (right_length->IsConstant()) {
    HConstant* c_right_length = HConstant::cast(right_length);
    DCHECK_NE(0, c_right_length->Integer32Value());
    if (c_right_length->Integer32Value() + 1 >= ConsString::kMinLength) {
      // The left string contains at least one character.
      return BuildCreateConsString(length, left, right, allocation_mode);
    }
  }

  // Check if we should create a cons string.
  IfBuilder if_createcons(this);
  if_createcons.If<HCompareNumericAndBranch>(
      length, Add<HConstant>(ConsString::kMinLength), Token::GTE);
  if_createcons.And();
  if_createcons.If<HCompareNumericAndBranch>(
      length, Add<HConstant>(ConsString::kMaxLength), Token::LTE);
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
      HConstant* one_byte_string_map =
          Add<HConstant>(isolate()->factory()->one_byte_string_map());

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
        Push(one_byte_string_map);
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
      HValue* size = BuildObjectSizeAlignment(Pop(), SeqString::kHeaderSize);

      IfBuilder if_size(this);
      if_size.If<HCompareNumericAndBranch>(
          size, Add<HConstant>(kMaxRegularHeapObjectSize), Token::LT);
      if_size.Then();
      {
        // Allocate the string object. HAllocate does not care whether we pass
        // STRING_TYPE or ONE_BYTE_STRING_TYPE here, so we just use STRING_TYPE.
        HAllocate* result =
            BuildAllocate(size, HType::String(), STRING_TYPE, allocation_mode);
        Add<HStoreNamedField>(result, HObjectAccess::ForMap(), map);

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
              left, graph()->GetConstant0(), String::TWO_BYTE_ENCODING, result,
              graph()->GetConstant0(), String::TWO_BYTE_ENCODING, left_length);

          // Copy characters from the right string.
          BuildCopySeqStringChars(
              right, graph()->GetConstant0(), String::TWO_BYTE_ENCODING, result,
              left_length, String::TWO_BYTE_ENCODING, right_length);
        }
        if_twobyte.Else();
        {
          // Copy characters from the left string.
          BuildCopySeqStringChars(
              left, graph()->GetConstant0(), String::ONE_BYTE_ENCODING, result,
              graph()->GetConstant0(), String::ONE_BYTE_ENCODING, left_length);

          // Copy characters from the right string.
          BuildCopySeqStringChars(
              right, graph()->GetConstant0(), String::ONE_BYTE_ENCODING, result,
              left_length, String::ONE_BYTE_ENCODING, right_length);
        }
        if_twobyte.End();

        // Count the native string addition.
        AddIncrementCounter(isolate()->counters()->string_add_native());

        // Return the sequential string.
        Push(result);
      }
      if_size.Else();
      {
        // Fallback to the runtime to add the two strings. The string has to be
        // allocated in LO space.
        Add<HPushArguments>(left, right);
        Push(Add<HCallRuntime>(Runtime::FunctionForId(Runtime::kStringAdd), 2));
      }
      if_size.End();
    }
    if_sameencodingandsequential.Else();
    {
      // Fallback to the runtime to add the two strings.
      Add<HPushArguments>(left, right);
      Push(Add<HCallRuntime>(Runtime::FunctionForId(Runtime::kStringAdd), 2));
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
  DCHECK(top_info()->IsStub() || checked_object->IsCompareMap() ||
         checked_object->IsCheckMaps());
  DCHECK(!IsFixedTypedArrayElementsKind(elements_kind) || !is_js_array);
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
        elements, isolate()->factory()->fixed_array_map());
    check_cow_map->ClearDependsOnFlag(kElementsKind);
  }
  HInstruction* length = NULL;
  if (is_js_array) {
    length = Add<HLoadNamedField>(
        checked_object->ActualValue(), checked_object,
        HObjectAccess::ForArrayLength(elements_kind));
  } else {
    length = AddLoadFixedArrayLength(elements);
  }
  length->set_type(HType::Smi());
  HValue* checked_key = NULL;
  if (IsFixedTypedArrayElementsKind(elements_kind)) {
    checked_object = Add<HCheckArrayBufferNotNeutered>(checked_object);

    HValue* external_pointer = Add<HLoadNamedField>(
        elements, nullptr,
        HObjectAccess::ForFixedTypedArrayBaseExternalPointer());
    HValue* base_pointer = Add<HLoadNamedField>(
        elements, nullptr, HObjectAccess::ForFixedTypedArrayBaseBasePointer());
    HValue* backing_store = AddUncasted<HAdd>(external_pointer, base_pointer,
                                              AddOfExternalAndTagged);

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
          backing_store, key, val, bounds_check, checked_object->ActualValue(),
          elements_kind, access_type);
      negative_checker.ElseDeopt(DeoptimizeReason::kNegativeKeyEncountered);
      negative_checker.End();
      length_checker.End();
      return result;
    } else {
      DCHECK(store_mode == STANDARD_STORE);
      checked_key = Add<HBoundsCheck>(key, length);
      return AddElementAccess(backing_store, checked_key, val, checked_object,
                              checked_object->ActualValue(), elements_kind,
                              access_type);
    }
  }
  DCHECK(fast_smi_only_elements ||
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
    Representation representation = HStoreKeyed::RequiredValueRepresentation(
        elements_kind, STORE_TO_INITIALIZED_ENTRY);
    val = AddUncasted<HForceRepresentation>(val, representation);
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
            elements, isolate()->factory()->fixed_array_map());
        check_cow_map->ClearDependsOnFlag(kElementsKind);
      }
    }
  }
  return AddElementAccess(elements, checked_key, val, checked_object, nullptr,
                          elements_kind, access_type, load_mode);
}


HValue* HGraphBuilder::BuildCalculateElementsSize(ElementsKind kind,
                                                  HValue* capacity) {
  int elements_size = IsFastDoubleElementsKind(kind)
      ? kDoubleSize
      : kPointerSize;

  HConstant* elements_size_value = Add<HConstant>(elements_size);
  HInstruction* mul =
      HMul::NewImul(isolate(), zone(), context(), capacity->ActualValue(),
                    elements_size_value);
  AddInstruction(mul);
  mul->ClearFlag(HValue::kCanOverflow);

  STATIC_ASSERT(FixedDoubleArray::kHeaderSize == FixedArray::kHeaderSize);

  HConstant* header_size = Add<HConstant>(FixedArray::kHeaderSize);
  HValue* total_size = AddUncasted<HAdd>(mul, header_size);
  total_size->ClearFlag(HValue::kCanOverflow);
  return total_size;
}


HAllocate* HGraphBuilder::AllocateJSArrayObject(AllocationSiteMode mode) {
  int base_size = JSArray::kSize;
  if (mode == TRACK_ALLOCATION_SITE) {
    base_size += AllocationMemento::kSize;
  }
  HConstant* size_in_bytes = Add<HConstant>(base_size);
  return Add<HAllocate>(size_in_bytes, HType::JSArray(), NOT_TENURED,
                        JS_OBJECT_TYPE, graph()->GetConstant0());
}


HConstant* HGraphBuilder::EstablishElementsAllocationSize(
    ElementsKind kind,
    int capacity) {
  int base_size = IsFastDoubleElementsKind(kind)
      ? FixedDoubleArray::SizeFor(capacity)
      : FixedArray::SizeFor(capacity);

  return Add<HConstant>(base_size);
}


HAllocate* HGraphBuilder::BuildAllocateElements(ElementsKind kind,
                                                HValue* size_in_bytes) {
  InstanceType instance_type = IsFastDoubleElementsKind(kind)
      ? FIXED_DOUBLE_ARRAY_TYPE
      : FIXED_ARRAY_TYPE;

  return Add<HAllocate>(size_in_bytes, HType::HeapObject(), NOT_TENURED,
                        instance_type, graph()->GetConstant0());
}


void HGraphBuilder::BuildInitializeElementsHeader(HValue* elements,
                                                  ElementsKind kind,
                                                  HValue* capacity) {
  Factory* factory = isolate()->factory();
  Handle<Map> map = IsFastDoubleElementsKind(kind)
      ? factory->fixed_double_array_map()
      : factory->fixed_array_map();

  Add<HStoreNamedField>(elements, HObjectAccess::ForMap(), Add<HConstant>(map));
  Add<HStoreNamedField>(elements, HObjectAccess::ForFixedArrayLength(),
                        capacity);
}


HValue* HGraphBuilder::BuildAllocateAndInitializeArray(ElementsKind kind,
                                                       HValue* capacity) {
  // The HForceRepresentation is to prevent possible deopt on int-smi
  // conversion after allocation but before the new object fields are set.
  capacity = AddUncasted<HForceRepresentation>(capacity, Representation::Smi());
  HValue* size_in_bytes = BuildCalculateElementsSize(kind, capacity);
  HValue* new_array = BuildAllocateElements(kind, size_in_bytes);
  BuildInitializeElementsHeader(new_array, kind, capacity);
  return new_array;
}


void HGraphBuilder::BuildJSArrayHeader(HValue* array,
                                       HValue* array_map,
                                       HValue* elements,
                                       AllocationSiteMode mode,
                                       ElementsKind elements_kind,
                                       HValue* allocation_site_payload,
                                       HValue* length_field) {
  Add<HStoreNamedField>(array, HObjectAccess::ForMap(), array_map);

  HValue* empty_fixed_array = Add<HLoadRoot>(Heap::kEmptyFixedArrayRootIndex);

  Add<HStoreNamedField>(
      array, HObjectAccess::ForPropertiesPointer(), empty_fixed_array);

  Add<HStoreNamedField>(array, HObjectAccess::ForElementsPointer(),
                        elements != nullptr ? elements : empty_fixed_array);

  Add<HStoreNamedField>(
      array, HObjectAccess::ForArrayLength(elements_kind), length_field);

  if (mode == TRACK_ALLOCATION_SITE) {
    BuildCreateAllocationMemento(
        array, Add<HConstant>(JSArray::kSize), allocation_site_payload);
  }
}


HInstruction* HGraphBuilder::AddElementAccess(
    HValue* elements, HValue* checked_key, HValue* val, HValue* dependency,
    HValue* backing_store_owner, ElementsKind elements_kind,
    PropertyAccessType access_type, LoadKeyedHoleMode load_mode) {
  if (access_type == STORE) {
    DCHECK(val != NULL);
    if (elements_kind == UINT8_CLAMPED_ELEMENTS) {
      val = Add<HClampToUint8>(val);
    }
    return Add<HStoreKeyed>(elements, checked_key, val, backing_store_owner,
                            elements_kind, STORE_TO_INITIALIZED_ENTRY);
  }

  DCHECK(access_type == LOAD);
  DCHECK(val == NULL);
  HLoadKeyed* load =
      Add<HLoadKeyed>(elements, checked_key, dependency, backing_store_owner,
                      elements_kind, load_mode);
  if (elements_kind == UINT32_ELEMENTS) {
    graph()->RecordUint32Instruction(load);
  }
  return load;
}


HLoadNamedField* HGraphBuilder::AddLoadMap(HValue* object,
                                           HValue* dependency) {
  return Add<HLoadNamedField>(object, dependency, HObjectAccess::ForMap());
}


HLoadNamedField* HGraphBuilder::AddLoadElements(HValue* object,
                                                HValue* dependency) {
  return Add<HLoadNamedField>(
      object, dependency, HObjectAccess::ForElementsPointer());
}


HLoadNamedField* HGraphBuilder::AddLoadFixedArrayLength(
    HValue* array,
    HValue* dependency) {
  return Add<HLoadNamedField>(
      array, dependency, HObjectAccess::ForFixedArrayLength());
}


HLoadNamedField* HGraphBuilder::AddLoadArrayLength(HValue* array,
                                                   ElementsKind kind,
                                                   HValue* dependency) {
  return Add<HLoadNamedField>(
      array, dependency, HObjectAccess::ForArrayLength(kind));
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


HValue* HGraphBuilder::BuildGrowElementsCapacity(HValue* object,
                                                 HValue* elements,
                                                 ElementsKind kind,
                                                 ElementsKind new_kind,
                                                 HValue* length,
                                                 HValue* new_capacity) {
  Add<HBoundsCheck>(
      new_capacity,
      Add<HConstant>((kMaxRegularHeapObjectSize - FixedArray::kHeaderSize) >>
                     ElementsKindToShiftSize(new_kind)));

  HValue* new_elements =
      BuildAllocateAndInitializeArray(new_kind, new_capacity);

  BuildCopyElements(elements, kind, new_elements,
                    new_kind, length, new_capacity);

  Add<HStoreNamedField>(object, HObjectAccess::ForElementsPointer(),
                        new_elements);

  return new_elements;
}


void HGraphBuilder::BuildFillElementsWithValue(HValue* elements,
                                               ElementsKind elements_kind,
                                               HValue* from,
                                               HValue* to,
                                               HValue* value) {
  if (to == NULL) {
    to = AddLoadFixedArrayLength(elements);
  }

  // Special loop unfolding case
  STATIC_ASSERT(JSArray::kPreallocatedArrayElements <=
                kElementLoopUnrollThreshold);
  int initial_capacity = -1;
  if (from->IsInteger32Constant() && to->IsInteger32Constant()) {
    int constant_from = from->GetInteger32Constant();
    int constant_to = to->GetInteger32Constant();

    if (constant_from == 0 && constant_to <= kElementLoopUnrollThreshold) {
      initial_capacity = constant_to;
    }
  }

  if (initial_capacity >= 0) {
    for (int i = 0; i < initial_capacity; i++) {
      HInstruction* key = Add<HConstant>(i);
      Add<HStoreKeyed>(elements, key, value, nullptr, elements_kind);
    }
  } else {
    // Carefully loop backwards so that the "from" remains live through the loop
    // rather than the to. This often corresponds to keeping length live rather
    // then capacity, which helps register allocation, since length is used more
    // other than capacity after filling with holes.
    LoopBuilder builder(this, context(), LoopBuilder::kPostDecrement);

    HValue* key = builder.BeginBody(to, from, Token::GT);

    HValue* adjusted_key = AddUncasted<HSub>(key, graph()->GetConstant1());
    adjusted_key->ClearFlag(HValue::kCanOverflow);

    Add<HStoreKeyed>(elements, adjusted_key, value, nullptr, elements_kind);

    builder.EndBody();
  }
}


void HGraphBuilder::BuildFillElementsWithHole(HValue* elements,
                                              ElementsKind elements_kind,
                                              HValue* from,
                                              HValue* to) {
  // Fast elements kinds need to be initialized in case statements below cause a
  // garbage collection.

  HValue* hole = IsFastSmiOrObjectElementsKind(elements_kind)
                     ? graph()->GetConstantHole()
                     : Add<HConstant>(HConstant::kHoleNaN);

  // Since we're about to store a hole value, the store instruction below must
  // assume an elements kind that supports heap object values.
  if (IsFastSmiOrObjectElementsKind(elements_kind)) {
    elements_kind = FAST_HOLEY_ELEMENTS;
  }

  BuildFillElementsWithValue(elements, elements_kind, from, to, hole);
}


void HGraphBuilder::BuildCopyProperties(HValue* from_properties,
                                        HValue* to_properties, HValue* length,
                                        HValue* capacity) {
  ElementsKind kind = FAST_ELEMENTS;

  BuildFillElementsWithValue(to_properties, kind, length, capacity,
                             graph()->GetConstantUndefined());

  LoopBuilder builder(this, context(), LoopBuilder::kPostDecrement);

  HValue* key = builder.BeginBody(length, graph()->GetConstant0(), Token::GT);

  key = AddUncasted<HSub>(key, graph()->GetConstant1());
  key->ClearFlag(HValue::kCanOverflow);

  HValue* element =
      Add<HLoadKeyed>(from_properties, key, nullptr, nullptr, kind);

  Add<HStoreKeyed>(to_properties, key, element, nullptr, kind);

  builder.EndBody();
}


void HGraphBuilder::BuildCopyElements(HValue* from_elements,
                                      ElementsKind from_elements_kind,
                                      HValue* to_elements,
                                      ElementsKind to_elements_kind,
                                      HValue* length,
                                      HValue* capacity) {
  int constant_capacity = -1;
  if (capacity != NULL &&
      capacity->IsConstant() &&
      HConstant::cast(capacity)->HasInteger32Value()) {
    int constant_candidate = HConstant::cast(capacity)->Integer32Value();
    if (constant_candidate <= kElementLoopUnrollThreshold) {
      constant_capacity = constant_candidate;
    }
  }

  bool pre_fill_with_holes =
    IsFastDoubleElementsKind(from_elements_kind) &&
    IsFastObjectElementsKind(to_elements_kind);
  if (pre_fill_with_holes) {
    // If the copy might trigger a GC, make sure that the FixedArray is
    // pre-initialized with holes to make sure that it's always in a
    // consistent state.
    BuildFillElementsWithHole(to_elements, to_elements_kind,
                              graph()->GetConstant0(), NULL);
  }

  if (constant_capacity != -1) {
    // Unroll the loop for small elements kinds.
    for (int i = 0; i < constant_capacity; i++) {
      HValue* key_constant = Add<HConstant>(i);
      HInstruction* value = Add<HLoadKeyed>(
          from_elements, key_constant, nullptr, nullptr, from_elements_kind);
      Add<HStoreKeyed>(to_elements, key_constant, value, nullptr,
                       to_elements_kind);
    }
  } else {
    if (!pre_fill_with_holes &&
        (capacity == NULL || !length->Equals(capacity))) {
      BuildFillElementsWithHole(to_elements, to_elements_kind,
                                length, NULL);
    }

    LoopBuilder builder(this, context(), LoopBuilder::kPostDecrement);

    HValue* key = builder.BeginBody(length, graph()->GetConstant0(),
                                    Token::GT);

    key = AddUncasted<HSub>(key, graph()->GetConstant1());
    key->ClearFlag(HValue::kCanOverflow);

    HValue* element = Add<HLoadKeyed>(from_elements, key, nullptr, nullptr,
                                      from_elements_kind, ALLOW_RETURN_HOLE);

    ElementsKind kind = (IsHoleyElementsKind(from_elements_kind) &&
                         IsFastSmiElementsKind(to_elements_kind))
      ? FAST_HOLEY_ELEMENTS : to_elements_kind;

    if (IsHoleyElementsKind(from_elements_kind) &&
        from_elements_kind != to_elements_kind) {
      IfBuilder if_hole(this);
      if_hole.If<HCompareHoleAndBranch>(element);
      if_hole.Then();
      HConstant* hole_constant = IsFastDoubleElementsKind(to_elements_kind)
                                     ? Add<HConstant>(HConstant::kHoleNaN)
                                     : graph()->GetConstantHole();
      Add<HStoreKeyed>(to_elements, key, hole_constant, nullptr, kind);
      if_hole.Else();
      HStoreKeyed* store =
          Add<HStoreKeyed>(to_elements, key, element, nullptr, kind);
      store->SetFlag(HValue::kTruncatingToNumber);
      if_hole.End();
    } else {
      HStoreKeyed* store =
          Add<HStoreKeyed>(to_elements, key, element, nullptr, kind);
      store->SetFlag(HValue::kTruncatingToNumber);
    }

    builder.EndBody();
  }

  Counters* counters = isolate()->counters();
  AddIncrementCounter(counters->inlined_copied_elements());
}

void HGraphBuilder::BuildCreateAllocationMemento(
    HValue* previous_object,
    HValue* previous_object_size,
    HValue* allocation_site) {
  DCHECK(allocation_site != NULL);
  HInnerAllocatedObject* allocation_memento = Add<HInnerAllocatedObject>(
      previous_object, previous_object_size, HType::HeapObject());
  AddStoreMapConstant(
      allocation_memento, isolate()->factory()->allocation_memento_map());
  Add<HStoreNamedField>(
      allocation_memento,
      HObjectAccess::ForAllocationMementoSite(),
      allocation_site);
  if (FLAG_allocation_site_pretenuring) {
    HValue* memento_create_count =
        Add<HLoadNamedField>(allocation_site, nullptr,
                             HObjectAccess::ForAllocationSiteOffset(
                                 AllocationSite::kPretenureCreateCountOffset));
    memento_create_count = AddUncasted<HAdd>(
        memento_create_count, graph()->GetConstant1());
    // This smi value is reset to zero after every gc, overflow isn't a problem
    // since the counter is bounded by the new space size.
    memento_create_count->ClearFlag(HValue::kCanOverflow);
    Add<HStoreNamedField>(
        allocation_site, HObjectAccess::ForAllocationSiteOffset(
            AllocationSite::kPretenureCreateCountOffset), memento_create_count);
  }
}


HInstruction* HGraphBuilder::BuildGetNativeContext() {
  return Add<HLoadNamedField>(
      context(), nullptr,
      HObjectAccess::ForContextSlot(Context::NATIVE_CONTEXT_INDEX));
}

HValue* HGraphBuilder::BuildArrayBufferViewFieldAccessor(HValue* object,
                                                         HValue* checked_object,
                                                         FieldIndex index) {
  NoObservableSideEffectsScope scope(this);
  HObjectAccess access = HObjectAccess::ForObservableJSObjectOffset(
      index.offset(), Representation::Tagged());
  HInstruction* buffer = Add<HLoadNamedField>(
      object, checked_object, HObjectAccess::ForJSArrayBufferViewBuffer());
  HInstruction* field = Add<HLoadNamedField>(object, checked_object, access);

  HInstruction* flags = Add<HLoadNamedField>(
      buffer, nullptr, HObjectAccess::ForJSArrayBufferBitField());
  HValue* was_neutered_mask =
      Add<HConstant>(1 << JSArrayBuffer::WasNeutered::kShift);
  HValue* was_neutered_test =
      AddUncasted<HBitwise>(Token::BIT_AND, flags, was_neutered_mask);

  IfBuilder if_was_neutered(this);
  if_was_neutered.If<HCompareNumericAndBranch>(
      was_neutered_test, graph()->GetConstant0(), Token::NE);
  if_was_neutered.Then();
  Push(graph()->GetConstant0());
  if_was_neutered.Else();
  Push(field);
  if_was_neutered.End();

  return Pop();
}

HOptimizedGraphBuilder::HOptimizedGraphBuilder(CompilationInfo* info,
                                               bool track_positions)
    : HGraphBuilder(info, CallInterfaceDescriptor(), track_positions),
      function_state_(NULL),
      initial_function_state_(this, info, NORMAL_RETURN, -1,
                              TailCallMode::kAllow),
      ast_context_(NULL),
      break_scope_(NULL),
      inlined_count_(0),
      globals_(10, info->zone()),
      osr_(new (info->zone()) HOsrBuilder(this)),
      bounds_(info->zone()) {
  // This is not initialized in the initializer list because the
  // constructor for the initial state relies on function_state_ == NULL
  // to know it's the initial state.
  function_state_ = &initial_function_state_;
  InitializeAstVisitor(info->isolate());
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
                                                  BailoutId continue_id,
                                                  HBasicBlock* exit_block,
                                                  HBasicBlock* continue_block) {
  if (continue_block != NULL) {
    if (exit_block != NULL) Goto(exit_block, continue_block);
    continue_block->SetJoinId(continue_id);
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
  HBasicBlock* loop_entry;

  if (osr()->HasOsrEntryAt(statement)) {
    loop_entry = osr()->BuildOsrLoopEntry(statement);
    if (function_state()->IsInsideDoExpressionScope()) {
      Bailout(kDoExpressionUnmodelable);
    }
  } else {
    loop_entry = BuildLoopEntry();
  }
  return loop_entry;
}


void HBasicBlock::FinishExit(HControlInstruction* instruction,
                             SourcePosition position) {
  Finish(instruction, position);
  ClearEnvironment();
}


std::ostream& operator<<(std::ostream& os, const HBasicBlock& b) {
  return os << "B" << b.block_id();
}

HGraph::HGraph(CompilationInfo* info, CallInterfaceDescriptor descriptor)
    : isolate_(info->isolate()),
      next_block_id_(0),
      entry_block_(NULL),
      blocks_(8, info->zone()),
      values_(16, info->zone()),
      phi_list_(NULL),
      uint32_instructions_(NULL),
      osr_(NULL),
      info_(info),
      descriptor_(descriptor),
      zone_(info->zone()),
      allow_code_motion_(false),
      use_optimistic_licm_(false),
      depends_on_empty_array_proto_elements_(false),
      depends_on_string_length_overflow_(false),
      type_change_checksum_(0),
      maximum_environment_size_(0),
      no_side_effects_scope_count_(0),
      disallow_adding_new_values_(false) {
  if (info->IsStub()) {
    // For stubs, explicitly add the context to the environment.
    start_environment_ =
        new (zone_) HEnvironment(zone_, descriptor.GetParameterCount() + 1);
  } else {
    start_environment_ =
        new(zone_) HEnvironment(NULL, info->scope(), info->closure(), zone_);
  }
  start_environment_->set_ast_id(BailoutId::FunctionContext());
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
                                                  HBasicBlock* block) {
    PostorderProcessor* result = new(zone) PostorderProcessor(NULL);
    return result->SetupSuccessors(zone, block, NULL);
  }

  PostorderProcessor* PerformStep(Zone* zone,
                                  ZoneList<HBasicBlock*>* order) {
    PostorderProcessor* next =
        PerformNonBacktrackingStep(zone, order);
    if (next != NULL) {
      return next;
    } else {
      return Backtrack(zone, order);
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
                                      HBasicBlock* loop_header) {
    if (block == NULL || block->IsOrdered() ||
        block->parent_loop_header() != loop_header) {
      kind_ = NONE;
      block_ = NULL;
      loop_ = NULL;
      loop_header_ = NULL;
      return this;
    } else {
      block_ = block;
      loop_ = NULL;
      block->MarkAsOrdered();

      if (block->IsLoopHeader()) {
        kind_ = SUCCESSORS_OF_LOOP_HEADER;
        loop_header_ = block;
        InitializeSuccessors();
        PostorderProcessor* result = Push(zone);
        return result->SetupLoopMembers(zone, block, block->loop_information(),
                                        loop_header);
      } else {
        DCHECK(block->IsFinished());
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
    DCHECK(block_->end()->FirstSuccessor() == NULL ||
           order->Contains(block_->end()->FirstSuccessor()) ||
           block_->end()->FirstSuccessor()->IsLoopHeader());
    DCHECK(block_->end()->SecondSuccessor() == NULL ||
           order->Contains(block_->end()->SecondSuccessor()) ||
           block_->end()->SecondSuccessor()->IsLoopHeader());
    order->Add(block_, zone);
  }

  // This method is the basic block to walk up the stack.
  PostorderProcessor* Pop(Zone* zone,
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
                                ZoneList<HBasicBlock*>* order) {
    PostorderProcessor* parent = Pop(zone, order);
    while (parent != NULL) {
      PostorderProcessor* next =
          parent->PerformNonBacktrackingStep(zone, order);
      if (next != NULL) {
        return next;
      } else {
        parent = parent->Pop(zone, order);
      }
    }
    return NULL;
  }

  PostorderProcessor* PerformNonBacktrackingStep(
      Zone* zone,
      ZoneList<HBasicBlock*>* order) {
    HBasicBlock* next_block;
    switch (kind_) {
      case SUCCESSORS:
        next_block = AdvanceSuccessors();
        if (next_block != NULL) {
          PostorderProcessor* result = Push(zone);
          return result->SetupSuccessors(zone, next_block, loop_header_);
        }
        break;
      case SUCCESSORS_OF_LOOP_HEADER:
        next_block = AdvanceSuccessors();
        if (next_block != NULL) {
          PostorderProcessor* result = Push(zone);
          return result->SetupSuccessors(zone, next_block, block());
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
          return result->SetupSuccessors(zone, next_block, loop_header_);
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

#ifdef DEBUG
  // Initially the blocks must not be ordered.
  for (int i = 0; i < blocks_.length(); ++i) {
    DCHECK(!blocks_[i]->IsOrdered());
  }
#endif

  PostorderProcessor* postorder =
      PostorderProcessor::CreateEntryProcessor(zone(), blocks_[0]);
  blocks_.Rewind(0);
  while (postorder) {
    postorder = postorder->PerformStep(zone(), &blocks_);
  }

#ifdef DEBUG
  // Now all blocks must be marked as ordered.
  for (int i = 0; i < blocks_.length(); ++i) {
    DCHECK(blocks_[i]->IsOrdered());
  }
#endif

  // Reverse block list and assign block IDs.
  for (int i = 0, j = blocks_.length(); --j >= i; ++i) {
    HBasicBlock* bi = blocks_[i];
    HBasicBlock* bj = blocks_[j];
    bi->set_block_id(j);
    bj->set_block_id(i);
    blocks_[i] = bj;
    blocks_[j] = bi;
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
                             CompilationInfo* info, InliningKind inlining_kind,
                             int inlining_id, TailCallMode tail_call_mode)
    : owner_(owner),
      compilation_info_(info),
      call_context_(NULL),
      inlining_kind_(inlining_kind),
      tail_call_mode_(tail_call_mode),
      function_return_(NULL),
      test_context_(NULL),
      entry_(NULL),
      arguments_object_(NULL),
      arguments_elements_(NULL),
      inlining_id_(inlining_id),
      outer_source_position_(SourcePosition::Unknown()),
      do_expression_scope_count_(0),
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

  if (owner->is_tracking_positions()) {
    outer_source_position_ = owner->source_position();
    owner->EnterInlinedSource(inlining_id);
    owner->SetSourcePosition(info->shared_info()->start_position());
  }
}


FunctionState::~FunctionState() {
  delete test_context_;
  owner_->set_function_state(outer_);

  if (owner_->is_tracking_positions()) {
    owner_->set_source_position(outer_source_position_);
    owner_->EnterInlinedSource(outer_->inlining_id());
  }
}


// Implementation of utility classes to represent an expression's context in
// the AST.
AstContext::AstContext(HOptimizedGraphBuilder* owner, Expression::Context kind)
    : owner_(owner),
      kind_(kind),
      outer_(owner->ast_context()),
      typeof_mode_(NOT_INSIDE_TYPEOF) {
  owner->set_ast_context(this);  // Push.
#ifdef DEBUG
  DCHECK_EQ(JS_FUNCTION, owner->environment()->frame_type());
  original_length_ = owner->environment()->length();
#endif
}


AstContext::~AstContext() {
  owner_->set_ast_context(outer_);  // Pop.
}


EffectContext::~EffectContext() {
  DCHECK(owner()->HasStackOverflow() || owner()->current_block() == NULL ||
         (owner()->environment()->length() == original_length_ &&
          (owner()->environment()->frame_type() == JS_FUNCTION ||
           owner()->environment()->frame_type() == TAIL_CALLER_FUNCTION)));
}


ValueContext::~ValueContext() {
  DCHECK(owner()->HasStackOverflow() || owner()->current_block() == NULL ||
         (owner()->environment()->length() == original_length_ + 1 &&
          (owner()->environment()->frame_type() == JS_FUNCTION ||
           owner()->environment()->frame_type() == TAIL_CALLER_FUNCTION)));
}


void EffectContext::ReturnValue(HValue* value) {
  // The value is simply ignored.
}


void ValueContext::ReturnValue(HValue* value) {
  // The value is tracked in the bailout environment, and communicated
  // through the environment as the result of the expression.
  if (value->CheckFlag(HValue::kIsArguments)) {
    if (flag_ == ARGUMENTS_FAKED) {
      value = owner()->graph()->GetConstantUndefined();
    } else if (!arguments_allowed()) {
      owner()->Bailout(kBadValueContextForArgumentsValue);
    }
  }
  owner()->Push(value);
}


void TestContext::ReturnValue(HValue* value) {
  BuildBranch(value);
}


void EffectContext::ReturnInstruction(HInstruction* instr, BailoutId ast_id) {
  DCHECK(!instr->IsControlInstruction());
  owner()->AddInstruction(instr);
  if (instr->HasObservableSideEffects()) {
    owner()->Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
  }
}


void EffectContext::ReturnControl(HControlInstruction* instr,
                                  BailoutId ast_id) {
  DCHECK(!instr->HasObservableSideEffects());
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
  DCHECK(!instr->IsControlInstruction());
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
  DCHECK(!instr->HasObservableSideEffects());
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
  DCHECK(!instr->IsControlInstruction());
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
  DCHECK(!instr->HasObservableSideEffects());
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
  ToBooleanHints expected(condition()->to_boolean_types());
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
  current_info()->AbortOptimization(reason);
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
  for_value.set_typeof_mode(INSIDE_TYPEOF);
  Visit(expr);
}


void HOptimizedGraphBuilder::VisitForControl(Expression* expr,
                                             HBasicBlock* true_block,
                                             HBasicBlock* false_block) {
  TestContext for_control(this, expr, true_block, false_block);
  Visit(expr);
}


void HOptimizedGraphBuilder::VisitExpressions(
    ZoneList<Expression*>* exprs) {
  for (int i = 0; i < exprs->length(); ++i) {
    CHECK_ALIVE(VisitForValue(exprs->at(i)));
  }
}


void HOptimizedGraphBuilder::VisitExpressions(ZoneList<Expression*>* exprs,
                                              ArgumentsAllowedFlag flag) {
  for (int i = 0; i < exprs->length(); ++i) {
    CHECK_ALIVE(VisitForValue(exprs->at(i), flag));
  }
}


bool HOptimizedGraphBuilder::BuildGraph() {
  if (IsDerivedConstructor(current_info()->literal()->kind())) {
    Bailout(kSuperReference);
    return false;
  }

  DeclarationScope* scope = current_info()->scope();
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

  VisitDeclarations(scope->declarations());
  Add<HSimulate>(BailoutId::Declarations());

  Add<HStackCheck>(HStackCheck::kFunctionEntry);

  VisitStatements(current_info()->literal()->body());
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
  DCHECK(unoptimized_code->kind() == Code::FUNCTION);
  Handle<TypeFeedbackInfo> type_info(
      TypeFeedbackInfo::cast(unoptimized_code->type_feedback_info()));
  int checksum = type_info->own_type_change_checksum();
  int composite_checksum = graph()->update_type_change_checksum(checksum);
  graph()->set_use_optimistic_licm(
      !type_info->matches_inlined_type_change_checksum(composite_checksum));
  type_info->set_inlined_type_change_checksum(composite_checksum);

  // Set this predicate early to avoid handle deref during graph optimization.
  graph()->set_allow_code_motion(
      current_info()->IsStub() ||
      current_info()->shared_info()->deopt_count() + 1 < FLAG_max_deopt_count);

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

  Run<HRepresentationChangesPhase>();

  Run<HInferTypesPhase>();

  // Must be performed before canonicalization to ensure that Canonicalize
  // will not remove semantically meaningful ToInt32 operations e.g. BIT_OR with
  // zero.
  Run<HUint32AnalysisPhase>();

  if (FLAG_use_canonicalizing) Run<HCanonicalizePhase>();

  if (FLAG_use_gvn) Run<HGlobalValueNumberingPhase>();

  if (FLAG_check_elimination) Run<HCheckEliminationPhase>();

  if (FLAG_store_elimination) Run<HStoreEliminationPhase>();

  Run<HRangeAnalysisPhase>();

  // Eliminate redundant stack checks on backwards branches.
  Run<HStackCheckEliminationPhase>();

  if (FLAG_array_bounds_checks_elimination) Run<HBoundsCheckEliminationPhase>();
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
      DCHECK(phi->ActualValue() == phi);
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
        DCHECK(instruction->IsInformativeDefinition());
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

  HPushArguments* push_args = New<HPushArguments>();
  while (!arguments.is_empty()) {
    push_args->AddInput(arguments.RemoveLast());
  }
  AddInstruction(push_args);
}


template <class Instruction>
HInstruction* HOptimizedGraphBuilder::PreProcessCall(Instruction* call) {
  PushArgumentsFromEnvironment(call->argument_count());
  return call;
}

void HOptimizedGraphBuilder::SetUpScope(DeclarationScope* scope) {
  HEnvironment* prolog_env = environment();
  int parameter_count = environment()->parameter_count();
  ZoneList<HValue*> parameters(parameter_count, zone());
  for (int i = 0; i < parameter_count; ++i) {
    HInstruction* parameter = Add<HParameter>(static_cast<unsigned>(i));
    parameters.Add(parameter, zone());
    environment()->Bind(i, parameter);
  }

  HConstant* undefined_constant = graph()->GetConstantUndefined();
  // Initialize specials and locals to undefined.
  for (int i = parameter_count + 1; i < environment()->length(); ++i) {
    environment()->Bind(i, undefined_constant);
  }
  Add<HPrologue>();

  HEnvironment* initial_env = environment()->CopyWithoutHistory();
  HBasicBlock* body_entry = CreateBasicBlock(initial_env);
  GotoNoSimulate(body_entry);
  set_current_block(body_entry);

  // Initialize context of prolog environment to undefined.
  prolog_env->BindContext(undefined_constant);

  // First special is HContext.
  HInstruction* context = Add<HContext>();
  environment()->BindContext(context);

  // Create an arguments object containing the initial parameters.  Set the
  // initial values of parameters including "this" having parameter index 0.
  DCHECK_EQ(scope->num_parameters() + 1, parameter_count);
  HArgumentsObject* arguments_object = New<HArgumentsObject>(parameter_count);
  for (int i = 0; i < parameter_count; ++i) {
    HValue* parameter = parameters.at(i);
    arguments_object->AddArgument(parameter, zone());
  }

  AddInstruction(arguments_object);

  // Handle the arguments and arguments shadow variables specially (they do
  // not have declarations).
  if (scope->arguments() != NULL) {
    environment()->Bind(scope->arguments(), arguments_object);
  }

  if (scope->rest_parameter() != nullptr) {
    return Bailout(kRestParameter);
  }

  if (scope->this_function_var() != nullptr ||
      scope->new_target_var() != nullptr) {
    return Bailout(kSuperReference);
  }

  // Trace the call.
  if (FLAG_trace && top_info()->IsOptimizing()) {
    Add<HCallRuntime>(Runtime::FunctionForId(Runtime::kTraceEnter), 0);
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
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());

  Scope* outer_scope = scope();
  Scope* scope = stmt->scope();
  BreakAndContinueInfo break_info(stmt, outer_scope);

  { BreakAndContinueScope push(&break_info, this);
    if (scope != NULL) {
      if (scope->NeedsContext()) {
        // Load the function object.
        DeclarationScope* declaration_scope = scope->GetDeclarationScope();
        HInstruction* function;
        HValue* outer_context = environment()->context();
        if (declaration_scope->is_script_scope() ||
            declaration_scope->is_eval_scope()) {
          function = new (zone())
              HLoadContextSlot(outer_context, Context::CLOSURE_INDEX,
                               HLoadContextSlot::kNoCheck);
        } else {
          function = New<HThisFunction>();
        }
        AddInstruction(function);
        // Allocate a block context and store it to the stack frame.
        HValue* scope_info = Add<HConstant>(scope->scope_info());
        Add<HPushArguments>(scope_info, function);
        HInstruction* inner_context = Add<HCallRuntime>(
            Runtime::FunctionForId(Runtime::kPushBlockContext), 2);
        inner_context->SetFlag(HValue::kHasNoObservableSideEffects);
        set_scope(scope);
        environment()->BindContext(inner_context);
      }
      VisitDeclarations(scope->declarations());
      AddSimulate(stmt->DeclsId(), REMOVABLE_SIMULATE);
    }
    CHECK_BAILOUT(VisitStatements(stmt->statements()));
  }
  set_scope(outer_scope);
  if (scope != NULL && current_block() != NULL &&
      scope->ContextLocalCount() > 0) {
    HValue* inner_context = environment()->context();
    HValue* outer_context = Add<HLoadNamedField>(
        inner_context, nullptr,
        HObjectAccess::ForContextSlot(Context::PREVIOUS_INDEX));

    environment()->BindContext(outer_context);
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
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  VisitForEffect(stmt->expression());
}


void HOptimizedGraphBuilder::VisitEmptyStatement(EmptyStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
}


void HOptimizedGraphBuilder::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* stmt) {
  Visit(stmt->statement());
}


void HOptimizedGraphBuilder::VisitIfStatement(IfStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
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

    // Technically, we should be able to handle the case when one side of
    // the test is not connected, but this can trip up liveness analysis
    // if we did not fully connect the test context based on some optimistic
    // assumption. If such an assumption was violated, we would end up with
    // an environment with optimized-out values. So we should always
    // conservatively connect the test context.
    CHECK(cond_true->HasPredecessor());
    CHECK(cond_false->HasPredecessor());

    cond_true->SetJoinId(stmt->ThenId());
    set_current_block(cond_true);
    CHECK_BAILOUT(Visit(stmt->then_statement()));
    cond_true = current_block();

    cond_false->SetJoinId(stmt->ElseId());
    set_current_block(cond_false);
    CHECK_BAILOUT(Visit(stmt->else_statement()));
    cond_false = current_block();

    HBasicBlock* join = CreateJoin(cond_true, cond_false, stmt->IfId());
    set_current_block(join);
  }
}


HBasicBlock* HOptimizedGraphBuilder::BreakAndContinueScope::Get(
    BreakableStatement* stmt,
    BreakType type,
    Scope** scope,
    int* drop_extra) {
  *drop_extra = 0;
  BreakAndContinueScope* current = this;
  while (current != NULL && current->info()->target() != stmt) {
    *drop_extra += current->info()->drop_extra();
    current = current->next();
  }
  DCHECK(current != NULL);  // Always found (unless stack is malformed).
  *scope = current->info()->scope();

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
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());

  if (function_state()->IsInsideDoExpressionScope()) {
    return Bailout(kDoExpressionUnmodelable);
  }

  Scope* outer_scope = NULL;
  Scope* inner_scope = scope();
  int drop_extra = 0;
  HBasicBlock* continue_block = break_scope()->Get(
      stmt->target(), BreakAndContinueScope::CONTINUE,
      &outer_scope, &drop_extra);
  HValue* context = environment()->context();
  Drop(drop_extra);
  int context_pop_count = inner_scope->ContextChainLength(outer_scope);
  if (context_pop_count > 0) {
    while (context_pop_count-- > 0) {
      HInstruction* context_instruction = Add<HLoadNamedField>(
          context, nullptr,
          HObjectAccess::ForContextSlot(Context::PREVIOUS_INDEX));
      context = context_instruction;
    }
    environment()->BindContext(context);
  }

  Goto(continue_block);
  set_current_block(NULL);
}


void HOptimizedGraphBuilder::VisitBreakStatement(BreakStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());

  if (function_state()->IsInsideDoExpressionScope()) {
    return Bailout(kDoExpressionUnmodelable);
  }

  Scope* outer_scope = NULL;
  Scope* inner_scope = scope();
  int drop_extra = 0;
  HBasicBlock* break_block = break_scope()->Get(
      stmt->target(), BreakAndContinueScope::BREAK,
      &outer_scope, &drop_extra);
  HValue* context = environment()->context();
  Drop(drop_extra);
  int context_pop_count = inner_scope->ContextChainLength(outer_scope);
  if (context_pop_count > 0) {
    while (context_pop_count-- > 0) {
      HInstruction* context_instruction = Add<HLoadNamedField>(
          context, nullptr,
          HObjectAccess::ForContextSlot(Context::PREVIOUS_INDEX));
      context = context_instruction;
    }
    environment()->BindContext(context);
  }
  Goto(break_block);
  set_current_block(NULL);
}


void HOptimizedGraphBuilder::VisitReturnStatement(ReturnStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
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
      CHECK_ALIVE(VisitForEffect(stmt->expression()));
      context->ReturnValue(graph()->GetConstantTrue());
    } else if (context->IsEffect()) {
      CHECK_ALIVE(VisitForEffect(stmt->expression()));
      Goto(function_return(), state);
    } else {
      DCHECK(context->IsValue());
      CHECK_ALIVE(VisitForValue(stmt->expression()));
      HValue* return_value = Pop();
      HValue* receiver = environment()->arguments_environment()->Lookup(0);
      HHasInstanceTypeAndBranch* typecheck =
          New<HHasInstanceTypeAndBranch>(return_value,
                                         FIRST_JS_RECEIVER_TYPE,
                                         LAST_JS_RECEIVER_TYPE);
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
      DCHECK(context->IsValue());
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
      // Visit in value context and ignore the result. This is needed to keep
      // environment in sync with full-codegen since some visitors (e.g.
      // VisitCountOperation) use the operand stack differently depending on
      // context.
      CHECK_ALIVE(VisitForValue(stmt->expression()));
      Pop();
      Goto(function_return(), state);
    } else {
      DCHECK(context->IsValue());
      CHECK_ALIVE(VisitForValue(stmt->expression()));
      AddLeaveInlined(Pop(), state);
    }
  }
  set_current_block(NULL);
}


void HOptimizedGraphBuilder::VisitWithStatement(WithStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  return Bailout(kWithStatement);
}


void HOptimizedGraphBuilder::VisitSwitchStatement(SwitchStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());

  ZoneList<CaseClause*>* clauses = stmt->cases();
  int clause_count = clauses->length();
  ZoneList<HBasicBlock*> body_blocks(clause_count, zone());

  CHECK_ALIVE(VisitForValue(stmt->tag()));
  Add<HSimulate>(stmt->EntryId());
  HValue* tag_value = Top();
  AstType* tag_type = bounds_.get(stmt->tag()).lower;

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
    CHECK_BAILOUT(VisitForValue(clause->label()));
    if (current_block() == NULL) return Bailout(kUnsupportedSwitchStatement);
    HValue* label_value = Pop();

    AstType* label_type = bounds_.get(clause->label()).lower;
    AstType* combined_type = clause->compare_type();
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

  BreakAndContinueInfo break_info(stmt, scope());
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
                                           BailoutId stack_check_id,
                                           HBasicBlock* loop_entry) {
  Add<HSimulate>(stack_check_id);
  HStackCheck* stack_check =
      HStackCheck::cast(Add<HStackCheck>(HStackCheck::kBackwardsBranch));
  DCHECK(loop_entry->IsLoopHeader());
  loop_entry->loop_information()->set_stack_check(stack_check);
  CHECK_BAILOUT(Visit(stmt->body()));
}


void HOptimizedGraphBuilder::VisitDoWhileStatement(DoWhileStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  DCHECK(current_block() != NULL);
  HBasicBlock* loop_entry = BuildLoopEntry(stmt);

  BreakAndContinueInfo break_info(stmt, scope());
  {
    BreakAndContinueScope push(&break_info, this);
    CHECK_BAILOUT(VisitLoopBody(stmt, stmt->StackCheckId(), loop_entry));
  }
  HBasicBlock* body_exit = JoinContinue(
      stmt, stmt->ContinueId(), current_block(), break_info.continue_block());
  HBasicBlock* loop_successor = NULL;
  if (body_exit != NULL) {
    set_current_block(body_exit);
    loop_successor = graph()->CreateBasicBlock();
    if (stmt->cond()->ToBooleanIsFalse()) {
      loop_entry->loop_information()->stack_check()->Eliminate();
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
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  DCHECK(current_block() != NULL);
  HBasicBlock* loop_entry = BuildLoopEntry(stmt);

  // If the condition is constant true, do not generate a branch.
  HBasicBlock* loop_successor = NULL;
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

  BreakAndContinueInfo break_info(stmt, scope());
  if (current_block() != NULL) {
    BreakAndContinueScope push(&break_info, this);
    CHECK_BAILOUT(VisitLoopBody(stmt, stmt->StackCheckId(), loop_entry));
  }
  HBasicBlock* body_exit = JoinContinue(
      stmt, stmt->ContinueId(), current_block(), break_info.continue_block());
  HBasicBlock* loop_exit = CreateLoop(stmt,
                                      loop_entry,
                                      body_exit,
                                      loop_successor,
                                      break_info.break_block());
  set_current_block(loop_exit);
}


void HOptimizedGraphBuilder::VisitForStatement(ForStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  if (stmt->init() != NULL) {
    CHECK_ALIVE(Visit(stmt->init()));
  }
  DCHECK(current_block() != NULL);
  HBasicBlock* loop_entry = BuildLoopEntry(stmt);

  HBasicBlock* loop_successor = graph()->CreateBasicBlock();
  HBasicBlock* body_entry = graph()->CreateBasicBlock();
  if (stmt->cond() != NULL) {
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
  } else {
    // Create dummy control flow so that variable liveness analysis
    // produces teh correct result.
    HControlInstruction* branch = New<HBranch>(graph()->GetConstantTrue());
    branch->SetSuccessorAt(0, body_entry);
    branch->SetSuccessorAt(1, loop_successor);
    FinishCurrentBlock(branch);
    set_current_block(body_entry);
  }

  BreakAndContinueInfo break_info(stmt, scope());
  if (current_block() != NULL) {
    BreakAndContinueScope push(&break_info, this);
    CHECK_BAILOUT(VisitLoopBody(stmt, stmt->StackCheckId(), loop_entry));
  }
  HBasicBlock* body_exit = JoinContinue(
      stmt, stmt->ContinueId(), current_block(), break_info.continue_block());

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
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());

  if (!stmt->each()->IsVariableProxy() ||
      !stmt->each()->AsVariableProxy()->var()->IsStackLocal()) {
    return Bailout(kForInStatementWithNonLocalEachVariable);
  }

  Variable* each_var = stmt->each()->AsVariableProxy()->var();

  CHECK_ALIVE(VisitForValue(stmt->enumerable()));
  HValue* enumerable = Top();  // Leave enumerable at the top.

  IfBuilder if_undefined_or_null(this);
  if_undefined_or_null.If<HCompareObjectEqAndBranch>(
      enumerable, graph()->GetConstantUndefined());
  if_undefined_or_null.Or();
  if_undefined_or_null.If<HCompareObjectEqAndBranch>(
      enumerable, graph()->GetConstantNull());
  if_undefined_or_null.ThenDeopt(DeoptimizeReason::kUndefinedOrNullInForIn);
  if_undefined_or_null.End();
  BuildForInBody(stmt, each_var, enumerable);
}


void HOptimizedGraphBuilder::BuildForInBody(ForInStatement* stmt,
                                            Variable* each_var,
                                            HValue* enumerable) {
  Handle<Map> meta_map = isolate()->factory()->meta_map();
  bool fast = stmt->for_in_type() == ForInStatement::FAST_FOR_IN;
  BuildCheckHeapObject(enumerable);
  Add<HCheckInstanceType>(enumerable, HCheckInstanceType::IS_JS_RECEIVER);
  Add<HSimulate>(stmt->ToObjectId());
  if (fast) {
    HForInPrepareMap* map = Add<HForInPrepareMap>(enumerable);
    Push(map);
    Add<HSimulate>(stmt->EnumId());
    Drop(1);
    Add<HCheckMaps>(map, meta_map);

    HForInCacheArray* array = Add<HForInCacheArray>(
        enumerable, map, DescriptorArray::kEnumCacheBridgeCacheIndex);
    HValue* enum_length = BuildEnumLength(map);

    HForInCacheArray* index_cache = Add<HForInCacheArray>(
        enumerable, map, DescriptorArray::kEnumCacheBridgeIndicesCacheIndex);
    array->set_index_cache(index_cache);

    Push(map);
    Push(array);
    Push(enum_length);
    Add<HSimulate>(stmt->PrepareId());
  } else {
    Runtime::FunctionId function_id = Runtime::kForInEnumerate;
    Add<HPushArguments>(enumerable);
    HCallRuntime* array =
        Add<HCallRuntime>(Runtime::FunctionForId(function_id), 1);
    Push(array);
    Add<HSimulate>(stmt->EnumId());
    Drop(1);

    IfBuilder if_fast(this);
    if_fast.If<HCompareMap>(array, meta_map);
    if_fast.Then();
    {
      HValue* cache_map = array;
      HForInCacheArray* cache = Add<HForInCacheArray>(
          enumerable, cache_map, DescriptorArray::kEnumCacheBridgeCacheIndex);
      HValue* enum_length = BuildEnumLength(cache_map);
      Push(cache_map);
      Push(cache);
      Push(enum_length);
      Add<HSimulate>(stmt->PrepareId(), FIXED_SIMULATE);
    }
    if_fast.Else();
    {
      Push(graph()->GetConstant1());
      Push(array);
      Push(AddLoadFixedArrayLength(array));
      Add<HSimulate>(stmt->PrepareId(), FIXED_SIMULATE);
    }
  }

  Push(graph()->GetConstant0());

  HBasicBlock* loop_entry = BuildLoopEntry(stmt);

  // Reload the values to ensure we have up-to-date values inside of the loop.
  // This is relevant especially for OSR where the values don't come from the
  // computation above, but from the OSR entry block.
  HValue* index = environment()->ExpressionStackAt(0);
  HValue* limit = environment()->ExpressionStackAt(1);
  HValue* array = environment()->ExpressionStackAt(2);
  HValue* type = environment()->ExpressionStackAt(3);
  enumerable = environment()->ExpressionStackAt(4);

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

  // Compute the next enumerated value.
  HValue* key = Add<HLoadKeyed>(array, index, index, nullptr, FAST_ELEMENTS);

  HBasicBlock* continue_block = nullptr;
  if (fast) {
    // Check if expected map still matches that of the enumerable.
    Add<HCheckMapValue>(enumerable, type);
    Add<HSimulate>(stmt->FilterId());
  } else {
    // We need the continue block here to be able to skip over invalidated keys.
    continue_block = graph()->CreateBasicBlock();

    // We cannot use the IfBuilder here, since we need to be able to jump
    // over the loop body in case of undefined result from %ForInFilter,
    // and the poor soul that is the IfBuilder get's really confused about
    // such "advanced control flow requirements".
    HBasicBlock* if_fast = graph()->CreateBasicBlock();
    HBasicBlock* if_slow = graph()->CreateBasicBlock();
    HBasicBlock* if_slow_pass = graph()->CreateBasicBlock();
    HBasicBlock* if_slow_skip = graph()->CreateBasicBlock();
    HBasicBlock* if_join = graph()->CreateBasicBlock();

    // Check if expected map still matches that of the enumerable.
    HValue* enumerable_map =
        Add<HLoadNamedField>(enumerable, nullptr, HObjectAccess::ForMap());
    FinishCurrentBlock(
        New<HCompareObjectEqAndBranch>(enumerable_map, type, if_fast, if_slow));
    set_current_block(if_fast);
    {
      // The enum cache for enumerable is still valid, no need to check key.
      Push(key);
      Goto(if_join);
    }
    set_current_block(if_slow);
    {
      Callable callable = CodeFactory::ForInFilter(isolate());
      HValue* values[] = {key, enumerable};
      HConstant* stub_value = Add<HConstant>(callable.code());
      Push(Add<HCallWithDescriptor>(stub_value, 0, callable.descriptor(),
                                    ArrayVector(values)));
      Add<HSimulate>(stmt->FilterId());
      FinishCurrentBlock(New<HCompareObjectEqAndBranch>(
          Top(), graph()->GetConstantUndefined(), if_slow_skip, if_slow_pass));
    }
    set_current_block(if_slow_pass);
    { Goto(if_join); }
    set_current_block(if_slow_skip);
    {
      // The key is no longer valid for enumerable, skip it.
      Drop(1);
      Goto(continue_block);
    }
    if_join->SetJoinId(stmt->FilterId());
    set_current_block(if_join);
    key = Pop();
  }

  Bind(each_var, key);
  Add<HSimulate>(stmt->AssignmentId());

  BreakAndContinueInfo break_info(stmt, scope(), 5);
  break_info.set_continue_block(continue_block);
  {
    BreakAndContinueScope push(&break_info, this);
    CHECK_BAILOUT(VisitLoopBody(stmt, stmt->StackCheckId(), loop_entry));
  }

  HBasicBlock* body_exit = JoinContinue(
      stmt, stmt->IncrementId(), current_block(), break_info.continue_block());

  if (body_exit != NULL) {
    set_current_block(body_exit);

    HValue* current_index = Pop();
    HValue* increment =
        AddUncasted<HAdd>(current_index, graph()->GetConstant1());
    increment->ClearFlag(HValue::kCanOverflow);
    Push(increment);
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
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  return Bailout(kForOfStatement);
}


void HOptimizedGraphBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  return Bailout(kTryCatchStatement);
}


void HOptimizedGraphBuilder::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  return Bailout(kTryFinallyStatement);
}


void HOptimizedGraphBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  return Bailout(kDebuggerStatement);
}


void HOptimizedGraphBuilder::VisitCaseClause(CaseClause* clause) {
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  Handle<SharedFunctionInfo> shared_info = Compiler::GetSharedFunctionInfo(
      expr, current_info()->script(), top_info());
  // We also have a stack overflow if the recursive compilation did.
  if (HasStackOverflow()) return;
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need pretenuring.
  HConstant* shared_info_value = Add<HConstant>(shared_info);
  HInstruction* instr;
  Handle<FeedbackVector> vector(current_feedback_vector(), isolate());
  HValue* vector_value = Add<HConstant>(vector);
  int index = FeedbackVector::GetIndex(expr->LiteralFeedbackSlot());
  HValue* index_value = Add<HConstant>(index);
  if (!expr->pretenure()) {
    Callable callable = CodeFactory::FastNewClosure(isolate());
    HValue* values[] = {shared_info_value, vector_value, index_value};
    HConstant* stub_value = Add<HConstant>(callable.code());
    instr = New<HCallWithDescriptor>(stub_value, 0, callable.descriptor(),
                                     ArrayVector(values));
  } else {
    Add<HPushArguments>(shared_info_value);
    Add<HPushArguments>(vector_value);
    Add<HPushArguments>(index_value);
    Runtime::FunctionId function_id =
        expr->pretenure() ? Runtime::kNewClosure_Tenured : Runtime::kNewClosure;
    instr = New<HCallRuntime>(Runtime::FunctionForId(function_id), 3);
  }
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitClassLiteral(ClassLiteral* lit) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  return Bailout(kClassLiteral);
}


void HOptimizedGraphBuilder::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  return Bailout(kNativeFunctionLiteral);
}


void HOptimizedGraphBuilder::VisitDoExpression(DoExpression* expr) {
  DoExpressionScope scope(this);
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  CHECK_ALIVE(VisitBlock(expr->block()));
  Visit(expr->result());
}


void HOptimizedGraphBuilder::VisitConditional(Conditional* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
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

bool HOptimizedGraphBuilder::CanInlineGlobalPropertyAccess(
    Variable* var, LookupIterator* it, PropertyAccessType access_type) {
  if (var->is_this()) return false;
  return CanInlineGlobalPropertyAccess(it, access_type);
}

bool HOptimizedGraphBuilder::CanInlineGlobalPropertyAccess(
    LookupIterator* it, PropertyAccessType access_type) {
  if (!current_info()->has_global_object()) {
    return false;
  }

  switch (it->state()) {
    case LookupIterator::ACCESSOR:
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::INTERCEPTOR:
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::NOT_FOUND:
      return false;
    case LookupIterator::DATA:
      if (access_type == STORE && it->IsReadOnly()) return false;
      if (!it->GetHolder<JSObject>()->IsJSGlobalObject()) return false;
      return true;
    case LookupIterator::JSPROXY:
    case LookupIterator::TRANSITION:
      UNREACHABLE();
  }
  UNREACHABLE();
  return false;
}


HValue* HOptimizedGraphBuilder::BuildContextChainWalk(Variable* var) {
  DCHECK(var->IsContextSlot());
  HValue* context = environment()->context();
  int length = scope()->ContextChainLength(var->scope());
  while (length-- > 0) {
    context = Add<HLoadNamedField>(
        context, nullptr,
        HObjectAccess::ForContextSlot(Context::PREVIOUS_INDEX));
  }
  return context;
}

void HOptimizedGraphBuilder::InlineGlobalPropertyLoad(LookupIterator* it,
                                                      BailoutId ast_id) {
  Handle<PropertyCell> cell = it->GetPropertyCell();
  top_info()->dependencies()->AssumePropertyCell(cell);
  auto cell_type = it->property_details().cell_type();
  if (cell_type == PropertyCellType::kConstant ||
      cell_type == PropertyCellType::kUndefined) {
    Handle<Object> constant_object(cell->value(), isolate());
    if (constant_object->IsConsString()) {
      constant_object = String::Flatten(Handle<String>::cast(constant_object));
    }
    HConstant* constant = New<HConstant>(constant_object);
    return ast_context()->ReturnInstruction(constant, ast_id);
  } else {
    auto access = HObjectAccess::ForPropertyCellValue();
    UniqueSet<Map>* field_maps = nullptr;
    if (cell_type == PropertyCellType::kConstantType) {
      switch (cell->GetConstantType()) {
        case PropertyCellConstantType::kSmi:
          access = access.WithRepresentation(Representation::Smi());
          break;
        case PropertyCellConstantType::kStableMap: {
          // Check that the map really is stable. The heap object could
          // have mutated without the cell updating state. In that case,
          // make no promises about the loaded value except that it's a
          // heap object.
          access = access.WithRepresentation(Representation::HeapObject());
          Handle<Map> map(HeapObject::cast(cell->value())->map());
          if (map->is_stable()) {
            field_maps = new (zone())
                UniqueSet<Map>(Unique<Map>::CreateImmovable(map), zone());
          }
          break;
        }
      }
    }
    HConstant* cell_constant = Add<HConstant>(cell);
    HLoadNamedField* instr;
    if (field_maps == nullptr) {
      instr = New<HLoadNamedField>(cell_constant, nullptr, access);
    } else {
      instr = New<HLoadNamedField>(cell_constant, nullptr, access, field_maps,
                                   HType::HeapObject());
    }
    instr->ClearDependsOnFlag(kInobjectFields);
    instr->SetDependsOnFlag(kGlobalVars);
    return ast_context()->ReturnInstruction(instr, ast_id);
  }
}

void HOptimizedGraphBuilder::VisitVariableProxy(VariableProxy* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  Variable* variable = expr->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      if (IsLexicalVariableMode(variable->mode())) {
        // TODO(rossberg): should this be an DCHECK?
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

      Handle<JSGlobalObject> global(current_info()->global_object());

      // Lookup in script contexts.
      {
        Handle<ScriptContextTable> script_contexts(
            global->native_context()->script_context_table());
        ScriptContextTable::LookupResult lookup;
        if (ScriptContextTable::Lookup(script_contexts, variable->name(),
                                       &lookup)) {
          Handle<Context> script_context = ScriptContextTable::GetContext(
              script_contexts, lookup.context_index);
          Handle<Object> current_value =
              FixedArray::get(*script_context, lookup.slot_index, isolate());

          // If the values is not the hole, it will stay initialized,
          // so no need to generate a check.
          if (current_value->IsTheHole(isolate())) {
            return Bailout(kReferenceToUninitializedVariable);
          }
          HInstruction* result = New<HLoadNamedField>(
              Add<HConstant>(script_context), nullptr,
              HObjectAccess::ForContextSlot(lookup.slot_index));
          return ast_context()->ReturnInstruction(result, expr->id());
        }
      }

      LookupIterator it(global, variable->name(), LookupIterator::OWN);
      it.TryLookupCachedProperty();
      if (CanInlineGlobalPropertyAccess(variable, &it, LOAD)) {
        InlineGlobalPropertyLoad(&it, expr->id());
        return;
      } else {
        Handle<FeedbackVector> vector(current_feedback_vector(), isolate());
        FeedbackSlot slot = expr->VariableFeedbackSlot();
        DCHECK(vector->IsLoadGlobalIC(slot));

        HValue* vector_value = Add<HConstant>(vector);
        HValue* slot_value = Add<HConstant>(vector->GetIndex(slot));
        Callable callable = CodeFactory::LoadGlobalICInOptimizedCode(
            isolate(), ast_context()->typeof_mode());
        HValue* stub = Add<HConstant>(callable.code());
        HValue* name = Add<HConstant>(variable->name());
        HValue* values[] = {name, slot_value, vector_value};
        HCallWithDescriptor* instr = New<HCallWithDescriptor>(
            Code::LOAD_GLOBAL_IC, stub, 0, callable.descriptor(),
            ArrayVector(values));
        return ast_context()->ReturnInstruction(instr, expr->id());
      }
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      HValue* value = LookupAndMakeLive(variable);
      if (value == graph()->GetConstantHole()) {
        DCHECK(IsDeclaredVariableMode(variable->mode()) &&
               variable->mode() != VAR);
        return Bailout(kReferenceToUninitializedVariable);
      }
      return ast_context()->ReturnValue(value);
    }

    case VariableLocation::CONTEXT: {
      HValue* context = BuildContextChainWalk(variable);
      HLoadContextSlot::Mode mode;
      switch (variable->mode()) {
        case LET:
        case CONST:
          mode = HLoadContextSlot::kCheckDeoptimize;
          break;
        default:
          mode = HLoadContextSlot::kNoCheck;
          break;
      }
      HLoadContextSlot* instr =
          new(zone()) HLoadContextSlot(context, variable->index(), mode);
      return ast_context()->ReturnInstruction(instr, expr->id());
    }

    case VariableLocation::LOOKUP:
      return Bailout(kReferenceToAVariableWhichRequiresDynamicLookup);

    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void HOptimizedGraphBuilder::VisitLiteral(Literal* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  HConstant* instr = New<HConstant>(expr->value());
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitRegExpLiteral(RegExpLiteral* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  Callable callable = CodeFactory::FastCloneRegExp(isolate());
  int index = FeedbackVector::GetIndex(expr->literal_slot());
  HValue* values[] = {AddThisFunction(), Add<HConstant>(index),
                      Add<HConstant>(expr->pattern()),
                      Add<HConstant>(expr->flags())};
  HConstant* stub_value = Add<HConstant>(callable.code());
  HInstruction* instr = New<HCallWithDescriptor>(
      stub_value, 0, callable.descriptor(), ArrayVector(values));
  return ast_context()->ReturnInstruction(instr, expr->id());
}


static bool CanInlinePropertyAccess(Handle<Map> map) {
  if (map->instance_type() == HEAP_NUMBER_TYPE) return true;
  if (map->instance_type() < FIRST_NONSTRING_TYPE) return true;
  return map->IsJSObjectMap() && !map->is_dictionary_map() &&
         !map->has_named_interceptor() &&
         // TODO(verwaest): Whitelist contexts to which we have access.
         !map->is_access_check_needed();
}


// Determines whether the given array or object literal boilerplate satisfies
// all limits to be considered for fast deep-copying and computes the total
// size of all objects that are part of the graph.
static bool IsFastLiteral(Handle<JSObject> boilerplate,
                          int max_depth,
                          int* max_properties) {
  if (boilerplate->map()->is_deprecated() &&
      !JSObject::TryMigrateInstance(boilerplate)) {
    return false;
  }

  DCHECK(max_depth >= 0 && *max_properties >= 0);
  if (max_depth == 0) return false;

  Isolate* isolate = boilerplate->GetIsolate();
  Handle<FixedArrayBase> elements(boilerplate->elements());
  if (elements->length() > 0 &&
      elements->map() != isolate->heap()->fixed_cow_array_map()) {
    if (boilerplate->HasFastSmiOrObjectElements()) {
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
    } else if (boilerplate->HasFastDoubleElements()) {
      if (elements->Size() > kMaxRegularHeapObjectSize) return false;
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
      if (details.location() != kField) continue;
      DCHECK_EQ(kData, details.kind());
      if ((*max_properties)-- == 0) return false;
      FieldIndex field_index = FieldIndex::ForDescriptor(boilerplate->map(), i);
      if (boilerplate->IsUnboxedDoubleField(field_index)) continue;
      Handle<Object> value(boilerplate->RawFastPropertyAt(field_index),
                           isolate);
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
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());

  Handle<JSFunction> closure = function_state()->compilation_info()->closure();
  HInstruction* literal;

  // Check whether to use fast or slow deep-copying for boilerplate.
  int max_properties = kMaxFastLiteralProperties;
  Handle<Object> literals_cell(
      closure->feedback_vector()->Get(expr->literal_slot()), isolate());
  Handle<AllocationSite> site;
  Handle<JSObject> boilerplate;
  if (!literals_cell->IsUndefined(isolate())) {
    // Retrieve the boilerplate
    site = Handle<AllocationSite>::cast(literals_cell);
    boilerplate = Handle<JSObject>(JSObject::cast(site->transition_info()),
                                   isolate());
  }

  if (!boilerplate.is_null() &&
      IsFastLiteral(boilerplate, kMaxFastLiteralDepth, &max_properties)) {
    AllocationSiteUsageContext site_context(isolate(), site, false);
    site_context.EnterNewScope();
    literal = BuildFastLiteral(boilerplate, &site_context);
    site_context.ExitScope(site, boilerplate);
  } else {
    NoObservableSideEffectsScope no_effects(this);
    Handle<BoilerplateDescription> constant_properties =
        expr->GetOrBuildConstantProperties(isolate());
    int literal_index = FeedbackVector::GetIndex(expr->literal_slot());
    int flags = expr->ComputeFlags(true);

    Add<HPushArguments>(AddThisFunction(), Add<HConstant>(literal_index),
                        Add<HConstant>(constant_properties),
                        Add<HConstant>(flags));

    Runtime::FunctionId function_id = Runtime::kCreateObjectLiteral;
    literal = Add<HCallRuntime>(Runtime::FunctionForId(function_id), 4);
  }

  // The object is expected in the bailout environment during computation
  // of the property values and is the value of the entire expression.
  Push(literal);
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    if (property->is_computed_name()) return Bailout(kComputedPropertyName);
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key()->AsLiteral();
    Expression* value = property->value();

    switch (property->kind()) {
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(value));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->IsStringLiteral()) {
          DCHECK(key->IsPropertyName());
          if (property->emit_store()) {
            CHECK_ALIVE(VisitForValue(value));
            HValue* value = Pop();

            Handle<Map> map = property->GetReceiverType();
            Handle<String> name = key->AsPropertyName();
            HValue* store;
            FeedbackSlot slot = property->GetSlot();
            if (map.is_null()) {
              // If we don't know the monomorphic type, do a generic store.
              CHECK_ALIVE(store = BuildNamedGeneric(STORE, NULL, slot, literal,
                                                    name, value));
            } else {
              PropertyAccessInfo info(this, STORE, map, name);
              if (info.CanAccessMonomorphic()) {
                HValue* checked_literal = Add<HCheckMaps>(literal, map);
                DCHECK(!info.IsAccessorConstant());
                info.MarkAsInitializingStore();
                store = BuildMonomorphicAccess(
                    &info, literal, checked_literal, value,
                    BailoutId::None(), BailoutId::None());
                DCHECK_NOT_NULL(store);
              } else {
                CHECK_ALIVE(store = BuildNamedGeneric(STORE, NULL, slot,
                                                      literal, name, value));
              }
            }
            if (store->IsInstruction()) {
              AddInstruction(HInstruction::cast(store));
            }
            DCHECK(store->HasObservableSideEffects());
            Add<HSimulate>(key->id(), REMOVABLE_SIMULATE);

            // Add [[HomeObject]] to function literals.
            if (FunctionLiteral::NeedsHomeObject(property->value())) {
              Handle<Symbol> sym = isolate()->factory()->home_object_symbol();
              HInstruction* store_home = BuildNamedGeneric(
                  STORE, NULL, property->GetSlot(1), value, sym, literal);
              AddInstruction(store_home);
              DCHECK(store_home->HasObservableSideEffects());
              Add<HSimulate>(property->value()->id(), REMOVABLE_SIMULATE);
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

  return ast_context()->ReturnValue(Pop());
}


void HOptimizedGraphBuilder::VisitArrayLiteral(ArrayLiteral* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();
  HInstruction* literal;

  Handle<AllocationSite> site;
  Handle<FeedbackVector> vector(environment()->closure()->feedback_vector(),
                                isolate());
  Handle<Object> literals_cell(vector->Get(expr->literal_slot()), isolate());
  Handle<JSObject> boilerplate_object;
  if (!literals_cell->IsUndefined(isolate())) {
    DCHECK(literals_cell->IsAllocationSite());
    site = Handle<AllocationSite>::cast(literals_cell);
    boilerplate_object = Handle<JSObject>(
        JSObject::cast(site->transition_info()), isolate());
  }

  // Check whether to use fast or slow deep-copying for boilerplate.
  int max_properties = kMaxFastLiteralProperties;
  if (!boilerplate_object.is_null() &&
      IsFastLiteral(boilerplate_object, kMaxFastLiteralDepth,
                    &max_properties)) {
    DCHECK(site->SitePointsToLiteral());
    AllocationSiteUsageContext site_context(isolate(), site, false);
    site_context.EnterNewScope();
    literal = BuildFastLiteral(boilerplate_object, &site_context);
    site_context.ExitScope(site, boilerplate_object);
  } else {
    NoObservableSideEffectsScope no_effects(this);
    Handle<ConstantElementsPair> constants =
        expr->GetOrBuildConstantElements(isolate());
    int literal_index = FeedbackVector::GetIndex(expr->literal_slot());
    int flags = expr->ComputeFlags(true);

    Add<HPushArguments>(AddThisFunction(), Add<HConstant>(literal_index),
                        Add<HConstant>(constants), Add<HConstant>(flags));

    Runtime::FunctionId function_id = Runtime::kCreateArrayLiteral;
    literal = Add<HCallRuntime>(Runtime::FunctionForId(function_id), 4);

    // Register to deopt if the boilerplate ElementsKind changes.
    if (!site.is_null()) {
      top_info()->dependencies()->AssumeTransitionStable(site);
    }
  }

  // The array is expected in the bailout environment during computation
  // of the property values and is the value of the entire expression.
  Push(literal);

  HInstruction* elements = NULL;

  for (int i = 0; i < length; i++) {
    Expression* subexpr = subexprs->at(i);
    DCHECK(!subexpr->IsSpread());

    // If the subexpression is a literal or a simple materialized literal it
    // is already set in the cloned array.
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    CHECK_ALIVE(VisitForValue(subexpr));
    HValue* value = Pop();
    if (!Smi::IsValid(i)) return Bailout(kNonSmiKeyInArrayLiteral);

    elements = AddLoadElements(literal);

    HValue* key = Add<HConstant>(i);

    if (!boilerplate_object.is_null()) {
      ElementsKind boilerplate_elements_kind =
          boilerplate_object->GetElementsKind();
      switch (boilerplate_elements_kind) {
        case FAST_SMI_ELEMENTS:
        case FAST_HOLEY_SMI_ELEMENTS:
        case FAST_ELEMENTS:
        case FAST_HOLEY_ELEMENTS:
        case FAST_DOUBLE_ELEMENTS:
        case FAST_HOLEY_DOUBLE_ELEMENTS: {
          Add<HStoreKeyed>(elements, key, value, nullptr,
                           boilerplate_elements_kind);
          break;
        }
        default:
          UNREACHABLE();
          break;
      }
    } else {
      HInstruction* instr = BuildKeyedGeneric(
          STORE, expr, expr->LiteralFeedbackSlot(), literal, key, value);
      AddInstruction(instr);
    }

    Add<HSimulate>(expr->GetIdForElement(i));
  }

  return ast_context()->ReturnValue(Pop());
}


HCheckMaps* HOptimizedGraphBuilder::AddCheckMap(HValue* object,
                                                Handle<Map> map) {
  BuildCheckHeapObject(object);
  return Add<HCheckMaps>(object, map);
}


HInstruction* HOptimizedGraphBuilder::BuildLoadNamedField(
    PropertyAccessInfo* info,
    HValue* checked_object) {
  // Check if this is a load of an immutable or constant property.
  if (checked_object->ActualValue()->IsConstant()) {
    Handle<Object> object(
        HConstant::cast(checked_object->ActualValue())->handle(isolate()));

    if (object->IsJSObject()) {
      LookupIterator it(object, info->name(),
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (it.IsFound()) {
        bool is_reaonly_non_configurable =
            it.IsReadOnly() && !it.IsConfigurable();
        if (is_reaonly_non_configurable ||
            (FLAG_track_constant_fields && info->IsDataConstantField())) {
          Handle<Object> value = JSReceiver::GetDataProperty(&it);
          if (!is_reaonly_non_configurable) {
            DCHECK(!it.is_dictionary_holder());
            // Add dependency on the map that introduced the field.
            Handle<Map> field_owner_map = it.GetFieldOwnerMap();
            top_info()->dependencies()->AssumeFieldOwner(field_owner_map);
          }
          return New<HConstant>(value);
        }
      }
    }
  }

  HObjectAccess access = info->access();
  if (access.representation().IsDouble() &&
      (!FLAG_unbox_double_fields || !access.IsInobject())) {
    // Load the heap number.
    checked_object = Add<HLoadNamedField>(
        checked_object, nullptr,
        access.WithRepresentation(Representation::Tagged()));
    // Load the double value from it.
    access = HObjectAccess::ForHeapNumberValue();
  }

  SmallMapList* map_list = info->field_maps();
  if (map_list->length() == 0) {
    return New<HLoadNamedField>(checked_object, checked_object, access);
  }

  UniqueSet<Map>* maps = new(zone()) UniqueSet<Map>(map_list->length(), zone());
  for (int i = 0; i < map_list->length(); ++i) {
    maps->Add(Unique<Map>::CreateImmovable(map_list->at(i)), zone());
  }
  return New<HLoadNamedField>(
      checked_object, checked_object, access, maps, info->field_type());
}

HValue* HOptimizedGraphBuilder::BuildStoreNamedField(PropertyAccessInfo* info,
                                                     HValue* checked_object,
                                                     HValue* value) {
  bool transition_to_field = info->IsTransition();
  // TODO(verwaest): Move this logic into PropertyAccessInfo.
  HObjectAccess field_access = info->access();

  bool store_to_constant_field = FLAG_track_constant_fields &&
                                 info->StoreMode() != INITIALIZING_STORE &&
                                 info->IsDataConstantField();

  HStoreNamedField *instr;
  if (field_access.representation().IsDouble() &&
      (!FLAG_unbox_double_fields || !field_access.IsInobject())) {
    HObjectAccess heap_number_access =
        field_access.WithRepresentation(Representation::Tagged());
    if (transition_to_field) {
      // The store requires a mutable HeapNumber to be allocated.
      NoObservableSideEffectsScope no_side_effects(this);
      HInstruction* heap_number_size = Add<HConstant>(HeapNumber::kSize);

      // TODO(hpayer): Allocation site pretenuring support.
      HInstruction* heap_number =
          Add<HAllocate>(heap_number_size, HType::HeapObject(), NOT_TENURED,
                         MUTABLE_HEAP_NUMBER_TYPE, graph()->GetConstant0());
      AddStoreMapConstant(
          heap_number, isolate()->factory()->mutable_heap_number_map());
      Add<HStoreNamedField>(heap_number, HObjectAccess::ForHeapNumberValue(),
                            value);
      instr = New<HStoreNamedField>(checked_object->ActualValue(),
                                    heap_number_access,
                                    heap_number);
    } else {
      // Already holds a HeapNumber; load the box and write its value field.
      HInstruction* heap_number =
          Add<HLoadNamedField>(checked_object, nullptr, heap_number_access);

      if (store_to_constant_field) {
        // If the field is constant check that the value we are going to store
        // matches current value.
        HInstruction* current_value = Add<HLoadNamedField>(
            heap_number, nullptr, HObjectAccess::ForHeapNumberValue());
        IfBuilder value_checker(this);
        value_checker.IfNot<HCompareNumericAndBranch>(current_value, value,
                                                      Token::EQ);
        value_checker.ThenDeopt(DeoptimizeReason::kValueMismatch);
        value_checker.End();
        return nullptr;

      } else {
        instr = New<HStoreNamedField>(heap_number,
                                      HObjectAccess::ForHeapNumberValue(),
                                      value, STORE_TO_INITIALIZED_ENTRY);
      }
    }
  } else {
    if (store_to_constant_field) {
      // If the field is constant check that the value we are going to store
      // matches current value.
      HInstruction* current_value = Add<HLoadNamedField>(
          checked_object->ActualValue(), checked_object, field_access);

      IfBuilder value_checker(this);
      if (field_access.representation().IsDouble()) {
        value_checker.IfNot<HCompareNumericAndBranch>(current_value, value,
                                                      Token::EQ);
      } else {
        value_checker.IfNot<HCompareObjectEqAndBranch>(current_value, value);
      }
      value_checker.ThenDeopt(DeoptimizeReason::kValueMismatch);
      value_checker.End();
      return nullptr;

    } else {
      if (field_access.representation().IsHeapObject()) {
        BuildCheckHeapObject(value);
      }

      if (!info->field_maps()->is_empty()) {
        DCHECK(field_access.representation().IsHeapObject());
        value = Add<HCheckMaps>(value, info->field_maps());
      }

      // This is a normal store.
      instr = New<HStoreNamedField>(checked_object->ActualValue(), field_access,
                                    value, info->StoreMode());
    }
  }

  if (transition_to_field) {
    Handle<Map> transition(info->transition());
    DCHECK(!transition->is_deprecated());
    instr->SetTransition(Add<HConstant>(transition));
  }
  return instr;
}

Handle<FieldType>
HOptimizedGraphBuilder::PropertyAccessInfo::GetFieldTypeFromMap(
    Handle<Map> map) const {
  DCHECK(IsFound());
  DCHECK(number_ < map->NumberOfOwnDescriptors());
  return handle(map->instance_descriptors()->GetFieldType(number_), isolate());
}

bool HOptimizedGraphBuilder::PropertyAccessInfo::IsCompatible(
    PropertyAccessInfo* info) {
  if (!CanInlinePropertyAccess(map_)) return false;

  // Currently only handle AstType::Number as a polymorphic case.
  // TODO(verwaest): Support monomorphic handling of numbers with a HCheckNumber
  // instruction.
  if (IsNumberType()) return false;

  // Values are only compatible for monomorphic load if they all behave the same
  // regarding value wrappers.
  if (IsValueWrapped() != info->IsValueWrapped()) return false;

  if (!LookupDescriptor()) return false;

  if (!IsFound()) {
    return (!info->IsFound() || info->has_holder()) &&
           map()->prototype() == info->map()->prototype();
  }

  // Mismatch if the other access info found the property in the prototype
  // chain.
  if (info->has_holder()) return false;

  if (IsAccessorConstant()) {
    return accessor_.is_identical_to(info->accessor_) &&
        api_holder_.is_identical_to(info->api_holder_);
  }

  if (IsDataConstant()) {
    return constant_.is_identical_to(info->constant_);
  }

  DCHECK(IsData());
  if (!info->IsData()) return false;

  Representation r = access_.representation();
  if (IsLoad()) {
    if (!info->access_.representation().IsCompatibleForLoad(r)) return false;
  } else {
    if (!info->access_.representation().IsCompatibleForStore(r)) return false;
  }
  if (info->access_.offset() != access_.offset()) return false;
  if (info->access_.IsInobject() != access_.IsInobject()) return false;
  if (IsLoad()) {
    if (field_maps_.is_empty()) {
      info->field_maps_.Clear();
    } else if (!info->field_maps_.is_empty()) {
      for (int i = 0; i < field_maps_.length(); ++i) {
        info->field_maps_.AddMapIfMissing(field_maps_.at(i), info->zone());
      }
      info->field_maps_.Sort();
    }
  } else {
    // We can only merge stores that agree on their field maps. The comparison
    // below is safe, since we keep the field maps sorted.
    if (field_maps_.length() != info->field_maps_.length()) return false;
    for (int i = 0; i < field_maps_.length(); ++i) {
      if (!field_maps_.at(i).is_identical_to(info->field_maps_.at(i))) {
        return false;
      }
    }
  }
  info->GeneralizeRepresentation(r);
  info->field_type_ = info->field_type_.Combine(field_type_);
  return true;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LookupDescriptor() {
  if (!map_->IsJSObjectMap()) return true;
  LookupDescriptor(*map_, *name_);
  return LoadResult(map_);
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LoadResult(Handle<Map> map) {
  if (!IsLoad() && IsProperty() && IsReadOnly()) {
    return false;
  }

  if (IsData()) {
    // Construct the object field access.
    int index = GetLocalFieldIndexFromMap(map);
    access_ = HObjectAccess::ForField(map, index, representation(), name_);

    // Load field map for heap objects.
    return LoadFieldMaps(map);
  } else if (IsAccessorConstant()) {
    Handle<Object> accessors = GetAccessorsFromMap(map);
    if (!accessors->IsAccessorPair()) return false;
    Object* raw_accessor =
        IsLoad() ? Handle<AccessorPair>::cast(accessors)->getter()
                 : Handle<AccessorPair>::cast(accessors)->setter();
    if (!raw_accessor->IsJSFunction() &&
        !raw_accessor->IsFunctionTemplateInfo())
      return false;
    Handle<Object> accessor = handle(HeapObject::cast(raw_accessor));
    CallOptimization call_optimization(accessor);
    if (call_optimization.is_simple_api_call()) {
      CallOptimization::HolderLookup holder_lookup;
      api_holder_ =
          call_optimization.LookupHolderOfExpectedType(map_, &holder_lookup);
    }
    accessor_ = accessor;
  } else if (IsDataConstant()) {
    constant_ = GetConstantFromMap(map);
  }

  return true;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LoadFieldMaps(
    Handle<Map> map) {
  // Clear any previously collected field maps/type.
  field_maps_.Clear();
  field_type_ = HType::Tagged();

  // Figure out the field type from the accessor map.
  Handle<FieldType> field_type = GetFieldTypeFromMap(map);

  // Collect the (stable) maps from the field type.
  if (field_type->IsClass()) {
    DCHECK(access_.representation().IsHeapObject());
    Handle<Map> field_map = field_type->AsClass();
    if (field_map->is_stable()) {
      field_maps_.Add(field_map, zone());
    }
  }

  if (field_maps_.is_empty()) {
    // Store is not safe if the field map was cleared.
    return IsLoad() || !field_type->IsNone();
  }

  // Determine field HType from field type.
  field_type_ = HType::FromFieldType(field_type, zone());
  DCHECK(field_type_.IsHeapObject());

  // Add dependency on the map that introduced the field.
  top_info()->dependencies()->AssumeFieldOwner(GetFieldOwnerFromMap(map));
  return true;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::LookupInPrototypes() {
  Handle<Map> map = this->map();
  if (name_->IsPrivate()) {
    NotFound();
    return !map->has_hidden_prototype();
  }

  while (map->prototype()->IsJSObject()) {
    holder_ = handle(JSObject::cast(map->prototype()));
    if (holder_->map()->is_deprecated()) {
      JSObject::TryMigrateInstance(holder_);
    }
    map = Handle<Map>(holder_->map());
    if (!CanInlinePropertyAccess(map)) {
      NotFound();
      return false;
    }
    LookupDescriptor(*map, *name_);
    if (IsFound()) return LoadResult(map);
  }

  NotFound();
  return !map->prototype()->IsJSReceiver();
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::IsIntegerIndexedExotic() {
  InstanceType instance_type = map_->instance_type();
  return instance_type == JS_TYPED_ARRAY_TYPE && name_->IsString() &&
         IsSpecialIndex(isolate()->unicode_cache(), String::cast(*name_));
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::CanAccessMonomorphic() {
  if (!CanInlinePropertyAccess(map_)) return false;
  if (IsJSObjectFieldAccessor()) return IsLoad();
  if (map_->IsJSFunctionMap() && map_->is_constructor() &&
      !map_->has_non_instance_prototype() &&
      name_.is_identical_to(isolate()->factory()->prototype_string())) {
    return IsLoad();
  }
  if (!LookupDescriptor()) return false;
  if (IsFound()) return IsLoad() || !IsReadOnly();
  if (IsIntegerIndexedExotic()) return false;
  if (!LookupInPrototypes()) return false;
  if (IsLoad()) return true;

  if (IsAccessorConstant()) return true;
  LookupTransition(*map_, *name_, NONE);
  if (IsTransitionToData() && map_->unused_property_fields() > 0) {
    // Construct the object field access.
    int descriptor = transition()->LastAdded();
    int index =
        transition()->instance_descriptors()->GetFieldIndex(descriptor) -
        map_->GetInObjectProperties();
    PropertyDetails details =
        transition()->instance_descriptors()->GetDetails(descriptor);
    Representation representation = details.representation();
    access_ = HObjectAccess::ForField(map_, index, representation, name_);

    // Load field map for heap objects.
    return LoadFieldMaps(transition());
  }
  return false;
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::CanAccessAsMonomorphic(
    SmallMapList* maps) {
  DCHECK(map_.is_identical_to(maps->first()));
  if (!CanAccessMonomorphic()) return false;
  STATIC_ASSERT(kMaxLoadPolymorphism == kMaxStorePolymorphism);
  if (maps->length() > kMaxLoadPolymorphism) return false;
  HObjectAccess access = HObjectAccess::ForMap();  // bogus default
  if (GetJSObjectFieldAccess(&access)) {
    for (int i = 1; i < maps->length(); ++i) {
      PropertyAccessInfo test_info(builder_, access_type_, maps->at(i), name_);
      HObjectAccess test_access = HObjectAccess::ForMap();  // bogus default
      if (!test_info.GetJSObjectFieldAccess(&test_access)) return false;
      if (!access.Equals(test_access)) return false;
    }
    return true;
  }

  // Currently only handle numbers as a polymorphic case.
  // TODO(verwaest): Support monomorphic handling of numbers with a HCheckNumber
  // instruction.
  if (IsNumberType()) return false;

  // Multiple maps cannot transition to the same target map.
  DCHECK(!IsLoad() || !IsTransition());
  if (IsTransition() && maps->length() > 1) return false;

  for (int i = 1; i < maps->length(); ++i) {
    PropertyAccessInfo test_info(builder_, access_type_, maps->at(i), name_);
    if (!test_info.IsCompatible(this)) return false;
  }

  return true;
}


Handle<Map> HOptimizedGraphBuilder::PropertyAccessInfo::map() {
  Handle<JSFunction> ctor;
  if (Map::GetConstructorFunction(
          map_, handle(current_info()->closure()->context()->native_context()))
          .ToHandle(&ctor)) {
    return handle(ctor->initial_map());
  }
  return map_;
}


static bool NeedsWrapping(Handle<Map> map, Handle<JSFunction> target) {
  return !map->IsJSObjectMap() &&
         is_sloppy(target->shared()->language_mode()) &&
         !target->shared()->native();
}


bool HOptimizedGraphBuilder::PropertyAccessInfo::NeedsWrappingFor(
    Handle<JSFunction> target) const {
  return NeedsWrapping(map_, target);
}


HValue* HOptimizedGraphBuilder::BuildMonomorphicAccess(
    PropertyAccessInfo* info, HValue* object, HValue* checked_object,
    HValue* value, BailoutId ast_id, BailoutId return_id,
    bool can_inline_accessor) {
  HObjectAccess access = HObjectAccess::ForMap();  // bogus default
  if (info->GetJSObjectFieldAccess(&access)) {
    DCHECK(info->IsLoad());
    return New<HLoadNamedField>(object, checked_object, access);
  }

  if (info->name().is_identical_to(isolate()->factory()->prototype_string()) &&
      info->map()->IsJSFunctionMap() && info->map()->is_constructor()) {
    DCHECK(!info->map()->has_non_instance_prototype());
    return New<HLoadFunctionPrototype>(checked_object);
  }

  HValue* checked_holder = checked_object;
  if (info->has_holder()) {
    Handle<JSObject> prototype(JSObject::cast(info->map()->prototype()));
    checked_holder = BuildCheckPrototypeMaps(prototype, info->holder());
  }

  if (!info->IsFound()) {
    DCHECK(info->IsLoad());
    return graph()->GetConstantUndefined();
  }

  if (info->IsData()) {
    if (info->IsLoad()) {
      return BuildLoadNamedField(info, checked_holder);
    } else {
      return BuildStoreNamedField(info, checked_object, value);
    }
  }

  if (info->IsTransition()) {
    DCHECK(!info->IsLoad());
    return BuildStoreNamedField(info, checked_object, value);
  }

  if (info->IsAccessorConstant()) {
    MaybeHandle<Name> maybe_name =
        FunctionTemplateInfo::TryGetCachedPropertyName(isolate(),
                                                       info->accessor());
    if (!maybe_name.is_null()) {
      Handle<Name> name = maybe_name.ToHandleChecked();
      PropertyAccessInfo cache_info(this, LOAD, info->map(), name);
      // Load new target.
      if (cache_info.CanAccessMonomorphic()) {
        return BuildLoadNamedField(&cache_info, checked_object);
      }
    }

    Push(checked_object);
    int argument_count = 1;
    if (!info->IsLoad()) {
      argument_count = 2;
      Push(value);
    }

    if (info->accessor()->IsJSFunction() &&
        info->NeedsWrappingFor(Handle<JSFunction>::cast(info->accessor()))) {
      HValue* function = Add<HConstant>(info->accessor());
      PushArgumentsFromEnvironment(argument_count);
      return NewCallFunction(function, argument_count, TailCallMode::kDisallow,
                             ConvertReceiverMode::kNotNullOrUndefined,
                             TailCallMode::kDisallow);
    } else if (FLAG_inline_accessors && can_inline_accessor) {
      bool success = info->IsLoad()
          ? TryInlineGetter(info->accessor(), info->map(), ast_id, return_id)
          : TryInlineSetter(
              info->accessor(), info->map(), ast_id, return_id, value);
      if (success || HasStackOverflow()) return NULL;
    }

    PushArgumentsFromEnvironment(argument_count);
    if (!info->accessor()->IsJSFunction()) {
      Bailout(kInliningBailedOut);
      return nullptr;
    }
    return NewCallConstantFunction(Handle<JSFunction>::cast(info->accessor()),
                                   argument_count, TailCallMode::kDisallow,
                                   TailCallMode::kDisallow);
  }

  DCHECK(info->IsDataConstant());
  if (info->IsLoad()) {
    return New<HConstant>(info->constant());
  } else {
    return New<HCheckValue>(value, Handle<JSFunction>::cast(info->constant()));
  }
}

void HOptimizedGraphBuilder::HandlePolymorphicNamedFieldAccess(
    PropertyAccessType access_type, Expression* expr, FeedbackSlot slot,
    BailoutId ast_id, BailoutId return_id, HValue* object, HValue* value,
    SmallMapList* maps, Handle<Name> name) {
  // Something did not match; must use a polymorphic load.
  int count = 0;
  HBasicBlock* join = NULL;
  HBasicBlock* number_block = NULL;
  bool handled_string = false;

  bool handle_smi = false;
  STATIC_ASSERT(kMaxLoadPolymorphism == kMaxStorePolymorphism);
  int i;
  for (i = 0; i < maps->length() && count < kMaxLoadPolymorphism; ++i) {
    PropertyAccessInfo info(this, access_type, maps->at(i), name);
    if (info.IsStringType()) {
      if (handled_string) continue;
      handled_string = true;
    }
    if (info.CanAccessMonomorphic()) {
      count++;
      if (info.IsNumberType()) {
        handle_smi = true;
        break;
      }
    }
  }

  if (i < maps->length()) {
    count = -1;
    maps->Clear();
  } else {
    count = 0;
  }
  HControlInstruction* smi_check = NULL;
  handled_string = false;

  for (i = 0; i < maps->length() && count < kMaxLoadPolymorphism; ++i) {
    PropertyAccessInfo info(this, access_type, maps->at(i), name);
    if (info.IsStringType()) {
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
    if (info.IsNumberType()) {
      Handle<Map> heap_number_map = isolate()->factory()->heap_number_map();
      compare = New<HCompareMap>(object, heap_number_map, if_true, if_false);
      dependency = smi_check;
    } else if (info.IsStringType()) {
      compare = New<HIsStringAndBranch>(object, if_true, if_false);
      dependency = compare;
    } else {
      compare = New<HCompareMap>(object, info.map(), if_true, if_false);
      dependency = compare;
    }
    FinishCurrentBlock(compare);

    if (info.IsNumberType()) {
      GotoNoSimulate(if_true, number_block);
      if_true = number_block;
    }

    set_current_block(if_true);

    HValue* access =
        BuildMonomorphicAccess(&info, object, dependency, value, ast_id,
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
      if (access->IsInstruction()) {
        HInstruction* instr = HInstruction::cast(access);
        if (!instr->IsLinked()) AddInstruction(instr);
      }
      if (!ast_context()->IsEffect()) Push(result);
    }

    if (current_block() != NULL) Goto(join);
    set_current_block(if_false);
  }

  // Finish up.  Unconditionally deoptimize if we've handled all the maps we
  // know about and do not want to handle ones we've never seen.  Otherwise
  // use a generic IC.
  if (count == maps->length() && FLAG_deoptimize_uncommon_cases) {
    FinishExitWithHardDeoptimization(
        DeoptimizeReason::kUnknownMapInPolymorphicAccess);
  } else {
    HInstruction* instr =
        BuildNamedGeneric(access_type, expr, slot, object, name, value);
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

  DCHECK(join != NULL);
  if (join->HasPredecessor()) {
    join->SetJoinId(ast_id);
    set_current_block(join);
    if (!ast_context()->IsEffect()) ast_context()->ReturnValue(Pop());
  } else {
    set_current_block(NULL);
  }
}

static bool ComputeReceiverTypes(Expression* expr, HValue* receiver,
                                 SmallMapList** t,
                                 HOptimizedGraphBuilder* builder) {
  Zone* zone = builder->zone();
  SmallMapList* maps = expr->GetReceiverTypes();
  *t = maps;
  bool monomorphic = expr->IsMonomorphic();
  if (maps != nullptr && receiver->HasMonomorphicJSObjectType()) {
    if (maps->length() > 0) {
      Map* root_map = receiver->GetMonomorphicJSObjectMap()->FindRootMap();
      maps->FilterForPossibleTransitions(root_map);
      monomorphic = maps->length() == 1;
    } else {
      // No type feedback, see if we can infer the type. This is safely
      // possible if the receiver had a known map at some point, and no
      // map-changing stores have happened to it since.
      Handle<Map> candidate_map = receiver->GetMonomorphicJSObjectMap();
      for (HInstruction* current = builder->current_block()->last();
           current != nullptr; current = current->previous()) {
        if (current->IsBlockEntry()) break;
        if (current->CheckChangesFlag(kMaps)) {
          // Only allow map changes that store the candidate map. We don't
          // need to care which object the map is being written into.
          if (!current->IsStoreNamedField()) break;
          HStoreNamedField* map_change = HStoreNamedField::cast(current);
          if (!map_change->value()->IsConstant()) break;
          HConstant* map_constant = HConstant::cast(map_change->value());
          if (!map_constant->representation().IsTagged()) break;
          Handle<Object> map = map_constant->handle(builder->isolate());
          if (!map.is_identical_to(candidate_map)) break;
        }
        if (current == receiver) {
          // We made it all the way back to the receiver without encountering
          // a map change! So we can assume that the receiver still has the
          // candidate_map we know about.
          maps->Add(candidate_map, zone);
          monomorphic = true;
          break;
        }
      }
    }
  }
  return monomorphic && CanInlinePropertyAccess(maps->first());
}


static bool AreStringTypes(SmallMapList* maps) {
  for (int i = 0; i < maps->length(); i++) {
    if (maps->at(i)->instance_type() >= FIRST_NONSTRING_TYPE) return false;
  }
  return true;
}

void HOptimizedGraphBuilder::BuildStore(Expression* expr, Property* prop,
                                        FeedbackSlot slot, BailoutId ast_id,
                                        BailoutId return_id,
                                        bool is_uninitialized) {
  if (!prop->key()->IsPropertyName()) {
    // Keyed store.
    HValue* value = Pop();
    HValue* key = Pop();
    HValue* object = Pop();
    bool has_side_effects = false;
    HValue* result =
        HandleKeyedElementAccess(object, key, value, expr, slot, ast_id,
                                 return_id, STORE, &has_side_effects);
    if (has_side_effects) {
      if (!ast_context()->IsEffect()) Push(value);
      Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
      if (!ast_context()->IsEffect()) Drop(1);
    }
    if (result == NULL) return;
    return ast_context()->ReturnValue(value);
  }

  // Named store.
  HValue* value = Pop();
  HValue* object = Pop();

  Literal* key = prop->key()->AsLiteral();
  Handle<String> name = Handle<String>::cast(key->value());
  DCHECK(!name.is_null());

  HValue* access = BuildNamedAccess(STORE, ast_id, return_id, expr, slot,
                                    object, name, value, is_uninitialized);
  if (access == NULL) return;

  if (!ast_context()->IsEffect()) Push(value);
  if (access->IsInstruction()) AddInstruction(HInstruction::cast(access));
  if (access->HasObservableSideEffects()) {
    Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
  }
  if (!ast_context()->IsEffect()) Drop(1);
  return ast_context()->ReturnValue(value);
}


void HOptimizedGraphBuilder::HandlePropertyAssignment(Assignment* expr) {
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  CHECK_ALIVE(VisitForValue(prop->obj()));
  if (!prop->key()->IsPropertyName()) {
    CHECK_ALIVE(VisitForValue(prop->key()));
  }
  CHECK_ALIVE(VisitForValue(expr->value()));
  BuildStore(expr, prop, expr->AssignmentSlot(), expr->id(),
             expr->AssignmentId(), expr->IsUninitialized());
}

HInstruction* HOptimizedGraphBuilder::InlineGlobalPropertyStore(
    LookupIterator* it, HValue* value, BailoutId ast_id) {
  Handle<PropertyCell> cell = it->GetPropertyCell();
  top_info()->dependencies()->AssumePropertyCell(cell);
  auto cell_type = it->property_details().cell_type();
  if (cell_type == PropertyCellType::kConstant ||
      cell_type == PropertyCellType::kUndefined) {
    Handle<Object> constant(cell->value(), isolate());
    if (value->IsConstant()) {
      HConstant* c_value = HConstant::cast(value);
      if (!constant.is_identical_to(c_value->handle(isolate()))) {
        Add<HDeoptimize>(DeoptimizeReason::kConstantGlobalVariableAssignment,
                         Deoptimizer::EAGER);
      }
    } else {
      HValue* c_constant = Add<HConstant>(constant);
      IfBuilder builder(this);
      if (constant->IsNumber()) {
        builder.If<HCompareNumericAndBranch>(value, c_constant, Token::EQ);
      } else {
        builder.If<HCompareObjectEqAndBranch>(value, c_constant);
      }
      builder.Then();
      builder.Else();
      Add<HDeoptimize>(DeoptimizeReason::kConstantGlobalVariableAssignment,
                       Deoptimizer::EAGER);
      builder.End();
    }
  }
  HConstant* cell_constant = Add<HConstant>(cell);
  auto access = HObjectAccess::ForPropertyCellValue();
  if (cell_type == PropertyCellType::kConstantType) {
    switch (cell->GetConstantType()) {
      case PropertyCellConstantType::kSmi:
        access = access.WithRepresentation(Representation::Smi());
        break;
      case PropertyCellConstantType::kStableMap: {
        // First check that the previous value of the {cell} still has the
        // map that we are about to check the new {value} for. If not, then
        // the stable map assumption was invalidated and we cannot continue
        // with the optimized code.
        Handle<HeapObject> cell_value(HeapObject::cast(cell->value()));
        Handle<Map> cell_value_map(cell_value->map());
        if (!cell_value_map->is_stable()) {
          Bailout(kUnstableConstantTypeHeapObject);
          return nullptr;
        }
        top_info()->dependencies()->AssumeMapStable(cell_value_map);
        // Now check that the new {value} is a HeapObject with the same map
        Add<HCheckHeapObject>(value);
        value = Add<HCheckMaps>(value, cell_value_map);
        access = access.WithRepresentation(Representation::HeapObject());
        break;
      }
    }
  }
  HInstruction* instr = New<HStoreNamedField>(cell_constant, access, value);
  instr->ClearChangesFlag(kInobjectFields);
  instr->SetChangesFlag(kGlobalVars);
  return instr;
}

// Because not every expression has a position and there is not common
// superclass of Assignment and CountOperation, we cannot just pass the
// owning expression instead of position and ast_id separately.
void HOptimizedGraphBuilder::HandleGlobalVariableAssignment(Variable* var,
                                                            HValue* value,
                                                            FeedbackSlot slot,
                                                            BailoutId ast_id) {
  Handle<JSGlobalObject> global(current_info()->global_object());

  // Lookup in script contexts.
  {
    Handle<ScriptContextTable> script_contexts(
        global->native_context()->script_context_table());
    ScriptContextTable::LookupResult lookup;
    if (ScriptContextTable::Lookup(script_contexts, var->name(), &lookup)) {
      if (lookup.mode == CONST) {
        return Bailout(kNonInitializerAssignmentToConst);
      }
      Handle<Context> script_context =
          ScriptContextTable::GetContext(script_contexts, lookup.context_index);

      Handle<Object> current_value =
          FixedArray::get(*script_context, lookup.slot_index, isolate());

      // If the values is not the hole, it will stay initialized,
      // so no need to generate a check.
      if (current_value->IsTheHole(isolate())) {
        return Bailout(kReferenceToUninitializedVariable);
      }

      HStoreNamedField* instr = Add<HStoreNamedField>(
          Add<HConstant>(script_context),
          HObjectAccess::ForContextSlot(lookup.slot_index), value);
      USE(instr);
      DCHECK(instr->HasObservableSideEffects());
      Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
      return;
    }
  }

  LookupIterator it(global, var->name(), LookupIterator::OWN);
  if (CanInlineGlobalPropertyAccess(var, &it, STORE)) {
    HInstruction* instr = InlineGlobalPropertyStore(&it, value, ast_id);
    if (!instr) return;
    AddInstruction(instr);
    if (instr->HasObservableSideEffects()) {
      Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
    }
  } else {
    HValue* global_object = Add<HLoadNamedField>(
        BuildGetNativeContext(), nullptr,
        HObjectAccess::ForContextSlot(Context::EXTENSION_INDEX));
    Handle<FeedbackVector> vector =
        handle(current_feedback_vector(), isolate());
    HValue* name = Add<HConstant>(var->name());
    HValue* vector_value = Add<HConstant>(vector);
    HValue* slot_value = Add<HConstant>(vector->GetIndex(slot));
    DCHECK(vector->IsStoreGlobalIC(slot));
    DCHECK_EQ(vector->GetLanguageMode(slot), function_language_mode());
    Callable callable = CodeFactory::StoreGlobalICInOptimizedCode(
        isolate(), function_language_mode());
    HValue* stub = Add<HConstant>(callable.code());
    HValue* values[] = {global_object, name, value, slot_value, vector_value};
    HCallWithDescriptor* instr =
        Add<HCallWithDescriptor>(Code::STORE_GLOBAL_IC, stub, 0,
                                 callable.descriptor(), ArrayVector(values));
    USE(instr);
    DCHECK(instr->HasObservableSideEffects());
    Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
  }
}


void HOptimizedGraphBuilder::HandleCompoundAssignment(Assignment* expr) {
  Expression* target = expr->target();
  VariableProxy* proxy = target->AsVariableProxy();
  Property* prop = target->AsProperty();
  DCHECK(proxy == NULL || prop == NULL);

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
      case VariableLocation::UNALLOCATED:
        HandleGlobalVariableAssignment(var, Top(), expr->AssignmentSlot(),
                                       expr->AssignmentId());
        break;

      case VariableLocation::PARAMETER:
      case VariableLocation::LOCAL:
        if (var->mode() == CONST) {
          return Bailout(kNonInitializerAssignmentToConst);
        }
        BindIfLive(var, Top());
        break;

      case VariableLocation::CONTEXT: {
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
            if (var->throw_on_const_assignment(function_language_mode())) {
              return Bailout(kNonInitializerAssignmentToConst);
            } else {
              return ast_context()->ReturnValue(Pop());
            }
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

      case VariableLocation::LOOKUP:
        return Bailout(kCompoundAssignmentToLookupSlot);

      case VariableLocation::MODULE:
        UNREACHABLE();
    }
    return ast_context()->ReturnValue(Pop());

  } else if (prop != NULL) {
    CHECK_ALIVE(VisitForValue(prop->obj()));
    HValue* object = Top();
    HValue* key = NULL;
    if (!prop->key()->IsPropertyName() || prop->IsStringAccess()) {
      CHECK_ALIVE(VisitForValue(prop->key()));
      key = Top();
    }

    CHECK_ALIVE(PushLoad(prop, object, key));

    CHECK_ALIVE(VisitForValue(expr->value()));
    HValue* right = Pop();
    HValue* left = Pop();

    Push(BuildBinaryOperation(operation, left, right, PUSH_BEFORE_SIMULATE));

    BuildStore(expr, prop, expr->AssignmentSlot(), expr->id(),
               expr->AssignmentId(), expr->IsUninitialized());
  } else {
    return Bailout(kInvalidLhsInCompoundAssignment);
  }
}


void HOptimizedGraphBuilder::VisitAssignment(Assignment* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());

  VariableProxy* proxy = expr->target()->AsVariableProxy();
  Property* prop = expr->target()->AsProperty();
  DCHECK(proxy == NULL || prop == NULL);

  if (expr->is_compound()) {
    HandleCompoundAssignment(expr);
    return;
  }

  if (prop != NULL) {
    HandlePropertyAssignment(expr);
  } else if (proxy != NULL) {
    Variable* var = proxy->var();

    if (var->mode() == CONST) {
      if (expr->op() != Token::INIT) {
        if (var->throw_on_const_assignment(function_language_mode())) {
          return Bailout(kNonInitializerAssignmentToConst);
        } else {
          CHECK_ALIVE(VisitForValue(expr->value()));
          return ast_context()->ReturnValue(Pop());
        }
      }
    }

    // Handle the assignment.
    switch (var->location()) {
      case VariableLocation::UNALLOCATED:
        CHECK_ALIVE(VisitForValue(expr->value()));
        HandleGlobalVariableAssignment(var, Top(), expr->AssignmentSlot(),
                                       expr->AssignmentId());
        return ast_context()->ReturnValue(Pop());

      case VariableLocation::PARAMETER:
      case VariableLocation::LOCAL: {
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

      case VariableLocation::CONTEXT: {
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
              // If we reached this point, the only possibility
              // is a sloppy assignment to a function name.
              DCHECK(function_language_mode() == SLOPPY &&
                     !var->throw_on_const_assignment(SLOPPY));
              return ast_context()->ReturnValue(Pop());
            default:
              mode = HStoreContextSlot::kNoCheck;
          }
        } else {
          DCHECK_EQ(Token::INIT, expr->op());
          mode = HStoreContextSlot::kNoCheck;
        }

        HValue* context = BuildContextChainWalk(var);
        HStoreContextSlot* instr = Add<HStoreContextSlot>(
            context, var->index(), mode, Top());
        if (instr->HasObservableSideEffects()) {
          Add<HSimulate>(expr->AssignmentId(), REMOVABLE_SIMULATE);
        }
        return ast_context()->ReturnValue(Pop());
      }

      case VariableLocation::LOOKUP:
        return Bailout(kAssignmentToLOOKUPVariable);

      case VariableLocation::MODULE:
        UNREACHABLE();
    }
  } else {
    return Bailout(kInvalidLeftHandSideInAssignment);
  }
}

void HOptimizedGraphBuilder::VisitSuspend(Suspend* expr) {
  // Generators are not optimized, so we should never get here.
  UNREACHABLE();
}


void HOptimizedGraphBuilder::VisitThrow(Throw* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  if (!ast_context()->IsEffect()) {
    // The parser turns invalid left-hand sides in assignments into throw
    // statements, which may not be in effect contexts. We might still try
    // to optimize such functions; bail out now if we do.
    return Bailout(kInvalidLeftHandSideInAssignment);
  }
  CHECK_ALIVE(VisitForValue(expr->exception()));

  HValue* value = environment()->Pop();
  if (!is_tracking_positions()) SetSourcePosition(expr->position());
  Add<HPushArguments>(value);
  Add<HCallRuntime>(Runtime::FunctionForId(Runtime::kThrow), 1);
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
      Add<HLoadNamedField>(string, nullptr, HObjectAccess::ForMap()), nullptr,
      HObjectAccess::ForMapInstanceType());
}


HInstruction* HGraphBuilder::AddLoadStringLength(HValue* string) {
  return AddInstruction(BuildLoadStringLength(string));
}


HInstruction* HGraphBuilder::BuildLoadStringLength(HValue* string) {
  if (string->IsConstant()) {
    HConstant* c_string = HConstant::cast(string);
    if (c_string->HasStringValue()) {
      return New<HConstant>(c_string->StringValue()->length());
    }
  }
  return New<HLoadNamedField>(string, nullptr,
                              HObjectAccess::ForStringLength());
}

HInstruction* HOptimizedGraphBuilder::BuildNamedGeneric(
    PropertyAccessType access_type, Expression* expr, FeedbackSlot slot,
    HValue* object, Handle<Name> name, HValue* value, bool is_uninitialized) {
  if (is_uninitialized) {
    Add<HDeoptimize>(
        DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess,
        Deoptimizer::SOFT);
  }
  Handle<FeedbackVector> vector(current_feedback_vector(), isolate());

  HValue* key = Add<HConstant>(name);
  HValue* vector_value = Add<HConstant>(vector);
  HValue* slot_value = Add<HConstant>(vector->GetIndex(slot));

  if (access_type == LOAD) {
    HValue* values[] = {object, key, slot_value, vector_value};
    if (!expr->AsProperty()->key()->IsPropertyName()) {
      DCHECK(vector->IsKeyedLoadIC(slot));
      // It's possible that a keyed load of a constant string was converted
      // to a named load. Here, at the last minute, we need to make sure to
      // use a generic Keyed Load if we are using the type vector, because
      // it has to share information with full code.
      Callable callable = CodeFactory::KeyedLoadICInOptimizedCode(isolate());
      HValue* stub = Add<HConstant>(callable.code());
      HCallWithDescriptor* result =
          New<HCallWithDescriptor>(Code::KEYED_LOAD_IC, stub, 0,
                                   callable.descriptor(), ArrayVector(values));
      return result;
    }
    DCHECK(vector->IsLoadIC(slot));
    Callable callable = CodeFactory::LoadICInOptimizedCode(isolate());
    HValue* stub = Add<HConstant>(callable.code());
    HCallWithDescriptor* result = New<HCallWithDescriptor>(
        Code::LOAD_IC, stub, 0, callable.descriptor(), ArrayVector(values));
    return result;

  } else {
    HValue* values[] = {object, key, value, slot_value, vector_value};
    if (vector->IsKeyedStoreIC(slot)) {
      // It's possible that a keyed store of a constant string was converted
      // to a named store. Here, at the last minute, we need to make sure to
      // use a generic Keyed Store if we are using the type vector, because
      // it has to share information with full code.
      DCHECK_EQ(vector->GetLanguageMode(slot), function_language_mode());
      Callable callable = CodeFactory::KeyedStoreICInOptimizedCode(
          isolate(), function_language_mode());
      HValue* stub = Add<HConstant>(callable.code());
      HCallWithDescriptor* result =
          New<HCallWithDescriptor>(Code::KEYED_STORE_IC, stub, 0,
                                   callable.descriptor(), ArrayVector(values));
      return result;
    }
    HCallWithDescriptor* result;
    if (vector->IsStoreOwnIC(slot)) {
      Callable callable = CodeFactory::StoreOwnICInOptimizedCode(isolate());
      HValue* stub = Add<HConstant>(callable.code());
      result = New<HCallWithDescriptor>(
          Code::STORE_IC, stub, 0, callable.descriptor(), ArrayVector(values));
    } else {
      DCHECK(vector->IsStoreIC(slot));
      DCHECK_EQ(vector->GetLanguageMode(slot), function_language_mode());
      Callable callable = CodeFactory::StoreICInOptimizedCode(
          isolate(), function_language_mode());
      HValue* stub = Add<HConstant>(callable.code());
      result = New<HCallWithDescriptor>(
          Code::STORE_IC, stub, 0, callable.descriptor(), ArrayVector(values));
    }
    return result;
  }
}

HInstruction* HOptimizedGraphBuilder::BuildKeyedGeneric(
    PropertyAccessType access_type, Expression* expr, FeedbackSlot slot,
    HValue* object, HValue* key, HValue* value) {
  Handle<FeedbackVector> vector(current_feedback_vector(), isolate());
  HValue* vector_value = Add<HConstant>(vector);
  HValue* slot_value = Add<HConstant>(vector->GetIndex(slot));

  if (access_type == LOAD) {
    HValue* values[] = {object, key, slot_value, vector_value};

    Callable callable = CodeFactory::KeyedLoadICInOptimizedCode(isolate());
    HValue* stub = Add<HConstant>(callable.code());
    HCallWithDescriptor* result =
        New<HCallWithDescriptor>(Code::KEYED_LOAD_IC, stub, 0,
                                 callable.descriptor(), ArrayVector(values));
    return result;
  } else {
    HValue* values[] = {object, key, value, slot_value, vector_value};

    Callable callable = CodeFactory::KeyedStoreICInOptimizedCode(
        isolate(), function_language_mode());
    HValue* stub = Add<HConstant>(callable.code());
    HCallWithDescriptor* result =
        New<HCallWithDescriptor>(Code::KEYED_STORE_IC, stub, 0,
                                 callable.descriptor(), ArrayVector(values));
    return result;
  }
}


LoadKeyedHoleMode HOptimizedGraphBuilder::BuildKeyedHoleMode(Handle<Map> map) {
  // Loads from a "stock" fast holey double arrays can elide the hole check.
  // Loads from a "stock" fast holey array can convert the hole to undefined
  // with impunity.
  LoadKeyedHoleMode load_mode = NEVER_RETURN_HOLE;
  bool holey_double_elements =
      *map == isolate()->get_initial_js_array_map(FAST_HOLEY_DOUBLE_ELEMENTS);
  bool holey_elements =
      *map == isolate()->get_initial_js_array_map(FAST_HOLEY_ELEMENTS);
  if ((holey_double_elements || holey_elements) &&
      isolate()->IsFastArrayConstructorPrototypeChainIntact()) {
    load_mode =
        holey_double_elements ? ALLOW_RETURN_HOLE : CONVERT_HOLE_TO_UNDEFINED;

    Handle<JSObject> prototype(JSObject::cast(map->prototype()), isolate());
    Handle<JSObject> object_prototype = isolate()->initial_object_prototype();
    BuildCheckPrototypeMaps(prototype, object_prototype);
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
  HCheckMaps* checked_object = Add<HCheckMaps>(object, map, dependency);

  if (access_type == STORE && map->prototype()->IsJSObject()) {
    // monomorphic stores need a prototype chain check because shape
    // changes could allow callbacks on elements in the chain that
    // aren't compatible with monomorphic keyed stores.
    PrototypeIterator iter(map);
    JSObject* holder = NULL;
    while (!iter.IsAtEnd()) {
      // JSProxies can't occur here because we wouldn't have installed a
      // non-generic IC if there were any.
      holder = *PrototypeIterator::GetCurrent<JSObject>(iter);
      iter.Advance();
    }
    DCHECK(holder && holder->IsJSObject());

    BuildCheckPrototypeMaps(handle(JSObject::cast(map->prototype())),
                            Handle<JSObject>(holder));
  }

  LoadKeyedHoleMode load_mode = BuildKeyedHoleMode(map);
  return BuildUncheckedMonomorphicElementAccess(
      checked_object, key, val,
      map->instance_type() == JS_ARRAY_TYPE,
      map->elements_kind(), access_type,
      load_mode, store_mode);
}


static bool CanInlineElementAccess(Handle<Map> map) {
  return map->IsJSObjectMap() &&
         (map->has_fast_elements() || map->has_fixed_typed_array_elements()) &&
         !map->has_indexed_interceptor() && !map->is_access_check_needed();
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
    if (!CanInlineElementAccess(map)) return NULL;
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
  LoadKeyedHoleMode load_mode = NEVER_RETURN_HOLE;
  if (has_seen_holey_elements) {
    // Make sure that all of the maps we are handling have the initial array
    // prototype.
    bool saw_non_array_prototype = false;
    for (int i = 0; i < maps->length(); ++i) {
      Handle<Map> map = maps->at(i);
      if (map->prototype() != *isolate()->initial_array_prototype()) {
        // We can't guarantee that loading the hole is safe. The prototype may
        // have an element at this position.
        saw_non_array_prototype = true;
        break;
      }
    }

    if (!saw_non_array_prototype) {
      Handle<Map> holey_map = handle(
          isolate()->get_initial_js_array_map(consolidated_elements_kind));
      load_mode = BuildKeyedHoleMode(holey_map);
      if (load_mode != NEVER_RETURN_HOLE) {
        for (int i = 0; i < maps->length(); ++i) {
          Handle<Map> map = maps->at(i);
          // The prototype check was already done for the holey map in
          // BuildKeyedHoleMode.
          if (!map.is_identical_to(holey_map)) {
            Handle<JSObject> prototype(JSObject::cast(map->prototype()),
                                       isolate());
            Handle<JSObject> object_prototype =
                isolate()->initial_object_prototype();
            BuildCheckPrototypeMaps(prototype, object_prototype);
          }
        }
      }
    }
  }
  HInstruction* instr = BuildUncheckedMonomorphicElementAccess(
      checked_object, key, val,
      most_general_consolidated_map->instance_type() == JS_ARRAY_TYPE,
      consolidated_elements_kind, LOAD, load_mode, STANDARD_STORE);
  return instr;
}

HValue* HOptimizedGraphBuilder::HandlePolymorphicElementAccess(
    Expression* expr, FeedbackSlot slot, HValue* object, HValue* key,
    HValue* val, SmallMapList* maps, PropertyAccessType access_type,
    KeyedAccessStoreMode store_mode, bool* has_side_effects) {
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
  MapHandles transition_target;
  transition_target.reserve(maps->length());
  // Collect possible transition targets.
  MapHandles possible_transitioned_maps;
  possible_transitioned_maps.reserve(maps->length());
  for (int i = 0; i < maps->length(); ++i) {
    Handle<Map> map = maps->at(i);
    // Loads from strings or loads with a mix of string and non-string maps
    // shouldn't be handled polymorphically.
    DCHECK(access_type != LOAD || !map->IsStringMap());
    ElementsKind elements_kind = map->elements_kind();
    if (CanInlineElementAccess(map) && IsFastElementsKind(elements_kind) &&
        elements_kind != GetInitialFastElementsKind()) {
      possible_transitioned_maps.push_back(map);
    }
    if (IsSloppyArgumentsElementsKind(elements_kind)) {
      HInstruction* result =
          BuildKeyedGeneric(access_type, expr, slot, object, key, val);
      *has_side_effects = result->HasObservableSideEffects();
      return AddInstruction(result);
    }
  }
  // Get transition target for each map (NULL == no transition).
  for (int i = 0; i < maps->length(); ++i) {
    Handle<Map> map = maps->at(i);
    // Don't generate elements kind transitions from stable maps.
    Map* transitioned_map =
        map->is_stable()
            ? nullptr
            : map->FindElementsKindTransitionedMap(possible_transitioned_maps);
    if (transitioned_map != nullptr) {
      transition_target.push_back(handle(transitioned_map));
    } else {
      transition_target.push_back(Handle<Map>());
    }
  }

  MapHandles untransitionable_maps;
  untransitionable_maps.reserve(maps->length());
  HTransitionElementsKind* transition = NULL;
  for (int i = 0; i < maps->length(); ++i) {
    Handle<Map> map = maps->at(i);
    DCHECK(map->IsMap());
    if (!transition_target.at(i).is_null()) {
      DCHECK(Map::IsValidElementsTransition(
          map->elements_kind(),
          transition_target.at(i)->elements_kind()));
      transition = Add<HTransitionElementsKind>(object, map,
                                                transition_target.at(i));
    } else {
      untransitionable_maps.push_back(map);
    }
  }

  // If only one map is left after transitioning, handle this case
  // monomorphically.
  DCHECK(untransitionable_maps.size() >= 1);
  if (untransitionable_maps.size() == 1) {
    Handle<Map> untransitionable_map = untransitionable_maps[0];
    HInstruction* instr = NULL;
    if (!CanInlineElementAccess(untransitionable_map)) {
      instr = AddInstruction(
          BuildKeyedGeneric(access_type, expr, slot, object, key, val));
    } else {
      instr = BuildMonomorphicElementAccess(
          object, key, val, transition, untransitionable_map, access_type,
          store_mode);
    }
    *has_side_effects |= instr->HasObservableSideEffects();
    return access_type == STORE ? val : instr;
  }

  HBasicBlock* join = graph()->CreateBasicBlock();

  for (Handle<Map> map : untransitionable_maps) {
    ElementsKind elements_kind = map->elements_kind();
    HBasicBlock* this_map = graph()->CreateBasicBlock();
    HBasicBlock* other_map = graph()->CreateBasicBlock();
    HCompareMap* mapcompare =
        New<HCompareMap>(object, map, this_map, other_map);
    FinishCurrentBlock(mapcompare);

    set_current_block(this_map);
    HInstruction* access = NULL;
    if (!CanInlineElementAccess(map)) {
      access = AddInstruction(
          BuildKeyedGeneric(access_type, expr, slot, object, key, val));
    } else {
      DCHECK(IsFastElementsKind(elements_kind) ||
             IsFixedTypedArrayElementsKind(elements_kind));
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
  DCHECK(join->predecessors()->length() > 0);
  // Deopt if none of the cases matched.
  NoObservableSideEffectsScope scope(this);
  FinishExitWithHardDeoptimization(
      DeoptimizeReason::kUnknownMapInPolymorphicElementAccess);
  set_current_block(join);
  return access_type == STORE ? val : Pop();
}

HValue* HOptimizedGraphBuilder::HandleKeyedElementAccess(
    HValue* obj, HValue* key, HValue* val, Expression* expr, FeedbackSlot slot,
    BailoutId ast_id, BailoutId return_id, PropertyAccessType access_type,
    bool* has_side_effects) {
  // A keyed name access with type feedback may contain the name.
  Handle<FeedbackVector> vector = handle(current_feedback_vector(), isolate());
  HValue* expected_key = key;
  if (!key->ActualValue()->IsConstant()) {
    Name* name = nullptr;
    if (access_type == LOAD) {
      KeyedLoadICNexus nexus(vector, slot);
      name = nexus.FindFirstName();
    } else {
      KeyedStoreICNexus nexus(vector, slot);
      name = nexus.FindFirstName();
    }
    if (name != nullptr) {
      Handle<Name> handle_name(name);
      expected_key = Add<HConstant>(handle_name);
      // We need a check against the key.
      bool in_new_space = isolate()->heap()->InNewSpace(*handle_name);
      Unique<Name> unique_name = Unique<Name>::CreateUninitialized(handle_name);
      Add<HCheckValue>(key, unique_name, in_new_space);
    }
  }
  if (expected_key->ActualValue()->IsConstant()) {
    Handle<Object> constant =
        HConstant::cast(expected_key->ActualValue())->handle(isolate());
    uint32_t array_index;
    if ((constant->IsString() &&
         !Handle<String>::cast(constant)->AsArrayIndex(&array_index)) ||
        constant->IsSymbol()) {
      if (!constant->IsUniqueName()) {
        constant = isolate()->factory()->InternalizeString(
            Handle<String>::cast(constant));
      }
      HValue* access =
          BuildNamedAccess(access_type, ast_id, return_id, expr, slot, obj,
                           Handle<Name>::cast(constant), val, false);
      if (access == NULL || access->IsPhi() ||
          HInstruction::cast(access)->IsLinked()) {
        *has_side_effects = false;
      } else {
        HInstruction* instr = HInstruction::cast(access);
        AddInstruction(instr);
        *has_side_effects = instr->HasObservableSideEffects();
      }
      return access;
    }
  }

  DCHECK(!expr->IsPropertyName());
  HInstruction* instr = NULL;

  SmallMapList* maps;
  bool monomorphic = ComputeReceiverTypes(expr, obj, &maps, this);

  bool force_generic = false;
  if (expr->GetKeyType() == PROPERTY) {
    // Non-Generic accesses assume that elements are being accessed, and will
    // deopt for non-index keys, which the IC knows will occur.
    // TODO(jkummerow): Consider adding proper support for property accesses.
    force_generic = true;
    monomorphic = false;
  } else if (access_type == STORE &&
             (monomorphic || (maps != NULL && !maps->is_empty()))) {
    // Stores can't be mono/polymorphic if their prototype chain has dictionary
    // elements. However a receiver map that has dictionary elements itself
    // should be left to normal mono/poly behavior (the other maps may benefit
    // from highly optimized stores).
    for (int i = 0; i < maps->length(); i++) {
      Handle<Map> current_map = maps->at(i);
      if (current_map->DictionaryElementsInPrototypeChainOnly()) {
        force_generic = true;
        monomorphic = false;
        break;
      }
    }
  } else if (access_type == LOAD && !monomorphic &&
             (maps != NULL && !maps->is_empty())) {
    // Polymorphic loads have to go generic if any of the maps are strings.
    // If some, but not all of the maps are strings, we should go generic
    // because polymorphic access wants to key on ElementsKind and isn't
    // compatible with strings.
    for (int i = 0; i < maps->length(); i++) {
      Handle<Map> current_map = maps->at(i);
      if (current_map->IsStringMap()) {
        force_generic = true;
        break;
      }
    }
  }

  if (monomorphic) {
    Handle<Map> map = maps->first();
    if (!CanInlineElementAccess(map)) {
      instr = AddInstruction(
          BuildKeyedGeneric(access_type, expr, slot, obj, key, val));
    } else {
      BuildCheckHeapObject(obj);
      instr = BuildMonomorphicElementAccess(
          obj, key, val, NULL, map, access_type, expr->GetStoreMode());
    }
  } else if (!force_generic && (maps != NULL && !maps->is_empty())) {
    return HandlePolymorphicElementAccess(expr, slot, obj, key, val, maps,
                                          access_type, expr->GetStoreMode(),
                                          has_side_effects);
  } else {
    if (access_type == STORE) {
      if (expr->IsAssignment() &&
          expr->AsAssignment()->HasNoTypeInformation()) {
        Add<HDeoptimize>(
            DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess,
            Deoptimizer::SOFT);
      }
    } else {
      if (expr->AsProperty()->HasNoTypeInformation()) {
        Add<HDeoptimize>(
            DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess,
            Deoptimizer::SOFT);
      }
    }
    instr = AddInstruction(
        BuildKeyedGeneric(access_type, expr, slot, obj, key, val));
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
    HInstruction* push_argument = New<HPushArguments>(argument);
    push_argument->InsertAfter(insert_after);
    insert_after = push_argument;
  }

  HArgumentsElements* arguments_elements = New<HArgumentsElements>(true);
  arguments_elements->ClearFlag(HValue::kUseGVN);
  arguments_elements->InsertAfter(insert_after);
  function_state()->set_arguments_elements(arguments_elements);
}

bool HOptimizedGraphBuilder::IsAnyParameterContextAllocated() {
  int count = current_info()->scope()->num_parameters();
  for (int i = 0; i < count; ++i) {
    if (current_info()->scope()->parameter(i)->location() ==
        VariableLocation::CONTEXT) {
      return true;
    }
  }
  return false;
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
    if (!String::Equals(name, isolate()->factory()->length_string())) {
      return false;
    }

    // Make sure we visit the arguments object so that the liveness analysis
    // still records the access.
    CHECK_ALIVE_OR_RETURN(VisitForValue(expr->obj(), ARGUMENTS_ALLOWED), true);
    Drop(1);

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
    // We need to take into account the KEYED_LOAD_IC feedback to guard the
    // HBoundsCheck instructions below.
    if (!expr->IsMonomorphic() && !expr->IsUninitialized()) return false;
    if (IsAnyParameterContextAllocated()) return false;
    CHECK_ALIVE_OR_RETURN(VisitForValue(expr->obj(), ARGUMENTS_ALLOWED), true);
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

HValue* HOptimizedGraphBuilder::BuildNamedAccess(
    PropertyAccessType access, BailoutId ast_id, BailoutId return_id,
    Expression* expr, FeedbackSlot slot, HValue* object, Handle<Name> name,
    HValue* value, bool is_uninitialized) {
  SmallMapList* maps;
  ComputeReceiverTypes(expr, object, &maps, this);
  DCHECK(maps != NULL);

  // Check for special case: Access via a single map to the global proxy
  // can also be handled monomorphically.
  if (maps->length() > 0) {
    Handle<Object> map_constructor =
        handle(maps->first()->GetConstructor(), isolate());
    if (map_constructor->IsJSFunction()) {
      Handle<Context> map_context =
          handle(Handle<JSFunction>::cast(map_constructor)->context());
      Handle<Context> current_context(current_info()->context());
      bool is_same_context_global_proxy_access =
          maps->length() == 1 &&  // >1 map => fallback to polymorphic
          maps->first()->IsJSGlobalProxyMap() &&
          (*map_context == *current_context);
      if (is_same_context_global_proxy_access) {
        Handle<JSGlobalObject> global_object(current_info()->global_object());
        LookupIterator it(global_object, name, LookupIterator::OWN);
        if (CanInlineGlobalPropertyAccess(&it, access)) {
          BuildCheckHeapObject(object);
          Add<HCheckMaps>(object, maps);
          if (access == LOAD) {
            InlineGlobalPropertyLoad(&it, expr->id());
            return nullptr;
          } else {
            return InlineGlobalPropertyStore(&it, value, expr->id());
          }
        }
      }
    }

    PropertyAccessInfo info(this, access, maps->first(), name);
    if (!info.CanAccessAsMonomorphic(maps)) {
      HandlePolymorphicNamedFieldAccess(access, expr, slot, ast_id, return_id,
                                        object, value, maps, name);
      return NULL;
    }

    HValue* checked_object;
    // AstType::Number() is only supported by polymorphic load/call handling.
    DCHECK(!info.IsNumberType());
    BuildCheckHeapObject(object);
    if (AreStringTypes(maps)) {
      checked_object =
          Add<HCheckInstanceType>(object, HCheckInstanceType::IS_STRING);
    } else {
      checked_object = Add<HCheckMaps>(object, maps);
    }
    return BuildMonomorphicAccess(
        &info, object, checked_object, value, ast_id, return_id);
  }

  return BuildNamedGeneric(access, expr, slot, object, name, value,
                           is_uninitialized);
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
  if (expr->IsStringAccess() && expr->GetKeyType() == ELEMENT) {
    HValue* index = Pop();
    HValue* string = Pop();
    HInstruction* char_code = BuildStringCharCodeAt(string, index);
    AddInstruction(char_code);
    if (char_code->IsConstant()) {
      HConstant* c_code = HConstant::cast(char_code);
      if (c_code->HasNumberValue() && std::isnan(c_code->DoubleValue())) {
        Add<HDeoptimize>(DeoptimizeReason::kOutOfBounds, Deoptimizer::EAGER);
      }
    }
    instr = NewUncasted<HStringCharFromCode>(char_code);

  } else if (expr->key()->IsPropertyName()) {
    Handle<String> name = expr->key()->AsLiteral()->AsPropertyName();
    HValue* object = Pop();

    HValue* value = BuildNamedAccess(LOAD, ast_id, expr->LoadId(), expr,
                                     expr->PropertyFeedbackSlot(), object, name,
                                     NULL, expr->IsUninitialized());
    if (value == NULL) return;
    if (value->IsPhi()) return ast_context()->ReturnValue(value);
    instr = HInstruction::cast(value);
    if (instr->IsLinked()) return ast_context()->ReturnValue(instr);

  } else {
    HValue* key = Pop();
    HValue* obj = Pop();

    bool has_side_effects = false;
    HValue* load = HandleKeyedElementAccess(
        obj, key, NULL, expr, expr->PropertyFeedbackSlot(), ast_id,
        expr->LoadId(), LOAD, &has_side_effects);
    if (has_side_effects) {
      if (ast_context()->IsEffect()) {
        Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
      } else {
        Push(load);
        Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
        Drop(1);
      }
    }
    if (load == NULL) return;
    return ast_context()->ReturnValue(load);
  }
  return ast_context()->ReturnInstruction(instr, ast_id);
}


void HOptimizedGraphBuilder::VisitProperty(Property* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());

  if (TryArgumentsAccess(expr)) return;

  CHECK_ALIVE(VisitForValue(expr->obj()));
  if (!expr->key()->IsPropertyName() || expr->IsStringAccess()) {
    CHECK_ALIVE(VisitForValue(expr->key()));
  }

  BuildLoad(expr, expr->id());
}

HInstruction* HGraphBuilder::BuildConstantMapCheck(Handle<JSObject> constant,
                                                   bool ensure_no_elements) {
  HCheckMaps* check = Add<HCheckMaps>(
      Add<HConstant>(constant), handle(constant->map()));
  check->ClearDependsOnFlag(kElementsKind);
  if (ensure_no_elements) {
    // TODO(ishell): remove this once we support NO_ELEMENTS elements kind.
    HValue* elements = AddLoadElements(check, nullptr);
    HValue* empty_elements =
        Add<HConstant>(isolate()->factory()->empty_fixed_array());
    IfBuilder if_empty(this);
    if_empty.IfNot<HCompareObjectEqAndBranch>(elements, empty_elements);
    if_empty.ThenDeopt(DeoptimizeReason::kWrongMap);
    if_empty.End();
  }
  return check;
}

HInstruction* HGraphBuilder::BuildCheckPrototypeMaps(Handle<JSObject> prototype,
                                                     Handle<JSObject> holder,
                                                     bool ensure_no_elements) {
  PrototypeIterator iter(isolate(), prototype, kStartAtReceiver);
  while (holder.is_null() ||
         !PrototypeIterator::GetCurrent(iter).is_identical_to(holder)) {
    BuildConstantMapCheck(PrototypeIterator::GetCurrent<JSObject>(iter),
                          ensure_no_elements);
    iter.Advance();
    if (iter.IsAtEnd()) {
      return NULL;
    }
  }
  return BuildConstantMapCheck(holder);
}


void HOptimizedGraphBuilder::AddCheckPrototypeMaps(Handle<JSObject> holder,
                                                   Handle<Map> receiver_map) {
  if (!holder.is_null()) {
    Handle<JSObject> prototype(JSObject::cast(receiver_map->prototype()));
    BuildCheckPrototypeMaps(prototype, holder);
  }
}

void HOptimizedGraphBuilder::BuildEnsureCallable(HValue* object) {
  NoObservableSideEffectsScope scope(this);
  const Runtime::Function* throw_called_non_callable =
      Runtime::FunctionForId(Runtime::kThrowCalledNonCallable);

  IfBuilder is_not_function(this);
  HValue* smi_check = is_not_function.If<HIsSmiAndBranch>(object);
  is_not_function.Or();
  HValue* map = AddLoadMap(object, smi_check);
  HValue* bit_field =
      Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapBitField());
  HValue* bit_field_masked = AddUncasted<HBitwise>(
      Token::BIT_AND, bit_field, Add<HConstant>(1 << Map::kIsCallable));
  is_not_function.IfNot<HCompareNumericAndBranch>(
      bit_field_masked, Add<HConstant>(1 << Map::kIsCallable), Token::EQ);
  is_not_function.Then();
  {
    Add<HPushArguments>(object);
    Add<HCallRuntime>(throw_called_non_callable, 1);
  }
  is_not_function.End();
}

HInstruction* HOptimizedGraphBuilder::NewCallFunction(
    HValue* function, int argument_count, TailCallMode syntactic_tail_call_mode,
    ConvertReceiverMode convert_mode, TailCallMode tail_call_mode) {
  if (syntactic_tail_call_mode == TailCallMode::kAllow) {
    BuildEnsureCallable(function);
  } else {
    DCHECK_EQ(TailCallMode::kDisallow, tail_call_mode);
  }
  HValue* arity = Add<HConstant>(argument_count - 1);

  HValue* op_vals[] = {function, arity};

  Callable callable =
      CodeFactory::Call(isolate(), convert_mode, tail_call_mode);
  HConstant* stub = Add<HConstant>(callable.code());

  return New<HCallWithDescriptor>(stub, argument_count, callable.descriptor(),
                                  ArrayVector(op_vals),
                                  syntactic_tail_call_mode);
}

HInstruction* HOptimizedGraphBuilder::NewCallFunctionViaIC(
    HValue* function, int argument_count, TailCallMode syntactic_tail_call_mode,
    ConvertReceiverMode convert_mode, TailCallMode tail_call_mode,
    FeedbackSlot slot) {
  if (syntactic_tail_call_mode == TailCallMode::kAllow) {
    BuildEnsureCallable(function);
  } else {
    DCHECK_EQ(TailCallMode::kDisallow, tail_call_mode);
  }
  int arity = argument_count - 1;
  Handle<FeedbackVector> vector(current_feedback_vector(), isolate());
  HValue* arity_val = Add<HConstant>(arity);
  HValue* index_val = Add<HConstant>(vector->GetIndex(slot));
  HValue* vector_val = Add<HConstant>(vector);

  HValue* op_vals[] = {function, arity_val, index_val, vector_val};
  Callable callable =
      CodeFactory::CallIC(isolate(), convert_mode, tail_call_mode);
  HConstant* stub = Add<HConstant>(callable.code());

  return New<HCallWithDescriptor>(stub, argument_count, callable.descriptor(),
                                  ArrayVector(op_vals),
                                  syntactic_tail_call_mode);
}

HInstruction* HOptimizedGraphBuilder::NewCallConstantFunction(
    Handle<JSFunction> function, int argument_count,
    TailCallMode syntactic_tail_call_mode, TailCallMode tail_call_mode) {
  HValue* target = Add<HConstant>(function);
  return New<HInvokeFunction>(target, function, argument_count,
                              syntactic_tail_call_mode, tail_call_mode);
}


class FunctionSorter {
 public:
  explicit FunctionSorter(int index = 0, int ticks = 0, int size = 0)
      : index_(index), ticks_(ticks), size_(size) {}

  int index() const { return index_; }
  int ticks() const { return ticks_; }
  int size() const { return size_; }

 private:
  int index_;
  int ticks_;
  int size_;
};


inline bool operator<(const FunctionSorter& lhs, const FunctionSorter& rhs) {
  int diff = lhs.ticks() - rhs.ticks();
  if (diff != 0) return diff > 0;
  return lhs.size() < rhs.size();
}


void HOptimizedGraphBuilder::HandlePolymorphicCallNamed(Call* expr,
                                                        HValue* receiver,
                                                        SmallMapList* maps,
                                                        Handle<String> name) {
  int argument_count = expr->arguments()->length() + 1;  // Includes receiver.
  FunctionSorter order[kMaxCallPolymorphism];

  bool handle_smi = false;
  bool handled_string = false;
  int ordered_functions = 0;

  TailCallMode syntactic_tail_call_mode = expr->tail_call_mode();
  TailCallMode tail_call_mode =
      function_state()->ComputeTailCallMode(syntactic_tail_call_mode);

  int i;
  for (i = 0; i < maps->length() && ordered_functions < kMaxCallPolymorphism;
       ++i) {
    PropertyAccessInfo info(this, LOAD, maps->at(i), name);
    if (info.CanAccessMonomorphic() && info.IsDataConstant() &&
        info.constant()->IsJSFunction()) {
      if (info.IsStringType()) {
        if (handled_string) continue;
        handled_string = true;
      }
      Handle<JSFunction> target = Handle<JSFunction>::cast(info.constant());
      if (info.IsNumberType()) {
        handle_smi = true;
      }
      expr->set_target(target);
      order[ordered_functions++] = FunctionSorter(
          i, target->shared()->profiler_ticks(), InliningAstSize(target));
    }
  }

  std::sort(order, order + ordered_functions);

  if (i < maps->length()) {
    maps->Clear();
    ordered_functions = -1;
  }

  HBasicBlock* number_block = NULL;
  HBasicBlock* join = NULL;
  handled_string = false;
  int count = 0;

  for (int fn = 0; fn < ordered_functions; ++fn) {
    int i = order[fn].index();
    PropertyAccessInfo info(this, LOAD, maps->at(i), name);
    if (info.IsStringType()) {
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
    if (info.IsNumberType()) {
      Handle<Map> heap_number_map = isolate()->factory()->heap_number_map();
      compare = New<HCompareMap>(receiver, heap_number_map, if_true, if_false);
    } else if (info.IsStringType()) {
      compare = New<HIsStringAndBranch>(receiver, if_true, if_false);
    } else {
      compare = New<HCompareMap>(receiver, map, if_true, if_false);
    }
    FinishCurrentBlock(compare);

    if (info.IsNumberType()) {
      GotoNoSimulate(if_true, number_block);
      if_true = number_block;
    }

    set_current_block(if_true);

    AddCheckPrototypeMaps(info.holder(), map);

    HValue* function = Add<HConstant>(expr->target());
    environment()->SetExpressionStackAt(0, function);
    Push(receiver);
    CHECK_ALIVE(VisitExpressions(expr->arguments()));
    bool needs_wrapping = info.NeedsWrappingFor(target);
    bool try_inline = FLAG_polymorphic_inlining && !needs_wrapping;
    if (FLAG_trace_inlining && try_inline) {
      Handle<JSFunction> caller = current_info()->closure();
      std::unique_ptr<char[]> caller_name =
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
      // use the regular call builtin for method calls to wrap the receiver.
      // TODO(verwaest): Support creation of value wrappers directly in
      // HWrapReceiver.
      HInstruction* call =
          needs_wrapping
              ? NewCallFunction(
                    function, argument_count, syntactic_tail_call_mode,
                    ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode)
              : NewCallConstantFunction(target, argument_count,
                                        syntactic_tail_call_mode,
                                        tail_call_mode);
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
  if (ordered_functions == maps->length() && FLAG_deoptimize_uncommon_cases) {
    FinishExitWithHardDeoptimization(
        DeoptimizeReason::kUnknownMapInPolymorphicCall);
  } else {
    Property* prop = expr->expression()->AsProperty();
    HInstruction* function =
        BuildNamedGeneric(LOAD, prop, prop->PropertyFeedbackSlot(), receiver,
                          name, NULL, prop->IsUninitialized());
    AddInstruction(function);
    Push(function);
    AddSimulate(prop->LoadId(), REMOVABLE_SIMULATE);

    environment()->SetExpressionStackAt(1, function);
    environment()->SetExpressionStackAt(0, receiver);
    CHECK_ALIVE(VisitExpressions(expr->arguments()));

    HInstruction* call = NewCallFunction(
        function, argument_count, syntactic_tail_call_mode,
        ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode);

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
  DCHECK(join != NULL);
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
                                         const char* reason,
                                         TailCallMode tail_call_mode) {
  if (FLAG_trace_inlining) {
    std::unique_ptr<char[]> target_name =
        target->shared()->DebugName()->ToCString();
    std::unique_ptr<char[]> caller_name =
        caller->shared()->DebugName()->ToCString();
    if (reason == NULL) {
      const char* call_mode =
          tail_call_mode == TailCallMode::kAllow ? "tail called" : "called";
      PrintF("Inlined %s %s from %s.\n", target_name.get(), call_mode,
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

  // Always inline functions that force inlining.
  if (target_shared->force_inline()) {
    return 0;
  }
  if (!target->shared()->IsUserJavaScript()) {
    return kNotInlinable;
  }

  if (target_shared->IsApiFunction()) {
    TraceInline(target, caller, "target is api function");
    return kNotInlinable;
  }

  // Do a quick check on source code length to avoid parsing large
  // inlining candidates.
  if (target_shared->SourceSize() >
      Min(FLAG_max_inlined_source_size, kUnlimitedMaxInlinedSourceSize)) {
    TraceInline(target, caller, "target text too big");
    return kNotInlinable;
  }

  // Target must be inlineable.
  BailoutReason noopt_reason = target_shared->disable_optimization_reason();
  if (!target_shared->IsInlineable() && noopt_reason != kHydrogenFilter) {
    TraceInline(target, caller, "target not inlineable");
    return kNotInlinable;
  }
  if (noopt_reason != kNoReason && noopt_reason != kHydrogenFilter) {
    TraceInline(target, caller, "target contains unsupported syntax [early]");
    return kNotInlinable;
  }

  int nodes_added = target_shared->ast_node_count();
  return nodes_added;
}

bool HOptimizedGraphBuilder::TryInline(Handle<JSFunction> target,
                                       int arguments_count,
                                       HValue* implicit_return_value,
                                       BailoutId ast_id, BailoutId return_id,
                                       InliningKind inlining_kind,
                                       TailCallMode syntactic_tail_call_mode) {
  if (target->context()->native_context() !=
      top_info()->closure()->context()->native_context()) {
    return false;
  }
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
  // Always inline small methods (<= 10 nodes).
  if (inlined_count_ > Min(FLAG_max_inlined_nodes_cumulative,
                           kUnlimitedMaxInlinedNodesCumulative)) {
    TraceInline(target, caller, "cumulative AST node limit reached");
    return false;
  }

  // Parse and allocate variables.
  // Use the same AstValueFactory for creating strings in the sub-compilation
  // step, but don't transfer ownership to target_info.
  Handle<SharedFunctionInfo> target_shared(target->shared());
  ParseInfo parse_info(target_shared, top_info()->parse_info()->zone_shared());
  parse_info.set_ast_value_factory(
      top_info()->parse_info()->ast_value_factory());
  parse_info.set_ast_value_factory_owned(false);

  CompilationInfo target_info(parse_info.zone(), &parse_info,
                              target->GetIsolate(), target);

  if (inlining_kind != CONSTRUCT_CALL_RETURN &&
      IsClassConstructor(target_shared->kind())) {
    TraceInline(target, caller, "target is classConstructor");
    return false;
  }

  if (target_shared->HasDebugInfo()) {
    TraceInline(target, caller, "target is being debugged");
    return false;
  }
  if (!Compiler::ParseAndAnalyze(&target_info)) {
    if (target_info.isolate()->has_pending_exception()) {
      // Parse or scope error, never optimize this function.
      SetStackOverflow();
      target_shared->DisableOptimization(kParseScopeError);
    }
    TraceInline(target, caller, "parse failure");
    return false;
  }
  if (target_shared->must_use_ignition_turbo()) {
    TraceInline(target, caller, "ParseAndAnalyze found incompatibility");
    return false;
  }

  if (target_info.scope()->NeedsContext()) {
    TraceInline(target, caller, "target has context-allocated variables");
    return false;
  }

  if (target_info.scope()->rest_parameter() != nullptr) {
    TraceInline(target, caller, "target uses rest parameters");
    return false;
  }

  FunctionLiteral* function = target_info.literal();

  // The following conditions must be checked again after re-parsing, because
  // earlier the information might not have been complete due to lazy parsing.
  nodes_added = function->ast_node_count();
  if (nodes_added > Min(FLAG_max_inlined_nodes, kUnlimitedMaxInlinedNodes)) {
    TraceInline(target, caller, "target AST is too large [late]");
    return false;
  }
  if (function->dont_optimize()) {
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
  }

  // Unsupported variable references present.
  if (function->scope()->this_function_var() != nullptr ||
      function->scope()->new_target_var() != nullptr) {
    TraceInline(target, caller, "target uses new target or this function");
    return false;
  }

  // All declarations must be inlineable.
  Declaration::List* decls = target_info.scope()->declarations();
  for (Declaration* decl : *decls) {
    if (decl->IsFunctionDeclaration() ||
        !decl->proxy()->var()->IsStackAllocated()) {
      TraceInline(target, caller, "target has non-trivial declaration");
      return false;
    }
  }

  // Generate the deoptimization data for the unoptimized version of
  // the target function if we don't already have it.
  if (!Compiler::EnsureDeoptimizationSupport(&target_info)) {
    TraceInline(target, caller, "could not generate deoptimization info");
    return false;
  }

  // Remember that we inlined this function. This needs to be called right
  // after the EnsureDeoptimizationSupport call so that the code flusher
  // does not remove the code with the deoptimization support.
  int inlining_id = top_info()->AddInlinedFunction(target_info.shared_info(),
                                                   source_position());

  // ----------------------------------------------------------------
  // After this point, we've made a decision to inline this function (so
  // TryInline should always return true).

  // If target was lazily compiled, it's literals array may not yet be set up.
  JSFunction::EnsureLiterals(target);

  // Type-check the inlined function.
  DCHECK(target_shared->has_deoptimization_support());
  AstTyper(target_info.isolate(), target_info.zone(), target_info.closure(),
           target_info.scope(), target_info.osr_ast_id(), target_info.literal(),
           &bounds_)
      .Run();

  // Save the pending call context. Set up new one for the inlined function.
  // The function state is new-allocated because we need to delete it
  // in two different places.
  FunctionState* target_state = new FunctionState(
      this, &target_info, inlining_kind, inlining_id,
      function_state()->ComputeTailCallMode(syntactic_tail_call_mode));

  HConstant* undefined = graph()->GetConstantUndefined();

  HEnvironment* inner_env = environment()->CopyForInlining(
      target, arguments_count, function, undefined,
      function_state()->inlining_kind(), syntactic_tail_call_mode);

  HConstant* context = Add<HConstant>(Handle<Context>(target->context()));
  inner_env->BindContext(context);

  // Create a dematerialized arguments object for the function, also copy the
  // current arguments values to use them for materialization.
  HEnvironment* arguments_env = inner_env->arguments_environment();
  int parameter_count = arguments_env->parameter_count();
  HArgumentsObject* arguments_object = Add<HArgumentsObject>(parameter_count);
  for (int i = 0; i < parameter_count; i++) {
    arguments_object->AddArgument(arguments_env->Lookup(i), zone());
  }

  // If the function uses arguments object then bind bind one.
  if (function->scope()->arguments() != NULL) {
    DCHECK(function->scope()->arguments()->IsStackAllocated());
    inner_env->Bind(function->scope()->arguments(), arguments_object);
  }

  // Capture the state before invoking the inlined function for deopt in the
  // inlined function. This simulate has no bailout-id since it's not directly
  // reachable for deopt, and is only used to capture the state. If the simulate
  // becomes reachable by merging, the ast id of the simulate merged into it is
  // adopted.
  Add<HSimulate>(BailoutId::None());

  current_block()->UpdateEnvironment(inner_env);
  Scope* saved_scope = scope();
  set_scope(target_info.scope());
  HEnterInlined* enter_inlined = Add<HEnterInlined>(
      return_id, target, context, arguments_count, function,
      function_state()->inlining_kind(), function->scope()->arguments(),
      arguments_object, syntactic_tail_call_mode);
  if (is_tracking_positions()) {
    enter_inlined->set_inlining_id(inlining_id);
  }

  function_state()->set_entry(enter_inlined);

  VisitDeclarations(target_info.scope()->declarations());
  VisitStatements(function->body());
  set_scope(saved_scope);
  if (HasStackOverflow()) {
    // Bail out if the inline function did, as we cannot residualize a call
    // instead, but do not disable optimization for the outer function.
    TraceInline(target, caller, "inline graph construction failed");
    target_shared->DisableOptimization(kInliningBailedOut);
    current_info()->RetryOptimization(kInliningBailedOut);
    delete target_state;
    return true;
  }

  // Update inlined nodes count.
  inlined_count_ += nodes_added;

  Handle<Code> unoptimized_code(target_shared->code());
  DCHECK(unoptimized_code->kind() == Code::FUNCTION);
  Handle<TypeFeedbackInfo> type_info(
      TypeFeedbackInfo::cast(unoptimized_code->type_feedback_info()));
  graph()->update_type_change_checksum(type_info->own_type_change_checksum());

  TraceInline(target, caller, NULL, syntactic_tail_call_mode);

  if (current_block() != NULL) {
    FunctionState* state = function_state();
    if (state->inlining_kind() == CONSTRUCT_CALL_RETURN) {
      // Falling off the end of an inlined construct call. In a test context the
      // return value will always evaluate to true, in a value context the
      // return value is the newly allocated receiver.
      if (call_context()->IsTest()) {
        inlined_test_context()->ReturnValue(graph()->GetConstantTrue());
      } else if (call_context()->IsEffect()) {
        Goto(function_return(), state);
      } else {
        DCHECK(call_context()->IsValue());
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
        DCHECK(call_context()->IsValue());
        AddLeaveInlined(implicit_return_value, state);
      }
    } else {
      // Falling off the end of a normal inlined function. This basically means
      // returning undefined.
      if (call_context()->IsTest()) {
        inlined_test_context()->ReturnValue(graph()->GetConstantFalse());
      } else if (call_context()->IsEffect()) {
        Goto(function_return(), state);
      } else {
        DCHECK(call_context()->IsValue());
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
    DCHECK(ast_context() == inlined_test_context());
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
  return TryInline(expr->target(), expr->arguments()->length(), NULL,
                   expr->id(), expr->ReturnId(), NORMAL_RETURN,
                   expr->tail_call_mode());
}


bool HOptimizedGraphBuilder::TryInlineConstruct(CallNew* expr,
                                                HValue* implicit_return_value) {
  return TryInline(expr->target(), expr->arguments()->length(),
                   implicit_return_value, expr->id(), expr->ReturnId(),
                   CONSTRUCT_CALL_RETURN, TailCallMode::kDisallow);
}

bool HOptimizedGraphBuilder::TryInlineGetter(Handle<Object> getter,
                                             Handle<Map> receiver_map,
                                             BailoutId ast_id,
                                             BailoutId return_id) {
  if (TryInlineApiGetter(getter, receiver_map, ast_id)) return true;
  if (getter->IsJSFunction()) {
    Handle<JSFunction> getter_function = Handle<JSFunction>::cast(getter);
    return TryInlineBuiltinGetterCall(getter_function, receiver_map, ast_id) ||
           TryInline(getter_function, 0, NULL, ast_id, return_id,
                     GETTER_CALL_RETURN, TailCallMode::kDisallow);
  }
  return false;
}

bool HOptimizedGraphBuilder::TryInlineSetter(Handle<Object> setter,
                                             Handle<Map> receiver_map,
                                             BailoutId id,
                                             BailoutId assignment_id,
                                             HValue* implicit_return_value) {
  if (TryInlineApiSetter(setter, receiver_map, id)) return true;
  return setter->IsJSFunction() &&
         TryInline(Handle<JSFunction>::cast(setter), 1, implicit_return_value,
                   id, assignment_id, SETTER_CALL_RETURN,
                   TailCallMode::kDisallow);
}


bool HOptimizedGraphBuilder::TryInlineIndirectCall(Handle<JSFunction> function,
                                                   Call* expr,
                                                   int arguments_count) {
  return TryInline(function, arguments_count, NULL, expr->id(),
                   expr->ReturnId(), NORMAL_RETURN, expr->tail_call_mode());
}


bool HOptimizedGraphBuilder::TryInlineBuiltinFunctionCall(Call* expr) {
  if (!expr->target()->shared()->HasBuiltinFunctionId()) return false;
  BuiltinFunctionId id = expr->target()->shared()->builtin_function_id();
  // We intentionally ignore expr->tail_call_mode() here because builtins
  // we inline here do not observe if they were tail called or not.
  switch (id) {
    case kMathCos:
    case kMathExp:
    case kMathRound:
    case kMathFround:
    case kMathFloor:
    case kMathAbs:
    case kMathSin:
    case kMathSqrt:
    case kMathLog:
    case kMathClz32:
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
        HInstruction* op =
            HMul::NewImul(isolate(), zone(), context(), left, right);
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


// static
bool HOptimizedGraphBuilder::IsReadOnlyLengthDescriptor(
    Handle<Map> jsarray_map) {
  DCHECK(!jsarray_map->is_dictionary_map());
  Isolate* isolate = jsarray_map->GetIsolate();
  Handle<Name> length_string = isolate->factory()->length_string();
  DescriptorArray* descriptors = jsarray_map->instance_descriptors();
  int number =
      descriptors->SearchWithCache(isolate, *length_string, *jsarray_map);
  DCHECK_NE(DescriptorArray::kNotFound, number);
  return descriptors->GetDetails(number).IsReadOnly();
}


// static
bool HOptimizedGraphBuilder::CanInlineArrayResizeOperation(
    Handle<Map> receiver_map) {
  return !receiver_map.is_null() && receiver_map->prototype()->IsJSObject() &&
         receiver_map->instance_type() == JS_ARRAY_TYPE &&
         IsFastElementsKind(receiver_map->elements_kind()) &&
         !receiver_map->is_dictionary_map() && receiver_map->is_extensible() &&
         (!receiver_map->is_prototype_map() || receiver_map->is_stable()) &&
         !IsReadOnlyLengthDescriptor(receiver_map);
}

bool HOptimizedGraphBuilder::TryInlineBuiltinGetterCall(
    Handle<JSFunction> function, Handle<Map> receiver_map, BailoutId ast_id) {
  if (!function->shared()->HasBuiltinFunctionId()) return false;
  BuiltinFunctionId id = function->shared()->builtin_function_id();

  // Try to inline getter calls like DataView.prototype.byteLength/byteOffset
  // as operations in the calling function.
  switch (id) {
    case kDataViewBuffer: {
      if (!receiver_map->IsJSDataViewMap()) return false;
      HObjectAccess access = HObjectAccess::ForMapAndOffset(
          receiver_map, JSDataView::kBufferOffset);
      HValue* object = Pop();  // receiver
      HInstruction* result = New<HLoadNamedField>(object, object, access);
      ast_context()->ReturnInstruction(result, ast_id);
      return true;
    }
    case kDataViewByteLength:
    case kDataViewByteOffset: {
      if (!receiver_map->IsJSDataViewMap()) return false;
      int offset = (id == kDataViewByteLength) ? JSDataView::kByteLengthOffset
                                               : JSDataView::kByteOffsetOffset;
      HObjectAccess access =
          HObjectAccess::ForMapAndOffset(receiver_map, offset);
      HValue* object = Pop();  // receiver
      HValue* checked_object = Add<HCheckArrayBufferNotNeutered>(object);
      HInstruction* result =
          New<HLoadNamedField>(object, checked_object, access);
      ast_context()->ReturnInstruction(result, ast_id);
      return true;
    }
    case kTypedArrayByteLength:
    case kTypedArrayByteOffset:
    case kTypedArrayLength: {
      if (!receiver_map->IsJSTypedArrayMap()) return false;
      int offset = (id == kTypedArrayLength)
                       ? JSTypedArray::kLengthOffset
                       : (id == kTypedArrayByteLength)
                             ? JSTypedArray::kByteLengthOffset
                             : JSTypedArray::kByteOffsetOffset;
      HObjectAccess access =
          HObjectAccess::ForMapAndOffset(receiver_map, offset);
      HValue* object = Pop();  // receiver
      HValue* checked_object = Add<HCheckArrayBufferNotNeutered>(object);
      HInstruction* result =
          New<HLoadNamedField>(object, checked_object, access);
      ast_context()->ReturnInstruction(result, ast_id);
      return true;
    }
    default:
      return false;
  }
}

// static
bool HOptimizedGraphBuilder::NoElementsInPrototypeChain(
    Handle<Map> receiver_map) {
  // TODO(ishell): remove this once we support NO_ELEMENTS elements kind.
  PrototypeIterator iter(receiver_map);
  Handle<Object> empty_fixed_array =
      iter.isolate()->factory()->empty_fixed_array();
  while (true) {
    Handle<JSObject> current = PrototypeIterator::GetCurrent<JSObject>(iter);
    if (current->elements() != *empty_fixed_array) return false;
    iter.Advance();
    if (iter.IsAtEnd()) {
      return true;
    }
  }
}

bool HOptimizedGraphBuilder::TryInlineBuiltinMethodCall(
    Handle<JSFunction> function, Handle<Map> receiver_map, BailoutId ast_id,
    int args_count_no_receiver) {
  if (!function->shared()->HasBuiltinFunctionId()) return false;
  BuiltinFunctionId id = function->shared()->builtin_function_id();
  int argument_count = args_count_no_receiver + 1;  // Plus receiver.

  if (receiver_map.is_null()) {
    HValue* receiver = environment()->ExpressionStackAt(args_count_no_receiver);
    if (receiver->IsConstant() &&
        HConstant::cast(receiver)->handle(isolate())->IsHeapObject()) {
      receiver_map =
          handle(Handle<HeapObject>::cast(
                     HConstant::cast(receiver)->handle(isolate()))->map());
    }
  }
  // Try to inline calls like Math.* as operations in the calling function.
  switch (id) {
    case kObjectHasOwnProperty: {
      // It's not safe to look through the phi for elements if we're compiling
      // for osr.
      if (top_info()->is_osr()) return false;
      if (argument_count != 2) return false;
      HValue* key = Top();
      if (!key->IsLoadKeyed()) return false;
      HValue* elements = HLoadKeyed::cast(key)->elements();
      if (!elements->IsPhi() || elements->OperandCount() != 1) return false;
      if (!elements->OperandAt(0)->IsForInCacheArray()) return false;
      HForInCacheArray* cache = HForInCacheArray::cast(elements->OperandAt(0));
      HValue* receiver = environment()->ExpressionStackAt(1);
      if (!receiver->IsPhi() || receiver->OperandCount() != 1) return false;
      if (cache->enumerable() != receiver->OperandAt(0)) return false;
      Drop(3);  // key, receiver, function
      Add<HCheckMapValue>(receiver, cache->map());
      ast_context()->ReturnValue(graph()->GetConstantTrue());
      return true;
    }
    case kStringCharCodeAt:
    case kStringCharAt:
      if (argument_count == 2) {
        HValue* index = Pop();
        HValue* string = Pop();
        Drop(1);  // Function.
        HInstruction* char_code =
            BuildStringCharCodeAt(string, index);
        if (id == kStringCharCodeAt) {
          ast_context()->ReturnInstruction(char_code, ast_id);
          return true;
        }
        AddInstruction(char_code);
        HInstruction* result = NewUncasted<HStringCharFromCode>(char_code);
        ast_context()->ReturnInstruction(result, ast_id);
        return true;
      }
      break;
    case kStringFromCharCode:
      if (argument_count == 2) {
        HValue* argument = Pop();
        Drop(2);  // Receiver and function.
        argument = AddUncasted<HForceRepresentation>(
            argument, Representation::Integer32());
        argument->SetFlag(HValue::kTruncatingToInt32);
        HInstruction* result = NewUncasted<HStringCharFromCode>(argument);
        ast_context()->ReturnInstruction(result, ast_id);
        return true;
      }
      break;
    case kMathCos:
    case kMathExp:
    case kMathRound:
    case kMathFround:
    case kMathFloor:
    case kMathAbs:
    case kMathSin:
    case kMathSqrt:
    case kMathLog:
    case kMathClz32:
      if (argument_count == 2) {
        HValue* argument = Pop();
        Drop(2);  // Receiver and function.
        HInstruction* op = NewUncasted<HUnaryMathOperation>(argument, id);
        ast_context()->ReturnInstruction(op, ast_id);
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
            DCHECK(!sqrt->HasObservableSideEffects());
            result = NewUncasted<HDiv>(one, sqrt);
          } else if (exponent == 2.0) {
            result = NewUncasted<HMul>(left, left);
          }
        }

        if (result == NULL) {
          result = NewUncasted<HPower>(left, right);
        }
        ast_context()->ReturnInstruction(result, ast_id);
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
        ast_context()->ReturnInstruction(result, ast_id);
        return true;
      }
      break;
    case kMathImul:
      if (argument_count == 3) {
        HValue* right = Pop();
        HValue* left = Pop();
        Drop(2);  // Receiver and function.
        HInstruction* result =
            HMul::NewImul(isolate(), zone(), context(), left, right);
        ast_context()->ReturnInstruction(result, ast_id);
        return true;
      }
      break;
    case kArrayPop: {
      if (!CanInlineArrayResizeOperation(receiver_map)) return false;
      ElementsKind elements_kind = receiver_map->elements_kind();

      Drop(args_count_no_receiver);
      HValue* result;
      HValue* reduced_length;
      HValue* receiver = Pop();

      HValue* checked_object = AddCheckMap(receiver, receiver_map);
      HValue* length =
          Add<HLoadNamedField>(checked_object, nullptr,
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
        result = AddElementAccess(elements, reduced_length, nullptr,
                                  bounds_check, nullptr, elements_kind, LOAD);
        HValue* hole = IsFastSmiOrObjectElementsKind(elements_kind)
                           ? graph()->GetConstantHole()
                           : Add<HConstant>(HConstant::kHoleNaN);
        if (IsFastSmiOrObjectElementsKind(elements_kind)) {
          elements_kind = FAST_HOLEY_ELEMENTS;
        }
        AddElementAccess(elements, reduced_length, hole, bounds_check, nullptr,
                         elements_kind, STORE);
        Add<HStoreNamedField>(
            checked_object, HObjectAccess::ForArrayLength(elements_kind),
            reduced_length, STORE_TO_INITIALIZED_ENTRY);

        if (!ast_context()->IsEffect()) Push(result);

        length_checker.End();
      }
      result = ast_context()->IsEffect() ? graph()->GetConstant0() : Top();
      Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
      if (!ast_context()->IsEffect()) Drop(1);

      ast_context()->ReturnValue(result);
      return true;
    }
    case kArrayPush: {
      if (!CanInlineArrayResizeOperation(receiver_map)) return false;
      ElementsKind elements_kind = receiver_map->elements_kind();

      // If there may be elements accessors in the prototype chain, the fast
      // inlined version can't be used.
      if (receiver_map->DictionaryElementsInPrototypeChainOnly()) return false;
      // If there currently can be no elements accessors on the prototype chain,
      // it doesn't mean that there won't be any later. Install a full prototype
      // chain check to trap element accessors being installed on the prototype
      // chain, which would cause elements to go to dictionary mode and result
      // in a map change.
      Handle<JSObject> prototype(JSObject::cast(receiver_map->prototype()));
      BuildCheckPrototypeMaps(prototype, Handle<JSObject>());

      // Protect against adding elements to the Array prototype, which needs to
      // route through appropriate bottlenecks.
      if (isolate()->IsFastArrayConstructorPrototypeChainIntact() &&
          !prototype->IsJSArray()) {
        return false;
      }

      const int argc = args_count_no_receiver;
      if (argc != 1) return false;

      HValue* value_to_push = Pop();
      HValue* array = Pop();
      Drop(1);  // Drop function.

      HInstruction* new_size = NULL;
      HValue* length = NULL;

      {
        NoObservableSideEffectsScope scope(this);

        length = Add<HLoadNamedField>(
            array, nullptr, HObjectAccess::ForArrayLength(elements_kind));

        new_size = AddUncasted<HAdd>(length, graph()->GetConstant1());

        bool is_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
        HValue* checked_array = Add<HCheckMaps>(array, receiver_map);
        BuildUncheckedMonomorphicElementAccess(
            checked_array, length, value_to_push, is_array, elements_kind,
            STORE, NEVER_RETURN_HOLE, STORE_AND_GROW_NO_TRANSITION);

        if (!ast_context()->IsEffect()) Push(new_size);
        Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
        if (!ast_context()->IsEffect()) Drop(1);
      }

      ast_context()->ReturnValue(new_size);
      return true;
    }
    case kArrayShift: {
      if (!CanInlineArrayResizeOperation(receiver_map)) return false;
      if (!NoElementsInPrototypeChain(receiver_map)) return false;
      ElementsKind kind = receiver_map->elements_kind();

      // If there may be elements accessors in the prototype chain, the fast
      // inlined version can't be used.
      if (receiver_map->DictionaryElementsInPrototypeChainOnly()) return false;

      // If there currently can be no elements accessors on the prototype chain,
      // it doesn't mean that there won't be any later. Install a full prototype
      // chain check to trap element accessors being installed on the prototype
      // chain, which would cause elements to go to dictionary mode and result
      // in a map change.
      BuildCheckPrototypeMaps(
          handle(JSObject::cast(receiver_map->prototype()), isolate()),
          Handle<JSObject>::null(), true);

      // Threshold for fast inlined Array.shift().
      HConstant* inline_threshold = Add<HConstant>(JSArray::kMaxCopyElements);

      Drop(args_count_no_receiver);
      HValue* result;
      HValue* receiver = Pop();
      HValue* checked_object = AddCheckMap(receiver, receiver_map);
      HValue* length = Add<HLoadNamedField>(
          receiver, checked_object, HObjectAccess::ForArrayLength(kind));

      Drop(1);  // Function.
      {
        NoObservableSideEffectsScope scope(this);

        IfBuilder if_lengthiszero(this);
        HValue* lengthiszero = if_lengthiszero.If<HCompareNumericAndBranch>(
            length, graph()->GetConstant0(), Token::EQ);
        if_lengthiszero.Then();
        {
          if (!ast_context()->IsEffect()) Push(graph()->GetConstantUndefined());
        }
        if_lengthiszero.Else();
        {
          HValue* elements = AddLoadElements(receiver);

          // Check if we can use the fast inlined Array.shift().
          IfBuilder if_inline(this);
          if_inline.If<HCompareNumericAndBranch>(
              length, inline_threshold, Token::LTE);
          if (IsFastSmiOrObjectElementsKind(kind)) {
            // We cannot handle copy-on-write backing stores here.
            if_inline.AndIf<HCompareMap>(
                elements, isolate()->factory()->fixed_array_map());
          }
          if_inline.Then();
          {
            // Remember the result.
            if (!ast_context()->IsEffect()) {
              Push(AddElementAccess(elements, graph()->GetConstant0(), nullptr,
                                    lengthiszero, nullptr, kind, LOAD));
            }

            // Compute the new length.
            HValue* new_length = AddUncasted<HSub>(
                length, graph()->GetConstant1());
            new_length->ClearFlag(HValue::kCanOverflow);

            // Copy the remaining elements.
            LoopBuilder loop(this, context(), LoopBuilder::kPostIncrement);
            {
              HValue* new_key = loop.BeginBody(
                  graph()->GetConstant0(), new_length, Token::LT);
              HValue* key = AddUncasted<HAdd>(new_key, graph()->GetConstant1());
              key->ClearFlag(HValue::kCanOverflow);
              ElementsKind copy_kind =
                  kind == FAST_HOLEY_SMI_ELEMENTS ? FAST_HOLEY_ELEMENTS : kind;
              HValue* element =
                  AddUncasted<HLoadKeyed>(elements, key, lengthiszero, nullptr,
                                          copy_kind, ALLOW_RETURN_HOLE);
              HStoreKeyed* store = Add<HStoreKeyed>(elements, new_key, element,
                                                    nullptr, copy_kind);
              store->SetFlag(HValue::kTruncatingToNumber);
            }
            loop.EndBody();

            // Put a hole at the end.
            HValue* hole = IsFastSmiOrObjectElementsKind(kind)
                               ? graph()->GetConstantHole()
                               : Add<HConstant>(HConstant::kHoleNaN);
            if (IsFastSmiOrObjectElementsKind(kind)) kind = FAST_HOLEY_ELEMENTS;
            Add<HStoreKeyed>(elements, new_length, hole, nullptr, kind,
                             INITIALIZING_STORE);

            // Remember new length.
            Add<HStoreNamedField>(
                receiver, HObjectAccess::ForArrayLength(kind),
                new_length, STORE_TO_INITIALIZED_ENTRY);
          }
          if_inline.Else();
          {
            Add<HPushArguments>(receiver);
            result = AddInstruction(NewCallConstantFunction(
                function, 1, TailCallMode::kDisallow, TailCallMode::kDisallow));
            if (!ast_context()->IsEffect()) Push(result);
          }
          if_inline.End();
        }
        if_lengthiszero.End();
      }
      result = ast_context()->IsEffect() ? graph()->GetConstant0() : Top();
      Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
      if (!ast_context()->IsEffect()) Drop(1);
      ast_context()->ReturnValue(result);
      return true;
    }
    case kArrayIndexOf:
    case kArrayLastIndexOf: {
      if (receiver_map.is_null()) return false;
      if (receiver_map->instance_type() != JS_ARRAY_TYPE) return false;
      if (!receiver_map->prototype()->IsJSObject()) return false;
      ElementsKind kind = receiver_map->elements_kind();
      if (!IsFastElementsKind(kind)) return false;
      if (argument_count != 2) return false;
      if (!receiver_map->is_extensible()) return false;

      // If there may be elements accessors in the prototype chain, the fast
      // inlined version can't be used.
      if (receiver_map->DictionaryElementsInPrototypeChainOnly()) return false;

      // If there currently can be no elements accessors on the prototype chain,
      // it doesn't mean that there won't be any later. Install a full prototype
      // chain check to trap element accessors being installed on the prototype
      // chain, which would cause elements to go to dictionary mode and result
      // in a map change.
      BuildCheckPrototypeMaps(
          handle(JSObject::cast(receiver_map->prototype()), isolate()),
          Handle<JSObject>::null());

      HValue* search_element = Pop();
      HValue* receiver = Pop();
      Drop(1);  // Drop function.

      ArrayIndexOfMode mode = (id == kArrayIndexOf)
          ? kFirstIndexOf : kLastIndexOf;
      HValue* index = BuildArrayIndexOf(receiver, search_element, kind, mode);

      if (!ast_context()->IsEffect()) Push(index);
      Add<HSimulate>(ast_id, REMOVABLE_SIMULATE);
      if (!ast_context()->IsEffect()) Drop(1);
      ast_context()->ReturnValue(index);
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
  if (V8_UNLIKELY(FLAG_runtime_stats)) return false;
  Handle<JSFunction> function = expr->target();
  int argc = expr->arguments()->length();
  SmallMapList receiver_maps;
  return TryInlineApiCall(function, receiver, &receiver_maps, argc, expr->id(),
                          kCallApiFunction, expr->tail_call_mode());
}


bool HOptimizedGraphBuilder::TryInlineApiMethodCall(
    Call* expr,
    HValue* receiver,
    SmallMapList* receiver_maps) {
  if (V8_UNLIKELY(FLAG_runtime_stats)) return false;
  Handle<JSFunction> function = expr->target();
  int argc = expr->arguments()->length();
  return TryInlineApiCall(function, receiver, receiver_maps, argc, expr->id(),
                          kCallApiMethod, expr->tail_call_mode());
}

bool HOptimizedGraphBuilder::TryInlineApiGetter(Handle<Object> function,
                                                Handle<Map> receiver_map,
                                                BailoutId ast_id) {
  if (V8_UNLIKELY(FLAG_runtime_stats)) return false;
  SmallMapList receiver_maps(1, zone());
  receiver_maps.Add(receiver_map, zone());
  return TryInlineApiCall(function,
                          NULL,  // Receiver is on expression stack.
                          &receiver_maps, 0, ast_id, kCallApiGetter,
                          TailCallMode::kDisallow);
}

bool HOptimizedGraphBuilder::TryInlineApiSetter(Handle<Object> function,
                                                Handle<Map> receiver_map,
                                                BailoutId ast_id) {
  SmallMapList receiver_maps(1, zone());
  receiver_maps.Add(receiver_map, zone());
  return TryInlineApiCall(function,
                          NULL,  // Receiver is on expression stack.
                          &receiver_maps, 1, ast_id, kCallApiSetter,
                          TailCallMode::kDisallow);
}

bool HOptimizedGraphBuilder::TryInlineApiCall(
    Handle<Object> function, HValue* receiver, SmallMapList* receiver_maps,
    int argc, BailoutId ast_id, ApiCallType call_type,
    TailCallMode syntactic_tail_call_mode) {
  if (V8_UNLIKELY(FLAG_runtime_stats)) return false;
  if (function->IsJSFunction() &&
      Handle<JSFunction>::cast(function)->context()->native_context() !=
          top_info()->closure()->context()->native_context()) {
    return false;
  }
  if (argc > CallApiCallbackStub::kArgMax) {
    return false;
  }

  CallOptimization optimization(function);
  if (!optimization.is_simple_api_call()) return false;
  Handle<Map> holder_map;
  for (int i = 0; i < receiver_maps->length(); ++i) {
    auto map = receiver_maps->at(i);
    // Don't inline calls to receivers requiring accesschecks.
    if (map->is_access_check_needed()) return false;
  }
  if (call_type == kCallApiFunction) {
    // Cannot embed a direct reference to the global proxy map
    // as it maybe dropped on deserialization.
    CHECK(!isolate()->serializer_enabled());
    DCHECK(function->IsJSFunction());
    DCHECK_EQ(0, receiver_maps->length());
    receiver_maps->Add(
        handle(Handle<JSFunction>::cast(function)->global_proxy()->map()),
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

  bool is_function = false;
  bool is_store = false;
  switch (call_type) {
    case kCallApiFunction:
    case kCallApiMethod:
      // Need to check that none of the receiver maps could have changed.
      Add<HCheckMaps>(receiver, receiver_maps);
      // Need to ensure the chain between receiver and api_holder is intact.
      if (holder_lookup == CallOptimization::kHolderFound) {
        AddCheckPrototypeMaps(api_holder, receiver_maps->first());
      } else {
        DCHECK_EQ(holder_lookup, CallOptimization::kHolderIsReceiver);
      }
      // Includes receiver.
      PushArgumentsFromEnvironment(argc + 1);
      is_function = true;
      break;
    case kCallApiGetter:
      // Receiver and prototype chain cannot have changed.
      DCHECK_EQ(0, argc);
      DCHECK_NULL(receiver);
      // Receiver is on expression stack.
      receiver = Pop();
      Add<HPushArguments>(receiver);
      break;
    case kCallApiSetter:
      {
        is_store = true;
        // Receiver and prototype chain cannot have changed.
        DCHECK_EQ(1, argc);
        DCHECK_NULL(receiver);
        // Receiver and value are on expression stack.
        HValue* value = Pop();
        receiver = Pop();
        Add<HPushArguments>(receiver, value);
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
  HValue* call_data = Add<HConstant>(call_data_obj);
  ApiFunction fun(v8::ToCData<Address>(api_call_info->callback()));
  ExternalReference ref = ExternalReference(&fun,
                                            ExternalReference::DIRECT_API_CALL,
                                            isolate());
  HValue* api_function_address = Add<HConstant>(ExternalReference(ref));

  HValue* op_vals[] = {Add<HConstant>(function), call_data, holder,
                       api_function_address};

  HInstruction* call = nullptr;
  CHECK(argc <= CallApiCallbackStub::kArgMax);
  if (!is_function) {
    CallApiCallbackStub stub(isolate(), is_store,
                             !optimization.is_constant_call());
    Handle<Code> code = stub.GetCode();
    HConstant* code_value = Add<HConstant>(code);
    call = New<HCallWithDescriptor>(
        code_value, argc + 1, stub.GetCallInterfaceDescriptor(),
        Vector<HValue*>(op_vals, arraysize(op_vals)), syntactic_tail_call_mode);
  } else {
    CallApiCallbackStub stub(isolate(), argc, false);
    Handle<Code> code = stub.GetCode();
    HConstant* code_value = Add<HConstant>(code);
    call = New<HCallWithDescriptor>(
        code_value, argc + 1, stub.GetCallInterfaceDescriptor(),
        Vector<HValue*>(op_vals, arraysize(op_vals)), syntactic_tail_call_mode);
    Drop(1);  // Drop function.
  }

  ast_context()->ReturnInstruction(call, ast_id);
  return true;
}


void HOptimizedGraphBuilder::HandleIndirectCall(Call* expr, HValue* function,
                                                int arguments_count) {
  Handle<JSFunction> known_function;
  int args_count_no_receiver = arguments_count - 1;
  if (function->IsConstant() &&
      HConstant::cast(function)->handle(isolate())->IsJSFunction()) {
    known_function =
        Handle<JSFunction>::cast(HConstant::cast(function)->handle(isolate()));
    if (TryInlineBuiltinMethodCall(known_function, Handle<Map>(), expr->id(),
                                   args_count_no_receiver)) {
      if (FLAG_trace_inlining) {
        PrintF("Inlining builtin ");
        known_function->ShortPrint();
        PrintF("\n");
      }
      return;
    }

    if (TryInlineIndirectCall(known_function, expr, args_count_no_receiver)) {
      return;
    }
  }

  TailCallMode syntactic_tail_call_mode = expr->tail_call_mode();
  TailCallMode tail_call_mode =
      function_state()->ComputeTailCallMode(syntactic_tail_call_mode);

  PushArgumentsFromEnvironment(arguments_count);
  HInvokeFunction* call =
      New<HInvokeFunction>(function, known_function, arguments_count,
                           syntactic_tail_call_mode, tail_call_mode);
  Drop(1);  // Function
  ast_context()->ReturnInstruction(call, expr->id());
}


bool HOptimizedGraphBuilder::TryIndirectCall(Call* expr) {
  DCHECK(expr->expression()->IsProperty());

  if (!expr->IsMonomorphic()) {
    return false;
  }
  Handle<Map> function_map = expr->GetReceiverTypes()->first();
  if (function_map->instance_type() != JS_FUNCTION_TYPE ||
      !expr->target()->shared()->HasBuiltinFunctionId()) {
    return false;
  }

  switch (expr->target()->shared()->builtin_function_id()) {
    case kFunctionCall: {
      if (expr->arguments()->length() == 0) return false;
      BuildFunctionCall(expr);
      return true;
    }
    case kFunctionApply: {
      // For .apply, only the pattern f.apply(receiver, arguments)
      // is supported.
      if (!CanBeFunctionApplyArguments(expr)) return false;

      BuildFunctionApply(expr);
      return true;
    }
    default: { return false; }
  }
  UNREACHABLE();
}


// f.apply(...)
void HOptimizedGraphBuilder::BuildFunctionApply(Call* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  CHECK_ALIVE(VisitForValue(args->at(0)));
  HValue* receiver = Pop();  // receiver
  HValue* function = Pop();  // f
  Drop(1);  // apply

  // Make sure the arguments object is live.
  VariableProxy* arg_two = args->at(1)->AsVariableProxy();
  LookupAndMakeLive(arg_two->var());

  Handle<Map> function_map = expr->GetReceiverTypes()->first();
  HValue* checked_function = AddCheckMap(function, function_map);

  if (function_state()->outer() == NULL) {
    TailCallMode syntactic_tail_call_mode = expr->tail_call_mode();
    TailCallMode tail_call_mode =
        function_state()->ComputeTailCallMode(syntactic_tail_call_mode);

    HInstruction* elements = Add<HArgumentsElements>(false);
    HInstruction* length = Add<HArgumentsLength>(elements);
    HValue* wrapped_receiver = BuildWrapReceiver(receiver, checked_function);
    HInstruction* result = New<HApplyArguments>(
        function, wrapped_receiver, length, elements, tail_call_mode);
    ast_context()->ReturnInstruction(result, expr->id());
  } else {
    // We are inside inlined function and we know exactly what is inside
    // arguments object. But we need to be able to materialize at deopt.
    DCHECK_EQ(environment()->arguments_environment()->parameter_count(),
              function_state()->entry()->arguments_object()->arguments_count());
    HArgumentsObject* args = function_state()->entry()->arguments_object();
    const ZoneList<HValue*>* arguments_values = args->arguments_values();
    int arguments_count = arguments_values->length();
    Push(function);
    Push(BuildWrapReceiver(receiver, checked_function));
    for (int i = 1; i < arguments_count; i++) {
      Push(arguments_values->at(i));
    }
    HandleIndirectCall(expr, function, arguments_count);
  }
}


// f.call(...)
void HOptimizedGraphBuilder::BuildFunctionCall(Call* expr) {
  HValue* function = Top();  // f
  Handle<Map> function_map = expr->GetReceiverTypes()->first();
  HValue* checked_function = AddCheckMap(function, function_map);

  // f and call are on the stack in the unoptimized code
  // during evaluation of the arguments.
  CHECK_ALIVE(VisitExpressions(expr->arguments()));

  int args_length = expr->arguments()->length();
  int receiver_index = args_length - 1;
  // Patch the receiver.
  HValue* receiver = BuildWrapReceiver(
      environment()->ExpressionStackAt(receiver_index), checked_function);
  environment()->SetExpressionStackAt(receiver_index, receiver);

  // Call must not be on the stack from now on.
  int call_index = args_length + 1;
  environment()->RemoveExpressionStackAt(call_index);

  HandleIndirectCall(expr, function, args_length);
}


HValue* HOptimizedGraphBuilder::ImplicitReceiverFor(HValue* function,
                                                    Handle<JSFunction> target) {
  SharedFunctionInfo* shared = target->shared();
  if (is_sloppy(shared->language_mode()) && !shared->native()) {
    // Cannot embed a direct reference to the global proxy
    // as is it dropped on deserialization.
    CHECK(!isolate()->serializer_enabled());
    Handle<JSObject> global_proxy(target->context()->global_proxy());
    return Add<HConstant>(global_proxy);
  }
  return graph()->GetConstantUndefined();
}


HValue* HOptimizedGraphBuilder::BuildArrayIndexOf(HValue* receiver,
                                                  HValue* search_element,
                                                  ElementsKind kind,
                                                  ArrayIndexOfMode mode) {
  DCHECK(IsFastElementsKind(kind));

  NoObservableSideEffectsScope no_effects(this);

  HValue* elements = AddLoadElements(receiver);
  HValue* length = AddLoadArrayLength(receiver, kind);

  HValue* initial;
  HValue* terminating;
  Token::Value token;
  LoopBuilder::Direction direction;
  if (mode == kFirstIndexOf) {
    initial = graph()->GetConstant0();
    terminating = length;
    token = Token::LT;
    direction = LoopBuilder::kPostIncrement;
  } else {
    DCHECK_EQ(kLastIndexOf, mode);
    initial = length;
    terminating = graph()->GetConstant0();
    token = Token::GT;
    direction = LoopBuilder::kPreDecrement;
  }

  Push(graph()->GetConstantMinus1());
  if (IsFastDoubleElementsKind(kind) || IsFastSmiElementsKind(kind)) {
    // Make sure that we can actually compare numbers correctly below, see
    // https://code.google.com/p/chromium/issues/detail?id=407946 for details.
    search_element = AddUncasted<HForceRepresentation>(
        search_element, IsFastSmiElementsKind(kind) ? Representation::Smi()
                                                    : Representation::Double());

    LoopBuilder loop(this, context(), direction);
    {
      HValue* index = loop.BeginBody(initial, terminating, token);
      HValue* element = AddUncasted<HLoadKeyed>(
          elements, index, nullptr, nullptr, kind, ALLOW_RETURN_HOLE);
      IfBuilder if_issame(this);
      if_issame.If<HCompareNumericAndBranch>(element, search_element,
                                             Token::EQ_STRICT);
      if_issame.Then();
      {
        Drop(1);
        Push(index);
        loop.Break();
      }
      if_issame.End();
    }
    loop.EndBody();
  } else {
    IfBuilder if_isstring(this);
    if_isstring.If<HIsStringAndBranch>(search_element);
    if_isstring.Then();
    {
      LoopBuilder loop(this, context(), direction);
      {
        HValue* index = loop.BeginBody(initial, terminating, token);
        HValue* element = AddUncasted<HLoadKeyed>(
            elements, index, nullptr, nullptr, kind, ALLOW_RETURN_HOLE);
        IfBuilder if_issame(this);
        if_issame.If<HIsStringAndBranch>(element);
        if_issame.AndIf<HStringCompareAndBranch>(
            element, search_element, Token::EQ_STRICT);
        if_issame.Then();
        {
          Drop(1);
          Push(index);
          loop.Break();
        }
        if_issame.End();
      }
      loop.EndBody();
    }
    if_isstring.Else();
    {
      IfBuilder if_isnumber(this);
      if_isnumber.If<HIsSmiAndBranch>(search_element);
      if_isnumber.OrIf<HCompareMap>(
          search_element, isolate()->factory()->heap_number_map());
      if_isnumber.Then();
      {
        HValue* search_number =
            AddUncasted<HForceRepresentation>(search_element,
                                              Representation::Double());
        LoopBuilder loop(this, context(), direction);
        {
          HValue* index = loop.BeginBody(initial, terminating, token);
          HValue* element = AddUncasted<HLoadKeyed>(
              elements, index, nullptr, nullptr, kind, ALLOW_RETURN_HOLE);

          IfBuilder if_element_isnumber(this);
          if_element_isnumber.If<HIsSmiAndBranch>(element);
          if_element_isnumber.OrIf<HCompareMap>(
              element, isolate()->factory()->heap_number_map());
          if_element_isnumber.Then();
          {
            HValue* number =
                AddUncasted<HForceRepresentation>(element,
                                                  Representation::Double());
            IfBuilder if_issame(this);
            if_issame.If<HCompareNumericAndBranch>(
                number, search_number, Token::EQ_STRICT);
            if_issame.Then();
            {
              Drop(1);
              Push(index);
              loop.Break();
            }
            if_issame.End();
          }
          if_element_isnumber.End();
        }
        loop.EndBody();
      }
      if_isnumber.Else();
      {
        LoopBuilder loop(this, context(), direction);
        {
          HValue* index = loop.BeginBody(initial, terminating, token);
          HValue* element = AddUncasted<HLoadKeyed>(
              elements, index, nullptr, nullptr, kind, ALLOW_RETURN_HOLE);
          IfBuilder if_issame(this);
          if_issame.If<HCompareObjectEqAndBranch>(
              element, search_element);
          if_issame.Then();
          {
            Drop(1);
            Push(index);
            loop.Break();
          }
          if_issame.End();
        }
        loop.EndBody();
      }
      if_isnumber.End();
    }
    if_isstring.End();
  }

  return Pop();
}

template <class T>
bool HOptimizedGraphBuilder::TryHandleArrayCall(T* expr, HValue* function) {
  if (!array_function().is_identical_to(expr->target())) {
    return false;
  }

  Handle<AllocationSite> site = expr->allocation_site();
  if (site.is_null()) return false;

  Add<HCheckValue>(function, array_function());

  int arguments_count = expr->arguments()->length();
  if (TryInlineArrayCall(expr, arguments_count, site)) return true;

  HInstruction* call = PreProcessCall(New<HCallNewArray>(
      function, arguments_count + 1, site->GetElementsKind(), site));
  if (expr->IsCall()) Drop(1);
  ast_context()->ReturnInstruction(call, expr->id());

  return true;
}


bool HOptimizedGraphBuilder::CanBeFunctionApplyArguments(Call* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  if (args->length() != 2) return false;
  VariableProxy* arg_two = args->at(1)->AsVariableProxy();
  if (arg_two == NULL || !arg_two->var()->IsStackAllocated()) return false;
  HValue* arg_two_value = environment()->Lookup(arg_two->var());
  if (!arg_two_value->CheckFlag(HValue::kIsArguments)) return false;
  DCHECK_NOT_NULL(current_info()->scope()->arguments());
  return true;
}


void HOptimizedGraphBuilder::VisitCall(Call* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  if (!is_tracking_positions()) SetSourcePosition(expr->position());
  Expression* callee = expr->expression();
  int argument_count = expr->arguments()->length() + 1;  // Plus receiver.
  HInstruction* call = NULL;

  TailCallMode syntactic_tail_call_mode = expr->tail_call_mode();
  TailCallMode tail_call_mode =
      function_state()->ComputeTailCallMode(syntactic_tail_call_mode);

  Property* prop = callee->AsProperty();
  if (prop != NULL) {
    CHECK_ALIVE(VisitForValue(prop->obj()));
    HValue* receiver = Top();

    SmallMapList* maps;
    ComputeReceiverTypes(expr, receiver, &maps, this);

    if (prop->key()->IsPropertyName() && maps->length() > 0) {
      Handle<String> name = prop->key()->AsLiteral()->AsPropertyName();
      PropertyAccessInfo info(this, LOAD, maps->first(), name);
      if (!info.CanAccessAsMonomorphic(maps)) {
        HandlePolymorphicCallNamed(expr, receiver, maps, name);
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

    if (function->IsConstant() &&
        HConstant::cast(function)->handle(isolate())->IsJSFunction()) {
      // Push the function under the receiver.
      environment()->SetExpressionStackAt(0, function);
      Push(receiver);

      Handle<JSFunction> known_function = Handle<JSFunction>::cast(
          HConstant::cast(function)->handle(isolate()));
      expr->set_target(known_function);

      if (TryIndirectCall(expr)) return;
      CHECK_ALIVE(VisitExpressions(expr->arguments()));

      Handle<Map> map = maps->length() == 1 ? maps->first() : Handle<Map>();
      if (TryInlineBuiltinMethodCall(known_function, map, expr->id(),
                                     expr->arguments()->length())) {
        if (FLAG_trace_inlining) {
          PrintF("Inlining builtin ");
          known_function->ShortPrint();
          PrintF("\n");
        }
        return;
      }
      if (TryInlineApiMethodCall(expr, receiver, maps)) return;

      // Wrap the receiver if necessary.
      if (NeedsWrapping(maps->first(), known_function)) {
        // Since HWrapReceiver currently cannot actually wrap numbers and
        // strings, use the regular call builtin for method calls to wrap
        // the receiver.
        // TODO(verwaest): Support creation of value wrappers directly in
        // HWrapReceiver.
        call = NewCallFunction(
            function, argument_count, syntactic_tail_call_mode,
            ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode);
      } else if (TryInlineCall(expr)) {
        return;
      } else {
        call =
            NewCallConstantFunction(known_function, argument_count,
                                    syntactic_tail_call_mode, tail_call_mode);
      }

    } else {
      ArgumentsAllowedFlag arguments_flag = ARGUMENTS_NOT_ALLOWED;
      if (CanBeFunctionApplyArguments(expr) && expr->is_uninitialized()) {
        // We have to use EAGER deoptimization here because Deoptimizer::SOFT
        // gets ignored by the always-opt flag, which leads to incorrect code.
        Add<HDeoptimize>(
            DeoptimizeReason::kInsufficientTypeFeedbackForCallWithArguments,
            Deoptimizer::EAGER);
        arguments_flag = ARGUMENTS_FAKED;
      }

      // Push the function under the receiver.
      environment()->SetExpressionStackAt(0, function);
      Push(receiver);

      CHECK_ALIVE(VisitExpressions(expr->arguments(), arguments_flag));
      call = NewCallFunction(function, argument_count, syntactic_tail_call_mode,
                             ConvertReceiverMode::kNotNullOrUndefined,
                             tail_call_mode);
    }
    PushArgumentsFromEnvironment(argument_count);

  } else {
    if (expr->is_possibly_eval()) {
      return Bailout(kPossibleDirectCallToEval);
    }

    // The function is on the stack in the unoptimized code during
    // evaluation of the arguments.
    CHECK_ALIVE(VisitForValue(expr->expression()));
    HValue* function = Top();
    if (function->IsConstant() &&
        HConstant::cast(function)->handle(isolate())->IsJSFunction()) {
      Handle<Object> constant = HConstant::cast(function)->handle(isolate());
      Handle<JSFunction> target = Handle<JSFunction>::cast(constant);
      expr->SetKnownGlobalTarget(target);
    }

    // Placeholder for the receiver.
    Push(graph()->GetConstantUndefined());
    CHECK_ALIVE(VisitExpressions(expr->arguments()));

    if (expr->IsMonomorphic() &&
        !IsClassConstructor(expr->target()->shared()->kind())) {
      Add<HCheckValue>(function, expr->target());

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
      if (TryHandleArrayCall(expr, function)) return;
      if (TryInlineCall(expr)) return;

      PushArgumentsFromEnvironment(argument_count);
      call = NewCallConstantFunction(expr->target(), argument_count,
                                     syntactic_tail_call_mode, tail_call_mode);
    } else {
      PushArgumentsFromEnvironment(argument_count);
      if (expr->is_uninitialized()) {
        // We've never seen this call before, so let's have Crankshaft learn
        // through the type vector.
        call = NewCallFunctionViaIC(function, argument_count,
                                    syntactic_tail_call_mode,
                                    ConvertReceiverMode::kNullOrUndefined,
                                    tail_call_mode, expr->CallFeedbackICSlot());
      } else {
        call = NewCallFunction(
            function, argument_count, syntactic_tail_call_mode,
            ConvertReceiverMode::kNullOrUndefined, tail_call_mode);
      }
    }
  }

  Drop(1);  // Drop the function.
  return ast_context()->ReturnInstruction(call, expr->id());
}

bool HOptimizedGraphBuilder::TryInlineArrayCall(Expression* expression,
                                                int argument_count,
                                                Handle<AllocationSite> site) {
  Handle<JSFunction> caller = current_info()->closure();
  Handle<JSFunction> target = array_function();

  if (!site->CanInlineCall()) {
    TraceInline(target, caller, "AllocationSite requested no inlining.");
    return false;
  }

  if (argument_count > 1) {
    TraceInline(target, caller, "Too many arguments to inline.");
    return false;
  }

  int array_length = 0;
  // Do not inline if the constant length argument is not a smi or outside the
  // valid range for unrolled loop initialization.
  if (argument_count == 1) {
    HValue* argument = Top();
    if (!argument->IsConstant()) {
      TraceInline(target, caller,
                  "Dont inline [new] Array(n) where n isn't constant.");
      return false;
    }

    HConstant* constant_argument = HConstant::cast(argument);
    if (!constant_argument->HasSmiValue()) {
      TraceInline(target, caller,
                  "Constant length outside of valid inlining range.");
      return false;
    }
    array_length = constant_argument->Integer32Value();
    if (array_length < 0 || array_length > kElementLoopUnrollThreshold) {
      TraceInline(target, caller,
                  "Constant length outside of valid inlining range.");
      return false;
    }
  }

  TraceInline(target, caller, NULL);

  NoObservableSideEffectsScope no_effects(this);

  // Register on the site for deoptimization if the transition feedback changes.
  top_info()->dependencies()->AssumeTransitionStable(site);

  // Build the array.
  ElementsKind kind = site->GetElementsKind();
  HValue* capacity;
  HValue* length;
  if (array_length == 0) {
    STATIC_ASSERT(0 < JSArray::kPreallocatedArrayElements);
    const int initial_capacity = JSArray::kPreallocatedArrayElements;
    capacity = Add<HConstant>(initial_capacity);
    length = graph()->GetConstant0();
  } else {
    length = Top();
    capacity = length;
    kind = GetHoleyElementsKind(kind);
  }

  // These HForceRepresentations are because we store these as fields in the
  // objects we construct, and an int32-to-smi HChange could deopt. Accept
  // the deopt possibility now, before allocation occurs.
  length = AddUncasted<HForceRepresentation>(length, Representation::Smi());
  capacity = AddUncasted<HForceRepresentation>(capacity, Representation::Smi());

  // Generate size calculation code here in order to make it dominate
  // the JSArray allocation.
  HValue* elements_size = BuildCalculateElementsSize(kind, capacity);

  // Bail out for large objects.
  HValue* max_size = Add<HConstant>(kMaxRegularHeapObjectSize);
  Add<HBoundsCheck>(elements_size, max_size);

  // Allocate (dealing with failure appropriately).
  AllocationSiteMode mode = DONT_TRACK_ALLOCATION_SITE;
  HAllocate* new_object = AllocateJSArrayObject(mode);

  // Fill in the fields: map, properties, length.
  Handle<Map> map_constant(isolate()->get_initial_js_array_map(kind));
  HValue* map = Add<HConstant>(map_constant);

  BuildJSArrayHeader(new_object, map,
                     nullptr,  // set elements to empty fixed array
                     mode, kind, nullptr, length);

  // Allocate and initialize the elements.
  HAllocate* elements = BuildAllocateElements(kind, elements_size);
  BuildInitializeElementsHeader(elements, kind, capacity);
  BuildFillElementsWithHole(elements, kind, graph()->GetConstant0(), capacity);

  // Set the elements.
  Add<HStoreNamedField>(new_object, HObjectAccess::ForElementsPointer(),
                        elements);

  int args_to_drop = argument_count + (expression->IsCall() ? 2 : 1);
  Drop(args_to_drop);
  ast_context()->ReturnValue(new_object);
  return true;
}


// Checks whether allocation using the given constructor can be inlined.
static bool IsAllocationInlineable(Handle<JSFunction> constructor) {
  return constructor->has_initial_map() &&
         !IsDerivedConstructor(constructor->shared()->kind()) &&
         !constructor->initial_map()->is_dictionary_map() &&
         constructor->initial_map()->instance_type() == JS_OBJECT_TYPE &&
         constructor->initial_map()->instance_size() <
             HAllocate::kMaxInlineSize;
}

void HOptimizedGraphBuilder::VisitCallNew(CallNew* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  if (!is_tracking_positions()) SetSourcePosition(expr->position());
  int argument_count = expr->arguments()->length() + 1;  // Plus constructor.
  Factory* factory = isolate()->factory();

  // The constructor function is on the stack in the unoptimized code
  // during evaluation of the arguments.
  CHECK_ALIVE(VisitForValue(expr->expression()));
  HValue* function = Top();
  CHECK_ALIVE(VisitExpressions(expr->arguments()));

  if (function->IsConstant() &&
      HConstant::cast(function)->handle(isolate())->IsJSFunction()) {
    Handle<Object> constant = HConstant::cast(function)->handle(isolate());
    expr->SetKnownGlobalTarget(Handle<JSFunction>::cast(constant));
  }

  if (FLAG_inline_construct &&
      expr->IsMonomorphic() &&
      IsAllocationInlineable(expr->target())) {
    Handle<JSFunction> constructor = expr->target();
    DCHECK(constructor->shared()->construct_stub() ==
               isolate()->builtins()->builtin(
                   Builtins::kJSConstructStubGenericRestrictedReturn) ||
           constructor->shared()->construct_stub() ==
               isolate()->builtins()->builtin(
                   Builtins::kJSConstructStubGenericUnrestrictedReturn) ||
           constructor->shared()->construct_stub() ==
               isolate()->builtins()->builtin(Builtins::kJSConstructStubApi));
    HValue* check = Add<HCheckValue>(function, constructor);

    // Force completion of inobject slack tracking before generating
    // allocation code to finalize instance size.
    constructor->CompleteInobjectSlackTrackingIfActive();

    // Calculate instance size from initial map of constructor.
    DCHECK(constructor->has_initial_map());
    Handle<Map> initial_map(constructor->initial_map());
    int instance_size = initial_map->instance_size();

    // Allocate an instance of the implicit receiver object.
    HValue* size_in_bytes = Add<HConstant>(instance_size);
    HAllocationMode allocation_mode;
    HAllocate* receiver = BuildAllocate(
        size_in_bytes, HType::JSObject(), JS_OBJECT_TYPE, allocation_mode);
    receiver->set_known_initial_map(initial_map);

    // Initialize map and fields of the newly allocated object.
    { NoObservableSideEffectsScope no_effects(this);
      DCHECK(initial_map->instance_type() == JS_OBJECT_TYPE);
      Add<HStoreNamedField>(receiver,
          HObjectAccess::ForMapAndOffset(initial_map, JSObject::kMapOffset),
          Add<HConstant>(initial_map));
      HValue* empty_fixed_array = Add<HConstant>(factory->empty_fixed_array());
      Add<HStoreNamedField>(receiver,
          HObjectAccess::ForMapAndOffset(initial_map,
                                         JSObject::kPropertiesOffset),
          empty_fixed_array);
      Add<HStoreNamedField>(receiver,
          HObjectAccess::ForMapAndOffset(initial_map,
                                         JSObject::kElementsOffset),
          empty_fixed_array);
      BuildInitializeInobjectProperties(receiver, initial_map);
    }

    // Replace the constructor function with a newly allocated receiver using
    // the index of the receiver from the top of the expression stack.
    const int receiver_index = argument_count - 1;
    DCHECK(environment()->ExpressionStackAt(receiver_index) == function);
    environment()->SetExpressionStackAt(receiver_index, receiver);

    if (TryInlineConstruct(expr, receiver)) {
      // Inlining worked, add a dependency on the initial map to make sure that
      // this code is deoptimized whenever the initial map of the constructor
      // changes.
      top_info()->dependencies()->AssumeInitialMapCantChange(initial_map);
      return;
    }

    // TODO(mstarzinger): For now we remove the previous HAllocate and all
    // corresponding instructions and instead add HPushArguments for the
    // arguments in case inlining failed.  What we actually should do is for
    // inlining to try to build a subgraph without mutating the parent graph.
    HInstruction* instr = current_block()->last();
    do {
      HInstruction* prev_instr = instr->previous();
      instr->DeleteAndReplaceWith(NULL);
      instr = prev_instr;
    } while (instr != check);
    environment()->SetExpressionStackAt(receiver_index, function);
  } else {
    // The constructor function is both an operand to the instruction and an
    // argument to the construct call.
    if (TryHandleArrayCall(expr, function)) return;
  }

  HValue* arity = Add<HConstant>(argument_count - 1);
  HValue* op_vals[] = {function, function, arity};
  Callable callable = CodeFactory::Construct(isolate());
  HConstant* stub = Add<HConstant>(callable.code());
  PushArgumentsFromEnvironment(argument_count);
  HInstruction* construct = New<HCallWithDescriptor>(
      stub, argument_count, callable.descriptor(), ArrayVector(op_vals));
  return ast_context()->ReturnInstruction(construct, expr->id());
}


void HOptimizedGraphBuilder::BuildInitializeInobjectProperties(
    HValue* receiver, Handle<Map> initial_map) {
  if (initial_map->GetInObjectProperties() != 0) {
    HConstant* undefined = graph()->GetConstantUndefined();
    for (int i = 0; i < initial_map->GetInObjectProperties(); i++) {
      int property_offset = initial_map->GetInObjectPropertyOffset(i);
      Add<HStoreNamedField>(receiver, HObjectAccess::ForMapAndOffset(
                                          initial_map, property_offset),
                            undefined);
    }
  }
}

void HOptimizedGraphBuilder::GenerateMaxSmi(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  HConstant* max_smi = New<HConstant>(static_cast<int32_t>(Smi::kMaxValue));
  return ast_context()->ReturnInstruction(max_smi, expr->id());
}


void HOptimizedGraphBuilder::GenerateTypedArrayMaxSizeInHeap(
    CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  HConstant* result = New<HConstant>(static_cast<int32_t>(
        FLAG_typed_array_max_size_in_heap));
  return ast_context()->ReturnInstruction(result, expr->id());
}


void HOptimizedGraphBuilder::GenerateArrayBufferGetByteLength(
    CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(expr->arguments()->at(0)));
  HValue* buffer = Pop();
  HInstruction* result = New<HLoadNamedField>(
      buffer, nullptr, HObjectAccess::ForJSArrayBufferByteLength());
  return ast_context()->ReturnInstruction(result, expr->id());
}


void HOptimizedGraphBuilder::GenerateArrayBufferViewGetByteLength(
    CallRuntime* expr) {
  NoObservableSideEffectsScope scope(this);
  DCHECK(expr->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(expr->arguments()->at(0)));
  HValue* view = Pop();

  return ast_context()->ReturnValue(BuildArrayBufferViewFieldAccessor(
      view, nullptr,
      FieldIndex::ForInObjectOffset(JSArrayBufferView::kByteLengthOffset)));
}


void HOptimizedGraphBuilder::GenerateArrayBufferViewGetByteOffset(
    CallRuntime* expr) {
  NoObservableSideEffectsScope scope(this);
  DCHECK(expr->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(expr->arguments()->at(0)));
  HValue* view = Pop();

  return ast_context()->ReturnValue(BuildArrayBufferViewFieldAccessor(
      view, nullptr,
      FieldIndex::ForInObjectOffset(JSArrayBufferView::kByteOffsetOffset)));
}

void HOptimizedGraphBuilder::GenerateArrayBufferViewWasNeutered(
    CallRuntime* expr) {
  NoObservableSideEffectsScope scope(this);
  DCHECK_EQ(expr->arguments()->length(), 1);
  CHECK_ALIVE(VisitForValue(expr->arguments()->at(0)));
  HValue* view = Pop();

  HInstruction* buffer = Add<HLoadNamedField>(
      view, nullptr, HObjectAccess::ForJSArrayBufferViewBuffer());
  HInstruction* flags = Add<HLoadNamedField>(
      buffer, nullptr, HObjectAccess::ForJSArrayBufferBitField());
  HValue* was_neutered_mask =
      Add<HConstant>(1 << JSArrayBuffer::WasNeutered::kShift);
  HValue* was_neutered =
      AddUncasted<HBitwise>(Token::BIT_AND, flags, was_neutered_mask);
  return ast_context()->ReturnValue(was_neutered);
}

void HOptimizedGraphBuilder::GenerateTypedArrayGetLength(
    CallRuntime* expr) {
  NoObservableSideEffectsScope scope(this);
  DCHECK(expr->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(expr->arguments()->at(0)));
  HValue* view = Pop();

  return ast_context()->ReturnValue(BuildArrayBufferViewFieldAccessor(
      view, nullptr,
      FieldIndex::ForInObjectOffset(JSTypedArray::kLengthOffset)));
}


void HOptimizedGraphBuilder::VisitCallRuntime(CallRuntime* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  if (expr->is_jsruntime()) {
    // Crankshaft always specializes to the native context, so we can just grab
    // the constant function from the current native context and embed that into
    // the code object.
    Handle<JSFunction> known_function(
        JSFunction::cast(
            current_info()->native_context()->get(expr->context_index())),
        isolate());

    // The callee and the receiver both have to be pushed onto the operand stack
    // before arguments are being evaluated.
    HConstant* function = Add<HConstant>(known_function);
    HValue* receiver = ImplicitReceiverFor(function, known_function);
    Push(function);
    Push(receiver);

    int argument_count = expr->arguments()->length() + 1;  // Count receiver.
    CHECK_ALIVE(VisitExpressions(expr->arguments()));
    PushArgumentsFromEnvironment(argument_count);
    HInstruction* call = NewCallConstantFunction(known_function, argument_count,
                                                 TailCallMode::kDisallow,
                                                 TailCallMode::kDisallow);
    Drop(1);  // Function
    return ast_context()->ReturnInstruction(call, expr->id());
  }

  const Runtime::Function* function = expr->function();
  DCHECK(function != NULL);
  switch (function->function_id) {
#define CALL_INTRINSIC_GENERATOR(Name) \
  case Runtime::kInline##Name:         \
    return Generate##Name(expr);

    FOR_EACH_HYDROGEN_INTRINSIC(CALL_INTRINSIC_GENERATOR)
#undef CALL_INTRINSIC_GENERATOR
    default: {
      int argument_count = expr->arguments()->length();
      CHECK_ALIVE(VisitExpressions(expr->arguments()));
      PushArgumentsFromEnvironment(argument_count);
      HCallRuntime* call = New<HCallRuntime>(function, argument_count);
      return ast_context()->ReturnInstruction(call, expr->id());
    }
  }
}


void HOptimizedGraphBuilder::VisitUnaryOperation(UnaryOperation* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
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
    HValue* language_mode = Add<HConstant>(
        static_cast<int32_t>(function_language_mode()), Representation::Smi());
    Add<HPushArguments>(obj, key, language_mode);
    HInstruction* instr =
        New<HCallRuntime>(Runtime::FunctionForId(Runtime::kDeleteProperty), 3);
    return ast_context()->ReturnInstruction(instr, expr->id());
  } else if (proxy != NULL) {
    Variable* var = proxy->var();
    if (var->IsUnallocated()) {
      Bailout(kDeleteWithGlobalVariable);
    } else if (var->IsStackAllocated() || var->IsContextSlot()) {
      // Result of deleting non-global variables is false.  'this' is not really
      // a variable, though we implement it as one.  The subexpression does not
      // have side effects.
      HValue* value = var->is_this() ? graph()->GetConstantTrue()
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

  DCHECK(ast_context()->IsValue());
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

static Representation RepresentationFor(AstType* type) {
  DisallowHeapAllocation no_allocation;
  if (type->Is(AstType::None())) return Representation::None();
  if (type->Is(AstType::SignedSmall())) return Representation::Smi();
  if (type->Is(AstType::Signed32())) return Representation::Integer32();
  if (type->Is(AstType::Number())) return Representation::Double();
  return Representation::Tagged();
}

HInstruction* HOptimizedGraphBuilder::BuildIncrement(CountOperation* expr) {
  // The input to the count operation is on top of the expression stack.
  Representation rep = RepresentationFor(expr->type());
  if (rep.IsNone() || rep.IsTagged()) {
    rep = Representation::Smi();
  }

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
  instr->ClearAllSideEffects();
  instr->SetFlag(HInstruction::kCannotBeTagged);
  return instr;
}

void HOptimizedGraphBuilder::BuildStoreForEffect(
    Expression* expr, Property* prop, FeedbackSlot slot, BailoutId ast_id,
    BailoutId return_id, HValue* object, HValue* key, HValue* value) {
  EffectContext for_effect(this);
  Push(object);
  if (key != NULL) Push(key);
  Push(value);
  BuildStore(expr, prop, slot, ast_id, return_id);
}


void HOptimizedGraphBuilder::VisitCountOperation(CountOperation* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  if (!is_tracking_positions()) SetSourcePosition(expr->position());
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
    if (var->mode() == CONST) {
      return Bailout(kNonInitializerAssignmentToConst);
    }
    // Argument of the count operation is a variable, not a property.
    DCHECK(prop == NULL);
    CHECK_ALIVE(VisitForValue(target));

    after = BuildIncrement(expr);
    input = returns_original_input ? Top() : Pop();
    Push(after);

    switch (var->location()) {
      case VariableLocation::UNALLOCATED:
        HandleGlobalVariableAssignment(var, after, expr->CountSlot(),
                                       expr->AssignmentId());
        break;

      case VariableLocation::PARAMETER:
      case VariableLocation::LOCAL:
        BindIfLive(var, after);
        break;

      case VariableLocation::CONTEXT: {
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

      case VariableLocation::LOOKUP:
        return Bailout(kLookupVariableInCountOperation);

      case VariableLocation::MODULE:
        UNREACHABLE();
    }

    Drop(returns_original_input ? 2 : 1);
    return ast_context()->ReturnValue(expr->is_postfix() ? input : after);
  }

  // Argument of the count operation is a property.
  DCHECK(prop != NULL);
  if (returns_original_input) Push(graph()->GetConstantUndefined());

  CHECK_ALIVE(VisitForValue(prop->obj()));
  HValue* object = Top();

  HValue* key = NULL;
  if (!prop->key()->IsPropertyName() || prop->IsStringAccess()) {
    CHECK_ALIVE(VisitForValue(prop->key()));
    key = Top();
  }

  CHECK_ALIVE(PushLoad(prop, object, key));

  after = BuildIncrement(expr);

  if (returns_original_input) {
    input = Pop();
    // Drop object and key to push it again in the effect context below.
    Drop(key == NULL ? 1 : 2);
    environment()->SetExpressionStackAt(0, input);
    CHECK_ALIVE(BuildStoreForEffect(expr, prop, expr->CountSlot(), expr->id(),
                                    expr->AssignmentId(), object, key, after));
    return ast_context()->ReturnValue(Pop());
  }

  environment()->SetExpressionStackAt(0, after);
  return BuildStore(expr, prop, expr->CountSlot(), expr->id(),
                    expr->AssignmentId());
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
        return New<HConstant>(std::numeric_limits<double>::quiet_NaN());
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
  return sub->left()->EqualsInteger32Constant(32) && sub->right() == sa;
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
  *operand = shr->left();
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

HValue* HGraphBuilder::EnforceNumberType(HValue* number, AstType* expected) {
  if (expected->Is(AstType::SignedSmall())) {
    return AddUncasted<HForceRepresentation>(number, Representation::Smi());
  }
  if (expected->Is(AstType::Signed32())) {
    return AddUncasted<HForceRepresentation>(number,
                                             Representation::Integer32());
  }
  return number;
}

HValue* HGraphBuilder::TruncateToNumber(HValue* value, AstType** expected) {
  if (value->IsConstant()) {
    HConstant* constant = HConstant::cast(value);
    Maybe<HConstant*> number =
        constant->CopyToTruncatedNumber(isolate(), zone());
    if (number.IsJust()) {
      *expected = AstType::Number();
      return AddInstruction(number.FromJust());
    }
  }

  // We put temporary values on the stack, which don't correspond to anything
  // in baseline code. Since nothing is observable we avoid recording those
  // pushes with a NoObservableSideEffectsScope.
  NoObservableSideEffectsScope no_effects(this);

  AstType* expected_type = *expected;

  // Separate the number type from the rest.
  AstType* expected_obj =
      AstType::Intersect(expected_type, AstType::NonNumber(), zone());
  AstType* expected_number =
      AstType::Intersect(expected_type, AstType::Number(), zone());

  // We expect to get a number.
  // (We need to check first, since AstType::None->Is(AstType::Any()) == true.
  if (expected_obj->Is(AstType::None())) {
    DCHECK(!expected_number->Is(AstType::None()));
    return value;
  }

  if (expected_obj->Is(AstType::Undefined())) {
    // This is already done by HChange.
    *expected = AstType::Union(expected_number, AstType::Number(), zone());
    return value;
  }

  return value;
}


HValue* HOptimizedGraphBuilder::BuildBinaryOperation(
    BinaryOperation* expr,
    HValue* left,
    HValue* right,
    PushBeforeSimulateBehavior push_sim_result) {
  AstType* left_type = bounds_.get(expr->left()).lower;
  AstType* right_type = bounds_.get(expr->right()).lower;
  AstType* result_type = bounds_.get(expr).lower;
  Maybe<int> fixed_right_arg = expr->fixed_right_arg();
  Handle<AllocationSite> allocation_site = expr->allocation_site();

  HAllocationMode allocation_mode;
  if (FLAG_allocation_site_pretenuring && !allocation_site.is_null()) {
    allocation_mode = HAllocationMode(allocation_site);
  }
  HValue* result = HGraphBuilder::BuildBinaryOperation(
      expr->op(), left, right, left_type, right_type, result_type,
      fixed_right_arg, allocation_mode, expr->id());
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
    Token::Value op, HValue* left, HValue* right, AstType* left_type,
    AstType* right_type, AstType* result_type, Maybe<int> fixed_right_arg,
    HAllocationMode allocation_mode, BailoutId opt_id) {
  bool maybe_string_add = false;
  if (op == Token::ADD) {
    // If we are adding constant string with something for which we don't have
    // a feedback yet, assume that it's also going to be a string and don't
    // generate deopt instructions.
    if (!left_type->IsInhabited() && right->IsConstant() &&
        HConstant::cast(right)->HasStringValue()) {
      left_type = AstType::String();
    }

    if (!right_type->IsInhabited() && left->IsConstant() &&
        HConstant::cast(left)->HasStringValue()) {
      right_type = AstType::String();
    }

    maybe_string_add = (left_type->Maybe(AstType::String()) ||
                        left_type->Maybe(AstType::Receiver()) ||
                        right_type->Maybe(AstType::String()) ||
                        right_type->Maybe(AstType::Receiver()));
  }

  Representation left_rep = RepresentationFor(left_type);
  Representation right_rep = RepresentationFor(right_type);

  if (!left_type->IsInhabited()) {
    Add<HDeoptimize>(
        DeoptimizeReason::kInsufficientTypeFeedbackForLHSOfBinaryOperation,
        Deoptimizer::SOFT);
    left_type = AstType::Any();
    left_rep = RepresentationFor(left_type);
    maybe_string_add = op == Token::ADD;
  }

  if (!right_type->IsInhabited()) {
    Add<HDeoptimize>(
        DeoptimizeReason::kInsufficientTypeFeedbackForRHSOfBinaryOperation,
        Deoptimizer::SOFT);
    right_type = AstType::Any();
    right_rep = RepresentationFor(right_type);
    maybe_string_add = op == Token::ADD;
  }

  if (!maybe_string_add) {
    left = TruncateToNumber(left, &left_type);
    right = TruncateToNumber(right, &right_type);
  }

  // Special case for string addition here.
  if (op == Token::ADD &&
      (left_type->Is(AstType::String()) || right_type->Is(AstType::String()))) {
    // Validate type feedback for left argument.
    if (left_type->Is(AstType::String())) {
      left = BuildCheckString(left);
    }

    // Validate type feedback for right argument.
    if (right_type->Is(AstType::String())) {
      right = BuildCheckString(right);
    }

    // Convert left argument as necessary.
    if (left_type->Is(AstType::Number())) {
      DCHECK(right_type->Is(AstType::String()));
      left = BuildNumberToString(left, left_type);
    } else if (!left_type->Is(AstType::String())) {
      DCHECK(right_type->Is(AstType::String()));
      return AddUncasted<HStringAdd>(
          left, right, allocation_mode.GetPretenureMode(),
          STRING_ADD_CONVERT_LEFT, allocation_mode.feedback_site());
    }

    // Convert right argument as necessary.
    if (right_type->Is(AstType::Number())) {
      DCHECK(left_type->Is(AstType::String()));
      right = BuildNumberToString(right, right_type);
    } else if (!right_type->Is(AstType::String())) {
      DCHECK(left_type->Is(AstType::String()));
      return AddUncasted<HStringAdd>(
          left, right, allocation_mode.GetPretenureMode(),
          STRING_ADD_CONVERT_RIGHT, allocation_mode.feedback_site());
    }

    // Fast paths for empty constant strings.
    Handle<String> left_string =
        left->IsConstant() && HConstant::cast(left)->HasStringValue()
            ? HConstant::cast(left)->StringValue()
            : Handle<String>();
    Handle<String> right_string =
        right->IsConstant() && HConstant::cast(right)->HasStringValue()
            ? HConstant::cast(right)->StringValue()
            : Handle<String>();
    if (!left_string.is_null() && left_string->length() == 0) return right;
    if (!right_string.is_null() && right_string->length() == 0) return left;
    if (!left_string.is_null() && !right_string.is_null()) {
      return AddUncasted<HStringAdd>(
          left, right, allocation_mode.GetPretenureMode(),
          STRING_ADD_CHECK_NONE, allocation_mode.feedback_site());
    }

    // Register the dependent code with the allocation site.
    if (!allocation_mode.feedback_site().is_null()) {
      DCHECK(!graph()->info()->IsStub());
      Handle<AllocationSite> site(allocation_mode.feedback_site());
      top_info()->dependencies()->AssumeTenuringDecision(site);
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
        left, right, allocation_mode.GetPretenureMode(), STRING_ADD_CHECK_NONE,
        allocation_mode.feedback_site());
  }

  // Special case for +x here.
  if (op == Token::MUL) {
    if (left->EqualsInteger32Constant(1)) {
      return BuildToNumber(right);
    }
    if (right->EqualsInteger32Constant(1)) {
      return BuildToNumber(left);
    }
  }

  if (graph()->info()->IsStub()) {
    left = EnforceNumberType(left, left_type);
    right = EnforceNumberType(right, right_type);
  }

  Representation result_rep = RepresentationFor(result_type);

  bool is_non_primitive = (left_rep.IsTagged() && !left_rep.IsSmi()) ||
                          (right_rep.IsTagged() && !right_rep.IsSmi());

  HInstruction* instr = NULL;
  // Only the stub is allowed to call into the runtime, since otherwise we would
  // inline several instructions (including the two pushes) for every tagged
  // operation in optimized code, which is more expensive, than a stub call.
  if (graph()->info()->IsStub() && is_non_primitive) {
    HValue* values[] = {left, right};
#define GET_STUB(Name)                                                       \
  do {                                                                       \
    Callable callable = CodeFactory::Name(isolate());                        \
    HValue* stub = Add<HConstant>(callable.code());                          \
    instr = AddUncasted<HCallWithDescriptor>(stub, 0, callable.descriptor(), \
                                             ArrayVector(values));           \
  } while (false)

    switch (op) {
      default:
        UNREACHABLE();
      case Token::ADD:
        GET_STUB(Add);
        break;
      case Token::SUB:
        GET_STUB(Subtract);
        break;
      case Token::MUL:
        GET_STUB(Multiply);
        break;
      case Token::DIV:
        GET_STUB(Divide);
        break;
      case Token::MOD:
        GET_STUB(Modulus);
        break;
      case Token::BIT_OR:
        GET_STUB(BitwiseOr);
        break;
      case Token::BIT_AND:
        GET_STUB(BitwiseAnd);
        break;
      case Token::BIT_XOR:
        GET_STUB(BitwiseXor);
        break;
      case Token::SAR:
        GET_STUB(ShiftRight);
        break;
      case Token::SHR:
        GET_STUB(ShiftRightLogical);
        break;
      case Token::SHL:
        GET_STUB(ShiftLeft);
        break;
    }
#undef GET_STUB
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
        if (fixed_right_arg.IsJust() &&
            !right->EqualsInteger32Constant(fixed_right_arg.FromJust())) {
          HConstant* fixed_right =
              Add<HConstant>(static_cast<int>(fixed_right_arg.FromJust()));
          IfBuilder if_same(this);
          if_same.If<HCompareNumericAndBranch>(right, fixed_right, Token::EQ);
          if_same.Then();
          if_same.ElseDeopt(DeoptimizeReason::kUnexpectedRHSOfBinaryOperation);
          right = fixed_right;
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
        HValue *operand, *shift_amount;
        if (left_type->Is(AstType::Signed32()) &&
            right_type->Is(AstType::Signed32()) &&
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
        if (instr->IsShr() && CanBeZero(right)) {
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
  if (call->is_jsruntime()) return false;
  if (call->function()->function_id != Runtime::kInlineClassOf) return false;
  DCHECK_EQ(call->arguments()->length(), 1);
  return true;
}

void HOptimizedGraphBuilder::VisitBinaryOperation(BinaryOperation* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
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
    CHECK(eval_right->HasPredecessor());
    eval_right->SetJoinId(expr->RightId());
    set_current_block(eval_right);
    Visit(expr->right());
  } else if (ast_context()->IsValue()) {
    CHECK_ALIVE(VisitForValue(expr->left()));
    DCHECK(current_block() != NULL);
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
    ToBooleanHints expected(expr->left()->to_boolean_types());
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
    DCHECK(ast_context()->IsEffect());
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

    // Technically, we should be able to handle the case when one side of
    // the test is not connected, but this can trip up liveness analysis
    // if we did not fully connect the test context based on some optimistic
    // assumption. If such an assumption was violated, we would end up with
    // an environment with optimized-out values. So we should always
    // conservatively connect the test context.

    CHECK(right_block->HasPredecessor());
    CHECK(empty_block->HasPredecessor());

    empty_block->SetJoinId(expr->id());

    right_block->SetJoinId(expr->RightId());
    set_current_block(right_block);
    CHECK_BAILOUT(VisitForEffect(expr->right()));
    right_block = current_block();

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

namespace {

bool IsLiteralCompareStrict(Isolate* isolate, HValue* left, Token::Value op,
                            HValue* right) {
  return op == Token::EQ_STRICT &&
         ((left->IsConstant() &&
           !HConstant::cast(left)->handle(isolate)->IsNumber() &&
           !HConstant::cast(left)->handle(isolate)->IsString()) ||
          (right->IsConstant() &&
           !HConstant::cast(right)->handle(isolate)->IsNumber() &&
           !HConstant::cast(right)->handle(isolate)->IsString()));
}

}  // namespace

void HOptimizedGraphBuilder::VisitCompareOperation(CompareOperation* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());

  if (!is_tracking_positions()) SetSourcePosition(expr->position());

  // Check for a few fast cases. The AST visiting behavior must be in sync
  // with the full codegen: We don't push both left and right values onto
  // the expression stack when one side is a special-case literal.
  Expression* sub_expr = NULL;
  Literal* literal;
  if (expr->IsLiteralCompareTypeof(&sub_expr, &literal)) {
    return HandleLiteralCompareTypeof(expr, sub_expr,
                                      Handle<String>::cast(literal->value()));
  }
  if (expr->IsLiteralCompareUndefined(&sub_expr)) {
    return HandleLiteralCompareNil(expr, sub_expr, kUndefinedValue);
  }
  if (expr->IsLiteralCompareNull(&sub_expr)) {
    return HandleLiteralCompareNil(expr, sub_expr, kNullValue);
  }

  if (IsClassOfTest(expr)) {
    CallRuntime* call = expr->left()->AsCallRuntime();
    DCHECK(call->arguments()->length() == 1);
    CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
    HValue* value = Pop();
    Literal* literal = expr->right()->AsLiteral();
    Handle<String> rhs = Handle<String>::cast(literal->value());
    HClassOfTestAndBranch* instr = New<HClassOfTestAndBranch>(value, rhs);
    return ast_context()->ReturnControl(instr, expr->id());
  }

  AstType* left_type = bounds_.get(expr->left()).lower;
  AstType* right_type = bounds_.get(expr->right()).lower;
  AstType* combined_type = expr->combined_type();

  CHECK_ALIVE(VisitForValue(expr->left()));
  CHECK_ALIVE(VisitForValue(expr->right()));

  HValue* right = Pop();
  HValue* left = Pop();
  Token::Value op = expr->op();

  if (IsLiteralCompareStrict(isolate(), left, op, right)) {
    HCompareObjectEqAndBranch* result =
        New<HCompareObjectEqAndBranch>(left, right);
    return ast_context()->ReturnControl(result, expr->id());
  }

  if (op == Token::INSTANCEOF) {
    // Check to see if the rhs of the instanceof is a known function.
    if (right->IsConstant() &&
        HConstant::cast(right)->handle(isolate())->IsJSFunction()) {
      Handle<JSFunction> function =
          Handle<JSFunction>::cast(HConstant::cast(right)->handle(isolate()));
      // Make sure that the {function} already has a meaningful initial map
      // (i.e. we constructed at least one instance using the constructor
      // {function}), and has an instance as .prototype.
      if (function->has_initial_map() &&
          !function->map()->has_non_instance_prototype()) {
        // Lookup @@hasInstance on the {function}.
        Handle<Map> function_map(function->map(), isolate());
        PropertyAccessInfo has_instance(
            this, LOAD, function_map,
            isolate()->factory()->has_instance_symbol());
        // Check if we are using the Function.prototype[@@hasInstance].
        if (has_instance.CanAccessMonomorphic() &&
            has_instance.IsDataConstant() &&
            has_instance.constant().is_identical_to(
                isolate()->function_has_instance())) {
          // Add appropriate receiver map check and prototype chain
          // checks to guard the @@hasInstance lookup chain.
          AddCheckMap(right, function_map);
          if (has_instance.has_holder()) {
            Handle<JSObject> prototype(
                JSObject::cast(has_instance.map()->prototype()), isolate());
            BuildCheckPrototypeMaps(prototype, has_instance.holder());
          }
          // Perform the prototype chain walk.
          Handle<Map> initial_map(function->initial_map(), isolate());
          top_info()->dependencies()->AssumeInitialMapCantChange(initial_map);
          HInstruction* prototype =
              Add<HConstant>(handle(initial_map->prototype(), isolate()));
          HHasInPrototypeChainAndBranch* result =
              New<HHasInPrototypeChainAndBranch>(left, prototype);
          return ast_context()->ReturnControl(result, expr->id());
        }
      }
    }

    Callable callable = CodeFactory::InstanceOf(isolate());
    HValue* stub = Add<HConstant>(callable.code());
    HValue* values[] = {left, right};
    HCallWithDescriptor* result = New<HCallWithDescriptor>(
        stub, 0, callable.descriptor(), ArrayVector(values));
    result->set_type(HType::Boolean());
    return ast_context()->ReturnInstruction(result, expr->id());

  } else if (op == Token::IN) {
    Callable callable = CodeFactory::HasProperty(isolate());
    HValue* stub = Add<HConstant>(callable.code());
    HValue* values[] = {left, right};
    HInstruction* result =
        New<HCallWithDescriptor>(stub, 0, callable.descriptor(),
                                 Vector<HValue*>(values, arraysize(values)));
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
    Token::Value op, HValue* left, HValue* right, AstType* left_type,
    AstType* right_type, AstType* combined_type, SourcePosition left_position,
    SourcePosition right_position, PushBeforeSimulateBehavior push_sim_result,
    BailoutId bailout_id) {
  // Cases handled below depend on collected type feedback. They should
  // soft deoptimize when there is no type feedback.
  if (!combined_type->IsInhabited()) {
    Add<HDeoptimize>(
        DeoptimizeReason::
            kInsufficientTypeFeedbackForCombinedTypeOfBinaryOperation,
        Deoptimizer::SOFT);
    combined_type = left_type = right_type = AstType::Any();
  }

  Representation left_rep = RepresentationFor(left_type);
  Representation right_rep = RepresentationFor(right_type);
  Representation combined_rep = RepresentationFor(combined_type);

  if (combined_type->Is(AstType::Receiver())) {
    if (Token::IsEqualityOp(op)) {
      // HCompareObjectEqAndBranch can only deal with object, so
      // exclude numbers.
      if ((left->IsConstant() &&
           HConstant::cast(left)->HasNumberValue()) ||
          (right->IsConstant() &&
           HConstant::cast(right)->HasNumberValue())) {
        Add<HDeoptimize>(
            DeoptimizeReason::kTypeMismatchBetweenFeedbackAndConstant,
            Deoptimizer::SOFT);
        // The caller expects a branch instruction, so make it happy.
        return New<HBranch>(graph()->GetConstantTrue());
      }
      if (op == Token::EQ) {
        // For abstract equality we need to check both sides are receivers.
        if (combined_type->IsClass()) {
          Handle<Map> map = combined_type->AsClass()->Map();
          AddCheckMap(left, map);
          AddCheckMap(right, map);
        } else {
          BuildCheckHeapObject(left);
          Add<HCheckInstanceType>(left, HCheckInstanceType::IS_JS_RECEIVER);
          BuildCheckHeapObject(right);
          Add<HCheckInstanceType>(right, HCheckInstanceType::IS_JS_RECEIVER);
        }
      } else {
        // For strict equality we only need to check one side.
        HValue* operand_to_check =
            left->block()->block_id() < right->block()->block_id() ? left
                                                                   : right;
        if (combined_type->IsClass()) {
          Handle<Map> map = combined_type->AsClass()->Map();
          AddCheckMap(operand_to_check, map);
        } else {
          BuildCheckHeapObject(operand_to_check);
          Add<HCheckInstanceType>(operand_to_check,
                                  HCheckInstanceType::IS_JS_RECEIVER);
        }
      }
      HCompareObjectEqAndBranch* result =
          New<HCompareObjectEqAndBranch>(left, right);
      return result;
    } else {
      if (combined_type->IsClass()) {
        // TODO(bmeurer): This is an optimized version of an x < y, x > y,
        // x <= y or x >= y, where both x and y are spec objects with the
        // same map. The CompareIC collects this map for us. So if we know
        // that there's no @@toPrimitive on the map (including the prototype
        // chain), and both valueOf and toString are the default initial
        // implementations (on the %ObjectPrototype%), then we can reduce
        // the comparison to map checks on x and y, because the comparison
        // will turn into a comparison of "[object CLASS]" to itself (the
        // default outcome of toString, since valueOf returns a spec object).
        // This is pretty much adhoc, so in TurboFan we could do a lot better
        // and inline the interesting parts of ToPrimitive (actually we could
        // even do that in Crankshaft but we don't want to waste too much
        // time on this now).
        DCHECK(Token::IsOrderedRelationalCompareOp(op));
        Handle<Map> map = combined_type->AsClass()->Map();
        PropertyAccessInfo value_of(this, LOAD, map,
                                    isolate()->factory()->valueOf_string());
        PropertyAccessInfo to_primitive(
            this, LOAD, map, isolate()->factory()->to_primitive_symbol());
        PropertyAccessInfo to_string(this, LOAD, map,
                                     isolate()->factory()->toString_string());
        PropertyAccessInfo to_string_tag(
            this, LOAD, map, isolate()->factory()->to_string_tag_symbol());
        if (to_primitive.CanAccessMonomorphic() && !to_primitive.IsFound() &&
            to_string_tag.CanAccessMonomorphic() &&
            (!to_string_tag.IsFound() || to_string_tag.IsData() ||
             to_string_tag.IsDataConstant()) &&
            value_of.CanAccessMonomorphic() && value_of.IsDataConstant() &&
            value_of.constant().is_identical_to(isolate()->object_value_of()) &&
            to_string.CanAccessMonomorphic() && to_string.IsDataConstant() &&
            to_string.constant().is_identical_to(
                isolate()->object_to_string())) {
          // We depend on the prototype chain to stay the same, because we
          // also need to deoptimize when someone installs @@toPrimitive
          // or @@toStringTag somewhere in the prototype chain.
          Handle<Object> prototype(map->prototype(), isolate());
          if (prototype->IsJSObject()) {
            BuildCheckPrototypeMaps(Handle<JSObject>::cast(prototype),
                                    Handle<JSObject>::null());
          }
          AddCheckMap(left, map);
          AddCheckMap(right, map);
          // The caller expects a branch instruction, so make it happy.
          return New<HBranch>(
              graph()->GetConstantBool(op == Token::LTE || op == Token::GTE));
        }
      }
      Bailout(kUnsupportedNonPrimitiveCompare);
      return NULL;
    }
  } else if (combined_type->Is(AstType::InternalizedString()) &&
             Token::IsEqualityOp(op)) {
    // If we have a constant argument, it should be consistent with the type
    // feedback (otherwise we fail assertions in HCompareObjectEqAndBranch).
    if ((left->IsConstant() &&
         !HConstant::cast(left)->HasInternalizedStringValue()) ||
        (right->IsConstant() &&
         !HConstant::cast(right)->HasInternalizedStringValue())) {
      Add<HDeoptimize>(
          DeoptimizeReason::kTypeMismatchBetweenFeedbackAndConstant,
          Deoptimizer::SOFT);
      // The caller expects a branch instruction, so make it happy.
      return New<HBranch>(graph()->GetConstantTrue());
    }
    BuildCheckHeapObject(left);
    Add<HCheckInstanceType>(left, HCheckInstanceType::IS_INTERNALIZED_STRING);
    BuildCheckHeapObject(right);
    Add<HCheckInstanceType>(right, HCheckInstanceType::IS_INTERNALIZED_STRING);
    HCompareObjectEqAndBranch* result =
        New<HCompareObjectEqAndBranch>(left, right);
    return result;
  } else if (combined_type->Is(AstType::String())) {
    BuildCheckHeapObject(left);
    Add<HCheckInstanceType>(left, HCheckInstanceType::IS_STRING);
    BuildCheckHeapObject(right);
    Add<HCheckInstanceType>(right, HCheckInstanceType::IS_STRING);
    HStringCompareAndBranch* result =
        New<HStringCompareAndBranch>(left, right, op);
    return result;
  } else if (combined_type->Is(AstType::Boolean())) {
    AddCheckMap(left, isolate()->factory()->boolean_map());
    AddCheckMap(right, isolate()->factory()->boolean_map());
    if (Token::IsEqualityOp(op)) {
      HCompareObjectEqAndBranch* result =
          New<HCompareObjectEqAndBranch>(left, right);
      return result;
    }
    left = Add<HLoadNamedField>(
        left, nullptr,
        HObjectAccess::ForOddballToNumber(Representation::Smi()));
    right = Add<HLoadNamedField>(
        right, nullptr,
        HObjectAccess::ForOddballToNumber(Representation::Smi()));
    HCompareNumericAndBranch* result =
        New<HCompareNumericAndBranch>(left, right, op);
    return result;
  } else {
    if (op == Token::EQ) {
      if (left->IsConstant() &&
          HConstant::cast(left)->GetInstanceType() == ODDBALL_TYPE &&
          HConstant::cast(left)->IsUndetectable()) {
        return New<HIsUndetectableAndBranch>(right);
      }

      if (right->IsConstant() &&
          HConstant::cast(right)->GetInstanceType() == ODDBALL_TYPE &&
          HConstant::cast(right)->IsUndetectable()) {
        return New<HIsUndetectableAndBranch>(left);
      }
    }

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
      return result;
    }
  }
}


void HOptimizedGraphBuilder::HandleLiteralCompareNil(CompareOperation* expr,
                                                     Expression* sub_expr,
                                                     NilValue nil) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  DCHECK(expr->op() == Token::EQ || expr->op() == Token::EQ_STRICT);
  if (!is_tracking_positions()) SetSourcePosition(expr->position());
  CHECK_ALIVE(VisitForValue(sub_expr));
  HValue* value = Pop();
  HControlInstruction* instr;
  if (expr->op() == Token::EQ_STRICT) {
    HConstant* nil_constant = nil == kNullValue
        ? graph()->GetConstantNull()
        : graph()->GetConstantUndefined();
    instr = New<HCompareObjectEqAndBranch>(value, nil_constant);
  } else {
    DCHECK_EQ(Token::EQ, expr->op());
    instr = New<HIsUndetectableAndBranch>(value);
  }
  return ast_context()->ReturnControl(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitSpread(Spread* expr) { UNREACHABLE(); }


void HOptimizedGraphBuilder::VisitEmptyParentheses(EmptyParentheses* expr) {
  UNREACHABLE();
}

void HOptimizedGraphBuilder::VisitGetIterator(GetIterator* expr) {
  UNREACHABLE();
}

void HOptimizedGraphBuilder::VisitImportCallExpression(
    ImportCallExpression* expr) {
  UNREACHABLE();
}

HValue* HOptimizedGraphBuilder::AddThisFunction() {
  return AddInstruction(BuildThisFunction());
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
  Handle<Map> initial_map(boilerplate_object->map());
  InstanceType instance_type = initial_map->instance_type();
  DCHECK(instance_type == JS_ARRAY_TYPE || instance_type == JS_OBJECT_TYPE);

  HType type = instance_type == JS_ARRAY_TYPE
      ? HType::JSArray() : HType::JSObject();
  HValue* object_size_constant = Add<HConstant>(initial_map->instance_size());

  PretenureFlag pretenure_flag = NOT_TENURED;
  Handle<AllocationSite> top_site(*site_context->top(), isolate());
  if (FLAG_allocation_site_pretenuring) {
    pretenure_flag = top_site->GetPretenureMode();
  }

  Handle<AllocationSite> current_site(*site_context->current(), isolate());
  if (*top_site == *current_site) {
    // We install a dependency for pretenuring only on the outermost literal.
    top_info()->dependencies()->AssumeTenuringDecision(top_site);
  }
  top_info()->dependencies()->AssumeTransitionStable(current_site);

  HInstruction* object =
      Add<HAllocate>(object_size_constant, type, pretenure_flag, instance_type,
                     graph()->GetConstant0(), top_site);

  // If allocation folding reaches kMaxRegularHeapObjectSize the
  // elements array may not get folded into the object. Hence, we set the
  // elements pointer to empty fixed array and let store elimination remove
  // this store in the folding case.
  HConstant* empty_fixed_array = Add<HConstant>(
      isolate()->factory()->empty_fixed_array());
  Add<HStoreNamedField>(object, HObjectAccess::ForElementsPointer(),
      empty_fixed_array);

  BuildEmitObjectHeader(boilerplate_object, object);

  // Similarly to the elements pointer, there is no guarantee that all
  // property allocations can get folded, so pre-initialize all in-object
  // properties to a safe value.
  BuildInitializeInobjectProperties(object, initial_map);

  // Copy in-object properties.
  if (initial_map->NumberOfFields() != 0 ||
      initial_map->unused_property_fields() > 0) {
    BuildEmitInObjectProperties(boilerplate_object, object, site_context,
                                pretenure_flag);
  }

  // Copy elements.
  Handle<FixedArrayBase> elements(boilerplate_object->elements());
  int elements_size = (elements->length() > 0 &&
      elements->map() != isolate()->heap()->fixed_cow_array_map()) ?
          elements->Size() : 0;

  if (pretenure_flag == TENURED &&
      elements->map() == isolate()->heap()->fixed_cow_array_map() &&
      isolate()->heap()->InNewSpace(*elements)) {
    // If we would like to pretenure a fixed cow array, we must ensure that the
    // array is already in old space, otherwise we'll create too many old-to-
    // new-space pointers (overflowing the store buffer).
    elements = Handle<FixedArrayBase>(
        isolate()->factory()->CopyAndTenureFixedCOWArray(
            Handle<FixedArray>::cast(elements)));
    boilerplate_object->set_elements(*elements);
  }

  HInstruction* object_elements = NULL;
  if (elements_size > 0) {
    HValue* object_elements_size = Add<HConstant>(elements_size);
    InstanceType instance_type = boilerplate_object->HasFastDoubleElements()
        ? FIXED_DOUBLE_ARRAY_TYPE : FIXED_ARRAY_TYPE;
    object_elements = Add<HAllocate>(object_elements_size, HType::HeapObject(),
                                     pretenure_flag, instance_type,
                                     graph()->GetConstant0(), top_site);
    BuildEmitElements(boilerplate_object, elements, object_elements,
                      site_context);
    Add<HStoreNamedField>(object, HObjectAccess::ForElementsPointer(),
                          object_elements);
  } else {
    Handle<Object> elements_field =
        Handle<Object>(boilerplate_object->elements(), isolate());
    HInstruction* object_elements_cow = Add<HConstant>(elements_field);
    Add<HStoreNamedField>(object, HObjectAccess::ForElementsPointer(),
                          object_elements_cow);
  }

  return object;
}


void HOptimizedGraphBuilder::BuildEmitObjectHeader(
    Handle<JSObject> boilerplate_object,
    HInstruction* object) {
  DCHECK(boilerplate_object->properties()->length() == 0);

  Handle<Map> boilerplate_object_map(boilerplate_object->map());
  AddStoreMapConstant(object, boilerplate_object_map);

  Handle<Object> properties_field =
      Handle<Object>(boilerplate_object->properties(), isolate());
  DCHECK(*properties_field == isolate()->heap()->empty_fixed_array());
  HInstruction* properties = Add<HConstant>(properties_field);
  HObjectAccess access = HObjectAccess::ForPropertiesPointer();
  Add<HStoreNamedField>(object, access, properties);

  if (boilerplate_object->IsJSArray()) {
    Handle<JSArray> boilerplate_array =
        Handle<JSArray>::cast(boilerplate_object);
    Handle<Object> length_field =
        Handle<Object>(boilerplate_array->length(), isolate());
    HInstruction* length = Add<HConstant>(length_field);

    DCHECK(boilerplate_array->length()->IsSmi());
    Add<HStoreNamedField>(object, HObjectAccess::ForArrayLength(
        boilerplate_array->GetElementsKind()), length);
  }
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
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    copied_fields++;
    FieldIndex field_index = FieldIndex::ForDescriptor(*boilerplate_map, i);


    int property_offset = field_index.offset();
    Handle<Name> name(descriptors->GetKey(i));

    // The access for the store depends on the type of the boilerplate.
    HObjectAccess access = boilerplate_object->IsJSArray() ?
        HObjectAccess::ForJSArrayOffset(property_offset) :
        HObjectAccess::ForMapAndOffset(boilerplate_map, property_offset);

    if (boilerplate_object->IsUnboxedDoubleField(field_index)) {
      CHECK(!boilerplate_object->IsJSArray());
      double value = boilerplate_object->RawFastDoublePropertyAt(field_index);
      access = access.WithRepresentation(Representation::Double());
      Add<HStoreNamedField>(object, access, Add<HConstant>(value));
      continue;
    }
    Handle<Object> value(boilerplate_object->RawFastPropertyAt(field_index),
                         isolate());

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
        HInstruction* double_box = Add<HAllocate>(
            heap_number_constant, HType::HeapObject(), pretenure_flag,
            MUTABLE_HEAP_NUMBER_TYPE, graph()->GetConstant0());
        AddStoreMapConstant(double_box,
            isolate()->factory()->mutable_heap_number_map());
        // Unwrap the mutable heap number from the boilerplate.
        HValue* double_value =
            Add<HConstant>(Handle<HeapNumber>::cast(value)->value());
        Add<HStoreNamedField>(
            double_box, HObjectAccess::ForHeapNumberValue(), double_value);
        value_instruction = double_box;
      } else if (representation.IsSmi()) {
        value_instruction = value->IsUninitialized(isolate())
                                ? graph()->GetConstant0()
                                : Add<HConstant>(value);
        // Ensure that value is stored as smi.
        access = access.WithRepresentation(representation);
      } else {
        value_instruction = Add<HConstant>(value);
      }

      Add<HStoreNamedField>(object, access, value_instruction);
    }
  }

  int inobject_properties = boilerplate_object->map()->GetInObjectProperties();
  HInstruction* value_instruction =
      Add<HConstant>(isolate()->factory()->one_pointer_filler_map());
  for (int i = copied_fields; i < inobject_properties; i++) {
    DCHECK(boilerplate_object->IsJSObject());
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
        Add<HLoadKeyed>(boilerplate_elements, key_constant, nullptr, nullptr,
                        kind, ALLOW_RETURN_HOLE);
    HInstruction* store = Add<HStoreKeyed>(object_elements, key_constant,
                                           value_instruction, nullptr, kind);
    store->SetFlag(HValue::kTruncatingToNumber);
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
      Add<HStoreKeyed>(object_elements, key_constant, result, nullptr, kind);
    } else {
      ElementsKind copy_kind =
          kind == FAST_HOLEY_SMI_ELEMENTS ? FAST_HOLEY_ELEMENTS : kind;
      HInstruction* value_instruction =
          Add<HLoadKeyed>(boilerplate_elements, key_constant, nullptr, nullptr,
                          copy_kind, ALLOW_RETURN_HOLE);
      Add<HStoreKeyed>(object_elements, key_constant, value_instruction,
                       nullptr, copy_kind);
    }
  }
}


void HOptimizedGraphBuilder::VisitThisFunction(ThisFunction* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  HInstruction* instr = BuildThisFunction();
  return ast_context()->ReturnInstruction(instr, expr->id());
}


void HOptimizedGraphBuilder::VisitSuperPropertyReference(
    SuperPropertyReference* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  return Bailout(kSuperReference);
}


void HOptimizedGraphBuilder::VisitSuperCallReference(SuperCallReference* expr) {
  DCHECK(!HasStackOverflow());
  DCHECK(current_block() != NULL);
  DCHECK(current_block()->HasPredecessor());
  return Bailout(kSuperReference);
}

void HOptimizedGraphBuilder::VisitDeclarations(
    Declaration::List* declarations) {
  DCHECK(globals_.is_empty());
  AstVisitor<HOptimizedGraphBuilder>::VisitDeclarations(declarations);
  if (!globals_.is_empty()) {
    Handle<FixedArray> array =
       isolate()->factory()->NewFixedArray(globals_.length(), TENURED);
    for (int i = 0; i < globals_.length(); ++i) array->set(i, *globals_.at(i));
    int flags = current_info()->GetDeclareGlobalsFlags();
    Handle<FeedbackVector> vector(current_feedback_vector(), isolate());
    Add<HDeclareGlobals>(array, flags, vector);
    globals_.Rewind(0);
  }
}


void HOptimizedGraphBuilder::VisitVariableDeclaration(
    VariableDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      DCHECK(!variable->binding_needs_init());
      globals_.Add(variable->name(), zone());
      FeedbackSlot slot = proxy->VariableFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals_.Add(handle(Smi::FromInt(slot.ToInt()), isolate()), zone());
      globals_.Add(isolate()->factory()->undefined_value(), zone());
      globals_.Add(isolate()->factory()->undefined_value(), zone());
      return;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
      if (variable->binding_needs_init()) {
        HValue* value = graph()->GetConstantHole();
        environment()->Bind(variable, value);
      }
      break;
    case VariableLocation::CONTEXT:
      if (variable->binding_needs_init()) {
        HValue* value = graph()->GetConstantHole();
        HValue* context = environment()->context();
        HStoreContextSlot* store = Add<HStoreContextSlot>(
            context, variable->index(), HStoreContextSlot::kNoCheck, value);
        if (store->HasObservableSideEffects()) {
          Add<HSimulate>(proxy->id(), REMOVABLE_SIMULATE);
        }
      }
      break;
    case VariableLocation::LOOKUP:
      return Bailout(kUnsupportedLookupSlotInDeclaration);
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void HOptimizedGraphBuilder::VisitFunctionDeclaration(
    FunctionDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      globals_.Add(variable->name(), zone());
      FeedbackSlot slot = proxy->VariableFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals_.Add(handle(Smi::FromInt(slot.ToInt()), isolate()), zone());

      // We need the slot where the literals array lives, too.
      slot = declaration->fun()->LiteralFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals_.Add(handle(Smi::FromInt(slot.ToInt()), isolate()), zone());

      Handle<SharedFunctionInfo> function = Compiler::GetSharedFunctionInfo(
          declaration->fun(), current_info()->script(), top_info());
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals_.Add(function, zone());
      return;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      CHECK_ALIVE(VisitForValue(declaration->fun()));
      HValue* value = Pop();
      BindIfLive(variable, value);
      break;
    }
    case VariableLocation::CONTEXT: {
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
    case VariableLocation::LOOKUP:
      return Bailout(kUnsupportedLookupSlotInDeclaration);
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void HOptimizedGraphBuilder::VisitRewritableExpression(
    RewritableExpression* node) {
  CHECK_ALIVE(Visit(node->expression()));
}


// Generators for inline runtime functions.
// Support for types.
void HOptimizedGraphBuilder::GenerateIsSmi(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HIsSmiAndBranch* result = New<HIsSmiAndBranch>(value);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsJSReceiver(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      New<HHasInstanceTypeAndBranch>(value,
                                     FIRST_JS_RECEIVER_TYPE,
                                     LAST_JS_RECEIVER_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}

void HOptimizedGraphBuilder::GenerateIsArray(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      New<HHasInstanceTypeAndBranch>(value, JS_ARRAY_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateIsTypedArray(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HHasInstanceTypeAndBranch* result =
      New<HHasInstanceTypeAndBranch>(value, JS_TYPED_ARRAY_TYPE);
  return ast_context()->ReturnControl(result, call->id());
}


void HOptimizedGraphBuilder::GenerateToInteger(CallRuntime* call) {
  DCHECK_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* input = Pop();
  if (input->type().IsSmi()) {
    return ast_context()->ReturnValue(input);
  } else {
    Callable callable = CodeFactory::ToInteger(isolate());
    HValue* stub = Add<HConstant>(callable.code());
    HValue* values[] = {input};
    HInstruction* result = New<HCallWithDescriptor>(
        stub, 0, callable.descriptor(), ArrayVector(values));
    return ast_context()->ReturnInstruction(result, call->id());
  }
}


void HOptimizedGraphBuilder::GenerateToObject(CallRuntime* call) {
  DCHECK_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HValue* result = BuildToObject(value);
  return ast_context()->ReturnValue(result);
}


void HOptimizedGraphBuilder::GenerateToString(CallRuntime* call) {
  DCHECK_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* input = Pop();
  if (input->type().IsString()) {
    return ast_context()->ReturnValue(input);
  } else {
    Callable callable = CodeFactory::ToString(isolate());
    HValue* stub = Add<HConstant>(callable.code());
    HValue* values[] = {input};
    HInstruction* result = New<HCallWithDescriptor>(
        stub, 0, callable.descriptor(), ArrayVector(values));
    return ast_context()->ReturnInstruction(result, call->id());
  }
}


void HOptimizedGraphBuilder::GenerateToLength(CallRuntime* call) {
  DCHECK_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  Callable callable = CodeFactory::ToLength(isolate());
  HValue* input = Pop();
  HValue* stub = Add<HConstant>(callable.code());
  HValue* values[] = {input};
  HInstruction* result = New<HCallWithDescriptor>(
      stub, 0, callable.descriptor(), ArrayVector(values));
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateToNumber(CallRuntime* call) {
  DCHECK_EQ(1, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  Callable callable = CodeFactory::ToNumber(isolate());
  HValue* input = Pop();
  HValue* result = BuildToNumber(input);
  if (result->HasObservableSideEffects()) {
    if (!ast_context()->IsEffect()) Push(result);
    Add<HSimulate>(call->id(), REMOVABLE_SIMULATE);
    if (!ast_context()->IsEffect()) result = Pop();
  }
  return ast_context()->ReturnValue(result);
}


void HOptimizedGraphBuilder::GenerateIsJSProxy(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* value = Pop();
  HIfContinuation continuation;
  IfBuilder if_proxy(this);

  HValue* smicheck = if_proxy.IfNot<HIsSmiAndBranch>(value);
  if_proxy.And();
  HValue* map = Add<HLoadNamedField>(value, smicheck, HObjectAccess::ForMap());
  HValue* instance_type =
      Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapInstanceType());
  if_proxy.If<HCompareNumericAndBranch>(
      instance_type, Add<HConstant>(JS_PROXY_TYPE), Token::EQ);

  if_proxy.CaptureContinuation(&continuation);
  return ast_context()->ReturnContinuation(&continuation, call->id());
}


void HOptimizedGraphBuilder::GenerateHasFastPackedElements(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* object = Pop();
  HIfContinuation continuation(graph()->CreateBasicBlock(),
                               graph()->CreateBasicBlock());
  IfBuilder if_not_smi(this);
  if_not_smi.IfNot<HIsSmiAndBranch>(object);
  if_not_smi.Then();
  {
    NoObservableSideEffectsScope no_effects(this);

    IfBuilder if_fast_packed(this);
    HValue* elements_kind = BuildGetElementsKind(object);
    if_fast_packed.If<HCompareNumericAndBranch>(
        elements_kind, Add<HConstant>(FAST_SMI_ELEMENTS), Token::EQ);
    if_fast_packed.Or();
    if_fast_packed.If<HCompareNumericAndBranch>(
        elements_kind, Add<HConstant>(FAST_ELEMENTS), Token::EQ);
    if_fast_packed.Or();
    if_fast_packed.If<HCompareNumericAndBranch>(
        elements_kind, Add<HConstant>(FAST_DOUBLE_ELEMENTS), Token::EQ);
    if_fast_packed.JoinContinuation(&continuation);
  }
  if_not_smi.JoinContinuation(&continuation);
  return ast_context()->ReturnContinuation(&continuation, call->id());
}


// Fast support for charCodeAt(n).
void HOptimizedGraphBuilder::GenerateStringCharCodeAt(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 2);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* index = Pop();
  HValue* string = Pop();
  HInstruction* result = BuildStringCharCodeAt(string, index);
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for SubString.
void HOptimizedGraphBuilder::GenerateSubString(CallRuntime* call) {
  DCHECK_EQ(3, call->arguments()->length());
  CHECK_ALIVE(VisitExpressions(call->arguments()));
  Callable callable = CodeFactory::SubString(isolate());
  HValue* stub = Add<HConstant>(callable.code());
  HValue* to = Pop();
  HValue* from = Pop();
  HValue* string = Pop();
  HValue* values[] = {string, from, to};
  HInstruction* result = New<HCallWithDescriptor>(
      stub, 0, callable.descriptor(), ArrayVector(values));
  result->set_type(HType::String());
  return ast_context()->ReturnInstruction(result, call->id());
}


// Fast support for calls.
void HOptimizedGraphBuilder::GenerateCall(CallRuntime* call) {
  DCHECK_LE(2, call->arguments()->length());
  CHECK_ALIVE(VisitExpressions(call->arguments()));
  CallTrampolineDescriptor descriptor(isolate());
  PushArgumentsFromEnvironment(call->arguments()->length() - 1);
  HValue* trampoline = Add<HConstant>(isolate()->builtins()->Call());
  HValue* target = Pop();
  HValue* values[] = {target, Add<HConstant>(call->arguments()->length() - 2)};
  HInstruction* result =
      New<HCallWithDescriptor>(trampoline, call->arguments()->length() - 1,
                               descriptor, ArrayVector(values));
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateFixedArrayGet(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 2);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* index = Pop();
  HValue* object = Pop();
  HInstruction* result = New<HLoadKeyed>(
      object, index, nullptr, nullptr, FAST_HOLEY_ELEMENTS, ALLOW_RETURN_HOLE);
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateFixedArraySet(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 3);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(2)));
  HValue* value = Pop();
  HValue* index = Pop();
  HValue* object = Pop();
  NoObservableSideEffectsScope no_effects(this);
  Add<HStoreKeyed>(object, index, value, nullptr, FAST_HOLEY_ELEMENTS);
  return ast_context()->ReturnValue(graph()->GetConstantUndefined());
}


void HOptimizedGraphBuilder::GenerateTheHole(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 0);
  return ast_context()->ReturnValue(graph()->GetConstantHole());
}


void HOptimizedGraphBuilder::GenerateCreateIterResultObject(CallRuntime* call) {
  DCHECK_EQ(2, call->arguments()->length());
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  CHECK_ALIVE(VisitForValue(call->arguments()->at(1)));
  HValue* done = Pop();
  HValue* value = Pop();
  HValue* result = BuildCreateIterResultObject(value, done);
  return ast_context()->ReturnValue(result);
}


void HOptimizedGraphBuilder::GenerateJSCollectionGetTable(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* receiver = Pop();
  HInstruction* result = New<HLoadNamedField>(
      receiver, nullptr, HObjectAccess::ForJSCollectionTable());
  return ast_context()->ReturnInstruction(result, call->id());
}


void HOptimizedGraphBuilder::GenerateStringGetRawHashField(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* object = Pop();
  HInstruction* result = New<HLoadNamedField>(
      object, nullptr, HObjectAccess::ForStringHashField());
  return ast_context()->ReturnInstruction(result, call->id());
}


template <typename CollectionType>
HValue* HOptimizedGraphBuilder::BuildAllocateOrderedHashTable() {
  static const int kCapacity = CollectionType::kMinCapacity;
  static const int kBucketCount = kCapacity / CollectionType::kLoadFactor;
  static const int kFixedArrayLength = CollectionType::kHashTableStartIndex +
                                       kBucketCount +
                                       (kCapacity * CollectionType::kEntrySize);
  static const int kSizeInBytes =
      FixedArray::kHeaderSize + (kFixedArrayLength * kPointerSize);

  // Allocate the table and add the proper map.
  HValue* table =
      Add<HAllocate>(Add<HConstant>(kSizeInBytes), HType::HeapObject(),
                     NOT_TENURED, FIXED_ARRAY_TYPE, graph()->GetConstant0());
  AddStoreMapConstant(table, isolate()->factory()->ordered_hash_table_map());

  // Initialize the FixedArray...
  HValue* length = Add<HConstant>(kFixedArrayLength);
  Add<HStoreNamedField>(table, HObjectAccess::ForFixedArrayLength(), length);

  // ...and the OrderedHashTable fields.
  Add<HStoreNamedField>(
      table,
      HObjectAccess::ForOrderedHashTableNumberOfBuckets<CollectionType>(),
      Add<HConstant>(kBucketCount));
  Add<HStoreNamedField>(
      table,
      HObjectAccess::ForOrderedHashTableNumberOfElements<CollectionType>(),
      graph()->GetConstant0());
  Add<HStoreNamedField>(
      table, HObjectAccess::ForOrderedHashTableNumberOfDeletedElements<
                 CollectionType>(),
      graph()->GetConstant0());

  // Fill the buckets with kNotFound.
  HValue* not_found = Add<HConstant>(CollectionType::kNotFound);
  for (int i = 0; i < kBucketCount; ++i) {
    Add<HStoreNamedField>(
        table, HObjectAccess::ForOrderedHashTableBucket<CollectionType>(i),
        not_found);
  }

  // Fill the data table with undefined.
  HValue* undefined = graph()->GetConstantUndefined();
  for (int i = 0; i < (kCapacity * CollectionType::kEntrySize); ++i) {
    Add<HStoreNamedField>(table,
                          HObjectAccess::ForOrderedHashTableDataTableIndex<
                              CollectionType, kBucketCount>(i),
                          undefined);
  }

  return table;
}


void HOptimizedGraphBuilder::GenerateSetInitialize(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* receiver = Pop();

  NoObservableSideEffectsScope no_effects(this);
  HValue* table = BuildAllocateOrderedHashTable<OrderedHashSet>();
  Add<HStoreNamedField>(receiver, HObjectAccess::ForJSCollectionTable(), table);
  return ast_context()->ReturnValue(receiver);
}


void HOptimizedGraphBuilder::GenerateMapInitialize(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* receiver = Pop();

  NoObservableSideEffectsScope no_effects(this);
  HValue* table = BuildAllocateOrderedHashTable<OrderedHashMap>();
  Add<HStoreNamedField>(receiver, HObjectAccess::ForJSCollectionTable(), table);
  return ast_context()->ReturnValue(receiver);
}


template <typename CollectionType>
void HOptimizedGraphBuilder::BuildOrderedHashTableClear(HValue* receiver) {
  HValue* old_table = Add<HLoadNamedField>(
      receiver, nullptr, HObjectAccess::ForJSCollectionTable());
  HValue* new_table = BuildAllocateOrderedHashTable<CollectionType>();
  Add<HStoreNamedField>(
      old_table, HObjectAccess::ForOrderedHashTableNextTable<CollectionType>(),
      new_table);
  Add<HStoreNamedField>(
      old_table, HObjectAccess::ForOrderedHashTableNumberOfDeletedElements<
                     CollectionType>(),
      Add<HConstant>(CollectionType::kClearedTableSentinel));
  Add<HStoreNamedField>(receiver, HObjectAccess::ForJSCollectionTable(),
                        new_table);
}


void HOptimizedGraphBuilder::GenerateSetClear(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* receiver = Pop();

  NoObservableSideEffectsScope no_effects(this);
  BuildOrderedHashTableClear<OrderedHashSet>(receiver);
  return ast_context()->ReturnValue(graph()->GetConstantUndefined());
}


void HOptimizedGraphBuilder::GenerateMapClear(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 1);
  CHECK_ALIVE(VisitForValue(call->arguments()->at(0)));
  HValue* receiver = Pop();

  NoObservableSideEffectsScope no_effects(this);
  BuildOrderedHashTableClear<OrderedHashMap>(receiver);
  return ast_context()->ReturnValue(graph()->GetConstantUndefined());
}

void HOptimizedGraphBuilder::GenerateDebugBreakInOptimizedCode(
    CallRuntime* call) {
  Add<HDebugBreak>();
  return ast_context()->ReturnValue(graph()->GetConstant0());
}


void HOptimizedGraphBuilder::GenerateDebugIsActive(CallRuntime* call) {
  DCHECK(call->arguments()->length() == 0);
  HValue* ref =
      Add<HConstant>(ExternalReference::debug_is_active_address(isolate()));
  HValue* value =
      Add<HLoadNamedField>(ref, nullptr, HObjectAccess::ForExternalUInteger8());
  return ast_context()->ReturnValue(value);
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
  DeclarationScope* declaration_scope = scope->GetDeclarationScope();
  Initialize(declaration_scope->num_parameters() + 1,
             declaration_scope->num_stack_slots(), 0);
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
  DCHECK(!block->IsLoopHeader());
  DCHECK(values_.length() == other->values_.length());

  int length = values_.length();
  for (int i = 0; i < length; ++i) {
    HValue* value = values_[i];
    if (value != NULL && value->IsPhi() && value->block() == block) {
      // There is already a phi for the i'th value.
      HPhi* phi = HPhi::cast(value);
      // Assert index is correct and that we haven't missed an incoming edge.
      DCHECK(phi->merged_index() == i || !phi->HasMergedIndex());
      DCHECK(phi->OperandCount() == block->predecessors()->length());
      phi->AddInput(other->values_[i]);
    } else if (values_[i] != other->values_[i]) {
      // There is a fresh value on the incoming edge, a phi is needed.
      DCHECK(values_[i] != NULL && other->values_[i] != NULL);
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
  DCHECK(value != NULL);
  assigned_variables_.Add(index, zone());
  values_[index] = value;
}


bool HEnvironment::HasExpressionAt(int index) const {
  return index >= parameter_count_ + specials_count_ + local_count_;
}


bool HEnvironment::ExpressionStackIsEmpty() const {
  DCHECK(length() >= first_expression_index());
  return length() == first_expression_index();
}


void HEnvironment::SetExpressionStackAt(int index_from_top, HValue* value) {
  int count = index_from_top + 1;
  int index = values_.length() - count;
  DCHECK(HasExpressionAt(index));
  // The push count must include at least the element in question or else
  // the new value will not be included in this environment's history.
  if (push_count_ < count) {
    // This is the same effect as popping then re-pushing 'count' elements.
    pop_count_ += (count - push_count_);
    push_count_ = count;
  }
  values_[index] = value;
}


HValue* HEnvironment::RemoveExpressionStackAt(int index_from_top) {
  int count = index_from_top + 1;
  int index = values_.length() - count;
  DCHECK(HasExpressionAt(index));
  // Simulate popping 'count' elements and then
  // pushing 'count - 1' elements back.
  pop_count_ += Max(count - push_count_, 0);
  push_count_ = Max(push_count_ - count, 0) + (count - 1);
  return values_.Remove(index);
}


void HEnvironment::Drop(int count) {
  for (int i = 0; i < count; ++i) {
    Pop();
  }
}


void HEnvironment::Print() const {
  OFStream os(stdout);
  os << *this << "\n";
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

void HEnvironment::MarkAsTailCaller() {
  DCHECK_EQ(JS_FUNCTION, frame_type());
  frame_type_ = TAIL_CALLER_FUNCTION;
}

void HEnvironment::ClearTailCallerMark() {
  DCHECK_EQ(TAIL_CALLER_FUNCTION, frame_type());
  frame_type_ = JS_FUNCTION;
}

HEnvironment* HEnvironment::CopyForInlining(
    Handle<JSFunction> target, int arguments, FunctionLiteral* function,
    HConstant* undefined, InliningKind inlining_kind,
    TailCallMode syntactic_tail_call_mode) const {
  DCHECK_EQ(JS_FUNCTION, frame_type());

  // Outer environment is a copy of this one without the arguments.
  int arity = function->scope()->num_parameters();

  HEnvironment* outer = Copy();
  outer->Drop(arguments + 1);  // Including receiver.
  outer->ClearHistory();

  if (syntactic_tail_call_mode == TailCallMode::kAllow) {
    DCHECK_EQ(NORMAL_RETURN, inlining_kind);
    outer->MarkAsTailCaller();
  }

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


std::ostream& operator<<(std::ostream& os, const HEnvironment& env) {
  for (int i = 0; i < env.length(); i++) {
    if (i == 0) os << "parameters\n";
    if (i == env.parameter_count()) os << "specials\n";
    if (i == env.parameter_count() + env.specials_count()) os << "locals\n";
    if (i == env.parameter_count() + env.specials_count() + env.local_count()) {
      os << "expressions\n";
    }
    HValue* val = env.values()->at(i);
    os << i << ": ";
    if (val != NULL) {
      os << val;
    } else {
      os << "NULL";
    }
    os << "\n";
  }
  return os << "\n";
}


void HTracer::TraceCompilation(CompilationInfo* info) {
  Tag tag(this, "compilation");
  std::string name;
  if (info->parse_info()) {
    Object* source_name = info->script()->name();
    if (source_name->IsString()) {
      String* str = String::cast(source_name);
      if (str->length() > 0) {
        name.append(str->ToCString().get());
        name.append(":");
      }
    }
  }
  std::unique_ptr<char[]> method_name = info->GetDebugName();
  name.append(method_name.get());
  if (info->IsOptimizing()) {
    PrintStringProperty("name", name.c_str());
    PrintIndent();
    trace_.Add("method \"%s:%d\"\n", method_name.get(),
               info->optimization_id());
  } else {
    PrintStringProperty("name", name.c_str());
    PrintStringProperty("method", "stub");
  }
  PrintLongProperty("date",
                    static_cast<int64_t>(base::OS::TimeCurrentMillis()));
}


void HTracer::TraceLithium(const char* name, LChunk* chunk) {
  DCHECK(!chunk->isolate()->concurrent_recompilation_enabled());
  AllowHandleDereference allow_deref;
  AllowDeferredHandleDereference allow_deferred_deref;
  Trace(name, chunk->graph(), chunk);
}


void HTracer::TraceHydrogen(const char* name, HGraph* graph) {
  DCHECK(!graph->isolate()->concurrent_recompilation_enabled());
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
        std::ostringstream os;
        os << phi->merged_index() << " " << NameOf(phi) << " " << *phi << "\n";
        trace_.Add(os.str().c_str());
      }
    }

    {
      Tag HIR_tag(this, "HIR");
      for (HInstructionIterator it(current); !it.Done(); it.Advance()) {
        HInstruction* instruction = it.Current();
        int uses = instruction->UseCount();
        PrintIndent();
        std::ostringstream os;
        os << "0 " << uses << " " << NameOf(instruction) << " " << *instruction;
        if (instruction->has_position()) {
          const SourcePosition pos = instruction->position();
          os << " pos:";
          if (pos.isInlined()) os << "inlining(" << pos.InliningId() << "),";
          os << pos.ScriptOffset();
        }
        os << " <|@\n";
        trace_.Add(os.str().c_str());
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
            std::ostringstream os;
            os << " [hir:" << NameOf(linstr->hydrogen_value()) << "] <|@\n";
            trace_.Add(os.str().c_str());
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
                   GetRegConfig()->GetDoubleRegisterName(assigned_reg));
      } else {
        DCHECK(op->IsRegister());
        trace_.Add(" \"%s\"",
                   GetRegConfig()->GetGeneralRegisterName(assigned_reg));
      }
    } else if (range->IsSpilled()) {
      LOperand* op = range->TopLevel()->GetSpillOperand();
      if (op->IsDoubleStackSlot()) {
        trace_.Add(" \"double_stack:%d\"", op->index());
      } else {
        DCHECK(op->IsStackSlot());
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
  if (!info->has_shared_info()) return;
  source_size_ += info->shared_info()->SourceSize();
}


void HStatistics::Print() {
  PrintF(
      "\n"
      "----------------------------------------"
      "----------------------------------------\n"
      "--- Hydrogen timing results:\n"
      "----------------------------------------"
      "----------------------------------------\n");
  base::TimeDelta sum;
  for (int i = 0; i < times_.length(); ++i) {
    sum += times_[i];
  }

  for (int i = 0; i < names_.length(); ++i) {
    PrintF("%33s", names_[i]);
    double ms = times_[i].InMillisecondsF();
    double percent = times_[i].PercentOf(sum);
    PrintF(" %8.3f ms / %4.1f %% ", ms, percent);

    size_t size = sizes_[i];
    double size_percent = static_cast<double>(size) * 100 / total_size_;
    PrintF(" %9zu bytes / %4.1f %%\n", size, size_percent);
  }

  PrintF(
      "----------------------------------------"
      "----------------------------------------\n");
  base::TimeDelta total = create_graph_ + optimize_graph_ + generate_code_;
  PrintF("%33s %8.3f ms / %4.1f %% \n", "Create graph",
         create_graph_.InMillisecondsF(), create_graph_.PercentOf(total));
  PrintF("%33s %8.3f ms / %4.1f %% \n", "Optimize graph",
         optimize_graph_.InMillisecondsF(), optimize_graph_.PercentOf(total));
  PrintF("%33s %8.3f ms / %4.1f %% \n", "Generate and install code",
         generate_code_.InMillisecondsF(), generate_code_.PercentOf(total));
  PrintF(
      "----------------------------------------"
      "----------------------------------------\n");
  PrintF("%33s %8.3f ms           %9zu bytes\n", "Total",
         total.InMillisecondsF(), total_size_);
  PrintF("%33s     (%.1f times slower than full code gen)\n", "",
         total.TimesOf(full_code_gen_));

  double source_size_in_kb = static_cast<double>(source_size_) / 1024;
  double normalized_time =  source_size_in_kb > 0
      ? total.InMillisecondsF() / source_size_in_kb
      : 0;
  double normalized_size_in_kb =
      source_size_in_kb > 0
          ? static_cast<double>(total_size_) / 1024 / source_size_in_kb
          : 0;
  PrintF("%33s %8.3f ms           %7.3f kB allocated\n",
         "Average per kB source", normalized_time, normalized_size_in_kb);
}


void HStatistics::SaveTiming(const char* name, base::TimeDelta time,
                             size_t size) {
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

}  // namespace internal
}  // namespace v8
