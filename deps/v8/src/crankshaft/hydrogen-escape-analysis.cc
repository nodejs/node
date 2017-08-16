// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-escape-analysis.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {


bool HEscapeAnalysisPhase::HasNoEscapingUses(HValue* value, int size) {
  for (HUseIterator it(value->uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();
    if (use->HasEscapingOperandAt(it.index())) {
      if (FLAG_trace_escape_analysis) {
        PrintF("#%d (%s) escapes through #%d (%s) @%d\n", value->id(),
               value->Mnemonic(), use->id(), use->Mnemonic(), it.index());
      }
      return false;
    }
    if (use->HasOutOfBoundsAccess(size)) {
      if (FLAG_trace_escape_analysis) {
        PrintF("#%d (%s) out of bounds at #%d (%s) @%d\n", value->id(),
               value->Mnemonic(), use->id(), use->Mnemonic(), it.index());
      }
      return false;
    }
    int redefined_index = use->RedefinedOperandIndex();
    if (redefined_index == it.index() && !HasNoEscapingUses(use, size)) {
      if (FLAG_trace_escape_analysis) {
        PrintF("#%d (%s) escapes redefinition #%d (%s) @%d\n", value->id(),
               value->Mnemonic(), use->id(), use->Mnemonic(), it.index());
      }
      return false;
    }
  }
  return true;
}


void HEscapeAnalysisPhase::CollectCapturedValues() {
  int block_count = graph()->blocks()->length();
  for (int i = 0; i < block_count; ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (!instr->IsAllocate()) continue;
      HAllocate* allocate = HAllocate::cast(instr);
      if (!allocate->size()->IsInteger32Constant()) continue;
      int size_in_bytes = allocate->size()->GetInteger32Constant();
      if (HasNoEscapingUses(instr, size_in_bytes)) {
        if (FLAG_trace_escape_analysis) {
          PrintF("#%d (%s) is being captured\n", instr->id(),
                 instr->Mnemonic());
        }
        captured_.Add(instr, zone());
      }
    }
  }
}


HCapturedObject* HEscapeAnalysisPhase::NewState(HInstruction* previous) {
  Zone* zone = graph()->zone();
  HCapturedObject* state =
      new(zone) HCapturedObject(number_of_values_, number_of_objects_, zone);
  state->InsertAfter(previous);
  return state;
}


// Create a new state for replacing HAllocate instructions.
HCapturedObject* HEscapeAnalysisPhase::NewStateForAllocation(
    HInstruction* previous) {
  HConstant* undefined = graph()->GetConstantUndefined();
  HCapturedObject* state = NewState(previous);
  for (int index = 0; index < number_of_values_; index++) {
    state->SetOperandAt(index, undefined);
  }
  return state;
}


// Create a new state full of phis for loop header entries.
HCapturedObject* HEscapeAnalysisPhase::NewStateForLoopHeader(
    HInstruction* previous,
    HCapturedObject* old_state) {
  HBasicBlock* block = previous->block();
  HCapturedObject* state = NewState(previous);
  for (int index = 0; index < number_of_values_; index++) {
    HValue* operand = old_state->OperandAt(index);
    HPhi* phi = NewPhiAndInsert(block, operand, index);
    state->SetOperandAt(index, phi);
  }
  return state;
}


// Create a new state by copying an existing one.
HCapturedObject* HEscapeAnalysisPhase::NewStateCopy(
    HInstruction* previous,
    HCapturedObject* old_state) {
  HCapturedObject* state = NewState(previous);
  for (int index = 0; index < number_of_values_; index++) {
    HValue* operand = old_state->OperandAt(index);
    state->SetOperandAt(index, operand);
  }
  return state;
}


// Insert a newly created phi into the given block and fill all incoming
// edges with the given value.
HPhi* HEscapeAnalysisPhase::NewPhiAndInsert(HBasicBlock* block,
                                            HValue* incoming_value,
                                            int index) {
  Zone* zone = graph()->zone();
  HPhi* phi = new(zone) HPhi(HPhi::kInvalidMergedIndex, zone);
  for (int i = 0; i < block->predecessors()->length(); i++) {
    phi->AddInput(incoming_value);
  }
  block->AddPhi(phi);
  return phi;
}


