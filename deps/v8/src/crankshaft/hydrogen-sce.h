// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_SCE_H_
#define V8_CRANKSHAFT_HYDROGEN_SCE_H_

#include "src/crankshaft/hydrogen.h"

namespace v8 {
namespace internal {


class HStackCheckEliminationPhase : public HPhase {
 public:
  explicit HStackCheckEliminationPhase(HGraph* graph)
      : HPhase("H_Stack check elimination", graph) { }

  void Run();
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_SCE_H_
