// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/pretenuring-propagation-reducer.h"

namespace v8::internal::compiler::turboshaft {

namespace {

bool CouldBeAllocate(const Operation& base) {
  return base.Is<PhiOp>() || base.Is<AllocateOp>();
}

}  // namespace

void PretenuringPropagationAnalyzer::ProcessStore(const StoreOp& store) {
  OpIndex base_idx = store.base();
  OpIndex value_idx = store.value();
  const Operation& base = input_graph_.Get(base_idx);
  const Operation& value = input_graph_.Get(value_idx);

  if (!CouldBeAllocate(base) || !CouldBeAllocate(value)) {
    return;
  }

  if (value.Is<AllocateOp>() &&
      value.Cast<AllocateOp>().type == AllocationType::kOld) {
    // {value} is already Old, and we don't care about new-to-old and old-to-old
    // stores.
    return;
  }

  if (value.Is<PhiOp>() && TryFind(value_idx) == nullptr) {
    // {value} is not worth being recorded, as it's not an Allocation (or a Phi
    // of Allocations) that could be promoted to Old.
    return;
  }

  ZoneVector<OpIndex>* stored_in_base = FindOrCreate(base_idx);
  stored_in_base->push_back(value_idx);
}

void PretenuringPropagationAnalyzer::ProcessPhi(const PhiOp& phi) {
  // Phis act as storing all of their inputs. It's not how they work in
  // practice, but if a Phi has a Young input, and is stored in an Old object,
  // it makes sense to Oldify the phi input.

  // For better performance, we only record inputs that could be an allocation:
  // Phis with an entry in {store_graph_} or AllocateOp.
  // Note that this is slightly imprecise for loop Phis (since if the backedge
  // is a Phi itself, it won't have an entry in {store_graph_} yet), but it
  // should still be good enough for most cases.

  base::SmallVector<OpIndex, 16> interesting_inputs;
  for (OpIndex input : phi.inputs()) {
    const Operation& op = input_graph_.Get(input);
    if (op.Is<AllocateOp>()) {
      interesting_inputs.push_back(input);
    } else if (op.Is<PhiOp>() && TryFind(input) != nullptr) {
      interesting_inputs.push_back(input);
    }
  }
  if (interesting_inputs.empty()) return;

  ZoneVector<OpIndex>* stored_in_phi = Create(input_graph_.Index(phi));
  for (OpIndex input : interesting_inputs) {
    stored_in_phi->push_back(input);
  }
}

void PretenuringPropagationAnalyzer::ProcessAllocate(
    const AllocateOp& allocate) {
  if (allocate.type == AllocationType::kOld) {
    // We could be a bit more lazy in storing old AllocateOp into {old_allocs_}
    // (by waiting for a Store or a Phi to use the AllocateOp), but there is
    // usually very few old allocation, so it makes sense to do it eagerly.
    old_allocs_.push_back(input_graph_.Index(allocate));
  }
}

bool PretenuringPropagationAnalyzer::PushContainedValues(OpIndex base) {
  // Push into {queue_} all of the values that are "contained" into {base}:
  // values that are stored to {base} if {base} is an AllocateOp, or Phi inputs
  // if {base} is a Phi.
  ZoneVector<OpIndex>* contained = TryFind(base);
  if (contained == nullptr) return false;
  for (OpIndex index : *contained) {
    queue_.push_back(index);
  }
  return true;
}

// Performs a DFS from {old_alloc} and mark everything it finds as Old. The DFS
// stops on already-Old nodes.
void PretenuringPropagationAnalyzer::OldifySubgraph(OpIndex old_alloc) {
  queue_.clear();
  if (!PushContainedValues(old_alloc)) return;

  while (!queue_.empty()) {
    OpIndex idx = queue_.back();
    queue_.pop_back();
    Operation& op = input_graph_.Get(idx);
    if (AllocateOp* alloc = op.TryCast<AllocateOp>()) {
      if (alloc->type == AllocationType::kOld) continue;
      alloc->type = AllocationType::kOld;
      PushContainedValues(idx);
    } else {
      DCHECK(op.Is<PhiOp>());
      if (old_phis_.find(idx) != old_phis_.end()) continue;
      old_phis_.insert(idx);
      PushContainedValues(idx);
    }
  }
}

void PretenuringPropagationAnalyzer::PropagateAllocationTypes() {
  for (OpIndex old_alloc : old_allocs_) {
    OldifySubgraph(old_alloc);
  }
}

void PretenuringPropagationAnalyzer::BuildStoreInputGraph() {
  for (auto& op : input_graph_.AllOperations()) {
    if (ShouldSkipOperation(op)) {
      continue;
    }
    switch (op.opcode) {
      case Opcode::kStore:
        ProcessStore(op.Cast<StoreOp>());
        break;
      case Opcode::kAllocate:
        ProcessAllocate(op.Cast<AllocateOp>());
        break;
      case Opcode::kPhi:
        ProcessPhi(op.Cast<PhiOp>());
        break;
      default:
        break;
    }
  }
}

void PretenuringPropagationAnalyzer::Run() {
  BuildStoreInputGraph();

  PropagateAllocationTypes();
}

}  // namespace v8::internal::compiler::turboshaft
