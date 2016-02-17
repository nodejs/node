// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_STORE_ELIMINATION_H_
#define V8_CRANKSHAFT_HYDROGEN_STORE_ELIMINATION_H_

#include "src/crankshaft/hydrogen.h"
#include "src/crankshaft/hydrogen-alias-analysis.h"

namespace v8 {
namespace internal {

class HStoreEliminationPhase : public HPhase {
 public:
  explicit HStoreEliminationPhase(HGraph* graph)
    : HPhase("H_Store elimination", graph),
      unobserved_(10, zone()),
      aliasing_() { }

  void Run();
 private:
  ZoneList<HStoreNamedField*> unobserved_;
  HAliasAnalyzer* aliasing_;

  void ProcessStore(HStoreNamedField* store);
  void ProcessLoad(HLoadNamedField* load);
  void ProcessInstr(HInstruction* instr, GVNFlagSet flags);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_STORE_ELIMINATION_H_
