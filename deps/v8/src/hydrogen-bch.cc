// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hydrogen-bch.h"

namespace v8 {
namespace internal {

/*
 * This class is a table with one element for eack basic block.
 *
 * It is used to check if, inside one loop, all execution paths contain
 * a bounds check for a particular [index, length] combination.
 * The reason is that if there is a path that stays in the loop without
 * executing a check then the check cannot be hoisted out of the loop (it
 * would likely fail and cause a deopt for no good reason).
 * We also check is there are paths that exit the loop early, and if yes we
 * perform the hoisting only if graph()->use_optimistic_licm() is true.
 * The reason is that such paths are realtively common and harmless (like in
 * a "search" method that scans an array until an element is found), but in
 * some cases they could cause a deopt if we hoist the check so this is a
 * situation we need to detect.
 */
class InductionVariableBlocksTable BASE_EMBEDDED {
 public:
  class Element {
   public:
    static const int kNoBlock = -1;

    HBasicBlock* block() { return block_; }
    void set_block(HBasicBlock* block) { block_ = block; }
    bool is_start() { return is_start_; }
    bool is_proper_exit() { return is_proper_exit_; }
    bool is_in_loop() { return is_in_loop_; }
    bool has_check() { return has_check_; }
    void set_has_check() { has_check_ = true; }
    InductionVariableLimitUpdate* additional_limit() {
      return &additional_limit_;
    }

    /*
     * Initializes the table element for a given loop (identified by its
     * induction variable).
     */
    void InitializeLoop(InductionVariableData* data) {
      ASSERT(data->limit() != NULL);
      HLoopInformation* loop = data->phi()->block()->current_loop();
      is_start_ = (block() == loop->loop_header());
      is_proper_exit_ = (block() == data->induction_exit_target());
      is_in_loop_ = loop->IsNestedInThisLoop(block()->current_loop());
      has_check_ = false;
    }

    // Utility methods to iterate over dominated blocks.
    void ResetCurrentDominatedBlock() { current_dominated_block_ = kNoBlock; }
    HBasicBlock* CurrentDominatedBlock() {
      ASSERT(current_dominated_block_ != kNoBlock);
      return current_dominated_block_ < block()->dominated_blocks()->length() ?
          block()->dominated_blocks()->at(current_dominated_block_) : NULL;
    }
    HBasicBlock* NextDominatedBlock() {
      current_dominated_block_++;
      return CurrentDominatedBlock();
    }

    Element()
        : block_(NULL), is_start_(false), is_proper_exit_(false),
          has_check_(false), additional_limit_(),
          current_dominated_block_(kNoBlock) {}

   private:
    HBasicBlock* block_;
    bool is_start_;
    bool is_proper_exit_;
    bool is_in_loop_;
    bool has_check_;
    InductionVariableLimitUpdate additional_limit_;
    int current_dominated_block_;
  };

  HGraph* graph() const { return graph_; }
  Counters* counters() const { return graph()->isolate()->counters(); }
  HBasicBlock* loop_header() const { return loop_header_; }
  Element* at(int index) const { return &(elements_.at(index)); }
  Element* at(HBasicBlock* block) const { return at(block->block_id()); }

  void AddCheckAt(HBasicBlock* block) {
    at(block->block_id())->set_has_check();
  }

  /*
   * Initializes the table for a given loop (identified by its induction
   * variable).
   */
  void InitializeLoop(InductionVariableData* data) {
    for (int i = 0; i < graph()->blocks()->length(); i++) {
      at(i)->InitializeLoop(data);
    }
    loop_header_ = data->phi()->block()->current_loop()->loop_header();
  }


  enum Hoistability {
    HOISTABLE,
    OPTIMISTICALLY_HOISTABLE,
    NOT_HOISTABLE
  };

