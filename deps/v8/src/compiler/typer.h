// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPER_H_
#define V8_COMPILER_TYPER_H_

#include "src/compiler/graph.h"
#include "src/compiler/operation-typer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class LoopVariableOptimizer;

class Typer {
 public:
  Typer(Isolate* isolate, Graph* graph);
  ~Typer();

  void Run();
  // TODO(bmeurer,jarin): Remove this once we have a notion of "roots" on Graph.
  void Run(const ZoneVector<Node*>& roots,
           LoopVariableOptimizer* induction_vars);

 private:
  class Visitor;
  class Decorator;

  Graph* graph() const { return graph_; }
  Zone* zone() const { return graph()->zone(); }
  Isolate* isolate() const { return isolate_; }
  OperationTyper* operation_typer() { return &operation_typer_; }

  Isolate* const isolate_;
  Graph* const graph_;
  Decorator* decorator_;
  TypeCache const& cache_;
  OperationTyper operation_typer_;

  Type* singleton_false_;
  Type* singleton_true_;
  Type* singleton_the_hole_;
  Type* falsish_;
  Type* truish_;

  DISALLOW_COPY_AND_ASSIGN(Typer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TYPER_H_
