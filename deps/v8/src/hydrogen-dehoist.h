// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_DEHOIST_H_
#define V8_HYDROGEN_DEHOIST_H_

#include "hydrogen.h"

namespace v8 {
namespace internal {


class HDehoistIndexComputationsPhase : public HPhase {
 public:
  explicit HDehoistIndexComputationsPhase(HGraph* graph)
      : HPhase("H_Dehoist index computations", graph) { }

  void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(HDehoistIndexComputationsPhase);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_DEHOIST_H_
