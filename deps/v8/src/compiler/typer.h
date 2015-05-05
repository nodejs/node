// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPER_H_
#define V8_COMPILER_TYPER_H_

#include "src/compiler/graph.h"
#include "src/types.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class LazyTypeCache;


class Typer {
 public:
  Typer(Isolate* isolate, Graph* graph, MaybeHandle<Context> context);
  ~Typer();

  void Run();

  Graph* graph() { return graph_; }
  MaybeHandle<Context> context() { return context_; }
  Zone* zone() { return graph_->zone(); }
  Isolate* isolate() { return isolate_; }

 private:
  class Visitor;
  class Decorator;

  Isolate* isolate_;
  Graph* graph_;
  MaybeHandle<Context> context_;
  Decorator* decorator_;

  Zone* zone_;
  Type* boolean_or_number;
  Type* undefined_or_null;
  Type* undefined_or_number;
  Type* negative_signed32;
  Type* non_negative_signed32;
  Type* singleton_false;
  Type* singleton_true;
  Type* singleton_zero;
  Type* singleton_one;
  Type* zero_or_one;
  Type* zeroish;
  Type* signed32ish;
  Type* unsigned32ish;
  Type* falsish;
  Type* truish;
  Type* integer;
  Type* weakint;
  Type* number_fun0_;
  Type* number_fun1_;
  Type* number_fun2_;
  Type* weakint_fun1_;
  Type* random_fun_;
  LazyTypeCache* cache_;

  DISALLOW_COPY_AND_ASSIGN(Typer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TYPER_H_
