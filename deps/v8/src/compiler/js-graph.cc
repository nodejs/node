// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/typer.h"

namespace v8 {
namespace internal {
namespace compiler {

Node* JSGraph::ImmovableHeapConstant(Handle<Object> object) {
  PrintableUnique<Object> unique =
      PrintableUnique<Object>::CreateImmovable(zone(), object);
  return NewNode(common()->HeapConstant(unique));
}


Node* JSGraph::NewNode(Operator* op) {
  Node* node = graph()->NewNode(op);
  typer_->Init(node);
  return node;
}


Node* JSGraph::UndefinedConstant() {
  if (!undefined_constant_.is_set()) {
    undefined_constant_.set(
        ImmovableHeapConstant(factory()->undefined_value()));
  }
  return undefined_constant_.get();
}


Node* JSGraph::TheHoleConstant() {
  if (!the_hole_constant_.is_set()) {
    the_hole_constant_.set(ImmovableHeapConstant(factory()->the_hole_value()));
  }
  return the_hole_constant_.get();
}


Node* JSGraph::TrueConstant() {
  if (!true_constant_.is_set()) {
    true_constant_.set(ImmovableHeapConstant(factory()->true_value()));
  }
  return true_constant_.get();
}


Node* JSGraph::FalseConstant() {
  if (!false_constant_.is_set()) {
    false_constant_.set(ImmovableHeapConstant(factory()->false_value()));
  }
  return false_constant_.get();
}


Node* JSGraph::NullConstant() {
  if (!null_constant_.is_set()) {
    null_constant_.set(ImmovableHeapConstant(factory()->null_value()));
  }
  return null_constant_.get();
}


Node* JSGraph::ZeroConstant() {
  if (!zero_constant_.is_set()) zero_constant_.set(NumberConstant(0.0));
  return zero_constant_.get();
}


Node* JSGraph::OneConstant() {
  if (!one_constant_.is_set()) one_constant_.set(NumberConstant(1.0));
  return one_constant_.get();
}


Node* JSGraph::NaNConstant() {
  if (!nan_constant_.is_set()) {
    nan_constant_.set(NumberConstant(base::OS::nan_value()));
  }
  return nan_constant_.get();
}


Node* JSGraph::HeapConstant(PrintableUnique<Object> value) {
  // TODO(turbofan): canonicalize heap constants using Unique<T>
  return NewNode(common()->HeapConstant(value));
}


Node* JSGraph::HeapConstant(Handle<Object> value) {
  // TODO(titzer): We could also match against the addresses of immortable
  // immovables here, even without access to the heap, thus always
  // canonicalizing references to them.
  return HeapConstant(
      PrintableUnique<Object>::CreateUninitialized(zone(), value));
}


Node* JSGraph::Constant(Handle<Object> value) {
  // Dereference the handle to determine if a number constant or other
  // canonicalized node can be used.
  if (value->IsNumber()) {
    return Constant(value->Number());
  } else if (value->IsUndefined()) {
    return UndefinedConstant();
  } else if (value->IsTrue()) {
    return TrueConstant();
  } else if (value->IsFalse()) {
    return FalseConstant();
  } else if (value->IsNull()) {
    return NullConstant();
  } else if (value->IsTheHole()) {
    return TheHoleConstant();
  } else {
    return HeapConstant(value);
  }
}


Node* JSGraph::Constant(double value) {
  if (BitCast<int64_t>(value) == BitCast<int64_t>(0.0)) return ZeroConstant();
  if (BitCast<int64_t>(value) == BitCast<int64_t>(1.0)) return OneConstant();
  return NumberConstant(value);
}


Node* JSGraph::Constant(int32_t value) {
  if (value == 0) return ZeroConstant();
  if (value == 1) return OneConstant();
  return NumberConstant(value);
}


Node* JSGraph::Int32Constant(int32_t value) {
  Node** loc = cache_.FindInt32Constant(value);
  if (*loc == NULL) {
    *loc = NewNode(common()->Int32Constant(value));
  }
  return *loc;
}


Node* JSGraph::NumberConstant(double value) {
  Node** loc = cache_.FindNumberConstant(value);
  if (*loc == NULL) {
    *loc = NewNode(common()->NumberConstant(value));
  }
  return *loc;
}


Node* JSGraph::Float64Constant(double value) {
  Node** loc = cache_.FindFloat64Constant(value);
  if (*loc == NULL) {
    *loc = NewNode(common()->Float64Constant(value));
  }
  return *loc;
}


Node* JSGraph::ExternalConstant(ExternalReference reference) {
  Node** loc = cache_.FindExternalConstant(reference);
  if (*loc == NULL) {
    *loc = NewNode(common()->ExternalConstant(reference));
  }
  return *loc;
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
