// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-osr.h"

#include "src/crankshaft/hydrogen.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// True iff. we are compiling for OSR and the statement is the entry.
bool HOsrBuilder::HasOsrEntryAt(IterationStatement* statement) {
  return statement->OsrEntryId() == builder_->current_info()->osr_ast_id();
}


HBasicBlock* HOsrBuilder::BuildOsrLoopEntry(IterationStatement* statement) {
  DCHECK(HasOsrEntryAt(statement));

  Zone* zone = builder_->zone();
  HGraph* graph = builder_->graph();

  // only one OSR point per compile is allowed.
  DCHECK(graph->osr() == NULL);

  // remember this builder as the one OSR builder in the graph.
  graph->set_osr(this);

  HBasicBlock* non_osr_entry = graph->CreateBasicBlock();
  osr_entry_ = graph->CreateBasicBlock();
  HValue* true_value = graph->GetConstantTrue();
  HBranch* test = builder_->New<HBranch>(true_value, ToBooleanHint::kNone,
                                         non_osr_entry, osr_entry_);
  builder_->FinishCurrentBlock(test);

  HBasicBlock* loop_predecessor = graph->CreateBasicBlock();
  builder_->Goto(non_osr_entry, loop_predecessor);

  builder_->set_current_block(osr_entry_);
  osr_entry_->set_osr_entry();
  BailoutId osr_entry_id = statement->OsrEntryId();

  HEnvironment *environment = builder_->environment();
  int first_expression_index = environment->first_expression_index();
  int length = environment->length();
  osr_values_ = new(zone) ZoneList<HUnknownOSRValue*>(length, zone);

  for (int i = 0; i < first_expression_index; ++i) {
    HUnknownOSRValue* osr_value
        = builder_->Add<HUnknownOSRValue>(environment, i);
    environment->Bind(i, osr_value);
    osr_values_->Add(osr_value, zone);
  }

  if (first_expression_index != length) {
    environment->Drop(length - first_expression_index);
    for (int i = first_expression_index; i < length; ++i) {
      HUnknownOSRValue* osr_value
          = builder_->Add<HUnknownOSRValue>(environment, i);
      environment->Push(osr_value);
      osr_values_->Add(osr_value, zone);
    }
  }

  unoptimized_frame_slots_ =
      environment->local_count() + environment->push_count();

  // Keep a copy of the old environment, since the OSR values need it
  // to figure out where exactly they are located in the unoptimized frame.
  environment = environment->Copy();
  builder_->current_block()->UpdateEnvironment(environment);

  builder_->Add<HSimulate>(osr_entry_id);
  builder_->Add<HOsrEntry>(osr_entry_id);
  HContext* context = builder_->Add<HContext>();
  environment->BindContext(context);
  builder_->Goto(loop_predecessor);
  loop_predecessor->SetJoinId(statement->EntryId());
  builder_->set_current_block(loop_predecessor);

  // Create the final loop entry
  osr_loop_entry_ = builder_->BuildLoopEntry();
  return osr_loop_entry_;
}


void HOsrBuilder::FinishGraph() {
  // do nothing for now.
}


void HOsrBuilder::FinishOsrValues() {
  const ZoneList<HPhi*>* phis = osr_loop_entry_->phis();
  for (int j = 0; j < phis->length(); j++) {
    HPhi* phi = phis->at(j);
    if (phi->HasMergedIndex()) {
      osr_values_->at(phi->merged_index())->set_incoming_value(phi);
    }
  }
}

}  // namespace internal
}  // namespace v8
