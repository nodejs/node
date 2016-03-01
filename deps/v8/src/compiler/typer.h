// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPER_H_
#define V8_COMPILER_TYPER_H_

#include "src/base/flags.h"
#include "src/compiler/graph.h"
#include "src/types.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;
class TypeCache;

namespace compiler {


class Typer {
 public:
  // Flags that control the mode of operation.
  enum Flag {
    kNoFlags = 0u,
    kDeoptimizationEnabled = 1u << 0,
  };
  typedef base::Flags<Flag> Flags;

  Typer(Isolate* isolate, Graph* graph, Flags flags = kNoFlags,
        CompilationDependencies* dependencies = nullptr,
        Type::FunctionType* function_type = nullptr);
  ~Typer();

  void Run();
  // TODO(bmeurer,jarin): Remove this once we have a notion of "roots" on Graph.
  void Run(const ZoneVector<Node*>& roots);

 private:
  class Visitor;
  class Decorator;

  Graph* graph() const { return graph_; }
  Zone* zone() const { return graph()->zone(); }
  Isolate* isolate() const { return isolate_; }
  Flags flags() const { return flags_; }
  CompilationDependencies* dependencies() const { return dependencies_; }
  Type::FunctionType* function_type() const { return function_type_; }

  Isolate* const isolate_;
  Graph* const graph_;
  Flags const flags_;
  CompilationDependencies* const dependencies_;
  Type::FunctionType* function_type_;
  Decorator* decorator_;
  TypeCache const& cache_;

  Type* singleton_false_;
  Type* singleton_true_;
  Type* singleton_the_hole_;
  Type* signed32ish_;
  Type* unsigned32ish_;
  Type* falsish_;
  Type* truish_;

  DISALLOW_COPY_AND_ASSIGN(Typer);
};

DEFINE_OPERATORS_FOR_FLAGS(Typer::Flags)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TYPER_H_
