// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/typer.h"

namespace v8 {
namespace internal {
namespace compiler {

Node* JSGraph::ImmovableHeapConstant(Handle<HeapObject> object) {
  Unique<HeapObject> unique = Unique<HeapObject>::CreateImmovable(object);
  return graph()->NewNode(common()->HeapConstant(unique));
}


Node* JSGraph::CEntryStubConstant(int result_size) {
  if (result_size == 1) {
    if (!c_entry_stub_constant_.is_set()) {
      c_entry_stub_constant_.set(
          ImmovableHeapConstant(CEntryStub(isolate(), 1).GetCode()));
    }
    return c_entry_stub_constant_.get();
  }

  return ImmovableHeapConstant(CEntryStub(isolate(), result_size).GetCode());
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
    nan_constant_.set(NumberConstant(std::numeric_limits<double>::quiet_NaN()));
  }
  return nan_constant_.get();
}


Node* JSGraph::HeapConstant(Unique<HeapObject> value) {
  // TODO(turbofan): canonicalize heap constants using Unique<T>
  return graph()->NewNode(common()->HeapConstant(value));
}


Node* JSGraph::HeapConstant(Handle<HeapObject> value) {
  // TODO(titzer): We could also match against the addresses of immortable
  // immovables here, even without access to the heap, thus always
  // canonicalizing references to them.
  // return HeapConstant(Unique<Object>::CreateUninitialized(value));
  // TODO(turbofan): This is a work-around to make Unique::HashCode() work for
  // value numbering. We need some sane way to compute a unique hash code for
  // arbitrary handles here.
  Unique<HeapObject> unique(reinterpret_cast<Address>(*value.location()),
                            value);
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
    return HeapConstant(Handle<HeapObject>::cast(value));
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
    *loc = graph()->NewNode(common()->Int32Constant(value));
  }
  return *loc;
}


Node* JSGraph::Int64Constant(int64_t value) {
  Node** loc = cache_.FindInt64Constant(value);
  if (*loc == NULL) {
    *loc = graph()->NewNode(common()->Int64Constant(value));
  }
  return *loc;
}


Node* JSGraph::NumberConstant(double value) {
  Node** loc = cache_.FindNumberConstant(value);
  if (*loc == NULL) {
    *loc = graph()->NewNode(common()->NumberConstant(value));
  }
  return *loc;
}


Node* JSGraph::Float32Constant(float value) {
  Node** loc = cache_.FindFloat32Constant(value);
  if (*loc == NULL) {
    *loc = graph()->NewNode(common()->Float32Constant(value));
  }
  return *loc;
}


Node* JSGraph::Float64Constant(double value) {
  Node** loc = cache_.FindFloat64Constant(value);
  if (*loc == NULL) {
    *loc = graph()->NewNode(common()->Float64Constant(value));
  }
  return *loc;
}


Node* JSGraph::ExternalConstant(ExternalReference reference) {
  Node** loc = cache_.FindExternalConstant(reference);
  if (*loc == NULL) {
    *loc = graph()->NewNode(common()->ExternalConstant(reference));
  }
  return *loc;
}


Node* JSGraph::EmptyFrameState() {
  if (!empty_frame_state_.is_set()) {
    Node* values = graph()->NewNode(common()->StateValues(0));
    Node* state_node = graph()->NewNode(
        common()->FrameState(JS_FRAME, BailoutId::None(),
                             OutputFrameStateCombine::Ignore()),
        values, values, values, NoContextConstant(), UndefinedConstant());
    empty_frame_state_.set(state_node);
  }
  return empty_frame_state_.get();
}


Node* JSGraph::DeadControl() {
  if (!dead_control_.is_set()) {
    Node* dead_node = graph()->NewNode(common()->Dead());
    dead_control_.set(dead_node);
  }
  return dead_control_.get();
}


void JSGraph::GetCachedNodes(NodeVector* nodes) {
  cache_.GetCachedNodes(nodes);
  SetOncePointer<Node>* ptrs[] = {
      &c_entry_stub_constant_, &undefined_constant_, &the_hole_constant_,
      &true_constant_,         &false_constant_,     &null_constant_,
      &zero_constant_,         &one_constant_,       &nan_constant_};
  for (size_t i = 0; i < arraysize(ptrs); i++) {
    if (ptrs[i]->is_set()) nodes->push_back(ptrs[i]->get());
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
