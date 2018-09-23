// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_CANONICALIZE_H_
#define V8_HYDROGEN_CANONICALIZE_H_

#include "src/hydrogen.h"

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


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_CANONICALIZE_H_
