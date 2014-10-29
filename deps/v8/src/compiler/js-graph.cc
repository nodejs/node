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
  Unique<Object> unique = Unique<Object>::CreateImmovable(object);
  return NewNode(common()->HeapConstant(unique));
}


Node* JSGraph::NewNode(const Operator* op) {
  Node* node = graph()->NewNode(op);
  typer_->Init(node);
  return node;
}


Node* JSGraph::CEntryStubConstant() {
  if (!c_entry_stub_constant_.is_set()) {
    c_entry_stub_constant_.set(
        ImmovableHeapConstant(CEntryStub(isolate(), 1).GetCode()));
  }
  return c_entry_stub_constant_.get();
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


Node* JSGraph::HeapConstant(Unique<Object> value) {
  // TODO(turbofan): canonicalize heap constants using Unique<T>
  return NewNode(common()->HeapConstant(value));
}


Node* JSGraph::HeapConstant(Handle<Object> value) {
  // TODO(titzer): We could also match against the addresses of immortable
  // immovables here, even without access to the heap, thus always
  // canonicalizing references to them.
  // return HeapConstant(Unique<Object>::CreateUninitialized(value));
  // TODO(turbofan): This is a work-around to make Unique::HashCode() work for
  // value numbering. We need some sane way to compute a unique hash code for
  // arbitrary handles here.
  Unique<Object> unique(reinterpret_cast<Address>(*value.location()), value);
  return HeapConstant(unique);
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
  if (bit_cast<int64_t>(value) == bit_cast<int64_t>(0.0)) return ZeroConstant();
  if (bit_cast<int64_t>(value) == bit_cast<int64_t>(1.0)) return OneConstant();
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


Node* JSGraph::Float32Constant(float value) {
  // TODO(turbofan): cache float32 constants.
  return NewNode(common()->Float32Constant(value));
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
