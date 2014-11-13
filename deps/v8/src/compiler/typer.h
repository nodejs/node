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
  explicit Typer(Graph* graph, MaybeHandle<Context> context);
  ~Typer();

  void Run();

  Graph* graph() { return graph_; }
  MaybeHandle<Context> context() { return context_; }
  Zone* zone() { return graph_->zone(); }
  Isolate* isolate() { return zone()->isolate(); }

 private:
  class Visitor;
  class RunVisitor;
  class WidenVisitor;
  class Decorator;

  Graph* graph_;
  MaybeHandle<Context> context_;
  Decorator* decorator_;

  Zone* zone_;
  Type* negative_signed32;
  Type* non_negative_signed32;
  Type* undefined_or_null;
  Type* singleton_false;
  Type* singleton_true;
  Type* singleton_zero;
  Type* singleton_one;
  Type* zero_or_one;
  Type* zeroish;
  Type* falsish;
  Type* integer;
  Type* weakint;
  Type* number_fun0_;
  Type* number_fun1_;
  Type* number_fun2_;
  Type* weakint_fun1_;
  Type* imul_fun_;
  Type* clz32_fun_;
  Type* random_fun_;
  Type* array_buffer_fun_;
  Type* int8_array_fun_;
  Type* int16_array_fun_;
  Type* int32_array_fun_;
  Type* uint8_array_fun_;
  Type* uint16_array_fun_;
  Type* uint32_array_fun_;
  Type* float32_array_fun_;
  Type* float64_array_fun_;

  ZoneVector<Handle<Object> > weaken_min_limits_;
  ZoneVector<Handle<Object> > weaken_max_limits_;
  DISALLOW_COPY_AND_ASSIGN(Typer);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_TYPER_H_
