// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_REMOVABLE_SIMULATES_H_
#define V8_CRANKSHAFT_HYDROGEN_REMOVABLE_SIMULATES_H_

#include "src/crankshaft/hydrogen.h"

namespace v8 {
namespace internal {


class HMergeRemovableSimulatesPhase : public HPhase {
 public:
  explicit HMergeRemovableSimulatesPhase(HGraph* graph)
      : HPhase("H_Merge removable simulates", graph) { }

  void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(HMergeRemovableSimulatesPhase);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_REMOVABLE_SIMULATES_H_
