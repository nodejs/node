// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_MARK_DEOPTIMIZE_H_
#define V8_HYDROGEN_MARK_DEOPTIMIZE_H_

#include "src/hydrogen.h"

namespace v8 {
namespace internal {


// Compute DeoptimizeOnUndefined flag for phis.  Any phi that can reach a use
// with DeoptimizeOnUndefined set must have DeoptimizeOnUndefined set.
// Currently only HCompareNumericAndBranch, with double input representation,
// has this flag set.  The flag is used by HChange tagged->double, which must
// deoptimize if one of its uses has this flag set.
class HMarkDeoptimizeOnUndefinedPhase : public HPhase {
 public:
  explicit HMarkDeoptimizeOnUndefinedPhase(HGraph* graph)
      : HPhase("H_Mark deoptimize on undefined", graph),
        worklist_(16, zone()) {}

  void Run();

 private:
  void ProcessPhi(HPhi* phi);

  // Preallocated worklist used as an optimization so we don't have
  // to allocate a new ZoneList for every ProcessPhi() invocation.
  ZoneList<HPhi*> worklist_;

  DISALLOW_COPY_AND_ASSIGN(HMarkDeoptimizeOnUndefinedPhase);
};


class HComputeChangeUndefinedToNaN : public HPhase {
 public:
  explicit HComputeChangeUndefinedToNaN(HGraph* graph)
      : HPhase("H_Compute change undefined to nan", graph) {}

  void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(HComputeChangeUndefinedToNaN);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_MARK_DEOPTIMIZE_H_
