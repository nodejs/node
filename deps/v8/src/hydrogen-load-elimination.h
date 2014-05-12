// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_LOAD_ELIMINATION_H_
#define V8_HYDROGEN_LOAD_ELIMINATION_H_

#include "hydrogen.h"

namespace v8 {
namespace internal {

class HLoadEliminationPhase : public HPhase {
 public:
  explicit HLoadEliminationPhase(HGraph* graph)
      : HPhase("H_Load elimination", graph) { }

  void Run();

 private:
  void EliminateLoads(HBasicBlock* block);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_LOAD_ELIMINATION_H_
