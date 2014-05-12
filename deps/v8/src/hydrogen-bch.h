// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_BCH_H_
#define V8_HYDROGEN_BCH_H_

#include "hydrogen.h"

namespace v8 {
namespace internal {


class HBoundsCheckHoistingPhase : public HPhase {
 public:
  explicit HBoundsCheckHoistingPhase(HGraph* graph)
      : HPhase("H_Bounds checks hoisting", graph) { }

  void Run() {
    HoistRedundantBoundsChecks();
  }

 private:
  void HoistRedundantBoundsChecks();

  DISALLOW_COPY_AND_ASSIGN(HBoundsCheckHoistingPhase);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_BCE_H_
