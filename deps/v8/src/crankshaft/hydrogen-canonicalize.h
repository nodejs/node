// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_CANONICALIZE_H_
#define V8_CRANKSHAFT_HYDROGEN_CANONICALIZE_H_

#include "src/crankshaft/hydrogen.h"

namespace v8 {
namespace internal {


class HCanonicalizePhase : public HPhase {
 public:
  explicit HCanonicalizePhase(HGraph* graph)
      : HPhase("H_Canonicalize", graph) { }

  void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(HCanonicalizePhase);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_CANONICALIZE_H_