  /*
   * This method checks if it is appropriate to hoist the bounds checks on an
   * induction variable out of the loop.
   * The problem is that in the loop code graph there could be execution paths
   * where the check is not performed, but hoisting the check has the same
   * semantics as performing it at every loop iteration, which could cause
   * unnecessary check failures (which would mean unnecessary deoptimizations).
   * The method returns OK if there are no paths that perform an iteration
   * (loop back to the header) without meeting a check, or UNSAFE is set if
   * early exit paths are found.
   */
  Hoistability CheckHoistability() {
    for (int i = 0; i < elements_.length(); i++) {
      at(i)->ResetCurrentDominatedBlock();
    }
    bool unsafe = false;

    HBasicBlock* current = loop_header();
    while (current != NULL) {
      HBasicBlock* next;

      if (at(current)->has_check() || !at(current)->is_in_loop()) {
        // We found a check or we reached a dominated block out of the loop,
        // therefore this block is safe and we can backtrack.
        next = NULL;
      } else {
        for (int i = 0; i < current->end()->SuccessorCount(); i ++) {
          Element* successor = at(current->end()->SuccessorAt(i));

          if (!successor->is_in_loop()) {
            if (!successor->is_proper_exit()) {
              // We found a path that exits the loop early, and is not the exit
              // related to the induction limit, therefore hoisting checks is
              // an optimistic assumption.
              unsafe = true;
            }
          }

          if (successor->is_start()) {
            // We found a path that does one loop iteration without meeting any
            // check, therefore hoisting checks would be likely to cause
            // unnecessary deopts.
            return NOT_HOISTABLE;
          }
        }

        next = at(current)->NextDominatedBlock();
      }

      // If we have no next block we need to backtrack the tree traversal.
      while (next == NULL) {
        current = current->dominator();
        if (current != NULL) {
          next = at(current)->NextDominatedBlock();
        } else {
          // We reached the root: next stays NULL.
          next = NULL;
          break;
        }
      }

      current = next;
    }

    return unsafe ? OPTIMISTICALLY_HOISTABLE : HOISTABLE;
  }

  explicit InductionVariableBlocksTable(HGraph* graph)
    : graph_(graph), loop_header_(NULL),
      elements_(graph->blocks()->length(), graph->zone()) {
    for (int i = 0; i < graph->blocks()->length(); i++) {
      Element element;
      element.set_block(graph->blocks()->at(i));
      elements_.Add(element, graph->zone());
      ASSERT(at(i)->block()->block_id() == i);
    }
  }

  // Tries to hoist a check out of its induction loop.
  void ProcessRelatedChecks(
      InductionVariableData::InductionVariableCheck* check,
      InductionVariableData* data) {
    HValue* length = check->check()->length();
    check->set_processed();
    HBasicBlock* header =
        data->phi()->block()->current_loop()->loop_header();
    HBasicBlock* pre_header = header->predecessors()->at(0);
    // Check that the limit is defined in the loop preheader.
    if (!data->limit()->IsInteger32Constant()) {
      HBasicBlock* limit_block = data->limit()->block();
      if (limit_block != pre_header &&
          !limit_block->Dominates(pre_header)) {
        return;
      }
    }
    // Check that the length and limit have compatible representations.
    if (!(data->limit()->representation().Equals(
            length->representation()) ||
        data->limit()->IsInteger32Constant())) {
      return;
    }
    // Check that the length is defined in the loop preheader.
    if (check->check()->length()->block() != pre_header &&
        !check->check()->length()->block()->Dominates(pre_header)) {
      return;
    }

    // Add checks to the table.
    for (InductionVariableData::InductionVariableCheck* current_check = check;
         current_check != NULL;
         current_check = current_check->next()) {
      if (current_check->check()->length() != length) continue;

      AddCheckAt(current_check->check()->block());
      current_check->set_processed();
    }

    // Check that we will not cause unwanted deoptimizations.
    Hoistability hoistability = CheckHoistability();
    if (hoistability == NOT_HOISTABLE ||
        (hoistability == OPTIMISTICALLY_HOISTABLE &&
         !graph()->use_optimistic_licm())) {
      return;
    }

    // We will do the hoisting, but we must see if the limit is "limit" or if
    // all checks are done on constants: if all check are done against the same
    // constant limit we will use that instead of the induction limit.
    bool has_upper_constant_limit = true;
    int32_t upper_constant_limit =
        check != NULL && check->HasUpperLimit() ? check->upper_limit() : 0;
    for (InductionVariableData::InductionVariableCheck* current_check = check;
         current_check != NULL;
         current_check = current_check->next()) {
      has_upper_constant_limit =
          has_upper_constant_limit &&
          check->HasUpperLimit() &&
          check->upper_limit() == upper_constant_limit;
      counters()->bounds_checks_eliminated()->Increment();
      current_check->check()->set_skip_check();
    }

    // Choose the appropriate limit.
    Zone* zone = graph()->zone();
    HValue* context = graph()->GetInvalidContext();
    HValue* limit = data->limit();
    if (has_upper_constant_limit) {
      HConstant* new_limit = HConstant::New(zone, context,
                                            upper_constant_limit);
      new_limit->InsertBefore(pre_header->end());
      limit = new_limit;
    }

    // If necessary, redefine the limit in the preheader.
    if (limit->IsInteger32Constant() &&
        limit->block() != pre_header &&
        !limit->block()->Dominates(pre_header)) {
      HConstant* new_limit = HConstant::New(zone, context,
                                            limit->GetInteger32Constant());
      new_limit->InsertBefore(pre_header->end());
      limit = new_limit;
    }

    // Do the hoisting.
    HBoundsCheck* hoisted_check = HBoundsCheck::New(
        zone, context, limit, check->check()->length());
    hoisted_check->InsertBefore(pre_header->end());
    hoisted_check->set_allow_equality(true);
    counters()->bounds_checks_hoisted()->Increment();
  }

