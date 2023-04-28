// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_BITCAST_ELIDER_H_
#define V8_COMPILER_BACKEND_BITCAST_ELIDER_H_

#include "src/compiler/node-marker.h"
#include "src/compiler/node.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

class Graph;

// Elide all the Bitcast and TruncateInt64ToInt32 nodes which are required by
// MachineGraphVerifier. This avoid generating redundant move instructions in
// instruction selection phase.
class BitcastElider {
 public:
  BitcastElider(Zone* zone, Graph* graph, bool is_builtin);
  ~BitcastElider() = default;

  void Reduce();

  void Enqueue(Node* node);
  void Revisit(Node* node);
  void VisitNode(Node* node);
  void ProcessGraph();

 private:
  Graph* const graph_;
  ZoneQueue<Node*> to_visit_;
  NodeMarker<bool> seen_;
  bool is_builtin_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_BITCAST_ELIDER_H_
