// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_VERIFIER_H_
#define V8_COMPILER_VERIFIER_H_

#include "src/v8.h"

#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

class Verifier {
 public:
  static void Run(Graph* graph);

 private:
  class Visitor;
  DISALLOW_COPY_AND_ASSIGN(Verifier);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_VERIFIER_H_
