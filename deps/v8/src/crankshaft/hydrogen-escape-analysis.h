// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_ESCAPE_ANALYSIS_H_
#define V8_CRANKSHAFT_HYDROGEN_ESCAPE_ANALYSIS_H_

#include "src/allocation.h"
#include "src/crankshaft/hydrogen.h"

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

  HValue* NewLoadReplacement(HLoadNamedField* load, HValue* load_value);

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


}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_ESCAPE_ANALYSIS_H_
