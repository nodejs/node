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
class Edge;
class Node;
class Schedule;

// Verifies properties of a graph, such as the well-formedness of inputs to
// each node, etc.
class Verifier {
 public:
  enum Typing { TYPED, UNTYPED };
  enum CheckInputs { kValuesOnly, kAll };
  enum CodeType { kDefault, kWasm };

  Verifier(const Verifier&) = delete;
  Verifier& operator=(const Verifier&) = delete;

  static void Run(Graph* graph, Typing typing = TYPED,
                  CheckInputs check_inputs = kAll,
                  CodeType code_type = kDefault);

#ifdef DEBUG
  // Verifies consistency of node inputs and uses:
  // - node inputs should agree with the input count computed from
  //   the node's operator.
  // - effect inputs should have effect outputs.
  // - control inputs should have control outputs.
  // - frame state inputs should be frame states.
  // - if the node has control uses, it should produce control.
  // - if the node has effect uses, it should produce effect.
  // - if the node has frame state uses, it must be a frame state.
  static void VerifyNode(Node* node);

  // Verify that {replacement} has the required outputs
  // (effect, control or frame state) to be used as an input for {edge}.
  static void VerifyEdgeInputReplacement(const Edge& edge,
                                         const Node* replacement);
#else
  static void VerifyNode(Node* node) {}
  static void VerifyEdgeInputReplacement(const Edge& edge,
                                         const Node* replacement) {}
#endif  // DEBUG

 private:
  class Visitor;
};

// Verifies properties of a schedule, such as dominance, phi placement, etc.
class V8_EXPORT_PRIVATE ScheduleVerifier {
 public:
  static void Run(Schedule* schedule);
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_VERIFIER_H_
