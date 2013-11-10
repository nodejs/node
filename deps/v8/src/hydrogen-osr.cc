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
#include "hydrogen-osr.h"

namespace v8 {
namespace internal {

// True iff. we are compiling for OSR and the statement is the entry.
bool HOsrBuilder::HasOsrEntryAt(IterationStatement* statement) {
  return statement->OsrEntryId() == builder_->current_info()->osr_ast_id();
}


HBasicBlock* HOsrBuilder::BuildOsrLoopEntry(IterationStatement* statement) {
  ASSERT(HasOsrEntryAt(statement));

  Zone* zone = builder_->zone();
  HGraph* graph = builder_->graph();

  // only one OSR point per compile is allowed.
  ASSERT(graph->osr() == NULL);

  // remember this builder as the one OSR builder in the graph.
  graph->set_osr(this);

  HBasicBlock* non_osr_entry = graph->CreateBasicBlock();
  osr_entry_ = graph->CreateBasicBlock();
  HValue* true_value = graph->GetConstantTrue();
  HBranch* test = builder_->New<HBranch>(true_value, ToBooleanStub::Types(),
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

} }  // namespace v8::internal