// Insert a newly created value check as a replacement for map checks.
HValue* HEscapeAnalysisPhase::NewMapCheckAndInsert(HCapturedObject* state,
                                                   HCheckMaps* mapcheck) {
  Zone* zone = graph()->zone();
  HValue* value = state->map_value();
  // TODO(mstarzinger): This will narrow a map check against a set of maps
  // down to the first element in the set. Revisit and fix this.
  HCheckValue* check = HCheckValue::New(graph()->isolate(), zone, NULL, value,
                                        mapcheck->maps()->at(0), false);
  check->InsertBefore(mapcheck);
  return check;
}


// Replace a field load with a given value, forcing Smi representation if
// necessary.
HValue* HEscapeAnalysisPhase::NewLoadReplacement(
    HLoadNamedField* load, HValue* load_value) {
  HValue* replacement = load_value;
  Representation representation = load->representation();
  if (representation.IsSmiOrInteger32() || representation.IsDouble()) {
    Zone* zone = graph()->zone();
    HInstruction* new_instr = HForceRepresentation::New(
        graph()->isolate(), zone, NULL, load_value, representation);
    new_instr->InsertAfter(load);
    replacement = new_instr;
  }
  return replacement;
}


// Performs a forward data-flow analysis of all loads and stores on the
// given captured allocation. This uses a reverse post-order iteration
// over affected basic blocks. All non-escaping instructions are handled
// and replaced during the analysis.
void HEscapeAnalysisPhase::AnalyzeDataFlow(HInstruction* allocate) {
  HBasicBlock* allocate_block = allocate->block();
  block_states_.AddBlock(NULL, graph()->blocks()->length(), zone());

  // Iterate all blocks starting with the allocation block, since the
  // allocation cannot dominate blocks that come before.
  int start = allocate_block->block_id();
  for (int i = start; i < graph()->blocks()->length(); i++) {
    HBasicBlock* block = graph()->blocks()->at(i);
    HCapturedObject* state = StateAt(block);

    // Skip blocks that are not dominated by the captured allocation.
    if (!allocate_block->Dominates(block) && allocate_block != block) continue;
    if (FLAG_trace_escape_analysis) {
      PrintF("Analyzing data-flow in B%d\n", block->block_id());
    }

    // Go through all instructions of the current block.
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      switch (instr->opcode()) {
        case HValue::kAllocate: {
          if (instr != allocate) continue;
          state = NewStateForAllocation(allocate);
          break;
        }
        case HValue::kLoadNamedField: {
          HLoadNamedField* load = HLoadNamedField::cast(instr);
          int index = load->access().offset() / kPointerSize;
          if (load->object() != allocate) continue;
          DCHECK(load->access().IsInobject());
          HValue* replacement =
            NewLoadReplacement(load, state->OperandAt(index));
          load->DeleteAndReplaceWith(replacement);
          if (FLAG_trace_escape_analysis) {
            PrintF("Replacing load #%d with #%d (%s)\n", load->id(),
                   replacement->id(), replacement->Mnemonic());
          }
          break;
        }
        case HValue::kStoreNamedField: {
          HStoreNamedField* store = HStoreNamedField::cast(instr);
          int index = store->access().offset() / kPointerSize;
          if (store->object() != allocate) continue;
          DCHECK(store->access().IsInobject());
          state = NewStateCopy(store->previous(), state);
          state->SetOperandAt(index, store->value());
          if (store->has_transition()) {
            state->SetOperandAt(0, store->transition());
          }
          if (store->HasObservableSideEffects()) {
            state->ReuseSideEffectsFromStore(store);
          }
          store->DeleteAndReplaceWith(store->ActualValue());
          if (FLAG_trace_escape_analysis) {
            PrintF("Replacing store #%d%s\n", instr->id(),
                   store->has_transition() ? " (with transition)" : "");
          }
          break;
        }
        case HValue::kArgumentsObject:
        case HValue::kCapturedObject:
        case HValue::kSimulate: {
          for (int i = 0; i < instr->OperandCount(); i++) {
            if (instr->OperandAt(i) != allocate) continue;
            instr->SetOperandAt(i, state);
          }
          break;
        }
        case HValue::kCheckHeapObject: {
          HCheckHeapObject* check = HCheckHeapObject::cast(instr);
          if (check->value() != allocate) continue;
          check->DeleteAndReplaceWith(check->ActualValue());
          break;
        }
        case HValue::kCheckMaps: {
          HCheckMaps* mapcheck = HCheckMaps::cast(instr);
          if (mapcheck->value() != allocate) continue;
          NewMapCheckAndInsert(state, mapcheck);
          mapcheck->DeleteAndReplaceWith(mapcheck->ActualValue());
          break;
        }
        default:
          // Nothing to see here, move along ...
          break;
      }
    }

    // Propagate the block state forward to all successor blocks.
    for (int i = 0; i < block->end()->SuccessorCount(); i++) {
      HBasicBlock* succ = block->end()->SuccessorAt(i);
      if (!allocate_block->Dominates(succ)) continue;
      if (succ->predecessors()->length() == 1) {
        // Case 1: This is the only predecessor, just reuse state.
        SetStateAt(succ, state);
      } else if (StateAt(succ) == NULL && succ->IsLoopHeader()) {
        // Case 2: This is a state that enters a loop header, be
        // pessimistic about loop headers, add phis for all values.
        SetStateAt(succ, NewStateForLoopHeader(succ->first(), state));
      } else if (StateAt(succ) == NULL) {
        // Case 3: This is the first state propagated forward to the
        // successor, leave a copy of the current state.
        SetStateAt(succ, NewStateCopy(succ->first(), state));
      } else {
        // Case 4: This is a state that needs merging with previously
        // propagated states, potentially introducing new phis lazily or
        // adding values to existing phis.
        HCapturedObject* succ_state = StateAt(succ);
        for (int index = 0; index < number_of_values_; index++) {
          HValue* operand = state->OperandAt(index);
          HValue* succ_operand = succ_state->OperandAt(index);
          if (succ_operand->IsPhi() && succ_operand->block() == succ) {
            // Phi already exists, add operand.
            HPhi* phi = HPhi::cast(succ_operand);
            phi->SetOperandAt(succ->PredecessorIndexOf(block), operand);
          } else if (succ_operand != operand) {
            // Phi does not exist, introduce one.
            HPhi* phi = NewPhiAndInsert(succ, succ_operand, index);
            phi->SetOperandAt(succ->PredecessorIndexOf(block), operand);
            succ_state->SetOperandAt(index, phi);
          }
        }
      }
    }
  }

  // All uses have been handled.
  DCHECK(allocate->HasNoUses());
  allocate->DeleteAndReplaceWith(NULL);
}


void HEscapeAnalysisPhase::PerformScalarReplacement() {
  for (int i = 0; i < captured_.length(); i++) {
    HAllocate* allocate = HAllocate::cast(captured_.at(i));

    // Compute number of scalar values and start with clean slate.
    int size_in_bytes = allocate->size()->GetInteger32Constant();
    number_of_values_ = size_in_bytes / kPointerSize;
    number_of_objects_++;
    block_states_.Rewind(0);

    // Perform actual analysis step.
    AnalyzeDataFlow(allocate);

    cumulative_values_ += number_of_values_;
    DCHECK(allocate->HasNoUses());
    DCHECK(!allocate->IsLinked());
  }
}


void HEscapeAnalysisPhase::Run() {
  // TODO(mstarzinger): We disable escape analysis with OSR for now, because
  // spill slots might be uninitialized. Needs investigation.
  if (graph()->has_osr()) return;
  int max_fixpoint_iteration_count = FLAG_escape_analysis_iterations;
  for (int i = 0; i < max_fixpoint_iteration_count; i++) {
    CollectCapturedValues();
    if (captured_.is_empty()) break;
    PerformScalarReplacement();
    captured_.Rewind(0);
  }
}


}  // namespace internal
}  // namespace v8
