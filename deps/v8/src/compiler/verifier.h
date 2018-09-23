// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_VERIFIER_H_
#define V8_COMPILER_VERIFIER_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace compiler {

class Graph;
class Schedule;

// Verifies properties of a graph, such as the well-formedness of inputs to
// each node, etc.
class Verifier {
 public:
  enum Typing { TYPED, UNTYPED };

  static void Run(Graph* graph, Typing typing = TYPED);

 private:
  class Visitor;
  DISALLOW_COPY_AND_ASSIGN(Verifier);
};

// Verifies properties of a schedule, such as dominance, phi placement, etc.
class ScheduleVerifier {
 public:
  static void Run(Schedule* schedule);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_VERIFIER_H_
