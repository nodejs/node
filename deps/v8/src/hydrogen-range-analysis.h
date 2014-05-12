// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_RANGE_ANALYSIS_H_
#define V8_HYDROGEN_RANGE_ANALYSIS_H_

#include "hydrogen.h"

namespace v8 {
namespace internal {


class HRangeAnalysisPhase : public HPhase {
 public:
  explicit HRangeAnalysisPhase(HGraph* graph)
      : HPhase("H_Range analysis", graph), changed_ranges_(16, zone()),
        in_worklist_(graph->GetMaximumValueID(), zone()),
        worklist_(32, zone()) {}

  void Run();

 private:
  void TraceRange(const char* msg, ...);
  void InferControlFlowRange(HCompareNumericAndBranch* test,
                             HBasicBlock* dest);
  void UpdateControlFlowRange(Token::Value op, HValue* value, HValue* other);
  void InferRange(HValue* value);
  void RollBackTo(int index);
  void AddRange(HValue* value, Range* range);
  void AddToWorklist(HValue* value) {
    if (in_worklist_.Contains(value->id())) return;
    in_worklist_.Add(value->id());
    worklist_.Add(value, zone());
  }
  void PropagateMinusZeroChecks(HValue* value);
  void PoisonRanges();

  ZoneList<HValue*> changed_ranges_;

  BitVector in_worklist_;
  ZoneList<HValue*> worklist_;

  DISALLOW_COPY_AND_ASSIGN(HRangeAnalysisPhase);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_RANGE_ANALYSIS_H_
