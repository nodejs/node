// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_REDUNDANT_PHI_H_
#define V8_HYDROGEN_REDUNDANT_PHI_H_

#include "hydrogen.h"

namespace v8 {
namespace internal {


// Replace all phis consisting of a single non-loop operand plus any number of
// loop operands by that single non-loop operand.
class HRedundantPhiEliminationPhase : public HPhase {
 public:
  explicit HRedundantPhiEliminationPhase(HGraph* graph)
      : HPhase("H_Redundant phi elimination", graph) { }

  void Run();
  void ProcessBlock(HBasicBlock* block);

 private:
  void ProcessPhis(const ZoneList<HPhi*>* phis);

  DISALLOW_COPY_AND_ASSIGN(HRedundantPhiEliminationPhase);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_REDUNDANT_PHI_H_
