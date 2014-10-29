// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_REPLAY_H_
#define V8_COMPILER_GRAPH_REPLAY_H_

#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Graph;

// Helper class to print a full replay of a graph. This replay can be used to
// materialize the same graph within a C++ unit test and hence test subsequent
// optimization passes on a graph without going through the construction steps.
class GraphReplayPrinter FINAL : public NullNodeVisitor {
 public:
#ifdef DEBUG
  static void PrintReplay(Graph* graph);
#else
  static void PrintReplay(Graph* graph) {}
#endif

  GenericGraphVisit::Control Pre(Node* node);
  void PostEdge(Node* from, int index, Node* to);

 private:
  GraphReplayPrinter() {}

  static void PrintReplayOpCreator(const Operator* op);

  DISALLOW_COPY_AND_ASSIGN(GraphReplayPrinter);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_REPLAY_H_