  void CollectInductionVariableData(HBasicBlock* bb) {
    bool additional_limit = false;

    for (int i = 0; i < bb->phis()->length(); i++) {
      HPhi* phi = bb->phis()->at(i);
      phi->DetectInductionVariable();
    }

    additional_limit = InductionVariableData::ComputeInductionVariableLimit(
        bb, at(bb)->additional_limit());

    if (additional_limit) {
      at(bb)->additional_limit()->updated_variable->
          UpdateAdditionalLimit(at(bb)->additional_limit());
    }

    for (HInstruction* i = bb->first(); i != NULL; i = i->next()) {
      if (!i->IsBoundsCheck()) continue;
      HBoundsCheck* check = HBoundsCheck::cast(i);
      InductionVariableData::BitwiseDecompositionResult decomposition;
      InductionVariableData::DecomposeBitwise(check->index(), &decomposition);
      if (!decomposition.base->IsPhi()) continue;
      HPhi* phi = HPhi::cast(decomposition.base);

      if (!phi->IsInductionVariable()) continue;
      InductionVariableData* data = phi->induction_variable_data();

      // For now ignore loops decrementing the index.
      if (data->increment() <= 0) continue;
      if (!data->LowerLimitIsNonNegativeConstant()) continue;

      // TODO(mmassi): skip OSR values for check->length().
      if (check->length() == data->limit() ||
          check->length() == data->additional_upper_limit()) {
        counters()->bounds_checks_eliminated()->Increment();
        check->set_skip_check();
        continue;
      }

      if (!phi->IsLimitedInductionVariable()) continue;

      int32_t limit = data->ComputeUpperLimit(decomposition.and_mask,
                                              decomposition.or_mask);
      phi->induction_variable_data()->AddCheck(check, limit);
    }

    for (int i = 0; i < bb->dominated_blocks()->length(); i++) {
      CollectInductionVariableData(bb->dominated_blocks()->at(i));
    }

    if (additional_limit) {
      at(bb->block_id())->additional_limit()->updated_variable->
          UpdateAdditionalLimit(at(bb->block_id())->additional_limit());
    }
  }

  void EliminateRedundantBoundsChecks(HBasicBlock* bb) {
    for (int i = 0; i < bb->phis()->length(); i++) {
      HPhi* phi = bb->phis()->at(i);
      if (!phi->IsLimitedInductionVariable()) continue;

      InductionVariableData* induction_data = phi->induction_variable_data();
      InductionVariableData::ChecksRelatedToLength* current_length_group =
          induction_data->checks();
      while (current_length_group != NULL) {
        current_length_group->CloseCurrentBlock();
        InductionVariableData::InductionVariableCheck* current_base_check =
            current_length_group->checks();
        InitializeLoop(induction_data);

        while (current_base_check != NULL) {
          ProcessRelatedChecks(current_base_check, induction_data);
          while (current_base_check != NULL &&
                 current_base_check->processed()) {
            current_base_check = current_base_check->next();
          }
        }

        current_length_group = current_length_group->next();
      }
    }
  }

 private:
  HGraph* graph_;
  HBasicBlock* loop_header_;
  ZoneList<Element> elements_;
};


void HBoundsCheckHoistingPhase::HoistRedundantBoundsChecks() {
  InductionVariableBlocksTable table(graph());
  table.CollectInductionVariableData(graph()->entry_block());
  for (int i = 0; i < graph()->blocks()->length(); i++) {
    table.EliminateRedundantBoundsChecks(graph()->blocks()->at(i));
  }
}

} }  // namespace v8::internal
