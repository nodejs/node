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

#define CACHED(name, expr) \
  cached_nodes_[name] ? cached_nodes_[name] : (cached_nodes_[name] = (expr))


Node* JSGraph::CEntryStubConstant(int result_size) {
  if (result_size == 1) {
    return CACHED(kCEntryStubConstant,
                  HeapConstant(CEntryStub(isolate(), 1).GetCode()));
  }
  return HeapConstant(CEntryStub(isolate(), result_size).GetCode());
}


Node* JSGraph::EmptyFixedArrayConstant() {
  return CACHED(kEmptyFixedArrayConstant,
                HeapConstant(factory()->empty_fixed_array()));
}


Node* JSGraph::UndefinedConstant() {
  return CACHED(kUndefinedConstant, HeapConstant(factory()->undefined_value()));
}


Node* JSGraph::TheHoleConstant() {
  return CACHED(kTheHoleConstant, HeapConstant(factory()->the_hole_value()));
}


Node* JSGraph::TrueConstant() {
  return CACHED(kTrueConstant, HeapConstant(factory()->true_value()));
}


Node* JSGraph::FalseConstant() {
  return CACHED(kFalseConstant, HeapConstant(factory()->false_value()));
}


Node* JSGraph::NullConstant() {
  return CACHED(kNullConstant, HeapConstant(factory()->null_value()));
}


Node* JSGraph::ZeroConstant() {
  return CACHED(kZeroConstant, NumberConstant(0.0));
}


Node* JSGraph::OneConstant() {
  return CACHED(kOneConstant, NumberConstant(1.0));
}


Node* JSGraph::NaNConstant() {
  return CACHED(kNaNConstant,
                NumberConstant(std::numeric_limits<double>::quiet_NaN()));
}


Node* JSGraph::HeapConstant(Handle<HeapObject> value) {
  if (value->IsConsString()) {
    value = String::Flatten(Handle<String>::cast(value), TENURED);
  }
  Node** loc = cache_.FindHeapConstant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->HeapConstant(value));
  }
  return *loc;
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
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Int32Constant(value));
  }
  return *loc;
}


Node* JSGraph::Int64Constant(int64_t value) {
  Node** loc = cache_.FindInt64Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Int64Constant(value));
  }
  return *loc;
}


Node* JSGraph::NumberConstant(double value) {
  Node** loc = cache_.FindNumberConstant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->NumberConstant(value));
  }
  return *loc;
}


Node* JSGraph::Float32Constant(float value) {
  Node** loc = cache_.FindFloat32Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Float32Constant(value));
  }
  return *loc;
}


Node* JSGraph::Float64Constant(double value) {
  Node** loc = cache_.FindFloat64Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Float64Constant(value));
  }
  return *loc;
}


Node* JSGraph::ExternalConstant(ExternalReference reference) {
  Node** loc = cache_.FindExternalConstant(reference);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->ExternalConstant(reference));
  }
  return *loc;
}


Node* JSGraph::ExternalConstant(Runtime::FunctionId function_id) {
  return ExternalConstant(ExternalReference(function_id, isolate()));
}


Node* JSGraph::EmptyFrameState() {
  Node* empty_frame_state = cached_nodes_[kEmptyFrameState];
  if (!empty_frame_state || empty_frame_state->IsDead()) {
    Node* state_values = graph()->NewNode(common()->StateValues(0));
    empty_frame_state = graph()->NewNode(
        common()->FrameState(BailoutId::None(),
                             OutputFrameStateCombine::Ignore(), nullptr),
        state_values, state_values, state_values, NoContextConstant(),
        UndefinedConstant(), graph()->start());
    cached_nodes_[kEmptyFrameState] = empty_frame_state;
  }
  return empty_frame_state;
}


Node* JSGraph::Dead() {
  return CACHED(kDead, graph()->NewNode(common()->Dead()));
}


void JSGraph::GetCachedNodes(NodeVector* nodes) {
  cache_.GetCachedNodes(nodes);
  for (size_t i = 0; i < arraysize(cached_nodes_); i++) {
    if (Node* node = cached_nodes_[i]) {
      if (!node->IsDead()) nodes->push_back(node);
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
