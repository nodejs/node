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

#ifndef V8_HYDROGEN_ESCAPE_ANALYSIS_H_
#define V8_HYDROGEN_ESCAPE_ANALYSIS_H_

#include "allocation.h"
#include "hydrogen.h"

namespace v8 {
namespace internal {


class HEscapeAnalysisPhase : public HPhase {
 public:
  explicit HEscapeAnalysisPhase(HGraph* graph)
      : HPhase("H_Escape analysis", graph),
        captured_(0, zone()),
        number_of_objects_(0),
        number_of_values_(0),
        cumulative_values_(0),
        block_states_(graph->blocks()->length(), zone()) { }

  void Run();

 private:
  void CollectCapturedValues();
  bool HasNoEscapingUses(HValue* value, int size);
  void PerformScalarReplacement();
  void AnalyzeDataFlow(HInstruction* instr);

  HCapturedObject* NewState(HInstruction* prev);
  HCapturedObject* NewStateForAllocation(HInstruction* prev);
  HCapturedObject* NewStateForLoopHeader(HInstruction* prev, HCapturedObject*);
  HCapturedObject* NewStateCopy(HInstruction* prev, HCapturedObject* state);

  HPhi* NewPhiAndInsert(HBasicBlock* block, HValue* incoming_value, int index);

  HValue* NewMapCheckAndInsert(HCapturedObject* state, HCheckMaps* mapcheck);

  HCapturedObject* StateAt(HBasicBlock* block) {
    return block_states_.at(block->block_id());
  }

  void SetStateAt(HBasicBlock* block, HCapturedObject* state) {
    block_states_.Set(block->block_id(), state);
  }

  // List of allocations captured during collection phase.
  ZoneList<HInstruction*> captured_;

  // Number of captured objects on which scalar replacement was done.
  int number_of_objects_;

  // Number of scalar values tracked during scalar replacement phase.
  int number_of_values_;
  int cumulative_values_;

  // Map of block IDs to the data-flow state at block entry during the
  // scalar replacement phase.
  ZoneList<HCapturedObject*> block_states_;
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_ESCAPE_ANALYSIS_H_
