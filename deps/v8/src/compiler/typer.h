// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPER_H_
#define V8_COMPILER_TYPER_H_

#include "src/v8.h"

#include "src/compiler/graph.h"
#include "src/compiler/opcodes.h"
#include "src/types.h"

namespace v8 {
namespace internal {
namespace compiler {

class Typer {
 public:
  explicit Typer(Zone* zone);

  void Init(Node* node);
  void Run(Graph* graph, MaybeHandle<Context> context);
  void Narrow(Graph* graph, Node* node, MaybeHandle<Context> context);
  void Widen(Graph* graph, Node* node, MaybeHandle<Context> context);

  void DecorateGraph(Graph* graph);

  Zone* zone() { return zone_; }
  Isolate* isolate() { return zone_->isolate(); }

 private:
  class Visitor;
  class RunVisitor;
  class NarrowVisitor;
  class WidenVisitor;

  Zone* zone_;
  Type* number_fun0_;
  Type* number_fun1_;
  Type* number_fun2_;
  Type* imul_fun_;
  Type* array_buffer_fun_;
  Type* int8_array_fun_;
  Type* int16_array_fun_;
  Type* int32_array_fun_;
  Type* uint8_array_fun_;
  Type* uint16_array_fun_;
  Type* uint32_array_fun_;
  Type* float32_array_fun_;
  Type* float64_array_fun_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_TYPER_H_
